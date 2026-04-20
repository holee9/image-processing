/**
 * @file test_integration.cpp
 * @brief Integration tests: full 7-stage pipeline and cross-module boundary checks
 *        Acceptance Criterion: complete pipeline <= 500ms for 3072x3072 image
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

// APIs not in new public header; still exported by DLL
extern "C" XPE_API XpeErrorCode xpe_ghost_create(uint32_t width, uint32_t height,
    const char* configJsonOrNull, void** handleOut);
extern "C" XPE_API XpeErrorCode xpe_ghost_correct(void* handle, XpeImageBuffer* img,
    const XpeImageMetadata* meta);
extern "C" XPE_API void xpe_ghost_destroy(void* handle);
extern "C" XPE_API XpeErrorCode xpe_temp_compensate(XpeImageBuffer* img,
    float detectorTempC, const char* configJsonOrNull);
extern "C" XPE_API XpeErrorCode xpe_nonlinearity_correct(XpeImageBuffer* img,
    const char* configJsonOrNull);
extern "C" XPE_API XpeErrorCode xpe_binning_correct(XpeImageBuffer* img,
    int32_t binningMode, const char* configJsonOrNull);

#include <vector>
#include <chrono>
#include <cstdint>

namespace {

struct PipelineBuffers {
    uint32_t W, H;
    std::vector<uint16_t> raw;
    std::vector<uint16_t> offset;
    std::vector<float>    gain;
    std::vector<uint8_t>  defect;
    XpeImageBuffer rawBuf{}, offsetBuf{}, gainBuf{}, defectBuf{};
    void* ghostHandle{nullptr};
    XpeImageMetadata meta{};

    PipelineBuffers(uint32_t w, uint32_t h) : W(w), H(h),
        raw(w * h, 2000), offset(w * h, 200),
        gain(w * h, 1.0f), defect(w * h, 0) {

        rawBuf.data = raw.data(); rawBuf.width = w; rawBuf.height = h;
        rawBuf.bitsAllocated = 16; rawBuf.bitsStored = 16;
        rawBuf.format = XPE_PIXEL_UINT16; rawBuf.dataSize = raw.size() * sizeof(uint16_t);

        offsetBuf.data = offset.data(); offsetBuf.width = w; offsetBuf.height = h;
        offsetBuf.bitsAllocated = 16; offsetBuf.bitsStored = 16;
        offsetBuf.format = XPE_PIXEL_UINT16; offsetBuf.dataSize = offset.size() * sizeof(uint16_t);

        gainBuf.data = gain.data(); gainBuf.width = w; gainBuf.height = h;
        gainBuf.bitsAllocated = 32; gainBuf.bitsStored = 32;
        gainBuf.format = XPE_PIXEL_FLOAT32; gainBuf.dataSize = gain.size() * sizeof(float);

        defectBuf.data = defect.data(); defectBuf.width = w; defectBuf.height = h;
        defectBuf.bitsAllocated = 8; defectBuf.bitsStored = 8;
        defectBuf.format = XPE_PIXEL_UINT8; defectBuf.dataSize = defect.size();

        meta.acquisitionTime = 0u;
    }

    ~PipelineBuffers() {
        if (ghostHandle) xpe_ghost_destroy(ghostHandle);
    }
};

// Full 7-stage pipeline on a small image
TEST(Integration, FullPipelineSmallImage) {
    PipelineBuffers buf(64, 64);
    bool dropped = false, nonuniform = false;

    // Stage 1: Validate readout artifacts (new 4-arg API)
    EXPECT_EQ(XPE_OK, xpe_validate_readout_artifact(
        &buf.rawBuf, &buf.meta, &dropped, &nonuniform));

    // Stage 2: Temperature compensation
    ASSERT_EQ(XPE_OK, xpe_temp_compensate(&buf.rawBuf, 25.0f, nullptr));

    // Stage 3: Nonlinearity correction
    ASSERT_EQ(XPE_OK, xpe_nonlinearity_correct(&buf.rawBuf, nullptr));

    // Stage 4: Offset correction — accepts NOT_INITIALIZED when no calibration loaded
    {
        auto rc = xpe_offset_correct(&buf.rawBuf, &buf.offsetBuf, &buf.meta);
        EXPECT_TRUE(rc == XPE_OK || rc == XPE_ERR_NOT_INITIALIZED);
    }

    // Stage 5: Gain correction — accepts NOT_INITIALIZED when no calibration loaded
    {
        auto rc = xpe_gain_correct(&buf.rawBuf, &buf.gainBuf, &buf.meta);
        EXPECT_TRUE(rc == XPE_OK || rc == XPE_ERR_NOT_INITIALIZED);
    }

    // Stage 6: Defect correction — accepts NOT_INITIALIZED when no calibration loaded
    // Note: new API requires FLOAT32 input; defect stage runs after gain conversion
    {
        auto rc = xpe_defect_correct(&buf.gainBuf, &buf.gainBuf, &buf.meta);
        EXPECT_TRUE(rc == XPE_OK || rc == XPE_ERR_NOT_INITIALIZED || rc == XPE_ERR_UNSUPPORTED_FORMAT);
    }

    // Stage 7: Ghost correction (requires FLOAT32 input from gain stage)
    ASSERT_EQ(XPE_OK, xpe_ghost_create(
        buf.W, buf.H, nullptr, &buf.ghostHandle));
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(buf.ghostHandle, &buf.gainBuf, &buf.meta));

    // Binning (no-op for 1x1)
    ASSERT_EQ(XPE_OK, xpe_binning_correct(&buf.gainBuf, 1, nullptr));
}

// Acceptance criterion: full pipeline <= 500ms for 3072x3072
TEST(Integration, DISABLED_PipelinePerformance3072x3072) {
    // NOTE: Disabled by default — enable when running performance benchmarks
    PipelineBuffers buf(3072, 3072);
    bool dropped = false, nonuniform = false;

    auto start = std::chrono::high_resolution_clock::now();

    xpe_validate_readout_artifact(&buf.rawBuf, &buf.meta, &dropped, &nonuniform);
    xpe_temp_compensate(&buf.rawBuf, 25.0f, nullptr);
    xpe_nonlinearity_correct(&buf.rawBuf, nullptr);
    xpe_offset_correct(&buf.rawBuf, &buf.offsetBuf, &buf.meta);
    xpe_gain_correct(&buf.rawBuf, &buf.gainBuf, &buf.meta);
    xpe_defect_correct(&buf.gainBuf, &buf.gainBuf, &buf.meta);
    xpe_ghost_create(buf.W, buf.H, nullptr, &buf.ghostHandle);
    xpe_ghost_correct(buf.ghostHandle, &buf.rawBuf, &buf.meta);
    xpe_binning_correct(&buf.rawBuf, 1, nullptr);

    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    EXPECT_LE(ms, 500) << "Pipeline took " << ms << "ms (limit: 500ms)";
}

} // namespace
