#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <BinaryData.h>
#include "Params.h"

namespace ui
{
using juce::Colour;
// leicht vintage: warmes Schwarz, Creme, gedaempfte Farben
inline const Colour kBg { 0xff1a1712 }, kPanel { 0xff24201a }, kPanelHi { 0xff332e27 },
                    kText { 0xffd9c6a1 }, kDim { 0xffa89c82 }, kAccent { 0xff7fcbb6 }, kLine { 0xff4a4439 };

// Schrift nirgends weiss: Texte in warmem Sand (kText), Werte etwas heller (kValue).
inline const Colour kValue { 0xffe2cfa8 };
inline const Colour kOffsetColour { 0xff8fd2dc };   // Offset (Swing): pastelliges Tuerkisblau

// Eine Schrift fuer die ganze Oberflaeche: Barlow Semi Condensed, im Plugin eingebaut (SIL Open Font
// License, Resources/Fonts/OFL.txt): technisch, schmal, ruhige Ziffern, auf Windows und Mac gleich.
// Normal fuer Beschriftungen, fett fuer Knoepfe und Titel, SemiBold fuer veraenderbare Werte.
// Ausdruecklich gesetzt, weil JUCE die Standardschrift beim globalen Look sucht, nicht bei unserem.
// Geteilt, solange ein Editor offen ist (SharedResourcePointer).
struct EmbeddedFonts
{
    // Barlow hat bei gleicher JUCE-Hoehe kleinere Buchstaben als die Systemschriften: angleichen
    static constexpr float kScale = 1.18f;

    juce::Typeface::Ptr regular  = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedRegular_ttf, BinaryData::BarlowSemiCondensedRegular_ttfSize);
    juce::Typeface::Ptr semiBold = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedSemiBold_ttf, BinaryData::BarlowSemiCondensedSemiBold_ttfSize);
    juce::Typeface::Ptr bold     = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedBold_ttf, BinaryData::BarlowSemiCondensedBold_ttfSize);
};

inline juce::Font uiFont (float size, bool bold = false)
{
    juce::SharedResourcePointer<EmbeddedFonts> fonts;
    return juce::Font (juce::FontOptions (bold ? fonts->bold : fonts->regular).withHeight (size * EmbeddedFonts::kScale));
}
inline juce::Font valueFont (float size)
{
    juce::SharedResourcePointer<EmbeddedFonts> fonts;
    return juce::Font (juce::FontOptions (fonts->semiBold).withHeight (size * EmbeddedFonts::kScale));
}

// Wiederkehrende Funktionen = gleiche Farbe: Dry (cremeweiss), Noise (Stahlblau)
inline const Colour kDryColour { 0xffe0cca6 }, kNoiseColour { 0xff9bb8e4 };

inline Colour unitColour (int u)
{
    static const juce::uint32 c[] = { 0xffec7a5c, 0xffeec265, 0xff7fc2ca, 0xffbb9cd6 };
    return Colour (c[u & 3]);
}

// ---- Symbole ----
inline void drawNoiseIcon (juce::Graphics& g, juce::Rectangle<float> r, Colour c)
{
    static const float h[] = { 0.35f, 0.8f, 0.5f, 1.0f, 0.3f, 0.7f, 0.45f, 0.9f, 0.55f };
    g.setColour (c);
    const int n = (int) (sizeof (h) / sizeof (h[0]));
    const float w = r.getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        const float bh = r.getHeight() * h[i];
        g.fillRect (juce::Rectangle<float> (r.getX() + (float) i * w + w * 0.2f, r.getCentreY() - bh * 0.5f, w * 0.6f, bh));
    }
}

// Dry = neutrale Form: Ring mit Punkt (das Original, die Quelle)
inline void drawDryIcon (juce::Graphics& g, juce::Point<float> c, Colour col)
{
    g.setColour (col);
    g.drawEllipse (c.x - 5.6f, c.y - 5.6f, 11.2f, 11.2f, 1.5f);
    g.fillEllipse (c.x - 2.1f, c.y - 2.1f, 4.2f, 4.2f);
}

// Expert = Zahnrad
inline void drawGearIcon (juce::Graphics& g, juce::Point<float> c, Colour col)
{
    g.setColour (col);
    for (int k = 0; k < 8; ++k)
    {
        juce::Path tooth;
        tooth.addRectangle (-1.5f, -7.0f, 3.0f, 3.2f);
        g.fillPath (tooth, juce::AffineTransform::rotation ((float) k * juce::MathConstants<float>::pi * 0.25f).translated (c.x, c.y));
    }
    juce::Path ring;
    ring.addEllipse (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f);
    g.strokePath (ring, juce::PathStrokeType (2.2f));
    g.fillEllipse (c.x - 1.6f, c.y - 1.6f, 3.2f, 3.2f);
}

// ---- Struktur: Koernung als Material jeder Flaeche, Licht von oben ----
// Koernungs-Kacheln, einmal erzeugt und geteilt, solange ein Editor offen ist (SharedResourcePointer):
// fein (Panels) und weich (Hintergrund: weichgezeichnetes Rauschen, wirkt wie Papier oder Stein statt
// wie Pixel). Warme Toene (Creme und Braunschwarz), damit die Struktur zur Flaechenfarbe gehoert.
struct GrainTile
{
    juce::Image fine { juce::Image::ARGB, 128, 128, true }, soft { juce::Image::ARGB, 256, 256, true };
    GrainTile()
    {
        juce::Random rnd (20261028);
        const Colour light (0xfff3e9d2), dark (0xff0d0905);
        auto tone = [&] (float v, float strength) { return (v > 0.0f ? light : dark).withAlpha (std::abs (v) * strength); };
        for (int y = 0; y < fine.getHeight(); ++y)
            for (int x = 0; x < fine.getWidth(); ++x)
                fine.setPixelAt (x, y, tone (rnd.nextFloat() - 0.5f, 0.09f));

        // weich: Rauschen sechsmal mit 3x3 weichzeichnen (nahtlos, die Kachel wiederholt sich unsichtbar)
        constexpr int n = 256;
        std::vector<float> v ((size_t) (n * n)), t ((size_t) (n * n));
        for (auto& x : v) x = rnd.nextFloat() - 0.5f;
        for (int pass = 0; pass < 6; ++pass)
        {
            for (int y = 0; y < n; ++y)
                for (int x = 0; x < n; ++x)
                {
                    float sum = 0.0f;
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx)
                            sum += v[(size_t) (((y + dy + n) % n) * n + (x + dx + n) % n)];
                    t[(size_t) (y * n + x)] = sum / 9.0f;
                }
            std::swap (v, t);
        }
        float peak = 1.0e-6f;
        for (auto x : v) peak = std::max (peak, std::abs (x));
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x)
                soft.setPixelAt (x, y, tone (v[(size_t) (y * n + x)] / peak * 0.5f, 0.09f));
    }
};

// Koernung, an der Ecke der Flaeche verankert: jede Flaeche hat ihr eigenes Muster, das mit ihr geht.
// Die feine Koernung wird in echter Bildschirmaufloesung gezeichnet (ein Korn = ein Bildschirmpixel): bei
// Windows-Skalierung oder kleinerem Fenster wuerde ein gestrecktes Texturpixel sonst als Block sichtbar.
// Die weiche Struktur des Hintergrunds hat keine harten Kanten und darf mitskalieren.
inline void drawGrain (juce::Graphics& g, juce::Rectangle<float> r, float radius, float strength = 1.0f, bool soft = false)
{
    juce::SharedResourcePointer<GrainTile> grain;
    const float scale = soft ? 1.0f : 1.0f / std::max (0.25f, g.getInternalContext().getPhysicalPixelScaleFactor());
    juce::FillType fill (soft ? grain->soft : grain->fine, juce::AffineTransform::scale (scale).translated (r.getX(), r.getY()));
    fill.setOpacity (strength);
    g.saveState();
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.setFillType (fill);
    g.fillRoundedRectangle (r, radius);
    g.restoreState();
}

// Licht von oben ueber Flaeche und Koernung: oben heller, unten dunkler (die Koernung wirkt wie Oberflaeche)
inline void drawLight (juce::Graphics& g, juce::Rectangle<float> r, float radius, float strength = 1.0f)
{
    juce::ColourGradient light (juce::Colours::white.withAlpha (0.05f * strength), 0.0f, r.getY(),
                                juce::Colours::black.withAlpha (0.16f * strength), 0.0f, r.getBottom(), false);
    light.addColour (0.4, juce::Colours::transparentBlack);
    g.setGradientFill (light);
    g.fillRoundedRectangle (r, radius);
}

// Kontur: oben ein heller Lichtsaum, unten eine dunkle Kante
inline void drawBevel (juce::Graphics& g, juce::Rectangle<float> r, float radius, float strength = 1.0f)
{
    juce::ColourGradient grad (juce::Colours::white.withAlpha (0.10f * strength), 0.0f, r.getY(),
                               juce::Colours::black.withAlpha (0.45f * strength), 0.0f, r.getBottom(), false);
    grad.addColour (0.35, juce::Colours::white.withAlpha (0.0f));
    grad.addColour (0.7, juce::Colours::black.withAlpha (0.0f));
    g.setGradientFill (grad);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

// Panel-Flaeche: Grundfarbe mit eigener Koernung, darueber das Licht, an den Kanten die Kontur
inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius, Colour base = kPanel, float grain = 1.0f)
{
    g.setColour (base);
    g.fillRoundedRectangle (r, radius);
    drawGrain (g, r, radius, grain);
    drawLight (g, r, radius);
    drawBevel (g, r, radius);
}

class QuadLookAndFeel : public juce::LookAndFeel_V4
{
public:
    QuadLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, kBg);
        setColour (juce::Label::textColourId, kText);
        setColour (juce::ComboBox::backgroundColourId, kPanelHi);
        setColour (juce::ComboBox::textColourId, kText);
        setColour (juce::ComboBox::outlineColourId, kLine);
        setColour (juce::ComboBox::arrowColourId, kDim);
        setColour (juce::PopupMenu::backgroundColourId, kPanel);
        setColour (juce::PopupMenu::textColourId, kText);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent.darker (0.5f));
        setColour (juce::PopupMenu::highlightedTextColourId, kText);
        setColour (juce::Slider::textBoxTextColourId, kText);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::trackColourId, kAccent);
        setColour (juce::Slider::backgroundColourId, kPanelHi);
        setColour (juce::TextButton::buttonColourId, kPanelHi);
        setColour (juce::TextButton::textColourOffId, kDim);
        setColour (juce::TextButton::textColourOnId, kBg);
        setColour (juce::TextEditor::backgroundColourId, kPanelHi);
        setColour (juce::TextEditor::textColourId, kText);
        setColour (juce::TooltipWindow::backgroundColourId, kPanelHi);
        setColour (juce::TooltipWindow::textColourId, kText);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
        const float d = std::min (r.getWidth(), r.getHeight());
        const auto c = r.getCentre();
        const float radius = d * 0.5f;
        const float arcR = radius - 2.0f;
        const auto col = s.findColour (juce::Slider::rotarySliderFillColourId);
        const float thick = 3.5f;

        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (kPanelHi);
        g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float ang = startAngle + pos * (endAngle - startAngle);
        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, ang, true);
        g.setColour (s.isEnabled() ? col : col.withAlpha (0.45f));
        g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Knopfkoerper: leicht gewoelbt (Verlauf), feine Kante, kleiner Schatten
        const float capR = arcR - 7.0f;
        float pr = arcR - 6.0f;
        if (capR > 6.0f)
        {
            const auto cap = juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillEllipse (cap.translated (0.0f, 1.5f));
            g.setGradientFill (juce::ColourGradient (kPanelHi.brighter (0.12f), c.x, cap.getY(), kPanel.darker (0.25f), c.x, cap.getBottom(), false));
            g.fillEllipse (cap);
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.drawEllipse (cap.reduced (0.5f), 1.0f);
            pr = capR - 2.0f;
        }

        g.setColour (kText.withAlpha (s.isEnabled() ? 1.0f : 0.4f));
        g.drawLine (c.x + std::sin (ang) * (pr * 0.35f), c.y - std::cos (ang) * (pr * 0.35f),
                    c.x + std::sin (ang) * pr, c.y - std::cos (ang) * pr, 2.0f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (2.0f, 3.0f);
        const bool on = b.getToggleState();
        const auto col = b.findColour (juce::ToggleButton::tickColourId);
        g.setColour (on ? col.withAlpha (b.isEnabled() ? 1.0f : 0.4f) : (highlighted ? kPanelHi.brighter (0.15f) : kPanelHi));
        g.fillRoundedRectangle (r, 6.0f);
        if (! on) { g.setColour (kLine.brighter (0.25f)); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f); }
        const auto& props = b.getProperties();
        const bool dryIcon = static_cast<bool> (props.getWithDefault ("dry", false));
        const bool noiseIcon = static_cast<bool> (props.getWithDefault ("noise", false));
        const bool gearIcon = static_cast<bool> (props.getWithDefault ("gear", false));
        const bool hasIcon = dryIcon || noiseIcon || gearIcon;
        const juce::Font font (ui::uiFont (hasIcon ? 11.5f : 12.5f, true));
        juce::GlyphArrangement ga;
        ga.addLineOfText (font, b.getButtonText(), 0.0f, 0.0f);
        const float tw = ga.getBoundingBox (0, -1, true).getWidth();
        (void) tw;
        const float x0 = r.getX() + 7.0f;   // Symbole aller Knoepfe stehen auf einer Linie, der Text mittig im Rest
        const float cy = r.getCentreY();
        const auto iconCol = on ? kBg : (dryIcon ? kDryColour : (noiseIcon ? kNoiseColour : col));
        if (dryIcon)   drawDryIcon (g, { x0 + 7.5f, cy }, iconCol);
        if (noiseIcon) drawNoiseIcon (g, juce::Rectangle<float> (x0, cy - 6.0f, 15.0f, 12.0f), iconCol);
        if (gearIcon)  drawGearIcon (g, { x0 + 7.5f, cy }, iconCol);
        g.setColour (on ? kBg : kDim);
        g.setFont (font);
        const auto textArea = hasIcon ? juce::Rectangle<float> (r.getX() + 26.0f, r.getY(), r.getWidth() - 28.0f, r.getHeight()) : r;
        g.drawText (b.getButtonText(), textArea, juce::Justification::centred);
    }

    // Zahlenfeld (Eigenschaft "numberbox"): schlichtes Feld, nur die Zahl.
    // Zahlenfeld fuer -1..+1 (Eigenschaft "bipolar"): Zahl frei lesbar, darunter eine schmale Leiste,
    // die von der Mitte aus fuellt
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        if (style == juce::Slider::LinearBar && s.getProperties().contains ("numberbox"))
        {
            const float rad = r.getHeight() * 0.5f;
            g.setColour (kBg.withAlpha (0.85f));
            g.fillRoundedRectangle (r, rad);
            g.setColour (s.isMouseOverOrDragging() ? kDim.withAlpha (0.8f) : kLine.withAlpha (0.8f));
            g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
            return;
        }
        if (style != juce::Slider::LinearBar || ! s.getProperties().contains ("bipolar"))
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s);
            return;
        }
        g.setColour (kBg);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (kLine);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        const auto track = r.reduced (5.0f, 0.0f).withTop (r.getBottom() - 6.0f).withHeight (3.0f);
        g.setColour (kPanelHi);
        g.fillRoundedRectangle (track, 1.5f);
        const float cx = track.getCentreX();
        const float pos = juce::jlimit (track.getX(), track.getRight(), sliderPos);
        const float a = std::min (cx, pos), b = std::max (cx, pos);
        g.setColour (s.findColour (juce::Slider::trackColourId).withMultipliedAlpha (s.isEnabled() ? 1.0f : 0.4f));
        g.fillRoundedRectangle (juce::Rectangle<float> (a, track.getY(), std::max (b - a, 2.0f), track.getHeight()), 1.5f);
        g.setColour (kText.withAlpha (0.35f));
        g.fillRect (juce::Rectangle<float> (cx - 0.5f, track.getY() - 1.0f, 1.0f, track.getHeight() + 2.0f));
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f);
        g.setColour (kPanelHi);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (kLine);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        juce::Path arrow;
        const float ax = (float) width - 14.0f, ay = (float) height * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (box.isEnabled() ? kDim : kDim.withAlpha (0.4f));
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (ui::uiFont (13.0f)); }

    // Reiter, Undo/Redo und andere Textknoepfe: Oberflaechen-Schrift
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override { return uiFont (std::min (14.0f, (float) buttonHeight * 0.55f)); }

    // Wertanzeigen der Regler und Zahlenfelder: Werte-Schrift
    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (valueFont (s.getSliderStyle() == juce::Slider::LinearBar ? 13.0f : 14.0f));
        return l;
    }
    juce::Font getPopupMenuFont() override { return juce::Font (ui::uiFont (14.0f)); }
};

// ---- Kleine Bedienelemente mit Parameter-Anbindung ----
using Apvts = juce::AudioProcessorValueTreeState;

// Aktueller Wert eines Parameters, direkt vom Parameter gelesen. Nicht die Rohwerte der APVTS oder den
// Zustand eines Knopfes verwenden, wenn man auf eine Parameteraenderung reagiert: JUCE benachrichtigt die
// Beobachter eines Parameters in umgekehrter Reihenfolge, spaetere Beobachter sehen sonst den alten Wert.
inline float currentValue (Apvts& apvts, const juce::String& id)
{
    auto* p = apvts.getParameter (id);
    return p->convertFrom0to1 (p->getValue());
}

inline juce::String fmtMs (double v) { return v < 100.0 ? juce::String (v, 1) + " ms" : juce::String (v, 0) + " ms"; }
inline juce::String fmtDb (double v) { return juce::String (v, 1) + " dB"; }
inline juce::String fmtPct (double v) { return juce::String (v * 100.0, 0) + " %"; }
inline juce::String fmtPctRaw (double v) { return juce::String (v, 1) + " %"; }
inline juce::String fmtHz (double v) { return v >= 1000.0 ? juce::String (v / 1000.0, 2) + " kHz" : juce::String (v, v < 100.0 ? 1 : 0) + " Hz"; }
inline juce::String fmtDeg (double v) { return juce::String (v, 0) + juce::String::fromUTF8 ("\xc2\xb0"); }
inline juce::String fmtNum (double v) { return juce::String (v, 2); }

inline double parseNum (const juce::String& t)
{
    double v = t.getDoubleValue();
    if (t.containsIgnoreCase ("k")) v *= 1000.0;
    return v;
}
inline double parsePct (const juce::String& t) { return t.getDoubleValue() / 100.0; }

// Notenwert (Index in qe::kNoteDivs) als Text und zurueck
inline juce::String fmtDiv (double v) { return qe::kNoteDivs[juce::jlimit (0, qe::kNumNoteDivs - 1, (int) std::lround (v))].name; }
inline double parseDiv (const juce::String& t)
{
    for (int i = 0; i < qe::kNumNoteDivs; ++i)
        if (t.trim() == qe::kNoteDivs[i].name) return (double) i;
    return t.getDoubleValue();
}

// Einheitliche Bedienung fuer Drehregler und Zahlenbalken:
// - Ziehen aendert den Wert (Drehregler: nach oben/rechts, Balken: nach rechts), mit Shift zehnmal feiner.
//   Die Aenderung wird schrittweise aufsummiert: Shift mitten im Ziehen laesst nichts springen.
// - Ein Klick ohne Ziehen oeffnet das Eingabefeld (Enter uebernimmt, Esc bricht ab).
// - Doppelklick setzt auf den Standardwert zurueck.
// Ein Klick auf einen Balken verstellt den Wert nicht (kein Springen zur Mausposition).
struct FineSlider : public juce::Slider, private juce::Timer
{
    static constexpr double kPixels = 250.0, kFineFactor = 10.0;   // Drehregler: Pixel fuer den ganzen Bereich

    FineSlider() { init(); }
    FineSlider (SliderStyle style, TextEntryBoxPosition box) : juce::Slider (style, box) { init(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        stopTimer();
        valueEditor.setVisible (false);
        lastPos = e.position;
        dragProp = valueToProportionOfLength (getValue());
        juce::Slider::mouseDown (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto st = getSliderStyle();
        if ((st != RotaryHorizontalVerticalDrag && st != LinearBar) || ! isEnabled() || e.mods.isPopupMenu())
        {
            juce::Slider::mouseDrag (e);
            return;
        }
        const bool numberBox = getProperties().contains ("numberbox");
        const double delta = (st == LinearBar && ! numberBox) ? (double) (e.position.x - lastPos.x)
                                                              : (double) ((e.position.x - lastPos.x) + (lastPos.y - e.position.y));
        const double pixels = ((st == LinearBar && ! numberBox) ? (double) std::max (getWidth(), 1) : kPixels) * (e.mods.isShiftDown() ? kFineFactor : 1.0);
        lastPos = e.position;
        dragProp = juce::jlimit (0.0, 1.0, dragProp + delta / pixels);
        setValue (proportionOfLengthToValue (dragProp), juce::sendNotificationSync);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        juce::Slider::mouseUp (e);
        // Einzelklick: Eingabe erst oeffnen, wenn kein Doppelklick mehr folgt
        if (isEnabled() && ! e.mouseWasDraggedSinceMouseDown() && e.getNumberOfClicks() == 1 && ! e.mods.isPopupMenu())
            startTimer (juce::MouseEvent::getDoubleClickTimeout() + 20);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        stopTimer();
        valueEditor.setVisible (false);
        juce::Slider::mouseDoubleClick (e);   // Standardwert (setDoubleClickReturnValue)
    }

    void resized() override
    {
        juce::Slider::resized();
        valueEditor.setBounds (getLookAndFeel().getSliderLayout (*this).textBoxBounds.expanded (0, 1));
    }

    // Die eingebaute Wertanzeige nur anzeigen lassen: alle Klicks gehen an den Regler (sie wird bei jedem
    // Stilwechsel neu erzeugt, daher hier). Zahlenfelder: kleine Schrift, Farbe je nach Wert (valueColour).
    void childrenChanged() override { styleValueLabel(); }
    void valueChanged() override { styleValueLabel(); }

    std::function<Colour (double)> valueColour;

private:
    void styleValueLabel()
    {
        for (auto* c : getChildren())
            if (auto* l = dynamic_cast<juce::Label*> (c))
            {
                l->setInterceptsMouseClicks (false, false);
                if (getProperties().contains ("numberbox")) l->setFont (valueFont (12.0f));
                if (valueColour) l->setColour (juce::Label::textColourId, valueColour (getValue()));
            }
    }

    void init()
    {
        setTextBoxIsEditable (false);
        setSliderSnapsToMousePosition (false);
        valueEditor.setJustification (juce::Justification::centred);
        valueEditor.setFont (valueFont (13.5f));
        valueEditor.setColour (juce::TextEditor::backgroundColourId, kBg);
        valueEditor.setColour (juce::TextEditor::textColourId, kText);
        valueEditor.setColour (juce::TextEditor::outlineColourId, kAccent);
        valueEditor.setColour (juce::TextEditor::focusedOutlineColourId, kAccent);
        valueEditor.setColour (juce::TextEditor::highlightColourId, kAccent.withAlpha (0.4f));
        valueEditor.onReturnKey = [this]
        {
            const auto text = valueEditor.getText();
            valueEditor.setVisible (false);
            if (text.containsAnyOf ("0123456789"))
                setValue (getValueFromText (text), juce::sendNotificationSync);
        };
        valueEditor.onEscapeKey = [this] { valueEditor.setVisible (false); };
        valueEditor.onFocusLost = [this] { valueEditor.setVisible (false); };
        addChildComponent (valueEditor);
    }

    void timerCallback() override
    {
        stopTimer();
        valueEditor.setBounds (getLookAndFeel().getSliderLayout (*this).textBoxBounds.expanded (0, 1));
        valueEditor.setText (getTextFromValue (getValue()), false);
        valueEditor.setVisible (true);
        valueEditor.toFront (true);
        valueEditor.selectAll();
    }

    juce::TextEditor valueEditor;
    juce::Point<float> lastPos;
    double dragProp = 0.0;
};

struct Knob : public juce::Component
{
    Knob (Apvts& apvts, const juce::String& id, const juce::String& title, Colour col,
          std::function<juce::String (double)> fmt, std::function<double (const juce::String&)> parse = parseNum)
    {
        addAndMakeVisible (slider);
        addAndMakeVisible (label);
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 16);
        slider.setColour (juce::Slider::rotarySliderFillColourId, col);
        slider.setColour (juce::Slider::textBoxTextColourId, kValue);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        label.setText (title, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (ui::uiFont (12.0f));
        label.setColour (juce::Label::textColourId, kDim);
        label.setInterceptsMouseClicks (false, false);
        attachment = std::make_unique<Apvts::SliderAttachment> (apvts, id, slider);
        slider.textFromValueFunction = fmt;
        slider.valueFromTextFunction = parse;
        if (auto* p = apvts.getParameter (id))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        slider.updateText();
    }
    void resized() override
    {
        auto b = getLocalBounds();
        if (label.isVisible()) label.setBounds (b.removeFromTop (16));
        slider.setBounds (b);
    }
    FineSlider slider;
    juce::Label label;
    std::unique_ptr<Apvts::SliderAttachment> attachment;
};

struct Toggle : public juce::ToggleButton
{
    Toggle (Apvts& apvts, const juce::String& id, const juce::String& text, Colour col, bool dry = false, bool noiseIcon = false)
    {
        setButtonText (text);
        setColour (juce::ToggleButton::tickColourId, dry ? kDryColour : col);
        getProperties().set ("dry", dry);
        getProperties().set ("noise", noiseIcon);
        attachment = std::make_unique<Apvts::ButtonAttachment> (apvts, id, *this);
    }
    std::unique_ptr<Apvts::ButtonAttachment> attachment;
};

} // namespace ui
