/**
 * @file test_ai_abi.cpp
 * @brief C ABI smoke tests for xpe_ai.dll -- SPEC-XPE-P3-AI
 *
 * Validates the C ABI boundary: version string, init/shutdown lifecycle,
 * null/invalid input handling, and deterministic size estimation.
 *
 * REQ-AI-001: Layer 1 dependency (xpe_common only).
 * REQ-AI-005: Opt-in activation (default off until init).
 *
 * @ingroup xpe_ai_tests
 */

#include "xpe/ai/ai_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include <gtest/gtest.h>

#include <cstring>
#include <vector>
#include <cstdint>

/* ============================================================================
 * Helper: create a valid XpeImageBuffer for testing
 * ============================================================================ */

static XpeImageBuffer makeTestBuffer(uint32_t w, uint32_t h,
                                      std::vector<uint16_t>& storage)
{
    storage.assign(w * h, 1000);
    return XpeImageBuffer{
        w, h,
        16,    // bitsAllocated
        12,    // bitsStored
        XPE_PIXEL_UINT16,
        storage.data(),
        static_cast<size_t>(w * h * 2)
    };
}

/* ============================================================================
 * Version Tests
 * ============================================================================ */

TEST(AiAbi, VersionReturnsNonNull) {
    const char* ver = xpe_ai_version();
    ASSERT_NE(ver, nullptr);
}

TEST(AiAbi, VersionReturnsNonEmptyString) {
    const char* ver = xpe_ai_version();
    ASSERT_NE(ver, nullptr);
    EXPECT_GT(std::strlen(ver), 0u);
}

TEST(AiAbi, VersionFormatIsSemanticVersioning) {
    const char* ver = xpe_ai_version();
    ASSERT_NE(ver, nullptr);

    // Expect format "X.Y.Z"
    int major = 0, minor = 0, patch = 0;
    int parsed = sscanf_s(ver, "%d.%d.%d", &major, &minor, &patch);
    EXPECT_EQ(parsed, 3) << "Version string must be X.Y.Z format, got: " << ver;
    EXPECT_GE(major, 0);
    EXPECT_GE(minor, 0);
    EXPECT_GE(patch, 0);
}

TEST(AiAbi, VersionIsDeterministicAcrossCalls) {
    const char* first = xpe_ai_version();
    const char* second = xpe_ai_version();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_STREQ(first, second);
}

/* ============================================================================
 * Init Tests
 * ============================================================================ */

TEST(AiAbi, InitWithNullModelDirReturnsInvalidInput) {
    XpeErrorCode ec = xpe_ai_init(nullptr, nullptr);
    EXPECT_EQ(ec, XPE_ERR_INVALID_INPUT);
}

TEST(AiAbi, InitWithValidPathSucceeds) {
    // Ensure clean state
    xpe_ai_shutdown();

    XpeErrorCode ec = xpe_ai_init("dummy_model_dir", nullptr);
    EXPECT_EQ(ec, XPE_OK);

    xpe_ai_shutdown();
}

TEST(AiAbi, InitWithNullConfigUsesDefaults) {
    xpe_ai_shutdown();

    XpeErrorCode ec = xpe_ai_init("dummy_model_dir", nullptr);
    EXPECT_EQ(ec, XPE_OK);

    xpe_ai_shutdown();
}

TEST(AiAbi, InitWithConfigJsonSucceeds) {
    xpe_ai_shutdown();

    const char* config = R"({"execution_provider":"cpu","timeout_ms":3000})";
    XpeErrorCode ec = xpe_ai_init("dummy_model_dir", config);
    EXPECT_EQ(ec, XPE_OK);

    xpe_ai_shutdown();
}

TEST(AiAbi, InitIsIdempotent) {
    xpe_ai_shutdown();

    XpeErrorCode ec1 = xpe_ai_init("dummy_model_dir", nullptr);
    EXPECT_EQ(ec1, XPE_OK);

    // Second init should be a no-op (returns XPE_OK)
    XpeErrorCode ec2 = xpe_ai_init("dummy_model_dir", nullptr);
    EXPECT_EQ(ec2, XPE_OK);

    xpe_ai_shutdown();
}

TEST(AiAbi, InitShutdownCycleRepeated) {
    // Verify no resource leaks across multiple init/shutdown cycles
    for (int i = 0; i < 10; ++i) {
        xpe_ai_shutdown();
        EXPECT_EQ(xpe_ai_init("dummy_model_dir", nullptr), XPE_OK);
    }
    xpe_ai_shutdown();
}

/* ============================================================================
 * Shutdown Tests
 * ============================================================================ */

TEST(AiAbi, ShutdownWithoutInitIsNoOp) {
    // Double shutdown without init -- must not crash
    xpe_ai_shutdown();
    xpe_ai_shutdown();
    SUCCEED();
}

TEST(AiAbi, ShutdownIsIdempotent) {
    xpe_ai_init("dummy_model_dir", nullptr);
    xpe_ai_shutdown();
    xpe_ai_shutdown();  // Second call -- must not crash
    xpe_ai_shutdown();  // Third call -- must not crash
    SUCCEED();
}

/* ============================================================================
 * xpe_stitch_estimate_size Tests (deterministic, no AI needed)
 * ============================================================================ */

TEST(AiAbi, StitchEstimateSizeDeterministic) {
    // Same input always produces same output
    std::vector<uint16_t> storage1, storage2, storage3;

    auto run = [&]() -> std::pair<uint32_t, uint32_t> {
        XpeImageBuffer parts[2];
        parts[0] = makeTestBuffer(256, 512, storage1);
        parts[1] = makeTestBuffer(256, 512, storage2);

        uint32_t w = 0, h = 0;
        XpeErrorCode ec = xpe_stitch_estimate_size(parts, 2, &w, &h);
        EXPECT_EQ(ec, XPE_OK);
        return {w, h};
    };

    auto [w1, h1] = run();
    auto [w2, h2] = run();
    auto [w3, h3] = run();

    EXPECT_EQ(w1, w2);
    EXPECT_EQ(w2, w3);
    EXPECT_EQ(h1, h2);
    EXPECT_EQ(h2, h3);
}

TEST(AiAbi, StitchEstimateSizeReturnsValidDimensions) {
    std::vector<uint16_t> s1, s2;

    XpeImageBuffer parts[2];
    parts[0] = makeTestBuffer(256, 512, s1);
    parts[1] = makeTestBuffer(256, 512, s2);

    uint32_t w = 0, h = 0;
    XpeErrorCode ec = xpe_stitch_estimate_size(parts, 2, &w, &h);
    ASSERT_EQ(ec, XPE_OK);

    EXPECT_GT(w, 0u);
    EXPECT_GT(h, 0u);
    EXPECT_LE(w, 4096u);
    EXPECT_LE(h, 4096u);
}

TEST(AiAbi, StitchEstimateSizeNullPartsReturnsInvalid) {
    uint32_t w = 0, h = 0;
    EXPECT_EQ(xpe_stitch_estimate_size(nullptr, 2, &w, &h),
              XPE_ERR_INVALID_INPUT);
}

TEST(AiAbi, StitchEstimateSizeNullOutputsReturnInvalid) {
    std::vector<uint16_t> s1, s2;
    XpeImageBuffer parts[2];
    parts[0] = makeTestBuffer(256, 512, s1);
    parts[1] = makeTestBuffer(256, 512, s2);

    uint32_t w = 0;
    EXPECT_EQ(xpe_stitch_estimate_size(parts, 2, &w, nullptr),
              XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(xpe_stitch_estimate_size(parts, 2, nullptr, &w),
              XPE_ERR_INVALID_INPUT);
}

TEST(AiAbi, StitchEstimateSizeSinglePartReturnsInvalid) {
    std::vector<uint16_t> s1;
    XpeImageBuffer parts[1];
    parts[0] = makeTestBuffer(256, 512, s1);

    uint32_t w = 0, h = 0;
    EXPECT_EQ(xpe_stitch_estimate_size(parts, 1, &w, &h),
              XPE_ERR_INVALID_INPUT);
}

TEST(AiAbi, StitchEstimateSizeInvalidBufferReturnsInvalid) {
    XpeImageBuffer parts[2] = {};
    parts[0].width = 0;
    parts[0].height = 0;

    uint32_t w = 0, h = 0;
    EXPECT_EQ(xpe_stitch_estimate_size(parts, 2, &w, &h),
              XPE_ERR_INVALID_INPUT);
}

TEST(AiAbi, StitchEstimateSizeClampsToMax4096) {
    // Create parts large enough to exceed 4096 when combined
    std::vector<uint16_t> s1, s2;
    XpeImageBuffer parts[2];
    parts[0] = makeTestBuffer(3000, 3000, s1);
    parts[1] = makeTestBuffer(3000, 3000, s2);

    uint32_t w = 0, h = 0;
    XpeErrorCode ec = xpe_stitch_estimate_size(parts, 2, &w, &h);
    ASSERT_EQ(ec, XPE_OK);

    EXPECT_LE(w, 4096u);
    EXPECT_LE(h, 4096u);
}

/* ============================================================================
 * Not-initialized guard tests
 * ============================================================================ */

TEST(AiAbi, BodypartRecognizeWithoutInitReturnsNotInitialized) {
    xpe_ai_shutdown();

    XpeImageBuffer buf{};
    char label[64] = {};
    float conf = 0.0f;
    EXPECT_EQ(xpe_bodypart_recognize(&buf, label, sizeof(label), &conf),
              XPE_ERR_NOT_INITIALIZED);
}

TEST(AiAbi, StitchImagesWithoutInitReturnsNotInitialized) {
    xpe_ai_shutdown();

    XpeImageBuffer parts[2] = {};
    XpeImageBuffer out = {};
    EXPECT_EQ(xpe_stitch_images(parts, 2, &out, nullptr),
              XPE_ERR_NOT_INITIALIZED);
}

TEST(AiAbi, BoneSuppressWithoutInitReturnsNotInitialized) {
    xpe_ai_shutdown();

    XpeImageBuffer img{};
    XpeImageBuffer out{};
    EXPECT_EQ(xpe_bone_suppress(&img, &out, nullptr),
              XPE_ERR_NOT_INITIALIZED);
}

TEST(AiAbi, DlDenoiseWithoutInitReturnsNotInitialized) {
    xpe_ai_shutdown();

    XpeImageBuffer img{};
    XpeImageMetadata meta{};
    EXPECT_EQ(xpe_dl_denoise(&img, &meta, nullptr),
              XPE_ERR_NOT_INITIALIZED);
}

TEST(AiAbi, GetModelCardWithoutInitReturnsNotInitialized) {
    xpe_ai_shutdown();

    char buf[4096] = {};
    EXPECT_EQ(xpe_ai_get_model_card("some_model", buf, sizeof(buf)),
              XPE_ERR_NOT_INITIALIZED);
}

TEST(AiAbi, SetFallbackModeWithoutInitReturnsNotInitialized) {
    xpe_ai_shutdown();

    EXPECT_EQ(xpe_ai_set_fallback_mode(1),
              XPE_ERR_NOT_INITIALIZED);
}
