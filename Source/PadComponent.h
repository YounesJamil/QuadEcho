#pragma once
#include "UiExtras.h"

// Draufsicht: Hoerer in der Mitte, vorne oben, hinten unten, vier ziehbare Punkte A-D und die
// Noise-Quelle (Symbol). Ziehen = X/Y, Shift-Ziehen oder Mausrad = Z (Punktgroesse + Schatten).
// Nur Einheiten, die im Algorithmus zum Ausgang gehoeren, haben eine Position: die anderen sind
// ausgegraut und nicht ziehbar (Operator-Prinzip). Die Noise ist ziehbar, solange sie an ist.
class PadComponent : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr int kPoints = qe::kNumUnits + 1;   // A-D und Noise (Index 4)

    explicit PadComponent (juce::AudioProcessorValueTreeState& apvts)
    {
        auto* um = apvts.undoManager;
        for (int u = 0; u < kPoints; ++u)
        {
            const auto uu = (size_t) u;
            for (int k = 0; k < 3; ++k)
            {
                const juce::String id = u < qe::kNumUnits ? qe::paramId (u, qe::pPosX + k) : juce::String (qe::kGlobalIds[qe::nzPosX + k]);
                attachments[uu][(size_t) k] = std::make_unique<juce::ParameterAttachment> (
                    *apvts.getParameter (id),
                    [this, uu, k] (float v) { value[uu][(size_t) k] = v; repaint(); }, um);
                attachments[uu][(size_t) k]->sendInitialUpdate();
            }
            const juce::String onId = u < qe::kNumUnits ? qe::paramId (u, qe::pOn) : juce::String (qe::kGlobalIds[qe::nzOn]);
            onAttachments[uu] = std::make_unique<juce::ParameterAttachment> (
                *apvts.getParameter (onId), [this, uu] (float v) { unitOn[uu] = v > 0.5f; repaint(); });
            onAttachments[uu]->sendInitialUpdate();
        }
        algoAttachment = std::make_unique<juce::ParameterAttachment> (
            *apvts.getParameter (qe::kIdAlgo), [this] (float v) { algo = (int) (v + 0.5f); repaint(); });
        algoAttachment->sendInitialUpdate();
        binAttachment = std::make_unique<juce::ParameterAttachment> (
            *apvts.getParameter (qe::kIdBinaural), [this] (float v) { binaural = v > 0.5f; repaint(); });
        binAttachment->sendInitialUpdate();
        setTooltip ("Drag a point: X = left/right, Y = front/back (distance from the listener).\nShift-drag or mouse wheel: height (Z). Double-click: Z back to 0.\nThe noise source (symbol) can be placed too once it is on.");
    }

    // Die Noise-Quelle ist nur im Expert-Modus sichtbar (und nur dann ziehbar)
    void setShowNoise (bool show) { showNoise = show; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto full = getLocalBounds().toFloat();
        ui::drawPanel (g, full, 10.0f);

        const auto pa = padArea();
        const auto c = pa.getCentre();
        const float hw = pa.getWidth() * 0.5f;

        g.setColour (ui::kBg.withAlpha (0.35f));
        g.fillRoundedRectangle (pa, 6.0f);
        g.setFont (ui::uiFont (11.0f));
        auto listener = [&g] (juce::Point<float> h)   // Hoerer, Nase zeigt nach oben (vorne)
        {
            g.setColour (ui::kText);
            g.fillEllipse (h.x - 8.0f, h.y - 8.0f, 16.0f, 16.0f);
            g.fillEllipse (h.x - 12.0f, h.y - 3.0f, 5.0f, 8.0f);
            g.fillEllipse (h.x + 7.0f, h.y - 3.0f, 5.0f, 8.0f);
            juce::Path nose;
            nose.addTriangle (h.x - 3.0f, h.y - 7.0f, h.x + 3.0f, h.y - 7.0f, h.x, h.y - 13.0f);
            g.fillPath (nose);
        };
        if (binaural)
        {
            // Raum um den Kopf: Entfernungsringe als echte Kreise (Radius 1/3, 2/3, 1), Hoerer in der Mitte
            for (int i = 1; i <= 3; ++i)
            {
                const float f = (float) i / 3.0f;
                g.setColour (ui::kLine.withAlpha (i == 3 ? 0.95f : 0.55f));
                g.drawEllipse (c.x - hw * f, c.y - hw * f, hw * f * 2.0f, hw * f * 2.0f, 1.0f);
            }
            g.setColour (ui::kLine.withAlpha (0.6f));
            g.drawLine (c.x, pa.getY(), c.x, pa.getBottom(), 1.0f);
            g.drawLine (pa.getX(), c.y, pa.getRight(), c.y, 1.0f);
            g.setColour (ui::kDim);
            g.drawText ("FRONT", juce::Rectangle<float> (c.x - 30.0f, full.getY() + 4.0f, 60.0f, 14.0f), juce::Justification::centred);
            g.drawText ("BACK", juce::Rectangle<float> (c.x - 30.0f, pa.getBottom() + 2.0f, 60.0f, 14.0f), juce::Justification::centred);
            listener (c);
        }
        else
        {
            // Stereofeld als Buehne: Tiefenlinien (vordere Haelfte praesent), Panorama-Linien, Hoerer unten davor
            for (int i = 1; i <= 3; ++i)
            {
                const float yy = pa.getBottom() - pa.getHeight() * (float) i / 4.0f;
                g.setColour (ui::kLine.withAlpha (i == 2 ? 0.75f : 0.45f));
                g.drawHorizontalLine ((int) yy, pa.getX() + 4.0f, pa.getRight() - 4.0f);
            }
            g.setColour (ui::kLine.withAlpha (0.6f));
            g.drawLine (c.x, pa.getY(), c.x, pa.getBottom(), 1.0f);
            g.setColour (ui::kLine.withAlpha (0.95f));
            g.drawLine (pa.getX(), pa.getBottom() - 0.5f, pa.getRight(), pa.getBottom() - 0.5f, 1.4f);   // Buehnenkante
            g.setColour (ui::kDim);
            g.drawText ("BACK", juce::Rectangle<float> (c.x - 30.0f, full.getY() + 4.0f, 60.0f, 14.0f), juce::Justification::centred);
            g.drawText ("FRONT", juce::Rectangle<float> (c.x + 16.0f, pa.getBottom() + 2.0f, 60.0f, 14.0f), juce::Justification::centredLeft);
            listener ({ c.x, pa.getBottom() + 12.0f });
        }
        g.setColour (ui::kDim);
        g.drawText ("L", juce::Rectangle<float> (pa.getX() - 18.0f, c.y - 8.0f, 14.0f, 16.0f), juce::Justification::centred);
        g.drawText ("R", juce::Rectangle<float> (pa.getRight() + 4.0f, c.y - 8.0f, 14.0f, 16.0f), juce::Justification::centred);

        // Punkte
        for (int u = 0; u < kPoints; ++u)
        {
            const auto uu = (size_t) u;
            const bool isNoise = u == qe::kNumUnits;
            if (isNoise && ! showNoise) continue;
            const auto p = pointPos (u);
            const float z = value[uu][2];
            const float r = 9.0f + (z + 1.0f) * 5.0f;
            const bool placed = isPlaced (u);
            const float alpha = placed ? (unitOn[uu] ? 1.0f : 0.6f) : 0.3f;
            const auto colour = isNoise ? ui::kNoiseColour : ui::unitColour (u);

            g.setColour (juce::Colours::black.withAlpha (0.45f * alpha));
            g.fillEllipse (juce::Rectangle<float> (r * 1.5f, r * 0.8f).withCentre ({ p.x, p.y + 4.0f + (z + 1.0f) * 9.0f }));

            const auto circle = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p);
            g.setColour (colour.withAlpha (alpha));
            g.fillEllipse (circle);

            // Gestrichelter Ring: Einheit ist aus (platzierbar) bzw. Zuspielung (nicht platziert)
            if (! placed || ! unitOn[uu])
            {
                juce::Path ring;
                ring.addEllipse (circle.expanded (3.0f));
                juce::Path dashed;
                const float dashes[] = { 3.0f, 3.0f };
                juce::PathStrokeType (1.2f).createDashedStroke (dashed, ring, dashes, 2);
                g.setColour (ui::kText.withAlpha (0.6f));
                g.fillPath (dashed);
            }
            g.setColour ((u == dragged ? ui::kText : ui::kBg).withAlpha (0.9f));
            g.drawEllipse (circle, u == dragged ? 2.0f : 1.2f);
            if (isNoise)
                ui::drawNoiseIcon (g, circle.reduced (r * 0.35f), ui::kBg);
            else
            {
                g.setColour (ui::kBg);
                g.setFont (ui::uiFont (juce::jmax (11.0f, r), true));
                g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + u)), circle, juce::Justification::centred);
            }
        }

        // Kopfzeile: Hinweis oder Werte waehrend des Ziehens
        auto head = full.reduced (8.0f, 4.0f).removeFromBottom (16.0f);   // Fusszeile unter dem Pad
        g.setFont (ui::uiFont (11.0f));
        if (dragged >= 0)
        {
            const auto uu = (size_t) dragged;
            g.setColour (ui::kText);
            g.drawText ((dragged < qe::kNumUnits ? juce::String::charToString ((juce::juce_wchar) ('A' + dragged)) : juce::String ("Noise"))
                            + "   X " + juce::String (value[uu][0], 2) + "   Y " + juce::String (value[uu][1], 2) + "   Z " + juce::String (value[uu][2], 2),
                        head, juce::Justification::centred);
        }
        else
        {
            g.setColour (ui::kDim);
            juce::String placedList;
            for (int u = 0; u < qe::kNumUnits; ++u)
                if (isPlaced (u)) placedList << (placedList.isEmpty() ? "" : ", ") << juce::String::charToString ((juce::juce_wchar) ('A' + u));
            if (showNoise && unitOn[(size_t) qe::kNumUnits]) placedList << (placedList.isEmpty() ? "" : ", ") << "Noise";
            g.drawText ("Placed: " + placedList + (binaural ? "   |   Binaural" : ""), head, juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragged = nearest (e.position);
        if (dragged < 0) return;
        for (auto& a : attachments[(size_t) dragged]) a->beginGesture();
        lastPos = e.position;
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragged < 0) return;
        const auto uu = (size_t) dragged;
        if (e.mods.isShiftDown())
        {
            setValue (dragged, 2, juce::jlimit (-1.0f, 1.0f, value[uu][2] - (e.position.y - lastPos.y) / 90.0f));
        }
        else
        {
            const auto pa = padArea();
            const auto c = pa.getCentre();
            setValue (dragged, 0, juce::jlimit (-1.0f, 1.0f, (e.position.x - c.x) / (pa.getWidth() * 0.5f)));
            setValue (dragged, 1, juce::jlimit (-1.0f, 1.0f, ySign() * (c.y - e.position.y) / (pa.getHeight() * 0.5f)));
        }
        lastPos = e.position;
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragged < 0) return;
        for (auto& a : attachments[(size_t) dragged]) a->endGesture();
        dragged = -1;
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int u = nearest (e.position, 40.0f);
        if (u < 0) return;
        attachments[(size_t) u][2]->setValueAsCompleteGesture (juce::jlimit (-1.0f, 1.0f, value[(size_t) u][2] + w.deltaY * 0.5f));
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int u = nearest (e.position);
        if (u < 0) return;
        attachments[(size_t) u][2]->setValueAsCompleteGesture (0.0f);
    }

private:
    // Einheiten: die am Ausgang hoerbar sind (ist eine Ausgangseinheit aus, die letzte eingeschaltete davor)
    // und Ausgaenge, in deren Zweig noch nichts an ist (vorab platzierbar); Noise: solange sie an ist
    bool isPlaced (int u) const
    {
        if (u == qe::kNumUnits) return showNoise && unitOn[(size_t) u];
        std::array<bool, qe::kNumUnits> on {}, none {};
        for (int k = 0; k < qe::kNumUnits; ++k) on[(size_t) k] = unitOn[(size_t) k];
        const auto p = qe::placementOf (algo, on, none, none);
        return (p.weight[(size_t) u] > 0.0f && on[(size_t) u]) || p.idle[(size_t) u];
    }

    // Quadrat in der Mitte: X und Y haben denselben Massstab wie im DSP (Radius = Entfernung)
    juce::Rectangle<float> padArea() const
    {
        const auto b = getLocalBounds().toFloat().withTrimmedTop (22.0f).withTrimmedBottom (38.0f).reduced (22.0f, 0.0f);
        const float side = std::min (b.getWidth(), b.getHeight());
        return juce::Rectangle<float> (side, side).withCentre (b.getCentre());
    }

    juce::Point<float> pointPos (int u) const
    {
        const auto pa = padArea();
        const auto c = pa.getCentre();
        const auto uu = (size_t) u;
        return { c.x + value[uu][0] * pa.getWidth() * 0.5f, c.y - ySign() * value[uu][1] * pa.getHeight() * 0.5f };
    }
    // Y = +1 ist immer "vorne": bei Binaural oben (vor dem Hoerer), auf der Stereo-Buehne unten (Buehnenkante, nah)
    float ySign() const { return binaural ? 1.0f : -1.0f; }

    int nearest (juce::Point<float> p, float maxDist = 26.0f) const
    {
        int best = -1;
        float bestD = maxDist;
        for (int u = 0; u < kPoints; ++u)
        {
            if (! isPlaced (u)) continue;   // ausgegraute Punkte sind nicht ziehbar
            const float d = pointPos (u).getDistanceFrom (p);
            if (d <= bestD) { bestD = d; best = u; }
        }
        return best;
    }

    void setValue (int u, int k, float v)
    {
        value[(size_t) u][(size_t) k] = v;
        attachments[(size_t) u][(size_t) k]->setValueAsPartOfGesture (v);
        repaint();
    }

    std::array<std::array<std::unique_ptr<juce::ParameterAttachment>, 3>, kPoints> attachments;
    std::array<std::unique_ptr<juce::ParameterAttachment>, kPoints> onAttachments;
    std::unique_ptr<juce::ParameterAttachment> algoAttachment, binAttachment;
    std::array<std::array<float, 3>, kPoints> value {};
    std::array<bool, kPoints> unitOn { { true, true, true, true, false } };
    int algo = 9;
    bool showNoise = false;
    bool binaural = false;
    int dragged = -1;
    juce::Point<float> lastPos;
};
