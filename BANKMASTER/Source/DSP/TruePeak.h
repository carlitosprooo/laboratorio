// BANKMASTER by KALOX — DSP core
// 4x polyphase interpolating true-peak estimator (ITU-R BS.1770-4 Annex 2 method,
// own Kaiser-windowed-sinc coefficient design).
#pragma once

#include "Common.h"

namespace bm
{
template <int Phases, int Taps, int CutoffPermille = 1000>
class TruePeakInterpolatorT
{
public:
    static constexpr int kTapsPerPhase = Taps;   // window spans +-Taps/2 samples
    static constexpr int kPhases = Phases;
    static constexpr int kLatency = kTapsPerPhase / 2; // samples
    static constexpr int kCentre = kTapsPerPhase / 2 - 1;
    static_assert ((Taps & (Taps - 1)) == 0, "taps per phase must be a power of two");

    TruePeakInterpolatorT() { design(); reset(); }

    void reset() noexcept
    {
        for (int c = 0; c < kMaxChannels; ++c) { std::fill (std::begin (hist[c]), std::end (hist[c]), 0.0f); pos[c] = 0; lastPhase[c] = 0.0f; }
    }

    /** Push one sample; returns the peak magnitude of the interval [m, m+1) where
        m is the sample kLatency samples in the past. */
    inline float process (int ch, float x) noexcept
    {
        float* h = hist[ch];
        int& p = pos[ch];
        h[p] = h[p + kTapsPerPhase] = x;
        p = (p + 1) & (kTapsPerPhase - 1);
        // oldest..newest = h[p .. p+Taps-1]; x[m+j] for j=-(Taps/2-1)..Taps/2 -> index j+kCentre
        const float* w = h + p;
        // interpolated values at m + ph/Phases (ph = 0..Phases, last = x[m+1])
        float v[kPhases + 1];
        v[0] = w[kCentre];
        v[kPhases] = w[kCentre + 1];
        for (int ph = 1; ph < kPhases; ++ph)
        {
            v[ph] = dotProduct (coeffs[ph], w, kTapsPerPhase);
        }
        int imax = 0;
        for (int i = 1; i <= kPhases; ++i) if (std::abs (v[i]) > std::abs (v[imax])) imax = i;
        float peak = std::abs (v[imax]);
        // parabolic refinement of the maximum between the interpolation phases
        const float prev = imax > 0 ? v[imax - 1] : lastPhase[ch];
        if (imax < kPhases)
            peak = std::max (peak, refine (prev, v[imax], v[imax + 1]));
        lastPhase[ch] = v[kPhases - 1];
        return peak;
    }

private:
    static inline float refine (float y0, float y1, float y2) noexcept
    {
        const float sgn = y1 >= 0.0f ? 1.0f : -1.0f;
        y0 *= sgn; y1 *= sgn; y2 *= sgn;
        const float den = y0 - 2.0f * y1 + y2;
        if (! (den < 0.0f) || y0 > y1 || y2 > y1) return y1;
        const float d = 0.5f * (y0 - y2) / den;
        return y1 - 0.25f * (y0 - y2) * d;
    }

    void design() noexcept
    {
        const double beta = Taps >= 64 ? 9.0 : Taps >= 32 ? 8.0 : 7.0;
        auto i0 = [] (double x) { double s = 1, t = 1; for (int k = 1; k < 40; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
        const double half = kTapsPerPhase * 0.5 + 0.5;
        for (int ph = 0; ph < kPhases; ++ph)
        {
            const double tau = ph / (double) kPhases;
            double sum = 0;
            double tmp[kTapsPerPhase];
            for (int j = 0; j < kTapsPerPhase; ++j)
            {
                const double t = (j - kCentre) - tau; // x[m + (j-c)] contributes h(j-c-tau)
                // cut-off relative to Nyquist; > 1 keeps content right up to fs/2 at full
                // level (conservative: slightly over-reads rather than under-reads)
                const double fc = CutoffPermille / 1000.0;
                const double sinc = std::abs (t) < 1e-12 ? fc : std::sin (kPi * fc * t) / (kPi * t);
                const double r = t / half;
                const double win = i0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0 (beta);
                tmp[j] = sinc * win;
                sum += tmp[j];
            }
            for (int j = 0; j < kTapsPerPhase; ++j) coeffs[ph][j] = (float) (tmp[j] / sum);
        }
    }

    float coeffs[kPhases][kTapsPerPhase] {};
    float hist[kMaxChannels][2 * kTapsPerPhase] {};
    int pos[kMaxChannels] {};
    float lastPhase[kMaxChannels] {};
};

/** BS.1770-style 4x estimator (metering). */
using TruePeakInterpolator = TruePeakInterpolatorT<4, 16>;
/** Accurate 8x estimator with parabolic refinement used by the dBTP meters. */
using TruePeakInterpolatorMeter = TruePeakInterpolatorT<8, 64, 1000>;
/** Higher-precision 8x estimator used by the limiter's true-peak safety stage
    (cut-off 3 % above Nyquist: errs on the safe side for content right below fs/2). */
using TruePeakInterpolatorHQ = TruePeakInterpolatorT<8, 64, 1030>;

} // namespace bm
