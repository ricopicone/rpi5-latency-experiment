/* ============================================================================
 * spidev_util.h  --  Thin spidev wrapper (header-only)
 *
 * The ADS1256 and DAC8552 share one SPI bus. We open it once in "no hardware
 * chip-select" mode (SPI_NO_CS) because the Waveshare board routes each chip's
 * CS to a plain GPIO; CS is toggled by gpio.c, not the SPI controller.
 *
 * spi_xfer() takes a per-call clock rate, so the slow ADS1256 (<= 1.92 MHz)
 * and the fast DAC8552 (15.6 MHz here) can share the same file descriptor.
 * ==========================================================================*/

#ifndef SPIDEV_UTIL_H
#define SPIDEV_UTIL_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include "config.h"

/* Open the SPI bus, set mode + word size. Returns fd, or -1 on error. */
static inline int spi_open(const char *dev)
{
    int fd = open(dev, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "spi: cannot open %s: %s\n", dev, strerror(errno));
        return -1;
    }

    /* SPI_NO_CS: ask the controller not to touch any CE pin (we own CS via
     * GPIO). Some drivers -- notably the Pi 5's RP1 spidev -- reject the
     * SPI_NO_CS bit through SPI_IOC_WR_MODE32 with EINVAL. Fall back to the
     * 8-bit SPI_IOC_WR_MODE in that case: the controller will then toggle
     * its own CE0 line on every transfer, but on this Waveshare HAT CE0
     * (GPIO8) is unused, so that toggling has no effect on the ADS1256 or
     * the DAC8552 -- both still see the GPIO22/GPIO23 CS we drive ourselves. */
    uint32_t mode32 = SPI_MODE | SPI_NO_CS;
    uint8_t  bits   = SPI_BITS_PER_WORD;

    if (ioctl(fd, SPI_IOC_WR_MODE32, &mode32) < 0) {
        uint8_t mode8 = SPI_MODE;
        if (ioctl(fd, SPI_IOC_WR_MODE, &mode8) < 0) {
            fprintf(stderr, "spi: SPI_IOC_WR_MODE: %s\n", strerror(errno));
            close(fd);
            return -1;
        }
        fprintf(stderr,
            "spi: note: kernel rejected SPI_NO_CS; controller CE0 will toggle\n"
            "     (harmless on this HAT -- chip selects are on GPIO22/23).\n");
    }
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        fprintf(stderr, "spi: SPI_IOC_WR_BITS_PER_WORD: %s\n", strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}

/* Full-duplex transfer of `len` bytes at clock `hz`.
 * `tx` or `rx` may be NULL (for read-only or write-only transfers).
 * Returns 0 on success, -1 on error. */
static inline int spi_xfer(int fd, const uint8_t *tx, uint8_t *rx,
                           size_t len, uint32_t hz)
{
    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof(tr));
    tr.tx_buf        = (uint64_t)(uintptr_t)tx;
    tr.rx_buf        = (uint64_t)(uintptr_t)rx;
    tr.len           = (uint32_t)len;
    tr.speed_hz      = hz;
    tr.bits_per_word = SPI_BITS_PER_WORD;

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        fprintf(stderr, "spi: SPI_IOC_MESSAGE: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

#endif /* SPIDEV_UTIL_H */
