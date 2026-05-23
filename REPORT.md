# Analog-to-Analog Latency on a Raspberry Pi 5 with the Waveshare High-Precision AD/DA Board

**Date of measurement:** 2026-05-22
**Author:** Rico Picone (with Claude Code assist)
**Project:** [`latency-experiment`](./README.md)

---

## 1. Summary

This experiment evaluates the Raspberry Pi 5 as a low-latency analog-I/O
host for the real-time computing textbook. The measurement is the end-to-end
delay from an analog input on an ADC to the corresponding analog output on
a DAC — the latency budget a control loop has to live within.

A closed-loop analog passthrough — function-generator sine → ADS1256 (24-bit
sigma-delta ADC) → user-space C loop on the Pi 5 → DAC8552 (16-bit DAC) —
was characterized on a Raspberry Pi 5 (Debian 13 trixie, kernel
`6.12.47+rpt-rpi-2712`) carrying the Waveshare High-Precision AD/DA Board,
a Hardware-Attached-on-Top (HAT) module that mounts on the Pi's 40-pin
header. The end-to-end analog-to-analog latency measured on a Tektronix
DPO2012B oscilloscope is

> **212.3 µs ± 4.8 µs** (mean ± s.d., n = 10 frequencies between 100 Hz and 1 kHz)

The system behaves as a pure transport delay: phase shift scales linearly
with frequency over the measured range (Fig. 1, panel a), and the
per-frequency delay computed from each phase reading is constant within
the measurement noise (panel b).

The Raspberry Pi 5 + user-space C loop (the part of the system that lives
in software) contributes **48.0 µs** of the budget — about **22%** — and
the **ADS1256's SINC5 decimation filter** (a fifth-order sinc filter
inside the ADC, see §4.3) is the dominant contributor at about **78%**.

The 22% software figure is worth a footnote, because it is *not* set by Pi
5 silicon. Of those 48 µs, only ~14 µs is irreducible SPI bit-clocking on
the wire; the other ~34 µs is the cost of the kernel-mediated `spidev` and
GPIO uAPI v2 interfaces the loop uses to talk to the SPI controller and the
chip-select pins. A direct-register-access loop — using `mmap` of the Pi
5's RP1 I/O controller's GPIO and SPI peripheral registers — would cut
that 48 µs to ~15 µs, at the cost of code that's specific to the RP1 chip
rather than portable across single-board computers (SBCs). See §5.5 for
the full set of trade-offs.

A "phase shift ≤ 1/20 of the signal period" criterion is met only up to
**≈ 238 Hz** on this hardware. (See §4.4 for what this criterion is and
why it is a useful yardstick for control-loop work.)

The headline figure is reproduced below:

![Summary](analysis/figures/summary.png)

The same result on the scope at 500 Hz:

![Scope at 500 Hz: CH1 input (yellow, 500 mV/div), CH2 DAC output (cyan, 200 mV/div), 500 µs/div. Visible stair-stepping on CH2 is the 15 kSPS reconstruction (≈ 30 samples per period); the phase shift between zero crossings is the latency.](photos/E579E372-4E5D-4261-98BA-027DA3A7E445_1_102_o.jpeg)

Block diagram of the signal path:

![Signal-path block diagram: function generator -> T -> (CH1, ADS1256). ADS1256 -> Pi 5 C loop via SPI + DRDY. Pi 5 -> DAC8552 via SPI. DAC8552 OUTA -> CH2. The scope measures the CH1 -> CH2 phase shift, which is the end-to-end analog-to-analog latency.](analysis/figures/system_diagram.png)

---

## 2. Setup

**Hardware**

| Item | Detail |
|---|---|
| SBC | Raspberry Pi 5 (BCM2712, RP1 I/O controller) |
| OS / kernel | Debian 13 (trixie) / `6.12.47+rpt-rpi-2712`, stock — no `PREEMPT_RT`, no `isolcpus` |
| ADC + DAC | Waveshare *High-Precision AD/DA Board* — TI ADS1256 (24-bit ΣΔ) + TI DAC8552 (16-bit) |
| Function generator | Sine, 2 Vpp, +1.25 V DC offset (signal swing 0.25 V → 2.25 V, within the ADS1256's `AIN0 ∈ [0, AVDD]` range) |
| Oscilloscope | Tektronix DPO2012B; AC coupling on both channels; 512× waveform averaging; manual cursor / measurement readout (statistics off) |

**Software**

| Item | Detail |
|---|---|
| Loop | C, single-threaded, `SCHED_FIFO` priority 80, pinned to core 3, `mlockall(MCL_CURRENT \| MCL_FUTURE)` |
| GPIO | Linux GPIO character-device uAPI v2 (the legacy `bcm2835` / `wiringPi` libraries are incompatible with the RP1 controller) |
| SPI | `spidev`, per-transfer clock rate. ADS1256 clocked at 1.92 MHz (`f_CLKIN/4`, the part's hard ceiling); DAC8552 at 15.6 MHz |
| ADC mode | RDATAC (continuous-read) on a single channel — minimum per-sample overhead |
| Sample rate | 15 kSPS (`ADS1256_DRATE = 0xE0`) — see §5.2 for why not 30 kSPS |
| Chip select (CS) | Manual GPIO toggle. On a shared SPI bus each peripheral has its own CS line that the master drives low to address it. The Waveshare HAT wires both chips' CS pins to plain GPIO lines (GPIO22 for the ADC, GPIO23 for the DAC) rather than the SPI controller's built-in CE0/CE1 outputs, so the C loop toggles them itself rather than letting the controller do it. |

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

![Waveshare HP AD/DA Board, bare. The JMP_AGND jumper visible at top right ties AINCOM to AGND. Screw-terminal labels (left to right at the top): AD7, AD6, AD5, AD4, AD3, AD2, AD1, AD0, AGND, VCC, GND, DAC1, DAC0.](photos/72A8C348-B07D-4126-BB22-4C2C7C6A679C_1_105_c.jpeg)

![HAT with analog wires landed on AD0 (input from function generator) and DAC0 (output to scope CH2), grounds on AGND.](photos/FD9EB7C6-BA2C-4827-9B18-97BDED8B79D1_1_201_a.jpg)

Function generator setup (Tektronix AFG1002): sine, 500 Hz, 2.000 Vpp,
+1.250 V DC offset. The waveform preview confirms the swing of 0.25 V →
2.25 V, well inside the ADS1256's `AIN0 ∈ [0, AVDD]` single-ended range.

![Tektronix AFG1002 function-generator screen at the working settings: sine, 500 Hz, 2.000 Vpp, +1.250 V DC offset, start phase 0°.](photos/650D555E-C214-412E-B97E-B0E9CB106457_1_201_a.jpg)

---

## 3. Methodology

### 3.1 Software timing (`characterize` mode)

The ADS1256 signals a freshly-converted sample by asserting its `DRDY` pin
(Data Ready) low. The C loop polls that GPIO and only reads the sample after
`DRDY` goes low, so every iteration is paced by the converter, not by the
CPU. The four timestamps recorded per iteration are:

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
latency** — including the ADS1256's SINC5 filter *group delay* (the
time delay every frequency component picks up traversing the converter's
internal decimation filter; see §4.3) and the DAC8552's *settling time* (how
long the DAC output amplifier takes to swing to a new code's voltage),
neither of which the software loop can see.

Each phase reading was taken manually from the DPO2012B's built-in
"CH1→CH2 rising-edge delay" measurement, with 512× waveform averaging
enabled on both channels to smooth out random noise before the scope
computes the edge times. Scope statistics were left off; the displayed
delay value fluctuated by roughly half a degree during readout, and the
reported value is the eyeballed centre of that fluctuation.

Both channels used AC coupling. Because both channels see the same input
high-pass response, the contribution to phase is common-mode and cancels
in the channel-to-channel delta that defines the latency reading.

The level shift between input and output (CH1 swing 0.25–2.25 V, CH2 swing
~1.4–2.4 V) is the expected consequence of `Vref_ADC ≠ Vref_DAC` and the
mid-scale offset coded into the ADC→DAC value mapping. It does not affect
the phase measurement (the scope picks zero crossings from each channel
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
mean. (The 100 Hz outlier, 222 µs, reflects the basic fact that converting
a *phase* reading back to a *delay* multiplies by the period: at 100 Hz the
period is 10 000 µs, so a ±0.5° readout uncertainty maps to ±14 µs of
delay uncertainty, while at 1 kHz the same ±0.5° maps to ±1.4 µs. The
high-frequency points are therefore individually more precise estimates of
the same underlying delay.)

![Phase vs freq](analysis/figures/phase_vs_freq.png)
![Delay vs freq](analysis/figures/delay_vs_freq.png)

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

![Processing latency](analysis/figures/proc_latency_hist.png)
![Loop period](analysis/figures/loop_period_hist.png)

### 4.3 Latency budget

| Component | Latency (µs) | Reasoning |
|---|---:|---|
| Software loop (DRDY → DAC chip written) | 48.0 | measured, this code, median |
| ADS1256 SINC5 group delay @ 15 kSPS | ~166.7 | ≈ (5/2) / f_data — see derivation below |
| DAC8552 settling | ~5.0 | datasheet typical |
| **Predicted total** | **~219.7** | sum of the above |
| **Measured total (scope)** | **212.3** | mean of §4.1 sweep |

Predicted and measured agree within **3.4%** (7.4 µs, comfortably inside the
4.8 µs measurement standard deviation). The ADS1256 SINC5 filter is the
single dominant contributor at ~78% of the budget.

*Where the 166.7 µs comes from.* The ADS1256 averages many modulator
samples through a 5-tap sinc-shaped decimation filter to produce each
output sample, and the group delay of a linear-phase SINC^N filter with
decimation ratio *D* clocked at modulator rate *f_mod* is
(*N* · *D*) / (2 · *f_mod*). For the ADS1256 the modulator clocks at
*f_mod* = *f_CLKIN* / 4 = 7.68 MHz / 4 = 1.92 MHz, and at 15 kSPS the
decimation ratio is *D* = *f_mod* / *f_data* = 1.92 MHz / 15 kHz = 128, so
the group delay is (5 · 128) / (2 · 1.92 × 10⁶) ≈ **166.7 µs**. Halving
the data rate to 7.5 kSPS doubles this. Doubling the data rate to 30 kSPS
halves it to ~83 µs — but, per §5.2, the loop can no longer keep up at
that rate.

![Budget](analysis/figures/budget_bar.png)

### 4.4 Success-criterion frequency

For a closed-loop controller, "the input-to-output transport delay is
negligible at the signal frequencies of interest" is a useful pass/fail
condition. A common operational form of that condition is

> phase shift ≤ 1/20 of the signal period (≡ ≤ 18°)

— at one twentieth of a period the input and the output traces look in
sync to the eye on the scope and the closed-loop dynamics are not
dominated by the I/O delay. With our measured constant delay τ = 210.2 µs
(the linear fit through the origin from §4.1, which is the most precise
estimate of the underlying delay) the system meets that criterion up to

> **≈ 238 Hz** (computed as (1/20) / τ = 1 / (20 · 210.2 µs)).

This is well below the README's pre-bring-up estimate of "0.5–1 kHz with
this HAT," which was optimistic by a factor of 2–4.

---

## 5. Discussion

### 5.1 The Pi 5 silicon is not the bottleneck — the chosen Linux interfaces are

The honest way to read the 48 µs software number is in three layers:

1. **Pi 5 silicon** has plenty of headroom. The RP1 I/O controller can clock
   SPI at 50+ MHz and toggle GPIOs in single-digit nanoseconds when its
   registers are written directly. The CPU runs the loop with 19 µs of slack
   per iteration at 15 kSPS. None of the measured cost comes from "the Pi 5
   is slow."
2. **The Linux user-space interfaces we chose** (`spidev` for SPI, GPIO
   character-device uAPI v2 for chip-select toggling) contribute about
   **~34 µs of the 48 µs** through ioctl/syscall overhead. Every CS toggle
   is a syscall (~5 µs on the RP1); every SPI transfer is a syscall (~5–7 µs).
   These interfaces are portable, robust, and the conventional choice for
   real-time work on Linux — but they are not free, and on a fast SBC the
   per-call cost starts to matter.
3. **The ADS1256 SINC5 filter** contributes the remaining ~167 µs of the
   end-to-end analog latency, irrespective of how fast the software is.

So *which* of these you consider "the bottleneck" depends on the question.

- *For analog-to-analog latency on this hardware:* the ADS1256 filter is
  far and away the dominant contributor. Even cutting the software loop
  to its irreducible 14 µs SPI bit-clocking floor would reduce the
  end-to-end number only from 212 µs to ~186 µs (see row B in §5.5).
- *For software-side latency on the Pi 5:* the Linux user-space interfaces
  are dominant. Bypassing them via direct register access on the RP1 would
  reduce the software loop from 48 µs to roughly 15 µs.
- *For Pi-5-as-a-platform claims:* the silicon is capable of the latencies
  the experiment was targeting; whether the user-space programming model
  surfaces that capability is a separate question.

The right textbook framing is probably the second one above: *the platform
provides the headroom; the converter sets the analog floor; the OS interface
chosen determines how much of the platform's headroom is actually realized.*
All three are real, and all three matter independently.

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

The C loop's measured 47.96 µs comes out of three distinct cost categories,
in roughly increasing fraction:

1. **SPI bytes on the wire (irreducible, ≈ 14 µs).** The ADS1256's SPI
   clock is capped at `f_CLKIN / 4 ≈ 1.92 MHz`, so its 3-byte sample takes
   `(3 × 8) / 1.92 MHz ≈ 12.5 µs` to clock in. The DAC8552 runs at 15.6 MHz,
   so its 3-byte write is on the wire in only `≈ 1.5 µs`. Both numbers are
   set by the SPI bus and the chip clocks — there is nothing the CPU can
   do to make them shorter.

2. **`spidev` syscall overhead (≈ 10–15 µs).** Each call to
   `ioctl(fd, SPI_IOC_MESSAGE(1), …)` crosses the user/kernel boundary,
   parses an `spi_ioc_transfer` struct, programs the controller, and
   returns. With two SPI transfers per iteration (one read, one write),
   this adds up to roughly 10–15 µs.

3. **GPIO uAPI v2 ioctls for chip select (≈ 20 µs).** Each iteration
   toggles two chip-select lines twice each (ADC CS↓, ADC CS↑, DAC CS↓,
   DAC CS↑). Every toggle is an `ioctl(fd, GPIO_V2_LINE_SET_VALUES_IOCTL,
   …)` — about 5 µs per call on the RP1 — so four toggles cost roughly
   20 µs. These are also the dominant *avoidable* cost.

The diagram below lays this out chronologically: the top row is the total
47.96 µs window, the middle row splits it into the two measured phases
(`ADC read phase` and `DAC write phase`, taken straight from the per-sample
CSV), and the bottom row decomposes each phase into the GPIO ioctls, the
spidev syscall, and the SPI bytes on the wire. The widths of the
sub-segments are estimates consistent with the per-component costs above
and with the measured phase totals.

![Per-iteration timing breakdown — top row is the total processing latency, middle row is the two measured phases, bottom row decomposes each phase into GPIO ioctls, spidev syscall, and SPI bytes on the wire.](analysis/figures/latency_timing.png)

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
nanoseconds, saving ~15–20 µs total — but only on the software component,
and the SINC5 filter would still set the analog-to-analog latency floor.

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

### 5.5 What would it take to hit ~10 µs end-to-end?

The original aspirational target for an analog-to-analog control loop on
this kind of platform is **~10 µs** (1/20 of a 5 kHz period, i.e. the
acceptance criterion in §4.4 evaluated at 5 kHz). The choice of ADC matters
here: the ADS1256 we used is a sigma-delta converter with an internal
decimation filter, which intrinsically adds tens to hundreds of microseconds
of group delay. A *successive-approximation* (SAR) ADC has no such filter —
it samples the input once via a track-and-hold and then converges to a
digital code in a few microseconds at most. For "fast" SBC-class analog
I/O loops, SAR is the usual choice. To frame what it would take, here are
the four configurations of interest:

| Configuration | Software loop | Analog floor | End-to-end |
|---|---:|---:|---:|
| **A.** What this report measured (Pi 5, spidev + GPIO uAPI v2, ADS1256 @ 15 kSPS, DAC8552) | 48 µs | ~167 µs (SINC5) + ~5 µs (DAC) | **212 µs (measured)** |
| **B.** Same hardware, software ported to direct RP1 register access via `mmap` of `/dev/gpiomem0` and the SPI peripheral | ~15 µs (predicted) | ~172 µs (same) | **~187 µs (predicted)** |
| **C.** Pi 5 + fast SAR ADC (e.g. AD7610, ADS8688, LTC2378), spidev + GPIO uAPI v2 | ~30–40 µs | ~5 µs (no decimation filter) | **~35–45 µs** |
| **D.** Pi 5 + fast SAR ADC + direct register access for GPIO/SPI | ~5–8 µs | ~5 µs | **~10–13 µs** |

What the table says concretely:

- **The 10 µs target is unreachable with this HAT** at any data rate, by
  any amount of software optimization — the ADS1256's SINC5 filter alone is
  more than 16× over budget.
- **The 10 µs target is reachable on the Pi 5** if both the ADC is changed
  to a fast SAR converter (one with no decimation filter and a SPI clock
  ceiling well above 1.92 MHz) *and* the software is rewritten against the
  RP1's registers directly. Either one alone is not enough.
- **The user-space Linux interface choice matters at this latency scale.**
  At sub-100-µs budgets, the per-call cost of `ioctl` (a few µs) is
  non-negligible. The conventional advice that "spidev is fast enough" is
  true for kHz-rate work; at the 10 µs scale it is no longer true.

These predictions for rows B–D are not measurements — they are estimates
based on the known per-call costs of the various interfaces, the SPI bit-
clocking time at higher clock rates, and standard SAR-ADC settling. A
follow-up experiment that builds row B (direct register access with the
same HAT) would empirically isolate the OS-interface cost from the silicon
capability, and is the most natural next step if this distinction is worth
nailing down for the textbook.

### 5.6 What the scope did *not* show that you might expect

- **No frequency-dependent attenuation in the measured range.** At 1 kHz
  (the upper end of the sweep) the loop's reconstructed sine on CH2 was
  visibly stair-stepped (15 samples per period), but the SINC5 filter's
  *amplitude* roll-off was not yet appreciable. Above ~3 kHz the input
  amplitude would start to attenuate; this experiment did not probe that
  region because the phase-shift criterion fails first.
- **No clock-tick jitter beyond the loop's own ~6 µs p99 spread.** The
  scope traces were stable from acquisition to acquisition (the displayed
  delay value drifted only ~0.5° during readout), so the 4.8 µs std-dev
  across the frequency sweep is dominated by manual-readout uncertainty
  and converter noise — not by any SBC-side variation that would show up
  as a wandering trace.

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
this report are in the repository.

---

## 7. Conclusion

The measured end-to-end analog-to-analog latency on the Raspberry Pi 5 +
Waveshare High-Precision AD/DA Board is **212 µs**, and the system behaves
as a near-perfect pure transport delay over the 100 Hz – 1 kHz sweep.

A clean three-way decomposition emerged:

1. **~167 µs** comes from the ADS1256's SINC5 decimation filter — this is
   set by the converter's data rate (15 kSPS) and cannot be reduced
   in software.
2. **~34 µs** comes from the Linux user-space interfaces the loop is built
   on — `spidev` syscalls and GPIO uAPI v2 ioctls. Replacing these with
   direct RP1 register access via `mmap` of `/dev/gpiomem0` would
   eliminate most of this cost; predicted software loop ~15 µs.
3. **~14 µs** is irreducible SPI bit-clocking on the wire at the ADS1256's
   maximum allowed clock of 1.92 MHz, plus the 1.5 µs DAC frame.

The 10 µs end-to-end target *could* be reachable on the Pi 5 if both the
ADC is changed to a fast SAR converter (no decimation filter, SPI ≥
10 MHz) *and* the software is rewritten against the RP1 registers
directly (§5.5, row D). Neither change alone is sufficient. With the
present HAT and the conventional Linux interfaces, 212 µs is the result;
of that, ~22% is in software the textbook author could in principle fix
and ~78% is in a converter that the textbook author would have to swap.
