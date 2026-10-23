/**
 * @file parallel.hpp
 * @brief A small fork-join helper for per-pixel work whose result does not depend on the order or the split (QA-A-296, Refs #278).
 *
 * xpe_parallel_chunks(items, minPerThread, fn): calls fn(begin, end) on disjoint ranges that together cover [0, items).
 * The ranges run on up to kMaxThreads threads (the caller's included); when there are too few items, or a thread cannot be
 * created, the work runs on the caller's thread. An exception thrown by fn on any thread is rethrown on the caller's after all
 * threads have finished. The split never changes a result: callers use it only where each item is computed from read-only data
 * and written to a place no other item writes.
 */
#pragma once

#include <algorithm>
#include <cstddef>
#include <exception>
#include <system_error>
#include <thread>
#include <vector>

constexpr unsigned XPE_PARALLEL_MAX_THREADS = 16;

// The most threads the calling thread's current call may use (0 = no limit beyond the hardware and XPE_PARALLEL_MAX_THREADS). The pipeline
// sets it for the duration of one call from its `maxThreads` key; the value is the caller's thread's, so concurrent calls do not share it.
inline unsigned& xpe_parallel_limit() { thread_local unsigned limit = 0; return limit; }
struct XpeParallelLimitGuard {
    unsigned saved;
    explicit XpeParallelLimitGuard(unsigned limit) : saved(xpe_parallel_limit()) { xpe_parallel_limit() = limit; }
    ~XpeParallelLimitGuard() { xpe_parallel_limit() = saved; }
    XpeParallelLimitGuard(const XpeParallelLimitGuard&) = delete;
    XpeParallelLimitGuard& operator=(const XpeParallelLimitGuard&) = delete;
};

template <class Fn>
void xpe_parallel_chunks(size_t items, size_t minPerThread, Fn&& fn)
{
    if (items == 0) return;
    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 1;
    if (xpe_parallel_limit() != 0 && xpe_parallel_limit() < hw) hw = xpe_parallel_limit();
    const size_t byWork = minPerThread == 0 ? items : items / minPerThread;
    const size_t threads = std::max<size_t>(1, std::min<size_t>({static_cast<size_t>(hw), static_cast<size_t>(XPE_PARALLEL_MAX_THREADS), byWork}));
    if (threads == 1) { fn(size_t{0}, items); return; }

    const size_t per = (items + threads - 1) / threads;
    std::vector<std::thread> pool;
    std::vector<std::exception_ptr> errors(threads);
    pool.reserve(threads - 1);
    for (size_t t = 1; t < threads; ++t) {
        const size_t b = std::min(items, t * per), e = std::min(items, b + per);
        if (b >= e) break;
        const auto body = [&fn, &errors, t, b, e]() {
            try { fn(b, e); } catch (...) { errors[t] = std::current_exception(); }
        };
        try {
            pool.emplace_back(body);
        } catch (const std::system_error&) {
            body();   // no thread to be had: this range runs here
        }
    }
    try { fn(size_t{0}, std::min(items, per)); } catch (...) { errors[0] = std::current_exception(); }
    for (auto& th : pool) th.join();
    for (auto& ep : errors) if (ep) std::rethrow_exception(ep);
}
