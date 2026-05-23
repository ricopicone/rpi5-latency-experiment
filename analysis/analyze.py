#!/usr/bin/env python3
"""
analyze.py -- post-process the latency-experiment hardware data.

Inputs (relative to repository root):
    data/phase_sweep.csv        -- scope-measured CH1 -> CH2 phase per frequency
    data/characterize_15ksps.csv -- per-sample software timings from the C loop

Outputs:
    analysis/figures/phase_vs_freq.png
    analysis/figures/delay_vs_freq.png
    analysis/figures/proc_latency_hist.png
    analysis/figures/loop_period_hist.png
    analysis/figures/budget_bar.png
    analysis/figures/summary.png       (4-panel composite for the report)
    stdout: a printed summary table

Run from the repository root:
    python3 analysis/analyze.py
"""

from __future__ import annotations

import csv
import os
import sys
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt


REPO_ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = REPO_ROOT / "data"
FIG_DIR = REPO_ROOT / "analysis" / "figures"
FIG_DIR.mkdir(parents=True, exist_ok=True)

# Conversion-rate interval (us) for ADS1256 at 15 kSPS.
CONVERSION_INTERVAL_US = 1e6 / 15_000  # 66.667 us

# 1/20-period success criterion, in degrees of phase shift.
PHASE_CRITERION_DEG = 360.0 / 20.0  # 18 deg


# ---------------------------------------------------------------------------
# Data loading
# ---------------------------------------------------------------------------
def load_phase_sweep(path: Path) -> tuple[np.ndarray, np.ndarray]:
    """Load the comment-tolerant phase sweep CSV: returns (freq_hz, phase_deg)."""
    freqs: list[float] = []
    phases: list[float] = []
    with path.open() as f:
        for line in f:
            s = line.strip()
            if not s or s.startswith("#") or s.startswith("freq"):
                continue
            parts = s.split(",")
            freqs.append(float(parts[0]))
            phases.append(float(parts[1]))
    return np.asarray(freqs), np.asarray(phases)


def load_characterize(path: Path) -> dict[str, np.ndarray]:
    """Load the per-sample CSV from --csv mode. ns columns are kept as ns."""
    cols: dict[str, list[float]] = {
        "adc_read_ns": [],
        "dac_write_ns": [],
        "proc_ns": [],
        "loop_ns": [],
    }
    with path.open() as f:
        reader = csv.DictReader(f)
        for row in reader:
            for k in cols:
                cols[k].append(float(row[k]))
    return {k: np.asarray(v) for k, v in cols.items()}


# ---------------------------------------------------------------------------
# Statistics helpers
# ---------------------------------------------------------------------------
def percentiles_us(samples_ns: np.ndarray) -> dict[str, float]:
    """Return min/median/p90/p99/max/mean of `samples_ns` in microseconds."""
    us = samples_ns * 1e-3
    return {
        "min": float(np.min(us)),
        "median": float(np.median(us)),
        "p90": float(np.percentile(us, 90)),
        "p99": float(np.percentile(us, 99)),
        "max": float(np.max(us)),
        "mean": float(np.mean(us)),
    }


def fmt_row(name: str, p: dict[str, float]) -> str:
    return (f"  {name:<22} "
            f"min {p['min']:8.2f}  med {p['median']:8.2f}  "
            f"p90 {p['p90']:8.2f}  p99 {p['p99']:8.2f}  "
            f"max {p['max']:9.2f}  mean {p['mean']:8.2f}")


# ---------------------------------------------------------------------------
# Plot helpers
# ---------------------------------------------------------------------------
def _save(fig: plt.Figure, name: str) -> Path:
    out = FIG_DIR / name
    fig.savefig(out, dpi=150, bbox_inches="tight")
    plt.close(fig)
    return out


def plot_phase_vs_freq(freqs: np.ndarray, phases: np.ndarray) -> Path:
    # Linear fit (forced through origin since phase = 360 * tau * f and tau > 0).
    tau_s = np.sum(freqs * phases) / np.sum(freqs * freqs) / 360.0
    fit_phases = 360.0 * tau_s * freqs
    f_crit = PHASE_CRITERION_DEG / (360.0 * tau_s)

    fig, ax = plt.subplots(figsize=(6.5, 4.2))
    ax.plot(freqs, phases, "o", color="C0", label="scope measurement", zorder=3)
    ax.plot(freqs, fit_phases, "-", color="C1",
            label=f"fit: $\\phi = 360\\,\\tau\\,f$, $\\tau$ = {tau_s*1e6:.1f} µs",
            zorder=2)
    ax.axhline(PHASE_CRITERION_DEG, color="C3", linestyle="--",
               label=f"1/20-period limit ({PHASE_CRITERION_DEG:.0f}°)")
    ax.axvline(f_crit, color="C3", linestyle=":",
               label=f"crossing: {f_crit:.0f} Hz")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Phase shift CH1→CH2 (degrees)")
    ax.set_title("Analog-to-analog phase shift vs frequency")
    ax.grid(True, alpha=0.3)
    ax.legend(loc="lower right")
    return _save(fig, "phase_vs_freq.png")


def plot_delay_vs_freq(freqs: np.ndarray, phases: np.ndarray) -> Path:
    delays_us = (phases / 360.0) * (1.0 / freqs) * 1e6
    mean_us = float(np.mean(delays_us))
    std_us = float(np.std(delays_us, ddof=1))

    fig, ax = plt.subplots(figsize=(6.5, 4.2))
    ax.plot(freqs, delays_us, "o", color="C0", label="per-frequency delay", zorder=3)
    ax.axhline(mean_us, color="C2", linestyle="-",
               label=f"mean = {mean_us:.1f} µs")
    ax.fill_between(freqs, mean_us - std_us, mean_us + std_us,
                    color="C2", alpha=0.15,
                    label=f"±1σ = ±{std_us:.1f} µs")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("End-to-end delay (µs)")
    ax.set_title("Pure-delay model check: delay is constant with frequency")
    ax.set_ylim(mean_us - 4 * std_us, mean_us + 4 * std_us)
    ax.grid(True, alpha=0.3)
    ax.legend(loc="upper right")
    return _save(fig, "delay_vs_freq.png")


def plot_proc_latency_hist(proc_ns: np.ndarray) -> Path:
    p = percentiles_us(proc_ns)
    us = proc_ns * 1e-3
    fig, ax = plt.subplots(figsize=(6.5, 4.2))
    ax.hist(us, bins=np.arange(p["min"] - 1, p["max"] + 1, 0.2),
            color="C0", edgecolor="none")
    ax.axvline(p["median"], color="C1", linestyle="-",
               label=f"median {p['median']:.2f} µs")
    ax.axvline(p["p99"], color="C3", linestyle="--",
               label=f"p99 {p['p99']:.2f} µs")
    ax.axvline(p["max"], color="C4", linestyle=":",
               label=f"max {p['max']:.2f} µs")
    ax.set_xlabel("Processing latency (µs)  — DRDY asserted → DAC chip written")
    ax.set_ylabel("Count")
    ax.set_title(f"Software processing latency, n = {len(us)} samples")
    ax.set_yscale("log")
    ax.grid(True, alpha=0.3)
    ax.legend(loc="upper right")
    return _save(fig, "proc_latency_hist.png")


def plot_loop_period_hist(loop_ns: np.ndarray) -> Path:
    p = percentiles_us(loop_ns)
    us = loop_ns * 1e-3
    fig, ax = plt.subplots(figsize=(6.5, 4.2))
    ax.hist(us, bins=np.arange(p["min"] - 1, p["max"] + 1, 0.2),
            color="C0", edgecolor="none")
    ax.axvline(CONVERSION_INTERVAL_US, color="C2", linestyle="-",
               label=f"15 kSPS interval {CONVERSION_INTERVAL_US:.2f} µs")
    ax.axvline(p["median"], color="C1", linestyle="--",
               label=f"median {p['median']:.2f} µs")
    ax.axvline(p["max"], color="C4", linestyle=":",
               label=f"max {p['max']:.2f} µs")
    ax.set_xlabel("Loop period (µs)")
    ax.set_ylabel("Count")
    ax.set_title(f"Iteration period, n = {len(us)} samples")
    ax.set_yscale("log")
    ax.grid(True, alpha=0.3)
    ax.legend(loc="upper right")
    return _save(fig, "loop_period_hist.png")


def plot_budget_bar(software_us: float, measured_total_us: float) -> Path:
    # SINC5 group delay for ADS1256 at 15 kSPS ≈ 2.5 / f_data. DAC8552 settling
    # taken as the datasheet typical (~5 us). The "predicted" total is the
    # sum; the "measured" total comes from the scope sweep.
    sinc_delay_us = 2.5 / 15_000 * 1e6   # 166.7 us
    dac_settle_us = 5.0
    predicted = software_us + sinc_delay_us + dac_settle_us

    components = [
        ("Software loop\n(DRDY → DAC write)", software_us, "C0"),
        ("ADS1256 SINC5\nfilter group delay", sinc_delay_us, "C1"),
        ("DAC8552\nsettling", dac_settle_us, "C2"),
    ]

    fig, ax = plt.subplots(figsize=(7.5, 4.5))
    bottom = 0.0
    for name, val, color in components:
        ax.bar(["predicted total"], [val], bottom=bottom, color=color, label=name)
        ax.text(0, bottom + val / 2, f"{val:.1f} µs",
                ha="center", va="center", color="white", fontsize=10, fontweight="bold")
        bottom += val
    ax.bar(["scope-measured\ntotal"], [measured_total_us], color="C7",
           label=f"scope measured ({measured_total_us:.1f} µs)")
    ax.text(1, measured_total_us / 2, f"{measured_total_us:.1f} µs",
            ha="center", va="center", color="white", fontsize=10, fontweight="bold")
    ax.set_ylabel("Latency (µs)")
    ax.set_title(f"Latency budget: predicted {predicted:.1f} µs vs measured {measured_total_us:.1f} µs")
    ax.grid(True, axis="y", alpha=0.3)
    ax.legend(loc="upper right", framealpha=0.95)
    return _save(fig, "budget_bar.png")


def plot_system_diagram() -> Path:
    """
    Block diagram of the signal path: function generator -> ADS1256 -> Pi 5 ->
    DAC8552 -> scope, with the T-junction tapping the input straight into CH1.
    """
    fig, ax = plt.subplots(figsize=(12.0, 5.5))
    ax.set_xlim(0, 16)
    ax.set_ylim(0, 7)
    ax.set_aspect("equal")
    ax.axis("off")

    def box(x, y, w, h, text, color="C0"):
        ax.add_patch(plt.Rectangle((x, y), w, h, fill=True, facecolor="white",
                                   edgecolor=color, linewidth=1.6))
        ax.text(x + w / 2, y + h / 2, text, ha="center", va="center", fontsize=9)

    def harrow(x0, x1, y, label=None, color="black", label_dy=0.32):
        ax.annotate("", xy=(x1, y), xytext=(x0, y),
                    arrowprops=dict(arrowstyle="-|>", color=color, lw=1.4,
                                    shrinkA=2, shrinkB=2))
        if label:
            ax.text((x0 + x1) / 2, y + label_dy, label,
                    ha="center", va="bottom", fontsize=8.5, color=color,
                    style="italic")

    # Row y-coordinates
    y_main = 3.0          # main signal-path row
    y_top = 5.5           # scope CH1 row
    y_bot = 0.7           # scope CH2 row

    # ---- main row, left to right ----
    # Function generator (x: 0.3..3.2)
    box(0.3, y_main - 0.8, 2.9, 1.6,
        "Function generator\nsine, 2 Vpp,\n+1.25 V offset", color="C3")

    # ADS1256 (x: 5.0..8.0)
    box(5.0, y_main - 0.8, 3.0, 1.6,
        "ADS1256\nAIN0 (single-ended)\n15 kSPS, SINC5", color="C0")

    # Raspberry Pi 5 (x: 9.5..12.5)
    box(9.5, y_main - 0.8, 3.0, 1.6,
        "Raspberry Pi 5\nuser-space C loop\nSCHED_FIFO, core 3", color="C1")

    # DAC8552 (x: 14.0..15.7)
    box(14.0, y_main - 0.8, 1.7, 1.6, "DAC8552\nOUTA", color="C0")

    # T-junction marker on funcgen output
    tx = 3.7
    ax.plot([3.2, tx], [y_main, y_main], "k-", lw=1.2)
    ax.plot([tx], [y_main], "ko", markersize=5, zorder=5)
    ax.text(tx + 0.05, y_main - 0.32, "T", fontsize=8, color="dimgray",
            style="italic")

    # T -> ADS1256
    harrow(tx, 5.0, y_main, "AD0 (signal)\nAINCOM (gnd)", label_dy=0.30)

    # ADS1256 -> Pi 5
    harrow(8.0, 9.5, y_main, "SPI @ 1.92 MHz\n+ DRDY (GPIO17)", label_dy=0.30)

    # Pi 5 -> DAC8552
    harrow(12.5, 14.0, y_main, "SPI @ 15.6 MHz", label_dy=0.30)

    # ---- scope CH1 row (top) ----
    box(13.6, y_top - 0.45, 2.1, 0.9, "Scope CH1\n(input)", color="C2")
    # T-junction up to CH1
    ax.plot([tx, tx], [y_main, y_top], "k-", lw=1.2)
    harrow(tx, 13.6, y_top, "CH1 (input reference, via BNC T)", color="dimgray",
           label_dy=0.18)

    # ---- scope CH2 row (bottom) ----
    box(13.6, y_bot - 0.45, 2.1, 0.9, "Scope CH2\n(output)", color="C2")
    # DAC8552 down to CH2
    ax.plot([14.85, 14.85], [y_main - 0.8, y_bot + 0.45], "k-", lw=1.2)
    # arrow head at the scope side
    ax.annotate("", xy=(14.85, y_bot + 0.45), xytext=(14.85, y_bot + 0.55),
                arrowprops=dict(arrowstyle="-|>", color="black", lw=1.4))

    # ---- side bracket showing the measured quantity ----
    bx = 15.85
    ax.plot([bx, bx + 0.15], [y_top, y_top], "k-", lw=1.0)
    ax.plot([bx + 0.15, bx + 0.15], [y_top, y_bot], "k-", lw=1.0)
    ax.plot([bx + 0.15, bx], [y_bot, y_bot], "k-", lw=1.0)
    ax.text(bx + 0.35, (y_top + y_bot) / 2,
            "scope measures\nCH1 → CH2 phase shift\n= analog-to-analog\n  latency",
            ha="left", va="center", fontsize=8, style="italic", color="dimgray")

    ax.set_title("Signal-path block diagram", fontsize=11)
    return _save(fig, "system_diagram.png")


def plot_latency_timing(adc_ns: np.ndarray, dac_ns: np.ndarray,
                        proc_ns: np.ndarray) -> Path:
    """
    Gantt-style breakdown of one loop iteration: top row is the full
    DRDY -> DAC-write window (~48 us median), middle row splits it into the
    measured ADC-read and DAC-write phases, bottom row decomposes each phase
    into GPIO ioctls + spidev syscall + SPI bit-clocking on the wire.

    The middle row's segment widths are the measured medians from the CSV;
    the bottom row's sub-segments are estimates that match the medians and
    are consistent with each component's known cost (per-CS GPIO ioctl ~5 us,
    spidev syscall ~2-3 us, SPI on-wire = (3*8/clock)*1e6 us).
    """
    adc_med = float(np.median(adc_ns)) * 1e-3
    dac_med = float(np.median(dac_ns)) * 1e-3
    proc_med = float(np.median(proc_ns)) * 1e-3

    # Sub-decomposition (estimates).
    spi_adc_wire = 3 * 8 / 1.92e6 * 1e6              # 12.5 us
    spi_dac_wire = 3 * 8 / 15.6e6 * 1e6              # 1.54 us
    gpio_each = 5.0                                  # ~5 us per ioctl
    spidev_adc = adc_med - 2 * gpio_each - spi_adc_wire
    spidev_dac = dac_med - 2 * gpio_each - spi_dac_wire

    fig, ax = plt.subplots(figsize=(11.0, 4.5))
    row_h = 0.6
    y_top = 3.0
    y_mid = 1.9
    y_bot = 0.8

    # Top row: total processing latency
    ax.barh(y_top, proc_med, height=row_h, color="C7", edgecolor="black",
            linewidth=0.8)
    ax.text(proc_med / 2, y_top, f"processing latency  {proc_med:.1f} µs",
            ha="center", va="center", color="white", fontsize=10,
            fontweight="bold")

    # Middle row: ADC read phase, DAC write phase
    ax.barh(y_mid, adc_med, left=0, height=row_h, color="C0",
            edgecolor="black", linewidth=0.8, label="ADC read phase")
    ax.text(adc_med / 2, y_mid, f"ADC read phase\n{adc_med:.1f} µs",
            ha="center", va="center", color="white", fontsize=9,
            fontweight="bold")
    ax.barh(y_mid, dac_med, left=adc_med, height=row_h, color="C1",
            edgecolor="black", linewidth=0.8, label="DAC write phase")
    ax.text(adc_med + dac_med / 2, y_mid, f"DAC write phase\n{dac_med:.1f} µs",
            ha="center", va="center", color="white", fontsize=9,
            fontweight="bold")

    # Bottom row: sub-decomposition
    def seg(left, width, color, text):
        ax.barh(y_bot, width, left=left, height=row_h, color=color,
                edgecolor="black", linewidth=0.6)
        if width >= 3.0:
            ax.text(left + width / 2, y_bot, text,
                    ha="center", va="center", fontsize=7.5, color="black")

    adc_segs = [
        (gpio_each,   "C4", "GPIO\nCS↓"),
        (spidev_adc,  "C9", "spidev\nsyscall"),
        (spi_adc_wire,"C0", f"SPI on wire\n{spi_adc_wire:.1f} µs"),
        (gpio_each,   "C4", "GPIO\nCS↑"),
    ]
    x = 0.0
    for w, c, t in adc_segs:
        seg(x, w, c, t)
        x += w

    dac_segs = [
        (gpio_each,   "C4", "GPIO\nCS↓"),
        (spidev_dac,  "C9", "spidev\nsyscall"),
        (spi_dac_wire,"C1", f"{spi_dac_wire:.1f} µs"),
        (gpio_each,   "C4", "GPIO\nCS↑"),
    ]
    x = adc_med
    for w, c, t in dac_segs:
        seg(x, w, c, t)
        x += w

    # Annotations linking rows
    ax.annotate("", xy=(0, y_mid + row_h / 2 + 0.05),
                xytext=(0, y_top - row_h / 2 - 0.05),
                arrowprops=dict(arrowstyle="-", color="dimgray", lw=0.8))
    ax.annotate("", xy=(proc_med, y_mid + row_h / 2 + 0.05),
                xytext=(proc_med, y_top - row_h / 2 - 0.05),
                arrowprops=dict(arrowstyle="-", color="dimgray", lw=0.8))
    ax.annotate("", xy=(0, y_bot + row_h / 2 + 0.05),
                xytext=(0, y_mid - row_h / 2 - 0.05),
                arrowprops=dict(arrowstyle="-", color="dimgray", lw=0.8))
    ax.annotate("", xy=(adc_med, y_bot + row_h / 2 + 0.05),
                xytext=(adc_med, y_mid - row_h / 2 - 0.05),
                arrowprops=dict(arrowstyle="-", color="dimgray", lw=0.8))
    ax.annotate("", xy=(proc_med, y_bot + row_h / 2 + 0.05),
                xytext=(proc_med, y_mid - row_h / 2 - 0.05),
                arrowprops=dict(arrowstyle="-", color="dimgray", lw=0.8))

    # Side labels for rows
    ax.text(-0.5, y_top, "total", ha="right", va="center", fontsize=9)
    ax.text(-0.5, y_mid, "phase", ha="right", va="center", fontsize=9)
    ax.text(-0.5, y_bot, "component", ha="right", va="center", fontsize=9)

    # Legend
    from matplotlib.patches import Patch
    handles = [
        Patch(color="C4", label="GPIO uAPI v2 ioctl (CS toggle, ~5 µs each)"),
        Patch(color="C9", label="spidev SPI_IOC_MESSAGE syscall overhead"),
        Patch(color="C0", label="SPI bytes on wire @ 1.92 MHz (ADC, 12.5 µs)"),
        Patch(color="C1", label="SPI bytes on wire @ 15.6 MHz (DAC, 1.5 µs)"),
    ]
    ax.legend(handles=handles, loc="lower center", bbox_to_anchor=(0.5, -0.25),
              ncol=2, frameon=False, fontsize=8)

    ax.set_xlim(-2.5, proc_med + 2.5)
    ax.set_ylim(0.2, 3.7)
    ax.set_xlabel("Time within one iteration (µs)")
    ax.set_yticks([])
    ax.set_title("Where the 48 µs of software loop time is spent "
                 "(median of 50 000 iterations)", fontsize=11)
    ax.grid(True, axis="x", alpha=0.3)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["left"].set_visible(False)
    return _save(fig, "latency_timing.png")


def plot_summary(
    freqs: np.ndarray,
    phases: np.ndarray,
    delays_us: np.ndarray,
    proc_ns: np.ndarray,
    loop_ns: np.ndarray,
    software_us: float,
    measured_total_us: float,
) -> Path:
    """Four-panel composite for the report."""
    fig, axes = plt.subplots(2, 2, figsize=(11.0, 7.5))

    # Panel A: phase vs freq
    ax = axes[0, 0]
    tau_s = np.sum(freqs * phases) / np.sum(freqs * freqs) / 360.0
    f_crit = PHASE_CRITERION_DEG / (360.0 * tau_s)
    ax.plot(freqs, phases, "o", color="C0", zorder=3)
    ax.plot(freqs, 360.0 * tau_s * freqs, "-", color="C1",
            label=f"linear fit, $\\tau$ = {tau_s*1e6:.1f} µs")
    ax.axhline(PHASE_CRITERION_DEG, color="C3", linestyle="--",
               label=f"1/20-period limit ({PHASE_CRITERION_DEG:.0f}°)")
    ax.axvline(f_crit, color="C3", linestyle=":",
               label=f"crossing: {f_crit:.0f} Hz")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Phase shift (deg)")
    ax.set_title("(a) Phase shift vs frequency")
    ax.grid(True, alpha=0.3)
    ax.legend(loc="lower right", fontsize=8)

    # Panel B: delay vs freq
    ax = axes[0, 1]
    mean_us = float(np.mean(delays_us))
    std_us = float(np.std(delays_us, ddof=1))
    ax.plot(freqs, delays_us, "o", color="C0", zorder=3)
    ax.axhline(mean_us, color="C2", label=f"mean {mean_us:.1f} µs")
    ax.fill_between(freqs, mean_us - std_us, mean_us + std_us,
                    color="C2", alpha=0.15, label=f"±1σ ±{std_us:.1f} µs")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Delay (µs)")
    ax.set_title("(b) End-to-end delay is constant with frequency")
    ax.set_ylim(mean_us - 4 * std_us, mean_us + 4 * std_us)
    ax.grid(True, alpha=0.3)
    ax.legend(loc="upper right", fontsize=8)

    # Panel C: software latency histogram
    ax = axes[1, 0]
    p = percentiles_us(proc_ns)
    us = proc_ns * 1e-3
    ax.hist(us, bins=np.arange(p["min"] - 1, p["max"] + 1, 0.2),
            color="C0", edgecolor="none")
    ax.axvline(p["median"], color="C1", linestyle="-",
               label=f"median {p['median']:.2f} µs")
    ax.axvline(p["p99"], color="C3", linestyle="--",
               label=f"p99 {p['p99']:.2f} µs")
    ax.set_xlabel("Software processing latency (µs)")
    ax.set_ylabel("Count")
    ax.set_yscale("log")
    ax.set_title(f"(c) Processing latency, n = {len(us)}")
    ax.grid(True, alpha=0.3)
    ax.legend(loc="upper right", fontsize=8)

    # Panel D: budget bar
    ax = axes[1, 1]
    sinc_delay_us = 2.5 / 15_000 * 1e6
    dac_settle_us = 5.0
    components = [
        ("Software loop", software_us, "C0"),
        ("ADS1256 SINC5\ngroup delay", sinc_delay_us, "C1"),
        ("DAC8552 settling", dac_settle_us, "C2"),
    ]
    bottom = 0.0
    for name, val, color in components:
        ax.bar(["predicted"], [val], bottom=bottom, color=color, label=f"{name} ({val:.1f} µs)")
        bottom += val
    ax.bar(["measured"], [measured_total_us], color="C7",
           label=f"scope total ({measured_total_us:.1f} µs)")
    ax.set_ylabel("Latency (µs)")
    ax.set_title("(d) Latency budget")
    ax.grid(True, axis="y", alpha=0.3)
    ax.legend(loc="lower right", fontsize=8, framealpha=0.95)

    fig.suptitle("Analog-to-analog latency, Raspberry Pi 5 + Waveshare HP AD/DA "
                 "(ADS1256 @ 15 kSPS)", fontsize=12)
    fig.tight_layout(rect=[0, 0, 1, 0.96])
    return _save(fig, "summary.png")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main() -> int:
    sweep_path = DATA_DIR / "phase_sweep.csv"
    char_path = DATA_DIR / "characterize_15ksps.csv"
    for p in (sweep_path, char_path):
        if not p.exists():
            sys.stderr.write(f"error: missing data file: {p}\n")
            return 1

    freqs, phases = load_phase_sweep(sweep_path)
    cdata = load_characterize(char_path)

    # Per-frequency delay (us) from phase + period.
    delays_us = (phases / 360.0) * (1.0 / freqs) * 1e6
    delay_mean_us = float(np.mean(delays_us))
    delay_std_us = float(np.std(delays_us, ddof=1))

    proc = percentiles_us(cdata["proc_ns"])
    loop = percentiles_us(cdata["loop_ns"])
    adc = percentiles_us(cdata["adc_read_ns"])
    dac = percentiles_us(cdata["dac_write_ns"])

    # -- Console summary -----------------------------------------------------
    print("=" * 78)
    print("Analog-to-analog latency experiment -- analysis")
    print("=" * 78)
    print()
    print(f"Phase sweep:        {len(freqs)} frequencies "
          f"({freqs[0]:.0f}..{freqs[-1]:.0f} Hz)")
    print(f"Software samples:   {len(cdata['proc_ns'])}  (15 kSPS, "
          f"interval {CONVERSION_INTERVAL_US:.2f} µs)")
    print()
    print("End-to-end delay (scope, CH1->CH2):")
    for f, ph, d in zip(freqs, phases, delays_us):
        print(f"  {int(f):>5d} Hz   {ph:5.2f} deg   ->   {d:6.1f} µs")
    print(f"  mean = {delay_mean_us:.1f} µs   std = {delay_std_us:.1f} µs   "
          f"(n = {len(delays_us)})")
    print()

    tau_s = float(np.sum(freqs * phases) / np.sum(freqs * freqs) / 360.0)
    f_crit = float(PHASE_CRITERION_DEG / (360.0 * tau_s))
    print(f"Linear fit through origin: tau = {tau_s*1e6:.1f} µs")
    print(f"1/20-period success limit ({PHASE_CRITERION_DEG:.0f}°) crossed at "
          f"{f_crit:.0f} Hz")
    print()

    print("Software timings (microseconds, from per-sample CSV):")
    print(fmt_row("ADC read (DRDY->ADC)", adc))
    print(fmt_row("DAC write (ADC->DAC)", dac))
    print(fmt_row("processing latency", proc))
    print(fmt_row("loop period", loop))
    print()

    # -- Plots ---------------------------------------------------------------
    out = []
    out.append(plot_system_diagram())
    out.append(plot_phase_vs_freq(freqs, phases))
    out.append(plot_delay_vs_freq(freqs, phases))
    out.append(plot_proc_latency_hist(cdata["proc_ns"]))
    out.append(plot_loop_period_hist(cdata["loop_ns"]))
    out.append(plot_budget_bar(proc["median"], delay_mean_us))
    out.append(plot_latency_timing(cdata["adc_read_ns"],
                                   cdata["dac_write_ns"],
                                   cdata["proc_ns"]))
    out.append(plot_summary(freqs, phases, delays_us,
                            cdata["proc_ns"], cdata["loop_ns"],
                            proc["median"], delay_mean_us))

    print("Wrote figures:")
    for p in out:
        print(f"  {p.relative_to(REPO_ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
