/**
 * @file test_zz_a161_probe.cpp
 * @brief QA-A-161 (#143 #148) -- MEASUREMENT PROBE, not a gate.
 *
 * Every case here is DISABLED_ on purpose: it measures, it does not judge.
 * Run with:  xpe_preprocess_tests.exe --gtest_also_run_disabled_tests
 *                                     --gtest_filter=A161Probe.*
 *
 * WHY BOTH INPUT FAMILIES. #143's harness uses UNIFORM synthetic frames. A
 * candidate ranked on those alone can be the one that makes #148 worse, because
 * what works on a uniform frame is exactly the global-sigma coupling that #148
 * is about. So every candidate is measured on both families and the table
 * carries both columns.
 *
 * THE LOCAL DETECTOR IS CONTROLLED. Candidates 2-4 need a sigma rule the shipped
 * config cannot express, so they run through a probe-local copy of the Hampel
 * loop. A copy can drift from the shipped code and then the whole table measures
 * the copy. Baseline_Local is that control: the same loop with the shipped rule,
 * compared pixel-for-pixel against xpe_defect_detect_runtime.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "runtime_detection.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kW = 1024;
constexpr uint32_t kH = 1024;
constexpr size_t   kN = static_cast<size_t>(kW) * kH;
constexpr double kTprFloor = 0.999;
constexpr double kFprCap   = 0.00001;

std::vector<size_t> defectSites() {
    std::vector<size_t> s;
    for (uint32_t y = 16; y < kH - 16; y += 32)
        for (uint32_t x = 16; x < kW - 16; x += 32)
            s.push_back(static_cast<size_t>(y) * kW + x);
    return s;
}

/* ---------------------------------------------------------------------------
 * INPUT FAMILIES
 *
 * Uniform: the #143 harness's frame -- Gaussian, mean 3000, constant sigma.
 *
 * Structured: rebuilt from #148's own table, because the repository has no
 * structured fixture for this detector. Three shapes, each with a LOCAL noise
 * sigma that the injected amplitude is scaled against, so "10 sigma" means the
 * same thing everywhere in the frame:
 *   scatter -- brightness ramp 2000 -> 3200 across x, sigma = 0.35*sqrt(I)
 *              (Poisson-like: noise grows with signal)
 *   edge    -- intensity step, left 1500 sigma 12 / right 3000 sigma 25
 *   lines   -- 8-pixel stripe pattern +-400 ADU on 3000, sigma 12
 * ------------------------------------------------------------------------- */

struct Frame {
    std::vector<float> px;
    std::vector<float> localSigma;   // per-pixel true noise sigma
};

Frame uniformFrame(float sigma, uint32_t seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<float> g(0.0f, 1.0f);
    Frame f; f.px.resize(kN); f.localSigma.assign(kN, sigma);
    for (size_t i = 0; i < kN; ++i) f.px[i] = 3000.0f + sigma * g(rng);
    return f;
}

Frame scatterFrame(uint32_t seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<float> g(0.0f, 1.0f);
    Frame f; f.px.resize(kN); f.localSigma.resize(kN);
    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            const float I = 2000.0f + 1200.0f * (static_cast<float>(x) / (kW - 1));
            const float s = 0.35f * std::sqrt(I);
            const size_t i = static_cast<size_t>(y) * kW + x;
            f.localSigma[i] = s;
            f.px[i] = I + s * g(rng);
        }
    }
    return f;
}

Frame edgeFrame(uint32_t seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<float> g(0.0f, 1.0f);
    Frame f; f.px.resize(kN); f.localSigma.resize(kN);
    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            const bool right = x >= kW / 2;
            const float I = right ? 3000.0f : 1500.0f;
            const float s = right ? 25.0f : 12.0f;
            const size_t i = static_cast<size_t>(y) * kW + x;
            f.localSigma[i] = s;
            f.px[i] = I + s * g(rng);
        }
    }
    return f;
}

// Stripe period is a parameter because the FIRST measurement exposed a fixture
// artefact: the defect lattice has a 32-pixel stride, so an 8-pixel stripe puts
// EVERY injected site at the same phase -- and that phase is a stripe boundary,
// the worst possible local window. 13 is coprime with 32, so the sites land
// across all phases. Both are reported; the difference is the artefact's size.
Frame linesFrameP(uint32_t seed, uint32_t period) {
    std::mt19937 rng(seed);
    std::normal_distribution<float> g(0.0f, 1.0f);
    Frame f; f.px.resize(kN); f.localSigma.assign(kN, 12.0f);
    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            const float I = 3000.0f + (((x / period) % 2u) ? 400.0f : -400.0f);
            const size_t i = static_cast<size_t>(y) * kW + x;
            f.px[i] = I + 12.0f * g(rng);
        }
    }
    return f;
}
Frame lines13Frame(uint32_t seed) { return linesFrameP(seed, 13u); }

Frame linesFrame(uint32_t seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<float> g(0.0f, 1.0f);
    Frame f; f.px.resize(kN); f.localSigma.assign(kN, 12.0f);
    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            const float I = 3000.0f + (((x / 8u) % 2u) ? 400.0f : -400.0f);
            const size_t i = static_cast<size_t>(y) * kW + x;
            f.px[i] = I + 12.0f * g(rng);
        }
    }
    return f;
}

XpeImageBuffer asImage(std::vector<float>& v) {
    XpeImageBuffer b{};
    b.data = v.data(); b.width = kW; b.height = kH;
    b.bitsAllocated = 32; b.bitsStored = 32;
    b.format = XPE_PIXEL_FLOAT32;
    b.dataSize = static_cast<uint32_t>(kN * sizeof(float));
    return b;
}

/* --- the shipped path, for the control ----------------------------------- */
std::vector<uint8_t> detectShipped(std::vector<float>& frame, double* ms) {
    XpeImageBuffer img = asImage(frame);
    std::vector<uint8_t> map(kN, 0);
    XpeImageBuffer out{};
    out.data = map.data(); out.width = kW; out.height = kH;
    out.bitsAllocated = 8; out.bitsStored = 8;
    out.format = XPE_PIXEL_UINT8; out.dataSize = static_cast<uint32_t>(kN);
    XpeImageMetadata meta{};
    const auto t0 = std::chrono::steady_clock::now();
    EXPECT_EQ(XPE_OK, xpe_defect_detect_runtime(&img, &meta, &out));
    const auto t1 = std::chrono::steady_clock::now();
    if (ms) *ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return map;
}

/* --- probe-local Hampel loop with a pluggable sigma rule ------------------ */

enum class SigmaRule {
    ShippedFloor,   // baseline: max(mad, 0.8 * globalSigma)          [control]
    Blend,          // candidate 2: sqrt(w*mad^2 + (1-w)*globalSigma^2)
    TilePooled,     // candidate 4: sigma from a local tile, not the frame
    Oracle,         // NOT a candidate: the TRUE local sigma. The ceiling.
    TileBlend,      // QA-A-163: blend against the TILE sigma, not the frame's
    TwoStage        // candidate 3: handled separately (needs two passes)
};

struct Variant {
    const char* name;
    int32_t window;
    SigmaRule rule;
    float blendW;      // Blend only
    uint32_t tile;     // TilePooled only
};

using namespace xpe::preprocess::internal;

// Difference-MAD sigma over an arbitrary rectangle -- the same estimator
// ComputeGlobalSigma uses frame-wide (runtime_detection.h:630), restricted to a
// tile. Reusing the estimator is deliberate: candidate 4 changes the SCOPE it is
// measured over, nothing else.
float tileSigma(const std::vector<float>& px, uint32_t x0, uint32_t y0, uint32_t tile) {
    std::vector<float> d;
    d.reserve(static_cast<size_t>(tile) * tile * 2u);
    const uint32_t x1 = std::min(x0 + tile, kW);
    const uint32_t y1 = std::min(y0 + tile, kH);
    for (uint32_t y = y0; y < y1; ++y)
        for (uint32_t x = x0 + 1; x < x1; ++x)
            d.push_back(px[static_cast<size_t>(y) * kW + x] - px[static_cast<size_t>(y) * kW + x - 1]);
    for (uint32_t y = y0 + 1; y < y1; ++y)
        for (uint32_t x = x0; x < x1; ++x)
            d.push_back(px[static_cast<size_t>(y) * kW + x] - px[static_cast<size_t>(y - 1) * kW + x]);
    if (d.empty()) return 0.0f;
    const size_t mid = d.size() / 2u;
    std::nth_element(d.begin(), d.begin() + mid, d.end());
    const float med = d[mid];
    for (float& v : d) v = std::abs(v - med);
    std::nth_element(d.begin(), d.begin() + mid, d.end());
    return d[mid] * RUNTIME_DETECTION_MAD_SCALE * 0.70710678f;
}

const std::vector<float>* g_oracleSigma = nullptr;   // set by measureOn for Oracle

std::vector<uint8_t> detectVariant(const Variant& v, std::vector<float>& frame, double* ms) {
    XpeImageBuffer img = asImage(frame);
    const float gs = ComputeGlobalSigma(&img);
    std::vector<uint8_t> map(kN, 0);

    // Tile sigma table, built once per frame when the rule needs it.
    std::vector<float> tileTable;
    uint32_t tilesX = 0;
    if (v.rule == SigmaRule::TilePooled || v.rule == SigmaRule::TileBlend) {
        tilesX = (kW + v.tile - 1u) / v.tile;
        const uint32_t tilesY = (kH + v.tile - 1u) / v.tile;
        tileTable.resize(static_cast<size_t>(tilesX) * tilesY);
        for (uint32_t ty = 0; ty < tilesY; ++ty)
            for (uint32_t tx = 0; tx < tilesX; ++tx)
                tileTable[static_cast<size_t>(ty) * tilesX + tx] =
                    tileSigma(frame, tx * v.tile, ty * v.tile, v.tile);
    }

    std::vector<float> wv, dev;
    const auto t0 = std::chrono::steady_clock::now();

    auto sigmaAt = [&](uint32_t x, uint32_t y, float mad) -> float {
        switch (v.rule) {
            case SigmaRule::ShippedFloor:
                return std::max(mad, RUNTIME_DETECTION_GLOBAL_SIGMA_FLOOR * gs);
            case SigmaRule::Blend:
                return std::sqrt(v.blendW * mad * mad + (1.0f - v.blendW) * gs * gs);
            case SigmaRule::Oracle:
                return (g_oracleSigma && !g_oracleSigma->empty())
                           ? (*g_oracleSigma)[static_cast<size_t>(y) * kW + x] : mad;
            case SigmaRule::TileBlend: {
                // The two mechanisms measured so far are orthogonal: the blend
                // shrinks the spread of the sigma estimate, the tile follows
                // spatially varying noise. This does both -- same blend, local
                // reference.
                const float ts = tileTable[static_cast<size_t>(y / v.tile) * tilesX + (x / v.tile)];
                return std::sqrt(v.blendW * mad * mad + (1.0f - v.blendW) * ts * ts);
            }
            case SigmaRule::TilePooled: {
                const float ts = tileTable[static_cast<size_t>(y / v.tile) * tilesX + (x / v.tile)];
                return std::max(mad, RUNTIME_DETECTION_GLOBAL_SIGMA_FLOOR * ts);
            }
            default:
                return mad;
        }
    };

    RuntimeDetectionConfig cfg = RuntimeDetection_DefaultConfig();
    cfg.windowSize = v.window;

    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            CollectNeighborValues(&img, x, y, cfg.windowSize, wv);
            if (wv.size() < RUNTIME_DETECTION_MIN_NEIGHBORS) continue;
            const float median = ComputeMedian(wv);
            dev.assign(wv.begin(), wv.end());
            const float mad = ComputeMAD(dev, median);
            const float sigma = sigmaAt(x, y, mad);
            const float centre = frame[static_cast<size_t>(y) * kW + x];
            const float d = std::abs(centre - median);
            if (sigma < 1e-6f) { if (d > 1e-6f) map[static_cast<size_t>(y) * kW + x] = 1; continue; }
            if (d > cfg.sigmaThreshold * sigma) map[static_cast<size_t>(y) * kW + x] = 1;
        }
    }
    const auto t1 = std::chrono::steady_clock::now();
    if (ms) *ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return map;
}

// Candidate 3: loose first pass, then re-judge each candidate with a window
// that EXCLUDES the other candidates. The masking is the point -- a defect in
// the window inflates the MAD and hides its neighbours.
std::vector<uint8_t> detectTwoStage(std::vector<float>& frame, int32_t window,
                                    float loose, float strict, double* ms) {
    XpeImageBuffer img = asImage(frame);
    const float gs = ComputeGlobalSigma(&img);
    const float floorV = RUNTIME_DETECTION_GLOBAL_SIGMA_FLOOR * gs;
    std::vector<uint8_t> cand(kN, 0), map(kN, 0);
    std::vector<float> wv, dev;

    const auto t0 = std::chrono::steady_clock::now();
    // pass 1 -- loose
    for (uint32_t y = 0; y < kH; ++y)
        for (uint32_t x = 0; x < kW; ++x) {
            CollectNeighborValues(&img, x, y, window, wv);
            if (wv.size() < RUNTIME_DETECTION_MIN_NEIGHBORS) continue;
            const float m = ComputeMedian(wv);
            dev.assign(wv.begin(), wv.end());
            const float sigma = std::max(ComputeMAD(dev, m), floorV);
            if (sigma < 1e-6f) continue;
            if (std::abs(frame[static_cast<size_t>(y) * kW + x] - m) > loose * sigma)
                cand[static_cast<size_t>(y) * kW + x] = 1;
        }

    // pass 2 -- strict, on candidates only, neighbours that are themselves
    // candidates removed from the estimate
    const int32_t half = window / 2;
    for (uint32_t y = 0; y < kH; ++y)
        for (uint32_t x = 0; x < kW; ++x) {
            const size_t i = static_cast<size_t>(y) * kW + x;
            if (!cand[i]) continue;
            wv.clear();
            for (int32_t dy = -half; dy <= half; ++dy)
                for (int32_t dx = -half; dx <= half; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    const int32_t nx = static_cast<int32_t>(x) + dx;
                    const int32_t ny = static_cast<int32_t>(y) + dy;
                    if (nx < 0 || ny < 0 || nx >= static_cast<int32_t>(kW) || ny >= static_cast<int32_t>(kH)) continue;
                    const size_t j = static_cast<size_t>(ny) * kW + static_cast<size_t>(nx);
                    if (cand[j]) continue;            // mask co-candidates
                    wv.push_back(frame[j]);
                }
            if (wv.size() < RUNTIME_DETECTION_MIN_NEIGHBORS) continue;
            const float m = ComputeMedian(wv);
            dev.assign(wv.begin(), wv.end());
            const float sigma = std::max(ComputeMAD(dev, m), floorV);
            if (sigma < 1e-6f) { if (std::abs(frame[i] - m) > 1e-6f) map[i] = 1; continue; }
            if (std::abs(frame[i] - m) > strict * sigma) map[i] = 1;
        }
    const auto t1 = std::chrono::steady_clock::now();
    if (ms) *ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return map;
}

/* --- measurement ---------------------------------------------------------- */

struct Result { double tpr = 0, fpr = 0, ms = 0; size_t tp = 0, inj = 0, fp = 0; };

template <typename Detector>
Result measureOn(Frame (*make)(uint32_t), float amplitudeSigma, uint32_t seed, Detector det) {
    Result r;
    const std::vector<size_t> sites = defectSites();
    r.inj = sites.size();

    Frame withDefects = make(seed);
    for (size_t s : sites) withDefects.px[s] += amplitudeSigma * withDefects.localSigma[s];
    g_oracleSigma = &withDefects.localSigma;
    double msA = 0.0;
    const std::vector<uint8_t> flagged = det(withDefects.px, &msA);
    for (size_t s : sites) if (flagged[s]) ++r.tp;

    Frame clean = make(seed);
    g_oracleSigma = &clean.localSigma;
    double msB = 0.0;
    const std::vector<uint8_t> flaggedClean = det(clean.px, &msB);
    for (size_t i = 0; i < kN; ++i) if (flaggedClean[i]) ++r.fp;

    r.tpr = static_cast<double>(r.tp) / static_cast<double>(r.inj);
    r.fpr = static_cast<double>(r.fp) / static_cast<double>(kN);
    r.ms  = (msA + msB) / 2.0;
    return r;
}

Frame uniform10(uint32_t s) { return uniformFrame(10.0f, s); }

void row(const char* name, const Result& uni5, const Result& sc, const Result& ed, const Result& li) {
    std::printf("| %-26s | %.4f | %.3e | %.4f | %.4f | %.4f | %7.1f | %-3s |\n",
                name, uni5.tpr, uni5.fpr, sc.tpr, ed.tpr, li.tpr, uni5.ms,
                (uni5.tpr >= kTprFloor && uni5.fpr < kFprCap) ? "YES" : "no");
}

// QA-A-162: the requirement now names 10 sigma for TPR, so the pass/fail column
// reads TPR@10-sigma AND FPR. The structured columns are carried at the same
// amplitude -- a candidate that meets the bar on uniform frames and collapses
// on a ramp is not a candidate.
void row10(const char* name, const Result& u, const Result& sc,
           const Result& ed, const Result& li) {
    std::printf("| %-26s | %8.4f | %.4f | %.4f | %.4f | %.3e | %7.1f | %-3s |\n",
                name, u.tpr, sc.tpr, ed.tpr, li.tpr, u.fpr, u.ms,
                (u.tpr >= kTprFloor && u.fpr < kFprCap) ? "YES" : "no");
}

class A161Probe : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr)); }
    void TearDown() override { xpe_preprocess_shutdown(); }
};

} // namespace

// CONTROL FIRST: does the probe-local loop reproduce the shipped detector?
// Everything in the table is worthless if it does not.
TEST_F(A161Probe, DISABLED_ControlLocalLoopMatchesShipped) {
    Frame f = uniformFrame(10.0f, 20260911u);
    for (size_t s : defectSites()) f.px[s] += 5.0f * 10.0f;
    std::vector<float> a = f.px, b = f.px;

    const std::vector<uint8_t> shipped = detectShipped(a, nullptr);
    const Variant base{"baseline-local", 3, SigmaRule::ShippedFloor, 0, 0};
    const std::vector<uint8_t> local = detectVariant(base, b, nullptr);

    size_t diff = 0;
    for (size_t i = 0; i < kN; ++i) if (shipped[i] != local[i]) ++diff;
    std::printf("[a161] CONTROL shipped-vs-local differing pixels = %zu / %zu\n", diff, kN);
    EXPECT_EQ(0u, diff) << "the probe-local loop must reproduce the shipped detector";
}

// #148 premise check: is the structured-frame TPR still 0, after QA-A-48
// replaced the value-MAD with a difference-MAD?
TEST_F(A161Probe, DISABLED_PremiseStructuredFrameTprToday) {
    for (auto [name, make] : std::initializer_list<std::pair<const char*, Frame(*)(uint32_t)>>{
             {"scatter", scatterFrame}, {"edge", edgeFrame}, {"lines(p8)", linesFrame}, {"lines(p13)", lines13Frame}}) {
        const Result r5  = measureOn(make, 5.0f, 20260911u,
                                     [](std::vector<float>& f, double* ms) { return detectShipped(f, ms); });
        const Result r10 = measureOn(make, 10.0f, 20260911u,
                                     [](std::vector<float>& f, double* ms) { return detectShipped(f, ms); });
        std::printf("[a161] PREMISE %-8s shipped TPR@5s=%.6f TPR@10s=%.6f FPR=%.3e\n",
                    name, r5.tpr, r10.tpr, r5.fpr);
    }
}

// QA-A-162 (#143): TPR is now specified at 10 sigma, so the whole table is
// re-read at that amplitude. The question changed from "raise TPR@5s" to
// "hold TPR@10s >= 0.999 while pushing FPR under 1e-5".
TEST_F(A161Probe, DISABLED_CandidateTableAt10Sigma) {
    std::printf("\n| candidate                  | uniTPR10 | scatTPR | edgeTPR | lineTPR | uniFPR    |   ms    | req |\n");
    std::printf("|----------------------------|----------|---------|---------|---------|-----------|---------|-----|\n");

    auto shippedDet = [](std::vector<float>& f, double* ms) { return detectShipped(f, ms); };
    row10("0 baseline (shipped)",
          measureOn(uniform10, 10.0f, 20260911u, shippedDet),
          measureOn(scatterFrame, 10.0f, 20260911u, shippedDet),
          measureOn(edgeFrame, 10.0f, 20260911u, shippedDet),
          measureOn(lines13Frame, 10.0f, 20260911u, shippedDet));

    const Variant variants[] = {
        {"0L baseline scalar",      3, SigmaRule::ShippedFloor, 0.0f, 0},
        {"-- ORACLE true sigma",    3, SigmaRule::Oracle,       0.0f, 0},
        {"1a window 5",             5, SigmaRule::ShippedFloor, 0.0f, 0},
        {"1b window 7",             7, SigmaRule::ShippedFloor, 0.0f, 0},
        {"1c window 9",             9, SigmaRule::ShippedFloor, 0.0f, 0},
        {"2b blend w=0.25",         3, SigmaRule::Blend,        0.25f, 0},
        {"2c blend w=0.15",         3, SigmaRule::Blend,        0.15f, 0},
        {"2d blend w=0.10",         3, SigmaRule::Blend,        0.10f, 0},
        {"2e blend w=0.05",         3, SigmaRule::Blend,        0.05f, 0},
        {"5a w5 + blend w=0.25",    5, SigmaRule::Blend,        0.25f, 0},
        {"5b w7 + blend w=0.25",    7, SigmaRule::Blend,        0.25f, 0},
        {"5c w5 + blend w=0.15",    5, SigmaRule::Blend,        0.15f, 0},
    };
    for (const Variant& v : variants) {
        auto det = [&v](std::vector<float>& f, double* ms) { return detectVariant(v, f, ms); };
        row10(v.name,
              measureOn(uniform10, 10.0f, 20260911u, det),
              measureOn(scatterFrame, 10.0f, 20260911u, det),
              measureOn(edgeFrame, 10.0f, 20260911u, det),
              measureOn(lines13Frame, 10.0f, 20260911u, det));
    }
    std::printf("\n");
}

TEST_F(A161Probe, DISABLED_CandidateTable) {
    std::printf("\n| candidate                  | uniTPR5 | uniFPR    | scatTPR | edgeTPR | lineTPR |   ms    | req |\n");
    std::printf("|----------------------------|---------|-----------|---------|---------|---------|---------|-----|\n");

    auto shippedDet = [](std::vector<float>& f, double* ms) { return detectShipped(f, ms); };
    row("0 baseline (shipped)",
        measureOn(uniform10, 5.0f, 20260911u, shippedDet),
        measureOn(scatterFrame, 5.0f, 20260911u, shippedDet),
        measureOn(edgeFrame, 5.0f, 20260911u, shippedDet),
        measureOn(lines13Frame, 5.0f, 20260911u, shippedDet));

    const Variant variants[] = {
        {"1a window 5 (24 nbrs)",   5, SigmaRule::ShippedFloor, 0.0f, 0},
        {"1b window 7 (48 nbrs)",   7, SigmaRule::ShippedFloor, 0.0f, 0},
        {"2a blend w=0.5",          3, SigmaRule::Blend,        0.5f, 0},
        {"2b blend w=0.25",         3, SigmaRule::Blend,        0.25f, 0},
        {"0L baseline scalar (time)", 3, SigmaRule::ShippedFloor, 0.0f, 0},
        {"-- ORACLE true sigma",      3, SigmaRule::Oracle,       0.0f, 0},
        {"4a tile 64 + floor",      3, SigmaRule::TilePooled,   0.0f, 64},
        {"4b tile 32 + window 5",   5, SigmaRule::TilePooled,   0.0f, 32},
    };
    for (const Variant& v : variants) {
        auto det = [&v](std::vector<float>& f, double* ms) { return detectVariant(v, f, ms); };
        row(v.name,
            measureOn(uniform10, 5.0f, 20260911u, det),
            measureOn(scatterFrame, 5.0f, 20260911u, det),
            measureOn(edgeFrame, 5.0f, 20260911u, det),
            measureOn(lines13Frame, 5.0f, 20260911u, det));
    }

    struct TS { const char* name; int32_t w; float loose, strict; };
    const TS stages[] = {
        {"3a two-stage 3/5 w3", 3, 3.0f, 5.0f},
        {"3b two-stage 3/5 w5", 5, 3.0f, 5.0f},
        {"3c two-stage 2.5/4 w5", 5, 2.5f, 4.0f},
    };
    for (const TS& t : stages) {
        auto det = [&t](std::vector<float>& f, double* ms) {
            return detectTwoStage(f, t.w, t.loose, t.strict, ms);
        };
        row(t.name,
            measureOn(uniform10, 5.0f, 20260911u, det),
            measureOn(scatterFrame, 5.0f, 20260911u, det),
            measureOn(edgeFrame, 5.0f, 20260911u, det),
            measureOn(lines13Frame, 5.0f, 20260911u, det));
    }
    std::printf("\n");
}

// QA-A-162: THE TOP CANDIDATES ARE SEPARATED BY A HANDFUL OF PIXELS.
//
// FPR 1e-5 on a 1024x1024 frame is 10.49 pixels. The leading candidates landed
// at 0, 1, 2 and 5 false pixels on one seed -- differences that a single frame
// cannot resolve, because a count that small is dominated by Poisson noise.
// Ranking them on one seed would be reading the seed, not the algorithm.
//
// This case re-measures FPR only, over several seeds, and reports the TOTAL
// count and the pooled rate. It also reports TPR@10-sigma per seed so a
// candidate cannot buy its FPR with detection it quietly gave up.
TEST_F(A161Probe, DISABLED_FprAcrossSeeds) {
    const uint32_t seeds[] = {20260911u, 7u, 1234u, 99991u, 424242u};
    const Variant cands[] = {
        {"0L baseline scalar",   3, SigmaRule::ShippedFloor, 0.0f,  0},
        {"-- ORACLE true sigma", 3, SigmaRule::Oracle,       0.0f,  0},
        {"1b window 7",          7, SigmaRule::ShippedFloor, 0.0f,  0},
        {"1c window 9",          9, SigmaRule::ShippedFloor, 0.0f,  0},
        {"2b blend w=0.25",      3, SigmaRule::Blend,        0.25f, 0},
        {"2c blend w=0.15",      3, SigmaRule::Blend,        0.15f, 0},
        {"2d blend w=0.10",      3, SigmaRule::Blend,        0.10f, 0},
        {"2e blend w=0.05",      3, SigmaRule::Blend,        0.05f, 0},
        {"5a w5 + blend w=0.25", 5, SigmaRule::Blend,        0.25f, 0},
        {"5c w5 + blend w=0.15", 5, SigmaRule::Blend,        0.15f, 0},
    };
    const size_t nSeeds = sizeof(seeds) / sizeof(seeds[0]);
    const double budgetPixels = kFprCap * static_cast<double>(kN);

    std::printf("\n| candidate                  | FP total | frames | pooled FPR | worst TPR10 | req |\n");
    std::printf("|----------------------------|----------|--------|------------|-------------|-----|\n");
    for (const Variant& v : cands) {
        size_t fpTotal = 0;
        double worstTpr = 1.0;
        for (uint32_t sd : seeds) {
            auto det = [&v](std::vector<float>& f, double* ms) { return detectVariant(v, f, ms); };
            const Result r = measureOn(uniform10, 10.0f, sd, det);
            fpTotal += r.fp;
            worstTpr = std::min(worstTpr, r.tpr);
        }
        const double pooled = static_cast<double>(fpTotal) / (static_cast<double>(kN) * nSeeds);
        std::printf("| %-26s | %8zu | %6zu | %.4e | %11.4f | %-3s |\n",
                    v.name, fpTotal, nSeeds, pooled, worstTpr,
                    (pooled < kFprCap && worstTpr >= kTprFloor) ? "YES" : "no");
    }
    std::printf("[a161] budget per frame = %.2f false pixels; %zu frames -> %.1f total\n",
                budgetPixels, nSeeds, budgetPixels * nSeeds);
}

// QA-A-162: WHAT THE SURVIVORS PAY AT LOWER AMPLITUDES.
//
// 5c's pooled FPR (2.29e-06) is BELOW the ORACLE's (4.96e-06). A detector
// cannot beat the true sigma by estimating better -- it beats it by estimating
// sigma HIGH, which raises the threshold and buys false-positive reduction with
// detection. At 10 sigma the margin hides that; the requirement's amplitude is
// not the only amplitude a panel sees, so the price is measured here.
//
// This is the mirror of the risk already recorded for 4a (tile sigma came out
// BELOW the true sigma, buying detection with false positives).
TEST_F(A161Probe, DISABLED_AmplitudeSweepOfSurvivors) {
    const Variant cands[] = {
        {"0L baseline scalar",   3, SigmaRule::ShippedFloor, 0.0f,  0},
        {"-- ORACLE true sigma", 3, SigmaRule::Oracle,       0.0f,  0},
        {"2c blend w=0.15",      3, SigmaRule::Blend,        0.15f, 0},
        {"2d blend w=0.10",      3, SigmaRule::Blend,        0.10f, 0},
        {"2e blend w=0.05",      3, SigmaRule::Blend,        0.05f, 0},
        {"5a w5 + blend w=0.25", 5, SigmaRule::Blend,        0.25f, 0},
        {"5c w5 + blend w=0.15", 5, SigmaRule::Blend,        0.15f, 0},
    };
    std::printf("\n| candidate                  | TPR@6s | TPR@7s | TPR@8s | TPR@10s |\n");
    std::printf("|----------------------------|--------|--------|--------|---------|\n");
    for (const Variant& v : cands) {
        auto det = [&v](std::vector<float>& f, double* ms) { return detectVariant(v, f, ms); };
        const Result r6  = measureOn(uniform10,  6.0f, 20260911u, det);
        const Result r7  = measureOn(uniform10,  7.0f, 20260911u, det);
        const Result r8  = measureOn(uniform10,  8.0f, 20260911u, det);
        const Result r10 = measureOn(uniform10, 10.0f, 20260911u, det);
        std::printf("| %-26s | %.4f | %.4f | %.4f | %7.4f |\n",
                    v.name, r6.tpr, r7.tpr, r8.tpr, r10.tpr);
    }
    std::printf("\n");
}

// QA-A-163 LANDING CONDITION 1: FPR on CLEAN STRUCTURED frames.
//
// Everything measured so far took its FPR from a uniform frame. The
// requirement says "clean clinical frames", and a clinical frame always has
// anatomy in it -- which is the whole of #148. A candidate chosen on uniform
// FPR alone is chosen on the input family that hides the failure mode.
//
// "Clean" here means the frame WITHOUT injected transients: the same generator,
// no defects. Pooled over seeds, because 1e-5 on this frame size is 10.49
// pixels and single-seed counts are Poisson noise (QA-A-162).
TEST_F(A161Probe, DISABLED_StructuredFrameFprOfTheChosenCandidate) {
    const uint32_t seeds[] = {20260911u, 7u, 1234u, 99991u, 424242u};
    const size_t nSeeds = sizeof(seeds) / sizeof(seeds[0]);

    struct Fam { const char* name; Frame (*make)(uint32_t); };
    const Fam fams[] = {
        {"uniform", uniform10}, {"scatter", scatterFrame},
        {"edge", edgeFrame}, {"lines(p13)", lines13Frame},
    };
    const Variant cands[] = {
        {"0L baseline scalar", 3, SigmaRule::ShippedFloor, 0.0f,  0},
        {"2d blend w=0.10",    3, SigmaRule::Blend,        0.10f, 0},
        {"2c blend w=0.15",    3, SigmaRule::Blend,        0.15f, 0},
        {"2e blend w=0.05",    3, SigmaRule::Blend,        0.05f, 0},
    };

    std::printf("\n| candidate            | family     | FP total | pooled FPR | req |\n");
    std::printf("|----------------------|------------|----------|------------|-----|\n");
    for (const Variant& v : cands) {
        for (const Fam& fam : fams) {
            size_t fpTotal = 0;
            for (uint32_t sd : seeds) {
                Frame clean = fam.make(sd);
                g_oracleSigma = &clean.localSigma;
                const std::vector<uint8_t> flagged = detectVariant(v, clean.px, nullptr);
                for (size_t i = 0; i < kN; ++i) if (flagged[i]) ++fpTotal;
            }
            const double pooled = static_cast<double>(fpTotal) / (static_cast<double>(kN) * nSeeds);
            std::printf("| %-20s | %-10s | %8zu | %.4e | %-3s |\n",
                        v.name, fam.name, fpTotal, pooled, (pooled < kFprCap) ? "YES" : "no");
        }
    }
    std::printf("\n");
}

// QA-A-163: WHY `edge` BREAKS -- is it the detector or the fixture?
//
// The lines/period-8 resonance (QA-A-161) taught this question. `edge` is a
// ONE-PIXEL-WIDE perfect step, and a real X-ray edge is not: scatter and focal
// spot blur it over several pixels. So two things are measured.
//
//   (a) WHERE the false positives are. If they sit on the step, the detector is
//       flagging the step itself -- which is arguably correct behaviour on an
//       input that says "these two pixels differ by 60 sigma".
//   (b) What a BLURRED step does. A step smeared over n pixels is the same
//       structure with a realistic slope.
TEST_F(A161Probe, DISABLED_WhyEdgeBreaks) {
    const uint32_t seeds[] = {20260911u, 7u, 1234u, 99991u, 424242u};
    const size_t nSeeds = sizeof(seeds) / sizeof(seeds[0]);
    const Variant v2d{"2d blend w=0.10", 3, SigmaRule::Blend, 0.10f, 0};

    // (a) where are they?
    std::printf("\n[a163] edge false positives by distance from the step (2d, 5 seeds)\n");
    size_t onStep = 0, offStep = 0;
    for (uint32_t sd : seeds) {
        Frame clean = edgeFrame(sd);
        const std::vector<uint8_t> flagged = detectVariant(v2d, clean.px, nullptr);
        for (uint32_t y = 0; y < kH; ++y)
            for (uint32_t x = 0; x < kW; ++x)
                if (flagged[static_cast<size_t>(y) * kW + x]) {
                    const int32_t d = std::abs(static_cast<int32_t>(x) - static_cast<int32_t>(kW / 2));
                    if (d <= 1) ++onStep; else ++offStep;
                }
    }
    const size_t offPixels = (static_cast<size_t>(kW) - 3u) * kH * nSeeds;
    std::printf("[a163]   within 1 px of the step : %zu\n", onStep);
    std::printf("[a163]   everywhere else         : %zu  -> FPR %.4e over %zu px\n",
                offStep, static_cast<double>(offStep) / static_cast<double>(offPixels), offPixels);

    // (b) what does a blurred step do?
    std::printf("[a163] edge FPR vs step blur width (2d, 5 seeds)\n");
    for (uint32_t blur : {0u, 1u, 2u, 4u, 8u}) {
        size_t fp = 0;
        for (uint32_t sd : seeds) {
            std::mt19937 rng(sd);
            std::normal_distribution<float> g(0.0f, 1.0f);
            std::vector<float> px(kN);
            for (uint32_t y = 0; y < kH; ++y)
                for (uint32_t x = 0; x < kW; ++x) {
                    // linear ramp of width `blur` centred on the step
                    const double t = blur == 0u
                        ? (x >= kW / 2 ? 1.0 : 0.0)
                        : std::min(1.0, std::max(0.0,
                              (static_cast<double>(x) - (kW / 2.0 - blur / 2.0)) / blur));
                    const float I = static_cast<float>(1500.0 + 1500.0 * t);
                    const float s = static_cast<float>(12.0 + 13.0 * t);
                    px[static_cast<size_t>(y) * kW + x] = I + s * g(rng);
                }
            const std::vector<uint8_t> flagged = detectVariant(v2d, px, nullptr);
            for (size_t i = 0; i < kN; ++i) if (flagged[i]) ++fp;
        }
        const double pooled = static_cast<double>(fp) / (static_cast<double>(kN) * nSeeds);
        std::printf("[a163]   blur %u px : FP %6zu  FPR %.4e  %s\n",
                    blur, fp, pooled, (pooled < kFprCap) ? "YES" : "no");
    }
}

// QA-A-163: the third hypothesis -- SPATIALLY VARYING NOISE.
//
// The first two were falsified: the false positives are NOT on the step (12 of
// 3951), and blurring the step over 8 pixels changes nothing (7.54e-4 ->
// 7.46e-4). What is left is that `edge` has TWO noise levels (left sigma 12,
// right sigma 25) and the blend has ONE global sigma. Where the global value
// under-estimates the local noise, the threshold is too low and everything is
// an outlier.
//
// Prediction, stated before measuring: the false positives concentrate on the
// NOISIER half, and the ratio is far from 50/50.
TEST_F(A161Probe, DISABLED_EdgeFalsePositivesBySide) {
    const uint32_t seeds[] = {20260911u, 7u, 1234u, 99991u, 424242u};
    const Variant cands[] = {
        {"0L baseline scalar", 3, SigmaRule::ShippedFloor, 0.0f,  0},
        {"2d blend w=0.10",    3, SigmaRule::Blend,        0.10f, 0},
        {"-- ORACLE true sigma", 3, SigmaRule::Oracle,     0.0f,  0},
    };
    std::printf("\n[a163] edge false positives by side (left sigma=12, right sigma=25), 5 seeds\n");
    std::printf("| candidate            |   left |  right | right share | globalSigma |\n");
    std::printf("|----------------------|--------|--------|-------------|-------------|\n");
    for (const Variant& v : cands) {
        size_t l = 0, r = 0;
        float gsSeen = 0.0f;
        for (uint32_t sd : seeds) {
            Frame clean = edgeFrame(sd);
            g_oracleSigma = &clean.localSigma;
            XpeImageBuffer img = asImage(clean.px);
            gsSeen = ComputeGlobalSigma(&img);
            const std::vector<uint8_t> flagged = detectVariant(v, clean.px, nullptr);
            for (uint32_t y = 0; y < kH; ++y)
                for (uint32_t x = 0; x < kW; ++x)
                    if (flagged[static_cast<size_t>(y) * kW + x]) {
                        if (x < kW / 2) ++l; else ++r;
                    }
        }
        const double share = (l + r) ? static_cast<double>(r) / static_cast<double>(l + r) : 0.0;
        std::printf("| %-20s | %6zu | %6zu | %10.1f%% | %11.3f |\n",
                    v.name, l, r, 100.0 * share, gsSeen);
    }
    std::printf("[a163] for reference: true sigma is 12 (left) and 25 (right)\n");
}

// QA-A-163: if ONE global sigma is the cause, a LOCAL one should fix it.
//
// The side split named the mechanism: global sigma is 16.71 while the true
// noise is 12 on the left and 25 on the right, so the right half runs with a
// threshold set for quieter pixels and flags everything. ORACLE, same frame and
// same window with only the sigma made local, splits 12/16 and meets the
// requirement -- so the frame is not hard; one global number is.
//
// That is what candidate 4a (tile sigma) does, and QA-A-161 flagged it as a
// risk for the opposite reason (its `edge` TPR EXCEEDED the oracle's, meaning
// the tile sigma came out BELOW the true value). Both observations are the same
// mechanism seen from two sides, so the tile is measured here on clean frames.
TEST_F(A161Probe, DISABLED_TileSigmaOnCleanStructuredFrames) {
    const uint32_t seeds[] = {20260911u, 7u, 1234u, 99991u, 424242u};
    const size_t nSeeds = sizeof(seeds) / sizeof(seeds[0]);
    struct Fam { const char* name; Frame (*make)(uint32_t); };
    const Fam fams[] = {
        {"uniform", uniform10}, {"scatter", scatterFrame},
        {"edge", edgeFrame}, {"lines(p13)", lines13Frame},
    };
    const Variant cands[] = {
        {"2d blend w=0.10",   3, SigmaRule::Blend,      0.10f, 0},
        {"4a tile 64",        3, SigmaRule::TilePooled, 0.0f, 64},
        {"4c tile 32",        3, SigmaRule::TilePooled, 0.0f, 32},
        {"4d tile 128",       3, SigmaRule::TilePooled, 0.0f, 128},
        {"-- ORACLE",         3, SigmaRule::Oracle,     0.0f,  0},
    };
    std::printf("\n| candidate         | family     | FP total | pooled FPR | req |\n");
    std::printf("|-------------------|------------|----------|------------|-----|\n");
    for (const Variant& v : cands) {
        for (const Fam& fam : fams) {
            size_t fp = 0;
            for (uint32_t sd : seeds) {
                Frame clean = fam.make(sd);
                g_oracleSigma = &clean.localSigma;
                const std::vector<uint8_t> flagged = detectVariant(v, clean.px, nullptr);
                for (size_t i = 0; i < kN; ++i) if (flagged[i]) ++fp;
            }
            const double pooled = static_cast<double>(fp) / (static_cast<double>(kN) * nSeeds);
            std::printf("| %-17s | %-10s | %8zu | %.4e | %-3s |\n",
                        v.name, fam.name, fp, pooled, (pooled < kFprCap) ? "YES" : "no");
        }
    }
    // and the TPR they keep
    std::printf("\n| candidate         | TPR@10s uni | scatter | edge   | lines  |\n");
    std::printf("|-------------------|-------------|---------|--------|--------|\n");
    for (const Variant& v : cands) {
        auto det = [&v](std::vector<float>& f, double* ms) { return detectVariant(v, f, ms); };
        std::printf("| %-17s | %11.4f | %.4f | %.4f | %.4f |\n", v.name,
                    measureOn(uniform10, 10.0f, 20260911u, det).tpr,
                    measureOn(scatterFrame, 10.0f, 20260911u, det).tpr,
                    measureOn(edgeFrame, 10.0f, 20260911u, det).tpr,
                    measureOn(lines13Frame, 10.0f, 20260911u, det).tpr);
    }
    std::printf("\n");
}

// QA-A-163: the two mechanisms are ORTHOGONAL, so combine them.
//
//   blend  shrinks the SPREAD of the sigma estimate  -> uniform FPR 1.2e-4 -> 7.8e-6
//   tile   follows SPATIALLY VARYING noise           -> edge FPR 7.5e-4 -> 1.2e-4
//
// Neither alone meets the requirement on every family. This measures the blend
// taken against the TILE sigma instead of the frame sigma. Reported as a
// measurement, not a proposal -- the choice is the lead's.
TEST_F(A161Probe, DISABLED_TileBlendCombination) {
    const uint32_t seeds[] = {20260911u, 7u, 1234u, 99991u, 424242u};
    const size_t nSeeds = sizeof(seeds) / sizeof(seeds[0]);
    struct Fam { const char* name; Frame (*make)(uint32_t); };
    const Fam fams[] = {
        {"uniform", uniform10}, {"scatter", scatterFrame},
        {"edge", edgeFrame}, {"lines(p13)", lines13Frame},
    };
    const Variant cands[] = {
        {"6a tile64 blend 0.10", 3, SigmaRule::TileBlend, 0.10f, 64},
        {"6b tile64 blend 0.25", 3, SigmaRule::TileBlend, 0.25f, 64},
        {"6c tile32 blend 0.10", 3, SigmaRule::TileBlend, 0.10f, 32},
        {"6d tile128 blend 0.10",3, SigmaRule::TileBlend, 0.10f, 128},
    };
    std::printf("\n| candidate            | family     | FP total | pooled FPR | req |\n");
    std::printf("|----------------------|------------|----------|------------|-----|\n");
    for (const Variant& v : cands) {
        for (const Fam& fam : fams) {
            size_t fp = 0;
            for (uint32_t sd : seeds) {
                Frame clean = fam.make(sd);
                const std::vector<uint8_t> flagged = detectVariant(v, clean.px, nullptr);
                for (size_t i = 0; i < kN; ++i) if (flagged[i]) ++fp;
            }
            const double pooled = static_cast<double>(fp) / (static_cast<double>(kN) * nSeeds);
            std::printf("| %-20s | %-10s | %8zu | %.4e | %-3s |\n",
                        v.name, fam.name, fp, pooled, (pooled < kFprCap) ? "YES" : "no");
        }
    }
    std::printf("\n| candidate            | TPR@10s uni | scatter | edge   | lines  |  ms   |\n");
    std::printf("|----------------------|-------------|---------|--------|--------|-------|\n");
    for (const Variant& v : cands) {
        auto det = [&v](std::vector<float>& f, double* ms) { return detectVariant(v, f, ms); };
        const Result u = measureOn(uniform10, 10.0f, 20260911u, det);
        std::printf("| %-20s | %11.4f | %.4f | %.4f | %.4f | %5.1f |\n", v.name, u.tpr,
                    measureOn(scatterFrame, 10.0f, 20260911u, det).tpr,
                    measureOn(edgeFrame, 10.0f, 20260911u, det).tpr,
                    measureOn(lines13Frame, 10.0f, 20260911u, det).tpr, u.ms);
    }
    std::printf("\n");
}

// QA-A-163 LANDING CONDITION 2 (partial): is the AVX2 path alive, and where
// does the shipped detector sit against the 60 ms target?
//
// spec.md:271 -- "<= 60 ms (AVX2, single thread) on the development machine for
// a 3072x3072 FLOAT32 frame". The candidates are all 1.2-1.4x the scalar
// baseline, so what that costs depends on how much headroom exists today.
//
// The AVX2 path is taken only for windowSize == 3 (runtime_detection.h:1074),
// which is why every surviving candidate keeps a 3x3 window. This measures the
// shipped entry point at the spec's own frame size.
TEST_F(A161Probe, DISABLED_ShippedTimingAtSpecFrameSize) {
    constexpr uint32_t kBigW = 3072, kBigH = 3072;
    const size_t bigN = static_cast<size_t>(kBigW) * kBigH;

    std::mt19937 rng(20260911u);
    std::normal_distribution<float> g(0.0f, 1.0f);
    std::vector<float> frame(bigN);
    for (size_t i = 0; i < bigN; ++i) frame[i] = 3000.0f + 10.0f * g(rng);

    XpeImageBuffer img{};
    img.data = frame.data(); img.width = kBigW; img.height = kBigH;
    img.bitsAllocated = 32; img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.dataSize = static_cast<uint32_t>(bigN * sizeof(float));

    std::vector<uint8_t> map(bigN, 0);
    XpeImageBuffer out{};
    out.data = map.data(); out.width = kBigW; out.height = kBigH;
    out.bitsAllocated = 8; out.bitsStored = 8;
    out.format = XPE_PIXEL_UINT8; out.dataSize = static_cast<uint32_t>(bigN);
    XpeImageMetadata meta{};

    // three runs: the first pays for page faults on a 36 MB frame
    for (int run = 0; run < 3; ++run) {
        const auto t0 = std::chrono::steady_clock::now();
        ASSERT_EQ(XPE_OK, xpe_defect_detect_runtime(&img, &meta, &out));
        const auto t1 = std::chrono::steady_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::printf("[a163] shipped 3072x3072 run %d : %7.1f ms   (spec.md:271 target 60 ms)\n",
                    run, ms);
    }
}

// QA-A-164 LANDING CONDITION: the SHIPPED path, now carrying 6d.
//
// Everything that chose 6d was measured on the probe's scalar loop. The shipped
// path adds AVX2, and the tile table is a structure that path did not have --
// QA-A-161 flagged exactly this for candidate 4a. Three questions:
//   (1) does the shipped detector produce the SAME map as the scalar rule?
//   (2) are the FPR / TPR the ones 6d was chosen for?
//   (3) is AVX2 still running, or did the tile table push everything scalar?
TEST_F(A161Probe, DISABLED_ShippedPathAfter6dLanded) {
    const uint32_t seeds[] = {20260911u, 7u, 1234u, 99991u, 424242u};
    const size_t nSeeds = sizeof(seeds) / sizeof(seeds[0]);
    struct Fam { const char* name; Frame (*make)(uint32_t); };
    const Fam fams[] = {
        {"uniform", uniform10}, {"scatter", scatterFrame},
        {"edge", edgeFrame}, {"lines(p13)", lines13Frame},
    };

    // (1) shipped vs the probe's own 6d -- the tile table is built the same way
    // by both, so a mismatch means the AVX2 run-splitting is wrong.
    const Variant v6d{"6d tile128 blend 0.10", 3, SigmaRule::TileBlend, 0.10f, 128};
    size_t totalDiff = 0;
    for (const Fam& fam : fams) {
        Frame f = fam.make(20260911u);
        std::vector<float> a = f.px, b = f.px;
        const std::vector<uint8_t> shipped = detectShipped(a, nullptr);
        const std::vector<uint8_t> local = detectVariant(v6d, b, nullptr);
        size_t d = 0;
        for (size_t i = 0; i < kN; ++i) if (shipped[i] != local[i]) ++d;
        std::printf("[a164] EQUIVALENCE %-10s shipped vs probe-6d differing = %zu / %zu\n",
                    fam.name, d, kN);
        totalDiff += d;
    }
    EXPECT_EQ(0u, totalDiff) << "the shipped path must compute what the probe measured";

    // (2) the rates 6d was chosen for, through the shipped entry point
    auto shippedDet = [](std::vector<float>& f, double* ms) { return detectShipped(f, ms); };
    std::printf("\n| family     | FP total | pooled FPR | req | TPR@10s |\n");
    std::printf("|------------|----------|------------|-----|---------|\n");
    for (const Fam& fam : fams) {
        size_t fp = 0;
        for (uint32_t sd : seeds) {
            Frame clean = fam.make(sd);
            const std::vector<uint8_t> flagged = detectShipped(clean.px, nullptr);
            for (size_t i = 0; i < kN; ++i) if (flagged[i]) ++fp;
        }
        const double pooled = static_cast<double>(fp) / (static_cast<double>(kN) * nSeeds);
        const Result t = measureOn(fam.make, 10.0f, 20260911u, shippedDet);
        std::printf("| %-10s | %8zu | %.4e | %-3s | %.4f  |\n",
                    fam.name, fp, pooled, (pooled < kFprCap) ? "YES" : "no", t.tpr);
    }

    // (3) is AVX2 still alive? Compare the shipped path against the probe's
    // scalar loop on the same frame. Before 6d the ratio was 8.4x.
    Frame f = uniform10(20260911u);
    std::vector<float> a = f.px, b = f.px;
    double msShipped = 0.0, msScalar = 0.0;
    detectShipped(a, &msShipped);
    detectVariant(v6d, b, &msScalar);
    std::printf("\n[a164] TIMING shipped %.1f ms vs probe scalar %.1f ms  -> %.1fx\n",
                msShipped, msScalar, msScalar / msShipped);
}

// QA-A-164: WHERE the shipped time goes now.
//
// 6d was chosen partly on "1.41x the scalar baseline". On the shipped path the
// measured cost is 2.68x (69.7 -> 187 ms at 3072^2), so the basis moved and the
// difference has to be attributed before anyone decides what to do about it.
TEST_F(A161Probe, DISABLED_WhereTheShippedTimeGoes) {
    constexpr uint32_t kBigW = 3072, kBigH = 3072;
    const size_t bigN = static_cast<size_t>(kBigW) * kBigH;
    std::mt19937 rng(20260911u);
    std::normal_distribution<float> g(0.0f, 1.0f);
    std::vector<float> frame(bigN);
    for (size_t i = 0; i < bigN; ++i) frame[i] = 3000.0f + 10.0f * g(rng);

    XpeImageBuffer img{};
    img.data = frame.data(); img.width = kBigW; img.height = kBigH;
    img.bitsAllocated = 32; img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.dataSize = static_cast<uint32_t>(bigN * sizeof(float));

    for (int run = 0; run < 3; ++run) {
        auto t0 = std::chrono::steady_clock::now();
        const float gs = ComputeGlobalSigma(&img);
        auto t1 = std::chrono::steady_clock::now();
        uint32_t tilesX = 0;
        std::vector<float> table = ComputeTileSigmas(&img, RUNTIME_DETECTION_TILE_SIZE, &tilesX);
        auto t2 = std::chrono::steady_clock::now();

        RuntimeDetectionConfig cfg = RuntimeDetection_DefaultConfig();
        cfg.globalSigmaFloor = RUNTIME_DETECTION_GLOBAL_SIGMA_FLOOR * gs;
        cfg.blendWeight = RUNTIME_DETECTION_BLEND_WEIGHT;
        cfg.tileSigma = table.data();
        cfg.tileSize = RUNTIME_DETECTION_TILE_SIZE;
        cfg.tilesX = tilesX;
        cfg.blendReference = gs;
        std::vector<uint8_t> map(bigN, 0);
        std::vector<float> wv, dev;
        auto t3 = std::chrono::steady_clock::now();
        DetectRowRange(&img, cfg, map.data(), 0u, kBigH, wv, dev);
        auto t4 = std::chrono::steady_clock::now();

        const double msGlobal = std::chrono::duration<double, std::milli>(t1 - t0).count();
        const double msTile   = std::chrono::duration<double, std::milli>(t2 - t1).count();
        const double msRows   = std::chrono::duration<double, std::milli>(t4 - t3).count();
        std::printf("[a164] 3072^2 run %d : globalSigma %6.1f | tileSigmas %6.1f | rows %6.1f | sum %6.1f ms\n",
                    run, msGlobal, msTile, msRows, msGlobal + msTile + msRows);
    }
}

// QA-A-164 FALSIFICATION: the claim is that the two mechanisms are ORTHOGONAL --
// neither alone reaches the requirement. Turning each off should break a
// different column. If turning one off changes nothing, it is not load-bearing.
TEST_F(A161Probe, DISABLED_OrthogonalityFalsification) {
    const uint32_t seeds[] = {20260911u, 7u, 1234u, 99991u, 424242u};
    const size_t nSeeds = sizeof(seeds) / sizeof(seeds[0]);
    struct Fam { const char* name; Frame (*make)(uint32_t); };
    const Fam fams[] = {
        {"uniform", uniform10}, {"scatter", scatterFrame},
        {"edge", edgeFrame}, {"lines(p13)", lines13Frame},
    };
    const Variant cands[] = {
        {"landed 6d",            3, SigmaRule::TileBlend,    0.10f, 128},
        {"blend OFF (w=1, tile)",3, SigmaRule::TileBlend,    1.00f, 128},
        {"tile OFF (frame ref)", 3, SigmaRule::Blend,        0.10f,   0},
    };
    std::printf("\n| variant                | uniform    | scatter    | edge       | lines      |\n");
    std::printf("|------------------------|------------|------------|------------|------------|\n");
    for (const Variant& v : cands) {
        std::printf("| %-22s |", v.name);
        for (const Fam& fam : fams) {
            size_t fp = 0;
            for (uint32_t sd : seeds) {
                Frame clean = fam.make(sd);
                const std::vector<uint8_t> flagged = detectVariant(v, clean.px, nullptr);
                for (size_t i = 0; i < kN; ++i) if (flagged[i]) ++fp;
            }
            const double pooled = static_cast<double>(fp) / (static_cast<double>(kN) * nSeeds);
            std::printf(" %.3e%s|", pooled, (pooled < kFprCap) ? " " : "*");
        }
        std::printf("\n");
    }
    std::printf("(* = over the 1e-5 requirement)\n");
}
