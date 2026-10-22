// BANKMASTER by KALOX — DSP core
// Metering: ITU-R BS.1770-4 / EBU R128 loudness (momentary, short-term,
// integrated with gating), true peak (dBTP), peak/RMS levels and stereo correlation.
// All buffers are fixed-size: safe to run on the audio thread.
#pragma once

#include "Svf.h"
#include "TruePeak.h"
#include <array>
#include <atomic>

namespace bm
{
class LoudnessMeter
{
public:
    static constexpr int kSubblocksShort = 30;   // 3 s of 100 ms sub-blocks
    static constexpr double kHistLo = -70.0, kHistHi = 10.0, kHistStep = 0.01;
    static constexpr int kHistBins = (int) ((kHistHi - kHistLo) / kHistStep) + 1;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        designKWeighting (fs, pre, rlb);
        hop = (int) std::lround (fs * 0.1);
        histCount.assign ((size_t) kHistBins, 0);
        histSum.assign ((size_t) kHistBins, 0.0);
        reset();
    }

    void reset()
    {
        pre.reset(); rlb.reset();
        acc[0] = acc[1] = 0; accN = 0;
        sub.fill (0.0); subCount = 0; subPos = 0;
        std::fill (histCount.begin(), histCount.end(), 0);
        std::fill (histSum.begin(), histSum.end(), 0.0);
        momentary.store (-INFINITY); shortTerm.store (-INFINITY); integrated.store (-INFINITY);
    }

    /** Standard K-weighting (pre-filter shelf + RLB high-pass) for any sample rate. */
    static void designKWeighting (double fs, Biquad& shelf, Biquad& hp)
    {
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (kPi * f0 / fs);
            const double Vh = std::pow (10.0, G / 20.0);
            const double Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            shelf.b0 = (Vh + Vb * K / Q + K * K) / a0;
            shelf.b1 = 2.0 * (K * K - Vh) / a0;
            shelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
            shelf.a1 = 2.0 * (K * K - 1.0) / a0;
            shelf.a2 = (1.0 - K / Q + K * K) / a0;
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (kPi * f0 / fs);
            const double a0 = 1.0 + K / Q + K * K;
            hp.b0 = 1.0; hp.b1 = -2.0; hp.b2 = 1.0;
            hp.a1 = 2.0 * (K * K - 1.0) / a0;
            hp.a2 = (1.0 - K / Q + K * K) / a0;
        }
    }

    void process (const float* const* data, int numCh, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < numCh; ++c)
            {
                const double y = rlb.process (c, pre.process (c, (double) data[c][i]));
                acc[c] += y * y;
            }
            if (++accN == hop) closeSubblock (numCh);
        }
    }

    float getMomentary() const noexcept { return momentary.load (std::memory_order_relaxed); }
    float getShortTerm() const noexcept { return shortTerm.load (std::memory_order_relaxed); }
    float getIntegrated() const noexcept { return integrated.load (std::memory_order_relaxed); }

    void requestReset() noexcept { resetRequested.store (true); }

private:
    static double toLufs (double meanSquare) noexcept { return meanSquare > 0 ? -0.691 + 10.0 * std::log10 (meanSquare) : -INFINITY; }

    void closeSubblock (int numCh) noexcept
    {
        if (resetRequested.exchange (false))
        {
            sub.fill (0.0); subCount = 0;
            std::fill (histCount.begin(), histCount.end(), 0);
            std::fill (histSum.begin(), histSum.end(), 0.0);
            integrated.store (-INFINITY);
        }
        double e = 0;
        for (int c = 0; c < numCh; ++c) e += acc[c] / hop;  // channel weights L = R = 1.0
        if (numCh == 1) e *= 1.0; // mono: single channel, weight 1
        acc[0] = acc[1] = 0; accN = 0;

        sub[(size_t) subPos] = e;
        subPos = (subPos + 1) % kSubblocksShort;
        subCount = std::min (subCount + 1, kSubblocksShort);

        auto meanLast = [this] (int k) {
            double s = 0;
            for (int j = 1; j <= k; ++j) s += sub[(size_t) ((subPos - j + kSubblocksShort) % kSubblocksShort)];
            return s / k;
        };
        if (subCount >= 4)
        {
            const double m = meanLast (4);
            momentary.store ((float) toLufs (m), std::memory_order_relaxed);
            // gating block (400 ms, 75 % overlap) for integrated loudness
            const double l = toLufs (m);
            if (l > -70.0)
            {
                const int bin = clampv ((int) ((l - kHistLo) / kHistStep), 0, kHistBins - 1);
                histCount[(size_t) bin]++;
                histSum[(size_t) bin] += m;
                updateIntegrated();
            }
        }
        if (subCount >= kSubblocksShort)
            shortTerm.store ((float) toLufs (meanLast (kSubblocksShort)), std::memory_order_relaxed);
        else if (subCount >= 4)
            shortTerm.store ((float) toLufs (meanLast (subCount)), std::memory_order_relaxed);
    }

    void updateIntegrated() noexcept
    {
        double s = 0; long long cnt = 0;
        for (int b = 0; b < kHistBins; ++b) { s += histSum[(size_t) b]; cnt += histCount[(size_t) b]; }
        if (cnt == 0) return;
        const double rel = toLufs (s / cnt) - 10.0;
        const int startBin = clampv ((int) std::ceil ((rel - kHistLo) / kHistStep), 0, kHistBins - 1);
        s = 0; cnt = 0;
        for (int b = startBin; b < kHistBins; ++b) { s += histSum[(size_t) b]; cnt += histCount[(size_t) b]; }
        if (cnt > 0) integrated.store ((float) toLufs (s / cnt), std::memory_order_relaxed);
    }

    double fs = 48000;
    Biquad pre, rlb;
    double acc[kMaxChannels] {};
    int accN = 0, hop = 4800;
    std::array<double, kSubblocksShort> sub {};
    int subCount = 0, subPos = 0;
    std::vector<long long> histCount;
    std::vector<double> histSum;
    std::atomic<float> momentary { -INFINITY }, shortTerm { -INFINITY }, integrated { -INFINITY };
    std::atomic<bool> resetRequested { false };
};

/** Peak / true-peak / RMS / correlation meter for one stereo point. */
class LevelMeter
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        decay = (float) std::exp (-1.0 / (0.3 * fs));   // peak fall-back for the UI value
        corrCoeff = (float) std::exp (-1.0 / (0.3 * fs));
        reset();
    }
    void reset() noexcept
    {
        tpi.reset();
        for (auto& a : peakHold) a.store (0.0f);
        truePeakMax.store (0.0f);
        sLR = sLL = sRR = 0;
    }
    void setTruePeak (bool on) noexcept { measureTruePeak = on; }

    void process (const float* const* data, int numCh, int n) noexcept
    {
        float pk[kMaxChannels] {};
        float tp = truePeakMax.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < numCh; ++c)
            {
                const float x = data[c][i];
                pk[c] = std::max (pk[c], std::abs (x));
                if (measureTruePeak) tp = std::max (tp, tpi.process (c, x));
            }
            if (numCh > 1)
            {
                const float l = data[0][i], r = data[1][i];
                sLR = l * r + corrCoeff * (sLR - l * r);
                sLL = l * l + corrCoeff * (sLL - l * l);
                sRR = r * r + corrCoeff * (sRR - r * r);
            }
        }
        for (int c = 0; c < numCh; ++c)
        {
            const float prev = peakHold[c].load (std::memory_order_relaxed) * std::pow (decay, (float) n);
            peakHold[c].store (std::max (prev, pk[c]), std::memory_order_relaxed);
        }
        if (numCh == 1) peakHold[1].store (peakHold[0].load());
        truePeakMax.store (tp, std::memory_order_relaxed);
        const double den = std::sqrt (sLL * sRR);
        correlation.store (numCh > 1 ? (den > 1e-12 ? (float) clampv (sLR / den, -1.0, 1.0) : 1.0f) : 1.0f, std::memory_order_relaxed);
    }

    float getPeak (int ch) const noexcept { return peakHold[ch].load (std::memory_order_relaxed); }
    float getTruePeakMax() const noexcept { return truePeakMax.load (std::memory_order_relaxed); }
    float getCorrelation() const noexcept { return correlation.load (std::memory_order_relaxed); }
    void resetTruePeak() noexcept { truePeakMax.store (0.0f); }

private:
    double fs = 48000;
    float decay = 0.999f, corrCoeff = 0.999f;
    bool measureTruePeak = true;
    TruePeakInterpolator tpi;
    std::atomic<float> peakHold[kMaxChannels] { {0.0f}, {0.0f} };
    std::atomic<float> truePeakMax { 0.0f }, correlation { 1.0f };
    double sLR = 0, sLL = 0, sRR = 0;
};

} // namespace bm
