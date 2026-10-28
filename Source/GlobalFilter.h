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
    void beginChunk (const GlobalFilterSettings& s, double bpm, int n);
    bool active() const { return cur.hpOn || cur.lpOn; }
    // verarbeitet den Chunk (n Samples, wie bei beginChunk) in-place
    void process (int inst, float* l, float* r, int n);

private:
    // [0] links, [1] rechts; 2. Ordnung mit Resonanz plus 1. Ordnung = 18 dB/Oktave
    struct Inst
    {
        std::array<juce::dsp::StateVariableTPTFilter<float>, 2> hp, lp;
        std::array<juce::dsp::FirstOrderTPTFilter<float>, 2> hp1, lp1;
        std::array<PeakEq, 2> peakHp, peakLp;   // Glocke an der Grenzfrequenz (je Kanal eigene Koeffizienten)
    };

    double sr = 44100.0;
    std::array<Inst, kInstances> insts;
    std::array<std::array<float, 20>, 2> fmBlocks {};
    GlobalFilterSettings cur;
    double phase = 0.0;   // LFO-Position in Zyklen, fortlaufend
    std::array<float, 2> modS { { 0.0f, 0.0f } };
    std::array<float, 2> qS { { -1.0f, -1.0f } };            // geglaettete Guete HP, LP (-1 = noch nicht gesetzt)
    std::array<std::array<float, 20>, 2> qBlocks {};          // je 16er-Block: Guete HP, LP
    std::array<float, 2> boostS { { 0.0f, 0.0f } };           // geglaetteter Boost HP, LP (dB)
    std::array<std::array<float, 20>, 2> boostBlocks {};      // je 16er-Block: Boost HP, LP
    std::array<int, 20> segStart {}, segLen {};               // Lage der Bloecke im Chunk
    int numSegs = 0;
    int framePos = 0;                                         // Position im 16er-Raster am Chunk-Anfang
    int prevLastSeg = -1;                                     // letzter Block des vorigen Chunks
};
} // namespace qe
