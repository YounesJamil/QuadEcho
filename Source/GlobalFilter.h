#pragma once
#include <juce_dsp/juce_dsp.h>
#include "DspTypes.h"

namespace qe
{
// Globaler Filter (HP + LP) mit eigenem LFO. Mehrere Instanzen (eigene Zustaende) teilen sich
// Koeffizienten und Modulation: 0..3 = Echo-Anteil je Einheit, 4 = Noise, 5 = Original ("Include dry").
// Ein Filter auf jeder Einheit und anschliessende Summe ist gleichwertig zu einem Filter auf der
// Summe; so laesst sich das erste Echo einer Einheit auf Wunsch aussparen.
// Phase (phaseDeg) = Versatz der rechten Filterbewegung gegen die linke (Stereo-Bewegung), wie
// "Phase R" bei den Einheiten. Daher hat jeder Kanal eigene Filter mit eigener Grenzfrequenz.
class GlobalFilter
{
public:
    static constexpr int kInstances = 6;

    void prepare (double sampleRate);
    void reset();
    void resetInstance (int inst);
    // einmal pro Chunk (n <= 256): schreitet den LFO fuer alle 16-Sample-Bloecke weiter. Die Bloecke liegen an der
    // absoluten Sample-Position; ein angefangener Block aus dem letzten Chunk wird mit seinen Werten zu Ende gefuehrt.
    // ppq: Songposition am Chunk-Anfang (< 0 = unbekannt/steht); mit LFO-Sync folgt die Phase ihr
    // inL/inR: Eingang der Engine fuer die Huellkurve (envMix), n Samples
    void beginChunk (const GlobalFilterSettings& s, double bpm, int n, double ppq = -1.0,
                     const float* inL = nullptr, const float* inR = nullptr);
    bool active() const { return cur.hpOn || cur.lpOn || cur.bellOn || std::abs (bellS) > kBoostOffDb; }
    // verarbeitet den Chunk (n Samples, wie bei beginChunk) in-place
    void process (int inst, float* l, float* r, int n);

    // Fuer die UI (lockfrei): LFO-Position in Zyklen, Rate, Huellkurve 0..1
    double getLfoPos() const { return posPub.load (std::memory_order_relaxed); }
    float getLfoRateHz() const { return ratePub.load (std::memory_order_relaxed); }
    float getEnvValue() const { return envPub.load (std::memory_order_relaxed); }

private:
    // [0] links, [1] rechts; 2. Ordnung mit Resonanz plus 1. Ordnung = 18 dB/Oktave
    struct Inst
    {
        std::array<juce::dsp::StateVariableTPTFilter<float>, 2> hp, lp;
        std::array<juce::dsp::FirstOrderTPTFilter<float>, 2> hp1, lp1;
        std::array<PeakEq, 2> peakHp, peakLp;   // Glocke an der Grenzfrequenz (je Kanal eigene Koeffizienten)
        std::array<PeakEq, 2> peakBell;         // freies Bell-Band
    };

    double sr = 44100.0;
    std::array<Inst, kInstances> insts;
    std::array<std::array<float, 20>, 2> fmBlocks {};
    GlobalFilterSettings cur;
    double phase = 0.0;   // LFO-Position in Zyklen, fortlaufend
    std::array<float, 2> modS { { 0.0f, 0.0f } };
    float env = 0.0f, envAtk = 0.01f, envRel = 0.001f;   // Huellkurve des Eingangs (wie NoiseSource)
    std::array<float, 2> qS { { -1.0f, -1.0f } };            // geglaettete Guete HP, LP (-1 = noch nicht gesetzt)
    std::array<std::array<float, 20>, 2> qBlocks {};          // je 16er-Block: Guete HP, LP
    std::array<float, 2> boostS { { 0.0f, 0.0f } };           // geglaetteter Boost HP, LP (dB)
    std::array<std::array<float, 20>, 2> boostBlocks {};      // je 16er-Block: Boost HP, LP
    float bellS = 0.0f;                                       // geglaetteter Gain des Bell-Bands (dB)
    std::array<float, 20> bellBlocks {};
    std::array<int, 20> segStart {}, segLen {};               // Lage der Bloecke im Chunk
    int numSegs = 0;
    int framePos = 0;                                         // Position im 16er-Raster am Chunk-Anfang
    int prevLastSeg = -1;                                     // letzter Block des vorigen Chunks
    std::atomic<double> posPub { 0.0 };
    std::atomic<float> ratePub { 0.5f }, envPub { 0.0f };
};
} // namespace qe
