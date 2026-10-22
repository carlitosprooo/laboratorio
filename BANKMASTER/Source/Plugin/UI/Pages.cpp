// BANKMASTER — BY KALOX · V2 module pages
#include "Pages.h"

namespace bmui
{
namespace
{
    String hzShort (float f) { return f >= 1000.0f ? String (f / 1000.0f, f >= 10000.0f ? 1 : 2) + "kHz" : String (f, f < 100.0f ? 1 : 0) + "Hz"; }

    /** Real response of a prototype band of the given type (icon). */
    void bandTypeIcon (juce::Graphics& g, Rectangle<float> r, bm::BandType t, juce::Colour c)
    {
        bm::EqBandParams b;
        b.enabled = true; b.type = t; b.freq = 630.0f; b.q = 1.2f; b.slopeIdx = 3; b.gainDb = 9.0f;
        Path p;
        for (int i = 0; i <= 24; ++i)
        {
            const double f = 20.0 * std::pow (1000.0, i / 24.0);
            const float d = (float) juce::jlimit (-14.0, 12.0, bm::bandMagnitudeDb (b, f, 48000.0));
            const auto pt = juce::Point<float> (r.getX() + r.getWidth() * i / 24.0f, r.getCentreY() + r.getHeight() * 0.15f - d / 14.0f * r.getHeight() * 0.5f);
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    /** Lays knobs out in equal columns. */
    void layoutKnobs (Rectangle<int> r, std::initializer_list<juce::Component*> items, int cols = 0, int gap = 6)
    {
        const int n = cols > 0 ? cols : (int) items.size();
        const int w = (r.getWidth() - gap * (n - 1)) / n;
        for (auto* c : items) { c->setBounds (r.removeFromLeft (w)); r.removeFromLeft (gap); }
    }

    struct Titles
    {
        static void draw (juce::Graphics& g, Rectangle<int> r, const String& t, juce::Colour c) { sectionTitle (g, r.toFloat(), t, c); }
    };
}

void Page::paintDeck (juce::Graphics& g)
{
    panel (g, deckArea.toFloat(), P(), 12.0f, true);
}

void Page::deckDivider (juce::Graphics& g, int x)
{
    g.setColour (P().edge);
    g.fillRect ((float) x, (float) deckArea.getY() + 18.0f, 1.0f, (float) deckArea.getHeight() - 36.0f);
}

// ============================================================================ EQ
void BandChip::paintButton (juce::Graphics& g, bool over, bool)
{
    const auto& p = P();
    const auto params = proc.getCurrentParams();
    const auto& b = params.eq.bands[(size_t) band];
    const auto col = p.bands[band];
    auto r = getLocalBounds().toFloat();
    g.setColour (selected ? p.panelHi.brighter (0.06f) : over ? p.panelHi : p.well);
    g.fillRoundedRectangle (r, 7);
    g.setColour (selected ? col : over ? p.edgeHi : p.edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 7, selected ? 1.4f : 1.0f);
    auto top = r.reduced (7, 6).removeFromTop (16);
    g.setColour (b.enabled ? col : col.withAlpha (0.25f));
    g.fillEllipse (top.removeFromLeft (8).withSizeKeepingCentre (8, 8));
    g.setColour (selected ? p.text : p.textDim);
    g.setFont (font (11.0f, Fonts::Bold));
    g.drawText (String (band + 1), top.withTrimmedLeft (5), juce::Justification::centredLeft, false);
    bandTypeIcon (g, top.removeFromRight (22).reduced (0, 2), b.type, selected ? col : p.textMute);
    g.setColour (b.enabled ? (selected ? p.text : p.textDim) : p.textMute);
    g.setFont (font (11.0f, Fonts::Medium));
    g.drawText (b.enabled ? hzShort (b.freq) : String ("OFF"), r.reduced (7, 6).removeFromBottom (17), juce::Justification::centredLeft, false);
}

EqPage::EqPage (BankMasterProcessor& p) : Page (p), display (p)
{
    addAndMakeVisible (display);
    display.onBandSelected = [this] (int b) { selectBand (b); };
    for (auto* c : std::initializer_list<juce::Component*> { &analyzerCap, &rangeCap, &analyzer, &range, &bandOn, &typeCap, &slopeCap, &placeCap,
                                                             &type, &slope, &place, &freq, &gain, &q, &dynOn, &dynRange, &dynThr })
        addAndMakeVisible (c);
    for (int b = 0; b < bm::kNumEqBands; ++b)
    {
        chips[b] = std::make_unique<BandChip> (p, b);
        chips[b]->onClick = [this, b] { selectBand (b); };
        addAndMakeVisible (*chips[b]);
    }
    auto& ui = proc.getUiState();
    const int rangeIdx = juce::jlimit (0, 3, (int) ui.getProperty ("eqRange", 1));
    const int mode = juce::jlimit (0, 2, (int) ui.getProperty ("analyzer", 2));
    const float ranges[] = { 3.0f, 6.0f, 12.0f, 30.0f };
    range.setSelected (rangeIdx);
    display.setRangeDb (ranges[rangeIdx]);
    analyzer.setSelected (mode);
    display.setAnalyzerMode (mode);
    range.onChange = [this, ranges] (int i) { display.setRangeDb (ranges[i]); proc.getUiState().setProperty ("eqRange", i, nullptr); };
    analyzer.onChange = [this] (int i) { display.setAnalyzerMode (i); proc.getUiState().setProperty ("analyzer", i, nullptr); };
    bandOn.setTooltip ("Band on / off");
    dynOn.setTooltip ("Dynamic EQ for this band");
    selectBand (juce::jlimit (0, bm::kNumEqBands - 1, (int) ui.getProperty ("eqBand", 2)));
}

void EqPage::selectBand (int b)
{
    band = b;
    display.setSelectedBand (b);
    proc.getUiState().setProperty ("eqBand", b, nullptr);
    for (int i = 0; i < bm::kNumEqBands; ++i) { chips[i]->selected = i == b; chips[i]->repaint(); }
    const auto col = P().bands[b];
    bandOn.bind (prm (bmp::id::eq (b, "on")), um());
    bandOn.setOnColour (col);
    type.bind (prm (bmp::id::eq (b, "type")), um());
    slope.bind (prm (bmp::id::eq (b, "slope")), um());
    place.bind (prm (bmp::id::eq (b, "place")), um());
    place.setAccent (col);
    freq.bind (prm (bmp::id::eq (b, "freq")), um());
    gain.bind (prm (bmp::id::eq (b, "gain")), um());
    q.bind (prm (bmp::id::eq (b, "q")), um());
    dynOn.bind (prm (bmp::id::eq (b, "dyn")), um());
    dynOn.setOnColour (col);
    dynRange.bind (prm (bmp::id::eq (b, "range")), um());
    dynThr.bind (prm (bmp::id::eq (b, "thr")), um());
    for (auto* k : { &freq, &gain, &q, &dynRange, &dynThr }) k->setAccent (col);
    updateEnablement();
    repaint();
}

void EqPage::updateEnablement()
{
    const auto bd = proc.getCurrentParams().eq.bands[(size_t) band];   // copy: getCurrentParams() returns a temporary
    const bool gainType = bm::bandSupportsGain (bd.type);
    const bool slopeType = bd.type == bm::BandType::LowCut || bd.type == bm::BandType::HighCut
                        || bd.type == bm::BandType::LowShelf || bd.type == bm::BandType::HighShelf;
    gain.setEnabled (gainType);
    slope.setEnabled (slopeType);
    dynOn.setEnabled (bm::bandSupportsDynamics (bd.type));
    const bool dyn = bd.dynOn && bm::bandSupportsDynamics (bd.type);
    dynRange.setEnabled (dyn);
    dynThr.setEnabled (dyn);
}

void EqPage::tick()
{
    display.tick();
    updateEnablement();
    const float a = proc.getChain().getEq().getCurrentGainDb (band);
    const auto bd = proc.getCurrentParams().eq.bands[(size_t) band];   // copy: getCurrentParams() returns a temporary
    const float now = bd.dynOn && bm::bandSupportsDynamics (bd.type) ? a - bd.gainDb : 0.0f;
    if (std::abs (now - activity) > 0.005f) { activity = now; repaint (deckArea); }
    for (auto& c : chips) c->repaint();
}

void EqPage::paint (juce::Graphics& g)
{
    const auto& p = P();
    paintDeck (g);
    auto deck = deckArea.reduced (18, 14);
    const auto col = p.bands[band];
    const auto bd = proc.getCurrentParams().eq.bands[(size_t) band];   // copy: getCurrentParams() returns a temporary
    Titles::draw (g, deck.withWidth (200).withHeight (18), "BANDS", p.module[0]);
    deckDivider (g, deck.getX() + 380);
    deckDivider (g, deck.getRight() - 320);
    Titles::draw (g, Rectangle<int> (deck.getX() + 400 + 28, deck.getY(), 300, 18),
                  "BAND " + String (band + 1) + String (juce::CharPointer_UTF8 ("  \xc2\xb7  ")) + bmp::bandTypeNames()[(int) bd.type], col);
    Titles::draw (g, Rectangle<int> (deck.getRight() - 300, deck.getY(), 200, 18), "DYNAMIC EQ", col);
    // dynamic activity (live gain change of the band)
    auto act = Rectangle<float> ((float) deck.getRight() - 300, (float) deck.getBottom() - 22, 300, 22);
    caption (g, act.removeFromLeft (80), "ACTIVITY", p, juce::Justification::centredLeft);
    auto val = act.removeFromRight (64);
    g.setColour (p.text);
    g.setFont (font (type::value, Fonts::Medium));
    g.drawText (trimNegativeZero (String (activity, 2)) + " dB", val, juce::Justification::centredRight, false);
    auto bar = act.reduced (4, 0).withSizeKeepingCentre (act.getWidth() - 8, 5);
    g.setColour (p.well); g.fillRoundedRectangle (bar, 2.5f);
    const float rangeAbs = juce::jmax (0.1f, std::abs (bd.dynRangeDb));
    g.setColour (col);
    g.fillRoundedRectangle (bar.withWidth (juce::jmax (4.0f, bar.getWidth() * juce::jlimit (0.0f, 1.0f, std::abs (activity) / rangeAbs))), 2.5f);
}

void EqPage::resized()
{
    auto r = getLocalBounds();
    auto disp = r.removeFromTop (juce::roundToInt (r.getHeight() * 0.60f));
    display.setBounds (disp);
    r.removeFromTop (12);
    deckArea = r;

    // display toolbar (top right of the display)
    auto tb = disp.reduced (14, 10).removeFromTop (24);
    auto rg = tb.removeFromRight (176);
    rangeCap.setBounds (rg.removeFromLeft (52));
    rg.removeFromLeft (8);
    range.setBounds (rg);
    tb.removeFromRight (16);
    auto an = tb.removeFromRight (282);
    analyzerCap.setBounds (an.removeFromLeft (74));
    an.removeFromLeft (8);
    analyzer.setBounds (an);

    auto deck = r.reduced (18, 14);
    auto c1 = deck.removeFromLeft (360);
    c1.removeFromTop (26);
    {
        auto row = c1.removeFromTop (50);
        const int w = row.getWidth() / bm::kNumEqBands;
        for (auto& c : chips) c->setBounds (row.removeFromLeft (w).reduced (3, 0));
    }
    c1.removeFromTop (10);
    {
        auto row = c1.removeFromTop (44);
        auto left = row.removeFromLeft (row.getWidth() / 2 - 6);
        row.removeFromLeft (12);
        typeCap.setBounds (left.removeFromTop (16)); type.setBounds (left.removeFromTop (26));
        slopeCap.setBounds (row.removeFromTop (16)); slope.setBounds (row.removeFromTop (26));
    }
    c1.removeFromTop (10);
    {
        auto row = c1.removeFromTop (26);
        placeCap.setBounds (row.removeFromLeft (92));
        place.setBounds (row);
    }
    deck.removeFromLeft (40);
    auto c3 = deck.removeFromRight (300);
    deck.removeFromRight (40);
    {
        auto t = c3.removeFromTop (20);
        dynOn.setBounds (t.removeFromRight (72).withSizeKeepingCentre (72, 22));
        c3.removeFromTop (6);
        c3.removeFromBottom (28);
        layoutKnobs (c3, { &dynRange, &dynThr });
    }
    {
        auto t = deck.removeFromTop (20);
        bandOn.setBounds (t.removeFromLeft (22).withSizeKeepingCentre (22, 22));
        deck.removeFromTop (6);
        layoutKnobs (deck, { &freq, &gain, &q });
    }
}

juce::Array<juce::Component*> EqPage::getBoundControls()
{
    return { &bandOn, &type, &slope, &place, &freq, &gain, &q, &dynOn, &dynRange, &dynThr };
}

// ============================================================================ SATURATION
SatPage::SatPage (BankMasterProcessor& p) : Page (p), display (p)
{
    addAndMakeVisible (display);
    style.bind (prm (bmp::id::satStyle), um());
    drive.bind (prm (bmp::id::satDrive), um());
    mix.bind (prm (bmp::id::satMix), um());
    tone.bind (prm (bmp::id::satTone), um());
    dyn.bind (prm (bmp::id::satDyn), um());
    fb.bind (prm (bmp::id::satFb), um());
    fbFreq.bind (prm (bmp::id::satFbFreq), um());
    out.bind (prm (bmp::id::satOut), um());
    os.bind (prm (bmp::id::satOs), um());
    hq.bind (prm (bmp::id::satHq), um());
    const auto mc = P().module[1];
    for (auto* k : { &drive, &mix, &tone, &dyn, &fb, &fbFreq, &out }) k->setAccent (mc);
    style.setAccent (mc);
    os.setAccent (mc);
    hq.setOnColour (mc);
    hq.setTooltip ("High-quality (steeper) oversampling filters");
    for (auto* c : std::initializer_list<juce::Component*> { &style, &drive, &mix, &tone, &dyn, &fb, &fbFreq, &out, &osCap, &os, &hq })
        addAndMakeVisible (c);
}

void SatPage::paint (juce::Graphics& g)
{
    const auto& p = P();
    const auto mc = p.module[1];
    paintDeck (g);
    auto deck = deckArea.reduced (18, 14);
    Titles::draw (g, deck.withWidth (200).withHeight (18), "STYLE", mc);
    Titles::draw (g, Rectangle<int> (deck.getX() + 334, deck.getY(), 200, 18), "SATURATE", mc);
    Titles::draw (g, Rectangle<int> (deck.getX() + 598, deck.getY(), 200, 18), "SHAPE & BLEND", mc);
    Titles::draw (g, Rectangle<int> (deck.getRight() - 190, deck.getY(), 190, 18), "QUALITY", mc);
    deckDivider (g, deck.getX() + 320);
    deckDivider (g, deck.getX() + 584);
    deckDivider (g, deck.getRight() - 210);
    const auto sp = proc.getCurrentParams().sat;
    const int F = 1 << sp.osFactorLog2;
    g.setColour (p.textMute);
    g.setFont (font (type::small, Fonts::Medium, 0.04f));
    g.drawText (sp.osFactorLog2 == 0 ? String ("NO OVERSAMPLING") : String ("LINEAR PHASE  ") + String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "  " + String (F) + "x",
                Rectangle<int> (deck.getRight() - 190, deck.getY() + 128, 190, 16), juce::Justification::centredLeft, false);
}

void SatPage::resized()
{
    auto r = getLocalBounds();
    display.setBounds (r.removeFromTop (juce::roundToInt (r.getHeight() * 0.58f)));
    r.removeFromTop (12);
    deckArea = r;
    auto deck = r.reduced (18, 14);
    auto c1 = deck.removeFromLeft (300);
    c1.removeFromTop (28);
    style.setBounds (c1);
    deck.removeFromLeft (34);
    auto hero = deck.removeFromLeft (230);
    hero.removeFromTop (20);
    drive.setBounds (hero);
    deck.removeFromLeft (34);
    auto c4 = deck.removeFromRight (190);
    deck.removeFromRight (40);
    c4.removeFromTop (28);
    osCap.setBounds (c4.removeFromTop (16));
    os.setBounds (c4.removeFromTop (28));
    c4.removeFromTop (12);
    hq.setBounds (c4.removeFromTop (28).withWidth (150));
    deck.removeFromTop (22);
    auto rowA = deck.removeFromTop (deck.getHeight() / 2).withTrimmedBottom (4), rowB = deck.withTrimmedTop (4);
    layoutKnobs (rowA, { &mix, &tone, &dyn });
    layoutKnobs (rowB, { &fb, &fbFreq, &out });
}

juce::Array<juce::Component*> SatPage::getBoundControls() { return { &style, &drive, &mix, &tone, &dyn, &fb, &fbFreq, &out, &os, &hq }; }

// ============================================================================ COMPRESSOR
CompPage::CompPage (BankMasterProcessor& p) : Page (p), display (p)
{
    addAndMakeVisible (display);
    style.bind (prm (bmp::id::compStyle), um());
    thr.bind (prm (bmp::id::compThr), um());
    ratio.bind (prm (bmp::id::compRatio), um());
    att.bind (prm (bmp::id::compAtt), um());
    rel.bind (prm (bmp::id::compRel), um());
    knee.bind (prm (bmp::id::compKnee), um());
    range.bind (prm (bmp::id::compRange), um());
    hold.bind (prm (bmp::id::compHold), um());
    la.bind (prm (bmp::id::compLa), um());
    makeup.bind (prm (bmp::id::compMakeup), um());
    mix.bind (prm (bmp::id::compMix), um());
    autoGain.bind (prm (bmp::id::compAutoGain), um());
    autoRel.bind (prm (bmp::id::compAutoRel), um());
    scHp.bind (prm (bmp::id::compScHp), um());
    scLp.bind (prm (bmp::id::compScLp), um());
    link.bind (prm (bmp::id::compLink), um());
    const auto mc = P().module[2];
    for (auto* k : { &thr, &ratio, &att, &rel, &knee, &range, &hold, &la, &makeup, &mix, &scHp, &scLp, &link }) k->setAccent (mc);
    style.setAccent (mc);
    autoGain.setTooltip ("Automatic make-up gain (added to MAKEUP)");
    autoRel.setTooltip ("Program-dependent release");
    makeup.setTooltip ("Make-up gain (added to the automatic make-up when AUTO GAIN is on)");
    for (auto* c : std::initializer_list<juce::Component*> { &styleCap, &style, &thr, &ratio, &att, &rel, &knee, &range, &hold, &la, &makeup, &mix,
                                                             &autoGain, &autoRel, &scHp, &scLp, &link })
        addAndMakeVisible (c);
}

void CompPage::tick()
{
    display.tick();
}

void CompPage::paint (juce::Graphics& g)
{
    const auto mc = P().module[2];
    paintDeck (g);
    auto deck = deckArea.reduced (18, 14);
    const int primW = juce::roundToInt (deck.getWidth() * 0.47f);
    Titles::draw (g, deck.withWidth (200).withHeight (18), "DYNAMICS", mc);
    Titles::draw (g, Rectangle<int> (deck.getX() + primW + 36, deck.getY(), 200, 18), "SHAPE", mc);
    Titles::draw (g, Rectangle<int> (deck.getRight() - 196, deck.getY(), 196, 18), "AUTO", mc);
    Titles::draw (g, Rectangle<int> (deck.getRight() - 196, deck.getY() + 108, 196, 18), "SIDECHAIN", mc);
    deckDivider (g, deck.getX() + primW + 18);
    deckDivider (g, deck.getRight() - 214);
}

void CompPage::resized()
{
    auto r = getLocalBounds();
    auto disp = r.removeFromTop (juce::roundToInt (r.getHeight() * 0.58f));
    display.setBounds (disp);
    r.removeFromTop (12);
    deckArea = r;
    auto tb = disp.reduced (14, 10).removeFromTop (24);
    style.setBounds (tb.removeFromRight (240));
    styleCap.setBounds (tb.removeFromRight (70).withTrimmedRight (8));

    auto deck = r.reduced (18, 14);
    auto prim = deck.removeFromLeft (juce::roundToInt (deck.getWidth() * 0.47f));
    prim.removeFromTop (24);
    layoutKnobs (prim, { &thr, &ratio, &att, &rel });
    deck.removeFromLeft (36);
    auto side = deck.removeFromRight (196);
    deck.removeFromRight (36);
    side.removeFromTop (26);
    autoGain.setBounds (side.removeFromTop (28));
    side.removeFromTop (8);
    autoRel.setBounds (side.removeFromTop (28));
    side.removeFromTop (44);
    layoutKnobs (side, { &scHp, &scLp, &link }, 3, 2);
    deck.removeFromTop (22);
    auto rowA = deck.removeFromTop (deck.getHeight() / 2).withTrimmedBottom (4), rowB = deck.withTrimmedTop (4);
    layoutKnobs (rowA, { &knee, &range, &hold });
    layoutKnobs (rowB, { &la, &makeup, &mix });
}

juce::Array<juce::Component*> CompPage::getBoundControls()
{
    return { &style, &thr, &ratio, &att, &rel, &knee, &range, &hold, &la, &makeup, &mix, &autoGain, &autoRel, &scHp, &scLp, &link };
}

// ============================================================================ LIMITER
LimPage::LimPage (BankMasterProcessor& p) : Page (p), display (p)
{
    addAndMakeVisible (display);
    style.bind (prm (bmp::id::limStyle), um());
    gain.bind (prm (bmp::id::limGain), um());
    ceiling.bind (prm (bmp::id::limCeil), um());
    att.bind (prm (bmp::id::limAtt), um());
    rel.bind (prm (bmp::id::limRel), um());
    la.bind (prm (bmp::id::limLa), um());
    linkT.bind (prm (bmp::id::limLinkT), um());
    linkR.bind (prm (bmp::id::limLinkR), um());
    tp.bind (prm (bmp::id::limTp), um());
    os.bind (prm (bmp::id::limOs), um());
    dither.bind (prm (bmp::id::limDither), um());
    style.setAccent (P().module[3]);
    tp.setTooltip ("True-peak limiting (inter-sample peaks)");
    os.setTooltip ("Oversampling of the true-peak stage");
    for (auto* c : std::initializer_list<juce::Component*> { &styleCap, &style, &gain, &ceiling, &att, &rel, &la, &linkT, &linkR, &tp, &osCap, &os, &ditherCap, &dither })
        addAndMakeVisible (c);
}

void LimPage::paint (juce::Graphics& g)
{
    const auto mc = P().module[3];
    paintDeck (g);
    auto deck = deckArea.reduced (18, 14);
    Titles::draw (g, deck.withWidth (200).withHeight (18), "DRIVE", mc);
    Titles::draw (g, Rectangle<int> (deck.getX() + 262, deck.getY(), 150, 18), "CEILING", mc);
    Titles::draw (g, Rectangle<int> (deck.getX() + 444, deck.getY(), 200, 18), "ENVELOPE & LINK", mc);
    Titles::draw (g, Rectangle<int> (deck.getRight() - 210, deck.getY(), 210, 18), "TRUE PEAK & OUTPUT", mc);
    deckDivider (g, deck.getX() + 246);
    deckDivider (g, deck.getX() + 428);
    deckDivider (g, deck.getRight() - 228);
}

void LimPage::resized()
{
    auto r = getLocalBounds();
    auto disp = r.removeFromTop (juce::roundToInt (r.getHeight() * 0.58f));
    display.setBounds (disp);
    r.removeFromTop (12);
    deckArea = r;
    auto tb = disp.reduced (14, 10).removeFromTop (24);
    style.setBounds (tb.removeFromRight (380));
    styleCap.setBounds (tb.removeFromRight (70).withTrimmedRight (8));

    auto deck = r.reduced (18, 14);
    auto hero = deck.removeFromLeft (230);
    hero.removeFromTop (20);
    gain.setBounds (hero);
    deck.removeFromLeft (32);
    auto ceil = deck.removeFromLeft (150);
    ceil.removeFromTop (34);
    ceiling.setBounds (ceil);
    deck.removeFromLeft (32);
    auto side = deck.removeFromRight (210);
    deck.removeFromRight (36);
    side.removeFromTop (26);
    tp.setBounds (side.removeFromTop (28).withWidth (150));
    side.removeFromTop (10);
    osCap.setBounds (side.removeFromTop (16));
    os.setBounds (side.removeFromTop (28));
    side.removeFromTop (8);
    ditherCap.setBounds (side.removeFromTop (16));
    dither.setBounds (side.removeFromTop (26));
    deck.removeFromTop (22);
    auto rowA = deck.removeFromTop (deck.getHeight() / 2).withTrimmedBottom (4), rowB = deck.withTrimmedTop (4);
    layoutKnobs (rowA, { &att, &rel, &la });
    layoutKnobs (rowB, { &linkT, &linkR }, 3);
}

juce::Array<juce::Component*> LimPage::getBoundControls()
{
    return { &style, &gain, &ceiling, &att, &rel, &la, &linkT, &linkR, &tp, &os, &dither };
}

} // namespace bmui
