/**
 * @file heap_growth.h
 * @brief Retention measured on the CRT heap, for the endurance cases (#181).
 *
 * The same file exists in the ai, dicom, display, enhance_advanced,
 * enhance_basic and preprocess test directories (gsvg keeps its own copy of the method in
 * test_gsvg_abi_smoke.cpp): each module's tests build into
 * their own executable and share no test-support library.
 *
 * Why not the working set: QA-B-92 (#180) showed that the process working set
 * does not follow a leak. Over 250 / 1000 / 4000 cycles a deliberate 64-byte
 * leak grew the CRT heap by 250 / 994 / 4000 blocks while the working set moved
 * -41 KB .. +2.47 MB, and a leak-free module failed a 1 MB working-set bound in
 * 2 of 20 local runs (and on CI, 2.59 MB). The heap walk counts the blocks that
 * are still allocated, which is what a leak is.
 *
 * With UCRT (/MD) every module DLL and the test allocate from the same CRT
 * heap, so _heapwalk sees the module's allocations. Each endurance case is
 * paired with a control that leaks on purpose inside the same cycle; without
 * it a zero proves nothing.
 *
 * XPE_HEAP_CYCLES overrides the cycle count (local proportionality checks).
 */
#pragma once

#include <cstdlib>
#include <string>
#include <vector>

#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
#  include <malloc.h>
#endif

namespace heap_growth {

struct Use { long long bytes = 0; long long blocks = 0; };

struct Growth {
    Use heap;                  // after - before, in bytes and blocks
    long long workingSet = 0;  // logged only
    int cycles = 0;
};

constexpr int       kDefaultCycles = 1000;
constexpr int       kWarmup = 100;
// Retention bound: fewer than one leaked block per ten cycles and under 16 KB.
constexpr long long kMaxBytes = 16 * 1024;
inline long long MaxBlocks(int cycles) { return cycles / 10; }

#ifdef _WIN32
inline Use Snapshot() {
    _HEAPINFO hi{};
    hi._pentry = nullptr;
    Use u;
    while (_heapwalk(&hi) == _HEAPOK) {
        if (hi._useflag == _USEDENTRY) {
            u.bytes += static_cast<long long>(hi._size);
            ++u.blocks;
        }
    }
    return u;
}

inline long long WorkingSet() {
    PROCESS_MEMORY_COUNTERS pmc;
    return GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))
        ? static_cast<long long>(pmc.WorkingSetSize) : 0;
}

inline int Cycles() {
    char* v = nullptr;
    size_t len = 0;
    int n = kDefaultCycles;
    if (_dupenv_s(&v, &len, "XPE_HEAP_CYCLES") == 0 && v != nullptr) {
        const int parsed = std::atoi(v);
        if (parsed > 0) n = parsed;
    }
    std::free(v);
    return n;
}
#else
// The endurance cases skip off Windows; these keep them compiling.
inline Use Snapshot() { return {}; }
inline long long WorkingSet() { return 0; }
inline int Cycles() { return kDefaultCycles; }
#endif

/**
 * Warm-up, baseline, measured cycles, second snapshot. cycle(i) is the unit of
 * work. When leakBytes > 0, every measured cycle also allocates that many bytes
 * and keeps them until after the second snapshot (the control).
 */
template <class Cycle>
Growth Measure(Cycle&& cycle, size_t leakBytes = 0) {
    Growth g;
    g.cycles = Cycles();
    std::vector<void*> held;
    held.reserve(static_cast<size_t>(g.cycles));   // before the baseline
    for (int i = 0; i < kWarmup; ++i) cycle(i);
    const Use h0 = Snapshot();
    const long long w0 = WorkingSet();
    for (int i = 0; i < g.cycles; ++i) {
        cycle(i);
        if (leakBytes > 0) held.push_back(std::malloc(leakBytes));
    }
    const Use h1 = Snapshot();
    g.workingSet = WorkingSet() - w0;
    g.heap = {h1.bytes - h0.bytes, h1.blocks - h0.blocks};
    for (void* p : held) std::free(p);
    return g;
}

inline std::string Describe(const Growth& g) {
    return "heap growth " + std::to_string(g.heap.bytes) + " bytes / " +
           std::to_string(g.heap.blocks) + " blocks over " + std::to_string(g.cycles) +
           " cycles, working set " + std::to_string(g.workingSet) + " bytes (not asserted)";
}

}  // namespace heap_growth
