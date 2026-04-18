/**
 * @file test_readout_validate.cpp
 * @brief TDD RED tests for SWU-1.9: xpe_validate_readout_artifact (REQ-P1A-001 to REQ-P1A-004)
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include <gtest/gtest.h>
#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <vector>
#include <cstring>

namespace {

class ReadoutValidateTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 64;
    static constexpr uint32_t H = 64;

    std::vector<uint16_t> pixels;
    XpeImageBuffer img{};
    int32_t score{-1};
    char    msg[256]{};

    void SetUp() override {
        pixels.assign(W * H, 32768); // mid-range clean image
        img.data          = pixels.data();
        img.width         = W;
        img.height        = H;
        img.bitsAllocated = 16;
        img.bitsStored    = 16;
        img.format        = XPE_PIXEL_UINT16;
        img.dataSize      = pixels.size() * sizeof(uint16_t);
    }
};

// REQ-P1A-001: clean image returns XPE_OK with score == 0
TEST_F(ReadoutValidateTest, CleanImageReturnsZeroScore) {
    ASSERT_EQ(XPE_OK, xpe_validate_readout_artifact(&img, &score, msg, sizeof(msg)));
    EXPECT_EQ(0, score);
}

// REQ-P1A-004: score > 80 still returns XPE_OK (warning, not error)
TEST_F(ReadoutValidateTest, HighScoreStillReturnsOk) {
    // Simulate severely noisy line: fill entire row 0 with max uint16
    for (uint32_t x = 0; x < W; ++x) pixels[x] = 65535;
    XpeErrorCode ret = xpe_validate_readout_artifact(&img, &score, msg, sizeof(msg));
    EXPECT_EQ(XPE_OK, ret); // must still be XPE_OK
    EXPECT_GT(score, 0);    // score should indicate artifact
}

// NULL rawImg returns error
TEST_F(ReadoutValidateTest, NullImageReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_validate_readout_artifact(nullptr, &score, msg, sizeof(msg)));
}

// NULL artifactScoreOut returns error
TEST_F(ReadoutValidateTest, NullScoreOutReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_validate_readout_artifact(&img, nullptr, msg, sizeof(msg)));
}

// NULL msgOut is acceptable (optional buffer)
TEST_F(ReadoutValidateTest, NullMsgOutIsAccepted) {
    EXPECT_EQ(XPE_OK,
              xpe_validate_readout_artifact(&img, &score, nullptr, 0));
}

// ADC saturation: all pixels at 65535 -> high score
TEST_F(ReadoutValidateTest, AllSaturatedPixelsHighScore) {
    std::fill(pixels.begin(), pixels.end(), 65535u);
    ASSERT_EQ(XPE_OK, xpe_validate_readout_artifact(&img, &score, msg, sizeof(msg)));
    EXPECT_GE(score, 80); // should be severely corrupted
}

} // namespace
