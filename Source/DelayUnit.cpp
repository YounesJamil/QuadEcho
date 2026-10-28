#include "DelayUnit.h"

namespace qe
{
namespace
{
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr int kSub = 16;

inline float waveform (int wave, double t) { return lfoWave (wave, t); }
} // namespace

void DelayUnit::prepare (double sampleRate)
{
    sr = sampleRate;
    // 4 s + Modulation (max 40 ms) + Interpolationsreserve
    capacity = (int) std::ceil (kMaxDelaySeconds * sr) + (int) std::ceil (0.05 * sr) + 16;
    for (auto& l : line) { l.dl.prepare (capacity); l.first.prepare (capacity); }
    capacity = line[0].dl.capacity();

    juce::dsp::ProcessSpec spec { sampleRate, 512, 2 };
    hp.prepare (spec); lp.prepare (spec); hp1.prepare (spec); lp1.prepare (spec);
    hp.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    lp.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    hp1.setType (juce::dsp::FirstOrderTPTFilterType::highpass);
    lp1.setType (juce::dsp::FirstOrderTPTFilterType::lowpass);

    for (auto* sm : { &inGainSm, &fbSm, &mixSm, &outSm, &widthSm, &driveSm }) sm->reset (sampleRate, 0.03);
    depthSm.reset (sampleRate, 0.05);
    fDepthSm.reset (sampleRate, 0.05);
    freezeSm.reset (sampleRate, 0.05);

    rpCoef  = 1.0f - std::exp (-1.0f / (0.2f * (float) sr));
    qCoef   = 1.0f - std::exp (-(float) kSub / (0.03f * (float) sr));   // Resonanz, pro Teilblock
    modCoef = 1.0f - std::exp (-1.0f / (0.004f * (float) sr));
    envAtk  = 1.0f - std::exp (-1.0f / (0.005f * (float) sr));
    envRel  = 1.0f - std::exp (-1.0f / (0.15f * (float) sr));
    reset();
}

void DelayUnit::reset()
{
    for (auto& l : line) { l.dl.clear(); l.first.clear(); }
    clearPos = capacity;
    resetState();
}

void DelayUnit::beginDeferredReset()
{
    clearPos = 0;
    resetState();
}

void DelayUnit::clearSome (int samplesPerLine)
{
    if (clearPos >= capacity) return;
    const int count = std::min (samplesPerLine, capacity - clearPos);
    for (auto& l : line) { l.dl.clearRange (clearPos, count); l.first.clearRange (clearPos, count); }
    clearPos += count;
}

void DelayUnit::resetState()
{
    for (auto& l : line) { l.fading = false; l.init = false; }
    hp.reset(); lp.reset(); hp1.reset(); lp1.reset(); peakHp.reset(); peakLp.reset();
    lfoPos = 0.0; modSm[0] = modSm[1] = fModSm = 0.0f; env = envMapped = 0.0f;
    lastTransition = -1;
    framePos = 0;
    initialised = false;
}

float DelayUnit::readLine (Line& l, float modSamples, float target, int transition, float& first, bool needFirst, bool frozen)
{
    const float maxD = (float) (capacity - 8);
    auto clampD = [maxD] (float d) { return std::min (std::max (d, 2.0f), maxD); };
    // liest die Hauptleitung und die Leitung ohne Feedback an derselben Stelle
    auto read2 = [&l, &clampD, needFirst] (float d, float& f)
    {
        d = clampD (d);
        f = needFirst ? l.first.readHermite (d) : 0.0f;
        return l.dl.readHermite (d);
    };

    if (! l.init) { l.baseA = l.baseB = l.cur = target; l.init = true; }

    // Freeze: ganzzahlige Verzoegerung (die Interpolation wuerde den Loop bei jeder Runde dunkler machen),
    // per Ueberblendung erreicht. Zeitaenderungen warten, bis Freeze wieder aus ist.
    if (frozen)
    {
        target = std::round (l.fading ? l.baseB : l.baseA);
        if (transition == Repitch) transition = Fade;
        l.cur = target;
    }

    if (transition == Repitch)
    {
        l.cur += (target - l.cur) * rpCoef;
        l.baseA = l.cur;
        l.fading = false;
        return read2 (l.baseA + modSamples, first);
    }

    if (! l.fading && std::abs (target - l.baseA) > (frozen ? 1.0e-3f : 0.5f))
    {
        l.fading = true;
        l.fadePos = 0;
        l.fadeLen = std::max (1, (int) (sr * (transition == Fade ? 0.06 : 0.003)));
        l.baseB = target;
    }

    if (! l.fading)
        return read2 (l.baseA + modSamples, first);

    const float g = (float) l.fadePos / (float) l.fadeLen;
    const float ga = std::cos (g * 1.57079632679f), gb = std::sin (g * 1.57079632679f);
    float fa, fb;
    const float ya = read2 (l.baseA + modSamples, fa);
    const float yb = read2 (l.baseB + modSamples, fb);
    first = ga * fa + gb * fb;
    if (++l.fadePos >= l.fadeLen) { l.baseA = l.baseB; l.fading = false; }
    return ga * ya + gb * yb;
}

void DelayUnit::process (const float* inL, const float* inR, float* outL, float* outR,
                         int n, const EchoSettings& s, double bpm, float* wetOutL, float* wetOutR,
                         float* firstOutL, float* firstOutR,
                         const float* origInL, const float* origInR, float* origOutL, float* origOutR)
{
    const float fs = (float) sr;
    const int trans = std::min (std::max (s.transition, 0), 2);
    if (trans != lastTransition)
    {
        for (auto& l : line) { if (l.fading) l.baseA = l.baseB; l.fading = false; l.cur = l.baseA; }
        lastTransition = trans;
    }

    // ---- Zielwerte ----
    const auto divBeats = [] (int i) { return divBeatsOf (i); };
    double msL, msR;
    delayTimesMs (s, bpm, msL, msR);
    const float target[2] = { (float) (msL * sr / 1000.0), (float) (msR * sr / 1000.0) };

    const double lfoHz = s.lfoSync ? (bpm / 60.0) / divBeats (s.lfoDiv) : (double) s.rateHz;
    const double lfoInc = lfoHz / sr;
    const float phaseOff = s.phaseDeg / 360.0f;
    const float fPhaseOff = s.filterPhaseDeg / 360.0f;
    const float lfoGate = s.lfoOn ? 1.0f : 0.0f;
    const float depthSamples = s.delayDepth * 0.010f * (s.x4 ? 4.0f : 1.0f) * fs;
    const float fbSign = s.fbInvert ? -1.0f : 1.0f;
    const float driveAmt = std::min (std::max (s.inGainDb / 24.0f, 0.0f), 1.0f);   // Input-Gain ueber 0 dB saettigt analog
    const float qLpTarget = 0.7071f + s.res * s.res * 4.0f, qHpTarget = 0.7071f + s.hpRes * s.hpRes * 4.0f;
    const int mode = s.mode;
    const bool frozen = s.freeze;
    const float depthTarget = frozen ? 0.0f : depthSamples;   // keine Modulation im gehaltenen Loop

    if (! initialised)
    {
        inGainSm.setCurrentAndTargetValue (dbToGain (s.inGainDb));
        fbSm.setCurrentAndTargetValue (s.feedback * fbSign);
        mixSm.setCurrentAndTargetValue (s.mix);
        outSm.setCurrentAndTargetValue (outputGain (s));
        widthSm.setCurrentAndTargetValue (s.width);
        driveSm.setCurrentAndTargetValue (driveAmt);
        depthSm.setCurrentAndTargetValue (depthTarget);
        fDepthSm.setCurrentAndTargetValue (s.filterDepth);
        freezeSm.setCurrentAndTargetValue (frozen ? 1.0f : 0.0f);
        qSm[0] = qHpTarget; qSm[1] = qLpTarget;
        boostSm[0] = s.hpBoost; boostSm[1] = s.lpBoost;
        initialised = true;
    }
    inGainSm.setTargetValue (dbToGain (s.inGainDb));
    fbSm.setTargetValue (s.feedback * fbSign);
    mixSm.setTargetValue (s.mix);
    outSm.setTargetValue (outputGain (s));
    widthSm.setTargetValue (s.width);
    driveSm.setTargetValue (driveAmt);
    depthSm.setTargetValue (depthTarget);
    fDepthSm.setTargetValue (s.filterDepth);
    freezeSm.setTargetValue (frozen ? 1.0f : 0.0f);

    const bool needFirst = firstOutL != nullptr;

    // Steuerwerte (Filter, Huellkurve, Saettigung) je 16 Samples, an der absoluten Sample-Position ausgerichtet:
    // ein Aufruf, der mitten in einem Teilblock beginnt, fuehrt ihn mit den Werten von dessen Anfang zu Ende.
    // So haengt der Ausgang nicht von der Blockgroesse des Hosts ab.
    for (int start = 0; start < n;)
    {
        const int len = std::min (kSub - framePos, n - start);
        if (framePos == 0)
        {
        // Envelope -> 0..1 (dB-Skala), pro Teilblock
        const float envDb = 20.0f * std::log10 (env + 1e-6f);
        envMapped = std::min (std::max ((envDb + 48.0f) / 48.0f, 0.0f), 1.0f);

        frameSat = makeSatCoefs (driveSm.getCurrentValue());   // einmal pro Teilblock

        // Filter-Cutoff inkl. Modulation, Resonanz geglaettet (hohe Guete springt sonst hoerbar)
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
        {
            const float modF = std::min (std::max (fModSm, -1.0f), 1.0f);
            const float fm = std::exp2 (modF * fDepthSm.getCurrentValue() * 2.0f);
            const float maxF = 0.45f * fs;
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
            modPub.store (0.5f * (modSm[0] + modSm[1]), std::memory_order_relaxed);
            envPub.store (envMapped, std::memory_order_relaxed);
        }
        }
        const SatCoefs& sat = frameSat;
        const bool hpOn = frameHpOn, lpOn = frameLpOn, hpPeak = frameHpPeak, lpPeak = frameLpPeak;

        for (int j = 0; j < len; ++j)
        {
            const int i = start + j;
            const float l = inL[i], r = inR[i];

            const float a = std::max (std::abs (l), std::abs (r));
            env += (a > env ? envAtk : envRel) * (a - env);

            // LFO (gemeinsame Phase, R mit Offset)
            const float lfo0 = lfoGate * waveform (s.wave, lfoPos);
            const float lfo1 = lfoGate * waveform (s.wave, lfoPos + phaseOff);
            const float lfoF = lfoGate * waveform (s.wave, lfoPos + fPhaseOff);
            if (j == 0 && framePos == 0)
            {
                lfoPub.store (0.5f * (lfo0 + lfo1), std::memory_order_relaxed);
                posPub.store (lfoPos, std::memory_order_relaxed);
                rateHzPub.store ((float) lfoHz, std::memory_order_relaxed);
            }
            lfoPos += lfoInc;
            const float m0 = lfo0 * (1.0f - s.envMix) + envMapped * s.envMix;
            const float m1 = lfo1 * (1.0f - s.envMix) + envMapped * s.envMix;
            const float mF = lfoF * (1.0f - s.envMix) + envMapped * s.envMix;
            fModSm += (mF - fModSm) * modCoef;
            modSm[0] += (m0 - modSm[0]) * modCoef;
            modSm[1] += (m1 - modSm[1]) * modCoef;

            const float inG = inGainSm.getNextValue();
            const float fb = fbSm.getNextValue();
            const float outG = outSm.getNextValue();
            const float wid = widthSm.getNextValue();
            const float lvl = mixSm.getNextValue();
            fDepthSm.skip (1);
            const float depthNow = depthSm.getNextValue();
            const float fz = freezeSm.getNextValue();

            // Eingang je Kanalmodus (Freeze schliesst ihn)
            float x0, x1;
            if (mode == PingPong)     { x0 = 0.5f * (l + r) * inG; x1 = 0.0f; }
            else if (mode == MidSide) { x0 = 0.5f * (l + r) * inG; x1 = 0.5f * (l - r) * inG; }
            else                      { x0 = l * inG; x1 = r * inG; }

            x0 = analogSat (x0, sat) * (1.0f - fz);
            x1 = analogSat (x1, sat) * (1.0f - fz);

            float fe0 = 0.0f, fe1 = 0.0f;
            const float y0 = readLine (line[0], modSm[0] * depthNow, target[0], trans, fe0, needFirst, frozen);
            const float y1 = readLine (line[1], modSm[1] * depthNow, target[1], trans, fe1, needFirst, frozen);

            // Feedbackpfad: Filter -> Soft-Saturation; Freeze blendet auf den unveraenderten Loop mit Feedback 1
            const float r0 = (mode == PingPong) ? y1 : y0;
            const float r1 = (mode == PingPong) ? y0 : y1;
            float f0 = r0, f1 = r1;
            if (hpOn) { f0 = hp1.processSample (0, hp.processSample (0, f0)); f1 = hp1.processSample (1, hp.processSample (1, f1)); }
            if (lpOn) { f0 = lp1.processSample (0, lp.processSample (0, f0)); f1 = lp1.processSample (1, lp.processSample (1, f1)); }
            if (hpPeak) { f0 = peakHp.process (0, f0); f1 = peakHp.process (1, f1); }
            if (lpPeak) { f0 = peakLp.process (0, f0); f1 = peakLp.process (1, f1); }
            f0 = loopSat (f0, sat);
            f1 = loopSat (f1, sat);
            float fbNow = fb;
            if (fz > 0.0f)
            {
                f0 += (r0 - f0) * fz;
                f1 += (r1 - f1) * fz;
                fbNow += (fbSign - fb) * fz;
            }
            float w0 = x0 + fbNow * f0, w1 = x1 + fbNow * f1;
            if (std::abs (w0) < 1e-18f) w0 = 0.0f;
            if (std::abs (w1) < 1e-18f) w1 = 0.0f;
            if (! std::isfinite (w0) || ! std::isfinite (w1)) { w0 = w1 = 0.0f; poisoned = true; }   // Schleife nie vergiften
            line[0].dl.push (w0);
            line[1].dl.push (w1);
            line[0].first.push (x0);
            line[1].first.push (x1);

            // Ausgang dekodieren
            float wl, wr;
            if (mode == MidSide) { wl = y0 + y1; wr = y0 - y1; }
            else                 { wl = y0;      wr = y1; }
            const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * wid;
            wl = mid + side;
            wr = mid - side;

            // Durchlaufendes Signal bleibt (wie bei einem Effektpedal), die Echos kommen mit dem Echo-Pegel dazu
            const float dl = s.inGainOnDry ? analogSat (l * inG, sat) : l;
            const float dr = s.inGainOnDry ? analogSat (r * inG, sat) : r;
            outL[i] = (dl + wl * lvl) * outG;
            outR[i] = (dr + wr * lvl) * outG;
            if (wetOutL != nullptr) { wetOutL[i] = wl * lvl * outG; wetOutR[i] = wr * lvl * outG; }
            if (firstOutL != nullptr)   // nur das erste Echo (ohne Feedback), gleich dekodiert wie das Wet-Signal
            {
                float fl, fr;
                if (mode == MidSide) { fl = fe0 + fe1; fr = fe0 - fe1; }
                else                 { fl = fe0; fr = fe1; }
                const float fmid = 0.5f * (fl + fr), fside = 0.5f * (fl - fr) * wid;
                firstOutL[i] = (fmid + fside) * lvl * outG;
                firstOutR[i] = (fmid - fside) * lvl * outG;
            }
            if (origOutL != nullptr)   // reiner Original-Anteil, durch denselben Dry-Weg
            {
                const float ol = origInL[i], orr = origInR[i];
                origOutL[i] = (s.inGainOnDry ? analogSat (ol * inG, sat) : ol) * outG;
                origOutR[i] = (s.inGainOnDry ? analogSat (orr * inG, sat) : orr) * outG;
            }
        }
        start += len;
        framePos += len;
        if (framePos == kSub)
        {
            driveSm.skip (kSub);
            framePos = 0;
        }
    }
    // Nicht-endlicher Zustand (Huellkurve, Modulation, Filter) z. B. durch einen NaN-Parameter: Einheit neu aufsetzen
    if (poisoned || ! std::isfinite (env) || ! std::isfinite (modSm[0] + modSm[1] + fModSm))
    {
        reset();
        poisoned = false;
    }
}
} // namespace qe
