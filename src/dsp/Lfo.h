// Bipolar triangle LFO (LF555-based integrator in the hardware), 0.02-5 Hz, with +/- peak LED flags.
#pragma once
#include <cmath>

namespace sbf {

struct Lfo
{
    double phase = 0.0;   // 0..1, value -1 at 0, +1 at 0.5
    double inc = 0.0;
    double fs = 48000.0;
    double value = -1.0;
    double ledThreshold = 0.9;
    bool ledPos = false, ledNeg = true;

    void prepare (double sampleRate) { fs = sampleRate; }
    void setRate (double hz) { inc = hz / fs; }
    void reset (double ph = 0.0) { phase = ph; }

    static inline double triangle (double ph) { return ph < 0.5 ? 4.0 * ph - 1.0 : 3.0 - 4.0 * ph; }

    inline double tick()
    {
        value = triangle (phase);
        ledPos = value >  ledThreshold;
        ledNeg = value < -ledThreshold;
        phase += inc;
        if (phase >= 1.0) phase -= std::floor (phase);
        return value;
    }
};

} // namespace sbf
