// Minimal WAV reader/writer (PCM 16/24/32 and IEEE float 32), JUCE-free, for the render tool and tests.
#pragma once
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace sbf {

struct WavData
{
    int sampleRate = 48000;
    int channels = 0;
    std::vector<std::vector<float>> ch;   // ch[c][frame]
    size_t frames() const { return ch.empty() ? 0 : ch[0].size(); }
};

namespace detail {
inline uint32_t rd32 (const uint8_t* p) { return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24); }
inline uint16_t rd16 (const uint8_t* p) { return (uint16_t) (p[0] | (p[1] << 8)); }
inline void wr32 (std::vector<uint8_t>& v, uint32_t x) { for (int i = 0; i < 4; ++i) v.push_back ((uint8_t) (x >> (8 * i))); }
inline void wr16 (std::vector<uint8_t>& v, uint16_t x) { v.push_back ((uint8_t) x); v.push_back ((uint8_t) (x >> 8)); }
}

inline bool readWav (const std::string& path, WavData& out, std::string* err = nullptr)
{
    FILE* f = std::fopen (path.c_str(), "rb");
    if (! f) { if (err) *err = "cannot open " + path; return false; }
    std::vector<uint8_t> d;
    { uint8_t buf[65536]; size_t n; while ((n = std::fread (buf, 1, sizeof buf, f)) > 0) d.insert (d.end(), buf, buf + n); }
    std::fclose (f);
    if (d.size() < 12 || std::memcmp (d.data(), "RIFF", 4) != 0 || std::memcmp (d.data() + 8, "WAVE", 4) != 0) { if (err) *err = "not a RIFF/WAVE file"; return false; }
    size_t pos = 12; int fmt = 0, bits = 0, chans = 0, rate = 0; const uint8_t* data = nullptr; size_t dataLen = 0;
    while (pos + 8 <= d.size())
    {
        const uint32_t len = detail::rd32 (d.data() + pos + 4);
        const uint8_t* body = d.data() + pos + 8;
        if (std::memcmp (d.data() + pos, "fmt ", 4) == 0 && len >= 16)
        {
            fmt = detail::rd16 (body); chans = detail::rd16 (body + 2); rate = (int) detail::rd32 (body + 4); bits = detail::rd16 (body + 14);
            if (fmt == 0xFFFE && len >= 26) fmt = detail::rd16 (body + 24);   // WAVE_FORMAT_EXTENSIBLE: sub-format GUID's first two bytes
        }
        else if (std::memcmp (d.data() + pos, "data", 4) == 0) { data = body; dataLen = std::min<size_t> (len, d.size() - (pos + 8)); }
        pos += 8 + len + (len & 1);
    }
    if (! data || chans <= 0 || bits <= 0) { if (err) *err = "missing fmt/data chunk"; return false; }
    const int bytes = bits / 8;
    const size_t frames = dataLen / (size_t) (bytes * chans);
    out.sampleRate = rate; out.channels = chans; out.ch.assign ((size_t) chans, std::vector<float> (frames));
    for (size_t i = 0; i < frames; ++i)
        for (int c = 0; c < chans; ++c)
        {
            const uint8_t* p = data + (i * (size_t) chans + (size_t) c) * (size_t) bytes;
            float v = 0.0f;
            if (fmt == 3 && bits == 32) { float x; std::memcpy (&x, p, 4); v = x; }
            else if (fmt == 3 && bits == 64) { double x; std::memcpy (&x, p, 8); v = (float) x; }
            else if (bits == 16) v = (float) (int16_t) detail::rd16 (p) / 32768.0f;
            else if (bits == 24) { int32_t x = (int32_t) ((p[0] << 8) | (p[1] << 16) | (p[2] << 24)) >> 8; v = (float) x / 8388608.0f; }
            else if (bits == 32) { int32_t x = (int32_t) detail::rd32 (p); v = (float) (x / 2147483648.0); }
            else if (bits == 8) v = ((float) p[0] - 128.0f) / 128.0f;
            out.ch[(size_t) c][i] = v;
        }
    return true;
}

inline bool writeWavFloat (const std::string& path, const WavData& w, std::string* err = nullptr)
{
    const uint32_t frames = (uint32_t) w.frames(), chans = (uint32_t) w.channels;
    std::vector<uint8_t> v; v.reserve (44 + frames * chans * 4);
    auto put = [&] (const char* s) { v.insert (v.end(), s, s + 4); };
    put ("RIFF"); detail::wr32 (v, 36 + frames * chans * 4); put ("WAVE");
    put ("fmt "); detail::wr32 (v, 16); detail::wr16 (v, 3); detail::wr16 (v, (uint16_t) chans); detail::wr32 (v, (uint32_t) w.sampleRate);
    detail::wr32 (v, (uint32_t) w.sampleRate * chans * 4); detail::wr16 (v, (uint16_t) (chans * 4)); detail::wr16 (v, 32);
    put ("data"); detail::wr32 (v, frames * chans * 4);
    for (uint32_t i = 0; i < frames; ++i)
        for (uint32_t c = 0; c < chans; ++c) { float x = w.ch[c][i]; uint32_t u; std::memcpy (&u, &x, 4); detail::wr32 (v, u); }
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) { if (err) *err = "cannot write " + path; return false; }
    std::fwrite (v.data(), 1, v.size(), f); std::fclose (f);
    return true;
}

} // namespace sbf
