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
    // Songposition (ppq) am Anfang des naechsten process(); < 0 = unbekannt/steht. Mit LFO-Sync folgt die Phase ihr.
    void setSongPosition (double ppq) { songPpq = ppq; }
    void process (float* outL, float* outR, int n, const NoiseSettings& s, double bpm,
                  const float* inL, const float* inR);

    // Fuer die UI (lockfrei): LFO und Auto-Pan (Position in Zyklen, Rate in Hz), Huellkurve 0..1
    double getLfoPos() const { return posPub.load (std::memory_order_relaxed); }
    float getLfoRateHz() const { return ratePub.load (std::memory_order_relaxed); }
    float getEnvValue() const { return envPub.load (std::memory_order_relaxed); }
    double getPanPos() const { return panPosPub.load (std::memory_order_relaxed); }
    float getPanRateHz() const { return panRatePub.load (std::memory_order_relaxed); }

private:
    struct Color { float p0 = 0, p1 = 0, p2 = 0, br = 0; };
    float colored (Color& c, float w, float col) const;

    double sr = 44100.0;
    static constexpr uint32_t kSeeds[4] = { 0x12345678u, 0x9e3779b9u, 0xdeadbeefu, 0x1b873593u };
    uint32_t seed[4] = { kSeeds[0], kSeeds[1], kSeeds[2], kSeeds[3] };
    Color col[3];
    float crk[2] = { 0.0f, 0.0f };
    float pinkNorm = 1.0f, brownNorm = 1.0f, crkDecay = 0.9f;
    juce::dsp::StateVariableTPTFilter<float> hp, lp;      // 2. Ordnung mit Resonanz ...
    juce::dsp::FirstOrderTPTFilter<float> hp1, lp1;        // ... plus 1. Ordnung = 18 dB/Oktave
    juce::SmoothedValue<float> levelSm;
    double phase = 0.0;   // LFO-Position in Zyklen, fortlaufend
    double panPhase = 0.0;   // Auto-Pan-Position in Zyklen, fortlaufend
    juce::SmoothedValue<float> panAmtSm;
    std::atomic<double> posPub { 0.0 }, panPosPub { 0.0 };
    std::atomic<float> ratePub { 0.5f }, envPub { 0.0f }, panRatePub { 0.5f };
    float qSm[2] = { -1.0f, -1.0f };   // geglaettete Guete HP, LP (-1 = noch nicht gesetzt)
    float boostSm[2] = { 0.0f, 0.0f };  // geglaetteter Boost HP, LP (dB)
    PeakEq peakHp, peakLp;             // Glocke an der Grenzfrequenz
    PeakEq peakBell;                   // freies Bell-Band
    float bellSm = 0.0f;
    bool frameBell = false;
    float fModS = 0.0f, env = 0.0f, envMapped = 0.0f, envAtk = 0.01f, envRel = 0.001f;
    bool initialised = false;
    int framePos = 0;   // Position im 16er-Teilblock (Steuerraster), ueber Aufrufe hinweg
    double songPpq = -1.0;
    float frameTrem = 1.0f;
    bool frameHpOn = false, frameLpOn = false, frameHpPeak = false, frameLpPeak = false;
};
} // namespace qe
