/**
 * @file ghost_oracle_harness.h
 * @brief The independent lag generators, the E2E ghost-removal metrics and the controls of the ghost oracle harness
 *        (QA-A-225, #241), shared by test_ghost_oracle.cpp (the library under test) and test_ghost_mix_compare.cpp (the
 *        allocation-failure executable that has the test-only seams). See test_ghost_oracle.cpp for what the harness asserts
 *        and what it only measures and prints, and for the independence rules (the generators do not use the module's formulas).
 */
#pragma once

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <random>
#include <string>
#include <vector>

namespace ghost_oracle {

constexpr uint32_t W = 16, H = 16;
constexpr size_t N = static_cast<size_t>(W) * H;
constexpr double kSat = 65535.0;
constexpr double kEps = 1e-12;

using Frame = std::vector<float>;
using Frames = std::vector<Frame>;

// Starman 2012 eq. (1): h(k) = b0*delta(k) + sum_n b_n exp(-a_n k), with the sum of ALL b (b0 included) equal to 1.
const double kA[4] = {2.5e-3, 2.1e-2, 1.6e-1, 7.6e-1};   // per frame
const double kB[4] = {7.1e-6, 1.1e-4, 1.7e-3, 1.8e-2};
inline double sumB() { return kB[0] + kB[1] + kB[2] + kB[3]; }

// ---- generators (independent of the module) ------------------------------------------------------------------

/** T1: y(k) = sum_j x(k-j) h(j); h(0) = b0 + sum b_n = 1, h(j>0) = sum b_n exp(-a_n j). Driven by the TRUE exposure x. */
inline std::vector<double> truthLti(const std::vector<double>& x) {
    std::vector<double> y(x.size());
    double s[4] = {0, 0, 0, 0};   // s_n(k) = sum_{j>=1} exp(-a_n j) x(k-j)
    const double b0 = 1.0 - sumB();
    for (size_t k = 0; k < x.size(); ++k) {
        double v = x[k] * (b0 + sumB());
        for (int n = 0; n < 4; ++n) v += kB[n] * s[n];
        y[k] = v;
        for (int n = 0; n < 4; ++n) s[n] = std::exp(-kA[n]) * (s[n] + x[k]);
    }
    return y;
}

/** T2: trap class n fills with efficiency b_n while empty capacity remains and releases at 1-exp(-a_n) per frame. */
inline std::vector<double> truthTrap(const std::vector<double>& x) {
    const double kQ[4] = {300.0, 400.0, 600.0, 800.0};
    std::vector<double> y(x.size());
    double q[4] = {0, 0, 0, 0};
    for (size_t k = 0; k < x.size(); ++k) {
        double release = 0.0;
        for (int n = 0; n < 4; ++n) {
            const double rel = q[n] * (1.0 - std::exp(-kA[n]));
            q[n] -= rel;
            release += rel;
        }
        for (int n = 0; n < 4; ++n) {
            const double fill = std::max(x[k] * kB[n] * (1.0 - q[n] / kQ[n]), 0.0);
            q[n] += fill;
        }
        y[k] = x[k] + release;
    }
    return y;
}

/** The CONTROL that is not independent: a lag made with the module's own recursion (what tier 1 inverts exactly). */
inline std::vector<double> selfModel(const std::vector<double>& x, double a1, double t1, double a2, double t2) {
    const double d1 = std::exp(-1.0 / t1), d2 = std::exp(-1.0 / t2);
    std::vector<double> y(x.size());
    double h1 = 0.0, h2 = 0.0;
    for (size_t k = 0; k < x.size(); ++k) {
        y[k] = x[k] + a1 * h1 + a2 * h2;
        h1 = d1 * h1 + y[k];
        h2 = d2 * h2 + y[k];
    }
    return y;
}

struct Seq {
    std::vector<double> x;       // the true exposure, one value per frame
    size_t nPre{0}, nExp{0}, nBlank{0};
    double level{0.0};           // fraction of saturation during the exposure
};

inline Seq exposureSeq(double level) {
    Seq s;
    s.nPre = 5; s.nExp = 200; s.nBlank = 50; s.level = level;
    s.x.assign(s.nPre, 0.0);
    s.x.insert(s.x.end(), s.nExp, level * kSat);
    s.x.insert(s.x.end(), s.nBlank, 0.0);
    return s;
}

/** 27 % for nHi frames, then a lower non-zero level: the falling step the zero clamp cannot hide. */
inline Seq fallingStepSeq(double hi, double lo, size_t nHi, size_t nLo) {
    Seq s;
    s.nPre = 0; s.nExp = nHi; s.nBlank = nLo; s.level = hi;
    s.x.assign(nHi, hi * kSat);
    s.x.insert(s.x.end(), nLo, lo * kSat);
    return s;
}

inline Frames makeFrames(const std::vector<double>& y, double sigma, unsigned seed) {
    std::mt19937 g(seed);
    std::normal_distribution<double> noise(0.0, sigma > 0.0 ? sigma : 1.0);
    Frames out;
    out.reserve(y.size());
    for (double v : y) {
        Frame f(N);
        for (auto& p : f) p = static_cast<float>(sigma > 0.0 ? v + noise(g) : v);
        out.push_back(std::move(f));
    }
    return out;
}

inline double meanOf(const Frame& f) {
    double s = 0.0;
    for (float v : f) s += v;
    return s / static_cast<double>(f.size());
}

inline bool allFinite(const Frames& fs) {
    for (const auto& f : fs)
        for (float v : f)
            if (!std::isfinite(v)) return false;
    return true;
}

inline bool sameBytes(const Frames& a, const Frames& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].size() != b[i].size() || std::memcmp(a[i].data(), b[i].data(), a[i].size() * sizeof(float)) != 0) return false;
    return true;
}

// ---- the metrics (E2E protocol 5.6, with the blanks of QA-A-225 D1 filled by the proposals of the design memo) -------

struct Blank {
    double raw{0}, res{0}, removal{0};
    // QA-A-225 M3-a: the same removal with the dark reference taken from the UNCORRECTED pre-exposure frames, and the share of
    // the corrected blank frame's pixels that are exactly 0 (the module clamps its output at 0).
    double resRawDark{0}, removalRawDark{0}, clampedShare{0};
};
struct Metrics {
    Blank k1, k10, k50;
    double darkRaw{0}, darkCor{0};   // mean of the pre-exposure frames, uncorrected and corrected (the module clamps at 0)
    double retention{0};      // mean of the corrected exposure frames (from the 50th on) / the true exposure
    double rawRetention{0};   // the same on the uncorrected frames
};

/** LagResidualPct(k) = 100*|mean(Y_blank_k) - mean(DarkRef)| / max(mean(ExposureSignal), eps);
 *  GhostRemovalPct(k) = 100*(LagRawPct - LagResidualPct)/max(LagRawPct, eps).
 *  DarkRef = the pre-exposure frames of the SAME path; ExposureSignal = the last exposure frame before correction. */
inline Metrics measure(const Seq& s, const Frames& y, const Frames& c) {
    Metrics m;
    double darkRaw = 0.0, darkCor = 0.0;
    for (size_t i = 0; i < s.nPre; ++i) { darkRaw += meanOf(y[i]); darkCor += meanOf(c[i]); }
    if (s.nPre > 0) { darkRaw /= static_cast<double>(s.nPre); darkCor /= static_cast<double>(s.nPre); }
    const double exposureSignal = meanOf(y[s.nPre + s.nExp - 1]);
    auto blank = [&](size_t k) {
        const size_t i = s.nPre + s.nExp + k - 1;
        Blank b;
        b.raw = 100.0 * std::fabs(meanOf(y[i]) - darkRaw) / std::max(exposureSignal, kEps);
        b.res = 100.0 * std::fabs(meanOf(c[i]) - darkCor) / std::max(exposureSignal, kEps);
        b.removal = 100.0 * (b.raw - b.res) / std::max(b.raw, kEps);
        b.resRawDark = 100.0 * std::fabs(meanOf(c[i]) - darkRaw) / std::max(exposureSignal, kEps);
        b.removalRawDark = 100.0 * (b.raw - b.resRawDark) / std::max(b.raw, kEps);
        size_t zeros = 0;
        for (float v : c[i]) if (v == 0.0f) ++zeros;
        b.clampedShare = static_cast<double>(zeros) / static_cast<double>(c[i].size());
        return b;
    };
    m.darkRaw = darkRaw; m.darkCor = darkCor;
    m.k1 = blank(1); m.k10 = blank(10); m.k50 = blank(50);
    double sc = 0.0, sr = 0.0;
    size_t cnt = 0;
    for (size_t i = s.nPre + 49; i < s.nPre + s.nExp; ++i, ++cnt) { sc += meanOf(c[i]); sr += meanOf(y[i]); }
    m.retention = sc / static_cast<double>(cnt) / (s.level * kSat);
    m.rawRetention = sr / static_cast<double>(cnt) / (s.level * kSat);
    return m;
}

struct Step { double rawRes1{0}, corRes1{0}, rawRes50{0}, corRes50{0}; };

/** Signed residual of frame k after the step down, in percent of the NEW level: (mean(frame) - x) / x. No clamp can
 *  hide it, because the new level is not zero. */
inline Step measureStep(const Seq& s, const Frames& y, const Frames& c) {
    auto res = [&](const Frames& f, size_t k) {
        const size_t i = s.nExp + k - 1;
        return 100.0 * (meanOf(f[i]) - s.x[i]) / s.x[i];
    };
    Step st;
    st.rawRes1 = res(y, 1); st.corRes1 = res(c, 1); st.rawRes50 = res(y, 50); st.corRes50 = res(c, 50);
    return st;
}

// ---- correctors: the module under test and the controls ---------------------------------------------------------

using Corrector = std::function<Frames(const Frames&)>;

inline Frames runModule(const char* cfg, const Frames& in, int* failures) {
    void* h = nullptr;
    EXPECT_EQ(XPE_OK, xpe_ghost_create(W, H, cfg, &h));
    Frames out;
    out.reserve(in.size());
    XpeImageMetadata meta{};   // acquisitionTime is not used by the ghost corrector (QA-A-226b)
    for (const auto& f : in) {
        Frame c = f;
        XpeImageBuffer b{};
        b.data = c.data(); b.width = W; b.height = H; b.bitsAllocated = 32; b.bitsStored = 32;
        b.format = XPE_PIXEL_FLOAT32; b.dataSize = c.size() * sizeof(float);
        if (xpe_ghost_correct(h, &b, &meta) != XPE_OK) ++*failures;
        out.push_back(std::move(c));
    }
    xpe_ghost_destroy(h);
    return out;
}

inline std::string lagCfg(int tier, double a1, double t1, double a2, double t2) {
    char buf[200];
    std::snprintf(buf, sizeof(buf), "{\"tier\":%d,\"alpha1\":%g,\"tau1\":%g,\"alpha2\":%g,\"tau2\":%g}", tier, a1, t1, a2, t2);
    return buf;
}

struct Named { std::string name; Corrector fn; bool control; bool uncalibrated; };

struct Failures { int n{0}; };

inline std::vector<Named> correctors(const Seq& s, Failures* fail) {
    std::vector<Named> v;
    v.push_back({"control:none", [](const Frames& y) { return y; }, true, false});
    v.push_back({"control:oracle(true x)", [&s](const Frames&) { return makeFrames(s.x, 0.0, 0); }, true, false});
    v.push_back({"control:zeros", [](const Frames& y) { return Frames(y.size(), Frame(N, 0.0f)); }, true, false});
    v.push_back({"module:uncalibrated(no config)", [fail](const Frames& y) { return runModule(nullptr, y, &fail->n); }, false, true});
    const struct { const char* tag; double a1, t1, a2, t2; } sets[] = {
        {"0.02/3/0.003/30", 0.02, 3.0, 0.003, 30.0},
        {"0.1/1/0.01/20", 0.1, 1.0, 0.01, 20.0},
    };
    for (const auto& st : sets)
        for (int tier = 1; tier <= 3; ++tier) {
            const std::string cfg = lagCfg(tier, st.a1, st.t1, st.a2, st.t2);
            v.push_back({std::string("module:tier") + std::to_string(tier) + " " + st.tag,
                         [cfg, fail](const Frames& y) { return runModule(cfg.c_str(), y, &fail->n); }, false, false});
        }
    return v;
}

inline void printRow(const char* truth, const char* what, double sigma, const std::string& corrector, const Metrics& m) {
    std::printf("[ghost-oracle] %-4s %-7s sigma=%-3g %-34s retention=%8.4f (raw %.4f) | k=1 raw %.4f%% res %.4f%% removal %7.2f%% | "
                "k=10 removal %7.2f%% | k=50 raw %.4f%% res %.4f%% removal %7.2f%%\n",
                truth, what, sigma, corrector.c_str(), m.retention, m.rawRetention, m.k1.raw, m.k1.res, m.k1.removal,
                m.k10.removal, m.k50.raw, m.k50.res, m.k50.removal);
}

// ---- QA-A-225 M3: deriving a lag configuration from a CALIBRATION sequence (test side) ------------------------------
//
// The module has no calibration tool and no consumer for one (leader decision), so the derivation lives here. A configuration is
// searched on a calibration sequence (an exposure followed by dark frames, the true exposure known) and then judged on OTHER
// sequences: other exposure levels, the other truth, a falling step (the hold-out rule, design memo R5). The search uses a
// double-precision REPLICA of the module's tier-1 recursion on uniform frames; the replica is checked against the real module
// (test_ghost_oracle.cpp) and is used for nothing else.

struct Lag { double a1{0}, t1{1}, a2{0}, t2{1}; };

/** S = alpha1/(1-exp(-1/tau1)) + alpha2/(1-exp(-1/tau2)), a zero-alpha term counting as 0 (the module refuses S >= 1). */
inline double steadyGain(const Lag& l) {
    auto term = [](double a, double t) { return a == 0.0 ? 0.0 : a / (1.0 - std::exp(-1.0 / t)); };
    return term(l.a1, l.t1) + term(l.a2, l.t2);
}

inline bool lagValid(const Lag& l) { return l.a1 >= 0.0 && l.a2 >= 0.0 && l.t1 > 0.0 && l.t2 > 0.0 && steadyGain(l) < 1.0; }

inline std::string lagCfg(int tier, const Lag& l) { return lagCfg(tier, l.a1, l.t1, l.a2, l.t2); }

/** The module's tier 1 on uniform frames, in double precision: c = raw - a1*h1 - a2*h2, h <- decay*h + raw, clamp at 0
 *  (`clamp = false` gives the signed value, which the module never outputs: the derivation reads it, see deriveCost). */
inline std::vector<double> replicaTier1(const std::vector<double>& y, const Lag& l, bool clamp = true) {
    const double d1 = std::exp(-1.0 / l.t1), d2 = std::exp(-1.0 / l.t2);
    double h1 = 0.0, h2 = 0.0;
    std::vector<double> out(y.size());
    for (size_t k = 0; k < y.size(); ++k) {
        const double c = y[k] - l.a1 * h1 - l.a2 * h2;
        h1 = d1 * h1 + y[k];
        h2 = d2 * h2 + y[k];
        out[k] = (clamp && c < 0.0) ? 0.0 : c;
    }
    return out;
}

struct Band { double lo, hi; };   // the accepted range of the signal retention on the calibration sequence

/** Cost of a candidate on the calibration sequence: the mean |SIGNED corrected blank frame| over the blank frames, plus a
 *  penalty that is 0 while the retention (mean corrected exposure from the 50th frame / the true exposure) is inside `band`.
 *  The blank frames are read WITHOUT the clamp at 0: a first version used the clamped value, and every derived set then scored
 *  a blank residual of exactly 0 ADU by over-subtracting (the clamp hides it), so the search stopped at the first such set. */
inline double deriveCost(const Seq& s, const std::vector<double>& yMeans, const Lag& l, const Band& band) {
    if (!lagValid(l)) return 1e12;
    const std::vector<double> c = replicaTier1(yMeans, l, /*clamp=*/false);
    double sc = 0.0, sx = 0.0;
    for (size_t i = s.nPre + 49; i < s.nPre + s.nExp; ++i) { sc += c[i]; sx += s.x[i]; }
    const double r = sc / sx;
    const double pen = (r >= band.lo && r <= band.hi) ? 0.0 : 1e6 * std::fabs(r - 1.0);
    double blank = 0.0;
    for (size_t i = s.nPre + s.nExp; i < s.nPre + s.nExp + s.nBlank; ++i) blank += std::fabs(c[i]);
    return blank / static_cast<double>(s.nBlank) + pen;
}

/** The retention of a candidate on the calibration sequence (what the band constrains). */
inline double deriveRetention(const Seq& s, const std::vector<double>& yMeans, const Lag& l) {
    const std::vector<double> c = replicaTier1(yMeans, l, /*clamp=*/false);
    double sc = 0.0, sx = 0.0;
    for (size_t i = s.nPre + 49; i < s.nPre + s.nExp; ++i) { sc += c[i]; sx += s.x[i]; }
    return sc / sx;
}

/** A seeded random hill-climb in log space (7 scales x 6000 draws, tau >= 0.3). Deterministic for a given standard library. */
inline Lag deriveLag(const Seq& s, const std::vector<double>& yMeans, const Band& band, unsigned seed = 0) {
    std::mt19937 g(seed);
    std::normal_distribution<double> nd(0.0, 1.0);
    Lag best{0.02, 3.0, 0.02, 40.0};
    double bc = deriveCost(s, yMeans, best, band);
    for (const double scale : {1.0, 0.6, 0.3, 0.15, 0.07, 0.03, 0.01}) {
        for (int it = 0; it < 6000; ++it) {
            Lag q{best.a1 * std::exp(scale * nd(g)), best.t1 * std::exp(scale * nd(g)),
                  best.a2 * std::exp(scale * nd(g)), best.t2 * std::exp(scale * nd(g))};
            q.t1 = std::max(q.t1, 0.3);
            q.t2 = std::max(q.t2, 0.3);
            const double c = deriveCost(s, yMeans, q, band);
            if (c < bc) { best = q; bc = c; }
        }
    }
    return best;
}

inline std::vector<double> meansOf(const Frames& fs) {
    std::vector<double> m;
    m.reserve(fs.size());
    for (const auto& f : fs) m.push_back(meanOf(f));
    return m;
}

/** The module under a derived configuration, one corrector per tier (the tiers share the alpha/tau). */
inline std::vector<Named> derivedCorrectors(const std::string& label, const Lag& l, Failures* fail) {
    std::vector<Named> v;
    for (int tier = 1; tier <= 3; ++tier) {
        const std::string cfg = lagCfg(tier, l);
        v.push_back({"derived[" + label + "] tier" + std::to_string(tier),
                     [cfg, fail](const Frames& y) { return runModule(cfg.c_str(), y, &fail->n); }, false, false});
    }
    return v;
}

// ---- QA-A-225 M4: a digest of a fixed tier-3 run, to show the test seam is inert at its defaults -------------------------

/** FNV-1a over the bytes of every pixel of every frame. */
inline uint64_t digestOf(const Frames& fs) {
    uint64_t h = 1469598103934665603ull;
    for (const auto& f : fs) {
        unsigned char b[sizeof(float)];
        for (float v : f) {
            std::memcpy(b, &v, sizeof(float));
            for (unsigned char c : b) { h ^= c; h *= 1099511628211ull; }
        }
    }
    return h;
}

/** The digest of tier 3 (0.02/3/0.003/30) on the noisy T1 27 % exposure sequence (sigma 5, seed 7). The same call in the library
 *  executable and in the allocation-failure executable (seam at its defaults) must print the same value. */
inline uint64_t tierThreeDigest() {
    const Seq s = exposureSeq(0.27);
    const auto y = makeFrames(truthLti(s.x), 5.0, 7);
    int failures = 0;
    const std::string cfg = lagCfg(3, 0.02, 3.0, 0.003, 30.0);
    const Frames out = runModule(cfg.c_str(), y, &failures);
    return failures == 0 ? digestOf(out) : 0ull;
}

}  // namespace ghost_oracle
