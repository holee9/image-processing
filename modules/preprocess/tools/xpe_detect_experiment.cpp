/**
 * @file xpe_detect_experiment.cpp
 * @brief QA-A-41 (#143 #144 #120): candidate detector comparison harness.
 *
 * EXPERIMENT ONLY. Nothing here ships and nothing in modules/preprocess/src is
 * changed by this card: the three candidates live in this translation unit and
 * are measured against the same conditions QA-A-40 used, so the numbers can be
 * compared row by row with that report.
 *
 * The candidates reuse the SHIPPED primitives from runtime_detection.h --
 * CollectNeighborValues, ComputeMedian, ComputeMAD -- so a difference in the
 * table is a difference in the candidate's rule, not in a reimplemented median.
 * The baseline row calls the shipped DetectDefectivePixel unchanged.
 *
 * Candidates (leader's list, QA-A-41 section 1):
 *   (a) larger window   3x3 -> 5x5 / 7x7, to shrink the MAD estimate's spread
 *
 * QA-A-42 (#143) re-baselined every row: the shipped rule is now 3x3 EXCLUDING
 * the centre (8 values) with a 5-neighbour minimum, per REQ-P1A-013. The
 * candidates follow the same collection rule, so the table still compares
 * detection rules rather than neighbourhood conventions.
 *   (b) global sigma floor   sigma_use = max(sigma_local, alpha * sigma_global)
 *   (c) two stage   loose local pass (kappa = 4) then a 9x9 re-judge of the
 *                   candidates only
 *
 * Conditions (identical to QA-A-40): 1024x1024, mean 3000 ADU, sigma = 10,
 * 961 injected sites on a 32-pixel lattice with a 16-pixel margin, seeded
 * std::mt19937. Amplitudes +5 sigma and +10 sigma, plus one dead-pixel
 * combination at -5 sigma. Seeds 0, 1, 2.
 *
 * Usage: xpe_detect_experiment [--quick]
 *        --quick runs seed 0 only (for a smoke check, not for the report).
 */

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "runtime_detection.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <immintrin.h>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kW = 1024;
constexpr uint32_t kH = 1024;
constexpr size_t   kN = static_cast<size_t>(kW) * kH;
constexpr float    kMean  = 3000.0f;
constexpr float    kSigma = 10.0f;

using xpe::preprocess::internal::CollectNeighborValues;
using xpe::preprocess::internal::ComputeMedian;
using xpe::preprocess::internal::ComputeMAD;
using xpe::preprocess::internal::ComputeGlobalSigma;
using xpe::preprocess::internal::DetectDefectivePixel;

/* ---------------------------------------------------------------- frames */

std::vector<size_t> defectSites() {
    std::vector<size_t> sites;
    for (uint32_t y = 16; y < kH - 16; y += 32) {
        for (uint32_t x = 16; x < kW - 16; x += 32) {
            sites.push_back(static_cast<size_t>(y) * kW + x);
        }
    }
    return sites;
}

std::vector<float> cleanFrame(uint32_t seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<float> noise(kMean, kSigma);
    std::vector<float> frame(kN);
    for (size_t i = 0; i < kN; ++i) frame[i] = noise(rng);
    return frame;
}

XpeImageBuffer wrap(std::vector<float>& frame) {
    XpeImageBuffer img{};
    img.data = frame.data();
    img.width = kW; img.height = kH;
    img.bitsAllocated = 32; img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.dataSize = static_cast<uint32_t>(kN * sizeof(float));
    return img;
}

/* ------------------------------------------------------------ candidates */

/** Frame-wide robust sigma: MAD of the whole frame, scaled to a sigma. */
float globalSigma(const std::vector<float>& frame) {
    std::vector<float> work = frame;
    const size_t mid = work.size() / 2;
    std::nth_element(work.begin(), work.begin() + static_cast<long>(mid), work.end());
    const float median = work[mid];

    for (float& v : work) v = std::fabs(v - median);
    std::nth_element(work.begin(), work.begin() + static_cast<long>(mid), work.end());
    return work[mid] * RUNTIME_DETECTION_MAD_SCALE;
}

enum class Kind { Baseline, Window, GlobalFloor, TwoStage };

struct Variant {
    std::string label;
    Kind        kind;
    int32_t     windowSize;   // Baseline / Window / GlobalFloor
    float       kappa;
    float       alpha;        // GlobalFloor
    int32_t     stage2Window; // TwoStage
    float       stage1Kappa;  // TwoStage
};

/** One pixel decision under @p v. sigmaGlobal is used only by GlobalFloor. */
bool decide(const XpeImageBuffer* img, uint32_t x, uint32_t y,
            const Variant& v, float sigmaGlobal,
            std::vector<float>& scratch, std::vector<float>& scratch2) {
    const float* pixels = static_cast<const float*>(img->data);
    const float  center = pixels[static_cast<size_t>(y) * img->width + x];

    auto localStats = [&](int32_t window, float* medianOut, float* madOut) {
        CollectNeighborValues(img, x, y, window, scratch);
        if (scratch.size() < RUNTIME_DETECTION_MIN_NEIGHBORS) return false;
        *medianOut = ComputeMedian(scratch);
        scratch2 = scratch;
        *madOut = ComputeMAD(scratch2, *medianOut);
        return true;
    };

    float median = 0.0f, mad = 0.0f;

    switch (v.kind) {
    case Kind::Baseline: {
        RuntimeDetectionConfig cfg;
        cfg.windowSize = v.windowSize;
        cfg.sigmaThreshold = v.kappa;
        return DetectDefectivePixel(img, x, y, cfg);
    }
    case Kind::Window: {
        if (!localStats(v.windowSize, &median, &mad)) return false;
        if (mad < 1e-6f) return std::fabs(center - median) > 1e-6f;
        return std::fabs(center - median) > v.kappa * mad;
    }
    case Kind::GlobalFloor: {
        if (!localStats(v.windowSize, &median, &mad)) return false;
        // mad here is already MAD * 1.4826 (ComputeMAD applies the scale), so
        // it is directly comparable to sigmaGlobal.
        const float sigmaUse = std::max(mad, v.alpha * sigmaGlobal);
        if (sigmaUse < 1e-6f) return std::fabs(center - median) > 1e-6f;
        return std::fabs(center - median) > v.kappa * sigmaUse;
    }
    case Kind::TwoStage: {
        if (!localStats(v.windowSize, &median, &mad)) return false;
        const bool stage1 = (mad < 1e-6f)
            ? std::fabs(center - median) > 1e-6f
            : std::fabs(center - median) > v.stage1Kappa * mad;
        if (!stage1) return false;

        if (!localStats(v.stage2Window, &median, &mad)) return false;
        if (mad < 1e-6f) return std::fabs(center - median) > 1e-6f;
        return std::fabs(center - median) > v.kappa * mad;
    }
    }
    return false;
}

/** Runs @p v over the whole frame; returns the flagged map and the elapsed ms. */
std::vector<uint8_t> run(std::vector<float>& frame, const Variant& v, double* msOut) {
    XpeImageBuffer img = wrap(frame);
    const float sigmaGlobal =
        (v.kind == Kind::GlobalFloor) ? globalSigma(frame) : 0.0f;

    std::vector<uint8_t> map(kN, 0);
    std::vector<float> scratch, scratch2;
    scratch.reserve(128); scratch2.reserve(128);

    const auto t0 = std::chrono::steady_clock::now();
    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            if (decide(&img, x, y, v, sigmaGlobal, scratch, scratch2)) {
                map[static_cast<size_t>(y) * kW + x] = 1;
            }
        }
    }
    const auto t1 = std::chrono::steady_clock::now();
    *msOut = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return map;
}

/* ------------------------------------------------------------- measuring */

struct Row {
    double tpr5 = 0.0, tpr10 = 0.0, tprDead = 0.0, fpr = 0.0;
    size_t fp = 0;
    double msTpr = 0.0, msClean = 0.0;
};

Row measureOne(const Variant& v, uint32_t seed, bool withDead) {
    Row row;
    const std::vector<size_t> sites = defectSites();

    auto tprFor = [&](float amplitudeSigma, double* msOut) {
        std::vector<float> frame = cleanFrame(seed);
        for (size_t s : sites) frame[s] += amplitudeSigma * kSigma;
        const std::vector<uint8_t> map = run(frame, v, msOut);
        size_t tp = 0;
        for (size_t s : sites) if (map[s]) ++tp;
        return static_cast<double>(tp) / static_cast<double>(sites.size());
    };

    double ms = 0.0;
    row.tpr5  = tprFor(5.0f, &row.msTpr);
    row.tpr10 = tprFor(10.0f, &ms);
    if (withDead) row.tprDead = tprFor(-5.0f, &ms);

    std::vector<float> clean = cleanFrame(seed);
    const std::vector<uint8_t> cleanMap = run(clean, v, &row.msClean);
    for (size_t i = 0; i < kN; ++i) if (cleanMap[i]) ++row.fp;
    row.fpr = static_cast<double>(row.fp) / static_cast<double>(kN);
    return row;
}

/* ------------------------------------------------------------- profiling */

/**
 * Stage decomposition for #144. Each stage is the previous one plus one more
 * piece of work, so the differences are the per-stage costs. Measured rather
 * than reasoned about: no profiler is installed in this environment.
 */
void profile(int32_t window) {
    std::vector<float> frame = cleanFrame(0u);
    XpeImageBuffer img = wrap(frame);
    std::vector<float> scratch, scratch2;
    scratch.reserve(128); scratch2.reserve(128);

    volatile float sink = 0.0f;

    auto timeIt = [&](const char* what, int stage) {
        const auto t0 = std::chrono::steady_clock::now();
        for (uint32_t y = 0; y < kH; ++y) {
            for (uint32_t x = 0; x < kW; ++x) {
                CollectNeighborValues(&img, x, y, window, scratch);
                if (stage == 0) { sink = sink + scratch[0]; continue; }
                const float median = ComputeMedian(scratch);
                if (stage == 1) { sink = sink + median; continue; }
                scratch2 = scratch;
                sink = sink + ComputeMAD(scratch2, median);
            }
        }
        const auto t1 = std::chrono::steady_clock::now();
        std::printf("  %-34s %8.1f ms\n", what,
                    std::chrono::duration<double, std::milli>(t1 - t0).count());
        std::fflush(stdout);
    };

    std::printf("[profile] window %dx%d, 1024x1024, buffer-reusing loop\n",
                window, window);
    timeIt("gather only", 0);
    timeIt("gather + median", 1);
    timeIt("gather + median + copy + MAD", 2);

    // The allocating form, for the same window: this is what the shipped
    // DetectDefectivePixel does per pixel.
    RuntimeDetectionConfig cfg;
    cfg.windowSize = window;
    cfg.sigmaThreshold = 5.0f;
    const auto t0 = std::chrono::steady_clock::now();
    size_t flagged = 0;
    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            if (DetectDefectivePixel(&img, x, y, cfg)) ++flagged;
        }
    }
    const auto t1 = std::chrono::steady_clock::now();
    std::printf("  %-34s %8.1f ms  (flagged %zu)\n",
                "shipped DetectDefectivePixel",
                std::chrono::duration<double, std::milli>(t1 - t0).count(), flagged);
    std::printf("\n");
    std::fflush(stdout);
}

/**
 * Times the shipped entry point at the two frame sizes REQ-P1A-013 names, so
 * the SPEC performance row can be read against a measurement rather than an
 * extrapolation.
 */
void timeEntryPoint(uint32_t w, uint32_t h) {
    const size_t n = static_cast<size_t>(w) * h;
    std::mt19937 rng(0u);
    std::normal_distribution<float> noise(kMean, kSigma);
    std::vector<float> frame(n);
    for (size_t i = 0; i < n; ++i) frame[i] = noise(rng);

    XpeImageBuffer img{};
    img.data = frame.data();
    img.width = w; img.height = h;
    img.bitsAllocated = 32; img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.dataSize = static_cast<uint32_t>(n * sizeof(float));

    std::vector<uint8_t> map(n, 0);
    XpeImageBuffer out{};
    out.data = map.data();
    out.width = w; out.height = h;
    out.bitsAllocated = 8; out.bitsStored = 8;
    out.format = XPE_PIXEL_UINT8;
    out.dataSize = static_cast<uint32_t>(n);

    XpeImageMetadata meta{};
    double best = 1e30;
    for (int rep = 0; rep < 3; ++rep) {
        const auto t0 = std::chrono::steady_clock::now();
        const XpeErrorCode rc = xpe_defect_detect_runtime(&img, &meta, &out);
        const auto t1 = std::chrono::steady_clock::now();
        if (rc != XPE_OK) { std::fprintf(stderr, "detect failed %d\n", (int)rc); return; }
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        if (ms < best) best = ms;
    }
    size_t flagged = 0;
    for (uint8_t v : map) if (v) ++flagged;
    std::printf("[time] %ux%u  best of 3: %8.1f ms   flagged %zu (%.3f%%)\n",
                w, h, best, flagged, 100.0 * (double)flagged / (double)n);
    std::fflush(stdout);
}

/* ------------------------------------------------------- QA-A-56 bounds */
//
// MEASUREMENT ONLY. Nothing here changes the detector; the card's output is a
// number and a verdict. The AVX2 block below is a THROUGHPUT PROBE, not a
// vectorised detector -- it exists so the compute bound rests on what this CPU
// actually does rather than on a vendor table.

/** Timing helper: best of `reps`, in milliseconds. */
template <typename F>
double bestMs(int reps, F fn) {
    double best = 1e30;
    for (int r = 0; r < reps; ++r) {
        const auto t0 = std::chrono::steady_clock::now();
        fn();
        const auto t1 = std::chrono::steady_clock::now();
        const double v = std::chrono::duration<double, std::milli>(t1 - t0).count();
        if (v < best) best = v;
    }
    return best;
}

/** All of `reps`, so the spread is visible rather than hidden by a min. */
template <typename F>
std::vector<double> allMs(int reps, F fn) {
    std::vector<double> out;
    for (int r = 0; r < reps; ++r) {
        const auto t0 = std::chrono::steady_clock::now();
        fn();
        const auto t1 = std::chrono::steady_clock::now();
        out.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    return out;
}

void printSpread(const char* label, const std::vector<double>& v, double bytes) {
    std::vector<double> s2 = v;
    std::sort(s2.begin(), s2.end());
    const double lo = s2.front(), med = s2[s2.size() / 2u], hi = s2.back();
    std::printf("  %-34s min %7.2f  med %7.2f  max %7.2f ms"
                "   -> %5.1f GB/s (median)   spread %.1f%%\n",
                label, lo, med, hi,
                bytes / (med / 1000.0) / 1.0e9,
                100.0 * (hi - lo) / med);
    std::fflush(stdout);
}

void bounds() {
    constexpr uint32_t W = 3072, H = 3072;
    constexpr size_t NPX = static_cast<size_t>(W) * H;
    const double inBytes  = static_cast<double>(NPX) * sizeof(float);   // 37.75 MB
    const double outBytes = static_cast<double>(NPX) * sizeof(uint8_t); //  9.44 MB

    std::vector<float> frame(NPX);
    std::mt19937 rng(0u);
    std::normal_distribution<float> noise(kMean, kSigma);
    for (size_t i = 0; i < NPX; ++i) frame[i] = noise(rng);
    std::vector<uint8_t> map(NPX, 0u);
    std::vector<float> scratch(NPX, 0.0f);

    volatile float sink = 0.0f;
    volatile uint8_t sinkb = 0u;

    std::printf("[bounds] 3072x3072 float32 in (%.2f MB), uint8 map out (%.2f MB)\n\n",
                inBytes / 1.0e6, outBytes / 1.0e6);

    /* ---------------------------------------------------- memory bound */
    std::printf("A. MEMORY -- measured streaming kernels, 7 runs each\n");

    auto readKernel = [&]{
        float acc = 0.0f;
        for (size_t i = 0; i < NPX; ++i) acc += frame[i];
        sink = sink + acc;
    };
    auto copyKernel = [&]{
        std::memcpy(scratch.data(), frame.data(), NPX * sizeof(float));
        sink = sink + scratch[0];
    };
    // The traffic shape the detector actually has: read float32, write uint8.
    auto detectTrafficKernel = [&]{
        for (size_t i = 0; i < NPX; ++i) map[i] = (frame[i] > 3000.0f) ? 1u : 0u;
        sinkb = static_cast<uint8_t>(sinkb + map[0]);
    };

    printSpread("read-only (sum)", allMs(7, readKernel), inBytes);
    printSpread("copy (read+write float)", allMs(7, copyKernel), 2.0 * inBytes);
    const std::vector<double> traf = allMs(7, detectTrafficKernel);
    printSpread("detector traffic shape", traf, inBytes + outBytes);

    std::vector<double> trafSorted = traf;
    std::sort(trafSorted.begin(), trafSorted.end());
    const double memBoundMs = trafSorted[trafSorted.size() / 2u];
    std::printf("\n  MEMORY LOWER BOUND (read input once + write map once): %.2f ms\n\n",
                memBoundMs);

    /* --------------------------------------------------- compute bound */
    std::printf("B. COMPUTE -- the 19 compare-exchange network, measured\n");

    // Scalar: one pixel's network per iteration, register resident, no memory.
    constexpr size_t kIters = 4000000;
    auto scalarNet = [&]{
        float a0 = 1.f, a1 = 9.f, a2 = 3.f, a3 = 7.f, a4 = 5.f, a5 = 2.f, a6 = 8.f, a7 = 4.f;
        for (size_t it = 0; it < kIters; ++it) {
            a0 += 1.0f;   // keep each iteration distinct
            xpe::preprocess::internal::MedianSortCE(a0, a1);
            xpe::preprocess::internal::MedianSortCE(a2, a3);
            xpe::preprocess::internal::MedianSortCE(a4, a5);
            xpe::preprocess::internal::MedianSortCE(a6, a7);
            xpe::preprocess::internal::MedianSortCE(a0, a2);
            xpe::preprocess::internal::MedianSortCE(a1, a3);
            xpe::preprocess::internal::MedianSortCE(a4, a6);
            xpe::preprocess::internal::MedianSortCE(a5, a7);
            xpe::preprocess::internal::MedianSortCE(a1, a2);
            xpe::preprocess::internal::MedianSortCE(a5, a6);
            xpe::preprocess::internal::MedianSortCE(a0, a4);
            xpe::preprocess::internal::MedianSortCE(a1, a5);
            xpe::preprocess::internal::MedianSortCE(a2, a6);
            xpe::preprocess::internal::MedianSortCE(a3, a7);
            xpe::preprocess::internal::MedianSortCE(a2, a4);
            xpe::preprocess::internal::MedianSortCE(a3, a5);
            xpe::preprocess::internal::MedianSortCE(a1, a2);
            xpe::preprocess::internal::MedianSortCE(a3, a4);
            xpe::preprocess::internal::MedianSortCE(a5, a6);
            sink = sink + (a3 + a4);
        }
    };
    const double scalarMs = bestMs(3, scalarNet);
    const double scalarPerNet = scalarMs * 1.0e6 / static_cast<double>(kIters);  // ns
    std::printf("  scalar 19-CE network   %8.2f ms / %zu nets = %.2f ns per network\n",
                scalarMs, kIters, scalarPerNet);

    // AVX2: the SAME network, eight pixels at a time. min/max are lane-wise, so
    // one pixel per lane needs no shuffles -- this is the shape a vectorised
    // detector would take, and it is the most favourable case for AVX2 here.
    constexpr size_t kVecIters = 1000000;
    auto avx2Net = [&]{
        __m256 a0 = _mm256_set1_ps(1.f), a1 = _mm256_set1_ps(9.f);
        __m256 a2 = _mm256_set1_ps(3.f), a3 = _mm256_set1_ps(7.f);
        __m256 a4 = _mm256_set1_ps(5.f), a5 = _mm256_set1_ps(2.f);
        __m256 a6 = _mm256_set1_ps(8.f), a7 = _mm256_set1_ps(4.f);
        const __m256 one = _mm256_set1_ps(1.0f);
        float out[8];
        for (size_t it = 0; it < kVecIters; ++it) {
            a0 = _mm256_add_ps(a0, one);
#define CE8(x, y) { const __m256 lo = _mm256_min_ps(x, y); \
                    const __m256 hi = _mm256_max_ps(x, y); x = lo; y = hi; }
            CE8(a0, a1) CE8(a2, a3) CE8(a4, a5) CE8(a6, a7)
            CE8(a0, a2) CE8(a1, a3) CE8(a4, a6) CE8(a5, a7)
            CE8(a1, a2) CE8(a5, a6)
            CE8(a0, a4) CE8(a1, a5) CE8(a2, a6) CE8(a3, a7)
            CE8(a2, a4) CE8(a3, a5)
            CE8(a1, a2) CE8(a3, a4) CE8(a5, a6)
#undef CE8
            _mm256_storeu_ps(out, _mm256_add_ps(a3, a4));
            sink = sink + out[0];
        }
    };
    const double avx2Ms = bestMs(3, avx2Net);
    const double avx2PerNet = avx2Ms * 1.0e6 / (static_cast<double>(kVecIters) * 8.0);
    std::printf("  AVX2   19-CE network   %8.2f ms / %zu x8 nets = %.2f ns per network"
                "   (%.2fx scalar)\n", avx2Ms, kVecIters, avx2PerNet, scalarPerNet / avx2PerNet);

    // The detector runs the network TWICE per pixel (median, then MAD median).
    const double px = static_cast<double>(NPX);
    const double scalarNetBound = 2.0 * px * scalarPerNet / 1.0e6;   // ms
    const double avx2NetBound   = 2.0 * px * avx2PerNet / 1.0e6;     // ms
    std::printf("\n  two networks per pixel x %.0f pixels:\n", px);
    std::printf("    scalar networks alone      %8.2f ms\n", scalarNetBound);
    std::printf("    AVX2 networks alone        %8.2f ms\n", avx2NetBound);
    std::printf("    (networks ONLY -- gather, abs, threshold, map write and the\n"
                "     whole global-sigma stage are all extra)\n\n");

    /* ------------------------------------------------------- the verdict */
    const double bound = (memBoundMs > avx2NetBound) ? memBoundMs : avx2NetBound;
    std::printf("C. LOWER BOUND = max(memory %.2f, AVX2 compute %.2f) = %.2f ms\n",
                memBoundMs, avx2NetBound, bound);
    std::printf("   SPEC budget 35 ms -> the bound is %.2fx the budget\n", bound / 35.0);
    std::printf("   headroom above the bound: 35 - %.2f = %.2f ms\n\n", bound, 35.0 - bound);
    std::fflush(stdout);
}

/** Card item 4: the same bandwidth kernel, measured in a different context. */
void boundsContextCheck() {
    constexpr size_t NPX = static_cast<size_t>(3072) * 3072;
    std::vector<float> frame(NPX);
    std::mt19937 rng(1u);
    std::normal_distribution<float> noise(kMean, kSigma);
    for (size_t i = 0; i < NPX; ++i) frame[i] = noise(rng);
    std::vector<uint8_t> map(NPX, 0u);
    volatile uint8_t sinkb = 0u;

    // Heavy unrelated work first, so the cache and the clock are in the state a
    // long program run leaves them in -- QA-A-55 found the same kernel differs
    // by ~17% between a tight loop and a long run.
    XpeImageBuffer img{};
    img.data = frame.data();
    img.width = 3072; img.height = 3072;
    img.bitsAllocated = 32; img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.dataSize = static_cast<uint32_t>(NPX * sizeof(float));
    volatile float sink = 0.0f;
    sink = sink + ComputeGlobalSigma(&img);

    auto detectTrafficKernel = [&]{
        for (size_t i = 0; i < NPX; ++i) map[i] = (frame[i] > 3000.0f) ? 1u : 0u;
        sinkb = static_cast<uint8_t>(sinkb + map[0]);
    };
    const double bytes = static_cast<double>(NPX) * (sizeof(float) + sizeof(uint8_t));
    std::printf("D. CONTEXT CHECK -- same traffic kernel after heavy work\n");
    printSpread("detector traffic shape (late)", allMs(7, detectTrafficKernel), bytes);
    std::printf("\n");
    std::fflush(stdout);
}

/* -------------------------------------------------- QA-A-55 global sigma */

/**
 * QA-A-55 (#144): dispersion first, then the breakdown.
 *
 * QA-A-54 left an unverified number behind -- the same ComputeGlobalSigma on the
 * same frame measured 306.2 ms before its change and 369.8 ms after, with not a
 * line of that function touched. Until the spread of the measurement is known,
 * no improvement smaller than the spread means anything. So this runs the
 * function many times and prints min / median / max before any decomposition.
 */
void sigmaDispersion(uint32_t w, uint32_t h, int reps) {
    const size_t n = static_cast<size_t>(w) * h;
    std::mt19937 rng(0u);
    std::normal_distribution<float> noise(kMean, kSigma);
    std::vector<float> frame(n);
    for (size_t i = 0; i < n; ++i) frame[i] = noise(rng);

    XpeImageBuffer img{};
    img.data = frame.data();
    img.width = w; img.height = h;
    img.bitsAllocated = 32; img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.dataSize = static_cast<uint32_t>(n * sizeof(float));

    std::vector<double> t;
    volatile float sink = 0.0f;
    for (int r = 0; r < reps; ++r) {
        const auto t0 = std::chrono::steady_clock::now();
        sink = sink + ComputeGlobalSigma(&img);
        const auto t1 = std::chrono::steady_clock::now();
        t.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    std::vector<double> sorted = t;
    std::sort(sorted.begin(), sorted.end());
    const double lo = sorted.front();
    const double med = sorted[sorted.size() / 2u];
    const double hi = sorted.back();

    std::printf("[sigma-dispersion] %ux%u, %d runs of ComputeGlobalSigma\n", w, h, reps);
    std::printf("  raw:");
    for (double v : t) std::printf(" %.1f", v);
    std::printf("\n");
    std::printf("  min %.1f  median %.1f  max %.1f  spread %.1f ms = %.1f%% of median\n\n",
                lo, med, hi, hi - lo, 100.0 * (hi - lo) / med);
    std::fflush(stdout);
}

/**
 * Inside ComputeGlobalSigma. Same discipline as QA-A-54: the only DIRECT number
 * is the function's own time; every stage below it comes from a replica built
 * here, so the replica/direct ratio is printed on the same line and the stage
 * shares are quoted against the replica, never against the direct total.
 */
void sigmaBreakdown(uint32_t w, uint32_t h) {
    const size_t n = static_cast<size_t>(w) * h;
    std::mt19937 rng(0u);
    std::normal_distribution<float> noise(kMean, kSigma);
    std::vector<float> frame(n);
    for (size_t i = 0; i < n; ++i) frame[i] = noise(rng);

    XpeImageBuffer img{};
    img.data = frame.data();
    img.width = w; img.height = h;
    img.bitsAllocated = 32; img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.dataSize = static_cast<uint32_t>(n * sizeof(float));

    volatile float sink = 0.0f;
    auto bestOf = [&](int reps, auto fn) {
        double best = 1e30;
        for (int r = 0; r < reps; ++r) {
            const auto t0 = std::chrono::steady_clock::now();
            fn();
            const auto t1 = std::chrono::steady_clock::now();
            const double v = std::chrono::duration<double, std::milli>(t1 - t0).count();
            if (v < best) best = v;
        }
        return best;
    };

    const double tDirect = bestOf(5, [&]{ sink = sink + ComputeGlobalSigma(&img); });

    const float* pixels = frame.data();
    std::vector<float> diff;
    diff.reserve(n);

    // Stage 1: build the horizontal difference array, exactly as shipped.
    const double tBuildH = bestOf(5, [&]{
        diff.clear();
        for (size_t y = 0; y < h; ++y) {
            const float* row = pixels + y * w;
            for (size_t x = 0; x + 1u < w; ++x) diff.push_back(row[x + 1u] - row[x]);
        }
    });

    // Keep one built copy to feed the later stages without rebuilding.
    diff.clear();
    for (size_t y = 0; y < h; ++y) {
        const float* row = pixels + y * w;
        for (size_t x = 0; x + 1u < w; ++x) diff.push_back(row[x + 1u] - row[x]);
    }
    const std::vector<float> built = diff;
    const size_t mid = built.size() / 2u;

    // Stage 2: the first selection.
    std::vector<float> work;
    const double tSelect1 = bestOf(5, [&]{
        work = built;
        std::nth_element(work.begin(), work.begin() + static_cast<std::ptrdiff_t>(mid), work.end());
        sink = sink + work[mid];
    });
    work = built;
    std::nth_element(work.begin(), work.begin() + static_cast<std::ptrdiff_t>(mid), work.end());
    const float median = work[mid];
    const std::vector<float> selected = work;

    // Stage 3: the absolute-deviation transform.
    const double tAbs = bestOf(5, [&]{
        work = selected;
        for (size_t i = 0; i < work.size(); ++i) work[i] = std::abs(work[i] - median);
        sink = sink + work[0];
    });
    work = selected;
    for (size_t i = 0; i < work.size(); ++i) work[i] = std::abs(work[i] - median);
    const std::vector<float> deviations = work;

    // Stage 4: the second selection.
    const double tSelect2 = bestOf(5, [&]{
        work = deviations;
        std::nth_element(work.begin(), work.begin() + static_cast<std::ptrdiff_t>(mid), work.end());
        sink = sink + work[mid];
    });

    // The copy each stage above pays so it can be re-run; subtracted out below.
    const double tCopy = bestOf(5, [&]{ work = built; sink = sink + work[0]; });

    const double oneDir = (tBuildH - 0.0) + (tSelect1 - tCopy) + (tAbs - tCopy) + (tSelect2 - tCopy);
    std::printf("[sigma-breakdown] %ux%u  (%zu diffs per direction)\n", w, h, built.size());
    std::printf("  %-44s %9.1f ms  [DIRECT]\n", "ComputeGlobalSigma (whole, best of 5)", tDirect);
    std::printf("  replica of ONE direction  %9.1f ms; x2 directions = %.1f ms"
                "  = %.2fx the direct call\n", oneDir, 2.0 * oneDir, 2.0 * oneDir / tDirect);
    std::printf("  (stage shares are of the one-direction replica)\n");
    std::printf("    %-42s %9.1f ms   %5.1f%%\n", "1 build difference array (push_back)",
                tBuildH, 100.0 * tBuildH / oneDir);
    std::printf("    %-42s %9.1f ms   %5.1f%%\n", "2 first selection (nth_element)",
                tSelect1 - tCopy, 100.0 * (tSelect1 - tCopy) / oneDir);
    std::printf("    %-42s %9.1f ms   %5.1f%%\n", "3 absolute-deviation transform",
                tAbs - tCopy, 100.0 * (tAbs - tCopy) / oneDir);
    std::printf("    %-42s %9.1f ms   %5.1f%%\n", "4 second selection (nth_element)",
                tSelect2 - tCopy, 100.0 * (tSelect2 - tCopy) / oneDir);
    std::printf("    (each stage above had a %.1f ms vector copy subtracted)\n\n", tCopy);
    std::fflush(stdout);
}

/* -------------------------------------------------- QA-A-54 decomposition */

/**
 * QA-A-54 (#144): where the time actually goes at the frame size the SPEC names.
 *
 * Everything before this card measured 1024x1024 and extrapolated to 3072x3072
 * by pixel count. That extrapolation assumes the cost per pixel is size
 * independent, which is exactly what a 9x larger working set is likely to
 * break -- so this measures 3072x3072 directly.
 *
 * The per-pixel stages are measured incrementally (gather, then gather+median,
 * then gather+median+copy+MAD), so each stage's own cost is a difference of two
 * measurements rather than a guess. The reconciliation at the end is the part
 * that matters: sigma + loop + memset must add up to the entry point's own time,
 * and whatever is left over is work nobody measured.
 */
void decompose(uint32_t w, uint32_t h) {
    const size_t n = static_cast<size_t>(w) * h;
    std::mt19937 rng(0u);
    std::normal_distribution<float> noise(kMean, kSigma);
    std::vector<float> frame(n);
    for (size_t i = 0; i < n; ++i) frame[i] = noise(rng);

    XpeImageBuffer img{};
    img.data = frame.data();
    img.width = w; img.height = h;
    img.bitsAllocated = 32; img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.dataSize = static_cast<uint32_t>(n * sizeof(float));

    std::vector<uint8_t> map(n, 0);
    XpeImageBuffer out{};
    out.data = map.data();
    out.width = w; out.height = h;
    out.bitsAllocated = 8; out.bitsStored = 8;
    out.format = XPE_PIXEL_UINT8;
    out.dataSize = static_cast<uint32_t>(n);

    auto ms = [](std::chrono::steady_clock::time_point a,
                 std::chrono::steady_clock::time_point b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };
    auto bestOf = [&](int reps, auto fn) {
        double best = 1e30;
        for (int r = 0; r < reps; ++r) {
            const auto t0 = std::chrono::steady_clock::now();
            fn();
            const auto t1 = std::chrono::steady_clock::now();
            const double v = ms(t0, t1);
            if (v < best) best = v;
        }
        return best;
    };

    std::printf("[decompose] %ux%u  (%zu pixels), best of 3 per row\n", w, h, n);

    // -- whole entry point
    XpeImageMetadata meta{};
    volatile XpeErrorCode rc = XPE_OK;
    const double tTotal = bestOf(3, [&]{
        rc = xpe_defect_detect_runtime(&img, &meta, &out);
    });
    if (rc != XPE_OK) { std::fprintf(stderr, "detect failed\n"); return; }

    // -- global sigma, the two estimators
    volatile float sink = 0.0f;
    const double tSigmaBm = bestOf(3, [&]{ sink = sink + ComputeGlobalSigma(&img); });

    // -- memset of the output map, which the entry point does once
    const double tMemset = bestOf(3, [&]{ std::memset(map.data(), 0, n); });

    // -- the per-pixel loop, in incremental stages
    RuntimeDetectionConfig cfg = RuntimeDetection_DefaultConfig();
    const float sg = ComputeGlobalSigma(&img);
    cfg.globalSigmaFloor = RUNTIME_DETECTION_GLOBAL_SIGMA_FLOOR * sg;
    cfg.globalSigmaCap = RUNTIME_DETECTION_GLOBAL_SIGMA_CAP * sg;

    std::vector<float> a, b;
    a.reserve(64); b.reserve(64);

    const double tGather = bestOf(3, [&]{
        for (uint32_t y = 0; y < h; ++y)
            for (uint32_t x = 0; x < w; ++x) {
                CollectNeighborValues(&img, x, y, cfg.windowSize, a);
                sink = sink + a[0];
            }
    });
    const double tGatherMed = bestOf(3, [&]{
        for (uint32_t y = 0; y < h; ++y)
            for (uint32_t x = 0; x < w; ++x) {
                CollectNeighborValues(&img, x, y, cfg.windowSize, a);
                sink = sink + ComputeMedian(a);
            }
    });
    const double tGatherMedMad = bestOf(3, [&]{
        for (uint32_t y = 0; y < h; ++y)
            for (uint32_t x = 0; x < w; ++x) {
                CollectNeighborValues(&img, x, y, cfg.windowSize, a);
                const float m = ComputeMedian(a);
                b.assign(a.begin(), a.end());
                sink = sink + ComputeMAD(b, m);
            }
    });
    const double tLoop = bestOf(3, [&]{
        for (uint32_t y = 0; y < h; ++y)
            for (uint32_t x = 0; x < w; ++x) {
                if (DetectDefectivePixel(&img, x, y, cfg, a, b)) {
                    map[static_cast<size_t>(y) * w + x] = 1;
                }
            }
    });

    // The three DIRECT measurements. Only these are attributable to the shipped
    // entry point; everything below them is a replica built in this TU.
    const double tLoopInSitu = tTotal - tSigmaBm - tMemset;
    std::printf("  %-42s %9.1f ms   %5.1f%%\n", "TOTAL xpe_defect_detect_runtime  [measured]",
                tTotal, 100.0);
    std::printf("  %-42s %9.1f ms   %5.1f%%\n", "  global sigma (Bm, shipped)     [measured]",
                tSigmaBm, 100.0 * tSigmaBm / tTotal);
    std::printf("  %-42s %9.1f ms   %5.1f%%\n", "  memset of the output map       [measured]",
                tMemset, 100.0 * tMemset / tTotal);
    std::printf("  %-42s %9.1f ms   %5.1f%%\n", "  per-pixel loop      [total - the two above]",
                tLoopInSitu, 100.0 * tLoopInSitu / tTotal);
    std::printf("  -> the three rows above sum to the total by construction;"
                " nothing is unaccounted.\n\n");

    // The REPLICA. Same primitives, but compiled here rather than inside the
    // DLL, so its absolute time is NOT the shipped loop's time -- the ratio
    // below says by how much. It is used only to split the loop into stages,
    // and the stage shares are therefore quoted against the replica's own total.
    std::printf("  replica loop, this TU  %9.1f ms  = %.2fx the in-situ loop\n",
                tLoop, tLoop / tLoopInSitu);
    std::printf("  (stage shares below are of the REPLICA, not of the total)\n");
    std::printf("    %-38s %9.1f ms   %5.1f%%\n", "gather only",
                tGather, 100.0 * tGather / tLoop);
    std::printf("    %-38s %9.1f ms   %5.1f%%\n", "median  (delta)",
                tGatherMed - tGather, 100.0 * (tGatherMed - tGather) / tLoop);
    std::printf("    %-38s %9.1f ms   %5.1f%%\n", "copy + MAD  (delta)",
                tGatherMedMad - tGatherMed, 100.0 * (tGatherMedMad - tGatherMed) / tLoop);
    std::printf("    %-38s %9.1f ms   %5.1f%%\n", "threshold + map write  (delta)",
                tLoop - tGatherMedMad, 100.0 * (tLoop - tGatherMedMad) / tLoop);
    const double tMedians = (tGatherMed - tGather) + (tGatherMedMad - tGatherMed);
    std::printf("    %-38s %9.1f ms   %5.1f%%\n", "  of which: the two selections",
                tMedians, 100.0 * tMedians / tLoop);

    std::printf("\n  SPEC budget 35 ms -> over by %.1fx\n\n", tTotal / 35.0);
    std::fflush(stdout);
}

/* ------------------------------------------------ QA-A-45 false negatives */

/**
 * Reproduces the QA-A-40/43/44 TPR@10-sigma case exactly and dumps every
 * injected site the shipped detector did NOT flag, with the numbers that
 * decided it.
 *
 * The conditions are copied from test_runtime_detection_rates.cpp so the FN set
 * is the same 13 pixels the rates suite reports -- 1024x1024, mean 3000 ADU,
 * sigma 10, the 32-pixel lattice, amplitude +10 sigma, seed 20260911.
 *
 * Nothing here changes the detector. It re-derives the same decision from the
 * same header primitives so the left and right sides of the comparison can be
 * printed.
 */
void reportFalseNegatives(uint32_t seed, float amplitudeSigma) {
    const std::vector<size_t> sites = defectSites();
    std::vector<float> frame = cleanFrame(seed);
    for (size_t site : sites) frame[site] += amplitudeSigma * kSigma;

    XpeImageBuffer img = wrap(frame);
    std::vector<uint8_t> map(kN, 0);
    XpeImageBuffer out{};
    out.data = map.data();
    out.width = kW; out.height = kH;
    out.bitsAllocated = 8; out.bitsStored = 8;
    out.format = XPE_PIXEL_UINT8;
    out.dataSize = static_cast<uint32_t>(kN);

    XpeImageMetadata meta{};
    if (xpe_defect_detect_runtime(&img, &meta, &out) != XPE_OK) {
        std::fprintf(stderr, "detect failed\n");
        return;
    }

    // The same floor the entry point computed, re-derived for reporting.
    const float sigmaGlobalRaw = xpe::preprocess::internal::ComputeGlobalSigma(&img);
    const float floor = RUNTIME_DETECTION_GLOBAL_SIGMA_FLOOR * sigmaGlobalRaw;
    const float kappa = RUNTIME_DETECTION_DEFAULT_SIGMA_THRESHOLD;
    const int32_t window = RUNTIME_DETECTION_DEFAULT_WINDOW_SIZE;

    std::printf("[fn] seed=%u amplitude=%.1f sigma  frame=%ux%u  sites=%zu\n",
                seed, amplitudeSigma, kW, kH, sites.size());
    std::printf("[fn] global sigma (MAD*1.4826) = %.6f, floor = %.6f * it = %.6f\n",
                sigmaGlobalRaw, RUNTIME_DETECTION_GLOBAL_SIGMA_FLOOR, floor);
    std::printf("[fn] kappa = %.1f, window = %dx%d excluding centre\n\n",
                kappa, window, window);
    std::printf("  idx      x     y   nbrs   value    median    |c-m|   "
                "mad*1.4826  sigma_use  kappa*sigma   margin  verdict\n");

    const float* pixels = static_cast<const float*>(img.data);
    std::vector<float> nbrs, dev;
    size_t missed = 0;

    for (size_t site : sites) {
        if (map[site]) continue;
        ++missed;

        const uint32_t x = static_cast<uint32_t>(site % kW);
        const uint32_t y = static_cast<uint32_t>(site / kW);

        xpe::preprocess::internal::CollectNeighborValues(&img, x, y, window, nbrs);
        const size_t nbrCount = nbrs.size();
        float median = 0.0f, mad = 0.0f;
        if (nbrCount >= RUNTIME_DETECTION_MIN_NEIGHBORS) {
            median = xpe::preprocess::internal::ComputeMedian(nbrs);
            dev = nbrs;
            mad = xpe::preprocess::internal::ComputeMAD(dev, median);
        }
        const float sigmaUse = (mad > floor) ? mad : floor;
        const float centre = pixels[site];
        const float lhs = std::fabs(centre - median);
        const float rhs = kappa * sigmaUse;

        const char* verdict = (nbrCount < RUNTIME_DETECTION_MIN_NEIGHBORS)
            ? "SKIPPED(min-nbrs)"
            : (lhs > rhs ? "flagged?!" : "below threshold");

        std::printf("  %3zu  %5u %5u   %4zu  %8.2f  %8.2f  %7.2f  %10.4f  %9.4f  "
                    "%11.4f  %7.2f  %s\n",
                    missed, x, y, nbrCount, centre, median, lhs, mad, sigmaUse,
                    rhs, lhs - rhs, verdict);

        // The eight neighbours, so the reader can check the median by hand.
        xpe::preprocess::internal::CollectNeighborValues(&img, x, y, window, nbrs);
        std::printf("        nbrs:");
        for (float v : nbrs) std::printf(" %.2f", v);
        std::printf("\n");
        std::fflush(stdout);
    }

    std::printf("\n[fn] missed %zu of %zu sites\n", missed, sites.size());
    std::fflush(stdout);
}

} // namespace

int main(int argc, char** argv) {
    bool quick = false, profileOnly = false, timeOnly = false, fnOnly = false;
    bool decomposeOnly = false;
    bool sigmaOnly = false;
    bool boundsOnly = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--quick") == 0) quick = true;
        if (std::strcmp(argv[i], "--profile") == 0) profileOnly = true;
        if (std::strcmp(argv[i], "--time") == 0) timeOnly = true;
        if (std::strcmp(argv[i], "--decompose") == 0) decomposeOnly = true;
        if (std::strcmp(argv[i], "--sigma") == 0) sigmaOnly = true;
        if (std::strcmp(argv[i], "--bounds") == 0) boundsOnly = true;
        if (std::strcmp(argv[i], "--fn10") == 0) fnOnly = true;
    }

    if (xpe_preprocess_init(nullptr) != XPE_OK) {
        std::fprintf(stderr, "xpe_preprocess_init failed\n");
        return 1;
    }

    if (fnOnly) {
        reportFalseNegatives(20260911u, 10.0f);
        xpe_preprocess_shutdown();
        return 0;
    }

    if (boundsOnly) {
        bounds();
        boundsContextCheck();
        xpe_preprocess_shutdown();
        return 0;
    }

    if (sigmaOnly) {
        sigmaDispersion(3072, 3072, 7);
        sigmaBreakdown(3072, 3072);
        xpe_preprocess_shutdown();
        return 0;
    }

    if (decomposeOnly) {
        decompose(1024, 1024);
        decompose(3072, 3072);
        xpe_preprocess_shutdown();
        return 0;
    }

    if (timeOnly) {
        timeEntryPoint(1024, 1024);
        timeEntryPoint(3072, 3072);
        xpe_preprocess_shutdown();
        return 0;
    }

    if (profileOnly) {
        profile(3);
        profile(7);
        xpe_preprocess_shutdown();
        return 0;
    }

    const std::vector<Variant> variants = {
        {"baseline 3x3 k=5",      Kind::Baseline,    3, 5.0f, 0.0f, 0, 0.0f},
        // Same rule as the baseline, but through this TU's buffer-reusing loop.
        // The gap between this row and the one above is allocation cost, not
        // detection behaviour -- without it the window rows would look faster
        // than they are for the wrong reason.
        {"    3x3 k=5 (reuse)",   Kind::Window,      3, 5.0f, 0.0f, 0, 0.0f},
        {"(a) 5x5 k=5",           Kind::Window,      5, 5.0f, 0.0f, 0, 0.0f},
        {"(a) 7x7 k=5",           Kind::Window,      7, 5.0f, 0.0f, 0, 0.0f},
        {"(b) 3x3 floor a=0.8",   Kind::GlobalFloor, 3, 5.0f, 0.8f, 0, 0.0f},
        {"(b) 3x3 floor a=1.0",   Kind::GlobalFloor, 3, 5.0f, 1.0f, 0, 0.0f},
        {"(c) 2-stage 3->7 k1=4", Kind::TwoStage,    3, 5.0f, 0.0f, 7, 4.0f},
        // Diagnostic, not one of the leader's three: the only lever that moves
        // TPR at exactly 5 sigma is kappa, and these rows show what it costs in
        // false positives.
        {"(d) 7x7 k=4 [diag]",    Kind::Window,      7, 4.0f, 0.0f, 0, 0.0f},
        {"(d) 3x3 k=4 [diag]",    Kind::Window,      3, 4.0f, 0.0f, 0, 0.0f},
    };

    const std::vector<uint32_t> seeds = quick ? std::vector<uint32_t>{0u}
                                              : std::vector<uint32_t>{0u, 1u, 2u};

    std::printf("variant                  seed   TPR@5s    TPR@10s   TPR@dead   "
                "FP        FPR         ms(tpr)  ms(clean)\n");
    std::printf("-------------------------------------------------------------"
                "-------------------------------------------\n");

    for (const Variant& v : variants) {
        double sumTpr5 = 0.0, sumFpr = 0.0, sumMs = 0.0;
        for (size_t i = 0; i < seeds.size(); ++i) {
            // The dead-pixel combination is run on seed 0 only -- one extra
            // full-frame pass per variant, which is what "1 조합" asks for.
            const Row r = measureOne(v, seeds[i], /*withDead=*/i == 0);
            std::printf("%-24s %4u  %8.6f  %8.6f  %8.6f  %8zu  %.3e  %7.1f  %7.1f\n",
                        v.label.c_str(), seeds[i], r.tpr5, r.tpr10,
                        (i == 0 ? r.tprDead : -1.0), r.fp, r.fpr,
                        r.msTpr, r.msClean);
            std::fflush(stdout);
            sumTpr5 += r.tpr5; sumFpr += r.fpr; sumMs += r.msTpr;
        }
        const double n = static_cast<double>(seeds.size());
        std::printf("%-24s  AVG  %8.6f  %8s  %8s  %.3e  %7.1f\n\n",
                    v.label.c_str(), sumTpr5 / n, "-", "-",
                    sumFpr / n, sumMs / n);
        std::fflush(stdout);
    }

    xpe_preprocess_shutdown();
    return 0;
}
