#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace qe
{
// Start-Presets: fertig eingestellte Klaenge als Ausgangspunkt
int numPresets();
juce::String presetName (int index);
// Setzt zuerst alles auf Standard, dann die Werte des Presets (Message-Thread)
void applyPreset (juce::AudioProcessorValueTreeState& apvts, int index);

// Eigene Presets: XML-Dateien (*.qepreset) in Dokumente/QUAD ECHO/Presets, mit Zustandsversion (aeltere werden
// beim Laden umgerechnet). Nur Parameterwerte, kein UI-Zustand. Message-Thread.
juce::File userPresetFolder();
juce::Array<juce::File> userPresets();
bool saveUserPreset (juce::AudioProcessorValueTreeState& apvts, const juce::File& file);
bool loadUserPreset (juce::AudioProcessorValueTreeState& apvts, const juce::File& file);
} // namespace qe
