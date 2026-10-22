// BANKMASTER by KALOX — factory presets (single source of truth for plugin + tests)
//
// KALOX MASTER — ORIGINAL reproduces the parameters read from the reference
// screenshots (Pro-Q 4 / Saturn 2 / Pro-C 2 / Pro-L 2). Values marked EST are
// estimates (knob positions / curve fitting) — see docs/PARAMETERS.md.
// All other presets are adjustable starting points, not automatic masters. Their
// limiter drive was calibrated for a mix at the INPUT LEARN reference (-20 LUFS
// integrated): AFRO ~ -9, DEEP ~ -10, TECH ~ -8.5, CLUB ~ -7.5, TRANSPARENT ~ -12,
// CUSTOM ~ -14 LUFS-I on the test material (results vary with the music).
#pragma once

#include "MasterChain.h"
#include <string>
#include <vector>

namespace bm
{
struct FactoryPreset
{
    std::string name;
    ChainParams params;
};

namespace presets
{
    inline EqBandParams band (BandType t, float f, float g, float q, int slopeIdx = 1, bool on = true)
    {
        EqBandParams b;
        b.enabled = on; b.type = t; b.freq = f; b.gainDb = g; b.q = q; b.slopeIdx = slopeIdx;
        return b;
    }
    inline EqBandParams dynBand (float f, float g, float q, float range, float thr)
    {
        auto b = band (BandType::Bell, f, g, q);
        b.dynOn = true; b.dynRangeDb = range; b.dynThresholdDb = thr;
        return b;
    }

    inline ChainParams kaloxOriginal()
    {
        ChainParams p;
        // ---- EQ (Pro-Q 4, Zero Latency)
        p.eq.bands[0] = band (BandType::LowCut, 25.0f, 0.0f, 1.0f, 3);          // 24 dB/oct, Q 1.000
        p.eq.bands[1] = band (BandType::Bell, 250.0f, -1.0f, 1.0f);             // -1.00 dB, Q 1.000
        p.eq.bands[2] = dynBand (861.41f, 0.0f, 1.622f, -1.0f, -30.0f);         // dynamic: range EST, threshold EST
        p.eq.bands[3] = dynBand (1559.3f, 0.0f, 1.622f, -1.0f, -30.0f);         // dynamic: range EST, threshold EST
        p.eq.bands[4] = band (BandType::HighShelf, 10000.0f, 1.0f, 1.0f, 1);    // +1.00 dB, 12 dB/oct
        // ---- Saturation (Saturn 2)
        p.sat.style = SatStyle::CleanTube;
        p.sat.drivePct = 20.0f;
        p.sat.mixPct = 30.0f;          // EST (knob at ~10 o'clock)
        p.sat.tone = 0.0f;
        p.sat.dynamics = 0.0f;         // knob centred
        p.sat.feedbackPct = 0.0f;      // EST (knob at minimum)
        p.sat.feedbackFreq = 2000.0f;  // EST
        p.sat.outputDb = -1.0f;        // EST (bottom bar shows -1.0 dB)
        p.sat.osFactorLog2 = 2;        // "High Quality" -> 4x linear-phase oversampling
        p.sat.quality = 1;
        // ---- Compressor (Pro-C 2)
        p.comp.style = CompStyle::Clean;
        p.comp.thresholdDb = -18.0f;
        p.comp.ratio = 2.0f;
        p.comp.kneeDb = 20.0f;         // EST (fitted from the transfer-curve display)
        p.comp.attackMs = 30.0f;
        p.comp.releaseMs = 100.0f;
        p.comp.autoRelease = false;
        p.comp.lookaheadMs = 0.0f;
        p.comp.holdMs = 0.0f;
        p.comp.rangeDb = 60.0f;
        p.comp.makeupDb = 0.0f;
        p.comp.autoGain = true;        // EST (AUTO GAIN label highlighted)
        p.comp.mixPct = 100.0f;        // Dry = OFF
        p.comp.stereoLinkPct = 100.0f; // EST
        // ---- Limiter (Pro-L 2)
        p.lim.style = LimStyle::Transparent;
        p.lim.inputGainDb = 11.0f;
        p.lim.ceilingDb = -0.1f;
        p.lim.attackMs = 98.21f;
        p.lim.releaseMs = 400.0f;
        p.lim.lookaheadMs = 1.004f;
        p.lim.truePeak = true;
        p.lim.osFactorLog2 = 4;        // 16x
        p.lim.linkTransientsPct = 0.0f; // EST
        p.lim.linkReleasePct = 0.0f;    // EST
        p.lim.dither = 0;
        return p;
    }

    inline ChainParams afroHouse()
    {
        ChainParams p = kaloxOriginal();
        p.eq.bands[0] = band (BandType::LowCut, 28.0f, 0.0f, 1.0f, 3);
        p.eq.bands[1] = band (BandType::Bell, 110.0f, 0.8f, 0.8f);
        p.eq.bands[2] = band (BandType::Bell, 350.0f, -1.2f, 1.2f);
        p.eq.bands[3] = dynBand (2500.0f, 0.0f, 1.4f, -1.5f, -30.0f);
        p.eq.bands[4] = band (BandType::HighShelf, 12000.0f, 1.5f, 1.0f, 1);
        p.sat.drivePct = 18.0f; p.sat.mixPct = 35.0f;
        p.comp.thresholdDb = -20.0f; p.comp.ratio = 1.8f; p.comp.kneeDb = 18.0f;
        p.comp.attackMs = 35.0f; p.comp.releaseMs = 120.0f; p.comp.autoRelease = true; p.comp.autoGain = true;
        p.lim.inputGainDb = 10.5f; p.lim.releaseMs = 300.0f; p.lim.lookaheadMs = 1.2f;
        return p;
    }

    inline ChainParams deepHouse()
    {
        ChainParams p = kaloxOriginal();
        p.eq.bands[0] = band (BandType::LowCut, 22.0f, 0.0f, 1.0f, 3);
        p.eq.bands[1] = band (BandType::Bell, 80.0f, 0.6f, 0.9f);
        p.eq.bands[2] = band (BandType::Bell, 300.0f, -1.0f, 1.0f);
        p.eq.bands[3] = dynBand (900.0f, 0.0f, 1.6f, -1.0f, -30.0f);
        p.eq.bands[4] = band (BandType::HighShelf, 10000.0f, 0.5f, 1.0f, 1);
        p.sat.style = SatStyle::WarmTube; p.sat.drivePct = 22.0f; p.sat.mixPct = 30.0f;
        p.comp.thresholdDb = -20.0f; p.comp.ratio = 1.6f; p.comp.attackMs = 40.0f; p.comp.releaseMs = 200.0f; p.comp.autoGain = true;
        p.lim.inputGainDb = 10.0f; p.lim.releaseMs = 500.0f;
        return p;
    }

    inline ChainParams techHouse()
    {
        ChainParams p = kaloxOriginal();
        p.eq.bands[0] = band (BandType::LowCut, 30.0f, 0.0f, 1.0f, 3);
        p.eq.bands[1] = band (BandType::Bell, 60.0f, 0.5f, 1.2f);
        p.eq.bands[2] = band (BandType::Bell, 250.0f, -1.5f, 1.0f);
        p.eq.bands[3] = dynBand (1200.0f, 0.0f, 1.6f, -1.5f, -30.0f);
        p.eq.bands[4] = band (BandType::HighShelf, 9000.0f, 1.5f, 1.0f, 1);
        p.sat.style = SatStyle::Tape; p.sat.drivePct = 25.0f; p.sat.mixPct = 35.0f;
        p.comp.thresholdDb = -16.0f; p.comp.ratio = 2.5f; p.comp.kneeDb = 12.0f;
        p.comp.attackMs = 20.0f; p.comp.releaseMs = 80.0f; p.comp.autoGain = true;
        p.lim.style = LimStyle::Punchy; p.lim.inputGainDb = 9.0f; p.lim.releaseMs = 250.0f;
        return p;
    }

    inline ChainParams club()
    {
        ChainParams p = kaloxOriginal();
        p.eq.bands[0] = band (BandType::LowCut, 30.0f, 0.0f, 1.0f, 3);
        p.eq.bands[1] = band (BandType::Bell, 50.0f, 1.0f, 1.0f);
        p.eq.bands[2] = band (BandType::Bell, 300.0f, -1.0f, 1.0f);
        p.eq.bands[3] = dynBand (3000.0f, 0.0f, 1.4f, -2.0f, -30.0f);
        p.eq.bands[4] = band (BandType::HighShelf, 12000.0f, 1.0f, 1.0f, 1);
        p.sat.style = SatStyle::WarmTube; p.sat.drivePct = 28.0f; p.sat.mixPct = 40.0f;
        p.comp.thresholdDb = -18.0f; p.comp.ratio = 2.5f; p.comp.attackMs = 15.0f; p.comp.releaseMs = 80.0f;
        p.comp.autoRelease = true; p.comp.autoGain = true;
        p.lim.style = LimStyle::Punchy; p.lim.inputGainDb = 11.5f; p.lim.releaseMs = 200.0f; p.lim.ceilingDb = -0.3f;
        return p;
    }

    inline ChainParams transparent()
    {
        ChainParams p = kaloxOriginal();
        p.eq.bands[0] = band (BandType::LowCut, 20.0f, 0.0f, 1.0f, 1);
        p.eq.bands[1] = band (BandType::Bell, 250.0f, 0.0f, 1.0f);
        p.eq.bands[2] = band (BandType::Bell, 1000.0f, 0.0f, 1.0f);
        p.eq.bands[3] = band (BandType::Bell, 4000.0f, 0.0f, 1.0f);
        p.eq.bands[4] = band (BandType::HighShelf, 10000.0f, 0.0f, 1.0f, 1);
        p.sat.drivePct = 8.0f; p.sat.mixPct = 20.0f; p.sat.outputDb = 0.0f;
        p.comp.thresholdDb = -16.0f; p.comp.ratio = 1.5f; p.comp.kneeDb = 24.0f;
        p.comp.attackMs = 50.0f; p.comp.releaseMs = 300.0f; p.comp.autoRelease = true; p.comp.autoGain = false;
        p.lim.inputGainDb = 10.0f; p.lim.releaseMs = 600.0f; p.lim.lookaheadMs = 2.0f;
        p.lim.linkTransientsPct = 75.0f; p.lim.linkReleasePct = 100.0f;
        return p;
    }

    inline ChainParams customUser()
    {
        ChainParams p = transparent();
        p.sat.drivePct = 0.0f; p.sat.mixPct = 100.0f;
        p.comp.thresholdDb = -12.0f; p.comp.ratio = 1.5f;
        p.lim.inputGainDb = 7.0f; p.lim.ceilingDb = -1.0f;
        return p;
    }
}

inline const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> list {
        { "KALOX MASTER - ORIGINAL", presets::kaloxOriginal() },
        { "AFRO HOUSE MASTER",       presets::afroHouse() },
        { "DEEP HOUSE MASTER",       presets::deepHouse() },
        { "TECH HOUSE MASTER",       presets::techHouse() },
        { "CLUB MASTER",             presets::club() },
        { "TRANSPARENT MASTER",      presets::transparent() },
        { "CUSTOM USER PRESET",      presets::customUser() },
    };
    return list;
}

} // namespace bm
