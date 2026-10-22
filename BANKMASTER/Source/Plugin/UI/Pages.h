// BANKMASTER — BY KALOX · V2 module pages: display on top, control deck below
#pragma once

#include "Displays.h"

namespace bmui
{
/** A static caption (uppercase, tracked). */
class CaptionText : public juce::Component
{
public:
    explicit CaptionText (const String& t, juce::Justification j = juce::Justification::centredLeft) : txt (t), just (j)
    {
        setInterceptsMouseClicks (false, false);
    }
    void setText (const String& t) { if (t != txt) { txt = t; repaint(); } }
    void paint (juce::Graphics& g) override { caption (g, getLocalBounds().toFloat(), txt, P(), just); }
private:
    String txt;
    juce::Justification just;
};

class Page : public juce::Component
{
public:
    explicit Page (BankMasterProcessor& p) : proc (p) {}
    virtual void tick() {}
    /** All parameter-bound controls on this page (for tests / diagnostics). */
    virtual juce::Array<juce::Component*> getBoundControls() { return {}; }
protected:
    BankMasterProcessor& proc;
    juce::UndoManager* um() { return &proc.getUndoManager(); }
    juce::RangedAudioParameter* prm (const String& id) { return proc.getState().getParameter (id); }
    /** Deck = panel under the display. */
    Rectangle<int> deckArea;
    void paintDeck (juce::Graphics& g);
    void deckDivider (juce::Graphics& g, int x);
};

// ============================================================================ EQ
class BandChip : public juce::Button
{
public:
    BandChip (BankMasterProcessor& p, int band) : juce::Button ("Band " + String (band + 1)), proc (p), band (band) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    bool selected = false;
private:
    BankMasterProcessor& proc;
    int band;
};

class EqPage : public Page
{
public:
    explicit EqPage (BankMasterProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
    void tick() override;
    void selectBand (int b);
    int getSelectedBand() const { return band; }
    juce::Array<juce::Component*> getBoundControls() override;
private:
    void updateEnablement();
    EqDisplay display;
    CaptionText analyzerCap { "ANALYZER", juce::Justification::centredRight }, rangeCap { "RANGE", juce::Justification::centredRight };
    ParamSegmented analyzer { { "PRE", "POST", "PRE + POST" }, 10.5f }, range { { "3", "6", "12", "30" }, 10.5f };
    std::unique_ptr<BandChip> chips[bm::kNumEqBands];
    ParamToggle bandOn { "Band on", ParamToggle::Style::Power };
    CaptionText typeCap { "TYPE" }, slopeCap { "SLOPE" }, placeCap { "PLACEMENT" };
    ParamSelector type, slope;
    ParamSegmented place { { "STEREO", "MID", "SIDE", "L", "R" }, 10.5f };
    ParamKnob freq { "Frequency", KnobSize::Primary }, gain { "Gain", KnobSize::Primary, true }, q { "Q", KnobSize::Primary };
    ParamToggle dynOn { "On", ParamToggle::Style::Pill };
    ParamKnob dynRange { "Range", KnobSize::Secondary, true }, dynThr { "Threshold", KnobSize::Secondary };
    int band = 2;
    float activity = 0.0f;
};

// ============================================================================ SATURATION
class SatPage : public Page
{
public:
    explicit SatPage (BankMasterProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
    void tick() override { display.tick(); }
    juce::Array<juce::Component*> getBoundControls() override;
private:
    SatDisplay display;
    ParamSegmented style { bmp::satStyleNames(), 11.0f, ParamSegmented::Look::SatCards };
    ParamKnob drive { "Drive", KnobSize::Hero };
    ParamKnob mix { "Mix", KnobSize::Secondary }, tone { "Tone", KnobSize::Secondary, true }, dyn { "Dynamics", KnobSize::Secondary, true },
              fb { "Feedback", KnobSize::Secondary }, fbFreq { "FB Freq", KnobSize::Secondary }, out { "Output", KnobSize::Secondary, true };
    CaptionText osCap { "OVERSAMPLING" };
    ParamSegmented os { { "1x", "2x", "4x", "8x", "16x" }, 10.5f };
    ParamToggle hq { "High quality", ParamToggle::Style::Pill };
};

// ============================================================================ COMPRESSOR
class CompPage : public Page
{
public:
    explicit CompPage (BankMasterProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
    void tick() override;
    juce::Array<juce::Component*> getBoundControls() override;
private:
    CompDisplay display;
    CaptionText styleCap { "STYLE", juce::Justification::centredRight };
    ParamSegmented style { { "CLEAN", "GLUE", "PUNCH" }, 10.5f };
    ParamKnob thr { "Threshold", KnobSize::Primary }, ratio { "Ratio", KnobSize::Primary }, att { "Attack", KnobSize::Primary }, rel { "Release", KnobSize::Primary };
    ParamKnob knee { "Knee", KnobSize::Secondary }, range { "Range", KnobSize::Secondary }, hold { "Hold", KnobSize::Secondary },
              la { "Lookahead", KnobSize::Secondary }, makeup { "Makeup", KnobSize::Secondary, true }, mix { "Mix", KnobSize::Secondary };
    ParamToggle autoGain { "Auto gain", ParamToggle::Style::Pill }, autoRel { "Auto release", ParamToggle::Style::Pill };
    ParamKnob scHp { "SC HP", KnobSize::Mini }, scLp { "SC LP", KnobSize::Mini }, link { "Link", KnobSize::Mini };
};

// ============================================================================ LIMITER
class LimPage : public Page
{
public:
    explicit LimPage (BankMasterProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
    void tick() override { display.tick(); }
    juce::Array<juce::Component*> getBoundControls() override;
private:
    LimDisplay display;
    CaptionText styleCap { "STYLE", juce::Justification::centredRight };
    ParamSegmented style { { "TRANSPARENT", "PUNCHY", "DYNAMIC", "SAFE" }, 10.5f };
    ParamKnob gain { "Gain", KnobSize::Hero }, ceiling { "Output", KnobSize::Primary };
    ParamKnob att { "Attack", KnobSize::Secondary }, rel { "Release", KnobSize::Secondary }, la { "Lookahead", KnobSize::Secondary },
              linkT { "Link trans.", KnobSize::Secondary }, linkR { "Link rel.", KnobSize::Secondary };
    ParamToggle tp { "True peak", ParamToggle::Style::Pill };
    CaptionText osCap { "OVERSAMPLING" }, ditherCap { "DITHER" };
    ParamSegmented os { { "1x", "2x", "4x", "8x", "16x" }, 10.5f };
    ParamSelector dither;
};

} // namespace bmui
