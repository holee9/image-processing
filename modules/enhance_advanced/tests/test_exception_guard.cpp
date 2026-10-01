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

#include <algorithm>
#include <mutex>
#include <utility>
#include <cstdint>
#include <vector>

#include <chrono>
#include <future>
#include <new>
#include <stdexcept>

#include "xpe/enhance_advanced/internal.h"
#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"
#include "../src/detail/parallel_rows.h"

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

// ---- QA-B-181c (Codex #37): the row-band split of ForRows ---------------------------------------------------------

TEST(AdvExceptionGuard, ForRowsPartitionsRowsNearIntMaxWithoutOverflow) {
    // The bodies only record their band, so INT32_MAX rows cost nothing. `(rows + threads - 1) / threads` left int
    // here and produced a negative band size.
    for (int threads : {2, 3, 4, 7, 64}) {
        std::mutex m;
        std::vector<std::pair<int, int>> bands;
        xpe_parallel::ForRows(0x7FFFFFFF, threads, [&](int y0, int y1) {
            std::lock_guard<std::mutex> g(m);
            bands.emplace_back(y0, y1);
        });
        std::sort(bands.begin(), bands.end());
        int64_t next = 0;
        for (const auto& b : bands) {
            EXPECT_EQ(next, b.first) << "threads=" << threads << ": bands must be contiguous";
            EXPECT_LT(b.first, b.second) << "threads=" << threads << ": no empty band";
            next = b.second;
        }
        EXPECT_EQ(0x7FFFFFFF, next) << "threads=" << threads << ": the bands must cover every row";
        EXPECT_FALSE(bands.empty()) << "control: the body ran";
    }
}

TEST(AdvExceptionGuard, ForRowsNeverPlansMoreBandsThanRows) {
    // A caller-supplied thread count is not trusted: 100000 threads over 10 rows is 10 bands, not 100000 slots.
    std::mutex m;
    std::vector<std::pair<int, int>> bands;
    xpe_parallel::ForRows(10, 100000, [&](int y0, int y1) {
        std::lock_guard<std::mutex> g(m);
        bands.emplace_back(y0, y1);
    });
    std::sort(bands.begin(), bands.end());
    int next = 0;
    for (const auto& b : bands) {
        EXPECT_EQ(next, b.first);
        EXPECT_LT(b.first, b.second);
        next = b.second;
    }
    EXPECT_EQ(10, next);
    EXPECT_LE(bands.size(), 10u);
}
