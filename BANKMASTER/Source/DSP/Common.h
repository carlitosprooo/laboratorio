// BANKMASTER by KALOX — DSP core
// Shared helpers. This library is framework-free (no JUCE) so it can be unit
// tested and ported (macOS / Windows / Linux) without the plugin wrapper.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
 #include <xmmintrin.h>
 #define BM_HAS_SSE 1
#endif

namespace bm
{
constexpr double kPi = 3.14159265358979323846;
constexpr int kMaxChannels = 2;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline double dbToGainD (double db) noexcept { return std::pow (10.0, db * 0.05); }

inline float gainToDb (float g, float floorDb = -150.0f) noexcept
{
    return g > 0.0f ? std::max (floorDb, 20.0f * std::log10 (g)) : floorDb;
}

template <typename T>
inline T clampv (T v, T lo, T hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }

/** One-pole coefficient for a time constant (time to reach ~63%). */
inline float timeCoeff (float ms, double sampleRate) noexcept
{
    if (ms <= 0.0f) return 0.0f;
    return (float) std::exp (-1.0 / (ms * 0.001 * sampleRate));
}

inline bool isFiniteSample (float x) noexcept { return std::isfinite (x); }

/** Flush denormals to zero for the lifetime of the object (audio thread). */
class ScopedNoDenormals
{
public:
    ScopedNoDenormals() noexcept
    {
       #if BM_HAS_SSE
        prev = _mm_getcsr();
        _mm_setcsr ((unsigned) (prev | 0x8040)); // FTZ | DAZ
       #elif defined(__aarch64__)
        uint64_t fpcr;
        asm volatile ("mrs %0, fpcr" : "=r"(fpcr));
        prev = fpcr;
        asm volatile ("msr fpcr, %0" :: "r"(fpcr | (1ull << 24)));
       #endif
    }
    ~ScopedNoDenormals() noexcept
    {
       #if BM_HAS_SSE
        _mm_setcsr ((unsigned) prev);
       #elif defined(__aarch64__)
        asm volatile ("msr fpcr, %0" :: "r"(prev));
       #endif
    }
private:
    uint64_t prev = 0;
};

/** Linear ramp smoother (click-free parameter changes). */
class LinearSmoother
{
public:
    void reset (double sampleRate, double rampSeconds) noexcept
    {
        steps = std::max (1, (int) std::lround (sampleRate * rampSeconds));
        remaining = 0;
        current = target;
    }
    void setCurrentAndTarget (float v) noexcept { current = target = v; remaining = 0; }
    void setTarget (float v) noexcept
    {
        if (v == target) return;
        target = v;
        remaining = steps;
        delta = (target - current) / (float) steps;
    }
    float next() noexcept
    {
        if (remaining <= 0) return target;
        current += delta;
        if (--remaining == 0) current = target;
        return current;
    }
    void skip (int n) noexcept
    {
        if (remaining <= 0) return;
        if (n >= remaining) { current = target; remaining = 0; return; }
        current += delta * (float) n;
        remaining -= n;
    }
    bool isSmoothing() const noexcept { return remaining > 0; }
    float getCurrent() const noexcept { return remaining > 0 ? current : target; }
    float getTarget() const noexcept { return target; }
private:
    float current = 0.0f, target = 0.0f, delta = 0.0f;
    int steps = 1, remaining = 0;
};

/** Dot product written so compilers auto-vectorise it (8 independent partial sums,
    no -ffast-math needed). Used by every FIR in the plugin. */
inline float dotProduct (const float* a, const float* b, int n) noexcept
{
    float acc[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    int j = 0;
    for (; j + 8 <= n; j += 8)
        for (int k = 0; k < 8; ++k)
            acc[k] += a[j + k] * b[j + k];
    float s = ((acc[0] + acc[1]) + (acc[2] + acc[3])) + ((acc[4] + acc[5]) + (acc[6] + acc[7]));
    for (; j < n; ++j) s += a[j] * b[j];
    return s;
}

/** S-curve (smoothstep) for cross-fades: no slope discontinuity at the fade ends. */
inline float sCurve (float x) noexcept { return x * x * (3.0f - 2.0f * x); }

/** Fixed-capacity multichannel delay line (allocates only in prepare). */
class DelayLine
{
public:
    void prepare (int numChannels, int maxDelay)
    {
        capacity = 1;
        while (capacity < maxDelay + 1) capacity <<= 1;
        mask = capacity - 1;
        for (int c = 0; c < kMaxChannels; ++c)
            buf[c].assign ((size_t) capacity, 0.0f);
        channels = numChannels;
        for (int& w : writePos) w = 0;
    }
    void setDelay (int samples) noexcept { delay = clampv (samples, 0, capacity - 1); }
    int getDelay() const noexcept { return delay; }
    void clear() noexcept
    {
        for (int c = 0; c < kMaxChannels; ++c) std::fill (buf[c].begin(), buf[c].end(), 0.0f);
    }
    /** Push one frame and return delayed sample for channel c (call per channel, then advance()). */
    float process (int c, float x) noexcept
    {
        auto& b = buf[c];
        const int w = writePos[c];
        b[(size_t) w] = x;
        return b[(size_t) ((w - delay) & mask)];
    }
    /** Advance all channels (frame-interleaved use). */
    void advance() noexcept { for (int c = 0; c < kMaxChannels; ++c) writePos[c] = (writePos[c] + 1) & mask; }
    /** Advance one channel (channel-by-channel block use). */
    void advanceChannel (int c) noexcept { writePos[c] = (writePos[c] + 1) & mask; }

    /** Block helper: in-place delay of a block. */
    void processBlock (float* const* data, int numCh, int n) noexcept
    {
        if (delay == 0) return;
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < numCh; ++c)
                data[c][i] = process (c, data[c][i]);
            advance();
        }
    }
private:
    std::vector<float> buf[kMaxChannels];
    int capacity = 1, mask = 0, writePos[kMaxChannels] {}, delay = 0, channels = 2;
};

} // namespace bm
