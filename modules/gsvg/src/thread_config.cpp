/**
 * @file thread_config.cpp
 * @brief Process-wide thread-count request for the gsvg image passes (#179, QA-B-105).
 *
 * Its own translation unit because the module tests compile grid_dwt.cpp and
 * virtual_grid.cpp directly (they call internals that gsvg.dll does not
 * export), so the storage has to link into the test binary as well.
 */
#include <atomic>

namespace {
std::atomic<int> g_maxThreads{0};
}

// Reads the request. 0 means automatic (see xpe_parallel::ResolveThreads).
int XpeGsvgThreadRequest() { return g_maxThreads.load(std::memory_order_relaxed); }

// Stores it; negative is stored as 0. Called by xpe_gsvg_set_max_threads.
void XpeGsvgSetThreadRequest(int threads) {
    g_maxThreads.store(threads > 0 ? threads : 0, std::memory_order_relaxed);
}
