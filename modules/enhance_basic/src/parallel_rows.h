/**
 * @file parallel_rows.h
 * @brief Row-band parallelism for per-pixel passes (#179, QA-B-103).
 *
 * The same file exists in modules/enhance_advanced/src/detail: the two modules build into
 * separate DLLs and share no internal library (modules/common belongs to
 * another lane).
 *
 * The contract is what makes the output independent of the thread count: the
 * caller's body must compute each row from inputs no band writes, and must not
 * accumulate across rows. Every band then runs the same arithmetic on the same
 * values in the same order as the single-threaded loop, so the result is
 * bit-identical for 1, 2, ... N threads.
 *
 * Threads are created per call. At the sizes these passes run on (3072x3072,
 * >= 50 ms of work) the creation cost is under a millisecond in total; a pool
 * would add lifetime and shutdown questions for no measurable gain here.
 */
#pragma once

#include <algorithm>
#include <thread>
#include <vector>

namespace xpe_parallel {

/**
 * @brief Resolve a requested thread count against the machine.
 * @param requested 0: automatic; otherwise the number asked for.
 * @param rows Work items (image rows); never start more threads than rows.
 * @return Number of threads to run, at least 1.
 *
 * Automatic is half the logical processors, capped at 4. The cap is the
 * oversubscription guard: an XPE pipeline runs several modules in one process
 * and a GUI thread alongside them, so a default that takes the whole machine
 * would slow the other work down (QA-B-103 measured the gain flattening past 4
 * threads anyway). A caller that owns the machine asks for more explicitly.
 */
inline int ResolveThreads(int requested, int rows) {
    int n = requested;
    if (n <= 0) {
        const unsigned hw = std::thread::hardware_concurrency();
        n = (hw == 0) ? 1 : std::min(4, static_cast<int>(hw / 2));
    }
    n = std::max(1, n);
    return std::min(n, std::max(1, rows));
}

/**
 * @brief Run body(y0, y1) over contiguous row bands [y0, y1).
 * @param rows Total rows.
 * @param threads Thread count from ResolveThreads (1 runs inline).
 * @param body Callable invoked once per band; must be safe to run in parallel.
 */
template <class Body>
void ForRows(int rows, int threads, Body body) {
    if (rows <= 0) return;
    if (threads <= 1) { body(0, rows); return; }

    const int band = (rows + threads - 1) / threads;
    std::vector<std::thread> pool;
    pool.reserve(static_cast<size_t>(threads) - 1);
    for (int t = 1; t < threads; ++t) {
        const int y0 = std::min(rows, t * band);
        const int y1 = std::min(rows, y0 + band);
        if (y0 >= y1) break;
        pool.emplace_back([&body, y0, y1] { body(y0, y1); });
    }
    body(0, std::min(rows, band));
    for (std::thread& t : pool) t.join();
}

}  // namespace xpe_parallel
