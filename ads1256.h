/* ============================================================================
 * ads1256.h  --  Driver for the TI ADS1256 24-bit sigma-delta ADC
 *
 * The ADS1256 is run in CONTINUOUS-READ mode (RDATAC) on a single fixed
 * channel: after ads1256_init() each new conversion can be read with three
 * SPI bytes and no command overhead. This is the lowest-latency way to track
 * a continuous analog input with this part.
 *
 * Latency note: the ADS1256 is a *precision* converter, not a *fast* one.
 * Its sigma-delta decimation filter imposes a group delay, and SCLK is capped
 * at 1.92 MHz so a 3-byte sample read alone takes ~12.5 us on the wire. Do
 * not expect this part to support a 10 us analog-to-analog budget -- measure
 * and see (that measurement is the point of the experiment).
 * ==========================================================================*/

#ifndef ADS1256_H
#define ADS1256_H

#include <stdint.h>

/* Reset the ADS1256, configure it (channel, data rate, gain from config.h),
 * self-calibrate, and enter continuous-read mode. Returns 0 or -1.
 * `spi_fd` is an open spidev descriptor (see spidev_util.h).
 * gpio_init() must have been called first. */
int ads1256_init(int spi_fd);

/* Block until DRDY is asserted (a fresh conversion is ready).
 * Returns 0 on success, -1 on timeout/error. */
int ads1256_wait_drdy(void);

/* Read one 24-bit sample (RDATAC mode -- three bytes, no command).
 * Returns the value sign-extended into an int32_t. Does NOT wait for DRDY;
 * call ads1256_wait_drdy() first if you want one fresh sample per call. */
int32_t ads1256_read_sample(int spi_fd);

/* Convert a raw ADS1256 code to volts (PGA = 1). For display only. */
double ads1256_to_volts(int32_t raw);

#endif /* ADS1256_H */
