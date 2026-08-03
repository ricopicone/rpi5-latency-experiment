---
project_note_id: 1018
title: 'TI vs ST: Next-Generation Platform Finalists'
---

# TI vs ST: Next-Generation Platform Finalists

**Real-Time Computing for Mechanical Engineers, 2nd Edition**
Board comparison—internal—August 2026 (rev 8)

> **Revision note.** Rev 8 corrects a material error. Through rev 7 this document claimed the STM32H533's timer encoder mode lacked hardware index handling and lacked quadrature-error detection, and that 1/T velocity required re-plumbing signals into another timer. All three claims were wrong for this part—they were true of older STM32 families (F4/F7/H7) and were carried forward from a 2019-era community post instead of being checked against the H533's own registers. The encoder section, the requirements table, and the recommendation have been rewritten accordingly, and the lean is now narrower. A new section on the ST form factor has been added.

---

## Context and bottom line

The first edition's platform, the NI myRIO-1900, is discontinued, and a replacement is needed for the second edition. An earlier memo made the case for the TI LaunchPad F28P55X; the group subsequently asked that an ST evaluation board be considered head-to-head, and added one new requirement: at least one bipolar analog input and one bipolar analog output (±5 V is sufficient; ±3.3 V acceptable—the myRIO provided ±10 V on its MSP connector, but the labs do not need the full range). This document is that comparison.

Two finalists: the TI **LaunchPad F28P55X** (TMS320F28P550SJ) and the ST **Nucleo-H533RE** (STM32H533RE)—both 2024 silicon. ST's classic mixed-signal control board, the Nucleo-G474RE, was considered and set aside: 2019 silicon already seven years into its life, and its headline advantage—six on-chip op-amps—turns out not to matter for the bipolar front-end (see below). It is not discussed further.

At the target ranges the analog front-end is **essentially identical for either board**—a handful of resistors on the input side and one external op-amp stage on the output side—so on-chip analog does not differentiate the boards. Encoder hardware, which earlier revisions treated as the decisive factor, turns out to be close to a wash once the H533's actual register set is checked. What remains for TI is the CLA as a jitter-free host for the control loop, PWM depth, the TMU, and a motor-control curriculum written for this exact silicon. What remains for ST is a faster scalar core, a second DAC channel, a lower price, a native Apple Silicon toolchain, and far cheaper replaceable hardware. **The lean is still TI, but it is now a lean on pedagogy and ecosystem rather than on peripherals, and a reasonable group could go the other way.**

---

## Lifecycle: reading the dates correctly

**ST publishes a date, but it's a rolling floor, not a launch-anchored runway.** ST's listed "available until" dates are a **10-year commitment restarting from January 1 of the current year** (every active STM32 currently shows January 2036 regardless of launch year). ST has historically kept renewing these dates annually while a part stays in the longevity program, but the *guarantee* at any moment is only ~10 years ahead, and ST's policy allows ending renewals if volumes fall—which is why design freshness matters: the H533 launched in **2024**.

**TI publishes no per-part date but has the stronger track record.** The F28P55x also launched in 2024; TI's C2000 parts routinely stay in production 15–20 years (the 2007-era F2833x and 2016-era F28379D are both still ACTIVE). Expectation: production into the late 2030s.

**Conclusion:** with both finalists on 2024 silicon, expected longevity is comparable and excellent; TI's assurance is precedent-based, ST's is contractual-but-rolling.

---

## The finalists in brief

The **TI LaunchPad F28P55X** (~$35–36, in stock at DigiKey and Mouser) carries the TMS320F28P550SJ: a 150 MHz C28x real-time core plus an FPU, a small NPU, and two control-specific accelerators worth defining. The **CLA (Control Law Accelerator)** is an independent 150 MHz floating-point co-processor that runs the fast inner control loop in parallel with the main core—triggered directly by peripheral events (ADC end-of-conversion, PWM period) with direct register access and no interrupt arbitration, so the loop executes with essentially zero jitter regardless of what the main CPU is doing. The **TMU (Trigonometric Math Unit)** extends the FPU with hardware sin/cos/atan2/sqrt/divide instructions that complete in a few cycles—the exact operations field-oriented motor control evaluates every loop iteration, which software trig on a generic core spends tens to hundreds of cycles on. Around these: five 12-bit ADCs (3.9 MSPS), one buffered 12-bit DAC, three dedicated eQEP encoder modules (two connectors pre-wired on the board), 24 ePWM channels, and the free Code Composer Studio toolchain.

The **ST Nucleo-H533RE** (~$23–26, in distribution) carries the STM32H533RE: a 250 MHz Cortex-M33F—375 DMIPS, notably quicker in scalar terms than the C28x. It covers every book requirement: two 12-bit ADCs at up to 5 MSPS, a **two-channel** buffered 12-bit DAC (one more analog output than the TI part provides), quadrature-encoder mode with hardware index on **six** timers, an advanced motor-control timer for PWM, three USARTs plus UARTs, plenty of GPIO, and one on-chip op-amp and comparator.

---

## The bipolar front-end at ±5 V / ±3.3 V: a wash

Both MCUs have strictly unipolar 0–3.3 V ADCs and DACs, so bipolar I/O requires external signal conditioning on either board—and that conditioning is board-agnostic, because the one amplifier that must exist (the bipolar output stage) is external on both chips: a 3.3 V-railed on-chip op-amp can never swing below ground, at ±10 V or ±5 V alike. This is also why targeting ±5 V rather than replicating the myRIO's ±10 V is worth it, even though it doesn't affect the board choice: it makes the front-end itself substantially cheaper and safer. First, the power problem collapses—driving ±10 V would require op-amp rails of roughly ±12 V, which on a USB-powered station means a boost converter plus an inverter, a real power-supply design; ±5 V outputs run from a ~$2 charge pump hanging off the 5 V rail the board already has. Second, protection gets easier—with no ±12 V rails on the bench, worst-case fault voltages and clamp energies at the student-facing jacks drop, and the protection-component sizing relaxes accordingly. Third, at ±3.3 V (though not at ±5 V) the input network degenerates to two equal resistors. Finally, at these modest ranges, on-chip op-amps (a headline feature of some ST parts) contribute at most a free input buffer—which *both* chips provide anyway: ST via its general-purpose op-amp, TI via its **PGAs (Programmable Gain Amplifiers)**—on-chip amplifiers sitting in front of the ADC whose gain is selected in software from fixed internal steps (unity included) rather than set by external resistors. A PGA is less flexible than a true op-amp—its feedback network is internal, so it can't be wired into arbitrary topologies—but as an ADC input buffer or gain stage it does the same job with zero external parts, which is the only role either chip's on-chip analog plays in this design. (The H533's single op-amp also offers a PGA mode with the same software-selectable gain idea.) Concretely, per channel:

---

**Input (bipolar signal → 0–3.3 V ADC).** At **±3.3 V**, a two-resistor network does it: equal resistors from the signal and from the 3.3 V rail produce (Vin + 3.3)/2, mapping ±3.3 V onto 0–3.3 V. At **±5 V**, it's a three-resistor network with the same idea. Add an RC for anti-aliasing. No active parts are strictly required if the source impedance is modest; if buffering is wanted, **both** chips provide it on-chip—ST via an op-amp in follower mode, TI via its PGAs (which support unity gain) feeding the ADC. So the input stage is: a few precision resistors, either board.

**Output (0–3.3 V DAC → bipolar).** This is where an external part is unavoidable on *every* candidate: swinging below ground requires an op-amp powered from a negative rail, full stop. The canonical stage is one op-amp wired as a level-shift/gain block (gain ≈ 2–3, referenced to 1.65 V), powered from ±5 V. The negative rail comes from either a <$1 charge-pump inverter (e.g., ICL7660A) off the board's 5 V USB rail, or the bench supply that's already on every lab station. Identical BOM for TI and ST.

**Net per-station front-end:** roughly one quad op-amp, one charge pump, and a half-dozen precision resistors—a ~$6 BOM, the same design either way, integrated into the course's custom interface board. A complete worked design—schematic, transfer functions, overvoltage/fault analysis, and BOM—exists as a companion document (*Bipolar Analog Front-End—Design Spec*), so total kit cost is already known. The takeaway for the board decision: ST's on-chip analog, while real, buys almost nothing *for this specific job*, because the only stage that truly needs an amplifier (the bipolar output) must be external on both platforms. ±3.3 V is marginally simpler than ±5 V on the input side only; the output stage is the same effort at either range, so the range should be chosen from the labs' actual sensor/actuator interfaces.

And pedagogically, this little circuit is a gift: students build or at least analyze the exact signal-conditioning chain that every real data-acquisition front-end contains.

---

## DAC architecture: both are true DACs—no PWM smoothing required

One requirement worth verifying explicitly is that neither board's "DAC" is secretly a filtered PWM output—and TI's documentation genuinely invites that worry. The F28P55X LaunchPad user guide (§2.1.3.8) describes "PWM DAC signals" on four BoosterPack pins, produced by low-pass-filtering a PWM output, with the RC filters **not populated** by default. A reader skimming the user guide could reasonably conclude that the board's analog output works this way. It does not: that section describes an *optional, auxiliary* facility (a C2000 debugging tradition for watching extra internal variables on a scope), **not** the device's actual DAC.

The real story on both finalists:

- **F28P55X**—the TMS320F28P550SJ has one true buffered 12-bit DAC (a resistor-string converter with an on-chip output buffer, "DACA"). The datasheet pin table places DACA_OUT on analog pin **A0**—physical pin 23 of the 100-pin PZ package used on the LaunchPad—and the board's pinout map (SPAZ056) confirms DACA_OUT is routed to the BoosterPack headers. No filtering, no PWM anywhere in the chain. (One nuance for the custom-board schematic: A0 is shared between DACA_OUT and ADC input A0, so that channel is one or the other.)
- **H533RE**—the STM32H533 has one true 12-bit DAC peripheral with **two output channels**, each a resistor-string converter with a switchable on-chip buffer, on package pins PA4/PA5, exposed on the Nucleo headers. Same conclusion: real DAC, no smoothing.

Bottom line: the "DAC = filtered PWM" pattern belongs to older/cheaper parts (and to hobby boards like the older Arduinos); both finalists provide true converter DACs, and the front-end design consumes them directly.

---

## Encoder interfaces: much closer than earlier revisions claimed

This section was wrong through rev 7 and has been rebuilt from ST's own register definitions, CMSIS device headers, HAL sources, and pin database for the STM32H533RE. The short version: **both parts decode quadrature in hardware, both reset the counter on the index pulse in hardware, both flag illegal quadrature transitions in hardware, and both can measure velocity by either the T or the 1/T method without external wiring.** The differences that remain are real but narrow, and they run in both directions.

**Index (Z) handling—parity, and arguably ST ahead.** The eQEP has dedicated index hardware that latches the position counter on the index edge, resets it every revolution, or both. The H533's timers have an encoder control register (`TIM_ECR`) that does the same thing: `IE` enables counter reset on the index event, and `IDIR`, `IBLK`, `FIDX`, and `IPOS` qualify it by direction, blank it against a third input, take only the first index, and select which A/B state the reset lands on. ST additionally raises an `IERRF` index-error flag when the index arrives inconsistent with the expected A/B state, which the eQEP has no equivalent for. The claim in earlier revisions that Z requires an EXTI interrupt was true of the F4/F7/H7 generation and is false here.

---

**Integrity—parity.** The eQEP raises a phase-error flag on illegal quadrature transitions and can interrupt on direction change. The H533 raises `TERRF` (transition error) and `DIRF` (direction change) for exactly these cases, plus `IDXF` and `IERRF` on the index path. The earlier statement that the STM32 "silently miscounts on an illegal transition" was simply incorrect.

**Velocity estimation—the one place TI still leads, and by less than claimed.** Measuring speed from an encoder has two classical methods: count edges per fixed time (T method—good at high speed, quantization-noisy at low speed) and time the interval between edges (1/T method—excellent at low speed). The eQEP contains both in one peripheral: a unit timer for T, and an edge-capture unit with its own capture timer and a 1-to-2048 edge-stride prescaler for 1/T, with position, capture period, and capture timer all latched **atomically** on the unit time-out event.

The H533 can do 1/T too, entirely on-chip, but as a two-timer composition rather than one peripheral: the encoder timer emits an **encoder clock** on its trigger output (`CR2.MMS = 1000`, documented in ST's AN4013 and available on all six encoder-capable timers), that routes internally to a second timer's `ITRx` input, and the second timer captures on `TRC`—so each captured value is a timestamp of an encoder edge. Note carefully why the obvious single-timer approach does *not* work: in encoder mode the timer's counter **is** the position, so capturing on its own channels latches position, not time. There is no second, time-based counter inside the encoder timer.

So the honest comparison on velocity is: one peripheral versus two, an edge-stride prescaler of 1–2048 versus the STM32 input-capture prescaler's /1–/8, and eQEP's atomic co-latching of position with period versus a composition where those come from different timers. Also worth stating plainly for a textbook: TI documents this thoroughly and ST does not—**there is no ST application note on encoder velocity estimation** (AN4776, the general-purpose timer cookbook, explicitly excludes motor-control use cases; AN4013 describes encoder modes but no velocity method). A student following vendor documentation reaches a working 1/T implementation on the C2000 and has to invent one on the STM32. For a book that teaches this material, that documentation gap may matter more than the hardware difference.

---

**Stall detection—TI only.** The eQEP has a watchdog (`QWDOG`) that flags a stalled encoder in hardware. The H533 timer has nothing dedicated; you get a stall timeout for free as the overflow of the second timer *if* you have built the 1/T chain, but that is a consequence of the construction, not a peripheral feature.

**Resource accounting—reframed.** Earlier revisions said the H533 "handles 2 encoders on general-purpose timers," which came from misreading a datasheet bullet about how many timers are 32-bit. Corrected: **six** timers support quadrature encoder mode with hardware index (TIM1, TIM2, TIM3, TIM4, TIM5, TIM8), of which only TIM2 and TIM5 are 32-bit and the rest are 16-bit; on the LQFP64 package used by the Nucleo-H533RE, TIM4 has no index pin, so five have a complete A/B/Z. LPTIM1 and LPTIM2 add a simpler 16-bit index-less encoder mode. The TI framing still holds in one respect—eQEP modules are dedicated peripherals that don't consume timers you'd want for PWM or capture—but the H533 has more encoder-capable timers than the F28P55x has eQEP modules, not fewer.

**What ST does that eQEP does not.** Worth recording so the comparison is symmetric: nine encoder decode modes (including clock-plus-direction and directional-clock variants, not just quadrature x1/x2/x4); the index conditioning features above; the `IERRF` index-consistency check; preloadable slave-mode selection for glitch-free mode switching; encoder counting in low-power Stop mode via LPTIM; and native DMA on encoder events, where the eQEP is not a DMA trigger source.

**Net:** the encoder argument, which rev 7 called "the single strongest technical argument for the TI board," is roughly a wash on capability. TI retains a genuine edge on single-peripheral 1/T with a deep prescaler, atomic latching, a hardware stall watchdog, documentation, and two pre-wired encoder connectors on the LaunchPad. That is worth something in a course built around encoders—but it is not the decisive advantage this document previously asserted.

---

## Control-loop timing: CPU timer ISR vs. CLA task

The first edition implemented its control loops as timed loops—a timer interrupt with an ISR executing a difference equation. That model transfers to either finalist unchanged, and it is worth being precise about what the C2000's CLA adds, because the difference is not *how you write the loop* but *how exact its period is*.

**The CPU timer ISR (the familiar structure).** A CPU timer with period T fires a PIE interrupt; the CPU saves context, runs the difference equation, restores context. C28x interrupt latency is tens of cycles—sub-microsecond at 150 MHz—so this is entirely viable, and it is the right first implementation to teach. Its limitation is contention: the ISR shares a CPU with UART logging, keypad polling, and every other interrupt source. Higher-priority ISRs delay it, equal-priority ones queue behind it, and context save/restore varies with what was executing. The result is jitter in T. For a difference equation that matters concretely: the sampling period the z-domain coefficients assume drifts from the period actually realized, so the closed-loop response deviates from the designed transfer function. The deviation is usually small and tolerable—but it grows precisely as the CPU is loaded with the I/O that other chapters of the book teach.

**The CLA task (the same skeleton, without the contention).** The CLA has eight tasks, each with a hardware-selectable trigger. A task is non-preemptive and runs to completion, launched by its trigger with a fixed few-cycle latency. Nothing else executes on the CLA, and the trigger reaches it directly from the peripheral rather than through the PIE. Persistent state—past inputs and outputs—lives in CLA data RAM between invocations, and TI's Digital Control Library ships its compensators in exactly this form.

---

**Two ways to build the timed loop on the CLA.** The direct translation keeps the existing mental model: configure a CPU timer with period T and select it as the trigger for CLA Task 1. Timer fires, task launches, difference equation executes, task ends. Structurally identical to the historical timed loop, with the CPU uninvolved.

The canonical control pattern is better, and is the one worth teaching: let an ePWM time base define T, have it fire an ADC start-of-conversion at the same phase every period, and trigger the CLA task from the ADC's end-of-conversion. Now the *sampling instant* is hardware-exact, computation begins the moment the sample exists, and the sample-to-actuation delay is constant. The period the coefficients assume is the period realized, and residual jitter is at clock level rather than ISR level.

**The discipline it imposes**—worth a paragraph in the book—is that task execution time must remain below T. An overrun produces a late or missed tick, which is the classic real-time scheduling failure made concrete and measurable.

**A pedagogical sequence this suggests:** implement the loop first as a CPU timer ISR (familiar, and matches the first edition), instrument it with a GPIO toggle, and measure the jitter on a scope while loading the CPU with UART traffic. Then move the same difference equation to a CLA task and watch the jitter collapse. Determinism stops being an assertion the book makes and becomes something students observe.

**On the ST side**, the equivalent is a timer ISR on the Cortex-M33, with the same contention characteristics as the C28x timer ISR—NVIC latency is comparable and the peripheral-triggered path (timer → ADC → DMA) can remove the CPU from the data path, but there is no independent co-processor to host the control law. The CPU-loop story is a wash; the CLA option exists only on the TI board. **With the encoder argument reduced to near-parity, this is now the strongest hardware differentiator in the comparison.**

---

## C library and driver support—verified

Both vendors ship free, comprehensive C support, so neither board is risky here—but they are not identical, and for this book's purposes TI's package is the better fit.

**TI: C2000Ware.** One SDK containing the peripheral driver library (driverlib), bit-field register headers, SysConfig code generation, and—explicitly confirmed for the F28P551x—device examples, FreeRTOS demos, and the math libraries. Driverlib's style is thin, readable, one-function-per-operation wrappers over registers (`EQEP_setPosition(...)`), which sits at exactly the abstraction level a real-time computing course wants: students can read the driver source and see the register write. Two inclusions matter specifically for us: the **Digital Control Library (DCL)**—PID/PI/2p2z/3p3z compensators hand-optimized for the C28x and CLA, with anti-windup and saturation handling, i.e., the exact algorithms the book teaches, shipped as documented reference code—and **IQmath**, the classic fixed-point library, if the book keeps its fixed-point-arithmetic material. The C2000 Academy curriculum teaches against driverlib, so course materials and vendor materials align.

---

**ST: STM32CubeH5.** The HAL (high-level, portable, verbose) plus the LL (low-layer, register-close—the better teaching layer, comparable in spirit to driverlib), CMSIS core and the well-documented CMSIS-DSP library (FIR/IIR/matrix/PID), BSPs, and middleware. Three caveats. First, the Cube H5 example projects target the **NUCLEO-H563ZI, NUCLEO-H503RB, and H573I-DK—not the NUCLEO-H533RE**; the HAL/LL drivers fully cover the H533 silicon and CubeMX generates correct init code for it, but students wouldn't get a folder of ready-made examples for their exact board. Second, ST's packaged RTOS middleware for H5 is the ThreadX family (Azure RTOS), not FreeRTOS—FreeRTOS runs fine on Cortex-M33 via its own port, but it arrives from outside the vendor package, whereas C2000Ware ships FreeRTOS demos for the F28P55x directly. Third, and newly identified in rev 8: **ST publishes no application note on encoder velocity estimation**, so the 1/T material a mechatronics course wants to teach has no vendor reference implementation on the ST side.

**Verdict:** both ecosystems clear the bar comfortably; the difference is alignment. TI provides examples for this exact board, a control-law library that mirrors the book's syllabus, documented encoder velocity methods, and the book's preferred RTOS in-box. ST provides a broader, more portable stack with several small frictions for our specific board, RTOS, and topic list. This gap is now doing more of the work in the recommendation than the silicon is.

---

## Does Arm matter for us?

It's tempting to count "Arm" as an ST advantage, but for this book it mostly isn't—and the first edition is the reason why. The myRIO's ARM was a Cortex-A9 **application processor** (Armv7-A) running NI's Linux Real-Time: virtual memory, an OS scheduler, NI's C API over kernel drivers. The H533's Cortex-M33 is a **microcontroller core** (Armv8-M): no MMU, no Linux, bare-metal or RTOS. These share a brand name and almost nothing else that a student touches—so there is no meaningful continuity from edition 1 in choosing Arm again.

Nor does the ISA matter much going forward: students write C against peripheral registers and vendor drivers on either chip, and the instruction set is invisible outside the debugger's disassembly window. The genuine Arm-ecosystem benefits are generic ones—skills nominally transfer across the many Cortex-M vendors, and third-party tools/RTOSes tend to support Arm first (this is exactly why ST's native VS Code debugging exists and TI's doesn't yet, and it is part of why ST reached native Apple Silicon first). Those are real but modest, and they're already reflected in the toolchain and community rows. Arm is not a tiebreaker here.

---

## Requirements comparison

| Requirement | TI LaunchPad F28P55X | ST Nucleo-H533RE |
|---|---|---|
| MCU, launch | F28P550SJ, **2024** | STM32H533RE, **2024** |
| Core / clock | 150 MHz C28x + CLA, FPU, TMU, NPU | 250 MHz Cortex-M33F (375 DMIPS) |
| Bipolar AI (≥1) | Resistor network (+on-chip PGA buffer) | Resistor network (+on-chip op-amp buffer) |
| Bipolar AO (≥1) | External op-amp stage | External op-amp stage |
| On-chip ADC | 5× 12-bit, 3.9 MSPS, 39 ch | 2× 12-bit, 5 MSPS |
| On-chip DAC (true, buffered) | 1× 12-bit (DACA_OUT on A0, at header) | 1× 12-bit, **2 channels** (PA4/PA5, at headers) |
| Encoder decode + HW index | 3× eQEP, dedicated peripherals | **6 timers** (TIM1/2/3/4/5/8), 5 with index pin on LQFP64; 2 are 32-bit |
| Encoder error flags | Phase error, direction change | Transition, index, index-error, direction (`TERRF`/`IDXF`/`IERRF`/`DIRF`) |
| Encoder 1/T velocity | **In-peripheral capture unit, 1–2048 prescaler, atomic latch** | On-chip via encoder-clock TRGO → 2nd timer capture; /1–/8 prescaler; no vendor app note |
| Encoder stall watchdog | **Yes (QWDOG)** | No dedicated equivalent |
| PWM | **24 ePWM (12 high-res, 150 ps)** | Advanced MC timer + GP timers |
| RT determinism | **Sub-µs interrupts; CLA co-processor for jitter-free loops** | Sub-µs interrupts; CPU-hosted loop only |
| Serial / display | 3 SCI + SPI/I2C/CAN | 3 USART + UARTs, SPI/I2C/FDCAN |
| GPIO (keypad) | 91 | ample (64-pin pkg) |
| Debug on board | XDS110; **isolated** on controlCARD variant | ST-LINK/V3EC, **not isolated, not detachable** |
| Headers / form factor | BoosterPack ×2; controlCARD + HSEC180 edge card available | Arduino Uno V3 + ST morpho; **no card-edge module exists** |
| C libraries | C2000Ware: driverlib + examples **for this board**, DCL, IQmath, FreeRTOS demos | CubeH5: HAL/LL + CMSIS-DSP; no H533RE examples; ThreadX-first; no encoder-velocity AN |
| macOS on Apple Silicon | x86 only; Rosetta 2 required; native promised by end of 2026 | **Native arm64** since Feb 2026 |
| Price (Jul 2026, verified) | ~$35–36, in stock | **~$23–26**, in stock |
| Replaceable-unit cost | ~$190–260 (controlCARD) | **~$26** (whole Nucleo) |
| Lifecycle | 2024 launch; TI precedent → late 2030s | 2024 launch; rolling 10-yr floor (now 01/2036) |

---

## Development environment—near-parity, with one live exception

Both vendors offer free, modern, cross-platform toolchains: ST with STM32CubeIDE plus an official, mature VS Code extension (full build/flash/debug, since STM32 is Arm), TI with the Theia-based CCS, which has VS Code's look and extension model, one-cable flash/debug via the on-board XDS110, and real-time variable watch/graphing that is particularly good in control labs. Either is a dramatic improvement over the first edition's archived myRIO C toolchain. On features, this is a wash.

**There is, however, one axis where the two are not currently equivalent: Apple Silicon.** A substantial share of mechanical-engineering students arrive with Apple Silicon Macs, and Apple is retiring the compatibility layer that TI's tools currently depend on.

**Apple's timeline.** macOS 26 (Tahoe) retains full Rosetta 2. macOS 27, arriving Fall 2026, runs only on M1-or-newer machines and *removes* Rosetta 2 during installation, though it can be manually reinstalled. macOS 28, Fall 2027, is the hard stop: Intel-only applications cease to function. Recent macOS 26.4 builds have begun warning users when an installed app will break.

---

**Where TI stands (as of August 2026): not yet native, but committed with a live deadline.** The current release, CCS 21.0.0 (June 15, 2026), ships a single macOS installer—`CCS_21.0.0.00014_mac_x86.dmg`. TI has delivered part of the work: the 20.4.0 release notes state the Theia IDE component is compiled for Arm, but qualify that "the backend components are still x86 based, so Rosetta is still required." The editor shell went native; the compiler, debug server, and XDS110 driver stack did not.

The commitment is recent and specific. In an E2E thread opened in **July 2026**, a TI engineer answered: "Yes. Work is already in progress for this. A version will be available before the end of the year, if not sooner." That places delivery inside the remaining months of **2026**, and the deadline has not passed.

**Where ST stands: already native.** An ST engineer confirmed that "the very first official release of STM32CubeIDE (Eclipse-based) with native support for macOS on ARM was delivered recently, at the end of February," with the VS Code extension pack on track to follow.

**Assessment.** ST holds a real advantage today. It should not by itself reverse the recommendation—TI has committed publicly and recently, completed the frontend half, named a deadline still open, and has a large education business that would be badly damaged by CCS failing on student Macs. The residual risk is schedule slip, not direction. It gets an explicit watch item below.

---

## Form factor: controlCARD has no ST equivalent

Because the course builds its own carrier board (motor amplifier, connectors, motor, encoder), how the MCU attaches to that carrier is a real design decision, and the two vendors answer it very differently.

**TI—controlCARD (TMDSCNCD28P55X / TMDSCNCD28P65X).** A card carrying the MCU plus an **isolated** XDS110, with a 180-contact HSEC8 card edge that plugs into a socket on the carrier. Verified details from the HSEC180 standard pinout in C2000Ware: 180 contacts are real (odd 1–179 on one face, even 2–180 on the other, 90 per face; pins 1–48 analog with JTAG on 1–8, 49–180 digital); about **127 of the 180 are usable signals** after 18 GND, 6× 5V0 plus VDD and VDDIO, VREFHI/VREFLO, 17 reserved, 7 JTAG, and reset; the standard reserves **pin 9 for DACA and pin 11 for DACB**, both shared with ADC1 inputs; and it exposes exactly **two complete eQEP interfaces** (pins 68/70/72/74 and 100/102/104/106), plus 24 analog channel positions, 16 ePWM, 8 SPI/eCAP, 6 sigma-delta, and 58 general GPIO. Because this is a *standard* shared across all 180-pin controlCARDs, a carrier designed to it accepts any card in the family. Cost is roughly $190–260 per card versus ~$35 for the LaunchPad, stock is thin (tens of units across authorized distributors, 12-week factory lead times once depleted), so a cohort's worth should be bought in one order.

---

**ST—no equivalent exists, and the substitute is different in kind.** There is no ST card-edge MCU module for any STM32 family. The "SOM-STM32xx" pages on st.com are partner pages for Emcraft, not ST products, and those SOMs are castellated solder-down modules on proprietary pinouts. STMod+ is a 20-pin peripheral-daughterboard connector, the STM32 analogue of mikroBUS—it does not carry an MCU. The STM32MP1/MP2 SOMs are Cortex-A Linux parts, wrong architecture for this course. MikroElektronika's "MCU CARD for STM32" is conceptually the closest match—a standardized swappable MCU card—but it is a 168-pin Hirose mezzanine on a MikroE-proprietary standard, carries no debugger, and **no STM32H5 card exists** for it.

ST's actual model runs the opposite direction: the Nucleo is the fixed base and expansion boards stack on top of it. So the practical ST route is to **treat the whole Nucleo-H533RE as the swappable module and build the carrier as a shield** that mates to its Arduino Uno V3 and ST morpho headers. This is ST's own supported pattern—the X-NUCLEO-IHM16M1 three-phase motor driver board does exactly this and even includes a quadrature-encoder connector.

Three consequences worth weighing:

- **Cost runs strongly ST's way.** A destroyed unit costs ~$26 (a whole Nucleo, debugger included) versus ~$190–260 for a controlCARD—roughly 8–10× cheaper per replaceable unit. For a course where students overvolt things, that is not a small point.
- **Mechanical and standards robustness run TI's way.** HSEC8 edge insertion is polarized and rated for many insertion cycles; a 38+38-pin 0.1" header stack has high mating force, weak keying, and is easy to misalign by a row. More importantly, the morpho pinout is **not** guaranteed consistent across Nucleo-64 boards—ST publishes a separate table per board, and the H533RE genuinely differs from the F4-generation morpho on a number of GPIO positions. If "this carrier design outlives one MCU" is a goal for the book, TI's published family-wide standard delivers that and ST does not.
- **Isolation is a real gap, and it is new to this analysis.** The controlCARD's XDS110 is isolated. The Nucleo's ST-LINK/V3EC is **not isolated and not physically detachable** (JP1 only holds it in reset so an external probe can drive the MIPI10 connector). With a motor amplifier sharing ground on the carrier, that ties a student's laptop USB port to the drive ground. ST's answer, B-STLINK-ISOL, is an accessory for the separate STLINK-V3SET probe, not for the on-board one. For a motor-drive course this deserves an explicit decision: isolate the power stage on the carrier, or budget USB isolators.

One further idea worth considering if the group leans ST: terminate the carrier at **ST's 34-pin motor-control connector** (pinout published in UM1970—3-phase PWM, current feedback, bus-voltage sense, Hall/encoder inputs, temperature) rather than directly at the morpho headers. That puts a published, family-independent interface at the amplifier boundary and insulates the carrier from Nucleo pinout churn.

---

## Would spending more buy anything? (up-market scan)

Both finalists are cheap, so it's fair to ask whether the selection was distorted by thriftiness. It wasn't—the up-market options were checked explicitly, and the pattern is consistent in both ecosystems: **the control-peripheral ceiling is already reached at the cheap boards; extra money buys cores, MHz, Ethernet, and safety certification, none of which the labs exercise.**

**TI, one tier up—LaunchPad F28P65X (~$66).** Dual 200 MHz C28x cores, ADCs with a 16-bit mode, two buffered DACs, six eQEP modules, more memory. Note what does *not* change: there is still exactly **one CLA**, shared across the two C28x cores. What the upgrade actually adds as a co-processor is a second full C28x core—arguably more useful than another CLA, since it runs arbitrary C with full peripheral access. The one thing lost is the NPU, exclusive to the P55x. Everything else is a superset, and the toolchain, driverlib, DCL, and book code carry over with a device-target change. Open item: whether the F28P65x's DACA and DACC land on the standard's pin 9/11 DAC positions must be confirmed from its schematic (SPRR478).

**TI, bleeding edge—LaunchPad F29H85X (roughly $80–100, restricted distribution).** TI's next-generation C29-core flagship—three 200 MHz 64-bit cores, six eQEP, 36 ePWM, lockstep functional safety. Verdict: wrong platform for a textbook anchor. The silicon carries **PREVIEW** status, distribution is restricted, the software stack is a new SDK rather than mature C2000Ware, there is no CLA, **no buffered DAC is listed**, and the C28x educational corpus does not transfer.

**TI, Arm industrial—LP-AM263 (well north of $100).** Quad Cortex-R5F at 400 MHz with C2000-style control peripherals and industrial Ethernet. Buys compute and connectivity the labs don't need, at the cost of multicore boot complexity that would hurt teaching.

**ST, sideways—Nucleo-H563ZI (~$27).** The H563 is one of the boards ST's Cube H5 examples actually target (curing the H533RE's no-examples caveat), and adds Ethernet and a 144-pin package. It gives up the H533's op-amp and comparator, which this design doesn't use. If the group chooses ST, the H563ZI deserves consideration over the H533RE.

**ST, up-market—H7 / H7R-S / Discovery kits / MP-series.** More money buys clock speed, displays, and eventually Linux—never better control peripherals, and never a card-edge form factor. The mainline H7 is 2017–2019 silicon; the MP-series' Linux reintroduces the determinism problem the book left behind with the myRIO.

**Conclusion:** price was never the binding constraint. The peripherals that decide this comparison are present (or absent) identically up and down each vendor's price ladder.

---

## Recommendation

**Lean: LaunchPad F28P55X—but narrowly, and on different grounds than rev 7 gave.**

The encoder argument that previously carried this recommendation does not survive verification. Both parts decode quadrature, reset on index, and flag illegal transitions in hardware; the H533 has more encoder-capable timers than the F28P55x has eQEP modules, and richer index conditioning. What TI retains there is 1/T velocity inside a single peripheral with a deep prescaler and atomic latching, a hardware stall watchdog, and—for a textbook, maybe most importantly—documentation of the method that ST simply does not publish.

What actually justifies the lean now:

- **The CLA.** An independent co-processor that hosts the control law with hardware-triggered, jitter-free timing has no ST equivalent at any price. It is both a practical advantage and the best teaching device in the comparison: implement the loop as a timer ISR, measure the jitter under load, move it to the CLA, watch the jitter collapse.
- **Curriculum alignment.** C2000Ware ships examples for this exact board, the DCL implements the compensators the book teaches, FreeRTOS demos are in-box, C2000 Academy is written for this kind of course, and TI documents encoder velocity methods. ST's stack is broader and more portable but has an example gap for this board, a ThreadX-first middleware story, and no encoder-velocity reference.
- **PWM depth and the TMU**, which are headroom rather than requirements, but free.
- **A published, family-wide carrier standard** if the group goes the controlCARD route, with isolated debug included—neither of which ST offers.

The case for ST is stronger than rev 7 admitted, and it now has four real legs: a $12 cheaper board and an **~8–10× cheaper replaceable unit**, a native Apple Silicon toolchain today, a second DAC channel, and encoder hardware that is genuinely competitive. If the group weighs replacement cost heavily—plausible in a lab where students destroy hardware—or judges the Apple Silicon risk unacceptable, the H533RE is a defensible pick and nothing in the labs would be blocked. The honest summary is that this is now a close call decided by ecosystem fit and the CLA, not by a decisive peripheral advantage.

---

## Remaining open questions for the group

- **Range:** ±5 V or ±3.3 V? Input stage is marginally simpler at ±3.3 V; the output stage is identical either way. Best decided from the labs' actual sensor/actuator interfaces.
- **Encoder count:** how many simultaneous encoders does the heaviest lab use? TI has 3 eQEP (6 on the F28P65x); the H533 has six encoder-capable timers, five with an index pin in the 64-pin package. Note a constraint if the controlCARD form factor is adopted: the HSEC180 standard exposes only two complete eQEP interfaces, so a third encoder means muxing eQEP signals onto pins the standard assigns to other functions.
- **DAC channel count:** the labs are believed to use one analog output; if a second is firmly required, the H533 has two channels and the F28P55X has one (a second TI channel would come from a ~$3 SPI DAC on the custom board).
- **Form factor and replacement economics:** LaunchPad + headers, controlCARD + HSEC180 socket, or Nucleo-as-shield? This is now a bigger question than it looked, because the replaceable-unit cost differs by ~10× and only TI offers a family-wide pinout standard.
- **Debug isolation (new):** if the carrier hosts a motor amplifier, the Nucleo's non-isolated, non-detachable ST-LINK ties student laptops to drive ground. Decide whether to isolate the power stage on the carrier or budget USB isolators. The controlCARD's isolated XDS110 sidesteps this.
- **WATCH—native arm64 CCS for Apple Silicon.** TI's public commitment (E2E, July 2026) is a native macOS build "before the end of the year, if not sooner," and that deadline is still open. Checkpoints: **(1)** each CCS release, confirm whether the macOS download list has gained an arm64 DMG; **(2)** **January 2027**—if TI's own deadline has passed with no native build, escalate to a decision input; **(3)** **before press**—if still x86-only, document a Rosetta-reinstall procedure for Mac students and note that macOS 28 (Fall 2027) ends Intel-app execution outright.
- **To verify before press (rev 8 gaps):** which `ITRx` index carries each timer's TRGO on the H533 (needed if the book presents the ST 1/T topology); whether the STM32 compare/capture-on-TRC paths remain available while `SMS` is set to an encoder mode; and confirmation of the eQEP-side claims from SPRUI33 if the book prints register-level detail.

---

## Sources

- [ST—product longevity program](https://www.st.com/content/st_com/en/about/quality-and-reliability/product-longevity.html)
- [ST—STM32H533RE product page](https://www.st.com/en/microcontrollers-microprocessors/stm32h533re.html)
- [ST—NUCLEO-H533RE board page](https://www.st.com/en/evaluation-tools/nucleo-h533re.html)
- [Octopart—NUCLEO-H533RE price/stock](https://octopart.com/part/stmicroelectronics/NUCLEO-H533RE)
- [TI—TMS320F28P550SJ product page](https://www.ti.com/product/TMS320F28P550SJ)
- [TI—TMS320F28P55x datasheet (DACA_OUT on A0, 100-pin PZ pin 23)](https://www.ti.com/lit/ds/symlink/tms320f28p550sj.pdf)
- [TI—LAUNCHXL-F28P55X user guide SPRUJC0 (§2.1.3.8 PWM-DAC)](https://www.ti.com/lit/ug/sprujc0a/sprujc0a.pdf)
- [TI—LAUNCHXL-F28P55X Pinout Map (SPAZ056)](https://www.ti.com/lit/pdf/spaz056)
- [TI—LAUNCHXL-F28P55X](https://www.ti.com/tool/LAUNCHXL-F28P55X)
- [TI—C2000Ware SDK](https://www.ti.com/tool/C2000WARE)
- [TI—Digital Control Library user's guide (SPRUID3)](https://www.ti.com/lit/ug/spruid3/spruid3.pdf)
- [ST—STM32CubeH5 on GitHub](https://github.com/STMicroelectronics/STM32CubeH5)
- [TI—High-Voltage Signal Conditioning for Low-Voltage ADCs (SBOA097)](https://www.ti.com/lit/an/sboa097b/sboa097b.pdf)
- [NI myRIO-1900 User Guide (±10 V MSP-C baseline)](https://download.ni.com/support/manuals/376047c.pdf)
- [TI—F29H850TU product page](https://www.ti.com/product/F29H850TU) · [TI—LAUNCHXL-F29H85X](https://www.ti.com/tool/LAUNCHXL-F29H85X) · [TI—LP-AM263](https://www.ti.com/tool/LP-AM263)
- [ST—STM32H563ZI product page](https://www.st.com/en/microcontrollers-microprocessors/stm32h563zi.html)

**Encoder peripherals (rev 8—the corrected section):**

- [ST—STM32H533 TIM2 encoder control register (ECR): IE, IDIR, IBLK, FIDX, IPOS](https://docs.rs/stm32h5/latest/stm32h5/stm32h533/tim2/ecr/index.html)—generated from ST's SVD; the primary evidence that hardware index reset exists on this part.
- [ST—STM32H5 TIM status-register flags: IDXF, DIRF, IERRF, TERRF](https://docs.rs/stm32h5/latest/stm32h5/stm32h503/tim2/sr/index.html)—hardware transition-error and index-error detection.
- [ST—AN4013, STM32 cross-series timer overview](https://www.st.com/resource/en/application_note/an4013-stm32-crossseries-timer-overview-stmicroelectronics.pdf)—§3.2 documents "encoder clock output" as a TRGO source, the basis of the on-chip 1/T topology.
- [ST—AN4776, general-purpose timer cookbook](https://www.st.com/resource/en/application_note/an4776-generalpurpose-timer-cookbook-for-stm32-microcontrollers-stmicroelectronics.pdf)—explicitly excludes motor-control use cases; part of the evidence that ST publishes no encoder-velocity method.
- [ST—stm32h533xx.h CMSIS device header](https://raw.githubusercontent.com/STMicroelectronics/cmsis_device_h5/main/Include/stm32h533xx.h)—`IS_TIM_ENCODER_INTERFACE_INSTANCE` (six timers) and `IS_TIM_32B_COUNTER_INSTANCE` (TIM2, TIM5 only).
- [ST—STM32H533RETx pin data](https://raw.githubusercontent.com/STMicroelectronics/STM32_open_pin_data/master/mcu/STM32H533RETx.xml)—TIM4 has no ETR (index) pin in LQFP64.
- [ST—STM32H533xx datasheet DS14539](https://www.st.com/resource/en/datasheet/stm32h533ce.pdf)—source of the "two 32-bit timers" bullet that earlier revisions misread as an encoder count.
- [ST community—TIM encoder mode with encoder clock output](https://community.st.com/t5/stm32-mcus-embedded-software/tim-encoder-mode-with-encoder-clock-output/td-p/790944)—working encoder-clock → ITR → capture chain on the same TIM IP generation.
- [ST community—index (Z) in encoder mode, older STM32 families](https://community.st.com/t5/stm32-mcus-products/how-to-use-index-track-in-quadrature-encoder-mode/td-p/507598)—**the source of the rev 1–7 error**; accurate for F4/F7/H7, not for H5. Retained as a caution.

**Form factor and modules (rev 8):**

- [TI—TMDSCNCD28P55X controlCARD](https://www.ti.com/tool/TMDSCNCD28P55X) · [TMDSCNCD28P65X](https://www.ti.com/tool/TMDSCNCD28P65X) · [180-pin docking station guide SPRUIJ6](https://www.ti.com/lit/pdf/spruij6) · [controlCARD schematic SPRR478](https://www.ti.com/lit/pdf/sprr478)
- HSEC180 controlCARD standard map—parsed from C2000Ware `boards/ExperimenterKits/DockingStation_HSEC_120or180pin/revF/180_HSEC8_DV_pinout_Rev_F.pdf`; full 180-pin table in project files as `HSEC180_controlCARD_standard_pinout.csv`
- [ST—UM3121, STM32H5 Nucleo-64 board (MB1814)](https://www.st.com/resource/en/user_manual/um3121-stm32h5-nucleo64-board-mb1814-stmicroelectronics.pdf)—morpho pinout for this board; ST-LINK/V3EC non-detachable, JP1 reset only.
- [ST—X-NUCLEO-IHM16M1 motor driver expansion board](https://www.st.com/en/evaluation-tools/x-nucleo-ihm16m1.html)—existence proof of the carrier-as-shield pattern with an encoder connector.
- [ST—UM1970 (34-pin motor-control connector pinout, Table 4)](https://www.st.com/resource/en/user_manual/um1970-getting-started-with-the-xnucleoihm09m1-motor-control-connector-expansion-board-for-stm32-nucleo-stmicroelectronics.pdf)
- [ST—TN1238, STMod+ interface specification](https://www.st.com/resource/en/technical_note/tn1238-stmod-interface-specification-stmicroelectronics.pdf)—confirms STMod+ is a peripheral-daughterboard port, not an MCU module.
- [ST—B-STLINK-ISOL](https://www.st.com/en/development-tools/b-stlink-isol.html)—isolation accessory, for STLINK-V3SET rather than the Nucleo's on-board probe.
- [MikroElektronika MCU CARD for STM32 (ST partner page)](https://www.st.com/en/partner-products-and-services/mcu-card-for-stm32.html)—closest third-party analogue; proprietary 168-pin mezzanine, no STM32H5 card, no on-card debugger.
- [Emcraft STM32H7 SOM](https://www.emcraft.com/som/stm32h7/stm32h7-som-ha-1_1.pdf)—castellated solder-down, proprietary pinout; not swappable.
- [WeAct STM32H562RG core board (open hardware)](https://github.com/WeActStudio/WeActStudio.STM32H5_64Pin_CoreBoard)—cheapest STM32H5 module, proprietary pinout, hobby supply chain.

**Apple Silicon / Rosetta 2:**

- [TI E2E—"Is CCS going to run on Apple silicon without depending on Rosetta?"](https://e2e.ti.com/support/processors-group/processors/f/processors-forum/1655979/ccstudio-is-ccs-going-to-run-on-apple-silicon-without-depending-on-rosetta)—**posted July 2026**; "A version will be available before the end of the year, if not sooner." The commitment is live, not lapsed.
- [TI—CCS 20.4.0 release notes](https://software-dl.ti.com/ccs/esd/CCSv20/CCS_20_4_0/exports/CCS_20.4.0_ReleaseNote.htm) · [TI—CCS 21.0.0 download page](https://www.ti.com/tool/download/CCSTUDIO/21.0.0)—the page to re-check each release.
- [ST community—the future of ST tools on macOS](https://community.st.com/stm32cubeide-for-visual-studio-code-mcus-133/the-future-of-st-tools-on-macos-in-2027-forward-deprecation-of-rosetta-2-163538) · [AppleInsider—macOS Intel-app timeline](https://appleinsider.com/articles/26/06/12/how-and-when-macos-will-finally-stop-support-for-intel-apps)

**Method note on rev 8:** the encoder corrections were verified against ST's own register definitions (SVD-derived), CMSIS device headers, HAL sources, pin database, and application notes—not against community posts. The rev 1–7 error came from trusting a forum thread about a different silicon generation. Items that could not be verified from a primary ST document are listed in the open questions rather than asserted.