// BANKMASTER — BY KALOX · V2 controls
#include "Controls.h"
#include "BinaryData.h"

namespace bmui
{
// ============================================================================ text
String trimNegativeZero (String s)
{
    if (s.startsWithChar ('-'))
    {
        // "-0.00 dB" -> "0.00 dB": only the leading number matters
        const auto numPart = s.substring (1).initialSectionContainingOnly ("0123456789.");
        if (numPart.isNotEmpty() && numPart.removeCharacters ("0.").isEmpty()) s = s.substring (1);
    }
    return s;
}

String paramText (const juce::RangedAudioParameter& p)
{
    return trimNegativeZero (p.getCurrentValueAsText());
}

// ============================================================================ fonts
namespace { int fontUsers = 0; }

EmbeddedFonts::EmbeddedFonts()
{
    if (fontUsers++ > 0) return;
    auto& f = Fonts::get();
    f.setFromMemory (Fonts::Regular, BinaryData::InterRegular_otf, BinaryData::InterRegular_otfSize);
    f.setFromMemory (Fonts::Medium, BinaryData::InterMedium_otf, BinaryData::InterMedium_otfSize);
    f.setFromMemory (Fonts::SemiBold, BinaryData::InterSemiBold_otf, BinaryData::InterSemiBold_otfSize);
    f.setFromMemory (Fonts::Bold, BinaryData::InterBold_otf, BinaryData::InterBold_otfSize);
    f.setFromMemory (Fonts::Display, BinaryData::MontserratExtraBold_ttf, BinaryData::MontserratExtraBold_ttfSize);
    f.setFromMemory (Fonts::DisplaySemi, BinaryData::MontserratSemiBold_ttf, BinaryData::MontserratSemiBold_ttfSize);
}

EmbeddedFonts::~EmbeddedFonts()
{
    if (--fontUsers == 0) Fonts::get().clear();
}

// ============================================================================ look and feel
LookAndFeelV2::LookAndFeelV2()
{
    const auto& p = P();
    setColour (juce::PopupMenu::backgroundColourId, p.panelHi);
    setColour (juce::PopupMenu::textColourId, p.text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, p.accentSoft);
    setColour (juce::PopupMenu::highlightedTextColourId, p.text);
    setColour (juce::ComboBox::textColourId, p.text);
    setColour (juce::ComboBox::backgroundColourId, p.well);
    setColour (juce::ComboBox::outlineColourId, p.edge);
    setColour (juce::TextEditor::textColourId, p.text);
    setColour (juce::TextEditor::backgroundColourId, p.well);
    setColour (juce::TextEditor::highlightColourId, p.accent.withAlpha (0.35f));
    setColour (juce::TextEditor::highlightedTextColourId, p.text);
    setColour (juce::CaretComponent::caretColourId, p.accent);
    setColour (juce::TextEditor::outlineColourId, p.edge);
    setColour (juce::TextEditor::focusedOutlineColourId, p.accent);
    setColour (juce::Label::textColourId, p.text);
    setColour (juce::TooltipWindow::backgroundColourId, p.panelHi);
    setColour (juce::TooltipWindow::textColourId, p.text);
}

juce::Typeface::Ptr LookAndFeelV2::getTypefaceForFont (const juce::Font& f)
{
    auto& fonts = Fonts::get();
    if (fonts.regular != nullptr && f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return f.isBold() ? fonts.semibold : fonts.regular;
    return LookAndFeel_V4::getTypefaceForFont (f);
}

void LookAndFeelV2::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    selectorBox (g, Rectangle<float> (0, 0, (float) w, (float) h), {}, P(), box.isMouseOver (true), box.isEnabled());
}

void LookAndFeelV2::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font LookAndFeelV2::getComboBoxFont (juce::ComboBox&) { return font (type::body, Fonts::Medium); }
juce::Font LookAndFeelV2::getPopupMenuFont() { return font (13.0f, Fonts::Regular); }

void LookAndFeelV2::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    const auto& p = P();
    g.fillAll (p.panelHi);
    g.setColour (p.edgeHi);
    g.drawRect (0, 0, w, h);
}

void LookAndFeelV2::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                                       bool isTicked, bool hasSubMenu, const String& text, const String& shortcutKeyText,
                                       const juce::Drawable*, const juce::Colour*)
{
    const auto& p = P();
    if (isSeparator)
    {
        g.setColour (p.edge);
        g.fillRect (area.reduced (8, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    auto r = area.toFloat().reduced (4, 1);
    if (isHighlighted && isActive)
    {
        g.setColour (p.accentSoft);
        g.fillRoundedRectangle (r, 5.0f);
    }
    if (isTicked)
    {
        g.setColour (p.accent);
        g.fillEllipse (Rectangle<float> (6, 6).withCentre ({ r.getX() + 12.0f, r.getCentreY() }));
    }
    g.setColour (isActive ? (isTicked ? p.text : p.text.withAlpha (0.88f)) : p.textMute);
    g.setFont (font (13.0f, isTicked ? Fonts::SemiBold : Fonts::Regular));
    g.drawText (text, r.withTrimmedLeft (24).withTrimmedRight (hasSubMenu ? 20 : 8), juce::Justification::centredLeft, true);
    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour (p.textMute);
        g.setFont (font (11.5f));
        g.drawText (shortcutKeyText, r.withTrimmedRight (10), juce::Justification::centredRight, false);
    }
    if (hasSubMenu)
    {
        Path ch;
        const float x = r.getRight() - 12, y = r.getCentreY();
        ch.startNewSubPath (x - 2, y - 4); ch.lineTo (x + 2, y); ch.lineTo (x - 2, y + 4);
        g.setColour (p.textDim);
        g.strokePath (ch, juce::PathStrokeType (1.4f));
    }
}

void LookAndFeelV2::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const String& name)
{
    caption (g, area.toFloat().withTrimmedLeft (14).withTrimmedTop (4), name, P(), juce::Justification::centredLeft, P().textMute);
}

void LookAndFeelV2::drawTooltip (juce::Graphics& g, const String& text, int w, int h)
{
    const auto& p = P();
    g.fillAll (p.panelHi);
    g.setColour (p.edgeHi);
    g.drawRect (0, 0, w, h);
    g.setColour (p.text);
    g.setFont (font (12.0f));
    g.drawFittedText (text, 8, 4, w - 16, h - 8, juce::Justification::centredLeft, 4, 1.0f);
}

juce::Rectangle<int> LookAndFeelV2::getTooltipBounds (const String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    juce::GlyphArrangement ga;
    ga.addFittedText (font (12.0f), tipText, 0, 0, 320, 200, juce::Justification::topLeft, 4, 1.0f);
    const auto b = ga.getBoundingBox (0, -1, true);
    const int w = (int) b.getWidth() + 18, h = (int) b.getHeight() + 10;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin (parentArea);
}

void LookAndFeelV2::drawCornerResizer (juce::Graphics& g, int w, int h, bool over, bool dragging)
{
    icon (g, Rectangle<float> ((float) w, (float) h).reduced (2), Icon::Resize, over || dragging ? P().textDim : P().textMute, 1.3f);
}

void LookAndFeelV2::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (P().well);
    g.fillRoundedRectangle (Rectangle<float> ((float) w, (float) h), 5.0f);
}

void LookAndFeelV2::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& te)
{
    g.setColour (te.hasKeyboardFocus (true) ? P().accent : P().edge);
    g.drawRoundedRectangle (Rectangle<float> ((float) w, (float) h).reduced (0.5f), 5.0f, 1.0f);
}

// ============================================================================ ParamKnob
ParamKnob::ParamKnob (const String& cap, KnobSize sz, bool bip, juce::Colour acc)
    : caption (cap), size (sz), bipolar (bip), accent (acc)
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setRotaryParameters (knobStartAngle(), knobEndAngle(), true);
    setMouseDragSensitivity (sz == KnobSize::Hero ? 320 : 240);
    setVelocityModeParameters (0.25, 1, 0.0, true, juce::ModifierKeys::shiftModifier); // Shift = fine
    setScrollWheelEnabled (true);
    setPopupMenuEnabled (false);
    setName (cap);
}

ParamKnob::~ParamKnob() { attachment.reset(); }

void ParamKnob::bind (juce::RangedAudioParameter* p, juce::UndoManager* um)
{
    attachment.reset();
    param = p;
    if (p == nullptr) return;
    setComponentID (p->getParameterID());
    attachment = std::make_unique<juce::SliderParameterAttachment> (*p, *this, um);
    setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    // the attachment maps the slider range through the parameter's own (skewed) range
    repaint();
}

void ParamKnob::paint (juce::Graphics& g)
{
    KnobSpec k;
    k.size = size;
    k.bipolar = bipolar;
    k.accent = accent;
    k.label = caption;
    k.enabled = isEnabled();
    k.hover = isMouseOverOrDragging();
    k.active = isMouseButtonDown();
    k.value = param != nullptr ? param->getValue() : (float) valueToProportionOfLength (getValue());
    k.valueText = overrideText.isNotEmpty() ? overrideText : (param != nullptr ? paramText (*param) : String());
    if (faceOnly) knobFace (g, getLocalBounds().toFloat().reduced (1), k, P());
    else knob (g, getLocalBounds().toFloat(), k, P());
}

void ParamKnob::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! faceOnly && knobValueRect (getLocalBounds().toFloat(), size).expanded (4).contains (e.position)) { showValueEditor(); return; }
    Slider::mouseDoubleClick (e);
}

void ParamKnob::showValueEditor()
{
    if (param == nullptr) return;
    editor = std::make_unique<juce::TextEditor>();
    editor->setJustification (juce::Justification::centred);
    editor->setFont (font (type::value, Fonts::Medium));
    editor->setText (paramText (*param).upToFirstOccurrenceOf (" ", false, false), false);
    editor->selectAll();
    editor->setBounds (knobValueRect (getLocalBounds().toFloat(), size).toNearestInt());
    auto commit = [this]
    {
        if (editor == nullptr || param == nullptr) return;
        const float v = param->getValueForText (editor->getText());
        param->beginChangeGesture();
        param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
        param->endChangeGesture();
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<ParamKnob> (this)] { if (sp != nullptr) sp->editor.reset(); });
    };
    editor->onReturnKey = commit;
    editor->onFocusLost = commit;
    editor->onEscapeKey = [this] { juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<ParamKnob> (this)] { if (sp != nullptr) sp->editor.reset(); }); };
    addAndMakeVisible (*editor);
    editor->grabKeyboardFocus();
}

// ============================================================================ ParamToggle
ParamToggle::ParamToggle (const String& text, Style s, juce::Colour on) : juce::Button (text), style (s), onColour (on)
{
    setClickingTogglesState (true);
}

void ParamToggle::bind (juce::RangedAudioParameter* p, juce::UndoManager* um)
{
    attachment.reset();
    param = p;
    if (p == nullptr) return;
    setComponentID (p->getParameterID());
    attachment = std::make_unique<juce::ButtonParameterAttachment> (*p, *this, um);
}

void ParamToggle::paintButton (juce::Graphics& g, bool over, bool)
{
    const auto& p = P();
    const bool on = getToggleState();
    auto r = getLocalBounds().toFloat();
    if (style == Style::Pill)
    {
        pillToggle (g, r, getButtonText(), on, p, true, over && isEnabled(), onColour);
        if (! isEnabled()) { g.setColour (p.bg1.withAlpha (0.55f)); g.fillRoundedRectangle (r, r.getHeight() * 0.5f); }
        return;
    }
    const auto col = onColour.isTransparent() ? p.accent : onColour;
    auto led = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), juce::jmin (r.getWidth(), r.getHeight())).reduced (1);
    g.setColour (over ? p.panelHi : p.well);
    g.fillEllipse (led);
    g.setColour (on ? col.withAlpha (0.75f) : p.edgeHi);
    g.drawEllipse (led.reduced (0.5f), 1.0f);
    if (on) { g.setColour (col.withAlpha (0.14f)); g.fillEllipse (led); }
    icon (g, led.reduced (led.getWidth() * 0.26f), Icon::Power, on ? col : p.textMute, 1.5f);
}

// ============================================================================ ParamSegmented
ParamSegmented::ParamSegmented (juce::StringArray l, float fh, Look lk) : labels (std::move (l)), fontH (fh), look (lk) {}

void ParamSegmented::bind (juce::RangedAudioParameter* p, juce::UndoManager* um)
{
    attachment.reset();
    param = p;
    if (p == nullptr) return;
    setComponentID (p->getParameterID());
    attachment = std::make_unique<juce::ParameterAttachment> (*p, [this] (float v)
    {
        const int i = juce::roundToInt (v);
        if (i != selected) { selected = i; repaint(); }
    }, um);
    attachment->sendInitialUpdate();
}

void ParamSegmented::choose (int i)
{
    if (i < 0 || i >= labels.size()) return;
    if (attachment != nullptr) attachment->setValueAsCompleteGesture ((float) i);
    else { setSelected (i); if (onChange) onChange (i); }
}

Rectangle<float> ParamSegmented::cellBounds (int i) const
{
    auto r = getLocalBounds().toFloat();
    if (look == Look::SatCards)
    {
        const float gap = 10.0f, w = (r.getWidth() - gap) / 2.0f, h = (r.getHeight() - gap) / 2.0f;
        return { r.getX() + (i % 2) * (w + gap), r.getY() + (i / 2) * (h + gap), w, h };
    }
    const float w = r.getWidth() / (float) juce::jmax (1, labels.size());
    return { r.getX() + i * w, r.getY(), w, r.getHeight() };
}

int ParamSegmented::indexAt (juce::Point<float> pt) const
{
    for (int i = 0; i < labels.size(); ++i)
        if (cellBounds (i).contains (pt)) return i;
    return -1;
}

void ParamSegmented::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled()) return;
    choose (indexAt (e.position));
}

void ParamSegmented::mouseMove (const juce::MouseEvent& e)
{
    const int h = indexAt (e.position);
    if (h != hover) { hover = h; repaint(); }
}

void ParamSegmented::paint (juce::Graphics& g)
{
    const auto& p = P();
    if (look == Look::Segments)
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (p.well);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (p.edge);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        for (int i = 0; i < labels.size(); ++i)
            segmentCell (g, cellBounds (i), labels[i], i == selected, i == hover && isEnabled(), p, fontH, accent);
        if (! isEnabled()) { g.setColour (p.bg1.withAlpha (0.55f)); g.fillRoundedRectangle (r, 6.0f); }
        return;
    }
    // saturation style cards with their real transfer curves
    const auto mc = accent.isTransparent() ? p.module[1] : accent;
    for (int i = 0; i < labels.size(); ++i)
    {
        const auto r = cellBounds (i);
        const bool sel = i == selected, hov = i == hover;
        g.setColour (sel ? p.panelHi.brighter (0.06f) : hov ? p.panelHi : p.well);
        g.fillRoundedRectangle (r, 8);
        g.setColour (sel ? mc : hov ? p.edgeHi : p.edge);
        g.drawRoundedRectangle (r.reduced (0.5f), 8, sel ? 1.4f : 1.0f);
        auto in = r.reduced (10, 7);
        auto viz = in.removeFromLeft (38.0f).withSizeKeepingCentre (38.0f, juce::jmin (38.0f, in.getHeight()));
        Path c;
        const auto st = (bm::SatStyle) i;
        for (int k = 0; k <= 24; ++k)
        {
            const float u = -1.0f + 2.0f * k / 24.0f;
            const float y = bm::Shaper::curve (st, u * 2.5f) / bm::Shaper::curve (st, 2.5f);
            const auto pt = juce::Point<float> (viz.getX() + viz.getWidth() * k / 24.0f, viz.getCentreY() - juce::jlimit (-1.2f, 1.2f, y) * viz.getHeight() * 0.42f);
            if (k == 0) c.startNewSubPath (pt); else c.lineTo (pt);
        }
        g.setColour (sel ? mc : p.textMute);
        g.strokePath (c, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        in.removeFromLeft (10);
        g.setColour (sel ? p.text : p.textDim);
        g.setFont (font (11.0f, Fonts::SemiBold, 0.05f));
        g.drawFittedText (labels[i].toUpperCase(), in.toNearestInt(), juce::Justification::centredLeft, 2, 1.0f);
    }
}

// ============================================================================ ParamSelector
ParamSelector::ParamSelector() { setJustificationType (juce::Justification::centredLeft); }

void ParamSelector::bind (juce::RangedAudioParameter* p, juce::UndoManager* um)
{
    attachment.reset();
    param = p;
    clear (juce::dontSendNotification);
    if (p == nullptr) return;
    setComponentID (p->getParameterID());
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p)) addItemList (c->choices, 1);
    attachment = std::make_unique<juce::ComboBoxParameterAttachment> (*p, *this, um);
}

// ============================================================================ ParamDragValue
ParamDragValue::ParamDragValue()
{
    setSliderStyle (juce::Slider::LinearBarVertical);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setMouseDragSensitivity (300);
    setVelocityModeParameters (0.25, 1, 0.0, true, juce::ModifierKeys::shiftModifier); // Shift = fine
    setSliderSnapsToMousePosition (false);
    setPopupMenuEnabled (false);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void ParamDragValue::bind (juce::RangedAudioParameter* p, juce::UndoManager* um)
{
    attachment.reset();
    param = p;
    if (p == nullptr) return;
    setComponentID (p->getParameterID());
    attachment = std::make_unique<juce::SliderParameterAttachment> (*p, *this, um);
    setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

// ============================================================================ TextButtonV2
void TextButtonV2::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto& p = P();
    auto r = getLocalBounds().toFloat();
    if (primary)
    {
        g.setColour (down ? p.accent.darker (0.2f) : over ? p.accent.brighter (0.1f) : p.accent);
        g.fillRoundedRectangle (r, 8);
        g.setColour (p.accentInk);
    }
    else
    {
        g.setColour (over ? p.panelHi.brighter (0.05f) : p.well);
        g.fillRoundedRectangle (r, 8);
        g.setColour (over ? p.edgeHi : p.edge);
        g.drawRoundedRectangle (r.reduced (0.5f), 8, 1);
        g.setColour (p.textDim);
    }
    g.setFont (font (11.5f, primary ? Fonts::Bold : Fonts::SemiBold, 0.08f));
    g.drawText (getButtonText().toUpperCase(), r, juce::Justification::centred, false);
}

} // namespace bmui
