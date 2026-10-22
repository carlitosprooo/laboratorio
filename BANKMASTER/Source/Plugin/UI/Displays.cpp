// BANKMASTER — BY KALOX · V2 real-time displays
#include "Displays.h"

namespace bmui
{
namespace
{
    void text (juce::Graphics& g, Rectangle<float> r, const String& t, float h, Fonts::Weight w, juce::Colour c,
               juce::Justification j = juce::Justification::centredLeft, float kern = 0.0f)
    {
        g.setColour (c);
        g.setFont (font (h, w, kern));
        g.drawText (t, r, j, false);
    }
    void hline (juce::Graphics& g, float y, float x0, float x1, juce::Colour c) { g.setColour (c); g.fillRect (x0, y, x1 - x0, 1.0f); }
    void vline (juce::Graphics& g, float x, float y0, float y1, juce::Colour c) { g.setColour (c); g.fillRect (x, y0, 1.0f, y1 - y0); }
    void dashed (juce::Graphics& g, juce::Line<float> l, juce::Colour c, float w = 1.0f)
    {
        const float dash[] = { 4.0f, 4.0f };
        g.setColour (c);
        g.drawDashedLine (l, dash, 2, w);
    }
    float toDb (float g, float floor = -100.0f) { return juce::Decibels::gainToDecibels (g, floor); }
    String hz (float f) { return f >= 1000.0f ? String (f / 1000.0f, f >= 10000.0f ? 1 : 2) + " kHz" : String (f, f < 100.0f ? 1 : 0) + " Hz"; }
    String db (float v, int dec = 1)
    {
        if (std::abs (v) < 0.5f * std::pow (10.0f, (float) -dec)) v = 0.0f;
        return (v > 0 ? "+" : "") + String (v, dec) + " dB";
    }
    void legend (juce::Graphics& g, Rectangle<float>& r, juce::Colour c, const String& t)
    {
        g.setColour (c);
        g.fillRoundedRectangle (r.removeFromLeft (10).withSizeKeepingCentre (10, 4), 2);
        text (g, r.removeFromLeft (t.length() * 7.5f + 14).withTrimmedLeft (6), t, 10.0f, Fonts::SemiBold, P().textMute, juce::Justification::centredLeft, 0.1f);
    }
}

void sectionTitle (juce::Graphics& g, Rectangle<float> r, const String& t, juce::Colour mark)
{
    g.setColour (mark);
    g.fillRoundedRectangle (Rectangle<float> (3, 12).withCentre ({ r.getX() + 2, r.getCentreY() }), 1.5f);
    caption (g, r.withTrimmedLeft (10), t, P(), juce::Justification::centredLeft, P().textDim);
}

// ============================================================================ spectrum
SpectrumAnalyzer::SpectrumAnalyzer()
    : ring ((size_t) kSize, 0.0f), fftData ((size_t) kSize * 2, 0.0f), smoothedPow ((size_t) kSize / 2, 1.0e-12f)
{
}

void SpectrumAnalyzer::update (BankMasterProcessor& p, bool isPost)
{
    float tmp[2048];
    int got = 0;
    bool fresh = false;
    while ((got = p.pullAnalyzer (isPost, tmp, 2048)) > 0)
    {
        for (int i = 0; i < got; ++i) { ring[(size_t) ringPos] = tmp[i]; ringPos = (ringPos + 1) % kSize; }
        filled = fresh = true;
    }
    if (! fresh) return;
    for (int i = 0; i < kSize; ++i) fftData[(size_t) i] = ring[(size_t) ((ringPos + i) % kSize)];
    std::fill (fftData.begin() + kSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) kSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());
    const float norm = 4.0f / (float) kSize;   // Hann, one-sided: a full-scale sine reads 0 dB
    for (int k = 0; k < kSize / 2; ++k)
    {
        const float a = fftData[(size_t) k] * norm;
        const float pw = a * a;
        float& s = smoothedPow[(size_t) k];
        s = pw > s ? s + (pw - s) * 0.6f : s + (pw - s) * 0.18f;   // quick rise, smooth fall
    }
}

float SpectrumAnalyzer::levelAt (double freq, double fs) const
{
    const double kc = freq / fs * kSize;
    const double sigma = std::log (2.0) / 12.0;   // ~1/6-octave wide Gaussian
    const int k0 = juce::jlimit (1, kSize / 2 - 2, (int) std::floor (kc * std::exp (-3.0 * sigma)) - 1);
    const int k1 = juce::jlimit (k0 + 1, kSize / 2 - 1, (int) std::ceil (kc * std::exp (3.0 * sigma)) + 1);
    double acc = 0, wsum = 0;
    for (int k = k0; k <= k1; ++k)
    {
        const double d = std::log ((double) k / kc) / sigma;
        const double w = std::exp (-0.5 * d * d);
        acc += w * smoothedPow[(size_t) k];
        wsum += w;
    }
    return 10.0f * std::log10 ((float) juce::jmax (1.0e-14, acc / juce::jmax (1.0e-12, wsum)));
}

// ============================================================================ EQ
EqDisplay::EqDisplay (BankMasterProcessor& p) : proc (p)
{
    cached = bm::applyOneMaster (proc.getCurrentParams());
}

void EqDisplay::resized() {}

Rectangle<float> EqDisplay::plotArea() const
{
    return getLocalBounds().toFloat().reduced (14, 12).withTrimmedTop (30).withTrimmedBottom (18).withTrimmedRight (26);
}

float EqDisplay::freqToX (double f) const
{
    const auto a = plotArea();
    return a.getX() + a.getWidth() * (float) (std::log (f / 20.0) / std::log (1000.0));
}
double EqDisplay::xToFreq (float x) const
{
    const auto a = plotArea();
    return 20.0 * std::pow (1000.0, (x - a.getX()) / a.getWidth());
}
float EqDisplay::dbToY (float d) const
{
    const auto a = plotArea();
    return a.getCentreY() - d / rangeDb * a.getHeight() * 0.5f;
}
float EqDisplay::yToDb (float y) const
{
    const auto a = plotArea();
    return (a.getCentreY() - y) / (a.getHeight() * 0.5f) * rangeDb;
}

juce::Point<float> EqDisplay::nodePos (int band) const
{
    const auto& b = cached.eq.bands[(size_t) band];
    const float g = bm::bandSupportsGain (b.type) ? b.gainDb : 0.0f;
    const auto a = plotArea();
    return { freqToX (b.freq), juce::jlimit (a.getY(), a.getBottom(), dbToY (g)) };
}

int EqDisplay::hitNode (juce::Point<float> pt) const
{
    int best = -1;
    float bestD = 14.0f;
    for (int b = 0; b < bm::kNumEqBands; ++b)
    {
        const float d = nodePos (b).getDistanceFrom (pt);
        if (d < bestD) { bestD = d; best = b; }
    }
    return best;
}

void EqDisplay::tick()
{
    pre.update (proc, false);
    post.update (proc, true);
    cached = bm::applyOneMaster (proc.getCurrentParams());
    for (int b = 0; b < bm::kNumEqBands; ++b) dynGain[b] = proc.getChain().getEq().getCurrentGainDb (b);
    repaint();
}

void EqDisplay::paint (juce::Graphics& g)
{
    const auto& p = P();
    const auto bounds = getLocalBounds().toFloat();
    display (g, bounds, p, 12.0f);
    const auto a = plotArea();
    const double fs = proc.getCurrentSampleRate();
    const bool eqOn = cached.moduleOn[bm::kModEq];

    sectionTitle (g, bounds.reduced (14, 10).removeFromTop (24).withWidth (140), "MASTER EQ", p.module[0]);

    // grid
    for (double f : { 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 90.0, 200.0, 300.0, 400.0, 500.0, 600.0, 700.0, 800.0, 900.0,
                      2000.0, 3000.0, 4000.0, 5000.0, 6000.0, 7000.0, 8000.0, 9000.0 })
        vline (g, freqToX (f), a.getY(), a.getBottom(), p.grid.withMultipliedAlpha (0.6f));
    const double fm[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    const char* fmL[] = { "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };
    for (int i = 0; i < 10; ++i)
    {
        const float x = freqToX (fm[i]);
        if (fm[i] == 100 || fm[i] == 1000 || fm[i] == 10000) vline (g, x, a.getY(), a.getBottom(), p.gridStrong);
        text (g, Rectangle<float> (x - 20, a.getBottom() + 4, 40, 14), fmL[i], 10.0f, Fonts::Medium, p.textMute, juce::Justification::centred);
    }
    const float step = rangeDb <= 3.0f ? 1.0f : rangeDb <= 6.0f ? 2.0f : rangeDb <= 12.0f ? 3.0f : 6.0f;
    for (float d = -rangeDb; d <= rangeDb + 0.01f; d += step)
    {
        const float y = dbToY (d);
        hline (g, y, a.getX(), a.getRight(), std::abs (d) < 0.01f ? p.gridStrong : p.grid);
        const int di = juce::roundToInt (d);
        text (g, Rectangle<float> (a.getRight() + 4, y - 7, 26, 14), di > 0 ? "+" + String (di) : String (di), 10.0f, Fonts::Medium, p.textMute);
    }

    // spectra: 4.5 dB/oct display tilt around 1 kHz, -90..-18 dB mapped to the lower 70 %
    auto specPath = [&] (const SpectrumAnalyzer& s, bool closed)
    {
        Path path;
        const float top = a.getY() + a.getHeight() * 0.30f, bottom = a.getBottom();
        const float lo = -96.0f, hi = -24.0f;
        bool started = false;
        for (float x = a.getX(); x <= a.getRight() + 0.5f; x += 2.0f)
        {
            const double f = xToFreq (x);
            if (f >= fs * 0.5) break;
            const float v = s.levelAt (f, fs) + 4.5f * (float) std::log2 (f / 1000.0);
            const float y = juce::jlimit (top, bottom, bottom - (v - lo) / (hi - lo) * (bottom - top));
            if (! started) { path.startNewSubPath (x, closed ? bottom : y); if (closed) path.lineTo (x, y); started = true; }
            else path.lineTo (x, y);
        }
        if (closed && started) { path.lineTo (path.getCurrentPosition().x, bottom); path.closeSubPath(); }
        return path;
    };
    const bool showPre = analyzerMode != 1, showPost = analyzerMode != 0;
    if (showPre && pre.hasData())
    {
        if (! showPost) { g.setColour (p.specPre.withAlpha (0.65f)); g.fillPath (specPath (pre, true)); }
    }
    if (showPost && post.hasData())
    {
        g.setGradientFill (juce::ColourGradient (p.specPost.withAlpha (0.16f), 0, a.getY() + a.getHeight() * 0.3f, p.specPost.withAlpha (0.01f), 0, a.getBottom(), false));
        g.fillPath (specPath (post, true));
        g.setColour (p.specPost.withAlpha (0.75f));
        g.strokePath (specPath (post, false), juce::PathStrokeType (1.1f));
    }
    if (showPre && pre.hasData())
    {
        g.setColour (p.textDim.withAlpha (0.45f));
        g.strokePath (specPath (pre, false), juce::PathStrokeType (1.0f));
    }

    // per-band fills, dynamic range, total curve
    auto curveOf = [&] (auto&& dbAt)
    {
        Path c;
        bool first = true;
        for (float x = a.getX(); x <= a.getRight() + 0.5f; x += 1.5f)
        {
            const float y = juce::jlimit (a.getY() - 4, a.getBottom() + 4, dbToY ((float) dbAt (xToFreq (x))));
            if (first) { c.startNewSubPath (x, y); first = false; } else c.lineTo (x, y);
        }
        return c;
    };
    for (int b = 0; b < bm::kNumEqBands; ++b)
    {
        const auto& bp = cached.eq.bands[(size_t) b];
        if (! bp.enabled) continue;
        const bool dyn = bp.dynOn && bm::bandSupportsDynamics (bp.type);
        auto c = curveOf ([&] (double f) { return bm::bandMagnitudeDb (bp, f, fs, dyn ? dynGain[b] : 1e9f); });
        Path fill (c);
        fill.lineTo (a.getRight(), dbToY (0)); fill.lineTo (a.getX(), dbToY (0)); fill.closeSubPath();
        g.setColour (p.bands[b].withAlpha (b == selected ? 0.22f : 0.10f));
        g.fillPath (fill);
        if (dyn)
        {
            auto lim = curveOf ([&] (double f) { return bm::bandMagnitudeDb (bp, f, fs, bp.gainDb + bp.dynRangeDb); });
            const float dash[] = { 3.0f, 3.0f };
            Path dp;
            juce::PathStrokeType (1.0f).createDashedStroke (dp, lim, dash, 2);
            g.setColour (p.bands[b].withAlpha (b == selected ? 0.9f : 0.5f));
            g.fillPath (dp);
        }
    }
    auto total = curveOf ([&] (double f)
    {
        double s = 0;
        for (int b = 0; b < bm::kNumEqBands; ++b)
        {
            const auto& bp = cached.eq.bands[(size_t) b];
            const bool dyn = bp.dynOn && bm::bandSupportsDynamics (bp.type);
            if (bp.enabled) s += bm::bandMagnitudeDb (bp, f, fs, dyn ? dynGain[b] : 1e9f);
        }
        return s;
    });
    glowStroke (g, total, eqOn ? p.curve : p.metal, 2.2f, eqOn ? 9.0f : 0.0f);

    // nodes
    for (int b = 0; b < bm::kNumEqBands; ++b)
    {
        const auto& bp = cached.eq.bands[(size_t) b];
        const auto c = nodePos (b);
        const auto col = bp.enabled ? p.bands[b] : p.metal;
        if (b == selected)
        {
            g.setColour (col.withAlpha (0.16f)); g.fillEllipse (Rectangle<float> (40, 40).withCentre (c));
            g.setColour (col.withAlpha (0.55f)); g.drawEllipse (Rectangle<float> (30, 30).withCentre (c), 1.2f);
            if (bp.enabled && bp.dynOn && bm::bandSupportsDynamics (bp.type))
            {
                // dynamic range handle
                const float yr = juce::jlimit (a.getY(), a.getBottom(), dbToY (bp.gainDb + bp.dynRangeDb));
                const bool far = std::abs (yr - c.y) > 16.0f;
                const float dir = yr >= c.y ? 1.0f : -1.0f;
                g.setColour (col);
                if (far) g.drawLine (c.x, c.y + 15.0f * dir, c.x, yr, 1.0f);
                g.fillRoundedRectangle (Rectangle<float> (14, 4).withCentre ({ c.x, far ? yr : c.y + 18.0f * dir }), 2);
            }
        }
        const float r = b == hover ? 10.0f : 9.0f;
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c.translated (0, 1.5f)));
        g.setColour (col); g.fillEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c));
        g.setColour (juce::Colours::white.withAlpha (b == selected ? 0.8f : 0.35f)); g.drawEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c), 1.0f);
        text (g, Rectangle<float> (r * 2, r * 2).withCentre (c), String (b + 1), 10.5f, Fonts::Bold, juce::Colour (0xff0b0c0d), juce::Justification::centred);
    }

    // read-out for the selected band
    if (selected >= 0)
    {
        const auto& bp = cached.eq.bands[(size_t) selected];
        const auto c = nodePos (selected);
        const auto col = p.bands[selected];
        const bool dyn = bp.dynOn && bm::bandSupportsDynamics (bp.type);
        String head = bmp::bandTypeNames()[(int) bp.type].toUpperCase();
        if (dyn) head << String (juce::CharPointer_UTF8 (" \xc2\xb7 DYNAMIC"));
        if (! bp.enabled) head << "  (OFF)";
        String line = hz (bp.freq);
        if (bm::bandSupportsGain (bp.type)) line << "   " << db (dyn ? dynGain[selected] : bp.gainDb, 2);
        line << "   Q " << String (bp.q, 2);
        const float w = 176.0f;
        auto tip = Rectangle<float> (w, 40).withPosition (c.x + 22, c.y - 62);
        if (tip.getRight() > a.getRight()) tip.setX (c.x - 22 - w);
        if (tip.getY() < a.getY()) tip.setY (c.y + 24);
        g.setColour (p.panelHi.withAlpha (0.96f)); g.fillRoundedRectangle (tip, 7);
        g.setColour (col.withAlpha (0.6f)); g.drawRoundedRectangle (tip.reduced (0.5f), 7, 1);
        auto ti = tip.reduced (10, 5);
        text (g, ti.removeFromTop (15), head, 10.0f, Fonts::SemiBold, col, juce::Justification::centredLeft, 0.1f);
        text (g, ti, line, 11.5f, Fonts::Medium, p.text);
    }
}

void EqDisplay::setParam (const String& pid, float value)
{
    if (auto* prm = proc.getState().getParameter (pid))
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
}

void EqDisplay::gesture (int band, bool begin)
{
    for (auto* w : { "freq", "gain", "q" })
        if (auto* prm = proc.getState().getParameter (bmp::id::eq (band, w)))
            begin ? prm->beginChangeGesture() : prm->endChangeGesture();
}

void EqDisplay::mouseDown (const juce::MouseEvent& e)
{
    const int hit = hitNode (e.position);
    if (hit < 0) return;
    selected = dragging = hit;
    gesture (hit, true);
    if (onBandSelected) onBandSelected (hit);
    repaint();
}

void EqDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    const auto a = plotArea();
    const float x = juce::jlimit (a.getX(), a.getRight(), e.position.x);
    const float y = juce::jlimit (a.getY(), a.getBottom(), e.position.y);
    setParam (bmp::id::eq (dragging, "freq"), (float) juce::jlimit (10.0, 30000.0, xToFreq (x)));
    if (bm::bandSupportsGain (cached.eq.bands[(size_t) dragging].type) && ! e.mods.isShiftDown())
        setParam (bmp::id::eq (dragging, "gain"), juce::jlimit (-24.0f, 24.0f, yToDb (y)));
    cached = bm::applyOneMaster (proc.getCurrentParams());
    repaint();
}

void EqDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0) gesture (dragging, false);
    dragging = -1;
}

void EqDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int hit = hitNode (e.position);
    if (hit >= 0)
    {
        if (auto* prm = proc.getState().getParameter (bmp::id::eq (hit, "on")))
            prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
        return;
    }
    // double-click on empty space: activate the first disabled band as a bell here
    for (int b = 0; b < bm::kNumEqBands; ++b)
        if (! cached.eq.bands[(size_t) b].enabled)
        {
            setParam (bmp::id::eq (b, "type"), (float) bm::BandType::Bell);
            setParam (bmp::id::eq (b, "freq"), (float) xToFreq (e.position.x));
            setParam (bmp::id::eq (b, "gain"), juce::jlimit (-24.0f, 24.0f, yToDb (e.position.y)));
            setParam (bmp::id::eq (b, "on"), 1.0f);
            selected = b;
            if (onBandSelected) onBandSelected (b);
            return;
        }
}

void EqDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    int band = hitNode (e.position);
    if (band < 0) band = selected;
    if (band < 0) return;
    const float q = cached.eq.bands[(size_t) band].q;
    setParam (bmp::id::eq (band, "q"), juce::jlimit (0.025f, 40.0f, q * std::pow (1.15f, w.deltaY * 4.0f)));
}

void EqDisplay::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

// ============================================================================ SATURATION
void SatDisplay::computeHarmonics (const bm::SatParams& sp)
{
    // same processor as the audio path, fed a 1 kHz tone at -6 dBFS
    constexpr int N = 8192, bin = 171;
    const double fs = 48000.0, f = bin * fs / N;
    bm::Saturation sat;
    sat.prepare (fs, 512, 1);
    sat.setParams (sp);
    sat.reset();
    std::vector<float> x ((size_t) N * 2);
    for (size_t i = 0; i < x.size(); ++i) x[i] = (float) (0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * f * (double) i / fs));
    for (size_t s = 0; s < x.size(); s += 512)
    {
        float* d[1] = { x.data() + s };
        sat.process (d, 1, 512);
    }
    juce::dsp::FFT fft (13);
    std::vector<float> buf ((size_t) N * 2, 0.0f);
    for (int i = 0; i < N; ++i)
    {
        const double w = 0.35875 - 0.48829 * std::cos (2 * juce::MathConstants<double>::pi * i / N) + 0.14128 * std::cos (4 * juce::MathConstants<double>::pi * i / N)
                       - 0.01168 * std::cos (6 * juce::MathConstants<double>::pi * i / N);
        buf[(size_t) i] = (float) (x[(size_t) (N + i)] * w);
    }
    fft.performFrequencyOnlyForwardTransform (buf.data());
    auto mag = [&] (int k) { float m = 0; for (int d = -2; d <= 2; ++d) m = juce::jmax (m, buf[(size_t) (k + d)]); return m; };
    const float h1 = juce::jmax (1.0e-12f, mag (bin));
    harmonicsDb.clear();
    double hp = 0;
    for (int h = 1; h <= 10; ++h)
    {
        const float m = mag (bin * h) / h1;
        harmonicsDb.push_back (toDb (m, -200.0f));
        if (h > 1) hp += (double) m * m;
    }
    thdPct = (float) (100.0 * std::sqrt (hp));
    haveHarmonics = true;
}

void SatDisplay::tick()
{
    const auto p = bm::applyOneMaster (proc.getCurrentParams());
    const auto& s = p.sat;
    const bool changed = ! haveHarmonics || s.style != lastSat.style || s.drivePct != lastSat.drivePct || s.mixPct != lastSat.mixPct
                      || s.tone != lastSat.tone || s.dynamics != lastSat.dynamics || s.feedbackPct != lastSat.feedbackPct
                      || s.feedbackFreq != lastSat.feedbackFreq || s.outputDb != lastSat.outputDb
                      || s.osFactorLog2 != lastSat.osFactorLog2 || s.quality != lastSat.quality;
    if (changed) { lastSat = s; pending = true; }
    const auto now = juce::Time::getMillisecondCounter();
    if (pending && now - lastCompute > 120)   // at most ~8 analyses per second while a knob moves
    {
        computeHarmonics (lastSat);
        lastCompute = now;
        pending = false;
    }
    auto& m = proc.getChain().getInputMeter();
    inputPeak = juce::jmax (m.getPeak (0), m.getPeak (1));
    repaint();
}

void SatDisplay::paint (juce::Graphics& g)
{
    const auto& p = P();
    const auto mc = p.module[1];
    const auto all = bm::applyOneMaster (proc.getCurrentParams());
    const auto& sp = all.sat;
    const bool on = all.moduleOn[bm::kModSat];
    auto bounds = getLocalBounds().toFloat();
    display (g, bounds, p, 12.0f);
    auto di = bounds.reduced (14, 10);
    {
        auto tb = di.removeFromTop (24);
        sectionTitle (g, tb.removeFromLeft (220), "MASTER SATURATION", mc);
        text (g, tb, String (juce::CharPointer_UTF8 ("TEST TONE 1 kHz \xc2\xb7 \xe2\x88\x92")) + "6 dBFS  " + String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "  THD "
                         + String (thdPct, thdPct < 0.01f ? 4 : 3) + " %", 10.0f, Fonts::Medium, p.textMute, juce::Justification::centredRight, 0.05f);
    }
    di.removeFromTop (8);

    // transfer curve
    auto tr = di.removeFromLeft (di.getHeight()).reduced (4);
    {
        caption (g, tr.removeFromTop (16), "TRANSFER", p, juce::Justification::centredLeft);
        auto pl = tr.reduced (18, 8);
        const float side = juce::jmin (pl.getWidth(), pl.getHeight());
        pl = Rectangle<float> (side, side).withCentre (pl.getCentre());
        for (int i = 0; i <= 8; ++i)
        {
            vline (g, pl.getX() + pl.getWidth() * i / 8, pl.getY(), pl.getBottom(), i == 4 ? p.gridStrong : p.grid);
            hline (g, pl.getY() + pl.getHeight() * i / 8, pl.getX(), pl.getRight(), i == 4 ? p.gridStrong : p.grid);
        }
        dashed (g, { pl.getX(), pl.getBottom(), pl.getRight(), pl.getY() }, p.textMute.withAlpha (0.6f));
        const float drv = std::pow (10.0f, bm::Shaper::driveDb (sp.drivePct) / 20.0f);
        const float outG = std::pow (10.0f, sp.outputDb / 20.0f);
        auto yAt = [&] (float x, float wet) { return outG * ((1 - wet) * x + wet * bm::Shaper::curve (sp.style, x * drv) / drv); };
        auto curve = [&] (float wet)
        {
            Path c;
            for (int i = 0; i <= 160; ++i)
            {
                const float x = -1.0f + 2.0f * i / 160.0f;
                const auto pt = juce::Point<float> (pl.getCentreX() + x * pl.getWidth() * 0.5f, pl.getCentreY() - juce::jlimit (-1.1f, 1.1f, yAt (x, wet)) * pl.getHeight() * 0.5f);
                if (i == 0) c.startNewSubPath (pt); else c.lineTo (pt);
            }
            return c;
        };
        const auto col = on ? mc : p.metal;
        g.setColour (col.withAlpha (0.35f));
        g.strokePath (curve (1.0f), juce::PathStrokeType (1.2f));
        glowStroke (g, curve (sp.mixPct * 0.01f), col, 2.2f, on ? 9.0f : 0.0f);
        // live input level on the curve
        const float lx = juce::jlimit (0.0f, 1.0f, inputPeak);
        const auto dot = juce::Point<float> (pl.getCentreX() + lx * pl.getWidth() * 0.5f, pl.getCentreY() - juce::jlimit (-1.1f, 1.1f, yAt (lx, sp.mixPct * 0.01f)) * pl.getHeight() * 0.5f);
        g.setColour (col.withAlpha (0.3f)); g.fillEllipse (Rectangle<float> (14, 14).withCentre (dot));
        g.setColour (juce::Colours::white); g.fillEllipse (Rectangle<float> (6, 6).withCentre (dot));
        text (g, Rectangle<float> (pl.getX() + 8, pl.getY() + 6, 200, 14), String (juce::CharPointer_UTF8 ("\xe2\x80\x94  OUTPUT (MIX ")) + String (sp.mixPct, 0) + " %)", 10.0f, Fonts::SemiBold, col, juce::Justification::centredLeft, 0.06f);
        text (g, Rectangle<float> (pl.getX() + 8, pl.getY() + 21, 200, 14), String (juce::CharPointer_UTF8 ("\xe2\x80\x94  SHAPER (100 % WET)")), 10.0f, Fonts::SemiBold, col.withAlpha (0.55f), juce::Justification::centredLeft, 0.06f);
        text (g, Rectangle<float> (pl.getRight() - 40, pl.getBottom() + 2, 40, 12), "IN", 9.5f, Fonts::SemiBold, p.textMute, juce::Justification::centredRight, 0.1f);
        text (g, Rectangle<float> (pl.getX() - 20, pl.getY(), 18, 12), "OUT", 8.5f, Fonts::SemiBold, p.textMute);
    }
    di.removeFromLeft (16);
    vline (g, di.getX(), di.getY() + 8, di.getBottom() - 8, p.edge);
    di.removeFromLeft (20);

    // harmonics
    {
        caption (g, di.removeFromTop (16), "HARMONICS  (RELATIVE TO FUNDAMENTAL)", p, juce::Justification::centredLeft);
        auto pl = di.reduced (30, 10).withTrimmedBottom (14).withTrimmedLeft (6);
        const float lo = -140.0f;
        for (int d = 0; d >= -140; d -= 20)
        {
            const float y = pl.getY() + pl.getHeight() * (d / lo);
            hline (g, y, pl.getX(), pl.getRight(), d == 0 ? p.gridStrong : p.grid);
            text (g, Rectangle<float> (pl.getX() - 38, y - 7, 32, 14), String (d), 10.0f, Fonts::Medium, p.textMute, juce::Justification::centredRight);
        }
        const int H = (int) harmonicsDb.size();
        const float bw = pl.getWidth() / (float) juce::jmax (1, H);
        for (int h = 0; h < H; ++h)
        {
            const float v = juce::jlimit (lo, 0.0f, harmonicsDb[(size_t) h]);
            auto col = Rectangle<float> (pl.getX() + h * bw, pl.getY(), bw, pl.getHeight()).reduced (bw * 0.22f, 0);
            text (g, Rectangle<float> (col.getX() - 10, pl.getBottom() + 2, col.getWidth() + 20, 12), "H" + String (h + 1), 9.5f, Fonts::SemiBold, p.textMute, juce::Justification::centred);
            if (v <= lo) continue;
            auto bar = col.withTop (pl.getY() + pl.getHeight() * (v / lo));
            const auto c = h == 0 ? p.textDim : (on ? mc : p.metal).withMultipliedAlpha (h % 2 == 1 ? 1.0f : 0.72f);
            g.setGradientFill (juce::ColourGradient (c, 0, bar.getY(), c.withAlpha (0.15f), 0, pl.getBottom(), false));
            g.fillRoundedRectangle (bar, 3);
            if (h > 0)
                text (g, Rectangle<float> (col.getX() - 12, bar.getY() - 15, col.getWidth() + 24, 12), String (juce::roundToInt (harmonicsDb[(size_t) h])) + " dB", 9.5f, Fonts::Medium, p.textDim, juce::Justification::centred);
        }
    }
}

// ============================================================================ COMPRESSOR
void CompDisplay::tick()
{
    gr.push (juce::jmax (0.0f, proc.getChain().getCompGainReduction()));
    auto& m = proc.getChain().getInputMeter();
    level.push (toDb (juce::jmax (m.getPeak (0), m.getPeak (1))));
    repaint();
}

void CompDisplay::paint (juce::Graphics& g)
{
    const auto& p = P();
    const auto mc = p.module[2];
    const auto all = bm::applyOneMaster (proc.getCurrentParams());
    const auto& cp = all.comp;
    const bool on = all.moduleOn[bm::kModComp];
    const auto col = on ? mc : p.metal;
    auto bounds = getLocalBounds().toFloat();
    display (g, bounds, p, 12.0f);
    auto di = bounds.reduced (14, 10);
    sectionTitle (g, di.removeFromTop (24).withWidth (220), "MASTER COMPRESSOR", mc);
    di.removeFromTop (8);
    const float grNow = gr.last();

    // transfer
    auto tr = di.removeFromLeft (di.getHeight()).reduced (4);
    {
        caption (g, tr.removeFromTop (16), "TRANSFER", p, juce::Justification::centredLeft);
        auto pl = tr.reduced (22, 6).withTrimmedBottom (10);
        const float side = juce::jmin (pl.getWidth(), pl.getHeight());
        pl = Rectangle<float> (side, side).withCentre (pl.getCentre());
        const float lo = -60.0f;
        auto X = [&] (float d) { return pl.getX() + pl.getWidth() * (1 - d / lo); };
        auto Y = [&] (float d) { return pl.getBottom() - pl.getHeight() * (1 - d / lo); };
        for (int d = -60; d <= 0; d += 10)
        {
            vline (g, X ((float) d), pl.getY(), pl.getBottom(), p.grid);
            hline (g, Y ((float) d), pl.getX(), pl.getRight(), p.grid);
            text (g, Rectangle<float> (X ((float) d) - 14, pl.getBottom() + 2, 28, 12), String (d), 9.5f, Fonts::Medium, p.textMute, juce::Justification::centred);
        }
        g.setColour (col.withAlpha (0.07f));
        g.fillRect (Rectangle<float>::leftTopRightBottom (X (juce::jmax (lo, cp.thresholdDb - cp.kneeDb * 0.5f)), pl.getY(), X (juce::jmin (0.0f, cp.thresholdDb + cp.kneeDb * 0.5f)), pl.getBottom()));
        dashed (g, { X (cp.thresholdDb), pl.getY(), X (cp.thresholdDb), pl.getBottom() }, col.withAlpha (0.6f));
        dashed (g, { pl.getX(), pl.getBottom(), pl.getRight(), pl.getY() }, p.textMute.withAlpha (0.5f));
        Path c;
        for (int i = 0; i <= 200; ++i)
        {
            const float x = lo + (-lo) * i / 200.0f;
            const float y = x - juce::jmin (cp.rangeDb, bm::CompCurve::gainReductionDb (x, cp.thresholdDb, cp.ratio, cp.kneeDb));
            if (i == 0) c.startNewSubPath (X (x), Y (y)); else c.lineTo (X (x), Y (y));
        }
        glowStroke (g, c, on ? p.curve : p.metal, 2.0f, on ? 8.0f : 0.0f);
        // live dot: input level where the static curve gives the measured gain reduction
        float liveIn = juce::jlimit (lo, 0.0f, level.last());
        if (grNow > 0.05f)
            for (float x = lo; x <= 0.0f; x += 0.05f)
                if (bm::CompCurve::gainReductionDb (x, cp.thresholdDb, cp.ratio, cp.kneeDb) >= grNow) { liveIn = x; break; }
        const auto dot = juce::Point<float> (X (liveIn), Y (liveIn - grNow));
        g.setColour (col.withAlpha (0.3f)); g.fillEllipse (Rectangle<float> (16, 16).withCentre (dot));
        g.setColour (col); g.fillEllipse (Rectangle<float> (10, 10).withCentre (dot));
        g.setColour (juce::Colours::white); g.fillEllipse (Rectangle<float> (4, 4).withCentre (dot));
        text (g, Rectangle<float> (X (cp.thresholdDb) + 6, pl.getY() + 4, 120, 13), "THR " + db (cp.thresholdDb, 1), 10.0f, Fonts::SemiBold, col, juce::Justification::centredLeft, 0.05f);
        text (g, Rectangle<float> (pl.getX() + 6, pl.getY() + 4, 100, 13), String (cp.ratio, 1) + " : 1", 11.0f, Fonts::SemiBold, p.text);
        text (g, Rectangle<float> (pl.getX() + 6, pl.getY() + 19, 100, 13), "KNEE " + String (cp.kneeDb, 0) + " dB", 10.0f, Fonts::SemiBold, p.textMute, juce::Justification::centredLeft, 0.05f);
    }
    di.removeFromLeft (14);
    vline (g, di.getX(), di.getY() + 8, di.getBottom() - 8, p.edge);
    di.removeFromLeft (18);

    // meters (right)
    auto mt = di.removeFromRight (128);
    {
        caption (g, mt.removeFromTop (16), "LEVEL / GR", p, juce::Justification::centred);
        auto m = mt.reduced (10, 6).withTrimmedBottom (34);
        const float w = 14, gap = (m.getWidth() - 3 * w) / 2.0f;
        auto mIn = m.removeFromLeft (w); m.removeFromLeft (gap);
        auto mGr = m.removeFromLeft (w); m.removeFromLeft (gap);
        auto mOut = m.removeFromLeft (w);
        const float makeup = cp.makeupDb + (cp.autoGain ? bm::CompCurve::autoMakeupDb (cp.thresholdDb, cp.ratio, cp.kneeDb) : 0.0f);
        const float inPk = level.last(), outPk = inPk - grNow + (on ? makeup : 0.0f);
        meterBar (g, mIn, inPk, p, true, -48.0f);
        meterBar (g, mOut, outPk, p, true, -48.0f);
        g.setColour (p.well); g.fillRoundedRectangle (mGr, 2);
        g.setColour (col); g.fillRoundedRectangle (mGr.withHeight (mGr.getHeight() * juce::jlimit (0.0f, 1.0f, grNow / 24.0f)), 2);
        auto lab = mt.removeFromBottom (32);
        const char* L3[3] = { "IN", "GR", "OUT" };
        const String V3[3] = { inPk < -99 ? String ("-inf") : String (inPk, 1), String (-grNow, 1), outPk < -99 ? String ("-inf") : String (outPk, 1) };
        const float xs[3] = { mIn.getCentreX(), mGr.getCentreX(), mOut.getCentreX() };
        for (int i = 0; i < 3; ++i)
        {
            text (g, Rectangle<float> (xs[i] - 24, lab.getY(), 48, 14), L3[i], 9.5f, Fonts::SemiBold, p.textMute, juce::Justification::centred, 0.1f);
            text (g, Rectangle<float> (xs[i] - 24, lab.getY() + 15, 48, 14), trimNegativeZero (V3[i]), 10.5f, Fonts::Medium, i == 1 ? col : p.text, juce::Justification::centred);
        }
    }
    di.removeFromRight (14);
    vline (g, di.getRight(), di.getY() + 8, di.getBottom() - 8, p.edge);
    di.removeFromRight (14);

    // level & GR history
    {
        auto hd = di.removeFromTop (16);
        caption (g, hd, String (juce::CharPointer_UTF8 ("LEVEL & GAIN REDUCTION  \xc2\xb7  5 s")), p, juce::Justification::centredLeft);
        auto lg = hd.removeFromRight (200);
        legend (g, lg, p.specPre.brighter (0.6f), "INPUT");
        legend (g, lg, col, "GAIN REDUCTION");
        auto pl = di.reduced (0, 8).withTrimmedRight (28);
        const float lo = -36.0f;
        auto Y = [&] (float d) { return pl.getY() + pl.getHeight() * juce::jlimit (0.0f, 1.0f, d / lo); };
        for (int d = 0; d >= -36; d -= 6)
        {
            hline (g, Y ((float) d), pl.getX(), pl.getRight(), p.grid);
            text (g, Rectangle<float> (pl.getRight() + 4, Y ((float) d) - 7, 26, 14), String (d), 10.0f, Fonts::Medium, p.textMute);
        }
        dashed (g, { pl.getX(), Y (cp.thresholdDb), pl.getRight(), Y (cp.thresholdDb) }, col.withAlpha (0.5f));
        Path area, grLine, grArea;
        area.startNewSubPath (pl.getX(), pl.getBottom());
        grArea.startNewSubPath (pl.getX(), pl.getY());
        for (int i = 0; i < History::kLen; ++i)
        {
            const float x = pl.getX() + pl.getWidth() * i / (float) (History::kLen - 1);
            area.lineTo (x, Y (level.at (i)));
            const float gy = Y (-gr.at (i));
            grArea.lineTo (x, gy);
            if (i == 0) grLine.startNewSubPath (x, gy); else grLine.lineTo (x, gy);
        }
        area.lineTo (pl.getRight(), pl.getBottom()); area.closeSubPath();
        grArea.lineTo (pl.getRight(), pl.getY()); grArea.closeSubPath();
        g.setGradientFill (juce::ColourGradient (p.specPre.brighter (0.5f).withAlpha (0.7f), 0, pl.getY(), p.specPre.withAlpha (0.25f), 0, pl.getBottom(), false));
        g.fillPath (area);
        g.setColour (col.withAlpha (0.16f)); g.fillPath (grArea);
        glowStroke (g, grLine, col, 1.6f, on ? 6.0f : 0.0f);
    }
}

// ============================================================================ LIMITER
LimDisplay::LimDisplay (BankMasterProcessor& p) : proc (p)
{
    addAndMakeVisible (resetButton);
    resetButton.setTooltip ("Reset the loudness, true-peak and GR-max readings");
    resetButton.onClick = [this]
    {
        proc.getChain().getLoudness().requestReset();
        proc.getChain().getOutputMeter().resetTruePeak();
        grMax = 0.0f;
    };
}

Rectangle<float> LimDisplay::loudnessPanel() const
{
    auto di = getLocalBounds().toFloat().reduced (14, 10);
    di.removeFromTop (32);
    return di.removeFromRight (250);
}

void LimDisplay::resized()
{
    const auto lp = loudnessPanel();
    resetButton.setBounds (Rectangle<float> (64, 22).withPosition (lp.getRight() - 64 - 14, lp.getY() + 12).toNearestInt());
}

void LimDisplay::tick()
{
    const float g = juce::jmax (0.0f, proc.getChain().getLimGainReduction());
    gr.push (g);
    grMax = juce::jmax (grMax, g);
    auto& m = proc.getChain().getOutputMeter();
    out.push (toDb (juce::jmax (m.getPeak (0), m.getPeak (1))));
    repaint();
}

void LimDisplay::paint (juce::Graphics& g)
{
    const auto& p = P();
    const auto mc = p.module[3];
    const auto all = proc.getCurrentParams();
    const auto& lp = all.lim;
    const bool on = all.moduleOn[bm::kModLim];
    const auto col = on ? mc : p.metal;
    auto bounds = getLocalBounds().toFloat();
    display (g, bounds, p, 12.0f);
    auto di = bounds.reduced (14, 10);
    sectionTitle (g, di.removeFromTop (24).withWidth (200), "MASTER LIMITER", mc);
    di.removeFromTop (8);

    // loudness panel (right)
    auto lu = di.removeFromRight (250);
    {
        auto& loud = proc.getChain().getLoudness();
        auto lufs = [] (float v) { return std::isfinite (v) && v > -99.0f ? String (v, 1) : String ("-inf"); };
        g.setColour (p.panel.withAlpha (0.6f)); g.fillRoundedRectangle (lu, 10);
        g.setColour (p.edge); g.drawRoundedRectangle (lu.reduced (0.5f), 10, 1);
        auto in = lu.reduced (18, 14);
        caption (g, in.removeFromTop (16), "INTEGRATED", p, juce::Justification::centredLeft);
        auto big = in.removeFromTop (52);
        const auto iv = lufs (loud.getIntegrated());
        g.setFont (font (42.0f, Fonts::SemiBold, -0.01f));
        const float wNum = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), iv);
        text (g, big, iv, 42.0f, Fonts::SemiBold, p.text, juce::Justification::centredLeft, -0.01f);
        text (g, big.withTrimmedLeft (wNum + 10).withTrimmedTop (16), "LUFS", 12.0f, Fonts::SemiBold, p.textDim, juce::Justification::centredLeft, 0.1f);
        in.removeFromTop (8);
        auto row = [&] (const String& l, const String& v, juce::Colour c)
        {
            auto rr = in.removeFromTop (25);
            hline (g, rr.getY(), rr.getX(), rr.getRight(), p.edge);
            caption (g, rr, l, p, juce::Justification::centredLeft);
            text (g, rr, v, 12.5f, Fonts::Medium, c, juce::Justification::centredRight);
        };
        const float tp = toDb (proc.getChain().getOutputMeter().getTruePeakMax());
        const double fs = proc.getCurrentSampleRate();
        row ("SHORT-TERM", lufs (loud.getShortTerm()) + " LUFS", p.text);
        row ("MOMENTARY", lufs (loud.getMomentary()) + " LUFS", p.text);
        row ("TRUE PEAK MAX", (tp > -99 ? trimNegativeZero (String (tp, 2)) : String ("-inf")) + " dBTP", tp > lp.ceilingDb + 0.05f ? p.danger : p.text);
        row ("GR MAX", trimNegativeZero (String (-grMax, 1)) + " dB", col);
        row ("LATENCY", String (proc.getLatencySamples()) + String (juce::CharPointer_UTF8 (" smp \xc2\xb7 ")) + String (proc.getLatencySamples() * 1000.0 / fs, 1) + " ms", p.textDim);
        row ("TRUE PEAK OS", lp.truePeak ? String (proc.getChain().getLimiterOsFactor()) + "x" : String ("OFF"), p.textDim);
    }
    di.removeFromRight (16);

    // output / input / GR history
    {
        auto hd = di.removeFromTop (16);
        caption (g, hd, String (juce::CharPointer_UTF8 ("OUTPUT  \xc2\xb7  GAIN REDUCTION  \xc2\xb7  5 s")), p, juce::Justification::centredLeft);
        auto lg = hd.removeFromRight (230);
        legend (g, lg, p.textMute, "INPUT");
        legend (g, lg, p.accent, "OUTPUT");
        legend (g, lg, col, "GR");

        auto pl = di.reduced (0, 8).withTrimmedRight (30);
        const float lo = -27.0f, top = 9.0f;
        auto Y = [&] (float d) { return pl.getY() + pl.getHeight() * juce::jlimit (0.0f, 1.0f, (top - d) / (top - lo)); };
        for (int d = 9; d >= -27; d -= 3)
        {
            hline (g, Y ((float) d), pl.getX(), pl.getRight(), d == 0 ? p.gridStrong : p.grid);
            text (g, Rectangle<float> (pl.getRight() + 4, Y ((float) d) - 7, 28, 14), d > 0 ? "+" + String (d) : String (d), 10.0f, Fonts::Medium, p.textMute);
        }
        Path inA, outA, outL, grA, grL;
        inA.startNewSubPath (pl.getX(), pl.getBottom());
        outA.startNewSubPath (pl.getX(), pl.getBottom());
        grA.startNewSubPath (pl.getX(), Y (top));
        for (int i = 0; i < History::kLen; ++i)
        {
            const float x = pl.getX() + pl.getWidth() * i / (float) (History::kLen - 1);
            const float o = out.at (i), gdb = gr.at (i);
            inA.lineTo (x, Y (o + gdb));
            outA.lineTo (x, Y (o));
            if (i == 0) outL.startNewSubPath (x, Y (o)); else outL.lineTo (x, Y (o));
            const float gy = Y (top - gdb);   // GR hangs from the top on the same dB scale
            grA.lineTo (x, gy);
            if (i == 0) grL.startNewSubPath (x, gy); else grL.lineTo (x, gy);
        }
        for (auto* pa : { &inA, &outA }) { pa->lineTo (pl.getRight(), pl.getBottom()); pa->closeSubPath(); }
        grA.lineTo (pl.getRight(), Y (top)); grA.closeSubPath();
        // light fills so the curves and the gain reduction stay in front
        g.setColour (p.textMute.withAlpha (0.22f)); g.fillPath (inA);
        g.setGradientFill (juce::ColourGradient (p.accent.withAlpha (0.20f), 0, Y (0), p.accent.withAlpha (0.03f), 0, pl.getBottom(), false));
        g.fillPath (outA);
        g.setColour (p.accent.withAlpha (0.85f));
        g.strokePath (outL, juce::PathStrokeType (1.2f));
        g.setColour (col.withAlpha (0.14f)); g.fillPath (grA);
        glowStroke (g, grL, col, 1.8f, on ? 6.0f : 0.0f);
        // ceiling
        const float cy = Y (lp.ceilingDb);
        g.setColour (p.text.withAlpha (0.85f)); g.fillRect (pl.getX(), cy - 0.5f, pl.getWidth(), 1.2f);
        auto tag = Rectangle<float> (138, 18).withPosition (pl.getX() + 8, cy + 5);
        g.setColour (p.panelHi.withAlpha (0.92f)); g.fillRoundedRectangle (tag, 4);
        text (g, tag, "CEILING " + trimNegativeZero (String (lp.ceilingDb, 1)) + (lp.truePeak ? " dBTP" : " dBFS"), 10.0f, Fonts::SemiBold, p.text, juce::Justification::centred, 0.05f);
    }
}

} // namespace bmui
