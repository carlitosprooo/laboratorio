// BANKMASTER by KALOX — minimal test framework + measurement helpers.
// Measurement code here is deliberately independent from the plugin DSP
// (own FFT, own high-resolution true-peak reference).
#pragma once

#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <vector>

namespace bmtest
{
struct TestCase { std::string name; std::function<void()> fn; };
inline std::vector<TestCase>& registry() { static std::vector<TestCase> r; return r; }
inline int& failures() { static int f = 0; return f; }
inline int& checks() { static int c = 0; return c; }
inline std::string& currentTest() { static std::string s; return s; }

struct Registrar { Registrar (const char* n, std::function<void()> f) { registry().push_back ({ n, std::move (f) }); } };

#define BM_TEST(name) \
    static void name(); \
    static bmtest::Registrar reg_##name (#name, name); \
    static void name()

#define CHECK(cond) do { ++bmtest::checks(); if (! (cond)) { ++bmtest::failures(); \
    std::printf ("    FAIL [%s] %s:%d: %s\n", bmtest::currentTest().c_str(), __FILE__, __LINE__, #cond); } } while (0)

#define CHECK_NEAR(a, b, tol) do { ++bmtest::checks(); const double _a = (a), _b = (b); \
    if (! (std::abs (_a - _b) <= (tol))) { ++bmtest::failures(); \
    std::printf ("    FAIL [%s] %s:%d: %s = %.6f, expected %.6f +- %.6f\n", bmtest::currentTest().c_str(), __FILE__, __LINE__, #a, _a, _b, (double) (tol)); } } while (0)

#define REPORT(...) do { std::printf ("    | "); std::printf (__VA_ARGS__); std::printf ("\n"); } while (0)

constexpr double kPi = 3.14159265358979323846;

using Buffer = std::vector<std::vector<float>>; // [channel][sample]

inline Buffer makeBuffer (int ch, int n) { return Buffer ((size_t) ch, std::vector<float> ((size_t) n, 0.0f)); }

inline void sine (Buffer& b, double freq, double amp, double fs, double phase = 0.0, int start = 0, int len = -1)
{
    const int n = (int) b[0].size();
    if (len < 0) len = n - start;
    for (auto& ch : b)
        for (int i = start; i < start + len && i < n; ++i)
            ch[(size_t) i] = (float) (amp * std::sin (2 * kPi * freq * i / fs + phase));
}

inline double rms (const std::vector<float>& x, int start = 0, int len = -1)
{
    if (len < 0) len = (int) x.size() - start;
    double s = 0; for (int i = start; i < start + len; ++i) s += (double) x[(size_t) i] * x[(size_t) i];
    return std::sqrt (s / std::max (1, len));
}

inline double peakAbs (const std::vector<float>& x, int start = 0, int len = -1)
{
    if (len < 0) len = (int) x.size() - start;
    double p = 0; for (int i = start; i < start + len; ++i) p = std::max (p, (double) std::abs (x[(size_t) i]));
    return p;
}

inline double db (double g) { return 20.0 * std::log10 (std::max (g, 1e-30)); }

/** Amplitude of a frequency component (least-squares sine fit over a window). */
inline double toneAmplitude (const std::vector<float>& x, double freq, double fs, int start, int len)
{
    double sc = 0, ss = 0;
    for (int i = start; i < start + len; ++i)
    {
        const double w = 2 * kPi * freq * i / fs;
        sc += x[(size_t) i] * std::cos (w); ss += x[(size_t) i] * std::sin (w);
    }
    return 2.0 * std::sqrt (sc * sc + ss * ss) / len;
}

// ---- radix-2 FFT
inline void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2 * kPi / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1);
            for (size_t j = 0; j < len / 2; ++j)
            {
                auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v; a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

/** Blackman-Harris windowed power spectrum (linear power per bin), size = power of 2. */
inline std::vector<double> powerSpectrum (const std::vector<float>& x, int start, int size)
{
    std::vector<std::complex<double>> a ((size_t) size);
    double wsum = 0;
    for (int i = 0; i < size; ++i)
    {
        const double t = 2 * kPi * i / (size - 1);
        const double w = 0.35875 - 0.48829 * std::cos (t) + 0.14128 * std::cos (2 * t) - 0.01168 * std::cos (3 * t);
        a[(size_t) i] = x[(size_t) (start + i)] * w;
        wsum += w;
    }
    fft (a);
    std::vector<double> p ((size_t) size / 2);
    for (size_t k = 0; k < p.size(); ++k) p[k] = std::norm (a[k]) / (wsum * wsum) * 4.0;
    return p;
}

/** Independent "ideal" true-peak reference: exact band-limited interpolation by FFT
    zero-padding (16x) on overlapping 4096-sample blocks (only the centre half of each
    block is used, so block edges never contribute). */
inline double referenceTruePeak (const std::vector<float>& x, int start = 0, int len = -1)
{
    // the whole signal is used as context; only peaks inside [start, start+len) count
    // (cutting the signal would create a step whose Gibbs ringing is not a real peak).
    // By default the last 2048 samples are excluded for the same reason (the buffer
    // ends mid-signal).
    if (len < 0) len = std::max (0, (int) x.size() - start - 2048);
    const int N = 4096, U = 16, hop = N / 2;
    const int total = (int) x.size();
    std::vector<float> padded ((size_t) (total + 2 * N), 0.0f);
    std::copy (x.begin(), x.end(), padded.begin() + N);
    double pk = 0;
    std::vector<std::complex<double>> a ((size_t) N), b ((size_t) (N * U));
    for (int s = 0; s + N <= (int) padded.size(); s += hop)
    {
        const int c0 = s + N / 4 - N, c1 = s + 3 * N / 4 - N; // centre region in signal indices
        if (c1 <= start || c0 >= start + len) continue;
        for (int i = 0; i < N; ++i) a[(size_t) i] = padded[(size_t) (s + i)];
        fft (a);
        std::fill (b.begin(), b.end(), std::complex<double> (0));
        for (int k = 0; k < N / 2; ++k) { b[(size_t) k] = a[(size_t) k]; if (k) b[(size_t) (N * U - k)] = a[(size_t) (N - k)]; }
        b[(size_t) (N / 2)] = a[(size_t) (N / 2)] * 0.5; b[(size_t) (N * U - N / 2)] = a[(size_t) (N / 2)] * 0.5;
        for (auto& v : b) v = std::conj (v);
        fft (b);
        for (int i = N * U / 4; i < 3 * N * U / 4; ++i)
        {
            const double pos = s - N + i / (double) U;
            if (pos < start || pos >= start + len) continue;
            pk = std::max (pk, std::abs (b[(size_t) i].real()) / N);
        }
    }
    return pk;
}

/** Music-like test signal: kick-like low bursts + bass + noise hats + inter-sample-peak tones.
    brightHats = true leaves the hats unfiltered up to Nyquist (stress case). */
inline Buffer musicLike (double fs, double seconds, double level, unsigned seed = 1, bool brightHats = false)
{
    const int n = (int) (fs * seconds);
    Buffer b = makeBuffer (2, n);
    std::mt19937 rng (seed);
    std::normal_distribution<double> nd (0, 1);
    double lpL = 0, lpR = 0;
    double hl[4] {}, hr[4] {};
    const double a = std::exp (-2 * kPi * 15000.0 / fs); // 4 x one-pole low-pass @ 15 kHz
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs;
        const double beat = std::fmod (t, 0.5);           // 120 BPM
        const double kick = std::exp (-beat * 18) * std::sin (2 * kPi * (50 + 120 * std::exp (-beat * 40)) * beat);
        const double bass = 0.5 * std::sin (2 * kPi * 55 * t) * (0.6 + 0.4 * std::sin (2 * kPi * 0.25 * t));
        const double hatEnv = std::exp (-std::fmod (t + 0.25, 0.25) * 60);
        const double nL = nd (rng), nR = nd (rng);
        lpL = 0.6 * lpL + 0.4 * nL; lpR = 0.6 * lpR + 0.4 * nR;
        double hatL = (nL - lpL) * hatEnv * 0.25, hatR = (nR - lpR) * hatEnv * 0.25;
        if (! brightHats)
            for (int k = 0; k < 4; ++k)
            {
                hl[k] = (1 - a) * hatL + a * hl[k]; hatL = hl[k];
                hr[k] = (1 - a) * hatR + a * hr[k]; hatR = hr[k];
            }
        if (! brightHats) { hatL *= 2.0; hatR *= 2.0; }
        const double pad = 0.15 * std::sin (2 * kPi * 440 * t) + 0.1 * std::sin (2 * kPi * 11025.0 * t + 0.785);
        b[0][(size_t) i] = (float) (level * (0.9 * kick + bass + hatL + pad));
        b[1][(size_t) i] = (float) (level * (0.9 * kick + bass + hatR + 0.9 * pad));
    }
    return b;
}

inline double nowSeconds()
{
    return std::chrono::duration<double> (std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace bmtest
