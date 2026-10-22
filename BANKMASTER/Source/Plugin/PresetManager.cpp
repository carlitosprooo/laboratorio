// BANKMASTER by KALOX — factory + user presets
#include "PresetManager.h"

namespace
{
    juce::File& dirOverride() { static juce::File f; return f; }
}

void PresetManager::setUserPresetDirectoryOverride (const juce::File& dir) { dirOverride() = dir; }

juce::File PresetManager::getUserPresetDirectory()
{
    if (dirOverride() != juce::File()) return dirOverride();
    // macOS: ~/Library/Application Support/KALOX/BANKMASTER/Presets
    // Windows: %APPDATA%\KALOX\BANKMASTER\Presets ; Linux: ~/.config/KALOX/BANKMASTER/Presets
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("KALOX").getChildFile ("BANKMASTER").getChildFile ("Presets");
}

juce::Array<juce::File> PresetManager::getUserPresetFiles() const
{
    auto files = getUserPresetDirectory().findChildFiles (juce::File::findFiles, false, juce::String ("*") + kExtension);
    files.sort();
    return files;
}

juce::StringArray PresetManager::getAllPresetNames() const
{
    juce::StringArray names;
    for (const auto& p : bm::getFactoryPresets()) names.add (p.name);
    for (const auto& f : getUserPresetFiles()) names.add (f.getFileNameWithoutExtension());
    return names;
}

bool PresetManager::loadPreset (int index)
{
    const auto& factory = bm::getFactoryPresets();
    if (index >= 0 && index < (int) factory.size())
    {
        bmp::writeChainToState (state, factory[(size_t) index].params);
        // module switches and master bypass return to their preset state too
        if (auto* b = state.getParameter (bmp::id::bypass)) b->setValueNotifyingHost (0.0f);
        setCurrentPresetName (factory[(size_t) index].name);
        return true;
    }
    const auto files = getUserPresetFiles();
    const int u = index - (int) factory.size();
    if (u < 0 || u >= files.size()) return false;

    auto xml = juce::XmlDocument::parse (files[u]);
    if (xml == nullptr || ! xml->hasTagName (state.state.getType())) return false;
    auto tree = juce::ValueTree::fromXml (*xml);
    tree.setProperty (kPresetNameProperty, files[u].getFileNameWithoutExtension(), nullptr);
    bmp::replaceStateAndSync (state, tree);
    return true;
}

bool PresetManager::loadPresetByName (const juce::String& name)
{
    const int i = getAllPresetNames().indexOf (name);
    return i >= 0 && loadPreset (i);
}

bool PresetManager::saveUserPreset (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) return false;
    for (const auto& p : bm::getFactoryPresets())
        if (juce::String (p.name) == name) return false; // factory names are read-only

    auto dir = getUserPresetDirectory();
    if (! dir.createDirectory()) return false;
    auto tree = state.copyState();
    tree.setProperty (kPresetNameProperty, name, nullptr);
    auto xml = tree.createXml();
    if (xml == nullptr) return false;
    const bool ok = xml->writeTo (dir.getChildFile (name + kExtension));
    if (ok) setCurrentPresetName (name);
    return ok;
}

bool PresetManager::deleteUserPreset (const juce::String& name)
{
    auto f = getUserPresetDirectory().getChildFile (name + kExtension);
    return f.existsAsFile() && f.deleteFile();
}

juce::String PresetManager::getCurrentPresetName() const
{
    return state.state.getProperty (kPresetNameProperty, "KALOX MASTER - ORIGINAL").toString();
}

int PresetManager::getCurrentPresetIndex() const
{
    return getAllPresetNames().indexOf (getCurrentPresetName());
}

void PresetManager::setCurrentPresetName (const juce::String& n)
{
    state.state.setProperty (kPresetNameProperty, n, nullptr);
}
