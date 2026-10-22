// BANKMASTER by KALOX — offline render + objective comparison tool
//
//   bankmaster_render --list-presets
//   bankmaster_render --in mix.wav --out master.wav [--preset "KALOX MASTER - ORIGINAL"] [--one 120]
//   bankmaster_render --compare reference.wav candidate.wav
//
// --compare measures both files (integrated / max short-term loudness, true peak,
// 1/3-octave spectrum) after latency alignment and prints the differences plus the
// null-test residual. Use it to compare BANKMASTER against the original
// Pro-Q 4 -> Saturn 2 -> Pro-C 2 -> Pro-L 2 chain rendered from the same mix.
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/DSP/FactoryPresets.h"

namespace
{
struct Audio { juce::AudioBuffer<float> buf; double fs = 0; };

bool readFile (const juce::File& f, Audio& a)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr) return false;
    a.fs = r->sampleRate;
    a.buf.setSize ((int) juce::jmin (2u, r->numChannels), (int) r->lengthInSamples);
    r->read (&a.buf, 0, (int) r->lengthInSamples, 0, true, a.buf.getNumChannels() > 1);
    return true;
}

bool writeFile (const juce::File& f, const Audio& a, int bits)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    auto os = f.createOutputStream();
    if (os == nullptr) return false;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), a.fs, (unsigned) a.buf.getNumChannels(), bits, {}, 0));
    if (w == nullptr) return false;
    os.release();
    return w->writeFromAudioSampleBuffer (a.buf, 0, a.buf.getNumSamples());
}

struct Measures { float integrated = -INFINITY, maxShort = -INFINITY, truePeak = -INFINITY; std::vector<double> bands; };

Measures measure (const Audio& a)
{
    Measures m;
    bm::LoudnessMeter lm; lm.prepare (a.fs);
    bm::LevelMeter lv; lv.prepare (a.fs);
    const int n = a.buf.getNumSamples(), ch = a.buf.getNumChannels();
    const int hop = (int) (a.fs * 0.1);
    for (int s = 0; s < n; s += hop)
    {
        const int len = juce::jmin (hop, n - s);
        const float* d[2] = { a.buf.getReadPointer (0, s), a.buf.getReadPointer (ch > 1 ? 1 : 0, s) };
        lm.process (d, ch, len);
        lv.process (d, ch, len);
        m.maxShort = juce::jmax (m.maxShort, lm.getShortTerm());
    }
    m.integrated = lm.getIntegrated();
    m.truePeak = juce::Decibels::gainToDecibels (lv.getTruePeakMax(), -100.0f);
    // 1/3-octave band energies (31.5 Hz .. 16 kHz) via 4th-order band-pass pairs
    for (int k = -16; k <= 13; ++k)
    {
        const double fc = 1000.0 * std::pow (2.0, k / 3.0);
        if (fc > a.fs * 0.45) break;
        auto c = bm::SvfCoeffs::design (bm::SectionType::BandPass, fc, 4.32, 0, a.fs);
        double e = 0;
        for (int cc = 0; cc < ch; ++cc)
        {
            bm::SvfState s1, s2;
            const float* x = a.buf.getReadPointer (cc);
            for (int i = 0; i < n; ++i) { const float y = s2.process (c, s1.process (c, x[i])); e += (double) y * y; }
        }
        m.bands.push_back (10.0 * std::log10 (e / std::max (1, n * ch) + 1e-20));
    }
    return m;
}

int bestLag (const Audio& a, const Audio& b, int maxLag)
{
    // coarse cross-correlation on the first channel (decimated search, then refine)
    const float* x = a.buf.getReadPointer (0);
    const float* y = b.buf.getReadPointer (0);
    const int n = juce::jmin (a.buf.getNumSamples(), b.buf.getNumSamples(), (int) (a.fs * 20));
    double best = -1e30; int lag = 0;
    for (int l = -maxLag; l <= maxLag; ++l)
    {
        double s = 0;
        for (int i = juce::jmax (0, -l); i < n - juce::jmax (0, l); i += 7) s += (double) x[i] * y[i + l];
        if (s > best) { best = s; lag = l; }
    }
    return lag;
}

int render (const juce::File& in, const juce::File& out, const juce::String& presetName, float onePct)
{
    Audio a;
    if (! readFile (in, a)) { std::printf ("cannot read %s\n", in.getFullPathName().toRawUTF8()); return 1; }
    const bm::FactoryPreset* preset = nullptr;
    for (const auto& p : bm::getFactoryPresets()) if (presetName == juce::String (p.name)) preset = &p;
    if (preset == nullptr) { std::printf ("unknown preset '%s' (use --list-presets)\n", presetName.toRawUTF8()); return 1; }
    auto params = preset->params;
    if (onePct >= 0) { params.oneMasterOn = true; params.oneMasterPct = onePct; }

    const int ch = juce::jmax (1, a.buf.getNumChannels());
    bm::MasterChain chain;
    chain.prepare (a.fs, 512, ch);
    chain.setParams (params);
    chain.reset();
    const int lat = chain.getLatency();
    const int n = a.buf.getNumSamples();
    juce::AudioBuffer<float> outBuf (ch, n + lat);
    outBuf.clear();
    for (int c = 0; c < ch; ++c) outBuf.copyFrom (c, 0, a.buf, c, 0, n);
    for (int s = 0; s < n + lat; s += 512)
    {
        const int len = juce::jmin (512, n + lat - s);
        float* d[2] = { outBuf.getWritePointer (0, s), outBuf.getWritePointer (ch > 1 ? 1 : 0, s) };
        chain.process (d, ch, len);
    }
    Audio o; o.fs = a.fs; o.buf.setSize (ch, n);
    for (int c = 0; c < ch; ++c) o.buf.copyFrom (c, 0, outBuf, c, lat, n); // latency removed
    if (! writeFile (out, o, 24)) { std::printf ("cannot write %s\n", out.getFullPathName().toRawUTF8()); return 1; }
    const auto mi = measure (a), mo = measure (o);
    std::printf ("preset   : %s%s\n", preset->name.c_str(), onePct >= 0 ? (" (ONE MASTER " + juce::String (onePct, 0) + "%)").toRawUTF8() : "");
    std::printf ("input    : %.2f LUFS-I, %.2f dBTP\n", mi.integrated, mi.truePeak);
    std::printf ("output   : %.2f LUFS-I, max short-term %.2f LUFS, %.2f dBTP\n", mo.integrated, mo.maxShort, mo.truePeak);
    std::printf ("latency  : %d samples (removed from the file)\n", lat);
    return 0;
}

int compare (const juce::File& ref, const juce::File& cand)
{
    Audio a, b;
    if (! readFile (ref, a) || ! readFile (cand, b)) { std::printf ("cannot read inputs\n"); return 1; }
    if (a.fs != b.fs) { std::printf ("sample rates differ (%.0f vs %.0f)\n", a.fs, b.fs); return 1; }
    const auto ma = measure (a), mb = measure (b);
    std::printf ("                 reference   candidate   difference\n");
    std::printf ("integrated LUFS  %9.2f   %9.2f   %+9.2f\n", ma.integrated, mb.integrated, mb.integrated - ma.integrated);
    std::printf ("max short-term   %9.2f   %9.2f   %+9.2f\n", ma.maxShort, mb.maxShort, mb.maxShort - ma.maxShort);
    std::printf ("true peak dBTP   %9.2f   %9.2f   %+9.2f\n", ma.truePeak, mb.truePeak, mb.truePeak - ma.truePeak);
    std::printf ("\n1/3-octave balance (candidate - reference, level-matched):\n");
    const double off = mb.integrated - ma.integrated;
    double worst = 0;
    for (size_t k = 0; k < juce::jmin (ma.bands.size(), mb.bands.size()); ++k)
    {
        const double fc = 1000.0 * std::pow (2.0, ((int) k - 16) / 3.0);
        const double d = mb.bands[k] - ma.bands[k] - off;
        worst = juce::jmax (worst, std::abs (d));
        std::printf ("  %8.1f Hz  %+6.2f dB  %s\n", fc, d, juce::String::repeatedString ("#", juce::jmin (40, (int) std::abs (d * 4))).toRawUTF8());
    }
    const int lag = bestLag (a, b, (int) (a.fs * 0.05));
    double num = 0, den = 0;
    const float g = std::pow (10.0f, (float) -off / 20.0f);
    const int n = juce::jmin (a.buf.getNumSamples(), b.buf.getNumSamples()) - std::abs (lag);
    for (int c = 0; c < juce::jmin (a.buf.getNumChannels(), b.buf.getNumChannels()); ++c)
        for (int i = juce::jmax (0, -lag); i < n; ++i)
        {
            const double r = a.buf.getSample (c, i), d = b.buf.getSample (c, i + lag) * g - r;
            num += d * d; den += r * r;
        }
    std::printf ("\nalignment lag %d samples; worst band deviation %.2f dB; null residual %.1f dB\n", lag, worst, 10 * std::log10 (num / (den + 1e-30) + 1e-30));
    return 0;
}
} // namespace

int main (int argc, char** argv)
{
    juce::StringArray args; for (int i = 1; i < argc; ++i) args.add (argv[i]);
    auto value = [&] (const juce::String& key) { const int i = args.indexOf (key); return i >= 0 && i + 1 < args.size() ? args[i + 1] : juce::String(); };

    if (args.contains ("--list-presets"))
    {
        for (const auto& p : bm::getFactoryPresets()) std::printf ("%s\n", p.name.c_str());
        return 0;
    }
    if (args.contains ("--compare") && args.size() >= 3)
    {
        const int i = args.indexOf ("--compare");
        return compare (juce::File::getCurrentWorkingDirectory().getChildFile (args[i + 1]),
                        juce::File::getCurrentWorkingDirectory().getChildFile (args[i + 2]));
    }
    if (value ("--in").isNotEmpty() && value ("--out").isNotEmpty())
    {
        const auto preset = value ("--preset").isNotEmpty() ? value ("--preset") : juce::String (bm::getFactoryPresets().front().name);
        const float one = value ("--one").isNotEmpty() ? value ("--one").getFloatValue() : -1.0f;
        return render (juce::File::getCurrentWorkingDirectory().getChildFile (value ("--in")),
                       juce::File::getCurrentWorkingDirectory().getChildFile (value ("--out")), preset, one);
    }
    std::printf ("usage:\n  bankmaster_render --list-presets\n"
                 "  bankmaster_render --in mix.wav --out master.wav [--preset NAME] [--one PCT]\n"
                 "  bankmaster_render --compare reference.wav candidate.wav\n");
    return 2;
}
