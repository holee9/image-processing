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

bool noStructure(uint32_t, uint32_t) { return false; }

/** The QA-A-46 frame: uniform noise, no structure at all. */
Frame makeUniform(uint32_t seed) {
    Frame f;
    f.name = "uniform";
    f.inStructure = &noStructure;
    f.structureAreaFraction = 0.0;
    f.pixels.resize(kN);
    f.trueSigma.assign(kN, 10.0f);
    std::mt19937 rng(seed);
    std::normal_distribution<float> noise(3000.0f, 10.0f);
    for (size_t i = 0; i < kN; ++i) f.pixels[i] = noise(rng);
    return f;
}

bool checker2Structure(uint32_t x, uint32_t y) { return ((x / 2u) + (y / 2u)) % 2u == 0u; }
bool checker8Structure(uint32_t x, uint32_t y) { return ((x / 8u) + (y / 8u)) % 2u == 0u; }
bool diagStructure(uint32_t x, uint32_t y)     { return ((x + y) % 3u) == 0u; }

/**
 * QA-A-49 (#148): structures that corrupt the horizontal AND vertical
 * difference statistics at the same time.
 *
 * min(h, v) works by assuming one direction stays clean. A checkerboard whose
 * cells are smaller than the 3x3 window breaks that assumption: every adjacent
 * pair, in either direction, straddles the pattern. A 45-degree stripe does the
 * same, because x+y advances by one for a step in either axis.
 *
 * @param cell  checker cell size in pixels. 2 is smaller than the 3x3 window,
 *              8 is larger -- the card asks for one of each.
 */
Frame makeChecker(uint32_t seed, uint32_t cell) {
    Frame f;
    f.name = (cell <= 2u) ? "checker2" : "checker8";
    f.inStructure = (cell <= 2u) ? &checker2Structure : &checker8Structure;
    f.structureAreaFraction = 0.5;
    f.pixels.resize(kN);
    f.trueSigma.assign(kN, 12.0f);
    std::mt19937 rng(seed);
    std::normal_distribution<float> unit(0.0f, 1.0f);
    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            const bool high = ((x / cell) + (y / cell)) % 2u == 0u;
            const size_t i = static_cast<size_t>(y) * kW + x;
            f.pixels[i] = 2500.0f + (high ? 40.0f : 0.0f) + 12.0f * unit(rng);
        }
    }
    return f;
}

/** 45-degree stripes, period 3: a step in x or in y both cross the pattern. */
Frame makeDiag(uint32_t seed) {
    Frame f;
    f.name = "diag";
    f.inStructure = &diagStructure;
    f.structureAreaFraction = 1.0 / 3.0;
    f.pixels.resize(kN);
    f.trueSigma.assign(kN, 12.0f);
    std::mt19937 rng(seed);
    std::normal_distribution<float> unit(0.0f, 1.0f);
    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            const float stripe = (((x + y) % 3u) == 0u) ? 40.0f : 0.0f;
            const size_t i = static_cast<size_t>(y) * kW + x;
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

/* ------------------------------------------------------------ estimators */
//
// QA-A-48 (#148). Four ways to estimate the noise sigma of a whole frame.
// The shipped one (A) measures the spread of the VALUES, which on a structured
// frame is the spread of the STRUCTURE -- A-47 measured 25x and 165x over the
// true noise. The others are built to cancel structure before measuring.

/** A: the shipped estimator -- MAD of the pixel values. */
float estValueMad(const std::vector<float>& px) {
    std::vector<float> w = px;
    const size_t mid = w.size() / 2;
    std::nth_element(w.begin(), w.begin() + static_cast<std::ptrdiff_t>(mid), w.end());
    const float med = w[mid];
    for (float& v : w) v = std::fabs(v - med);
    std::nth_element(w.begin(), w.begin() + static_cast<std::ptrdiff_t>(mid), w.end());
    return w[mid] * RUNTIME_DETECTION_MAD_SCALE;
}

/**
 * B: MAD of adjacent-pixel differences, horizontal and vertical pooled.
 *
 * A difference of two neighbouring pixels cancels any structure that is smooth
 * at the one-pixel scale, and leaves the difference of two independent noise
 * draws. That difference has standard deviation sqrt(2) * sigma, so the
 * estimate must be divided by sqrt(2):
 *
 *     d = I(x+1,y) - I(x,y) = (s(x+1,y) - s(x,y)) + (n1 - n2)
 *     Var(n1 - n2) = 2 * sigma^2   ->   sd(d) = sqrt(2) * sigma
 *     sigma_hat = MAD(d) * 1.4826 / sqrt(2)
 *
 * Dropping the sqrt(2) overestimates by 41% -- silently, since nothing else
 * would look wrong.
 */
float estDiffMad(const std::vector<float>& px) {
    std::vector<float> d;
    d.reserve(2 * kN);
    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x + 1 < kW; ++x) {
            const size_t i = static_cast<size_t>(y) * kW + x;
            d.push_back(px[i + 1] - px[i]);
        }
    }
    for (uint32_t y = 0; y + 1 < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            const size_t i = static_cast<size_t>(y) * kW + x;
            d.push_back(px[i + kW] - px[i]);
        }
    }
    const size_t mid = d.size() / 2;
    std::nth_element(d.begin(), d.begin() + static_cast<std::ptrdiff_t>(mid), d.end());
    const float med = d[mid];
    for (float& v : d) v = std::fabs(v - med);
    std::nth_element(d.begin(), d.begin() + static_cast<std::ptrdiff_t>(mid), d.end());
    return d[mid] * RUNTIME_DETECTION_MAD_SCALE / 1.41421356f;
}

/**
 * B-h: the same statistic from HORIZONTAL differences only.
 *
 * Memory matters at 3072x3072: pooling both directions builds 2n differences
 * (75 MB) where the shipped estimator copies n floats (36 MB). Horizontal-only
 * keeps the footprint at today's level. The trade is directional -- it is blind
 * to column-oriented artefacts in the same way pooling is half-blind to both.
 */
float estDiffMadH(const std::vector<float>& px) {
    std::vector<float> d;
    d.reserve(kN);
    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x + 1 < kW; ++x) {
            const size_t i = static_cast<size_t>(y) * kW + x;
            d.push_back(px[i + 1] - px[i]);
        }
    }
    const size_t mid = d.size() / 2;
    std::nth_element(d.begin(), d.begin() + static_cast<std::ptrdiff_t>(mid), d.end());
    const float med = d[mid];
    for (float& v : d) v = std::fabs(v - med);
    std::nth_element(d.begin(), d.begin() + static_cast<std::ptrdiff_t>(mid), d.end());
    return d[mid] * RUNTIME_DETECTION_MAD_SCALE / 1.41421356f;
}

/**
 * Bm: min(horizontal, vertical) difference-MAD.
 *
 * Horizontal-only is blind to column artefacts and vertical-only to row ones.
 * Taking the smaller of the two picks whichever direction the structure did NOT
 * corrupt: a row stripe inflates the vertical statistic and leaves the
 * horizontal one clean, and vice versa. Peak memory stays at one difference
 * array because the two passes run in sequence.
 */
float estDiffMadMin(const std::vector<float>& px) {
    auto madOf = [](std::vector<float>& d) {
        const size_t mid = d.size() / 2;
        std::nth_element(d.begin(), d.begin() + static_cast<std::ptrdiff_t>(mid), d.end());
        const float med = d[mid];
        for (float& v : d) v = std::fabs(v - med);
        std::nth_element(d.begin(), d.begin() + static_cast<std::ptrdiff_t>(mid), d.end());
        return d[mid] * RUNTIME_DETECTION_MAD_SCALE / 1.41421356f;
    };

    std::vector<float> d;
    d.reserve(kN);
    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x + 1 < kW; ++x) {
            const size_t i = static_cast<size_t>(y) * kW + x;
            d.push_back(px[i + 1] - px[i]);
        }
    }
    const float h = madOf(d);

    d.clear();
    for (uint32_t y = 0; y + 1 < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            const size_t i = static_cast<size_t>(y) * kW + x;
            d.push_back(px[i + kW] - px[i]);
        }
    }
    const float v = madOf(d);
    return (h < v) ? h : v;
}

/**
 * C: Immerkaer's Laplacian estimator (J. Immerkaer, "Fast Noise Variance
 * Estimation", CVIU 1996). Convolve with
 *     [ 1 -2  1 ; -2  4 -2 ;  1 -2  1 ]
 * which annihilates any locally linear signal, then
 *     sigma = sqrt(pi/2) / (6 * (W-2) * (H-2)) * sum |response|
 * The 6 is the L2 norm of the mask and sqrt(pi/2) converts a mean absolute
 * deviation to a standard deviation for Gaussian noise.
 */
float estImmerkaer(const std::vector<float>& px) {
    double acc = 0.0;
    for (uint32_t y = 1; y + 1 < kH; ++y) {
        for (uint32_t x = 1; x + 1 < kW; ++x) {
            const size_t i = static_cast<size_t>(y) * kW + x;
            const double r =
                  1.0 * px[i - kW - 1] - 2.0 * px[i - kW] + 1.0 * px[i - kW + 1]
                - 2.0 * px[i - 1]      + 4.0 * px[i]      - 2.0 * px[i + 1]
                + 1.0 * px[i + kW - 1] - 2.0 * px[i + kW] + 1.0 * px[i + kW + 1];
            acc += std::fabs(r);
        }
    }
    const double n = static_cast<double>(kW - 2) * static_cast<double>(kH - 2);
    return static_cast<float>(std::sqrt(3.14159265358979 / 2.0) * acc / (6.0 * n));
}

/**
 * D: median of the per-pixel local MAD estimates. Structure only inflates the
 * minority of pixels that sit on it, and a median ignores a minority. Costs a
 * full neighbourhood pass -- as expensive as the detection itself.
 */
float estLocalMadMedian(const std::vector<float>& local) {
    std::vector<float> w;
    w.reserve(local.size());
    for (float v : local) if (v > 0.0f) w.push_back(v);
    if (w.empty()) return 0.0f;
    const size_t mid = w.size() / 2;
    std::nth_element(w.begin(), w.begin() + static_cast<std::ptrdiff_t>(mid), w.end());
    return w[mid];
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

/** QA-A-48 §1: every estimator against every frame. */
void compareEstimators() {
    std::vector<Frame> frames;
    frames.push_back(makeUniform(20260911u));
    frames.push_back(makeScatter(20260912u));
    frames.push_back(makeEdge(20260912u));
    frames.push_back(makeLines(20260912u));
    frames.push_back(makeChecker(20260912u, 2u));
    frames.push_back(makeChecker(20260912u, 8u));
    frames.push_back(makeDiag(20260912u));

    std::printf("QA-A-48 estimator comparison. 1024x1024.\n");
    std::printf("A = MAD of values (shipped)   B = MAD of adjacent differences / sqrt(2)\n");
    std::printf("C = Immerkaer Laplacian       D = median of per-pixel local MAD\n\n");
    std::printf("  frame     true-sigma-med        A  ratio        B  ratio"
                "       Bm  ratio        C  ratio        D  ratio\n");

    for (Frame& f : frames) {
        XpeImageBuffer img = wrap(f.pixels);
        const Quantiles ts = quantiles(f.trueSigma);
        const std::vector<float> local =
            localSigmaMap(img, RUNTIME_DETECTION_DEFAULT_WINDOW_SIZE);

        const float a = estValueMad(f.pixels);
        const float b = estDiffMad(f.pixels);
        const float bh = estDiffMadMin(f.pixels);
        const float c = estImmerkaer(f.pixels);
        const float d = estLocalMadMedian(local);

        std::printf("  %-9s %13.4f %9.3f %6.2fx %9.3f %6.2fx %9.3f %6.2fx %9.3f %6.2fx %9.3f %6.2fx\n",
                    f.name.c_str(), ts.med,
                    a, a / ts.med, b, b / ts.med, bh, bh / ts.med,
                    c, c / ts.med, d, d / ts.med);
        std::fflush(stdout);
    }
}

/** QA-A-48 §2: the A-43..A-47 tables, re-run under the new estimator. */
void regress() {
    struct Setting { const char* label; float alpha; float beta; };
    const Setting settings[] = {
        {"shipped (a=0.80, cap off)", 0.80f, 0.00f},
        {"candidate (a=0.95, b=1.2)", 0.95f, 1.20f},
    };

    std::vector<Frame> frames;
    frames.push_back(makeUniform(20260911u));
    frames.push_back(makeScatter(20260912u));
    frames.push_back(makeEdge(20260912u));
    frames.push_back(makeLines(20260912u));
    frames.push_back(makeChecker(20260912u, 2u));
    frames.push_back(makeChecker(20260912u, 8u));
    frames.push_back(makeDiag(20260912u));

    const std::vector<size_t> sites = defectSites();

    for (const Setting& st : settings) {
        std::printf("\n===== %s =====\n", st.label);
        std::printf("  frame      sigma_g   TPR@5s   TPR@6s   TPR@8s  TPR@10s  FN@10s"
                    "   cleanFP    cleanFPR  1%%cap  enrich\n");

        for (Frame& f : frames) {
            XpeImageBuffer img = wrap(f.pixels);
            const float sg = ComputeGlobalSigma(&img);
            const float floor = st.alpha * sg;
            const float cap = st.beta * sg;

            const std::vector<uint8_t> cleanMap = detect(img, floor, cap);
            const FpStats fp = classify(cleanMap, f);
            const double fpr = static_cast<double>(fp.total) / static_cast<double>(kN);

            double tpr[4] = {0, 0, 0, 0};
            size_t fn10 = 0;
            const float amps[4] = {5.0f, 6.0f, 8.0f, 10.0f};
            for (int k = 0; k < 4; ++k) {
                std::vector<float> inj = f.pixels;
                for (size_t site : sites) inj[site] += amps[k] * f.trueSigma[site];
                XpeImageBuffer ii = wrap(inj);
                const std::vector<uint8_t> m = detect(ii, floor, cap);
                size_t tp = 0;
                for (size_t site : sites) if (m[site]) ++tp;
                tpr[k] = static_cast<double>(tp) / static_cast<double>(sites.size());
                if (k == 3) fn10 = sites.size() - tp;
            }

            std::printf("  %-9s %9.3f %8.6f %8.6f %8.6f %8.6f %7zu %9zu  %.3e   %-3s %6.2fx\n",
                        f.name.c_str(), sg, tpr[0], tpr[1], tpr[2], tpr[3], fn10,
                        fp.total, fpr,
                        (fp.total < kN / 100u) ? "ok" : "OVER", fp.enrichment);
            std::fflush(stdout);
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    bool verifyOnly = false, estOnly = false, regressOnly = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--verify") == 0) verifyOnly = true;
        if (std::strcmp(argv[i], "--estimators") == 0) estOnly = true;
        if (std::strcmp(argv[i], "--regress") == 0) regressOnly = true;
    }

    if (xpe_preprocess_init(nullptr) != XPE_OK) {
        std::fprintf(stderr, "xpe_preprocess_init failed\n");
        return 1;
    }

    if (regressOnly) {
        regress();
        xpe_preprocess_shutdown();
        return 0;
    }

    if (estOnly) {
        compareEstimators();
        xpe_preprocess_shutdown();
        return 0;
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
