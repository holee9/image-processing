/**
 * @file test_ai_worker_exe_missing.cpp
 * @brief What the bone-suppression worker path does when xpe_ai_worker.exe is not next to xpe_ai.dll (QA-B-194b).
 *
 * The module looks for the worker beside its own DLL. A copy of xpe_ai.dll in a directory that holds no worker is the
 * installation this test describes (for example an application copy from which only the worker was removed). The
 * result was asked for as a fact (leader, 2026-10-03): is that failure counted toward switching the worker off?
 * The test OBSERVES it and pins what it observes; it does not assume it.
 *
 * The copy is loaded under its own path, so it is a second module with its own state; xpe_common.dll, which holds the
 * alert queue, is the one this process already has.
 */

#include <gtest/gtest.h>

#include "xpe/ai/ai_api.h"
#include "xpe/ai/ai_onnx_session.h"
#include "xpe/common/xpe_error.h"

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#ifndef XPE_AI_DLL_PATH
#error "XPE_AI_DLL_PATH must be defined by the build (modules/ai/CMakeLists.txt)"
#endif
#ifndef XPE_AI_TEST_DATA_DIR
#error "XPE_AI_TEST_DATA_DIR must be defined by the build (modules/ai/CMakeLists.txt)"
#endif
#ifndef XPE_AI_WORKER_EXE
#error "XPE_AI_WORKER_EXE must be defined by the build (modules/ai/CMakeLists.txt)"
#endif

namespace {

namespace fs = std::filesystem;

using InitFn = XpeErrorCode (*)(const char*, const char*);
using ShutdownFn = XpeErrorCode (*)();
using BoneFn = XpeErrorCode (*)(const XpeImageBuffer*, XpeImageBuffer*, const XpeImageMetadata*);
using StateFn = XpeErrorCode (*)(int32_t*, uint32_t*, uint32_t*);

struct Copy {
    fs::path dir;
    HMODULE mod{nullptr};
    InitFn init{nullptr};
    ShutdownFn shutdown{nullptr};
    BoneFn bone{nullptr};
    StateFn state{nullptr};

    explicit Copy(bool withWorker = false) {
        const fs::path src(XPE_AI_DLL_PATH);
        dir = fs::temp_directory_path() / ("xpe_noworker_" + std::to_string(GetCurrentProcessId()));
        std::error_code ec;
        fs::remove_all(dir, ec);
        fs::create_directories(dir);
        // xpe_ai.dll and what it loads from its own directory; NOT xpe_ai_worker.exe.
        for (const char* name : {"xpe_ai.dll", "onnxruntime.dll"}) {
            if (fs::exists(src.parent_path() / name)) fs::copy_file(src.parent_path() / name, dir / name);
        }
        if (withWorker) fs::copy_file(fs::path(XPE_AI_WORKER_EXE), dir / "xpe_ai_worker.exe");
        mod = LoadLibraryExA((dir / "xpe_ai.dll").string().c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!mod) return;
        init = reinterpret_cast<InitFn>(GetProcAddress(mod, "xpe_ai_init"));
        shutdown = reinterpret_cast<ShutdownFn>(GetProcAddress(mod, "xpe_ai_shutdown"));
        bone = reinterpret_cast<BoneFn>(GetProcAddress(mod, "xpe_bone_suppress"));
        state = reinterpret_cast<StateFn>(GetProcAddress(mod, "xpe_ai_worker_state"));
    }
    ~Copy() {
        if (mod) {
            if (shutdown) shutdown();
            FreeLibrary(mod);
        }
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
    bool Ready() const { return mod && init && shutdown && bone && state; }
    Copy(const Copy&) = delete;
    Copy& operator=(const Copy&) = delete;
};

}  // namespace

TEST(WorkerExeMissing, TheBoneSuppressionWorkerPathCountsAMissingWorkerExecutableAsAFailure) {
    Copy c;
    ASSERT_TRUE(c.Ready()) << "could not load the copy of xpe_ai.dll (error " << GetLastError() << ")";
    ASSERT_FALSE(fs::exists(c.dir / "xpe_ai_worker.exe")) << "the copy must have no worker beside it";
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, c.init(c.dir.string().c_str(), "{\"use_worker\": true}"));

    int32_t observedState[4] = {};
    uint32_t observedFailures[4] = {};
    XpeErrorCode observedRc[4] = {};
    for (int i = 0; i < 4; ++i) {
        std::vector<float> in(9, 1.0f), out(9, -777.0f);
        XpeImageBuffer ib{}, ob{};
        ib.width = ib.height = ob.width = ob.height = 3;
        ib.bitsAllocated = ib.bitsStored = ob.bitsAllocated = ob.bitsStored = 32;
        ib.format = ob.format = XPE_PIXEL_FLOAT32;
        ib.data = in.data();
        ob.data = out.data();
        ib.dataSize = ob.dataSize = in.size() * sizeof(float);
        observedRc[i] = c.bone(&ib, &ob, nullptr);
        uint32_t ceiling = 0;
        ASSERT_EQ(XPE_OK, c.state(&observedState[i], &observedFailures[i], &ceiling));
    }
    // What was observed, pinned: each call fails with the code of "could not start the worker", the count grows by one
    // per call, the third failure switches the worker off, and the fourth call returns at once (no new failure).
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(XPE_ERR_IO_FAILED, observedRc[i]) << "call " << i;
        EXPECT_EQ(static_cast<uint32_t>(i + 1), observedFailures[i]) << "call " << i << ": a missing worker is counted";
    }
    EXPECT_EQ(XPE_AI_WORKER_DISABLED, observedState[2]) << "the third failure switches the worker off";
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, observedRc[3]) << "a switched-off worker returns the documented fallback signal";
    EXPECT_EQ(XPE_AI_WORKER_DISABLED, observedState[3]);
    xpe_clear_alerts();
}

// The control: the SAME copy of the DLL, with the worker put beside it and a signed model, does the work. So the
// failures above come from the absence of the worker and from nothing else about the copy.
TEST(WorkerExeMissing, ControlTheSameCopyWithTheWorkerBesideItProcessesAnImage) {
    if (xpe::ai::OnnxSession::IsStubBuild()) GTEST_SKIP() << "stub build: no worker runs a model";
    Copy c(/*withWorker=*/true);
    ASSERT_TRUE(c.Ready()) << "could not load the copy of xpe_ai.dll (error " << GetLastError() << ")";
    for (const char* name : {"bone_suppress.onnx", "bone_suppress.json", "bone_suppress.sig"}) {
        const fs::path from = fs::path(XPE_AI_TEST_DATA_DIR) / "models_x2" / name;
        if (fs::exists(from)) fs::copy_file(from, c.dir / name);
    }
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, c.init(c.dir.string().c_str(), "{\"use_worker\": true}"));
    std::vector<float> in(9, 1.0f), out(9, -777.0f);
    XpeImageBuffer ib{}, ob{};
    ib.width = ib.height = ob.width = ob.height = 3;
    ib.bitsAllocated = ib.bitsStored = ob.bitsAllocated = ob.bitsStored = 32;
    ib.format = ob.format = XPE_PIXEL_FLOAT32;
    ib.data = in.data();
    ob.data = out.data();
    ib.dataSize = ob.dataSize = in.size() * sizeof(float);
    EXPECT_EQ(XPE_OK, c.bone(&ib, &ob, nullptr));
    for (const float v : out) EXPECT_EQ(2.0f, v);
    xpe_clear_alerts();
}
