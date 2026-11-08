"""Refresh the fidelity summary table in README.md from docs/results/results.json (numbers, not adjectives)."""
from __future__ import annotations

import json
import sys
from pathlib import Path


def main():
    root = Path(__file__).resolve().parent.parent
    res = json.loads((root / (sys.argv[1] if len(sys.argv) > 1 else "docs/results/results.json")).read_text())
    by = {r["name"]: r for r in res["results"]}
    m = lambda n: by[n]["metrics"]  # noqa: E731
    rows = [("Test", "Reference", "Measured", "Gate", "Result")]

    def st(n):
        r = by[n]
        return "skipped" if r["skipped"] else ("PASS" if r["pass"] else "FAIL")

    d = m("8.1 delay law")
    rows.append(("8.1 delay law (22 points)", d["reference"], f"worst error {d['worst_error_pct']} %", "≤ 3 %", st("8.1 delay law")))
    b = m("8.2 bandwidth law")["settings"]
    rows.append(("8.2 bandwidth (−3 dB)", "published 14 kHz at 0.5 ms", "; ".join(f"{r['manual_ms']} ms: {r['f3db_hz'] / 1000:.1f} kHz" for r in b), "±10 %", st("8.2 bandwidth law")))
    mm = m("8.3 mode matrix")
    rows.append(("8.3 FLANGER III matrix", mm["reference"], f"own {mm['measured']['own_db']:.2f} dB, cross {mm['measured']['cross_db']:.2f} dB (sign {mm['measured']['cross_sign']:+d}), dry {mm['measured']['dry_db']:.2f} dB", "±0.5 dB", st("8.3 mode matrix")))
    fb = m("8.4 feedback ceiling")["settings"]
    rows.append(("8.4 feedback ceiling", "no self-oscillation", "tails " + ", ".join(f"{r['tail_dbfs']:.0f}" for r in fb) + " dBFS; comb peak " + ", ".join(f"{r['max_comb_peak_db']:.1f}" for r in fb) + " dB", "tail < −60 dBFS; peak ±1 dB vs hardware", st("8.4 feedback ceiling")))
    c = m("8.5 compander")
    rows.append(("8.5 compander", c["reference"], f"release τ {c['release_tau_ms']} ms, attack settles in {c['attack_settle_3db_ms']} ms, floor breathes {c['noise_with_tone_db'] - c['noise_without_tone_db']:.0f} dB", "τ within 20 %", st("8.5 compander")))
    lf = m("8.6 LFO")
    rows.append(("8.6 LFO", "triangle, 0.02–5 Hz", ", ".join(f"{r['rate_hz']} Hz → {r['measured_hz']:.4g} Hz" for r in lf["rates"]) + f"; triangle corr ≥ {min(r['triangle_corr'] for r in lf['rates']):.4f}", "±5 %, triangle, LEDs at peaks", st("8.6 LFO")))
    th = m("THD+N sweep")["levels"]
    rows.append(("THD+N (1 kHz)", "hardware curve pending", ", ".join(f"{r['level_dbfs']} dBFS: {r['thdn_db']:.0f} dB" for r in th if r["level_dbfs"] in (-40, -20, -10, 0)), "informational", st("THD+N sweep")))
    al = m("aliasing")["cases"]
    rows.append(("aliasing", "≤ −80 dBFS", "; ".join(f"{r['tone_hz'] // 1000} kHz @ {r['input_dbfs']} dBFS: {r['worst_alias_dbfs']:.0f} dBFS" for r in al), "gate at −12 dBFS", st("aliasing")))
    nf = m("noise floor")["cases"]
    rows.append(("noise floor", "< −70 dBm", "; ".join(f"{r['manual_ms']} ms: {r['rms_dbm']:.0f} dBm" for r in nf if r["noise"] == "on") + f"; off: {[r['rms_dbfs'] for r in nf if r['noise'] == 'off'][0]:.0f} dBFS", "< −70 dBm", st("noise floor")))
    rows.append(("null test vs hardware", "—", "no captures yet", "< −30 dB", st("null test")))
    cp = m("CPU / latency")
    rows.append(("CPU / latency", "48 kHz, 64 samples", ", ".join(f"{r['mode']} {r['cpu_percent']:.1f} %" for r in cp["cpu_48k_64"]) + f"; latency {cp['latency_samples']} samples", "informational", st("CPU / latency")))

    table = ["| " + " | ".join(rows[0]) + " |", "|---|---|---|---|---|"] + ["| " + " | ".join(r) + " |" for r in rows[1:]]
    readme = root / "README.md"
    txt = readme.read_text()
    a, b_ = "<!-- FIDELITY_TABLE_START -->", "<!-- FIDELITY_TABLE_END -->"
    i, j = txt.index(a) + len(a), txt.index(b_)
    readme.write_text(txt[:i] + "\n" + "\n".join(table) + "\n" + txt[j:])
    print("\n".join(table))


if __name__ == "__main__":
    main()
