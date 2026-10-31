#include "NoiseSource.h"

namespace qe
{
namespace
{
constexpr int kSub = 16;

inline float rnd (uint32_t& s)   // xorshift32, -1 .. 1
{
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return (float) (int32_t) s * (1.0f / 2147483648.0f);
}
} // namespace

float NoiseSource::colored (Color& c, float w, float cl) const
{
    c.p0 = 0.99765f * c.p0 + w * 0.0990460f;
    c.p1 = 0.96300f * c.p1 + w * 0.2965164f;
    c.p2 = 0.57000f * c.p2 + w * 1.0526913f;
    const float pink = (c.p0 + c.p1 + c.p2 + w * 0.1848f) * pinkNorm;
    c.br = 0.998f * c.br + 0.05f * w;
    const float brown = c.br * brownNorm;
    if (cl < 0.5f) { const float t = cl * 2.0f; return brown + (pink - brown) * t; }
    const float t = (cl - 0.5f) * 2.0f;
    return pink + (w - pink) * t;
}

void NoiseSource::prepare (double sampleRate)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, 512, 2 };
    hp.prepare (spec); lp.prepare (spec); hp1.prepare (spec); lp1.prepare (spec);
    hp.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    lp.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    hp1.setType (juce::dsp::FirstOrderTPTFilterType::highpass);
    lp1.setType (juce::dsp::FirstOrderTPTFilterType::lowpass);
    levelSm.reset (sampleRate, 0.03);
    panAmtSm.reset (sampleRate, 0.05);
    crkDecay = std::exp (-1.0f / (0.0015f * (float) sampleRate));
    envAtk = 1.0f - std::exp (-1.0f / (0.005f * (float) sr));
    envRel = 1.0f - std::exp (-1.0f / (0.15f * (float) sr));

    // Normalisierung: rosa und braunes Rauschen auf gleichen Effektivwert wie weisses bringen
    pinkNorm = brownNorm = 1.0f;
    uint32_t s = 0xabcdef01u;
    Color c; double pe = 0, be = 0; int count = 0;
    for (int i = 0; i < 96000; ++i)
    {
        const float w = rnd (s) * 1.7320508f;
        c.p0 = 0.99765f * c.p0 + w * 0.0990460f; c.p1 = 0.96300f * c.p1 + w * 0.2965164f; c.p2 = 0.57000f * c.p2 + w * 1.0526913f;
        c.br = 0.998f * c.br + 0.05f * w;
        if (i > 8000) { const float p = c.p0 + c.p1 + c.p2 + w * 0.1848f; pe += (double) p * p; be += (double) c.br * c.br; ++count; }
    }
    pinkNorm = (float) (1.0 / std::sqrt (pe / count));
    brownNorm = (float) (1.0 / std::sqrt (be / count));
    reset();
}

void NoiseSource::reset()
{
    for (auto& c : col) c = Color();
    crk[0] = crk[1] = 0.0f;
    hp.reset(); lp.reset(); hp1.reset(); lp1.reset(); peakHp.reset(); peakLp.reset(); peakBell.reset();
    phase = 0.0;
    panPhase = 0.0;
    framePos = 0;
    std::copy (std::begin (kSeeds), std::end (kSeeds), std::begin (seed));   // gleicher Zufall nach jedem reset()
    fModS = env = envMapped = 0.0f;
    qSm[0] = qSm[1] = -1.0f;
    initialised = false;
}

void NoiseSource::process (float* outL, float* outR, int n, const NoiseSettings& s, double bpm,
                           const float* inL, const float* inR)
{
    const float amp = 0.125f * s.level * s.level;   // 100 % = deutlich (Effektivwert ca. -18 dBFS), 30 % = Hintergrund
    if (! initialised) { levelSm.setCurrentAndTargetValue (s.on ? amp : 0.0f); panAmtSm.setCurrentAndTargetValue (s.panAmt); initialised = true; }
    levelSm.setTargetValue (s.on ? amp : 0.0f);
    panAmtSm.setTargetValue (s.panAmt);

    if (! s.on && ! isActive())
    {
        std::fill (outL, outL + n, 0.0f);
        std::fill (outR, outR + n, 0.0f);
        framePos = (framePos + n) % kSub;   // Raster laeuft weiter
        hp.reset(); lp.reset(); hp1.reset(); lp1.reset(); peakHp.reset(); peakLp.reset(); peakBell.reset();
        return;
    }

    const float fs = (float) sr;
    const double lfoHz = s.lfoSync ? (bpm / 60.0) / divBeatsOf (s.lfoDiv) : (double) s.rateHz;
    if (s.lfoSync && songPpq >= 0.0)   // phase gilt fuer den Anfang des naechsten Teilblocks: dort ausrichten
    {
        const int toFrame = (kSub - framePos) % kSub;
        const double want = (songPpq + (double) toFrame / sr * bpm / 60.0) / divBeatsOf (s.lfoDiv);
        if (std::abs (want - phase) > 1.0e-6) phase = want;
    }
    const float qLpTarget = 0.7071f + s.res * s.res * 4.0f, qHpTarget = 0.7071f + s.hpRes * s.hpRes * 4.0f;
    const float qCoef = 1.0f - std::exp (-(float) kSub / (0.03f * (float) sr));   // Resonanz geglaettet, ca. 30 ms
    if (qSm[0] < 0.0f) { qSm[0] = qHpTarget; qSm[1] = qLpTarget; }
    const float wIndep = std::sqrt (std::min (std::max (s.width, 0.0f), 1.0f));
    const float wCommon = std::sqrt (1.0f - std::min (std::max (s.width, 0.0f), 1.0f));
    const float crkRate = (4.0f + 60.0f * s.crackle * s.crackle) / fs;     // Wahrscheinlichkeit pro Sample
    const float crkAmt = s.crackle * 1.6f;
    const float maxF = 0.45f * fs;
    // Auto-Pan: eigener Sinus-LFO, pro Sample (Equal-Power, Mitte = unveraendert)
    const double panDivBeats = divBeatsOf (s.panDiv);
    if (s.panSync && songPpq >= 0.0)
    {
        const double want = songPpq / panDivBeats;
        if (std::abs (want - panPhase) > 1.0e-6) panPhase = want;
    }
    const double panInc = (s.panSync ? (bpm / 60.0) / panDivBeats : (double) s.panRateHz) / sr;

    // Steuerwerte je 16 Samples an der absoluten Sample-Position (wie DelayUnit): unabhaengig von der Blockgroesse
    for (int start = 0; start < n;)
    {
        const int len = std::min (kSub - framePos, n - start);
        if (framePos == 0)
        {
        // Hüllkurve des Eingangs (dB-Skala) und LFO pro Teilblock
        const float envDb = 20.0f * std::log10 (env + 1e-6f);
        envMapped = std::min (std::max ((envDb + 48.0f) / 48.0f, 0.0f), 1.0f);
        phase += lfoHz / sr * (double) kSub;
        const double pl = phase + s.phaseDeg / 360.0;
        const float lfoF = s.lfoOn ? lfoWave (s.wave, phase) : 0.0f;
        const float lfoLevelU = s.lfoOn ? (lfoWave (s.wave, pl) + 1.0f) * 0.5f : 1.0f;
        const float mF = lfoF * (1.0f - s.envMix) + envMapped * s.envMix;
        fModS += (mF - fModS) * 0.25f;
        const float u = lfoLevelU * (1.0f - s.envMix) + envMapped * s.envMix;
        frameTrem = 1.0f - s.depthL * (1.0f - u);
        const float fm = std::exp2 (fModS * s.depthF * 2.0f);
        qSm[0] += (qHpTarget - qSm[0]) * qCoef;
        qSm[1] += (qLpTarget - qSm[1]) * qCoef;
        const float qHp = qSm[0], qLp = qSm[1];
        boostSm[0] += (s.hpBoost - boostSm[0]) * qCoef;
        boostSm[1] += (s.lpBoost - boostSm[1]) * qCoef;
        frameHpOn = s.hpOn;
        frameLpOn = s.lpOn;
        frameHpPeak = s.hpOn && boostSm[0] > kBoostOffDb;
        frameLpPeak = s.lpOn && boostSm[1] > kBoostOffDb;
        const bool hpPeak = frameHpPeak, lpPeak = frameLpPeak;
        bellSm += ((s.bellOn ? s.bellGain : 0.0f) - bellSm) * qCoef;   // Bell-Band, wie in der DelayUnit
        frameBell = std::abs (bellSm) > kBoostOffDb;
        const float bellF = std::min (s.bellMod ? std::max (s.bellHz * fm, 20.0f) : s.bellHz, maxF);
        if (frameBell) peakBell.set (sr, bellF, s.bellQ, bellSm); else peakBell.reset();

        if (s.hpOn)
        {
            const float fc = std::min (std::max (s.hpHz * fm, 20.0f), maxF);
            hp.setCutoffFrequency (fc); hp.setResonance (qHp); hp1.setCutoffFrequency (fc);
            if (hpPeak) peakHp.set (sr, fc, qHp, boostSm[0]); else peakHp.reset();
        }
        else { hp.reset(); hp1.reset(); peakHp.reset(); }
        if (s.lpOn)
        {
            const float fc = std::min (std::max (s.lpHz * fm, 100.0f), maxF);
            lp.setCutoffFrequency (fc); lp.setResonance (qLp); lp1.setCutoffFrequency (fc);
            if (lpPeak) peakLp.set (sr, fc, qLp, boostSm[1]); else peakLp.reset();
        }
        else { lp.reset(); lp1.reset(); peakLp.reset(); }
        }
        const float trem = frameTrem;
        const bool hpOn = frameHpOn, lpOn = frameLpOn, hpPeak = frameHpPeak, lpPeak = frameLpPeak, bell = frameBell;

        for (int j = 0; j < len; ++j)
        {
            const int i = start + j;
            const float a = std::max (std::abs (inL[i]), std::abs (inR[i]));
            env += (a > env ? envAtk : envRel) * (a - env);

            const float wC = rnd (seed[0]) * 1.7320508f, wL = rnd (seed[1]) * 1.7320508f, wR = rnd (seed[2]) * 1.7320508f;
            const float cC = colored (col[0], wC, s.color), cL = colored (col[1], wL, s.color), cR = colored (col[2], wR, s.color);
            float nl = wIndep * cL + wCommon * cC;
            float nr = wIndep * cR + wCommon * cC;

            // Knistern: seltene, kurze Knackser (exponentiell abklingend), pro Kanal unabhaengig
            for (int c = 0; c < 2; ++c)
            {
                crk[c] *= crkDecay;
                if (0.5f * (rnd (seed[3]) + 1.0f) < crkRate)
                {
                    const float r = 0.5f * (rnd (seed[3]) + 1.0f);
                    crk[c] = (rnd (seed[3]) < 0.0f ? -1.0f : 1.0f) * (0.35f + 0.65f * r * r);
                }
            }
            nl += crkAmt * crk[0];
            nr += crkAmt * crk[1];

            if (hpOn) { nl = hp1.processSample (0, hp.processSample (0, nl)); nr = hp1.processSample (1, hp.processSample (1, nr)); }
            if (lpOn) { nl = lp1.processSample (0, lp.processSample (0, nl)); nr = lp1.processSample (1, lp.processSample (1, nr)); }
            if (hpPeak) { nl = peakHp.process (0, nl); nr = peakHp.process (1, nr); }
            if (lpPeak) { nl = peakLp.process (0, nl); nr = peakLp.process (1, nr); }
            if (bell) { nl = peakBell.process (0, nl); nr = peakBell.process (1, nr); }

            const float g = levelSm.getNextValue() * trem;
            const float pa = panAmtSm.getNextValue();
            if (pa > 0.0f)
            {
                const float th = (pa * (float) std::sin (6.283185307179586 * (panPhase - std::floor (panPhase))) + 1.0f) * 0.78539816f;
                nl *= 1.41421356f * std::cos (th);
                nr *= 1.41421356f * std::sin (th);
            }
            panPhase += panInc;
            outL[i] = nl * g;
            outR[i] = nr * g;
        }
        start += len;
        framePos = (framePos + len) % kSub;
    }
    posPub.store (phase, std::memory_order_relaxed);
    ratePub.store ((float) lfoHz, std::memory_order_relaxed);
    envPub.store (envMapped, std::memory_order_relaxed);
    panPosPub.store (panPhase, std::memory_order_relaxed);
    panRatePub.store ((float) (panInc * sr), std::memory_order_relaxed);
}
} // namespace qe
