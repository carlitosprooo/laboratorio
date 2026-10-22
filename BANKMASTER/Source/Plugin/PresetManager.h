// BANKMASTER by KALOX — factory + user presets
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"

class PresetManager
{
public:
    static constexpr const char* kExtension = ".bmpreset";
    static constexpr const char* kPresetNameProperty = "presetName";

    explicit PresetManager (juce::AudioProcessorValueTreeState& s) : state (s) {}

    /** Factory presets followed by user presets found on disk. */
    juce::StringArray getAllPresetNames() const;
    int getNumFactoryPresets() const { return (int) bm::getFactoryPresets().size(); }

    bool loadPreset (int index);                 // index into getAllPresetNames()
    bool loadPresetByName (const juce::String& name);
    bool saveUserPreset (const juce::String& name);
    bool deleteUserPreset (const juce::String& name);

    juce::String getCurrentPresetName() const;
    int getCurrentPresetIndex() const;
    void setCurrentPresetName (const juce::String& n);

    static juce::File getUserPresetDirectory();
    /** Overrides the preset folder (used by the tests). */
    static void setUserPresetDirectoryOverride (const juce::File& dir);

private:
    juce::Array<juce::File> getUserPresetFiles() const;
    juce::AudioProcessorValueTreeState& state;
};
