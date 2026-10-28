#pragma once
#include <juce_dsp/juce_dsp.h>
#include "DelayLine.h"
#include "DspTypes.h"

namespace qe
{
struct SpatialOptions
{
    bool distanceDelay = false;
    bool earlyReflections = true;
    bool binaural = false;
    float doppler = 1.0f;
};

// Psychoakustische Raumplatzierung einer Einheit (Stereo, in-place).
// Geometrie: Hoerer in der Mitte des Pads, X rechts, Y vorne/hinten, Radius = Entfernung
// (d = 1 + 3 * Radius Meter, -6 dB pro Verdopplung), Z = Hoehe.
//
// Stereo-Modus: Equal-Power-Pan nach X, ITD bis 0,6 ms, Hoehenverlust am abgewandten Ohr;
//   hinten klingt dezent dumpfer und leiser (Stereo kann hinten nicht wirklich abbilden).
// Binaural-Modus (Kopfhoerer): Kugel-Kopf-Modell: ITD nach Woodworth, Kopfschatten nach
//   Brown/Duda, Ohrmuschel-Daempfung hinten, Hoehenfaerbung.
// Beide: Luftabsorption, Stereobreite sinkt mit Distanz, optionale Laufzeit d/343 (mit Doppler),
//   3 sehr kurze fruehe Reflexionen, Z-Peak/Notch 6..10 kHz. Kein Hall.
class Spatializer
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (float* l, float* r, int n, const PosSettings& p, const SpatialOptions& o);

private:
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float tick (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };
    struct Shadow   // 1. Ordnung: Kopfschatten (Brown/Duda)
    {
        float b0 = 1, b1 = 0, a1 = 0, x1 = 0, y1 = 0;
        float tick (float x)
        {
            const float y = b0 * x + b1 * x1 - a1 * y1;
            x1 = x; y1 = y;
            return y;
        }
    };

    void setPeak (Biquad& b, float freq, float gainDb, float q) const;

    double sr = 44100.0;
    DelayLine dl[2], er;
    juce::SmoothedValue<float> xSm, ySm, zSm, distOnSm, binSm;
    float lpState[2] = { 0, 0 };
    float erLp = 0.0f;
    float dRef = 1.0f;
    Biquad peak[2];
    Shadow shadow[2];
    float gPrev[2] = { 1.0f, 1.0f };
    bool initialised = false;
};
} // namespace qe
