/**
 * @file test_voi_lut.cpp
 * @brief Unit tests for xpe_apply_voi_lut and xpe_voi_preset_create (SWU-3.2)
 * SPEC: SPEC-XPE-P1B-DISP
 * REQ-DISP-009 to REQ-DISP-018
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <chrono>
#include <vector>
#include "perf_measure.h"

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

static float* pixels(XpeImageBuffer& img) {
    return static_cast<float*>(img.data);
}

// =============================================================================
// REQ-DISP-009: LINEAR windowing
// =============================================================================

TEST(VoiLut, Linear_CenterWindow) {
    // REQ-DISP-009: LINEAR: output[i] = clamp((input[i] - (center - width/2)) / width * range + minOut, minOut, maxOut)
    // center=500, width=1000, minOut=0, maxOut=255
    // input = center -> output = 0 + (500 - (500-500)) / 1000 * 255 = 127.5
    XpeImageBuffer img = make_float32_image(1, 1, 500.0f);
    XpeVoiLutParams params{};
    params.mode   = XPE_VOI_LINEAR;
    params.center = 500.0f;
    params.width  = 1000.0f;
    params.minOut = 0.0f;
    params.maxOut = 255.0f;

    XpeErrorCode rc = xpe_apply_voi_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    // (500 - (500 - 500)) / 1000 * 255 + 0 = 127.5
    EXPECT_NEAR(pixels(img)[0], 127.5f, 0.5f);
    free_image(img);
}

TEST(VoiLut, Linear_ClampMin) {
    // REQ-DISP-012: output clamped to minOut
    XpeImageBuffer img = make_float32_image(1, 1, -9999.0f);
    XpeVoiLutParams params{};
    params.mode   = XPE_VOI_LINEAR;
    params.center = 0.0f;
    params.width  = 1000.0f;
    params.minOut = 0.0f;
    params.maxOut = 255.0f;

    XpeErrorCode rc = xpe_apply_voi_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_FLOAT_EQ(pixels(img)[0], 0.0f);
    free_image(img);
}

TEST(VoiLut, Linear_ClampMax) {
    // REQ-DISP-012: output clamped to maxOut
    XpeImageBuffer img = make_float32_image(1, 1, 9999.0f);
    XpeVoiLutParams params{};
    params.mode   = XPE_VOI_LINEAR;
    params.center = 0.0f;
    params.width  = 1000.0f;
    params.minOut = 0.0f;
    params.maxOut = 255.0f;

    XpeErrorCode rc = xpe_apply_voi_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_FLOAT_EQ(pixels(img)[0], 255.0f);
    free_image(img);
}

// =============================================================================
// REQ-DISP-010: LINEAR_EXACT windowing (DICOM PS3.3 C.11.2.1.3)
// =============================================================================

TEST(VoiLut, LinearExact_CenterValue) {
    // REQ-DISP-010: LINEAR_EXACT center maps to midpoint of [minOut, maxOut]
    XpeImageBuffer img = make_float32_image(1, 1, 40.0f);
    XpeVoiLutParams params{};
    params.mode   = XPE_VOI_LINEAR_EXACT;
    params.center = 40.0f;
    params.width  = 80.0f;
    params.minOut = 0.0f;
    params.maxOut = 255.0f;

    XpeErrorCode rc = xpe_apply_voi_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    // center input -> output = (minOut + maxOut) / 2 = 127.5
    EXPECT_NEAR(pixels(img)[0], 127.5f, 1.0f);
    free_image(img);
}

// =============================================================================
// REQ-DISP-011: SIGMOID windowing
// =============================================================================

TEST(VoiLut, Sigmoid_CenterValue) {
    // REQ-DISP-011: SIGMOID center -> output near midpoint
    // sigmoid(0) = 0.5, so center -> (maxOut - minOut) * 0.5 + minOut
    XpeImageBuffer img = make_float32_image(1, 1, 500.0f);
    XpeVoiLutParams params{};
    params.mode   = XPE_VOI_SIGMOID;
    params.center = 500.0f;
    params.width  = 1000.0f;
    params.minOut = 0.0f;
    params.maxOut = 255.0f;

    XpeErrorCode rc = xpe_apply_voi_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    // At center: sigmoid(0) = 0.5 -> output = 127.5
    EXPECT_NEAR(pixels(img)[0], 127.5f, 1.0f);
    free_image(img);
}

TEST(VoiLut, Sigmoid_OutputClampedToRange) {
    // REQ-DISP-012: SIGMOID output still clamped to [minOut, maxOut]
    // Extreme high value approaches maxOut
    XpeImageBuffer img = make_float32_image(1, 1, 99999.0f);
    XpeVoiLutParams params{};
    params.mode   = XPE_VOI_SIGMOID;
    params.center = 0.0f;
    params.width  = 1000.0f;
    params.minOut = 0.0f;
    params.maxOut = 255.0f;

    XpeErrorCode rc = xpe_apply_voi_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_LE(pixels(img)[0], 255.0f);
    EXPECT_GE(pixels(img)[0], 0.0f);
    free_image(img);
}

// =============================================================================
// REQ-DISP-013 to REQ-DISP-014: Error cases
// =============================================================================

TEST(VoiLut, Error_NullImg) {
    // REQ-DISP-013: NULL img -> XPE_ERR_INVALID_INPUT
    XpeVoiLutParams params{};
    params.mode  = XPE_VOI_LINEAR;
    params.width = 1000.0f;
    XpeErrorCode rc = xpe_apply_voi_lut(nullptr, &params);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
}

TEST(VoiLut, Error_NullParams) {
    // REQ-DISP-013: NULL params -> XPE_ERR_INVALID_INPUT
    XpeImageBuffer img = make_float32_image(2, 2, 0.0f);
    XpeErrorCode rc = xpe_apply_voi_lut(&img, nullptr);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
    free_image(img);
}

TEST(VoiLut, Error_WrongFormat) {
    // REQ-DISP-014: format != FLOAT32 -> XPE_ERR_UNSUPPORTED_FORMAT
    uint16_t buf[4] = {100, 200, 300, 400};
    XpeImageBuffer img{};
    img.width = 2; img.height = 2;
    img.format = XPE_PIXEL_UINT16;
    img.bitsAllocated = 16; img.bitsStored = 16;
    img.dataSize = 8;
    img.data = buf;

    XpeVoiLutParams params{};
    params.mode  = XPE_VOI_LINEAR;
    params.width = 1000.0f;
    XpeErrorCode rc = xpe_apply_voi_lut(&img, &params);
    EXPECT_EQ(rc, XPE_ERR_UNSUPPORTED_FORMAT);
    EXPECT_EQ(buf[0], 100); // unchanged
}

TEST(VoiLut, Error_ZeroWidth) {
    // REQ-DISP-015: width <= 0 -> XPE_ERR_INVALID_INPUT; image unchanged
    XpeImageBuffer img = make_float32_image(2, 2, 42.0f);
    XpeVoiLutParams params{};
    params.mode   = XPE_VOI_LINEAR;
    params.width  = 0.0f;
    params.minOut = 0.0f;
    params.maxOut = 255.0f;
    XpeErrorCode rc = xpe_apply_voi_lut(&img, &params);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
    EXPECT_FLOAT_EQ(pixels(img)[0], 42.0f); // must not be modified
    free_image(img);
}

TEST(VoiLut, Error_NegativeWidth) {
    // REQ-DISP-015: negative width also invalid
    XpeImageBuffer img = make_float32_image(1, 1, 42.0f);
    XpeVoiLutParams params{};
    params.mode  = XPE_VOI_LINEAR;
    params.width = -100.0f;
    XpeErrorCode rc = xpe_apply_voi_lut(&img, &params);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
    free_image(img);
}

// =============================================================================
// REQ-DISP-017: Presets
// =============================================================================

TEST(VoiLut, Preset_Bone) {
    // REQ-DISP-017 (revised 2026-09-17, #177): provisional full 16-bit DN
    // window for every body part until #151 supplies per-part DN values.
    XpeVoiLutParams params{};
    XpeErrorCode rc = xpe_voi_preset_create(&params, XPE_BODY_BONE);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_FLOAT_EQ(params.center, 32768.0f);
    EXPECT_FLOAT_EQ(params.width, 65535.0f);
}

TEST(VoiLut, Preset_Lung) {
    // REQ-DISP-017 (revised 2026-09-17, #177): provisional full 16-bit DN
    // window for every body part until #151 supplies per-part DN values.
    XpeVoiLutParams params{};
    XpeErrorCode rc = xpe_voi_preset_create(&params, XPE_BODY_LUNG);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_FLOAT_EQ(params.center, 32768.0f);
    EXPECT_FLOAT_EQ(params.width, 65535.0f);
}

TEST(VoiLut, Preset_Abdomen) {
    // REQ-DISP-017 (revised 2026-09-17, #177): provisional full 16-bit DN
    // window for every body part until #151 supplies per-part DN values.
    XpeVoiLutParams params{};
    XpeErrorCode rc = xpe_voi_preset_create(&params, XPE_BODY_ABDOMEN);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_FLOAT_EQ(params.center, 32768.0f);
    EXPECT_FLOAT_EQ(params.width, 65535.0f);
}

TEST(VoiLut, Preset_Head) {
    // REQ-DISP-017 (revised 2026-09-17, #177): provisional full 16-bit DN
    // window for every body part until #151 supplies per-part DN values.
    XpeVoiLutParams params{};
    XpeErrorCode rc = xpe_voi_preset_create(&params, XPE_BODY_HEAD);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_FLOAT_EQ(params.center, 32768.0f);
    EXPECT_FLOAT_EQ(params.width, 65535.0f);
}

TEST(VoiLut, Preset_InvalidBodyPart) {
    // REQ-DISP-018: invalid bodyPart -> XPE_ERR_INVALID_INPUT
    XpeVoiLutParams params{};
    XpeErrorCode rc = xpe_voi_preset_create(&params, static_cast<XpeBodyPart>(999));
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
}

TEST(VoiLut, Preset_NullParams) {
    // REQ-DISP-018: NULL params -> XPE_ERR_INVALID_INPUT
    XpeErrorCode rc = xpe_voi_preset_create(nullptr, XPE_BODY_BONE);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
}

// #177 (QA-B-82): the presets act on detector DN, not HU (REQ-DISP-017 as
// revised 2026-09-17). A constant assertion only compares a value with itself,
// so this case checks the behaviour instead: over the whole 16-bit DN ramp a
// preset must not crush the image into one output value. GUI-C-84 measured
// the HU-era Abdomen 40/400 turning a wrist fixture into a single level.
//
// Two measures, because they fail differently: a narrow HU window still yields
// a few hundred distinct levels on a full ramp, but clips almost every pixel
// to one end -- so the share of the most common level is the one that catches
// the crush; the distinct count guards against a window so wide or so shifted
// that the ramp barely moves the output.
TEST(VoiLut, Preset_DnRampIsNotCrushed) {
    const uint32_t kW = 256, kH = 256;   // 65536 pixels, one per DN value
    const struct { XpeBodyPart part; const char* name; } kParts[] = {
        {XPE_BODY_BONE, "BONE"}, {XPE_BODY_LUNG, "LUNG"},
        {XPE_BODY_ABDOMEN, "ABDOMEN"}, {XPE_BODY_HEAD, "HEAD"},
    };
    for (const auto& part : kParts) {
        SCOPED_TRACE(part.name);
        XpeVoiLutParams params{};
        ASSERT_EQ(XPE_OK, xpe_voi_preset_create(&params, part.part));

        XpeImageBuffer img = make_float32_image(kW, kH, 0.0f);
        float* px = pixels(img);
        for (size_t i = 0; i < (size_t)kW * kH; ++i) px[i] = static_cast<float>(i);
        ASSERT_EQ(XPE_OK, xpe_apply_voi_lut(&img, &params));

        // Quantise to the preset's own output range in 256 steps.
        std::vector<size_t> hist(256, 0);
        const float range = params.maxOut - params.minOut;
        for (size_t i = 0; i < (size_t)kW * kH; ++i) {
            float t = (px[i] - params.minOut) / range;
            int bin = static_cast<int>(std::lround(t * 255.0f));
            bin = bin < 0 ? 0 : (bin > 255 ? 255 : bin);
            ++hist[static_cast<size_t>(bin)];
        }
        size_t distinct = 0, largest = 0;
        for (size_t n : hist) {
            if (n) ++distinct;
            if (n > largest) largest = n;
        }
        const double modeShare = static_cast<double>(largest) / (kW * kH);
        GTEST_LOG_(INFO) << part.name << " c=" << params.center << " w=" << params.width
                         << " | distinct 8-bit levels=" << distinct
                         << " | largest level share=" << modeShare;

        EXPECT_GE(distinct, 128u) << "the DN ramp reaches fewer than half the output levels";
        EXPECT_LE(modeShare, 0.5) << "more than half of the DN ramp lands on one output level";
        free_image(img);
    }
}

// =============================================================================
// REQ-DISP-016: Performance test (<= 16ms for 3072x3072)
// =============================================================================

TEST(VoiLut, Performance_3072x3072) {
    // REQ-DISP-016: LINEAR windowing <= 16ms for 3072x3072
    XpeImageBuffer img = make_float32_image(3072, 3072, 500.0f);
    XpeVoiLutParams params{};
    params.mode   = XPE_VOI_LINEAR;
    params.center = 500.0f;
    params.width  = 1000.0f;
    params.minOut = 0.0f;
    params.maxOut = 255.0f;

    auto t0 = std::chrono::high_resolution_clock::now();
    XpeErrorCode rc = xpe_apply_voi_lut(&img, &params);
    auto t1 = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    EXPECT_EQ(rc, XPE_OK);
    EXPECT_LE(ms, 16) << "VoiLUT LINEAR 3072x3072 took " << ms << "ms (limit 16ms)";
    free_image(img);
}

// ---------------------------------------------------------------------------
// #179 (QA-B-86): measure-only benchmark at the SPEC size -- no time assertion.
// The name carries BenchmarkFreeze (selected by benchmark-regression.yml -R)
// and Performance (excluded by ci.yml -E, which runs on shared runners).
// ---------------------------------------------------------------------------
TEST(VoiLut, BenchmarkFreeze_Performance_REQ_DISP_016_Linear3072) {
    constexpr uint32_t kSize = 3072;
    const size_t n = static_cast<size_t>(kSize) * kSize;
    std::vector<float> pristine(n);
    for (size_t i = 0; i < n; ++i)
        pristine[i] = static_cast<float>((i * 2654435761u) % 65536u);
    XpeImageBuffer img = make_float32_image(kSize, kSize, 0.0f);
    auto reset = [&] {
        std::copy(pristine.begin(), pristine.end(), static_cast<float*>(img.data));
    };
    XpeVoiLutParams params{};
    ASSERT_EQ(XPE_OK, xpe_voi_preset_create(&params, XPE_BODY_BONE));
    perf_measure::Measure("REQ-DISP-016/xpe_apply_voi_lut", "3072x3072", reset,
                          [&] { return xpe_apply_voi_lut(&img, &params); });
    const float* out = static_cast<const float*>(img.data);
    for (size_t i = 0; i < n; ++i) {
        if (!std::isfinite(out[i])) { ADD_FAILURE() << "non-finite output at " << i; break; }
    }
    std::free(img.data);
}
