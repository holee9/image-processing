/**
 * @file test_pipeline_ex.cpp
 * @brief TDD tests for SWU-1.11/1.12: Pre-loaded state pipeline and batch processing
 *        Validates calibration state load/release, pipeline_ex, and batch API.
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"
#include <string>

#include <vector>
#include <cstdio>
#include <filesystem>
#include <chrono>
#include <cstring>

namespace fs = std::filesystem;

namespace {

// QA-A-15 (#120): xpe_calib_save no longer takes a buffer -- it saves the
// calibration currently loaded in the module. These fixtures need arbitrary
// maps on disk, so they write XCal v1 files directly, the same way
// test_offset_correct.cpp does.
static void writeXCalFixture(const std::string& path, XCalType type,
                             XCalPixelFormat fmt, uint32_t w, uint32_t h,
                             const void* payload, uint64_t payloadLen,
                             uint64_t expiryMs) {
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version          = XCAL_VERSION;
    hdr.type             = static_cast<uint32_t>(type);
    hdr.pixel_format     = static_cast<uint32_t>(fmt);
    hdr.width            = w;
    hdr.height           = h;
    hdr.expiry_epoch_ms  = static_cast<int64_t>(expiryMs);
    hdr.payload_len      = payloadLen;

    ASSERT_EQ(XPE_OK,
              write_xcal_file(path.c_str(), hdr, nullptr, 0,
                              static_cast<const uint8_t*>(payload), payloadLen));
}

class PipelineExTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 8;
    static constexpr uint32_t H = 8;

    fs::path tmpDir;
    fs::path offsetFile;
    fs::path gainFile;
    fs::path defectFile;

    void SetUp() override {
        // #117 decision B / QA-A-20: the correction entry points now check the
        // module lifecycle flag for real, so the suite has to initialize.
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));

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
            // XCal v1 requires OFFSET payloads to be FLOAT32 (xcal_validator.cpp:78);
            // the retired xpe_calib_save() converted, a direct write must not assume.
            std::vector<float> data(W * H, 100.0f);
            writeXCalFixture(offsetFile.string(), XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32, W, H,
                             data.data(), data.size() * sizeof(float), expiry);
        }

        // Gain map: float32, value = 2.0
        {
            std::vector<float> data(W * H, 2.0f);
            writeXCalFixture(gainFile.string(), XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32, W, H,
                             data.data(), data.size() * sizeof(float), expiry);
        }

        // Defect map: uint8, all zeros (no defects)
        {
            std::vector<uint8_t> data(W * H, 0);
            writeXCalFixture(defectFile.string(), XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, W, H,
                             data.data(), data.size() * sizeof(uint8_t), expiry);
        }
    }

    void TearDown() override {
        xpe_preprocess_shutdown();
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

    // #117 decision B: xpe_calib_state_load loads into the global calibration and
    // leaves the struct's map buffers empty by contract. The observable effect is
    // that a correction call now succeeds instead of reporting CALIB_NOT_LOADED.
    XpeImageBuffer   probeIn{}, probeOut{};
    std::vector<uint16_t> probeInData = makeTestImage(200, probeIn);
    std::vector<float>    probeOutData(W * H, 0.0f);
    probeOut.data          = probeOutData.data();
    probeOut.width         = W;
    probeOut.height        = H;
    probeOut.bitsAllocated = 32;
    probeOut.bitsStored    = 32;
    probeOut.format        = XPE_PIXEL_FLOAT32;
    probeOut.dataSize      = probeOutData.size() * sizeof(float);

    XpeImageMetadata probeMeta{};
    EXPECT_EQ(XPE_OK, xpe_gain_correct(&probeIn, &probeOut, &probeMeta));

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
    XpeImageBuffer img{};
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

    // No free() here. The old map-argument gain_correct replaced img.data with a
    // malloc'd float buffer and handed ownership to the caller; the shipped one
    // writes into a caller-supplied output buffer, so img.data still belongs to
    // the vector that allocated it. Freeing it here double-freed and aborted the
    // whole test binary (QA-A-12 ASan: attempting double-free).
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
    XpeImageBuffer img{};
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
    XpeImageBuffer img{};
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
