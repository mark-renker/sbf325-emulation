// Small JUCE-free helpers: soft clip, one-pole smoother, polyphase IIR half-band 2x, noise.
#pragma once
#include <cmath>
#include <cstdint>
#include <array>
#include <algorithm>

namespace sbf {

constexpr double kPi = 3.14159265358979323846;

inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g)  { return 20.0 * std::log10 (std::max (g, 1e-30)); }

// Symmetric soft limiter: y = x / (1 + |x/L|^p)^(1/p). Transparent well below L, asymptotes to +-L.
inline double softClip (double x, double L, double p)
{
    const double a = std::fabs (x / L);
    if (a < 1e-9) return x;
    const double a2 = a * a, a4 = a2 * a2;
    if (p == 4.0) return x / std::sqrt (std::sqrt (1.0 + a4));
    if (p == 8.0) return x / std::sqrt (std::sqrt (std::sqrt (1.0 + a4 * a4)));
    if (p == 16.0) { const double a8 = a4 * a4; return x / std::sqrt (std::sqrt (std::sqrt (std::sqrt (1.0 + a8 * a8)))); }
    return x / std::pow (1.0 + std::pow (a, p), 1.0 / p);
}

// One-pole exponential smoother (time constant in seconds).
struct Smoother
{
    double v = 0.0, target = 0.0, a = 1.0;
    void prepare (double fs, double tauSec) { a = tauSec > 0 ? 1.0 - std::exp (-1.0 / (tauSec * fs)) : 1.0; }
    void reset (double x) { v = target = x; }
    void setTarget (double x) { target = x; }
    inline double tick() { v += a * (target - v); return v; }
};

// First-order allpass section (c + z^-1) / (1 + c z^-1), the building block of polyphase IIR half-bands.
struct Allpass1
{
    double c = 0.0, x1 = 0.0, y1 = 0.0;
    inline double tick (double x)
    {
        const double y = c * (x - y1) + x1;
        x1 = x; y1 = y;
        return y;
    }
    void reset() { x1 = y1 = 0.0; }
};

// Polyphase IIR half-band (two allpass branches). Coefficients: 12-section set, passband flat to
// 0.45 fs, stopband < -100 dB above 0.55 fs (verified numerically), ~4 samples group delay at 2x rate.
struct HalfBandCoefs
{
    static constexpr int kN = 6;
    static constexpr std::array<double, kN> a { 0.036681502163648017, 0.2746317593794541, 0.56109896978791948,
                                               0.769741833862266,     0.8922608180038789, 0.962094548378084 };
    static constexpr std::array<double, kN> b { 0.13654762463195771, 0.42313861743656667, 0.6775400499741616,
                                               0.839889624849638,    0.9315419599631839, 0.9878163707328971 };
};

struct HalfBandUp2x   // host rate in -> two samples out
{
    std::array<Allpass1, HalfBandCoefs::kN> A, B;
    HalfBandUp2x() { for (int i = 0; i < HalfBandCoefs::kN; ++i) { A[(size_t) i].c = HalfBandCoefs::a[(size_t) i]; B[(size_t) i].c = HalfBandCoefs::b[(size_t) i]; } }
    inline void tick (double x, double& y0, double& y1)
    {
        double u = x, v = x;
        for (auto& s : A) u = s.tick (u);
        for (auto& s : B) v = s.tick (v);
        y0 = u; y1 = v;
    }
    void reset() { for (auto& s : A) s.reset(); for (auto& s : B) s.reset(); }
};

struct HalfBandDown2x // two samples in -> host rate out; y[n] = (H x)[2n]; the up/down pair delays by 3.97 host samples
{
    std::array<Allpass1, HalfBandCoefs::kN> A, B;
    double xPrev = 0.0;
    HalfBandDown2x() { for (int i = 0; i < HalfBandCoefs::kN; ++i) { A[(size_t) i].c = HalfBandCoefs::a[(size_t) i]; B[(size_t) i].c = HalfBandCoefs::b[(size_t) i]; } }
    inline double tick (double x0, double x1)
    {
        double u = x0, v = xPrev;
        xPrev = x1;
        for (auto& s : A) u = s.tick (u);
        for (auto& s : B) v = s.tick (v);
        return 0.5 * (u + v);
    }
    void reset() { for (auto& s : A) s.reset(); for (auto& s : B) s.reset(); xPrev = 0.0; }
};

// Integer delay line (dry-path latency compensation).
template <int N>
struct FixedDelay
{
    std::array<float, N + 1> buf {};
    int w = 0, len = N;
    void setLength (int n) { len = std::clamp (n, 0, N); }
    inline float tick (float x)
    {
        buf[(size_t) w] = x;
        int r = w - len; if (r < 0) r += N + 1;
        w = (w + 1) % (N + 1);
        return buf[(size_t) r];
    }
    void reset() { buf.fill (0.0f); w = 0; }
};

// xorshift-based Gaussian noise (Box-Muller), deterministic per seed for reproducible renders.
struct Noise
{
    uint64_t s = 0x9E3779B97F4A7C15ull;
    void seed (uint64_t x) { s = x ? x : 1; }
    inline double uniform() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (double) (s >> 11) * (1.0 / 9007199254740992.0); }
    inline double gaussian()
    {
        double u1 = uniform(), u2 = uniform();
        if (u1 < 1e-300) u1 = 1e-300;
        return std::sqrt (-2.0 * std::log (u1)) * std::cos (2.0 * kPi * u2);
    }
    // cheap near-Gaussian (sum of four uniforms, unit variance) for per-tick BBD noise
    inline double fastGaussian() { return (uniform() + uniform() + uniform() + uniform() - 2.0) * 1.7320508075688772; }
};

} // namespace sbf
