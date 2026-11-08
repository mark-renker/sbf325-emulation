"""Null test against reference hardware captures (Section 8, first bullet).

Expects, under --reference, pairs named <case>_dry.wav / <case>_wet.wav plus <case>.json holding the
renderer parameters ({"args": ["--mode", "fl2", "--manual", "2", ...]}). The dry capture is rendered
through the model with those parameters, time-aligned to the wet capture by cross-correlation, gain
matched at 1 kHz, subtracted; residual reported in dB RMS relative to the wet capture, with its spectrum.
Gate: residual better than -30 dB on each static case. Skipped when no captures are present.
"""
from __future__ import annotations

import json
import numpy as np
from common import Ctx, read_wav, delay_by_xcorr, rms_db, spectrum, db, fig, save, result, standalone, SERIES


def run(ctx: Ctx):
    if ctx.reference is None or not list(ctx.reference.glob("*_wet.wav")):
        return result("null test", True, {}, notes="no reference captures under --reference; skipped", skipped=True, gate="residual < -30 dB")
    rows, plots, ok = [], [], True
    for wet_path in sorted(ctx.reference.glob("*_wet.wav")):
        case = wet_path.name[:-8]
        dry_path = ctx.reference / f"{case}_dry.wav"
        cfg = ctx.reference / f"{case}.json"
        if not dry_path.exists() or not cfg.exists():
            continue
        args = json.loads(cfg.read_text()).get("args", [])
        wet, sr = read_wav(wet_path)
        ctx.sr = sr
        model, _ = ctx.render(f"null_{case}", *args, inp=dry_path)
        n = min(len(wet), len(model))
        a, b = wet[:n, 0], model[:n, 0]
        d = delay_by_xcorr(a, b, sr, 0.2)
        shift = int(round(d * sr))
        b = np.roll(b, -shift)
        g = np.dot(a, b) / max(np.dot(b, b), 1e-30)
        resid = a - g * b
        res_db = rms_db(resid) - rms_db(a)
        rows.append({"case": case, "align_shift_samples": shift, "gain_match": round(float(g), 4), "residual_db": round(res_db, 2)})
        ok &= res_db < -30
        f, ax = fig(f"Null residual spectrum: {case}", "frequency (Hz)", "level (dBFS)", log_x=True)
        fr, m = spectrum(a, sr)
        fr2, r = spectrum(resid, sr)
        ax.plot(fr, db(m), color=SERIES[0], lw=0.7, label="hardware wet")
        ax.plot(fr2, db(r), color=SERIES[1], lw=0.7, label="residual")
        ax.legend(frameon=False)
        plots.append(save(f, ctx, f"null_{case}"))
    return result("null test", ok, {"cases": rows}, plots, gate="residual < -30 dB")


if __name__ == "__main__":
    standalone(run)
