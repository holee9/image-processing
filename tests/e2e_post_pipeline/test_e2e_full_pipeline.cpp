/**
 * @file test_e2e_full_pipeline.cpp
 * @brief Google Test suite for full XPE post-processing pipeline E2E performance
 *
 * Gate G2 E2E Performance: Pre -> Post -> Display < 3000ms @ 3072x3072
 *
 * Pipeline stages exercised:
 * - Enhance Basic: Exposure Index, Log Transform, Noise Reduction, CLAHE, USM
 * - GSVG: Vignette + Grid suppression on uint16 buffer
 * - Display: Modality LUT, VOI LUT (Bone preset), Presentation LUT (GSDF)
 *
 * Notes:
 * - Synthesizes a constant-valued float32 image (no DICOM I/O).
 * - GSVG operates on uint16; we convert a snapshot of the float buffer to uint16
 *   and back to keep the float pipeline usable. This adds a small overhead but
 *   reflects the integration shape of a Phase 1 production pipeline.
 */

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/enhance_basic/enhance_basic_api.h"
#include "xpe/enhance_advanced/enhance_advanced_api.h"
#include "xpe/gsvg/gsvg_api.h"
#include "xpe/display/display_api.h"

#include "gtest/gtest.h"

#include <chrono>
#include <cstring>
#include <vector>

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

// @MX:NOTE: [AUTO] Synthesize a contiguous float32 image without DICOM I/O.
static XpeImageBuffer make_f32(uint32_t width, uint32_t height, float value) {
    XpeImageBuffer img{};
    img.width = width;
    img.height = height;
    img.bitsAllocated = 32;
    img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.dataSize = static_cast<size_t>(width) * height * sizeof(float);
    img.data = new uint8_t[img.dataSize];

    float* ptr = static_cast<float*>(img.data);
    const int64_t n = static_cast<int64_t>(width) * height;
    for (int64_t i = 0; i < n; ++i) {
        ptr[i] = value;
    }
    return img;
}

static void free_img(XpeImageBuffer& img) {
    delete[] static_cast<uint8_t*>(img.data);
    img.data = nullptr;
    img.dataSize = 0;
}

static void set_body_part(XpeImageMetadata& meta, const char* part) {
    std::strncpy(meta.bodyPart, part, sizeof(meta.bodyPart) - 1);
    meta.bodyPart[sizeof(meta.bodyPart) - 1] = '\0';
}

/* ============================================================================
 * E2E Performance Test
 * ============================================================================ */

namespace {

// Run the full Phase 1 pipeline on a float32 image of the given dimensions.
// Returns XPE_OK on success, otherwise the first non-OK error code encountered.
static XpeErrorCode run_pipeline(uint32_t width, uint32_t height, int64_t& total_ms_out) {
    auto img = make_f32(width, height, 500.0f);

    XpeImageMetadata meta{};
    set_body_part(meta, "CHEST");
    float outEI = 0.0f, outDI = 0.0f;

    XpeNoiseReduceParams nr_params{};
    nr_params.mode = XPE_NOISE_BILATERAL;
    nr_params.sigma_space = 3.0f;
    nr_params.sigma_range = 50.0f;

    XpeClaheParams clahe_params{};
    clahe_params.clip_limit = 3.0f;
    clahe_params.tile_width = 8;
    clahe_params.tile_height = 8;

    XpeUsmParams usm_params{};
    usm_params.amount = 0.5f;
    usm_params.radius = 2.0f;
    usm_params.threshold = 10.0f;

    // Display LUT params
    XpeModalityLutParams mod_lut_params{};
    mod_lut_params.mode = XPE_MODALITY_LUT_LINEAR;
    mod_lut_params.rescaleSlope = 1.0f;
    mod_lut_params.rescaleIntercept = 0.0f;

    XpeVoiLutParams voi_lut_params{};
    XpeErrorCode preset_err = xpe_voi_preset_create(&voi_lut_params, XPE_BODY_BONE);
    if (preset_err != XPE_OK) {
        free_img(img);
        return preset_err;
    }
    voi_lut_params.minOut = 0.0f;
    voi_lut_params.maxOut = 1.0f;

    XpePresentationLutParams pres_lut_params{};
    // Identity-ish LUT: linear ramp over [0..1023] -> [0..65535]
    for (uint32_t i = 0; i < 1024; ++i) {
        uint32_t v = (i * 65535u) / 1023u;
        pres_lut_params.lutData[i] = static_cast<uint16_t>(v);
    }
    pres_lut_params.gsdfEnabled = 0;

    // GSVG handle (vignette + grid suppression both off for stable timing baseline)
    void* gsvg_handle = nullptr;
    XpeErrorCode gsvg_err = xpe_gsvg_init(&gsvg_handle,
                                          "{\"vignette_correction\":false,\"grid_suppression\":true}");
    if (gsvg_err != XPE_OK) {
        free_img(img);
        return gsvg_err;
    }

    auto t0 = std::chrono::high_resolution_clock::now();

    // STAGE 1: Exposure Index (Enhance Basic)
    XpeErrorCode err = xpe_calc_exposure_index(&img, &meta, &outEI, &outDI);
    if (err != XPE_OK) { xpe_gsvg_shutdown(gsvg_handle); free_img(img); return err; }

    // STAGE 2: Log Transform (Enhance Basic)
    err = xpe_log_transform(&img, 1000.0f);
    if (err != XPE_OK) { xpe_gsvg_shutdown(gsvg_handle); free_img(img); return err; }

    // STAGE 3: Noise Reduction (Enhance Basic, Bilateral)
    err = xpe_noise_reduce(&img, &nr_params);
    if (err != XPE_OK) { xpe_gsvg_shutdown(gsvg_handle); free_img(img); return err; }

    // STAGE 4: CLAHE (Enhance Basic)
    err = xpe_contrast_enhance(&img, &clahe_params);
    if (err != XPE_OK) { xpe_gsvg_shutdown(gsvg_handle); free_img(img); return err; }

    // STAGE 5: USM (Enhance Basic)
    err = xpe_edge_enhance(&img, &usm_params);
    if (err != XPE_OK) { xpe_gsvg_shutdown(gsvg_handle); free_img(img); return err; }

    // STAGE 6: GSVG (uint16 domain). Convert float -> uint16, run, convert back.
    {
        const int64_t n = static_cast<int64_t>(width) * height;
        std::vector<uint16_t> u16(static_cast<size_t>(n));
        const float* fpx = static_cast<const float*>(img.data);
        for (int64_t i = 0; i < n; ++i) {
            float v = fpx[i];
            if (v < 0.0f) v = 0.0f;
            if (v > 65535.0f) v = 65535.0f;
            u16[static_cast<size_t>(i)] = static_cast<uint16_t>(v);
        }
        err = xpe_gsvg_process(gsvg_handle, u16.data(), u16.data(),
                               static_cast<int>(width), static_cast<int>(height),
                               nullptr);
        if (err != XPE_OK) { xpe_gsvg_shutdown(gsvg_handle); free_img(img); return err; }
        float* fpx_out = static_cast<float*>(img.data);
        for (int64_t i = 0; i < n; ++i) {
            fpx_out[i] = static_cast<float>(u16[static_cast<size_t>(i)]);
        }
    }

    // STAGE 7: Display LUT chain (Modality -> VOI -> Presentation)
    err = xpe_apply_modality_lut(&img, &mod_lut_params);
    if (err != XPE_OK) { xpe_gsvg_shutdown(gsvg_handle); free_img(img); return err; }

    err = xpe_apply_voi_lut(&img, &voi_lut_params);
    if (err != XPE_OK) { xpe_gsvg_shutdown(gsvg_handle); free_img(img); return err; }

    err = xpe_apply_presentation_lut(&img, &pres_lut_params);
    // Note: presentation_lut performs a domain transition float32 -> uint16 and
    // reallocates img.data. free_img() correctly frees the new buffer.
    if (err != XPE_OK) { xpe_gsvg_shutdown(gsvg_handle); free_img(img); return err; }

    auto t1 = std::chrono::high_resolution_clock::now();
    total_ms_out = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    xpe_gsvg_shutdown(gsvg_handle);
    free_img(img);
    return XPE_OK;
}

}  // anonymous namespace

TEST(FullPipelineE2E, PostProcess_3072x3072_Within3000ms) {
    constexpr uint32_t kWidth = 3072;
    constexpr uint32_t kHeight = 3072;
    int64_t total_ms = 0;

    XpeErrorCode err = run_pipeline(kWidth, kHeight, total_ms);
    ASSERT_EQ(XPE_OK, err) << "Pipeline returned error code " << static_cast<int>(err);

    std::cout << "[ E2E ] Full post-processing pipeline (E2E) took " << total_ms
              << "ms for " << kWidth << "x" << kHeight
              << " image (budget: 3000ms)" << std::endl;

    EXPECT_LE(total_ms, 3000)
        << "Full post-processing pipeline (E2E) took " << total_ms
        << "ms for " << kWidth << "x" << kHeight
        << " image (budget: 3000ms)";
}

TEST(FullPipelineE2E, PostProcess_512x512_Within200ms) {
    constexpr uint32_t kWidth = 512;
    constexpr uint32_t kHeight = 512;
    int64_t total_ms = 0;

    XpeErrorCode err = run_pipeline(kWidth, kHeight, total_ms);
    ASSERT_EQ(XPE_OK, err) << "Pipeline returned error code " << static_cast<int>(err);

    std::cout << "[ E2E ] Full pipeline (512x512) took " << total_ms
              << "ms (budget: 200ms)" << std::endl;

    EXPECT_LE(total_ms, 200)
        << "Full pipeline (512x512) took " << total_ms << "ms (budget: 200ms)";
}
