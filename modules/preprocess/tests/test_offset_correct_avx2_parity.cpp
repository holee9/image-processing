/**
 * @file test_offset_correct_avx2_parity.cpp
 * @brief TDD RED tests for AVX2 parity: multiple calls with same input must be bit-identical
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 *
 * Tests the new 3-arg API: xpe_offset_correct(input, output, metadata)
 * Note: Tests use existing calibration loaded via xpe_calib_load_offset
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <random>

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
    const char* offsetPath = "test_offset_avx2_parity_offset.xcal";

    void SetUp() override {
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));

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

        std::vector<float> offsetMap(pixelCount);
        for (size_t i = 0; i < pixelCount; ++i) {
            offsetMap[i] = static_cast<float>(i % 257) + 0.25f;
        }
        loadOffsetMap(offsetMap, 1024, 768);
    }

    void TearDown() override {
        std::remove(offsetPath);
        std::remove("test_offset_avx2_parity_offset.xcal.tmp");
        xpe_preprocess_shutdown();
    }

    void loadOffsetMap(const std::vector<float>& values, uint32_t width, uint32_t height) {
        std::remove(offsetPath);
        std::remove("test_offset_avx2_parity_offset.xcal.tmp");

        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(XCAL_TYPE_OFFSET);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = width;
        hdr.height = height;
        hdr.payload_len = static_cast<uint64_t>(values.size() * sizeof(float));

        ASSERT_EQ(write_xcal_file(offsetPath, hdr, nullptr, 0,
                                  reinterpret_cast<const uint8_t*>(values.data()),
                                  hdr.payload_len),
                  XPE_OK);
        ASSERT_EQ(xpe_calib_load_offset(offsetPath), XPE_OK);
    }
};

// ===========================================================================
// QA-A-73 (#160): the shipped path against the SCALAR REFERENCE.
//
// WHAT THIS FILE ASSERTED BEFORE. The two tests below require repeated calls to
// agree with each other. That is determinism, not correctness: a kernel that is
// consistently wrong passes both, and so does one that never runs. The gain
// suite had the same shape and QA-A-72 measured the consequence -- breaking the
// AVX2 multiply made only the new reference comparison fail while both
// determinism tests stayed green.
//
// WHY IT MATTERS NOW. QA-A-73 removed xpe_has_avx2(), the runtime probe that
// chose between the vector and scalar forms. On x86 the vector form is now the
// only path the library takes, so the scalar form's job changed: it is no longer
// a fallback anyone can reach, it is the independent implementation this test
// compares against. If this test goes, that comparison goes with it silently --
// see the note on xpe_offset_apply_scalar_reference.
//
// TOLERANCE: EXACT. Not borrowed from the gain suite -- AC-GAIN-004 allows 1 ULP
// because gain correction multiplies floats, and this does not: REQ-P1A-010
// states the vector kernel is "bit-identical to scalar version", the output is
// uint16, and there is no ULP constant anywhere in offset_correct.cpp. An exact
// comparison is what the requirement asks for. (That requirement also says
// "verified by test suite". Until this test, it was not.)
// ===========================================================================
TEST_F(OffsetCorrectAVX2ParityTest, ShippedPathMatchesTheScalarReference) {
    const uint32_t w = 1024u, h = 768u;
    const size_t n = static_cast<size_t>(w) * h;

    // An offset map that exercises all three arms of the scalar rule: values it
    // subtracts cleanly, values that drive the result below 0 (clamped), and the
    // rounding at .5. A constant map would leave the clamps untested.
    std::vector<float> offsets(n);
    for (size_t i = 0; i < n; ++i) {
        offsets[i] = static_cast<float>(i % 5000u) * 1.5f - 500.0f;   // -500 .. 6998.5
    }
    loadOffsetMap(offsets, w, h);

    ASSERT_EQ(XPE_OK, xpe_offset_correct(&input1, &output1, &metadata));

    std::vector<uint16_t> reference(n, 0u);
    xpe_offset_apply_scalar_reference(inputPixels1.data(), offsets.data(),
                                      reference.data(), n);

    size_t differing = 0, firstBad = 0;
    int32_t worstGap = 0;
    for (size_t i = 0; i < n; ++i) {
        if (outputPixels1[i] == reference[i]) continue;
        if (differing == 0) firstBad = i;
        ++differing;
        const int32_t gap = static_cast<int32_t>(outputPixels1[i]) -
                            static_cast<int32_t>(reference[i]);
        const int32_t mag = (gap < 0) ? -gap : gap;
        if (mag > worstGap) worstGap = mag;
    }

    // Printed, not assumed: a clamp or rounding difference would show as a small
    // worst_gap over many pixels, a kernel error as a large one.
    std::printf("[offset-parity] %ux%u  differing=%zu of %zu  worst_gap=%d\n",
                w, h, differing, n, worstGap);
    std::fflush(stdout);

    EXPECT_EQ(0u, differing)
        << differing << " pixels differ from the scalar reference, first at index "
        << firstBad << " (shipped " << outputPixels1[firstBad]
        << " vs reference " << reference[firstBad] << "); REQ-P1A-010 requires"
           " bit-identical";
}

TEST_F(OffsetCorrectAVX2ParityTest, MultipleCallsAreBitIdentical) {
    ASSERT_EQ(XPE_OK, xpe_offset_correct(&input1, &output1, &metadata));
    ASSERT_EQ(XPE_OK, xpe_offset_correct(&input2, &output2, &metadata));

    for (size_t i = 0; i < outputPixels1.size(); ++i) {
        EXPECT_EQ(outputPixels1[i], outputPixels2[i])
            << "Pixel mismatch at index " << i;
    }
}

TEST_F(OffsetCorrectAVX2ParityTest, ParityWithNonMultipleStride) {
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

    std::vector<float> offsetMap(oddSize);
    for (size_t i = 0; i < oddSize; ++i) {
        offsetMap[i] = static_cast<float>(i % 31) + 0.75f;
    }
    loadOffsetMap(offsetMap, 1000, 1);

    ASSERT_EQ(XPE_OK, xpe_offset_correct(&in1, &out1, &metadata));
    ASSERT_EQ(XPE_OK, xpe_offset_correct(&in2, &out2, &metadata));

    for (size_t i = 0; i < oddSize; ++i) {
        EXPECT_EQ(smallOutput1[i], smallOutput2[i]);
    }
}

} // namespace
