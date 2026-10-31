#pragma once
#include "ExpertPanel.h"
#include "PadComponent.h"
#include "PluginProcessor.h"
#include "RoutingSelector.h"
#include "UiCommon.h"
#include "UiExtras.h"
#include "UnitPanel.h"
#include "Widgets.h"

class QuadEchoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit QuadEchoEditor (QuadEchoProcessor&);
    ~QuadEchoEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    struct Content : public juce::Component
    {
        void paint (juce::Graphics& g) override;
        std::function<void (juce::Graphics&)> painter;   // Karten im Kopfbereich
    };

    static juce::Rectangle<int> masterCard() { return { 20, 206, 360, 132 }; }
    static juce::Rectangle<int> roomCard()   { return { 692, 48, 288, 290 }; }

    void timerCallback() override;
    void selectUnit (int u);
    void selectPage (int p);
    void setExpert (bool open);
    void setCompact (bool compact);   // oberer Bereich (Routing, Master, Pad, Position & Room) eingeklappt
    static constexpr int kTopH = 298;   // so viel ruecken Einheiten und Expert beim Einklappen hoch
    int topShift() const { return topCollapsed ? kTopH : 0; }
    int logicalHeight() const { return kBaseH - topShift() + (expertOpen ? (expert != nullptr ? expert->preferredHeight() : ExpertPanel::kHeight) + 6 : 0); }
    void updateSizeLimits();
    void updateNoiseVisibility();
    void updatePlacement();
    void applyExpertSize();

    static constexpr int kW = 1000, kBaseH = 752;
    static constexpr int kDefaultW = 860;   // Standardgroesse: 14 % kleiner als die Entwurfsbreite (mit Expert passt es auf 1080p)
    static constexpr int kPosRows = qe::kNumUnits + 1;   // A-D und Noise

    QuadEchoProcessor& proc;
    juce::SharedResourcePointer<ui::GrainTile> grain;   // Koernung, solange der Editor offen ist
    juce::SharedResourcePointer<ui::EmbeddedFonts> fonts;  // eingebaute Schrift, solange der Editor offen ist
    ui::QuadLookAndFeel laf;
    Content content;
    juce::TooltipWindow tooltipWindow { &content, 450 };

    RoutingSelector routing;
    std::unique_ptr<ui::IconChoice> routeMode;   // Audio / Phase / Phase + Dry
    ui::FineSlider routeDepth { juce::Slider::LinearBar, juce::Slider::TextBoxLeft };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> routeDepthAttach;
    std::unique_ptr<juce::ParameterAttachment> routeWatch, limiterWatch;
    PadComponent pad;
    std::unique_ptr<ui::Knob> gMix, gOut, gFb, gDoppler;
    std::unique_ptr<ui::Toggle> gBinaural, gDist, gEarly, gLimiter, gLimitDry, gFreezeAll, gAutoLevel;
    std::array<std::array<std::unique_ptr<juce::Slider>, 3>, kPosRows> posSliders;
    std::array<std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 3>, kPosRows> posAttach;
    std::array<std::unique_ptr<UnitPanel>, qe::kNumUnits> panels;
    std::array<std::unique_ptr<UnitChip>, qe::kNumUnits> chips;
    std::array<juce::TextButton, 2> pageTabs;
    juce::TextButton undoBtn { "Undo" }, redoBtn { "Redo" };
    juce::ToggleButton expertBtn { "EXPERT" }, compactBtn { "COMPACT" };
    // Preset-Menue: vor dem Aufklappen neu einlesen (eigene Presets koennen dazugekommen sein)
    struct PresetBox : juce::ComboBox
    {
        std::function<void()> beforePopup;
        void showPopup() override { if (beforePopup) beforePopup(); juce::ComboBox::showPopup(); }
    };
    PresetBox presetBox;
    juce::Array<juce::File> userPresetFiles;
    std::unique_ptr<juce::FileChooser> presetChooser;
    void refreshPresets();
    static constexpr int kUserPresetId = 100, kSavePresetId = 1000, kOpenFolderId = 1001;
    std::unique_ptr<ui::PeakMeter> masterMeter;
    juce::Label title, subtitle, outLabel, credit;
    std::array<juce::Label, kPosRows> posLabels;
    std::array<juce::Label, 3> posHeads;
    std::unique_ptr<juce::ParameterAttachment> algoWatch, noiseWatch, distWatch;
    std::array<std::unique_ptr<juce::ParameterAttachment>, qe::kNumUnits> onWatch;
    std::unique_ptr<ExpertPanel> expert;

    bool expertOpen = false, noiseOnState = false, topCollapsed = false;
    int currentUnit = 0, currentPage = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuadEchoEditor)
};
