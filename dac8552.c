/* ============================================================================
 * dac8552.c  --  TI DAC8552 driver implementation
 * ==========================================================================*/

#include "dac8552.h"
#include "config.h"
#include "gpio.h"
#include "spidev_util.h"

/* DAC8552 control byte (bits 23:16 of the 24-bit frame).
 * 0x30 = LDA + LDB set, buffer-select = A:  write the following 16 bits to
 * input buffer A and load both DAC latches, so output A updates immediately.
 * (Channel B would be 0x34. This experiment uses channel A only.) */
#define DAC8552_CTRL_A   0x30

int dac8552_init(int fd)
{
    dac8552_write_a(fd, 0x8000);   /* mid-scale */
    return 0;
}

void dac8552_write_a(int fd, uint16_t code)
{
    uint8_t tx[3] = {
        DAC8552_CTRL_A,
        (uint8_t)(code >> 8),
        (uint8_t)(code & 0xFF)
    };

    gpio_dac_cs(1);                                   /* SYNC low           */
    spi_xfer(fd, tx, NULL, 3, DAC8552_SPI_HZ);
    gpio_dac_cs(0);                                   /* SYNC high -> load  */
}
