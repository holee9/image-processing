/**
 * @file test_display_integration.cpp
 * @brief Integration tests for full display pipeline and boundary cases.
 * SPEC: SPEC-XPE-P1B-DISP
 * REQ-DISP-029 to REQ-DISP-035
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>

#include "xpe/display/display_api.h"

// =============================================================================
// Test Helpers
// =============================================================================

static XpeImageBuffer make_float32_image(uint32_t w, uint32_t h, float fill_value) {
    XpeImageBuffer img{};
    img.width         = w;
    img.height        = h;
    img.format        = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32;
    img.bitsStored    = 32;
    img.dataSize      = w * h * sizeof(float);
    img.data          = std::malloc(img.dataSize);
    float* px = static_cast<float*>(img.data);
    for (size_t i = 0; i < (size_t)w * h; ++i) px[i] = fill_value;
    return img;
}

static void free_image(XpeImageBuffer& img) {
    std::free(img.data);
    img.data = nullptr;
}

static float* float_pixels(XpeImageBuffer& img) {
    return static_cast<float*>(img.data);
}

static uint16_t* uint16_pixels(XpeImageBuffer& img) {
    return static_cast<uint16_t*>(img.data);
}

// Build an identity presentation LUT
static void make_identity_plut(XpePresentationLutParams& p) {
    for (int i = 0; i < 1024; ++i) p.lutData[i] = static_cast<uint16_t>(i);
    p.gsdfEnabled = 0;
}

// =============================================================================
// REQ-DISP-029: Full Pipeline Test
// ModalityLUT(LINEAR) -> VOI(LINEAR) -> PresentationLUT
// =============================================================================

TEST(DisplayIntegration, FullPipeline_LinearModality_LinearVoi_PresLut) {
    // REQ-DISP-029: Full pipeline produces valid uint16 output
    XpeImageBuffer img = make_float32_image(4, 4, 1000.0f);

    // Stage 1: Modality LUT LINEAR
    XpeModalityLutParams mlut{};
    mlut.mode             = XPE_MODALITY_LUT_LINEAR;
    mlut.rescaleSlope     = 1.0f;
    mlut.rescaleIntercept = -900.0f; // 1000 * 1 - 900 = 100
    ASSERT_EQ(xpe_apply_modality_lut(&img, &mlut), XPE_OK);
    EXPECT_FLOAT_EQ(float_pixels(img)[0], 100.0f);

    // Stage 2: VOI LUT LINEAR — window around 100
    XpeVoiLutParams voi{};
    voi.mode   = XPE_VOI_LINEAR;
    voi.center = 100.0f;
    voi.width  = 200.0f;  // window: [0, 200] -> output: [0, 1]
    voi.minOut = 0.0f;
    voi.maxOut = 1.0f;
    ASSERT_EQ(xpe_apply_voi_lut(&img, &voi), XPE_OK);
    // (100 - (100-100)) / 200 * 1 + 0 = 0.5
    EXPECT_NEAR(float_pixels(img)[0], 0.5f, 0.01f);

    // Stage 3: Presentation LUT — identity maps 0.5 -> index 512 -> 512
    XpePresentationLutParams plut{};
    make_identity_plut(plut);
    ASSERT_EQ(xpe_apply_presentation_lut(&img, &plut), XPE_OK);

    EXPECT_EQ(img.format, XPE_PIXEL_UINT16);
    EXPECT_EQ(uint16_pixels(img)[0], 512u);
    free_image(img);
}

// =============================================================================
// REQ-DISP-030: Full Pipeline with TABLE Modality LUT
// =============================================================================

TEST(DisplayIntegration, FullPipeline_TableModality_SigmoidVoi) {
    // REQ-DISP-030: TABLE modality + SIGMOID VOI + PresLUT
    uint16_t lut_data[5] = {0, 100, 200, 300, 400};
    XpeImageBuffer img = make_float32_image(1, 1, 2.0f);

    // Stage 1: TABLE maps 2.0 -> index 2 -> 200.0f
    XpeModalityLutParams mlut{};
    mlut.mode           = XPE_MODALITY_LUT_TABLE;
    mlut.lutData        = lut_data;
    mlut.lutLength      = 5;
    mlut.lutFirstMapped = 0;
    ASSERT_EQ(xpe_apply_modality_lut(&img, &mlut), XPE_OK);
    EXPECT_FLOAT_EQ(float_pixels(img)[0], 200.0f);

    // Stage 2: SIGMOID around 200
    XpeVoiLutParams voi{};
    voi.mode   = XPE_VOI_SIGMOID;
    voi.center = 200.0f;
    voi.width  = 400.0f;
    voi.minOut = 0.0f;
    voi.maxOut = 1.0f;
    ASSERT_EQ(xpe_apply_voi_lut(&img, &voi), XPE_OK);
    // At center, sigmoid = 0.5
    EXPECT_NEAR(float_pixels(img)[0], 0.5f, 0.01f);

    // Stage 3: Presentation LUT
    XpePresentationLutParams plut{};
    make_identity_plut(plut);
    ASSERT_EQ(xpe_apply_presentation_lut(&img, &plut), XPE_OK);
    EXPECT_EQ(img.format, XPE_PIXEL_UINT16);
    free_image(img);
}

// =============================================================================
// REQ-DISP-031: Preset-driven pipeline
// =============================================================================

TEST(DisplayIntegration, PresetDrivenPipeline_BonePreset) {
    // REQ-DISP-031: Use BONE preset through VOI then PresentationLUT
    XpeImageBuffer img = make_float32_image(2, 2, 500.0f);

    // Apply linear modality (identity)
    XpeModalityLutParams mlut{};
    mlut.mode             = XPE_MODALITY_LUT_LINEAR;
    mlut.rescaleSlope     = 1.0f;
    mlut.rescaleIntercept = 0.0f;
    ASSERT_EQ(xpe_apply_modality_lut(&img, &mlut), XPE_OK);

    // Use BONE preset
    XpeVoiLutParams voi{};
    ASSERT_EQ(xpe_voi_preset_create(&voi, XPE_BODY_BONE), XPE_OK);
    ASSERT_EQ(xpe_apply_voi_lut(&img, &voi), XPE_OK);
    // Bone center=500, width=2000, minOut=0, maxOut=255
    // input=500 (center) -> output = 127.5
    EXPECT_NEAR(float_pixels(img)[0], 127.5f, 1.0f);

    XpePresentationLutParams plut{};
    make_identity_plut(plut);
    ASSERT_EQ(xpe_apply_presentation_lut(&img, &plut), XPE_OK);
    EXPECT_EQ(img.format, XPE_PIXEL_UINT16);
    free_image(img);
}

// =============================================================================
// REQ-DISP-034: Edge case — 1x1 image full pipeline
// =============================================================================

TEST(DisplayIntegration, EdgeCase_1x1FullPipeline) {
    // REQ-DISP-034: 1x1 image must not crash through full pipeline
    XpeImageBuffer img = make_float32_image(1, 1, 0.5f);

    XpeModalityLutParams mlut{};
    mlut.mode             = XPE_MODALITY_LUT_LINEAR;
    mlut.rescaleSlope     = 1.0f;
    mlut.rescaleIntercept = 0.0f;
    ASSERT_EQ(xpe_apply_modality_lut(&img, &mlut), XPE_OK);

    XpeVoiLutParams voi{};
    voi.mode   = XPE_VOI_LINEAR;
    voi.center = 0.5f;
    voi.width  = 1.0f;
    voi.minOut = 0.0f;
    voi.maxOut = 1.0f;
    ASSERT_EQ(xpe_apply_voi_lut(&img, &voi), XPE_OK);

    XpePresentationLutParams plut{};
    make_identity_plut(plut);
    ASSERT_EQ(xpe_apply_presentation_lut(&img, &plut), XPE_OK);

    EXPECT_EQ(img.format, XPE_PIXEL_UINT16);
    EXPECT_EQ(img.width, 1u);
    EXPECT_EQ(img.height, 1u);
    free_image(img);
}

// =============================================================================
// REQ-DISP-035: Edge case — 4096x4096 image (stress test, no crash)
// =============================================================================

TEST(DisplayIntegration, EdgeCase_4096x4096NocrashModalityLut) {
    // REQ-DISP-035: 4096x4096 must not crash
    XpeImageBuffer img = make_float32_image(4096, 4096, 100.0f);

    XpeModalityLutParams mlut{};
    mlut.mode             = XPE_MODALITY_LUT_LINEAR;
    mlut.rescaleSlope     = 1.0f;
    mlut.rescaleIntercept = 0.0f;

    XpeErrorCode rc = xpe_apply_modality_lut(&img, &mlut);
    EXPECT_EQ(rc, XPE_OK);
    free_image(img);
}

// =============================================================================
// REQ-DISP-032: Thread-safety — independent buffers
// (Single-threaded verification that each buffer is processed independently)
// =============================================================================

TEST(DisplayIntegration, IndependentBuffers_NoInterference) {
    // REQ-DISP-032: processing independent buffers produces independent results
    XpeImageBuffer img1 = make_float32_image(2, 2, 100.0f);
    XpeImageBuffer img2 = make_float32_image(2, 2, 200.0f);

    XpeModalityLutParams mlut{};
    mlut.mode             = XPE_MODALITY_LUT_LINEAR;
    mlut.rescaleSlope     = 1.0f;
    mlut.rescaleIntercept = 0.0f;

    ASSERT_EQ(xpe_apply_modality_lut(&img1, &mlut), XPE_OK);
    ASSERT_EQ(xpe_apply_modality_lut(&img2, &mlut), XPE_OK);

    EXPECT_FLOAT_EQ(float_pixels(img1)[0], 100.0f);
    EXPECT_FLOAT_EQ(float_pixels(img2)[0], 200.0f);

    free_image(img1);
    free_image(img2);
}

// =============================================================================
// REQ-DISP-033: No heap memory outliving function scope (no leaks in happy path)
// =============================================================================

TEST(DisplayIntegration, NoLeak_PresLutReplacesBuffer) {
    // REQ-DISP-033: xpe_apply_presentation_lut frees float32 buffer and replaces with uint16
    // After the call, img->data points to a new uint16 allocation; free it once.
    XpeImageBuffer img = make_float32_image(3, 3, 0.75f);
    void* old_ptr = img.data;

    XpePresentationLutParams plut{};
    make_identity_plut(plut);
    ASSERT_EQ(xpe_apply_presentation_lut(&img, &plut), XPE_OK);

    // Old pointer must have been freed (we cannot easily verify, but new ptr must differ)
    EXPECT_NE(img.data, old_ptr);
    EXPECT_EQ(img.format, XPE_PIXEL_UINT16);
    free_image(img);
}

// =============================================================================
// REQ-DISP-028: GSDF-calibrated LUT used in full pipeline
// =============================================================================

TEST(DisplayIntegration, GsdfPipeline_CalibrateThenApply) {
    // REQ-DISP-028: GSDF LUT is computed then applied
    float lum[5] = {1.0f, 10.0f, 50.0f, 200.0f, 500.0f};
    XpePresentationLutParams plut{};
    ASSERT_EQ(xpe_gsdf_calibrate(lum, 5, &plut), XPE_OK);
    EXPECT_EQ(plut.gsdfEnabled, 1);

    XpeImageBuffer img = make_float32_image(2, 2, 0.5f);

    // Apply VOI first to get normalized output
    XpeVoiLutParams voi{};
    voi.mode   = XPE_VOI_LINEAR;
    voi.center = 0.5f;
    voi.width  = 1.0f;
    voi.minOut = 0.0f;
    voi.maxOut = 1.0f;
    ASSERT_EQ(xpe_apply_voi_lut(&img, &voi), XPE_OK);

    // Apply GSDF-calibrated presentation LUT
    ASSERT_EQ(xpe_apply_presentation_lut(&img, &plut), XPE_OK);
    EXPECT_EQ(img.format, XPE_PIXEL_UINT16);
    free_image(img);
}

// =============================================================================
// REQ-DISP-029: Version API
// =============================================================================

TEST(DisplayIntegration, VersionString_NotNull) {
    // REQ-DISP-029: xpe_display_version() returns non-NULL, non-empty string
    const char* ver = xpe_display_version();
    ASSERT_NE(ver, nullptr);
    EXPECT_GT(strlen(ver), 0u);
}

// =============================================================================
// #123 (QA-B-21): XpeImageBuffer.dataSize size-consistency guard, per entry point
//
// Contract (api-spec "XpeImageBuffer.dataSize on input"):
//   dataSize == 0                           -> unspecified, accepted
//   0 < dataSize < width*height*bpp(format) -> XPE_ERR_INVALID_INPUT
//   dataSize >= width*height*bpp(format)    -> accepted
//
// All three display entry points route through xpe_validate_float32()
// (display_helpers.cpp). Each gets a short-dataSize case and a dataSize==0
// case so the guard is proven to fire on that path, not merely to compile.
// The allocation is always full (32*32 floats); only the declared dataSize is
// short, so a case that reaches the implementation stays inside its buffer.
// =============================================================================

namespace {

constexpr uint32_t kGuardW = 32;
constexpr uint32_t kGuardH = 32;
constexpr size_t   kGuardShortBytes = static_cast<size_t>(16) * sizeof(float);

// Fully allocated 32x32 FLOAT32 image with a caller-chosen declared dataSize.
static XpeImageBuffer make_guard_image(size_t declaredDataSize) {
    XpeImageBuffer img = make_float32_image(kGuardW, kGuardH, 100.0f);
    img.dataSize = declaredDataSize;
    return img;
}

static XpeModalityLutParams guard_modality_params() {
    XpeModalityLutParams p{};
    p.mode             = XPE_MODALITY_LUT_LINEAR;
    p.rescaleSlope     = 1.0f;
    p.rescaleIntercept = 0.0f;
    return p;
}

static XpeVoiLutParams guard_voi_params() {
    XpeVoiLutParams p{};
    p.mode   = XPE_VOI_LINEAR;
    p.center = 100.0f;
    p.width  = 200.0f;
    p.minOut = 0.0f;
    p.maxOut = 4095.0f;
    return p;
}

static XpePresentationLutParams guard_presentation_params() {
    XpePresentationLutParams p{};
    for (int i = 0; i < 1024; ++i) {
        p.lutData[i] = static_cast<uint16_t>(i * 4);
    }
    p.gsdfEnabled = 0;
    return p;
}

}  // namespace

TEST(DisplayDataSizeGuard, ModalityLut_ShortDataSize_ReturnsInvalidInput) {
    XpeImageBuffer img = make_guard_image(kGuardShortBytes);
    XpeModalityLutParams p = guard_modality_params();
    EXPECT_EQ(xpe_apply_modality_lut(&img, &p), XPE_ERR_INVALID_INPUT);
    free_image(img);
}

TEST(DisplayDataSizeGuard, VoiLut_ShortDataSize_ReturnsInvalidInput) {
    XpeImageBuffer img = make_guard_image(kGuardShortBytes);
    XpeVoiLutParams p = guard_voi_params();
    EXPECT_EQ(xpe_apply_voi_lut(&img, &p), XPE_ERR_INVALID_INPUT);
    free_image(img);
}

TEST(DisplayDataSizeGuard, PresentationLut_ShortDataSize_ReturnsInvalidInput) {
    XpeImageBuffer img = make_guard_image(kGuardShortBytes);
    XpePresentationLutParams p = guard_presentation_params();
    EXPECT_EQ(xpe_apply_presentation_lut(&img, &p), XPE_ERR_INVALID_INPUT);
    free_image(img);
}

TEST(DisplayDataSizeGuard, ModalityLut_ZeroDataSize_Accepted) {
    XpeImageBuffer img = make_guard_image(0);
    XpeModalityLutParams p = guard_modality_params();
    EXPECT_EQ(xpe_apply_modality_lut(&img, &p), XPE_OK);
    free_image(img);
}

TEST(DisplayDataSizeGuard, VoiLut_ZeroDataSize_Accepted) {
    XpeImageBuffer img = make_guard_image(0);
    XpeVoiLutParams p = guard_voi_params();
    EXPECT_EQ(xpe_apply_voi_lut(&img, &p), XPE_OK);
    free_image(img);
}

TEST(DisplayDataSizeGuard, PresentationLut_ZeroDataSize_Accepted) {
    XpeImageBuffer img = make_guard_image(0);
    XpePresentationLutParams p = guard_presentation_params();
    // On success the entry point replaces img.data with a freshly allocated
    // uint16 buffer, so free_image() below releases that one.
    EXPECT_EQ(xpe_apply_presentation_lut(&img, &p), XPE_OK);
    free_image(img);
}

// ASan probe (#123 QA-B-21 step 4): a GENUINELY short allocation. With the
// guard in place this is rejected before any pixel is read; with the guard
// removed the LUT loop reads 32*32 floats out of a 16-float allocation, which
// is the heap-buffer-overflow READ the ASan run observes.
TEST(DisplayDataSizeGuardAsanProbe, ModalityLut_TrulyShortBuffer_RejectedBeforeRead) {
    std::vector<float> tiny(16, 100.0f);
    XpeImageBuffer img{};
    img.width         = kGuardW;
    img.height        = kGuardH;
    img.bitsAllocated = 32;
    img.bitsStored    = 32;
    img.format        = XPE_PIXEL_FLOAT32;
    img.data          = tiny.data();
    img.dataSize      = tiny.size() * sizeof(float);
    XpeModalityLutParams p = guard_modality_params();
    EXPECT_EQ(xpe_apply_modality_lut(&img, &p), XPE_ERR_INVALID_INPUT);
}

// ---------------------------------------------------------------------------
// #105 G3: working-set measurement, mirroring enhance_basic
// test_enhance_integration.cpp (92bcf17) and preprocess T-010. Duplicated per
// module rather than exported: xpe_common's surface is fixed at 16 symbols
// (REQ-P0-008).
// ---------------------------------------------------------------------------
#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
static SIZE_T get_working_set_bytes() {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
}
#else
static size_t get_working_set_bytes() { return 0; }
#endif

// WARMUP exists because the first cycles fault in fresh heap pages and grow the
// CRT allocator arena; counting that one-time cost as "leak" would make the
// threshold a measure of startup, not of retention. The baseline is snapshotted
// after warm-up so only steady-state growth is scored.
constexpr int    ENDURANCE_CYCLES = 1000;
constexpr int    ENDURANCE_WARMUP = 100;
constexpr size_t ENDURANCE_ONE_MB = 1024u * 1024u;

// #105 G3: heap growth over 1000 alloc-process-free cycles.
// display had no endurance loop and no heap measurement before this. The cycle
// exercises all three entry points; presentation_lut is included deliberately
// because it frees the float32 buffer and allocates a uint16 one, which is the
// path most likely to retain.
TEST(DisplayEndurance, ThousandCycles_MemoryGrowthUnderOneMB) {
#ifndef _WIN32
    GTEST_SKIP() << "Working-set measurement is Windows-only in this build";
#endif
    XpeModalityLutParams mod = guard_modality_params();
    XpeVoiLutParams      voi = guard_voi_params();
    XpePresentationLutParams pres = guard_presentation_params();

    auto one_cycle = [&](int i) {
        XpeImageBuffer img = make_float32_image(64, 64, 0.5f);
        ASSERT_EQ(xpe_apply_modality_lut(&img, &mod), XPE_OK) << "cycle " << i;
        ASSERT_EQ(xpe_apply_voi_lut(&img, &voi), XPE_OK) << "cycle " << i;
        // Renormalise into [0,1] before the presentation LUT: voi_lut maps into
        // [minOut,maxOut] and the presentation stage clamps its input.
        float* px = float_pixels(img);
        for (size_t k = 0; k < 64u * 64u; ++k) px[k] = px[k] / 4095.0f;
        ASSERT_EQ(xpe_apply_presentation_lut(&img, &pres), XPE_OK) << "cycle " << i;
        free_image(img);  // frees the uint16 buffer presentation_lut installed
    };

    for (int i = 0; i < ENDURANCE_WARMUP; ++i) one_cycle(i);

    const auto before = get_working_set_bytes();
    for (int i = 0; i < ENDURANCE_CYCLES; ++i) one_cycle(i);
    const auto after = get_working_set_bytes();

    if (after > before) {
        EXPECT_LT(after - before, ENDURANCE_ONE_MB)
            << "Working set grew by " << (after - before) / 1024 << " KB over "
            << ENDURANCE_CYCLES << " display modality/voi/presentation cycles";
    }
}
