/* ============================================================================
 * gpio.h  --  GPIO access for the Waveshare AD/DA board on a Raspberry Pi 5
 *
 * Uses the Linux GPIO character-device uAPI v2 (<linux/gpio.h>, kernel >= 5.10).
 * This is the only GPIO method that works on the Pi 5: the legacy bcm2835 /
 * wiringPi libraries poke fixed BCM283x peripheral addresses through /dev/mem
 * and silently misbehave on the Pi 5's RP1 I/O controller.
 *
 * Lines used:
 *   - ADC DRDY  : input  (ADS1256 asserts low when a conversion is ready)
 *   - ADC RESET : output (drive low to reset the ADS1256)
 *   - ADC CS    : output (drive low to select the ADS1256 on the SPI bus)
 *   - DAC CS    : output (drive low to select the DAC8552 on the SPI bus)
 * ==========================================================================*/

#ifndef GPIO_H
#define GPIO_H

/* Open the GPIO chip and request all four lines.
 * Outputs start de-asserted (CS high, RESET high). Returns 0 or -1. */
int  gpio_init(void);

/* Release the lines and close the chip. */
void gpio_close(void);

/* Returns 1 if DRDY is currently low (conversion ready), 0 if high,
 * -1 on error. */
int  gpio_drdy_is_low(void);

/* Chip-select control. active = 1 drives the line LOW (chip selected);
 * active = 0 drives it HIGH (chip released). */
void gpio_adc_cs(int active);
void gpio_dac_cs(int active);

/* ADS1256 hardware reset line. asserted = 1 drives RESET LOW. */
void gpio_adc_reset(int asserted);

#endif /* GPIO_H */
