#pragma once
#include "UiControls.h"
#include "Widgets.h"

// Live-Anzeige der Ducking-Absenkung in dB (liest lockfrei aus der Engine)
class DuckMeter : public juce::Component, private juce::Timer
{
public:
    explicit DuckMeter (QuadEchoProcessor& p) : proc (p)
    {
        setInterceptsMouseClicks (false, false);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        constexpr float range = 24.0f;   // Skala 0 .. -24 dB
        auto b = getLocalBounds().toFloat();
        g.setFont (ui::uiFont (10.5f, true));
        g.setColour (ui::kDim);
        auto head = b.removeFromTop (16.0f);
        g.drawText ("ECHO REDUCTION", head, juce::Justification::centredLeft);
        g.setColour (shown > 0.05f ? ui::kText : ui::kDim);
        g.drawText ((shown > 0.05f ? "-" : "") + juce::String (shown, 1) + " dB", head, juce::Justification::centredRight);

        auto bar = b.removeFromTop (12.0f);
        g.setColour (ui::kBg);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (ui::kAccent);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * std::min (shown / range, 1.0f)), 3.0f);
        g.setColour (ui::kLine);
        g.drawRoundedRectangle (bar, 3.0f, 1.0f);

        g.setFont (ui::uiFont (10.0f));
        g.setColour (ui::kDim);
        for (float db : { 0.0f, 6.0f, 12.0f, 18.0f, 24.0f })
        {
            const float x = bar.getX() + bar.getWidth() * db / range;
            g.drawVerticalLine ((int) x, bar.getBottom(), bar.getBottom() + 3.0f);
            const bool first = db == 0.0f, last = db == range;   // Randbeschriftung nach innen ausrichten
            const float lx = first ? x : (last ? x - 30.0f : x - 15.0f);
            g.drawText (first ? juce::String ("0") : "-" + juce::String ((int) db), juce::Rectangle<float> (lx, bar.getBottom() + 3.0f, 30.0f, 12.0f),
                        first ? juce::Justification::centredLeft : (last ? juce::Justification::centredRight : juce::Justification::centred));
        }
    }

private:
    void timerCallback() override
    {
        const float r = proc.getEngine().takeDuckReduction();
        const float next = r > shown ? r : shown + (r - shown) * 0.3f;   // steigt sofort, faellt weich
        if (std::abs (next - shown) > 0.01f) { shown = next; repaint(); }
    }
    QuadEchoProcessor& proc;
    float shown = 0.0f;
};

// Expert-Bereich (klappt unten aus): gleicher Stil wie das Einheiten-Panel, Blau als Kennfarbe.
// Seiten: Globaler Filter, Noise und Ducking. Kompakt gehalten (kleine Regler, ein bis zwei Reihen).
// Filter als EQ-Kurven, LFO-Formen als Symbole.
class ExpertPanel : public juce::Component
{
public:
    static constexpr int kHeight = 264;       // Noise-Seite (hoechste)
    static constexpr int kFilterHeight = 180; // Filter- und Ducking-Seite
    static constexpr int kPages = 3;
    std::function<void()> onHeightChanged;
    int preferredHeight() const { return page == 1 ? kHeight : kFilterHeight; }

    explicit ExpertPanel (QuadEchoProcessor& p)
        : proc (p), duckMeter (p),
          gfGraph (p.apvts, { qe::kGlobalIds[qe::gfHpOn], qe::kGlobalIds[qe::gfLpOn], qe::kGlobalIds[qe::gfHp], qe::kGlobalIds[qe::gfLp],
                              qe::kGlobalIds[qe::gfHpRes], qe::kGlobalIds[qe::gfRes], qe::kGlobalIds[qe::gfHpBoost], qe::kGlobalIds[qe::gfLpBoost] },
                   juce::Colour (0xffbb9cd6)),
          nzGraph (p.apvts, { qe::kGlobalIds[qe::nzHpOn], qe::kGlobalIds[qe::nzLpOn], qe::kGlobalIds[qe::nzHp], qe::kGlobalIds[qe::nzLp],
                              qe::kGlobalIds[qe::nzHpRes], qe::kGlobalIds[qe::nzRes], qe::kGlobalIds[qe::nzHpBoost], qe::kGlobalIds[qe::nzLpBoost] },
                   juce::Colour (0xffbb9cd6))
    {
        using namespace qe;
        auto& apvts = proc.apvts;
        const juce::Colour tFilter (0xffbb9cd6);

        auto K = [&] (int g, const juce::String& title, std::function<juce::String (double)> fmt,
                      std::function<double (const juce::String&)> parse, juce::Colour col)
        {
            knobs[(size_t) g] = std::make_unique<ui::Knob> (apvts, kGlobalIds[g], title, col, fmt, parse);
            knobs[(size_t) g]->slider.setTooltip (ui::tooltipForExpert (kGlobalIds[g]));
            knobs[(size_t) g]->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 15);
            addChildComponent (*knobs[(size_t) g]);
        };
        auto T = [&] (int g, const juce::String& text, juce::Colour col, bool dry = false, bool noiseIcon = false)
        {
            toggles[(size_t) g] = std::make_unique<ui::Toggle> (apvts, kGlobalIds[g], text, col, dry, noiseIcon);
            toggles[(size_t) g]->setTooltip (ui::tooltipForExpert (kGlobalIds[g]));
            addChildComponent (*toggles[(size_t) g]);
        };
        auto W = [&] (int g)
        {
            waves[(size_t) g] = ui::makeWaveChoice (apvts, kGlobalIds[g], ui::kModColour);
            addChildComponent (*waves[(size_t) g]);
        };
        const juce::String phi = juce::String::fromUTF8 ("\xcf\x86");
        const auto mod = ui::kModColour;

        // Seite 0: globaler Filter
        addChildComponent (gfGraph);
        T (gfInclDry, "INCLUDE DRY", tFilter, true);
        T (gfSkipFirst, "SKIP 1ST ECHO", tFilter);
        T (gfLfoOn, "LFO ON", mod);
        W (gfWave);
        T (gfSync, "SYNC", mod);
        K (gfRate, "Rate", ui::fmtHz, ui::parseNum, mod);
        K (gfDiv, "Rate (sync)", ui::fmtDiv, ui::parseDiv, mod);
        K (gfDepth, "Depth", ui::fmtPct, ui::parsePct, mod);
        K (gfPhase, phi + " Phase R", ui::fmtDeg, ui::parseNum, mod);

        // Seite 1: Noise
        const auto nc = ui::kNoiseColour;
        T (nzOn, "NOISE ON", nc, false, true);
        K (nzLevel, "Level", ui::fmtPct, ui::parsePct, nc);
        K (nzColor, "Color", [] (double v) { return v < 0.17 ? juce::String ("brown") : v < 0.4 ? juce::String ("dark pink") : v < 0.6 ? juce::String ("pink") : v < 0.85 ? juce::String ("light") : juce::String ("white"); },
          [] (const juce::String& t) { return t.getDoubleValue(); }, nc);
        K (nzCrackle, "Crackle", ui::fmtPct, ui::parsePct, nc);
        K (nzWidth, "Width", ui::fmtPct, ui::parsePct, nc);
        K (nzInto, "Into echoes", ui::fmtPct, ui::parsePct, nc);
        addChildComponent (nzGraph);
        T (nzLfoOn, "LFO ON", mod);
        W (nzWave);
        T (nzSync, "SYNC", mod);
        K (nzRate, "Rate", ui::fmtHz, ui::parseNum, mod);
        K (nzDiv, "Rate (sync)", ui::fmtDiv, ui::parseDiv, mod);
        K (nzDepthF, "Filter depth", ui::fmtPct, ui::parsePct, mod);
        K (nzDepthL, "Level depth", ui::fmtPct, ui::parsePct, mod);
        K (nzPhase, phi + " Level", ui::fmtDeg, ui::parseNum, mod);
        K (nzEnv, "Envelope", ui::fmtPct, ui::parsePct, mod);

        // Seite 2: Ducking
        const auto dc = ui::kAccent;
        T (dkOn, "DUCKING ON", dc);
        K (dkThreshold, "Threshold", ui::fmtDb, ui::parseNum, dc);
        K (dkAmount, "Amount", [] (double v) { return "-" + juce::String (v, 1) + " dB"; },
          [] (const juce::String& t) { return std::abs (t.getDoubleValue()); }, dc);
        K (dkAttack, "Attack", ui::fmtMs, ui::parseNum, dc);
        K (dkRelease, "Release", ui::fmtMs, ui::parseNum, dc);
        addChildComponent (duckMeter);

        static const char* pageNames[kPages] = { "GLOBAL FILTER", "NOISE", "DUCKING" };
        for (int i = 0; i < kPages; ++i)
        {
            auto& b = tabs[(size_t) i];
            b.setButtonText (pageNames[i]);
            b.setClickingTogglesState (true);
            b.setRadioGroupId (303);
            b.setColour (juce::TextButton::buttonOnColourId, kExpertColour);
            b.onClick = [this, i] { setPage (i); };
            addAndMakeVisible (b);
        }
        page = juce::jlimit (0, kPages - 1, (int) apvts.state.getProperty ("uiExpertPage", 0));
        tabs[(size_t) page].setToggleState (true, juce::dontSendNotification);

        for (int g : { gfSync, gfLfoOn, nzSync, nzLfoOn, dkOn })
        {
            watchers.push_back (std::make_unique<juce::ParameterAttachment> (
                *apvts.getParameter (kGlobalIds[g]), [this] (float) { updateVisibility(); }));
            watchers.back()->sendInitialUpdate();
        }
        updateVisibility();
    }

    void setPage (int p)
    {
        page = p;
        proc.apvts.state.setProperty ("uiExpertPage", p, nullptr);
        updateVisibility(); resized(); repaint();
        if (onHeightChanged) onHeightChanged();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        // gleicher Grund wie das Einheiten-Panel; Blau (wie der EXPERT-Knopf) nur als Kennfarbe oben und im Titel
        ui::drawPanel (g, b, 10.0f);
        g.setColour (kExpertColour.withAlpha (0.8f));
        g.fillRoundedRectangle (b.withHeight (3.0f), 1.5f);

        ui::drawGearIcon (g, { 22.0f, 16.0f }, kExpertColour);
        g.setFont (ui::uiFont (11.0f, true));
        g.setColour (ui::kDim);
        g.drawText ("EXPERT", 36, 8, 64, 16, juce::Justification::centredLeft);

        const juce::Colour tFilter (0xffbb9cd6);
        auto card = [&] (const juce::String& t, juce::Rectangle<int> r, juce::Colour tint) { ui::drawCard (g, r, t, tint); };
        if (page == 0)
        {
            card ("FILTER (ON ALL ECHOES)", cardA(), tFilter);
            card ("MODULATION", cardB(), ui::kModColour);
            g.setColour (ui::kDim);
            g.setFont (ui::uiFont (11.0f));
            g.drawText ("Sits behind all units and filters every echo once, including the first. The original stays untouched unless INCLUDE DRY is on.",
                        juce::Rectangle<int> (20, kRowY + kCardH + 8, 920, 32), juce::Justification::topLeft);
        }
        else if (page == 1)
        {
            card ("NOISE SOURCE", sourceCard(), ui::kNoiseColour);
            card ("NOISE FILTER", filterCard(), tFilter);
            card ("NOISE MODULATION", modCard(), ui::kModColour);
        }
        else
        {
            card ("DUCKING", duckCard(), ui::kAccent);
            g.setColour (ui::kDim);
            g.setFont (ui::uiFont (11.0f));
            g.drawText ("The echoes dip while the input is above the threshold and come back in the pauses (vocals, solos). The original and the noise stay untouched.",
                        juce::Rectangle<int> (20, kRowY + kCardH + 8, 920, 32), juce::Justification::topLeft);
        }
    }

    void resized() override
    {
        using namespace qe;
        auto at = [] (juce::Component* c, juce::Rectangle<int> r) { if (c != nullptr) c->setBounds (r); };
        for (int i = 0; i < kPages; ++i)
            tabs[(size_t) i].setBounds (100 + i * 118, 5, 112, 22);

        // kleine Regler: 80 breit, 84 hoch
        auto kb = [] (juce::Rectangle<int> card, int x, int w = 80) { return juce::Rectangle<int> (card.getX() + x, card.getY() + 16, w, 86); };
        auto tg = [] (juce::Rectangle<int> card, int x, int row, int w = 100) { return juce::Rectangle<int> (card.getX() + x, card.getY() + 20 + row * 34, w, 28); };
        // LFO-Block: Formen als Symbole (zwei Reihen), darunter LFO an und Sync
        auto lfoBlock = [&] (juce::Rectangle<int> card, int wave, int lfoOn, int sync, int width)
        {
            at (waves[(size_t) wave].get(), juce::Rectangle<int> (card.getX() + 8, card.getY() + 18, width, 44));
            const int half = (width - 4) / 2;
            at (toggles[(size_t) lfoOn].get(), juce::Rectangle<int> (card.getX() + 6, card.getY() + 66, half + 4, 28));
            at (toggles[(size_t) sync].get(), juce::Rectangle<int> (card.getX() + 10 + half, card.getY() + 66, half + 2, 28));
        };

        // ---- globaler Filter ----
        const auto a = cardA(), b = cardB();
        gfGraph.setBounds (a.getX() + 8, a.getY() + 18, 356, 82);
        at (toggles[gfInclDry].get(), tg (a, 372, 0, 106));
        at (toggles[gfSkipFirst].get(), tg (a, 372, 1, 106));
        lfoBlock (b, gfWave, gfLfoOn, gfSync, 126);
        at (knobs[gfRate].get(), kb (b, 142));  at (knobs[gfDiv].get(), kb (b, 142));
        at (knobs[gfDepth].get(), kb (b, 226));
        at (knobs[gfPhase].get(), kb (b, 310));

        // ---- Noise ----
        const auto s = sourceCard();
        at (toggles[nzOn].get(), juce::Rectangle<int> (s.getX() + 12, s.getY() + 36, 124, 34));
        const int order[5] = { nzLevel, nzColor, nzCrackle, nzWidth, nzInto };
        for (int i = 0; i < 5; ++i)
            at (knobs[(size_t) order[i]].get(), kb (s, 160 + i * 110, 84));

        const auto f = filterCard(), m = modCard();
        nzGraph.setBounds (f.getX() + 8, f.getY() + 18, f.getWidth() - 16, 82);
        lfoBlock (m, nzWave, nzLfoOn, nzSync, 116);
        at (knobs[nzRate].get(), kb (m, 130, 78));  at (knobs[nzDiv].get(), kb (m, 130, 78));
        at (knobs[nzDepthF].get(), kb (m, 208, 78));
        at (knobs[nzDepthL].get(), kb (m, 286, 78));
        at (knobs[nzPhase].get(), kb (m, 364, 78));
        at (knobs[nzEnv].get(), kb (m, 442, 78));

        // ---- Ducking ----
        const auto d = duckCard();
        at (toggles[dkOn].get(), juce::Rectangle<int> (d.getX() + 12, d.getY() + 36, 136, 34));
        const int duckOrder[4] = { dkThreshold, dkAmount, dkAttack, dkRelease };
        for (int i = 0; i < 4; ++i)
            at (knobs[(size_t) duckOrder[i]].get(), kb (d, 170 + i * 100, 84));
        duckMeter.setBounds (d.getX() + 600, d.getY() + 34, 310, 46);
    }

private:
    static constexpr int kRowY = 34, kCardH = 106;
    inline static const juce::Colour kExpertColour { 0xff5f90c4 };   // Kennfarbe (wie der EXPERT-Knopf)

    juce::Rectangle<int> cardA() const { return { 12, kRowY, 482, kCardH }; }
    juce::Rectangle<int> cardB() const { return { 502, kRowY, 446, kCardH }; }
    juce::Rectangle<int> sourceCard() const { return { 12, kRowY, 936, kCardH }; }
    juce::Rectangle<int> filterCard() const { return { 12, kRowY + kCardH + 6, 392, kCardH }; }
    juce::Rectangle<int> modCard() const { return { 412, kRowY + kCardH + 6, 536, kCardH }; }
    juce::Rectangle<int> duckCard() const { return { 12, kRowY, 936, kCardH }; }

    void updateVisibility()
    {
        using namespace qe;
        auto val = [this] (int g) { return ui::currentValue (proc.apvts, qe::kGlobalIds[g]) > 0.5f; };   // aktuell, siehe currentValue
        auto show = [] (juce::Component* c, bool v) { if (c != nullptr) c->setVisible (v); };
        auto dim = [] (juce::Component* c, bool on) { if (c != nullptr) { c->setEnabled (on); c->setAlpha (on ? 1.0f : 0.5f); } };

        const bool p0 = page == 0, p1 = page == 1, p2 = page == 2;
        gfGraph.setVisible (p0);
        for (int g : { gfDepth, gfPhase }) show (knobs[(size_t) g].get(), p0);
        for (int g : { gfInclDry, gfSkipFirst, gfLfoOn, gfSync }) show (toggles[(size_t) g].get(), p0);
        show (waves[gfWave].get(), p0);
        show (knobs[gfRate].get(), p0 && ! val (gfSync));
        show (knobs[gfDiv].get(), p0 && val (gfSync));

        nzGraph.setVisible (p1);
        for (int g : { nzLevel, nzColor, nzCrackle, nzWidth, nzInto, nzDepthF, nzDepthL, nzPhase, nzEnv }) show (knobs[(size_t) g].get(), p1);
        for (int g : { nzOn, nzLfoOn, nzSync }) show (toggles[(size_t) g].get(), p1);
        show (waves[nzWave].get(), p1);
        show (knobs[nzRate].get(), p1 && ! val (nzSync));
        show (knobs[nzDiv].get(), p1 && val (nzSync));

        for (auto* c : std::initializer_list<juce::Component*> { waves[gfWave].get(), toggles[gfSync].get(), knobs[gfRate].get(), knobs[gfDiv].get(), knobs[gfDepth].get(), knobs[gfPhase].get() })
            dim (c, val (gfLfoOn));
        for (auto* c : std::initializer_list<juce::Component*> { waves[nzWave].get(), toggles[nzSync].get(), knobs[nzRate].get(), knobs[nzDiv].get(), knobs[nzPhase].get() })
            dim (c, val (nzLfoOn));

        show (toggles[dkOn].get(), p2);
        for (int g : { dkThreshold, dkAmount, dkAttack, dkRelease })
        {
            show (knobs[(size_t) g].get(), p2);
            dim (knobs[(size_t) g].get(), val (dkOn));
        }
        duckMeter.setVisible (p2);
    }

    QuadEchoProcessor& proc;
    int page = 0;
    std::array<std::unique_ptr<ui::Knob>, qe::gCount> knobs;
    std::array<std::unique_ptr<ui::Toggle>, qe::gCount> toggles;
    std::array<std::unique_ptr<ui::IconChoice>, qe::gCount> waves;
    std::array<juce::TextButton, kPages> tabs;
    DuckMeter duckMeter;
    ui::FilterGraph gfGraph, nzGraph;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> watchers;
};
