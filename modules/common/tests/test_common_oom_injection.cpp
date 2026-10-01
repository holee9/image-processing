/**
 * @file test_common_oom_injection.cpp
 * @brief Allocation-failure sweeps over the xpe_common entry points that keep state (QA-A-204, #233).
 *
 * xpe_common is a shared library, and a replaced operator new in a test executable does not reach code that
 * lives in another module. This target therefore compiles the library sources into itself (the same
 * method as xpe_preprocess_oom_tests, QA-A-200) and fails exactly the K-th allocation of one call, K = 1, 2, ...,
 * until a call finishes with fewer than K allocations. Every K is held to the same three questions:
 *
 *   1. Did an exception leave the C ABI function?
 *   2. If the call returned an error, is the state as the call found it (or, for the alert queue, is every
 *      alert accounted for)?
 *   3. Is the error code the right one (an allocation failure is OUT_OF_MEMORY, never a code that
 *      names a different fault)?
 *
 * No gtest macro is evaluated while the allocator is armed: gtest allocates too.
 */

#include <gtest/gtest.h>

#include "xpe/common/xpe_common_api.h"
#include "xpe/common/xpe_error.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <new>
#include <string>
#include <vector>

/* The accessor xpe_common.cpp compiles in under XPE_COMMON_TEST_HOOKS: the exact number of alerts the
 * queue has counted as lost (the loss alert's text is only a report of it). */
uint64_t xpe_common_alerts_dropped_for_test();

/* =========================================================================
 * The injecting allocator
 * ========================================================================= */

namespace {
std::atomic<long> g_failAt{0};      // 0 = disarmed; otherwise the 1-based index of the allocation to fail
std::atomic<long> g_count{0};
std::atomic<bool> g_injected{false};

void arm(long k) {
    g_count.store(0);
    g_injected.store(false);
    g_failAt.store(k);
}
bool disarm() {
    g_failAt.store(0);
    return g_injected.load();
}
}  // namespace

void* operator new(std::size_t n) {
    const long limit = g_failAt.load(std::memory_order_relaxed);
    if (limit > 0 && g_count.fetch_add(1) + 1 == limit) {
        g_injected.store(true);
        throw std::bad_alloc();
    }
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {

using Call = std::function<XpeErrorCode()>;
using Setup = std::function<void()>;
using Check = std::function<std::string(XpeErrorCode rc, bool injected)>;

/** Fails the K-th allocation of `call` for K = 1.. until a call finishes with fewer allocations. */
void sweep(const char* label, const Setup& setup, const Call& call, const Check& check) {
    constexpr long kMax = 2000;
    long injections = 0;
    for (long k = 1; k <= kMax; ++k) {
        setup();
        bool escaped = false;
        XpeErrorCode rc = XPE_OK;
        arm(k);
        try {
            rc = call();
        } catch (...) {
            escaped = true;
        }
        const bool injected = disarm();

        ASSERT_FALSE(escaped) << label << ": an exception left the C ABI function when allocation #" << k << " failed";
        const std::string why = check(rc, injected);
        ASSERT_TRUE(why.empty()) << label << ": allocation #" << k << " failed, rc " << rc << ": " << why;
        if (!injected) {
            ASSERT_GT(injections, 0) << label << ": the sweep never reached a library allocation";
            std::printf("[sweep] %-44s %3ld allocation points covered\n", label, injections);
            return;
        }
        ++injections;
    }
    FAIL() << label << ": the sweep did not finish within " << kMax << " allocations";
}

/** Whether the library considers itself initialized, asked through a call that allocates nothing. */
bool initialized() {
    float a = 0, b = 0, c = 0;
    return xpe_get_param_range("CHEST", "gamma", &a, &b, &c) == XPE_OK;
}

constexpr const char* kLossPrefix = "alert queue overflow:";

struct QueueView {
    long nonLoss{0};
    long loss{0};
    uint64_t lossTextCount{0};
};
QueueView view() {
    QueueView v;
    for (int32_t i = 0;; ++i) {
        char buf[256];
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) != XPE_OK) break;
        if (sev == XPE_ALERT_ERROR && std::strncmp(buf, kLossPrefix, std::strlen(kLossPrefix)) == 0) {
            ++v.loss;
            v.lossTextCount = std::strtoull(buf + std::strlen(kLossPrefix), nullptr, 10);
        } else {
            ++v.nonLoss;
        }
    }
    return v;
}

// Long enough that copying it allocates (std::string keeps up to 15 characters inline).
std::string longMessage(int n) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "alert-%04d: padding so that this message does not fit in a small string", n);
    return buf;
}

const std::string kLongConfig =
    "{\"note\":\"padding so that this configuration does not fit in a small string: "
    "0123456789012345678901234567890123456789012345678901234567890123456789\"}";

class CommonOom : public ::testing::Test {
protected:
    void SetUp() override { xpe_shutdown(); xpe_clear_alerts(); }
    void TearDown() override {
        xpe_log_set_file(nullptr);
        xpe_clear_alerts();
        xpe_shutdown();
        std::remove("oom_common_log_file_name_long.txt");
    }
};

}  // namespace

/* =========================================================================
 * Control: the harness reaches library code
 * ========================================================================= */

TEST_F(CommonOom, TheInjectionReachesAnAllocationMadeInsideTheLibrary) {
    ASSERT_EQ(XPE_OK, xpe_init(nullptr));
    const std::string m = longMessage(1);
    arm(1);
    // Through std::function: MSVC /EHsc treats a direct extern "C" call as non-throwing, so a try/catch
    // around it does not reliably see the exception.
    const std::function<void()> call = [&] { xpe_alert_push(m.c_str(), XPE_ALERT_INFO); };
    try { call(); } catch (...) {}
    const bool injected = disarm();
    EXPECT_TRUE(injected) << "the first allocation of xpe_alert_push must be one the harness can fail";
}

/* =========================================================================
 * xpe_alert_push: every alert is either in the queue or counted as lost
 * ========================================================================= */

namespace {
/** A queue holding `before` alerts of `sev`, with `pushed` receiving how many alerts were pushed in all. */
Setup queueOf(int before, int32_t sev, long* pushed) {
    return [before, sev, pushed] {
        xpe_shutdown();
        xpe_clear_alerts();
        ASSERT_EQ(XPE_OK, xpe_init(nullptr));
        for (int i = 0; i < before; ++i) xpe_alert_push(longMessage(i).c_str(), sev);
        *pushed = before;
    };
}

/**
 * The accounting invariant: pushed == alerts in the queue + alerts counted as lost, the queue never exceeds
 * its 64 slots, and there is at most one loss alert. After a further, unarmed push the loss alert's text
 * has caught up with the count.
 */
Check accounted(long* pushed) {
    return [pushed](XpeErrorCode, bool) -> std::string {
        QueueView v = view();
        const uint64_t dropped = xpe_common_alerts_dropped_for_test();
        char msg[160];
        if (static_cast<uint64_t>(v.nonLoss) + dropped != static_cast<uint64_t>(*pushed + 1)) {
            std::snprintf(msg, sizeof(msg), "%ld alerts pushed but %ld in the queue and %llu counted as lost",
                          *pushed + 1, v.nonLoss, static_cast<unsigned long long>(dropped));
            return msg;
        }
        if (v.nonLoss + v.loss > 64 || v.loss > 1) {
            std::snprintf(msg, sizeof(msg), "queue holds %ld alerts and %ld loss alerts", v.nonLoss, v.loss);
            return msg;
        }
        // The report catches up: one more push, then the loss alert states the exact count.
        xpe_alert_push(longMessage(9999).c_str(), XPE_ALERT_INFO);
        ++*pushed;
        v = view();
        const uint64_t dropped2 = xpe_common_alerts_dropped_for_test();
        if (dropped2 > 0 && (v.loss != 1 || v.lossTextCount != dropped2)) {
            std::snprintf(msg, sizeof(msg), "loss alert says %llu, the count is %llu",
                          static_cast<unsigned long long>(v.lossTextCount), static_cast<unsigned long long>(dropped2));
            return msg;
        }
        return {};
    };
}
}  // namespace

TEST_F(CommonOom, APushIntoAFullQueueAccountsForEveryAlert) {
    long pushed = 0;
    const std::string m = longMessage(5000);
    sweep("xpe_alert_push (full queue)", queueOf(64, XPE_ALERT_INFO, &pushed),
          [&] { xpe_alert_push(m.c_str(), XPE_ALERT_INFO); return XPE_OK; }, accounted(&pushed));
}

TEST_F(CommonOom, APushIntoAQueueWithRoomAccountsForEveryAlert) {
    long pushed = 0;
    const std::string m = longMessage(5000);
    sweep("xpe_alert_push (room in the queue)", queueOf(10, XPE_ALERT_WARNING, &pushed),
          [&] { xpe_alert_push(m.c_str(), XPE_ALERT_ERROR); return XPE_OK; }, accounted(&pushed));
}

TEST_F(CommonOom, APushIntoAQueueThatAlreadyHoldsALossAlertAccountsForEveryAlert) {
    long pushed = 0;
    const std::string m = longMessage(5000);
    sweep("xpe_alert_push (loss alert present)", queueOf(70, XPE_ALERT_INFO, &pushed),
          [&] { xpe_alert_push(m.c_str(), XPE_ALERT_INFO); return XPE_OK; }, accounted(&pushed));
}

/* =========================================================================
 * xpe_init: an error return leaves the library as the call found it
 * ========================================================================= */

TEST_F(CommonOom, AnInitThatFailsLeavesTheLibraryUninitialized) {
    sweep("xpe_init (first init)",
          [] { xpe_shutdown(); xpe_clear_alerts(); ASSERT_FALSE(initialized()); },
          [] { return xpe_init(kLongConfig.c_str()); },
          [](XpeErrorCode rc, bool) -> std::string {
              if (rc == XPE_OK) return initialized() ? std::string() : "returned OK but is not initialized";
              if (initialized()) return "returned an error but the library is initialized (partial commit)";
              if (rc != XPE_ERR_OUT_OF_MEMORY) return "an allocation failure must be XPE_ERR_OUT_OF_MEMORY";
              return {};
          });
}

TEST_F(CommonOom, ARe_initThatFailsLeavesTheQueuedAlertsAndTheInitializedStateAlone) {
    sweep("xpe_init (re-init)",
          [] {
              xpe_shutdown(); xpe_clear_alerts();
              ASSERT_EQ(XPE_OK, xpe_init(nullptr));
              for (int i = 0; i < 3; ++i) xpe_alert_push(longMessage(i).c_str(), XPE_ALERT_WARNING);
              ASSERT_EQ(3, xpe_get_pending_alert_count());
          },
          [] { return xpe_init(kLongConfig.c_str()); },
          [](XpeErrorCode rc, bool) -> std::string {
              if (rc == XPE_OK) return xpe_get_pending_alert_count() == 0 ? std::string() : "a successful re-init clears the queue";
              if (!initialized()) return "returned an error and left the library uninitialized";
              if (xpe_get_pending_alert_count() != 3) return "returned an error but the queued alerts were cleared (partial commit)";
              if (rc != XPE_ERR_OUT_OF_MEMORY) return "an allocation failure must be XPE_ERR_OUT_OF_MEMORY";
              return {};
          });
}

/* =========================================================================
 * xpe_configure / xpe_log_set_file: an allocation failure is OUT_OF_MEMORY, not the code of another fault
 * ========================================================================= */

TEST_F(CommonOom, AConfigureThatRunsOutOfMemoryIsNotReportedAsAnInvalidConfiguration) {
    // Controls: the codes the function does give for a bad document and for a good one.
    ASSERT_EQ(XPE_OK, xpe_init(nullptr));
    ASSERT_EQ(XPE_ERR_CONFIG_INVALID, xpe_configure("{bad"));
    ASSERT_EQ(XPE_OK, xpe_configure(kLongConfig.c_str()));
    sweep("xpe_configure",
          [] { xpe_shutdown(); ASSERT_EQ(XPE_OK, xpe_init(nullptr)); },
          [] { return xpe_configure(kLongConfig.c_str()); },
          [](XpeErrorCode rc, bool) -> std::string {
              if (rc == XPE_OK) return {};
              return rc == XPE_ERR_OUT_OF_MEMORY ? std::string() : "an allocation failure must be XPE_ERR_OUT_OF_MEMORY";
          });
}

TEST_F(CommonOom, ALogFileThatRunsOutOfMemoryIsNotReportedAsAnIoFailure) {
    // Control: a real I/O fault (a directory that does not exist) is still IO_FAILED.
    ASSERT_EQ(XPE_OK, xpe_init(nullptr));
    ASSERT_EQ(XPE_ERR_IO_FAILED, xpe_log_set_file("no_such_directory_for_xpe/oom_common_log.txt"));
    sweep("xpe_log_set_file",
          [] { xpe_log_set_file(nullptr); std::remove("oom_common_log_file_name_long.txt"); },
          [] { return xpe_log_set_file("oom_common_log_file_name_long.txt"); },
          [](XpeErrorCode rc, bool) -> std::string {
              if (rc == XPE_OK) return {};
              return rc == XPE_ERR_OUT_OF_MEMORY ? std::string() : "an allocation failure must be XPE_ERR_OUT_OF_MEMORY";
          });
}
