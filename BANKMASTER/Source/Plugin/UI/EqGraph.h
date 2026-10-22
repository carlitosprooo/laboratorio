// BANKMASTER by KALOX — EQ display: real-time spectrum, response curve, draggable nodes
#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../PluginProcessor.h"
#include "Theme.h"

class SpectrumAnalyzer
{
public:
    static constexpr int kOrder = 12, kSize = 1 << kOrder;
    SpectrumAnalyzer();
    /** Pulls new samples from the processor and updates the smoothed spectrum (dB). */
    void update (BankMasterProcessor& p, bool post);
    float levelAt (double freq, double fs) const;
    bool hasData() const { return filled; }
private:
    juce::dsp::FFT fft { kOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) kSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> ring, scratch, fftData, smoothed;
    int ringPos = 0;
    bool filled = false;
};

class EqGraph : public juce::Component
{
public:
    explicit EqGraph (BankMasterProcessor& p);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    void mouseMove (const juce::MouseEvent& e) override;

    /** Called by the editor timer (~30 Hz). */
    void tick();

    std::function<void (int)> onBandSelected;
    void setSelectedBand (int b) { selected = b; repaint(); }
    int getSelectedBand() const { return selected; }

    static constexpr float kDbRange = 12.0f;

private:
    juce::Rectangle<float> plotArea() const;
    float freqToX (double f) const;
    double xToFreq (float x) const;
    float dbToY (float db) const;
    float yToDb (float y) const;
    float specToY (float db) const;
    juce::Point<float> nodePos (int band, const bm::ChainParams& p) const;
    int hitNode (juce::Point<float> pt) const;
    void setParam (const juce::String& pid, float value);
    void beginGesture (int band);
    void endGesture (int band);

    BankMasterProcessor& proc;
    SpectrumAnalyzer pre, post;
    bm::ChainParams cached;
    float dynGain[bm::kNumEqBands] {};
    int selected = 1, dragging = -1, hover = -1;
};
