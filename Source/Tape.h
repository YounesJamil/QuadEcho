#pragma once
#include <complex>
#include "DspTypes.h"
#include "Oversampler.h"

namespace qe
{
// Band-Charakter fuer den Schreibweg einer Einheit (wie der Aufnahmekopf eines Bandechos): alles, was auf das Band
// geht (Eingang plus Feedback), laeuft hindurch, das erste Echo einmal, jede Wiederholung erneut.
//   Pre-Emphasis (+6 dB Hoehen) -> weiche Saettigung (x / sqrt(1 + x^2), mit 2x-Oversampling) -> De-Emphasis (exakte
//   Umkehrung: kleine Signale bleiben linear, laute Hoehen saettigen frueher) -> Bandfrequenzgang (Head Bump bei
//   100 Hz, Hoehenverlust ab 10 kHz, kein Tiefbass unter 25 Hz), auf hoechstens 0 dB normiert, damit die Schleife
//   nicht aufschaukelt und STABLE weiter stimmt.
// Zwei Kanaele; mit Oversampling verzoegert der Schreibweg um Oversampler2x::kLatency (wie die Saettigung von Clean).
class TapeStage
{
public:
    void prepare (double sampleRate, bool oversample)
    {
        sr = sampleRate;
        useOs = oversample;
        // Shelf 1. Ordnung H(s) = (G s + 1) / (s + 1) bei 3 kHz, bilinear mit Vorverzerrung; De-Emphasis = 1 / H
        constexpr double G = 2.0;
        const double K = 1.0 / std::tan (3.141592653589793 * std::min (3000.0, 0.4 * sr) / sr);
        pre = { (float) ((G * K + 1.0) / (K + 1.0)), (float) ((1.0 - G * K) / (K + 1.0)), (float) ((1.0 - K) / (K + 1.0)) };
        de = { (float) ((K + 1.0) / (G * K + 1.0)), (float) ((1.0 - K) / (G * K + 1.0)), (float) ((1.0 - G * K) / (G * K + 1.0)) };
        bump.set (sr, 100.0f, 1.0f, 2.0f);
        lpA = (float) (1.0 - std::exp (-6.283185307179586 * std::min (10000.0, 0.4 * sr) / sr));
        hpA = (float) (1.0 - std::exp (-6.283185307179586 * 25.0 / sr));
        trim = (float) (1.0 / peakGain());
        reset();
    }

    void reset()
    {
        for (auto& s : st) s = State();
        bump.reset();
        for (auto& o : os) o.reset();
    }

    float process (int ch, float x)
    {
        auto& s = st[(size_t) ch];
        const float e = pre.b0 * x + pre.b1 * s.px - pre.a1 * s.py;
        s.px = x; s.py = e;
        float y;
        if (useOs)
        {
            float a, b;
            os[(size_t) ch].up (e, a, b);
            y = os[(size_t) ch].down (sat (a), sat (b));
        }
        else y = sat (e);
        const float d = de.b0 * y + de.b1 * s.dx - de.a1 * s.dy;
        s.dx = y; s.dy = d;
        float t = bump.process (ch, d);
        s.lp += lpA * (t - s.lp);
        s.hp += hpA * (s.lp - s.hp);
        return (s.lp - s.hp) * trim;
    }

    // groesste Verstaerkung des Bandfrequenzgangs ohne Normierung (Raster, einmal in prepare)
    double peakGain() const
    {
        double best = 0.0;
        for (int k = 0; k <= 300; ++k)
        {
            const double f = 20.0 * std::pow (0.45 * sr / 20.0, k / 300.0);
            const std::complex<double> zi = std::polar (1.0, -6.283185307179586 * f / sr);
            const auto h = ((double) bump.b0 + (double) bump.b1 * zi + (double) bump.b2 * zi * zi) / (1.0 + (double) bump.a1 * zi + (double) bump.a2 * zi * zi)
                         * ((double) lpA / (1.0 - (1.0 - (double) lpA) * zi))
                         * (1.0 - (double) hpA / (1.0 - (1.0 - (double) hpA) * zi));
            best = std::max (best, std::abs (h));
        }
        return std::max (best, 1.0e-3);
    }

private:
    static float sat (float x) { return x / std::sqrt (1.0f + x * x); }   // Steigung 1 im Ursprung, weicher als tanh

    struct Shelf { float b0 = 1.0f, b1 = 0.0f, a1 = 0.0f; };
    struct State { float px = 0, py = 0, dx = 0, dy = 0, lp = 0, hp = 0; };
    double sr = 48000.0;
    bool useOs = false;
    Shelf pre, de;
    PeakEq bump;
    float lpA = 1.0f, hpA = 0.0f, trim = 1.0f;
    std::array<State, 2> st {};
    std::array<Oversampler2x, 2> os;
};
} // namespace qe
