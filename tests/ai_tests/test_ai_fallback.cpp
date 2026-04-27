/**
 * @file test_ai_fallback.cpp
 * @brief Fallback routing tests for xpe_ai.dll -- SPEC-XPE-P3-AI
 *
 * Validates deterministic fallback behavior:
 * - xpe_ai_set_fallback_mode() toggles
 * - Stub functions return XPE_ERR_PROCESSING_FAILED
 * - Confidence threshold handling
 * - Input validation for inference functions
 *
 * REQ-AI-002: Deterministic fallback for all AI functions.
 * REQ-AI-012: Low-confidence event triggers fallback.
 *
 * @ingroup xpe_ai_tests
 */

#include "xpe/ai/ai_api.h"
#include "xpe/ai/ai_worker_protocol.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include <gtest/gtest.h>

#include <cstring>
#include <vector>
#include <cstdint>

/* ============================================================================
 * Test Fixture: initialize AI module before each test
 * ============================================================================ */

class AiFallbackTest : public ::testing::Test {
protected:
    void SetUp() override {
        xpe_ai_shutdown();
        ASSERT_EQ(xpe_ai_init("dummy_model_dir", nullptr), XPE_OK);
    }

    void TearDown() override {
        xpe_ai_shutdown();
    }
};

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
 * Fallback Mode Toggle Tests
 * ============================================================================ */

TEST_F(AiFallbackTest, SetFallbackModeEnableReturnsOk) {
    EXPECT_EQ(xpe_ai_set_fallback_mode(1), XPE_OK);
}

TEST_F(AiFallbackTest, SetFallbackModeDisableReturnsOk) {
    EXPECT_EQ(xpe_ai_set_fallback_mode(0), XPE_OK);
}

TEST_F(AiFallbackTest, SetFallbackModeToggleRepeated) {
    // Toggle multiple times -- all should succeed
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(xpe_ai_set_fallback_mode(i % 2), XPE_OK);
    }
}

TEST_F(AiFallbackTest, SetFallbackModeNonZeroEnables) {
    // Any non-zero value should enable fallback mode
    EXPECT_EQ(xpe_ai_set_fallback_mode(42), XPE_OK);
    EXPECT_EQ(xpe_ai_set_fallback_mode(-1), XPE_OK);
    EXPECT_EQ(xpe_ai_set_fallback_mode(1000), XPE_OK);
}

/* ============================================================================
 * Stub Function Returns XPE_ERR_PROCESSING_FAILED
 * ============================================================================ */

TEST_F(AiFallbackTest, BodypartRecognizeStubReturnsProcessingFailed) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, storage);

    char label[64] = {};
    float confidence = 1.0f;

    XpeErrorCode ec = xpe_bodypart_recognize(&img, label, sizeof(label),
                                              &confidence);
    EXPECT_EQ(ec, XPE_ERR_PROCESSING_FAILED);
}

TEST_F(AiFallbackTest, BodypartRecognizeSetsConfidenceToZero) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, storage);

    char label[64] = {};
    float confidence = 1.0f;

    xpe_bodypart_recognize(&img, label, sizeof(label), &confidence);

    // In stub mode, confidence should be set to 0.0
    EXPECT_FLOAT_EQ(confidence, 0.0f);
}

TEST_F(AiFallbackTest, BodypartRecognizeSetsUnknownLabel) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, storage);

    char label[64] = {};
    xpe_bodypart_recognize(&img, label, sizeof(label), nullptr);

    EXPECT_STREQ(label, "UNKNOWN");
}

TEST_F(AiFallbackTest, StitchImagesStubReturnsProcessingFailed) {
    std::vector<uint16_t> s1, s2, s3;
    XpeImageBuffer parts[2];
    parts[0] = makeTestBuffer(256, 512, s1);
    parts[1] = makeTestBuffer(256, 512, s2);

    XpeImageBuffer out = makeTestBuffer(1024, 512, s3);

    XpeErrorCode ec = xpe_stitch_images(parts, 2, &out, nullptr);
    EXPECT_EQ(ec, XPE_ERR_PROCESSING_FAILED);
}

TEST_F(AiFallbackTest, BoneSuppressStubReturnsProcessingFailed) {
    std::vector<uint16_t> in_storage, out_storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, in_storage);
    XpeImageBuffer out = makeTestBuffer(64, 64, out_storage);

    XpeErrorCode ec = xpe_bone_suppress(&img, &out, nullptr);
    EXPECT_EQ(ec, XPE_ERR_PROCESSING_FAILED);
}

TEST_F(AiFallbackTest, DlDenoiseStubReturnsProcessingFailed) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, storage);

    XpeImageMetadata meta{};
    std::strcpy(meta.bodyPart, "CHEST");
    meta.mAs = 2.0f;

    XpeErrorCode ec = xpe_dl_denoise(&img, &meta, nullptr);
    EXPECT_EQ(ec, XPE_ERR_PROCESSING_FAILED);
}

/* ============================================================================
 * Confidence Threshold Default Value
 * ============================================================================ */

TEST_F(AiFallbackTest, ConfidenceThresholdDefaultIs06) {
    // Verify the protocol default constant matches the spec (0.6)
    EXPECT_FLOAT_EQ(XPE_AI_DEFAULT_CONFIDENCE_THRESHOLD, 0.6f);
}

TEST_F(AiFallbackTest, ConfidenceThresholdInRange) {
    // The default confidence threshold must be in [0, 1]
    EXPECT_GE(XPE_AI_DEFAULT_CONFIDENCE_THRESHOLD, 0.0f);
    EXPECT_LE(XPE_AI_DEFAULT_CONFIDENCE_THRESHOLD, 1.0f);
}

/* ============================================================================
 * Input Validation for Inference Functions
 * ============================================================================ */

TEST_F(AiFallbackTest, BodypartRecognizeNullImgReturnsInvalid) {
    char label[64] = {};
    float conf = 0.0f;
    EXPECT_EQ(xpe_bodypart_recognize(nullptr, label, sizeof(label), &conf),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(AiFallbackTest, BodypartRecognizeNullLabelReturnsInvalid) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, storage);
    float conf = 0.0f;
    EXPECT_EQ(xpe_bodypart_recognize(&img, nullptr, 64, &conf),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(AiFallbackTest, BodypartRecognizeZeroBufLenReturnsTooSmall) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, storage);
    char label[64] = {};
    EXPECT_EQ(xpe_bodypart_recognize(&img, label, 0, nullptr),
              XPE_ERR_BUFFER_TOO_SMALL);
}

TEST_F(AiFallbackTest, BodypartRecognizeNullConfOutIsAcceptable) {
    // confidenceOut may be NULL per API contract
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, storage);
    char label[64] = {};

    // Should not return INVALID_INPUT for null confidenceOut
    XpeErrorCode ec = xpe_bodypart_recognize(&img, label, sizeof(label), nullptr);
    // In stub mode, returns PROCESSING_FAILED but not INVALID_INPUT
    EXPECT_NE(ec, XPE_ERR_INVALID_INPUT);
}

TEST_F(AiFallbackTest, StitchImagesNullPartsReturnsInvalid) {
    std::vector<uint16_t> storage;
    XpeImageBuffer out = makeTestBuffer(512, 512, storage);
    EXPECT_EQ(xpe_stitch_images(nullptr, 2, &out, nullptr),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(AiFallbackTest, StitchImagesPartCountOneReturnsInvalid) {
    std::vector<uint16_t> s1, s2;
    XpeImageBuffer parts[1];
    parts[0] = makeTestBuffer(256, 512, s1);
    XpeImageBuffer out = makeTestBuffer(512, 512, s2);

    EXPECT_EQ(xpe_stitch_images(parts, 1, &out, nullptr),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(AiFallbackTest, StitchImagesNullOutputReturnsInvalid) {
    std::vector<uint16_t> s1, s2;
    XpeImageBuffer parts[2];
    parts[0] = makeTestBuffer(256, 512, s1);
    parts[1] = makeTestBuffer(256, 512, s2);

    EXPECT_EQ(xpe_stitch_images(parts, 2, nullptr, nullptr),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(AiFallbackTest, StitchImagesNullOutputDataReturnsTooSmall) {
    std::vector<uint16_t> s1, s2;
    XpeImageBuffer parts[2];
    parts[0] = makeTestBuffer(256, 512, s1);
    parts[1] = makeTestBuffer(256, 512, s2);

    XpeImageBuffer out{};
    out.data = nullptr;
    out.dataSize = 0;

    EXPECT_EQ(xpe_stitch_images(parts, 2, &out, nullptr),
              XPE_ERR_BUFFER_TOO_SMALL);
}

TEST_F(AiFallbackTest, BoneSuppressNullImgReturnsInvalid) {
    std::vector<uint16_t> storage;
    XpeImageBuffer out = makeTestBuffer(64, 64, storage);
    EXPECT_EQ(xpe_bone_suppress(nullptr, &out, nullptr),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(AiFallbackTest, BoneSuppressNullOutputReturnsInvalid) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, storage);
    EXPECT_EQ(xpe_bone_suppress(&img, nullptr, nullptr),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(AiFallbackTest, BoneSuppressDimensionMismatchReturnsInvalid) {
    std::vector<uint16_t> in_storage, out_storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, in_storage);
    XpeImageBuffer out = makeTestBuffer(128, 64, out_storage);

    EXPECT_EQ(xpe_bone_suppress(&img, &out, nullptr),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(AiFallbackTest, DlDenoiseNullImgReturnsInvalid) {
    XpeImageMetadata meta{};
    EXPECT_EQ(xpe_dl_denoise(nullptr, &meta, nullptr),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(AiFallbackTest, DlDenoiseNullMetaReturnsInvalid) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, storage);
    EXPECT_EQ(xpe_dl_denoise(&img, nullptr, nullptr),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(AiFallbackTest, BodypartRecognizeInvalidBufferReturnsInvalid) {
    // Zero-sized image
    XpeImageBuffer img{};
    img.width = 0;
    img.height = 0;

    char label[64] = {};
    float conf = 0.0f;
    EXPECT_EQ(xpe_bodypart_recognize(&img, label, sizeof(label), &conf),
              XPE_ERR_INVALID_INPUT);
}
