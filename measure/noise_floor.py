"""Noise floor: silence in, FLANGER II, noise model on. Published spec: < -70 dBm (0 dBm = 0.775 V RMS).

Full scale (1.0) in the model is +20 dBm (the input/output maximum), so -70 dBm = -90 dBFS.
Gate: broadband output noise (20 Hz - 20 kHz) < -90 dBFS with noise on, and < -120 dBFS with noise off.
"""
from __future__ import annotations

import numpy as np
from common import Ctx, spectrum, db, rms_db, fig, save, result, standalone, SERIES


def run(ctx: Ctx):
    rows, plots = [], []
    f, ax = fig("Noise floor spectrum (silence in, FLANGER II, 5 ms)", "frequency (Hz)", "level (dBFS / bin)", log_x=True)
    for i, (noise, manual) in enumerate([("on", 5), ("on", 0.5), ("on", 15), ("off", 5)]):
        y, sr = ctx.render(f"noise_{noise}_{manual}", "--mode", "fl2", "--manual", str(manual), "--depth", "0", "--feedback", "0",
                           "--noise", noise, "--gen", "silence", "--len", "2", "--channels", "a")
        seg = y[int(0.5 * sr):, 0]
        from scipy.signal import butter, sosfiltfilt
        sos = butter(4, [20 / (sr / 2), 20000 / (sr / 2)], btype="band", output="sos")
        level = rms_db(sosfiltfilt(sos, seg))
        rows.append({"noise": noise, "manual_ms": manual, "rms_dbfs": round(level, 1), "rms_dbm": round(level + 20, 1)})
        fr, mag = spectrum(seg, sr)
        sel = (fr >= 20) & (fr <= 48000)
        ax.plot(fr[sel], db(mag[sel]), color=SERIES[i], lw=0.7, label=f"noise {noise}, {manual} ms")
    ax.set_ylim(-200, -60)
    ax.legend(frameon=False)
    plots.append(save(f, ctx, "noise_floor"))
    on = [r["rms_dbfs"] for r in rows if r["noise"] == "on"]
    off = [r["rms_dbfs"] for r in rows if r["noise"] == "off"]
    ok = max(on) < -90 and max(off) < -120
    return result("noise floor", ok, {"cases": rows}, plots, gate="< -70 dBm (-90 dBFS) with noise on; < -120 dBFS off")


if __name__ == "__main__":
    standalone(run)
