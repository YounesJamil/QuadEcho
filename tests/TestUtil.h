// Gemeinsame Hilfen fuer DSP-Tests und das Referenz-Harness (Signale, Render-Schleifen, WAV-Ausgabe)
#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>
#include <iostream>
#include <vector>
#include "../Source/QuadEngine.h"

namespace qetest
{
using namespace qe;

inline int gFailures = 0, gChecks = 0;

inline void check (bool ok, const std::string& msg)
{
    ++gChecks;
    if (! ok) { ++gFailures; std::cout << "  FAIL: " << msg << std::endl; }
}

struct Sig { std::vector<float> l, r; };

inline Sig makeSig (size_t n) { return { std::vector<float> (n, 0.0f), std::vector<float> (n, 0.0f) }; }

inline Sig impulse (size_t n, size_t at = 0)
{
    auto s = makeSig (n);
    s.l[at] = s.r[at] = 1.0f;
    return s;
}

inline Sig sine (size_t n, double sr, double hz, float amp)
{
    auto s = makeSig (n);
    for (size_t i = 0; i < n; ++i)
        s.l[i] = s.r[i] = amp * (float) std::sin (6.283185307179586 * hz * (double) i / sr);
    return s;
}

inline Sig noise (size_t n, float amp, int seed = 1)
{
    juce::Random rnd (seed);
    auto s = makeSig (n);
    for (size_t i = 0; i < n; ++i) { s.l[i] = amp * (rnd.nextFloat() * 2 - 1); s.r[i] = amp * (rnd.nextFloat() * 2 - 1); }
    return s;
}

// Kurzer "Pluck": abklingende Mischung aus Grundton und Obertoenen, danach Stille
inline Sig pluck (size_t n, double sr)
{
    auto s = makeSig (n);
    const size_t len = std::min (n, (size_t) (0.4 * sr));
    for (size_t i = 0; i < len; ++i)
    {
        const double t = (double) i / sr;
        const double env = std::exp (-9.0 * t);
        const double v = std::sin (6.2831853 * 220.0 * t) + 0.5 * std::sin (6.2831853 * 440.0 * t)
                       + 0.25 * std::sin (6.2831853 * 880.0 * t);
        s.l[i] = s.r[i] = (float) (0.35 * env * v);
    }
    return s;
}

inline bool finite (const Sig& s, float limit = 100.0f)
{
    for (size_t i = 0; i < s.l.size(); ++i)
        if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i]) || std::abs (s.l[i]) > limit || std::abs (s.r[i]) > limit)
            return false;
    return true;
}

inline float peakAbs (const std::vector<float>& v, size_t from = 0, size_t to = 0)
{
    if (to == 0 || to > v.size()) to = v.size();
    float m = 0;
    for (size_t i = from; i < to; ++i) m = std::max (m, std::abs (v[i]));
    return m;
}

inline float rms (const std::vector<float>& v, size_t from, size_t to)
{
    to = std::min (to, v.size());
    double e = 0;
    for (size_t i = from; i < to; ++i) e += (double) v[i] * v[i];
    return (float) std::sqrt (e / (double) std::max<size_t> (1, to - from));
}

inline float maxStep (const Sig& s, size_t from, size_t to)
{
    float m = 0;
    for (size_t i = std::max<size_t> (from, 1); i < std::min (to, s.l.size()); ++i)
        m = std::max ({ m, std::abs (s.l[i] - s.l[i - 1]), std::abs (s.r[i] - s.r[i - 1]) });
    return m;
}

inline size_t argMaxAbs (const std::vector<float>& v, size_t from = 0)
{
    size_t k = from; float m = -1;
    for (size_t i = from; i < v.size(); ++i) if (std::abs (v[i]) > m) { m = std::abs (v[i]); k = i; }
    return k;
}

inline float toDb (float x) { return 20.0f * std::log10 (std::max (x, 1e-9f)); }

using Mutator = std::function<void (size_t, EngineSettings&)>;

inline Sig runEngine (QuadEngine& e, EngineSettings s, const Sig& in, int block, double bpm = 120.0, const Mutator& mut = {})
{
    Sig out = in;
    for (size_t pos = 0; pos < in.l.size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, in.l.size() - pos);
        if (mut) mut (pos, s);
        e.process (out.l.data() + pos, out.r.data() + pos, n, s, bpm);
    }
    return out;
}

// Liefert die Echos der Einheit (ohne das durchlaufende Signal); full = true: den ganzen Ausgang
inline Sig runUnit (DelayUnit& u, const EchoSettings& s, const Sig& in, int block, double bpm = 120.0, bool full = false)
{
    Sig out = in, wet = in;
    for (size_t pos = 0; pos < in.l.size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, in.l.size() - pos);
        u.process (in.l.data() + pos, in.r.data() + pos, out.l.data() + pos, out.r.data() + pos, n, s, bpm,
                   wet.l.data() + pos, wet.r.data() + pos);
    }
    return full ? out : wet;
}

// Echo-Pegel 100 %, ohne Feedback
inline EchoSettings wetOnly()
{
    EchoSettings s;
    s.mix = 1.0f; s.feedback = 0.0f; s.mode = Stereo; s.transition = Fade;
    return s;
}

// Nur Einheit A, Parallel-Algorithmus, Standard-Mix (global 50 %, Echo-Pegel 100 %)
inline EngineSettings singleUnitSettings (int unit)
{
    EngineSettings s;
    s.algorithm = 9;
    for (int u = 0; u < kNumUnits; ++u) s.echo[(size_t) u].on = (u == unit);
    return s;
}

inline void writeWav (const juce::File& f, const Sig& s, double sr)
{
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream> (f);
    juce::WavAudioFormat fmt;
    auto w = fmt.createWriterFor (os, juce::AudioFormatWriterOptions {}.withSampleRate (sr).withNumChannels (2).withBitsPerSample (24));
    if (w == nullptr) { check (false, "WAV-Writer: " + f.getFullPathName().toStdString()); return; }
    const int n = (int) s.l.size();
    juce::AudioBuffer<float> buf (2, n);
    buf.copyFrom (0, 0, s.l.data(), n);
    buf.copyFrom (1, 0, s.r.data(), n);
    // Clipping im Render vermeiden
    for (int c = 0; c < 2; ++c) juce::FloatVectorOperations::clip (buf.getWritePointer (c), buf.getReadPointer (c), -1.0f, 1.0f, n);
    w->writeFromAudioSampleBuffer (buf, 0, n);
}

} // namespace qetest
