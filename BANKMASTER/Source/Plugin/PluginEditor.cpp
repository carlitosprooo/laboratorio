// BANKMASTER — BY KALOX · V2 editor
#include "PluginEditor.h"

using namespace bmui;

BankMasterEditor::BankMasterEditor (BankMasterProcessor& p)
    : AudioProcessorEditor (p), proc (p), header (p), nav (p), globalBar (p)
{
    setLookAndFeel (&lnf);
    tooltips.setLookAndFeel (&lnf);
    setOpaque (true);

    addAndMakeVisible (content);
    content.setSize (kDesignWidth, kDesignHeight);
    content.addAndMakeVisible (header);
    content.addAndMakeVisible (nav);
    content.addAndMakeVisible (globalBar);

    auto eq = std::make_unique<EqPage> (proc);
    eqPage = eq.get();
    pages[0] = std::move (eq);
    pages[1] = std::make_unique<SatPage> (proc);
    pages[2] = std::make_unique<CompPage> (proc);
    pages[3] = std::make_unique<LimPage> (proc);
    for (auto& pg : pages) content.addChildComponent (*pg);

    content.addChildComponent (shield);
    content.addChildComponent (revertPopover);
    content.addChildComponent (savePopover);
    shield.onClickOutside = [this] { cancelPopover(); };
    revertPopover.onCancel = [this] { cancelPopover(); };
    revertPopover.cancel.onClick = [this] { cancelPopover(); };
    revertPopover.confirm.onClick = [this] { confirmRevert(); };
    savePopover.onCancel = [this] { cancelPopover(); };
    savePopover.cancel.onClick = [this] { cancelPopover(); };
    savePopover.confirm.onClick = [this] { commitSave(); };
    savePopover.nameEditor.onReturnKey = [this] { commitSave(); };
    savePopover.nameEditor.onEscapeKey = [this] { cancelPopover(); };

    // header actions
    header.prev.onClick = [this]
    {
        const int n = proc.getPresets().getAllPresetNames().size();
        loadPresetIndex ((proc.getPresets().getCurrentPresetIndex() - 1 + n) % n);
    };
    header.next.onClick = [this]
    {
        const int n = proc.getPresets().getAllPresetNames().size();
        loadPresetIndex ((proc.getPresets().getCurrentPresetIndex() + 1) % n);
    };
    header.presetName.onClick = [this] { showPresetMenu(); };
    header.save.onClick = [this] { showSavePopover(); };
    header.revert.onClick = [this] { requestRevert(); };
    header.ab.onChange = [this] (int slot) { proc.beginUndoTransaction ("A/B"); proc.selectAbSlot (slot); };
    header.copy.onClick = [this] { proc.copyAbToOther(); };
    header.undoRedo.undoButton.onClick = [this] { doUndo(); };
    header.undoRedo.redoButton.onClick = [this] { doRedo(); };
    header.menu.onClick = [this] { showMainMenu(); };
    nav.onSelect = [this] (int m) { showTab (m); };

    addMouseListener (this, true);   // every click starts a new undo step
    setWantsKeyboardFocus (true);

    // proportional resizing, the size is remembered with the session
    auto& ui = proc.getUiState();
    const int w = juce::jlimit (kMinWidth, kMaxWidth, (int) ui.getProperty ("width", kDesignWidth));
    const int h = juce::jlimit (kMinHeight, kMaxHeight, (int) ui.getProperty ("height", kDesignHeight));
    setResizable (true, true);
    setResizeLimits (kMinWidth, kMinHeight, kMaxWidth, kMaxHeight);   // may resize (and store) before setSize
    setSize (w, h);

    showTab (juce::jlimit (0, 3, (int) ui.getProperty ("page", 0)));
    header.tick();
    startTimerHz (50);
}

BankMasterEditor::~BankMasterEditor()
{
    stopTimer();
    removeMouseListener (this);
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void BankMasterEditor::paint (juce::Graphics& g)
{
    const auto& pal = P();
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (pal.bg0, b.getCentreX(), b.getY(), pal.bg1, b.getCentreX(), b.getBottom(), false));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (pal.accent.withAlpha (0.035f), b.getCentreX(), b.getY() - 200 * uiScale,
                                             pal.accent.withAlpha (0.0f), b.getCentreX(), b.getY() + 260 * uiScale, true));
    g.fillRect (b);
    // header / global-bar bands run across the full window width (seamless at any aspect ratio)
    const auto c = content.getBoundsInParent().toFloat();
    const float headerBottom = c.getY() + 64.0f * uiScale, barTop = c.getY() + (kDesignHeight - 62) * uiScale;
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.fillRect (0.0f, 0.0f, b.getWidth(), headerBottom);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRect (0.0f, barTop, b.getWidth(), b.getBottom() - barTop);
    g.setColour (pal.edge);
    g.fillRect (0.0f, headerBottom - uiScale, b.getWidth(), juce::jmax (1.0f, uiScale));
    g.fillRect (0.0f, barTop, b.getWidth(), juce::jmax (1.0f, uiScale));
}

void BankMasterEditor::resized()
{
    const float sx = (float) getWidth() / kDesignWidth, sy = (float) getHeight() / kDesignHeight;
    uiScale = juce::jmin (sx, sy);
    const float ox = ((float) getWidth() - kDesignWidth * uiScale) * 0.5f;
    const float oy = ((float) getHeight() - kDesignHeight * uiScale) * 0.5f;
    content.setTransform (juce::AffineTransform::scale (uiScale).translated (ox, oy));

    auto r = juce::Rectangle<int> (0, 0, kDesignWidth, kDesignHeight);
    header.setBounds (r.removeFromTop (64));
    nav.setBounds (r.removeFromTop (62));
    globalBar.setBounds (r.removeFromBottom (62));
    auto main = r.reduced (16, 8).withTrimmedBottom (4);
    for (auto& pg : pages) pg->setBounds (main);
    shield.setBounds (content.getLocalBounds());

    auto& ui = proc.getUiState();
    ui.setProperty ("width", getWidth(), nullptr);
    ui.setProperty ("height", getHeight(), nullptr);
    repaint();
}

void BankMasterEditor::mouseDown (const juce::MouseEvent&)
{
    proc.beginUndoTransaction();
}

bool BankMasterEditor::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    if (mods.isCommandDown() && (k.getKeyCode() == 'Z' || k.getKeyCode() == 'z'))
    {
        if (mods.isShiftDown()) doRedo(); else doUndo();
        return true;
    }
    if (mods.isCommandDown() && (k.getKeyCode() == 'Y' || k.getKeyCode() == 'y')) { doRedo(); return true; }
    if (k == juce::KeyPress::escapeKey && (revertPopover.isVisible() || savePopover.isVisible())) { cancelPopover(); return true; }
    return false;
}

void BankMasterEditor::showTab (int index)
{
    currentTab = juce::jlimit (0, 3, index);
    for (int i = 0; i < 4; ++i) pages[(size_t) i]->setVisible (i == currentTab);
    nav.setActive (currentTab);
    proc.getUiState().setProperty ("page", currentTab, nullptr);
}

void BankMasterEditor::timerCallback()
{
    pages[(size_t) currentTab]->tick();
    globalBar.tick();
    nav.tick();
    header.tick();
}

// ---------------------------------------------------------------------------- undo / revert
void BankMasterEditor::doUndo()
{
    if (proc.getUndoManager().canUndo()) proc.undo();
    header.tick();
}

void BankMasterEditor::doRedo()
{
    if (proc.getUndoManager().canRedo()) proc.redo();
    header.tick();
}

void BankMasterEditor::requestRevert()
{
    if (! proc.getPresets().isModified()) return;
    revertPopover.presetName = proc.getPresets().getCurrentPresetName();
    shield.setVisible (true);
    shield.toFront (false);
    revertPopover.showUnder (header.revert.getBounds().translated (header.getX(), header.getY()), 380, 150);
}

void BankMasterEditor::confirmRevert()
{
    cancelPopover();
    proc.beginUndoTransaction ("Revert to preset");
    proc.getPresets().revertToCurrentPreset();
    proc.beginUndoTransaction();   // closes the step: the revert is undone in one go
    header.tick();
}

void BankMasterEditor::cancelPopover()
{
    revertPopover.setVisible (false);
    savePopover.setVisible (false);
    savePopover.message.clear();
    shield.setVisible (false);
}

// ---------------------------------------------------------------------------- presets
void BankMasterEditor::loadPresetIndex (int index)
{
    proc.beginUndoTransaction ("Load preset");
    proc.getPresets().loadPreset (index);
    proc.beginUndoTransaction();
    header.tick();
}

void BankMasterEditor::showPresetMenu()
{
    auto& pm = proc.getPresets();
    const auto names = pm.getAllPresetNames();
    const auto current = pm.getCurrentPresetName();
    juce::PopupMenu m;
    m.addSectionHeader ("FACTORY");
    for (int i = 0; i < names.size(); ++i)
    {
        if (i == pm.getNumFactoryPresets()) { m.addSeparator(); m.addSectionHeader ("USER"); }
        m.addItem (i + 1, names[i].replace (" - ", juce::String (juce::CharPointer_UTF8 (" \xe2\x80\x94 "))), true, names[i] == current);
    }
    m.addSeparator();
    m.addItem (10001, juce::String (juce::CharPointer_UTF8 ("Save as\xe2\x80\xa6")));
    const bool isUser = names.indexOf (current) >= pm.getNumFactoryPresets();
    m.addItem (10002, "Delete \"" + current + "\"", isUser);
    m.addItem (10003, "Show presets folder");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&header.presetName).withMinimumWidth (header.presetName.getWidth()),
                     [this, names, current] (int r)
    {
        if (r <= 0) return;
        if (r <= names.size()) { loadPresetIndex (r - 1); return; }
        if (r == 10001) showSavePopover();
        if (r == 10002) proc.getPresets().deleteUserPreset (current);
        if (r == 10003)
        {
            auto dir = PresetManager::getUserPresetDirectory();
            dir.createDirectory();
            dir.revealToUser();
        }
    });
}

void BankMasterEditor::showSavePopover()
{
    auto name = proc.getPresets().getCurrentPresetName();
    if (proc.getPresets().getAllPresetNames().indexOf (name) < proc.getPresets().getNumFactoryPresets()) name << " (user)";
    savePopover.nameEditor.setText (name, false);
    savePopover.nameEditor.selectAll();
    savePopover.message.clear();
    shield.setVisible (true);
    shield.toFront (false);
    savePopover.showUnder (header.save.getBounds().translated (header.getX(), header.getY()), 360, 150);
    savePopover.nameEditor.grabKeyboardFocus();
}

void BankMasterEditor::commitSave()
{
    const auto name = savePopover.nameEditor.getText().trim();
    if (proc.getPresets().saveUserPreset (name)) { cancelPopover(); header.tick(); return; }
    savePopover.message = "Could not save \"" + name + "\" (empty or a factory name).";
    savePopover.repaint();
}

void BankMasterEditor::showMainMenu()
{
    juce::PopupMenu sizes;
    const int sz[][2] = { { 1080, 700 }, { 1200, 780 }, { 1400, 910 }, { 1600, 1040 }, { 2000, 1300 } };
    for (int i = 0; i < 5; ++i)
        sizes.addItem (100 + i, juce::String (sz[i][0]) + juce::String (juce::CharPointer_UTF8 (" \xc3\x97 ")) + juce::String (sz[i][1]) + (i == 1 ? "  (default)" : ""),
                       true, getWidth() == sz[i][0] && getHeight() == sz[i][1]);
    juce::PopupMenu m;
    m.addSubMenu ("Window size", sizes);
    m.addItem (2, "Show presets folder");
    m.addSeparator();
    m.addItem (3, juce::String ("BANKMASTER ") + JucePlugin_VersionString + juce::String (juce::CharPointer_UTF8 (" \xe2\x80\x94 BY KALOX")), false);
    m.addItem (4, "Kalox Audio", false);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&header.menu), [this, sz] (int r)
    {
        if (r >= 100 && r < 105) setSize (sz[r - 100][0], sz[r - 100][1]);
        if (r == 2)
        {
            auto dir = PresetManager::getUserPresetDirectory();
            dir.createDirectory();
            dir.revealToUser();
        }
    });
}

// ---------------------------------------------------------------------------- tests
juce::Array<juce::Component*> BankMasterEditor::getAllBoundControls()
{
    juce::Array<juce::Component*> all;
    for (auto& pg : pages) all.addArray (pg->getBoundControls());
    all.add (&header.oneOn, &header.oneAmt, &nav.inGain, &nav.outGain, &globalBar.target, &globalBar.bypass);
    for (auto& n : nav.nodes) all.add (&n->power);
    return all;
}
