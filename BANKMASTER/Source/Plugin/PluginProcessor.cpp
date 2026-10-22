// BANKMASTER BY KALOX — audio processor (single instance, full chain)
#include "PluginProcessor.h"
#include "PluginEditor.h"

BankMasterProcessor::BankMasterProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, &undoManager, "BANKMASTER", bmp::createLayout()),
      cache (state),
      presets (state)
{
    presets.setCurrentPresetName (bm::getFactoryPresets().front().name);
    chain.prepare (currentRate, maxBlock, 2);
    chain.setParams (cache.read());
    chain.reset();
    startTimerHz (10);
}

BankMasterProcessor::~BankMasterProcessor() { stopTimer(); }

bool BankMasterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void BankMasterProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentRate = sampleRate;
    maxBlock = juce::jmax (16, samplesPerBlock);
    chain.prepare (sampleRate, maxBlock, juce::jmax (1, getTotalNumOutputChannels()));
    chain.setParams (cache.read());
    chain.reset();
    setLatencySamples (chain.getLatency());
    pendingLatency.store (-1);
    preFifo.fifo.reset(); postFifo.fifo.reset();
    processed.store (0);
}

void BankMasterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();
    for (int c = numCh; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    if (numCh == 0 || n == 0) return;

    chain.setParams (cache.read());
    const int lat = chain.getLatency();
    if (lat != getLatencySamples()) pendingLatency.store (lat); // reported from the message thread

    // process in chunks of the prepared size so the analyzer taps cover every sample
    for (int start = 0; start < n; start += maxBlock)
    {
        const int len = juce::jmin (maxBlock, n - start);
        float* d[2] = { buffer.getWritePointer (0, start), numCh > 1 ? buffer.getWritePointer (1, start) : nullptr };
        chain.process (d, numCh, len);
        preFifo.push (chain.getPreEqTap(), len);
        postFifo.push (chain.getPostEqTap(), len);
    }
    processed.fetch_add (n);
}

void BankMasterProcessor::timerCallback()
{
    const int lat = pendingLatency.exchange (-1);
    if (lat >= 0 && lat != getLatencySamples()) setLatencySamples (lat);
}

void BankMasterProcessor::Fifo::push (const float* x, int n)
{
    int s1, n1, s2, n2;
    fifo.prepareToWrite (n, s1, n1, s2, n2);
    if (n1 > 0) std::copy (x, x + n1, data.data() + s1);
    if (n2 > 0) std::copy (x + n1, x + n1 + n2, data.data() + s2);
    fifo.finishedWrite (n1 + n2); // when full, newest samples are dropped (UI only)
}

int BankMasterProcessor::Fifo::pull (float* dest, int maxSamples)
{
    int s1, n1, s2, n2;
    fifo.prepareToRead (maxSamples, s1, n1, s2, n2);
    if (n1 > 0) std::copy (data.data() + s1, data.data() + s1 + n1, dest);
    if (n2 > 0) std::copy (data.data() + s2, data.data() + s2 + n2, dest + n1);
    fifo.finishedRead (n1 + n2);
    return n1 + n2;
}

int BankMasterProcessor::pullAnalyzer (bool post, float* dest, int maxSamples)
{
    return (post ? postFifo : preFifo).pull (dest, maxSamples);
}

// ---------------------------------------------------------------------------- programs
int BankMasterProcessor::getNumPrograms() { return presets.getNumFactoryPresets(); }
int BankMasterProcessor::getCurrentProgram() { return juce::jmax (0, presets.getCurrentPresetIndex()); }
void BankMasterProcessor::setCurrentProgram (int index) { presets.loadPreset (index); }
const juce::String BankMasterProcessor::getProgramName (int index)
{
    const auto& f = bm::getFactoryPresets();
    return index >= 0 && index < (int) f.size() ? juce::String (f[(size_t) index].name) : juce::String();
}

// ---------------------------------------------------------------------------- state
void BankMasterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto tree = state.copyState();
    tree.setProperty ("version", JucePlugin_VersionString, nullptr);
    if (uiState.getNumProperties() > 0) tree.appendChild (uiState.createCopy(), nullptr);
    if (auto xml = tree.createXml()) copyXmlToBinary (*xml, destData);
}

void BankMasterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            const auto ui = tree.getChildWithName (uiState.getType());
            if (ui.isValid())
            {
                uiState.copyPropertiesFrom (ui, nullptr);
                tree.removeChild (ui, nullptr);
            }
            bmp::replaceStateAndSync (state, tree);
        }
}

// ---------------------------------------------------------------------------- INPUT LEARN
void BankMasterProcessor::startInputLearn()
{
    chain.getInputLoudness().requestReset();
    learning = true;
}

float BankMasterProcessor::stopInputLearn (bool apply)
{
    learning = false;
    const float measured = chain.getInputLoudness().getIntegrated();
    if (! apply || ! std::isfinite (measured)) return std::numeric_limits<float>::quiet_NaN();
    const float target = state.getRawParameterValue (bmp::id::inTarget)->load();
    const float gain = juce::jlimit (-24.0f, 24.0f, target - measured);
    if (auto* p = state.getParameter (bmp::id::inGain))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (gain));
        p->endChangeGesture();
    }
    return gain;
}

// ---------------------------------------------------------------------------- A/B
void BankMasterProcessor::selectAbSlot (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == abSlot) return;
    abSlots[abSlot] = state.copyState();
    if (! abSlots[slot].isValid()) abSlots[slot] = state.copyState();
    bmp::replaceStateAndSync (state, abSlots[slot].createCopy());
    abSlot = slot;
}

void BankMasterProcessor::copyAbToOther()
{
    abSlots[1 - abSlot] = state.copyState();
}

juce::AudioProcessorEditor* BankMasterProcessor::createEditor() { return new BankMasterEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BankMasterProcessor(); }
