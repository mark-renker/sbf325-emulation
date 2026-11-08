"""8.4 Feedback ceiling: FEEDBACK = 100 % at every MANUAL setting must not self-oscillate; report the
first comb peak gain.

Gate: output decays after the input stops (tail RMS < -60 dBFS 2 s after the burst) at 0.5/1/2/5/10/15 ms;
peak gain within 1 dB of hardware when reference/feedback_peak.csv ("manual_ms,peak_db") exists.
"""
from __future__ import annotations

import csv
import numpy as np
from common import Ctx, rms_db, transfer_from_sweep, smooth_db, db, fig, save, result, standalone, SERIES

SETTINGS = [0.5, 1.0, 2.0, 5.0, 10.0, 15.0]


def run(ctx: Ctx):
    hw = {}
    ref = ctx.ref_file("feedback_peak.csv")
    if ref:
        with open(ref) as fh:
            for row in csv.DictReader(fh):
                hw[float(row["manual_ms"])] = float(row["peak_db"])
    rows, ok, curves = [], True, []
    sweep, sr = ctx.render("fb_in", "--mode", "off", "--gen", "sweep:20:20000", "--len", "4", "--amp", "-30", "--channels", "a")
    for ms in SETTINGS:
        # burst then silence: 0.5 s of 1 kHz at -12 dBFS, then 2.5 s of nothing
        y, sr = ctx.render(f"fb_burst_{ms}", "--mode", "fl2", "--manual", str(ms), "--depth", "0", "--feedback", "100",
                           "--gen", "burst:1000:0.5:10", "--len", "3", "--amp", "-12", "--channels", "a")
        tail = rms_db(y[int(2.5 * sr):, 0])
        during = rms_db(y[int(0.2 * sr):int(0.5 * sr), 0])
        peak_abs = float(np.max(np.abs(y[:, 0])))
        decays = tail < -60 and peak_abs < 1.0
        # comb peak gain from a low-level sweep (linear regime)
        s, _ = ctx.render(f"fb_sweep_{ms}", "--mode", "fl2", "--manual", str(ms), "--depth", "0", "--feedback", "100", "--noise", "off",
                          "--gen", "sweep:20:20000", "--len", "4", "--amp", "-30", "--channels", "a")
        f, H = transfer_from_sweep(sweep[:, 0], s[:, 0], sr, 20, 20000)
        m = smooth_db(f, db(H), 1 / 48)
        peak = float(np.max(m[(f > 40) & (f < 18000)]))
        first_peak_hz = float(f[(f > 40) & (f < 18000)][np.argmax(m[(f > 40) & (f < 18000)])])
        r = {"manual_ms": ms, "tail_dbfs": round(tail, 1), "during_dbfs": round(during, 1), "peak_abs": round(peak_abs, 3),
             "decays": decays, "max_comb_peak_db": round(peak, 2), "peak_at_hz": round(first_peak_hz, 1)}
        if ms in hw:
            r["ref_peak_db"] = hw[ms]
            r["peak_err_db"] = round(abs(peak - hw[ms]), 2)
            ok &= r["peak_err_db"] <= 1.0
        ok &= decays
        rows.append(r)
        curves.append((ms, f, m))
    f, ax = fig("8.4 Comb response at FEEDBACK = 100 %", "frequency (Hz)", "gain (dB)", log_x=True)
    for i, (ms, fr, m) in enumerate(curves):
        ax.plot(fr, m, color=SERIES[i], lw=1.2, label=f"{ms} ms")
    ax.legend(frameon=False, ncol=3)
    plots = [save(f, ctx, "feedback_ceiling")]
    return result("8.4 feedback ceiling", ok, {"settings": rows, "reference": "hardware csv" if hw else "none (decay gate only)"},
                  plots, gate="no self-oscillation at 100 %; comb peak within 1 dB of hardware")


if __name__ == "__main__":
    standalone(run)
