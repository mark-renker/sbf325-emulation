// Unit tests for each DSP block (no framework): BBD resampler, modal filters, compander, LFO, matrix,
// half-band, WAV I/O, feedback ceiling. Build: clang++ -std=c++17 -O2 tests/UnitTests.cpp -o sbf_tests
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <complex>
#include "../src/dsp/Flanger.h"
#include "../src/dsp/Wav.h"

using namespace sbf;
static int fails = 0, passes = 0;
#define CHECK(cond, ...) do { if (cond) { ++passes; } else { ++fails; std::printf ("  FAIL %s:%d  ", __FILE__, __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

static double rms (const std::vector<double>& v, size_t from = 0) { double s = 0; size_t c = 0; for (size_t i = from; i < v.size(); ++i) { s += v[i] * v[i]; ++c; } return c ? std::sqrt (s / c) : 0; }
// Blackman-Harris windowed DFT at one frequency: returns the complex amplitude (peak-normalised)
static std::complex<double> sineDft (const std::vector<double>& v, double f, double fs, size_t from)
{
    std::complex<double> acc = 0; double wsum = 0; const size_t N = v.size() - from;
    for (size_t i = from; i < v.size(); ++i)
    {
        const double x = (double) (i - from) / (double) (N - 1);
        const double w = 0.35875 - 0.48829 * std::cos (2 * kPi * x) + 0.14128 * std::cos (4 * kPi * x) - 0.01168 * std::cos (6 * kPi * x);
        acc += w * v[i] * std::exp (std::complex<double> (0, -2 * kPi * f * i / fs)); wsum += w;
    }
    return 2.0 * acc / wsum;
}
static double sineLevel (const std::vector<double>& v, double f, double fs, size_t from) { return std::abs (sineDft (v, f, fs, from)); }

// ---- modal filter vs analytic Butterworth ----------------------------------------------------
static void testModal()
{
    std::printf ("[modal filter]\n");
    const double fs = 96000, T = 1 / fs;
    for (int order : { 2, 3, 4 })
    {
        ModalLpf f; f.design (order, 15000); f.prepareTable (T);
        ModalLpf::Step st; f.makeStep (st, T);
        for (double hz : { 1000.0, 5000.0, 15000.0, 30000.0 })
        {
            f.reset(); std::vector<double> y; double xp = 0;
            for (int i = 0; i < 9600; ++i) { double x = std::sin (2 * kPi * hz * (i + 1) * T); f.advanceLinear (st, xp, x); xp = x; y.push_back (f.out()); }
            const double got = sineLevel (y, hz, fs, 4800), want = f.magnitude (hz) * std::fabs (std::sin (kPi * hz * T) / (kPi * hz * T)) * std::fabs (std::sin (kPi * hz * T) / (kPi * hz * T));
            // linear interpolation of the drive adds a sinc^2 droop; compare against that
            CHECK (std::fabs (gainToDb (got) - gainToDb (want)) < 0.3, "order %d @ %g Hz: got %.3f dB want %.3f dB", order, hz, gainToDb (got), gainToDb (want));
        }
        // table vs direct exp
        for (double dt : { 0.0, 1e-7, 3.3e-6, T }) { ModalLpf::Step s1, s2; f.makeStep (s1, dt); f.tab[0].v.clear(); f.makeStep (s2, dt); f.prepareTable (T);
            CHECK (std::abs (s1.e[0] - s2.e[0]) < 1e-7, "exp table error %g at dt=%g", std::abs (s1.e[0] - s2.e[0]), dt); }
        CHECK (f.groupDelayDc() > 0 && f.groupDelayDc() < 1e-4, "group delay %g", f.groupDelayDc());
    }
}

// ---- BBD: delay = 512 / f_clk, flat passband, images filtered -----------------------------
static void testBbd()
{
    std::printf ("[bbd]\n");
    Tunables t; t.bbdNoiseRms = 0; t.clockFeedthrough = 0;
    const double fs = 96000;
    for (double fClk : { 1.024e6, 200e3, 34133.0 })
    {
        Bbd b; b.prepare (fs, t);
        const double want = 512.0 / fClk + b.groupDelayDc();
        // impulse (as a one-sample triangle of the linear drive) -> find centroid of the response
        std::vector<double> y; const int n = (int) (fs * 0.03);
        for (int i = 0; i < n; ++i) y.push_back (b.process (i == 10 ? 1.0 : 0.0, fClk));
        double num = 0, den = 0; for (int i = 0; i < n; ++i) { num += i * y[(size_t) i] * y[(size_t) i]; den += y[(size_t) i] * y[(size_t) i]; }
        const double got = (num / den - 10) / fs;
        CHECK (std::fabs (got - want) / want < 0.03, "f_clk %g: delay %.1f us want %.1f us", fClk, got * 1e6, want * 1e6);
        // passband gain at 1 kHz
        Bbd b2; b2.prepare (fs, t); std::vector<double> s;
        for (int i = 0; i < 19200; ++i) s.push_back (b2.process (0.1 * std::sin (2 * kPi * 1000 * (i + 1) / fs), fClk));
        const double g = sineLevel (s, 1000, fs, 9600) / 0.1;
        CHECK (std::fabs (gainToDb (g)) < 0.3, "f_clk %g: 1 kHz gain %.2f dB", fClk, gainToDb (g));
    }
    // aliasing: 1 kHz at the fastest clock must not produce components elsewhere above -90 dB
    {
        Bbd b; b.prepare (fs, t); std::vector<double> s;
        for (int i = 0; i < 48000; ++i) s.push_back (b.process (0.5 * std::sin (2 * kPi * 1000 * (i + 1) / fs), 1.024e6));
        double worst = 0; for (double hz = 300; hz < 40000; hz += 250) if (std::fabs (hz - 1000) > 150) worst = std::max (worst, sineLevel (s, hz, fs, 24000));
        CHECK (gainToDb (worst / 0.5) < -90, "spur %.1f dB", gainToDb (worst / 0.5));
    }
    // clock-dependent bandwidth: at 34 kHz clock the sampling images of a 12 kHz tone sit at 22 kHz
    {
        Bbd b; b.prepare (fs, t); std::vector<double> s;
        for (int i = 0; i < 48000; ++i) s.push_back (b.process (0.5 * std::sin (2 * kPi * 12000 * (i + 1) / fs), 34133.0));
        const double img = sineLevel (s, 34133.0 - 12000, fs, 24000) / 0.5;
        CHECK (img > 1e-3 && img < 0.5, "image level %.1f dB (expected a real, filtered image)", gainToDb (img));
    }
    // variable clock: sweeping the clock must not blow up or go silent
    {
        Bbd b; b.prepare (fs, t); double peak = 0, e = 0;
        for (int i = 0; i < 96000; ++i) { double f = 34133.0 + (1.024e6 - 34133.0) * 0.5 * (1 + std::sin (2 * kPi * 3 * i / fs)); double y = b.process (0.3 * std::sin (2 * kPi * 440 * i / fs), f); peak = std::max (peak, std::fabs (y)); e += y * y; }
        CHECK (peak < 0.6 && e > 1, "swept clock peak %g energy %g", peak, e);
    }
}

// ---- compander ------------------------------------------------------------------------------
static void testCompander()
{
    std::printf ("[compander]\n");
    const double fs = 96000;
    for (double lvlDb : { -30.0, -16.0, 0.0 })
    {
        Ne570 enc, dec; enc.prepare (fs, 22, 0.158, 40); dec.prepare (fs, 22, 0.158, 40);
        const double A = 0.158 * dbToGain (lvlDb); std::vector<double> y, e;
        for (int i = 0; i < 48000; ++i) { double x = A * std::sin (2 * kPi * 1000 * i / fs); double c = enc.compress (x); e.push_back (c); y.push_back (dec.expand (c)); }
        const double g = sineLevel (y, 1000, fs, 24000) / A, ge = sineLevel (e, 1000, fs, 24000) / 0.158;
        CHECK (std::fabs (gainToDb (g)) < 0.5, "round trip at %g dB: %.2f dB", lvlDb, gainToDb (g));
        CHECK (std::fabs (gainToDb (ge) - 0.5 * lvlDb) < 0.7, "2:1 law at %g dB: encoded %.2f dB (want %.2f)", lvlDb, gainToDb (ge), 0.5 * lvlDb);
    }
    // envelope time constant: expander gain after a step should reach 63 % in ~tau
    {
        Ne570 dec; dec.prepare (fs, 22, 0.158, 40); int n63 = -1;
        for (int i = 0; i < 48000; ++i) { dec.expand (0.158 * std::sin (2 * kPi * 1000 * i / fs)); if (n63 < 0 && dec.env > 0.63 * 0.158) n63 = i; }
        CHECK (std::fabs (n63 / fs - 0.022) < 0.004, "tau %.1f ms", n63 / fs * 1e3);
    }
}

// ---- LFO ------------------------------------------------------------------------------------
static void testLfo()
{
    std::printf ("[lfo]\n");
    Lfo l; l.prepare (48000); l.setRate (2.0); l.reset();
    std::vector<double> v; int pos = 0, neg = 0; for (int i = 0; i < 48000; ++i) { v.push_back (l.tick()); pos += l.ledPos; neg += l.ledNeg; }
    double mn = 1, mx = -1; for (double x : v) { mn = std::min (mn, x); mx = std::max (mx, x); }
    CHECK (mn <= -0.999 && mx >= 0.999, "range %g..%g", mn, mx);
    // zero crossings => 2 Hz: 4 crossings per second
    int zc = 0; for (size_t i = 1; i < v.size(); ++i) if ((v[i - 1] < 0) != (v[i] < 0)) ++zc;
    CHECK (zc == 4, "zero crossings %d", zc);
    // triangle: |d/dt| constant
    double d1 = std::fabs (v[100] - v[99]), d2 = std::fabs (v[5000] - v[4999]);
    CHECK (std::fabs (d1 - d2) < 1e-9 && std::fabs (d1 - 8.0 / 48000) < 1e-9, "slope %g %g", d1, d2);
    CHECK (pos > 0 && neg > 0 && std::fabs (pos - neg) < 10, "LED counts %d %d", pos, neg);
}

// ---- half-band round trip delay ---------------------------------------------------------------
static void testHalfband()
{
    std::printf ("[half-band]\n");
    for (double hz : { 500.0, 2000.0, 8000.0 })
    {
        HalfBandUp2x up; HalfBandDown2x dn; std::vector<double> x, y;
        for (int i = 0; i < 9600; ++i) { double a, b; double s = std::sin (2 * kPi * hz * i / 48000.0); up.tick (s, a, b); x.push_back (s); y.push_back (dn.tick (a, b)); }
        const double ph = std::arg (sineDft (y, hz, 48000, 4800) / sineDft (x, hz, 48000, 4800));
        double d = -ph / (2 * kPi * hz) * 48000.0; while (d < Flanger::kLatency - 2) d += 48000.0 / hz;
        CHECK (std::fabs (d - Flanger::kLatency) < 0.25, "round-trip phase delay at %g Hz: %.2f samples (expect %d)", hz, d, Flanger::kLatency);
    }
    // unity gain at 1 kHz
    HalfBandUp2x u2; HalfBandDown2x d2; std::vector<double> s;
    for (int i = 0; i < 9600; ++i) { double a, b; u2.tick (std::sin (2 * kPi * 1000 * i / 48000.0), a, b); s.push_back (d2.tick (a, b)); }
    CHECK (std::fabs (sineLevel (s, 1000, 48000, 4800) - 1.0) < 0.01, "gain %g", sineLevel (s, 1000, 48000, 4800));
}

// ---- mode matrix / routing ----------------------------------------------------------------------
static void testMatrix()
{
    std::printf ("[mode matrix]\n");
    Tunables t; t.bbdNoiseRms = 0; t.clockFeedthrough = 0;
    auto render = [&] (Mode m, bool feedA, bool feedB, double dryG, double wetG, std::vector<double>& oa, std::vector<double>& ob)
    {
        Tunables tt = t; tt.flangerDry = dryG; tt.flangerWet = wetG;
        Flanger f; f.prepare (48000, tt); Params p; p.mode = m; p.depth = 0; p.feedback = 0; p.manualMs = 5; p.noise = false; f.setParams (p, true);
        const int n = 24000; std::vector<float> a (n), b (n), ya (n), yb (n);
        for (int i = 0; i < n; ++i) { float s = (float) (0.1 * std::sin (2 * kPi * 1000 * i / 48000.0)); a[(size_t) i] = feedA ? s : 0; b[(size_t) i] = feedB ? s : 0; }
        f.process (a.data(), b.data(), ya.data(), yb.data(), n);
        oa.assign (ya.begin(), ya.end()); ob.assign (yb.begin(), yb.end());
    };
    std::vector<double> oa, ob;
    // FLANGER III: signal into A only, wet-only -> own in A, cross in B
    render (Mode::Flanger3, true, false, 0, 1, oa, ob);
    const double own = sineLevel (oa, 1000, 48000, 12000) / 0.1, cross = sineLevel (ob, 1000, 48000, 12000) / 0.1;
    CHECK (std::fabs (own - std::fabs (t.fl3Own)) < 0.03, "FL3 own %.3f", own);
    CHECK (std::fabs (cross - std::fabs (t.fl3Cross)) < 0.03, "FL3 cross %.3f", cross);
    // FLANGER II: no cross talk
    render (Mode::Flanger2, true, false, 1, 1, oa, ob);
    CHECK (rms (ob, 12000) < 1e-6, "FL2 leakage %g", rms (ob, 12000));
    // FLANGER I: both outputs identical, B input summed in
    render (Mode::Flanger1, false, true, 1, 1, oa, ob);
    double diff = 0; for (size_t i = 0; i < oa.size(); ++i) diff = std::max (diff, std::fabs (oa[i] - ob[i]));
    CHECK (diff < 1e-9 && rms (oa, 12000) > 0.05, "FL1 outputs differ by %g, level %g", diff, rms (oa, 12000));
    // OFF: dry through with latency, unity
    render (Mode::Off, true, true, 1, 1, oa, ob);
    CHECK (std::fabs (sineLevel (oa, 1000, 48000, 12000) / 0.1 - 1.0) < 1e-3, "OFF gain %g", sineLevel (oa, 1000, 48000, 12000) / 0.1);
    // PHASE invert: wet-only output flips sign
    {
        Tunables tt = t; tt.flangerDry = 0; Flanger f; f.prepare (48000, tt);
        Params p; p.mode = Mode::Flanger2; p.depth = 0; p.manualMs = 1; p.noise = false; f.setParams (p, true);
        const int n = 24000; std::vector<float> a (n), ya (n), yb (n), ya2 (n), yb2 (n);
        for (int i = 0; i < n; ++i) a[(size_t) i] = (float) (0.1 * std::sin (2 * kPi * 500 * i / 48000.0));
        f.process (a.data(), a.data(), ya.data(), yb.data(), n);
        p.phaseAInvert = true; Flanger f2; f2.prepare (48000, tt); f2.setParams (p, true); f2.process (a.data(), a.data(), ya2.data(), yb2.data(), n);
        double corr = 0; for (int i = 12000; i < n; ++i) corr += ya[(size_t) i] * ya2[(size_t) i];
        CHECK (corr < 0, "phase invert correlation %g", corr);
    }
}

// ---- feedback ceiling: no self-oscillation at 100 % across the MANUAL range ----------------------
static void testFeedback()
{
    std::printf ("[feedback]\n");
    Tunables t; t.clockFeedthrough = 0;
    for (double ms : { 0.5, 2.0, 15.0 })
    {
        Flanger f; f.prepare (48000, t); Params p; p.mode = Mode::Flanger2; p.feedback = 1.0; p.depth = 0; p.manualMs = ms; f.setParams (p, true);
        const int n = 48000 * 3; std::vector<float> a (n, 0.0f), ya (n), yb (n);
        for (int i = 0; i < 4800; ++i) a[(size_t) i] = (float) (0.3 * std::sin (2 * kPi * 1000 * i / 48000.0));
        f.process (a.data(), a.data(), ya.data(), yb.data(), n);
        double tail = 0; for (int i = n - 4800; i < n; ++i) tail = std::max (tail, std::fabs ((double) ya[(size_t) i]));
        double peak = 0; for (int i = 0; i < n; ++i) peak = std::max (peak, std::fabs ((double) ya[(size_t) i]));
        CHECK (tail < 1e-3 && peak < 1.5, "manual %g ms: tail %.2e peak %.2f", ms, tail, peak);
    }
}

static void testWav()
{
    std::printf ("[wav]\n");
    WavData w; w.sampleRate = 48000; w.channels = 2; w.ch.assign (2, std::vector<float> (100));
    for (int i = 0; i < 100; ++i) { w.ch[0][(size_t) i] = (float) i / 100; w.ch[1][(size_t) i] = -(float) i / 100; }
    const std::string path = "/tmp/sbf_test.wav";
    CHECK (writeWavFloat (path, w), "write");
    WavData r; std::string err; CHECK (readWav (path, r, &err), "read: %s", err.c_str());
    CHECK (r.frames() == 100 && r.channels == 2 && std::fabs (r.ch[1][50] + 0.5f) < 1e-6, "roundtrip");
}

static void testSmoothing()
{
    std::printf ("[smoothing / latency]\n");
    Tunables t; t.bbdNoiseRms = 0; Flanger f; f.prepare (48000, t);
    CHECK (f.latencySamples() == Flanger::kLatency, "latency %d", f.latencySamples());
    Params p; p.mode = Mode::Flanger2; p.depth = 0; p.manualMs = 1; f.setParams (p, true);
    // impulse: dry arrives after exactly `latency` samples
    std::vector<float> a (2000, 0.0f), ya (2000), yb (2000); a[100] = 1.0f;
    Tunables tt = t; tt.flangerWet = 0; Flanger g; g.prepare (48000, tt); g.setParams (p, true); g.process (a.data(), a.data(), ya.data(), yb.data(), 2000);
    int at = 0; for (int i = 0; i < 2000; ++i) if (std::fabs (ya[(size_t) i]) > 0.5f) at = i;
    CHECK (at == 100 + Flanger::kLatency, "dry impulse at %d", at);
}

int main()
{
    testModal(); testBbd(); testCompander(); testLfo(); testHalfband(); testMatrix(); testFeedback(); testWav(); testSmoothing();
    std::printf ("%d passed, %d failed\n", passes, fails);
    return fails ? 1 : 0;
}
