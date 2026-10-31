#pragma once
#include "UiExtras.h"

namespace ui
{
// ---------------------------------------------------------------------------
// Symbole fuer Auswahlen (statt Aufklapp-Kaestchen). Jedes zeichnet sich in r mit Farbe c.
namespace icons
{
inline void strokePath (juce::Graphics& g, const juce::Path& p, Colour c, float w = 1.6f)
{
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// LFO-Form: eine Periode, Random als Ausschnitt ueber drei Zyklen
inline void wave (juce::Graphics& g, juce::Rectangle<float> r, Colour c, int w)
{
    r = r.reduced (r.getWidth() * 0.16f, r.getHeight() * 0.24f);
    juce::Path p;
    constexpr int steps = 60;
    for (int i = 0; i <= steps; ++i)
    {
        const double t = (double) i / steps;
        const double pos = w == qe::SmoothRandom ? 4.0 + 3.0 * t : std::min (t, 0.9999);
        const float v = qe::lfoWave (w, pos);
        const float x = r.getX() + r.getWidth() * (float) t, y = r.getCentreY() - v * r.getHeight() * 0.5f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    strokePath (g, p, c);
}

// Note: Kopf um (x, y), Hals nach oben, flags = Anzahl Faehnchen
inline void note (juce::Graphics& g, float x, float y, float s, Colour c, int flags)
{
    juce::Path head;
    head.addEllipse (-s * 0.55f, -s * 0.38f, s * 1.1f, s * 0.76f);
    g.setColour (c);
    g.fillPath (head, juce::AffineTransform::rotation (-0.35f).translated (x, y));
    const float sx = x + s * 0.48f, top = y - s * 2.3f;
    g.drawLine (sx, y - s * 0.1f, sx, top, 1.3f);
    for (int k = 0; k < flags; ++k)
    {
        juce::Path f;
        const float fy = top + (float) k * s * 0.55f;
        f.startNewSubPath (sx, fy);
        f.quadraticTo (sx + s * 0.9f, fy + s * 0.35f, sx + s * 0.7f, fy + s * 1.0f);
        strokePath (g, f, c, 1.2f);
    }
}

// Sync-Modus: Notes, Triplet, Dotted, 16th
inline void syncMode (juce::Graphics& g, juce::Rectangle<float> r, Colour c, int mode)
{
    const float s = std::min (r.getHeight() * 0.2f, r.getWidth() * 0.12f);
    const float cy = r.getCentreY() + s * 1.1f;
    if (mode == qe::SyncTriplet)
    {
        const float d = s * 1.6f, x0 = r.getCentreX() - d;
        for (int k = 0; k < 3; ++k) note (g, x0 + d * (float) k, cy, s * 0.8f, c, 0);
        const float beamY = cy - s * 0.8f * 2.3f;
        g.fillRect (juce::Rectangle<float> (x0 + s * 0.38f, beamY, d * 2.0f, 1.6f));
        g.setFont (ui::uiFont (s * 1.4f, true));
        g.drawText ("3", juce::Rectangle<float> (r.getCentreX() - s, beamY - s * 1.6f, s * 2.0f, s * 1.4f), juce::Justification::centred);
        return;
    }
    const float x = r.getCentreX() - s * 0.4f;
    note (g, x, cy, s, c, mode == qe::Sync16th ? 2 : (mode == qe::SyncNotes ? 1 : 0));
    if (mode == qe::SyncDotted)
        g.fillEllipse (x + s * 1.3f, cy - s * 0.25f, s * 0.5f, s * 0.5f);
}

// Kanalmodus: Stereo (zwei getrennte Spuren), Ping Pong (Echo springt zwischen zwei Seiten hin und her),
// Mid/Side (Buchstaben M/S, eindeutiger als jedes Bild)
inline void channelMode (juce::Graphics& g, juce::Rectangle<float> r, Colour c, int mode)
{
    r = r.reduced (r.getWidth() * 0.2f, r.getHeight() * 0.26f);
    const float y1 = r.getY(), y2 = r.getBottom();
    if (mode == qe::Stereo)
    {
        for (float y : { y1, y2 })
        {
            juce::Path p;
            p.startNewSubPath (r.getX(), y);
            p.lineTo (r.getRight(), y);
            p.startNewSubPath (r.getRight() - 4.0f, y - 3.0f);
            p.lineTo (r.getRight(), y);
            p.lineTo (r.getRight() - 4.0f, y + 3.0f);
            strokePath (g, p, c, 1.4f);
        }
    }
    else if (mode == qe::PingPong)
    {
        // zwei Seiten (L, R) als kurze Waende, dazwischen ein Ball, der hin und her springt
        const float lx = r.getX(), rx = r.getRight();
        g.setColour (c);
        g.fillRoundedRectangle (lx - 1.0f, y1 - 2.0f, 2.5f, y2 - y1 + 4.0f, 1.0f);
        g.fillRoundedRectangle (rx - 1.5f, y1 - 2.0f, 2.5f, y2 - y1 + 4.0f, 1.0f);
        juce::Path p;
        p.startNewSubPath (lx + 3.0f, y1);
        p.lineTo (rx - 3.0f, (y1 + y2) * 0.5f);
        p.lineTo (lx + 3.0f, y2);
        juce::Path dashed;
        const float dashes[] = { 2.5f, 2.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, p, dashes, 2);
        g.fillPath (dashed);
        g.fillEllipse (lx + 1.5f, y2 - 2.5f, 5.0f, 5.0f);
    }
    else
    {
        g.setColour (c);
        g.setFont (ui::uiFont (std::min (r.getHeight() * 1.5f, 15.0f), true));
        g.drawText ("M/S", r.expanded (4.0f), juce::Justification::centred);
    }
}

// Uebergang bei Zeitaenderung: Repitch (Gleiten), Fade (Ueberblenden), Jump (Sprung)
inline void transition (juce::Graphics& g, juce::Rectangle<float> r, Colour c, int t)
{
    r = r.reduced (r.getWidth() * 0.22f, r.getHeight() * 0.26f);
    juce::Path p;
    if (t == qe::Repitch)
    {
        p.startNewSubPath (r.getX(), r.getBottom());
        p.cubicTo (r.getCentreX(), r.getBottom(), r.getCentreX(), r.getY(), r.getRight(), r.getY());
    }
    else if (t == qe::Fade)
    {
        p.startNewSubPath (r.getX(), r.getY());     p.lineTo (r.getRight(), r.getBottom());
        p.startNewSubPath (r.getX(), r.getBottom()); p.lineTo (r.getRight(), r.getY());
    }
    else
    {
        p.startNewSubPath (r.getX(), r.getBottom());
        p.lineTo (r.getCentreX(), r.getBottom());
        p.lineTo (r.getCentreX(), r.getY());
        p.lineTo (r.getRight(), r.getY());
    }
    strokePath (g, p, c, 1.5f);
}
} // namespace icons

// ---------------------------------------------------------------------------
// Auswahl als Reihe von Symbolen (statt Aufklapp-Kaestchen), an einen Auswahl-Parameter gebunden
struct IconChoice : public juce::Component, public juce::TooltipClient
{
    using Icon = std::function<void (juce::Graphics&, juce::Rectangle<float>, Colour)>;

    IconChoice (Apvts& apvts, const juce::String& id, std::vector<Icon> iconList, juce::StringArray names, Colour accentColour,
                const juce::String& title = {}, int rowsCount = 1)
        : icons (std::move (iconList)), tips (std::move (names)), accent (accentColour), heading (title), rows (rowsCount),
          attachment (*apvts.getParameter (id), [this] (float v) { selected = (int) (v + 0.5f); repaint(); }, apvts.undoManager)
    {
        attachment.sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        if (heading.isNotEmpty())
        {
            g.setColour (kDim);
            g.setFont (ui::uiFont (12.0f));
            g.drawText (heading, getLocalBounds().removeFromTop (16).reduced (4, 0), juce::Justification::centredLeft);
        }
        const float alpha = isEnabled() ? 1.0f : 0.45f;
        for (int i = 0; i < (int) icons.size(); ++i)
        {
            const auto cell = cellBounds (i);
            const bool sel = i == selected, hov = i == hover && isEnabled();
            g.setColour (sel ? accent.withMultipliedAlpha (alpha) : (hov ? kPanelHi.brighter (0.15f) : kPanelHi));
            g.fillRoundedRectangle (cell, 5.0f);
            if (! sel)
            {
                g.setColour (kLine.withMultipliedAlpha (0.8f));
                g.drawRoundedRectangle (cell.reduced (0.5f), 5.0f, 1.0f);
            }
            icons[(size_t) i] (g, cell, sel ? kBg : (hov ? kText : kDim).withMultipliedAlpha (alpha));
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = cellAt (e.position);
        if (i >= 0 && i != selected) attachment.setValueAsCompleteGesture ((float) i);
    }
    void mouseMove (const juce::MouseEvent& e) override { const int i = cellAt (e.position); if (i != hover) { hover = i; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    juce::String getTooltip() override { return juce::isPositiveAndBelow (hover, tips.size()) ? tips[hover] : juce::String(); }

private:
    juce::Rectangle<float> cellBounds (int i) const
    {
        auto b = getLocalBounds().toFloat();
        if (heading.isNotEmpty()) b.removeFromTop (17.0f);
        const int n = (int) icons.size(), perRow = (n + rows - 1) / rows;
        constexpr float gap = 3.0f;
        const float w = (b.getWidth() - gap * (float) (perRow - 1)) / (float) perRow;
        const float h = (b.getHeight() - gap * (float) (rows - 1)) / (float) rows;
        const int row = i / perRow, col = i % perRow;
        return { b.getX() + (float) col * (w + gap), b.getY() + (float) row * (h + gap), w, h };
    }
    int cellAt (juce::Point<float> p) const
    {
        for (int i = 0; i < (int) icons.size(); ++i)
            if (cellBounds (i).contains (p)) return i;
        return -1;
    }

    std::vector<Icon> icons;
    juce::StringArray tips;
    Colour accent;
    juce::String heading;
    int rows = 1;
    juce::ParameterAttachment attachment;
    int selected = 0, hover = -1;
};

// Fertige Symbol-Auswahlen
inline std::unique_ptr<IconChoice> makeWaveChoice (Apvts& apvts, const juce::String& id, Colour accent, int rows = 2)
{
    std::vector<IconChoice::Icon> list;
    for (int w = 0; w < qe::kNumWaves; ++w)
        list.push_back ([w] (juce::Graphics& g, juce::Rectangle<float> r, Colour c) { icons::wave (g, r, c, w); });
    return std::make_unique<IconChoice> (apvts, id, std::move (list),
                                         juce::StringArray { "Sine", "Triangle", "Saw up", "Saw down", "Square", "Random (smooth)" }, accent, juce::String(), rows);
}

inline std::unique_ptr<IconChoice> makeSyncModeChoice (Apvts& apvts, const juce::String& id, Colour accent)
{
    std::vector<IconChoice::Icon> list;
    for (int m = 0; m < 4; ++m)
        list.push_back ([m] (juce::Graphics& g, juce::Rectangle<float> r, Colour c) { icons::syncMode (g, r, c, m); });
    return std::make_unique<IconChoice> (apvts, id, std::move (list),
                                         juce::StringArray { "Notes", "Triplet", "Dotted", "16th (time as a number of 16th notes)" }, accent);
}

inline std::unique_ptr<IconChoice> makeChannelModeChoice (Apvts& apvts, const juce::String& id, Colour accent)
{
    std::vector<IconChoice::Icon> list;
    for (int m = 0; m < 3; ++m)
        list.push_back ([m] (juce::Graphics& g, juce::Rectangle<float> r, Colour c) { icons::channelMode (g, r, c, m); });
    return std::make_unique<IconChoice> (apvts, id, std::move (list),
                                         juce::StringArray { "Stereo: left and right independent", "Ping Pong: the echo bounces left-right",
                                                             "Mid/Side: mid and side delayed separately" }, accent, "Channel mode");
}

inline std::unique_ptr<IconChoice> makeTransitionChoice (Apvts& apvts, const juce::String& id, Colour accent)
{
    std::vector<IconChoice::Icon> list;
    for (int t = 0; t < 3; ++t)
        list.push_back ([t] (juce::Graphics& g, juce::Rectangle<float> r, Colour c) { icons::transition (g, r, c, t); });
    return std::make_unique<IconChoice> (apvts, id, std::move (list),
                                         juce::StringArray { "Repitch: tape-like pitch glide", "Fade: smooth crossfade", "Jump: immediate (tiny crossfade)" },
                                         accent, "Transition");
}

// ---------------------------------------------------------------------------
// Notenwert als Drehregler: geht nur durch die Werte des Sync-Modus (Notes, Triplet oder Dotted).
// Der Parameter bleibt der Index in kNoteDivs (siehe qe::divInMode).
struct NoteDivKnob : public juce::Component
{
    NoteDivKnob (Apvts& apvts, const juce::String& divId, const juce::String& modeId, std::function<juce::String()> prefix, Colour col)
        : divParam (apvts.getParameter (divId)),
          divAttach (*divParam, [this] (float v) { div = (int) (v + 0.5f); refresh(); }, apvts.undoManager),
          modeWatch (*apvts.getParameter (modeId), [this] (float v) { mode = (int) (v + 0.5f); refresh(); })
    {
        addAndMakeVisible (slider);
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 16);
        slider.setColour (juce::Slider::rotarySliderFillColourId, col);
        slider.setColour (juce::Slider::textBoxTextColourId, kValue);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        slider.textFromValueFunction = [this, prefix] (double v) { return prefix() + " " + qe::kNoteDivs[entryAt ((int) std::lround (v))].name; };
        slider.valueFromTextFunction = [this] (const juce::String& t)
        {
            const auto name = t.fromLastOccurrenceOf (" ", false, false).trim();
            for (int k = 0; k < (int) entries.size(); ++k)
                if (name == qe::kNoteDivs[entries[(size_t) k]].name) return (double) k;
            return slider.getValue();
        };
        slider.onDragStart = [this] { divAttach.beginGesture(); };
        slider.onDragEnd = [this] { divAttach.endGesture(); };
        slider.onValueChange = [this]
        {
            if (updating) return;
            const int idx = entryAt ((int) std::lround (slider.getValue()));
            if (slider.isMouseButtonDown()) divAttach.setValueAsPartOfGesture ((float) idx);
            else                            divAttach.setValueAsCompleteGesture ((float) idx);
        };
        divAttach.sendInitialUpdate();
        modeWatch.sendInitialUpdate();
    }

    void resized() override { slider.setBounds (getLocalBounds()); }
    FineSlider slider;

private:
    int entryAt (int k) const { return entries.empty() ? 0 : entries[(size_t) juce::jlimit (0, (int) entries.size() - 1, k)]; }

    void refresh()
    {
        entries.clear();
        const int fam = qe::familyOfMode (mode);
        for (int i = 0; i < qe::kNumNoteDivs; ++i)
            if (qe::noteFamily (i) == fam && qe::kNoteDivs[i].beats <= qe::kMaxDelayBeats) entries.push_back (i);
        const int cur = qe::divInMode (div, mode), def = qe::divInMode ((int) divParam->convertFrom0to1 (divParam->getDefaultValue()), mode);
        auto posOf = [this] (int idx) { for (int k = 0; k < (int) entries.size(); ++k) if (entries[(size_t) k] == idx) return k; return 0; };
        updating = true;
        slider.setRange (0.0, (double) std::max (1, (int) entries.size() - 1), 1.0);
        slider.setValue ((double) posOf (cur), juce::dontSendNotification);
        slider.setDoubleClickReturnValue (true, (double) posOf (def));
        updating = false;
        slider.updateText();
    }

    juce::RangedAudioParameter* divParam;
    juce::ParameterAttachment divAttach, modeWatch;
    std::vector<int> entries;
    int div = 0, mode = 0;
    bool updating = false;
};

// ---------------------------------------------------------------------------
// LINK-Knopf der Zeit: leuchtet bei Single-Delay (Parameter "link" an, Standard): der rechte Regler folgt dem
// linken und ist ausgegraut. Aus = der rechte Regler ist frei. Jeder Klick schaltet den Parameter
// direkt um; der Knopf schaltet sich nicht selbst, er zeigt nur an (so braucht es nie zwei Klicks).
struct LinkToggle : public juce::ToggleButton
{
    LinkToggle (Apvts& apvts, const juce::String& id, Colour col)
        : param (apvts.getParameter (id)),
          attachment (*param, [this] (float v) { setToggleState (v > 0.5f, juce::dontSendNotification); }, apvts.undoManager)
    {
        setButtonText ("LINK");
        setColour (juce::ToggleButton::tickColourId, col);
        setClickingTogglesState (false);
        onClick = [this] { attachment.setValueAsCompleteGesture (param->getValue() > 0.5f ? 0.0f : 1.0f); };
        attachment.sendInitialUpdate();
    }
    juce::RangedAudioParameter* param;
    juce::ParameterAttachment attachment;
};

// ---------------------------------------------------------------------------
// Filter als EQ-Kurve mit zwei Punkten (HP und LP), 18 dB/Oktave wie im DSP. Ziehen: links/rechts = Frequenz,
// hoch = Boost (Glocke an der Grenzfrequenz, 0 bis +18 dB; ganz unten sitzt der Punkt auf der Filterkurve).
// Mit Alt gedrueckt hoch/runter = Guete Q (Resonanz und Breite der Glocke). Shift = fein.
// Doppelklick: Frequenz und Resonanz des Punktes auf Standard. An/Aus: kleine Kaestchen HP/LP oben rechts.
// Die Werte stehen klein oben links. Bell-Band: Doppelklick auf eine freie Stelle setzt es dorthin (Frequenz, Gain),
// Ziehen = Frequenz und Gain (-18 bis +18 dB), Alt = Breite (Q), Doppelklick auf das Band entfernt es.
class FilterGraph : public juce::Component, public juce::SettableTooltipClient
{
public:
    struct Ids { juce::String hpOn, lpOn, hp, lp, hpRes, lpRes, hpBoost, lpBoost, bellOn, bellHz, bellGain, bellQ; };

    FilterGraph (Apvts& apvts, const Ids& ids, Colour col) : colour (col)
    {
        const juce::String list[kParams] = { ids.hpOn, ids.lpOn, ids.hp, ids.lp, ids.hpRes, ids.lpRes, ids.hpBoost, ids.lpBoost,
                                             ids.bellOn, ids.bellHz, ids.bellGain, ids.bellQ };
        for (int k = 0; k < kParams; ++k)
        {
            params[(size_t) k] = apvts.getParameter (list[k]);
            attach[(size_t) k] = std::make_unique<juce::ParameterAttachment> (
                *params[(size_t) k], [this, k] (float v) { value[(size_t) k] = v; repaint(); }, apvts.undoManager);
            attach[(size_t) k]->sendInitialUpdate();
        }
        setTooltip ("Drag a point: left/right = frequency, up = boost at the cutoff (up to +18 dB).\nAlt + drag up/down: Q (resonance and width). Shift = fine. Double-click: back to default.\nDouble-click on an empty spot: bell band there (drag = frequency and gain, Alt = width, double-click on it: remove).\nHP / LP boxes top right: filter on/off. 18 dB per octave.");
    }

    void paint (juce::Graphics& g) override
    {
        const auto pa = plotArea();
        g.setColour (kBg.withAlpha (0.55f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

        // Raster: 100 Hz, 1 kHz, 10 kHz und 0 dB
        g.setFont (ui::uiFont (9.5f));
        for (float f : { 100.0f, 1000.0f, 10000.0f })
        {
            const float x = xOf (f);
            g.setColour (kLine.withAlpha (0.6f));
            g.drawVerticalLine ((int) x, pa.getY(), pa.getBottom());
            g.setColour (kDim.withAlpha (0.7f));
            g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f), juce::Rectangle<float> (x + 2.0f, pa.getBottom() - 12.0f, 30.0f, 12.0f), juce::Justification::centredLeft);
        }
        g.setColour (kLine);
        g.drawHorizontalLine ((int) yOf (0.0f), pa.getX(), pa.getRight());

        // Kurve (beide Filter zusammen)
        juce::Path curve;
        constexpr int steps = 140;
        for (int i = 0; i <= steps; ++i)
        {
            const float x = pa.getX() + pa.getWidth() * (float) i / (float) steps;
            const float y = yOf (juce::jlimit (kDbBottom - 6.0f, kDbTop, magnitudeDb (freqOf (x))));
            if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }
        juce::Path fill (curve);
        fill.lineTo (pa.getRight(), pa.getBottom());
        fill.lineTo (pa.getX(), pa.getBottom());
        fill.closeSubPath();
        g.setColour (colour.withAlpha (0.12f));
        g.fillPath (fill);
        g.setColour (colour);
        g.strokePath (curve, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Punkte
        for (int h = 0; h < 2; ++h)
        {
            const auto p = handlePos (h);
            const bool on = isOn (h);
            const auto circle = juce::Rectangle<float> (12.0f, 12.0f).withCentre (p);
            if (on)
            {
                g.setColour (colour);
                g.fillEllipse (circle);
                g.setColour (kBg);
                g.drawEllipse (circle, 1.2f);
            }
            else
            {
                g.setColour (kBg);
                g.fillEllipse (circle);
                g.setColour (colour.withAlpha (0.55f));
                g.drawEllipse (circle.reduced (0.6f), 1.4f);
            }
            g.setColour (on ? kText : kDim);
            g.setFont (ui::uiFont (9.5f, true));
            g.drawText (h == 0 ? "HP" : "LP", circle.translated (0.0f, -13.0f).expanded (6.0f, 0.0f), juce::Justification::centred);
        }
        if (bellOn())   // Bell-Band: Raute auf seinem Gain, mit Wert
        {
            const auto p = handlePos (2);
            juce::Path d;
            d.addQuadrilateral (p.x, p.y - 7.0f, p.x + 7.0f, p.y, p.x, p.y + 7.0f, p.x - 7.0f, p.y);
            g.setColour (colour);
            g.fillPath (d);
            g.setColour (kBg);
            g.strokePath (d, juce::PathStrokeType (1.2f));
            g.setColour (kText);
            g.setFont (ui::uiFont (9.5f, true));
            g.drawText ((bellGain() >= 0.0f ? "+" : "") + juce::String (bellGain(), 1) + " dB", juce::Rectangle<float> (p.x - 30.0f, p.y + (bellGain() >= 0.0f ? 8.0f : -20.0f), 60.0f, 12.0f), juce::Justification::centred);
        }

        // Zeile ueber der Kurve: links die Werte, rechts die Kaestchen HP / LP zum An- und Ausschalten
        g.setFont (ui::uiFont (10.5f));
        auto line = getLocalBounds().toFloat().reduced (8.0f, 2.0f).removeFromTop (14.0f);
        line.removeFromRight (74.0f);
        const float half = line.getWidth() * 0.5f;
        for (int h = 0; h < 2; ++h)
        {
            g.setColour (isOn (h) ? kText.withAlpha (0.85f) : kDim.withAlpha (0.5f));
            const juce::String txt = juce::String (h == 0 ? "HP " : "LP ") + fmtHz (freq (h)) + "  Q " + juce::String (q (res (h)), 2)
                                     + (boost (h) > qe::kBoostOffDb ? "  +" + juce::String (boost (h), 1) + " dB" : juce::String());
            g.drawText (txt, juce::Rectangle<float> (line.getX() + (float) h * half, line.getY(), half, line.getHeight()), juce::Justification::centredLeft);
        }
        for (int h = 0; h < 2; ++h)
        {
            const auto box = boxBounds (h);
            const bool on = isOn (h);
            g.setColour (on ? colour : kPanelHi);
            g.fillRoundedRectangle (box, 3.0f);
            g.setColour (on ? colour : kLine);
            g.drawRoundedRectangle (box.reduced (0.5f), 3.0f, 1.0f);
            g.setColour (on ? kBg : kDim);
            g.setFont (ui::uiFont (9.5f, true));
            g.drawText (h == 0 ? "HP" : "LP", box, juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int h = 0; h < 2; ++h)
            if (boxBounds (h).contains (e.position))
            {
                attach[(size_t) (HpOn + h)]->setValueAsCompleteGesture (isOn (h) ? 0.0f : 1.0f);
                dragged = -1;
                return;
            }
        dragged = nearest (e.position);
        moved = false;
        if (dragged < 0) return;
        lastPos = e.position;
        if (dragged == 2)
        {
            logF = std::log (bellHz());
            qDb = juce::Decibels::gainToDecibels (bellQ());
            boostDb = bellGain();
        }
        else
        {
            logF = std::log (freq (dragged));
            qDb = juce::Decibels::gainToDecibels (q (res (dragged)));
            boostDb = boost (dragged);
        }
        for (int k : gestureParams (dragged)) attach[(size_t) k]->beginGesture();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragged < 0) return;
        moved = moved || e.getDistanceFromDragStart() > 2;
        if (! moved) return;
        const auto pa = plotArea();
        const float fine = e.mods.isShiftDown() ? 0.1f : 1.0f;
        const float dyDb = -(e.position.y - lastPos.y) * fine * (kDbTop - kDbBottom) / pa.getHeight();
        if (dragged == 2)
        {
            if (e.mods.isAltDown())   // Alt: Breite (Q) des Bell-Bands
            {
                const auto& qr = params[BellQ]->getNormalisableRange();
                qDb = juce::jlimit (juce::Decibels::gainToDecibels (qr.start), juce::Decibels::gainToDecibels (qr.end), qDb + dyDb);
                attach[BellQ]->setValueAsPartOfGesture (juce::Decibels::decibelsToGain (qDb));
            }
            else
            {
                logF += (double) ((e.position.x - lastPos.x) * fine) * std::log (20000.0 / 20.0) / (double) pa.getWidth();
                boostDb = juce::jlimit (-18.0f, 18.0f, boostDb + dyDb);
                attach[BellHz]->setValueAsPartOfGesture (juce::jlimit (20.0f, 20000.0f, (float) std::exp (logF)));
                attach[BellGain]->setValueAsPartOfGesture (boostDb);
            }
            lastPos = e.position;
            return;
        }
        const float resMax = params[(size_t) (HpRes + dragged)]->getNormalisableRange().end;
        const float boostMax = params[(size_t) (HpBoost + dragged)]->getNormalisableRange().end;
        if (e.mods.isAltDown())   // Alt: nur die Guete (hoch/runter)
        {
            qDb = juce::jlimit (juce::Decibels::gainToDecibels (0.7071f), juce::Decibels::gainToDecibels (q (resMax)), qDb + dyDb);
            const float qv = juce::Decibels::decibelsToGain (qDb);
            attach[(size_t) (HpRes + dragged)]->setValueAsPartOfGesture (std::min (resMax, std::sqrt (std::max (0.0f, qv - 0.7071f) / 4.0f)));
        }
        else                      // links/rechts = Frequenz, hoch/runter = Boost
        {
            logF += (double) ((e.position.x - lastPos.x) * fine) * std::log (20000.0 / 20.0) / (double) pa.getWidth();
            boostDb = juce::jlimit (0.0f, boostMax, boostDb + dyDb);
            const auto& fr = params[(size_t) (Hp + dragged)]->getNormalisableRange();
            attach[(size_t) (Hp + dragged)]->setValueAsPartOfGesture (juce::jlimit (fr.start, fr.end, (float) std::exp (logF)));
            attach[(size_t) (HpBoost + dragged)]->setValueAsPartOfGesture (boostDb);
        }
        lastPos = e.position;
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragged < 0) return;
        for (int k : gestureParams (dragged)) attach[(size_t) k]->endGesture();
        dragged = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int h = nearest (e.position);
        if (h == 2) { attach[BellOn]->setValueAsCompleteGesture (0.0f); return; }   // Bell-Band entfernen
        if (h < 0)
        {
            // freie Stelle: Bell-Band an Frequenz und Hoehe der Maus (zwischen -18 und +18 dB)
            if (! plotArea().contains (e.position)) return;
            const float gain = juce::jlimit (-18.0f, 18.0f, dbOf (e.position.y));
            for (int k : { BellHz, BellGain, BellOn }) attach[(size_t) k]->beginGesture();
            attach[BellHz]->setValueAsPartOfGesture (juce::jlimit (20.0f, 20000.0f, freqOf (e.position.x)));
            attach[BellGain]->setValueAsPartOfGesture (std::abs (gain) < 1.0f ? 6.0f : gain);
            attach[BellOn]->setValueAsPartOfGesture (1.0f);
            for (int k : { BellHz, BellGain, BellOn }) attach[(size_t) k]->endGesture();
            return;
        }
        for (int k : { Hp + h, HpRes + h, HpBoost + h })
            attach[(size_t) k]->setValueAsCompleteGesture (params[(size_t) k]->convertFrom0to1 (params[(size_t) k]->getDefaultValue()));
    }

private:
    enum { HpOn = 0, LpOn, Hp, Lp, HpRes, LpRes, HpBoost, LpBoost, BellOn, BellHz, BellGain, BellQ, kParams };
    std::vector<int> gestureParams (int h) const { return h == 2 ? std::vector<int> { BellHz, BellGain, BellQ } : std::vector<int> { Hp + h, HpRes + h, HpBoost + h }; }
    static constexpr float kDbTop = 30.0f, kDbBottom = -36.0f;   // oben Platz fuer Resonanz plus Boost
    static constexpr float kFirstOrderDb = -3.0103f;   // Stufe 1. Ordnung bei der Grenzfrequenz

    // Kaestchen HP / LP oben rechts
    juce::Rectangle<float> boxBounds (int h) const
    {
        const auto line = getLocalBounds().toFloat().reduced (8.0f, 2.0f).removeFromTop (14.0f);
        return { line.getRight() - 66.0f + (float) h * 34.0f, line.getY(), 30.0f, 13.0f };
    }
    // Hoehe des Punktes: Pegel bei der Grenzfrequenz (Resonanz der 2. Ordnung, 1. Ordnung und Boost)
    float handleDb (int h) const { return juce::Decibels::gainToDecibels (q (res (h))) + kFirstOrderDb + boost (h); }

    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced (6.0f, 4.0f).withTrimmedTop (14.0f); }
    bool isOn (int h) const { return value[(size_t) (HpOn + h)] > 0.5f; }
    float freq (int h) const { return value[(size_t) (Hp + h)]; }
    float res (int h) const { return value[(size_t) (HpRes + h)]; }
    float boost (int h) const { return value[(size_t) (HpBoost + h)]; }
    static float q (float r) { return 0.7071f + r * r * 4.0f; }
    bool bellOn() const { return value[BellOn] > 0.5f; }
    float bellHz() const { return value[BellHz]; }
    float bellGain() const { return value[BellGain]; }
    float bellQ() const { return value[BellQ]; }
    float dbOf (float y) const { const auto pa = plotArea(); return kDbTop - (y - pa.getY()) * (kDbTop - kDbBottom) / pa.getHeight(); }

    float xOf (float f) const { const auto pa = plotArea(); return pa.getX() + pa.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); }
    float freqOf (float x) const { const auto pa = plotArea(); return 20.0f * std::pow (1000.0f, (x - pa.getX()) / pa.getWidth()); }
    float yOf (float dbv) const { const auto pa = plotArea(); return pa.getY() + pa.getHeight() * (kDbTop - dbv) / (kDbTop - kDbBottom); }

    // Betragsgang der beiden Filter (2. Ordnung mit Resonanz plus 1. Ordnung, wie im DSP) in dB
    float magnitudeDb (float f) const
    {
        float gain = 1.0f;
        for (int h = 0; h < 2; ++h)
        {
            if (! isOn (h)) continue;
            const float x = f / freq (h), qq = q (res (h));
            const float den2 = std::sqrt ((1.0f - x * x) * (1.0f - x * x) + (x / qq) * (x / qq));
            const float den1 = std::sqrt (1.0f + x * x);
            gain *= (h == 0 ? x * x * x : 1.0f) / std::max (den2 * den1, 1.0e-6f);
            if (boost (h) > qe::kBoostOffDb)   // Glocke an der Grenzfrequenz (gleiche Guete)
            {
                const float A = std::pow (10.0f, boost (h) / 40.0f), d = (1.0f - x * x) * (1.0f - x * x);
                gain *= std::sqrt ((d + (x * A / qq) * (x * A / qq)) / (d + (x / (A * qq)) * (x / (A * qq))));
            }
        }
        if (bellOn() && std::abs (bellGain()) > qe::kBoostOffDb)   // Bell-Band (Anhebung oder Absenkung)
        {
            const float x = f / bellHz(), qq = bellQ(), A = std::pow (10.0f, bellGain() / 40.0f), d = (1.0f - x * x) * (1.0f - x * x);
            gain *= std::sqrt ((d + (x * A / qq) * (x * A / qq)) / (d + (x / (A * qq)) * (x / (A * qq))));
        }
        return juce::Decibels::gainToDecibels (gain, -100.0f);
    }

    juce::Point<float> handlePos (int h) const
    {
        if (h == 2) return { xOf (bellHz()), yOf (bellGain()) };
        return { xOf (freq (h)), yOf (handleDb (h)) };
    }

    int nearest (juce::Point<float> p) const
    {
        int best = -1;
        float bestD = 16.0f;
        for (int h = 0; h < (bellOn() ? 3 : 2); ++h)
        {
            const float d = handlePos (h).getDistanceFrom (p);
            if (d < bestD) { bestD = d; best = h; }
        }
        return best;
    }

    Colour colour;
    std::array<juce::RangedAudioParameter*, kParams> params {};
    std::array<std::unique_ptr<juce::ParameterAttachment>, kParams> attach;
    std::array<float, kParams> value {};
    int dragged = -1;
    bool moved = false;
    juce::Point<float> lastPos;
    double logF = 0.0;
    float qDb = 0.0f, boostDb = 0.0f;
};
} // namespace ui
