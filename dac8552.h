/* ============================================================================
 * dac8552.h  --  Driver for the TI DAC8552 16-bit dual DAC
 *
 * Only channel A is used. Each write is a single 24-bit SPI frame
 * (8 control bits + 16 data bits) that updates the output immediately.
 * The DAC8552 is fast: at 15.6 MHz a frame is on the wire in ~1.5 us, so
 * it is not the latency bottleneck in this experiment -- the ADS1256 is.
 * ==========================================================================*/

#ifndef DAC8552_H
#define DAC8552_H

#include <stdint.h>

/* Bring the DAC to a known state (channel A at mid-scale). Returns 0. */
int dac8552_init(int spi_fd);

/* Write a 16-bit code to channel A and update the output now.
 * 0x0000 -> ~0 V, 0xFFFF -> ~V_REF. */
void dac8552_write_a(int spi_fd, uint16_t code);

#endif /* DAC8552_H */
