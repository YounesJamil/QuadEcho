#pragma once
#include "DelayUnit.h"
#include "GlobalFilter.h"
#include "NoiseSource.h"
#include "Spatializer.h"

namespace qe
{
// Vier Einheiten + Algorithmus-Routing + Noise-Quelle + globaler Filter + globaler Dry/Wet,
// Output und Soft-Limiter. Rein DSP, ohne JUCE-Parameter (testbar ohne Plugin-Host).
//
// Signalfluss: Zwischen den Einheiten laeuft das Signal unveraendert (Echo-Ausgang).
// Nur Einheiten, die im Algorithmus zum Ausgang gehen, werden im 3D-Raum platziert
// (Operator-Prinzip). Ist eine Ausgangseinheit aus (Bypass), uebernehmen ihre Quellen die Platzierung,
// in einer Kette also die letzte eingeschaltete Einheit (placementOf in DspTypes.h).
// Solo hoert stattdessen die gewaehlten Einheiten an ihrer Position.
// Stumme Einheiten (Mute) liefern ueberall Stille. Ausgangseinheiten, in deren Zweig keine
// Einheit aktiv ist, zaehlen nicht zur Summe (kein doppeltes Dry-Signal).
// Original und Echos: Neben dem Signal laeuft eine Spur des reinen Originals durch die Einheiten
// (nur deren Dry-Weg). Am Ausgang ist Echo = Ausgang - Original-Spur. So bestimmt allein der globale
// Dry/Wet, wie viel Original zu hoeren ist (100 % = nur Echos, z. B. fuer einen Send).
// Noise (Expert): eigene Quelle mit eigener Position; "Into echoes" ueberblendet zwischen
// direkt zum Ausgang und in die Delays hinein. Der globale Filter sitzt hinter der Summe der Echos
// (Noise direkt eingeschlossen); mit "Include dry" filtert er auch das Original.
// Ducking (Expert): senkt die Echos ab, solange der Eingang laut ist (nicht Original und Noise).
class QuadEngine
{
public:
    void prepare (double sampleRate);
    void reset();
    // Stereo in-place, beliebige Blockgroesse
    void process (float* l, float* r, int n, const EngineSettings& s, double bpm);

    // Fuer die UI (lockfrei): Spitzenpegel seit dem letzten Abruf (setzt zurueck)
    float takeUnitPeak (int u) { return unitPeaks[(size_t) u].exchange (0.0f); }
    float takeMasterPeak() { return masterPeak.exchange (0.0f); }
    // Groesste Ducking-Absenkung in dB seit dem letzten Abruf (setzt zurueck)
    float takeDuckReduction() { return duckReduction.exchange (0.0f); }
    const DelayUnit& unit (int u) const { return units[(size_t) u]; }

private:
    static constexpr int kChunk = 256;
    static constexpr int kClearPerSample = 512;   // verteiltes Loeschen einer ausgeschalteten Einheit: Samples je Leitung pro Audio-Sample
    struct Block { std::array<float, kChunk> l {}, r {}; };

    void processChunk (float* l, float* r, int n, const EngineSettings& s, double bpm);
    void computeDuck (int n, const DuckSettings& d, float* gain);

    double sr = 44100.0;
    std::array<DelayUnit, kNumUnits> units;
    std::array<Spatializer, kNumUnits> spatial;
    NoiseSource noise;
    Spatializer noiseSpatial;
    GlobalFilter gfilter;
    std::array<juce::SmoothedValue<float>, kNumUnits> onSm, outW;
    juce::SmoothedValue<float> activeSm, deadSm, intoSm, inclDrySm;
    std::array<juce::SmoothedValue<float>, kNumUnits> dryPlaceSm, dryPanSm;   // Place Dry: Anteil und Seite (X)
    std::array<bool, kNumUnits> wasActive { { false, false, false, false } };
    std::array<bool, kNumUnits> spatialLive { { false, false, false, false } };
    bool noiseSpatialLive = false;
    std::array<Block, kNumUnits> outs, origs, firsts, spOut, spDry;
    Block ext, src, origSrc, tmp, origTmp, firstTmp, wetM, noiseRaw, noiseOut;
    std::array<std::atomic<float>, kNumUnits> unitPeaks;
    std::atomic<float> masterPeak { 0.0f }, duckReduction { 0.0f };
    float duckEnv = 0.0f, duckDb = 0.0f, duckEnvRel = 0.001f;

    int activeAlgo = 0;
    enum class Gate { Idle, Down, Up } gateState = Gate::Idle;
    float gate = 1.0f;
    juce::SmoothedValue<float> mixSm, outSm;
    bool initialised = false;
    bool poisoned = false;   // ein Ausgangs-Sample war nicht endlich: am Ende des Chunks alles zuruecksetzen
};
} // namespace qe
