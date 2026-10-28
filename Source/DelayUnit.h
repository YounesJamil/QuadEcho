#pragma once
#include <juce_dsp/juce_dsp.h>
#include "DelayLine.h"
#include "DspTypes.h"
#include "Oversampler.h"

namespace qe
{
// Eine Echo-Einheit (Stereo rein/raus), ohne Raumplatzierung.
// Ausgang = durchlaufendes Signal + Echos * Echo-Pegel, beides mit dem Output der Einheit.
// origIn/origOut (optional): der reine Original-Anteil des Eingangs, nur durch den Dry-Weg dieser
// Einheit geschickt (Gain on Dry, Output, Mute). Damit trennt die Engine am Ende Original und Echos.
class DelayUnit
{
public:
    void prepare (double sampleRate);
    void reset();
    // Wie reset(), aber die grossen Delay-Leitungen werden erst mit clearSome() nach und nach geloescht
    // (bei 192 kHz 16 MB: in einem Block geloescht dauert das ein Vielfaches der Blockzeit).
    // Vor dem naechsten process() muss finishClear() laufen, falls noch etwas uebrig ist.
    void beginDeferredReset();
    // Songposition (Viertel, ppq) am Anfang des naechsten process()-Aufrufs; < 0 = unbekannt oder Wiedergabe steht.
    // Mit LFO-Sync folgt die LFO-Phase dieser Position (Neuausrichtung nur bei Spruengen).
    void setSongPosition (double ppq) { songPpq = ppq; }
    void clearSome (int samplesPerLine);
    void finishClear() { clearSome (capacity); }
    bool clearPending() const { return clearPos < capacity; }
    void process (const float* inL, const float* inR, float* outL, float* outR,
                  int n, const EchoSettings& s, double bpm,
                  float* wetOutL = nullptr, float* wetOutR = nullptr,
                  float* firstOutL = nullptr, float* firstOutR = nullptr,
                  const float* origInL = nullptr, const float* origInR = nullptr,
                  float* origOutL = nullptr, float* origOutR = nullptr);

    // Fuer die UI (lockfrei): aktuelle Modulation
    float getLfoValue() const { return lfoPub.load (std::memory_order_relaxed); }
    float getEnvValue() const { return envPub.load (std::memory_order_relaxed); }
    float getModValue() const { return modPub.load (std::memory_order_relaxed); }
    double getLfoPos() const { return posPub.load (std::memory_order_relaxed); }   // fortlaufend, in Zyklen
    float getLfoRateHz() const { return rateHzPub.load (std::memory_order_relaxed); }

private:
    struct Line
    {
        DelayLine dl;
        DelayLine first;   // gleiche Verzoegerung ohne Feedback: nur das erste Echo
        double baseA = 0.0, baseB = 0.0;   // Lese-Position in Samples (double, siehe DelayLine::readHermite)
        double cur = 0.0;   // Repitch-Glaettung in double: in float bleibt sie bis 1200 Samples vor dem Ziel stehen
        bool fading = false, init = false;
        int fadePos = 0, fadeLen = 1;
    };

    float readLine (Line& l, double modSamples, double target, int transition, float& first, bool needFirst, bool frozen);
    void resetState();

    double sr = 44100.0;
    int capacity = 0;
    int clearPos = 0;   // bis hierhin sind die Leitungen geloescht (capacity = fertig)
    std::array<Line, 2> line;
    juce::dsp::StateVariableTPTFilter<float> hp, lp;      // 2. Ordnung mit Resonanz ...
    juce::dsp::FirstOrderTPTFilter<float> hp1, lp1;        // ... plus 1. Ordnung = 18 dB/Oktave
    juce::SmoothedValue<float> inGainSm, fbSm, mixSm, outSm, widthSm, fDepthSm, driveSm, kneeSm, freezeSm;
    juce::SmoothedValue<double> depthSm;   // Modulationstiefe in Samples: bestimmt den Lese-Index
    double lfoPos = 0.0;   // LFO-Position in Zyklen, fortlaufend
    double songPpq = -1.0;
    // Huellkurve und Modulationsglaettung in double: bei 192 kHz sind die Koeffizienten ~1e-5, in float ungenau
    double modSm[2] = { 0.0, 0.0 };
    double fModSm = 0.0;
    double env = 0.0;
    float envMapped = 0.0f;
    double rpCoef = 0.001;
    double modCoef = 0.01, envAtk = 0.01, envRel = 0.001;
    float qCoef = 0.03f;
    float qSm[2] = { 0.8f, 0.8f };   // geglaettete Guete HP, LP
    float boostSm[2] = { 0.0f, 0.0f };   // geglaetteter Boost HP, LP (dB)
    PeakEq peakHp, peakLp;              // Glocke an der Grenzfrequenz
    std::atomic<float> lfoPub { 0.0f }, envPub { 0.0f }, modPub { 0.0f }, rateHzPub { 1.0f };
    std::atomic<double> posPub { 0.0 };
    int lastTransition = -1;
    // Saettigungen mit doppelter Rate (nur unter 88,2 kHz): Input- und Schleifenpfad verzoegern dabei um osLatency,
    // die Leitungen werden um genau so viel frueher gelesen; der Freeze-Pfad laeuft ueber eine exakte Verzoegerung
    bool useOs = false;
    double osLatency = 0.0;
    std::array<Oversampler2x, 2> osX, osF;
    std::array<LatencyDelay, 2> rDelay, xDelay;   // xDelay: Input-Pfad ohne Drive bleibt eine exakte Kopie
    int xOsWarm = -1;                             // Input-Oversampling: -1 = aus (Drive 0), sonst Samples seit dem Einschalten
    int framePos = 0;                   // Position im 16er-Teilblock (Steuerraster), ueber Aufrufe hinweg
    SatCoefs frameSat;                  // Steuerwerte des laufenden Teilblocks
    bool frameHpOn = false, frameLpOn = false, frameHpPeak = false, frameLpPeak = false;
    bool initialised = false;
    bool poisoned = false;   // ein Schleifen-Sample war nicht endlich: am Blockende neu aufsetzen
};
} // namespace qe
