// BANKMASTER BY KALOX — plug-in host check: identity, parameters, state and audio of a BUILT plug-in.
// Used to prove that a new version sounds exactly like the previous one (V1 x V2 null test).
//
//   bankmaster_hostcheck describe <plugin>                 identity of a VST3 bundle / AU component
//   bankmaster_hostcheck render   <plugin> <outDir> [--state-from <dir>]
//        renders a fixed program through the plug-in for several scenarios (default state, every
//        factory preset, 44.1 kHz, a random state) and writes raw float output + metadata.
//        With --state-from, the random scenario loads the state saved by another version.
//   bankmaster_hostcheck compare  <dirA> <dirB>            sample-by-sample comparison (exit 1 on any difference)
//
// The two versions are rendered in separate runs: an Audio Unit is identified by its codes, so only
// one version can be registered at a time.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace
{
using juce::String;

struct Scenario { String name; double fs; int block; int program; bool random; double seconds; };

std::vector<Scenario> scenarios()
{
    std::vector<Scenario> s;
    s.push_back ({ "default_48k_512", 48000.0, 512, -1, false, 12.0 });
    s.push_back ({ "default_44k1_256", 44100.0, 256, -1, false, 8.0 });
    for (int p = 0; p < 7; ++p) s.push_back ({ "program" + String (p) + "_48k_512", 48000.0, 512, p, false, 8.0 });
    s.push_back ({ "random_state_48k_480", 48000.0, 480, -1, true, 8.0 });
    return s;
}

/** Deterministic stereo programme: kick, bass, chords, hats and pink noise at about -18 LUFS. */
void makeSignal (double fs, int n, std::vector<float>& L, std::vector<float>& R)
{
    L.assign ((size_t) n, 0.0f); R.assign ((size_t) n, 0.0f);
    const double pi = juce::MathConstants<double>::pi, beat = 60.0 / 124.0;
    juce::Random rng (2026);
    double pk[2][7] {};
    double hp[2] {};
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs, tb = std::fmod (t, beat);
        const double kick = std::exp (-tb * 9.0) * std::sin (2 * pi * (47.0 * tb + 110.0 / 35.0 * (1.0 - std::exp (-tb * 35.0))));
        double saw = 0;
        const double f0 = ((int) (t / (4 * beat)) % 2 == 0) ? 55.0 : 49.0;
        for (int h = 1; h <= 6; ++h) saw += std::sin (2 * pi * f0 * h * t) / h;
        const double bass = 0.35 * saw * (1.0 - std::exp (-tb * 7.0));
        double pad = 0;
        for (double c : { 220.0, 261.63, 329.63 }) pad += 0.05 * std::sin (2 * pi * c * t) + 0.02 * std::sin (2 * pi * 2 * c * t);
        double out[2];
        for (int c = 0; c < 2; ++c)
        {
            const double w = rng.nextDouble() * 2.0 - 1.0;
            auto& b = pk[c];
            b[0] = 0.99886 * b[0] + w * 0.0555179; b[1] = 0.99332 * b[1] + w * 0.0750759;
            b[2] = 0.96900 * b[2] + w * 0.1538520; b[3] = 0.86650 * b[3] + w * 0.3104856;
            b[4] = 0.55000 * b[4] + w * 0.5329522; b[5] = -0.7616 * b[5] - w * 0.0168980;
            const double pink = (b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + w * 0.5362) * 0.11;
            b[6] = w * 0.115926;
            const double hat = (w - hp[c]) * std::exp (-std::fmod (t, beat / 4) * 50.0) * 0.12; hp[c] = w;
            out[c] = 0.8 * kick + bass + pad * (c == 0 ? 1.0 : 0.9) + 0.25 * pink + hat;
        }
        L[(size_t) i] = (float) (0.22 * out[0]);
        R[(size_t) i] = (float) (0.22 * out[1]);
    }
}

std::unique_ptr<juce::AudioPluginInstance> load (juce::AudioPluginFormatManager& fm, const String& path, double fs, int block,
                                                 juce::PluginDescription* descOut = nullptr)
{
    juce::OwnedArray<juce::PluginDescription> types;
    for (auto* f : fm.getFormats())
        if (f->fileMightContainThisPluginType (path))
        {
            f->findAllTypesForFile (types, path);
            if (! types.isEmpty()) break;
        }
    if (types.isEmpty()) { std::printf ("no plug-in found in %s\n", path.toRawUTF8()); return nullptr; }
    String err;
    auto inst = fm.createPluginInstance (*types[0], fs, block, err);
    if (inst == nullptr) { std::printf ("could not load %s: %s\n", path.toRawUTF8(), err.toRawUTF8()); return nullptr; }
    if (descOut != nullptr) *descOut = *types[0];
    return inst;
}

String describe (const juce::PluginDescription& d)
{
    String s;
    s << "name=" << d.name << "\n"
      << "manufacturer=" << d.manufacturerName << "\n"
      << "format=" << d.pluginFormatName << "\n"
      << "version=" << d.version << "\n"
      << "uniqueId=" << String::toHexString (d.uniqueId) << "\n"
      << "deprecatedUid=" << String::toHexString (d.deprecatedUid) << "\n"
      << "identifier=" << d.createIdentifierString() << "\n"
      << "category=" << d.category << "\n";
    return s;
}

/** Parameter identity: ID, name, normalised default, steps, bool (must never change between versions). */
String parameterDump (juce::AudioPluginInstance& p)
{
    String s;
    for (auto* prm : p.getParameters())
    {
        String id;
        if (auto* h = dynamic_cast<juce::HostedAudioProcessorParameter*> (prm)) id = h->getParameterID();
        s << id << "|" << prm->getName (64) << "|" << String (prm->getDefaultValue(), 6) << "|" << prm->getNumSteps()
          << "|" << (prm->isBoolean() ? 1 : 0) << "\n";
    }
    return s;
}

/** Display text of every parameter at its default (presentation only). */
String parameterTexts (juce::AudioPluginInstance& p)
{
    String s;
    for (auto* prm : p.getParameters())
        s << prm->getName (64) << "|" << prm->getText (prm->getDefaultValue(), 64) << "\n";
    return s;
}

void randomise (juce::AudioPluginInstance& p)
{
    juce::Random r (77);
    for (auto* prm : p.getParameters())
    {
        String id;
        if (auto* h = dynamic_cast<juce::HostedAudioProcessorParameter*> (prm)) id = h->getParameterID();
        if (id == "bypass") continue;   // keep the chain audible
        prm->setValueNotifyingHost (r.nextFloat());
    }
}

int render (const String& pluginPath, const juce::File& outDir, const juce::File& stateFrom)
{
    juce::AudioPluginFormatManager fm;
    fm.addDefaultFormats();
    outDir.createDirectory();
    for (const auto& sc : scenarios())
    {
        juce::PluginDescription desc;
        auto p = load (fm, pluginPath, sc.fs, sc.block, &desc);
        if (p == nullptr) return 1;
        p->enableAllBuses();
        p->setPlayConfigDetails (2, 2, sc.fs, sc.block);
        if (sc.name == "default_48k_512")
        {
            outDir.getChildFile ("description.txt").replaceWithText (describe (desc));
            outDir.getChildFile ("parameters.txt").replaceWithText (parameterDump (*p));
            outDir.getChildFile ("parameter_texts.txt").replaceWithText (parameterTexts (*p));
        }
        if (sc.program >= 0) p->setCurrentProgram (sc.program);
        if (sc.random)
        {
            // the reference version randomises a separate instance and saves its state; every
            // version (reference included) then renders from that state restored into a fresh instance
            const auto blobFile = (stateFrom != juce::File() ? stateFrom : outDir).getChildFile ("random_state.bin");
            if (stateFrom == juce::File())
            {
                auto src = load (fm, pluginPath, sc.fs, sc.block);
                if (src == nullptr) return 1;
                randomise (*src);
                juce::MemoryBlock mb;
                src->getStateInformation (mb);
                blobFile.replaceWithData (mb.getData(), mb.getSize());
            }
            juce::MemoryBlock mb;
            if (! blobFile.loadFileAsData (mb)) { std::printf ("missing %s\n", blobFile.getFullPathName().toRawUTF8()); return 1; }
            p->setStateInformation (mb.getData(), (int) mb.getSize());
            // plug-in parameter values after the state is restored (host-only Bypass / Program excluded)
            String vals;
            for (auto* prm : p->getParameters())
            {
                String id;
                if (auto* h = dynamic_cast<juce::HostedAudioProcessorParameter*> (prm)) id = h->getParameterID();
                if (prm->getName (64) == "Bypass" || prm->getName (64) == "Program") continue;
                vals << id << "=" << String (prm->getValue(), 7) << "\n";
            }
            outDir.getChildFile ("random_state_values.txt").replaceWithText (vals);
        }
        // let message-thread work (latency report, async updates) settle, as a host would
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        p->prepareToPlay (sc.fs, sc.block);

        const int n = (int) (sc.fs * sc.seconds);
        std::vector<float> L, R;
        makeSignal (sc.fs, n, L, R);
        juce::AudioBuffer<float> buf (2, sc.block);
        juce::MidiBuffer midi;
        juce::FileOutputStream os (outDir.getChildFile (sc.name + ".f32"));
        os.setPosition (0); os.truncate();
        for (int start = 0; start < n; start += sc.block)
        {
            const int len = juce::jmin (sc.block, n - start);
            buf.setSize (2, len, false, false, true);
            std::copy (L.begin() + start, L.begin() + start + len, buf.getWritePointer (0));
            std::copy (R.begin() + start, R.begin() + start + len, buf.getWritePointer (1));
            p->processBlock (buf, midi);
            for (int i = 0; i < len; ++i) { os.writeFloat (buf.getSample (0, i)); os.writeFloat (buf.getSample (1, i)); }
        }
        os.flush();
        String meta;
        meta << "latency=" << p->getLatencySamples() << "\nsamples=" << n << "\n";
        outDir.getChildFile (sc.name + ".txt").replaceWithText (meta);
        p->releaseResources();
        std::printf ("rendered %-24s latency %d\n", sc.name.toRawUTF8(), p->getLatencySamples());
    }
    return 0;
}

int compare (const juce::File& a, const juce::File& b)
{
    int failures = 0;
    auto same = [&] (const String& file)
    {
        const auto ta = a.getChildFile (file).loadFileAsString(), tb = b.getChildFile (file).loadFileAsString();
        const bool ok = ta == tb && ta.isNotEmpty();
        std::printf ("%-34s %s\n", file.toRawUTF8(), ok ? "identical" : "DIFFERENT");
        if (! ok) ++failures;
    };
    same ("parameters.txt");
    same ("random_state_values.txt");
    {
        // display texts: the only accepted change is the removal of the negative zero ("-0.00 dB" -> "0.00 dB")
        juce::StringArray ta, tb;
        ta.addLines (a.getChildFile ("parameter_texts.txt").loadFileAsString());
        tb.addLines (b.getChildFile ("parameter_texts.txt").loadFileAsString());
        int changed = 0, bad = ta.size() != tb.size() || ta.isEmpty() ? 1 : 0;
        for (int i = 0; i < juce::jmin (ta.size(), tb.size()); ++i)
            if (ta[i] != tb[i])
            {
                const auto va = ta[i].fromFirstOccurrenceOf ("|", false, false), vb = tb[i].fromFirstOccurrenceOf ("|", false, false);
                const bool negZero = ta[i].upToFirstOccurrenceOf ("|", false, false) == tb[i].upToFirstOccurrenceOf ("|", false, false)
                                  && va == "-" + vb && vb.initialSectionContainingOnly ("0.").removeCharacters ("0.").isEmpty()
                                  && vb.initialSectionContainingOnly ("0.").isNotEmpty();
                std::printf ("    text change: \"%s\" -> \"%s\" %s\n", ta[i].toRawUTF8(), tb[i].toRawUTF8(), negZero ? "(negative zero removed)" : "(UNEXPECTED)");
                ++changed;
                if (! negZero) ++bad;
            }
        std::printf ("%-34s %s\n", "parameter_texts.txt", bad ? "DIFFERENT" : changed ? "identical except the negative-zero fix" : "identical");
        if (bad) ++failures;
    }
    for (const auto& sc : scenarios())
    {
        same (sc.name + ".txt");   // latency + length
        juce::MemoryBlock ma, mb;
        a.getChildFile (sc.name + ".f32").loadFileAsData (ma);
        b.getChildFile (sc.name + ".f32").loadFileAsData (mb);
        const size_t n = juce::jmin (ma.getSize(), mb.getSize()) / sizeof (float);
        const auto* xa = static_cast<const float*> (ma.getData());
        const auto* xb = static_cast<const float*> (mb.getData());
        double maxDiff = 0, sumSq = 0, peak = 0;
        size_t differing = 0;
        for (size_t i = 0; i < n; ++i)
        {
            const double d = std::abs ((double) xa[i] - (double) xb[i]);
            if (std::memcmp (xa + i, xb + i, sizeof (float)) != 0) ++differing;
            maxDiff = juce::jmax (maxDiff, d);
            sumSq += d * d;
            peak = juce::jmax (peak, (double) std::abs (xa[i]));
        }
        const bool identical = ma.getSize() == mb.getSize() && ma.getSize() > 0 && differing == 0;
        std::printf ("%-34s %s  samples %zu  differing %zu  max |diff| %.3g (%.1f dBFS)  output peak %.2f dBFS\n",
                     (sc.name + ".f32").toRawUTF8(), identical ? "BIT-IDENTICAL" : "DIFFERENT", n, differing, maxDiff,
                     20.0 * std::log10 (juce::jmax (maxDiff, 1e-30)), 20.0 * std::log10 (juce::jmax (peak, 1e-30)));
        if (! identical) ++failures;
    }
    std::printf ("\n%s\n", failures == 0 ? "RESULT: outputs, latency, parameters and restored state are identical"
                                         : "RESULT: differences found");
    return failures == 0 ? 0 : 1;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::StringArray args;
    for (int i = 1; i < argc; ++i) args.add (argv[i]);
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    if (args.size() >= 2 && args[0] == "describe")
    {
        juce::AudioPluginFormatManager fm;
        fm.addDefaultFormats();
        juce::PluginDescription d;
        auto p = load (fm, cwd.getChildFile (args[1]).getFullPathName(), 48000.0, 512, &d);
        if (p == nullptr) return 1;
        std::printf ("%s", describe (d).toRawUTF8());
        std::printf ("parameters=%d\nlatency(48k)=%d\n", p->getParameters().size(), (p->prepareToPlay (48000.0, 512), p->getLatencySamples()));
        return 0;
    }
    if (args.size() >= 3 && args[0] == "render")
    {
        juce::File from;
        const int i = args.indexOf ("--state-from");
        if (i > 0 && i + 1 < args.size()) from = cwd.getChildFile (args[i + 1]);
        return render (cwd.getChildFile (args[1]).getFullPathName(), cwd.getChildFile (args[2]), from);
    }
    if (args.size() >= 3 && args[0] == "compare")
        return compare (cwd.getChildFile (args[1]), cwd.getChildFile (args[2]));
    std::printf ("usage:\n  bankmaster_hostcheck describe <plugin>\n  bankmaster_hostcheck render <plugin> <outDir> [--state-from <dir>]\n"
                 "  bankmaster_hostcheck compare <dirA> <dirB>\n");
    return 2;
}
