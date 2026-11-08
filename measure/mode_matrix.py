"""8.3 Mode matrix: FLANGER III, signal into channel A only; measure wet / dry / cross coefficients.

Gate: coefficients within 0.5 dB of the reference (hardware csv reference/mode_matrix.csv with
"coef,db" rows own/cross/dry, else the model's declared tunables). Also verifies FLANGER I/II routing.
"""
from __future__ import annotations

import csv
import numpy as np
from common import Ctx, tone_level, db, result, standalone, fig, save, SERIES


def run(ctx: Ctx):
    hw = {}
    ref = ctx.ref_file("mode_matrix.csv")
    if ref:
        with open(ref) as fh:
            for row in csv.DictReader(fh):
                hw[row["coef"]] = float(row["db"])
    info = ctx.info()
    t = info["tunables"]
    amp = 10 ** (-20 / 20)
    base = ["--mode", "fl3", "--manual", "5", "--depth", "0", "--feedback", "0", "--noise", "off",
            "--gen", "sine:1000", "--len", "1.5", "--amp", "-20", "--channels", "a"]
    wet, sr = ctx.render("mm_wet", *base, "--wet-only")
    dry, _ = ctx.render("mm_dry", *base, "--dry-only")
    seg = slice(int(0.5 * sr), None)
    own = tone_level(wet[seg, 0], sr, 1000) / amp
    cross = tone_level(wet[seg, 1], sr, 1000) / amp
    dryA = tone_level(dry[seg, 0], sr, 1000) / amp
    dryB = tone_level(dry[seg, 1], sr, 1000) / amp
    # sign of the cross term from the correlation between the two wet outputs
    sign = np.sign(float(np.dot(wet[seg, 0], wet[seg, 1])))
    meas = {"own_db": db(own), "cross_db": db(cross), "dry_db": db(dryA), "dry_leak_to_B_db": db(dryB), "cross_sign": int(sign)}
    expect = hw if hw else {"own": db(abs(t["fl3Own"] * t["flangerWet"])), "cross": db(abs(t["fl3Cross"] * t["flangerWet"])), "dry": db(abs(t["flangerDry"]))}
    errs = {k: abs(meas[f"{k}_db"] - expect[k]) for k in ("own", "cross", "dry")}
    ok = all(e <= 0.5 for e in errs.values()) and dryB < 1e-4
    if not hw:
        ok &= sign == np.sign(t["fl3Cross"])
    # FL1 / FL2 routing sanity
    fl2, _ = ctx.render("mm_fl2", "--mode", "fl2", "--manual", "5", "--depth", "0", "--noise", "off", "--gen", "sine:1000", "--len", "1", "--amp", "-20", "--channels", "a")
    fl1, _ = ctx.render("mm_fl1", "--mode", "fl1", "--manual", "5", "--depth", "0", "--noise", "off", "--gen", "sine:1000", "--len", "1", "--amp", "-20", "--channels", "b")
    leak = tone_level(fl2[seg, 1], sr, 1000) / amp
    fl1_same = float(np.max(np.abs(fl1[seg, 0] - fl1[seg, 1])))
    ok &= leak < 1e-4 and fl1_same < 1e-6
    meas.update({"fl2_B_leak_db": db(leak), "fl1_output_difference": fl1_same})
    f, ax = fig("8.3 FLANGER III matrix: signal into A only (1 kHz)", "coefficient", "level (dB re input)")
    names = ["own wet (A->A)", "cross wet (A->B)", "dry (A->A)"]
    vals = [meas["own_db"], meas["cross_db"], meas["dry_db"]]
    exp = [expect["own"], expect["cross"], expect["dry"]]
    x = np.arange(3)
    ax.bar(x - 0.18, vals, 0.34, color=SERIES[0], label="measured")
    ax.bar(x + 0.18, exp, 0.34, color=SERIES[1], label="reference")
    ax.set_xticks(x, names)
    ax.legend(frameon=False)
    plots = [save(f, ctx, "mode_matrix")]
    return result("8.3 mode matrix", ok, {"measured": {k: (round(v, 3) if isinstance(v, float) else v) for k, v in meas.items()},
                  "expected_db": {k: round(v, 3) for k, v in expect.items()}, "errors_db": {k: round(v, 3) for k, v in errs.items()},
                  "reference": "hardware csv" if hw else "declared tunables"}, plots, gate="own/cross/dry within 0.5 dB")


if __name__ == "__main__":
    standalone(run)
