// BANKMASTER by KALOX — DSP core
// MODULE 1: MASTER EQ — 5-band zero-latency parametric / dynamic EQ.
#pragma once

#include "Svf.h"
#include <array>
#include <atomic>

namespace bm
{
enum class BandType : int { Bell = 0, LowShelf, HighShelf, LowCut, HighCut, Notch, BandPass, NumTypes };
enum class Placement : int { Stereo = 0, Mid, Side, Left, Right, NumPlacements };

constexpr int kNumEqBands = 5;
constexpr int kMaxSections = 4;
constexpr int kNumSlopes = 6;
inline int slopeDbPerOct (int idx) noexcept
{
    static const int s[kNumSlopes] = { 6, 12, 18, 24, 36, 48 };
    return s[clampv (idx, 0, kNumSlopes - 1)];
}

struct EqBandParams
{
    bool enabled = false;
    BandType type = BandType::Bell;
    float freq = 1000.0f, gainDb = 0.0f, q = 1.0f;
    int slopeIdx = 1; // 12 dB/oct
    Placement placement = Placement::Stereo;
    bool dynOn = false;
    float dynRangeDb = 0.0f;     // signed: negative = cut when signal exceeds threshold
    float dynThresholdDb = -30.0f;
};

struct EqParams
{
    std::array<EqBandParams, kNumEqBands> bands {};
};

inline bool bandSupportsGain (BandType t) noexcept
{
    return t == BandType::Bell || t == BandType::LowShelf || t == BandType::HighShelf;
}
inline bool bandSupportsDynamics (BandType t) noexcept { return bandSupportsGain (t); }

/** Builds the cascade of sections for one band. Returns the number of sections. */
inline int designBand (BandType type, double freq, double q, double gainDb, int slopeIdx,
                       double fs, SvfCoeffs* out) noexcept
{
    const int order = slopeDbPerOct (slopeIdx) / 6;
    int n = 0;
    auto butterQ = [] (int N, int i) // i-th 2nd-order section of an order-N Butterworth
    {
        if (N % 2 == 0) return 1.0 / (2.0 * std::cos (kPi * (2 * i + 1) / (2.0 * N)));
        return 1.0 / (2.0 * std::cos (kPi * (i + 1) / (double) N));
    };

    switch (type)
    {
        case BandType::Bell:     out[n++] = SvfCoeffs::design (SectionType::Bell, freq, q, gainDb, fs); break;
        case BandType::Notch:    out[n++] = SvfCoeffs::design (SectionType::Notch, freq, q, 0, fs); break;
        case BandType::BandPass: out[n++] = SvfCoeffs::design (SectionType::BandPass, freq, q, 0, fs); break;
        case BandType::LowCut:
        case BandType::HighCut:
        {
            const bool hp = type == BandType::LowCut;
            const int pairs = order / 2;
            if (order % 2 == 1)
                out[n++] = SvfCoeffs::design (hp ? SectionType::HighPass1 : SectionType::LowPass1, freq, 0.7071, 0, fs);
            for (int i = 0; i < pairs; ++i)
            {
                double qi = butterQ (order, i);
                // user Q = 1.0 => maximally flat (Butterworth). The resonant (last)
                // section is scaled so Q > 1 adds a resonant bump, Q < 1 softens the knee.
                if (i == pairs - 1) qi *= q;
                out[n++] = SvfCoeffs::design (hp ? SectionType::HighPass : SectionType::LowPass, freq, qi, 0, fs);
            }
            break;
        }
        case BandType::LowShelf:
        case BandType::HighShelf:
        {
            const bool low = type == BandType::LowShelf;
            if (order == 1)
            {
                out[n++] = SvfCoeffs::design (low ? SectionType::LowShelf1 : SectionType::HighShelf1, freq, 1, gainDb, fs);
                break;
            }
            const int N = std::min (order, 2 * kMaxSections);
            const int pairs = N / 2;
            if (N % 2 == 1)
                out[n++] = SvfCoeffs::design (low ? SectionType::LowShelf1 : SectionType::HighShelf1, freq, 1, gainDb / N, fs);
            for (int i = 0; i < pairs && n < kMaxSections; ++i)
            {
                double qi = (N == 2 ? 0.70710678 : butterQ (N, i));
                if (i == pairs - 1) qi *= q;
                out[n++] = SvfCoeffs::design (low ? SectionType::LowShelf : SectionType::HighShelf, freq, qi, gainDb * 2.0 / N, fs);
            }
            break;
        }
        default: break;
    }
    return n;
}

/** Magnitude (dB) of one band at frequency f — used by tests and the UI curve. */
inline double bandMagnitudeDb (const EqBandParams& p, double f, double fs, float gainOverrideDb = 1e9f) noexcept
{
    if (! p.enabled) return 0.0;
    SvfCoeffs c[kMaxSections];
    const double g = gainOverrideDb < 1e8f ? gainOverrideDb : p.gainDb;
    const int n = designBand (p.type, p.freq, p.q, g, p.slopeIdx, fs, c);
    std::complex<double> h = 1.0;
    for (int i = 0; i < n; ++i) h *= c[i].response (f, fs);
    return 20.0 * std::log10 (std::max (std::abs (h), 1e-12));
}

class MasterEQ
{
public:
    void prepare (double sampleRate, int /*maxBlock*/)
    {
        fs = sampleRate;
        for (auto& b : bands) b.prepare (fs);
        reset();
    }

    void reset()
    {
        for (auto& b : bands) b.reset();
    }

    void setParams (const EqParams& p) noexcept
    {
        for (int i = 0; i < kNumEqBands; ++i) bands[(size_t) i].setParams (p.bands[(size_t) i]);
    }

    void process (float* const* data, int numCh, int n) noexcept
    {
        for (auto& b : bands) b.process (data, numCh, n);
    }

    /** Current (dynamic) gain of a band, readable from any thread. */
    float getCurrentGainDb (int band) const noexcept { return bands[(size_t) band].currentGainDb.load (std::memory_order_relaxed); }

private:
    struct Band
    {
        static constexpr int kUpdate = 16;
        double fs = 48000;
        EqBandParams p;
        SvfCoeffs coeffs[kMaxSections];
        int numSections = 0;
        SvfState st[kMaxChannels][kMaxSections];
        SvfCoeffs detCoeff;
        SvfState detState[kMaxChannels];
        float env[kMaxChannels] {};
        float envAtt = 0, envRel = 0;
        LinearSmoother logFreq, gain, logQ, mix;
        float dynGainDb = 0;
        bool needsDesign = true;
        int lastType = -1, lastSlope = -1;
        std::atomic<float> currentGainDb { 0.0f };

        void prepare (double sr)
        {
            fs = sr;
            logFreq.reset (fs, 0.03); gain.reset (fs, 0.03); logQ.reset (fs, 0.03); mix.reset (fs, 0.025);
            envAtt = timeCoeff (3.0f, fs);
            envRel = timeCoeff (80.0f, fs);
        }
        void reset()
        {
            for (auto& ch : st) for (auto& s : ch) s.reset();
            for (auto& s : detState) s.reset();
            for (auto& e : env) e = 0;
            logFreq.setCurrentAndTarget (std::log2 (p.freq));
            gain.setCurrentAndTarget (p.gainDb);
            logQ.setCurrentAndTarget (std::log2 (p.q));
            mix.setCurrentAndTarget (p.enabled ? 1.0f : 0.0f);
            dynGainDb = 0;
            needsDesign = true;
        }
        void setParams (const EqBandParams& np) noexcept
        {
            const bool structural = (int) np.type != lastType || np.slopeIdx != lastSlope;
            p = np;
            p.freq = clampv (p.freq, 10.0f, 30000.0f);
            p.q = clampv (p.q, 0.025f, 40.0f);
            if (structural)
            {
                // type/slope switch: jump coefficients, keep filter state (sections are re-used)
                logFreq.setCurrentAndTarget (std::log2 (p.freq));
                gain.setCurrentAndTarget (p.gainDb);
                logQ.setCurrentAndTarget (std::log2 (p.q));
                for (auto& ch : st) for (auto& s : ch) s.reset();
                lastType = (int) p.type; lastSlope = p.slopeIdx;
                needsDesign = true;
            }
            logFreq.setTarget (std::log2 (p.freq));
            gain.setTarget (p.gainDb);
            logQ.setTarget (std::log2 (p.q));
            mix.setTarget (p.enabled ? 1.0f : 0.0f);
        }

        void design (float extraGainDb) noexcept
        {
            const double f = std::exp2 (logFreq.getCurrent());
            const double q = std::exp2 (logQ.getCurrent());
            const double g = gain.getCurrent() + extraGainDb;
            numSections = designBand (p.type, f, q, g, p.slopeIdx, fs, coeffs);
            if (p.dynOn && bandSupportsDynamics (p.type))
            {
                const SectionType dt = p.type == BandType::LowShelf ? SectionType::LowPass
                                     : p.type == BandType::HighShelf ? SectionType::HighPass
                                     : SectionType::BandPass;
                detCoeff = SvfCoeffs::design (dt, f, p.type == BandType::Bell ? q : 0.7071, 0, fs);
            }
            currentGainDb.store ((float) g, std::memory_order_relaxed);
        }

        inline float runSections (int ch, float x) noexcept
        {
            for (int s = 0; s < numSections; ++s) x = st[ch][s].process (coeffs[s], x);
            return x;
        }

        void process (float* const* data, int numCh, int n) noexcept
        {
            if (! p.enabled && ! mix.isSmoothing() && mix.getCurrent() == 0.0f)
                return; // fully off (state was reset when it faded out)

            const bool dyn = p.dynOn && bandSupportsDynamics (p.type) && p.dynRangeDb != 0.0f;
            float* L = data[0];
            float* R = numCh > 1 ? data[1] : nullptr;

            for (int start = 0; start < n; start += kUpdate)
            {
                const int len = std::min (kUpdate, n - start);
                const bool smoothing = logFreq.isSmoothing() || gain.isSmoothing() || logQ.isSmoothing();
                logFreq.skip (len); gain.skip (len); logQ.skip (len);

                if (dyn)
                {
                    float lev = env[0];
                    if (R != nullptr && (p.placement == Placement::Stereo)) lev = std::max (lev, env[1]);
                    const float levDb = gainToDb (lev, -120.0f);
                    const float over = std::max (0.0f, levDb - p.dynThresholdDb);
                    const float amount = 1.0f - std::exp (-over / 6.0f);
                    dynGainDb = p.dynRangeDb * amount;
                }
                else dynGainDb = 0.0f;

                if (smoothing || needsDesign || dyn)
                {
                    design (dynGainDb);
                    needsDesign = false;
                }

                for (int i = start; i < start + len; ++i)
                {
                    const float m = sCurve (mix.next());
                    float l = L[i], r = R ? R[i] : 0.0f;
                    float wl = l, wr = r;

                    switch (p.placement)
                    {
                        case Placement::Stereo:
                            wl = runSections (0, l);
                            if (R) wr = runSections (1, r);
                            if (dyn) { detect (0, l); if (R) detect (1, r); }
                            break;
                        case Placement::Left:
                            wl = runSections (0, l);
                            if (dyn) detect (0, l);
                            break;
                        case Placement::Right:
                            if (R) { wr = runSections (1, r); if (dyn) detect (0, r); }
                            break;
                        case Placement::Mid:
                        case Placement::Side:
                        {
                            const float mid = 0.5f * (l + r), side = 0.5f * (l - r);
                            float mm = mid, ss = side;
                            if (p.placement == Placement::Mid) { mm = runSections (0, mid); if (dyn) detect (0, mid); }
                            else                                { ss = runSections (0, side); if (dyn) detect (0, side); }
                            wl = mm + ss; wr = mm - ss;
                            if (! R) wl = mm;
                            break;
                        }
                        default: break;
                    }
                    L[i] = l + m * (wl - l);
                    if (R) R[i] = r + m * (wr - r);
                }
            }

            if (! p.enabled && ! mix.isSmoothing())
            {
                for (auto& ch : st) for (auto& s : ch) s.reset();
                for (auto& s : detState) s.reset();
                currentGainDb.store (0.0f, std::memory_order_relaxed);
            }
        }

        inline void detect (int ch, float x) noexcept
        {
            const float y = std::abs (detState[ch].process (detCoeff, x));
            float& e = env[ch];
            e = y > e ? y + envAtt * (e - y) : y + envRel * (e - y);
        }
    };

    double fs = 48000;
    std::array<Band, kNumEqBands> bands;
};

} // namespace bm
