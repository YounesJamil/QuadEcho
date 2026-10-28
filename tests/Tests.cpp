// DSP-Tests und Offline-Renders fuer QUAD ECHO.
// Aufruf: QuadEchoTests [Ausgabeordner fuer WAVs]
//        QuadEchoTests --harness ...   (Referenz-Harness, siehe Harness.h)
#include <chrono>
#include "../Source/StateMigration.h"
#include "../Source/Tail.h"
#include "TestUtil.h"
#include "Harness.h"
#include "HarnessCommon.h"

using namespace qetest;

namespace
{
// ---------------------------------------------------------------------------
void testDelayTimes()
{
    std::cout << "[Delay-Zeiten]" << std::endl;
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        for (int block : { 64, 512, 1000 })
        {
            DelayUnit u; u.prepare (sr);
            auto s = wetOnly(); s.timeL = 100.0f;
            auto out = runUnit (u, s, impulse ((size_t) (0.5 * sr)), block);
            const size_t expect = (size_t) std::llround (0.1 * sr);
            check (argMaxAbs (out.l) == expect, "100 ms bei " + std::to_string ((int) sr) + " Hz, Block " + std::to_string (block)
                   + ": Peak bei " + std::to_string (argMaxAbs (out.l)) + ", erwartet " + std::to_string (expect));
            check (std::abs (out.l[expect] - 1.0f) < 0.01f, "Peak-Amplitude 1.0, ist " + std::to_string (out.l[expect]));
        }
    }

    // Tempo-Sync: 1/8 bei 120 BPM = 250 ms, 1/4 punktiert bei 90 BPM = 1000 ms, 1/8T bei 100 BPM = 200 ms
    struct Case { int div; double bpm; double ms; };
    for (auto c : { Case { 5, 120.0, 250.0 }, Case { 9, 90.0, 1000.0 }, Case { 4, 100.0, 200.0 } })
    {
        DelayUnit u; u.prepare (48000.0);
        auto s = wetOnly(); s.sync = true; s.divL = c.div; s.syncMode = s.syncModeR = noteFamily (c.div);   // Notes/Triplet/Dotted passend zum Notenwert
        auto out = runUnit (u, s, impulse (96000), 256, c.bpm);
        const size_t expect = (size_t) std::llround (c.ms * 48.0);
        check ((long) argMaxAbs (out.l) - (long) expect <= 1 && (long) expect - (long) argMaxAbs (out.l) <= 1,
               "Sync-Delay " + std::to_string (c.ms) + " ms: Peak bei " + std::to_string (argMaxAbs (out.l)));
    }

    // Sync-Modi wie bei Ableton Echo: 16th = Anzahl 16tel, Moduswechsel behaelt die Grundnote
    {
        EchoSettings s; s.sync = true; s.link = true;
        double l = 0, r = 0;
        // Sync-Modus je Kanal (Link aus): L in 16teln, R triolisch
        {
            EchoSettings t; t.sync = true; t.link = false; t.syncMode = Sync16th; t.sixL = 3; t.syncModeR = SyncTriplet; t.divR = 8;   // 1/4 -> 1/4T
            delayTimesMs (t, 120.0, l, r);
            check (std::abs (l - 375.0) < 1e-6 && std::abs (r - 1000.0 / 3.0) < 1e-6, "Sync-Modus je Kanal: L 3/16, R 1/4T (" + std::to_string (l) + " / " + std::to_string (r) + " ms)");
        }
        s.syncMode = Sync16th; s.sixL = 3;
        delayTimesMs (s, 120.0, l, r);
        check (std::abs (l - 375.0) < 1e-6, "16th: 3/16 bei 120 BPM = 375 ms (" + std::to_string (l) + ")");
        s.divL = 5;   // 1/8
        s.syncMode = SyncTriplet; delayTimesMs (s, 120.0, l, r);
        check (std::abs (l - 500.0 / 3.0) < 1e-6, "Triplet: 1/8 wird 1/8T (" + std::to_string (l) + " ms)");
        s.syncMode = SyncDotted; delayTimesMs (s, 120.0, l, r);
        check (std::abs (l - 375.0) < 1e-6, "Dotted: 1/8 wird 1/8. (" + std::to_string (l) + " ms)");
        s.syncMode = SyncNotes; s.divL = 4; delayTimesMs (s, 120.0, l, r);   // 1/8T im Modus Notes -> 1/8
        check (std::abs (l - 250.0) < 1e-6, "Notes: 1/8T wird 1/8 (" + std::to_string (l) + " ms)");
        s.syncMode = Sync16th; s.sixL = 4; s.offsetL = -10.0f; s.offset = 20.0f; delayTimesMs (s, 120.0, l, r);
        check (std::abs (l - 450.0) < 1e-6 && std::abs (r - 600.0) < 1e-6, "Offset je Kanal: L -10 %, R +20 % (" + std::to_string (l) + " / " + std::to_string (r) + " ms)");
        check (divInMode (16, SyncTriplet) == 13 && divInMode (0, SyncDotted) == 3, "Moduswechsel ohne passende Grundnote: naechstliegender Wert (4/1 -> 1/1T, 1/32 -> 1/16.)");
    }

    // Link aus, Offset +50 % auf R: L 100 ms, R 200 ms * 1.5 = 300 ms
    {
        DelayUnit u; u.prepare (48000.0);
        auto s = wetOnly(); s.link = false; s.timeL = 100.0f; s.timeR = 200.0f; s.offset = 50.0f;
        auto out = runUnit (u, s, impulse (48000), 256);
        check (argMaxAbs (out.l) == 4800, "Offset: L bei 100 ms");
        check (argMaxAbs (out.r) == 14400, "Offset: R bei 300 ms, ist " + std::to_string (argMaxAbs (out.r)));
    }

    // Ping-Pong: L-Echo bei tL, R-Echo bei tL + tR
    {
        DelayUnit u; u.prepare (48000.0);
        auto s = wetOnly(); s.mode = PingPong; s.feedback = 0.5f; s.timeL = 100.0f;
        auto out = runUnit (u, s, impulse (48000), 256);
        check (argMaxAbs (out.l, 0) == 4800 && out.r[4800] == 0.0f, "Ping-Pong: erstes Echo nur links");
        check (std::abs (out.r[9600]) > 0.1f && std::abs (out.l[9600]) < 1e-6f, "Ping-Pong: zweites Echo nur rechts bei 200 ms");
        check (std::abs (out.l[14400]) > 0.05f, "Ping-Pong: drittes Echo wieder links");
    }

    // Mid-Side: Mono-Impuls erscheint nach Dekodierung auf beiden Kanaelen gleich
    {
        DelayUnit u; u.prepare (48000.0);
        auto s = wetOnly(); s.mode = MidSide; s.timeL = 100.0f;
        auto out = runUnit (u, s, impulse (24000), 256);
        check (std::abs (out.l[4800] - out.r[4800]) < 1e-5f && out.l[4800] > 0.9f, "Mid-Side: mono bleibt mittig (L " + std::to_string (out.l[4800]) + ", R " + std::to_string (out.r[4800]) + ", peak " + std::to_string (argMaxAbs (out.l)) + ")");
    }

    // Feedback-Invert: zweites Echo mit umgekehrter Phase
    {
        DelayUnit u; u.prepare (48000.0);
        auto s = wetOnly(); s.feedback = 0.5f; s.fbInvert = true; s.timeL = 50.0f;
        auto out = runUnit (u, s, impulse (24000), 256);
        check (out.l[2400] > 0.9f && out.l[4800] < -0.1f, "Feedback-Invert kehrt zweite Wiederholung um");
    }

    // Durchlaufendes Signal bleibt, Echo-Pegel skaliert nur die Echos; Input-Gain wirkt aufs Dry nur mit Schalter
    {
        DelayUnit u; u.prepare (48000.0);
        auto s = wetOnly(); s.inGainDb = -6.0206f; s.mix = 0.5f; s.timeL = 100.0f;
        auto out = runUnit (u, s, impulse (24000), 256, 120.0, true);
        check (std::abs (out.l[0] - 1.0f) < 0.01f, "Dry laeuft unveraendert durch (" + std::to_string (out.l[0]) + ")");
        check (std::abs (out.l[4800] - 0.5f * 0.5f) < 0.01f, "Echo mit Input-Gain und Echo-Pegel 50 % (" + std::to_string (out.l[4800]) + ")");
        DelayUnit u2; u2.prepare (48000.0); s.inGainOnDry = true;
        auto out2 = runUnit (u2, s, impulse (24000), 256, 120.0, true);
        check (std::abs (out2.l[0] - 0.5f) < 0.01f, "Dry mit 'Gain auch auf Dry'");
    }
}

void testFeedback()
{
    std::cout << "[Feedback]" << std::endl;
    const double sr = 48000.0;
    {
        DelayUnit u; u.prepare (sr);
        auto s = wetOnly(); s.feedback = 0.9f; s.timeL = 50.0f; s.hpOn = true; s.lpOn = true; s.lpHz = 8000.0f;
        auto in = makeSig ((size_t) (20 * sr));
        auto burst = noise ((size_t) (0.05 * sr), 0.5f);
        std::copy (burst.l.begin(), burst.l.end(), in.l.begin());
        std::copy (burst.r.begin(), burst.r.end(), in.r.begin());
        auto out = runUnit (u, s, in, 512);
        check (finite (out), "Feedback 90 %: endlich");
        const float early = rms (out.l, (size_t) (0.1 * sr), (size_t) (1.0 * sr));
        const float late = rms (out.l, (size_t) (19 * sr), (size_t) (20 * sr));
        check (late < early * 0.01f, "Feedback 90 % klingt ab (early " + std::to_string (early) + ", late " + std::to_string (late) + ")");
    }
    {
        DelayUnit u; u.prepare (sr);
        auto s = wetOnly(); s.feedback = 1.1f; s.timeL = 80.0f; s.lpOn = true; s.lpHz = 900.0f; s.res = 1.0f; s.lpBoost = 18.0f;   // hoechste Resonanz und voller Boost
        auto in = makeSig ((size_t) (20 * sr));
        auto burst = noise ((size_t) (0.1 * sr), 0.8f);
        std::copy (burst.l.begin(), burst.l.end(), in.l.begin());
        std::copy (burst.r.begin(), burst.r.end(), in.r.begin());
        auto out = runUnit (u, s, in, 512);
        check (finite (out, 5.0f), "Feedback 110 % mit hoechster Resonanz und Boost bleibt dank Saettigung begrenzt (Peak " + std::to_string (peakAbs (out.l)) + ")");
    }
}

// 18 dB/Oktave: eine Oktave hoeher (2 -> 4 kHz) liegt bei LP 1 kHz rund 17 dB tiefer (12 dB/Oktave: rund 12 dB)
void testFilterSlope()
{
    std::cout << "[Filtersteilheit 18 dB/Oktave]" << std::endl;
    const double sr = 48000.0;
    auto level = [&] (double hz)
    {
        GlobalFilter f; f.prepare (sr);
        GlobalFilterSettings gs; gs.lpOn = true; gs.lpHz = 1000.0f; gs.res = 0.0f; gs.depth = 0.0f;
        auto s = sine ((size_t) sr, sr, hz, 0.5f);
        for (size_t pos = 0; pos < s.l.size(); pos += 256)
        {
            const int n = (int) std::min<size_t> (256, s.l.size() - pos);
            f.beginChunk (gs, 120.0, n);
            f.process (0, s.l.data() + pos, s.r.data() + pos, n);
        }
        return rms (s.l, (size_t) (0.5 * sr), s.l.size());
    };
    const float a = level (2000.0), b = level (4000.0);
    check (toDb (b / a) < -15.0f, "LP 1 kHz: 18 dB/Oktave (2 -> 4 kHz: " + std::to_string (toDb (b / a)) + " dB)");

    // Boost: Glocke an der Grenzfrequenz, +12 dB bei 1 kHz gegen ohne Boost
    auto boosted = [&] (float boostDb)
    {
        GlobalFilter f; f.prepare (sr);
        GlobalFilterSettings gs; gs.lpOn = true; gs.lpHz = 1000.0f; gs.res = 0.0f; gs.depth = 0.0f; gs.lpBoost = boostDb;
        auto s = sine ((size_t) sr, sr, 1000.0, 0.1f);
        for (size_t pos = 0; pos < s.l.size(); pos += 256)
        {
            const int n = (int) std::min<size_t> (256, s.l.size() - pos);
            f.beginChunk (gs, 120.0, n);
            f.process (0, s.l.data() + pos, s.r.data() + pos, n);
        }
        return rms (s.l, (size_t) (0.5 * sr), s.l.size());
    };
    const float gainDb = toDb (boosted (12.0f) / boosted (0.0f));
    check (std::abs (gainDb - 12.0f) < 1.0f, "Boost +12 dB an der Grenzfrequenz (" + std::to_string (gainDb) + " dB)");
}

void testFuzz()
{
    std::cout << "[Fuzz: keine NaN/Inf]" << std::endl;
    juce::Random rnd (42);
    auto rf = [&] (float a, float b) { return a + (b - a) * rnd.nextFloat(); };
    auto ri = [&] (int n) { return rnd.nextInt (n); };
    for (double sr : { 44100.0, 192000.0 })
    {
        QuadEngine e; e.prepare (sr);
        EngineSettings s;
        bool ok = true;
        const int blocks[] = { 1, 7, 64, 333, 1024, 4096 };
        for (int iter = 0; iter < 300 && ok; ++iter)
        {
            for (int u = 0; u < kNumUnits; ++u)
            {
                auto& c = s.echo[(size_t) u];
                c.on = ri (6) != 0; c.sync = ri (2); c.link = ri (2);
                c.timeL = rf (1, 4000); c.timeR = rf (1, 4000); c.divL = ri (17); c.divR = ri (17);
                c.mode = ri (3); c.offset = rf (-50, 50); c.feedback = rf (0, 1.1f); c.fbInvert = ri (2);
                c.inGainDb = rf (-24, 12); c.inGainOnDry = ri (2); c.positionDry = ri (2); c.hpOn = ri (2); c.lpOn = ri (2); c.mute = ri (8) == 0; c.solo = ri (10) == 0;
                c.hpHz = rf (20, 8000); c.lpHz = rf (200, 20000); c.res = rf (0, 1); c.hpRes = rf (0, 1); c.hpBoost = rf (0, 18); c.lpBoost = rf (0, 18); c.offsetL = rf (-50, 50);
                c.transition = ri (3); c.mix = rf (0, 1); c.outDb = rf (-24, 12); c.width = rf (0, 2);
                c.wave = ri (kNumWaves); c.lfoOn = ri (4) != 0; c.filterPhaseDeg = rf (0, 360); c.rateHz = rf (0.01f, 20); c.lfoSync = ri (2); c.lfoDiv = ri (17);
                c.phaseDeg = rf (0, 360); c.x4 = ri (2); c.delayDepth = rf (0, 1); c.filterDepth = rf (0, 1); c.envMix = rf (0, 1);
                c.freeze = ri (4) == 0;
                s.pos[(size_t) u] = { rf (-1, 1), rf (-1, 1), rf (-1, 1) };
            }
            {
                auto& g = s.gfilter;
                g.hpOn = ri (2); g.lpOn = ri (2); g.hpHz = rf (20, 8000); g.lpHz = rf (200, 20000); g.res = rf (0, 1); g.hpRes = rf (0, 1); g.hpBoost = rf (0, 18); g.lpBoost = rf (0, 18);
                g.includeDry = ri (2); g.skipFirstEcho = ri (2); g.lfoOn = ri (2); g.wave = ri (kNumWaves); g.rateHz = rf (0.01f, 20); g.lfoSync = ri (2); g.lfoDiv = ri (17);
                g.depth = rf (0, 1); g.phaseDeg = rf (0, 360);
                auto& nz = s.noise;
                nz.on = ri (2); nz.level = rf (0, 1); nz.color = rf (0, 1); nz.crackle = rf (0, 1); nz.width = rf (0, 1); nz.into = rf (0, 1);
                nz.hpOn = ri (2); nz.lpOn = ri (2); nz.hpHz = rf (20, 8000); nz.lpHz = rf (200, 20000); nz.res = rf (0, 1); nz.hpRes = rf (0, 1); nz.hpBoost = rf (0, 18); nz.lpBoost = rf (0, 18);
                nz.lfoOn = ri (2); nz.wave = ri (kNumWaves); nz.rateHz = rf (0.01f, 20); nz.lfoSync = ri (2); nz.lfoDiv = ri (17);
                nz.depthF = rf (0, 1); nz.depthL = rf (0, 1); nz.phaseDeg = rf (0, 360); nz.envMix = rf (0, 1);
                nz.pos = { rf (-1, 1), rf (-1, 1), rf (-1, 1) };
                auto& d = s.duck;
                d.on = ri (2); d.thresholdDb = rf (-60, 0); d.amountDb = rf (0, 40); d.attackMs = rf (0.1f, 200); d.releaseMs = rf (20, 3000);
            }
            s.algorithm = ri (kNumAlgorithms); s.globalMix = rf (0, 1); s.globalOutDb = rf (-24, 12);
            s.distanceDelay = ri (2); s.earlyReflections = ri (2); s.binaural = ri (2); s.doppler = rf (0, 2); s.feedbackScale = rf (0, 1); s.softLimiter = ri (2);
            auto in = noise ((size_t) blocks[ri (6)], 0.5f, iter);
            auto out = runEngine (e, s, in, blocks[ri (6)], rf (30, 300));
            if (! finite (out, 1000.0f)) { ok = false; check (false, "NaN/Inf/Explosion in Fuzz-Iteration " + std::to_string (iter)); }
        }
        check (ok, "Fuzz " + std::to_string ((int) sr) + " Hz");
    }
}

void testParameterClicks()
{
    std::cout << "[Klickfreiheit Parameter]" << std::endl;
    const double sr = 48000.0;
    const size_t n = (size_t) (3 * sr), jumpAt = (size_t) (1 * sr);
    const auto in = sine (n, sr, 440.0, 0.5f);

    struct P { const char* name; std::function<void (EchoSettings&, bool)> set; };
    const std::vector<P> params = {
        { "Delay-Zeit (Repitch)", [] (EchoSettings& s, bool b) { s.transition = Repitch; s.timeL = b ? 300.0f : 100.0f; } },
        { "Delay-Zeit (Fade)",    [] (EchoSettings& s, bool b) { s.transition = Fade;    s.timeL = b ? 300.0f : 100.0f; } },
        { "Delay-Zeit (Jump)",    [] (EchoSettings& s, bool b) { s.transition = Jump;    s.timeL = b ? 300.0f : 100.0f; } },
        { "Mix",        [] (EchoSettings& s, bool b) { s.mix = b ? 0.9f : 0.1f; } },
        { "Feedback",   [] (EchoSettings& s, bool b) { s.feedback = b ? 0.5f : 0.0f; } },
        { "Input-Gain", [] (EchoSettings& s, bool b) { s.inGainDb = b ? 6.0f : -24.0f; } },
        { "Output",     [] (EchoSettings& s, bool b) { s.outDb = b ? 6.0f : -24.0f; } },
        { "Width",      [] (EchoSettings& s, bool b) { s.width = b ? 2.0f : 0.0f; } },
        { "Filter an/aus", [] (EchoSettings& s, bool b) { s.hpOn = b; s.lpOn = b; s.hpHz = 400; s.lpHz = 1000; } },
        { "Mute", [] (EchoSettings& s, bool b) { s.mute = b; } },
        { "Output -24 dB", [] (EchoSettings& s, bool b) { s.outDb = b ? -24.0f : 0.0f; } },
        { "LP-Cutoff",  [] (EchoSettings& s, bool b) { s.lpOn = true; s.lpHz = b ? 15000.0f : 300.0f; } },
        { "HP-Cutoff",  [] (EchoSettings& s, bool b) { s.hpOn = true; s.hpHz = b ? 5000.0f : 30.0f; } },
        { "Resonanz",   [] (EchoSettings& s, bool b) { s.hpOn = true; s.lpOn = true; s.res = s.hpRes = b ? 1.0f : 0.0f; } },
        { "Mod-Tiefe",  [] (EchoSettings& s, bool b) { s.delayDepth = b ? 1.0f : 0.0f; s.rateHz = 2; } },
        { "Offset",     [] (EchoSettings& s, bool b) { s.link = true; s.offset = b ? 40.0f : -40.0f; } },
        { "Einheit an/aus", [] (EchoSettings& s, bool b) { s.on = b; } },
        { "Freeze",     [] (EchoSettings& s, bool b) { s.freeze = b; s.feedback = 0.5f; s.timeL = 100.01f; } },
    };

    for (auto& p : params)
    {
        QuadEngine e; e.prepare (sr);
        auto base = singleUnitSettings (0);
        base.echo[0].feedback = 0.3f;
        base.echo[0].timeL = 100.0f;
        p.set (base.echo[0], false);
        auto out = runEngine (e, base, in, 128, 120.0, [&] (size_t pos, EngineSettings& s) { p.set (s.echo[0], pos >= jumpAt); });
        // Sprung relativ zur lokalen Amplitude (ein reiner Sinus hat max. 2*pi*f/sr = 0.058)
        const float step = maxStep (out, jumpAt - 2000, n) / std::max (peakAbs (out.l, jumpAt - 2000, n), 0.05f);
        const float ref = maxStep (out, (size_t) (0.5 * sr), jumpAt - 2000) / std::max (peakAbs (out.l, (size_t) (0.5 * sr), jumpAt - 2000), 0.05f);
        check (finite (out, 10.0f) && step < std::max (0.12f, ref * 2.5f),
               std::string ("Parameterwechsel ") + p.name + ": rel. Sprung " + std::to_string (step) + " (Referenz " + std::to_string (ref) + ")");
    }
}

void testAlgorithms()
{
    std::cout << "[Algorithmen]" << std::endl;
    const double sr = 48000.0;
    for (int a = 0; a < kNumAlgorithms; ++a)
    {
        QuadEngine e; e.prepare (sr);
        EngineSettings s; s.algorithm = a;
        const float times[4] = { 100, 150, 200, 250 };
        for (int u = 0; u < kNumUnits; ++u) { s.echo[(size_t) u].on = true; s.echo[(size_t) u].timeL = times[u]; s.echo[(size_t) u].feedback = 0.3f; }
        auto out = runEngine (e, s, impulse ((size_t) (3 * sr)), 256);
        check (finite (out, 10.0f), std::string ("Algorithmus ") + kAlgorithms[a].name + ": endlich");
        check (peakAbs (out.l, (size_t) (0.05 * sr)) > 0.01f && peakAbs (out.r, (size_t) (0.05 * sr)) > 0.01f,
               std::string ("Algorithmus ") + kAlgorithms[a].name + " liefert Echo-Signal");
        // Mono-Eingang
        QuadEngine e2; e2.prepare (sr);
        auto mono = runEngine (e2, s, sine ((size_t) sr, sr, 300.0, 0.4f), 100);
        check (peakAbs (mono.l) > 0.05f, std::string ("Algorithmus ") + kAlgorithms[a].name + " liefert Signal bei Sinus");
    }
}

void testAlgorithmSwitch()
{
    std::cout << "[Algorithmuswechsel]" << std::endl;
    const double sr = 48000.0;
    QuadEngine e; e.prepare (sr);
    EngineSettings s; s.globalMix = 0.8f;
    // Echo-Pegel 50 %: vier Einheiten in Serie mit vollem Pegel wuerden den Limiter hart treiben (steile Flanken sind dann kein Klick)
    for (int u = 0; u < kNumUnits; ++u) { s.echo[(size_t) u].on = true; s.echo[(size_t) u].timeL = 60.0f + 40.0f * (float) u; s.echo[(size_t) u].feedback = 0.4f; s.echo[(size_t) u].mix = 0.5f; }
    const size_t n = (size_t) (3.5 * sr);
    auto in = sine (n, sr, 330.0, 0.4f);
    auto out = runEngine (e, s, in, 177, 120.0, [&] (size_t pos, EngineSettings& st) { st.algorithm = (int) (pos / (size_t) (0.3 * sr)) % kNumAlgorithms; });
    const float step = maxStep (out, (size_t) (0.4 * sr), n);
    check (finite (out, 10.0f) && step < 0.2f, "Algorithmuswechsel ohne Klick (max. Sprung " + std::to_string (step) + ")");
    check (peakAbs (out.l, (size_t) (3.0 * sr)) > 0.05f, "Nach dem Wechsel kommt wieder Signal");
}

void testSpatial()
{
    std::cout << "[3D-Platzierung]" << std::endl;
    const double sr = 48000.0;
    auto spatialRun = [&] (const PosSettings& p, const Sig& in, bool distDelay = false, bool early = false, bool binaural = false)
    {
        Spatializer sp; sp.prepare (sr);
        Sig out = in;
        for (size_t pos = 0; pos < in.l.size(); pos += 256)
        {
            const int n = (int) std::min<size_t> (256, in.l.size() - pos);
            SpatialOptions so; so.distanceDelay = distDelay; so.earlyReflections = early; so.binaural = binaural; so.doppler = 1.0f;
            sp.process (out.l.data() + pos, out.r.data() + pos, n, p, so);
        }
        return out;
    };
    const auto tone = sine ((size_t) sr, sr, 1000.0, 0.5f);
    const size_t a = (size_t) (0.5 * sr), b = (size_t) sr;

    // Entfernung: bis zum 2. Ring (Radius 2/3) praesent, hoechstens -3 dB; aeusserer Ring -12 dB
    const float l1 = rms (spatialRun ({ 0, 0.0f, 0 }, tone).l, a, b);
    const float lHalf = rms (spatialRun ({ 0, 0.5f, 0 }, tone).l, a, b);
    const float lNear = rms (spatialRun ({ 0, 2.0f / 3.0f, 0 }, tone).l, a, b);
    const float l4 = rms (spatialRun ({ 0, 1.0f, 0 }, tone).l, a, b);
    check (toDb (lHalf / l1) > -3.0f && toDb (lNear / l1) > -3.6f, "Bis zum 2. Ring hoechstens ca. -3 dB (Radius 0.5: " + std::to_string (toDb (lHalf / l1)) + ", 2/3: " + std::to_string (toDb (lNear / l1)) + ")");
    check (std::abs (toDb (l4 / l1) + 12.0f) < 0.8f, "Aeusserer Ring (4 m): -12 dB (ist " + std::to_string (toDb (l4 / l1)) + ")");

    // X: Equal-Power-Panning
    {
        auto left = spatialRun ({ -1, 0.0f, 0 }, tone), right = spatialRun ({ 1, 0.0f, 0 }, tone);
        check (rms (left.r, a, b) < 0.005f && rms (left.l, a, b) > 0.05f, "X=-1: Signal links");
        check (rms (right.l, a, b) < 0.005f && rms (right.r, a, b) > 0.05f, "X=+1: Signal rechts");
        auto mid = spatialRun ({ 0, 0.0f, 0 }, tone);
        const float pc = rms (mid.l, a, b) * rms (mid.l, a, b) + rms (mid.r, a, b) * rms (mid.r, a, b);
        const float ps = rms (spatialRun ({ -0.5f, 0.0f, 0 }, tone).l, a, b);
        const float ps2 = rms (spatialRun ({ -0.5f, 0.0f, 0 }, tone).r, a, b);
        const float dHalf = 1.0f + 0.41421356f * 0.75f;                 // Distanz bei Radius 0.5 (Nahbereich bis 2/3: 1 .. 1.414 m)
        const float d2 = dHalf * dHalf;
        check (std::abs ((ps * ps + ps2 * ps2) * d2 / pc - 1.0f) < 0.15f, "Equal-Power: Gesamtleistung beim Pan konstant (nach Distanzkorrektur)");
    }

    // ITD: x=0.5 -> linkes Ohr etwa 0.5*0.6 ms spaeter
    {
        auto o = spatialRun ({ 0.5f, 0.0f, 0 }, impulse ((size_t) (0.2 * sr), 100));
        const long dl = (long) argMaxAbs (o.l), dr = (long) argMaxAbs (o.r);
        check (dl - dr >= 12 && dl - dr <= 17, "ITD bei x=0.5: " + std::to_string (dl - dr) + " Samples (erwartet ~14)");
    }

    // Luftabsorption: Rauschen weit weg ist dunkler (HF-Anteil relativ zum Gesamtpegel)
    {
        const auto nz = noise ((size_t) (2 * sr), 0.4f, 7);
        auto hf = [&] (const std::vector<float>& v) { double e = 0; for (size_t i = a + 1; i < v.size(); ++i) { const double d = (double) v[i] - v[i - 1]; e += d * d; } return e; };
        auto tot = [&] (const std::vector<float>& v) { double e = 0; for (size_t i = a; i < v.size(); ++i) e += (double) v[i] * v[i]; return e; };
        auto nearO = spatialRun ({ 0, 0.0f, 0 }, nz), farO = spatialRun ({ 0, 1.0f, 0 }, nz);
        const double rn = hf (nearO.l) / tot (nearO.l), rf = hf (farO.l) / tot (farO.l);
        check (rf < rn * 0.6, "Luftabsorption: weit entfernt dunkler (" + std::to_string (rf / rn) + ")");
    }

    // Stereobreite sinkt mit Distanz
    {
        auto wide = makeSig ((size_t) sr);
        for (size_t i = 0; i < wide.l.size(); ++i) { wide.l[i] = 0.4f * std::sin (6.2831853f * 500.0f * (float) i / (float) sr); wide.r[i] = -wide.l[i]; }
        auto sideE = [&] (const Sig& s) { double e = 0; for (size_t i = a; i < s.l.size(); ++i) { const double d = 0.5 * ((double) s.l[i] - s.r[i]); e += d * d; } return e; };
        auto midE  = [&] (const Sig& s) { double e = 0; for (size_t i = a; i < s.l.size(); ++i) { const double d = 0.5 * ((double) s.l[i] + s.r[i]); e += d * d; } return e; };
        auto nearO = spatialRun ({ 0, 0.0f, 0 }, wide), farO = spatialRun ({ 0, 1.0f, 0 }, wide);
        (void) midE;
        check (sideE (farO) < sideE (nearO) * 0.2, "Stereobreite nimmt mit Distanz ab");
    }

    // Z: Peak bei 10 kHz fuer z=+1
    {
        const auto hi = sine ((size_t) sr, sr, 10000.0, 0.3f);
        const float flat = rms (spatialRun ({ 0, 0.0f, 0 }, hi).l, a, b);
        const float up = rms (spatialRun ({ 0, 0.0f, 1.0f }, hi).l, a, b);
        check (toDb (up / flat) > 3.0f && toDb (up / flat) < 7.0f, "Hoehe z=+1: Anhebung um 10 kHz (" + std::to_string (toDb (up / flat)) + " dB)");
        const auto lo = sine ((size_t) sr, sr, 500.0, 0.3f);
        const float flat2 = rms (spatialRun ({ 0, 0.0f, 0 }, lo).l, a, b);
        const float up2 = rms (spatialRun ({ 0, 0.0f, 1.0f }, lo).l, a, b);
        check (std::abs (toDb (up2 / flat2)) < 1.0f, "Hoehe beeinflusst Tiefen kaum");
    }

    // Frueh-Reflexionen nur bei Distanz hoerbar, Laufzeit abschaltbar
    {
        auto o = spatialRun ({ 0, 1.0f, 0 }, impulse ((size_t) (0.1 * sr)), false, true);
        check (peakAbs (o.l, 100, 2000) > 0.0005f, "Fruehe Reflexionen vorhanden");
        auto od = spatialRun ({ 0, 1.0f, 0 }, impulse ((size_t) (0.2 * sr)), true, false);
        const size_t pk = argMaxAbs (od.l);
        check (pk > (size_t) (0.008 * sr), "Distanz-Laufzeit aktiv: Peak bei " + std::to_string (pk));
        auto on = spatialRun ({ 0, 1.0f, 0 }, impulse ((size_t) (0.2 * sr)), false, false);
        check (argMaxAbs (on.l) < 5, "Distanz-Laufzeit aus: keine Verzoegerung");
    }

    // Keine Zipper: X springt von -1 nach +1, Y von 0 nach 1, Z von -1 nach 1
    {
        Spatializer sp; sp.prepare (sr);
        auto in = sine ((size_t) (2 * sr), sr, 1000.0, 0.5f);
        Sig out = in;
        for (size_t pos = 0; pos < in.l.size(); pos += 128)
        {
            PosSettings p = pos < (size_t) sr ? PosSettings { -1, 0, -1 } : PosSettings { 1, 1, 1 };
            sp.process (out.l.data() + pos, out.r.data() + pos, (int) std::min<size_t> (128, in.l.size() - pos), p, SpatialOptions { false, true, false, 1.0f });
        }
        const float step = maxStep (out, (size_t) (0.3 * sr), out.l.size());
        check (step < 0.3f, "Positionssprung ohne Zipper (max. Sprung " + std::to_string (step) + ")");
    }
}

void testModulation()
{
    std::cout << "[Modulation]" << std::endl;
    const double sr = 48000.0;
    // Random (weich): stetig auch an Zyklusgrenzen, jeder Zyklus ein anderer Wert, Bereich -1..1
    {
        float maxJump = 0.0f, lo = 1.0f, hi = -1.0f;
        float prev = lfoWave (SmoothRandom, 0.0);
        for (int i = 1; i <= 20000; ++i)
        {
            const float v = lfoWave (SmoothRandom, (double) i / 1000.0);
            maxJump = std::max (maxJump, std::abs (v - prev));
            lo = std::min (lo, v); hi = std::max (hi, v);
            prev = v;
        }
        check (maxJump < 0.01f && lo >= -1.0f && hi <= 1.0f && hi - lo > 1.0f,
               "Random-LFO: weich, ohne Spruenge, nutzt den Bereich (max. Schritt " + std::to_string (maxJump) + ", " + std::to_string (lo) + ".." + std::to_string (hi) + ")");
        check (lfoWave (SmoothRandom, 3.0) != lfoWave (SmoothRandom, 4.0), "Random-LFO: jeder Zyklus ein neuer Wert");
    }
    for (int wave = 0; wave < kNumWaves; ++wave)
    {
        DelayUnit u; u.prepare (sr);
        auto s = wetOnly(); s.timeL = 100.0f; s.delayDepth = 0.5f; s.rateHz = 2.0f; s.wave = wave; s.hpOn = true; s.lpOn = true; s.filterDepth = 0.5f;
        auto out = runUnit (u, s, sine ((size_t) (2 * sr), sr, 440.0, 0.5f), 200);
        check (finite (out, 5.0f) && peakAbs (out.l, (size_t) sr) > 0.1f, "LFO-Welle " + std::to_string (wave) + " liefert Signal");
    }
    {
        // Delay-Modulation verschiebt das Echo
        DelayUnit u; u.prepare (sr);
        auto s = wetOnly(); s.timeL = 100.0f; s.delayDepth = 1.0f; s.rateHz = 1.0f; s.wave = SawUp; s.phaseDeg = 0.0f;
        auto out = runUnit (u, s, impulse ((size_t) (1 * sr), 2000), 256);
        check (argMaxAbs (out.l) != 2000 + 4800, "Delay-Modulation verschiebt Echo");
        // x4-Schalter vergroessert die Verschiebung
        DelayUnit u2; u2.prepare (sr); s.x4 = true;
        auto out4 = runUnit (u2, s, impulse ((size_t) (1 * sr), 2000), 256);
        check (std::abs ((long) argMaxAbs (out4.l) - 6800) > std::abs ((long) argMaxAbs (out.l) - 6800), "x4 vergroessert die Modulation");
    }
    {
        // Phase-Offset: L/R unterschiedlich
        DelayUnit u; u.prepare (sr);
        auto s = wetOnly(); s.timeL = 100.0f; s.delayDepth = 1.0f; s.rateHz = 1.0f; s.wave = SawUp; s.phaseDeg = 180.0f;
        auto out = runUnit (u, s, impulse ((size_t) (1 * sr), 2000), 256);
        check (argMaxAbs (out.l) != argMaxAbs (out.r), "Phase-Offset trennt L und R");
    }
    {
        // Envelope-Mix: Eingangspegel steuert die Modulation
        DelayUnit u; u.prepare (sr);
        auto s = wetOnly(); s.timeL = 100.0f; s.delayDepth = 1.0f; s.envMix = 1.0f; s.delayDepth = 1.0f;
        auto out = runUnit (u, s, sine ((size_t) sr, sr, 440.0, 0.5f), 256);
        check (finite (out, 5.0f) && peakAbs (out.l, (size_t) (0.5 * sr)) > 0.1f, "Envelope-Mix liefert Signal");
    }
}



void testNewFeatures()
{
    std::cout << "[Mute, Solo, Output-aus, Limiter, Feedback-Bremse, Doppler, Binaural]" << std::endl;
    const double sr = 48000.0;
    const size_t n = (size_t) (3 * sr);
    const auto imp = impulse (n, 100);

    auto tail = [&] (const Sig& s, size_t from) { return std::max (peakAbs (s.l, from), peakAbs (s.r, from)); };

    // Mute: keine Echos mehr, Original geht unveraendert durch
    {
        QuadEngine e; e.prepare (sr);
        auto s = singleUnitSettings (0); s.echo[0].mute = true; s.echo[0].feedback = 0.5f; s.softLimiter = false;
        auto o = runEngine (e, s, imp, 256);
        check (tail (o, (size_t) (0.05 * sr)) < 1e-4f, "Mute: keine Echos");
        check (std::abs (o.l[100] - 1.0f) < 0.02f, "Mute: Original bleibt erhalten");
    }
    // Mute: Einheit stumm, auch in Serie (B bekommt nichts von A)
    {
        QuadEngine e; e.prepare (sr);
        EngineSettings s; s.algorithm = 0;
        s.echo[0].on = true; s.echo[0].mute = true; s.echo[0].timeL = 100.0f;
        s.echo[1].on = true; s.echo[1].timeL = 150.0f;
        auto o = runEngine (e, s, imp, 256);
        check (tail (o, (size_t) (0.05 * sr)) < 1e-4f, "Mute in Serie: nichts mehr hoerbar (Peak " + std::to_string (tail (o, (size_t) (0.05 * sr))) + ")");
    }
    // Solo: nur B hoerbar (Echo bei 150 ms, nicht bei 100 ms)
    {
        QuadEngine e; e.prepare (sr);
        EngineSettings s; s.algorithm = 9;
        s.echo[0].on = true; s.echo[0].timeL = 100.0f; s.echo[0].mix = 1.0f; s.echo[0].feedback = 0.0f;
        s.echo[1].on = true; s.echo[1].timeL = 150.0f; s.echo[1].mix = 1.0f; s.echo[1].feedback = 0.0f; s.echo[1].solo = true;
        auto o = runEngine (e, s, imp, 256);
        const size_t pk = argMaxAbs (o.l, (size_t) (0.05 * sr));
        check (std::abs ((long) pk - (long) (0.150 * sr + 100)) < 4, "Solo: nur Einheit B ist hoerbar (Peak bei " + std::to_string (pk) + ")");
        check (peakAbs (o.l, (size_t) (0.095 * sr) + 100, (size_t) (0.105 * sr) + 100) < 1e-3f, "Solo: A schweigt");
    }
    // Ausgeschaltete Einheiten in Parallel verdoppeln das Dry nicht
    {
        QuadEngine e; e.prepare (sr);
        auto s = singleUnitSettings (0); s.echo[0].feedback = 0.0f; s.globalMix = 0.0f; s.softLimiter = false;
        auto o = runEngine (e, s, imp, 256);
        check (std::abs (o.l[100] - 1.0f) < 0.05f && std::abs (o.r[100] - 1.0f) < 0.05f, "Parallel nur A, Dry/Wet 0: Original unveraendert (L " + std::to_string (o.l[100]) + ")");
        check (tail (o, (size_t) (0.05 * sr)) < 1e-4f, "Dry/Wet 0: keine Echos");
    }
    // Limiter: Spitzen bleiben unter 1.0, ohne Limiter nicht
    {
        QuadEngine e1, e2; e1.prepare (sr); e2.prepare (sr);
        auto s = singleUnitSettings (0); s.globalOutDb = 12.0f;
        auto loud = sine ((size_t) sr, sr, 300.0, 0.9f);
        s.softLimiter = true; s.limitDry = true;   // Limiter auf alles (ohne ON DRY bleibt ein heisses Original unangetastet, siehe K-g)
        const float pOn = peakAbs (runEngine (e1, s, loud, 256).l);
        s.softLimiter = false;
        const float pOff = peakAbs (runEngine (e2, s, loud, 256).l);
        check (pOn <= 1.0001f && pOff > 1.5f, "Soft-Limiter begrenzt auf 1.0 (an " + std::to_string (pOn) + ", aus " + std::to_string (pOff) + ")");
        QuadEngine e3; e3.prepare (sr);
        auto quiet = sine ((size_t) sr, sr, 300.0, 0.3f);
        EngineSettings s3; for (auto& ec : s3.echo) ec.on = false;
        const auto oq = runEngine (e3, s3, quiet, 256);
        check (std::abs (peakAbs (oq.l, 1000) - 0.3f) < 0.01f, "Limiter laesst leise Signale unveraendert");
    }
    // Globale Feedback-Bremse
    {
        QuadEngine e; e.prepare (sr);
        auto s = singleUnitSettings (0); s.echo[0].mix = 1.0f; s.echo[0].feedback = 0.9f; s.echo[0].timeL = 100.0f;
        s.feedbackScale = 0.0f;
        auto o = runEngine (e, s, imp, 256);
        check (peakAbs (o.l, (size_t) (0.25 * sr)) < 1e-3f, "Feedback-Bremse 0: nur ein Echo");
    }

    // Place Dry: ohne den Schalter bleibt das Original in der Mitte, mit Schalter wandert es an die Position
    {
        auto run = [&] (bool placeDry)
        {
            QuadEngine e; e.prepare (sr);
            auto s = singleUnitSettings (0);
            s.globalMix = 0.0f; s.echo[0].feedback = 0.0f; s.echo[0].positionDry = placeDry; s.softLimiter = false;
            s.pos[0] = { -1.0f, 0.0f, 0.0f };
            return runEngine (e, s, imp, 256);
        };
        const auto off = run (false), on = run (true);
        check (std::abs (off.l[100] - 1.0f) < 0.05f && std::abs (off.r[100] - 1.0f) < 0.05f, "Place Dry aus: Original bleibt mittig (L " + std::to_string (off.l[100]) + ", R " + std::to_string (off.r[100]) + ")");
        check (std::abs (on.l[100] - 1.41421356f) < 0.05f && std::abs (on.r[100]) < 0.01f, "Place Dry an: Original wandert ganz nach links, gleiche Leistung (L " + std::to_string (on.l[100]) + ", R " + std::to_string (on.r[100]) + ")");
    }

    // Analoger Drive: Input-Gain ueber 0 dB saettigt weich (Pegel waechst weniger als linear, Obertoene), 0 dB bleibt transparent
    {
        auto run = [&] (float gainDb)
        {
            DelayUnit u; u.prepare (sr);
            auto es = wetOnly(); es.timeL = 20.0f; es.inGainDb = gainDb;
            return runUnit (u, es, sine ((size_t) sr, sr, 440.0, 0.2f), 256);
        };
        auto harm = [&] (const Sig& o, int h)   // Anteil der h-ten Harmonischen (Fourier-Koeffizient), relativ zur Grundwelle
        {
            double re1 = 0, im1 = 0, reh = 0, imh = 0;
            for (size_t i = (size_t) (0.3 * sr); i < (size_t) (0.9 * sr); ++i)
            {
                const double ph = 6.283185307179586 * 440.0 * (double) i / sr;
                re1 += o.l[i] * std::cos (ph);        im1 += o.l[i] * std::sin (ph);
                reh += o.l[i] * std::cos (h * ph);    imh += o.l[i] * std::sin (h * ph);
            }
            return std::sqrt (reh * reh + imh * imh) / std::max (std::sqrt (re1 * re1 + im1 * im1), 1e-9);
        };
        const auto flat = run (0.0f), hot = run (18.0f);
        const float lin = 0.2f * dbToGain (18.0f);
        check (std::abs (peakAbs (flat.l, 4800) - 0.2f) < 0.01f && harm (flat, 3) < 0.002, "Input-Gain 0 dB: transparent");
        check (peakAbs (hot.l, 4800) < lin * 0.6f && harm (hot, 2) > 0.01 && harm (hot, 3) > 0.01,
               "Input-Gain +18 dB saettigt analog (Peak " + std::to_string (peakAbs (hot.l, 4800)) + " statt linear " + std::to_string (lin)
               + ", 2. Harm. " + std::to_string (harm (hot, 2)) + ", 3. Harm. " + std::to_string (harm (hot, 3)) + ")");
    }

    // LFO aus: keine Delay-Modulation mehr (Echo bleibt exakt am Platz); Filter-Phase aendert die Filterbewegung
    {
        DelayUnit u1, u2; u1.prepare (sr); u2.prepare (sr);
        auto es = wetOnly(); es.timeL = 100.0f; es.delayDepth = 1.0f; es.rateHz = 1.0f; es.wave = SawUp;
        auto on = runUnit (u1, es, impulse ((size_t) sr, 2000), 256);
        es.lfoOn = false;
        auto off = runUnit (u2, es, impulse ((size_t) sr, 2000), 256);
        check (argMaxAbs (off.l) == 2000 + 4800, "LFO aus: Echo bleibt exakt bei 100 ms");
        check (argMaxAbs (on.l) != 2000 + 4800, "LFO an: Echo wird verschoben");
    }
    {
        auto run = [&] (float phaseDeg)
        {
            DelayUnit u; u.prepare (sr);
            auto es = wetOnly(); es.timeL = 20.0f; es.lpOn = true; es.lpHz = 3000.0f; es.filterDepth = 1.0f; es.rateHz = 2.0f; es.feedback = 0.6f;
            es.filterPhaseDeg = phaseDeg;
            return runUnit (u, es, noise ((size_t) (2 * sr), 0.4f, 5), 256);
        };
        const auto a0 = run (0.0f), a180 = run (180.0f);
        double diff = 0, ref = 0;
        for (size_t i = (size_t) sr; i < a0.l.size(); ++i) { const double d = (double) a0.l[i] - a180.l[i]; diff += d * d; ref += (double) a0.l[i] * a0.l[i]; }
        check (diff > 0.05 * ref, "Filter-Phase 180 Grad: Filterbewegung gegenlaeufig, Ausgang unterscheidet sich (" + std::to_string (diff / std::max (ref, 1e-9)) + ")");
    }

    // ---- Drive in der Schleife: Wiederholungen werden dichter, bleiben aber stabil ----
    {
        auto run = [&] (float gainDb, float fb)
        {
            DelayUnit u; u.prepare (sr);
            auto es = wetOnly(); es.timeL = 100.0f; es.feedback = fb; es.inGainDb = gainDb;
            auto in = makeSig ((size_t) (8 * sr));
            auto burst = sine ((size_t) (0.05 * sr), sr, 440.0, 0.2f);
            std::copy (burst.l.begin(), burst.l.end(), in.l.begin());
            std::copy (burst.r.begin(), burst.r.end(), in.r.begin());
            return runUnit (u, es, in, 256);
        };
        auto h3 = [&] (const Sig& o, int echo)   // 3. Harmonische relativ zur Grundwelle in der echo-ten Wiederholung
        {
            const size_t from = (size_t) ((echo * 0.1 + 0.005) * sr), len = (size_t) (0.04 * sr);
            double r1 = 0, i1 = 0, r3 = 0, i3 = 0;
            for (size_t i = from; i < from + len; ++i)
            {
                const double ph = 6.283185307179586 * 440.0 * (double) (i - from) / sr;
                r1 += o.l[i] * std::cos (ph); i1 += o.l[i] * std::sin (ph);
                r3 += o.l[i] * std::cos (3 * ph); i3 += o.l[i] * std::sin (3 * ph);
            }
            return std::sqrt (r3 * r3 + i3 * i3) / std::max (std::sqrt (r1 * r1 + i1 * i1), 1e-9);
        };
        const auto hot = run (12.0f, 0.8f);
        check (finite (hot, 5.0f), "Drive in der Schleife: endlich");
        check (h3 (hot, 5) > h3 (hot, 1) * 1.1, "Drive in der Schleife: spaete Wiederholungen dichter (1.: " + std::to_string (h3 (hot, 1)) + ", 5.: " + std::to_string (h3 (hot, 5)) + ")");
        const auto decay = run (24.0f, 0.9f);
        check (finite (decay, 5.0f) && rms (decay.l, (size_t) (7 * sr), (size_t) (8 * sr)) < rms (decay.l, (size_t) (0.1 * sr), (size_t) (1 * sr)) * 0.5f,
               "Drive in der Schleife bei 90 % Feedback klingt ab");
        const auto clean = run (0.0f, 0.5f);
        check (h3 (clean, 3) < h3 (hot, 3) * 0.5, "Ohne Drive bleiben die Wiederholungen deutlich sauberer (" + std::to_string (h3 (clean, 3)) + " statt " + std::to_string (h3 (hot, 3)) + ")");
    }

    // ---- Globaler Filter: filtert jedes Echo (auch das erste), Include dry filtert das Original ----
    {
        auto run = [&] (bool lpOn, bool inclDry, bool feedbackFilter)
        {
            QuadEngine e; e.prepare (sr);
            auto st = singleUnitSettings (0);
            st.softLimiter = false;
            st.echo[0].mix = 0.5f; st.echo[0].feedback = 0.0f; st.echo[0].timeL = 200.0f; st.echo[0].sync = false;
            st.echo[0].lpOn = feedbackFilter; st.echo[0].lpHz = 600.0f;
            st.gfilter.lpOn = lpOn; st.gfilter.lpHz = 600.0f; st.gfilter.includeDry = inclDry;
            auto in = makeSig ((size_t) (1 * sr));
            const auto burst = noise ((size_t) (0.1 * sr), 0.4f, 21);       // kurzer Rauschimpuls: Original und Echo liegen zeitlich getrennt
            std::copy (burst.l.begin(), burst.l.end(), in.l.begin());
            std::copy (burst.r.begin(), burst.r.end(), in.r.begin());
            return runEngine (e, st, in, 256);
        };
        auto hf = [&] (const Sig& o, size_t from, size_t to)
        {
            double h = 0, t = 0;
            for (size_t i = from + 1; i < to; ++i) { const double d = (double) o.l[i] - o.l[i - 1]; h += d * d; t += (double) o.l[i] * o.l[i]; }
            return h / std::max (t, 1e-12);
        };
        const size_t dryA = (size_t) (0.02 * sr), dryB = (size_t) (0.09 * sr), echoA = (size_t) (0.21 * sr), echoB = (size_t) (0.29 * sr);
        const auto off = run (false, false, false), on = run (true, false, false), inc = run (true, true, false), fbOnly = run (false, false, true);
        check (hf (on, echoA, echoB) < hf (off, echoA, echoB) * 0.5, "Globaler Filter: erstes Echo wird gefiltert (" + std::to_string (hf (on, echoA, echoB) / hf (off, echoA, echoB)) + ")");
        check (hf (fbOnly, echoA, echoB) > hf (off, echoA, echoB) * 0.8, "Feedback-Filter allein erreicht das erste Echo nicht");
        check (hf (on, dryA, dryB) > hf (off, dryA, dryB) * 0.8, "Globaler Filter ohne Include dry: Original bleibt unberuehrt");
        check (hf (inc, dryA, dryB) < hf (off, dryA, dryB) * 0.5, "Include dry: Original wird mitgefiltert (" + std::to_string (hf (inc, dryA, dryB) / hf (off, dryA, dryB)) + ")");
    }

    // ---- Globaler Filter: erstes Echo aussparen ----
    {
        auto run = [&] (bool gfOn, bool skipFirst)
        {
            QuadEngine e; e.prepare (sr);
            auto st = singleUnitSettings (0);
            st.softLimiter = false;
            st.echo[0].mix = 1.0f; st.echo[0].feedback = 0.6f; st.echo[0].timeL = 100.0f; st.echo[0].sync = false;
            st.gfilter.lpOn = gfOn; st.gfilter.lpHz = 600.0f; st.gfilter.skipFirstEcho = skipFirst;
            auto in = makeSig ((size_t) (1 * sr));
            const auto burst = noise ((size_t) (0.04 * sr), 0.4f, 33);
            std::copy (burst.l.begin(), burst.l.end(), in.l.begin());
            std::copy (burst.r.begin(), burst.r.end(), in.r.begin());
            return runEngine (e, st, in, 256);
        };
        auto hf = [&] (const Sig& o, double t0, double t1)
        {
            double h = 0, t = 0;
            for (size_t i = (size_t) (t0 * sr) + 1; i < (size_t) (t1 * sr); ++i) { const double d = (double) o.l[i] - o.l[i - 1]; h += d * d; t += (double) o.l[i] * o.l[i]; }
            return h / std::max (t, 1e-12);
        };
        const auto off = run (false, false), all = run (true, false), skip = run (true, true);
        // erstes Echo bei 100..140 ms, zweites bei 200..240 ms
        check (hf (all, 0.1, 0.14) < hf (off, 0.1, 0.14) * 0.5, "Globaler Filter ohne Skip: erstes Echo gefiltert");
        check (hf (skip, 0.1, 0.14) > hf (off, 0.1, 0.14) * 0.8, "Skip 1st echo: erstes Echo bleibt ungefiltert (" + std::to_string (hf (skip, 0.1, 0.14) / hf (off, 0.1, 0.14)) + ")");
        check (hf (skip, 0.2, 0.24) < hf (off, 0.2, 0.24) * 0.5, "Skip 1st echo: Wiederholungen werden gefiltert (" + std::to_string (hf (skip, 0.2, 0.24) / hf (off, 0.2, 0.24)) + ")");
    }

    // ---- Noise ----
    {
        auto run = [&] (NoiseSettings ns, const Sig& in, bool unitOn = false, size_t len = 0)
        {
            QuadEngine e; e.prepare (sr);
            auto st = singleUnitSettings (0);
            st.softLimiter = false;
            for (auto& ec : st.echo) ec.on = false;
            if (unitOn) { st.echo[0].on = true; st.echo[0].mix = 1.0f; st.echo[0].feedback = 0.0f; st.echo[0].timeL = 150.0f; st.echo[0].sync = false; }
            st.noise = ns;
            (void) len;
            return runEngine (e, st, in, 256);
        };
        const auto silence = makeSig ((size_t) (2 * sr));
        const size_t a = (size_t) (0.5 * sr), b = (size_t) (2 * sr);
        auto hfR = [&] (const std::vector<float>& v)
        {
            double h = 0, t = 0;
            for (size_t i = a + 1; i < b; ++i) { const double d = (double) v[i] - v[i - 1]; h += d * d; t += (double) v[i] * v[i]; }
            return h / std::max (t, 1e-12);
        };

        NoiseSettings ns; ns.on = true; ns.level = 0.5f; ns.pos = { 0.0f, 0.1f, 0.0f };
        const auto base = run (ns, silence);
        check (rms (base.l, a, b) > 0.005f && finite (base, 2.0f), "Noise an: hoerbares Rauschen (rms " + std::to_string (rms (base.l, a, b)) + ")");

        auto off = ns; off.on = false;
        check (peakAbs (run (off, silence).l, a, b) < 1e-5f, "Noise aus: Stille");
        auto zero = ns; zero.level = 0.0f;
        check (peakAbs (run (zero, silence).l, a, b) < 1e-5f, "Noise Level 0: Stille");

        auto loud = ns; loud.level = 1.0f;
        check (rms (run (loud, silence).l, a, b) > rms (base.l, a, b) * 2.5f, "Noise: 100 % deutlich intensiver als 50 %");

        auto white = ns; white.color = 1.0f; auto brown = ns; brown.color = 0.0f;
        const double rw = hfR (run (white, silence).l), rb = hfR (run (brown, silence).l);
        check (rw > rb * 20.0, "Noise Farbe: weiss deutlich heller als braun (" + std::to_string (rw / std::max (rb, 1e-12)) + ")");

        // Knistern: braunes Grundrauschen per Hochpass entfernen, uebrig bleiben die Knackser (hoher Spitzenfaktor)
        auto crk = ns; crk.level = 0.8f; crk.color = 0.0f; crk.hpOn = true; crk.hpHz = 3000.0f; crk.crackle = 1.0f;
        auto nocrk = crk; nocrk.crackle = 0.0f;
        const auto crest = [&] (const Sig& o) { return peakAbs (o.l, a, b) / std::max (rms (o.l, a, b), 1e-9f); };
        check (crest (run (crk, silence)) > crest (run (nocrk, silence)) * 1.5f,
               "Crackle: Knackser erhoehen den Spitzenfaktor (" + std::to_string (crest (run (crk, silence))) + " statt " + std::to_string (crest (run (nocrk, silence))) + ")");

        auto left = ns; left.pos = { -1.0f, 0.0f, 0.0f }; left.width = 1.0f;
        const auto ln = run (left, silence);
        check (rms (ln.l, a, b) > rms (ln.r, a, b) * 5.0f, "Noise an Position links: Signal links");

        auto narrow = ns; narrow.width = 0.0f;
        const auto nw = run (narrow, silence);
        double dsum = 0, ssum = 0;
        for (size_t i = a; i < b; ++i) { dsum += std::abs (nw.l[i] - nw.r[i]); ssum += std::abs (nw.l[i]) + std::abs (nw.r[i]); }
        check (dsum < 0.05 * ssum, "Noise Breite 0: mono");

        // Stereo: Korrelation zwischen L und R sinkt mit der Breite (0 = mono, 1 = unabhaengige Kanaele)
        {
            auto corr = [&] (float width)
            {
                auto nw2 = ns; nw2.width = width; nw2.pos = { 0.0f, 0.0f, 0.0f };
                const auto o = run (nw2, silence);
                double lr = 0, ll = 0, rr = 0;
                for (size_t i = a; i < b; ++i) { lr += (double) o.l[i] * o.r[i]; ll += (double) o.l[i] * o.l[i]; rr += (double) o.r[i] * o.r[i]; }
                return lr / std::max (std::sqrt (ll * rr), 1e-12);
            };
            const double c0 = corr (0.0f), c5 = corr (0.5f), c1 = corr (1.0f);
            check (c0 > 0.95 && c1 < 0.2 && c5 > c1 && c5 < c0,
                   "Noise ist Stereo: Korrelation Breite 0 / 50 / 100 % = " + std::to_string (c0) + " / " + std::to_string (c5) + " / " + std::to_string (c1));
        }

        // Into echoes: 0 = direkt, 1 = nur in die Delays (ohne aktive Einheit also still, mit Einheit mit Verzoegerung)
        auto into = ns; into.into = 1.0f;
        check (peakAbs (run (into, silence, false).l, a, b) < 1e-4f, "Into echoes 1 ohne aktive Einheit: nichts direkt hoerbar");
        const auto viaUnit = run (into, silence, true);
        check (rms (viaUnit.l, a, b) > 0.003f, "Into echoes 1 mit Einheit: Noise kommt ueber die Echos (rms " + std::to_string (rms (viaUnit.l, a, b)) + ")");

        // Filter wirkt auf die Noise
        auto lpn = ns; lpn.color = 1.0f; lpn.lpOn = true; lpn.lpHz = 500.0f;
        check (hfR (run (lpn, silence).l) < hfR (run (white, silence).l) * 0.2, "Noise-Filter (LP) daempft die Hoehen");
    }

    // ---- Binaural ----
    auto spRun = [&] (const PosSettings& p, const Sig& in, bool binaural, bool early = false)
    {
        Spatializer sp; sp.prepare (sr);
        Sig out = in;
        SpatialOptions so; so.binaural = binaural; so.earlyReflections = early;
        for (size_t pos = 0; pos < in.l.size(); pos += 256)
            sp.process (out.l.data() + pos, out.r.data() + pos, (int) std::min<size_t> (256, in.l.size() - pos), p, so);
        return out;
    };
    const auto nz = noise ((size_t) (2 * sr), 0.4f, 11);
    const size_t a = (size_t) (0.5 * sr);
    auto hfRatio = [&] (const std::vector<float>& v)
    {
        double hf = 0, tot = 0;
        for (size_t i = a + 1; i < v.size(); ++i) { const double d = (double) v[i] - v[i - 1]; hf += d * d; tot += (double) v[i] * v[i]; }
        return hf / std::max (tot, 1e-12);
    };
    {
        auto rt = spRun ({ 0.8f, 0.3f, 0 }, nz, true), lf = spRun ({ -0.8f, 0.3f, 0 }, nz, true);
        const float rR = rms (rt.r, a, nz.l.size()), rL = rms (rt.l, a, nz.l.size());
        const float lR = rms (lf.r, a, nz.l.size()), lL = rms (lf.l, a, nz.l.size());
        check (rR > rL * 1.5f, "Binaural: Quelle rechts, rechtes Ohr lauter (R " + std::to_string (rR) + ", L " + std::to_string (rL) + ")");
        check (std::abs (rR - lL) < 0.05f * rR && std::abs (rL - lR) < 0.05f * rL + 1e-4f, "Binaural: links/rechts spiegelsymmetrisch");
        auto ctr = spRun ({ 0.0f, 0.8f, 0 }, nz, true);
        check (std::abs (rms (ctr.l, a, nz.l.size()) - rms (ctr.r, a, nz.l.size())) < 0.02f * rms (ctr.l, a, nz.l.size()), "Binaural: Quelle vorne ist links/rechts gleich");
    }
    {
        // ITD: Quelle genau rechts -> linkes Ohr ca. 0,65 ms spaeter (Woodworth: 0.0875/343*(pi/2+1) = 0.656 ms)
        auto o = spRun ({ 1.0f, 0.0f, 0 }, impulse ((size_t) (0.1 * sr), 200), true);
        const long dl = (long) argMaxAbs (o.l), dr = (long) argMaxAbs (o.r);
        check (dl - dr >= 25 && dl - dr <= 36, "Binaural ITD bei 90 Grad: " + std::to_string (dl - dr) + " Samples (erwartet ~31)");
    }
    {
        // Hinten dumpfer als vorne (Ohrmuschel), auch im Stereo-Modus dezent
        auto front = spRun ({ 0.0f, 0.8f, 0 }, nz, true), rear = spRun ({ 0.0f, -0.8f, 0 }, nz, true);
        check (hfRatio (rear.l) < hfRatio (front.l) * 0.6, "Binaural: hinten dunkler als vorne (" + std::to_string (hfRatio (rear.l) / hfRatio (front.l)) + ")");
        auto frontS = spRun ({ 0.0f, 0.8f, 0 }, nz, false), rearS = spRun ({ 0.0f, -0.8f, 0 }, nz, false);
        check (hfRatio (rearS.l) < hfRatio (frontS.l) * 0.95, "Stereo: hinten dezent dunkler als vorne");
        check (rms (rearS.l, a, nz.l.size()) < rms (frontS.l, a, nz.l.size()), "Stereo: hinten etwas leiser");
    }
    {
        // Umschalten Stereo <-> Binaural ohne Klick
        Spatializer sp; sp.prepare (sr);
        auto in = sine ((size_t) (2 * sr), sr, 1000.0, 0.5f);
        Sig out = in;
        for (size_t pos = 0; pos < in.l.size(); pos += 128)
        {
            SpatialOptions so; so.binaural = pos >= (size_t) sr;
            sp.process (out.l.data() + pos, out.r.data() + pos, (int) std::min<size_t> (128, in.l.size() - pos), { 0.7f, 0.5f, 0.3f }, so);
        }
        const float step = maxStep (out, (size_t) (0.5 * sr), out.l.size());
        check (step < 0.4f, "Binaural-Umschaltung ohne Klick (max. Sprung " + std::to_string (step) + ")");
    }
    {
        // Bewegung im Binaural-Modus ohne Zipper
        Spatializer sp; sp.prepare (sr);
        auto in = sine ((size_t) (2 * sr), sr, 800.0, 0.5f);
        Sig out = in;
        for (size_t pos = 0; pos < in.l.size(); pos += 128)
        {
            PosSettings p = pos < (size_t) sr ? PosSettings { -1, -1, -0.5f } : PosSettings { 1, 1, 0.5f };
            SpatialOptions so; so.binaural = true;
            sp.process (out.l.data() + pos, out.r.data() + pos, (int) std::min<size_t> (128, in.l.size() - pos), p, so);
        }
        const float step = maxStep (out, (size_t) (0.3 * sr), out.l.size());
        check (step < 0.4f, "Binaural: Positionssprung ohne Zipper (max. Sprung " + std::to_string (step) + ")");
    }

    // ---- Doppler: Entfernen der Quelle senkt die Tonhoehe, Regler skaliert das ----
    {
        auto freqWhileMoving = [&] (float doppler)
        {
            Spatializer sp; sp.prepare (sr);
            const size_t len = (size_t) (1.0 * sr);
            auto in = sine (len, sr, 1000.0, 0.5f);
            Sig out = in;
            for (size_t pos = 0; pos < len; pos += 128)
            {
                // vom 2. Ring zum aeusseren Ring (1,4 m -> 4 m): dort wirkt die Entfernung
                const float y = 2.0f / 3.0f + std::min (1.0f, (float) pos / (float) (0.7 * sr)) / 3.0f;
                SpatialOptions so; so.distanceDelay = true; so.doppler = doppler;
                sp.process (out.l.data() + pos, out.r.data() + pos, (int) std::min<size_t> (128, len - pos), { 0.0f, y, 0.0f }, so);
            }
            const size_t from = (size_t) (0.15 * sr), to = (size_t) (0.55 * sr);
            int crossings = 0;
            for (size_t i = from + 1; i < to; ++i) if (out.l[i - 1] < 0.0f && out.l[i] >= 0.0f) ++crossings;
            return (double) crossings / ((double) (to - from) / sr);
        };
        const double f0 = freqWhileMoving (0.0f), f1 = freqWhileMoving (1.0f), f2 = freqWhileMoving (2.0f);
        check (f1 < f0 - 5.0 && f2 < f1 - 5.0, "Doppler: Quelle entfernt sich, Tonhoehe sinkt, Regler skaliert (0: " + std::to_string (f0) + ", 1: " + std::to_string (f1) + ", 2: " + std::to_string (f2) + " Hz)");
    }
}

// ---------------------------------------------------------------------------
void testMixFreezeDuck()
{
    std::cout << "[Dry/Wet-Konzept, Freeze, Ducking, Zustands-Umrechnung]" << std::endl;
    const double sr = 48000.0;
    const auto imp = impulse ((size_t) (1.5 * sr), 100);
    auto near = [] (const std::vector<float>& v, size_t at, int w = 4)   // Spitze in einem kleinen Fenster
    {
        return peakAbs (v, at >= (size_t) w ? at - (size_t) w : 0, at + (size_t) w + 1);
    };

    // Globaler Dry/Wet ist der einzige Regler fuer das Original
    {
        auto run = [&] (float gMix)
        {
            QuadEngine e; e.prepare (sr);
            auto s = singleUnitSettings (0);
            s.globalMix = gMix; s.softLimiter = false; s.earlyReflections = false;
            s.echo[0].feedback = 0.0f; s.echo[0].sync = false; s.echo[0].timeL = 100.0f;
            return runEngine (e, s, imp, 256);
        };
        const auto half = run (0.5f), wet = run (1.0f);
        check (std::abs (half.l[100] - 0.7071f) < 0.01f, "Standard (Dry/Wet 50 %, Echo-Pegel 100 %): Original bei -3 dB (" + std::to_string (half.l[100]) + ")");
        check (std::abs (wet.l[100]) < 1e-4f && near (wet.l, 100 + 4800) > 0.1f, "Dry/Wet 100 %: kein Original, nur Echos (Original " + std::to_string (wet.l[100]) + ")");
    }
    // Kette A > B (Serial, C/D aus) bei 100 % Wet: Echos von A, B und A+B, kein Original
    {
        QuadEngine e; e.prepare (sr);
        EngineSettings s; s.algorithm = 0; s.globalMix = 1.0f; s.softLimiter = false; s.earlyReflections = false;
        s.echo[0].on = true; s.echo[0].sync = false; s.echo[0].timeL = 100.0f; s.echo[0].feedback = 0.0f;
        s.echo[1].on = true; s.echo[1].sync = false; s.echo[1].timeL = 150.0f; s.echo[1].feedback = 0.0f;
        const auto o = runEngine (e, s, imp, 256);
        check (std::abs (o.l[100]) < 1e-4f, "Kette bei 100 % Wet: kein Original");
        check (near (o.l, 100 + 4800) > 0.1f && near (o.l, 100 + 7200) > 0.1f && near (o.l, 100 + 12000) > 0.1f,
               "Kette: Echos von A (100 ms), B (150 ms) und A+B (250 ms) sind da");
    }
    // Gain on Dry wirkt weiterhin auf das Original am Ausgang
    {
        QuadEngine e; e.prepare (sr);
        auto s = singleUnitSettings (0);
        s.globalMix = 0.0f; s.softLimiter = false; s.echo[0].inGainDb = -6.0206f; s.echo[0].inGainOnDry = true;
        const auto o = runEngine (e, s, imp, 256);
        check (std::abs (o.l[100] - 0.5f) < 0.01f, "Gain on Dry: Original mit -6 dB (" + std::to_string (o.l[100]) + ")");
    }

    // Freeze: Eingang zu, Loop haelt Pegel und Hoehen (auch bei gebrochener Delay-Zeit), danach klingt er wieder ab
    {
        DelayUnit u; u.prepare (sr);
        auto es = wetOnly(); es.timeL = 100.01f; es.feedback = 0.5f;
        const size_t n = (size_t) (9 * sr);
        auto in = makeSig (n);
        const auto burst = noise ((size_t) (0.05 * sr), 0.5f, 3);
        std::copy (burst.l.begin(), burst.l.end(), in.l.begin());
        std::copy (burst.r.begin(), burst.r.end(), in.r.begin());
        const auto later = sine ((size_t) sr, sr, 1000.0, 0.5f);   // lautes Signal waehrend Freeze: darf nicht hinein
        std::copy (later.l.begin(), later.l.end(), in.l.begin() + (long) (2 * sr));
        std::copy (later.r.begin(), later.r.end(), in.r.begin() + (long) (2 * sr));
        Sig out = makeSig (n), wet = makeSig (n);
        for (size_t pos = 0; pos < n; pos += 256)
        {
            const int len = (int) std::min<size_t> (256, n - pos);
            es.freeze = pos >= (size_t) (0.08 * sr) && pos < (size_t) (6 * sr);
            u.process (in.l.data() + pos, in.r.data() + pos, out.l.data() + pos, out.r.data() + pos, len, es, 120.0, wet.l.data() + pos, wet.r.data() + pos);
        }
        const float early = rms (wet.l, (size_t) (0.5 * sr), (size_t) (1.5 * sr));
        const float late = rms (wet.l, (size_t) (4.5 * sr), (size_t) (5.5 * sr));
        check (finite (wet, 2.0f) && early > 0.01f && std::abs (toDb (late) - toDb (early)) < 0.5f,
               "Freeze haelt den Pegel (" + std::to_string (toDb (early)) + " dB -> " + std::to_string (toDb (late)) + " dB)");
        auto hfr = [&] (double t0, double t1)
        {
            double h = 0, t = 0;
            for (size_t i = (size_t) (t0 * sr) + 1; i < (size_t) (t1 * sr); ++i) { const double d = (double) wet.l[i] - wet.l[i - 1]; h += d * d; t += (double) wet.l[i] * wet.l[i]; }
            return h / std::max (t, 1e-12);
        };
        check (hfr (4.5, 5.5) > hfr (0.5, 1.5) * 0.9, "Freeze: der Loop wird nicht dunkler (" + std::to_string (hfr (4.5, 5.5) / hfr (0.5, 1.5)) + ")");
        // 1 kHz waehrend Freeze haette eine klare Spitze im Spektrum: Energie bei 1 kHz relativ zum Loop pruefen
        double re = 0, im = 0;
        for (size_t i = (size_t) (4 * sr); i < (size_t) (5 * sr); ++i)
        {
            const double ph = 6.283185307179586 * 1000.0 * (double) i / sr;
            re += wet.l[i] * std::cos (ph); im += wet.l[i] * std::sin (ph);
        }
        const double amp1k = 2.0 * std::sqrt (re * re + im * im) / sr;
        check (amp1k < 0.02, "Freeze: neues Eingangssignal kommt nicht in den Loop (1 kHz: " + std::to_string (amp1k) + ")");
        check (rms (wet.l, (size_t) (8 * sr), (size_t) (9 * sr)) < early * 0.01f, "Nach Freeze klingt der Loop wieder ab");
    }

    // Ducking: Echos werden leiser, solange gespielt wird, und kommen in der Pause zurueck
    {
        auto run = [&] (bool duck)
        {
            QuadEngine e; e.prepare (sr);
            auto s = singleUnitSettings (0);
            s.globalMix = 1.0f; s.softLimiter = false; s.earlyReflections = false;
            s.echo[0].feedback = 0.0f; s.echo[0].sync = false; s.echo[0].timeL = 300.0f;
            s.duck.on = duck; s.duck.thresholdDb = -30.0f; s.duck.amountDb = 12.0f; s.duck.attackMs = 10.0f; s.duck.releaseMs = 50.0f;
            auto in = makeSig ((size_t) (2 * sr));
            const auto tone = sine ((size_t) sr, sr, 440.0, 0.5f);
            std::copy (tone.l.begin(), tone.l.end(), in.l.begin());
            std::copy (tone.r.begin(), tone.r.end(), in.r.begin());
            return runEngine (e, s, in, 256);
        };
        const auto plain = run (false), ducked = run (true);
        const float during = rms (ducked.l, (size_t) (0.5 * sr), (size_t) (0.95 * sr)) / rms (plain.l, (size_t) (0.5 * sr), (size_t) (0.95 * sr));
        const float pause = rms (ducked.l, (size_t) (1.2 * sr), (size_t) (1.29 * sr)) / rms (plain.l, (size_t) (1.2 * sr), (size_t) (1.29 * sr));
        check (std::abs (toDb (during) + 12.0f) < 1.0f, "Ducking: waehrend des Spielens -12 dB (" + std::to_string (toDb (during)) + " dB)");
        check (toDb (pause) > -1.0f, "Ducking: in der Pause kommen die Echos zurueck (" + std::to_string (toDb (pause)) + " dB)");
    }

    // Umrechnung alter Zustaende: Verhaeltnis Original/Echo bleibt
    {
        juce::XmlElement old ("STATE");
        auto add = [&old] (const char* id, double v) { auto* p = old.createNewChildElement ("PARAM"); p->setAttribute ("id", id); p->setAttribute ("value", v); };
        add ("a_on", 1.0); add ("a_mix", 0.5); add ("b_on", 1.0); add ("b_mix", 0.25); add ("gMix", 1.0);
        migrateState (old);
        auto val = [&old] (const char* id)
        {
            for (auto* c : old.getChildWithTagNameIterator ("PARAM")) if (c->getStringAttribute ("id") == id) return c->getDoubleAttribute ("value");
            return -1.0;
        };
        // alt: Original = cos(33.75 Grad) = 0.8315 (mittlerer Mix 0.375), Echo A = sin 45 = 0.7071, Echo B = sin 22.5 = 0.3827
        const double g = val ("gMix") * 1.5707963267948966;
        check (std::abs (val ("a_mix") - 1.0) < 1e-6 && std::abs (val ("b_mix") - 0.3827 / 0.7071) < 1e-3, "Umrechnung: Echo-Pegel A 100 %, B im alten Verhaeltnis");
        check (std::abs (std::tan (g) - 0.7071 / 0.8315) < 1e-3 && old.getIntAttribute (kStateVersionAttr) == kStateVersion,
               "Umrechnung: globaler Dry/Wet trifft das alte Verhaeltnis, Version gesetzt (" + std::to_string (val ("gMix")) + ")");
        check (val ("a_syncMode") == SyncNotes, "Umrechnung: alte Projekte behalten Notenwerte (Sync-Modus Notes)");
        check (std::abs (val ("a_hpRes") - 0.2) < 1e-9 && std::abs (val ("gf_hpRes") - 0.2) < 1e-9, "Umrechnung: gemeinsame Resonanz gilt fuer HP und LP");
        check (val ("a_syncModeR") == val ("a_syncMode"), "Umrechnung: R bekommt den Sync-Modus von L");
        const double before = val ("gMix");
        migrateState (old);
        check (val ("gMix") == before, "Umrechnung laeuft nur einmal");
    }

    // Globaler Filter, Phase R: 0 Grad = beide Seiten gleich, 180 Grad = rechts gegenlaeufig (Stereo-Bewegung)
    {
        auto run = [&] (float phaseDeg)
        {
            QuadEngine e; e.prepare (sr);
            auto s = singleUnitSettings (0);
            s.globalMix = 1.0f; s.softLimiter = false; s.earlyReflections = false; s.pos[0] = { 0.0f, 0.3f, 0.0f };
            s.echo[0].feedback = 0.5f; s.echo[0].sync = false; s.echo[0].timeL = 50.0f;
            s.gfilter.lpOn = true; s.gfilter.lpHz = 800.0f; s.gfilter.depth = 1.0f; s.gfilter.rateHz = 2.0f; s.gfilter.phaseDeg = phaseDeg;
            auto in = noise ((size_t) (2 * sr), 0.3f, 9);
            in.r = in.l;   // mono hinein
            return runEngine (e, s, in, 256);
        };
        auto lrDiff = [&] (const Sig& o)
        {
            double d = 0, t = 0;
            for (size_t i = (size_t) sr; i < o.l.size(); ++i) { const double x = (double) o.l[i] - o.r[i]; d += x * x; t += (double) o.l[i] * o.l[i]; }
            return d / std::max (t, 1e-12);
        };
        const double same = lrDiff (run (0.0f)), opposite = lrDiff (run (180.0f));
        check (same < 1e-6 && opposite > 0.05, "Globaler Filter Phase R: 0 Grad gleich, 180 Grad Stereo-Bewegung (" + std::to_string (same) + " / " + std::to_string (opposite) + ")");
    }
}

// ---------------------------------------------------------------------------
void testPlacement()
{
    std::cout << "[Platzierung: ausgeschaltete Ausgangseinheit gibt die Position weiter]" << std::endl;
    using B4 = std::array<bool, kNumUnits>;
    const B4 none { { false, false, false, false } };
    auto w = [&] (int algo, B4 on) { return placementOf (algo, on, none, none).weight; };

    auto serial = w (0, { { true, true, true, false } });   // A>B>C>D, D aus: C ist der Ausgang
    check (serial[2] == 1.0f && serial[3] == 0.0f && serial[0] == 0.0f, "Serial, D aus: C wird platziert");
    auto onlyA = w (0, { { true, false, false, false } });
    check (onlyA[0] == 1.0f, "Serial, nur A an: A wird platziert");
    auto diamond = w (2, { { true, true, true, false } });  // A>(B,C)>D, D aus
    check (std::abs (diamond[1] - 0.5f) < 1e-6f && std::abs (diamond[2] - 0.5f) < 1e-6f, "Diamond, D aus: B und C je zur Haelfte");
    auto fanIn = w (5, { { true, false, false, false } });  // (A,B,C)>D, nur A an
    check (std::abs (fanIn[0] - 1.0f / 3.0f) < 1e-6f, "Fan In, D aus: A mit einem Drittel (B und C tragen nur das Original)");
    const auto dual = placementOf (1, { { true, false, false, false } }, none, none);   // A>B, C>D, nur A
    check (dual.weight[0] == 1.0f && dual.weight[3] == 0.0f && dual.idle[3], "Dual Chain, nur A: A platziert, D vorab platzierbar");
    auto mutedC = placementOf (0, { { true, true, true, false } }, { { false, false, true, false } }, none);
    check (mutedC.weight[2] == 0.0f && mutedC.dead == 1.0f, "Serial, D aus, C stumm: nichts hoerbar");

    // Place Dry auf einer von drei parallelen Einheiten (A mittig, B links, C rechts) betrifft nur ihr Drittel
    // des Originals: Pegel des Originals (vor dem ersten Echo) mit und ohne Place Dry auf A
    {
        const double sr = 48000.0;
        auto dryLevel = [&] (bool placeA, bool distDelay, bool early)
        {
            QuadEngine e; e.prepare (sr);
            EngineSettings s; s.algorithm = 9; s.softLimiter = false; s.distanceDelay = distDelay; s.earlyReflections = early;
            for (int u = 0; u < 3; ++u)
            {
                auto& c = s.echo[(size_t) u];
                c.on = true; c.sync = false; c.timeL = 400.0f; c.feedback = 0.0f;
            }
            s.echo[0].positionDry = placeA;
            s.pos[0] = { 0.0f, 0.0f, 0.0f };
            s.pos[1] = { -0.6f, 0.3f, 0.0f };
            s.pos[2] = { 0.6f, 0.3f, 0.0f };
            const auto o = runEngine (e, s, noise ((size_t) (0.35 * sr), 0.3f, 4), 256);
            return 0.5f * (rms (o.l, (size_t) (0.1 * sr), (size_t) (0.35 * sr)) + rms (o.r, (size_t) (0.1 * sr), (size_t) (0.35 * sr)));
        };
        for (int variant = 0; variant < 3; ++variant)
        {
            const bool dd = variant == 1, er = variant == 2;
            const float off = dryLevel (false, dd, er), on = dryLevel (true, dd, er);
            check (std::abs (toDb (on / off)) < 0.2f, std::string ("Place Dry auf A (mittig, 1 von 3 Einheiten") + (dd ? ", Dist. Delay" : "") + (er ? ", Early Refl." : "")
                                                       + "): Original bleibt gleich laut (" + std::to_string (toDb (on / off)) + " dB)");
        }
    }

    // Im Klang: Serial mit D aus, C ganz links, D ganz rechts -> die Echos kommen links
    {
        const double sr = 48000.0;
        QuadEngine e; e.prepare (sr);
        EngineSettings s; s.algorithm = 0; s.globalMix = 1.0f; s.softLimiter = false; s.earlyReflections = false;
        for (int u = 0; u < 3; ++u) { s.echo[(size_t) u].on = true; s.echo[(size_t) u].sync = false; s.echo[(size_t) u].timeL = 60.0f; s.echo[(size_t) u].feedback = 0.0f; }
        s.echo[3].on = false;
        s.pos[2] = { -1.0f, 0.0f, 0.0f };
        s.pos[3] = { 1.0f, 0.0f, 0.0f };
        const auto o = runEngine (e, s, impulse ((size_t) sr, 100), 256);
        check (peakAbs (o.l) > 0.05f && peakAbs (o.r) < peakAbs (o.l) * 0.1f,
               "Serial, D aus: Echos an der Position von C (L " + std::to_string (peakAbs (o.l)) + ", R " + std::to_string (peakAbs (o.r)) + ")");
    }
}

// ---------------------------------------------------------------------------
// Regressionstests zu den Befunden in AUDIT.md (je Befund ein Abschnitt)
void testAuditFixes()
{
    std::cout << "[Audit-Befunde]" << std::endl;
    const double sr = 48000.0;

    // A1: ein nicht-endliches Eingangs-Sample darf den Ausgang nicht dauerhaft vergiften
    {
        auto s = harness::worstCase();
        for (float bad : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() })
        {
            const size_t n = (size_t) (2.0 * sr);
            auto in = noise ((size_t) (0.3 * sr), 0.1f, 9);
            in.l.resize (n, 0.0f); in.r.resize (n, 0.0f);
            in.l[(size_t) (0.2 * sr)] = bad;
            in.l[(size_t) (1.0 * sr)] = in.r[(size_t) (1.0 * sr)] = 0.5f;
            QuadEngine e; e.prepare (sr);
            const auto out = runEngine (e, s, in, 512);
            check (finite (out, 10.0f), std::string ("A1: Ausgang endlich trotz ") + (std::isnan (bad) ? "NaN" : "Inf") + " im Eingang");
            check (peakAbs (out.l, (size_t) (1.05 * sr), n) > 0.01f, "A1: nach dem Fehler kommen wieder Echos");
        }
    }

    // B1: Ausschalten einer Einheit darf keinen Block um ein Vielfaches verlaengern (192 kHz, Block 32, CPU-Zeit des Threads)
    {
        const double fs = 192000.0;
        QuadEngine e; e.prepare (fs);
        EngineSettings s = harness::worstCase();
        const size_t n = (size_t) (3.0 * fs);
        auto io = noise (n, 0.3f, 3);
        std::vector<double> cyc;
        for (size_t pos = 0; pos < n; pos += 32)
        {
            s.echo[1].on = (pos / (size_t) (0.25 * fs)) % 2 == 0;
            const auto c0 = harness::threadCycles();
            e.process (io.l.data() + pos, io.r.data() + pos, 32, s, 120.0);
            if (pos >= (size_t) (0.3 * fs)) cyc.push_back ((double) (harness::threadCycles() - c0));
        }
        std::sort (cyc.begin(), cyc.end());
        const double median = cyc[cyc.size() / 2], third = cyc[cyc.size() - 3];
        check (median <= 0.0 || third < 6.0 * median, "B1: Einheit aus kostet hoechstens 6 x Median-Block (drittgroesster Block "
               + std::to_string (third / std::max (median, 1.0)) + " x)");
    }

    // A4: dieselbe Instanz liefert nach reset() und prepare() denselben Ausgang wie beim ersten Mal (auch mit Noise)
    {
        for (const auto& c : harness::baselineCases (sr))
        {
            if (c.name != "feat_noise_into" && c.name != "feat_gfilter" && c.name != "algo01_stereo") continue;
            const auto in = harness::stimulus (2, sr);
            QuadEngine e; e.prepare (sr);
            const auto a1 = runEngine (e, c.s, in, 512, c.bpm, c.mut);
            e.reset();
            const auto a2 = runEngine (e, c.s, in, 512, c.bpm, c.mut);
            e.prepare (sr);
            const auto a3 = runEngine (e, c.s, in, 512, c.bpm, c.mut);
            auto same = [] (const Sig& x, const Sig& y) { return std::memcmp (x.l.data(), y.l.data(), x.l.size() * sizeof (float)) == 0
                                                               && std::memcmp (x.r.data(), y.r.data(), x.r.size() * sizeof (float)) == 0; };
            check (same (a1, a2) && same (a1, a3), "A4: " + c.name + " nach reset()/prepare() bitgenau wie beim ersten Render");
        }
    }

    // A5: gemeldete Ausklingzeit deckt die echte ab (Ausgang danach unter -100 dBFS), unendlich bei Dauerschwingung
    {
        auto s = singleUnitSettings (0);
        s.globalMix = 1.0f;
        s.echo[0].feedback = 0.8f; s.echo[0].timeL = 1000.0f;
        const double tail = tailSeconds (s, 120.0, sr);
        QuadEngine e; e.prepare (sr);
        const size_t n = (size_t) ((std::isfinite (tail) ? tail : 10.0) * sr) + (size_t) sr;
        auto out = runEngine (e, s, impulse (n, 0), 512);
        const float after = peakAbs (out.l, (size_t) (tail * sr), n);
        check (std::isfinite (tail) && after < 1.0e-5f && tail < 47.1 * 1.5,
               "A5: Tail bei Feedback 0.8 / 1 s: gemeldet " + std::to_string (tail) + " s, danach " + std::to_string (toDb (after)) + " dBFS");
        auto lp = s.echo[0];
        lp.hpOn = lp.lpOn = true; lp.hpHz = 150.0f; lp.lpHz = 2500.0f; lp.res = lp.hpRes = 1.0f; lp.hpBoost = lp.lpBoost = 12.0f;
        check (std::abs (toDb ((float) loopFilterPeakGain (lp, sr)) - 22.5f) < 0.3f, "A5: Filter-Spitze in der Schleife wie analytisch (+22,5 dB)");
        auto osc = s; osc.echo[0] = lp; osc.echo[0].feedback = 0.5f;
        auto one = s; one.echo[0].feedback = 1.0f;
        auto fr = s; fr.echo[0].freeze = true;
        check (std::isinf (tailSeconds (osc, 120.0, sr)) && std::isinf (tailSeconds (one, 120.0, sr)) && std::isinf (tailSeconds (fr, 120.0, sr)),
               "A5: unendlich bei Schleifenverstaerkung >= 1 und Freeze");
    }

    // K-g: Limiter nur auf die Echos (Schalter "ON DRY" aus): heisses Original bleibt bitgenau, Summe bleibt begrenzt
    {
        auto s = singleUnitSettings (0);
        s.globalMix = 0.0f; s.softLimiter = true;
        const auto hot = sine ((size_t) sr, sr, 440.0, 0.95f);
        QuadEngine e1; e1.prepare (sr);
        const auto o1 = runEngine (e1, s, hot, 512);
        check (std::memcmp (o1.l.data(), hot.l.data(), hot.l.size() * sizeof (float)) == 0,
               "K-g: Limiter ohne ON DRY laesst ein heisses Original (0,95) bitgenau durch (Spitze " + std::to_string (peakAbs (o1.l)) + ")");
        s.limitDry = true;
        QuadEngine e2; e2.prepare (sr);
        check (std::abs (peakAbs (runEngine (e2, s, hot, 512).l) - 0.927f) < 0.002f, "K-g: mit ON DRY wirkt der Limiter wie bisher auf das Original");
        auto w = singleUnitSettings (0);
        w.echo[0].feedback = 0.9f; w.echo[0].timeL = 50.0f; w.globalMix = 0.5f;
        QuadEngine e3; e3.prepare (sr);
        check (peakAbs (runEngine (e3, w, sine ((size_t) (2 * sr), sr, 220.0, 0.9f), 512).l) <= 1.0001f,
               "K-g: Original plus Echos: Summe bleibt unter 0 dBFS");
        auto hotOut = w; hotOut.globalOutDb = 12.0f; hotOut.globalMix = 0.3f;
        const auto hin = sine ((size_t) (2 * sr), sr, 220.0, 0.9f);
        QuadEngine e4; e4.prepare (sr);
        const auto o4 = runEngine (e4, hotOut, hin, 512);
        auto dryOnly = hotOut; dryOnly.echo[0].on = false;
        QuadEngine e5; e5.prepare (sr);
        const auto o5 = runEngine (e5, dryOnly, hin, 512);
        check (peakAbs (o4.l) <= std::max (1.0f, peakAbs (o5.l)) + 1.0e-4f,
               "K-g: Original selbst ueber 0 dBFS: Summe nie lauter als das Original (" + std::to_string (peakAbs (o4.l)) + " / " + std::to_string (peakAbs (o5.l)) + ")");
        juce::XmlElement v5 ("STATE");
        v5.setAttribute (kStateVersionAttr, 5);
        migrateState (v5);
        auto* p = v5.getChildByAttribute ("id", "limitDry");
        check (p != nullptr && p->getDoubleAttribute ("value") > 0.5 && v5.getIntAttribute (kStateVersionAttr) == kStateVersion,
               "K-g: alte Zustaende (Version 5) behalten den Limiter auf dem Original");
    }

    // K-a: Repitch erreicht die eingestellte Delay-Zeit (vorher blieb die float-Glaettung bis 1200 Samples davor stehen)
    for (double fs : { 48000.0, 192000.0 })
        for (const auto j : { std::pair<float, float> { 100.0f, 350.0f }, { 1000.0f, 4000.0f }, { 3999.0f, 3000.0f } })
        {
            const double soll = (double) j.second * fs / 1000.0;
            const double ist = harness::measureDelaySamples (fs, j.first, j.second, Repitch, 4.0);
            check (std::abs (ist - soll) < 0.05, "K-a: Repitch " + std::to_string ((int) j.first) + " -> " + std::to_string ((int) j.second)
                   + " ms bei " + std::to_string ((int) fs) + " Hz: Abweichung " + std::to_string (ist - soll) + " Samples");
        }

    // K-b: Delay-Index in double: eine lange gebrochene Zeit sitzt genau (float: Stufen von 1/16 Sample bei 4 s / 192 kHz;
    // 3999,99 ms = 767998,08 Samples wurden zu 767998,0625)
    {
        const double fs = 192000.0;
        const double soll = 3999.99 * fs / 1000.0;
        const double ist = harness::measureDelaySamples (fs, 3999.99f, 3999.99f, Fade, 4.5);
        const double sollF = (double) 3999.99f * fs / 1000.0;   // der Parameter selbst ist float
        check (std::abs (ist - sollF) < 0.002, "K-b: 3999,99 ms bei 192 kHz auf 0,002 Samples genau (Abweichung "
               + std::to_string (ist - sollF) + ", Soll " + std::to_string (soll) + ")");
    }

    // A2/A3: bei konstanten Parametern ist der Ausgang unabhaengig von der Blockgroesse (auch 1, 441, wechselnd)
    {
        auto render = [] (const harness::Case& c, const Sig& in, double fs, const std::function<int (size_t)>& next)
        {
            QuadEngine e; e.prepare (fs);
            Sig out = in;
            size_t pos = 0, k = 0;
            while (pos < in.l.size())
            {
                const int len = (int) std::min<size_t> ((size_t) next (k++), in.l.size() - pos);
                e.process (out.l.data() + pos, out.r.data() + pos, len, c.s, c.bpm);
                pos += (size_t) len;
            }
            return out;
        };
        for (double fs : { 48000.0, 192000.0 })
            for (const auto& c : harness::baselineCases (fs))
            {
                if (c.mut) continue;   // Automation greift am Blockanfang, ist also nicht blockgroessen-unabhaengig
                const auto in = harness::stimulus (2, fs);
                const auto ref = render (c, in, fs, [] (size_t) { return 512; });
                for (int cfg = 0; cfg < 3; ++cfg)
                {
                    const auto out = render (c, in, fs, [cfg] (size_t k) {
                        if (cfg == 0) return 1;
                        if (cfg == 1) return 441;
                        juce::Random r ((juce::int64) (k * 7919 + 5)); return 1 + r.nextInt (1024); });
                    const bool same = std::memcmp (ref.l.data(), out.l.data(), ref.l.size() * sizeof (float)) == 0
                                   && std::memcmp (ref.r.data(), out.r.data(), ref.r.size() * sizeof (float)) == 0;
                    if (! same)
                        check (false, "A3: " + c.name + " bei " + std::to_string ((int) fs) + " Hz, Block "
                                      + (cfg == 0 ? "1" : (cfg == 1 ? "441" : "wechselnd")) + " nicht bitgenau gleich Block 512");
                    else
                        check (true, "");
                }
            }
    }
}

void benchmark()
{
    std::cout << "[CPU-Last, Worst Case: 4 Einheiten parallel, Filter, Modulation, Reflexionen]" << std::endl;
    for (double sr : { 48000.0, 192000.0 })
    {
        QuadEngine e; e.prepare (sr);
        EngineSettings s; s.algorithm = 9;
        for (int u = 0; u < kNumUnits; ++u)
        {
            auto& c = s.echo[(size_t) u];
            c.on = true; c.hpOn = true; c.lpOn = true; c.delayDepth = 0.5f; c.filterDepth = 0.5f; c.rateHz = 1.5f;
            c.mode = PingPong; c.feedback = 0.6f; c.timeL = 200.0f + 50.0f * (float) u;
            s.pos[(size_t) u] = { -0.6f + 0.4f * (float) u, 0.5f, 0.3f };
        }
        const double seconds = 10.0;
        const auto in = noise ((size_t) (seconds * sr), 0.3f);
        const auto t0 = std::chrono::steady_clock::now();
        auto out = runEngine (e, s, in, 512);
        const double el = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        const double cpu = 100.0 * el / seconds;
        std::cout << "  " << (int) sr << " Hz: " << cpu << " % eines Kerns" << std::endl;
        check (finite (out, 10.0f) && cpu < 30.0, "CPU-Last bei " + std::to_string ((int) sr) + " Hz unter 30 % (" + std::to_string (cpu) + " %)");
    }
}

void benchmarkDetail()
{
    std::cout << "[CPU-Detail, 48 kHz, Block 512, in % eines Kerns]" << std::endl;
    const double sr = 48000.0, seconds = 10.0;
    const auto in = noise ((size_t) (seconds * sr), 0.3f);
    auto measure = [&] (const char* name, const std::function<void (EngineSettings&)>& setup)
    {
        QuadEngine e; e.prepare (sr);
        EngineSettings s; s.algorithm = 9;
        for (auto& ec : s.echo) ec.on = false;
        setup (s);
        const auto t0 = std::chrono::steady_clock::now();
        auto out = runEngine (e, s, in, 512);
        const double el = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::cout << "  " << name << ": " << 100.0 * el / seconds << " %" << (finite (out, 10.0f) ? "" : "  (NICHT ENDLICH)") << std::endl;
    };
    auto units = [] (EngineSettings& s, int count)
    {
        for (int u = 0; u < count; ++u)
        {
            auto& c = s.echo[(size_t) u];
            c.on = true; c.feedback = 0.6f; c.timeL = 200.0f + 50.0f * (float) u;
            s.pos[(size_t) u] = { -0.6f + 0.4f * (float) u, 0.5f, 0.3f };
        }
    };
    measure ("alles aus (nur Durchreichen)", [&] (EngineSettings&) {});
    measure ("1 Einheit", [&] (EngineSettings& s) { units (s, 1); });
    measure ("4 Einheiten parallel", [&] (EngineSettings& s) { units (s, 4); });
    measure ("4 Einheiten + Filter/Mod/PingPong", [&] (EngineSettings& s) {
        units (s, 4);
        for (auto& c : s.echo) { c.hpOn = c.lpOn = true; c.delayDepth = 0.5f; c.filterDepth = 0.5f; c.mode = PingPong; }
    });
    measure ("... + Binaural, Reflexionen, Distanz-Delay", [&] (EngineSettings& s) {
        units (s, 4);
        for (auto& c : s.echo) { c.hpOn = c.lpOn = true; c.delayDepth = 0.5f; c.filterDepth = 0.5f; c.mode = PingPong; }
        s.binaural = true; s.earlyReflections = true; s.distanceDelay = true;
    });
    measure ("... + Drive (+24 dB, Schleife)", [&] (EngineSettings& s) {
        units (s, 4);
        for (auto& c : s.echo) { c.hpOn = c.lpOn = true; c.inGainDb = 24.0f; }
    });
    measure ("... + Noise + globaler Filter", [&] (EngineSettings& s) {
        units (s, 4);
        s.noise.on = true; s.noise.level = 0.5f; s.noise.crackle = 0.5f; s.noise.hpOn = s.noise.lpOn = true;
        s.gfilter.hpOn = s.gfilter.lpOn = true; s.gfilter.depth = 0.5f;
    });
    measure ("nur Noise (keine Einheit)", [&] (EngineSettings& s) { s.noise.on = true; s.noise.level = 0.5f; });
}

// ---------------------------------------------------------------------------
void renderAll (const juce::File& dir)
{
    std::cout << "[Offline-Renders -> " << dir.getFullPathName().toStdString() << "]" << std::endl;
    const double sr = 48000.0;
    const size_t n = (size_t) (4 * sr);
    const auto imp = impulse (n, 100);
    const auto plk = pluck (n, sr);
    int files = 0;

    // Jede Einheit einzeln (Impuls + Pluck), unterschiedliche Einstellungen
    for (int u = 0; u < kNumUnits; ++u)
    {
        for (int variant = 0; variant < 3; ++variant)
        {
            auto s = singleUnitSettings (u);
            auto& c = s.echo[(size_t) u];
            c.feedback = 0.5f; c.timeL = 200.0f + 60.0f * (float) u; c.mix = 0.6f;
            if (variant == 1) { c.mode = PingPong; c.link = false; c.timeR = c.timeL * 0.75f; c.lpOn = true; c.lpHz = 3000.0f; }
            if (variant == 2) { c.mode = MidSide; c.delayDepth = 0.6f; c.rateHz = 0.7f; c.wave = Triangle; c.sync = true; c.divL = 8; }
            s.pos[(size_t) u] = { -0.5f + 0.33f * (float) u, 0.1f, 0 };
            QuadEngine e; e.prepare (sr);
            const std::string name = std::string ("unit_") + (char) ('A' + u) + "_v" + std::to_string (variant);
            auto o1 = runEngine (e, s, imp, 512);
            check (finite (o1, 10.0f) && peakAbs (o1.l, 2000) > 0.001f, name + " Impuls liefert Signal");
            writeWav (dir.getChildFile (name + "_impulse.wav"), o1, sr); ++files;
            QuadEngine e2; e2.prepare (sr);
            writeWav (dir.getChildFile (name + "_pluck.wav"), runEngine (e2, s, plk, 512), sr); ++files;
        }
    }

    // Jeder Algorithmus
    for (int a = 0; a < kNumAlgorithms; ++a)
    {
        EngineSettings s; s.algorithm = a;
        const float times[4] = { 180, 270, 120, 90 };
        const float xs[4] = { -0.8f, -0.3f, 0.3f, 0.8f };
        for (int u = 0; u < kNumUnits; ++u)
        {
            s.echo[(size_t) u].on = true; s.echo[(size_t) u].timeL = times[u];
            s.echo[(size_t) u].feedback = 0.35f; s.echo[(size_t) u].mix = 0.6f;
            s.pos[(size_t) u] = { xs[u], 0.15f + 0.1f * (float) u, 0 };
        }
        char nm[64]; std::snprintf (nm, sizeof nm, "algo_%02d_%s", a + 1, kAlgorithms[a].name);
        std::string name (nm); for (auto& ch : name) if (ch == '+') ch = '_'; else if (ch == ' ') ch = '_';
        QuadEngine e; e.prepare (sr);
        auto o = runEngine (e, s, imp, 512);
        check (finite (o, 10.0f) && peakAbs (o.l, 2000) > 0.001f && peakAbs (o.r, 2000) > 0.001f, name + " Impuls liefert Signal (L " + std::to_string (peakAbs (o.l, 2000)) + ", R " + std::to_string (peakAbs (o.r, 2000)) + ", max " + std::to_string (peakAbs (o.l)) + ")");
        writeWav (dir.getChildFile (name + "_impulse.wav"), o, sr); ++files;
        QuadEngine e2; e2.prepare (sr);
        writeWav (dir.getChildFile (name + "_pluck.wav"), runEngine (e2, s, plk, 512), sr); ++files;
    }

    // Mehrere 3D-Positionen (Einheit A allein, Pluck)
    struct Pos { const char* name; PosSettings p; };
    const Pos positions[] = {
        { "x-1_y0_z0", { -1, 0, 0 } }, { "x0_y0_z0", { 0, 0, 0 } }, { "x+1_y0_z0", { 1, 0, 0 } },
        { "x0_y0.5_z0", { 0, 0.5f, 0 } }, { "x0_y1_z0", { 0, 1, 0 } },
        { "x0_y0_z-1", { 0, 0, -1 } }, { "x0_y0_z+1", { 0, 0, 1 } }, { "x-0.7_y0.6_z+0.5", { -0.7f, 0.6f, 0.5f } } };
    for (const auto& p : positions)
    {
        auto s = singleUnitSettings (0);
        s.echo[0].feedback = 0.45f; s.echo[0].timeL = 250.0f; s.echo[0].mix = 0.7f;
        s.pos[0] = p.p;
        QuadEngine e; e.prepare (sr);
        auto o = runEngine (e, s, plk, 512);
        check (finite (o, 10.0f) && peakAbs (o.l) + peakAbs (o.r) > 0.01f, std::string ("Position ") + p.name + " liefert Signal");
        writeWav (dir.getChildFile (std::string ("pos_") + p.name + "_pluck.wav"), o, sr); ++files;
        auto s2 = s; s2.distanceDelay = true;
        QuadEngine e2; e2.prepare (sr);
        writeWav (dir.getChildFile (std::string ("pos_") + p.name + "_impulse_distdelay.wav"), runEngine (e2, s2, imp, 512), sr); ++files;
    }
    std::cout << "  " << files << " WAV-Dateien geschrieben" << std::endl;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedNoDenormals noDenormals;
    if (argc > 1 && juce::String (argv[1]) == "--harness")
        return harnessMain (argc, argv);
    const juce::File outDir = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory().getChildFile ("test-renders");

    testDelayTimes();
    testFeedback();
    testModulation();
    testParameterClicks();
    testAlgorithms();
    testAlgorithmSwitch();
    testSpatial();
    testNewFeatures();
    testFilterSlope();
    testFuzz();
    testMixFreezeDuck();
    testPlacement();
    testAuditFixes();
    benchmark();
    benchmarkDetail();
    renderAll (outDir);

    std::cout << std::endl << gChecks << " Pruefungen, " << gFailures << " fehlgeschlagen" << std::endl;
    return gFailures == 0 ? 0 : 1;
}
