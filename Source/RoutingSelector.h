#pragma once
#include "UiCommon.h"

// 10 Algorithmen als kleine Blockdiagramme, Auswahl per Klick.
class RoutingSelector : public juce::Component
{
public:
    explicit RoutingSelector (juce::AudioProcessorValueTreeState& apvts)
        : attachment (*apvts.getParameter (qe::kIdAlgo), [this] (float v) { selected = (int) (v + 0.5f); repaint(); })
    {
        attachment.sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < qe::kNumAlgorithms; ++i)
            paintTile (g, i, tileBounds (i));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = hit (e.position);
        if (i >= 0) attachment.setValueAsCompleteGesture ((float) i);
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        const int i = hit (e.position);
        if (i != hover) { hover = i; repaint(); }
    }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

private:
    static constexpr int kCols = 5, kRows = 2;

    juce::Rectangle<float> tileBounds (int i) const
    {
        const float w = (float) getWidth() / kCols, h = (float) getHeight() / kRows;
        return { (float) (i % kCols) * w, (float) (i / kCols) * h, w, h };
    }
    int hit (juce::Point<float> p) const
    {
        for (int i = 0; i < qe::kNumAlgorithms; ++i)
            if (tileBounds (i).contains (p)) return i;
        return -1;
    }

    void paintTile (juce::Graphics& g, int idx, juce::Rectangle<float> tile) const
    {
        const auto& algo = qe::kAlgorithms[idx];
        const bool sel = idx == selected;
        auto b = tile.reduced (3.0f);
        ui::drawPanel (g, b, 7.0f, sel ? ui::kPanelHi.brighter (0.1f) : (idx == hover ? ui::kPanelHi : ui::kPanel), 0.6f);
        g.setColour (sel ? ui::kAccent : ui::kLine.withAlpha (0.6f));
        g.drawRoundedRectangle (b, 7.0f, sel ? 1.8f : 1.0f);

        // Tiefe jeder Einheit = Reihe im Diagramm
        int depth[qe::kNumUnits] = {};
        int maxDepth = 0;
        for (int u = 0; u < qe::kNumUnits; ++u)
        {
            for (int j = 0; j < u; ++j)
                if (algo.sources[(size_t) u] & (1 << j)) depth[u] = std::max (depth[u], depth[j] + 1);
            maxDepth = std::max (maxDepth, depth[u]);
        }
        auto area = b.reduced (8.0f, 6.0f).withTrimmedBottom (14.0f);
        const float box = 10.0f;
        juce::Point<float> pos[qe::kNumUnits];
        for (int d = 0; d <= maxDepth; ++d)
        {
            int count = 0;
            for (int u = 0; u < qe::kNumUnits; ++u) if (depth[u] == d) ++count;
            int k = 0;
            for (int u = 0; u < qe::kNumUnits; ++u)
                if (depth[u] == d)
                {
                    const float px = area.getX() + area.getWidth() * ((float) k + 0.5f) / (float) count;
                    const float py = maxDepth == 0 ? area.getCentreY()
                                                   : area.getY() + box * 0.5f + (area.getHeight() - box) * (float) d / (float) maxDepth;
                    pos[u] = { px, py };
                    ++k;
                }
        }

        // Verbindungen
        g.setColour (ui::kDim.withAlpha (0.8f));
        for (int u = 0; u < qe::kNumUnits; ++u)
            for (int j = 0; j < u; ++j)
                if (algo.sources[(size_t) u] & (1 << j))
                    g.drawLine (pos[j].x, pos[j].y + box * 0.5f, pos[u].x, pos[u].y - box * 0.5f, 1.2f);

        // Einheiten + Ausgangsmarkierung
        g.setFont (ui::uiFont (8.5f, true));
        for (int u = 0; u < qe::kNumUnits; ++u)
        {
            const auto r = juce::Rectangle<float> (box, box).withCentre (pos[u]);
            const bool isOut = (algo.outputs & (1 << u)) != 0;
            if (isOut)
            {
                g.setColour (ui::kAccent);
                g.drawLine (pos[u].x, r.getBottom(), pos[u].x, r.getBottom() + 4.0f, 1.6f);
            }
            g.setColour (ui::unitColour (u));
            g.fillRoundedRectangle (r, 2.5f);
            g.setColour (ui::kBg);
            g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + u)), r, juce::Justification::centred);
        }

        g.setColour (sel ? ui::kText : ui::kDim);
        g.setFont (ui::uiFont (10.0f));
        g.drawText (algo.name, b.removeFromBottom (14.0f), juce::Justification::centred);
    }

    juce::ParameterAttachment attachment;
    int selected = 0, hover = -1;
};
