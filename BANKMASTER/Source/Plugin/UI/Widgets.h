// BANKMASTER by KALOX — reusable controls bound to parameters
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

using APVTS = juce::AudioProcessorValueTreeState;

/** Circular knob + caption + value read-out, attached to one parameter. */
class Knob : public juce::Component
{
public:
    Knob (APVTS& s, const juce::String& paramId, const juce::String& caption, juce::Colour accent = theme::iceDeep)
        : name (caption)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 16);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        slider.setPopupDisplayEnabled (false, false, nullptr);
        addAndMakeVisible (slider);
        attachment = std::make_unique<APVTS::SliderAttachment> (s, paramId, slider);
        if (auto* p = s.getParameter (paramId))
        {
            slider.textFromValueFunction = [p] (double v) { return p->getText (p->convertTo0to1 ((float) v), 32); };
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        }
        slider.updateText();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::textDim);
        g.setFont (theme::font (11.5f, true));
        g.drawText (name.toUpperCase(), getLocalBounds().removeFromTop (16), juce::Justification::centred);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop (16);
        slider.setBounds (r);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, r.getWidth(), 16);
    }
    juce::Slider slider;

private:
    juce::String name;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

/** Toggle button attached to a bool parameter. */
class Toggle : public juce::TextButton
{
public:
    Toggle (APVTS& s, const juce::String& paramId, const juce::String& text, juce::Colour onColour = theme::iceDeep)
        : juce::TextButton (text)
    {
        setClickingTogglesState (true);
        setColour (juce::TextButton::buttonOnColourId, onColour);
        attachment = std::make_unique<APVTS::ButtonAttachment> (s, paramId, *this);
    }
private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

/** Combo box + caption attached to a choice parameter. */
class Choice : public juce::Component
{
public:
    Choice (APVTS& s, const juce::String& paramId, const juce::String& caption)
        : name (caption)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (paramId)))
            box.addItemList (p->choices, 1);
        addAndMakeVisible (box);
        attachment = std::make_unique<APVTS::ComboBoxAttachment> (s, paramId, box);
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::textDim);
        g.setFont (theme::font (11.5f, true));
        g.drawText (name.toUpperCase(), getLocalBounds().removeFromTop (16), juce::Justification::centredLeft);
    }
    void resized() override { auto r = getLocalBounds(); r.removeFromTop (16); box.setBounds (r.removeFromTop (26)); }
    juce::ComboBox box;
private:
    juce::String name;
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

/** Lays knobs out in a grid inside a rectangle. */
inline void layoutRow (juce::Rectangle<int> r, std::initializer_list<juce::Component*> items, int gap = 6, int maxHeight = 150)
{
    const int n = (int) items.size();
    if (n == 0) return;
    const int w = (r.getWidth() - gap * (n - 1)) / n;
    const int h = juce::jmin (r.getHeight(), maxHeight, w + 36);
    r = r.withSizeKeepingCentre (r.getWidth(), h);
    for (auto* c : items) { c->setBounds (r.removeFromLeft (w)); r.removeFromLeft (gap); }
}
