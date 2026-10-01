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
