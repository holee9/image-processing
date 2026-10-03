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

/* ---------------------------------------------------------------------------
 * QA-A-147 (#212): renamed from FullPipelineSmallImage, and the three
 * correction stages now assert the code they actually return.
 *
 * WHAT THIS TEST ANSWERS: do the seven entry points COMPOSE -- buffer and
 * format handoff from stage to stage, in order, without a crash -- when no
 * calibration is loaded. That is the only thing here no other test covers.
 *
 * WHAT IT DOES NOT ANSWER: whether each stage computes the right pixels.
 * That is covered per stage, with pixel-level assertions, in
 * test_golden_reference.cpp (offset :161/:192, gain :254, ghost :349/:364,
 * temp :463). Loading calibration here would duplicate that and introduce
 * global state into a test that has none.
 *
 * WHY THE NAME CHANGED. "FullPipelineSmallImage" promised a full pipeline.
 * This test never calls xpe_preprocess_init() or xpe_calib_load_*, so three
 * of the seven stages cannot do any work -- MEASURED, not assumed: a probe
 * printed rc for stages 4/5/6 and got -6 (XPE_ERR_NOT_INITIALIZED) in all
 * three contexts (this test alone, the full default order, and shuffle seed
 * 9). Nothing else was ever returned.
 *
 * WHY THE ALLOWED SETS WENT. They read
 *   EXPECT_TRUE(rc == XPE_OK || rc == NOT_INITIALIZED || rc == CALIB_NOT_LOADED)
 * so the stage passed whether it worked or did nothing at all -- that is how
 * a sentinel injected into xpe_defect_correct (QA-A-146b) left this test
 * green. git blame: the set was widened by f3ccb0ec (2026-09-10) when
 * XPE_ERR_CALIB_NOT_LOADED was introduced, added defensively to every site
 * rather than because any site returned it. Per the measurement above, the
 * XPE_OK / CALIB_NOT_LOADED / UNSUPPORTED_FORMAT branches are unreachable
 * here. Asserting the single code that does occur also makes leaked global
 * init from another test go red, which is the behaviour we want.
 * ------------------------------------------------------------------------- */
TEST(Integration, UncalibratedPipelineComposes) {
    PipelineBuffers buf(64, 64);
    bool dropped = false, nonuniform = false;

    // Stage 1: Validate readout artifacts (new 4-arg API)
    EXPECT_EQ(XPE_OK, xpe_validate_readout_artifact(
        &buf.rawBuf, &buf.meta, &dropped, &nonuniform));

    // Stage 2: Temperature compensation
    ASSERT_EQ(XPE_OK, xpe_temp_compensate(&buf.rawBuf, 25.0f, nullptr));

    // Stage 3: Nonlinearity correction
    ASSERT_EQ(XPE_OK, xpe_nonlinearity_correct(&buf.rawBuf, nullptr));

    // Stage 4: Offset correction -- refuses; no calibration is loaded
    {
        auto rc = xpe_offset_correct(&buf.rawBuf, &buf.offsetBuf, &buf.meta);
        EXPECT_EQ(XPE_ERR_NOT_INITIALIZED, rc);
    }

    // Stage 5: Gain correction -- refuses; no calibration is loaded
    {
        auto rc = xpe_gain_correct(&buf.rawBuf, &buf.gainBuf, &buf.meta);
        EXPECT_EQ(XPE_ERR_NOT_INITIALIZED, rc);
    }

    // Stage 6: Defect correction -- refuses; no calibration is loaded.
    // Called in-place, which the buffer aliasing contract allows (REQ-P1A-012).
    {
        auto rc = xpe_defect_correct(&buf.gainBuf, &buf.gainBuf, &buf.meta);
        EXPECT_EQ(XPE_ERR_NOT_INITIALIZED, rc);
    }

    // Stage 7: Ghost correction (requires FLOAT32 input from gain stage)
    ASSERT_EQ(XPE_OK, xpe_ghost_create(
        buf.W, buf.H, nullptr, &buf.ghostHandle));
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(buf.ghostHandle, &buf.gainBuf, &buf.meta));

    // Binning (no-op for 1x1)
    ASSERT_EQ(XPE_OK, xpe_binning_correct(&buf.gainBuf, 1, nullptr));

    // END STATE, IN PIXELS -- QA-A-147 (#212). Before this the test asserted no
    // pixel at all, so an identity implementation of every stage passed. The
    // three refusing stages leave gainBuf at its initial 1.0f, ghost frame 0 is
    // a documented exact pass-through (test_golden_reference.cpp:349), and 1x1
    // binning is a no-op -- so the composed run must leave the buffer intact.
    // Any stage that writes where it should not now shows up here.
    for (size_t i = 0; i < buf.gain.size(); ++i) {
        ASSERT_FLOAT_EQ(1.0f, buf.gain[i])
            << "pixel[" << i << "] changed; no stage in this run may write gainBuf";
    }

    // QA-A-140 (#196): the nonlinearity stage above runs with no LUT loaded and
    // now reports its no-op, so this pipeline leaves one alert behind. Drained
    // through the product's own path (hygiene axis 4, QA-A-138).
    xpe_clear_alerts();
}

// QA-A-230 M3 (#245): the former DISABLED_PipelinePerformance3072x3072 lived here. It was off, and would not
// have measured the requirement if it were on (no calibration loaded: offset, gain and defect refused at once,
// ghost refused a UINT16 buffer, no return code read). SRS-CALIB-PERF-001's 500 ms is now asserted by
// PipelinePerformance3072.TheWholeFrameWithCalibrationLoadedFitsSrsPerf001 (test_pipeline_performance_3072.cpp),
// on a path with the maps loaded and every stage's effect checked.

} // namespace
