/**
 * @file test_gain_correct.cpp
 * @brief TDD RED tests for SWU-1.2: xpe_gain_correct (REQ-P1A-016 to REQ-P1A-019)
 *        Validates uint16->float32 domain transition.
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include <gtest/gtest.h>
#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <vector>
#include <cmath>
#include <cstdlib>

namespace {

class GainCorrectTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 4;
    static constexpr uint32_t H = 4;

    std::vector<uint16_t> rawPixels;
    std::vector<float>    gainPixels;
    XpeImageBuffer img{};
    XpeImageBuffer gainMap{};

    void SetUp() override {
        rawPixels.assign(W * H, 2000);
        gainPixels.assign(W * H, 1.5f);

        img.data          = rawPixels.data();
        img.width         = W;
        img.height        = H;
        img.bitsAllocated = 16;
        img.bitsStored    = 16;
        img.format        = XPE_PIXEL_UINT16;
        img.dataSize      = rawPixels.size() * sizeof(uint16_t);

        gainMap.data          = gainPixels.data();
        gainMap.width         = W;
        gainMap.height        = H;
        gainMap.bitsAllocated = 32;
        gainMap.bitsStored    = 32;
        gainMap.format        = XPE_PIXEL_FLOAT32;
        gainMap.dataSize      = gainPixels.size() * sizeof(float);
    }

    void TearDown() override {
        // xpe_gain_correct replaces img.data with a malloc'd float buffer.
        // Free it if the pointer was replaced (ownership transferred to us).
        if (img.data && img.data != rawPixels.data()) {
            std::free(img.data);
            img.data = nullptr;
        }
    }
};

// REQ-P1A-016: corrected[i] = img[i] / gainMap[i] (flat-field normalization)
TEST_F(GainCorrectTest, AppliesGainCorrection) {
    ASSERT_EQ(XPE_OK, xpe_gain_correct(&img, &gainMap));
    const auto* out = static_cast<const float*>(img.data);
    EXPECT_NEAR(2000.0f / 1.5f, out[0], 1e-3f);
}

// REQ-P1A-017: output format must be float32 after conversion
TEST_F(GainCorrectTest, OutputFormatIsFloat32) {
    ASSERT_EQ(XPE_OK, xpe_gain_correct(&img, &gainMap));
    EXPECT_EQ(XPE_PIXEL_FLOAT32, img.format);
}

// NULL checks
TEST_F(GainCorrectTest, NullImgReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_gain_correct(nullptr, &gainMap));
}

TEST_F(GainCorrectTest, NullGainMapReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_gain_correct(&img, nullptr));
}

// Dimension mismatch
TEST_F(GainCorrectTest, DimensionMismatchReturnsError) {
    XpeImageBuffer badGain = gainMap;
    badGain.height = H + 1;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_gain_correct(&img, &badGain));
}

// Unity gain map: pixels converted to float but values unchanged (as float)
TEST_F(GainCorrectTest, UnityGainPreservesValues) {
    std::fill(gainPixels.begin(), gainPixels.end(), 1.0f);
    ASSERT_EQ(XPE_OK, xpe_gain_correct(&img, &gainMap));
    const auto* out = static_cast<const float*>(img.data);
    EXPECT_NEAR(2000.0f, out[0], 1e-3f);
}

// Zero dimensions must be rejected before any buffer access
TEST_F(GainCorrectTest, ZeroWidthReturnsError) {
    img.width = 0;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_gain_correct(&img, &gainMap));
}

// Truncated input buffers must be rejected before any read.
TEST_F(GainCorrectTest, TruncatedImgDataSizeReturnsError) {
    img.dataSize = rawPixels.size() * sizeof(uint16_t) - 1;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_gain_correct(&img, &gainMap));
}

// Truncated gain maps must also be rejected.
TEST_F(GainCorrectTest, TruncatedGainDataSizeReturnsError) {
    gainMap.dataSize = gainPixels.size() * sizeof(float) - 1;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_gain_correct(&img, &gainMap));
}

// Output byte size is updated correctly after conversion.
TEST_F(GainCorrectTest, OutputDataSizeEqualsPixelCountTimesFloat) {
    ASSERT_EQ(XPE_OK, xpe_gain_correct(&img, &gainMap));
    EXPECT_EQ(W * H * sizeof(float), img.dataSize);
}

} // namespace
