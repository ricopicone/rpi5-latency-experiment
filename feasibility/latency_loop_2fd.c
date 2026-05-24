/* ============================================================================
 * latency_loop_2fd.c -- Phase 2.5 of the §6 feasibility plan.
 *
 * Question: how much of the ~14 us ADC and ~17 us DAC `spidev` per-call cost
 * is the per-transfer clock-rate prescaler reconfiguration (the ADS1256 runs
 * at 1.92 MHz and the DAC8552 at 15.6 MHz, and the current loop alternates
 * between them on every iteration)?
 *
 * Method: open `/dev/spidev0.0` TWICE -- once for the ADC, once for the DAC.
 * Set each FD's max-speed permanently at init via `SPI_IOC_WR_MAX_SPEED_HZ`,
 * and in the hot path omit `tr.speed_hz` (== 0) so the driver uses the FD
 * default. The kernel should then not reconfigure the prescaler on every
 * transfer. Everything else is identical to `latency_loop_mmap`: gpio_mmap.c
 * for fast GPIO, ads1256.c for init (one-time, doesn't matter which FD).
 *
 * Build: this file is built into `latency_loop_2fd` by feasibility/Makefile.
 * Run:   sudo ./latency_loop_2fd --mode characterize -n 50000
 *
 * If the software median drops materially below 45.83 us (the Phase 2 number),
 * then a significant chunk of spidev's cost is the prescaler reconfig and
 * the path to Phase 3 looks even better. If it doesn't drop much, then the
 * residual is genuine kernel SPI driver overhead, and Phase 3's full bypass
 * is the only remaining lever.
 * ==========================================================================*/

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/spi/spidev.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "gpio.h"
#include "ads1256.h"
#include "dac8552.h"

/* ---- helpers ----------------------------------------------------------- */
enum run_mode { MODE_PASSTHROUGH, MODE_CHARACTERIZE };

static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int sig) { (void)sig; g_stop = 1; }

static inline int64_t ns_between(const struct timespec *a,
                                 const struct timespec *b)
{
    return (int64_t)(b->tv_sec  - a->tv_sec ) * 1000000000LL
         + (int64_t)(b->tv_nsec - a->tv_nsec);
}

/* Open /dev/spidev0.0 with mode + bits, and set its DEFAULT max speed once.
 * Subsequent spi_xfer_fixed() calls will use that speed because we'll pass
 * tr.speed_hz = 0 in the transfer struct. */
static int spi_open_fixed(const char *dev, uint32_t hz)
{
    int fd = open(dev, O_RDWR);
    if (fd < 0) { perror("open spidev"); return -1; }
    uint32_t mode32 = SPI_MODE | SPI_NO_CS;
    if (ioctl(fd, SPI_IOC_WR_MODE32, &mode32) < 0) {
        uint8_t mode8 = SPI_MODE;
        if (ioctl(fd, SPI_IOC_WR_MODE, &mode8) < 0) {
            perror("SPI_IOC_WR_MODE"); close(fd); return -1;
        }
    }
    uint8_t bits = SPI_BITS_PER_WORD;
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        perror("SPI_IOC_WR_BITS_PER_WORD"); close(fd); return -1;
    }
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &hz) < 0) {
        perror("SPI_IOC_WR_MAX_SPEED_HZ"); close(fd); return -1;
    }
    return fd;
}

/* Transfer N bytes using the FD's pre-set speed -- speed_hz omitted. */
static inline int spi_xfer_fixed(int fd, const uint8_t *tx, uint8_t *rx,
                                 unsigned len)
{
    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof(tr));
    tr.tx_buf = (uint64_t)(uintptr_t)tx;
    tr.rx_buf = (uint64_t)(uintptr_t)rx;
    tr.len    = len;
    tr.bits_per_word = SPI_BITS_PER_WORD;
    /* tr.speed_hz left at 0 -> driver uses the FD's max_speed_hz set above. */
    return ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
}

/* ---- realtime setup ---------------------------------------------------- */
static void setup_realtime(int core)
{
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
        perror("warn: mlockall");
    struct sched_param sp = { .sched_priority = RT_PRIORITY };
    if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0)
        perror("warn: SCHED_FIFO");
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(core, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0)
        perror("warn: sched_setaffinity");
    volatile unsigned char buf[256 * 1024];
    memset((void *)buf, 0, sizeof(buf));
}

/* ---- statistics -------------------------------------------------------- */
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

static void usage(const char *prog)
{
    printf(
"Usage: %s [options]\n"
"  -m, --mode MODE       passthrough (default) or characterize\n"
"  -n, --iterations N    samples to record (default 50000)\n"
"  -c, --csv FILE        write per-sample CSV (characterize mode)\n"
"      --core N          CPU core to pin (default %d)\n"
"      --no-rt           skip real-time scheduling\n"
"  -h, --help            this message\n",
        prog, RT_CPU_CORE);
}

/* ---- main -------------------------------------------------------------- */
int main(int argc, char **argv)
{
    enum run_mode mode = MODE_PASSTHROUGH;
    size_t  iterations = 50000;
    int     core       = RT_CPU_CORE;
    int     use_rt     = 1;
    const char *csv_path = NULL;

    static struct option opts[] = {
        {"mode",       required_argument, 0, 'm'},
        {"iterations", required_argument, 0, 'n'},
        {"csv",        required_argument, 0, 'c'},
        {"core",       required_argument, 0,  1 },
        {"no-rt",      no_argument,       0,  2 },
        {"help",       no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    int o;
    while ((o = getopt_long(argc, argv, "m:n:c:h", opts, NULL)) != -1) {
        switch (o) {
        case 'm':
            if (!strcmp(optarg, "passthrough"))       mode = MODE_PASSTHROUGH;
            else if (!strcmp(optarg, "characterize")) mode = MODE_CHARACTERIZE;
            else { fprintf(stderr, "unknown mode '%s'\n", optarg); return 1; }
            break;
        case 'n': iterations = strtoul(optarg, NULL, 10); break;
        case 'c': csv_path   = optarg;                    break;
        case  1 : core       = atoi(optarg);              break;
        case  2 : use_rt     = 0;                         break;
        case 'h': usage(argv[0]); return 0;
        default : usage(argv[0]); return 1;
        }
    }
    if (iterations == 0) iterations = 1;

    printf("=== latency_loop_2fd (Phase 2.5: two spidev FDs) ===\n");
    printf("  spi adc fd  : %s @ %.2f MHz  (default speed pre-set)\n",
           SPI_DEVICE, ADS1256_SPI_HZ / 1e6);
    printf("  spi dac fd  : %s @ %.2f MHz  (default speed pre-set)\n",
           SPI_DEVICE, DAC8552_SPI_HZ / 1e6);
    printf("  mode        : %s\n", mode==MODE_PASSTHROUGH ? "passthrough":"characterize");
    printf("  realtime    : %s\n\n", use_rt ? "SCHED_FIFO" : "disabled");

    int64_t *t_adc  = calloc(iterations, sizeof(int64_t));
    int64_t *t_dac  = calloc(iterations, sizeof(int64_t));
    int64_t *t_proc = calloc(iterations, sizeof(int64_t));
    int64_t *t_loop = calloc(iterations, sizeof(int64_t));
    int32_t *v_raw  = csv_path ? calloc(iterations, sizeof(int32_t)) : NULL;
    if (!t_adc || !t_dac || !t_proc || !t_loop) {
        fprintf(stderr, "out of memory\n"); return 1;
    }

    if (use_rt) setup_realtime(core);

    /* Open TWO FDs, each with its own fixed default speed. */
    int spi_adc = spi_open_fixed(SPI_DEVICE, ADS1256_SPI_HZ);
    int spi_dac = spi_open_fixed(SPI_DEVICE, DAC8552_SPI_HZ);
    if (spi_adc < 0 || spi_dac < 0) return 1;

    if (gpio_init() != 0) return 1;

    /* Use either FD for init -- the init path doesn't matter for hot-path
     * timing. ads1256.c's spi_xfer calls will set their own speed_hz, which
     * the driver will honor for those one-time calls. */
    if (ads1256_init(spi_adc) != 0) { gpio_close(); return 1; }
    dac8552_init(spi_dac);
    printf("\n");

    signal(SIGINT, on_sigint);
    if (mode == MODE_PASSTHROUGH)
        printf("Passthrough -- Ctrl-C to stop.\n\n");
    else
        printf("Characterizing %zu iterations...\n\n", iterations);

    /* ---- hot loop ----------------------------------------------------- */
    struct timespec ta, tb, tc, td, td_prev;
    clock_gettime(CLOCK_MONOTONIC, &td_prev);

    size_t recorded = 0;
    for (size_t i = 0; !g_stop; i++) {
        if (mode == MODE_CHARACTERIZE && i >= iterations) break;

        clock_gettime(CLOCK_MONOTONIC, &ta);
        if (ads1256_wait_drdy() != 0) { g_stop = 1; break; }
        clock_gettime(CLOCK_MONOTONIC, &tb);

        /* ADC read: 3 bytes via spi_adc FD (default 1.92 MHz). */
        uint8_t rx[3] = {0};
        gpio_adc_cs(1);
        spi_xfer_fixed(spi_adc, NULL, rx, 3);
        gpio_adc_cs(0);
        int32_t raw = ((int32_t)rx[0] << 16) | ((int32_t)rx[1] << 8) | (int32_t)rx[2];
        if (raw & 0x00800000) raw |= (int32_t)0xFF000000;
        clock_gettime(CLOCK_MONOTONIC, &tc);

        /* DAC write: 3 bytes via spi_dac FD (default 15.6 MHz). */
        uint16_t code = (uint16_t)(((raw >> 8) & 0xFFFF) ^ 0x8000);
        uint8_t tx[3] = { 0x30, (uint8_t)(code >> 8), (uint8_t)(code & 0xFF) };
        gpio_dac_cs(1);
        spi_xfer_fixed(spi_dac, tx, NULL, 3);
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

    gpio_close();
    close(spi_adc); close(spi_dac);
    free(t_adc); free(t_dac); free(t_proc); free(t_loop); free(v_raw);
    return 0;
}
