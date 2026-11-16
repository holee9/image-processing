/**
 * @file test_temp_nonlinearity_binning.cpp
 * @brief TDD RED tests for SWU-1.8: xpe_binning_correct, REQ-P1A-090/091 (binning); runtime detection.
 *        The temperature tests (SWU-1.6) were removed with the stage (QA-A-335), the nonlinearity tests (SWU-1.7)
 *        with that stage (QA-A-336).
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <vector>
#include <cmath>
#include <limits>

namespace {

static XpeImageBuffer make_float32_buf(std::vector<float>& data, uint32_t w, uint32_t h) {
    XpeImageBuffer buf{};
    buf.data          = data.data();
    buf.width         = w;
    buf.height        = h;
    buf.bitsAllocated = 32;
    buf.bitsStored    = 32;
    buf.format        = XPE_PIXEL_FLOAT32;
    buf.dataSize      = data.size() * sizeof(float);
    return buf;
}

static XpeImageBuffer make_uint8_buf(std::vector<uint8_t>& data, uint32_t w, uint32_t h) {
    XpeImageBuffer buf{};
    buf.data          = data.data();
    buf.width         = w;
    buf.height        = h;
    buf.bitsAllocated = 8;
    buf.bitsStored    = 8;
    buf.format        = XPE_PIXEL_UINT8;
    buf.dataSize      = data.size();
    return buf;
}

/* === Binning Correction === */

TEST(BinningCorrect, NullImgReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_binning_correct(nullptr, 2, nullptr));
}

// REQ-P1A-091: binningMode == 1 is no-op, must return XPE_OK
TEST(BinningCorrect, Binning1x1IsNoOp) {
    std::vector<float> data(16, 100.0f);
    XpeImageBuffer buf = make_float32_buf(data, 4, 4);
    EXPECT_EQ(XPE_OK, xpe_binning_correct(&buf, 1, nullptr));
    EXPECT_NEAR(100.0f, *static_cast<const float*>(buf.data), 1e-6f);
}

// REQ-P1A-091: unknown binning mode (not 1, 2 or 4) -> XPE_ERR_CONFIG_INVALID
TEST(BinningCorrect, UnknownBinningModeReturnsError) {
    std::vector<float> data(16, 100.0f);
    XpeImageBuffer buf = make_float32_buf(data, 4, 4);
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_binning_correct(&buf, 3, nullptr));
}

// 2x2 binning with valid config must succeed
TEST(BinningCorrect, Binning2x2ReturnsOk) {
    std::vector<float> data(16, 100.0f);
    XpeImageBuffer buf = make_float32_buf(data, 4, 4);
    EXPECT_EQ(XPE_OK, xpe_binning_correct(&buf, 2, nullptr));
}

/* === Runtime Defect Detection === */

TEST(RuntimeDetection, HotPixelOnFlatFieldIsDetected) {
    constexpr uint32_t W = 5;
    constexpr uint32_t H = 5;
    std::vector<float> data(W * H, 100.0f);
    data[12] = 500.0f;
    std::vector<uint8_t> mask(W * H, 0);

    XpeImageBuffer img = make_float32_buf(data, W, H);
    XpeImageBuffer out = make_uint8_buf(mask, W, H);

    EXPECT_EQ(XPE_OK, xpe_defect_detect_runtime(&img, nullptr, &out));
    EXPECT_EQ(1u, mask[12]);
}

TEST(RuntimeDetection, CleanImageClearsOnlyMaskPixelBytes) {
    constexpr uint32_t W = 5;
    constexpr uint32_t H = 5;
    constexpr size_t N = static_cast<size_t>(W) * H;
    std::vector<float> data(N, 100.0f);
    std::vector<uint8_t> mask(N + 4, 0xAA);

    XpeImageBuffer img = make_float32_buf(data, W, H);
    XpeImageBuffer out = make_uint8_buf(mask, W, H);

    EXPECT_EQ(XPE_OK, xpe_defect_detect_runtime(&img, nullptr, &out));

    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(0u, mask[i]);
    }
    for (size_t i = N; i < mask.size(); ++i) {
        EXPECT_EQ(0xAAu, mask[i]);
    }
}

TEST(RuntimeDetection, OutputDataSizeTooSmallReturnsBufferTooSmall) {
    constexpr uint32_t W = 5;
    constexpr uint32_t H = 5;
    constexpr size_t N = static_cast<size_t>(W) * H;
    std::vector<float> data(N, 100.0f);
    std::vector<uint8_t> mask(N - 1, 0);

    XpeImageBuffer img = make_float32_buf(data, W, H);
    XpeImageBuffer out = make_uint8_buf(mask, W, H);

    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL,
              xpe_defect_detect_runtime(&img, nullptr, &out));
}

TEST(RuntimeDetection, InputDataSizeTooSmallReturnsInvalidInput) {
    constexpr uint32_t W = 5;
    constexpr uint32_t H = 5;
    std::vector<float> data(W * H, 100.0f);
    std::vector<uint8_t> mask(W * H, 0);

    XpeImageBuffer img = make_float32_buf(data, W, H);
    img.dataSize -= 1;
    XpeImageBuffer out = make_uint8_buf(mask, W, H);

    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_defect_detect_runtime(&img, nullptr, &out));
}

TEST(RuntimeDetection, ZeroDimensionsReturnInvalidInput) {
    constexpr uint32_t W = 5;
    constexpr uint32_t H = 5;
    std::vector<float> data(W * H, 100.0f);
    std::vector<uint8_t> mask(W * H, 0);

    XpeImageBuffer img = make_float32_buf(data, W, H);
    XpeImageBuffer out = make_uint8_buf(mask, W, H);
    img.width = 0;
    out.width = 0;

    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_defect_detect_runtime(&img, nullptr, &out));
}

} // namespace
