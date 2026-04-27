/**
 * @file test_ai_model_card.cpp
 * @brief Model Card API tests for xpe_ai.dll -- SPEC-XPE-P3-AI
 *
 * Validates the Model Card transparency API:
 * - JSON schema validation for loaded models
 * - Stub model card format verification
 * - Buffer handling (null, too small, exact size)
 * - Unknown model returns IO_FAILED
 *
 * REQ-AI-010: Model Card transparency API.
 * REQ-AI-011: JSON schema conformance.
 * REQ-AI-008: Model metadata.
 *
 * @ingroup xpe_ai_tests
 */

#include "xpe/ai/ai_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include <gtest/gtest.h>

#include <cstring>
#include <string>

/* ============================================================================
 * Test Fixture: initialize AI module before each test
 * ============================================================================ */

class AiModelCardTest : public ::testing::Test {
protected:
    void SetUp() override {
        xpe_ai_shutdown();
        ASSERT_EQ(xpe_ai_init("dummy_model_dir", nullptr), XPE_OK);
    }

    void TearDown() override {
        xpe_ai_shutdown();
    }

    /**
     * @brief Extract a JSON string value by key from a simple JSON string.
     *
     * Minimal JSON parser for testing -- finds "key":"value" patterns.
     */
    static std::string extractJsonString(const char* json, const char* key) {
        std::string search = std::string("\"") + key + "\"";
        const char* pos = std::strstr(json, search.c_str());
        if (!pos) return "";

        // Skip to the colon
        const char* colon = std::strchr(pos + search.size(), ':');
        if (!colon) return "";

        // Skip whitespace and opening quote
        const char* p = colon + 1;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
        if (*p != '"') return "";
        ++p;

        // Read until closing quote
        std::string result;
        while (*p && *p != '"') {
            if (*p == '\\' && *(p + 1)) {
                result += *(p + 1);
                p += 2;
            } else {
                result += *p;
                ++p;
            }
        }
        return result;
    }

    /**
     * @brief Check if a JSON string contains a specific key.
     */
    static bool jsonContainsKey(const char* json, const char* key) {
        std::string search = std::string("\"") + key + "\"";
        return std::strstr(json, search.c_str()) != nullptr;
    }
};

/* ============================================================================
 * Model Card for Known Models
 * ============================================================================ */

TEST_F(AiModelCardTest, GetModelCardForKnownModelReturnsOk) {
    char buf[4096] = {};
    XpeErrorCode ec = xpe_ai_get_model_card("bodypart_cnn_v1", buf, sizeof(buf));
    EXPECT_EQ(ec, XPE_OK);
}

TEST_F(AiModelCardTest, GetModelCardContainsModelId) {
    char buf[4096] = {};
    xpe_ai_get_model_card("bodypart_cnn_v1", buf, sizeof(buf));

    std::string modelId = extractJsonString(buf, "model_id");
    EXPECT_EQ(modelId, "bodypart_cnn_v1");
}

TEST_F(AiModelCardTest, GetModelCardContainsModelVersion) {
    char buf[4096] = {};
    xpe_ai_get_model_card("bodypart_cnn_v1", buf, sizeof(buf));

    std::string version = extractJsonString(buf, "model_version");
    EXPECT_FALSE(version.empty());
    // Stub version should contain "stub"
    EXPECT_NE(version.find("stub"), std::string::npos)
        << "Stub model card version should indicate stub build";
}

TEST_F(AiModelCardTest, GetModelCardContainsIntendedUse) {
    char buf[4096] = {};
    xpe_ai_get_model_card("bodypart_cnn_v1", buf, sizeof(buf));

    EXPECT_TRUE(jsonContainsKey(buf, "intended_use"));
}

TEST_F(AiModelCardTest, GetModelCardContainsLimitations) {
    char buf[4096] = {};
    xpe_ai_get_model_card("bodypart_cnn_v1", buf, sizeof(buf));

    EXPECT_TRUE(jsonContainsKey(buf, "limitations"));
}

TEST_F(AiModelCardTest, GetModelCardContainsPccpStatus) {
    char buf[4096] = {};
    xpe_ai_get_model_card("bodypart_cnn_v1", buf, sizeof(buf));

    EXPECT_TRUE(jsonContainsKey(buf, "pccp_status"));
}

TEST_F(AiModelCardTest, GetModelCardContainsPublishedDate) {
    char buf[4096] = {};
    xpe_ai_get_model_card("bodypart_cnn_v1", buf, sizeof(buf));

    EXPECT_TRUE(jsonContainsKey(buf, "published_date"));
}

TEST_F(AiModelCardTest, GetModelCardContainsTrainingDataSummary) {
    char buf[4096] = {};
    xpe_ai_get_model_card("bodypart_cnn_v1", buf, sizeof(buf));

    EXPECT_TRUE(jsonContainsKey(buf, "training_data_summary"));
}

TEST_F(AiModelCardTest, GetModelCardOutputIsValidJson) {
    char buf[4096] = {};
    xpe_ai_get_model_card("bodypart_cnn_v1", buf, sizeof(buf));

    // Verify it starts with '{' and ends with '}'
    size_t len = std::strlen(buf);
    ASSERT_GT(len, 0u);
    EXPECT_EQ(buf[0], '{');
    EXPECT_EQ(buf[len - 1], '}');
}

/* ============================================================================
 * Model Card for Different Known Models
 * ============================================================================ */

TEST_F(AiModelCardTest, GetModelCardForStitchModel) {
    char buf[4096] = {};
    EXPECT_EQ(xpe_ai_get_model_card("stitch_feature_match_v1", buf, sizeof(buf)),
              XPE_OK);
    EXPECT_TRUE(jsonContainsKey(buf, "model_id"));
}

TEST_F(AiModelCardTest, GetModelCardForBoneSuppressModel) {
    char buf[4096] = {};
    EXPECT_EQ(xpe_ai_get_model_card("bone_suppress_unet_v1", buf, sizeof(buf)),
              XPE_OK);
    EXPECT_TRUE(jsonContainsKey(buf, "model_id"));
}

TEST_F(AiModelCardTest, GetModelCardForDenoiseModel) {
    char buf[4096] = {};
    EXPECT_EQ(xpe_ai_get_model_card("dl_denoise_ssl_v1", buf, sizeof(buf)),
              XPE_OK);
    EXPECT_TRUE(jsonContainsKey(buf, "model_id"));
}

/* ============================================================================
 * Model Card for Unknown Models
 * ============================================================================ */

TEST_F(AiModelCardTest, GetModelCardForUnknownModelReturnsIoFailed) {
    char buf[4096] = {};
    XpeErrorCode ec = xpe_ai_get_model_card("nonexistent_model_v99",
                                             buf, sizeof(buf));
    EXPECT_EQ(ec, XPE_ERR_IO_FAILED);
}

TEST_F(AiModelCardTest, GetModelCardForUnknownModelStillWritesJson) {
    // Even for unknown models, the function should write some JSON
    char buf[4096] = {};
    xpe_ai_get_model_card("nonexistent_model_v99", buf, sizeof(buf));

    size_t len = std::strlen(buf);
    ASSERT_GT(len, 0u);
    EXPECT_EQ(buf[0], '{');

    // Should contain the model ID we asked for
    std::string modelId = extractJsonString(buf, "model_id");
    EXPECT_EQ(modelId, "nonexistent_model_v99");
}

TEST_F(AiModelCardTest, GetModelCardForUnknownModelContainsError) {
    char buf[4096] = {};
    xpe_ai_get_model_card("nonexistent_model_v99", buf, sizeof(buf));

    EXPECT_TRUE(jsonContainsKey(buf, "error"));
}

/* ============================================================================
 * Buffer Handling Tests
 * ============================================================================ */

TEST_F(AiModelCardTest, GetModelCardNullModelIdReturnsInvalid) {
    char buf[4096] = {};
    EXPECT_EQ(xpe_ai_get_model_card(nullptr, buf, sizeof(buf)),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(AiModelCardTest, GetModelCardNullBufReturnsInvalid) {
    EXPECT_EQ(xpe_ai_get_model_card("bodypart_cnn_v1", nullptr, 4096),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(AiModelCardTest, GetModelCardZeroBufSizeReturnsTooSmall) {
    char buf[1] = {};
    EXPECT_EQ(xpe_ai_get_model_card("bodypart_cnn_v1", buf, 0),
              XPE_ERR_BUFFER_TOO_SMALL);
}

TEST_F(AiModelCardTest, GetModelCardSmallBufferReturnsTooSmall) {
    // Buffer of 1 byte -- guaranteed too small for any JSON
    char buf[1] = {};
    XpeErrorCode ec = xpe_ai_get_model_card("bodypart_cnn_v1", buf, sizeof(buf));
    EXPECT_EQ(ec, XPE_ERR_BUFFER_TOO_SMALL);
}

TEST_F(AiModelCardTest, GetModelCardExactSizeBufferSucceeds) {
    // First call to determine the actual size
    char probeBuf[8192] = {};
    xpe_ai_get_model_card("bodypart_cnn_v1", probeBuf, sizeof(probeBuf));
    size_t exactSize = std::strlen(probeBuf) + 1; // +1 for null terminator

    // Second call with exact size
    std::vector<char> exactBuf(exactSize);
    XpeErrorCode ec = xpe_ai_get_model_card("bodypart_cnn_v1",
                                             exactBuf.data(),
                                             static_cast<size_t>(exactBuf.size()));
    EXPECT_EQ(ec, XPE_OK);
}
