// NE570 compander model: 2:1 compressor (feedback topology) before the BBD, 1:2 expander after it.
//
// The NE570's gain cell is driven by a full-wave rectifier averaged by an external capacitor
// (tau = 10 kOhm x C_rect). Compressor: Vout = Vin * Vref / avg|Vout|  (steady state: Vout^2 = Vin*Vref,
// i.e. 2:1 in dB). Expander: Vout = Vin * avg|Vin| / Vref. The averaged rectifier output is scaled so
// that a sine of peak Vref passes at unity; the maximum gain is limited as in the real part.
#pragma once
#include <cmath>
#include <algorithm>

namespace sbf {

struct Ne570
{
    double env = 0.0, a = 1.0, vref = 0.158, floorEnv = 0.00158;
    static constexpr double kRectScale = 1.5707963267948966; // pi/2: mean|sin| -> peak

    void prepare (double fs, double tauMs, double unityPeak, double maxGainDb)
    {
        a = tauMs > 0 ? 1.0 - std::exp (-1.0 / (tauMs * 1e-3 * fs)) : 1.0;
        vref = unityPeak;
        floorEnv = vref * std::pow (10.0, -maxGainDb / 20.0);
        env = 0.0;
    }
    void reset() { env = 0.0; }

    inline double compress (double x)
    {
        const double y = x * (vref / std::max (env, floorEnv));
        env += a * (std::fabs (y) * kRectScale - env);
        return y;
    }
    inline double expand (double x)
    {
        env += a * (std::fabs (x) * kRectScale - env);
        return x * (std::max (env, floorEnv) / vref);
    }
    double gainDb() const { return 20.0 * std::log10 (std::max (env, floorEnv) / vref); }
};

} // namespace sbf
