/**
 * @file test_parallel_unwind.cpp
 * @brief Reviewer PRE313-317 P1 (QA-A-315, Refs #278): when starting a later range's task fails with an exception
 *        xpe_parallel_chunks does not catch (std::bad_alloc), every range already running finishes before the per-range
 *        error slots it writes are freed.
 *
 * Its own executable because it replaces the global operator new/delete, which only code compiled into the same
 * executable sees (xpe_parallel_chunks is a header template, so it is). The replacement does nothing unless a test arms
 * it, and then it acts only on allocations made by the test's own thread:
 *  - count mode: counts them, to learn how many allocations starting one task costs on this runtime;
 *  - inject mode: the first allocation of the second task's launch throws std::bad_alloc, after waiting until the first
 *    task is running; and the first block of the error-slot size (the slots) is not returned to the heap when deleted:
 *    it is zero-filled and kept, so a write into it after its release shows as a non-zero byte instead of corrupting
 *    the heap.
 */

#include <gtest/gtest.h>

#include "parallel.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <new>
#include <stdexcept>
#include <thread>

namespace {

using namespace std::chrono_literals;

thread_local bool tl_caller = false;           // set only on the test's own thread while armed
std::atomic<bool> g_armed{false};
std::atomic<long> g_count{0};                  // caller-thread allocations while armed
std::atomic<long> g_failAt{0};                 // 1-based index of the caller-thread allocation to fail; 0 = count only
std::atomic<bool> g_injected{false};
std::atomic<size_t> g_watchSize{0};            // inject mode: size of the error-slot block
std::atomic<void*> g_watch{nullptr};
std::atomic<bool> g_watchFreed{false};
std::atomic<bool> g_injectMode{false};
std::atomic<bool> g_task1Started{false};

bool waitFor(const std::atomic<bool>& flag, std::chrono::milliseconds limit)
{
    const auto end = std::chrono::steady_clock::now() + limit;
    while (!flag.load()) {
        if (std::chrono::steady_clock::now() >= end) return false;
        std::this_thread::yield();
    }
    return true;
}

}  // namespace

void* operator new(std::size_t n)
{
    const bool mine = tl_caller && g_armed.load();
    if (mine) {
        const long k = g_count.fetch_add(1) + 1;
        if (k == g_failAt.load()) {
            g_injected.store(true);
            (void)waitFor(g_task1Started, 5000ms);   // the first range must already be running when the launch fails
            throw std::bad_alloc();
        }
    }
    void* p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    if (mine && n == g_watchSize.load()) {
        void* expected = nullptr;
        g_watch.compare_exchange_strong(expected, p);
    }
    return p;
}
void* operator new[](std::size_t n) { return operator new(n); }
void operator delete(void* p) noexcept
{
    if (!p) return;
    if (p == g_watch.load() && !g_watchFreed.load()) {
        std::memset(p, 0, g_watchSize.load());   // kept, not freed: later writes stay visible
        g_watchFreed.store(true);
        return;
    }
    std::free(p);
}
void operator delete[](void* p) noexcept { operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { operator delete(p); }

namespace {

// Range [1, 2) is the first range started as a task. In inject mode it waits (bounded) for the slots to be released,
// then throws, so xpe_parallel_chunks writes its error slot.
void rangeBody(size_t b, size_t)
{
    if (b != 1) return;
    g_task1Started.store(true);
    if (!g_injectMode.load()) return;
    (void)waitFor(g_watchFreed, 300ms);
    throw std::runtime_error("range 1 failed");
}

long countAllocs(unsigned threads)
{
    g_injectMode.store(false);
    g_failAt.store(0);
    g_count.store(0);
    XpeParallelLimitGuard lim(threads);
    tl_caller = true;
    g_armed.store(true);
    xpe_parallel_chunks(threads, 1, rangeBody);
    g_armed.store(false);
    tl_caller = false;
    return g_count.load();
}

}  // namespace

TEST(ParallelUnwind, ARunningRangeFinishesBeforeItsErrorSlotIsFreedWhenALaterLaunchThrowsBadAlloc)
{
    if (std::thread::hardware_concurrency() < 3) GTEST_SKIP() << "needs 3 hardware threads";

    for (int i = 0; i < 3; ++i) {   // warm-up: the runtime's thread pool is created before anything is counted
        XpeParallelLimitGuard lim(3);
        xpe_parallel_chunks(3, 1, rangeBody);
    }

    // Calibration: with 2 ranges the caller allocates the slots, the task list and one launch; with 3, one more launch.
    // Equal launch costs mean allocation c2 + 1 is the first allocation of the second launch.
    const long c2 = countAllocs(2), c2b = countAllocs(2), c3 = countAllocs(3);
    std::printf("[calibration] caller-thread allocations: 2 ranges = %ld (repeat %ld), 3 ranges = %ld\n", c2, c2b, c3);
    std::fflush(stdout);
    ASSERT_EQ(c2, c2b) << "allocation count not repeatable";
    ASSERT_GE(c2, 3);
    ASSERT_EQ(c3 - c2, c2 - 2) << "a launch does not cost the same number of allocations each time";

    g_task1Started.store(false);
    g_injected.store(false);
    g_watch.store(nullptr);
    g_watchFreed.store(false);
    g_watchSize.store(3 * sizeof(std::exception_ptr));
    g_injectMode.store(true);
    g_count.store(0);
    g_failAt.store(c2 + 1);

    bool threwBadAlloc = false, threwOther = false;
    {
        XpeParallelLimitGuard lim(3);
        tl_caller = true;
        g_armed.store(true);
        try {
            xpe_parallel_chunks(3, 1, rangeBody);
        } catch (const std::bad_alloc&) {
            threwBadAlloc = true;
        } catch (...) {
            threwOther = true;
        }
        g_armed.store(false);
        tl_caller = false;
    }
    g_injectMode.store(false);
    std::this_thread::sleep_for(200ms);   // a write that lands late is still seen

    void* block = g_watch.load();
    bool clean = true;
    if (block && g_watchFreed.load()) {
        const unsigned char* bytes = static_cast<const unsigned char*>(block);
        for (size_t i = 0; i < g_watchSize.load(); ++i) clean = clean && bytes[i] == 0;
    }
    const bool freed = g_watchFreed.load();
    g_watch.store(nullptr);
    g_watchSize.store(0);
    if (block && freed) std::free(block);

    EXPECT_TRUE(g_injected.load()) << "the second launch's allocation was not reached";
    EXPECT_TRUE(g_task1Started.load()) << "the first range never ran";
    EXPECT_TRUE(threwBadAlloc) << "the launch failure did not propagate as std::bad_alloc (other exception: " << threwOther << ")";
    ASSERT_NE(block, nullptr) << "the error-slot block was not identified";
    EXPECT_TRUE(freed) << "the error-slot block was not released by the call";
    EXPECT_TRUE(clean) << "a running range wrote its error slot after the slots were freed (use after free)";
}
