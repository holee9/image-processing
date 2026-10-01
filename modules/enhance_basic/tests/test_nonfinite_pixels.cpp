/**
 * @file test_nonfinite_pixels.cpp
 * @brief A module neither makes a non-finite pixel from finite input nor reports success on one (QA-B-181f, #233).
 *
 * QA-B-181e measured that a non-finite pixel went through xpe_contrast_enhance with rc=0 and spread to the whole
 * image, and that ONE finite pixel (100) became +inf in xpe_log_inverse, which the next stage then spread. The rule
 * (leader decision): the consumer refuses non-finite input (XPE_ERR_INVALID_INPUT, image untouched); the producer
 * refuses a request whose result would not be finite (image untouched). Zeroing a bad pixel is not chosen: it would
 * make an upstream fault look like a normal image.
 *
 * Inputs here are chosen so that a range comparison does NOT stop them in either floating-point mode: +inf passes
 * every `x <= 0` / `x > hi` style test, and a NaN normFactor is refused by `normFactor <= 0` only when the module is
 * built with /fp:fast (the shipped build) and accepted under /fp:precise.
 */
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include "xpe/enhance_basic/enhance_basic_api.h"

namespace {

constexpr float kInf = std::numeric_limits<float>::infinity();
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

std::vector<float> Ramp(int w, int h, float scale) {
    std::vector<float> px(static_cast<size_t>(w) * h);
    uint32_t s = 17;
    for (size_t i = 0; i < px.size(); ++i) {
        s = s * 1664525u + 1013904223u;
        px[i] = static_cast<float>(s >> 24) * scale + static_cast<float>(i % 13);
    }
    return px;
}

XpeImageBuffer Wrap(std::vector<float>& px, int w, int h) {
    XpeImageBuffer b{};
    b.width = static_cast<uint32_t>(w);
    b.height = static_cast<uint32_t>(h);
    b.format = XPE_PIXEL_FLOAT32;
    b.bitsAllocated = 32;
    b.bitsStored = 32;
    b.data = px.data();
    b.dataSize = px.size() * sizeof(float);
    return b;
}

bool SameBytes(const std::vector<float>& a, const std::vector<float>& b) {
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}

constexpr int kW = 64, kH = 64;
const float kBad[] = {kNaN, kInf, -kInf};
const char* const kBadName[] = {"NaN", "+inf", "-inf"};

// Run `call` on a ramp whose pixel 10 is `bad`; the answer and whether the pixels were left untouched.
template <class F>
void ExpectRefusedUntouched(F call, float bad, const char* what) {
    std::vector<float> px = Ramp(kW, kH, 0.01f);
    px[10] = bad;
    const std::vector<float> before = px;
    XpeImageBuffer img = Wrap(px, kW, kH);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, call(&img)) << what << ": pixel = " << bad;
    // NaN != NaN, so compare bytes, not values.
    EXPECT_TRUE(SameBytes(before, px)) << what << ": the pixels must be left exactly as they were";
}

}  // namespace

// ---- consumer: xpe_contrast_enhance ---------------------------------------------------------------------------

TEST(NonFinitePixels, ContrastEnhanceRefusesANonFinitePixelAndLeavesTheImageAlone) {
    for (int i = 0; i < 3; ++i) {
        ExpectRefusedUntouched([](XpeImageBuffer* img) {
            XpeClaheParams p{3.0f, 4, 4};
            return xpe_contrast_enhance(img, &p);
        }, kBad[i], kBadName[i]);
    }
}

TEST(NonFinitePixels, ContrastEnhanceStillProcessesLargeFinitePixels) {
    std::vector<float> px = Ramp(kW, kH, 0.01f);
    px[10] = 3.0e38f;
    XpeImageBuffer img = Wrap(px, kW, kH);
    XpeClaheParams p{3.0f, 4, 4};
    EXPECT_EQ(XPE_OK, xpe_contrast_enhance(&img, &p)) << "a huge but finite pixel is data, not an error";
}

TEST(NonFinitePixels, ContrastEnhanceRefusesFinitePixelsWhoseRangeLeavesFloat) {
    // QA-B-181e measured this: -3e38 and 3e38 are both finite, but max - min is +inf, and the whole output came out
    // non-finite with rc=0. A finite input that cannot be processed finitely is refused, image untouched.
    std::vector<float> px = Ramp(kW, kH, 0.01f);
    px[3] = -3.0e38f;
    px[10] = 3.0e38f;
    const std::vector<float> before = px;
    XpeImageBuffer img = Wrap(px, kW, kH);
    XpeClaheParams p{3.0f, 4, 4};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_contrast_enhance(&img, &p));
    EXPECT_TRUE(SameBytes(before, px));
}

// ---- xpe_log_inverse: producer and consumer -------------------------------------------------------------------

TEST(NonFinitePixels, LogInverseRefusesANonFinitePixel) {
    for (int i = 0; i < 3; ++i) {
        ExpectRefusedUntouched([](XpeImageBuffer* img) { return xpe_log_inverse(img, 1.0f); }, kBad[i], kBadName[i]);
    }
}

TEST(NonFinitePixels, LogInverseRefusesANonFiniteNormFactor) {
    // NaN is refused by `normFactor <= 0` only under /fp:fast; +inf passes that test in every mode and used to turn
    // the whole image into zeros (exp(0) - 1) with rc=0.
    for (float nf : {kNaN, kInf}) {
        std::vector<float> px = Ramp(kW, kH, 0.01f);
        const std::vector<float> before = px;
        XpeImageBuffer img = Wrap(px, kW, kH);
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_log_inverse(&img, nf)) << "normFactor = " << nf;
        EXPECT_TRUE(SameBytes(before, px)) << "normFactor = " << nf;
    }
}

TEST(NonFinitePixels, LogInverseRefusesAFinitePixelWhoseResultWouldNotBeFinite) {
    // exp(pixel / normFactor * ln 10) leaves float above about 88.7: pixel 100 at normFactor 1 is the 181e case.
    for (float v : {100.0f, 1000.0f, 38.5f / 0.434294f * 1.0f + 5.0f}) {
        std::vector<float> px = Ramp(kW, kH, 0.01f);
        px[10] = v;
        const std::vector<float> before = px;
        XpeImageBuffer img = Wrap(px, kW, kH);
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_log_inverse(&img, 1.0f)) << "pixel = " << v;
        EXPECT_TRUE(SameBytes(before, px)) << "pixel = " << v << ": no half-converted image may be left behind";
    }
}

TEST(NonFinitePixels, LogInverseStillConvertsAnOrdinaryImage) {
    std::vector<float> px = Ramp(kW, kH, 0.01f);
    px[10] = 38.0f;   // 38 * ln 10 = 87.5: the largest value the float range still holds at normFactor 1
    XpeImageBuffer img = Wrap(px, kW, kH);
    ASSERT_EQ(XPE_OK, xpe_log_inverse(&img, 1.0f));
    for (float v : px) ASSERT_TRUE(std::isfinite(v));
}

TEST(NonFinitePixels, TheFinitePixelThatBecameInfinityNowStopsAtTheFirstStage) {
    // The 181e chain: one finite pixel -> xpe_log_inverse -> +inf -> xpe_contrast_enhance spread it with rc=0.
    std::vector<float> px = Ramp(kW, kH, 0.01f);
    px[10] = 100.0f;
    XpeImageBuffer img = Wrap(px, kW, kH);
    EXPECT_NE(XPE_OK, xpe_log_inverse(&img, 1.0f));
    for (float v : px) ASSERT_TRUE(std::isfinite(v)) << "the first stage must not hand a non-finite pixel on";
}

// ---- xpe_log_transform: producer and consumer -----------------------------------------------------------------

TEST(NonFinitePixels, LogTransformRefusesANonFinitePixel) {
    for (int i = 0; i < 3; ++i) {
        ExpectRefusedUntouched([](XpeImageBuffer* img) { return xpe_log_transform(img, 1.0f); }, kBad[i], kBadName[i]);
    }
}

TEST(NonFinitePixels, LogTransformRefusesANonFiniteNormFactor) {
    for (float nf : {kNaN, kInf}) {
        std::vector<float> px = Ramp(kW, kH, 0.01f);
        const std::vector<float> before = px;
        XpeImageBuffer img = Wrap(px, kW, kH);
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_log_transform(&img, nf)) << "normFactor = " << nf;
        EXPECT_TRUE(SameBytes(before, px)) << "normFactor = " << nf;
    }
}

TEST(NonFinitePixels, LogTransformRefusesAFiniteRequestWhoseResultWouldNotBeFinite) {
    // normFactor * log10(1 + pixel) with a finite but enormous normFactor leaves float.
    std::vector<float> px = Ramp(kW, kH, 100.0f);
    const std::vector<float> before = px;
    XpeImageBuffer img = Wrap(px, kW, kH);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_log_transform(&img, 3.0e38f));
    EXPECT_TRUE(SameBytes(before, px));
}

TEST(NonFinitePixels, LogTransformStillConvertsAnOrdinaryImage) {
    std::vector<float> px = Ramp(kW, kH, 100.0f);
    XpeImageBuffer img = Wrap(px, kW, kH);
    ASSERT_EQ(XPE_OK, xpe_log_transform(&img, 1.0f));
    for (float v : px) ASSERT_TRUE(std::isfinite(v));
}

// ---- the producer side of xpe_noise_reduce: finite image + finite, valid parameters must give a finite image ------

namespace {

// A flat area (equal neighbours: difference 0) next to an edge, so both `diff == 0` and `diff != 0` occur.
std::vector<float> FlatAndEdge() {
    std::vector<float> px(static_cast<size_t>(kW) * kH, 100.0f);
    for (int y = 0; y < kH; ++y) {
        for (int x = kW / 2; x < kW; ++x) px[static_cast<size_t>(y) * kW + x] = 900.0f;
    }
    return px;
}

}  // namespace

TEST(NonFinitePixels, NoiseReduceNeverMakesANonFinitePixelFromFiniteInputAndParameters) {
    // sigma_range / h_param so small that 1 / sigma^2 leaves float: the weight exp(-diff^2 * inf) is exp(-0 * inf) =
    // NaN where the neighbours are equal. Whatever the module answers must be finite or a refusal.
    const float tiny[] = {1.0f, 1.0e-10f, 1.0e-20f, 1.0e-30f, 1.0e-38f};
    for (float s : tiny) {
        {
            std::vector<float> px = FlatAndEdge();
            XpeImageBuffer img = Wrap(px, kW, kH);
            XpeNoiseReduceParams p{};
            p.mode = XPE_NOISE_BILATERAL;
            p.sigma_space = 2.0f;
            p.sigma_range = s;
            p.search_window = 21;
            p.patch_size = 7;
            p.h_param = 10.0f;
            const XpeErrorCode rc = xpe_noise_reduce(&img, &p);
            if (s == 1.0f) ASSERT_EQ(XPE_OK, rc) << "control: an ordinary sigma_range is accepted";
            if (rc == XPE_OK) {
                for (float v : px) ASSERT_TRUE(std::isfinite(v)) << "bilateral sigma_range = " << s;
            }
        }
        {
            std::vector<float> px = FlatAndEdge();
            XpeImageBuffer img = Wrap(px, kW, kH);
            XpeNoiseReduceParams p{};
            p.mode = XPE_NOISE_NLM;
            p.sigma_space = 2.0f;
            p.sigma_range = 10.0f;
            p.search_window = 11;
            p.patch_size = 3;
            p.h_param = s;
            const XpeErrorCode rc = xpe_noise_reduce(&img, &p);
            if (s == 1.0f) ASSERT_EQ(XPE_OK, rc) << "control: an ordinary h_param is accepted";
            if (rc == XPE_OK) {
                for (float v : px) ASSERT_TRUE(std::isfinite(v)) << "nlm h_param = " << s;
            }
        }
    }
}
