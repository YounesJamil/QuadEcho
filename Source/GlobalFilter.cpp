#include "GlobalFilter.h"

namespace qe
{
void GlobalFilter::prepare (double sampleRate)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, 512, 1 };
    for (auto& i : insts)
        for (int c = 0; c < 2; ++c)
        {
            i.hp[(size_t) c].prepare (spec); i.lp[(size_t) c].prepare (spec);
            i.hp1[(size_t) c].prepare (spec); i.lp1[(size_t) c].prepare (spec);
            i.hp[(size_t) c].setType (juce::dsp::StateVariableTPTFilterType::highpass);
            i.lp[(size_t) c].setType (juce::dsp::StateVariableTPTFilterType::lowpass);
            i.hp1[(size_t) c].setType (juce::dsp::FirstOrderTPTFilterType::highpass);
            i.lp1[(size_t) c].setType (juce::dsp::FirstOrderTPTFilterType::lowpass);
        }
    reset();
}

void GlobalFilter::reset()
{
    for (int i = 0; i < kInstances; ++i) resetInstance (i);
    phase = 0.0;
    modS = { { 0.0f, 0.0f } };
    qS = { { -1.0f, -1.0f } };
    boostS = { { 0.0f, 0.0f } };
    cur = GlobalFilterSettings();
    for (auto& f : fmBlocks) f.fill (1.0f);
    numSegs = 0;
    framePos = 0;
    prevLastSeg = -1;
}

void GlobalFilter::resetInstance (int inst)
{
    auto& i = insts[(size_t) inst];
    for (auto& f : i.hp) f.reset();
    for (auto& f : i.lp) f.reset();
    for (auto& f : i.hp1) f.reset();
    for (auto& f : i.lp1) f.reset();
    for (auto& f : i.peakHp) f.reset();
    for (auto& f : i.peakLp) f.reset();
}

void GlobalFilter::beginChunk (const GlobalFilterSettings& s, double bpm, int n, double ppq)
{
    cur = s;
    const double hz = s.lfoSync ? (bpm / 60.0) / divBeatsOf (s.lfoDiv) : (double) s.rateHz;
    if (s.lfoSync && ppq >= 0.0)   // phase gilt fuer den Anfang des naechsten 16er-Blocks: dort ausrichten
    {
        const int toFrame = (16 - framePos) % 16;
        const double want = (ppq + (double) toFrame / sr * bpm / 60.0) / divBeatsOf (s.lfoDiv);
        if (std::abs (want - phase) > 1.0e-6) phase = want;
    }
    const float offR = s.phaseDeg / 360.0f;   // rechter Kanal versetzt
    const float qTarget[2] = { 0.7071f + s.hpRes * s.hpRes * 4.0f, 0.7071f + s.res * s.res * 4.0f };
    const float qCoef = 1.0f - std::exp (-16.0f / (0.03f * (float) sr));   // Resonanz geglaettet, ca. 30 ms
    numSegs = 0;
    int pos = 0;
    if (framePos != 0)   // angefangener Block: Werte des letzten Blocks (Index numSegs-1 des vorigen Chunks) weiter verwenden
    {
        const int last = std::max (prevLastSeg, 0);
        for (int c = 0; c < 2; ++c)
        {
            fmBlocks[(size_t) c][0] = fmBlocks[(size_t) c][(size_t) last];
            qBlocks[(size_t) c][0] = qBlocks[(size_t) c][(size_t) last];
            boostBlocks[(size_t) c][0] = boostBlocks[(size_t) c][(size_t) last];
        }
        segStart[0] = 0;
        segLen[0] = std::min (16 - framePos, n);
        numSegs = 1;
        pos = segLen[0];
    }
    for (; pos < n && numSegs < (int) segStart.size(); pos += 16)
    {
        const int b = numSegs++;
        segStart[(size_t) b] = pos;
        segLen[(size_t) b] = std::min (16, n - pos);
        for (int k = 0; k < 2; ++k)
        {
            qS[(size_t) k] = qS[(size_t) k] < 0.0f ? qTarget[k] : qS[(size_t) k] + (qTarget[k] - qS[(size_t) k]) * qCoef;
            qBlocks[(size_t) k][(size_t) b] = qS[(size_t) k];
            const float boostTarget = k == 0 ? s.hpBoost : s.lpBoost;
            boostS[(size_t) k] += (boostTarget - boostS[(size_t) k]) * qCoef;
            boostBlocks[(size_t) k][(size_t) b] = boostS[(size_t) k];
        }
        phase += hz / sr * 16.0;
        for (int c = 0; c < 2; ++c)
        {
            const float target = s.lfoOn ? lfoWave (s.wave, phase + (c == 1 ? (double) offR : 0.0)) : 0.0f;
            modS[(size_t) c] += (target - modS[(size_t) c]) * 0.25f;
            fmBlocks[(size_t) c][(size_t) b] = std::exp2 (modS[(size_t) c] * s.depth * 2.0f);
        }
    }
    prevLastSeg = numSegs - 1;
    framePos = (framePos + n) % 16;
}

void GlobalFilter::process (int inst, float* l, float* r, int n)
{
    if (! cur.hpOn && ! cur.lpOn) { resetInstance (inst); return; }
    auto& f = insts[(size_t) inst];
    const float maxF = 0.45f * (float) sr;
    float* ch[2] = { l, r };

    for (int b = 0; b < numSegs; ++b)
    {
        const int start = segStart[(size_t) b];
        const int len = std::min (segLen[(size_t) b], n - start);
        if (len <= 0) break;
        const auto bi = (size_t) b;
        for (int c = 0; c < 2; ++c)
        {
            const auto cc = (size_t) c;
            const float fm = fmBlocks[cc][bi], qHp = qBlocks[0][bi], qLp = qBlocks[1][bi];
            const bool hpPeak = cur.hpOn && boostBlocks[0][bi] > kBoostOffDb, lpPeak = cur.lpOn && boostBlocks[1][bi] > kBoostOffDb;
            auto& hp = f.hp[cc];
            auto& lp = f.lp[cc];
            auto& hp1 = f.hp1[cc];
            auto& lp1 = f.lp1[cc];
            auto& peakHp = f.peakHp[cc];
            auto& peakLp = f.peakLp[cc];
            if (cur.hpOn)
            {
                const float fc = std::min (std::max (cur.hpHz * fm, 20.0f), maxF);
                hp.setCutoffFrequency (fc); hp.setResonance (qHp); hp1.setCutoffFrequency (fc);
                if (hpPeak) peakHp.set (sr, fc, qHp, boostBlocks[0][bi]); else peakHp.reset();
            }
            else { hp.reset(); hp1.reset(); peakHp.reset(); }
            if (cur.lpOn)
            {
                const float fc = std::min (std::max (cur.lpHz * fm, 100.0f), maxF);
                lp.setCutoffFrequency (fc); lp.setResonance (qLp); lp1.setCutoffFrequency (fc);
                if (lpPeak) peakLp.set (sr, fc, qLp, boostBlocks[1][bi]); else peakLp.reset();
            }
            else { lp.reset(); lp1.reset(); peakLp.reset(); }

            float* x = ch[c] + start;
            for (int j = 0; j < len; ++j)
            {
                float a = x[j];
                if (cur.hpOn) a = hp1.processSample (0, hp.processSample (0, a));
                if (cur.lpOn) a = lp1.processSample (0, lp.processSample (0, a));
                if (hpPeak) a = peakHp.process (0, a);
                if (lpPeak) a = peakLp.process (0, a);
                x[j] = a;
            }
        }
    }
}
} // namespace qe
