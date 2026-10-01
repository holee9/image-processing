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

#include <atomic>
#include <chrono>
#include <thread>
#include <cstdint>
#include <cstdio>
#include <filesystem>
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
    if (void* p = std::malloc(n ? n : 1)) { g_live.fetch_add(1); return p; }
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return operator new(n); }
void operator delete(void* p) noexcept { if (p) { g_live.fetch_sub(1); std::free(p); } }
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
        // dataSize is the INPUT size, as in every other pipeline test: the temperature stage copies dataSize bytes
        // into a buffer of width*height uint16, so a larger value would overrun it (QA-A-202b: seen as heap
        // corruption when this was N floats). The buffer itself has room for N floats.
        g_img[f].dataSize = N * sizeof(uint16_t);
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
