#pragma once
#include <complex>
#include <limits>
#include "DspTypes.h"

namespace qe
{
// Groesste Verstaerkung der Filterkette in der Feedback-Schleife einer Einheit (HP 2. + 1. Ordnung, LP 2. + 1. Ordnung,
// Glocken an den Grenzfrequenzen), ausgewertet auf einem logarithmischen Frequenzraster. Formeln wie im Plugin:
// TPT-Filter = bilineare Transformation mit Vorverzerrung auf fc, Glocke nach RBJ (PeakEq).
inline double loopFilterPeakGain (const EchoSettings& s, double sr)
{
    if (! s.hpOn && ! s.lpOn) return 1.0;
    const double maxF = 0.45 * sr;
    const double hpHz = std::min (std::max ((double) s.hpHz, 20.0), maxF), lpHz = std::min (std::max ((double) s.lpHz, 100.0), maxF);
    const double qHp = 0.7071 + s.hpRes * s.hpRes * 4.0, qLp = 0.7071 + s.res * s.res * 4.0;
    auto peak = [sr] (double fc, double q, double gainDb, std::complex<double> z)
    {
        PeakEq p;
        p.set (sr, (float) fc, (float) q, (float) gainDb);
        const auto zi = 1.0 / z;
        return ((double) p.b0 + (double) p.b1 * zi + (double) p.b2 * zi * zi) / (1.0 + (double) p.a1 * zi + (double) p.a2 * zi * zi);
    };
    double best = 0.0;
    constexpr int kPoints = 240;
    for (int k = 0; k <= kPoints; ++k)
    {
        const double f = 20.0 * std::pow (maxF / 20.0, (double) k / kPoints);
        const double w = 2.0 * 3.141592653589793 * f / sr;
        const std::complex<double> z = std::polar (1.0, w), j (0.0, 1.0);
        std::complex<double> h = 1.0;
        if (s.hpOn)
        {
            const double v = std::tan (w / 2.0) / std::tan (3.141592653589793 * hpHz / sr);
            h *= (-v * v / (1.0 - v * v + j * v / qHp)) * (j * v / (1.0 + j * v));
            if (s.hpBoost > kBoostOffDb) h *= peak (hpHz, qHp, s.hpBoost, z);
        }
        if (s.lpOn)
        {
            const double v = std::tan (w / 2.0) / std::tan (3.141592653589793 * lpHz / sr);
            h *= (1.0 / (1.0 - v * v + j * v / qLp)) * (1.0 / (1.0 + j * v));
            if (s.lpBoost > kBoostOffDb) h *= peak (lpHz, qLp, s.lpBoost, z);
        }
        best = std::max (best, std::abs (h));
    }
    return best;
}

// Ausklingzeit in Sekunden, bis die Echos um 100 dB abgeklungen sind (fuer getTailLengthSeconds).
// Unendlich, wenn eine Einheit eine Schleifenverstaerkung >= 1 hat (der tanh haelt sie dann am Schwingen),
// eingefroren ist oder Noise laeuft (Noise klingt auch ohne Eingang). Ketten werden konservativ aufsummiert.
inline double tailSeconds (const EngineSettings& s, double bpm, double sr)
{
    constexpr double inf = std::numeric_limits<double>::infinity();
    if (s.noise.on) return inf;
    double total = 0.0;
    for (const auto& e : s.echo)
    {
        if (! e.on) continue;
        if (e.freeze) return inf;
        double msL, msR;
        delayTimesMs (e, bpm, msL, msR);
        const double delay = std::max (msL, msR) * 0.001 + (e.x4 ? 0.04 : 0.01) * e.delayDepth;
        const double loop = std::abs ((double) e.feedback * (double) s.feedbackScale) * loopFilterPeakGain (e, sr);
        if (loop >= 1.0) return inf;
        const double repeats = loop > 1.0e-9 ? std::log (1.0e-5) / std::log (loop) : 0.0;
        total += delay * (1.0 + repeats);
    }
    return total + 0.1;   // Raum: Laufzeit, Reflexionen, Filter
}
} // namespace qe
