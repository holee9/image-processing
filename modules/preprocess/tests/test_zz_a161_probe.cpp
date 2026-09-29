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
    if (v.rule == SigmaRule::TilePooled) {
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
