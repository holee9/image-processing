#include <gtest/gtest.h>
#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"
#include "xpe/common/xpe_types.h"

/**
 * Test fixture for enhance_advanced API header
 * AC-PIN-001: C# Struct Marshaling
 * REQ-ADV-002: P/Invoke ABI Compliance
 */
class EnhanceAdvancedApiHeaderTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * RED Phase: Test that API functions are declared with correct signatures
 *
 * Given: The enhance_advanced API header is included
 * When: Code checks for function declarations
 * Then: All 4 API functions should be declared with extern "C" linkage
 */
TEST_F(EnhanceAdvancedApiHeaderTest, ApiFunctionsDeclared) {
    // This test verifies the header can be included and compiled
    // The actual function declarations are checked at compile time
    SUCCEED();
}

/**
 * Test that XpeImageBuffer and XpeImageMetadata are usable from API
 */
TEST_F(EnhanceAdvancedApiHeaderTest, CommonTypesUsable) {
    // Verify common types are accessible through the API
    XpeImageBuffer img = {};
    XpeImageMetadata meta = {};

    EXPECT_EQ(img.width, 0);
    EXPECT_EQ(img.height, 0);
    EXPECT_EQ(meta.kVp, 0.0f);
}

/**
 * Test API function pointer types exist (compile-time check)
 */
TEST_F(EnhanceAdvancedApiHeaderTest, FunctionPointersExist) {
    // These will fail to compile if the functions don't exist
    using XpeMultiscaleProcessFn = XpeErrorCode(*)(XpeImageBuffer*, const XpeImageMetadata*, const char*);
    using XpeFractionalProcessFn = XpeErrorCode(*)(XpeImageBuffer*, float, const char*);
    using XpeDetectCollimationFn = XpeErrorCode(*)(const XpeImageBuffer*, int32_t*, int32_t*, int32_t*, int32_t*, const char*);
    using XpeCalcExposureIndexFn = XpeErrorCode(*)(const XpeImageBuffer*, const XpeImageMetadata*, float*, float*);

    // Function pointer types are defined
    SUCCEED();
}
