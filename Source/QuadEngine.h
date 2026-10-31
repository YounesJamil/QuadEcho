#pragma once
#include "DelayUnit.h"
#include "GlobalFilter.h"
#include "NoiseSource.h"
#include "Spatializer.h"
#include "Chorus.h"

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
    // Stereo in-place, beliebige Blockgroesse. ppq = Songposition am Blockanfang in Vierteln (< 0 = unbekannt oder
    // Wiedergabe steht): LFOs mit Sync richten ihre Phase daran aus.
    void process (float* l, float* r, int n, const EngineSettings& s, double bpm, double ppq = -1.0);

    // Fuer die UI (lockfrei): Spitzenpegel seit dem letzten Abruf (setzt zurueck)
    float takeUnitPeak (int u) { return unitPeaks[(size_t) u].exchange (0.0f); }
    float takeMasterPeak() { return masterPeak.exchange (0.0f); }
    // Groesste Ducking-Absenkung in dB seit dem letzten Abruf (setzt zurueck)
    float takeDuckReduction() { return duckReduction.exchange (0.0f); }
    // Spitzenpegel des Eingangs seit dem letzten Abruf (setzt zurueck), fuer die Threshold-Anzeige
    float takeInputPeak() { return inputPeak.exchange (0.0f); }
    const DelayUnit& unit (int u) const { return units[(size_t) u]; }
    const GlobalFilter& globalFilter() const { return gfilter; }
    const NoiseSource& noiseSource() const { return noise; }

private:
    static constexpr int kChunk = 256;
    static constexpr int kClearPerSample = 512;   // verteiltes Loeschen einer ausgeschalteten Einheit: Samples je Leitung pro Audio-Sample
    struct Block { std::array<float, kChunk> l {}, r {}; };

    void processChunk (float* l, float* r, int n, const EngineSettings& s, double bpm, double ppq);
    void computeDuck (int n, const DuckSettings& d, float* gain);

    double sr = 44100.0;
    std::array<DelayUnit, kNumUnits> units;
    std::array<Spatializer, kNumUnits> spatial;
    NoiseSource noise;
    Spatializer noiseSpatial;
    GlobalFilter gfilter;
    std::array<juce::SmoothedValue<float>, kNumUnits> onSm, outW;
    juce::SmoothedValue<float> activeSm, deadSm, soloDeadSm, intoSm, inclDrySm;
    std::array<juce::SmoothedValue<float>, kNumUnits> dryPlaceSm, dryPanSm;   // Place Dry: Anteil und Seite (X)
    std::array<bool, kNumUnits> wasActive { { false, false, false, false } };
    std::array<bool, kNumUnits> spatialLive { { false, false, false, false } };
    bool noiseSpatialLive = false;
    std::array<Block, kNumUnits> outs, origs, firsts, spOut, spDry;
    Block ext, src, origSrc, tmp, origTmp, firstTmp, wetM, noiseRaw, noiseOut, phaseMod;
    std::array<std::atomic<float>, kNumUnits> unitPeaks;
    std::atomic<float> masterPeak { 0.0f }, duckReduction { 0.0f }, inputPeak { 0.0f };
    float duckEnv = 0.0f, duckDb = 0.0f, duckEnvRel = 0.001f;

    int activeAlgo = 0, activeRoute = RouteAudio;
    enum class Gate { Idle, Down, Up } gateState = Gate::Idle;
    float gate = 1.0f;
    juce::SmoothedValue<float> mixSm, outSm, routeDepthSm;
    // Stereo (Expert): Breite/Enhance je fuer Echos und Original (linear, daher getrennt und weich zuschaltbar)
    struct Enhancer
    {
        std::vector<float> buf;
        int pos = 0;
        float hp = 0.0f;
        void prepare (int len) { buf.assign ((size_t) std::max (len, 1), 0.0f); reset(); }
        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); pos = 0; hp = 0.0f; }
        // Mitte hochpassgefiltert (300 Hz) und ca. 9 ms verzoegert: gibt sie als Seite dazu, in L + R hebt sie sich auf
        void process (float& l, float& r, float width, float enh, float hpA)
        {
            const float mid = 0.5f * (l + r);
            float side = 0.5f * (l - r) * width;
            hp += hpA * (mid - hp);
            const float delayed = buf[(size_t) pos];
            buf[(size_t) pos] = mid - hp;
            pos = pos + 1 == (int) buf.size() ? 0 : pos + 1;
            side += 0.6f * enh * delayed;
            l = mid + side;
            r = mid - side;
        }
    };
    std::array<Enhancer, 2> enhancer;   // 0 = Echos, 1 = Original
    juce::SmoothedValue<float> stWidthSm, stEnhSm, stInclSm;
    float enhHpA = 0.04f;
    bool stereoLive = false;
    BbdChorus echoChorus;               // Chorus auf der Summe der Echos
    bool initialised = false;
    bool poisoned = false;   // ein Ausgangs-Sample war nicht endlich: am Ende des Chunks alles zuruecksetzen
};
} // namespace qe
