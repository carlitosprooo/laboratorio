// BANKMASTER by KALOX — permanent bottom bar: INPUT, OUTPUT, LUFS, TRUE PEAK, GR, CORRELATION, BYPASS
#pragma once

#include "Widgets.h"
#include "../PluginProcessor.h"

class MeterBar : public juce::Component
{
public:
    explicit MeterBar (BankMasterProcessor& p);
    void paint (juce::Graphics& g) override;
    void resized() override;
    void tick();

private:
    struct Smooth { float v = -60.0f; void set (float db) { v = db > v ? db : v + (db - v) * 0.18f; } };
    void drawLevel (juce::Graphics& g, juce::Rectangle<float> r, float db, float minDb = -48.0f);
    juce::Rectangle<int> sections[6];
    BankMasterProcessor& proc;
    Knob inGain, outGain;
    Toggle bypass;
    juce::TextButton resetLufs { "RESET" }, resetTp { "RESET" }, learn { "LEARN" };
    juce::Slider target;
    std::unique_ptr<APVTS::SliderAttachment> targetAttachment;
    juce::String learnText;
    Smooth inL, inR, outL, outR;
    float lufsM = -INFINITY, lufsS = -INFINITY, lufsI = -INFINITY, tp = -100, grComp = 0, grLim = 0, corr = 1;
};
