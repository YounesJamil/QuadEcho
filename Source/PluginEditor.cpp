#include "PluginEditor.h"
#include "Presets.h"

void QuadEchoEditor::Content::paint (juce::Graphics& g)
{
    g.fillAll (ui::kBg);
    const auto b = getLocalBounds().toFloat();
    ui::drawGrain (g, b, 0.0f, 1.0f, true);   // Hintergrund: weiche Struktur (keine Pixel)
    // leichte Abdunklung zu den Raendern
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, b.getCentreX(), b.getHeight() * 0.4f,
                                             juce::Colours::black.withAlpha (0.3f), 0.0f, 0.0f, true));
    g.fillRect (b);
    if (painter) painter (g);
}

QuadEchoEditor::QuadEchoEditor (QuadEchoProcessor& p)
    : AudioProcessorEditor (&p), proc (p), routing (p.apvts), pad (p.apvts)
{
    setLookAndFeel (&laf);
    addAndMakeVisible (content);
    setWantsKeyboardFocus (true);

    auto& apvts = p.apvts;

    title.setText ("QUAD ECHO", juce::dontSendNotification);
    title.setFont (ui::uiFont (24.0f, true));
    title.setColour (juce::Label::textColourId, ui::kText);
    subtitle.setText ("4x delay / routing / 3D", juce::dontSendNotification);
    subtitle.setFont (ui::uiFont (12.0f));
    subtitle.setColour (juce::Label::textColourId, ui::kDim);
    outLabel.setText ("OUT", juce::dontSendNotification);
    outLabel.setFont (ui::uiFont (11.0f, true));
    outLabel.setColour (juce::Label::textColourId, ui::kDim);
    outLabel.setJustificationType (juce::Justification::centredRight);
    credit.setText (juce::String::fromUTF8 ("QUAD ECHO  \xc2\xb7  by Younes Jamil"), juce::dontSendNotification);
    credit.setFont (ui::uiFont (11.0f));
    credit.setColour (juce::Label::textColourId, ui::kDim);
    credit.setJustificationType (juce::Justification::centredRight);
    for (auto* l : { &title, &subtitle, &outLabel, &credit }) content.addAndMakeVisible (*l);

    // Karten im Kopfbereich: Master (links unter dem Routing) und Position/Raum (rechts neben dem Pad)
    content.painter = [this] (juce::Graphics& g)
    {
        // weicher Schatten unter den grossen Flaechen (einfach gezeichnet: Meter und Kurven loesen oft Neuzeichnen aus)
        auto shadow = [&g] (juce::Rectangle<int> r, float radius)
        {
            const auto f = r.toFloat().translated (0.0f, 2.0f);
            g.setColour (juce::Colours::black.withAlpha (0.18f));
            g.fillRoundedRectangle (f.expanded (3.0f), radius + 3.0f);
            g.setColour (juce::Colours::black.withAlpha (0.3f));
            g.fillRoundedRectangle (f.expanded (1.0f), radius + 1.0f);
        };
        if (! topCollapsed) shadow (pad.getBounds(), 10.0f);
        shadow (panels[(size_t) currentUnit]->getBounds(), 10.0f);
        if (expert->isVisible()) shadow (expert->getBounds(), 10.0f);
        for (auto& c : chips) shadow (c->getBounds(), 8.0f);
        if (! topCollapsed)
        {
            ui::drawCard (g, masterCard(), "MASTER", ui::kAccent);
            ui::drawCard (g, roomCard(), "POSITION & ROOM", ui::kAccent);
        }
    };

    // Presets, Undo/Redo, Master-Meter
    refreshPresets();
    presetBox.beforePopup = [this] { refreshPresets(); };
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.setTooltip ("Factory sounds and your own presets (Documents/QUAD ECHO/Presets). \"Save current as...\" stores the current sound.");
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id >= 1 && id <= qe::numPresets()) { qe::applyPreset (proc.apvts, id - 1); return; }
        if (id >= kUserPresetId && id < kUserPresetId + userPresetFiles.size())
        {
            qe::loadUserPreset (proc.apvts, userPresetFiles[id - kUserPresetId]);
            return;
        }
        presetBox.setSelectedId (0, juce::dontSendNotification);   // Aktionen sind keine Auswahl
        if (id == kOpenFolderId)
        {
            qe::userPresetFolder().createDirectory();
            qe::userPresetFolder().startAsProcess();
        }
        else if (id == kSavePresetId)
        {
            qe::userPresetFolder().createDirectory();
            presetChooser = std::make_unique<juce::FileChooser> ("Save preset", qe::userPresetFolder().getChildFile ("My Preset.qepreset"), "*.qepreset");
            presetChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                            | juce::FileBrowserComponent::warnAboutOverwriting,
                                        [this] (const juce::FileChooser& fc)
                                        {
                                            auto f = fc.getResult();
                                            if (f == juce::File()) return;
                                            f = f.withFileExtension ("qepreset");
                                            if (qe::saveUserPreset (proc.apvts, f))
                                            {
                                                refreshPresets();
                                                presetBox.setText (f.getFileNameWithoutExtension(), juce::dontSendNotification);
                                            }
                                        });
        }
    };
    content.addAndMakeVisible (presetBox);
    undoBtn.setTooltip ("Undo (Ctrl+Z)");
    redoBtn.setTooltip ("Redo (Ctrl+Y or Ctrl+Shift+Z)");
    undoBtn.onClick = [this] { proc.undoManager.undo(); };
    redoBtn.onClick = [this] { proc.undoManager.redo(); };
    content.addAndMakeVisible (undoBtn);
    content.addAndMakeVisible (redoBtn);
    masterMeter = std::make_unique<ui::PeakMeter> ([this] { return proc.getEngine().takeMasterPeak(); });
    content.addAndMakeVisible (*masterMeter);

    content.addAndMakeVisible (routing);

    // Routing-Modus als Textfelder, daneben die Tiefe der Phasenmodulation als Wertfeld (wie die Offset-Pillen)
    {
        std::vector<ui::IconChoice::Icon> list;
        for (const char* t : { "AUDIO", "PHASE", "PHASE + DRY" })
            list.push_back ([t] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
                            { g.setColour (c); g.setFont (ui::uiFont (11.0f, true)); g.drawText (t, r, juce::Justification::centred); });
        routeMode = std::make_unique<ui::IconChoice> (apvts, qe::kIdRouteMode, std::move (list),
            juce::StringArray { "Audio: a unit feeds the next with its sound (echoes of echoes)",
                                "Phase: the echoes of a unit move the delay time of the next one (FM-like); the next one gets your input",
                                "Phase + Dry: like Phase, but the whole output (with the original) modulates: every note bends itself" },
            ui::kAccent);
        content.addAndMakeVisible (*routeMode);
        routeDepth.getProperties().set ("numberbox", true);
        routeDepth.setColour (juce::Slider::textBoxTextColourId, ui::kText);
        routeDepth.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        routeDepth.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        routeDepthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, qe::kIdRouteDepth, routeDepth);
        routeDepth.textFromValueFunction = [] (double v) { const double ms = v * v * qe::kMaxRouteMs; return "Depth " + juce::String (ms, ms < 1.0 ? 2 : 1) + " ms"; };
        routeDepth.valueFromTextFunction = [] (const juce::String& t) { return std::sqrt (std::max (0.0, t.retainCharacters (".0123456789").getDoubleValue()) / (double) qe::kMaxRouteMs); };
        routeDepth.setDoubleClickReturnValue (true, 0.25);
        routeDepth.setTooltip ("Phase depth: how far the source moves the delay time at full level (up to 10 ms, fine steps at the start). Only in PHASE modes.");
        routeDepth.updateText();
        content.addAndMakeVisible (routeDepth);
        routeWatch = std::make_unique<juce::ParameterAttachment> (*apvts.getParameter (qe::kIdRouteMode), [this] (float v)
        {
            const bool phase = v > 0.5f;
            routeDepth.setEnabled (phase);
            routeDepth.setAlpha (phase ? 1.0f : 0.4f);
        });
        routeWatch->sendInitialUpdate();
    }
    content.addAndMakeVisible (pad);

    // Globale Regler
    gMix = std::make_unique<ui::Knob> (apvts, qe::kIdGlobalMix, "Global Dry/Wet", ui::kAccent, ui::fmtPct, ui::parsePct);
    gOut = std::make_unique<ui::Knob> (apvts, qe::kIdGlobalOut, "Global Output", ui::kAccent, ui::fmtDb);
    gFb = std::make_unique<ui::Knob> (apvts, qe::kIdGlobalFb, "Feedback Scale", ui::kAccent, ui::fmtPct, ui::parsePct);
    gDoppler = std::make_unique<ui::Knob> (apvts, qe::kIdDoppler, "Doppler", ui::kAccent, ui::fmtPct, ui::parsePct);
    gBinaural = std::make_unique<ui::Toggle> (apvts, qe::kIdBinaural, "BINAURAL", ui::kAccent);
    gDist = std::make_unique<ui::Toggle> (apvts, qe::kIdDistDelay, "DIST. DELAY", ui::kAccent);
    gEarly = std::make_unique<ui::Toggle> (apvts, qe::kIdEarly, "EARLY REFL.", ui::kAccent);
    gLimiter = std::make_unique<ui::Toggle> (apvts, qe::kIdLimiter, "LIMITER", ui::kAccent);
    gLimitDry = std::make_unique<ui::Toggle> (apvts, qe::kIdLimitDry, "ON DRY", ui::kAccent);
    gFreezeAll = std::make_unique<ui::Toggle> (apvts, qe::kIdFreezeAll, "FREEZE ALL", ui::kNoiseColour);
    limiterWatch = std::make_unique<juce::ParameterAttachment> (*apvts.getParameter (qe::kIdLimiter), [this] (float v)
    {
        gLimitDry->setEnabled (v > 0.5f);   // ON DRY wirkt nur mit Limiter
        gLimitDry->setAlpha (v > 0.5f ? 1.0f : 0.5f);
    });
    limiterWatch->sendInitialUpdate();
    gFreezeAll->setTooltip ("Freezes all units at once (in addition to their own FREEZE): the echoes hold as endless loops.");
    for (auto* k : { gMix.get(), gOut.get(), gFb.get(), gDoppler.get() })
    {
        k->slider.setTooltip (ui::tooltipForGlobal (k == gMix.get() ? qe::kIdGlobalMix : k == gOut.get() ? qe::kIdGlobalOut
                                                  : k == gFb.get() ? qe::kIdGlobalFb : qe::kIdDoppler));
        content.addAndMakeVisible (*k);
    }
    gBinaural->setTooltip (ui::tooltipForGlobal (qe::kIdBinaural));
    gDist->setTooltip (ui::tooltipForGlobal (qe::kIdDistDelay));
    gEarly->setTooltip (ui::tooltipForGlobal (qe::kIdEarly));
    gLimiter->setTooltip (ui::tooltipForGlobal (qe::kIdLimiter));
    gLimitDry->setTooltip (ui::tooltipForGlobal (qe::kIdLimitDry));
    for (auto* t : { gBinaural.get(), gDist.get(), gEarly.get(), gLimiter.get(), gLimitDry.get(), gFreezeAll.get() }) content.addAndMakeVisible (*t);

    // Positions-Zahlenfelder (A-D und Noise): Balken von der Mitte aus, Wert per Doppelklick eintippen
    static const char* heads[3] = { "X  LEFT / RIGHT", "Y  BACK / FRONT", "Z  HEIGHT" };
    for (int k = 0; k < 3; ++k)
    {
        auto& h = posHeads[(size_t) k];
        h.setText (heads[k], juce::dontSendNotification);
        h.setFont (ui::uiFont (9.5f, true));
        h.setColour (juce::Label::textColourId, ui::kDim);
        h.setJustificationType (juce::Justification::centred);
        content.addAndMakeVisible (h);
    }
    for (int u = 0; u < kPosRows; ++u)
        for (int k = 0; k < 3; ++k)
        {
            const bool isNoise = u == qe::kNumUnits;
            const auto rowColour = isNoise ? ui::kNoiseColour : ui::unitColour (u);
            auto s = std::make_unique<ui::FineSlider> (juce::Slider::LinearBar, juce::Slider::TextBoxLeft);
            s->getProperties().set ("bipolar", true);
            s->setColour (juce::Slider::trackColourId, rowColour.withAlpha (0.45f));
            s->setColour (juce::Slider::textBoxTextColourId, ui::kValue);
            s->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            const juce::String id = isNoise ? juce::String (qe::kGlobalIds[qe::nzPosX + k]) : qe::paramId (u, qe::pPosX + k);
            posAttach[(size_t) u][(size_t) k] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, *s);
            s->textFromValueFunction = [] (double v) { return (v > 0.004 ? "+" : "") + juce::String (v, 2); };
            s->valueFromTextFunction = [] (const juce::String& t) { return t.retainCharacters ("-.0123456789").getDoubleValue(); };
            if (auto* prm = apvts.getParameter (id))
                s->setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
            s->setTooltip ((k == 0 ? juce::String ("Left (-) / right (+)") : k == 1 ? juce::String ("Back (-) / front (+): distance from the listener") : juce::String ("Height: down (-) / up (+)"))
                           + ". Drag (Shift = fine), click to type an exact value, double-click: default.");
            s->updateText();
            content.addAndMakeVisible (*s);
            posSliders[(size_t) u][(size_t) k] = std::move (s);
        }
    for (int u = 0; u < kPosRows; ++u)
    {
        auto& pl = posLabels[(size_t) u];
        pl.setText (u < qe::kNumUnits ? juce::String::charToString ((juce::juce_wchar) ('A' + u)) : juce::String ("N"), juce::dontSendNotification);
        pl.setFont (ui::uiFont (14.0f, true));
        pl.setColour (juce::Label::textColourId, u < qe::kNumUnits ? ui::unitColour (u) : ui::kNoiseColour);
        pl.setJustificationType (juce::Justification::centred);
        if (u == qe::kNumUnits) pl.setTooltip ("Noise source (Expert)");
        content.addAndMakeVisible (pl);
    }

    // Einheiten: Chips, Panels
    for (int u = 0; u < qe::kNumUnits; ++u)
    {
        panels[(size_t) u] = std::make_unique<UnitPanel> (p, u);
        content.addChildComponent (*panels[(size_t) u]);
        chips[(size_t) u] = std::make_unique<UnitChip> (p, u, [this] (int sel) { selectUnit (sel); });
        content.addAndMakeVisible (*chips[(size_t) u]);
    }
    static const char* pageNames[2] = { "ECHO", "MODULATION" };
    for (int i = 0; i < 2; ++i)
    {
        auto& b = pageTabs[(size_t) i];
        b.setButtonText (pageNames[i]);
        b.setClickingTogglesState (true);
        b.setRadioGroupId (102);
        b.setColour (juce::TextButton::buttonOnColourId, ui::kAccent);
        b.onClick = [this, i] { selectPage (i); };
        content.addAndMakeVisible (b);
    }

    // Expert-Bereich
    expert = std::make_unique<ExpertPanel> (p);
    expert->onHeightChanged = [this] { applyExpertSize(); };
    content.addChildComponent (*expert);
    expertBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff5f90c4));
    expertBtn.getProperties().set ("gear", true);
    expertBtn.setTooltip ("Expert section: global filter, noise source and more.");
    expertBtn.onClick = [this] { setExpert (expertBtn.getToggleState()); };
    content.addAndMakeVisible (expertBtn);

    // Zahlenfelder nur fuer Einheiten, die am Ausgang platziert sind (Algorithmus und An/Aus); Noise nur wenn an
    algoWatch = std::make_unique<juce::ParameterAttachment> (*apvts.getParameter (qe::kIdAlgo), [this] (float) { updatePlacement(); });
    for (int u = 0; u < qe::kNumUnits; ++u)
        onWatch[(size_t) u] = std::make_unique<juce::ParameterAttachment> (*apvts.getParameter (qe::paramId (u, qe::pOn)), [this] (float) { updatePlacement(); });
    updatePlacement();
    noiseWatch = std::make_unique<juce::ParameterAttachment> (*apvts.getParameter (qe::kGlobalIds[qe::nzOn]), [this] (float v)
    {
        const bool on = v > 0.5f;
        for (auto& s : posSliders[(size_t) qe::kNumUnits])
        {
            s->setEnabled (on);
            s->setAlpha (on ? 1.0f : 0.35f);
        }
        noiseOnState = on;
        updateNoiseVisibility();
    });
    noiseWatch->sendInitialUpdate();
    // Doppler wirkt nur mit Distanz-Laufzeit: sonst abgedunkelt
    distWatch = std::make_unique<juce::ParameterAttachment> (*apvts.getParameter (qe::kIdDistDelay), [this] (float v)
    {
        const bool on = v > 0.5f;
        gDoppler->setEnabled (on);
        gDoppler->setAlpha (on ? 1.0f : 0.5f);
    });
    distWatch->sendInitialUpdate();

    // Gespeicherter UI-Zustand (Fensterbreite, Einheit, Seite, Expert)
    int w = (int) apvts.state.getProperty ("uiW", kDefaultW);
    if (w < kW * 6 / 10 || w > kW * 2) w = kDefaultW;
    currentUnit = juce::jlimit (0, qe::kNumUnits - 1, (int) apvts.state.getProperty ("uiUnit", 0));
    currentPage = juce::jlimit (0, 1, (int) apvts.state.getProperty ("uiPage", 0));
    expertOpen = (bool) apvts.state.getProperty ("uiExpert", false);
    topCollapsed = (bool) apvts.state.getProperty ("uiCompact", false);
    compactBtn.setColour (juce::ToggleButton::tickColourId, ui::kAccent);
    compactBtn.setTooltip ("Compact: folds away routing, master, pad and position (the window gets smaller). Click again to unfold.");
    compactBtn.setToggleState (topCollapsed, juce::dontSendNotification);
    compactBtn.onClick = [this] { setCompact (compactBtn.getToggleState()); };
    content.addAndMakeVisible (compactBtn);
    pageTabs[(size_t) currentPage].setToggleState (true, juce::dontSendNotification);
    selectPage (currentPage);
    selectUnit (currentUnit);
    expert->setVisible (expertOpen);
    expertBtn.setToggleState (expertOpen, juce::dontSendNotification);
    updateNoiseVisibility();

    setResizable (true, true);
    updateSizeLimits();
    setSize (w, (int) std::round ((double) w * (double) logicalHeight() / (double) kW));
    if (topCollapsed) setCompact (true);   // gespeicherter Zustand: Bereiche ausblenden
    startTimerHz (8);
}

QuadEchoEditor::~QuadEchoEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void QuadEchoEditor::updateSizeLimits()
{
    const double aspect = (double) kW / (double) logicalHeight();
    getConstrainer()->setFixedAspectRatio (aspect);
    setResizeLimits (kW * 6 / 10, (int) std::round (kW * 6 / 10 / aspect), kW * 2, (int) std::round (kW * 2 / aspect));
}

// Noise-Punkt im Pad und Positionszeile "N": sichtbar, wenn die Noise an ist oder der Expert-Modus offen ist
void QuadEchoEditor::updateNoiseVisibility()
{
    const bool show = (expertOpen || noiseOnState) && ! topCollapsed;
    pad.setShowNoise (show);
    posLabels[(size_t) qe::kNumUnits].setVisible (show);
    for (auto& s : posSliders[(size_t) qe::kNumUnits]) s->setVisible (show);
}

// Gleiche Regel wie im DSP: ist eine Ausgangseinheit aus, uebernimmt die letzte eingeschaltete davor
void QuadEchoEditor::updatePlacement()
{
    auto& apvts = proc.apvts;
    const int algo = (int) (ui::currentValue (apvts, qe::kIdAlgo) + 0.5f);   // aktuell, siehe currentValue
    std::array<bool, qe::kNumUnits> on {}, none {};
    for (int u = 0; u < qe::kNumUnits; ++u)
        on[(size_t) u] = ui::currentValue (apvts, qe::paramId (u, qe::pOn)) > 0.5f;
    const auto p = qe::placementOf (algo, on, none, none);
    for (int u = 0; u < qe::kNumUnits; ++u)
    {
        const bool placed = (p.weight[(size_t) u] > 0.0f && on[(size_t) u]) || p.idle[(size_t) u];
        for (auto& s : posSliders[(size_t) u])
        {
            s->setEnabled (placed);
            s->setAlpha (placed ? 1.0f : 0.35f);
        }
    }
}

void QuadEchoEditor::applyExpertSize()
{
    if (! expertOpen) return;
    updateSizeLimits();
    setSize (getWidth(), (int) std::round ((double) getWidth() * (double) logicalHeight() / (double) kW));
}

void QuadEchoEditor::setCompact (bool compact)
{
    topCollapsed = compact;
    proc.apvts.state.setProperty ("uiCompact", compact, nullptr);
    compactBtn.setToggleState (compact, juce::dontSendNotification);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &routing, routeMode.get(), &routeDepth, gMix.get(), gOut.get(), gFb.get(),
                                                                       gLimiter.get(), gLimitDry.get(), gFreezeAll.get(), &pad, gBinaural.get(),
                                                                       gEarly.get(), gDist.get(), gDoppler.get() })
        c->setVisible (! compact);
    for (auto& h : posHeads) h.setVisible (! compact);
    for (int u = 0; u < qe::kNumUnits; ++u)
    {
        posLabels[(size_t) u].setVisible (! compact);
        for (auto& s : posSliders[(size_t) u]) s->setVisible (! compact);
    }
    updateNoiseVisibility();
    updateSizeLimits();
    setSize (getWidth(), (int) std::round ((double) getWidth() * (double) logicalHeight() / (double) kW));
    resized();
    repaint();
}

void QuadEchoEditor::setExpert (bool open)
{
    expertOpen = open;
    proc.apvts.state.setProperty ("uiExpert", open, nullptr);
    expert->setVisible (open);
    expertBtn.setToggleState (open, juce::dontSendNotification);
    updateNoiseVisibility();
    updateSizeLimits();
    setSize (getWidth(), (int) std::round ((double) getWidth() * (double) logicalHeight() / (double) kW));
}

void QuadEchoEditor::timerCallback()
{
    undoBtn.setEnabled (proc.undoManager.canUndo());
    redoBtn.setEnabled (proc.undoManager.canRedo());
}

bool QuadEchoEditor::keyPressed (const juce::KeyPress& key)
{
    const bool cmd = key.getModifiers().isCommandDown();
    const int code = key.getKeyCode();
    if (cmd && (code == 'Z' || code == 'z') && ! key.getModifiers().isShiftDown()) { proc.undoManager.undo(); return true; }
    if (cmd && (code == 'Y' || code == 'y')) { proc.undoManager.redo(); return true; }
    if (cmd && (code == 'Z' || code == 'z') && key.getModifiers().isShiftDown()) { proc.undoManager.redo(); return true; }
    return false;
}

void QuadEchoEditor::selectUnit (int u)
{
    currentUnit = u;
    for (int i = 0; i < qe::kNumUnits; ++i)
    {
        panels[(size_t) i]->setVisible (i == u);
        chips[(size_t) i]->setSelected (i == u);
    }
    panels[(size_t) u]->setPage (currentPage);
    panels[(size_t) u]->toFront (false);
    proc.apvts.state.setProperty ("uiUnit", u, nullptr);
}

void QuadEchoEditor::selectPage (int p)
{
    currentPage = p;
    for (auto& panel : panels) panel->setPage (p);
    proc.apvts.state.setProperty ("uiPage", p, nullptr);
}

void QuadEchoEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::kBg);
}

void QuadEchoEditor::resized()
{
    const float scale = (float) getWidth() / (float) kW;
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, kW, logicalHeight());
    proc.apvts.state.setProperty ("uiW", getWidth(), nullptr);

    title.setBounds (20, 8, 200, 32);
    subtitle.setBounds (196, 14, 160, 20);
    compactBtn.setBounds (340, 10, 82, 28);
    expertBtn.setBounds (428, 10, 104, 28);
    presetBox.setBounds (540, 10, 150, 28);
    undoBtn.setBounds (698, 10, 48, 28);
    redoBtn.setBounds (750, 10, 48, 28);
    outLabel.setBounds (806, 14, 32, 20);
    masterMeter->setBounds (842, 19, 148, 10);

    routing.setBounds (20, 48, 360, 126);
    routeMode->setBounds (20, 178, 260, 22);
    routeDepth.setBounds (286, 179, 94, 20);

    // Master: Mix, Output, Feedback-Bremse, Limiter
    const auto m = masterCard();
    gMix->setBounds (m.getX() + 6, m.getY() + 22, 88, 102);
    gOut->setBounds (m.getX() + 94, m.getY() + 22, 88, 102);
    gFb->setBounds (m.getX() + 182, m.getY() + 22, 88, 102);
    gLimiter->setBounds (m.getX() + 274, m.getY() + 24, 80, 26);
    gLimitDry->setBounds (m.getX() + 274, m.getY() + 56, 80, 26);
    gFreezeAll->setBounds (m.getX() + 274, m.getY() + 88, 80, 26);

    pad.setBounds (392, 48, 288, 290);

    // Position (Zahlen) und Raum (Kopfhoerer, Laufzeit, Reflexionen, Doppler)
    const auto rc = roomCard();
    const int colX = rc.getX() + 26, colW = 84, rowY = rc.getY() + 40, rowH = 24, rowStep = 28;
    for (int k = 0; k < 3; ++k)
        posHeads[(size_t) k].setBounds (colX + k * (colW + 2), rc.getY() + 22, colW, 14);
    for (int u = 0; u < kPosRows; ++u)
    {
        const int y = rowY + u * rowStep;
        posLabels[(size_t) u].setBounds (rc.getX() + 6, y, 18, rowH);
        for (int k = 0; k < 3; ++k)
            posSliders[(size_t) u][(size_t) k]->setBounds (colX + k * (colW + 2), y, colW, rowH);
    }
    // Doppler sitzt neben DIST. DELAY, von dem er abhaengt
    const int roomY = rowY + kPosRows * rowStep + 8;
    gBinaural->setBounds (rc.getX() + 8, roomY, 100, 30);
    gEarly->setBounds (rc.getX() + 110, roomY, 100, 30);
    gDist->setBounds (rc.getX() + 110, roomY + 36, 100, 30);
    gDoppler->setBounds (rc.getX() + 210, roomY - 6, 76, 96);
    gDoppler->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 15);

    const int dy = -topShift();   // eingeklappt: alles darunter rueckt hoch
    for (int u = 0; u < qe::kNumUnits; ++u)
        chips[(size_t) u]->setBounds (20 + u * 118, 346 + dy, 112, 46);
    pageTabs[0].setBounds (806, 350 + dy, 84, 34);
    pageTabs[1].setBounds (894, 350 + dy, 96, 34);

    for (auto& panel : panels)
        panel->setBounds (20, 398 + dy, 960, 334);

    credit.setBounds (600, 734 + dy, 390, 14);
    expert->setBounds (20, kBaseH + 2 + dy, 960, expert->preferredHeight());
}

// Preset-Menue: Werks-Presets, eigene Presets aus dem Dokumente-Ordner, Speichern und Ordner oeffnen
void QuadEchoEditor::refreshPresets()
{
    const auto text = presetBox.getText();
    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("Factory");
    for (int i = 0; i < qe::numPresets(); ++i) presetBox.addItem (qe::presetName (i), i + 1);
    userPresetFiles = qe::userPresets();
    if (! userPresetFiles.isEmpty())
    {
        presetBox.addSectionHeading ("My presets");
        for (int i = 0; i < userPresetFiles.size(); ++i) presetBox.addItem (userPresetFiles[i].getFileNameWithoutExtension(), kUserPresetId + i);
    }
    presetBox.addSeparator();
    presetBox.addItem ("Save current as...", kSavePresetId);
    presetBox.addItem ("Open presets folder", kOpenFolderId);
    presetBox.setText (text, juce::dontSendNotification);
}
