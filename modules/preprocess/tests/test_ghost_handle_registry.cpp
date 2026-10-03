/**
 * @file test_ghost_handle_registry.cpp
 * @brief QA-A-229 M5 (#245, REQ-P1A-086): a ghost handle is recognised by a registry of live handles, never by
 *        reading the memory the pointer points at.
 *
 * isValid used to test `gh->magic == kMagic`. That dereferences the caller's pointer, so a handle that was
 * already destroyed (freed memory), a pointer into some other object that happens to start with the sentinel,
 * or an unmapped address were all judged by reading them. Now only a pointer that xpe_ghost_create handed out
 * and xpe_ghost_destroy has not yet taken back is valid, and the answer comes from a mutex-protected set.
 *
 * What the registry does NOT do (written in the header): a destroyed handle's address can be handed out again
 * by a later xpe_ghost_create, and a stale pointer to it then reads as valid (ABA) -- telling the two apart needs a
 * token handle. And a destroy that runs while another thread is inside a call on the SAME handle is still the
 * caller's to prevent.
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

extern "C" XPE_API void xpe_ghost_destroy(void* handle);

namespace {

constexpr uint32_t W = 8, H = 8;

void* makeHandle() {
    void* h = nullptr;
    EXPECT_EQ(XPE_OK, xpe_ghost_create(W, H, nullptr, &h));
    xpe_clear_alerts();   // the creation warning (uncalibrated handle) is not what these cases are about
    return h;
}

XpeErrorCode correctOne(void* h) {
    std::vector<float> px(W * H, 500.0f);
    XpeImageBuffer img{};
    img.width = W; img.height = H; img.format = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = img.bitsStored = 32;
    img.data = px.data(); img.dataSize = px.size() * sizeof(float);
    XpeImageMetadata meta{};
    meta.pixelPitch_mm = 0.14f;
    const XpeErrorCode rc = xpe_ghost_correct(h, &img, &meta);
    xpe_clear_alerts();
    return rc;
}

class GhostRegistry : public ::testing::Test {
protected:
    void TearDown() override { xpe_clear_alerts(); }
};

}  // namespace

TEST_F(GhostRegistry, ALiveHandleIsValid) {
    void* h = makeHandle();
    EXPECT_EQ(XPE_OK, xpe_ghost_reset(h));
    EXPECT_EQ(XPE_OK, correctOne(h));
    xpe_ghost_destroy(h);
}

// The behaviour Boundary.GhostUseAfterDestroyReturnsError was named for and never exercised (it only passed nullptr).
TEST_F(GhostRegistry, UseAfterDestroyIsRefused) {
    void* h = makeHandle();
    xpe_ghost_destroy(h);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ghost_reset(h));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, correctOne(h));
}

// A pointer the module never handed out is refused even when the memory it points at looks exactly like a
// handle. The old magic test accepted this object, and xpe_ghost_destroy would have `delete`d a stack object.
TEST_F(GhostRegistry, AForgedHandleCarryingTheMagicIsRefused) {
    GhostCorrectorHandle forged;   // constructed: magic == kMagic, width/height 0, no buffers
    ASSERT_EQ(GhostCorrectorHandle::kMagic, forged.magic);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ghost_reset(&forged));
    xpe_ghost_destroy(&forged);    // must not free a stack object
    EXPECT_EQ(GhostCorrectorHandle::kMagic, forged.magic) << "a refused destroy must leave the object alone";
}

#ifdef _WIN32
// The answer must not depend on reading the pointer: an address with no access rights is refused, not faulted on.
TEST_F(GhostRegistry, AnUnreadableAddressIsRefusedNotDereferenced) {
    void* page = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
    ASSERT_NE(nullptr, page);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ghost_reset(page));
    xpe_ghost_destroy(page);
    VirtualFree(page, 0, MEM_RELEASE);
}
#endif

TEST_F(GhostRegistry, DestroyingTwiceIsHarmless) {
    void* h = makeHandle();
    xpe_ghost_destroy(h);
    xpe_ghost_destroy(h);   // the second call finds nothing registered and touches nothing
    SUCCEED();
}

// Several threads destroy the SAME handle at once: the registry removal is the single winner, the others return
// without touching the memory. Before, each of them read the handle and the losers could delete it twice.
TEST_F(GhostRegistry, ConcurrentDestroyOfOneHandleFreesItOnce) {
    constexpr int kThreads = 6, kRounds = 200;
    std::vector<void*> alive;                 // created first, so their addresses cannot be reused during the storm
    for (int i = 0; i < 4; ++i) alive.push_back(makeHandle());
    std::vector<void*> destroyed;
    for (int r = 0; r < kRounds; ++r) {
        void* h = makeHandle();
        destroyed.push_back(h);
        std::atomic<int> ready{0};
        std::vector<std::thread> ts;
        for (int t = 0; t < kThreads; ++t) {
            ts.emplace_back([&] {
                ready.fetch_add(1);
                while (ready.load() < kThreads) std::this_thread::yield();
                xpe_ghost_destroy(h);
            });
        }
        for (auto& t : ts) t.join();
    }
    for (void* h : destroyed) EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ghost_reset(h));
    for (void* h : alive) EXPECT_EQ(XPE_OK, xpe_ghost_reset(h)) << "a handle that was never destroyed stays valid";
    for (void* h : alive) xpe_ghost_destroy(h);
}

// The registry is shared by every handle: create / use / destroy from many threads must lose no entry and
// invent none.
TEST_F(GhostRegistry, CreateUseDestroyFromManyThreadsKeepsTheRegistryIntact) {
    std::vector<void*> alive;
    for (int i = 0; i < 4; ++i) alive.push_back(makeHandle());
    std::atomic<int> bad{0};
    std::vector<std::thread> ts;
    for (int t = 0; t < 8; ++t) {
        ts.emplace_back([&] {
            for (int i = 0; i < 300; ++i) {
                void* h = nullptr;
                if (xpe_ghost_create(W, H, nullptr, &h) != XPE_OK) { bad.fetch_add(1); continue; }
                if (xpe_ghost_reset(h) != XPE_OK) bad.fetch_add(1);
                xpe_ghost_destroy(h);
            }
        });
    }
    for (auto& t : ts) t.join();
    xpe_clear_alerts();
    EXPECT_EQ(0, bad.load());
    for (void* h : alive) EXPECT_EQ(XPE_OK, xpe_ghost_reset(h));
    for (void* h : alive) xpe_ghost_destroy(h);
}

// Not an assertion: what one validity check adds to a call, so the report can quote a measured number.
// xpe_ghost_reset on an 8x8 handle is a validity check + a lock + clearing four 64-float planes, so the same loop
// run before and after the registry gives the difference; xpe_ghost_correct makes one check per frame.
TEST_F(GhostRegistry, MeasureTheCostOfTheValidityCheck) {
    void* h = makeHandle();
    constexpr int kCalls = 2000000;
    volatile bool sink = false;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kCalls; ++i) sink = (xpe_ghost_reset(h) == XPE_OK);
    const auto t1 = std::chrono::steady_clock::now();
    (void)sink;
    const double nsPerCall = std::chrono::duration<double, std::nano>(t1 - t0).count() / kCalls;

    // One frame, for scale: a 1024 x 1024 correction through a handle (uncalibrated: passes through; calibrated
    // handles do far more work per frame, so this is the SMALLEST frame cost the check can be compared with).
    void* big = nullptr;
    ASSERT_EQ(XPE_OK, xpe_ghost_create(1024, 1024, nullptr, &big));
    xpe_clear_alerts();
    std::vector<float> px(1024u * 1024u, 500.0f);
    XpeImageBuffer img{};
    img.width = 1024; img.height = 1024; img.format = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = img.bitsStored = 32;
    img.data = px.data(); img.dataSize = px.size() * sizeof(float);
    XpeImageMetadata meta{};
    meta.pixelPitch_mm = 0.14f;
    const auto f0 = std::chrono::steady_clock::now();
    const XpeErrorCode rc = xpe_ghost_correct(big, &img, &meta);
    const auto f1 = std::chrono::steady_clock::now();
    xpe_clear_alerts();
    const double frameNs = std::chrono::duration<double, std::nano>(f1 - f0).count();
    std::printf("[ghost-handle-check] %.1f ns per xpe_ghost_reset on an 8x8 handle (%d calls); one 1024x1024 frame (rc %d): %.0f ns; "
                "reset/frame = %.6f%%\n", nsPerCall, kCalls, static_cast<int>(rc), frameNs, 100.0 * nsPerCall / frameNs);
    xpe_ghost_destroy(big);
    xpe_ghost_destroy(h);
    SUCCEED();
}
