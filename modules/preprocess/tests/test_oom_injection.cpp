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

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <functional>
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
    const void* gm; const void* gp; uint32_t gnc, gw, gh; int64_t gt, ge; bool ghq; double gqr; unsigned gqd;
    const void* dm; uint32_t dw, dh; int64_t de;
    bool operator==(const Snap& o) const {
        return om == o.om && ow == o.ow && oh == o.oh && ot == o.ot && oe == o.oe &&
               gm == o.gm && gp == o.gp && gnc == o.gnc && gw == o.gw && gh == o.gh && gt == o.gt &&
               ge == o.ge && ghq == o.ghq && gqr == o.gqr && gqd == o.gqd && dm == o.dm && dw == o.dw && dh == o.dh && de == o.de;
    }
};
Snap snap() {
    std::lock_guard<std::mutex> lk(g_calib_mutex);
    return Snap{g_calib.offset_map.get(), g_calib.offset_width, g_calib.offset_height, g_calib.offset_timestamp,
                g_calib.offset_expiry_ms,
                g_calib.gain_map.get(), g_calib.gain_poly_coeffs.get(), g_calib.gain_poly_num_coeffs,
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
