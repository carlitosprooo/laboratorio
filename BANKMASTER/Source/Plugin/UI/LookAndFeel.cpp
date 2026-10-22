// BANKMASTER by KALOX — custom LookAndFeel
#include "LookAndFeel.h"

BMLookAndFeel::BMLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, theme::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, theme::text);
    setColour (juce::ComboBox::backgroundColourId, juce::Colours::white);
    setColour (juce::ComboBox::textColourId, theme::text);
    setColour (juce::ComboBox::outlineColourId, theme::panelEdge);
    setColour (juce::ComboBox::arrowColourId, theme::iceDeep);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xfff5f9fc));
    setColour (juce::PopupMenu::textColourId, theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::ice.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, theme::text);
    setColour (juce::TextButton::buttonColourId, juce::Colours::white);
    setColour (juce::TextButton::buttonOnColourId, theme::iceDeep);
    setColour (juce::TextButton::textColourOffId, theme::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
    setColour (juce::TextEditor::textColourId, theme::text);
    setColour (juce::TextEditor::highlightColourId, theme::ice.withAlpha (0.4f));
    setColour (juce::TextEditor::outlineColourId, theme::panelEdge);
    setColour (juce::TextEditor::focusedOutlineColourId, theme::iceDeep);
    setColour (juce::AlertWindow::backgroundColourId, theme::panel);
    setColour (juce::AlertWindow::textColourId, theme::text);
    setColour (juce::CaretComponent::caretColourId, theme::iceDeep);
}

void BMLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                      float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto r = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre());
    const float cx = r.getCentreX(), cy = r.getCentreY();
    const float radius = size * 0.5f;
    const float ringW = juce::jmax (3.0f, size * 0.075f);
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool enabled = s.isEnabled();

    // track
    juce::Path track;
    track.addCentredArc (cx, cy, radius - ringW * 0.5f, radius - ringW * 0.5f, 0, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xffd5e1eb));
    g.strokePath (track, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc (bipolar sliders start from the centre)
    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float from = bipolar ? startAngle + (float) s.valueToProportionOfLength (0.0) * (endAngle - startAngle) : startAngle;
    juce::Path arc;
    arc.addCentredArc (cx, cy, radius - ringW * 0.5f, radius - ringW * 0.5f, 0, juce::jmin (from, angle), juce::jmax (from, angle), true);
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId, true);
    const auto arcCol = enabled ? (accent.isTransparent() ? theme::iceDeep : accent) : theme::steel.withAlpha (0.5f);
    g.setColour (arcCol.withAlpha (0.25f));
    g.strokePath (arc, juce::PathStrokeType (ringW + 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (arcCol);
    g.strokePath (arc, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // knob body
    const float bodyR = radius - ringW - 3.0f;
    const auto body = juce::Rectangle<float> (bodyR * 2, bodyR * 2).withCentre ({ cx, cy });
    g.setColour (theme::shadow);
    g.fillEllipse (body.translated (0, 2.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, cx, body.getY(), juce::Colour (0xffd9e5ef), cx, body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (juce::Colour (0xffb8c9d8));
    g.drawEllipse (body, 1.0f);
    // inner highlight
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.drawEllipse (body.reduced (2.0f), 1.0f);

    // pointer
    const float pLen = bodyR * 0.62f;
    juce::Path ptr;
    ptr.addRoundedRectangle (-1.6f, -bodyR + 4.0f, 3.2f, pLen, 1.6f);
    g.setColour (enabled ? theme::text : theme::steel);
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (cx, cy));
}

void BMLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                      juce::Slider::SliderStyle style, juce::Slider& s)
{
    const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const bool horiz = style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar;
    const auto track = horiz ? r.withSizeKeepingCentre (r.getWidth(), 5.0f) : r.withSizeKeepingCentre (5.0f, r.getHeight());
    g.setColour (juce::Colour (0xffd5e1eb));
    g.fillRoundedRectangle (track, 2.5f);
    g.setColour (s.isEnabled() ? theme::iceDeep : theme::steel);
    if (horiz) g.fillRoundedRectangle (track.withRight (pos), 2.5f);
    else g.fillRoundedRectangle (track.withTop (pos), 2.5f);
    const auto thumb = juce::Rectangle<float> (14, 14).withCentre (horiz ? juce::Point<float> (pos, r.getCentreY()) : juce::Point<float> (r.getCentreX(), pos));
    g.setColour (juce::Colours::white);
    g.fillEllipse (thumb);
    g.setColour (theme::iceDeep);
    g.drawEllipse (thumb, 1.5f);
}

void BMLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const float rad = juce::jmin (r.getHeight() * 0.5f, 9.0f);
    const bool on = b.getToggleState();
    if (on)
    {
        const auto accent = b.findColour (juce::TextButton::buttonOnColourId);
        g.setGradientFill (juce::ColourGradient (accent.brighter (0.25f), r.getX(), r.getY(), accent.darker (0.1f), r.getX(), r.getBottom(), false));
    }
    else
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, r.getX(), r.getY(), juce::Colour (0xffe3ebf2), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, rad);
    g.setColour (on ? b.findColour (juce::TextButton::buttonOnColourId).darker (0.2f) : theme::panelEdge);
    g.drawRoundedRectangle (r, rad, 1.0f);
    if (over || down)
    {
        g.setColour (juce::Colours::white.withAlpha (down ? 0.25f : 0.15f));
        g.fillRoundedRectangle (r, rad);
    }
}

void BMLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? b.findColour (juce::TextButton::textColourOnId) : b.findColour (juce::TextButton::textColourOffId));
    if (! b.isEnabled()) g.setColour (theme::textDim);
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 1);
}

void BMLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, 0, 0, juce::Colour (0xffe9f0f6), 0, (float) h, false));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (box.hasKeyboardFocus (true) ? theme::iceDeep : theme::panelEdge);
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.startNewSubPath (ax - 4, ay - 2); arrow.lineTo (ax, ay + 2); arrow.lineTo (ax + 4, ay - 2);
    g.setColour (theme::iceDeep);
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void BMLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

juce::Label* BMLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (theme::font (12.0f));
    l->setColour (juce::Label::textColourId, theme::text);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setJustificationType (juce::Justification::centred);
    return l;
}
