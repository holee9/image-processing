/**
 * @file test_voi_auto_window.cpp
 * @brief Tests of xpe_voi_auto_window (QA-B-214, user decision on #251): the anatomy-based automatic VOI window.
 * SPEC: SPEC-XPE-P1B-DISP
 *
 * The images here are built from a fixed pseudo-random generator so that the expected window can be stated from the
 * construction (class boundaries and quantiles of the generating distributions), not from the function under test.
 * The real-frame behaviour is measured by the QA-B-214 harness (tests/e2e_post_pipeline/test_m2_measure.cpp), not here.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "xpe/common/xpe_error.h"
#include "xpe/display/display_api.h"

namespace {

// Deterministic uniform [0,1) stream (64-bit LCG, the high bits).
struct Lcg {
    uint64_t s;
    explicit Lcg(uint64_t seed) : s(seed) {}
    double next() {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<double>(s >> 11) / 9007199254740992.0;
    }
};

XpeImageBuffer make_image(uint32_t w, uint32_t h, const std::vector<float>& px) {
    XpeImageBuffer img{};
    img.width = w;
    img.height = h;
    img.format = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32;
    img.bitsStored = 32;
    img.dataSize = px.size() * sizeof(float);
    img.data = std::malloc(img.dataSize);
    std::memcpy(img.data, px.data(), img.dataSize);
    return img;
}

void free_image(XpeImageBuffer& img) {
    std::free(img.data);
    img.data = nullptr;
}

// Anatomy fraction `anat` uniform in [aLo, aHi]; `transition` fraction uniform in [tLo, tHi]; the rest background in [bLo, bHi].
std::vector<float> three_population(size_t n, uint64_t seed, double anat, double aLo, double aHi, double transition,
                                    double tLo, double tHi, double bLo, double bHi) {
    Lcg g(seed);
    std::vector<float> v(n);
    for (size_t i = 0; i < n; ++i) {
        const double c = g.next(), u = g.next();
        if (c < anat) v[i] = static_cast<float>(aLo + (aHi - aLo) * u);
        else if (c < anat + transition) v[i] = static_cast<float>(tLo + (tHi - tLo) * u);
        else v[i] = static_cast<float>(bLo + (bHi - bLo) * u);
    }
    return v;
}

constexpr uint32_t kSide = 512;

XpeVoiLutParams sentinel() {
    XpeVoiLutParams p{};
    p.mode = static_cast<XpeVoiLutMode>(99);
    p.center = -7.0f;
    p.width = -7.0f;
    p.minOut = -7.0f;
    p.maxOut = -7.0f;
    return p;
}

bool untouched(const XpeVoiLutParams& p) {
    return p.mode == static_cast<XpeVoiLutMode>(99) && p.center == -7.0f && p.width == -7.0f && p.minOut == -7.0f &&
           p.maxOut == -7.0f;
}

int alert_count_containing(const char* needle) {
    int hits = 0;
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < n; ++i) {
        char msg[512] = {};
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, msg, sizeof msg, &sev) == XPE_OK && std::strstr(msg, needle)) ++hits;
    }
    return hits;
}

}  // namespace

// A detector-like frame: 15 % anatomy spread over [1000, 2000], 85 % background in [2900, 3000].
// The window must be the anatomy window: low = 0.5 % quantile of the anatomy (1005), high = 5 % quantile of the background
// (2905). The whole-image 1 %..99 % window of the same data is [~1067, ~2999] -- far enough away that the two cannot be mixed up.
TEST(VoiAutoWindow, BimodalImageGivesTheAnatomyWindow) {
    const auto px = three_population(kSide * kSide, 11, 0.15, 1000, 2000, 0.0, 0, 0, 2900, 3000);
    XpeImageBuffer img = make_image(kSide, kSide, px);
    XpeVoiLutParams out = sentinel();
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_voi_auto_window(&img, &out));
    const double lo = out.center - out.width / 2.0, hi = out.center + out.width / 2.0;
    EXPECT_EQ(XPE_VOI_LINEAR_EXACT, out.mode);
    EXPECT_FLOAT_EQ(0.0f, out.minOut);
    EXPECT_FLOAT_EQ(1.0f, out.maxOut);
    EXPECT_NEAR(1005.0, lo, 10.0) << "low end: 0.5 % quantile of the anatomy class";
    EXPECT_NEAR(2905.0, hi, 6.0) << "high end: 5 % quantile of the background class";
    EXPECT_EQ(0, alert_count_containing("VOI auto window")) << "no fallback alert on a two-class image";
    free_image(img);
}

TEST(VoiAutoWindow, WindowEndsMapToTheEndsOfTheOutputRange) {
    const auto px = three_population(kSide * kSide, 12, 0.15, 1000, 2000, 0.0, 0, 0, 2900, 3000);
    XpeImageBuffer img = make_image(kSide, kSide, px);
    XpeVoiLutParams win = sentinel();
    ASSERT_EQ(XPE_OK, xpe_voi_auto_window(&img, &win));
    free_image(img);
    const float lo = win.center - win.width / 2.0f, hi = win.center + win.width / 2.0f;
    XpeImageBuffer probe = make_image(4, 1, {lo - 100.0f, lo, hi, hi + 100.0f});
    ASSERT_EQ(XPE_OK, xpe_apply_voi_lut(&probe, &win));
    const float* o = static_cast<const float*>(probe.data);
    EXPECT_NEAR(0.0f, o[0], 1e-6f);
    EXPECT_NEAR(0.0f, o[1], 1e-4f);
    EXPECT_NEAR(1.0f, o[2], 1e-4f);
    EXPECT_NEAR(1.0f, o[3], 1e-6f);
    free_image(probe);
}

// The tissue-to-air transition at the skin line (here 5 % of the pixels, spread over [2000, 2900]) must stay inside the window.
// Cutting the window at the Otsu threshold -- the first design -- clipped the whole transition.
TEST(VoiAutoWindow, TissueToAirTransitionStaysInsideTheWindow) {
    const auto px = three_population(kSide * kSide, 13, 0.15, 1000, 2000, 0.05, 2000, 2900, 2900, 3000);
    XpeImageBuffer img = make_image(kSide, kSide, px);
    XpeVoiLutParams out = sentinel();
    ASSERT_EQ(XPE_OK, xpe_voi_auto_window(&img, &out));
    const double hi = out.center + out.width / 2.0;
    size_t transitionAbove = 0, transition = 0;
    for (const float v : px) {
        if (v >= 2000.0f && v < 2900.0f) {
            ++transition;
            if (v > hi) ++transitionAbove;
        }
    }
    EXPECT_GT(transition, 0u);
    EXPECT_EQ(0u, transitionAbove) << "window high end " << hi << " must lie above the transition (up to 2900)";
    free_image(img);
}

// No two classes: a unimodal (bell-shaped) frame. Otsu separability of a bell is about 0.64, under the 0.75 bound.
TEST(VoiAutoWindow, UnimodalImageFallsBackToTheWholeImageWindowWithAnInfoAlert) {
    Lcg g(14);
    std::vector<float> px(kSide * kSide);
    for (auto& v : px) v = static_cast<float>(1000.0 + 500.0 * (g.next() + g.next() + g.next() + g.next()));  // Irwin-Hall n=4
    std::vector<float> sorted = px;
    std::sort(sorted.begin(), sorted.end());
    const double q01 = sorted[static_cast<size_t>(0.01 * static_cast<double>(sorted.size()))];
    const double q99 = sorted[static_cast<size_t>(0.99 * static_cast<double>(sorted.size()))];
    XpeImageBuffer img = make_image(kSide, kSide, px);
    XpeVoiLutParams out = sentinel();
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_voi_auto_window(&img, &out));
    // The function histograms a 1-in-16 sample, so a 1 % quantile (about 160 sampled pixels here) carries sampling noise of a
    // few bins; 1 % of the range bounds it. The answer if the fallback did NOT happen (the Otsu half-class quantiles) is 20 %
    // or more of the range away from these.
    const double tol = 0.01 * (sorted.back() - sorted.front());
    EXPECT_NEAR(q01, out.center - out.width / 2.0, tol);
    EXPECT_NEAR(q99, out.center + out.width / 2.0, tol);
    EXPECT_EQ(1, alert_count_containing("VOI auto window")) << "one Info alert says the fallback was used";
    xpe_clear_alerts();
    free_image(img);
}

// One class holds under 2 % of the pixels: that is not an anatomy to isolate (dead pixels of a flat field).
TEST(VoiAutoWindow, TinyClassFallsBackToTheWholeImageWindow) {
    const auto px = three_population(kSide * kSide, 15, 0.01, 1000, 2000, 0.0, 0, 0, 2900, 3000);
    XpeImageBuffer img = make_image(kSide, kSide, px);
    XpeVoiLutParams out = sentinel();
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_voi_auto_window(&img, &out));
    EXPECT_EQ(1, alert_count_containing("VOI auto window"));
    // Whole-image 1 %..99 % window: the low end is inside the 1 % anatomy (at its very bottom), the high end is in the background.
    EXPECT_GT(out.center + out.width / 2.0, 2900.0);
    xpe_clear_alerts();
    free_image(img);
}

TEST(VoiAutoWindow, FlatImageGivesCenterAtTheValueAndWidthOne) {
    XpeImageBuffer img = make_image(64, 64, std::vector<float>(64 * 64, 1234.5f));
    XpeVoiLutParams out = sentinel();
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_voi_auto_window(&img, &out));
    EXPECT_FLOAT_EQ(1234.5f, out.center);
    EXPECT_FLOAT_EQ(1.0f, out.width);
    EXPECT_EQ(XPE_VOI_LINEAR_EXACT, out.mode);
    EXPECT_EQ(1, alert_count_containing("VOI auto window"));
    xpe_clear_alerts();
    free_image(img);
}

TEST(VoiAutoWindow, SameImageGivesTheSameWindowTwice) {
    const auto px = three_population(kSide * kSide, 16, 0.15, 1000, 2000, 0.05, 2000, 2900, 2900, 3000);
    XpeImageBuffer img = make_image(kSide, kSide, px);
    XpeVoiLutParams a = sentinel(), b = sentinel();
    ASSERT_EQ(XPE_OK, xpe_voi_auto_window(&img, &a));
    ASSERT_EQ(XPE_OK, xpe_voi_auto_window(&img, &b));
    EXPECT_EQ(0, std::memcmp(&a, &b, sizeof a));
    free_image(img);
}

TEST(VoiAutoWindow, RejectsBadArgumentsAndLeavesOutParamsUntouched) {
    const auto px = three_population(64 * 64, 17, 0.15, 1000, 2000, 0.0, 0, 0, 2900, 3000);
    XpeImageBuffer good = make_image(64, 64, px);
    XpeVoiLutParams out = sentinel();

    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_voi_auto_window(nullptr, &out));
    EXPECT_TRUE(untouched(out));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_voi_auto_window(&good, nullptr));

    XpeImageBuffer empty = good;
    empty.width = 0;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_voi_auto_window(&empty, &out));
    EXPECT_TRUE(untouched(out));

    XpeImageBuffer nodata = good;
    nodata.data = nullptr;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_voi_auto_window(&nodata, &out));
    EXPECT_TRUE(untouched(out));

    XpeImageBuffer u16 = good;
    u16.format = XPE_PIXEL_UINT16;
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, xpe_voi_auto_window(&u16, &out));
    EXPECT_TRUE(untouched(out));

    XpeImageBuffer shortSize = good;
    shortSize.dataSize = good.dataSize - 4;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_voi_auto_window(&shortSize, &out));
    EXPECT_TRUE(untouched(out));

    free_image(good);
}

// A non-finite pixel is refused wherever it is (the check is a full pass, not only over the histogram sample): pixel [1] is
// not on the 1-in-4 sampling grid.
TEST(VoiAutoWindow, NonFinitePixelAnywhereIsRefused) {
    for (const float bad : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                            -std::numeric_limits<float>::infinity()}) {
        auto px = three_population(64 * 64, 18, 0.15, 1000, 2000, 0.0, 0, 0, 2900, 3000);
        px[1] = bad;
        XpeImageBuffer img = make_image(64, 64, px);
        XpeVoiLutParams out = sentinel();
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_voi_auto_window(&img, &out));
        EXPECT_TRUE(untouched(out));
        free_image(img);
    }
}


// =====================================================================================================================
// QA-B-214b (Codex #167): API boundaries of the automatic window.
// =====================================================================================================================

namespace {

// Grid pixels of the 1-in-4 sampling of a large image: x % 4 == 0 and y % 4 == 0.
bool on_sample_grid(size_t x, size_t y) { return x % 4 == 0 && y % 4 == 0; }

}  // namespace

// Finding 3, the exact Codex reproduction: 8 x 8, only the four pixels a 1-in-4 sample would take hold the background (3000),
// the other 60 are anatomy in [1000, 2000]. The first design histogrammed the four background points and returned a
// one-bin window around 3000 with every anatomy pixel clipped.
TEST(VoiAutoWindow, CodexReproductionEightByEightKeepsTheAnatomyInsideTheWindow) {
    std::vector<float> px(64);
    Lcg g(21);
    for (uint32_t y = 0; y < 8; ++y) {
        for (uint32_t x = 0; x < 8; ++x) {
            px[y * 8 + x] = ((x % 4 == 0) && (y % 4 == 0)) ? 3000.0f : static_cast<float>(1000.0 + 1000.0 * g.next());
        }
    }
    float anatMin = 1e30f, anatMax = -1e30f;
    for (uint32_t i = 0; i < 64; ++i) {
        if (px[i] != 3000.0f) {
            anatMin = std::min(anatMin, px[i]);
            anatMax = std::max(anatMax, px[i]);
        }
    }
    XpeImageBuffer img = make_image(8, 8, px);
    XpeVoiLutParams out = sentinel();
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_voi_auto_window(&img, &out));
    const double lo = out.center - out.width / 2.0, hi = out.center + out.width / 2.0;
    EXPECT_LE(lo, anatMin + 3.0) << "the window starts at the anatomy";
    EXPECT_GE(hi, anatMax) << "the window ends above the anatomy (it was a one-bin window around 3000 before)";
    EXPECT_GT(out.width, 1000.0f);
    // Whether this 6 % background and broad anatomy pass the two-class test (Otsu separability about 0.63 here, under 0.75, so it
    // takes the whole-image 1..99 % fallback and posts the Info alert) is not what the finding is about: either way the window
    // is made of all 64 pixels and holds the anatomy, which is what is asserted above.
    xpe_clear_alerts();
    free_image(img);
}

// Finding 3, two classes OUTSIDE the sampling grid in a LARGE image (1280 x 1280 = 1.6 M pixels, sampled 1 in 16): every grid
// pixel is background 3000, every other pixel is anatomy [1000, 2000] (85 %) or background [2900, 3000] (15 %). The sample alone
// is a flat image; the window must still be the one the whole image gives.
TEST(VoiAutoWindow, TwoClassesOffTheSampleGridInALargeImageAreStillFound) {
    constexpr uint32_t kN = 1280;
    Lcg g(22);
    std::vector<float> px(static_cast<size_t>(kN) * kN);
    for (uint32_t y = 0; y < kN; ++y) {
        for (uint32_t x = 0; x < kN; ++x) {
            const double c = g.next(), u = g.next();
            float v;
            if (on_sample_grid(x, y)) v = 3000.0f;
            else if (c < 0.85) v = static_cast<float>(1000.0 + 1000.0 * u);
            else v = static_cast<float>(2900.0 + 100.0 * u);
            px[static_cast<size_t>(y) * kN + x] = v;
        }
    }
    XpeImageBuffer img = make_image(kN, kN, px);
    XpeVoiLutParams out = sentinel();
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_voi_auto_window(&img, &out));
    const double lo = out.center - out.width / 2.0, hi = out.center + out.width / 2.0;
    EXPECT_NEAR(1005.0, lo, 12.0) << "anatomy class low end (0.5 % quantile)";
    EXPECT_NEAR(2907.0, hi, 14.0) << "background class 5 % quantile (the grid pixels at 3000 are part of the class)";
    EXPECT_EQ(0, alert_count_containing("VOI auto window"));
    free_image(img);
}

// Finding 3, "fallback is the real whole-image quantiles": the sampled pixels alone form a bell (no two classes, so the sample
// says fallback), the other 15/16 of the image is clearly two-class. A fallback decided on the sample would be wrong.
TEST(VoiAutoWindow, FallbackOfTheSampleIsConfirmedOnTheWholeImage) {
    constexpr uint32_t kN = 1280;
    Lcg g(23);
    std::vector<float> px(static_cast<size_t>(kN) * kN);
    for (uint32_t y = 0; y < kN; ++y) {
        for (uint32_t x = 0; x < kN; ++x) {
            float v;
            if (on_sample_grid(x, y)) {
                // a bell over [800, 3200], wide enough that every off-grid value lies INSIDE the range the sample spans: the
                // representativeness check stays quiet and only the confirmation of the fallback on the whole image can save it
                v = static_cast<float>(800.0 + 600.0 * (g.next() + g.next() + g.next() + g.next()));
            } else if (g.next() < 0.15) {
                v = static_cast<float>(1000.0 + 1000.0 * g.next());
            } else {
                v = static_cast<float>(2900.0 + 100.0 * g.next());
            }
            px[static_cast<size_t>(y) * kN + x] = v;
        }
    }
    XpeImageBuffer img = make_image(kN, kN, px);
    XpeVoiLutParams out = sentinel();
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_voi_auto_window(&img, &out));
    const double lo = out.center - out.width / 2.0, hi = out.center + out.width / 2.0;
    EXPECT_NEAR(1005.0, lo, 25.0) << "the anatomy window of the whole image, not the 1..99 % range of the sample";
    EXPECT_NEAR(2905.0, hi, 20.0);
    EXPECT_EQ(0, alert_count_containing("VOI auto window")) << "no fallback: the whole image has two classes";
    free_image(img);
}

// Finding 3, a sample that DOES show two classes but misses the lower part of the anatomy: the grid pixels hold anatomy only in
// [1500, 2000], the other pixels hold it in [1000, 2000]. Both classes are present in the sample (no fallback), so only the
// representativeness check (the pixels below the sample's own minimum are 7 % of the image) can send it to the full histogram.
TEST(VoiAutoWindow, SampleThatMissesTheLowerPartOfTheAnatomyIsNotTrusted) {
    constexpr uint32_t kN = 1280;
    Lcg g(27);
    std::vector<float> px(static_cast<size_t>(kN) * kN);
    for (uint32_t y = 0; y < kN; ++y) {
        for (uint32_t x = 0; x < kN; ++x) {
            const bool anatomy = g.next() < 0.15;
            const double u = g.next();
            float v;
            if (anatomy) v = on_sample_grid(x, y) ? static_cast<float>(1500.0 + 500.0 * u) : static_cast<float>(1000.0 + 1000.0 * u);
            else v = static_cast<float>(2900.0 + 100.0 * u);
            px[static_cast<size_t>(y) * kN + x] = v;
        }
    }
    XpeImageBuffer img = make_image(kN, kN, px);
    XpeVoiLutParams out = sentinel();
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_voi_auto_window(&img, &out));
    const double lo = out.center - out.width / 2.0;
    EXPECT_LT(lo, 1030.0) << "the window starts at the anatomy of the whole image (about 1005), not at the sample's 1500";
    EXPECT_EQ(0, alert_count_containing("VOI auto window"));
    free_image(img);
}

// Finding 3, the fallback window of a LARGE unimodal image is made of the real whole-image 1 % and 99 % quantiles.
TEST(VoiAutoWindow, FallbackWindowOfALargeImageIsTheWholeImageQuantileRange) {
    constexpr uint32_t kN = 1280;
    Lcg g(24);
    std::vector<float> px(static_cast<size_t>(kN) * kN);
    for (auto& v : px) v = static_cast<float>(1000.0 + 500.0 * (g.next() + g.next() + g.next() + g.next()));
    std::vector<float> sorted = px;
    std::sort(sorted.begin(), sorted.end());
    const double q01 = sorted[sorted.size() / 100], q99 = sorted[sorted.size() * 99 / 100];
    const double bin = (sorted.back() - sorted.front()) / 1024.0;
    XpeImageBuffer img = make_image(kN, kN, px);
    XpeVoiLutParams out = sentinel();
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_voi_auto_window(&img, &out));
    EXPECT_NEAR(q01, out.center - out.width / 2.0, 1.5 * bin) << "within a bin and a half of the exact 1 % quantile";
    EXPECT_NEAR(q99, out.center + out.width / 2.0, 1.5 * bin);
    EXPECT_EQ(1, alert_count_containing("VOI auto window"));
    xpe_clear_alerts();
    free_image(img);
}

// Finding 4: a window that float cannot hold is an error with the output untouched, never an OK with an infinite width.
// Two finite classes at -FLT_MAX and +FLT_MAX span 2 * FLT_MAX.
TEST(VoiAutoWindow, ClassesAtPlusAndMinusFltMaxAreRefusedNotReturnedWithInfiniteWidth) {
    Lcg g(25);
    std::vector<float> px(64 * 64);
    for (auto& v : px) v = g.next() < 0.15 ? -FLT_MAX : FLT_MAX;
    XpeImageBuffer img = make_image(64, 64, px);
    XpeVoiLutParams out = sentinel();
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_voi_auto_window(&img, &out));
    EXPECT_TRUE(untouched(out));
    free_image(img);
}

// The invariant for every extreme finite input: OK means a finite center and a finite positive width; anything else is an error
// with the output untouched. (Which of the two a given image gets is not the point here, the invariant is.)
TEST(VoiAutoWindow, ExtremeFiniteRangesNeverGiveANonFiniteOrNonPositiveWindow) {
    struct Case { const char* name; float low; float high; };
    const Case cases[] = {
        {"-FLT_MAX .. +FLT_MAX", -FLT_MAX, FLT_MAX},
        {"0 .. FLT_MAX", 0.0f, FLT_MAX},
        {"-FLT_MAX .. 0", -FLT_MAX, 0.0f},
        {"FLT_MAX/2 .. FLT_MAX", FLT_MAX / 2.0f, FLT_MAX},
        {"one denormal step 0 .. 1.4e-45", 0.0f, std::numeric_limits<float>::denorm_min()},
        {"adjacent floats at 1.0", 1.0f, std::nextafter(1.0f, 2.0f)},
        {"adjacent floats at FLT_MAX", std::nextafter(FLT_MAX, 0.0f), FLT_MAX},
        {"tiny normal range 1e-30 .. 2e-30", 1e-30f, 2e-30f},
    };
    for (const Case& c : cases) {
        for (const double anatFraction : {0.15, 0.5}) {
            Lcg g(26);
            std::vector<float> px(64 * 64);
            for (auto& v : px) v = g.next() < anatFraction ? c.low : c.high;
            XpeImageBuffer img = make_image(64, 64, px);
            XpeVoiLutParams out = sentinel();
            const XpeErrorCode rc = xpe_voi_auto_window(&img, &out);
            SCOPED_TRACE(std::string(c.name) + " anatomy fraction " + std::to_string(anatFraction));
            if (rc == XPE_OK) {
                EXPECT_TRUE(std::isfinite(out.center)) << "center";
                EXPECT_TRUE(std::isfinite(out.width)) << "width";
                EXPECT_GT(out.width, 0.0f);
                EXPECT_EQ(XPE_VOI_LINEAR_EXACT, out.mode);
            } else {
                EXPECT_EQ(XPE_ERR_INVALID_INPUT, rc);
                EXPECT_TRUE(untouched(out)) << "an error leaves the output untouched";
            }
            free_image(img);
        }
    }
}

// A flat image of an extreme finite value is still a window (center = the value, width 1).
TEST(VoiAutoWindow, FlatImageOfAnExtremeValueIsStillAWindow) {
    for (const float v : {FLT_MAX, -FLT_MAX}) {
        XpeImageBuffer img = make_image(16, 16, std::vector<float>(256, v));
        XpeVoiLutParams out = sentinel();
        xpe_clear_alerts();
        ASSERT_EQ(XPE_OK, xpe_voi_auto_window(&img, &out));
        EXPECT_FLOAT_EQ(v, out.center);
        EXPECT_FLOAT_EQ(1.0f, out.width);
        xpe_clear_alerts();
        free_image(img);
    }
}
