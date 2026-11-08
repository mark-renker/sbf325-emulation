"""8.2 Bandwidth law: -3 dB point of the wet path at six delay settings (0.5, 1, 2, 5, 10, 15 ms).

Gate: within +-10 % of the reference. Reference = hardware csv (reference/bandwidth.csv: "manual_ms,f3db_hz")
when present. Without captures the published 75 Hz-14 kHz figure is gated at the short-delay end (0.5 ms,
fastest clock) and the other settings are reported; bandwidth must not shrink as the clock speeds up.
(At 15 ms the clock is 34 kHz: the bucket hold alone costs 2.6 dB at 14 kHz and the 1:2 expander doubles
that, so 14 kHz cannot be the slow-clock figure for this topology - see docs/ASSUMPTIONS.md.)
"""
from __future__ import annotations

import csv
import numpy as np
from common import Ctx, transfer_from_sweep, smooth_db, fig, save, result, standalone, db, SERIES

SETTINGS = [0.5, 1.0, 2.0, 5.0, 10.0, 15.0]


def run(ctx: Ctx):
    hw = {}
    ref = ctx.ref_file("bandwidth.csv")
    if ref:
        with open(ref) as fh:
            for row in csv.DictReader(fh):
                hw[float(row["manual_ms"])] = float(row["f3db_hz"])
    rows, curves = [], []
    sweep, sr = ctx.render("bw_in", "--mode", "off", "--gen", "sweep:20:40000", "--len", "4", "--amp", "-12", "--channels", "a")
    for ms in SETTINGS:
        y, sr = ctx.render(f"bw_{ms}", "--mode", "fl2", "--manual", str(ms), "--depth", "0", "--feedback", "0", "--wet-only",
                           "--noise", "off", "--gen", "sweep:20:40000", "--len", "4", "--amp", "-12", "--channels", "a")
        f, H = transfer_from_sweep(sweep[:, 0], y[:, 0], sr, 20, 40000)
        m = smooth_db(f, db(H), 1 / 24)
        ref_db = np.mean(m[(f > 200) & (f < 1000)])
        above = f[(m < ref_db - 3) & (f > 1000)]
        f3 = float(above[0]) if len(above) else float("nan")
        rows.append({"manual_ms": ms, "f3db_hz": round(f3, 1), "ref_hz": hw.get(ms)})
        curves.append((ms, f, m - ref_db))
    f, ax = fig("8.2 Wet-path magnitude response vs delay setting", "frequency (Hz)", "relative level (dB)", log_x=True)
    for i, (ms, fr, m) in enumerate(curves):
        sel = (fr >= 20) & (fr <= 40000)
        ax.plot(fr[sel], m[sel], color=SERIES[i], lw=1.6, label=f"{ms} ms")
    ax.axhline(-3, color="#52514e", lw=0.8, ls="--")
    ax.set_ylim(-40, 3)
    ax.legend(frameon=False, ncol=3)
    plots = [save(f, ctx, "bandwidth")]
    ok = True
    notes = []
    if hw:
        for r in rows:
            if r["ref_hz"]:
                e = abs(r["f3db_hz"] - r["ref_hz"]) / r["ref_hz"] * 100
                r["error_pct"] = round(e, 2)
                ok &= e <= 10
    else:
        fast = [r for r in rows if r["manual_ms"] == 0.5][0]["f3db_hz"]
        ok = abs(fast - 14000) / 14000 <= 0.10
        notes.append(f"no hardware captures: gating the 0.5 ms setting against the published 14 kHz (+-10 %); got {fast:.0f} Hz")
        f3s = [r["f3db_hz"] for r in rows][::-1]
        ok &= all(b >= a * 0.98 for a, b in zip(f3s, f3s[1:]))  # bandwidth must not shrink as the clock speeds up
    return result("8.2 bandwidth law", ok, {"settings": rows}, plots, "; ".join(notes), gate="-3 dB point within +-10 % of reference")


if __name__ == "__main__":
    standalone(run)
