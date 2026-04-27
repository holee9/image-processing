/**
 * @file test_ghost_correct.cpp
 * @brief TDD RED tests for SWU-1.4: Ghost/Lag Correction (REQ-P1A-029 to REQ-P1A-034)
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <vector>

namespace {

class GhostCorrectTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 16;
    static constexpr uint32_t H = 16;

    void* handle{nullptr};
    std::vector<float> imgPixels;
    XpeImageBuffer img{};
    XpeImageMetadata meta{};

    void SetUp() override {
        imgPixels.assign(W * H, 1000.0f);
        img.data          = imgPixels.data();
        img.width         = W;
        img.height        = H;
        img.bitsAllocated = 32;
        img.bitsStored    = 32;
        img.format        = XPE_PIXEL_FLOAT32;
        img.dataSize      = imgPixels.size() * sizeof(float);
        meta.acquisitionTime = 0.0;
    }

    void TearDown() override {
        if (handle) {
            xpe_ghost_destroy(handle);
            handle = nullptr;
        }
    }
};

// REQ-P1A-029: xpe_ghost_create returns XPE_OK with valid dimensions
TEST_F(GhostCorrectTest, CreateSucceeds) {
    EXPECT_EQ(XPE_OK, xpe_ghost_create(W, H, nullptr, &handle));
    EXPECT_NE(nullptr, handle);
}

// REQ-P1A-031: zero dimensions return error
TEST_F(GhostCorrectTest, ZeroWidthReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ghost_create(0, H, nullptr, &handle));
}

TEST_F(GhostCorrectTest, ZeroHeightReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ghost_create(W, 0, nullptr, &handle));
}

TEST_F(GhostCorrectTest, NullHandleOutReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ghost_create(W, H, nullptr, nullptr));
}

// REQ-P1A-034: xpe_ghost_reset clears history
TEST_F(GhostCorrectTest, ResetSucceeds) {
    ASSERT_EQ(XPE_OK, xpe_ghost_create(W, H, nullptr, &handle));
    EXPECT_EQ(XPE_OK, xpe_ghost_reset(handle));
}

TEST_F(GhostCorrectTest, ResetNullHandleReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ghost_reset(nullptr));
}

// xpe_ghost_destroy is a no-op for null (must not crash)
TEST_F(GhostCorrectTest, DestroyNullIsNoOp) {
    EXPECT_NO_FATAL_FAILURE(xpe_ghost_destroy(nullptr));
}

// REQ-P1A-032: xpe_ghost_correct succeeds on valid inputs
TEST_F(GhostCorrectTest, CorrectSucceedsOnValidHandle) {
    ASSERT_EQ(XPE_OK, xpe_ghost_create(W, H, nullptr, &handle));
    EXPECT_EQ(XPE_OK, xpe_ghost_correct(handle, &img, &meta));
}

// Null handle returns error
TEST_F(GhostCorrectTest, CorrectNullHandleReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ghost_correct(nullptr, &img, &meta));
}

// Dimension mismatch between handle and image returns error
TEST_F(GhostCorrectTest, CorrectDimensionMismatchReturnsError) {
    ASSERT_EQ(XPE_OK, xpe_ghost_create(W, H, nullptr, &handle));
    XpeImageBuffer wrongImg = img;
    wrongImg.width = W + 4;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ghost_correct(handle, &wrongImg, &meta));
}

// Repeated frames reduce the visible residual after the first history update.
TEST_F(GhostCorrectTest, RepeatedFramesApplyHistoryCorrection) {
    ASSERT_EQ(XPE_OK, xpe_ghost_create(W, H, nullptr, &handle));
    std::vector<float> first(W * H, 500.0f);
    XpeImageBuffer f1 = img;
    f1.data = first.data();
    f1.dataSize = first.size() * sizeof(float);

    meta.acquisitionTime = 1;
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(handle, &f1, &meta));
    EXPECT_NEAR(500.0f, first[W * H / 2], 1e-3f);

    std::fill(first.begin(), first.end(), 500.0f);
    meta.acquisitionTime = 2;
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(handle, &f1, &meta));
    EXPECT_LT(first[W * H / 2], 500.0f);
}

} // namespace
