// Referenz-Harness fuer das DSP-Audit, Aufruf siehe Harness.h.
// Baseline: feste Matrix aus Faellen (jeder Algorithmus x Kanalmodus, jede Transition x Kanalmodus, Einzelfunktionen)
// x Anregung (Impuls, Sweep, Rauschen) x Samplerate (44.1/48/96/192 kHz), Blockgroesse 512, 120 BPM, je 1,5 s.
// Jeder spaetere Commit wird dagegen geprueft: Refactors muessen bitgenau nullen.
#if defined(_WIN32)
 #define NOMINMAX
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
#endif
#include "Harness.h"
#include "HarnessCommon.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <map>
#include <memory>
#include <numeric>
#if defined(_M_X64) || defined(__x86_64__)
 #if defined(_MSC_VER)
  #include <intrin.h>
 #else
  #include <x86intrin.h>
 #endif
 #define QE_HAVE_TSC 1
#endif

using namespace qetest;

namespace harness
{
// ---------------------------------------------------------------------------
// Anregungen: Impuls -6 dBFS bei Sample 64; exponentieller Sweep 20 Hz .. min(20 kHz, 0.45 fs) in 1 s bei -12 dBFS;
// weisses Rauschen (L/R unabhaengig) 1 s bei -12 dBFS. Danach Stille fuer die Ausklingphase.
Sig stimulus (int kind, double sr)
{
    const size_t n = (size_t) (kBaselineSeconds * sr), active = (size_t) sr;
    if (kind == 0) { auto s = makeSig (n); s.l[64] = s.r[64] = 0.5f; return s; }
    if (kind == 1)
    {
        auto s = makeSig (n);
        const double f1 = 20.0, f2 = std::min (20000.0, 0.45 * sr), T = 1.0, k = std::log (f2 / f1);
        const size_t fade = (size_t) (0.002 * sr);
        for (size_t i = 0; i < active; ++i)
        {
            const double t = (double) i / sr;
            double g = 0.25;
            if (i < fade) g *= 0.5 - 0.5 * std::cos (3.141592653589793 * (double) i / (double) fade);
            if (active - i < fade) g *= 0.5 - 0.5 * std::cos (3.141592653589793 * (double) (active - i) / (double) fade);
            s.l[i] = s.r[i] = (float) (g * std::sin (6.283185307179586 * f1 * T / k * (std::exp (t / T * k) - 1.0)));
        }
        return s;
    }
    auto s = noise (active, 0.25f, 1);
    s.l.resize (n, 0.0f);
    s.r.resize (n, 0.0f);
    return s;
}


// Vier verschiedene Einheiten: A mit Drive, B mit HP, C mit LP + Boost, D moduliert. Feste Zeiten (keine Transition).
EngineSettings algoBase (int algo, int mode)
{
    EngineSettings s;
    s.algorithm = algo;
    const float times[4] = { 70.0f, 110.0f, 160.0f, 230.0f };
    const PosSettings pos[4] = { { -0.6f, 0.5f, 0.0f }, { 0.6f, 0.5f, 0.2f }, { -0.3f, -0.4f, -0.3f }, { 0.3f, 0.8f, 0.5f } };
    for (int u = 0; u < kNumUnits; ++u)
    {
        auto& c = s.echo[(size_t) u];
        c.on = true; c.mode = mode; c.link = false;
        c.timeL = times[u]; c.timeR = times[u] * 1.27f;
        c.feedback = 0.45f; c.mix = 0.8f;
        s.pos[(size_t) u] = pos[u];
    }
    s.echo[0].inGainDb = 6.0f;
    s.echo[1].hpOn = true; s.echo[1].hpHz = 250.0f; s.echo[1].hpRes = 0.4f;
    s.echo[2].lpOn = true; s.echo[2].lpHz = 3000.0f; s.echo[2].res = 0.6f; s.echo[2].lpBoost = 4.0f;
    s.echo[3].delayDepth = 0.3f; s.echo[3].rateHz = 1.3f; s.echo[3].wave = Triangle;
    return s;
}

std::vector<Case> baselineCases (double sr)
{
    std::vector<Case> cs;
    const auto at = [sr] (double seconds) { return (size_t) (seconds * sr); };

    for (int a = 0; a < kNumAlgorithms; ++a)
        for (int m = 0; m < 3; ++m)
        {
            char nm[64];
            std::snprintf (nm, sizeof (nm), "algo%02d_%s", a + 1, kModeNames[m]);
            cs.push_back ({ nm, algoBase (a, m), {} });
        }

    for (int t = 0; t < 3; ++t)
        for (int m = 0; m < 3; ++m)
        {
            auto s = singleUnitSettings (0);
            auto& c = s.echo[0];
            c.transition = t; c.mode = m; c.timeL = 120.0f; c.feedback = 0.5f;
            cs.push_back ({ std::string ("trans_") + kTransNames[t] + "_" + kModeNames[m], s,
                            [at] (size_t p, EngineSettings& st) { st.echo[0].timeL = p >= at (0.9) ? 90.0f : (p >= at (0.4) ? 180.0f : 120.0f); } });
        }

    auto single = [] (const std::function<void (EchoSettings&)>& f)
    {
        auto s = singleUnitSettings (0);
        s.echo[0].feedback = 0.5f; s.echo[0].timeL = 130.0f;
        f (s.echo[0]);
        return s;
    };
    auto withEngine = [] (EngineSettings s, const std::function<void (EngineSettings&)>& f) { f (s); return s; };

    cs.push_back ({ "feat_freeze", single ([] (EchoSettings& c) { c.feedback = 0.6f; }),
                    [at] (size_t p, EngineSettings& st) { st.echo[0].freeze = p >= at (0.5) && p < at (1.2); } });
    cs.push_back ({ "feat_freeze_invert", single ([] (EchoSettings& c) { c.feedback = 0.6f; c.fbInvert = true; }),
                    [at] (size_t p, EngineSettings& st) { st.echo[0].freeze = p >= at (0.5) && p < at (1.2); } });
    cs.push_back ({ "feat_duck", withEngine (algoBase (9, Stereo), [] (EngineSettings& s) { s.duck.on = true; s.duck.thresholdDb = -30.0f; s.duck.amountDb = 12.0f; }), {} });
    cs.push_back ({ "feat_noise_into", withEngine (single ([] (EchoSettings&) {}), [] (EngineSettings& s) {
                        auto& nz = s.noise; nz.on = true; nz.level = 0.5f; nz.crackle = 0.3f; nz.into = 0.5f;
                        nz.hpOn = nz.lpOn = true; nz.depthF = 0.5f; nz.depthL = 0.3f; }), {} });
    cs.push_back ({ "feat_gfilter", withEngine (algoBase (0, Stereo), [] (EngineSettings& s) {
                        auto& g = s.gfilter; g.hpOn = g.lpOn = true; g.hpHz = 300.0f; g.lpHz = 4000.0f; g.res = 0.5f; g.lpBoost = 3.0f;
                        g.includeDry = true; g.skipFirstEcho = true; g.depth = 0.4f; g.rateHz = 0.7f; }), {} });
    for (int bin = 0; bin < 2; ++bin)
        cs.push_back ({ bin ? "feat_binaural_move" : "feat_stereo_move",
                        withEngine (algoBase (9, Stereo), [bin] (EngineSettings& s) { s.binaural = bin != 0; s.distanceDelay = true; s.doppler = 1.0f; }),
                        [sr] (size_t p, EngineSettings& st) { st.pos[0].x = -1.0f + 2.0f * (float) ((double) p / (kBaselineSeconds * sr)); } });
    cs.push_back ({ "feat_drive24", single ([] (EchoSettings& c) { c.inGainDb = 24.0f; c.inGainOnDry = true; c.feedback = 0.7f; }), {} });
    for (int lim = 0; lim < 2; ++lim)
        cs.push_back ({ lim ? "feat_fb110_res_limiter" : "feat_fb110_res_nolimiter",
                        withEngine (single ([] (EchoSettings& c) {
                            c.feedback = 1.1f; c.hpOn = c.lpOn = true; c.hpHz = 200.0f; c.lpHz = 2500.0f; c.res = c.hpRes = 1.0f; c.hpBoost = c.lpBoost = 12.0f; }),
                            [lim] (EngineSettings& s) { s.softLimiter = lim != 0; }), {} });
    for (int w = 0; w < kNumWaves; ++w)
        cs.push_back ({ "feat_lfo_wave" + std::to_string (w), single ([w] (EchoSettings& c) {
                            c.wave = w; c.delayDepth = 0.5f; c.filterDepth = 0.5f; c.lpOn = true; c.lpHz = 2000.0f; c.res = 0.5f;
                            c.rateHz = 3.0f; c.phaseDeg = 90.0f; c.filterPhaseDeg = 180.0f; }), {} });
    cs.push_back ({ "feat_lfo_sync_x4", single ([] (EchoSettings& c) { c.lfoSync = true; c.lfoDiv = 5; c.x4 = true; c.delayDepth = 0.3f; }), {}, 123.0 });
    cs.push_back ({ "feat_sync_times", single ([] (EchoSettings& c) { c.sync = true; c.link = false; c.divL = 6; c.syncModeR = Sync16th; c.sixR = 3; }), {}, 123.0 });
    cs.push_back ({ "feat_envmix", single ([] (EchoSettings& c) { c.envMix = 0.7f; c.delayDepth = 0.4f; c.filterDepth = 0.6f; c.lpOn = true; c.lpHz = 3000.0f; }), {} });
    cs.push_back ({ "feat_mix0", withEngine (algoBase (3, Stereo), [] (EngineSettings& s) { s.globalMix = 0.0f; }), {} });
    cs.push_back ({ "feat_mix0_nolimiter", withEngine (algoBase (3, Stereo), [] (EngineSettings& s) { s.globalMix = 0.0f; s.softLimiter = false; }), {} });
    cs.push_back ({ "feat_mix1", withEngine (algoBase (3, Stereo), [] (EngineSettings& s) { s.globalMix = 1.0f; }), {} });
    cs.push_back ({ "feat_placedry", withEngine (algoBase (9, Stereo), [] (EngineSettings& s) { for (auto& e : s.echo) e.positionDry = true; }), {} });
    cs.push_back ({ "feat_solo_mute", withEngine (algoBase (0, Stereo), [] (EngineSettings& s) { s.echo[1].solo = true; s.echo[2].mute = true; }), {} });
    cs.push_back ({ "feat_unit_toggle", algoBase (0, Stereo),
                    [at] (size_t p, EngineSettings& st) { st.echo[1].on = ! (p >= at (0.3) && p < at (0.8)); } });
    cs.push_back ({ "feat_algo_switch", algoBase (0, Stereo),
                    [at] (size_t p, EngineSettings& st) { st.algorithm = (int) (p / at (0.25)) % kNumAlgorithms; } });
    cs.push_back ({ "feat_width_offset", single ([] (EchoSettings& c) { c.width = 1.8f; c.offset = 20.0f; c.offsetL = -10.0f; }), {} });
    cs.push_back ({ "feat_fbscale_out", withEngine (algoBase (1, Stereo), [] (EngineSettings& s) { s.feedbackScale = 0.5f; s.globalOutDb = -3.0f; }), {} });
    return cs;
}

std::string renderName (const Case& c, int stim, double sr)
{
    return c.name + "_" + kStimNames[stim] + "_" + std::to_string ((int) sr);
}

Sig renderCase (const Case& c, int stim, double sr)
{
    auto e = std::make_unique<QuadEngine>();
    e->prepare (sr);
    return runEngine (*e, c.s, stimulus (stim, sr), kBaselineBlock, c.bpm, c.mut);
}

// FNV-1a 64 ueber die Bitmuster (erst L, dann R)
uint64_t hashSig (const Sig& s)
{
    uint64_t h = 1469598103934665603ull;
    for (const auto* v : { &s.l, &s.r })
    {
        const auto* p = reinterpret_cast<const unsigned char*> (v->data());
        for (size_t i = 0; i < v->size() * sizeof (float); ++i) { h ^= p[i]; h *= 1099511628211ull; }
    }
    return h;
}

std::string hex64 (uint64_t v) { char b[20]; std::snprintf (b, sizeof (b), "%016llx", (unsigned long long) v); return b; }

double dbfs (double x) { return x > 0.0 ? 20.0 * std::log10 (x) : -999.0; }

void writeWavFloat (const juce::File& f, const Sig& s, double sr)
{
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream> (f);
    juce::WavAudioFormat fmt;
    auto w = fmt.createWriterFor (os, juce::AudioFormatWriterOptions {}.withSampleRate (sr).withNumChannels (2).withBitsPerSample (32));
    if (w == nullptr) { std::cout << "  WAV-Writer fehlgeschlagen: " << f.getFullPathName() << std::endl; return; }
    const int n = (int) s.l.size();
    juce::AudioBuffer<float> buf (2, n);
    buf.copyFrom (0, 0, s.l.data(), n);
    buf.copyFrom (1, 0, s.r.data(), n);
    w->writeFromAudioSampleBuffer (buf, 0, n);
}

bool readWavFloat (const juce::File& f, Sig& s)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr || r->numChannels != 2) return false;
    const int n = (int) r->lengthInSamples;
    juce::AudioBuffer<float> buf (2, n);
    r->read (&buf, 0, n, 0, true, true);
    s = makeSig ((size_t) n);
    std::copy (buf.getReadPointer (0), buf.getReadPointer (0) + n, s.l.begin());
    std::copy (buf.getReadPointer (1), buf.getReadPointer (1) + n, s.r.begin());
    return true;
}

template <typename F>
void forEachRender (F&& f)
{
    for (double sr : kRates)
        for (const auto& c : baselineCases (sr))
            for (int stim = 0; stim < 3; ++stim)
                f (c, stim, sr);
}

int baselineWrite (const juce::File& dir, const juce::File& manifestCopy)
{
    dir.createDirectory();
    juce::String manifest = "# QUAD ECHO Baseline v1: name\tsr\tframes\tfnv1a64\tpeak_dBFS\trms_dBFS\n";
    int count = 0;
    forEachRender ([&] (const Case& c, int stim, double sr)
    {
        const auto out = renderCase (c, stim, sr);
        const auto name = renderName (c, stim, sr);
        writeWavFloat (dir.getChildFile (name + ".wav"), out, sr);
        double pk = 0.0, e = 0.0;
        for (const auto* v : { &out.l, &out.r })
            for (float x : *v) { pk = std::max (pk, (double) std::abs (x)); e += (double) x * x; }
        manifest << name << "\t" << (int) sr << "\t" << (int) out.l.size() << "\t" << hex64 (hashSig (out)) << "\t"
                 << juce::String (dbfs (pk), 2) << "\t" << juce::String (dbfs (std::sqrt (e / (2.0 * (double) out.l.size()))), 2) << "\n";
        if (++count % 100 == 0) std::cout << "  " << count << " Renders" << std::endl;
    });
    dir.getChildFile ("manifest.tsv").replaceWithText (manifest, false, false, "\n");
    if (manifestCopy != juce::File()) manifestCopy.replaceWithText (manifest, false, false, "\n");
    std::cout << count << " Renders nach " << dir.getFullPathName() << std::endl;
    return 0;
}

// Vergleich: bitgenau? sonst groesste Abweichung in dBFS. Mit WAV-Ordner oder nur gegen die Pruefsummen des Manifests.
int baselineCheck (const juce::File& dirOrManifest, bool hashesOnly)
{
    std::map<std::string, std::string> hashes;
    if (hashesOnly)
    {
        juce::StringArray lines;
        lines.addLines (dirOrManifest.loadFileAsString());
        for (const auto& ln : lines)
        {
            if (ln.startsWith ("#") || ln.trim().isEmpty()) continue;
            juce::StringArray t;
            t.addTokens (ln, "\t", "");
            if (t.size() >= 4) hashes[t[0].toStdString()] = t[3].toStdString();
        }
    }
    int same = 0, differ = 0, missing = 0;
    double worst = -999.0;
    std::string worstName;
    forEachRender ([&] (const Case& c, int stim, double sr)
    {
        const auto out = renderCase (c, stim, sr);
        const auto name = renderName (c, stim, sr);
        if (hashesOnly)
        {
            auto it = hashes.find (name);
            if (it == hashes.end()) { ++missing; std::cout << "  FEHLT: " << name << std::endl; return; }
            if (it->second == hex64 (hashSig (out))) ++same;
            else { ++differ; std::cout << "  ANDERS: " << name << std::endl; }
            return;
        }
        Sig ref;
        if (! readWavFloat (dirOrManifest.getChildFile (name + ".wav"), ref) || ref.l.size() != out.l.size())
        {
            ++missing; std::cout << "  FEHLT: " << name << std::endl; return;
        }
        double d = 0.0;
        for (size_t i = 0; i < out.l.size(); ++i)
            d = std::max ({ d, (double) std::abs (out.l[i] - ref.l[i]), (double) std::abs (out.r[i] - ref.r[i]) });
        const bool bitexact = std::memcmp (out.l.data(), ref.l.data(), out.l.size() * sizeof (float)) == 0
                           && std::memcmp (out.r.data(), ref.r.data(), out.r.size() * sizeof (float)) == 0;
        if (bitexact) { ++same; return; }
        ++differ;
        std::cout << "  ANDERS: " << name << "  max. Abweichung " << dbfs (d) << " dBFS" << std::endl;
        if (dbfs (d) > worst) { worst = dbfs (d); worstName = name; }
    });
    std::cout << same << " bitgenau, " << differ << " abweichend, " << missing << " fehlend";
    if (differ > 0 && ! hashesOnly) std::cout << " (groesste: " << worst << " dBFS in " << worstName << ")";
    std::cout << std::endl;
    return differ + missing == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------
// Dumps fuer das Python-Referenzmodell: eine DelayUnit allein, ohne Drive (Eingangs-Saettigung aus),
// Anregung -20 dBFS (Rauschstoss 0..0.2 s, Impuls bei 0.3 s). Pro Fall: JSON mit Einstellungen und Aenderungen,
// Eingang und Ausgang (nur Echos) als float32, kanalweise hintereinander.
juce::var echoToVar (const EchoSettings& c)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("on", c.on); o->setProperty ("sync", c.sync); o->setProperty ("link", c.link);
    o->setProperty ("timeL", c.timeL); o->setProperty ("timeR", c.timeR);
    o->setProperty ("divL", c.divL); o->setProperty ("divR", c.divR);
    o->setProperty ("syncMode", c.syncMode); o->setProperty ("syncModeR", c.syncModeR);
    o->setProperty ("sixL", c.sixL); o->setProperty ("sixR", c.sixR);
    o->setProperty ("mode", c.mode); o->setProperty ("offset", c.offset); o->setProperty ("offsetL", c.offsetL);
    o->setProperty ("feedback", c.feedback); o->setProperty ("fbInvert", c.fbInvert);
    o->setProperty ("inGainDb", c.inGainDb); o->setProperty ("inGainOnDry", c.inGainOnDry);
    o->setProperty ("hpOn", c.hpOn); o->setProperty ("lpOn", c.lpOn);
    o->setProperty ("hpHz", c.hpHz); o->setProperty ("lpHz", c.lpHz); o->setProperty ("res", c.res); o->setProperty ("hpRes", c.hpRes);
    o->setProperty ("hpBoost", c.hpBoost); o->setProperty ("lpBoost", c.lpBoost);
    o->setProperty ("transition", c.transition); o->setProperty ("mix", c.mix);
    o->setProperty ("outDb", c.outDb); o->setProperty ("width", c.width);
    o->setProperty ("wave", c.wave); o->setProperty ("rateHz", c.rateHz);
    o->setProperty ("lfoSync", c.lfoSync); o->setProperty ("lfoDiv", c.lfoDiv);
    o->setProperty ("phaseDeg", c.phaseDeg); o->setProperty ("x4", c.x4);
    o->setProperty ("delayDepth", c.delayDepth); o->setProperty ("filterDepth", c.filterDepth); o->setProperty ("envMix", c.envMix);
    o->setProperty ("lfoOn", c.lfoOn); o->setProperty ("filterPhaseDeg", c.filterPhaseDeg); o->setProperty ("freeze", c.freeze);
    return juce::var (o);
}

void writeF32 (const juce::File& f, const Sig& s)
{
    f.deleteFile();
    juce::FileOutputStream os (f);
    os.write (s.l.data(), s.l.size() * sizeof (float));
    os.write (s.r.data(), s.r.size() * sizeof (float));
}

int refDump (const juce::File& dir)
{
    dir.createDirectory();
    struct RefCase
    {
        std::string name;
        std::function<void (EchoSettings&)> setup;
        double seconds = 1.0;
        double changeAt = -1.0;   // s: timeL springt hier auf changeTo
        float changeTo = 0.0f;
    };
    const std::vector<RefCase> cases = {
        { "r01_integer",   [] (EchoSettings& c) { c.timeL = 100.0f; c.feedback = 0.5f; } },
        { "r02_fraction",  [] (EchoSettings& c) { c.link = false; c.timeL = 100.37f; c.timeR = 133.21f; c.feedback = 0.6f; } },
        { "r03_filters",   [] (EchoSettings& c) { c.link = false; c.timeL = 100.37f; c.timeR = 133.21f; c.feedback = 0.8f;
                                                  c.hpOn = c.lpOn = true; c.hpHz = 150.0f; c.hpRes = 0.5f; c.hpBoost = 6.0f;
                                                  c.lpHz = 2500.0f; c.res = 0.8f; c.lpBoost = 9.0f; } },
        { "r04_pingpong",  [] (EchoSettings& c) { c.mode = PingPong; c.timeL = 87.3f; c.feedback = 0.6f; c.width = 0.7f; } },
        { "r05_midside",   [] (EchoSettings& c) { c.mode = MidSide; c.link = false; c.timeL = 91.1f; c.timeR = 143.9f; c.feedback = 0.5f; c.width = 1.5f; } },
        { "r06_lfo",       [] (EchoSettings& c) { c.timeL = 120.0f; c.feedback = 0.5f; c.delayDepth = 0.4f; c.rateHz = 2.3f; c.phaseDeg = 90.0f;
                                                  c.filterDepth = 0.5f; c.lpOn = true; c.lpHz = 3000.0f; c.res = 0.3f; c.filterPhaseDeg = 45.0f; } },
        { "r07_envelope",  [] (EchoSettings& c) { c.timeL = 120.0f; c.feedback = 0.5f; c.envMix = 0.6f; c.delayDepth = 0.3f;
                                                  c.filterDepth = 0.5f; c.lpOn = true; c.lpHz = 3000.0f; } },
        { "r08_repitch",   [] (EchoSettings& c) { c.transition = Repitch; c.timeL = 100.0f; c.feedback = 0.4f; }, 1.5, 0.25, 350.0f },
        { "r09_fade",      [] (EchoSettings& c) { c.transition = Fade;    c.timeL = 100.0f; c.feedback = 0.4f; }, 1.5, 0.25, 350.0f },
        { "r10_jump",      [] (EchoSettings& c) { c.transition = Jump;    c.timeL = 100.0f; c.feedback = 0.4f; }, 1.5, 0.25, 350.0f },
        { "r11_fbinvert",  [] (EchoSettings& c) { c.timeL = 77.7f; c.feedback = 0.7f; c.fbInvert = true; c.outDb = -3.0f; c.mix = 0.8f; } },
        { "r12_tri_x4",    [] (EchoSettings& c) { c.timeL = 150.0f; c.feedback = 0.5f; c.wave = Triangle; c.x4 = true; c.delayDepth = 0.2f; c.rateHz = 1.1f; } },
        { "r13_random",    [] (EchoSettings& c) { c.timeL = 150.0f; c.feedback = 0.5f; c.wave = SmoothRandom; c.delayDepth = 0.3f; c.rateHz = 4.0f; } },
        { "r14_filters_stable", [] (EchoSettings& c) { c.timeL = 100.0f; c.feedback = 0.5f; c.hpOn = c.lpOn = true; c.hpHz = 150.0f; c.hpRes = 0.2f;
                                                       c.lpHz = 2500.0f; c.res = 0.3f; c.lpBoost = 3.0f; } },
    };
    for (double sr : kRates)
        for (const auto& rc : cases)
        {
            EchoSettings s = wetOnly();
            rc.setup (s);
            const size_t n = (size_t) (rc.seconds * sr);
            auto in = noise ((size_t) (0.2 * sr), 0.1f, 7);
            in.l.resize (n, 0.0f); in.r.resize (n, 0.0f);
            in.l[(size_t) (0.3 * sr)] = in.r[(size_t) (0.3 * sr)] = 0.1f;

            DelayUnit u;
            u.prepare (sr);
            Sig wet = in, out = in;
            const size_t changeAt = rc.changeAt >= 0.0 ? (size_t) (rc.changeAt * sr) : n + 1;
            EchoSettings cur = s;
            for (size_t pos = 0; pos < n; pos += (size_t) kBaselineBlock)
            {
                const int len = (int) std::min<size_t> ((size_t) kBaselineBlock, n - pos);
                if (pos >= changeAt) cur.timeL = rc.changeTo;
                u.process (in.l.data() + pos, in.r.data() + pos, out.l.data() + pos, out.r.data() + pos, len, cur, 120.0,
                           wet.l.data() + pos, wet.r.data() + pos);
            }
            const std::string base = rc.name + "_" + std::to_string ((int) sr);
            auto* o = new juce::DynamicObject();
            o->setProperty ("name", juce::String (rc.name));
            o->setProperty ("sr", sr);
            o->setProperty ("n", (int) n);
            o->setProperty ("block", kBaselineBlock);
            o->setProperty ("bpm", 120.0);
            o->setProperty ("settings", echoToVar (s));
            juce::Array<juce::var> changes;
            if (rc.changeAt >= 0.0)
            {
                auto* ch = new juce::DynamicObject();
                ch->setProperty ("at", (int) changeAt);
                ch->setProperty ("field", "timeL");
                ch->setProperty ("value", rc.changeTo);
                changes.add (juce::var (ch));
            }
            o->setProperty ("changes", changes);
            dir.getChildFile (base + ".json").replaceWithText (juce::JSON::toString (juce::var (o)), false, false, "\n");
            writeF32 (dir.getChildFile (base + "_in.f32"), in);
            writeF32 (dir.getChildFile (base + "_wet.f32"), wet);
        }
    std::cout << cases.size() * 4 << " Faelle nach " << dir.getFullPathName() << std::endl;
    return 0;
}

// ---------------------------------------------------------------------------
// Zeit pro Block. Der Messthread laeuft auf einem festen Kern mit hoher Prioritaet; die erste Sekunde zaehlt nicht.
// "worst": alles an, was Rechenzeit kostet (4 Einheiten parallel mit Filtern + Boost, Modulation, Drive, Freeze aus,
// Binaural, Distanz, Reflexionen, Noise, globaler Filter mit Skip 1st echo, Ducking) und laufende Ueberblendungen.
EngineSettings worstCase()
{
    EngineSettings s = algoBase (9, MidSide);
    for (auto& c : s.echo)
    {
        c.hpOn = c.lpOn = true; c.res = c.hpRes = 0.8f; c.hpBoost = c.lpBoost = 6.0f;
        c.delayDepth = 0.5f; c.filterDepth = 0.5f; c.envMix = 0.3f; c.inGainDb = 24.0f; c.inGainOnDry = true;
        c.feedback = 0.8f; c.positionDry = true; c.wave = SmoothRandom;
    }
    s.binaural = true; s.distanceDelay = true; s.earlyReflections = true;
    auto& nz = s.noise; nz.on = true; nz.level = 0.5f; nz.crackle = 0.5f; nz.into = 0.5f; nz.hpOn = nz.lpOn = true; nz.lpBoost = 6.0f; nz.depthF = 0.5f;
    auto& g = s.gfilter; g.hpOn = g.lpOn = true; g.lpBoost = 6.0f; g.depth = 0.5f; g.skipFirstEcho = true; g.includeDry = true;
    s.duck.on = true;
    return s;
}

struct BenchResult { double p50, p99, p999, max, mean, wallMax, cyclesPerSample; std::vector<std::pair<double, size_t>> top; };

// CPU-Zeit des Threads je Block (Windows: QueryThreadCycleTime, umgerechnet ueber die TSC-Frequenz). Zeiten, in denen
// Windows den Thread unterbricht, zaehlen so nicht mit. Die Wanduhr laeuft zum Vergleich mit (wallMax).
uint64_t threadCycles()
{
#if defined(_WIN32)
    ULONG64 c = 0;
    QueryThreadCycleTime (GetCurrentThread(), &c);
    return (uint64_t) c;
#else
    return 0;
#endif
}

BenchResult benchRun (double sr, int block, double seconds, const EngineSettings& s0, const Mutator& mut)
{
    auto e = std::make_unique<QuadEngine>();
    e->prepare (sr);
    const size_t n = (size_t) (seconds * sr), warm = (size_t) sr;
    auto io = noise (n, 0.3f, 3);
    EngineSettings s = s0;
    std::vector<double> wall, cyc;
    std::vector<size_t> at;
    wall.reserve (n / (size_t) block + 1); cyc.reserve (n / (size_t) block + 1); at.reserve (n / (size_t) block + 1);
    size_t counted = 0;
    const auto runStart = std::chrono::steady_clock::now();
#if QE_HAVE_TSC
    const auto tscStart = __rdtsc();
#endif
    for (size_t pos = 0; pos < n; pos += (size_t) block)
    {
        const int len = (int) std::min<size_t> ((size_t) block, n - pos);
        if (mut) mut (pos, s);
        const auto c0 = threadCycles();
        const auto t0 = std::chrono::steady_clock::now();
        e->process (io.l.data() + pos, io.r.data() + pos, len, s, 120.0);
        const auto t1 = std::chrono::steady_clock::now();
        const auto c1 = threadCycles();
        if (pos < warm) continue;
        wall.push_back (std::chrono::duration<double, std::micro> (t1 - t0).count());
        cyc.push_back ((double) (c1 - c0));
        at.push_back (pos);
        counted += (size_t) len;
    }
    const double runUs = std::chrono::duration<double, std::micro> (std::chrono::steady_clock::now() - runStart).count();
    // Zyklen -> Mikrosekunden; ohne Zyklenzaehler gilt die Wanduhr
    double usPerCycle = 0.0;
#if QE_HAVE_TSC
    usPerCycle = runUs / (double) (__rdtsc() - tscStart);
#endif
    std::vector<double> us (wall.size());
    for (size_t k = 0; k < us.size(); ++k) us[k] = cyc[k] > 0.0 && usPerCycle > 0.0 ? cyc[k] * usPerCycle : wall[k];

    BenchResult r {};
    std::vector<size_t> idx (us.size());
    std::iota (idx.begin(), idx.end(), 0);
    std::sort (idx.begin(), idx.end(), [&] (size_t x, size_t y) { return us[x] < us[y]; });
    auto pct = [&] (double p) { return us[idx[std::min (idx.size() - 1, (size_t) (p * (double) idx.size()))]]; };
    r.p50 = pct (0.5); r.p99 = pct (0.99); r.p999 = pct (0.999); r.max = us[idx.back()];
    r.mean = std::accumulate (us.begin(), us.end(), 0.0) / (double) us.size();
    r.wallMax = *std::max_element (wall.begin(), wall.end());
    r.cyclesPerSample = std::accumulate (cyc.begin(), cyc.end(), 0.0) / (double) std::max<size_t> (counted, 1);
    for (size_t k = 0; k < std::min<size_t> (5, idx.size()); ++k)
        r.top.push_back ({ us[idx[idx.size() - 1 - k]], at[idx[idx.size() - 1 - k]] });
    return r;
}

int bench (double seconds, int runs)
{
    juce::Thread::setCurrentThreadAffinityMask (1u << 2);   // P-Kern (i5-13600KF: logische Kerne 0..11)
    juce::Process::setPriority (juce::Process::HighPriority);
    struct Scn { std::string name; EngineSettings s; std::function<Mutator (double)> mut; };
    const auto fades = [] (double sr) -> Mutator {   // Zeitwechsel alle 70 ms: Fade-Ueberblendungen laufen fast immer
        return [sr] (size_t p, EngineSettings& st) {
            const bool odd = (p / (size_t) (0.07 * sr)) % 2 != 0;
            for (int u = 0; u < kNumUnits; ++u) st.echo[(size_t) u].timeL = (odd ? 150.0f : 100.0f) + 20.0f * (float) u;
        };
    };
    const auto toggles = [] (double sr) -> Mutator {   // Einheit B alle 0,5 s an/aus (reset() beim Ausschalten)
        return [sr] (size_t p, EngineSettings& st) { st.echo[1].on = (p / (size_t) (0.5 * sr)) % 2 == 0; };
    };
    const auto algos = [] (double sr) -> Mutator {
        return [sr] (size_t p, EngineSettings& st) { st.algorithm = (int) (p / (size_t) (0.5 * sr)) % kNumAlgorithms; };
    };
    const std::vector<Scn> scns = {
        { "default (Serial, 4 Einheiten, Standardwerte)", EngineSettings(), nullptr },
        { "worst", worstCase(), nullptr },
        { "worst + Fades", worstCase(), fades },
        { "worst + Einheit an/aus", worstCase(), toggles },
        { "worst + Algorithmuswechsel", worstCase(), algos },
    };
    std::cout << "[CPU-Zeit des Threads pro Block, " << runs << " Laeufe zu " << seconds
              << " s, Median ueber die Laeufe, erste Sekunde ausgelassen]" << std::endl;
    for (const auto& cfg : { std::pair<double, int> { 192000.0, 32 }, { 48000.0, 32 }, { 44100.0, 512 } })
    {
        const double deadline = 1.0e6 * cfg.second / cfg.first;
        std::cout << (int) cfg.first << " Hz, Block " << cfg.second << " (Frist " << juce::String (deadline, 1) << " us)" << std::endl;
        for (const auto& sc : scns)
        {
            std::vector<BenchResult> rs;
            for (int k = 0; k < runs; ++k) rs.push_back (benchRun (cfg.first, cfg.second, seconds, sc.s, sc.mut ? sc.mut (cfg.first) : Mutator()));
            auto med = [&rs] (double BenchResult::* f)
            {
                std::vector<double> v;
                for (const auto& x : rs) v.push_back (x.*f);
                std::sort (v.begin(), v.end());
                return v[v.size() / 2];
            };
            auto pc = [deadline] (double v) { return juce::String (100.0 * v / deadline, 1) + " %"; };
            std::cout << "  " << sc.name << ": Mittel " << pc (med (&BenchResult::mean))
                      << ", p50 " << pc (med (&BenchResult::p50)) << ", p99 " << pc (med (&BenchResult::p99))
                      << ", p99.9 " << pc (med (&BenchResult::p999)) << ", max " << pc (med (&BenchResult::max))
                      << " (" << juce::String (med (&BenchResult::max), 1) << " us; Wanduhr max " << pc (med (&BenchResult::wallMax)) << ")"
                      << ", " << juce::String (med (&BenchResult::cyclesPerSample), 0) << " Zyklen/Sample" << std::endl;
            std::cout << "    langsamste Bloecke (letzter Lauf):";
            for (const auto& t : rs.back().top) std::cout << " " << juce::String (t.first, 1) << " us @ " << juce::String ((double) t.second / cfg.first, 3) << " s;";
            std::cout << std::endl;
        }
    }
    return 0;
}
} // namespace harness

using namespace harness;

int harnessMain (int argc, char** argv)
{
    const juce::String cmd = argc > 2 ? juce::String (argv[2]) : juce::String();
    const juce::String arg = argc > 3 ? juce::String (argv[3]) : juce::String();
    const auto file = [] (const juce::String& p) { return juce::File::getCurrentWorkingDirectory().getChildFile (p); };
    if (cmd == "baseline-write" && arg.isNotEmpty()) return baselineWrite (file (arg), argc > 4 ? file (argv[4]) : juce::File());
    if (cmd == "baseline-check" && arg.isNotEmpty()) return baselineCheck (file (arg), false);
    if (cmd == "manifest-check" && arg.isNotEmpty()) return baselineCheck (file (arg), true);
    if (cmd == "refdump" && arg.isNotEmpty())        return refDump (file (arg));
    if (cmd == "audit")                              return auditMain (arg, argc > 4 ? file (argv[4]) : file ("audit-out"));
    if (cmd == "bench")                              return bench (arg.isNotEmpty() ? arg.getDoubleValue() : 20.0,
                                                                  argc > 4 ? juce::String (argv[4]).getIntValue() : 5);
    std::cout << "Aufruf: siehe tests/Harness.h" << std::endl;
    return 2;
}
