#pragma once
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include "DspTypes.h"

namespace qe
{
// Analoger Chorus im Stil der klassischen Eimerketten-Choere (BBD) aus Synthesizern der 80er:
// ein Dreieck-LFO, rechts gegenlaeufig (breites Stereo), dunkles Nass-Signal (Anti-Aliasing- und Rekonstruktions-
// Tiefpass der Eimerkette), leichte Saettigung und auf Wunsch ein Hauch Rauschen.
// Modus I: langsam und breit, II: etwas schneller, I + II: schnelles, flaches Flirren.
// Mix 0 = trocken, 0,5 = trocken und nass gleich laut (klassisch), 1 = nur nass. Linear gemischt: in der Feedback-
// Schleife verstaerkt er nie (hoechstens 0 dB).
class BbdChorus
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        len = (int) std::ceil (0.012 * sr) + 4;
        for (auto& b : buf) b.assign ((size_t) len, 0.0f);
        lpHz = 9000.0f;
        lpA = 1.0f - std::exp (-6.2831853f * lpHz / (float) sr);
        mixSm.reset (sampleRate, 0.05);
        initialised = false;
        reset();
    }

    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
        pos = 0;
        phase = 0.0;
        for (auto& s : lp) s = 0.0f;
        noiseState = 0x2545F491u;   // Mix-Glaettung bleibt (sonst wuerde das Einblenden nach einem Reset springen)
    }

    // mix: Ziel (0 = aus, wird weich ausgeblendet); noise: Rauschen der Eimerkette (nur einmal im Signalweg sinnvoll)
    bool active() const { return mixSm.isSmoothing() || mixSm.getCurrentValue() > 0.0f; }
    // rate/depth: Faktoren auf Geschwindigkeit und Tiefe des Modus; toneHz: Tiefpass der Eimerkette
    void setTarget (int modeIn, float mix, float rate, float depthF, float toneHz)
    {
        mode = std::min (std::max (modeIn, 0), 2);
        rateF = std::min (std::max (rate, 0.25f), 4.0f);
        depthTarget = std::min (std::max (depthF, 0.0f), 2.0f);
        if (toneHz != lpHz) { lpHz = toneHz; lpA = 1.0f - std::exp (-6.2831853f * std::min (std::max (toneHz, 2000.0f), 18000.0f) / (float) sr); }
        if (! initialised) { mixSm.setCurrentAndTargetValue (mix); depthSm = depthTarget; initialised = true; }
        mixSm.setTargetValue (mix);
    }

    void process (float& l, float& r, bool noise)
    {
        static constexpr double rates[3] = { 0.5, 0.83, 8.0 };     // Hz
        static constexpr float centre[3] = { 3.5f, 3.5f, 3.3f };    // ms
        static constexpr float depth[3] = { 1.8f, 1.8f, 0.3f };     // ms
        phase += rates[mode] * (double) rateF / sr;
        if (phase >= 1.0) phase -= 1.0;
        const float tri = (float) (phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase);   // Dreieck -1..1
        const float m = mixSm.getNextValue();
        depthSm += (depthTarget - depthSm) * 0.002f;   // Tiefe weich nachfuehren (Spruenge der Lesestelle knacken)

        float in[2] = { l, r };
        float wet[2];
        for (int c = 0; c < 2; ++c)
        {
            auto& b = buf[(size_t) c];
            b[(size_t) pos] = in[c];
            const float dMs = centre[mode] + (c == 0 ? tri : -tri) * depth[mode] * depthSm;
            float d = dMs * 0.001f * (float) sr;
            d = std::min (std::max (d, 1.0f), (float) (len - 3));
            const int di = (int) d;
            const float fr = d - (float) di;
            int i0 = pos - di; if (i0 < 0) i0 += len;
            int i1 = i0 - 1;   if (i1 < 0) i1 += len;
            float y = b[(size_t) i0] + (b[(size_t) i1] - b[(size_t) i0]) * fr;
            // Eimerkette: zwei Tiefpaesse 1. Ordnung (dunkel), weiche Saettigung, leises Rauschen
            lp[c * 2] += lpA * (y - lp[c * 2]);
            lp[c * 2 + 1] += lpA * (lp[c * 2] - lp[c * 2 + 1]);
            y = lp[c * 2 + 1];
            y = std::tanh (1.4f * y) / 1.4f;
            if (noise) y += 1.2e-4f * nextNoise();
            wet[c] = y;
        }
        pos = pos + 1 == len ? 0 : pos + 1;
        l = in[0] + (wet[0] - in[0]) * m;
        r = in[1] + (wet[1] - in[1]) * m;
    }

private:
    float nextNoise()   // reproduzierbar (xorshift), -1..1
    {
        noiseState ^= noiseState << 13; noiseState ^= noiseState >> 17; noiseState ^= noiseState << 5;
        return (float) (noiseState & 0xffffff) / 8388608.0f - 1.0f;
    }

    double sr = 48000.0, phase = 0.0;
    int len = 1, pos = 0, mode = 0;
    std::array<std::vector<float>, 2> buf;
    float lp[4] = { 0, 0, 0, 0 };
    float lpA = 1.0f, lpHz = 9000.0f;
    float rateF = 1.0f, depthTarget = 1.0f, depthSm = 1.0f;
    uint32_t noiseState = 0x2545F491u;
    juce::SmoothedValue<float> mixSm;
    bool initialised = false;
};
} // namespace qe
