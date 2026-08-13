---
project_note_id: 1020
title: Board Configuration and Lab Resource Allocation—Nucleo-U575ZI-Q
---

# Board Configuration and Lab Resource Allocation

**Real-Time Computing for Mechanical Engineers, 2nd Edition**
NUCLEO-U575ZI-Q (STM32U575ZITxQ)—internal working document—August 2026 (rev 1)

Companion to *Platform Selection: ST STM32, Nucleo-U575ZI-Q*, which records why this board was chosen. This document records **how it will be used**: the pin map, the peripheral budget, and the real-time design rules the labs depend on.

Everything below was verified against ST's pin database (`STM32_open_pin_data`), the board's own CubeMX default configuration, UM2861 Table 19 (the CN11/CN12 morpho map), ST's Zephyr board devicetree, and the STM32U5 HAL and CMSIS headers. Where a claim rests on estimate rather than citation, it says so.

---

## Board at a glance

| | |
|---|---|
| MCU | STM32U575ZITxQ, Cortex-M33 with TrustZone and MPU, **160 MHz**, LQFP144 |
| Memory | 2048 KB flash (dual bank), **786 KB SRAM** |
| Flash wait states at 160 MHz | **4**, VOS Range 1; ICACHE available (see real-time rules) |
| FPU | FPv5, **single precision only**—no `double` in hardware |
| Math acceleration | CORDIC, FMAC |
| ADC | ADC1: **14/12/10/8-bit, 2.5 MSPS**, 15 external channels · ADC4: 12-bit, low-power domain, 17 channels |
| DAC | 1 peripheral, **2 buffered outputs**, 2.05 µs settling, timer-triggered and DMA-fed |
| On-chip analog | **2 op-amps with PGA, 2 comparators** |
| Advanced timers | TIM1, TIM8—complementary outputs, dead-time, two break inputs each |
| Encoder-capable timers | **6** (TIM1/2/3/4/5/8), all with hardware index via `TIM_ECR`; TIM2 and TIM5 are 32-bit |
| Serial | 6 USART/UART, 3 SPI, 4 I²C, 1 FDCAN |
| GPIO at morpho headers | 110, of which **92 free** after board functions |
| Not present | **No Ethernet MAC** (structural—see the platform doc) |

Board functions consuming pins by default: PA9/PA10 (ST-LINK VCP on USART1), PA11/PA12 (USB), PA13/PA14 (SWD), PC13 (user button), PC14/PC15 (LSE), PH0/PH1 (HSE/MCO), PC7/PB7/PG2 (LD1/LD2/LD3), PC2 (VBUS_SENSE), PB14/PB15/PB5 (UCPD).

Note that **VBUS_SENSE is on PC2, not PA4**, which is why both DAC channels are free on this board and only one is free on the Nucleo-H563ZI.

---

## Pin allocation

This allocation was checked programmatically against three things: that the alternate function exists on that pin, that the pin appears on a morpho header, and that the pin is not consumed by a board function. **No duplicates, all checks pass, 64 GPIO remain free.**

| Function | Signal | Pin | Header |
|---|---|---|---|
| **Encoder 1—TIM2 (32-bit)** | | | |
| A | `TIM2_CH1` | PA0 | CN11-28 |
| B | `TIM2_CH2` | PA1 | CN11-30 |
| Z (index) | `TIM2_ETR` | PA15 | CN11-17 |
| **Encoder 2—TIM3 (16-bit)** | | | |
| A | `TIM3_CH1` | PE3 | CN11-47 |
| B | `TIM3_CH2` | PE4 | CN11-48 |
| Z (index) | `TIM3_ETR` | PE2 | CN11-46 |
| **Motor amplifier—TIM1 (advanced)** | | | |
| PWM high | `TIM1_CH1` | PE9 | CN12-52 |
| PWM low (complementary) | `TIM1_CH1N` | PE8 | CN12-40 |
| Fault / break | `TIM1_BKIN` | PE15 | CN12-53 |
| **Analog** | | | |
| Bipolar analog out | `DAC1_OUT1` | PA4 | CN11-32 |
| Bipolar analog in | `ADC1_IN1` | PC0 | CN11-38 |
| **Serial** | | | |
| Second UART TX | `USART2_TX` | PD5 | CN11-41 |
| Second UART RX | `USART2_RX` | PD6 | CN11-43 |
| I²C (optional LCD backpack) | `I2C1_SCL` / `SDA` | PB8 / PB9 | CN12-3 / CN12-5 |
| **Keypad 4×3** | rows R1–R4 | PF0, PF1, PF2, PF3 | CN11-53/51/52, CN12-58 |
| | cols C1–C3 | PF4, PF5, PF6 | CN12-38/36, CN11-9 |
| **Character LCD, 4-bit** | RS, E | PG0, PG1 | CN11-59, CN11-58 |
| | D4–D7 | PG3, PG4, PG5, PG6 | CN11-44, CN12-69/68/70 |

**28 pins used, 64 free.** `USART2` on PD5/PD6 is not a guess—it is exactly what ST's own Zephyr board file enables, which is good evidence those pins are clear.

**On the index input:** the encoder index is driven from the timer's **ETR pin**, confirmed in `stm32u5xx_hal_tim_ex.h` where `TIM_ENCODERINDEX_POLARITY_*` are aliases of `TIM_ETRPOLARITY_*`. Z must therefore go to an ETR pin distinct from CH1/CH2, which is why encoder 1's index is on PA15 rather than PA0 (PA0 carries both `TIM2_CH1` and `TIM2_ETR`).

---

## Expansion headroom

**TIM1 is completely unobstructed.** A full three-phase complementary set plus both break inputs sits on one contiguous port, all free:

```
TIM1_CH1/CH1N  = PE9  / PE8
TIM1_CH2/CH2N  = PE11 / PE10
TIM1_CH3/CH3N  = PE13 / PE12
TIM1_CH4       = PE14
TIM1_BKIN      = PE15      TIM1_BKIN2 = PE14
```

This scales from a single H-bridge lab to a three-phase inverter lab **without re-pinning**—the allocation above uses CH1/CH1N and leaves the rest in place.

**PWM resolution at 160 MHz**, TIM1 on APB2 with prescaler 1:

| PWM frequency | Edge-aligned (ARR+1) | Effective bits | Center-aligned | Effective bits |
|---|---|---|---|---|
| 10 kHz | 16 000 | 13.97 | 8 000 | 12.97 |
| 20 kHz | 8 000 | 12.97 | 4 000 | 11.97 |
| 50 kHz | 3 200 | 11.64 | 1 600 | 10.64 |
| 100 kHz | 1 600 | 10.64 | 800 | 9.64 |

Both TIM1 and TIM8 also implement **timer dithering** (`TIM_CR1_DITHEN`), adding four fractional bits of effective duty resolution at a fixed carrier—a good sidebar topic.

**Analog headroom.** 21 ADC-capable pins remain free at the headers, and the second DAC channel (PA5, CN12-11) is available for a debug or plant-simulation output. ADC1's 14-bit mode runs at the **full 2.5 MSPS**—there is no sample-rate penalty for choosing top resolution, which is unusual and worth exploiting.

---

## Known conflicts and cautions

**OPAMP1 collides with encoder 1.** `OPAMP1_VINP` is PA0 and `OPAMP1_VINM` is PA1, which the allocation above uses for encoder 1. If a lab module needs OPAMP1, either move encoder 1 to `TIM2_CH1`/`TIM2_CH2` on PA15/PB3, or use **OPAMP2** (PA6/PA7, output PB0), which does not conflict. Both work—just don't do both on the same board without checking.

**TIM8 cannot produce a full three-phase set on this board.** `TIM8_CH2` exists only on PC7, which is the green LED LD1. TIM8 can still do a one- or two-phase complementary pair (CH1/CH1N on PC6/PA5 or PA7, CH3/CH3N on PC8/PB1). Use TIM1 for the motor amplifier and keep TIM8 as the spare.

**Per-pin 5 V tolerance is an open item.** ST's datasheet says the U575 has "up to 136 fast I/Os... most 5V-tolerant," but the per-pin I/O-structure column (FT / FT_a / TT / TT_a) is not in the machine-readable pin data and could not be extracted from DS13737. **Action:** read DS13737 Table 25 (legend) and Table 26 (pin definitions) for PA0, PA1, PA15, PE2, PE3, PE4 (encoder inputs) and PF0–PF6 (keypad) before finalizing the wiring.

**Design guidance that is robust to the answer:** buffer a 5 V encoder's A/B/Z lines with a 74LVC-series part or a divider regardless. It removes the tolerance question, protects a board students will mis-wire, and is itself a worthwhile lab topic. Drive the keypad from 3V3 and the question does not arise.

**One solder-bridge row to confirm on hardware.** One extraction of UM2861 Table 20 hinted at a bridge affecting PA0; this did not corroborate against ST's own board configuration, which does not list PA0 as used. If PA0 turns out to be encumbered, move encoder 1 to PA15/PB3 as above.

---

## Real-time design rules

This section is the reason the companion document exists. **The workload is not CPU-bound; everything that can go wrong is a latency problem, not a throughput problem.**

### CPU budget

For a 10 kHz control loop at 160 MHz:

| Item | Estimate | % CPU |
|---|---|---|
| ADC conversion, 14-bit, DMA-fed | 527 ns | 0—overlapped |
| 2p2z difference equation + I/O in the ISR | ~0.4–0.6 µs | 0.4–0.6% |
| Task notification + context switch | ~1.5–2.8 µs | 1.5–2.8% |
| **Control total** | ~2–3.4 µs of 100 µs | **~2–3.5%** |
| UI thread (keypad, LCD, UART) | remainder | ~0–5% typical |
| **Headroom** | | **> 90%** |

At 1 kHz the control path is under 0.4%. Even at 50 kHz it stays comfortably under 20%.

**A result worth teaching:** the scheduling overhead exceeds the control arithmetic—roughly 1.5–2.8 µs to wake a thread versus 0.4–0.6 µs to evaluate the difference equation. A natural lab is to implement the loop as a task, measure it, then implement it directly in the ISR and measure again.

**Caveat on these numbers.** Arm does not publish instruction cycle counts or an interrupt-latency figure for the Cortex-M33—the M4 has a published table, the M33 does not. The figures above are engineering estimates with stated reasoning, not citations. `DWT->CYCCNT` is available on this part, and "here is how you measure it on your hardware" is better pedagogy than a table anyway.

---

### What actually threatens the control loop

Three distinct mechanisms, with very different consequences. Getting this distinction right is most of the real-time lesson.

| Mechanism | Blocks a higher-priority **task**? | Blocks a higher-priority **ISR**? |
|---|---|---|
| **Being slow**—spinning, polling a peripheral flag | No | No |
| **Scheduler suspension**—`vTaskSuspendAll()` | **Yes** | No |
| **Interrupt masking**—`taskENTER_CRITICAL()` raises BASEPRI | **Yes** | **Yes** |

**Harmless.** A 5 ms LCD refresh over I²C, 7 ms of UART output at 115200, a slow keypad scan with settling delays, blocking `HAL_I2C_Master_Transmit` / `HAL_SPI_TransmitReceive` / `HAL_UART_Transmit` in polling mode. All of these merely consume CPU the control loop was not using; a lower-priority task is preempted with no added latency. **`HAL_Delay()` is in this category**—it busy-waits on a tick counter and does *not* mask interrupts. (Two caveats: move the HAL timebase off SysTick when FreeRTOS owns it—ST ships `Examples/HAL/HAL_TimeBase_TIM` for exactly this, and CubeMX prompts for it—and never call `HAL_Delay()` from an ISR.)

**Harmful.**

- **`taskENTER_CRITICAL()` around LCD writes—the canonical student bug.** An HD44780 in 4-bit mode needs roughly 37–100 µs of execution per byte. A student who wraps that in a critical section "to keep the LCD timing clean" creates tens to hundreds of microseconds of interrupt masking, which at 10 kHz means **missed control periods**, not merely jitter. This deserves its own lab.
- **Dynamic allocation.** `pvPortMalloc`/`vPortFree` under `heap_4` call `vTaskSuspendAll()`, so `malloc`, `free`, and any `printf`-family call that allocates can delay a higher-priority control *task* even though the ISR still runs on time. Avoid allocation after init; use `snprintf` into a static buffer, or avoid `printf` in the UI path entirely.
- **Priority inversion via a shared bus.** FreeRTOS mutexes have priority inheritance and bound it, but the clean answer is not to share. In the allocation above the control loop touches only ADC, DAC and TIM, while the UI thread owns USART2, I²C1 and GPIO—deliberately disjoint.
- **Flash programming.** A page erase stalls the flash interface for milliseconds; any code or literal fetched from flash stalls with it. Catastrophic at 10 kHz. If a lab logs to flash, teach this explicitly. Mitigations: dual-bank execution, or relocate the control ISR to SRAM.
- **Run-time clock or voltage-scaling changes.** `HAL_RCC_ClockConfig()` reprograms the flash wait-state count and spins waiting for it to take effect; timing is undefined during that window. Never call it after the control loop starts.
- **Low-power mode transitions.** Entering Stop gates clocks and requires PLL relock on wake. **Disable FreeRTOS tickless idle for the control labs**, then introduce it later as its own lab where the tradeoff is the point.
- **SMPS transitions.** The `-Q` package has an SMPS regulator; switching it at run time injects a supply transient, and SMPS ripple can appear in ADC readings. Prefer the LDO for the analog labs—and measuring the difference is a good exercise.

---

### The structural rule: zero-latency interrupts

Because interrupt masking is the only mechanism that can break a hard deadline, the book should teach this explicitly:

> Give the control timer/ADC interrupt an NVIC priority numerically **lower** (more urgent) than `configMAX_SYSCALL_INTERRUPT_PRIORITY`, so that `taskENTER_CRITICAL()` anywhere in the system **cannot** mask it.

The cost is that this ISR may not call any FreeRTOS API—no `...FromISR` calls. In exchange, the control loop becomes immune to every mistake a student can make in the UI thread. Explaining *why* is a complete lecture on BASEPRI, priority grouping, and the boundary between the RTOS and the hardware. ST's board configuration sets `NVIC_PRIORITYGROUP_3`, which is the relevant knob.

### Cache and code placement

At 160 MHz the flash runs at **4 wait states**. ICACHE converts that deterministic cost into a bimodal one: fast on a hit, slow on a miss. Average throughput improves and **worst-case jitter appears**—exactly the tradeoff a real-time text should teach.

Two U5-specific opportunities:

- **The ICACHE has hardware hit and miss monitors** (`ICACHE_HMONR`, `ICACHE_MMONR`). Students can measure their own miss rate around the control loop rather than being told a number.
- **ST's U5 examples force `ICACHE_1WAY`** (direct-mapped) in 112 places. Direct-mapped has markedly worse worst-case behavior under conflict misses. If determinism is the lesson, delete that line and keep the 2-way default.

With 786 KB of SRAM, relocating the control ISR and its state to SRAM (`__attribute__((section(".RamFunc")))`) is trivially affordable and removes flash wait states and cache behavior from the loop entirely. **Measuring loop jitter with code in flash-plus-cache versus in SRAM is an excellent lab.**

### Floating point

The Cortex-M33 FPU is **single precision only**. `double` arithmetic falls back to software emulation, typically 50–100× slower per operation. The book must be explicit about `float`, the `f` suffix on literals, and using `sinf` rather than `sin`—a `1.0` where `1.0f` was meant will silently promote an entire expression to software `double`. This is a useful teaching moment rather than a defect, but it needs to be taught rather than discovered.

---

## Verification checklist for when boards arrive

- [ ] Confirm **PA4 and PA5** are free and drive the DAC (SB35/SB38 should be OFF; LD1 should be on PC7, not PA5).
- [ ] Confirm **PA0** is unencumbered by any solder bridge; if not, move encoder 1 to PA15/PB3.
- [ ] Read **DS13737 Table 25/26** for the 5 V-tolerance structure of PA0, PA1, PA15, PE2, PE3, PE4, PF0–PF6.
- [ ] Bring up **TIM2 and TIM3 encoder mode with hardware index** and confirm `TIM_ECR` index reset behaves as documented; confirm `TERRF`/`IERRF` fire on a deliberately corrupted quadrature input.
- [ ] Port the **STM32CubeG4 `TIM_Encoder` and `TIM_EncoderIndex_PulseOnCompare` examples**—the only vendor encoder examples ST publishes—and confirm they transfer to the U5's TIM IP. This is the highest-value item; it directly reduces the largest lab-development cost.
- [ ] Measure the **notification-to-thread latency** with a GPIO toggle and confirm the CPU budget above.
- [ ] Verify the **zero-latency interrupt** configuration by deliberately taking a long critical section in the UI thread and confirming the control ISR still fires on time.
- [ ] Confirm **X-CUBE-FREERTOS** installs via CubeMX's Package Manager and that the four U575ZI-Q example applications build and run, particularly `FreeRTOS_MPU`.

---

## Sources

- [NUCLEO-U575ZI-Q](https://www.st.com/en/evaluation-tools/nucleo-u575zi-q.html) · [UM2861 (MB1549 board manual)](https://www.st.com/resource/en/user_manual/um2861-stm32u5-nucleo144-board-mb1549-stmicroelectronics.pdf)—Table 19 is the CN11/CN12 morpho map used for every pin assignment above.
- [DS13737—STM32U575xx datasheet](https://www.st.com/resource/en/datasheet/stm32u575zi.pdf)—Tables 25 and 26 hold the per-pin I/O structure needed to close the 5 V-tolerance question.
- [STM32_open_pin_data](https://github.com/STMicroelectronics/STM32_open_pin_data)—`mcu/STM32U575ZITxQ.xml` for the die-exact peripheral and alternate-function inventory; `boards/` for ST's own default board configuration.
- [Zephyr `boards/st/nucleo_u575zi_q`](https://github.com/zephyrproject-rtos/zephyr/tree/main/boards/st/nucleo_u575zi_q)—ST-authored devicetree; source of the USART2 on PD5/PD6 and DAC-on-PA4 confirmations, and of the LED assignments.
- [STM32CubeU5](https://github.com/STMicroelectronics/STM32CubeU5)—`SystemClock_Config()` across the U575ZI-Q tree confirms `FLASH_LATENCY_4` with `PWR_REGULATOR_VOLTAGE_SCALE1`; the ICACHE associativity observation comes from the same tree.
- `stm32u5xx_hal_adc.h`, `stm32u5xx_hal_tim_ex.h`—ADC resolution constants (14-bit is ADC1/ADC2 only) and the encoder-index/ETR aliasing.
- [Arm Cortex-M33 TRM](https://documentation-service.arm.com/static/5f15c42420b7cf4bc5247f3a)—FPv5 single-precision FPU. [Arm interrupt-latency guide](https://developer.arm.com/community/arm-community-blogs/b/architectures-and-processors-blog/posts/beginner-guide-on-interrupt-latency-and-interrupt-latency-of-the-arm-cortex-m-processors)—lists M0/M0+/M3/M4 but **not M33**, which is why the latency figures here are estimates.
- [FreeRTOS FAQ—memory usage, boot times, context-switch times](https://www.freertos.org/Why-FreeRTOS/FAQs/Memory-usage-boot-times-context/)—FreeRTOS's position that context-switch time is port- and clock-dependent and must be measured.
- [STM32CubeG4—`TIM_Encoder` and `TIM_EncoderIndex_PulseOnCompare`](https://github.com/STMicroelectronics/STM32CubeG4)—the only vendor encoder examples ST publishes in any family; candidates for porting.