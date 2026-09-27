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

/* ---------------------------------------------------------------------------
 * QA-A-146 (#209): in-place is a supported call shape, and it must give the
 * SAME answer as the out-of-place one.
 *
 * WHY THIS PAIR AND NOT A SINGLE TEST. The function keeps an uncorrected
 * snapshot so a neighbour read never sees an already-corrected pixel. Reading
 * the input directly is enough for that -- the input is never written --
 * UNLESS the caller passed one buffer for both, which two call sites do
 * (test_integration.cpp:100, :131). The snapshot is therefore taken only when
 * the buffers overlap, and these two tests are what makes that branch
 * checkable: same input, two call shapes, identical output.
 *
 * A test that only ran the in-place case and asserted "rc == XPE_OK" would
 * pass against a version that skipped the snapshot and produced order-
 * dependent values, because nothing would compare them to anything. The
 * out-of-place run is the independently derived answer.
 *
 * The defects are adjacent so both the 4-neighbour mean and the 3x3 median
 * path run; a single isolated defect would leave the cluster path untested.
 * ------------------------------------------------------------------------- */
TEST_F(DefectCorrectTest, InPlaceMatchesOutOfPlace) {
    // A cluster (two adjacent) plus an isolated defect, all interior.
    defectPixels[5 * W + 5] = 1;
    defectPixels[5 * W + 6] = 1;
    defectPixels[9 * W + 9] = 1;
    // A solid 3x3 block: its centre has no valid 4-neighbour, so the r=1..3
    // ring fallback in xpe_interpolate_pixel runs -- the widest read radius
    // this function has, and the one most likely to reach a written pixel.
    for (uint32_t dy = 0; dy < 3; ++dy)
        for (uint32_t dx = 0; dx < 3; ++dx)
            defectPixels[(20 + dy) * W + (20 + dx)] = 1;
    for (uint32_t i = 0; i < W * H; ++i) {
        imgPixels[i] = 1000.0f + static_cast<float>(i % 37);
    }
    loadDefectMap();

    // Out-of-place: distinct buffers.
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&img, &output, &metadata));
    const std::vector<float> expected = outPixels;

    // In-place: one buffer for both, the shape test_integration.cpp uses.
    std::vector<float> both = imgPixels;
    XpeImageBuffer buf = img;
    buf.data     = both.data();
    buf.dataSize = both.size() * sizeof(float);
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&buf, &buf, &metadata));

    for (size_t i = 0; i < both.size(); ++i) {
        ASSERT_EQ(expected[i], both[i])
            << "in-place and out-of-place diverged at index " << i
            << " (" << expected[i] << " vs " << both[i] << ")";
    }
}

/** CONTROL for the branch: the out-of-place path, which now SKIPS the
 *  snapshot, still corrects. Without this, deleting the correction entirely
 *  would leave the pair above passing -- both shapes would be equally wrong. */
TEST_F(DefectCorrectTest, OutOfPlaceStillCorrectsWithoutTheSnapshot) {
    defectPixels[7 * W + 7] = 1;
    for (uint32_t i = 0; i < W * H; ++i) {
        imgPixels[i] = 1000.0f + static_cast<float>(i % 37);
    }
    // The defect reads as a STUCK pixel, not as another ramp sample. With the
    // ramp value in place the 4-neighbour mean happens to equal the centre
    // (1009 either way), so "the value changed" could not distinguish a
    // correction from a plain copy -- the first version of this test asserted
    // exactly that and failed for that reason.
    imgPixels[7 * W + 7] = 5.0f;
    loadDefectMap();

    ASSERT_EQ(XPE_OK, xpe_defect_correct(&img, &output, &metadata));
    const auto* out = static_cast<const float*>(output.data);

    // The defect's own value must have been replaced by its neighbours' mean,
    // computed here from the INPUT rather than by re-running the kernel.
    const float mean = (imgPixels[7 * W + 6] + imgPixels[7 * W + 8] +
                        imgPixels[6 * W + 7] + imgPixels[8 * W + 7]) / 4.0f;
    EXPECT_FLOAT_EQ(mean, out[7 * W + 7]);
    EXPECT_NE(imgPixels[7 * W + 7], out[7 * W + 7]) << "the defect was not corrected";
}

/** QA-A-146c (#209): the aliasing contract admits exactly two shapes --
 *  identical buffers, or fully disjoint ones. A PARTIAL overlap is refused
 *  before anything is written.
 *
 *  Not because it is known to produce a wrong answer, but because no caller
 *  does it and nothing measures whether it would be right; an error code
 *  makes the violation observable instead of letting it run into UB in
 *  silence. This test is what keeps the refusal from being dead code. */
TEST_F(DefectCorrectTest, PartiallyOverlappingBuffersAreRefused) {
    defectPixels[7 * W + 7] = 1;
    loadDefectMap();

    // One allocation, two windows into it offset by a single pixel.
    std::vector<float> shared(W * H + 1, 1000.0f);
    XpeImageBuffer in = img;
    in.data     = shared.data();
    in.dataSize = W * H * sizeof(float);
    XpeImageBuffer out = output;
    out.data     = shared.data() + 1;   // overlaps `in` everywhere but one end
    out.dataSize = W * H * sizeof(float);

    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_defect_correct(&in, &out, &metadata));

    // The two admitted shapes still pass, so the check is not simply refusing
    // everything -- the control for the refusal above.
    EXPECT_EQ(XPE_OK, xpe_defect_correct(&img, &output, &metadata));
    std::vector<float> same = imgPixels;
    XpeImageBuffer buf = img;
    buf.data     = same.data();
    buf.dataSize = same.size() * sizeof(float);
    EXPECT_EQ(XPE_OK, xpe_defect_correct(&buf, &buf, &metadata));
}

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
