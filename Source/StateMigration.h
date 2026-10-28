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
constexpr int kStateVersion = 6;
inline const char* const kStateVersionAttr = "qeVersion";

// Rechnet einen Zustand der Version 1 so um, dass das Verhaeltnis von Original zu Echos ungefaehr
// erhalten bleibt. Alt (Einheit u mit Mix m, globaler Mix G, Winkel = Wert * pi/2):
//   Original ~ cos G + sin G * cos m_mittel,   Echo_u ~ sin G * sin m_u
// Neu (Echo-Pegel L_u, globaler Mix g):
//   Original = cos g,                          Echo_u = sin g * L_u
// Mit L_u = sin m_u / max(sin m) und tan g = Echo_max / Original.
inline void migrateState (juce::XmlElement& xml)
{
    const int version = xml.getIntAttribute (kStateVersionAttr, 1);
    if (version >= kStateVersion)
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
            setValue (juce::String (letters[u]) + "_syncMode", (double) noteFamily (div));
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
} // namespace qe
