"""8.5 Compander behaviour: noise-floor modulation during a 1 kHz tone burst (tone on/off).

The expander opens with the tone and closes after it, so the device's noise floor breathes. We measure
the envelope of the output above 3 kHz (away from the tone). Release = time constant fitted to the log
envelope between -4 and -14 dB after the tone stops (the NE570 averager is first order, so this is a
straight line); attack = time after the tone starts until the floor settles within 3 dB (the feedback
compressor overshoots, so a 10-90 % figure is not meaningful). Gate: within 20 % of the reference
(hardware csv reference/compander.csv "attack_ms,release_tau_ms"); without captures the release must be
within 20 % of the model's declared time constant and the attack must settle within 3 tau.
"""
from __future__ import annotations

import csv
import numpy as np
from common import Ctx, envelope, fig, save, result, standalone, db


def release_tau(env_db, sr, t_off, ref_db):
    """Fit the log-envelope slope between -4 and -14 dB (re the steady level) after t_off; return tau (ms)."""
    seg = env_db[t_off:t_off + int(0.5 * sr)] - ref_db
    i1 = int(np.argmax(seg < -4))
    i2 = int(np.argmax(seg < -14))
    if i2 <= i1 + 10:
        return float("nan")
    t = np.arange(i1, i2) / sr
    slope = np.polyfit(t, seg[i1:i2], 1)[0]  # dB/s
    return -8.685889638 / slope * 1e3


def attack_settle(env_db, sr, t_on, ref_db, delay_s):
    """Time (ms) after the tone starts (plus the BBD delay) at which the floor first comes within 3 dB of its settled level."""
    start = t_on + int(delay_s * sr)
    seg = env_db[start:start + int(0.5 * sr)] - ref_db
    i = int(np.argmax(seg > -3))
    return i / sr * 1e3


def run(ctx: Ctx):
    hw = None
    ref = ctx.ref_file("compander.csv")
    if ref:
        with open(ref) as fh:
            r = next(csv.DictReader(fh))
            hw = (float(r["attack_ms"]), float(r["release_tau_ms"]))
    info = ctx.info("--manual", "1")
    tau = info["tunables"]["compTauMs"]
    # 1 kHz at the compander's unity level (-16 dB re full scale); a raised BBD noise so the floor is well above -120 dB
    y, sr = ctx.render("comp_burst", "--mode", "fl2", "--manual", "1", "--depth", "0", "--feedback", "0", "--wet-only",
                       "--set", "bbdNoiseRms=0.001", "--gen", "burst:1000:1.0:1.0", "--len", "4", "--amp", "-16", "--channels", "a")
    env = envelope(y[:, 0], sr, hp_hz=3000, win_s=0.003)
    env_db = db(env)
    on_db = float(np.median(env_db[int(2.5 * sr):int(2.9 * sr)]))
    off_db = float(np.median(env_db[int(3.6 * sr):int(3.95 * sr)]))
    attack = attack_settle(env_db, sr, int(2.0 * sr), on_db, info["expectedWetDelaySec"])  # tone on at 2.0 s
    release = release_tau(env_db, sr, int(3.0 * sr), on_db)                               # off at 3.0 s
    exp_att, exp_rel = hw if hw else (3 * tau, tau)
    e_rel = abs(release - exp_rel) / exp_rel * 100
    e_att = abs(attack - exp_att) / exp_att * 100 if hw else 0.0
    att_ok = e_att <= 20 if hw else attack <= exp_att
    f, ax = fig("8.5 Noise-floor envelope (>3 kHz) around a 1 kHz tone burst", "time (s)", "envelope (dB)")
    t = np.arange(len(env)) / sr
    sel = (t > 1.8) & (t < 3.4)
    ax.plot(t[sel], db(env[sel]), color="#2a78d6", lw=1.4)
    ax.axvline(2.0, color="#52514e", lw=0.8, ls="--")
    ax.axvline(3.0, color="#52514e", lw=0.8, ls="--")
    plots = [save(f, ctx, "compander")]
    ok = att_ok and e_rel <= 20 and on_db > off_db + 6
    return result("8.5 compander", ok, {"attack_settle_3db_ms": round(attack, 2), "release_tau_ms": round(release, 2),
                  "expected_attack_ms": round(exp_att, 2), "expected_release_tau_ms": round(exp_rel, 2),
                  "attack_err_pct": round(e_att, 1), "release_err_pct": round(e_rel, 1),
                  "noise_with_tone_db": round(on_db, 1), "noise_without_tone_db": round(off_db, 1),
                  "reference": "hardware csv" if hw else f"model tau = {tau} ms"}, plots,
                  gate="release tau within 20 % of reference; attack settles")


if __name__ == "__main__":
    standalone(run)
