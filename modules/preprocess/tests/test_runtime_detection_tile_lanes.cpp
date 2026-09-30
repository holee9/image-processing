/**
 * @file test_runtime_detection_tile_lanes.cpp
 * @brief QA-A-173 (#230): a run of eight columns that straddles a tile boundary
 *        stays in the vector path and must decide every pixel exactly as the
 *        scalar rule does.
 *
 * WHY THESE TESTS EXIST AND WHY THEY LOOK LIKE THIS.
 *
 * DetectEightPixelsAvx2 used to broadcast one reference sigma, so a run that
 * straddled a tile boundary went to the scalar path (6% of the pixels at tile
 * 128, two thirds of the row loop). It now takes one reference per lane. The map
 * must not change.
 *
 * The scalar rule ResolveSigma evaluates ((w*mad)*mad) + refSq. The vector code
 * for a run inside one tile evaluates (w*(mad*mad)) + refSq. Those differ in the
 * last place for about 4% of (mad, ref) pairs, so a pixel exactly at the decision
 * threshold can be decided differently. A straddling run therefore uses the
 * SCALAR association. QA-A-173 measured what happens if it does not: on every
 * natural frame and on all seven real CalData_6 frames the two agree pixel for
 * pixel, and only frames that put pixels at the threshold show the difference --
 * 18 pixels flipped in 10 such frames. So a test built from ordinary frames
 * passes either way and proves nothing.
 *
 * Hence the fixture here: pixels bisected to the exact decision threshold of the
 * scalar rule, then offset by -2..+2 ulp, at columns that straddle a tile
 * boundary, against a tile table that is independent of the frame (so changing a
 * pixel cannot move it). Reverting to the vector association, or swapping the
 * left and right reference, makes these tests fail; that was checked, not assumed.
 *
 * The existing thread-parity tests use RuntimeDetection_DefaultConfig(), which has
 * no tile table, so they never reach this code. ThreadedRunsMatchOneThread covers
 * the combination the shipped configuration has: a tile table AND several threads.
 */

#include <gtest/gtest.h>

#include "runtime_detection.h"
#include "xpe/common/xpe_types.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

namespace {

using namespace xpe::preprocess::internal;

XpeImageBuffer Wrap(std::vector<float>& px, uint32_t w, uint32_t h) {
    XpeImageBuffer img{};
    img.data = px.data(); img.width = w; img.height = h;
    img.bitsAllocated = 32; img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.dataSize = static_cast<uint32_t>(px.size() * sizeof(float));
    return img;
}

std::vector<uint8_t> ScalarMap(const XpeImageBuffer& img, const RuntimeDetectionConfig& cfg) {
    std::vector<uint8_t> map(static_cast<size_t>(img.width) * img.height, 0u);
    std::vector<float> wv, dev;
    wv.reserve(64); dev.reserve(64);
    for (uint32_t y = 0; y < img.height; ++y)
        for (uint32_t x = 0; x < img.width; ++x)
            if (DetectDefectivePixel(&img, x, y, cfg, wv, dev))
                map[static_cast<size_t>(y) * img.width + x] = 1u;
    return map;
}

std::vector<uint8_t> RowsMap(const XpeImageBuffer& img, const RuntimeDetectionConfig& cfg) {
    std::vector<uint8_t> map(static_cast<size_t>(img.width) * img.height, 0u);
    std::vector<float> wv, dev;
    wv.reserve(64); dev.reserve(64);
    DetectRowRange(&img, cfg, map.data(), 0u, img.height, wv, dev);
    return map;
}

size_t Differing(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, size_t* first) {
    size_t n = 0;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i] != b[i]) { if (n == 0 && first) *first = i; ++n; }
    return n;
}

struct BoundaryFrame {
    std::vector<float> px;
    std::vector<float> table;
    RuntimeDetectionConfig cfg;
    uint32_t w = 0, h = 0;
    size_t sites = 0;
    std::vector<size_t> siteIdx;   // where the threshold pixels are
};

#if XPE_DETECT_HAS_AVX2
/**
 * The sigma the vector code computed for a run inside one tile BEFORE QA-A-173:
 * weight * (mad * mad) + refSq. Kept here as an oracle, so a fixture can be built
 * around the pixels where that expression and the scalar rule (mad * weight * mad)
 * disagree -- see HazardPixelsAreDecidedByTheScalarRule.
 */
float OldVectorSigma(float mad, float weight, float refSq) {
    const __m256 m = _mm256_set1_ps(mad);
    const __m256 s = _mm256_sqrt_ps(_mm256_add_ps(
        _mm256_mul_ps(_mm256_set1_ps(weight), _mm256_mul_ps(m, m)), _mm256_set1_ps(refSq)));
    return _mm_cvtss_f32(_mm256_castps256_ps128(s));
}
#endif

uint32_t BitsOf(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
float FloatOf(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }

/**
 * Noise whose level steps across the frame, a fixed tile table, and at columns of
 * every straddling run (all eight lanes get used) a pixel bisected to the exact
 * decision threshold of the scalar rule and offset by -2..+2 ulp.
 */
BoundaryFrame MakeBoundaryFrame(uint32_t w, uint32_t h, uint32_t tile, uint32_t seed, uint32_t rowStep,
                                bool hazard = false) {
    BoundaryFrame b;
    b.w = w; b.h = h;
    b.px.resize(static_cast<size_t>(w) * h);
    std::mt19937 rng(seed);
    std::normal_distribution<float> g(0.0f, 1.0f);
    const uint32_t stepX = std::max<uint32_t>(2u, w / 3u);
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x)
            b.px[static_cast<size_t>(y) * w + x] = 3000.0f + ((x < stepX) ? 10.0f : 40.0f) * g(rng);

    const uint32_t tilesX = (w + tile - 1u) / tile, tilesY = (h + tile - 1u) / tile;
    b.table.resize(static_cast<size_t>(tilesX) * tilesY);
    for (uint32_t ty = 0; ty < tilesY; ++ty)
        for (uint32_t tx = 0; tx < tilesX; ++tx)
            b.table[static_cast<size_t>(ty) * tilesX + tx] = 8.0f + static_cast<float>((tx * 7u + ty * 3u) % 40u);

    b.cfg = RuntimeDetection_DefaultConfig();
    b.cfg.blendWeight = RUNTIME_DETECTION_BLEND_WEIGHT;
    b.cfg.tileSigma = b.table.data();
    b.cfg.tileSize = tile;
    b.cfg.tilesX = tilesX;
    b.cfg.blendReference = 0.0f;

    XpeImageBuffer img = Wrap(b.px, w, h);
    std::vector<float> wv, dev;
    wv.reserve(64); dev.reserve(64);
    size_t counter = 0;
    for (uint32_t y = 4; y + 4 < h; y += rowStep) {
        for (uint32_t x0 = 1u; x0 + 8u <= w - 1u; x0 += 8u) {
            if ((x0 / tile) == ((x0 + 7u) / tile)) continue;
            const uint32_t x = x0 + static_cast<uint32_t>((y / rowStep + x0 / tile) % 8u);
            if (x < 2u || x + 2u >= w) continue;
            const size_t idx = static_cast<size_t>(y) * w + x;
#if XPE_DETECT_HAS_AVX2
            if (hazard) {
                // Search for a neighbourhood and a pixel value where the scalar rule and
                // the pre-QA-A-173 vector expression DECIDE DIFFERENTLY. It needs the two
                // sigmas to differ in the last place (about 4% of neighbourhoods) AND a
                // representable pixel value to lie between the two thresholds (about 6%
                // of those), so most attempts fail; the search is bounded and a site it
                // cannot satisfy is skipped.
                std::mt19937 nrng(seed ^ (static_cast<uint32_t>(idx) * 2654435761u));
                std::normal_distribution<float> ng(0.0f, 1.0f);
                const float nsig = (x < stepX) ? 10.0f : 40.0f;
                static const int kDy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
                static const int kDx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
                std::vector<float> dv;
                bool found = false;
                for (int attempt = 0; attempt < 12000 && !found; ++attempt) {
                    for (int k = 0; k < 8; ++k)
                        b.px[static_cast<size_t>(static_cast<int>(y) + kDy[k]) * w + static_cast<size_t>(static_cast<int>(x) + kDx[k])] =
                            3000.0f + nsig * ng(nrng);
                    CollectNeighborValues(&img, x, y, 3, wv);
                    const float medN = ComputeMedian(wv);
                    dv.assign(wv.begin(), wv.end());
                    const float madN = ComputeMAD(dv, medN);
                    const float refN = BlendReferenceAt(b.cfg, x, y);
                    const float sScalar = ResolveSigma(madN, b.cfg, x, y);
                    const float sOld = OldVectorSigma(madN, b.cfg.blendWeight,
                                                      (1.0f - b.cfg.blendWeight) * refN * refN);
                    if (BitsOf(sScalar) == BitsOf(sOld)) continue;
                    const float tS = b.cfg.sigmaThreshold * sScalar;
                    const float tV = b.cfg.sigmaThreshold * sOld;
                    const uint32_t base = BitsOf(medN + std::min(tS, tV));
                    for (int k = -40; k <= 40; ++k) {
                        const float v = FloatOf(static_cast<uint32_t>(static_cast<int64_t>(base) + k));
                        const float d = std::abs(v - medN);
                        if ((d > tS) != (d > tV)) { b.px[idx] = v; found = true; break; }
                    }
                }
                if (found) { b.siteIdx.push_back(idx); ++counter; }
                continue;
            }
#else
            (void)hazard;
#endif
            CollectNeighborValues(&img, x, y, 3, wv);
            std::vector<float> nb = wv;
            std::sort(nb.begin(), nb.end());
            const float med = 0.5f * (nb[3] + nb[4]);
            b.px[idx] = med;
            if (DetectDefectivePixel(&img, x, y, b.cfg, wv, dev)) continue;
            b.px[idx] = med + 2000.0f;
            if (!DetectDefectivePixel(&img, x, y, b.cfg, wv, dev)) continue;
            uint32_t lo = 0, hi = 0;
            float flo = med, fhi = med + 2000.0f;
            std::memcpy(&lo, &flo, 4); std::memcpy(&hi, &fhi, 4);
            while (hi - lo > 1u) {
                const uint32_t mid = lo + (hi - lo) / 2u;
                float fm; std::memcpy(&fm, &mid, 4);
                b.px[idx] = fm;
                if (DetectDefectivePixel(&img, x, y, b.cfg, wv, dev)) hi = mid; else lo = mid;
            }
            const int k = static_cast<int>(counter % 5u) - 2;
            const uint32_t bits = static_cast<uint32_t>(static_cast<int64_t>(hi) + k);
            float v; std::memcpy(&v, &bits, 4);
            b.px[idx] = v;
            b.siteIdx.push_back(idx);
            ++counter;
        }
    }
    b.sites = counter;
    return b;
}

void ExpectMatchesScalar(uint32_t w, uint32_t h, uint32_t tile, uint32_t seed, uint32_t rowStep,
                         bool hazard = false) {
    BoundaryFrame b = MakeBoundaryFrame(w, h, tile, seed, rowStep, hazard);
    ASSERT_GT(b.sites, hazard ? 40u : 100u) << "the fixture placed too few boundary pixels to mean anything";
    const XpeImageBuffer img = Wrap(b.px, w, h);
    const std::vector<uint8_t> rows = RowsMap(img, b.cfg);
    const std::vector<uint8_t> ref = ScalarMap(img, b.cfg);
    size_t first = 0;
    const size_t d = Differing(rows, ref, &first);
    EXPECT_EQ(0u, d) << w << "x" << h << " tile " << tile << ": " << d
                     << " pixels differ from the scalar rule; first at row " << first / w
                     << " col " << first % w;
    // Non-vacuous, counted AT THE BOUNDARY PIXELS ONLY: the -2..+2 ulp offsets put
    // about three fifths of them on the flagged side and two fifths on the other,
    // so a wrong sigma has both directions to flip in. (Counting flags over the
    // whole frame would be meaningless: the synthetic tile table does not match the
    // frame's real noise, so ordinary pixels are flagged too.)
    size_t flaggedAtSites = 0;
    for (size_t idx : b.siteIdx) flaggedAtSites += ref[idx];
    EXPECT_GT(flaggedAtSites, b.sites / 5u);
    EXPECT_LT(flaggedAtSites, (b.sites * 4u) / 5u);
}

}  // namespace

// A tile of 8 or more: the straddling run stays vector. 128 is the shipped size;
// 100 is not a multiple of 8, so runs straddle at every offset; 8 is the smallest
// tile the vector path takes.
TEST(TileLanesTest, StraddlingRunsMatchTheScalarRule_Tile128) { ExpectMatchesScalar(1024u, 1024u, 128u, 20260932u, 3u); }
TEST(TileLanesTest, StraddlingRunsMatchTheScalarRule_Tile100) { ExpectMatchesScalar(1000u, 400u, 100u, 20260931u, 3u); }
TEST(TileLanesTest, StraddlingRunsMatchTheScalarRule_Tile64OddSize) { ExpectMatchesScalar(517u, 203u, 64u, 20260933u, 3u); }
TEST(TileLanesTest, StraddlingRunsMatchTheScalarRule_Tile16) { ExpectMatchesScalar(640u, 240u, 16u, 20260934u, 3u); }
TEST(TileLanesTest, StraddlingRunsMatchTheScalarRule_Tile8) { ExpectMatchesScalar(400u, 200u, 8u, 20260935u, 3u); }

// HAZARD PIXELS. The tests above put pixels at the scalar decision threshold, but
// that only exposes the last-place difference between the two sigma expressions
// when a pixel happens to fall in the gap between the two thresholds -- about one
// site in four hundred. Reverting to the vector association passed four of those
// five tests. These build the gap on purpose: every site is a neighbourhood and a
// pixel value where the scalar rule and the old vector expression DECIDE
// DIFFERENTLY, so using the wrong association flips all of them, not some. The
// shipped path must still agree with the scalar rule at every one.
#if XPE_DETECT_HAS_AVX2
TEST(TileLanesTest, HazardPixelsAreDecidedByTheScalarRule_Tile128) { ExpectMatchesScalar(1024u, 512u, 128u, 20260941u, 9u, true); }
TEST(TileLanesTest, HazardPixelsAreDecidedByTheScalarRule_Tile100) { ExpectMatchesScalar(1000u, 400u, 100u, 20260942u, 9u, true); }
TEST(TileLanesTest, HazardPixelsAreDecidedByTheScalarRule_Tile64OddSize) { ExpectMatchesScalar(517u, 203u, 64u, 20260943u, 6u, true); }
TEST(TileLanesTest, HazardPixelsAreDecidedByTheScalarRule_Tile16) { ExpectMatchesScalar(640u, 240u, 16u, 20260944u, 6u, true); }
TEST(TileLanesTest, HazardPixelsAreDecidedByTheScalarRule_Tile8) { ExpectMatchesScalar(400u, 200u, 8u, 20260945u, 6u, true); }
#endif

// A tile narrower than 8 columns can put a run across THREE tiles; two references
// do not express that, so those runs must stay on the scalar path. The threshold
// pixels are what give this test teeth: an ordinary frame would agree even if
// someone extended the vector path to narrow tiles carelessly, because a wrong
// reference only changes the outcome for a pixel near the threshold.
TEST(TileLanesTest, TilesNarrowerThanEightStayScalarAndAgree_Tile7) { ExpectMatchesScalar(400u, 200u, 7u, 20260936u, 3u); }
TEST(TileLanesTest, TilesNarrowerThanEightStayScalarAndAgree_Tile4) { ExpectMatchesScalar(400u, 200u, 4u, 20260940u, 3u); }

// No tile table at all: the frame-wide reference only. tileSize is 0 here, which
// the per-lane code must not divide by.
TEST(TileLanesTest, NoTileTableUsesTheFrameReferenceAndAgrees) {
    const uint32_t w = 300u, h = 120u;
    std::mt19937 rng(20260937u);
    std::normal_distribution<float> g(0.0f, 1.0f);
    std::vector<float> px(static_cast<size_t>(w) * h);
    for (float& v : px) v = 3000.0f + 12.0f * g(rng);
    XpeImageBuffer img = Wrap(px, w, h);
    RuntimeDetectionConfig cfg = RuntimeDetection_DefaultConfig();
    cfg.blendWeight = RUNTIME_DETECTION_BLEND_WEIGHT;
    cfg.blendReference = ComputeGlobalSigma(&img);
    EXPECT_EQ(0u, Differing(RowsMap(img, cfg), ScalarMap(img, cfg), nullptr));
}

// The historical rule (blendWeight == 0): floor and cap, no reference at all.
TEST(TileLanesTest, HistoricalRuleIsUnchanged) {
    const uint32_t w = 300u, h = 120u;
    std::mt19937 rng(20260938u);
    std::normal_distribution<float> g(0.0f, 1.0f);
    std::vector<float> px(static_cast<size_t>(w) * h);
    for (float& v : px) v = 3000.0f + 12.0f * g(rng);
    XpeImageBuffer img = Wrap(px, w, h);
    const float sg = ComputeGlobalSigma(&img);
    RuntimeDetectionConfig cfg = RuntimeDetection_DefaultConfig();
    cfg.globalSigmaFloor = RUNTIME_DETECTION_GLOBAL_SIGMA_FLOOR * sg;
    cfg.globalSigmaCap = RUNTIME_DETECTION_GLOBAL_SIGMA_CAP * sg;
    EXPECT_EQ(0u, Differing(RowsMap(img, cfg), ScalarMap(img, cfg), nullptr));
}

// A tile table AND several threads -- the combination the shipped configuration
// has and no earlier test covers. DetectFrame overwrites the cap from the global
// sigma, so this is also a tile table with a cap greater than zero.
TEST(TileLanesTest, ThreadedRunsMatchOneThread) {
    BoundaryFrame b = MakeBoundaryFrame(1024u, 1024u, 128u, 20260939u, 3u);
    ASSERT_GT(b.sites, 100u);
    const XpeImageBuffer img = Wrap(b.px, b.w, b.h);
    RuntimeDetectionConfig cfg = b.cfg;
    cfg.threadCount = 1;
    std::vector<uint8_t> one(b.px.size(), 0u);
    DetectFrame(&img, cfg, one.data(), one.size());
    size_t flagged = 0;
    for (uint8_t v : one) flagged += v;
    ASSERT_GT(flagged, 20u);
    for (uint32_t T : {2u, 3u, 5u, 20u}) {
        cfg.threadCount = T;
        std::vector<uint8_t> many(b.px.size(), 0u);
        DetectFrame(&img, cfg, many.data(), many.size());
        size_t first = 0;
        EXPECT_EQ(0u, Differing(many, one, &first)) << "threads " << T << ", first at row "
                                                    << first / b.w << " col " << first % b.w;
    }
}
