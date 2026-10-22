// BANKMASTER by KALOX — parameter layout and ChainParams <-> APVTS mapping
#include "Parameters.h"

namespace bmp
{
using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;
using Range = juce::NormalisableRange<float>;

juce::StringArray bandTypeNames()  { return { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch", "Band Pass" }; }
juce::StringArray slopeNames()     { return { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" }; }
juce::StringArray placementNames() { return { "Stereo", "Mid", "Side", "Left", "Right" }; }
juce::StringArray satStyleNames()  { return { "Clean Tube", "Warm Tube", "Tape", "Soft Clip" }; }
juce::StringArray compStyleNames() { return { "Clean", "Glue", "Punch" }; }
juce::StringArray limStyleNames()  { return { "Transparent", "Punchy", "Dynamic", "Safe" }; }
juce::StringArray osNames()        { return { "Off (1x)", "2x", "4x", "8x", "16x" }; }
juce::StringArray ditherNames()    { return { "Off", "16 bit", "24 bit" }; }

namespace
{
    Range logRange (float lo, float hi, float centre)
    {
        Range r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    juce::String dbText (float v, int) { return juce::String (v, std::abs (v) < 10 ? 2 : 1) + " dB"; }
    juce::String pctText (float v, int) { return juce::String (v, 1) + " %"; }
    juce::String msText (float v, int) { return v < 10 ? juce::String (v, 2) + " ms" : v < 1000 ? juce::String (v, 1) + " ms" : juce::String (v / 1000.0f, 2) + " s"; }
    juce::String hzText (float v, int) { return v < 1000 ? juce::String (v, v < 100 ? 2 : 1) + " Hz" : juce::String (v / 1000.0f, 2) + " kHz"; }
    juce::String ratioText (float v, int) { return juce::String (v, 2) + ":1"; }

    struct Builder
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        void f (const juce::String& pid, const juce::String& name, Range r, float def,
                std::function<juce::String (float, int)> text, const juce::String& label = {})
        {
            auto attrs = juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (text)).withLabel (label);
            params.push_back (std::make_unique<APF> (juce::ParameterID { pid, 1 }, name, r, def, attrs));
        }
        void b (const juce::String& pid, const juce::String& name, bool def)
        {
            params.push_back (std::make_unique<APB> (juce::ParameterID { pid, 1 }, name, def));
        }
        void c (const juce::String& pid, const juce::String& name, const juce::StringArray& items, int def)
        {
            params.push_back (std::make_unique<APC> (juce::ParameterID { pid, 1 }, name, items, def));
        }
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    const auto d = bm::presets::kaloxOriginal(); // defaults = KALOX MASTER - ORIGINAL
    Builder B;

    B.f (id::inGain, "Input Gain", Range (-24, 24, 0.01f), d.inputGainDb, dbText);
    B.f (id::inTarget, "Input Learn Target", Range (-30, -10, 0.5f), -20.0f, [] (float v, int) { return juce::String (v, 1) + " LUFS"; });
    B.f (id::outGain, "Output Gain", Range (-24, 12, 0.01f), d.outputGainDb, dbText);
    B.b (id::bypass, "Master Bypass", false);
    B.b (id::oneOn, "ONE MASTER On", false);
    B.f (id::oneAmt, "ONE MASTER", Range (0, 200, 0.1f), 100, pctText);
    B.b (id::eqOn, "EQ On", true);
    B.b (id::satOn, "Saturation On", true);
    B.b (id::compOn, "Compressor On", true);
    B.b (id::limOn, "Limiter On", true);

    for (int i = 0; i < bm::kNumEqBands; ++i)
    {
        const auto& bd = d.eq.bands[(size_t) i];
        const juce::String n = "EQ Band " + juce::String (i + 1) + " ";
        B.b (id::eq (i, "on"), n + "On", bd.enabled);
        B.c (id::eq (i, "type"), n + "Type", bandTypeNames(), (int) bd.type);
        B.f (id::eq (i, "freq"), n + "Freq", logRange (10, 30000, 1000), bd.freq, hzText);
        B.f (id::eq (i, "gain"), n + "Gain", Range (-24, 24, 0.01f), bd.gainDb, dbText);
        B.f (id::eq (i, "q"), n + "Q", logRange (0.025f, 40, 1), bd.q, [] (float v, int) { return juce::String (v, 3); });
        B.c (id::eq (i, "slope"), n + "Slope", slopeNames(), bd.slopeIdx);
        B.c (id::eq (i, "place"), n + "Placement", placementNames(), (int) bd.placement);
        B.b (id::eq (i, "dyn"), n + "Dynamic", bd.dynOn);
        B.f (id::eq (i, "range"), n + "Dyn Range", Range (-24, 24, 0.01f), bd.dynRangeDb, dbText);
        B.f (id::eq (i, "thr"), n + "Dyn Threshold", Range (-60, 0, 0.1f), bd.dynThresholdDb, dbText);
    }

    B.c (id::satStyle, "Sat Style", satStyleNames(), (int) d.sat.style);
    B.f (id::satDrive, "Sat Drive", Range (0, 100, 0.1f), d.sat.drivePct, pctText);
    B.f (id::satMix, "Sat Mix", Range (0, 100, 0.1f), d.sat.mixPct, pctText);
    B.f (id::satTone, "Sat Tone", Range (-100, 100, 0.1f), d.sat.tone, [] (float v, int) { return juce::String (v, 1); });
    B.f (id::satDyn, "Sat Dynamics", Range (-100, 100, 0.1f), d.sat.dynamics, [] (float v, int) { return juce::String (v, 1); });
    B.f (id::satFb, "Sat Feedback", Range (0, 100, 0.1f), d.sat.feedbackPct, pctText);
    B.f (id::satFbFreq, "Sat Feedback Freq", logRange (20, 20000, 1000), d.sat.feedbackFreq, hzText);
    B.f (id::satOut, "Sat Output", Range (-24, 24, 0.01f), d.sat.outputDb, dbText);
    B.c (id::satOs, "Sat Oversampling", osNames(), d.sat.osFactorLog2);
    B.b (id::satHq, "Sat High Quality", d.sat.quality == 1);

    B.c (id::compStyle, "Comp Style", compStyleNames(), (int) d.comp.style);
    B.f (id::compThr, "Comp Threshold", Range (-60, 0, 0.01f), d.comp.thresholdDb, dbText);
    B.f (id::compRatio, "Comp Ratio", logRange (1, 20, 3), d.comp.ratio, ratioText);
    B.f (id::compKnee, "Comp Knee", Range (0, 72, 0.1f), d.comp.kneeDb, dbText);
    B.f (id::compAtt, "Comp Attack", logRange (0.005f, 250, 10), d.comp.attackMs, msText);
    B.f (id::compRel, "Comp Release", logRange (10, 2500, 200), d.comp.releaseMs, msText);
    B.b (id::compAutoRel, "Comp Auto Release", d.comp.autoRelease);
    B.f (id::compLa, "Comp Lookahead", Range (0, 20, 0.01f), d.comp.lookaheadMs, msText);
    B.f (id::compHold, "Comp Hold", Range (0, 500, 0.1f), d.comp.holdMs, msText);
    B.f (id::compRange, "Comp Range", Range (0, 60, 0.1f), d.comp.rangeDb, dbText);
    B.f (id::compMakeup, "Comp Makeup", Range (-12, 36, 0.01f), d.comp.makeupDb, dbText);
    B.b (id::compAutoGain, "Comp Auto Gain", d.comp.autoGain);
    B.f (id::compMix, "Comp Dry/Wet", Range (0, 100, 0.1f), d.comp.mixPct, pctText);
    B.f (id::compScHp, "Comp SC High-pass", logRange (10, 500, 60), d.comp.scHpFreq, [] (float v, int) { return v <= 10.5f ? juce::String ("Off") : hzText (v, 0); });
    B.f (id::compScLp, "Comp SC Low-pass", logRange (1000, 20000, 6000), std::min (20000.0f, d.comp.scLpFreq), [] (float v, int) { return v >= 19999 ? juce::String ("Off") : hzText (v, 0); });
    B.f (id::compLink, "Comp Stereo Link", Range (0, 100, 0.1f), d.comp.stereoLinkPct, pctText);

    B.c (id::limStyle, "Lim Style", limStyleNames(), (int) d.lim.style);
    B.f (id::limGain, "Lim Gain", Range (-12, 30, 0.01f), d.lim.inputGainDb, dbText);
    B.f (id::limCeil, "Lim Ceiling", Range (-12, 0, 0.01f), d.lim.ceilingDb, [] (float v, int) { return juce::String (v, 2) + " dB"; });
    B.f (id::limAtt, "Lim Attack", logRange (0.1f, 1000, 50), d.lim.attackMs, msText);
    B.f (id::limRel, "Lim Release", logRange (1, 10000, 300), d.lim.releaseMs, msText);
    B.f (id::limLa, "Lim Lookahead", Range (0, 5, 0.001f), d.lim.lookaheadMs, [] (float v, int) { return juce::String (v, 3) + " ms"; });
    B.b (id::limTp, "Lim True Peak", d.lim.truePeak);
    B.c (id::limOs, "Lim Oversampling", osNames(), d.lim.osFactorLog2);
    B.f (id::limLinkT, "Lim Link Transients", Range (0, 100, 0.1f), d.lim.linkTransientsPct, pctText);
    B.f (id::limLinkR, "Lim Link Release", Range (0, 100, 0.1f), d.lim.linkReleasePct, pctText);
    B.c (id::limDither, "Lim Dither", ditherNames(), d.lim.dither);

    return { B.params.begin(), B.params.end() };
}

ParamCache::ParamCache (juce::AudioProcessorValueTreeState& s) : state (s)
{
    for (int i = 0; i < bm::kNumEqBands; ++i)
    {
        auto& b = bands[i];
        b.on = get (id::eq (i, "on")); b.type = get (id::eq (i, "type")); b.freq = get (id::eq (i, "freq"));
        b.gain = get (id::eq (i, "gain")); b.q = get (id::eq (i, "q")); b.slope = get (id::eq (i, "slope"));
        b.place = get (id::eq (i, "place")); b.dyn = get (id::eq (i, "dyn")); b.range = get (id::eq (i, "range"));
        b.thr = get (id::eq (i, "thr"));
    }
    inGain = get (id::inGain); outGain = get (id::outGain); bypass = get (id::bypass); oneOn = get (id::oneOn);
    oneAmt = get (id::oneAmt); eqOn = get (id::eqOn); satOn = get (id::satOn); compOn = get (id::compOn); limOn = get (id::limOn);
    satStyle = get (id::satStyle); satDrive = get (id::satDrive); satMix = get (id::satMix); satTone = get (id::satTone);
    satDyn = get (id::satDyn); satFb = get (id::satFb); satFbFreq = get (id::satFbFreq); satOut = get (id::satOut);
    satOs = get (id::satOs); satHq = get (id::satHq);
    compStyle = get (id::compStyle); compThr = get (id::compThr); compRatio = get (id::compRatio); compKnee = get (id::compKnee);
    compAtt = get (id::compAtt); compRel = get (id::compRel); compAutoRel = get (id::compAutoRel); compLa = get (id::compLa);
    compHold = get (id::compHold); compRange = get (id::compRange); compMakeup = get (id::compMakeup);
    compAutoGain = get (id::compAutoGain); compMix = get (id::compMix); compScHp = get (id::compScHp);
    compScLp = get (id::compScLp); compLink = get (id::compLink);
    limStyle = get (id::limStyle); limGain = get (id::limGain); limCeil = get (id::limCeil); limAtt = get (id::limAtt);
    limRel = get (id::limRel); limLa = get (id::limLa); limTp = get (id::limTp); limOs = get (id::limOs);
    limLinkT = get (id::limLinkT); limLinkR = get (id::limLinkR); limDither = get (id::limDither);
}

bm::ChainParams ParamCache::read() const noexcept
{
    auto v = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed); };
    auto on = [&] (const std::atomic<float>* a) { return v (a) > 0.5f; };
    auto ix = [&] (const std::atomic<float>* a) { return (int) std::lround (v (a)); };

    bm::ChainParams p;
    p.inputGainDb = v (inGain); p.outputGainDb = v (outGain); p.masterBypass = on (bypass);
    p.oneMasterOn = on (oneOn); p.oneMasterPct = v (oneAmt);
    p.moduleOn[bm::kModEq] = on (eqOn); p.moduleOn[bm::kModSat] = on (satOn);
    p.moduleOn[bm::kModComp] = on (compOn); p.moduleOn[bm::kModLim] = on (limOn);

    for (int i = 0; i < bm::kNumEqBands; ++i)
    {
        auto& b = p.eq.bands[(size_t) i];
        const auto& c = bands[i];
        b.enabled = on (c.on); b.type = (bm::BandType) ix (c.type); b.freq = v (c.freq); b.gainDb = v (c.gain);
        b.q = v (c.q); b.slopeIdx = ix (c.slope); b.placement = (bm::Placement) ix (c.place);
        b.dynOn = on (c.dyn); b.dynRangeDb = v (c.range); b.dynThresholdDb = v (c.thr);
    }
    p.sat.style = (bm::SatStyle) ix (satStyle); p.sat.drivePct = v (satDrive); p.sat.mixPct = v (satMix);
    p.sat.tone = v (satTone); p.sat.dynamics = v (satDyn); p.sat.feedbackPct = v (satFb);
    p.sat.feedbackFreq = v (satFbFreq); p.sat.outputDb = v (satOut); p.sat.osFactorLog2 = ix (satOs);
    p.sat.quality = on (satHq) ? 1 : 0;

    p.comp.style = (bm::CompStyle) ix (compStyle); p.comp.thresholdDb = v (compThr); p.comp.ratio = v (compRatio);
    p.comp.kneeDb = v (compKnee); p.comp.attackMs = v (compAtt); p.comp.releaseMs = v (compRel);
    p.comp.autoRelease = on (compAutoRel); p.comp.lookaheadMs = v (compLa); p.comp.holdMs = v (compHold);
    p.comp.rangeDb = v (compRange); p.comp.makeupDb = v (compMakeup); p.comp.autoGain = on (compAutoGain);
    p.comp.mixPct = v (compMix); p.comp.scHpFreq = v (compScHp); p.comp.scLpFreq = v (compScLp) >= 19999 ? 22000.0f : v (compScLp);
    p.comp.stereoLinkPct = v (compLink);

    p.lim.style = (bm::LimStyle) ix (limStyle); p.lim.inputGainDb = v (limGain); p.lim.ceilingDb = v (limCeil);
    p.lim.attackMs = v (limAtt); p.lim.releaseMs = v (limRel); p.lim.lookaheadMs = v (limLa);
    p.lim.truePeak = on (limTp); p.lim.osFactorLog2 = ix (limOs); p.lim.linkTransientsPct = v (limLinkT);
    p.lim.linkReleasePct = v (limLinkR); p.lim.dither = ix (limDither);
    return p;
}

void replaceStateAndSync (juce::AudioProcessorValueTreeState& s, const juce::ValueTree& newState)
{
    s.replaceState (newState);
    for (int i = 0; i < newState.getNumChildren(); ++i)
    {
        const auto child = newState.getChild (i);
        const auto pid = child.getProperty ("id").toString();
        if (pid.isEmpty() || ! child.hasProperty ("value")) continue;
        if (auto* prm = s.getParameter (pid))
        {
            const float target = prm->convertTo0to1 ((float) child.getProperty ("value"));
            if (std::abs (prm->getValue() - target) > 1e-7f)
                prm->setValueNotifyingHost (target);
        }
    }
}

void writeChainToState (juce::AudioProcessorValueTreeState& s, const bm::ChainParams& p)
{
    auto set = [&] (const juce::String& pid, float value)
    {
        if (auto* prm = s.getParameter (pid))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
            prm->endChangeGesture();
        }
    };
    auto setB = [&] (const juce::String& pid, bool b) { set (pid, b ? 1.0f : 0.0f); };

    set (id::inGain, p.inputGainDb); set (id::outGain, p.outputGainDb);
    setB (id::oneOn, p.oneMasterOn); set (id::oneAmt, p.oneMasterPct);
    setB (id::eqOn, p.moduleOn[bm::kModEq]); setB (id::satOn, p.moduleOn[bm::kModSat]);
    setB (id::compOn, p.moduleOn[bm::kModComp]); setB (id::limOn, p.moduleOn[bm::kModLim]);
    for (int i = 0; i < bm::kNumEqBands; ++i)
    {
        const auto& b = p.eq.bands[(size_t) i];
        setB (id::eq (i, "on"), b.enabled); set (id::eq (i, "type"), (float) b.type); set (id::eq (i, "freq"), b.freq);
        set (id::eq (i, "gain"), b.gainDb); set (id::eq (i, "q"), b.q); set (id::eq (i, "slope"), (float) b.slopeIdx);
        set (id::eq (i, "place"), (float) b.placement); setB (id::eq (i, "dyn"), b.dynOn);
        set (id::eq (i, "range"), b.dynRangeDb); set (id::eq (i, "thr"), b.dynThresholdDb);
    }
    set (id::satStyle, (float) p.sat.style); set (id::satDrive, p.sat.drivePct); set (id::satMix, p.sat.mixPct);
    set (id::satTone, p.sat.tone); set (id::satDyn, p.sat.dynamics); set (id::satFb, p.sat.feedbackPct);
    set (id::satFbFreq, p.sat.feedbackFreq); set (id::satOut, p.sat.outputDb); set (id::satOs, (float) p.sat.osFactorLog2);
    setB (id::satHq, p.sat.quality == 1);
    set (id::compStyle, (float) p.comp.style); set (id::compThr, p.comp.thresholdDb); set (id::compRatio, p.comp.ratio);
    set (id::compKnee, p.comp.kneeDb); set (id::compAtt, p.comp.attackMs); set (id::compRel, p.comp.releaseMs);
    setB (id::compAutoRel, p.comp.autoRelease); set (id::compLa, p.comp.lookaheadMs); set (id::compHold, p.comp.holdMs);
    set (id::compRange, p.comp.rangeDb); set (id::compMakeup, p.comp.makeupDb); setB (id::compAutoGain, p.comp.autoGain);
    set (id::compMix, p.comp.mixPct); set (id::compScHp, p.comp.scHpFreq); set (id::compScLp, std::min (20000.0f, p.comp.scLpFreq));
    set (id::compLink, p.comp.stereoLinkPct);
    set (id::limStyle, (float) p.lim.style); set (id::limGain, p.lim.inputGainDb); set (id::limCeil, p.lim.ceilingDb);
    set (id::limAtt, p.lim.attackMs); set (id::limRel, p.lim.releaseMs); set (id::limLa, p.lim.lookaheadMs);
    setB (id::limTp, p.lim.truePeak); set (id::limOs, (float) p.lim.osFactorLog2); set (id::limLinkT, p.lim.linkTransientsPct);
    set (id::limLinkR, p.lim.linkReleasePct); set (id::limDither, (float) p.lim.dither);
}

} // namespace bmp
