// BANKMASTER by KALOX — DSP core
// MODULE 4: MASTER LIMITER — oversampled lookahead true-peak limiter.
//
// Algorithm (own design):
//   1. input drive (dB) -> oversampling (1x..16x, linear-phase FIR)
//   2. required gain r[n] = min(1, ceiling / |x[n]|) at the oversampled rate
//   3. SLOW stage: program-dependent gain riding (Attack / Release controls) that
//      absorbs sustained overs so the fast stage works less (lower distortion)
//   4. FAST stage on the residual: sliding-window minimum over the lookahead,
//      exponential release, then a moving average of the same length. The
//      min-hold + average guarantees gain <= requirement at the delayed sample.
//   5. downsample -> TRUE-PEAK SAFETY stage at base rate (4x interpolated
//      detector + short lookahead) -> final sample clamp at the ceiling.
#pragma once

#include "Oversampler.h"
#include "TruePeak.h"
#include <atomic>
#include <random>

namespace bm
{
enum class LimStyle : int { Transparent = 0, Punchy, Dynamic, Safe, NumStyles };

struct LimParams
{
    LimStyle style = LimStyle::Transparent;
    float inputGainDb = 0.0f;   // drive
    float ceilingDb = -0.1f;    // dBTP when truePeak is on, dBFS otherwise
    float attackMs = 100.0f;    // slow stage attack
    float releaseMs = 400.0f;
    float lookaheadMs = 1.0f;   // 0..5
    bool truePeak = true;
    int osFactorLog2 = 4;       // 0..4 -> 1x..16x
    float linkTransientsPct = 0.0f;
    float linkReleasePct = 0.0f;
    int dither = 0;             // 0 off, 1 = 16 bit, 2 = 24 bit (TPDF)
};

/** Sliding-window minimum (monotonic deque), fixed capacity. */
class SlidingMin
{
public:
    void prepare (int maxWindow) { cap = 1; while (cap < maxWindow + 2) cap <<= 1; val.assign ((size_t) cap, 1.0f); idx.assign ((size_t) cap, 0); reset(); }
    void setWindow (int w) noexcept { window = std::max (1, w); }
    void reset() noexcept { head = tail = 0; t = 0; }
    inline float push (float v) noexcept
    {
        const int m = cap - 1;
        while (tail != head && val[(size_t) ((tail - 1) & m)] >= v) --tail;
        val[(size_t) (tail & m)] = v; idx[(size_t) (tail & m)] = t; ++tail;
        while (idx[(size_t) (head & m)] <= t - window) ++head;
        ++t;
        return val[(size_t) (head & m)];
    }
private:
    std::vector<float> val; std::vector<long long> idx;
    int cap = 1, window = 1; long long head = 0, tail = 0, t = 0;
};

/** Moving average with exact periodic re-summation (no drift). */
class MovingAverage
{
public:
    void prepare (int maxLen) { buf.assign ((size_t) std::max (1, maxLen), 1.0f); reset(); }
    void setLength (int l) noexcept { len = clampv (l, 1, (int) buf.size()); reset(); }
    void reset() noexcept { std::fill (buf.begin(), buf.begin() + len, 1.0f); sum = len; pos = 0; }
    inline float push (float v) noexcept
    {
        sum += v - buf[(size_t) pos];
        buf[(size_t) pos] = v;
        if (++pos == len)
        {
            pos = 0;
            double s = 0; for (int i = 0; i < len; ++i) s += buf[(size_t) i];
            sum = s;
        }
        return (float) (sum / len);
    }
private:
    std::vector<float> buf; double sum = 1; int len = 1, pos = 0;
};

/** Linear-phase FIR low-pass that removes the last few kHz below Nyquist before the
    true-peak safety stage (finite-length interpolators under-read energy right below
    fs/2). Passband 0.44 fs (21.1 kHz @ 48k, 19.4 kHz @ 44.1k), > 60 dB at 0.49 fs. */
class GuardLowpass
{
public:
    static constexpr int kTaps = 95;           // odd, symmetric
    static constexpr int kLatency = kTaps / 2;
    GuardLowpass()
    {
        const double fc = 0.4675, beta = 5.65;  // cut-off (x fs) between 0.44 and 0.495
        const int c = kTaps / 2;
        double sum = 0;
        for (int i = 0; i < kTaps; ++i)
        {
            const double k = i - c;
            const double s = k == 0 ? 2 * fc : std::sin (2 * kPi * fc * k) / (kPi * k);
            const double r = k / (double) c;
            h[i] = s * detail::besselI0 (beta * std::sqrt (std::max (0.0, 1 - r * r))) / detail::besselI0 (beta);
            sum += h[i];
        }
        for (double& v : h) v /= sum;
        for (int i = 0; i < kTaps; ++i) hf[i] = (float) h[i];
        reset();
    }
    void reset() noexcept { for (auto& b : buf) std::fill (std::begin (b), std::end (b), 0.0f); for (int& p : pos) p = 0; }
    inline float process (int ch, float x) noexcept
    {
        float* b = buf[ch];
        int& p = pos[ch];
        b[p] = b[p + kTaps] = x;
        p = p + 1 == kTaps ? 0 : p + 1;
        const float* w = b + p; // oldest .. newest
        return dotProduct (hf, w, kTaps);
    }
private:
    double h[kTaps] {};
    float hf[kTaps] {};
    float buf[kMaxChannels][2 * kTaps] {};
    int pos[kMaxChannels] {};
};

class Limiter
{
public:
    static constexpr float kMaxLookaheadMs = 5.0f;
    static constexpr int kSafetyWindow = 48;      // base-rate samples (1 ms @ 48 kHz)
    static constexpr int kSafetyGuardK = 4;       // full reduction also on +-K neighbours of a peak
    static constexpr double kMaxOsRate = 768000.0;

    void prepare (double sampleRate, int maxBlock, int numChannels)
    {
        fs = sampleRate;
        channels = numChannels;
        maxOsLog2 = 0;
        while (maxOsLog2 < Oversampler::kMaxStages && fs * (1 << (maxOsLog2 + 1)) <= kMaxOsRate + 1.0) ++maxOsLog2;
        os.prepare (numChannels, maxBlock, maxOsLog2);
        const int maxLa = (int) std::ceil (kMaxLookaheadMs * 0.001 * fs) << maxOsLog2;
        for (int c = 0; c < kMaxChannels; ++c)
        {
            fastMin[c].prepare (maxLa + 2);
            fastAvg[c].prepare (maxLa + 2);
            safeMin[c].prepare (kSafetyWindow + 2 * kSafetyGuardK + 2);
            safeAvg[c].prepare (kSafetyWindow + 2);
        }
        audioDelay.prepare (numChannels, maxLa + TruePeakInterpolatorHQ::kLatency + 2);
        for (auto& r : reqBuf) r.assign ((size_t) (maxBlock << maxOsLog2), 1.0f);
        slowDelay.prepare (numChannels, maxLa + 2);
        safeDelay.prepare (numChannels, kSafetyWindow + kSafetyGuardK + TruePeakInterpolatorHQ::kLatency + 4);
        driveSm.reset (fs, 0.05); ceilSm.reset (fs, 0.05);
        configured = false;
        applyStructure();
        reset();
    }

    void reset()
    {
        os.reset();
        audioDelay.clear(); slowDelay.clear(); safeDelay.clear();
        tpi.reset(); tpiMain.reset(); guard.reset();
        for (int c = 0; c < kMaxChannels; ++c)
        {
            fastMin[c].reset(); fastAvg[c].reset(); safeMin[c].reset(); safeAvg[c].reset();
            slow[c] = 1.0f; fastRel[c] = 1.0f; safeRel[c] = 1.0f; prevPeak[c] = 0.0f;
        }
        driveSm.setCurrentAndTarget (dbToGain (p.inputGainDb));
        ceilSm.setCurrentAndTarget (dbToGain (p.ceilingDb));
    }

    void setParams (const LimParams& np) noexcept
    {
        const int laSamples = (int) std::lround (clampv (np.lookaheadMs, 0.0f, kMaxLookaheadMs) * 0.001 * fs);
        const bool structural = np.osFactorLog2 != p.osFactorLog2 || laSamples != lookBase || np.truePeak != p.truePeak;
        p = np;
        p.ceilingDb = clampv (p.ceilingDb, -24.0f, 0.0f);
        lookBase = laSamples;
        if (structural || ! configured) applyStructure();

        driveSm.setTarget (dbToGain (clampv (p.inputGainDb, -24.0f, 36.0f)));
        ceilSm.setTarget (dbToGain (p.ceilingDb));

        // style constants
        float slowAmount = 0.8f, fastRelFrac = 0.06f, fastRelMin = 6.0f;
        switch (p.style)
        {
            case LimStyle::Transparent: slowAmount = 0.85f; fastRelFrac = 0.06f; fastRelMin = 8.0f; break;
            case LimStyle::Punchy:      slowAmount = 0.45f; fastRelFrac = 0.03f; fastRelMin = 4.0f; break;
            case LimStyle::Dynamic:     slowAmount = 0.6f;  fastRelFrac = 0.02f; fastRelMin = 2.5f; break;
            case LimStyle::Safe:        slowAmount = 1.0f;  fastRelFrac = 0.12f; fastRelMin = 15.0f; break;
            default: break;
        }
        slowAmt = slowAmount;
        const double osRate = fs * os.getFactor();
        slowAtt = timeCoeff (std::max (0.1f, p.attackMs), osRate);
        slowRel = timeCoeff (std::max (1.0f, p.releaseMs), osRate);
        fastRelC = timeCoeff (std::max (fastRelMin, p.releaseMs * fastRelFrac), osRate);
        safeRelC = timeCoeff (30.0f, fs);
        linkT = clampv (p.linkTransientsPct, 0.0f, 100.0f) * 0.01f;
        linkR = clampv (p.linkReleasePct, 0.0f, 100.0f) * 0.01f;
        ditherAmp = p.dither == 1 ? 1.0f / 32768.0f : p.dither == 2 ? 1.0f / 8388608.0f : 0.0f;
    }

    int getLatency() const noexcept { return latency; }
    int getEffectiveOsFactor() const noexcept { return os.getFactor(); }
    float getGainReductionDb() const noexcept { return grMeter.load (std::memory_order_relaxed); }
    long long getSafetyClampCount() const noexcept { return clampCount; }

    void process (float* const* data, int numCh, int n) noexcept
    {
        // 1. drive
        for (int i = 0; i < n; ++i)
        {
            const float g = driveSm.next();
            for (int c = 0; c < numCh; ++c) data[c][i] *= g;
        }
        const float ceil = ceilSm.getTarget();
        ceilSm.skip (n);
        // the oversampled stage limits slightly below the ceiling so the safety
        // stage only has to catch reconstruction overshoot
        const float ceilMain = ceil * 0.995f;
        // tiny margin for the residual error of the interpolated estimate
        const float ceilSafe = p.truePeak ? ceil * 0.997f : ceil;

        // 2..4 oversampled limiting
        const int F = os.getFactor();
        float* const* up = os.processUp (data, numCh, n);
        const int N = n * F;
        float minGain = 1.0f;
        // pass 1: required gain per channel (+ linked minimum)
        for (int c = 0; c < numCh; ++c)
        {
            float* r = reqBuf[c].data();
            const float* x = up[c];
            if (mainTp)
                for (int k = 0; k < N; ++k) { const float a = tpiMain.process (c, x[k]); r[k] = a > ceilMain ? ceilMain / a : 1.0f; }
            else
                for (int k = 0; k < N; ++k) { const float a = std::abs (x[k]); r[k] = a > ceilMain ? ceilMain / a : 1.0f; }
        }
        float* linked = reqBuf[2].data();
        if (numCh > 1) for (int k = 0; k < N; ++k) linked[k] = std::min (reqBuf[0][(size_t) k], reqBuf[1][(size_t) k]);
        else std::copy (reqBuf[0].begin(), reqBuf[0].begin() + N, linked);

        // pass 2: slow stage + fast stage + gain application, channel by channel
        for (int c = 0; c < numCh; ++c)
        {
            const float* r = reqBuf[c].data();
            float* x = up[c];
            float s = slow[c], fr = fastRel[c];
            auto& fmin = fastMin[c];
            auto& favg = fastAvg[c];
            for (int k = 0; k < N; ++k)
            {
                // slow stage (release linking)
                const float rr = linkR * linked[k] + (1.0f - linkR) * r[k];
                const float target = 1.0f - slowAmt * (1.0f - rr);
                s = target < s ? target + slowAtt * (s - target) : target + slowRel * (s - target);
                // fast stage on the residual (transient linking)
                const float rt = linkT * linked[k] + (1.0f - linkT) * r[k];
                const float resid = rt < s ? rt / s : 1.0f;
                const float held = fmin.push (resid);
                fr = std::min (held, 1.0f - (1.0f - fr) * fastRelC);
                const float fg = favg.push (fr);
                const float sDel = slowDelay.process (c, s);
                const float xd = audioDelay.process (c, x[k]);
                const float g = fg * sDel;
                x[k] = xd * g;
                minGain = std::min (minGain, g);
                slowDelay.advanceChannel (c);
                audioDelay.advanceChannel (c);
            }
            slow[c] = s; fastRel[c] = fr;
        }
        os.processDown (data, numCh, n);

        // 5. true-peak safety stage + final clamp
        for (int i = 0; i < n; ++i)
        {
            float sreq[kMaxChannels] { 1.0f, 1.0f };
            for (int c = 0; c < numCh; ++c)
            {
                if (p.truePeak) data[c][i] = guard.process (c, data[c][i]);
                const float x = data[c][i];
                float pk;
                if (p.truePeak)
                {
                    const float e = tpi.process (c, x);
                    pk = std::max (e, prevPeak[c]);
                    prevPeak[c] = e;
                }
                else pk = std::abs (x);
                sreq[c] = pk > ceilSafe ? ceilSafe / pk : 1.0f;
            }
            if (numCh > 1) { const float m = std::min (sreq[0], sreq[1]); sreq[0] = sreq[1] = m; }
            for (int c = 0; c < numCh; ++c)
            {
                const float held = safeMin[c].push (sreq[c]);
                float& fr = safeRel[c];
                fr = std::min (held, 1.0f - (1.0f - fr) * safeRelC);
                const float g = safeAvg[c].push (fr);
                float y = safeDelay.process (c, data[c][i]) * g;
                if (ditherAmp > 0.0f) y += ditherAmp * (rnd() - rnd());
                if (std::abs (y) > ceil) { y = y > 0 ? ceil : -ceil; ++clampCount; }
                data[c][i] = y;
                minGain = std::min (minGain, g * 1.0f);
            }
            safeDelay.advance();
        }
        grMeter.store (-gainToDb (std::max (minGain, 1e-6f)), std::memory_order_relaxed);
    }

private:
    float rnd() noexcept { rngState = rngState * 1664525u + 1013904223u; return (float) (rngState >> 8) * (1.0f / 16777216.0f); }

    void applyStructure() noexcept
    {
        const int osLog2 = clampv (p.osFactorLog2, 0, maxOsLog2);
        os.configure (osLog2, 1);
        const int F = os.getFactor();
        const int D = lookBase * F;            // audio delay at OS rate (multiple of F)
        for (int c = 0; c < kMaxChannels; ++c)
        {
            fastMin[c].setWindow (D + 1);
            fastAvg[c].setLength (D + 1);
            fastMin[c].reset();
            // hold = avg + 2K and delay = avg - 1 + K: the interpolation neighbourhood
            // (+-K samples) of every peak receives the full reduction
            safeMin[c].setWindow (kSafetyWindow + 1 + 2 * kSafetyGuardK);
            safeAvg[c].setLength (kSafetyWindow + 1);
            safeMin[c].reset();
        }
        mainTp = p.truePeak && F == 1;
        const int mainTpLat = mainTp ? TruePeakInterpolatorHQ::kLatency : 0;
        audioDelay.setDelay (D + mainTpLat);
        slowDelay.setDelay (D);
        tpiMain.reset();
        const int tpLat = p.truePeak ? TruePeakInterpolatorHQ::kLatency : 0;
        const int guardLat = p.truePeak ? GuardLowpass::kLatency : 0;
        guard.reset();
        safeDelay.setDelay (kSafetyWindow + kSafetyGuardK + tpLat);
        latency = lookBase + mainTpLat + os.getLatency() + guardLat + kSafetyWindow + kSafetyGuardK + tpLat;
        configured = true;
    }

    LimParams p;
    double fs = 48000;
    int channels = 2, maxOsLog2 = 4, lookBase = -1, latency = 0;
    Oversampler os;
    TruePeakInterpolatorHQ tpi, tpiMain;
    GuardLowpass guard;
    bool mainTp = false;
    SlidingMin fastMin[kMaxChannels], safeMin[kMaxChannels];
    MovingAverage fastAvg[kMaxChannels], safeAvg[kMaxChannels];
    DelayLine audioDelay, slowDelay, safeDelay;
    std::vector<float> reqBuf[3];
    LinearSmoother driveSm, ceilSm;
    float slow[kMaxChannels] {}, fastRel[kMaxChannels] {}, safeRel[kMaxChannels] {}, prevPeak[kMaxChannels] {};
    float slowAmt = 0.8f, slowAtt = 0, slowRel = 0, fastRelC = 0, safeRelC = 0, linkT = 0, linkR = 0, ditherAmp = 0;
    bool configured = false;
    uint32_t rngState = 22222u;
    long long clampCount = 0;
    std::atomic<float> grMeter { 0.0f };
};

} // namespace bm
