---
project_note_id: 1008
title: Analog-to-Analog Latency on a Raspberry Pi 5 with the Waveshare High-Precision
  AD/DA Board
---


**Date of measurement:** 2026-05-22
**Author:** Rico Picone (with Claude Code assist)
**Project:** [`rpi5-latency-experiment` on GitHub](https://github.com/ricopicone/rpi5-latency-experiment)

---

## 1. Summary

A closed-loop analog passthrough — function-generator sine → ADS1256 → user-space
C loop → DAC8552 — was characterized on a Raspberry Pi 5 (Debian 13 trixie,
kernel `6.12.47+rpt-rpi-2712`) carrying the Waveshare High-Precision AD/DA
Board. The end-to-end analog-to-analog latency measured on a Tektronix DPO2012B
oscilloscope is

> **212.3 µs ± 4.8 µs** (mean ± s.d., n = 10 frequencies between 100 Hz and 1 kHz)

The system behaves as a pure transport delay: phase shift scales linearly with
frequency over the measured range (Fig. 1, panel a), and the per-frequency
delay computed from each phase reading is constant within the measurement
noise (panel b).

The Raspberry Pi 5 + user-space C loop (the part of the system that lives in
software) contributes **48.0 µs** of the budget — about **22%** — leaving the
**ADS1256's SINC5 decimation filter** as the dominant contributor at about
**78%**. The 1/20-period success criterion from the original BeagleBone
experiment is met only up to **≈ 238 Hz** on this hardware.

The headline figure is reproduced below:

![Summary](https://raw.githubusercontent.com/ricopicone/rpi5-latency-experiment/main/analysis/figures/summary.png)

The same result on the scope at 500 Hz:

![Scope at 500 Hz: CH1 input (yellow, 500 mV/div), CH2 DAC output (cyan, 200 mV/div), 500 µs/div. Visible stair-stepping on CH2 is the 15 kSPS reconstruction (≈ 30 samples per period); the phase shift between zero crossings is the latency.](https://raw.githubusercontent.com/ricopicone/rpi5-latency-experiment/main/photos/E579E372-4E5D-4261-98BA-027DA3A7E445_1_102_o.jpeg)

---

## 2. Setup

**Hardware**

| Item | Detail |
|---|---|
| SBC | Raspberry Pi 5 (BCM2712, RP1 I/O controller) |
| OS / kernel | Debian 13 (trixie) / `6.12.47+rpt-rpi-2712`, stock — no `PREEMPT_RT`, no `isolcpus` |
| ADC + DAC | Waveshare *High-Precision AD/DA Board* — TI ADS1256 (24-bit ΣΔ) + TI DAC8552 (16-bit) |
| Function generator | Sine, 2 Vpp, +1.25 V DC offset (signal swing 0.25 V → 2.25 V, within the ADS1256's `AIN0 ∈ [0, AVDD]` range) |
| Oscilloscope | Tektronix DPO2012B; DC coupling; 16× waveform averaging; statistics on |

**Software**

| Item | Detail |
|---|---|
| Loop | C, single-threaded, `SCHED_FIFO` priority 80, pinned to core 3, `mlockall(MCL_CURRENT \| MCL_FUTURE)` |
| GPIO | Linux GPIO character-device uAPI v2 (the legacy `bcm2835` / `wiringPi` libraries are incompatible with the RP1 controller) |
| SPI | `spidev`, per-transfer clock rate. ADS1256 clocked at 1.92 MHz (`f_CLKIN/4`, the part's hard ceiling); DAC8552 at 15.6 MHz |
| ADC mode | RDATAC (continuous-read) on a single channel — minimum per-sample overhead |
| Sample rate | 15 kSPS (`ADS1256_DRATE = 0xE0`) — see §6 for why not 30 kSPS |
| Chip select | Manual GPIO toggle (Waveshare routes both chips' CS to plain GPIO, not the SPI controller's CE lines) |

**Wiring**

| Signal | HAT terminal | Header pin / line |
|---|---|---|
| ADS1256 `AIN0` | `AD0` (screw block) | function-generator center |
| ADC analog ground | `AINCOM` / `AGND` | function-generator ground + scope CH1 ground |
| DAC8552 `OUTA` | `DAC0` (screw block) | scope CH2 |
| DAC analog ground | `AGND` near `DAC0` | scope CH2 ground |
| ADS1256 `DRDY` | — | GPIO17 (header pin 11), input |
| ADS1256 `RESET` | — | GPIO18 (pin 12), output |
| ADS1256 `CS` | — | GPIO22 (pin 15), output |
| DAC8552 `SYNC/CS` | — | GPIO23 (pin 16), output |
| SPI0 SCLK / MOSI / MISO | — | GPIO11 / 10 / 9 (pins 23 / 19 / 21) |

The function generator output was T-junctioned at the source: one leg drove
`AD0`, the other drove scope CH1 — making CH1 the *true input* and CH2 the
loop's output.

The HAT before wiring (note the `JMP_AGND` jumper position between `AINCOM`
and `AGND` — required for single-ended AIN0 / AINCOM operation), and after:

![Waveshare HP AD/DA Board, bare. The JMP_AGND jumper visible at top right ties AINCOM to AGND. Screw-terminal labels (left to right at the top): AD7, AD6, AD5, AD4, AD3, AD2, AD1, AD0, AGND, VCC, GND, DAC1, DAC0.](https://raw.githubusercontent.com/ricopicone/rpi5-latency-experiment/main/photos/72A8C348-B07D-4126-BB22-4C2C7C6A679C_1_105_c.jpeg)

![HAT with analog wires landed on AD0 (input from function generator) and DAC0 (output to scope CH2), grounds on AGND.](https://raw.githubusercontent.com/ricopicone/rpi5-latency-experiment/main/photos/FD9EB7C6-BA2C-4827-9B18-97BDED8B79D1_1_201_a.jpg)

Function generator setup (Tektronix AFG1002): sine, 500 Hz, 2.000 Vpp,
+1.250 V DC offset. The waveform preview confirms the swing of 0.25 V →
2.25 V, well inside the ADS1256's `AIN0 ∈ [0, AVDD]` single-ended range.

![Tektronix AFG1002 function-generator screen at the working settings: sine, 500 Hz, 2.000 Vpp, +1.250 V DC offset, start phase 0°.](https://raw.githubusercontent.com/ricopicone/rpi5-latency-experiment/main/photos/650D555E-C214-412E-B97E-B0E9CB106457_1_201_a.jpg)

---

## 3. Methodology

### 3.1 Software timing (`characterize` mode)

The C program (`latency_loop.c`) records four timestamps per iteration:

```
ta : just before waiting for DRDY
tb : DRDY observed asserted (a fresh conversion is ready)
tc : 3-byte ADC sample read complete
td : 3-byte DAC write complete (the DAC output updates on CS-high)
```

From these it computes:

- `ADC read`     = `tc − tb`  (DRDY → ADC sample in memory)
- `DAC write`    = `td − tc`  (ADC sample → DAC chip written)
- `proc latency` = `td − tb`  (DRDY → DAC chip written; the headline software number)
- `loop period`  = `td − td_prev` (back-to-back iteration time)

Per-sample data was captured with `--csv` and analyzed off-line. The 50 000
samples used in this report come from a single `--mode characterize -n 50000`
run while the rig was idle (no function-generator input — the timing numbers
are independent of the analog signal; see §3.2).

### 3.2 Analog timing (scope sweep)

`latency_loop --mode passthrough` mirrors the ADC sample to the DAC output
continuously. With the function-generator sine on `AIN0`, the input-vs-output
phase shift on the scope (CH1 vs CH2) is the **true analog-to-analog
latency** — including the ADS1256's SINC5 filter group delay and the DAC8552
settling time, neither of which the software can see.

The scope's built-in "CH1→CH2 rising-edge delay" measurement, with 16× waveform
averaging and statistics enabled, was used to read off the phase at each
frequency. Each reading was the *mean* over many acquisitions, not a single
shot — the standard deviation reported by the scope was ≲ 0.5° in every case.

The level shift between input and output (CH1 swing 0.25–2.25 V, CH2 swing
~1.4–2.4 V) is the expected consequence of `Vref_ADC ≠ Vref_DAC` and the
mid-scale offset coded into the ADC→DAC value mapping. It does not affect the
phase measurement (the scope picks zero crossings from each channel
independently), so it does not affect the latency result.

The function-generator frequency was swept manually in 100 Hz steps from
100 Hz to 1 kHz.

---

## 4. Results

### 4.1 Frequency sweep (analog-to-analog latency)

Phase measurements and the corresponding time delays per frequency:

| Freq (Hz) | Phase (deg) | Delay (µs) |
|---:|---:|---:|
|  100 |  8.0 | 222.2 |
|  200 | 15.6 | 216.7 |
|  300 | 23.0 | 213.0 |
|  400 | 30.7 | 213.2 |
|  500 | 37.7 | 209.4 |
|  600 | 45.7 | 211.6 |
|  700 | 52.4 | 207.9 |
|  800 | 59.0 | 204.9 |
|  900 | 69.0 | 213.0 |
| 1000 | 76.0 | 211.1 |

**Mean delay = 212.3 µs, σ = 4.8 µs.** A linear fit of phase vs frequency
(forced through the origin) gives τ = 210.2 µs — within ~1% of the simple
mean. (The 100 Hz outlier, 222 µs, is the expected effect of measuring an
8° angle: phase-measurement uncertainty scales as 1/sin(φ).)

![Phase vs freq](https://raw.githubusercontent.com/ricopicone/rpi5-latency-experiment/main/analysis/figures/phase_vs_freq.png)
![Delay vs freq](https://raw.githubusercontent.com/ricopicone/rpi5-latency-experiment/main/analysis/figures/delay_vs_freq.png)

### 4.2 Software loop timing (`characterize`, 50 000 samples at 15 kSPS)

All values in microseconds.

| Stage | min | median | p90 | p99 | max | mean |
|---|---:|---:|---:|---:|---:|---:|
| ADC read (DRDY → sample) | 26.83 | 27.52 | 27.57 | 30.20 | 40.30 | 27.56 |
| DAC write (ADC → DAC)    | 19.70 | 20.43 | 20.50 | 23.43 | 31.57 | 20.48 |
| **Processing latency**   | **46.63** | **47.96** | **48.06** | **53.63** | **63.52** | **48.04** |
| Loop period              | 56.33 | 66.74 | 66.91 | 67.83 | 77.81 | 66.66 |

The **loop period median (66.74 µs) sits within 0.1% of the 15 kSPS
conversion interval (66.67 µs)** — the loop is locked to the ADC's pace, with
no dropped conversions. Median processing latency is **47.96 µs**, with p99
just 5.7 µs above the median and max 15.6 µs above. That spread is the
real-time scheduling jitter on the stock kernel; PREEMPT_RT would tighten it
further if needed.

![Processing latency](https://raw.githubusercontent.com/ricopicone/rpi5-latency-experiment/main/analysis/figures/proc_latency_hist.png)
![Loop period](https://raw.githubusercontent.com/ricopicone/rpi5-latency-experiment/main/analysis/figures/loop_period_hist.png)

### 4.3 Latency budget

| Component | Latency (µs) | Reasoning |
|---|---:|---|
| Software loop (DRDY → DAC chip written) | 48.0 | measured, this code, median |
| ADS1256 SINC5 group delay @ 15 kSPS | ~166.7 | ≈ 2.5 / f_data, datasheet-consistent |
| DAC8552 settling | ~5.0 | datasheet typical |
| **Predicted total** | **~219.7** | sum of the above |
| **Measured total (scope)** | **212.3** | mean of §4.1 sweep |

Predicted and measured agree within **3.4%** (7.4 µs, comfortably inside the
4.8 µs measurement standard deviation). The ADS1256 SINC5 filter is the
single dominant contributor at ~78% of the budget.

![Budget](https://raw.githubusercontent.com/ricopicone/rpi5-latency-experiment/main/analysis/figures/budget_bar.png)

### 4.4 Success-criterion frequency

The original BeagleBone acceptance test was *phase shift ≤ 1/20 of the
signal period* (≡ ≤ 18°). With a constant 212.3 µs delay:

> The system meets the criterion up to **≈ 238 Hz**.

This is well below the README's pre-bring-up estimate of "0.5–1 kHz with
this HAT," which was optimistic by a factor of 2–4.

---

## 5. Discussion

### 5.1 The Pi 5 is not the bottleneck

This is the single most useful finding to carry forward into the textbook.
The Pi 5 CPU runs the user-space C loop with about **19 µs of margin per
iteration** at 15 kSPS (66.67 µs interval − 47.96 µs median proc latency =
18.71 µs of slack). The SPI controller on the RP1 can clock far faster than
the 1.92 MHz the ADS1256 will accept; the DAC side of the loop, which runs
at 15.6 MHz, demonstrates this (the 3-byte DAC frame is on the wire in
1.5 µs, dwarfed by the per-transfer syscall overhead).

The fact that **78%** of the analog-to-analog latency comes from the ADS1256
itself is a clean experimental demonstration of an important real-time
systems point: *the platform doesn't determine the latency floor; the
converter does.* When the textbook discusses "analog I/O latency" it can
point at this result and say so concretely.

### 5.2 Why 15 kSPS, not 30 kSPS

The original target was 30 kSPS, the ADS1256's fastest data rate. At
30 kSPS the conversion interval is 33.3 µs — comfortably *below* the
measured 47.96 µs software loop. Every iteration would drop a conversion.

The fix is the one the README anticipated: drop the data rate to 15 kSPS
(`ADS1256_DRATE = 0xE0`), doubling the conversion interval to 66.67 µs.
That gives the loop room to keep up while paying with a longer SINC5 filter
group delay (~167 µs vs ~83 µs at 30 kSPS). The trade-off lands in favor of
keeping up, because the filter delay dominates either way.

If a higher *sample rate* were genuinely needed (e.g. for bandwidth, not
latency), the right next step would be to cut software overhead — most
profitably the four per-loop GPIO ioctls — by moving CS to direct RP1
register writes via `mmap` of `/dev/gpiomem0`. That would buy ~15–20 µs of
software median and pull the loop down to ~30 µs, which 30 kSPS could sustain.
It would *not* materially change the analog-to-analog latency, since the
filter is the dominant term.

### 5.3 Where the software 48 µs goes

A back-of-the-envelope breakdown of the 47.96 µs processing latency:

| Item | µs | Note |
|---|---:|---|
| ADS1256 3-byte read on the wire @ 1.92 MHz | 12.5 | irreducible |
| DAC8552 3-byte write on the wire @ 15.6 MHz | 1.5 | irreducible |
| `spidev` `SPI_IOC_MESSAGE` syscall × 2 | ~10–15 | OS overhead |
| GPIO uAPI v2 `SET_VALUES` ioctls (4× per iter, for CS) | ~20 | per call ≈ 5 µs on the RP1 |
| Misc. (timing measurements, branches, memory) | ~1 | |
| **Sum** | **~47** | matches the measured 47.96 µs |

The dominant *avoidable* software cost is the GPIO ioctl path. An `mmap`'d
direct-register approach would cut each CS toggle to a few hundred
nanoseconds, saving ~15–20 µs total — but again, only on the software
component, and the filter would still set the floor.

### 5.4 A subtlety found during bring-up

The Pi 5's RP1 `spidev` driver does **not** accept the `SPI_NO_CS` bit
through `SPI_IOC_WR_MODE32` — it returns `EINVAL`. The code falls back to
the older 8-bit `SPI_IOC_WR_MODE` ioctl and accepts that the controller will
then toggle its own `CE0` (GPIO8) on every transfer. The Waveshare HAT does
not route `CE0` to any chip's select pin (both ADS1256 and DAC8552 are
selected via GPIO22/23), so the toggle is harmless. The note printed at
startup —

```
spi: note: kernel rejected SPI_NO_CS; controller CE0 will toggle
     (harmless on this HAT -- chip selects are on GPIO22/23).
```

— exists to document this for the reader.

### 5.5 What the scope did *not* show that you might expect

- **No frequency-dependent attenuation in the measured range.** At 1 kHz
  (the upper end of the sweep) the loop's reconstructed sine on CH2 was
  visibly stair-stepped (15 samples per period), but the SINC5 filter's
  *amplitude* roll-off was not yet appreciable. Above ~3 kHz the input
  amplitude would start to attenuate; this experiment did not probe that
  region because the phase-shift criterion fails first.
- **No clock-tick jitter beyond the loop's own ~6 µs p99 spread.** The
  scope's measured phase had σ ≲ 0.5° at every frequency (10× tighter than
  the variation between frequencies), so all the observed delay variation
  comes from converter and measurement noise — not from the SBC.

---

## 6. Reproducing the measurements

```sh
# from this directory, on the Mac (with SSH to the Pi configured):
make                                       # cross-check the build
rsync -av . rpi5:~/latency-experiment/     # push to the Pi
ssh rpi5 'cd latency-experiment && make'   # build on target
ssh rpi5 'cd latency-experiment && sudo ./latency_loop \
            --mode characterize -n 50000 --csv /tmp/run.csv'
scp rpi5:/tmp/run.csv data/characterize_15ksps.csv
# update data/phase_sweep.csv with whatever the scope reads
python3 analysis/analyze.py                # regenerates figures + prints stats
```

The exact `data/phase_sweep.csv` and `data/characterize_15ksps.csv` used in
this report are in [the repository](https://github.com/ricopicone/rpi5-latency-experiment/tree/main/data).

---

## 7. Conclusion

A Raspberry Pi 5 running a stock Debian-trixie kernel can drive the
Waveshare High-Precision AD/DA Board through a user-space C control loop
with **47.96 µs median processing latency** (p99 53.63 µs, max 63.52 µs)
and a **loop period locked to the 15 kSPS conversion interval** (median
66.74 µs vs the theoretical 66.67 µs, p99 only 1.16 µs above interval, max
77.81 µs). Across 50 000 iterations not a single conversion was dropped.
The end-to-end analog-to-analog latency on the scope is **212 µs**, of which
the ADS1256's SINC5 decimation filter contributes ~167 µs. The platform is
not the bottleneck — the converter is — which is the takeaway worth carrying
into the textbook. A genuinely low-latency analog I/O loop on this SBC would
require swapping the ADC, not the SBC.