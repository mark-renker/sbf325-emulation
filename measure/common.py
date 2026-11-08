"""Shared helpers for the measurement scripts: renderer invocation, WAV I/O, spectra, plotting.

Every script in this directory takes rendered WAVs (from tools/Render.cpp) and emits numbers and
plots. No listening descriptions. Thresholds come from the spec (docs/SPEC_*.pdf, Section 8).
"""
from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

try:  # soundfile is preferred; fall back to scipy for environments without libsndfile
    import soundfile as _sf

    def read_wav(path):
        data, sr = _sf.read(str(path), dtype="float64", always_2d=True)
        return data, sr

    def write_wav(path, data, sr):
        _sf.write(str(path), np.asarray(data, dtype="float32"), sr, subtype="FLOAT")
except ImportError:  # pragma: no cover
    from scipy.io import wavfile as _wf

    def read_wav(path):
        sr, data = _wf.read(str(path))
        data = np.asarray(data)
        if data.dtype.kind == "i":
            data = data / float(2 ** (8 * data.dtype.itemsize - 1))
        if data.ndim == 1:
            data = data[:, None]
        return data.astype("float64"), sr

    def write_wav(path, data, sr):
        _wf.write(str(path), sr, np.asarray(data, dtype="float32"))

# Categorical colours in fixed order (validated palette, light surface)
SERIES = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4", "#008300", "#6250d6", "#e34948"]
INK = "#0b0b0b"
INK2 = "#52514e"
GRID = "#e4e3df"

FS = 96000  # capture rate mandated by the spec (96 kHz / 24-bit)


@dataclass
class Ctx:
    renderer: Path
    out: Path
    reference: Path | None = None
    sets: list[str] = field(default_factory=list)
    sr: int = FS
    tmp: Path | None = None

    def __post_init__(self):
        self.renderer = Path(self.renderer).resolve()
        self.out = Path(self.out).resolve()
        self.out.mkdir(parents=True, exist_ok=True)
        (self.out / "plots").mkdir(exist_ok=True)
        self.tmp = Path(tempfile.mkdtemp(prefix="sbf_measure_"))
        if self.reference is not None:
            self.reference = Path(self.reference).resolve()

    def render(self, name: str, *args: str, lfo_wav: bool = False, inp: Path | None = None):
        """Run the renderer with parameters; returns (data[frames, 2], sr) and optionally the LFO WAV."""
        out = self.tmp / f"{name}.wav"
        cmd = [str(self.renderer), "--sr", str(self.sr)]
        for s in self.sets:
            cmd += ["--set", s]
        cmd += list(args)
        lfo = None
        if lfo_wav:
            lfo = self.tmp / f"{name}_lfo.wav"
            cmd += ["--lfo-wav", str(lfo)]
        if inp is not None:
            cmd.append(str(inp))
        cmd.append(str(out))
        subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
        data, sr = read_wav(out)
        if lfo_wav:
            ldata, _ = read_wav(lfo)
            return data, sr, ldata
        return data, sr

    def info(self, *args: str) -> dict:
        cmd = [str(self.renderer), "--info", "--sr", str(self.sr)]
        for s in self.sets:
            cmd += ["--set", s]
        cmd += list(args)
        return json.loads(subprocess.run(cmd, check=True, capture_output=True, text=True).stdout)

    def ref_file(self, name: str) -> Path | None:
        if self.reference is None:
            return None
        p = self.reference / name
        return p if p.exists() else None


# ---------------------------------------------------------------- signal analysis

def db(x):
    return 20.0 * np.log10(np.maximum(np.abs(x), 1e-30))


def rms_db(x):
    return 20.0 * np.log10(max(float(np.sqrt(np.mean(np.square(x)))), 1e-30))


def spectrum(x, sr, nfft=None):
    """Blackman-Harris windowed magnitude spectrum, peak-amplitude normalised (a full-scale sine -> 0 dBFS)."""
    x = np.asarray(x, dtype="float64")
    n = len(x) if nfft is None else nfft
    w = np.blackman(len(x)) if False else _bh(len(x))
    X = np.fft.rfft(x * w, n=n)
    mag = 2.0 * np.abs(X) / np.sum(w)
    f = np.fft.rfftfreq(n, 1.0 / sr)
    return f, mag


def _bh(n):
    k = np.arange(n) / max(n - 1, 1)
    return 0.35875 - 0.48829 * np.cos(2 * np.pi * k) + 0.14128 * np.cos(4 * np.pi * k) - 0.01168 * np.cos(6 * np.pi * k)


def tone_level(x, sr, f, bw=None):
    """Peak amplitude (linear) of the strongest bin within +-bw of f."""
    fr, mag = spectrum(x, sr)
    bw = bw or 3 * sr / len(x)
    sel = (fr >= f - bw) & (fr <= f + bw)
    return float(mag[sel].max()) if sel.any() else 0.0


def transfer_from_sweep(inp, out, sr, f0=20.0, f1=20000.0):
    """Complex transfer function H(f) = Out/In from an exponential sweep (spectral division with a floor)."""
    n = 1 << int(np.ceil(np.log2(len(inp) + len(out))))
    I = np.fft.rfft(inp, n)
    O = np.fft.rfft(out, n)
    f = np.fft.rfftfreq(n, 1.0 / sr)
    floor = np.max(np.abs(I)) * 1e-4
    H = O / np.where(np.abs(I) < floor, floor, I)
    sel = (f >= f0) & (f <= f1)
    return f[sel], H[sel]


def smooth_db(f, mag_db, frac_oct=1 / 12):
    """Fractional-octave smoothing of a dB response (running mean over a log-frequency window, O(N))."""
    lf = np.log2(np.maximum(f, 1e-6))
    half = frac_oct / 2
    lo = np.searchsorted(lf, lf - half, side="left")
    hi = np.searchsorted(lf, lf + half, side="right")
    cs = np.concatenate([[0.0], np.cumsum(mag_db)])
    return (cs[hi] - cs[lo]) / np.maximum(hi - lo, 1)


def delay_by_xcorr(ref, sig, sr, max_delay_s=0.05):
    """Delay of sig relative to ref (seconds) via cross-correlation peak with parabolic interpolation."""
    n = 1 << int(np.ceil(np.log2(len(ref) + len(sig))))
    R = np.fft.rfft(ref, n)
    S = np.fft.rfft(sig, n)
    xc = np.fft.irfft(S * np.conj(R), n)
    m = int(max_delay_s * sr)
    seg = xc[: m + 1]
    k = int(np.argmax(seg))
    if 0 < k < len(seg) - 1:
        y0, y1, y2 = seg[k - 1], seg[k], seg[k + 1]
        den = y0 - 2 * y1 + y2
        k = k + (0.5 * (y0 - y2) / den if den != 0 else 0.0)
    return k / sr


def envelope(x, sr, hp_hz=None, win_s=0.002):
    """Short-time RMS envelope (optionally after a high-pass) at the signal rate."""
    from scipy.signal import butter, sosfiltfilt

    y = np.asarray(x, dtype="float64")
    if hp_hz:
        sos = butter(4, hp_hz / (sr / 2), btype="high", output="sos")
        y = sosfiltfilt(sos, y)
    w = max(int(win_s * sr), 1)
    e = np.sqrt(np.convolve(np.square(y), np.ones(w) / w, mode="same"))
    return e


# ---------------------------------------------------------------- results / plotting

def fig(title, xlabel, ylabel, log_x=False):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    f, ax = plt.subplots(figsize=(8, 4.2), dpi=110)
    f.patch.set_facecolor("#fcfcfb")
    ax.set_facecolor("#fcfcfb")
    ax.set_title(title, loc="left", color=INK, fontsize=11)
    ax.set_xlabel(xlabel, color=INK2)
    ax.set_ylabel(ylabel, color=INK2)
    ax.grid(True, color=GRID, linewidth=0.8)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    for s in ("left", "bottom"):
        ax.spines[s].set_color(GRID)
    ax.tick_params(colors=INK2)
    if log_x:
        ax.set_xscale("log")
    return f, ax


def save(f, ctx: Ctx, name: str) -> str:
    p = ctx.out / "plots" / f"{name}.png"
    f.tight_layout()
    f.savefig(p)
    import matplotlib.pyplot as plt

    plt.close(f)
    return str(p.relative_to(ctx.out))


def result(name, passed, metrics, plots=(), notes="", gate="", skipped=False):
    return {"name": name, "pass": bool(passed), "skipped": skipped, "gate": gate, "metrics": metrics, "plots": list(plots), "notes": notes}


def standalone(run):
    """Make a script runnable on its own: python delay_law.py <renderer> [--out dir] [--reference dir]."""
    import argparse

    ap = argparse.ArgumentParser()
    ap.add_argument("renderer")
    ap.add_argument("--out", default="measure/results")
    ap.add_argument("--reference", default=None)
    ap.add_argument("--set", action="append", default=[])
    a = ap.parse_args()
    ctx = Ctx(Path(a.renderer), Path(a.out), Path(a.reference) if a.reference else None, a.set)
    r = run(ctx)
    print(json.dumps(r, indent=2))
    sys.exit(0 if r["pass"] or r["skipped"] else 1)
