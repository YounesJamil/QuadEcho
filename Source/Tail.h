#pragma once
#include <complex>
#include <limits>
#include "DspTypes.h"

namespace qe
{
// Filterkette in der Feedback-Schleife einer Einheit (HP 2. + 1. Ordnung, LP 2. + 1. Ordnung, Glocken an den
// Grenzfrequenzen). Formeln wie im Plugin: TPT-Filter = bilineare Transformation mit Vorverzerrung auf fc,
// Glocke nach RBJ (PeakEq).
struct LoopFilterSpec
{
    double sr = 48000.0;
    bool hpOn = false, lpOn = false;
    double hpHz = 80.0, lpHz = 12000.0, qHp = 0.7071, qLp = 0.7071;
    PeakEq peakHp, peakLp, peakBell;
    bool hpPeak = false, lpPeak = false, bellPk = false;
    double bellHz = 1000.0;

    LoopFilterSpec (double sampleRate, bool hpOn_, double hpHz_, double qHp_, float hpBoostDb,
                    bool lpOn_, double lpHz_, double qLp_, float lpBoostDb,
                    float bellDb = 0.0f, double bellHz_ = 1000.0, double bellQ = 1.0)
        : sr (sampleRate), hpOn (hpOn_), lpOn (lpOn_), hpHz (hpHz_), lpHz (lpHz_), qHp (qHp_), qLp (qLp_), bellHz (bellHz_)
    {
        hpPeak = hpOn && hpBoostDb > kBoostOffDb;
        lpPeak = lpOn && lpBoostDb > kBoostOffDb;
        bellPk = std::abs (bellDb) > kBoostOffDb;
        if (hpPeak) peakHp.set (sr, (float) hpHz, (float) qHp, hpBoostDb);
        if (lpPeak) peakLp.set (sr, (float) lpHz, (float) qLp, lpBoostDb);
        if (bellPk) peakBell.set (sr, (float) bellHz, (float) bellQ, bellDb);
    }

    double magnitude (double f) const
    {
        constexpr double pi = 3.141592653589793;
        const double w = 2.0 * pi * f / sr;
        const std::complex<double> z = std::polar (1.0, w), j (0.0, 1.0);
        auto bell = [z] (const PeakEq& p)
        {
            const auto zi = 1.0 / z;
            return ((double) p.b0 + (double) p.b1 * zi + (double) p.b2 * zi * zi) / (1.0 + (double) p.a1 * zi + (double) p.a2 * zi * zi);
        };
        std::complex<double> h = 1.0;
        if (hpOn)
        {
            const double v = std::tan (w / 2.0) / std::tan (pi * hpHz / sr);
            h *= (-v * v / (1.0 - v * v + j * v / qHp)) * (j * v / (1.0 + j * v));
            if (hpPeak) h *= bell (peakHp);
        }
        if (lpOn)
        {
            const double v = std::tan (w / 2.0) / std::tan (pi * lpHz / sr);
            h *= (1.0 / (1.0 - v * v + j * v / qLp)) * (1.0 / (1.0 + j * v));
            if (lpPeak) h *= bell (peakLp);
        }
        if (bellPk) h *= bell (peakBell);
        return std::abs (h);
    }

    // Maximum auf einem logarithmischen Raster (genau, fuer die Tail-Laenge)
    double peakOnGrid() const
    {
        if (! hpOn && ! lpOn && ! bellPk) return 1.0;
        const double maxF = 0.45 * sr;
        double best = 0.0;
        constexpr int kPoints = 240;
        for (int k = 0; k <= kPoints; ++k)
            best = std::max (best, magnitude (20.0 * std::pow (maxF / 20.0, (double) k / kPoints)));
        return best;
    }

    // Maximum an wenigen Stellen (schnell, fuer die Schleife): die Spitzen liegen an den Grenzfrequenzen
    // (Glocken) bzw. knapp darunter (LP-Resonanz) oder darueber (HP-Resonanz)
    double peakFast() const
    {
        if (! hpOn && ! lpOn && ! bellPk) return 1.0;
        const double maxF = 0.45 * sr;
        double best = 0.0;
        auto at = [&] (double f) { best = std::max (best, magnitude (std::min (std::max (f, 10.0), maxF))); };
        if (lpOn)
        {
            if (qLp > 0.7072) at (lpHz * std::sqrt (1.0 - 1.0 / (2.0 * qLp * qLp)));
            for (double k : { 0.85, 0.92, 1.0, 1.08 }) at (lpHz * k);
        }
        if (hpOn)
        {
            if (qHp > 0.7072) at (hpHz / std::sqrt (1.0 - 1.0 / (2.0 * qHp * qHp)));
            for (double k : { 0.92, 1.0, 1.08, 1.18 }) at (hpHz * k);
        }
        if (bellPk) for (double k : { 0.9, 1.0, 1.1 }) at (bellHz * k);
        // Feinsuche um den besten Punkt (Goldener Schnitt auf log. Frequenz): breite Spitzen bei kleiner Resonanz
        // liegen sonst zwischen den Stuetzstellen
        double bestF = 10.0, bestM = -1.0;
        auto track = [&] (double f) { const double m = magnitude (std::min (std::max (f, 10.0), maxF)); if (m > bestM) { bestM = m; bestF = f; } return m; };
        if (lpOn) for (double k : { 0.5, 0.65, 0.75, 0.85, 0.92, 1.0 }) track (lpHz * k);
        if (hpOn) for (double k : { 1.0, 1.08, 1.18, 1.35, 1.6, 2.0 }) track (hpHz * k);
        if (bellPk) for (double k : { 0.8, 0.9, 1.0, 1.1, 1.25 }) track (bellHz * k);
        double lo = std::log (bestF / 1.3), hi = std::log (bestF * 1.3);
        constexpr double g = 0.6180339887498949;
        for (int it = 0; it < 12; ++it)
        {
            const double a1 = hi - g * (hi - lo), a2 = lo + g * (hi - lo);
            if (track (std::exp (a1)) > track (std::exp (a2))) hi = a2; else lo = a1;
        }
        return std::max (best, bestM);
    }
};

inline LoopFilterSpec loopFilterOf (const EchoSettings& s, double sr)
{
    const double maxF = 0.45 * sr;
    return { sr, s.hpOn, std::min (std::max ((double) s.hpHz, 20.0), maxF), 0.7071 + s.hpRes * s.hpRes * 4.0, s.hpBoost,
             s.lpOn, std::min (std::max ((double) s.lpHz, 100.0), maxF), 0.7071 + s.res * s.res * 4.0, s.lpBoost,
             s.bellOn ? s.bellGain : 0.0f, std::min ((double) s.bellHz, maxF), (double) s.bellQ };
}

// Groesste Verstaerkung der Filterkette in der Schleife (Raster)
inline double loopFilterPeakGain (const EchoSettings& s, double sr) { return loopFilterOf (s, sr).peakOnGrid(); }

// Ausklingzeit in Sekunden, bis die Echos um 100 dB abgeklungen sind (fuer getTailLengthSeconds).
// Unendlich, wenn eine Einheit eine Schleifenverstaerkung >= 1 hat (der tanh haelt sie dann am Schwingen),
// eingefroren ist oder Noise laeuft (Noise klingt auch ohne Eingang). Ketten werden konservativ aufsummiert.
// Mit STABLE gleicht die Einheit die Filterspitze aus: die Schleife ist dann hoechstens so laut wie das Feedback.
inline double tailSeconds (const EngineSettings& s, double bpm, double sr)
{
    constexpr double inf = std::numeric_limits<double>::infinity();
    if (s.noise.on) return inf;
    double total = 0.0;
    for (const auto& e : s.echo)
    {
        if (! e.on) continue;
        if (e.freeze || s.freezeAll) return inf;
        double msL, msR;
        delayTimesMs (e, bpm, msL, msR);
        const double delay = std::max (msL, msR) * 0.001 + 0.01 * std::max (e.lfoAmt, e.envAmt) * e.delayDepth + 0.001 * e.wow;
        const double peak = loopFilterPeakGain (e, sr);
        const double loop = std::abs ((double) e.feedback * (double) s.feedbackScale) * (e.stableFb ? std::min (peak, 1.0) : peak);
        if (loop >= 1.0) return inf;
        const double repeats = loop > 1.0e-9 ? std::log (1.0e-5) / std::log (loop) : 0.0;
        total += delay * (1.0 + repeats);
    }
    return total + 0.1;   // Raum: Laufzeit, Reflexionen, Filter
}
} // namespace qe
