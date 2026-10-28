#pragma once
#include <array>
#include <cmath>

namespace qe
{
// 2-fach-Oversampling fuer die Saettigungen (AUDIT.md C3): linearphasiger Halbband-FIR, 47 Taps, Kaiser-Fenster
// (beta 8). Durchlass bis 0,42 fs flach (< 0,001 dB), Sperrbereich ab 0,58 fs unter -80 dB.
// Hoch- und Runtertasten zusammen verzoegern um genau kLatency Samples (ganzzahlig, wichtig fuer Freeze).
// Polyphasen-Form: beim Hochtasten ist jeder zweite Ausgang eine reine Verzoegerung, beim Runtertasten
// traegt nur der Mittel-Tap die ungeraden Samples.
class Oversampler2x
{
public:
    static constexpr int kK = 12;                     // Taps = 4K - 1 = 47
    static constexpr int kTaps = 4 * kK - 1;
    static constexpr int kLatency = 2 * kK - 1;       // Samples der Grundrate (Hoch + Runter)

    Oversampler2x() { design(); }

    void reset()
    {
        xHist.fill (0.0f);
        evenHist.fill (0.0f);
        oddHist.fill (0.0f);
        xPos = evenPos = oddPos = 0;
    }

    // Ein Sample der Grundrate -> zwei Samples der doppelten Rate (zeitlich: a vor b)
    void up (float x, float& a, float& b)
    {
        xPos = (xPos + kHist - 1) % kHist;
        xHist[(size_t) xPos] = xHist[(size_t) (xPos + kHist)] = x;   // doppelt abgelegt: lineares Lesen ohne Umbruch
        const float* h = &xHist[(size_t) xPos];                       // h[j] = x[n - j]
        a = dot (upTaps.data(), h);
        b = h[kK - 1];
    }

    // Zwei Samples der doppelten Rate -> ein Sample der Grundrate
    float down (float a, float b)
    {
        evenPos = (evenPos + kHist - 1) % kHist;
        evenHist[(size_t) evenPos] = evenHist[(size_t) (evenPos + kHist)] = a;
        oddPos = (oddPos + kHist - 1) % kHist;
        oddHist[(size_t) oddPos] = oddHist[(size_t) (oddPos + kHist)] = b;
        const float* e = &evenHist[(size_t) evenPos];
        return dot (downTaps.data(), e) + 0.5f * oddHist[(size_t) (oddPos + kK)];
    }

private:
    static constexpr int kHist = 2 * kK + 1;

    // Skalarprodukt ueber 2K Taps mit vier unabhaengigen Teilsummen (eine einzige Summenkette waere durch die
    // Additionslatenz etwa viermal langsamer)
    static float dot (const float* taps, const float* x)
    {
        float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f, s3 = 0.0f;
        for (int j = 0; j < 2 * kK; j += 4)
        {
            s0 += taps[j] * x[j];
            s1 += taps[j + 1] * x[j + 1];
            s2 += taps[j + 2] * x[j + 2];
            s3 += taps[j + 3] * x[j + 3];
        }
        return (s0 + s1) + (s2 + s3);
    }

    void design()
    {
        // Kaiser-Fenster, I0 als Reihe
        auto i0 = [] (double x)
        {
            double sum = 1.0, term = 1.0;
            for (int k = 1; k < 40; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += term; }
            return sum;
        };
        constexpr double beta = 8.0, pi = 3.141592653589793;
        constexpr int c = (kTaps - 1) / 2;
        std::array<double, kTaps> h {};
        double sum = 0.0;
        for (int k = 0; k < kTaps; ++k)
        {
            const int m = k - c;
            double v = 0.0;
            if (m == 0) v = 0.5;
            else if (m % 2 != 0) v = std::sin (0.5 * pi * m) / (pi * m);   // 0.5 * sinc(m / 2)
            const double r = (double) m / c;
            v *= i0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0 (beta);
            h[(size_t) k] = v;
            sum += v;
        }
        for (int j = 0; j < 2 * kK; ++j)
        {
            const double hk = h[(size_t) (2 * j)] / sum;   // gerade k: die Seitentaps des Halbbands
            upTaps[(size_t) j] = (float) (2.0 * hk);       // Hochtasten: Null-Einfuegen halbiert die Energie
            downTaps[(size_t) j] = (float) hk;
        }
    }

    std::array<float, 2 * kK> upTaps {}, downTaps {};
    std::array<float, 2 * kHist> xHist {}, evenHist {}, oddHist {};
    int xPos = 0, evenPos = 0, oddPos = 0;
};

// Ganzzahlige Verzoegerung um Oversampler2x::kLatency (Freeze-Pfad: muss exakt bleiben)
class LatencyDelay
{
public:
    void reset() { buf.fill (0.0f); pos = 0; }
    float process (float x)
    {
        const float y = buf[(size_t) pos];
        buf[(size_t) pos] = x;
        pos = (pos + 1) % Oversampler2x::kLatency;
        return y;
    }

private:
    std::array<float, Oversampler2x::kLatency> buf {};
    int pos = 0;
};
} // namespace qe
