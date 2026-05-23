/* ============================================================================
 * config.h  --  Hardware/experiment configuration
 *
 * Analog-to-analog feedback-control latency experiment
 * Raspberry Pi 5  +  Waveshare "High-Precision AD/DA Board"
 *   - ADC: TI ADS1256  (24-bit sigma-delta, SPI)
 *   - DAC: TI DAC8552  (16-bit, dual-channel, SPI)
 *
 * ---------------------------------------------------------------------------
 * !!! VERIFY EVERYTHING IN THIS FILE AGAINST YOUR OWN BOARD !!!
 *
 * The values below match the well-known Waveshare High-Precision AD/DA Board
 * and a standard Raspberry Pi 5. If you have a board revision with a different
 * silkscreen, or you rewired anything, the pin numbers WILL be wrong. Every
 * line marked  [VERIFY]  is something to confirm on the Waveshare wiki page
 * for your board revision before trusting a measurement.
 * ==========================================================================*/

#ifndef CONFIG_H
#define CONFIG_H

/* ---------------------------------------------------------------------------
 * SPI bus
 *
 * The ADS1256 and DAC8552 share ONE physical SPI bus (SCLK/MOSI common, plus
 * MISO for the ADC). Chip-select is NOT done by the SPI controller on this
 * board -- Waveshare routes each device's CS to a plain GPIO (see below), so
 * we open the bus in "no hardware CS" mode and toggle CS ourselves.
 *
 * Enable SPI0 first:  sudo raspi-config  ->  Interface Options  ->  SPI
 * (or add `dtparam=spi=on` to /boot/firmware/config.txt and reboot).
 * -------------------------------------------------------------------------*/
#define SPI_DEVICE        "/dev/spidev0.0"   /* [VERIFY] SPI0, CE0 node      */
#define SPI_MODE          1                  /* CPOL=0, CPHA=1 (both chips)  */
#define SPI_BITS_PER_WORD 8

/* ADS1256 SCLK ceiling.
 * The ADS1256 requires SCLK <= f_CLKIN / 4. The Waveshare board clocks the
 * ADS1256 from a 7.68 MHz crystal, so 7.68e6 / 4 = 1.92 MHz is the hard
 * ceiling. We sit just under it. Going faster corrupts reads.            */
#define ADS1256_SPI_HZ    1920000u           /* [VERIFY] f_CLKIN/4           */

/* DAC8552 tolerates a much faster SCLK (datasheet allows up to 30 MHz).
 * 15.6 MHz is a safe, round value well within the Pi 5 SPI capability.
 * Per-transfer speed is set individually, so the ADC and DAC can differ. */
#define DAC8552_SPI_HZ    15600000u

/* ---------------------------------------------------------------------------
 * GPIO  (Raspberry Pi 5)
 *
 * On the Pi 5 the 40-pin header GPIOs live on the RP1 I/O controller and are
 * exposed by the kernel as a GPIO character device. On current Raspberry Pi
 * OS that is /dev/gpiochip0; on some kernels it was /dev/gpiochip4. If line
 * requests fail with ENODEV, try the other one (see README).
 *
 * IMPORTANT: the old bcm2835 / wiringPi "/dev/mem poke fixed addresses"
 * libraries that Waveshare's sample code uses DO NOT WORK on the Pi 5.
 * This project uses the kernel GPIO uAPI v2 instead -- see gpio.c.
 *
 * Line numbers below are BCM GPIO numbers (== uAPI line offsets on gpiochip0).
 * -------------------------------------------------------------------------*/
#define GPIO_CHIP         "/dev/gpiochip0"   /* [VERIFY] Pi 5 header bank    */

#define LINE_ADC_DRDY     17  /* [VERIFY] ADS1256 DRDY  -> header pin 11, IN */
#define LINE_ADC_RESET    18  /* [VERIFY] ADS1256 RESET -> header pin 12, OUT*/
#define LINE_ADC_CS       22  /* [VERIFY] ADS1256 CS    -> header pin 15, OUT*/
#define LINE_DAC_CS       23  /* [VERIFY] DAC8552 SYNC   -> header pin 16, OUT*/
/* ADS1256 PDWN is tied high (normal operation) on the Waveshare board, so
 * it is not controlled here. [VERIFY] on your board revision.             */

/* ---------------------------------------------------------------------------
 * ADS1256 acquisition settings
 * -------------------------------------------------------------------------*/

/* Data-rate register (DRATE) code.
 *   0xF0 = 30 kSPS  -- the fastest the part offers
 *   0xE0 = 15 kSPS  -- next step down, doubles the conversion interval
 * On the Pi 5 + spidev + GPIO uAPI, a single loop iteration costs ~48 us
 * (~33 us of which is GPIO/syscall overhead per CS toggle and per spidev
 * transfer); at 30 kSPS the 33.3 us conversion interval is too tight and the
 * loop drops samples. 15 kSPS (66.7 us interval) sustains comfortably and is
 * the right starting point. Trade-off: a lower rate increases the sigma-delta
 * filter's group delay, so the *analog* latency on the scope grows somewhat. */
#define ADS1256_DRATE     0xE0u   /* 15000 SPS */

/* PGA gain = 1 (ADCON bits 2:0 = 000); clock-out and sensor-detect off. */
#define ADS1256_ADCON     0x00u

/* STATUS register: MSB-first, auto-cal off, input buffer off.
 * Buffer off keeps the input settling fast; it is fine for a low-impedance
 * source such as a function generator. Set to 0x02 to enable the buffer. */
#define ADS1256_STATUS    0x00u

/* Input multiplexer (MUX). Default below = single-ended: AIN0 (+) / AINCOM (-).
 *   single-ended AIN0 : 0x08      (AINCOM nibble = 8)
 *   differential 0/1  : 0x01      (AIN0 = +, AIN1 = -)
 * NOTE: with single-ended AIN0 referenced to AINCOM=GND, the input pin must
 * stay within 0..AVDD. A bipolar sine will violate this -- give the function
 * generator a positive DC offset so the signal stays in range (see README). */
#define ADS1256_MUX       0x08u

/* ---------------------------------------------------------------------------
 * Reference voltages (used only for the optional human-readable volt scaling;
 * the latency/phase measurement on the oscilloscope does not depend on them).
 * -------------------------------------------------------------------------*/
#define ADC_VREF          2.5    /* [VERIFY] ADS1256 reference, volts        */
#define DAC_VREF          5.0    /* [VERIFY] DAC8552 reference, volts        */

/* ---------------------------------------------------------------------------
 * Real-time scheduling
 * -------------------------------------------------------------------------*/
#define RT_PRIORITY       80     /* SCHED_FIFO priority (1..99)              */
#define RT_CPU_CORE       3      /* CPU core to pin the loop to (Pi 5: 0..3) */

#endif /* CONFIG_H */
