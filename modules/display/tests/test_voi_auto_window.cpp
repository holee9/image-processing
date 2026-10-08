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
