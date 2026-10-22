// BANKMASTER — BY KALOX · V2 controls bound to parameters (knobs, toggles, segmented, selectors)
#pragma once

#include "../PluginProcessor.h"
#include "DesignSystem.h"

namespace bmui
{
using namespace bmv2;
using juce::String;
using juce::Rectangle;
inline const Palette& P() { return palette(); }

/** Display text of a parameter: the parameter's own text, never "-0.00". */
String paramText (const juce::RangedAudioParameter& p);
String trimNegativeZero (String s);

/** Fonts embedded in the binary; the first instance loads them, the last one releases them. */
struct EmbeddedFonts
{
    EmbeddedFonts();
    ~EmbeddedFonts();
};

// ============================================================================ look and feel
class LookAndFeelV2 : public juce::LookAndFeel_V4
{
public:
    LookAndFeelV2();
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const String& text, const String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const String&) override;
    void drawTooltip (juce::Graphics&, const String& text, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
};

// ============================================================================ knob
/** Rotary knob with caption + value field. Drag (vertical), Shift = fine, wheel,
    double-click on the face = default value, double-click on the value = type a value. */
class ParamKnob : public juce::Slider
{
public:
    ParamKnob (const String& caption, KnobSize size, bool bipolar = false, juce::Colour accent = {});
    ~ParamKnob() override;
    void bind (juce::RangedAudioParameter* param, juce::UndoManager* um);
    void setAccent (juce::Colour c) { accent = c; repaint(); }
    void setCaption (const String& c) { caption = c; repaint(); }
    /** Draws only the knob face in the whole bounds (no caption / value field). */
    void setFaceOnly (bool f) { faceOnly = f; repaint(); }
    /** Overrides the text shown in the value field (e.g. "AUTO"); empty = parameter text. */
    void setValueOverride (const String& t) { if (t != overrideText) { overrideText = t; repaint(); } }
    juce::RangedAudioParameter* getParameter() const noexcept { return param; }
    KnobSize getKnobSize() const noexcept { return size; }

    void paint (juce::Graphics&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void enablementChanged() override { repaint(); }
    void showValueEditor();

private:
    String caption, overrideText;
    KnobSize size;
    bool bipolar, faceOnly = false;
    juce::Colour accent;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
    std::unique_ptr<juce::TextEditor> editor;
};

// ============================================================================ toggles
/** Bool parameter as a pill with LED (style Pill) or a round power button (style Power). */
class ParamToggle : public juce::Button
{
public:
    enum class Style { Pill, Power };
    ParamToggle (const String& text, Style style = Style::Pill, juce::Colour onColour = {});
    void bind (juce::RangedAudioParameter* param, juce::UndoManager* um);
    void setOnColour (juce::Colour c) { onColour = c; repaint(); }
    juce::RangedAudioParameter* getParameter() const noexcept { return param; }
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    Style style;
    juce::Colour onColour;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ButtonParameterAttachment> attachment;
};

// ============================================================================ segmented
/** Choice parameter as a segmented control (or as style cards). */
class ParamSegmented : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Look { Segments, SatCards };
    ParamSegmented (juce::StringArray labels, float fontHeight = 11.0f, Look look = Look::Segments);
    void bind (juce::RangedAudioParameter* param, juce::UndoManager* um);
    void setAccent (juce::Colour c) { accent = c; repaint(); }
    int getSelectedIndex() const noexcept { return selected; }
    juce::RangedAudioParameter* getParameter() const noexcept { return param; }
    /** Selects item i as a user gesture (also used by the tests). */
    void choose (int i);
    /** Without a parameter the control is a plain UI selector: setSelected + onChange. */
    void setSelected (int i) { if (i != selected) { selected = i; repaint(); } }
    std::function<void (int)> onChange;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
private:
    int indexAt (juce::Point<float>) const;
    Rectangle<float> cellBounds (int i) const;
    juce::StringArray labels;
    float fontH;
    Look look;
    juce::Colour accent;
    int selected = 0, hover = -1;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// ============================================================================ selector
/** Choice parameter as a drop-down. */
class ParamSelector : public juce::ComboBox
{
public:
    ParamSelector();
    void bind (juce::RangedAudioParameter* param, juce::UndoManager* um);
    juce::RangedAudioParameter* getParameter() const noexcept { return param; }
private:
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ComboBoxParameterAttachment> attachment;
};

// ============================================================================ value drag
/** A parameter edited by dragging a value read-out (INPUT / OUTPUT trims, LEARN target).
    The owner draws it through onPaint. */
class ParamDragValue : public juce::Slider
{
public:
    ParamDragValue();
    void bind (juce::RangedAudioParameter* param, juce::UndoManager* um);
    juce::RangedAudioParameter* getParameter() const noexcept { return param; }
    std::function<void (juce::Graphics&, ParamDragValue&)> onPaint;
    void paint (juce::Graphics& g) override { if (onPaint) onPaint (g, *this); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    String text() const { return param != nullptr ? paramText (*param) : String(); }
private:
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
};

// ============================================================================ plain buttons
class IconButtonV2 : public juce::Button
{
public:
    IconButtonV2 (const String& name, Icon ic) : juce::Button (name), ic (ic) {}
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        iconButton (g, getLocalBounds().toFloat(), ic, P(), isEnabled(), over);
    }
private:
    Icon ic;
};

/** Text button in the design language (popover actions). */
class TextButtonV2 : public juce::Button
{
public:
    TextButtonV2 (const String& text, bool primary = false) : juce::Button (text), primary (primary) {}
    void paintButton (juce::Graphics& g, bool over, bool down) override;
private:
    bool primary;
};

} // namespace bmui
