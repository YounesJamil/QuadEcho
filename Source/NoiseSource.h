#pragma once
#include <cstdint>
#include <juce_dsp/juce_dsp.h>
#include "DspTypes.h"

namespace qe
{
// Noise-Oszillator (Bandrauschen und Knistern) mit eigenem Filter und Modulationssystem.
// Farbe: 0 = braun (dunkel, Koerper), 0.5 = rosa, 1 = weiss. Crackle: seltene, kurze Knackser.
// Stereo (Breite = Anteil unabhaengiger Kanaele). Hinten wird die Quelle im 3D-Raum platziert.
class NoiseSource
{
public:
    void prepare (double sampleRate);
    void reset();
    bool isActive() const { return levelSm.isSmoothing() || levelSm.getCurrentValue() > 0.0f; }
    void process (float* outL, float* outR, int n, const NoiseSettings& s, double bpm,
                  const float* inL, const float* inR);

private:
    struct Color { float p0 = 0, p1 = 0, p2 = 0, br = 0; };
    float colored (Color& c, float w, float col) const;

    double sr = 44100.0;
    uint32_t seed[4] = { 0x12345678u, 0x9e3779b9u, 0xdeadbeefu, 0x1b873593u };
    Color col[3];
    float crk[2] = { 0.0f, 0.0f };
    float pinkNorm = 1.0f, brownNorm = 1.0f, crkDecay = 0.9f;
    juce::dsp::StateVariableTPTFilter<float> hp, lp;      // 2. Ordnung mit Resonanz ...
    juce::dsp::FirstOrderTPTFilter<float> hp1, lp1;        // ... plus 1. Ordnung = 18 dB/Oktave
    juce::SmoothedValue<float> levelSm;
    double phase = 0.0;   // LFO-Position in Zyklen, fortlaufend
    float qSm[2] = { -1.0f, -1.0f };   // geglaettete Guete HP, LP (-1 = noch nicht gesetzt)
    float boostSm[2] = { 0.0f, 0.0f };  // geglaetteter Boost HP, LP (dB)
    PeakEq peakHp, peakLp;             // Glocke an der Grenzfrequenz
    float fModS = 0.0f, env = 0.0f, envMapped = 0.0f, envAtk = 0.01f, envRel = 0.001f;
    bool initialised = false;
    int framePos = 0;   // Position im 16er-Teilblock (Steuerraster), ueber Aufrufe hinweg
    float frameTrem = 1.0f;
    bool frameHpOn = false, frameLpOn = false, frameHpPeak = false, frameLpPeak = false;
};
} // namespace qe
