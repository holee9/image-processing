/**
 * @file test_oom_injection.cpp
 * @brief Allocation-failure injection for the calibration loaders (QA-A-200, #216).
 *
 * This executable compiles the product sources into itself (see CMakeLists.txt) instead of linking
 * the xpe_preprocess DLL. That is the point: the replacement `operator new` below only reaches code
 * built into this executable; the DLL binds to its own runtime's allocator, so the DLL-linked test
 * executable cannot make a product allocation fail.
 *
 * Method: for K = 1, 2, 3, ... make exactly the K-th allocation made during one call throw
 * std::bad_alloc, and check what the call did. The sweep ends when a call completes with fewer than
 * K allocations. Two things are checked at every K:
 *   - no exception leaves the C ABI function (the cached loaders and the plain loaders);
 *   - a plain loader that reports an error left the module-global calibration store exactly as it
 *     found it (no partial commit), and a cached hit that reports an error did too.
 * gtest macros are never evaluated while an injection is armed: they allocate through this same
 * operator new.
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"
#include "xcal_reader.hpp"
#include "rle_codec.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <mutex>
#include <new>
#include <string>
#include <vector>

/* =========================================================================
 * The injecting allocator
 * ========================================================================= */

namespace guard {
// QA-A-205: while the guard is on, every block gets 256 canary bytes behind it, recorded in a small table
// keyed by the block's address (no header, so blocks allocated by another module and freed here are
// unaffected). operator delete looks the block up and reports a canary that changed. Nothing here
// allocates.
constexpr size_t kCanary = 256;
constexpr unsigned char kFill = 0xA5;
constexpr size_t kSlots = 4096;
struct Slot { void* p; size_t n; };
Slot g_slots[kSlots];
std::atomic<bool> g_on{false};
std::atomic<long> g_recorded{0};     // blocks currently in the table
std::atomic<long> g_overruns{0};
void* const kTomb = reinterpret_cast<void*>(1);
size_t slotOf(const void* p) { return (reinterpret_cast<uintptr_t>(p) >> 4) % kSlots; }
void record(void* p, size_t n) {
    size_t i = slotOf(p);
    while (g_slots[i].p && g_slots[i].p != kTomb) i = (i + 1) % kSlots;
    g_slots[i] = Slot{p, n};
    g_recorded.fetch_add(1);
    std::memset(static_cast<unsigned char*>(p) + n, kFill, kCanary);
}
void check(void* p) {
    size_t i = slotOf(p);
    for (size_t probes = 0; probes < kSlots && g_slots[i].p; ++probes, i = (i + 1) % kSlots) {
        if (g_slots[i].p != p) continue;
        const auto* tail = static_cast<const unsigned char*>(p) + g_slots[i].n;
        for (size_t k = 0; k < kCanary; ++k) {
            if (tail[k] != kFill) { g_overruns.fetch_add(1); break; }
        }
        g_slots[i].p = kTomb;
        g_recorded.fetch_sub(1);
        return;
    }
}
void on(bool v) { g_on.store(v); }
void reset() { g_overruns.store(0); }
long overruns() { return g_overruns.load(); }
}  // namespace guard

#ifdef _WIN32
namespace pageguard {
// QA-A-208 (Codex #34): a block placed so that its END is the end of committed pages, followed by a PAGE_NOACCESS
// page. A read or a write past the end of the block -- even by one 16-byte step -- is an access violation, so an
// over-read, which the canary scheme above cannot see (it only notices bytes that were WRITTEN), is observable.
// Blocks of at least kMinBytes bytes made while the mode is on are page blocks; they are found again by address
// in a small table. Nothing here allocates through operator new.
constexpr size_t kPage = 4096;
constexpr size_t kMinBytes = 16;
constexpr size_t kSlots = 2048;
struct Slot { void* user; void* base; };
Slot g_slots[kSlots];
std::atomic<bool> g_on{false};
std::atomic<long> g_live{0};
void on(bool v) { g_on.store(v); }
void* allocate(size_t n) noexcept {
    const size_t body = (n + kPage - 1) / kPage * kPage;
    auto* base = static_cast<unsigned char*>(VirtualAlloc(nullptr, body + kPage, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!base) return nullptr;
    DWORD old = 0;
    if (!VirtualProtect(base + body, kPage, PAGE_NOACCESS, &old)) { VirtualFree(base, 0, MEM_RELEASE); return nullptr; }
    void* user = base + body - (n + 15) / 16 * 16;
    for (size_t i = 0; i < kSlots; ++i) {
        if (!g_slots[i].user) { g_slots[i] = Slot{user, base}; g_live.fetch_add(1); return user; }
    }
    VirtualFree(base, 0, MEM_RELEASE);
    return nullptr;
}
bool release(void* p) noexcept {
    for (size_t i = 0; i < kSlots; ++i) {
        if (g_slots[i].user == p) {
            void* base = g_slots[i].base;
            g_slots[i] = Slot{};
            g_live.fetch_sub(1);
            VirtualFree(base, 0, MEM_RELEASE);
            return true;
        }
    }
    return false;
}
/** Frees what a call that faulted never released; returns how many blocks that was. */
long reset() noexcept {
    long n = 0;
    for (size_t i = 0; i < kSlots; ++i) {
        if (g_slots[i].user) { VirtualFree(g_slots[i].base, 0, MEM_RELEASE); g_slots[i] = Slot{}; ++n; }
    }
    g_live.store(0);
    return n;
}
}  // namespace pageguard
#endif

namespace {
std::atomic<long> g_failAt{0};      // 0 = disarmed; otherwise the 1-based index of the allocation to fail
std::atomic<long> g_count{0};
std::atomic<bool> g_injected{false};
std::atomic<size_t> g_maxAlloc{0};  // largest single request made while armed (QA-A-202)
std::atomic<long> g_live{0};        // operator-new blocks currently alive (leak accounting, QA-A-203)

void arm(long k) {
    g_count.store(0);
    g_maxAlloc.store(0);
    g_injected.store(false);
    g_failAt.store(k);
}
/** Disarms and reports whether the K-th allocation was reached (and so failed). */
bool disarm() {
    g_failAt.store(0);
    return g_injected.load();
}
}  // namespace

void* operator new(std::size_t n) {
    const long limit = g_failAt.load(std::memory_order_relaxed);
    if (limit > 0) {
        size_t cur = g_maxAlloc.load();
        while (n > cur && !g_maxAlloc.compare_exchange_weak(cur, n)) {}
    }
    if (limit > 0 && g_count.fetch_add(1) + 1 == limit) {
        g_injected.store(true);
        throw std::bad_alloc();
    }
    const bool guarded = guard::g_on.load(std::memory_order_relaxed);
    const size_t want = n ? n : 1;
#ifdef _WIN32
    if (pageguard::g_on.load(std::memory_order_relaxed) && want >= pageguard::kMinBytes) {
        if (void* pg = pageguard::allocate(want)) { g_live.fetch_add(1); return pg; }
    }
#endif
    if (void* p = std::malloc(want + (guarded ? guard::kCanary : 0))) {
        g_live.fetch_add(1);
        if (guarded) guard::record(p, want);
        return p;
    }
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return operator new(n); }
void operator delete(void* p) noexcept {
    if (p) {
#ifdef _WIN32
        if (pageguard::g_live.load(std::memory_order_relaxed) > 0 && pageguard::release(p)) {
            g_live.fetch_sub(1);
            return;
        }
#endif
        if (guard::g_recorded.load(std::memory_order_relaxed) > 0) guard::check(p);
        g_live.fetch_sub(1);
        std::free(p);
    }
}
void operator delete[](void* p) noexcept { operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { operator delete(p); }

/* =========================================================================
 * Fixtures
 * ========================================================================= */

namespace {

constexpr uint32_t W = 4, H = 4;
constexpr size_t N = static_cast<size_t>(W) * H;

// A config block long enough that copying it allocates (std::string keeps up to 15 characters inline).
const std::string kGainJson =
    "{\"fit_r_squared\":\"0.900000\",\"polynomial_degree\":\"2\","
    "\"note\":\"padding so that this config block does not fit in a small string: "
    "0123456789012345678901234567890123456789012345678901234567890123456789\"}";

void writeFile(const char* path, uint32_t type, uint32_t fmt, const void* data, size_t bytes,
               const std::string& json, int64_t expiryMs = 0) {
    std::remove(path);
    XCalFileHeader hdr{};
    hdr.expiry_epoch_ms = expiryMs;
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION; hdr.type = type; hdr.pixel_format = fmt;
    hdr.width = W; hdr.height = H; hdr.payload_len = bytes;
    ASSERT_EQ(XPE_OK, write_xcal_file(path, hdr, reinterpret_cast<const uint8_t*>(json.data()), json.size(),
                                      static_cast<const uint8_t*>(data), bytes));
}
void writeOffset(const char* path, float v, int64_t expiryMs = 0) {
    std::vector<float> m(N, v);
    writeFile(path, XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32, m.data(), m.size() * sizeof(float), "{}", expiryMs);
}
void writeGain(const char* path, float v, const std::string& json, int64_t expiryMs = 0) {
    std::vector<float> m(N, v);
    writeFile(path, XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32, m.data(), m.size() * sizeof(float), json, expiryMs);
}
// Polynomial gain file (degree 1: two coefficient planes, pixel-major). The config block carries a dose
// range so the loader reads it, and is long enough that copying it allocates.
void writeGainPoly(const char* path, const std::string& json) {
    std::vector<float> coeffs(N * 2);
    for (size_t p = 0; p < N; ++p) { coeffs[p * 2] = 1.0f; coeffs[p * 2 + 1] = 0.0f; }
    writeFile(path, XCAL_TYPE_GAIN_POLY, XCAL_FMT_FLOAT32, coeffs.data(), coeffs.size() * sizeof(float), json);
}
const std::string kPolyJson =
    "{\"dose_min\":10.0,\"dose_max\":100.0,\"note\":\"padding so that this config block does not fit in a "
    "small string: 0123456789012345678901234567890123456789012345678901234567890123456789\"}";
const std::string kPolyJsonNoRange =
    "{\"note\":\"a polynomial file written before the range field existed, padded past the small-string "
    "size: 0123456789012345678901234567890123456789012345678901234567890123456789\"}";

void writeDefect(const char* path, bool flagged) {
    std::vector<uint8_t> m(N, 0);
    if (flagged) m[5] = 1;
    writeFile(path, XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, m.data(), m.size(), "{}");
}

/** Everything the loaders write, reduced to comparable values. */
struct Snap {
    const void* om; uint32_t ow, oh; int64_t ot, oe;
    const void* gm; const void* gp; const void* gdi; uint32_t gdc; uint32_t gnc, gw, gh; int64_t gt, ge; bool ghq; double gqr; unsigned gqd;
    const void* dm; uint32_t dw, dh; int64_t de;
    bool operator==(const Snap& o) const {
        return om == o.om && ow == o.ow && oh == o.oh && ot == o.ot && oe == o.oe &&
               gm == o.gm && gp == o.gp && gdi == o.gdi && gdc == o.gdc && gnc == o.gnc && gw == o.gw && gh == o.gh && gt == o.gt &&
               ge == o.ge && ghq == o.ghq && gqr == o.gqr && gqd == o.gqd && dm == o.dm && dw == o.dw && dh == o.dh && de == o.de;
    }
};
Snap snap() {
    std::lock_guard<std::mutex> lk(g_calib_mutex);
    return Snap{g_calib.offset_map.get(), g_calib.offset_width, g_calib.offset_height, g_calib.offset_timestamp,
                g_calib.offset_expiry_ms,
                g_calib.gain_map.get(), g_calib.gain_poly_coeffs.get(), g_calib.gain_defect_idx.get(), g_calib.gain_defect_count,
                g_calib.gain_poly_num_coeffs,
                g_calib.gain_width, g_calib.gain_height, g_calib.gain_timestamp, g_calib.gain_expiry_ms,
                g_calib.gain_has_quality, g_calib.gain_quality.r_squared,
                static_cast<unsigned>(g_calib.gain_quality.polynomial_degree),
                g_calib.defect_map.get(), g_calib.defect_width, g_calib.defect_height, g_calib.defect_expiry_ms};
}
void resetStore() {
    std::lock_guard<std::mutex> lk(g_calib_mutex);
    g_calib = CalibrationData{};
}

class OomInjection : public ::testing::Test {
protected:
    void SetUp() override { resetStore(); xpe_calib_cache_clear(); }
    void TearDown() override {
        xpe_calib_cache_clear();
        resetStore();
        xpe_clear_alerts();
        xpe_preprocess_shutdown();
        for (const char* p : {"oom_long_entry_file_name.xcal", "oom_q.xcal"}) std::remove(p);
    }
};

using Setup = std::function<void()>;
using Call = std::function<XpeErrorCode()>;

/**
 * Fails the K-th allocation of `call` for K = 1.. until a call finishes with fewer allocations.
 * `unchangedOnError`: a call that returns an error must leave the store as the setup left it.
 *
 * Every call, failed or not, is also held to two invariants (QA-A-203, Codex #21): the calibration
 * cache's list and index still describe the same entries, and no block is left behind -- the live
 * operator-new blocks, counted after the store and the cache are emptied, equal what they were after the
 * same emptying before the call. The cache's pixel buffers come from operator new for this reason.
 */
void sweep(const char* label, const Setup& setup, const Call& call, bool unchangedOnError,
           const std::function<std::string(XpeErrorCode rc)>& extra = nullptr) {
    constexpr long kMax = 3000;
    long injections = 0;
    for (long k = 1; k <= kMax; ++k) {
        setup();
        resetStore(); xpe_calib_cache_clear(); xpe_clear_alerts();
        const long liveBase = g_live.load();
        setup();
        const Snap before = snap();
        bool escaped = false;
        XpeErrorCode rc = XPE_OK;
        arm(k);
        try {
            rc = call();
        } catch (...) {
            escaped = true;
        }
        const bool injected = disarm();

        // An exception that crosses an extern "C" function is not unwound reliably: MSVC /EHsc treats
        // such a function as non-throwing and does not run its locals' destructors, so a lock_guard
        // in the function stays locked. Detect that, and release the lock so the sweep can report it
        // instead of failing every later test with "resource deadlock would occur".
        bool lockLeaked = false;
        if (g_calib_mutex.try_lock()) {
            g_calib_mutex.unlock();
        } else {
            lockLeaked = true;
            g_calib_mutex.unlock();
        }
        const Snap after = snap();
        // An extra, call-specific check, made while the store still holds what the call left in it.
        const std::string extraWhy = extra ? extra(rc) : std::string();
        const bool cacheConsistent = xpe_calib_cache_is_consistent();
        resetStore(); xpe_calib_cache_clear(); xpe_clear_alerts();
        const long liveAfter = g_live.load();

        ASSERT_FALSE(escaped) << label << ": an exception left the C ABI function when allocation #" << k
                              << " failed" << (lockLeaked ? " (and the calibration store lock stayed held)" : "");
        ASSERT_FALSE(lockLeaked) << label << ": the calibration store lock stayed held after allocation #" << k << " failed";
        ASSERT_TRUE(extraWhy.empty()) << label << ": allocation #" << k << " failed (rc " << rc << "): " << extraWhy;
        ASSERT_TRUE(cacheConsistent) << label << ": allocation #" << k << " failed (rc " << rc
                                     << ") and the cache's list and index no longer agree";
        ASSERT_EQ(liveBase, liveAfter) << label << ": allocation #" << k << " failed (rc " << rc
                                       << ") and " << (liveAfter - liveBase) << " block(s) were left behind";
        if (!injected) {
            EXPECT_EQ(XPE_OK, rc) << label << ": the call finished without reaching allocation #" << k;
            ASSERT_GT(injections, 0) << label << ": the sweep never reached a product allocation";
            std::printf("[sweep] %-44s %3ld allocation points covered\n", label, injections);
            return;
        }
        ++injections;
        if (unchangedOnError && rc != XPE_OK) {
            ASSERT_TRUE(before == after)
                << label << ": allocation #" << k << " failed, the call returned " << rc
                << " but the calibration store changed (partial commit)";
        }
    }
    FAIL() << label << ": the sweep did not finish within " << kMax << " allocations";
}

}  // namespace

/* =========================================================================
 * Control: the harness reaches product code
 * ========================================================================= */

TEST_F(OomInjection, TheInjectionReachesAnAllocationMadeInsideAProductLoader) {
    writeOffset("oom_long_entry_file_name.xcal", 100.0f);
    arm(1);
    XpeErrorCode rc = XPE_OK;
    bool escaped = false;
    try {
        rc = xpe_calib_load_offset("oom_long_entry_file_name.xcal");
    } catch (...) {
        escaped = true;
    }
    const bool injected = disarm();
    EXPECT_TRUE(injected) << "control: the first allocation inside the loader must be the injected one";
    EXPECT_FALSE(escaped) << "the plain loader answers an allocation failure with a code";
    EXPECT_NE(XPE_OK, rc) << "control: a failed first allocation must fail the load";
}

/* =========================================================================
 * Plain loaders: an error leaves the store untouched
 * ========================================================================= */

TEST_F(OomInjection, APlainGainLoadThatFailsLeavesTheStoreUntouched) {
    writeGain("oom_q.xcal", 4.0f, "{}");
    writeGain("oom_long_entry_file_name.xcal", 2.0f, kGainJson);
    sweep("xpe_calib_load_gain",
          [] { resetStore(); xpe_calib_cache_clear(); ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_q.xcal")); },
          [] { return xpe_calib_load_gain("oom_long_entry_file_name.xcal"); }, /*unchangedOnError=*/true);
}

TEST_F(OomInjection, APlainOffsetLoadThatFailsLeavesTheStoreUntouched) {
    writeOffset("oom_q.xcal", 300.0f);
    writeOffset("oom_long_entry_file_name.xcal", 100.0f);
    sweep("xpe_calib_load_offset",
          [] { resetStore(); ASSERT_EQ(XPE_OK, xpe_calib_load_offset("oom_q.xcal")); },
          [] { return xpe_calib_load_offset("oom_long_entry_file_name.xcal"); }, true);
}

TEST_F(OomInjection, APlainDefectLoadThatFailsLeavesTheStoreUntouched) {
    writeDefect("oom_q.xcal", false);
    writeDefect("oom_long_entry_file_name.xcal", true);
    sweep("xpe_calib_load_defect_map",
          [] { resetStore(); ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map("oom_q.xcal")); },
          [] { return xpe_calib_load_defect_map("oom_long_entry_file_name.xcal"); }, true);
}

TEST_F(OomInjection, APlainPolynomialGainLoadWithADoseRangeThatFailsLeavesTheStoreUntouched) {
    writeGain("oom_q.xcal", 4.0f, "{}");
    writeGainPoly("oom_long_entry_file_name.xcal", kPolyJson);
    sweep("xpe_calib_load_gain (polynomial, range)",
          [] { resetStore(); xpe_clear_alerts(); ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_q.xcal")); },
          [] { return xpe_calib_load_gain("oom_long_entry_file_name.xcal"); }, true);
}

TEST_F(OomInjection, APlainPolynomialGainLoadWithoutADoseRangeThatFailsLeavesTheStoreUntouched) {
    writeGain("oom_q.xcal", 4.0f, "{}");
    writeGainPoly("oom_long_entry_file_name.xcal", kPolyJsonNoRange);
    sweep("xpe_calib_load_gain (polynomial, no range)",
          [] { resetStore(); xpe_clear_alerts(); ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_q.xcal")); },
          [] { return xpe_calib_load_gain("oom_long_entry_file_name.xcal"); }, true);
}

/* =========================================================================
 * Cached loaders: no exception leaves the C ABI; a failed hit leaves the store untouched
 * ========================================================================= */

TEST_F(OomInjection, ACachedGainMissLeavesNoExceptionEscaping) {
    writeGain("oom_q.xcal", 4.0f, "{}");
    writeGain("oom_long_entry_file_name.xcal", 2.0f, kGainJson);
    sweep("xpe_calib_load_gain_cached (miss)",
          [] { resetStore(); xpe_calib_cache_clear(); ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_q.xcal")); },
          [] { XpeImageBuffer v{}; return xpe_calib_load_gain_cached("oom_long_entry_file_name.xcal", &v); }, false);
}

TEST_F(OomInjection, ACachedPolynomialGainLoadLeavesNoExceptionEscaping) {
    writeGain("oom_q.xcal", 4.0f, "{}");
    writeGainPoly("oom_long_entry_file_name.xcal", kPolyJson);
    sweep("xpe_calib_load_gain_cached (polynomial)",
          [] { resetStore(); xpe_clear_alerts(); xpe_calib_cache_clear(); ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_q.xcal")); },
          [] { XpeImageBuffer v{}; return xpe_calib_load_gain_cached("oom_long_entry_file_name.xcal", &v); }, false);
}

TEST_F(OomInjection, ACachedOffsetMissLeavesNoExceptionEscaping) {
    writeOffset("oom_q.xcal", 300.0f);
    writeOffset("oom_long_entry_file_name.xcal", 100.0f);
    sweep("xpe_calib_load_offset_cached (miss)",
          [] { resetStore(); xpe_calib_cache_clear(); ASSERT_EQ(XPE_OK, xpe_calib_load_offset("oom_q.xcal")); },
          [] { XpeImageBuffer v{}; return xpe_calib_load_offset_cached("oom_long_entry_file_name.xcal", &v); }, false);
}

TEST_F(OomInjection, ACachedDefectMissLeavesNoExceptionEscaping) {
    writeDefect("oom_q.xcal", false);
    writeDefect("oom_long_entry_file_name.xcal", true);
    sweep("xpe_calib_load_defect_cached (miss)",
          [] { resetStore(); xpe_calib_cache_clear(); ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map("oom_q.xcal")); },
          [] { XpeImageBuffer v{}; return xpe_calib_load_defect_cached("oom_long_entry_file_name.xcal", &v); }, false);
}

TEST_F(OomInjection, ACachedGainHitThatFailsLeavesTheStoreUntouched) {
    writeGain("oom_q.xcal", 4.0f, "{}");
    writeGain("oom_long_entry_file_name.xcal", 2.0f, kGainJson);
    sweep("xpe_calib_load_gain_cached (hit)",
          [] {
              resetStore(); xpe_calib_cache_clear();
              XpeImageBuffer v{};
              ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached("oom_long_entry_file_name.xcal", &v));      // cached
              ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_q.xcal"));                 // store = Q
          },
          [] { XpeImageBuffer v{}; return xpe_calib_load_gain_cached("oom_long_entry_file_name.xcal", &v); }, true);
}

TEST_F(OomInjection, ACachedOffsetHitThatFailsLeavesTheStoreUntouched) {
    writeOffset("oom_q.xcal", 300.0f);
    writeOffset("oom_long_entry_file_name.xcal", 100.0f);
    sweep("xpe_calib_load_offset_cached (hit)",
          [] {
              resetStore(); xpe_calib_cache_clear();
              XpeImageBuffer v{};
              ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("oom_long_entry_file_name.xcal", &v));
              ASSERT_EQ(XPE_OK, xpe_calib_load_offset("oom_q.xcal"));
          },
          [] { XpeImageBuffer v{}; return xpe_calib_load_offset_cached("oom_long_entry_file_name.xcal", &v); }, true);
}

TEST_F(OomInjection, ACachedDefectHitThatFailsLeavesTheStoreUntouched) {
    writeDefect("oom_q.xcal", false);
    writeDefect("oom_long_entry_file_name.xcal", true);
    sweep("xpe_calib_load_defect_cached (hit)",
          [] {
              resetStore(); xpe_calib_cache_clear();
              XpeImageBuffer v{};
              ASSERT_EQ(XPE_OK, xpe_calib_load_defect_cached("oom_long_entry_file_name.xcal", &v));
              ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map("oom_q.xcal"));
          },
          [] { XpeImageBuffer v{}; return xpe_calib_load_defect_cached("oom_long_entry_file_name.xcal", &v); }, true);
}

/* =========================================================================
 * QA-A-202 (#233): the two corrections that held the calibration lock while copying a whole map
 * ========================================================================= */

// The offset correction takes shared ownership of the map and reads it in place: a frame allocates nothing,
// so there is nothing left in it to fail (it used to copy the whole map on every frame, under the lock).
TEST_F(OomInjection, AnOffsetCorrectionAllocatesNothing) {
    writeOffset("oom_q.xcal", 300.0f);
    xpe_preprocess_init(nullptr);
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset("oom_q.xcal"));
    std::vector<uint16_t> in(N, 1000), out(N, 0);
    XpeImageBuffer i{}, o{};
    i.data = in.data(); i.width = W; i.height = H; i.bitsAllocated = 16; i.bitsStored = 16; i.format = XPE_PIXEL_UINT16;
    i.dataSize = static_cast<uint32_t>(N * 2);
    o = i;
    o.data = out.data();
    XpeImageMetadata meta{};

    arm(1000000000L);                       // counts allocations, never fails one
    XpeErrorCode rc = XPE_OK;
    try { rc = std::function<XpeErrorCode()>([&] { return xpe_offset_correct(&i, &o, &meta); })(); } catch (...) { rc = XPE_ERR_INTERNAL; }
    const long allocations = g_count.load();
    disarm();
    ASSERT_EQ(XPE_OK, rc);
    EXPECT_EQ(700u, out[0]) << "control: the correction ran (1000 - 300)";
    EXPECT_EQ(0, allocations) << "a frame must not allocate (a map copy per frame is the defect this pins)";
}

TEST_F(OomInjection, ADefectCorrectionThatFailsLeavesTheStoreUntouchedAndTheLockFree) {
    writeDefect("oom_q.xcal", true);        // flags pixel 5, so the clustering buffers are needed
    static std::vector<float> in(N, 1000.0f), out(N, 0.0f);
    in[5] = 5000.0f;
    sweep("xpe_defect_correct",
          [] {
              resetStore(); xpe_clear_alerts();
              xpe_preprocess_init(nullptr);
              ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map("oom_q.xcal"));
          },
          [] {
              XpeImageBuffer i{}, o{};
              i.data = in.data(); i.width = W; i.height = H; i.bitsAllocated = 32; i.bitsStored = 32; i.format = XPE_PIXEL_FLOAT32;
              i.dataSize = static_cast<uint32_t>(N * 4);
              o = i;
              o.data = out.data();
              XpeImageMetadata meta{};
              return xpe_defect_correct(&i, &o, &meta);
          }, /*unchangedOnError=*/true);
}

// A malformed number in a ghost configuration used to throw after the handle and its two history buffers had
// been allocated, and nothing freed them.
TEST_F(OomInjection, AGhostCreationThatIsRefusedLeavesNoBlocksBehind) {
    for (const char* cfg : {"{\"alpha1\":\"abc\"}", "{\"tier\":\"x\"}", "{\"tau2\":\"1e999\"}"}) {
        void* handle = nullptr;
        const long before = g_live.load();
        XpeErrorCode rc = XPE_OK;
        bool threw = false;
        try { rc = std::function<XpeErrorCode()>([&] { return xpe_ghost_create(W, H, cfg, &handle); })(); } catch (...) { threw = true; }
        const long after = g_live.load();
        EXPECT_FALSE(threw) << cfg;
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, rc) << cfg;
        EXPECT_EQ(nullptr, handle) << cfg;
        EXPECT_EQ(before, after) << cfg << ": blocks left behind by a refused creation";
    }
}

// The defect correction takes shared ownership of the map and reads it in place: no request in a frame is as
// large as the map (it used to copy the whole map, under the lock). The frame is 256x256, so a copy of the
// map is a 65536-byte request, while the clustering bit-sets are 8 KiB.
TEST_F(OomInjection, ADefectCorrectionDoesNotCopyTheMap) {
    constexpr uint32_t w = 256, h = 256;
    constexpr size_t n = static_cast<size_t>(w) * h;
    xpe_preprocess_init(nullptr);
    {
        std::lock_guard<std::mutex> lk(g_calib_mutex);
        g_calib.defect_map.reset(new uint8_t[n]());
        g_calib.defect_map.get()[5 * w + 5] = 1;
        g_calib.defect_width = w;
        g_calib.defect_height = h;
    }
    std::vector<float> in(n, 1000.0f), out(n, 0.0f);
    in[5 * w + 5] = 5000.0f;
    XpeImageBuffer i{}, o{};
    i.data = in.data(); i.width = w; i.height = h; i.bitsAllocated = 32; i.bitsStored = 32; i.format = XPE_PIXEL_FLOAT32;
    i.dataSize = static_cast<uint32_t>(n * 4);
    o = i;
    o.data = out.data();
    XpeImageMetadata meta{};

    arm(1000000000L);
    XpeErrorCode rc = XPE_OK;
    try { rc = std::function<XpeErrorCode()>([&] { return xpe_defect_correct(&i, &o, &meta); })(); } catch (...) { rc = XPE_ERR_INTERNAL; }
    const size_t largest = g_maxAlloc.load();
    disarm();
    ASSERT_EQ(XPE_OK, rc);
    EXPECT_NEAR(1000.0f, out[5 * w + 5], 1.0f) << "control: the flagged pixel was repaired";
    EXPECT_GT(largest, 0u) << "control: the frame made allocations the counter could see";
    EXPECT_LT(largest, n) << "a request as large as the map means the map was copied";
}

/* =========================================================================
 * Hit timing (QA-A-203b, Codex #22): the expiry is judged AFTER the file was opened
 * ========================================================================= */

// The plain reader opens and reads the file and only then looks at the clock. A hit that took the time
// before it opened the file would let an entry that expired while the open was slow (a network path)
// through and install it. The test makes the open check take longer than the entry has left.
TEST_F(OomInjection, AHitJudgesTheExpiryAfterTheOpenCheckNotBefore) {
    struct Case { const char* name; std::function<void(int64_t)> write; std::function<XpeErrorCode()> cached; };
    const Case cases[] = {
        {"offset", [](int64_t e) { writeOffset("oom_q.xcal", 100.0f, e); },
         [] { XpeImageBuffer v{}; return xpe_calib_load_offset_cached("oom_q.xcal", &v); }},
        {"gain",   [](int64_t e) { writeGain("oom_q.xcal", 2.0f, "{}", e); },
         [] { XpeImageBuffer v{}; return xpe_calib_load_gain_cached("oom_q.xcal", &v); }},
        {"defect", [](int64_t e) {
             std::vector<uint8_t> m(N, 0);
             writeFile("oom_q.xcal", XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, m.data(), m.size(), "{}", e); },
         [] { XpeImageBuffer v{}; return xpe_calib_load_defect_cached("oom_q.xcal", &v); }},
    };
    for (const Case& k : cases) {
        SCOPED_TRACE(k.name);
        xpe_calib_cache_clear();
        resetStore();
        xpe_cache_after_open_check_hook = nullptr;
        const int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        k.write(nowMs + 600);
        ASSERT_EQ(XPE_OK, k.cached()) << "the first call loads and caches the file";
        // Control: with a fast open check, an immediate second call is a hit and the entry is still valid.
        ASSERT_EQ(XPE_OK, k.cached()) << "control: the entry is still valid right after the load";

        // The open check now takes longer than the entry has left (600 ms of expiry, 900 ms of delay).
        xpe_cache_after_open_check_hook = [] { std::this_thread::sleep_for(std::chrono::milliseconds(900)); };
        const XpeErrorCode rc = k.cached();
        xpe_cache_after_open_check_hook = nullptr;
        EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED, rc) << "the clock must be read after the open check, not before it";
    }
}

/* =========================================================================
 * The three pipeline entry points (QA-A-202b, Codex #23)
 * ========================================================================= */

namespace pipe {

using Call_t = std::function<XpeErrorCode()>;
constexpr int kFrames = 2;

std::vector<uint8_t> g_bytes[kFrames];           // each frame's buffer: N uint16 pixels in room for N floats
XpeImageBuffer g_img[kFrames];
XpeImageMetadata g_meta[kFrames];
std::vector<uint8_t> g_bytes0[kFrames];          // as the setup left them
XpeImageMetadata g_meta0[kFrames];
uint64_t g_digest0 = 0;
void* g_ghost = nullptr;
std::vector<float> g_hist1_0, g_hist2_0;

// Long enough that reading it allocates (std::string keeps up to 15 characters inline), and every stage on.
const char* const kConfig =
    "{\"detectorTempC\":\"25.5\",\"binningMode\":\"2\",\"note\":\"padding so that this configuration does "
    "not fit in a small string: 0123456789012345678901234567890123456789012345678901234567890123456789\"}";

uint64_t storeDigest() {
    std::lock_guard<std::mutex> lk(g_calib_mutex);
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h](const void* p, size_t n) {
        const auto* b = static_cast<const unsigned char*>(p);
        for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
    };
    mix(&g_calib.offset_width, sizeof g_calib.offset_width);
    mix(&g_calib.gain_width, sizeof g_calib.gain_width);
    mix(&g_calib.defect_width, sizeof g_calib.defect_width);
    if (g_calib.offset_map) mix(g_calib.offset_map.get(), N * sizeof(float));
    if (g_calib.gain_map) mix(g_calib.gain_map.get(), N * sizeof(float));
    if (g_calib.defect_map) mix(g_calib.defect_map.get(), N);
    return h;
}

bool sameMeta(const XpeImageMetadata& a, const XpeImageMetadata& b) { return std::memcmp(&a, &b, sizeof a) == 0; }

void writeCalibDir() {
    std::filesystem::create_directories("oom_pipe_calib");
    writeOffset("oom_pipe_calib/offset.xcal", 100.0f);
    writeGain("oom_pipe_calib/gain.xcal", 2.0f, "{}");
    writeDefect("oom_pipe_calib/defect.xcal", true);
}

/** The module initialized, the three maps in the store, a fresh ghost handle, and the frames to process. */
void setup() {
    resetStore();
    xpe_calib_cache_clear();
    xpe_clear_alerts();
    // Shutdown first: it clears every module global, including the quality metadata a gain load leaves in
    // the module (resetStore() only empties the store), so each scenario starts from the same state.
    xpe_preprocess_shutdown();
    xpe_preprocess_init(nullptr);
    xpe_calib_load_offset("oom_pipe_calib/offset.xcal");
    xpe_calib_load_gain("oom_pipe_calib/gain.xcal");
    xpe_calib_load_defect_map("oom_pipe_calib/defect.xcal");
    if (g_ghost) xpe_ghost_destroy(g_ghost);
    g_ghost = nullptr;
    xpe_ghost_create(W, H, nullptr, &g_ghost);
    for (int f = 0; f < kFrames; ++f) {
        g_bytes[f].assign(N * sizeof(float), 0);
        auto* px = reinterpret_cast<uint16_t*>(g_bytes[f].data());
        for (size_t i = 0; i < N; ++i) px[i] = static_cast<uint16_t>(1000 + f * 50 + (i * 37) % 300);
        g_img[f] = XpeImageBuffer{};
        g_img[f].data = g_bytes[f].data();
        g_img[f].width = W; g_img[f].height = H;
        g_img[f].bitsAllocated = 16; g_img[f].bitsStored = 16;
        g_img[f].format = XPE_PIXEL_UINT16;
        // dataSize is the room the buffer has, which the pipeline writes its float32 result into: N floats.
        // (This was N uint16 -- a claim smaller than the result that the pipeline wrongly accepted and
        // truncated to; QA-A-205b, Codex #29 A1. The claim is the buffer's real size, so the buffer is not lied about.)
        g_img[f].dataSize = N * sizeof(float);
        g_meta[f] = XpeImageMetadata{};
        g_bytes0[f] = g_bytes[f];
        g_meta0[f] = g_meta[f];
    }
    g_digest0 = storeDigest();
    auto* gh = static_cast<GhostCorrectorHandle*>(g_ghost);
    g_hist1_0 = gh->hist1;
    g_hist2_0 = gh->hist2;
}

/** The reference: what each frame looks like after a clean run. */
struct Reference { std::vector<uint8_t> bytes[kFrames]; XpeImageMetadata meta[kFrames]; };
Reference g_ref;

void captureReference(const Call_t& run, int frames, bool withGhost = true) {
    setup();
    ASSERT_EQ(XPE_OK, run()) << "control: a clean run of the entry point succeeds";
    for (int f = 0; f < frames; ++f) {
        g_ref.bytes[f] = g_bytes[f];
        g_ref.meta[f] = g_meta[f];
        EXPECT_NE(g_bytes0[f], g_bytes[f]) << "control: the clean run changed frame " << f;
        EXPECT_NE(0u, g_meta[f].flags & XPE_FLAG_OFFSET_CORRECTED) << "control: the offset stage ran";
        EXPECT_NE(0u, g_meta[f].flags & XPE_FLAG_GAIN_CORRECTED) << "control: the gain stage ran";
        EXPECT_NE(0u, g_meta[f].flags & XPE_FLAG_DEFECT_CORRECTED) << "control: the defect stage ran";
        if (withGhost) EXPECT_NE(0u, g_meta[f].flags & XPE_FLAG_GHOST_CORRECTED) << "control: the ghost stage ran";
    }
}

/**
 * After a call that returned an error: it is OUT_OF_MEMORY, and what the call was given is as the setup left
 * it. A frame may instead be wholly processed (a batch carries on past a failed frame), never half.
 */
std::string afterError(XpeErrorCode rc, int frames) {
    if (rc == XPE_OK) return {};
    if (rc != XPE_ERR_OUT_OF_MEMORY) return "an allocation failure must be XPE_ERR_OUT_OF_MEMORY";
    bool anyProcessed = false;
    for (int f = 0; f < frames; ++f) {
        const bool untouched = g_bytes[f] == g_bytes0[f] && sameMeta(g_meta[f], g_meta0[f]);
        const bool processed = g_bytes[f] == g_ref.bytes[f] && sameMeta(g_meta[f], g_ref.meta[f]);
        anyProcessed = anyProcessed || processed;
        if (!untouched && !processed) {
            char msg[96];
            std::snprintf(msg, sizeof msg, "frame %d is neither as it was nor fully processed (meta flags %08x, was %08x)",
                          f, g_meta[f].flags, g_meta0[f].flags);
            return msg;
        }
    }
    if (storeDigest() != g_digest0) return "the calibration store changed";
    auto* gh = static_cast<GhostCorrectorHandle*>(g_ghost);
    if (!anyProcessed && (gh->hist1 != g_hist1_0 || gh->hist2 != g_hist2_0)) return "the ghost history changed";
    return {};
}

}  // namespace pipe

class OomPipeline : public ::testing::Test {
protected:
    void SetUp() override { resetStore(); xpe_calib_cache_clear(); pipe::writeCalibDir(); }
    void TearDown() override {
        if (pipe::g_ghost) xpe_ghost_destroy(pipe::g_ghost);
        pipe::g_ghost = nullptr;
        xpe_calib_cache_clear();
        resetStore();
        xpe_clear_alerts();
        xpe_preprocess_shutdown();
        std::error_code ec;
        std::filesystem::remove_all("oom_pipe_calib", ec);
        std::filesystem::remove_all("oom_pipe_calibB", ec);
    }
};

TEST_F(OomPipeline, PipelineExThatRunsOutOfMemoryLeavesEverythingAsItWas) {
    const pipe::Call_t run = [] {
        return xpe_preprocess_pipeline_ex(&pipe::g_img[0], &pipe::g_meta[0], nullptr, pipe::g_ghost, pipe::kConfig);
    };
    pipe::captureReference(run, 1);
    sweep("xpe_preprocess_pipeline_ex", pipe::setup, run, false,
          [](XpeErrorCode rc) { return pipe::afterError(rc, 1); });
}

TEST_F(OomPipeline, PipelineThatRunsOutOfMemoryLeavesEverythingAsItWas) {
    const pipe::Call_t run = [] {
        return xpe_preprocess_pipeline(&pipe::g_img[0], &pipe::g_meta[0], "oom_pipe_calib", pipe::g_ghost, pipe::kConfig);
    };
    pipe::captureReference(run, 1);
    sweep("xpe_preprocess_pipeline", pipe::setup, run, false,
          [](XpeErrorCode rc) { return pipe::afterError(rc, 1); });
}

TEST_F(OomPipeline, PipelineBatchThatRunsOutOfMemoryLeavesEveryFrameWholeOrUntouched) {
    const pipe::Call_t run = [] {
        XpeImageBuffer imgs[pipe::kFrames] = {pipe::g_img[0], pipe::g_img[1]};
        XpeImageMetadata metas[pipe::kFrames] = {pipe::g_meta[0], pipe::g_meta[1]};
        // The batch API takes arrays: hand it copies of the descriptors (their data pointers are the
        // frames' own buffers) and copy the metadata back. No ghost handle: its history makes a frame's
        // result depend on the frames before it, and a batch carries on past a failed frame.
        const XpeErrorCode rc = xpe_preprocess_pipeline_batch(imgs, pipe::kFrames, metas, "oom_pipe_calib",
                                                              nullptr, pipe::kConfig);
        pipe::g_meta[0] = metas[0];
        pipe::g_meta[1] = metas[1];
        return rc;
    };
    pipe::captureReference(run, pipe::kFrames, /*withGhost=*/false);
    sweep("xpe_preprocess_pipeline_batch", pipe::setup, run, false,
          [](XpeErrorCode rc) { return pipe::afterError(rc, pipe::kFrames); });
}

/* =========================================================================
 * The size the caller claims for the frame (QA-A-205, #234)
 * ========================================================================= */

// The temperature stage copied img->dataSize bytes into a buffer of width*height uint16, so a dataSize larger
// than the frame the dimensions describe -- which is what a caller must pass to have the float result of the
// gain stage written back -- overran it. The allocator in this executable puts a canary behind every block
// allocated while the guard is on and checks it when the block is freed, so an overrun is reported here
// instead of corrupting the heap.

namespace dsz {

constexpr size_t kCapacity = N * sizeof(float) + 64;   // room for every claim below
constexpr unsigned char kSentinel = 0xAB;

struct Frame {
    std::vector<uint8_t> bytes;
    XpeImageBuffer img{};
    XpeImageMetadata meta{};
    std::vector<uint8_t> before;
    explicit Frame(size_t claim) : bytes(kCapacity, 0) {
        auto* px = reinterpret_cast<uint16_t*>(bytes.data());
        for (size_t i = 0; i < N; ++i) px[i] = static_cast<uint16_t>(1000 + (i * 37) % 300);
        std::fill(bytes.begin() + static_cast<long>(N * sizeof(float)), bytes.end(), kSentinel);
        img.data = bytes.data();
        img.width = W; img.height = H;
        img.bitsAllocated = 16; img.bitsStored = 16;
        img.format = XPE_PIXEL_UINT16;
        img.dataSize = claim;
        before = bytes;
    }
};

using Entry = std::function<XpeErrorCode(Frame&, const char* config)>;

// Readout validation (stage 0.5) looks at dataSize itself, so a refusal there is not the pipeline's own; this
// configuration leaves it out.
const char* const kNoReadout = "{\"bypassReadout\":true,\"detectorTempC\":\"25.5\",\"binningMode\":\"2\"}";

struct Entries { const char* name; Entry call; };

std::vector<Entries> entries() {
    return {
        {"xpe_preprocess_pipeline_ex",
         [](Frame& f, const char* cfg) { return xpe_preprocess_pipeline_ex(&f.img, &f.meta, nullptr, nullptr, cfg); }},
        {"xpe_preprocess_pipeline",
         [](Frame& f, const char* cfg) { return xpe_preprocess_pipeline(&f.img, &f.meta, "oom_pipe_calib", nullptr, cfg); }},
        {"xpe_preprocess_pipeline_batch",
         [](Frame& f, const char* cfg) {
             return xpe_preprocess_pipeline_batch(&f.img, 1, &f.meta, "oom_pipe_calib", nullptr, cfg);
         }},
    };
}

/** Runs the entry point on a frame claiming `claim` bytes; reports whether the allocator saw an overrun. */
XpeErrorCode run(const Entry& call, Frame& f, long* overruns, const char* config = pipe::kConfig) {
    guard::reset();
    guard::on(true);
    XpeErrorCode rc = XPE_OK;
    try { rc = call(f, config); } catch (...) { rc = XPE_ERR_PROCESSING_FAILED; }
    guard::on(false);
    *overruns = guard::overruns();
    return rc;
}

}  // namespace dsz

TEST_F(OomPipeline, TheFrameCopiedIsTheOneTheDimensionsDescribeWhateverSizeTheCallerClaims) {
    for (const auto& e : dsz::entries()) {
        SCOPED_TRACE(e.name);
        // Controls first: the guard is able to see an overrun at all (a block that is written past its
        // end is reported), and a clean run reports none.
        {
            guard::reset();
            guard::on(true);
            // Through function pointers, so that the compiler cannot elide the allocation.
            void* (*const allocate)(std::size_t) = static_cast<void* (*)(std::size_t)>(&::operator new);
            void (*const release)(void*) noexcept = static_cast<void (*)(void*) noexcept>(&::operator delete);
            auto* block = static_cast<volatile unsigned char*>(allocate(16));
            block[16] = 0x00;                          // one byte past the block, into its canary
            release(const_cast<unsigned char*>(block));
            guard::on(false);
            ASSERT_EQ(1, guard::overruns()) << "control: the guard reports a write past the end of a block";
        }
        pipe::setup();

        dsz::Frame full(N * sizeof(float));
        long overruns = -1;
        ASSERT_EQ(XPE_OK, dsz::run(e.call, full, &overruns)) << "control: the frame-sized claim succeeds";
        EXPECT_EQ(0, overruns) << "a claim of one float frame must not overrun any buffer";
        EXPECT_NE(full.before, full.bytes) << "control: the frame was processed";

        // (A claim of N * sizeof(uint16_t) used to be listed here as a success for a float result. It is
        // not: the float frame needs N * sizeof(float) bytes -- see TheOutputCapacityMustHold...)
        const size_t claims[] = {N * sizeof(float) + 13};
        for (size_t claim : claims) {
            SCOPED_TRACE(claim);
            pipe::setup();
            dsz::Frame f(claim);
            ASSERT_EQ(XPE_OK, dsz::run(e.call, f, &overruns));
            EXPECT_EQ(0, overruns) << "a claim of " << claim << " bytes overran a buffer";
            // Nothing at or beyond the claimed size was written.
            EXPECT_TRUE(std::equal(f.bytes.begin() + static_cast<long>(claim), f.bytes.end(),
                                   f.before.begin() + static_cast<long>(claim)))
                << "bytes beyond the claimed size were written";
            // What was written is the same result, truncated to the claim: the output is the float frame
            // of the clean run, and a claim larger than a frame does not change it.
            const size_t comparable = std::min(claim, N * sizeof(float));
            EXPECT_TRUE(std::equal(f.bytes.begin(), f.bytes.begin() + static_cast<long>(comparable),
                                   full.bytes.begin()))
                << "the result for a claim of " << claim << " bytes differs from the frame-sized claim";
        }
    }
}

TEST_F(OomPipeline, AClaimSmallerThanTheFrameIsRefusedBeforeAnythingIsDone) {
    for (const auto& e : dsz::entries()) {
        SCOPED_TRACE(e.name);
        for (const char* config : {pipe::kConfig, dsz::kNoReadout}) {
            SCOPED_TRACE(config);
            for (size_t claim : {N * sizeof(uint16_t) - 1, N * sizeof(uint16_t) - 2, static_cast<size_t>(1)}) {
                SCOPED_TRACE(claim);
                pipe::setup();
                dsz::Frame f(claim);
                long overruns = -1;
                const XpeErrorCode rc = dsz::run(e.call, f, &overruns, config);
                EXPECT_EQ(XPE_ERR_INVALID_INPUT, rc) << "the buffer is smaller than the dimensions need";
                EXPECT_EQ(0, overruns);
                EXPECT_EQ(f.before, f.bytes) << "a refused call must not touch the frame";
                EXPECT_EQ(0u, f.meta.flags) << "a refused call must not touch the metadata";
            }
        }
    }
}

/* =========================================================================
 * The room the caller gives for the result, and the byte count it implies (QA-A-205b, Codex #29 A1-A3)
 * ========================================================================= */

// The pipeline writes its result into the buffer it read from. A float result needs width*height*4 bytes; the
// copy used to be min(dataSize, that) and returned OK, so a buffer of width*height*2 -- or one of 0, "size not
// given" -- came back as a half-written float frame with the format of a whole one.

namespace dcap {

// Every stage that makes float is bypassed: what the pipeline hands back is the uint16 frame.
const char* const kUint16Out =
    "{\"bypassReadout\":true,\"bypassGain\":true,\"bypassBinning\":true,\"bypassDefect\":true,"
    "\"bypassGhost\":true,\"detectorTempC\":\"25.5\"}";

// No stage runs: the final stage is the caller's own buffer, so the frame comes back as it went in and there is
// nothing to copy (a copy of a range onto itself is undefined, which is why the product skips it).
const char* const kNoStage =
    "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,\"bypassNonlinearity\":true,"
    "\"bypassGain\":true,\"bypassBinning\":true,\"bypassDefect\":true,\"bypassGhost\":true}";

struct Shape {
    const char* name;
    const char* config;
    size_t outputBytes;
    decltype(XpeImageBuffer::format) format;
    bool changesTheFrame;
};

}  // namespace dcap

TEST_F(OomPipeline, TheOutputCapacityMustHoldTheWholeFinalFrame) {
    const dcap::Shape shapes[] = {
        {"float result", dsz::kNoReadout, N * sizeof(float), XPE_PIXEL_FLOAT32, true},
        {"uint16 result", dcap::kUint16Out, N * sizeof(uint16_t), XPE_PIXEL_UINT16, true},
        {"no stage runs", dcap::kNoStage, N * sizeof(uint16_t), XPE_PIXEL_UINT16, false},
    };
    const size_t claims[] = {0, N * sizeof(uint16_t), N * sizeof(float) - 1, N * sizeof(float), N * sizeof(float) + 13};

    for (const auto& e : dsz::entries()) {
        for (const auto& shape : shapes) {
            SCOPED_TRACE(std::string(e.name) + " / " + shape.name);

            // Control: with room to spare the configuration yields the format and the frame this row expects.
            long overruns = -1;
            pipe::setup();
            dsz::Frame ref(N * sizeof(float) + 13);
            ASSERT_EQ(XPE_OK, dsz::run(e.call, ref, &overruns, shape.config)) << "control: a roomy claim succeeds";
            ASSERT_EQ(shape.format, ref.img.format) << "control: this configuration gives that format";
            if (shape.changesTheFrame) ASSERT_NE(ref.before, ref.bytes) << "control: the frame was processed";
            else ASSERT_EQ(ref.before, ref.bytes) << "control: with no stage the frame comes back as it went in";

            for (size_t claim : claims) {
                SCOPED_TRACE(claim);
                pipe::setup();
                dsz::Frame f(claim);
                const XpeErrorCode rc = dsz::run(e.call, f, &overruns, shape.config);
                EXPECT_EQ(0, overruns);
                if (claim < shape.outputBytes) {
                    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, rc) << "the result does not fit in the claimed size";
                    EXPECT_EQ(f.before, f.bytes) << "a refused call must not write a partial frame";
                    EXPECT_EQ(0u, f.meta.flags) << "a refused call must not claim a stage ran";
                    EXPECT_EQ(XPE_PIXEL_UINT16, f.img.format) << "a refused call must not change the format";
                    EXPECT_EQ(16u, f.img.bitsAllocated);
                } else {
                    EXPECT_EQ(XPE_OK, rc);
                    EXPECT_EQ(shape.format, f.img.format);
                    EXPECT_TRUE(std::equal(f.bytes.begin(), f.bytes.begin() + static_cast<long>(shape.outputBytes),
                                           ref.bytes.begin()))
                        << "the whole final frame is written, the same one whatever the spare room";
                    EXPECT_TRUE(std::equal(f.bytes.begin() + static_cast<long>(shape.outputBytes), f.bytes.end(),
                                           f.before.begin() + static_cast<long>(shape.outputBytes)))
                        << "nothing is written beyond the final frame";
                }
            }
        }
    }
}

// A batch checks each frame as it comes. The batch contract is "carry on past a failed frame and report the first
// error", and frames of a batch are not required to share dimensions, so one frame's room says nothing about
// another's: checking all frames before the first would make frame 3's buffer decide whether frame 1 is processed.
TEST_F(OomPipeline, ABatchChecksTheRoomOfEachFrameAndCarriesOnPastARefusal) {
    pipe::setup();
    dsz::Frame ref(N * sizeof(float));
    long overruns = -1;
    const auto batchOne = [](dsz::Frame& f, const char* cfg) {
        return xpe_preprocess_pipeline_batch(&f.img, 1, &f.meta, "oom_pipe_calib", nullptr, cfg);
    };
    ASSERT_EQ(XPE_OK, dsz::run(batchOne, ref, &overruns, dsz::kNoReadout));

    pipe::setup();
    dsz::Frame tight(N * sizeof(float) - 1), roomy(N * sizeof(float));
    XpeImageBuffer imgs[2] = {tight.img, roomy.img};
    XpeImageMetadata metas[2] = {tight.meta, roomy.meta};
    guard::reset();
    guard::on(true);
    const XpeErrorCode rc = xpe_preprocess_pipeline_batch(imgs, 2, metas, "oom_pipe_calib", nullptr, dsz::kNoReadout);
    guard::on(false);
    EXPECT_EQ(0, guard::overruns());

    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, rc) << "the first error is reported";
    EXPECT_EQ(tight.before, tight.bytes) << "the refused frame is untouched";
    EXPECT_EQ(0u, metas[0].flags);
    EXPECT_EQ(XPE_PIXEL_UINT16, imgs[0].format);
    EXPECT_EQ(XPE_PIXEL_FLOAT32, imgs[1].format) << "the next frame was processed";
    EXPECT_TRUE(std::equal(roomy.bytes.begin(), roomy.bytes.begin() + static_cast<long>(N * sizeof(float)),
                           ref.bytes.begin()))
        << "and its result is the one a single-frame batch gives";
}

// width*height*2 (and *4) were unchecked multiplications. Two uint32 sides need 64 bits for their product and
// the byte count needs one or two more, so the count wrapped to a small number that then passed the size checks
// and sized the copies. The refused shapes need no allocation to test: the claim is 0 or a few bytes, and the
// frame is never read.
TEST_F(OomPipeline, AFrameWhoseByteCountDoesNotFitInSizeTIsRefusedBeforeItIsRead) {
    if constexpr (sizeof(size_t) < 8) { GTEST_SKIP() << "the shapes below overflow a 64-bit size_t"; }

    struct Dims { const char* what; uint32_t w, h; bool fits; };
    const Dims dims[] = {
        {"w*h*2 wraps (the shape Codex #29 names)", 0xFFFFFFFFu, 0x80000001u, false},
        {"w*h*2 fits, w*h*4 wraps", 0x80000000u, 0x80000001u, false},
        {"w*h itself is 2^64-2^33+1", 0xFFFFFFFFu, 0xFFFFFFFFu, false},
        {"a side is 0 (w)", 0u, 16u, false},
        {"a side is 0 (h)", 16u, 0u, false},
        // Control: 2^61 pixels, 2^63 float bytes -- everything fits in a size_t, so this is a size question
        // (claim 0 = size not given = no room), not an overflow.
        {"largest-ish shape that fits", 0x80000000u, 0x40000000u, true},
    };

    for (const auto& e : dsz::entries()) {
        for (const auto& d : dims) {
            for (size_t claim : {static_cast<size_t>(0), static_cast<size_t>(64)}) {
                SCOPED_TRACE(std::string(e.name) + " / " + d.what + " / claim " + std::to_string(claim));
                pipe::setup();
                dsz::Frame f(claim);
                f.img.width = d.w;
                f.img.height = d.h;
                long overruns = -1;
                bool threw = false;
                XpeErrorCode rc = XPE_OK;
                guard::reset();
                guard::on(true);
                try { rc = e.call(f, dsz::kNoReadout); } catch (...) { threw = true; }
                guard::on(false);
                overruns = guard::overruns();
                EXPECT_FALSE(threw);
                EXPECT_EQ(0, overruns);
                if (d.fits) {
                    EXPECT_EQ(claim == 0 ? XPE_ERR_BUFFER_TOO_SMALL : XPE_ERR_INVALID_INPUT, rc)
                        << "control: a count that fits is judged by the size the caller gave";
                } else {
                    EXPECT_EQ(XPE_ERR_INVALID_INPUT, rc) << "a count that does not fit, or an empty frame";
                }
                EXPECT_EQ(f.before, f.bytes);
                EXPECT_EQ(0u, f.meta.flags);
            }
        }
    }
}

/* =========================================================================
 * The calibration set is replaced as a whole or not at all (QA-A-202c, Codex #27 A1)
 * ========================================================================= */

// The pipeline read offset.xcal, gain.xcal and defect.xcal into the global store one after the other, so a
// failure in the second or third left the new first map beside the old others. The earlier OomPipeline tests
// reloaded the SAME files that were already in the store, so a half-replaced store looked like the original
// -- they could not see this. Here the store holds set A and the pipeline is pointed at set B, whose maps,
// gain quality metadata and file contents differ from A's in everything the digest looks at.

namespace calibset {

constexpr const char* kDirB = "oom_pipe_calibB";

/** Everything the three loads put into the store, plus the module's quality metadata. */
uint64_t fullDigest() {
    uint64_t h;
    {
        std::lock_guard<std::mutex> lk(g_calib_mutex);
        h = 1469598103934665603ull;
        auto mix = [&h](const void* p, size_t n) {
            const auto* b = static_cast<const unsigned char*>(p);
            for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
        };
        mix(&g_calib.offset_width, sizeof g_calib.offset_width);
        mix(&g_calib.offset_height, sizeof g_calib.offset_height);
        mix(&g_calib.offset_timestamp, sizeof g_calib.offset_timestamp);
        mix(&g_calib.offset_expiry_ms, sizeof g_calib.offset_expiry_ms);
        mix(g_calib.offset_session_id, sizeof g_calib.offset_session_id);
        mix(&g_calib.gain_width, sizeof g_calib.gain_width);
        mix(&g_calib.gain_height, sizeof g_calib.gain_height);
        mix(&g_calib.gain_timestamp, sizeof g_calib.gain_timestamp);
        mix(&g_calib.gain_expiry_ms, sizeof g_calib.gain_expiry_ms);
        mix(g_calib.gain_session_id, sizeof g_calib.gain_session_id);
        mix(&g_calib.gain_has_quality, sizeof g_calib.gain_has_quality);
        mix(&g_calib.gain_quality.r_squared, sizeof g_calib.gain_quality.r_squared);
        mix(&g_calib.gain_poly_num_coeffs, sizeof g_calib.gain_poly_num_coeffs);
        mix(&g_calib.defect_width, sizeof g_calib.defect_width);
        mix(&g_calib.defect_height, sizeof g_calib.defect_height);
        mix(&g_calib.defect_expiry_ms, sizeof g_calib.defect_expiry_ms);
        if (g_calib.offset_map) mix(g_calib.offset_map.get(), N * sizeof(float));
        if (g_calib.gain_map) mix(g_calib.gain_map.get(), N * sizeof(float));
        if (g_calib.defect_map) mix(g_calib.defect_map.get(), N);
    }
    XpeCalibQualityMeta q{};
    xpe_calib_get_quality_meta(&q);
    auto mixq = [&h](const void* p, size_t n) {
        const auto* b = static_cast<const unsigned char*>(p);
        for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
    };
    mixq(&q.r_squared, sizeof q.r_squared);
    mixq(&q.previous_r_squared, sizeof q.previous_r_squared);
    mixq(&q.polynomial_degree, 1);
    mixq(&q.num_points, 1);
    mixq(&q.calibration_mode, 1);
    mixq(&q.calibration_pass, 1);
    return h;
}

const char* const kBGainJson =
    "{\"fit_r_squared\":\"0.97\",\"polynomial_degree\":\"2\",\"actual_dose_levels\":\"4\",\"calibration_mode\":\"3\"}";

void writeDefectFile(const char* path, bool flagged, int64_t expiryMs) {
    std::vector<uint8_t> m(N, 0);
    if (flagged) m[5] = 1;
    writeFile(path, XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, m.data(), m.size(), "{}", expiryMs);
}

/** Set B: different maps and different gain metadata from set A (offset 100 / gain 2 / defect flagged). */
void writeSetB(const std::string& gainJson = kBGainJson) {
    std::filesystem::create_directories(kDirB);
    writeOffset("oom_pipe_calibB/offset.xcal", 300.0f);
    writeGain("oom_pipe_calibB/gain.xcal", 4.0f, gainJson);
    writeDefectFile("oom_pipe_calibB/defect.xcal", false, 0);
}

/** The digest the store has after set B has been loaded on its own -- what a successful call must leave. */
uint64_t digestOfB() {
    pipe::setup();
    EXPECT_EQ(XPE_OK, xpe_calib_load_offset("oom_pipe_calibB/offset.xcal"));
    EXPECT_EQ(XPE_OK, xpe_calib_load_gain("oom_pipe_calibB/gain.xcal"));
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_map("oom_pipe_calibB/defect.xcal"));
    return fullDigest();
}

/** The store as set A left it. */
uint64_t digestOfA() {
    pipe::setup();
    return fullDigest();
}

using Run = std::function<XpeErrorCode()>;

/** Entry points that read the three files from a directory: the single frame call and the batch. */
std::vector<std::pair<const char*, Run>> byDirectory() {
    return {
        {"xpe_preprocess_pipeline",
         [] { return xpe_preprocess_pipeline(&pipe::g_img[0], &pipe::g_meta[0], calibset::kDirB, nullptr, pipe::kConfig); }},
        {"xpe_preprocess_pipeline_batch",
         [] {
             XpeImageBuffer img = pipe::g_img[0];
             XpeImageMetadata meta = pipe::g_meta[0];
             return xpe_preprocess_pipeline_batch(&img, 1, &meta, calibset::kDirB, nullptr, pipe::kConfig);
         }},
    };
}

}  // namespace calibset

TEST_F(OomPipeline, SetsAAndBDifferInEverythingTheDigestLooksAt) {
    calibset::writeSetB();
    const uint64_t a = calibset::digestOfA();
    const uint64_t b = calibset::digestOfB();
    EXPECT_NE(a, b) << "control: the two sets are distinguishable, so a half-replaced store is too";
    EXPECT_EQ(a, calibset::digestOfA()) << "control: the digest of an untouched store is stable";
}

TEST_F(OomPipeline, AFileErrorInTheSecondOrThirdLoadLeavesTheWholeCalibrationSetAsItWas) {
    struct Case { const char* name; std::function<void()> damage; };
    const Case cases[] = {
        {"gain.xcal missing", [] { std::remove("oom_pipe_calibB/gain.xcal"); }},
        {"gain.xcal corrupted (checksum)", [] {
             std::fstream f("oom_pipe_calibB/gain.xcal", std::ios::in | std::ios::out | std::ios::binary);
             f.seekg(-1, std::ios::end); char c = 0; f.read(&c, 1); c = static_cast<char>(c ^ 0x5A);
             f.seekp(-1, std::ios::end); f.write(&c, 1); }},
        {"gain.xcal with a malformed quality field", [] { calibset::writeSetB("{\"fit_r_squared\":\"abc\"}"); }},
        {"defect.xcal missing", [] { std::remove("oom_pipe_calibB/defect.xcal"); }},
        {"defect.xcal expired", [] { calibset::writeDefectFile("oom_pipe_calibB/defect.xcal", false, 1); }},
    };
    for (const auto& entry : calibset::byDirectory()) {
        for (const Case& c : cases) {
            SCOPED_TRACE(std::string(entry.first) + " / " + c.name);
            calibset::writeSetB();
            const uint64_t a = calibset::digestOfA();
            const uint64_t b = calibset::digestOfB();
            ASSERT_NE(a, b);

            // Control: with set B intact the call succeeds and the store becomes B, all of it.
            pipe::setup();
            ASSERT_EQ(XPE_OK, entry.second()) << "control: set B loads through the pipeline";
            ASSERT_EQ(b, calibset::fullDigest()) << "control: a successful call leaves exactly set B";

            // The damage, then the same call on a store holding set A.
            c.damage();
            pipe::setup();
            ASSERT_EQ(a, calibset::fullDigest());
            const XpeErrorCode rc = entry.second();
            EXPECT_NE(XPE_OK, rc) << "the damaged set must be refused";
            EXPECT_EQ(a, calibset::fullDigest())
                << "a refused call must leave all three maps, their dimensions and the quality metadata as they were";
        }
    }
}

TEST_F(OomPipeline, AnOutOfMemoryInTheSecondOrThirdLoadLeavesTheWholeCalibrationSetAsItWas) {
    calibset::writeSetB();
    const uint64_t a = calibset::digestOfA();
    const uint64_t b = calibset::digestOfB();
    ASSERT_NE(a, b);
    for (const auto& entry : calibset::byDirectory()) {
        // The store is wholly set A or wholly set B after every call -- never a mixture. It is A while the
        // set is still being read (nothing is committed before all three files were staged) and B once the
        // set has loaded, even if a later stage then runs out of memory (the loads succeeded; that is not
        // undone, exactly as for xpe_calib_load_* followed by a correction call). The allocation order is
        // fixed, so as the failing allocation moves later the store goes from A to B once and stays there.
        auto seenB = std::make_shared<bool>(false);
        sweep(entry.first, pipe::setup, entry.second, false, [a, b, seenB](XpeErrorCode rc) -> std::string {
            const uint64_t now = calibset::fullDigest();
            if (rc == XPE_OK) return now == b ? std::string() : "the call succeeded but the store is not set B";
            if (rc != XPE_ERR_OUT_OF_MEMORY) return "an allocation failure must be XPE_ERR_OUT_OF_MEMORY";
            if (now == b) { *seenB = true; return {}; }
            if (now != a) return "the store is neither set A nor set B: a failed load replaced part of it";
            return *seenB ? "the store went back to set A after it had already become set B" : std::string();
        });
        EXPECT_TRUE(*seenB) << entry.first << ": control: some failing allocation lands after the set was committed";
    }
}

/* =========================================================================
 * Quality metadata and the calibration set a frame is processed with (QA-A-202d, Codex #32 A1 / A2)
 * ========================================================================= */

// The three maps and the gain quality metadata move to the store together, so a reader that looks at the
// quality the moment the maps are in sees the new set's. The quality used to be updated AFTER g_calib_mutex was
// released, in its own global that no lock guarded: between the two, the new maps and the old quality coexisted,
// and a concurrent reader raced the writer. A frame likewise read the three maps one stage at a time, each stage
// taking the lock afresh, so a set loaded between two stages gave one frame an offset from A and a gain from B.

namespace qmeta {

struct Seen { bool fired{false}; XpeCalibQualityMeta q{}; XpeErrorCode rc{XPE_OK}; };
Seen g_seen;

void observe() {
    g_seen.fired = true;
    g_seen.rc = xpe_calib_get_quality_meta(&g_seen.q);
}

struct HookGuard {
    explicit HookGuard(void (*h)()) { xpe_calib_after_set_commit_hook = h; }
    ~HookGuard() { xpe_calib_after_set_commit_hook = nullptr; }
};

}  // namespace qmeta

TEST_F(OomPipeline, TheQualityMetadataIsCurrentTheMomentTheMapsAre) {
    calibset::writeSetB();
    for (const auto& e : calibset::byDirectory()) {
        SCOPED_TRACE(e.first);
        pipe::setup();
        {
            XpeCalibQualityMeta before{};
            ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&before));
            ASSERT_NE(0.97, before.r_squared) << "control: set A's store does not already hold set B's quality";
        }
        qmeta::g_seen = qmeta::Seen{};
        {
            qmeta::HookGuard guard(&qmeta::observe);
            ASSERT_EQ(XPE_OK, e.second());
        }
        ASSERT_TRUE(qmeta::g_seen.fired) << "control: the observation point was reached";
        ASSERT_EQ(XPE_OK, qmeta::g_seen.rc);
        // What the hook saw is what the gain file of set B says: written the moment the maps were.
        EXPECT_DOUBLE_EQ(0.97, qmeta::g_seen.q.r_squared) << "the maps are set B's; the quality read beside them must be too";
        EXPECT_EQ(2u, qmeta::g_seen.q.polynomial_degree);
        EXPECT_EQ(4u, qmeta::g_seen.q.num_points);
        EXPECT_EQ(3u, qmeta::g_seen.q.calibration_mode);
    }
}

// Help, not proof: the deterministic test above is the evidence. A reader that runs while the store is replaced
// over and over must see one gain file's quality or the other's -- never fields of both.
TEST_F(OomPipeline, AQualityReadWhileTheStoreIsBeingReplacedSeesOneFilesFields) {
    std::filesystem::create_directories("oom_pipe_calibB");
    writeGain("oom_pipe_calibB/q1.xcal", 2.0f,
              "{\"fit_r_squared\":\"0.91\",\"polynomial_degree\":\"1\",\"actual_dose_levels\":\"3\",\"calibration_mode\":\"2\"}");
    writeGain("oom_pipe_calibB/q2.xcal", 4.0f,
              "{\"fit_r_squared\":\"0.97\",\"polynomial_degree\":\"2\",\"actual_dose_levels\":\"4\",\"calibration_mode\":\"3\"}");
    pipe::setup();
    std::atomic<bool> stop{false};
    std::atomic<long> torn{0}, reads{0};
    std::thread reader([&] {
        while (!stop.load()) {
            XpeCalibQualityMeta q{};
            xpe_calib_get_quality_meta(&q);
            const bool one = (q.r_squared == 0.91 && q.polynomial_degree == 1 && q.num_points == 3 && q.calibration_mode == 2);
            const bool two = (q.r_squared == 0.97 && q.polynomial_degree == 2 && q.num_points == 4 && q.calibration_mode == 3);
            const bool none = (q.r_squared == 0.0 && q.polynomial_degree == 0 && q.num_points == 0 && q.calibration_mode == 0);
            if (!one && !two && !none) torn.fetch_add(1);
            reads.fetch_add(1);
        }
    });
    for (int i = 0; i < 300; ++i) {
        xpe_calib_load_gain((i % 2 == 0) ? "oom_pipe_calibB/q1.xcal" : "oom_pipe_calibB/q2.xcal");
    }
    stop.store(true);
    reader.join();
    EXPECT_GT(reads.load(), 0L) << "control: the reader read";
    EXPECT_EQ(0L, torn.load()) << "a read returned fields that belong to no single gain file";
}

namespace snapshot {

int g_fired = 0;   // times the hook was called (a batch of two frames calls it twice)
int g_loads = 0;   // times set B was loaded by the hook: once, after the first frame's offset stage
bool g_armed = false;

void loadSetB() {
    ++g_loads;
    xpe_calib_load_offset("oom_pipe_calibB/offset.xcal");
    xpe_calib_load_gain("oom_pipe_calibB/gain.xcal");
    xpe_calib_load_defect_map("oom_pipe_calibB/defect.xcal");
}

void hook(int stage) {
    if (stage == 2 && g_armed && g_fired++ == 0) loadSetB();
}

struct HookGuard {
    HookGuard() { g_fired = 0; g_loads = 0; g_armed = true; xpe_pipeline_after_stage_hook = &hook; }
    ~HookGuard() { xpe_pipeline_after_stage_hook = nullptr; g_armed = false; }
};

using Entry = std::pair<const char*, std::function<XpeErrorCode(int frames)>>;

/** The entry points, each handed `frames` frames of the fixture and no ghost handle (a ghost's history would
 *  make a second frame depend on the first). Set A is in the store (or on disk, for the directory forms). */
std::vector<Entry> entries() {
    return {
        {"xpe_preprocess_pipeline_ex",
         [](int) { return xpe_preprocess_pipeline_ex(&pipe::g_img[0], &pipe::g_meta[0], nullptr, nullptr, pipe::kConfig); }},
        {"xpe_preprocess_pipeline",
         [](int) { return xpe_preprocess_pipeline(&pipe::g_img[0], &pipe::g_meta[0], "oom_pipe_calib", nullptr, pipe::kConfig); }},
        {"xpe_preprocess_pipeline_batch",
         [](int frames) {
             return xpe_preprocess_pipeline_batch(pipe::g_img, static_cast<uint32_t>(frames), pipe::g_meta,
                                                  "oom_pipe_calib", nullptr, pipe::kConfig);
         }},
    };
}

}  // namespace snapshot

TEST_F(OomPipeline, AFrameIsProcessedWithOneCalibrationSetEvenIfAnotherIsLoadedMidFrame) {
    calibset::writeSetB();
    for (const auto& e : snapshot::entries()) {
        SCOPED_TRACE(e.first);
        const int frames = (std::string(e.first) == "xpe_preprocess_pipeline_batch") ? pipe::kFrames : 1;

        // Reference 1: set A alone.
        pipe::setup();
        ASSERT_EQ(XPE_OK, e.second(frames));
        std::vector<uint8_t> onlyA[pipe::kFrames];
        for (int f = 0; f < frames; ++f) onlyA[f] = pipe::g_bytes[f];

        // Reference 2 (control): set B alone must give a different frame, or "the same as set A" says nothing.
        pipe::setup();
        snapshot::loadSetB();
        ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_ex(&pipe::g_img[0], &pipe::g_meta[0], nullptr, nullptr, pipe::kConfig));
        ASSERT_NE(onlyA[0], pipe::g_bytes[0]) << "control: the two sets give different frames";

        // The run under test: set A is current when the frame starts; set B is loaded right after its offset stage.
        pipe::setup();
        {
            snapshot::HookGuard guard;
            ASSERT_EQ(XPE_OK, e.second(frames));
            ASSERT_GE(snapshot::g_fired, 1) << "control: the observation point was reached";
            ASSERT_EQ(1, snapshot::g_loads) << "control: set B was loaded in the middle of the frame";
        }
        for (int f = 0; f < frames; ++f) {
            EXPECT_EQ(onlyA[f], pipe::g_bytes[f])
                << "frame " << f << " is not what set A alone gives: it read the set that was loaded mid-frame";
        }
    }
}

// The reader's side of the lock (QA-A-202e: synchronised on the reader's ARRIVAL at the lock attempt).
//
// The commit holds g_calib_mutex while it moves the maps and the quality. A reader is started from inside that
// critical section. xpe_calib_quality_before_lock_hook marks the moment it is about to take the lock, so "the reader
// has got as far as the getter" is OBSERVED, not assumed from the thread having been created. From there the test
// gives a reader that does not wait for the lock a full second to finish: a getter without the lock finishes at
// once (or, if it is delayed, after the delay -- any delay under a second is still caught). A getter that takes the
// lock cannot finish before the section ends. After the section ends, the reader must see the new set's quality.
namespace readerlock {

std::atomic<bool> g_arrived{false};
std::atomic<bool> g_done{false};
XpeCalibQualityMeta g_q{};
std::thread g_reader;
bool g_arrivedInTime = false;
bool g_finishedInsideTheSection = false;

void beforeLock() { g_arrived.store(true); }

void hook() {
    g_arrived.store(false);
    g_done.store(false);
    g_reader = std::thread([] {
        xpe_calib_get_quality_meta(&g_q);
        g_done.store(true);
    });
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    while (!g_arrived.load() && Clock::now() - start < std::chrono::seconds(10)) std::this_thread::yield();
    g_arrivedInTime = g_arrived.load();
    const auto window = Clock::now();
    while (!g_done.load() && Clock::now() - window < std::chrono::milliseconds(1000)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    g_finishedInsideTheSection = g_done.load();
}

struct HookGuard {
    HookGuard() {
        xpe_calib_quality_before_lock_hook = &beforeLock;
        xpe_calib_in_set_commit_hook = &hook;
    }
    ~HookGuard() {
        xpe_calib_in_set_commit_hook = nullptr;
        xpe_calib_quality_before_lock_hook = nullptr;
    }
};

}  // namespace readerlock

TEST_F(OomPipeline, AQualityReadStartedInsideTheCommitWaitsForItAndSeesTheNewSet) {
    calibset::writeSetB();
    pipe::setup();
    readerlock::g_arrivedInTime = false;
    readerlock::g_finishedInsideTheSection = false;
    {
        readerlock::HookGuard guard;
        ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_batch(pipe::g_img, 1, pipe::g_meta, calibset::kDirB, nullptr, pipe::kConfig));
    }
    ASSERT_TRUE(readerlock::g_reader.joinable()) << "control: the observation point inside the commit was reached";
    readerlock::g_reader.join();
    ASSERT_TRUE(readerlock::g_arrivedInTime) << "control: the reader reached the getter's lock attempt";
    EXPECT_FALSE(readerlock::g_finishedInsideTheSection) << "the reader finished while the commit still held the lock";
    EXPECT_DOUBLE_EQ(0.97, readerlock::g_q.r_squared) << "after the section ended the reader saw set B's quality";
    EXPECT_EQ(2u, readerlock::g_q.polynomial_degree);
}

/* =========================================================================
 * Every configuration reads only what it owns and ends in the format it was predicted to (QA-A-208, Codex #34 A1)
 * ========================================================================= */

// With the gain stage bypassed, stage 4 was the stage-3 buffer -- N uint16 values -- yet binning, the defect stage
// and the ghost stage all treat their input as N float32 values. Binning and ghost copied N*4 bytes out of it (a
// read past the end of the stage buffer, or, when no earlier stage made a buffer, the upper half of the caller's
// buffer read as floats), and the defect stage refused a uint16 frame outright. A gain bypass now means "gain = 1":
// the frame is converted to float32 explicitly before the first stage that needs floats.
//
// Reads past the end of a block are made visible by the page-guard allocator (a NOACCESS page right after every
// block made during the call); the access violation is caught and reported as a failure of that combination.

namespace combo {

struct Config { bool temp, offset, gain; int binning; bool defect, ghost; };   // binning: 0 bypassed, 1, 2

std::string json(const Config& c) {
    std::string j = "{\"bypassReadout\":true,\"bypassNonlinearity\":true,\"detectorTempC\":\"25.5\"";
    j += c.temp ? ",\"bypassTemp\":false" : ",\"bypassTemp\":true";
    j += c.offset ? ",\"bypassOffset\":false" : ",\"bypassOffset\":true";
    j += c.gain ? ",\"bypassGain\":false" : ",\"bypassGain\":true";
    j += (c.binning == 0) ? ",\"bypassBinning\":true" : ",\"bypassBinning\":false";
    j += ",\"binningMode\":\"" + std::to_string(c.binning == 0 ? 1 : c.binning) + "\"";
    j += c.defect ? ",\"bypassDefect\":false" : ",\"bypassDefect\":true";
    j += c.ghost ? ",\"bypassGhost\":false}" : ",\"bypassGhost\":true}";
    return j;
}

/** What the final frame must be, stated here independently of the product: float32 when a float stage runs. */
bool predictFloat(const Config& c) { return c.gain || c.binning == 2 || c.defect || c.ghost; }

std::string describe(const Config& c) {
    char buf[96];
    std::snprintf(buf, sizeof buf, "temp=%d offset=%d gain=%d binning=%d defect=%d ghost=%d",
                  c.temp, c.offset, c.gain, c.binning, c.defect, c.ghost);
    return buf;
}

#ifdef _WIN32
struct Ctx { XpeImageBuffer* img; XpeImageMetadata* meta; const char* cfg; void* ghost; XpeErrorCode rc; };
void runOne(void* p) {
    auto* c = static_cast<Ctx*>(p);
    c->rc = xpe_preprocess_pipeline_ex(c->img, c->meta, nullptr, c->ghost, c->cfg);
}
// No C++ object with a destructor may live in a function with __try (C2712), so the call goes through a pointer.
int callCatching(void (*fn)(void*), void* ctx) {
    __try {
        fn(ctx);
        return 0;
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        return 1;
    }
}

/** Runs the pipeline on `f` with the page-guard allocator on. Returns true if the call faulted (a read or write
 *  past the end of a block); `*rc` is the call's result otherwise. */
bool runGuarded(dsz::Frame& f, const Config& c, XpeErrorCode* rc) {
    const std::string cfg = json(c);
    Ctx ctx{&f.img, &f.meta, cfg.c_str(), c.ghost ? pipe::g_ghost : nullptr, XPE_OK};
    pageguard::on(true);
    const int faulted = callCatching(&runOne, &ctx);
    pageguard::on(false);
    if (faulted) g_live.fetch_sub(pageguard::reset());
    *rc = ctx.rc;
    return faulted != 0;
}
#endif

}  // namespace combo

TEST_F(OomPipeline, AGainBypassFollowedByBinningDoesNotReadPastTheStageBuffer) {
#ifndef _WIN32
    GTEST_SKIP() << "the page-guard allocator is Windows-only";
#else
    // The case Codex #34 names: N uint16 pixels with room for N floats, gain bypassed, binning 2, temperature on.
    const combo::Config c{true, true, false, 2, false, false};
    pipe::setup();
    dsz::Frame f(N * sizeof(float));
    XpeErrorCode rc = XPE_OK;
    const bool faulted = combo::runGuarded(f, c, &rc);
    EXPECT_FALSE(faulted) << "the binning stage read past the end of the gain-bypassed stage buffer";
    EXPECT_EQ(XPE_OK, rc);
    EXPECT_EQ(XPE_PIXEL_FLOAT32, f.img.format);
#endif
}

TEST_F(OomPipeline, EveryConfigurationReadsOnlyWhatItOwnsAndEndsInTheFormatItIsPredictedToEndIn) {
#ifndef _WIN32
    GTEST_SKIP() << "the page-guard allocator is Windows-only";
#else
    std::filesystem::create_directories("oom_pipe_calibOnes");
    writeGain("oom_pipe_calibOnes/gain.xcal", 1.0f, "{}");

    int combos = 0, comparedToUnitGain = 0, floatEnds = 0, uint16Ends = 0;
    std::vector<std::string> failures;
    for (int temp = 0; temp < 2; ++temp)
    for (int offset = 0; offset < 2; ++offset)
    for (int gain = 0; gain < 2; ++gain)
    for (int binning = 0; binning < 3; ++binning)
    for (int defect = 0; defect < 2; ++defect)
    for (int ghost = 0; ghost < 2; ++ghost) {
        const combo::Config c{temp != 0, offset != 0, gain != 0, binning, defect != 0, ghost != 0};
        SCOPED_TRACE(combo::describe(c));
        ++combos;

        pipe::setup();
        dsz::Frame f(N * sizeof(float));
        XpeErrorCode rc = XPE_OK;
        const bool faulted = combo::runGuarded(f, c, &rc);
        if (faulted) { failures.push_back(combo::describe(c) + ": a stage read or wrote past the end of a buffer it owns"); continue; }
        if (rc != XPE_OK) { failures.push_back(combo::describe(c) + ": rc " + std::to_string(rc)); continue; }

        const bool isFloat = combo::predictFloat(c);
        EXPECT_EQ(isFloat ? XPE_PIXEL_FLOAT32 : XPE_PIXEL_UINT16, f.img.format) << "the format is not the predicted one";
        EXPECT_EQ(isFloat ? 32u : 16u, f.img.bitsAllocated);
        (isFloat ? floatEnds : uint16Ends)++;
        const size_t outBytes = isFloat ? N * sizeof(float) : N * sizeof(uint16_t);
        EXPECT_TRUE(std::equal(f.bytes.begin() + static_cast<long>(outBytes), f.bytes.end(),
                               f.before.begin() + static_cast<long>(outBytes)))
            << "bytes beyond the final frame were written";

        // A gain bypass means "gain = 1": the same frame as a run with the gain stage on and a gain map of ones.
        if (!c.gain && isFloat) {
            combo::Config on = c;
            on.gain = true;
            pipe::setup();
            ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_pipe_calibOnes/gain.xcal"));
            dsz::Frame g(N * sizeof(float));
            XpeErrorCode rc2 = XPE_OK;
            if (combo::runGuarded(g, on, &rc2) || rc2 != XPE_OK) {
                failures.push_back(combo::describe(on) + ": the reference run with a gain map of ones failed");
                continue;
            }
            if (!std::equal(f.bytes.begin(), f.bytes.begin() + static_cast<long>(outBytes), g.bytes.begin()))
                failures.push_back(combo::describe(c) + ": a gain bypass does not give the frame a gain map of ones gives");
            ++comparedToUnitGain;
        }
    }
    {
        std::string all;
        for (const auto& f : failures) all += std::string(1, '\n') + "  " + f;
        EXPECT_TRUE(failures.empty()) << failures.size() << " configuration(s) failed:" << all;
    }
    EXPECT_EQ(96, combos) << "control: every combination was run";
    EXPECT_GT(floatEnds, 0);
    EXPECT_GT(uint16Ends, 0);
    EXPECT_GT(comparedToUnitGain, 0) << "control: some combinations were compared with a unit gain map";
    std::printf("[combo] %d configurations; %d end in float32, %d in uint16; %d compared with a gain map of ones\n",
                combos, floatEnds, uint16Ends, comparedToUnitGain);
    std::error_code ec;
    std::filesystem::remove_all("oom_pipe_calibOnes", ec);
#endif
}

/* =========================================================================
 * The quality record describes the gain calibration that is current (QA-A-202e, Codex #38 A1 / A2)
 * ========================================================================= */

// A gain file without quality metadata is a legitimate file. Loading one used to leave the PREVIOUS file's record
// in place -- the maps and a quality that belonged to another file were then committed together under one lock,
// no race needed. A load now always replaces the record: with the file's quality, or, when the file carries none,
// with the "no quality" record (valid = 0, every field zero, previous_r_squared still the history). The same for
// a cache hit, which also keeps the file-quality copy beside the map (gain_quality / gain_has_quality) in step.

static_assert(sizeof(XpeCalibQualityMeta) == 88, "the validity and presence bytes must fit in the old padding");
static_assert(offsetof(XpeCalibQualityMeta, valid) == 3, "valid sits right after num_points");
static_assert(offsetof(XpeCalibQualityMeta, has_r_squared) == 4, "has_r_squared sits in the padding before r_squared (QA-A-208d)");
static_assert(offsetof(XpeCalibQualityMeta, has_previous_r_squared) == 73, "has_previous_r_squared sits in the padding after calibration_pass (QA-A-208d)");
static_assert(offsetof(XpeCalibQualityMeta, r_squared) == 8, "r_squared did not move");
static_assert(offsetof(XpeCalibQualityMeta, calibration_timestamp) == 16, "calibration_timestamp did not move");
static_assert(offsetof(XpeCalibQualityMeta, detector_serial) == 24, "detector_serial did not move");
static_assert(offsetof(XpeCalibQualityMeta, firmware_version) == 56, "firmware_version did not move");
static_assert(offsetof(XpeCalibQualityMeta, calibration_pass) == 72, "calibration_pass did not move");
static_assert(offsetof(XpeCalibQualityMeta, previous_r_squared) == 80, "previous_r_squared did not move");

namespace qcur {

const char* const kJsonA =
    "{\"fit_r_squared\":\"0.91\",\"polynomial_degree\":\"1\",\"actual_dose_levels\":\"3\",\"calibration_mode\":\"2\"}";
const char* const kJsonC =
    "{\"fit_r_squared\":\"0.97\",\"polynomial_degree\":\"2\",\"actual_dose_levels\":\"4\",\"calibration_mode\":\"3\"}";
const char* const kDirA = "oom_pipe_calibQA";      // gain 2.0, quality A
const char* const kDirNone = "oom_pipe_calibQN";   // gain 4.0, no quality metadata
const char* const kDirC = "oom_pipe_calibQC";      // gain 3.0, quality C
const char* const kDirP = "oom_pipe_calibQP";      // gain 5.0, a PARTIAL quality: polynomial_degree only, no R2
const char* const kDirN2 = "oom_pipe_calibQM";     // gain 6.0, no quality metadata (a second one)
const char* const kDirM1 = "oom_pipe_calibQZ";     // gain 7.0, fit_r_squared exactly -1.0 (QA-A-208d)

void writeSetDir(const char* dir, float gain, const std::string& gainJson) {
    std::filesystem::create_directories(dir);
    writeOffset((std::string(dir) + "/offset.xcal").c_str(), 100.0f);
    writeGain((std::string(dir) + "/gain.xcal").c_str(), gain, gainJson);
    writeDefect((std::string(dir) + "/defect.xcal").c_str(), true);
}
void writeAll() {
    writeSetDir(kDirA, 2.0f, kJsonA);
    writeSetDir(kDirNone, 4.0f, "{}");
    writeSetDir(kDirC, 3.0f, kJsonC);
    writeSetDir(kDirP, 5.0f, "{\"polynomial_degree\":\"2\"}");
    writeSetDir(kDirN2, 6.0f, "{}");
    writeSetDir(kDirM1, 7.0f, "{\"fit_r_squared\":-1.000000000,\"polynomial_degree\":\"2\"}");
}
void removeAll() {
    std::error_code ec;
    for (const char* d : {kDirA, kDirNone, kDirC, kDirP, kDirN2, kDirM1}) std::filesystem::remove_all(d, ec);
}

XpeCalibQualityMeta current() {
    XpeCalibQualityMeta q{};
    EXPECT_EQ(XPE_OK, xpe_calib_get_quality_meta(&q));
    return q;
}

std::atomic<long> g_hitChecks{0};
void countHit() { g_hitChecks.fetch_add(1); }

struct Way {
    const char* name;
    bool isHit;
    std::function<void(const std::string&)> prepare;       // before set A is made current
    std::function<XpeErrorCode(const std::string&)> load;   // the call that makes `dir`'s gain current
};

std::string gainPath(const std::string& dir) { return dir + "/gain.xcal"; }

/** The frames as pipe::setup left them: a pipeline call processes its frame in place, and a second call on the
 *  processed (float32) frame would fail on its format, which is not what the sequence tests are about. */
void freshFrames() {
    for (int f = 0; f < pipe::kFrames; ++f) {
        pipe::g_bytes[f] = pipe::g_bytes0[f];
        pipe::g_img[f].data = pipe::g_bytes[f].data();
        pipe::g_img[f].format = XPE_PIXEL_UINT16;
        pipe::g_img[f].bitsAllocated = 16;
        pipe::g_img[f].bitsStored = 16;
        pipe::g_img[f].dataSize = N * sizeof(float);
        pipe::g_meta[f] = pipe::g_meta0[f];
    }
}

std::vector<Way> ways() {
    const auto none = [](const std::string&) {};
    const auto cachedLoad = [](const std::string& d) {
        XpeImageBuffer v{};
        return xpe_calib_load_gain_cached(gainPath(d).c_str(), &v);
    };
    return {
        {"xpe_calib_load_gain", false, none,
         [](const std::string& d) { return xpe_calib_load_gain(gainPath(d).c_str()); }},
        {"xpe_calib_load_gain_cached (miss)", false, none, cachedLoad},
        {"xpe_calib_load_gain_cached (hit)", true, [cachedLoad](const std::string& d) { cachedLoad(d); }, cachedLoad},
        {"xpe_preprocess_pipeline", false, none,
         [](const std::string& d) {
             freshFrames();
             return xpe_preprocess_pipeline(&pipe::g_img[0], &pipe::g_meta[0], d.c_str(), nullptr, pipe::kConfig);
         }},
        {"xpe_preprocess_pipeline_batch", false, none,
         [](const std::string& d) {
             freshFrames();
             return xpe_preprocess_pipeline_batch(pipe::g_img, 1, pipe::g_meta, d.c_str(), nullptr, pipe::kConfig);
         }},
    };
}

/** Set A (with quality) is current; then `dir` is made current by `way`. Returns after the load. */
void aThenOther(const Way& way, const char* dir) {
    pipe::setup();
    way.prepare(dir);                              // the hit way warms the cache for `dir` first
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset("oom_pipe_calibQA/offset.xcal"));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_pipe_calibQA/gain.xcal"));
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map("oom_pipe_calibQA/defect.xcal"));
    const XpeCalibQualityMeta a = current();
    EXPECT_EQ(1u, a.valid) << "control: set A's quality is the record";
    EXPECT_DOUBLE_EQ(0.91, a.r_squared);
    g_hitChecks.store(0);
    xpe_cache_after_open_check_hook = &countHit;
    const XpeErrorCode rc = way.load(dir);
    xpe_cache_after_open_check_hook = nullptr;
    ASSERT_EQ(XPE_OK, rc);
    if (way.isHit) ASSERT_GT(g_hitChecks.load(), 0L) << "control: the load was a cache hit";
}

}  // namespace qcur

TEST_F(OomPipeline, ALoadOfAGainWithoutQualityLeavesNoPreviousFilesQualityBehind) {
    qcur::writeAll();
    for (const auto& way : qcur::ways()) {
        SCOPED_TRACE(way.name);
        qcur::aThenOther(way, qcur::kDirNone);
        const XpeCalibQualityMeta q = qcur::current();
        EXPECT_EQ(0u, q.valid) << "the current gain has no quality metadata, and the record must say so";
        EXPECT_DOUBLE_EQ(0.0, q.r_squared) << "the previous file's R2 is still reported as the current one";
        EXPECT_EQ(0u, q.polynomial_degree);
        EXPECT_EQ(0u, q.num_points);
        EXPECT_EQ(0u, q.calibration_mode);
        EXPECT_DOUBLE_EQ(0.91, q.previous_r_squared) << "the history keeps set A's R2, apart from the current record";
    }
    qcur::removeAll();
}

TEST_F(OomPipeline, ALoadOfAGainWithDifferentQualityReplacesTheRecordOnEveryPath) {
    qcur::writeAll();
    for (const auto& way : qcur::ways()) {
        SCOPED_TRACE(way.name);
        qcur::aThenOther(way, qcur::kDirC);
        const XpeCalibQualityMeta q = qcur::current();
        EXPECT_EQ(1u, q.valid);
        EXPECT_DOUBLE_EQ(0.97, q.r_squared);
        EXPECT_EQ(2u, q.polynomial_degree);
        EXPECT_EQ(4u, q.num_points);
        EXPECT_EQ(3u, q.calibration_mode);
        EXPECT_DOUBLE_EQ(0.91, q.previous_r_squared);
    }
    qcur::removeAll();
}

TEST_F(OomPipeline, AFileWithoutQualityThenOneWithQualityKeepsTheHistoryAcrossTheGap) {
    qcur::writeAll();
    pipe::setup();
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_pipe_calibQA/gain.xcal"));      // record: A
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_pipe_calibQN/gain.xcal"));      // record: none, history 0.91
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_pipe_calibQC/gain.xcal"));      // record: C, previous is still A's
    const XpeCalibQualityMeta q = qcur::current();
    EXPECT_EQ(1u, q.valid);
    EXPECT_DOUBLE_EQ(0.97, q.r_squared);
    EXPECT_DOUBLE_EQ(0.91, q.previous_r_squared) << "a file with no quality in between does not erase the history";
    qcur::removeAll();
}

// The file-quality copy (gain_quality) is what the cache publishes with the map. A hit used to install the map and
// the shared record but leave gain_quality as the previously current file's.
TEST_F(OomPipeline, ACacheHitInstallsTheFilesQualityCopyBesideTheMap) {
    qcur::writeAll();
    struct Row { const char* dir; float gain; bool hasQuality; double r2; };
    for (const Row& row : {Row{qcur::kDirC, 3.0f, true, 0.97}, Row{qcur::kDirNone, 4.0f, false, 0.0}}) {
        SCOPED_TRACE(row.dir);
        const qcur::Way hit = qcur::ways()[2];             // "xpe_calib_load_gain_cached (hit)"; a COPY: ways() is a temporary
        ASSERT_TRUE(hit.isHit);
        qcur::aThenOther(hit, row.dir);
        std::lock_guard<std::mutex> lk(g_calib_mutex);
        ASSERT_TRUE(g_calib.gain_map != nullptr);
        EXPECT_FLOAT_EQ(row.gain, g_calib.gain_map.get()[0]) << "the hit installed this file's map";
        EXPECT_EQ(row.hasQuality, g_calib.gain_has_quality) << "the quality copy says whether THIS file carries one";
        EXPECT_DOUBLE_EQ(row.r2, g_calib.gain_quality.r_squared) << "and holds this file's values, not the previous file's";
    }
    qcur::removeAll();
}

/* =========================================================================
 * The R2 history survives records that have no R2 (QA-A-202f, Codex #41)
 * ========================================================================= */

// previous_r_squared is "the R2 of the last record that HAD one". A gain file may carry quality fields without
// fit_r_squared: its record is valid (the other fields are real) but its R2 is the no-data value -1.0. The history
// used to be taken from any VALID record, so such a record, or a "no quality" one after it, replaced the last
// known R2 by -1.0. The history now moves on only from a record whose R2 is known (valid, and inside the documented
// range 0..1); the three places that chain it -- a generated record, a gain load with quality, a gain load
// without -- follow the same rule.

namespace qhist {

/** A record is made current by `way`, with set A (R2 0.91) current before it. */
void startFromA(const qcur::Way& way, std::initializer_list<const char*> warm) {
    pipe::setup();
    for (const char* d : warm) way.prepare(d);                // the hit way warms the cache for every set it will load
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset("oom_pipe_calibQA/offset.xcal"));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_pipe_calibQA/gain.xcal"));
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map("oom_pipe_calibQA/defect.xcal"));
    const XpeCalibQualityMeta a = qcur::current();
    EXPECT_EQ(1u, a.valid);
    EXPECT_DOUBLE_EQ(0.91, a.r_squared);
}

}  // namespace qhist

TEST_F(OomPipeline, APartialQualityRecordDoesNotInterruptTheR2History_AThenPartialThenC) {
    qcur::writeAll();
    for (const auto& way : qcur::ways()) {
        SCOPED_TRACE(way.name);
        qhist::startFromA(way, {qcur::kDirP, qcur::kDirC});
        ASSERT_EQ(XPE_OK, way.load(qcur::kDirP));
        const XpeCalibQualityMeta b = qcur::current();
        EXPECT_EQ(1u, b.valid) << "the partial file's other quality fields are real";
        EXPECT_EQ(2u, b.polynomial_degree);
        EXPECT_DOUBLE_EQ(-1.0, b.r_squared) << "there is no R2 in that file";
        EXPECT_DOUBLE_EQ(0.91, b.previous_r_squared) << "the history is A's R2";
        EXPECT_EQ(0u, b.has_r_squared) << "the file gave no fit_r_squared: there is no R2 (the -1.0 is the fill value)";
        EXPECT_EQ(1u, b.has_previous_r_squared);
        ASSERT_EQ(XPE_OK, way.load(qcur::kDirC));
        const XpeCalibQualityMeta c = qcur::current();
        EXPECT_EQ(1u, c.has_r_squared);
        EXPECT_EQ(1u, c.has_previous_r_squared);
        EXPECT_DOUBLE_EQ(0.97, c.r_squared);
        EXPECT_DOUBLE_EQ(0.91, c.previous_r_squared) << "the partial record in between must not turn the history into -1.0";
    }
    qcur::removeAll();
}

TEST_F(OomPipeline, ARecordWithNoQualityAfterAPartialOneKeepsTheR2History_AThenPartialThenNoneThenC) {
    qcur::writeAll();
    for (const auto& way : qcur::ways()) {
        SCOPED_TRACE(way.name);
        qhist::startFromA(way, {qcur::kDirP, qcur::kDirNone, qcur::kDirN2, qcur::kDirC});
        ASSERT_EQ(XPE_OK, way.load(qcur::kDirP));
        ASSERT_EQ(XPE_OK, way.load(qcur::kDirNone));
        XpeCalibQualityMeta d = qcur::current();
        EXPECT_EQ(0u, d.valid);
        EXPECT_DOUBLE_EQ(0.91, d.previous_r_squared) << "A -> partial -> none: the last known R2 is still A's";
        ASSERT_EQ(XPE_OK, way.load(qcur::kDirN2));
        d = qcur::current();
        EXPECT_EQ(0u, d.valid);
        EXPECT_DOUBLE_EQ(0.91, d.previous_r_squared) << "two records without quality in a row";
        ASSERT_EQ(XPE_OK, way.load(qcur::kDirC));
        EXPECT_DOUBLE_EQ(0.91, qcur::current().previous_r_squared);
    }
    qcur::removeAll();
}

TEST_F(OomPipeline, AGeneratedRecordFollowsTheSameRule_AThenPartialThenGeneratedThenPartialThenC) {
    qcur::writeAll();
    pipe::setup();
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_pipe_calibQA/gain.xcal"));            // A: R2 0.91
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_pipe_calibQP/gain.xcal"));            // partial: no R2
    XpeCalibQualityMeta g{};
    g.calibration_mode = 3; g.polynomial_degree = 2; g.num_points = 4; g.r_squared = 0.99;
    xpe_calib_record_quality_meta(g);                                                // a generation: R2 0.99
    XpeCalibQualityMeta q = qcur::current();
    EXPECT_EQ(1u, q.valid);
    EXPECT_DOUBLE_EQ(0.99, q.r_squared);
    EXPECT_DOUBLE_EQ(0.91, q.previous_r_squared) << "the generation's history skips the partial record: A's R2";
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_pipe_calibQP/gain.xcal"));            // partial again
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_pipe_calibQC/gain.xcal"));            // C: R2 0.97
    q = qcur::current();
    EXPECT_DOUBLE_EQ(0.97, q.r_squared);
    EXPECT_DOUBLE_EQ(0.99, q.previous_r_squared) << "the last known R2 is the generation's";
    qcur::removeAll();
}

// A real R2 can be negative (a fit worse than the mean: the generator reports -0.0766 for the poly-fixture ladder).
// Only the -1.0 "not given" value means "no R2", so a negative generated R2 is the history of the record after it
// (QA-A-208c; the first form of the rule, "R2 >= 0", threw such a value away).
TEST_F(OomPipeline, ANegativeGeneratedR2IsKnownAndBecomesTheHistory) {
    qcur::writeAll();
    pipe::setup();
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_pipe_calibQA/gain.xcal"));            // A: R2 0.91
    XpeCalibQualityMeta g{};
    g.calibration_mode = 3; g.polynomial_degree = 2; g.num_points = 4; g.r_squared = -0.25;
    xpe_calib_record_quality_meta(g);                                                // a generation: R2 -0.25
    XpeCalibQualityMeta q = qcur::current();
    EXPECT_EQ(1u, q.valid);
    EXPECT_DOUBLE_EQ(-0.25, q.r_squared);
    EXPECT_DOUBLE_EQ(0.91, q.previous_r_squared);
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_pipe_calibQC/gain.xcal"));            // C: R2 0.97
    q = qcur::current();
    EXPECT_DOUBLE_EQ(0.97, q.r_squared);
    EXPECT_DOUBLE_EQ(-0.25, q.previous_r_squared) << "-0.25 is a real R2, not the no-data value";
    qcur::removeAll();
}

// QA-A-208d (Codex #45): whether a record has an R2 is a flag, not a value. A fit worse than the mean by exactly the
// SS_tot (SS_res = 2 * SS_tot) is -1.0, and the generator prints it as -1.000000000: the file loads, the record
// has an R2, and the history chain passes it on as a real value. The first form of the rule used -1.0 as the "not
// given" marker, so such a file could not be loaded and the history could not tell it from "none".
TEST_F(OomPipeline, AnR2OfExactlyMinusOneIsARealValueAndChainsAsOne) {
    qcur::writeAll();
    for (const auto& way : qcur::ways()) {
        SCOPED_TRACE(way.name);
        qhist::startFromA(way, {qcur::kDirM1, qcur::kDirC, qcur::kDirNone, qcur::kDirP});
        ASSERT_EQ(XPE_OK, way.load(qcur::kDirM1));
        const XpeCalibQualityMeta b = qcur::current();
        EXPECT_EQ(1u, b.valid);
        EXPECT_EQ(1u, b.has_r_squared) << "the key is there: the file gave an R2, and it is -1.0";
        EXPECT_DOUBLE_EQ(-1.0, b.r_squared);
        EXPECT_DOUBLE_EQ(0.91, b.previous_r_squared);
        EXPECT_EQ(1u, b.has_previous_r_squared);
        ASSERT_EQ(XPE_OK, way.load(qcur::kDirC));
        const XpeCalibQualityMeta c = qcur::current();
        EXPECT_DOUBLE_EQ(0.97, c.r_squared);
        EXPECT_DOUBLE_EQ(-1.0, c.previous_r_squared) << "B's R2 is -1.0 and it is a real value: C's history";
        EXPECT_EQ(1u, c.has_previous_r_squared) << "...and the flag says it is one";
        // through a record with no quality at all, and through a partial one
        ASSERT_EQ(XPE_OK, way.load(qcur::kDirM1));
        ASSERT_EQ(XPE_OK, way.load(qcur::kDirNone));
        const XpeCalibQualityMeta d = qcur::current();
        EXPECT_EQ(0u, d.valid);
        EXPECT_DOUBLE_EQ(-1.0, d.previous_r_squared);
        EXPECT_EQ(1u, d.has_previous_r_squared);
        ASSERT_EQ(XPE_OK, way.load(qcur::kDirM1));
        ASSERT_EQ(XPE_OK, way.load(qcur::kDirP));
        const XpeCalibQualityMeta e = qcur::current();
        EXPECT_EQ(0u, e.has_r_squared);
        EXPECT_DOUBLE_EQ(-1.0, e.previous_r_squared);
        EXPECT_EQ(1u, e.has_previous_r_squared);
    }
    qcur::removeAll();
}

TEST_F(OomPipeline, WithNoEarlierR2TheHistoryIsFlaggedAsNoneEvenWhenTheFirstRecordIsMinusOne) {
    qcur::writeAll();
    pipe::setup();
    XpeCalibQualityMeta q = qcur::current();
    EXPECT_EQ(0u, q.has_previous_r_squared) << "start-up: no record at all";
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_pipe_calibQZ/gain.xcal"));            // the first record: R2 -1.0
    q = qcur::current();
    EXPECT_EQ(1u, q.has_r_squared);
    EXPECT_EQ(0u, q.has_previous_r_squared) << "nothing earlier had an R2";
    EXPECT_DOUBLE_EQ(-1.0, q.previous_r_squared) << "the fill value";
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_pipe_calibQC/gain.xcal"));            // C: R2 0.97
    q = qcur::current();
    EXPECT_EQ(1u, q.has_previous_r_squared) << "B had one: -1.0";
    EXPECT_DOUBLE_EQ(-1.0, q.previous_r_squared);
    // a generation of exactly -1.0 chains the same way
    XpeCalibQualityMeta g{};
    g.calibration_mode = 3; g.polynomial_degree = 2; g.num_points = 4; g.r_squared = -1.0;
    xpe_calib_record_quality_meta(g);
    q = qcur::current();
    EXPECT_EQ(1u, q.has_r_squared);
    EXPECT_DOUBLE_EQ(-1.0, q.r_squared);
    EXPECT_DOUBLE_EQ(0.97, q.previous_r_squared);
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_pipe_calibQA/gain.xcal"));
    q = qcur::current();
    EXPECT_DOUBLE_EQ(-1.0, q.previous_r_squared) << "the generation's -1.0 is a real R2";
    EXPECT_EQ(1u, q.has_previous_r_squared);
    qcur::removeAll();
}

/* =========================================================================
 * xpe_nonlinearity_correct, the stand-alone entry point (QA-A-209b, Codex #49 item 4; QA-A-201 survey)
 * ========================================================================= */
//
// It had no guard, and the parsing of its configuration allocates (QA-A-209): a failed allocation was an exception
// out of a C ABI function. It now returns XPE_ERR_OUT_OF_MEMORY and leaves the frame as it found it.

namespace q209b {

XpeImageBuffer buf(void* d, XpePixelFormat f, uint32_t bits) {
    XpeImageBuffer b{};
    b.data = d; b.width = W; b.height = H; b.bitsAllocated = bits; b.bitsStored = bits; b.format = f;
    b.dataSize = static_cast<uint32_t>(N * (bits / 8));
    return b;
}

}  // namespace q209b

TEST_F(OomInjection, ANonlinearityCorrectionThatFailsLeavesTheFrameUntouchedAndNoExceptionEscapes) {
    static const std::string cfg = "{\"panel.nonlinearity_mode\":\"POLY\",\"panel.nonlin_poly_c1\":2.0,\"panel.adc_max\":65535}";
    static std::vector<uint16_t> px(N);
    sweep("xpe_nonlinearity_correct", [] { xpe_preprocess_init(nullptr); },
          [&] {
              std::fill(px.begin(), px.end(), static_cast<uint16_t>(1000));
              XpeImageBuffer img = q209b::buf(px.data(), XPE_PIXEL_UINT16, 16);
              return xpe_nonlinearity_correct(&img, cfg.c_str());
          },
          /*unchangedOnError=*/false,
          [&](XpeErrorCode rc) -> std::string {
              bool same = true, doubled = true;
              for (const uint16_t v : px) { same = same && v == 1000; doubled = doubled && v == 2000; }
              if (rc == XPE_OK) return doubled ? std::string() : "the polynomial was not applied";
              if (rc != XPE_ERR_OUT_OF_MEMORY) return "an allocation failure was reported as another error";
              return same ? std::string() : "the frame was changed although the call failed";
          });
}

TEST_F(OomInjection, ANonlinearityCorrectionWithTheLutPathThatFailsLeavesTheFrameUntouched) {
    // the configuration names no polynomial, a table is loaded: the LUT path (a clamp-free identity-halving table)
    {
        std::vector<uint16_t> lut(4096u);
        for (uint32_t i = 0; i < 4096u; ++i) lut[i] = static_cast<uint16_t>(i / 2u);
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION; hdr.type = static_cast<uint32_t>(XCAL_TYPE_NONLIN_LUT);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT16);
        hdr.width = 4096; hdr.height = 1; hdr.payload_len = lut.size() * sizeof(uint16_t);
        hdr.created_epoch_ms = 1700000000000ll;
        std::remove("oom_nlut_halving.xcal");
        ASSERT_EQ(XPE_OK, write_xcal_file("oom_nlut_halving.xcal", hdr, reinterpret_cast<const uint8_t*>("{}"), 2,
                                          reinterpret_cast<const uint8_t*>(lut.data()), lut.size() * sizeof(uint16_t)));
    }
    static const std::string cfg = "{\"panel.linear\":\"false\",\"panel.target_platform\":\"CPU\"}";
    static std::vector<uint16_t> px(N);
    sweep("xpe_nonlinearity_correct (table)",
          [] { xpe_preprocess_init(nullptr); EXPECT_EQ(XPE_OK, xpe_calib_load_nonlin_lut("oom_nlut_halving.xcal")); },
          [&] {
              std::fill(px.begin(), px.end(), static_cast<uint16_t>(1000));
              XpeImageBuffer img = q209b::buf(px.data(), XPE_PIXEL_UINT16, 16);
              return xpe_nonlinearity_correct(&img, cfg.c_str());
          },
          /*unchangedOnError=*/false,
          [&](XpeErrorCode rc) -> std::string {
              bool same = true, halved = true;
              for (const uint16_t v : px) { same = same && v == 1000; halved = halved && v == 500; }
              if (rc == XPE_OK) return halved ? std::string() : "the table was not applied";
              if (rc != XPE_ERR_OUT_OF_MEMORY) return "an allocation failure was reported as another error";
              return same ? std::string() : "the frame was changed although the call failed";
          });
    std::remove("oom_nlut_halving.xcal");
}

/* =========================================================================
 * The exports that left an allocation failure unguarded (QA-A-204 3/3, QA-A-201 survey item 5)
 * ========================================================================= */
//
// xpe_verify_offset / _gain / _pipeline, xpe_bpm_generate, xpe_defect_detect_runtime,
// xpe_calib_generate_nonlin_lut and xpe_nonlinearity_correct had no guard: a failed allocation was an exception out
// of a C ABI function. Two more things made it worse than that. Seven helpers were declared noexcept and allocate
// (the sort copy and the histogram of the verify metrics, the five working-buffer builders of the BPM generator), so a
// failed allocation in them is std::terminate, not an exception. And a call that dies half way leaves what it had
// written: the verify functions clear and then fill the caller's metrics. A failed call now returns
// XPE_ERR_OUT_OF_MEMORY and leaves what a refused call leaves -- the metrics cleared (the contract of every other
// error return of those functions), an output buffer or file untouched.

namespace q204c {

XpeImageBuffer buf(void* d, XpePixelFormat f, uint32_t bits) {
    XpeImageBuffer b{};
    b.data = d; b.width = W; b.height = H; b.bitsAllocated = bits; b.bitsStored = bits; b.format = f;
    b.dataSize = static_cast<uint32_t>(N * (bits / 8));
    return b;
}

XpeCalibrationMetrics g_metrics{};
XpeCalibrationMetrics g_baseline{};

bool sameBytes(const double& a, const double& b) { return std::memcmp(&a, &b, sizeof(double)) == 0; }

bool sameMetrics(const XpeCalibrationMetrics& a, const XpeCalibrationMetrics& b) {
    return sameBytes(a.dark_bias, b.dark_bias) && sameBytes(a.dsnu, b.dsnu) &&
           sameBytes(a.residual_noise, b.residual_noise) && sameBytes(a.prnu_before, b.prnu_before) &&
           sameBytes(a.prnu_after, b.prnu_after) && sameBytes(a.flatness_pct, b.flatness_pct) &&
           sameBytes(a.gain_coverage, b.gain_coverage) && a.invalid_gain_count == b.invalid_gain_count &&
           a.defect_count == b.defect_count && sameBytes(a.defect_density, b.defect_density) &&
           sameBytes(a.correction_error, b.correction_error) && sameBytes(a.snr_improvement_db, b.snr_improvement_db) &&
           a.overall_pass == b.overall_pass && sameBytes(a.dark_reduction_db, b.dark_reduction_db) &&
           sameBytes(a.dsnu_adu, b.dsnu_adu) && a.measured_mask == b.measured_mask;
}

bool isCleared(const XpeCalibrationMetrics& m) {
    XpeCalibrationMetrics z{};
    return sameMetrics(m, z);
}

void fillGarbage(XpeCalibrationMetrics* m) { std::memset(m, 0x5A, sizeof(*m)); }

/** The metrics sweep: the call leaves g_metrics; a failure leaves it cleared, a success leaves exactly the baseline. */
void sweepMetrics(const char* label, const Call& call) {
    sweep(label, [] {}, [&] { fillGarbage(&g_metrics); return call(); }, /*unchangedOnError=*/false,
          [](XpeErrorCode rc) -> std::string {
              if (rc == XPE_OK) return sameMetrics(g_metrics, g_baseline) ? std::string() : "the result differs from the call that failed nothing";
              if (rc != XPE_ERR_OUT_OF_MEMORY) return "an allocation failure was reported as another error";
              return isCleared(g_metrics) ? std::string() : "the metrics were left half filled";
          });
}

}  // namespace q204c

TEST_F(OomInjection, AVerifyOffsetThatFailsLeavesTheMetricsClearedAndNoExceptionEscapes) {
    using namespace q204c;
    static std::vector<uint16_t> raw(N), cor(N);
    for (size_t i = 0; i < N; ++i) {
        raw[i] = static_cast<uint16_t>(100 + (i % 4) * 50 + (i / 4) * 7);
        cor[i] = static_cast<uint16_t>(raw[i] > 90 ? raw[i] - 90 : 0);
    }
    XpeImageBuffer r = buf(raw.data(), XPE_PIXEL_UINT16, 16), c = buf(cor.data(), XPE_PIXEL_UINT16, 16);
    fillGarbage(&g_baseline);
    ASSERT_EQ(XPE_OK, xpe_verify_offset(&r, &c, nullptr, &g_baseline));
    ASSERT_NE(0u, g_baseline.measured_mask) << "control: the baseline measured something";
    sweepMetrics("xpe_verify_offset", [&] { return xpe_verify_offset(&r, &c, nullptr, &g_metrics); });
}

TEST_F(OomInjection, AVerifyGainThatFailsLeavesTheMetricsClearedAndNoExceptionEscapes) {
    using namespace q204c;
    static std::vector<uint16_t> before(N);
    static std::vector<float> after(N), gain(N, 1.0f);
    for (size_t i = 0; i < N; ++i) {
        before[i] = static_cast<uint16_t>(1000 + (i % 4) * 40);
        after[i] = 1000.0f + static_cast<float>(i % 3);
    }
    XpeImageBuffer b = buf(before.data(), XPE_PIXEL_UINT16, 16), a = buf(after.data(), XPE_PIXEL_FLOAT32, 32),
                   g = buf(gain.data(), XPE_PIXEL_FLOAT32, 32);
    fillGarbage(&g_baseline);
    ASSERT_EQ(XPE_OK, xpe_verify_gain(&b, &a, &g, XPE_GAIN_SEMANTICS_UNKNOWN, &g_baseline));
    ASSERT_NE(0u, g_baseline.measured_mask) << "control: the baseline measured something";
    sweepMetrics("xpe_verify_gain", [&] { return xpe_verify_gain(&b, &a, &g, XPE_GAIN_SEMANTICS_UNKNOWN, &g_metrics); });
}

TEST_F(OomInjection, AVerifyPipelineThatFailsLeavesTheMetricsClearedAndNoExceptionEscapes) {
    using namespace q204c;
    static std::vector<uint16_t> raw(N);
    static std::vector<float> fin(N);
    for (size_t i = 0; i < N; ++i) {
        raw[i] = static_cast<uint16_t>(1000 + (i % 4) * 60);
        fin[i] = 1000.0f + static_cast<float>(i % 3);
    }
    XpeImageBuffer r = buf(raw.data(), XPE_PIXEL_UINT16, 16), f = buf(fin.data(), XPE_PIXEL_FLOAT32, 32);
    fillGarbage(&g_baseline);
    ASSERT_EQ(XPE_OK, xpe_verify_pipeline(&r, &f, nullptr, &g_baseline));
    ASSERT_NE(0u, g_baseline.measured_mask) << "control: the baseline measured something";
    sweepMetrics("xpe_verify_pipeline", [&] { return xpe_verify_pipeline(&r, &f, nullptr, &g_metrics); });
}

TEST_F(OomInjection, AVerifyDefectAllocatesNothingSoThereIsNothingToGuard) {
    using namespace q204c;
    static std::vector<float> img(N, 1000.0f);
    static std::vector<uint8_t> map(N, 0);
    map[3] = 1;
    XpeImageBuffer i = buf(img.data(), XPE_PIXEL_FLOAT32, 32), m = buf(map.data(), XPE_PIXEL_UINT8, 8);
    XpeCalibrationMetrics out{};
    arm(1000000000L);
    XpeErrorCode rc = XPE_OK;
    try { rc = std::function<XpeErrorCode()>([&] { return xpe_verify_defect(&i, &m, &out); })(); } catch (...) { rc = XPE_ERR_INTERNAL; }
    const long allocations = g_count.load();
    disarm();
    EXPECT_EQ(XPE_OK, rc);
    EXPECT_EQ(0, allocations);
}

TEST_F(OomInjection, ABpmGenerationThatFailsLeavesTheOutputMapUntouchedAndNoExceptionEscapes) {
    static std::vector<std::vector<uint16_t>> dark(5, std::vector<uint16_t>(N, 100)), bright(10, std::vector<uint16_t>(N, 3000));
    dark[1][5] = 3000;                      // a hot pixel, so the map is not all zero
    bright[2][9] = 5;                       // a dead one
    static std::vector<XpeImageBuffer> d, b;
    d.clear(); b.clear();
    for (auto& f : dark) d.push_back(q204c::buf(f.data(), XPE_PIXEL_UINT16, 16));
    for (auto& f : bright) b.push_back(q204c::buf(f.data(), XPE_PIXEL_UINT16, 16));
    static std::vector<uint8_t> out(N), baseline(N);
    XpeImageBuffer o = q204c::buf(out.data(), XPE_PIXEL_UINT8, 8);
    std::fill(out.begin(), out.end(), static_cast<uint8_t>(0xAB));
    ASSERT_EQ(XPE_OK, xpe_bpm_generate(d.data(), 5, b.data(), 10, nullptr, &o));
    baseline = out;
    sweep("xpe_bpm_generate", [] {},
          [&] {
              std::fill(out.begin(), out.end(), static_cast<uint8_t>(0xAB));
              return xpe_bpm_generate(d.data(), 5, b.data(), 10, nullptr, &o);
          },
          /*unchangedOnError=*/false,
          [&](XpeErrorCode rc) -> std::string {
              if (rc == XPE_OK) return out == baseline ? std::string() : "the map differs from an undisturbed run";
              if (rc != XPE_ERR_OUT_OF_MEMORY) return "an allocation failure was reported as another error";
              for (const uint8_t v : out) if (v != 0xAB) return "the output map was written although the call failed";
              return std::string();
          });
}

TEST_F(OomInjection, ARuntimeDetectionThatFailsLeavesTheOutputMapUntouchedAndNoExceptionEscapes) {
    static std::vector<float> img(N, 1000.0f);
    img[5] = 9000.0f;
    static std::vector<uint8_t> out(N), baseline(N);
    XpeImageBuffer i = q204c::buf(img.data(), XPE_PIXEL_FLOAT32, 32), o = q204c::buf(out.data(), XPE_PIXEL_UINT8, 8);
    std::fill(out.begin(), out.end(), static_cast<uint8_t>(0xAB));
    ASSERT_EQ(XPE_OK, xpe_defect_detect_runtime(&i, nullptr, &o));
    baseline = out;
    sweep("xpe_defect_detect_runtime", [] {},
          [&] {
              std::fill(out.begin(), out.end(), static_cast<uint8_t>(0xAB));
              return xpe_defect_detect_runtime(&i, nullptr, &o);
          },
          /*unchangedOnError=*/false,
          [&](XpeErrorCode rc) -> std::string {
              if (rc == XPE_OK) return out == baseline ? std::string() : "the map differs from an undisturbed run";
              if (rc != XPE_ERR_OUT_OF_MEMORY) return "an allocation failure was reported as another error";
              for (const uint8_t v : out) if (v != 0xAB) return "the output map was written although the call failed";
              return std::string();
          });
}

TEST_F(OomInjection, ANonlinearityLutGenerationThatFailsWritesNoFileAndNoExceptionEscapes) {
    const char* path = "oom_nlut_out.xcal";
    static const std::string tmpPath = std::string(path) + ".tmp";
    constexpr int kLevels = 10;
    static std::vector<std::vector<uint16_t>> flats(kLevels);
    static std::vector<XpeImageBuffer> bufs;
    static double doses[kLevels];
    bufs.clear();
    for (int l = 0; l < kLevels; ++l) {
        doses[l] = 100.0 * (l + 1);
        flats[l].assign(N, static_cast<uint16_t>(300 * (l + 1)));       // a linear response: gain 3
        bufs.push_back(q204c::buf(flats[l].data(), XPE_PIXEL_UINT16, 16));
    }
    std::remove(path); std::remove((std::string(path) + ".tmp").c_str());
    ASSERT_EQ(XPE_OK, xpe_calib_generate_nonlin_lut(bufs.data(), doses, kLevels, nullptr, 4096u, path, nullptr));
    ASSERT_TRUE(std::filesystem::exists(path)) << "control: the undisturbed call wrote the table";
    std::remove(path);
    sweep("xpe_calib_generate_nonlin_lut", [] {},
          [&] {
              std::remove(path); std::remove(tmpPath.c_str());   // tmpPath is built outside: nothing in here may allocate
              return xpe_calib_generate_nonlin_lut(bufs.data(), doses, kLevels, nullptr, 4096u, path, nullptr);
          },
          /*unchangedOnError=*/false,
          [&](XpeErrorCode rc) -> std::string {
              const bool file = std::filesystem::exists(path), tmp = std::filesystem::exists(std::string(path) + ".tmp");
              std::remove(path); std::remove((std::string(path) + ".tmp").c_str());
              if (rc == XPE_OK) return file ? std::string() : "the call succeeded and wrote no file";
              if (rc != XPE_ERR_OUT_OF_MEMORY) return "an allocation failure was reported as another error";
              return (file || tmp) ? "a failed call left a file behind" : std::string();
          });
}

/* =========================================================================
 * An XCal config block is parsed ONCE per load (QA-A-209c, Codex #53 item 2)
 * ========================================================================= */
//
// read_xcal_file parsed the block to read the compression metadata and threw the document away; the gain and
// nonlinearity-table loaders then parsed the same text again for the quality fields, the dose range and the
// extension start. The reader now hands its document to the loader. xpe_config_parse_calls counts every parse
// (compiled into this executable only, with XPE_CACHE_TEST_HOOKS).

namespace q209c {

unsigned long parsesDuring(const std::function<XpeErrorCode()>& call, XpeErrorCode* rc) {
    const unsigned long before = xpe_config_parse_calls;
    *rc = call();
    return xpe_config_parse_calls - before;
}

}  // namespace q209c

TEST_F(OomInjection, EveryLoaderParsesTheConfigBlockOfAFileExactlyOnce) {
    xpe_preprocess_init(nullptr);
    XpeErrorCode rc = XPE_OK;

    // a gain file whose block carries quality fields, so the loader has something to read from the document
    {
        std::vector<float> g(N, 1.0f);
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION; hdr.type = static_cast<uint32_t>(XCAL_TYPE_GAIN);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32); hdr.width = W; hdr.height = H;
        hdr.created_epoch_ms = 1700000000000ll;
        const std::string cfg = "{\"fit_r_squared\":0.9995,\"polynomial_degree\":1,\"actual_dose_levels\":4,\"calibration_mode\":1}";
        std::remove("oom_q209c_gain.xcal");
        ASSERT_EQ(XPE_OK, write_xcal_file("oom_q209c_gain.xcal", hdr, reinterpret_cast<const uint8_t*>(cfg.data()), cfg.size(),
                                          reinterpret_cast<const uint8_t*>(g.data()), g.size() * sizeof(float)));
    }
    EXPECT_EQ(1ul, q209c::parsesDuring([&] { return xpe_calib_load_gain("oom_q209c_gain.xcal"); }, &rc));
    EXPECT_EQ(XPE_OK, rc);

    // a nonlinearity table with the extension-start key
    {
        std::vector<uint16_t> lut(4096u);
        for (uint32_t i = 0; i < 4096u; ++i) lut[i] = static_cast<uint16_t>(i);
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION; hdr.type = static_cast<uint32_t>(XCAL_TYPE_NONLIN_LUT);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT16); hdr.width = 4096; hdr.height = 1;
        hdr.created_epoch_ms = 1700000000000ll;
        const std::string cfg = "{\"xcal_nonlin_extension_start\":4000}";
        std::remove("oom_q209c_lut.xcal");
        ASSERT_EQ(XPE_OK, write_xcal_file("oom_q209c_lut.xcal", hdr, reinterpret_cast<const uint8_t*>(cfg.data()), cfg.size(),
                                          reinterpret_cast<const uint8_t*>(lut.data()), lut.size() * sizeof(uint16_t)));
    }
    EXPECT_EQ(1ul, q209c::parsesDuring([&] { return xpe_calib_load_nonlin_lut("oom_q209c_lut.xcal"); }, &rc));
    EXPECT_EQ(XPE_OK, rc);

    std::remove("oom_q209c_gain.xcal");
    std::remove("oom_q209c_lut.xcal");
}

/* =========================================================================
 * The legacy-repair alert is built BEFORE any output is touched (QA-A-209d, Codex #56 item 1)
 * ========================================================================= */
//
// read_xcal_file raises a warning when it accepted a file of the older writer's shape. The text is a std::string built
// from the file's path, so it can fail to allocate. It used to be built AFTER the outputs (header, config, payload,
// document) were set to the success values: a failed allocation reported OUT_OF_MEMORY with the caller's variables
// already overwritten. It is now built first, and the alert is pushed -- a call that cannot throw -- after the outputs
// are committed.

TEST_F(OomInjection, ALegacyFileThatFailsToBuildItsWarningLeavesEveryOutputArgumentUntouched) {
    // an old-writer file: a DEFECT map, RLE-compressed, with a caller config -> the block ends "}}"
    const std::string longDir(80, 'd');   // a long path, so the alert text is a real allocation
    const std::string path = "oom_legacy_" + longDir + ".xcal";
    {
        const std::vector<uint8_t> raw(4096u, 0u);
        std::vector<uint8_t> rle;
        ASSERT_EQ(XPE_OK, rle_encode(raw.data(), raw.size(), rle));
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION; hdr.type = static_cast<uint32_t>(XCAL_TYPE_DEFECT);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT8_MASK); hdr.width = 64; hdr.height = 64;
        hdr.created_epoch_ms = 1700000000000ll;
        const std::string legacy = "{\"mode\":\"production\",\"xcal_compression\":1,\"xcal_raw_payload_len\":4096}}";
        std::remove(path.c_str());
        ASSERT_EQ(XPE_OK, write_xcal_file_ex(path.c_str(), hdr, reinterpret_cast<const uint8_t*>(legacy.data()), legacy.size(),
                                             rle.data(), rle.size(), /*compress_defect=*/false));
    }

    static XCalFileHeader outHdr;
    static std::vector<uint8_t> outCfg, outPay;
    static XpeConfigDoc outDoc;
    static XCalFileHeader sentinelHdr;
    std::memset(&sentinelHdr, 0x5A, sizeof(sentinelHdr));
    const std::vector<uint8_t> sentinelCfg{1, 2, 3}, sentinelPay{9, 9};
    auto reset = [&] {
        outHdr = sentinelHdr;
        outCfg = sentinelCfg;
        outPay = sentinelPay;
        outDoc.entries.clear();
        outDoc.entries.push_back(XpeConfigEntry{"sentinel", "kept", true, true});
    };

    // The real sweep: the reset happens before arming, so the armed region is read_xcal_file alone.
    constexpr long kMax = 400;
    long failures = 0;
    for (long k = 1; k <= kMax; ++k) {
        reset();
        xpe_clear_alerts();
        bool escaped = false;
        XpeErrorCode rc = XPE_OK;
        arm(k);
        try {
            rc = read_xcal_file(path.c_str(), outHdr, outCfg, outPay, /*check_expiry=*/false, XCAL_TYPE_DEFECT, &outDoc);
        } catch (...) {
            escaped = true;
        }
        const bool injected = disarm();
        ASSERT_FALSE(escaped) << "allocation #" << k;
        if (rc == XPE_OK) {
            EXPECT_EQ(4096u, outPay.size()) << "a successful read returns the decompressed map (allocation #" << k << ")";
            EXPECT_NE(nullptr, outDoc.find("xcal_compression"));
        } else {
            ++failures;
            EXPECT_EQ(XPE_ERR_OUT_OF_MEMORY, rc) << "allocation #" << k;
            EXPECT_EQ(0, std::memcmp(&outHdr, &sentinelHdr, sizeof(outHdr))) << "the header output was changed by a failed read (allocation #" << k << ")";
            EXPECT_EQ(sentinelCfg, outCfg) << "the config output was changed by a failed read (allocation #" << k << ")";
            EXPECT_EQ(sentinelPay, outPay) << "the payload output was changed by a failed read (allocation #" << k << ")";
            EXPECT_EQ(1u, outDoc.entries.size()) << "the document output was changed by a failed read (allocation #" << k << ")";
            EXPECT_EQ(0, xpe_get_pending_alert_count()) << "a failed read raised its warning (allocation #" << k << ")";
        }
        if (!injected) break;                              // this run made fewer allocations than k: the sweep is complete
    }
    EXPECT_GT(failures, 10) << "control: the sweep reached many failure points";
    std::remove(path.c_str());
}


/* =========================================================================
 * QA-A-211 (#233): the classification paths allocate too -- the scan's index vector, the list the store keeps, the
 * polynomial's per-frame list, the union mask of the defect stage. Each of them fails at every allocation point.
 * 20x20 files: a 4x4 frame cannot hold a bad pixel under the 5% cap.
 * ========================================================================= */
namespace q211 {

constexpr uint32_t kW = 20, kH = 20;
constexpr size_t kN = static_cast<size_t>(kW) * kH;

void writeBig(const char* path, uint32_t type, uint32_t fmt, const void* data, size_t bytes) {
    std::remove(path);
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION; hdr.type = type; hdr.pixel_format = fmt;
    hdr.width = kW; hdr.height = kH; hdr.payload_len = bytes;
    const std::string json = "{}";
    ASSERT_EQ(XPE_OK, write_xcal_file(path, hdr, reinterpret_cast<const uint8_t*>(json.data()), json.size(),
                                      static_cast<const uint8_t*>(data), bytes));
}
void writeScalarWithBad(const char* path) {
    std::vector<float> m(kN, 2.0f);
    m[37] = 0.0f; m[205] = 50.0f;
    writeBig(path, XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32, m.data(), m.size() * sizeof(float));
}
// degree 1, gain = c0 + c1 * x: pixel 37 has all-zero coefficients (classified), the rest are the constant 2.0
void writePolyWithBad(const char* path) {
    std::vector<float> c(kN * 2, 0.0f);
    for (size_t q = 0; q < kN; ++q) c[q * 2] = (q == 37) ? 0.0f : 2.0f;
    writeBig(path, XCAL_TYPE_GAIN_POLY, XCAL_FMT_FLOAT32, c.data(), c.size() * sizeof(float));
}
void writeEmptyDefects(const char* path) {
    std::vector<uint8_t> m(kN, 0);
    writeBig(path, XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, m.data(), m.size());
}

}  // namespace q211

TEST_F(OomInjection, AScalarGainLoadThatClassifiesPixelsAndFailsLeavesTheStoreUntouched) {
    using namespace q211;
    writeGain("oom_q.xcal", 4.0f, "{}");
    writeScalarWithBad("oom_long_entry_file_name.xcal");
    sweep("xpe_calib_load_gain (classified pixels)",
          [] { resetStore(); xpe_calib_cache_clear(); xpe_clear_alerts(); ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_q.xcal")); },
          [] { return xpe_calib_load_gain("oom_long_entry_file_name.xcal"); }, /*unchangedOnError=*/true,
          [](XpeErrorCode rc) -> std::string {
              if (rc != XPE_OK) return {};
              std::lock_guard<std::mutex> lk(g_calib_mutex);
              return g_calib.gain_defect_count == 2 && g_calib.gain_defect_idx ? std::string() : "a load that succeeded lost the classification";
          });
}

TEST_F(OomInjection, ACachedScalarGainHitThatClassifiesPixelsAndFailsLeavesTheStoreUntouched) {
    using namespace q211;
    writeGain("oom_q.xcal", 4.0f, "{}");
    writeScalarWithBad("oom_long_entry_file_name.xcal");
    sweep("xpe_calib_load_gain_cached (hit, classified)",
          [] {
              resetStore(); xpe_calib_cache_clear(); xpe_clear_alerts();
              XpeImageBuffer v{};
              ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached("oom_long_entry_file_name.xcal", &v));   // cached with its list
              ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_q.xcal"));                                 // store = Q, no classification
          },
          [] { XpeImageBuffer v{}; return xpe_calib_load_gain_cached("oom_long_entry_file_name.xcal", &v); }, /*unchangedOnError=*/true,
          [](XpeErrorCode rc) -> std::string {
              if (rc != XPE_OK) return {};
              std::lock_guard<std::mutex> lk(g_calib_mutex);
              return g_calib.gain_defect_count == 2 && g_calib.gain_defect_idx ? std::string() : "a hit that succeeded did not install the classification";
          });
}

TEST_F(OomInjection, ADefectCorrectionThatUnitesTheGainClassificationAndFailsLeavesTheStoreUntouched) {
    using namespace q211;
    writeScalarWithBad("oom_q.xcal");
    writeEmptyDefects("oom_long_entry_file_name.xcal");
    static std::vector<float> in(kN, 1000.0f), out(kN, 0.0f);
    in[37] = 9000.0f; in[205] = 9000.0f;
    sweep("xpe_defect_correct (gain-classified pixels)",
          [] {
              resetStore(); xpe_clear_alerts();
              xpe_preprocess_init(nullptr);
              ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_q.xcal"));
              ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map("oom_long_entry_file_name.xcal"));
          },
          [] {
              XpeImageBuffer i{}, o{};
              i.data = in.data(); i.width = kW; i.height = kH; i.bitsAllocated = 32; i.bitsStored = 32; i.format = XPE_PIXEL_FLOAT32;
              i.dataSize = static_cast<uint32_t>(kN * 4);
              o = i;
              o.data = out.data();
              XpeImageMetadata meta{};
              return xpe_defect_correct(&i, &o, &meta);
          }, /*unchangedOnError=*/true,
          [](XpeErrorCode rc) -> std::string {
              if (rc != XPE_OK) return {};
              return (out[37] == 1000.0f && out[205] == 1000.0f) ? std::string() : "the classified pixels were not corrected";
          });
}

TEST_F(OomInjection, APolynomialGainApplicationThatClassifiesPixelsAndFailsReportsOutOfMemory) {
    using namespace q211;
    writePolyWithBad("oom_q.xcal");
    static std::vector<uint16_t> in(kN, 3000);
    static std::vector<float> out(kN, 0.0f);
    sweep("xpe_gain_correct (polynomial, classified pixels)",
          [] { resetStore(); xpe_clear_alerts(); xpe_preprocess_init(nullptr); ASSERT_EQ(XPE_OK, xpe_calib_load_gain("oom_q.xcal")); },
          [] {
              XpeImageBuffer i{}, o{};
              i.data = in.data(); i.width = kW; i.height = kH; i.bitsAllocated = 16; i.bitsStored = 16; i.format = XPE_PIXEL_UINT16;
              i.dataSize = static_cast<uint32_t>(kN * 2);
              o.data = out.data(); o.width = kW; o.height = kH; o.bitsAllocated = 32; o.bitsStored = 32; o.format = XPE_PIXEL_FLOAT32;
              o.dataSize = static_cast<uint32_t>(kN * 4);
              XpeImageMetadata meta{};
              return xpe_gain_correct(&i, &o, &meta);
          }, /*unchangedOnError=*/true,
          [](XpeErrorCode rc) -> std::string {
              if (rc == XPE_OK) return out[37] == 3000.0f ? std::string() : "the classified pixel did not carry gain 1.0";
              return rc == XPE_ERR_OUT_OF_MEMORY ? std::string() : "an allocation failure was reported as another error";
          });
}
