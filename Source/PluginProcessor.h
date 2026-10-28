#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "QuadEngine.h"

class QuadEchoProcessor : public juce::AudioProcessor
{
public:
    QuadEchoProcessor();
    ~QuadEchoProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "QUAD ECHO"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::UndoManager undoManager { 30000, 2000 };
    juce::AudioProcessorValueTreeState apvts;

    // Fuer die UI (nur lockfreie Abfragen verwenden)
    qe::QuadEngine& getEngine() { return engine; }
    double getBpm() const { return lastBpm.load (std::memory_order_relaxed); }

private:
    static constexpr int kScratch = 512;

    std::atomic<double> lastBpm { 120.0 };
    qe::ParamReader reader;
    qe::QuadEngine engine;
    qe::EngineSettings settings;
    std::array<float, kScratch> scratchL {}, scratchR {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuadEchoProcessor)
};
