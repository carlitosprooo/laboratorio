// BANKMASTER — BY KALOX · V2 design system (concept A — OBSIDIAN / NEON).
// Pure JUCE drawing code shared by the plug-in UI (macOS + Windows) and the
// art / mock-up tool (Design/Preview). No platform-specific code.
#pragma once

#include <juce_graphics/juce_graphics.h>

namespace bmv2
{
using juce::Colour;
using juce::Graphics;
using juce::Path;
using juce::Rectangle;

// ============================================================================ palette
struct Palette
{
    Colour bg0, bg1;                 // window gradient
    Colour panel, panelHi, well;     // raised panel / highlight / recessed well (value fields)
    Colour edge, edgeHi;             // hairlines
    Colour text, textDim, textMute;
    Colour accent, accentSoft, accentInk; // accent / translucent accent / text on accent
    Colour metal;                    // metallic grey (inactive)
    Colour warn, danger, ok;
    Colour disp0, disp1, grid, gridStrong;
    Colour specPre, specPost, curve;
    Colour bands[5];
    Colour module[4];                // EQ green, SATURATION amber, COMPRESSOR turquoise, LIMITER coral
};

/** Concept A — obsidian black, graphite, metallic grey, controlled neon green (approved). */
inline const Palette& palette()
{
    static const Palette p = []
    {
        Palette q;
        q.bg0 = Colour (0xff131517); q.bg1 = Colour (0xff0a0b0c);
        q.panel = Colour (0xff17191b); q.panelHi = Colour (0xff202326); q.well = Colour (0xff0c0e0f);
        q.edge = Colour (0xff272b2e); q.edgeHi = Colour (0xff3a4044);
        // text levels tuned for legibility of small labels on 100 % (non-Retina) displays
        q.text = Colour (0xfff1f4f5); q.textDim = Colour (0xffadb6bb); q.textMute = Colour (0xff76818a);
        q.accent = Colour (0xffa6f23a); q.accentSoft = Colour (0x2ea6f23a); q.accentInk = Colour (0xff0d1205);
        q.metal = Colour (0xff6f787d);
        q.warn = Colour (0xffffc247); q.danger = Colour (0xffff5a5a); q.ok = Colour (0xffa6f23a);
        q.disp0 = Colour (0xff101214); q.disp1 = Colour (0xff0a0b0c);
        q.grid = Colour (0x15ffffff); q.gridStrong = Colour (0x29ffffff);
        q.specPre = Colour (0xff3d4347); q.specPost = Colour (0xffa6f23a); q.curve = Colour (0xfff4f7f8);
        const juce::uint32 b[] = { 0xffff6b6b, 0xffffb347, 0xffa6f23a, 0xff3ad6c8, 0xff59b8ff };
        for (int i = 0; i < 5; ++i) q.bands[i] = Colour (b[i]);
        const juce::uint32 m[] = { 0xffa6f23a, 0xffffb347, 0xff3ad6c8, 0xffff6b6b };
        for (int i = 0; i < 4; ++i) q.module[i] = Colour (m[i]);
        return q;
    }();
    return p;
}

// ============================================================================ type
/** Inter (UI) + Montserrat (wordmark), SIL OFL 1.1, embedded so text renders the same on every OS. */
struct Fonts
{
    juce::Typeface::Ptr regular, medium, semibold, bold, display, displaySemi;

    static Fonts& get() { static Fonts f; return f; }

    enum Weight { Regular, Medium, SemiBold, Bold, Display, DisplaySemi };

    void setFromMemory (Weight w, const void* data, size_t size)
    {
        auto t = juce::Typeface::createSystemTypefaceFor (data, size);
        switch (w)
        {
            case Regular: regular = t; break;
            case Medium: medium = t; break;
            case SemiBold: semibold = t; break;
            case Bold: bold = t; break;
            case Display: display = t; break;
            case DisplaySemi: displaySemi = t; break;
        }
    }

    void load (const juce::File& dir)
    {
        auto tf = [&] (Weight w, const char* name)
        {
            juce::MemoryBlock mb;
            if (dir.getChildFile (name).loadFileAsData (mb)) setFromMemory (w, mb.getData(), mb.getSize());
        };
        tf (Regular, "Inter-Regular.otf"); tf (Medium, "Inter-Medium.otf");
        tf (SemiBold, "Inter-SemiBold.otf"); tf (Bold, "Inter-Bold.otf");
        tf (Display, "Montserrat-ExtraBold.ttf"); tf (DisplaySemi, "Montserrat-SemiBold.ttf");
    }

    void clear() { regular = medium = semibold = bold = display = displaySemi = nullptr; }
    bool isLoaded() const { return regular != nullptr && semibold != nullptr; }

    juce::Font font (float h, Weight w = Regular, float kerning = 0.0f) const
    {
        juce::Typeface::Ptr t = w == Medium ? medium : w == SemiBold ? semibold : w == Bold ? bold
                              : w == Display ? display : w == DisplaySemi ? displaySemi : regular;
        auto opts = juce::FontOptions{}.withHeight (h);
        if (t != nullptr) opts = opts.withTypeface (t);
        return juce::Font (opts).withExtraKerningFactor (kerning);
    }
};

inline juce::Font font (float h, Fonts::Weight w = Fonts::Regular, float kerning = 0.0f) { return Fonts::get().font (h, w, kerning); }

// type scale (logical px at 100 %): never below 10 px; numbers use Medium for even stroke weight
namespace type
{
    constexpr float caption = 10.5f;     // tracked uppercase labels
    constexpr float small = 10.5f;       // secondary read-outs
    constexpr float value = 12.5f;       // values in fields
    constexpr float body = 12.5f;
    constexpr float title = 13.5f;
}

// ============================================================================ helpers
/** Luminous stroke: a few widening translucent passes under the core line.
    Cheap enough to run on every animation frame (no image blur). */
inline void glowStroke (Graphics& g, const Path& p, Colour c, float width, float glowRadius)
{
    const juce::PathStrokeType::JointStyle j = juce::PathStrokeType::curved;
    const juce::PathStrokeType::EndCapStyle e = juce::PathStrokeType::rounded;
    if (glowRadius > 0.0f)
    {
        const float passes[3][2] = { { 1.0f, 0.05f }, { 0.62f, 0.09f }, { 0.32f, 0.16f } };
        for (auto& ps : passes)
        {
            g.setColour (c.withMultipliedAlpha (ps[1]));
            g.strokePath (p, juce::PathStrokeType (width + glowRadius * ps[0] * 1.6f, j, e));
        }
    }
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (width, j, e));
}

inline void panel (Graphics& g, Rectangle<float> r, const Palette& p, float radius = 10.0f, bool raised = true)
{
    if (raised) // rectangle shadow: gradient based, no blur (cheap on every repaint)
        juce::DropShadow (juce::Colours::black.withAlpha (0.42f), 16, { 0, 5 }).drawForRectangle (g, r.reduced (radius * 0.5f).toNearestInt());
    g.setGradientFill (juce::ColourGradient (p.panelHi, r.getX(), r.getY(), p.panel, r.getX(), r.getY() + juce::jmin (120.0f, r.getHeight()), false));
    g.fillRoundedRectangle (r, radius);
    g.setColour (p.edge);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.045f));
    g.drawLine (r.getX() + radius, r.getY() + 1.0f, r.getRight() - radius, r.getY() + 1.0f, 1.0f);
}

inline void display (Graphics& g, Rectangle<float> r, const Palette& p, float radius = 8.0f)
{
    g.setGradientFill (juce::ColourGradient (p.disp0, r.getCentreX(), r.getY(), p.disp1, r.getCentreX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

inline void caption (Graphics& g, Rectangle<float> r, const juce::String& t, const Palette& p,
                     juce::Justification j = juce::Justification::centred, Colour c = {})
{
    g.setColour (c.isTransparent() ? p.textDim : c);
    g.setFont (font (type::caption, Fonts::SemiBold, 0.09f));
    g.drawText (t.toUpperCase(), r, j, false);
}

inline void valueField (Graphics& g, Rectangle<float> r, const juce::String& t, const Palette& p, bool active = false, bool enabled = true)
{
    g.setColour (p.well);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (active ? p.accent.withAlpha (0.7f) : p.edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    g.setColour (enabled ? p.text : p.textMute);
    g.setFont (font (type::value, Fonts::Medium));
    g.drawText (t, r, juce::Justification::centred, false);
}

// ============================================================================ knobs
enum class KnobSize { Hero, Primary, Secondary, Mini };

struct KnobSpec
{
    float value = 0.5f;          // 0..1
    bool bipolar = false;
    juce::String label, valueText;
    KnobSize size = KnobSize::Primary;
    bool hover = false, enabled = true, active = false;
    Colour accent;               // transparent = palette accent
};

inline float knobStartAngle() { return juce::MathConstants<float>::pi * 1.25f; }
inline float knobEndAngle() { return juce::MathConstants<float>::pi * 2.75f; }

/** Knob face only (no label), drawn in the square r. */
inline void knobFace (Graphics& g, Rectangle<float> r, const KnobSpec& k, const Palette& p)
{
    const float startA = knobStartAngle(), endA = knobEndAngle();
    const float size = juce::jmin (r.getWidth(), r.getHeight());
    const auto c = r.getCentre();
    const float R = size * 0.5f;
    const bool hero = k.size == KnobSize::Hero;
    const bool small = k.size == KnobSize::Secondary || k.size == KnobSize::Mini;
    const float ringW = hero ? juce::jmax (4.0f, size * 0.035f) : small ? juce::jmax (3.2f, size * 0.07f) : juce::jmax (3.0f, size * 0.05f);
    const float arcR = R - ringW * 0.5f - 1.0f;
    const auto acc = (k.accent.isTransparent() ? p.accent : k.accent).withMultipliedAlpha (k.enabled ? 1.0f : 0.35f);
    const float ang = startA + juce::jlimit (0.0f, 1.0f, k.value) * (endA - startA);

    // scale ticks (hero / primary)
    if (! small)
    {
        const int n = hero ? 41 : 11;
        for (int i = 0; i < n; ++i)
        {
            const float a = startA + (endA - startA) * i / (n - 1);
            const bool major = hero ? (i % 10 == 0) : (i % 5 == 0);
            const float r0 = R + (hero ? 6.0f : 3.5f), r1 = r0 + (major ? (hero ? 7.0f : 4.5f) : (hero ? 3.5f : 2.0f));
            g.setColour ((major ? p.textMute : p.edgeHi).withMultipliedAlpha (k.enabled ? 1.0f : 0.5f));
            g.drawLine (c.x + r0 * std::sin (a), c.y - r0 * std::cos (a), c.x + r1 * std::sin (a), c.y - r1 * std::cos (a), hero ? 1.3f : 1.1f);
        }
    }

    // track
    Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0, startA, endA, true);
    g.setColour (Colour (0xff23272a));
    g.strokePath (track, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc
    const float from = k.bipolar ? (startA + endA) * 0.5f : startA;
    if (std::abs (ang - from) > 0.01f)
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, arcR, arcR, 0, juce::jmin (from, ang), juce::jmax (from, ang), true);
        glowStroke (g, arc, acc, ringW, hero ? 12.0f : small ? 5.0f : 7.0f);
    }
    else if (k.bipolar)
    {
        // centred bipolar value: small notch so the zero position is still visible
        Path notch;
        notch.addCentredArc (c.x, c.y, arcR, arcR, 0, from - 0.035f, from + 0.035f, true);
        g.setColour (acc);
        g.strokePath (notch, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // body
    const float bodyR = arcR - ringW * 0.5f - (hero ? 9.0f : small ? 3.5f : 5.0f);
    const auto body = Rectangle<float> (bodyR * 2, bodyR * 2).withCentre (c);
    {
        // soft contact shadow (radial gradient, no blur)
        juce::ColourGradient sh (juce::Colours::black.withAlpha (0.55f), c.x, c.y + bodyR * 0.18f,
                                 juce::Colours::transparentBlack, c.x + bodyR * 1.35f, c.y + bodyR * 0.18f, true);
        g.setGradientFill (sh);
        g.fillEllipse (body.expanded (bodyR * 0.35f).translated (0, bodyR * 0.12f));
    }
    {
        juce::ColourGradient grad (Colour (0xff42474b), c.x, body.getY(), Colour (0xff0b0c0d), c.x, body.getBottom(), false);
        grad.addColour (0.45, Colour (0xff222528));
        g.setGradientFill (grad);
        g.fillEllipse (body);
    }
    const auto face = body.reduced (bodyR * 0.10f);
    {
        juce::ColourGradient grad (Colour (0xff272a2d), face.getX(), face.getY(), Colour (0xff131517), face.getRight(), face.getBottom(), false);
        g.setGradientFill (grad);
        g.fillEllipse (face);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawEllipse (face, 1.0f);
    }
    g.setColour (juce::Colours::white.withAlpha (k.hover || k.active ? 0.26f : 0.12f));
    g.drawEllipse (body.reduced (0.5f), 1.0f);

    // pointer: luminous line (all sizes); smaller knobs get a bright white line for contrast
    const float pr0 = bodyR * (hero ? 0.46f : small ? 0.30f : 0.38f), pr1 = bodyR * (small ? 0.80f : 0.82f);
    Path ptr;
    ptr.startNewSubPath (c.x + pr0 * std::sin (ang), c.y - pr0 * std::cos (ang));
    ptr.lineTo (c.x + pr1 * std::sin (ang), c.y - pr1 * std::cos (ang));
    if (small)
    {
        g.setColour (k.enabled ? juce::Colours::white : p.metal);
        g.strokePath (ptr, juce::PathStrokeType (juce::jmax (2.0f, bodyR * 0.13f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
        glowStroke (g, ptr, k.enabled ? acc : p.metal, hero ? 3.4f : 2.6f, hero ? 8.0f : 5.0f);
}

/** Room reserved around a knob face for the outer scale ticks. */
inline float knobTickRoom (KnobSize s) { return s == KnobSize::Hero ? 30.0f : s == KnobSize::Primary ? 12.0f : 0.0f; }
inline float knobMaxFace (KnobSize s) { return s == KnobSize::Hero ? 150.0f : s == KnobSize::Primary ? 92.0f : s == KnobSize::Secondary ? 52.0f : 30.0f; }
inline float knobCaptionH() { return 17.0f; }
inline float knobValueH (KnobSize s) { return s == KnobSize::Hero ? 26.0f : 22.0f; }

/** Face rectangle of a full knob (caption + face + value) laid out in r. */
inline Rectangle<float> knobFaceRect (Rectangle<float> r, KnobSize size)
{
    r.removeFromTop (knobCaptionH());
    r.removeFromBottom (knobValueH (size) + (size == KnobSize::Hero ? 8.0f : 5.0f));
    const float s = juce::jmax (8.0f, juce::jmin (knobMaxFace (size), juce::jmin (r.getWidth(), r.getHeight()) - knobTickRoom (size)));
    return Rectangle<float> (s, s).withCentre (r.getCentre());
}

inline Rectangle<float> knobValueRect (Rectangle<float> r, KnobSize size)
{
    auto v = r.removeFromBottom (knobValueH (size));
    const float vw = juce::jmin (v.getWidth(), size == KnobSize::Hero ? 120.0f : 88.0f);
    return v.withSizeKeepingCentre (vw, v.getHeight());
}

/** Knob with caption above and value field below. Returns the face rect. */
inline Rectangle<float> knob (Graphics& g, Rectangle<float> r, const KnobSpec& k, const Palette& p)
{
    caption (g, r.withHeight (knobCaptionH()), k.label, p, juce::Justification::centred, k.enabled ? Colour() : p.textMute);
    const auto faceR = knobFaceRect (r, k.size);
    knobFace (g, faceR, k, p);
    const auto val = knobValueRect (r, k.size);
    if (k.size == KnobSize::Hero)
    {
        g.setColour (k.enabled ? p.text : p.textMute);
        g.setFont (font (20.0f, Fonts::SemiBold));
        g.drawText (k.valueText, val, juce::Justification::centred, false);
    }
    else
        valueField (g, val, k.valueText, p, k.active, k.enabled);
    return faceR;
}

// ============================================================================ buttons
inline void pillToggle (Graphics& g, Rectangle<float> r, const juce::String& t, bool on, const Palette& p, bool led = true,
                        bool hover = false, Colour onColour = {})
{
    const auto acc = onColour.isTransparent() ? p.accent : onColour;
    const float rad = r.getHeight() * 0.5f;
    g.setColour (on ? acc.withAlpha (0.18f) : (hover ? p.panelHi : p.well));
    g.fillRoundedRectangle (r, rad);
    g.setColour (on ? acc.withAlpha (0.85f) : (hover ? p.edgeHi : p.edge));
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
    auto inner = r.reduced (rad * 0.55f, 0);
    if (led)
    {
        const auto ledR = Rectangle<float> (7, 7).withCentre ({ inner.getX() + 4.0f, r.getCentreY() });
        if (on) { g.setColour (acc.withAlpha (0.25f)); g.fillEllipse (ledR.expanded (3.0f)); }
        g.setColour (on ? acc : p.textMute.withAlpha (0.7f));
        g.fillEllipse (ledR);
        inner.removeFromLeft (12.0f);
    }
    g.setColour (on ? p.text : p.textDim);
    g.setFont (font (11.0f, Fonts::SemiBold, 0.07f));
    g.drawText (t.toUpperCase(), inner, juce::Justification::centred, false);
}

inline void segmentCell (Graphics& g, Rectangle<float> cell, const juce::String& t, bool sel, bool hover, const Palette& p, float fontH, Colour acc = {})
{
    cell = cell.reduced (2.0f);
    if (sel)
    {
        g.setColour (p.panelHi.brighter (0.10f));
        g.fillRoundedRectangle (cell, 4.5f);
        g.setColour ((acc.isTransparent() ? p.accent : acc).withAlpha (0.9f));
        g.drawRoundedRectangle (cell.reduced (0.5f), 4.5f, 1.0f);
    }
    else if (hover)
    {
        g.setColour (p.panelHi.withAlpha (0.6f));
        g.fillRoundedRectangle (cell, 4.5f);
    }
    g.setColour (sel ? p.text : hover ? p.text.withAlpha (0.85f) : p.textDim);
    g.setFont (font (fontH, Fonts::SemiBold, 0.04f));
    g.drawText (t, cell, juce::Justification::centred, false);
}

inline void segmented (Graphics& g, Rectangle<float> r, const juce::StringArray& items, int sel, const Palette& p, float fontH = 11.0f, int hover = -1)
{
    g.setColour (p.well);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (p.edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    const float w = r.getWidth() / (float) juce::jmax (1, items.size());
    for (int i = 0; i < items.size(); ++i)
        segmentCell (g, Rectangle<float> (r.getX() + i * w, r.getY(), w, r.getHeight()), items[i], i == sel, i == hover, p, fontH);
}

inline void chevron (Graphics& g, juce::Point<float> c, Colour col, float s = 3.5f)
{
    Path chev;
    chev.startNewSubPath (c.x - s, c.y - s * 0.5f); chev.lineTo (c.x, c.y + s * 0.5f); chev.lineTo (c.x + s, c.y - s * 0.5f);
    g.setColour (col);
    g.strokePath (chev, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

inline void selectorBox (Graphics& g, Rectangle<float> r, const juce::String& value, const Palette& p, bool hover = false, bool enabled = true)
{
    g.setColour (hover ? p.panelHi : p.well);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (hover ? p.edgeHi : p.edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (enabled ? p.text : p.textMute);
    g.setFont (font (type::body, Fonts::Medium));
    g.drawText (value, r.reduced (10, 0).withTrimmedRight (14), juce::Justification::centredLeft, true);
    chevron (g, { r.getRight() - 14.0f, r.getCentreY() }, enabled ? p.textDim : p.textMute);
}

inline void selector (Graphics& g, Rectangle<float> r, const juce::String& label, const juce::String& value, const Palette& p)
{
    caption (g, r.removeFromTop (16.0f), label, p, juce::Justification::centredLeft);
    selectorBox (g, r.removeFromTop (26.0f), value, p);
}

enum class Icon { Undo, Redo, Prev, Next, Save, Revert, Power, Copy, Menu, Resize, Learn };

inline void icon (Graphics& g, Rectangle<float> r, Icon ic, Colour c, float stroke = 1.6f)
{
    Path pth;
    const auto ctr = r.getCentre();
    const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    auto arrowHead = [&] (juce::Point<float> tip, float dirAngle)
    {
        const float a = 0.55f, len = s * 0.42f;
        pth.startNewSubPath (tip.x - len * std::cos (dirAngle - a), tip.y - len * std::sin (dirAngle - a));
        pth.lineTo (tip);
        pth.lineTo (tip.x - len * std::cos (dirAngle + a), tip.y - len * std::sin (dirAngle + a));
    };
    bool fillDot = false;
    switch (ic)
    {
        case Icon::Undo: case Icon::Redo: case Icon::Revert:
        {
            const bool undo = ic != Icon::Redo;
            const float rr = s * 0.62f;
            const float a0 = undo ? -2.2f : 2.2f, a1 = undo ? 1.6f : -1.6f;
            pth.addCentredArc (ctr.x, ctr.y + s * 0.08f, rr, rr, 0, a0, a1, true);
            const auto tip = juce::Point<float> (ctr.x + rr * std::sin (a0), ctr.y + s * 0.08f - rr * std::cos (a0));
            arrowHead (tip, undo ? 2.9f : 0.25f);
            fillDot = ic == Icon::Revert;
            break;
        }
        case Icon::Prev: case Icon::Next:
        {
            const float d = ic == Icon::Prev ? 1.0f : -1.0f;
            pth.startNewSubPath (ctr.x + d * s * 0.22f, ctr.y - s * 0.42f);
            pth.lineTo (ctr.x - d * s * 0.22f, ctr.y);
            pth.lineTo (ctr.x + d * s * 0.22f, ctr.y + s * 0.42f);
            break;
        }
        case Icon::Save:
            pth.addRoundedRectangle (ctr.x - s * 0.6f, ctr.y - s * 0.6f, s * 1.2f, s * 1.2f, 2.0f);
            pth.addRectangle (ctr.x - s * 0.32f, ctr.y - s * 0.6f, s * 0.64f, s * 0.36f);
            pth.addRectangle (ctr.x - s * 0.36f, ctr.y + s * 0.12f, s * 0.72f, s * 0.48f);
            break;
        case Icon::Power:
            pth.addCentredArc (ctr.x, ctr.y + s * 0.06f, s * 0.58f, s * 0.58f, 0, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
            pth.startNewSubPath (ctr.x, ctr.y - s * 0.7f);
            pth.lineTo (ctr.x, ctr.y - s * 0.05f);
            break;
        case Icon::Copy:
            pth.addRoundedRectangle (ctr.x - s * 0.55f, ctr.y - s * 0.35f, s * 0.8f, s * 0.9f, 2.0f);
            pth.startNewSubPath (ctr.x - s * 0.25f, ctr.y - s * 0.6f);
            pth.lineTo (ctr.x + s * 0.55f, ctr.y - s * 0.6f);
            pth.lineTo (ctr.x + s * 0.55f, ctr.y + s * 0.3f);
            break;
        case Icon::Menu:
            for (int i = -1; i <= 1; ++i) { pth.startNewSubPath (ctr.x - s * 0.55f, ctr.y + i * s * 0.4f); pth.lineTo (ctr.x + s * 0.55f, ctr.y + i * s * 0.4f); }
            break;
        case Icon::Resize:
            pth.startNewSubPath (ctr.x + s * 0.6f, ctr.y - s * 0.2f); pth.lineTo (ctr.x - s * 0.2f, ctr.y + s * 0.6f);
            pth.startNewSubPath (ctr.x + s * 0.6f, ctr.y + s * 0.25f); pth.lineTo (ctr.x + s * 0.25f, ctr.y + s * 0.6f);
            break;
        case Icon::Learn:
            pth.addEllipse (ctr.x - s * 0.55f, ctr.y - s * 0.55f, s * 1.1f, s * 1.1f);
            pth.addEllipse (ctr.x - s * 0.18f, ctr.y - s * 0.18f, s * 0.36f, s * 0.36f);
            break;
    }
    g.setColour (c);
    g.strokePath (pth, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (fillDot) g.fillEllipse (Rectangle<float> (s * 0.3f, s * 0.3f).withCentre ({ ctr.x, ctr.y + s * 0.08f }));
}

inline void iconButton (Graphics& g, Rectangle<float> r, Icon ic, const Palette& p, bool enabled = true, bool hover = false)
{
    g.setColour (hover && enabled ? p.panelHi : p.well);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (hover && enabled ? p.edgeHi : p.edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
    icon (g, r.reduced (r.getHeight() * 0.28f), ic, enabled ? p.text : p.textMute);
}

// ============================================================================ meters
inline void meterBar (Graphics& g, Rectangle<float> r, float db, const Palette& p, bool vertical, float minDb = -48.0f, float holdDb = -200.0f)
{
    g.setColour (p.well);
    g.fillRoundedRectangle (r, 2.0f);
    const float f = juce::jlimit (0.0f, 1.0f, (db - minDb) / -minDb);
    auto fill = vertical ? r.withTop (r.getBottom() - r.getHeight() * f) : r.withWidth (r.getWidth() * f);
    juce::ColourGradient grad = vertical
        ? juce::ColourGradient (p.danger, r.getX(), r.getY(), p.accent.withSaturation (0.5f).darker (0.2f), r.getX(), r.getBottom(), false)
        : juce::ColourGradient (p.accent.withSaturation (0.5f).darker (0.2f), r.getX(), r.getY(), p.danger, r.getRight(), r.getY(), false);
    grad.addColour (0.12, p.warn);
    grad.addColour (0.3, p.accent);
    g.setGradientFill (grad);
    if (f > 0.0f) g.fillRoundedRectangle (fill, 2.0f);
    if (holdDb > minDb)
    {
        const float hf = juce::jlimit (0.0f, 1.0f, (holdDb - minDb) / -minDb);
        g.setColour (p.text);
        if (vertical) g.fillRect (r.getX(), r.getBottom() - r.getHeight() * hf - 1.0f, r.getWidth(), 1.5f);
        else g.fillRect (r.getX() + r.getWidth() * hf - 1.0f, r.getY(), 1.5f, r.getHeight());
    }
}

// ============================================================================ brand
/** The BANKMASTER mark: a limiter transfer curve rising into a luminous ceiling
    over a filled "signal" area — input → output, mastered. */
inline Path markCurve (Rectangle<float> r)
{
    Path c;
    const float x0 = r.getX(), y0 = r.getBottom(), w = r.getWidth(), h = r.getHeight();
    c.startNewSubPath (x0, y0);
    c.lineTo (x0 + w * 0.42f, y0 - h * 0.58f);
    c.quadraticTo (x0 + w * 0.56f, y0 - h * 0.78f, x0 + w * 0.74f, y0 - h * 0.80f);
    c.lineTo (x0 + w, y0 - h * 0.80f);
    return c;
}

inline void logoMark (Graphics& g, Rectangle<float> r, const Palette& p, bool tile = false)
{
    const float s = juce::jmin (r.getWidth(), r.getHeight());
    r = Rectangle<float> (s, s).withCentre (r.getCentre());
    const bool small = s <= 40.0f;   // favicon / tray sizes: simpler, bolder drawing
    if (tile)
    {
        const auto t = r.reduced (s * (small ? 0.03f : 0.08f));
        const float rad = t.getWidth() * 0.225f;
        if (! small)
        {
            Path tilePath;
            tilePath.addRoundedRectangle (t, rad);
            juce::DropShadow (juce::Colours::black.withAlpha (0.6f), (int) (s * 0.05f), { 0, (int) (s * 0.02f) }).drawForPath (g, tilePath);
        }
        // machined rim: light top-left -> dark bottom-right
        g.setGradientFill (juce::ColourGradient (Colour (0xff5a6066), t.getX(), t.getY(), Colour (0xff050606), t.getRight(), t.getBottom(), false));
        g.fillRoundedRectangle (t, rad);
        const auto in = t.reduced (juce::jmax (1.0f, s * 0.012f));
        const float rin = rad - s * 0.012f;
        {
            juce::ColourGradient grad (Colour (0xff25292c), in.getX(), in.getY(), Colour (0xff060707), in.getRight(), in.getBottom(), false);
            grad.addColour (0.55, Colour (0xff111315));
            g.setGradientFill (grad);
            g.fillRoundedRectangle (in, rin);
        }
        if (! small)
        {
            // soft accent bloom behind the knee + glass highlight on the upper half
            const auto knee = juce::Point<float> (in.getX() + in.getWidth() * 0.6f, in.getY() + in.getHeight() * 0.36f);
            g.setGradientFill (juce::ColourGradient (p.accent.withAlpha (0.16f), knee.x, knee.y, p.accent.withAlpha (0.0f), knee.x + in.getWidth() * 0.55f, knee.y, true));
            g.fillRoundedRectangle (in, rin);
            Path gloss;
            gloss.addRoundedRectangle (in.getX(), in.getY(), in.getWidth(), in.getHeight() * 0.48f, rin, rin, true, true, false, false);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.055f), in.getX(), in.getY(), juce::Colours::white.withAlpha (0.0f), in.getX(), in.getY() + in.getHeight() * 0.48f, false));
            g.fillPath (gloss);
        }
        r = in.reduced (in.getWidth() * (small ? 0.13f : 0.17f));
    }
    // fine analyser grid (major + minor)
    if (tile && ! small)
    {
        for (int i = 1; i < 8; ++i)
        {
            g.setColour (juce::Colours::white.withAlpha (i % 2 == 0 ? 0.07f : 0.03f));
            g.drawLine (r.getX() + r.getWidth() * i / 8, r.getY(), r.getX() + r.getWidth() * i / 8, r.getBottom(), juce::jmax (0.6f, s * 0.0025f));
            g.drawLine (r.getX(), r.getY() + r.getHeight() * i / 8, r.getRight(), r.getY() + r.getHeight() * i / 8, juce::jmax (0.6f, s * 0.0025f));
        }
    }
    auto curveBox = r.reduced (r.getWidth() * 0.04f);
    Path c = markCurve (curveBox);
    // filled area under the curve
    Path area (c);
    area.lineTo (curveBox.getRight(), curveBox.getBottom());
    area.closeSubPath();
    g.setGradientFill (juce::ColourGradient (p.accent.withAlpha (small ? 0.25f : 0.34f), curveBox.getX(), curveBox.getY() + curveBox.getHeight() * 0.2f,
                                             p.accent.withAlpha (0.0f), curveBox.getX(), curveBox.getBottom(), false));
    g.fillPath (area);
    // ceiling: fine double rule with an end tick
    const float cy = curveBox.getBottom() - curveBox.getHeight() * 0.80f;
    const float lw = juce::jmax (1.0f, s * 0.006f);
    if (! small)
    {
        g.setColour (p.accent.withAlpha (0.38f));
        g.drawLine (curveBox.getX(), cy - s * 0.016f, curveBox.getRight(), cy - s * 0.016f, lw);
        g.setColour (p.accent.withAlpha (0.18f));
        g.drawLine (curveBox.getX(), cy - s * 0.032f, curveBox.getRight(), cy - s * 0.032f, lw * 0.8f);
        g.setColour (p.accent.withAlpha (0.7f));
        g.drawLine (curveBox.getRight(), cy - s * 0.05f, curveBox.getRight(), cy + s * 0.012f, lw * 1.4f);
    }
    // curve: deep green at the start rising to a near-white neon at the ceiling
    const float cw = small ? juce::jmax (1.5f, s * 0.085f) : juce::jmax (1.6f, s * 0.055f);
    if (! small)
    {
        const float passes[3][2] = { { 0.11f, 0.05f }, { 0.07f, 0.10f }, { 0.04f, 0.18f } };
        for (auto& ps : passes)
        {
            g.setColour (p.accent.withAlpha (ps[1]));
            g.strokePath (c, juce::PathStrokeType (cw + s * ps[0], juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
    {
        juce::ColourGradient cg (p.accent.darker (0.45f), curveBox.getX(), curveBox.getBottom(), p.accent.interpolatedWith (juce::Colours::white, 0.35f), curveBox.getRight(), cy, false);
        cg.addColour (0.45, p.accent);
        g.setGradientFill (cg);
        g.strokePath (c, juce::PathStrokeType (cw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    if (small && s < 24.0f) return;   // knee dot only where it reads
    // knee point: white core with an accent halo ring
    const auto knee = juce::Point<float> (curveBox.getX() + curveBox.getWidth() * 0.56f, curveBox.getBottom() - curveBox.getHeight() * 0.755f);
    const float kr = juce::jmax (1.6f, s * 0.042f);
    if (! small)
    {
        g.setColour (p.accent.withAlpha (0.35f));
        g.drawEllipse (Rectangle<float> (kr * 3.4f, kr * 3.4f).withCentre (knee), juce::jmax (1.0f, s * 0.008f));
    }
    g.setColour (juce::Colours::white);
    g.fillEllipse (Rectangle<float> (kr * 2, kr * 2).withCentre (knee));
}

/** Wordmark: "BANK" white + "MASTER" accent, "BY KALOX" (always uppercase) below. */
inline void wordmark (Graphics& g, Rectangle<float> r, const Palette& p, float height, bool withByline = true)
{
    auto f = font (height, Fonts::Display, 0.06f);
    g.setFont (f);
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, "BANK", 0, 0);
    const float wBank = ga.getBoundingBox (0, -1, true).getWidth() + height * 0.06f;
    auto line = r.removeFromTop (height * 1.05f);
    g.setColour (p.text);
    g.drawText ("BANK", line, juce::Justification::centredLeft, false);
    g.setColour (p.accent);
    g.drawText ("MASTER", line.withTrimmedLeft (wBank), juce::Justification::centredLeft, false);
    if (withByline)
    {
        const float bh = juce::jmax (9.0f, height * 0.30f);
        g.setColour (p.textDim);
        g.setFont (font (bh, Fonts::SemiBold, height < 30.0f ? 0.32f : 0.42f));
        g.drawText ("BY KALOX", r.removeFromTop (juce::jmax (12.0f, height * 0.48f)).translated (height * 0.03f, 0), juce::Justification::centredLeft, false);
    }
}

} // namespace bmv2
