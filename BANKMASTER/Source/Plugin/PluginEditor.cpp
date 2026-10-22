// BANKMASTER by KALOX — main editor
#include "PluginEditor.h"

namespace
{
    const char* kTabNames[] = { "EQ", "SATURATION", "COMPRESSOR", "LIMITER" };
}

BankMasterEditor::BankMasterEditor (BankMasterProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      oneMaster (p.getState(), bmp::id::oneAmt, "One Master", theme::iceDeep),
      oneOn (p.getState(), bmp::id::oneOn, "ON"),
      meters (p)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("Preset");
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0 && presetBox.getText() != proc.getPresets().getCurrentPresetName())
        {
            proc.getUndoManager().beginNewTransaction ("Load preset");
            proc.getPresets().loadPreset (idx);
        }
    };
    for (auto* b : { &prevPreset, &nextPreset, &savePreset, &abA, &abB, &abCopy, &undo, &redo })
        addAndMakeVisible (b);
    prevPreset.onClick = [this] {
        const int n = proc.getPresets().getAllPresetNames().size();
        const int i = proc.getPresets().getCurrentPresetIndex();
        proc.getUndoManager().beginNewTransaction ("Load preset");
        proc.getPresets().loadPreset ((i - 1 + n) % n);
    };
    nextPreset.onClick = [this] {
        const int n = proc.getPresets().getAllPresetNames().size();
        const int i = proc.getPresets().getCurrentPresetIndex();
        proc.getUndoManager().beginNewTransaction ("Load preset");
        proc.getPresets().loadPreset ((i + 1) % n);
    };
    savePreset.onClick = [this] { savePresetDialog(); };

    abA.setClickingTogglesState (false);
    abB.setClickingTogglesState (false);
    abA.onClick = [this] { proc.selectAbSlot (0); };
    abB.onClick = [this] { proc.selectAbSlot (1); };
    abCopy.setTooltip ("Copy the current slot to the other one");
    abCopy.onClick = [this] { proc.copyAbToOther(); };
    undo.onClick = [this] { proc.getUndoManager().undo(); };
    redo.onClick = [this] { proc.getUndoManager().redo(); };

    addAndMakeVisible (oneMaster);
    addAndMakeVisible (oneOn);

    const juce::String powerIds[] = { bmp::id::eqOn, bmp::id::satOn, bmp::id::compOn, bmp::id::limOn };
    for (int i = 0; i < 4; ++i)
    {
        tabs[i].setButtonText (kTabNames[i]);
        tabs[i].setRadioGroupId (1001);
        tabs[i].setClickingTogglesState (true);
        tabs[i].onClick = [this, i] { showTab (i); };
        addAndMakeVisible (tabs[i]);
        power[i] = std::make_unique<Toggle> (proc.getState(), powerIds[i], juce::CharPointer_UTF8 ("\xe2\x8f\xbb"), theme::ok.darker (0.2f));
        power[i]->setTooltip (juce::String ("Enable / bypass ") + kTabNames[i]);
        addAndMakeVisible (*power[i]);
    }

    pages[0] = std::make_unique<EqPage> (proc);
    pages[1] = std::make_unique<SatPage> (proc);
    pages[2] = std::make_unique<CompPage> (proc);
    pages[3] = std::make_unique<LimPage> (proc);
    for (auto& pg : pages) addChildComponent (*pg);

    addAndMakeVisible (meters);
    addMouseListener (this, true); // begin undo transactions on every click

    setSize (kWidth, kHeight);
    refreshPresetBox();
    showTab (0);
    startTimerHz (30);
}

BankMasterEditor::~BankMasterEditor()
{
    stopTimer();
    removeMouseListener (this);
    setLookAndFeel (nullptr);
}

void BankMasterEditor::mouseDown (const juce::MouseEvent&)
{
    proc.getUndoManager().beginNewTransaction();
}

void BankMasterEditor::showTab (int index)
{
    currentTab = juce::jlimit (0, 3, index);
    for (int i = 0; i < 4; ++i)
    {
        pages[i]->setVisible (i == currentTab);
        tabs[i].setToggleState (i == currentTab, juce::dontSendNotification);
    }
    repaint();
}

void BankMasterEditor::refreshPresetBox()
{
    auto& pm = proc.getPresets();
    const auto names = pm.getAllPresetNames();
    presetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < names.size(); ++i)
    {
        if (i == pm.getNumFactoryPresets()) presetBox.addSeparator();
        presetBox.addItem (names[i], i + 1);
    }
    lastPresetName = pm.getCurrentPresetName();
    lastPresetCount = names.size();
    const int idx = names.indexOf (lastPresetName);
    if (idx >= 0) presetBox.setSelectedItemIndex (idx, juce::dontSendNotification);
    else presetBox.setText (lastPresetName, juce::dontSendNotification);
}

void BankMasterEditor::savePresetDialog()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name for the user preset:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.getPresets().getCurrentPresetName() + " (user)");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result)
    {
        if (result == 1)
        {
            const auto name = w->getTextEditorContents ("name");
            if (! proc.getPresets().saveUserPreset (name))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save preset",
                                                        "Could not save \"" + name + "\" (factory names are read-only).");
            refreshPresetBox();
        }
    }), true);
}

void BankMasterEditor::timerCallback()
{
    pages[currentTab]->tick();
    meters.tick();
    auto& pm = proc.getPresets();
    if (pm.getCurrentPresetName() != lastPresetName || pm.getAllPresetNames().size() != lastPresetCount)
        refreshPresetBox();
    abA.setToggleState (proc.getAbSlot() == 0, juce::dontSendNotification);
    abB.setToggleState (proc.getAbSlot() == 1, juce::dontSendNotification);
    undo.setEnabled (proc.getUndoManager().canUndo());
    redo.setEnabled (proc.getUndoManager().canRedo());
    oneMaster.setEnabled (proc.getCurrentParams().oneMasterOn);
}

void BankMasterEditor::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (theme::bgTop, 0, 0, theme::bgBottom, 0, b.getBottom(), false));
    g.fillAll();
    // subtle diagonal sheen
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.35f), 0, 0, juce::Colours::white.withAlpha (0.0f), b.getWidth() * 0.6f, b.getHeight() * 0.5f, false));
    g.fillRect (b);

    // header / logo
    auto header = getLocalBounds().removeFromTop (84).reduced (24, 10);
    g.setColour (theme::text);
    g.setFont (theme::font (34.0f, true));
    g.drawText ("BANKMASTER", header.removeFromLeft (300).withTrimmedBottom (26), juce::Justification::bottomLeft);
    auto sub = getLocalBounds().removeFromTop (84).reduced (24, 10).removeFromLeft (300).removeFromBottom (26);
    g.setColour (theme::iceDeep);
    g.setFont (theme::font (13.0f, true));
    g.drawText ("by KALOX", sub.removeFromLeft (70), juce::Justification::centredLeft);
    g.setColour (theme::textDim);
    g.setFont (theme::font (11.5f, true));
    g.drawText ("4-IN-1 MASTERING SUITE", sub, juce::Justification::centredLeft);

    // ice accent line under the header
    g.setGradientFill (juce::ColourGradient (theme::ice.withAlpha (0.0f), 0, 84, theme::ice, b.getWidth() * 0.5f, 84, false));
    g.fillRect (24.0f, 83.0f, b.getWidth() * 0.5f - 24.0f, 2.0f);
    g.setGradientFill (juce::ColourGradient (theme::ice, b.getWidth() * 0.5f, 84, theme::ice.withAlpha (0.0f), b.getWidth() - 24, 84, false));
    g.fillRect (b.getWidth() * 0.5f, 83.0f, b.getWidth() * 0.5f - 24.0f, 2.0f);

    // chain indicator next to the tabs
    g.setColour (theme::textDim);
    g.setFont (theme::font (11.0f, true));
    g.drawText ("INPUT  >  EQ  >  SATURATION  >  COMPRESSOR  >  LIMITER  >  OUTPUT",
                juce::Rectangle<int> (getWidth() - 470, 96, 446, 34), juce::Justification::centredRight);
}

void BankMasterEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (84).reduced (24, 16);
    header.removeFromLeft (310);
    // ONE MASTER (right)
    auto one = header.removeFromRight (150);
    oneMaster.setBounds (one.removeFromLeft (86).withTrimmedTop (-10).withHeight (78));
    oneOn.setBounds (one.withSizeKeepingCentre (52, 26).translated (0, 8));
    header.removeFromRight (16);
    auto hb = header.withSizeKeepingCentre (header.getWidth(), 30);
    prevPreset.setBounds (hb.removeFromLeft (30)); hb.removeFromLeft (4);
    presetBox.setBounds (hb.removeFromLeft (250)); hb.removeFromLeft (4);
    nextPreset.setBounds (hb.removeFromLeft (30)); hb.removeFromLeft (8);
    savePreset.setBounds (hb.removeFromLeft (58)); hb.removeFromLeft (18);
    abA.setBounds (hb.removeFromLeft (32)); hb.removeFromLeft (2);
    abB.setBounds (hb.removeFromLeft (32)); hb.removeFromLeft (4);
    abCopy.setBounds (hb.removeFromLeft (56)); hb.removeFromLeft (18);
    undo.setBounds (hb.removeFromLeft (58)); hb.removeFromLeft (4);
    redo.setBounds (hb.removeFromLeft (58));

    // tabs
    auto tabRow = r.removeFromTop (46).reduced (24, 6);
    for (int i = 0; i < 4; ++i)
    {
        auto t = tabRow.removeFromLeft (170);
        power[i]->setBounds (t.removeFromLeft (34).reduced (0, 1));
        t.removeFromLeft (2);
        tabs[i].setBounds (t);
        tabRow.removeFromLeft (10);
    }

    auto bottom = r.removeFromBottom (128).reduced (24, 8);
    meters.setBounds (bottom);
    auto pageArea = r.reduced (24, 6);
    for (auto& pg : pages) pg->setBounds (pageArea);
}
