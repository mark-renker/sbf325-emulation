"""CPU and latency budget: per-instance CPU at 48 kHz / 64-sample buffers (renderer --cpu), reported latency."""
from __future__ import annotations

import json
import subprocess
from common import Ctx, result, standalone


def run(ctx: Ctx):
    rows = []
    for mode, manual in (("fl1", 2), ("fl2", 2), ("fl3", 2), ("chorus", 10), ("fl2", 0.5)):
        cmd = [str(ctx.renderer), "--cpu", "--mode", mode, "--manual", str(manual), "--depth", "50", "--rate", "0.5"]
        for s in ctx.sets:
            cmd += ["--set", s]
        r = json.loads(subprocess.run(cmd, check=True, capture_output=True, text=True).stdout)
        rows.append({"mode": mode, "manual_ms": manual, "cpu_percent": round(r["cpuPercent"], 2)})
    info = ctx.info()
    return result("CPU / latency", info["latencySamples"] == 4, {"cpu_48k_64": rows, "latency_samples": info["latencySamples"],
                  "latency_note": "0 algorithmic; 4 samples oversampling (half-band up/down round trip), reported to the host"},
                  gate="informational CPU; latency reported exactly")


if __name__ == "__main__":
    standalone(run)
