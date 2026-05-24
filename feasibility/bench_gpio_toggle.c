/* ============================================================================
 * bench_gpio_toggle.c -- Phase 1 of the §6 feasibility plan.
 *
 * Question: at what rate can a user-space program on the Raspberry Pi 5
 * toggle a single GPIO pin via direct (mmap'd) access to the RP1 SYS_RIO
 * peripheral registers?
 *
 * Method: mmap the RP1 RIO region (physical 0x1f000e0000, size 0x0000c000)
 * through /dev/mem, then in a tight loop alternately write to RIO_OUT_SET
 * and RIO_OUT_CLR for one chosen pin. Time N iterations with clock_monotonic;
 * report nanoseconds per toggle.
 *
 * Reference for register layout: drivers/pinctrl/pinctrl-rp1.c in the
 * raspberrypi/linux kernel tree (rpi-6.12.y), specifically the per-bank
 * descriptor at line ~291 and the RP1_{RW,XOR,SET,CLR}_OFFSET aliases.
 * Bank 0 (GPIO0-27) uses rio_offset = 0x0000 within the RIO region.
 *
 * Pin configuration is left to the kernel pinctrl driver via the `pinctrl`
 * utility -- this program ONLY exercises the fast toggle path. Set up the
 * pin once before running:
 *
 *     sudo pinctrl set 5 op pn dl    # pin GPIO5 = output, no pull, drive low
 *
 * Run:
 *     sudo ./bench_gpio_toggle [iterations]
 *
 * Default iterations: 10 000 000 set+clr cycles  =  20 000 000 toggles.
 *
 * Build (on the Pi):
 *     make
 *
 * Result interpretation against the §6.1 pass gate:
 *   per toggle <=  500 ns  -> PASS  (Phase 1 succeeds; proceed to Phase 2)
 *   per toggle  > 1000 ns  -> FAIL  (stop; Pi 5 not viable for ~10 us loops)
 *   500..1000 ns           -> marginal; investigate before proceeding
 * ==========================================================================*/

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

/* RP1 peripheral region: see /proc/iomem and rp1 dts node. */
#define RP1_RIO_BASE_PHYS  0x1f000e0000UL
#define RP1_REGION_SIZE    0x0000c000UL   /* 48 KB; covers all 3 banks */

/* Alias offsets within an RP1 peripheral region. */
#define RP1_RW_OFFSET      0x0000u
#define RP1_XOR_OFFSET     0x1000u
#define RP1_SET_OFFSET     0x2000u
#define RP1_CLR_OFFSET     0x3000u

/* SYS_RIO register offsets (within the RW alias). */
#define RIO_OUT            0x00u
#define RIO_OE             0x04u
#define RIO_IN             0x08u

/* Bank 0 occupies rio_offset 0x0000; covers GPIO0..GPIO27. */
#define BANK0_RIO_OFFSET   0x0000u

#define DEFAULT_TEST_PIN   5            /* GPIO5  ->  header pin 29; unused */
#define DEFAULT_ITERATIONS 10000000L

static int64_t ns_between(const struct timespec *a, const struct timespec *b)
{
    return (int64_t)(b->tv_sec  - a->tv_sec ) * 1000000000LL
         + (int64_t)(b->tv_nsec - a->tv_nsec);
}

int main(int argc, char **argv)
{
    long iterations = (argc > 1) ? atol(argv[1]) : DEFAULT_ITERATIONS;
    int  pin        = (argc > 2) ? atoi(argv[2]) : DEFAULT_TEST_PIN;
    if (iterations <= 0) iterations = DEFAULT_ITERATIONS;
    if (pin < 0 || pin > 27) {
        fprintf(stderr, "pin %d out of bank-0 range (0..27)\n", pin);
        return 1;
    }

    /* /dev/mem requires CAP_SYS_RAWIO (run with sudo). */
    int fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "open /dev/mem: %s   (run with sudo)\n", strerror(errno));
        return 1;
    }

    void *map = mmap(NULL, RP1_REGION_SIZE, PROT_READ | PROT_WRITE,
                     MAP_SHARED, fd, (off_t)RP1_RIO_BASE_PHYS);
    if (map == MAP_FAILED) {
        fprintf(stderr, "mmap RIO @ 0x%lx: %s\n",
                RP1_RIO_BASE_PHYS, strerror(errno));
        close(fd);
        return 1;
    }

    /* The SET/CLR aliases let us atomically set or clear individual bits
     * without a read-modify-write -- a single 32-bit write per toggle. */
    volatile uint32_t *out_set = (volatile uint32_t *)
        ((uintptr_t)map + RP1_SET_OFFSET + BANK0_RIO_OFFSET + RIO_OUT);
    volatile uint32_t *out_clr = (volatile uint32_t *)
        ((uintptr_t)map + RP1_CLR_OFFSET + BANK0_RIO_OFFSET + RIO_OUT);

    const uint32_t bit = 1u << pin;

    /* Real-time hygiene: lock memory, pin to core 3, raise priority a bit. */
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
        perror("warn: mlockall");
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(3, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0)
        perror("warn: sched_setaffinity");
    struct sched_param sp = { .sched_priority = 80 };
    if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0)
        perror("warn: SCHED_FIFO");

    printf("=== RP1 GPIO toggle micro-benchmark (Phase 1) ===\n");
    printf("  pin         : GPIO%d\n", pin);
    printf("  iterations  : %ld set+clr cycles  (= %ld toggles)\n",
           iterations, iterations * 2);
    printf("  RIO physical: 0x%lx  +  set 0x%x / clr 0x%x  +  RIO_OUT 0x%x\n",
           RP1_RIO_BASE_PHYS, RP1_SET_OFFSET, RP1_CLR_OFFSET, RIO_OUT);
    printf("\n  (configure the pin beforehand with:  sudo pinctrl set %d op pn dl)\n\n",
           pin);

    /* Warm cache and TLB with a short pre-run. */
    for (int w = 0; w < 1000; w++) { *out_set = bit; *out_clr = bit; }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (long i = 0; i < iterations; i++) {
        *out_set = bit;
        *out_clr = bit;
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);

    int64_t total_ns = ns_between(&t0, &t1);
    long    toggles  = iterations * 2;
    double  ns_per_toggle = (double)total_ns / (double)toggles;
    double  mhz_toggle    = 1000.0 / ns_per_toggle;

    printf("Results\n");
    printf("  total time      : %.3f ms\n", total_ns / 1e6);
    printf("  toggles         : %ld\n",     toggles);
    printf("  ns per toggle   : %.1f ns\n", ns_per_toggle);
    printf("  toggle rate     : %.2f MHz\n", mhz_toggle);
    printf("\nPass/fail vs §6.1 of REPORT.md:\n");
    if (ns_per_toggle <= 500.0) {
        printf("  -> PASS  (<= 500 ns/toggle). Proceed to Phase 2.\n");
    } else if (ns_per_toggle <= 1000.0) {
        printf("  -> MARGINAL  (500-1000 ns/toggle). Investigate before Phase 2.\n");
    } else {
        printf("  -> FAIL  (> 1000 ns/toggle). Pi 5 not viable for ~10 us loops.\n");
    }

    munmap(map, RP1_REGION_SIZE);
    close(fd);
    return 0;
}
