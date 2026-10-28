#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace qe
{
// Start-Presets: fertig eingestellte Klaenge als Ausgangspunkt
int numPresets();
juce::String presetName (int index);
// Setzt zuerst alles auf Standard, dann die Werte des Presets (Message-Thread)
void applyPreset (juce::AudioProcessorValueTreeState& apvts, int index);
} // namespace qe
