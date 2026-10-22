// BANKMASTER by KALOX — DSP core
// MODULE 3: MASTER COMPRESSOR — feed-forward log-domain bus compressor.
#pragma once

#include "Svf.h"
#include <atomic>

namespace bm
{
enum class CompStyle : int { Clean = 0, Glue, Punch, NumStyles };

struct CompParams
{
    CompStyle style = CompStyle::Clean;
    float thresholdDb = -18.0f;
    float ratio = 2.0f;
    float kneeDb = 20.0f;
    float attackMs = 30.0f;
    float releaseMs = 100.0f;
    bool autoRelease = false;
    float lookaheadMs = 0.0f;   // 0..20
    float holdMs = 0.0f;        // 0..500
    float rangeDb = 60.0f;      // max gain reduction
    float makeupDb = 0.0f;
    bool autoGain = false;
    float mixPct = 100.0f;
    float scHpFreq = 10.0f;     // <= 10 Hz = off
    float scLpFreq = 22000.0f;  // >= 20 kHz = off
    float stereoLinkPct = 100.0f;
};

struct CompCurve
{
    /** Static gain reduction (positive dB) for an input level (dB). Quadratic soft knee. */
    static inline float gainReductionDb (float x, float T, float R, float W) noexcept
    {
        const float slope = 1.0f - 1.0f / std::max (1.0f, R);
        const float d = x - T;
        if (W > 0.0f && 2.0f * std::abs (d) <= W)
        {
            const float t = d + 0.5f * W;
            return slope * t * t / (2.0f * W);
        }
        return d > 0.0f ? slope * d : 0.0f;
    }

    /** Auto make-up: half of the static reduction a 0 dBFS signal would receive. */
    static inline float autoMakeupDb (float T, float R, float W) noexcept
    {
        return 0.5f * gainReductionDb (0.0f, T, R, W);
    }
};

class Compressor
{
public:
    static constexpr float kMaxLookaheadMs = 20.0f;

    void prepare (double sampleRate, int /*maxBlock*/, int numChannels)
    {
        fs = sampleRate;
        channels = numChannels;
        delay.prepare (numChannels, (int) std::ceil (kMaxLookaheadMs * 0.001 * fs) + 4);
        thrSm.reset (fs, 0.05); ratioSm.reset (fs, 0.05); kneeSm.reset (fs, 0.05);
        makeupSm.reset (fs, 0.05); mixSm.reset (fs, 0.05); rangeSm.reset (fs, 0.05);
        reset();
    }

    void reset()
    {
        delay.clear();
        for (int c = 0; c < kMaxChannels; ++c)
        {
            grState[c] = 0; grSlow[c] = 0; rms[c] = 0; pkEnv[c] = 0; holdCount[c] = 0; fbOut[c] = 0;
            hp[c].reset(); lp[c].reset();
        }
        thrSm.setCurrentAndTarget (p.thresholdDb);
        ratioSm.setCurrentAndTarget (p.ratio);
        kneeSm.setCurrentAndTarget (p.kneeDb);
        rangeSm.setCurrentAndTarget (p.rangeDb);
        makeupSm.setCurrentAndTarget (effectiveMakeup());
        mixSm.setCurrentAndTarget (p.mixPct * 0.01f);
        updateDelay();
    }

    void setParams (const CompParams& np) noexcept
    {
        const bool laChanged = np.lookaheadMs != p.lookaheadMs;
        p = np;
        p.ratio = clampv (p.ratio, 1.0f, 100.0f);
        p.kneeDb = clampv (p.kneeDb, 0.0f, 72.0f);
        thrSm.setTarget (p.thresholdDb);
        ratioSm.setTarget (p.ratio);
        kneeSm.setTarget (p.kneeDb);
        rangeSm.setTarget (clampv (p.rangeDb, 0.0f, 60.0f));
        makeupSm.setTarget (effectiveMakeup());
        mixSm.setTarget (clampv (p.mixPct, 0.0f, 100.0f) * 0.01f);
        if (laChanged) updateDelay();

        const float attackScale = p.style == CompStyle::Punch ? 1.4f : 1.0f;
        att = timeCoeff (std::max (0.005f, p.attackMs * attackScale), fs);
        // the detector's 10 ms peak release is part of the total release time
        rel = timeCoeff (std::max (1.0f, p.releaseMs - 10.0f), fs);
        relSlow = timeCoeff (std::max (1.0f, p.releaseMs * 4.0f), fs);
        relFast = timeCoeff (std::max (1.0f, p.releaseMs * 0.35f), fs);
        rmsCoeff = timeCoeff (p.style == CompStyle::Glue ? 8.0f : 2.0f, fs);
        pkRel = timeCoeff (10.0f, fs);
        holdSamples = (int) std::lround (clampv (p.holdMs, 0.0f, 500.0f) * 0.001 * fs);
        hpOn = p.scHpFreq > 10.5f;
        lpOn = p.scLpFreq < 19999.0f;
        if (hpOn) hpC = SvfCoeffs::design (SectionType::HighPass, p.scHpFreq, 0.7071, 0, fs);
        if (lpOn) lpC = SvfCoeffs::design (SectionType::LowPass, std::min ((double) p.scLpFreq, fs * 0.45), 0.7071, 0, fs);
    }

    int getLatency() const noexcept { return delay.getDelay(); }
    float getGainReductionDb() const noexcept { return grMeter.load (std::memory_order_relaxed); }

    void process (float* const* data, int numCh, int n) noexcept
    {
        float maxGr = 0.0f;
        const float link = clampv (p.stereoLinkPct, 0.0f, 100.0f) * 0.01f;

        for (int i = 0; i < n; ++i)
        {
            const float T = thrSm.next(), R = ratioSm.next(), W = kneeSm.next();
            const float range = rangeSm.next();
            const float makeup = dbToGain (makeupSm.next());
            const float mix = mixSm.next();

            // ---- detector
            float lvl[kMaxChannels] {};
            for (int c = 0; c < numCh; ++c)
            {
                float s = p.style == CompStyle::Punch ? fbOut[c] : data[c][i]; // Punch = feedback topology
                if (hpOn) s = hp[c].process (hpC, s);
                if (lpOn) s = lp[c].process (lpC, s);
                if (p.style == CompStyle::Glue)
                {
                    rms[c] = s * s + rmsCoeff * (rms[c] - s * s);
                    lvl[c] = std::sqrt (std::max (rms[c], 0.0f)) * 1.41421356f;
                }
                else
                {
                    // peak follower (instant attack, short release) removes carrier
                    // ripple so the static curve holds for steady tones
                    const float a = std::abs (s);
                    pkEnv[c] = a > pkEnv[c] ? a : a + pkRel * (pkEnv[c] - a);
                    lvl[c] = pkEnv[c];
                }
            }
            float linked = lvl[0];
            for (int c = 1; c < numCh; ++c) linked = std::max (linked, lvl[c]);

            // ---- gain computer + ballistics (smooth branching in the dB domain)
            float gr[kMaxChannels] {};
            for (int c = 0; c < numCh; ++c)
            {
                const float l = link * linked + (1.0f - link) * lvl[c];
                const float target = std::min (range, CompCurve::gainReductionDb (gainToDb (l, -120.0f), T, R, W));
                float& g = grState[c];
                if (target > g) { g = target + att * (g - target); holdCount[c] = holdSamples; }
                else if (holdCount[c] > 0) { --holdCount[c]; }
                else
                {
                    float r = rel;
                    if (p.autoRelease)
                    {
                        // program dependent: sustained reduction -> slower release,
                        // short transient reduction -> faster release
                        grSlow[c] = g + relSlow * (grSlow[c] - g);
                        const float sustain = clampv (grSlow[c] / std::max (g, 1e-3f), 0.0f, 1.0f);
                        r = relFast + (relSlow - relFast) * sustain;
                    }
                    g = target + r * (g - target);
                }
                gr[c] = g;
            }

            // ---- apply (lookahead delays the audio path only)
            float frame[kMaxChannels];
            for (int c = 0; c < numCh; ++c) frame[c] = delay.process (c, data[c][i]);
            delay.advance();
            for (int c = 0; c < numCh; ++c)
            {
                const float grGain = dbToGain (-gr[c]);
                const float wet = frame[c] * grGain * makeup;
                fbOut[c] = frame[c] * grGain;
                data[c][i] = frame[c] + mix * (wet - frame[c]);
                maxGr = std::max (maxGr, gr[c]);
            }
        }
        grMeter.store (maxGr, std::memory_order_relaxed);
    }

private:
    float effectiveMakeup() const noexcept
    {
        return p.makeupDb + (p.autoGain ? CompCurve::autoMakeupDb (p.thresholdDb, p.ratio, p.kneeDb) : 0.0f);
    }
    void updateDelay() noexcept
    {
        delay.setDelay ((int) std::lround (clampv (p.lookaheadMs, 0.0f, kMaxLookaheadMs) * 0.001 * fs));
    }

    CompParams p;
    double fs = 48000;
    int channels = 2;
    DelayLine delay;
    LinearSmoother thrSm, ratioSm, kneeSm, makeupSm, mixSm, rangeSm;
    float att = 0, rel = 0, relSlow = 0, relFast = 0, rmsCoeff = 0, pkRel = 0;
    int holdSamples = 0;
    float grState[kMaxChannels] {}, grSlow[kMaxChannels] {}, rms[kMaxChannels] {}, fbOut[kMaxChannels] {}, pkEnv[kMaxChannels] {};
    int holdCount[kMaxChannels] {};
    bool hpOn = false, lpOn = false;
    SvfCoeffs hpC, lpC;
    SvfState hp[kMaxChannels], lp[kMaxChannels];
    std::atomic<float> grMeter { 0.0f };
};

} // namespace bm
