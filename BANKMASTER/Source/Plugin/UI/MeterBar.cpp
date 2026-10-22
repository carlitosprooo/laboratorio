// BANKMASTER by KALOX — permanent bottom bar
#include "MeterBar.h"

namespace
{
    juce::String fmt (float v, int dec = 1)
    {
        if (! std::isfinite (v) || v < -99.0f) return "-inf";
        return (v > 0 ? "+" : "") + juce::String (v, dec);
    }
}

MeterBar::MeterBar (BankMasterProcessor& p)
    : proc (p),
      inGain (p.getState(), bmp::id::inGain, "Input"),
      outGain (p.getState(), bmp::id::outGain, "Output"),
      bypass (p.getState(), bmp::id::bypass, "MASTER BYPASS", theme::warn)
{
    addAndMakeVisible (inGain);
    addAndMakeVisible (outGain);
    addAndMakeVisible (bypass);
    addAndMakeVisible (resetLufs);
    addAndMakeVisible (resetTp);
    resetLufs.onClick = [this] { proc.getChain().getLoudness().requestReset(); };
    addAndMakeVisible (learn);
    learn.setClickingTogglesState (true);
    learn.setColour (juce::TextButton::buttonOnColourId, theme::warn);
    learn.setTooltip ("INPUT LEARN: play the song (ideally the loudest section), then press again. "
                      "INPUT gain is set so the mix reaches the target loudness before the chain.");
    learn.onClick = [this]
    {
        if (learn.getToggleState()) proc.startInputLearn();
        else
        {
            const float g = proc.stopInputLearn (true);
            learnText = std::isfinite (g) ? "INPUT set to " + juce::String (g, 1) + " dB" : juce::String ("no signal measured");
        }
    };
    target.setSliderStyle (juce::Slider::LinearBar);
    target.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
    target.setColour (juce::Slider::trackColourId, theme::ice.withAlpha (0.35f));
    target.setColour (juce::Slider::textBoxTextColourId, theme::text);
    target.setTooltip ("INPUT LEARN target loudness");
    addAndMakeVisible (target);
    targetAttachment = std::make_unique<APVTS::SliderAttachment> (proc.getState(), bmp::id::inTarget, target);
    target.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " LUFS"; };
    target.updateText();
    resetTp.onClick = [this] { proc.getChain().getOutputMeter().resetTruePeak(); };
}

void MeterBar::tick()
{
    auto& c = proc.getChain();
    auto db = [] (float g) { return juce::Decibels::gainToDecibels (g, -100.0f); };
    inL.set (db (c.getInputMeter().getPeak (0))); inR.set (db (c.getInputMeter().getPeak (1)));
    outL.set (db (c.getOutputMeter().getPeak (0))); outR.set (db (c.getOutputMeter().getPeak (1)));
    lufsM = c.getLoudness().getMomentary(); lufsS = c.getLoudness().getShortTerm(); lufsI = c.getLoudness().getIntegrated();
    tp = db (c.getOutputMeter().getTruePeakMax());
    grComp = juce::jmax (0.0f, c.getCompGainReduction()); grLim = juce::jmax (0.0f, c.getLimGainReduction());
    corr = c.getOutputMeter().getCorrelation();
    if (proc.isLearningInput())
    {
        const float m = proc.getLearnMeasuredLufs();
        learnText = "measuring: " + (std::isfinite (m) ? juce::String (m, 1) + " LUFS" : juce::String ("..."));
    }
    learn.setToggleState (proc.isLearningInput(), juce::dontSendNotification);
    repaint();
}

void MeterBar::drawLevel (juce::Graphics& g, juce::Rectangle<float> r, float db, float minDb)
{
    g.setColour (juce::Colour (0xff1a2633));
    g.fillRoundedRectangle (r, 2.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, (db - minDb) / -minDb);
    auto fill = r.withTop (r.getBottom() - r.getHeight() * frac);
    g.setGradientFill (juce::ColourGradient (theme::danger, r.getX(), r.getY(), theme::ice, r.getX(), r.getY() + r.getHeight() * 0.25f, false));
    g.fillRoundedRectangle (fill, 2.0f);
}

void MeterBar::paint (juce::Graphics& g)
{
    theme::drawPanel (g, getLocalBounds().toFloat().reduced (1.0f), 12.0f);
    const char* titles[] = { "INPUT", "OUTPUT", "LOUDNESS", "TRUE PEAK", "GAIN REDUCTION", "CORRELATION" };
    for (int i = 0; i < 6; ++i)
    {
        auto s = sections[i];
        g.setColour (theme::iceDeep);
        g.setFont (theme::font (11.0f, true));
        g.drawText (titles[i], s.removeFromTop (16), juce::Justification::centredLeft);
        if (i > 0)
        {
            g.setColour (theme::panelEdge);
            g.drawVerticalLine (sections[i].getX() - 8, (float) sections[i].getY() + 4, (float) sections[i].getBottom() - 4);
        }
    }

    // input / output L-R meters (right side of their sections)
    auto meterPair = [&] (juce::Rectangle<int> s, float l, float r)
    {
        auto m = s.withTrimmedTop (18).removeFromRight (64).toFloat();
        auto txt = m.removeFromBottom (14);
        auto bars = m.reduced (4, 2);
        drawLevel (g, bars.removeFromLeft (bars.getWidth() / 2 - 2), l);
        bars.removeFromLeft (4);
        drawLevel (g, bars, r);
        g.setColour (theme::text);
        g.setFont (theme::font (10.5f, true));
        g.drawText (fmt (juce::jmax (l, r)), txt, juce::Justification::centred);
    };
    meterPair (sections[0], inL.v, inR.v);
    if (learnText.isNotEmpty())
    {
        g.setColour (proc.isLearningInput() ? theme::warn.darker (0.3f) : theme::textDim);
        g.setFont (theme::font (10.0f));
        g.drawText (learnText, sections[0].withTrimmedTop (16).withTrimmedLeft (70).withWidth (sections[0].getWidth() - 134).removeFromBottom (14), juce::Justification::centred);
    }
    meterPair (sections[1], outL.v, outR.v);

    // loudness
    {
        auto s = sections[2].withTrimmedTop (18).withTrimmedBottom (26);
        const char* n[] = { "M", "S", "I" };
        const float v[] = { lufsM, lufsS, lufsI };
        const int w = s.getWidth() / 3;
        for (int i = 0; i < 3; ++i)
        {
            auto cell = s.removeFromLeft (w);
            g.setColour (theme::textDim);
            g.setFont (theme::font (10.5f, true));
            g.drawText (n[i], cell.removeFromTop (14), juce::Justification::centred);
            g.setColour (i == 2 ? theme::iceDeep : theme::text);
            g.setFont (theme::font (i == 2 ? 20.0f : 17.0f, true));
            g.drawText (fmt (v[i]), cell.removeFromTop (26), juce::Justification::centred);
        }
        g.setColour (theme::textDim);
        g.setFont (theme::font (10.0f));
        g.drawText ("LUFS", s, juce::Justification::centred);
    }
    // true peak
    {
        auto s = sections[3].withTrimmedTop (18).withTrimmedBottom (26);
        const auto ceil = proc.getCurrentParams().lim.ceilingDb;
        g.setColour (tp > ceil + 0.05f ? theme::danger : theme::text);
        g.setFont (theme::font (22.0f, true));
        g.drawText (fmt (tp, 2), s.removeFromTop (34), juce::Justification::centred);
        g.setColour (theme::textDim);
        g.setFont (theme::font (10.0f));
        g.drawText ("dBTP  (ceiling " + juce::String (ceil, 2) + ")", s, juce::Justification::centredTop);
    }
    // gain reduction bars
    {
        auto s = sections[4].withTrimmedTop (20).reduced (0, 4);
        auto bar = [&] (juce::Rectangle<int> row, const juce::String& name, float gr)
        {
            g.setColour (theme::textDim);
            g.setFont (theme::font (10.5f, true));
            g.drawText (name, row.removeFromLeft (40), juce::Justification::centredLeft);
            auto val = row.removeFromRight (52);
            auto r = row.toFloat().reduced (0, 5);
            g.setColour (juce::Colour (0xff1a2633));
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (theme::danger);
            g.fillRoundedRectangle (r.withWidth (r.getWidth() * juce::jlimit (0.0f, 1.0f, gr / 12.0f)), 3.0f);
            g.setColour (theme::text);
            g.drawText (gr < 0.05f ? juce::String ("0.0 dB") : "-" + juce::String (gr, 1) + " dB", val, juce::Justification::centredRight);
        };
        const int h = s.getHeight() / 2;
        bar (s.removeFromTop (h), "COMP", grComp);
        bar (s, "LIM", grLim);
    }
    // correlation
    {
        auto s = sections[5].withTrimmedTop (22);
        auto r = s.removeFromTop (16).toFloat();
        g.setColour (juce::Colour (0xff1a2633));
        g.fillRoundedRectangle (r, 3.0f);
        const float cx = r.getCentreX();
        const float x = cx + corr * r.getWidth() * 0.5f;
        g.setColour (corr < 0 ? theme::danger : theme::ok);
        g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (cx, x), r.getY() + 2, std::abs (x - cx) + 1.0f, r.getHeight() - 4), 2.0f);
        g.setColour (theme::textOnDark.withAlpha (0.5f));
        g.drawVerticalLine ((int) cx, r.getY(), r.getBottom());
        g.setColour (theme::textDim);
        g.setFont (theme::font (10.0f));
        auto lbl = s.removeFromTop (14);
        g.drawText ("-1", lbl, juce::Justification::centredLeft);
        g.drawText ("0", lbl, juce::Justification::centred);
        g.drawText ("+1", lbl, juce::Justification::centredRight);
        g.setColour (theme::text);
        g.setFont (theme::font (14.0f, true));
        g.drawText (juce::String (corr, 2), s.removeFromTop (20), juce::Justification::centred);
    }
}

void MeterBar::resized()
{
    auto r = getLocalBounds().reduced (16, 10);
    bypass.setBounds (r.removeFromRight (124).withSizeKeepingCentre (124, 40));
    r.removeFromRight (14);
    const int widths[] = { 214, 142, 176, 118, 170, 0 };
    for (int i = 0; i < 6; ++i)
    {
        sections[i] = i < 5 ? r.removeFromLeft (widths[i]) : r;
        if (i < 5) r.removeFromLeft (14);
    }
    inGain.setBounds (sections[0].withTrimmedTop (16).removeFromLeft (70));
    auto lc = sections[0].withTrimmedTop (18).withTrimmedLeft (74).withTrimmedRight (66).withTrimmedBottom (16);
    learn.setBounds (lc.removeFromTop (26));
    lc.removeFromTop (6);
    target.setBounds (lc.removeFromTop (20));
    outGain.setBounds (sections[1].withTrimmedTop (16).removeFromLeft (78));
    resetLufs.setBounds (sections[2].withTrimmedTop (sections[2].getHeight() - 22).withSizeKeepingCentre (64, 20));
    resetTp.setBounds (sections[3].withTrimmedTop (sections[3].getHeight() - 22).withSizeKeepingCentre (64, 20));
}
