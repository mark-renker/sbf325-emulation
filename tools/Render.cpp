// sbf_render — standalone WAV->WAV renderer / test-signal generator for the measurement scripts.
//
//   sbf_render [options] [input.wav] output.wav
//   sbf_render --info [--set k=v ...]            print model facts as JSON (latency, delays, tunables)
//   sbf_render --cpu                              per-instance CPU at 48 kHz / 64-sample buffers
//
// Signal options (when no input.wav):  --gen sweep[:f0:f1]|sine:f|impulse|burst:f:on:off|silence|noise
//   --len s  --sr hz  --amp dBFS  --channels a|b|ab  --cv-sine hz  --cv-file wav  --lfo-wav path
// Parameters: --mode off|fl1|fl2|fl3|chorus --manual ms --rate hz --depth % --feedback %
//   --chb-mod norm|inv --phase-a norm|inv --phase-b norm|inv --in-trim dB --out-trim dB --noise on|off
//   --cv-param v  --wet-only  --dry-only  --block n  --set key=value
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>
#include "../src/dsp/Flanger.h"
#include "../src/dsp/Wav.h"

using namespace sbf;

static void usage() { std::fprintf (stderr, "usage: sbf_render [options] [in.wav] out.wav   (see tools/Render.cpp header)\n"); }

struct Gen { std::string kind = "sweep"; double f0 = 20, f1 = 20000, f = 1000, on = 0.5, off = 0.5; };

static std::vector<double> splitNums (const std::string& s)
{
    std::vector<double> v; size_t p = 0;
    while (p <= s.size()) { size_t q = s.find (':', p); if (q == std::string::npos) q = s.size(); if (q > p) v.push_back (std::atof (s.substr (p, q - p).c_str())); p = q + 1; }
    return v;
}

static void generate (const Gen& g, double sr, double len, double amp, std::vector<float>& out)
{
    const size_t n = (size_t) std::llround (len * sr);
    out.assign (n, 0.0f);
    Noise rng; rng.seed (42);
    if (g.kind == "sweep")
    {
        const double T = len, k = std::log (g.f1 / g.f0);
        for (size_t i = 0; i < n; ++i) { const double t = i / sr; out[i] = (float) (amp * std::sin (2.0 * kPi * g.f0 * T / k * (std::exp (t / T * k) - 1.0))); }
    }
    else if (g.kind == "sine") { for (size_t i = 0; i < n; ++i) out[i] = (float) (amp * std::sin (2.0 * kPi * g.f * i / sr)); }
    else if (g.kind == "impulse") { if (n > 0) out[std::min<size_t> (n - 1, (size_t) (0.1 * sr))] = (float) amp; }
    else if (g.kind == "burst")
    {
        const double per = g.on + g.off;
        for (size_t i = 0; i < n; ++i) { const double t = i / sr; const double ph = std::fmod (t, per); if (ph < g.on) out[i] = (float) (amp * std::sin (2.0 * kPi * g.f * t)); }
    }
    else if (g.kind == "noise") { for (size_t i = 0; i < n; ++i) out[i] = (float) (amp * 0.3 * rng.gaussian()); }
    // silence: zeros
}

int main (int argc, char** argv)
{
    Params P; Tunables T; Gen gen;
    std::string inPath, outPath, cvFile, lfoWav, chans = "ab";
    double len = 2.0, sr = 96000.0, ampDb = -12.0, cvSine = 0.0;
    int block = 64; bool info = false, cpu = false, haveGen = false;
    std::vector<std::string> sets;

    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        auto next = [&] () -> std::string { if (i + 1 >= argc) { usage(); std::exit (2); } return argv[++i]; };
        if (a == "--info") info = true;
        else if (a == "--cpu") cpu = true;
        else if (a == "--gen") { haveGen = true; auto v = next(); auto c = v.find (':'); gen.kind = v.substr (0, c); auto nums = c == std::string::npos ? std::vector<double>{} : splitNums (v.substr (c + 1));
            if (gen.kind == "sweep") { if (nums.size() > 0) gen.f0 = nums[0]; if (nums.size() > 1) gen.f1 = nums[1]; }
            else if (gen.kind == "sine") { if (! nums.empty()) gen.f = nums[0]; }
            else if (gen.kind == "burst") { if (nums.size() > 0) gen.f = nums[0]; if (nums.size() > 1) gen.on = nums[1]; if (nums.size() > 2) gen.off = nums[2]; } }
        else if (a == "--len") len = std::atof (next().c_str());
        else if (a == "--sr") sr = std::atof (next().c_str());
        else if (a == "--amp") ampDb = std::atof (next().c_str());
        else if (a == "--channels") chans = next();
        else if (a == "--cv-sine") cvSine = std::atof (next().c_str());
        else if (a == "--cv-file") cvFile = next();
        else if (a == "--lfo-wav") lfoWav = next();
        else if (a == "--mode") { auto m = next(); P.mode = m == "off" ? Mode::Off : m == "fl1" ? Mode::Flanger1 : m == "fl2" ? Mode::Flanger2 : m == "fl3" ? Mode::Flanger3 : Mode::Chorus; }
        else if (a == "--manual") P.manualMs = std::atof (next().c_str());
        else if (a == "--rate") P.rateHz = std::atof (next().c_str());
        else if (a == "--depth") P.depth = std::atof (next().c_str()) / 100.0;
        else if (a == "--feedback") P.feedback = std::atof (next().c_str()) / 100.0;
        else if (a == "--chb-mod") P.chBModInvert = next() == "inv";
        else if (a == "--phase-a") P.phaseAInvert = next() == "inv";
        else if (a == "--phase-b") P.phaseBInvert = next() == "inv";
        else if (a == "--in-trim") P.inTrimDb = std::atof (next().c_str());
        else if (a == "--out-trim") P.outTrimDb = std::atof (next().c_str());
        else if (a == "--noise") P.noise = next() != "off";
        else if (a == "--cv-param") { P.cvMode = CvMode::Parameter; P.cvValue = std::atof (next().c_str()); }
        else if (a == "--wet-only") sets.insert (sets.end(), { "flangerDry=0", "chorusDry=0" });
        else if (a == "--dry-only") sets.insert (sets.end(), { "flangerWet=0", "chorusWet=0" });
        else if (a == "--block") block = std::atoi (next().c_str());
        else if (a == "--set") sets.push_back (next());
        else if (a.rfind ("--", 0) == 0) { std::fprintf (stderr, "unknown option %s\n", a.c_str()); usage(); return 2; }
        else if (inPath.empty() && outPath.empty() && i + 1 < argc && argv[i + 1][0] != '-') inPath = a;
        else outPath = a;
    }
    for (auto& s : sets) if (! T.setFromString (s)) { std::fprintf (stderr, "unknown tunable: %s\n", s.c_str()); return 2; }
    if (! cvFile.empty() || cvSine > 0) P.cvMode = CvMode::Sidechain;

    WavData in;
    if (! inPath.empty())
    {
        std::string err; if (! readWav (inPath, in, &err)) { std::fprintf (stderr, "%s\n", err.c_str()); return 1; }
        sr = in.sampleRate;
        if (in.channels == 1) in.ch.push_back (in.ch[0]);
    }

    Flanger fl; fl.prepare (sr, T); fl.setParams (P, true);

    if (info)
    {
        std::printf ("{\n  \"sampleRate\": %g,\n  \"latencySamples\": %d,\n  \"analogGroupDelaySec\": %.9g,\n  \"bbdHalfStages\": %d,\n",
                     sr, fl.latencySamples(), fl.analogGroupDelaySec(), fl.lfoRef().fs > 0 ? (int) std::lround (T.bbdStages / 2) : 0);
        std::printf ("  \"clockMinHz\": %.6g,\n  \"clockMaxHz\": %.6g,\n  \"chorusClockMaxHz\": %.6g,\n", fl.fMin (Mode::Flanger2), fl.fMax (Mode::Flanger2), fl.fMax (Mode::Chorus));
        std::printf ("  \"expectedWetDelaySec\": %.9g,\n  \"mode\": %d,\n  \"manualMs\": %g,\n", fl.expectedWetDelaySec (P.manualMs, P.mode), (int) P.mode, P.manualMs);
        std::printf ("  \"tunables\": {\n");
        bool first = true;
        T.forEach ([&] (const char* k, double v, double, const char*) { std::printf ("%s    \"%s\": %.10g", first ? "" : ",\n", k, v); first = false; });
        std::printf ("\n  }\n}\n");
        return 0;
    }

    if (cpu)
    {
        const double rate = 48000.0; const int bs = 64; const int blocks = (int) (rate * 10.0 / bs);
        Flanger f2; f2.prepare (rate, T); P.mode = P.mode == Mode::Off ? Mode::Flanger2 : P.mode; f2.setParams (P, true);
        std::vector<float> a (bs), b (bs), oa (bs), ob (bs); Noise rng;
        for (int i = 0; i < bs; ++i) { a[(size_t) i] = (float) (0.1 * rng.gaussian()); b[(size_t) i] = (float) (0.1 * rng.gaussian()); }
        auto t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < blocks; ++k) f2.process (a.data(), b.data(), oa.data(), ob.data(), bs);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("{ \"sampleRate\": 48000, \"blockSize\": 64, \"mode\": %d, \"cpuPercent\": %.3f, \"audioSeconds\": 10 }\n", (int) P.mode, 100.0 * secs / 10.0);
        return 0;
    }

    if (outPath.empty()) { usage(); return 2; }

    std::vector<float> sigA, sigB;
    if (! inPath.empty()) { sigA = in.ch[0]; sigB = in.ch[1]; }
    else
    {
        if (! haveGen) gen.kind = "sweep";
        std::vector<float> s; generate (gen, sr, len, dbToGain (ampDb), s);
        sigA = chans.find ('a') != std::string::npos ? s : std::vector<float> (s.size(), 0.0f);
        sigB = chans.find ('b') != std::string::npos ? s : std::vector<float> (s.size(), 0.0f);
    }
    const size_t n = sigA.size();
    std::vector<float> cv;
    if (! cvFile.empty())
    {
        WavData c; std::string err; if (! readWav (cvFile, c, &err)) { std::fprintf (stderr, "%s\n", err.c_str()); return 1; }
        cv.assign (n, 0.0f); for (size_t i = 0; i < n && i < c.frames(); ++i) cv[i] = c.ch[0][i];
    }
    else if (cvSine > 0) { cv.resize (n); for (size_t i = 0; i < n; ++i) cv[i] = (float) std::sin (2.0 * kPi * cvSine * i / sr); }

    WavData out; out.sampleRate = (int) sr; out.channels = 2; out.ch.assign (2, std::vector<float> (n));
    WavData lfoOut; if (! lfoWav.empty()) { lfoOut.sampleRate = (int) sr; lfoOut.channels = 2; lfoOut.ch.assign (2, std::vector<float> (n)); }

    for (size_t pos = 0; pos < n; pos += (size_t) block)
    {
        const int m = (int) std::min<size_t> ((size_t) block, n - pos);
        fl.process (sigA.data() + pos, sigB.data() + pos, out.ch[0].data() + pos, out.ch[1].data() + pos, m, cv.empty() ? nullptr : cv.data() + pos);
        if (! lfoWav.empty())
        {
            // sample the LFO per block for the LED/rate check (block-exact enough at 64 samples)
            for (int i = 0; i < m; ++i) { lfoOut.ch[0][pos + (size_t) i] = fl.meters.lfo.load(); lfoOut.ch[1][pos + (size_t) i] = fl.meters.ledPos.load() ? 1.0f : fl.meters.ledNeg.load() ? -1.0f : 0.0f; }
        }
    }
    std::string err;
    if (! writeWavFloat (outPath, out, &err)) { std::fprintf (stderr, "%s\n", err.c_str()); return 1; }
    if (! lfoWav.empty() && ! writeWavFloat (lfoWav, lfoOut, &err)) { std::fprintf (stderr, "%s\n", err.c_str()); return 1; }
    return 0;
}
