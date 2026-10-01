/**
 * @file test_zz_a174_floor.cpp
 * @brief QA-A-174 (#230) -- shipped path and lower-bound terms, SAME session.
 *
 * All DISABLED_ on purpose. Run with:
 *   xpe_preprocess_tests.exe --gtest_also_run_disabled_tests --gtest_filter=A174Floor.*
 *
 * WHY. The 76 ms lower bound of QA-A-166/167 was measured in another session than
 * the shipped path, and absolute ms do not travel between sessions (this machine
 * is bimodal). Its selection term (63 ms) is also an in-situ measurement of the
 * shipped code (stage 68 - differences 5), so "shipped / bound" pins that term at
 * 1.0 by construction. This probe times, in ONE process and ONE round, the shipped
 * entry point, its stages, and every bound term, alternating order between
 * rounds, and reports each term with its provenance.
 *
 * PRODUCT CODE IS NOT TOUCHED. Everything below calls inline functions of
 * runtime_detection.h or the public entry point.
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
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace {

using namespace xpe::preprocess::internal;

constexpr uint32_t kW = 3072, kH = 3072;
constexpr size_t   kN = static_cast<size_t>(kW) * kH;

double msOf(std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

template <typename F>
double timeMs(F&& f) {
    const auto t0 = std::chrono::steady_clock::now();
    f();
    const auto t1 = std::chrono::steady_clock::now();
    return msOf(t0, t1);
}

std::vector<float> noiseFrame() {
    std::mt19937 rng(20260911u);
    std::normal_distribution<float> g(0.0f, 1.0f);
    std::vector<float> f(kN);
    for (size_t i = 0; i < kN; ++i) f[i] = 3000.0f + 10.0f * g(rng);
    return f;
}

/** SelectKthSmallest with the fixed costs removed (QA-A-166 probe copy). */
struct ReusableSelector {
    std::vector<uint32_t> hist;
    std::vector<uint32_t> touched;
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

double minOf(const std::vector<double>& v) { return *std::min_element(v.begin(), v.end()); }
double maxOf(const std::vector<double>& v) { return *std::max_element(v.begin(), v.end()); }
double medOf(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2u];
}

class A174Floor : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr)); }
    void TearDown() override { xpe_preprocess_shutdown(); }
};

} // namespace

TEST_F(A174Floor, DISABLED_SameSessionFloor) {
    constexpr int kRounds = 20;
    constexpr uint32_t T = RUNTIME_DETECTION_TILE_SIZE;
    const uint32_t tilesX = (kW + T - 1u) / T, tilesY = (kH + T - 1u) / T;
    const size_t tiles = static_cast<size_t>(tilesX) * tilesY;
    const size_t perTile = 2u * static_cast<size_t>(T) * (T - 1u);

    std::vector<float> frame = noiseFrame();
    std::vector<uint8_t> map(kN, 0u);
    XpeImageBuffer img{};
    img.data = frame.data(); img.width = kW; img.height = kH;
    img.bitsAllocated = 32; img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.dataSize = static_cast<uint32_t>(kN * sizeof(float));
    XpeImageBuffer out{};
    out.data = map.data(); out.width = kW; out.height = kH;
    out.bitsAllocated = 8; out.bitsStored = 8;
    out.format = XPE_PIXEL_UINT8;
    out.dataSize = static_cast<uint32_t>(kN);
    XpeImageMetadata meta{};

    // --- inputs for the isolated selection loops: the REAL differences of one
    //     interior tile (tile 1,1), replicated across all tiles (QA-A-167 method).
    std::vector<float> chunk(perTile), scratch(perTile);
    {
        const uint32_t x0 = T, x1 = 2u * T, y0 = T, y1 = 2u * T;
        size_t n = 0;
        for (uint32_t y = y0; y < y1; ++y) {
            const float* row = frame.data() + static_cast<size_t>(y) * kW;
            for (uint32_t x = x0 + 1u; x < x1; ++x) chunk[n++] = row[x] - row[x - 1u];
        }
        for (uint32_t y = y0 + 1u; y < y1; ++y) {
            const float* row = frame.data() + static_cast<size_t>(y) * kW;
            const float* up = row - kW;
            for (uint32_t x = x0; x < x1; ++x) chunk[n++] = row[x] - up[x];
        }
        ASSERT_EQ(perTile, n);
    }

    std::unique_ptr<float[]> diffBuf(new float[perTile]);
    std::vector<float> src(kN, 1.0f), dst(kN);
    ReusableSelector reuse;
    volatile float sink = 0.0f;

    // per-round measurements, keyed by term
    struct Term { std::string name; std::string provenance; std::function<double()> run; std::vector<double> ms; };
    std::vector<Term> terms;

    terms.push_back({"SHIPPED whole path (xpe_defect_detect_runtime)", "shipped", [&] {
        XpeErrorCode rc = XPE_OK;
        const double ms = timeMs([&] { rc = xpe_defect_detect_runtime(&img, &meta, &out); });
        EXPECT_EQ(XPE_OK, rc);
        return ms; }, {}});
    terms.push_back({"shipped stage: tile sigma (BuildFrameConfig)", "shipped", [&] {
        std::vector<float> ts;
        return timeMs([&] { RuntimeDetectionConfig c = BuildFrameConfig(&img, ts); sink = c.sigmaThreshold; }); }, {}});
    terms.push_back({"shipped stage: memset of map", "shipped", [&] {
        return timeMs([&] { std::memset(map.data(), 0, kN); }); }, {}});
    terms.push_back({"shipped stage: row traversal (DetectRowRange)", "shipped", [&] {
        std::vector<float> ts;
        RuntimeDetectionConfig c = BuildFrameConfig(&img, ts);
        std::memset(map.data(), 0, kN);
        std::vector<float> wv, dev; wv.reserve(64); dev.reserve(64);
        return timeMs([&] { DetectRowRange(&img, c, map.data(), 0u, kH, wv, dev); }); }, {}});
    terms.push_back({"floor: detection network 2 x N x ns/net", "independent kernel x computed count", [&] {
#if XPE_DETECT_HAS_AVX2
        constexpr size_t kNets = 1000000u;
        __m256 v[8];
        for (int k = 0; k < 8; ++k) v[k] = _mm256_set1_ps(static_cast<float>(k) * 1.37f);
        const double msNet = timeMs([&] {
            __m256 acc = _mm256_setzero_ps();
            for (size_t i = 0; i < kNets; ++i) {
                v[0] = _mm256_add_ps(v[0], _mm256_set1_ps(1e-7f));
                acc = _mm256_add_ps(acc, MedianOfEight8(v));
            }
            alignas(32) float o[8];
            _mm256_store_ps(o, acc);
            sink = o[0];
        });
        const double nsPerNet = msNet * 1e6 / static_cast<double>(kNets * 8u);
        return 2.0 * static_cast<double>(kN) * nsPerNet / 1e6;
#else
        return 0.0;
#endif
    }, {}});
    terms.push_back({"floor: tile differences (loop copy)", "independent", [&] {
        return timeMs([&] {
            for (uint32_t ty = 0; ty < tilesY; ++ty)
                for (uint32_t tx = 0; tx < tilesX; ++tx) {
                    const uint32_t x0 = tx * T, x1 = std::min(x0 + T, kW);
                    const uint32_t y0 = ty * T, y1 = std::min(y0 + T, kH);
                    float* o = diffBuf.get(); size_t n = 0;
                    for (uint32_t y = y0; y < y1; ++y) {
                        const float* row = frame.data() + static_cast<size_t>(y) * kW;
                        for (uint32_t x = x0 + 1u; x < x1; ++x) o[n++] = row[x] - row[x - 1u];
                    }
                    for (uint32_t y = y0 + 1u; y < y1; ++y) {
                        const float* row = frame.data() + static_cast<size_t>(y) * kW;
                        const float* up = row - kW;
                        for (uint32_t x = x0; x < x1; ++x) o[n++] = row[x] - up[x];
                    }
                    if (n) sink = o[n - 1u];
                }
        }); }, {}});
    terms.push_back({"floor: per-tile copy only (subtracted from the 3 selection loops)", "independent", [&] {
        return timeMs([&] { for (size_t t = 0; t < tiles; ++t) std::copy(chunk.begin(), chunk.end(), scratch.begin()); }); }, {}});

    auto selLoop = [&](auto&& selector) {
        return timeMs([&] {
            for (size_t t = 0; t < tiles; ++t) {
                std::copy(chunk.begin(), chunk.end(), scratch.begin());
                const float m = selector(scratch.data(), perTile, perTile / 2u);
                for (size_t i = 0; i < perTile; ++i) scratch[i] = std::abs(scratch[i] - m);
                sink = selector(scratch.data(), perTile, perTile / 2u);
            }
        });
    };
    terms.push_back({"floor: selection, SHIPPED algorithm in an isolated loop (incl. copy)", "isolated loop, shipped algorithm", [&] {
        return selLoop([](float* d, size_t n, size_t k) { return SelectKthSmallest(d, n, k); }); }, {}});
    terms.push_back({"alt: selection, histogram reused (incl. copy)", "isolated loop, other algorithm", [&] {
        return selLoop([&](float* d, size_t n, size_t k) { return reuse.select(d, n, k); }); }, {}});
    terms.push_back({"alt: selection, std::nth_element (incl. copy)", "isolated loop, other algorithm", [&] {
        return selLoop([](float* d, size_t n, size_t k) { std::nth_element(d, d + k, d + n); return d[k]; }); }, {}});
    terms.push_back({"floor: memcpy 37.75 MB (bandwidth reference)", "independent", [&] {
        return timeMs([&] { std::memcpy(dst.data(), src.data(), kN * sizeof(float)); }); }, {}});

    // one discarded warm-up of everything
    for (auto& t : terms) (void)t.run();

    // Alternate the ORDER between rounds so neither side owns the first/last slot.
    std::vector<size_t> order(terms.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    for (int r = 0; r < kRounds; ++r) {
        std::vector<size_t> o = order;
        if (r % 2 == 1) std::reverse(o.begin(), o.end());
        for (size_t i : o) terms[i].ms.push_back(terms[i].run());
    }

    std::printf("\n[a174] 3072x3072 FLOAT32, AVX2, single thread, %d rounds, 1 warm-up discarded;"
                " order forward on even rounds, reversed on odd\n", kRounds);
    std::printf("| # | term | provenance | min ms | median | max ms |\n|---|---|---|---|---|---|\n");
    for (size_t i = 0; i < terms.size(); ++i) {
        std::printf("| %zu | %s | %s | %.2f | %.2f | %.2f |\n", i, terms[i].name.c_str(),
                    terms[i].provenance.c_str(), minOf(terms[i].ms), medOf(terms[i].ms), maxOf(terms[i].ms));
    }
    std::printf("\n[a174] per-round observations (ms), same index as above\n");
    for (size_t i = 0; i < terms.size(); ++i) {
        std::printf("[a174] #%zu obs:", i);
        for (double v : terms[i].ms) std::printf(" %.2f", v);
        std::printf("\n");
    }

    // ---- derived, all from this session -------------------------------------
    const auto& W  = terms[0].ms; const auto& TS = terms[1].ms; const auto& MS = terms[2].ms;
    const auto& RW = terms[3].ms; const auto& DN = terms[4].ms; const auto& DF = terms[5].ms;
    const auto& CP = terms[6].ms; const auto& S0 = terms[7].ms; const auto& S1 = terms[8].ms;
    const auto& S2 = terms[9].ms; const auto& MC = terms[10].ms;

    const double w = minOf(W), ts = minOf(TS), ms = minOf(MS), rw = minOf(RW);
    const double dn = minOf(DN), df = minOf(DF), cp = minOf(CP);
    const double selShipped = minOf(S0) - cp, selReuse = minOf(S1) - cp, selNth = minOf(S2) - cp;
    const double selInSitu = ts - df;
    const double gbs = (2.0 * kN * sizeof(float)) / (minOf(MC) * 1e-3) / 1e9;
    const double mb = static_cast<double>(kN) * 4.0 * 2.0 / 1e6 + static_cast<double>(kN) / 1e6;
    const double memBound = mb / (gbs * 1000.0) * 1000.0;

    std::printf("\n[a174] session minima: shipped %.2f | tile-sigma stage %.2f | memset %.2f | rows %.2f"
                " | stage sum %.2f\n", w, ts, ms, rw, ts + ms + rw);
    std::printf("[a174] bound terms:  detection net %.2f | differences %.2f | memory %.2f (%.1f GB/s)\n",
                dn, df, memBound, gbs);
    std::printf("[a174] selection:    in-situ (stage - differences) %.2f | isolated shipped %.2f"
                " | histogram reused %.2f | nth_element %.2f\n", selInSitu, selShipped, selReuse, selNth);

    const double floorOld = dn + df + selInSitu;
    const double floorIso = dn + df + selShipped;
    const double floorBest = dn + df + std::min({selShipped, selReuse, selNth});
    std::printf("[a174] FLOOR (old definition, in-situ selection)     = %.2f + %.2f + %.2f = %.2f ms\n",
                dn, df, selInSitu, floorOld);
    std::printf("[a174] FLOOR (isolated-loop shipped selection)       = %.2f + %.2f + %.2f = %.2f ms\n",
                dn, df, selShipped, floorIso);
    std::printf("[a174] FLOOR (fastest measured selector)             = %.2f ms\n", floorBest);
    std::printf("[a174] R1  shipped / floor(old def)      = %.3f\n", w / floorOld);
    std::printf("[a174] R2  shipped / floor(isolated sel) = %.3f\n", w / floorIso);
    std::printf("[a174] R3  shipped / floor(fastest sel)  = %.3f\n", w / floorBest);
    std::printf("[a174] R'  selection removed from BOTH   = (%.2f - %.2f) / (%.2f + %.2f) = %.3f\n",
                w, selInSitu, dn, df, (w - selInSitu) / (dn + df));

    // per-round ratios (each round's shipped vs that round's own floor terms)
    std::vector<double> r1, r2;
    for (int r = 0; r < kRounds; ++r) {
        r1.push_back(W[r] / (DN[r] + TS[r]));                         // old def: DN + DF + (TS - DF)
        r2.push_back(W[r] / (DN[r] + DF[r] + (S0[r] - CP[r])));
    }
    std::printf("[a174] per-round R1: min %.3f median %.3f max %.3f | per-round R2: min %.3f median %.3f max %.3f\n",
                minOf(r1), medOf(r1), maxOf(r1), minOf(r2), medOf(r2), maxOf(r2));
}
