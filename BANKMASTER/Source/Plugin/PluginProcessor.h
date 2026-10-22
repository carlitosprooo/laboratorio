// BANKMASTER BY KALOX — audio processor (single instance, full chain)
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "PresetManager.h"

class BankMasterProcessor : public juce::AudioProcessor,
                            private juce::Timer
{
public:
    static constexpr const char* kName = "BANKMASTER";
    static constexpr int kAnalyzerFifoSize = 1 << 15;

    BankMasterProcessor();
    ~BankMasterProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return kName; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- editor access
    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }
    juce::UndoManager& getUndoManager() noexcept { return undoManager; }
    /** Undo helpers. APVTS copies parameter changes into its ValueTree (where undo is
        recorded) from an adaptive timer of up to ~500 ms; copyState() flushes that
        immediately, so each transaction contains exactly the edits made before it. */
    void beginUndoTransaction (const juce::String& name = {}) { state.copyState(); undoManager.beginNewTransaction (name); }
    void undo() { state.copyState(); undoManager.undo(); }
    void redo() { state.copyState(); undoManager.redo(); }
    PresetManager& getPresets() noexcept { return presets; }
    bm::MasterChain& getChain() noexcept { return chain; }
    double getCurrentSampleRate() const noexcept { return currentRate; }
    bm::ChainParams getCurrentParams() const { return cache.read(); }

    /** Analyzer taps: mono pre-EQ / post-EQ samples (single producer / single consumer). */
    int pullAnalyzer (bool post, float* dest, int maxSamples);

    // ---- INPUT LEARN: measures the integrated loudness of the incoming mix and
    // sets INPUT gain so the chain always works from the same reference level
    void startInputLearn();
    /** Stops learning; when apply is true sets INPUT = target - measured. Returns the gain set (dB) or NaN. */
    float stopInputLearn (bool apply);
    bool isLearningInput() const noexcept { return learning; }
    float getLearnMeasuredLufs() { return chain.getInputLoudness().getIntegrated(); }

    // ---- A/B compare
    int getAbSlot() const noexcept { return abSlot; }
    void selectAbSlot (int slot);
    void copyAbToOther();

    /** Editor preferences saved with the session but outside the parameter state
        (window size, EQ display range, analyzer mode, open page). Message thread only. */
    juce::ValueTree& getUiState() noexcept { return uiState; }

    /** Number of samples processed since prepare (for tests / diagnostics). */
    juce::int64 getProcessedSamples() const noexcept { return processed.load(); }

private:
    void timerCallback() override;
    struct Fifo
    {
        juce::AbstractFifo fifo { kAnalyzerFifoSize };
        std::vector<float> data = std::vector<float> ((size_t) kAnalyzerFifoSize, 0.0f);
        void push (const float* x, int n);
        int pull (float* dest, int maxSamples);
    };

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState state;
    bmp::ParamCache cache;
    PresetManager presets;
    bm::MasterChain chain;
    Fifo preFifo, postFifo;
    double currentRate = 48000.0;
    int maxBlock = 512;
    std::atomic<int> pendingLatency { -1 };
    std::atomic<juce::int64> processed { 0 };
    juce::ValueTree abSlots[2];
    int abSlot = 0;
    bool learning = false;
    juce::ValueTree uiState { "UI" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BankMasterProcessor)
};
