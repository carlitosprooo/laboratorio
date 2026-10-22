// BANKMASTER — BY KALOX · V2 frame: header, navigation, global bar, popovers
#include "Chrome.h"

namespace bmui
{
namespace
{
    void txt (juce::Graphics& g, Rectangle<float> r, const String& t, float h, Fonts::Weight w, juce::Colour c,
               juce::Justification j = juce::Justification::centredLeft, float kern = 0.0f)
    {
        g.setColour (c);
        g.setFont (font (h, w, kern));
        g.drawText (t, r, j, false);
    }
    void vline (juce::Graphics& g, float x, float y0, float y1, juce::Colour c) { g.setColour (c); g.fillRect (x, y0, 1.0f, y1 - y0); }
    float toDb (float g) { return juce::Decibels::gainToDecibels (g, -100.0f); }
    String num (float v, int dec) { return v < -99.0f || ! std::isfinite (v) ? String ("-inf") : trimNegativeZero (String (v, dec)); }
    String utf8 (const char* s) { return String (juce::CharPointer_UTF8 (s)); }
}

// ============================================================================ undo / redo
UndoRedoControl::UndoRedoControl()
{
    addAndMakeVisible (undoButton);
    addAndMakeVisible (redoButton);
    undoButton.setTooltip (utf8 ("Undo (\xe2\x8c\x98Z / Ctrl+Z)"));
    redoButton.setTooltip (utf8 ("Redo (\xe2\x87\xa7\xe2\x8c\x98Z / Ctrl+Y)"));
}

void UndoRedoControl::paint (juce::Graphics& g)
{
    const auto& p = P();
    auto r = getLocalBounds().toFloat();
    g.setColour (p.well);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (p.edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    vline (g, r.getCentreX(), r.getY() + 8, r.getBottom() - 8, p.edgeHi);
}

void UndoRedoControl::resized()
{
    auto r = getLocalBounds();
    undoButton.setBounds (r.removeFromLeft (r.getWidth() / 2));
    redoButton.setBounds (r);
}

void UndoRedoControl::setState (bool canUndo, bool canRedo)
{
    if (undoButton.isEnabled() != canUndo) undoButton.setEnabled (canUndo);
    if (redoButton.isEnabled() != canRedo) redoButton.setEnabled (canRedo);
}

void UndoRedoControl::Half::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto& p = P();
    auto r = getLocalBounds().toFloat();
    if (over && isEnabled())
    {
        g.setColour (p.panelHi.withAlpha (down ? 1.0f : 0.7f));
        g.fillRoundedRectangle (r.reduced (2), 6.0f);
    }
    const auto c = isEnabled() ? p.text : p.textMute;
    if (undo)
    {
        icon (g, r.removeFromLeft (30).reduced (7.5f, 9.0f).translated (5, 0), Icon::Undo, c, 1.7f);
        txt (g, r.withTrimmedLeft (6), "UNDO", 11.0f, Fonts::SemiBold, c, juce::Justification::centredLeft, 0.1f);
    }
    else
    {
        icon (g, r.removeFromRight (30).reduced (7.5f, 9.0f).translated (-5, 0), Icon::Redo, c, 1.7f);
        txt (g, r.withTrimmedRight (6), "REDO", 11.0f, Fonts::SemiBold, c, juce::Justification::centredRight, 0.1f);
    }
}

// ============================================================================ preset name
void PresetName::paintButton (juce::Graphics& g, bool over, bool)
{
    const auto& p = P();
    auto r = getLocalBounds().toFloat();
    if (over) { g.setColour (p.panelHi.withAlpha (0.6f)); g.fillRect (r); }
    auto nm = r.reduced (10, 3);
    const String top = modified ? kind + utf8 ("  \xc2\xb7  MODIFIED") : kind;
    txt (g, nm.removeFromTop (15).translated (0, 2), top, 9.5f, Fonts::SemiBold, modified ? p.warn : p.textMute, juce::Justification::centred, 0.14f);
    g.setColour (p.text);
    g.setFont (font (13.5f, Fonts::SemiBold, 0.02f));
    g.drawFittedText (name.replace (" - ", utf8 (" \xe2\x80\x94 ")), nm.toNearestInt(), juce::Justification::centred, 1, 0.8f);
}

// ============================================================================ header
Header::Header (BankMasterProcessor& p) : proc (p)
{
    for (auto* c : std::initializer_list<juce::Component*> { &prev, &next, &save, &revert, &copy, &menu, &presetName, &ab, &undoRedo, &oneOn, &oneAmt })
        addAndMakeVisible (c);
    oneOn.bind (p.getState().getParameter (bmp::id::oneOn), &p.getUndoManager());
    oneAmt.bind (p.getState().getParameter (bmp::id::oneAmt), &p.getUndoManager());
    oneAmt.setFaceOnly (true);
    prev.setTooltip ("Previous preset");
    next.setTooltip ("Next preset");
    save.setTooltip (utf8 ("Save as user preset\xe2\x80\xa6"));
    revert.setTooltip ("Revert to the loaded preset (asks for confirmation, can be undone)");
    copy.setTooltip ("Copy the current A/B slot to the other one");
    menu.setTooltip ("Window size, presets folder, about");
    presetName.setTooltip ("Browse presets");
    ab.setTooltip ("A/B compare");
    oneOn.setTooltip ("ONE MASTER on / off");
    oneAmt.setTooltip ("ONE MASTER intensity (100 % = settings as dialled)");
}

void Header::paint (juce::Graphics& g)
{
    const auto& p = P();
    auto r = getLocalBounds().toFloat();
    g.setColour (p.edge);
    g.fillRect (r.getX(), r.getBottom() - 1.0f, r.getWidth(), 1.0f);
    auto row = r.reduced (16, 11);
    logoMark (g, row.removeFromLeft (row.getHeight()), p, true);
    row.removeFromLeft (10);
    wordmark (g, row.removeFromLeft (200).withTrimmedTop (4), p, 19.0f, true);

    // preset frame
    auto pr = presetName.getBounds().toFloat().getUnion (prev.getBounds().toFloat()).getUnion (next.getBounds().toFloat());
    g.setColour (p.well); g.fillRoundedRectangle (pr, 9.0f);
    g.setColour (p.edge); g.drawRoundedRectangle (pr.reduced (0.5f), 9.0f, 1.0f);
    vline (g, (float) prev.getRight(), pr.getY() + 8, pr.getBottom() - 8, p.edge);
    vline (g, (float) next.getX(), pr.getY() + 8, pr.getBottom() - 8, p.edge);

    // ONE MASTER frame + labels
    const auto one = oneArea.toFloat();
    const bool on = oneOn.getToggleState();
    g.setColour (p.well); g.fillRoundedRectangle (one, 9.0f);
    g.setColour (on ? p.accent.withAlpha (0.6f) : p.edge); g.drawRoundedRectangle (one.reduced (0.5f), 9.0f, 1.0f);
    auto lab = one.withTrimmedLeft (34).withTrimmedRight (44);
    txt (g, lab.removeFromTop (22).translated (0, 3), "ONE MASTER", 10.5f, Fonts::SemiBold, on ? p.text : p.textDim, juce::Justification::centredLeft, 0.1f);
    txt (g, lab.translated (0, -3), oneAmt.getParameter() != nullptr ? paramText (*oneAmt.getParameter()) : String(), 11.5f, Fonts::Medium, on ? p.accent : p.textMute);
}

void Header::resized()
{
    auto row = getLocalBounds().reduced (16, 11);
    row.removeFromLeft (row.getHeight() + 10 + 200);
    menu.setBounds (row.removeFromRight (36).withSizeKeepingCentre (36, 36));
    row.removeFromRight (14);
    oneArea = row.removeFromRight (150).withSizeKeepingCentre (150, 42);
    {
        auto o = oneArea;
        oneOn.setBounds (o.removeFromLeft (30).withSizeKeepingCentre (24, 24).translated (4, 0));
        oneAmt.setBounds (o.removeFromRight (42).withSizeKeepingCentre (34, 34).translated (-4, 0));
    }
    row.removeFromRight (12);
    undoRedo.setBounds (row.removeFromRight (168).withSizeKeepingCentre (168, 34));
    row.removeFromRight (12);
    {
        auto abr = row.removeFromRight (118).withSizeKeepingCentre (118, 34);
        copy.setBounds (abr.removeFromRight (34));
        abr.removeFromRight (6);
        ab.setBounds (abr);
    }
    auto pr = row.withSizeKeepingCentre (juce::jmin (470, row.getWidth() - 20), 40);
    auto icons = pr.removeFromRight (78);
    save.setBounds (icons.removeFromLeft (36).withSizeKeepingCentre (36, 36));
    icons.removeFromLeft (6);
    revert.setBounds (icons.removeFromLeft (36).withSizeKeepingCentre (36, 36));
    pr.removeFromRight (8);
    prev.setBounds (pr.removeFromLeft (34));
    next.setBounds (pr.removeFromRight (34));
    presetName.setBounds (pr);
}

void Header::tick()
{
    auto& pm = proc.getPresets();
    const auto name = pm.getCurrentPresetName();
    const bool modified = pm.isModified();
    const String kind = pm.getAllPresetNames().indexOf (name) < pm.getNumFactoryPresets() ? "FACTORY" : "USER";
    if (name != presetName.name || modified != presetName.modified || kind != presetName.kind)
    {
        presetName.name = name;
        presetName.modified = modified;
        presetName.kind = kind;
        presetName.repaint();
    }
    revert.setEnabled (modified);
    ab.setSelected (proc.getAbSlot());
    undoRedo.setState (proc.getUndoManager().canUndo(), proc.getUndoManager().canRedo());
    oneAmt.setEnabled (oneOn.getToggleState());
    repaint (oneArea);
}

// ============================================================================ navigation
ModuleNode::ModuleNode (BankMasterProcessor& p, int m) : juce::Button ({}), proc (p), module (m)
{
    const char* names[4] = { "EQ", "SATURATION", "COMPRESSOR", "LIMITER" };
    setName (names[m]);
    const juce::String ids[4] = { bmp::id::eqOn, bmp::id::satOn, bmp::id::compOn, bmp::id::limOn };
    power.bind (p.getState().getParameter (ids[m]), &p.getUndoManager());
    power.setOnColour (P().module[m]);
    power.setTooltip (String ("Enable / bypass ") + names[m]);
    addAndMakeVisible (power);
    setTooltip (String ("Open ") + names[m]);
}

void ModuleNode::resized()
{
    power.setBounds (getLocalBounds().reduced (10, 6).removeFromLeft (26).withSizeKeepingCentre (24, 24));
}

void ModuleNode::paintButton (juce::Graphics& g, bool over, bool)
{
    const auto& p = P();
    const auto mc = p.module[module];
    auto n = getLocalBounds().toFloat();
    const auto all = bm::applyOneMaster (proc.getCurrentParams());
    const bool on = all.moduleOn[module];
    if (active)
    {
        g.setColour (mc.withAlpha (0.10f));
        g.fillRoundedRectangle (n.expanded (0, 0), 9.0f);
    }
    g.setColour (active ? p.panelHi : over ? p.panelHi.withAlpha (0.8f) : p.panel);
    g.fillRoundedRectangle (n.reduced (1), 8.0f);
    g.setColour (active ? mc.withAlpha (0.85f) : over ? p.edgeHi : p.edge);
    g.drawRoundedRectangle (n.reduced (1.5f), 8.0f, active ? 1.3f : 1.0f);
    auto in = n.reduced (10, 6);
    in.removeFromLeft (34);
    // mini visual
    auto viz = in.removeFromRight (juce::jmin (64.0f, in.getWidth() * 0.32f)).reduced (0, 4);
    g.setColour (p.well); g.fillRoundedRectangle (viz, 4.0f);
    auto vz = viz.reduced (5, 4);
    const auto vc = on ? mc : p.metal;
    if (module == 0 || module == 1)
    {
        Path c;
        for (int i = 0; i <= 40; ++i)
        {
            float y;
            if (module == 0)
            {
                const double f = 20.0 * std::pow (1000.0, i / 40.0);
                double d = 0;
                for (const auto& b : all.eq.bands)
                    if (b.enabled) d += bm::bandMagnitudeDb (b, f, proc.getCurrentSampleRate());
                y = vz.getCentreY() - (float) juce::jlimit (-6.0, 6.0, d) / 6.0f * vz.getHeight() * 0.5f;
            }
            else
            {
                const float u = -1.0f + 2.0f * i / 40.0f;
                const float drv = std::pow (10.0f, bm::Shaper::driveDb (all.sat.drivePct) / 20.0f) * 4.0f;
                y = vz.getCentreY() - bm::Shaper::curve (all.sat.style, u * drv) / bm::Shaper::curve (all.sat.style, drv) * vz.getHeight() * 0.45f;
            }
            const auto pt = juce::Point<float> (vz.getX() + vz.getWidth() * i / 40.0f, y);
            if (i == 0) c.startNewSubPath (pt); else c.lineTo (pt);
        }
        g.setColour (vc); g.strokePath (c, juce::PathStrokeType (1.3f));
    }
    else
    {
        const float gr = juce::jmax (0.0f, module == 2 ? proc.getChain().getCompGainReduction() : proc.getChain().getLimGainReduction());
        auto bar = vz.withSizeKeepingCentre (vz.getWidth(), 6.0f);
        g.setColour (p.panelHi); g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (vc); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.05f, 1.0f, gr / 12.0f)), 3.0f);
    }
    in.removeFromRight (6);
    auto t1 = in.removeFromTop (in.getHeight() * 0.52f);
    txt (g, t1, "0" + String (module + 1), 9.5f, Fonts::SemiBold, active ? mc : p.textMute, juce::Justification::bottomLeft, 0.1f);
    txt (g, t1.withTrimmedLeft (19), getName(), 11.5f, Fonts::Bold, active ? p.text : (on ? p.textDim : p.textMute), juce::Justification::bottomLeft, 0.1f);
    String sub;
    if (! on) sub = "BYPASSED";
    else if (module == 0)
    {
        int n = 0, d = 0;
        for (const auto& b : all.eq.bands) { n += b.enabled ? 1 : 0; d += (b.enabled && b.dynOn && bm::bandSupportsDynamics (b.type)) ? 1 : 0; }
        sub = String (n) + " BANDS" + (d > 0 ? utf8 (" \xc2\xb7 ") + String (d) + " DYN" : String());
    }
    else if (module == 1) sub = bmp::satStyleNames()[(int) all.sat.style].toUpperCase() + utf8 (" \xc2\xb7 ") + String (all.sat.drivePct, 0) + " %";
    else
    {
        const float gr = juce::jmax (0.0f, module == 2 ? proc.getChain().getCompGainReduction() : proc.getChain().getLimGainReduction());
        sub = "GR " + num (-gr, 1) + " dB";
    }
    txt (g, in, sub, 10.0f, Fonts::Medium, ! on ? p.warn.withAlpha (0.8f) : active ? p.textDim : p.textMute, juce::Justification::topLeft, 0.04f);
}

NavStrip::NavStrip (BankMasterProcessor& p) : proc (p)
{
    for (int m = 0; m < 4; ++m)
    {
        nodes[m] = std::make_unique<ModuleNode> (p, m);
        nodes[m]->onClick = [this, m] { if (onSelect) onSelect (m); };
        addAndMakeVisible (*nodes[m]);
    }
    inGain.bind (p.getState().getParameter (bmp::id::inGain), &p.getUndoManager());
    outGain.bind (p.getState().getParameter (bmp::id::outGain), &p.getUndoManager());
    inGain.onPaint = [this] (juce::Graphics& g, ParamDragValue& v) { paintIoNode (g, v, "INPUT", true); };
    outGain.onPaint = [this] (juce::Graphics& g, ParamDragValue& v) { paintIoNode (g, v, "OUTPUT", false); };
    inGain.setTooltip ("INPUT gain: drag up / down, Shift = fine, double-click = 0 dB. Set automatically by INPUT LEARN.");
    outGain.setTooltip ("OUTPUT trim: drag up / down, Shift = fine, double-click = 0 dB");
    addAndMakeVisible (inGain);
    addAndMakeVisible (outGain);
}

void NavStrip::paintIoNode (juce::Graphics& g, ParamDragValue& v, const String& title, bool input)
{
    const auto& p = P();
    auto n = v.getLocalBounds().toFloat();
    const bool over = v.isMouseOverOrDragging();
    g.setColour (over ? p.panelHi : p.panel); g.fillRoundedRectangle (n.reduced (1), 8.0f);
    g.setColour (over ? p.edgeHi : p.edge); g.drawRoundedRectangle (n.reduced (1.5f), 8.0f, 1.0f);
    auto in = n.reduced (12, 6);
    txt (g, in.removeFromTop (in.getHeight() * 0.5f), title, 10.0f, Fonts::SemiBold, p.textDim, juce::Justification::bottomLeft, 0.14f);
    txt (g, in, v.text(), 12.5f, Fonts::Medium, p.text, juce::Justification::topLeft);
    if (input && proc.isLearningInput())
    {
        auto l = n.reduced (8).removeFromRight (44).withSizeKeepingCentre (44, 18);
        g.setColour (p.warn.withAlpha (0.2f)); g.fillRoundedRectangle (l, 9.0f);
        txt (g, l, "LEARN", 9.0f, Fonts::Bold, p.warn, juce::Justification::centred, 0.08f);
    }
    // drag affordance
    auto ar = n.reduced (8).removeFromRight (10).withSizeKeepingCentre (10, 16);
    if (! (input && proc.isLearningInput()))
    {
        chevron (g, { ar.getCentreX(), ar.getY() + 3.0f }, over ? p.textDim : p.textMute.withAlpha (0.6f), 3.0f);
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::pi, ar.getCentreX(), ar.getBottom() - 3.0f));
        chevron (g, { ar.getCentreX(), ar.getBottom() - 3.0f }, over ? p.textDim : p.textMute.withAlpha (0.6f), 3.0f);
        g.restoreState();
    }
}

void NavStrip::paint (juce::Graphics& g)
{
    const auto& p = P();
    auto row = getLocalBounds().toFloat().reduced (16, 9);
    g.setColour (p.edgeHi);
    g.fillRect (row.getX() + 20, row.getCentreY(), row.getWidth() - 40, 1.0f);
    for (auto a : arrows)
    {
        // gap in the flow line + chevron
        g.setColour (P().bg0);
        g.fillRect (a.toFloat().withSizeKeepingCentre ((float) a.getWidth(), 3.0f));
        Path ch;
        const float x = (float) a.getCentreX(), y = (float) a.getCentreY();
        ch.startNewSubPath (x - 2.5f, y - 4.0f); ch.lineTo (x + 2.5f, y); ch.lineTo (x - 2.5f, y + 4.0f);
        g.setColour (p.textMute);
        g.strokePath (ch, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void NavStrip::resized()
{
    auto row = getLocalBounds().reduced (16, 9);
    const int io = 118, arrow = 20;
    const int modW = (row.getWidth() - 2 * io - 5 * arrow) / 4;
    arrows.clear();
    inGain.setBounds (row.removeFromLeft (io));
    arrows.add (row.removeFromLeft (arrow));
    for (int m = 0; m < 4; ++m)
    {
        nodes[m]->setBounds (row.removeFromLeft (modW));
        arrows.add (row.removeFromLeft (arrow));
    }
    outGain.setBounds (row.removeFromLeft (io));
}

void NavStrip::setActive (int module)
{
    for (int m = 0; m < 4; ++m) { nodes[m]->active = m == module; nodes[m]->repaint(); }
}

void NavStrip::tick()
{
    for (auto& n : nodes) n->repaint();
    inGain.repaint();
}

// ============================================================================ global bar
void GlobalBar::LearnButton::paintButton (juce::Graphics& g, bool over, bool)
{
    const auto& p = P();
    auto r = getLocalBounds().toFloat();
    const float rad = r.getHeight() * 0.5f;
    g.setColour (learning ? p.warn.withAlpha (0.16f) : over ? p.panelHi : p.well);
    g.fillRoundedRectangle (r, rad);
    g.setColour (learning ? p.warn.withAlpha (0.8f) : over ? p.edgeHi : p.edge);
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
    auto li = r.reduced (11, 0);
    icon (g, li.removeFromLeft (16).withSizeKeepingCentre (16, 16), Icon::Learn, learning ? p.warn : p.accent, 1.5f);
    li.removeFromLeft (8);
    txt (g, li.removeFromTop (r.getHeight() * 0.52f), learning ? "LEARNING" : "LEARN", 10.5f, Fonts::SemiBold, learning ? p.warn : p.text, juce::Justification::bottomLeft, 0.1f);
    if (status.isNotEmpty())
        txt (g, li, status, 9.5f, Fonts::Medium, p.textDim, juce::Justification::topLeft, 0.03f);
}

GlobalBar::GlobalBar (BankMasterProcessor& p) : proc (p)
{
    addAndMakeVisible (learn);
    addAndMakeVisible (target);
    addAndMakeVisible (bypass);
    addAndMakeVisible (resetLoudness);
    bypass.bind (p.getState().getParameter (bmp::id::bypass), &p.getUndoManager());
    bypass.setOnColour (P().warn);
    bypass.setTooltip ("Master bypass (latency-compensated, click-free)");
    target.bind (p.getState().getParameter (bmp::id::inTarget), &p.getUndoManager());
    target.setTooltip ("INPUT LEARN target loudness: drag up / down, double-click = -20 LUFS");
    target.onPaint = [this] (juce::Graphics& g, ParamDragValue& v)
    {
        const auto& pal = P();
        auto r = v.getLocalBounds().toFloat();
        if (v.isMouseOverOrDragging()) { g.setColour (pal.panelHi); g.fillRoundedRectangle (r, 4.0f); }
        txt (g, r, v.text(), 9.5f, Fonts::Medium, v.isMouseOverOrDragging() ? pal.text : pal.textDim, juce::Justification::centredLeft, 0.03f);
    };
    learn.setTooltip ("INPUT LEARN: play the loudest section, click, wait a few seconds, click again. "
                      "INPUT gain is set so the mix reaches the target loudness before the chain.");
    learn.onClick = [this]
    {
        if (! proc.isLearningInput()) { proc.startInputLearn(); learn.status.clear(); }
        else
        {
            const float g = proc.stopInputLearn (true);
            learn.status = std::isfinite (g) ? "INPUT " + (g > 0 ? String ("+") : String()) + num (g, 1) + " dB" : String ("NO SIGNAL");
            statusUntil = juce::Time::getMillisecondCounter() + 4000;
        }
    };
    resetLoudness.setAlpha (0.0f);
    resetLoudness.setTooltip ("Click to reset loudness and true peak");
    resetLoudness.onClick = [this]
    {
        proc.getChain().getLoudness().requestReset();
        proc.getChain().getOutputMeter().resetTruePeak();
    };
}

void GlobalBar::tick()
{
    auto& c = proc.getChain();
    inL.set (toDb (c.getInputMeter().getPeak (0))); inR.set (toDb (c.getInputMeter().getPeak (1)));
    outL.set (toDb (c.getOutputMeter().getPeak (0))); outR.set (toDb (c.getOutputMeter().getPeak (1)));
    lufsM = c.getLoudness().getMomentary(); lufsS = c.getLoudness().getShortTerm(); lufsI = c.getLoudness().getIntegrated();
    tp = toDb (c.getOutputMeter().getTruePeakMax());
    grComp = juce::jmax (0.0f, c.getCompGainReduction()); grLim = juce::jmax (0.0f, c.getLimGainReduction());
    corr = c.getOutputMeter().getCorrelation();
    learn.learning = proc.isLearningInput();
    if (learn.learning)
    {
        const float m = proc.getLearnMeasuredLufs();
        learn.status = std::isfinite (m) && m > -99.0f ? num (m, 1) + " LUFS" : utf8 ("\xe2\x80\xa6");
    }
    else if (statusUntil != 0 && juce::Time::getMillisecondCounter() > statusUntil)
    {
        learn.status.clear();
        statusUntil = 0;
    }
    target.setVisible (! learn.learning && learn.status.isEmpty());
    learn.repaint();
    repaint();
}

void GlobalBar::resized()
{
    auto row = getLocalBounds().toFloat().reduced (16, 10);
    dividers.clear();
    auto take = [&] (float w) { auto s = row.removeFromLeft (w); return s; };
    auto divider = [&] { row.removeFromLeft (11); dividers.add (row.getX()); row.removeFromLeft (11); };
    secIn = take (150); divider();
    secLoud = take (168); divider();
    secTp = take (100); divider();
    secGr = take (128); divider();
    secCorr = take (108); divider();
    secOut = take (150);
    auto rr = row.withTrimmedLeft (12);
    bypass.setBounds (rr.removeFromRight (92).withSizeKeepingCentre (92, 34).toNearestInt());
    rr.removeFromRight (8);
    auto lb = rr.withSizeKeepingCentre (rr.getWidth(), 34).toNearestInt();
    learn.setBounds (lb);
    target.setBounds (lb.withTrimmedLeft (11 + 16 + 8).withTrimmedTop (lb.getHeight() / 2 + 1).withTrimmedRight (10).withHeight (14));
    resetLoudness.setBounds (secLoud.getUnion (secTp).toNearestInt());
}

void GlobalBar::paint (juce::Graphics& g)
{
    const auto& p = P();
    for (auto x : dividers) vline (g, x, (float) getHeight() * 0.5f - 18, (float) getHeight() * 0.5f + 18, p.edge);

    auto lr = [&] (Rectangle<float> a, const String& t, float l, float r)
    {
        caption (g, a.removeFromTop (14), t, p, juce::Justification::centredLeft);
        auto vv = a.removeFromRight (52);
        txt (g, vv, num (juce::jmax (l, r), 1), 12.5f, Fonts::Medium, juce::jmax (l, r) > -0.1f ? p.danger : p.text, juce::Justification::centredRight);
        a.removeFromRight (8);
        auto m1 = a.withHeight (5).withY (a.getCentreY() - 6), m2 = m1.translated (0, 8);
        meterBar (g, m1, l, p, false, -48.0f, l + 0.5f);
        meterBar (g, m2, r, p, false, -48.0f, r + 0.5f);
    };
    lr (secIn, "INPUT", inL.v, inR.v);
    {
        auto a = secLoud;
        caption (g, a.removeFromTop (14), "LOUDNESS  LUFS", p, juce::Justification::centredLeft);
        const float cw = a.getWidth() / 3.0f;
        const char* labels[3] = { "M", "S", "I" };
        const float vals[3] = { lufsM, lufsS, lufsI };
        for (int i = 0; i < 3; ++i)
        {
            auto c = a.removeFromLeft (cw);
            txt (g, c.removeFromLeft (13), labels[i], 10.5f, Fonts::SemiBold, p.textMute);
            txt (g, c, num (vals[i], 1), i == 2 ? 16.0f : 13.0f, i == 2 ? Fonts::SemiBold : Fonts::Medium, i == 2 ? p.accent : p.text);
        }
    }
    {
        auto a = secTp;
        caption (g, a.removeFromTop (14), "TRUE PEAK", p, juce::Justification::centredLeft);
        const float ceil = proc.getCurrentParams().lim.ceilingDb;
        txt (g, a, num (tp, 2) + " dBTP", 13.0f, Fonts::Medium, tp > ceil + 0.05f ? p.danger : p.text);
    }
    {
        auto a = secGr;
        caption (g, a.removeFromTop (14), "GAIN REDUCTION", p, juce::Justification::centredLeft);
        auto l1 = a.removeFromTop (a.getHeight() * 0.5f), l2 = a;
        auto grRow = [&] (Rectangle<float> rr, const String& t, float v, juce::Colour c)
        {
            txt (g, rr.removeFromLeft (34), t, 9.5f, Fonts::SemiBold, p.textMute, juce::Justification::centredLeft, 0.08f);
            txt (g, rr.removeFromRight (32), num (-v, 1), 11.5f, Fonts::Medium, p.text, juce::Justification::centredRight);
            auto b = rr.reduced (4, 0).withSizeKeepingCentre (rr.getWidth() - 8, 4);
            g.setColour (p.well); g.fillRoundedRectangle (b, 2);
            g.setColour (c); g.fillRoundedRectangle (b.withWidth (b.getWidth() * juce::jlimit (0.03f, 1.0f, v / 12.0f)), 2);
        };
        grRow (l1, "COMP", grComp, p.module[2]);
        grRow (l2, "LIM", grLim, p.module[3]);
    }
    {
        auto a = secCorr;
        caption (g, a.removeFromTop (14), "CORRELATION", p, juce::Justification::centredLeft);
        auto v = a.removeFromRight (34);
        txt (g, v, trimNegativeZero (String (corr, 2)), 12.5f, Fonts::Medium, p.text, juce::Justification::centredRight);
        auto b = a.withSizeKeepingCentre (a.getWidth() - 6, 6);
        g.setColour (p.well); g.fillRoundedRectangle (b, 3);
        vline (g, b.getCentreX(), b.getY() - 2, b.getBottom() + 2, p.edgeHi);
        const float x = b.getX() + b.getWidth() * (juce::jlimit (-1.0f, 1.0f, corr) + 1.0f) * 0.5f;
        g.setColour (corr < 0 ? p.danger : p.accent);
        g.fillRect (Rectangle<float>::leftTopRightBottom (juce::jmin (x, b.getCentreX()), b.getY(), juce::jmax (x, b.getCentreX()), b.getBottom()));
        g.fillEllipse (Rectangle<float> (8, 8).withCentre ({ x, b.getCentreY() }));
    }
    lr (secOut, "OUTPUT", outL.v, outR.v);
}

// ============================================================================ popovers
Popover::Popover()
{
    setWantsKeyboardFocus (true);
    setAlwaysOnTop (true);
}

void Popover::paint (juce::Graphics& g)
{
    const auto& p = P();
    auto b = body().toFloat();
    juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 24, { 0, 8 }).drawForRectangle (g, b.reduced (6).toNearestInt());
    g.setColour (p.panelHi); g.fillRoundedRectangle (b, 12);
    g.setColour (p.edgeHi); g.drawRoundedRectangle (b.reduced (0.5f), 12, 1);
    Path tip;
    tip.addTriangle (arrowX - 8, b.getY() + 1, arrowX + 8, b.getY() + 1, arrowX, 0);
    g.setColour (p.panelHi); g.fillPath (tip);
}

bool Popover::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { if (onCancel) onCancel(); return true; }
    return false;
}

void Popover::showUnder (Rectangle<int> anchor, int width, int height)
{
    auto parent = getParentComponent() != nullptr ? getParentComponent()->getLocalBounds() : Rectangle<int> (0, 0, 1200, 780);
    auto r = Rectangle<int> (width, height + kArrow).withPosition (anchor.getCentreX() - width + 60, anchor.getBottom() + 4);
    r = r.constrainedWithin (parent.reduced (8));
    setBounds (r);
    arrowX = (float) (anchor.getCentreX() - r.getX());
    setVisible (true);
    toFront (true);
    grabKeyboardFocus();
}

RevertPopover::RevertPopover()
{
    addAndMakeVisible (cancel);
    addAndMakeVisible (confirm);
}

void RevertPopover::paint (juce::Graphics& g)
{
    Popover::paint (g);
    const auto& p = P();
    auto in = body().toFloat().reduced (18, 14);
    auto t = in.removeFromTop (20);
    icon (g, t.removeFromLeft (18).withSizeKeepingCentre (16, 16), Icon::Revert, p.warn, 1.6f);
    txt (g, t.withTrimmedLeft (8), "Revert to loaded preset?", 14.0f, Fonts::SemiBold, p.text);
    in.removeFromTop (6);
    g.setColour (p.textDim);
    g.setFont (font (12.0f));
    g.drawFittedText (utf8 ("All changes since \"") + presetName.replace (" - ", utf8 (" \xe2\x80\x94 ")) + "\" was loaded will be discarded. You can still undo this.",
                      in.removeFromTop (50).toNearestInt(), juce::Justification::topLeft, 3, 1.0f);
}

void RevertPopover::resized()
{
    auto b = body().reduced (18, 14);
    auto btns = b.removeFromBottom (32);
    confirm.setBounds (btns.removeFromRight (110));
    btns.removeFromRight (10);
    cancel.setBounds (btns.removeFromRight (100));
}

SavePopover::SavePopover()
{
    addAndMakeVisible (nameEditor);
    addAndMakeVisible (cancel);
    addAndMakeVisible (confirm);
    nameEditor.setFont (font (13.0f));
    nameEditor.setIndents (8, 6);
}

void SavePopover::paint (juce::Graphics& g)
{
    Popover::paint (g);
    const auto& p = P();
    auto in = body().toFloat().reduced (18, 14);
    auto t = in.removeFromTop (20);
    icon (g, t.removeFromLeft (18).withSizeKeepingCentre (16, 16), Icon::Save, p.accent, 1.5f);
    txt (g, t.withTrimmedLeft (8), "Save user preset", 14.0f, Fonts::SemiBold, p.text);
    if (message.isNotEmpty())
    {
        g.setColour (p.danger);
        g.setFont (font (11.5f));
        g.drawFittedText (message, in.withTrimmedTop (44).removeFromTop (32).toNearestInt(), juce::Justification::topLeft, 2, 1.0f);
    }
}

void SavePopover::resized()
{
    auto b = body().reduced (18, 14);
    b.removeFromTop (28);
    nameEditor.setBounds (b.removeFromTop (30));
    auto btns = b.removeFromBottom (32);
    confirm.setBounds (btns.removeFromRight (100));
    btns.removeFromRight (10);
    cancel.setBounds (btns.removeFromRight (100));
}

} // namespace bmui
