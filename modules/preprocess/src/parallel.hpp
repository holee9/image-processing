/**
 * @file parallel.hpp
 * @brief A small fork-join helper for per-pixel work whose result does not depend on the order or the split (QA-A-296, Refs #278).
 *
 * xpe_parallel_chunks(items, minPerThread, fn): calls fn(begin, end) on disjoint ranges that together cover [0, items).
 * The ranges run on up to kMaxThreads threads (the caller's included); when there are too few items, or a thread cannot be
 * had, the work runs on the caller's thread. An exception thrown by fn on any thread is rethrown on the caller's after all
 * threads have finished. The split never changes a result: callers use it only where each item is computed from read-only data
 * and written to a place no other item writes.
 */
#pragma once

#include <algorithm>
#include <cstddef>
#include <exception>
#include <memory>
#include <system_error>
#include <thread>
#include <type_traits>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

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

// One range of xpe_parallel_chunks, run on another thread. Its destructor waits until the range has finished, so however the
// call ends -- a return or an exception -- no range is still running when the per-range error slots are freed.
// QA-A-320 (Refs #278): on Windows a range is a work item of the process thread pool (QA-A-315 measured 0.49 ms for an empty call
// that created and destroyed a std::thread per range, evidence 06). It replaces std::async: the MSVC runtime's std::async keeps a
// failure to schedule its task (a failed allocation in the scheduler) inside the task, rethrows it from wait() and again from the
// task state's noexcept destructor, which ends the process with std::terminate (QA-A-320 evidence 04-06). Here a work item that
// cannot be created runs on the caller's thread, and the one allocation a start makes (the task object) throws before anything starts.
template <class Range>
class XpeRangeTask {
public:
    explicit XpeRangeTask(const Range& range) : range_(range) {}
    XpeRangeTask(const XpeRangeTask&) = delete;
    XpeRangeTask& operator=(const XpeRangeTask&) = delete;
    ~XpeRangeTask() { wait(); }

    // Starts the range on another thread; false when no thread could be had (the caller then runs it).
    bool start()
    {
#ifdef _WIN32
        work_ = CreateThreadpoolWork(&XpeRangeTask::run, this, nullptr);
        if (work_ == nullptr) return false;
        SubmitThreadpoolWork(work_);
        return true;
#else
        try {
            thread_ = std::thread([this] { range_(); });
        } catch (const std::system_error&) {
            return false;
        }
        return true;
#endif
    }

    void wait() noexcept
    {
#ifdef _WIN32
        if (work_ != nullptr) {
            WaitForThreadpoolWorkCallbacks(work_, FALSE);
            CloseThreadpoolWork(work_);
            work_ = nullptr;
        }
#else
        if (thread_.joinable()) thread_.join();
#endif
    }

private:
    Range range_;
#ifdef _WIN32
    PTP_WORK work_ = nullptr;
    static void CALLBACK run(PTP_CALLBACK_INSTANCE /*instance*/, PVOID self, PTP_WORK /*work*/) { static_cast<XpeRangeTask*>(self)->range_(); }
#else
    std::thread thread_;
#endif
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
    // One range: never throws; an exception from fn is kept in the range's error slot.
    struct Range {
        std::remove_reference_t<Fn>* fn;
        std::exception_ptr* error;
        size_t begin, end;
        void operator()() const noexcept
        {
            try { (*fn)(begin, end); } catch (...) { *error = std::current_exception(); }
        }
    };
    // Reviewer PRE313-317 P1: `errors` is declared before `pool` so that it is destroyed after it. A task's destructor waits for its
    // range, so when an exception leaves this function (std::bad_alloc from allocating a task) every running range finishes
    // writing errors[t] before `errors` is freed.
    std::vector<std::exception_ptr> errors(threads);
    std::vector<std::unique_ptr<XpeRangeTask<Range>>> pool;
    pool.reserve(threads - 1);
    for (size_t t = 1; t < threads; ++t) {
        const size_t b = std::min(items, t * per), e = std::min(items, b + per);
        if (b >= e) break;
        const Range range{&fn, &errors[t], b, e};
        pool.push_back(std::make_unique<XpeRangeTask<Range>>(range));   // reserved: the push itself does not allocate
        if (!pool.back()->start()) range();                              // no thread to be had: this range runs here
    }
    Range{&fn, &errors[0], size_t{0}, std::min(items, per)}();
    for (auto& task : pool) task->wait();
    for (auto& ep : errors) if (ep) std::rethrow_exception(ep);
}
