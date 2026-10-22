// BANKMASTER by KALOX — the four processor pages
#include "Pages.h"

namespace
{
    juce::String fmtDb (float v, int dec = 1)
    {
        if (! std::isfinite (v) || v < -99.0f) return "-inf";
        return (v > 0 ? "+" : "") + juce::String (v, dec);
    }

    void sectionTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t)
    {
        g.setColour (theme::iceDeep);
        g.setFont (theme::font (12.0f, true));
        g.drawText (t, r, juce::Justification::centredLeft);
    }
}

// ============================================================================ EQ
EqPage::EqPage (BankMasterProcessor& p) : proc (p), graph (p)
{
    addAndMakeVisible (graph);
    graph.onBandSelected = [this] (int b) { selectBand (b); };
    for (int b = 0; b < bm::kNumEqBands; ++b)
    {
        auto& bt = bandButtons[b];
        bt.setButtonText (juce::String (b + 1));
        bt.setClickingTogglesState (false);
        bt.setColour (juce::TextButton::buttonOnColourId, theme::bandColour (b).darker (0.35f));
        bt.onClick = [this, b] { selectBand (b); };
        addAndMakeVisible (bt);
    }
    selectBand (1);
}

void EqPage::selectBand (int b)
{
    band = b;
    graph.setSelectedBand (b);
    for (int i = 0; i < bm::kNumEqBands; ++i) bandButtons[i].setToggleState (i == b, juce::dontSendNotification);
    auto& s = proc.getState();
    const auto col = theme::bandColour (b).darker (0.3f);
    bandOn = std::make_unique<Toggle> (s, bmp::id::eq (b, "on"), "BAND ON", col);
    dynOn = std::make_unique<Toggle> (s, bmp::id::eq (b, "dyn"), "DYNAMIC", col);
    type = std::make_unique<Choice> (s, bmp::id::eq (b, "type"), "Type");
    slope = std::make_unique<Choice> (s, bmp::id::eq (b, "slope"), "Slope");
    place = std::make_unique<Choice> (s, bmp::id::eq (b, "place"), "Stereo / M-S");
    freq = std::make_unique<Knob> (s, bmp::id::eq (b, "freq"), "Freq", col);
    gain = std::make_unique<Knob> (s, bmp::id::eq (b, "gain"), "Gain", col);
    q = std::make_unique<Knob> (s, bmp::id::eq (b, "q"), "Q", col);
    range = std::make_unique<Knob> (s, bmp::id::eq (b, "range"), "Dyn Range", col);
    thr = std::make_unique<Knob> (s, bmp::id::eq (b, "thr"), "Threshold", col);
    for (juce::Component* c : { (juce::Component*) bandOn.get(), (juce::Component*) dynOn.get(), (juce::Component*) type.get(),
                                (juce::Component*) slope.get(), (juce::Component*) place.get(), (juce::Component*) freq.get(),
                                (juce::Component*) gain.get(), (juce::Component*) q.get(), (juce::Component*) range.get(), (juce::Component*) thr.get() })
        addAndMakeVisible (c);
    resized();
    updateEnablement();
    repaint();
}

void EqPage::updateEnablement()
{
    const auto p = proc.getCurrentParams();
    const auto& bd = p.eq.bands[(size_t) band];
    const bool gainType = bm::bandSupportsGain (bd.type);
    const bool slopeType = bd.type == bm::BandType::LowCut || bd.type == bm::BandType::HighCut
                        || bd.type == bm::BandType::LowShelf || bd.type == bm::BandType::HighShelf;
    if (gain) gain->setEnabled (gainType);
    if (slope) slope->setEnabled (slopeType);
    if (dynOn) dynOn->setEnabled (bm::bandSupportsDynamics (bd.type));
    const bool dyn = bd.dynOn && bm::bandSupportsDynamics (bd.type);
    if (range) range->setEnabled (dyn);
    if (thr) thr->setEnabled (dyn);
}

void EqPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    r.removeFromTop (r.getHeight() - 140);
    theme::drawPanel (g, r.toFloat());
    sectionTitle (g, r.reduced (14, 0).removeFromTop (24).translated (0, 4), "BAND");
}

void EqPage::resized()
{
    auto r = getLocalBounds();
    auto bottom = r.removeFromBottom (140);
    r.removeFromBottom (10);
    graph.setBounds (r);

    auto c = bottom.reduced (14, 10);
    c.removeFromTop (22);
    auto bandCol = c.removeFromLeft (150);
    auto row = bandCol.removeFromTop (30);
    for (int b = 0; b < bm::kNumEqBands; ++b) { bandButtons[b].setBounds (row.removeFromLeft (28)); row.removeFromLeft (2); }
    bandCol.removeFromTop (8);
    if (bandOn) bandOn->setBounds (bandCol.removeFromTop (30).withWidth (142));
    c.removeFromLeft (10);
    auto combos = c.removeFromLeft (150);
    if (type) type->setBounds (combos.removeFromTop (44));
    if (slope) slope->setBounds (combos.removeFromTop (44));
    c.removeFromLeft (10);
    auto combos2 = c.removeFromLeft (150);
    if (place) place->setBounds (combos2.removeFromTop (44));
    combos2.removeFromTop (14);
    if (dynOn) dynOn->setBounds (combos2.removeFromTop (30));
    c.removeFromLeft (16);
    if (freq) layoutRow (c, { freq.get(), gain.get(), q.get(), range.get(), thr.get() }, 10);
}

// ============================================================================ SATURATION
void SatCurve::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    theme::drawDisplay (g, b);
    const auto a = b.reduced (18.0f);
    const auto p = bm::applyOneMaster (proc.getCurrentParams());
    g.setColour (theme::grid);
    for (int i = 1; i < 4; ++i)
    {
        g.drawVerticalLine ((int) (a.getX() + a.getWidth() * i / 4), a.getY(), a.getBottom());
        g.drawHorizontalLine ((int) (a.getY() + a.getHeight() * i / 4), a.getX(), a.getRight());
    }
    g.setColour (theme::gridStrong);
    g.drawLine (a.getX(), a.getBottom(), a.getRight(), a.getY(), 1.0f); // unity
    // transfer curve of the wet path for input -1..+1 (drive applied, output-compensated)
    const float drv = std::pow (10.0f, bm::Shaper::driveDb (p.sat.drivePct) / 20.0f);
    const float mix = p.sat.mixPct * 0.01f;
    juce::Path curve;
    for (int i = 0; i <= 200; ++i)
    {
        const float x = -1.0f + 2.0f * i / 200.0f;
        const float wet = bm::Shaper::curve (p.sat.style, x * drv) / drv;
        const float y = x + mix * (wet - x);
        const float px = a.getX() + (x + 1) * 0.5f * a.getWidth();
        const float py = a.getBottom() - (juce::jlimit (-1.2f, 1.2f, y) + 1) * 0.5f * a.getHeight();
        if (i == 0) curve.startNewSubPath (px, py); else curve.lineTo (px, py);
    }
    g.setColour (theme::ice.withAlpha (0.3f));
    g.strokePath (curve, juce::PathStrokeType (6.0f));
    g.setColour (theme::ice);
    g.strokePath (curve, juce::PathStrokeType (2.2f));
    g.setColour (theme::textOnDark.withAlpha (0.6f));
    g.setFont (theme::font (11.0f));
    g.drawText ("TRANSFER CURVE  |  " + bmp::satStyleNames()[(int) p.sat.style].toUpperCase() + "  |  DRIVE " + juce::String (p.sat.drivePct, 1) + " %  |  MIX " + juce::String (p.sat.mixPct, 0) + " %",
                b.reduced (12, 6).removeFromTop (16), juce::Justification::centredLeft);
    g.drawText ("OVERSAMPLING " + bmp::osNames()[p.sat.osFactorLog2] + "  |  LINEAR-PHASE FIR", b.reduced (12, 6).removeFromBottom (16), juce::Justification::centredRight);
}

SatPage::SatPage (BankMasterProcessor& p)
    : curve (p),
      style (p.getState(), bmp::id::satStyle, "Style"), os (p.getState(), bmp::id::satOs, "Oversampling"),
      hq (p.getState(), bmp::id::satHq, "HIGH QUALITY"),
      drive (p.getState(), bmp::id::satDrive, "Drive"), mix (p.getState(), bmp::id::satMix, "Mix"),
      tone (p.getState(), bmp::id::satTone, "Tone"), dyn (p.getState(), bmp::id::satDyn, "Dynamics"),
      fb (p.getState(), bmp::id::satFb, "Feedback"), fbFreq (p.getState(), bmp::id::satFbFreq, "FB Freq"),
      out (p.getState(), bmp::id::satOut, "Output")
{
    for (juce::Component* c : { (juce::Component*) &curve, (juce::Component*) &style, (juce::Component*) &os, (juce::Component*) &hq,
                                (juce::Component*) &drive, (juce::Component*) &mix, (juce::Component*) &tone, (juce::Component*) &dyn,
                                (juce::Component*) &fb, (juce::Component*) &fbFreq, (juce::Component*) &out })
        addAndMakeVisible (c);
}

void SatPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    r.removeFromLeft (r.getWidth() * 42 / 100 + 10);
    theme::drawPanel (g, r.toFloat());
    sectionTitle (g, r.reduced (16, 0).removeFromTop (30), "MASTER SATURATION");
}

void SatPage::resized()
{
    auto r = getLocalBounds();
    curve.setBounds (r.removeFromLeft (r.getWidth() * 42 / 100));
    r.removeFromLeft (10);
    auto c = r.reduced (16, 12);
    c.removeFromTop (22);
    auto top = c.removeFromTop (46);
    style.setBounds (top.removeFromLeft (180)); top.removeFromLeft (12);
    os.setBounds (top.removeFromLeft (140)); top.removeFromLeft (12);
    hq.setBounds (top.removeFromLeft (130).withTrimmedTop (16));
    c.removeFromTop (12);
    auto big = c.removeFromLeft (c.getWidth() / 3);
    drive.setBounds (big.withSizeKeepingCentre (juce::jmin (big.getWidth(), 210), juce::jmin (big.getHeight(), 230)));
    auto rows = c;
    auto row1 = rows.removeFromTop (rows.getHeight() / 2);
    layoutRow (row1.reduced (4), { &mix, &tone, &dyn }, 8);
    layoutRow (rows.reduced (4), { &fb, &fbFreq, &out }, 8);
}

// ============================================================================ COMPRESSOR
void CompDisplay::tick()
{
    gr.push (proc.getChain().getCompGainReduction());
    level.push (juce::Decibels::gainToDecibels (juce::jmax (proc.getChain().getOutputMeter().getPeak (0), proc.getChain().getOutputMeter().getPeak (1)), -60.0f));
    repaint();
}

void CompDisplay::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    theme::drawDisplay (g, b);
    const auto p = bm::applyOneMaster (proc.getCurrentParams());

    // transfer curve (square, left)
    auto sq = b.reduced (16.0f);
    sq = sq.withWidth (sq.getHeight());
    g.setColour (theme::grid);
    for (int i = 1; i < 6; ++i)
    {
        g.drawVerticalLine ((int) (sq.getX() + sq.getWidth() * i / 6), sq.getY(), sq.getBottom());
        g.drawHorizontalLine ((int) (sq.getY() + sq.getHeight() * i / 6), sq.getX(), sq.getRight());
    }
    auto toX = [&] (float db) { return sq.getX() + (db + 60.0f) / 60.0f * sq.getWidth(); };
    auto toY = [&] (float db) { return sq.getBottom() - (db + 60.0f) / 60.0f * sq.getHeight(); };
    g.setColour (theme::gridStrong);
    g.drawLine (toX (-60), toY (-60), toX (0), toY (0), 1.0f);
    juce::Path c;
    for (int i = 0; i <= 240; ++i)
    {
        const float x = -60.0f + 60.0f * i / 240.0f;
        const float y = x - juce::jmin (p.comp.rangeDb, bm::CompCurve::gainReductionDb (x, p.comp.thresholdDb, p.comp.ratio, p.comp.kneeDb));
        if (i == 0) c.startNewSubPath (toX (x), toY (y)); else c.lineTo (toX (x), toY (y));
    }
    g.setColour (theme::ice);
    g.strokePath (c, juce::PathStrokeType (2.2f));
    g.setColour (theme::warn.withAlpha (0.6f));
    const float tx = toX (p.comp.thresholdDb);
    for (float y = sq.getY(); y < sq.getBottom(); y += 6) g.drawLine (tx, y, tx, y + 3, 1.0f);
    g.setColour (theme::textOnDark.withAlpha (0.6f));
    g.setFont (theme::font (10.5f));
    g.drawText ("THR " + juce::String (p.comp.thresholdDb, 1) + " dB   " + juce::String (p.comp.ratio, 2) + ":1   KNEE " + juce::String (p.comp.kneeDb, 1),
                sq.withHeight (16).translated (4, 2), juce::Justification::centredLeft);

    // gain-reduction history (right)
    auto h = b.reduced (16.0f).withTrimmedLeft (sq.getWidth() + 18.0f);
    g.setColour (theme::grid);
    for (int db = 3; db <= 12; db += 3)
    {
        const float y = h.getY() + h.getHeight() * db / 15.0f;
        g.drawHorizontalLine ((int) y, h.getX(), h.getRight());
        g.drawText ("-" + juce::String (db), juce::Rectangle<float> (h.getRight() - 24, y - 7, 22, 14), juce::Justification::centredRight);
    }
    juce::Path lv, grp;
    for (int i = 0; i < History::kLen; ++i)
    {
        const float x = h.getX() + h.getWidth() * i / (History::kLen - 1);
        const float ly = h.getBottom() - (juce::jmax (-60.0f, level.at (i)) + 60.0f) / 60.0f * h.getHeight();
        const float gy = h.getY() + juce::jmin (15.0f, gr.at (i)) / 15.0f * h.getHeight();
        if (i == 0) { lv.startNewSubPath (x, h.getBottom()); lv.lineTo (x, ly); grp.startNewSubPath (x, gy); }
        else { lv.lineTo (x, ly); grp.lineTo (x, gy); }
    }
    lv.lineTo (h.getRight(), h.getBottom()); lv.closeSubPath();
    g.setColour (theme::ice.withAlpha (0.16f));
    g.fillPath (lv);
    g.setColour (theme::danger);
    g.strokePath (grp, juce::PathStrokeType (2.0f));
    g.setColour (theme::textOnDark.withAlpha (0.6f));
    g.drawText ("GAIN REDUCTION  " + juce::String (juce::jmax (0.0f, gr.at (History::kLen - 1)), 1) + " dB", h.withHeight (16), juce::Justification::centredLeft);
}

CompPage::CompPage (BankMasterProcessor& p)
    : display (p),
      style (p.getState(), bmp::id::compStyle, "Style"),
      autoGain (p.getState(), bmp::id::compAutoGain, "AUTO GAIN"), autoRel (p.getState(), bmp::id::compAutoRel, "AUTO RELEASE"),
      thr (p.getState(), bmp::id::compThr, "Threshold"), ratio (p.getState(), bmp::id::compRatio, "Ratio"),
      knee (p.getState(), bmp::id::compKnee, "Knee"), att (p.getState(), bmp::id::compAtt, "Attack"),
      rel (p.getState(), bmp::id::compRel, "Release"), range (p.getState(), bmp::id::compRange, "Range"),
      hold (p.getState(), bmp::id::compHold, "Hold"), la (p.getState(), bmp::id::compLa, "Lookahead"),
      makeup (p.getState(), bmp::id::compMakeup, "Makeup"), mix (p.getState(), bmp::id::compMix, "Dry/Wet"),
      scHp (p.getState(), bmp::id::compScHp, "SC HP"), scLp (p.getState(), bmp::id::compScLp, "SC LP"),
      link (p.getState(), bmp::id::compLink, "St. Link")
{
    for (juce::Component* c : { (juce::Component*) &display, (juce::Component*) &style, (juce::Component*) &autoGain, (juce::Component*) &autoRel,
                                (juce::Component*) &thr, (juce::Component*) &ratio, (juce::Component*) &knee, (juce::Component*) &att,
                                (juce::Component*) &rel, (juce::Component*) &range, (juce::Component*) &hold, (juce::Component*) &la,
                                (juce::Component*) &makeup, (juce::Component*) &mix, (juce::Component*) &scHp, (juce::Component*) &scLp,
                                (juce::Component*) &link })
        addAndMakeVisible (c);
}

void CompPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    r.removeFromTop (r.getHeight() * 45 / 100 + 10);
    theme::drawPanel (g, r.toFloat());
    sectionTitle (g, r.reduced (16, 0).removeFromTop (28), "MASTER COMPRESSOR");
}

void CompPage::resized()
{
    auto r = getLocalBounds();
    display.setBounds (r.removeFromTop (r.getHeight() * 45 / 100));
    r.removeFromTop (10);
    auto c = r.reduced (16, 8);
    c.removeFromTop (20);
    auto left = c.removeFromLeft (170);
    style.setBounds (left.removeFromTop (46));
    left.removeFromTop (10);
    autoGain.setBounds (left.removeFromTop (30)); left.removeFromTop (8);
    autoRel.setBounds (left.removeFromTop (30));
    c.removeFromLeft (14);
    auto row1 = c.removeFromTop (c.getHeight() / 2);
    layoutRow (row1, { &thr, &ratio, &knee, &att, &rel, &range }, 8);
    layoutRow (c, { &hold, &la, &makeup, &mix, &scHp, &scLp, &link }, 8);
}

// ============================================================================ LIMITER
void LimDisplay::tick()
{
    gr.push (proc.getChain().getLimGainReduction());
    out.push (juce::Decibels::gainToDecibels (juce::jmax (proc.getChain().getOutputMeter().getPeak (0), proc.getChain().getOutputMeter().getPeak (1)), -48.0f));
    repaint();
}

void LimDisplay::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    theme::drawDisplay (g, b);
    const auto a = b.reduced (14.0f).withTrimmedRight (30);
    const auto p = proc.getCurrentParams();
    auto toY = [&] (float db) { return a.getY() + (-db / 36.0f) * a.getHeight(); }; // 0 .. -36 dB
    g.setFont (theme::font (10.5f));
    for (int db = 0; db >= -36; db -= 6)
    {
        g.setColour (theme::grid);
        g.drawHorizontalLine ((int) toY ((float) db), a.getX(), a.getRight());
        g.setColour (theme::textOnDark.withAlpha (0.5f));
        g.drawText (juce::String (db), juce::Rectangle<float> (a.getRight() + 4, toY ((float) db) - 7, 28, 14), juce::Justification::centredLeft);
    }
    juce::Path o, gp;
    for (int i = 0; i < History::kLen; ++i)
    {
        const float x = a.getX() + a.getWidth() * i / (History::kLen - 1);
        const float oy = toY (juce::jmax (-36.0f, out.at (i)));
        const float gy = toY (-juce::jmin (36.0f, gr.at (i)));
        if (i == 0) { o.startNewSubPath (x, a.getBottom()); o.lineTo (x, oy); gp.startNewSubPath (x, gy); }
        else { o.lineTo (x, oy); gp.lineTo (x, gy); }
    }
    o.lineTo (a.getRight(), a.getBottom()); o.closeSubPath();
    g.setGradientFill (juce::ColourGradient (theme::ice.withAlpha (0.5f), 0, a.getY(), theme::iceDeep.withAlpha (0.08f), 0, a.getBottom(), false));
    g.fillPath (o);
    g.setColour (theme::danger);
    g.strokePath (gp, juce::PathStrokeType (2.0f));
    // ceiling line
    g.setColour (theme::warn);
    const float cy = toY (p.lim.ceilingDb);
    g.drawLine (a.getX(), cy, a.getRight(), cy, 1.2f);
    g.setColour (theme::textOnDark.withAlpha (0.7f));
    g.drawText ("OUTPUT LEVEL  /  GAIN REDUCTION " + juce::String (juce::jmax (0.0f, gr.at (History::kLen - 1)), 1) + " dB  /  CEILING " + juce::String (p.lim.ceilingDb, 2) + (p.lim.truePeak ? " dBTP" : " dBFS"),
                a.withHeight (16).translated (6, 2), juce::Justification::centredLeft);
}

LimPage::LimPage (BankMasterProcessor& p)
    : proc (p), display (p),
      style (p.getState(), bmp::id::limStyle, "Style"), os (p.getState(), bmp::id::limOs, "Oversampling"),
      dither (p.getState(), bmp::id::limDither, "Dither"),
      tp (p.getState(), bmp::id::limTp, "TRUE PEAK"),
      gain (p.getState(), bmp::id::limGain, "Gain"), ceil (p.getState(), bmp::id::limCeil, "Ceiling"),
      att (p.getState(), bmp::id::limAtt, "Attack"), rel (p.getState(), bmp::id::limRel, "Release"),
      la (p.getState(), bmp::id::limLa, "Lookahead"), linkT (p.getState(), bmp::id::limLinkT, "Link Trans."),
      linkR (p.getState(), bmp::id::limLinkR, "Link Release")
{
    for (juce::Component* c : { (juce::Component*) &display, (juce::Component*) &style, (juce::Component*) &os, (juce::Component*) &dither,
                                (juce::Component*) &tp, (juce::Component*) &gain, (juce::Component*) &ceil, (juce::Component*) &att,
                                (juce::Component*) &rel, (juce::Component*) &la, (juce::Component*) &linkT, (juce::Component*) &linkR })
        addAndMakeVisible (c);
}

void LimPage::tick()
{
    display.tick();
    auto& l = proc.getChain().getLoudness();
    lufsText[0] = fmtDb (l.getMomentary());
    lufsText[1] = fmtDb (l.getShortTerm());
    lufsText[2] = fmtDb (l.getIntegrated());
    tpText = fmtDb (juce::Decibels::gainToDecibels (proc.getChain().getOutputMeter().getTruePeakMax(), -100.0f), 2);
    const double fs = proc.getCurrentSampleRate();
    latencyText = juce::String (proc.getLatencySamples()) + " smp / " + juce::String (proc.getLatencySamples() * 1000.0 / fs, 2) + " ms  |  OS " + juce::String (proc.getChain().getLimiterOsFactor()) + "x";
    repaint (getLocalBounds().withTrimmedLeft (getWidth() * 62 / 100));
}

void LimPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (r.getHeight() * 52 / 100);
    r.removeFromTop (10);
    theme::drawPanel (g, r.toFloat());
    sectionTitle (g, r.reduced (16, 0).removeFromTop (28), "MASTER LIMITER");

    // loudness panel
    auto lp = top.removeFromRight (top.getWidth() * 38 / 100).withTrimmedLeft (10);
    theme::drawPanel (g, lp.toFloat());
    auto c = lp.reduced (16, 12);
    sectionTitle (g, c.removeFromTop (18), "LOUDNESS  (EBU R128 / BS.1770-4)");
    c.removeFromTop (6);
    const char* names[] = { "MOMENTARY", "SHORT-TERM", "INTEGRATED" };
    auto row = c.removeFromTop (78);
    const int w = row.getWidth() / 3;
    for (int i = 0; i < 3; ++i)
    {
        auto cell = row.removeFromLeft (w);
        g.setColour (theme::textDim);
        g.setFont (theme::font (11.0f, true));
        g.drawText (names[i], cell.removeFromTop (16), juce::Justification::centred);
        g.setColour (i == 2 ? theme::iceDeep : theme::text);
        g.setFont (theme::font (i == 2 ? 30.0f : 24.0f, true));
        g.drawText (lufsText[i], cell.removeFromTop (38), juce::Justification::centred);
        g.setColour (theme::textDim);
        g.setFont (theme::font (11.0f));
        g.drawText ("LUFS", cell, juce::Justification::centredTop);
    }
    c.removeFromTop (8);
    auto tpRow = c.removeFromTop (54);
    g.setColour (theme::textDim);
    g.setFont (theme::font (11.0f, true));
    g.drawText ("TRUE PEAK (MAX)", tpRow.removeFromTop (16), juce::Justification::centredLeft);
    const float tpv = tpText.getFloatValue();
    g.setColour (tpv > proc.getCurrentParams().lim.ceilingDb + 0.05f ? theme::danger : theme::text);
    g.setFont (theme::font (26.0f, true));
    g.drawText (tpText + " dBTP", tpRow, juce::Justification::centredLeft);
    g.setColour (theme::textDim);
    g.setFont (theme::font (11.0f));
    g.drawText ("LATENCY " + latencyText, c.removeFromTop (18), juce::Justification::centredLeft);
}

void LimPage::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (r.getHeight() * 52 / 100);
    display.setBounds (top.removeFromLeft (top.getWidth() * 62 / 100));
    r.removeFromTop (10);
    auto c = r.reduced (16, 8);
    c.removeFromTop (20);
    auto left = c.removeFromLeft (360);
    auto l1 = left.removeFromTop (46);
    style.setBounds (l1.removeFromLeft (170)); l1.removeFromLeft (10);
    os.setBounds (l1);
    left.removeFromTop (10);
    auto l2 = left.removeFromTop (46);
    dither.setBounds (l2.removeFromLeft (170)); l2.removeFromLeft (10);
    tp.setBounds (l2.withTrimmedTop (16));
    c.removeFromLeft (14);
    layoutRow (c, { &gain, &ceil, &att, &rel, &la, &linkT, &linkR }, 8);
}
