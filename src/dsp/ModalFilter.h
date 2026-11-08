// Continuous-time Butterworth low-pass evaluated exactly at irregular instants.
//
// The BBD's anti-alias and reconstruction filters are analog; the BBD samples its input at the clock
// instants (not on the host grid) and holds its output between them. Following Raffel & Smith,
// "Practical Modeling of Bucket-Brigade Device Circuits" (DAFx 2010), the filter is decomposed into
// first-order modes (partial fractions) whose states can be advanced analytically over any interval
// for a piecewise-linear (input side) or piecewise-constant (output side) drive. No host-rate
// sampling of the BBD clock takes place, so clock images alias only where the analog filters let them.
#pragma once
#include <complex>
#include <vector>
#include <cmath>
#include <algorithm>
#include "Util.h"

namespace sbf {

struct ModalLpf
{
    using C = std::complex<double>;
    static constexpr int kMaxOrder = 4;

    int n = 0;
    C p[kMaxOrder], r[kMaxOrder], invp[kMaxOrder], s[kMaxOrder];
    double fc = 0.0;

    // exp(p dt) table over dt in [0, tableMax] for cheap step construction
    struct ExpTable { std::vector<C> v; double max = 0, scale = 0; };
    ExpTable tab[kMaxOrder];

    struct Step { C e[kMaxOrder], a[kMaxOrder], b[kMaxOrder]; double dt = 0; };

    void design (int order, double cutoffHz)
    {
        n = std::clamp (order, 1, kMaxOrder);
        fc = cutoffHz;
        const double wc = 2.0 * kPi * cutoffHz;
        for (int k = 0; k < n; ++k)
        {
            const double th = kPi * (2.0 * (k + 1) + n - 1) / (2.0 * n);
            p[k] = wc * C (std::cos (th), std::sin (th));
            invp[k] = 1.0 / p[k];
        }
        for (int k = 0; k < n; ++k)
        {
            C den (1.0, 0.0);
            for (int j = 0; j < n; ++j) if (j != k) den *= (p[k] - p[j]);
            r[k] = std::pow (wc, n) / den;
        }
        for (int k = 0; k < n; ++k)
            if (tab[k].max > 0) buildTable (k, tab[k].max);
    }

    // Prepare the exp table for intervals up to maxDt (seconds); the caller promises dt <= maxDt.
    void prepareTable (double maxDt, int entries = 1024)
    {
        for (int k = 0; k < n; ++k) { tab[k].max = maxDt; tab[k].v.resize ((size_t) entries + 2); buildTable (k, maxDt); }
    }
    void buildTable (int k, double maxDt)
    {
        auto& t = tab[k];
        const int entries = (int) t.v.size() - 2;
        t.scale = entries / maxDt;
        for (int i = 0; i < entries + 2; ++i) t.v[(size_t) i] = std::exp (p[k] * (i / t.scale));
    }

    inline C expP (int k, double dt) const
    {
        const auto& t = tab[k];
        if (t.v.empty() || dt > t.max || dt < 0) return std::exp (p[k] * dt);
        const double x = dt * t.scale;
        const int i = (int) x;
        const double f = x - i;
        return t.v[(size_t) i] + f * (t.v[(size_t) i + 1] - t.v[(size_t) i]);
    }

    void makeStep (Step& st, double dt) const
    {
        st.dt = dt;
        for (int k = 0; k < n; ++k)
        {
            st.e[k] = expP (k, dt);
            st.a[k] = (st.e[k] - 1.0) * invp[k];
            st.b[k] = (st.a[k] - dt) * invp[k];
        }
    }

    // input ramps linearly from u0 (at the start) to u1 (at the end of the interval)
    inline void advanceLinear (const Step& st, double u0, double u1)
    {
        const double m = st.dt > 0 ? (u1 - u0) / st.dt : 0.0;
        for (int k = 0; k < n; ++k) s[k] = st.e[k] * s[k] + r[k] * (u0 * st.a[k] + m * st.b[k]);
    }
    // input is constant over the interval
    inline void advanceHold (const Step& st, double u)
    {
        for (int k = 0; k < n; ++k) s[k] = st.e[k] * s[k] + (r[k] * u) * st.a[k];
    }
    inline double out() const
    {
        double y = 0.0;
        for (int k = 0; k < n; ++k) y += s[k].real();
        return y;
    }
    void reset() { for (auto& x : s) x = 0.0; }

    // analytic magnitude (Butterworth) and low-frequency group delay (seconds)
    double magnitude (double hz) const { return 1.0 / std::sqrt (1.0 + std::pow (hz / fc, 2.0 * n)); }
    double groupDelayDc() const { double g = 0; for (int k = 0; k < n; ++k) g += (-invp[k]).real(); return g; }
};

} // namespace sbf
