---
project_note_id: 1018
title: 'TI vs ST: Next-Generation Platform Finalists'
---

# TI vs ST: Next-Generation Platform Finalists

**Real-Time Computing for Mechanical Engineers, 2nd Edition**
Board comparison—internal—August 2026 (rev 10)

> **Revision note.** Rev 8 corrected a major error: earlier revisions claimed the STM32H533 lacked hardware encoder index and error detection, which was true of older STM32 families but false for this part. Rev 9 corrected four more things: TI makes no pinout-compatibility guarantee for controlCARDs; the LaunchPad's USB isolation is defeated by default jumpers; neither the H533 nor the H563 has an on-chip op-amp or comparator; and the Nucleo-H563ZI costs ~$39–44, not ~$27. **Rev 10 corrects the controlCARD price**, which earlier revisions gave as $190–260—that is the **F28P65X** card's price, mistakenly applied to the F28P55X card as well. The P55x controlCARD is **~$79 direct from TI** (~$94–99 through authorized distributors). This materially changes the replacement-cost argument and strengthens the controlCARD option, so the form-factor section, the requirements table, and the recommendation have been revised.

---

## Context and bottom line

The first edition's platform, the NI myRIO-1900, is discontinued, and a replacement is needed for the second edition. An earlier memo made the case for the TI LaunchPad F28P55X; the group asked that an ST evaluation board be considered head-to-head, and added one new requirement: at least one bipolar analog input and one bipolar analog output (±5 V is sufficient; ±3.3 V acceptable—the myRIO provided ±10 V, but the labs do not need the full range).

Two finalists: the TI **LaunchPad F28P55X** (TMS320F28P550SJ) and the ST **Nucleo-H533RE** (STM32H533RE)—both 2024 silicon. Two other ST boards were evaluated and set aside: the Nucleo-G474RE (2019 silicon, seven years into its life) and the Nucleo-H563ZI (see the up-market scan).

The analog front-end is essentially identical on either board, so on-chip analog does not differentiate them. Encoder *hardware* is close to a wash once the H533's actual registers are checked. **What decides this comparison is software and documentation, not silicon**: TI ships a GUI that configures every eQEP feature, two complete velocity examples that run on jumper wires, and a board-specific Academy lab, where ST ships no encoder example anywhere in STM32CubeH5 and no application note on encoder velocity. Around that sit the CLA, PWM depth, and the TMU. **The lean is TI, on the grounds that the labs can actually be written.**

---

## Lifecycle: reading the dates correctly

**ST publishes a date, but it's a rolling floor, not a launch-anchored runway.** ST's "available until" dates are a **10-year commitment restarting from January 1 of the current year**. Every active STM32 currently shows January 2036 regardless of launch year—the H563 (announced March 2023) and the H533 (April 2024) carry the identical 01/2036 date. ST has historically kept renewing while a part stays in the longevity program, but the guarantee at any moment is ~10 years ahead, which is why design freshness still matters.

**TI publishes no per-part date but has the stronger track record.** The F28P55x launched in 2024; TI's C2000 parts routinely stay in production 15–20 years (the 2007-era F2833x and 2016-era F28379D are both still ACTIVE). Expectation: production into the late 2030s.

**Conclusion:** with both finalists on 2024 silicon, expected longevity is comparable and excellent; TI's assurance is precedent-based, ST's is contractual-but-rolling.

---

## The finalists in brief

The **TI LaunchPad F28P55X** (~$35–36, in stock at DigiKey and Mouser) carries the TMS320F28P550SJ: a 150 MHz C28x real-time core plus an FPU, a small NPU, and two control-specific accelerators. The **CLA (Control Law Accelerator)** is an independent 150 MHz floating-point co-processor that runs the fast inner control loop in parallel with the main core—triggered directly by peripheral events with direct register access and no interrupt arbitration, so the loop executes with essentially zero jitter regardless of what the main CPU is doing. The **TMU (Trigonometric Math Unit)** extends the FPU with hardware sin/cos/atan2/sqrt/divide **instructions** completing in a few cycles—the exact operations field-oriented motor control evaluates every iteration. Around these: five 12-bit ADCs (3.9 MSPS), one buffered 12-bit DAC, three dedicated eQEP encoder modules (two connectors pre-wired on the board), 24 ePWM channels (12 with 150 ps high-resolution mode), and PGAs in front of the ADCs.

The **ST Nucleo-H533RE** (~$23–26, in distribution) carries the STM32H533RE: a 250 MHz Cortex-M33F—375 DMIPS, notably quicker in scalar terms than the C28x. It covers every book requirement: two 12-bit ADCs at up to 5 MSPS, a **two-channel** buffered 12-bit DAC, quadrature-encoder mode with hardware index on six timers, an advanced motor-control timer for PWM, three USARTs plus UARTs, and plenty of GPIO. Note two absences corrected in rev 9: it has **no on-chip op-amp or comparator** (ST's web parametric table says otherwise, but the datasheet, the CMSIS headers, and ST's pin database all agree it does not), and it has **no CORDIC and no FMAC**—those belong to the H56x/H57x parts, even though RM0481 documents them because that manual covers several families jointly.

---

## The bipolar front-end at ±5 V / ±3.3 V: a wash

Both MCUs have strictly unipolar 0–3.3 V ADCs and DACs, so bipolar I/O requires external signal conditioning on either board—and that conditioning is board-agnostic, because the one amplifier that must exist (the bipolar output stage) is external on both chips: a 3.3 V-railed on-chip op-amp can never swing below ground, at ±10 V or ±5 V alike. This is also why targeting ±5 V rather than the myRIO's ±10 V is worth it. First, the power problem collapses—driving ±10 V would need op-amp rails around ±12 V, which on a USB-powered station means a boost converter plus an inverter; ±5 V outputs run from a ~$2 charge pump off the 5 V rail the board already has. Second, protection gets easier—with no ±12 V rails on the bench, worst-case fault voltages and clamp energies at the student-facing jacks drop. Third, at ±3.3 V (though not at ±5 V) the input network degenerates to two equal resistors.

**Correction (rev 9): on-chip input buffering is a TI-only option, not a wash.** Earlier revisions said both chips could buffer the ADC input on-chip. TI can, via its **PGAs (Programmable Gain Amplifiers)**—on-chip amplifiers in front of the ADC whose gain is selected in software from fixed internal steps, unity included. A PGA is less flexible than a true op-amp because its feedback network is internal, but as an ADC input buffer it does the job with zero external parts. The H533 has nothing equivalent: no op-amp, no comparator. In practice this changes little, because the design uses an external quad op-amp anyway as the sacrificial protection layer—but the claim as written was wrong and is now corrected.

---

**Input (bipolar signal → 0–3.3 V ADC).** At **±3.3 V**, equal resistors from the signal and from the 3.3 V rail produce (Vin + 3.3)/2, mapping ±3.3 V onto 0–3.3 V. At **±5 V**, it's a three-resistor network with the same idea, plus an RC for anti-aliasing. No active parts are strictly required if the source impedance is modest.

**Output (0–3.3 V DAC → bipolar).** An external part is unavoidable on *every* candidate: swinging below ground requires an op-amp powered from a negative rail. The canonical stage is one op-amp as a level-shift/gain block (gain ≈ 2–3, referenced to 1.65 V) on ±5 V rails, with the negative rail from a <$1 charge-pump inverter or the bench supply. Identical BOM for TI and ST.

**Net per-station front-end:** one quad op-amp, one charge pump, and a half-dozen precision resistors—a ~$6 BOM, the same design either way, integrated into the course's custom interface board. A complete worked design exists as a companion document (*Bipolar Analog Front-End—Design Spec*). Pedagogically this circuit is a gift: students analyze the exact signal-conditioning chain every real data-acquisition front-end contains.

---

## DAC architecture: both are true DACs—no PWM smoothing required

Neither board's DAC is a filtered PWM output, though TI's documentation invites the worry. The F28P55X LaunchPad user guide (§2.1.3.8) describes "PWM DAC signals" on four BoosterPack pins produced by low-pass-filtering a PWM output, with the RC filters **not populated** by default. That is an *optional, auxiliary* debug facility—a C2000 tradition for watching internal variables on a scope—**not** the device's actual DAC.

- **F28P55X**—one true buffered 12-bit DAC (resistor-string converter with on-chip output buffer, "DACA"). DACA_OUT is on analog pin **A0**, physical pin 23 of the 100-pin PZ package, routed to the BoosterPack headers per SPAZ056. (Note for the carrier schematic: A0 is shared between DACA_OUT and ADC input A0, so that channel is one or the other.)
- **H533RE**—one true 12-bit DAC peripheral with **two output channels**, each a resistor-string converter with a switchable buffer, on PA4/PA5, exposed on the Nucleo headers.

---

## Encoder interfaces: hardware near-parity, software not close

Earlier revisions rested the recommendation on encoder hardware and were wrong to. Rebuilt from ST's register definitions, CMSIS headers, HAL sources, and pin database: **both parts decode quadrature in hardware, both reset the counter on the index pulse in hardware, both flag illegal quadrature transitions in hardware, and both can measure velocity by T or 1/T without external wiring.** The decisive differences turn out to be in the software and documentation, not the silicon.

**Index (Z)—parity, arguably ST ahead.** The eQEP latches and/or resets position on the index edge. The H533's `TIM_ECR` does the same: `IE` enables counter reset on index, with `IDIR`, `IBLK`, `FIDX`, and `IPOS` qualifying by direction, blanking against a third input, taking only the first index, and selecting which A/B state the reset lands on. ST additionally raises `IERRF` when the index arrives inconsistent with the expected A/B state. The earlier claim that Z requires an EXTI interrupt was true of the F4/F7/H7 generation and false here.

**Integrity—parity, and neither part corrects.** The eQEP flags phase errors and can interrupt on direction change; the H533 raises `TERRF` (transition error), `DIRF`, `IDXF` and `IERRF`. Worth stating explicitly in the book: **no quadrature decoder in this class performs error *correction*.** Both only detect and set a flag; recovery is always software's job. The closest thing to correction is index-triggered re-initialization, which both have, and which *bounds* accumulated error to one revolution rather than repairing a miscount. TI simply provides more independent detectors—phase error, a per-revolution position-count check, capture-validity flags, and a hardware **stall watchdog (QWDOG) that ST has no equivalent for**.

---

**Velocity—TI leads, but less than claimed.** The eQEP contains both methods in one peripheral: a unit timer for T, and an edge-capture unit with its own capture timer and a **1-to-2048** edge-stride prescaler for 1/T, with position, capture period, and capture timer latched **atomically** on the unit time-out event.

The H533 can do 1/T entirely on-chip, but as a two-timer composition: the encoder timer emits an **encoder clock** on its trigger output (`CR2.MMS = 1000`, documented in AN4013, available on all six encoder-capable timers), which routes internally to a second timer's `ITRx`, and that timer captures on `TRC`—so each captured value timestamps an encoder edge. Note why the obvious single-timer approach fails: in encoder mode the counter **is** position, so capturing on its own channels latches position, not time. So the honest hardware comparison is one peripheral versus two, a 1–2048 prescaler versus /1–/8, and atomic co-latching versus values from different timers.

**Resource accounting—corrected.** Earlier revisions said the H533 "handles 2 encoders on general-purpose timers," which came from misreading a datasheet bullet about how many timers are 32-bit. Correct: **six** timers support quadrature encoder mode with hardware index (TIM1, TIM2, TIM3, TIM4, TIM5, TIM8); only TIM2 and TIM5 are 32-bit; on the LQFP64 package TIM4 has no index pin, so five have a complete A/B/Z. LPTIM1/2 add a simpler index-less encoder mode. TI's framing survives only in that eQEP modules are dedicated and don't consume timers wanted for PWM or capture—but the H533 has *more* encoder-capable timers than the F28P55x has eQEP modules.

**What ST does that eQEP does not:** nine decode modes including clock-plus-direction and directional-clock variants; the index conditioning above; the `IERRF` consistency check; preloadable slave-mode selection for glitch-free switching; encoder counting in Stop mode via LPTIM; and native DMA on encoder events, where the eQEP is not a DMA trigger source.

---

## Encoder usability: where TI actually wins

With the hardware roughly even, the software is the whole story, and here the two are not close.

**Getting a first count—a tie.** ST is fewer keystrokes: CubeMX generates `MX_TIMx_Init()`, and the student writes `HAL_TIM_Encoder_Start()` plus a counter read—about three lines. TI needs `Board_init()` plus the usual C2000 boilerplate. But TI wins the friction that actually bites: the LaunchPad has a **dedicated QEP header** and SysConfig assigns the pins from board data, so there is no pin-mux research; and QPOSCNT is 32-bit with a hardware `QPOSMAX` wrap, so there is no wraparound code. On the H533 the student must pick a timer and discover that only TIM2 and TIM5 are 32-bit, or write 16-bit rollover handling.

**Adding index/homing—TI by a wide margin.** In SysConfig it is a dropdown, and TI's C2000 Academy eQEP lab *for the F28P55X LaunchPad specifically* already sets it. On ST the API exists and is decent (`HAL_TIMEx_ConfigEncoderIndex` with a seven-field struct), but three frictions stack up: the index arrives on **ETR**, which creates a real pin-routing trap—there is a documented case where a pin was both CH1 and ETR on the same alternate function, producing spurious index interrupts until the channel moved; **no HAL start function enables the index interrupt**, so the student must add `__HAL_TIM_ENABLE_IT(&htim, TIM_IT_IDX)` after reading the header to learn it exists; and there is no evidence CubeMX exposes index configuration in its GUI at all.

**Diagnosing a noisy encoder—TI moderately ahead.** TI: check boxes in SysConfig for the interrupt sources you want, write one ISR. ST: better than expected—the HAL does dispatch four dedicated callbacks (`HAL_TIMEx_EncoderIndexCallback`, `DirectionChangeCallback`, `IndexErrorCallback`, `TransitionErrorCallback`)—but nothing enables the corresponding `DIER` bits for you, and there is no stall watchdog at all.

**Velocity—the largest gap in the entire comparison.** TI ships `eqep_ex1_freq_cal` and `eqep_ex2_pos_speed` in C2000Ware for this device family. Both compute the T-method and 1/T results side by side, both come with design spreadsheets for the prescaler and scaling derivation, and both synthesize the encoder signals from ePWM so **they run with three jumper wires and no motor**—which is very close to the exact lab this book wants. SysConfig even shows a computed capture-timer frequency while you pick the prescalers. On the ST side: **the entire STM32CubeH5 package contains zero files with "encoder" in the path**, the NUCLEO-H533RE has no TIM examples of any kind, AN4776 (the timer cookbook) explicitly scopes out motor control, and AN4013 covers encoder modes with no velocity guidance. X-CUBE-MCSDK has an encoder speed component but it is bound to a Workbench-generated FOC project and requires a rotor-alignment procedure after every reset—not a route to send a mechanical-engineering student down. Building 1/T on the H533 means hand-writing `CR2.MMS`, `SMCR.TS`, `CCMR1.CC1S`, and `CCER.CC1E` with no wrapper, no example, and no app note.

One hazard worth flagging in the book: third-party STM32 encoder tutorials are abundant and nearly all target pre-H5 families, teaching the obsolete EXTI-index workaround. A student following them never learns the index hardware exists.

**Net:** for a first position-count lab the two are equivalent. For index/homing TI is clearly easier. For velocity—including the T-vs-1/T trade-off this book wants to teach—TI is the difference between a lab you can write in a week and one you can't.

---

## Maximum count rate: neither MCU is the limit

Both parts are far beyond anything the labs will reach, so the interesting content is *what actually binds*. For a 512-line encoder at 4× decode, 2048 counts/rev.

| | TI F28P55x | ST STM32H533 |
|---|---|---|
| Raw ceiling | ~150 Mcounts/s (1 count/SYSCLK) | ~250 Mcounts/s (250 MHz timer clock) |
| Equivalent RPM, 512-line | ~4.4 million | ~7.3 million |
| At max input filter | **~3,445 RPM** (GPIO qualifier, /510) | ~14,300 RPM |
| Datasheet spec | `t_w(QEPP) ≥ 2 SYSCLK` | none published |

At 3,000 RPM you need 102,400 counts/s, roughly 1,500–2,400× under the raw ceiling. **The number that can actually bite is the input filter**: TI's GPIO qualifier at its maximum divider requires a ~17 µs stable pulse and caps you around 3,445 RPM, which is inside the range of an ordinary lab motor. That trade—noise immunity bought with bandwidth—is real engineering content and would make a good lab exercise on either platform.

Two limits bind before either MCU does. A typical 512-line optical encoder is rated 100–300 kHz per channel, i.e. 3,000–17,500 RPM equivalent. And long unshielded single-ended cables running beside motor PWM will inject false edges long before anything runs out of speed. Those false edges are *directional* in quadrature, so the count walks off monotonically rather than jittering—a distinctive symptom worth teaching. The cure is a differential (RS-422) encoder, not a faster MCU. Note also that neither part detects a *swallowed* edge, one too fast for the filter: the phase-error and transition-error flags catch illegal transitions, not missing ones.

---

## Control-loop timing: CPU timer ISR vs. CLA task

The first edition implemented control loops as timed loops—a timer interrupt with an ISR executing a difference equation. That model transfers to either finalist unchanged. What the CLA adds is not *how you write the loop* but *how exact its period is*.

**The CPU timer ISR.** A CPU timer with period T fires a PIE interrupt; the CPU saves context, runs the difference equation, restores context. C28x interrupt latency is tens of cycles—sub-microsecond at 150 MHz—so this is entirely viable and is the right first implementation to teach. Its limitation is contention: the ISR shares a CPU with UART logging, keypad polling, and every other interrupt source. Higher-priority ISRs delay it, equal-priority ones queue behind it, and context save/restore varies with what was executing. The result is jitter in T, which means the sampling period the z-domain coefficients assume drifts from the period actually realized—and the deviation grows precisely as the CPU is loaded with the I/O other chapters teach.

**The CLA task.** Eight tasks, each with a hardware-selectable trigger. A task is non-preemptive and runs to completion, launched with a fixed few-cycle latency. Nothing else executes on the CLA, and the trigger reaches it directly from the peripheral rather than through the PIE. Persistent state lives in CLA data RAM between invocations, and TI's Digital Control Library ships its compensators in exactly this form.

---

**Two ways to build the timed loop on the CLA.** The direct translation keeps the existing mental model: configure a CPU timer with period T as the trigger for CLA Task 1. Structurally identical to the historical timed loop, with the CPU uninvolved. The canonical pattern is better and is the one worth teaching: let an ePWM time base define T, have it fire an ADC start-of-conversion at the same phase every period, and trigger the CLA task from the ADC's end-of-conversion. Now the *sampling instant* is hardware-exact, computation begins the moment the sample exists, and the sample-to-actuation delay is constant.

**The discipline it imposes**—worth a paragraph in the book—is that task execution time must remain below T. An overrun produces a late or missed tick: the classic real-time scheduling failure, made concrete and measurable.

**A pedagogical sequence:** implement the loop first as a CPU timer ISR, instrument it with a GPIO toggle, and measure jitter on a scope while loading the CPU with UART traffic. Then move the same difference equation to a CLA task and watch the jitter collapse. Determinism stops being an assertion and becomes an observation.

### Would a second core do the same thing?

A reasonable question, since dual-core parts exist. The answer is half yes, and the distinction is worth teaching.

A dedicated second core solves the **scheduling** problem—nothing else competes for that processor, which is the dominant jitter source in practice. That is a real win. It does not solve the **dispatch** problem: the trigger still arrives through the interrupt controller with register stacking and unstacking (ST's own AN5788 measures roughly 29 cycles in and 27 out on a Cortex-M4, about 56 cycles of overhead), plus cache-state variance and bus arbitration with the other core and the DMA engines. The CLA has no interrupt controller in the path, no stack, no preemption, and dedicated buses into private zero-wait-state RAM. **CLA determinism is a property of the hardware; second-core determinism is a property of your discipline**—achievable with care, silently breakable by a later maintainer.

What a second core gives you that the CLA cannot is substantial and worth crediting: full C/C++ (CLA C is a restricted subset—no recursion, no standard library), far more memory, full peripheral access, an RTOS, and real debugging. Different tool, not a substitute.

Note that "more cores" is not an ST differentiator. TI ships dual-C28x parts, and ST's only dual-core MCUs are the 2018-era H745/H747/H755/H757, which have neither CORDIC nor FMAC—you cannot get ST's math accelerators and ST's second core in the same package.

**On the ST side generally**, the equivalent of the CLA is a timer ISR on the Cortex-M33, with the same contention characteristics as the C28x timer ISR. The peripheral-triggered path (timer → ADC → DMA) can remove the CPU from the data path, but there is no independent co-processor to host the control law. The CPU-loop story is a wash; the CLA option exists only on TI. **This is now the strongest hardware differentiator in the comparison.**

---

## C library and driver support

Both vendors ship free, comprehensive C support, so neither board is risky—but they are not identical.

**TI: C2000Ware.** One SDK with the peripheral driver library (driverlib), bit-field register headers, SysConfig code generation, device examples, FreeRTOS demos, and the math libraries. Driverlib is thin, readable, one-function-per-operation wrappers over registers (`EQEP_setPosition(...)`)—exactly the abstraction level a real-time computing course wants, since students can read the driver source and see the register write. Two inclusions matter specifically: the **Digital Control Library (DCL)**—PID/PI/2p2z/3p3z compensators hand-optimized for the C28x and CLA with anti-windup and saturation handling, i.e. the exact algorithms the book teaches, as documented reference code—and **IQmath** if the book keeps its fixed-point material. C2000 Academy teaches against driverlib, so course and vendor materials align.

**ST: STM32CubeH5.** The HAL plus the LL (low-layer, register-close—the better teaching layer, comparable in spirit to driverlib), CMSIS core and CMSIS-DSP (FIR/IIR/matrix/PID), BSPs, and middleware. Three caveats: the Cube H5 example projects target the **NUCLEO-H563ZI, NUCLEO-H503RB, and H573I-DK—not the NUCLEO-H533RE**; the packaged RTOS middleware is ThreadX rather than FreeRTOS (which runs fine but arrives from outside the vendor package); and, as above, there is **no encoder example and no encoder-velocity application note anywhere**.

**Verdict:** both clear the bar; the difference is alignment. TI provides examples for this exact board, a control-law library mirroring the syllabus, documented encoder velocity methods, and the book's preferred RTOS in-box. This gap is doing more work in the recommendation than the silicon is.

---

## Does Arm matter for us?

It's tempting to count "Arm" as an ST advantage, but for this book it mostly isn't—and the first edition is why. The myRIO's ARM was a Cortex-A9 **application processor** running NI's Linux Real-Time: virtual memory, an OS scheduler, a C API over kernel drivers. The H533's Cortex-M33 is a **microcontroller core**: no MMU, no Linux, bare-metal or RTOS. They share a brand name and almost nothing a student touches, so there is no continuity from edition 1 in choosing Arm again.

Nor does the ISA matter much going forward: students write C against peripheral registers and vendor drivers either way. The genuine Arm-ecosystem benefits are generic—skills nominally transfer across Cortex-M vendors, and third-party tools support Arm first (which is why ST's native VS Code debugging exists and why ST reached native Apple Silicon first). Real but modest, and already reflected in the toolchain rows.

---

## Requirements comparison

| Requirement | TI LaunchPad F28P55X | ST Nucleo-H533RE |
|---|---|---|
| MCU, launch | F28P550SJ, **2024** | STM32H533RE, **2024** |
| Core / clock | 150 MHz C28x + CLA, FPU, TMU, NPU | 250 MHz Cortex-M33F (375 DMIPS) |
| Math acceleration | **TMU: sin/cos/atan2/sqrt/divide as instructions, few cycles** | FPU + DSP only—**no CORDIC, no FMAC on this part** |
| Bipolar AI (≥1) | Resistor network (+on-chip PGA buffer available) | Resistor network (**no on-chip op-amp**) |
| Bipolar AO (≥1) | External op-amp stage | External op-amp stage |
| On-chip ADC | 5× 12-bit, 3.9 MSPS, 39 ch | 2× 12-bit, 5 MSPS |
| On-chip DAC (true, buffered) | 1× 12-bit (DACA_OUT on A0, at header) | 1× 12-bit, **2 channels** (PA4/PA5) |
| Encoder decode + HW index | 3× eQEP, dedicated peripherals | **6 timers**, 5 with index pin on LQFP64; 2 are 32-bit |
| Encoder error flags | Phase error, position-count check, direction | `TERRF`/`IDXF`/`IERRF`/`DIRF` (comparable) |
| Encoder **software** | **SysConfig GUI for every feature; 2 velocity examples w/ spreadsheets; Academy lab for this board** | **No encoder example in all of CubeH5; no velocity app note; index not in CubeMX** |
| Encoder 1/T velocity | In-peripheral capture unit, 1–2048 prescaler, atomic latch | 2-timer composition via encoder-clock TRGO; /1–/8 |
| Encoder stall watchdog | **Yes (QWDOG)** | No equivalent |
| Max count rate (512-line) | ~4.4M RPM raw; ~3,445 RPM at max filter | ~7.3M RPM raw; ~14,300 RPM at max filter |
| PWM | **24 ePWM (12 high-res, 150 ps)** | Advanced MC timer + GP timers; **no HRTIM on any H5** |
| RT determinism | **CLA co-processor for jitter-free loops** | CPU-hosted loop only |
| Serial / display | 3 SCI + SPI/I2C/CAN | 3 USART + UARTs, SPI/I2C/FDCAN |
| Debug on board | XDS110; isolator present but **defeated by default jumpers**; controlCARD **inherently isolated** | ST-LINK/V3EC, **not isolated, not detachable** |
| Form factor | BoosterPack ×2; **controlCARD edge card that drops into our own carrier** | Arduino Uno V3 + morpho; **no card-edge module exists** |
| macOS on Apple Silicon | x86 only; Rosetta 2 required; native promised by end of 2026 | **Native arm64** since Feb 2026 |
| Board price (Aug 2026) | ~$35–36 LaunchPad; **~$79 controlCARD (TI direct)** | ~$23–26 |
| Replaceable-unit cost | ~$35 (LaunchPad) or **~$79 (P55x controlCARD)** | ~$26 |
| Lifecycle | 2024; TI precedent → late 2030s | 2024; rolling 10-yr floor (01/2036) |

---

## Development environment—near-parity, with one live exception

Both vendors offer free, modern, cross-platform toolchains: ST with STM32CubeIDE plus a mature VS Code extension, TI with the Theia-based CCS, which has VS Code's look and extension model, one-cable flash/debug via the on-board XDS110, and real-time variable watch/graphing that is particularly good in control labs. Either is a dramatic improvement over the first edition's archived myRIO C toolchain. On features, a wash.

**One axis is not equivalent: Apple Silicon.** A substantial share of mechanical-engineering students arrive with Apple Silicon Macs, and Apple is retiring the compatibility layer TI's tools depend on. macOS 26 retains full Rosetta 2; macOS 27 (Fall 2026) runs only on M1-or-newer and *removes* Rosetta 2 during installation, though it can be reinstalled manually; macOS 28 (Fall 2027) is the hard stop.

**TI as of August 2026: not yet native, but committed with a live deadline.** CCS 21.0.0 (June 15, 2026) ships a single macOS installer, `CCS_21.0.0.00014_mac_x86.dmg`. The 20.4.0 release notes state the Theia IDE component is compiled for Arm but "the backend components are still x86 based, so Rosetta is still required." In an E2E thread opened in **July 2026**, a TI engineer answered: "Yes. Work is already in progress for this. A version will be available before the end of the year, if not sooner." That deadline is still open.

**ST: already native** since the end of February 2026.

**Assessment.** A real ST advantage today, but the residual risk is schedule slip rather than direction, and TI's own deadline beats Apple's cutoff by about a year. It gets a watch item below.

---

## Form factor and debug isolation

Because the course builds its own carrier board, how the MCU attaches matters. Rev 9 corrected two overstatements here; rev 10 corrects a price that made the controlCARD look far less attractive than it is.

**The economics, corrected.** Earlier revisions priced the controlCARD at $190–260. That is the **F28P65X** card ($200–209 at distributors); the **F28P55X card is ~$79 direct from TI**, or $94–99 through DigiKey, Mouser, and Arrow. And because the carrier board would host the HSEC socket directly, **no docking station is needed**—the controlCARD is the entire MCU purchase, with a dock wanted only for early prototyping before the carrier exists. So the real comparison is roughly **$79 per replaceable MCU module versus ~$26 for a whole Nucleo**, about 3×, not the 7–10× rev 9 asserted. At that spread the controlCARD's advantages—an isolated debug probe that no jumper can defeat, a polarized edge connector rated for many insertion cycles, and a swappable module so a destroyed part doesn't take the carrier with it—look like good value rather than a luxury.

One supply caveat: authorized stock on the P55x card is thin right now (DigiKey 2, Arrow 1, Mouser 29 at last check), so a cohort's worth should be ordered in one go, ideally direct from TI. High quantities listed at independent resellers are not authorized-distributor stock and shouldn't be counted on for a course.

---

**Pinout is a convention, not a guarantee.** SPRUIJ6 says the dock supports controlCARDs conforming to the 180-pin and 120-pin connector *footprint*—footprint, not pinout, and "several," not "all." The pinout map is not a numbered TI literature document; it ships inside C2000Ware.

What *is* stable across cards is the skeleton: power, JTAG on 1–8, reset, ePWM ordinals (PWM1A on 49, PWM6A on 61), and eQEP1 on 100/102/106, which held across five cards spanning about eight years. What is **not** stable is the analog map. Pin 21 is ADC-A4 on the F28379D, ADC-A5 on the F280039C, and ADC-A3/B3/C5 on the F28P550SJ—an ADC input on all three, so it mates, but a different module and channel, which determines whether two signals can be sampled simultaneously. This also resolves an open question from earlier revisions: **the P55x has no DAC-B on pin 11 at all, and the P65x puts DAC-C on pin 14**, which on an F28379D is plain ADC-B1. A carrier routing pin 11 to a DAC monitor gets nothing on either candidate card.

**A physical-length ambiguity to settle before cutting copper.** TI's product page for the TMDSCNCD28P55X says "Standard 180-pin controlCARD HSEC interface," but SPRUJA7 describes it as a "120-pin HSEC8 Edge Card Interface" and its C2000Ware pinout file is named `..._120cCARD_pinout`. The F280049C has exactly the same contradiction, and a TI engineer on E2E confirmed that one is a 120-pin card—so "180-pin controlCARD" appears to be an ecosystem label rather than a finger count. TI's own dock uses two connectors precisely to accommodate both lengths. **Verify against a physical card before laying out the socket.** Practical hedge: design to pins 1–120, mirror TI's two-connector arrangement, and don't hard-assign meaning to analog pins in course material.

TI has also broken this once per generation: DIMM100 → HSEC (requiring the TMDSADAP180TO100 adapter), and the F29H85x has now **abandoned controlCARD entirely** for a "controlSOM" using Samtec board-to-board connectors, with backward compatibility only through a separately purchased adapter. So the carrier should be sold internally as "reduces rework," not "future-proof."

---

**Debug isolation.** The LaunchPad has a USB isolator, but SPRUJC0 is explicit that "by default, both shunts are populated... meaning that the USB is NOT isolated from the XDS110 and F28P55x MCU regions." Pull both JP1 shunts and you get real galvanic isolation, but the board then has no power and must be fed 5 V externally—which the carrier could do for ~$0. TI's LaunchPad page pointedly omits the word "isolated," while the controlCARD page says "Isolated on-board XDS110 USB-to-JTAG debug probe." **Only the controlCARD's isolation is inherent and not defeatable by a jumper.** The Nucleo has none and cannot be separated from its ST-LINK.

**Is the grounding issue real? Mostly not, at these voltages.** 12–24 V is SELV: no shock hazard and no hazardous path to a laptop. The most informative data point is that ST sells the X-NUCLEO-IHM07M1, an **8–48 V** three-phase motor shield designed to stack directly on a non-isolated Nucleo and be debugged over its non-isolated ST-LINK, and UM1943 contains no warning about isolation, ground loops, or PC connection at all. The genuine benefit of isolation here is debug-link robustness and ADC noise immunity against brushed-motor EMI—the symptom is "the debugger lost the target mid-demo," not smoke. Two free mitigations beat any hardware purchase: **specify a floating bench supply** (don't earth the motor return) and **have students run on battery during motor labs**. A two-prong laptop charger's Y-capacitors inject mains-frequency leakage into board ground, which is its own good teaching moment. An earthed bench-scope ground clip is a far more effective ground-loop generator than any USB cable. One purchasing gotcha—common ADuM3160-based USB isolator dongles are full-speed only, and both the XDS110 and ST-LINK/V3EC are high-speed devices.

**ST has no card-edge module, for any STM32 family.** The "SOM-STM32xx" pages on st.com are partner pages for Emcraft—castellated solder-down modules on proprietary pinouts. STMod+ is a 20-pin peripheral-daughterboard connector that carries no MCU. The STM32MP SOMs are Cortex-A Linux parts. MikroElektronika's "MCU CARD for STM32" is the closest concept but is a proprietary 168-pin mezzanine with no debugger and no STM32H5 card. ST's model runs the other direction: the Nucleo is the fixed base and expansion boards stack on top, which is what the X-NUCLEO-IHM16M1 motor driver does—and it includes a quadrature-encoder connector. If the group leans ST, consider terminating the carrier at **ST's 34-pin motor-control connector** (pinout in UM1970) rather than at the morpho headers, which puts a published, family-independent interface at the amplifier boundary. Note the morpho pinout is **not** guaranteed consistent across Nucleo-64 boards either—ST publishes a separate table per board—so neither vendor offers a durable cross-board guarantee.

---

## Would spending more buy anything? (up-market scan)

**TI, one tier up—LaunchPad F28P65X (~$66; controlCARD ~$200–209).** Dual 200 MHz C28x cores, ADCs with a 16-bit mode, two buffered DACs, six eQEP modules, more memory. Note what does *not* change: still exactly **one CLA**, shared across both cores. What the upgrade adds as a co-processor is a second full C28x core—arguably more useful than another CLA, since it runs arbitrary C with full peripheral access. The one loss is the NPU, exclusive to the P55x. Toolchain, driverlib, DCL, and book code carry over with a device-target change, which makes this a natural senior-design upgrade path even if the course standardizes on the P55x.

**TI, bleeding edge—LaunchPad F29H85X (~$80–100, restricted distribution).** TI's C29-core flagship. Wrong platform for a textbook anchor: **PREVIEW** status, restricted distribution, a new SDK rather than mature C2000Ware, no CLA, **no buffered DAC listed**, no transfer of the C28x educational corpus, and it abandons the controlCARD form factor.

**TI, Arm industrial—LP-AM263 (north of $100).** Quad Cortex-R5F with C2000-style control peripherals and industrial Ethernet. Buys compute and connectivity the labs don't need, at the cost of multicore boot complexity that would hurt teaching.

---

**ST, sideways—Nucleo-H563ZI: evaluated in rev 9 and rejected.** This deserves recording because it looked attractive. The H563 *does* have the **CORDIC and FMAC** the H533 lacks, its LQFP144 package gives all six encoder timers a complete A/B/Z set (TIM4_ETR appears at PE0), it has 4× the flash and 2.4× the RAM, and ST's Cube H5 examples actually target it. It is 13 months older silicon (announced March 2023 vs April 2024) but carries the **identical 01/2036 longevity date**, so age costs nothing in guaranteed availability. Nonetheless:

- **Price inverts the comparison.** Current distributor pricing is **$39–44** (low of $38.96; DigiKey around $43), not the ~$27 quoted in earlier revisions. That makes the ST board *more expensive than the $35 TI LaunchPad*.
- **The board undoes the package gain.** Ethernet is populated and RMII occupies **PA1**, the only CH2 pin for TIM5 and one of two for TIM2—so **both 32-bit encoder timers are blocked in the stock configuration**. PA4 is VBUS_SENSE by default, blocking TIM5_ETR and DAC1_OUT1. The conflict-free encoder-with-index options out of the box are TIM3 or TIM4, both 16-bit. Recovering a 32-bit encoder means clearing solder bridges—rework in a teaching lab. The cheaper H533RE has TIM2 and TIM5 available untouched.
- **The accelerators close less than hoped.** CORDIC covers sin, cos, atan2, modulus, sqrt, ln and hyperbolics in q1.31 with DMA—about 14× faster than `math.h`—but it is a **memory-mapped peripheral, not an instruction**. TMU results land in a register with no bus transaction; CORDIC requires a store, a wait, and a load. Net effect is roughly **3–5× more latency per trig call in wall-clock terms despite the faster clock**, widening if data is float rather than fixed-point. CORDIC also has no divide, where TMU has `DIVF32`. FMAC is more capable than expected—ST's AN5305 has a working 3p3z example—but it has **no hardware trigger input** (the chain is timer/ADC → DMA → FMAC), is q1.15 only, and **cannot branch**, so anti-windup, saturation with integrator freeze, feedforward, gain scheduling, and fault response all return to the CPU ISR. **FMAC replaces a multiply-accumulate; the CLA replaces a control loop.**
- **The one gap it would cure, it doesn't.** Cube H5 examples target the H563ZI, but there are still **zero encoder examples anywhere in CubeH5**.

**Decision: keep the Nucleo-H533RE as the ST finalist.**

**ST, the road not taken—STM32G474.** Worth one paragraph because it is ST's closest analogue to a C2000: HRTIM, CORDIC, FMAC, five ADCs, four DACs, on-chip op-amps and comparators, and seven encoder-capable timers with hardware index. It is the only STM32 with HRTIM *and* CORDIC *and* FMAC together, and it is the target of ST's own digital-control app notes (AN5305, AN5788). If the objection to the H533 were "not enough analog and control hardware to teach with," the G474 answers it more completely than the H563ZI does. It is disqualified here on age—2019 silicon, 170 MHz Cortex-M4F, much less memory—for a book with a 10+ year shelf life. But the group should know it exists and why it was set aside.

**ST, up-market—H7 / H7R-S / MP-series.** More money buys clock speed, displays, and eventually Linux—never better control peripherals and never a card-edge form factor. No STM32H5 of any kind has HRTIM. The H7 dual-core parts have neither CORDIC nor FMAC. The MP-series' Linux reintroduces the determinism problem the book left behind with the myRIO.

---

## Recommendation

**Lean: TI F28P55X—narrowly, and on software grounds.**

The encoder-hardware argument that carried earlier revisions does not survive verification. Both parts decode quadrature, reset on index, and flag illegal transitions in hardware; the H533 has more encoder-capable timers than the F28P55x has eQEP modules, and richer index conditioning. Neither performs error correction.

What justifies the lean now:

- **Encoder software and documentation.** TI ships a GUI that configures every eQEP feature including index, capture and watchdog; two complete velocity examples with design spreadsheets that run on jumper wires; and a C2000 Academy lab for this exact board. ST ships no encoder example anywhere in CubeH5, no encoder-velocity application note, and no CubeMX support for index configuration. For a textbook, this is the difference between a lab that can be written in the time available and one that can't.
- **The CLA.** An independent co-processor hosting the control law with hardware-triggered, jitter-free timing has no ST equivalent at any price, and no ST accelerator substitutes for it. It is also the best teaching device in the comparison.
- **The TMU**, which the H533 has nothing to answer with—no CORDIC, no FMAC—and which remains 3–5× faster than the CORDIC even on the parts that have one.
- **PWM depth**, including 150 ps high-resolution mode that no STM32H5 can match.
- **A hardware stall watchdog** and more independent encoder error detectors.
- **A card-edge form factor that drops into our own carrier**, at ~$79 per module with inherently isolated debug and no docking station required. ST has no equivalent at any price.

The case for ST is real but narrower than rev 9 made it. ST is still cheaper—$23–26 versus $35–36 for the entry board, and ~$26 versus ~$79 per replaceable module—but that is roughly a 3× spread on the module, not the 7–10× earlier revisions asserted, and for that premium TI's card buys galvanic isolation and a swappable module that protects the carrier. ST's clearest remaining advantage is the **native Apple Silicon toolchain today** where TI has only a promise, plus a second DAC channel and competitive encoder hardware. Two rev 8 arguments for TI remain withdrawn: the controlCARD pinout guarantee (which does not exist) and the LaunchPad's isolation (defeated by default jumpers, though recoverable for free).

If the group judges the Apple Silicon risk unacceptable, the H533RE is defensible and nothing in the labs would be blocked. Otherwise the lab-development cost is where the two genuinely diverge, and that cost lands on us rather than on the students.

**Suggested configuration if TI is chosen:** standardize the course on the **F28P55X**, with the **controlCARD in an HSEC socket on the custom carrier** as the production configuration and LaunchPads for prototyping and for students who want their own hardware. The F28P65X remains a drop-in upgrade for senior design—same toolchain, same driverlib, same book code—so students can move up without relearning anything.

---

## Remaining open questions for the group

- **Range:** ±5 V or ±3.3 V? Input stage is marginally simpler at ±3.3 V; the output stage is identical either way. Decide from the labs' actual sensor/actuator interfaces.
- **Encoder count:** how many simultaneous encoders does the heaviest lab use? TI has 3 eQEP (6 on the F28P65x); the H533 has six encoder-capable timers, five with an index pin in the 64-pin package. If the controlCARD form factor is adopted, the HSEC standard exposes only two complete eQEP interfaces.
- **Encoder type and cabling:** single-ended or differential (RS-422)? At lab speeds neither MCU is close to its count-rate limit, so signal integrity beside motor PWM is the binding constraint, not bandwidth. Also fix the input-filter setting deliberately—TI's maximum qualifier setting caps at ~3,445 RPM with a 512-line encoder.
- **DAC channel count:** the labs are believed to use one analog output; if a second is firmly required, the H533 has two channels and the F28P55X has one (a second TI channel would come from a ~$3 SPI DAC on the carrier).
- **Form factor:** LaunchPad + headers, controlCARD in a socket on the carrier, or Nucleo-as-shield? At the corrected ~$79 the controlCARD is a much easier case than earlier revisions suggested.
- **Debug isolation:** the controlCARD is inherently isolated. If the LaunchPad is chosen instead, removing both JP1 shunts and supplying 5 V from the carrier gets the same benefit for ~$0 and should probably be the default.
- **WATCH—native arm64 CCS.** TI's public commitment (E2E, July 2026) is a native macOS build "before the end of the year." Checkpoints: **(1)** each CCS release, check whether the macOS download list has gained an arm64 DMG; **(2)** **January 2027**—if TI's own deadline passes with no native build, escalate to a decision input; **(3)** **before press**—if still x86-only, document a Rosetta-reinstall procedure and note the Fall 2027 macOS 28 hard stop.
- **To verify physically before committing to a carrier:** whether the TMDSCNCD28P55X is a 120-finger or 180-finger card (TI's own documentation contradicts itself), and whether the F28P65x's DACA/DACC appear where SPRR478 suggests.
- **Procurement:** authorized stock on the P55x controlCARD is currently thin (single-digit to low-tens at DigiKey, Arrow, and Mouser). Buy a cohort's worth in one order, preferably direct from TI.
- **To verify if ST is chosen:** which `ITRx` index carries each timer's TRGO on the H533 (needed for the 1/T topology), and whether capture-on-TRC remains available while `SMS` is set to an encoder mode.

---

## Sources

**TI—device and boards**
- [TMS320F28P550SJ product page](https://www.ti.com/product/TMS320F28P550SJ) · [TMS320F28P55x datasheet](https://www.ti.com/lit/ds/symlink/tms320f28p550sj.pdf) · [LAUNCHXL-F28P55X](https://www.ti.com/tool/LAUNCHXL-F28P55X) · [user guide SPRUJC0](https://www.ti.com/lit/ug/sprujc0a/sprujc0a.pdf) (§2.1.3.8 PWM-DAC; JP1 isolation shunts populated by default) · [Pinout map SPAZ056](https://www.ti.com/lit/pdf/spaz056) · [DigiKey—LAUNCHXL-F28P55X](https://www.digikey.com/en/products/detail/texas-instruments/LAUNCHXL-F28P55X/22674628)
- [TMDSCNCD28P55X](https://www.ti.com/tool/TMDSCNCD28P55X) ("Standard 180-pin controlCARD HSEC interface"; "Isolated on-board XDS110") · [Octopart—TMDSCNCD28P55X price/stock](https://octopart.com/part/texas-instruments/TMDSCNCD28P55X) · [TMDSCNCD28P65X](https://www.ti.com/tool/TMDSCNCD28P65X) · [Octopart—TMDSCNCD28P65X price/stock](https://octopart.com/part/texas-instruments/TMDSCNCD28P65X) · [180-pin docking station guide SPRUIJ6](https://www.ti.com/lit/pdf/spruij6) · [controlCARD schematic SPRR478](https://www.ti.com/lit/pdf/sprr478)
- [C2000Ware SDK](https://www.ti.com/tool/C2000WARE) · [Digital Control Library SPRUID3](https://www.ti.com/lit/ug/spruid3/spruid3.pdf) · [eQEP driverlib API](https://software-dl.ti.com/C2000/docs/C2000_driverlib_api_guide/f28p55x/build/html/modules/eqep.html) · [c2000ware-core-sdk (SysConfig eqep.js, examples/eqep)](https://github.com/TexasInstruments/c2000ware-core-sdk) · [c2000ware-c2000-academy (F28P55x eQEP lab)](https://github.com/TexasInstruments/c2000ware-c2000-academy)
- [eQEP module reference guide SPRUFK8](https://engineering.purdue.edu/~dionysis/EE452/Lab10/eQEP_User_Guide_sprufk8.pdf) · [SPRABX2—CW/CCW support and QMA error detection](https://www.ti.com/lit/sprabx2) · [SPRU514—TMU intrinsics](https://downloads.ti.com/docs/esd/SPRU514/trigonometric-math-unit-tmu-intrinsics-t365164-6.html) · [SPRY288—C2000 computational performance](https://www.ti.com/lit/pdf/spry288)
- HSEC standard map—parsed from C2000Ware `boards/ExperimenterKits/DockingStation_HSEC_120or180pin/revF/180_HSEC8_DV_pinout_Rev_F.pdf`; full 180-pin table in project files as `HSEC180_controlCARD_standard_pinout.csv`
- [F29H850TU](https://www.ti.com/product/F29H850TU) · [LAUNCHXL-F29H85X](https://www.ti.com/tool/LAUNCHXL-F29H85X) · [LP-AM263](https://www.ti.com/tool/LP-AM263)

**ST—device and board**
- [STM32H533RE product page](https://www.st.com/en/microcontrollers-microprocessors/stm32h533re.html) · [NUCLEO-H533RE](https://www.st.com/en/evaluation-tools/nucleo-h533re.html) · [DS14539—H523/H533 datasheet](https://www.st.com/resource/en/datasheet/stm32h533re.pdf) (analog feature list: no op-amp, no comparator) · [UM3121—Nucleo-64 MB1814](https://www.st.com/resource/en/user_manual/um3121-stm32h5-nucleo64-board-mb1814-stmicroelectronics.pdf) · [Octopart—NUCLEO-H533RE price/stock](https://octopart.com/part/stmicroelectronics/NUCLEO-H533RE)
- [STM32H563ZI](https://www.st.com/en/microcontrollers-microprocessors/stm32h563zi.html) · [NUCLEO-H563ZI](https://www.st.com/en/evaluation-tools/nucleo-h563zi.html) · [DS14258—H562/H563 datasheet](https://www.st.com/resource/en/datasheet/stm32h563ri.pdf) · [UM3115—Nucleo-144 MB1404](https://www.st.com/resource/en/user_manual/um3115-stm32h5-nucleo144-board-mb1404-stmicroelectronics.pdf) (Ethernet on PA1; PA4 as VBUS_SENSE) · [Octopart—NUCLEO-H563ZI price/stock](https://octopart.com/part/stmicroelectronics/NUCLEO-H563ZI)
- [ST product longevity program](https://www.st.com/content/st_com/en/about/quality-and-reliability/product-longevity.html) · [ST press release 7 Mar 2023 (H563)](https://newsroom.st.com/media-center/press-item.html/p4519.html) · [ST blog 3 Apr 2024 (H533)](https://blog.st.com/stm32h5/)

**ST—encoder and accelerators (the rev 8/9 corrections)**
- [STM32H533 TIM2 ECR register: IE, IDIR, IBLK, FIDX, IPOS](https://docs.rs/stm32h5/latest/stm32h5/stm32h533/tim2/ecr/index.html)—SVD-derived; primary evidence for hardware index reset.
- [STM32H5 TIM status flags: IDXF, DIRF, IERRF, TERRF](https://docs.rs/stm32h5/latest/stm32h5/stm32h503/tim2/sr/index.html)—hardware transition- and index-error detection.
- [stm32h533xx.h](https://raw.githubusercontent.com/STMicroelectronics/cmsis_device_h5/main/Include/stm32h533xx.h) and [stm32h563xx.h](https://raw.githubusercontent.com/STMicroelectronics/cmsis_device_h5/main/Include/stm32h563xx.h)—six encoder timers on both; TIM2/TIM5 32-bit; no OPAMP/COMP on either; CORDIC/FMAC on H563 only.
- [STM32H533RETx pin data](https://raw.githubusercontent.com/STMicroelectronics/STM32_open_pin_data/master/mcu/STM32H533RETx.xml) and [STM32H563ZITx](https://raw.githubusercontent.com/STMicroelectronics/STM32_open_pin_data/master/mcu/STM32H563ZITx.xml)—TIM4 has no ETR pin in LQFP64.
- [stm32h5xx_hal_driver](https://github.com/STMicroelectronics/stm32h5xx_hal_driver)—`HAL_TIMEx_ConfigEncoderIndex` routes index via ETR; `HAL_TIM_Encoder_Start_IT` enables only CC1/CC2; four encoder callbacks exist.
- [STM32CubeH5 firmware](https://github.com/STMicroelectronics/STM32CubeH5)—**zero files with "encoder" in the path**; NUCLEO-H533RE has no TIM examples.
- [AN4013—cross-series timer overview](https://www.st.com/resource/en/application_note/an4013-stm32-crossseries-timer-overview-stmicroelectronics.pdf) (§3.2 encoder-clock TRGO) · [AN4776—timer cookbook](https://www.st.com/resource/en/application_note/an4776-generalpurpose-timer-cookbook-for-stm32-microcontrollers-stmicroelectronics.pdf) (explicitly excludes motor control) · [AN5325—CORDIC](https://www.st.com/resource/en/application_note/an5325-getting-started-with-the-cordic-coprocessor-stmicroelectronics.pdf) · [AN5305—FMAC digital filters, 3p3z example](https://www.st.com/resource/en/application_note/an5305-digital-filter-implementation-with-the-fmac-using-stm32cubeg4-mcu-package-stmicroelectronics.pdf) · [AN5464—MCSDK position control](https://www.st.com/resource/en/application_note/an5464-position-control-of-a-threephase-permanent-magnet-motor-using-xcubemcsdk-or-xcubemcsdkful-stmicroelectronics.pdf)
- [ST community—encoder-clock TRGO to capture chain](https://community.st.com/t5/stm32-mcus-embedded-software/tim-encoder-mode-with-encoder-clock-output/td-p/790944) · [ST community—TIM2 encoder + index ETR pin conflict](https://community.st.com/t5/stm32-mcus-embedded-software/stm32g474ve-tim2-encoder-index/td-p/726269) · [ST community—index in encoder mode, **older families only**](https://community.st.com/t5/stm32-mcus-products/how-to-use-index-track-in-quadrature-encoder-mode/td-p/507598)—**the source of the rev 1–7 error**, retained as a caution.

**ST—form factor and motor boards**
- [X-NUCLEO-IHM16M1](https://www.st.com/en/evaluation-tools/x-nucleo-ihm16m1.html) (carrier-as-shield with encoder connector) · [X-NUCLEO-IHM07M1 / UM1943](https://www.st.com/en/evaluation-tools/x-nucleo-ihm07m1.html) (8–48 V shield on non-isolated Nucleo, no isolation warning) · [UM1970—34-pin motor-control connector](https://www.st.com/resource/en/user_manual/um1970-getting-started-with-the-xnucleoihm09m1-motor-control-connector-expansion-board-for-stm32-nucleo-stmicroelectronics.pdf) · [TN1238—STMod+ specification](https://www.st.com/resource/en/technical_note/tn1238-stmod-interface-specification-stmicroelectronics.pdf) · [B-STLINK-ISOL](https://www.st.com/en/development-tools/b-stlink-isol.html) · [MikroE MCU CARD for STM32](https://www.st.com/en/partner-products-and-services/mcu-card-for-stm32.html)

**Toolchain / Apple Silicon**
- [TI E2E—native arm64 CCS commitment, July 2026](https://e2e.ti.com/support/processors-group/processors/f/processors-forum/1655979/ccstudio-is-ccs-going-to-run-on-apple-silicon-without-depending-on-rosetta) · [CCS 20.4.0 release notes](https://software-dl.ti.com/ccs/esd/CCSv20/CCS_20_4_0/exports/CCS_20.4.0_ReleaseNote.htm) · [CCS 21.0.0 download page](https://www.ti.com/tool/download/CCSTUDIO/21.0.0) · [ST community—ST tools on macOS](https://community.st.com/stm32cubeide-for-visual-studio-code-mcus-133/the-future-of-st-tools-on-macos-in-2027-forward-deprecation-of-rosetta-2-163538) · [AppleInsider—macOS Intel-app timeline](https://appleinsider.com/articles/26/06/12/how-and-when-macos-will-finally-stop-support-for-intel-apps)

**Method note.** Rev 8–10 corrections were verified against vendor datasheets, SVD-derived register definitions, CMSIS device headers, HAL sources, SDK file trees, ST's pin database, and distributor pricing—not against community posts. The rev 1–7 encoder error came from trusting a forum thread about a different silicon generation; the rev 1–9 price error came from applying one card's price to a different card. Where a claim could not be verified from a primary source, it appears in the open questions rather than in the body.