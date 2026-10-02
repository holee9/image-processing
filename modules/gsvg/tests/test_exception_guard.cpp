/**
 * @file test_exception_guard.cpp
 * @brief No exception leaves xpe_gsvg_process / _process_masked / _process_ex (QA-B-181, QA-B-179, #233).
 *
 * QA-B-179 measured, for all three entry points, an image whose declared size made the passes throw out of the C
 * ABI without any memory pressure:
 *   - width = height = INT_MAX with grid_suppression -> std::length_error (a double working image of 4.6e18
 *     pixels cannot even be represented)
 *   - width = height = 1e9 -> std::bad_alloc (representable, not allocatable)
 * The lengths the caller passes are only compared against width * height (a buffer's real size is unknowable),
 * so a caller that states a huge image and matching huge lengths gets past the length checks. Both are now error
 * codes: an unrepresentable size is INVALID_INPUT, an unallocatable one OUT_OF_MEMORY, and the handle keeps
 * working afterwards.
 *
 * Each failure happens in the first allocation, before the (tiny, real) buffers are read, so the inputs below
 * are safe to pass. The bad_alloc cases need a request larger than the machine's commit limit; where the machine
 * could in principle satisfy it, the test skips rather than allocate.
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <mutex>
#include <utility>

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include "test_data_paths.h"
#include "xpe/gsvg/gsvg_api.h"
#include "../src/parallel_rows.h"

namespace {

const std::string kTablePath = xpe_gsvg_test::Data("virtual_grid_synthetic_table.csv");

std::string VirtualGridConfig() {
    return std::string("{\"virtual_grid\": true, \"vg_table_path\": \"") + kTablePath +
           "\", \"vg_kvp\": 80, \"vg_grid_ratio\": 10, \"vg_pixel_pitch_mm\": 1.0,"
           " \"vg_air_signal\": 60000, \"vg_iterations\": 5}";
}

bool CannotBeCommitted(double bytes) {
    MEMORYSTATUSEX m{};
    m.dwLength = sizeof(m);
    GlobalMemoryStatusEx(&m);
    return bytes > 2.0 * static_cast<double>(m.ullTotalPageFile);
}

enum class Entry { Process, Masked, Ex };

struct Handle {
    void* h = nullptr;
    explicit Handle(const char* cfg) { EXPECT_EQ(XPE_OK, xpe_gsvg_init(&h, cfg)); }
    ~Handle() { xpe_gsvg_shutdown(h); }
};

/** Calls one entry point with the SAME declared size and the SAME (tiny) buffers. true when an exception left. */
bool Call(Entry e, void* h, uint16_t* px, size_t declaredCount, int w, int hgt, XpeErrorCode* rc) {
    try {
        XpeGsvgResult res{};
        res.structSize = sizeof(res);
        switch (e) {
            case Entry::Process: *rc = xpe_gsvg_process(h, px, declaredCount, px, declaredCount, w, hgt, nullptr, 0); break;
            case Entry::Masked:  *rc = xpe_gsvg_process_masked(h, px, declaredCount, px, declaredCount, w, hgt, nullptr, 0, nullptr, 0); break;
            case Entry::Ex:      *rc = xpe_gsvg_process_ex(h, px, declaredCount, px, declaredCount, w, hgt, nullptr, 0, nullptr, 0, &res); break;
        }
        return false;
    } catch (...) {
        return true;
    }
}

const Entry kEntries[] = {Entry::Process, Entry::Masked, Entry::Ex};
const char* Name(Entry e) { return e == Entry::Process ? "process" : e == Entry::Masked ? "process_masked" : "process_ex"; }

}  // namespace

TEST(GsvgExceptionGuard, AnUnrepresentableSizeIsInvalidInputForEveryEntryPoint) {
    Handle hd("{\"grid_suppression\": true}");
    for (Entry e : kEntries) {
        uint16_t px[64];
        for (int i = 0; i < 64; ++i) px[i] = static_cast<uint16_t>(100 + i);
        const std::vector<uint16_t> before(px, px + 64);
        XpeErrorCode rc = XPE_OK;
        const bool escaped = Call(e, hd.h, px, SIZE_MAX, 2147483647, 2147483647, &rc);
        EXPECT_FALSE(escaped) << "an exception left xpe_gsvg_" << Name(e);
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, rc) << Name(e);
        EXPECT_EQ(before, std::vector<uint16_t>(px, px + 64)) << Name(e) << ": the buffer must be untouched";
    }
}

TEST(GsvgExceptionGuard, AnUnallocatableGridSuppressionSizeIsOutOfMemoryForEveryEntryPoint) {
    const double bytes = 8.0 * 1e9 * 1e9;   // the double working image
    if (CannotBeCommitted(bytes) == false) GTEST_SKIP() << "this machine's commit limit could satisfy " << bytes << " bytes";
    Handle hd("{\"grid_suppression\": true}");
    for (Entry e : kEntries) {
        uint16_t px[64];
        for (int i = 0; i < 64; ++i) px[i] = static_cast<uint16_t>(100 + i);
        const std::vector<uint16_t> before(px, px + 64);
        XpeErrorCode rc = XPE_OK;
        const bool escaped = Call(e, hd.h, px, SIZE_MAX, 1000000000, 1000000000, &rc);
        EXPECT_FALSE(escaped) << "an exception left xpe_gsvg_" << Name(e);
        EXPECT_EQ(XPE_ERR_OUT_OF_MEMORY, rc) << Name(e);
        EXPECT_EQ(before, std::vector<uint16_t>(px, px + 64)) << Name(e) << ": the buffer must be untouched";
    }
}

TEST(GsvgExceptionGuard, AnUnallocatableVirtualGridSizeIsOutOfMemoryForEveryEntryPoint) {
    const double bytes = 2.0 * 1e9 * 1e9;   // the copy kept so the original pixels can be restored
    if (CannotBeCommitted(bytes) == false) GTEST_SKIP() << "this machine's commit limit could satisfy " << bytes << " bytes";
    Handle hd(VirtualGridConfig().c_str());
    for (Entry e : kEntries) {
        uint16_t px[64];
        for (int i = 0; i < 64; ++i) px[i] = static_cast<uint16_t>(100 + i);
        const std::vector<uint16_t> before(px, px + 64);
        XpeErrorCode rc = XPE_OK;
        const bool escaped = Call(e, hd.h, px, SIZE_MAX, 1000000000, 1000000000, &rc);
        EXPECT_FALSE(escaped) << "an exception left xpe_gsvg_" << Name(e);
        EXPECT_EQ(XPE_ERR_OUT_OF_MEMORY, rc) << Name(e);
        EXPECT_EQ(before, std::vector<uint16_t>(px, px + 64)) << Name(e) << ": the buffer must be untouched";
    }
}

// The handle is still usable after a failed call: a normal image is processed and the code is the normal one.
TEST(GsvgExceptionGuard, TheHandleStaysUsableAfterAFailedCall) {
    Handle hd("{\"grid_suppression\": true}");
    uint16_t big[4] = {1, 2, 3, 4};
    XpeErrorCode rc = XPE_OK;
    EXPECT_FALSE(Call(Entry::Process, hd.h, big, SIZE_MAX, 2147483647, 2147483647, &rc));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, rc);
    std::vector<uint16_t> px(64 * 64, 1000), out(64 * 64, 0);
    EXPECT_EQ(XPE_OK, xpe_gsvg_process(hd.h, px.data(), px.size(), out.data(), out.size(), 64, 64, nullptr, 0));
}

// The new size bound sits ABOVE every honest image: a large but valid frame is not caught by it.
TEST(GsvgExceptionGuard, AnOrdinaryLargeFrameIsNotRejectedBySizeBound) {
    Handle hd("{}");
    const int w = 1024, h = 1024;
    std::vector<uint16_t> px(static_cast<size_t>(w) * h, 1000), out(px.size(), 0);
    EXPECT_EQ(XPE_OK, xpe_gsvg_process(hd.h, px.data(), px.size(), out.data(), out.size(), w, h, nullptr, 0));
}

// ---- QA-B-181c (Codex #37): the row-band split of ForRows ---------------------------------------------------------

TEST(GsvgExceptionGuard, ForRowsPartitionsRowsNearIntMaxWithoutOverflow) {
    // The bodies only record their band, so INT32_MAX rows cost nothing. `(rows + threads - 1) / threads` left int
    // here and produced a negative band size.
    for (int threads : {2, 3, 4, 7, 64}) {
        std::mutex m;
        std::vector<std::pair<int, int>> bands;
        xpe_parallel::ForRows(0x7FFFFFFF, threads, [&](int y0, int y1) {
            std::lock_guard<std::mutex> g(m);
            bands.emplace_back(y0, y1);
        });
        std::sort(bands.begin(), bands.end());
        int64_t next = 0;
        for (const auto& b : bands) {
            EXPECT_EQ(next, b.first) << "threads=" << threads << ": bands must be contiguous";
            EXPECT_LT(b.first, b.second) << "threads=" << threads << ": no empty band";
            next = b.second;
        }
        EXPECT_EQ(0x7FFFFFFF, next) << "threads=" << threads << ": the bands must cover every row";
        EXPECT_FALSE(bands.empty()) << "control: the body ran";
    }
}

TEST(GsvgExceptionGuard, ForRowsNeverPlansMoreBandsThanRows) {
    // A caller-supplied thread count is not trusted: 100000 threads over 10 rows is 10 bands, not 100000 slots.
    std::mutex m;
    std::vector<std::pair<int, int>> bands;
    xpe_parallel::ForRows(10, 100000, [&](int y0, int y1) {
        std::lock_guard<std::mutex> g(m);
        bands.emplace_back(y0, y1);
    });
    std::sort(bands.begin(), bands.end());
    int next = 0;
    for (const auto& b : bands) {
        EXPECT_EQ(next, b.first);
        EXPECT_LT(b.first, b.second);
        next = b.second;
    }
    EXPECT_EQ(10, next);
    EXPECT_LE(bands.size(), 10u);
}

// ---- QA-B-181d (Codex #48 census): two caller-supplied floats reached an integer conversion unchecked ---------

namespace {

std::string VgConfigWithLevels(const char* levels) {
    return std::string("{\"virtual_grid\": true, \"vg_table_path\": \"") + kTablePath +
           "\", \"vg_kvp\": 80, \"vg_grid_ratio\": 10, \"vg_pixel_pitch_mm\": 1.0,"
           " \"vg_air_signal\": 60000, \"vg_iterations\": 5, \"vg_pyramid_levels\": " + levels + "}";
}

XpeErrorCode InitWith(const std::string& cfg) {
    void* h = nullptr;
    const XpeErrorCode rc = xpe_gsvg_init(&h, cfg.c_str());
    if (rc == XPE_OK) xpe_gsvg_shutdown(h);
    return rc;
}

}  // namespace

// vg_pyramid_levels is a JSON number checked only for being integral, then converted to int. 1e10 is integral and
// is no int; the 4..8 range test came AFTER the conversion, on whatever the conversion produced.
TEST(GsvgExceptionGuard, AVirtualGridPyramidLevelCountBeyondIntIsRefusedByTheConfig) {
    ASSERT_EQ(XPE_OK, InitWith(VgConfigWithLevels("6"))) << "control: an ordinary level count is accepted";
    for (const char* bad : {"1e10", "-1e10", "4294967296", "1e300", "9"}) {
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, InitWith(VgConfigWithLevels(bad))) << "vg_pyramid_levels=" << bad;
    }
    EXPECT_EQ(XPE_OK, InitWith(VgConfigWithLevels("0"))) << "0 (no pyramid) stays valid";
    EXPECT_EQ(XPE_OK, InitWith(VgConfigWithLevels("8"))) << "the upper boundary stays valid";
}

// A NaN gain (or inf * 0) made clamp() a no-op -- both comparisons are false -- and the cast of NaN to uint16 undefined.
TEST(GsvgExceptionGuard, AVignetteGainMapWithANonFiniteElementIsInvalidInput) {
    void* h = nullptr;
    ASSERT_EQ(XPE_OK, xpe_gsvg_init(&h, "{\"vignette_correction\": true}"));
    const int w = 16, hgt = 16;
    std::vector<uint16_t> src(w * hgt, 1000), dst(w * hgt, 7);
    std::vector<float> gain(w * hgt, 1.0f);
    EXPECT_EQ(XPE_OK, xpe_gsvg_process(h, src.data(), src.size(), dst.data(), dst.size(), w, hgt, gain.data(), gain.size()))
        << "control: a finite gain map is applied";

    const float bad[] = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                         -std::numeric_limits<float>::infinity()};
    for (float b : bad) {
        std::vector<float> g = gain;
        g[100] = b;
        std::vector<uint16_t> out(w * hgt, 7);
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_gsvg_process(h, src.data(), src.size(), out.data(), out.size(), w, hgt, g.data(), g.size()))
            << "gain=" << b;
        EXPECT_EQ(std::vector<uint16_t>(w * hgt, 7), out) << "a refused call must not write the output, gain=" << b;
    }
    // Large FINITE gains stay legal: the product is clamped to 65535 before the conversion.
    std::vector<float> huge(w * hgt, 1e30f);
    EXPECT_EQ(XPE_OK, xpe_gsvg_process(h, src.data(), src.size(), dst.data(), dst.size(), w, hgt, huge.data(), huge.size()));
    EXPECT_EQ(65535u, dst[0]);
    xpe_gsvg_shutdown(h);
}

// QA-B-181f (#233): the virtual grid works in double, and the last step converts to uint16 with clamp(round(x)). A
// non-finite value reaches that step from FINITE settings: QA-B-181e measured vg_pyramid_gain 1e308 giving zeros and
// 65535s in places with rc=0, where clamp() is a no-op for NaN and the cast of NaN is undefined. The call is refused
// with the config error the other virtual-grid refusals use, and the original image is restored (the existing
// contract of that path); a bad value is not repaired into a plausible pixel.
TEST(GsvgExceptionGuard, AVirtualGridWhoseResultIsNotFiniteIsRefusedAndTheOriginalIsRestored) {
    auto config = [](const std::string& extra) {
        return std::string("{\"virtual_grid\": true, \"vg_table_path\": \"") + xpe_gsvg_test::Data("virtual_grid_synthetic_table.csv") +
               "\", \"vg_kvp\": 80, \"vg_grid_ratio\": 10, \"vg_pixel_pitch_mm\": 1.0, \"vg_air_signal\": 60000, \"vg_iterations\": 5" + extra + "}";
    };
    const int w = 64, hgt = 64;
    std::vector<uint16_t> src(static_cast<size_t>(w) * hgt);
    for (int y = 0; y < hgt; ++y) {
        for (int x = 0; x < w; ++x) src[static_cast<size_t>(y) * w + x] = static_cast<uint16_t>(30000 + 10 * x + 5 * y);
    }
    {
        void* h = nullptr;
        ASSERT_EQ(XPE_OK, xpe_gsvg_init(&h, config("").c_str()));
        std::vector<uint16_t> out(src.size(), 7);
        EXPECT_EQ(XPE_OK, xpe_gsvg_process(h, src.data(), src.size(), out.data(), out.size(), w, hgt, nullptr, 0)) << "control";
        xpe_gsvg_shutdown(h);
    }
    for (const char* extra : {", \"vg_pyramid_gain\": 1e308", ", \"vg_pyramid_gain\": 1e308, \"vg_denoise_k\": 1e308"}) {
        void* h = nullptr;
        ASSERT_EQ(XPE_OK, xpe_gsvg_init(&h, config(extra).c_str())) << extra;
        std::vector<uint16_t> out(src.size(), 7);
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_gsvg_process(h, src.data(), src.size(), out.data(), out.size(), w, hgt, nullptr, 0)) << extra;
        EXPECT_EQ(src, out) << "the original image is restored on a refusal: " << extra;
        xpe_gsvg_shutdown(h);
    }
}
