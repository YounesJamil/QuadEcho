#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace qe
{
constexpr int kNumUnits = 4;
constexpr int kNumAlgorithms = 10;
constexpr float kMaxDelaySeconds = 16.0f;   // 8 Takte bei 120 BPM
constexpr float kMinModFactor = 0.1f, kMaxModFactor = 10.0f;   // LFO x / ENV x
constexpr double kMaxModSeconds = 0.010 * kMaxModFactor;        // groesste Delay-Modulation (Tiefe 100 %)
constexpr double kMaxDelayBeats = 32.0;      // der Delay-Regler zeigt Notenwerte bis 8 Takte

// Notenwerte fuer Delay-Sync und LFO-Sync. Ab einem Takt (4/4) als Zahl der Takte: 1, 1.5, 2 .. 24.
// family: 0 gerade, 1 triolisch, 2 punktiert (1.5 Takte = punktierte Ganze). Seit Zustandsversion 7; vorher endete
// die Liste mit 1/1, 2/1, 4/1 (Umrechnung in StateMigration.h).
struct NoteDiv { const char* name; double beats; int family; };
inline constexpr NoteDiv kNoteDivs[] = {
    { "1/32", 0.125, 0 }, { "1/16T", 1.0 / 6.0, 1 }, { "1/16", 0.25, 0 }, { "1/16.", 0.375, 2 },
    { "1/8T", 1.0 / 3.0, 1 }, { "1/8", 0.5, 0 }, { "1/8.", 0.75, 2 }, { "1/4T", 2.0 / 3.0, 1 },
    { "1/4", 1.0, 0 }, { "1/4.", 1.5, 2 }, { "1/2T", 4.0 / 3.0, 1 }, { "1/2", 2.0, 0 },
    { "1/2.", 3.0, 2 }, { "1/1T", 8.0 / 3.0, 1 }, { "1", 4.0, 0 }, { "1.5", 6.0, 2 },
    { "2", 8.0, 0 }, { "3", 12.0, 0 }, { "4", 16.0, 0 }, { "5", 20.0, 0 }, { "6", 24.0, 0 }, { "7", 28.0, 0 }, { "8", 32.0, 0 }, { "9", 36.0, 0 }, { "10", 40.0, 0 }, { "11", 44.0, 0 }, { "12", 48.0, 0 }, { "13", 52.0, 0 }, { "14", 56.0, 0 }, { "15", 60.0, 0 }, { "16", 64.0, 0 }, { "17", 68.0, 0 }, { "18", 72.0, 0 }, { "19", 76.0, 0 }, { "20", 80.0, 0 }, { "21", 84.0, 0 }, { "22", 88.0, 0 }, { "23", 92.0, 0 }, { "24", 96.0, 0 } };
constexpr int kNumNoteDivs = (int) std::size (kNoteDivs);

enum ChannelMode { Stereo = 0, PingPong, MidSide };
enum Transition { Repitch = 0, Fade, Jump };
enum SyncMode { SyncNotes = 0, SyncTriplet, SyncDotted, Sync16th };   // wie Ableton Echo
enum RouteMode { RouteAudio = 0, RoutePhase, RoutePhaseDry };
constexpr float kMaxRouteMs = 10.0f;   // Phase Depth 100 %; Regler quadratisch (25 % = 0,6 ms)
enum Pattern { PatOff = 0, PatStairs, PatZigzag, PatSync, PatRandom };   // Schritt-Muster statt LFO-Welle
enum LfoWave { Sine = 0, Triangle, SawUp, SawDown, Square, SmoothRandom };
constexpr int kNumWaves = 6;

struct EchoSettings
{
    bool on = true, sync = false, link = true;
    float timeL = 250.0f, timeR = 250.0f;      // ms
    int divL = 5, divR = 5;                    // Index in kNoteDivs
    int syncMode = SyncNotes, syncModeR = SyncNotes;   // je Kanal: Notes/Triplet/Dotted (divL/divR) oder 16th (sixL/sixR)
    int sixL = 2, sixR = 2;                    // Anzahl 16tel, 1..16
    int mode = Stereo;
    float offset = 0.0f;                       // % der R-Zeit (Swing)
    float offsetL = 0.0f;                      // % der L-Zeit
    float feedback = 0.4f;                     // 0..1.1
    bool fbInvert = false;
    float inGainDb = 0.0f;
    bool inGainOnDry = false;
    bool hpOn = false, lpOn = false;
    float hpHz = 80.0f, lpHz = 12000.0f, res = 0.2f, hpRes = 0.2f;   // res = LP-Resonanz
    float hpBoost = 0.0f, lpBoost = 0.0f;      // dB, Glocke an der Grenzfrequenz
    int transition = Fade;
    float mix = 1.0f;                          // Echo-Pegel 0..1: das durchlaufende Signal bleibt, die Echos kommen dazu
    float outDb = 0.0f, width = 1.0f;
    int wave = Sine;
    float rateHz = 1.0f;
    bool lfoSync = false;
    int lfoDiv = 8;
    float phaseDeg = 0.0f;
    float lfoAmt = 1.0f, envAmt = 1.0f;   // Faktor auf LFO- bzw. Huellkurven-Anteil der Modulation (Delay und Filter)
    float delayDepth = 0.0f, filterDepth = 0.0f, envMix = 0.0f;
    bool mute = false, solo = false;
    bool positionDry = false;   // Platzierung wirkt auch auf den Dry-Anteil dieser Einheit
    bool lfoOn = true;          // LFO als Modulationsquelle an/aus (Huellkurve bleibt aktiv)
    float filterPhaseDeg = 0.0f; // Phasenversatz der Filter-Modulation relativ zur Delay-Modulation
    bool freeze = false;        // Schleife halten: Eingang zu, Feedback genau 1, ohne Filter und Saettigung
    bool stableFb = false;      // STABLE: Filterspitze in der Schleife ausgleichen (kein Aufschaukeln durch Resonanz/Boost)
    int pattern = PatOff;       // Schritt-Muster statt der LFO-Welle (ein LFO-Zyklus = ein Muster)
    int steps = 16;             // Stufen je Zyklus (8 oder 16)
    float glide = 0.0f;         // 0 = springt, 1 = gleitet ueber die ganze Stufe
    int seed = 1;               // Random-Muster
    bool bellOn = false;        // freies Bell-Band in der Feedback-Schleife (Glocke, Frequenz/Gain/Q frei)
    float bellHz = 1000.0f, bellGain = 0.0f, bellQ = 1.0f;
    bool chorusLoop = false;    // Chorus in der Schleife (von der Engine aus den globalen Chorus-Einstellungen gesetzt)
    int chorusMode = 0;
    float chorusMix = 0.5f;
    float wow = 0.0f;           // Wow/Flutter 0..1: Bandlauf-Schwankung der Lesestelle (unabhaengig vom LFO)
    bool tape = false;          // TAPE: Schreibweg (Eingang + Feedback) mit Band-Charakter statt tanh, siehe Tape.h
};

struct PosSettings
{
    float x = 0.0f;    // -1 links .. +1 rechts
    float y = 0.25f;   // -1 hinten .. +1 vorne (Hoerer in der Mitte, Radius = Entfernung)
    float z = 0.0f;    // -1 tief .. +1 hoch
};

// Globaler Filter hinter den Einheiten
struct GlobalFilterSettings
{
    bool hpOn = false, lpOn = false;
    float hpHz = 100.0f, lpHz = 12000.0f, res = 0.2f, hpRes = 0.2f;   // res = LP-Resonanz
    float hpBoost = 0.0f, lpBoost = 0.0f;   // dB, Glocke an der Grenzfrequenz
    bool includeDry = false;        // auch das Original filtern (Master-Filter)
    bool skipFirstEcho = false;     // erstes Echo jeder Einheit nicht filtern, nur die Wiederholungen
    bool lfoOn = true;
    int wave = Sine;
    float rateHz = 0.5f;
    bool lfoSync = false;
    int lfoDiv = 8;
    float depth = 0.0f;             // 0..1 = +-2 Oktaven
    float phaseDeg = 0.0f;
    float envMix = 0.0f;            // Modulationsquelle: 0 = LFO, 1 = Huellkurve des Eingangs (wie bei der Noise)
    bool bellOn = false;            // freies Bell-Band
    float bellHz = 1000.0f, bellGain = 0.0f, bellQ = 1.0f;
};

// Noise-Quelle (Expert-Modus)
struct NoiseSettings
{
    bool on = false;
    float level = 0.3f;             // 0..1 (100 % = intensiv)
    float color = 0.5f;             // 0 braun, 0.5 rosa, 1 weiss
    float crackle = 0.0f;           // Knistern 0..1
    float width = 1.0f;             // Stereobreite 0..1
    float into = 0.0f;              // 0 = direkt zum Ausgang, 1 = in die Delays hinein
    bool hpOn = false, lpOn = false;
    float hpHz = 200.0f, lpHz = 8000.0f, res = 0.2f, hpRes = 0.2f;    // res = LP-Resonanz
    float hpBoost = 0.0f, lpBoost = 0.0f;   // dB, Glocke an der Grenzfrequenz
    bool lfoOn = true;
    int wave = Sine;
    float rateHz = 0.5f;
    bool lfoSync = false;
    int lfoDiv = 8;
    float depthF = 0.0f, depthL = 0.0f;   // Modulation von Filter und Pegel
    float phaseDeg = 0.0f;                // Phasenversatz Pegel- gegen Filter-Modulation
    float envMix = 0.0f;
    PosSettings pos { 0.0f, 0.2f, 0.0f };
    bool bellOn = false;            // freies Bell-Band
    float bellHz = 1000.0f, bellGain = 0.0f, bellQ = 1.0f;
    float panAmt = 0.0f;            // Auto-Pan (Sinus): 0 = aus, 1 = ganz links bis ganz rechts
    float panRateHz = 0.5f;
    bool panSync = false;
    int panDiv = 8;
};

// Ducking (Expert): die Echos werden leiser, solange am Eingang gespielt wird
struct DuckSettings
{
    bool on = false;
    float thresholdDb = -30.0f;     // ab diesem Eingangspegel wird abgesenkt (weiches Knie, 6 dB)
    float amountDb = 12.0f;         // maximale Absenkung
    float attackMs = 10.0f;
    float releaseMs = 400.0f;
};

// Stereo (Expert): Breite und Enhance der Echos, mit "Include dry" auch des Originals
struct StereoSettings
{
    float width = 1.0f;             // 0 = mono, 1 = unveraendert, 2 = doppelt so breit (Seite x2)
    float enhance = 0.0f;           // 0..1: verzoegerte, hochpassgefilterte Mitte als Seite (mono-kompatibel)
    bool includeDry = false;
};

// Analoger Chorus (Expert): auf der Summe der Echos oder in der Schleife jeder Einheit
enum ChorusPlace { ChorusOnEchoes = 0, ChorusInLoop };
struct ChorusSettings
{
    bool on = false;
    int mode = 0;                   // 0 = I, 1 = II, 2 = I + II
    float mix = 0.5f;
    int place = ChorusOnEchoes;
};

struct EngineSettings
{
    GlobalFilterSettings gfilter;
    NoiseSettings noise;
    DuckSettings duck;
    StereoSettings stereo;
    ChorusSettings chorus;
    std::array<EchoSettings, kNumUnits> echo;
    std::array<PosSettings, kNumUnits> pos;
    int algorithm = 0;
    float globalMix = 0.5f;       // einziger Regler fuer das Original: 0 = nur Original, 1 = nur Echos
    float globalOutDb = 0.0f;
    bool distanceDelay = false;
    bool earlyReflections = true;
    bool binaural = false;        // Kopfhoerer-Modus (Kugel-Kopf-Modell)
    float doppler = 1.0f;         // 0..2, wirkt mit distanceDelay
    float feedbackScale = 1.0f;   // globaler Feedback-Faktor 0..2 (1 = wie in den Einheiten)
    bool softLimiter = true;
    bool limitDry = false;        // Limiter auch auf das Original; aus = Original unangetastet, nur die Echos werden begrenzt
    bool freezeAll = false;       // FREEZE ALL: alle Einheiten halten (zusaetzlich zu ihren eigenen Freeze-Schaltern)
    int routeMode = RouteAudio;   // Verbindungen zwischen den Einheiten: Audio oder Phasenmodulation (siehe QuadEngine)
    float routeDepth = 0.25f;     // Phasenmodulation: Lese-Verschiebung bei Vollaussteuerung = routeDepth^2 * kMaxRouteMs
};

// Operator-Prinzip: sources[i] = Bitmaske der Einheiten, die Einheit i speisen
// (0 = externer Eingang). outputs = Einheiten, die zum Ausgang gehen.
struct AlgoDef
{
    const char* name;
    std::array<int, kNumUnits> sources;
    int outputs;
};

inline constexpr AlgoDef kAlgorithms[kNumAlgorithms] = {
    { "Serial",      { 0, 1, 2, 4 }, 8 },    // A>B>C>D
    { "Dual Chain",  { 0, 1, 0, 4 }, 10 },   // A>B, C>D
    { "Diamond",     { 0, 1, 1, 6 }, 8 },    // A>(B,C)>D
    { "Chain+Solo",  { 0, 1, 2, 0 }, 12 },   // A>B>C, D
    { "Fan Out",     { 0, 1, 1, 1 }, 14 },   // A>(B,C,D)
    { "Fan In",      { 0, 0, 0, 7 }, 8 },    // (A,B,C)>D
    { "Split End",   { 0, 1, 2, 2 }, 12 },   // A>B>(C,D)
    { "Pair+Solo",   { 0, 1, 0, 0 }, 14 },   // A>B, C, D
    { "Merge Chain", { 0, 0, 3, 4 }, 8 },    // (A,B)>C>D
    { "Parallel",    { 0, 0, 0, 0 }, 15 } }; // A, B, C, D

// Wer ist am Ausgang hoerbar und an seiner Position platziert? Eine Ausgangseinheit, die aus ist (Bypass),
// reicht ihren Eingang nur durch: dann uebernehmen ihre Quellen die Platzierung (in einer Kette die letzte
// eingeschaltete Einheit), bei mehreren Quellen zu gleichen Teilen (sie werden ja gemittelt).
// weight[u] = Anteil der Einheit u an der Ausgangssumme. dead = Anteil stummer Einheiten (Mute): zaehlt
// fuer die Mittelung mit, klingt aber nicht. Ausgaenge, in deren Zweig keine Einheit an ist, zaehlen nicht.
// Quellen haben immer einen kleineren Index als die gespeiste Einheit (siehe kAlgorithms).
struct Placement
{
    std::array<float, kNumUnits> weight {};
    float dead = 0.0f;
    float soloDead = 0.0f;   // nur fuer die Echos: bei Solo zaehlen die uebrigen Ausgaenge mit (Solo so laut wie im Mix)
    std::array<bool, kNumUnits> idle {};   // Ausgang, in dessen Zweig nichts an ist (UI: vorab platzierbar)
};

inline Placement placementOf (int algorithm, const std::array<bool, kNumUnits>& on, const std::array<bool, kNumUnits>& muted,
                              const std::array<bool, kNumUnits>& solo)
{
    const AlgoDef& a = kAlgorithms[std::min (std::max (algorithm, 0), kNumAlgorithms - 1)];
    std::array<bool, kNumUnits> branch {};   // die Einheit selbst oder eine Quelle davor ist an
    bool anySolo = false;
    for (int u = 0; u < kNumUnits; ++u)
    {
        bool b = on[(size_t) u];
        for (int j = 0; j < u; ++j)
            if (a.sources[(size_t) u] & (1 << j)) b = b || branch[(size_t) j];
        branch[(size_t) u] = b;
        anySolo = anySolo || solo[(size_t) u];
    }
    std::array<float, kNumUnits> pending {};
    Placement p;
    int mixStarts = 0, soloStarts = 0;
    for (int u = 0; u < kNumUnits; ++u)
    {
        const bool isOutput = (a.outputs & (1 << u)) != 0;
        const bool start = anySolo ? solo[(size_t) u] : (isOutput && branch[(size_t) u]);
        if (start && ! muted[(size_t) u]) pending[(size_t) u] = 1.0f;
        p.idle[(size_t) u] = isOutput && ! branch[(size_t) u];
        mixStarts += (isOutput && branch[(size_t) u] && ! muted[(size_t) u]) ? 1 : 0;
        soloStarts += (solo[(size_t) u] && ! muted[(size_t) u]) ? 1 : 0;
    }
    if (anySolo) p.soloDead = (float) std::max (0, mixStarts - soloStarts);
    for (int u = kNumUnits - 1; u >= 0; --u)
    {
        const float w = pending[(size_t) u];
        if (w <= 0.0f) continue;
        const int mask = a.sources[(size_t) u];
        if (muted[(size_t) u]) { p.dead += w; continue; }
        if (on[(size_t) u] || mask == 0) { p.weight[(size_t) u] += w; continue; }   // aus und ohne Quelle: traegt nur das Original
        int count = 0;
        for (int j = 0; j < kNumUnits; ++j) count += (mask >> j) & 1;
        for (int j = 0; j < kNumUnits; ++j)
            if (mask & (1 << j)) pending[(size_t) j] += w / (float) count;
    }
    return p;
}

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

// Glockenfilter (Peaking EQ nach RBJ, Transposed Direct Form II) fuer bis zu zwei Kanaele: der Boost an der
// Grenzfrequenz der HP-/LP-Filter. Koeffizienten werden pro Teilblock gesetzt und folgen so der Modulation.
struct PeakEq
{
    void set (double sampleRate, float freq, float q, float gainDb)
    {
        const double A = std::pow (10.0, (double) gainDb / 40.0);
        const double w0 = 6.283185307179586 * std::min ((double) freq, 0.45 * sampleRate) / sampleRate;
        const double alpha = std::sin (w0) / (2.0 * std::max ((double) q, 0.1));
        const double cw = std::cos (w0), a0 = 1.0 + alpha / A;
        b0 = (float) ((1.0 + alpha * A) / a0);
        b1 = (float) (-2.0 * cw / a0);
        b2 = (float) ((1.0 - alpha * A) / a0);
        a1 = b1;
        a2 = (float) ((1.0 - alpha / A) / a0);
    }
    float process (int ch, float x)
    {
        const float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }

    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1[2] = { 0.0f, 0.0f }, z2[2] = { 0.0f, 0.0f };
};
constexpr float kBoostOffDb = 0.05f;   // darunter kein Glockenfilter

// Reproduzierbarer Zufallswert -1..1 fuer den LFO-Zyklus k
inline float lfoRandomAt (int64_t k)
{
    uint64_t x = (uint64_t) k * 0x9E3779B97F4A7C15ull;
    x ^= x >> 31; x *= 0xBF58476D1CE4E5B9ull; x ^= x >> 29; x *= 0x94D049BB133111EBull; x ^= x >> 32;
    return (float) ((double) (x >> 11) * (1.0 / 9007199254740992.0)) * 2.0f - 1.0f;
}

// LFO-Wellenform. t = LFO-Position in Zyklen, fortlaufend (nicht auf 0..1 umgebrochen): Random braucht
// den Zyklus, um von einem Zufallswert weich (Kosinus) zum naechsten zu gleiten.
inline float lfoWave (int wave, double t)
{
    const double cycle = std::floor (t);
    const float p = (float) (t - cycle);
    switch (wave)
    {
        case Sine:     return std::sin (6.28318530717958647692f * p);
        case Triangle: { float q = p + 0.25f; q -= std::floor (q); return 1.0f - 4.0f * std::abs (q - 0.5f); }
        case SawUp:    return 2.0f * p - 1.0f;
        case SawDown:  return 1.0f - 2.0f * p;
        case SmoothRandom:
        {
            const float a = lfoRandomAt ((int64_t) cycle), b = lfoRandomAt ((int64_t) cycle + 1);
            return a + (b - a) * (0.5f - 0.5f * std::cos (3.14159265358979f * p));
        }
        default:       return p < 0.5f ? 1.0f : -1.0f;
    }
}

// Wie lfoWave, aber in double (Delay-Modulation: die Welle bestimmt den Lese-Index)
inline double lfoWaveD (int wave, double t)
{
    const double cycle = std::floor (t);
    const double p = t - cycle;
    switch (wave)
    {
        case Sine:     return std::sin (6.283185307179586 * p);
        case Triangle: { double q = p + 0.25; q -= std::floor (q); return 1.0 - 4.0 * std::abs (q - 0.5); }
        case SawUp:    return 2.0 * p - 1.0;
        case SawDown:  return 1.0 - 2.0 * p;
        case SmoothRandom:
        {
            const double a = lfoRandomAt ((int64_t) cycle), b = lfoRandomAt ((int64_t) cycle + 1);
            return a + (b - a) * (0.5 - 0.5 * std::cos (3.141592653589793 * p));
        }
        default:       return p < 0.5 ? 1.0 : -1.0;
    }
}

// Wert der Stufe k (0 .. steps-1) eines Schritt-Musters, -1..1
inline double patternStep (int pattern, int steps, int seed, int k)
{
    const double pos = steps > 1 ? (double) k / (double) (steps - 1) : 0.0;
    switch (pattern)
    {
        case PatStairs: return 2.0 * pos - 1.0;                                            // Treppe aufwaerts
        case PatZigzag: return (k % 2 == 0 ? 1.0 : -1.0) * (0.25 + 0.75 * pos);             // abwechselnd, waechst
        case PatSync:   { const int m = k % 8; return (m == 0 || m == 3 || m == 6) ? 1.0 : -0.6; }   // 3-3-2 (Tresillo)
        default:        return lfoRandomAt ((int64_t) seed * 1000003 + k);                  // Random, fest je Seed
    }
}

// Schritt-Muster als Welle: t = Position in Zyklen; Glide gleitet am Ende jeder Stufe (Kosinus) zur naechsten
inline double stepWaveD (const EchoSettings& s, double t)
{
    const int steps = s.steps == 8 ? 8 : 16;
    const double p = (t - std::floor (t)) * (double) steps;
    const int k = std::min ((int) p, steps - 1);
    const double a = patternStep (s.pattern, steps, s.seed, k);
    if (s.glide <= 0.0f) return a;
    const double gl = (double) std::min (s.glide, 1.0f), frac = p - (double) k;
    const double w = std::min (std::max ((frac - (1.0 - gl)) / gl, 0.0), 1.0);
    if (w <= 0.0) return a;
    const double b = patternStep (s.pattern, steps, s.seed, (k + 1) % steps);
    return a + (b - a) * (0.5 - 0.5 * std::cos (3.141592653589793 * w));
}

// Modulationsquelle der Einheit: Schritt-Muster oder LFO-Welle
inline double modWaveD (const EchoSettings& s, double t) { return s.pattern != PatOff ? stepWaveD (s, t) : lfoWaveD (s.wave, t); }

// Koeffizienten der Saettigungen: werden einmal pro Teilblock berechnet, nicht pro Sample
struct SatCoefs
{
    float drive = 0.0f;
    // Input-Saettigung (analogSat)
    float k = 1.0f, comp = 1.0f, blend = 0.0f;
    // Schleifen-Saettigung (loopSat)
    float t = 1.0f, b = 0.0f, tb = 0.0f, norm = 1.0f;
    // Input Gain unter 0 dB: linearer Bereich bis knee, darueber weich gegen 1,0 (0 = reiner tanh)
    float knee = 0.0f, kneeRange = 1.0f;
};

// Knie der Schleifen-Saettigung aus dem Input Gain: 0 dB und mehr = 0 (tanh wie bisher), -24 dB = 0,8 (sauber bis -1,9 dBFS)
inline float loopKneeOf (float inGainDb) { return 0.8f * std::min (std::max (-inGainDb / 24.0f, 0.0f), 1.0f); }

inline SatCoefs makeSatCoefs (float drive, float knee = 0.0f)
{
    SatCoefs c;
    c.drive = drive;
    if (drive <= 1.0e-4f)
    {
        c.knee = knee > 1.0e-4f ? knee : 0.0f;
        c.kneeRange = 1.0f - c.knee;
        return c;
    }
    constexpr float b0 = 0.12f, tb0 = 0.119427915f, n0 = 0.985737f;   // tanh(b0), 1 - tanh(b0)^2
    c.k = 1.0f + drive * 2.5f;
    c.comp = 0.25f / ((std::tanh (0.25f * c.k + b0) - tb0) / n0);
    c.blend = std::min (1.0f, drive * 6.0f);
    c.t = 1.0f - 0.6f * drive;
    c.b = 0.12f * drive;
    c.tb = std::tanh (c.b);
    c.norm = 1.0f - c.tb * c.tb;
    return c;
}

// Analoges Saettigungs-Gefuehl fuer den Input: weiche tanh-Kennlinie mit leichter Asymmetrie
// (gerade Obertoene). drive 0 = transparent. Kleine Signale bleiben nahezu unveraendert.
inline float analogSat (float x, const SatCoefs& c)
{
    if (c.blend <= 0.0f) return x;
    constexpr float b0 = 0.12f, tb0 = 0.119427915f, n0 = 0.985737f;
    const float y = ((std::tanh (x * c.k + b0) - tb0) / n0) * c.comp;
    return x + (y - x) * c.blend;
}

// Saettigung IN der Feedback-Schleife (Band-/Pedal-Charakter): Steigung 1 im Ursprung (stabil),
// mit steigendem Drive frueher und asymmetrischer saettigend. drive 0 = tanh.
inline float loopSat (float x, const SatCoefs& c)
{
    if (c.drive <= 1.0e-4f)
    {
        if (c.knee <= 0.0f) return std::tanh (x);
        // schwaechere Saettigung (Input Gain unter 0 dB): bis zum Knie linear, darueber tanh-Uebergang zur Grenze 1,0
        const float a = std::abs (x);
        if (a <= c.knee) return x;
        const float y = c.knee + c.kneeRange * std::tanh ((a - c.knee) / c.kneeRange);
        return x < 0.0f ? -y : y;
    }
    return c.t * (std::tanh (x / c.t + c.b) - c.tb) / c.norm;
}

constexpr float kOutputOffDb = -1000.0f;  // Output hat keinen Aus-Zustand mehr (dafuer: Unit On, Mute)
inline bool isMuted (const EchoSettings& s) { return s.mute || s.outDb <= kOutputOffDb; }
inline float outputGain (const EchoSettings& s) { return isMuted (s) ? 0.0f : dbToGain (s.outDb); }

inline double divBeatsOf (int i) { return kNoteDivs[std::min (std::max (i, 0), kNumNoteDivs - 1)].beats; }

constexpr int kMaxSixteenths = 16;
inline double sixteenthBeats (int n) { return 0.25 * (double) std::min (std::max (n, 1), kMaxSixteenths); }

// Familie eines Notenwerts in kNoteDivs: 0 gerade, 1 triolisch ("T"), 2 punktiert (".")
inline int noteFamily (int i) { return kNoteDivs[std::min (std::max (i, 0), kNumNoteDivs - 1)].family; }
inline int familyOfMode (int mode) { return mode == SyncTriplet ? 1 : (mode == SyncDotted ? 2 : 0); }

// Grundnote (gerade) eines Notenwerts in Beats: 1/8T und 1/8. haben die Grundnote 1/8
inline double noteBaseBeats (int i)
{
    const double b = divBeatsOf (i);
    const int f = noteFamily (i);
    return f == 1 ? b * 1.5 : (f == 2 ? b / 1.5 : b);
}

// Notenwert im gewaehlten Sync-Modus: dieselbe Grundnote in der Familie des Modus (1/8 -> 1/8T -> 1/8.),
// gibt es sie dort nicht, die naechstliegende
inline int divInMode (int div, int mode)
{
    div = std::min (std::max (div, 0), kNumNoteDivs - 1);
    const int fam = familyOfMode (mode);
    if (noteFamily (div) == fam) return div;
    int best = div;
    double bestDist = 1.0e9;
    for (int i = 0; i < kNumNoteDivs; ++i)
        if (noteFamily (i) == fam)
        {
            const double d = std::abs (std::log (noteBaseBeats (i) / noteBaseBeats (div)));
            if (d < bestDist) { bestDist = d; best = i; }
        }
    return best;
}

// Effektive Delay-Zeiten L/R in ms (Sync in Notenwerten oder 16teln, Link, Offset, Begrenzung auf 16 s)
inline void delayTimesMs (const EchoSettings& s, double bpm, double& msL, double& msR)
{
    const double beatMs = 60000.0 / std::max (20.0, bpm);
    auto syncBeats = [&s] (bool right)
    {
        const int mode = right ? s.syncModeR : s.syncMode;
        return mode == Sync16th ? sixteenthBeats (right ? s.sixR : s.sixL) : divBeatsOf (divInMode (right ? s.divR : s.divL, mode));
    };
    msL = s.sync ? syncBeats (false) * beatMs : (double) s.timeL;
    msR = s.link ? msL : (s.sync ? syncBeats (true) * beatMs : (double) s.timeR);
    msL *= 1.0 + 0.01 * (double) s.offsetL;
    msR *= 1.0 + 0.01 * (double) s.offset;
    const double maxMs = (double) kMaxDelaySeconds * 1000.0;
    msL = std::min (std::max (msL, 1.0), maxMs);
    msR = std::min (std::max (msR, 1.0), maxMs);
}
} // namespace qe
