#!/usr/bin/env python3
# =============================================================================
# latency_loop.py -- analog-to-analog latency experiment (Python prototype)
#
# Raspberry Pi 5 + Waveshare High-Precision AD/DA Board (ADS1256 + DAC8552).
#
# This is the Python counterpart of the C program in the parent directory.
# It exists for two reasons:
#   1. Fastest way to confirm the wiring is correct ("first light").
#   2. A deliberate latency *comparison* baseline -- running this next to the
#      C version shows exactly what the Python interpreter and per-call
#      syscall overhead cost you in a real-time control loop. Expect this
#      version to be noticeably slower and far more jittery than the C one;
#      that contrast is itself a useful data point.
#
# Dependencies (Raspberry Pi OS):
#   sudo apt install python3-spidev python3-lgpio
# lgpio is used because it supports the Pi 5's RP1 I/O controller; the older
# RPi.GPIO / pigpio paths do not work reliably on the Pi 5.
#
# Run:  sudo python3 latency_loop.py --mode characterize
# =============================================================================

import argparse
import os
import sys
import time

try:
    import spidev
except ImportError:
    sys.exit("missing 'spidev'  ->  sudo apt install python3-spidev")
try:
    import lgpio
except ImportError:
    sys.exit("missing 'lgpio'   ->  sudo apt install python3-lgpio")

# --- configuration -- keep these in sync with ../config.h --------------------
SPI_BUS, SPI_DEV   = 0, 0          # /dev/spidev0.0
SPI_MODE           = 1             # CPOL=0, CPHA=1
ADS1256_SPI_HZ     = 1_920_000     # f_CLKIN / 4 ceiling
DAC8552_SPI_HZ     = 15_600_000

GPIO_CHIP          = 0             # /dev/gpiochip0 on the Pi 5 (try 4 if this fails)
LINE_ADC_DRDY      = 17
LINE_ADC_RESET     = 18
LINE_ADC_CS        = 22
LINE_DAC_CS        = 23

ADS1256_DRATE      = 0xF0          # 30 kSPS
ADS1256_ADCON      = 0x00          # PGA = 1
ADS1256_STATUS     = 0x00          # MSB first, auto-cal off, buffer off
ADS1256_MUX        = 0x08          # single-ended AIN0 / AINCOM

# ADS1256 commands / registers
_SDATAC, _RDATAC, _RREG, _WREG, _SELFCAL = 0x0F, 0x03, 0x10, 0x50, 0xF0


class Board:
    """ADS1256 + DAC8552 on the shared SPI bus, with manual GPIO chip-select."""

    def __init__(self):
        self.spi = spidev.SpiDev()
        self.spi.open(SPI_BUS, SPI_DEV)
        self.spi.mode = SPI_MODE
        try:
            self.spi.no_cs = True          # we drive CS ourselves via GPIO
        except OSError:
            pass                           # some kernels reject this; harmless

        try:
            self.chip = lgpio.gpiochip_open(GPIO_CHIP)
        except lgpio.error:
            self.chip = lgpio.gpiochip_open(4)   # Pi 5 alternate node

        lgpio.gpio_claim_input(self.chip, LINE_ADC_DRDY)
        for line in (LINE_ADC_RESET, LINE_ADC_CS, LINE_DAC_CS):
            lgpio.gpio_claim_output(self.chip, line, 1)   # idle high

    # ---- low-level helpers --------------------------------------------------
    def _adc_cs(self, active):
        lgpio.gpio_write(self.chip, LINE_ADC_CS, 0 if active else 1)

    def _dac_cs(self, active):
        lgpio.gpio_write(self.chip, LINE_DAC_CS, 0 if active else 1)

    def _adc_cmd(self, byte):
        self._adc_cs(True)
        self.spi.xfer2([byte], ADS1256_SPI_HZ)
        self._adc_cs(False)

    # ---- ADS1256 ------------------------------------------------------------
    def ads1256_init(self):
        # hardware reset pulse
        lgpio.gpio_write(self.chip, LINE_ADC_RESET, 1)
        time.sleep(0.001)
        lgpio.gpio_write(self.chip, LINE_ADC_RESET, 0)
        time.sleep(0.001)
        lgpio.gpio_write(self.chip, LINE_ADC_RESET, 1)
        time.sleep(0.01)

        self._adc_cmd(_SDATAC)
        time.sleep(0.001)

        # verify communication: STATUS register, ID nibble should read 0x3
        self._adc_cs(True)
        self.spi.xfer2([_RREG | 0x00, 0x00], ADS1256_SPI_HZ)
        time.sleep(20e-6)                                  # t6 delay
        status = self.spi.xfer2([0xFF], ADS1256_SPI_HZ)[0]
        self._adc_cs(False)
        if (status >> 4) != 0x3:
            print(f"ads1256: WARNING unexpected STATUS=0x{status:02X} "
                  f"(ID 0x{status >> 4:X}, expected 0x3) -- check wiring")
        else:
            print(f"ads1256: detected (STATUS=0x{status:02X})")

        # configure STATUS, MUX, ADCON, DRATE in one WREG burst
        self._adc_cs(True)
        self.spi.xfer2([_WREG | 0x00, 0x03,
                        ADS1256_STATUS, ADS1256_MUX,
                        ADS1256_ADCON, ADS1256_DRATE], ADS1256_SPI_HZ)
        self._adc_cs(False)
        time.sleep(0.001)

        self._adc_cmd(_SELFCAL)
        self.wait_drdy()                                   # cal complete
        self._adc_cmd(_RDATAC)
        time.sleep(20e-6)
        print(f"ads1256: configured (DRATE=0x{ADS1256_DRATE:02X}, RDATAC active)")

    def wait_drdy(self, limit=20_000_000):
        for _ in range(limit):
            if lgpio.gpio_read(self.chip, LINE_ADC_DRDY) == 0:
                return True
        print("ads1256: timeout waiting for DRDY -- check wiring")
        return False

    def read_sample(self):
        """Read one 24-bit sample in RDATAC mode; return a signed int."""
        self._adc_cs(True)
        b = self.spi.xfer2([0xFF, 0xFF, 0xFF], ADS1256_SPI_HZ)
        self._adc_cs(False)
        v = (b[0] << 16) | (b[1] << 8) | b[2]
        if v & 0x800000:
            v -= 0x1000000
        return v

    # ---- DAC8552 ------------------------------------------------------------
    def dac_write_a(self, code):
        """Write 16-bit code to DAC channel A; control byte 0x30 = load A."""
        self._dac_cs(True)
        self.spi.xfer2([0x30, (code >> 8) & 0xFF, code & 0xFF], DAC8552_SPI_HZ)
        self._dac_cs(False)

    def close(self):
        try:
            self.spi.close()
        finally:
            lgpio.gpiochip_close(self.chip)


def report(name, samples):
    """Print a percentile row in microseconds."""
    if not samples:
        print(f"  {name:<22} (no samples)")
        return
    s = sorted(samples)
    n = len(s)
    us = 1e-3
    print(f"  {name:<22} min {s[0]*us:8.2f}  med {s[n//2]*us:8.2f}  "
          f"p90 {s[n*90//100]*us:8.2f}  p99 {s[n*99//100]*us:8.2f}  "
          f"max {s[-1]*us:9.2f}  mean {sum(s)/n*us:8.2f}")


def main():
    ap = argparse.ArgumentParser(description="ADC->DAC latency experiment (Python)")
    ap.add_argument("-m", "--mode", choices=("passthrough", "characterize"),
                    default="passthrough")
    ap.add_argument("-n", "--iterations", type=int, default=50_000,
                    help="samples to record / characterize")
    ap.add_argument("--rt", action="store_true",
                    help="request SCHED_FIFO real-time scheduling")
    args = ap.parse_args()

    if args.rt:
        try:
            os.sched_setscheduler(0, os.SCHED_FIFO,
                                  os.sched_param(80))
            print("realtime: SCHED_FIFO priority 80")
        except (PermissionError, OSError) as e:
            print(f"realtime: could not set SCHED_FIFO ({e}); run with sudo")

    print("=== ADC->DAC latency experiment (Python prototype) ===")
    print(f"  mode: {args.mode}\n")

    board = Board()
    try:
        board.ads1256_init()
        board.dac_write_a(0x8000)
        print()

        if args.mode == "passthrough":
            print("Running passthrough -- compare scope channels; Ctrl-C to stop.\n")
        else:
            print(f"Characterizing {args.iterations} iterations...\n")

        t_adc, t_dac, t_proc, t_loop = [], [], [], []
        clk = time.perf_counter_ns
        td_prev = clk()
        i = 0
        try:
            while True:
                if args.mode == "characterize" and i >= args.iterations:
                    break
                if not board.wait_drdy():
                    break
                tb = clk()
                raw = board.read_sample()
                tc = clk()
                code = ((raw >> 8) & 0xFFFF) ^ 0x8000
                board.dac_write_a(code)
                td = clk()

                if len(t_proc) < args.iterations:
                    t_adc.append(tc - tb)
                    t_dac.append(td - tc)
                    t_proc.append(td - tb)
                    t_loop.append(td - td_prev)
                td_prev = td
                i += 1
        except KeyboardInterrupt:
            pass

        print(f"\nStopped after {len(t_proc)} recorded iterations.\n")
        print("Timing breakdown (microseconds):")
        report("ADC read (DRDY->ADC)", t_adc)
        report("DAC write (ADC->DAC)", t_dac)
        report("processing latency*", t_proc)
        report("loop period", t_loop)
        print("\n  * DRDY asserted -> DAC updated. True analog-to-analog latency")
        print("    adds the ADS1256 filter group delay + DAC settling and must")
        print("    be read from the oscilloscope. Compare these numbers with the")
        print("    C version to see the cost of the Python interpreter.")
    finally:
        board.close()


if __name__ == "__main__":
    main()
