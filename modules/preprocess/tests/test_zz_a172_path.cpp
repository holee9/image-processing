/**
 * @file test_zz_a172_path.cpp
 * @brief QA-A-172 (#230) -- does a path to the 1.3x target exist?
 *
 * All DISABLED_ on purpose. Run with:
 *   xpe_preprocess_tests.exe --gtest_also_run_disabled_tests --gtest_filter=A172Path.*
 *
 * WHY THIS PROBE EXISTS. The card assumed the dominant term (tile selection)
 * could not be isolated because xpe_interpolate_pixel / median_filter_cluster
 * are not exported (LNK2019 in QA-A-144 / QA-A-169). Those two functions belong
 * to defect_correct.cpp (REQ-P1A-012). The detector this card is about
 * (REQ-P1A-013) keeps SelectKthSmallest, ComputeTileSigmas, BuildFrameConfig
 * and DetectRowRange as `inline` functions in runtime_detection.h, so a test
 * calls them directly: there is no DLL boundary to cross. QA-A-165 and
 * QA-A-166 already did exactly that.
 *
 * WHAT WAS NOT DECOMPOSED. QA-A-165 wrote the shipped total as
 *   tile diff 6 + tile selection 65 + detect 53 = 122 ms
 * while its own falsification table gave the AVX2 detect pass as 13.5 ms. Those
 * two "detect" figures cannot both describe the same pass, so ~40 ms sat in no
 * decomposition. This probe times the stages of the SHIPPED entry point in one
 * process and checks that they add up to the whole -- the check QA-A-169's
 * unexplained 4.93 ms showed is needed.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "runtime_detection.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
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

std::vector<float> noiseFrame() {
    std::mt19937 rng(20260911u);
    std::normal_distribution<float> g(0.0f, 1.0f);
    std::vector<float> f(kN);
    for (size_t i = 0; i < kN; ++i) f[i] = 3000.0f + 10.0f * g(rng);
    return f;
}

XpeImageBuffer imageOf(std::vector<float>& f) {
    XpeImageBuffer img{};
    img.data = f.data(); img.width = kW; img.height = kH;
    img.bitsAllocated = 32; img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.dataSize = static_cast<uint32_t>(kN * sizeof(float));
    return img;
}

XpeImageBuffer mapOf(std::vector<uint8_t>& m) {
    XpeImageBuffer out{};
    out.data = m.data(); out.width = kW; out.height = kH;
    out.bitsAllocated = 8; out.bitsStored = 8;
    out.format = XPE_PIXEL_UINT8;
    out.dataSize = static_cast<uint32_t>(kN);
    return out;
}

double minOf(const std::vector<double>& v) { return *std::min_element(v.begin(), v.end()); }

class A172Path : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr)); }
    void TearDown() override { xpe_preprocess_shutdown(); }
};

} // namespace

// (1) Do the stages of the shipped entry point add up to the shipped total?
//
// Each round times, in this order and in ONE process: the shipped entry point,
// then the three things it does (BuildFrameConfig, memset of the map,
// DetectRowRange). The stages call the same inline functions the entry point
// calls, on the same frame, into a map buffer that was already touched.
TEST_F(A172Path, DISABLED_StageSumVsWhole) {
    std::vector<float> frame = noiseFrame();
    std::vector<uint8_t> map(kN, 0u);
    XpeImageBuffer img = imageOf(frame);
    XpeImageBuffer out = mapOf(map);
    XpeImageMetadata meta{};

    constexpr int kRounds = 12;
    std::vector<double> whole, tile, clear, rows, sumRound;

    // one discarded warm-up of everything
    (void)xpe_defect_detect_runtime(&img, &meta, &out);

    for (int r = 0; r < kRounds; ++r) {
        const auto w0 = std::chrono::steady_clock::now();
        const XpeErrorCode rc = xpe_defect_detect_runtime(&img, &meta, &out);
        const auto w1 = std::chrono::steady_clock::now();
        ASSERT_EQ(XPE_OK, rc);

        std::vector<float> tileSigmas;
        const auto a0 = std::chrono::steady_clock::now();
        RuntimeDetectionConfig cfg = BuildFrameConfig(&img, tileSigmas);
        const auto a1 = std::chrono::steady_clock::now();
        std::memset(map.data(), 0, kN);
        const auto a2 = std::chrono::steady_clock::now();
        std::vector<float> wv, dev;
        wv.reserve(64); dev.reserve(64);
        DetectRowRange(&img, cfg, map.data(), 0u, kH, wv, dev);
        const auto a3 = std::chrono::steady_clock::now();

        whole.push_back(msOf(w0, w1));
        tile.push_back(msOf(a0, a1));
        clear.push_back(msOf(a1, a2));
        rows.push_back(msOf(a2, a3));
        sumRound.push_back(tile.back() + clear.back() + rows.back());
    }

    std::printf("\n[a172] shipped entry point vs its stages -- 3072x3072 FLOAT32, AVX2,\n");
    std::printf("[a172]   single thread, i7-12700, %d rounds, 1 warm-up discarded\n\n", kRounds);
    std::printf("| round | whole | tile sigma | memset | rows  | stage sum | whole - sum |\n");
    std::printf("|-------|-------|------------|--------|-------|-----------|-------------|\n");
    for (int r = 0; r < kRounds; ++r) {
        std::printf("| %5d | %5.1f | %10.1f | %6.2f | %5.1f | %9.1f | %11.1f |\n",
                    r, whole[r], tile[r], clear[r], rows[r], sumRound[r],
                    whole[r] - sumRound[r]);
    }
    const double mW = minOf(whole), mT = minOf(tile), mC = minOf(clear), mR = minOf(rows);
    std::printf("\n[a172]   MINIMUM of each:  whole %.1f | tile %.1f | memset %.2f | rows %.1f\n",
                mW, mT, mC, mR);
    std::printf("[a172]   sum of minima    %.1f   vs   whole minimum %.1f   (gap %.1f ms)\n\n",
                mT + mC + mR, mW, mW - (mT + mC + mR));
}

#if XPE_DETECT_HAS_AVX2

namespace {

// FALSIFICATION SWITCH. Read at run time so /WX cannot fold it away (an
// `if (false && ...)` is a build error under this project's warnings).
static volatile bool kSwapRefs = false;

/**
 * PROBE-LOCAL COPY of DetectEightPixelsAvx2, changed in exactly ONE place: the
 * blend reference is a per-lane vector instead of a broadcast, so a run whose
 * eight columns straddle a tile boundary can stay in the vector path.
 *
 * Every other line is the shipped one. That is what makes the equivalence check
 * below mean something -- and it is a COPY, so the check compares it pixel for
 * pixel against the shipped DetectRowRange rather than trusting it. Only valid
 * for blendWeight > 0 (the shipped configuration).
 */
inline void DetectEightPixelsPerLane(const float* pixels, uint32_t w, uint32_t x,
                                     uint32_t y, const RuntimeDetectionConfig& config,
                                     uint8_t* map) {
    const float* rowUp = pixels + static_cast<size_t>(y - 1u) * w;
    const float* rowMid = pixels + static_cast<size_t>(y) * w;
    const float* rowDn = pixels + static_cast<size_t>(y + 1u) * w;

    __m256 n[8];
    n[0] = _mm256_loadu_ps(rowUp + x - 1u);
    n[1] = _mm256_loadu_ps(rowUp + x);
    n[2] = _mm256_loadu_ps(rowUp + x + 1u);
    n[3] = _mm256_loadu_ps(rowMid + x - 1u);
    n[4] = _mm256_loadu_ps(rowMid + x + 1u);
    n[5] = _mm256_loadu_ps(rowDn + x - 1u);
    n[6] = _mm256_loadu_ps(rowDn + x);
    n[7] = _mm256_loadu_ps(rowDn + x + 1u);

    const __m256 median = MedianOfEight8(n);

    const __m256 absMask = _mm256_castsi256_ps(_mm256_set1_epi32(0x7FFFFFFF));
    __m256 d[8];
    for (int k = 0; k < 8; ++k) {
        d[k] = _mm256_and_ps(_mm256_sub_ps(n[k], median), absMask);
    }
    const __m256 mad = _mm256_mul_ps(MedianOfEight8(d),
                                     _mm256_set1_ps(RUNTIME_DETECTION_MAD_SCALE));

    // THE ONE CHANGE. Lanes [0, cut) read the tile that column x is in, lanes
    // [cut, 8) read the tile column x+7 is in. When the run does not straddle,
    // both reads hit the same tile and cut <= 0, so every lane takes the right
    // value and it equals the left one -- the same result as the broadcast.
    const float wgt = config.blendWeight;
    float sqL = (1.0f - wgt) * BlendReferenceAt(config, x, y) * BlendReferenceAt(config, x, y);
    float sqR = (1.0f - wgt) * BlendReferenceAt(config, x + 7u, y) * BlendReferenceAt(config, x + 7u, y);
    if (kSwapRefs) { const float t = sqL; sqL = sqR; sqR = t; }
    const uint32_t tile = config.tileSize;
    const int32_t cut = static_cast<int32_t>(((x + 7u) / tile) * tile) - static_cast<int32_t>(x);
    const __m256i lane = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
    const __m256 useL = _mm256_castsi256_ps(_mm256_cmpgt_epi32(_mm256_set1_epi32(cut), lane));
    const __m256 refSq = _mm256_blendv_ps(_mm256_set1_ps(sqR), _mm256_set1_ps(sqL), useL);

    const __m256 weight = _mm256_set1_ps(wgt);
    __m256 sigma = _mm256_sqrt_ps(_mm256_add_ps(_mm256_mul_ps(weight, _mm256_mul_ps(mad, mad)), refSq));
    if (config.globalSigmaCap > 0.0f) {
        sigma = SelectLesserOf(_mm256_set1_ps(config.globalSigmaCap), sigma);
    }

    const __m256 centre = _mm256_loadu_ps(rowMid + x);
    const __m256 deviation = _mm256_and_ps(_mm256_sub_ps(centre, median), absMask);

    const __m256 eps = _mm256_set1_ps(1e-6f);
    const __m256 flat = _mm256_cmp_ps(sigma, eps, _CMP_LT_OQ);
    const __m256 flatVerdict = _mm256_cmp_ps(deviation, eps, _CMP_GT_OQ);
    const __m256 threshold = _mm256_mul_ps(_mm256_set1_ps(config.sigmaThreshold), sigma);
    const __m256 normVerdict = _mm256_cmp_ps(deviation, threshold, _CMP_GT_OQ);

    const int bits = _mm256_movemask_ps(_mm256_blendv_ps(normVerdict, flatVerdict, flat));

    uint8_t* out = map + static_cast<size_t>(y) * w + x;
    for (int k = 0; k < 8; ++k) {
        out[k] = static_cast<uint8_t>((bits >> k) & 1);
    }
}

/** DetectRowRange's shape with NO tile-straddle branch. Same walk, same edges. */
inline void DetectRowRangePerLane(const XpeImageBuffer* img, const RuntimeDetectionConfig& config,
                                  uint8_t* map, uint32_t y0, uint32_t y1,
                                  std::vector<float>& wv, std::vector<float>& dev) {
    const uint32_t w = img->width;
    const uint32_t h = img->height;
    const float* pixels = static_cast<const float*>(img->data);
    auto scalarSpan = [&](uint32_t y, uint32_t xa, uint32_t xb) {
        for (uint32_t x = xa; x < xb; ++x) {
            if (DetectDefectivePixel(img, x, y, config, wv, dev)) {
                map[static_cast<size_t>(y) * w + x] = 1u;
            }
        }
    };
    for (uint32_t y = y0; y < y1; ++y) {
        if (y != 0u && y + 1u < h) {
            scalarSpan(y, 0u, 1u);
            uint32_t x = 1u;
            for (; x + 8u <= w - 1u; x += 8u) {
                DetectEightPixelsPerLane(pixels, w, x, y, config, map);
            }
            if (x + 1u < w) {
                DetectEightPixelsPerLane(pixels, w, DetectRowLastRunStart(w), y, config, map);
            }
            scalarSpan(y, w - 1u, w);
            continue;
        }
        scalarSpan(y, 0u, w);
    }
}

/** Columns of the forward walk that belong to a tile-straddling run. */
std::vector<uint8_t> straddleColumns(uint32_t tile) {
    std::vector<uint8_t> col(kW, 0u);
    for (uint32_t x = 1u; x + 8u <= kW - 1u; x += 8u) {
        if ((x / tile) != ((x + 7u) / tile)) {
            for (uint32_t k = 0; k < 8u; ++k) col[x + k] = 1u;
        }
    }
    return col;
}

/** Noise whose level steps 4x across a tile boundary, plus outliers beside every
 *  boundary. A uniform frame would give every tile the same reference, so a run
 *  that took the WRONG lane's reference would still produce the right answer and
 *  the equivalence check could not fail. */
std::vector<float> stepFrame() {
    std::mt19937 rng(20260930u);
    std::normal_distribution<float> g(0.0f, 1.0f);
    std::vector<float> f(kN);
    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            const float sigma = (x < 1024u) ? 10.0f : 40.0f;
            f[static_cast<size_t>(y) * kW + x] = 3000.0f + sigma * g(rng);
        }
    }
    std::uniform_real_distribution<float> k(4.0f, 8.0f);
    for (uint32_t y = 3; y + 3 < kH; y += 5) {
        for (uint32_t b = 128; b < kW; b += 128) {
            const float s = (b - 1u < 1024u) ? 10.0f : 40.0f;
            f[static_cast<size_t>(y) * kW + (b - 1u)] += k(rng) * s;
            f[static_cast<size_t>(y) * kW + b] += k(rng) * s;
        }
    }
    return f;
}

size_t mismatches(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, size_t* flagged) {
    size_t diff = 0, fl = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) ++diff;
        if (a[i] != 0u) ++fl;
    }
    if (flagged) *flagged = fl;
    return diff;
}

} // namespace

// (2) What does the tile-straddle fallback cost? Two independent estimates that
// must agree: the time difference between the shipped rows and the same rows
// with no straddling run, and the direct cost of the pixels that go scalar.
TEST_F(A172Path, DISABLED_StraddleFallbackCost) {
    std::vector<float> frame = noiseFrame();
    XpeImageBuffer img = imageOf(frame);
    std::vector<float> tileSigmas;
    RuntimeDetectionConfig cfg = BuildFrameConfig(&img, tileSigmas);

    // The same config, one tile per row: every run shares a tile, so nothing
    // straddles. The numbers it produces differ; only its TIME is used.
    std::vector<float> oneTile(1, tileSigmas[0]);
    RuntimeDetectionConfig cfgOne = cfg;
    cfgOne.tileSigma = oneTile.data();
    cfgOne.tileSize = kW;
    cfgOne.tilesX = 1u;

    std::vector<uint8_t> mapS(kN, 0u), mapN(kN, 0u);
    std::vector<float> wv, dev; wv.reserve(64); dev.reserve(64);

    constexpr int kRounds = 12;
    std::vector<double> rowsShipped, rowsNoStraddle;
    DetectRowRange(&img, cfg, mapS.data(), 0u, kH, wv, dev);        // warm-up
    DetectRowRange(&img, cfgOne, mapN.data(), 0u, kH, wv, dev);
    for (int r = 0; r < kRounds; ++r) {
        auto a0 = std::chrono::steady_clock::now();
        DetectRowRange(&img, cfg, mapS.data(), 0u, kH, wv, dev);
        auto a1 = std::chrono::steady_clock::now();
        DetectRowRange(&img, cfgOne, mapN.data(), 0u, kH, wv, dev);
        auto a2 = std::chrono::steady_clock::now();
        rowsShipped.push_back(msOf(a0, a1));
        rowsNoStraddle.push_back(msOf(a1, a2));
    }

    // Independent estimate: exactly the pixels DetectRowRange sends to
    // scalarSpan because their run straddles, timed through the same function.
    const std::vector<uint8_t> col = straddleColumns(cfg.tileSize);
    std::vector<uint32_t> xs;
    for (uint32_t x = 0; x < kW; ++x) if (col[x]) xs.push_back(x);
    const size_t perRow = xs.size();
    const size_t totalPx = perRow * (kH - 2u);
    std::vector<uint8_t> sink(kN, 0u);
    std::vector<double> scalarMs;
    for (int r = 0; r < 5; ++r) {
        auto s0 = std::chrono::steady_clock::now();
        for (uint32_t y = 1u; y + 1u < kH; ++y) {
            for (uint32_t x : xs) {
                if (DetectDefectivePixel(&img, x, y, cfg, wv, dev)) {
                    sink[static_cast<size_t>(y) * kW + x] = 1u;
                }
            }
        }
        auto s1 = std::chrono::steady_clock::now();
        scalarMs.push_back(msOf(s0, s1));
    }
    // What the SAME pixels cost when they stay in the vector path.
    std::vector<double> vecMs;
    const float* px = static_cast<const float*>(img.data);
    for (int r = 0; r < 5; ++r) {
        auto v0 = std::chrono::steady_clock::now();
        for (uint32_t y = 1u; y + 1u < kH; ++y) {
            for (uint32_t x = 1u; x + 8u <= kW - 1u; x += 8u) {
                if ((x / cfg.tileSize) != ((x + 7u) / cfg.tileSize)) {
                    DetectEightPixelsAvx2(px, kW, x, y, cfgOne, sink.data());
                }
            }
        }
        auto v1 = std::chrono::steady_clock::now();
        vecMs.push_back(msOf(v0, v1));
    }

    const double mS = minOf(rowsShipped), mN = minOf(rowsNoStraddle);
    const double mSc = minOf(scalarMs), mV = minOf(vecMs);
    std::printf("\n[a172] tile-straddle fallback -- 3072x3072 FLOAT32, AVX2, single thread,\n");
    std::printf("[a172]   i7-12700, tile %u, %d interleaved rounds, minimum reported\n\n",
                cfg.tileSize, kRounds);
    std::printf("[a172]   pixels sent to the scalar path per row : %zu of %u (%.2f%%)\n",
                perRow, kW, 100.0 * static_cast<double>(perRow) / kW);
    std::printf("[a172]   ... over the frame                      : %zu of %zu (%.2f%%)\n",
                totalPx, kN, 100.0 * static_cast<double>(totalPx) / static_cast<double>(kN));
    std::printf("[a172]   rows, shipped                           : %6.1f ms\n", mS);
    std::printf("[a172]   rows, no straddling run (time only)     : %6.1f ms\n", mN);
    std::printf("[a172]   ==> time difference                     : %6.1f ms\n", mS - mN);
    std::printf("[a172]   those pixels through the scalar path    : %6.1f ms  (%.1f ns/pixel)\n",
                mSc, mSc * 1e6 / static_cast<double>(totalPx));
    std::printf("[a172]   the same runs through the vector path   : %6.1f ms\n", mV);
    std::printf("[a172]   ==> scalar - vector for those runs      : %6.1f ms\n", mSc - mV);
    std::printf("[a172]   the two estimates differ by             : %6.1f ms\n\n",
                (mS - mN) - (mSc - mV));
}

// (3) The replacement, measured rather than calculated: is the per-lane form
// pixel-identical to the shipped one, and what does it cost?
TEST_F(A172Path, DISABLED_PerLaneEquivalenceAndTime) {
    std::vector<float> noise = noiseFrame();
    std::vector<float> step = stepFrame();
    const std::vector<uint8_t> col = straddleColumns(RUNTIME_DETECTION_TILE_SIZE);

    struct Named { const char* name; std::vector<float>* f; };
    const Named frames[] = { {"noise (uniform)", &noise}, {"step 10->40 + outliers", &step} };

    std::printf("\n[a172] equivalence of the per-lane form against the shipped DetectRowRange\n\n");
    std::printf("| frame                    | flagged | flagged in straddling runs | mismatches | mismatches, refs swapped |\n");
    std::printf("|--------------------------|---------|----------------------------|------------|--------------------------|\n");
    for (const Named& nf : frames) {
        XpeImageBuffer img = imageOf(*nf.f);
        std::vector<float> ts;
        RuntimeDetectionConfig cfg = BuildFrameConfig(&img, ts);
        std::vector<uint8_t> a(kN, 0u), b(kN, 0u), c(kN, 0u);
        std::vector<float> wv, dev; wv.reserve(64); dev.reserve(64);
        DetectRowRange(&img, cfg, a.data(), 0u, kH, wv, dev);
        DetectRowRangePerLane(&img, cfg, b.data(), 0u, kH, wv, dev);
        kSwapRefs = true;                                   // falsification arm
        DetectRowRangePerLane(&img, cfg, c.data(), 0u, kH, wv, dev);
        kSwapRefs = false;

        size_t flagged = 0;
        const size_t diff = mismatches(a, b, &flagged);
        const size_t diffSwap = mismatches(a, c, nullptr);
        size_t inStraddle = 0;
        for (uint32_t y = 1u; y + 1u < kH; ++y)
            for (uint32_t x = 0; x < kW; ++x)
                if (col[x] && a[static_cast<size_t>(y) * kW + x]) ++inStraddle;
        std::printf("| %-24s | %7zu | %26zu | %10zu | %24zu |\n",
                    nf.name, flagged, inStraddle, diff, diffSwap);
        EXPECT_EQ(0u, diff) << nf.name;
    }
    std::printf("\n");

    // Timing. Rounds interleave the shipped rows and the per-lane rows so a slow
    // spell in this machine's bimodal timing hits both.
    XpeImageBuffer img = imageOf(noise);
    std::vector<float> ts;
    RuntimeDetectionConfig cfg = BuildFrameConfig(&img, ts);
    std::vector<uint8_t> mS(kN, 0u), mP(kN, 0u);
    std::vector<float> wv, dev; wv.reserve(64); dev.reserve(64);
    DetectRowRange(&img, cfg, mS.data(), 0u, kH, wv, dev);
    DetectRowRangePerLane(&img, cfg, mP.data(), 0u, kH, wv, dev);
    constexpr int kRounds = 12;
    std::vector<double> tS, tP;
    for (int r = 0; r < kRounds; ++r) {
        auto a0 = std::chrono::steady_clock::now();
        DetectRowRange(&img, cfg, mS.data(), 0u, kH, wv, dev);
        auto a1 = std::chrono::steady_clock::now();
        DetectRowRangePerLane(&img, cfg, mP.data(), 0u, kH, wv, dev);
        auto a2 = std::chrono::steady_clock::now();
        tS.push_back(msOf(a0, a1));
        tP.push_back(msOf(a1, a2));
    }
    std::printf("[a172] rows only, 3072x3072 FLOAT32, AVX2, single thread, i7-12700,\n");
    std::printf("[a172]   %d interleaved rounds, minimum reported\n", kRounds);
    std::printf("[a172]   shipped DetectRowRange        %6.1f ms\n", minOf(tS));
    std::printf("[a172]   per-lane variant (probe-only) %6.1f ms\n", minOf(tP));
    std::printf("[a172]   observations shipped  : ");
    for (double v : tS) std::printf("%.1f ", v);
    std::printf("\n[a172]   observations per-lane : ");
    for (double v : tP) std::printf("%.1f ", v);
    std::printf("\n\n");
}

// (4) The WHOLE path with the replacement, measured -- not the shipped total
// minus the rows difference, which would be a calculation. The replacement
// pipeline does what xpe_defect_detect_runtime does (BuildFrameConfig, clear the
// map, walk the rows) with only the row walk swapped, and it is timed in the same
// rounds as the shipped entry point so a slow spell hits both.
TEST_F(A172Path, DISABLED_WholePathWithPerLane) {
    std::vector<float> frame = noiseFrame();
    XpeImageBuffer img = imageOf(frame);
    std::vector<uint8_t> mapShipped(kN, 0u), mapPerLane(kN, 0u);
    XpeImageBuffer out = mapOf(mapShipped);
    XpeImageMetadata meta{};

    auto perLanePipeline = [&]() {
        std::vector<float> tileSigmas;
        RuntimeDetectionConfig cfg = BuildFrameConfig(&img, tileSigmas);
        std::memset(mapPerLane.data(), 0, kN);
        std::vector<float> wv, dev;
        wv.reserve(64); dev.reserve(64);
        DetectRowRangePerLane(&img, cfg, mapPerLane.data(), 0u, kH, wv, dev);
    };

    ASSERT_EQ(XPE_OK, xpe_defect_detect_runtime(&img, &meta, &out));   // warm-up
    perLanePipeline();
    size_t flagged = 0;
    EXPECT_EQ(0u, mismatches(mapShipped, mapPerLane, &flagged));

    constexpr int kRounds = 12;
    std::vector<double> tS, tP;
    for (int r = 0; r < kRounds; ++r) {
        const auto a0 = std::chrono::steady_clock::now();
        ASSERT_EQ(XPE_OK, xpe_defect_detect_runtime(&img, &meta, &out));
        const auto a1 = std::chrono::steady_clock::now();
        perLanePipeline();
        const auto a2 = std::chrono::steady_clock::now();
        tS.push_back(msOf(a0, a1));
        tP.push_back(msOf(a1, a2));
    }
    // The maps still agree after the timed rounds.
    EXPECT_EQ(0u, mismatches(mapShipped, mapPerLane, nullptr));

    std::printf("\n[a172] WHOLE path, 3072x3072 FLOAT32, AVX2, single thread, i7-12700,\n");
    std::printf("[a172]   %d interleaved rounds, minimum reported, maps identical (0 mismatches)\n", kRounds);
    std::printf("[a172]   shipped xpe_defect_detect_runtime        %6.1f ms\n", minOf(tS));
    std::printf("[a172]   BuildFrameConfig + memset + per-lane rows %6.1f ms\n", minOf(tP));
    std::printf("[a172]   observations shipped  : ");
    for (double v : tS) std::printf("%.1f ", v);
    std::printf("\n[a172]   observations per-lane : ");
    for (double v : tP) std::printf("%.1f ", v);
    std::printf("\n\n");
}

#endif  // XPE_DETECT_HAS_AVX2

// (5) QA-A-144 / QA-A-169 could not time xpe_interpolate_pixel: LNK2019, because
// the DLL does not export it. It has external linkage and lives in helpers.cpp
// (alongside the JSON and CRC-32 helpers), and the test executable already
// compiles other non-exported
// product sources directly (xpe_sha256_backend.cpp carries the comment "Sha256Stream
// is not exported from the DLL"). This case exists to show that the same
// one-line route works here -- it is only reachable if the executable links.
//
// median_filter_cluster is NOT reachable this way: it sits in an anonymous
// namespace (defect_correct.cpp), i.e. internal linkage, so compiling the file
// into the executable does not make it visible to another translation unit.
#include "xpe/preprocess/xpe_preprocess_internal.h"

TEST_F(A172Path, DISABLED_InterpolateKernelIsolated) {
    std::vector<float> frame = noiseFrame();
    std::vector<uint8_t> mask(kN, 0u);
    // isolated sites on a regular lattice, ~0.1% of the frame (the SPEC density)
    const uint32_t stride = 32u;
    std::vector<std::pair<uint32_t, uint32_t>> sites;
    for (uint32_t y = 2; y + 2 < kH; y += stride)
        for (uint32_t x = 2; x + 2 < kW; x += stride) {
            mask[static_cast<size_t>(y) * kW + x] = 1u;
            sites.emplace_back(x, y);
        }

    volatile float sink = 0.0f;
    constexpr int kPasses = 200;
    double best = 1e18;
    for (int rep = 0; rep < 7; ++rep) {
        const auto t0 = std::chrono::steady_clock::now();
        float acc = 0.0f;
        for (int p = 0; p < kPasses; ++p) {
            for (const auto& s : sites) {
                acc += xpe_interpolate_pixel(frame.data(), mask.data(), s.first, s.second, kW, kH);
            }
        }
        const auto t1 = std::chrono::steady_clock::now();
        sink = acc;
        best = std::min(best, msOf(t0, t1));
    }
    (void)sink;
    const double calls = static_cast<double>(sites.size()) * kPasses;
    const double nsPerCall = best * 1e6 / calls;
    std::printf("\n[a172] xpe_interpolate_pixel in isolation -- 3072x3072 FLOAT32, isolated\n");
    std::printf("[a172]   defects, %zu sites (%.3f%% of the frame), %d passes, best of 7\n",
                sites.size(), 100.0 * static_cast<double>(sites.size()) / static_cast<double>(kN), kPasses);
    std::printf("[a172]   %.1f ns per call  ->  one pass over all sites = %.3f ms\n\n",
                nsPerCall, nsPerCall * static_cast<double>(sites.size()) / 1e6);
}
