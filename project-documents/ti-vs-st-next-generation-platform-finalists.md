---
project_note_id: 1018
title: 'TI vs ST: Next-Generation Platform Finalists'
---

# TI vs ST: Next-Generation Platform Finalists

**Real-Time Computing for Mechanical Engineers, 2nd Edition**
Board comparison—internal—August 2026 (rev 11)

> **Revision note.** Rev 11 records a group decision that changes the weighting: **the book will use an RTOS and will host the control loop in a managed thread**, preserving the first edition's structure (which ran threads on real-time Linux). Hardware offload—the C2000 CLA, ST's DMA/FMAC paths—will be *taught as an option available on some devices* rather than used for the labs. That demotes what rev 10 called "the strongest hardware differentiator" to a sidebar, and promotes RTOS ecosystem quality to a first-order criterion. A new RTOS section has been added, the control-loop section rewritten around threads, and the recommendation reweighted—**the two finalists are now close enough that the decision turns on which cost the group would rather pay.** Earlier corrections still stand: the encoder-hardware error (rev 8), the controlCARD pinout and LaunchPad isolation overstatements and the op-amp error (rev 9), and the controlCARD price (rev 10).

---

## Context and bottom line

The first edition's platform, the NI myRIO-1900, is discontinued. An earlier memo made the case for the TI LaunchPad F28P55X; the group asked that an ST board be considered head-to-head, and added one requirement: at least one bipolar analog input and one bipolar analog output (±5 V sufficient, ±3.3 V acceptable).

Two finalists: the TI **LaunchPad F28P55X** (TMS320F28P550SJ) and the ST **Nucleo-H533RE** (STM32H533RE)—both 2024 silicon. Two other ST boards were evaluated and set aside: the Nucleo-G474RE (2019 silicon) and the Nucleo-H563ZI (see the up-market scan).

The analog front-end is essentially identical on either board. Encoder *hardware* is close to a wash. With the CLA out of the labs, the comparison now reduces to two asymmetric costs:

- **TI costs us development time.** ST ships no encoder example anywhere in its H5 package and no application note on encoder velocity, so the encoder labs would be built from the reference manual. TI ships a configuration GUI, two complete velocity examples, and a board-specific Academy lab.
- **ST costs us less of everything else.** Its FreeRTOS port is core-team maintained where TI's is a single-vendor fork of an abandoned community contribution; it has an MPU for task isolation where the C28x has none; it runs three different RTOSes where the C28x runs one and a half; it has no 16-bit-byte tax on third-party middleware; its toolchain is already native on Apple Silicon; and it is cheaper.

**With the RTOS decision made, the lean has narrowed to roughly even, with ST slightly ahead on everything except encoder lab-development effort.**

---

## Lifecycle: reading the dates correctly

**ST publishes a date, but it's a rolling floor, not a launch-anchored runway.** ST's "available until" dates are a **10-year commitment restarting from January 1 of the current year**. Every active STM32 currently shows January 2036 regardless of launch year—the H563 (March 2023) and the H533 (April 2024) carry the identical date. ST has historically kept renewing while a part stays in the longevity program, but the guarantee at any moment is ~10 years ahead.

**TI publishes no per-part date but has the stronger track record.** The F28P55x launched in 2024; TI's C2000 parts routinely stay in production 15–20 years (the 2007-era F2833x and 2016-era F28379D are both still ACTIVE). Expectation: production into the late 2030s.

**Conclusion:** with both finalists on 2024 silicon, expected longevity is comparable and excellent; TI's assurance is precedent-based, ST's is contractual-but-rolling.

---

## The finalists in brief

The **TI LaunchPad F28P55X** (~$35–36) carries the TMS320F28P550SJ: a 150 MHz C28x real-time core with an FPU, a small NPU, and two control-specific accelerators—the **CLA**, an independent 150 MHz floating-point co-processor, and the **TMU**, which adds hardware sin/cos/atan2/sqrt/divide *instructions*. Around these: five 12-bit ADCs (3.9 MSPS), one buffered 12-bit DAC, three dedicated eQEP encoder modules (two connectors pre-wired), 24 ePWM channels (12 with 150 ps high-resolution mode), and PGAs in front of the ADCs.

The **ST Nucleo-H533RE** (~$23–26) carries the STM32H533RE: a 250 MHz Cortex-M33F—375 DMIPS, notably quicker in scalar terms than the C28x—with an MPU and TrustZone. Two 12-bit ADCs at up to 5 MSPS, a **two-channel** buffered 12-bit DAC, quadrature-encoder mode with hardware index on six timers, an advanced motor-control timer, three USARTs plus UARTs, ample GPIO. Two absences corrected in rev 9: **no on-chip op-amp or comparator** (ST's web parametric table says otherwise; the datasheet, CMSIS headers, and pin database all agree it does not), and **no CORDIC and no FMAC**—those belong to the H56x/H57x parts.

---

## The bipolar front-end at ±5 V / ±3.3 V: a wash

Both MCUs have strictly unipolar 0–3.3 V ADCs and DACs, so bipolar I/O requires external conditioning on either board—and that conditioning is board-agnostic, because the one amplifier that must exist (the bipolar output stage) is external on both chips: a 3.3 V-railed on-chip op-amp can never swing below ground. Targeting ±5 V rather than the myRIO's ±10 V collapses the power problem (a ~$2 charge pump off the existing 5 V rail instead of a boost-plus-inverter design) and relaxes fault-protection sizing at the student-facing jacks. At ±3.3 V the input network degenerates to two equal resistors.

**Correction (rev 9): on-chip input buffering is a TI-only option.** TI can buffer the ADC input with its **PGAs**—on-chip amplifiers whose gain is software-selected from fixed internal steps, unity included. The H533 has nothing equivalent. In practice this changes little, since the design uses an external quad op-amp anyway as the sacrificial protection layer.

**Per-station front-end:** one quad op-amp, one charge pump, a half-dozen precision resistors—a ~$6 BOM, the same design either way. Full worked design in the companion document (*Bipolar Analog Front-End—Design Spec*).

---

## DAC architecture: both are true DACs

Neither board's DAC is a filtered PWM output, though TI's documentation invites the worry: the LaunchPad user guide (§2.1.3.8) describes "PWM DAC signals" on four BoosterPack pins with RC filters **not populated** by default. That is an optional debug facility, not the device's DAC.

- **F28P55X**—one true buffered 12-bit DAC ("DACA"), DACA_OUT on analog pin **A0**, physical pin 23 of the 100-pin PZ package, routed to the BoosterPack headers per SPAZ056. A0 is shared between DACA_OUT and ADC input A0, so that channel is one or the other.
- **H533RE**—one 12-bit DAC peripheral with **two output channels**, on PA4/PA5, exposed on the Nucleo headers.

---

## Encoder interfaces: hardware near-parity, software not close

Earlier revisions rested the recommendation on encoder hardware and were wrong to. Rebuilt from ST's register definitions, CMSIS headers, HAL sources, and pin database: **both parts decode quadrature in hardware, both reset the counter on index in hardware, both flag illegal quadrature transitions in hardware, and both can measure velocity by T or 1/T without external wiring.**

**Index (Z)—parity, arguably ST ahead.** The H533's `TIM_ECR` provides `IE` (counter reset on index) with `IDIR`, `IBLK`, `FIDX`, and `IPOS` qualifying by direction, blanking against a third input, taking only the first index, and selecting which A/B state the reset lands on. ST additionally raises `IERRF` when the index arrives inconsistent with the expected A/B state. The earlier claim that Z requires an EXTI interrupt was true of the F4/F7/H7 generation and false here.

**Integrity—parity, and neither part corrects.** eQEP flags phase errors and direction changes; the H533 raises `TERRF`, `DIRF`, `IDXF`, `IERRF`. Worth stating in the book: **no quadrature decoder in this class performs error *correction*.** Both detect and flag; recovery is software's job. Index-triggered re-initialization *bounds* accumulated error to one revolution rather than repairing a miscount. TI provides more independent detectors—phase error, a per-revolution count check, capture-validity flags, and a hardware **stall watchdog (QWDOG) with no ST equivalent**.

---

**Velocity—TI leads, but less than claimed.** The eQEP contains both methods in one peripheral: a unit timer for T, and an edge-capture unit with its own capture timer and a **1-to-2048** edge-stride prescaler for 1/T, latching position, capture period, and capture timer **atomically** on unit time-out.

The H533 can do 1/T entirely on-chip, but as a two-timer composition: the encoder timer emits an **encoder clock** on its trigger output (`CR2.MMS = 1000`, per AN4013, on all six encoder-capable timers), routed internally to a second timer's `ITRx`, which captures on `TRC`. Note why the single-timer approach fails: in encoder mode the counter *is* position, so capturing on its own channels latches position, not time.

**Resource accounting—corrected.** Six timers support quadrature encoder mode with hardware index (TIM1–TIM5, TIM8); only TIM2 and TIM5 are 32-bit; on LQFP64, TIM4 has no index pin, so five have a complete A/B/Z. LPTIM1/2 add a simpler index-less mode. The H533 has *more* encoder-capable timers than the F28P55x has eQEP modules; eQEP's advantage is that they are dedicated and don't consume timers wanted for PWM or capture.

**What ST does that eQEP does not:** nine decode modes including clock-plus-direction variants; the index conditioning above; the `IERRF` consistency check; preloadable slave-mode selection; encoder counting in Stop mode via LPTIM; and native DMA on encoder events, where eQEP is not a DMA trigger source.

---

## Encoder usability: the one place TI clearly wins

With the hardware roughly even, the software is the story.

**First count—a tie.** ST is fewer keystrokes (CubeMX generates the init; the student writes `HAL_TIM_Encoder_Start()` plus a counter read). TI wins the friction that bites: a **dedicated QEP header** with SysConfig assigning pins from board data, and a 32-bit QPOSCNT with hardware `QPOSMAX` wrap so there is no rollover code. On the H533 the student must discover that only TIM2 and TIM5 are 32-bit.

**Index/homing—TI by a wide margin.** In SysConfig it is a dropdown, and TI's C2000 Academy eQEP lab *for the F28P55X LaunchPad specifically* already sets it. On ST the API is decent (`HAL_TIMEx_ConfigEncoderIndex`, seven-field struct) but three frictions stack: the index arrives on **ETR**, which creates a pin-routing trap (a documented case had a pin serving as both CH1 and ETR on the same alternate function, producing spurious index interrupts); **no HAL start function enables the index interrupt**, so the student must add `__HAL_TIM_ENABLE_IT(&htim, TIM_IT_IDX)` after reading the header to learn it exists; and there is no evidence CubeMX exposes index configuration at all.

**Velocity—the largest gap in the comparison.** TI ships `eqep_ex1_freq_cal` and `eqep_ex2_pos_speed`: both compute T and 1/T side by side, both come with design spreadsheets for prescaler and scaling derivation, and both synthesize the encoder signals from ePWM so **they run with three jumper wires and no motor**. SysConfig even shows a computed capture-timer frequency while you pick prescalers. On the ST side, **STM32CubeH5 contains zero files with "encoder" in the path**, the NUCLEO-H533RE has no TIM examples at all, AN4776 explicitly scopes out motor control, and AN4013 covers encoder modes with no velocity guidance. X-CUBE-MCSDK's encoder component is bound to a Workbench-generated FOC project requiring rotor alignment after every reset. Building 1/T on the H533 means hand-writing `CR2.MMS`, `SMCR.TS`, `CCMR1.CC1S`, and `CCER.CC1E` with no wrapper and no example.

**Hazard worth flagging in the book:** third-party STM32 encoder tutorials are abundant and nearly all target pre-H5 families, teaching the obsolete EXTI-index workaround. A student following them never learns the index hardware exists.

---

## Maximum count rate: neither MCU is the limit

For a 512-line encoder at 4× decode, 2048 counts/rev.

| | TI F28P55x | ST STM32H533 |
|---|---|---|
| Raw ceiling | ~150 Mcounts/s (1 count/SYSCLK) | ~250 Mcounts/s (250 MHz timer clock) |
| Equivalent RPM, 512-line | ~4.4 million | ~7.3 million |
| At max input filter | **~3,445 RPM** (GPIO qualifier, /510) | ~14,300 RPM |
| Datasheet spec | `t_w(QEPP) ≥ 2 SYSCLK` | none published |

At 3,000 RPM you need 102,400 counts/s—roughly 1,500–2,400× under the raw ceiling. **The number that can bite is the input filter**: TI's GPIO qualifier at maximum divider requires a ~17 µs stable pulse and caps around 3,445 RPM, inside the range of an ordinary lab motor. That trade—noise immunity bought with bandwidth—is real engineering content and a good lab exercise on either platform.

Two limits bind first. A typical 512-line optical encoder is rated 100–300 kHz per channel (3,000–17,500 RPM equivalent). And long unshielded single-ended cables beside motor PWM inject false edges long before anything runs out of speed—directional in quadrature, so the count walks off monotonically rather than jittering. The cure is a differential (RS-422) encoder, not a faster MCU. Neither part detects a *swallowed* edge; the error flags catch illegal transitions, not missing ones.

---

## RTOS support

The group has decided the book will use an RTOS and manage threads, as the first edition did on real-time Linux. That makes this section a first-order criterion rather than a checkbox.

**Both parts run FreeRTOS, both at kernel V11.2.0, and neither vendor ships it where you would look.** TI's is *not* in C2000Ware core—it lives in a sibling repository, `c2000ware-FreeRTOS`, which does include F28P55x demos. ST's is *not* in STM32CubeH5, whose only vendored RTOS is ThreadX—FreeRTOS arrives through a separate official pack, **X-CUBE-FREERTOS**, installed via CubeMX's Package Manager. Rev 10 described this asymmetrically ("TI ships it directly, ST's arrives from outside the vendor package"); that was wrong in both halves and is corrected here.

**The substantive difference is the port's standing.** FreeRTOS classifies ports into three tiers. The **Cortex-M33 port is core-team maintained**—reviewed, tested, LTS-eligible, with TrustZone and non-TrustZone variants and `mpu_wrappers_v2_asm.c` for FreeRTOS-MPU. The **C28x port is community-tier**, the lowest: contributed by an individual, functionally unchanged since September 2022, never core-team reviewed, never LTS-eligible. TI forked it, relicensed it MIT, and maintains its copy on a per-release cadence—but those changes never flow upstream, and there is **no port-validation project for the F28P55x** (only the F2838x has one). A signal worth noting: TI *did* invest in FreeRTOS's tier-2 partner process—for the **C29x**, not the C28x.

---

**What each side provides.** TI has a SysConfig **graphical** configurator for tasks, queues, semaphores, timers, and heap scheme, plus ROV kernel-aware debugging (CCS Theia only). But the F28P55x demos are LED-blinky and ROV—no queue or semaphore example on this exact part; the richer suite and port-validation projects are wired only to the F2838x. ST's four H5 FreeRTOS applications are richer—mutex, **MPU**, TrustZone queues, low-power semaphore, all with STM32CubeIDE projects—but target the H563ZI, so the H533RE version is regenerated rather than opened.

**Beyond FreeRTOS, ST has options and TI does not.**

| | TI F28P55x | ST H533RE |
|---|---|---|
| FreeRTOS | V11.2.0, TI fork, **community-tier port** | V11.2.0, **core-team port**, LTS-eligible |
| Second option | µC/OS-II (C28x port exists, commercial support); **TI-RTOS/SYS-BIOS is dead** | ThreadX + NetX/FileX/LevelX/USBX, apps shipped for this board |
| Third option | none | **Zephyr**, `nucleo_h533re` in-tree, Apache-2.0 |
| Task isolation | **none**—no MPU, no privilege levels | MPU + TrustZone; FreeRTOS-MPU and ThreadX Modules both available |

TI's own page states TI-RTOS is "for legacy devices and support is not available"; the last C2000 TI-RTOS document is from 2015. **Zephyr cannot run on C28x at all**—there is no C28x architecture in the tree, and this is structural rather than a matter of effort (see the 16-bit byte issue below).

**A licensing note on ThreadX.** ST ships Microsoft-licensed ThreadX 6.4.0, not the Eclipse MIT-licensed 6.5.0 upstream. The STM32H5 series *is* on Microsoft's licensed-hardware list, so coursework and student projects on this board are covered. But it is a hardware-conditional proprietary license, unlike FreeRTOS (MIT) or Zephyr (Apache-2.0)—a student who learns it here and moves to a non-ST part inherits a question they would not otherwise have.

---

### Two C28x architecture facts that matter more now

**No memory protection.** The C28x has **no MPU and no privilege levels**. What it has is master-based, block-granular protection—which bus master (CPU, DMA, CLA, NPU) may access which RAM block—configured once at init. Useful, but a different thing: task isolation is not achievable, and consistent with that, **FreeRTOS-MPU does not exist for C28x**. The Cortex-M33 has an MPU plus TrustZone, and ST's `FreeRTOS_MPU` example is close to a ready-made lecture: privileged and unprivileged tasks, `xTaskCreateRestricted()`, unprivileged tasks attempting illegal writes, MemManage faults caught, reported over serial, and recovered from. For a book that now teaches thread management, that is a directly relevant capability the TI part cannot demonstrate.

**Sixteen-bit bytes.** On the C28x, `CHAR_BIT == 16`. There are no 8-bit types (`uint8_t` is remapped to 16 bits), `sizeof` returns words rather than octets, `#pragma pack(1)` is unsupported so a struct cannot be laid over a wire-format packet, arithmetic does not wrap at 0xFF, and—separately—**`double` is only 32 bits**, so numerical code assuming double precision silently loses it. TI documents the consequences in its own migration note, SPRAD88, including a worked example where a struct occupying 32 bits on Arm occupies 48 on C28x.

The practical consequence: lwIP, FreeRTOS-Plus-TCP, FatFs, littlefs, mbedTLS, and essentially any byte-slinging library will not compile-and-run unmodified. The FreeRTOS *kernel* is unaffected because it manipulates words and stacks rather than octet streams—which is exactly why the kernel port exists and the middleware does not. **How much this matters depends on the labs.** The current requirement list (UART logging, keypad, character display) touches none of it. If the book ever wants a filesystem, a network stack, or a wire protocol, it binds hard. It is also a genuinely good teaching topic in its own right—"portable C" as a claim that has to be checked, with SPRAD88 as assigned reading.

Two C28x quirks are teaching gems rather than problems: the stack **grows up** (`portSTACK_GROWTH = 1`) and `BaseType_t` is 16-bit. Both are one-paragraph illustrations that portability assumptions are assumptions.

---

## Control-loop timing: a thread, as before

The first edition ran the control loop as a thread under real-time Linux. The group has decided to keep that structure on the new platform, which makes the port conceptually clean: **the loop remains a thread; only the scheduler underneath it changes.**

**The canonical pattern.** A hardware timer (or an ePWM/ADC end-of-conversion) fires an ISR that does essentially nothing except signal a high-priority thread—`vTaskNotifyGiveFromISR()` or `xSemaphoreGiveFromISR()` with a context switch on exit. The thread wakes, reads the sample, evaluates the difference equation, writes the actuator, and blocks again. This is structurally identical to a real-time Linux thread waiting on a timerfd or a `clock_nanosleep()` deadline, so the first edition's material transfers with a change of API rather than a change of concept—and the quantitative story improves by orders of magnitude: PREEMPT_RT wakeup jitter on an application processor is typically tens of microseconds, where an MCU RTOS is sub-microsecond to low single-digit microseconds.

**What the book now has to teach, which it should.** Jitter no longer comes from an OS with virtual memory and a general-purpose scheduler; it comes from identifiable, measurable sources: tick resolution and whether the loop is tick-driven or interrupt-driven, ISR-to-thread wakeup latency, priority assignment, critical sections and interrupt masking in other code, and preemption by higher-priority threads. Every one of those is instrumentable with a GPIO toggle and a scope. That is a better real-time computing lesson than the first edition could deliver, because on an MCU the student can actually see and account for the whole latency chain.

**The two platforms are close here.** Both offer sub-microsecond interrupt latency and the same FreeRTOS API. The C28x has slightly lower raw interrupt latency in cycles; the M33 runs at a higher clock. The differences are within the noise of how the loop is written. Where they diverge is everything *around* the loop: port maturity, MPU-based isolation, and the middleware portability discussed above.

**Sidebar material—hardware offload.** The book will teach that some devices can take the loop off the CPU entirely, without relying on it for the labs. The C2000 **CLA** is the strongest example: an independent 150 MHz floating-point processor with its own bus, register set, pipeline, and memories, launched directly by an ADC end-of-conversion or PWM event with no CPU involvement and no dependence on the scheduler. (One precision note so a sharp student does not catch us: CLA/CPU contention for a shared RAM block is hardware-arbitrated and bounded but not literally zero, so "architecturally decoupled from scheduling" is accurate where "completely unaffected" overstates it.) ST's analogue is timer-triggered ADC into DMA, which removes the CPU from the steady-state data path but has no place to put the control law itself; on the H56x/H57x parts the FMAC can evaluate an IIR compensator, though it has no hardware trigger and cannot branch, so anti-windup, saturation, and mode logic return to the CPU. **The pedagogical point survives on either board**: hardware offload is what you reach for when thread-level determinism is not enough, and the CLA is the cleanest illustration in the industry—worth a figure and a discussion regardless of which part the labs use.

---

## C library and driver support

**TI: C2000Ware.** One SDK with driverlib, bit-field register headers, SysConfig code generation, device examples, and math libraries. Driverlib is thin, readable, one-function-per-operation wrappers over registers (`EQEP_setPosition(...)`)—the right abstraction level for this course, since students can read the driver source and see the register write. Two inclusions matter: the **Digital Control Library (DCL)**—PID/PI/2p2z/3p3z compensators with anti-windup and saturation handling, i.e. the algorithms the book teaches, as documented reference code—and **IQmath** for the fixed-point material. C2000 Academy teaches against driverlib, so course and vendor materials align.

**ST: STM32CubeH5.** HAL plus LL (register-close, the better teaching layer), CMSIS core and CMSIS-DSP (FIR/IIR/matrix/PID), BSPs, and middleware. Caveats: the Cube H5 examples target the **H563ZI, H503RB, and H573I-DK—not the H533RE**; the vendored RTOS is ThreadX with FreeRTOS in a separate pack; and there is **no encoder example and no encoder-velocity application note anywhere**.

**Verdict:** TI's package is better aligned to the control content (DCL mirrors the syllabus, examples target this board, encoder velocity is documented). ST's is better aligned to the systems content (mature RTOS ports, MPU examples, portable middleware). With the RTOS decision made, these now roughly offset.

---

## Does Arm matter for us?

Less than it looks, but more than rev 10 allowed. The myRIO's ARM was a Cortex-A9 **application processor** running Linux; the H533's Cortex-M33 is a **microcontroller core**—no MMU, no Linux. They share a brand name and little a student touches, so there is no direct continuity from edition 1.

What *does* transfer is the ecosystem consequence, and the RTOS decision brings it into scope: third-party tools and RTOSes support Arm first. That is why ST has three RTOS options and the C28x has one and a half, why Zephyr exists on one and not the other, and why ST reached native Apple Silicon first. Arm is not a tiebreaker on its own, but it is the reason several of ST's advantages exist.

---

## Requirements comparison

| Requirement | TI LaunchPad F28P55X | ST Nucleo-H533RE |
|---|---|---|
| MCU, launch | F28P550SJ, **2024** | STM32H533RE, **2024** |
| Core / clock | 150 MHz C28x + CLA, FPU, TMU, NPU | 250 MHz Cortex-M33F (375 DMIPS) |
| Math acceleration | **TMU: trig as instructions, few cycles** | FPU + DSP only—no CORDIC/FMAC on this part |
| Bipolar AI / AO | Resistor network (+PGA buffer) / external op-amp | Resistor network (no on-chip op-amp) / external op-amp |
| On-chip ADC | 5× 12-bit, 3.9 MSPS, 39 ch | 2× 12-bit, 5 MSPS |
| On-chip DAC | 1× 12-bit buffered | 1× 12-bit, **2 channels** |
| Encoder decode + HW index | 3× eQEP, dedicated | **6 timers**, 5 with index pin; 2 are 32-bit |
| Encoder **software** | **SysConfig GUI; 2 velocity examples w/ spreadsheets; Academy lab for this board** | **No encoder example in CubeH5; no velocity app note** |
| Encoder stall watchdog | **Yes (QWDOG)** | No equivalent |
| **FreeRTOS port tier** | Community—unreviewed, no LTS, no validation project for this part | **Core team—reviewed, tested, LTS-eligible** |
| **Other RTOS options** | µC/OS-II; TI-RTOS **dead**; **Zephyr impossible** | **ThreadX in-box (apps for this board) + Zephyr in-tree** |
| **Task isolation / MPU** | **None**—no MPU, no privilege levels | **MPU + TrustZone; FreeRTOS-MPU demoed** |
| **Third-party middleware** | **16-bit byte breaks lwIP/FatFs/mbedTLS etc.** | Standard 8-bit byte |
| Control-loop thread latency | Sub-µs interrupts; slightly lower cycle latency | Sub-µs interrupts; higher clock |
| Hardware offload (taught, not used) | **CLA**—cleanest example in the industry | DMA path; FMAC only on H56x/H57x |
| PWM | **24 ePWM (12 high-res, 150 ps)** | Advanced MC timer + GP timers; no HRTIM on any H5 |
| Debug on board | XDS110; isolator **defeated by default jumpers**; controlCARD inherently isolated | ST-LINK/V3EC, not isolated, not detachable |
| Form factor | BoosterPack ×2; **controlCARD edge card into our carrier** | Arduino Uno V3 + morpho; no card-edge module exists |
| macOS on Apple Silicon | x86 only; native promised by end of 2026 | **Native arm64** since Feb 2026 |
| Board price | ~$35–36; ~$79 controlCARD | **~$23–26** |
| Lifecycle | 2024; TI precedent → late 2030s | 2024; rolling 10-yr floor (01/2036) |

---

## Development environment—near-parity, with one live exception

Both vendors offer free, modern, cross-platform toolchains: ST with STM32CubeIDE plus a mature VS Code extension, TI with the Theia-based CCS, which has VS Code's look and extension model, one-cable flash/debug via the on-board XDS110, and real-time variable watch/graphing that is particularly good in control labs. On features, a wash.

**One axis is not equivalent: Apple Silicon.** macOS 26 retains Rosetta 2; macOS 27 (Fall 2026) runs only on M1-or-newer and *removes* Rosetta 2 during installation, though it can be reinstalled; macOS 28 (Fall 2027) is the hard stop. CCS 21.0.0 (June 15, 2026) ships a single macOS installer, `CCS_21.0.0.00014_mac_x86.dmg`; the 20.4.0 release notes state the Theia component is Arm-compiled but "the backend components are still x86 based, so Rosetta is still required." In an E2E thread opened **July 2026**, a TI engineer answered: "Yes. Work is already in progress for this. A version will be available before the end of the year, if not sooner"—a deadline still open. ST has been native since end of February 2026. Real ST advantage today; residual risk is schedule slip rather than direction. Watch item below.

---

## Form factor and debug isolation

**The economics, corrected in rev 10.** Earlier revisions priced the controlCARD at $190–260; that is the **F28P65X** card. The **F28P55X card is ~$79 direct from TI** (~$94–99 at distributors). Because the carrier board hosts the HSEC socket, **no docking station is needed**—the controlCARD is the entire MCU purchase, with a dock wanted only for prototyping before the carrier exists. So the comparison is ~$79 per replaceable module versus ~$26 for a whole Nucleo, about 3×. At that spread the controlCARD's inherently isolated debug probe, polarized edge connector, and swappable-module property look like good value. Caveat: authorized stock is thin (single digits at DigiKey and Arrow, ~29 at Mouser), so order a cohort at once, preferably direct from TI.

**Pinout is a convention, not a guarantee.** SPRUIJ6 says the dock supports cards conforming to the connector *footprint*—not pinout, and "several," not "all." Stable across cards: power, JTAG on 1–8, reset, ePWM ordinals, eQEP1 on 100/102/106. **Not** stable: the analog map. Pin 21 is ADC-A4 on the F28379D, ADC-A5 on the F280039C, and ADC-A3/B3/C5 on the F28P550SJ. This also resolves an earlier open question: **the P55x has no DAC-B on pin 11, and the P65x puts DAC-C on pin 14.**

**A physical-length ambiguity to settle before cutting copper.** TI's product page calls the TMDSCNCD28P55X a "Standard 180-pin controlCARD HSEC interface," but SPRUJA7 describes it as a "120-pin HSEC8 Edge Card Interface" and its C2000Ware pinout file is named `..._120cCARD_pinout`. The F280049C has the same contradiction, and TI confirmed on E2E that one is 120-pin. **Verify against a physical card.** Hedge: design to pins 1–120, mirror TI's two-connector arrangement, don't hard-assign analog pins in course material. Note also that TI has broken this once per generation—DIMM100 → HSEC, and now the F29H85x abandons controlCARD entirely for a Samtec-connector "controlSOM."

**Debug isolation.** SPRUJC0 is explicit that on the LaunchPad "by default, both shunts are populated... meaning that the USB is NOT isolated." Pull both JP1 shunts for real galvanic isolation, but the board then needs 5 V from the carrier—free to provide. Only the **controlCARD's** isolation is inherent. The Nucleo has none and cannot be separated from its ST-LINK.

**Is the grounding issue real? Mostly not, at these voltages.** 12–24 V is SELV: no shock hazard, no hazardous path to a laptop. The informative data point is that ST sells the X-NUCLEO-IHM07M1, an **8–48 V** three-phase motor shield designed to stack on a non-isolated Nucleo and be debugged over its non-isolated ST-LINK, and UM1943 carries no isolation or ground-loop warning at all. The genuine benefit is debug-link robustness and ADC noise immunity—the symptom is "the debugger lost the target mid-demo," not smoke. Two free mitigations beat any purchase: **specify a floating bench supply** and **have students run on battery during motor labs**. An earthed scope ground clip is a far more effective ground-loop generator than any USB cable. Purchasing gotcha: common ADuM3160 USB isolator dongles are full-speed only; both XDS110 and ST-LINK/V3EC are high-speed.

**ST has no card-edge module, for any STM32 family.** The "SOM-STM32xx" pages are Emcraft partner pages for castellated solder-down modules. STMod+ carries no MCU. MikroElektronika's "MCU CARD for STM32" is a proprietary 168-pin mezzanine with no debugger and no H5 card. ST's model is the Nucleo as fixed base with shields on top—which is what X-NUCLEO-IHM16M1 does, encoder connector included. If ST is chosen, consider terminating the carrier at **ST's 34-pin motor-control connector** (UM1970) rather than the morpho headers, since the morpho pinout is not guaranteed consistent across Nucleo-64 boards either.

---

## Would spending more buy anything? (up-market scan)

**TI, one tier up—LaunchPad F28P65X (~$66; controlCARD ~$200–209).** Dual 200 MHz C28x cores, 16-bit ADC mode, two buffered DACs, six eQEP, more memory. Still exactly **one CLA**, shared across both cores; what you gain as a co-processor is a second full C28x core. Loses the NPU. Toolchain and book code carry over with a device-target change—a natural senior-design upgrade path.

**TI, bleeding edge—LaunchPad F29H85X (~$80–100, restricted).** **PREVIEW** silicon, restricted distribution, new SDK rather than mature C2000Ware, no CLA, **no buffered DAC listed**, no transfer of the C28x educational corpus, and it abandons the controlCARD form factor. Wrong platform for a textbook anchor. Note one forward-looking signal: FreeRTOS's **C29x** port is partner-supported (tier 2), where C28x is community-tier.

**TI, Arm industrial—LP-AM263 (>$100).** Quad Cortex-R5F with C2000-style control peripherals and industrial Ethernet. Buys compute and connectivity the labs don't need, at the cost of multicore boot complexity that would hurt teaching.

---

**ST, sideways—Nucleo-H563ZI: evaluated in rev 9 and rejected.** The H563 *does* have the **CORDIC and FMAC** the H533 lacks, its LQFP144 gives all six encoder timers a complete A/B/Z, it has 4× flash and 2.4× RAM, ST's Cube H5 examples target it, and it carries the **identical 01/2036 longevity date** despite being 13 months older. But: distributor pricing is **$39–44**, making it *more expensive than the $35 TI LaunchPad*; Ethernet occupies **PA1**, blocking **both 32-bit encoder timers** in the stock configuration, and PA4 is VBUS_SENSE, blocking TIM5_ETR and DAC1_OUT1, so the conflict-free encoder-with-index options are TIM3 or TIM4, both 16-bit; CORDIC is a memory-mapped peripheral rather than an instruction, so it runs **3–5× slower per trig call than the TMU** despite the faster clock, and has no divide; and FMAC has no hardware trigger, is q1.15 only, and cannot branch. Crucially, the one gap it would cure it does not: there are still **zero encoder examples anywhere in CubeH5**.

One point in its favor now worth recording: **all four of ST's H5 FreeRTOS example applications target the H563ZI**, including the MPU one. If the RTOS material becomes central enough that having vendor-supplied example projects matters more than the encoder pin conflicts, this deserves a second look. **Current decision: keep the Nucleo-H533RE as the ST finalist.**

**ST, the road not taken—STM32G474.** ST's closest analogue to a C2000: HRTIM, CORDIC, FMAC, five ADCs, four DACs, on-chip op-amps and comparators, seven encoder-capable timers with hardware index, and the target of ST's own digital-control app notes. Disqualified on age—2019 silicon, 170 MHz Cortex-M4F—for a book with a 10+ year shelf life, but the group should know why it was set aside.

**ST, up-market—H7 / H7R-S / MP-series.** More money buys clock speed, displays, and eventually Linux—never better control peripherals and never a card-edge form factor. No STM32H5 has HRTIM. The H7 dual-core parts have neither CORDIC nor FMAC.

---

## Recommendation

**The RTOS decision has narrowed this to roughly even, with ST slightly ahead.** Rev 10 leaned TI on two pillars. One of them—the CLA as a jitter-free host for the control loop—has been removed from the labs by group decision and is now sidebar material, which it can be on either board. The other—encoder tooling—stands and is the strongest remaining TI argument.

**What TI still wins:** encoder lab development. SysConfig configures every eQEP feature including index and capture; two complete velocity examples with design spreadsheets run on jumper wires; a C2000 Academy lab targets this exact board. ST has none of this and no encoder-velocity application note at all. TI also wins on PWM depth, the TMU, DCL matching the syllabus, and a card-edge form factor with inherently isolated debug that ST cannot match at any price.

**What ST wins, and it is now a longer list:** a core-team-maintained, LTS-eligible FreeRTOS port versus a single-vendor fork of an abandoned community contribution with no validation project for this device; an MPU and TrustZone for genuine task isolation, with a ready-made FreeRTOS-MPU demonstration, where the C28x has no memory protection at all; three RTOS options versus one and a half, including Zephyr, which the C28x architecturally cannot run; standard 8-bit bytes, where the C28x's 16-bit byte breaks essentially all third-party middleware; a native Apple Silicon toolchain today; a second DAC channel; and a cheaper board.

**The decision reduces to which cost the group prefers to pay.** Choosing ST means building the encoder labs from the reference manual—real work, landing on us, probably the single largest identified development cost in the project. Choosing TI means teaching thread management on a platform with no memory protection, a community-tier kernel port, and an architecture that forecloses most third-party middleware and Zephyr entirely—a cost that lands on the book's systems content and on students who later reuse the skills.

**Given that the book is titled *Real-Time Computing* and now centers on RTOS thread management, the systems-side costs are closer to the core of the book than the encoder-lab costs are.** On that reasoning ST is the marginally better fit, and the encoder-lab effort is a known, bounded, one-time expense we can scope. But this is close enough that either choice is defensible, and if the group weighs lab-development capacity heavily, TI remains a good answer.

### If ST is chosen: which RTOS?

**Recommend FreeRTOS for the book, with the other two as sidebars.**

- **FreeRTOS**—MIT-licensed, core-team Cortex-M33 port, LTS-eligible, and the smallest conceptual surface that still covers everything the book teaches: tasks, queues, semaphores, mutexes with priority inheritance, software timers, and direct-to-task notifications. Richard Barry's official kernel books are free PDFs, which is a real asset for a textbook. It is also what students are most likely to meet in industry. Teach the **native FreeRTOS API** rather than the CMSIS-RTOS2 wrapper—the wrapper adds a layer of indirection that obscures exactly what the book wants visible—while mentioning that the wrapper exists for portability.
- **ThreadX**—technically strong and the only one shipped in-box for the H533RE, with a genuinely interesting scheduling feature (preemption-threshold) that is worth a paragraph. Set aside as the primary because ST ships it under a hardware-conditional Microsoft license rather than the Eclipse MIT upstream, and because the student-facing learning corpus is much thinner.
- **Zephyr**—the most future-proof and the closest in spirit to the real-time Linux the first edition used, with `nucleo_h533re` supported in-tree under Apache-2.0. Set aside as the primary because it is an operating system *plus* a build system: west, devicetree, and Kconfig are a large conceptual overhead for a mechanical-engineering audience, and its driver abstraction actively hides the register-level view this book wants students to have. It is an excellent final chapter—"here is where this goes next"—and an excellent option for senior design.

---

## Remaining open questions for the group

- **Board:** TI F28P55X or ST H533RE? The decision now turns on encoder-lab development effort (favors TI) versus RTOS/systems quality and portability (favors ST).
- **RTOS:** FreeRTOS recommended; ThreadX and Zephyr as sidebars. Decide also whether to teach the native FreeRTOS API or CMSIS-RTOS2 (recommend native).
- **Control-loop thread structure:** confirm the timer-ISR-signals-high-priority-thread pattern and decide whether the loop is tick-driven or interrupt-driven. The jitter measurement lab should follow directly from this choice.
- **Range:** ±5 V or ±3.3 V? Decide from the labs' actual sensor/actuator interfaces.
- **Encoder count and type:** how many simultaneous encoders in the heaviest lab, and single-ended or differential? At lab speeds signal integrity binds, not bandwidth. Set the input filter deliberately—TI's maximum qualifier caps at ~3,445 RPM with a 512-line encoder.
- **DAC channel count:** one analog output is believed sufficient; the H533 has two channels, the F28P55X one.
- **Form factor:** LaunchPad, controlCARD in a carrier socket, or Nucleo-as-shield. At the corrected ~$79 the controlCARD is a much easier case than earlier revisions implied.
- **Debug isolation:** controlCARD is inherently isolated; if the LaunchPad is chosen, removing both JP1 shunts and supplying 5 V from the carrier gets the same benefit for ~$0 and should be the default.
- **Middleware scope:** does any planned lab need a filesystem, a network stack, or a wire protocol? If yes, the C28x 16-bit byte becomes a hard constraint rather than a teaching topic.
- **WATCH—native arm64 CCS.** TI's July 2026 commitment is a native build "before the end of the year." Checkpoints: each CCS release, check for an arm64 DMG; **January 2027**, escalate if TI's own deadline passes; **before press**, document a Rosetta-reinstall procedure if still x86-only.
- **To verify physically:** whether the TMDSCNCD28P55X is a 120- or 180-finger card, and whether the F28P65x's DACA/DACC land where SPRR478 suggests.

---

## Sources

**TI—device and boards**
- [TMS320F28P550SJ](https://www.ti.com/product/TMS320F28P550SJ) · [F28P55x datasheet](https://www.ti.com/lit/ds/symlink/tms320f28p550sj.pdf) · [LAUNCHXL-F28P55X](https://www.ti.com/tool/LAUNCHXL-F28P55X) · [user guide SPRUJC0](https://www.ti.com/lit/ug/sprujc0a/sprujc0a.pdf) · [Pinout map SPAZ056](https://www.ti.com/lit/pdf/spaz056) · [DigiKey](https://www.digikey.com/en/products/detail/texas-instruments/LAUNCHXL-F28P55X/22674628)
- [TMDSCNCD28P55X](https://www.ti.com/tool/TMDSCNCD28P55X) · [Octopart price/stock](https://octopart.com/part/texas-instruments/TMDSCNCD28P55X) · [TMDSCNCD28P65X](https://www.ti.com/tool/TMDSCNCD28P65X) · [Octopart price/stock](https://octopart.com/part/texas-instruments/TMDSCNCD28P65X) · [SPRUIJ6 docking station](https://www.ti.com/lit/pdf/spruij6) · [SPRR478 schematic](https://www.ti.com/lit/pdf/sprr478)
- [C2000Ware](https://www.ti.com/tool/C2000WARE) · [DCL SPRUID3](https://www.ti.com/lit/ug/spruid3/spruid3.pdf) · [eQEP driverlib API](https://software-dl.ti.com/C2000/docs/C2000_driverlib_api_guide/f28p55x/build/html/modules/eqep.html) · [c2000ware-core-sdk](https://github.com/TexasInstruments/c2000ware-core-sdk) · [c2000ware-c2000-academy (F28P55x eQEP lab)](https://github.com/TexasInstruments/c2000ware-c2000-academy) · [eQEP guide SPRUFK8](https://engineering.purdue.edu/~dionysis/EE452/Lab10/eQEP_User_Guide_sprufk8.pdf) · [SPRABX2 QMA](https://www.ti.com/lit/sprabx2) · [SPRU514 TMU intrinsics](https://downloads.ti.com/docs/esd/SPRU514/trigonometric-math-unit-tmu-intrinsics-t365164-6.html)
- HSEC standard map—parsed from C2000Ware `boards/ExperimenterKits/DockingStation_HSEC_120or180pin/revF/180_HSEC8_DV_pinout_Rev_F.pdf`; full table in project files as `HSEC180_controlCARD_standard_pinout.csv`
- [F29H850TU](https://www.ti.com/product/F29H850TU) · [LAUNCHXL-F29H85X](https://www.ti.com/tool/LAUNCHXL-F29H85X) · [LP-AM263](https://www.ti.com/tool/LP-AM263)

**RTOS (rev 11)**
- [FreeRTOS-Kernel `portable/`](https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/main/portable)—**no C28x port upstream**; `ARM_CM33` and `ARM_CM33_NTZ` present with secure/non-secure variants and `mpu_wrappers_v2_asm.c`.
- [FreeRTOS Community-Supported-Ports `CCS/C2000_C28x`](https://github.com/FreeRTOS/FreeRTOS-Kernel-Community-Supported-Ports/tree/main/CCS/C2000_C28x)—the C28x port; three commits ever, functionally unchanged since Sept 2022.
- [FreeRTOS Partner-Supported-Ports](https://github.com/FreeRTOS/FreeRTOS-Kernel-Partner-Supported-Ports)—contains **C2000_C29x_F29H85x**, not C28x. Tier definitions in `portable/ThirdParty/README.md`.
- [c2000ware-FreeRTOS](https://github.com/TexasInstruments/c2000ware-FreeRTOS)—TI's fork; F28P55x demos at `kernel/FreeRTOS/Demo/C2000_F28P55x_C28x_CCS/`; kernel V11.2.0; SysConfig FreeRTOS tool at `kernel/.meta/freertos_tool/`; ROV at `kernel/FreeRTOS/rov/`.
- [TI-RTOS](https://www.ti.com/tool/TI-RTOS)—"for legacy devices and support is not available."
- [X-CUBE-FREERTOS](https://github.com/STMicroelectronics/x-cube-freertos) and [product page](https://www.st.com/en/embedded-software/x-cube-freertos.html)—FreeRTOS for STM32H5; four H563ZI applications including `FreeRTOS_MPU`. Kernel via [stm32-mw-freertos](https://github.com/STMicroelectronics/stm32-mw-freertos), V11.2.0 with CMSIS-RTOS2 layer.
- [STM32CubeH5 `Middlewares/`](https://github.com/STMicroelectronics/STM32CubeH5/tree/main/Middlewares)—ThreadX/NetXDuo/FileX/LevelX/USBX vendored; **no FreeRTOS**.
- [stm32-mw-threadx](https://github.com/STMicroelectronics/stm32-mw-threadx)—ThreadX 6.4.0 under Microsoft Azure RTOS license; STM32H5 on `LICENSED-HARDWARE.txt`. Upstream [eclipse-threadx/threadx](https://github.com/eclipse-threadx/threadx) is MIT at v6.5.0.
- [Zephyr `arch/`](https://github.com/zephyrproject-rtos/zephyr/tree/main/arch)—no C28x architecture. [Zephyr `boards/st/nucleo_h533re`](https://github.com/zephyrproject-rtos/zephyr/tree/main/boards/st/nucleo_h533re)—in-tree board support.
- [µC/OS-II C28x port](https://github.com/weston-embedded/uC-OS2/tree/develop/Ports) · [SEGGER embOS ports overview](https://www.segger.com/products/rtos/embos/supported-cores-compiler/embos-ports-overview/)—no C28x entry.
- [SPRU514 data types](https://downloads.ti.com/docs/esd/SPRU514Q/data-types-stdz0555922.html)—`char` is 16 bits, `double` is 32 bits. [SPRAD88](https://www.ti.com/lit/pdf/sprad88)—consequences for porting byte-oriented code.
- [CLA Software Development Guide](https://software-dl.ti.com/C2000/docs/cla_software_dev_guide/intro.html)—independent bus/registers/pipeline, direct peripheral triggering, hardware-arbitrated shared-memory access.

**ST—device, encoder, form factor**
- [STM32H533RE](https://www.st.com/en/microcontrollers-microprocessors/stm32h533re.html) · [NUCLEO-H533RE](https://www.st.com/en/evaluation-tools/nucleo-h533re.html) · [DS14539](https://www.st.com/resource/en/datasheet/stm32h533re.pdf) · [UM3121 Nucleo-64 MB1814](https://www.st.com/resource/en/user_manual/um3121-stm32h5-nucleo64-board-mb1814-stmicroelectronics.pdf) · [Octopart price/stock](https://octopart.com/part/stmicroelectronics/NUCLEO-H533RE)
- [STM32H563ZI](https://www.st.com/en/microcontrollers-microprocessors/stm32h563zi.html) · [NUCLEO-H563ZI](https://www.st.com/en/evaluation-tools/nucleo-h563zi.html) · [DS14258](https://www.st.com/resource/en/datasheet/stm32h563ri.pdf) · [UM3115 Nucleo-144 MB1404](https://www.st.com/resource/en/user_manual/um3115-stm32h5-nucleo144-board-mb1404-stmicroelectronics.pdf) · [Octopart price/stock](https://octopart.com/part/stmicroelectronics/NUCLEO-H563ZI)
- [STM32H533 TIM2 ECR](https://docs.rs/stm32h5/latest/stm32h5/stm32h533/tim2/ecr/index.html) · [TIM status flags](https://docs.rs/stm32h5/latest/stm32h5/stm32h503/tim2/sr/index.html) · [stm32h533xx.h](https://raw.githubusercontent.com/STMicroelectronics/cmsis_device_h5/main/Include/stm32h533xx.h) · [STM32H533RETx pin data](https://raw.githubusercontent.com/STMicroelectronics/STM32_open_pin_data/master/mcu/STM32H533RETx.xml) · [stm32h5xx_hal_driver](https://github.com/STMicroelectronics/stm32h5xx_hal_driver) · [STM32CubeH5](https://github.com/STMicroelectronics/STM32CubeH5)
- [AN4013 timer overview](https://www.st.com/resource/en/application_note/an4013-stm32-crossseries-timer-overview-stmicroelectronics.pdf) · [AN4776 timer cookbook](https://www.st.com/resource/en/application_note/an4776-generalpurpose-timer-cookbook-for-stm32-microcontrollers-stmicroelectronics.pdf) · [AN5325 CORDIC](https://www.st.com/resource/en/application_note/an5325-getting-started-with-the-cordic-coprocessor-stmicroelectronics.pdf) · [AN5305 FMAC](https://www.st.com/resource/en/application_note/an5305-digital-filter-implementation-with-the-fmac-using-stm32cubeg4-mcu-package-stmicroelectronics.pdf) · [AN5464 MCSDK position control](https://www.st.com/resource/en/application_note/an5464-position-control-of-a-threephase-permanent-magnet-motor-using-xcubemcsdk-or-xcubemcsdkful-stmicroelectronics.pdf)
- [ST community—encoder-clock TRGO chain](https://community.st.com/t5/stm32-mcus-embedded-software/tim-encoder-mode-with-encoder-clock-output/td-p/790944) · [ST community—encoder index ETR pin conflict](https://community.st.com/t5/stm32-mcus-embedded-software/stm32g474ve-tim2-encoder-index/td-p/726269) · [ST community—index in encoder mode, **older families only**](https://community.st.com/t5/stm32-mcus-products/how-to-use-index-track-in-quadrature-encoder-mode/td-p/507598)—**source of the rev 1–7 error**, retained as a caution.
- [X-NUCLEO-IHM16M1](https://www.st.com/en/evaluation-tools/x-nucleo-ihm16m1.html) · [X-NUCLEO-IHM07M1](https://www.st.com/en/evaluation-tools/x-nucleo-ihm07m1.html) · [UM1970 34-pin MC connector](https://www.st.com/resource/en/user_manual/um1970-getting-started-with-the-xnucleoihm09m1-motor-control-connector-expansion-board-for-stm32-nucleo-stmicroelectronics.pdf) · [TN1238 STMod+](https://www.st.com/resource/en/technical_note/tn1238-stmod-interface-specification-stmicroelectronics.pdf) · [B-STLINK-ISOL](https://www.st.com/en/development-tools/b-stlink-isol.html) · [MikroE MCU CARD for STM32](https://www.st.com/en/partner-products-and-services/mcu-card-for-stm32.html)
- [ST longevity program](https://www.st.com/content/st_com/en/about/quality-and-reliability/product-longevity.html) · [ST press release 7 Mar 2023](https://newsroom.st.com/media-center/press-item.html/p4519.html) · [ST blog 3 Apr 2024](https://blog.st.com/stm32h5/)

**Toolchain / Apple Silicon**
- [TI E2E—arm64 CCS commitment, July 2026](https://e2e.ti.com/support/processors-group/processors/f/processors-forum/1655979/ccstudio-is-ccs-going-to-run-on-apple-silicon-without-depending-on-rosetta) · [CCS 20.4.0 release notes](https://software-dl.ti.com/ccs/esd/CCSv20/CCS_20_4_0/exports/CCS_20.4.0_ReleaseNote.htm) · [CCS 21.0.0 download](https://www.ti.com/tool/download/CCSTUDIO/21.0.0) · [ST community—ST tools on macOS](https://community.st.com/stm32cubeide-for-visual-studio-code-mcus-133/the-future-of-st-tools-on-macos-in-2027-forward-deprecation-of-rosetta-2-163538) · [AppleInsider—macOS Intel-app timeline](https://appleinsider.com/articles/26/06/12/how-and-when-macos-will-finally-stop-support-for-intel-apps)

**Method note.** Rev 8–11 corrections were verified against vendor datasheets, SVD-derived register definitions, CMSIS device headers, HAL and kernel sources, SDK and RTOS repository file trees, ST's pin database, and distributor pricing—not against community posts. The rev 1–7 encoder error came from trusting a forum thread about a different silicon generation; the rev 1–9 price error came from applying one card's price to a different card. Claims that could not be verified from a primary source appear in the open questions rather than in the body.