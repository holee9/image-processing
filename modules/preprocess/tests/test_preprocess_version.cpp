/**
 * @file test_preprocess_version.cpp
 * @brief xpe_preprocess_version contract (QA-A-199, #216; REQ-P1A-106 draft in QA-A-198).
 *
 * The contract: a pointer to a NUL-terminated string that is never NULL, stays valid for the
 * lifetime of the process, and does not depend on whether the module is initialized. The value
 * itself is deliberately not pinned ("0.1.0" is a source constant that differs from the CMake
 * project version); only its shape is, so a later version bump does not turn these red.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"

#include <cstring>
#include <string>

namespace {

/** "digits.digits.digits", nothing else. */
bool isDottedTriple(const std::string& s) {
    int dots = 0;
    bool digitSeen = false;
    for (char c : s) {
        if (c >= '0' && c <= '9') { digitSeen = true; continue; }
        if (c == '.' && digitSeen) { ++dots; digitSeen = false; continue; }
        return false;
    }
    return dots == 2 && digitSeen;
}

class PreprocessVersion : public ::testing::Test {
protected:
    // Start every test uninitialized; an earlier test in the binary may have left the module up.
    void SetUp() override { xpe_preprocess_shutdown(); }
    void TearDown() override { xpe_preprocess_shutdown(); }
};

} // namespace

TEST_F(PreprocessVersion, IsNeverNullAndNonEmpty) {
    const char* v = xpe_preprocess_version();
    ASSERT_NE(nullptr, v);
    EXPECT_GT(std::strlen(v), 0u);
}

TEST_F(PreprocessVersion, IsStaticSameAddressAndSameContentOnEveryCall) {
    const char* first = xpe_preprocess_version();
    ASSERT_NE(nullptr, first);
    const std::string content = first;
    for (int i = 0; i < 100; ++i) {
        const char* again = xpe_preprocess_version();
        EXPECT_EQ(first, again) << "call " << i << ": the pointer must be the same static storage";
        ASSERT_NE(nullptr, again);
        EXPECT_EQ(content, again) << "call " << i;
    }
}

TEST_F(PreprocessVersion, DoesNotDependOnTheInitializationState) {
    ASSERT_FALSE(xpe_preprocess_is_initialized()) << "precondition: the module starts uninitialized";
    const char* before = xpe_preprocess_version();
    ASSERT_NE(nullptr, before);
    const std::string expected = before;

    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    ASSERT_TRUE(xpe_preprocess_is_initialized()) << "precondition: the module is now initialized";
    const char* during = xpe_preprocess_version();
    ASSERT_NE(nullptr, during);
    EXPECT_EQ(before, during) << "same address before and after init";
    EXPECT_EQ(expected, during) << "same content before and after init";

    xpe_preprocess_shutdown();
    ASSERT_FALSE(xpe_preprocess_is_initialized()) << "precondition: the module is uninitialized again";
    const char* after = xpe_preprocess_version();
    ASSERT_NE(nullptr, after);
    EXPECT_EQ(before, after) << "same address after shutdown";
    EXPECT_EQ(expected, after) << "same content after shutdown";
}

TEST_F(PreprocessVersion, HasTheShapeDigitsDotDigitsDotDigits) {
    const char* v = xpe_preprocess_version();
    ASSERT_NE(nullptr, v);
    EXPECT_TRUE(isDottedTriple(v)) << "version string: \"" << v << "\"";
    // Control: the shape check refuses the near misses it exists to refuse.
    EXPECT_FALSE(isDottedTriple("v0.1.0"));
    EXPECT_FALSE(isDottedTriple("0.1"));
    EXPECT_FALSE(isDottedTriple("0.1.0.4"));
    EXPECT_FALSE(isDottedTriple("0..1"));
    EXPECT_FALSE(isDottedTriple(""));
    EXPECT_TRUE(isDottedTriple("12.0.345"));
}
