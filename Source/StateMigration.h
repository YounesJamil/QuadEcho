#pragma once
#include <juce_core/juce_core.h>
#include <cmath>
#include "DspTypes.h"

namespace qe
{
// Zustandsversion (Attribut am gespeicherten Zustand):
// 1 = Dry/Wet je Einheit (equal power) plus globaler Dry/Wet zwischen Eingang und Einheiten
// 2 = Echo-Pegel je Einheit (das Original laeuft durch), nur der globale Dry/Wet bestimmt das Original
// 3 = Sync-Modus je Einheit (Notes, Triplet, Dotted, 16th; Standard 16th)
// 4 = getrennte Resonanz fuer HP und LP (res = LP, hpRes neu), Offset auch fuer L
// 5 = Sync-Modus je Kanal (syncMode = L, syncModeR neu)
// 6 = Limiter wahlweise nur auf die Echos (limitDry neu, Standard aus); aeltere Zustaende behalten "auf alles"
// 7 = Notenwerte ab einem Takt als Taktzahl (1, 1.5, 2 .. 24 statt 1/1, 2/1, 4/1): andere Indizes in kNoteDivs
// 8 = Schalter x4 ersetzt durch LFO- und ENV-Faktor (0,1-10), die auch auf das Filter wirken
constexpr int kStateVersion = 8;
inline const char* const kStateVersionAttr = "qeVersion";

namespace detail
{
// Notenwert-Liste bis Zustandsversion 6 (Index -> Schlaege, Familie 0 gerade / 1 triolisch / 2 punktiert)
struct OldDiv { double beats; int family; };
inline constexpr OldDiv kOldDivs[17] = {
    { 0.125, 0 }, { 1.0 / 6.0, 1 }, { 0.25, 0 }, { 0.375, 2 }, { 1.0 / 3.0, 1 }, { 0.5, 0 }, { 0.75, 2 }, { 2.0 / 3.0, 1 },
    { 1.0, 0 }, { 1.5, 2 }, { 4.0 / 3.0, 1 }, { 2.0, 0 }, { 3.0, 2 }, { 8.0 / 3.0, 1 }, { 4.0, 0 }, { 8.0, 0 }, { 16.0, 0 } };

inline int oldNoteFamily (int i) { return kOldDivs[std::min (std::max (i, 0), 16)].family; }

// divInMode mit der alten Liste: welcher Notenwert bei diesem Sync-Modus tatsaechlich klang
inline int oldDivInMode (int div, int mode)
{
    div = std::min (std::max (div, 0), 16);
    const int fam = familyOfMode (mode);
    if (kOldDivs[div].family == fam) return div;
    auto base = [] (int i) { const auto& d = kOldDivs[i]; return d.family == 1 ? d.beats * 1.5 : (d.family == 2 ? d.beats / 1.5 : d.beats); };
    int best = div;
    double bestDist = 1.0e9;
    for (int i = 0; i < 17; ++i)
        if (kOldDivs[i].family == fam)
        {
            const double d = std::abs (std::log (base (i) / base (div)));
            if (d < bestDist) { bestDist = d; best = i; }
        }
    return best;
}

// alter Index -> neuer Index (bis 1/1 gleich; 2/1 = "2", 4/1 = "4")
inline int remapOldDiv (int i)
{
    i = std::min (std::max (i, 0), 16);
    return i <= 14 ? i : (i == 15 ? 16 : 18);
}
} // namespace detail

// Rechnet einen Zustand der Version 1 so um, dass das Verhaeltnis von Original zu Echos ungefaehr
// erhalten bleibt. Alt (Einheit u mit Mix m, globaler Mix G, Winkel = Wert * pi/2):
//   Original ~ cos G + sin G * cos m_mittel,   Echo_u ~ sin G * sin m_u
// Neu (Echo-Pegel L_u, globaler Mix g):
//   Original = cos g,                          Echo_u = sin g * L_u
// Mit L_u = sin m_u / max(sin m) und tan g = Echo_max / Original.
namespace detail
{
inline void migrateToV6 (juce::XmlElement& xml, int version)
{
    if (version >= 6)
        return;

    auto find = [&xml] (const juce::String& id) -> juce::XmlElement*
    {
        for (auto* c : xml.getChildWithTagNameIterator ("PARAM"))
            if (c->getStringAttribute ("id") == id) return c;
        return nullptr;
    };
    auto value = [&find] (const juce::String& id, double def)
    {
        auto* e = find (id);
        return e != nullptr ? e->getDoubleAttribute ("value", def) : def;
    };
    auto setValue = [&xml, &find] (const juce::String& id, double v)
    {
        auto* e = find (id);
        if (e == nullptr) { e = xml.createNewChildElement ("PARAM"); e->setAttribute ("id", id); }
        e->setAttribute ("value", v);
    };

    static const char* const letters[4] = { "a", "b", "c", "d" };

    // 5 -> 6: bisher wirkte der Limiter immer auch auf das Original
    setValue ("limitDry", 1.0);
    if (version >= 5)
    {
        xml.setAttribute (kStateVersionAttr, kStateVersion);
        return;
    }

    // 2 -> 3 (unten) setzt den Sync-Modus; 4 -> 5: R bekommt den Modus von L
    auto copySyncModeToR = [&]
    {
        for (int u = 0; u < 4; ++u)
            setValue (juce::String (letters[u]) + "_syncModeR", value (juce::String (letters[u]) + "_syncMode", 0.0));
    };

    if (version >= 4)
    {
        copySyncModeToR();
        xml.setAttribute (kStateVersionAttr, kStateVersion);
        return;
    }

    // 3 -> 4: die bisher gemeinsame Resonanz gilt fuer beide Filter
    for (int u = 0; u < 4; ++u)
        setValue (juce::String (letters[u]) + "_hpRes", value (juce::String (letters[u]) + "_res", 0.2));
    setValue ("gf_hpRes", value ("gf_res", 0.2));
    setValue ("nz_hpRes", value ("nz_res", 0.2));

    // 2 -> 3: der neue Standard (16th) wuerde die Zeiten aendern. Alte Zustaende bekommen den Modus,
    // zu dem ihr Notenwert gehoert (gerade = Notes, T = Triplet, . = Dotted), und klingen wie vorher.
    if (version < 3)
    {
        static const int oldDefaultDiv[4] = { 5, 6, 8, 2 };
        for (int u = 0; u < 4; ++u)
        {
            const int div = (int) std::lround (value (juce::String (letters[u]) + "_divL", oldDefaultDiv[u]));
            setValue (juce::String (letters[u]) + "_syncMode", (double) oldNoteFamily (div));
        }
    }
    copySyncModeToR();
    if (version >= 2)
    {
        xml.setAttribute (kStateVersionAttr, kStateVersion);
        return;
    }

    // 1 -> 2: Dry/Wet je Einheit -> Echo-Pegel und globaler Mix
    constexpr double halfPi = 1.5707963267948966;
    double mix[4];
    bool on[4];
    int onCount = 0;
    for (int u = 0; u < 4; ++u)
    {
        mix[u] = value (juce::String (letters[u]) + "_mix", 0.5);
        on[u] = value (juce::String (letters[u]) + "_on", u == 0 ? 1.0 : 0.0) > 0.5;
        onCount += on[u] ? 1 : 0;
    }
    // Massstab: die eingeschalteten Einheiten (ohne eingeschaltete alle)
    double sMax = 0.0, mSum = 0.0;
    int count = 0;
    for (int u = 0; u < 4; ++u)
        if (on[u] || onCount == 0)
        {
            sMax = std::max (sMax, std::sin (mix[u] * halfPi));
            mSum += mix[u];
            ++count;
        }
    const double mMean = count > 0 ? mSum / count : 0.5;
    const double norm = sMax > 1.0e-4 ? sMax : 1.0;
    for (int u = 0; u < 4; ++u)
        setValue (juce::String (letters[u]) + "_mix", juce::jlimit (0.0, 1.0, std::sin (mix[u] * halfPi) / norm));

    const double G = value ("gMix", 1.0) * halfPi;
    const double original = std::cos (G) + std::sin (G) * std::cos (mMean * halfPi);
    const double echo = std::sin (G) * sMax;
    setValue ("gMix", juce::jlimit (0.0, 1.0, std::atan2 (echo, original) / halfPi));

    xml.setAttribute (kStateVersionAttr, kStateVersion);
}

// 6 -> 7: Notenwerte auf die neue Liste umrechnen. Delay-Sync behaelt genau den Wert, der vorher klang (auch im
// Triplet-/Dotted-Modus, wo der naechstliegende Wert der Familie gilt), LFO-Sync die gleiche Zahl Schlaege.
inline void remapNoteDivsV7 (juce::XmlElement& xml)
{
    auto find = [&xml] (const juce::String& id) -> juce::XmlElement*
    {
        for (auto* c : xml.getChildWithTagNameIterator ("PARAM"))
            if (c->getStringAttribute ("id") == id) return c;
        return nullptr;
    };
    auto remapParam = [&find] (const juce::String& id) { if (auto* e = find (id)) e->setAttribute ("value", (double) remapOldDiv ((int) std::lround (e->getDoubleAttribute ("value")))); };
    static const char* const letters[4] = { "a", "b", "c", "d" };
    for (int u = 0; u < 4; ++u)
    {
        const juce::String L (letters[u]);
        for (int side = 0; side < 2; ++side)
        {
            auto* divE = find (L + (side == 0 ? "_divL" : "_divR"));
            if (divE == nullptr) continue;
            auto* modeE = find (L + (side == 0 ? "_syncMode" : "_syncModeR"));
            const int oldDiv = (int) std::lround (divE->getDoubleAttribute ("value"));
            const int mode = modeE != nullptr ? (int) std::lround (modeE->getDoubleAttribute ("value")) : (int) Sync16th;
            int newDiv = remapOldDiv (oldDiv);
            if (mode != Sync16th)
            {
                const int want = remapOldDiv (oldDivInMode (oldDiv, mode));
                if (divInMode (newDiv, mode) != want) newDiv = want;
            }
            divE->setAttribute ("value", (double) newDiv);
        }
        remapParam (L + "_lfoDiv");
    }
    remapParam ("gf_div");
    remapParam ("nz_div");
}

// 7 -> 8: x4 an wird zu LFO- und ENV-Faktor 4 (die Delay-Modulation bleibt gleich). Die Faktoren wirken jetzt auch
// auf das Filter, das x4 nicht kannte: die Filter-Tiefe wird geviertelt, damit es gleich klingt.
inline void migrateModFactorsV8 (juce::XmlElement& xml)
{
    auto find = [&xml] (const juce::String& id) -> juce::XmlElement*
    {
        for (auto* c : xml.getChildWithTagNameIterator ("PARAM"))
            if (c->getStringAttribute ("id") == id) return c;
        return nullptr;
    };
    static const char* const letters[4] = { "a", "b", "c", "d" };
    for (int u = 0; u < 4; ++u)
    {
        const juce::String L (letters[u]);
        auto* x4 = find (L + "_x4");
        if (x4 == nullptr) continue;
        if (x4->getDoubleAttribute ("value") > 0.5)
        {
            for (const char* name : { "_lfoAmt", "_envAmt" })
            {
                auto* e = find (L + name);
                if (e == nullptr) { e = xml.createNewChildElement ("PARAM"); e->setAttribute ("id", L + name); }
                e->setAttribute ("value", 4.0);
            }
            if (auto* fd = find (L + "_fDepth")) fd->setAttribute ("value", fd->getDoubleAttribute ("value") / 4.0);
        }
        xml.removeChildElement (x4, true);
    }
}
} // namespace detail

inline void migrateState (juce::XmlElement& xml)
{
    const int version = xml.getIntAttribute (kStateVersionAttr, 1);
    if (version >= kStateVersion)
        return;
    if (version < 7)
    {
        detail::migrateToV6 (xml, version);   // rechnet mit der alten Notenwert-Liste
        detail::remapNoteDivsV7 (xml);
    }
    detail::migrateModFactorsV8 (xml);
    xml.setAttribute (kStateVersionAttr, kStateVersion);
}
} // namespace qe
