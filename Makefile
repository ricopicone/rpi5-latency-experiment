# ============================================================================
# Makefile -- analog-to-analog latency experiment
#
# No external libraries. Uses only the kernel's spidev and GPIO uAPI v2,
# so a plain C toolchain on Raspberry Pi OS is enough:  sudo apt install build-essential
# ============================================================================

CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -std=gnu11
LDFLAGS ?=

TARGET  := latency_loop
OBJS    := latency_loop.o gpio.o ads1256.o dac8552.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# header dependencies
latency_loop.o: latency_loop.c config.h spidev_util.h gpio.h ads1256.h dac8552.h
gpio.o:         gpio.c         gpio.h config.h
ads1256.o:      ads1256.c      ads1256.h config.h gpio.h spidev_util.h
dac8552.o:      dac8552.c      dac8552.h config.h gpio.h spidev_util.h

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
