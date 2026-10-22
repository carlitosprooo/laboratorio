// BANKMASTER V2 — design preview renderer (mockups for approval).
// Not part of the plugin. Renders the proposed UI with the V2 design-system
// primitives (Design/V2/DesignSystem.h) and real data from the unchanged DSP.
//
//   bankmaster_design_preview <fontsDir> <outDir>

#include <juce_graphics/juce_graphics.h>
#include "../V2/DesignSystem.h"
#include "Sim.h"

using namespace bmv2;
using juce::String;
using juce::Justification;
using F = Fonts;
static juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

namespace
{
const preview::Results* S = nullptr;

// ------------------------------------------------------------------ utilities
float nrm (float v, float lo, float hi) { return juce::jlimit (0.0f, 1.0f, (v - lo) / (hi - lo)); }
float nrmSkew (float v, float lo, float hi, float centre)
{
    const float skew = std::log (0.5f) / std::log ((centre - lo) / (hi - lo));
    return std::pow (nrm (v, lo, hi), skew);
}
String hz (float f) { return f >= 1000.0f ? String (f / 1000.0f, f >= 10000.0f ? 1 : 2) + " kHz" : String (f, f < 100.0f ? 1 : 0) + " Hz"; }
String db (float v, int dec = 1, const char* unit = " dB")
{
    if (std::abs (v) < 0.5f * std::pow (10.0f, (float) -dec)) v = 0.0f; // never "-0.0"
    return (v > 0 ? "+" : "") + String (v, dec) + unit;
}
String ms (float v) { return v < 10.0f ? String (v, 2) + " ms" : v < 100.0f ? String (v, 1) + " ms" : String (v, 0) + " ms"; }

void text (Graphics& g, Rectangle<float> r, const String& t, float h, F::Weight w, Colour c,
           Justification j = Justification::centredLeft, float kern = 0.0f)
{
    g.setColour (c);
    g.setFont (font (h, w, kern));
    g.drawText (t, r, j, false);
}

void vline (Graphics& g, float x, float y0, float y1, Colour c) { g.setColour (c); g.fillRect (x, y0, 1.0f, y1 - y0); }
void hline (Graphics& g, float y, float x0, float x1, Colour c) { g.setColour (c); g.fillRect (x0, y, x1 - x0, 1.0f); }

void dashed (Graphics& g, juce::Line<float> l, Colour c, float w = 1.0f)
{
    const float dash[] = { 4.0f, 4.0f };
    g.setColour (c);
    g.drawDashedLine (l, dash, 2, w);
}

/** Section header inside a deck: small coloured marker + tracked caption. */
void sectionTitle (Graphics& g, Rectangle<float> r, const String& t, const Palette& p, Colour mark)
{
    g.setColour (mark);
    g.fillRoundedRectangle (Rectangle<float> (3, 11).withCentre ({ r.getX() + 2, r.getCentreY() }), 1.5f);
    caption (g, r.withTrimmedLeft (10), t, p, Justification::centredLeft, p.textDim);
}

Rectangle<float> squareIn (Rectangle<float> r, float s) { return Rectangle<float> (s, s).withCentre (r.getCentre()); }

// -------------------------------------------------------------- chrome pieces
void background (Graphics& g, Rectangle<float> r, const Palette& p)
{
    g.setGradientFill (juce::ColourGradient (p.bg0, r.getCentreX(), r.getY(), p.bg1, r.getCentreX(), r.getBottom(), false));
    g.fillRect (r);
    // very subtle vignette / noise-free sheen at the top
    g.setGradientFill (juce::ColourGradient (p.accent.withAlpha (0.035f), r.getCentreX(), r.getY() - 200,
                                             p.accent.withAlpha (0.0f), r.getCentreX(), r.getY() + 260, true));
    g.fillRect (r);
}

void undoRedo (Graphics& g, Rectangle<float> r, const Palette& p, bool canUndo, bool canRedo)
{
    g.setColour (p.well);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (p.edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    auto left = r.removeFromLeft (r.getWidth() * 0.5f);
    vline (g, left.getRight(), r.getY() + 8, r.getBottom() - 8, p.edgeHi);
    icon (g, left.removeFromLeft (30).reduced (7.5f, 9.0f).translated (5, 0), Icon::Undo, canUndo ? p.text : p.textMute, 1.7f);
    text (g, left.withTrimmedLeft (6), "UNDO", 11.0f, F::SemiBold, canUndo ? p.text : p.textMute, Justification::centredLeft, 0.1f);
    auto right = r;
    icon (g, right.removeFromRight (30).reduced (7.5f, 9.0f).translated (-5, 0), Icon::Redo, canRedo ? p.text : p.textMute, 1.7f);
    text (g, right.withTrimmedRight (6), "REDO", 11.0f, F::SemiBold, canRedo ? p.text : p.textMute, Justification::centredRight, 0.1f);
}

struct HeaderState
{
    bool modified = false, canUndo = true, canRedo = false;
    int ab = 0;
    bool oneOn = false;
    mutable Rectangle<float> revertBounds;   // filled by header(): anchor for the confirmation popover
};

void header (Graphics& g, Rectangle<float> r, const Palette& p, const HeaderState& st)
{
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.fillRect (r);
    hline (g, r.getBottom() - 1, r.getX(), r.getRight(), p.edge);

    auto row = r.reduced (16, 11);
    logoMark (g, row.removeFromLeft (row.getHeight()), p, true);
    row.removeFromLeft (10);
    wordmark (g, row.removeFromLeft (200).withTrimmedTop (4), p, 19.0f, true);

    // right cluster
    iconButton (g, row.removeFromRight (36).withSizeKeepingCentre (36, 36), Icon::Menu, p);
    row.removeFromRight (14);
    {
        auto one = row.removeFromRight (148).withSizeKeepingCentre (148, 42);
        g.setColour (p.well); g.fillRoundedRectangle (one, 9.0f);
        g.setColour (st.oneOn ? p.accent.withAlpha (0.6f) : p.edge); g.drawRoundedRectangle (one.reduced (0.5f), 9.0f, 1.0f);
        auto pw = one.removeFromLeft (30);
        icon (g, pw.reduced (8.5f, 12.5f).translated (3, 0), Icon::Power, st.oneOn ? p.accent : p.textMute, 1.6f);
        KnobSpec k; k.size = KnobSize::Mini; k.value = 0.5f; k.enabled = st.oneOn;
        knobFace (g, one.removeFromRight (40).reduced (4), k, p);
        text (g, one.removeFromTop (22).translated (0, 3), "ONE MASTER", 10.0f, F::SemiBold, st.oneOn ? p.text : p.textDim, Justification::centredLeft, 0.12f);
        text (g, one.translated (0, -3), "100 %", 11.0f, F::Medium, st.oneOn ? p.accent : p.textMute);
    }
    row.removeFromRight (12);
    undoRedo (g, row.removeFromRight (164).withSizeKeepingCentre (164, 34), p, st.canUndo, st.canRedo);
    row.removeFromRight (12);
    {
        auto abr = row.removeFromRight (118).withSizeKeepingCentre (118, 34);
        iconButton (g, abr.removeFromRight (34), Icon::Copy, p);
        abr.removeFromRight (6);
        segmented (g, abr, { "A", "B" }, st.ab, p, 12.0f);
    }

    // preset browser (centre)
    auto pr = row.withSizeKeepingCentre (juce::jmin (470.0f, row.getWidth() - 20), 40);
    auto icons = pr.removeFromRight (78);
    iconButton (g, icons.removeFromLeft (36).withSizeKeepingCentre (36, 36), Icon::Save, p);
    icons.removeFromLeft (6);
    st.revertBounds = icons.removeFromLeft (36).withSizeKeepingCentre (36, 36);
    iconButton (g, st.revertBounds, Icon::Revert, p, st.modified);
    pr.removeFromRight (8);
    g.setColour (p.well); g.fillRoundedRectangle (pr, 9.0f);
    g.setColour (p.edge); g.drawRoundedRectangle (pr.reduced (0.5f), 9.0f, 1.0f);
    auto prev = pr.removeFromLeft (34), next = pr.removeFromRight (34);
    icon (g, prev.reduced (11, 13), Icon::Prev, p.textDim, 1.8f);
    icon (g, next.reduced (11, 13), Icon::Next, p.textDim, 1.8f);
    vline (g, prev.getRight(), pr.getY() + 8, pr.getBottom() - 8, p.edge);
    vline (g, next.getX(), pr.getY() + 8, pr.getBottom() - 8, p.edge);
    auto nm = pr.reduced (12, 3);
    text (g, nm.removeFromTop (15).translated (0, 2), st.modified ? U("FACTORY  \xc2\xb7  MODIFIED") : "FACTORY", 9.0f, F::SemiBold,
          st.modified ? p.warn : p.textMute, Justification::centred, 0.16f);
    text (g, nm, U("KALOX MASTER \xe2\x80\x94 ORIGINAL"), 13.5f, F::SemiBold, p.text, Justification::centred, 0.02f);
}

/** Signal-flow navigation: INPUT -> EQ -> SAT -> COMP -> LIM -> OUTPUT. */
void navStrip (Graphics& g, Rectangle<float> r, const Palette& p, int active)
{
    auto row = r.reduced (16, 9);
    const float io = 118, arrow = 20;
    const float modW = (row.getWidth() - 2 * io - 5 * arrow) / 4.0f;
    // flow line behind the nodes
    hline (g, row.getCentreY(), row.getX() + 20, row.getRight() - 20, p.edgeHi);

    auto arrowAt = [&] (Rectangle<float> a)
    {
        Path ch;
        const float x = a.getCentreX(), y = a.getCentreY();
        ch.startNewSubPath (x - 2.5f, y - 4.0f); ch.lineTo (x + 2.5f, y); ch.lineTo (x - 2.5f, y + 4.0f);
        g.setColour (p.textMute);
        g.strokePath (ch, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    auto ioNode = [&] (Rectangle<float> n, const String& t, const String& v, bool learn)
    {
        g.setColour (p.panel); g.fillRoundedRectangle (n, 8.0f);
        g.setColour (p.edge); g.drawRoundedRectangle (n.reduced (0.5f), 8.0f, 1.0f);
        auto in = n.reduced (12, 6);
        text (g, in.removeFromTop (in.getHeight() * 0.5f), t, 9.5f, F::SemiBold, p.textDim, Justification::bottomLeft, 0.16f);
        text (g, in, v, 12.0f, F::Medium, p.text, Justification::topLeft);
        if (learn)
        {
            auto l = n.reduced (8).removeFromRight (40).withSizeKeepingCentre (40, 18);
            g.setColour (p.accentSoft); g.fillRoundedRectangle (l, 9.0f);
            text (g, l, "LRN", 9.0f, F::Bold, p.accent, Justification::centred, 0.1f);
        }
    };

    ioNode (row.removeFromLeft (io), "INPUT", db (S->inputGainDb), true);
    arrowAt (row.removeFromLeft (arrow));
    const char* names[4] = { "EQ", "SATURATION", "COMPRESSOR", "LIMITER" };
    for (int m = 0; m < 4; ++m)
    {
        auto n = row.removeFromLeft (modW);
        const bool act = m == active;
        const auto mc = p.module[m];
        if (act)
        {
            Path sh; sh.addRoundedRectangle (n, 8.0f);
            juce::DropShadow (mc.withAlpha (0.28f), 16, {}).drawForPath (g, sh);
        }
        g.setColour (act ? p.panelHi : p.panel); g.fillRoundedRectangle (n, 8.0f);
        g.setColour (act ? mc.withAlpha (0.85f) : p.edge); g.drawRoundedRectangle (n.reduced (0.5f), 8.0f, act ? 1.3f : 1.0f);
        auto in = n.reduced (10, 6);
        // power LED
        auto pw = in.removeFromLeft (26);
        auto led = squareIn (pw, 22);
        g.setColour (p.well); g.fillEllipse (led);
        g.setColour (mc.withAlpha (0.55f)); g.drawEllipse (led.reduced (0.5f), 1.0f);
        icon (g, led.reduced (5.5f), Icon::Power, mc, 1.5f);
        in.removeFromLeft (8);
        // mini visual on the right
        auto viz = in.removeFromRight (juce::jmin (64.0f, in.getWidth() * 0.32f)).reduced (0, 4);
        g.setColour (p.well); g.fillRoundedRectangle (viz, 4.0f);
        auto vz = viz.reduced (5, 4);
        if (m == 0)
        {
            Path c;
            for (int i = 0; i <= 40; ++i)
            {
                const double f = 20.0 * std::pow (1000.0, i / 40.0);
                double dB = 0;
                for (int b = 0; b < bm::kNumEqBands; ++b)
                    if (S->params.eq.bands[(size_t) b].enabled) dB += bm::bandMagnitudeDb (S->params.eq.bands[(size_t) b], f, S->fs);
                const float y = vz.getCentreY() - (float) juce::jlimit (-6.0, 6.0, dB) / 6.0f * vz.getHeight() * 0.5f;
                if (i == 0) c.startNewSubPath (vz.getX() + vz.getWidth() * i / 40.0f, y);
                else c.lineTo (vz.getX() + vz.getWidth() * i / 40.0f, y);
            }
            g.setColour (mc); g.strokePath (c, juce::PathStrokeType (1.3f));
        }
        else if (m == 1)
        {
            Path c;
            for (int i = 0; i <= 30; ++i)
            {
                const float u = -1.0f + 2.0f * i / 30.0f;
                const float y = bm::Shaper::curve (bm::SatStyle::CleanTube, u * 2.2f) / 2.2f;
                const auto pt = juce::Point<float> (vz.getX() + vz.getWidth() * i / 30.0f, vz.getCentreY() - y * vz.getHeight() * 0.5f);
                if (i == 0) c.startNewSubPath (pt); else c.lineTo (pt);
            }
            g.setColour (mc); g.strokePath (c, juce::PathStrokeType (1.3f));
        }
        else
        {
            const float gr = m == 2 ? S->compGrNow : S->limGrNow;
            auto bar = vz.withSizeKeepingCentre (vz.getWidth(), 6.0f);
            g.setColour (p.panelHi); g.fillRoundedRectangle (bar, 3.0f);
            g.setColour (mc); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.04f, 1.0f, gr / 12.0f)), 3.0f);
        }
        in.removeFromRight (6);
        auto t1 = in.removeFromTop (in.getHeight() * 0.52f);
        text (g, t1, String::formatted ("0%d", m + 1), 9.0f, F::SemiBold, act ? mc : p.textMute, Justification::bottomLeft, 0.1f);
        text (g, t1.withTrimmedLeft (18), names[m], 11.5f, F::Bold, act ? p.text : p.textDim, Justification::bottomLeft, 0.1f);
        String sub;
        if (m == 0) sub = U("5 BANDS \xc2\xb7 2 DYN");
        if (m == 1) sub = U("CLEAN TUBE \xc2\xb7 20 %");
        if (m == 2) sub = "GR " + String (-S->compGrNow, 1) + " dB";
        if (m == 3) sub = "GR " + String (-S->limGrNow, 1) + " dB";
        text (g, in, sub, 10.0f, F::Medium, act ? p.textDim : p.textMute, Justification::topLeft, 0.04f);
        arrowAt (row.removeFromLeft (arrow));
    }
    ioNode (row.removeFromLeft (io), "OUTPUT", db (0.0f), false);
}

/** Compact global bar: INPUT | LUFS | TRUE PEAK | GR | CORRELATION | OUTPUT | LEARN | BYPASS. */
void globalBar (Graphics& g, Rectangle<float> r, const Palette& p)
{
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRect (r);
    hline (g, r.getY(), r.getX(), r.getRight(), p.edge);
    auto row = r.reduced (16, 10);

    auto divider = [&] { row.removeFromLeft (11); vline (g, row.getX(), row.getY() + 4, row.getBottom() - 4, p.edge); row.removeFromLeft (11); };
    auto lrMeter = [&] (Rectangle<float> a, const String& t, float l, float rr, const String& v)
    {
        caption (g, a.removeFromTop (14), t, p, Justification::centredLeft);
        auto vv = a.removeFromRight (56);
        text (g, vv, v, 12.0f, F::Medium, p.text, Justification::centredRight);
        a.removeFromRight (8);
        auto m1 = a.withHeight (5).withY (a.getCentreY() - 6), m2 = m1.translated (0, 8);
        meterBar (g, m1, l, p, false, -48.0f, l + 0.5f);
        meterBar (g, m2, rr, p, false, -48.0f, rr + 0.5f);
    };

    lrMeter (row.removeFromLeft (156), "INPUT", S->inPkL, S->inPkR, db (std::max (S->inPkL, S->inPkR)));
    divider();
    {
        auto a = row.removeFromLeft (172);
        caption (g, a.removeFromTop (14), "LOUDNESS  LUFS", p, Justification::centredLeft);
        const float cw = a.getWidth() / 3.0f;
        const char* labels[3] = { "M", "S", "I" };
        const float vals[3] = { S->lufsM, S->lufsS, S->lufsI };
        for (int i = 0; i < 3; ++i)
        {
            auto c = a.removeFromLeft (cw);
            text (g, c.removeFromLeft (12), labels[i], 10.0f, F::SemiBold, p.textMute, Justification::centredLeft);
            text (g, c, String (vals[i], 1), i == 2 ? 16.0f : 13.0f, i == 2 ? F::SemiBold : F::Medium, i == 2 ? p.accent : p.text, Justification::centredLeft);
        }
    }
    divider();
    {
        auto a = row.removeFromLeft (96);
        caption (g, a.removeFromTop (14), "TRUE PEAK", p, Justification::centredLeft);
        text (g, a, String (S->truePeakDb, 2) + " dBTP", 13.0f, F::Medium, S->truePeakDb > -0.1f + 0.05f ? p.danger : p.text);
    }
    divider();
    {
        auto a = row.removeFromLeft (132);
        caption (g, a.removeFromTop (14), "GAIN REDUCTION", p, Justification::centredLeft);
        auto l1 = a.removeFromTop (a.getHeight() * 0.5f), l2 = a;
        auto grRow = [&] (Rectangle<float> rr, const String& t, float v, Colour c)
        {
            text (g, rr.removeFromLeft (34), t, 9.5f, F::SemiBold, p.textMute, Justification::centredLeft, 0.1f);
            text (g, rr.removeFromRight (32), String (-v, 1), 11.0f, F::Medium, p.text, Justification::centredRight);
            auto b = rr.reduced (4, 0).withSizeKeepingCentre (rr.getWidth() - 8, 4);
            g.setColour (p.well); g.fillRoundedRectangle (b, 2);
            g.setColour (c); g.fillRoundedRectangle (b.withWidth (b.getWidth() * juce::jlimit (0.03f, 1.0f, v / 12.0f)), 2);
        };
        grRow (l1, "COMP", S->compGrNow, p.module[2]);
        grRow (l2, "LIM", S->limGrNow, p.module[3]);
    }
    divider();
    {
        auto a = row.removeFromLeft (112);
        caption (g, a.removeFromTop (14), "CORRELATION", p, Justification::centredLeft);
        auto v = a.removeFromRight (34);
        text (g, v, String (S->corr, 2), 12.0f, F::Medium, p.text, Justification::centredRight);
        auto b = a.withSizeKeepingCentre (a.getWidth() - 6, 6);
        g.setColour (p.well); g.fillRoundedRectangle (b, 3);
        vline (g, b.getCentreX(), b.getY() - 2, b.getBottom() + 2, p.edgeHi);
        const float x = b.getX() + b.getWidth() * (S->corr + 1.0f) * 0.5f;
        g.setColour (S->corr < 0 ? p.danger : p.accent);
        g.fillRoundedRectangle (Rectangle<float> (b.getCentreX(), b.getY(), x - b.getCentreX(), b.getHeight()), 3);
        g.fillEllipse (Rectangle<float> (8, 8).withCentre ({ x, b.getCentreY() }));
    }
    divider();
    lrMeter (row.removeFromLeft (156), "OUTPUT", S->outPkL, S->outPkR, db (std::max (S->outPkL, S->outPkR)));

    // right: LEARN + BYPASS
    auto rr = row.withTrimmedLeft (12);
    auto byp = rr.removeFromRight (100).withSizeKeepingCentre (100, 34);
    pillToggle (g, byp, "BYPASS", false, p);
    rr.removeFromRight (8);
    auto learn = rr.removeFromRight (124).withSizeKeepingCentre (124, 34);
    g.setColour (p.well); g.fillRoundedRectangle (learn, 17);
    g.setColour (p.edge); g.drawRoundedRectangle (learn.reduced (0.5f), 17, 1);
    auto li = learn.reduced (11, 0);
    icon (g, li.removeFromLeft (16).withSizeKeepingCentre (16, 16), Icon::Learn, p.accent, 1.5f);
    li.removeFromLeft (8);
    text (g, li.removeFromTop (learn.getHeight() * 0.52f), "LEARN", 10.0f, F::SemiBold, p.text, Justification::bottomLeft, 0.1f);
    text (g, li, U("\xe2\x88\x92") + "20.0 LUFS", 9.0f, F::Medium, p.textDim, Justification::topLeft, 0.04f);
}

// ------------------------------------------------------------------ the deck
struct Deck
{
    Graphics& g; const Palette& p; Rectangle<float> area;
    void panelBg() { panel (g, area, p, 12.0f, true); }
    void divider (float x) { vline (g, x, area.getY() + 18, area.getBottom() - 18, p.edge); }
};

void knobRow (Graphics& g, Rectangle<float> r, const Palette& p, std::initializer_list<KnobSpec> ks, int cols = 0)
{
    const float w = r.getWidth() / (float) (cols > 0 ? cols : (int) ks.size());
    for (auto& k : ks) knob (g, r.removeFromLeft (w).reduced (6, 0), k, p);
}

KnobSpec K (const String& label, const String& v, float val, KnobSize sz = KnobSize::Primary, bool bip = false, bool en = true)
{
    KnobSpec k; k.label = label; k.valueText = v; k.value = val; k.size = sz; k.bipolar = bip; k.enabled = en; return k;
}

// ===================================================================== EQ
float freqToX (Rectangle<float> r, double f) { return r.getX() + r.getWidth() * (float) (std::log (f / 20.0) / std::log (1000.0)); }
double xToFreq (Rectangle<float> r, float x) { return 20.0 * std::pow (1000.0, (x - r.getX()) / r.getWidth()); }

void bandTypeIcon (Graphics& g, Rectangle<float> r, bm::BandType t, Colour c)
{
    bm::EqBandParams b; b.enabled = true; b.type = t; b.freq = 630.0f; b.q = 1.2f; b.slopeIdx = 3;
    b.gainDb = t == bm::BandType::LowShelf ? 9.0f : t == bm::BandType::HighShelf ? 9.0f : 9.0f;
    Path c1;
    for (int i = 0; i <= 24; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, i / 24.0);
        const float dB = (float) juce::jlimit (-14.0, 12.0, bm::bandMagnitudeDb (b, f, 48000.0));
        const auto pt = juce::Point<float> (r.getX() + r.getWidth() * i / 24.0f, r.getCentreY() + r.getHeight() * 0.15f - dB / 14.0f * r.getHeight() * 0.5f);
        if (i == 0) c1.startNewSubPath (pt); else c1.lineTo (pt);
    }
    g.setColour (c);
    g.strokePath (c1, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void eqScreen (Graphics& g, Rectangle<float> r, const Palette& p)
{
    const int sel = 2; // dynamic band 861 Hz selected
    const auto& bands = S->params.eq.bands;
    const double rangeDb = 6.0;
    auto disp = r.removeFromTop (r.getHeight() * 0.60f);
    r.removeFromTop (12);
    display (g, disp, p, 12.0f);

    auto plot = disp.reduced (14, 12).withTrimmedBottom (18).withTrimmedRight (26).withTrimmedTop (30);
    // toolbar
    {
        auto tb = disp.reduced (14, 10).removeFromTop (24);
        sectionTitle (g, tb.removeFromLeft (150), "MASTER EQ", p, p.module[0]);
        auto rng = tb.removeFromRight (176);
        caption (g, rng.removeFromLeft (52), "RANGE", p, Justification::centredRight);
        rng.removeFromLeft (8);
        segmented (g, rng, { "3", "6", "12", "30" }, 1, p, 10.5f);
        tb.removeFromRight (16);
        auto an = tb.removeFromRight (270);
        caption (g, an.removeFromLeft (70), "ANALYZER", p, Justification::centredRight);
        an.removeFromLeft (8);
        segmented (g, an, { "PRE", "POST", "PRE + POST" }, 2, p, 10.0f);
        tb.removeFromRight (16);
        // legend
        auto lg = tb.removeFromRight (180);
        g.setColour (p.specPre.brighter (0.4f)); g.fillRoundedRectangle (lg.removeFromLeft (10).withSizeKeepingCentre (10, 4), 2);
        text (g, lg.removeFromLeft (44).withTrimmedLeft (6), "PRE", 9.5f, F::SemiBold, p.textMute, Justification::centredLeft, 0.12f);
        g.setColour (p.specPost); g.fillRoundedRectangle (lg.removeFromLeft (10).withSizeKeepingCentre (10, 4), 2);
        text (g, lg.withTrimmedLeft (6), "POST", 9.5f, F::SemiBold, p.textMute, Justification::centredLeft, 0.12f);
    }
    auto yOfDb = [&] (double dB) { return plot.getCentreY() - (float) (dB / rangeDb) * plot.getHeight() * 0.5f; };

    // grid
    const double fl[] = { 30, 40, 50, 60, 70, 80, 90, 200, 300, 400, 500, 600, 700, 800, 900, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000 };
    for (double f : fl) vline (g, freqToX (plot, f), plot.getY(), plot.getBottom(), p.grid.withMultipliedAlpha (0.6f));
    const double fm[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    const char* fmL[] = { "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };
    for (int i = 0; i < 10; ++i)
    {
        const float x = freqToX (plot, fm[i]);
        if (fm[i] == 100 || fm[i] == 1000 || fm[i] == 10000) vline (g, x, plot.getY(), plot.getBottom(), p.gridStrong);
        text (g, Rectangle<float> (x - 20, plot.getBottom() + 4, 40, 14), fmL[i], 9.5f, F::Medium, p.textMute, Justification::centred);
    }
    for (int d = -6; d <= 6; d += 2)
    {
        const float y = yOfDb (d);
        hline (g, y, plot.getX(), plot.getRight(), d == 0 ? p.gridStrong : p.grid);
        text (g, Rectangle<float> (plot.getRight() + 4, y - 7, 24, 14), d > 0 ? "+" + String (d) : String (d), 9.5f, F::Medium, p.textMute, Justification::centredLeft);
    }

    // spectra (display tilt 4.5 dB/oct around 1 kHz, 1/6-octave smoothing)
    auto specPath = [&] (const std::vector<double>& sp, bool closed)
    {
        Path path;
        const int N = S->fftN;
        const float top = plot.getY() + plot.getHeight() * 0.30f, bottom = plot.getBottom();
        const double lo = -96.0, hi = -30.0;
        bool started = false;
        for (float x = plot.getX(); x <= plot.getRight(); x += 2.0f)
        {
            const double f = xToFreq (plot, x);
            // 1/3-octave Gaussian smoothing over fractional bins (no staircase at low frequencies)
            const double sigma = std::log (2.0) / 6.0;
            const double kc = f * N / S->fs;
            const int k0 = juce::jlimit (1, N / 2, (int) std::floor (kc * std::exp (-3 * sigma)) - 1);
            const int k1 = juce::jlimit (k0, N / 2, (int) std::ceil (kc * std::exp (3 * sigma)) + 1);
            double acc = 0, wsum = 0;
            for (int k = k0; k <= k1; ++k)
            {
                const double d = std::log ((double) k / kc) / sigma;
                const double w = std::exp (-0.5 * d * d);
                acc += w * std::pow (10.0, sp[(size_t) k] / 10.0); wsum += w;
            }
            double v = 10.0 * std::log10 (std::max (1e-30, acc / std::max (1e-12, wsum))) + 4.5 * std::log2 (f / 1000.0);
            const float y = juce::jlimit (top, bottom, bottom - (float) ((v - lo) / (hi - lo)) * (bottom - top));
            if (! started) { path.startNewSubPath (x, closed ? bottom : y); if (closed) path.lineTo (x, y); started = true; }
            else path.lineTo (x, y);
        }
        if (closed) { path.lineTo (plot.getRight(), bottom); path.closeSubPath(); }
        return path;
    };
    g.setColour (p.specPre.withAlpha (0.65f));
    g.fillPath (specPath (S->specPre, true));
    {
        auto post = specPath (S->specPost, true);
        g.setGradientFill (juce::ColourGradient (p.specPost.withAlpha (0.16f), 0, plot.getY() + plot.getHeight() * 0.3f, p.specPost.withAlpha (0.01f), 0, plot.getBottom(), false));
        g.fillPath (post);
        g.setColour (p.specPost.withAlpha (0.75f));
        g.strokePath (specPath (S->specPost, false), juce::PathStrokeType (1.1f));
        g.setColour (p.textDim.withAlpha (0.45f));
        g.strokePath (specPath (S->specPre, false), juce::PathStrokeType (1.0f));
    }

    // per-band fills, dynamic range, total curve
    auto curveOf = [&] (auto&& dbAt)
    {
        Path c;
        for (float x = plot.getX(); x <= plot.getRight() + 0.5f; x += 1.5f)
        {
            const float y = juce::jlimit (plot.getY() - 4, plot.getBottom() + 4, yOfDb (dbAt (xToFreq (plot, x))));
            if (x == plot.getX()) c.startNewSubPath (x, y); else c.lineTo (x, y);
        }
        return c;
    };
    for (int b = 0; b < bm::kNumEqBands; ++b)
    {
        const auto& bp = bands[(size_t) b];
        if (! bp.enabled) continue;
        const float gNow = bp.dynOn ? S->eqDynGain[b] : bp.gainDb;
        auto c = curveOf ([&] (double f) { return bm::bandMagnitudeDb (bp, f, S->fs, bp.dynOn ? gNow : 1e9f); });
        Path fill (c);
        fill.lineTo (plot.getRight(), yOfDb (0)); fill.lineTo (plot.getX(), yOfDb (0)); fill.closeSubPath();
        g.setColour (p.bands[b].withAlpha (b == sel ? 0.22f : 0.10f));
        g.fillPath (fill);
        if (bp.dynOn)
        {
            // dynamic range: static curve to (static + range)
            auto lim = curveOf ([&] (double f) { return bm::bandMagnitudeDb (bp, f, S->fs, bp.gainDb + bp.dynRangeDb); });
            g.setColour (p.bands[b].withAlpha (b == sel ? 0.9f : 0.5f));
            const float dash[] = { 3.0f, 3.0f };
            Path dashedP;
            juce::PathStrokeType (1.0f).createDashedStroke (dashedP, lim, dash, 2);
            g.fillPath (dashedP);
        }
    }
    auto total = curveOf ([&] (double f)
    {
        double s = 0;
        for (int b = 0; b < bm::kNumEqBands; ++b)
        {
            const auto& bp = bands[(size_t) b];
            if (bp.enabled) s += bm::bandMagnitudeDb (bp, f, S->fs, bp.dynOn ? S->eqDynGain[b] : 1e9f);
        }
        return s;
    });
    glowStroke (g, total, p.curve, 2.2f, 10.0f);

    // nodes
    for (int b = 0; b < bm::kNumEqBands; ++b)
    {
        const auto& bp = bands[(size_t) b];
        const float x = freqToX (plot, bp.freq);
        const float y = yOfDb (bm::bandSupportsGain (bp.type) ? bp.gainDb : 0.0);
        const auto c = juce::Point<float> (x, y);
        if (b == sel)
        {
            g.setColour (p.bands[b].withAlpha (0.16f)); g.fillEllipse (Rectangle<float> (40, 40).withCentre (c));
            g.setColour (p.bands[b].withAlpha (0.55f)); g.drawEllipse (Rectangle<float> (30, 30).withCentre (c), 1.2f);
            // dyn range handle
            const float yr = yOfDb (bp.gainDb + bp.dynRangeDb);
            g.setColour (p.bands[b]);
            g.drawLine (x, y + 15, x, yr + 10, 1.0f);
            g.fillRoundedRectangle (Rectangle<float> (14, 4).withCentre ({ x, yr + 12 }), 2);
        }
        juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 6, { 0, 1 }).drawForRectangle (g, Rectangle<float> (18, 18).withCentre (c).toNearestInt());
        g.setColour (p.bands[b]); g.fillEllipse (Rectangle<float> (18, 18).withCentre (c));
        g.setColour (juce::Colours::white.withAlpha (0.35f)); g.drawEllipse (Rectangle<float> (18, 18).withCentre (c), 1.0f);
        text (g, Rectangle<float> (18, 18).withCentre (c), String (b + 1), 10.0f, F::Bold, Colour (0xff0b0c0d), Justification::centred);
        if (b == sel)
        {
            auto tip = Rectangle<float> (150, 40).withPosition (x + 22, y - 60);
            g.setColour (p.panelHi.withAlpha (0.96f)); g.fillRoundedRectangle (tip, 7);
            g.setColour (p.bands[b].withAlpha (0.6f)); g.drawRoundedRectangle (tip.reduced (0.5f), 7, 1);
            auto ti = tip.reduced (10, 5);
            text (g, ti.removeFromTop (15), U("BELL \xc2\xb7 DYNAMIC"), 9.0f, F::SemiBold, p.bands[b], Justification::centredLeft, 0.12f);
            text (g, ti, hz (bp.freq) + "   " + db (S->eqDynGain[b], 2) + "   Q " + String (bp.q, 2), 11.0f, F::Medium, p.text);
        }
    }

    // ---- control deck
    Deck d { g, p, r };
    d.panelBg();
    auto deck = r.reduced (18, 14);
    const auto& bp = bands[(size_t) sel];
    const auto bc = p.bands[sel];

    // band selector + type
    auto c1 = deck.removeFromLeft (360);
    sectionTitle (g, c1.removeFromTop (18), "BANDS", p, p.module[0]);
    c1.removeFromTop (8);
    {
        auto chips = c1.removeFromTop (50);
        const float cw = chips.getWidth() / 5.0f;
        for (int b = 0; b < 5; ++b)
        {
            auto ch = chips.removeFromLeft (cw).reduced (3, 0);
            const bool s = b == sel;
            g.setColour (s ? p.panelHi.brighter (0.06f) : p.well); g.fillRoundedRectangle (ch, 7);
            g.setColour (s ? p.bands[b] : p.edge); g.drawRoundedRectangle (ch.reduced (0.5f), 7, s ? 1.4f : 1.0f);
            auto top = ch.reduced (6, 5).removeFromTop (16);
            g.setColour (p.bands[b]); g.fillEllipse (top.removeFromLeft (8).withSizeKeepingCentre (8, 8));
            text (g, top.withTrimmedLeft (5), String (b + 1), 10.0f, F::Bold, s ? p.text : p.textDim);
            bandTypeIcon (g, top.removeFromRight (22).reduced (0, 2), bands[(size_t) b].type, s ? p.bands[b] : p.textMute);
            text (g, ch.reduced (6, 5).removeFromBottom (18), hz (bands[(size_t) b].freq).replace (" ", ""), 10.0f, F::Medium, s ? p.text : p.textDim, Justification::centredLeft);
        }
    }
    c1.removeFromTop (10);
    {
        auto rowA = c1.removeFromTop (44);
        selector (g, rowA.removeFromLeft (rowA.getWidth() * 0.5f - 6), "TYPE", "Bell", p);
        rowA.removeFromLeft (12);
        selector (g, rowA, "SLOPE", "12 dB/oct", p);
    }
    c1.removeFromTop (10);
    {
        auto pl = c1.removeFromTop (26);
        caption (g, pl.removeFromLeft (92), "PLACEMENT", p, Justification::centredLeft);
        segmented (g, pl, { "STEREO", "MID", "SIDE", "L", "R" }, 0, p, 10.0f);
    }

    deck.removeFromLeft (20); d.divider (deck.getX()); deck.removeFromLeft (20);

    // dynamics (right)
    auto c3 = deck.removeFromRight (300);
    deck.removeFromRight (20); d.divider (deck.getRight()); deck.removeFromRight (20);
    {
        auto t = c3.removeFromTop (18);
        sectionTitle (g, t, "DYNAMIC EQ", p, bc);
        pillToggle (g, t.removeFromRight (64).withSizeKeepingCentre (64, 20), "ON", true, p);
        c3.removeFromTop (8);
        auto act = c3.removeFromBottom (26);
        caption (g, act.removeFromLeft (110), "ACTIVITY", p, Justification::centredLeft);
        auto bar = act.withSizeKeepingCentre (act.getWidth() - 60, 5).withX (act.getX());
        g.setColour (p.well); g.fillRoundedRectangle (bar, 2.5f);
        g.setColour (bc); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.04f, 1.0f, std::abs (S->eqDynGain[sel]) / std::abs (bp.dynRangeDb))), 2.5f);
        text (g, act.removeFromRight (56), db (S->eqDynGain[sel], 2), 11.0f, F::Medium, p.text, Justification::centredRight);
        c3.removeFromBottom (6);
        KnobSpec k1 = K ("RANGE", db (bp.dynRangeDb, 2), nrm (bp.dynRangeDb, -24, 24), KnobSize::Secondary, true);
        KnobSpec k2 = K ("THRESHOLD", db (bp.dynThresholdDb, 1), nrm (bp.dynThresholdDb, -60, 0), KnobSize::Secondary);
        k1.accent = bc; k2.accent = bc;
        knobRow (g, c3, p, { k1, k2 });
    }

    // primary knobs (centre)
    {
        sectionTitle (g, deck.removeFromTop (18), U("BAND 3  \xc2\xb7  ") + hz (bp.freq), p, bc);
        deck.removeFromTop (6);
        KnobSpec f = K ("FREQUENCY", hz (bp.freq), nrmSkew (bp.freq, 10, 30000, 1000));
        KnobSpec gk = K ("GAIN", db (bp.gainDb, 2), nrm (bp.gainDb, -24, 24), KnobSize::Primary, true);
        KnobSpec q = K ("Q", String (bp.q, 3), nrmSkew (bp.q, 0.025f, 40, 1));
        f.accent = bc; gk.accent = bc; q.accent = bc;
        knobRow (g, deck, p, { f, gk, q });
    }
}

// ============================================================== SATURATION
void satStyleCard (Graphics& g, Rectangle<float> r, bm::SatStyle st, const String& name, bool sel, const Palette& p, Colour mc)
{
    g.setColour (sel ? p.panelHi.brighter (0.05f) : p.well); g.fillRoundedRectangle (r, 8);
    g.setColour (sel ? mc : p.edge); g.drawRoundedRectangle (r.reduced (0.5f), 8, sel ? 1.4f : 1.0f);
    auto in = r.reduced (10, 7);
    auto viz = in.removeFromLeft (38.0f).withSizeKeepingCentre (38.0f, juce::jmin (38.0f, in.getHeight()));
    Path c;
    for (int i = 0; i <= 24; ++i)
    {
        const float u = -1.0f + 2.0f * i / 24.0f;
        const float y = bm::Shaper::curve (st, u * 2.5f) / bm::Shaper::curve (st, 2.5f);
        const auto pt = juce::Point<float> (viz.getX() + viz.getWidth() * i / 24.0f, viz.getCentreY() - juce::jlimit (-1.2f, 1.2f, y) * viz.getHeight() * 0.42f);
        if (i == 0) c.startNewSubPath (pt); else c.lineTo (pt);
    }
    g.setColour (sel ? mc : p.textMute);
    g.strokePath (c, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    in.removeFromLeft (10);
    g.setColour (sel ? p.text : p.textDim);
    g.setFont (font (10.5f, F::SemiBold, 0.06f));
    g.drawFittedText (name, in.toNearestInt(), Justification::centredLeft, 2, 1.0f);
}

void satScreen (Graphics& g, Rectangle<float> r, const Palette& p)
{
    const auto& sp = S->params.sat;
    const auto mc = p.module[1];
    auto disp = r.removeFromTop (r.getHeight() * 0.58f);
    r.removeFromTop (12);
    display (g, disp, p, 12.0f);
    auto di = disp.reduced (14, 10);
    {
        auto tb = di.removeFromTop (24);
        sectionTitle (g, tb.removeFromLeft (220), "MASTER SATURATION", p, mc);
        text (g, tb, U("TEST TONE 1 kHz \xc2\xb7 \xe2\x88\x92") + "6 dBFS  " + U("\xc2\xb7") + "  THD " + String (S->thdPct, 3) + " %", 10.0f, F::Medium, p.textMute, Justification::centredRight, 0.06f);
    }
    di.removeFromTop (8);

    // transfer curve (square)
    auto tr = di.removeFromLeft (di.getHeight()).reduced (4);
    {
        caption (g, tr.removeFromTop (16), "TRANSFER", p, Justification::centredLeft);
        auto pl = tr.reduced (18, 8);
        pl = squareIn (pl, juce::jmin (pl.getWidth(), pl.getHeight()));
        for (int i = 0; i <= 8; ++i)
        {
            vline (g, pl.getX() + pl.getWidth() * i / 8, pl.getY(), pl.getBottom(), i == 4 ? p.gridStrong : p.grid);
            hline (g, pl.getY() + pl.getHeight() * i / 8, pl.getX(), pl.getRight(), i == 4 ? p.gridStrong : p.grid);
        }
        dashed (g, { pl.getX(), pl.getBottom(), pl.getRight(), pl.getY() }, p.textMute.withAlpha (0.6f));
        const float drv = std::pow (10.0f, bm::Shaper::driveDb (sp.drivePct) / 20.0f);
        const float outG = std::pow (10.0f, sp.outputDb / 20.0f);
        auto curve = [&] (float wet)
        {
            Path c;
            for (int i = 0; i <= 160; ++i)
            {
                const float x = -1.0f + 2.0f * i / 160.0f;
                const float y = outG * ((1 - wet) * x + wet * bm::Shaper::curve (sp.style, x * drv) / drv);
                const auto pt = juce::Point<float> (pl.getCentreX() + x * pl.getWidth() * 0.5f, pl.getCentreY() - juce::jlimit (-1.1f, 1.1f, y) * pl.getHeight() * 0.5f);
                if (i == 0) c.startNewSubPath (pt); else c.lineTo (pt);
            }
            return c;
        };
        g.setColour (mc.withAlpha (0.35f));
        g.strokePath (curve (1.0f), juce::PathStrokeType (1.2f));
        glowStroke (g, curve (sp.mixPct * 0.01f), mc, 2.2f, 10.0f);
        text (g, Rectangle<float> (pl.getX() + 8, pl.getY() + 6, 160, 14), U("\xe2\x80\x94  OUTPUT (MIX 30 %)"), 9.0f, F::SemiBold, mc, Justification::centredLeft, 0.08f);
        text (g, Rectangle<float> (pl.getX() + 8, pl.getY() + 20, 160, 14), U("\xe2\x80\x94  SHAPER (100 % WET)"), 9.0f, F::SemiBold, mc.withAlpha (0.55f), Justification::centredLeft, 0.08f);
        text (g, Rectangle<float> (pl.getRight() - 40, pl.getBottom() + 2, 40, 12), "IN", 9.0f, F::SemiBold, p.textMute, Justification::centredRight, 0.1f);
        text (g, Rectangle<float> (pl.getX() - 18, pl.getY(), 16, 12), "OUT", 8.0f, F::SemiBold, p.textMute, Justification::centredLeft, 0.0f);
    }
    di.removeFromLeft (16);
    vline (g, di.getX(), di.getY() + 8, di.getBottom() - 8, p.edge);
    di.removeFromLeft (20);

    // harmonics
    {
        caption (g, di.removeFromTop (16), "HARMONICS  (RELATIVE TO FUNDAMENTAL)", p, Justification::centredLeft);
        auto pl = di.reduced (30, 10).withTrimmedBottom (14);
        const float lo = -140.0f;
        for (int d = 0; d >= -140; d -= 20)
        {
            const float y = pl.getY() + pl.getHeight() * (d / lo);
            hline (g, y, pl.getX(), pl.getRight(), d == 0 ? p.gridStrong : p.grid);
            text (g, Rectangle<float> (pl.getX() - 34, y - 7, 30, 14), String (d), 9.0f, F::Medium, p.textMute, Justification::centredRight);
        }
        const int H = (int) S->harmonicsDb.size();
        const float bw = pl.getWidth() / (float) H;
        for (int h = 0; h < H; ++h)
        {
            const float v = juce::jlimit (lo, 0.0f, S->harmonicsDb[(size_t) h]);
            auto col = Rectangle<float> (pl.getX() + h * bw, pl.getY(), bw, pl.getHeight()).reduced (bw * 0.22f, 0);
            auto bar = col.withTop (pl.getY() + pl.getHeight() * (v / lo));
            const auto c = h == 0 ? p.textDim : mc.withMultipliedAlpha (h % 2 == 1 ? 1.0f : 0.7f);
            g.setGradientFill (juce::ColourGradient (c, 0, bar.getY(), c.withAlpha (0.15f), 0, pl.getBottom(), false));
            g.fillRoundedRectangle (bar, 3);
            text (g, Rectangle<float> (col.getX() - 10, pl.getBottom() + 2, col.getWidth() + 20, 12), "H" + String (h + 1), 9.0f, F::SemiBold, p.textMute, Justification::centred);
            if (h > 0 && S->harmonicsDb[(size_t) h] > lo)
                text (g, Rectangle<float> (col.getX() - 10, bar.getY() - 15, col.getWidth() + 20, 12), String (juce::roundToInt (S->harmonicsDb[(size_t) h])) + " dB", 8.5f, F::Medium, p.textDim, Justification::centred);
        }
    }

    // ---- deck
    Deck d { g, p, r };
    d.panelBg();
    auto deck = r.reduced (18, 14);
    auto c1 = deck.removeFromLeft (300);
    sectionTitle (g, c1.removeFromTop (18), "STYLE", p, mc);
    c1.removeFromTop (10);
    {
        const float h = (c1.getHeight() - 10) / 2.0f;
        auto rowA = c1.removeFromTop (h), rowB = c1.withTrimmedTop (10);
        satStyleCard (g, rowA.removeFromLeft (rowA.getWidth() * 0.5f - 5), bm::SatStyle::CleanTube, "CLEAN TUBE", true, p, mc);
        satStyleCard (g, rowA.withTrimmedLeft (10), bm::SatStyle::WarmTube, "WARM TUBE", false, p, mc);
        satStyleCard (g, rowB.removeFromLeft (rowB.getWidth() * 0.5f - 5), bm::SatStyle::Tape, "TAPE", false, p, mc);
        satStyleCard (g, rowB.withTrimmedLeft (10), bm::SatStyle::SoftClip, "SOFT CLIP", false, p, mc);
    }
    deck.removeFromLeft (20); d.divider (deck.getX()); deck.removeFromLeft (14);

    auto hero = deck.removeFromLeft (230);
    sectionTitle (g, hero.removeFromTop (18), "SATURATE", p, mc);
    {
        KnobSpec k = K ("DRIVE", String (sp.drivePct, 1) + " %", sp.drivePct / 100.0f, KnobSize::Hero);
        k.accent = mc;
        knob (g, hero.reduced (8, 0), k, p);
    }
    deck.removeFromLeft (14); d.divider (deck.getX()); deck.removeFromLeft (20);

    auto c4 = deck.removeFromRight (190);
    deck.removeFromRight (20); d.divider (deck.getRight()); deck.removeFromRight (20);
    {
        sectionTitle (g, c4.removeFromTop (18), "QUALITY", p, mc);
        c4.removeFromTop (10);
        caption (g, c4.removeFromTop (16), "OVERSAMPLING", p, Justification::centredLeft);
        segmented (g, c4.removeFromTop (28), { "1x", "2x", "4x", "8x", "16x" }, sp.osFactorLog2, p, 10.0f);
        c4.removeFromTop (12);
        pillToggle (g, c4.removeFromTop (28).withWidth (140), "HIGH QUALITY", sp.quality == 1, p);
        c4.removeFromTop (10);
        text (g, c4.removeFromTop (16), U("LINEAR PHASE \xc2\xb7 ") + String (S->params.sat.osFactorLog2 > 0 ? "4x" : "1x"), 9.5f, F::Medium, p.textMute, Justification::centredLeft, 0.06f);
    }
    sectionTitle (g, deck.removeFromTop (18), "SHAPE & BLEND", p, mc);
    deck.removeFromTop (4);
    auto rowA = deck.removeFromTop (deck.getHeight() * 0.5f).withTrimmedBottom (4), rowB = deck.withTrimmedTop (4);
    auto sk = [&] (const String& l, const String& v, float val, bool bip = false) { auto k = K (l, v, val, KnobSize::Secondary, bip); k.accent = mc; return k; };
    knobRow (g, rowA, p, { sk ("MIX", String (sp.mixPct, 0) + " %", sp.mixPct / 100.0f), sk ("TONE", String (sp.tone, 1), nrm (sp.tone, -100, 100), true),
                           sk ("DYNAMICS", String (sp.dynamics, 1), nrm (sp.dynamics, -100, 100), true) });
    knobRow (g, rowB, p, { sk ("FEEDBACK", String (sp.feedbackPct, 0) + " %", sp.feedbackPct / 100.0f), sk ("FB FREQ", hz (sp.feedbackFreq), nrmSkew (sp.feedbackFreq, 20, 20000, 1000)),
                           sk ("OUTPUT", db (sp.outputDb, 1), nrm (sp.outputDb, -24, 24), true) });
}

// ============================================================== COMPRESSOR
void compScreen (Graphics& g, Rectangle<float> r, const Palette& p)
{
    const auto& cp = S->params.comp;
    const auto mc = p.module[2];
    auto disp = r.removeFromTop (r.getHeight() * 0.58f);
    r.removeFromTop (12);
    display (g, disp, p, 12.0f);
    auto di = disp.reduced (14, 10);
    {
        auto tb = di.removeFromTop (24);
        sectionTitle (g, tb.removeFromLeft (220), "MASTER COMPRESSOR", p, mc);
        auto sty = tb.removeFromRight (240);
        segmented (g, sty, { "CLEAN", "GLUE", "PUNCH" }, (int) cp.style, p, 10.5f);
        caption (g, tb.removeFromRight (70), "STYLE", p, Justification::centredRight);
    }
    di.removeFromTop (8);

    // transfer
    auto tr = di.removeFromLeft (di.getHeight()).reduced (4);
    float liveIn = cp.thresholdDb;
    {
        caption (g, tr.removeFromTop (16), "TRANSFER", p, Justification::centredLeft);
        auto pl = tr.reduced (22, 6).withTrimmedBottom (10);
        pl = squareIn (pl, juce::jmin (pl.getWidth(), pl.getHeight()));
        const float lo = -60.0f;
        auto X = [&] (float dB) { return pl.getX() + pl.getWidth() * (1 - dB / lo); };
        auto Y = [&] (float dB) { return pl.getBottom() - pl.getHeight() * (1 - dB / lo); };
        for (int d = -60; d <= 0; d += 10)
        {
            vline (g, X ((float) d), pl.getY(), pl.getBottom(), p.grid);
            hline (g, Y ((float) d), pl.getX(), pl.getRight(), p.grid);
            text (g, Rectangle<float> (X ((float) d) - 14, pl.getBottom() + 2, 28, 12), String (d), 8.5f, F::Medium, p.textMute, Justification::centred);
        }
        // knee zone
        g.setColour (mc.withAlpha (0.07f));
        g.fillRect (Rectangle<float>::leftTopRightBottom (X (cp.thresholdDb - cp.kneeDb * 0.5f), pl.getY(), X (juce::jmin (0.0f, cp.thresholdDb + cp.kneeDb * 0.5f)), pl.getBottom()));
        dashed (g, { X (cp.thresholdDb), pl.getY(), X (cp.thresholdDb), pl.getBottom() }, mc.withAlpha (0.6f));
        dashed (g, { pl.getX(), pl.getBottom(), pl.getRight(), pl.getY() }, p.textMute.withAlpha (0.5f));
        Path c;
        for (int i = 0; i <= 200; ++i)
        {
            const float x = lo + (-lo) * i / 200.0f;
            const float y = x - bm::CompCurve::gainReductionDb (x, cp.thresholdDb, cp.ratio, cp.kneeDb);
            if (i == 0) c.startNewSubPath (X (x), Y (y)); else c.lineTo (X (x), Y (y));
        }
        glowStroke (g, c, p.curve, 2.0f, 8.0f);
        // live level dot: input level where the static curve gives the measured GR
        for (float x = lo; x <= 0.0f; x += 0.05f)
            if (bm::CompCurve::gainReductionDb (x, cp.thresholdDb, cp.ratio, cp.kneeDb) >= S->compGrNow) { liveIn = x; break; }
        const auto dot = juce::Point<float> (X (liveIn), Y (liveIn - S->compGrNow));
        juce::DropShadow (mc, 12, {}).drawForRectangle (g, Rectangle<float> (10, 10).withCentre (dot).toNearestInt());
        g.setColour (mc); g.fillEllipse (Rectangle<float> (10, 10).withCentre (dot));
        g.setColour (juce::Colours::white); g.fillEllipse (Rectangle<float> (4, 4).withCentre (dot));
        text (g, Rectangle<float> (X (cp.thresholdDb) + 6, pl.getY() + 4, 120, 13), "THR " + db (cp.thresholdDb, 1), 9.0f, F::SemiBold, mc, Justification::centredLeft, 0.06f);
        text (g, Rectangle<float> (pl.getX() + 6, pl.getY() + 4, 100, 13), String (cp.ratio, 1) + " : 1", 10.0f, F::SemiBold, p.text, Justification::centredLeft);
        text (g, Rectangle<float> (pl.getX() + 6, pl.getY() + 18, 100, 13), "KNEE " + String (cp.kneeDb, 0) + " dB", 9.0f, F::SemiBold, p.textMute, Justification::centredLeft, 0.06f);
    }
    di.removeFromLeft (14);
    vline (g, di.getX(), di.getY() + 8, di.getBottom() - 8, p.edge);
    di.removeFromLeft (18);

    // meters (right)
    auto mt = di.removeFromRight (128);
    {
        caption (g, mt.removeFromTop (16), "LEVEL / GR", p, Justification::centred);
        auto m = mt.reduced (10, 6).withTrimmedBottom (34);
        const float w = 14, gap = (m.getWidth() - 3 * w) / 2.0f;
        auto mIn = m.removeFromLeft (w); m.removeFromLeft (gap);
        auto mGr = m.removeFromLeft (w); m.removeFromLeft (gap);
        auto mOut = m.removeFromLeft (w);
        const float makeup = cp.autoGain ? bm::CompCurve::autoMakeupDb (cp.thresholdDb, cp.ratio, cp.kneeDb) : cp.makeupDb;
        float inPk = S->preTapPeakDb.back(), outPk = inPk - S->compGrNow + makeup;
        meterBar (g, mIn, inPk, p, true, -48.0f);
        meterBar (g, mOut, outPk, p, true, -48.0f);
        g.setColour (p.well); g.fillRoundedRectangle (mGr, 2);
        g.setColour (mc); g.fillRoundedRectangle (mGr.withHeight (mGr.getHeight() * juce::jlimit (0.0f, 1.0f, S->compGrNow / 24.0f)), 2);
        auto lab = mt.removeFromBottom (32);
        const char* L3[3] = { "IN", "GR", "OUT" };
        const String V3[3] = { String (inPk, 1), String (-S->compGrNow, 1), String (outPk, 1) };
        const float xs[3] = { mIn.getCentreX(), mGr.getCentreX(), mOut.getCentreX() };
        for (int i = 0; i < 3; ++i)
        {
            text (g, Rectangle<float> (xs[i] - 22, lab.getY(), 44, 14), L3[i], 9.0f, F::SemiBold, p.textMute, Justification::centred, 0.1f);
            text (g, Rectangle<float> (xs[i] - 22, lab.getY() + 14, 44, 14), V3[i], 10.0f, F::Medium, i == 1 ? mc : p.text, Justification::centred);
        }
    }
    di.removeFromRight (14);
    vline (g, di.getRight(), di.getY() + 8, di.getBottom() - 8, p.edge);
    di.removeFromRight (14);

    // GR history
    {
        auto hd = di.removeFromTop (16);
        caption (g, hd, U("LEVEL & GAIN REDUCTION  \xc2\xb7  5 s"), p, Justification::centredLeft);
        auto lg = hd.removeFromRight (190);
        g.setColour (p.specPre.brighter (0.5f)); g.fillRoundedRectangle (lg.removeFromLeft (10).withSizeKeepingCentre (10, 4), 2);
        text (g, lg.removeFromLeft (64).withTrimmedLeft (6), "INPUT", 9.0f, F::SemiBold, p.textMute, Justification::centredLeft, 0.1f);
        g.setColour (mc); g.fillRoundedRectangle (lg.removeFromLeft (10).withSizeKeepingCentre (10, 4), 2);
        text (g, lg.withTrimmedLeft (6), "GAIN REDUCTION", 9.0f, F::SemiBold, p.textMute, Justification::centredLeft, 0.1f);
        auto pl = di.reduced (0, 8).withTrimmedRight (28);
        const float lo = -36.0f;
        auto Y = [&] (float dB) { return pl.getY() + pl.getHeight() * juce::jlimit (0.0f, 1.0f, dB / lo); };
        for (int d = 0; d >= -36; d -= 6)
        {
            hline (g, Y ((float) d), pl.getX(), pl.getRight(), p.grid);
            text (g, Rectangle<float> (pl.getRight() + 4, Y ((float) d) - 7, 26, 14), String (d), 9.0f, F::Medium, p.textMute, Justification::centredLeft);
        }
        dashed (g, { pl.getX(), Y (cp.thresholdDb), pl.getRight(), Y (cp.thresholdDb) }, mc.withAlpha (0.5f));
        // peak envelope with a 300 ms fall-back, as the plug-in's level history does
        std::vector<float> lv (S->preTapPeakDb);
        const float fall = 20.0f * (float) S->blockSize / (float) S->fs / 0.3f;
        for (size_t i = 1; i < lv.size(); ++i) lv[i] = std::max (lv[i], lv[i - 1] - fall);
        const auto& gr = S->compGr;
        const int n = (int) juce::jmin ((size_t) (5.0 * S->fs / S->blockSize), lv.size());
        const size_t off = lv.size() - (size_t) n;
        Path area, grLine, grArea;
        area.startNewSubPath (pl.getX(), pl.getBottom());
        grArea.startNewSubPath (pl.getX(), pl.getY());
        for (int i = 0; i < n; ++i)
        {
            const float x = pl.getX() + pl.getWidth() * i / (float) (n - 1);
            area.lineTo (x, Y (lv[off + (size_t) i]));
            const float gy = Y (-gr[off + (size_t) i]);
            grArea.lineTo (x, gy);
            if (i == 0) grLine.startNewSubPath (x, gy); else grLine.lineTo (x, gy);
        }
        area.lineTo (pl.getRight(), pl.getBottom()); area.closeSubPath();
        grArea.lineTo (pl.getRight(), pl.getY()); grArea.closeSubPath();
        g.setGradientFill (juce::ColourGradient (p.specPre.brighter (0.5f).withAlpha (0.7f), 0, pl.getY(), p.specPre.withAlpha (0.25f), 0, pl.getBottom(), false));
        g.fillPath (area);
        g.setColour (mc.withAlpha (0.18f)); g.fillPath (grArea);
        glowStroke (g, grLine, mc, 1.6f, 6.0f);
    }

    // ---- deck
    Deck d { g, p, r };
    d.panelBg();
    auto deck = r.reduced (18, 14);
    auto prim = deck.removeFromLeft (deck.getWidth() * 0.47f);
    sectionTitle (g, prim.removeFromTop (18), "DYNAMICS", p, mc);
    prim.removeFromTop (6);
    auto pk = [&] (const String& l, const String& v, float val) { auto k = K (l, v, val); k.accent = mc; return k; };
    knobRow (g, prim, p, { pk ("THRESHOLD", db (cp.thresholdDb, 1), nrm (cp.thresholdDb, -60, 0)),
                           pk ("RATIO", String (cp.ratio, 2) + " : 1", nrmSkew (cp.ratio, 1, 20, 3)),
                           pk ("ATTACK", ms (cp.attackMs), nrmSkew (cp.attackMs, 0.005f, 250, 10)),
                           pk ("RELEASE", ms (cp.releaseMs), nrmSkew (cp.releaseMs, 10, 2500, 200)) });
    deck.removeFromLeft (18); d.divider (deck.getX()); deck.removeFromLeft (18);

    auto side = deck.removeFromRight (196);
    deck.removeFromRight (18); d.divider (deck.getRight()); deck.removeFromRight (18);
    {
        sectionTitle (g, side.removeFromTop (18), "AUTO", p, mc);
        side.removeFromTop (8);
        pillToggle (g, side.removeFromTop (28), "AUTO GAIN", cp.autoGain, p);
        side.removeFromTop (8);
        pillToggle (g, side.removeFromTop (28), "AUTO RELEASE", cp.autoRelease, p);
        side.removeFromTop (14);
        // collapsed advanced sidechain card
        auto sc = side.removeFromTop (40);
        g.setColour (p.well); g.fillRoundedRectangle (sc, 8);
        g.setColour (p.edge); g.drawRoundedRectangle (sc.reduced (0.5f), 8, 1);
        auto si = sc.reduced (12, 4);
        text (g, si.removeFromTop (17), U("SIDECHAIN  \xe2\x96\xb8"), 10.0f, F::SemiBold, p.text, Justification::bottomLeft, 0.1f);
        text (g, si, U("HP off \xc2\xb7 LP off \xc2\xb7 LINK ") + String (cp.stereoLinkPct, 0) + " %", 9.5f, F::Medium, p.textMute, Justification::topLeft);
    }
    sectionTitle (g, deck.removeFromTop (18), "SHAPE", p, mc);
    deck.removeFromTop (4);
    auto rowA = deck.removeFromTop (deck.getHeight() * 0.5f).withTrimmedBottom (4), rowB = deck.withTrimmedTop (4);
    auto sk = [&] (const String& l, const String& v, float val) { auto k = K (l, v, val, KnobSize::Secondary); k.accent = mc; return k; };
    knobRow (g, rowA, p, { sk ("KNEE", String (cp.kneeDb, 1) + " dB", nrm (cp.kneeDb, 0, 72)), sk ("RANGE", String (cp.rangeDb, 1) + " dB", nrm (cp.rangeDb, 0, 60)),
                           sk ("HOLD", ms (cp.holdMs), nrm (cp.holdMs, 0, 500)) });
    knobRow (g, rowB, p, { sk ("LOOKAHEAD", ms (cp.lookaheadMs), nrm (cp.lookaheadMs, 0, 20)),
                           sk ("MAKEUP", cp.autoGain ? "AUTO" : db (cp.makeupDb, 1), nrm (cp.makeupDb, -12, 36)),
                           sk ("MIX", String (cp.mixPct, 0) + " %", cp.mixPct / 100.0f) });
}

// ================================================================= LIMITER
void limScreen (Graphics& g, Rectangle<float> r, const Palette& p)
{
    const auto& lp = S->params.lim;
    const auto mc = p.module[3];
    auto disp = r.removeFromTop (r.getHeight() * 0.58f);
    r.removeFromTop (12);
    display (g, disp, p, 12.0f);
    auto di = disp.reduced (14, 10);
    {
        auto tb = di.removeFromTop (24);
        sectionTitle (g, tb.removeFromLeft (200), "MASTER LIMITER", p, mc);
        auto sty = tb.removeFromRight (360);
        segmented (g, sty, { "TRANSPARENT", "PUNCHY", "DYNAMIC", "SAFE" }, (int) lp.style, p, 10.0f);
        caption (g, tb.removeFromRight (70), "STYLE", p, Justification::centredRight);
    }
    di.removeFromTop (8);

    // loudness panel (right)
    auto lu = di.removeFromRight (250);
    {
        g.setColour (p.panel.withAlpha (0.6f)); g.fillRoundedRectangle (lu, 10);
        g.setColour (p.edge); g.drawRoundedRectangle (lu.reduced (0.5f), 10, 1);
        auto in = lu.reduced (18, 14);
        caption (g, in.removeFromTop (14), "INTEGRATED", p, Justification::centredLeft);
        auto big = in.removeFromTop (54);
        text (g, big, String (S->lufsI, 1), 44.0f, F::SemiBold, p.text, Justification::centredLeft, -0.01f);
        text (g, big.withTrimmedLeft (98).withTrimmedTop (16), "LUFS", 12.0f, F::SemiBold, p.textDim, Justification::centredLeft, 0.1f);
        in.removeFromTop (8);
        auto row = [&] (const String& l, const String& v, Colour c)
        {
            auto rr = in.removeFromTop (25);
            hline (g, rr.getY(), rr.getX(), rr.getRight(), p.edge);
            caption (g, rr, l, p, Justification::centredLeft);
            text (g, rr, v, 12.0f, F::Medium, c, Justification::centredRight);
        };
        row ("SHORT-TERM", String (S->lufsS, 1) + " LUFS", p.text);
        row ("MOMENTARY", String (S->lufsM, 1) + " LUFS", p.text);
        row ("TRUE PEAK MAX", String (S->truePeakDb, 2) + " dBTP", p.text);
        row ("GR MAX", String (-S->limGrMax, 1) + " dB", mc);
        row ("LATENCY", String (S->latency) + U(" smp \xc2\xb7 ") + String (S->latency * 1000.0 / S->fs, 1) + " ms", p.textDim);
        row ("TRUE PEAK OS", String (S->limOs) + "x", p.textDim);
    }
    di.removeFromRight (16);

    // waveform + GR history
    {
        auto hd = di.removeFromTop (16);
        caption (g, hd, U("OUTPUT  \xc2\xb7  GAIN REDUCTION  \xc2\xb7  5 s"), p, Justification::centredLeft);
        auto lg = hd.removeFromRight (250);
        g.setColour (p.textMute); g.fillRoundedRectangle (lg.removeFromLeft (10).withSizeKeepingCentre (10, 4), 2);
        text (g, lg.removeFromLeft (64).withTrimmedLeft (6), "INPUT", 9.0f, F::SemiBold, p.textMute, Justification::centredLeft, 0.1f);
        g.setColour (p.accent); g.fillRoundedRectangle (lg.removeFromLeft (10).withSizeKeepingCentre (10, 4), 2);
        text (g, lg.removeFromLeft (70).withTrimmedLeft (6), "OUTPUT", 9.0f, F::SemiBold, p.textMute, Justification::centredLeft, 0.1f);
        g.setColour (mc); g.fillRoundedRectangle (lg.removeFromLeft (10).withSizeKeepingCentre (10, 4), 2);
        text (g, lg.withTrimmedLeft (6), "GR", 9.0f, F::SemiBold, p.textMute, Justification::centredLeft, 0.1f);

        auto pl = di.reduced (0, 8).withTrimmedRight (30);
        const float lo = -27.0f, top = 9.0f;
        auto Y = [&] (float dB) { return pl.getY() + pl.getHeight() * juce::jlimit (0.0f, 1.0f, (top - dB) / (top - lo)); };
        for (int d = 9; d >= -27; d -= 3)
        {
            hline (g, Y ((float) d), pl.getX(), pl.getRight(), p.grid);
            text (g, Rectangle<float> (pl.getRight() + 4, Y ((float) d) - 7, 28, 14), String (d), 9.0f, F::Medium, p.textMute, Justification::centredLeft);
        }
        std::vector<float> outPk (S->outPeakDb), inPk (S->outPeakDb);
        const auto& gr = S->limGr;
        for (size_t i = 0; i < inPk.size(); ++i) inPk[i] += gr[i];
        const float fall = 20.0f * (float) S->blockSize / (float) S->fs / 0.3f;
        for (auto* v : { &outPk, &inPk })
            for (size_t i = 1; i < v->size(); ++i) (*v)[i] = std::max ((*v)[i], (*v)[i - 1] - fall);
        const int n = (int) juce::jmin ((size_t) (5.0 * S->fs / S->blockSize), outPk.size());
        const size_t off = outPk.size() - (size_t) n;
        Path inA, outA, grA, grL;
        inA.startNewSubPath (pl.getX(), pl.getBottom());
        outA.startNewSubPath (pl.getX(), pl.getBottom());
        grA.startNewSubPath (pl.getX(), Y (top));
        for (int i = 0; i < n; ++i)
        {
            const float x = pl.getX() + pl.getWidth() * i / (float) (n - 1);
            const float o = outPk[off + (size_t) i], gdb = gr[off + (size_t) i];
            inA.lineTo (x, Y (inPk[off + (size_t) i]));
            outA.lineTo (x, Y (o));
            const float gy = Y (top - gdb);   // GR drawn down from the top, same dB scale
            grA.lineTo (x, gy);
            if (i == 0) grL.startNewSubPath (x, gy); else grL.lineTo (x, gy);
        }
        for (auto* pa : { &inA, &outA }) { pa->lineTo (pl.getRight(), pl.getBottom()); pa->closeSubPath(); }
        grA.lineTo (pl.getRight(), Y (top)); grA.closeSubPath();
        g.setColour (p.textMute.withAlpha (0.45f)); g.fillPath (inA);
        g.setGradientFill (juce::ColourGradient (p.accent.withAlpha (0.55f), 0, pl.getY(), p.accent.withAlpha (0.12f), 0, pl.getBottom(), false));
        g.fillPath (outA);
        g.setColour (mc.withAlpha (0.20f)); g.fillPath (grA);
        glowStroke (g, grL, mc, 1.6f, 6.0f);
        // ceiling
        const float cy = Y (lp.ceilingDb);
        g.setColour (p.text.withAlpha (0.85f)); g.fillRect (pl.getX(), cy - 0.5f, pl.getWidth(), 1.2f);
        auto tag = Rectangle<float> (128, 18).withPosition (pl.getX() + 8, cy + 5);
        g.setColour (p.panelHi.withAlpha (0.9f)); g.fillRoundedRectangle (tag, 4);
        text (g, tag, "CEILING " + String (lp.ceilingDb, 1) + " dBTP", 9.5f, F::SemiBold, p.text, Justification::centred, 0.06f);
    }

    // ---- deck
    Deck d { g, p, r };
    d.panelBg();
    auto deck = r.reduced (18, 14);
    auto hero = deck.removeFromLeft (230);
    sectionTitle (g, hero.removeFromTop (18), "DRIVE", p, mc);
    {
        KnobSpec k = K ("GAIN", db (lp.inputGainDb, 1), nrm (lp.inputGainDb, -12, 30), KnobSize::Hero);
        k.accent = p.accent;
        knob (g, hero.reduced (8, 0), k, p);
    }
    deck.removeFromLeft (14); d.divider (deck.getX()); deck.removeFromLeft (18);
    auto ceil = deck.removeFromLeft (150);
    sectionTitle (g, ceil.removeFromTop (18), "CEILING", p, mc);
    ceil.removeFromTop (14);
    {
        KnobSpec k = K ("OUTPUT", String (lp.ceilingDb, 2) + " dB", nrm (lp.ceilingDb, -12, 0));
        knob (g, ceil.withTrimmedBottom (ceil.getHeight() * 0.0f), k, p);
    }
    deck.removeFromLeft (18); d.divider (deck.getX()); deck.removeFromLeft (18);
    auto side = deck.removeFromRight (210);
    deck.removeFromRight (18); d.divider (deck.getRight()); deck.removeFromRight (18);
    {
        sectionTitle (g, side.removeFromTop (18), "TRUE PEAK & OUTPUT", p, mc);
        side.removeFromTop (8);
        pillToggle (g, side.removeFromTop (28).withWidth (150), "TRUE PEAK", lp.truePeak, p);
        side.removeFromTop (10);
        caption (g, side.removeFromTop (16), "OVERSAMPLING", p, Justification::centredLeft);
        segmented (g, side.removeFromTop (28), { "1x", "2x", "4x", "8x", "16x" }, lp.osFactorLog2, p, 10.0f);
        side.removeFromTop (8);
        selector (g, side.removeFromTop (44), "DITHER", "Off", p);
    }
    sectionTitle (g, deck.removeFromTop (18), "ENVELOPE & LINK", p, mc);
    deck.removeFromTop (4);
    auto rowA = deck.removeFromTop (deck.getHeight() * 0.5f).withTrimmedBottom (4), rowB = deck.withTrimmedTop (4);
    auto sk = [&] (const String& l, const String& v, float val) { auto k = K (l, v, val, KnobSize::Secondary); return k; };
    knobRow (g, rowA, p, { sk ("ATTACK", ms (lp.attackMs), nrmSkew (lp.attackMs, 0.1f, 1000, 50)), sk ("RELEASE", ms (lp.releaseMs), nrmSkew (lp.releaseMs, 1, 10000, 300)),
                           sk ("LOOKAHEAD", String (lp.lookaheadMs, 3) + " ms", nrm (lp.lookaheadMs, 0, 5)) });
    knobRow (g, rowB, p, { sk ("LINK TRANS.", String (lp.linkTransientsPct, 0) + " %", lp.linkTransientsPct / 100.0f),
                           sk ("LINK REL.", String (lp.linkReleasePct, 0) + " %", lp.linkReleasePct / 100.0f) }, 3);
}

// ================================================================== screens
using ScreenFn = void (*) (Graphics&, Rectangle<float>, const Palette&);

juce::Image renderScreen (const Palette& p, int module, float W, float H, float scale, const HeaderState& hs = {})
{
    juce::Image img (juce::Image::ARGB, (int) (W * scale), (int) (H * scale), true);
    Graphics g (img);
    g.addTransform (juce::AffineTransform::scale (scale));
    Rectangle<float> r (0, 0, W, H);
    background (g, r, p);
    header (g, r.removeFromTop (64), p, hs);
    navStrip (g, r.removeFromTop (62), p, module);
    globalBar (g, r.removeFromBottom (62), p);
    auto main = r.reduced (16, 8).withTrimmedBottom (4);
    const ScreenFn fns[4] = { eqScreen, satScreen, compScreen, limScreen };
    fns[module] (g, main, p);
    // resize grip
    icon (g, Rectangle<float> (W - 16, H - 16, 14, 14), Icon::Resize, p.textMute, 1.2f);
    return img;
}

void save (const juce::Image& img, const juce::File& f)
{
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat png;
    png.writeImageToStream (img, os);
    std::printf ("wrote %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}

// ======================================================= interaction states
void popoverShadow (Graphics& g, Rectangle<float> r)
{
    Path sh; sh.addRoundedRectangle (r, 12.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 28, { 0, 10 }).drawForPath (g, sh);
}

juce::Image renderStates (const Palette& p, float scale)
{
    const float W = 1200, H = 690;
    juce::Image img (juce::Image::ARGB, (int) (W * scale), (int) (H * scale), true);
    Graphics g (img);
    g.addTransform (juce::AffineTransform::scale (scale));
    Rectangle<float> r (0, 0, W, H);
    background (g, r, p);
    auto lbl = [&] (Rectangle<float> a, const String& t) { text (g, a, t, 11.0f, F::SemiBold, p.textDim, Justification::centredLeft, 0.14f); };

    lbl (r.removeFromTop (30).reduced (16, 0).translated (0, 6), U("1  PRESET MODIFIED \xe2\x80\x94 REVERT IS ENABLED, UNDO AVAILABLE"));
    HeaderState hs; hs.modified = true; hs.canUndo = true; hs.canRedo = true;
    auto h1 = r.removeFromTop (64);
    header (g, h1, p, hs);
    r.removeFromTop (10);
    lbl (r.removeFromTop (24).reduced (16, 0), U("2  CLICK REVERT \xe2\x86\x92 CONFIRMATION (ESC = CANCEL, THE REVERT ITSELF CAN BE UNDONE)"));
    auto h2 = r.removeFromTop (64);
    header (g, h2, p, hs);

    // preset browser dropdown (left part of the lower area)
    {
        auto area = r.withTrimmedTop (16);
        auto pop = Rectangle<float> (300, 300).withPosition (24, area.getY() + 24);
        lbl (Rectangle<float> (pop.getX() - 8, area.getY(), 400, 22), "3  PRESET BROWSER (CLICK THE NAME)");
        popoverShadow (g, pop);
        g.setColour (p.panelHi); g.fillRoundedRectangle (pop, 12);
        g.setColour (p.edgeHi); g.drawRoundedRectangle (pop.reduced (0.5f), 12, 1);
        auto in = pop.reduced (8, 10);
        caption (g, in.removeFromTop (20).withTrimmedLeft (10), "FACTORY", p, Justification::centredLeft, p.textMute);
        const String names[] = { U("KALOX MASTER \xe2\x80\x94 ORIGINAL"), "AFRO HOUSE MASTER", "DEEP HOUSE MASTER", "TECH HOUSE MASTER", "CLUB MASTER", "TRANSPARENT MASTER", "CUSTOM USER PRESET" };
        for (int i = 0; i < 7; ++i)
        {
            auto it = in.removeFromTop (26);
            if (i == 0) { g.setColour (p.accentSoft); g.fillRoundedRectangle (it, 6); }
            text (g, it.withTrimmedLeft (12), names[i], 12.0f, i == 0 ? F::SemiBold : F::Regular, i == 0 ? p.text : p.textDim);
            if (i == 0) { g.setColour (p.accent); g.fillEllipse (Rectangle<float> (6, 6).withCentre ({ it.getRight() - 14, it.getCentreY() })); }
        }
        in.removeFromTop (4);
        hline (g, in.getY(), in.getX() + 6, in.getRight() - 6, p.edge);
        in.removeFromTop (6);
        caption (g, in.removeFromTop (20).withTrimmedLeft (10), "USER", p, Justification::centredLeft, p.textMute);
        text (g, in.removeFromTop (24).withTrimmedLeft (12), "My Club Master v2", 12.0f, F::Regular, p.textDim);
        text (g, in.removeFromTop (24).withTrimmedLeft (12), U("Save as\xe2\x80\xa6        Show folder"), 11.5f, F::Medium, p.accent);
    }

    // knob anatomy / states (right part of the lower area)
    {
        auto area = r.withTrimmedTop (200).withTrimmedLeft (360).reduced (16, 0);
        lbl (Rectangle<float> (area.getX(), area.getY(), 600, 22), U("4  CONTROL FAMILY \xe2\x80\x94 HERO \xc2\xb7 PRIMARY \xc2\xb7 SECONDARY \xc2\xb7 MINI \xc2\xb7 HOVER \xc2\xb7 DISABLED"));
        auto row = area.withTrimmedTop (30).withHeight (210);
        auto hk = K ("HERO", "+11.0 dB", 0.55f, KnobSize::Hero);
        knob (g, row.removeFromLeft (200), hk, p);
        row.removeFromLeft (10);
        knob (g, row.removeFromLeft (120).withSizeKeepingCentre (120, 150), K ("PRIMARY", "-18.0 dB", 0.7f), p);
        knob (g, row.removeFromLeft (100).withSizeKeepingCentre (100, 110), K ("SECONDARY", "20.0 dB", 0.28f, KnobSize::Secondary), p);
        auto hov = K ("HOVER", "2.00 : 1", 0.4f, KnobSize::Primary); hov.hover = true;
        knob (g, row.removeFromLeft (120).withSizeKeepingCentre (120, 150), hov, p);
        {
            auto mk = K ("MINI", "100 %", 0.5f, KnobSize::Mini);
            knob (g, row.removeFromLeft (80).withSizeKeepingCentre (80, 80), mk, p);
        }
        knob (g, row.removeFromLeft (100).withSizeKeepingCentre (100, 110), K ("DISABLED", "0.0 ms", 0.0f, KnobSize::Secondary, false, false), p);
        auto rest = area.withTrimmedTop (250).withHeight (34);
        pillToggle (g, rest.removeFromLeft (130), "TRUE PEAK", true, p);
        rest.removeFromLeft (10);
        pillToggle (g, rest.removeFromLeft (130), "AUTO RELEASE", false, p);
        rest.removeFromLeft (10);
        segmented (g, rest.removeFromLeft (200), { "1x", "2x", "4x", "8x", "16x" }, 4, p);
        rest.removeFromLeft (10);
        undoRedo (g, rest.removeFromLeft (164), p, true, false);
    }

    // revert confirmation popover anchored under the revert button
    {
        const float revertX = hs.revertBounds.getCentreX();
        auto pop = Rectangle<float> (360, 150).withPosition (revertX - 300, h2.getBottom() + 8);
        popoverShadow (g, pop);
        g.setColour (p.panelHi); g.fillRoundedRectangle (pop, 12);
        g.setColour (p.edgeHi); g.drawRoundedRectangle (pop.reduced (0.5f), 12, 1);
        Path arrowTip; arrowTip.addTriangle (revertX - 7, pop.getY() + 1, revertX + 7, pop.getY() + 1, revertX, pop.getY() - 7);
        g.setColour (p.panelHi); g.fillPath (arrowTip);
        auto in = pop.reduced (18, 14);
        auto t = in.removeFromTop (20);
        icon (g, t.removeFromLeft (18).withSizeKeepingCentre (16, 16), Icon::Revert, p.warn, 1.6f);
        text (g, t.withTrimmedLeft (8), "Revert to loaded preset?", 13.5f, F::SemiBold, p.text);
        in.removeFromTop (6);
        g.setColour (p.textDim);
        g.setFont (font (11.5f, F::Regular));
        g.drawFittedText (U("All changes since \"KALOX MASTER \xe2\x80\x94 ORIGINAL\" was loaded will be discarded. You can still undo this."),
                          in.removeFromTop (48).toNearestInt(), Justification::topLeft, 3, 1.0f);
        auto btns = in.removeFromBottom (32);
        auto ok = btns.removeFromRight (110), cancel = btns.removeFromRight (100).translated (-10, 0);
        g.setColour (p.well); g.fillRoundedRectangle (cancel, 8);
        g.setColour (p.edge); g.drawRoundedRectangle (cancel.reduced (0.5f), 8, 1);
        text (g, cancel, "CANCEL", 11.0f, F::SemiBold, p.textDim, Justification::centred, 0.1f);
        g.setColour (p.accent); g.fillRoundedRectangle (ok, 8);
        text (g, ok, "REVERT", 11.0f, F::Bold, p.accentInk, Justification::centred, 0.1f);
    }

    return img;
}

// ============================================================== brand sheet
juce::Image renderIcon (const Palette& p, int px)
{
    juce::Image img (juce::Image::ARGB, px, px, true);
    Graphics g (img);
    logoMark (g, Rectangle<float> (0, 0, (float) px, (float) px), p, true);
    return img;
}

juce::Image renderBrand (const Palette& p)
{
    const float W = 1600, H = 1040;
    juce::Image img (juce::Image::ARGB, (int) W, (int) H, true);
    Graphics g (img);
    Rectangle<float> r (0, 0, W, H);
    background (g, r, p);
    auto lbl = [&] (Rectangle<float> a, const String& t) { text (g, a, t, 12.0f, F::SemiBold, p.textDim, Justification::centredLeft, 0.16f); };

    text (g, Rectangle<float> (48, 28, 900, 24), U("BANKMASTER \xe2\x80\x94 BY KALOX  \xc2\xb7  IDENTITY  \xc2\xb7  CONCEPT ") + p.name, 13.0f, F::SemiBold, p.textDim, Justification::centredLeft, 0.16f);
    text (g, Rectangle<float> (48, 50, 900, 20), p.tagline, 12.0f, F::Regular, p.textMute);

    // main lockup
    auto lock = Rectangle<float> (48, 100, 900, 220);
    panel (g, lock, p, 18, true);
    logoMark (g, lock.withWidth (220).reduced (24), p, true);
    wordmark (g, lock.withTrimmedLeft (240).withTrimmedTop (66), p, 62.0f, true);

    // light background lockup check + mono
    auto mono = Rectangle<float> (980, 100, 572, 220);
    g.setColour (Colour (0xfff2f3f4)); g.fillRoundedRectangle (mono, 18);
    {
        Palette lp = p; lp.text = Colour (0xff121416); lp.textDim = Colour (0xff4a5156);
        lp.accent = p.accent.withMultipliedBrightness (0.8f).withMultipliedSaturation (1.1f);
        logoMark (g, mono.withWidth (180).reduced (26), p, true);
        wordmark (g, mono.withTrimmedLeft (190).withTrimmedTop (80), lp, 40.0f, true);
    }

    // app icon sizes at true pixel size
    lbl (Rectangle<float> (48, 350, 900, 20), U("APP ICON \xe2\x80\x94 macOS (.icns) \xc2\xb7 Windows (.ico) \xc2\xb7 DMG volume \xc2\xb7 Setup.exe \xe2\x80\x94 SHOWN AT REAL PIXEL SIZE"));
    float x = 48;
    for (int s : { 256, 128, 64, 48, 32, 24, 16 })
    {
        auto ic = renderIcon (p, s);
        g.drawImageAt (ic, (int) x, (int) (380 + (256 - s)));
        text (g, Rectangle<float> (x, 644, (float) juce::jmax (s, 40), 16), String (s) + " px", 10.0f, F::Medium, p.textMute, Justification::centredLeft);
        x += (float) s + 28;
    }

    // DMG window mock
    auto dmg = Rectangle<float> (48, 690, 560, 320);
    lbl (dmg.withHeight (20).translated (0, -24).withY (668), "DMG WINDOW");
    {
        juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 24, { 0, 8 }).drawForRectangle (g, dmg.toNearestInt());
        g.setColour (Colour (0xff2b2d30)); g.fillRoundedRectangle (dmg, 10);
        auto bar = dmg.removeFromTop (28);
        for (int i = 0; i < 3; ++i)
        {
            const juce::uint32 cs[] = { 0xffff5f57, 0xfffebc2e, 0xff28c840 };
            g.setColour (Colour (cs[i])); g.fillEllipse (bar.getX() + 12 + i * 20, bar.getCentreY() - 6, 12, 12);
        }
        text (g, bar, "BANKMASTER 1.0", 11.0f, F::Medium, Colour (0xffc8ccd0), Justification::centred);
        auto body = dmg.reduced (0).withTrimmedBottom (0);
        g.saveState();
        g.reduceClipRegion (body.toNearestInt());
        g.setGradientFill (juce::ColourGradient (p.bg0.brighter (0.05f), body.getX(), body.getY(), p.bg1, body.getRight(), body.getBottom(), false));
        g.fillRect (body);
        logoMark (g, Rectangle<float> (body.getX() + 22, body.getY() + 20, 50, 50), p, true);
        wordmark (g, Rectangle<float> (body.getX() + 82, body.getY() + 26, 300, 50), p, 22.0f, true);
        const char* labels[3] = { "Install BANKMASTER.pkg", "Read Me - Leia-me.txt", "Uninstall BANKMASTER.command" };
        for (int i = 0; i < 3; ++i)
        {
            const float cx = body.getX() + 100 + i * 180, cy = body.getY() + 160;
            if (i == 0) logoMark (g, Rectangle<float> (72, 72).withCentre ({ cx, cy }), p, true);
            else
            {
                auto doc = Rectangle<float> (50, 62).withCentre ({ cx, cy });
                g.setColour (i == 1 ? Colour (0xffe9ecef) : Colour (0xff3a3f44)); g.fillRoundedRectangle (doc, 5);
                text (g, doc, i == 1 ? "TXT" : ">_", 11.0f, F::Bold, i == 1 ? Colour (0xff4a5156) : p.textDim, Justification::centred);
            }
            text (g, Rectangle<float> (cx - 85, cy + 44, 170, 16), labels[i], 11.0f, F::Medium, Colour (0xffe6e8ea), Justification::centred);
        }
        text (g, Rectangle<float> (body.getX() + 24, body.getBottom() - 46, 520, 16), "1  Double-click Install BANKMASTER.pkg   2  Choose VST3 / AU / App   3  Rescan in your DAW",
              10.5f, F::Medium, p.textDim, Justification::centredLeft, 0.04f);
        g.restoreState();
    }

    // Setup.exe wizard mock
    auto setup = Rectangle<float> (650, 690, 500, 320);
    lbl (Rectangle<float> (650, 668, 400, 20), "WINDOWS SETUP WIZARD");
    {
        juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 24, { 0, 8 }).drawForRectangle (g, setup.toNearestInt());
        g.setColour (Colour (0xfff3f3f3)); g.fillRect (setup);
        auto bar = setup.removeFromTop (30);
        g.setColour (Colour (0xffffffff)); g.fillRect (bar);
        text (g, bar.withTrimmedLeft (34), "Setup - BANKMASTER 1.0.0", 11.0f, F::Regular, Colour (0xff222222));
        g.drawImage (renderIcon (p, 32), Rectangle<float> (6, bar.getY() + 6, 18, 18).withX (setup.getX() + 8));
        auto side = setup.removeFromLeft (164);
        g.setGradientFill (juce::ColourGradient (p.bg0, side.getX(), side.getY(), p.bg1, side.getX(), side.getBottom(), false));
        g.fillRect (side);
        logoMark (g, Rectangle<float> (side.getCentreX() - 46, side.getY() + 40, 92, 92), p, true);
        text (g, Rectangle<float> (side.getX(), side.getY() + 150, side.getWidth(), 24), "BANK", 22.0f, F::Display, p.text, Justification::centred, 0.06f);
        text (g, Rectangle<float> (side.getX(), side.getY() + 172, side.getWidth(), 24), "MASTER", 22.0f, F::Display, p.accent, Justification::centred, 0.06f);
        text (g, Rectangle<float> (side.getX(), side.getY() + 200, side.getWidth(), 16), "BY KALOX", 9.5f, F::SemiBold, p.textDim, Justification::centred, 0.42f);
        auto body = setup.reduced (22, 20);
        text (g, body.removeFromTop (28), "Welcome to the BANKMASTER Setup Wizard", 14.0f, F::SemiBold, Colour (0xff111111));
        g.setColour (Colour (0xff333333));
        g.setFont (font (11.5f));
        g.drawFittedText ("This will install BANKMASTER 1.0.0 (VST3 and Standalone) by Kalox Audio on your computer.\n\nClick Next to continue.",
                          body.removeFromTop (90).toNearestInt(), Justification::topLeft, 6, 1.0f);
        auto btns = body.removeFromBottom (26);
        auto b2 = btns.removeFromRight (84), b1 = btns.removeFromRight (84).translated (-8, 0);
        for (auto bb : { b1, b2 })
        {
            g.setColour (Colour (0xfffdfdfd)); g.fillRoundedRectangle (bb, 4);
            g.setColour (Colour (0xffb8b8b8)); g.drawRoundedRectangle (bb.reduced (0.5f), 4, 1);
        }
        text (g, b1, "Next >", 11.0f, F::Regular, Colour (0xff111111), Justification::centred);
        text (g, b2, "Cancel", 11.0f, F::Regular, Colour (0xff111111), Justification::centred);
    }

    // Ableton browser grouping mock
    auto ab = Rectangle<float> (1190, 690, 362, 320);
    lbl (Rectangle<float> (1190, 668, 400, 20), U("ABLETON LIVE BROWSER \xe2\x80\x94 VENDOR"));
    {
        g.setColour (Colour (0xff1e1e1e)); g.fillRect (ab);
        auto in = ab.reduced (14, 12);
        text (g, in.removeFromTop (22), "Plug-Ins", 12.0f, F::SemiBold, Colour (0xffd0d0d0));
        auto item = [&] (int level, const String& t, bool folder, bool hl)
        {
            auto rr = in.removeFromTop (22);
            if (hl) { g.setColour (Colour (0xff3b4a5a)); g.fillRect (rr); }
            rr.removeFromLeft (14.0f * (float) level);
            text (g, rr, (folder ? String (U("\xe2\x96\xbe  ")) : String ("    ")) + t, 11.5f, F::Regular, folder ? Colour (0xffbdbdbd) : Colour (0xffe8e8e8));
        };
        item (0, "VST3", true, false);
        item (1, "Kalox Audio", true, false);
        item (2, "BANKMASTER", false, true);
        item (2, "KaloxSooth", false, false);
        item (2, "KaloxStart", false, false);
        in.removeFromTop (14);
        g.setColour (Colour (0xff9a9a9a));
        g.setFont (font (10.5f));
        g.drawFittedText ("V2 reports the vendor as \"Kalox Audio\" (was \"KALOX\"), so BANKMASTER sits with the other Kalox plug-ins. "
                          "Manufacturer code Klox and plug-in code Bkms stay unchanged: existing Live sets keep loading it.",
                          in.toNearestInt(), Justification::topLeft, 8, 1.0f);
    }
    return img;
}

juce::Image renderOverview (const std::vector<juce::Image>& shots, const std::vector<String>& labels, const Palette& p)
{
    const int cols = 4, tw = 600, th = 390, pad = 24, lab = 26;
    const int rows = (int) (shots.size() + cols - 1) / cols;
    juce::Image img (juce::Image::ARGB, cols * tw + (cols + 1) * pad, rows * (th + lab) + (rows + 1) * pad, true);
    Graphics g (img);
    g.fillAll (Colour (0xff08090a));
    for (size_t i = 0; i < shots.size(); ++i)
    {
        const int cx = pad + (int) (i % cols) * (tw + pad), cy = pad + (int) (i / cols) * (th + lab + pad);
        text (g, Rectangle<float> ((float) cx, (float) cy, (float) tw, (float) lab - 6), labels[i], 13.0f, F::SemiBold, p.textDim, Justification::centredLeft, 0.12f);
        g.drawImage (shots[i], Rectangle<float> ((float) cx, (float) (cy + lab), (float) tw, (float) th));
    }
    return img;
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("usage: %s <fontsDir> <outDir>\n", argv[0]); return 1; }
    juce::File fonts (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    out.createDirectory();
    Fonts::get().load (fonts);
    if (Fonts::get().display == nullptr || Fonts::get().semibold == nullptr) { std::printf ("fonts not found in %s\n", fonts.getFullPathName().toRawUTF8()); return 1; }

    const auto res = preview::run();
    S = &res;
    std::printf ("sim: LUFS-I %.2f  S %.2f  TP %.2f dBTP  compGR %.2f (max %.2f)  limGR %.2f (max %.2f)  corr %.2f  latency %d  THD %.4f%%\n",
                 res.lufsI, res.lufsS, res.truePeakDb, res.compGrNow, res.compGrMax, res.limGrNow, res.limGrMax, res.corr, res.latency, res.thdPct);

    const char* mods[4] = { "1_eq", "2_saturation", "3_compressor", "4_limiter" };
    const char* modNames[4] = { "EQ", "SATURATION", "COMPRESSOR", "LIMITER" };
    std::vector<juce::Image> thumbs;
    std::vector<String> labels;
    for (int c = 0; c < 2; ++c)
    {
        const auto p = c == 0 ? conceptA() : conceptB();
        const String tag = c == 0 ? "A" : "B";
        for (int m = 0; m < 4; ++m)
        {
            auto img = renderScreen (p, m, 1200, 780, 2.0f);
            save (img, out.getChildFile ("concept" + tag + "_" + mods[m] + ".png"));
            thumbs.push_back (img);
            labels.push_back ("CONCEPT " + tag + U("  \xc2\xb7  ") + modNames[m]);
        }
        save (renderStates (p, 2.0f), out.getChildFile ("concept" + tag + "_5_states.png"));
        save (renderBrand (p), out.getChildFile ("concept" + tag + "_6_brand.png"));
        // minimum window size, rendered at 1x (worst case for legibility on a 100 % Windows display)
        save (renderScreen (p, 0, 1200, 780, 1000.0f / 1200.0f), out.getChildFile ("concept" + tag + "_7_min_size_1000x650_at_1x.png"));
        for (int s : { 1024, 256, 32, 16 })
            save (renderIcon (p, s), out.getChildFile ("icon" + tag + "_" + String (s) + ".png"));
    }
    save (renderOverview (thumbs, labels, conceptA()), out.getChildFile ("0_overview.png"));
    return 0;
}
