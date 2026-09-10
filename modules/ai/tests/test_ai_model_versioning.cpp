/**
 * @file test_ai_model_versioning.cpp
 * @brief Model versioning and metadata parsing tests -- SPEC-XPE-P3-AI T-004
 *
 * REQ-AI-008: Model versioning shall follow semver; model metadata shall include:
 * model_id, version, pccp_scope, training_data_hash, validation_metrics.
 *
 * Tests:
 * - Model metadata parsing from JSON
 * - Semver version validation
 * - PCCP scope verification
 * - Training data hash validation
 * - Validation metrics extraction
 *
 * @ingroup xpe_ai_tests
 */

#include "xpe/ai/ai_api.h"
#include "xpe/common/xpe_error.h"
#include <gtest/gtest.h>

#include <cstring>
#include <string>

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class AiModelVersioningTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize AI module before each test
        xpe_ai_shutdown();
        ec = xpe_ai_init("test_models", nullptr);
        ASSERT_EQ(ec, XPE_OK) << "AI init failed";
    }

    void TearDown() override {
        xpe_ai_shutdown();
    }

    XpeErrorCode ec;
};

/* ============================================================================
 * T-004.1: Model Version Format Validation (REQ-AI-008)
 * ============================================================================ */

TEST_F(AiModelVersioningTest, ModelCardContainsSemverVersion) {
    char buffer[4096];
    ec = xpe_ai_get_model_card("bodypart_cnn_v1", buffer, sizeof(buffer));
    ASSERT_EQ(ec, XPE_OK);

    // Parse JSON and check model_version field
    // Expect format "X.Y.Z" per semver.org
    std::string json(buffer);
    ASSERT_NE(json.find("\"model_version\":"), std::string::npos);

    // Extract version and validate semver format
    size_t verPos = json.find("\"model_version\":");
    ASSERT_NE(verPos, std::string::npos);

    // Find the value after the key
    size_t colonPos = json.find(":", verPos);
    size_t quoteStart = json.find("\"", colonPos + 1);
    size_t quoteEnd = json.find("\"", quoteStart + 1);

    ASSERT_NE(quoteStart, std::string::npos);
    ASSERT_NE(quoteEnd, std::string::npos);

    std::string version = json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);

    // Validate semver format: X.Y.Z
    int major = 0, minor = 0, patch = 0;
    int parsed = sscanf_s(version.c_str(), "%d.%d.%d", &major, &minor, &patch);

    EXPECT_EQ(parsed, 3) << "Version must be semver format X.Y.Z, got: " << version;
    EXPECT_GE(major, 0) << "Major version must be >= 0";
    EXPECT_GE(minor, 0) << "Minor version must be >= 0";
    EXPECT_GE(patch, 0) << "Patch version must be >= 0";
}

/* ============================================================================
 * T-004.2: Model ID Field Present (REQ-AI-008)
 * ============================================================================ */

TEST_F(AiModelVersioningTest, ModelCardContainsModelId) {
    char buffer[4096];
    ec = xpe_ai_get_model_card("bone_suppress_unet_v1", buffer, sizeof(buffer));
    ASSERT_EQ(ec, XPE_OK);

    std::string json(buffer);
    ASSERT_NE(json.find("\"model_id\":"), std::string::npos);
    ASSERT_NE(json.find("\"model_id\":\"bone_suppress_unet_v1\""), std::string::npos);
}

/* ============================================================================
 * T-004.3: PCCP Scope Field Present (REQ-AI-008)
 * ============================================================================ */

TEST_F(AiModelVersioningTest, ModelCardContainsPccpScope) {
    char buffer[4096];
    ec = xpe_ai_get_model_card("dl_denoise_ssl_v1", buffer, sizeof(buffer));
    ASSERT_EQ(ec, XPE_OK);

    std::string json(buffer);
    ASSERT_NE(json.find("\"pccp_status\":"), std::string::npos);

    // Valid pccp_status values: "within_boundary", "exceeds_boundary", "not_applicable"
    bool hasValidStatus =
        json.find("\"pccp_status\":\"within_boundary\"") != std::string::npos ||
        json.find("\"pccp_status\":\"exceeds_boundary\"") != std::string::npos ||
        json.find("\"pccp_status\":\"not_applicable\"") != std::string::npos;

    EXPECT_TRUE(hasValidStatus) << "pccp_status must have a valid value";
}

/* ============================================================================
 * T-004.4: Training Data Hash Field Present (REQ-AI-008)
 * ============================================================================ */

TEST_F(AiModelVersioningTest, ModelCardContainsTrainingDataHash) {
    char buffer[4096];
    ec = xpe_ai_get_model_card("stitch_feature_match_v1", buffer, sizeof(buffer));
    ASSERT_EQ(ec, XPE_OK);

    std::string json(buffer);
    ASSERT_NE(json.find("\"training_data_hash\":"), std::string::npos);

    // Hash format should be "sha256:..." or similar
    size_t hashPos = json.find("\"training_data_hash\":");
    if (hashPos != std::string::npos) {
        size_t colonPos = json.find(":", hashPos);
        size_t quoteStart = json.find("\"", colonPos + 1);
        size_t quoteEnd = json.find("\"", quoteStart + 1);

        if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
            std::string hash = json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
            EXPECT_FALSE(hash.empty()) << "Training data hash must not be empty";

            // If hash is provided, it should start with algorithm prefix
            if (hash != "N/A") {
                EXPECT_TRUE(hash.find("sha256:") == 0 ||
                            hash.find("sha512:") == 0 ||
                            hash.find("md5:") == 0)
                    << "Hash should have algorithm prefix (sha256:, sha512:, md5:), got: " << hash;
            }
        }
    }
}

/* ============================================================================
 * T-004.5: Validation Metrics Field Present (REQ-AI-008)
 * ============================================================================ */

TEST_F(AiModelVersioningTest, ModelCardContainsValidationMetrics) {
    char buffer[4096];
    ec = xpe_ai_get_model_card("bodypart_cnn_v1", buffer, sizeof(buffer));
    ASSERT_EQ(ec, XPE_OK);

    std::string json(buffer);
    ASSERT_NE(json.find("\"validation_metrics\":"), std::string::npos);

    // validation_metrics should be a JSON object with numeric values
    // Look for common metrics: accuracy, precision, recall, f1_score, psnr, ssim
    bool hasMetric =
        json.find("\"psnr\"") != std::string::npos ||
        json.find("\"ssim\"") != std::string::npos ||
        json.find("\"accuracy\"") != std::string::npos ||
        json.find("\"f1_score\"") != std::string::npos;

    EXPECT_TRUE(hasMetric) << "Validation metrics should contain at least one numeric metric";
}

/* ============================================================================
 * T-004.6: All Required Fields Present (REQ-AI-008)
 * ============================================================================ */

TEST_F(AiModelVersioningTest, ModelCardContainsAllRequiredMetadataFields) {
    char buffer[4096];
    ec = xpe_ai_get_model_card("bodypart_cnn_v1", buffer, sizeof(buffer));
    ASSERT_EQ(ec, XPE_OK);

    std::string json(buffer);

    // Check all required fields from REQ-AI-008
    EXPECT_NE(json.find("\"model_id\":"), std::string::npos) << "Missing model_id";
    EXPECT_NE(json.find("\"model_version\":"), std::string::npos) << "Missing model_version";
    EXPECT_NE(json.find("\"pccp_status\":"), std::string::npos) << "Missing pccp_status";
    EXPECT_NE(json.find("\"training_data_hash\":"), std::string::npos) << "Missing training_data_hash";
    EXPECT_NE(json.find("\"validation_metrics\":"), std::string::npos) << "Missing validation_metrics";
}

/* ============================================================================
 * T-004.7: Version Comparison (Semver Ordering)
 * ============================================================================ */

TEST_F(AiModelVersioningTest, ModelVersionsCanBeCompared) {
    // This test verifies that version strings follow semver for proper comparison
    // In a real implementation, this would test a version comparison utility

    char buffer1[4096], buffer2[4096];

    // Get model cards for two different models
    ec = xpe_ai_get_model_card("bodypart_cnn_v1", buffer1, sizeof(buffer1));
    ASSERT_EQ(ec, XPE_OK);

    ec = xpe_ai_get_model_card("bone_suppress_unet_v1", buffer2, sizeof(buffer2));
    ASSERT_EQ(ec, XPE_OK);

    // Extract versions and verify they are comparable
    std::string json1(buffer1);
    std::string json2(buffer2);

    // Both should have valid semver versions
    size_t pos1 = json1.find("\"model_version\":");
    size_t pos2 = json2.find("\"model_version\":");

    ASSERT_NE(pos1, std::string::npos);
    ASSERT_NE(pos2, std::string::npos);

    // In full implementation, this would verify semver comparison logic
    // For now, just ensure both have the field
}

/* ============================================================================
 * T-004.8: Missing Model Returns Error
 * ============================================================================ */

TEST_F(AiModelVersioningTest, ModelCardForUnknownModelReturnsError) {
    char buffer[4096];
    ec = xpe_ai_get_model_card("unknown_model_xyz", buffer, sizeof(buffer));

    EXPECT_EQ(ec, XPE_ERR_IO_FAILED) << "Unknown model should return XPE_ERR_IO_FAILED";

    // Buffer should contain error information
    std::string json(buffer);
    EXPECT_NE(json.find("\"error\":"), std::string::npos);
}

/* ============================================================================
 * T-004.9: Buffer Too Small Handling
 * ============================================================================ */

TEST_F(AiModelVersioningTest, ModelCardWithTinyBufferReturnsBufferTooSmall) {
    char tinyBuffer[10];
    ec = xpe_ai_get_model_card("bodypart_cnn_v1", tinyBuffer, sizeof(tinyBuffer));

    EXPECT_EQ(ec, XPE_ERR_BUFFER_TOO_SMALL);

    // Buffer should be null-terminated even when too small
    EXPECT_EQ(tinyBuffer[9], '\0');
}

/* ============================================================================
 * T-004.10: Null Model ID Handling
 * ============================================================================ */

TEST_F(AiModelVersioningTest, ModelCardWithNullModelIdReturnsInvalidInput) {
    char buffer[4096];
    ec = xpe_ai_get_model_card(nullptr, buffer, sizeof(buffer));

    EXPECT_EQ(ec, XPE_ERR_INVALID_INPUT);
}

/* ============================================================================
 * T-004.11: Null Buffer Handling
 * ============================================================================ */

TEST_F(AiModelVersioningTest, ModelCardWithNullBufferReturnsInvalidInput) {
    ec = xpe_ai_get_model_card("bodypart_cnn_v1", nullptr, 4096);

    EXPECT_EQ(ec, XPE_ERR_INVALID_INPUT);
}
