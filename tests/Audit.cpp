// Messungen fuer AUDIT.md (Phase 1). Aufruf: QuadEchoTests --harness audit <name|all> [ausgabeordner]
//   blocksize  Blockgroessen 1/32/64/441/4096/wechselnd gegen 512, alle Baseline-Faelle, 48 und 192 kHz
//   reset      dieselbe Instanz nach reset() bzw. prepare() gegen den ersten Render
//   numerics   Delay-Zeit nach Repitch (Phasenmessung) und Aufloesung des float-Index
//   robust     NaN/Inf/1e30 im Eingang, Denormals ohne FTZ, extreme Automation, Ausklingzeit
//   rtalloc    Speicheranforderungen waehrend process() (zaehlender operator new)
//   dumps      Rohdaten fuer tests/ref/audit_analysis.py (Schleifenverstaerkung, Hermite, Aliasing)
// Die Messungen pruefen nichts (kein Exit-Code), sie liefern Zahlen. Grenzwerte stehen in SPEC.md.
#include "HarnessCommon.h"
#include <atomic>
#include <complex>
#include <cstdlib>
#include <new>
#if defined(_M_X64) || defined(__x86_64__)
 #include <xmmintrin.h>
 #define QE_HAVE_MXCSR 1
#endif

// ---- zaehlender operator new (nur fuer rtalloc; im ganzen Test-Binary aktiv, zaehlt aber nur bei gesetztem Flag) ----
namespace
{
std::atomic<bool> gCountAllocs { false };
std::atomic<long> gAllocs { 0 };
}

void* operator new (std::size_t n)
{
    if (gCountAllocs.load (std::memory_order_relaxed)) gAllocs.fetch_add (1, std::memory_order_relaxed);
    if (void* p = std::malloc (n == 0 ? 1 : n)) return p;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t n) { return operator new (n); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

namespace harness
{
namespace
{
using Blocks = std::function<int (size_t blockIndex)>;

Sig renderBlocks (const Case& c, const Sig& in, double sr, const Blocks& nextBlock)
{
    auto e = std::make_unique<QuadEngine>();
    e->prepare (sr);
    Sig out = in;
    EngineSettings s = c.s;
    size_t pos = 0, k = 0;
    while (pos < in.l.size())
    {
        const int n = (int) std::min<size_t> ((size_t) nextBlock (k++), in.l.size() - pos);
        if (c.mut) c.mut (pos, s);
        e->process (out.l.data() + pos, out.r.data() + pos, n, s, c.bpm);
        pos += (size_t) n;
    }
    return out;
}

struct Diff { bool bitexact; double maxDb; double firstAt; };

Diff compare (const Sig& a, const Sig& b, double sr)
{
    double d = 0.0;
    size_t first = a.l.size();
    for (size_t i = 0; i < a.l.size(); ++i)
    {
        const double e = std::max (std::abs ((double) a.l[i] - b.l[i]), std::abs ((double) a.r[i] - b.r[i]));
        const bool same = std::memcmp (&a.l[i], &b.l[i], sizeof (float)) == 0 && std::memcmp (&a.r[i], &b.r[i], sizeof (float)) == 0;
        if (! same && first == a.l.size()) first = i;
        d = std::max (d, e);
    }
    return { first == a.l.size(), dbfs (d), (double) first / sr };
}

juce::String fmtDb (double db) { return db < -900.0 ? juce::String ("0") : juce::String (db, 1) + " dBFS"; }

// ---------------------------------------------------------------------------
void auditBlocksize()
{
    std::cout << "[Blockgroessen gegen Block 512 (Baseline-Raster); Faelle ohne / mit Automation getrennt]" << std::endl;
    struct Cfg { const char* name; Blocks f; };
    const std::vector<Cfg> cfgs = {
        { "1", [] (size_t) { return 1; } },
        { "32", [] (size_t) { return 32; } },
        { "64", [] (size_t) { return 64; } },
        { "441", [] (size_t) { return 441; } },
        { "4096", [] (size_t) { return 4096; } },
        { "wechselnd 1..1024", [] (size_t k) { juce::Random r ((juce::int64) (k * 7919 + 5)); return 1 + r.nextInt (1024); } },
    };
    for (double sr : { 48000.0, 192000.0 })
    {
        const auto cases = baselineCases (sr);
        for (const auto& cfg : cfgs)
        {
            int staticSame = 0, staticTotal = 0, autoSame = 0, autoTotal = 0;
            double worstStatic = -999.0, worstAuto = -999.0;
            std::string worstStaticName, worstAutoName;
            std::map<std::string, double> perCase;
            for (const auto& c : cases)
                for (int stim : { 0, 2 })
                {
                    const auto in = stimulus (stim, sr);
                    const auto ref = renderBlocks (c, in, sr, [] (size_t) { return kBaselineBlock; });
                    const auto out = renderBlocks (c, in, sr, cfg.f);
                    const auto d = compare (ref, out, sr);
                    const bool automated = (bool) c.mut;
                    (automated ? autoTotal : staticTotal)++;
                    if (d.bitexact) (automated ? autoSame : staticSame)++;
                    auto& worst = automated ? worstAuto : worstStatic;
                    auto& wn = automated ? worstAutoName : worstStaticName;
                    if (! d.bitexact && d.maxDb > worst) { worst = d.maxDb; wn = renderName (c, stim, sr); }
                    if (! d.bitexact) perCase[c.name] = std::max (perCase.count (c.name) ? perCase[c.name] : -999.0, d.maxDb);
                }
            std::cout << "  " << (int) sr << " Hz, Block " << cfg.name << ": ohne Automation " << staticSame << "/" << staticTotal
                      << " bitgenau (schlechteste " << fmtDb (worstStatic) << " " << worstStaticName << "), mit Automation "
                      << autoSame << "/" << autoTotal << " (schlechteste " << fmtDb (worstAuto) << " " << worstAutoName << ")" << std::endl;
            if (! perCase.empty())
            {
                std::cout << "    abweichend:";
                for (const auto& [name, db] : perCase) std::cout << " " << name << " " << juce::String (db, 0) << ";";
                std::cout << std::endl;
            }
        }
    }
}

// ---------------------------------------------------------------------------
void auditReset()
{
    std::cout << "[Dieselbe Instanz nach reset() bzw. prepare(): gleich dem ersten Render? (48 kHz, Rauschen und Impuls)]" << std::endl;
    const double sr = 48000.0;
    int sameReset = 0, samePrep = 0, total = 0;
    for (const auto& c : baselineCases (sr))
        for (int stim : { 0, 2 })
        {
            const auto in = stimulus (stim, sr);
            auto e = std::make_unique<QuadEngine>();
            e->prepare (sr);
            const auto a = runEngine (*e, c.s, in, kBaselineBlock, c.bpm, c.mut);
            e->reset();
            const auto b = runEngine (*e, c.s, in, kBaselineBlock, c.bpm, c.mut);
            e->prepare (sr);
            const auto p = runEngine (*e, c.s, in, kBaselineBlock, c.bpm, c.mut);
            const auto db = compare (a, b, sr), dp = compare (a, p, sr);
            ++total;
            sameReset += db.bitexact ? 1 : 0;
            samePrep += dp.bitexact ? 1 : 0;
            if (! db.bitexact || ! dp.bitexact)
                std::cout << "  " << renderName (c, stim, sr) << ": nach reset() " << (db.bitexact ? "gleich" : fmtDb (db.maxDb) + " ab " + juce::String (db.firstAt, 4) + " s")
                          << ", nach prepare() " << (dp.bitexact ? "gleich" : fmtDb (dp.maxDb) + " ab " + juce::String (dp.firstAt, 4) + " s") << std::endl;
        }
    std::cout << "  bitgenau: nach reset() " << sameReset << "/" << total << ", nach prepare() " << samePrep << "/" << total << std::endl;
}

// ---------------------------------------------------------------------------
void auditNumerics()
{
    std::cout << "[Delay-Zeit nach einem Zeitsprung: Soll gegen Ist (Phasenmessung, 8 s nach dem Sprung)]" << std::endl;
    struct J { float from, to; };
    for (double sr : { 48000.0, 192000.0 })
        for (const J j : { J { 100.0f, 350.0f }, J { 350.0f, 100.0f }, J { 1000.0f, 4000.0f }, J { 3000.0f, 3999.0f }, J { 3999.0f, 3000.0f } })
            for (int t : { Repitch, Fade })
            {
                const double soll = (double) j.to * sr / 1000.0;
                const double ist = measureDelaySamples (sr, j.from, j.to, t, 8.0);
                std::cout << "  " << (int) sr << " Hz, " << j.from << " -> " << j.to << " ms, " << kTransNames[t] << ": Soll "
                          << juce::String (soll, 3) << ", Ist " << juce::String (ist, 3) << " Samples, Abweichung "
                          << juce::String (ist - soll, 3) << " Samples = " << juce::String (1000.0 * (ist - soll) / sr, 4) << " ms" << std::endl;
            }

    std::cout << "[Aufloesung des float-Delay-Index (Abstand benachbarter float-Werte)]" << std::endl;
    for (double sr : kRates)
        for (double sec : { 0.1, 1.0, 4.0 })
        {
            const float d = (float) (sec * sr);
            const double ulp = (double) std::nextafter (d, 2.0f * d) - (double) d;
            std::cout << "  " << (int) sr << " Hz, " << sec << " s (" << (long) d << " Samples): " << ulp << " Samples = "
                      << juce::String (1.0e9 * ulp / sr, 1) << " ns" << std::endl;
        }
}

// ---------------------------------------------------------------------------
void auditRobust()
{
    const double sr = 48000.0;
    std::cout << "[Nicht-endlicher Eingang: ein einzelnes Sample links bei 0,5 s; Impuls bei 3 s prueft, ob danach wieder Echos kommen]" << std::endl;
    struct Cfg { const char* name; EngineSettings s; };
    auto single = singleUnitSettings (0);
    single.echo[0].feedback = 0.5f; single.echo[0].timeL = 200.0f;
    auto frozen = single;
    frozen.echo[0].freeze = true;
    auto gf = algoBase (0, Stereo);
    gf.gfilter.lpOn = true;
    const std::vector<Cfg> cfgs = { { "1 Einheit", single }, { "Serial, 4 Einheiten + globaler Filter", gf }, { "Worst Case", worstCase() } };
    struct Bad { const char* name; float v; };
    for (const auto& cfg : cfgs)
        for (const Bad bad : { Bad { "NaN", std::numeric_limits<float>::quiet_NaN() }, Bad { "+Inf", std::numeric_limits<float>::infinity() }, Bad { "1e30", 1.0e30f } })
        {
            const size_t n = (size_t) (4.0 * sr);
            auto in = noise ((size_t) (0.4 * sr), 0.1f, 9);
            in.l.resize (n, 0.0f); in.r.resize (n, 0.0f);
            in.l[(size_t) (0.5 * sr)] = bad.v;
            in.l[(size_t) (3.0 * sr)] = in.r[(size_t) (3.0 * sr)] = 0.5f;
            auto e = std::make_unique<QuadEngine>();
            e->prepare (sr);
            const auto out = runEngine (*e, cfg.s, in, 512);
            size_t bad1 = 0, last = 0;
            for (size_t i = 0; i < n; ++i)
                if (! std::isfinite (out.l[i]) || ! std::isfinite (out.r[i])) { ++bad1; last = i; }
            const float pk = peakAbs (out.l, (size_t) (3.0 * sr) + 10, n);
            std::cout << "  " << cfg.name << ", " << bad.name << ": " << bad1 << " nicht-endliche Ausgangs-Samples"
                      << (bad1 > 0 ? " (bis " + juce::String ((double) last / sr, 3) + " s)" : juce::String())
                      << ", Echo-Spitze nach dem Impuls bei 3 s: " << (std::isfinite (pk) ? juce::String (dbfs (pk), 1) + " dBFS" : juce::String ("nicht endlich")) << std::endl;
        }

#if QE_HAVE_MXCSR
    std::cout << "[Denormals: Ausklang nach Impuls, 40 s Stille, CPU-Zeit mit und ohne FTZ/DAZ]" << std::endl;
    for (const auto& cfg : { Cfg { "1 Einheit, Feedback 0.9, HP+LP", [] { auto s = singleUnitSettings (0); s.echo[0].feedback = 0.9f; s.echo[0].hpOn = s.echo[0].lpOn = true; return s; }() },
                             Cfg { "Worst Case", worstCase() } })
    {
        double cyc[2] {};
        for (int ftz = 0; ftz < 2; ++ftz)
        {
            const unsigned int old = _mm_getcsr();
            _mm_setcsr (ftz ? (old | 0x8040u) : (old & ~0x8040u));
            const size_t n = (size_t) (40.0 * sr);
            auto in = impulse (n, 100);
            auto s = cfg.s;
            s.noise.on = false;
            auto e = std::make_unique<QuadEngine>();
            e->prepare (sr);
            Sig out = in;
            for (size_t pos = 0; pos < n; pos += 512)
            {
                const int len = (int) std::min<size_t> (512, n - pos);
                const auto c0 = threadCycles();
                e->process (out.l.data() + pos, out.r.data() + pos, len, s, 120.0);
                if (pos >= (size_t) (20.0 * sr)) cyc[ftz] += (double) (threadCycles() - c0);
            }
            _mm_setcsr (old);
        }
        std::cout << "  " << cfg.name << ": ohne FTZ " << juce::String (cyc[0] / 1.0e6, 1) << " Mio. Zyklen, mit FTZ "
                  << juce::String (cyc[1] / 1.0e6, 1) << " Mio. (Faktor " << juce::String (cyc[0] / std::max (cyc[1], 1.0), 2) << ")" << std::endl;
    }
#endif

    std::cout << "[Extreme Automation: jeder Parameter springt bei jedem 32er-Block zufaellig zwischen Minimum und Maximum, 30 s]" << std::endl;
    for (int lim = 0; lim < 2; ++lim)
    {
        juce::Random rnd (77);
        auto ext = [&] (float a, float b) { return rnd.nextBool() ? a : b; };
        auto e = std::make_unique<QuadEngine>();
        e->prepare (sr);
        const size_t n = (size_t) (30.0 * sr);
        auto io = noise (n, 0.5f, 4);
        EngineSettings s;
        float pk = 0.0f;
        size_t nonFinite = 0;
        for (size_t pos = 0; pos < n; pos += 32)
        {
            for (auto& c : s.echo)
            {
                c.on = rnd.nextBool(); c.feedback = ext (0.0f, 1.1f); c.fbInvert = rnd.nextBool(); c.timeL = ext (1.0f, 4000.0f); c.timeR = ext (1.0f, 4000.0f);
                c.link = rnd.nextBool(); c.mode = rnd.nextInt (3); c.transition = rnd.nextInt (3); c.inGainDb = ext (-24.0f, 24.0f); c.inGainOnDry = rnd.nextBool();
                c.hpOn = rnd.nextBool(); c.lpOn = rnd.nextBool(); c.hpHz = ext (20.0f, 8000.0f); c.lpHz = ext (200.0f, 20000.0f);
                c.res = ext (0.0f, 1.0f); c.hpRes = ext (0.0f, 1.0f); c.hpBoost = ext (0.0f, 18.0f); c.lpBoost = ext (0.0f, 18.0f);
                c.mix = ext (0.0f, 1.0f); c.outDb = ext (-24.0f, 12.0f); c.width = ext (0.0f, 2.0f); c.delayDepth = ext (0.0f, 1.0f); c.x4 = rnd.nextBool();
                c.filterDepth = ext (0.0f, 1.0f); c.envMix = ext (0.0f, 1.0f); c.rateHz = ext (0.01f, 20.0f); c.wave = rnd.nextInt (kNumWaves); c.freeze = rnd.nextInt (8) == 0;
                c.offset = ext (-50.0f, 50.0f); c.offsetL = ext (-50.0f, 50.0f);
            }
            s.algorithm = rnd.nextInt (kNumAlgorithms); s.globalMix = ext (0.0f, 1.0f); s.globalOutDb = ext (-24.0f, 12.0f);
            s.feedbackScale = ext (0.0f, 1.0f); s.binaural = rnd.nextBool(); s.distanceDelay = rnd.nextBool(); s.doppler = ext (0.0f, 2.0f);
            for (auto& p : s.pos) p = { ext (-1.0f, 1.0f), ext (-1.0f, 1.0f), ext (-1.0f, 1.0f) };
            s.gfilter.hpOn = rnd.nextBool(); s.gfilter.lpOn = rnd.nextBool(); s.gfilter.res = ext (0.0f, 1.0f); s.gfilter.lpBoost = ext (0.0f, 18.0f); s.gfilter.depth = ext (0.0f, 1.0f);
            s.noise.on = rnd.nextBool(); s.noise.level = ext (0.0f, 1.0f); s.noise.into = ext (0.0f, 1.0f);
            s.duck.on = rnd.nextBool();
            s.softLimiter = lim != 0;
            const int len = (int) std::min<size_t> (32, n - pos);
            e->process (io.l.data() + pos, io.r.data() + pos, len, s, 120.0);
            for (size_t i = pos; i < pos + (size_t) len; ++i)
            {
                if (! std::isfinite (io.l[i]) || ! std::isfinite (io.r[i])) ++nonFinite;
                else pk = std::max ({ pk, std::abs (io.l[i]), std::abs (io.r[i]) });
            }
        }
        std::cout << "  Limiter " << (lim ? "an" : "aus") << ": " << nonFinite << " nicht-endliche Samples, Spitze " << juce::String (dbfs (pk), 1) << " dBFS" << std::endl;
    }

    std::cout << "[Ausklingzeit bis unter -100 dBFS (Spitze je 100 ms) nach einem Impuls von -6 dBFS; gemeldet werden 10 s]" << std::endl;
    struct T { float fb, ms; bool lp; };
    for (const T t : { T { 0.5f, 500.0f, false }, T { 0.8f, 1000.0f, false }, T { 0.95f, 1000.0f, false }, T { 0.95f, 4000.0f, false }, T { 0.95f, 1000.0f, true }, T { 1.0f, 500.0f, false } })
    {
        auto s = singleUnitSettings (0);
        s.globalMix = 1.0f;
        s.echo[0].feedback = t.fb; s.echo[0].timeL = t.ms; s.echo[0].lpOn = t.lp; s.echo[0].lpHz = 3000.0f;
        const double maxSec = 600.0;
        const size_t win = (size_t) (0.1 * sr);
        auto e = std::make_unique<QuadEngine>();
        e->prepare (sr);
        std::vector<float> l (win), r (win);
        double lastLoud = 0.0;
        for (size_t blk = 0; (double) (blk * win) / sr < maxSec; ++blk)
        {
            std::fill (l.begin(), l.end(), 0.0f); std::fill (r.begin(), r.end(), 0.0f);
            if (blk == 0) l[0] = r[0] = 0.5f;
            for (size_t p = 0; p < win; p += 512)
            {
                const int len = (int) std::min<size_t> (512, win - p);
                e->process (l.data() + p, r.data() + p, len, s, 120.0);
            }
            float pk = 0.0f;
            for (size_t i = 0; i < win; ++i) pk = std::max ({ pk, std::abs (l[i]), std::abs (r[i]) });
            if (pk > 1.0e-5f) lastLoud = (double) ((blk + 1) * win) / sr;
            if (lastLoud + 30.0 < (double) (blk * win) / sr) break;   // 30 s still: fertig
        }
        std::cout << "  Feedback " << t.fb << ", " << t.ms << " ms" << (t.lp ? ", LP 3 kHz" : "") << ": "
                  << (lastLoud >= maxSec - 1.0 ? juce::String ("> 600 s (klingt nicht aus)") : juce::String (lastLoud, 1) + " s") << std::endl;
    }
}

// ---------------------------------------------------------------------------
void auditRtAlloc()
{
    std::cout << "[Speicheranforderungen waehrend process(), 192 kHz, Block 32, je 3 s]" << std::endl;
    const double sr = 192000.0;
    {
        gAllocs = 0;
        gCountAllocs = true;
        auto probe = std::make_unique<std::vector<float>> (16);
        gCountAllocs = false;
        std::cout << "  Selbstprobe des Zaehlers (2 Anforderungen erwartet): " << gAllocs.load() << std::endl;
    }
    struct Scn { const char* name; EngineSettings s; Mutator mut; };
    const std::vector<Scn> scns = {
        { "Standard", EngineSettings(), {} },
        { "Worst Case", worstCase(), {} },
        { "Worst Case + Einheit an/aus + Algorithmuswechsel", worstCase(),
          [sr] (size_t p, EngineSettings& st) { st.echo[1].on = (p / (size_t) (0.3 * sr)) % 2 == 0; st.algorithm = (int) (p / (size_t) (0.5 * sr)) % kNumAlgorithms; } },
    };
    for (const auto& sc : scns)
    {
        auto e = std::make_unique<QuadEngine>();
        e->prepare (sr);
        const size_t n = (size_t) (3.0 * sr);
        auto io = noise (n, 0.3f, 2);
        EngineSettings s = sc.s;
        gAllocs = 0;
        for (size_t pos = 0; pos < n; pos += 32)
        {
            if (sc.mut) sc.mut (pos, s);
            gCountAllocs = true;
            e->process (io.l.data() + pos, io.r.data() + pos, 32, s, 120.0);
            gCountAllocs = false;
        }
        std::cout << "  " << sc.name << ": " << gAllocs.load() << " Anforderungen" << std::endl;
    }
}

// ---------------------------------------------------------------------------
// Rohdaten fuer audit_analysis.py
void dumpUnit (const juce::File& dir, const std::string& name, double sr, const EchoSettings& s, const Sig& in)
{
    DelayUnit u;
    u.prepare (sr);
    Sig out = in, wet = in;
    for (size_t pos = 0; pos < in.l.size(); pos += 512)
    {
        const int len = (int) std::min<size_t> (512, in.l.size() - pos);
        u.process (in.l.data() + pos, in.r.data() + pos, out.l.data() + pos, out.r.data() + pos, len, s, 120.0, wet.l.data() + pos, wet.r.data() + pos);
    }
    auto* o = new juce::DynamicObject();
    o->setProperty ("name", juce::String (name));
    o->setProperty ("sr", sr);
    o->setProperty ("n", (int) in.l.size());
    o->setProperty ("delaySamples", (double) s.timeL * sr / 1000.0);
    o->setProperty ("settings", echoToVar (s));
    dir.getChildFile (name + ".json").replaceWithText (juce::JSON::toString (juce::var (o)), false, false, "\n");
    writeF32 (dir.getChildFile (name + "_in.f32"), in);
    writeF32 (dir.getChildFile (name + "_wet.f32"), wet);
}

void auditDumps (const juce::File& dir)
{
    dir.createDirectory();
    int count = 0;
    // Schleifenverstaerkung: Impuls -60 dBFS, 200 ms (ganzzahlig), Feedback 0.5. Echo 2 / Echo 1 = 0.5 * H(Filter).
    for (double sr : { 48000.0, 192000.0 })
    {
        for (float res : { 0.0f, 0.5f, 1.0f })
            for (float boost : { 0.0f, 6.0f, 12.0f })
            {
                EchoSettings s = wetOnly();
                s.timeL = 200.0f; s.feedback = 0.5f;
                s.hpOn = s.lpOn = true; s.hpHz = 150.0f; s.lpHz = 2500.0f;
                s.res = s.hpRes = res; s.hpBoost = s.lpBoost = boost;
                const auto in = [&] { auto x = impulse ((size_t) (0.65 * sr), 0); x.l[0] = x.r[0] = 0.001f; return x; }();
                dumpUnit (dir, "loop_res" + juce::String (res, 1).toStdString() + "_boost" + std::to_string ((int) boost) + "_" + std::to_string ((int) sr), sr, s, in);
                ++count;
            }
        EchoSettings lp = wetOnly();
        lp.timeL = 200.0f; lp.feedback = 0.5f; lp.lpOn = true; lp.lpHz = 12000.0f; lp.res = 1.0f; lp.lpBoost = 18.0f;
        auto in = impulse ((size_t) (0.65 * sr), 0); in.l[0] = in.r[0] = 0.001f;
        dumpUnit (dir, "loop_lp12k_res1.0_boost18_" + std::to_string ((int) sr), sr, lp, in);
        ++count;
        // Hermite: Nachkommaanteil 0, 0.25, 0.5 ohne Filter
        for (double frac : { 0.0, 0.25, 0.5 })
        {
            EchoSettings h = wetOnly();
            h.feedback = 0.5f;
            h.timeL = (float) ((std::floor (0.2 * sr) + frac) * 1000.0 / sr);
            dumpUnit (dir, "hermite_frac" + juce::String (frac, 2).toStdString() + "_" + std::to_string ((int) sr), sr, h, in);
            ++count;
        }
    }
    // Aliasing der Schleifen-Saettigung: 7-kHz-Sinus, 0,3 s, Feedback 0.9, 300 ms
    for (double sr : { 48000.0, 96000.0 })
        for (float amp : { 0.1f, 0.5f })
            for (float drive : { 0.0f, 24.0f })
            {
                EchoSettings s = wetOnly();
                s.timeL = 300.0f; s.feedback = 0.9f; s.inGainDb = drive;
                const size_t n = (size_t) (3.0 * sr);
                auto in = makeSig (n);
                for (size_t i = 0; i < (size_t) (0.25 * sr); ++i)
                    in.l[i] = in.r[i] = amp * (float) std::sin (6.283185307179586 * 7000.0 * (double) i / sr);
                dumpUnit (dir, "alias_amp" + juce::String (amp, 1).toStdString() + "_drive" + std::to_string ((int) drive) + "_" + std::to_string ((int) sr), sr, s, in);
                ++count;
            }
    std::cout << count << " Dumps nach " << dir.getFullPathName() << std::endl;
}
} // namespace

// Delay-Zeit per Phasenvergleich: 20-Hz-Sinus, DFT ueber ganze Perioden der letzten Sekunde, Echo = Eingang um D verschoben.
double measureDelaySamples (double sr, float fromMs, float toMs, int transition, double settleSeconds)
{
    const double f = 20.0;
    const size_t n = (size_t) ((1.0 + settleSeconds + 1.0) * sr);
    auto in = makeSig (n);
    for (size_t i = 0; i < n; ++i) in.l[i] = in.r[i] = (float) (0.1 * std::sin (6.283185307179586 * f * (double) i / sr));
    DelayUnit u;
    u.prepare (sr);
    EchoSettings s = wetOnly();
    s.transition = transition;
    s.timeL = fromMs;
    Sig out = in, wet = in;
    for (size_t pos = 0; pos < n; pos += 512)
    {
        const int len = (int) std::min<size_t> (512, n - pos);
        if (pos >= (size_t) sr) s.timeL = toMs;
        u.process (in.l.data() + pos, in.r.data() + pos, out.l.data() + pos, out.r.data() + pos, len, s, 120.0, wet.l.data() + pos, wet.r.data() + pos);
    }
    const size_t w = (size_t) sr, n0 = n - w;
    std::complex<double> X, Y;
    const double om = 6.283185307179586 * f / sr;
    for (size_t i = n0; i < n; ++i)
    {
        const std::complex<double> e (std::cos (om * (double) i), -std::sin (om * (double) i));
        X += (double) in.l[i] * e;
        Y += (double) wet.l[i] * e;
    }
    double lag = std::arg (X) - std::arg (Y);   // = om * D (mod 2 pi)
    const double period = sr / f, expect = (double) toMs * sr / 1000.0;
    double D = lag / om;
    D += period * std::round ((expect - D) / period);
    return D;
}

int auditMain (const juce::String& which, const juce::File& outDir)
{
    const bool all = which == "all";
    if (all || which == "blocksize") auditBlocksize();
    if (all || which == "reset")     auditReset();
    if (all || which == "numerics")  auditNumerics();
    if (all || which == "robust")    auditRobust();
    if (all || which == "rtalloc")   auditRtAlloc();
    if (all || which == "dumps")     auditDumps (outDir);
    return 0;
}
} // namespace harness
