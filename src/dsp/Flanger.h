// Stereo Bucket Flanger — top-level engine (JUCE-free).
//
// Per channel (A and B identical; FLANGER I uses only A):
//   in -> input trim -> [+ feedback] -> NE570 compressor -> anti-alias LPF -> BBD (1024 stages, variable
//   clock) -> reconstruction LPF -> NE570 expander -> +/- polarity (PHASE) -> feedback tap -> wet
//   wet/dry sum -> cross-mix matrix (FLANGER III) -> output clip -> output trim -> out
// Control path: LFO (bipolar triangle) x DEPTH + MANUAL -> [CH-B: optional inversion] -> clock VCO.
// EXT CV replaces the LFO term. The delay-line island runs at 2x host rate (polyphase IIR half-bands);
// the dry path is delayed by the half-bands' 4-sample round trip so the comb is exact, and that
// 4-sample figure is the plugin's reported (oversampling) latency.
#pragma once
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include "Tunables.h"
#include "Util.h"
#include "Bbd.h"
#include "Compander.h"
#include "Lfo.h"

namespace sbf {

enum class Mode { Off = 0, Flanger1, Flanger2, Flanger3, Chorus };
enum class CvMode { Off = 0, Sidechain, Parameter };

struct Params
{
    Mode mode = Mode::Flanger2;
    double manualMs = 2.0;      // 0.5-15 (flanger) / 5-15 (chorus)
    double rateHz = 0.5;        // 0.02-5
    double depth = 0.5;         // 0-1
    double feedback = 0.0;      // 0-1 of the calibrated maximum
    bool chBModInvert = false;
    bool phaseAInvert = false, phaseBInvert = false;
    CvMode cvMode = CvMode::Off;
    double cvValue = 0.0;       // -1..1 when cvMode == Parameter
    double inTrimDb = 0.0, outTrimDb = 0.0;
    bool noise = true;
    bool bypass = false;
};

struct Meters
{
    std::atomic<float> lfo { 0.0f };
    std::atomic<bool> ledPos { false }, ledNeg { false };
    std::atomic<bool> inSignal { false }, inClip { false };
    std::atomic<float> cpuLoad { 0.0f };
};

class Flanger
{
public:
    static constexpr int kLatency = 4;   // half-band up + down round trip (3.97 samples), host samples, any rate

    Tunables tun;
    Meters meters;

    void prepare (double sampleRate, const Tunables& t)
    {
        tun = t;
        fs = sampleRate;
        os = tun.oversample > 0.5 ? 2 : 1;
        fs2 = fs * os;
        for (int c = 0; c < 2; ++c)
        {
            auto& L = line[c];
            L.bbd.prepare (fs2, tun, (uint64_t) (0x1234 + c * 7919));
            L.enc.prepare (fs2, tun.compTauMs, tun.compVref, tun.compMaxGainDb);
            L.dec.prepare (fs2, tun.compTauMs, tun.compVref, tun.compMaxGainDb);
            L.up.reset(); L.down.reset(); L.fb = 0.0;
            dry[c].reset(); dry[c].setLength (os == 2 ? kLatency : 0);
        }
        lfo.prepare (fs); lfo.ledThreshold = tun.lfoLedThreshold; lfo.reset (0.0);
        const double tau = tun.smoothMs * 1e-3;
        sManual.prepare (fs, tau); sDepth.prepare (fs, tau); sFeedback.prepare (fs, tau);
        sInTrim.prepare (fs, tau); sOutTrim.prepare (fs, tau);
        setParams (params, true);
    }

    void reset()
    {
        for (auto& L : line) { L.bbd.reset(); L.enc.reset(); L.dec.reset(); L.up.reset(); L.down.reset(); L.fb = 0.0; }
        for (auto& d : dry) d.reset();
        lfo.reset (0.0);
    }

    int latencySamples() const { return os == 2 ? kLatency : 0; }
    double sampleRate() const { return fs; }

    void setParams (const Params& p, bool immediate = false)
    {
        params = p;
        const double man = std::clamp (p.manualMs, minMs (p.mode), maxMs (p.mode));
        sManual.setTarget (controlForDelay (man * 1e-3, p.mode));
        sDepth.setTarget (std::clamp (p.depth, 0.0, 1.0));
        sFeedback.setTarget (isFlanger (p.mode) ? tun.fbMax * std::clamp (p.feedback, 0.0, 1.0) : 0.0);
        sInTrim.setTarget (dbToGain (p.inTrimDb));
        sOutTrim.setTarget (dbToGain (p.outTrimDb));
        lfo.setRate (std::clamp (p.rateHz, 0.001, 50.0));
        for (auto& L : line) L.bbd.noiseOn = p.noise;
        if (immediate)
        {
            sManual.reset (sManual.target); sDepth.reset (sDepth.target); sFeedback.reset (sFeedback.target);
            sInTrim.reset (sInTrim.target); sOutTrim.reset (sOutTrim.target);
        }
    }
    const Params& currentParams() const { return params; }

    // cv: optional per-sample external control (-1..1), used when cvMode == Sidechain
    void process (const float* inA, const float* inB, float* outA, float* outB, int n, const float* cv = nullptr)
    {
        const Mode mode = params.mode;
        float peak = 0.0f;
        const bool useA = mode != Mode::Off, useB = mode == Mode::Flanger2 || mode == Mode::Flanger3 || mode == Mode::Chorus;
        const double dryG = mode == Mode::Chorus ? tun.chorusDry : tun.flangerDry;
        const double wetG = mode == Mode::Chorus ? tun.chorusWet : tun.flangerWet;
        const double polA = params.phaseAInvert ? -1.0 : 1.0, polB = params.phaseBInvert ? -1.0 : 1.0;
        const bool lfoOn = params.cvMode == CvMode::Off;

        for (int i = 0; i < n; ++i)
        {
            const double vMan = sManual.tick(), depth = sDepth.tick(), fbG = sFeedback.tick();
            const double gIn = sInTrim.tick(), gOut = sOutTrim.tick();
            const double lfoV = lfo.tick();
            double mod = lfoV;
            if (params.cvMode == CvMode::Sidechain) mod = cv ? std::clamp ((double) cv[i], -1.0, 1.0) : 0.0;
            else if (params.cvMode == CvMode::Parameter) mod = std::clamp (params.cvValue, -1.0, 1.0);
            (void) lfoOn;

            double xA = inA[i] * gIn, xB = (inB ? inB[i] : inA[i]) * gIn;
            peak = std::max (peak, std::max (std::fabs ((float) xA), std::fabs ((float) xB)));

            if (mode == Mode::Off)
            {
                outA[i] = (float) (dry[0].tick ((float) xA) * gOut);
                outB[i] = (float) (dry[1].tick ((float) xB) * gOut);
                continue;
            }
            if (mode == Mode::Flanger1) { xA = xA + tun.fl1InputBMix * xB; xB = xA; }

            const double fA = clockFor (control (vMan, depth, mod), mode);
            const double fB = clockFor (control (vMan, depth, params.chBModInvert ? -mod : mod), mode);

            const double wetA = useA ? runLine (line[0], xA, fA, fbG, polA) : 0.0;
            const double wetB = useB ? runLine (line[1], xB, fB, fbG, polB) : 0.0;
            const double dA = dry[0].tick ((float) xA), dB = dry[1].tick ((float) xB);

            double yA, yB;
            switch (mode)
            {
                case Mode::Flanger1: yA = yB = dryG * dA + wetG * wetA; break;
                case Mode::Flanger3:
                    yA = dryG * dA + wetG * (tun.fl3Own * wetA + tun.fl3Cross * wetB);
                    yB = dryG * dB + wetG * (tun.fl3Own * wetB + tun.fl3Cross * wetA);
                    break;
                case Mode::Off: case Mode::Flanger2: case Mode::Chorus: default:
                    yA = dryG * dA + wetG * wetA; yB = dryG * dB + wetG * wetB; break;
            }
            outA[i] = (float) (softClip (yA, tun.clipLevel, tun.clipHardness) * gOut);
            outB[i] = (float) (softClip (yB, tun.clipLevel, tun.clipHardness) * gOut);
        }
        meters.lfo.store ((float) lfo.value);
        meters.ledPos.store (lfo.ledPos); meters.ledNeg.store (lfo.ledNeg);
        meters.inSignal.store (peak > (float) dbToGain (tun.inLedSignalDb));
        meters.inClip.store (peak >= 1.0f);
    }

    // ---- analysis helpers (used by the render tool / measurement scripts) ----
    double minMs (Mode m) const { return m == Mode::Chorus ? tun.chorusMinMs : tun.flangerMinMs; }
    double maxMs (Mode m) const { return m == Mode::Chorus ? tun.chorusMaxMs : tun.flangerMaxMs; }
    static bool isFlanger (Mode m) { return m == Mode::Flanger1 || m == Mode::Flanger2 || m == Mode::Flanger3; }
    double fMin (Mode m) const { return line[0].bbd.clockFor (maxMs (m) * 1e-3); }
    double fMax (Mode m) const { return line[0].bbd.clockFor (minMs (m) * 1e-3); }

    // normalised control (0..1) -> clock frequency, per the VCO law
    double clockFor (double v, Mode m) const
    {
        v = std::clamp (v, 0.0, 1.0);
        const double lo = fMin (m), hi = fMax (m);
        return tun.vcoLaw > 0.5 ? lo * std::pow (hi / lo, v) : lo + v * (hi - lo);
    }
    double controlForDelay (double delaySec, Mode m) const
    {
        const double f = line[0].bbd.clockFor (delaySec), lo = fMin (m), hi = fMax (m);
        return std::clamp (tun.vcoLaw > 0.5 ? std::log (f / lo) / std::log (hi / lo) : (f - lo) / (hi - lo), 0.0, 1.0);
    }
    double control (double vMan, double depth, double mod) const
    {
        if (tun.depthLaw > 0.5) return std::clamp (vMan + 0.5 * depth * mod, 0.0, 1.0);
        return (1.0 - depth) * vMan + depth * (0.5 + 0.5 * mod);
    }
    // wet-path delay the model expects for a static MANUAL setting: BBD + analog filter group delay
    double expectedWetDelaySec (double manualMs, Mode m) const
    {
        const double man = std::clamp (manualMs, minMs (m), maxMs (m));
        return line[0].bbd.delayFor (clockFor (controlForDelay (man * 1e-3, m), m)) + line[0].bbd.groupDelayDc();
    }
    double analogGroupDelaySec() const { return line[0].bbd.groupDelayDc(); }
    const Lfo& lfoRef() const { return lfo; }

private:
    struct Line
    {
        Ne570 enc, dec;
        Bbd bbd;
        HalfBandUp2x up;
        HalfBandDown2x down;
        double fb = 0.0;
    };

    inline double lineStep (Line& L, double x, double fClk, double fbG, double pol)
    {
        double v = x + fbG * L.fb;
        if (tun.companderOn > 0.5) v = L.enc.compress (v);
        double b = L.bbd.process (v, fClk);
        if (tun.companderOn > 0.5) b = L.dec.expand (b);
        b *= pol;
        L.fb = b;
        return b;
    }
    inline double runLine (Line& L, double x, double fClk, double fbG, double pol)
    {
        if (os == 1) return lineStep (L, x, fClk, fbG, pol);
        double u0, u1; L.up.tick (x, u0, u1);
        const double y0 = lineStep (L, u0, fClk, fbG, pol);
        const double y1 = lineStep (L, u1, fClk, fbG, pol);
        return L.down.tick (y0, y1);
    }

    Params params;
    double fs = 48000.0, fs2 = 96000.0; int os = 2;
    Line line[2];
    FixedDelay<kLatency> dry[2];
    Lfo lfo;
    Smoother sManual, sDepth, sFeedback, sInTrim, sOutTrim;
};

} // namespace sbf
