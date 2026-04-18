/**
 * @file test_xpe_calib_generate_offset.cpp
 * @brief Unit tests for xpe_calib_generate_offset (T-008)
 *
 * SPEC-XPE-P1A SUP-01 -- REQ-P1A-017, REQ-P1A-033
 *
 * Test cases:
 *  1.  Single frame: output == frame pixels (identity average)
 *  2.  Two frames: output == pixel-wise average
 *  3.  Three frames: average verified
 *  4.  Output file passes read_xcal_file (SHA-256, magic, type)
 *  5.  Null dark_frames -> XPE_ERR_INVALID_INPUT
 *  6.  Null output_path -> XPE_ERR_INVALID_INPUT
 *  7.  num_frames == 0 -> XPE_ERR_INVALID_INPUT
 *  8.  num_frames < 0 -> XPE_ERR_INVALID_INPUT
 *  9.  Dimension mismatch between frames -> XPE_ERR_INVALID_INPUT
 * 10.  Unsupported format (FLOAT32 frame) -> XPE_ERR_UNSUPPORTED_FORMAT
 * 11.  Output header width/height match input
 * 12.  Output pixel format is FLOAT32
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <limits>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_reader.hpp"

namespace {
constexpr uint32_t W = 8;
constexpr uint32_t H = 8;

// Build a UINT16 XpeImageBuffer from a flat vector.
struct FrameHelper {
    std::vector<uint16_t> pixels;
    XpeImageBuffer buf;

    explicit FrameHelper(uint32_t w, uint32_t h, uint16_t fill = 0) {
        pixels.assign(static_cast<size_t>(w) * h, fill);
        std::memset(&buf, 0, sizeof(buf));
        buf.width       = w;
        buf.height      = h;
        buf.format      = XPE_PIXEL_UINT16;
        buf.bitsAllocated = 16;
        buf.bitsStored    = 16;
        buf.data        = pixels.data();
        buf.dataSize    = pixels.size() * sizeof(uint16_t);
    }

    // Set individual pixel
    void set(uint32_t row, uint32_t col, uint16_t val) {
        pixels[row * buf.width + col] = val;
    }
};

} // anonymous namespace

class GenerateOffsetTest : public ::testing::Test {
protected:
    const char* out_path = "t008_offset.xcal";

    void TearDown() override {
        std::remove(out_path);
        std::remove((std::string(out_path) + ".tmp").c_str());
    }
};

// =============================================================================
// Test 1: Single frame -> output == input pixels
// =============================================================================
TEST_F(GenerateOffsetTest, SingleFrame_OutputEqualsInput) {
    FrameHelper f(W, H, 1000);
    XpeErrorCode rc = xpe_calib_generate_offset(&f.buf, 1, 10.0f, 25.0f, out_path);
    ASSERT_EQ(rc, XPE_OK);

    XCalFileHeader hdr;
    std::vector<uint8_t> cfg, payload;
    ASSERT_EQ(read_xcal_file(out_path, hdr, cfg, payload,
                             /*check_expiry=*/false, XCAL_TYPE_OFFSET), XPE_OK);

    const float* pix = reinterpret_cast<const float*>(payload.data());
    for (size_t i = 0; i < static_cast<size_t>(W) * H; ++i) {
        EXPECT_FLOAT_EQ(pix[i], 1000.0f) << "pixel[" << i << "]";
        if (pix[i] != 1000.0f) break;
    }
}

// =============================================================================
// Test 2: Two frames: average = (a + b) / 2
// =============================================================================
TEST_F(GenerateOffsetTest, TwoFrames_AverageIsCorrect) {
    FrameHelper f1(W, H, 100);
    FrameHelper f2(W, H, 200);
    XpeImageBuffer frames[2] = { f1.buf, f2.buf };

    ASSERT_EQ(xpe_calib_generate_offset(frames, 2, 10.0f, 25.0f, out_path), XPE_OK);

    XCalFileHeader hdr;
    std::vector<uint8_t> cfg, payload;
    ASSERT_EQ(read_xcal_file(out_path, hdr, cfg, payload,
                             /*check_expiry=*/false, XCAL_TYPE_OFFSET), XPE_OK);

    const float* pix = reinterpret_cast<const float*>(payload.data());
    for (size_t i = 0; i < static_cast<size_t>(W) * H; ++i) {
        EXPECT_FLOAT_EQ(pix[i], 150.0f) << "pixel[" << i << "]";
        if (pix[i] != 150.0f) break;
    }
}

// =============================================================================
// Test 3: Three frames: average verified
// =============================================================================
TEST_F(GenerateOffsetTest, ThreeFrames_AverageIsCorrect) {
    FrameHelper f1(W, H, 0);
    FrameHelper f2(W, H, 300);
    FrameHelper f3(W, H, 600);
    XpeImageBuffer frames[3] = { f1.buf, f2.buf, f3.buf };

    ASSERT_EQ(xpe_calib_generate_offset(frames, 3, 10.0f, 25.0f, out_path), XPE_OK);

    XCalFileHeader hdr;
    std::vector<uint8_t> cfg, payload;
    ASSERT_EQ(read_xcal_file(out_path, hdr, cfg, payload,
                             /*check_expiry=*/false, XCAL_TYPE_OFFSET), XPE_OK);

    const float* pix = reinterpret_cast<const float*>(payload.data());
    float expected = (0.0f + 300.0f + 600.0f) / 3.0f;  // 300.0f
    for (size_t i = 0; i < static_cast<size_t>(W) * H; ++i) {
        EXPECT_NEAR(pix[i], expected, 0.001f) << "pixel[" << i << "]";
        if (std::abs(pix[i] - expected) > 0.001f) break;
    }
}

// =============================================================================
// Test 4: Output file passes SHA-256 + type verification
// =============================================================================
TEST_F(GenerateOffsetTest, OutputFile_PassesSHA256AndTypeVerification) {
    FrameHelper f(W, H, 500);
    ASSERT_EQ(xpe_calib_generate_offset(&f.buf, 1, 0.0f, 0.0f, out_path), XPE_OK);

    XCalFileHeader hdr;
    std::vector<uint8_t> cfg, payload;
    XpeErrorCode rc = read_xcal_file(out_path, hdr, cfg, payload,
                                     /*check_expiry=*/false, XCAL_TYPE_OFFSET);
    EXPECT_EQ(rc, XPE_OK);
}

// =============================================================================
// Test 5: Null dark_frames -> INVALID_INPUT
// =============================================================================
TEST_F(GenerateOffsetTest, NullFrames_ReturnsInvalidInput) {
    EXPECT_EQ(xpe_calib_generate_offset(nullptr, 1, 0.0f, 0.0f, out_path),
              XPE_ERR_INVALID_INPUT);
}

// =============================================================================
// Test 6: Null output_path -> INVALID_INPUT
// =============================================================================
TEST_F(GenerateOffsetTest, NullOutputPath_ReturnsInvalidInput) {
    FrameHelper f(W, H, 0);
    EXPECT_EQ(xpe_calib_generate_offset(&f.buf, 1, 0.0f, 0.0f, nullptr),
              XPE_ERR_INVALID_INPUT);
}

// =============================================================================
// Test 7: num_frames == 0 -> INVALID_INPUT
// =============================================================================
TEST_F(GenerateOffsetTest, ZeroFrames_ReturnsInvalidInput) {
    FrameHelper f(W, H, 0);
    EXPECT_EQ(xpe_calib_generate_offset(&f.buf, 0, 0.0f, 0.0f, out_path),
              XPE_ERR_INVALID_INPUT);
}

// =============================================================================
// Test 8: num_frames < 0 -> INVALID_INPUT
// =============================================================================
TEST_F(GenerateOffsetTest, NegativeFrames_ReturnsInvalidInput) {
    FrameHelper f(W, H, 0);
    EXPECT_EQ(xpe_calib_generate_offset(&f.buf, -5, 0.0f, 0.0f, out_path),
              XPE_ERR_INVALID_INPUT);
}

// =============================================================================
// Test 9: Dimension mismatch between frames -> INVALID_INPUT
// =============================================================================
TEST_F(GenerateOffsetTest, DimensionMismatch_ReturnsInvalidInput) {
    FrameHelper f1(W, H, 100);
    FrameHelper f2(W + 1, H, 200);  // different width
    XpeImageBuffer frames[2] = { f1.buf, f2.buf };

    EXPECT_EQ(xpe_calib_generate_offset(frames, 2, 0.0f, 0.0f, out_path),
              XPE_ERR_INVALID_INPUT);
}

// =============================================================================
// Test 10: Unsupported format (FLOAT32 input) -> UNSUPPORTED_FORMAT
// =============================================================================
TEST_F(GenerateOffsetTest, UnsupportedFormat_ReturnsUnsupportedFormat) {
    std::vector<float> fdata(static_cast<size_t>(W) * H, 1.0f);
    XpeImageBuffer buf;
    std::memset(&buf, 0, sizeof(buf));
    buf.width    = W;
    buf.height   = H;
    buf.format   = XPE_PIXEL_FLOAT32;
    buf.data     = fdata.data();
    buf.dataSize = fdata.size() * sizeof(float);

    EXPECT_EQ(xpe_calib_generate_offset(&buf, 1, 0.0f, 0.0f, out_path),
              XPE_ERR_UNSUPPORTED_FORMAT);
}

// =============================================================================
// Test 11: Output header width/height match input
// =============================================================================
TEST_F(GenerateOffsetTest, OutputHeader_WidthHeightMatchInput) {
    FrameHelper f(W, H, 42);
    ASSERT_EQ(xpe_calib_generate_offset(&f.buf, 1, 0.0f, 0.0f, out_path), XPE_OK);

    XCalFileHeader hdr;
    std::vector<uint8_t> cfg, payload;
    ASSERT_EQ(read_xcal_file(out_path, hdr, cfg, payload,
                             /*check_expiry=*/false), XPE_OK);
    EXPECT_EQ(hdr.width,  W);
    EXPECT_EQ(hdr.height, H);
}

// =============================================================================
// Test 12: Output pixel format is FLOAT32
// =============================================================================
TEST_F(GenerateOffsetTest, OutputHeader_PixelFormatIsFloat32) {
    FrameHelper f(W, H, 0);
    ASSERT_EQ(xpe_calib_generate_offset(&f.buf, 1, 0.0f, 0.0f, out_path), XPE_OK);

    XCalFileHeader hdr;
    std::vector<uint8_t> cfg, payload;
    ASSERT_EQ(read_xcal_file(out_path, hdr, cfg, payload,
                             /*check_expiry=*/false), XPE_OK);
    EXPECT_EQ(hdr.pixel_format, static_cast<uint32_t>(XCAL_FMT_FLOAT32));
}
