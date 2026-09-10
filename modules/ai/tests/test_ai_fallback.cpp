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

#include <cstdio>
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
    XpeImageBuffer parts[2]{};
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
    std::snprintf(meta.bodyPart, sizeof(meta.bodyPart), "%s", "CHEST");
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

// Renamed and re-asserted by QA-B-42. It expected XPE_ERR_BUFFER_TOO_SMALL,
// which was the behaviour before #142 drew the line between a missing output
// argument and a short one. The old expectation recorded what the code did;
// it is superseded, not wrong-then.
TEST_F(AiFallbackTest, BodypartRecognizeZeroBufLenReturnsInvalidInput) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, storage);
    char label[64] = {};
    EXPECT_EQ(xpe_bodypart_recognize(&img, label, 0, nullptr),
              XPE_ERR_INVALID_INPUT);
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
    XpeImageBuffer parts[1]{};
    parts[0] = makeTestBuffer(256, 512, s1);
    XpeImageBuffer out = makeTestBuffer(512, 512, s2);

    EXPECT_EQ(xpe_stitch_images(parts, 1, &out, nullptr),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(AiFallbackTest, StitchImagesNullOutputReturnsInvalid) {
    std::vector<uint16_t> s1, s2;
    XpeImageBuffer parts[2]{};
    parts[0] = makeTestBuffer(256, 512, s1);
    parts[1] = makeTestBuffer(256, 512, s2);

    EXPECT_EQ(xpe_stitch_images(parts, 2, nullptr, nullptr),
              XPE_ERR_INVALID_INPUT);
}

// Renamed and re-asserted by QA-B-42. It expected XPE_ERR_BUFFER_TOO_SMALL,
// which was the behaviour before #142 drew the line between a missing output
// argument and a short one. The old expectation recorded what the code did;
// it is superseded, not wrong-then.
TEST_F(AiFallbackTest, StitchImagesNullOutputDataReturnsInvalidInput) {
    std::vector<uint16_t> s1, s2;
    XpeImageBuffer parts[2]{};
    parts[0] = makeTestBuffer(256, 512, s1);
    parts[1] = makeTestBuffer(256, 512, s2);

    XpeImageBuffer out{};
    out.data = nullptr;
    out.dataSize = 0;

    EXPECT_EQ(xpe_stitch_images(parts, 2, &out, nullptr),
              XPE_ERR_INVALID_INPUT);
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

/* ============================================================================
 * Error-code precedence regression guard (#119)
 *
 * QA-B-13 moved the required-pointer NULL checks ahead of the initialisation
 * guard in five xpe_ai entry points, but nothing pinned that order — the 108
 * existing cases all pass with either order, so a refactor could silently undo
 * it. The two assertions below differ only in whether the arguments are NULL,
 * so they fail if the order is reversed (both would return NOT_INITIALIZED)
 * and equally if the initialisation guard is dropped (both INVALID_INPUT).
 * ============================================================================ */

class AiErrorPrecedenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure the module is NOT initialised.
        xpe_ai_shutdown();
    }

    void TearDown() override {
        // Restore initialised state for subsequent tests.
        xpe_ai_init("dummy_model_dir", nullptr);
    }
};

TEST_F(AiErrorPrecedenceTest, NullArgumentOutranksNotInitialized) {
    EXPECT_EQ(xpe_dl_denoise(nullptr, nullptr, nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(AiErrorPrecedenceTest, ValidArgumentsReachNotInitialized) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, storage);
    XpeImageMetadata meta{};
    EXPECT_EQ(xpe_dl_denoise(&img, &meta, nullptr), XPE_ERR_NOT_INITIALIZED);
}

// The same pair for the other four entry points QA-B-13 reordered. Each pair
// differs only in whether the required pointers are NULL, so a reversal shows
// up as both arms returning NOT_INITIALIZED and a dropped init guard as both
// returning INVALID_INPUT.

TEST_F(AiErrorPrecedenceTest, BodypartRecognize_NullArgumentOutranksNotInitialized) {
    EXPECT_EQ(xpe_bodypart_recognize(nullptr, nullptr, 0, nullptr),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(AiErrorPrecedenceTest, BodypartRecognize_ValidArgumentsReachNotInitialized) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, storage);
    char label[64] = {};
    float conf = 0.0f;
    EXPECT_EQ(xpe_bodypart_recognize(&img, label, sizeof(label), &conf),
              XPE_ERR_NOT_INITIALIZED);
}

TEST_F(AiErrorPrecedenceTest, StitchImages_NullArgumentOutranksNotInitialized) {
    EXPECT_EQ(xpe_stitch_images(nullptr, 2, nullptr, nullptr),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(AiErrorPrecedenceTest, StitchImages_ValidArgumentsReachNotInitialized) {
    std::vector<uint16_t> s0, s1;
    XpeImageBuffer parts[2] = { makeTestBuffer(64, 64, s0),
                                makeTestBuffer(64, 64, s1) };
    XpeImageBuffer stitched{};
    EXPECT_EQ(xpe_stitch_images(parts, 2, &stitched, nullptr),
              XPE_ERR_NOT_INITIALIZED);
}

TEST_F(AiErrorPrecedenceTest, BoneSuppress_NullArgumentOutranksNotInitialized) {
    EXPECT_EQ(xpe_bone_suppress(nullptr, nullptr, nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(AiErrorPrecedenceTest, BoneSuppress_ValidArgumentsReachNotInitialized) {
    std::vector<uint16_t> inStore, outStore;
    XpeImageBuffer img = makeTestBuffer(64, 64, inStore);
    XpeImageBuffer soft = makeTestBuffer(64, 64, outStore);
    EXPECT_EQ(xpe_bone_suppress(&img, &soft, nullptr), XPE_ERR_NOT_INITIALIZED);
}

TEST_F(AiErrorPrecedenceTest, GetModelCard_NullArgumentOutranksNotInitialized) {
    EXPECT_EQ(xpe_ai_get_model_card(nullptr, nullptr, 0), XPE_ERR_INVALID_INPUT);
}

TEST_F(AiErrorPrecedenceTest, GetModelCard_ValidArgumentsReachNotInitialized) {
    char buf[256] = {};
    EXPECT_EQ(xpe_ai_get_model_card("denoise", buf, sizeof(buf)),
              XPE_ERR_NOT_INITIALIZED);
}

/* ============================================================================
 * #123 (QA-B-21): XpeImageBuffer.dataSize size-consistency guard, per entry point
 *
 * Contract (api-spec "XpeImageBuffer.dataSize on input"):
 *   dataSize == 0                           -> unspecified, accepted
 *   0 < dataSize < width*height*bpp(format) -> XPE_ERR_INVALID_INPUT
 *   dataSize >= width*height*bpp(format)    -> accepted
 *
 * All five ai image entry points route through validateImageBuffer() (ai.cpp).
 * Each gets a short-dataSize case and a dataSize==0 case, so the guard is
 * proven to fire on that path rather than merely to compile.
 *
 * The storage is always fully allocated (64x64 uint16); only the declared
 * dataSize is short, so a case that reaches the stub stays inside its own
 * allocation. The stubs return XPE_ERR_PROCESSING_FAILED, so "accepted" is
 * asserted as "not XPE_ERR_INVALID_INPUT".
 * ========================================================================= */

namespace {

constexpr uint32_t kGuardW = 64;
constexpr uint32_t kGuardH = 64;
/* 64x64 UINT16 declared; room claimed for 16 pixels only. */
constexpr size_t   kGuardShortBytes = static_cast<size_t>(16) * 2;

/* Fully allocated buffer whose declared dataSize the caller chooses. */
static XpeImageBuffer makeGuardBuffer(std::vector<uint16_t>& storage,
                                      size_t declaredDataSize)
{
    XpeImageBuffer img = makeTestBuffer(kGuardW, kGuardH, storage);
    img.dataSize = declaredDataSize;
    return img;
}

}  // namespace

TEST_F(AiFallbackTest, DataSizeGuard_BodypartRecognize_ShortReturnsInvalid) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeGuardBuffer(storage, kGuardShortBytes);
    char label[64] = {};
    float conf = 0.0f;
    EXPECT_EQ(xpe_bodypart_recognize(&img, label, sizeof(label), &conf),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(AiFallbackTest, DataSizeGuard_StitchImages_ShortReturnsInvalid) {
    std::vector<uint16_t> s1, s2, sOut;
    XpeImageBuffer parts[2]{};
    parts[0] = makeGuardBuffer(s1, kGuardShortBytes);
    parts[1] = makeTestBuffer(kGuardW, kGuardH, s2);
    XpeImageBuffer out = makeTestBuffer(1024, 512, sOut);
    EXPECT_EQ(xpe_stitch_images(parts, 2, &out, nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(AiFallbackTest, DataSizeGuard_StitchEstimateSize_ShortReturnsInvalid) {
    std::vector<uint16_t> s1, s2;
    XpeImageBuffer parts[2]{};
    parts[0] = makeGuardBuffer(s1, kGuardShortBytes);
    parts[1] = makeTestBuffer(kGuardW, kGuardH, s2);
    uint32_t w = 0, h = 0;
    EXPECT_EQ(xpe_stitch_estimate_size(parts, 2, &w, &h), XPE_ERR_INVALID_INPUT);
}

TEST_F(AiFallbackTest, DataSizeGuard_BoneSuppress_ShortReturnsInvalid) {
    std::vector<uint16_t> sIn, sOut;
    XpeImageBuffer img = makeGuardBuffer(sIn, kGuardShortBytes);
    XpeImageBuffer out = makeTestBuffer(kGuardW, kGuardH, sOut);
    EXPECT_EQ(xpe_bone_suppress(&img, &out, nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(AiFallbackTest, DataSizeGuard_DlDenoise_ShortReturnsInvalid) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeGuardBuffer(storage, kGuardShortBytes);
    XpeImageMetadata meta{};
    EXPECT_EQ(xpe_dl_denoise(&img, &meta, nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(AiFallbackTest, DataSizeGuard_BodypartRecognize_ZeroAccepted) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeGuardBuffer(storage, 0);
    char label[64] = {};
    float conf = 0.0f;
    EXPECT_NE(xpe_bodypart_recognize(&img, label, sizeof(label), &conf),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(AiFallbackTest, DataSizeGuard_StitchImages_ZeroAccepted) {
    std::vector<uint16_t> s1, s2, sOut;
    XpeImageBuffer parts[2]{};
    parts[0] = makeGuardBuffer(s1, 0);
    parts[1] = makeGuardBuffer(s2, 0);
    XpeImageBuffer out = makeTestBuffer(1024, 512, sOut);
    EXPECT_NE(xpe_stitch_images(parts, 2, &out, nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(AiFallbackTest, DataSizeGuard_StitchEstimateSize_ZeroAccepted) {
    std::vector<uint16_t> s1, s2;
    XpeImageBuffer parts[2]{};
    parts[0] = makeGuardBuffer(s1, 0);
    parts[1] = makeGuardBuffer(s2, 0);
    uint32_t w = 0, h = 0;
    EXPECT_EQ(xpe_stitch_estimate_size(parts, 2, &w, &h), XPE_OK);
}

TEST_F(AiFallbackTest, DataSizeGuard_BoneSuppress_ZeroAccepted) {
    std::vector<uint16_t> sIn, sOut;
    XpeImageBuffer img = makeGuardBuffer(sIn, 0);
    XpeImageBuffer out = makeTestBuffer(kGuardW, kGuardH, sOut);
    EXPECT_NE(xpe_bone_suppress(&img, &out, nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(AiFallbackTest, DataSizeGuard_DlDenoise_ZeroAccepted) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeGuardBuffer(storage, 0);
    XpeImageMetadata meta{};
    EXPECT_NE(xpe_dl_denoise(&img, &meta, nullptr), XPE_ERR_INVALID_INPUT);
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

/* #105 G3: heap growth over 1000 init -> process -> shutdown cycles.
 *
 * ai is a stub build: the inference entry points return
 * XPE_ERR_PROCESSING_FAILED without doing work, so this loop does NOT measure
 * inference retention -- the ONNX path is unbuilt and unmeasured. What it does
 * cover is the module lifecycle: xpe_ai_init / xpe_ai_shutdown, the config
 * parse, and the per-call validation path, which is where a handle or arena
 * leak would show up. */
TEST(AiEndurance, ThousandCycles_MemoryGrowthUnderOneMB) {
#ifndef _WIN32
    GTEST_SKIP() << "Working-set measurement is Windows-only in this build";
#endif
    std::vector<uint16_t> storage;

    auto one_cycle = [&](int i) {
        ASSERT_EQ(xpe_ai_init("dummy_model_dir", nullptr), XPE_OK) << "cycle " << i;

        XpeImageBuffer img = makeTestBuffer(64, 64, storage);
        char label[64] = {};
        float conf = 0.0f;
        // Stub: PROCESSING_FAILED is the expected return, not a failure.
        EXPECT_EQ(xpe_bodypart_recognize(&img, label, sizeof(label), &conf),
                  XPE_ERR_PROCESSING_FAILED) << "cycle " << i;

        XpeImageMetadata meta{};
        EXPECT_EQ(xpe_dl_denoise(&img, &meta, nullptr),
                  XPE_ERR_PROCESSING_FAILED) << "cycle " << i;

        xpe_ai_shutdown();
    };

    for (int i = 0; i < ENDURANCE_WARMUP; ++i) one_cycle(i);

    const auto before = get_working_set_bytes();
    for (int i = 0; i < ENDURANCE_CYCLES; ++i) one_cycle(i);
    const auto after = get_working_set_bytes();

    if (after > before) {
        EXPECT_LT(after - before, ENDURANCE_ONE_MB)
            << "Working set grew by " << (after - before) / 1024 << " KB over "
            << ENDURANCE_CYCLES << " ai init/process/shutdown cycles";
    }
}

/* ============================================================================
 * #142 (QA-B-41): the empty-image contract.
 *
 * Unlike enhance_basic (QA-B-40), display and dicom (QA-B-41), this module
 * ALREADY held the contract: validateImageBuffer() rejects a zero width or
 * height and a NULL data pointer (ai.cpp). There was no RED phase here, and
 * nothing to fix. These cases exist so that stays true -- the rule is now
 * asserted rather than merely present in the code.
 *
 * Ordering matters and is asserted implicitly: every inference entry point
 * checks initialisation BEFORE the buffer, so these cases run initialised (the
 * fixture does that). Uninitialised, the same input answers NOT_INITIALIZED,
 * which is a different contract and not what is under test here.
 * ============================================================================ */

TEST_F(AiFallbackTest, EmptyImageContract_ZeroWidthIsInvalidInput) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, storage);
    img.width    = 0;
    img.dataSize = 0;   // 0 means unspecified (#123), so it is not what fails

    char label[64] = {};
    float conf = 0.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_bodypart_recognize(&img, label, sizeof(label), &conf));

    XpeImageBuffer out = makeTestBuffer(64, 64, storage);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_bone_suppress(&img, &out, nullptr));

    XpeImageMetadata meta{};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dl_denoise(&img, &meta, nullptr));
}

TEST_F(AiFallbackTest, EmptyImageContract_ZeroHeightIsInvalidInput) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, storage);
    img.height   = 0;
    img.dataSize = 0;

    char label[64] = {};
    float conf = 0.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_bodypart_recognize(&img, label, sizeof(label), &conf));

    XpeImageMetadata meta{};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dl_denoise(&img, &meta, nullptr));
}

TEST_F(AiFallbackTest, EmptyImageContract_NullDataIsInvalidInput) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, storage);
    img.data = nullptr;

    char label[64] = {};
    float conf = 0.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_bodypart_recognize(&img, label, sizeof(label), &conf));

    XpeImageMetadata meta{};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dl_denoise(&img, &meta, nullptr));
}

// The stitch entry points take an ARRAY of images; the contract must reach the
// elements, not just the array pointer.
TEST_F(AiFallbackTest, EmptyImageContract_StitchRejectsEmptyElement) {
    std::vector<uint16_t> a;
    std::vector<uint16_t> b;
    XpeImageBuffer parts[2] = {makeTestBuffer(64, 64, a), makeTestBuffer(64, 64, b)};
    parts[1].width    = 0;
    parts[1].dataSize = 0;

    uint32_t w = 0;
    uint32_t h = 0;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_stitch_estimate_size(parts, 2, &w, &h));

    std::vector<uint16_t> outStorage;
    XpeImageBuffer out = makeTestBuffer(128, 64, outStorage);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_stitch_images(parts, 2, &out, nullptr));
}

// The complement. A well-formed image must not be rejected as invalid input --
// in a stub build the inference functions answer PROCESSING_FAILED, which is
// the documented fallback signal, not a validation failure.
TEST_F(AiFallbackTest, EmptyImageContract_ValidImageStillAccepted) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, storage);

    char label[64] = {};
    float conf = 0.0f;
    EXPECT_NE(XPE_ERR_INVALID_INPUT,
              xpe_bodypart_recognize(&img, label, sizeof(label), &conf));

    XpeImageMetadata meta{};
    EXPECT_NE(XPE_ERR_INVALID_INPUT, xpe_dl_denoise(&img, &meta, nullptr));

    std::vector<uint16_t> a;
    std::vector<uint16_t> b;
    XpeImageBuffer parts[2] = {makeTestBuffer(64, 64, a), makeTestBuffer(64, 64, b)};
    uint32_t w = 0;
    uint32_t h = 0;
    EXPECT_EQ(XPE_OK, xpe_stitch_estimate_size(parts, 2, &w, &h));
    EXPECT_GT(w, 0u);
    EXPECT_GT(h, 0u);
}

/* ============================================================================
 * #142 (QA-B-42): the output-buffer contract.
 *
 * leader's decision: a NULL output pointer or a declared size of 0 is
 * XPE_ERR_INVALID_INPUT -- the argument does not exist. XPE_ERR_BUFFER_TOO_SMALL
 * is reserved for a buffer that is real but short. NULL/0 is judged first.
 *
 * This module disagreed with itself before QA-B-42: xpe_bone_suppress ran its
 * output image through validateImageBuffer and answered INVALID_INPUT, while
 * xpe_stitch_images answered BUFFER_TOO_SMALL for the same shape of fault. A
 * caller had to write two branches for one condition.
 * ============================================================================ */

TEST_F(AiFallbackTest, OutputBufferContract_ZeroLengthLabelBufferIsInvalidInput) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, storage);

    char label[64] = {};
    float conf = 0.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_bodypart_recognize(&img, label, 0, &conf));
}

// A real but short buffer is the other half of the contract. The stub writes
// "UNKNOWN", so 4 bytes cannot hold it -- and silently truncating was the trap
// QA-B-39 documented.
TEST_F(AiFallbackTest, OutputBufferContract_ShortLabelBufferIsBufferTooSmall) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, storage);

    char label[4] = {};
    float conf = 0.0f;
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL,
              xpe_bodypart_recognize(&img, label, sizeof(label), &conf));
}

TEST_F(AiFallbackTest, OutputBufferContract_EmptyStitchOutputIsInvalidInput) {
    std::vector<uint16_t> a;
    std::vector<uint16_t> b;
    XpeImageBuffer parts[2] = {makeTestBuffer(64, 64, a), makeTestBuffer(64, 64, b)};

    std::vector<uint16_t> outStorage;
    XpeImageBuffer out = makeTestBuffer(128, 64, outStorage);
    out.data = nullptr;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_stitch_images(parts, 2, &out, nullptr))
        << "a NULL output data pointer is a missing argument, not a small buffer";

    XpeImageBuffer out2 = makeTestBuffer(128, 64, outStorage);
    out2.dataSize = 0;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_stitch_images(parts, 2, &out2, nullptr))
        << "a declared size of 0 is a missing argument, not a small buffer";
}

TEST_F(AiFallbackTest, OutputBufferContract_ZeroLengthModelCardBufferIsInvalidInput) {
    char buf[64] = {};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ai_get_model_card("bodypart_cnn_v1", buf, 0));
}

// The complement: buffers that are big enough must still work, so the guards
// above cannot be satisfied by rejecting everything.
TEST_F(AiFallbackTest, OutputBufferContract_AdequateBuffersStillAccepted) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, storage);

    char label[64] = {};
    float conf = 0.0f;
    const XpeErrorCode labelRc = xpe_bodypart_recognize(&img, label, sizeof(label), &conf);
    EXPECT_NE(XPE_ERR_INVALID_INPUT, labelRc);
    EXPECT_NE(XPE_ERR_BUFFER_TOO_SMALL, labelRc);

    char card[4096] = {};
    EXPECT_EQ(XPE_OK, xpe_ai_get_model_card("bodypart_cnn_v1", card, sizeof(card)));

    std::vector<uint16_t> a;
    std::vector<uint16_t> b;
    XpeImageBuffer parts[2] = {makeTestBuffer(64, 64, a), makeTestBuffer(64, 64, b)};
    std::vector<uint16_t> outStorage;
    XpeImageBuffer out = makeTestBuffer(128, 64, outStorage);
    const XpeErrorCode stitchRc = xpe_stitch_images(parts, 2, &out, nullptr);
    EXPECT_NE(XPE_ERR_INVALID_INPUT, stitchRc);
    EXPECT_NE(XPE_ERR_BUFFER_TOO_SMALL, stitchRc);
}
