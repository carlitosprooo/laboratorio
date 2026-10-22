// BANKMASTER by KALOX — main editor
#pragma once

#include "PluginProcessor.h"
#include "UI/LookAndFeel.h"
#include "UI/Pages.h"
#include "UI/MeterBar.h"

class BankMasterEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    static constexpr int kWidth = 1200, kHeight = 800;

    explicit BankMasterEditor (BankMasterProcessor&);
    ~BankMasterEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    void showTab (int index);
    int getCurrentTab() const noexcept { return currentTab; }

private:
    void timerCallback() override;
    void refreshPresetBox();
    void savePresetDialog();

    BankMasterProcessor& proc;
    BMLookAndFeel lnf;

    // header
    juce::ComboBox presetBox;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" }, savePreset { "SAVE" };
    juce::TextButton abA { "A" }, abB { "B" }, abCopy { "COPY" }, undo { "UNDO" }, redo { "REDO" };
    Knob oneMaster;
    Toggle oneOn;

    // tabs
    juce::TextButton tabs[4];
    std::unique_ptr<Toggle> power[4];
    std::unique_ptr<Page> pages[4];
    int currentTab = 0;

    MeterBar meters;
    juce::String lastPresetName;
    int lastPresetCount = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BankMasterEditor)
};
