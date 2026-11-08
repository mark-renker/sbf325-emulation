"""THD+N vs input level: 1 kHz, -40..0 dBFS in 5 dB steps, static setting (FLANGER II, 15 ms, no modulation).

MANUAL = 15 ms puts 1 kHz and its harmonics on comb peaks, so the wet/dry sum does not weight them.
Informational against the hardware curve (reference/thd.csv "level_dbfs,thdn_db"); the self-gate is
THD+N <= -50 dB at -20 dBFS and monotonic behaviour (no NaN/inf).
"""
from __future__ import annotations

import csv
import numpy as np
from common import Ctx, spectrum, fig, save, result, standalone, SERIES

LEVELS = list(range(-40, 1, 5))


def thdn(x, sr, f0=1000.0):
    fr, mag = spectrum(x, sr)
    band = (fr >= 20) & (fr <= 20000)
    fund = (fr >= f0 - 10) & (fr <= f0 + 10)
    p_total = np.sum(mag[band] ** 2)
    p_fund = np.sum(mag[band & fund] ** 2)
    return 10 * np.log10(max(p_total - p_fund, 1e-30) / max(p_fund, 1e-30))


def run(ctx: Ctx):
    hw = {}
    ref = ctx.ref_file("thd.csv")
    if ref:
        with open(ref) as fh:
            for row in csv.DictReader(fh):
                hw[int(float(row["level_dbfs"]))] = float(row["thdn_db"])
    rows = []
    for lv in LEVELS:
        y, sr = ctx.render(f"thd_{lv}", "--mode", "fl2", "--manual", "15", "--depth", "0", "--feedback", "0",
                           "--gen", "sine:1000", "--len", "1.5", "--amp", str(lv), "--channels", "a")
        seg = y[int(0.5 * sr):, 0]
        rows.append({"level_dbfs": lv, "thdn_db": round(float(thdn(seg, sr)), 2), "ref_db": hw.get(lv)})
    f, ax = fig("THD+N vs input level (1 kHz, FLANGER II, 15 ms, static)", "input level (dBFS)", "THD+N (dB)")
    ax.plot([r["level_dbfs"] for r in rows], [r["thdn_db"] for r in rows], "-o", color=SERIES[0], lw=2, ms=5, label="model")
    if hw:
        ax.plot(sorted(hw), [hw[k] for k in sorted(hw)], "--o", color=SERIES[1], lw=1.5, ms=4, label="hardware")
        ax.legend(frameon=False)
    plots = [save(f, ctx, "thd")]
    at20 = [r for r in rows if r["level_dbfs"] == -20][0]["thdn_db"]
    ok = np.isfinite([r["thdn_db"] for r in rows]).all() and at20 <= -50
    return result("THD+N sweep", ok, {"levels": rows, "reference": "hardware csv" if hw else "none (informational)"}, plots,
                  gate="finite; THD+N at -20 dBFS <= -50 dB (hardware curve when available)")


if __name__ == "__main__":
    standalone(run)
