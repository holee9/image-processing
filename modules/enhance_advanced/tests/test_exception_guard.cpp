/**
 * @file test_exception_guard.cpp
 * @brief xpe_enhance_advanced_init cannot throw out of the C ABI (QA-B-181, QA-B-179, #233).
 *
 * QA-B-179 measured that a std::bad_alloc inside xpe_enhance_advanced_init escaped it: the function's own try
 * caught only nlohmann::json::exception. (Its lock was released -- the function has a try region of its own --
 * but the exception still crossed the C ABI.) The init is now guarded by XpeAdvGuardedCall (internal.h).
 *
 * What is and is not covered here: the guard is header-inline, so the first group runs it directly with every
 * kind of exception. An allocation failure INSIDE xpe_enhance_advanced_init cannot be injected from a test (the
 * module is a DLL with its own allocator, and the failing allocation is in the JSON parser), so the second group
 * checks that the wired-up init still behaves for every ordinary input and that its lock is free after each call.
 */
#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <new>
#include <stdexcept>

#include "xpe/enhance_advanced/internal.h"
#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"

TEST(AdvExceptionGuard, BadAllocBecomesOutOfMemory) {
    EXPECT_EQ(XPE_ERR_OUT_OF_MEMORY, XpeAdvGuardedCall([]() -> XpeErrorCode { throw std::bad_alloc(); }));
}

TEST(AdvExceptionGuard, AnyOtherExceptionBecomesProcessingFailed) {
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, XpeAdvGuardedCall([]() -> XpeErrorCode { throw std::length_error("x"); }));
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, XpeAdvGuardedCall([]() -> XpeErrorCode { throw 42; }));
}

TEST(AdvExceptionGuard, ANormalReturnPassesThroughUnchanged) {
    EXPECT_EQ(XPE_OK, XpeAdvGuardedCall([]() -> XpeErrorCode { return XPE_OK; }));
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, XpeAdvGuardedCall([]() -> XpeErrorCode { return XPE_ERR_CONFIG_INVALID; }));
}

namespace {
// The init takes g_initMutex; a call from another thread returns only if the mutex is free.
bool InitReturnsWithin(const char* cfg, XpeErrorCode* rc) {
    auto f = std::async(std::launch::async, [cfg] { return xpe_enhance_advanced_init(cfg); });
    if (f.wait_for(std::chrono::seconds(5)) != std::future_status::ready) return false;
    *rc = f.get();
    return true;
}
}  // namespace

TEST(AdvExceptionGuard, InitStillMapsOrdinaryInputsAndLeavesItsLockFree) {
    xpe_enhance_advanced_shutdown();
    XpeErrorCode rc = XPE_OK;
    ASSERT_TRUE(InitReturnsWithin(nullptr, &rc));
    EXPECT_EQ(XPE_OK, rc);
    ASSERT_TRUE(InitReturnsWithin("", &rc));
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, rc) << "an empty string is invalid";
    ASSERT_TRUE(InitReturnsWithin("{not json", &rc));
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, rc) << "malformed JSON";
    ASSERT_TRUE(InitReturnsWithin("{\"mfp\": {}}", &rc));
    EXPECT_EQ(XPE_OK, rc);
    xpe_enhance_advanced_shutdown();
}
