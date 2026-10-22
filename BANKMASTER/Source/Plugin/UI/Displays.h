// BANKMASTER — BY KALOX · V2 real-time displays (EQ, saturation, compressor, limiter)
#pragma once

#include <juce_dsp/juce_dsp.h>
#include "Controls.h"

namespace bmui
{
/** Rolling history (oldest -> newest). */
struct History
{
    static constexpr int kLen = 250;   // 5 s at the 50 Hz UI rate
    explicit History (float init = 0.0f) { std::fill (std::begin (v), std::end (v), init); }
    float v[kLen];
    int pos = 0;
    void push (float x) { v[pos] = x; pos = (pos + 1) % kLen; }
    float at (int i) const { return v[(pos + i) % kLen]; }   // i = 0 oldest
    float last() const { return at (kLen - 1); }
};

/** Section header used inside displays and decks: coloured marker + tracked caption. */
void sectionTitle (juce::Graphics&, Rectangle<float> r, const String& text, juce::Colour marker);

// ============================================================================ spectrum
class SpectrumAnalyzer
{
public:
    static constexpr int kOrder = 13, kSize = 1 << kOrder;
    SpectrumAnalyzer();
    /** Pulls new samples from the processor and updates the smoothed spectrum (dB). */
    void update (BankMasterProcessor& p, bool post);
    /** Level (dB) around freq with 1/6-octave Gaussian smoothing. */
    float levelAt (double freq, double fs) const;
    bool hasData() const { return filled; }
private:
    juce::dsp::FFT fft { kOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) kSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> ring, fftData, smoothedPow;
    int ringPos = 0;
    bool filled = false;
};

// ============================================================================ EQ
class EqDisplay : public juce::Component
{
public:
    explicit EqDisplay (BankMasterProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

    void tick();
    std::function<void (int)> onBandSelected;
    void setSelectedBand (int b) { selected = b; repaint(); }
    int getSelectedBand() const { return selected; }
    /** Display range in dB (3, 6, 12, 30). */
    void setRangeDb (float r) { rangeDb = r; repaint(); }
    /** 0 = pre, 1 = post, 2 = pre + post */
    void setAnalyzerMode (int m) { analyzerMode = m; repaint(); }

private:
    Rectangle<float> plotArea() const;
    float freqToX (double f) const;
    double xToFreq (float x) const;
    float dbToY (float db) const;
    float yToDb (float y) const;
    juce::Point<float> nodePos (int band) const;
    int hitNode (juce::Point<float> pt) const;
    void setParam (const String& pid, float value);
    void gesture (int band, bool begin);

    BankMasterProcessor& proc;
    SpectrumAnalyzer pre, post;
    bm::ChainParams cached;
    float dynGain[bm::kNumEqBands] {};
    float rangeDb = 6.0f;
    int analyzerMode = 2;
    int selected = 2, dragging = -1, hover = -1;
};

// ============================================================================ SATURATION
class SatDisplay : public juce::Component
{
public:
    explicit SatDisplay (BankMasterProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void tick();
private:
    void computeHarmonics (const bm::SatParams& sp);
    BankMasterProcessor& proc;
    bm::SatParams lastSat;
    bool haveHarmonics = false;
    std::vector<float> harmonicsDb;
    float thdPct = 0, inputPeak = 0;
    juce::uint32 lastCompute = 0;
    bool pending = true;
};

// ============================================================================ COMPRESSOR
class CompDisplay : public juce::Component
{
public:
    explicit CompDisplay (BankMasterProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void tick();
private:
    BankMasterProcessor& proc;
    History gr { 0.0f }, level { -100.0f };
};

// ============================================================================ LIMITER
class LimDisplay : public juce::Component
{
public:
    explicit LimDisplay (BankMasterProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
private:
    BankMasterProcessor& proc;
    History gr { 0.0f }, out { -100.0f };
    float grMax = 0.0f;
    TextButtonV2 resetButton { "Reset" };
    Rectangle<float> loudnessPanel() const;
};

} // namespace bmui
