/* ============================================================================
 * gpio_mmap.c  --  drop-in replacement for ../gpio.c that uses direct mmap
 *                  access to the RP1 SYS_RIO peripheral.
 *
 * Phase 2 of the §6 feasibility plan. Same gpio.h API as the original, so
 * ../latency_loop.c links unchanged. The only thing that changes is which
 * gpio.c is compiled in.
 *
 * Reference for register layout: drivers/pinctrl/pinctrl-rp1.c in the
 * raspberrypi/linux kernel tree (rpi-6.12.y). Per-bank descriptors give
 * gpio_offset / rio_offset / pads_offset; per-region alias offsets give the
 * fast SET/CLR atomic-write paths.
 *
 * Pin configuration (FUNCSEL=SIO, OEOVER, pad pull, drive strength) is
 * delegated to the kernel pinctrl driver at init time via the `pinctrl`
 * command-line utility. After that, the hot path is just mmap'd 32-bit
 * register writes -- no syscalls, no ioctls.
 * ==========================================================================*/

#define _GNU_SOURCE
#include "gpio.h"        /* shared API; same file the original gpio.c uses */
#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/* ----- RP1 register layout (from pinctrl-rp1.c, bank 0 = GPIO0..27) ------ */
#define RP1_RIO_BASE_PHYS  0x1f000e0000UL
#define RP1_REGION_SIZE    0x0000c000UL    /* 48 KB; covers all 3 banks      */

#define RP1_RW_OFFSET      0x0000u
#define RP1_XOR_OFFSET     0x1000u
#define RP1_SET_OFFSET     0x2000u
#define RP1_CLR_OFFSET     0x3000u

#define BANK0_RIO_OFFSET   0x0000u

#define RIO_OUT            0x00u
#define RIO_OE             0x04u
#define RIO_IN             0x08u

/* ----- module state ------------------------------------------------------ */
static int   g_devmem_fd  = -1;
static void *g_rio_map    = MAP_FAILED;

static volatile uint32_t *p_out_set;  /* SET alias  + RIO_OUT  */
static volatile uint32_t *p_out_clr;  /* CLR alias  + RIO_OUT  */
static volatile uint32_t *p_in_rd;    /* RW  alias  + RIO_IN   */

static const uint32_t bit_drdy  = 1u << LINE_ADC_DRDY;
static const uint32_t bit_reset = 1u << LINE_ADC_RESET;
static const uint32_t bit_adccs = 1u << LINE_ADC_CS;
static const uint32_t bit_daccs = 1u << LINE_DAC_CS;

/* ----- helpers ----------------------------------------------------------- */

/* Use the kernel pinctrl utility to set up each pin. We delegate pinmux to
 * the kernel because doing it manually means writing to the IO_BANK and
 * PADS regions, which conflict with the kernel's driver state. The pinctrl
 * tool exits cleanly after the change and the kernel leaves the pin alone
 * (no continuous owner), so our subsequent register writes are uncontested. */
static int pinctrl_run(const char *args)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "pinctrl set %s >/dev/null 2>&1", args);
    int rc = system(cmd);
    if (rc != 0) {
        fprintf(stderr, "gpio_mmap: 'pinctrl set %s' failed (rc=%d)\n", args, rc);
        return -1;
    }
    return 0;
}

/* ----- API --------------------------------------------------------------- */

int gpio_init(void)
{
    /* All four pins must be on bank 0 (GPIO 0..27). */
    if (LINE_ADC_DRDY  > 27 || LINE_ADC_RESET > 27 ||
        LINE_ADC_CS    > 27 || LINE_DAC_CS    > 27) {
        fprintf(stderr, "gpio_mmap: pins must be on RP1 bank 0 (0..27)\n");
        return -1;
    }

    /* Outputs start de-asserted (CS high, RESET high == not asserted). */
    char buf[32];
    snprintf(buf, sizeof(buf), "%d op pn dh", LINE_ADC_RESET);
    if (pinctrl_run(buf) < 0) return -1;
    snprintf(buf, sizeof(buf), "%d op pn dh", LINE_ADC_CS);
    if (pinctrl_run(buf) < 0) return -1;
    snprintf(buf, sizeof(buf), "%d op pn dh", LINE_DAC_CS);
    if (pinctrl_run(buf) < 0) return -1;
    snprintf(buf, sizeof(buf), "%d ip pn",   LINE_ADC_DRDY);
    if (pinctrl_run(buf) < 0) return -1;

    /* Map the RIO peripheral. /dev/mem requires root (sudo). */
    g_devmem_fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC);
    if (g_devmem_fd < 0) {
        fprintf(stderr, "gpio_mmap: open /dev/mem: %s   (sudo required)\n",
                strerror(errno));
        return -1;
    }
    g_rio_map = mmap(NULL, RP1_REGION_SIZE, PROT_READ | PROT_WRITE,
                     MAP_SHARED, g_devmem_fd, (off_t)RP1_RIO_BASE_PHYS);
    if (g_rio_map == MAP_FAILED) {
        fprintf(stderr, "gpio_mmap: mmap RIO @ 0x%lx: %s\n",
                RP1_RIO_BASE_PHYS, strerror(errno));
        close(g_devmem_fd); g_devmem_fd = -1;
        return -1;
    }

    p_out_set = (volatile uint32_t *)
        ((uintptr_t)g_rio_map + RP1_SET_OFFSET + BANK0_RIO_OFFSET + RIO_OUT);
    p_out_clr = (volatile uint32_t *)
        ((uintptr_t)g_rio_map + RP1_CLR_OFFSET + BANK0_RIO_OFFSET + RIO_OUT);
    p_in_rd   = (volatile uint32_t *)
        ((uintptr_t)g_rio_map + RP1_RW_OFFSET  + BANK0_RIO_OFFSET + RIO_IN);

    /* All three outputs start HIGH (CS released, RESET inactive). */
    *p_out_set = bit_reset | bit_adccs | bit_daccs;

    return 0;
}

void gpio_close(void)
{
    if (g_rio_map != MAP_FAILED) {
        munmap(g_rio_map, RP1_REGION_SIZE);
        g_rio_map = MAP_FAILED;
    }
    if (g_devmem_fd >= 0) {
        close(g_devmem_fd);
        g_devmem_fd = -1;
    }
}

int gpio_drdy_is_low(void)
{
    /* DRDY asserted (conversion ready) means the input pin reads LOW. */
    return (*p_in_rd & bit_drdy) ? 0 : 1;
}

/* CS active (== chip selected) means the pin is driven LOW (we use CLR);
 * CS inactive means the pin is driven HIGH (we use SET). */
void gpio_adc_cs(int active)
{
    if (active) *p_out_clr = bit_adccs;
    else        *p_out_set = bit_adccs;
}

void gpio_dac_cs(int active)
{
    if (active) *p_out_clr = bit_daccs;
    else        *p_out_set = bit_daccs;
}

void gpio_adc_reset(int asserted)
{
    /* RESET asserted means the pin is driven LOW. */
    if (asserted) *p_out_clr = bit_reset;
    else          *p_out_set = bit_reset;
}
