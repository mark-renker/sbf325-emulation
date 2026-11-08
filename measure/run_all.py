"""Run every measurement against the standalone renderer, write results.json + RESULTS.md with plots,
and exit non-zero if any Section 8 gate fails.

    python3 measure/run_all.py build/sbf_render --out docs/results [--reference captures/]
"""
from __future__ import annotations

import argparse
import datetime as dt
import json
import platform
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from common import Ctx  # noqa: E402

import delay_law, bandwidth, mode_matrix, feedback_ceiling, compander, lfo, thd, aliasing, noise_floor, null_test, cpu  # noqa: E402

SCRIPTS = [delay_law, bandwidth, mode_matrix, feedback_ceiling, compander, lfo, thd, aliasing, noise_floor, null_test, cpu]


def fmt(v):
    if isinstance(v, float):
        return f"{v:.4g}"
    return str(v)


def table(rows):
    if not rows:
        return ""
    keys = list(rows[0].keys())
    out = ["| " + " | ".join(keys) + " |", "|" + "---|" * len(keys)]
    for r in rows:
        out.append("| " + " | ".join(fmt(r.get(k, "")) for k in keys) + " |")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("renderer")
    ap.add_argument("--out", default="docs/results")
    ap.add_argument("--reference", default=None)
    ap.add_argument("--set", action="append", default=[])
    ap.add_argument("--only", default=None, help="comma-separated script names")
    a = ap.parse_args()
    ctx = Ctx(Path(a.renderer), Path(a.out), Path(a.reference) if a.reference else None, a.set)
    results = []
    for mod in SCRIPTS:
        name = mod.__name__
        if a.only and name not in a.only.split(","):
            continue
        print(f"== {name}", flush=True)
        try:
            r = mod.run(ctx)
        except Exception as e:  # keep going, report the failure
            r = {"name": name, "pass": False, "skipped": False, "gate": "", "metrics": {}, "plots": [], "notes": f"exception: {e!r}"}
        print(("   SKIP " if r["skipped"] else "   PASS " if r["pass"] else "   FAIL ") + r["name"] + (f"  ({r['notes']})" if r["notes"] else ""), flush=True)
        results.append(r)
    info = ctx.info()
    report = {"generated": dt.datetime.now().isoformat(timespec="seconds"), "machine": platform.platform(), "renderer": str(ctx.renderer),
              "sample_rate": ctx.sr, "tunables": info["tunables"], "results": results}
    (ctx.out / "results.json").write_text(json.dumps(report, indent=2))

    md = ["# Measured results", "", f"Generated {report['generated']} on {report['machine']} at {ctx.sr} Hz by `measure/run_all.py`.",
          "Reference: " + ("hardware captures in `" + str(ctx.reference) + "`" if ctx.reference else "**none** - gates compare the model against its own declared constants and the published specifications; no reference unit was measured."), "",
          "| Test | Gate | Result |", "|---|---|---|"]
    for r in results:
        st = "skipped" if r["skipped"] else ("**PASS**" if r["pass"] else "**FAIL**")
        md.append(f"| {r['name']} | {r['gate']} | {st} |")
    for r in results:
        md += ["", f"## {r['name']}", ""]
        if r["notes"]:
            md += [r["notes"], ""]
        for k, v in r["metrics"].items():
            if isinstance(v, list) and v and isinstance(v[0], dict):
                md += [f"**{k}**", "", table(v), ""]
            elif isinstance(v, dict):
                md += [f"**{k}**: " + ", ".join(f"{kk} = {fmt(vv)}" for kk, vv in v.items()), ""]
            else:
                md += [f"**{k}**: {fmt(v)}", ""]
        for p in r["plots"]:
            md.append(f"![{r['name']}]({p})")
            md.append("")
    (ctx.out / "RESULTS.md").write_text("\n".join(md))
    failed = [r["name"] for r in results if not r["pass"] and not r["skipped"]]
    print("\n" + ("ALL GATES PASS" if not failed else "FAILED: " + ", ".join(failed)))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
