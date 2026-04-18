#include <gtest/gtest.h>
#include "xpe/common/xpe_error.h"

/**
 * Test fixture for safety violation error code
 * AC-LC-005: Safety violation error code exists
 * REQ-ADV-051: Overshoot limiting is mandatory (SAF-100)
 */
class XpeErrorSafetyViolationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * RED Phase: Test that XPE_ERR_SAFETY_VIOLATION error code exists
 *
 * Given: The xpe_common error module is loaded
 * When: Code checks for XPE_ERR_SAFETY_VIOLATION
 * Then: The error code should be defined and equal to -11
 */
TEST_F(XpeErrorSafetyViolationTest, SafetyViolationErrorCodeExists) {
    // Expect XPE_ERR_SAFETY_VIOLATION to be defined
    // This test will fail until we add the error code
    EXPECT_EQ(XPE_ERR_SAFETY_VIOLATION, -11);
}

/**
 * Test that error string mapping exists for safety violation
 */
TEST_F(XpeErrorSafetyViolationTest, SafetyViolationErrorStringMapped) {
    const char* error_str = xpe_error_string(XPE_ERR_SAFETY_VIOLATION);
    ASSERT_NE(error_str, nullptr);
    EXPECT_STRNE(error_str, "");
    EXPECT_STREQ(error_str, "Safety violation");
}
