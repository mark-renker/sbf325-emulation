"""8.1 Delay law: wet-path delay vs MANUAL at 11 points, FLANGER and CHORUS ranges.

Gate: measured delay within 3 % of the reference. With hardware captures (reference/delay_law.csv:
"mode,manual_ms,delay_us") the reference is the hardware; otherwise the reference is the model's own
nominal delay (stages / 2 f_clk plus the analog filters' low-frequency group delay) from --info.
"""
from __future__ import annotations

import csv
import numpy as np
from common import Ctx, delay_by_xcorr, fig, save, result, standalone

POINTS = {"fl2": np.linspace(0.5, 15.0, 11), "chorus": np.linspace(5.0, 15.0, 11)}


def run(ctx: Ctx):
    hw = {}
    ref = ctx.ref_file("delay_law.csv")
    if ref:
        with open(ref) as fh:
            for row in csv.DictReader(fh):
                hw[(row["mode"], round(float(row["manual_ms"]), 3))] = float(row["delay_us"])
    rows, worst = [], 0.0
    for mode, pts in POINTS.items():
        for ms in pts:
            args = ["--mode", mode, "--manual", f"{ms:.4f}", "--depth", "0", "--feedback", "0", "--wet-only",
                    "--noise", "off", "--gen", "impulse", "--len", "0.3", "--amp", "-6", "--channels", "a"]
            y, sr = ctx.render(f"delay_{mode}_{ms:.2f}", *args)
            info = ctx.info("--mode", mode, "--manual", f"{ms:.4f}")
            imp = np.zeros(len(y))
            imp[int(0.1 * sr)] = 1.0
            d = delay_by_xcorr(imp, y[:, 0], sr) - info["latencySamples"] / sr
            key = (mode, round(float(ms), 3))
            expect = hw[key] * 1e-6 if key in hw else info["expectedWetDelaySec"]
            err = abs(d - expect) / expect * 100
            worst = max(worst, err)
            rows.append({"mode": mode, "manual_ms": round(float(ms), 3), "measured_us": round(d * 1e6, 2),
                         "expected_us": round(expect * 1e6, 2), "error_pct": round(err, 3)})
    f, ax = fig("8.1 Delay law: wet-path delay vs MANUAL", "MANUAL setting (ms)", "measured delay (ms)")
    for i, mode in enumerate(POINTS):
        r = [x for x in rows if x["mode"] == mode]
        ax.plot([x["manual_ms"] for x in r], [x["measured_us"] / 1000 for x in r], "-o", color=["#2a78d6", "#eb6834"][i], lw=2, ms=5, label=f"{mode.upper()} measured")
        ax.plot([x["manual_ms"] for x in r], [x["expected_us"] / 1000 for x in r], "--", color=["#2a78d6", "#eb6834"][i], lw=1, label=f"{mode.upper()} reference")
    ax.legend(frameon=False)
    plots = [save(f, ctx, "delay_law")]
    return result("8.1 delay law", worst <= 3.0, {"points": rows, "worst_error_pct": round(worst, 3),
                  "reference": "hardware csv" if hw else "model nominal (no hardware captures)"},
                  plots, gate="worst error <= 3 % of reading")


if __name__ == "__main__":
    standalone(run)
