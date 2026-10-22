// BANKMASTER V2 — design system (proposal, NOT linked into the plugin yet).
// Pure JUCE drawing code: the same primitives render the design previews and,
// after approval, the plugin UI.
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
    juce::String name, tagline;
    Colour bg0, bg1;                 // window gradient
    Colour panel, panelHi, well;     // raised panel / highlight / recessed well (value fields)
    Colour edge, edgeHi;             // hairlines
    Colour text, textDim, textMute;
    Colour accent, accentSoft, accentInk; // accent / translucent accent / text on accent
    Colour metal;                    // metallic grey (secondary arcs, inactive)
    Colour warn, danger, ok;
    Colour disp0, disp1, grid, gridStrong;
    Colour specPre, specPost, curve;
    Colour bands[5];
    Colour module[4];                // EQ, SAT, COMP, LIM tint (small markers only)
};

inline Palette conceptA()
{
    Palette p;
    p.name = "A  OBSIDIAN / NEON";
    p.tagline = "obsidian black, graphite, metallic grey, controlled lime";
    p.bg0 = Colour (0xff121416); p.bg1 = Colour (0xff0a0b0c);
    p.panel = Colour (0xff17191b); p.panelHi = Colour (0xff1f2225); p.well = Colour (0xff0d0f10);
    p.edge = Colour (0xff25292c); p.edgeHi = Colour (0xff353a3e);
    p.text = Colour (0xffeef1f2); p.textDim = Colour (0xff9aa3a8); p.textMute = Colour (0xff5d666b);
    p.accent = Colour (0xffa6f23a); p.accentSoft = Colour (0x33a6f23a); p.accentInk = Colour (0xff0d1205);
    p.metal = Colour (0xff6f787d);
    p.warn = Colour (0xffffc247); p.danger = Colour (0xffff5a5a); p.ok = Colour (0xffa6f23a);
    p.disp0 = Colour (0xff101214); p.disp1 = Colour (0xff0b0c0d);
    p.grid = Colour (0x14ffffff); p.gridStrong = Colour (0x26ffffff);
    p.specPre = Colour (0xff3b4145); p.specPost = Colour (0xffa6f23a); p.curve = Colour (0xfff4f7f8);
    const juce::uint32 b[] = { 0xffff6b6b, 0xffffb347, 0xffa6f23a, 0xff3ad6c8, 0xff59b8ff };
    for (int i = 0; i < 5; ++i) p.bands[i] = Colour (b[i]);
    const juce::uint32 m[] = { 0xffa6f23a, 0xffffb347, 0xff3ad6c8, 0xffff6b6b };
    for (int i = 0; i < 4; ++i) p.module[i] = Colour (m[i]);
    return p;
}

inline Palette conceptB()
{
    Palette p;
    p.name = "B  GRAPHITE / VIOLET";
    p.tagline = "black, lead grey, violet light, subtle depth";
    p.bg0 = Colour (0xff141419); p.bg1 = Colour (0xff0b0b0e);
    p.panel = Colour (0xff19191f); p.panelHi = Colour (0xff212129); p.well = Colour (0xff0e0e12);
    p.edge = Colour (0xff292932); p.edgeHi = Colour (0xff3a3a46);
    p.text = Colour (0xfff1f0f6); p.textDim = Colour (0xffa19fb0); p.textMute = Colour (0xff625f72);
    p.accent = Colour (0xffa47cff); p.accentSoft = Colour (0x33a47cff); p.accentInk = Colour (0xff0f0a1c);
    p.metal = Colour (0xff76738a);
    p.warn = Colour (0xffffc06a); p.danger = Colour (0xffff5f7a); p.ok = Colour (0xff6fe3b0);
    p.disp0 = Colour (0xff111116); p.disp1 = Colour (0xff0b0b0f);
    p.grid = Colour (0x14ffffff); p.gridStrong = Colour (0x26ffffff);
    p.specPre = Colour (0xff3a3946); p.specPost = Colour (0xffa47cff); p.curve = Colour (0xfff6f4ff);
    const juce::uint32 b[] = { 0xffff6f91, 0xffffb86b, 0xffc6a8ff, 0xff6ee7d8, 0xff6fb6ff };
    for (int i = 0; i < 5; ++i) p.bands[i] = Colour (b[i]);
    const juce::uint32 m[] = { 0xffa47cff, 0xffffb86b, 0xff6ee7d8, 0xffff6f91 };
    for (int i = 0; i < 4; ++i) p.module[i] = Colour (m[i]);
    return p;
}

// ============================================================================ type
struct Fonts
{
    juce::Typeface::Ptr regular, medium, semibold, bold, display, displaySemi;

    static Fonts& get() { static Fonts f; return f; }

    void load (const juce::File& dir)
    {
        auto tf = [&] (const char* name) -> juce::Typeface::Ptr
        {
            juce::MemoryBlock mb;
            if (! dir.getChildFile (name).loadFileAsData (mb)) return nullptr;
            return juce::Typeface::createSystemTypefaceFor (mb.getData(), mb.getSize());
        };
        regular = tf ("Inter-Regular.otf"); medium = tf ("Inter-Medium.otf");
        semibold = tf ("Inter-SemiBold.otf"); bold = tf ("Inter-Bold.otf");
        display = tf ("Montserrat-ExtraBold.ttf"); displaySemi = tf ("Montserrat-SemiBold.ttf");
    }

    enum Weight { Regular, Medium, SemiBold, Bold, Display, DisplaySemi };

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

// ============================================================================ helpers
inline void glowStroke (Graphics& g, const Path& p, Colour c, float width, float glowRadius)
{
    Path stroked;
    juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (stroked, p);
    juce::DropShadow (c.withMultipliedAlpha (0.55f), (int) glowRadius, {}).drawForPath (g, stroked);
    g.setColour (c);
    g.fillPath (stroked);
}

inline void panel (Graphics& g, Rectangle<float> r, const Palette& p, float radius = 10.0f, bool raised = true)
{
    if (raised)
    {
        Path sh; sh.addRoundedRectangle (r, radius);
        juce::DropShadow (juce::Colours::black.withAlpha (0.45f), 18, { 0, 6 }).drawForPath (g, sh);
    }
    g.setGradientFill (juce::ColourGradient (p.panelHi, r.getX(), r.getY(), p.panel, r.getX(), r.getY() + juce::jmin (120.0f, r.getHeight()), false));
    g.fillRoundedRectangle (r, radius);
    g.setColour (p.edge);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    // top inner highlight
    g.setColour (juce::Colours::white.withAlpha (0.04f));
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
    g.setFont (font (10.5f, Fonts::SemiBold, 0.12f));
    g.drawText (t.toUpperCase(), r, j);
}

inline void valueField (Graphics& g, Rectangle<float> r, const juce::String& t, const Palette& p, bool active = false)
{
    g.setColour (p.well);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (active ? p.accent.withAlpha (0.6f) : p.edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    g.setColour (p.text);
    g.setFont (font (12.5f, Fonts::Medium));
    g.drawText (t, r, juce::Justification::centred);
}

// ============================================================================ knobs
enum class KnobSize { Hero, Primary, Secondary, Mini };

struct KnobSpec
{
    float value = 0.5f;          // 0..1
    bool bipolar = false;
    juce::String label, valueText;
    KnobSize size = KnobSize::Primary;
    bool hover = false, enabled = true;
    Colour accent;               // transparent = palette accent
};

/** Knob face only (no label), drawn in the square r. */
inline void knobFace (Graphics& g, Rectangle<float> r, const KnobSpec& k, const Palette& p)
{
    const float startA = juce::MathConstants<float>::pi * 1.25f;
    const float endA = juce::MathConstants<float>::pi * 2.75f;
    const float size = juce::jmin (r.getWidth(), r.getHeight());
    const auto c = r.getCentre();
    const float R = size * 0.5f;
    const bool hero = k.size == KnobSize::Hero;
    const float ringW = hero ? juce::jmax (4.0f, size * 0.035f) : juce::jmax (2.5f, size * 0.05f);
    const float arcR = R - ringW * 0.5f - 1.0f;
    const auto acc = (k.accent.isTransparent() ? p.accent : k.accent).withMultipliedAlpha (k.enabled ? 1.0f : 0.35f);
    const float ang = startA + k.value * (endA - startA);

    // scale ticks (hero / primary)
    if (k.size == KnobSize::Hero || k.size == KnobSize::Primary)
    {
        const int n = hero ? 41 : 11;
        for (int i = 0; i < n; ++i)
        {
            const float a = startA + (endA - startA) * i / (n - 1);
            const bool major = hero ? (i % 10 == 0) : (i % 5 == 0);
            const float r0 = R + (hero ? 6.0f : 3.0f), r1 = r0 + (major ? (hero ? 7.0f : 4.0f) : (hero ? 3.5f : 2.0f));
            g.setColour ((major ? p.textMute : p.edgeHi).withMultipliedAlpha (k.enabled ? 1.0f : 0.5f));
            g.drawLine (c.x + r0 * std::sin (a), c.y - r0 * std::cos (a), c.x + r1 * std::sin (a), c.y - r1 * std::cos (a), hero ? 1.3f : 1.0f);
        }
    }

    // track
    Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0, startA, endA, true);
    g.setColour (p.well.brighter (0.15f));
    g.strokePath (track, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc
    const float from = k.bipolar ? (startA + endA) * 0.5f : startA;
    if (std::abs (ang - from) > 0.01f)
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, arcR, arcR, 0, juce::jmin (from, ang), juce::jmax (from, ang), true);
        glowStroke (g, arc, acc, ringW, hero ? 14.0f : 8.0f);
    }

    // body
    const float bodyR = arcR - ringW * 0.5f - (hero ? 9.0f : 5.0f);
    const auto body = Rectangle<float> (bodyR * 2, bodyR * 2).withCentre (c);
    {
        Path sh; sh.addEllipse (body);
        juce::DropShadow (juce::Colours::black.withAlpha (0.8f), (int) (bodyR * 0.35f), { 0, (int) (bodyR * 0.12f) }).drawForPath (g, sh);
    }
    {
        juce::ColourGradient grad (Colour (0xff3a3e42), c.x, body.getY(), Colour (0xff0c0d0e), c.x, body.getBottom(), false);
        grad.addColour (0.45, Colour (0xff202326));
        g.setGradientFill (grad);
        g.fillEllipse (body);
    }
    // brushed / machined face
    const auto face = body.reduced (bodyR * 0.10f);
    {
        juce::ColourGradient grad (Colour (0xff24272a), face.getX(), face.getY(), Colour (0xff141618), face.getRight(), face.getBottom(), false);
        g.setGradientFill (grad);
        g.fillEllipse (face);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawEllipse (face, 1.0f);
    }
    g.setColour (juce::Colours::white.withAlpha (k.hover ? 0.22f : 0.10f));
    g.drawEllipse (body.reduced (0.5f), 1.0f);

    // pointer: luminous line (hero / primary) or dot (secondary / mini)
    const float pr0 = bodyR * (hero ? 0.48f : 0.40f), pr1 = bodyR * 0.82f;
    if (k.size == KnobSize::Secondary || k.size == KnobSize::Mini)
    {
        const float pr = bodyR * 0.66f, dot = juce::jmax (2.2f, bodyR * 0.16f);
        const auto pt = juce::Point<float> (c.x + pr * std::sin (ang), c.y - pr * std::cos (ang));
        g.setColour (k.enabled ? juce::Colours::white : p.metal);
        g.fillEllipse (Rectangle<float> (dot * 2, dot * 2).withCentre (pt));
    }
    else
    {
        Path ptr;
        ptr.startNewSubPath (c.x + pr0 * std::sin (ang), c.y - pr0 * std::cos (ang));
        ptr.lineTo (c.x + pr1 * std::sin (ang), c.y - pr1 * std::cos (ang));
        glowStroke (g, ptr, k.enabled ? acc : p.metal, hero ? 3.4f : 2.4f, hero ? 10.0f : 6.0f);
    }
}

/** Knob with caption above and value field below. Returns the face rect. */
inline Rectangle<float> knob (Graphics& g, Rectangle<float> r, const KnobSpec& k, const Palette& p)
{
    const float capH = 16.0f, valH = k.size == KnobSize::Hero ? 26.0f : 22.0f;
    caption (g, r.removeFromTop (capH), k.label, p, juce::Justification::centred, k.enabled ? Colour() : p.textMute);
    auto val = r.removeFromBottom (valH);
    r.removeFromBottom (k.size == KnobSize::Hero ? 8.0f : 5.0f);
    // the scale ticks of hero / primary knobs sit outside the face: keep room for them
    const float tickRoom = k.size == KnobSize::Hero ? 30.0f : k.size == KnobSize::Primary ? 12.0f : 0.0f;
    const float maxS = k.size == KnobSize::Hero ? 150.0f : k.size == KnobSize::Primary ? 92.0f : k.size == KnobSize::Secondary ? 50.0f : 28.0f;
    const float s = juce::jmin (maxS, juce::jmin (r.getWidth(), r.getHeight()) - tickRoom);
    const auto faceR = Rectangle<float> (s, s).withCentre (r.getCentre());
    knobFace (g, faceR, k, p);
    const float vw = juce::jmin (val.getWidth(), k.size == KnobSize::Hero ? 110.0f : 84.0f);
    if (k.size == KnobSize::Hero)
    {
        g.setColour (k.enabled ? p.text : p.textMute);
        g.setFont (font (20.0f, Fonts::SemiBold));
        g.drawText (k.valueText, val, juce::Justification::centred);
    }
    else
        valueField (g, val.withSizeKeepingCentre (vw, val.getHeight()), k.valueText, p);
    return faceR;
}

// ============================================================================ buttons
inline void pillToggle (Graphics& g, Rectangle<float> r, const juce::String& t, bool on, const Palette& p, bool led = true)
{
    const float rad = r.getHeight() * 0.5f;
    g.setColour (on ? p.accentSoft : p.well);
    g.fillRoundedRectangle (r, rad);
    g.setColour (on ? p.accent.withAlpha (0.8f) : p.edge);
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
    auto inner = r.reduced (rad * 0.55f, 0);
    if (led)
    {
        const auto ledR = Rectangle<float> (7, 7).withCentre ({ inner.getX() + 4.0f, r.getCentreY() });
        if (on) { Path sh; sh.addEllipse (ledR); juce::DropShadow (p.accent, 8, {}).drawForPath (g, sh); }
        g.setColour (on ? p.accent : p.textMute.withAlpha (0.6f));
        g.fillEllipse (ledR);
        inner.removeFromLeft (12.0f);
    }
    g.setColour (on ? p.text : p.textDim);
    g.setFont (font (11.0f, Fonts::SemiBold, 0.08f));
    g.drawText (t.toUpperCase(), inner, juce::Justification::centred);
}

inline void segmented (Graphics& g, Rectangle<float> r, const juce::StringArray& items, int sel, const Palette& p, float fontH = 11.0f)
{
    g.setColour (p.well);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (p.edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    const float w = r.getWidth() / (float) items.size();
    for (int i = 0; i < items.size(); ++i)
    {
        auto cell = Rectangle<float> (r.getX() + i * w, r.getY(), w, r.getHeight()).reduced (2.0f);
        if (i == sel)
        {
            g.setColour (p.panelHi.brighter (0.08f));
            g.fillRoundedRectangle (cell, 4.5f);
            g.setColour (p.accent.withAlpha (0.85f));
            g.drawRoundedRectangle (cell.reduced (0.5f), 4.5f, 1.0f);
        }
        g.setColour (i == sel ? p.text : p.textDim);
        g.setFont (font (fontH, Fonts::SemiBold, 0.05f));
        g.drawText (items[i], cell, juce::Justification::centred);
    }
}

inline void selector (Graphics& g, Rectangle<float> r, const juce::String& label, const juce::String& value, const Palette& p)
{
    caption (g, r.removeFromTop (16.0f), label, p, juce::Justification::centredLeft);
    r = r.removeFromTop (26.0f);
    g.setColour (p.well);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (p.edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (p.text);
    g.setFont (font (12.0f, Fonts::Medium));
    g.drawText (value, r.reduced (10, 0), juce::Justification::centredLeft);
    Path chev;
    const float x = r.getRight() - 14.0f, y = r.getCentreY();
    chev.startNewSubPath (x - 3.5f, y - 1.8f); chev.lineTo (x, y + 1.8f); chev.lineTo (x + 3.5f, y - 1.8f);
    g.setColour (p.textDim);
    g.strokePath (chev, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
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
            if (ic == Icon::Revert) pth.addEllipse (ctr.x - 1.2f, ctr.y + s * 0.08f - 1.2f, 2.4f, 2.4f);
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
        {
            pth.addRoundedRectangle (ctr.x - s * 0.6f, ctr.y - s * 0.6f, s * 1.2f, s * 1.2f, 2.0f);
            pth.addRectangle (ctr.x - s * 0.32f, ctr.y - s * 0.6f, s * 0.64f, s * 0.36f);
            pth.addRectangle (ctr.x - s * 0.36f, ctr.y + s * 0.12f, s * 0.72f, s * 0.48f);
            break;
        }
        case Icon::Power:
        {
            pth.addCentredArc (ctr.x, ctr.y + s * 0.06f, s * 0.58f, s * 0.58f, 0, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
            pth.startNewSubPath (ctr.x, ctr.y - s * 0.7f);
            pth.lineTo (ctr.x, ctr.y - s * 0.05f);
            break;
        }
        case Icon::Copy:
        {
            pth.addRoundedRectangle (ctr.x - s * 0.55f, ctr.y - s * 0.35f, s * 0.8f, s * 0.9f, 2.0f);
            pth.startNewSubPath (ctr.x - s * 0.25f, ctr.y - s * 0.6f);
            pth.lineTo (ctr.x + s * 0.55f, ctr.y - s * 0.6f);
            pth.lineTo (ctr.x + s * 0.55f, ctr.y + s * 0.3f);
            break;
        }
        case Icon::Menu:
            for (int i = -1; i <= 1; ++i) { pth.startNewSubPath (ctr.x - s * 0.55f, ctr.y + i * s * 0.4f); pth.lineTo (ctr.x + s * 0.55f, ctr.y + i * s * 0.4f); }
            break;
        case Icon::Resize:
            pth.startNewSubPath (ctr.x + s * 0.6f, ctr.y - s * 0.2f); pth.lineTo (ctr.x - s * 0.2f, ctr.y + s * 0.6f);
            pth.startNewSubPath (ctr.x + s * 0.6f, ctr.y + s * 0.25f); pth.lineTo (ctr.x + s * 0.25f, ctr.y + s * 0.6f);
            break;
        case Icon::Learn:
        {
            // target / crosshair
            pth.addEllipse (ctr.x - s * 0.55f, ctr.y - s * 0.55f, s * 1.1f, s * 1.1f);
            pth.addEllipse (ctr.x - s * 0.18f, ctr.y - s * 0.18f, s * 0.36f, s * 0.36f);
            break;
        }
    }
    g.setColour (c);
    g.strokePath (pth, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

inline void iconButton (Graphics& g, Rectangle<float> r, Icon ic, const Palette& p, bool enabled = true, const juce::String& label = {})
{
    g.setColour (p.well);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (p.edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
    auto ir = label.isEmpty() ? r : r.removeFromLeft (r.getHeight());
    icon (g, ir.reduced (r.getHeight() * 0.28f), ic, enabled ? p.text : p.textMute);
    if (label.isNotEmpty())
    {
        g.setColour (enabled ? p.textDim : p.textMute);
        g.setFont (font (10.5f, Fonts::SemiBold, 0.1f));
        g.drawText (label, r.withTrimmedRight (8), juce::Justification::centredLeft);
    }
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
    g.fillRoundedRectangle (fill, 2.0f);
    if (holdDb > minDb)
    {
        const float hf = juce::jlimit (0.0f, 1.0f, (holdDb - minDb) / -minDb);
        g.setColour (p.text);
        if (vertical) g.fillRect (r.getX(), r.getBottom() - r.getHeight() * hf - 1.0f, r.getWidth(), 1.5f);
        else g.fillRect (r.getX() + r.getWidth() * hf - 1.0f, r.getY(), 1.5f, r.getHeight());
    }
}

inline void grBar (Graphics& g, Rectangle<float> r, float grDb, const Palette& p, float maxDb = 12.0f)
{
    g.setColour (p.well);
    g.fillRoundedRectangle (r, 2.0f);
    const float f = juce::jlimit (0.0f, 1.0f, grDb / maxDb);
    g.setColour (p.danger.withAlpha (0.9f));
    g.fillRoundedRectangle (r.withWidth (r.getWidth() * f), 2.0f);
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
        Path tilePath;
        tilePath.addRoundedRectangle (t, rad);
        if (! small)
            juce::DropShadow (juce::Colours::black.withAlpha (0.6f), (int) (s * 0.05f), { 0, (int) (s * 0.02f) }).drawForPath (g, tilePath);
        juce::ColourGradient grad (Colour (0xff26292c), t.getX(), t.getY(), Colour (0xff070808), t.getRight(), t.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (t, rad);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (t.reduced (s * 0.004f), rad, juce::jmax (1.0f, s * 0.006f));
        r = t.reduced (t.getWidth() * (small ? 0.14f : 0.17f));
    }
    // faint analyser grid
    g.setColour (juce::Colours::white.withAlpha (tile && ! small ? 0.06f : 0.0f));
    for (int i = 1; i < 4; ++i)
    {
        g.drawVerticalLine ((int) (r.getX() + r.getWidth() * i / 4), r.getY(), r.getBottom());
        g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * i / 4), r.getX(), r.getRight());
    }
    auto curveBox = r.reduced (r.getWidth() * 0.04f);
    Path c = markCurve (curveBox);
    // filled area under the curve
    Path area (c);
    area.lineTo (curveBox.getRight(), curveBox.getBottom());
    area.closeSubPath();
    g.setGradientFill (juce::ColourGradient (p.accent.withAlpha (0.38f), curveBox.getX(), curveBox.getY() + curveBox.getHeight() * 0.2f,
                                             p.accent.withAlpha (0.0f), curveBox.getX(), curveBox.getBottom(), false));
    g.fillPath (area);
    // ceiling line
    const float cy = curveBox.getBottom() - curveBox.getHeight() * 0.80f;
    g.setColour (p.accent.withAlpha (0.35f));
    g.drawLine (curveBox.getX(), cy, curveBox.getRight(), cy, juce::jmax (1.0f, s * 0.008f));
    glowStroke (g, c, p.accent, small ? juce::jmax (1.5f, s * 0.085f) : juce::jmax (1.6f, s * 0.055f), small ? 2.0f : juce::jmax (4.0f, s * 0.09f));
    // knee point
    const auto knee = juce::Point<float> (curveBox.getX() + curveBox.getWidth() * 0.56f, curveBox.getBottom() - curveBox.getHeight() * 0.755f);
    if (small && s < 24.0f) return;   // knee dot only where it reads
    const float kr = juce::jmax (1.6f, s * 0.045f);
    g.setColour (juce::Colours::white);
    g.fillEllipse (Rectangle<float> (kr * 2, kr * 2).withCentre (knee));
}

/** Wordmark: "BANK" white + "MASTER" accent (family link with KaloxSooth), "BY KALOX" below. */
inline void wordmark (Graphics& g, Rectangle<float> r, const Palette& p, float height, bool withByline = true)
{
    auto f = font (height, Fonts::Display, 0.06f);
    g.setFont (f);
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, "BANK", 0, 0);
    const float wBank = ga.getBoundingBox (0, -1, true).getWidth() + height * 0.06f;
    auto line = r.removeFromTop (height * 1.05f);
    g.setColour (p.text);
    g.drawText ("BANK", line, juce::Justification::centredLeft);
    g.setColour (p.accent);
    g.drawText ("MASTER", line.withTrimmedLeft (wBank), juce::Justification::centredLeft);
    if (withByline)
    {
        g.setColour (p.textDim);
        const float bh = juce::jmax (8.5f, height * 0.30f);
        g.setFont (font (bh, Fonts::SemiBold, height < 30.0f ? 0.30f : 0.42f));
        g.drawText ("BY KALOX", r.removeFromTop (juce::jmax (12.0f, height * 0.48f)).translated (height * 0.03f, 0), juce::Justification::centredLeft);
    }
}

} // namespace bmv2
