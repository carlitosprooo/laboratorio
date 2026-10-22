// BANKMASTER by KALOX — DSP core
// Zero-latency (minimum phase) filter sections based on the trapezoidal-integrated
// state variable filter topology (published by A. Simper / Cytomic, "Linear
// Trapezoidal Integrated SVF"). The topology stays stable under fast parameter
// modulation, which is required for the dynamic EQ bands and click-free automation.
#pragma once

#include "Common.h"
#include <complex>

namespace bm
{
enum class SectionType : int
{
    LowPass, HighPass, BandPass, Notch, Bell, LowShelf, HighShelf,
    LowPass1, HighPass1, LowShelf1, HighShelf1, // first-order sections
    Bypass
};

struct SvfCoeffs
{
    SectionType type = SectionType::Bypass;
    // second order
    float a1 = 0, a2 = 0, a3 = 0, m0 = 1, m1 = 0, m2 = 0;
    // first order (one-pole TPT): G = g/(1+g)
    float G = 0;
    // analysis data (prototype evaluated at s = j*tan(pi f/fs)/g)
    double g = 1, k = 1, A = 1;

    static SvfCoeffs design (SectionType t, double freq, double q, double gainDb, double fs) noexcept
    {
        SvfCoeffs c;
        c.type = t;
        freq = clampv (freq, 1.0, fs * 0.4999);
        q = std::max (q, 0.025);
        const double w = std::tan (kPi * freq / fs);
        double g = w, k = 1.0 / q, A = std::pow (10.0, gainDb / 40.0);
        double m0 = 0, m1 = 0, m2 = 0;

        switch (t)
        {
            case SectionType::LowPass:   m2 = 1; break;
            case SectionType::HighPass:  m0 = 1; m1 = -k; m2 = -1; break;
            case SectionType::BandPass:  m1 = 1; break;               // peak gain 1/k -> use k-normalised below
            case SectionType::Notch:     m0 = 1; m1 = -k; break;
            case SectionType::Bell:      k = 1.0 / (q * A); m0 = 1; m1 = k * (A * A - 1.0); break;
            case SectionType::LowShelf:  g = w / std::sqrt (A); m0 = 1; m1 = k * (A - 1.0); m2 = A * A - 1.0; break;
            case SectionType::HighShelf: g = w * std::sqrt (A); m0 = A * A; m1 = k * (1.0 - A) * A; m2 = 1.0 - A * A; break;
            case SectionType::LowPass1: case SectionType::HighPass1:
                break;
            case SectionType::LowShelf1:  g = w / A; break;    // A^2 = linear gain, pole/zero split
            case SectionType::HighShelf1: g = w * A; break;
            case SectionType::Bypass: break;
        }
        if (t == SectionType::BandPass) m1 = k; // unity gain at centre frequency

        c.g = g; c.k = k; c.A = A;
        c.a1 = (float) (1.0 / (1.0 + g * (g + k)));
        c.a2 = (float) (g * c.a1);
        c.a3 = (float) (g * c.a2);
        c.m0 = (float) m0; c.m1 = (float) m1; c.m2 = (float) m2;
        c.G = (float) (g / (1.0 + g));
        return c;
    }

    /** Exact digital response (the TPT SVF equals the pre-warped bilinear transform). */
    std::complex<double> response (double f, double fs) const noexcept
    {
        using cd = std::complex<double>;
        if (type == SectionType::Bypass) return 1.0;
        const double wf = std::tan (kPi * std::min (f, fs * 0.49999) / fs);
        const cd s (0.0, wf / g);
        switch (type)
        {
            case SectionType::LowPass1:  return 1.0 / (s + 1.0);
            case SectionType::HighPass1: return s / (s + 1.0);
            case SectionType::LowShelf1: { const double G2 = A * A; return (s + G2) / (s + 1.0); }
            case SectionType::HighShelf1:{ const double G2 = A * A; return (G2 * s + 1.0) / (s + 1.0); }
            default: break;
        }
        const cd D = s * s + k * s + 1.0;
        const cd bp = s / D, lp = 1.0 / D;
        return (double) m0 + (double) m1 * bp + (double) m2 * lp;
    }
};

/** One filter section state (per channel). */
struct SvfState
{
    float ic1 = 0, ic2 = 0;
    void reset() noexcept { ic1 = ic2 = 0; }

    inline float process (const SvfCoeffs& c, float v0) noexcept
    {
        switch (c.type)
        {
            case SectionType::Bypass: return v0;
            case SectionType::LowPass1: case SectionType::HighPass1:
            case SectionType::LowShelf1: case SectionType::HighShelf1:
            {
                const float v = (v0 - ic1) * c.G;
                const float lp = v + ic1;
                ic1 = lp + v;
                const float hp = v0 - lp;
                if (c.type == SectionType::LowPass1)  return lp;
                if (c.type == SectionType::HighPass1) return hp;
                const float G2 = (float) (c.A * c.A);
                if (c.type == SectionType::LowShelf1) return hp + G2 * lp;
                return G2 * hp + lp;
            }
            default:
            {
                const float v3 = v0 - ic2;
                const float v1 = c.a1 * ic1 + c.a2 * v3;
                const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
                ic1 = 2.0f * v1 - ic1;
                ic2 = 2.0f * v2 - ic2;
                return c.m0 * v0 + c.m1 * v1 + c.m2 * v2;
            }
        }
    }
};

/** Biquad in transposed direct form II (used by the K-weighting filters). */
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1[kMaxChannels] {}, z2[kMaxChannels] {};
    void reset() noexcept { for (int c = 0; c < kMaxChannels; ++c) z1[c] = z2[c] = 0; }
    inline double process (int ch, double x) noexcept
    {
        const double y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }
};

} // namespace bm
