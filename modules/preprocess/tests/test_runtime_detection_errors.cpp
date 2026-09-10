/**
 * @file test_runtime_detection_errors.cpp
 * @brief Runtime defect detection — error paths (QA-A-33, #120 #112)
 *
 * Companion to `test_runtime_detection_functional.cpp`. The deleted suite
 * (QA-A-27) carried three argument-validation cases; this file restores them
 * against the shipped signature and adds the checks the old one did not make.
 *
 * NOT repeated here: the NULL-argument rule. QA-A-14's
 * `test_error_precedence.cpp` already probes `xpe_defect_detect_runtime`'s
 * required pointers in both module states and pins XPE_ERR_INVALID_INPUT --
 * duplicating it would give two places to update for one contract. What this
 * file covers instead is the NULL DATA POINTER inside an otherwise valid
 * buffer, plus format, dimension and size mismatches, which that probe does not
 * reach.
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <vector>

namespace {

constexpr uint32_t W = 16, H = 16;
constexpr size_t   N = static_cast<size_t>(W) * H;

XpeImageBuffer floatImage(std::vector<float>& v, uint32_t w = W, uint32_t h = H) {
    XpeImageBuffer b{};
    b.data = v.data(); b.width = w; b.height = h;
    b.bitsAllocated = 32; b.bitsStored = 32;
    b.format = XPE_PIXEL_FLOAT32;
    b.dataSize = v.size() * sizeof(float);
    return b;
}

XpeImageBuffer maskBuffer(std::vector<uint8_t>& v, uint32_t w = W, uint32_t h = H) {
    XpeImageBuffer b{};
    b.data = v.data(); b.width = w; b.height = h;
    b.bitsAllocated = 8; b.bitsStored = 8;
    b.format = XPE_PIXEL_UINT8;
    b.dataSize = v.size();
    return b;
}

class RuntimeDetectionErrorTest : public ::testing::Test {
protected:
    std::vector<float>   pixels{std::vector<float>(N, 1000.0f)};
    std::vector<uint8_t> mask{std::vector<uint8_t>(N, 0u)};
    XpeImageMetadata     meta{};

    void SetUp() override    { ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr)); }
    void TearDown() override { xpe_preprocess_shutdown(); }
};

// A buffer struct that is itself non-NULL but carries a NULL data pointer is a
// distinct failure from a NULL buffer, and it must not be dereferenced.
TEST_F(RuntimeDetectionErrorTest, NullImageDataIsRejected) {
    XpeImageBuffer img = floatImage(pixels);
    img.data = nullptr;
    XpeImageBuffer out = maskBuffer(mask);

    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_defect_detect_runtime(&img, &meta, &out));
}

TEST_F(RuntimeDetectionErrorTest, NullOutputDataIsRejected) {
    XpeImageBuffer img = floatImage(pixels);
    XpeImageBuffer out = maskBuffer(mask);
    out.data = nullptr;

    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_defect_detect_runtime(&img, &meta, &out));
}

// The detector reads FLOAT32. A UINT16 image is rejected rather than
// reinterpreted.
TEST_F(RuntimeDetectionErrorTest, NonFloatImageIsRejected) {
    std::vector<uint16_t> u16(N, 1000u);
    XpeImageBuffer img{};
    img.data = u16.data();
    img.width = W; img.height = H;
    img.bitsAllocated = 16; img.bitsStored = 16;
    img.format = XPE_PIXEL_UINT16;
    img.dataSize = u16.size() * sizeof(uint16_t);
    XpeImageBuffer out = maskBuffer(mask);

    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_defect_detect_runtime(&img, &meta, &out));
}

// The defect map is a UINT8 mask; a FLOAT32 output buffer is rejected.
TEST_F(RuntimeDetectionErrorTest, NonUint8DefectMapIsRejected) {
    XpeImageBuffer img = floatImage(pixels);
    std::vector<float> wrongOut(N, 0.0f);
    XpeImageBuffer out = floatImage(wrongOut);

    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_defect_detect_runtime(&img, &meta, &out));
}

// Width mismatch between image and defect map.
TEST_F(RuntimeDetectionErrorTest, WidthMismatchIsRejected) {
    XpeImageBuffer img = floatImage(pixels);
    std::vector<uint8_t> narrow(static_cast<size_t>(W - 1) * H, 0u);
    XpeImageBuffer out = maskBuffer(narrow, W - 1, H);

    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_defect_detect_runtime(&img, &meta, &out));
}

// Height mismatch, separately: a single dimension check that only looked at
// width would pass the case above and fail here.
TEST_F(RuntimeDetectionErrorTest, HeightMismatchIsRejected) {
    XpeImageBuffer img = floatImage(pixels);
    std::vector<uint8_t> shortMask(static_cast<size_t>(W) * (H - 1), 0u);
    XpeImageBuffer out = maskBuffer(shortMask, W, H - 1);

    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_defect_detect_runtime(&img, &meta, &out));
}

// Dimensions agree but the output buffer is too small to hold the mask. This is
// the read/write-overflow guard, and it reports BUFFER_TOO_SMALL rather than
// INVALID_INPUT (runtime_detection.cpp:152).
TEST_F(RuntimeDetectionErrorTest, UndersizedDefectMapReportsBufferTooSmall) {
    XpeImageBuffer img = floatImage(pixels);
    XpeImageBuffer out = maskBuffer(mask);
    out.dataSize = N / 2;               // claims half the bytes it needs

    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL,
              xpe_defect_detect_runtime(&img, &meta, &out));
}

// An input buffer whose dataSize is smaller than width*height*sizeof(float) is
// rejected before any pixel is read.
TEST_F(RuntimeDetectionErrorTest, UndersizedImageIsRejected) {
    XpeImageBuffer img = floatImage(pixels);
    img.dataSize = N * sizeof(float) / 2;
    XpeImageBuffer out = maskBuffer(mask);

    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_defect_detect_runtime(&img, &meta, &out));
}

// Metadata is unused by this entry point (`(void)metadata` at
// runtime_detection.cpp:122), so a NULL one must not fail the call -- it is an
// optional pointer, and the precedence rule 1 does not apply to it.
TEST_F(RuntimeDetectionErrorTest, NullMetadataIsAccepted) {
    XpeImageBuffer img = floatImage(pixels);
    XpeImageBuffer out = maskBuffer(mask);

    EXPECT_EQ(XPE_OK, xpe_defect_detect_runtime(&img, nullptr, &out))
        << "metadata is unused; NULL is not an error here";
}

// A zero-dimension image has no pixel to test and must be rejected rather than
// looping zero times and reporting success on an unwritten map.
TEST_F(RuntimeDetectionErrorTest, ZeroDimensionImageIsRejected) {
    std::vector<float>   none;
    std::vector<uint8_t> noneMask;
    XpeImageBuffer img = floatImage(none, 0, 0);
    XpeImageBuffer out = maskBuffer(noneMask, 0, 0);

    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_defect_detect_runtime(&img, &meta, &out));
}

} // namespace
