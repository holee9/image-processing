#include <gtest/gtest.h>
#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"
#include "xpe/common/xpe_error.h"

/**
 * Test fixture for enhance_advanced lifecycle
 * AC-LC-001: Initialization with default config
 * AC-LC-002: Processing before init
 * AC-LC-003: Shutdown after init
 * REQ-ADV-001: Module initialization
 * REQ-ADV-020: Not-initialized guard
 */
class EnhanceAdvancedLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure clean state before each test
        xpe_enhance_advanced_shutdown();
    }

    void TearDown() override {
        // Clean up after each test
        xpe_enhance_advanced_shutdown();
    }
};

/**
 * RED Phase: Test initialization with default config (NULL)
 *
 * Given: The module is not initialized
 * When: xpe_enhance_advanced_init(NULL) is called
 * Then: The function should return XPE_OK
 * And: The module enters initialized state
 */
TEST_F(EnhanceAdvancedLifecycleTest, InitWithDefaultConfig) {
    // RED: This test will fail until we implement init
    XpeErrorCode result = xpe_enhance_advanced_init(NULL);
    EXPECT_EQ(result, XPE_OK);
}

/**
 * Test initialization with custom config JSON
 */
TEST_F(EnhanceAdvancedLifecycleTest, InitWithCustomConfig) {
    const char* config = "{\"log_level\":\"debug\"}";
    XpeErrorCode result = xpe_enhance_advanced_init(config);
    EXPECT_EQ(result, XPE_OK);
}

/**
 * Test initialization with empty config string (should fail)
 */
TEST_F(EnhanceAdvancedLifecycleTest, InitWithEmptyConfigString) {
    const char* emptyConfig = "";
    XpeErrorCode result = xpe_enhance_advanced_init(emptyConfig);
    EXPECT_EQ(result, XPE_ERR_CONFIG_INVALID);
}

/**
 * RED Phase: Test shutdown after initialization
 *
 * Given: The module is initialized
 * When: xpe_enhance_advanced_shutdown() is called
 * Then: All resources should be released
 * And: Subsequent calls should return NOT_INITIALIZED
 */
TEST_F(EnhanceAdvancedLifecycleTest, ShutdownAfterInit) {
    // First initialize
    ASSERT_EQ(xpe_enhance_advanced_init(NULL), XPE_OK);

    // Shutdown
    xpe_enhance_advanced_shutdown();

    // Verify not initialized state by trying to process
    XpeImageBuffer img = {};
    XpeImageMetadata meta = {};
    XpeErrorCode result = xpe_multiscale_process(&img, &meta, NULL);
    EXPECT_EQ(result, XPE_ERR_NOT_INITIALIZED);
}

/**
 * Test version function
 */
TEST_F(EnhanceAdvancedLifecycleTest, VersionReturnsValidString) {
    const char* version = xpe_enhance_advanced_version();
    ASSERT_NE(version, nullptr);
    EXPECT_STRNE(version, "");
}

/**
 * RED Phase: Test processing before initialization (AC-LC-002)
 *
 * Given: The module is NOT initialized
 * When: Any processing function is called
 * Then: The function should return XPE_ERR_NOT_INITIALIZED
 */
TEST_F(EnhanceAdvancedLifecycleTest, ProcessingBeforeInit) {
    XpeImageBuffer img = {};
    XpeImageMetadata meta = {};
    int32_t x0, y0, x1, y1;
    float ei, di;

    // All processing functions should return NOT_INITIALIZED
    EXPECT_EQ(xpe_multiscale_process(&img, &meta, NULL), XPE_ERR_NOT_INITIALIZED);
    EXPECT_EQ(xpe_fractional_process(&img, 1.0f, NULL), XPE_ERR_NOT_INITIALIZED);
    EXPECT_EQ(xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, NULL), XPE_ERR_NOT_INITIALIZED);
    EXPECT_EQ(xpe_calc_exposure_index(&img, &meta, &ei, &di), XPE_ERR_NOT_INITIALIZED);
}

/**
 * Test double initialization (idempotent)
 */
TEST_F(EnhanceAdvancedLifecycleTest, DoubleInitialization) {
    ASSERT_EQ(xpe_enhance_advanced_init(NULL), XPE_OK);
    // Second init should succeed (idempotent)
    EXPECT_EQ(xpe_enhance_advanced_init(NULL), XPE_OK);
}

/**
 * Test shutdown without initialization (safe no-op)
 */
TEST_F(EnhanceAdvancedLifecycleTest, ShutdownWithoutInit) {
    // Should not crash
    xpe_enhance_advanced_shutdown();
    SUCCEED();
}
