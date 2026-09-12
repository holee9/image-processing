/**
 * @file xpe_struct_frames.cpp
 * @brief QA-A-47 (#143): does beta flag good pixels where the noise is structured?
 *
 * EXPERIMENT ONLY. Nothing here ships. QA-A-46 measured the global sigma floor
 * and ceiling on a UNIFORM Gaussian frame and recorded the limit that mattered:
 * on such a frame every region has the same noise, so "the local sigma estimate
 * is 1.2x the global one" can only ever mean estimation error. A real frame is
 * not like that. This tool builds three frames that are not like that either,
 * and measures what the floor and the ceiling do there.
 *
 * These are SIMULATIONS. Calling them real frames would be a lie, and whether
 * they represent a real detector is not something this tool can answer.
 *
 * Three structures, each carrying regions whose true sigma exceeds 1.2x the
 * frame's own average noise:
 *
 *   scatter   a smooth brightness ramp with signal-dependent noise, sigma
 *             proportional to sqrt(I) -- the shape photon statistics give
 *   edge      a step in brightness with different noise on each side, the way
 *             an anatomical boundary separates two exposure regimes
 *   lines     periodic row stripes (grid / detector line artefact) on top of
 *             uniform noise
 *
 * The clean versions come first: false positives are the primary question.
 *
 * Usage: xpe_struct_frames            full report
 *        xpe_struct_frames --verify   cross-check one cell against the DLL entry
 */

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "runtime_detection.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kW = 1024;
constexpr uint32_t kH = 1024;
constexpr size_t   kN = static_cast<size_t>(kW) * kH;

using xpe::preprocess::internal::CollectNeighborValues;
using xpe::preprocess::internal::ComputeMedian;
using xpe::preprocess::internal::ComputeMAD;
using xpe::preprocess::internal::ComputeGlobalSigma;

/* ------------------------------------------------------------------ frames */

struct Frame {
    std::string        name;
    std::vector<float> pixels;
    std::vector<float> trueSigma;   // the sigma actually used at each pixel
    // Returns true when (x,y) sits inside the frame's structural feature.
    bool (*inStructure)(uint32_t x, uint32_t y);
    double structureAreaFraction;
};

bool scatterStructure(uint32_t, uint32_t y) {
    // The brighter half carries the higher sigma (noise grows with signal).
    return y >= kH / 2;
}
bool edgeStructure(uint32_t x, uint32_t) {
    // Within 2 pixels of the step -- the neighbourhood straddles both levels.
    return x >= kW / 2 - 2 && x < kW / 2 + 2;
}
bool lineStructure(uint32_t, uint32_t y) {
    return (y % 8u) == 0u;    // the stripe rows themselves
}

/** Signal-dependent noise on a smooth ramp: sigma = 0.35 * sqrt(I). */
Frame makeScatter(uint32_t seed) {
    Frame f;
    f.name = "scatter";
    f.inStructure = &scatterStructure;
    f.structureAreaFraction = 0.5;
    f.pixels.resize(kN);
    f.trueSigma.resize(kN);
    std::mt19937 rng(seed);
    std::normal_distribution<float> unit(0.0f, 1.0f);
    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            const double ramp = static_cast<double>(y) / (kH - 1);
            const float I = static_cast<float>(2000.0 + 1200.0 * ramp);
            const float s = 0.35f * std::sqrt(I);
            const size_t i = static_cast<size_t>(y) * kW + x;
            f.trueSigma[i] = s;
            f.pixels[i] = I + s * unit(rng);
        }
    }
    return f;
}

/** A brightness step with a different sigma on each side. */
Frame makeEdge(uint32_t seed) {
    Frame f;
    f.name = "edge";
    f.inStructure = &edgeStructure;
    f.structureAreaFraction = 4.0 / static_cast<double>(kW);
    f.pixels.resize(kN);
    f.trueSigma.resize(kN);
    std::mt19937 rng(seed);
    std::normal_distribution<float> unit(0.0f, 1.0f);
    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            const bool right = x >= kW / 2;
            const float I = right ? 3000.0f : 1500.0f;
            const float s = right ? 25.0f : 12.0f;
            const size_t i = static_cast<size_t>(y) * kW + x;
            f.trueSigma[i] = s;
            f.pixels[i] = I + s * unit(rng);
        }
    }
    return f;
}

/** Periodic row stripes on uniform noise. */
Frame makeLines(uint32_t seed) {
    Frame f;
    f.name = "lines";
    f.inStructure = &lineStructure;
    f.structureAreaFraction = 1.0 / 8.0;
    f.pixels.resize(kN);
    f.trueSigma.resize(kN);
    std::mt19937 rng(seed);
    std::normal_distribution<float> unit(0.0f, 1.0f);
    for (uint32_t y = 0; y < kH; ++y) {
        const float stripe = ((y % 8u) == 0u) ? 40.0f : 0.0f;
        for (uint32_t x = 0; x < kW; ++x) {
            const size_t i = static_cast<size_t>(y) * kW + x;
            f.trueSigma[i] = 12.0f;
            f.pixels[i] = 2500.0f + stripe + 12.0f * unit(rng);
        }
    }
    return f;
}

XpeImageBuffer wrap(std::vector<float>& p) {
    XpeImageBuffer img{};
    img.data = p.data();
    img.width = kW; img.height = kH;
    img.bitsAllocated = 32; img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.dataSize = static_cast<uint32_t>(kN * sizeof(float));
    return img;
}

/* ----------------------------------------------------------- measurements */

struct Quantiles { float lo, med, hi; };

Quantiles quantiles(std::vector<float> v) {
    Quantiles q{0, 0, 0};
    if (v.empty()) return q;
    const size_t n = v.size();
    auto nth = [&](double frac) {
        const size_t k = static_cast<size_t>(frac * static_cast<double>(n - 1));
        std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(k), v.end());
        return v[k];
    };
    q.lo = nth(0.0); q.med = nth(0.5); q.hi = nth(1.0);
    return q;
}

/** The local sigma estimate the detector would compute at each pixel. */
std::vector<float> localSigmaMap(const XpeImageBuffer& img, int32_t window) {
    std::vector<float> out(kN, 0.0f);
    std::vector<float> nbrs, dev;
    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            CollectNeighborValues(&img, x, y, window, nbrs);
            if (nbrs.size() < RUNTIME_DETECTION_MIN_NEIGHBORS) continue;
            const float m = ComputeMedian(nbrs);
            dev = nbrs;
            out[static_cast<size_t>(y) * kW + x] = ComputeMAD(dev, m);
        }
    }
    return out;
}

/** Runs the shipped rule with an explicit (floor, cap) and returns the map. */
std::vector<uint8_t> detect(const XpeImageBuffer& img, float floor, float cap) {
    RuntimeDetectionConfig cfg = RuntimeDetection_DefaultConfig();
    cfg.globalSigmaFloor = floor;
    cfg.globalSigmaCap = cap;

    std::vector<uint8_t> map(kN, 0);
    std::vector<float> nbrs, dev;
    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            if (xpe::preprocess::internal::DetectDefectivePixel(&img, x, y, cfg, nbrs, dev)) {
                map[static_cast<size_t>(y) * kW + x] = 1;
            }
        }
    }
    return map;
}

struct FpStats {
    size_t total = 0;
    size_t inStructure = 0;
    double enrichment = 0.0;   // (fp in structure / fp total) / area fraction
};

FpStats classify(const std::vector<uint8_t>& map, const Frame& f) {
    FpStats s;
    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            if (!map[static_cast<size_t>(y) * kW + x]) continue;
            ++s.total;
            if (f.inStructure(x, y)) ++s.inStructure;
        }
    }
    if (s.total > 0 && f.structureAreaFraction > 0.0) {
        s.enrichment = (static_cast<double>(s.inStructure) / static_cast<double>(s.total))
                     / f.structureAreaFraction;
    }
    return s;
}

std::vector<size_t> defectSites() {
    std::vector<size_t> sites;
    for (uint32_t y = 16; y < kH - 16; y += 32) {
        for (uint32_t x = 16; x < kW - 16; x += 32) {
            sites.push_back(static_cast<size_t>(y) * kW + x);
        }
    }
    return sites;
}

void reportFrame(Frame& f) {
    XpeImageBuffer img = wrap(f.pixels);
    const float sigmaGlobal = ComputeGlobalSigma(&img);
    const Quantiles ts = quantiles(f.trueSigma);
    const std::vector<float> localMap = localSigmaMap(img, RUNTIME_DETECTION_DEFAULT_WINDOW_SIZE);
    const Quantiles ls = quantiles(localMap);

    std::printf("\n================ frame: %s ================\n", f.name.c_str());
    std::printf("  ComputeGlobalSigma (MAD*1.4826 of VALUES) = %10.4f\n", sigmaGlobal);
    std::printf("  true sigma      min %8.4f  med %8.4f  max %8.4f\n", ts.lo, ts.med, ts.hi);
    std::printf("  local estimate  min %8.4f  med %8.4f  max %8.4f\n", ls.lo, ls.med, ls.hi);
    std::printf("  ratio: global / true-median = %8.4f   local-median / true-median = %8.4f\n",
                sigmaGlobal / ts.med, ls.med / ts.med);

    const float alpha = 0.95f;
    const float betas[] = {0.0f, 1.2f, 1.3f, 1.5f};

    std::printf("\n  --- clean frame: false positives at alpha=%.2f ---\n", alpha);
    std::printf("   beta     floor      cap        FP    in-struct  enrichment\n");
    for (float b : betas) {
        const float floor = alpha * sigmaGlobal;
        const float cap = b * sigmaGlobal;
        const std::vector<uint8_t> map = detect(img, floor, cap);
        const FpStats s = classify(map, f);
        std::printf("   %4.2f  %9.3f  %9.3f  %8zu  %8zu   %8.2fx\n",
                    b, floor, (b > 0.0f ? cap : 0.0f), s.total, s.inStructure, s.enrichment);
        std::fflush(stdout);
    }

    // --- injected version -------------------------------------------------
    const std::vector<size_t> sites = defectSites();
    std::vector<float> injected = f.pixels;
    for (size_t site : sites) injected[site] += 10.0f * f.trueSigma[site];
    XpeImageBuffer injImg = wrap(injected);

    std::printf("\n  --- 10-sigma injected: FN at alpha=%.2f ---\n", alpha);
    std::printf("   beta        TP        FN      TPR      extra-FP\n");
    for (float b : betas) {
        const float floor = alpha * sigmaGlobal;
        const float cap = b * sigmaGlobal;
        const std::vector<uint8_t> map = detect(injImg, floor, cap);
        size_t tp = 0;
        for (size_t site : sites) if (map[site]) ++tp;
        size_t flagged = 0;
        for (uint8_t v : map) if (v) ++flagged;
        std::printf("   %4.2f  %8zu  %8zu  %7.6f  %10zu\n",
                    b, tp, sites.size() - tp,
                    static_cast<double>(tp) / static_cast<double>(sites.size()),
                    flagged - tp);
        std::fflush(stdout);
    }
}

/**
 * Cross-check: the tool re-derives the shipped rule rather than calling the DLL
 * entry point, so one cell is verified against xpe_defect_detect_runtime with
 * the compiled-in constants.
 */
void verifyAgainstEntryPoint() {
    Frame f = makeLines(4242u);
    XpeImageBuffer img = wrap(f.pixels);
    const float sigmaGlobal = ComputeGlobalSigma(&img);

    const std::vector<uint8_t> mine =
        detect(img, RUNTIME_DETECTION_GLOBAL_SIGMA_FLOOR * sigmaGlobal,
               RUNTIME_DETECTION_GLOBAL_SIGMA_CAP * sigmaGlobal);

    std::vector<uint8_t> theirs(kN, 0);
    XpeImageBuffer out{};
    out.data = theirs.data();
    out.width = kW; out.height = kH;
    out.bitsAllocated = 8; out.bitsStored = 8;
    out.format = XPE_PIXEL_UINT8;
    out.dataSize = static_cast<uint32_t>(kN);
    XpeImageMetadata meta{};
    const XpeErrorCode rc = xpe_defect_detect_runtime(&img, &meta, &out);

    size_t diff = 0, mineFlag = 0, theirsFlag = 0;
    for (size_t i = 0; i < kN; ++i) {
        if (mine[i]) ++mineFlag;
        if (theirs[i]) ++theirsFlag;
        if (mine[i] != theirs[i]) ++diff;
    }
    std::printf("[verify] rc=%d  tool-flagged=%zu  dll-flagged=%zu  mismatches=%zu\n",
                static_cast<int>(rc), mineFlag, theirsFlag, diff);
}

} // namespace

int main(int argc, char** argv) {
    bool verifyOnly = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--verify") == 0) verifyOnly = true;
    }

    if (xpe_preprocess_init(nullptr) != XPE_OK) {
        std::fprintf(stderr, "xpe_preprocess_init failed\n");
        return 1;
    }

    if (verifyOnly) {
        verifyAgainstEntryPoint();
        xpe_preprocess_shutdown();
        return 0;
    }

    std::printf("QA-A-47 structured-frame probe. 1024x1024, seed 20260912.\n");
    std::printf("These are SIMULATIONS, not real detector frames.\n");
    std::printf("alpha fixed at 0.95 (the QA-A-44 minimum meeting the FPR requirement).\n");

    Frame scatter = makeScatter(20260912u);
    Frame edge    = makeEdge(20260912u);
    Frame lines   = makeLines(20260912u);
    reportFrame(scatter);
    reportFrame(edge);
    reportFrame(lines);

    xpe_preprocess_shutdown();
    return 0;
}
