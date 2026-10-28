#include "Params.h"

namespace qe
{
namespace
{
using FloatP = juce::AudioParameterFloat;
using BoolP = juce::AudioParameterBool;
using ChoiceP = juce::AudioParameterChoice;
using Range = juce::NormalisableRange<float>;

juce::StringArray noteDivNames()
{
    juce::StringArray a;
    for (int i = 0; i < kNumNoteDivs; ++i) a.add (kNoteDivs[i].name);
    return a;
}

juce::AudioParameterFloatAttributes unit (const juce::String& label)
{
    return juce::AudioParameterFloatAttributes().withLabel (label);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<ChoiceP> (juce::ParameterID { kIdAlgo, 1 }, "Algorithm",
                                           [] { juce::StringArray a; for (auto& d : kAlgorithms) a.add (d.name); return a; }(), 9));
    layout.add (std::make_unique<FloatP> (juce::ParameterID { kIdGlobalMix, 1 }, "Global Dry/Wet", Range (0.0f, 1.0f, 0.001f), 0.5f, unit ("")));
    layout.add (std::make_unique<FloatP> (juce::ParameterID { kIdGlobalOut, 1 }, "Global Output", Range (-24.0f, 12.0f, 0.1f), 0.0f, unit ("dB")));
    layout.add (std::make_unique<BoolP> (juce::ParameterID { kIdDistDelay, 1 }, "Distance Delay", false));
    layout.add (std::make_unique<BoolP> (juce::ParameterID { kIdEarly, 1 }, "Early Reflections", true));
    layout.add (std::make_unique<BoolP> (juce::ParameterID { kIdBinaural, 1 }, "Binaural (Headphones)", false));
    layout.add (std::make_unique<FloatP> (juce::ParameterID { kIdDoppler, 1 }, "Doppler", Range (0.0f, 2.0f, 0.01f), 1.0f, unit ("")));
    layout.add (std::make_unique<FloatP> (juce::ParameterID { kIdGlobalFb, 1 }, "Feedback Scale", Range (0.0f, 1.0f, 0.001f), 1.0f, unit ("")));
    layout.add (std::make_unique<BoolP> (juce::ParameterID { kIdLimiter, 1 }, "Soft Limiter", true));
    layout.add (std::make_unique<BoolP> (juce::ParameterID { kIdLimitDry, 1 }, "Limiter On Dry", false));

    // ---- Expert: globaler Filter ----
    {
        static const juce::StringArray waves { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random" };
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("gfilter", "Global Filter", "|");
        auto id = [] (int k) { return juce::ParameterID { kGlobalIds[k], 1 }; };
        g->addChild (std::make_unique<BoolP> (id (gfHpOn), "GF HP On", false));
        g->addChild (std::make_unique<BoolP> (id (gfLpOn), "GF LP On", false));
        g->addChild (std::make_unique<FloatP> (id (gfHp), "GF HP Freq", Range (20.0f, 8000.0f, 0.1f, 0.3f), 100.0f, unit ("Hz")));
        g->addChild (std::make_unique<FloatP> (id (gfLp), "GF LP Freq", Range (200.0f, 20000.0f, 0.1f, 0.3f), 12000.0f, unit ("Hz")));
        g->addChild (std::make_unique<FloatP> (id (gfRes), "GF LP Resonance", Range (0.0f, 1.0f, 0.001f), 0.2f, unit ("")));
        g->addChild (std::make_unique<FloatP> (id (gfHpRes), "GF HP Resonance", Range (0.0f, 1.0f, 0.001f), 0.2f, unit ("")));
        g->addChild (std::make_unique<FloatP> (id (gfHpBoost), "GF HP Boost", Range (0.0f, 18.0f, 0.1f), 0.0f, unit ("dB")));
        g->addChild (std::make_unique<FloatP> (id (gfLpBoost), "GF LP Boost", Range (0.0f, 18.0f, 0.1f), 0.0f, unit ("dB")));
        g->addChild (std::make_unique<BoolP> (id (gfInclDry), "GF Include Dry", false));
        g->addChild (std::make_unique<BoolP> (id (gfSkipFirst), "GF Skip First Echo", false));
        g->addChild (std::make_unique<BoolP> (id (gfLfoOn), "GF LFO On", true));
        g->addChild (std::make_unique<ChoiceP> (id (gfWave), "GF LFO Wave", waves, 0));
        g->addChild (std::make_unique<FloatP> (id (gfRate), "GF LFO Rate", Range (0.01f, 20.0f, 0.001f, 0.4f), 0.5f, unit ("Hz")));
        g->addChild (std::make_unique<BoolP> (id (gfSync), "GF LFO Sync", false));
        g->addChild (std::make_unique<ChoiceP> (id (gfDiv), "GF LFO Division", noteDivNames(), 8));
        g->addChild (std::make_unique<FloatP> (id (gfDepth), "GF Mod Depth", Range (0.0f, 1.0f, 0.001f), 0.0f, unit ("")));
        g->addChild (std::make_unique<FloatP> (id (gfPhase), "GF LFO Phase R", Range (0.0f, 360.0f, 0.1f), 0.0f, unit ("deg")));
        layout.add (std::move (g));
    }

    // ---- Expert: Noise ----
    {
        static const juce::StringArray waves { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random" };
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("noise", "Noise", "|");
        auto id = [] (int k) { return juce::ParameterID { kGlobalIds[k], 1 }; };
        g->addChild (std::make_unique<BoolP> (id (nzOn), "Noise On", false));
        g->addChild (std::make_unique<FloatP> (id (nzLevel), "Noise Level", Range (0.0f, 1.0f, 0.001f), 0.3f, unit ("")));
        g->addChild (std::make_unique<FloatP> (id (nzColor), "Noise Color", Range (0.0f, 1.0f, 0.001f), 0.5f, unit ("")));
        g->addChild (std::make_unique<FloatP> (id (nzCrackle), "Noise Crackle", Range (0.0f, 1.0f, 0.001f), 0.0f, unit ("")));
        g->addChild (std::make_unique<FloatP> (id (nzWidth), "Noise Width", Range (0.0f, 1.0f, 0.001f), 1.0f, unit ("")));
        g->addChild (std::make_unique<FloatP> (id (nzInto), "Noise Into Echoes", Range (0.0f, 1.0f, 0.001f), 0.0f, unit ("")));
        g->addChild (std::make_unique<BoolP> (id (nzHpOn), "Noise HP On", false));
        g->addChild (std::make_unique<BoolP> (id (nzLpOn), "Noise LP On", false));
        g->addChild (std::make_unique<FloatP> (id (nzHp), "Noise HP Freq", Range (20.0f, 8000.0f, 0.1f, 0.3f), 200.0f, unit ("Hz")));
        g->addChild (std::make_unique<FloatP> (id (nzLp), "Noise LP Freq", Range (200.0f, 20000.0f, 0.1f, 0.3f), 8000.0f, unit ("Hz")));
        g->addChild (std::make_unique<FloatP> (id (nzRes), "Noise LP Resonance", Range (0.0f, 1.0f, 0.001f), 0.2f, unit ("")));
        g->addChild (std::make_unique<FloatP> (id (nzHpRes), "Noise HP Resonance", Range (0.0f, 1.0f, 0.001f), 0.2f, unit ("")));
        g->addChild (std::make_unique<FloatP> (id (nzHpBoost), "Noise HP Boost", Range (0.0f, 18.0f, 0.1f), 0.0f, unit ("dB")));
        g->addChild (std::make_unique<FloatP> (id (nzLpBoost), "Noise LP Boost", Range (0.0f, 18.0f, 0.1f), 0.0f, unit ("dB")));
        g->addChild (std::make_unique<BoolP> (id (nzLfoOn), "Noise LFO On", true));
        g->addChild (std::make_unique<ChoiceP> (id (nzWave), "Noise LFO Wave", waves, 0));
        g->addChild (std::make_unique<FloatP> (id (nzRate), "Noise LFO Rate", Range (0.01f, 20.0f, 0.001f, 0.4f), 0.5f, unit ("Hz")));
        g->addChild (std::make_unique<BoolP> (id (nzSync), "Noise LFO Sync", false));
        g->addChild (std::make_unique<ChoiceP> (id (nzDiv), "Noise LFO Division", noteDivNames(), 8));
        g->addChild (std::make_unique<FloatP> (id (nzDepthF), "Noise Mod Filter Depth", Range (0.0f, 1.0f, 0.001f), 0.0f, unit ("")));
        g->addChild (std::make_unique<FloatP> (id (nzDepthL), "Noise Mod Level Depth", Range (0.0f, 1.0f, 0.001f), 0.0f, unit ("")));
        g->addChild (std::make_unique<FloatP> (id (nzPhase), "Noise LFO Phase", Range (0.0f, 360.0f, 0.1f), 0.0f, unit ("deg")));
        g->addChild (std::make_unique<FloatP> (id (nzEnv), "Noise Envelope Mix", Range (0.0f, 1.0f, 0.001f), 0.0f, unit ("")));
        g->addChild (std::make_unique<FloatP> (id (nzPosX), "Noise Position X", Range (-1.0f, 1.0f, 0.001f), 0.0f, unit ("")));
        g->addChild (std::make_unique<FloatP> (id (nzPosY), "Noise Position Y", Range (-1.0f, 1.0f, 0.001f), 0.2f, unit ("")));
        g->addChild (std::make_unique<FloatP> (id (nzPosZ), "Noise Position Z", Range (-1.0f, 1.0f, 0.001f), 0.0f, unit ("")));
        layout.add (std::move (g));
    }

    // ---- Expert: Ducking ----
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("ducking", "Ducking", "|");
        auto id = [] (int k) { return juce::ParameterID { kGlobalIds[k], 1 }; };
        g->addChild (std::make_unique<BoolP> (id (dkOn), "Ducking On", false));
        g->addChild (std::make_unique<FloatP> (id (dkThreshold), "Ducking Threshold", Range (-60.0f, 0.0f, 0.1f), -30.0f, unit ("dB")));
        g->addChild (std::make_unique<FloatP> (id (dkAmount), "Ducking Amount", Range (0.0f, 40.0f, 0.1f), 12.0f, unit ("dB")));
        g->addChild (std::make_unique<FloatP> (id (dkAttack), "Ducking Attack", Range (0.1f, 200.0f, 0.01f, 0.4f), 10.0f, unit ("ms")));
        g->addChild (std::make_unique<FloatP> (id (dkRelease), "Ducking Release", Range (20.0f, 3000.0f, 0.1f, 0.4f), 400.0f, unit ("ms")));
        layout.add (std::move (g));
    }

    static const float defTime[kNumUnits] = { 250.0f, 375.0f, 500.0f, 125.0f };
    static const float defX[kNumUnits] = { -0.4f, -0.15f, 0.15f, 0.4f };
    static const float defY[kNumUnits] = { 0.3f, 0.6f, 0.6f, 0.3f };
    static const int defDiv[kNumUnits] = { 5, 6, 8, 2 };   // 1/8, 1/8., 1/4, 1/16
    static const int defSix[kNumUnits] = { 2, 3, 4, 1 };   // dieselben Zeiten in 16teln
    static const juce::StringArray divs = noteDivNames();

    for (int u = 0; u < kNumUnits; ++u)
    {
        const juce::String L = juce::String::charToString ((juce::juce_wchar) ('A' + u)) + " ";
        auto id = [u] (int p) { return juce::ParameterID { paramId (u, p), 1 }; };
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("unit" + juce::String (u), "Unit " + L.trim(), "|");

        group->addChild (std::make_unique<BoolP> (id (pOn), L + "On", u == 0));
        group->addChild (std::make_unique<BoolP> (id (pMute), L + "Mute", false));
        group->addChild (std::make_unique<BoolP> (id (pSolo), L + "Solo", false));
        group->addChild (std::make_unique<BoolP> (id (pSync), L + "Sync", true));
        group->addChild (std::make_unique<BoolP> (id (pLink), L + "Link", true));
        group->addChild (std::make_unique<FloatP> (id (pTimeL), L + "Time L", Range (1.0f, 4000.0f, 0.01f, 0.35f), defTime[u], unit ("ms")));
        group->addChild (std::make_unique<FloatP> (id (pTimeR), L + "Time R", Range (1.0f, 4000.0f, 0.01f, 0.35f), defTime[u], unit ("ms")));
        group->addChild (std::make_unique<ChoiceP> (id (pDivL), L + "Division L", divs, defDiv[u]));
        group->addChild (std::make_unique<ChoiceP> (id (pDivR), L + "Division R", divs, defDiv[u]));
        group->addChild (std::make_unique<ChoiceP> (id (pMode), L + "Channel Mode", juce::StringArray { "Stereo", "Ping Pong", "Mid/Side" }, 0));
        group->addChild (std::make_unique<FloatP> (id (pOffset), L + "Offset R", Range (-50.0f, 50.0f, 0.1f), 0.0f, unit ("%")));
        group->addChild (std::make_unique<FloatP> (id (pFb), L + "Feedback", Range (0.0f, 1.1f, 0.001f), 0.4f, unit ("")));
        group->addChild (std::make_unique<BoolP> (id (pFbInv), L + "Feedback Invert", false));
        group->addChild (std::make_unique<FloatP> (id (pInGain), L + "Input Gain", Range (-24.0f, 24.0f, 0.1f), 0.0f, unit ("dB")));
        group->addChild (std::make_unique<BoolP> (id (pInDry), L + "Gain on Dry", false));
        group->addChild (std::make_unique<BoolP> (id (pPosDry), L + "Place Dry", false));
        group->addChild (std::make_unique<BoolP> (id (pHpOn), L + "HP On", false));
        group->addChild (std::make_unique<BoolP> (id (pLpOn), L + "LP On", false));
        group->addChild (std::make_unique<FloatP> (id (pHp), L + "HP Freq", Range (20.0f, 8000.0f, 0.1f, 0.3f), 100.0f, unit ("Hz")));
        group->addChild (std::make_unique<FloatP> (id (pLp), L + "LP Freq", Range (200.0f, 20000.0f, 0.1f, 0.3f), 12000.0f, unit ("Hz")));
        group->addChild (std::make_unique<FloatP> (id (pRes), L + "LP Resonance", Range (0.0f, 1.0f, 0.001f), 0.2f, unit ("")));
        group->addChild (std::make_unique<ChoiceP> (id (pTrans), L + "Transition", juce::StringArray { "Repitch", "Fade", "Jump" }, 1));
        group->addChild (std::make_unique<FloatP> (id (pMix), L + "Echo Level", Range (0.0f, 1.0f, 0.001f), 1.0f, unit ("")));
        group->addChild (std::make_unique<FloatP> (id (pOut), L + "Output", Range (-24.0f, 12.0f, 0.1f), 0.0f, unit ("dB")));
        group->addChild (std::make_unique<FloatP> (id (pWidth), L + "Width", Range (0.0f, 2.0f, 0.001f), 1.0f, unit ("")));
        group->addChild (std::make_unique<BoolP> (id (pLfoOn), L + "LFO On", true));
        group->addChild (std::make_unique<ChoiceP> (id (pWave), L + "LFO Wave", juce::StringArray { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random" }, 0));
        group->addChild (std::make_unique<FloatP> (id (pRate), L + "LFO Rate", Range (0.01f, 20.0f, 0.001f, 0.4f), 1.0f, unit ("Hz")));
        group->addChild (std::make_unique<BoolP> (id (pLfoSync), L + "LFO Sync", false));
        group->addChild (std::make_unique<ChoiceP> (id (pLfoDiv), L + "LFO Division", divs, 8));
        group->addChild (std::make_unique<FloatP> (id (pPhase), L + "LFO Phase R", Range (0.0f, 360.0f, 0.1f), 0.0f, unit ("deg")));
        group->addChild (std::make_unique<FloatP> (id (pFPhase), L + "LFO Phase Filter", Range (0.0f, 360.0f, 0.1f), 0.0f, unit ("deg")));
        group->addChild (std::make_unique<BoolP> (id (pX4), L + "Mod x4", false));
        group->addChild (std::make_unique<FloatP> (id (pDDepth), L + "Mod Delay Depth", Range (0.0f, 1.0f, 0.001f), 0.0f, unit ("")));
        group->addChild (std::make_unique<FloatP> (id (pFDepth), L + "Mod Filter Depth", Range (0.0f, 1.0f, 0.001f), 0.0f, unit ("")));
        group->addChild (std::make_unique<FloatP> (id (pEnv), L + "Envelope Mix", Range (0.0f, 1.0f, 0.001f), 0.0f, unit ("")));
        group->addChild (std::make_unique<FloatP> (id (pPosX), L + "Position X", Range (-1.0f, 1.0f, 0.001f), defX[u], unit ("")));
        group->addChild (std::make_unique<FloatP> (id (pPosY), L + "Position Y", Range (-1.0f, 1.0f, 0.001f), defY[u], unit ("")));
        group->addChild (std::make_unique<FloatP> (id (pPosZ), L + "Position Z", Range (-1.0f, 1.0f, 0.001f), 0.0f, unit ("")));
        group->addChild (std::make_unique<BoolP> (id (pFreeze), L + "Freeze", false));
        group->addChild (std::make_unique<BoolP> (id (pFbStable), L + "Feedback Stable", false));
        group->addChild (std::make_unique<ChoiceP> (id (pSyncMode), L + "Sync Mode L", juce::StringArray { "Notes", "Triplet", "Dotted", "16th" }, Sync16th));
        group->addChild (std::make_unique<ChoiceP> (id (pSyncModeR), L + "Sync Mode R", juce::StringArray { "Notes", "Triplet", "Dotted", "16th" }, Sync16th));
        group->addChild (std::make_unique<juce::AudioParameterInt> (id (pSixL), L + "16ths L", 1, kMaxSixteenths, defSix[u]));
        group->addChild (std::make_unique<juce::AudioParameterInt> (id (pSixR), L + "16ths R", 1, kMaxSixteenths, defSix[u]));
        group->addChild (std::make_unique<FloatP> (id (pOffsetL), L + "Offset L", Range (-50.0f, 50.0f, 0.1f), 0.0f, unit ("%")));
        group->addChild (std::make_unique<FloatP> (id (pHpRes), L + "HP Resonance", Range (0.0f, 1.0f, 0.001f), 0.2f, unit ("")));
        group->addChild (std::make_unique<FloatP> (id (pHpBoost), L + "HP Boost", Range (0.0f, 18.0f, 0.1f), 0.0f, unit ("dB")));
        group->addChild (std::make_unique<FloatP> (id (pLpBoost), L + "LP Boost", Range (0.0f, 18.0f, 0.1f), 0.0f, unit ("dB")));

        layout.add (std::move (group));
    }
    return layout;
}

ParamReader::ParamReader (juce::AudioProcessorValueTreeState& apvts)
{
    for (int u = 0; u < kNumUnits; ++u)
        for (int p = 0; p < pCount; ++p)
            unit[(size_t) u][(size_t) p] = apvts.getRawParameterValue (paramId (u, p));
    algo = apvts.getRawParameterValue (kIdAlgo);
    gMix = apvts.getRawParameterValue (kIdGlobalMix);
    gOut = apvts.getRawParameterValue (kIdGlobalOut);
    distDelay = apvts.getRawParameterValue (kIdDistDelay);
    early = apvts.getRawParameterValue (kIdEarly);
    binaural = apvts.getRawParameterValue (kIdBinaural);
    doppler = apvts.getRawParameterValue (kIdDoppler);
    gFb = apvts.getRawParameterValue (kIdGlobalFb);
    limiter = apvts.getRawParameterValue (kIdLimiter);
    limitDry = apvts.getRawParameterValue (kIdLimitDry);
    for (int g = 0; g < gCount; ++g) glob[(size_t) g] = apvts.getRawParameterValue (kGlobalIds[g]);
}

void ParamReader::readUnit (int u, EchoSettings& c, PosSettings& pos) const
{
    const auto& a = unit[(size_t) u];
    auto f = [&a] (int p) { return a[(size_t) p]->load(); };
    auto b = [&f] (int p) { return f (p) > 0.5f; };
    auto i = [&f] (int p) { return (int) (f (p) + 0.5f); };

    c.on = b (pOn); c.mute = b (pMute); c.solo = b (pSolo); c.sync = b (pSync); c.link = b (pLink);
    c.timeL = f (pTimeL); c.timeR = f (pTimeR); c.divL = i (pDivL); c.divR = i (pDivR);
    c.mode = i (pMode); c.offset = f (pOffset); c.feedback = f (pFb); c.fbInvert = b (pFbInv);
    c.inGainDb = f (pInGain); c.inGainOnDry = b (pInDry); c.positionDry = b (pPosDry); c.hpOn = b (pHpOn); c.lpOn = b (pLpOn);
    c.hpHz = f (pHp); c.lpHz = f (pLp); c.res = f (pRes); c.transition = i (pTrans);
    c.mix = f (pMix); c.outDb = f (pOut); c.width = f (pWidth);
    c.wave = i (pWave); c.rateHz = f (pRate); c.lfoSync = b (pLfoSync); c.lfoDiv = i (pLfoDiv);
    c.phaseDeg = f (pPhase); c.lfoOn = b (pLfoOn); c.filterPhaseDeg = f (pFPhase); c.x4 = b (pX4); c.delayDepth = f (pDDepth);
    c.filterDepth = f (pFDepth); c.envMix = f (pEnv); c.freeze = b (pFreeze); c.stableFb = b (pFbStable);
    c.syncMode = i (pSyncMode); c.syncModeR = i (pSyncModeR); c.sixL = i (pSixL); c.sixR = i (pSixR); c.offsetL = f (pOffsetL); c.hpRes = f (pHpRes); c.hpBoost = f (pHpBoost); c.lpBoost = f (pLpBoost);
    pos = { f (pPosX), f (pPosY), f (pPosZ) };
}

void ParamReader::read (EngineSettings& s) const
{
    for (int u = 0; u < kNumUnits; ++u)
        readUnit (u, s.echo[(size_t) u], s.pos[(size_t) u]);
    s.algorithm = (int) (algo->load() + 0.5f);
    s.globalMix = gMix->load();
    s.globalOutDb = gOut->load();
    s.distanceDelay = distDelay->load() > 0.5f;
    s.earlyReflections = early->load() > 0.5f;
    s.binaural = binaural->load() > 0.5f;
    s.doppler = doppler->load();
    s.feedbackScale = gFb->load();
    s.softLimiter = limiter->load() > 0.5f;
    s.limitDry = limitDry->load() > 0.5f;

    auto gf = [this] (int k) { return glob[(size_t) k]->load(); };
    auto gb = [&gf] (int k) { return gf (k) > 0.5f; };
    auto gi = [&gf] (int k) { return (int) (gf (k) + 0.5f); };
    auto& f = s.gfilter;
    f.hpOn = gb (gfHpOn); f.lpOn = gb (gfLpOn); f.hpHz = gf (gfHp); f.lpHz = gf (gfLp); f.res = gf (gfRes); f.hpRes = gf (gfHpRes); f.hpBoost = gf (gfHpBoost); f.lpBoost = gf (gfLpBoost);
    f.includeDry = gb (gfInclDry); f.skipFirstEcho = gb (gfSkipFirst); f.lfoOn = gb (gfLfoOn); f.wave = gi (gfWave); f.rateHz = gf (gfRate);
    f.lfoSync = gb (gfSync); f.lfoDiv = gi (gfDiv); f.depth = gf (gfDepth); f.phaseDeg = gf (gfPhase);
    auto& n = s.noise;
    n.on = gb (nzOn); n.level = gf (nzLevel); n.color = gf (nzColor); n.crackle = gf (nzCrackle); n.width = gf (nzWidth);
    n.into = gf (nzInto); n.hpOn = gb (nzHpOn); n.lpOn = gb (nzLpOn); n.hpHz = gf (nzHp); n.lpHz = gf (nzLp); n.res = gf (nzRes); n.hpRes = gf (nzHpRes); n.hpBoost = gf (nzHpBoost); n.lpBoost = gf (nzLpBoost);
    n.lfoOn = gb (nzLfoOn); n.wave = gi (nzWave); n.rateHz = gf (nzRate); n.lfoSync = gb (nzSync); n.lfoDiv = gi (nzDiv);
    n.depthF = gf (nzDepthF); n.depthL = gf (nzDepthL); n.phaseDeg = gf (nzPhase); n.envMix = gf (nzEnv);
    n.pos = { gf (nzPosX), gf (nzPosY), gf (nzPosZ) };
    auto& d = s.duck;
    d.on = gb (dkOn); d.thresholdDb = gf (dkThreshold); d.amountDb = gf (dkAmount); d.attackMs = gf (dkAttack); d.releaseMs = gf (dkRelease);
}
} // namespace qe
