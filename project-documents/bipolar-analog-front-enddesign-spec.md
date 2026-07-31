---
project_note_id: 1019
title: Bipolar Analog Front-End—Design Spec
---

# Bipolar Analog Front-End—Design Spec

**Real-Time Computing for Mechanical Engineers, 2nd Edition**
Signal-conditioning subsystem for integration into the course's custom motor/interface board—internal—July 2026

Sub-circuit schematics (panels A, B, C) are embedded in their sections below.

---

## Scope and assumptions

One bipolar analog input (AI) and one bipolar analog output (AO), ±5 V nominal, interfacing a 3.3 V MCU (works identically for the LaunchPad F28P55X and the Nucleo-H533RE—the design touches only 3V3A/VDDA, one ADC pin, one DAC pin, +5 V, and ground). Designed as a subsystem to drop into the existing custom board rather than a standalone shield, and hardened against the standard student abuse cases: overvoltage on the jacks, shorts to ground and to supply rails, ESD, and miswired power. All values verified numerically; a ±3.3 V variant is a two-resistor change (table at the end).

**Signal plan:**

| Path | Field side | MCU side | Transfer function |
|---|---|---|---|
| AI | ±5 V | 0–3.3 V ADC | V_adc = 0.332·V_in + 1.65 |
| AO | ±5 V (±4.9 V guaranteed) | 0–3.3 V DAC | V_out = 3.01·(V_dac − 1.65) |

---

## Panel A—analog input: ±5 V → 0–3.3 V

![frontend-panel-a-input.png](https://projects.ricopic.one/rtc-book/images/8/raw/)

A three-resistor passive network does the attenuation and level shift in one step: R1 (30.1 kΩ) from the jack, R2 (20 kΩ) to 3V3A, R3 (59 kΩ) to ground meet at one node, giving V_node = 0.332·V_in + 0.499·VDDA—i.e. ±5 V maps to 0.00–3.30 V (verified: −5 V → −0.011 V, +5 V → 3.305 V; the ~10 mV overrange at the extremes is clipped by the buffer and absorbed in calibration, or back R1 off to 30.9 kΩ for guaranteed margin). U1A buffers the ~10 kΩ Thevenin impedance so the ADC's sample-and-hold sees a stiff source—this matters at the C2000's 3.9 MSPS acquisition times and equally for the H533. R4 (1 kΩ) plus the BAT54S clamp pair (D1a/D1b) then bound the ADC pin to −0.3…+3.6 V no matter what the buffer does.

Because the network is referenced to VDDA and the ADC also uses VDDA as its reference, the conversion is **ratiometric**: supply drift cancels to first order. Gain accuracy is set by resistor matching (0.1% parts → 0.13% worst-case on the input network, 0.20% on the output stage, verified by exhaustive corner analysis), which the two-point software calibration below removes entirely.

A note on the source: both finalist MCUs drive this stage from a **true buffered DAC** and read it with a conventional SAR ADC—there is no PWM filtering anywhere in this signal chain, on either board. (The "PWM DAC" pins mentioned in the TI LaunchPad user guide are an optional debug facility with unpopulated filters; the front-end uses the real DACA_OUT / PA4 outputs.)

Add a capacitor across R3 (e.g., 2.2 nF, giving f_c ≈ 7 kHz with the ~10 kΩ node impedance) as a single-pole anti-aliasing filter; size it to the labs' loop rate.

---

## Panel B—analog output: 0–3.3 V DAC → ±5 V

![frontend-panel-b-output.png](https://projects.ricopic.one/rtc-book/images/9/raw/)

A classic four-resistor difference amplifier around U1B: the DAC drives the non-inverting leg through R5 (10 kΩ) with R6 (30.1 kΩ) to **ground**; V_mid (1.65 V) drives the inverting leg through R7 (10 kΩ) with R8 (30.1 kΩ) as feedback. With both ratios matched at 3.01, V_out = 3.01·(V_dac − V_mid): DAC code zero → −4.97 V, mid-code → 0 V, full-code → +4.97 V (verified numerically). Since V_mid is derived from VDDA/2 and the DAC full-scale is VDDA, this stage is also ratiometric—V_out = 3.01·VDDA·(D/4095 − ½)—so rail drift cancels here too.

One honest headroom note: a rail-to-rail op-amp on ±5 V rails swings to within a few tens of millivolts of the rails into the light load of a motor-amp input, so the guaranteed full-scale is **±4.9 V**, with ±4.97 V typical. If the labs need true ±5.0 V with margin, run the op-amps from the custom board's existing ±12 V (or motor) supply instead of the charge pump—the specified OPA4990 is a 40 V part precisely so that this is a drop-in option (adjust TVS to SMAJ10CA and re-check clamp currents).

---

## Panel C—mid-rail reference and power

![frontend-panel-c-vmid-power.png](https://projects.ricopic.one/rtc-book/images/10/raw/)

Panel C is pure housekeeping for the other two panels, and it's worth being precise about who uses what:

**V_mid (U1C and the R10/R11 divider) serves Panel B only.** It is the 1.65 V reference for the difference amplifier's inverting leg (via R7)—the voltage the output stage subtracts from the DAC signal so that mid-scale lands at 0 V. Panel A does *not* use V_mid: the input network generates its own level shift directly from VDDA through R2, which is both simpler and keeps the input path's offset ratiometric on its own terms. So if V_mid were removed, the analog input would be unaffected and the analog output would pin to one rail.

**Why V_mid needs a buffer at all:** the diff amp draws a *signal-dependent* current through R7 (as V_out swings ±5 V, the current in the R7/R8 leg changes). A bare 10 kΩ/10 kΩ divider has a 5 kΩ source impedance, which would add to R7, wreck the carefully matched 3.01 ratio, and make the gain error vary with output level. Buffering makes V_mid a stiff source; since U1C is a spare section of the quad package it's free. Don't be tempted to use an absolute 1.65 V reference IC instead—ratiometric cancellation *requires* V_mid to track VDDA/2.

**The power block serves both panels.** The LM27762 charge pump generates the ±5VA rails that power all sections of U1 (the input buffer, the output stage, and the V_mid buffer), from the MCU board's +5 V through polyfuse F1. Total analog draw is a few mA, far inside its capability. As noted above, an existing ±12 V rail on the custom board is a fine substitute—one fewer IC.

---

## Protection: fault analysis

Design targets: survive any jack forced to ±24 V continuously, any output shorted indefinitely, ESD per the TVS ratings, and gross miswiring of the 5 V feed—with no component replacement needed (all protection is self-resetting).

| Fault (student-typical) | What happens | Outcome |
|---|---|---|
| AI jack forced to +24 V | R1 limits fault current to 0.61 mA (11 mW in R1); node clamps at ~5.5 V via U1A's integrated input clamp, which carries only **0.41 mA** (rated 10 mA); TVS holds transients | Survives indefinitely |
| AI jack forced to −24 V | Mirror case (0.61 mA through R1); U1A output rails at ~−4.9 V; R4 + D1b clamp the ADC pin at −0.3 V with ~4.7 mA shunted to ground | ADC pin protected; survives |
| AI jack ESD / inductive spike | SMAJ10CA TVS clamps at the connector before anything sensitive | Survives |
| AO shorted to ground at full scale | ~23 mA through R9; OPA4990 output is short-circuit-limited anyway | Survives indefinitely |
| AO forced to +12 V (e.g., wired to supply) | Without protection ~77 mA would flow—hence F2, a 50 mA polyfuse: trips in well under a second, fault current falls to mA; SMAJ6.0CA clamps transients | Self-resets when fault removed |
| AO wired to AI (very common) | AO drives the AI divider (~40 kΩ load)—harmless; actually a useful loopback self-test the lab can exploit | Fine by design |
| +5 V feed reversed or overloaded | F1 polyfuse; add a series Schottky (optional D2, MBR120) if reverse polarity is a realistic assembly risk | Self-resets |

Two deliberate choices worth flagging: the PTC (not a plain resistor) on the AO is what converts the worst realistic fault—output wired to a supply—from a burned op-amp into a self-resetting nuisance; and every clamp references a rail that can absorb the current (the BAT54S pairs into 3V3A/GND see sub-5 mA worst case, within any regulator's sink/source tolerance at these levels).

---

## Bill of materials (per station, qty-25 pricing, approx.)

| Ref | Part | Function | ~Cost |
|---|---|---|---|
| U1 | OPA4990 (quad, RRIO, 40 V) | buffer / output stage / V_mid | $1.40 |
| U2 | LM27762 | ±5 V charge pump (omit if ±12 V exists) | $1.90 |
| D1 | BAT54S (dual Schottky) | ADC pin clamp | $0.15 |
| Z1 | SMAJ10CA | AI jack TVS | $0.30 |
| Z2 | SMAJ6.0CA | AO jack TVS | $0.30 |
| F1 | PTC 500 mA | 5 V feed | $0.30 |
| F2 | PTC 50 mA | AO fault limit | $0.35 |
| R1–R11 | 0.1% 25 ppm thin-film, 0603 | signal network | $1.10 |
| C | 100 nF ×4, 2.2 nF ×1, charge-pump caps | decoupling / AA filter | $0.50 |
| | | **Total** | **≈ $6.30** |

---

## ±3.3 V variant (alternative build, if that range is chosen)

| Element | ±5 V build | ±3.3 V build |
|---|---|---|
| Input network | R1 30.1 k / R2 20 k / R3 59 k | R1 20 k / R2 20 k / **R3 omitted** |
| Output gain pair | R6 = R8 = 30.1 k (gain 3.01) | R6 = R8 = 20 k (gain 2.00) |
| AO TVS | SMAJ6.0CA | SMAJ5.0A works |
| Guaranteed AO full-scale | ±4.9 V | ±3.3 V (ample headroom on ±5 rails) |

The ±3.3 V input network is elegantly minimal—two equal resistors, V_adc = (V_in + 3.3)/2—and the output stage gains margin. Functionally either range works; choose from the actual sensor/actuator interfaces on the custom board.

---

## Integration notes for the custom board

Route the conditioning section's ground as analog ground, tied to the motor amplifier's power ground at a single point, and keep the divider node, V_mid, and the diff-amp resistors away from the PWM/motor traces (the 30 kΩ nodes are the sensitive ones). Take 3V3A/VDDA from the MCU board's analog supply pin, not from a digital 3.3 V rail, so the ratiometric assumption holds. Star the two TVS grounds close to their jacks. If the board already carries ±12 V for the motor amp, delete U2/F1 and run U1 from ±12 V—everything else is unchanged except the AO TVS (→ SMAJ10CA) and the guaranteed swing (true ±5 V with 7 V of margin).

---

## Calibration (and the lab it becomes)

With 0.1% resistors, uncalibrated accuracy is ~0.2% of full scale plus offsets—already fine for control labs. A two-point software calibration (drive the DAC, loop AO→AI externally, sweep, least-squares the gain/offset of the combined chain, then separate the two paths with one known external voltage) reduces residual error to ADC noise. This procedure is exactly the "characterize your instrument" exercise the first edition never had, and it exercises the DAC, ADC, UART logging, and the students' understanding of the transfer functions in one lab.

---

## Design decisions summary

Chosen: passive-divider input (simplest thing that survives ±24 V), four-resistor diff-amp output (canonical, one op-amp, ratiometric), VDDA-derived V_mid (ratiometric), quad 40 V op-amp (one package, supply-flexible, short-circuit-proof), PTC + TVS at both jacks (self-resetting protection), charge-pump ±5 V with a documented ±12 V alternative. Rejected: instrumentation amps and precision references (overkill; break ratiometric cancellation), on-chip op-amps/PGAs as primary stages (ties the design to one MCU and provides no fault isolation—an external quad op-amp *is* the sacrificial protection layer for the MCU's analog pins), and relying on op-amp abs-max ratings in place of clamps (students exceed abs-max ratings for a living).