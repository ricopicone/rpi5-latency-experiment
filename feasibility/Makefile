# Feasibility-test programs. Each is standalone -- not part of the main
# latency_loop build. See REPORT.md §6 for context and acceptance gates.

CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -std=gnu11 -I..
LDFLAGS ?=

PROGS := bench_gpio_toggle latency_loop_mmap

all: $(PROGS)

# Phase 1 micro-benchmark: standalone.
bench_gpio_toggle: bench_gpio_toggle.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Phase 2 binary: identical to the parent latency_loop except it links the
# mmap-based gpio implementation here in place of ../gpio.c.
LL_OBJS := latency_loop.o ads1256.o dac8552.o gpio_mmap.o

latency_loop_mmap: $(LL_OBJS)
	$(CC) $(CFLAGS) -o $@ $(LL_OBJS) $(LDFLAGS)

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
	rm -f $(PROGS) $(LL_OBJS)

.PHONY: all clean
