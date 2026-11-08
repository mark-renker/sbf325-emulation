"""Aliasing: 10 kHz and 15 kHz sines, DEPTH 100 % (the clock sweeps its whole 34 kHz-1 MHz range), FLANGER II.

RATE is set to 0.1 Hz rather than the 5 Hz maximum: with the 1/x clock law the delay changes at up to
4.4 s per second at 5 Hz, so the wet tone's Doppler skirt covers the entire band and nothing could be
told apart from aliasing. At 0.1 Hz the skirt is within +-9 % of the tone while every clock rate is
still visited. Sampling images (f_clk - f) lie above 19 kHz, so the search band is 50 Hz up to 15 %
below the tone: anything there is an alias / intermodulation product of the implementation.
Gate: no component above -80 dBFS at -12 dBFS input (linear regime). The 0 dBFS case is reported;
there the modelled input-stage clipping creates harmonics that the BBD aliases, which the hardware
is expected to share (spec: "unless the hardware shows the same behaviour") - confirm with captures.
"""
from __future__ import annotations

import numpy as np
from common import Ctx, spectrum, db, fig, save, result, standalone, SERIES


def run(ctx: Ctx):
    rows, plots, ok = [], [], True
    for amp in (-12, 0):
        f, ax = fig(f"Aliasing: 10 / 15 kHz at {amp} dBFS, DEPTH 100 %, RATE 0.1 Hz", "frequency (Hz)", "level (dBFS)")
        for i, tone in enumerate((10000, 15000)):
            y, sr = ctx.render(f"alias_{tone}_{amp}", "--mode", "fl2", "--manual", "2", "--depth", "100", "--rate", "0.1", "--feedback", "0",
                               "--noise", "off", "--gen", f"sine:{tone}", "--len", "6", "--amp", str(amp), "--channels", "a")
            fr, mag = spectrum(y[int(0.5 * sr):, 0], sr)
            m = db(mag)
            band = (fr >= 50) & (fr <= 0.85 * tone)
            worst = float(np.max(m[band]))
            at = float(fr[band][np.argmax(m[band])])
            rows.append({"input_dbfs": amp, "tone_hz": tone, "worst_alias_dbfs": round(worst, 1), "at_hz": round(at, 1)})
            if amp == -12:
                ok &= worst <= -80
            sel = fr <= 24000
            ax.plot(fr[sel], m[sel], color=SERIES[i], lw=0.8, label=f"{tone // 1000} kHz input")
        ax.axhline(-80, color="#52514e", lw=0.8, ls="--")
        ax.set_ylim(-140, 5)
        ax.legend(frameon=False)
        plots.append(save(f, ctx, f"aliasing_{abs(amp)}"))
    return result("aliasing", ok, {"cases": rows}, plots, gate="products below the tone <= -80 dBFS at -12 dBFS input; 0 dBFS reported")


if __name__ == "__main__":
    standalone(run)
