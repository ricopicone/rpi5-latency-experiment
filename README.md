# Analog-to-Analog Latency Experiment — Raspberry Pi 5

Measures the latency of a closed-loop analog passthrough on a **Raspberry Pi 5**
with the **Waveshare High-Precision AD/DA Board** (TI **ADS1256** ADC + TI
**DAC8552** DAC). A sine wave from a function generator is sampled by the
ADS1256 and immediately reproduced on DAC8552 channel A; an oscilloscope
comparing input against output shows the end-to-end latency as a phase shift.

This is the Raspberry Pi version of the BeagleBone AI-64 experiment — same
concept, same success criterion (phase shift ≤ 1/20 of the signal period).

## What the code measures, and what it cannot

The program times the **processing latency**: from `DRDY` (the ADS1256
signalling "a conversion is ready") to the moment the DAC output is updated.
That is the part that lives in software and on the SPI bus.

It **cannot** measure the ADS1256's internal sigma-delta filter group delay or
the DAC's settling time — those are analog and show up only on the scope. So:

> **True analog-to-analog latency = oscilloscope phase shift.**
> The numbers the program prints are the *software/SPI component* of it.

Run the program for the scope measurement; use the printed numbers to see how
much of the total is software and how stable (jitter-free) the loop is.

## Files

| File | Purpose |
|---|---|
| `config.h` | **All hardware constants — read and verify this first.** Pin map, SPI device, clock and data rates. |
| `spidev_util.h` | spidev open/transfer helpers. |
| `gpio.c` / `.h` | GPIO via the kernel uAPI v2 (works on the Pi 5; `bcm2835`/`wiringPi` do not). |
| `ads1256.c` / `.h` | ADS1256 driver — reset, configure, continuous-read mode. |
| `dac8552.c` / `.h` | DAC8552 driver. |
| `latency_loop.c` | Main real-time program. |
| `Makefile` | Build (no external libraries). |
| `python/latency_loop.py` | Python prototype — wiring check + latency comparison baseline. |

## Hardware and wiring

The Waveshare board is a HAT: it seats directly on the 40-pin header, so the
digital wiring is just "plug it on." What you do need to connect are the
analog signals, and what you must *verify* is the pin map the code assumes.

**Pin map assumed by `config.h`** (BCM GPIO numbers — confirm against the
Waveshare wiki page for *your* board revision):

| Signal | BCM line | Header pin | Direction |
|---|---|---|---|
| ADS1256 `DRDY` | GPIO17 | 11 | input |
| ADS1256 `RESET` | GPIO18 | 12 | output |
| ADS1256 `CS` | GPIO22 | 15 | output |
| DAC8552 `SYNC`/`CS` | GPIO23 | 16 | output |
| SPI0 `SCLK` / `MOSI` / `MISO` | GPIO11 / 10 / 9 | 23 / 19 / 21 | SPI0 |

Chip-select is **not** done by the SPI controller — the Waveshare board routes
each chip's CS to a plain GPIO, so the code opens the bus with `SPI_NO_CS` and
toggles CS itself. The ADS1256 `PDWN` pin is tied high on the board.

**Analog connections:**

- Function generator → ADS1256 `AIN0`, ground → `AINCOM`.
- Oscilloscope channel 1 → the same function-generator node (the input).
- Oscilloscope channel 2 → DAC `OUTA`.

**Input range — important.** With single-ended `AIN0` referenced to
`AINCOM = GND`, the input pin must stay between 0 V and AVDD (≈5 V). A sine
centered on 0 V swings negative and violates that. Give the function generator
a **positive DC offset** so the whole waveform stays in range (e.g. 2 Vpp
centered at +1.25 V). The DAC output is likewise unipolar. A fixed DC offset
and any amplitude difference between the two traces do not affect the *phase
shift* the scope measures, so they do not matter for the latency result.

## One-time Raspberry Pi setup

1. **Enable SPI:** `sudo raspi-config` → Interface Options → SPI → Yes.
   (Equivalently, `dtparam=spi=on` in `/boot/firmware/config.txt`, then reboot.)
   Confirm `/dev/spidev0.0` exists.

2. **Build tools:** `sudo apt install build-essential`

3. **For the Python prototype:** `sudo apt install python3-spidev python3-lgpio`

4. **(Recommended) lower scheduling jitter:**
   - Install a `PREEMPT_RT` kernel if available for your Pi OS image, *or* at
     minimum keep the loop on an otherwise-idle core.
   - Isolate core 3 by adding `isolcpus=3 nohz_full=3` to
     `/boot/firmware/cmdline.txt`. The program pins the loop to core 3 by
     default (`RT_CPU_CORE` in `config.h`).

## Build and run (C — the recommended version)

```sh
make
sudo ./latency_loop --mode characterize        # fixed run, prints statistics
sudo ./latency_loop --mode passthrough         # runs until Ctrl-C (use the scope)
```

`sudo` is needed for real-time scheduling and `mlockall`. Without it the
program still runs (use `--no-rt`), just with more jitter.

Options: `--mode {passthrough|characterize}`, `--iterations N`,
`--csv FILE` (per-sample CSV in characterize mode, handy for plotting),
`--core N`, `--no-rt`, `--help`.

## Run (Python prototype)

```sh
cd python
sudo python3 latency_loop.py --mode characterize --rt
```

Use this first to confirm the wiring works ("first light"), then run it
alongside the C version: the gap between the two is a clean measurement of
what the Python interpreter and per-call overhead cost in a real-time loop.

## Interpreting the output

```
Timing breakdown (microseconds):
  ADC read (DRDY->ADC)    min ...  med ...  p90 ...  p99 ...  max ...  mean ...
  DAC write (ADC->DAC)    ...
  processing latency*     ...
  loop period             ...
```

- **processing latency** — `DRDY` → DAC updated. The headline software number.
- **loop period** — should sit near the ADS1256 conversion interval
  (**≈33.3 µs at 30 kSPS**). A `max` loop period far above the median means an
  iteration was preempted — i.e. a **dropped sample**. The original success
  test ("increase frequency to 5 kHz without dropping a period") is exactly
  this: watch the `max` loop period and the scope for skipped cycles.
- **min vs. p99 vs. max** — the spread is your jitter. On a `PREEMPT_RT`
  kernel with an isolated core it should be tight; on a stock kernel under
  load, expect occasional large outliers.

## Recommended runtime approach

You left the runtime choice open. Recommendation: **C in user space with
`SCHED_FIFO` real-time priority on an isolated core**, which is what
`latency_loop.c` does. Rationale:

- The latency floor here is set by the **ADS1256 and the SPI bus**, not by the
  CPU. C with `SCHED_FIFO` already gets you to that floor; a kernel driver or
  bare-metal would mainly reduce *jitter*, not the *median* — a large effort
  for a small gain in this experiment.
- Python is the right tool to verify wiring and to *demonstrate* interpreter
  overhead, but its per-iteration jitter makes it unsuitable as the final
  measurement loop.
- A `PREEMPT_RT` kernel plus `isolcpus` is the cheapest, highest-leverage
  improvement and needs no code change.

## Measured results (validated on hardware, 2026-05-22)

This section was speculative before the bring-up; it now reports what the
bench actually produced. See [`REPORT.md`](REPORT.md) for the full write-up
and [`analysis/figures/`](analysis/figures/) for the plots.

**End-to-end analog latency: 212.3 µs ± 4.8 µs** (scope, mean over a 100 Hz –
1 kHz sine sweep). The system behaves as a pure transport delay — phase shift
scales linearly with frequency.

**Latency budget that reproduces the measurement:**

| Component | Contribution | Source |
|---|---|---|
| Software loop (DRDY → DAC chip written) | **48.0 µs** (median, n = 50 000) | this code, `SCHED_FIFO`, isolated core |
| ADS1256 SINC5 filter group delay @ 15 kSPS | **~166.7 µs** | ≈ 2.5 / f_data |
| DAC8552 settling | **~5 µs** | datasheet typical |
| **Predicted total** | **~219.7 µs** | |
| **Measured total** | **212.3 µs** | scope phase sweep |

The Pi 5 + this code is **22%** of the budget; the ADS1256's SINC5 filter is
**~78%** of it. The original 10 µs target is unreachable *in principle* on
this HAT — the ADC's filter alone is more than 16× over budget.

**1/20-period success criterion:** met up to **≈238 Hz**. Above that, the
analog-to-analog phase shift exceeds 18° and the original BeagleBone
acceptance test fails. The README's pre-bring-up estimate of "0.5–1 kHz" was
optimistic by a factor of 2–4.

**The Raspberry Pi 5 is not the bottleneck.** Its CPU runs the loop at 15
kSPS with the loop period locked to the 66.67 µs conversion interval (median
66.74 µs, max 77.81 µs across 50 000 samples). The RP1 SPI controller can
clock far faster than the ADS1256 needs; the DAC8552 side here runs at 15.6
MHz and contributes ~1.5 µs of bit-clocking. If the book's real-time examples
need a genuinely low-latency analog loop, the thing to change is the **ADC**
— a fast SAR (SPI at 20–50 MHz) or a parallel-interface converter — not the
single-board computer.

**Why 15 kSPS instead of 30 kSPS.** At 30 kSPS (33.3 µs interval) the loop
median was 49.4 µs — every iteration dropped a conversion. Dropping the data
rate to 15 kSPS (66.7 µs interval, `ADS1256_DRATE = 0xE0` in `config.h`) lets
the loop keep up cleanly. The trade is a longer SINC5 filter group delay,
which dominates the analog-to-analog number anyway, so the net effect on
end-to-end latency is small.

So this experiment does double duty: it characterizes the HAT, and it cleanly
separates "platform capability" (excellent) from "converter capability"
(the real constraint) — which is exactly the kind of distinction worth drawing
in a real-time computing text.

## Re-running the analysis

```sh
python3 analysis/analyze.py
```

Reads `data/phase_sweep.csv` (scope measurements) and
`data/characterize_15ksps.csv` (per-sample software timings from the C loop),
prints a summary table, and regenerates the plots in `analysis/figures/`.

## Verify on your board before measuring

- Every line marked `[VERIFY]` in `config.h` (pin numbers, SPI node,
  reference voltages, data rate).
- `config.h` GPIO chip: the Pi 5 header bank is usually `/dev/gpiochip0`; on
  some kernels it is `/dev/gpiochip4`. `gpio.c` tries both, but confirm with
  `gpiodetect`.
- The SPI mode: both chips are configured for SPI mode 1 (CPOL=0, CPHA=1). If
  the ADS1256 ID check fails or the DAC output is wrong, mode is the first
  suspect — see Troubleshooting.

## Troubleshooting

- **`ads1256: unexpected STATUS` / ID nibble not 0x3** — SPI is not talking to
  the ADC. Check the HAT is fully seated, that SPI is enabled, the CS line in
  `config.h`, and the SPI mode.
- **`timeout waiting for DRDY`** — the `DRDY` GPIO line number is wrong, or the
  ADC never started converting. Verify `LINE_ADC_DRDY` and the GPIO chip.
- **`cannot open /dev/spidev0.0`** — SPI not enabled, or your build outputs to
  `spidev0.1`; adjust `SPI_DEVICE` in `config.h`.
- **DAC output looks wrong (inverted, half-scale, noisy)** — check the DAC
  `SYNC` line number, the DAC SPI mode, and the `DAC_VREF` assumption.
- **Large jitter / outliers in the timing report** — expected on a stock
  kernel; use a `PREEMPT_RT` kernel, `isolcpus`, and run with `sudo`.
- **`spi: note: kernel rejected SPI_NO_CS; controller CE0 will toggle`** —
  expected on the Pi 5. The RP1 spidev driver does not accept the `SPI_NO_CS`
  bit through `SPI_IOC_WR_MODE32`; the code falls back to the 8-bit mode
  ioctl and the controller toggles its own `CE0` (GPIO8). The Waveshare HAT
  does not route `CE0`, so the toggle is harmless — chip selects go via
  GPIO22/23 as before.

---

*Validated on real hardware on 2026-05-22 (see [`REPORT.md`](REPORT.md)).
Compiles cleanly with `-Wall -Wextra -std=gnu11` and runs on a stock
Raspberry Pi OS (trixie) kernel.*
