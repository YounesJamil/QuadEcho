#pragma once
#include "UiControls.h"
#include "Widgets.h"

// Bedienfeld einer Einheit in Karten: Seite 0 = Echo, Seite 1 = Modulation.
class UnitPanel : public juce::Component
{
public:
    UnitPanel (QuadEchoProcessor& proc, int unitIndex)
        : unit (unitIndex), echoDisplay (proc, unitIndex), modDisplay (proc, unitIndex), stepEditor (proc.apvts, unitIndex, ui::kModColour),
          filterGraph (proc.apvts, { qe::paramId (unitIndex, qe::pHpOn), qe::paramId (unitIndex, qe::pLpOn), qe::paramId (unitIndex, qe::pHp),
                                     qe::paramId (unitIndex, qe::pLp), qe::paramId (unitIndex, qe::pHpRes), qe::paramId (unitIndex, qe::pRes),
                                     qe::paramId (unitIndex, qe::pHpBoost), qe::paramId (unitIndex, qe::pLpBoost),
                                     qe::paramId (unitIndex, qe::pBellOn), qe::paramId (unitIndex, qe::pBellHz), qe::paramId (unitIndex, qe::pBellGain),
                                     qe::paramId (unitIndex, qe::pBellQ), qe::paramId (unitIndex, qe::pBellMod) },
                       ui::unitColour (unitIndex))
    {
        using namespace qe;
        auto& apvts = proc.apvts;
        const auto col = ui::unitColour (unit);
        auto id = [this] (int p) { return paramId (unit, p); };

        auto colFor = [col] (int p)
        {
            return (p == pRate || p == pLfoDiv || p == pPhase || p == pFPhase || p == pLfoOn || p == pDDepth || p == pFDepth || p == pEnv || p == pLfoSync || p == pLfoAmt || p == pEnvAmt || p == pGlide || p == pSeed)
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
        addToggle (pTape, "TAPE");

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
        // Faktoren 0,1-10 auf LFO- bzw. Huellkurven-Anteil (Delay und Filter)
        auto fmtFactor = [] (double v) { return juce::String::fromUTF8 ("\xc3\x97") + juce::String (v, v < 1.0 ? 2 : 1); };
        auto parseFactor = [] (const juce::String& t) { return t.retainCharacters (".0123456789").getDoubleValue(); };
        addKnob (pLfoAmt, juce::String::fromUTF8 ("LFO \xc3\x97"), fmtFactor, parseFactor);
        addKnob (pEnvAmt, juce::String::fromUTF8 ("ENV \xc3\x97"), fmtFactor, parseFactor);
        addKnob (pDDepth, "Delay depth", ui::fmtPct, ui::parsePct);
        addKnob (pFDepth, "Filter depth", ui::fmtPct, ui::parsePct);
        addKnob (pEnv, "Envelope mix", ui::fmtPct, ui::parsePct);

        // ---- Schritt-Muster (ersetzt die LFO-Welle, wenn nicht Off) ----
        {
            auto textChoice = [&] (int p, juce::StringArray labels, juce::StringArray tips)
            {
                std::vector<ui::IconChoice::Icon> list;
                for (const auto& t : labels)
                    list.push_back ([t] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
                                    { g.setColour (c); g.setFont (ui::uiFont (11.0f, true)); g.drawText (t, r, juce::Justification::centred); });
                addChoice (p, std::make_unique<ui::IconChoice> (apvts, id (p), std::move (list), tips, ui::kModColour));
            };
            textChoice (pPattern, { "OFF", "STAIRS", "ZIGZAG", "SYNCOPATED", "RANDOM" },
                        { "Off: the LFO wave is used", "Stairs: steps rising over one LFO cycle", "Zigzag: alternating up and down, growing",
                          "Syncopated: 3-3-2 accents", "Random: a fixed random pattern per seed (NEW SEED for another one)" });
            textChoice (pSteps, { "8", "16" }, { "8 steps per LFO cycle", "16 steps per LFO cycle" });
            toggles[pPatCustom] = std::make_unique<ui::Toggle> (apvts, id (pPatCustom), "DRAW", ui::kModColour);
            toggles[pPatCustom]->setTooltip ("DRAW: your own steps instead of the chosen pattern (drawn in the box below, each step is automatable).");
            addChildComponent (*toggles[pPatCustom]);
            auto pill = [&] (ui::FineSlider& b, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att, int p,
                             std::function<juce::String (double)> txt, std::function<double (const juce::String&)> parse, double def, const juce::String& tip)
            {
                b.getProperties().set ("numberbox", true);
                b.setColour (juce::Slider::textBoxTextColourId, ui::kText);
                b.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
                b.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
                att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id (p), b);
                b.textFromValueFunction = std::move (txt);
                b.valueFromTextFunction = std::move (parse);
                b.setDoubleClickReturnValue (true, def);
                b.setTooltip (tip);
                b.updateText();
                addChildComponent (b);
            };
            auto digits = [] (const juce::String& t) { return t.retainCharacters (".0123456789").getDoubleValue(); };
            pill (glideBox, glideAttach, pGlide, [] (double v) { return "Glide " + juce::String (v * 100.0, 0) + "%"; },
                  [digits] (const juce::String& t) { return digits (t) / 100.0; }, 0.0, "Glide: 0 = the delay time jumps from step to step, 100% = it slides over the whole step.");
            pill (wowBox, wowAttach, pWow, [] (double v) { return "Wow " + juce::String (v * 100.0, 0) + "%"; },
                  [digits] (const juce::String& t) { return digits (t) / 100.0; }, 0.0,
                  "Wow/Flutter: the tape transport wobbles (slow wow plus fast flutter), each repeat bends a bit more. Independent of the LFO. 0 = off.");
            pill (seedBox, seedAttach, pSeed, [] (double v) { return "Seed " + juce::String ((int) std::lround (v)); }, digits, 1.0, "Seed of the random pattern (drag or type).");
            seedBtn.setButtonText ("NEW SEED");
            seedBtn.setTooltip ("A new random pattern (sets PATTERN to RANDOM).");
            seedBtn.onClick = [this, &apvts]
            {
                auto setParam = [&apvts, this] (int p, float v)
                {
                    auto* prm = apvts.getParameter (qe::paramId (unit, p));
                    prm->beginChangeGesture();
                    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
                    prm->endChangeGesture();
                };
                setParam (qe::pSeed, (float) juce::Random::getSystemRandom().nextInt ({ 1, 1000 }));
                setParam (qe::pPattern, (float) qe::PatRandom);
            };
            addChildComponent (seedBtn);
        }

        addChildComponent (echoDisplay);
        addChildComponent (modDisplay);
        addChildComponent (stepEditor);

        for (int p : { pSync, pSyncMode, pSyncModeR, pLink, pMode, pLfoSync, pLfoOn, pPattern, pPatCustom, pHpOn, pLpOn, pBellOn, pEnv, pDDepth, pFDepth })
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
            cardsOn ("PATTERN", cardPattern(), ui::kModColour);
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
        at (toggles[pTape].get(), juce::Rectangle<int> (f.getRight() - 80, f.getY() + 3, 70, 16));
        wowBox.setBounds (f.getRight() - 160, f.getY() + 3, 74, 16);

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
        at (knobs[pLfoAmt].get(), lfk (426));

        const auto dp = cardDepth();
        at (knobs[pDDepth].get(), kb (dp, 0));
        at (knobs[pFDepth].get(), kb (dp, 1));
        at (knobs[pEnv].get(), kb (dp, 2));
        at (knobs[pEnvAmt].get(), kb (dp, 3));

        const auto pc = cardPattern();
        at (choices[pPattern].get(), juce::Rectangle<int> (pc.getX() + 80, pc.getY() + 7, 390, 26));
        at (toggles[pPatCustom].get(), juce::Rectangle<int> (pc.getX() + 476, pc.getY() + 7, 56, 26));
        at (choices[pSteps].get(), juce::Rectangle<int> (pc.getX() + 538, pc.getY() + 7, 64, 26));
        glideBox.setBounds (pc.getX() + 610, pc.getY() + 10, 96, 20);
        seedBox.setBounds (pc.getX() + 712, pc.getY() + 10, 84, 20);
        seedBtn.setBounds (pc.getX() + 804, pc.getY() + 7, 124, 26);
        modDisplay.setBounds (cardLive());
        stepEditor.setBounds (cardLive());
    }

private:
    juce::Rectangle<int> cardTime() const     { return { 10, 12, 320, 166 }; }
    juce::Rectangle<int> cardFeedback() const { return { 338, 12, 300, 166 }; }
    juce::Rectangle<int> cardPreview() const  { return { 646, 12, 304, 166 }; }
    juce::Rectangle<int> cardFilter() const   { return { 10, 186, 412, 136 }; }
    juce::Rectangle<int> cardLevel() const    { return { 430, 186, 520, 136 }; }
    juce::Rectangle<int> cardLfo() const      { return { 10, 12, 520, 136 }; }
    juce::Rectangle<int> cardDepth() const    { return { 538, 12, 412, 136 }; }
    juce::Rectangle<int> cardPattern() const  { return { 10, 156, 940, 40 }; }   // Titel links, Bedienung in einer Reihe
    juce::Rectangle<int> cardLive() const     { return { 10, 202, 940, 120 }; }

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
        for (int p : { pSync, pFbInv, pFreeze, pFbStable, pTape, pInDry, pPosDry, pOn }) toggles[(size_t) p]->setVisible (echo);
        linkToggle->setVisible (echo);
        wowBox.setVisible (echo);
        echoDisplay.setVisible (echo);
        filterGraph.setVisible (echo);

        // Single-Delay (LINK an, Standard): die rechte Zeit folgt der linken (ausgegraut); LINK aus gibt sie frei
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
        knobs[pLfoAmt]->setVisible (! echo);
        // LFO aus: die LFO-Regler werden abgedunkelt
        for (auto* c : std::initializer_list<juce::Component*> { choices[pWave].get(), toggles[pLfoSync].get(), knobs[pRate].get(), knobs[pLfoDiv].get(),
                                                                  knobs[pPhase].get(), knobs[pFPhase].get(), knobs[pLfoAmt].get() })
        {
            c->setEnabled (lfoOn);
            c->setAlpha (lfoOn ? 1.0f : 0.5f);
        }
        for (int p : { pDDepth, pFDepth, pEnv, pEnvAmt }) knobs[(size_t) p]->setVisible (! echo);
        // Schritt-Muster: ersetzt die Welle (abgedunkelt); Seed nur fuer Random
        // DRAW: gezeichnete Stufen ersetzen das Muster; der Editor liegt dann statt der Live-Anzeige in der unteren Karte
        const bool draw = val (pPatCustom);
        const int pat = (int) (cur (pPattern) + 0.5f);
        const bool stepped = pat != PatOff || draw;
        for (auto* c : std::initializer_list<juce::Component*> { choices[pPattern].get(), toggles[pPatCustom].get(), choices[pSteps].get(), &glideBox, &seedBox, &seedBtn })
            c->setVisible (! echo);
        for (auto* c : std::initializer_list<juce::Component*> { choices[pSteps].get(), &glideBox })
        {
            c->setEnabled (stepped);
            c->setAlpha (stepped ? 1.0f : 0.45f);
        }
        choices[pPattern]->setAlpha (draw ? 0.45f : 1.0f);
        seedBox.setEnabled (pat == PatRandom && ! draw);
        seedBox.setAlpha (pat == PatRandom && ! draw ? 1.0f : 0.45f);
        if (stepped) { choices[pWave]->setEnabled (false); choices[pWave]->setAlpha (0.45f); }

        // Abdunkeln, was gerade nichts bewirkt (bleibt einstellbar)
        auto dimC = [] (juce::Component* c, bool on) { if (c != nullptr) { c->setEnabled (on); c->setAlpha (on ? 1.0f : 0.5f); } };
        const bool filt = val (pHpOn) || val (pLpOn);
        dimC (toggles[pFbStable].get(), filt || val (pBellOn));                 // STABLE braucht ein Filter in der Schleife
        const bool delayMod = cur (pDDepth) > 0.001f, filterMod = filt && cur (pFDepth) > 0.001f;
        dimC (knobs[pFDepth].get(), filt);                                       // Filter-Tiefe wirkt nur auf HP/LP
        dimC (knobs[pFPhase].get(), filterMod && lfoOn);
        dimC (knobs[pEnv].get(), delayMod || filterMod);
        dimC (knobs[pEnvAmt].get(), (delayMod || filterMod) && cur (pEnv) > 0.001f);
        dimC (knobs[pLfoAmt].get(), (delayMod || filterMod) && lfoOn && cur (pEnv) < 0.999f);
        modDisplay.setVisible (! echo && ! draw);
        stepEditor.setVisible (! echo && draw);
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
    ui::FineSlider glideBox { juce::Slider::LinearBar, juce::Slider::TextBoxLeft }, seedBox { juce::Slider::LinearBar, juce::Slider::TextBoxLeft },
                   wowBox { juce::Slider::LinearBar, juce::Slider::TextBoxLeft };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> glideAttach, seedAttach, wowAttach;
    juce::TextButton seedBtn;
    EchoDisplay echoDisplay;
    ModDisplay modDisplay;
    ui::StepEditor stepEditor;
    ui::FilterGraph filterGraph;
};
