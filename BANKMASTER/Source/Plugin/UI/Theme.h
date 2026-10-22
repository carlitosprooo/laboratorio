// BANKMASTER by KALOX — visual identity (ice blue / blue-grey / white)
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace theme
{
    // chassis
    inline const juce::Colour bgTop        { 0xffeef4f9 };
    inline const juce::Colour bgBottom     { 0xffcfdbe6 };
    inline const juce::Colour panel        { 0xfff8fbfd };
    inline const juce::Colour panelEdge    { 0xffc3d2df };
    inline const juce::Colour shadow       { 0x2a3b5468 };
    // text
    inline const juce::Colour text         { 0xff1d2b39 };
    inline const juce::Colour textDim      { 0xff6c7f92 };
    inline const juce::Colour textOnDark   { 0xffe9f3fb };
    // accents
    inline const juce::Colour ice          { 0xff7cc4f0 };
    inline const juce::Colour iceDeep      { 0xff3c8fd0 };
    inline const juce::Colour iceGlow      { 0x667cc4f0 };
    inline const juce::Colour steel        { 0xff8a9db0 };
    inline const juce::Colour warn         { 0xffe9a23b };
    inline const juce::Colour danger       { 0xffe2595b };
    inline const juce::Colour ok           { 0xff5cc59a };
    // displays (graphs)
    inline const juce::Colour displayTop   { 0xff1c2a38 };
    inline const juce::Colour displayBot   { 0xff121c26 };
    inline const juce::Colour grid         { 0x1fdbe9f5 };
    inline const juce::Colour gridStrong   { 0x38dbe9f5 };

    inline juce::Colour bandColour (int band)
    {
        static const juce::uint32 c[] = { 0xff8fd3ff, 0xff6fb0ff, 0xffb49cff, 0xff7fe0d0, 0xfff2b5e0 };
        return juce::Colour (c[band % 5]);
    }

    inline juce::Font font (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }

    inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius = 10.0f)
    {
        g.setColour (shadow);
        g.fillRoundedRectangle (r.translated (0, 2), radius);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, r.getX(), r.getY(), panel.darker (0.03f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);
        g.setColour (panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    }

    inline void drawDisplay (juce::Graphics& g, juce::Rectangle<float> r, float radius = 8.0f)
    {
        g.setGradientFill (juce::ColourGradient (displayTop, r.getX(), r.getY(), displayBot, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);
        g.setColour (juce::Colour (0xff0b121a));
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    }
}
