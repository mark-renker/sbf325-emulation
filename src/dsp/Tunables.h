// Stereo Bucket Flanger — model constants that the schematic/measurements may revise.
// Every value the spec (docs/SPEC_SBF-325_Flanger_Emulation.pdf §10) leaves open lives here,
// so it can be overridden at runtime (render CLI --set key=value, plugin tunables file)
// instead of being baked into code. Defaults are documented in docs/ASSUMPTIONS.md.
#pragma once
#include <cstring>
#include <cmath>
#include <string>

namespace sbf {

// X(name, default, doc)
#define SBF_TUNABLES(X)                                                                                   \
    /* ---- BBD / clock ---- */                                                                           \
    X(bbdStages,        1024.0, "BBD stages (MN3007 = 1024); delay = stages / (2 f_clk)")                \
    X(flangerMinMs,     0.5,    "FLANGER shortest delay, ms (service notes: clock period 1 us)")          \
    X(flangerMaxMs,     15.0,   "FLANGER longest delay, ms (clock period ~30 us)")                        \
    X(chorusMinMs,      5.0,    "CHORUS shortest delay, ms")                                              \
    X(chorusMaxMs,      15.0,   "CHORUS longest delay, ms")                                               \
    X(vcoLaw,           0.0,    "clock VCO law: 0 = linear f_clk vs control (delay ~ 1/V), 1 = exponential")\
    X(depthLaw,         0.0,    "0 = crossfade (DEPTH=10 removes MANUAL, spans full range), 1 = additive, clamped")\
    X(aaCutoffHz,       18000.0,"anti-alias LPF (before BBD) -3 dB, Hz, Butterworth (fixed; see ASSUMPTIONS)")                      \
    X(aaOrder,          3.0,    "anti-alias LPF order (1..4)")                                             \
    X(reconCutoffHz,    18000.0,"reconstruction LPF (after BBD) -3 dB, Hz, Butterworth (fixed)")                  \
    X(reconOrder,       4.0,    "reconstruction LPF order (1..4)")                                         \
    X(reconTrack,       0.0,    "if > 0, reconstruction cutoff = min(reconCutoffHz, reconTrack * f_clk)")  \
    X(bbdLoss,          1.0,    "BBD charge-transfer gain per pass (1 = lossless)")                        \
    X(bbdNoiseRms,      3e-5,   "white noise injected per BBD sample, rms (full scale = 1.0 = +20 dBm)")  \
    X(clockFeedthrough, 2e-5,   "clock feedthrough amplitude at the BBD output before reconstruction LPF")  \
    X(clipLevel,        1.0,    "soft-clip ceiling of BBD input stage / output op-amp (1.0 = +20 dBm)")   \
    X(clipHardness,     16.0,   "soft-clip knee exponent (4/8/16 fast paths; 16 = op-amp-like hard knee)")                          \
    /* ---- NE570 compander ---- */                                                                       \
    X(companderOn,      1.0,    "1 = NE570 2:1/1:2 compander in the delay loop, 0 = bypass it")           \
    X(compTauMs,        22.0,   "NE570 rectifier averaging time constant, ms (10 kOhm x C_rect)")         \
    X(compVref,         0.158,  "compander unity level (sine peak, full scale 1.0): -16 dB re +20 dBm")   \
    X(compMaxGainDb,    40.0,   "maximum compressor gain / expander attenuation, dB")                     \
    /* ---- feedback ---- */                                                                              \
    X(fbMax,            0.95,   "loop gain at FEEDBACK = 100 % (factory trim: just before oscillation)")  \
    /* ---- LFO ---- */                                                                                   \
    X(lfoLedThreshold,  0.9,    "|LFO| above which the +/- peak LEDs light")                              \
    /* ---- modes / mix matrix ---- */                                                                    \
    X(fl1InputBMix,     1.0,    "FLANGER I: gain of input B summed into channel A's line")                \
    X(fl3Own,           1.0,    "FLANGER III: gain of each channel's own wet signal")                     \
    X(fl3Cross,        -0.5,    "FLANGER III: gain of the other channel's wet signal (cross-mix)")        \
    X(flangerDry,       1.0,    "FLANGER I/II/III dry gain in the wet/dry sum")                           \
    X(flangerWet,       1.0,    "FLANGER I/II/III wet gain in the wet/dry sum")                           \
    X(chorusDry,        1.0,    "CHORUS dry gain")                                                        \
    X(chorusWet,        1.0,    "CHORUS wet gain")                                                        \
    /* ---- plugin behaviour ---- */                                                                      \
    X(smoothMs,         20.0,   "parameter smoothing time constant for MANUAL/DEPTH/FEEDBACK, ms")        \
    X(inLedSignalDb,   -40.0,   "input LED green threshold, dBFS after input trim")                       \
    X(oversample,       1.0,    "1 = BBD island runs at 2x host rate (default), 0 = host rate (lower CPU)")

struct Tunables
{
#define SBF_DECL(n, d, doc) double n = d;
    SBF_TUNABLES(SBF_DECL)
#undef SBF_DECL

    bool set (const char* key, double v)
    {
#define SBF_SET(n, d, doc) if (std::strcmp (key, #n) == 0) { n = v; return true; }
        SBF_TUNABLES(SBF_SET)
#undef SBF_SET
        return false;
    }
    bool get (const char* key, double& v) const
    {
#define SBF_GET(n, d, doc) if (std::strcmp (key, #n) == 0) { v = n; return true; }
        SBF_TUNABLES(SBF_GET)
#undef SBF_GET
        return false;
    }
    template <class F> void forEach (F f) const
    {
#define SBF_EACH(n, d, doc) f (#n, n, d, doc);
        SBF_TUNABLES(SBF_EACH)
#undef SBF_EACH
    }
    // "key=value" (returns false if the key is unknown)
    bool setFromString (const std::string& kv)
    {
        auto eq = kv.find ('=');
        if (eq == std::string::npos) return false;
        return set (kv.substr (0, eq).c_str(), std::atof (kv.c_str() + eq + 1));
    }
};

} // namespace sbf
