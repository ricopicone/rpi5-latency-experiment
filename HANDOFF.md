# Handoff — Analog-to-Analog Latency Experiment

Brief for a Claude Code session continuing development of this project.
(If you prefer it auto-loaded, rename this file to `CLAUDE.md`.)

## Purpose

Measure the latency of a closed-loop analog passthrough on a **Raspberry Pi 5**
with the **Waveshare High-Precision AD/DA Board** (TI **ADS1256** 24-bit ADC +
**DAC8552** 16-bit DAC, both on SPI). A function-generator sine is sampled by
the ADS1256 and immediately re-emitted on DAC8552 channel A; an oscilloscope
reads the end-to-end latency as input-vs-output phase shift.

This feeds a larger goal: evaluating the Pi 5 as the single-board computer for
the next edition of a real-time computing textbook (it succeeds a BeagleBone
AI-64). See `README.md` for the full experiment description and wiring.

## Current status

- **Validated on real hardware** (Raspberry Pi 5, Waveshare High-Precision
  AD/DA, 2026-05-22). See [`REPORT.md`](REPORT.md). Headline number:
  **212.3 µs ± 4.8 µs** analog-to-analog latency on the scope (mean over a
  100 Hz–1 kHz sine sweep). The ADS1256 SINC5 filter contributes ~78% of that
  budget; the software loop contributes ~22% (median 47.96 µs).
- **All source is written and compiles cleanly** — `-Wall -Wextra -std=gnu11`,
  no warnings.
- Configuration that turned out to be needed on the Pi 5:
  - `ADS1256_DRATE = 0xE0` (15 kSPS, not the original 30 kSPS — see
    "Latency reality" below).
  - `spidev_util.h` falls back from `SPI_IOC_WR_MODE32` to `SPI_IOC_WR_MODE`
    when the kernel rejects the `SPI_NO_CS` bit, which is the case on the
    RP1 driver. The controller then toggles its own `CE0` (GPIO8), but that
    pin is unused on this HAT so it is harmless. See `spidev_util.h:36-58`.
  - All four `[VERIFY]` pins in `config.h` (17/18/22/23) and the GPIO chip
    (`/dev/gpiochip0`) matched the Waveshare board on the bench — no edits.

## File / architecture map

| File | Role |
|---|---|
| `config.h` | **All hardware constants.** Pin map, SPI node, clock/data rates, RT settings. Every `[VERIFY]` line is unconfirmed. |
| `spidev_util.h` | Header-only spidev helpers: `spi_open()`, `spi_xfer()` (per-call clock rate). |
| `gpio.c` / `.h` | GPIO via kernel **uAPI v2** (`<linux/gpio.h>`). DRDY input + manual CS/RESET outputs. |
| `ads1256.c` / `.h` | ADS1256 driver: reset, register config, self-cal, **RDATAC** continuous mode, 24-bit signed read. |
| `dac8552.c` / `.h` | DAC8552 driver: 24-bit control+data frame, channel A, immediate update. |
| `latency_loop.c` | Main: arg parsing, RT setup, the ADC→DAC loop, timing stats, CSV, SIGINT handling. |
| `Makefile` | Build. No external libraries. |
| `python/latency_loop.py` | Python prototype (spidev + lgpio) — wiring check and latency-comparison baseline. |
| `README.md` | User-facing: wiring table, setup, run, interpretation, latency discussion. |

## Design invariants — do not undo these without a hardware reason

1. **Kernel interfaces only.** spidev + GPIO uAPI v2. The legacy
   `bcm2835` / `wiringPi` libraries poke fixed BCM283x addresses through
   `/dev/mem` and **do not work on the Pi 5's RP1 controller**. Waveshare's
   sample code uses them — do not copy that approach. No external libs in C.
2. **`SPI_NO_CS` + manual GPIO chip-select.** The board routes each chip's CS
   to a plain GPIO (GPIO22 ADC, GPIO23 DAC), not the controller's CE lines.
   The SPI controller must not toggle CS.
3. **ADS1256 in RDATAC continuous mode** on one fixed channel — lowest
   per-sample overhead (no command, no `t6` delay per read).
4. **Per-transfer SPI clock.** ADC reads at `ADS1256_SPI_HZ` (1.92 MHz, the
   `f_CLKIN/4` hard ceiling); DAC writes at `DAC8552_SPI_HZ` (15.6 MHz) — same
   file descriptor, different `speed_hz` per `spi_ioc_transfer`.
5. **Two implementations on purpose.** C is the real instrument; Python is for
   first-light wiring checks and as a deliberate interpreter-overhead baseline.
6. **`config.h` is the single source of hardware truth.** Keep new constants
   there; keep `python/latency_loop.py`'s copies in sync.

## Hardware bring-up checklist (first real-hardware session)

1. `sudo raspi-config` → enable SPI; confirm `/dev/spidev0.0` exists.
2. `gpiodetect` — confirm the 40-pin bank is `gpiochip0` (some kernels:
   `gpiochip4`). `gpio.c` already tries both; `config.h` documents it.
3. Verify every `[VERIFY]` pin in `config.h` against the Waveshare wiki page
   for the exact board revision in hand.
4. Run the Python version first (`sudo python3 latency_loop.py -m characterize`).
   The ADS1256 `STATUS` ID nibble must read `0x3` — if not, SPI is not talking
   to the ADC (mode, CS line, or wiring).
5. If the DAC output is inverted / half-scale / wrong, suspect the DAC SPI
   mode — flip `SPI_MODE` (try 1 ↔ 2) and re-test. Both chips are currently
   assumed mode 1.
6. Then run the C version under `sudo`.

## Open items / TODO

- **DONE: confirm the loop sustains 30 kSPS.** It does not — at 30 kSPS the
  loop median was 49.4 µs vs the 33.3 µs interval, so every iteration drops a
  conversion. `ADS1256_DRATE` is now `0xE0` (15 kSPS); the loop locks at
  66.74 µs median, 77.81 µs max.
- **DONE: confirm DAC8552 SPI mode.** Mode 1 works for both chips, no change
  needed.
- **DONE: spidev / GPIO ioctl overhead is now measured.** ADC stage 27.5 µs
  median (12.5 µs SPI on the wire + ~15 µs overhead), DAC stage 20.4 µs
  median (1.5 µs SPI + ~19 µs overhead). Per-CS-toggle GPIO ioctl cost is
  about 5 µs; per SPI transfer ioctl about 5–10 µs.
- **Optional optimization:** the four per-loop GPIO ioctls (2× ADC CS, 2× DAC
  CS) could be replaced with direct RP1 register access via an `mmap` of
  `/dev/gpiomem0`. This would cut ~20 µs off the software loop median, but
  the ADS1256's SINC5 group delay (~167 µs at 15 kSPS) dominates the
  end-to-end number, so the gain on the scope-measured latency is roughly
  10%. Only pursue if jitter (not median) is the goal; current jitter is
  already low (proc latency p99 is 53.63 µs vs 47.96 µs median).
- Consider a DAC-only signal-generator mode for characterizing the DAC path
  independently.
- `ads1256.c` does not read back the configuration registers after `WREG`;
  a verify-readback would harden bring-up.

## Latency reality — confirmed by the bench

Do **not** spend effort micro-optimizing the loop expecting to approach the
original ~10 µs target. The latency floor is set by the **ADS1256**, not the
Pi 5 and not this code. Measured budget at 15 kSPS:

| Component | µs | Note |
|---|---|---|
| Software loop (DRDY → DAC chip written) | 48.0 | median over 50 000 samples |
| ADS1256 SINC5 filter group delay | ~166.7 | ≈ 2.5 / f_data |
| DAC8552 settling | ~5 | datasheet typical |
| **Predicted total** | **~219.7** | |
| **Measured total** | **212.3** | scope, 100 Hz–1 kHz sweep mean |

The ADS1256 filter alone is ~78% of the budget; the Pi 5 + this code is
~22%. The 1/20-period success criterion is met only up to **~238 Hz** on
this HAT. If the textbook's examples need a genuinely low-latency loop, the
change to make is the **ADC**, not the SBC. Loop-side optimization only
buys lower jitter, not lower median latency.

## Build & run

```sh
make                                        # build (needs build-essential)
sudo ./latency_loop --mode characterize     # fixed run + statistics
sudo ./latency_loop --mode passthrough      # runs until Ctrl-C (oscilloscope)
sudo ./latency_loop -m characterize -c run.csv -n 100000   # + per-sample CSV
make clean
```

`sudo` is for `SCHED_FIFO` + `mlockall`. `--no-rt` runs without privilege
(more jitter). Python: `sudo apt install python3-spidev python3-lgpio` first.

## Environment notes

- Validated on: Raspberry Pi 5, Debian 13 (trixie), kernel
  `6.12.47+rpt-rpi-2712` (stock — no PREEMPT_RT, no `isolcpus`). Jitter was
  already low on this stock configuration; PREEMPT_RT would tighten it
  further but is not required to reproduce the headline numbers.
- A development sandbox can `make` this for aarch64 to catch compile errors,
  but cannot execute the hardware loop. Treat "compiles" and "works" as
  separate milestones.
- For the lowest-jitter measurements: a `PREEMPT_RT` kernel and
  `isolcpus=3 nohz_full=3` on the kernel command line (the loop pins to core 3
  by default, `RT_CPU_CORE` in `config.h`).
