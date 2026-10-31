#pragma once
#include <vector>
#include <cmath>
#include <cstring>
#include "DspTypes.h"
#include "HrtfData.h"

namespace qe
{
// Gemessene HRTF (MIT KEMAR, Gardner und Martin 1994, siehe tools/make_hrtf.py): diffusfeld-entzerrt, minimalphasig,
// 64 Taps bei 44,1 kHz, Raster Hoehe -40..90 (10 Grad) x Azimut 0..355 (5 Grad, 0 = vorne, 90 = rechts).
// Die Laufzeitdifferenz der Ohren steckt nicht in den Filtern (die liefert der Spatializer).
//
// HrtfPair: ein Filterpaar (linkes/rechtes Ohr) fuer eine Quelle. prepare rechnet die Filter auf die Abtastrate um
// (gefensterter Sinc); setDirection interpoliert zwischen den vier Nachbarrichtungen und blendet beim Wechsel ueber
// einen 16er-Block zum neuen Filter (kein Zippern). Kein Speicher im Audio-Thread.
class HrtfPair
{
public:
    static constexpr int kEl = 14, kAz = 72, kTaps = 64;
    static constexpr double kRate = 44100.0;

    // Daten als float (einmal kopiert: die eingebetteten Bytes sind nicht sicher ausgerichtet). Erster Aufruf in prepare.
    static const float* grid()
    {
        static const std::vector<float> data = []
        {
            std::vector<float> d ((size_t) (kEl * kAz * 2 * kTaps));
            jassert (HrtfData::kemar_df_mp64_binSize == (int) (d.size() * sizeof (float)));
            std::memcpy (d.data(), HrtfData::kemar_df_mp64_bin, std::min (d.size() * sizeof (float), (size_t) HrtfData::kemar_df_mp64_binSize));
            return d;
        }();
        return data.data();
    }

    void prepare (double sampleRate)
    {
        grid();   // Daten laden (nicht im Audio-Thread)
        const double ratio = kRate / sampleRate;   // Quell-Samples je Ziel-Sample
        n = std::max (16, (int) std::lround (kTaps / ratio));
        const double fc = std::min (1.0, 1.0 / ratio);   // beim Herunterrechnen tiefpassen
        resample.assign ((size_t) (n * kTaps), 0.0f);
        for (int i = 0; i < n; ++i)
            for (int k = 0; k < kTaps; ++k)
            {
                const double x = (double) i * ratio - (double) k;
                if (std::abs (x) >= 8.0) continue;
                const double s = x == 0.0 ? 1.0 : std::sin (3.141592653589793 * fc * x) / (3.141592653589793 * fc * x);
                const double win = 0.5 + 0.5 * std::cos (3.141592653589793 * x / 8.0);
                resample[(size_t) (i * kTaps + k)] = (float) (ratio * fc * s * win);
            }
        for (auto* v : { &cur, &prev }) v->assign ((size_t) (2 * n), 0.0f);
        buf.assign ((size_t) (4 * n), 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        w = 0;
        fadePos = fadeLen = 0;
        lastAz = lastEl = lastLat = -1000.0f;
    }

    // Richtung in Grad (Azimut beliebig, Hoehe -40..90); lateral 0..1: 0 = am Kopf (Filter wie vorne)
    void setDirection (float azDeg, float elDeg, float lateral)
    {
        if (std::abs (azDeg - lastAz) < 0.25f && std::abs (elDeg - lastEl) < 0.25f && std::abs (lateral - lastLat) < 0.005f)
            return;
        const bool first = lastAz < -999.0f;
        lastAz = azDeg; lastEl = elDeg; lastLat = lateral;

        float h44[2][kTaps];
        interpolate (azDeg, elDeg, h44);
        if (lateral < 1.0f)
        {
            float f44[2][kTaps];
            interpolate (0.0f, elDeg, f44);
            for (int e = 0; e < 2; ++e)
                for (int k = 0; k < kTaps; ++k) h44[e][k] = f44[e][k] + (h44[e][k] - f44[e][k]) * lateral;
        }
        std::swap (cur, prev);
        for (int e = 0; e < 2; ++e)
            for (int i = 0; i < n; ++i)
            {
                float acc = 0.0f;
                const float* m = resample.data() + i * kTaps;
                for (int k = 0; k < kTaps; ++k) acc += m[k] * h44[e][k];
                cur[(size_t) (e * n + i)] = acc;
            }
        if (first) { prev = cur; fadePos = fadeLen = 0; }
        else { fadePos = 0; fadeLen = 16; }
    }

    // l und r: Signal des linken bzw. rechten Ohrs (vor dem Filter); danach gefiltert
    void process (float& l, float& r)
    {
        float* b = buf.data();
        b[w] = b[w + n] = l;
        b[2 * n + w] = b[3 * n + w] = r;
        const float* xl = b + w + n;       // neuestes Sample, rueckwaerts lesen
        const float* xr = b + 2 * n + w + n;
        float yl = 0.0f, yr = 0.0f;
        const float* hl = cur.data();
        const float* hr = cur.data() + n;
        for (int k = 0; k < n; ++k) { yl += hl[k] * xl[-k]; yr += hr[k] * xr[-k]; }
        if (fadePos < fadeLen)
        {
            float pl = 0.0f, pr = 0.0f;
            const float* ql = prev.data();
            const float* qr = prev.data() + n;
            for (int k = 0; k < n; ++k) { pl += ql[k] * xl[-k]; pr += qr[k] * xr[-k]; }
            const float a = (float) (++fadePos) / (float) fadeLen;
            yl = pl + (yl - pl) * a;
            yr = pr + (yr - pr) * a;
        }
        w = w + 1 == n ? 0 : w + 1;
        l = yl;
        r = yr;
    }

private:
    // bilinear zwischen den vier Nachbarn des Rasters (44,1 kHz)
    static void interpolate (float azDeg, float elDeg, float out[2][kTaps])
    {
        const float* g = grid();
        float az = std::fmod (azDeg, 360.0f);
        if (az < 0.0f) az += 360.0f;
        const float fe = (std::min (std::max (elDeg, -40.0f), 90.0f) + 40.0f) / 10.0f;
        const int e0 = std::min ((int) fe, kEl - 2);
        const float te = fe - (float) e0;
        const float fa = az / 5.0f;
        const int a0 = (int) fa % kAz, a1 = (a0 + 1) % kAz;
        const float ta = fa - std::floor (fa);
        auto at = [g] (int e, int a, int ear, int k) { return g[((e * kAz + a) * 2 + ear) * kTaps + k]; };
        for (int ear = 0; ear < 2; ++ear)
            for (int k = 0; k < kTaps; ++k)
            {
                const float lo = at (e0, a0, ear, k) + (at (e0, a1, ear, k) - at (e0, a0, ear, k)) * ta;
                const float hi = at (e0 + 1, a0, ear, k) + (at (e0 + 1, a1, ear, k) - at (e0 + 1, a0, ear, k)) * ta;
                out[ear][k] = lo + (hi - lo) * te;
            }
    }

    int n = 16, w = 0, fadePos = 0, fadeLen = 0;
    std::vector<float> resample, cur, prev, buf;
    float lastAz = -1000.0f, lastEl = -1000.0f, lastLat = -1000.0f;
};
} // namespace qe
