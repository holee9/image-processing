/**
 * @file test_header_guards.cpp
 * @brief Both public header names must be includable together (#142 D2).
 *
 * Before QA-B-40 the two headers shared the include guard
 * XPE_ENHANCE_ADVANCED_API_H, so a translation unit including both got only
 * the first one's contents. The failure was silent: enhance_advanced_api.h
 * declared just the version accessor, so including it first made every other
 * declaration vanish with no diagnostic -- the build broke only for a consumer
 * that actually called one of them.
 *
 * This file is that consumer. It includes both names, in the order that used
 * to lose declarations, and then takes the address of every exported function.
 * Taking an address requires a visible declaration, so a regression stops the
 * build here rather than in whichever downstream file happened to call first.
 */
#include "xpe/enhance_advanced/enhance_advanced_api.h"
#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"

#include <gtest/gtest.h>

namespace {

// One pointer per exported symbol. The initialisation is what does the work:
// it fails to compile if the declaration is not visible.
const void* const kDeclared[] = {
    reinterpret_cast<const void*>(&xpe_enhance_advanced_init),
    reinterpret_cast<const void*>(&xpe_enhance_advanced_shutdown),
    reinterpret_cast<const void*>(&xpe_enhance_advanced_version),
    reinterpret_cast<const void*>(&xpe_multiscale_process),
    reinterpret_cast<const void*>(&xpe_fractional_process),
    reinterpret_cast<const void*>(&xpe_detect_collimation),
    reinterpret_cast<const void*>(&xpe_adv_calc_exposure_index),
};

}  // namespace

// The compile is the assertion; this case reports it as a result and confirms
// the addresses are real rather than optimised away.
TEST(EnhanceAdvancedHeaderGuardTest, BothHeaderNamesExposeTheFullApi) {
    ASSERT_EQ(7u, sizeof(kDeclared) / sizeof(kDeclared[0]));
    for (const void* fn : kDeclared) {
        EXPECT_NE(nullptr, fn);
    }
}

// The version accessor is the one symbol the compatibility header used to
// declare on its own, so it is the one whose behaviour must be unchanged by
// the forwarding rewrite.
TEST(EnhanceAdvancedHeaderGuardTest, VersionStillReachableUnderCompatName) {
    const char* v = xpe_enhance_advanced_version();
    ASSERT_NE(nullptr, v);
    EXPECT_STREQ("1.0.0", v);
}
