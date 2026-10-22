// BANKMASTER by KALOX — DSP core
// Complete chain: INPUT -> EQ -> SATURATION -> COMPRESSOR -> LIMITER -> OUTPUT
// in a single instance, with latency-compensated click-free bypasses and metering.
#pragma once

#include "MasterEQ.h"
#include "Saturation.h"
#include "Compressor.h"
#include "Limiter.h"
#include "Meters.h"

namespace bm
{
enum ModuleId { kModEq = 0, kModSat, kModComp, kModLim, kNumModules };

struct ChainParams
{
    float inputGainDb = 0.0f;
    float outputGainDb = 0.0f;
    bool masterBypass = false;
    bool moduleOn[kNumModules] { true, true, true, true };
    bool oneMasterOn = false;
    float oneMasterPct = 100.0f;    // 0..200, 100 = settings as dialled
    EqParams eq;
    SatParams sat;
    CompParams comp;
    LimParams lim;
};

/** ONE MASTER macro: scales processing *intensity* of all four modules around the
    dialled settings (100 %). 0 % = EQ flat (cuts kept), no drive, 1:1 compression,
    limiter at unity drive (peak protection only). 200 % = twice the EQ moves and
    drive, compression ratio slope doubled, limiter drive up to +6 dB more (never
    above +24 dB in total). It never touches the output trim or the ceiling. */
inline ChainParams applyOneMaster (const ChainParams& in) noexcept
{
    if (! in.oneMasterOn) return in;
    ChainParams p = in;
    const float k = clampv (in.oneMasterPct, 0.0f, 200.0f) / 100.0f;

    for (auto& b : p.eq.bands)
    {
        if (bandSupportsGain (b.type)) b.gainDb = clampv (b.gainDb * k, -24.0f, 24.0f);
        b.dynRangeDb = clampv (b.dynRangeDb * k, -24.0f, 24.0f);
    }
    p.sat.drivePct = clampv (in.sat.drivePct * k, 0.0f, 100.0f);
    p.comp.ratio = std::max (1.0f, 1.0f + (in.comp.ratio - 1.0f) * k);

    const float g = in.lim.inputGainDb;
    if (k <= 1.0f) p.lim.inputGainDb = g > 0.0f ? g * k : g;
    else           p.lim.inputGainDb = std::min (24.0f, g + (k - 1.0f) * std::min (6.0f, std::max (0.0f, g) * 0.5f));
    return p;
}

class MasterChain
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels)
    {
        fs = sampleRate;
        channels = std::min (numChannels, kMaxChannels);
        maxBlock = std::max (16, maxBlockSize);
        eq.prepare (fs, maxBlock);
        sat.prepare (fs, maxBlock, channels);
        comp.prepare (fs, maxBlock, channels);
        lim.prepare (fs, maxBlock, channels);
        for (int c = 0; c < kMaxChannels; ++c)
        {
            dryIn[c].assign ((size_t) maxBlock, 0.0f);
            modIn[c].assign ((size_t) maxBlock, 0.0f);
        }
        tapPre.assign ((size_t) maxBlock, 0.0f);
        tapPost.assign ((size_t) maxBlock, 0.0f);
        const int maxLat = 8192;
        masterDelay.prepare (channels, maxLat);
        for (auto& d : modDelay) d.prepare (channels, maxLat);
        inSm.reset (fs, 0.05); outSm.reset (fs, 0.05); bypassSm.reset (fs, 0.03);
        for (auto& m : modMix) m.reset (fs, 0.03);
        declickStep = (float) (1.0 / (0.012 * fs)); // 12 ms fades
        inMeter.prepare (fs); outMeter.prepare (fs); loud.prepare (fs); inLoud.prepare (fs);
        reset();
    }

    void reset()
    {
        if (pendingStructure)
        {
            active = structureOf (effective); pendingStructure = false;
            sat.setParams (effective.sat); comp.setParams (effective.comp); lim.setParams (effective.lim);
            updateLatencies();
        }
        eq.reset(); sat.reset(); comp.reset(); lim.reset();
        masterDelay.clear();
        for (auto& d : modDelay) d.clear();
        inSm.setCurrentAndTarget (dbToGain (params.inputGainDb));
        outSm.setCurrentAndTarget (dbToGain (params.outputGainDb));
        bypassSm.setCurrentAndTarget (params.masterBypass ? 0.0f : 1.0f);
        for (int m = 0; m < kNumModules; ++m) modMix[m].setCurrentAndTarget (params.moduleOn[m] ? 1.0f : 0.0f);
        inMeter.reset(); outMeter.reset(); loud.reset(); inLoud.reset();
        declickState = Declick::Idle; declickGain = 1.0f;
    }

    /** Call once per block before process (audio thread).
        Structural changes (oversampling, look-ahead, true-peak mode) change the
        latency and the internal filter states: they are applied behind a short
        fade-out / fade-in so they never click. */
    void setParams (const ChainParams& raw) noexcept
    {
        params = raw;
        effective = applyOneMaster (raw);

        const Structure wanted = structureOf (effective);
        if (! structureValid) { active = wanted; structureValid = true; }
        if (! (wanted == active))
        {
            pendingStructure = true;
            if (declickState == Declick::Idle) declickState = Declick::FadingOut;
        }
        ChainParams applied = effective;
        applyStructure (applied, active);   // keep the running structure until faded out

        eq.setParams (applied.eq);
        sat.setParams (applied.sat);
        comp.setParams (applied.comp);
        lim.setParams (applied.lim);
        inSm.setTarget (dbToGain (clampv (raw.inputGainDb, -24.0f, 24.0f)));
        outSm.setTarget (dbToGain (clampv (raw.outputGainDb, -24.0f, 12.0f)));
        bypassSm.setTarget (raw.masterBypass ? 0.0f : 1.0f);
        for (int m = 0; m < kNumModules; ++m) modMix[m].setTarget (raw.moduleOn[m] ? 1.0f : 0.0f);
        updateLatencies();
    }

    const ChainParams& getEffectiveParams() const noexcept { return effective; }
    int getLatency() const noexcept { return totalLatency; }

    /** In-place processing. Blocks larger than the prepared size are split. */
    void process (float* const* data, int numCh, int n) noexcept
    {
        ScopedNoDenormals noDenormals;
        numCh = std::min (numCh, channels);
        for (int start = 0; start < n; start += maxBlock)
        {
            const int len = std::min (maxBlock, n - start);
            float* ptr[kMaxChannels] = { data[0] + start, numCh > 1 ? data[1] + start : nullptr };
            processChunk (ptr, numCh, len);
        }
    }

    // ---- metering / analysis access (any thread)
    LevelMeter& getInputMeter() noexcept { return inMeter; }
    LevelMeter& getOutputMeter() noexcept { return outMeter; }
    LoudnessMeter& getLoudness() noexcept { return loud; }
    /** Loudness of the raw input (before the INPUT gain) — used by INPUT LEARN. */
    LoudnessMeter& getInputLoudness() noexcept { return inLoud; }
    const MasterEQ& getEq() const noexcept { return eq; }
    float getCompGainReduction() const noexcept { return comp.getGainReductionDb(); }
    float getLimGainReduction() const noexcept { return lim.getGainReductionDb(); }
    long long getLimiterSafetyClamps() const noexcept { return lim.getSafetyClampCount(); }
    int getLimiterOsFactor() const noexcept { return lim.getEffectiveOsFactor(); }
    /** Mono analysis taps of the last processed chunk (pre / post EQ). */
    const float* getPreEqTap() const noexcept { return tapPre.data(); }
    const float* getPostEqTap() const noexcept { return tapPost.data(); }
    int getLastChunkSize() const noexcept { return lastChunk; }
    int getModuleLatency (int m) const noexcept { return modLatency[m]; }

private:
    struct Structure
    {
        int satOs = 0, satQ = 0, limOs = 0; float compLa = 0, limLa = 0; bool limTp = true;
        bool operator== (const Structure& o) const noexcept
        {
            return satOs == o.satOs && satQ == o.satQ && limOs == o.limOs && compLa == o.compLa && limLa == o.limLa && limTp == o.limTp;
        }
    };
    enum class Declick { Idle, FadingOut, FadingIn };

    static Structure structureOf (const ChainParams& p) noexcept
    {
        Structure s;
        s.satOs = p.sat.osFactorLog2; s.satQ = p.sat.quality; s.limOs = p.lim.osFactorLog2;
        s.compLa = p.comp.lookaheadMs; s.limLa = p.lim.lookaheadMs; s.limTp = p.lim.truePeak;
        return s;
    }
    static void applyStructure (ChainParams& p, const Structure& s) noexcept
    {
        p.sat.osFactorLog2 = s.satOs; p.sat.quality = s.satQ; p.lim.osFactorLog2 = s.limOs;
        p.comp.lookaheadMs = s.compLa; p.lim.lookaheadMs = s.limLa; p.lim.truePeak = s.limTp;
    }

    /** Runs at the end of a chunk: switches structure once the output is silent. */
    void advanceDeclick() noexcept
    {
        if (declickState == Declick::FadingOut && declickGain <= 0.0f && pendingStructure)
        {
            active = structureOf (effective);
            pendingStructure = false;
            ChainParams applied = effective;
            sat.setParams (applied.sat); comp.setParams (applied.comp); lim.setParams (applied.lim);
            sat.reset(); comp.reset(); lim.reset();
            for (auto& d : modDelay) d.clear();
            masterDelay.clear();
            updateLatencies();
            // stay silent until the re-initialised delay lines are filled with audio
            declickHold = totalLatency + 64;
            declickState = Declick::FadingIn;
        }
    }

    void updateLatencies() noexcept
    {
        modLatency[kModEq] = 0;
        modLatency[kModSat] = sat.getLatency();
        modLatency[kModComp] = comp.getLatency();
        modLatency[kModLim] = lim.getLatency();
        int total = 0;
        for (int m = 0; m < kNumModules; ++m)
        {
            modDelay[m].setDelay (modLatency[m]);
            total += modLatency[m];
        }
        if (total != totalLatency)
        {
            totalLatency = total;
            masterDelay.setDelay (total);
        }
    }

    template <typename Fn>
    void runModule (int m, float* const* d, int numCh, int n, Fn&& fn) noexcept
    {
        // the module is always processed (keeps its state valid -> no clicks on re-enable);
        // a latency-aligned copy of its input is cross-faded in when bypassed
        auto& mix = modMix[m];
        const bool fullyOn = ! mix.isSmoothing() && mix.getTarget() == 1.0f;
        for (int c = 0; c < numCh; ++c) std::copy (d[c], d[c] + n, modIn[c].data());
        fn (d, numCh, n);
        float* mi[kMaxChannels] = { modIn[0].data(), modIn[1].data() };
        modDelay[m].processBlock (mi, numCh, n);
        if (fullyOn) return;
        for (int i = 0; i < n; ++i)
        {
            const float g = sCurve (mix.next());
            for (int c = 0; c < numCh; ++c) d[c][i] = modIn[c][(size_t) i] + g * (d[c][i] - modIn[c][(size_t) i]);
        }
    }

    void processChunk (float* const* d, int numCh, int n) noexcept
    {
        // sanitize input (NaN / Inf protection keeps every filter state finite)
        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < n; ++i)
                if (! std::isfinite (d[c][i])) d[c][i] = 0.0f;

        for (int c = 0; c < numCh; ++c) std::copy (d[c], d[c] + n, dryIn[c].data());
        inLoud.process (d, numCh, n);   // raw input loudness (before INPUT gain) for INPUT LEARN

        for (int i = 0; i < n; ++i)
        {
            const float g = inSm.next();
            for (int c = 0; c < numCh; ++c) d[c][i] *= g;
        }
        inMeter.process (d, numCh, n);

        for (int i = 0; i < n; ++i) tapPre[(size_t) i] = numCh > 1 ? 0.5f * (d[0][i] + d[1][i]) : d[0][i];
        runModule (kModEq, d, numCh, n, [this] (float* const* x, int c, int k) { eq.process (x, c, k); });
        for (int i = 0; i < n; ++i) tapPost[(size_t) i] = numCh > 1 ? 0.5f * (d[0][i] + d[1][i]) : d[0][i];
        lastChunk = n;

        runModule (kModSat, d, numCh, n, [this] (float* const* x, int c, int k) { sat.process (x, c, k); });
        runModule (kModComp, d, numCh, n, [this] (float* const* x, int c, int k) { comp.process (x, c, k); });
        runModule (kModLim, d, numCh, n, [this] (float* const* x, int c, int k) { lim.process (x, c, k); });

        for (int i = 0; i < n; ++i)
        {
            const float g = outSm.next();
            for (int c = 0; c < numCh; ++c) d[c][i] *= g;
        }

        // master bypass: cross-fade against the latency-compensated dry input
        float* dr[kMaxChannels] = { dryIn[0].data(), dryIn[1].data() };
        masterDelay.processBlock (dr, numCh, n);
        if (bypassSm.isSmoothing() || bypassSm.getTarget() < 1.0f)
        {
            for (int i = 0; i < n; ++i)
            {
                const float g = sCurve (bypassSm.next());
                for (int c = 0; c < numCh; ++c) d[c][i] = dr[c][i] + g * (d[c][i] - dr[c][i]);
            }
        }

        if (declickState != Declick::Idle)
        {
            for (int i = 0; i < n; ++i)
            {
                if (declickState == Declick::FadingOut) declickGain = std::max (0.0f, declickGain - declickStep);
                else if (declickState == Declick::FadingIn && declickHold > 0) --declickHold;
                else if (declickState == Declick::FadingIn)
                {
                    declickGain = std::min (1.0f, declickGain + declickStep);
                    if (declickGain >= 1.0f) declickState = Declick::Idle;
                }
                const float g = sCurve (declickGain);
                for (int c = 0; c < numCh; ++c) d[c][i] *= g;
            }
            advanceDeclick();
        }

        outMeter.process (d, numCh, n);
        loud.process (d, numCh, n);
    }

    double fs = 48000;
    int channels = 2, maxBlock = 512, lastChunk = 0, totalLatency = -1;
    ChainParams params, effective;
    MasterEQ eq;
    Saturation sat;
    Compressor comp;
    Limiter lim;
    std::vector<float> dryIn[kMaxChannels], modIn[kMaxChannels], tapPre, tapPost;
    DelayLine masterDelay, modDelay[kNumModules];
    int modLatency[kNumModules] {};
    LinearSmoother inSm, outSm, bypassSm, modMix[kNumModules];
    LevelMeter inMeter, outMeter;
    LoudnessMeter loud, inLoud;
    Structure active;
    bool structureValid = false, pendingStructure = false;
    Declick declickState = Declick::Idle;
    float declickGain = 1.0f, declickStep = 1.0f / 480.0f;
    int declickHold = 0;
};

} // namespace bm
