// BANKMASTER by KALOX — parameter layout and ChainParams <-> APVTS mapping
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../DSP/FactoryPresets.h"

namespace bmp
{
namespace id
{
    inline const juce::String inGain = "in_gain", inTarget = "in_target", outGain = "out_gain", bypass = "bypass",
        oneOn = "one_on", oneAmt = "one_amt",
        eqOn = "eq_on", satOn = "sat_on", compOn = "comp_on", limOn = "lim_on",
        satStyle = "sat_style", satDrive = "sat_drive", satMix = "sat_mix", satTone = "sat_tone", satDyn = "sat_dyn",
        satFb = "sat_fb", satFbFreq = "sat_fbfreq", satOut = "sat_out", satOs = "sat_os", satHq = "sat_hq",
        compStyle = "comp_style", compThr = "comp_thr", compRatio = "comp_ratio", compKnee = "comp_knee",
        compAtt = "comp_att", compRel = "comp_rel", compAutoRel = "comp_autorel", compLa = "comp_la",
        compHold = "comp_hold", compRange = "comp_range", compMakeup = "comp_makeup", compAutoGain = "comp_autogain",
        compMix = "comp_mix", compScHp = "comp_schp", compScLp = "comp_sclp", compLink = "comp_link",
        limStyle = "lim_style", limGain = "lim_gain", limCeil = "lim_ceil", limAtt = "lim_att", limRel = "lim_rel",
        limLa = "lim_la", limTp = "lim_tp", limOs = "lim_os", limLinkT = "lim_linkt", limLinkR = "lim_linkr",
        limDither = "lim_dither";

    inline juce::String eq (int band, const char* what) { return "eq" + juce::String (band + 1) + "_" + what; }
}

juce::StringArray bandTypeNames();
juce::StringArray slopeNames();
juce::StringArray placementNames();
juce::StringArray satStyleNames();
juce::StringArray compStyleNames();
juce::StringArray limStyleNames();
juce::StringArray osNames();
juce::StringArray ditherNames();

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

/** Lock-free cached access to all parameter values (audio thread). */
class ParamCache
{
public:
    explicit ParamCache (juce::AudioProcessorValueTreeState& s);
    bm::ChainParams read() const noexcept;
private:
    juce::AudioProcessorValueTreeState& state;
    std::atomic<float>* get (const juce::String& pid) const { return state.getRawParameterValue (pid); }
    struct Band { std::atomic<float> *on, *type, *freq, *gain, *q, *slope, *place, *dyn, *range, *thr; };
    Band bands[bm::kNumEqBands] {};
    std::atomic<float> *inGain, *outGain, *bypass, *oneOn, *oneAmt, *eqOn, *satOn, *compOn, *limOn,
        *satStyle, *satDrive, *satMix, *satTone, *satDyn, *satFb, *satFbFreq, *satOut, *satOs, *satHq,
        *compStyle, *compThr, *compRatio, *compKnee, *compAtt, *compRel, *compAutoRel, *compLa, *compHold,
        *compRange, *compMakeup, *compAutoGain, *compMix, *compScHp, *compScLp, *compLink,
        *limStyle, *limGain, *limCeil, *limAtt, *limRel, *limLa, *limTp, *limOs, *limLinkT, *limLinkR, *limDither;
};

/** Replaces the whole state and makes every parameter object match it exactly.
    (APVTS::replaceState alone leaves a parameter untouched when its quantised tree
    value did not change, e.g. a bool parameter holding a raw 0.64 from the host.) */
void replaceStateAndSync (juce::AudioProcessorValueTreeState& s, const juce::ValueTree& newState);

/** Writes a ChainParams snapshot into the parameters (message thread, notifies host). */
void writeChainToState (juce::AudioProcessorValueTreeState& s, const bm::ChainParams& p);

} // namespace bmp
