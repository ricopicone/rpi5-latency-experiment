---
project_note_id: 1018
title: 'TI vs ST: Next-Generation Platform Finalists'
---

# Platform Selection: ST STM32, Nucleo-U575ZI-Q

**Real-Time Computing for Mechanical Engineers, 2nd Edition**
Board comparison and decision record—internal—August 2026 (rev 12)

> **Revision note.** Rev 11 recorded the group's decision to use an RTOS with managed threads and to teach hardware offload (the C2000 CLA) as an option rather than use it in the labs. **Rev 12 records two consequences.** First, the group has concluded that RTOS ecosystem quality is decisive, which selects **ST over TI**. Second, a survey of ST's current lineup finds that the previous ST finalist, the Nucleo-H533RE, is the wrong model—and disqualifying, not merely suboptimal: **its DAC output pin PA4 is not routed to any header**, so the board cannot provide the bipolar analog output the labs require without a solder modification. The recommended board is now the **Nucleo-U575ZI-Q (STM32U575ZI-Q)**. The TI comparison is retained below as the decision record.

---

## Decision summary

**Platform: ST STM32. Board: Nucleo-U575ZI-Q (~$27).** RTOS: FreeRTOS recommended, with ThreadX and Zephyr as sidebar topics.

The decisive criterion was RTOS support, once the group decided the book would use an RTOS and manage threads as the first edition did on real-time Linux. On that axis TI's C2000 is materially weaker: its FreeRTOS port is a single-vendor fork of an abandoned community contribution, the C28x has no MPU or privilege levels so task isolation is impossible, Zephyr cannot run on it at all, and its 16-bit byte forecloses most third-party middleware.

Within ST, the board choice changed because the criteria changed. When RTOS ecosystem quality became the top criterion, the H533RE turned out to be the only STM32H5 Nucleo *excluded* from ST's FreeRTOS example pack while carrying a hard I/O defect that had gone unnoticed.

---

## Part I—Why ST over TI

### What the comparison came down to

Both candidate MCUs cleared every functional requirement. Three rounds of verification removed most of the apparent differentiators:

- **The analog front-end is board-agnostic.** Bipolar I/O needs an external op-amp stage on any 3.3 V part, because a rail-to-rail amplifier on a 3.3 V supply cannot swing below ground. A ~$6 BOM, the same design either way. (See the companion *Bipolar Analog Front-End—Design Spec*.)
- **Encoder hardware is near-parity.** Rev 8 corrected a significant error here: earlier revisions claimed the STM32 lacked hardware index handling and quadrature error detection. It does not. `TIM_ECR` provides index-triggered counter reset with direction qualification, blanking, first-index-only, and A/B-state positioning; `TERRF`/`IERRF`/`IDXF`/`DIRF` provide error and event flags. Both families detect errors; **neither corrects them**—index re-initialization bounds accumulated error to one revolution rather than repairing a miscount.
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
- **Two complete velocity examples** (`eqep_ex1_freq_cal`, `eqep_ex2_pos_speed`) compute T-method and 1/T side by side, ship with design spreadsheets for the scaling derivation, and synthesize encoder signals from ePWM so they run on three jumper wires with no motor.
- **A C2000 Academy lab** targets the F28P55X LaunchPad specifically.
- A hardware **stall watchdog (QWDOG)** with no ST equivalent, 24 ePWM channels with 150 ps high-resolution mode, the **TMU** (trig as single-cycle-class instructions rather than a memory-mapped peripheral), the DCL control library mirroring the syllabus, and a **controlCARD card-edge form factor** at ~$79 with inherently isolated debug that ST cannot match at any price.

**Mitigation found in rev 12, and it matters:** STM32CubeG4 ships `TIM_Encoder` and `TIM_EncoderIndex_PulseOnCompare` example projects—the only ST family with encoder examples of any kind. Because the TIM peripheral with `ECR` is common across G4, H5, and U5, **those examples are usable as reference material on the U575ZI-Q** even though the G4 board itself was set aside on age. This should be verified early, since it materially reduces the largest identified lab-development cost.

---

## Part II—Choosing the ST board

Once RTOS ecosystem quality became the top criterion, the board question had to be re-asked. The result overturned the previous finalist.

### The support matrix

Enumerated from ST's own repositories: the X-CUBE-FREERTOS `Projects/` tree, STM32Cube family packages, and Zephyr's in-tree board directories.

| Board | Vendor FreeRTOS apps | ThreadX apps | Zephyr board | Cube examples incl. DAC/TIM? | Price |
|---|---|---|---|---|---|
| **NUCLEO-U575ZI-Q** | **4** | **7 (most of any ST board)** | yes, DAC/ADC/PWM enabled | **yes**—CORDIC, DAC, FMAC, TIM, OPAMP | **~$27** |
| NUCLEO-H563ZI | 4 | 5 | yes, richest yaml | yes | ~$43 |
| NUCLEO-H533RE | **0** | 2 | stub—no DAC or I2C node | **no DAC, no TIM** | ~$24 |
| NUCLEO-G474RE | 0 (series excluded from pack; 3 legacy in-package apps) | **0** | yes | **yes, incl. encoder + encoder-index** | ~$20 |
| NUCLEO-U385RG-Q | 4 | — | yes | — | ~$24 |
| STM32H5F5J-DK | 0 | 1 | yes | — | ~$124 |

X-CUBE-FREERTOS covers exactly nine boards across STM32U5, H5, WBA, C0, U0, U3 and N6. **The H533RE is not among them, and neither is any H5 board except the H563ZI. STM32G4 is excluded as a series**—a forward-looking signal worth noting even though the G4's in-package FreeRTOS applications still exist.

### Why the H533RE is out

Beyond the zero vendor FreeRTOS applications and the stub Zephyr port, it has a hard I/O defect that earlier revisions missed:

**DAC1_OUT1 (PA4) is not routed to any header.** ST's own Zephyr morpho-connector map for this board lists PA0, PA1 and PA5 but contains no PA4 entry, and UM3121's connector table agrees. The only reachable DAC output is **DAC1_OUT2 on PA5, which is the user LED LD2**—usable only by removing solder bridge SB6, or by accepting an LED and series resistor as a nonlinear load on the DAC buffer. For a course whose new requirement is one bipolar analog output, and whose students are mechanical engineers who may buy their own boards, requiring a soldering-iron modification on every unit is disqualifying.

Secondary marks against it: no CORDIC, no FMAC, no DAC or TIM examples in STM32CubeH5 for this board, and a 26-week manufacturer lead time.

Its encoder situation was actually fine—TIM2 and TIM3 both give a clean A/B/Z—so the earlier analysis was not wrong about encoders. It was looking at the wrong constraint.

### Why the U575ZI-Q wins

**RTOS ecosystem—the decisive criterion.** Four vendor FreeRTOS applications target this exact board (Mutex, **MPU**, Queues/ThreadFlags with TrustZone, and Semaphore with tickless idle), the same set the H563ZI gets. It also carries **seven ThreadX applications, more than any other ST board**, including a FreeRTOS-wrapper example and an MPU example. Its Zephyr board support is real rather than nominal, with DAC, ADC and PWM nodes actually enabled.

**I/O headroom—the best of any candidate.** The U5 has no Ethernet MAC, so the entire RMII pin block that constrains the H563ZI is free, and the virtual COM port sits on PA9/PA10 rather than PA2/PA3. The practical result: **all six encoder-capable timers provide a complete A/B/Z at the headers, and both DAC channels are free**, with no solder-bridge work anywhere. Verified against ST's own board devicetree: `&dac1` is enabled on `dac1_out1_pa4`, and the user LEDs are on PC7, PB7 and PG2 rather than PA5.

A clean allocation with zero modifications: encoder 1 on TIM2 (PA0/PA1/PA15), encoder 2 on TIM3 (PA6/PB5/PD2), analog out on PA4, PWM on TIM4 or TIM1, VCP already on PA9/PA10, leaving ample GPIO for the keypad.

**Silicon.** 160 MHz Cortex-M33 with TrustZone and MPU; **CORDIC and FMAC** (both absent on the H533); **a 14-bit ADC** at 2.5 MSPS plus a second 12-bit ADC—better resolution than either the H5 or the TI part offers; two 12-bit DAC channels; and **two on-chip op-amps and two comparators**. Longevity to 01/2036, identical to every other candidate.

**Price and supply.** ~$26.64 at DigiKey with ~400 in stock, essentially the same as the H533RE and about $17 under the H563ZI, with none of the H533RE's 26-week lead time.

**What it costs.** 160 MHz instead of 250—immaterial at course loop rates, where even a 10 kHz loop leaves more than a hundredfold headroom, though it is a cosmetic loss on a spec sheet. The family is low-power-oriented, which surfaces mainly as one tickless-idle example that is a legitimate RTOS teaching topic rather than noise. It requires enabling ICACHE and has an SMPS power path, both handled by CubeMX and worth one paragraph. And it is a Nucleo-144, physically larger than a Nucleo-64, which matters for the custom carrier layout.

### Runners-up, recorded so they aren't relitigated

**NUCLEO-H563ZI (~$43)** is the safe choice if the group prefers to stay on the H5 family, and rev 9's objection to it was overstated: Ethernet costs TIM5 and one TIM2 channel, but **four** timers still give a clean A/B/Z with no rework, and DAC1_OUT2 on PA5 is free because LD1 is on PB0. It has the richest Cube example set of any H5 board. It loses to the U575ZI-Q on price and pin headroom, not on capability.

**NUCLEO-G474RE (~$20)** is the cheapest and most available, and is the **only** ST board with vendor encoder examples—including an index example—which is precisely the gap that made TI attractive. It also has HRTIM, CORDIC, FMAC, op-amps and comparators. It stays out on two grounds: 2019 silicon for a book with a 10+ year shelf life, and ST's exclusion of the whole G4 series from the modern FreeRTOS pack, which is a signal about where ST is investing. Its encoder examples remain useful as reference material.

**NUCLEO-U385RG-Q (~$24)** is the only Nucleo-64 with all four modern FreeRTOS applications, but at 96 MHz with no CORDIC, no FMAC, and only four encoder timers, it is oriented at ultra-low-power rather than control.

**Newly discovered ST sub-families**, for a future edition rather than this one: **H543/H553** are essentially "H533 plus CORDIC" at 1 MB flash, and **H5E4/H5E5/H5F4/H5F5** reach 3–4 MB flash with 1.5 MB RAM. All are pre-distribution—ST reports no distributor availability—with no FreeRTOS pack support and, for the two Nucleos, no Zephyr board. The STM32H5F5J-DK exists at $124–138 but is a display-oriented discovery kit in the wrong form factor.

---

## Part III—Design consequences

### The bipolar front-end

Unchanged in substance: bipolar I/O requires external conditioning on any 3.3 V part, because the output stage must swing below ground and no 3.3 V-railed on-chip amplifier can. Roughly one quad op-amp, one charge pump, and a half-dozen precision resistors—a ~$6 BOM. Targeting ±5 V rather than the myRIO's ±10 V keeps the power design to a ~$2 charge pump off the existing 5 V rail instead of a boost-plus-inverter, and relaxes fault-protection sizing at the student-facing jacks.

**One correction propagates from the board change.** Rev 9 stated that on-chip ADC input buffering was a TI-only option, because the H533 has neither an op-amp nor a comparator. That was correct for the H533 and is **not correct for the U575ZI-Q**, which has two op-amps and two comparators. The design does not depend on this either way—an external quad op-amp remains the right choice because it is the sacrificial protection layer for the MCU's analog pins—but the U5 does offer an on-chip follower if a future revision wants one. The U5's 14-bit ADC also gives the input path better resolution than the design assumed.

### Encoders

Six encoder-capable timers with hardware index via `TIM_ECR`; TIM2 and TIM5 are 32-bit, the rest 16-bit. All six reach a complete A/B/Z at the headers on this board.

For velocity, the 1/T method is built from the encoder timer's **encoder-clock trigger output** (`CR2.MMS = 1000`) routed internally to a second timer's `ITRx` and captured on `TRC`—entirely on-chip, but a two-timer composition rather than the single-peripheral capture unit the eQEP provides. Note why the single-timer approach fails: in encoder mode the counter *is* position, so capturing on its own channels latches position rather than time. The STM32CubeG4 encoder examples are the closest thing to vendor reference material and should be evaluated early.

**Count rate is not a constraint.** At 160 MHz the timer ceiling is far beyond any lab motor; for a 512-line encoder at 4× decode, a 3,000 RPM shaft produces 102,400 counts/s against a ceiling in the tens of millions. What binds first is the encoder's own rating (100–300 kHz per channel, i.e. 3,000–17,500 RPM equivalent) and, well before that, signal integrity on unshielded single-ended cable beside motor PWM. False edges in quadrature are *directional*, so the count walks off monotonically rather than jittering—a distinctive symptom worth teaching. The cure is a differential (RS-422) encoder. Setting the timer input filter deliberately, and understanding that it trades noise immunity for bandwidth, is a good lab exercise in its own right.

### RTOS

**FreeRTOS is recommended** for the book. MIT-licensed, core-team Cortex-M33 port, LTS-eligible, and the smallest conceptual surface covering everything the book teaches: tasks, queues, semaphores, mutexes with priority inheritance, software timers, and direct-to-task notifications. Richard Barry's official kernel books are free PDFs, which is a real asset for a textbook, and it is what students are most likely to meet in industry. It arrives through the **X-CUBE-FREERTOS** pack via CubeMX's Package Manager rather than in the base Cube package—worth telling students explicitly, since this trips people up. **Teach the native FreeRTOS API rather than the CMSIS-RTOS2 wrapper**; the wrapper exists for portability but adds indirection over exactly the mechanisms the book wants visible.

**ThreadX** is shipped in-box with seven applications for this board and has a genuinely interesting scheduling feature—preemption-threshold—worth a sidebar. It is not recommended as the primary because ST ships it under a hardware-conditional Microsoft license rather than the Eclipse MIT upstream, and the student-facing learning corpus is thinner.

**Zephyr** is the most future-proof and the closest in spirit to the real-time Linux the first edition used, with `nucleo_u575zi_q` supported in-tree under Apache-2.0. Not recommended as the primary because it is an operating system *plus* a build system—west, devicetree, Kconfig—which is heavy for a mechanical-engineering audience, and its driver abstraction hides the register-level view the book wants students to have. It makes an excellent closing chapter and senior-design option.

### The control loop as a thread

The first edition ran the loop as a thread under real-time Linux; that structure is preserved. A hardware timer fires an ISR that does essentially nothing except signal a high-priority thread—`vTaskNotifyGiveFromISR()` or `xSemaphoreGiveFromISR()` with a context switch on exit. The thread wakes, reads the sample, evaluates the difference equation, writes the actuator, and blocks. Structurally identical to a thread waiting on a timerfd, so the first edition's material transfers with a change of API rather than of concept—and the numbers improve by orders of magnitude, since PREEMPT_RT wakeup jitter on an application processor is typically tens of microseconds where an MCU RTOS is sub-microsecond to low single digits.

What the book now teaches is better than the first edition could deliver: jitter comes from identifiable, measurable sources—tick resolution, whether the loop is tick-driven or interrupt-driven, ISR-to-thread wakeup latency, priority assignment, critical sections in other code, preemption by higher-priority threads—and every one is instrumentable with a GPIO toggle and a scope.

**Hardware offload remains sidebar material.** The C2000 CLA is the strongest example and deserves a figure: an independent processor with its own bus, registers, pipeline and memories, launched directly by an ADC end-of-conversion with no CPU involvement and no scheduler dependence. (Precision note so a sharp student doesn't catch us: CLA/CPU contention for shared RAM is hardware-arbitrated and bounded but not literally zero, so "decoupled from scheduling" is accurate where "completely unaffected" overstates it.) On the U575ZI-Q the analogues are timer-triggered ADC into DMA for acquisition, and the **FMAC** for the compensator arithmetic—which can host a 3p3z IIR (ST's AN5305 has a worked example) but has no hardware trigger, is q1.15 fixed-point only, and cannot branch, so anti-windup, saturation, mode switching and fault response all return to the CPU. That contrast is itself the lesson: an accelerator replaces the arithmetic, a co-processor replaces the loop.

---

## Open questions and next steps

- **Confirm the G4 encoder examples port to the U5.** `TIM_Encoder` and `TIM_EncoderIndex_PulseOnCompare` in STM32CubeG4 are the only ST encoder examples that exist; the TIM IP with `ECR` is common across G4/H5/U5, so they should transfer. This is the highest-value thing to verify early, because it directly reduces the largest lab-development cost.
- **Order two or three Nucleo-U575ZI-Q boards now** and confirm the pin allocation on hardware: PA4 and PA5 free for DAC, TIM2 and TIM3 clean for encoders, and the default solder-bridge states (the U575 user manual mentions PA4 shared with SAI/SPI/UCPD via SB35/SB38, though ST's own Zephyr devicetree routes DAC1_OUT1 to PA4 unconditionally).
- **Range:** ±5 V or ±3.3 V for the front end? Decide from the labs' actual sensor and actuator interfaces. Note the U5's 14-bit ADC gives more headroom than the design assumed.
- **RTOS API:** native FreeRTOS or CMSIS-RTOS2 (recommend native). And confirm the control-loop thread structure—timer ISR signalling a high-priority thread—plus whether the loop is tick-driven or interrupt-driven, since the jitter lab follows directly from that choice.
- **Encoder count and type:** how many simultaneous encoders in the heaviest lab, and single-ended or differential? At lab speeds signal integrity binds, not bandwidth.
- **Carrier form factor:** the Nucleo-144 is physically larger than the Nucleo-64 the earlier analysis assumed. ST has **no card-edge module** for any STM32 family, so the carrier mates to the Zio and morpho headers; consider terminating at ST's published 34-pin motor-control connector (UM1970) rather than directly at the morpho pinout, which is not guaranteed consistent across boards.
- **Debug isolation:** the Nucleo's ST-LINK/V3EC is not isolated and not detachable. At 12–24 V this is a SELV system with no shock hazard, and ST itself ships 8–48 V motor shields for non-isolated Nucleos with no isolation warning. The real risk is debug-link robustness and ADC noise. Two free mitigations: specify a **floating** bench supply and have students run on battery during motor labs. Note that an earthed scope ground clip is a more effective ground-loop generator than any USB cable, and that common ADuM3160 USB isolator dongles are full-speed only while ST-LINK/V3EC is high-speed.
- **Middleware scope:** does any planned lab need a filesystem, network stack, or wire protocol? On the U5 this is unconstrained; recording the question because it was a binding constraint on the alternative.
- **Verify before publishing:** the H533RE PA4 finding rests on ST's Zephyr morpho map plus a UM3121 table extraction that showed one inconsistency elsewhere; per-instance `ECR` gating across TIM1/2/3/4/5/8 is assumed uniform from the shared `TIM_TypeDef` rather than read from the reference manual.

---

## Sources

**Chosen board and family**
- [NUCLEO-U575ZI-Q](https://www.st.com/en/evaluation-tools/nucleo-u575zi-q.html) · [STM32U575ZI](https://www.st.com/en/microcontrollers-microprocessors/stm32u575zi.html) · [Octopart price/stock](https://octopart.com/part/stmicroelectronics/NUCLEO-U575ZI-Q)
- [Zephyr `boards/st/nucleo_u575zi_q`](https://github.com/zephyrproject-rtos/zephyr/tree/main/boards/st/nucleo_u575zi_q)—ST-authored board devicetree: `&dac1` enabled on `dac1_out1_pa4`; LEDs on PC7/PB7/PG2.
- [stm32u575xx.h CMSIS header](https://raw.githubusercontent.com/STMicroelectronics/cmsis_device_u5/master/Include/stm32u575xx.h)—`IS_TIM_ENCODER_INTERFACE_INSTANCE` = TIM1/2/3/4/5/8; `ECR` present in `TIM_TypeDef`; CORDIC and FMAC present.
- [STM32CubeU5](https://github.com/STMicroelectronics/STM32CubeU5)—example coverage including CORDIC, DAC, FMAC, OPAMP, TIM.
- [STM32_open_pin_data](https://github.com/STMicroelectronics/STM32_open_pin_data)—alternate-function maps used for the header-availability analysis.

**RTOS**
- [X-CUBE-FREERTOS](https://github.com/STMicroelectronics/x-cube-freertos)—v1.6.0; nine board directories, 34 applications; four for NUCLEO-U575ZI-Q; **none for NUCLEO-H533RE**; STM32G4 unsupported as a series. Kernel via [stm32-mw-freertos](https://github.com/STMicroelectronics/stm32-mw-freertos), V11.2.0 with CMSIS-RTOS2 layer. [Product page](https://www.st.com/en/embedded-software/x-cube-freertos.html)
- [FreeRTOS-Kernel `portable/`](https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/main/portable)—`ARM_CM33`/`ARM_CM33_NTZ` core-team ports with `mpu_wrappers_v2_asm.c`; **no C28x port**. [Community-Supported-Ports `CCS/C2000_C28x`](https://github.com/FreeRTOS/FreeRTOS-Kernel-Community-Supported-Ports/tree/main/CCS/C2000_C28x)—three commits ever, unchanged since Sept 2022. [Partner-Supported-Ports](https://github.com/FreeRTOS/FreeRTOS-Kernel-Partner-Supported-Ports)—contains C29x, not C28x.
- [c2000ware-FreeRTOS](https://github.com/TexasInstruments/c2000ware-FreeRTOS)—TI's fork, F28P55x demos, SysConfig FreeRTOS tool, ROV. [TI-RTOS](https://www.ti.com/tool/TI-RTOS)—"for legacy devices and support is not available."
- [stm32-mw-threadx](https://github.com/STMicroelectronics/stm32-mw-threadx)—ThreadX 6.4.0 under Microsoft license; STM32U5 and H5 on `LICENSED-HARDWARE.txt`. Upstream [eclipse-threadx/threadx](https://github.com/eclipse-threadx/threadx) is MIT at v6.5.0.
- [Zephyr `arch/`](https://github.com/zephyrproject-rtos/zephyr/tree/main/arch)—no C28x architecture.

**Boards evaluated and set aside**
- [NUCLEO-H533RE](https://www.st.com/en/evaluation-tools/nucleo-h533re.html) · [UM3121 (MB1814)](https://www.st.com/resource/en/user_manual/um3121-stm32h5-nucleo64-board-mb1814-stmicroelectronics.pdf) · [Zephyr `nucleo_h533re/st_morpho_connector.dtsi`](https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/st/nucleo_h533re/st_morpho_connector.dtsi)—**PA4 absent from the morpho map**; PA0, PA1, PA5 present.
- [NUCLEO-H563ZI](https://www.st.com/en/evaluation-tools/nucleo-h563zi.html) · [UM3115 (MB1404)](https://www.st.com/resource/en/user_manual/um3115-stm32h5-nucleo144-board-mb1404-stmicroelectronics.pdf) · [DS14258](https://www.st.com/resource/en/datasheet/stm32h563ri.pdf)
- [NUCLEO-G474RE](https://www.st.com/en/evaluation-tools/nucleo-g474re.html) · [STM32CubeG4](https://github.com/STMicroelectronics/STM32CubeG4)—`Projects/NUCLEO-G474RE/Examples/TIM/TIM_Encoder` and `TIM_EncoderIndex_PulseOnCompare`; the only ST encoder examples in any family.
- [cmsis-device-h5](https://github.com/STMicroelectronics/cmsis-device-h5)—headers for h543/h553/h5e4/h5e5/h5f4/h5f5, the newly identified sub-families. [NUCLEO-H5E5ZJ](https://www.st.com/en/evaluation-tools/nucleo-h5e5zj.html)—"No availability of distributors reported."

**TI comparison (decision record)**
- [TMS320F28P550SJ](https://www.ti.com/product/TMS320F28P550SJ) · [LAUNCHXL-F28P55X](https://www.ti.com/tool/LAUNCHXL-F28P55X) · [TMDSCNCD28P55X](https://www.ti.com/tool/TMDSCNCD28P55X) · [C2000Ware](https://www.ti.com/tool/C2000WARE) · [c2000ware-c2000-academy](https://github.com/TexasInstruments/c2000ware-c2000-academy) · [eQEP driverlib API](https://software-dl.ti.com/C2000/docs/C2000_driverlib_api_guide/f28p55x/build/html/modules/eqep.html) · [DCL SPRUID3](https://www.ti.com/lit/ug/spruid3/spruid3.pdf)
- [SPRU514 data types](https://downloads.ti.com/docs/esd/SPRU514Q/data-types-stdz0555922.html)—`char` is 16 bits, `double` is 32 bits. [SPRAD88](https://www.ti.com/lit/pdf/sprad88)—porting consequences. [CLA Software Development Guide](https://software-dl.ti.com/C2000/docs/cla_software_dev_guide/intro.html)
- [TI E2E—arm64 CCS commitment, July 2026](https://e2e.ti.com/support/processors-group/processors/f/processors-forum/1655979/ccstudio-is-ccs-going-to-run-on-apple-silicon-without-depending-on-rosetta) · [CCS 21.0.0 download](https://www.ti.com/tool/download/CCSTUDIO/21.0.0)

**Method note.** Corrections across revisions 8–12 were verified against vendor datasheets, SVD-derived register definitions, CMSIS device headers, HAL and kernel sources, SDK and RTOS repository file trees, ST's pin database and board devicetrees, and distributor pricing—not against community posts. Four errors were caught and corrected in the process: the encoder-hardware claim (rev 8, from a forum thread about a different silicon generation), the controlCARD pinout guarantee and LaunchPad isolation overstatements and the on-chip op-amp claim (rev 9), the controlCARD price (rev 10, one card's price applied to another), and the ST board selection itself (rev 12, an unrouted DAC pin on the previous finalist). Claims that could not be verified from a primary source appear in the open questions rather than in the body.