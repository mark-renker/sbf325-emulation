// Bucket-brigade delay simulated at its own (variable) clock.
//
// A 1024-stage BBD driven by a two-phase clock at f_clk moves a charge packet through two stages per
// period, so it behaves as a 512-sample delay line sampled at f_clk. Per processing sample (period T):
//   * the clock ticks at instants t_k = (1 - phase) * Tc + k * Tc that fall inside [0, T);
//   * between events the analog anti-alias filter integrates the piecewise-linear input exactly and
//     the reconstruction filter integrates the held (zero-order) BBD output exactly (ModalLpf);
//   * at each tick the anti-alias filter output is sampled into the bucket chain (soft-clipped, with
//     the device's noise) and the chain's oldest bucket becomes the new held output.
// This is the variable-rate resampler approach of Raffel & Smith (DAFx 2010) / Huovilainen (DAFx 2005).
#pragma once
#include <vector>
#include <cmath>
#include "ModalFilter.h"
#include "Tunables.h"
#include "Util.h"

namespace sbf {

struct Bbd
{
    int half = 512;
    std::vector<float> ring;
    int w = 0;

    ModalLpf aa, recon;
    ModalLpf::Step aaT, reT;          // full-period steps (no tick)
    ModalLpf::Step aaTc, reTc;        // tick-spacing steps (within one period)
    ModalLpf::Step aaF, reF;          // fragment steps
    double lastTc = -1.0;

    double fs = 96000.0, T = 1.0 / 96000.0;
    double phase = 0.0;               // fraction of the clock period elapsed since the last tick
    double xPrev = 0.0;               // input at the start of the current period
    double hold = 0.0;                // held BBD output

    double loss = 1.0, noiseRms = 0.0, feedthrough = 0.0, clipL = 1.0, clipP = 4.0;
    double reconTrack = 0.0, reconCutoff = 15000.0; int reconOrder = 4;
    double ftPhase = 0.0;
    bool noiseOn = true;
    Noise rng;

    void prepare (double sampleRate, const Tunables& t, uint64_t seed = 1)
    {
        fs = sampleRate; T = 1.0 / fs;
        half = std::max (2, (int) std::lround (t.bbdStages / 2.0));
        ring.assign ((size_t) half, 0.0f);
        w = 0; phase = 0.0; xPrev = 0.0; hold = 0.0; ftPhase = 0.0; lastTc = -1.0;
        loss = t.bbdLoss; noiseRms = t.bbdNoiseRms; feedthrough = t.clockFeedthrough;
        clipL = t.clipLevel; clipP = t.clipHardness;
        reconTrack = t.reconTrack; reconCutoff = t.reconCutoffHz; reconOrder = (int) t.reconOrder;
        aa.design ((int) t.aaOrder, t.aaCutoffHz);
        recon.design (reconOrder, reconCutoff);
        aa.prepareTable (T); recon.prepareTable (T);
        aa.makeStep (aaT, T); recon.makeStep (reT, T);
        aa.reset(); recon.reset();
        rng.seed (seed);
    }

    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        w = 0; phase = 0.0; xPrev = 0.0; hold = 0.0; lastTc = -1.0;
        aa.reset(); recon.reset();
    }

    // nominal delay (seconds) for a clock frequency
    double delayFor (double fClk) const { return half / fClk; }
    double clockFor (double delaySec) const { return half / delaySec; }

    inline void tick()
    {
        double v = aa.out();
        v = softClip (v, clipL, clipP);
        if (noiseOn && noiseRms > 0) v += noiseRms * rng.fastGaussian();
        const float outV = ring[(size_t) w];
        ring[(size_t) w] = (float) v;
        if (++w >= half) w = 0;
        hold = loss * outV;
    }

    // one processing-rate sample; x is the input value at the END of this period
    inline double process (double x, double fClk)
    {
        const double Tc = 1.0 / fClk;
        if (reconTrack > 0.0)
        {
            const double want = std::min (reconCutoff, reconTrack * fClk);
            if (std::fabs (want - recon.fc) > 0.01 * recon.fc) { recon.design (reconOrder, want); recon.makeStep (reT, T); lastTc = -1.0; }
        }
        double t = 0.0;
        double tNext = (1.0 - phase) * Tc;
        if (tNext >= T)
        {
            aa.advanceLinear (aaT, xPrev, x);
            recon.advanceHold (reT, hold);
            phase += T / Tc;
        }
        else
        {
            if (std::fabs (Tc - lastTc) > 1e-15) { aa.makeStep (aaTc, Tc); recon.makeStep (reTc, Tc); lastTc = Tc; }
            const double slope = (x - xPrev) / T;
            // first fragment
            aa.makeStep (aaF, tNext); recon.makeStep (reF, tNext);
            aa.advanceLinear (aaF, xPrev, xPrev + slope * tNext);
            recon.advanceHold (reF, hold);
            t = tNext; tick();
            // full tick intervals
            while (t + Tc < T)
            {
                aa.advanceLinear (aaTc, xPrev + slope * t, xPrev + slope * (t + Tc));
                recon.advanceHold (reTc, hold);
                t += Tc; tick();
            }
            // last fragment
            const double dl = T - t;
            aa.makeStep (aaF, dl); recon.makeStep (reF, dl);
            aa.advanceLinear (aaF, xPrev + slope * t, x);
            recon.advanceHold (reF, hold);
            phase = dl / Tc;
        }
        xPrev = x;
        double y = recon.out();
        if (noiseOn && feedthrough > 0.0 && fClk < 0.5 * fs)
        {
            ftPhase += fClk * T; if (ftPhase >= 1.0) ftPhase -= 1.0;
            y += feedthrough * recon.magnitude (fClk) * std::sin (2.0 * kPi * ftPhase);
        }
        return y;
    }

    double groupDelayDc() const { return aa.groupDelayDc() + recon.groupDelayDc(); }
};

} // namespace sbf
