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
constexpr float kMaxDelaySeconds = 4.0f;

struct NoteDiv { const char* name; double beats; };
inline constexpr NoteDiv kNoteDivs[] = {
    { "1/32", 0.125 }, { "1/16T", 1.0 / 6.0 }, { "1/16", 0.25 }, { "1/16.", 0.375 },
    { "1/8T", 1.0 / 3.0 }, { "1/8", 0.5 }, { "1/8.", 0.75 }, { "1/4T", 2.0 / 3.0 },
    { "1/4", 1.0 }, { "1/4.", 1.5 }, { "1/2T", 4.0 / 3.0 }, { "1/2", 2.0 },
    { "1/2.", 3.0 }, { "1/1T", 8.0 / 3.0 }, { "1/1", 4.0 }, { "2/1", 8.0 }, { "4/1", 16.0 } };
constexpr int kNumNoteDivs = 17;

enum ChannelMode { Stereo = 0, PingPong, MidSide };
enum Transition { Repitch = 0, Fade, Jump };
enum SyncMode { SyncNotes = 0, SyncTriplet, SyncDotted, Sync16th };   // wie Ableton Echo
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
    bool x4 = false;
    float delayDepth = 0.0f, filterDepth = 0.0f, envMix = 0.0f;
    bool mute = false, solo = false;
    bool positionDry = false;   // Platzierung wirkt auch auf den Dry-Anteil dieser Einheit
    bool lfoOn = true;          // LFO als Modulationsquelle an/aus (Huellkurve bleibt aktiv)
    float filterPhaseDeg = 0.0f; // Phasenversatz der Filter-Modulation relativ zur Delay-Modulation
    bool freeze = false;        // Schleife halten: Eingang zu, Feedback genau 1, ohne Filter und Saettigung
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

struct EngineSettings
{
    GlobalFilterSettings gfilter;
    NoiseSettings noise;
    DuckSettings duck;
    std::array<EchoSettings, kNumUnits> echo;
    std::array<PosSettings, kNumUnits> pos;
    int algorithm = 0;
    float globalMix = 0.5f;       // einziger Regler fuer das Original: 0 = nur Original, 1 = nur Echos
    float globalOutDb = 0.0f;
    bool distanceDelay = false;
    bool earlyReflections = true;
    bool binaural = false;        // Kopfhoerer-Modus (Kugel-Kopf-Modell)
    float doppler = 1.0f;         // 0..2, wirkt mit distanceDelay
    float feedbackScale = 1.0f;   // globale Feedback-Bremse 0..1
    bool softLimiter = true;
    bool limitDry = false;        // Limiter auch auf das Original; aus = Original unangetastet, nur die Echos werden begrenzt
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
    for (int u = 0; u < kNumUnits; ++u)
    {
        const bool isOutput = (a.outputs & (1 << u)) != 0;
        const bool start = anySolo ? solo[(size_t) u] : (isOutput && branch[(size_t) u]);
        if (start && ! muted[(size_t) u]) pending[(size_t) u] = 1.0f;
        p.idle[(size_t) u] = isOutput && ! branch[(size_t) u];
    }
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

// Wie lfoWave, aber in double (Delay-Modulation: die Welle bestimmt den Lese-Index, siehe AUDIT.md C5/C6)
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

inline float analogSat (float x, const SatCoefs& c)
{
    if (c.blend <= 0.0f) return x;
    constexpr float b0 = 0.12f, tb0 = 0.119427915f, n0 = 0.985737f;
    const float y = ((std::tanh (x * c.k + b0) - tb0) / n0) * c.comp;
    return x + (y - x) * c.blend;
}

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

// Saettigung IN der Feedback-Schleife (Band-/Pedal-Charakter): Steigung 1 im Ursprung (stabil),
// mit steigendem Drive frueher und asymmetrischer saettigend. drive 0 = tanh.
inline float loopSat (float x, float drive)
{
    if (drive <= 1.0e-4f) return std::tanh (x);
    const float t = 1.0f - 0.6f * drive;
    const float b = 0.12f * drive;
    const float tb = std::tanh (b), norm = 1.0f - tb * tb;
    return t * (std::tanh (x / t + b) - tb) / norm;
}

// Analoges Saettigungs-Gefuehl fuer den Input: weiche tanh-Kennlinie mit leichter Asymmetrie
// (gerade Obertoene). drive 0 = transparent. Kleine Signale bleiben nahezu unveraendert.
inline float analogSat (float x, float drive)
{
    if (drive <= 1.0e-4f) return x;
    constexpr float b = 0.12f, tb = 0.119427915f, norm = 0.985737f;   // tanh(b), 1 - tanh(b)^2
    const float k = 1.0f + drive * 2.5f;
    auto f = [=] (float v) { return (std::tanh (v + b) - tb) / norm; };
    const float y = f (x * k) * (0.25f / f (0.25f * k));
    const float blend = std::min (1.0f, drive * 6.0f);
    return x + (y - x) * blend;
}

constexpr float kOutputOffDb = -1000.0f;  // Output hat keinen Aus-Zustand mehr (dafuer: Unit On, Mute)
inline bool isMuted (const EchoSettings& s) { return s.mute || s.outDb <= kOutputOffDb; }
inline float outputGain (const EchoSettings& s) { return isMuted (s) ? 0.0f : dbToGain (s.outDb); }

inline double divBeatsOf (int i) { return kNoteDivs[std::min (std::max (i, 0), kNumNoteDivs - 1)].beats; }

constexpr int kMaxSixteenths = 16;
inline double sixteenthBeats (int n) { return 0.25 * (double) std::min (std::max (n, 1), kMaxSixteenths); }

// Familie eines Notenwerts in kNoteDivs: 0 gerade, 1 triolisch ("T"), 2 punktiert (".")
inline int noteFamily (int i)
{
    const char* name = kNoteDivs[std::min (std::max (i, 0), kNumNoteDivs - 1)].name;
    const char last = name[std::strlen (name) - 1];
    return last == 'T' ? 1 : (last == '.' ? 2 : 0);
}
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

// Effektive Delay-Zeiten L/R in ms (Sync in Notenwerten oder 16teln, Link, Offset, Begrenzung auf 4 s)
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
