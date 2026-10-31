#include "Presets.h"
#include "Params.h"
#include "StateMigration.h"

namespace qe
{
namespace
{
struct Setter
{
    juce::AudioProcessorValueTreeState& apvts;

    void set (const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }
    void unitParam (int u, int p, float value) { set (paramId (u, p), value); }
    void unitOn (int u, bool on) { unitParam (u, pOn, on ? 1.0f : 0.0f); }
    void pos (int u, float x, float y, float z = 0.0f)
    {
        unitParam (u, pPosX, x); unitParam (u, pPosY, y); unitParam (u, pPosZ, z);
    }
    // Notenwert (Index in kNoteDivs), eingestellt im 16tel-Modus (Standard): Notenwert und 16tel passen zusammen
    void div (int u, int idx)
    {
        unitParam (u, pSync, 1.0f); unitParam (u, pDivL, (float) idx); unitParam (u, pDivR, (float) idx);
        const float six = (float) (divBeatsOf (idx) * 4.0);
        unitParam (u, pSixL, six); unitParam (u, pSixR, six);
    }
};

constexpr int kDiv16 = 2, kDiv8 = 5, kDiv8Dot = 6, kDiv4 = 8, kDiv2 = 11;

const char* const kNames[] = { "Init", "Slap", "Ping Pong", "Dotted Eighth", "Tape Chain", "Wide Space", "Binaural Orbit",
                               "Tape Wobble", "Phase Chain", "Glitch Steps", "Chorus Space", "Dub Siren",
                               "Filtered Dub", "Distant Atmosphere", "Total Chaos" };
constexpr int kDiv2Dot = 12, kDiv1Bar = 14, kDiv2Bars = 16;
} // namespace

int numPresets() { return (int) (sizeof (kNames) / sizeof (kNames[0])); }

juce::String presetName (int index)
{
    return juce::isPositiveAndBelow (index, numPresets()) ? juce::String (kNames[index]) : juce::String();
}

void applyPreset (juce::AudioProcessorValueTreeState& apvts, int index)
{
    if (apvts.undoManager != nullptr) apvts.undoManager->beginNewTransaction ("Preset");

    // alles auf Standard
    for (auto* p : apvts.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            r->setValueNotifyingHost (r->getDefaultValue());

    // Echo-Pegel je Einheit (pMix) steht auf 100 %, das Verhaeltnis Original/Echos setzt der globale Dry/Wet
    Setter s { apvts };
    switch (index)
    {
        case 1: // Slap: kurzer Slapback
            s.unitParam (0, pSync, 0.0f); s.unitParam (0, pTimeL, 95.0f);
            s.unitParam (0, pFb, 0.12f); s.unitParam (0, pHp, 150.0f);
            s.set (kIdGlobalMix, 0.3f);
            break;
        case 2: // Ping Pong
            s.div (0, kDiv8); s.unitParam (0, pMode, 1.0f); s.unitParam (0, pFb, 0.5f);
            s.unitParam (0, pLpOn, 1.0f); s.unitParam (0, pLp, 7000.0f);
            s.set (kIdGlobalMix, 0.4f);
            break;
        case 3: // Dotted Eighth: zwei Einheiten parallel, links/rechts
            s.set (kIdAlgo, 9.0f);
            s.unitOn (0, true); s.div (0, kDiv8Dot); s.unitParam (0, pFb, 0.45f); s.pos (0, -0.6f, 0.3f);
            s.unitOn (1, true); s.div (1, kDiv4);     s.unitParam (1, pFb, 0.3f);  s.unitParam (1, pMix, 0.73f); s.pos (1, 0.6f, 0.3f);
            s.set (kIdGlobalMix, 0.35f);
            break;
        case 4: // Tape Chain: A > B seriell, Bandecho-Gleiten, dunkle Wiederholungen
            s.set (kIdAlgo, 0.0f);
            s.unitOn (0, true); s.div (0, kDiv8); s.unitParam (0, pTrans, 0.0f); s.unitParam (0, pFb, 0.45f);
            s.unitParam (0, pLpOn, 1.0f); s.unitParam (0, pLp, 3500.0f);
            s.unitParam (0, pDDepth, 0.15f); s.unitParam (0, pRate, 0.35f);
            s.unitOn (1, true); s.div (1, kDiv4); s.unitParam (1, pFb, 0.3f);
            s.unitParam (1, pLpOn, 1.0f); s.unitParam (1, pLp, 3000.0f); s.unitParam (1, pMix, 0.73f);
            s.set (kIdGlobalMix, 0.5f);
            break;
        case 5: // Wide Space: vier Einheiten parallel, verteilt im Raum
            s.set (kIdAlgo, 9.0f);
            s.unitOn (0, true); s.div (0, kDiv8);  s.pos (0, -0.7f, 0.3f);
            s.unitOn (1, true); s.div (1, kDiv4);  s.pos (1, 0.7f, 0.3f);
            s.unitOn (2, true); s.div (2, kDiv2);  s.pos (2, -0.3f, 0.8f);
            s.unitOn (3, true); s.div (3, kDiv16); s.pos (3, 0.3f, 0.8f);
            for (int u = 0; u < kNumUnits; ++u) s.unitParam (u, pFb, 0.35f);
            s.set (kIdEarly, 1.0f);
            s.set (kIdGlobalMix, 0.3f);
            break;
        case 6: // Binaural Orbit: Kopfhoerer, Echos auch hinter dem Hoerer
            s.set (kIdAlgo, 9.0f); s.set (kIdBinaural, 1.0f);
            s.unitOn (0, true); s.div (0, kDiv8);    s.pos (0, -0.6f, 0.5f);
            s.unitOn (1, true); s.div (1, kDiv8Dot); s.pos (1, 0.6f, 0.5f);
            s.unitOn (2, true); s.div (2, kDiv4);    s.pos (2, 0.0f, -0.7f);
            for (int u = 0; u < 3; ++u) s.unitParam (u, pFb, 0.35f);
            s.set (kIdGlobalMix, 0.35f);
            break;
        case 7: // Tape Wobble: ausgeleiertes Bandecho, punktierte Achtel, dunkel und warm angefahren
            s.unitOn (0, true); s.div (0, kDiv8Dot); s.unitParam (0, pFb, 0.65f);
            s.unitParam (0, pTape, 1.0f); s.unitParam (0, pWow, 0.7f); s.unitParam (0, pInGain, 6.0f);
            s.unitParam (0, pLpOn, 1.0f); s.unitParam (0, pLp, 4500.0f); s.unitParam (0, pTrans, 0.0f);
            s.set (kIdGlobalMix, 0.4f);
            break;
        case 8: // Phase Chain: vier Einheiten in Serie, die Echos verbiegen die naechste Einheit (FM-artig)
            s.set (kIdAlgo, 0.0f); s.set (kIdRouteMode, (float) RoutePhase); s.set (kIdRouteDepth, 0.45f);
            s.unitOn (0, true); s.div (0, kDiv8);    s.unitParam (0, pFb, 0.4f);
            s.unitOn (1, true); s.div (1, kDiv16);   s.unitParam (1, pFb, 0.4f);
            s.unitOn (2, true); s.div (2, kDiv8Dot); s.unitParam (2, pFb, 0.4f);
            s.unitOn (3, true); s.div (3, kDiv4);    s.unitParam (3, pFb, 0.5f);
            s.set (kIdGlobalMix, 0.5f);
            break;
        case 9: // Glitch Steps: Zufallsmuster im Takt springt durch die Delay-Zeit, B treppt im Ping Pong
            s.set (kIdAlgo, 9.0f);
            s.unitOn (0, true); s.div (0, kDiv8); s.unitParam (0, pFb, 0.55f);
            s.unitParam (0, pPattern, (float) PatRandom); s.unitParam (0, pSteps, 1.0f); s.unitParam (0, pSeed, 42.0f);
            s.unitParam (0, pLfoSync, 1.0f); s.unitParam (0, pLfoDiv, (float) kDiv1Bar); s.unitParam (0, pDDepth, 0.6f); s.unitParam (0, pLfoAmt, 3.0f);
            s.pos (0, -0.5f, 0.6f);
            s.unitOn (1, true); s.div (1, kDiv16); s.unitParam (1, pFb, 0.45f); s.unitParam (1, pMode, 1.0f);
            s.unitParam (1, pPattern, (float) PatStairs); s.unitParam (1, pSteps, 0.0f); s.unitParam (1, pGlide, 0.3f);
            s.unitParam (1, pLfoSync, 1.0f); s.unitParam (1, pLfoDiv, (float) kDiv1Bar); s.unitParam (1, pDDepth, 0.4f); s.unitParam (1, pLfoAmt, 2.0f);
            s.pos (1, 0.5f, 0.6f);
            s.set (kIdGlobalMix, 0.45f);
            break;
        case 10: // Chorus Space: zwei breite Einheiten, analoger Chorus in jeder Schleife, Stereo weit
            s.set (kIdAlgo, 9.0f);
            s.unitOn (0, true); s.div (0, kDiv8Dot); s.unitParam (0, pFb, 0.55f); s.pos (0, -0.8f, 0.5f);
            s.unitOn (1, true); s.div (1, kDiv4);    s.unitParam (1, pFb, 0.5f);  s.pos (1, 0.8f, 0.5f);
            for (int u = 0; u < 2; ++u) { s.unitParam (u, pLpOn, 1.0f); s.unitParam (u, pLp, 6000.0f); }
            s.set (kGlobalIds[chOn], 1.0f); s.set (kGlobalIds[chMode], 1.0f); s.set (kGlobalIds[chPlace], (float) ChorusInLoop);
            s.set (kGlobalIds[stWidth], 1.6f); s.set (kGlobalIds[stEnhance], 0.5f);
            s.set (kIdGlobalMix, 0.4f);
            break;
        case 11: // Dub Siren: Feedback ueber 100 %, Resonanz und Bell-Band in der Schleife, Band und Leiern
            s.unitOn (0, true); s.div (0, kDiv8Dot); s.unitParam (0, pFb, 1.02f);
            s.unitParam (0, pLpOn, 1.0f); s.unitParam (0, pLp, 2200.0f); s.unitParam (0, pRes, 0.6f);
            s.unitParam (0, pHpOn, 1.0f); s.unitParam (0, pHp, 250.0f);
            s.unitParam (0, pBellOn, 1.0f); s.unitParam (0, pBellHz, 900.0f); s.unitParam (0, pBellGain, 6.0f); s.unitParam (0, pBellQ, 2.0f);
            s.unitParam (0, pTape, 1.0f); s.unitParam (0, pWow, 0.3f);
            s.unitParam (0, pDDepth, 0.2f); s.unitParam (0, pRate, 0.3f);
            s.set (kIdGlobalMix, 0.5f);
            break;
        case 12: // Filtered Dub: Ping Pong, resonanter HP/LP, der sich ueber 2 Takte oeffnet und schliesst, Band
            s.unitOn (0, true); s.div (0, kDiv8Dot); s.unitParam (0, pFb, 0.72f); s.unitParam (0, pMode, 1.0f);
            s.unitParam (0, pHpOn, 1.0f); s.unitParam (0, pHp, 380.0f); s.unitParam (0, pHpRes, 0.45f);
            s.unitParam (0, pLpOn, 1.0f); s.unitParam (0, pLp, 1600.0f); s.unitParam (0, pRes, 0.7f);
            s.unitParam (0, pLfoSync, 1.0f); s.unitParam (0, pLfoDiv, (float) kDiv2Bars); s.unitParam (0, pFDepth, 0.6f);
            s.unitParam (0, pTape, 1.0f); s.unitParam (0, pWow, 0.2f); s.unitParam (0, pInGain, 3.0f);
            s.set (kIdGlobalMix, 0.45f);
            break;
        case 13: // Distant Atmosphere: vier lange Echos hinten auf der Buehne, Reflexionen, Chorus, breit und dunkel
            s.set (kIdAlgo, 9.0f); s.set (kIdEarly, 1.0f); s.set (kIdDistDelay, 1.0f);
            s.unitOn (0, true); s.div (0, kDiv2);    s.unitParam (0, pFb, 0.65f); s.pos (0, -0.7f, -0.6f);
            s.unitOn (1, true); s.div (1, kDiv2Dot); s.unitParam (1, pFb, 0.6f);  s.pos (1, 0.7f, -0.7f);
            s.unitOn (2, true); s.div (2, kDiv1Bar); s.unitParam (2, pFb, 0.55f); s.pos (2, -0.25f, -0.9f);
            s.unitOn (3, true); s.div (3, kDiv4);    s.unitParam (3, pFb, 0.5f);  s.pos (3, 0.25f, -0.8f);
            for (int u = 0; u < kNumUnits; ++u)
            {
                s.unitParam (u, pLpOn, 1.0f); s.unitParam (u, pLp, 3800.0f); s.unitParam (u, pHpOn, 1.0f); s.unitParam (u, pHp, 180.0f);
                s.unitParam (u, pWow, 0.15f); s.unitParam (u, pMix, 0.8f);
            }
            s.set (kGlobalIds[chOn], 1.0f); s.set (kGlobalIds[chMode], 1.0f); s.set (kGlobalIds[chMix], 0.35f);
            s.set (kGlobalIds[stWidth], 1.5f); s.set (kGlobalIds[stEnhance], 0.6f);
            s.set (kIdGlobalMix, 0.45f);
            break;
        case 14: // Total Chaos: Serie mit Phase + Dry, Zufallsmuster ueberall, Band, Chorus in der Schleife, Feedback ueber 100 %
        {
            s.set (kIdAlgo, 0.0f); s.set (kIdRouteMode, (float) RoutePhaseDry); s.set (kIdRouteDepth, 0.8f);
            const int divs[kNumUnits] = { kDiv16, kDiv8Dot, kDiv8, kDiv4 };
            const float seeds[kNumUnits] = { 13.0f, 77.0f, 256.0f, 901.0f };
            for (int u = 0; u < kNumUnits; ++u)
            {
                s.unitOn (u, true); s.div (u, divs[u]); s.unitParam (u, pFb, 0.85f);
                s.unitParam (u, pPattern, (float) PatRandom); s.unitParam (u, pSeed, seeds[u]); s.unitParam (u, pSteps, 1.0f);
                s.unitParam (u, pLfoSync, 1.0f); s.unitParam (u, pLfoDiv, (float) (u % 2 == 0 ? kDiv1Bar : kDiv2Bars));
                s.unitParam (u, pDDepth, 0.7f); s.unitParam (u, pLfoAmt, 6.0f); s.unitParam (u, pGlide, 0.15f);
                s.unitParam (u, pEnv, 0.4f); s.unitParam (u, pEnvAmt, 4.0f); s.unitParam (u, pFDepth, 0.5f);
                s.unitParam (u, pLpOn, 1.0f); s.unitParam (u, pLp, 5000.0f); s.unitParam (u, pRes, 0.5f);
                s.unitParam (u, pTape, 1.0f); s.unitParam (u, pWow, 0.5f); s.unitParam (u, pInGain, 6.0f);
            }
            s.set (kIdGlobalFb, 1.15f);
            s.set (kGlobalIds[chOn], 1.0f); s.set (kGlobalIds[chMode], 2.0f); s.set (kGlobalIds[chPlace], (float) ChorusInLoop);
            s.set (kGlobalIds[stWidth], 2.0f); s.set (kGlobalIds[stEnhance], 1.0f);
            s.set (kIdGlobalMix, 0.55f);
            break;
        }
        default:
            break; // Init: nur Standardwerte
    }

    // Werte sofort in den gespeicherten Zustand uebernehmen (sonst weicht der Baum bis zum naechsten Timer ab)
    apvts.copyState();
}
juce::File userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("QUAD ECHO").getChildFile ("Presets");
}

juce::Array<juce::File> userPresets()
{
    auto files = userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.qepreset");
    files.sort();
    return files;
}

bool saveUserPreset (juce::AudioProcessorValueTreeState& apvts, const juce::File& file)
{
    juce::XmlElement xml ("QUADECHOPRESET");
    xml.setAttribute (kStateVersionAttr, kStateVersion);
    for (auto* p : apvts.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", r->paramID);
            e->setAttribute ("value", (double) r->convertFrom0to1 (r->getValue()));
        }
    file.getParentDirectory().createDirectory();
    return xml.writeTo (file);
}

bool loadUserPreset (juce::AudioProcessorValueTreeState& apvts, const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("QUADECHOPRESET")) return false;
    migrateState (*xml);   // aeltere Presets auf die aktuelle Bedeutung der Parameter umrechnen
    if (apvts.undoManager != nullptr) apvts.undoManager->beginNewTransaction ("Preset");
    // erst alles auf Standard (fehlende Parameter = neue Funktionen bleiben neutral), dann die Werte der Datei
    for (auto* p : apvts.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            r->setValueNotifyingHost (r->getDefaultValue());
    for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
        if (auto* r = apvts.getParameter (e->getStringAttribute ("id")))
            r->setValueNotifyingHost (r->convertTo0to1 ((float) e->getDoubleAttribute ("value")));
    apvts.copyState();
    return true;
}
} // namespace qe
