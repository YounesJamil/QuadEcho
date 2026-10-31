#include "Spatializer.h"

namespace qe
{
namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr int kSub = 16;
constexpr float kMaxItdSeconds = 0.0006f;
constexpr float kSpeedOfSound = 343.0f;
constexpr float kHeadRadius = 0.0875f;

inline float lerp (float a, float b, float t) { return a + (b - a) * t; }

// Radius im Pad -> Entfernung in Metern. Bis zum 2. Ring (2/3) bleibt die Quelle nah und praesent
// (hoechstens -3 dB, kaum Luftdaempfung), danach greift die Entfernung steil:
// aeusserer Ring 4 m (-12 dB), Ecken (Radius 1.41) ca. 5,6 m (-15 dB)
inline float distanceMeters (float radius)
{
    constexpr float rNear = 2.0f / 3.0f, dNear = 1.41421356f, dRing = 4.0f;
    if (radius <= rNear) return 1.0f + (dNear - 1.0f) * radius / rNear;
    if (radius <= 1.0f)  return dNear + (dRing - dNear) * (radius - rNear) / (1.0f - rNear);
    return dRing + 3.86f * (radius - 1.0f);
}
inline float clampf (float v, float lo, float hi) { return std::min (std::max (v, lo), hi); }

// Kopfschatten-Faktor alphaW (Brown/Duda): 2.0 = Ohr zeigt zur Quelle, 0.1 = tiefer Schatten
inline float shadowAlpha (float angleToEar)
{
    return 1.05f + 0.95f * std::cos (std::min (1.2f * angleToEar, kPi));
}
} // namespace

void Spatializer::prepare (double sampleRate)
{
    sr = sampleRate;
    const int cap = (int) std::ceil (0.06 * sr);
    dl[0].prepare (cap);
    dl[1].prepare (cap);
    er.prepare (cap);
    for (auto* sm : { &xSm, &ySm, &zSm }) sm->reset (sampleRate, 0.05);
    distOnSm.reset (sampleRate, 0.1);
    binSm.reset (sampleRate, 0.08);
    hrtf.prepare (sampleRate);
    reset();
}

void Spatializer::reset()
{
    dl[0].clear(); dl[1].clear(); er.clear();
    lpState[0] = lpState[1] = 0.0f;
    erLp = 0.0f;
    peak[0] = Biquad(); peak[1] = Biquad();
    shadow[0] = Shadow(); shadow[1] = Shadow();
    tone[0] = Biquad(); tone[1] = Biquad();
    toneSm = toneSet = 0.0f;
    hrtf.reset();
    gPrev[0] = gPrev[1] = 1.0f;
    framePos = 0;
    initialised = false;
}

void Spatializer::setPeak (Biquad& b, float freq, float gainDb, float q) const
{
    const float A = std::pow (10.0f, gainDb / 40.0f);
    const float w0 = 2.0f * kPi * std::min (freq, 0.45f * (float) sr) / (float) sr;
    const float alpha = std::sin (w0) / (2.0f * q);
    const float cw = std::cos (w0);
    const float a0 = 1.0f + alpha / A;
    b.b0 = (1.0f + alpha * A) / a0;
    b.b1 = (-2.0f * cw) / a0;
    b.b2 = (1.0f - alpha * A) / a0;
    b.a1 = (-2.0f * cw) / a0;
    b.a2 = (1.0f - alpha / A) / a0;
}

// RBJ-High-Shelf mit Steilheit S = 1
void Spatializer::setHighShelf (Biquad& b, float freq, float gainDb) const
{
    const float A = std::pow (10.0f, gainDb / 40.0f);
    const float w0 = 2.0f * kPi * std::min (freq, 0.45f * (float) sr) / (float) sr;
    const float cw = std::cos (w0), alpha = std::sin (w0) / 2.0f * 1.41421356f, sA = 2.0f * std::sqrt (A) * alpha;
    const float a0 = (A + 1.0f) - (A - 1.0f) * cw + sA;
    b.b0 = A * ((A + 1.0f) + (A - 1.0f) * cw + sA) / a0;
    b.b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cw) / a0;
    b.b2 = A * ((A + 1.0f) + (A - 1.0f) * cw - sA) / a0;
    b.a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cw) / a0;
    b.a2 = ((A + 1.0f) - (A - 1.0f) * cw - sA) / a0;
}

void Spatializer::process (float* l, float* r, int n, const PosSettings& p, const SpatialOptions& o)
{
    const float fs = (float) sr;
    if (! initialised)
    {
        xSm.setCurrentAndTargetValue (clampf (p.x, -1.0f, 1.0f));
        ySm.setCurrentAndTargetValue (clampf (p.y, -1.0f, 1.0f));
        zSm.setCurrentAndTargetValue (clampf (p.z, -1.0f, 1.0f));
        distOnSm.setCurrentAndTargetValue (o.distanceDelay ? 1.0f : 0.0f);
        binSm.setCurrentAndTargetValue (o.binaural ? 1.0f : 0.0f);
        dRef = -1.0f;
        initialised = true;
    }
    xSm.setTargetValue (clampf (p.x, -1.0f, 1.0f));
    ySm.setTargetValue (clampf (p.y, -1.0f, 1.0f));
    zSm.setTargetValue (clampf (p.z, -1.0f, 1.0f));
    distOnSm.setTargetValue (o.distanceDelay ? 1.0f : 0.0f);
    binSm.setTargetValue (o.binaural ? 1.0f : 0.0f);
    const float dopplerAmt = clampf (o.doppler, 0.0f, 2.0f);
    const float maxDelay = (float) dl[0].capacity() - 8.0f;

    // Steuerwerte je 16 Samples an der absoluten Sample-Position: ein Aufruf, der mitten in einem Teilblock beginnt,
    // fuehrt ihn mit dessen Werten zu Ende (unabhaengig von der Blockgroesse des Hosts)
    for (int start = 0; start < n;)
    {
        const int len = std::min (kSub - framePos, n - start);
        if (framePos == 0)
        {
        const float x = xSm.getNextValue(); xSm.skip (kSub - 1);
        const float y = ySm.getNextValue(); ySm.skip (kSub - 1);
        const float z = zSm.getNextValue(); zSm.skip (kSub - 1);
        const float dOn = distOnSm.getNextValue(); distOnSm.skip (kSub - 1);
        const float bm = binSm.getNextValue(); binSm.skip (kSub - 1);

        // ---- Geometrie: Radius -> Distanz in Metern (Mindestabstand 1 m) ----
        const float radius = std::min (std::sqrt (x * x + y * y), 1.41421356f);
        // Stereo (Buehne): der Hoerer sitzt vor der Buehne, Y = +1 ist die Buehnenkante (nah), -1 die Rueckwand.
        // Die vordere Haelfte bleibt praesent, hinten bis 2,8 m (-9 dB), schmaler, dumpfer, mehr Raum. X ist nur Panorama.
        const float depth = clampf (0.5f * (1.0f - y), 0.0f, 1.0f);
        const float dStage = 1.0f + 1.82f * depth * depth * depth;
        const float d = lerp (dStage, distanceMeters (radius), bm);   // Binaural: Abstand zur Mitte (Hoerer in der Mitte)
        const float gDist = 1.0f / d;
        fGDist = gDist;
        const float azimuth = radius > 1e-4f ? std::atan2 (x, y) : 0.0f;       // 0 = vorne, +90 Grad = rechts
        const float elevation = z * 0.7853982f;                                 // +-45 Grad
        const float rearW = std::max (0.0f, -std::cos (azimuth)) * std::min (1.0f, radius * 3.0f);   // nur Binaural

        // Stereo breite sinkt mit Distanz
        fWDist = lerp (1.0f - 0.45f * depth * depth, 1.0f / (1.0f + 0.2f * (d - 1.0f)), bm);

        // Luftabsorption
        const float fcAir = 20000.0f / (1.0f + 0.9f * (d - 1.0f));

        // ---- Stereo-Modus ----
        const float pan = clampf (x, -1.0f, 1.0f);
        const float theta = (pan + 1.0f) * 0.25f * kPi;
        const float gS[2] = { gDist * 1.41421356f * std::cos (theta),
                              gDist * 1.41421356f * std::sin (theta) };
        const float itdS = std::abs (pan) * kMaxItdSeconds * fs;
        const float dS[2] = { pan > 0.0f ? itdS : 0.0f, pan < 0.0f ? itdS : 0.0f };
        const float shadowS = std::pow (0.3f, std::abs (pan));
        const float fcS[2] = { fcAir * (pan > 0.0f ? shadowS : 1.0f), fcAir * (pan < 0.0f ? shadowS : 1.0f) };
        const float awS[2] = { 1.0f, 1.0f };

        // ---- Binaural-Modus (Kugel-Kopf) ----
        const float lateralFade = std::min (1.0f, radius / 0.12f);   // in Kopfnaehe keine Seitenwirkung
        const float sinA = clampf (std::sin (azimuth) * std::cos (elevation) * lateralFade, -1.0f, 1.0f);
        const float alpha = std::asin (sinA);
        const float itdB = (kHeadRadius / kSpeedOfSound) * (std::abs (alpha) + std::abs (sinA)) * fs;
        const float dB[2] = { sinA > 0.0f ? itdB : 0.0f, sinA < 0.0f ? itdB : 0.0f };
        // Pegel- und Klangunterschiede der Ohren, Hinten und Hoehe kommen aus der gemessenen HRTF (unten im Signalweg)
        const float awB[2] = { 1.0f, 1.0f };
        const float gB[2] = { gDist, gDist };
        const float fcRearB = fcAir;
        (void) rearW;
        if (bm > 0.0f || o.binaural)
            hrtf.setDirection (azimuth * 57.29578f, elevation * 57.29578f, lateralFade);
        fBm = bm;
        // binTone: Gain je Teilblock geglaettet; bei 0 dB laeuft kein Filter (alte Presets bitgleich)
        const float toneT = clampf (o.binToneDb, -6.0f, 6.0f);
        toneSm += (toneT - toneSm) * 0.05f;
        if (std::abs (toneT - toneSm) < 1.0e-3f) toneSm = toneT;
        if (toneSm == 0.0f) { if (toneSet != 0.0f) { tone[0] = Biquad(); tone[1] = Biquad(); toneSet = 0.0f; } }
        else if (toneSm != toneSet) { setHighShelf (tone[0], 4000.0f, toneSm); setHighShelf (tone[1], 4000.0f, toneSm); toneSet = toneSm; }

        // ---- Mischung der Modi (weich beim Umschalten) ----
        float aw[2], fc[2];
        for (int c = 0; c < 2; ++c)
        {
            fG[c] = lerp (gS[c], gB[c], bm);
            fDly[c] = lerp (dS[c], dB[c], bm);
            aw[c] = lerp (awS[c], awB[c], bm);
            fc[c] = std::exp (lerp (std::log (fcS[c]), std::log (fcRearB), bm));
        }

        // Entfernungs-Laufzeit mit Doppler: langsam nachgefuehrte Referenz, Aenderungen skaliert
        if (dRef < 0.0f) dRef = d;
        dRef += (d - dRef) * (1.0f - std::exp (-(float) kSub / (2.0f * fs)));
        const float meters = std::max (0.0f, dRef + dopplerAmt * (d - dRef));
        fDistDelay = dOn * (meters / kSpeedOfSound) * fs;

        for (int c = 0; c < 2; ++c)
            fLpA[c] = std::exp (-2.0f * kPi * std::min (fc[c], 0.45f * fs) / fs);

        // Kopfschatten-Filter (1. Ordnung, bilinear)
        {
            const float w0 = kSpeedOfSound / kHeadRadius, k = 2.0f * fs;
            for (int c = 0; c < 2; ++c)
            {
                const float den = k + 2.0f * w0;
                shadow[c].b0 = (aw[c] * k + 2.0f * w0) / den;
                shadow[c].b1 = (2.0f * w0 - aw[c] * k) / den;
                shadow[c].a1 = (2.0f * w0 - k) / den;
            }
        }

        // Hoehe: Peak (z>0) bzw. Senke (z<0) zwischen 6 und 10 kHz
        const float fz = 6000.0f + 4000.0f * (0.5f * z + 0.5f);
        setPeak (peak[0], fz, 5.0f * z * (1.0f - bm), 1.5f);   // Binaural: die HRTF bildet die Hoehe selbst ab
        setPeak (peak[1], fz, 5.0f * z * (1.0f - bm), 1.5f);

        // Fruehe Reflexionen (Anteil steigt mit Distanz)
        fErAmt = o.earlyReflections ? lerp (0.4f * depth * depth * depth, 0.35f * clampf ((d - 1.0f) / 3.0f, 0.0f, 1.0f), bm) : 0.0f;
        }
        const float erA = std::exp (-2.0f * kPi * 5000.0f / fs);
        const float erTap[3] = { 0.0053f * fs, 0.0089f * fs, 0.0131f * fs };
        const float erGain[3] = { 1.0f, 0.75f, 0.55f };
        const float* g = fG;
        const float* dly = fDly;
        const float* lpA = fLpA;
        const float wDist = fWDist, gDist = fGDist, distDelay = fDistDelay, erAmt = fErAmt;

        for (int j = 0; j < len; ++j)
        {
            const int i = start + j;
            const float t = (float) (framePos + j + 1) / (float) kSub;
            const float gl = gPrev[0] + (g[0] - gPrev[0]) * t;
            const float gr = gPrev[1] + (g[1] - gPrev[1]) * t;

            const float m = 0.5f * (l[i] + r[i]);
            const float s = 0.5f * (l[i] - r[i]) * wDist;
            const float l2 = (m + s) * gl;
            const float r2 = (m - s) * gr;

            dl[0].push (l2);
            dl[1].push (r2);
            float l3 = dl[0].readLinear (clampf (distDelay + dly[0], 0.0f, maxDelay));
            float r3 = dl[1].readLinear (clampf (distDelay + dly[1], 0.0f, maxDelay));

            l3 = shadow[0].tick (l3);
            r3 = shadow[1].tick (r3);
            if (fBm > 0.0f)   // gemessene HRTF, beim Umschalten weich ueberblendet
            {
                float hl = l3, hr = r3;
                hrtf.process (hl, hr);
                if (toneSet != 0.0f) { hl = tone[0].tick (hl); hr = tone[1].tick (hr); }
                l3 += (hl - l3) * fBm;
                r3 += (hr - r3) * fBm;
            }

            lpState[0] = l3 * (1.0f - lpA[0]) + lpState[0] * lpA[0];
            lpState[1] = r3 * (1.0f - lpA[1]) + lpState[1] * lpA[1];
            l3 = peak[0].tick (lpState[0]);
            r3 = peak[1].tick (lpState[1]);

            // Fruehe Reflexionen aus dem Mono-Signal
            erLp = (m * gDist) * (1.0f - erA) + erLp * erA;
            er.push (erLp);
            if (erAmt > 0.0f)
            {
                const float t1 = er.readLinear (erTap[0]), t2 = er.readLinear (erTap[1]),
                            t3 = er.readLinear (erTap[2]);
                l3 += erAmt * (erGain[0] * t1 - 0.7f * erGain[2] * t3);
                r3 += erAmt * (erGain[1] * t2 + 0.7f * erGain[2] * t3);
            }

            l[i] = l3;
            r[i] = r3;
        }
        start += len;
        framePos += len;
        if (framePos == kSub)
        {
            gPrev[0] = g[0];
            gPrev[1] = g[1];
            framePos = 0;
        }
    }
}
} // namespace qe
