// BANKMASTER by KALOX — the four processor pages
#pragma once

#include "EqGraph.h"
#include "Widgets.h"

/** Rolling history (oldest -> newest) for the dynamics displays. */
struct History
{
    static constexpr int kLen = 360;
    explicit History (float init = 0.0f) { std::fill (std::begin (v), std::end (v), init); }
    float v[kLen];
    int pos = 0;
    void push (float x) { v[pos] = x; pos = (pos + 1) % kLen; }
    float at (int i) const { return v[(pos + i) % kLen]; } // i = 0 oldest
};

class Page : public juce::Component
{
public:
    virtual void tick() {}
};

// ============================================================================ EQ
class EqPage : public Page
{
public:
    explicit EqPage (BankMasterProcessor& p);
    void resized() override;
    void paint (juce::Graphics& g) override;
    void tick() override { graph.tick(); updateEnablement(); }
private:
    void selectBand (int b);
    void updateEnablement();
    BankMasterProcessor& proc;
    EqGraph graph;
    juce::TextButton bandButtons[bm::kNumEqBands];
    std::unique_ptr<Toggle> bandOn, dynOn;
    std::unique_ptr<Choice> type, slope, place;
    std::unique_ptr<Knob> freq, gain, q, range, thr;
    int band = 1;
};

// ============================================================================ SATURATION
class SatCurve : public juce::Component
{
public:
    explicit SatCurve (BankMasterProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
private:
    BankMasterProcessor& proc;
};

class SatPage : public Page
{
public:
    explicit SatPage (BankMasterProcessor& p);
    void resized() override;
    void paint (juce::Graphics& g) override;
    void tick() override { curve.repaint(); }
private:
    SatCurve curve;
    Choice style, os;
    Toggle hq;
    Knob drive, mix, tone, dyn, fb, fbFreq, out;
};

// ============================================================================ COMPRESSOR
class CompDisplay : public juce::Component
{
public:
    explicit CompDisplay (BankMasterProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
    void tick();
private:
    BankMasterProcessor& proc;
    History gr { 0.0f }, level { -100.0f };
};

class CompPage : public Page
{
public:
    explicit CompPage (BankMasterProcessor& p);
    void resized() override;
    void paint (juce::Graphics& g) override;
    void tick() override { display.tick(); }
private:
    CompDisplay display;
    Choice style;
    Toggle autoGain, autoRel;
    Knob thr, ratio, knee, att, rel, range, hold, la, makeup, mix, scHp, scLp, link;
};

// ============================================================================ LIMITER
class LimDisplay : public juce::Component
{
public:
    explicit LimDisplay (BankMasterProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
    void tick();
private:
    BankMasterProcessor& proc;
    History gr { 0.0f }, out { -100.0f };
};

class LimPage : public Page
{
public:
    explicit LimPage (BankMasterProcessor& p);
    void resized() override;
    void paint (juce::Graphics& g) override;
    void tick() override;
private:
    BankMasterProcessor& proc;
    LimDisplay display;
    Choice style, os, dither;
    Toggle tp;
    Knob gain, ceil, att, rel, la, linkT, linkR;
    juce::String lufsText[3], tpText, latencyText;
};
