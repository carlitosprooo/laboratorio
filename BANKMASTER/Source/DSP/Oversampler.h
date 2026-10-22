// BANKMASTER by KALOX — DSP core
// Multi-stage 2x oversampler built from linear-phase half-band FIR filters
// (Kaiser-windowed sinc, designed at prepare time). Latency is always an
// integer number of base-rate samples so it can be reported to the host.
#pragma once

#include "Common.h"
#include <vector>

namespace bm
{
namespace detail
{
    inline double besselI0 (double x) noexcept
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 50; ++k)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
            if (term < 1e-12 * sum) break;
        }
        return sum;
    }

    inline double kaiserBeta (double attenDb) noexcept
    {
        if (attenDb > 50.0) return 0.1102 * (attenDb - 8.7);
        if (attenDb >= 21.0) return 0.5842 * std::pow (attenDb - 21.0, 0.4) + 0.07886 * (attenDb - 21.0);
        return 0.0;
    }

    /** Half-band low-pass (cut-off = rate/4). numTaps = 4m+3. Returns full symmetric kernel. */
    inline std::vector<double> designHalfband (int numTaps, double attenDb)
    {
        std::vector<double> h ((size_t) numTaps, 0.0);
        const int c = (numTaps - 1) / 2;
        const double beta = kaiserBeta (attenDb);
        const double i0b = besselI0 (beta);
        for (int i = 0; i < numTaps; ++i)
        {
            const int k = i - c;
            double s;
            if (k == 0) s = 0.5;
            else if (k % 2 == 0) s = 0.0;
            else s = std::sin (kPi * k * 0.5) / (kPi * k);
            const double r = (double) k / (double) c;
            const double w = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b;
            h[(size_t) i] = s * w;
        }
        return h;
    }

    inline int halfbandTapsFor (double transitionWidthNorm, double attenDb) noexcept
    {
        // Kaiser estimate: N = (A - 7.95) / (14.36 * df) + 1
        int n = (int) std::ceil ((attenDb - 7.95) / (14.36 * transitionWidthNorm) + 1.0);
        n = std::max (n, 7);
        while ((n - 3) % 4 != 0) ++n; // enforce 4m+3
        return n;
    }
}

class HalfbandStage
{
public:
    void prepare (int numTaps, double attenDb, int maxInputBlock)
    {
        const auto h = detail::designHalfband (numTaps, attenDb);
        taps = numTaps;
        centre = (numTaps - 1) / 2;   // odd
        // non-zero even-index coefficients (relative to kernel index)
        evenCoeffs.clear();
        for (int i = 0; i < numTaps; i += 2) evenCoeffs.push_back ((float) h[(size_t) i]);
        nEven = (int) evenCoeffs.size();
        revCoeffs.assign (evenCoeffs.rbegin(), evenCoeffs.rend()); // oldest-first order for dotProduct
        histLen = nEven + 2;
        for (int c = 0; c < kMaxChannels; ++c)
        {
            upHist[c].assign ((size_t) (2 * (histLen + maxInputBlock)), 0.0f);
            dnEven[c].assign ((size_t) (2 * (histLen + maxInputBlock)), 0.0f);
            dnOdd[c].assign ((size_t) (2 * (histLen + maxInputBlock)), 0.0f);
        }
        reset();
    }
    void reset()
    {
        for (int c = 0; c < kMaxChannels; ++c)
        {
            std::fill (upHist[c].begin(), upHist[c].end(), 0.0f);
            std::fill (dnEven[c].begin(), dnEven[c].end(), 0.0f);
            std::fill (dnOdd[c].begin(), dnOdd[c].end(), 0.0f);
        }
        for (int c = 0; c < kMaxChannels; ++c) upPos[c] = dnPos[c] = 0;
    }
    int getCentre() const noexcept { return centre; }

    /** in: n samples, out: 2n samples. Gain-compensated (x2). */
    void upsample (int ch, const float* in, float* out, int n) noexcept
    {
        // y[2n]   = sum_{i even} 2 h[i] x[n - i/2]
        // y[2n+1] = x[n - (centre-1)/2]
        auto& hst = upHist[ch];
        const int half = (centre - 1) / 2;
        const int cap = (int) hst.size() / 2;
        int pos = upPos[ch];
        for (int i = 0; i < n; ++i)
        {
            hst[(size_t) pos] = hst[(size_t) (pos + cap)] = in[i];
            // newest sample at pos; x[n-j] = hst[pos - j] -> use mirrored buffer for contiguous reads
            const float* xp = &hst[(size_t) (pos + cap)];
            out[2 * i] = 2.0f * dotProduct (revCoeffs.data(), xp - (nEven - 1), nEven);
            out[2 * i + 1] = xp[-half];
            pos = (pos + 1 == cap) ? 0 : pos + 1;
        }
        upPos[ch] = pos;
    }

    /** in: 2n samples, out: n samples. */
    void downsample (int ch, const float* in, float* out, int n) noexcept
    {
        // y[n] = h[c] v[2n - c] + sum_{i even} h[i] v[2n - i]
        auto& ev = dnEven[ch];
        auto& od = dnOdd[ch];
        const int cap = (int) ev.size() / 2;
        const int oddIdx = (centre - 1) / 2; // v[2n-c] = odd sample (n - (c+1)/2 + 1)th back
        int pos = dnPos[ch];
        for (int i = 0; i < n; ++i)
        {
            ev[(size_t) pos] = ev[(size_t) (pos + cap)] = in[2 * i];
            od[(size_t) pos] = od[(size_t) (pos + cap)] = in[2 * i + 1];
            const float* ep = &ev[(size_t) (pos + cap)];
            const float* op = &od[(size_t) (pos + cap)];
            float acc = dotProduct (revCoeffs.data(), ep - (nEven - 1), nEven);
            // v[2n - c] with c odd: index 2n - c = 2(n - (c+1)/2) + 1 -> odd sample (c+1)/2 frames back
            acc += 0.5f * op[-(oddIdx + 1)];
            out[i] = acc;
            pos = (pos + 1 == cap) ? 0 : pos + 1;
        }
        dnPos[ch] = pos;
    }

private:
    int taps = 0, centre = 0, nEven = 0, histLen = 0;
    std::vector<float> evenCoeffs, revCoeffs;
    std::vector<float> upHist[kMaxChannels], dnEven[kMaxChannels], dnOdd[kMaxChannels];
    int upPos[kMaxChannels] {}, dnPos[kMaxChannels] {};
};

class Oversampler
{
public:
    static constexpr int kMaxStages = 4; // up to 16x

    /** factorLog2: 0..4. quality 0 = normal, 1 = high. */
    void prepare (int numChannels, int maxBlock, int maxFactorLog2)
    {
        channels = numChannels;
        maxBlockSize = maxBlock;
        maxStages = clampv (maxFactorLog2, 0, kMaxStages);
        for (int s = 0; s < kMaxStages; ++s)
            for (int c = 0; c < kMaxChannels; ++c)
                bufs[s][c].assign ((size_t) (maxBlock << (s + 1)), 0.0f);
        padDelay.prepare (numChannels, 64 << kMaxStages);
        for (int qi = 0; qi < 2; ++qi)
        {
            auto& set = stageSets[qi];
            const double atten = qi ? 110.0 : 85.0;
            for (int s = 0; s < kMaxStages; ++s)
            {
                // stage s (0-based) runs at 2^(s+1) * fs. Passband edge 0.455 fs (stage 0)
                // and 0.5 fs (later stages: content above is already attenuated).
                const double rate = (double) (1 << (s + 1));
                const double fp = (s == 0) ? (qi ? 0.455 : 0.44) : 0.5;
                const double fsb = rate * 0.5 - fp;
                const double df = (fsb - fp) / rate;
                set.stages[s].prepare (detail::halfbandTapsFor (df, atten), atten, maxBlock << s);
            }
        }
        configure (0, 1);
    }

    /** Real-time safe: filters for both qualities are built in prepare(). */
    void configure (int factorLog2, int quality)
    {
        factorLog2 = clampv (factorLog2, 0, maxStages);
        stagesActive = factorLog2;
        q = quality ? 1 : 0;
        auto& set = stageSets[q];
        stages = set.stages;
        // integer latency: total delay at top rate must be a multiple of the factor
        const int F = 1 << stagesActive;
        long topDelay = 0;
        for (int s = 0; s < stagesActive; ++s)
            topDelay += 2L * stages[s].getCentre() * (1L << (stagesActive - 1 - s));
        pad = (int) ((F - (topDelay % F)) % F);
        padDelay.setDelay (pad);
        latency = (int) ((topDelay + pad) / F);
        reset();
    }

    void reset()
    {
        for (auto& set : stageSets) for (auto& s : set.stages) s.reset();
        padDelay.clear();
    }

    int getFactor() const noexcept { return 1 << stagesActive; }
    int getFactorLog2() const noexcept { return stagesActive; }
    int getLatency() const noexcept { return latency; }

    /** Upsamples; returns pointers to the oversampled buffers (length n * factor). */
    float* const* processUp (const float* const* in, int numCh, int n) noexcept
    {
        for (int c = 0; c < numCh; ++c)
        {
            if (stagesActive == 0)
            {
                std::copy (in[c], in[c] + n, bufs[0][c].data());
                outPtr[c] = bufs[0][c].data();
                continue;
            }
            const float* src = in[c];
            int len = n;
            for (int s = 0; s < stagesActive; ++s)
            {
                stages[s].upsample (c, src, bufs[s][c].data(), len);
                src = bufs[s][c].data();
                len *= 2;
            }
            outPtr[c] = bufs[stagesActive - 1][c].data();
        }
        return outPtr;
    }

    /** Downsamples the buffers previously returned by processUp into out. */
    void processDown (float* const* out, int numCh, int n) noexcept
    {
        if (stagesActive == 0)
        {
            for (int c = 0; c < numCh; ++c) std::copy (bufs[0][c].data(), bufs[0][c].data() + n, out[c]);
            return;
        }
        const int top = n << stagesActive;
        if (pad > 0)
        {
            float* p[kMaxChannels] = { outPtr[0], numCh > 1 ? outPtr[1] : nullptr };
            padDelay.processBlock (p, numCh, top);
        }
        for (int c = 0; c < numCh; ++c)
        {
            int len = top;
            for (int s = stagesActive - 1; s >= 0; --s)
            {
                float* src = bufs[s][c].data();
                float* dst = s > 0 ? bufs[s - 1][c].data() : out[c];
                len /= 2;
                stages[s].downsample (c, src, dst, len);
            }
        }
    }

private:
    struct StageSet { HalfbandStage stages[kMaxStages]; };
    StageSet stageSets[2];
    HalfbandStage* stages = stageSets[1].stages;
    std::vector<float> bufs[kMaxStages][kMaxChannels];
    float* outPtr[kMaxChannels] {};
    DelayLine padDelay;
    int channels = 2, maxBlockSize = 512, maxStages = 4, stagesActive = 0, q = 1, pad = 0, latency = 0;
};

} // namespace bm
