#include "DelayUnit.h"
#include "Tail.h"

namespace qe
{
namespace
{
constexpr int kSub = 16;

inline double waveform (int wave, double t) { return lfoWaveD (wave, t); }
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

    for (auto* sm : { &inGainSm, &fbSm, &mixSm, &outSm, &widthSm, &driveSm, &kneeSm }) sm->reset (sampleRate, 0.03);
    depthSm.reset (sampleRate, 0.05);
    fDepthSm.reset (sampleRate, 0.05);
    freezeSm.reset (sampleRate, 0.05);

    useOs = sr < 88200.0;   // ab 88,2 kHz landen die staerksten Spiegelfrequenzen ohnehin ueber dem Hoerbereich
    osLatency = useOs ? (double) Oversampler2x::kLatency : 0.0;
    rpCoef  = 1.0 - std::exp (-1.0 / (0.2 * sr));
    qCoef   = 1.0f - std::exp (-(float) kSub / (0.03f * (float) sr));   // Resonanz, pro Teilblock
    modCoef = 1.0 - std::exp (-1.0 / (0.004 * sr));
    envAtk  = 1.0 - std::exp (-1.0 / (0.005 * sr));
    envRel  = 1.0 - std::exp (-1.0 / (0.15 * sr));
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
    lfoPos = 0.0; modSm[0] = modSm[1] = fModSm = 0.0; env = 0.0; envMapped = 0.0f;
    lastTransition = -1;
    stableComp = stableTarget = 1.0f;
    stableKey = {};
    for (auto& o : osX) o.reset();
    for (auto& o : osF) o.reset();
    for (auto& d : rDelay) d.reset();
    for (auto& d : xDelay) d.reset();
    xOsWarm = -1;
    framePos = 0;
    initialised = false;
}

float DelayUnit::readLine (Line& l, double modSamples, double target, int transition, float& first, bool needFirst, bool frozen)
{
    const double maxD = (double) (capacity - 8);
    auto clampD = [maxD] (double d) { return std::min (std::max (d, 2.0), maxD); };
    // liest die Hauptleitung und die Leitung ohne Feedback an derselben Stelle
    const double lat = osLatency;
    auto read2 = [&l, &clampD, needFirst, lat] (double d, float& f)
    {
        d = clampD (d - lat);   // Oversampling verzoegert den Schleifeneingang um lat: entsprechend frueher lesen
        f = needFirst ? l.first.readHermite (d) : 0.0f;
        return l.dl.readHermite (d);
    };

    if (! l.init) { l.baseA = l.baseB = target; l.cur = target; l.init = true; }

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
        if (std::abs (target - l.cur) < 1.0e-6) l.cur = target;   // am Ziel einrasten
        l.baseA = l.cur;
        l.fading = false;
        return read2 (l.baseA + modSamples, first);
    }

    if (! l.fading && std::abs (target - l.baseA) > (frozen ? 1.0e-3 : 0.5))
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
    const double target[2] = { msL * sr / 1000.0, msR * sr / 1000.0 };

    const double lfoHz = s.lfoSync ? (bpm / 60.0) / divBeats (s.lfoDiv) : (double) s.rateHz;
    if (s.lfoSync && songPpq >= 0.0)   // Phase an der Songposition ausrichten (im normalen Lauf stimmt sie schon)
    {
        const double want = songPpq / divBeats (s.lfoDiv);
        if (std::abs (want - lfoPos) > 1.0e-6) lfoPos = want;
    }
    const double lfoInc = lfoHz / sr;
    const double phaseOff = s.phaseDeg / 360.0;
    const double fPhaseOff = s.filterPhaseDeg / 360.0;
    const double depthSamples = (double) s.delayDepth * 0.010 * (s.x4 ? 4.0 : 1.0) * sr;
    const float fbSign = s.fbInvert ? -1.0f : 1.0f;
    const float driveAmt = std::min (std::max (s.inGainDb / 24.0f, 0.0f), 1.0f);   // Input-Gain ueber 0 dB saettigt analog
    const float qLpTarget = 0.7071f + s.res * s.res * 4.0f, qHpTarget = 0.7071f + s.hpRes * s.hpRes * 4.0f;
    const int mode = s.mode;
    const bool frozen = s.freeze;
    const double depthTarget = frozen ? 0.0 : depthSamples;   // keine Modulation im gehaltenen Loop

    if (! initialised)
    {
        inGainSm.setCurrentAndTargetValue (dbToGain (s.inGainDb));
        fbSm.setCurrentAndTargetValue (s.feedback * fbSign);
        mixSm.setCurrentAndTargetValue (s.mix);
        outSm.setCurrentAndTargetValue (outputGain (s));
        widthSm.setCurrentAndTargetValue (s.width);
        driveSm.setCurrentAndTargetValue (driveAmt);
        kneeSm.setCurrentAndTargetValue (loopKneeOf (s.inGainDb));
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
    kneeSm.setTargetValue (loopKneeOf (s.inGainDb));
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
        const double envDb = 20.0 * std::log10 (env + 1e-6);
        envMapped = (float) std::min (std::max ((envDb + 48.0) / 48.0, 0.0), 1.0);

        frameSat = makeSatCoefs (driveSm.getCurrentValue(), kneeSm.getCurrentValue());   // einmal pro Teilblock

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
            const float modF = (float) std::min (std::max (fModSm, -1.0), 1.0);
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

            // STABLE: Verstaerkung der Filterkette an ihrer Spitze ausgleichen, damit keine Frequenz pro Runde lauter
            // wird als das Feedback (ohne STABLE darf die Schleife an der Resonanz aufschaukeln)
            float compTarget = 1.0f;
            if (s.stableFb && (s.hpOn || s.lpOn))
            {
                const double hpFc = std::min (std::max ((double) s.hpHz * fm, 20.0), (double) maxF);
                const double lpFc = std::min (std::max ((double) s.lpHz * fm, 100.0), (double) maxF);
                const std::array<double, 7> key { { (s.hpOn ? 1.0 : 0.0) + (s.lpOn ? 2.0 : 0.0), hpFc, lpFc, qHp, qLp, boostSm[0], boostSm[1] } };
                if (key != stableKey)
                {
                    const LoopFilterSpec spec (sr, s.hpOn, hpFc, qHp, boostSm[0], s.lpOn, lpFc, qLp, boostSm[1]);
                    stableTarget = (float) (1.0 / std::max (1.0, spec.peakFast()));
                    stableKey = key;
                }
                compTarget = stableTarget;
            }
            stableComp += (compTarget - stableComp) * qCoef;
            if (std::abs (compTarget - stableComp) < 1.0e-6f) stableComp = compTarget;
            modPub.store ((float) (0.5 * (modSm[0] + modSm[1])), std::memory_order_relaxed);
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
            // LFO aus: nichts berechnen (0 * Welle ergibt ohnehin 0)
            const double lfo0 = s.lfoOn ? waveform (s.wave, lfoPos) : 0.0;
            const double lfo1 = s.lfoOn ? waveform (s.wave, lfoPos + phaseOff) : 0.0;
            const double lfoF = s.lfoOn ? waveform (s.wave, lfoPos + fPhaseOff) : 0.0;
            if (j == 0 && framePos == 0)
            {
                lfoPub.store ((float) (0.5 * (lfo0 + lfo1)), std::memory_order_relaxed);
                posPub.store (lfoPos, std::memory_order_relaxed);
                rateHzPub.store ((float) lfoHz, std::memory_order_relaxed);
            }
            lfoPos += lfoInc;
            const double em = s.envMix, envPart = (double) envMapped * em;
            const double m0 = lfo0 * (1.0 - em) + envPart;
            const double m1 = lfo1 * (1.0 - em) + envPart;
            const double mF = lfoF * (1.0 - em) + envPart;
            fModSm += (mF - fModSm) * modCoef;
            modSm[0] += (m0 - modSm[0]) * modCoef;
            modSm[1] += (m1 - modSm[1]) * modCoef;

            const float inG = inGainSm.getNextValue();
            const float fb = fbSm.getNextValue();
            const float outG = outSm.getNextValue();
            const float wid = widthSm.getNextValue();
            const float lvl = mixSm.getNextValue();
            fDepthSm.skip (1);
            const double depthNow = depthSm.getNextValue();
            const float fz = freezeSm.getNextValue();

            // Eingang je Kanalmodus (Freeze schliesst ihn)
            float x0, x1;
            if (mode == PingPong)     { x0 = 0.5f * (l + r) * inG; x1 = 0.0f; }
            else if (mode == MidSide) { x0 = 0.5f * (l + r) * inG; x1 = 0.5f * (l - r) * inG; }
            else                      { x0 = l * inG; x1 = r * inG; }

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
            if (stableComp != 1.0f) { f0 *= stableComp; f1 *= stableComp; }
            float rr0 = r0, rr1 = r1;   // unveraenderter Loop fuer Freeze, zeitlich passend zu den Saettigungspfaden
            if (useOs)
            {
                auto osIn = [&sat] (Oversampler2x& os, float v)
                {
                    float a, b;
                    os.up (v, a, b);
                    return os.down (analogSat (a, sat), analogSat (b, sat));
                };
                auto osLoop = [&sat] (Oversampler2x& os, float v)
                {
                    float a, b;
                    os.up (v, a, b);
                    return os.down (loopSat (a, sat), loopSat (b, sat));
                };
                // Ohne Drive keine Input-Saettigung: exakte Verzoegerung, der Oversampler ruht. Beim Aufdrehen
                // laeuft er erst 48 Samples warm und wird dann ueber 32 Samples eingeblendet (der Drive selbst
                // faehrt ueber 30 ms hoch, die Saettigung ist in dieser Zeit noch praktisch null).
                float xs0 = xDelay[0].process (x0), xs1 = xDelay[1].process (x1);
                if (sat.blend > 0.0f)
                {
                    if (xOsWarm < 0) { osX[0].reset(); osX[1].reset(); xOsWarm = 0; }
                    const float o0 = osIn (osX[0], x0), o1 = osIn (osX[1], x1);
                    if (xOsWarm >= 80) { xs0 = o0; xs1 = o1; }
                    else
                    {
                        const float wgt = (float) std::max (0, xOsWarm - 48) / 32.0f;
                        xs0 += (o0 - xs0) * wgt;
                        xs1 += (o1 - xs1) * wgt;
                        ++xOsWarm;
                    }
                }
                else xOsWarm = -1;
                x0 = xs0 * (1.0f - fz);
                x1 = xs1 * (1.0f - fz);
                f0 = osLoop (osF[0], f0);
                f1 = osLoop (osF[1], f1);
                rr0 = rDelay[0].process (r0);
                rr1 = rDelay[1].process (r1);
            }
            else
            {
                x0 = analogSat (x0, sat) * (1.0f - fz);
                x1 = analogSat (x1, sat) * (1.0f - fz);
                f0 = loopSat (f0, sat);
                f1 = loopSat (f1, sat);
            }
            float fbNow = fb;
            if (fz > 0.0f)
            {
                f0 += (rr0 - f0) * fz;
                f1 += (rr1 - f1) * fz;
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
            kneeSm.skip (kSub);
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
