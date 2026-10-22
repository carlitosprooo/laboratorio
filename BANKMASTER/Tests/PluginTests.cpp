// BANKMASTER BY KALOX — plugin-level tests (JUCE wrapper): state, presets, automation,
// A/B, undo/redo, bus layouts, latency reporting, editor creation + UI snapshots.
//   bankmaster_plugin_tests [--snapshot <dir>]
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Source/Plugin/PluginProcessor.h"
#include "../Source/Plugin/PluginEditor.h"
#include <thread>
#include <map>

namespace
{
int failures = 0, checks = 0;
#define PCHECK(c) do { ++checks; if (! (c)) { ++failures; std::printf ("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define PREPORT(...) do { std::printf ("    | "); std::printf (__VA_ARGS__); std::printf ("\n"); } while (0)

void fillMusic (juce::AudioBuffer<float>& b, double fs, int offset, float level)
{
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = (offset + i) / fs;
        const double beat = std::fmod (t, 0.5);
        const double kick = std::exp (-beat * 18) * std::sin (2 * juce::MathConstants<double>::pi * (50 + 120 * std::exp (-beat * 40)) * beat);
        const double bass = 0.5 * std::sin (2 * juce::MathConstants<double>::pi * 55 * t);
        const double pad = 0.2 * std::sin (2 * juce::MathConstants<double>::pi * 440 * t) + 0.1 * std::sin (2 * juce::MathConstants<double>::pi * 6000 * t);
        for (int c = 0; c < b.getNumChannels(); ++c)
            b.setSample (c, i, (float) (level * (0.9 * kick + bass + pad * (c == 0 ? 1.0 : 0.8))));
    }
}

void setParam (BankMasterProcessor& p, const juce::String& id, float value)
{
    auto* prm = p.getState().getParameter (id);
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    prm->endChangeGesture();
}

float getParam (BankMasterProcessor& p, const juce::String& id)
{
    return p.getState().getRawParameterValue (id)->load();
}

void runTest (const char* name, const std::function<void()>& fn)
{
    const int before = failures;
    std::printf ("[ RUN  ] %s\n", name);
    fn();
    std::printf ("[ %s ] %s\n", failures == before ? " OK " : "FAIL", name);
}

/** Denser program for the UI snapshots: the music signal plus pink noise, so the
    analyser shows a realistic broadband spectrum instead of isolated test tones. */
void fillRich (juce::AudioBuffer<float>& b, double fs, int offset, float level)
{
    fillMusic (b, fs, offset, level);
    static juce::Random rng (11);
    static double s[2][7] {};
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const double w = rng.nextFloat() * 2.0 - 1.0;
            auto& k = s[c];
            k[0] = 0.99886 * k[0] + w * 0.0555179; k[1] = 0.99332 * k[1] + w * 0.0750759;
            k[2] = 0.96900 * k[2] + w * 0.1538520; k[3] = 0.86650 * k[3] + w * 0.3104856;
            k[4] = 0.55000 * k[4] + w * 0.5329522; k[5] = -0.7616 * k[5] - w * 0.0168980;
            const double pink = (k[0] + k[1] + k[2] + k[3] + k[4] + k[5] + k[6] + w * 0.5362) * 0.11;
            k[6] = w * 0.115926;
            b.addSample (c, i, (float) (level * 0.6 * pink));
        }
}

bool hasDisplay()
{
   #if JUCE_LINUX
    return std::getenv ("DISPLAY") != nullptr;
   #else
    return true;
   #endif
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::String snapshotDir;
    for (int i = 1; i < argc - 1; ++i) if (juce::String (argv[i]) == "--snapshot") snapshotDir = argv[i + 1];

    const auto tempPresetDir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("bankmaster_test_presets");
    tempPresetDir.deleteRecursively();
    PresetManager::setUserPresetDirectoryOverride (tempPresetDir);

    runTest ("defaults_are_kalox_master_original", [&]
    {
        BankMasterProcessor p;
        const auto c = p.getCurrentParams();
        const auto k = bm::presets::kaloxOriginal();
        PCHECK (p.getPresets().getCurrentPresetName() == "KALOX MASTER - ORIGINAL");
        PCHECK (std::abs (c.eq.bands[0].freq - 25.0f) < 0.01f && c.eq.bands[0].type == bm::BandType::LowCut && c.eq.bands[0].slopeIdx == 3);
        PCHECK (std::abs (c.eq.bands[2].freq - 861.41f) < 0.05f && std::abs (c.eq.bands[2].q - 1.622f) < 0.001f);
        PCHECK (std::abs (c.eq.bands[3].freq - 1559.3f) < 0.1f);
        PCHECK (std::abs (c.eq.bands[4].gainDb - 1.0f) < 1e-4f && c.eq.bands[4].type == bm::BandType::HighShelf);
        PCHECK (c.sat.style == bm::SatStyle::CleanTube && std::abs (c.sat.drivePct - 20.0f) < 1e-3f);
        PCHECK (std::abs (c.comp.thresholdDb + 18.0f) < 1e-3f && std::abs (c.comp.ratio - 2.0f) < 1e-3f);
        PCHECK (std::abs (c.comp.attackMs - 30.0f) < 1e-2f && std::abs (c.comp.releaseMs - 100.0f) < 0.1f);
        PCHECK (std::abs (c.lim.inputGainDb - 11.0f) < 1e-3f && std::abs (c.lim.ceilingDb + 0.1f) < 1e-4f);
        PCHECK (std::abs (c.lim.attackMs - 98.21f) < 0.05f && std::abs (c.lim.releaseMs - 400.0f) < 0.5f);
        PCHECK (std::abs (c.lim.lookaheadMs - 1.004f) < 1e-3f && c.lim.truePeak && c.lim.osFactorLog2 == 4 && c.lim.dither == 0);
        PCHECK (c.moduleOn[0] && c.moduleOn[1] && c.moduleOn[2] && c.moduleOn[3] && ! c.masterBypass);
        (void) k;
    });

    runTest ("processes_real_audio_reports_latency", [&]
    {
        BankMasterProcessor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        PCHECK (p.getLatencySamples() == p.getChain().getLatency());
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        double maxAbs = 0; bool finite = true;
        for (int b = 0; b < 48000 * 6 / 512; ++b)
        {
            fillMusic (buf, 48000.0, b * 512, 0.12f);
            p.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i) { const float x = buf.getSample (c, i); finite &= std::isfinite (x); if (b > 10) maxAbs = std::max (maxAbs, (double) std::abs (x)); }
        }
        const float I = p.getChain().getLoudness().getIntegrated();
        PREPORT ("6 s through the full chain: finite %d, sample peak %.3f dBFS, integrated %.2f LUFS, latency %d samples",
                 (int) finite, juce::Decibels::gainToDecibels (maxAbs), I, p.getLatencySamples());
        PCHECK (finite);
        PCHECK (juce::Decibels::gainToDecibels (maxAbs) <= -0.1 + 1e-3);
        PCHECK (I > -20.0f && I < -3.0f);

        // larger-than-prepared host block is split internally
        juce::AudioBuffer<float> big (2, 4096);
        fillMusic (big, 48000.0, 0, 0.1f);
        p.processBlock (big, midi);
        PCHECK (std::isfinite (big.getSample (0, 4095)));

        // latency change (lookahead) is reported from the message thread
        setParam (p, bmp::id::limLa, 3.0f);
        for (int b = 0; b < 20; ++b) { fillMusic (buf, 48000.0, b * 512, 0.1f); p.processBlock (buf, midi); }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        PREPORT ("after lookahead 1.004 -> 3 ms: host latency %d, chain latency %d", p.getLatencySamples(), p.getChain().getLatency());
        PCHECK (p.getLatencySamples() == p.getChain().getLatency());
    });

    runTest ("input_learn_sets_reference_level", [&]
    {
        BankMasterProcessor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
        const float amp = juce::Decibels::decibelsToGain (-31.0f); // 1 kHz stereo sine -> -31 LUFS
        p.startInputLearn();
        for (int b = 0; b < 48000 * 8 / 512; ++b)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    buf.setSample (c, i, amp * std::sin (2.0f * juce::MathConstants<float>::pi * 1000.0f * (float) (b * 512 + i) / 48000.0f));
            p.processBlock (buf, midi);
        }
        const float measured = p.getLearnMeasuredLufs();
        const float g = p.stopInputLearn (true);
        PREPORT ("measured %.2f LUFS -> INPUT gain set to %+.2f dB (target -20 LUFS)", measured, g);
        PCHECK (std::abs (measured + 31.0f) < 0.15f);
        PCHECK (std::abs (g - 11.0f) < 0.15f);
        PCHECK (std::abs (getParam (p, bmp::id::inGain) - g) < 0.02f);
        PCHECK (! p.isLearningInput());
    });

    runTest ("bus_layouts", [&]
    {
        BankMasterProcessor p;
        juce::AudioProcessor::BusesLayout stereo, mono, surround;
        stereo.inputBuses.add (juce::AudioChannelSet::stereo()); stereo.outputBuses.add (juce::AudioChannelSet::stereo());
        mono.inputBuses.add (juce::AudioChannelSet::mono()); mono.outputBuses.add (juce::AudioChannelSet::mono());
        surround.inputBuses.add (juce::AudioChannelSet::create5point1()); surround.outputBuses.add (juce::AudioChannelSet::create5point1());
        PCHECK (p.checkBusesLayoutSupported (stereo));
        PCHECK (p.checkBusesLayoutSupported (mono));
        PCHECK (! p.checkBusesLayoutSupported (surround));
        // mono processing
        p.setPlayConfigDetails (1, 1, 44100.0, 256);
        p.prepareToPlay (44100.0, 256);
        juce::AudioBuffer<float> buf (1, 256); juce::MidiBuffer midi;
        bool finite = true;
        for (int b = 0; b < 400; ++b) { fillMusic (buf, 44100.0, b * 256, 0.2f); p.processBlock (buf, midi); for (int i = 0; i < 256; ++i) finite &= std::isfinite (buf.getSample (0, i)); }
        PCHECK (finite);
    });

    runTest ("state_save_and_restore_roundtrip", [&]
    {
        BankMasterProcessor a;
        setParam (a, bmp::id::eq (2, "gain"), -3.5f);
        setParam (a, bmp::id::satDrive, 37.0f);
        setParam (a, bmp::id::compRatio, 4.0f);
        setParam (a, bmp::id::limCeil, -1.0f);
        setParam (a, bmp::id::limOs, 2.0f);
        setParam (a, bmp::id::oneOn, 1.0f);
        setParam (a, bmp::id::oneAmt, 130.0f);
        juce::MemoryBlock mb;
        a.getStateInformation (mb);
        BankMasterProcessor b;
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        int mismatches = 0;
        for (auto* prm : a.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
                if (std::abs (rp->getValue() - b.getState().getParameter (rp->getParameterID())->getValue()) > 1e-6f) ++mismatches;
        PREPORT ("%d parameters, %d mismatches after restore, state size %d bytes", a.getParameters().size(), mismatches, (int) mb.getSize());
        PCHECK (mismatches == 0);
        PCHECK (std::abs (getParam (b, bmp::id::satDrive) - 37.0f) < 0.05f);
        PCHECK (std::abs (getParam (b, bmp::id::limCeil) + 1.0f) < 1e-3f);
        // garbage state is ignored safely
        const char junk[] = "not a plugin state";
        b.setStateInformation (junk, (int) sizeof (junk));
        PCHECK (std::abs (getParam (b, bmp::id::satDrive) - 37.0f) < 0.05f);
    });

    runTest ("factory_presets_load_exact_values", [&]
    {
        BankMasterProcessor p;
        const auto& list = bm::getFactoryPresets();
        PCHECK (p.getNumPrograms() == (int) list.size() && list.size() == 7);
        for (int i = 0; i < (int) list.size(); ++i)
        {
            p.setCurrentProgram (i);
            const auto c = p.getCurrentParams();
            const auto& e = list[(size_t) i].params;
            bool ok = std::abs (c.lim.inputGainDb - e.lim.inputGainDb) < 0.01f && std::abs (c.comp.ratio - e.comp.ratio) < 0.01f
                   && std::abs (c.sat.drivePct - e.sat.drivePct) < 0.05f && c.lim.style == e.lim.style;
            for (int b = 0; b < bm::kNumEqBands; ++b)
            {
                const auto& x = c.eq.bands[(size_t) b]; const auto& y = e.eq.bands[(size_t) b];
                ok &= x.enabled == y.enabled && x.type == y.type && std::abs (x.freq - y.freq) / y.freq < 1e-3f
                   && std::abs (x.gainDb - y.gainDb) < 0.01f && std::abs (x.q - y.q) / y.q < 2e-3f && x.dynOn == y.dynOn;
            }
            PREPORT ("%-26s -> %s", list[(size_t) i].name.c_str(), ok ? "exact" : "MISMATCH");
            PCHECK (ok);
            PCHECK (p.getPresets().getCurrentPresetName() == juce::String (list[(size_t) i].name));
            PCHECK (p.getCurrentProgram() == i);
        }
    });

    runTest ("user_preset_save_load_delete", [&]
    {
        BankMasterProcessor p;
        setParam (p, bmp::id::limGain, 7.25f);
        setParam (p, bmp::id::eq (1, "freq"), 333.0f);
        PCHECK (p.getPresets().saveUserPreset ("My Club Master"));
        PCHECK (! p.getPresets().saveUserPreset ("KALOX MASTER - ORIGINAL")); // factory names are read-only
        PCHECK (tempPresetDir.getChildFile ("My Club Master.bmpreset").existsAsFile());
        p.setCurrentProgram (0);
        PCHECK (std::abs (getParam (p, bmp::id::limGain) - 11.0f) < 1e-3f);
        PCHECK (p.getPresets().loadPresetByName ("My Club Master"));
        PCHECK (std::abs (getParam (p, bmp::id::limGain) - 7.25f) < 1e-3f);
        PCHECK (std::abs (getParam (p, bmp::id::eq (1, "freq")) - 333.0f) < 0.5f);
        PCHECK (p.getPresets().getCurrentPresetName() == "My Club Master");
        PCHECK (p.getPresets().getAllPresetNames().size() == 8);
        PCHECK (p.getPresets().deleteUserPreset ("My Club Master"));
        PCHECK (p.getPresets().getAllPresetNames().size() == 7);
    });

    runTest ("ab_compare_and_undo_redo", [&]
    {
        BankMasterProcessor p;
        setParam (p, bmp::id::limGain, 5.0f);
        p.selectAbSlot (1);                          // B starts as a copy of A
        PCHECK (std::abs (getParam (p, bmp::id::limGain) - 5.0f) < 1e-3f);
        setParam (p, bmp::id::limGain, 9.0f);
        p.selectAbSlot (0);
        PCHECK (std::abs (getParam (p, bmp::id::limGain) - 5.0f) < 1e-3f);
        p.selectAbSlot (1);
        PCHECK (std::abs (getParam (p, bmp::id::limGain) - 9.0f) < 1e-3f);
        p.copyAbToOther();                           // B -> A
        p.selectAbSlot (0);
        PCHECK (std::abs (getParam (p, bmp::id::limGain) - 9.0f) < 1e-3f);

        // no message-loop pumping: the processor's undo helpers flush APVTS explicitly,
        // so the grouping is deterministic on every platform
        auto& um = p.getUndoManager();
        p.beginUndoTransaction();
        um.clearUndoHistory();
        p.beginUndoTransaction();
        setParam (p, bmp::id::compThr, -24.0f);
        p.beginUndoTransaction();
        setParam (p, bmp::id::compThr, -30.0f);
        PCHECK (um.canUndo());
        p.undo();
        PCHECK (std::abs (getParam (p, bmp::id::compThr) + 24.0f) < 1e-3f);
        p.undo();
        PCHECK (std::abs (getParam (p, bmp::id::compThr) + 18.0f) < 1e-3f);
        p.redo();
        PCHECK (std::abs (getParam (p, bmp::id::compThr) + 24.0f) < 1e-3f);
    });

    runTest ("automation_from_another_thread_while_processing", [&]
    {
        BankMasterProcessor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 128);
        p.prepareToPlay (48000.0, 128);
        std::atomic<bool> stop { false };
        std::thread automator ([&]
        {
            juce::Random r (5);
            const char* ids[] = { "eq2_gain", "eq3_freq", "sat_drive", "sat_mix", "comp_thr", "comp_ratio", "lim_gain", "lim_rel", "one_amt", "out_gain", "in_gain" };
            while (! stop.load())
            {
                auto* prm = p.getState().getParameter (ids[r.nextInt (11)]);
                prm->setValueNotifyingHost (r.nextFloat());
                std::this_thread::sleep_for (std::chrono::microseconds (200));
            }
        });
        juce::AudioBuffer<float> buf (2, 128); juce::MidiBuffer midi;
        bool finite = true;
        for (int b = 0; b < 48000 * 4 / 128; ++b)
        {
            fillMusic (buf, 48000.0, b * 128, 0.2f);
            p.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 128; ++i) finite &= std::isfinite (buf.getSample (c, i));
        }
        stop = true; automator.join();
        PCHECK (finite);
    });

    // ------------------------------------------------------------------------------- V2 editor
    auto pump = [] (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); };
    auto feed = [] (BankMasterProcessor& p, int blocks, int offset)
    {
        juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
        for (int b = 0; b < blocks; ++b) { fillRich (buf, 48000.0, (offset + b) * 512, 0.12f); p.processBlock (buf, midi); }
    };
    auto makeEditor = [] (BankMasterProcessor& p)
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        return std::unique_ptr<BankMasterEditor> (dynamic_cast<BankMasterEditor*> (ed.release()));
    };
    // every visible component lies inside its parent (nothing cut off), recursively
    std::function<int (juce::Component&)> countOutside = [&] (juce::Component& c) -> int
    {
        int n = 0;
        for (auto* ch : c.getChildren())
        {
            if (! ch->isVisible() || ch->getWidth() <= 0) continue;
            if (! c.getLocalBounds().contains (ch->getBounds())) { ++n; PREPORT ("outside: %s in %s", ch->getName().toRawUTF8(), c.getName().toRawUTF8()); }
            n += countOutside (*ch);
        }
        return n;
    };

    runTest ("v2_editor_sizes_and_proportional_layout", [&]
    {
        if (! hasDisplay()) { PREPORT ("no display: skipped (run under xvfb-run)"); return; }
        BankMasterProcessor p;
        auto e = makeEditor (p);
        PCHECK (e != nullptr);
        PCHECK (e->getWidth() == 1200 && e->getHeight() == 780);
        PCHECK (std::abs (e->getUiScale() - 1.0f) < 1e-6f);
        PCHECK (countOutside (e->getContent()) == 0);
        auto* c = e->getConstrainer();
        PCHECK (c != nullptr && c->getMinimumWidth() == 1080 && c->getMinimumHeight() == 700
                && c->getMaximumWidth() == 2000 && c->getMaximumHeight() == 1300);
        const int sizes[][2] = { { 1080, 700 }, { 1200, 780 }, { 1600, 1040 }, { 2000, 1300 }, { 1500, 760 }, { 1100, 1000 } };
        for (auto& sz : sizes)
        {
            e->setSize (sz[0], sz[1]);
            const auto shown = e->getContent().getBoundsInParent();   // content after the proportional transform
            const auto t = e->getContent().getTransform();
            const auto scaled = juce::Rectangle<float> (0, 0, 1200, 780).transformedBy (t);
            PREPORT ("%d x %d -> scale %.3f, content %.1f x %.1f at (%.1f, %.1f)", sz[0], sz[1], e->getUiScale(),
                     scaled.getWidth(), scaled.getHeight(), scaled.getX(), scaled.getY());
            juce::ignoreUnused (shown);
            PCHECK (scaled.getX() >= -0.01f && scaled.getY() >= -0.01f);
            PCHECK (scaled.getRight() <= (float) sz[0] + 0.01f && scaled.getBottom() <= (float) sz[1] + 0.01f);
            PCHECK (std::abs (scaled.getWidth() - sz[0]) < 0.6f || std::abs (scaled.getHeight() - sz[1]) < 0.6f); // fills one axis
        }
        // 1080 x 700 is the smallest allowed size; the smallest text then measures 10.5 px * 0.897 = 9.4 px
        e->setSize (1080, 700);
        PCHECK (e->getUiScale() > 0.89f);
    });

    runTest ("v2_every_parameter_bound_to_a_control_both_ways", [&]
    {
        if (! hasDisplay()) { PREPORT ("no display: skipped (run under xvfb-run)"); return; }
        BankMasterProcessor p;
        auto e = makeEditor (p);
        std::map<juce::String, int> covered;
        auto& eqPage = e->getEqPage();
        int bindingFailures = 0;
        auto checkControl = [&] (juce::Component* c)
        {
            const auto pid = c->getComponentID();
            auto* prm = p.getState().getParameter (pid);
            if (prm == nullptr) { ++bindingFailures; PREPORT ("control without parameter: %s", c->getName().toRawUTF8()); return; }
            covered[pid]++;
            const float orig = prm->getValue();
            // host -> UI
            const float target = orig < 0.5f ? 0.8f : 0.2f;
            prm->setValueNotifyingHost (target);
            const float hostVal = prm->getValue();
            bool uiOk = false;
            if (auto* k = dynamic_cast<juce::Slider*> (c)) uiOk = std::abs (prm->convertTo0to1 ((float) k->getValue()) - hostVal) < 1e-3f;
            else if (auto* t = dynamic_cast<bmui::ParamToggle*> (c)) uiOk = t->getToggleState() == (hostVal > 0.5f);
            else if (auto* sg = dynamic_cast<bmui::ParamSegmented*> (c)) uiOk = sg->getSelectedIndex() == juce::roundToInt (prm->convertFrom0to1 (hostVal));
            else if (auto* cb = dynamic_cast<bmui::ParamSelector*> (c)) uiOk = cb->getSelectedItemIndex() == juce::roundToInt (prm->convertFrom0to1 (hostVal));
            if (! uiOk) { ++bindingFailures; PREPORT ("host->UI failed: %s", pid.toRawUTF8()); }
            // UI -> host
            bool hostOk = false;
            if (auto* k = dynamic_cast<juce::Slider*> (c))
            {
                const double v = k->proportionOfLengthToValue (orig < 0.5f ? 0.3 : 0.7);
                k->setValue (v, juce::sendNotificationSync);
                hostOk = std::abs (prm->getValue() - prm->convertTo0to1 ((float) v)) < 1e-3f;
            }
            else if (auto* t = dynamic_cast<bmui::ParamToggle*> (c))
            {
                const bool want = ! t->getToggleState();
                t->setToggleState (want, juce::sendNotificationSync);
                hostOk = (prm->getValue() > 0.5f) == want;
            }
            else if (auto* sg = dynamic_cast<bmui::ParamSegmented*> (c))
            {
                const int want = sg->getSelectedIndex() == 0 ? 1 : 0;
                sg->choose (want);
                hostOk = juce::roundToInt (prm->convertFrom0to1 (prm->getValue())) == want;
            }
            else if (auto* cb = dynamic_cast<bmui::ParamSelector*> (c))
            {
                const int want = cb->getSelectedItemIndex() == 0 ? 1 : 0;
                cb->setSelectedItemIndex (want, juce::sendNotificationSync);
                hostOk = juce::roundToInt (prm->convertFrom0to1 (prm->getValue())) == want;
            }
            if (! hostOk) { ++bindingFailures; PREPORT ("UI->host failed: %s", pid.toRawUTF8()); }
            prm->setValueNotifyingHost (orig);
        };
        for (int b = 0; b < bm::kNumEqBands; ++b)
        {
            eqPage.selectBand (b);
            for (auto* c : e->getAllBoundControls())
            {
                const auto pid = c->getComponentID();
                const bool eqBandControl = pid.startsWith ("eq") && pid.containsChar ('_') && pid != bmp::id::eqOn;
                if (eqBandControl || b == 0) checkControl (c);
            }
        }
        int missing = 0;
        for (auto* prm : p.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
                if (covered.find (rp->getParameterID()) == covered.end()) { ++missing; PREPORT ("no control for %s", rp->getParameterID().toRawUTF8()); }
        PREPORT ("%d parameters, %d covered by controls, %d binding failures", p.getParameters().size(), (int) covered.size(), bindingFailures);
        PCHECK (missing == 0);
        PCHECK (bindingFailures == 0);
        PCHECK ((int) covered.size() == p.getParameters().size());
    });

    runTest ("v2_undo_redo_buttons_and_revert_with_confirmation", [&]
    {
        if (! hasDisplay()) { PREPORT ("no display: skipped (run under xvfb-run)"); return; }
        BankMasterProcessor p;
        p.setCurrentProgram (0);
        auto e = makeEditor (p);
        auto& h = e->getHeader();
        auto& um = p.getUndoManager();
        p.beginUndoTransaction();
        um.clearUndoHistory();
        e->getHeader().tick();
        PCHECK (! p.getPresets().isModified());
        PCHECK (! h.revert.isEnabled());
        PCHECK (! h.undoRedo.undoButton.isEnabled() && ! h.undoRedo.redoButton.isEnabled());

        // a user edit (gesture as the knob makes it)
        p.beginUndoTransaction ("edit");
        setParam (p, bmp::id::compThr, -26.0f);
        setParam (p, bmp::id::limGain, 8.5f);
        p.beginUndoTransaction();
        h.tick();
        PCHECK (p.getPresets().isModified());
        PCHECK (h.presetName.modified);
        PCHECK (h.revert.isEnabled());
        PCHECK (h.undoRedo.undoButton.isEnabled());

        // revert asks first; cancel keeps the edit
        h.revert.triggerClick();
        pump (50);
        PCHECK (e->isRevertPopoverVisible());
        e->cancelPopover();
        PCHECK (! e->isRevertPopoverVisible());
        PCHECK (std::abs (getParam (p, bmp::id::compThr) + 26.0f) < 1e-3f);

        // confirm restores the preset exactly
        e->requestRevert();
        PCHECK (e->isRevertPopoverVisible());
        e->confirmRevert();
        PCHECK (! e->isRevertPopoverVisible());
        PCHECK (std::abs (getParam (p, bmp::id::compThr) + 18.0f) < 1e-3f);
        PCHECK (std::abs (getParam (p, bmp::id::limGain) - 11.0f) < 1e-3f);
        h.tick();
        PCHECK (! p.getPresets().isModified());
        PCHECK (! h.revert.isEnabled());

        // the revert is one undo step: the UNDO arrow brings the edit back, REDO reverts again
        h.undoRedo.undoButton.triggerClick();
        pump (50);
        PCHECK (std::abs (getParam (p, bmp::id::compThr) + 26.0f) < 1e-3f);
        PCHECK (std::abs (getParam (p, bmp::id::limGain) - 8.5f) < 1e-3f);
        h.tick();
        PCHECK (p.getPresets().isModified());
        PCHECK (h.undoRedo.redoButton.isEnabled());
        h.undoRedo.redoButton.triggerClick();
        pump (50);
        PCHECK (std::abs (getParam (p, bmp::id::compThr) + 18.0f) < 1e-3f);
        PCHECK (std::abs (getParam (p, bmp::id::limGain) - 11.0f) < 1e-3f);
        // and the original edit itself can be undone back to the preset
        e->doUndo();
        e->doUndo();
        PCHECK (std::abs (getParam (p, bmp::id::compThr) + 18.0f) < 1e-3f);

        // keyboard shortcuts
        p.beginUndoTransaction ("edit2");
        setParam (p, bmp::id::satDrive, 55.0f);
        p.beginUndoTransaction();
        PCHECK (e->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)));
        PCHECK (std::abs (getParam (p, bmp::id::satDrive) - 20.0f) < 1e-3f);
        PCHECK (e->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)));
        PCHECK (std::abs (getParam (p, bmp::id::satDrive) - 55.0f) < 1e-3f);

        // revert of a user preset returns to the saved file values
        PCHECK (p.getPresets().saveUserPreset ("Revert Test"));
        setParam (p, bmp::id::limCeil, -1.0f);
        h.tick();
        PCHECK (p.getPresets().isModified());
        e->requestRevert();
        e->confirmRevert();
        PCHECK (std::abs (getParam (p, bmp::id::limCeil) + 0.1f) < 1e-3f);
        PCHECK (std::abs (getParam (p, bmp::id::satDrive) - 55.0f) < 1e-3f);
        PCHECK (! p.getPresets().isModified());
        p.getPresets().deleteUserPreset ("Revert Test");
    });

    runTest ("v2_ab_presets_navigation_and_ui_state", [&]
    {
        if (! hasDisplay()) { PREPORT ("no display: skipped (run under xvfb-run)"); return; }
        BankMasterProcessor p;
        p.setCurrentProgram (0);
        {
            auto e = makeEditor (p);
            auto& h = e->getHeader();
            h.next.triggerClick(); pump (30);
            PCHECK (p.getPresets().getCurrentPresetIndex() == 1);
            h.prev.triggerClick(); pump (30);
            PCHECK (p.getPresets().getCurrentPresetIndex() == 0);
            setParam (p, bmp::id::limGain, 4.0f);
            h.ab.choose (1);
            PCHECK (p.getAbSlot() == 1);
            setParam (p, bmp::id::limGain, 9.0f);
            h.ab.choose (0);
            PCHECK (std::abs (getParam (p, bmp::id::limGain) - 4.0f) < 1e-3f);
            // navigation: clicking a module node opens its page
            e->getNavStrip().nodes[2]->triggerClick(); pump (30);
            PCHECK (e->getCurrentTab() == 2);
            e->getEqPage().selectBand (4);
            e->setSize (1400, 910);
        }
        juce::MemoryBlock mb;
        p.getStateInformation (mb);
        BankMasterProcessor q;
        q.setStateInformation (mb.getData(), (int) mb.getSize());
        auto e2 = makeEditor (q);
        PCHECK (e2->getWidth() == 1400 && e2->getHeight() == 910);
        PCHECK (e2->getCurrentTab() == 2);
        PCHECK (e2->getEqPage().getSelectedBand() == 4);
        PCHECK (std::abs (q.getState().getRawParameterValue (bmp::id::limGain)->load() - 4.0f) < 1e-3f);
        // the UI preferences never end up in the parameter tree / presets
        PCHECK (! q.getState().state.getChildWithName ("UI").isValid());
    });

    runTest ("v2_snapshots_all_pages_sizes_and_scales", [&]
    {
        if (! hasDisplay()) { PREPORT ("no display: skipped (run under xvfb-run)"); return; }
        BankMasterProcessor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        auto e = makeEditor (p);
        const char* names[] = { "eq", "saturation", "compressor", "limiter" };
        struct Shot { int w, h; float scale; const char* tag; };
        // 2x = Retina; 1.25x / 1.5x = Windows display scaling; 1080 x 700 at 1x = smallest window on a 100 % screen
        const Shot shots[] = { { 1200, 780, 1.0f, "1200x780_1x" }, { 1200, 780, 2.0f, "1200x780_2x" },
                               { 1200, 780, 1.25f, "1200x780_125pct" }, { 1200, 780, 1.5f, "1200x780_150pct" },
                               { 1080, 700, 1.0f, "1080x700_1x" }, { 2000, 1300, 1.0f, "2000x1300_1x" } };
        int offset = 0;
        for (int tab = 0; tab < 4; ++tab)
        {
            e->setSize (1200, 780);
            e->showTab (tab);
            for (int b = 0; b < 150; ++b)   // ~1.6 s of audio so meters, analyser and histories are filled
            {
                feed (p, 1, offset++);
                if (b % 3 == 0) pump (20);
            }
            for (auto& s : shots)
            {
                if (snapshotDir.isEmpty()) break;
                if (tab > 0 && (s.w != 1200 || s.scale == 1.25f || s.scale == 1.5f)) continue;   // every page at 1x / 2x, the EQ page at every size and scale
                e->setSize (s.w, s.h);
                pump (30);
                auto img = e->createComponentSnapshot (e->getLocalBounds(), true, s.scale);
                PCHECK (img.getWidth() == juce::roundToInt (s.w * s.scale));
                auto f = juce::File (snapshotDir).getChildFile (juce::String ("v2_") + names[tab] + "_" + s.tag + ".png");
                f.getParentDirectory().createDirectory();
                f.deleteFile();
                juce::FileOutputStream os (f);
                juce::PNGImageFormat png;
                PCHECK (os.openedOk() && png.writeImageToStream (img, os));
                PREPORT ("snapshot %s", f.getFileName().toRawUTF8());
            }
        }
        e->setSize (1200, 780);
    });

    tempPresetDir.deleteRecursively();
    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
