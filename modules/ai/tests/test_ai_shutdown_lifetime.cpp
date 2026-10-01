/**
 * @file test_ai_shutdown_lifetime.cpp
 * @brief xpe_ai_shutdown ends its lock before it destroys the state the lock guards (QA-B-178, Codex #25).
 *
 * Defect: xpe_ai_shutdown held a std::lock_guard on state->mtx in the scope where it ran `delete state`, so
 * the guard unlocked an already destroyed mutex when the function returned. That is undefined behaviour in a
 * single thread; no ordinary assertion sees it (the freed memory usually still holds a plausible value).
 *
 * The evidence is an ordering fact, read at the one moment that matters: the test-only hook
 * xpe_ai_test_set_before_state_delete_hook is called immediately before the delete and reports whether the
 * state's mutex is still locked (probed from a helper thread). Locked means a guard that is still alive will
 * unlock it after the delete. No external memory tool is involved, so the result does not depend on which
 * one is installed.
 *
 * Needs a build with XPE_AI_TEST_HOOKS (default ON for module tests, OFF for a delivered DLL); otherwise skips.
 */
#include <gtest/gtest.h>

#include <atomic>

#include "xpe/ai/ai_api.h"
#include "xpe/common/xpe_types.h"

#ifdef XPE_AI_TEST_HOOKS
extern "C" __declspec(dllimport) void xpe_ai_test_set_before_state_delete_hook(void (*hook)(int mutexStillHeld));

namespace {

std::atomic<int> g_calls{0};
std::atomic<int> g_heldCalls{0};

void OnBeforeDelete(int mutexStillHeld) {
    g_calls.fetch_add(1);
    if (mutexStillHeld) g_heldCalls.fetch_add(1);
}

struct ShutdownLifetimeFixture : public ::testing::Test {
    void SetUp() override {
        xpe_ai_shutdown();
        g_calls = 0;
        g_heldCalls = 0;
        xpe_ai_test_set_before_state_delete_hook(&OnBeforeDelete);
    }
    void TearDown() override {
        xpe_ai_test_set_before_state_delete_hook(nullptr);
        xpe_ai_shutdown();
    }
};

const char* const kModelDir = "C:/xpe_lifetime_probe/models";

}  // namespace

TEST_F(ShutdownLifetimeFixture, TheStateMutexIsNotHeldWhenTheStateIsDeleted) {
    ASSERT_EQ(XPE_OK, xpe_ai_init(kModelDir, nullptr));
    xpe_ai_shutdown();
    ASSERT_EQ(1, g_calls.load()) << "control: the hook must have been reached exactly once";
    EXPECT_EQ(0, g_heldCalls.load()) << "xpe_ai_shutdown deletes the state while its lock_guard is still alive";
}

TEST_F(ShutdownLifetimeFixture, ShuttingDownAnUninitialisedModuleIsANoOp) {
    xpe_ai_shutdown();
    EXPECT_EQ(0, g_calls.load()) << "nothing to delete: the hook must not run";
}

// A hundred init -> use -> shutdown cycles in one process, then one more init that is used normally. Every
// cycle reaches the delete exactly once, never with the mutex held, and after each shutdown the module is back
// to "not initialised" (the header says so) rather than being undefined.
TEST_F(ShutdownLifetimeFixture, ARepeatedInitUseShutdownCycleStaysCorrect) {
    for (int i = 0; i < 100; ++i) {
        ASSERT_EQ(XPE_OK, xpe_ai_init(kModelDir, nullptr)) << "cycle " << i;
        ASSERT_EQ(XPE_OK, xpe_ai_set_fallback_mode(1)) << "cycle " << i;
        xpe_ai_shutdown();
        ASSERT_EQ(XPE_ERR_NOT_INITIALIZED, xpe_ai_set_fallback_mode(0)) << "cycle " << i;
    }
    EXPECT_EQ(100, g_calls.load()) << "control: one delete per cycle";
    EXPECT_EQ(0, g_heldCalls.load());
    ASSERT_EQ(XPE_OK, xpe_ai_init(kModelDir, nullptr));
    EXPECT_EQ(XPE_OK, xpe_ai_set_fallback_mode(1));
}

#else  // XPE_AI_TEST_HOOKS

TEST(ShutdownLifetime, SkippedWithoutTestHooks) {
    GTEST_SKIP() << "built without XPE_AI_TEST_HOOKS: the ordering inside xpe_ai_shutdown cannot be observed";
}

#endif
