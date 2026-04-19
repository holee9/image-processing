/**
 * @file test_defect_correct.cpp
 * @brief TDD RED tests for SWU-1.3:
 *        xpe_defect_correct, xpe_defect_detect_runtime (REQ-P1A-024 to REQ-P1A-028)
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include <gtest/gtest.h>
#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <vector>

namespace {

class DefectCorrectTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 8;
    static constexpr uint32_t H = 8;

    std::vector<float>   imgPixels;
    std::vector<uint8_t> defectPixels;
    XpeImageBuffer img{};
    XpeImageBuffer defectMap{};

    void SetUp() override {
        imgPixels.assign(W * H, 1000.0f);
        defectPixels.assign(W * H, 0); // no defects by default

        img.data          = imgPixels.data();
        img.width         = W;
        img.height        = H;
        img.bitsAllocated = 32;
        img.bitsStored    = 32;
        img.format        = XPE_PIXEL_FLOAT32;
        img.dataSize      = imgPixels.size() * sizeof(float);

        defectMap.data          = defectPixels.data();
        defectMap.width         = W;
        defectMap.height        = H;
        defectMap.bitsAllocated = 8;
        defectMap.bitsStored    = 8;
        defectMap.format        = XPE_PIXEL_UINT8;
        defectMap.dataSize      = defectPixels.size();
    }
};

// REQ-P1A-024: no defects -> pixels unchanged
TEST_F(DefectCorrectTest, NoDefectsLeavesImageUnchanged) {
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&img, &defectMap, nullptr));
    const auto* out = static_cast<const float*>(img.data);
    EXPECT_NEAR(1000.0f, out[W + 1], 1e-3f); // interior pixel
}

// REQ-P1A-025: single defect pixel replaced by interpolated value
TEST_F(DefectCorrectTest, SingleDefectPixelIsReplaced) {
    // Set center pixel as defect with a very different value
    const uint32_t cx = 4, cy = 4;
    imgPixels[cy * W + cx] = 0.0f; // broken pixel
    defectPixels[cy * W + cx] = 1; // mark as defect

    ASSERT_EQ(XPE_OK, xpe_defect_correct(&img, &defectMap, nullptr));
    const auto* out = static_cast<const float*>(img.data);
    // Replaced value should be close to neighbours (1000.0f)
    EXPECT_NEAR(1000.0f, out[cy * W + cx], 100.0f);
}

// REQ-P1A-027: float32 format required
TEST_F(DefectCorrectTest, NullImgReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_defect_correct(nullptr, &defectMap, nullptr));
}

TEST_F(DefectCorrectTest, NullDefectMapReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_defect_correct(&img, nullptr, nullptr));
}

TEST_F(DefectCorrectTest, DimensionMismatchReturnsError) {
    XpeImageBuffer badMap = defectMap;
    badMap.width = W + 1;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_defect_correct(&img, &badMap, nullptr));
}

// REQ-P1A-012: defect cluster (2x2) handled by median filter
TEST_F(DefectCorrectTest, DefectClusterUsesMedianFilter) {
    // Create a 2x2 defect cluster with varying neighbors
    const uint32_t cx = 4, cy = 4;
    imgPixels[cy * W + cx] = 0.0f;       // center defect
    imgPixels[cy * W + cx + 1] = 0.0f;   // right neighbor defect
    imgPixels[(cy + 1) * W + cx] = 0.0f; // bottom neighbor defect
    imgPixels[(cy + 1) * W + cx + 1] = 0.0f; // bottom-right defect

    defectPixels[cy * W + cx] = 1;
    defectPixels[cy * W + cx + 1] = 1;
    defectPixels[(cy + 1) * W + cx] = 1;
    defectPixels[(cy + 1) * W + cx + 1] = 1;

    // Set neighbor values to test median
    imgPixels[(cy - 1) * W + cx] = 500.0f;   // top
    imgPixels[(cy + 2) * W + cx] = 1500.0f;  // bottom
    imgPixels[cy * W + (cx - 1)] = 800.0f;   // left
    imgPixels[cy * W + (cx + 2)] = 1200.0f;  // right

    ASSERT_EQ(XPE_OK, xpe_defect_correct(&img, &defectMap, nullptr));
    const auto* out = static_cast<const float*>(img.data);

    // All defects should be corrected to values near neighbors
    EXPECT_GT(out[cy * W + cx], 400.0f);
    EXPECT_LT(out[cy * W + cx], 1600.0f);
}

// REQ-P1A-012: edge defect uses only in-bounds neighbors
TEST_F(DefectCorrectTest, EdgeDefectUsesInBoundsNeighbors) {
    // Top-left corner defect
    imgPixels[0] = 0.0f;
    defectPixels[0] = 1;

    // Set in-bounds neighbors
    imgPixels[1] = 800.0f;          // right
    imgPixels[W] = 1200.0f;         // bottom

    ASSERT_EQ(XPE_OK, xpe_defect_correct(&img, &defectMap, nullptr));
    const auto* out = static_cast<const float*>(img.data);

    // Corner should be interpolated from in-bounds neighbors
    EXPECT_NEAR(1000.0f, out[0], 500.0f);
}

// REQ-P1A-012: correction recall >= 99% on synthetic defects
TEST_F(DefectCorrectTest, CorrectionRecallOnSyntheticDefects) {
    // Inject 100 random defects
    std::vector<uint32_t> defectPositions;
    for (int i = 0; i < 100; ++i) {
        uint32_t pos = (i * 17) % (W * H); // pseudo-random
        defectPositions.push_back(pos);
        imgPixels[pos] = 0.0f;
        defectPixels[pos] = 1;
    }

    ASSERT_EQ(XPE_OK, xpe_defect_correct(&img, &defectMap, nullptr));
    const auto* out = static_cast<const float*>(img.data);

    // Count corrected defects (value changed from 0.0f)
    int correctedCount = 0;
    for (uint32_t pos : defectPositions) {
        if (out[pos] > 100.0f) { // significantly different from broken value
            ++correctedCount;
        }
    }

    // REQ-P1A-012: recall >= 99%
    float recall = static_cast<float>(correctedCount) / static_cast<float>(defectPositions.size());
    EXPECT_GE(recall, 0.99f) << "Correction recall " << recall << " below 99% threshold";
}

/* === Runtime detection === */

TEST_F(DefectCorrectTest, DetectRuntimeNullImgReturnsError) {
    XpeImageBuffer outMap{};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_defect_detect_runtime(nullptr, &outMap, nullptr));
}

TEST_F(DefectCorrectTest, DetectRuntimeNullOutReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_defect_detect_runtime(&img, nullptr, nullptr));
}

TEST_F(DefectCorrectTest, DetectRuntimeCleanImageYieldsZeroDefects) {
    std::vector<uint8_t> outData(W * H, 0xFF); // initialize to non-zero
    XpeImageBuffer outMap{};
    outMap.data          = outData.data();
    outMap.width         = W;
    outMap.height        = H;
    outMap.bitsAllocated = 8;
    outMap.bitsStored    = 8;
    outMap.format        = XPE_PIXEL_UINT8;
    outMap.dataSize      = outData.size();

    ASSERT_EQ(XPE_OK, xpe_defect_detect_runtime(&img, &outMap, nullptr));
    // Uniform image should have no detected defects
    for (uint32_t i = 0; i < W * H; ++i)
        EXPECT_EQ(0, outData[i]) << "pixel " << i << " should not be defect";
}

} // namespace
