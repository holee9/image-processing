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
#include <cstdint>
#include <cstdio>
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

void arm(long k) {
    g_count.store(0);
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
    if (limit > 0 && g_count.fetch_add(1) + 1 == limit) {
        g_injected.store(true);
        throw std::bad_alloc();
    }
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

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
               const std::string& json) {
    std::remove(path);
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION; hdr.type = type; hdr.pixel_format = fmt;
    hdr.width = W; hdr.height = H; hdr.payload_len = bytes;
    ASSERT_EQ(XPE_OK, write_xcal_file(path, hdr, reinterpret_cast<const uint8_t*>(json.data()), json.size(),
                                      static_cast<const uint8_t*>(data), bytes));
}
void writeOffset(const char* path, float v) {
    std::vector<float> m(N, v);
    writeFile(path, XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32, m.data(), m.size() * sizeof(float), "{}");
}
void writeGain(const char* path, float v, const std::string& json) {
    std::vector<float> m(N, v);
    writeFile(path, XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32, m.data(), m.size() * sizeof(float), json);
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
        for (const char* p : {"oom_long_entry_file_name.xcal", "oom_q.xcal"}) std::remove(p);
    }
};

using Setup = std::function<void()>;
using Call = std::function<XpeErrorCode()>;

/**
 * Fails the K-th allocation of `call` for K = 1.. until a call finishes with fewer allocations.
 * `unchangedOnError`: a call that returns an error must leave the store as the setup left it.
 */
void sweep(const char* label, const Setup& setup, const Call& call, bool unchangedOnError) {
    constexpr long kMax = 3000;
    long injections = 0;
    for (long k = 1; k <= kMax; ++k) {
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

        ASSERT_FALSE(escaped) << label << ": an exception left the C ABI function when allocation #" << k
                              << " failed" << (lockLeaked ? " (and the calibration store lock stayed held)" : "");
        ASSERT_FALSE(lockLeaked) << label << ": the calibration store lock stayed held after allocation #" << k << " failed";
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
