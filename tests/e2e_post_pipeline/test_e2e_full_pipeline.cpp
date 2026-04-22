/**
 * @file test_e2e_full_pipeline.cpp
 * @brief Google Test suite for full XPE post-processing pipeline E2E performance
 *
 * Gate G2 E2E Performance: Pre → Post → Display → DICOM < 3000ms @ 3072×3072
 *
 * This test validates the full end-to-end performance of the XPE post-processing
 * pipeline including all stages:
 * - Preprocess: Ghost correction, Gain/Offset (when available)
 * - Post-process: Log transform, Noise reduction, CLAHE, USM, Collimation
 * - Display: Modality LUT, VOI LUT, Presentation LUT, GSDF calibration
 * - DICOM: Encoding (when available)
 *
 * Target: < 3000ms for 3072×3072 image
 */

#include "xpe/common/xpe_common_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/enhance_basic/enhance_basic_api.h"
#include "xpe/enhance_advanced/enhance_advanced_api.h"
#include "xpe/gsvg/gsvg_api.h"
#include "xpe/display/display_api.h"

#include "gtest/gtest.h"

#include <chrono>
#include <cstring>

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

/**
 * @brief Create a float32 test image filled with constant value
 */
static XpeImageBuffer make_f32(int32_t width, int32_t height, float value) {
    XpeImageBuffer img{};
    img.width = width;
    img.height = height;
    img.format = XPE_PIXEL_FLOAT32;
    img.stride = width * sizeof(float);
    img.data = new uint8_t[width * height * sizeof(float)];

    float* ptr = reinterpret_cast<float*>(img.data);
    for (int64_t i = 0; i < static_cast<int64_t>(width) * height; ++i) {
        ptr[i] = value;
    }

    return img;
}

/**
 * @brief Free image buffer created by make_f32
 */
static void free_img(XpeImageBuffer& img) {
    delete[] img.data;
    img.data = nullptr;
}

/**
 * @brief Set body part in metadata for EI calculation
 */
static void set_body_part(XpeImageMetadata& meta, const char* part) {
    std::strncpy(meta.bodyPart, part, sizeof(meta.bodyPart) - 1);
    meta.bodyPart[sizeof(meta.bodyPart) - 1] = '\0';
}

/* ============================================================================
 * E2E Performance Test
 * ============================================================================ */

/**
 * @test FullPipeline_3072x3072_Within3000ms
 * @brief Measure end-to-end latency of complete post-processing pipeline
 *
 * Pipeline stages:
 * 1. Exposure Index calculation
 * 2. Log transform
 * 3. Noise reduction (Bilateral)
 * 4. Contrast enhancement (CLAHE)
 * 5. Edge enhancement (USM)
 * 6. GSVG correction (Grid + Vignette)
 * 7. Display LUT chain (Modality → VOI → Presentation → GSDF)
 *
 * Performance budget: 3000ms total
 *
 * @note This test does NOT include:
 * - Preprocess (Ghost/Gain/Offset) - measured separately in preprocess module
 * - DICOM encoding - measured separately in dicom module
 */
TEST(FullPipelineE2E, PostProcess_3072x3072_Within3000ms) {
    // Create 3072x3072 test image (typical clinical size)
    constexpr int32_t kWidth = 3072;
    constexpr int32_t kHeight = 3072;
    constexpr float kPixelValue = 500.0f;  // Mid-range value

    auto img = make_f32(kWidth, kHeight, kPixelValue);

    // Prepare metadata for EI calculation
    XpeImageMetadata meta{};
    set_body_part(meta, "CHEST");
    float outEI = 0.0f, outDI = 0.0f;

    // Prepare Enhance Basic parameters
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

    // Prepare Enhance Advanced parameters (GSVG)
    XpeCollimationParams coll_params{};
    coll_params.confidence_threshold = 0.85f;

    XpeGridParams grid_params{};
    grid_params.enabled = true;
    grid_params.grid_frequency = 40.0f;  // Typical grid frequency

    XpeVignetteParams vignette_params{};
    vignette_params.enabled = true;
    // vignette_map would be loaded from calibration in real scenario

    XpeGsvParams gsv_params{};
    gsv_params.collimation = &coll_params;
    gsv_params.grid = &grid_params;
    gsv_params.vignette = &vignette_params;

    // Prepare Display LUT parameters
    XpeModalityLutParams mod_lut_params{};
    mod_lut_params.rescale_intercept = 0.0f;
    mod_lut_params.rescale_slope = 1.0f;

    XpeVoiLutParams voi_lut_params{};
    voi_lut_params.width = kWidth;
    voi_lut_params.height = kHeight;
    // Use preset-based VOI LUT (Bone preset)

    XpePresentationLutParams pres_lut_params{};
    pres_lut_params.gsdf_apply = true;  // Apply GSDF calibration

    // Start E2E timer
    auto t0 = std::chrono::high_resolution_clock::now();

    // ===== STAGE 1: Exposure Index (Enhance Basic) =====
    ASSERT_EQ(XPE_OK, xpe_calc_exposure_index(&img, &meta, &outEI, &outDI));

    // ===== STAGE 2: Log Transform (Enhance Basic) =====
    ASSERT_EQ(XPE_OK, xpe_log_transform(&img, 1000.0f));

    // ===== STAGE 3: Noise Reduction (Enhance Basic) =====
    ASSERT_EQ(XPE_OK, xpe_noise_reduce(&img, &nr_params));

    // ===== STAGE 4: Contrast Enhancement (Enhance Basic) =====
    ASSERT_EQ(XPE_OK, xpe_contrast_enhance(&img, &clahe_params));

    // ===== STAGE 5: Edge Enhancement (Enhance Basic) =====
    ASSERT_EQ(XPE_OK, xpe_edge_enhance(&img, &usm_params));

    // ===== STAGE 6: GSVG Correction (Enhance Advanced) =====
    // Note: GSVG init would be done once at startup in real scenario
    // Here we test the process call directly
    ASSERT_EQ(XPE_OK, xpe_gsv_process(&img, &gsv_params));

    // ===== STAGE 7: Display LUT Chain =====
    // Modality LUT
    ASSERT_EQ(XPE_OK, xpe_display_modality_lut(&img, &mod_lut_params));

    // VOI LUT (Bone preset)
    ASSERT_EQ(XPE_OK, xpe_display_voi_lut_preset(&img, XPE_VOI_PRESET_BONE, &voi_lut_params));

    // Presentation LUT with GSDF
    ASSERT_EQ(XPE_OK, xpe_display_presentation_lut(&img, &pres_lut_params));

    // Stop E2E timer
    auto t1 = std::chrono::high_resolution_clock::now();
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    // Verify performance budget
    EXPECT_LE(total_ms, 3000)
        << "Full post-processing pipeline (E2E) took " << total_ms
        << "ms for " << kWidth << "x" << kHeight
        << " image (budget: 3000ms)\n"
        << "Breakdown by stage:\n"
        << "  1. Exposure Index: ~1-5ms\n"
        << "  2. Log Transform: ~10-30ms\n"
        << "  3. Noise Reduction: ~50-150ms (Bilateral 3072x3072)\n"
        << "  4. CLAHE: ~10-50ms\n"
        << "  5. USM: ~10-40ms\n"
        << "  6. GSVG: ~5-50ms\n"
        << "  7. Modality LUT: ~10-20ms\n"
        << "  8. VOI LUT: ~10-20ms\n"
        << "  9. Presentation LUT: ~20-40ms";

    // Verify output is valid (not all zeros, not NaN)
    float* ptr = reinterpret_cast<float*>(img.data);
    bool has_valid_data = false;
    for (int64_t i = 0; i < static_cast<int64_t>(kWidth) * kHeight; i += 1000) {  // Sample every 1000th pixel
        if (ptr[i] > 0.0f && ptr[i] < 1e6f && !std::isnan(ptr[i])) {
            has_valid_data = true;
            break;
        }
    }
    EXPECT_TRUE(has_valid_data) << "Output image appears to contain only invalid values";

    free_img(img);
}

/**
 * @test FullPipeline_512x512_Within200ms
 * @brief Quick smoke test with smaller image size
 *
 * This test runs the same pipeline on a 512x512 image with a 200ms budget.
 * Useful for rapid development iteration.
 */
TEST(FullPipelineE2E, PostProcess_512x512_Within200ms) {
    constexpr int32_t kWidth = 512;
    constexpr int32_t kHeight = 512;
    constexpr float kPixelValue = 500.0f;

    auto img = make_f32(kWidth, kHeight, kPixelValue);

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

    XpeCollimationParams coll_params{};
    XpeGridParams grid_params{};
    grid_params.enabled = true;
    XpeVignetteParams vignette_params{};
    XpeGsvParams gsv_params{};
    gsv_params.collimation = &coll_params;
    gsv_params.grid = &grid_params;
    gsv_params.vignette = &vignette_params;

    XpeModalityLutParams mod_lut_params{};
    XpeVoiLutParams voi_lut_params{};
    voi_lut_params.width = kWidth;
    voi_lut_params.height = kHeight;
    XpePresentationLutParams pres_lut_params{};
    pres_lut_params.gsdf_apply = true;

    auto t0 = std::chrono::high_resolution_clock::now();

    ASSERT_EQ(XPE_OK, xpe_calc_exposure_index(&img, &meta, &outEI, &outDI));
    ASSERT_EQ(XPE_OK, xpe_log_transform(&img, 1000.0f));
    ASSERT_EQ(XPE_OK, xpe_noise_reduce(&img, &nr_params));
    ASSERT_EQ(XPE_OK, xpe_contrast_enhance(&img, &clahe_params));
    ASSERT_EQ(XPE_OK, xpe_edge_enhance(&img, &usm_params));
    ASSERT_EQ(XPE_OK, xpe_gsv_process(&img, &gsv_params));
    ASSERT_EQ(XPE_OK, xpe_display_modality_lut(&img, &mod_lut_params));
    ASSERT_EQ(XPE_OK, xpe_display_voi_lut_preset(&img, XPE_VOI_PRESET_BONE, &voi_lut_params));
    ASSERT_EQ(XPE_OK, xpe_display_presentation_lut(&img, &pres_lut_params));

    auto t1 = std::chrono::high_resolution_clock::now();
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    EXPECT_LE(total_ms, 200)
        << "Full pipeline (512x512) took " << total_ms << "ms (budget: 200ms)";

    free_img(img);
}
