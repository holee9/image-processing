/**
 * @file test_defect_correct.cpp
 * @brief TDD RED tests for SWU-1.3:
 *        xpe_defect_correct, xpe_defect_detect_runtime (REQ-P1A-024 to REQ-P1A-028)
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 *
 * #117 decision B (QA-A-20): the defect map is not a parameter. It is loaded
 * into the global calibration by xpe_calib_load_defect_map() and the entry
 * point is xpe_defect_correct(input, output, metadata) -- argument 2 is the
 * output buffer. The old calls passed the map there; the types matched, so the
 * compiler stayed silent and the cases failed at run time.
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

class DefectCorrectTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 32;
    static constexpr uint32_t H = 32;

    std::vector<float>   imgPixels;
    std::vector<float>   outPixels;
    std::vector<uint8_t> defectPixels;
    XpeImageBuffer   img{};
    XpeImageBuffer   output{};
    XpeImageMetadata metadata{};
    const char* defectPath = "test_defect_correct_defect.xcal";

    void SetUp() override {
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));

        imgPixels.assign(W * H, 1000.0f);
        outPixels.assign(W * H, 0.0f);
        defectPixels.assign(W * H, 0); // no defects by default

        img.data          = imgPixels.data();
        img.width         = W;
        img.height        = H;
        img.bitsAllocated = 32;
        img.bitsStored    = 32;
        img.format        = XPE_PIXEL_FLOAT32;
        img.dataSize      = imgPixels.size() * sizeof(float);

        output.data          = outPixels.data();
        output.width         = W;
        output.height        = H;
        output.bitsAllocated = 32;
        output.bitsStored    = 32;
        output.format        = XPE_PIXEL_FLOAT32;
        output.dataSize      = outPixels.size() * sizeof(float);
    }

    void TearDown() override {
        std::remove(defectPath);
        std::remove("test_defect_correct_defect.xcal.tmp");
        xpe_preprocess_shutdown();
    }

    // Publishes the current defectPixels into the global calibration. Cases
    // mutate defectPixels first, so this runs immediately before the call.
    void loadDefectMap() {
        std::remove(defectPath);
        std::remove("test_defect_correct_defect.xcal.tmp");

        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version      = XCAL_VERSION;
        hdr.type         = static_cast<uint32_t>(XCAL_TYPE_DEFECT);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT8_MASK);
        hdr.width        = W;
        hdr.height       = H;
        hdr.payload_len  = static_cast<uint64_t>(defectPixels.size());

        ASSERT_EQ(XPE_OK,
                  write_xcal_file(defectPath, hdr, nullptr, 0,
                                  defectPixels.data(), hdr.payload_len));
        ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(defectPath));
    }
};

// REQ-P1A-024: no defects -> pixels unchanged
TEST_F(DefectCorrectTest, NoDefectsLeavesImageUnchanged) {
    loadDefectMap();
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&img, &output, &metadata));
    const auto* out = static_cast<const float*>(output.data);
    EXPECT_NEAR(1000.0f, out[W + 1], 1e-3f); // interior pixel
}

// REQ-P1A-025: single defect pixel replaced by interpolated value
TEST_F(DefectCorrectTest, SingleDefectPixelIsReplaced) {
    // Set center pixel as defect with a very different value
    const uint32_t cx = 4, cy = 4;
    imgPixels[cy * W + cx] = 0.0f; // broken pixel
    defectPixels[cy * W + cx] = 1; // mark as defect

    loadDefectMap();
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&img, &output, &metadata));
    const auto* out = static_cast<const float*>(output.data);
    // Replaced value should be close to neighbours (1000.0f)
    EXPECT_NEAR(1000.0f, out[cy * W + cx], 100.0f);
}

// REQ-P1A-027: float32 format required
TEST_F(DefectCorrectTest, NullInputReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_defect_correct(nullptr, &output, &metadata));
}

TEST_F(DefectCorrectTest, NullOutputReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_defect_correct(&img, nullptr, &metadata));
}

// Output dimensions must match the input.
TEST_F(DefectCorrectTest, DimensionMismatchReturnsError) {
    output.width = W + 1;
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, xpe_defect_correct(&img, &output, &metadata));
}

// #117: with the module up but no defect map loaded, the call reports the
// missing calibration rather than pretending the module was never initialized.
TEST_F(DefectCorrectTest, DefectMapNotLoadedReturnsCalibNotLoaded) {
    EXPECT_EQ(XPE_ERR_CALIB_NOT_LOADED, xpe_defect_correct(&img, &output, &metadata));
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

    loadDefectMap();
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&img, &output, &metadata));
    const auto* out = static_cast<const float*>(output.data);

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

    loadDefectMap();
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&img, &output, &metadata));
    const auto* out = static_cast<const float*>(output.data);

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

    loadDefectMap();
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&img, &output, &metadata));
    const auto* out = static_cast<const float*>(output.data);

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
              xpe_defect_detect_runtime(nullptr, nullptr, &outMap));
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

    ASSERT_EQ(XPE_OK, xpe_defect_detect_runtime(&img, nullptr, &outMap));
    // Uniform image should have no detected defects
    for (uint32_t i = 0; i < W * H; ++i)
        EXPECT_EQ(0, outData[i]) << "pixel " << i << " should not be defect";
}

} // namespace
