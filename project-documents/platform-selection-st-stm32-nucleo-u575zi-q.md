---
project_note_id: 1018
title: 'Platform Selection: ST STM32, Nucleo-U575ZI-Q'
---

# Platform Selection: ST STM32, Nucleo-U575ZI-Q

**Real-Time Computing for Mechanical Engineers, 2nd Edition**
Board comparison and decision record—internal—August 2026 (rev 13)

> **Revision note.** Rev 11 recorded the group's decision to use an RTOS with managed threads and to teach hardware offload as an option rather than use it in the labs. Rev 12 recorded the consequences: **ST over TI** on RTOS grounds, and a board change from the Nucleo-H533RE (whose DAC output pin PA4 is not routed to any header) to the **Nucleo-U575ZI-Q**. **Rev 13** records that the U575ZI-Q has now been checked systematically against the full accumulated requirements list rather than on RTOS and pin availability alone, that the Nucleo-H563ZI was re-examined with cost removed as a factor, and that the detailed pin map and real-time design rules now live in a companion document. The TI comparison is retained below as the decision record.

---

## Decision summary

**Platform: ST STM32. Board: Nucleo-U575ZI-Q (~$27).** RTOS: FreeRTOS recommended, with ThreadX and Zephyr as sidebar topics.

The decisive criterion was RTOS support, once the group decided the book would use an RTOS and manage threads as the first edition did on real-time Linux. On that axis TI's C2000 is materially weaker: its FreeRTOS port is a single-vendor fork of an abandoned community contribution, the C28x has no MPU or privilege levels so task isolation is impossible, Zephyr cannot run on it at all, and its 16-bit byte forecloses most third-party middleware.

Within ST, the board choice changed twice as the criteria sharpened. The H533RE failed a hard requirement. The H563ZI was re-examined with cost removed and still lost, on grounds that turn out to matter more for a mechatronics course than clock speed does.

**Companion documents:** *Board Configuration and Lab Resource Allocation* (pin map, peripheral budget, real-time design rules) and *Bipolar Analog Front-End—Design Spec* (the conditioning circuit).

---

## Part I—Why ST over TI

### What the comparison came down to

Both candidate MCUs cleared every functional requirement. Three rounds of verification removed most of the apparent differentiators:

- **The analog front-end is board-agnostic.** Bipolar I/O needs an external op-amp stage on any 3.3 V part, because a rail-to-rail amplifier on a 3.3 V supply cannot swing below ground. A ~$6 BOM, the same design either way.
- **Encoder hardware is near-parity.** Rev 8 corrected a significant error: earlier revisions claimed the STM32 lacked hardware index handling and quadrature error detection. It does not. `TIM_ECR` provides index-triggered counter reset with direction qualification, blanking, first-index-only, and A/B-state positioning; `TERRF`/`IERRF`/`IDXF`/`DIRF` provide error and event flags. Both families detect errors; **neither corrects them**—index re-initialization bounds accumulated error to one revolution rather than repairing a miscount.
- **The CLA was decisive until it wasn't.** The C2000's Control Law Accelerator is an independent 150 MHz floating-point processor that hosts a control loop with hardware triggering and no scheduler dependence. It has no ST equivalent at any price. The group's decision to run the loop in an RTOS thread moved this from the deciding factor to sidebar material—which it can be on either platform, and should be, because it is the cleanest illustration of hardware/software co-design in the industry.

### What decided it: RTOS and systems quality

| | TI C2000 F28P55x | ST STM32 |
|---|---|---|
| FreeRTOS port tier | **Community**—unreviewed, never LTS, no validation project for this device, functionally frozen since Sept 2022 | **Core team**—reviewed, tested, LTS-eligible, TrustZone and non-TrustZone variants |
| Task isolation | **None**—no MPU, no privilege levels | MPU + TrustZone; FreeRTOS-MPU and ThreadX Modules both available |
| Other RTOS options | µC/OS-II; TI-RTOS is dead ("support is not available") | ThreadX in-box; **Zephyr** in-tree |
| Zephyr | **Architecturally impossible**—no C28x arch, and the 16-bit byte precludes one | Supported |
| Third-party middleware | **16-bit byte** breaks lwIP, FatFs, littlefs, mbedTLS, any byte-slinging library | Standard 8-bit byte |
| macOS on Apple Silicon | x86 only; native promised by end of 2026 | **Native arm64** since Feb 2026 |

Two C28x architecture facts carried disproportionate weight once thread management became central. It has **no memory protection**—only master-based, block-granular rules about which bus master may reach which RAM block, which cannot isolate tasks. And `CHAR_BIT == 16`: there are no 8-bit types, `sizeof` returns words rather than octets, `#pragma pack(1)` is unsupported so a struct cannot overlay a wire-format packet, arithmetic does not wrap at 0xFF, and `double` is only 32 bits. TI documents the consequences in its own migration note SPRAD88. The FreeRTOS *kernel* is unaffected because it manipulates words and stacks; the middleware ecosystem is not.

### What we gave up by not choosing TI

Honesty requires recording this. TI's encoder tooling is better and it is the one place the choice costs us real work:

- **SysConfig** configures every eQEP feature—decoder mode, index, latch, capture unit with a live prescaler calculation, watchdog, and interrupt sources—as a GUI, generating all driverlib calls.
- **Two complete velocity examples** (`eqep_ex1_freq_cal`, `eqep_ex2_pos_speed`) compute T-method and 1/T side by side, ship with design spreadsheets, and synthesize encoder signals from ePWM so they run on three jumper wires with no motor.
- **A C2000 Academy lab** targets the F28P55X LaunchPad specifically.
- A hardware **stall watchdog (QWDOG)** with no ST equivalent, 24 ePWM channels with 150 ps high-resolution mode, the **TMU** (trig as instructions rather than a memory-mapped peripheral), the DCL control library mirroring the syllabus, and a **controlCARD card-edge form factor** at ~$79 with inherently isolated debug that ST cannot match at any price.

**Mitigation, and it matters:** STM32CubeG4 ships `TIM_Encoder` and `TIM_EncoderIndex_PulseOnCompare` example projects—the only ST family with encoder examples of any kind. Because the TIM peripheral with `ECR` is common across G4, H5 and U5, **those examples should be usable as reference material on the U575ZI-Q**. Verifying this early is the highest-value item on the list, because it directly reduces the largest identified lab-development cost.

---

## Part II—Choosing the ST board

### The support matrix

Enumerated from ST's own repositories: the X-CUBE-FREERTOS `Projects/` tree, STM32Cube family packages, and Zephyr's in-tree board directories.

| Board | Vendor FreeRTOS apps | ThreadX apps | Zephyr board | Cube examples incl. DAC/TIM? | Price |
|---|---|---|---|---|---|
| **NUCLEO-U575ZI-Q** | **4** | **7 (most of any ST board)** | yes, DAC/ADC/PWM enabled | **yes**—CORDIC, DAC, FMAC, TIM, OPAMP | **~$27** |
| NUCLEO-H563ZI | 4 | 5 | yes | yes | ~$43 |
| NUCLEO-H533RE | **0** | 2 | stub—no DAC or I2C node | **no DAC, no TIM** | ~$24 |
| NUCLEO-G474RE | 0 (series excluded from pack; 3 legacy in-package apps) | **0** | yes | **yes, incl. encoder + encoder-index** | ~$20 |

X-CUBE-FREERTOS covers exactly nine boards across STM32U5, H5, WBA, C0, U0, U3 and N6. **The H533RE is not among them, and neither is any H5 board except the H563ZI. STM32G4 is excluded as a series.**

### Why the H533RE is out

Beyond the zero vendor FreeRTOS applications and the stub Zephyr port, it has a hard I/O defect: **DAC1_OUT1 (PA4) is not routed to any header.** ST's own Zephyr morpho-connector map for this board lists PA0, PA1 and PA5 but contains no PA4 entry, and UM3121's connector table agrees. The only reachable DAC output is **DAC1_OUT2 on PA5, which is the user LED LD2**—usable only by removing solder bridge SB6, or by accepting an LED and series resistor as a nonlinear load on the DAC buffer. For a course whose requirement is one bipolar analog output, on boards students may buy themselves, requiring a soldering-iron modification on every unit is disqualifying. Secondary marks: no CORDIC, no FMAC, no DAC or TIM examples in STM32CubeH5 for this board, and a 26-week manufacturer lead time.

### Why the U575ZI-Q wins

**RTOS ecosystem—the decisive criterion.** Four vendor FreeRTOS applications target this exact board (Mutex, **MPU**, Queues/ThreadFlags with TrustZone, Semaphore with tickless idle), plus **seven ThreadX applications, more than any other ST board**. Its Zephyr board support is real rather than nominal, with DAC, ADC and PWM nodes enabled.

**I/O headroom—the best of any candidate.** The U5 has no Ethernet MAC, so the RMII pin block that constrains the H563ZI is free, and the virtual COM port sits on PA9/PA10 rather than PA2/PA3. **All six encoder-capable timers provide a complete A/B/Z at the headers, and both DAC channels are free**, with no solder-bridge work. VBUS_SENSE is on PC2, not PA4—the opposite of the H563 board. 92 of 110 header GPIO are free, and 64 remain after a full lab allocation.

**Silicon.** 160 MHz Cortex-M33 with TrustZone and MPU; CORDIC and FMAC; **a 14-bit ADC at the full 2.5 MSPS with no sample-rate penalty**; two buffered DAC channels; and **two on-chip op-amps with PGA plus two comparators**. 2 MB flash, 786 KB SRAM. Longevity to 01/2036.

**What it costs.** 160 MHz instead of 250—immaterial, quantified below. A low-power family orientation, surfacing mainly as one tickless-idle example that is a legitimate teaching topic. Nucleo-144 rather than Nucleo-64, which affects the carrier layout.

### Verified against the full requirements list

Rev 12 selected this board on RTOS ecosystem, encoder and DAC pin availability, price and silicon generation. Rev 13 checked it against everything else the requirements list has accumulated, using ST's pin database, the board's own CubeMX default configuration, UM2861's morpho table, and ST's Zephyr devicetree, cross-validated four ways. **It clears every requirement with margin.** Highlights:

- **PWM for the motor amplifier.** TIM1 and TIM8 are advanced-control timers with complementary outputs, dead-time and two break inputs each. With TIM2 and TIM3 taken by encoders, **TIM1 remains completely unobstructed—a full three-phase complementary set plus both breaks on one contiguous port (PE8–PE15), all free.** 14 effective bits of duty resolution at 10 kHz. Scales from an H-bridge lab to a three-phase inverter lab without re-pinning.
- **Serial.** Six USART/UART instances; the VCP takes USART1, leaving five free at the headers, plus three SPI and four I²C.
- **Analog.** 21 ADC-capable pins free; DAC settling 2.05 µs, timer-triggered and DMA-fed.
- **A concrete, machine-verified pin allocation exists**—two encoders with index, TIM1 PWM with break, DAC out, ADC in, a second UART, I²C, a 4×3 keypad and a 4-bit character LCD—using 28 pins with 64 free afterwards and no conflicts.

**One open item and two cautions.** Per-pin **5 V tolerance** could not be closed from machine-readable sources and needs DS13737 Tables 25/26 read for the specific encoder and keypad pins; buffer 5 V encoder lines regardless. **OPAMP1's inputs are PA0/PA1**, which collide with encoder 1 in the reference allocation—use OPAMP2 or move the encoder. **TIM8_CH2 exists only on PC7 (LD1)**, so TIM8 cannot do a full three-phase set on this board; use TIM1.

Details, the full pin table, and the real-time design rules are in the companion document.

### Compute headroom for the two-thread labs

The labs run a high-priority control thread and a lower-priority UI thread (keypad, LCD, UART) simultaneously. **The workload is not CPU-bound.** For a 10 kHz loop at 160 MHz: ADC conversion 527 ns (DMA-fed, overlapped), the 2p2z difference equation plus I/O roughly 0.4–0.6 µs, and task notification plus context switch roughly 1.5–2.8 µs—about **2–3.5% of the processor**, leaving over 90% headroom. At 1 kHz it is under 0.4%.

Two findings worth carrying into the book. First, **the scheduling overhead exceeds the control arithmetic**, which makes "implement the loop as a task, measure it, then implement it in the ISR and measure again" a natural lab. Second, **everything that can go wrong here is a latency problem, not a throughput problem**—the risk is a UI thread that masks interrupts or suspends the scheduler, not one that is merely slow. The companion document works through the taxonomy and the zero-latency-interrupt rule that makes the control loop immune to UI-thread mistakes.

(Note that Arm publishes no instruction cycle counts or interrupt-latency figure for the Cortex-M33, so those numbers are engineering estimates with stated reasoning rather than citations. `DWT->CYCCNT` is available, and measuring is better pedagogy than a table.)

### The H563ZI, re-examined with cost removed

Asked again with price set aside, the answer holds—and the reason is not the one that looked obvious.

**The U575 wins on on-chip analog, decisively.** The H563 has **zero op-amps and zero comparators**; the U575 has two op-amps with built-in PGA and two comparators. For a mechatronics course that is directly on topic: a programmable-gain sensor front end with no external parts, a quantization-versus-noise experiment run by changing gain in software, and a hardware overcurrent trip wired comparator-to-TIM1-break entirely on chip. On the H563 every one of those needs a breadboard, which at class scale means BOM, wiring errors, and TA hours. The U575 also has the 14-bit ADC, both DAC channels free versus one (PA4 is VBUS_SENSE on the H563 board), 92 versus 87 free GPIO, 21 versus 12 free ADC pins, more SRAM, and richer Cube coverage.

**250 MHz is not a reason.** At 10 kHz the choice is between 96.5% idle and 97.8% idle, and the H563's advantage is partly eaten by its extra flash wait state (5 versus 4).

**Ethernet is the one real reason, and it is a clean flip condition.** The H563ZI has a populated PHY and RJ45; **the U575 has no Ethernet MAC in silicon**, which no board-level workaround recovers. Networked telemetry, remote monitoring, and especially measuring network-induced delay and jitter in a closed loop are first-class real-time computing topics. ST ships four Ethernet applications on the H563ZI, including UDP echo client and server—nearly a distributed-control lab already written. Caveat: those are **NetX Duo, not lwIP**; lwIP is not bundled in CubeH5 and CubeMX does not support it for H5, so it needs manual integration. Ethernet also permanently consumes nine pins whether used or not.

**Recommendation:** standardize on the U575ZI-Q and buy a small set of H563ZI boards if a networking module materializes. Both are Nucleo-144, share the morpho footprint, differ by about five GPIO positions out of 110, and run the same HAL on the same core—porting a lab is a pin-map edit, which also makes "port this driver to a different MCU" a realistic assignment.

Other conditions that would flip the choice: a control loop above roughly 50–100 kHz, a need for genuinely simultaneous dual-channel sampling (the H563's two matched 12-bit ADCs do this; the U575's ADC1/ADC4 pair is asymmetric), or a lab needing two independent CAN buses on one board.

### Runners-up, recorded so they aren't relitigated

**NUCLEO-G474RE (~$20)** is the cheapest and most available, and the **only** ST board with vendor encoder examples—including an index example. It stays out on 2019 silicon and ST's exclusion of the whole G4 series from the modern FreeRTOS pack. Its encoder examples remain useful as reference material.

**NUCLEO-U385RG-Q (~$24)** is the only Nucleo-64 with all four modern FreeRTOS applications, but at 96 MHz with no CORDIC, no FMAC and four encoder timers, it is oriented at ultra-low-power rather than control.

**Newly discovered ST sub-families**, for a future edition: **H543/H553** are essentially "H533 plus CORDIC" at 1 MB flash, and **H5E4/H5E5/H5F4/H5F5** reach 3–4 MB flash with 1.5 MB RAM. All are pre-distribution with no FreeRTOS pack support.

---

## Part III—Design consequences

### The bipolar front-end

Unchanged in substance: bipolar I/O requires external conditioning on any 3.3 V part, because the output stage must swing below ground. Roughly one quad op-amp, one charge pump and a half-dozen precision resistors—a ~$6 BOM. Targeting ±5 V rather than the myRIO's ±10 V keeps the power design to a ~$2 charge pump off the existing 5 V rail and relaxes fault-protection sizing at the student-facing jacks.

**A correction propagates from the board change.** Rev 9 stated that on-chip ADC input buffering was a TI-only option, because the H533 has neither an op-amp nor a comparator. That was correct for the H533 and is **not correct for the U575ZI-Q**, which has two of each. The design does not depend on it—an external quad op-amp remains the right choice because it is the sacrificial protection layer for the MCU's analog pins—but the U5 offers an on-chip follower or PGA stage if a future revision wants one, and its 14-bit ADC gives the input path better resolution than the design assumed.

### Encoders

Six encoder-capable timers with hardware index via `TIM_ECR`; TIM2 and TIM5 are 32-bit. All six reach a complete A/B/Z at the headers. The index arrives on the timer's **ETR pin**, so Z must use an ETR pin distinct from CH1/CH2.

For velocity, the 1/T method is built from the encoder timer's **encoder-clock trigger output** (`CR2.MMS = 1000`) routed internally to a second timer's `ITRx` and captured on `TRC`—entirely on-chip, but a two-timer composition rather than the single-peripheral capture unit the eQEP provides. The single-timer approach fails because in encoder mode the counter *is* position, so capturing on its own channels latches position rather than time.

**Count rate is not a constraint.** For a 512-line encoder at 4× decode, 3,000 RPM produces 102,400 counts/s against a ceiling in the tens of millions. What binds first is the encoder's own rating (100–300 kHz per channel) and, well before that, signal integrity on unshielded single-ended cable beside motor PWM. False edges in quadrature are *directional*, so the count walks off monotonically rather than jittering. The cure is a differential (RS-422) encoder. Setting the input filter deliberately—trading noise immunity for bandwidth—is a good lab exercise.

### RTOS

**FreeRTOS is recommended.** MIT-licensed, core-team Cortex-M33 port, LTS-eligible, and the smallest conceptual surface covering tasks, queues, semaphores, mutexes with priority inheritance, timers and notifications. Richard Barry's official kernel books are free PDFs. It arrives through the **X-CUBE-FREERTOS** pack via CubeMX's Package Manager rather than in the base Cube package—worth telling students explicitly. **Teach the native FreeRTOS API rather than the CMSIS-RTOS2 wrapper**, which adds indirection over exactly the mechanisms the book wants visible.

**ThreadX** ships in-box with seven applications for this board and has an interesting scheduling feature—preemption-threshold—worth a sidebar. Not primary, because ST ships it under a hardware-conditional Microsoft license rather than the Eclipse MIT upstream.

**Zephyr** is the most future-proof and closest in spirit to the real-time Linux the first edition used, in-tree under Apache-2.0. Not primary, because it is an operating system *plus* a build system, and its driver abstraction hides the register-level view the book wants. An excellent closing chapter.

### The control loop as a thread

A hardware timer fires an ISR that does essentially nothing except signal a high-priority thread; the thread wakes, reads the sample, evaluates the difference equation, writes the actuator, and blocks. Structurally identical to a thread waiting on a timerfd under real-time Linux, so the first edition's material transfers with a change of API rather than of concept—and the numbers improve by orders of magnitude, since PREEMPT_RT wakeup jitter is typically tens of microseconds where an MCU RTOS is sub-microsecond to low single digits.

What the book now teaches is better than the first edition could deliver: jitter comes from identifiable, measurable sources—tick resolution, ISR-to-thread wakeup latency, priority assignment, critical sections elsewhere in the system, preemption—and every one is instrumentable with a GPIO toggle and a scope.

**Hardware offload remains sidebar material.** The C2000 CLA is the strongest example and deserves a figure. On the U575ZI-Q the analogues are timer-triggered ADC into DMA for acquisition, and the **FMAC** for compensator arithmetic—which can host a 3p3z IIR but has no hardware trigger, is q1.15 fixed-point only, and cannot branch, so anti-windup, saturation, mode switching and fault response all return to the CPU. That contrast is itself the lesson: an accelerator replaces the arithmetic, a co-processor replaces the loop.

---

## Open questions and next steps

- **Confirm the G4 encoder examples port to the U5.** Highest-value item; directly reduces the largest lab-development cost.
- **Order two or three Nucleo-U575ZI-Q boards** and work the verification checklist in the companion document—PA4/PA5 free for DAC, PA0 unencumbered, 5 V tolerance for the encoder and keypad pins, encoder index behavior, and the CPU-budget measurement.
- **Range:** ±5 V or ±3.3 V for the front end? Decide from the labs' actual sensor and actuator interfaces; the U5's 14-bit ADC gives more headroom than the design assumed.
- **RTOS API:** native FreeRTOS or CMSIS-RTOS2 (recommend native). Confirm the control-loop thread structure and whether the loop is tick-driven or interrupt-driven, since the jitter lab follows from that choice.
- **Encoder count and type:** how many simultaneous encoders, and single-ended or differential?
- **Networking:** is a networked-telemetry or distributed-control chapter in scope? This is the one condition that would flip the board to the H563ZI, and it cannot be recovered later without a board change.
- **Carrier form factor:** Nucleo-144, larger than the Nucleo-64 earlier analysis assumed. ST has no card-edge module for any STM32 family, so the carrier mates to the Zio and morpho headers; consider terminating at ST's published 34-pin motor-control connector (UM1970) rather than directly at the morpho pinout, which is not guaranteed consistent across boards.
- **Debug isolation:** the ST-LINK/V3EC is not isolated and not detachable. At 12–24 V this is SELV with no shock hazard, and ST ships 8–48 V motor shields for non-isolated Nucleos with no isolation warning. The real risk is debug-link robustness and ADC noise. Two free mitigations: specify a **floating** bench supply, and have students run on battery during motor labs.

---

## Sources

**Chosen board and family**
- [NUCLEO-U575ZI-Q](https://www.st.com/en/evaluation-tools/nucleo-u575zi-q.html) · [UM2861 (MB1549)](https://www.st.com/resource/en/user_manual/um2861-stm32u5-nucleo144-board-mb1549-stmicroelectronics.pdf) · [DS13737 (STM32U575xx)](https://www.st.com/resource/en/datasheet/stm32u575zi.pdf) · [Octopart price/stock](https://octopart.com/part/stmicroelectronics/NUCLEO-U575ZI-Q)
- [Zephyr `boards/st/nucleo_u575zi_q`](https://github.com/zephyrproject-rtos/zephyr/tree/main/boards/st/nucleo_u575zi_q)—ST-authored devicetree: `&dac1` on `dac1_out1_pa4`, LEDs on PC7/PB7/PG2, USART2 on PD5/PD6.
- [STM32_open_pin_data](https://github.com/STMicroelectronics/STM32_open_pin_data) · [STM32CubeU5](https://github.com/STMicroelectronics/STM32CubeU5) · `stm32u5xx_hal_adc.h` (14-bit is ADC1/ADC2 only) · `stm32u5xx_hal_tim_ex.h` (encoder index via ETR)

**RTOS**
- [X-CUBE-FREERTOS](https://github.com/STMicroelectronics/x-cube-freertos)—nine boards, 34 applications; four for NUCLEO-U575ZI-Q; none for NUCLEO-H533RE; STM32G4 unsupported as a series. Kernel via [stm32-mw-freertos](https://github.com/STMicroelectronics/stm32-mw-freertos), V11.2.0.
- [FreeRTOS-Kernel `portable/`](https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/main/portable)—core-team `ARM_CM33` ports; **no C28x**. [Community-Supported-Ports `CCS/C2000_C28x`](https://github.com/FreeRTOS/FreeRTOS-Kernel-Community-Supported-Ports/tree/main/CCS/C2000_C28x)—unchanged since Sept 2022.
- [stm32-mw-threadx](https://github.com/STMicroelectronics/stm32-mw-threadx) · [eclipse-threadx/threadx](https://github.com/eclipse-threadx/threadx) · [Zephyr `arch/`](https://github.com/zephyrproject-rtos/zephyr/tree/main/arch)—no C28x architecture.
- [TI-RTOS](https://www.ti.com/tool/TI-RTOS)—"for legacy devices and support is not available." [c2000ware-FreeRTOS](https://github.com/TexasInstruments/c2000ware-FreeRTOS)

**Boards evaluated and set aside**
- [NUCLEO-H533RE](https://www.st.com/en/evaluation-tools/nucleo-h533re.html) · [UM3121 (MB1814)](https://www.st.com/resource/en/user_manual/um3121-stm32h5-nucleo64-board-mb1814-stmicroelectronics.pdf) · [Zephyr `nucleo_h533re/st_morpho_connector.dtsi`](https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/st/nucleo_h533re/st_morpho_connector.dtsi)—**PA4 absent from the morpho map**.
- [NUCLEO-H563ZI](https://www.st.com/en/evaluation-tools/nucleo-h563zi.html) · [UM3115 (MB1404)](https://www.st.com/resource/en/user_manual/um3115-stm32h5-nucleo144-board-mb1404-stmicroelectronics.pdf) · [DS14258](https://www.st.com/resource/en/datasheet/stm32h563ri.pdf) · [ST: lwIP on STM32H5 requires manual integration](https://community.st.com/t5/stm32-mcus/how-to-use-the-lwip-ethernet-middleware-on-the-stm32h5-series/ta-p/691100)
- [NUCLEO-G474RE](https://www.st.com/en/evaluation-tools/nucleo-g474re.html) · [STM32CubeG4](https://github.com/STMicroelectronics/STM32CubeG4)—`TIM_Encoder` and `TIM_EncoderIndex_PulseOnCompare`, the only ST encoder examples in any family.

**TI comparison (decision record)**
- [TMS320F28P550SJ](https://www.ti.com/product/TMS320F28P550SJ) · [LAUNCHXL-F28P55X](https://www.ti.com/tool/LAUNCHXL-F28P55X) · [TMDSCNCD28P55X](https://www.ti.com/tool/TMDSCNCD28P55X) · [C2000Ware](https://www.ti.com/tool/C2000WARE) · [c2000ware-c2000-academy](https://github.com/TexasInstruments/c2000ware-c2000-academy) · [DCL SPRUID3](https://www.ti.com/lit/ug/spruid3/spruid3.pdf)
- [SPRU514 data types](https://downloads.ti.com/docs/esd/SPRU514Q/data-types-stdz0555922.html) · [SPRAD88](https://www.ti.com/lit/pdf/sprad88) · [CLA Software Development Guide](https://software-dl.ti.com/C2000/docs/cla_software_dev_guide/intro.html)
- [TI E2E—arm64 CCS commitment, July 2026](https://e2e.ti.com/support/processors-group/processors/f/processors-forum/1655979/ccstudio-is-ccs-going-to-run-on-apple-silicon-without-depending-on-rosetta)

**Method note.** Corrections across revisions 8–13 were verified against vendor datasheets, SVD-derived register definitions, CMSIS device headers, HAL and kernel sources, SDK and RTOS repository file trees, ST's pin database and board devicetrees, board user manuals, and distributor pricing—not against community posts. Five errors were caught and corrected in the process: the encoder-hardware claim (rev 8, from a forum thread about a different silicon generation), the controlCARD pinout guarantee and LaunchPad isolation overstatements and the on-chip op-amp claim (rev 9), the controlCARD price (rev 10, one card's price applied to another), the ST board selection (rev 12, an unrouted DAC pin on the previous finalist), and the scope of the evaluation itself (rev 13, a board selected on a subset of criteria before being checked against all of them). Claims that could not be verified from a primary source appear in the open questions rather than in the body.