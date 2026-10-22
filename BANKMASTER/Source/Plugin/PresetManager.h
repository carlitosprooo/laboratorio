// BANKMASTER BY KALOX — factory + user presets
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include <map>

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

    /** Plain parameter values (id -> value) that loading the given preset writes; empty if unknown. */
    std::map<juce::String, float> getPresetValues (const juce::String& name) const;
    /** True when any parameter differs from the values of the current (last loaded) preset. */
    bool isModified() const;
    /** Reloads the current preset (the caller wraps it in an undo transaction). */
    bool revertToCurrentPreset();

    static juce::File getUserPresetDirectory();
    /** Overrides the preset folder (used by the tests). */
    static void setUserPresetDirectoryOverride (const juce::File& dir);

private:
    juce::Array<juce::File> getUserPresetFiles() const;
    juce::AudioProcessorValueTreeState& state;
    mutable juce::String cachedName;
    mutable std::map<juce::String, float> cachedValues;
};
