#pragma once
#include "UiControls.h"
#include "Widgets.h"

// Bedienfeld einer Einheit in Karten: Seite 0 = Echo, Seite 1 = Modulation.
class UnitPanel : public juce::Component
{
public:
    UnitPanel (QuadEchoProcessor& proc, int unitIndex)
        : unit (unitIndex), echoDisplay (proc, unitIndex), modDisplay (proc, unitIndex),
          filterGraph (proc.apvts, { qe::paramId (unitIndex, qe::pHpOn), qe::paramId (unitIndex, qe::pLpOn), qe::paramId (unitIndex, qe::pHp),
                                     qe::paramId (unitIndex, qe::pLp), qe::paramId (unitIndex, qe::pHpRes), qe::paramId (unitIndex, qe::pRes),
                                     qe::paramId (unitIndex, qe::pHpBoost), qe::paramId (unitIndex, qe::pLpBoost) },
                       ui::unitColour (unitIndex))
    {
        using namespace qe;
        auto& apvts = proc.apvts;
        const auto col = ui::unitColour (unit);
        auto id = [this] (int p) { return paramId (unit, p); };

        auto colFor = [col] (int p)
        {
            return (p == pRate || p == pLfoDiv || p == pPhase || p == pFPhase || p == pLfoOn || p == pDDepth || p == pFDepth || p == pEnv || p == pLfoSync || p == pX4)
                       ? ui::kModColour : col;
        };
        auto addKnob = [&] (int p, const juce::String& title, std::function<juce::String (double)> fmt,
                            std::function<double (const juce::String&)> parse = ui::parseNum)
        {
            knobs[(size_t) p] = std::make_unique<ui::Knob> (apvts, id (p), title, colFor (p), fmt, parse);
            knobs[(size_t) p]->slider.setTooltip (ui::tooltipForUnitParam (p));
            addChildComponent (*knobs[(size_t) p]);
        };
        auto addToggle = [&] (int p, const juce::String& text, bool dry = false)
        {
            toggles[(size_t) p] = std::make_unique<ui::Toggle> (apvts, id (p), text, colFor (p), dry);
            toggles[(size_t) p]->setTooltip (ui::tooltipForUnitParam (p));
            addChildComponent (*toggles[(size_t) p]);
        };
        auto addChoice = [&] (int p, std::unique_ptr<ui::IconChoice> c)
        {
            choices[(size_t) p] = std::move (c);
            addChildComponent (*choices[(size_t) p]);
        };

        // ---- Zeit (wie bei Ableton Echo): je Seite Sync-Modus als kleine Symbole, darunter die Zeit (L/R bzw. M/S
        // steht im Wert), darunter das Offset-Feld ----
        state = &apvts;
        auto side = [this] (int s, std::function<juce::String (double)> fmt) { return [this, s, fmt] (double v) { return sideName (s) + " " + fmt (v); }; };
        auto parseDigits = [] (const juce::String& t) { return t.retainCharacters ("-.0123456789").getDoubleValue(); };
        addKnob (pTimeL, "Time L", side (0, ui::fmtMs), parseDigits);
        addKnob (pTimeR, "Time R", side (1, ui::fmtMs), parseDigits);
        auto fmtSix = [] (double v) { return juce::String ((int) std::lround (v)); };   // Anzahl 16tel
        auto parseSix = [] (const juce::String& t) { return t.upToFirstOccurrenceOf ("/", false, false).retainCharacters ("0123456789").getDoubleValue(); };
        addKnob (pSixL, "Time L", side (0, fmtSix), parseSix);
        addKnob (pSixR, "Time R", side (1, fmtSix), parseSix);
        for (int p : { pTimeL, pTimeR, pSixL, pSixR })
            knobs[(size_t) p]->label.setVisible (false);
        for (int c = 0; c < 2; ++c)
        {
            divKnobs[(size_t) c] = std::make_unique<ui::NoteDivKnob> (apvts, id (c == 0 ? pDivL : pDivR), id (c == 0 ? pSyncMode : pSyncModeR),
                                                                      [this, c] { return sideName (c); }, col);
            divKnobs[(size_t) c]->slider.setTooltip (ui::tooltipForUnitParam (pDivL));
            addChildComponent (*divKnobs[(size_t) c]);
        }
        addChoice (pSyncMode, ui::makeSyncModeChoice (apvts, id (pSyncMode), col));
        addChoice (pSyncModeR, ui::makeSyncModeChoice (apvts, id (pSyncModeR), col));


        // Offset je Kanal als kleine Pille unter dem Zeitregler (wie bei Ableton Echo): nur der Wert, +-50 %;
        // bei 0 gedimmt, sonst pastelliges Tuerkisblau
        for (int c = 0; c < 2; ++c)
        {
            auto& box = offsetBox[(size_t) c];
            box.getProperties().set ("numberbox", true);
            box.setColour (juce::Slider::textBoxTextColourId, ui::kDim);   // bei 0 gedimmt (valueColour)
            box.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            box.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            box.valueColour = [] (double v) { return std::abs (v) < 0.05 ? ui::kDim : ui::kOffsetColour; };
            offsetAttach[(size_t) c] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id (c == 0 ? pOffsetL : pOffset), box);
            box.textFromValueFunction = [] (double v) { return juce::String (v >= -0.04 ? "+" : "") + juce::String (v, std::abs (v - std::round (v)) < 0.05 ? 0 : 1) + "%"; };
            box.valueFromTextFunction = [] (const juce::String& t) { return t.retainCharacters ("-+.0123456789").getDoubleValue(); };
            box.setDoubleClickReturnValue (true, 0.0);
            box.setTooltip ("Offset of this side's time in % (-50 to +50, swing). Drag (Shift = fine), click to type, double-click: 0.");
            box.updateText();
            addChildComponent (box);
        }
        addToggle (pSync, "SYNC");
        linkToggle = std::make_unique<ui::LinkToggle> (apvts, id (pLink), col);
        linkToggle->setTooltip (ui::tooltipForUnitParam (pLink));
        addChildComponent (*linkToggle);

        // ---- Feedback ----
        addKnob (pFb, "Feedback", ui::fmtPct, ui::parsePct);
        addChoice (pMode, ui::makeChannelModeChoice (apvts, id (pMode), col));
        addChoice (pTrans, ui::makeTransitionChoice (apvts, id (pTrans), col));
        addToggle (pFbInv, juce::String::fromUTF8 ("\xc3\x98 INVERT"));
        addToggle (pFreeze, "FREEZE");
        addToggle (pFbStable, "STABLE");

        // ---- Feedback-Filter als EQ-Kurve ----
        addChildComponent (filterGraph);

        // ---- Pegel ----
        addKnob (pInGain, "Input", ui::fmtDb);
        addKnob (pMix, "Echo level", ui::fmtPct, ui::parsePct);
        addKnob (pOut, "Output", ui::fmtDb);
        addKnob (pWidth, "Width", ui::fmtPct, ui::parsePct);
        addToggle (pInDry, "GAIN ON DRY", true);
        addToggle (pPosDry, "PLACE DRY", true);
        addToggle (pOn, "UNIT ON");

        // ---- Modulation ----
        addChoice (pWave, ui::makeWaveChoice (apvts, id (pWave), ui::kModColour));
        addToggle (pLfoOn, "LFO ON");
        addToggle (pLfoSync, "LFO SYNC");
        addKnob (pRate, "Rate", ui::fmtHz);
        addKnob (pLfoDiv, "Rate (sync)", ui::fmtDiv, ui::parseDiv);
        addKnob (pPhase, juce::String::fromUTF8 ("\xcf\x86 Phase R"), ui::fmtDeg);
        addKnob (pFPhase, juce::String::fromUTF8 ("\xcf\x86 Filter"), ui::fmtDeg);
        addToggle (pX4, "x4");
        addKnob (pDDepth, "Delay depth", ui::fmtPct, ui::parsePct);
        addKnob (pFDepth, "Filter depth", ui::fmtPct, ui::parsePct);
        addKnob (pEnv, "Envelope mix", ui::fmtPct, ui::parsePct);

        addChildComponent (echoDisplay);
        addChildComponent (modDisplay);

        for (int p : { pSync, pSyncMode, pSyncModeR, pLink, pMode, pLfoSync, pLfoOn })
        {
            watchers.push_back (std::make_unique<juce::ParameterAttachment> (
                *apvts.getParameter (id (p)), [this] (float) { updateVisibility(); }));
            watchers.back()->sendInitialUpdate();
        }
        updateVisibility();
    }

    void setPage (int p) { page = p; updateVisibility(); resized(); repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto col = ui::unitColour (unit);
        auto b = getLocalBounds().toFloat();
        ui::drawPanel (g, b, 10.0f);
        g.setColour (col.withAlpha (0.8f));
        g.fillRoundedRectangle (b.withHeight (3.0f), 1.5f);

        const auto cardsOn = [&] (const juce::String& t, juce::Rectangle<int> r, juce::Colour tint) { ui::drawCard (g, r, t, tint); };
        if (page == 0)   // Echo-Seite: neutral, die Einheitsfarbe ist der einzige Akzent
        {
            cardsOn ("TIME", cardTime(), col);
            cardsOn ("FEEDBACK", cardFeedback(), col);
            cardsOn ("ECHO PREVIEW", cardPreview(), col);
            cardsOn ("FEEDBACK FILTER", cardFilter(), col);
            cardsOn ("LEVEL", cardLevel(), col);
        }
        else
        {
            cardsOn ("LFO", cardLfo(), ui::kModColour);
            cardsOn ("DEPTH AND ENVELOPE", cardDepth(), ui::kModColour);
        }
    }

    void resized() override
    {
        using namespace qe;
        auto at = [] (juce::Component* c, juce::Rectangle<int> r) { if (c != nullptr) c->setBounds (r); };
        auto kb = [] (juce::Rectangle<int> card, int slot) { return juce::Rectangle<int> (card.getX() + 8 + slot * 100, card.getY() + 18, 100, 112); };

        // ---- Echo-Seite ----
        // Zeit: oben der Sync-Modus, darunter die Zeiten, darunter je ein Offset-Feld
        const auto t = cardTime();
        at (choices[pSyncMode].get(), juce::Rectangle<int> (t.getX() + 12, t.getY() + 21, 92, 24));
        at (choices[pSyncModeR].get(), juce::Rectangle<int> (t.getX() + 112, t.getY() + 21, 92, 24));
        auto tk = [&t] (int slot) { return juce::Rectangle<int> (t.getX() + 8 + slot * 100, t.getY() + 47, 100, 93); };
        at (knobs[pTimeL].get(), tk (0));   at (knobs[pSixL].get(), tk (0));   at (divKnobs[0].get(), tk (0));
        at (knobs[pTimeR].get(), tk (1));   at (knobs[pSixR].get(), tk (1));   at (divKnobs[1].get(), tk (1));
        for (int c = 0; c < 2; ++c)
            offsetBox[(size_t) c].setBounds (t.getX() + 30 + c * 100, t.getY() + 142, 56, 17);
        at (toggles[pSync].get(), juce::Rectangle<int> (t.getX() + 216, t.getY() + 18, 96, 30));
        at (linkToggle.get(), juce::Rectangle<int> (t.getX() + 216, t.getY() + 54, 96, 30));

        const auto f = cardFeedback();
        at (knobs[pFb].get(), juce::Rectangle<int> (f.getX() + 8, f.getY() + 30, 100, 112));
        at (choices[pMode].get(), juce::Rectangle<int> (f.getX() + 112, f.getY() + 18, 180, 48));
        at (choices[pTrans].get(), juce::Rectangle<int> (f.getX() + 112, f.getY() + 70, 180, 48));
        at (toggles[pFbInv].get(), juce::Rectangle<int> (f.getX() + 112, f.getY() + 126, 88, 30));
        at (toggles[pFreeze].get(), juce::Rectangle<int> (f.getX() + 204, f.getY() + 126, 88, 30));

        echoDisplay.setBounds (cardPreview().withTrimmedTop (4));
        filterGraph.setBounds (cardFilter().reduced (8, 0).withTrimmedTop (20).withTrimmedBottom (8));
        at (toggles[pFbStable].get(), juce::Rectangle<int> (cardFilter().getRight() - 196, cardFilter().getY() + 3, 70, 16));

        const auto lv = cardLevel();
        at (knobs[pInGain].get(), kb (lv, 0));
        at (knobs[pMix].get(), kb (lv, 1));
        at (knobs[pOut].get(), kb (lv, 2));
        at (knobs[pWidth].get(), kb (lv, 3));
        auto lvToggle = [&lv] (int row) { return juce::Rectangle<int> (lv.getX() + 408, lv.getY() + 20 + row * 37, 106, 31); };
        at (toggles[pInDry].get(), lvToggle (0));
        at (toggles[pPosDry].get(), lvToggle (1));
        at (toggles[pOn].get(), lvToggle (2));

        // ---- Modulations-Seite ----
        const auto lf = cardLfo();
        auto lfk = [&lf] (int x) { return juce::Rectangle<int> (lf.getX() + x, lf.getY() + 18, 92, 112); };
        at (choices[pWave].get(), juce::Rectangle<int> (lf.getX() + 10, lf.getY() + 20, 136, 50));
        at (toggles[pLfoOn].get(), juce::Rectangle<int> (lf.getX() + 12, lf.getY() + 74, 132, 26));
        at (toggles[pLfoSync].get(), juce::Rectangle<int> (lf.getX() + 12, lf.getY() + 104, 132, 26));
        at (knobs[pRate].get(), lfk (154));  at (knobs[pLfoDiv].get(), lfk (154));
        at (knobs[pPhase].get(), lfk (246));
        at (knobs[pFPhase].get(), lfk (338));
        at (toggles[pX4].get(), juce::Rectangle<int> (lf.getX() + 436, lf.getY() + 44, 76, 32));

        const auto dp = cardDepth();
        at (knobs[pDDepth].get(), kb (dp, 0));
        at (knobs[pFDepth].get(), kb (dp, 1));
        at (knobs[pEnv].get(), kb (dp, 2));

        modDisplay.setBounds (cardLive());
    }

private:
    juce::Rectangle<int> cardTime() const     { return { 10, 12, 320, 166 }; }
    juce::Rectangle<int> cardFeedback() const { return { 338, 12, 300, 166 }; }
    juce::Rectangle<int> cardPreview() const  { return { 646, 12, 304, 166 }; }
    juce::Rectangle<int> cardFilter() const   { return { 10, 186, 412, 136 }; }
    juce::Rectangle<int> cardLevel() const    { return { 430, 186, 520, 136 }; }
    juce::Rectangle<int> cardLfo() const      { return { 10, 12, 520, 136 }; }
    juce::Rectangle<int> cardDepth() const    { return { 538, 12, 412, 136 }; }
    juce::Rectangle<int> cardLive() const     { return { 10, 156, 940, 166 }; }

    void updateVisibility()
    {
        using namespace qe;
        auto cur = [this] (int p) { return ui::currentValue (*state, qe::paramId (unit, p)); };   // aktuell, siehe currentValue
        auto val = [&cur] (int p) { return cur (p) > 0.5f; };
        const bool echo = page == 0;
        const bool sync = val (pSync), split = ! val (pLink), lfoSync = val (pLfoSync), lfoOn = val (pLfoOn);
        const bool six[2] = { (int) (cur (pSyncMode) + 0.5f) == Sync16th, (int) (cur (pSyncModeR) + 0.5f) == Sync16th };

        for (int p : { pTimeL, pTimeR }) knobs[(size_t) p]->setVisible (echo && ! sync);
        for (int c = 0; c < 2; ++c)
        {
            knobs[(size_t) (c == 0 ? pSixL : pSixR)]->setVisible (echo && sync && six[c]);
            divKnobs[(size_t) c]->setVisible (echo && sync && ! six[c]);
        }
        for (int p : { pSyncMode, pSyncModeR })
        {
            choices[(size_t) p]->setVisible (echo);
            choices[(size_t) p]->setEnabled (sync && (p == pSyncMode || split));
            choices[(size_t) p]->setAlpha (sync && (p == pSyncMode || split) ? 1.0f : 0.45f);
        }
        // Beschriftung L/R bzw. M/S (Mid/Side) in den Zeitreglern
        for (int p : { pTimeL, pTimeR, pSixL, pSixR }) knobs[(size_t) p]->slider.updateText();
        for (auto& k : divKnobs) k->slider.updateText();
        for (auto& b : offsetBox) b.setVisible (echo);
        for (int p : { pFb, pInGain, pMix, pOut, pWidth }) knobs[(size_t) p]->setVisible (echo);
        for (int p : { pMode, pTrans }) choices[(size_t) p]->setVisible (echo);
        for (int p : { pSync, pFbInv, pFreeze, pFbStable, pInDry, pPosDry, pOn }) toggles[(size_t) p]->setVisible (echo);
        linkToggle->setVisible (echo);
        echoDisplay.setVisible (echo);
        filterGraph.setVisible (echo);

        // Single-Delay (LINK aus, Standard): die rechte Zeit folgt der linken (ausgegraut); LINK an gibt sie frei
        for (auto* c : std::initializer_list<juce::Component*> { knobs[pTimeR].get(), knobs[pSixR].get(), divKnobs[1].get() })
        {
            c->setEnabled (split);
            c->setAlpha (split ? 1.0f : 0.5f);
        }

        choices[pWave]->setVisible (! echo);
        toggles[pLfoOn]->setVisible (! echo);
        toggles[pLfoSync]->setVisible (! echo);
        knobs[pRate]->setVisible (! echo && ! lfoSync);
        knobs[pLfoDiv]->setVisible (! echo && lfoSync);
        knobs[pPhase]->setVisible (! echo);
        knobs[pFPhase]->setVisible (! echo);
        toggles[pX4]->setVisible (! echo);
        // LFO aus: die LFO-Regler werden abgedunkelt
        for (auto* c : std::initializer_list<juce::Component*> { choices[pWave].get(), toggles[pLfoSync].get(), knobs[pRate].get(), knobs[pLfoDiv].get(),
                                                                  knobs[pPhase].get(), knobs[pFPhase].get() })
        {
            c->setEnabled (lfoOn);
            c->setAlpha (lfoOn ? 1.0f : 0.5f);
        }
        for (int p : { pDDepth, pFDepth, pEnv }) knobs[(size_t) p]->setVisible (! echo);
        modDisplay.setVisible (! echo);
    }

    int unit = 0;
    int page = 0;
    std::array<std::unique_ptr<ui::Knob>, qe::pCount> knobs;
    std::array<std::unique_ptr<ui::Toggle>, qe::pCount> toggles;
    std::array<std::unique_ptr<ui::IconChoice>, qe::pCount> choices;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> watchers;
    std::unique_ptr<ui::LinkToggle> linkToggle;
    juce::AudioProcessorValueTreeState* state = nullptr;
    juce::String sideName (int s) const
    {
        const bool ms = state != nullptr && (int) (ui::currentValue (*state, qe::paramId (unit, qe::pMode)) + 0.5f) == qe::MidSide;
        return ms ? (s == 0 ? "M" : "S") : (s == 0 ? "L" : "R");
    }
    std::array<std::unique_ptr<ui::NoteDivKnob>, 2> divKnobs;
    std::array<ui::FineSlider, 2> offsetBox { { { juce::Slider::LinearBar, juce::Slider::TextBoxLeft }, { juce::Slider::LinearBar, juce::Slider::TextBoxLeft } } };
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 2> offsetAttach;
    EchoDisplay echoDisplay;
    ModDisplay modDisplay;
    ui::FilterGraph filterGraph;
};
