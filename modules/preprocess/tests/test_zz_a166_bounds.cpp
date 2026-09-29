/**
 * @file test_zz_a166_bounds.cpp
 * @brief QA-A-166 (#143) -- LOWER BOUND of the detector as it stands today.
 *
 * All DISABLED_ on purpose. Run with:
 *   xpe_preprocess_tests.exe --gtest_also_run_disabled_tests --gtest_filter=A166Bounds.*
 *
 * METHOD FOLLOWS QA-A-56 so the two are comparable:
 *   - a bound is what the arithmetic CANNOT avoid, so implementation overhead
 *     (allocation, per-call fixed cost) is excluded rather than measured;
 *   - the memory bound is minimum traffic divided by the machine's streaming
 *     bandwidth, measured with memcpy -- not by the rate a scalar loop happens
 *     to reach, which is dependency-bound and understates the machine;
 *   - the compute bound is the dominant kernel measured in isolation, times the
 *     number of times the detector must run it.
 *
 * WHAT QA-A-56 DID NOT HAVE: a per-tile sigma stage. It did not exist then, so
 * the 27.1 ms bound it produced -- and the 60 ms target derived from it -- do
 * not cover this algorithm. That gap is what this card measures.
 *
 * MACHINE IDENTITY. spec.md defines the development machine as "the machine
 * QA-A-56 measured on". This file re-runs QA-A-56's two reference kernels, so
 * the ratio between this machine and that one is a measurement rather than an
 * assumption -- QA-A-164/165 had to leave it open.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "runtime_detection.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <random>
#include <vector>

namespace {

using namespace xpe::preprocess::internal;

constexpr uint32_t kW = 3072, kH = 3072;
constexpr size_t   kN = static_cast<size_t>(kW) * kH;

double msOf(std::chrono::steady_clock::time_point a,
            std::chrono::steady_clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

/** Best of @p reps -- a lower bound wants the cleanest run, not the average. */
template <typename F>
double bestMs(int reps, F&& f) {
    double best = 1e18;
    for (int i = 0; i < reps; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        f();
        const auto t1 = std::chrono::steady_clock::now();
        best = std::min(best, msOf(t0, t1));
    }
    return best;
}

std::vector<float> noiseFrame() {
    std::mt19937 rng(20260911u);
    std::normal_distribution<float> g(0.0f, 1.0f);
    std::vector<float> f(kN);
    for (size_t i = 0; i < kN; ++i) f[i] = 3000.0f + 10.0f * g(rng);
    return f;
}

class A166Bounds : public ::testing::Test {};

} // namespace

// ---------------------------------------------------------------------------
// (1) MACHINE IDENTITY -- QA-A-56's two reference kernels, re-run here.
// ---------------------------------------------------------------------------
TEST_F(A166Bounds, DISABLED_MachineReferenceKernels) {
    // memcpy streaming bandwidth. QA-A-56: 1.79-2.04 ms -> 40.7 GB/s.
    std::vector<float> src(kN), dst(kN);
    std::memset(src.data(), 1, src.size() * sizeof(float));
    const double msCopy = bestMs(7, [&] { std::memcpy(dst.data(), src.data(), kN * sizeof(float)); });
    const double gbs = (2.0 * kN * sizeof(float)) / (msCopy * 1e-3) / 1e9;
    std::printf("[a166] memcpy  %.2f ms -> %.1f GB/s     (QA-A-56: 1.79-2.04 ms, 40.7 GB/s)\n",
                msCopy, gbs);

#if XPE_DETECT_HAS_AVX2
    // The AVX2 median network, timed in isolation. QA-A-56: 0.85 ns per network.
    // This is the network the detector actually runs today (MedianOfEight8);
    // QA-A-56 named a 19-CE 9-element network, so the two are comparable in
    // role, not necessarily identical in gate count -- stated, not assumed.
    constexpr size_t kNets = 1000000u;
    __m256 v[8];
    for (int k = 0; k < 8; ++k) v[k] = _mm256_set1_ps(static_cast<float>(k) * 1.37f);
    volatile float sink = 0.0f;
    const double msNet = bestMs(5, [&] {
        __m256 acc = _mm256_setzero_ps();
        for (size_t i = 0; i < kNets; ++i) {
            v[0] = _mm256_add_ps(v[0], _mm256_set1_ps(1e-7f));  // defeat hoisting
            acc = _mm256_add_ps(acc, MedianOfEight8(v));
        }
        alignas(32) float out[8];
        _mm256_store_ps(out, acc);
        sink = out[0];
    });
    (void)sink;
    const double nsPerNet = msNet * 1e6 / static_cast<double>(kNets * 8u);
    std::printf("[a166] AVX2 median network  %.2f ms / %zu x8 = %.2f ns per network"
                "   (QA-A-56: 0.85 ns)\n", msNet, kNets, nsPerNet);
#else
    std::printf("[a166] AVX2 unavailable in this build\n");
#endif
}

// ---------------------------------------------------------------------------
// (2) DETECTION NETWORK -- the 15.97 ms line of QA-A-56, for today's kernel.
// ---------------------------------------------------------------------------
TEST_F(A166Bounds, DISABLED_DetectionNetworkBound) {
#if XPE_DETECT_HAS_AVX2
    constexpr size_t kNets = 1000000u;
    __m256 v[8];
    for (int k = 0; k < 8; ++k) v[k] = _mm256_set1_ps(static_cast<float>(k) * 1.37f);
    volatile float sink = 0.0f;
    const double msNet = bestMs(5, [&] {
        __m256 acc = _mm256_setzero_ps();
        for (size_t i = 0; i < kNets; ++i) {
            v[0] = _mm256_add_ps(v[0], _mm256_set1_ps(1e-7f));
            acc = _mm256_add_ps(acc, MedianOfEight8(v));
        }
        alignas(32) float out[8];
        _mm256_store_ps(out, acc);
        sink = out[0];
    });
    (void)sink;
    const double nsPerNet = msNet * 1e6 / static_cast<double>(kNets * 8u);
    // The detector runs TWO networks per pixel: the neighbour median, then the
    // median of the absolute deviations.
    const double msDetect = 2.0 * static_cast<double>(kN) * nsPerNet / 1e6;
    std::printf("[a166] detection networks : 2 x %zu px x %.2f ns = %.2f ms"
                "   (QA-A-56 equivalent: 15.97 ms)\n", kN, nsPerNet, msDetect);
#else
    std::printf("[a166] AVX2 unavailable\n");
#endif
}

// ---------------------------------------------------------------------------
// (3) SIGMA STAGES -- traffic and selection, separated.
// ---------------------------------------------------------------------------
TEST_F(A166Bounds, DISABLED_SigmaStageBounds) {
    std::vector<float> frame = noiseFrame();
    XpeImageBuffer img{};
    img.data = frame.data(); img.width = kW; img.height = kH;
    img.bitsAllocated = 32; img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.dataSize = static_cast<uint32_t>(kN * sizeof(float));

    // --- bandwidth reference for the traffic arithmetic
    std::vector<float> src(kN), dst(kN);
    std::memset(src.data(), 1, src.size() * sizeof(float));
    const double msCopy = bestMs(7, [&] { std::memcpy(dst.data(), src.data(), kN * sizeof(float)); });
    const double gbs = (2.0 * kN * sizeof(float)) / (msCopy * 1e-3) / 1e9;

    // --- global sigma, as a control (not used by the shipped rule any more)
    volatile float gsSink = 0.0f;
    const double msGlobal = bestMs(3, [&] { gsSink = ComputeGlobalSigma(&img); });
    // QA-A-56 costed it by traffic: write H diffs (37.7 MB), read them back for
    // two selections, same for V. Its figure was 453 MB / 40.7 GB/s = 11.1 ms.
    const double globalTrafficMB = 453.0;
    std::printf("[a166] global sigma  measured %.1f ms | traffic bound %.1f MB / %.1f GB/s = %.2f ms"
                "   (QA-A-56: 11.1 ms)\n",
                msGlobal, globalTrafficMB, gbs, globalTrafficMB / (gbs * 1000.0) * 1000.0);
    (void)gsSink;

    // --- tile sigma, split: difference production vs selection
    constexpr uint32_t T = RUNTIME_DETECTION_TILE_SIZE;
    const uint32_t tilesX = (kW + T - 1u) / T, tilesY = (kH + T - 1u) / T;
    const size_t maxPerTile = 2u * static_cast<size_t>(T) * (T - 1u);
    const size_t tiles = static_cast<size_t>(tilesX) * tilesY;

    std::unique_ptr<float[]> buf(new float[maxPerTile]);
    volatile float sink = 0.0f;
    const double msDiffs = bestMs(3, [&] {
        for (uint32_t ty = 0; ty < tilesY; ++ty)
            for (uint32_t tx = 0; tx < tilesX; ++tx) {
                const uint32_t x0 = tx * T, x1 = std::min(x0 + T, kW);
                const uint32_t y0 = ty * T, y1 = std::min(y0 + T, kH);
                float* out = buf.get(); size_t n = 0;
                for (uint32_t y = y0; y < y1; ++y) {
                    const float* row = frame.data() + static_cast<size_t>(y) * kW;
                    for (uint32_t x = x0 + 1u; x < x1; ++x) out[n++] = row[x] - row[x - 1u];
                }
                for (uint32_t y = y0 + 1u; y < y1; ++y) {
                    const float* row = frame.data() + static_cast<size_t>(y) * kW;
                    const float* up = row - kW;
                    for (uint32_t x = x0; x < x1; ++x) out[n++] = row[x] - up[x];
                }
                if (n) sink = out[n - 1u];
            }
    });
    (void)sink;
    std::printf("[a166] tile sigma  differences  %.2f ms  (%zu tiles x %zu elems)\n",
                msDiffs, tiles, maxPerTile);

    // --- selection: three ways, to separate the BOUND from this implementation
    std::mt19937 rng(7u);
    std::normal_distribution<float> g(0.0f, 1.0f);
    std::vector<float> chunk(maxPerTile);
    std::vector<float> scratch(maxPerTile);

    auto perTileSelection = [&](const char* name, auto&& selector) {
        for (size_t i = 0; i < maxPerTile; ++i) chunk[i] = g(rng);
        // two selections per tile: median, then median of |x - median|
        const double ms = bestMs(3, [&] {
            for (size_t t = 0; t < tiles; ++t) {
                std::copy(chunk.begin(), chunk.end(), scratch.begin());
                const float m = selector(scratch.data(), maxPerTile, maxPerTile / 2u);
                for (size_t i = 0; i < maxPerTile; ++i) scratch[i] = std::abs(scratch[i] - m);
                sink = selector(scratch.data(), maxPerTile, maxPerTile / 2u);
            }
        });
        std::printf("[a166] tile sigma  selection [%-22s] %7.2f ms\n", name, ms);
        return ms;
    };

    const double msShipped = perTileSelection("SelectKthSmallest", [](float* d, size_t n, size_t k) {
        return SelectKthSmallest(d, n, k);
    });
    const double msNth = perTileSelection("std::nth_element", [](float* d, size_t n, size_t k) {
        std::nth_element(d, d + k, d + n);
        return d[k];
    });
    std::printf("[a166]   -> shipped is %.2fx the nth_element figure\n", msShipped / msNth);

    // The copy inside the loop is part of neither bound -- it exists so each
    // repetition starts from the same data. Measured separately so it can be
    // subtracted rather than guessed at.
    const double msCopyOnly = bestMs(3, [&] {
        for (size_t t = 0; t < tiles; ++t) std::copy(chunk.begin(), chunk.end(), scratch.begin());
    });
    std::printf("[a166]   (the per-tile copy inside those loops: %.2f ms -- subtract it)\n", msCopyOnly);
}

// ---------------------------------------------------------------------------
// (4) MEMORY -- minimum traffic for the whole detector as it stands.
// ---------------------------------------------------------------------------
TEST_F(A166Bounds, DISABLED_MemoryBound) {
    std::vector<float> src(kN), dst(kN);
    std::memset(src.data(), 1, src.size() * sizeof(float));
    const double msCopy = bestMs(7, [&] { std::memcpy(dst.data(), src.data(), kN * sizeof(float)); });
    const double gbs = (2.0 * kN * sizeof(float)) / (msCopy * 1e-3) / 1e9;

    // Compulsory traffic, counting each pass the algorithm cannot avoid:
    //   detection    read frame 37.75 MB + write map 9.44 MB
    //   tile sigma   read frame 37.75 MB again (a separate pass over the frame)
    // The tile difference buffer is 128 KB and stays in cache, so it is not
    // counted as DRAM traffic -- stated so the number can be disputed.
    const double mbFrame = static_cast<double>(kN) * 4.0 / 1e6;
    const double mbMap   = static_cast<double>(kN) * 1.0 / 1e6;
    const double mbTotal = mbFrame + mbMap + mbFrame;
    std::printf("[a166] memory  %.2f MB (frame %.2f + map %.2f + frame again %.2f)"
                " / %.1f GB/s = %.2f ms   (QA-A-56: 47.19 MB, 1.16 ms)\n",
                mbTotal, mbFrame, mbMap, mbFrame, gbs, mbTotal / (gbs * 1000.0) * 1000.0);
}

// ---------------------------------------------------------------------------
// (5) THE SELECTION BOUND -- what is left when the per-call fixed cost is gone.
//
// QA-A-165 estimated "41 ms if the fixed cost were removed" by scaling the
// per-element rate of a single large call. That was a CALCULATION. This is the
// measurement: the same two-pass radix selection with the histogram allocated
// once and cleared only where it was touched.
//
// PROBE ONLY. Nothing here goes into the shipped path -- the card forbids it,
// and the point is to learn the bound, not to take it.
// ---------------------------------------------------------------------------
namespace {

/** SelectKthSmallest with the two fixed costs removed, for measurement only. */
struct ReusableSelector {
    std::vector<uint32_t> hist;
    std::vector<uint32_t> touched;          // buckets to clear, instead of all 65536
    ReusableSelector() : hist(1u << 16, 0u) { touched.reserve(1u << 12); }

    float select(const float* values, size_t n, size_t k) {
        if (values == nullptr || n == 0u) return 0.0f;
        if (k >= n) k = n - 1u;

        touched.clear();
        for (size_t i = 0; i < n; ++i) {
            const uint32_t b = FloatSortKey(values[i]) >> 16;
            if (hist[b]++ == 0u) touched.push_back(b);
        }
        size_t seen = 0; uint32_t high = 0u;
        // Walk only the buckets that exist, in order.
        std::sort(touched.begin(), touched.end());
        for (uint32_t b : touched) {
            if (seen + hist[b] > k) { high = b; break; }
            seen += hist[b];
        }
        for (uint32_t b : touched) hist[b] = 0u;

        touched.clear();
        for (size_t i = 0; i < n; ++i) {
            const uint32_t key = FloatSortKey(values[i]);
            if ((key >> 16) == high) {
                const uint32_t b = key & 0xFFFFu;
                if (hist[b]++ == 0u) touched.push_back(b);
            }
        }
        const size_t rank = k - seen;
        size_t inner = 0; uint32_t low = 0u;
        std::sort(touched.begin(), touched.end());
        for (uint32_t b : touched) {
            if (inner + hist[b] > rank) { low = b; break; }
            inner += hist[b];
        }
        for (uint32_t b : touched) hist[b] = 0u;

        return SortKeyToFloat((static_cast<uint32_t>(high) << 16) | low);
    }
};

} // namespace

TEST_F(A166Bounds, DISABLED_SelectionLowerBound) {
    constexpr uint32_t T = RUNTIME_DETECTION_TILE_SIZE;
    const uint32_t tilesX = (kW + T - 1u) / T, tilesY = (kH + T - 1u) / T;
    const size_t tiles = static_cast<size_t>(tilesX) * tilesY;
    const size_t perTile = 2u * static_cast<size_t>(T) * (T - 1u);

    std::mt19937 rng(7u);
    std::normal_distribution<float> g(0.0f, 1.0f);
    std::vector<float> chunk(perTile), scratch(perTile);
    for (size_t i = 0; i < perTile; ++i) chunk[i] = g(rng);

    volatile float sink = 0.0f;
    const double msCopyOnly = bestMs(3, [&] {
        for (size_t t = 0; t < tiles; ++t) std::copy(chunk.begin(), chunk.end(), scratch.begin());
    });

    ReusableSelector sel;
    const double msReuse = bestMs(3, [&] {
        for (size_t t = 0; t < tiles; ++t) {
            std::copy(chunk.begin(), chunk.end(), scratch.begin());
            const float m = sel.select(scratch.data(), perTile, perTile / 2u);
            for (size_t i = 0; i < perTile; ++i) scratch[i] = std::abs(scratch[i] - m);
            sink = sel.select(scratch.data(), perTile, perTile / 2u);
        }
    });

    const double msShipped = bestMs(3, [&] {
        for (size_t t = 0; t < tiles; ++t) {
            std::copy(chunk.begin(), chunk.end(), scratch.begin());
            const float m = SelectKthSmallest(scratch.data(), perTile, perTile / 2u);
            for (size_t i = 0; i < perTile; ++i) scratch[i] = std::abs(scratch[i] - m);
            sink = SelectKthSmallest(scratch.data(), perTile, perTile / 2u);
        }
    });
    (void)sink;

    // The |x - m| pass is arithmetic the algorithm cannot avoid, so it belongs
    // in the bound; the copy does not.
    std::printf("\n[a166] selection over %zu tiles x %zu elements, two selections each\n", tiles, perTile);
    std::printf("[a166]   shipped SelectKthSmallest   %7.2f ms\n", msShipped - msCopyOnly);
    std::printf("[a166]   histogram reused (bound)    %7.2f ms\n", msReuse - msCopyOnly);
    std::printf("[a166]   fixed cost removed          %7.2f ms  (%.0f%%)\n",
                msShipped - msReuse,
                100.0 * (msShipped - msReuse) / (msShipped - msCopyOnly));
    std::printf("[a166]   (per-tile copy subtracted from both: %.2f ms)\n", msCopyOnly);
}
