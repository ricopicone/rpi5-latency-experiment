---
project_note_id: 1018
title: 'TI vs ST: Next-Generation Platform Finalists'
---

# TI vs ST: Next-Generation Platform Finalists

**Real-Time Computing for Mechanical Engineers, 2nd Edition**
Board comparison—internal—August 2026 (rev 5)

---

## Context and bottom line

The first edition's platform, the NI myRIO-1900, is discontinued, and a replacement is needed for the second edition. An earlier memo made the case for the TI LaunchPad F28P55X; the group subsequently asked that an ST evaluation board be considered head-to-head, and added one new requirement: at least one bipolar analog input and one bipolar analog output (±5 V is sufficient; ±3.3 V acceptable—the myRIO provided ±10 V on its MSP connector, but the labs do not need the full range). This document is that comparison.

Two finalists: the TI **LaunchPad F28P55X** (TMS320F28P550SJ) and the ST **Nucleo-H533RE** (STM32H533RE)—both 2024 silicon. ST's classic mixed-signal control board, the Nucleo-G474RE, was considered and set aside: 2019 silicon already seven years into its life, and its headline advantage—six on-chip op-amps—turns out not to matter for the bipolar front-end (see below). It is not discussed further.

At the target ranges the analog front-end is **essentially identical for either board**—a handful of resistors on the input side and one external op-amp stage on the output side—so on-chip analog does not differentiate the boards. The IDE story is likewise a wash. What's left is real-time-control fit, lifecycle, and price, and on that basis **the F28P55X takes the lead**: dedicated eQEP encoder hardware, 24 ePWM channels, the CLA and TMU (glossed in the next section), a purpose-built motor-control curriculum, and 2024 silicon—at a $12 premium over the ST board.

---

## Lifecycle: reading the dates correctly

**ST publishes a date, but it's a rolling floor, not a launch-anchored runway.** ST's listed "available until" dates are a **10-year commitment restarting from January 1 of the current year** (every active STM32 currently shows January 2036 regardless of launch year). ST has historically kept renewing these dates annually while a part stays in the longevity program, but the *guarantee* at any moment is only ~10 years ahead, and ST's policy allows ending renewals if volumes fall—which is why design freshness matters: the H533 launched in **2024**.

**TI publishes no per-part date but has the stronger track record.** The F28P55x also launched in 2024; TI's C2000 parts routinely stay in production 15–20 years (the 2007-era F2833x and 2016-era F28379D are both still ACTIVE). Expectation: production into the late 2030s.

**Conclusion:** with both finalists on 2024 silicon, expected longevity is comparable and excellent; TI's assurance is precedent-based, ST's is contractual-but-rolling.

---

## The finalists in brief

The **TI LaunchPad F28P55X** (~$35–36, in stock at DigiKey and Mouser) carries the TMS320F28P550SJ: a 150 MHz C28x real-time core plus an FPU, a small NPU, and two control-specific accelerators worth defining. The **CLA (Control Law Accelerator)** is an independent 150 MHz floating-point co-processor that runs the fast inner control loop in parallel with the main core—triggered directly by peripheral events (ADC end-of-conversion, PWM period) with direct register access and no interrupt arbitration, so the loop executes with essentially zero jitter regardless of what the main CPU is doing. The **TMU (Trigonometric Math Unit)** extends the FPU with hardware sin/cos/atan2/sqrt/divide instructions that complete in a few cycles—the exact operations field-oriented motor control evaluates every loop iteration, which software trig on a generic core spends tens to hundreds of cycles on. Around these: five 12-bit ADCs (3.9 MSPS), one buffered 12-bit DAC, three dedicated eQEP encoder modules (two connectors pre-wired on the board), 24 ePWM channels, and the free Code Composer Studio toolchain. It was the subject of the earlier selection memo, which covers it in more depth.

The **ST Nucleo-H533RE** (~$23–26, in distribution) carries the STM32H533RE: a 250 MHz Cortex-M33F—375 DMIPS, notably quicker in scalar terms than the C28x. It covers every book requirement: two 12-bit ADCs at up to 5 MSPS, a **two-channel** buffered 12-bit DAC (one more analog output than the TI part provides), quadrature-encoder mode on two 32-bit general-purpose timers, an advanced motor-control timer for PWM, three USARTs plus UARTs, plenty of GPIO, and one on-chip op-amp and comparator.

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

Bottom line: the "DAC = filtered PWM" pattern belongs to older/cheaper parts (and to hobby boards like the older Arduinos); both finalists provide true converter DACs, and the front-end design consumes them directly. The PWM-DAC pins on the TI board can simply be ignored—or treated as a free bonus scope channel for debugging, which is all TI ever intended them to be.

---

## Encoder interfaces: eQEP vs. timer encoder mode—the strongest TI argument

Both chips can count quadrature edges in hardware with zero CPU load: digital input filters, 4× decode, an up/down position counter. For a single encoder at moderate speed with no index pulse, they are equivalent, and the labs would run on either. The differences appear exactly where a mechatronics course wants to go deeper.

**Index (Z) handling.** The eQEP has dedicated index hardware: it can latch the position counter on the index edge, reset it every revolution, or both—homing and revolution counting happen entirely in silicon, with no interrupt and no latency. The STM32 timer, by contrast, has no index input in encoder mode—and this is architectural, not a configuration gap: encoder mode occupies the timer's slave-mode controller, so the "reset counter on trigger" trick is unavailable, and ST's own community guidance is to route Z to an EXTI interrupt and zero the counter in software. That works at low speed, but the ISR latency means the reset lands some number of counts late—and the error varies with speed. For any lab involving homing or absolute-within-a-rev position, TI does in hardware what ST approximates in an ISR.

---

**Velocity estimation.** This is the pedagogically rich one. Measuring speed from an encoder has two classical methods: count edges per fixed time (T method—good at high speed, quantization-noisy at low speed) and time the interval between edges (1/T method—excellent at low speed). The eQEP contains both: a unit timer for the T method and an **edge-capture unit** that timestamps encoder edges in hardware for the 1/T method, with a prescaler to pick the edge stride. A real-time-computing book can teach the T vs. 1/T trade-off—a genuinely classic topic in motion control—and implement both with clean hardware support. On the STM32, only the T method comes naturally (read the counter every sample period); implementing 1/T means re-plumbing the encoder signal into additional capture channels on another timer and servicing capture interrupts, a workaround rather than a lesson.

**Integrity.** The eQEP raises a phase-error flag on illegal quadrature transitions (both channels changing at once—the signature of noise or a failing encoder), has a watchdog that flags stalled motion, and can interrupt on direction change. The STM32 timer silently miscounts on an illegal transition; there is no error detection. In a student lab full of long unshielded encoder cables next to motor PWM, "the hardware tells you the count is corrupted" versus "the count is silently wrong" is a real difference in debuggability—and a teachable one.

**Resource accounting.** eQEP modules are dedicated peripherals—all three come free without consuming timers, and the LaunchPad has two encoder connectors pre-wired. On the H533, each encoder consumes one of the two 32-bit general-purpose timers, which are also the timers you'd otherwise use for capture or extra PWM.

In short: the STM32 timer *can be coaxed into* encoder work; the eQEP *was designed for* it. Given that encoder-based motion control sits at the center of the book's labs, this is the single strongest technical argument for the TI board.

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

**On the ST side**, the equivalent is a timer ISR on the Cortex-M33, with the same contention characteristics as the C28x timer ISR—NVIC latency is comparable and the peripheral-triggered path (timer → ADC → DMA) can remove the CPU from the data path, but there is no independent co-processor to host the control law. The CPU-loop story is a wash; the CLA option exists only on the TI board.

---

## C library and driver support—verified

Both vendors ship free, comprehensive C support, so neither board is risky here—but they are not identical, and for this book's purposes TI's package is the better fit.

**TI: C2000Ware.** One SDK containing the peripheral driver library (driverlib), bit-field register headers, SysConfig code generation, and—explicitly confirmed for the F28P551x—device examples, FreeRTOS demos, and the math libraries. Driverlib's style is thin, readable, one-function-per-operation wrappers over registers (`EQEP_setPosition(...)`), which sits at exactly the abstraction level a real-time computing course wants: students can read the driver source and see the register write. The bit-field headers offer the fully manual alternative when a chapter wants to go all the way down. Two inclusions matter specifically for us: the **Digital Control Library (DCL)**—PID/PI/2p2z/3p3z compensators hand-optimized for the C28x and CLA, with anti-windup and saturation handling, i.e., the exact algorithms the book teaches, shipped as documented reference code—and **IQmath**, the classic fixed-point library, if the book keeps its fixed-point-arithmetic material. The C2000 Academy curriculum teaches against driverlib, so course materials and vendor materials align.

---

**ST: STM32CubeH5.** The HAL (high-level, portable, verbose) plus the LL (low-layer, register-close—the better teaching layer, comparable in spirit to driverlib), CMSIS core and the well-documented CMSIS-DSP library (FIR/IIR/matrix/PID), BSPs, and middleware. Two caveats, both confirmed against ST's published package contents. First, the Cube H5 example projects target the **NUCLEO-H563ZI, NUCLEO-H503RB, and H573I-DK—not the NUCLEO-H533RE**; the HAL/LL drivers fully cover the H533 silicon and CubeMX generates correct init code for it, but students wouldn't get a folder of ready-made examples for their exact board, and the course would adapt H563/H503 projects. Second, ST's packaged RTOS middleware for H5 is the ThreadX family (Azure RTOS), not FreeRTOS—FreeRTOS runs fine on Cortex-M33 via its own port, but it arrives from outside the vendor package, whereas C2000Ware ships FreeRTOS demos for the F28P55x directly.

**Verdict:** both ecosystems clear the bar comfortably; the difference is alignment. TI provides examples for this exact board, a control-law library that mirrors the book's syllabus, and the book's preferred RTOS in-box. ST provides a broader, more portable stack with two small frictions for our specific board and RTOS choice. Score this one narrowly for TI.

---

## Does Arm matter for us?

It's tempting to count "Arm" as an ST advantage, but for this book it mostly isn't—and the first edition is the reason why. The myRIO's ARM was a Cortex-A9 **application processor** (Armv7-A) running NI's Linux Real-Time: virtual memory, an OS scheduler, NI's C API over kernel drivers. The H533's Cortex-M33 is a **microcontroller core** (Armv8-M): no MMU, no Linux, bare-metal or RTOS. These share a brand name and almost nothing else that a student touches—so there is no meaningful continuity from edition 1 in choosing Arm again.

Nor does the ISA matter much going forward: students write C against peripheral registers and vendor drivers on either chip, and the instruction set is invisible outside the debugger's disassembly window. The genuine Arm-ecosystem benefits are generic ones—skills nominally transfer across the many Cortex-M vendors, and third-party tools/RTOSes tend to support Arm first (this is exactly why ST's native VS Code debugging exists and TI's doesn't yet). Those are real but modest, and they're already reflected in the toolchain and community rows. Arm is not a tiebreaker here; the peripherals are.

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
| Encoders (see dedicated section) | **3× eQEP: HW index, HW 1/T velocity capture, error flags; 2 connectors on board** | Encoder mode on 2× 32-bit GP timers; index via EXTI ISR; T-method velocity only |
| PWM | **24 ePWM (12 high-res, 150 ps)** | Advanced MC timer + GP timers |
| RT determinism | Sub-µs interrupts; CLA co-processor for jitter-free loops | Sub-µs interrupts; CPU-hosted loop only |
| Serial / display | 3 SCI + SPI/I2C/CAN | 3 USART + UARTs, SPI/I2C/FDCAN |
| GPIO (keypad) | 91 | ample (64-pin pkg) |
| Debug on board | XDS110 | ST-LINK/V3EC |
| Headers | BoosterPack ×2 | Arduino Uno V3 + Morpho |
| C libraries (see dedicated section) | C2000Ware: driverlib + examples **for this board**, DCL control library, IQmath, FreeRTOS demos | CubeH5: HAL/LL + CMSIS-DSP; no H533RE example projects; ThreadX-first middleware |
| Community | Large; C2000 Academy, motor-control education canon | Growing (newer part); big STM32 base |
| Price (Jul 2026, verified) | ~$35–36, in stock | ~$23–26, in stock |
| Lifecycle | 2024 launch; TI precedent → late 2030s | 2024 launch; rolling 10-yr floor (now 01/2036) |

---

## Development environment—brief, because it's a wash

Both vendors now offer free, modern, cross-platform toolchains: ST with STM32CubeIDE plus an official, mature VS Code extension (full build/flash/debug, since STM32 is Arm), TI with the Theia-based CCS 20, which has VS Code's look and extension model, one-cable flash/debug via the on-board XDS110, and real-time variable watch/graphing that is particularly good in control labs (native VS Code debug for C28x is on TI's roadmap but not shipped). Either is a dramatic improvement over the first edition's archived myRIO C toolchain, and neither should decide this choice.

---

## Would spending more buy anything? (up-market scan)

Both finalists are cheap, so it's fair to ask whether the selection was distorted by thriftiness. It wasn't—but the up-market options were checked explicitly, and the pattern is consistent in both ecosystems: **the control-peripheral ceiling is already reached at the cheap boards; extra money buys cores, MHz, Ethernet, and safety certification, none of which the labs exercise.**

**TI, one tier up—LaunchPad F28P65X (~$66).** Dual 200 MHz C28x cores, ADCs with a 16-bit mode, two buffered DACs, six eQEP modules, more memory. Note what does *not* change: there is still exactly **one CLA**, shared across the two C28x cores. What the upgrade actually adds as a co-processor is a second full C28x core—arguably more useful than another CLA, since it runs arbitrary C with full peripheral access where the CLA runs small trigger-launched tasks. The one thing lost is the NPU, which is exclusive to the P55x. Everything else is a superset, and the toolchain, driverlib, DCL, and book code carry over with a device-target change.

---

**TI, bleeding edge—LaunchPad F29H85X (roughly $80–100, restricted distribution).** This is the one genuinely new thing money could buy: TI's next-generation C29-core flagship—three 200 MHz 64-bit cores, two 16-bit plus three 12-bit ADCs, six eQEP, 36 ePWM, lockstep functional safety. It was checked carefully because it signals TI's long-term direction. Verdict: wrong platform for a textbook anchor. The silicon carries **PREVIEW** status, the LaunchPad is in restricted distribution, the software stack is a new SDK rather than mature C2000Ware, there is no CLA (its role is absorbed by the extra cores), **no buffered DAC is listed**, and the enormous C28x educational corpus (C2000 Academy, DCL, decades of app notes) does not yet transfer. A first printing should not ride a preview part—and the C28x is in no danger: TI's 15–20-year production precedent plus the F28P55x's 2024 launch covers the edition's life comfortably.

**TI, Arm industrial—LP-AM263 (well north of $100).** Quad Cortex-R5F at 400 MHz with C2000-style ePWM/eQEP control peripherals and industrial Ethernet (EtherCAT/PROFINET). Money here buys compute and connectivity the labs don't need, at the cost of multicore boot/configuration complexity that would actively hurt teaching. The control peripherals are the same class as the $35 board's.

---

**TI, form factor—controlCARD (TMDSCNCD28P55X / TMDSCNCD28P65X).** Both finalist-family devices are also sold as controlCARDs: a card with a 180-contact HSEC8 edge connector and an isolated on-board XDS110, designed to plug into a custom carrier board. For a course that already builds its own motor/interface board this is attractive—the MCU becomes a swappable module, so a destroyed part means replacing a card rather than the whole custom board. Verified details, from the HSEC180 standard pinout in C2000Ware:

- **180 contacts are real**: odd pins 1–179 on one face of the card, even pins 2–180 on the other, 90 per face. Pins 1–48 are the analog block (JTAG occupies 1–8); 49–180 are digital.
- **About 127 of the 180 are usable signals.** The overhead is 18 GND, 6× 5V0 plus VDD and VDDIO, VREFHI/VREFLO, 17 reserved, 7 JTAG, and device reset.
- **The standard reserves pin 9 for DACA and pin 11 for DACB**, both shared with ADC1 inputs—so a standards-compliant carrier gets two DAC positions, which is the mechanism by which the F28P65x's second buffered DAC would actually reach a custom board.
- **The standard exposes exactly two complete eQEP interfaces**—pins 68/70/72/74 and 100/102/104/106, each carrying A, B, strobe, and index. This is the practical cap on encoders at the standard positions, regardless of the six eQEP modules the F28P65x contains.
- Beyond those: 24 analog channel positions, 14 ePWM, 8 SPI/eCAP, 6 sigma-delta, and 56 general GPIO.

Because this map is a *standard* shared across all 180-pin controlCARDs, a carrier board designed to it accepts any card in the family—which is what makes a "book on one part, projects on another" split mechanically clean. Costs: roughly $190–260 per card versus ~$35 for the LaunchPad, and stock is thinner (tens of units across authorized distributors, with 12-week factory lead times once depleted), so a cohort's worth should be bought in one order rather than restocked mid-semester. Lifecycle and supply-chain risk are both rated Low.

One caveat to settle before committing copper: the P55x and P65x controlCARD folders in C2000Ware contain only web redirects, not per-card pinout PDFs, so the card-specific question—whether the F28P65x's DACA and DACC land on the standard's pin 9/11 DAC positions—must be confirmed from its schematic (SPRR478) rather than assumed.

---

**ST, sideways—Nucleo-H563ZI (~$27).** Not meaningfully more expensive, but worth flagging: the H563 is one of the boards ST's Cube H5 example projects actually target (curing the H533RE's no-examples caveat), and it adds Ethernet and a 144-pin package. It gives up the H533's op-amp and comparator—which this design doesn't use. If the group chooses ST, the H563ZI deserves consideration over the H533RE. It does not change the TI-vs-ST decision: its encoder situation (timer mode, EXTI index, T-method velocity) is identical.

**ST, up-market—H7 / H7R-S / Discovery kits / MP-series.** More money on the ST side buys clock speed (480–600 MHz Cortex-M7), displays, and eventually Linux (MP-series)—never better control peripherals. No ST part at any price adds eQEP-class encoder hardware, hardware trig, or a CLA analog; the mainline H7 is 2017–2019 silicon; the 2024 H7R/S line is graphics/memory-focused with thin analog; and the MP-series' Linux reintroduces exactly the determinism problem the book left behind with the myRIO.

**Conclusion:** price was never the binding constraint. The peripherals that decide this comparison—encoder hardware, control accelerators, true DACs—are all present (or absent) identically up and down each vendor's price ladder. The inexpensive finalists stand.

---

## Recommendation

**Lean: LaunchPad F28P55X.** With the front-end and IDE effectively identical across the two boards, lifecycle even (both 2024 parts), and Arm set aside as a non-factor, the decision rests on the peripherals—and there the eQEP section is the headline: hardware index handling, hardware 1/T velocity capture, quadrature-error detection, and two pre-wired encoder connectors, versus a timer mode that must be coaxed and interrupt-assisted into the same jobs. Around that core argument sit TI's supporting advantages: 24 ePWM channels with the deepest PWM feature set in the industry, the CLA as both a jitter-free host for the control loop and a teachable example of real-time co-processing, the TMU accelerating exactly the trig that motor control uses, an NPU that opens an edge-ML chapter later, and the C2000 Academy curriculum written for precisely this kind of course. The H533RE counters with a faster scalar core, a second DAC channel, and a ~$12 lower price—real but modest advantages, none of which touch the book's core labs the way the encoder/PWM hardware does.

If the committee weighs unit cost heavily (a $12 difference across a whole cohort is real money) or values the Arm-ecosystem transferability of STM32 skills, the H533RE is a defensible pick and nothing in the labs would be blocked. But for a book whose identity is real-time control of mechanical systems, the C2000 is the part that was designed for the job.

---

## Remaining open questions for the group

- **Range:** ±5 V or ±3.3 V? Input stage is marginally simpler at ±3.3 V; the output stage is identical either way. Best decided from the labs' actual sensor/actuator interfaces.
- **Encoder count:** how many simultaneous encoders does the heaviest lab use? On-chip, TI handles 3 in dedicated hardware (6 on the F28P65x) and the H533 handles 2 on general-purpose timers. Note a separate constraint if the controlCARD form factor is adopted: the HSEC180 standard exposes only two complete eQEP interfaces, so a third encoder would mean muxing eQEP signals onto pins the standard assigns to other functions—possible, but a board-design tradeoff rather than a free configuration change, and constrained by which GPIOs the device's pin-mux table permits for each eQEP signal.
- **DAC channel count:** the labs are believed to use one analog output; if a second is ever firmly required, note that the H533 has two DAC channels while the F28P55X has one (a second TI channel would come from a ~$3 SPI DAC on the custom board).
- **Form factor:** LaunchPad + headers, or controlCARD + HSEC180 socket on the custom board? The latter costs ~5× more per unit but makes the MCU a swappable module and standardizes the mechanical interface across future projects.

---

## Sources

- [ST—product longevity program](https://www.st.com/content/st_com/en/about/quality-and-reliability/product-longevity.html)
- [ST—STM32H533RE product page (analog set, longevity from Jan 2026)](https://www.st.com/en/microcontrollers-microprocessors/stm32h533re.html)
- [ST—STM32H523/533 series page](https://www.st.com/en/microcontrollers-microprocessors/stm32h523-533.html)
- [Octopart—NUCLEO-H533RE price/stock](https://octopart.com/part/stmicroelectronics/NUCLEO-H533RE)
- [TI—TMS320F28P550SJ product page](https://www.ti.com/product/TMS320F28P550SJ)
- [TI—TMS320F28P55x datasheet (DACA_OUT on A0, 100-pin PZ pin 23)](https://www.ti.com/lit/ds/symlink/tms320f28p550sj.pdf)
- [TI—LAUNCHXL-F28P55X user guide SPRUJC0 (§2.1.3.8 PWM-DAC)](https://www.ti.com/lit/ug/sprujc0a/sprujc0a.pdf)
- [TI—LAUNCHXL-F28P55X Pinout Map (SPAZ056)](https://www.ti.com/lit/pdf/spaz056)
- [TI E2E—"PWM as DAC" (the debug technique, distinct from the buffered DAC)](https://e2e.ti.com/support/microcontrollers/c2000-microcontrollers-group/c2000/f/c2000-microcontrollers-forum/376436/pwm-as-dac)
- [ST community—index (Z) pulse in encoder mode requires EXTI workaround](https://community.st.com/t5/stm32-mcus-products/how-to-use-index-track-in-quadrature-encoder-mode/td-p/507598)
- [TI—C2000Ware SDK](https://www.ti.com/tool/C2000WARE)
- [TI—Digital Control Library user's guide (SPRUID3)](https://www.ti.com/lit/ug/spruid3/spruid3.pdf)
- [ST—STM32CubeH5 on GitHub (HAL/LL, CMSIS, ThreadX middleware)](https://github.com/STMicroelectronics/STM32CubeH5)
- [TI—LAUNCHXL-F28P55X](https://www.ti.com/tool/LAUNCHXL-F28P55X)
- [Octopart—LAUNCHXL-F28P55X price/stock](https://octopart.com/part/texas-instruments/LAUNCHXL-F28P55X)
- [TI—High-Voltage Signal Conditioning for Low-Voltage ADCs (SBOA097)](https://www.ti.com/lit/an/sboa097b/sboa097b.pdf)
- [TI E2E—bipolar voltage interface to ADC](https://e2e.ti.com/support/microcontrollers/c2000-microcontrollers-group/c2000/f/c2000-microcontrollers-forum/251100/bipolar-voltage-interface-to-adc)
- [NI myRIO-1900 User Guide (±10 V MSP-C baseline)](https://download.ni.com/support/manuals/376047c.pdf)
- [TI—F29H850TU product page (C29 preview status)](https://www.ti.com/product/F29H850TU)
- [TI—LAUNCHXL-F29H85X](https://www.ti.com/tool/LAUNCHXL-F29H85X)
- [TI—LP-AM263 LaunchPad (AM263x quad-R5F)](https://www.ti.com/tool/LP-AM263)
- [ST—STM32H563ZI product page](https://www.st.com/en/microcontrollers-microprocessors/stm32h563zi.html)
- [TI—TMDSCNCD28P65X controlCARD](https://www.ti.com/tool/TMDSCNCD28P65X)
- [TI—180-Pin ControlCARD Docking Station Information Guide (SPRUIJ6)](https://www.ti.com/lit/pdf/spruij6)
- [TI—TMDSCNCD28P65X controlCARD schematic (SPRR478)](https://www.ti.com/lit/pdf/sprr478)
- [TrustedParts—TMDSCNCD28P65X stock and risk ratings](https://www.trustedparts.com/en/part/texas-instruments/TMDSCNCD28P65X)
- C2000Ware `boards/ExperimenterKits/DockingStation_HSEC_120or180pin/revF/180_HSEC8_DV_pinout_Rev_F.pdf`—the authoritative HSEC180 controlCARD standard map (source of the 180-pin figures above; a parsed CSV of all 180 pins accompanies this document)