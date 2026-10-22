// BANKMASTER by KALOX — EQ display: real-time spectrum, response curve, draggable nodes
#include "EqGraph.h"

// ============================================================================ analyzer
SpectrumAnalyzer::SpectrumAnalyzer()
    : ring ((size_t) kSize, 0.0f), scratch ((size_t) kSize, 0.0f), fftData ((size_t) kSize * 2, 0.0f), smoothed ((size_t) kSize / 2, -120.0f)
{
}

void SpectrumAnalyzer::update (BankMasterProcessor& p, bool isPost)
{
    float tmp[2048];
    int got = 0;
    while ((got = p.pullAnalyzer (isPost, tmp, 2048)) > 0)
    {
        for (int i = 0; i < got; ++i) { ring[(size_t) ringPos] = tmp[i]; ringPos = (ringPos + 1) % kSize; }
        filled = true;
    }
    if (! filled) return;
    for (int i = 0; i < kSize; ++i) fftData[(size_t) i] = ring[(size_t) ((ringPos + i) % kSize)];
    std::fill (fftData.begin() + kSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) kSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());
    const float norm = 4.0f / (float) kSize; // Hann coherent gain compensation, one-sided
    for (int k = 0; k < kSize / 2; ++k)
    {
        const float db = juce::Decibels::gainToDecibels (fftData[(size_t) k] * norm, -120.0f);
        float& s = smoothed[(size_t) k];
        s = db > s ? db : s + (db - s) * 0.25f; // fast attack, smooth fall
    }
}

float SpectrumAnalyzer::levelAt (double freq, double fs) const
{
    const double bin = freq / fs * kSize;
    const double lo = bin * 0.971, hi = bin * 1.03;   // 1/12-octave neighbourhood
    if (hi - lo < 1.0)
    {
        // fewer than one bin per pixel (low frequencies): interpolate between bins
        const int k0 = juce::jlimit (1, kSize / 2 - 2, (int) std::floor (bin));
        const float t = (float) juce::jlimit (0.0, 1.0, bin - k0);
        return smoothed[(size_t) k0] + t * (smoothed[(size_t) k0 + 1] - smoothed[(size_t) k0]);
    }
    float m = -120.0f;
    for (int k = juce::jlimit (1, kSize / 2 - 1, (int) std::floor (lo)); k <= juce::jlimit (1, kSize / 2 - 1, (int) std::ceil (hi)); ++k)
        m = juce::jmax (m, smoothed[(size_t) k]);
    return m;
}

// ============================================================================ graph
EqGraph::EqGraph (BankMasterProcessor& p) : proc (p)
{
    cached = proc.getCurrentParams();
    setOpaque (false);
}

juce::Rectangle<float> EqGraph::plotArea() const { return getLocalBounds().toFloat().reduced (2.0f).withTrimmedLeft (4).withTrimmedRight (36).withTrimmedBottom (20).withTrimmedTop (6); }

float EqGraph::freqToX (double f) const
{
    const auto a = plotArea();
    return a.getX() + a.getWidth() * (float) (std::log (f / 20.0) / std::log (1000.0));
}
double EqGraph::xToFreq (float x) const
{
    const auto a = plotArea();
    return 20.0 * std::pow (1000.0, (x - a.getX()) / a.getWidth());
}
float EqGraph::dbToY (float db) const
{
    const auto a = plotArea();
    return a.getCentreY() - db / kDbRange * a.getHeight() * 0.5f;
}
float EqGraph::yToDb (float y) const
{
    const auto a = plotArea();
    return (a.getCentreY() - y) / (a.getHeight() * 0.5f) * kDbRange;
}
float EqGraph::specToY (float db) const
{
    const auto a = plotArea();
    return a.getY() + a.getHeight() * juce::jlimit (0.0f, 1.0f, -db / 96.0f); // 0..-96 dBFS
}

juce::Point<float> EqGraph::nodePos (int band, const bm::ChainParams& p) const
{
    const auto& b = p.eq.bands[(size_t) band];
    const float g = bm::bandSupportsGain (b.type) ? b.gainDb : 0.0f;
    return { freqToX (b.freq), dbToY (juce::jlimit (-kDbRange, kDbRange, g)) };
}

int EqGraph::hitNode (juce::Point<float> pt) const
{
    int best = -1; float bestD = 14.0f;
    for (int b = 0; b < bm::kNumEqBands; ++b)
    {
        const float d = nodePos (b, cached).getDistanceFrom (pt);
        if (d < bestD) { bestD = d; best = b; }
    }
    return best;
}

void EqGraph::tick()
{
    pre.update (proc, false);
    post.update (proc, true);
    cached = bm::applyOneMaster (proc.getCurrentParams());
    for (int b = 0; b < bm::kNumEqBands; ++b) dynGain[b] = proc.getChain().getEq().getCurrentGainDb (b);
    repaint();
}

void EqGraph::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    theme::drawDisplay (g, bounds);
    const auto a = plotArea();
    const double fs = proc.getCurrentSampleRate();

    // grid
    g.setFont (theme::font (10.5f));
    for (double f : { 20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 90.0, 100.0, 200.0, 300.0, 400.0, 500.0, 600.0, 700.0, 800.0, 900.0,
                      1000.0, 2000.0, 3000.0, 4000.0, 5000.0, 6000.0, 7000.0, 8000.0, 9000.0, 10000.0, 20000.0 })
    {
        const float x = freqToX (f);
        const bool major = f == 20 || f == 50 || f == 100 || f == 200 || f == 500 || f == 1000 || f == 2000 || f == 5000 || f == 10000 || f == 20000;
        g.setColour (major ? theme::gridStrong : theme::grid);
        g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
        if (major)
        {
            g.setColour (theme::textOnDark.withAlpha (0.55f));
            const juce::String t = f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f);
            g.drawText (t, juce::Rectangle<float> (x - 20, a.getBottom() + 3, 40, 14), juce::Justification::centred);
        }
    }
    for (float db = -12; db <= 12; db += 3)
    {
        const float y = dbToY (db);
        g.setColour (db == 0 ? theme::gridStrong.withAlpha (0.45f) : theme::grid);
        g.drawHorizontalLine ((int) y, a.getX(), a.getRight());
        g.setColour (theme::ice.withAlpha (0.75f));
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (a.getRight() + 4, y - 7, 30, 14), juce::Justification::centredLeft);
    }

    // spectrum (pre = faint, post = ice fill)
    auto spectrumPath = [&] (const SpectrumAnalyzer& s, bool closed)
    {
        juce::Path p;
        bool started = false;
        for (float x = a.getX(); x <= a.getRight(); x += 2.0f)
        {
            const double f = xToFreq (x);
            if (f >= fs * 0.5) break;
            const float y = specToY (s.levelAt (f, fs));
            if (! started) { p.startNewSubPath (x, closed ? a.getBottom() : y); if (closed) p.lineTo (x, y); started = true; }
            else p.lineTo (x, y);
        }
        if (closed && started) { p.lineTo (p.getCurrentPosition().x, a.getBottom()); p.closeSubPath(); }
        return p;
    };
    if (pre.hasData())
    {
        g.setColour (theme::textOnDark.withAlpha (0.18f));
        g.strokePath (spectrumPath (pre, false), juce::PathStrokeType (1.0f));
    }
    if (post.hasData())
    {
        auto p = spectrumPath (post, true);
        g.setGradientFill (juce::ColourGradient (theme::ice.withAlpha (0.28f), 0, a.getY(), theme::ice.withAlpha (0.04f), 0, a.getBottom(), false));
        g.fillPath (p);
        g.setColour (theme::ice.withAlpha (0.45f));
        g.strokePath (spectrumPath (post, false), juce::PathStrokeType (1.0f));
    }

    // selected band shape
    const int W = (int) a.getWidth();
    if (selected >= 0 && cached.eq.bands[(size_t) selected].enabled)
    {
        juce::Path shape;
        const auto& b = cached.eq.bands[(size_t) selected];
        shape.startNewSubPath (a.getX(), dbToY (0));
        for (int i = 0; i <= W; i += 2)
        {
            const float x = a.getX() + (float) i;
            const float db = (float) bm::bandMagnitudeDb (b, xToFreq (x), fs);
            shape.lineTo (x, dbToY (juce::jlimit (-kDbRange - 2, kDbRange + 2, db)));
        }
        shape.lineTo (a.getRight(), dbToY (0));
        shape.closeSubPath();
        g.setColour (theme::bandColour (selected).withAlpha (0.18f));
        g.fillPath (shape);
    }

    // total response: static (dashed when dynamics move it) + live dynamic curve
    auto totalCurve = [&] (bool live)
    {
        juce::Path c;
        for (int i = 0; i <= W; i += 2)
        {
            const float x = a.getX() + (float) i;
            const double f = xToFreq (x);
            double db = 0;
            for (int b = 0; b < bm::kNumEqBands; ++b)
            {
                const auto& bd = cached.eq.bands[(size_t) b];
                const bool dyn = live && bd.dynOn && bm::bandSupportsDynamics (bd.type);
                db += bm::bandMagnitudeDb (bd, f, fs, dyn ? dynGain[b] : 1e9f);
            }
            const float y = dbToY (juce::jlimit (-kDbRange - 3, kDbRange + 3, (float) db));
            if (i == 0) c.startNewSubPath (x, y); else c.lineTo (x, y);
        }
        return c;
    };
    const bool eqActive = cached.moduleOn[bm::kModEq];
    g.setColour ((eqActive ? juce::Colours::white : theme::steel).withAlpha (0.35f));
    g.strokePath (totalCurve (false), juce::PathStrokeType (1.2f));
    g.setColour (eqActive ? juce::Colours::white : theme::steel);
    g.strokePath (totalCurve (true), juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // nodes
    for (int b = 0; b < bm::kNumEqBands; ++b)
    {
        const auto& bd = cached.eq.bands[(size_t) b];
        const auto pos = nodePos (b, cached);
        const auto col = theme::bandColour (b);
        const float r = (b == selected ? 9.0f : 7.5f) + (b == hover ? 1.0f : 0.0f);
        if (bd.dynOn && bm::bandSupportsDynamics (bd.type) && bd.enabled)
        {
            // dynamic range indicator
            const float y2 = dbToY (juce::jlimit (-kDbRange, kDbRange, bd.gainDb + bd.dynRangeDb));
            g.setColour (col.withAlpha (0.6f));
            g.drawLine (pos.x, pos.y, pos.x, y2, 2.0f);
            g.fillEllipse (pos.x - 3, y2 - 3, 6, 6);
        }
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (pos.x - r, pos.y - r + 1.5f, r * 2, r * 2);
        g.setColour (bd.enabled ? col : col.withSaturation (0.0f).withAlpha (0.5f));
        g.fillEllipse (pos.x - r, pos.y - r, r * 2, r * 2);
        g.setColour (b == selected ? juce::Colours::white : juce::Colours::black.withAlpha (0.4f));
        g.drawEllipse (pos.x - r, pos.y - r, r * 2, r * 2, b == selected ? 2.0f : 1.0f);
        g.setColour (juce::Colour (0xff10202e));
        g.setFont (theme::font (10.5f, true));
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (pos.x - r, pos.y - r, r * 2, r * 2), juce::Justification::centred);
    }

    // read-out for the selected band
    if (selected >= 0)
    {
        const auto& b = cached.eq.bands[(size_t) selected];
        juce::String t = juce::String (b.freq < 1000 ? juce::String (b.freq, 2) + " Hz" : juce::String (b.freq / 1000.0f, 3) + " kHz");
        if (bm::bandSupportsGain (b.type)) t << "   " << (b.gainDb > 0 ? "+" : "") << juce::String (b.gainDb, 2) << " dB";
        t << "   Q " << juce::String (b.q, 3);
        if (b.type == bm::BandType::LowCut || b.type == bm::BandType::HighCut || b.type == bm::BandType::LowShelf || b.type == bm::BandType::HighShelf)
            t << "   " << bm::slopeDbPerOct (b.slopeIdx) << " dB/oct";
        g.setFont (theme::font (12.5f, true));
        g.setColour (theme::bandColour (selected));
        g.drawText ("BAND " + juce::String (selected + 1) + "   " + t, juce::Rectangle<float> (a.getX() + 10, a.getY() + 6, 520, 18), juce::Justification::centredLeft);
    }
    g.setFont (theme::font (10.5f));
    g.setColour (theme::textOnDark.withAlpha (0.5f));
    g.drawText ("ZERO LATENCY  |  ANALYZER PRE + POST", juce::Rectangle<float> (a.getRight() - 300, a.getY() + 6, 292, 16), juce::Justification::centredRight);
}

void EqGraph::setParam (const juce::String& pid, float value)
{
    if (auto* p = proc.getState().getParameter (pid))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void EqGraph::beginGesture (int band)
{
    for (auto* w : { "freq", "gain", "q" })
        if (auto* p = proc.getState().getParameter (bmp::id::eq (band, w))) p->beginChangeGesture();
}
void EqGraph::endGesture (int band)
{
    for (auto* w : { "freq", "gain", "q" })
        if (auto* p = proc.getState().getParameter (bmp::id::eq (band, w))) p->endChangeGesture();
}

void EqGraph::mouseDown (const juce::MouseEvent& e)
{
    const int hit = hitNode (e.position);
    if (hit >= 0)
    {
        selected = hit;
        dragging = hit;
        beginGesture (hit);
        if (onBandSelected) onBandSelected (hit);
        repaint();
    }
}

void EqGraph::mouseDrag (const juce::MouseEvent& e)
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

void EqGraph::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0) endGesture (dragging);
    dragging = -1;
}

void EqGraph::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int hit = hitNode (e.position);
    if (hit >= 0)
    {
        if (auto* p = proc.getState().getParameter (bmp::id::eq (hit, "on")))
            p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
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

void EqGraph::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    int band = hitNode (e.position);
    if (band < 0) band = selected;
    if (band < 0) return;
    const float q = cached.eq.bands[(size_t) band].q;
    setParam (bmp::id::eq (band, "q"), juce::jlimit (0.025f, 40.0f, q * std::pow (1.15f, w.deltaY * 4.0f)));
}

void EqGraph::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}
