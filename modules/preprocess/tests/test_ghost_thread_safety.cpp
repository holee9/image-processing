/**
 * @file test_ghost_thread_safety.cpp
 * @brief SRS-CALIB-NFR-003 (QA-A-185, #232): one ghost handle may be used by several threads.
 *
 * xpe_ghost_correct mutates the handle's history (hist1/hist2) in place, so two threads on one
 * handle used to lose updates. The handle now carries its own mutex.
 *
 * Method. Both threads feed the same frame with the same acquisition time, so every
 * interleaving of whole calls ends in the same history as a serial replay of all calls. With
 * tau1 = tau2 = 1e12 frames the decay rounds to exactly 1.0f, the history never forgets, and
 * hist1[i] is the number of updates applied to element i: a lost update stays visible forever
 * (with the default time constants the history contracts and hides it).
 *
 * Controls, each compared with the same serial replay:
 *   - ExternalMutex : one shared handle serialised from outside -- proves the method can pass
 *   - OwnHandles    : one handle per thread -- proves the method does not fail by itself
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr uint32_t kSide = 16;
constexpr int kCallsPerThread = 2000;
constexpr int kRuns = 20;
constexpr uint64_t kAcqTime = 10u;
const char* const kNoForgetting = R"({"tau1":"1e12","tau2":"1e12"})";

XpeImageBuffer f32(std::vector<float>& px) {
    XpeImageBuffer b{};
    b.data = px.data(); b.width = kSide; b.height = kSide;
    b.bitsAllocated = 32; b.bitsStored = 32;
    b.format = XPE_PIXEL_FLOAT32;
    b.dataSize = static_cast<uint32_t>(px.size() * sizeof(float));
    return b;
}

struct State { std::vector<float> h1, h2; };

State stateOf(void* h) {
    const auto* gh = static_cast<const GhostCorrectorHandle*>(h);
    return State{gh->hist1, gh->hist2};
}

bool sameBits(const State& a, const State& b) {
    return a.h1.size() == b.h1.size() && a.h2.size() == b.h2.size() &&
           std::memcmp(a.h1.data(), b.h1.data(), a.h1.size() * sizeof(float)) == 0 &&
           std::memcmp(a.h2.data(), b.h2.data(), a.h2.size() * sizeof(float)) == 0;
}

/** Calls the handle `calls` times from the current thread. Returns the number of failed calls. */
int feed(void* h, int calls, std::mutex* external) {
    std::vector<float> px(static_cast<size_t>(kSide) * kSide);
    XpeImageMetadata meta{};
    meta.acquisitionTime = kAcqTime;
    int failures = 0;
    for (int i = 0; i < calls; ++i) {
        std::fill(px.begin(), px.end(), 1.0f);
        XpeImageBuffer img = f32(px);
        XpeErrorCode rc;
        if (external) {
            std::lock_guard<std::mutex> lk(*external);
            rc = xpe_ghost_correct(h, &img, &meta);
        } else {
            rc = xpe_ghost_correct(h, &img, &meta);
        }
        if (rc != XPE_OK) ++failures;
    }
    return failures;
}

void* create() {
    void* h = nullptr;
    EXPECT_EQ(XPE_OK, xpe_ghost_create(kSide, kSide, kNoForgetting, &h));
    return h;
}

State serialReplay(int calls) {
    void* h = create();
    EXPECT_EQ(0, feed(h, calls, nullptr));
    State s = stateOf(h);
    xpe_ghost_destroy(h);
    return s;
}

/** Runs two threads on `target[0]` / `target[1]` and returns how many runs ended in a different state. */
int runsThatDiverge(bool sharedHandle, bool externalMutex, const State& ref2K, const State& refK) {
    int diverged = 0;
    for (int run = 0; run < kRuns; ++run) {
        void* a = create();
        void* b = sharedHandle ? a : create();
        std::mutex ext;
        std::atomic<bool> go{false};
        std::atomic<int> failures{0};
        auto worker = [&](void* h) {
            while (!go.load(std::memory_order_acquire)) { /* start both threads together */ }
            failures += feed(h, kCallsPerThread, externalMutex ? &ext : nullptr);
        };
        std::thread t1(worker, a), t2(worker, b);
        go.store(true, std::memory_order_release);
        t1.join(); t2.join();

        EXPECT_EQ(0, failures.load()) << "xpe_ghost_correct returned an error";
        const bool same = sharedHandle ? sameBits(stateOf(a), ref2K)
                                       : (sameBits(stateOf(a), refK) && sameBits(stateOf(b), refK));
        if (!same) ++diverged;
        xpe_ghost_destroy(a);
        if (!sharedHandle) xpe_ghost_destroy(b);
    }
    return diverged;
}

class GhostThreadSafety : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr)); }
    void TearDown() override { xpe_preprocess_shutdown(); }
};

} // namespace

// SRS-CALIB-NFR-003: two threads on ONE handle end in the serial-replay state, every run.
TEST_F(GhostThreadSafety, SharedHandleLosesNoUpdates) {
    const State ref2K = serialReplay(2 * kCallsPerThread);
    const State refK = serialReplay(kCallsPerThread);
    EXPECT_EQ(0, runsThatDiverge(true, false, ref2K, refK))
        << "runs (of " << kRuns << ") whose final history differs from the serial replay";
}

// Control: the same comparison with the handle serialised from outside must pass.
TEST_F(GhostThreadSafety, ControlExternalMutexMatchesSerialReplay) {
    const State ref2K = serialReplay(2 * kCallsPerThread);
    const State refK = serialReplay(kCallsPerThread);
    EXPECT_EQ(0, runsThatDiverge(true, true, ref2K, refK));
}

// Control: one handle per thread must pass.
TEST_F(GhostThreadSafety, ControlOneHandlePerThreadMatchesSerialReplay) {
    const State ref2K = serialReplay(2 * kCallsPerThread);
    const State refK = serialReplay(kCallsPerThread);
    EXPECT_EQ(0, runsThatDiverge(false, false, ref2K, refK));
}

// ---------------------------------------------------------------------------
// reset() against correct() on one handle
//
// One thread runs kResetFrames correct() calls (frame value 1, history never forgets, so every
// call adds exactly 1 to every element). The other thread calls reset() as often as it can until
// the first one is done. Every serialisation of whole calls ends with ALL elements of hist1 and
// hist2 equal to one integer k in [0, kResetFrames]: k = the correct() calls after the last reset.
// A reset that runs inside a correct() leaves some elements zeroed and others not, or loses an
// update, and the final history is not uniform (or hist1 != hist2).
// ---------------------------------------------------------------------------
namespace {

constexpr uint32_t kResetSide = 64;
constexpr int kResetFrames = 400;
constexpr int kResetRuns = 60;

/** Empty string when the history is a valid serialised outcome, else a description of what is wrong. */
std::string whyNotSerialised(void* h) {
    const State s = stateOf(h);
    const float v = s.h1.empty() ? 0.0f : s.h1[0];
    for (size_t i = 0; i < s.h1.size(); ++i)
        if (s.h1[i] != v) return "hist1 is not uniform at element " + std::to_string(i);
    for (size_t i = 0; i < s.h2.size(); ++i)
        if (s.h2[i] != v) return "hist2 differs from hist1 at element " + std::to_string(i);
    if (v < 0.0f || v > static_cast<float>(kResetFrames) || v != static_cast<float>(static_cast<int>(v)))
        return "history value " + std::to_string(v) + " is not an integer in [0, " + std::to_string(kResetFrames) + "]";
    return std::string();
}

} // namespace

TEST_F(GhostThreadSafety, ResetAndCorrectOnOneHandleEndInASerialisedState) {
    int bad = 0;
    std::string firstWhy;
    for (int run = 0; run < kResetRuns; ++run) {
        void* h = nullptr;
        ASSERT_EQ(XPE_OK, xpe_ghost_create(kResetSide, kResetSide, kNoForgetting, &h));
        std::atomic<bool> go{false}, done{false};
        std::atomic<int> failures{0};

        std::thread corrector([&] {
            std::vector<float> px(static_cast<size_t>(kResetSide) * kResetSide);
            XpeImageMetadata meta{};
            meta.acquisitionTime = kAcqTime;
            while (!go.load(std::memory_order_acquire)) {}
            for (int i = 0; i < kResetFrames; ++i) {
                std::fill(px.begin(), px.end(), 1.0f);
                XpeImageBuffer img{};
                img.data = px.data(); img.width = kResetSide; img.height = kResetSide;
                img.bitsAllocated = 32; img.bitsStored = 32; img.format = XPE_PIXEL_FLOAT32;
                img.dataSize = static_cast<uint32_t>(px.size() * sizeof(float));
                if (xpe_ghost_correct(h, &img, &meta) != XPE_OK) ++failures;
            }
            done.store(true, std::memory_order_release);
        });
        std::thread resetter([&] {
            while (!go.load(std::memory_order_acquire)) {}
            while (!done.load(std::memory_order_acquire))
                if (xpe_ghost_reset(h) != XPE_OK) ++failures;
        });
        go.store(true, std::memory_order_release);
        corrector.join(); resetter.join();

        EXPECT_EQ(0, failures.load());
        const std::string why = whyNotSerialised(h);
        if (!why.empty()) { if (bad == 0) firstWhy = why; ++bad; }
        xpe_ghost_destroy(h);
    }
    EXPECT_EQ(0, bad) << "runs (of " << kResetRuns << ") that ended in a state no serial order produces; first: " << firstWhy;
}
