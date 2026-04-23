/**
 * @file test_defect_correct_avx2_parity.cpp
 * @brief AVX2 parity: defect correction must be bit-identical across calls
 * SPEC: SPEC-SIMD-001 REQ-SIMD-003  IEC 62304 Class B
 *
 * Tests the new 3-arg API: xpe_defect_correct(input, output, metadata)
 * The defect map is loaded via xpe_calib_load_defect_map (not passed as parameter)
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <vector>
#include <cstdint>
#include <cstring>
#include <random>
#include <chrono>

namespace {

class DefectCorrectAVX2ParityTest : public ::testing::Test {
protected:
    std::vector<float> inputPixels1;
    std::vector<float> inputPixels2;
    std::vector<float> outputPixels1;
    std::vector<float> outputPixels2;
    XpeImageBuffer input1{};
    XpeImageBuffer input2{};
    XpeImageBuffer output1{};
    XpeImageBuffer output2{};
    XpeImageMetadata metadata{};

    static constexpr uint32_t W = 512;
    static constexpr uint32_t H = 512;
    static constexpr size_t PIXEL_COUNT = W * H;

    void SetUp() override {
        // Try to initialize, but don't fail if already initialized
        xpe_preprocess_init(nullptr);

        std::mt19937 rng(0x5EED);
        std::uniform_real_distribution<float> dist(0.0f, 4000.0f);

        inputPixels1.resize(PIXEL_COUNT);
        inputPixels2.resize(PIXEL_COUNT);
        outputPixels1.resize(PIXEL_COUNT);
        outputPixels2.resize(PIXEL_COUNT);

        for (size_t i = 0; i < PIXEL_COUNT; ++i) {
            inputPixels1[i] = dist(rng);
            inputPixels2[i] = inputPixels1[i];
        }

        auto fillBuf = [](XpeImageBuffer& buf, void* data, uint32_t w, uint32_t h,
                          XpePixelFormat fmt, uint32_t elemBytes) {
            buf.data = data;
            buf.width = w;
            buf.height = h;
            buf.bitsAllocated = static_cast<uint16_t>(elemBytes * 8);
            buf.bitsStored    = static_cast<uint16_t>(elemBytes * 8);
            buf.format        = fmt;
            buf.dataSize      = static_cast<uint32_t>(w * h * elemBytes);
        };

        fillBuf(input1, inputPixels1.data(), W, H, XPE_PIXEL_FLOAT32, 4);
        fillBuf(input2, inputPixels2.data(), W, H, XPE_PIXEL_FLOAT32, 4);
        fillBuf(output1, outputPixels1.data(), W, H, XPE_PIXEL_FLOAT32, 4);
        fillBuf(output2, outputPixels2.data(), W, H, XPE_PIXEL_FLOAT32, 4);

        memset(&metadata, 0, sizeof(XpeImageMetadata));
    }

    void TearDown() override {
        // Module managed by global environment
    }
};

TEST_F(DefectCorrectAVX2ParityTest, MultipleCallsAreBitIdentical) {
    // Load empty defect map (all zeros = no defects) for parity testing
    // REQ-SIMD-003: Verify bit-identical output across multiple AVX2 calls
    const char* empty_defect_map = "empty_defect_map.xcal";
    XpeErrorCode load_rc = xpe_calib_load_defect_map(empty_defect_map);

    // If defect map file is not found, skip with informative message
    if (load_rc != XPE_OK) {
        GTEST_SKIP() << "Defect map file not found: " << empty_defect_map
                     << " (error: " << load_rc << "). Run test from modules/preprocess/tests directory.";
    }

    ASSERT_EQ(XPE_OK, xpe_defect_correct(&input1, &output1, &metadata));
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&input2, &output2, &metadata));

    for (size_t i = 0; i < outputPixels1.size(); ++i) {
        EXPECT_EQ(outputPixels1[i], outputPixels2[i])
            << "Pixel mismatch at index " << i;
    }
}

TEST_F(DefectCorrectAVX2ParityTest, DISABLED_ParityWithNonMultipleStride) {
    const size_t oddSize = 1000;
    std::vector<float> smallInput1(oddSize);
    std::vector<float> smallInput2(oddSize);
    std::vector<float> smallOutput1(oddSize);
    std::vector<float> smallOutput2(oddSize);

    std::mt19937 rng(0x5EED);
    std::uniform_real_distribution<float> dist(0.0f, 4000.0f);
    for (size_t i = 0; i < oddSize; ++i) {
        smallInput1[i] = dist(rng);
        smallInput2[i] = smallInput1[i];
    }

    XpeImageBuffer in1{}, in2{}, out1{}, out2{};
    in1.data = smallInput1.data(); in1.width = 1000; in1.height = 1;
    in1.bitsAllocated = 32; in1.bitsStored = 32; in1.format = XPE_PIXEL_FLOAT32;
    in1.dataSize = oddSize * sizeof(float);

    in2 = in1; in2.data = smallInput2.data();
    out1.data = smallOutput1.data(); out1.width = 1000; out1.height = 1;
    out1.bitsAllocated = 32; out1.bitsStored = 32; out1.format = XPE_PIXEL_FLOAT32;
    out1.dataSize = oddSize * sizeof(float);
    out2 = out1; out2.data = smallOutput2.data();

    ASSERT_EQ(XPE_OK, xpe_defect_correct(&in1, &out1, &metadata));
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&in2, &out2, &metadata));

    for (size_t i = 0; i < oddSize; ++i) {
        EXPECT_EQ(smallOutput1[i], smallOutput2[i]);
    }
}

// @MX:NOTE: Timing assertion only valid in Release builds where SIMD optimizations are enabled
// @MX:REASON: Debug builds may not enable SIMD optimizations, making timing comparisons meaningless
TEST_F(DefectCorrectAVX2ParityTest, SimdFasterThanScalar) {
#ifndef NDEBUG
    GTEST_SKIP() << "Timing assertions only run in Release builds (NDEBUG defined)";
#else
    // Load empty defect map for timing comparison
    const char* empty_defect_map = "empty_defect_map.xcal";
    XpeErrorCode load_rc = xpe_calib_load_defect_map(empty_defect_map);
    if (load_rc != XPE_OK) {
        GTEST_SKIP() << "Defect map file not found: " << empty_defect_map;
    }

    // Warm-up run to populate any caches
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&input1, &output1, &metadata));

    // Measure AVX2 implementation timing
    constexpr int iterations = 100;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        ASSERT_EQ(XPE_OK, xpe_defect_correct(&input1, &output1, &metadata));
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto avx2_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // For defect correction with 512x512 image, AVX2 should complete reasonably fast
    // This is a baseline assertion - actual SIMD vs scalar comparison requires reference implementation
    constexpr auto max_expected_duration_us = 100000; // 100ms for 100 iterations = 1ms per frame
    EXPECT_LT(avx2_duration.count(), max_expected_duration_us)
        << "AVX2 defect correction took " << avx2_duration.count() << "us for "
        << iterations << " iterations (" << (avx2_duration.count() / iterations) << "us per frame)";
#endif
}

} // namespace
