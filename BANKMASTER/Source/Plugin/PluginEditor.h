// BANKMASTER — BY KALOX · V2 editor (one code base for macOS and Windows)
#pragma once

#include "PluginProcessor.h"
#include "UI/Chrome.h"

class BankMasterEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    /** The layout is designed at 1200 x 780 and scaled proportionally to the window. */
    static constexpr int kDesignWidth = 1200, kDesignHeight = 780;
    static constexpr int kMinWidth = 1080, kMinHeight = 700, kMaxWidth = 2000, kMaxHeight = 1300;

    explicit BankMasterEditor (BankMasterProcessor&);
    ~BankMasterEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    void showTab (int index);
    int getCurrentTab() const noexcept { return currentTab; }
    float getUiScale() const noexcept { return uiScale; }

    // ---- actions (buttons; also used by the tests)
    void requestRevert();      // shows the confirmation popover
    void confirmRevert();      // performs the revert (one undo step)
    void cancelPopover();
    bool isRevertPopoverVisible() const { return revertPopover.isVisible(); }
    void doUndo();
    void doRedo();

    // ---- test access
    bmui::Header& getHeader() { return header; }
    bmui::GlobalBar& getGlobalBar() { return globalBar; }
    bmui::NavStrip& getNavStrip() { return nav; }
    bmui::Page& getPage (int i) { return *pages[(size_t) i]; }
    bmui::EqPage& getEqPage() { return *eqPage; }
    juce::Component& getContent() { return content; }
    /** Every control bound to a parameter, across all pages, header, navigation and global bar. */
    juce::Array<juce::Component*> getAllBoundControls();

private:
    void timerCallback() override;
    void showPresetMenu();
    void showMainMenu();
    void showSavePopover();
    void commitSave();
    void loadPresetIndex (int index);

    BankMasterProcessor& proc;
    bmui::EmbeddedFonts fonts;
    bmui::LookAndFeelV2 lnf;
    juce::TooltipWindow tooltips { this, 700 };

    juce::Component content;
    bmui::Header header;
    bmui::NavStrip nav;
    bmui::GlobalBar globalBar;
    std::unique_ptr<bmui::Page> pages[4];
    bmui::EqPage* eqPage = nullptr;
    bmui::PopoverShield shield;
    bmui::RevertPopover revertPopover;
    bmui::SavePopover savePopover;
    int currentTab = 0;
    float uiScale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BankMasterEditor)
};
