// BANKMASTER — BY KALOX · V2 frame: header, signal-flow navigation, global bar, popovers
#pragma once

#include "Pages.h"

namespace bmui
{
// ============================================================================ header
class UndoRedoControl : public juce::Component
{
public:
    UndoRedoControl();
    void paint (juce::Graphics&) override;
    void resized() override;
    void setState (bool canUndo, bool canRedo);
    struct Half : public juce::Button
    {
        Half (bool isUndo) : juce::Button (isUndo ? "Undo" : "Redo"), undo (isUndo) {}
        void paintButton (juce::Graphics&, bool over, bool down) override;
        bool undo;
    };
    Half undoButton { true }, redoButton { false };
};

class PresetName : public juce::Button
{
public:
    PresetName() : juce::Button ("Preset") {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    String name, kind = "FACTORY";
    bool modified = false;
};

class Header : public juce::Component
{
public:
    explicit Header (BankMasterProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();

    IconButtonV2 prev { "Previous preset", Icon::Prev }, next { "Next preset", Icon::Next };
    IconButtonV2 save { "Save preset", Icon::Save }, revert { "Revert to preset", Icon::Revert };
    IconButtonV2 copy { "Copy A/B", Icon::Copy }, menu { "Menu", Icon::Menu };
    PresetName presetName;
    ParamSegmented ab { { "A", "B" }, 12.0f };
    UndoRedoControl undoRedo;
    ParamToggle oneOn { "ONE MASTER on", ParamToggle::Style::Power };
    ParamKnob oneAmt { "One master", KnobSize::Mini };
private:
    BankMasterProcessor& proc;
    Rectangle<int> oneArea;
};

// ============================================================================ navigation
class ModuleNode : public juce::Button
{
public:
    ModuleNode (BankMasterProcessor& p, int module);
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void resized() override;
    bool active = false;
    ParamToggle power { "Module on", ParamToggle::Style::Power };
private:
    BankMasterProcessor& proc;
    int module;
};

class NavStrip : public juce::Component
{
public:
    explicit NavStrip (BankMasterProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
    void setActive (int module);
    std::function<void (int)> onSelect;
    std::unique_ptr<ModuleNode> nodes[4];
    ParamDragValue inGain, outGain;
private:
    void paintIoNode (juce::Graphics&, ParamDragValue&, const String& title, bool input);
    BankMasterProcessor& proc;
    juce::Array<Rectangle<int>> arrows;
};

// ============================================================================ global bar
class GlobalBar : public juce::Component
{
public:
    explicit GlobalBar (BankMasterProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();

    struct LearnButton : public juce::Button
    {
        LearnButton() : juce::Button ("Input learn") { setClickingTogglesState (false); }
        void paintButton (juce::Graphics&, bool over, bool down) override;
        String status;
        bool learning = false;
    };
    LearnButton learn;
    ParamDragValue target;
    ParamToggle bypass { "Bypass", ParamToggle::Style::Pill };
    juce::TextButton resetLoudness;   // transparent hit area over the loudness / true-peak read-outs
private:
    struct Smooth { float v = -100.0f; void set (float d) { v = d > v ? d : v + (d - v) * 0.2f; } };
    BankMasterProcessor& proc;
    Rectangle<float> secIn, secLoud, secTp, secGr, secCorr, secOut;
    juce::Array<float> dividers;
    Smooth inL, inR, outL, outR;
    float lufsM = -INFINITY, lufsS = -INFINITY, lufsI = -INFINITY, tp = -100.0f, grComp = 0, grLim = 0, corr = 1;
    juce::uint32 statusUntil = 0;
};

// ============================================================================ popovers
/** Dims nothing, but catches clicks outside a popover (= cancel). */
class PopoverShield : public juce::Component
{
public:
    std::function<void()> onClickOutside;
    void mouseDown (const juce::MouseEvent&) override { if (onClickOutside) onClickOutside(); }
};

class Popover : public juce::Component
{
public:
    Popover();
    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;
    /** Places the popover under anchor (content coordinates) with its arrow pointing at it. */
    void showUnder (Rectangle<int> anchor, int width, int height);
    std::function<void()> onCancel;
protected:
    float arrowX = 0.0f;
    static constexpr int kArrow = 8;
    Rectangle<int> body() const { return getLocalBounds().withTrimmedTop (kArrow); }
};

class RevertPopover : public Popover
{
public:
    RevertPopover();
    void paint (juce::Graphics&) override;
    void resized() override;
    String presetName;
    TextButtonV2 cancel { "Cancel" }, confirm { "Revert", true };
};

class SavePopover : public Popover
{
public:
    SavePopover();
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::TextEditor nameEditor;
    TextButtonV2 cancel { "Cancel" }, confirm { "Save", true };
    String message;
};

} // namespace bmui
