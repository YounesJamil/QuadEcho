#include "Presets.h"
#include "Params.h"

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

const char* const kNames[] = { "Init", "Slap", "Ping Pong", "Dotted Eighth", "Tape Chain", "Wide Space", "Binaural Orbit" };
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
        default:
            break; // Init: nur Standardwerte
    }

    // Werte sofort in den gespeicherten Zustand uebernehmen (sonst weicht der Baum bis zum naechsten Timer ab)
    apvts.copyState();
}
} // namespace qe
