/**
 * @file test_ghost_oracle.cpp
 * @brief An oracle harness for the ghost-removal metrics of SRS-CALIB-FUNC-020: independent lag generators, the E2E
 *        metrics, a signal-retention companion, a clamp-free falling-step measurement, and positive / negative
 *        controls (QA-A-225 M1, #241).
 *
 * WHAT THIS FILE ASSERTS AND WHAT IT DOES NOT. The module's numbers are MEASURED and PRINTED (`[ghost-oracle]` lines);
 * no threshold is asserted on them. Three things are undecided by the requirement text and belong to the user (QA-A-225
 * design memo, D1): which blank frame k carries the 90 %, what "lag is present" means, and the pass line of the
 * companion metric. What IS asserted is mechanical:
 *   - the generators satisfy their own definitions (closed forms, not the module's formulas);
 *   - the metric code gives the answers the controls must give (none = 0 %, oracle = 100 % with retention 1, a corrector
 *     that zeroes everything scores 100 % on the blank-frame metric AND 0 retention -- the reason the blank-frame metric
 *     cannot stand alone);
 *   - the harness is deterministic, every module call succeeds and every output is finite;
 *   - a handle without calibrated lag parameters (QA-A-226) leaves every frame untouched, so its removal is exactly 0 %.
 *
 * INDEPENDENCE (QA-A-225 R4). The generators below do not use the module's formulas or its alpha/tau. T1 is the
 * discrete-time LTI impulse response of Starman et al. 2012 (Med Phys 39:6035), eq. (1), driven by the TRUE exposure,
 * with the a_n / b_n TDS-CALIB-001 4.4.2 lists for the "Varex 4030CB". T2 is a finite-trap model: this project's own
 * formulation (NOT an equation of the paper), used for its trend only -- the lag of a fixed fraction shrinks as the
 * exposure grows (the direction of the paper's Table 3). The ONE place that is not independent is the self-model
 * control, on purpose: it shows that a lag made by the module's own recursion is "corrected" 100 %, which proves nothing
 * about the module.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "preprocess_state_fixture.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr uint32_t W = 16, H = 16;
constexpr size_t N = static_cast<size_t>(W) * H;
constexpr double kSat = 65535.0;
constexpr double kEps = 1e-12;

using Frame = std::vector<float>;
using Frames = std::vector<Frame>;

// Starman 2012 eq. (1): h(k) = b0*delta(k) + sum_n b_n exp(-a_n k), with the sum of ALL b (b0 included) equal to 1.
const double kA[4] = {2.5e-3, 2.1e-2, 1.6e-1, 7.6e-1};   // per frame
const double kB[4] = {7.1e-6, 1.1e-4, 1.7e-3, 1.8e-2};
double sumB() { return kB[0] + kB[1] + kB[2] + kB[3]; }

// ---- generators (independent of the module) ------------------------------------------------------------------

/** T1: y(k) = sum_j x(k-j) h(j); h(0) = b0 + sum b_n = 1, h(j>0) = sum b_n exp(-a_n j). Driven by the TRUE exposure x. */
std::vector<double> truthLti(const std::vector<double>& x) {
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
std::vector<double> truthTrap(const std::vector<double>& x) {
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
std::vector<double> selfModel(const std::vector<double>& x, double a1, double t1, double a2, double t2) {
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

Seq exposureSeq(double level) {
    Seq s;
    s.nPre = 5; s.nExp = 200; s.nBlank = 50; s.level = level;
    s.x.assign(s.nPre, 0.0);
    s.x.insert(s.x.end(), s.nExp, level * kSat);
    s.x.insert(s.x.end(), s.nBlank, 0.0);
    return s;
}

/** 27 % for nHi frames, then a lower non-zero level: the falling step the zero clamp cannot hide. */
Seq fallingStepSeq(double hi, double lo, size_t nHi, size_t nLo) {
    Seq s;
    s.nPre = 0; s.nExp = nHi; s.nBlank = nLo; s.level = hi;
    s.x.assign(nHi, hi * kSat);
    s.x.insert(s.x.end(), nLo, lo * kSat);
    return s;
}

Frames makeFrames(const std::vector<double>& y, double sigma, unsigned seed) {
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

double meanOf(const Frame& f) {
    double s = 0.0;
    for (float v : f) s += v;
    return s / static_cast<double>(f.size());
}

bool allFinite(const Frames& fs) {
    for (const auto& f : fs)
        for (float v : f)
            if (!std::isfinite(v)) return false;
    return true;
}

bool sameBytes(const Frames& a, const Frames& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].size() != b[i].size() || std::memcmp(a[i].data(), b[i].data(), a[i].size() * sizeof(float)) != 0) return false;
    return true;
}

// ---- the metrics (E2E protocol 5.6, with the blanks of QA-A-225 D1 filled by the proposals of the design memo) -------

struct Blank { double raw{0}, res{0}, removal{0}; };
struct Metrics {
    Blank k1, k10, k50;
    double retention{0};      // mean of the corrected exposure frames (from the 50th on) / the true exposure
    double rawRetention{0};   // the same on the uncorrected frames
};

/** LagResidualPct(k) = 100*|mean(Y_blank_k) - mean(DarkRef)| / max(mean(ExposureSignal), eps);
 *  GhostRemovalPct(k) = 100*(LagRawPct - LagResidualPct)/max(LagRawPct, eps).
 *  DarkRef = the pre-exposure frames of the SAME path; ExposureSignal = the last exposure frame before correction. */
Metrics measure(const Seq& s, const Frames& y, const Frames& c) {
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
        return b;
    };
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
Step measureStep(const Seq& s, const Frames& y, const Frames& c) {
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

Frames runModule(const char* cfg, const Frames& in, int* failures) {
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

std::string lagCfg(int tier, double a1, double t1, double a2, double t2) {
    char buf[200];
    std::snprintf(buf, sizeof(buf), "{\"tier\":%d,\"alpha1\":%g,\"tau1\":%g,\"alpha2\":%g,\"tau2\":%g}", tier, a1, t1, a2, t2);
    return buf;
}

struct Named { std::string name; Corrector fn; bool control; bool uncalibrated; };

struct Failures { int n{0}; };

std::vector<Named> correctors(const Seq& s, Failures* fail) {
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

void printRow(const char* truth, const char* what, double sigma, const std::string& corrector, const Metrics& m) {
    std::printf("[ghost-oracle] %-4s %-7s sigma=%-3g %-34s retention=%8.4f (raw %.4f) | k=1 raw %.4f%% res %.4f%% removal %7.2f%% | "
                "k=10 removal %7.2f%% | k=50 raw %.4f%% res %.4f%% removal %7.2f%%\n",
                truth, what, sigma, corrector.c_str(), m.retention, m.rawRetention, m.k1.raw, m.k1.res, m.k1.removal,
                m.k10.removal, m.k50.raw, m.k50.res, m.k50.removal);
}

class GhostOracle : public XpePreprocessStateFixture {
protected:
    void SetUp() override { XpePreprocessStateFixture::SetUp(); xpe_clear_alerts(); }
    void TearDown() override { xpe_clear_alerts(); XpePreprocessStateFixture::TearDown(); }
};

}  // namespace

// ---- the generators satisfy their own definitions ----------------------------------------------------------------

TEST_F(GhostOracle, TheLtiTruthSatisfiesItsDefinition) {
    // h(0) = b0 + sum b_n = 1 (the first frame of a lone exposure is not lagged)
    {
        std::vector<double> x(10, 0.0);
        x[3] = 1.0;
        const auto y = truthLti(x);
        EXPECT_NEAR(1.0, y[3], 1e-12);
        for (size_t k = 0; k < 3; ++k) EXPECT_EQ(0.0, y[k]) << "nothing before the exposure";
        // the tail of the impulse response: h(j) = sum b_n exp(-a_n j), positive and decreasing
        for (size_t j = 1; j < 6; ++j) {
            double want = 0.0;
            for (int n = 0; n < 4; ++n) want += kB[n] * std::exp(-kA[n] * static_cast<double>(j));
            EXPECT_NEAR(want, y[3 + j], 1e-12) << "h(" << j << ")";
            if (j > 1) EXPECT_LT(y[3 + j], y[3 + j - 1]);
        }
    }
    // steady state of a constant exposure: y_ss = x * (1 + sum b_n / (exp(a_n) - 1)) -- a closed form, not the generator's loop
    {
        std::vector<double> x(40000, 1000.0);
        const auto y = truthLti(x);
        double gain = 1.0;
        for (int n = 0; n < 4; ++n) gain += kB[n] / (std::exp(kA[n]) - 1.0);
        EXPECT_NEAR(1000.0 * gain, y.back(), 1000.0 * gain * 1e-6);
        EXPECT_GT(gain, 1.0);
    }
}

TEST_F(GhostOracle, TheTrapTruthLagShrinksRelativelyAsTheExposureGrows) {
    // T2's design claim (the direction of Starman's Table 3), asserted as a property of the generator and not of the module
    double prev = 1e9;
    for (const double level : {0.02, 0.27, 0.92}) {
        const Seq s = exposureSeq(level);
        const auto y = truthTrap(s.x);
        const double lag = 100.0 * std::fabs(y[s.nPre + s.nExp] - y[0]) / y[s.nPre + s.nExp - 1];
        std::printf("[ghost-oracle] T2 raw first blank frame lag at %2.0f%%: %.4f%% of the exposure signal\n", level * 100.0, lag);
        EXPECT_LT(lag, prev) << "level " << level;
        prev = lag;
    }
    // the LTI truth is level-independent in relative terms (what T2 adds)
    const auto a = exposureSeq(0.02), b = exposureSeq(0.92);
    const auto ya = truthLti(a.x), yb = truthLti(b.x);
    const double la = (ya[a.nPre + a.nExp] - ya[0]) / ya[a.nPre + a.nExp - 1];
    const double lb = (yb[b.nPre + b.nExp] - yb[0]) / yb[b.nPre + b.nExp - 1];
    std::printf("[ghost-oracle] T1 raw first blank frame lag: %.4f%% at 2%%, %.4f%% at 92%% (level-independent)\n", 100.0 * la, 100.0 * lb);
    EXPECT_NEAR(la, lb, 1e-9);
}

// ---- the metric code answers what the controls must answer ------------------------------------------------------

TEST_F(GhostOracle, TheControlsGiveTheAnswersTheyMust) {
    for (const bool trap : {false, true}) {
        for (const double level : {0.02, 0.27, 0.92}) {
            const Seq s = exposureSeq(level);
            const auto y = makeFrames(trap ? truthTrap(s.x) : truthLti(s.x), 0.0, 0);
            Failures f;
            const auto cs = correctors(s, &f);

            const Metrics none = measure(s, y, cs[0].fn(y));
            EXPECT_NEAR(0.0, none.k1.removal, 1e-9) << "no correction removes nothing";
            EXPECT_NEAR(0.0, none.k50.removal, 1e-9);
            EXPECT_NEAR(none.rawRetention, none.retention, 1e-12);

            const Metrics oracle = measure(s, y, cs[1].fn(y));
            EXPECT_NEAR(100.0, oracle.k1.removal, 1e-6) << "the true exposure leaves no lag";
            EXPECT_NEAR(100.0, oracle.k50.removal, 1e-6);
            EXPECT_NEAR(1.0, oracle.retention, 1e-6) << "and keeps the signal";

            // an offset added to every frame cancels against the dark reference of the SAME path: nothing removed, and the
            // retention moves by exactly the offset (this is the control that exercises DarkRef)
            Frames shifted = y;
            for (auto& frame : shifted) for (auto& px : frame) px += 100.0f;
            const Metrics off = measure(s, y, shifted);
            EXPECT_NEAR(0.0, off.k1.removal, 5e-3) << "level " << level;
            EXPECT_NEAR(0.0, off.k50.removal, 5e-3) << "level " << level;
            EXPECT_NEAR(none.retention + 100.0 / (level * kSat), off.retention, 1e-6);

            // LagRaw against the direct expression, with the exposure signal of the UNCORRECTED last exposure frame
            // (for the oracle the corrected frame differs from it, so this is where a wrong choice would show)
            {
                const auto yy = trap ? truthTrap(s.x) : truthLti(s.x);
                const double direct = 100.0 * std::fabs(yy[s.nPre + s.nExp]) / yy[s.nPre + s.nExp - 1];
                EXPECT_NEAR(direct, oracle.k1.raw, 1e-4) << "level " << level;
                // and the retention window (frames 50..end of the exposure) against the direct mean of the generator
                double sum = 0.0;
                size_t cnt = 0;
                for (size_t i = s.nPre + 49; i < s.nPre + s.nExp; ++i, ++cnt) sum += yy[i];
                EXPECT_NEAR(sum / static_cast<double>(cnt) / (level * kSat), none.retention, 1e-6) << "level " << level;
            }

            // the trap: a corrector that zeroes everything scores 100 % on the blank-frame metric and keeps NO signal
            const Metrics zeros = measure(s, y, cs[2].fn(y));
            EXPECT_NEAR(100.0, zeros.k1.removal, 1e-9);
            EXPECT_NEAR(100.0, zeros.k50.removal, 1e-9);
            EXPECT_EQ(0.0, zeros.retention) << "the companion metric is what tells it apart";
        }
    }
}

TEST_F(GhostOracle, TheStepMeasurementSeesWhatTheClampHides) {
    // 27 % -> 5 %: a corrector that zeroes everything leaves a residual of -100 % of the new level, no clamp can hide it
    const Seq s = fallingStepSeq(0.27, 0.05, 200, 100);
    const auto y = makeFrames(truthLti(s.x), 0.0, 0);
    Failures f;
    const auto cs = correctors(s, &f);
    const Step none = measureStep(s, y, cs[0].fn(y));
    EXPECT_GT(none.rawRes1, 0.0) << "the lag of the high level sits on top of the low one";
    EXPECT_NEAR(none.rawRes1, none.corRes1, 1e-12);
    const Step oracle = measureStep(s, y, cs[1].fn(y));
    EXPECT_NEAR(0.0, oracle.corRes1, 1e-6);
    EXPECT_NEAR(0.0, oracle.corRes50, 1e-6);
    const Step zeros = measureStep(s, y, cs[2].fn(y));
    EXPECT_NEAR(-100.0, zeros.corRes1, 1e-9);
    EXPECT_NEAR(-100.0, zeros.corRes50, 1e-9);
}

TEST_F(GhostOracle, ASelfModelLagIsCorrectedToTheLetterWhichProvesNothing) {
    // The positive control, on purpose NOT independent: the module's own recursion made the lag, the same configuration
    // inverts it exactly. Read it as "the metric code can say 100 %", never as evidence about the module.
    const Seq s = exposureSeq(0.27);
    const double a1 = 0.02, t1 = 3.0, a2 = 0.003, t2 = 30.0;
    const auto y = makeFrames(selfModel(s.x, a1, t1, a2, t2), 0.0, 0);
    int failures = 0;
    const auto same = runModule(lagCfg(1, a1, t1, a2, t2).c_str(), y, &failures);
    const Metrics m = measure(s, y, same);
    printRow("self", "27%", 0.0, "control:self-model, same config", m);
    EXPECT_EQ(0, failures);
    EXPECT_GE(m.k1.removal, 99.99);
    EXPECT_GE(m.k50.removal, 99.99);
    EXPECT_NEAR(1.0, m.retention, 1e-4);
    // a different configuration on the same data is NOT 100 %: the metric does discriminate
    const auto wrong = runModule(lagCfg(1, 0.01, 8.0, 0.001, 60.0).c_str(), y, &failures);
    const Metrics w = measure(s, y, wrong);
    printRow("self", "27%", 0.0, "control:self-model, other config", w);
    EXPECT_LT(w.k1.removal, 99.9);
    EXPECT_EQ(0, failures);
}

// ---- the module, measured (printed, not judged) ---------------------------------------------------------------

TEST_F(GhostOracle, MeasuresTheModuleAgainstTheIndependentTruths) {
    std::printf("[ghost-oracle] columns: truth T1 = Starman LTI, T2 = finite trap | level of saturation | sigma ADU | corrector | "
                "retention = corrected exposure mean (frames 50..200) / true exposure | LagRaw/LagResidual/GhostRemoval per blank frame k\n");
    Failures fail;
    int rows = 0;
    for (const bool trap : {false, true})
        for (const double level : {0.02, 0.27, 0.92})
            for (const double sigma : {0.0, 5.0}) {
                const Seq s = exposureSeq(level);
                const auto y = makeFrames(trap ? truthTrap(s.x) : truthLti(s.x), sigma, 7);
                for (const auto& c : correctors(s, &fail)) {
                    const Frames out = c.fn(y);
                    ASSERT_EQ(y.size(), out.size());
                    EXPECT_TRUE(allFinite(out)) << c.name;
                    const Metrics m = measure(s, y, out);
                    char lv[16];
                    std::snprintf(lv, sizeof(lv), "%.0f%%", level * 100.0);
                    printRow(trap ? "T2" : "T1", lv, sigma, c.name, m);
                    ++rows;
                    if (c.uncalibrated) {
                        // QA-A-226: no calibrated lag parameters, so no correction at all -- mechanical, not a threshold
                        EXPECT_TRUE(sameBytes(y, out)) << "an uncalibrated handle must leave every frame as it came";
                        EXPECT_NEAR(0.0, m.k1.removal, 1e-9);
                        EXPECT_NEAR(0.0, m.k50.removal, 1e-9);
                    }
                }
            }
    EXPECT_EQ(0, fail.n) << "every xpe_ghost_correct call must succeed";
    std::printf("[ghost-oracle] %d exposure rows\n", rows);
}

TEST_F(GhostOracle, MeasuresTheFallingStepWhereTheClampCannotHide) {
    std::printf("[ghost-oracle] falling step 27%% -> 5%% (200 + 100 frames): signed residual in %% of the NEW level, "
                "raw -> corrected, at the 1st and the 50th frame after the step\n");
    Failures fail;
    for (const bool trap : {false, true}) {
        const Seq s = fallingStepSeq(0.27, 0.05, 200, 100);
        const auto y = makeFrames(trap ? truthTrap(s.x) : truthLti(s.x), 0.0, 0);
        for (const auto& c : correctors(s, &fail)) {
            const Frames out = c.fn(y);
            EXPECT_TRUE(allFinite(out)) << c.name;
            const Step st = measureStep(s, y, out);
            auto removal = [](double raw, double cor) { return 100.0 * (1.0 - std::fabs(cor) / std::max(std::fabs(raw), kEps)); };
            std::printf("[ghost-oracle] %-4s step %-34s k=1 %+9.4f%% -> %+9.4f%% (removal %7.2f%%) | k=50 %+9.4f%% -> %+9.4f%% (removal %7.2f%%)\n",
                        trap ? "T2" : "T1", c.name.c_str(), st.rawRes1, st.corRes1, removal(st.rawRes1, st.corRes1),
                        st.rawRes50, st.corRes50, removal(st.rawRes50, st.corRes50));
            if (c.uncalibrated) {
                EXPECT_TRUE(sameBytes(y, out));
                EXPECT_EQ(st.rawRes1, st.corRes1);
            }
        }
    }
    EXPECT_EQ(0, fail.n);
}

TEST_F(GhostOracle, TheHarnessIsDeterministic) {
    const Seq s = exposureSeq(0.27);
    const auto y = makeFrames(truthLti(s.x), 5.0, 7);
    int failures = 0;
    const std::string cfg = lagCfg(3, 0.1, 1.0, 0.01, 20.0);
    const Metrics a = measure(s, y, runModule(cfg.c_str(), y, &failures));
    const Metrics b = measure(s, y, runModule(cfg.c_str(), y, &failures));
    EXPECT_EQ(0, failures);
    EXPECT_EQ(a.k1.removal, b.k1.removal);
    EXPECT_EQ(a.k50.removal, b.k50.removal);
    EXPECT_EQ(a.retention, b.retention);
    EXPECT_TRUE(sameBytes(makeFrames(truthLti(s.x), 5.0, 7), y)) << "the noise is seeded";
}
