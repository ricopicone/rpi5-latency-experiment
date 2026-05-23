/* ============================================================================
 * latency_loop.c  --  Analog-to-analog feedback-control latency experiment
 *
 * Raspberry Pi 5 + Waveshare High-Precision AD/DA Board (ADS1256 + DAC8552).
 *
 * The loop samples an analog input on the ADS1256 and immediately reproduces
 * it on DAC8552 channel A. An oscilloscope comparing the function-generator
 * input against the DAC output shows the end-to-end latency as a phase shift.
 *
 *   modes:
 *     passthrough   run forever; mirror ADC -> DAC for scope measurement.
 *                   Press Ctrl-C to stop and print timing statistics.
 *     characterize  run a fixed number of iterations, then print a full
 *                   timing breakdown (and optionally a per-sample CSV).
 *
 * What the software CAN measure: the processing latency from "ADC conversion
 * ready" (DRDY) to "DAC output updated". What it CANNOT measure: the ADS1256
 * internal sigma-delta filter group delay and the DAC settling time -- those
 * appear only in the oscilloscope's input-vs-output phase shift. The true
 * analog-to-analog latency is the scope figure; the printed numbers are the
 * software component of it.
 *
 * Build:  make            Run:  sudo ./latency_loop --mode characterize
 * ==========================================================================*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <sched.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/mman.h>

#include "config.h"
#include "spidev_util.h"
#include "gpio.h"
#include "ads1256.h"
#include "dac8552.h"

/* ------------------------------------------------------------------------ */
enum run_mode { MODE_PASSTHROUGH, MODE_CHARACTERIZE };

static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int sig) { (void)sig; g_stop = 1; }

static inline int64_t ns_between(const struct timespec *a,
                                 const struct timespec *b)
{
    return (int64_t)(b->tv_sec  - a->tv_sec) * 1000000000LL
         + (int64_t)(b->tv_nsec - a->tv_nsec);
}

/* ---- real-time configuration ------------------------------------------- */
/* None of these are fatal: without privilege the program still runs, just
 * with more scheduling jitter. That itself is a useful thing to measure. */
static void setup_realtime(int core)
{
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
        perror("warn: mlockall (run with sudo for best results)");

    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = RT_PRIORITY;
    if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0)
        perror("warn: sched_setscheduler SCHED_FIFO");

    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(core, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0)
        perror("warn: sched_setaffinity");

    /* Pre-fault the stack so no page fault occurs inside the loop. */
    volatile unsigned char buf[256 * 1024];
    memset((void *)buf, 0, sizeof(buf));
}

/* ---- statistics -------------------------------------------------------- */
static int cmp_i64(const void *a, const void *b)
{
    int64_t x = *(const int64_t *)a, y = *(const int64_t *)b;
    return (x > y) - (x < y);
}

/* Sort a copy of `v` and print a percentile row in microseconds. */
static void report_stage(const char *name, const int64_t *v, size_t n)
{
    if (n == 0) { printf("  %-22s (no samples)\n", name); return; }

    int64_t *s = malloc(n * sizeof(int64_t));
    if (!s) { fprintf(stderr, "out of memory\n"); return; }
    memcpy(s, v, n * sizeof(int64_t));
    qsort(s, n, sizeof(int64_t), cmp_i64);

    long double sum = 0;
    for (size_t i = 0; i < n; i++) sum += (long double)v[i];

    const double k = 1e-3;   /* ns -> us */
    printf("  %-22s min %8.2f  med %8.2f  p90 %8.2f  p99 %8.2f  "
           "max %9.2f  mean %8.2f\n",
           name,
           s[0]            * k,
           s[n / 2]        * k,
           s[(n * 90)/100] * k,
           s[(n * 99)/100] * k,
           s[n - 1]        * k,
           (double)(sum / n) * k);
    free(s);
}

/* ------------------------------------------------------------------------ */
static void usage(const char *prog)
{
    printf(
"Usage: %s [options]\n"
"  -m, --mode MODE       passthrough (default) or characterize\n"
"  -n, --iterations N    samples to record / characterize (default 200000)\n"
"  -c, --csv FILE        write a per-sample CSV (characterize mode)\n"
"      --core N          CPU core to pin the loop to (default %d)\n"
"      --no-rt           skip real-time scheduling setup (dry runs)\n"
"  -h, --help            this message\n",
        prog, RT_CPU_CORE);
}

int main(int argc, char **argv)
{
    enum run_mode mode = MODE_PASSTHROUGH;
    size_t  iterations = 200000;
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
            if      (!strcmp(optarg, "passthrough"))  mode = MODE_PASSTHROUGH;
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

    printf("=== ADC->DAC latency experiment ===\n");
    printf("  board   : Waveshare High-Precision AD/DA (ADS1256 + DAC8552)\n");
    printf("  spi     : %s   adc %.2f MHz   dac %.2f MHz\n",
           SPI_DEVICE, ADS1256_SPI_HZ / 1e6, DAC8552_SPI_HZ / 1e6);
    printf("  mode    : %s\n",
           mode == MODE_PASSTHROUGH ? "passthrough" : "characterize");
    printf("  realtime: %s\n\n", use_rt ? "SCHED_FIFO" : "disabled (--no-rt)");

    /* ---- record buffers ------------------------------------------------ */
    int64_t *t_adc  = calloc(iterations, sizeof(int64_t)); /* DRDY->ADC done */
    int64_t *t_dac  = calloc(iterations, sizeof(int64_t)); /* ADC->DAC done  */
    int64_t *t_proc = calloc(iterations, sizeof(int64_t)); /* DRDY->DAC done */
    int64_t *t_loop = calloc(iterations, sizeof(int64_t)); /* iteration time */
    int32_t *v_raw  = csv_path ? calloc(iterations, sizeof(int32_t)) : NULL;
    if (!t_adc || !t_dac || !t_proc || !t_loop) {
        fprintf(stderr, "out of memory for %zu samples\n", iterations);
        return 1;
    }

    /* ---- bring up hardware --------------------------------------------- */
    if (use_rt) setup_realtime(core);

    int spi = spi_open(SPI_DEVICE);
    if (spi < 0) return 1;

    if (gpio_init() != 0) { close(spi); return 1; }

    if (ads1256_init(spi) != 0) { gpio_close(); close(spi); return 1; }
    dac8552_init(spi);
    printf("\n");

    signal(SIGINT, on_sigint);

    if (mode == MODE_PASSTHROUGH)
        printf("Running passthrough -- compare scope channels; Ctrl-C to stop.\n\n");
    else
        printf("Characterizing %zu iterations...\n\n", iterations);

    /* ---- the loop ------------------------------------------------------ */
    struct timespec ta, tb, tc, td, td_prev;
    clock_gettime(CLOCK_MONOTONIC, &td_prev);

    size_t recorded = 0;
    for (size_t i = 0; !g_stop; i++) {
        if (mode == MODE_CHARACTERIZE && i >= iterations) break;

        clock_gettime(CLOCK_MONOTONIC, &ta);
        if (ads1256_wait_drdy() != 0) { g_stop = 1; break; }
        clock_gettime(CLOCK_MONOTONIC, &tb);

        int32_t raw = ads1256_read_sample(spi);
        clock_gettime(CLOCK_MONOTONIC, &tc);

        /* 24-bit two's complement -> top 16 bits -> 16-bit offset binary.
         * ADC 0 V maps to DAC mid-scale; the copy is level-shifted, which
         * does not affect the phase the oscilloscope measures. */
        uint16_t code = (uint16_t)(((raw >> 8) & 0xFFFF) ^ 0x8000);
        dac8552_write_a(spi, code);
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

    printf("\nStopped after %zu recorded iterations.\n\n", recorded);

    /* ---- report -------------------------------------------------------- */
    printf("Timing breakdown (microseconds):\n");
    report_stage("ADC read (DRDY->ADC)",  t_adc,  recorded);
    report_stage("DAC write (ADC->DAC)",  t_dac,  recorded);
    report_stage("processing latency*",   t_proc, recorded);
    report_stage("loop period",           t_loop, recorded);
    printf(
"\n  * processing latency = DRDY asserted -> DAC output updated.\n"
"    True analog-to-analog latency = this + the ADS1256 sigma-delta filter\n"
"    group delay + DAC settling, and must be read from the oscilloscope as\n"
"    the input-vs-output phase shift. The loop period should sit near the\n"
"    ADS1256 conversion interval (~33.3 us at 30 kSPS); a much larger 'max'\n"
"    loop period means an iteration was preempted -- a dropped sample.\n");

    /* ---- optional CSV -------------------------------------------------- */
    if (csv_path && v_raw) {
        FILE *f = fopen(csv_path, "w");
        if (!f) {
            perror("warn: cannot open CSV file");
        } else {
            fprintf(f, "index,adc_raw,adc_volts,adc_read_ns,dac_write_ns,"
                       "proc_ns,loop_ns\n");
            for (size_t i = 0; i < recorded; i++)
                fprintf(f, "%zu,%d,%.6f,%ld,%ld,%ld,%ld\n",
                        i, v_raw[i], ads1256_to_volts(v_raw[i]),
                        (long)t_adc[i], (long)t_dac[i],
                        (long)t_proc[i], (long)t_loop[i]);
            fclose(f);
            printf("\nWrote %zu rows to %s\n", recorded, csv_path);
        }
    }

    /* ---- shutdown ------------------------------------------------------ */
    gpio_close();
    close(spi);
    free(t_adc); free(t_dac); free(t_proc); free(t_loop); free(v_raw);
    return 0;
}
