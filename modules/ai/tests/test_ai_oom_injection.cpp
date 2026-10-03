/**
 * @file test_ai_oom_injection.cpp
 * @brief Allocation-failure sweeps over the xpe_ai entry points that allocate and keep state (QA-B-194 M5, #130).
 *
 * xpe_ai.dll binds to its own runtime's allocator, and a replaced operator new in a test executable does not reach
 * code that lives in another module. This target therefore compiles the library sources into itself (the method
 * of xpe_preprocess_oom_tests and xpe_common_oom_tests) and fails exactly the K-th allocation of one call,
 * K = 1, 2, ... until a call finishes with fewer than K allocations. Every K is held to the same questions:
 *
 *   1. Did an exception leave the C ABI function?
 *   2. If the call returned an error, is the module as the call found it, and did the failed call leak? (Live
 *      allocation blocks are counted: after the call's own cleanup the count is where it was before the call.)
 *   3. Is the code the right one -- an allocation failure is XPE_ERR_OUT_OF_MEMORY, never a code that names a
 *      different fault?
 *   4. Is the module USABLE afterwards (an unarmed repeat of the same call gives its normal answer)?
 *
 * What a sweep covers, so nobody reads it as more: only allocations made by `operator new` in THIS executable --
 * the module's own containers, strings, nlohmann::json, spdlog and the C++ ONNX Runtime wrapper objects. The
 * allocations inside xpe_common (the alert queue) and inside ONNX Runtime's own allocator are NOT failed, so a
 * session-creation failure inside ONNX Runtime itself is not proven by any sweep here.
 *
 * WHAT NO SWEEP HERE CAN COVER (found by the first run of this file): nlohmann::json 3.11.3 destroys a NON-EMPTY
 * array or object by moving its children into a std::vector, and its destructor is noexcept -- so when that
 * allocation fails the process calls std::terminate, from inside the library, where this module cannot catch it
 * (stack at the terminate: operator new <- std::vector<json>::reserve <- ~basic_json <- parseConfig, K=25 of an
 * xpe_ai_init that was given a config with keys). The module uses a json DOM in three places: parseConfig, the
 * body-part label sidecar (ai_bodypart_model.h) and the model-metadata sidecar (ai_onnx_session.cpp). A sweep that
 * reaches a non-empty DOM therefore ends the whole executable, not one test. The sweeps below stay on either side
 * of that: xpe_ai_init with NO config and with an EMPTY object (the parser allocates, the empty DOM's destructor
 * does not), body-part recognition on its WARM path (the model and labels are already loaded; the cold path reads
 * the sidecar). The cold body-part path and a config with keys are NOT proven by any sweep.
 *
 * No gtest macro is evaluated while the allocator is armed: gtest allocates too. Findings are collected and
 * reported together at the end of each sweep, so one run shows every K that misbehaved.
 */

#include <gtest/gtest.h>

#include "xpe/ai/ai_onnx_session.h"
#include "xpe/ai/ai_api.h"
#include "xpe/common/xpe_error.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <new>
#include <string>
#include <vector>

#ifndef XPE_AI_TEST_DATA_DIR
#error "XPE_AI_TEST_DATA_DIR must be defined by the build (modules/ai/CMakeLists.txt)"
#endif

/* =========================================================================
 * The injecting allocator
 * ========================================================================= */

namespace {
std::atomic<long> g_failAt{0};   // 0 = disarmed; otherwise the 1-based index of the allocation to fail
std::atomic<long> g_count{0};
std::atomic<bool> g_injected{false};
std::atomic<long> g_live{0};     // blocks allocated by this operator new and not yet freed

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
    if (void* p = std::malloc(n ? n : 1)) {
        g_live.fetch_add(1, std::memory_order_relaxed);
        return p;
    }
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return operator new(n); }
void operator delete(void* p) noexcept {
    if (p) g_live.fetch_sub(1, std::memory_order_relaxed);
    std::free(p);
}
void operator delete[](void* p) noexcept { operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { operator delete(p); }

namespace {

const std::string kData = XPE_AI_TEST_DATA_DIR;
// Long enough that copying it allocates (std::string keeps up to 15 characters inline).
const std::string kModelsX2 = kData + "/models_x2";
const std::string kBodyPartA = kData + "/models_bodypart_a";

struct Observation {
    XpeErrorCode rc{XPE_OK};
    bool injected{false};
    long liveDelta{0};   // live blocks after the call minus before it
};

using Setup = std::function<void()>;
using Call = std::function<XpeErrorCode()>;
using Cleanup = std::function<void()>;
/** Returns "" when this K is fine, otherwise what is wrong. Runs disarmed. */
using Check = std::function<std::string(const Observation&)>;

/**
 * Fails the K-th allocation of `call` for K = 1.. until a call finishes with fewer allocations. @p cleanup returns the
 * module to the state before @p setup; after it, the live block count must equal what it was before the setup.
 */
void sweep(const char* label, const Setup& setup, const Call& call, const Cleanup& cleanup, const Check& check) {
    constexpr long kMax = 4000;
    // One unarmed lap first: whatever the module and its libraries create once and keep (logger, thread-local
    // buffers, ONNX Runtime's environment) exists before the count that must come back to its baseline.
    setup();
    (void)call();
    cleanup();

    long injections = 0;
    std::vector<std::string> problems;
    std::map<int, long> codes;   // rc -> how many K ended with it
    for (long k = 1; k <= kMax; ++k) {
        const long before = g_live.load();
        setup();
        const long afterSetup = g_live.load();
        bool escaped = false;
        Observation o;
        arm(k);
        try {
            o.rc = call();
        } catch (...) {
            escaped = true;
        }
        o.injected = disarm();
        o.liveDelta = g_live.load() - afterSetup;

        std::string why;
        if (!escaped) why = check(o);
        cleanup();
        // Measured BEFORE this loop records anything: the bookkeeping below allocates (a map node, a string), and
        // counting it would report the harness's own memory as the module's. `why` may own one block of its own.
        const long leaked = g_live.load() - before - (why.capacity() > 15 ? 1 : 0);

        if (escaped) {
            problems.push_back("K=" + std::to_string(k) + ": an exception left the C ABI function");
        } else {
            ++codes[static_cast<int>(o.rc)];
            if (!why.empty()) problems.push_back("K=" + std::to_string(k) + " rc=" + std::to_string(o.rc) + ": " + why);
        }
        if (leaked != 0) {
            problems.push_back("K=" + std::to_string(k) + ": " + std::to_string(leaked) +
                               " allocation(s) still live after the call and its cleanup");
        }
        if (!o.injected && !escaped) {
            if (injections == 0) problems.push_back("the sweep never reached a library allocation");
            break;
        }
        ++injections;
    }
    std::printf("[sweep] %-46s %4ld allocation points covered; results:", label, injections);
    for (const auto& c : codes) std::printf(" rc %d x%ld;", c.first, c.second);
    std::printf("\n");
    if (problems.size() > 25) problems.resize(25);
    std::string all;
    for (const std::string& p : problems) all += "\n  " + p;
    ASSERT_TRUE(problems.empty()) << label << ": " << all;
    ASSERT_LT(injections, kMax) << label << ": the sweep did not finish within " << kMax << " allocations";
}

bool IsInitialised() {
    int32_t state = -1;
    return xpe_ai_worker_state(&state, nullptr, nullptr) != XPE_ERR_NOT_INITIALIZED;   // allocates nothing
}

void Reset() {
    xpe_ai_shutdown();
    xpe_clear_alerts();
}

struct Img {
    std::vector<float> px;
    XpeImageBuffer buf{};
    Img(uint32_t w, uint32_t h, float base) : px(static_cast<size_t>(w) * h) {
        for (size_t i = 0; i < px.size(); ++i) px[i] = base + static_cast<float>(i);
        buf.width = w;
        buf.height = h;
        buf.bitsAllocated = 32;
        buf.bitsStored = 32;
        buf.format = XPE_PIXEL_FLOAT32;
        buf.data = px.data();
        buf.dataSize = px.size() * sizeof(float);
    }
};

class AiOom : public ::testing::Test {
protected:
    void SetUp() override { Reset(); }
    void TearDown() override { Reset(); }
};

bool IsStubBuild() { return xpe::ai::OnnxSession::IsStubBuild(); }

}  // namespace

/* =========================================================================
 * Control: the harness reaches library code
 * ========================================================================= */

TEST_F(AiOom, TheInjectionReachesAnAllocationMadeInsideTheModule) {
    // Without this, a sweep that finds nothing could mean "the allocator never reaches the module".
    const std::function<XpeErrorCode()> call = [] {
        return xpe_ai_init("a model directory path that is long enough to need the heap", nullptr);
    };
    arm(1);
    try { (void)call(); } catch (...) {}
    const bool injected = disarm();
    EXPECT_TRUE(injected) << "the first allocation of xpe_ai_init must be one the harness can fail";
}

/* =========================================================================
 * xpe_ai_init: nothing leaks and the module is as it was
 * ========================================================================= */

namespace {
Check initChecks() {
    return [](const Observation& o) -> std::string {
        if (o.rc == XPE_OK) return IsInitialised() ? "" : "returned OK but the module is not initialised";
        if (o.rc != XPE_ERR_OUT_OF_MEMORY) return "an allocation failure must be OUT_OF_MEMORY";
        if (IsInitialised()) return "the call failed but the module reports itself initialised";
        if (o.liveDelta != 0) return std::to_string(o.liveDelta) + " allocation(s) kept by a failed init";
        return "";
    };
}
}  // namespace

TEST_F(AiOom, InitWithoutAConfigFailsCleanlyAtEveryAllocation) {
    const std::string dir = kModelsX2;
    sweep("xpe_ai_init (no config)",
          [] { Reset(); },
          [&] { return xpe_ai_init(dir.c_str(), nullptr); },
          [] { Reset(); },
          initChecks());
}

TEST_F(AiOom, InitWithAnEmptyConfigObjectFailsCleanlyAtEveryAllocation) {
    // "{}" runs the JSON parser (which allocates) and leaves an empty DOM, whose destructor does not allocate: the
    // largest config that can be swept (see the file header).
    const std::string dir = kModelsX2;
    sweep("xpe_ai_init (config {})",
          [] { Reset(); },
          [&] { return xpe_ai_init(dir.c_str(), "{}"); },
          [] { Reset(); },
          initChecks());
}

TEST_F(AiOom, ARefusedInitIsUsableAfterwards) {
    // The state a failed init leaves must not poison the next one: after every K, an unarmed init succeeds.
    const std::string dir = kModelsX2;
    long tried = 0, worked = 0;
    sweep("xpe_ai_init, then an unarmed init",
          [] { Reset(); },
          [&] { return xpe_ai_init(dir.c_str(), "{}"); },
          [&] {
              // the cleanup of THIS sweep is the check: after a FAILED first init, a second, unarmed init must work
              if (!IsInitialised()) {
                  ++tried;
                  if (xpe_ai_init(dir.c_str(), "{}") == XPE_OK) ++worked;
              }
              Reset();
          },
          [](const Observation&) -> std::string { return ""; });
    EXPECT_GT(tried, 0) << "no K failed the first init, so the second one was never tried";
    EXPECT_EQ(tried, worked) << "after a failed init, a later unarmed init did not succeed";
}

/* =========================================================================
 * xpe_ai_get_model_card
 * ========================================================================= */

namespace {
Setup initModels() {
    return [] {
        Reset();
        (void)xpe_ai_init(kModelsX2.c_str(), nullptr);
    };
}
}  // namespace

// QA-B-197 M2: the card is made from the verified sidecar of a model in the model directory (it used to be a constant),
// so the first call reads and verifies the files, parses the sidecar and builds the card -- all of it under the sweep.
TEST_F(AiOom, ModelCardOfALoadedModelFailsCleanlyAtEveryAllocation) {
    char buf[4096];
    sweep("xpe_ai_get_model_card (a loaded model)",
          initModels(),
          [&] { return xpe_ai_get_model_card("bone_toy_x2", buf, sizeof(buf)); },
          [] { Reset(); },
          [&](const Observation& o) -> std::string {
              if (o.rc == XPE_OK) return buf[0] == '{' ? "" : "OK but no card";
              if (o.rc != XPE_ERR_OUT_OF_MEMORY) return "an allocation failure must be OUT_OF_MEMORY";
              if (o.liveDelta != 0) return std::to_string(o.liveDelta) + " allocation(s) kept by a failed call";
              // usable afterwards: the lock is not held and the answer is the normal one
              return xpe_ai_get_model_card("bone_toy_x2", buf, sizeof(buf)) == XPE_OK ? "" : "not usable afterwards";
          });
}

TEST_F(AiOom, ModelCardOfAnUnknownModelFailsCleanlyAtEveryAllocation) {
    char buf[4096];
    sweep("xpe_ai_get_model_card (not loaded)",
          initModels(),
          [&] { return xpe_ai_get_model_card("no_such_model_for_the_sweep", buf, sizeof(buf)); },
          [] { Reset(); },
          [&](const Observation& o) -> std::string {
              if (o.rc == XPE_ERR_CONFIG_INVALID) return "";   // the documented unavailable answer (QA-B-197: was IO_FAILED)
              if (o.rc != XPE_ERR_OUT_OF_MEMORY) return "an allocation failure must be OUT_OF_MEMORY";
              if (o.liveDelta != 0) return std::to_string(o.liveDelta) + " allocation(s) kept by a failed call";
              return xpe_ai_get_model_card("no_such_model_for_the_sweep", buf, sizeof(buf)) == XPE_ERR_CONFIG_INVALID
                         ? ""
                         : "not usable afterwards";
          });
}

/* =========================================================================
 * xpe_bone_suppress, in process (needs ONNX Runtime for a model to run)
 * ========================================================================= */

TEST_F(AiOom, BoneSuppressionFailsCleanlyAtEveryAllocation) {
    if (IsStubBuild()) GTEST_SKIP() << "stub build: no model is loaded, so the in-process path has nothing to run";
    Img in(3, 3, 1.0f);
    Img out(3, 3, 0.0f);
    sweep("xpe_bone_suppress (in process, cold session)",
          initModels(),
          [&] {
              std::fill(out.px.begin(), out.px.end(), -777.0f);
              return xpe_bone_suppress(&in.buf, &out.buf, nullptr);
          },
          [] { Reset(); },
          [&](const Observation& o) -> std::string {
              if (o.rc == XPE_OK) {
                  for (size_t i = 0; i < in.px.size(); ++i) {
                      if (out.px[i] != in.px[i] * 2.0f) return "OK but the pixels are not the model's";
                  }
                  return "";
              }
              if (o.rc != XPE_ERR_OUT_OF_MEMORY) return "an allocation failure must be OUT_OF_MEMORY";
              // not usable afterwards? the same call, unarmed, on the same module state
              std::fill(out.px.begin(), out.px.end(), -777.0f);
              if (xpe_bone_suppress(&in.buf, &out.buf, nullptr) != XPE_OK) return "not usable afterwards";
              return "";
          });
}

/* =========================================================================
 * xpe_bodypart_recognize, in process (needs ONNX Runtime)
 * ========================================================================= */

TEST_F(AiOom, BodyPartRecognitionFailsCleanlyAtEveryAllocation) {
    if (IsStubBuild()) GTEST_SKIP() << "stub build: no model is loaded, so the in-process path has nothing to run";
    Img in(4, 4, 0.0f);
    char label[64];
    float confidence = -1.0f;
    sweep("xpe_bodypart_recognize (in process, warm model)",
          [&] {
              Reset();
              (void)xpe_ai_init(kBodyPartA.c_str(), nullptr);
              // load the model and its labels unarmed: the cold path reads the json sidecar (see the file header)
              char l[64];
              float c = 0.0f;
              (void)xpe_bodypart_recognize(&in.buf, l, sizeof(l), &c);
          },
          [&] {
              std::memset(label, 'x', sizeof(label));
              confidence = -1.0f;
              return xpe_bodypart_recognize(&in.buf, label, sizeof(label), &confidence);
          },
          [] { Reset(); },
          [&](const Observation& o) -> std::string {
              if (o.rc == XPE_OK) return std::strcmp(label, "CHEST") == 0 ? "" : "OK but not the model's label";
              if (o.rc != XPE_ERR_OUT_OF_MEMORY) return "an allocation failure must be OUT_OF_MEMORY";
              float c = 0.0f;
              char l[64];
              if (xpe_bodypart_recognize(&in.buf, l, sizeof(l), &c) != XPE_OK) return "not usable afterwards";
              return "";
          });
}

/* =========================================================================
 * Session creation that runs out of memory, on the cold paths no allocation sweep can reach (QA-B-194b)
 *
 * The cold body-part path reads a JSON sidecar, and a sweep that fails an allocation inside a non-empty nlohmann DOM
 * ends the process (see the file header). The shortage that matters here happens at one place -- where the ONNX Runtime
 * session is built -- so it is injected there: OnnxSession::TestSetBeforeSessionHook is called inside the try block
 * that builds the session, and a hook that throws std::bad_alloc is that shortage at exactly that place.
 * ========================================================================= */

namespace xpe::ai {
void TestSetBeforeSessionHook(void (*hook)());   // ai_onnx_session.cpp, test builds only
}

namespace {
void ThrowBadAlloc() { throw std::bad_alloc(); }
struct SessionOomScope {
    SessionOomScope() { xpe::ai::TestSetBeforeSessionHook(&ThrowBadAlloc); }
    ~SessionOomScope() { xpe::ai::TestSetBeforeSessionHook(nullptr); }
    SessionOomScope(const SessionOomScope&) = delete;
    SessionOomScope& operator=(const SessionOomScope&) = delete;
};
}  // namespace

TEST_F(AiOom, ABodyPartSessionThatRunsOutOfMemoryIsOutOfMemoryNotAnUnavailableModel) {
    if (IsStubBuild()) GTEST_SKIP() << "stub build: no session is built";
    Img in(4, 4, 0.0f);
    char label[64];
    float confidence = -1.0f;
    ASSERT_EQ(XPE_OK, xpe_ai_init(kBodyPartA.c_str(), nullptr));
    XpeErrorCode rc;
    {
        const SessionOomScope oom;
        rc = xpe_bodypart_recognize(&in.buf, label, sizeof(label), &confidence);
    }
    EXPECT_EQ(XPE_ERR_OUT_OF_MEMORY, rc) << "the cold first call; was UNKNOWN + PROCESSING_FAILED before QA-B-194b";
    EXPECT_EQ(0, xpe_get_pending_alert_count()) << "a shortage of memory is not 'the model is unavailable': no alert";
    // not remembered as unavailable: the next call, with memory, loads the model and answers
    EXPECT_EQ(XPE_OK, xpe_bodypart_recognize(&in.buf, label, sizeof(label), &confidence));
    EXPECT_STREQ("CHEST", label);
}

// QA-B-195c (Codex #90): the same shortage, but while the body-part LABELS are read, which used to be reported as a
// sidecar that is "not valid JSON" -- the model unavailable -- instead of out of memory.
namespace xpe::ai {
void TestSetBeforeLabelParseHook(void (*hook)());   // ai_onnx_session.cpp, test builds only
}

namespace {
int g_labelCallsUntilFailure = 0;
void FailTheKthLabelCall() {
    if (--g_labelCallsUntilFailure == 0) throw std::bad_alloc();
}
struct LabelOomScope {
    explicit LabelOomScope(int k) {
        g_labelCallsUntilFailure = k;
        xpe::ai::TestSetBeforeLabelParseHook(&FailTheKthLabelCall);
    }
    ~LabelOomScope() { xpe::ai::TestSetBeforeLabelParseHook(nullptr); }
    LabelOomScope(const LabelOomScope&) = delete;
    LabelOomScope& operator=(const LabelOomScope&) = delete;
};
}  // namespace

TEST_F(AiOom, AShortageOfMemoryWhileReadingTheBodyPartLabelsIsOutOfMemoryNotAnUnavailableModel) {
    if (IsStubBuild()) GTEST_SKIP() << "stub build: no session is built";
    Img in(4, 4, 0.0f);
    char label[64];
    float confidence = -1.0f;
    // models_bodypart_a has three labels: the hook is reached for the start of the parse and once per label
    for (int k = 1; k <= 4; ++k) {
        xpe_ai_shutdown();
        xpe_clear_alerts();
        ASSERT_EQ(XPE_OK, xpe_ai_init(kBodyPartA.c_str(), nullptr));
        XpeErrorCode rc;
        {
            const LabelOomScope oom(k);
            rc = xpe_bodypart_recognize(&in.buf, label, sizeof(label), &confidence);
        }
        EXPECT_EQ(XPE_ERR_OUT_OF_MEMORY, rc) << "call " << k << ": was UNKNOWN + PROCESSING_FAILED (the model 'unavailable')";
        EXPECT_EQ(0, xpe_get_pending_alert_count()) << "call " << k << ": a shortage of memory raises no 'unavailable' alert";
        // not remembered: the next call, with memory, loads the model and answers
        EXPECT_EQ(XPE_OK, xpe_bodypart_recognize(&in.buf, label, sizeof(label), &confidence)) << "call " << k;
        EXPECT_STREQ("CHEST", label);
    }
}

TEST_F(AiOom, ABoneSessionThatRunsOutOfMemoryIsOutOfMemoryOnTheInProcessPath) {
    if (IsStubBuild()) GTEST_SKIP() << "stub build: no session is built";
    Img in(3, 3, 1.0f);
    Img out(3, 3, 0.0f);
    ASSERT_EQ(XPE_OK, xpe_ai_init(kModelsX2.c_str(), nullptr));
    std::fill(out.px.begin(), out.px.end(), -777.0f);
    XpeErrorCode rc;
    {
        const SessionOomScope oom;
        rc = xpe_bone_suppress(&in.buf, &out.buf, nullptr);
    }
    EXPECT_EQ(XPE_ERR_OUT_OF_MEMORY, rc);
    for (const float v : out.px) EXPECT_EQ(-777.0f, v) << "the output is untouched";
    EXPECT_EQ(XPE_OK, xpe_bone_suppress(&in.buf, &out.buf, nullptr)) << "usable afterwards";
}

/* =========================================================================
 * The buffer that holds a whole model cannot be allocated (QA-B-195b, Codex #88)
 *
 * Loading reads the model into ONE buffer the size of the file, then the sidecar and the signature the same way. That
 * allocation sat before the catch that maps a shortage of memory, which the C ABI's outer catch hid on this path (the
 * worker had no such catch: see test_ai_worker_boundary.cpp). A hook that throws std::bad_alloc where the buffer is
 * allocated is that shortage at that place; the first read of the load is the model's.
 * ========================================================================= */

namespace xpe::ai {
void TestSetBeforeFileReadHook(void (*hook)());   // ai_onnx_session.cpp, test builds only
}

namespace {
int g_readsUntilFailure = 0;
void FailTheKthRead() {
    if (--g_readsUntilFailure == 0) throw std::bad_alloc();
}
struct ReadOomScope {
    explicit ReadOomScope(int k) {
        g_readsUntilFailure = k;
        xpe::ai::TestSetBeforeFileReadHook(&FailTheKthRead);
    }
    ~ReadOomScope() { xpe::ai::TestSetBeforeFileReadHook(nullptr); }
    ReadOomScope(const ReadOomScope&) = delete;
    ReadOomScope& operator=(const ReadOomScope&) = delete;
};
}  // namespace

TEST_F(AiOom, AModelBufferThatCannotBeAllocatedIsOutOfMemoryOnTheInProcessBonePath) {
    if (IsStubBuild()) GTEST_SKIP() << "stub build: no model is loaded";
    for (int k = 1; k <= 3; ++k) {   // the model, its sidecar, its signature
        Reset();
        Img in(3, 3, 1.0f);
        Img out(3, 3, 0.0f);
        ASSERT_EQ(XPE_OK, xpe_ai_init(kModelsX2.c_str(), nullptr));
        std::fill(out.px.begin(), out.px.end(), -777.0f);
        XpeErrorCode rc;
        {
            const ReadOomScope oom(k);
            rc = xpe_bone_suppress(&in.buf, &out.buf, nullptr);
        }
        EXPECT_EQ(XPE_ERR_OUT_OF_MEMORY, rc) << "read " << k;
        EXPECT_EQ(0, xpe_get_pending_alert_count()) << "read " << k << ": a shortage of memory raises no 'unavailable' alert";
        for (const float v : out.px) EXPECT_EQ(-777.0f, v) << "read " << k << ": the output is untouched";
        EXPECT_EQ(XPE_OK, xpe_bone_suppress(&in.buf, &out.buf, nullptr)) << "read " << k << ": usable afterwards";
        for (size_t i = 0; i < in.px.size(); ++i) EXPECT_EQ(in.px[i] * 2.0f, out.px[i]) << "read " << k << " pixel " << i;
    }
}

TEST_F(AiOom, AModelBufferThatCannotBeAllocatedIsOutOfMemoryOnTheInProcessBodyPartPath) {
    if (IsStubBuild()) GTEST_SKIP() << "stub build: no model is loaded";
    for (int k = 1; k <= 3; ++k) {
        Reset();
        Img in(4, 4, 0.0f);
        char label[64];
        float confidence = -1.0f;
        ASSERT_EQ(XPE_OK, xpe_ai_init(kBodyPartA.c_str(), nullptr));
        XpeErrorCode rc;
        {
            const ReadOomScope oom(k);
            rc = xpe_bodypart_recognize(&in.buf, label, sizeof(label), &confidence);
        }
        EXPECT_EQ(XPE_ERR_OUT_OF_MEMORY, rc) << "read " << k;
        EXPECT_EQ(0, xpe_get_pending_alert_count()) << "read " << k;
        EXPECT_EQ(XPE_OK, xpe_bodypart_recognize(&in.buf, label, sizeof(label), &confidence)) << "read " << k << ": usable afterwards";
        EXPECT_STREQ("CHEST", label);
    }
}
