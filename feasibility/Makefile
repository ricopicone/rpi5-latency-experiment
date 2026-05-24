# Feasibility-test programs. Each is standalone -- not part of the main
# latency_loop build. See REPORT.md §6 for context and acceptance gates.

CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -std=gnu11 -I..
LDFLAGS ?=

PROGS := bench_gpio_toggle latency_loop_mmap latency_loop_2fd latency_loop_p3 spi_peek

all: $(PROGS)

# Phase 1 micro-benchmark: standalone.
bench_gpio_toggle: bench_gpio_toggle.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Phase 2 binary: identical to the parent latency_loop except it links the
# mmap-based gpio implementation here in place of ../gpio.c.
LL_OBJS := latency_loop.o ads1256.o dac8552.o gpio_mmap.o

latency_loop_mmap: $(LL_OBJS)
	$(CC) $(CFLAGS) -o $@ $(LL_OBJS) $(LDFLAGS)

# Phase 2.5 binary: two spidev FDs, each with its speed set once at init,
# no per-transfer speed_hz. Same gpio_mmap.c as Phase 2 for the fast GPIO
# path; uses ads1256.c/dac8552.c from the parent only for init.
LL2_OBJS := latency_loop_2fd.o ads1256.o dac8552.o gpio_mmap.o

latency_loop_2fd: $(LL2_OBJS)
	$(CC) $(CFLAGS) -o $@ $(LL2_OBJS) $(LDFLAGS)

latency_loop_2fd.o: latency_loop_2fd.c ../config.h ../gpio.h ../ads1256.h ../dac8552.h
	$(CC) $(CFLAGS) -c -o $@ latency_loop_2fd.c

# Phase 3 binary: direct mmap'd access to the RP1 SPI peripheral registers,
# bypassing spidev entirely. Uses gpio_mmap.c for fast CS.
LL3_OBJS := latency_loop_p3.o ads1256.o dac8552.o gpio_mmap.o

latency_loop_p3: $(LL3_OBJS)
	$(CC) $(CFLAGS) -o $@ $(LL3_OBJS) $(LDFLAGS)

latency_loop_p3.o: latency_loop_p3.c spi_rp1.h ../config.h ../gpio.h ../ads1256.h ../dac8552.h
	$(CC) $(CFLAGS) -c -o $@ latency_loop_p3.c

# Diagnostic: peek at the SPI controller's current register state.
spi_peek: spi_peek.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Compile the parent project's sources into this directory's object files.
latency_loop.o: ../latency_loop.c ../config.h ../spidev_util.h ../gpio.h ../ads1256.h ../dac8552.h
	$(CC) $(CFLAGS) -c -o $@ ../latency_loop.c

ads1256.o: ../ads1256.c ../ads1256.h ../config.h ../gpio.h ../spidev_util.h
	$(CC) $(CFLAGS) -c -o $@ ../ads1256.c

dac8552.o: ../dac8552.c ../dac8552.h ../config.h ../gpio.h ../spidev_util.h
	$(CC) $(CFLAGS) -c -o $@ ../dac8552.c

gpio_mmap.o: gpio_mmap.c ../gpio.h ../config.h
	$(CC) $(CFLAGS) -c -o $@ gpio_mmap.c

clean:
	rm -f $(PROGS) $(LL_OBJS) $(LL2_OBJS) $(LL3_OBJS)

.PHONY: all clean
