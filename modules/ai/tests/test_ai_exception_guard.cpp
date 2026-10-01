/**
 * @file test_ai_exception_guard.cpp
 * @brief No exception leaves xpe_bone_suppress / xpe_ai_get_model_card, and the module lock is released (QA-B-181).
 *
 * Under /EHsc an `extern "C"` function that has no try region of its own does not run its destructors when a
 * callee throws, so the std::lock_guard on the module mutex stayed locked forever and xpe_ai_shutdown hung
 * (measured in QA-B-179: an input path with bytes the ANSI code page cannot convert made xpe_bone_suppress
 * throw std::system_error; an allocation failure inside xpe_ai_get_model_card did the same).
 *
 * Two kinds of evidence:
 *  - A test hook (xpe_ai_test_set_mutex_held_hook) that THROWS places an exception inside the locked region
 *    deterministically: no dependence on the allocator or on the process code page. After the call the test
 *    proves the lock is free by running xpe_ai_shutdown on a watchdog thread.
 *  - The measured trigger, an undecodable model path, when this process's code page has one. It does not on
 *    every machine (the CI code page differs from the one the defect was measured on), so that test skips
 *    with the reason instead of passing vacuously.
 *
 * Needs XPE_AI_TEST_HOOKS (default ON for module tests, OFF for a delivered DLL); otherwise skips.
 */
#include <gtest/gtest.h>

#include <windows.h>

#include <atomic>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

#include "xpe/ai/ai_api.h"
#include "xpe/ai/ai_onnx_session.h"
#include "xpe/common/xpe_types.h"

#ifdef XPE_AI_TEST_HOOKS
extern "C" __declspec(dllimport) void xpe_ai_test_set_mutex_held_hook(void (*hook)(void));

namespace {

std::atomic<int> g_throwMode{0};   // 0 none, 1 std::bad_alloc, 2 std::runtime_error

void ThrowingHook() {
    const int m = g_throwMode.load();
    if (m == 1) throw std::bad_alloc();
    if (m == 2) throw std::runtime_error("injected inside the critical section");
}

DWORD WINAPI ShutdownThread(LPVOID) {
    xpe_ai_shutdown();
    return 0;
}

/**
 * Proves the module mutex is free: xpe_ai_shutdown locks it, so it returns only if nobody holds it. A stuck
 * mutex would hang this thread and then every later test in the process, so the failing path reports and ends
 * the process instead of hanging the run.
 */
void ExpectModuleLockIsFree(const char* what) {
    HANDLE t = CreateThread(nullptr, 0, ShutdownThread, nullptr, 0, nullptr);
    ASSERT_NE(nullptr, t);
    const DWORD w = WaitForSingleObject(t, 5000);
    if (w != WAIT_OBJECT_0) {
        std::fprintf(stderr,
                     "[ FAILED ] %s: xpe_ai_shutdown did not return within 5 s -- the module mutex was left locked. "
                     "Ending the process: every later test would hang on it.\n", what);
        std::fflush(stderr);
        TerminateProcess(GetCurrentProcess(), 3);
    }
    CloseHandle(t);
}

struct Frame {
    float in[16];
    float out[16];
    XpeImageBuffer a{}, b{};
    Frame() {
        for (int i = 0; i < 16; ++i) { in[i] = static_cast<float>(i); out[i] = -7.0f; }
        a.width = b.width = 4;
        a.height = b.height = 4;
        a.bitsAllocated = b.bitsAllocated = 32;
        a.bitsStored = b.bitsStored = 32;
        a.format = b.format = XPE_PIXEL_FLOAT32;
        a.data = in;
        b.data = out;
        a.dataSize = b.dataSize = sizeof(in);
    }
    bool OutputUntouched() const {
        for (int i = 0; i < 16; ++i) if (out[i] != -7.0f) return false;
        return true;
    }
};

struct GuardFixture : public ::testing::Test {
    void SetUp() override {
        g_throwMode = 0;
        xpe_ai_shutdown();
        ASSERT_EQ(XPE_OK, xpe_ai_init("C:/xpe_exception_guard_probe", nullptr));
        xpe_ai_test_set_mutex_held_hook(&ThrowingHook);
    }
    void TearDown() override {
        g_throwMode = 0;
        xpe_ai_test_set_mutex_held_hook(nullptr);
        xpe_ai_shutdown();
    }
};

/** Runs fn; true when an exception left it. */
template <class F>
bool Escaped(F fn, XpeErrorCode* rc) {
    try {
        *rc = fn();
        return false;
    } catch (...) {
        return true;
    }
}

/** A narrow string this process's code page cannot convert to a path, or empty if it has none. */
std::string FindUndecodableName() {
    const char* candidates[] = {"C:\\xpe_probe_\xC3\x28", "C:\\xpe_probe_\x81\x20", "C:\\xpe_probe_\xFE\xFE",
                                "C:\\xpe_probe_\xA1", "C:\\xpe_probe_\xED\xA0\x80"};
    for (const char* c : candidates) {
        try {
            const std::filesystem::path p{std::string(c)};
            (void)p;
        } catch (const std::system_error&) {
            return c;
        }
    }
    return std::string();
}

}  // namespace

TEST_F(GuardFixture, BoneSuppressMapsABadAllocInTheLockedRegionToOutOfMemoryAndReleasesTheLock) {
    Frame f;
    g_throwMode = 1;
    XpeErrorCode rc = XPE_OK;
    const bool escaped = Escaped([&] { return xpe_bone_suppress(&f.a, &f.b, nullptr); }, &rc);
    g_throwMode = 0;
    EXPECT_FALSE(escaped) << "an exception left xpe_bone_suppress";
    EXPECT_EQ(XPE_ERR_OUT_OF_MEMORY, rc);
    EXPECT_TRUE(f.OutputUntouched()) << "the output must not be touched when the call fails";
    ExpectModuleLockIsFree("xpe_bone_suppress after std::bad_alloc");
}

TEST_F(GuardFixture, BoneSuppressMapsAnyOtherExceptionToProcessingFailedAndReleasesTheLock) {
    Frame f;
    g_throwMode = 2;
    XpeErrorCode rc = XPE_OK;
    const bool escaped = Escaped([&] { return xpe_bone_suppress(&f.a, &f.b, nullptr); }, &rc);
    g_throwMode = 0;
    EXPECT_FALSE(escaped) << "an exception left xpe_bone_suppress";
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, rc);
    EXPECT_TRUE(f.OutputUntouched());
    ExpectModuleLockIsFree("xpe_bone_suppress after std::runtime_error");
}

TEST_F(GuardFixture, GetModelCardMapsABadAllocInTheLockedRegionToOutOfMemoryAndReleasesTheLock) {
    char buf[4096];
    g_throwMode = 1;
    XpeErrorCode rc = XPE_OK;
    const bool escaped = Escaped([&] { return xpe_ai_get_model_card("bone_suppress_unet_v1", buf, sizeof(buf)); }, &rc);
    g_throwMode = 0;
    EXPECT_FALSE(escaped) << "an exception left xpe_ai_get_model_card";
    EXPECT_EQ(XPE_ERR_OUT_OF_MEMORY, rc);
    ExpectModuleLockIsFree("xpe_ai_get_model_card after std::bad_alloc");
}

// After a failed call the module is fully usable again: a new init, a normal card, a normal (missing-model) call.
TEST_F(GuardFixture, TheModuleStaysUsableAfterAFailedCall) {
    Frame f;
    g_throwMode = 1;
    XpeErrorCode rc = XPE_OK;
    Escaped([&] { return xpe_bone_suppress(&f.a, &f.b, nullptr); }, &rc);
    g_throwMode = 0;
    char buf[4096];
    EXPECT_EQ(XPE_OK, xpe_ai_get_model_card("bone_suppress_unet_v1", buf, sizeof(buf)));
    EXPECT_NE(std::string::npos, std::string(buf).find("bone_suppress_unet_v1"));
    EXPECT_EQ(XPE_ERR_IO_FAILED, xpe_bone_suppress(&f.a, &f.b, nullptr)) << "no model on disk: the normal -9";
}

// The trigger measured in QA-B-179, when this process's code page has an undecodable name.
TEST(AiExceptionGuard, AnUndecodableModelPathIsAnErrorCodeNotAnException) {
    const std::string bad = FindUndecodableName();
    if (bad.empty()) {
        GTEST_SKIP() << "this process's ANSI code page (" << GetACP()
                     << ") converts every candidate byte sequence; the measured trigger needs one it cannot";
    }
    xpe_ai_shutdown();
    ASSERT_EQ(XPE_OK, xpe_ai_init(bad.c_str(), nullptr));
    Frame f;
    XpeErrorCode rc = XPE_OK;
    const bool escaped = Escaped([&] { return xpe_bone_suppress(&f.a, &f.b, nullptr); }, &rc);
    EXPECT_FALSE(escaped) << "the undecodable path made xpe_bone_suppress throw";
    EXPECT_EQ(XPE_ERR_IO_FAILED, rc) << "an unusable path is reported like a missing model";
    EXPECT_TRUE(f.OutputUntouched());
    ExpectModuleLockIsFree("xpe_bone_suppress with an undecodable model path");
}

// The same trigger one level down, in the code the exe compiles itself: FileExists no longer throws.
TEST(AiExceptionGuard, OnnxSessionCreateReportsAnUndecodablePathAsInvalidModelPath) {
    const std::string bad = FindUndecodableName();
    if (bad.empty()) GTEST_SKIP() << "no undecodable name for ANSI code page " << GetACP();
    xpe::ai::OnnxSessionConfig cfg;
    cfg.model_path = bad + "\\bone_suppress.onnx";
    bool threw = false;
    xpe::ai::OnnxErrorCode code = xpe::ai::OnnxErrorCode::kOk;
    try {
        code = xpe::ai::OnnxSession::Create(cfg).code;
    } catch (...) {
        threw = true;
    }
    EXPECT_FALSE(threw);
    EXPECT_EQ(xpe::ai::OnnxErrorCode::kInvalidModelPath, code);
}

#else  // XPE_AI_TEST_HOOKS

TEST(AiExceptionGuard, SkippedWithoutTestHooks) {
    GTEST_SKIP() << "built without XPE_AI_TEST_HOOKS: an exception cannot be placed inside the locked region";
}

#endif
