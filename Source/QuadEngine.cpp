#include "QuadEngine.h"

namespace qe
{
namespace
{
// Weiche Begrenzung: unter 0.8 unveraendert, darueber sanft gegen 1.0
inline float softLimit (float x)
{
    constexpr float knee = 0.8f, range = 0.2f;
    const float a = std::abs (x);
    if (a <= knee) return x;
    const float y = knee + range * std::tanh ((a - knee) / range);
    return x < 0.0f ? -y : y;
}

// Limiter nur auf die Echos: das Original d bleibt unangetastet, die Echos werden in den verbleibenden Platz gedrueckt.
// Das Knie wandert mit dem Original mit (mindestens |d|): liegt das Original unter dem Knie 0,8, ist es genau
// softLimit (Summe); darueber bleibt die Summe unter 1,0 bzw. nie ueber dem Original, falls dieses selbst heisser ist.
inline float softLimitEchoes (float d, float sum)
{
    const float a = std::abs (d);
    if (a <= 0.8f) return softLimit (sum);
    const float s = std::abs (sum);
    if (s <= a) return sum;
    const float range = 1.0f - a;
    const float y = range > 0.0f ? a + range * std::tanh ((s - a) / range) : a;
    return sum < 0.0f ? -y : y;
}
} // namespace

void QuadEngine::prepare (double sampleRate)
{
    sr = sampleRate;
    for (auto& u : units) u.prepare (sampleRate);
    for (auto& s : spatial) s.prepare (sampleRate);
    noise.prepare (sampleRate);
    noiseSpatial.prepare (sampleRate);
    gfilter.prepare (sampleRate);
    for (auto& o : onSm) o.reset (sampleRate, 0.02);
    for (auto& o : outW) o.reset (sampleRate, 0.02);
    for (auto& o : dryPlaceSm) o.reset (sampleRate, 0.03);
    for (auto& o : dryPanSm) o.reset (sampleRate, 0.05);
    activeSm.reset (sampleRate, 0.02);
    deadSm.reset (sampleRate, 0.02);
    intoSm.reset (sampleRate, 0.02);
    inclDrySm.reset (sampleRate, 0.03);
    mixSm.reset (sampleRate, 0.03);
    outSm.reset (sampleRate, 0.03);
    for (auto& p : unitPeaks) p.store (0.0f);
    duckEnvRel = 1.0f - std::exp (-1.0f / (0.03f * (float) sr));
    reset();
}

void QuadEngine::reset()
{
    for (auto& u : units) u.reset();
    for (auto& s : spatial) s.reset();
    noise.reset();
    noiseSpatial.reset();
    gfilter.reset();
    wasActive.fill (false);
    spatialLive.fill (false);
    noiseSpatialLive = false;
    gate = 1.0f;
    gateState = Gate::Idle;
    duckEnv = duckDb = 0.0f;
    initialised = false;
}

// Ducking: Spitzenpegel des Eingangs -> Absenkung in dB (weiches Knie +-3 dB um den Threshold),
// mit Attack (Absenken) und Release (Zurueckkommen) geglaettet
void QuadEngine::computeDuck (int n, const DuckSettings& d, float* gain)
{
    if (! d.on && duckDb > -1.0e-3f)
    {
        duckDb = 0.0f;
        std::fill (gain, gain + n, 1.0f);
        return;
    }
    const float fs = (float) sr;
    const float atk = 1.0f - std::exp (-1.0f / (std::max (d.attackMs, 0.1f) * 0.001f * fs));
    const float rel = 1.0f - std::exp (-1.0f / (std::max (d.releaseMs, 1.0f) * 0.001f * fs));
    float deepest = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const auto ui = (size_t) i;
        const float a = std::max (std::abs (ext.l[ui]), std::abs (ext.r[ui]));
        duckEnv = a > duckEnv ? a : duckEnv + (a - duckEnv) * duckEnvRel;   // haelt auch tiefe Toene
        float target = 0.0f;
        if (d.on)
        {
            const float over = 20.0f * std::log10 (duckEnv + 1.0e-9f) - d.thresholdDb;
            target = -d.amountDb * std::min (std::max ((over + 3.0f) / 6.0f, 0.0f), 1.0f);
        }
        duckDb += (target - duckDb) * (target < duckDb ? atk : rel);
        gain[i] = dbToGain (duckDb);
        deepest = std::min (deepest, duckDb);
    }
    if (-deepest > duckReduction.load (std::memory_order_relaxed))
        duckReduction.store (-deepest, std::memory_order_relaxed);
}

void QuadEngine::process (float* l, float* r, int n, const EngineSettings& s, double bpm)
{
    // Nicht-endliche Samples vom Host (NaN, Inf) wuerden Delay-Leitungen und Filter dauerhaft vergiften: als Stille behandeln
    for (int i = 0; i < n; ++i)
    {
        if (! std::isfinite (l[i])) l[i] = 0.0f;
        if (! std::isfinite (r[i])) r[i] = 0.0f;
    }
    for (int pos = 0; pos < n; pos += kChunk)
        processChunk (l + pos, r + pos, std::min (kChunk, n - pos), s, bpm);
}

void QuadEngine::processChunk (float* l, float* r, int n, const EngineSettings& s, double bpm)
{
    const int requested = std::min (std::max (s.algorithm, 0), kNumAlgorithms - 1);
    bool snapWeights = false;
    if (! initialised)
    {
        activeAlgo = requested;
        mixSm.setCurrentAndTargetValue (s.globalMix);
        outSm.setCurrentAndTargetValue (dbToGain (s.globalOutDb));
        intoSm.setCurrentAndTargetValue (s.noise.into);
        inclDrySm.setCurrentAndTargetValue (s.gfilter.includeDry ? 1.0f : 0.0f);
        for (int u = 0; u < kNumUnits; ++u)
        {
            onSm[(size_t) u].setCurrentAndTargetValue (s.echo[(size_t) u].on ? 1.0f : 0.0f);
            dryPlaceSm[(size_t) u].setCurrentAndTargetValue (s.echo[(size_t) u].positionDry ? 1.0f : 0.0f);
            dryPanSm[(size_t) u].setCurrentAndTargetValue (juce::jlimit (-1.0f, 1.0f, s.pos[(size_t) u].x));
        }
        snapWeights = true;
        initialised = true;
    }
    mixSm.setTargetValue (s.globalMix);
    outSm.setTargetValue (dbToGain (s.globalOutDb));
    intoSm.setTargetValue (s.noise.into);
    inclDrySm.setTargetValue (s.gfilter.includeDry ? 1.0f : 0.0f);

    // Algorithmuswechsel: Ausgang kurz ausblenden, Topologie wechseln, wieder einblenden
    if (requested != activeAlgo && gateState == Gate::Idle) gateState = Gate::Down;
    if (gateState == Gate::Down && gate <= 0.0f)
    {
        activeAlgo = requested;
        gateState = Gate::Up;
        for (auto& sp : spatial) sp.reset();
        spatialLive.fill (false);
        snapWeights = true;
    }
    const AlgoDef& algo = kAlgorithms[activeAlgo];

    std::copy (l, l + n, ext.l.begin());
    std::copy (r, r + n, ext.r.begin());

    bool anySolo = false;
    for (const auto& e : s.echo) anySolo = anySolo || e.solo;

    SpatialOptions so;
    so.distanceDelay = s.distanceDelay;
    so.earlyReflections = s.earlyReflections;
    so.binaural = s.binaural;
    so.doppler = s.doppler;

    const bool gfOn = s.gfilter.hpOn || s.gfilter.lpOn;
    gfilter.beginChunk (s.gfilter, bpm, n);
    const bool needFirst = s.gfilter.skipFirstEcho && gfOn;   // erste-Echo-Leitung nur lesen, wenn sie gebraucht wird

    // ---- Noise: roh erzeugen, Anteile "direkt" und "in die Echos" ----
    NoiseSettings ns = s.noise;
    ns.on = s.noise.on && ! anySolo;
    noise.process (noiseRaw.l.data(), noiseRaw.r.data(), n, ns, bpm, ext.l.data(), ext.r.data());
    const bool noiseLive = noise.isActive();
    std::array<float, kChunk> dGain, iGain;
    for (int i = 0; i < n; ++i)
    {
        const float a = intoSm.getNextValue();
        dGain[(size_t) i] = std::cos (a * 1.57079632679f);
        iGain[(size_t) i] = std::sin (a * 1.57079632679f);
    }

    // Globaler Dry/Wet (equal power) und Ducking pro Sample; wirken schon vor der Raumplatzierung
    std::array<float, kChunk> dryGa, wetGa, duckGa;
    for (int i = 0; i < n; ++i)
    {
        const float m = mixSm.getNextValue();
        dryGa[(size_t) i] = std::cos (m * 1.57079632679f);
        wetGa[(size_t) i] = std::sin (m * 1.57079632679f);
    }
    computeDuck (n, s.duck, duckGa.data());

    for (int u = 0; u < kNumUnits; ++u)
    {
        const auto uu = (size_t) u;
        const int mask = algo.sources[uu];

        if (mask == 0)
        {
            std::copy (ext.l.begin(), ext.l.begin() + n, src.l.begin());
            std::copy (ext.r.begin(), ext.r.begin() + n, src.r.begin());
            std::copy (ext.l.begin(), ext.l.begin() + n, origSrc.l.begin());
            std::copy (ext.r.begin(), ext.r.begin() + n, origSrc.r.begin());
            if (noiseLive)   // Noise laeuft in die Delays hinein (mitgeechot, in der Schleife gesaettigt)
            {
                // lokale __restrict-Zeiger: der Compiler darf vektorisieren (Member-Arrays koennten sich sonst ueberlappen)
                float* __restrict sl = src.l.data();
                float* __restrict sr2 = src.r.data();
                const float* __restrict nl = noiseRaw.l.data();
                const float* __restrict nr = noiseRaw.r.data();
                const float* __restrict ig = iGain.data();
                for (int i = 0; i < n; ++i)
                {
                    sl[i] += nl[i] * ig[i];
                    sr2[i] += nr[i] * ig[i];
                }
            }
        }
        else
        {
            int count = 0;
            for (auto* blk : { &src, &origSrc })
            {
                std::fill (blk->l.begin(), blk->l.begin() + n, 0.0f);
                std::fill (blk->r.begin(), blk->r.begin() + n, 0.0f);
            }
            for (int j = 0; j < kNumUnits; ++j)
                if (mask & (1 << j))
                {
                    ++count;
                    const auto uj = (size_t) j;
                    float* __restrict dst[4] = { src.l.data(), src.r.data(), origSrc.l.data(), origSrc.r.data() };
                    const float* __restrict add[4] = { outs[uj].l.data(), outs[uj].r.data(), origs[uj].l.data(), origs[uj].r.data() };
                    for (int k = 0; k < 4; ++k)
                    {
                        float* __restrict d = dst[k];
                        const float* __restrict a = add[k];
                        for (int i = 0; i < n; ++i) d[i] += a[i];
                    }
                }
            const float inv = 1.0f / (float) std::max (count, 1);
            for (float* __restrict d : { src.l.data(), src.r.data(), origSrc.l.data(), origSrc.r.data() })
                for (int i = 0; i < n; ++i) d[i] *= inv;
        }

        onSm[uu].setTargetValue (s.echo[uu].on ? 1.0f : 0.0f);
        const bool active = s.echo[uu].on || onSm[uu].isSmoothing() || onSm[uu].getCurrentValue() > 0.0f;

        if (active)
        {
            if (units[uu].clearPending()) units[uu].finishClear();   // vor dem Ende des verteilten Loeschens wieder an
            wasActive[uu] = true;
            EchoSettings es = s.echo[uu];
            es.feedback *= s.feedbackScale;
            units[uu].process (src.l.data(), src.r.data(), tmp.l.data(), tmp.r.data(), n, es, bpm,
                               nullptr, nullptr,
                               needFirst ? firstTmp.l.data() : nullptr, needFirst ? firstTmp.r.data() : nullptr,
                               origSrc.l.data(), origSrc.r.data(), origTmp.l.data(), origTmp.r.data());
            for (int i = 0; i < n; ++i)
            {
                const auto ui = (size_t) i;
                const float g = onSm[uu].getNextValue();
                if (needFirst)
                {
                    firsts[uu].l[ui] = firstTmp.l[ui] * g;
                    firsts[uu].r[ui] = firstTmp.r[ui] * g;
                }
                outs[uu].l[ui] = src.l[ui] + (tmp.l[ui] - src.l[ui]) * g;
                outs[uu].r[ui] = src.r[ui] + (tmp.r[ui] - src.r[ui]) * g;
                origs[uu].l[ui] = origSrc.l[ui] + (origTmp.l[ui] - origSrc.l[ui]) * g;
                origs[uu].r[ui] = origSrc.r[ui] + (origTmp.r[ui] - origSrc.r[ui]) * g;
            }
        }
        else
        {
            if (wasActive[uu]) { units[uu].beginDeferredReset(); wasActive[uu] = false; }
            units[uu].clearSome (n * kClearPerSample);
            std::copy (src.l.begin(), src.l.begin() + n, outs[uu].l.begin());
            std::copy (src.r.begin(), src.r.begin() + n, outs[uu].r.begin());
            std::copy (origSrc.l.begin(), origSrc.l.begin() + n, origs[uu].l.begin());
            std::copy (origSrc.r.begin(), origSrc.r.begin() + n, origs[uu].r.begin());
            std::fill (firsts[uu].l.begin(), firsts[uu].l.begin() + n, 0.0f);
            std::fill (firsts[uu].r.begin(), firsts[uu].r.begin() + n, 0.0f);
        }

        // Pegelanzeige der Einheit: nur ihre Echos (Ausgang ohne Original-Spur)
        float pk = 0.0f;
        for (int i = 0; i < n; ++i)
            pk = std::max ({ pk, std::abs (outs[uu].l[(size_t) i] - origs[uu].l[(size_t) i]),
                                 std::abs (outs[uu].r[(size_t) i] - origs[uu].r[(size_t) i]) });
        if (s.echo[uu].on && pk > unitPeaks[uu].load (std::memory_order_relaxed))
            unitPeaks[uu].store (pk, std::memory_order_relaxed);
    }

    // Ausgangseinheiten: Raumplatzierung + Gewichte. Ist eine Ausgangseinheit aus, uebernehmen ihre
    // Quellen (in der Kette die letzte eingeschaltete Einheit) Position und Anteil.
    std::array<bool, kNumUnits> onArr, mutedArr, soloArr;
    for (int u = 0; u < kNumUnits; ++u)
    {
        onArr[(size_t) u] = s.echo[(size_t) u].on;
        mutedArr[(size_t) u] = isMuted (s.echo[(size_t) u]);
        soloArr[(size_t) u] = s.echo[(size_t) u].solo;
    }
    const Placement placement = placementOf (activeAlgo, onArr, mutedArr, soloArr);
    bool anyOut = false;
    for (int u = 0; u < kNumUnits; ++u)
    {
        const auto uu = (size_t) u;
        const float target = placement.weight[uu];
        anyOut = anyOut || target > 0.0f;
        if (snapWeights) outW[uu].setCurrentAndTargetValue (target);
        else             outW[uu].setTargetValue (target);

        const bool live = target > 0.0f || outW[uu].isSmoothing() || outW[uu].getCurrentValue() > 0.0f;
        if (live)
        {
            spatialLive[uu] = true;
            // Die Echos werden im Raum platziert. Das Original bleibt unplatziert, ausser "Place Dry"
            // ist an (weich ueberblendet; der Spatializer ist linear).
            dryPlaceSm[uu].setTargetValue (s.echo[uu].positionDry ? 1.0f : 0.0f);
            dryPanSm[uu].setTargetValue (juce::jlimit (-1.0f, 1.0f, s.pos[uu].x));
            // Echo-Anteil = Ausgang minus Original-Spur (auch Echos frueherer Einheiten in der Kette);
            // globaler Filter darauf, optional ohne das erste Echo dieser Einheit
            const bool skip = needFirst;
            for (int c = 0; c < 2; ++c)
            {
                float* __restrict w = c == 0 ? wetM.l.data() : wetM.r.data();
                const float* __restrict o = c == 0 ? outs[uu].l.data() : outs[uu].r.data();
                const float* __restrict g = c == 0 ? origs[uu].l.data() : origs[uu].r.data();
                const float* __restrict f = c == 0 ? firsts[uu].l.data() : firsts[uu].r.data();
                if (skip) for (int i = 0; i < n; ++i) w[i] = o[i] - g[i] - f[i];
                else      for (int i = 0; i < n; ++i) w[i] = o[i] - g[i] - 0.0f;
            }
            if (gfOn)
            {
                gfilter.process (u, wetM.l.data(), wetM.r.data(), n);
                if (skip)
                    for (int c = 0; c < 2; ++c)
                    {
                        float* __restrict w = c == 0 ? wetM.l.data() : wetM.r.data();
                        const float* __restrict f = c == 0 ? firsts[uu].l.data() : firsts[uu].r.data();
                        for (int i = 0; i < n; ++i) w[i] += f[i];
                    }
            }
            for (int i = 0; i < n; ++i)
            {
                const auto ui = (size_t) i;
                const float a = dryPlaceSm[uu].getNextValue();
                const float px = dryPanSm[uu].getNextValue();
                const float we = wetGa[ui] * duckGa[ui];
                // Echos mit Wet-Anteil und Ducking werden im Raum platziert
                spOut[uu].l[ui] = wetM.l[ui] * we;
                spOut[uu].r[ui] = wetM.r[ui] * we;
                // Anteil des Originals: mit "Place Dry" nur per Pegel an die Seite der Einheit (Equal-Power,
                // Breite schrumpft zur Seite hin). Ohne Laufzeit, Entfernung, Luftfilter und Reflexionen: sonst
                // ergibt er mit dem Rest des Originals einen Kammfilter, und das Original wird leiser und duenner.
                const float ol = origs[uu].l[ui], orr = origs[uu].r[ui];
                if (a > 0.0f)
                {
                    const float th = (px + 1.0f) * 0.78539816f;
                    const float gl = 1.41421356f * std::cos (th), gr = 1.41421356f * std::sin (th);
                    const float mid = 0.5f * (ol + orr), side = 0.5f * (ol - orr) * (1.0f - std::abs (px));
                    spDry[uu].l[ui] = ol + a * ((mid + side) * gl - ol);
                    spDry[uu].r[ui] = orr + a * ((mid - side) * gr - orr);
                }
                else
                {
                    spDry[uu].l[ui] = ol;
                    spDry[uu].r[ui] = orr;
                }
            }
            spatial[uu].process (spOut[uu].l.data(), spOut[uu].r.data(), n, s.pos[uu], so);
        }
        else if (spatialLive[uu])
        {
            spatial[uu].reset();
            gfilter.resetInstance (u);
            spatialLive[uu] = false;
        }
    }
    // Anteil des Wet-Pfads: 1 = Einheiten am Ausgang, 0 = nichts aktiv (Dry geht unveraendert durch)
    if (snapWeights) activeSm.setCurrentAndTargetValue (anyOut ? 1.0f : 0.0f);
    else             activeSm.setTargetValue (anyOut ? 1.0f : 0.0f);
    // stumme Zweige zaehlen fuer die Mittelung mit, klingen aber nicht
    if (snapWeights) deadSm.setCurrentAndTargetValue (anyOut ? placement.dead : 0.0f);
    else             deadSm.setTargetValue (anyOut ? placement.dead : 0.0f);

    // Noise direkt zum Ausgang, an seiner Position im Raum
    if (noiseLive)
    {
        for (int i = 0; i < n; ++i)
        {
            noiseOut.l[(size_t) i] = noiseRaw.l[(size_t) i] * dGain[(size_t) i];
            noiseOut.r[(size_t) i] = noiseRaw.r[(size_t) i] * dGain[(size_t) i];
        }
        noiseSpatial.process (noiseOut.l.data(), noiseOut.r.data(), n, ns.pos, so);
        if (gfOn) gfilter.process (4, noiseOut.l.data(), noiseOut.r.data(), n);
        noiseSpatialLive = true;
    }
    else
    {
        if (noiseSpatialLive) { noiseSpatial.reset(); gfilter.resetInstance (4); noiseSpatialLive = false; }
        std::fill (noiseOut.l.begin(), noiseOut.l.begin() + n, 0.0f);
        std::fill (noiseOut.r.begin(), noiseOut.r.begin() + n, 0.0f);
    }

    std::array<float, kChunk> busL, busR, dryL, dryR, dryFL, dryFR, outGa, inclA;
    const float gateStep = 1.0f / (0.015f * (float) sr);
    for (int i = 0; i < n; ++i)
    {
        if (gateState == Gate::Down)    gate = std::max (0.0f, gate - gateStep);
        else if (gateState == Gate::Up) { gate = std::min (1.0f, gate + gateStep); if (gate >= 1.0f) gateState = Gate::Idle; }

        float sumW = 0.0f, wl = 0.0f, wr = 0.0f, wlD = 0.0f, wrD = 0.0f;
        for (int u = 0; u < kNumUnits; ++u)
        {
            const float w = outW[(size_t) u].getNextValue();
            if (w > 0.0f)
            {
                wl += spOut[(size_t) u].l[(size_t) i] * w;
                wr += spOut[(size_t) u].r[(size_t) i] * w;
                wlD += spDry[(size_t) u].l[(size_t) i] * w;
                wrD += spDry[(size_t) u].r[(size_t) i] * w;
                sumW += w;
            }
        }
        sumW += deadSm.getNextValue();
        const float inv = gate / std::max (sumW, 1e-3f);
        const float act = activeSm.getNextValue();
        const auto ui = (size_t) i;
        const float dryG = dryGa[ui], wetG = wetGa[ui];
        outGa[ui] = outSm.getNextValue();
        inclA[ui] = inclDrySm.getNextValue();

        // Original: Mittel der Anteile der Ausgangseinheiten (mit Place Dry seitlich verschoben) (mit Gain on Dry, Output, Mute).
        // Waehrend eines Algorithmuswechsels und ohne Ausgangseinheit das Eingangssignal selbst.
        float oL = ext.l[ui], oR = ext.r[ui];
        if (sumW > 1e-6f)
        {
            oL = gate * wlD / sumW + (1.0f - gate) * ext.l[ui];
            oR = gate * wrD / sumW + (1.0f - gate) * ext.r[ui];
        }
        // Wet-Bus: Echos (bereits mit Wet-Anteil, gefiltert, platziert) plus Noise direkt
        busL[ui] = wl * inv * act + noiseOut.l[ui] * wetG;
        busR[ui] = wr * inv * act + noiseOut.r[ui] * wetG;
        // Ohne aktive Einheit bleibt das Original unveraendert; sonst Equal-Power-Mix
        dryL[ui] = ext.l[ui] * (1.0f - act) + oL * dryG * act;
        dryR[ui] = ext.r[ui] * (1.0f - act) + oR * dryG * act;
        dryFL[ui] = dryL[ui];
        dryFR[ui] = dryR[ui];
    }
    if (gfOn) gfilter.process (5, dryFL.data(), dryFR.data(), n);   // "Include dry": Original mitfiltern (weich ueberblendet)

    float masterPk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const auto ui = (size_t) i;
        const float dl = dryL[ui] + (dryFL[ui] - dryL[ui]) * inclA[ui];
        const float dr = dryR[ui] + (dryFR[ui] - dryR[ui]) * inclA[ui];
        float ol = (dl + busL[ui]) * outGa[ui];
        float orr = (dr + busR[ui]) * outGa[ui];
        if (s.softLimiter)
        {
            if (s.limitDry) { ol = softLimit (ol); orr = softLimit (orr); }
            else            { ol = softLimitEchoes (dl * outGa[ui], ol); orr = softLimitEchoes (dr * outGa[ui], orr); }
        }
        // Letzte Absicherung (z. B. NaN aus einem Parameter): nie NaN/Inf ausgeben, danach alles neu aufsetzen
        if (! std::isfinite (ol) || ! std::isfinite (orr)) { ol = orr = 0.0f; poisoned = true; }
        l[i] = ol;
        r[i] = orr;
        masterPk = std::max ({ masterPk, std::abs (ol), std::abs (orr) });
    }
    if (masterPk > masterPeak.load (std::memory_order_relaxed))
        masterPeak.store (masterPk, std::memory_order_relaxed);
    if (poisoned)
    {
        reset();
        poisoned = false;
    }
}
} // namespace qe
