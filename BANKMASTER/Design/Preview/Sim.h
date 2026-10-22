// BANKMASTER V2 design previews — real data for the mockups.
// Runs the unchanged BANKMASTER DSP (KALOX MASTER - ORIGINAL) on a synthetic
// mix and records exactly what the UI would display: spectra, gain reduction,
// levels, loudness, true peak, saturation harmonics.
#pragma once

#include "MasterChain.h"
#include "FactoryPresets.h"
#include <complex>
#include <random>
#include <vector>

namespace preview
{
constexpr double kPi = 3.14159265358979323846;

inline void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2 * kPi / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

/** Averaged power spectrum (Hann, 50 % overlap), returned in dB per bin. */
inline std::vector<double> averagedSpectrumDb (const std::vector<float>& x, int N)
{
    std::vector<double> acc ((size_t) N / 2 + 1, 0.0);
    int frames = 0;
    std::vector<std::complex<double>> buf ((size_t) N);
    double wsum = 0;
    for (int i = 0; i < N; ++i) wsum += 0.5 - 0.5 * std::cos (2 * kPi * i / (N - 1));
    for (size_t start = 0; start + (size_t) N <= x.size(); start += (size_t) N / 2, ++frames)
    {
        for (int i = 0; i < N; ++i)
            buf[(size_t) i] = x[start + (size_t) i] * (0.5 - 0.5 * std::cos (2 * kPi * i / (N - 1)));
        fft (buf);
        for (size_t k = 0; k < acc.size(); ++k) acc[k] += std::norm (buf[k]);
    }
    for (auto& v : acc) v = 10.0 * std::log10 (std::max (1e-30, 4.0 * v / (std::max (1, frames) * wsum * wsum)));
    return acc;
}

/** A dance-music-like stereo mix: kick, bass, chords, claps, hats, pink-noise air. */
inline void makeMix (double fs, double seconds, std::vector<float>& L, std::vector<float>& R)
{
    const int n = (int) (fs * seconds);
    L.assign ((size_t) n, 0.0f); R.assign ((size_t) n, 0.0f);
    std::mt19937 rng (7);
    std::normal_distribution<double> nd (0, 1);
    const double beat = 60.0 / 124.0;
    const double bassNotes[4] = { 55.0, 55.0, 49.0, 41.2 };
    const double chord[4] = { 220.0, 261.63, 329.63, 392.0 };
    double pk[2][7] {};              // pink noise states
    double hp1[2] {}, hp2[2] {}, hpIn[2] {}, hpIn2[2] {};
    double bpA[2] {}, bpB[2] {};
    double basslp = 0;
    const double bpF = 2 * std::sin (kPi * 1500.0 / fs), bpQ = 1.0 / 3.0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs;
        const double tb = std::fmod (t, beat);
        const int beatIdx = (int) (t / beat);
        const int bar = beatIdx / 4;
        // kick
        const double kick = std::exp (-tb * 9.0) * std::sin (2 * kPi * (47.0 * tb + 110.0 / 35.0 * (1.0 - std::exp (-tb * 35.0))))
                          + 0.25 * std::exp (-tb * 300.0) * nd (rng) * 0.3;
        // bass on the off-beats, ducked by the kick
        const double f0 = bassNotes[bar % 4];
        double saw = 0;
        for (int h = 1; h <= 7; ++h) saw += std::sin (2 * kPi * f0 * h * t) / h;
        basslp = 0.92 * basslp + 0.08 * saw;
        const double duck = 1.0 - std::exp (-tb * 7.0);
        const double bass = 0.55 * basslp * duck * (tb > beat * 0.45 ? 1.0 : 0.6);
        // chords (gentle detune between the channels)
        double padL = 0, padR = 0;
        for (double c : chord)
            for (int h = 1; h <= 8; ++h)
            {
                const double a = 0.028 / std::pow ((double) h, 1.4);
                padL += a * std::sin (2 * kPi * c * h * t * 0.9985);
                padR += a * std::sin (2 * kPi * c * h * t * 1.0015 + 0.7);
            }
        const double padEnv = 0.75 + 0.25 * std::sin (2 * kPi * t / (beat * 8));
        // noise sources
        double out[2];
        const double common = nd (rng);
        for (int c = 0; c < 2; ++c)
        {
            const double w = 0.6 * common + 0.8 * nd (rng);
            auto& b = pk[c];
            b[0] = 0.99886 * b[0] + w * 0.0555179; b[1] = 0.99332 * b[1] + w * 0.0750759;
            b[2] = 0.96900 * b[2] + w * 0.1538520; b[3] = 0.86650 * b[3] + w * 0.3104856;
            b[4] = 0.55000 * b[4] + w * 0.5329522; b[5] = -0.7616 * b[5] - w * 0.0168980;
            const double pink = (b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + w * 0.5362) * 0.11;
            b[6] = w * 0.115926;
            // hats: differentiated noise, 16ths, open hat on the off-beat
            const double x1 = w - hpIn[c]; hpIn[c] = w; hp1[c] = x1;
            const double x2 = hp1[c] - hpIn2[c]; hpIn2[c] = hp1[c]; hp2[c] = x2;
            const double t16 = std::fmod (t, beat / 4), t8 = std::fmod (t + beat / 2, beat);
            const double hats = 0.05 * x2 * (std::exp (-t16 * 55.0) + 1.6 * std::exp (-t8 * 11.0));
            // clap on 2 and 4
            bpA[c] += bpF * bpB[c]; const double hpv = w - bpA[c] - bpQ * bpB[c]; bpB[c] += bpF * hpv;
            const double tc = std::fmod (t + beat, 2 * beat);
            const double clap = 0.9 * bpB[c] * std::exp (-tc * 16.0);
            out[c] = 0.95 * kick + bass + (c == 0 ? padL : padR) * padEnv + 0.12 * pink + hats + clap;
        }
        L[(size_t) i] = (float) out[0];
        R[(size_t) i] = (float) out[1];
    }
}

struct Results
{
    double fs = 48000;
    bm::ChainParams params;
    float inputGainDb = 0, rawInputLufs = 0;
    // histories (one value per block, oldest first)
    int blockSize = 256;
    std::vector<float> inPeakDb, outPeakDb, compGr, limGr, preTapPeakDb, shortTerm;
    // spectra (dB per bin, N = 8192) pre / post EQ
    int fftN = 32768;
    std::vector<double> specPre, specPost;
    float eqDynGain[bm::kNumEqBands] {};
    // final meter readings
    float lufsI = 0, lufsS = 0, lufsM = 0, truePeakDb = 0, corr = 1, outPkL = 0, outPkR = 0, inPkL = 0, inPkR = 0;
    float compGrNow = 0, limGrNow = 0, compGrMax = 0, limGrMax = 0;
    int latency = 0, limOs = 1;
    // saturation: harmonic levels relative to the fundamental (H1 = 0 dB), 1 kHz tone at -6 dBFS
    std::vector<float> harmonicsDb;
    float thdPct = 0;
};

inline Results run()
{
    Results r;
    const double fs = r.fs;
    const double seconds = 14.0;
    std::vector<float> L, R;
    makeMix (fs, seconds, L, R);

    // raw mix at -23.4 LUFS; INPUT LEARN (target -20 LUFS) then sets +3.4 dB
    {
        bm::LoudnessMeter lm;
        lm.prepare (fs);
        for (size_t s = 0; s + 4800 <= L.size(); s += 4800)
        {
            float* d[2] = { L.data() + s, R.data() + s };
            lm.process (d, 2, 4800);
        }
        const float g = (float) std::pow (10.0, (-23.4 - lm.getIntegrated()) / 20.0);
        for (auto& v : L) v *= g;
        for (auto& v : R) v *= g;
        r.rawInputLufs = -23.4f;
        r.inputGainDb = 3.4f;
    }

    r.params = bm::getFactoryPresets()[0].params;
    r.params.inputGainDb = r.inputGainDb;
    bm::MasterChain chain;
    const int B = r.blockSize;
    chain.prepare (fs, B, 2);
    chain.setParams (r.params);
    chain.reset();

    std::vector<float> pre, post;
    const size_t keepFrom = (size_t) (fs * 6.0);
    const float inG = std::pow (10.0f, r.inputGainDb / 20.0f);
    auto toDb = [] (float x) { return 20.0f * std::log10 (std::max (x, 1e-6f)); };
    for (size_t s = 0; s + (size_t) B <= L.size(); s += (size_t) B)
    {
        float inPk = 0;
        for (int i = 0; i < B; ++i) inPk = std::max ({ inPk, std::abs (L[s + (size_t) i]), std::abs (R[s + (size_t) i]) });
        float* d[2] = { L.data() + s, R.data() + s };
        chain.setParams (r.params);
        chain.process (d, 2, B);
        float outPk = 0;
        for (int i = 0; i < B; ++i) outPk = std::max ({ outPk, std::abs (d[0][i]), std::abs (d[1][i]) });
        float tapPk = 0;
        const float* tp = chain.getPreEqTap();
        for (int i = 0; i < chain.getLastChunkSize(); ++i) tapPk = std::max (tapPk, std::abs (tp[i]));
        if (s >= keepFrom)
        {
            r.inPeakDb.push_back (toDb (inPk * inG));
            r.outPeakDb.push_back (toDb (outPk));
            r.compGr.push_back (std::abs (chain.getCompGainReduction()));
            r.limGr.push_back (std::abs (chain.getLimGainReduction()));
            r.preTapPeakDb.push_back (toDb (tapPk));
            r.shortTerm.push_back (chain.getLoudness().getShortTerm());
            const float* a = chain.getPreEqTap();
            const float* b = chain.getPostEqTap();
            pre.insert (pre.end(), a, a + chain.getLastChunkSize());
            post.insert (post.end(), b, b + chain.getLastChunkSize());
        }
    }
    r.specPre = averagedSpectrumDb (pre, r.fftN);
    r.specPost = averagedSpectrumDb (post, r.fftN);
    for (int b = 0; b < bm::kNumEqBands; ++b) r.eqDynGain[b] = chain.getEq().getCurrentGainDb (b);
    r.lufsI = chain.getLoudness().getIntegrated();
    r.lufsS = chain.getLoudness().getShortTerm();
    r.lufsM = chain.getLoudness().getMomentary();
    r.truePeakDb = toDb (chain.getOutputMeter().getTruePeakMax());
    r.corr = chain.getOutputMeter().getCorrelation();
    r.outPkL = toDb (chain.getOutputMeter().getPeak (0));
    r.outPkR = toDb (chain.getOutputMeter().getPeak (1));
    r.inPkL = toDb (chain.getInputMeter().getPeak (0));
    r.inPkR = toDb (chain.getInputMeter().getPeak (1));
    r.compGrNow = std::abs (chain.getCompGainReduction());
    r.limGrNow = std::abs (chain.getLimGainReduction());
    for (float v : r.compGr) r.compGrMax = std::max (r.compGrMax, v);
    for (float v : r.limGr) r.limGrMax = std::max (r.limGrMax, v);
    r.latency = chain.getLatency();
    r.limOs = chain.getLimiterOsFactor();

    // saturation harmonics with the preset's saturation settings
    {
        bm::Saturation sat;
        const int N = 16384, bin = 341;   // 999 Hz, bin-centred
        const double f = bin * fs / N;
        sat.prepare (fs, 512, 1);
        sat.setParams (r.params.sat);
        sat.reset();
        std::vector<float> x ((size_t) N * 3);
        for (size_t i = 0; i < x.size(); ++i) x[i] = (float) (0.5 * std::sin (2 * kPi * f * (double) i / fs));
        for (size_t s = 0; s < x.size(); s += 512)
        {
            float* d[1] = { x.data() + s };
            sat.process (d, 1, 512);
        }
        std::vector<std::complex<double>> buf ((size_t) N);
        for (int i = 0; i < N; ++i)
        {
            const double w = 0.35875 - 0.48829 * std::cos (2 * kPi * i / N) + 0.14128 * std::cos (4 * kPi * i / N) - 0.01168 * std::cos (6 * kPi * i / N);
            buf[(size_t) i] = x[(size_t) (2 * N + i)] * w;
        }
        fft (buf);
        auto mag = [&] (int k) { double m = 0; for (int d = -2; d <= 2; ++d) m = std::max (m, std::abs (buf[(size_t) (k + d)])); return m; };
        const double h1 = mag (bin);
        double harmPow = 0;
        for (int h = 1; h <= 10; ++h)
        {
            const double m = mag (bin * h);
            r.harmonicsDb.push_back ((float) (20.0 * std::log10 (std::max (1e-12, m / h1))));
            if (h > 1) harmPow += (m / h1) * (m / h1);
        }
        r.thdPct = (float) (100.0 * std::sqrt (harmPow));
    }
    return r;
}
} // namespace preview
