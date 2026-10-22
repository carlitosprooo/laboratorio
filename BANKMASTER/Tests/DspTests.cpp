// BANKMASTER by KALOX — DSP unit tests (framework-free, run on every platform / arch).
#include "TestUtils.h"
#include "../Source/DSP/FactoryPresets.h"

using namespace bmtest;
using bm::ChainParams;

namespace
{
constexpr double kFs = 48000.0;

struct ChainRunner
{
    bm::MasterChain chain;
    int block;
    ChainRunner (const ChainParams& p, double fs = kFs, int blockSize = 512)
        : block (blockSize)
    {
        chain.prepare (fs, blockSize, 2);
        chain.setParams (p);
        chain.reset();
    }
    void run (Buffer& b, const std::function<void (int, bm::MasterChain&)>& perBlock = {})
    {
        const int n = (int) b[0].size();
        for (int s = 0; s < n; s += block)
        {
            const int len = std::min (block, n - s);
            if (perBlock) perBlock (s, chain);
            float* d[2] = { b[0].data() + s, b[1].data() + s };
            chain.process (d, 2, len);
        }
    }
};

ChainParams neutralChain()
{
    ChainParams p;
    for (auto& b : p.eq.bands) b.enabled = false;
    p.sat.drivePct = 0; p.sat.mixPct = 0; p.sat.outputDb = 0;
    p.comp.thresholdDb = 0; p.comp.ratio = 1; p.comp.autoGain = false;
    p.lim.inputGainDb = 0; p.lim.ceilingDb = 0;
    return p;
}

double measureGainDb (const std::function<void (float* const*, int, int)>& proc, double freq, double fs, int latency = 0)
{
    const int n = (int) (fs * 0.6);
    Buffer b = makeBuffer (2, n);
    sine (b, freq, 0.1, fs);
    for (int s = 0; s < n; s += 256)
    {
        float* d[2] = { b[0].data() + s, b[1].data() + s };
        proc (d, 2, std::min (256, n - s));
    }
    const int start = n / 2, len = n / 2 - latency - 1;
    (void) len;
    return db (toneAmplitude (b[0], freq, fs, start, n - start) / 0.1);
}
} // namespace

// =====================================================================================
// 2/3. Filters: stability + exact frequency response
// =====================================================================================
BM_TEST (svf_sections_match_analytic_response)
{
    using bm::SectionType;
    const SectionType types[] = { SectionType::LowPass, SectionType::HighPass, SectionType::BandPass, SectionType::Notch,
                                  SectionType::Bell, SectionType::LowShelf, SectionType::HighShelf,
                                  SectionType::LowPass1, SectionType::HighPass1, SectionType::LowShelf1, SectionType::HighShelf1 };
    double worst = 0;
    for (auto t : types)
        for (double fc : { 60.0, 1000.0, 9000.0 })
        {
            const auto c = bm::SvfCoeffs::design (t, fc, 1.3, 6.0, kFs);
            for (double f : { 40.0, 200.0, 1000.0, 5000.0, 15000.0 })
            {
                if (t == SectionType::Notch && std::abs (f - fc) < 1) continue;
                bm::SvfState st[2];
                const double measured = measureGainDb ([&] (float* const* d, int ch, int n) {
                    for (int c2 = 0; c2 < ch; ++c2) for (int i = 0; i < n; ++i) d[c2][i] = st[c2].process (c, d[c2][i]);
                }, f, kFs);
                const double expected = 20 * std::log10 (std::abs (c.response (f, kFs)));
                if (expected > -60) worst = std::max (worst, std::abs (measured - expected));
            }
        }
    REPORT ("worst |measured - analytic| over 11 types x 3 fc x 5 f = %.4f dB", worst);
    CHECK (worst < 0.02);
}

BM_TEST (eq_kalox_frequency_response)
{
    const auto p = bm::presets::kaloxOriginal();
    // static response of the preset bands (dynamic bands at rest: 0 dB)
    auto total = [&] (double f) { double s = 0; for (auto& b : p.eq.bands) s += bm::bandMagnitudeDb (b, f, kFs); return s; };
    auto band = [&] (int i, double f) { return bm::bandMagnitudeDb (p.eq.bands[(size_t) i], f, kFs); };

    CHECK_NEAR (band (0, 25.0), -3.01, 0.05);          // 4th-order Butterworth low cut at fc
    CHECK_NEAR (band (0, 12.5), -24.1, 0.3);           // one octave below: ~-24 dB
    CHECK_NEAR (band (0, 100.0), 0.0, 0.01);
    CHECK_NEAR (band (1, 250.0), -1.0, 0.001);         // bell -1 dB @ 250 Hz
    CHECK_NEAR (band (4, 10000.0), 0.5, 0.05);         // 2nd-order shelf: half gain at fc
    CHECK_NEAR (band (4, 20000.0), 1.0, 0.12);         // full +1 dB at the top
    CHECK_NEAR (band (2, 861.41), 0.0, 1e-6);          // dynamic bands 0 dB static
    REPORT ("preset curve: 25 Hz %.2f | 50 Hz %.2f | 250 Hz %.2f | 1 kHz %.2f | 10 kHz %.2f | 16 kHz %.2f dB",
            total (25), total (50), total (250), total (1000), total (10000), total (16000));

    // processed vs analytic (static bands only, dynamics disabled)
    auto q = p; q.eq.bands[2].dynOn = q.eq.bands[3].dynOn = false;
    bm::MasterEQ eq; eq.prepare (kFs, 256); eq.setParams (q.eq); eq.reset();
    double worst = 0;
    for (double f : { 20.0, 25.0, 40.0, 120.0, 250.0, 900.0, 3000.0, 10000.0, 16000.0 })
    {
        eq.reset();
        const double m = measureGainDb ([&] (float* const* d, int ch, int n) { eq.process (d, ch, n); }, f, kFs);
        double e = 0; for (auto& b : q.eq.bands) e += bm::bandMagnitudeDb (b, f, kFs);
        worst = std::max (worst, std::abs (m - e));
    }
    REPORT ("processed EQ vs analytic curve, worst deviation = %.4f dB", worst);
    CHECK (worst < 0.02);
}

BM_TEST (eq_stability_under_extreme_modulation)
{
    bm::MasterEQ eq; eq.prepare (kFs, 64); eq.reset();
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> u (0, 1);
    const int n = (int) kFs * 10;
    Buffer b = makeBuffer (2, n);
    std::normal_distribution<float> nd (0, 0.3f);
    for (auto& ch : b) for (auto& x : ch) x = nd (rng);
    double maxAbs = 0; bool finite = true;
    for (int s = 0; s < n; s += 64)
    {
        bm::EqParams p;
        for (auto& band : p.bands)
        {
            band.enabled = u (rng) > 0.1f;
            band.type = (bm::BandType) (int) (u (rng) * 6.999f);
            band.freq = 10.0f * std::pow (2400.0f, u (rng));       // 10 Hz .. 24 kHz
            band.gainDb = -24 + 48 * u (rng);
            band.q = 0.025f * std::pow (1600.0f, u (rng));        // 0.025 .. 40
            band.slopeIdx = (int) (u (rng) * 5.999f);
            band.placement = (bm::Placement) (int) (u (rng) * 4.999f);
            band.dynOn = u (rng) > 0.5f; band.dynRangeDb = -12 + 24 * u (rng); band.dynThresholdDb = -60 * u (rng);
        }
        eq.setParams (p);
        float* d[2] = { b[0].data() + s, b[1].data() + s };
        eq.process (d, 2, 64);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 64; ++i) { finite &= std::isfinite (d[c][i]); maxAbs = std::max (maxAbs, (double) std::abs (d[c][i])); }
    }
    // after the modulation stops: 3 s of silence with the last (static) settings -> must decay
    double tailPeak = 1;
    {
        Buffer z = makeBuffer (2, (int) kFs * 3);
        for (int s = 0; s < (int) z[0].size(); s += 64) { float* d[2] = { z[0].data() + s, z[1].data() + s }; eq.process (d, 2, 64); }
        tailPeak = std::max (peakAbs (z[0], (int) kFs * 2), peakAbs (z[1], (int) kFs * 2));
        finite &= std::isfinite (tailPeak);
    }
    REPORT ("10 s white noise, all bands randomised every 64 samples (5 x +-24 dB, Q 0.025..40): max |y| = %.1f, finite = %d, residual after 2 s silence = %.2e",
            maxAbs, (int) finite, tailPeak);
    CHECK (finite);
    CHECK (maxAbs < 1.0e6);      // 5 cascaded +24 dB bands = +120 dB theoretical maximum
    CHECK (tailPeak < 1e-4 * std::max (1.0, maxAbs));

    // impulse responses decay (energy strictly decreasing) for every type at extreme settings
    int decaying = 0, total = 0;
    for (int t = 0; t < (int) bm::BandType::NumTypes; ++t)
        for (float q : { 0.025f, 1.0f, 40.0f })
            for (float f : { 10.0f, 1000.0f, 23900.0f })
            {
                bm::MasterEQ e; e.prepare (kFs, 512);
                bm::EqParams p; p.bands[0].enabled = true; p.bands[0].type = (bm::BandType) t; p.bands[0].freq = f;
                p.bands[0].q = q; p.bands[0].gainDb = 24; p.bands[0].slopeIdx = 5;
                e.setParams (p); e.reset();
                Buffer imp = makeBuffer (2, 48000 * 8); imp[0][0] = imp[1][0] = 1.0f;
                for (int s = 0; s < (int) imp[0].size(); s += 512) { float* d[2] = { imp[0].data() + s, imp[1].data() + s }; e.process (d, 2, 512); }
                const double e1 = rms (imp[0], 48000 * 2, 48000), e2 = rms (imp[0], 48000 * 7, 48000);
                const bool ok = std::isfinite (e2) && (e2 < 1e-9 || e2 < 0.5 * e1);
                ++total; decaying += ok ? 1 : 0;
                if (! ok) REPORT ("type %d q %.3f f %.0f: rms 2-3 s %.3e, 7-8 s %.3e", t, q, f, e1, e2);
            }
    REPORT ("impulse responses decaying: %d / %d (types x Q {0.025, 1, 40} x f {10, 1k, 23.9k}, 48 dB/oct, +24 dB)", decaying, total);
    CHECK (decaying == total);
}

BM_TEST (eq_dynamic_band_reacts_to_level)
{
    bm::EqParams p; p.bands[0] = bm::presets::dynBand (861.41f, 0.0f, 1.622f, -1.0f, -30.0f);
    auto runAt = [&] (double ampDb) {
        bm::MasterEQ e; e.prepare (kFs, 256); e.setParams (p); e.reset();
        return measureGainDb ([&] (float* const* d, int ch, int n) {
            // scale up/down around the processing so measureGainDb sees a ratio
            const float g = (float) std::pow (10.0, ampDb / 20) / 0.1f;
            for (int c = 0; c < ch; ++c) for (int i = 0; i < n; ++i) d[c][i] *= g;
            e.process (d, ch, n);
            for (int c = 0; c < ch; ++c) for (int i = 0; i < n; ++i) d[c][i] /= g;
        }, 861.41, kFs);
    };
    const double quiet = runAt (-60), loud = runAt (-6);
    REPORT ("dynamic band (range -1 dB, thr -30 dB): gain @-60 dBFS = %.3f dB, @-6 dBFS = %.3f dB", quiet, loud);
    CHECK_NEAR (quiet, 0.0, 0.02);
    CHECK_NEAR (loud, -1.0, 0.1);
}

// =====================================================================================
// Oversampler
// =====================================================================================
BM_TEST (oversampler_flat_passband_integer_latency_and_image_rejection)
{
    for (int f = 1; f <= 4; ++f)
        for (int q = 0; q <= 1; ++q)
        {
            bm::Oversampler os; os.prepare (2, 256, 4); os.configure (f, q);
            const int lat = os.getLatency();
            // impulse -> peak at latency
            Buffer imp = makeBuffer (2, 4096); imp[0][100] = 1.0f;
            for (int s = 0; s < 4096; s += 256)
            {
                const float* in[2] = { imp[0].data() + s, imp[1].data() + s };
                os.processUp (in, 2, 256);
                float* out[2] = { imp[0].data() + s, imp[1].data() + s };
                os.processDown (out, 2, 256);
            }
            int pk = 0; for (int i = 0; i < 4096; ++i) if (std::abs (imp[0][(size_t) i]) > std::abs (imp[0][(size_t) pk])) pk = i;
            CHECK (pk == 100 + lat);
            // passband ripple at 1 kHz, 10 kHz, 19 kHz
            for (double fr : { 1000.0, 10000.0, 19000.0 })
            {
                os.reset();
                const double g = measureGainDb ([&] (float* const* d, int ch, int n) {
                    os.processUp (d, ch, n); os.processDown (d, ch, n); }, fr, 44100.0);
                CHECK_NEAR (g, 0.0, fr < 19500 ? 0.01 : 0.1);
            }
            if (q == 1) REPORT ("oversampling %2dx (high quality): latency %d samples", 1 << f, lat);
        }

    // image rejection: 18 kHz tone at 44.1k upsampled 2x, energy above 22.05 kHz
    bm::Oversampler os; os.prepare (1, 4096, 4); os.configure (1, 1);
    Buffer b = makeBuffer (1, 4096); sine (b, 18000, 0.5, 44100);
    const float* in[1] = { b[0].data() };
    float* const* up = os.processUp (in, 1, 4096);
    std::vector<float> u (up[0], up[0] + 8192);
    auto ps = powerSpectrum (u, 0, 8192);
    double sig = 0, img = 0;
    for (size_t k = 0; k < ps.size(); ++k) { const double fk = k * 88200.0 / 8192; if (fk < 22050) sig = std::max (sig, ps[k]); else img = std::max (img, ps[k]); }
    REPORT ("2x upsampling image rejection (18 kHz @44.1k): %.1f dB", 10 * std::log10 (img / sig));
    CHECK (10 * std::log10 (img / sig) < -100.0);
}

// =====================================================================================
// 4. Compressor behaviour
// =====================================================================================
BM_TEST (compressor_static_curve_matches_design)
{
    auto cp = bm::presets::kaloxOriginal().comp;
    cp.autoGain = false;
    double worst = 0;
    for (double lvl : { -40.0, -30.0, -24.0, -18.0, -12.0, -6.0, 0.0 })
    {
        bm::Compressor c; c.prepare (kFs, 256, 2); c.setParams (cp); c.reset();
        const double amp = std::pow (10.0, lvl / 20);
        const int n = (int) kFs * 2;
        Buffer b = makeBuffer (2, n); sine (b, 1000, amp, kFs);
        for (int s = 0; s < n; s += 256) { float* d[2] = { b[0].data() + s, b[1].data() + s }; c.process (d, 2, std::min (256, n - s)); }
        const double grMeasured = -db (toneAmplitude (b[0], 1000, kFs, n / 2, n / 2) / amp);
        const double grExpected = bm::CompCurve::gainReductionDb ((float) lvl, -18, 2, 20);
        worst = std::max (worst, std::abs (grMeasured - grExpected));
        REPORT ("in %6.1f dBFS -> GR measured %5.2f dB, design %5.2f dB", lvl, grMeasured, grExpected);
    }
    CHECK (worst < 0.35); // peak detector ripple on a 1 kHz sine
}

BM_TEST (compressor_attack_release_timing)
{
    auto cp = bm::presets::kaloxOriginal().comp; cp.autoGain = false; cp.kneeDb = 0;
    bm::Compressor c; c.prepare (kFs, 1, 2); c.setParams (cp); c.reset();
    // DC-like step (square at high level) from -40 dB to 0 dB for 1 s, then back
    const int n = (int) kFs * 2;
    std::vector<float> gr ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const float x = (i > n / 4 && i < n / 2) ? 1.0f : 0.01f;
        float l = x, r = x; float* d[2] = { &l, &r };
        c.process (d, 2, 1);
        gr[(size_t) i] = c.getGainReductionDb();
    }
    const float finalGr = gr[(size_t) (n / 2 - 1)];
    int tAtt = 0; for (int i = n / 4; i < n / 2; ++i) if (gr[(size_t) i] >= 0.632f * finalGr) { tAtt = i - n / 4; break; }
    int tRel = 0; for (int i = n / 2; i < n; ++i) if (gr[(size_t) i] <= 0.368f * finalGr) { tRel = i - n / 2; break; }
    REPORT ("final GR %.2f dB, attack(63%%) = %.1f ms (set 30), release(63%%) = %.1f ms (set 100)", finalGr, tAtt * 1000 / kFs, tRel * 1000 / kFs);
    CHECK_NEAR (finalGr, 9.0, 0.05);
    CHECK_NEAR (tAtt * 1000 / kFs, 30.0, 3.0);
    CHECK_NEAR (tRel * 1000 / kFs, 100.0, 8.0);
}

// =====================================================================================
// 5. Saturation: harmonic distortion and aliasing
// =====================================================================================
static std::pair<double, double> satThdAndAlias (const bm::SatParams& sp, double freq, double amp, double fs)
{
    bm::Saturation s; s.prepare (fs, 512, 2); s.setParams (sp); s.reset();
    const int n = 65536 + 16384;
    Buffer b = makeBuffer (2, n); sine (b, freq, amp, fs);
    for (int i = 0; i < n; i += 512) { float* d[2] = { b[0].data() + i, b[1].data() + i }; s.process (d, 2, std::min (512, n - i)); }
    auto ps = powerSpectrum (b[0], 16384, 65536);
    const double binHz = fs / 65536;
    double fund = 0, harm = 0, alias = 0;
    for (size_t k = 2; k < ps.size(); ++k)
    {
        const double fk = k * binHz;
        bool isHarm = false;
        for (int h = 1; h * freq < fs / 2; ++h) if (std::abs (fk - h * freq) < 6 * binHz) { isHarm = true; if (h == 1) fund += ps[k]; else harm += ps[k]; }
        if (! isHarm) alias = std::max (alias, ps[k]);
    }
    return { 10 * std::log10 (harm / fund + 1e-30), 10 * std::log10 (alias / fund + 1e-30) };
}

BM_TEST (saturation_harmonics_and_aliasing)
{
    auto sp = bm::presets::kaloxOriginal().sat;
    sp.mixPct = 100; sp.outputDb = 0;
    const auto clean = satThdAndAlias (sp, 1000, 0.5, 44100);
    REPORT ("Clean Tube 20%% drive, 1 kHz @ -6 dBFS: THD = %.1f dB", clean.first);
    CHECK (clean.first < -30.0 && clean.first > -80.0);

    auto off = sp; off.drivePct = 0;
    const auto t0 = satThdAndAlias (off, 1000, 0.5, 44100);
    REPORT ("Clean Tube 0%% drive: THD = %.1f dB", t0.first);
    CHECK (t0.first < clean.first);

    auto hot = sp; hot.drivePct = 90; hot.style = bm::SatStyle::SoftClip;
    hot.osFactorLog2 = 0; const auto a1 = satThdAndAlias (hot, 9000, 0.7, 44100);
    hot.osFactorLog2 = 2; const auto a4 = satThdAndAlias (hot, 9000, 0.7, 44100);
    hot.osFactorLog2 = 4; const auto a16 = satThdAndAlias (hot, 9000, 0.7, 44100);
    REPORT ("aliasing (soft clip 90%%, 9 kHz @44.1k, worst non-harmonic): 1x %.1f dB | 4x %.1f dB | 16x %.1f dB", a1.second, a4.second, a16.second);
    CHECK (a4.second < a1.second - 20.0);
    CHECK (a16.second < a4.second - 15.0);
    // preset case: Clean Tube 20 %, 4x, high-frequency content -> aliasing far below audibility
    auto pre = sp; const auto ap = satThdAndAlias (pre, 9000, 0.5, 44100);
    REPORT ("KALOX preset saturation (Clean Tube 20%%, 4x), 9 kHz @ -6 dBFS: worst alias %.1f dB", ap.second);
    CHECK (ap.second < -90.0);
}

// =====================================================================================
// 6. True peak + limiter
// =====================================================================================
BM_TEST (truepeak_meter_accuracy)
{
    // fs/4 sine with 45 degree phase: samples at 0.707, true peak 1.0 (EBU Tech 3341 style)
    for (double ampDb : { -6.0, 0.0 })
    {
        bm::LevelMeter m; m.prepare (kFs);
        Buffer b = makeBuffer (2, 48000); sine (b, kFs / 4, std::pow (10, ampDb / 20), kFs, kPi / 4);
        const float* d[2] = { b[0].data(), b[1].data() };
        m.process (d, 2, 48000);
        const double tp = db (m.getTruePeakMax()), sp = db (peakAbs (b[0]));
        REPORT ("fs/4 sine @%.1f dB: sample peak %.2f dBFS, true peak meter %.2f dBTP", ampDb, sp, tp);
        CHECK_NEAR (tp, ampDb, 0.2);
    }
    // 1 kHz and 10 kHz sines
    for (double f : { 997.0, 9973.0, 19000.0 })
    {
        bm::LevelMeter m; m.prepare (kFs);
        Buffer b = makeBuffer (2, 48000); sine (b, f, 0.5, kFs, 0.3);
        const float* d[2] = { b[0].data(), b[1].data() };
        m.process (d, 2, 2000);          // the abrupt start of the tone rings (a real overshoot)
        m.resetTruePeak();               // -> measure the steady tone only
        const float* d2[2] = { b[0].data() + 2000, b[1].data() + 2000 };
        m.process (d2, 2, 46000);
        REPORT ("%.0f Hz sine @ -6.02 dBFS: true peak meter %.3f dBTP", f, db (m.getTruePeakMax()));
        CHECK_NEAR (db (m.getTruePeakMax()), db (0.5), 0.05);
    }
}

BM_TEST (limiter_respects_true_peak_ceiling)
{
    struct Case { int os; bool tp; float la; bm::LimStyle st; float gain; };
    const Case cases[] = {
        { 4, true, 1.004f, bm::LimStyle::Transparent, 11 }, { 4, true, 0.0f, bm::LimStyle::Punchy, 18 },
        { 2, true, 5.0f, bm::LimStyle::Dynamic, 14 },       { 0, true, 1.0f, bm::LimStyle::Safe, 11 },
        { 0, false, 1.0f, bm::LimStyle::Transparent, 11 },  { 3, false, 2.0f, bm::LimStyle::Transparent, 20 },
    };
    for (const auto& cs : cases)
    {
        auto lp = bm::presets::kaloxOriginal().lim;
        lp.osFactorLog2 = cs.os; lp.truePeak = cs.tp; lp.lookaheadMs = cs.la; lp.style = cs.st; lp.inputGainDb = cs.gain;
        bm::Limiter lim; lim.prepare (kFs, 512, 2); lim.setParams (lp); lim.reset();
        Buffer b = musicLike (kFs, 4.0, 0.5, 3);
        const int n = (int) b[0].size();
        for (int s = 0; s < n; s += 512) { float* d[2] = { b[0].data() + s, b[1].data() + s }; lim.process (d, 2, std::min (512, n - s)); }
        const int skip = 4800;
        const double sp = db (std::max (peakAbs (b[0], skip), peakAbs (b[1], skip)));
        const double tp = db (std::max (referenceTruePeak (b[0], skip), referenceTruePeak (b[1], skip)));
        REPORT ("OS %2dx TP %-3s LA %.2f ms style %d gain +%.0f dB -> sample peak %.3f dBFS, ideal true peak %.3f dBTP, latency %d, safety clamps %lld",
                1 << cs.os, cs.tp ? "on" : "off", cs.la, (int) cs.st, cs.gain, sp, tp, lim.getLatency(), lim.getSafetyClampCount());
        CHECK (sp <= -0.1 + 1e-4);
        // documented accuracy: <= +0.05 dB with oversampling >= 4x, <= +0.35 dB at 1x
        // (residual comes from content right below Nyquist, see docs/TEST_REPORT.md)
        if (cs.tp) CHECK (tp <= -0.1 + (cs.os >= 2 ? 0.05 : 0.35));
    }
}

BM_TEST (limiter_true_peak_stress_near_nyquist)
{
    // pathological: white-noise hats with full energy up to 24 kHz at 48 kHz
    for (int os : { 4, 0 })
    {
        auto lp = bm::presets::kaloxOriginal().lim; lp.osFactorLog2 = os;
        bm::Limiter lim; lim.prepare (kFs, 512, 2); lim.setParams (lp); lim.reset();
        Buffer b = musicLike (kFs, 4.0, 0.5, 3, true);
        const int n = (int) b[0].size();
        for (int s = 0; s < n; s += 512) { float* d[2] = { b[0].data() + s, b[1].data() + s }; lim.process (d, 2, std::min (512, n - s)); }
        const double tp = db (std::max (referenceTruePeak (b[0], 4800), referenceTruePeak (b[1], 4800)));
        REPORT ("stress (energy up to Nyquist) OS %2dx: ideal true peak %.3f dBTP (ceiling -0.1)", 1 << os, tp);
        CHECK (tp <= -0.1 + 0.35);
    }
}

BM_TEST (limiter_transparent_below_ceiling)
{
    auto lp = bm::presets::kaloxOriginal().lim; lp.inputGainDb = 0;
    bm::Limiter lim; lim.prepare (kFs, 512, 2); lim.setParams (lp); lim.reset();
    Buffer b = makeBuffer (2, 48000); sine (b, 1000, 0.25, kFs);
    Buffer ref = b;
    for (int s = 0; s < 48000; s += 512) { float* d[2] = { b[0].data() + s, b[1].data() + s }; lim.process (d, 2, std::min (512, 48000 - s)); }
    const int L = lim.getLatency();
    double err = 0; for (int i = 10000; i < 40000; ++i) err = std::max (err, (double) std::abs (b[0][(size_t) i] - ref[0][(size_t) (i - L)]));
    REPORT ("limiter latency %d samples (%.2f ms) at 16x; max deviation on -12 dBFS sine = %.2e", L, L * 1000 / kFs, err);
    CHECK (err < 1e-4);
}

// =====================================================================================
// Loudness (EBU R128 / BS.1770-4)
// =====================================================================================
BM_TEST (loudness_kweighting_and_ebu_cases)
{
    bm::Biquad s, h; bm::LoudnessMeter::designKWeighting (48000, s, h);
    CHECK_NEAR (s.b0, 1.53512485958697, 1e-7); CHECK_NEAR (s.b1, -2.69169618940638, 1e-7);
    CHECK_NEAR (s.b2, 1.19839281085285, 1e-7); CHECK_NEAR (s.a1, -1.69065929318241, 1e-7);
    CHECK_NEAR (s.a2, 0.73248077421585, 1e-7);
    CHECK_NEAR (h.a1, -1.99004745483398, 1e-7); CHECK_NEAR (h.a2, 0.99007225036621, 1e-7);

    // Tech 3341 case 1: 1 kHz stereo sine at -23 dBFS -> M, S, I = -23.0 LUFS
    {
        bm::LoudnessMeter m; m.prepare (kFs);
        Buffer b = makeBuffer (2, 48000 * 20); sine (b, 1000, std::pow (10, -23.0 / 20), kFs);
        const float* d[2] = { b[0].data(), b[1].data() };
        m.process (d, 2, (int) b[0].size());
        REPORT ("1 kHz @-23 dBFS: M %.2f S %.2f I %.2f LUFS", m.getMomentary(), m.getShortTerm(), m.getIntegrated());
        CHECK_NEAR (m.getIntegrated(), -23.0, 0.1);
        CHECK_NEAR (m.getMomentary(), -23.0, 0.1);
        CHECK_NEAR (m.getShortTerm(), -23.0, 0.1);
    }
    // Tech 3341 case 3-like gating: 10 s @-36, 60 s @-23, 10 s @-36 -> I = -23.0
    {
        bm::LoudnessMeter m; m.prepare (kFs);
        const int n = 48000 * 80;
        Buffer b = makeBuffer (2, n);
        sine (b, 1000, std::pow (10, -36.0 / 20), kFs, 0, 0, 480000);
        sine (b, 1000, std::pow (10, -23.0 / 20), kFs, 0, 480000, 48000 * 60);
        sine (b, 1000, std::pow (10, -36.0 / 20), kFs, 0, 48000 * 70, 480000);
        const float* d[2] = { b[0].data(), b[1].data() };
        m.process (d, 2, n);
        REPORT ("gating case (-36/-23/-36 dBFS): I %.2f LUFS", m.getIntegrated());
        CHECK_NEAR (m.getIntegrated(), -23.0, 0.1);
    }
    // 44.1 kHz works too
    {
        bm::LoudnessMeter m; m.prepare (44100);
        Buffer b = makeBuffer (2, 44100 * 10); sine (b, 1000, std::pow (10, -18.0 / 20), 44100);
        const float* d[2] = { b[0].data(), b[1].data() };
        m.process (d, 2, (int) b[0].size());
        CHECK_NEAR (m.getIntegrated(), -18.0, 0.1);
    }
}

// =====================================================================================
// 7. Latency + alignment of the full chain and bypass paths
// =====================================================================================
BM_TEST (chain_latency_reported_matches_measured)
{
    auto p = neutralChain();
    p.sat.osFactorLog2 = 2; p.comp.lookaheadMs = 5.0f; p.lim.osFactorLog2 = 4; p.lim.lookaheadMs = 1.004f;
    p.sat.mixPct = 100; // wet path through the oversampler, drive 0 on a tiny impulse = linear
    ChainRunner r (p);
    const int L = r.chain.getLatency();
    Buffer b = makeBuffer (2, 48000); b[0][1000] = b[1][1000] = 0.001f;
    r.run (b);
    int pk = 0; for (int i = 0; i < 48000; ++i) if (std::abs (b[0][(size_t) i]) > std::abs (b[0][(size_t) pk])) pk = i;
    REPORT ("reported latency %d samples (sat %d + comp %d + lim %d), measured impulse delay %d",
            L, r.chain.getModuleLatency (bm::kModSat), r.chain.getModuleLatency (bm::kModComp), r.chain.getModuleLatency (bm::kModLim), pk - 1000);
    CHECK (pk - 1000 == L);

    // master bypass = bit-exact delayed input (null test)
    auto q = bm::presets::kaloxOriginal(); q.masterBypass = true;
    ChainRunner rb (q);
    Buffer in = musicLike (kFs, 1.0, 0.3, 9), out = in;
    rb.run (out);
    const int Lb = rb.chain.getLatency();
    double err = 0; for (int i = Lb; i < (int) in[0].size(); ++i) err = std::max (err, (double) std::abs (out[0][(size_t) i] - in[0][(size_t) (i - Lb)]));
    REPORT ("KALOX preset latency %d samples (%.2f ms @48k); master-bypass null residual %.2e", Lb, Lb * 1000 / kFs, err);
    CHECK (err == 0.0);

    // each module bypassed keeps alignment
    for (int m = 0; m < bm::kNumModules; ++m)
    {
        auto pm = p; pm.moduleOn[m] = false;
        ChainRunner rm (pm);
        Buffer bi = makeBuffer (2, 48000); bi[0][1000] = bi[1][1000] = 0.001f;
        rm.run (bi);
        int k = 0; for (int i = 0; i < 48000; ++i) if (std::abs (bi[0][(size_t) i]) > std::abs (bi[0][(size_t) k])) k = i;
        CHECK (k - 1000 == rm.chain.getLatency());
        CHECK (rm.chain.getLatency() == L);
    }
}

// =====================================================================================
// 8. Automation, 10. click-free bypass, 14. NaN / Inf
// =====================================================================================
BM_TEST (automation_random_parameters_stays_finite_and_under_ceiling)
{
    auto base = bm::presets::kaloxOriginal();
    ChainRunner r (base, kFs, 128);
    Buffer b = musicLike (kFs, 8.0, 0.4, 5);
    std::mt19937 rng (11);
    std::uniform_real_distribution<float> u (0, 1);
    r.run (b, [&] (int, bm::MasterChain& c) {
        auto p = base;
        p.inputGainDb = -6 + 12 * u (rng);
        for (auto& band : p.eq.bands) { band.gainDb = -12 + 24 * u (rng); band.freq *= 0.5f + u (rng); band.q *= 0.5f + u (rng); }
        p.sat.drivePct = 100 * u (rng); p.sat.mixPct = 100 * u (rng); p.sat.tone = -100 + 200 * u (rng);
        p.sat.dynamics = -100 + 200 * u (rng); p.sat.feedbackPct = 100 * u (rng); p.sat.feedbackFreq = 100 + 10000 * u (rng);
        p.comp.thresholdDb = -40 * u (rng); p.comp.ratio = 1 + 9 * u (rng); p.comp.attackMs = 0.1f + 100 * u (rng);
        p.comp.releaseMs = 10 + 500 * u (rng); p.comp.mixPct = 100 * u (rng); p.comp.autoRelease = u (rng) > 0.5f;
        p.lim.inputGainDb = 18 * u (rng); p.lim.attackMs = 200 * u (rng); p.lim.releaseMs = 1 + 1000 * u (rng);
        p.oneMasterOn = u (rng) > 0.5f; p.oneMasterPct = 200 * u (rng);
        c.setParams (p);
    });
    bool finite = true; for (auto& ch : b) for (float x : ch) finite &= std::isfinite (x);
    const double sp = db (std::max (peakAbs (b[0], 4800), peakAbs (b[1], 4800)));
    REPORT ("8 s, every parameter randomised every 128 samples: finite = %d, sample peak %.3f dBFS", (int) finite, sp);
    CHECK (finite);
    CHECK (sp <= -0.1 + 1e-4);
}

BM_TEST (bypass_toggles_are_click_free)
{
    // A click is a discontinuity: it shows up as a spike of the 2nd difference.
    // Each toggle type is measured separately against the steady-state value.
    const auto p = bm::presets::kaloxOriginal();
    const char* names[] = { "master bypass", "EQ bypass", "SAT bypass", "COMP bypass", "LIM bypass",
                            "SAT oversampling 4x->1x", "LIM oversampling 16x->4x", "LIM look-ahead 1->3 ms", "COMP look-ahead 0->5 ms" };
    for (int which = 0; which < 9; ++which)
    {
        bm::MasterChain ch; ch.prepare (kFs, 256, 2); ch.setParams (p); ch.reset();
        Buffer b = makeBuffer (2, (int) kFs * 3); sine (b, 110, 0.2, kFs);
        const int n = (int) b[0].size();
        int blk = 0, toggleAt = 0;
        for (int s = 0; s < n; s += 256)
        {
            auto q = p;
            const bool on = blk >= 200 && blk < 300;
            if (blk == 200) toggleAt = s;
            if (on)
            {
                switch (which)
                {
                    case 0: q.masterBypass = true; break;
                    case 1: case 2: case 3: case 4: q.moduleOn[which - 1] = false; break;
                    case 5: q.sat.osFactorLog2 = 0; break;
                    case 6: q.lim.osFactorLog2 = 2; break;
                    case 7: q.lim.lookaheadMs = 3.0f; break;
                    default: q.comp.lookaheadMs = 5.0f; break;
                }
            }
            ch.setParams (q);
            float* d[2] = { b[0].data() + s, b[1].data() + s };
            ch.process (d, 2, std::min (256, n - s));
            ++blk;
        }
        double steady = 0, after = 0;
        for (int i = (int) kFs / 2; i < n; ++i)
        {
            const double d2 = std::abs (b[0][(size_t) i] - 2 * b[0][(size_t) i - 1] + b[0][(size_t) i - 2]);
            if (i < toggleAt) steady = std::max (steady, d2); else after = std::max (after, d2);
        }
        REPORT ("%-26s on/off: max |2nd diff| %.2e vs steady %.2e (x%.2f)", names[which], after, steady, after / steady);
        CHECK (after < steady * 1.5);
    }
}

BM_TEST (nan_inf_and_extreme_inputs)
{
    ChainRunner r (bm::presets::kaloxOriginal());
    const int n = 48000 * 2;
    Buffer b = makeBuffer (2, n);
    for (int i = 0; i < n; ++i)
    {
        float x;
        switch ((i / 4800) % 6)
        {
            case 0: x = 0.0f; break;                                         // silence
            case 1: x = 1.0f; break;                                         // DC full scale
            case 2: x = (i % 2) ? 1.0f : -1.0f; break;                       // Nyquist square
            case 3: x = (i % 97 == 0) ? NAN : 0.5f * std::sin (i * 0.1f); break;
            case 4: x = (i % 101 == 0) ? INFINITY : -0.5f; break;
            default: x = (i % 7) ? 8.0f : -8.0f; break;                      // +18 dBFS hot input
        }
        b[0][(size_t) i] = x; b[1][(size_t) i] = -x;
    }
    r.run (b);
    bool finite = true; for (auto& ch : b) for (float x : ch) finite &= std::isfinite (x);
    CHECK (finite);
    CHECK (db (std::max (peakAbs (b[0]), peakAbs (b[1]))) <= -0.1 + 1e-4);
    // the chain recovers: a normal signal afterwards is processed normally
    Buffer c = musicLike (kFs, 1.0, 0.3);
    r.run (c);
    bool fin2 = true; for (auto& ch : c) for (float x : ch) fin2 &= std::isfinite (x);
    CHECK (fin2 && rms (c[0], 24000) > 0.01);
}

// =====================================================================================
// ONE MASTER macro
// =====================================================================================
BM_TEST (one_master_is_monotonic_and_safe)
{
    auto base = bm::presets::kaloxOriginal(); base.oneMasterOn = true;
    // 100 % == settings as dialled
    {
        auto p = base; p.oneMasterPct = 100;
        auto e = bm::applyOneMaster (p);
        CHECK (e.lim.inputGainDb == base.lim.inputGainDb && e.comp.ratio == base.comp.ratio && e.sat.drivePct == base.sat.drivePct);
    }
    {
        auto p = base; p.oneMasterPct = 0;
        auto e = bm::applyOneMaster (p);
        CHECK (e.comp.ratio == 1.0f && e.sat.drivePct == 0.0f && e.lim.inputGainDb == 0.0f && e.eq.bands[1].gainDb == 0.0f);
        CHECK (e.lim.ceilingDb == base.lim.ceilingDb && e.outputGainDb == base.outputGainDb);
    }
    double prev = -1e9;
    for (float pct : { 0.f, 50.f, 100.f, 150.f, 200.f })
    {
        auto p = base; p.oneMasterPct = pct;
        ChainRunner r (p);
        Buffer b = musicLike (kFs, 6.0, 0.15, 21);
        r.run (b);
        const double I = r.chain.getLoudness().getIntegrated();
        const double tp = db (std::max (referenceTruePeak (b[0], 4800), referenceTruePeak (b[1], 4800)));
        REPORT ("ONE MASTER %3.0f%%: integrated %.2f LUFS, true peak %.2f dBTP, limiter drive %.2f dB",
                pct, I, tp, bm::applyOneMaster (p).lim.inputGainDb);
        CHECK (I > prev);
        CHECK (tp <= -0.1 + 0.05);
        prev = I;
    }
}

// =====================================================================================
// 1. Real audio + 11. CPU
// =====================================================================================
BM_TEST (kalox_preset_processes_real_audio_and_cpu_budget)
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        auto p = bm::presets::kaloxOriginal();
        ChainRunner r (p, fs, 512);
        const double seconds = 20.0;
        Buffer b = musicLike (fs, seconds, 0.12, 4);
        const double inRms = rms (b[0]);
        const double t0 = nowSeconds();
        r.run (b);
        const double el = nowSeconds() - t0;
        const double outI = r.chain.getLoudness().getIntegrated();
        const double tp = db (std::max (referenceTruePeak (b[0], (int) fs), referenceTruePeak (b[1], (int) fs)));
        REPORT ("fs %.0f: %.0f s of audio in %.3f s -> %.2f%% of one core (realtime x%.0f) | in RMS %.1f dBFS -> out %.2f LUFS-I, %.2f dBTP, limiter GR %.1f dB, comp GR %.1f dB, lim OS %dx",
                fs, seconds, el, 100 * el / seconds, seconds / el, db (inRms), outI, tp,
                r.chain.getLimGainReduction(), r.chain.getCompGainReduction(), r.chain.getLimiterOsFactor());
        CHECK (tp <= -0.1 + 0.05);
        CHECK (outI > -20 && outI < -3);
       #ifdef NDEBUG
        CHECK (el / seconds < 0.30 * fs / 48000.0);
       #endif
    }
}

int main (int argc, char** argv)
{
    std::string filter = argc > 1 ? argv[1] : "";
    std::printf ("BANKMASTER DSP tests (%s, %s)\n",
                #if defined(__aarch64__) || defined(_M_ARM64)
                 "arm64",
                #elif defined(__x86_64__) || defined(_M_X64)
                 "x86_64",
                #else
                 "unknown-arch",
                #endif
                #ifdef NDEBUG
                 "release"
                #else
                 "debug"
                #endif
                );
    int run = 0;
    for (auto& t : registry())
    {
        if (! filter.empty() && t.name.find (filter) == std::string::npos) continue;
        currentTest() = t.name;
        const int before = failures();
        const double t0 = nowSeconds();
        std::printf ("[ RUN  ] %s\n", t.name.c_str());
        std::fflush (stdout);
        t.fn();
        std::printf ("[ %s ] %s (%.2f s)\n", failures() == before ? " OK " : "FAIL", t.name.c_str(), nowSeconds() - t0);
        ++run;
    }
    std::printf ("\n%d tests, %d checks, %d failures\n", run, checks(), failures());
    return failures() == 0 ? 0 : 1;
}
