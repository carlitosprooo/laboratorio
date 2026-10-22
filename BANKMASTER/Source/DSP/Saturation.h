// BANKMASTER by KALOX — DSP core
// MODULE 2: MASTER SATURATION — oversampled harmonic saturation.
#pragma once

#include "Oversampler.h"
#include "Svf.h"

namespace bm
{
enum class SatStyle : int { CleanTube = 0, WarmTube, Tape, SoftClip, NumStyles };

struct SatParams
{
    SatStyle style = SatStyle::CleanTube;
    float drivePct = 0.0f;      // 0..100
    float mixPct = 100.0f;      // 0..100
    float tone = 0.0f;          // -100..+100 (tilt, dark..bright)
    float dynamics = 0.0f;      // -100..+100
    float feedbackPct = 0.0f;   // 0..100
    float feedbackFreq = 2000.0f;
    float outputDb = 0.0f;
    int osFactorLog2 = 2;       // 0..4 (1x..16x)
    int quality = 1;            // 0 normal, 1 high
};

/** Static transfer curves (shared by DSP, tests and the UI display). */
struct Shaper
{
    static inline float curve (SatStyle s, float u) noexcept
    {
        switch (s)
        {
            case SatStyle::CleanTube:
            {
                // Smooth algebraic sigmoid with a small bias -> gentle, mostly 2nd/3rd harmonics.
                constexpr float b = 0.12f;
                auto f = [] (float x) { return x / std::sqrt (1.0f + x * x); };
                const float fb = f (b);
                const float slope0 = 1.0f / ((1.0f + b * b) * std::sqrt (1.0f + b * b));
                return (f (u + b) - fb) / slope0;
            }
            case SatStyle::WarmTube:
            {
                constexpr float b = 0.28f;
                const float tb = std::tanh (b);
                return (std::tanh (u + b) - tb) / (1.0f - tb * tb);
            }
            case SatStyle::Tape:
                return std::tanh (u);
            case SatStyle::SoftClip:
            default:
            {
                const float x = clampv (u, -1.5f, 1.5f);
                return x - (4.0f / 27.0f) * x * x * x;
            }
        }
    }

    /** Gain into the curve: 0 % = -12 dB (curve operates almost linearly for
        program material), 100 % = +24 dB. Output is divided by the same gain,
        so drive changes the amount of saturation, not the level of small signals. */
    static float driveDb (float drivePct) noexcept { return clampv (drivePct, 0.0f, 100.0f) * 0.36f - 12.0f; }
};

class Saturation
{
public:
    void prepare (double sampleRate, int maxBlock, int numChannels)
    {
        fs = sampleRate;
        channels = numChannels;
        maxBlockSize = maxBlock;
        os.prepare (numChannels, maxBlock, Oversampler::kMaxStages);
        dryDelay.prepare (numChannels, 4096);
        for (int c = 0; c < kMaxChannels; ++c) dry[c].assign ((size_t) maxBlock, 0.0f);
        driveSm.reset (fs, 0.05); mixSm.reset (fs, 0.05); outSm.reset (fs, 0.05);
        fbSm.reset (fs, 0.05); dynSm.reset (fs, 0.05);
        envAtt = timeCoeff (5.0f, fs); envRel = timeCoeff (120.0f, fs);
        configured = false;
        applyStructure();
        reset();
    }

    void reset()
    {
        os.reset();
        dryDelay.clear();
        for (int c = 0; c < kMaxChannels; ++c) { fbState[c].reset(); fbPrev[c] = 0; dc[c] = dcIn[c] = 0; env[c] = 0; toneLo[c].reset(); toneHi[c].reset(); }
        driveSm.setCurrentAndTarget (Shaper::driveDb (p.drivePct));
        mixSm.setCurrentAndTarget (p.mixPct * 0.01f);
        outSm.setCurrentAndTarget (dbToGain (p.outputDb));
        fbSm.setCurrentAndTarget (p.feedbackPct * 0.01f);
        dynSm.setCurrentAndTarget (p.dynamics * 0.01f);
    }

    void setParams (const SatParams& np) noexcept
    {
        const bool structural = np.osFactorLog2 != p.osFactorLog2 || np.quality != p.quality;
        p = np;
        p.osFactorLog2 = clampv (p.osFactorLog2, 0, Oversampler::kMaxStages);
        if (structural || ! configured) applyStructure();
        driveSm.setTarget (Shaper::driveDb (p.drivePct));
        mixSm.setTarget (clampv (p.mixPct, 0.0f, 100.0f) * 0.01f);
        outSm.setTarget (dbToGain (p.outputDb));
        fbSm.setTarget (clampv (p.feedbackPct, 0.0f, 100.0f) * 0.01f);
        dynSm.setTarget (clampv (p.dynamics, -100.0f, 100.0f) * 0.01f);

        // tone: tilt around 1 kHz, +-6 dB at the extremes (split between two shelves)
        const double tiltDb = clampv (p.tone, -100.0f, 100.0f) * 0.03;
        toneLoC = SvfCoeffs::design (SectionType::LowShelf, 1000.0, 0.5, -tiltDb, fs);
        toneHiC = SvfCoeffs::design (SectionType::HighShelf, 1000.0, 0.5, tiltDb, fs);
        toneActive = std::abs (tiltDb) > 1e-4;

        const double osRate = fs * os.getFactor();
        fbC = SvfCoeffs::design (SectionType::BandPass, clampv ((double) p.feedbackFreq, 20.0, fs * 0.45), 1.5, 0, osRate);
    }

    int getLatency() const noexcept { return os.getLatency(); }

    void process (float* const* data, int numCh, int n) noexcept
    {
        // keep a latency-aligned dry copy for the mix control
        for (int c = 0; c < numCh; ++c) std::copy (data[c], data[c] + n, dry[c].data());
        float* dp[kMaxChannels] = { dry[0].data(), dry[1].data() };
        dryDelay.processBlock (dp, numCh, n);

        const int F = os.getFactor();
        float* const* up = os.processUp (data, numCh, n);

        // per base-sample control values (smoothed)
        for (int i = 0; i < n; ++i)
        {
            const float dDb = driveSm.next();
            const float fb = fbSm.next() * 0.6f; // keep loop gain < 1
            const float dyn = dynSm.next();
            const float drvStatic = dbToGainFast (dDb);
            for (int c = 0; c < numCh; ++c)
            {
                float* u = up[c] + i * F;
                for (int k = 0; k < F; ++k)
                {
                    const float x = u[k];
                    const float a = std::abs (x);
                    env[c] = a > env[c] ? a + envAtt * (env[c] - a) : a + envRelOs * (env[c] - a);
                    float drv = drvStatic;
                    if (dyn != 0.0f)
                    {
                        // dynamics > 0: loud passages saturate more; < 0: less (relative to -18 dBFS)
                        const float lev = clampv ((gainToDb (env[c], -90.0f) + 18.0f) / 18.0f, -1.0f, 1.0f);
                        drv = dbToGainFast (std::max (-12.0f, dDb + dyn * lev * 12.0f));
                    }
                    const float in = x * drv + fb * fbPrev[c];
                    const float y = Shaper::curve (p.style, in);
                    fbPrev[c] = fbState[c].process (fbC, y);
                    u[k] = y / drv;
                }
            }
        }

        os.processDown (data, numCh, n);

        for (int c = 0; c < numCh; ++c)
        {
            float* x = data[c];
            for (int i = 0; i < n; ++i)
            {
                // DC blocker (asymmetric curves generate DC), ~5 Hz
                const float v = x[i];
                dc[c] = v - dcIn[c] + dcR * dc[c];
                dcIn[c] = v;
                float w = dc[c];
                if (toneActive) w = toneHi[c].process (toneHiC, toneLo[c].process (toneLoC, w));
                x[i] = w;
            }
        }
        for (int i = 0; i < n; ++i)
        {
            const float m = mixSm.next();
            const float g = outSm.next();
            for (int c = 0; c < numCh; ++c)
                data[c][i] = g * (dry[c][(size_t) i] + m * (data[c][i] - dry[c][(size_t) i]));
        }
    }

private:
    static inline float dbToGainFast (float db) noexcept { return std::exp (db * 0.11512925464970228f); }

    void applyStructure() noexcept
    {
        os.configure (p.osFactorLog2, p.quality);
        dryDelay.setDelay (os.getLatency());
        envRelOs = timeCoeff (120.0f, fs * os.getFactor());
        envAtt = timeCoeff (5.0f, fs * os.getFactor());
        dcR = (float) std::exp (-2.0 * kPi * 5.0 / fs);
        configured = true;
    }

    SatParams p;
    double fs = 48000;
    int channels = 2, maxBlockSize = 512;
    Oversampler os;
    DelayLine dryDelay;
    std::vector<float> dry[kMaxChannels];
    LinearSmoother driveSm, mixSm, outSm, fbSm, dynSm;
    SvfCoeffs fbC, toneLoC, toneHiC;
    SvfState fbState[kMaxChannels], toneLo[kMaxChannels], toneHi[kMaxChannels];
    float fbPrev[kMaxChannels] {}, dc[kMaxChannels] {}, dcIn[kMaxChannels] {}, env[kMaxChannels] {};
    float envAtt = 0, envRel = 0, envRelOs = 0, dcR = 0.999f;
    bool toneActive = false, configured = false;
};

} // namespace bm
