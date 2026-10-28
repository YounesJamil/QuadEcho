#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "StateMigration.h"

QuadEchoProcessor::QuadEchoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "STATE", qe::createLayout()),
      reader (apvts)
{
    apvts.state.setProperty (qe::kStateVersionAttr, qe::kStateVersion, nullptr);
}

void QuadEchoProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (0);
}

bool QuadEchoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return in == out;
}

void QuadEchoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int n = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);
    if (numCh == 0 || n == 0)
        return;

    reader.read (settings);

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                if (*b > 1.0) bpm = *b;

    lastBpm.store (bpm, std::memory_order_relaxed);

    if (numCh >= 2)
    {
        engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, settings, bpm);
    }
    else
    {
        // Mono: intern Stereo (L = R), Ausgang wird heruntergemischt
        float* mono = buffer.getWritePointer (0);
        for (int pos = 0; pos < n; pos += kScratch)
        {
            const int len = std::min (kScratch, n - pos);
            std::copy (mono + pos, mono + pos + len, scratchL.begin());
            std::copy (mono + pos, mono + pos + len, scratchR.begin());
            engine.process (scratchL.data(), scratchR.data(), len, settings, bpm);
            for (int i = 0; i < len; ++i)
                mono[pos + i] = 0.5f * (scratchL[(size_t) i] + scratchR[(size_t) i]);
        }
    }
}

juce::AudioProcessorEditor* QuadEchoProcessor::createEditor() { return new QuadEchoEditor (*this); }

void QuadEchoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void QuadEchoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            qe::migrateState (*xml);   // aeltere Zustaende auf die aktuelle Bedeutung der Parameter umrechnen
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new QuadEchoProcessor(); }
