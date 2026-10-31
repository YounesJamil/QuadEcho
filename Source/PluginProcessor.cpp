#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "StateMigration.h"
#include "Tail.h"

QuadEchoProcessor::QuadEchoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
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
    if (layouts.inputBuses.size() > 1)   // Sidechain: aus, mono oder stereo
    {
        const auto& sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return in == out;
}

void QuadEchoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);
    auto main = getBusBuffer (buffer, false, 0);   // Haupt-Bus (Ein = Aus); der Sidechain liegt in eigenen Kanaelen
    const int numCh = main.getNumChannels();
    if (numCh == 0 || n == 0)
        return;
    const float* scL = nullptr;
    const float* scR = nullptr;
    if (getBusCount (true) > 1)
    {
        const auto sc = getBusBuffer (buffer, true, 1);
        if (sc.getNumChannels() > 0)
        {
            scL = sc.getReadPointer (0);
            scR = sc.getReadPointer (std::min (1, sc.getNumChannels() - 1));
        }
    }

    reader.read (settings);

    double bpm = 120.0, ppq = -1.0;   // ppq: Songposition, nur waehrend der Wiedergabe (LFO-Sync auf den Takt)
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())
                if (*b > 1.0) bpm = *b;
            if (pos->getIsPlaying())
                if (auto q = pos->getPpqPosition())
                    if (*q >= 0.0) ppq = *q;
        }

    lastBpm.store (bpm, std::memory_order_relaxed);

    if (numCh >= 2)
    {
        engine.process (main.getWritePointer (0), main.getWritePointer (1), n, settings, bpm, ppq, scL, scR);
    }
    else
    {
        // Mono: intern Stereo (L = R), Ausgang wird heruntergemischt
        float* mono = main.getWritePointer (0);
        for (int pos = 0; pos < n; pos += kScratch)
        {
            const int len = std::min (kScratch, n - pos);
            std::copy (mono + pos, mono + pos + len, scratchL.begin());
            std::copy (mono + pos, mono + pos + len, scratchR.begin());
            engine.process (scratchL.data(), scratchR.data(), len, settings, bpm,
                            ppq >= 0.0 ? ppq + (double) pos / getSampleRate() * bpm / 60.0 : -1.0,
                            scL != nullptr ? scL + pos : nullptr, scR != nullptr ? scR + pos : nullptr);
            for (int i = 0; i < len; ++i)
                mono[pos + i] = 0.5f * (scratchL[(size_t) i] + scratchR[(size_t) i]);
        }
    }
}

double QuadEchoProcessor::getTailLengthSeconds() const
{
    qe::EngineSettings s;
    reader.read (s);
    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    const double t = qe::tailSeconds (s, lastBpm.load (std::memory_order_relaxed), sr);
    // VST3 kennt "unendlich" (JUCE meldet kInfiniteTail); AU reicht den Wert ungeprueft weiter, dort 10 Minuten
    if (std::isinf (t) && wrapperType != wrapperType_VST3) return 600.0;
    return t;
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
