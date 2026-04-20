/**
 * @file test_offset_correct_avx2_parity.cpp
 * @brief TDD RED tests for AVX2 parity: scalar vs AVX2 must be bit-identical
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include <gtest/gtest.h>
#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <vector>
#include <cstdint>
#include <cstring>
#include <random>

namespace {

class OffsetCorrectAVX2ParityTest : public ::testing::Test {
protected:
    std::vector<uint16_t> rawPixelsScalar;
    std::vector<uint16_t> rawPixelsAVX2;
    std::vector<uint16_t> offsetPixels;
    XpeImageBuffer imgScalar{};
    XpeImageBuffer imgAVX2{};
    XpeImageBuffer offsetMap{};

    void SetUp() override {
        // Use deterministic random seed for reproducibility
        std::mt19937 rng(0x5EED); // Fixed seed
        std::uniform_int_distribution<uint16_t> rawDist(0, 4000);
        std::uniform_int_distribution<uint16_t> offsetDist(0, 500);

        const size_t pixelCount = 1024 * 768; // Typical X-ray detector size
        rawPixelsScalar.resize(pixelCount);
        rawPixelsAVX2.resize(pixelCount);
        offsetPixels.resize(pixelCount);

        for (size_t i = 0; i < pixelCount; ++i) {
            rawPixelsScalar[i] = rawDist(rng);
            rawPixelsAVX2[i] = rawPixelsScalar[i]; // Identical input
            offsetPixels[i] = offsetDist(rng);
        }

        // Setup scalar image buffer
        imgScalar.data = rawPixelsScalar.data();
        imgScalar.width = 1024;
        imgScalar.height = 768;
        imgScalar.bitsAllocated = 16;
        imgScalar.bitsStored = 16;
        imgScalar.format = XPE_PIXEL_UINT16;
        imgScalar.dataSize = rawPixelsScalar.size() * sizeof(uint16_t);

        // Setup AVX2 image buffer (identical)
        imgAVX2.data = rawPixelsAVX2.data();
        imgAVX2.width = 1024;
        imgAVX2.height = 768;
        imgAVX2.bitsAllocated = 16;
        imgAVX2.bitsStored = 16;
        imgAVX2.format = XPE_PIXEL_UINT16;
        imgAVX2.dataSize = rawPixelsAVX2.size() * sizeof(uint16_t);

        // Setup offset map
        offsetMap.data = offsetPixels.data();
        offsetMap.width = 1024;
        offsetMap.height = 768;
        offsetMap.bitsAllocated = 16;
        offsetMap.bitsStored = 16;
        offsetMap.format = XPE_PIXEL_UINT16;
        offsetMap.dataSize = offsetPixels.size() * sizeof(uint16_t);
    }
};

// REQ-P1A-010: Scalar and AVX2 must produce bit-identical results
TEST_F(OffsetCorrectAVX2ParityTest, ScalarAndAVX2AreBitIdentical) {
    ASSERT_EQ(XPE_OK, xpe_offset_correct(&imgScalar, &offsetMap));
    ASSERT_EQ(XPE_OK, xpe_offset_correct(&imgAVX2, &offsetMap));

    // Every pixel must be identical
    for (size_t i = 0; i < rawPixelsScalar.size(); ++i) {
        EXPECT_EQ(rawPixelsScalar[i], rawPixelsAVX2[i])
            << "Pixel mismatch at index " << i
            << " (scalar=" << rawPixelsScalar[i]
            << ", avx2=" << rawPixelsAVX2[i] << ")";
    }
}

// Test with image size not multiple of AVX2 stride (16)
TEST_F(OffsetCorrectAVX2ParityTest, ParityWithNonMultipleStride) {
    // Use size that forces tail processing: 1000 pixels (62*16 + 8)
    const size_t oddSize = 1000;
    imgScalar.width = 1000;
    imgScalar.height = 1;
    imgScalar.dataSize = oddSize * sizeof(uint16_t);
    imgAVX2.width = 1000;
    imgAVX2.height = 1;
    imgAVX2.dataSize = oddSize * sizeof(uint16_t);
    offsetMap.width = 1000;
    offsetMap.height = 1;
    offsetMap.dataSize = oddSize * sizeof(uint16_t);

    ASSERT_EQ(XPE_OK, xpe_offset_correct(&imgScalar, &offsetMap));
    ASSERT_EQ(XPE_OK, xpe_offset_correct(&imgAVX2, &offsetMap));

    for (size_t i = 0; i < oddSize; ++i) {
        EXPECT_EQ(rawPixelsScalar[i], rawPixelsAVX2[i])
            << "Tail pixel mismatch at index " << i;
    }
}

// Test edge cases: all underflow cases
TEST_F(OffsetCorrectAVX2ParityTest, ParityWithAllUnderflow) {
    std::fill(rawPixelsScalar.begin(), rawPixelsScalar.end(), 100);
    std::fill(rawPixelsAVX2.begin(), rawPixelsAVX2.end(), 100);
    std::fill(offsetPixels.begin(), offsetPixels.end(), 500); // All underflow

    ASSERT_EQ(XPE_OK, xpe_offset_correct(&imgScalar, &offsetMap));
    ASSERT_EQ(XPE_OK, xpe_offset_correct(&imgAVX2, &offsetMap));

    // All should be clamped to 0
    for (size_t i = 0; i < rawPixelsScalar.size(); ++i) {
        EXPECT_EQ(0u, rawPixelsScalar[i]);
        EXPECT_EQ(0u, rawPixelsAVX2[i]);
        EXPECT_EQ(rawPixelsScalar[i], rawPixelsAVX2[i]);
    }
}

// Test edge cases: max UINT16 values
TEST_F(OffsetCorrectAVX2ParityTest, ParityWithMaxUInt16Values) {
    std::fill(rawPixelsScalar.begin(), rawPixelsScalar.end(), UINT16_MAX);
    std::fill(rawPixelsAVX2.begin(), rawPixelsAVX2.end(), UINT16_MAX);
    std::fill(offsetPixels.begin(), offsetPixels.end(), 1000);

    ASSERT_EQ(XPE_OK, xpe_offset_correct(&imgScalar, &offsetMap));
    ASSERT_EQ(XPE_OK, xpe_offset_correct(&imgAVX2, &offsetMap));

    for (size_t i = 0; i < rawPixelsScalar.size(); ++i) {
        EXPECT_EQ(UINT16_MAX - 1000, rawPixelsScalar[i]);
        EXPECT_EQ(UINT16_MAX - 1000, rawPixelsAVX2[i]);
        EXPECT_EQ(rawPixelsScalar[i], rawPixelsAVX2[i]);
    }
}

// Test small image that may bypass AVX2 (threshold < 16 pixels)
TEST_F(OffsetCorrectAVX2ParityTest, ParityWithSmallImageBelowAVX2Threshold) {
    const size_t tinySize = 10; // Below AVX2 threshold
    std::vector<uint16_t> tinyRawScalar(tinySize, 1000);
    std::vector<uint16_t> tinyRawAVX2(tinySize, 1000);
    std::vector<uint16_t> tinyOffset(tinySize, 200);

    XpeImageBuffer tinyScalar{};
    tinyScalar.data = tinyRawScalar.data();
    tinyScalar.width = 10;
    tinyScalar.height = 1;
    tinyScalar.bitsAllocated = 16;
    tinyScalar.bitsStored = 16;
    tinyScalar.format = XPE_PIXEL_UINT16;
    tinyScalar.dataSize = tinySize * sizeof(uint16_t);

    XpeImageBuffer tinyAVX2 = tinyScalar;
    tinyAVX2.data = tinyRawAVX2.data();

    XpeImageBuffer tinyOffsetMap{};
    tinyOffsetMap.data = tinyOffset.data();
    tinyOffsetMap.width = 10;
    tinyOffsetMap.height = 1;
    tinyOffsetMap.bitsAllocated = 16;
    tinyOffsetMap.bitsStored = 16;
    tinyOffsetMap.format = XPE_PIXEL_UINT16;
    tinyOffsetMap.dataSize = tinySize * sizeof(uint16_t);

    ASSERT_EQ(XPE_OK, xpe_offset_correct(&tinyScalar, &tinyOffsetMap));
    ASSERT_EQ(XPE_OK, xpe_offset_correct(&tinyAVX2, &tinyOffsetMap));

    for (size_t i = 0; i < tinySize; ++i) {
        EXPECT_EQ(tinyRawScalar[i], tinyRawAVX2[i])
            << "Small image mismatch at index " << i;
    }
}

// Test boundary pixels (first and last row)
TEST_F(OffsetCorrectAVX2ParityTest, ParityForBoundaryPixels) {
    const size_t width = 1024;
    const size_t height = 768;

    // Set first row to specific pattern
    for (size_t x = 0; x < width; ++x) {
        rawPixelsScalar[x] = 1000 + x;
        rawPixelsAVX2[x] = 1000 + x;
        offsetPixels[x] = 100;
    }

    // Set last row to specific pattern
    const size_t lastRowOffset = (height - 1) * width;
    for (size_t x = 0; x < width; ++x) {
        rawPixelsScalar[lastRowOffset + x] = static_cast<uint16_t>(2000 + x);
        rawPixelsAVX2[lastRowOffset + x] = static_cast<uint16_t>(2000 + x);
        offsetPixels[lastRowOffset + x] = 200;
    }

    ASSERT_EQ(XPE_OK, xpe_offset_correct(&imgScalar, &offsetMap));
    ASSERT_EQ(XPE_OK, xpe_offset_correct(&imgAVX2, &offsetMap));

    // Verify first row
    for (size_t x = 0; x < width; ++x) {
        EXPECT_EQ(rawPixelsScalar[x], rawPixelsAVX2[x])
            << "First row mismatch at x=" << x;
    }

    // Verify last row
    for (size_t x = 0; x < width; ++x) {
        EXPECT_EQ(rawPixelsScalar[lastRowOffset + x], rawPixelsAVX2[lastRowOffset + x])
            << "Last row mismatch at x=" << x;
    }
}

} // namespace
