"""8.6 LFO: waveform shape (triangle vs other), rate accuracy at min/mid/max, +/- peak LED timing.

Gate: rate within 5 % at 0.02 / 0.3 / 5 Hz; waveform correlates > 0.999 with an ideal triangle; the LED
flags straddle the waveform peaks. Uses the renderer's --lfo-wav tap (channel 0 = LFO, channel 1 = LED).
"""
from __future__ import annotations

import numpy as np
from common import Ctx, fig, save, result, standalone, SERIES


def run(ctx: Ctx):
    rows, ok, plots = [], True, []
    for i, rate in enumerate([0.02, 0.3, 5.0]):
        length = max(3.0, 2.5 / rate)
        y, sr, lfo = ctx.render(f"lfo_{rate}", "--mode", "fl2", "--rate", str(rate), "--depth", "100", "--gen", "silence", "--len", str(length), lfo_wav=True)
        v, led = lfo[:, 0], lfo[:, 1]
        # period from upward zero crossings
        zc = np.where((v[:-1] < 0) & (v[1:] >= 0))[0]
        per = np.diff(zc) / sr
        meas = 1.0 / np.mean(per) if len(per) else float("nan")
        err = abs(meas - rate) / rate * 100
        # triangle fit: ideal triangle at the measured rate, best phase
        t = np.arange(len(v)) / sr
        ph = (t * meas) % 1.0
        best = 0.0
        for shift in np.linspace(0, 1, 200, endpoint=False):
            p = (ph + shift) % 1.0
            tri = np.where(p < 0.5, 4 * p - 1, 3 - 4 * p)
            c = float(np.corrcoef(v, tri)[0, 1])
            best = max(best, c)
        sine = float(max(np.corrcoef(v, np.sin(2 * np.pi * (ph + s)))[0, 1] for s in np.linspace(0, 1, 50, endpoint=False)))
        # LED timing: the positive LED must be on at the positive peak, off at the zero crossing
        pk = np.argmax(v[int(0.1 * len(v)):]) + int(0.1 * len(v))
        led_ok = led[pk] > 0.5 and all(abs(led[z]) < 0.5 for z in zc[:3])
        on_frac = float(np.mean(led > 0.5))
        rows.append({"rate_hz": rate, "measured_hz": round(meas, 5), "error_pct": round(err, 3), "triangle_corr": round(best, 5),
                     "sine_corr": round(sine, 4), "led_at_peak": bool(led_ok), "led_on_fraction": round(on_frac, 3)})
        ok &= err <= 5 and best > 0.999 and led_ok
        if rate == 0.3:
            f, ax = fig("8.6 LFO waveform and +/- peak LEDs at 0.3 Hz", "time (s)", "LFO (normalised)")
            sel = t < 7
            ax.plot(t[sel], v[sel], color=SERIES[0], lw=1.6, label="LFO")
            ax.plot(t[sel], led[sel] * 0.95, color=SERIES[1], lw=1.0, label="LED (+1 right, -1 left)")
            ax.legend(frameon=False)
            plots.append(save(f, ctx, "lfo"))
    shape = "triangle" if all(r["triangle_corr"] > 0.999 for r in rows) else "other"
    return result("8.6 LFO", ok, {"rates": rows, "waveform": shape}, plots, gate="rate within 5 %; triangle; LEDs at peaks")


if __name__ == "__main__":
    standalone(run)
