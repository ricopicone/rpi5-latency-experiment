/* ============================================================================
 * gpio.c  --  GPIO character-device uAPI v2 implementation
 *
 * Two line requests are made on the GPIO chip:
 *   - input  request:  [ DRDY ]
 *   - output request:  [ ADC_RESET, ADC_CS, DAC_CS ]   (indices 0, 1, 2)
 *
 * Each request yields its own file descriptor; per-line reads/writes go
 * through GPIO_V2_LINE_{GET,SET}_VALUES_IOCTL on those descriptors.
 * ==========================================================================*/

#include "gpio.h"
#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>

/* Indices of the three output lines within the output line request. */
enum { OUT_RESET = 0, OUT_ADC_CS = 1, OUT_DAC_CS = 2, OUT_COUNT = 3 };

static int g_chip_fd = -1;   /* /dev/gpiochipN                              */
static int g_in_fd   = -1;   /* line-request fd for DRDY (input)            */
static int g_out_fd  = -1;   /* line-request fd for RESET/ADC_CS/DAC_CS      */

/* Try the configured chip first, then the Pi 5's alternate node. */
static int chip_open(void)
{
    const char *candidates[] = { GPIO_CHIP, "/dev/gpiochip4", "/dev/gpiochip0" };
    for (unsigned i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        int fd = open(candidates[i], O_RDWR | O_CLOEXEC);
        if (fd >= 0) {
            if (i != 0)
                fprintf(stderr, "gpio: note: using %s (config said %s)\n",
                        candidates[i], GPIO_CHIP);
            return fd;
        }
    }
    fprintf(stderr, "gpio: cannot open a GPIO chip: %s\n", strerror(errno));
    return -1;
}

int gpio_init(void)
{
    g_chip_fd = chip_open();
    if (g_chip_fd < 0)
        return -1;

    /* ---- input request: DRDY ------------------------------------------- */
    struct gpio_v2_line_request in_req;
    memset(&in_req, 0, sizeof(in_req));
    in_req.offsets[0]   = LINE_ADC_DRDY;
    in_req.num_lines    = 1;
    in_req.config.flags = GPIO_V2_LINE_FLAG_INPUT;
    strncpy(in_req.consumer, "addal-drdy", sizeof(in_req.consumer) - 1);

    if (ioctl(g_chip_fd, GPIO_V2_GET_LINE_IOCTL, &in_req) < 0) {
        fprintf(stderr, "gpio: request DRDY (line %d): %s\n",
                LINE_ADC_DRDY, strerror(errno));
        gpio_close();
        return -1;
    }
    g_in_fd = in_req.fd;

    /* ---- output request: RESET, ADC_CS, DAC_CS ------------------------- */
    struct gpio_v2_line_request out_req;
    memset(&out_req, 0, sizeof(out_req));
    out_req.offsets[OUT_RESET]  = LINE_ADC_RESET;
    out_req.offsets[OUT_ADC_CS] = LINE_ADC_CS;
    out_req.offsets[OUT_DAC_CS] = LINE_DAC_CS;
    out_req.num_lines           = OUT_COUNT;
    out_req.config.flags        = GPIO_V2_LINE_FLAG_OUTPUT;
    strncpy(out_req.consumer, "addal-ctl", sizeof(out_req.consumer) - 1);

    /* Initial output values: every line HIGH (CS released, RESET inactive). */
    out_req.config.num_attrs            = 1;
    out_req.config.attrs[0].attr.id     = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
    out_req.config.attrs[0].attr.values = 0x7;   /* bits 0,1,2 = 1 */
    out_req.config.attrs[0].mask        = 0x7;

    if (ioctl(g_chip_fd, GPIO_V2_GET_LINE_IOCTL, &out_req) < 0) {
        fprintf(stderr, "gpio: request output lines: %s\n", strerror(errno));
        gpio_close();
        return -1;
    }
    g_out_fd = out_req.fd;

    return 0;
}

void gpio_close(void)
{
    if (g_in_fd  >= 0) { close(g_in_fd);  g_in_fd  = -1; }
    if (g_out_fd >= 0) { close(g_out_fd); g_out_fd = -1; }
    if (g_chip_fd >= 0) { close(g_chip_fd); g_chip_fd = -1; }
}

int gpio_drdy_is_low(void)
{
    struct gpio_v2_line_values v;
    v.mask = 0x1;   /* DRDY is at index 0 of the input request */
    v.bits = 0;
    if (ioctl(g_in_fd, GPIO_V2_LINE_GET_VALUES_IOCTL, &v) < 0)
        return -1;
    return (v.bits & 0x1) ? 0 : 1;   /* low => conversion ready */
}

/* Set one output line (by request index) to `level` (0 or 1). */
static void out_set(unsigned idx, unsigned level)
{
    struct gpio_v2_line_values v;
    v.mask = (uint64_t)1 << idx;
    v.bits = (uint64_t)(level ? 1 : 0) << idx;
    (void)ioctl(g_out_fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &v);
}

void gpio_adc_cs(int active)   { out_set(OUT_ADC_CS, active ? 0 : 1); }
void gpio_dac_cs(int active)   { out_set(OUT_DAC_CS, active ? 0 : 1); }
void gpio_adc_reset(int asrt)  { out_set(OUT_RESET,  asrt   ? 0 : 1); }
