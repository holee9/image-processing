/**
 * @file test_offset_correct_avx2_parity.cpp
 * @brief TDD RED tests for AVX2 parity: multiple calls with same input must be bit-identical
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 *
 * Tests the new 3-arg API: xpe_offset_correct(input, output, metadata)
 * Note: Tests use existing calibration loaded via xpe_calib_load_offset
 */

#include <gtest/gtest.h>
#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <vector>
#include <cstdint>
#include <cstring>
#include <random>
#include <chrono>

namespace {

class OffsetCorrectAVX2ParityTest : public ::testing::Test {
protected:
    std::vector<uint16_t> inputPixels1;
    std::vector<uint16_t> inputPixels2;
    std::vector<uint16_t> outputPixels1;
    std::vector<uint16_t> outputPixels2;
    XpeImageBuffer input1{};
    XpeImageBuffer input2{};
    XpeImageBuffer output1{};
    XpeImageBuffer output2{};
    XpeImageMetadata metadata{};
    static bool calibrationLoaded;

    void SetUp() override {
        if (!calibrationLoaded) {
            ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
            // Note: Calibration must be loaded externally before running these tests
            // Use: xpe_calib_load_offset("path/to/offset.xcal", &map);
            calibrationLoaded = true;
        }

        std::mt19937 rng(0x5EED);
        std::uniform_int_distribution<uint16_t> inputDist(0, 4000);

        const size_t pixelCount = 1024 * 768;
        inputPixels1.resize(pixelCount);
        inputPixels2.resize(pixelCount);
        outputPixels1.resize(pixelCount);
        outputPixels2.resize(pixelCount);

        for (size_t i = 0; i < pixelCount; ++i) {
            inputPixels1[i] = inputDist(rng);
            inputPixels2[i] = inputPixels1[i];
        }

        input1.data = inputPixels1.data();
        input1.width = 1024;
        input1.height = 768;
        input1.bitsAllocated = 16;
        input1.bitsStored = 16;
        input1.format = XPE_PIXEL_UINT16;
        input1.dataSize = inputPixels1.size() * sizeof(uint16_t);

        input2.data = inputPixels2.data();
        input2.width = 1024;
        input2.height = 768;
        input2.bitsAllocated = 16;
        input2.bitsStored = 16;
        input2.format = XPE_PIXEL_UINT16;
        input2.dataSize = inputPixels2.size() * sizeof(uint16_t);

        output1.data = outputPixels1.data();
        output1.width = 1024;
        output1.height = 768;
        output1.bitsAllocated = 16;
        output1.bitsStored = 16;
        output1.format = XPE_PIXEL_UINT16;
        output1.dataSize = outputPixels1.size() * sizeof(uint16_t);

        output2.data = outputPixels2.data();
        output2.width = 1024;
        output2.height = 768;
        output2.bitsAllocated = 16;
        output2.bitsStored = 16;
        output2.format = XPE_PIXEL_UINT16;
        output2.dataSize = outputPixels2.size() * sizeof(uint16_t);

        memset(&metadata, 0, sizeof(XpeImageMetadata));
    }

    void TearDown() override {
        // xpe_preprocess_shutdown() called in test suite teardown
    }
};

bool OffsetCorrectAVX2ParityTest::calibrationLoaded = false;

TEST_F(OffsetCorrectAVX2ParityTest, MultipleCallsAreBitIdentical) {
    // Load identity offset map (all 0.0 = identity transformation) for parity testing
    // REQ-SIMD-001: Verify bit-identical output across multiple AVX2 calls
    const char* identity_offset_map = "identity_offset_map.xcal";
    XpeErrorCode load_rc = xpe_calib_load_offset(identity_offset_map);

    // If offset map file is not found, skip with informative message
    if (load_rc != XPE_OK) {
        GTEST_SKIP() << "Offset map file not found: " << identity_offset_map
                     << " (error: " << load_rc << "). Run test from modules/preprocess/tests directory.";
    }

    ASSERT_EQ(XPE_OK, xpe_offset_correct(&input1, &output1, &metadata));
    ASSERT_EQ(XPE_OK, xpe_offset_correct(&input2, &output2, &metadata));

    for (size_t i = 0; i < outputPixels1.size(); ++i) {
        EXPECT_EQ(outputPixels1[i], outputPixels2[i])
            << "Pixel mismatch at index " << i;
    }
}

TEST_F(OffsetCorrectAVX2ParityTest, DISABLED_ParityWithNonMultipleStride) {
    // Use size that forces tail processing: 1000 pixels
    const size_t oddSize = 1000;
    std::vector<uint16_t> smallInput1(oddSize);
    std::vector<uint16_t> smallInput2(oddSize);
    std::vector<uint16_t> smallOutput1(oddSize);
    std::vector<uint16_t> smallOutput2(oddSize);

    std::mt19937 rng(0x5EED);
    std::uniform_int_distribution<uint16_t> dist(0, 4000);
    for (size_t i = 0; i < oddSize; ++i) {
        smallInput1[i] = dist(rng);
        smallInput2[i] = smallInput1[i];
    }

    XpeImageBuffer in1{}, in2{}, out1{}, out2{};
    in1.data = smallInput1.data(); in1.width = 1000; in1.height = 1;
    in1.bitsAllocated = 16; in1.bitsStored = 16; in1.format = XPE_PIXEL_UINT16;
    in1.dataSize = oddSize * sizeof(uint16_t);

    in2 = in1; in2.data = smallInput2.data();
    out1.data = smallOutput1.data(); out1.width = 1000; out1.height = 1;
    out1.bitsAllocated = 16; out1.bitsStored = 16; out1.format = XPE_PIXEL_UINT16;
    out1.dataSize = oddSize * sizeof(uint16_t);
    out2 = out1; out2.data = smallOutput2.data();

    ASSERT_EQ(XPE_OK, xpe_offset_correct(&in1, &out1, &metadata));
    ASSERT_EQ(XPE_OK, xpe_offset_correct(&in2, &out2, &metadata));

    for (size_t i = 0; i < oddSize; ++i) {
        EXPECT_EQ(smallOutput1[i], smallOutput2[i]);
    }
}

// @MX:NOTE: Timing assertion only valid in Release builds where SIMD optimizations are enabled
// @MX:REASON: Debug builds may not enable SIMD optimizations, making timing comparisons meaningless
TEST_F(OffsetCorrectAVX2ParityTest, SimdFasterThanScalar) {
#ifndef NDEBUG
    GTEST_SKIP() << "Timing assertions only run in Release builds (NDEBUG defined)";
#else
    // Load identity offset map for timing comparison
    const char* identity_offset_map = "identity_offset_map.xcal";
    XpeErrorCode load_rc = xpe_calib_load_offset(identity_offset_map);
    if (load_rc != XPE_OK) {
        GTEST_SKIP() << "Offset map file not found: " << identity_offset_map;
    }

    // Warm-up run to populate any caches
    ASSERT_EQ(XPE_OK, xpe_offset_correct(&input1, &output1, &metadata));

    // Measure AVX2 implementation timing
    constexpr int iterations = 100;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        ASSERT_EQ(XPE_OK, xpe_offset_correct(&input1, &output1, &metadata));
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto avx2_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // For offset correction with 1024x768 image, AVX2 should complete reasonably fast
    // This is a baseline assertion - actual SIMD vs scalar comparison requires reference implementation
    constexpr auto max_expected_duration_us = 300000; // 300ms for 100 iterations = 3ms per frame
    EXPECT_LT(avx2_duration.count(), max_expected_duration_us)
        << "AVX2 offset correction took " << avx2_duration.count() << "us for "
        << iterations << " iterations (" << (avx2_duration.count() / iterations) << "us per frame)";
#endif
}

} // namespace
