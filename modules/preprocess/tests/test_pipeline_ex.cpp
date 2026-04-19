/**
 * @file test_pipeline_ex.cpp
 * @brief TDD tests for SWU-1.11/1.12: Pre-loaded state pipeline and batch processing
 *        Validates calibration state load/release, pipeline_ex, and batch API.
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include <gtest/gtest.h>
#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <vector>
#include <cstdio>
#include <filesystem>
#include <chrono>
#include <cstring>

namespace fs = std::filesystem;

namespace {

class PipelineExTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 8;
    static constexpr uint32_t H = 8;

    fs::path tmpDir;
    fs::path offsetFile;
    fs::path gainFile;
    fs::path defectFile;

    void SetUp() override {
        tmpDir = fs::temp_directory_path() / "xpe_pipeline_ex_test";
        fs::create_directories(tmpDir);

        offsetFile = tmpDir / "offset.xcal";
        gainFile   = tmpDir / "gain.xcal";
        defectFile = tmpDir / "defect.xcal";

        const uint64_t expiry = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()
        ) + 365ULL * 24 * 3600 * 1000;

        // Offset map: uint16, value = 100
        {
            std::vector<uint16_t> data(W * H, 100);
            XpeImageBuffer buf{};
            buf.data = data.data();
            buf.width = W; buf.height = H;
            buf.bitsAllocated = 16; buf.bitsStored = 16;
            buf.format = XPE_PIXEL_UINT16;
            buf.dataSize = data.size() * sizeof(uint16_t);
            ASSERT_EQ(XPE_OK, xpe_calib_save(&buf, offsetFile.string().c_str(), expiry, nullptr));
        }

        // Gain map: float32, value = 2.0
        {
            std::vector<float> data(W * H, 2.0f);
            XpeImageBuffer buf{};
            buf.data = data.data();
            buf.width = W; buf.height = H;
            buf.bitsAllocated = 32; buf.bitsStored = 32;
            buf.format = XPE_PIXEL_FLOAT32;
            buf.dataSize = data.size() * sizeof(float);
            ASSERT_EQ(XPE_OK, xpe_calib_save(&buf, gainFile.string().c_str(), expiry, nullptr));
        }

        // Defect map: uint8, all zeros (no defects)
        {
            std::vector<uint8_t> data(W * H, 0);
            XpeImageBuffer buf{};
            buf.data = data.data();
            buf.width = W; buf.height = H;
            buf.bitsAllocated = 8; buf.bitsStored = 8;
            buf.format = XPE_PIXEL_UINT8;
            buf.dataSize = data.size();
            ASSERT_EQ(XPE_OK, xpe_calib_save(&buf, defectFile.string().c_str(), expiry, nullptr));
        }
    }

    void TearDown() override {
        fs::remove_all(tmpDir);
    }

    /**
     * @brief Create a test image (uint16, all pixels = value)
     */
    std::vector<uint16_t> makeTestImage(uint16_t value, XpeImageBuffer& out) {
        std::vector<uint16_t> data(W * H, value);
        out.data          = data.data();
        out.width         = W;
        out.height        = H;
        out.bitsAllocated = 16;
        out.bitsStored    = 16;
        out.format        = XPE_PIXEL_UINT16;
        out.dataSize      = data.size() * sizeof(uint16_t);
        return data;
    }
};

// --- Calibration State Load/Release ---

TEST_F(PipelineExTest, StateLoadPopulatesAllMaps) {
    XpeCalibrationState state = {};
    ASSERT_EQ(XPE_OK, xpe_calib_state_load(&state, tmpDir.string().c_str()));

    EXPECT_TRUE(state.offsetLoaded);
    EXPECT_TRUE(state.gainLoaded);
    EXPECT_TRUE(state.defectLoaded);

    EXPECT_NE(nullptr, state.offsetMap.data);
    EXPECT_NE(nullptr, state.gainMap.data);
    EXPECT_NE(nullptr, state.defectMap.data);

    EXPECT_EQ(W, state.offsetMap.width);
    EXPECT_EQ(H, state.offsetMap.height);

    xpe_calib_state_release(&state);
}

TEST_F(PipelineExTest, StateReleaseFreesMemory) {
    XpeCalibrationState state = {};
    ASSERT_EQ(XPE_OK, xpe_calib_state_load(&state, tmpDir.string().c_str()));

    xpe_calib_state_release(&state);

    EXPECT_EQ(nullptr, state.offsetMap.data);
    EXPECT_EQ(nullptr, state.gainMap.data);
    EXPECT_EQ(nullptr, state.defectMap.data);
    EXPECT_FALSE(state.offsetLoaded);
    EXPECT_FALSE(state.gainLoaded);
    EXPECT_FALSE(state.defectLoaded);
}

TEST_F(PipelineExTest, StateReleaseOnZeroInitIsSafe) {
    XpeCalibrationState state = {};
    // Should not crash
    xpe_calib_state_release(&state);
    EXPECT_EQ(nullptr, state.offsetMap.data);
}

TEST_F(PipelineExTest, StateLoadNullReturnsError) {
    XpeCalibrationState state = {};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calib_state_load(nullptr, tmpDir.string().c_str()));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calib_state_load(&state, nullptr));
}

TEST_F(PipelineExTest, StateLoadNonexistentDirSkipsMaps) {
    XpeCalibrationState state = {};
    // Non-existent directory — all loads should fail gracefully
    EXPECT_EQ(XPE_OK, xpe_calib_state_load(&state, "/nonexistent_dir_xpe_test"));

    EXPECT_FALSE(state.offsetLoaded);
    EXPECT_FALSE(state.gainLoaded);
    EXPECT_FALSE(state.defectLoaded);

    xpe_calib_state_release(&state);
}

// --- Pipeline EX (with pre-loaded state) ---

TEST_F(PipelineExTest, PipelineExWithState) {
    XpeCalibrationState state = {};
    ASSERT_EQ(XPE_OK, xpe_calib_state_load(&state, tmpDir.string().c_str()));

    // Create test image (uint16, value = 200)
    XpeImageBuffer img;
    auto imgData = makeTestImage(200, img);

    XpeImageMetadata meta = {};
    meta.flags = 0;

    // Run pipeline with bypass for stages without calibration
    const char* config = "{\"bypassReadout\":true,\"bypassTemp\":true,"
                          "\"bypassNonlinearity\":true,\"bypassBinning\":true,"
                          "\"bypassGhost\":true}";
    XpeErrorCode rc = xpe_preprocess_pipeline_ex(&img, &meta, &state, nullptr, config);
    ASSERT_EQ(XPE_OK, rc);

    // Verify flags were set
    EXPECT_TRUE(meta.flags & XPE_FLAG_OFFSET_CORRECTED);
    EXPECT_TRUE(meta.flags & XPE_FLAG_GAIN_CORRECTED);

    // After offset correction (200 - 100 = 100) then gain (100 * 1/2.0 = 50.0)
    // Note: gain_correct allocates new float buffer
    EXPECT_EQ(XPE_PIXEL_FLOAT32, img.format);
    const float* result = static_cast<const float*>(img.data);
    EXPECT_NEAR(50.0f, result[0], 0.01f);

    // Clean up: gain_correct allocated a new buffer
    std::free(img.data);
    xpe_calib_state_release(&state);
}

TEST_F(PipelineExTest, PipelineExNullImgReturnsError) {
    XpeCalibrationState state = {};
    XpeImageMetadata meta = {};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_preprocess_pipeline_ex(nullptr, &meta, &state, nullptr, nullptr));
    xpe_calib_state_release(&state);
}

TEST_F(PipelineExTest, PipelineExNullStateSkipsCalibration) {
    XpeImageBuffer img;
    auto imgData = makeTestImage(200, img);
    XpeImageMetadata meta = {};

    const char* config = "{\"bypassReadout\":true,\"bypassTemp\":true,"
                          "\"bypassNonlinearity\":true,\"bypassBinning\":true,"
                          "\"bypassGhost\":true,\"bypassOffset\":true,"
                          "\"bypassGain\":true,\"bypassDefect\":true}";
    XpeErrorCode rc = xpe_preprocess_pipeline_ex(&img, &meta, nullptr, nullptr, config);
    EXPECT_EQ(XPE_OK, rc);
}

// --- Batch Processing ---

TEST_F(PipelineExTest, BatchProcessesMultipleFrames) {
    static constexpr uint32_t kBatchSize = 3;

    // Create batch images
    std::vector<std::vector<uint16_t>> imgDataVec;
    std::vector<XpeImageBuffer> images(kBatchSize);
    std::vector<XpeImageMetadata> metas(kBatchSize);

    for (uint32_t i = 0; i < kBatchSize; ++i) {
        imgDataVec.push_back(std::vector<uint16_t>(W * H, 200));
        auto& img = images[i];
        img.data          = imgDataVec.back().data();
        img.width         = W;
        img.height        = H;
        img.bitsAllocated = 16;
        img.bitsStored    = 16;
        img.format        = XPE_PIXEL_UINT16;
        img.dataSize      = W * H * sizeof(uint16_t);

        metas[i].flags = 0;
    }

    const char* config = "{\"bypassReadout\":true,\"bypassTemp\":true,"
                          "\"bypassNonlinearity\":true,\"bypassBinning\":true,"
                          "\"bypassGhost\":true}";

    XpeErrorCode rc = xpe_preprocess_pipeline_batch(
        images.data(), kBatchSize, metas.data(),
        tmpDir.string().c_str(), nullptr, config);
    ASSERT_EQ(XPE_OK, rc);

    // Verify all frames were processed
    for (uint32_t i = 0; i < kBatchSize; ++i) {
        EXPECT_TRUE(metas[i].flags & XPE_FLAG_OFFSET_CORRECTED)
            << "Frame " << i << " missing offset flag";
        EXPECT_TRUE(metas[i].flags & XPE_FLAG_GAIN_CORRECTED)
            << "Frame " << i << " missing gain flag";
        EXPECT_EQ(XPE_PIXEL_FLOAT32, images[i].format)
            << "Frame " << i << " should be float32 after gain correction";

        // After offset (200-100=100) then gain (100/2.0=50.0)
        const float* result = static_cast<const float*>(images[i].data);
        EXPECT_NEAR(50.0f, result[0], 0.01f)
            << "Frame " << i << " pixel value mismatch";

        // Clean up float buffer allocated by gain correction
        std::free(images[i].data);
    }
}

TEST_F(PipelineExTest, BatchNullImagesReturnsError) {
    XpeImageMetadata meta = {};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_preprocess_pipeline_batch(nullptr, 1, &meta, tmpDir.string().c_str(),
                                             nullptr, nullptr));
}

TEST_F(PipelineExTest, BatchZeroCountReturnsError) {
    XpeImageBuffer img = {};
    XpeImageMetadata meta = {};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_preprocess_pipeline_batch(&img, 0, &meta, tmpDir.string().c_str(),
                                             nullptr, nullptr));
}

TEST_F(PipelineExTest, BatchNullMetasReturnsError) {
    XpeImageBuffer img = {};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_preprocess_pipeline_batch(&img, 1, nullptr, tmpDir.string().c_str(),
                                             nullptr, nullptr));
}

TEST_F(PipelineExTest, BatchSingleFrameWorks) {
    XpeImageBuffer img;
    auto imgData = makeTestImage(200, img);
    XpeImageMetadata meta = {};
    meta.flags = 0;

    const char* config = "{\"bypassReadout\":true,\"bypassTemp\":true,"
                          "\"bypassNonlinearity\":true,\"bypassBinning\":true,"
                          "\"bypassGhost\":true}";

    XpeErrorCode rc = xpe_preprocess_pipeline_batch(
        &img, 1, &meta, tmpDir.string().c_str(), nullptr, config);
    ASSERT_EQ(XPE_OK, rc);

    EXPECT_TRUE(meta.flags & XPE_FLAG_OFFSET_CORRECTED);
    EXPECT_TRUE(meta.flags & XPE_FLAG_GAIN_CORRECTED);
    EXPECT_EQ(XPE_PIXEL_FLOAT32, img.format);

    std::free(img.data);
}

TEST_F(PipelineExTest, BatchContinuesOnError) {
    // Create a batch where the middle frame has invalid dimensions
    static constexpr uint32_t kBatchSize = 3;

    std::vector<std::vector<uint16_t>> imgDataVec;
    std::vector<XpeImageBuffer> images(kBatchSize);
    std::vector<XpeImageMetadata> metas(kBatchSize);

    for (uint32_t i = 0; i < kBatchSize; ++i) {
        imgDataVec.push_back(std::vector<uint16_t>(W * H, 200));
        auto& img = images[i];
        img.data          = imgDataVec.back().data();
        img.width         = W;
        img.height        = H;
        img.bitsAllocated = 16;
        img.bitsStored    = 16;
        img.format        = XPE_PIXEL_UINT16;
        img.dataSize      = W * H * sizeof(uint16_t);
        metas[i].flags = 0;
    }

    // Corrupt middle image dimensions to trigger offset_correct dimension mismatch
    images[1].width = 0;

    const char* config = "{\"bypassReadout\":true,\"bypassTemp\":true,"
                          "\"bypassNonlinearity\":true,\"bypassBinning\":true,"
                          "\"bypassGhost\":true}";

    // Batch should return error from the failing frame but process others
    XpeErrorCode rc = xpe_preprocess_pipeline_batch(
        images.data(), kBatchSize, metas.data(),
        tmpDir.string().c_str(), nullptr, config);
    // First error encountered should be returned
    EXPECT_NE(XPE_OK, rc);

    // Frame 0 and 2 should still have been processed
    EXPECT_TRUE(metas[0].flags & XPE_FLAG_OFFSET_CORRECTED);
    EXPECT_TRUE(metas[2].flags & XPE_FLAG_OFFSET_CORRECTED);

    // Clean up float buffers from successful frames
    if (images[0].format == XPE_PIXEL_FLOAT32) std::free(images[0].data);
    // images[1] failed, no new buffer
    if (images[2].format == XPE_PIXEL_FLOAT32) std::free(images[2].data);
}

} // namespace
