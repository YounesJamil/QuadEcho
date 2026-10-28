#pragma once
#include "PluginProcessor.h"
#include "UiExtras.h"

// ---------------------------------------------------------------------------
// Echo-Anzeige: die ersten Wiederholungen der gewaehlten Einheit (aus den Einstellungen berechnet)
class EchoDisplay : public juce::Component, private juce::Timer
{
public:
    EchoDisplay (QuadEchoProcessor& p, int unitIndex) : proc (p), reader (p.apvts), unit (unitIndex)
    {
        setInterceptsMouseClicks (false, false);
        gFb = p.apvts.getRawParameterValue (qe::kIdGlobalFb);
        startTimerHz (20);
    }

    void paint (juce::Graphics& g) override
    {
        qe::EchoSettings es; qe::PosSettings ps;
        reader.readUnit (unit, es, ps);
        const auto col = ui::unitColour (unit);
        auto b = getLocalBounds().toFloat().reduced (10.0f, 22.0f);
        const float fb = es.freeze ? 1.0f : std::min (es.feedback * gFb->load(), 1.1f);   // Freeze haelt die Wiederholungen

        double tl, tr;
        qe::delayTimesMs (es, proc.getBpm(), tl, tr);
        struct Ev { double t; float amp; int row; };
        std::vector<Ev> ev;
        if (es.mode == qe::PingPong)
        {
            double t = 0.0;
            for (int k = 1; k <= 24; ++k)
            {
                t += (k % 2 == 1) ? tl : tr;
                const float amp = std::pow (fb, (float) (k - 1));
                if (amp < 0.03f || t > 6000.0) break;
                ev.push_back ({ t, std::min (amp, 1.0f), (k - 1) % 2 });
            }
        }
        else
        {
            for (int row = 0; row < 2; ++row)
                for (int k = 1; k <= 24; ++k)
                {
                    const double t = (double) k * (row == 0 ? tl : tr);
                    const float amp = std::pow (fb, (float) (k - 1));
                    if (amp < 0.03f || t > 6000.0) break;
                    ev.push_back ({ t, std::min (amp, 1.0f), row });
                }
        }
        // x-Achse in Takten des Host-Tempos: 1, 2, 4 oder 8 Takte, so dass die ersten vier Wiederholungen
        // hineinpassen. Bleibt stehen, solange sich die Delay-Zeit nur wenig aendert.
        const double barMs = 4.0 * 60000.0 / std::max (20.0, proc.getBpm());
        std::vector<double> times;
        for (auto& e : ev) times.push_back (e.t);
        std::sort (times.begin(), times.end());
        const double need = times.empty() ? std::max (tl, tr) : times[std::min<size_t> (3, times.size() - 1)] * 1.05;
        int bars = 1;
        while (bars < 8 && (double) bars * barMs < need) bars *= 2;
        const double tMax = (double) bars * barMs;

        const bool dim = ! es.on || qe::isMuted (es);
        const float alpha = dim ? 0.3f : 1.0f;
        const bool ms = es.mode == qe::MidSide;
        const char* rowName[2] = { ms ? "M" : "L", ms ? "S" : "R" };
        const float rowH = b.getHeight() * 0.5f - 4.0f;
        const float x0 = b.getX() + 16.0f, w = b.getWidth() - 16.0f;

        // Raster: Takte kraeftig, Viertel mittel, Sechzehntel fein (nur wenn genug Platz)
        const int sixteenths = bars * 16;
        for (int k = 1; k < sixteenths; ++k)
        {
            const bool bar = k % 16 == 0, beat = k % 4 == 0;
            if (! bar && ! (beat && bars <= 4) && ! (bars <= 1)) continue;
            const float x = x0 + w * (float) k / (float) sixteenths;
            g.setColour (ui::kLine.withAlpha (bar ? 0.9f : (beat ? 0.45f : 0.18f)));
            g.drawVerticalLine ((int) x, b.getY(), b.getBottom());
        }

        g.setFont (ui::uiFont (11.0f));
        for (int row = 0; row < 2; ++row)
        {
            const float base = b.getY() + (row + 1) * (rowH + 4.0f) - 4.0f;
            g.setColour (ui::kDim);
            g.drawText (rowName[row], (int) b.getX(), (int) (base - rowH), 14, (int) rowH, juce::Justification::centred);
            g.setColour (ui::kLine);
            g.drawHorizontalLine ((int) base, x0, x0 + w);
            // Eingangs-Impuls
            g.setColour (ui::kText.withAlpha (0.55f * alpha));
            g.fillRect (juce::Rectangle<float> (x0, base - rowH * 0.9f, 2.0f, rowH * 0.9f));
        }
        for (auto& e : ev)
        {
            if (e.t > tMax) continue;
            const float x = x0 + (float) (e.t / tMax) * w;
            const float base = b.getY() + (e.row + 1) * (rowH + 4.0f) - 4.0f;
            const float h = std::max (2.0f, e.amp * rowH * 0.9f);
            g.setColour (col.withAlpha (alpha * (0.35f + 0.65f * e.amp)));
            g.fillRoundedRectangle (x - 2.0f, base - h, 4.0f, h, 1.5f);
        }

        // Taktnummern unter der Achse, rechts die Laenge
        g.setColour (ui::kDim);
        for (int k = 0; k < bars; k += (bars > 4 ? 2 : 1))
            g.drawText (juce::String (k + 1), (int) (x0 + w * (float) k / (float) bars) + 2, (int) (b.getBottom() + 1), 20, 14, juce::Justification::centredLeft);
        g.drawText (juce::String (bars) + (bars == 1 ? " bar" : " bars"), (int) (x0 + w - 60), (int) (b.getBottom() + 1), 60, 14, juce::Justification::centredRight);
        g.drawText (dim ? "unit silent" : (es.freeze ? "frozen" : "repeats"), (int) b.getX(), (int) (b.getY() - 18), (int) b.getWidth(), 14, juce::Justification::centredRight);
    }

    void setUnit (int u) { unit = u; repaint(); }

private:
    void timerCallback() override { if (isShowing()) repaint(); }
    QuadEchoProcessor& proc;
    qe::ParamReader reader;
    std::atomic<float>* gFb = nullptr;
    int unit;
};

// ---------------------------------------------------------------------------
// Modulationsanzeige: zeigt live, was LFO und Huellkurve gerade bewirken
class ModDisplay : public juce::Component, private juce::Timer
{
public:
    ModDisplay (QuadEchoProcessor& p, int unitIndex) : proc (p), reader (p.apvts), unit (unitIndex)
    {
        setInterceptsMouseClicks (false, false);
        envHist.fill (0.0f);
        startTimerHz (60);
    }

    void setUnit (int u) { unit = u; envHist.fill (0.0f); repaint(); }

    void paint (juce::Graphics& g) override
    {
        qe::EchoSettings es; qe::PosSettings ps;
        reader.readUnit (unit, es, ps);
        const auto& du = proc.getEngine().unit (unit);

        // Gleicher Stil wie die Karten; Kurven in den Funktionsfarben: LFO = Modulation (pink),
        // Huellkurve = Eingang (Dry, creme), Delay = Einheitsfarbe, Filter = lila
        const juce::Colour lfoC = ui::kModColour, envC = ui::kDryColour, delayC = ui::unitColour (unit), filterC (0xffbb9cd6);
        auto full = getLocalBounds().toFloat();
        g.setColour (ui::kBg.withAlpha (0.6f));
        g.fillRoundedRectangle (full, 8.0f);
        g.setColour (ui::kText.withAlpha (0.09f));
        g.drawRoundedRectangle (full.reduced (0.5f), 8.0f, 1.0f);

        g.setColour (ui::kModColour);
        g.fillRoundedRectangle (juce::Rectangle<float> (10.0f, 6.0f, 3.0f, 9.0f), 1.5f);
        g.setFont (ui::uiFont (10.5f, true));
        g.setColour (ui::kDim);
        g.drawText ("LIVE MODULATION", 18, 3, 200, 14, juce::Justification::centredLeft);
        g.setFont (ui::uiFont (10.5f));
        g.drawText ("last 4 s, right = now", 130, 3, 200, 14, juce::Justification::centredLeft);

        const float delayRangeMs = 10.0f * (es.x4 ? 4.0f : 1.0f);
        const float delayScale = es.delayDepth * delayRangeMs;      // ms bei Modulation = 1
        const float filterScale = es.filterDepth * 2.0f;            // Oktaven bei Modulation = 1
        const bool filterActive = es.hpOn || es.lpOn;
        const float envMix = es.envMix;
        const double posNow = du.getLfoPos();
        const float rate = du.getLfoRateHz();
        const float phaseOff = es.phaseDeg / 360.0f;
        constexpr float window = 4.0f;

        auto lfoAt = [&] (float tau, bool right)
        {
            return (es.lfoOn ? 1.0f : 0.0f) * qe::lfoWave (es.wave, posNow + (right ? phaseOff : 0.0f) - (double) (rate * tau));
        };
        auto lfoFilterAt = [&] (float tau)
        {
            return (es.lfoOn ? 1.0f : 0.0f) * qe::lfoWave (es.wave, posNow + es.filterPhaseDeg / 360.0 - (double) (rate * tau));
        };
        auto envAt = [&] (float tau)
        {
            const int back = juce::jlimit (0, kHist - 1, (int) std::round (tau * 60.0f));
            return envHist[(size_t) ((head - 1 - back + 2 * kHist) % kHist)];
        };

        const juce::Font laneFont (ui::uiFont (11.0f, true));
        auto area = getLocalBounds().reduced (10, 20);
        const int laneH = area.getHeight() / 4;

        struct Lane { const char* name; juce::Colour col; bool bipolar; bool active; juce::String now; };
        const float mixNowL = lfoAt (0, false) * (1.0f - envMix) + envAt (0) * envMix;
        const float mixNowR = lfoAt (0, true) * (1.0f - envMix) + envAt (0) * envMix;
        auto noNegZero = [] (float v) { return std::abs (v) < 0.005f ? 0.0f : v; };   // kein "+-0.00"
        const float dNow = noNegZero (0.5f * (mixNowL + mixNowR) * delayScale);
        const float fNow = noNegZero ((lfoFilterAt (0) * (1.0f - envMix) + envAt (0) * envMix) * filterScale);
        const Lane lanes[4] = {
            { "LFO",       lfoC,    true,  es.lfoOn && envMix < 0.999f, juce::String (lfoAt (0, false), 2) },
            { "ENVELOPE",  envC,    false, envMix > 0.001f, juce::String ((int) std::round (envAt (0) * envMix * 100.0f)) + " %" },
            { "DELAY",     delayC,  true,  es.delayDepth > 0.001f, (dNow >= 0 ? "+" : "") + juce::String (dNow, 1) + " ms" },
            { "FILTER",    filterC, true,  es.filterDepth > 0.001f && filterActive, (fNow >= 0 ? "+" : "") + juce::String (fNow, 2) + " oct" } };

        for (int i = 0; i < 4; ++i)
        {
            auto lane = area.removeFromTop (laneH).reduced (0, 2);
            const float alpha = lanes[i].active ? 1.0f : 0.3f;
            g.setFont (laneFont);
            g.setColour (alpha < 1.0f ? ui::kDim.withAlpha (0.5f) : ui::kDim);
            g.drawText (lanes[i].name, lane.removeFromLeft (84), juce::Justification::centredLeft);
            g.setColour (lanes[i].col.withAlpha (alpha));
            g.drawText (lanes[i].now, lane.removeFromRight (84), juce::Justification::centredRight);

            auto r = lane.toFloat().reduced (4.0f, 0.0f);
            g.setColour (ui::kPanelHi.withAlpha (0.45f));
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (ui::kLine.withAlpha (0.5f));
            for (int k = 1; k < 8; ++k) g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) k / 8.0f), r.getY(), r.getBottom());
            if (lanes[i].bipolar) { g.setColour (ui::kLine); g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight()); }

            auto yOf = [&] (float v)
            {
                v = juce::jlimit (-1.0f, 1.0f, v);
                return lanes[i].bipolar ? r.getCentreY() - v * (r.getHeight() * 0.5f - 2.0f)
                                        : r.getBottom() - 2.0f - juce::jlimit (0.0f, 1.0f, v) * (r.getHeight() - 4.0f);
            };
            // Wert der Lane zur Zeit tau (vor "jetzt"), normiert auf -1..1 bzw. 0..1
            auto valueAt = [&] (float tau, bool right)
            {
                const float e = envAt (tau);
                const float mix = lfoAt (tau, right) * (1.0f - envMix) + e * envMix;
                switch (i)
                {
                    case 0: return lfoAt (tau, right);
                    case 1: return e * envMix;                   // waechst mit Envelope mix
                    case 2: return mix * es.delayDepth;          // Amplitude waechst mit der Depth
                    default: return (lfoFilterAt (tau) * (1.0f - envMix) + e * envMix) * es.filterDepth;
                }
            };

            const int twoTraces = (i == 0 || i == 2) && std::abs (phaseOff) > 0.002f ? 2 : 1;
            for (int t = 0; t < twoTraces; ++t)
            {
                juce::Path path;
                const int w = (int) r.getWidth();
                for (int px = 0; px <= w; px += 2)
                {
                    const float tau = window * (1.0f - (float) px / (float) w);
                    const float x = r.getX() + (float) px, y = yOf (valueAt (tau, t == 1));
                    if (px == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
                }
                const auto col = lanes[i].col.withAlpha (alpha * (t == 1 ? 0.6f : 1.0f));
                if (i == 1)
                {
                    juce::Path fill (path);
                    fill.lineTo (r.getRight(), r.getBottom() - 2.0f);
                    fill.lineTo (r.getX(), r.getBottom() - 2.0f);
                    fill.closeSubPath();
                    g.setColour (lanes[i].col.withAlpha (0.18f * alpha));
                    g.fillPath (fill);
                }
                g.setColour (col);
                g.strokePath (path, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            // "Jetzt"-Linie
            g.setColour (lanes[i].col.withAlpha (0.5f * alpha));
            g.drawVerticalLine ((int) r.getRight() - 1, r.getY(), r.getBottom());
        }
    }

private:
    void timerCallback() override
    {
        envHist[(size_t) head] = proc.getEngine().unit (unit).getEnvValue();
        head = (head + 1) % kHist;
        if (isShowing()) repaint();
    }

    static constexpr int kHist = 300;   // 5 s bei 60 Hz
    QuadEchoProcessor& proc;
    qe::ParamReader reader;
    int unit;
    std::array<float, kHist> envHist;
    int head = 0;
};

// ---------------------------------------------------------------------------
// Einheiten-Chip: Auswahl, An/Aus (Punkt: gefuellt = an, Ring = aus), Mute, Solo, Echo-Pegel
class UnitChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    UnitChip (QuadEchoProcessor& p, int unitIndex, std::function<void (int)> onSelect)
        : unit (unitIndex), select (std::move (onSelect)),
          muteBtn (p.apvts, qe::paramId (unitIndex, qe::pMute), "M", juce::Colour (0xffec7a5c)),
          soloBtn (p.apvts, qe::paramId (unitIndex, qe::pSolo), "S", juce::Colour (0xffeec265)),
          meter ([&p, unitIndex] { return p.getEngine().takeUnitPeak (unitIndex); }, ui::unitColour (unitIndex)),
          onAttachment (*p.apvts.getParameter (qe::paramId (unitIndex, qe::pOn)),
                        [this] (float v) { unitOn = v > 0.5f; repaint(); }, &p.undoManager),
          muteWatch (*p.apvts.getParameter (qe::paramId (unitIndex, qe::pMute)),
                     [this] (float v) { muted = v > 0.5f; repaint(); }),
          freezeWatch (*p.apvts.getParameter (qe::paramId (unitIndex, qe::pFreeze)),
                       [this] (float v) { frozen = v > 0.5f; repaint(); })
    {
        muteBtn.setTooltip (ui::tooltipForUnitParam (qe::pMute));
        soloBtn.setTooltip (ui::tooltipForUnitParam (qe::pSolo));
        setTooltip ("Click: edit this unit. Click the letter: unit on (filled) / off (ring). Blue ring: frozen. Bar: echo level.");
        addAndMakeVisible (muteBtn);
        addAndMakeVisible (soloBtn);
        addAndMakeVisible (meter);
        onAttachment.sendInitialUpdate();
        muteWatch.sendInitialUpdate();
        freezeWatch.sendInitialUpdate();
    }

    void setSelected (bool s) { selected = s; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto col = ui::unitColour (unit);
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        ui::drawPanel (g, b, 8.0f, selected ? ui::kPanelHi.brighter (0.08f) : ui::kPanel, 0.8f);
        g.setColour (selected ? col : ui::kLine.withAlpha (0.6f));
        g.drawRoundedRectangle (b, 8.0f, selected ? 1.8f : 1.0f);

        // An/Aus: gefuellter Punkt = an, Ring = aus
        const auto dot = dotBounds();
        if (unitOn)
        {
            g.setColour (col);
            g.fillEllipse (dot);
        }
        else
        {
            g.setColour (col.withAlpha (0.55f));
            g.drawEllipse (dot.reduced (1.0f), 1.6f);
        }
        g.setColour (unitOn ? ui::kBg : col.withAlpha (0.7f));
        g.setFont (ui::uiFont (15.0f, true));
        g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + unit)), dot.toNearestInt(), juce::Justification::centred);

        // Freeze: eisblauer Ring um den Punkt (Mute zeigt der leuchtende M-Knopf)
        if (frozen && unitOn)
        {
            g.setColour (ui::kNoiseColour);
            g.drawEllipse (dot.expanded (3.0f), 1.6f);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (6);
        meter.setBounds (b.removeFromBottom (7));
        b.removeFromBottom (3);
        auto right = b.removeFromRight (56);
        muteBtn.setBounds (right.removeFromLeft (28));
        soloBtn.setBounds (right.removeFromLeft (28));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (dotBounds().expanded (3.0f).contains (e.position))
            onAttachment.setValueAsCompleteGesture (unitOn ? 0.0f : 1.0f);
        select (unit);
    }

private:
    juce::Rectangle<float> dotBounds() const { return juce::Rectangle<float> (26.0f, 26.0f).withPosition (8.0f, 6.0f); }

    int unit;
    std::function<void (int)> select;
    ui::Toggle muteBtn, soloBtn;
    ui::PeakMeter meter;
    juce::ParameterAttachment onAttachment, muteWatch, freezeWatch;
    bool unitOn = false, muted = false, frozen = false, selected = false;
};
