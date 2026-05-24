/* ============================================================================
 * latency_loop_p3.c -- Phase 3 of the §6 feasibility plan.
 *
 * Full kernel-bypass loop. Direct mmap of:
 *   - RP1 SPI0 peripheral (Designware APB SSI registers) for the SPI bus
 *   - RP1 SYS_RIO for GPIO (via gpio_mmap.c, same as Phase 2)
 *
 * We open /dev/spidev0.0 once and hold it open -- this claims the SPI pins
 * (GPIO 9/10/11) at their ALT function and keeps the clock running -- but
 * we never call any spidev ioctl; the hot path talks to the controller's
 * registers directly.
 *
 * Reference: spi_rp1.h for the DesignWare APB SSI register layout.
 *
 * Run: sudo ./latency_loop_p3 --mode characterize -n 50000
 * ==========================================================================*/

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/spi/spidev.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "gpio.h"
#include "ads1256.h"   /* for one-time init; hot path is local */
#include "dac8552.h"   /* same */
#include "spi_rp1.h"

/* ----- module state for the SPI MMIO path -------------------------------- */
static int   g_spidev_fd  = -1;
static int   g_devmem_fd  = -1;
static void *g_spi_map    = MAP_FAILED;

static uint32_t g_baudr_adc;   /* divisor for ~1.92 MHz */
static uint32_t g_baudr_dac;   /* divisor for ~14 MHz   */

static inline void spi_disable(void) { R(g_spi_map, DW_SSIENR) = 0; }
static inline void spi_enable (void) { R(g_spi_map, DW_SSIENR) = 1; }

/* Configure the controller for an N-frame transfer at the given BAUDR and
 * transfer mode. Must be called while SSI is disabled. */
static inline void spi_setup(uint32_t baudr, uint32_t ctrlr0, uint32_t n_frames)
{
    R(g_spi_map, DW_BAUDR ) = baudr;
    R(g_spi_map, DW_CTRLR0) = ctrlr0;
    R(g_spi_map, DW_CTRLR1) = n_frames - 1;   /* N-1 for RO mode */
    R(g_spi_map, DW_SER   ) = 1;              /* enable slave 0 (CE0 toggles; HAT ignores) */
}

/* Read 3 bytes from the ADS1256 via direct register access.
 * Uses RX-only mode (TMOD=RO) at the ADC's BAUDR.
 *
 * In RO mode: write CTRLR1 = N-1, enable SSI, write one dummy byte to DR to
 * start the transfer (the controller clocks N frames automatically); then
 * pop N bytes from DR as they arrive. */
static inline int32_t spi_read_adc_3bytes(void)
{
    spi_disable();
    spi_setup(g_baudr_adc, DW_CTRLR0_RX_8BIT_MODE1, 3);
    spi_enable();

    R(g_spi_map, DW_DR) = 0;  /* dummy write triggers receive in RO mode */

    /* Wait for all 3 bytes to arrive in the RX FIFO. */
    while (R(g_spi_map, DW_RXFLR) < 3) { /* spin */ }

    uint32_t b0 = R(g_spi_map, DW_DR) & 0xFFu;
    uint32_t b1 = R(g_spi_map, DW_DR) & 0xFFu;
    uint32_t b2 = R(g_spi_map, DW_DR) & 0xFFu;

    int32_t v = (int32_t)((b0 << 16) | (b1 << 8) | b2);
    if (v & 0x00800000) v |= (int32_t)0xFF000000;
    return v;
}

/* Write 3 bytes to the DAC8552 via direct register access.
 * Uses TX-only mode (TMOD=TO) at the DAC's BAUDR. */
static inline void spi_write_dac_3bytes(uint8_t b0, uint8_t b1, uint8_t b2)
{
    spi_disable();
    spi_setup(g_baudr_dac, DW_CTRLR0_TX_8BIT_MODE1, 3);
    spi_enable();

    R(g_spi_map, DW_DR) = b0;
    R(g_spi_map, DW_DR) = b1;
    R(g_spi_map, DW_DR) = b2;

    /* Wait for the transfer to finish so the DAC's CS-high (released by the
     * caller after this returns) lands AFTER the last bit is on the wire. */
    while ( R(g_spi_map, DW_SR) & DW_SR_BUSY ) { /* spin */ }
}

/* ----- realtime + scaffolding (same as latency_loop_2fd) ----------------- */
enum run_mode { MODE_PASSTHROUGH, MODE_CHARACTERIZE };

static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int sig) { (void)sig; g_stop = 1; }

static inline int64_t ns_between(const struct timespec *a,
                                 const struct timespec *b)
{
    return (int64_t)(b->tv_sec  - a->tv_sec ) * 1000000000LL
         + (int64_t)(b->tv_nsec - a->tv_nsec);
}

static void setup_realtime(int core)
{
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) perror("warn: mlockall");
    struct sched_param sp = { .sched_priority = RT_PRIORITY };
    if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0) perror("warn: SCHED_FIFO");
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(core, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0) perror("warn: sched_setaffinity");
    volatile unsigned char buf[256 * 1024];
    memset((void *)buf, 0, sizeof(buf));
}

static int cmp_i64(const void *a, const void *b)
{
    int64_t x = *(const int64_t *)a, y = *(const int64_t *)b;
    return (x > y) - (x < y);
}

static void report_stage(const char *name, const int64_t *v, size_t n)
{
    if (n == 0) { printf("  %-22s (no samples)\n", name); return; }
    int64_t *s = malloc(n * sizeof(int64_t));
    memcpy(s, v, n * sizeof(int64_t));
    qsort(s, n, sizeof(int64_t), cmp_i64);
    long double sum = 0;
    for (size_t i = 0; i < n; i++) sum += (long double)v[i];
    const double k = 1e-3;
    printf("  %-22s min %8.2f  med %8.2f  p90 %8.2f  p99 %8.2f  "
           "max %9.2f  mean %8.2f\n", name,
           s[0]*k, s[n/2]*k, s[(n*90)/100]*k, s[(n*99)/100]*k,
           s[n-1]*k, (double)(sum/n)*k);
    free(s);
}

/* ----- DRDY polling using mmap'd GPIO (re-uses gpio_mmap.c's pointers) --- */
extern int gpio_drdy_is_low(void);

static int wait_drdy(void)
{
    for (long i = 0; i < 20000000L; i++) {
        int s = gpio_drdy_is_low();
        if (s < 0) return -1;
        if (s == 1) return 0;
    }
    fprintf(stderr, "phase3: DRDY timeout\n");
    return -1;
}

/* ----- main -------------------------------------------------------------- */
static void usage(const char *p)
{
    printf("Usage: %s [-m passthrough|characterize] [-n N] [-c CSV] [--core N] [--no-rt]\n", p);
}

int main(int argc, char **argv)
{
    enum run_mode mode = MODE_PASSTHROUGH;
    size_t iterations = 50000;
    int core = RT_CPU_CORE, use_rt = 1;
    const char *csv_path = NULL;

    static struct option opts[] = {
        {"mode", required_argument, 0, 'm'},
        {"iterations", required_argument, 0, 'n'},
        {"csv", required_argument, 0, 'c'},
        {"core", required_argument, 0, 1},
        {"no-rt", no_argument, 0, 2},
        {"help", no_argument, 0, 'h'},
        {0,0,0,0}
    };
    int o;
    while ((o = getopt_long(argc, argv, "m:n:c:h", opts, NULL)) != -1) {
        switch (o) {
        case 'm':
            if (!strcmp(optarg,"passthrough"))   mode = MODE_PASSTHROUGH;
            else if (!strcmp(optarg,"characterize")) mode = MODE_CHARACTERIZE;
            else { fprintf(stderr,"bad mode\n"); return 1; }
            break;
        case 'n': iterations = strtoul(optarg, NULL, 10); break;
        case 'c': csv_path = optarg; break;
        case  1 : core = atoi(optarg); break;
        case  2 : use_rt = 0; break;
        case 'h': usage(argv[0]); return 0;
        default : usage(argv[0]); return 1;
        }
    }
    if (iterations == 0) iterations = 1;

    /* Compute BAUDR divisors. The controller forces BAUDR even, and the
     * effective SCK = SSI_CLK / BAUDR. For the ADS1256 we need <= 1.92 MHz,
     * so BAUDR >= 200/1.92 ≈ 104.17 -- use 106 for headroom (1.887 MHz). */
    g_baudr_adc = 106;
    g_baudr_dac = 14;   /* 200/14 = 14.29 MHz, well within DAC8552 spec */

    printf("=== latency_loop_p3 (Phase 3: mmap'd SPI registers) ===\n");
    printf("  spi peripheral: 0x%lx (RP1 SPI0), SSI_CLK=200 MHz\n", RP1_SPI0_PHYS);
    printf("  ADC BAUDR=%u  -> SCK=%.3f MHz\n", g_baudr_adc, 200.0/g_baudr_adc);
    printf("  DAC BAUDR=%u  -> SCK=%.3f MHz\n", g_baudr_dac, 200.0/g_baudr_dac);
    printf("  mode: %s\n\n", mode==MODE_PASSTHROUGH ? "passthrough" : "characterize");

    int64_t *t_adc  = calloc(iterations, sizeof(int64_t));
    int64_t *t_dac  = calloc(iterations, sizeof(int64_t));
    int64_t *t_proc = calloc(iterations, sizeof(int64_t));
    int64_t *t_loop = calloc(iterations, sizeof(int64_t));
    int32_t *v_raw  = csv_path ? calloc(iterations, sizeof(int32_t)) : NULL;
    if (!t_adc || !t_dac || !t_proc || !t_loop) {
        fprintf(stderr,"oom\n"); return 1;
    }

    if (use_rt) setup_realtime(core);

    /* Open /dev/spidev0.0 just to claim SPI pins + keep clock running.
     * We won't call any spidev ioctl after this; all transfers go through
     * direct register access. */
    g_spidev_fd = open(SPI_DEVICE, O_RDWR);
    if (g_spidev_fd < 0) { perror("open spidev"); return 1; }
    /* Do one trivial speed set so the kernel finalizes pinmux for SPI0. */
    uint32_t hz = ADS1256_SPI_HZ;
    ioctl(g_spidev_fd, SPI_IOC_WR_MAX_SPEED_HZ, &hz);

    /* mmap the SPI0 peripheral. */
    g_devmem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (g_devmem_fd < 0) { perror("open /dev/mem"); return 1; }
    g_spi_map = mmap(NULL, RP1_SPI0_SIZE, PROT_READ | PROT_WRITE,
                     MAP_SHARED, g_devmem_fd, (off_t)RP1_SPI0_PHYS);
    if (g_spi_map == MAP_FAILED) { perror("mmap SPI0"); return 1; }

    /* GPIO via mmap (gpio_mmap.c handles pinctrl + mapping). */
    if (gpio_init() != 0) return 1;

    /* Initial controller state: disable, set up something benign. */
    spi_disable();
    spi_setup(g_baudr_adc, DW_CTRLR0_RX_8BIT_MODE1, 1);

    /* ADS1256 init + RDATAC needs a working SPI path -- bring it up using
     * a one-time call into the parent driver, which goes through spidev's
     * machinery. After this call returns we go back to mmap. */
    if (ads1256_init(g_spidev_fd) != 0) { gpio_close(); return 1; }
    dac8552_init(g_spidev_fd);
    /* The driver init paths likely left CTRLR0/BAUDR pointing at the last
     * speed set by spidev. Reset to our defaults before the hot loop. */
    spi_disable();
    printf("\n");

    signal(SIGINT, on_sigint);
    if (mode == MODE_PASSTHROUGH) printf("Passthrough -- Ctrl-C to stop.\n\n");
    else printf("Characterizing %zu iterations...\n\n", iterations);

    /* ---- HOT LOOP ----------------------------------------------------- */
    struct timespec ta, tb, tc, td, td_prev;
    clock_gettime(CLOCK_MONOTONIC, &td_prev);

    size_t recorded = 0;
    for (size_t i = 0; !g_stop; i++) {
        if (mode == MODE_CHARACTERIZE && i >= iterations) break;

        clock_gettime(CLOCK_MONOTONIC, &ta);
        if (wait_drdy() != 0) { g_stop = 1; break; }
        clock_gettime(CLOCK_MONOTONIC, &tb);

        /* ADC read via direct register access. */
        gpio_adc_cs(1);
        int32_t raw = spi_read_adc_3bytes();
        gpio_adc_cs(0);
        clock_gettime(CLOCK_MONOTONIC, &tc);

        /* DAC write via direct register access. */
        uint16_t code = (uint16_t)(((raw >> 8) & 0xFFFF) ^ 0x8000);
        gpio_dac_cs(1);
        spi_write_dac_3bytes(0x30, (uint8_t)(code >> 8), (uint8_t)(code & 0xFF));
        gpio_dac_cs(0);
        clock_gettime(CLOCK_MONOTONIC, &td);

        if (recorded < iterations) {
            t_adc [recorded] = ns_between(&tb, &tc);
            t_dac [recorded] = ns_between(&tc, &td);
            t_proc[recorded] = ns_between(&tb, &td);
            t_loop[recorded] = ns_between(&td_prev, &td);
            if (v_raw) v_raw[recorded] = raw;
            recorded++;
        }
        td_prev = td;
    }

    printf("\nStopped after %zu iterations.\n\n", recorded);
    printf("Timing breakdown (microseconds):\n");
    report_stage("ADC read (DRDY->ADC)", t_adc,  recorded);
    report_stage("DAC write (ADC->DAC)", t_dac,  recorded);
    report_stage("processing latency",   t_proc, recorded);
    report_stage("loop period",          t_loop, recorded);

    if (csv_path && v_raw) {
        FILE *f = fopen(csv_path, "w");
        if (!f) perror("warn: cannot open CSV");
        else {
            fprintf(f, "index,adc_raw,adc_volts,adc_read_ns,dac_write_ns,proc_ns,loop_ns\n");
            for (size_t i = 0; i < recorded; i++)
                fprintf(f, "%zu,%d,%.6f,%ld,%ld,%ld,%ld\n", i, v_raw[i],
                        ads1256_to_volts(v_raw[i]),
                        (long)t_adc[i], (long)t_dac[i],
                        (long)t_proc[i], (long)t_loop[i]);
            fclose(f);
            printf("\nWrote %zu rows to %s\n", recorded, csv_path);
        }
    }

    spi_disable();
    munmap(g_spi_map, RP1_SPI0_SIZE);
    close(g_devmem_fd);
    close(g_spidev_fd);
    gpio_close();
    free(t_adc); free(t_dac); free(t_proc); free(t_loop); free(v_raw);
    return 0;
}
