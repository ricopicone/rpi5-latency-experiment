# Feasibility-test programs. Each is standalone -- not part of the main
# latency_loop build. See REPORT.md §6 for context and acceptance gates.

CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -std=gnu11
LDFLAGS ?=

PROGS := bench_gpio_toggle

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(PROGS)

.PHONY: all clean
