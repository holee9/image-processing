/**
 * @file test_exception_guard.cpp
 * @brief No exception leaves xpe_noise_reduce / xpe_contrast_enhance / xpe_edge_enhance (QA-B-181, QA-B-179, #233).
 *
 * QA-B-179 measured inputs that made these three exported functions throw out of the C ABI with no memory
 * pressure at all:
 *   - xpe_noise_reduce (NLM): a declared 2^20 x 2^20 image with dataSize 0 (accepted: "unspecified") -> bad_alloc
 *   - xpe_noise_reduce (bilateral) and xpe_edge_enhance: width 0x80000000 became a negative int -> length_error
 *   - xpe_contrast_enhance: tile_width 0x40000000 overflowed `tile_width * 2` (undefined behaviour) -> length_error
 * Each is now an error code, the image descriptor and pixels are untouched, and the module keeps working.
 *
 * The guard itself is xpe_guarded_call (enhance_basic_internal.h); it is header-inline, so these tests run it
 * directly with each exception kind. The exported functions are wired to it, which the size-driven allocation
 * failures below exercise end to end. Those need a request larger than the machine's commit limit; where the
 * machine could in principle satisfy it the test skips rather than allocate.
 */
#include <gtest/gtest.h>

#include <mutex>
#include <utility>

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>
#include <stdexcept>
#include <vector>

#include "xpe/enhance_basic/enhance_basic_api.h"
#include "xpe/enhance_basic/enhance_basic_internal.h"
#include "../src/parallel_rows.h"

namespace {

XpeImageBuffer Declared(uint32_t w, uint32_t h, float* data, size_t bytes) {
    XpeImageBuffer b{};
    b.width = w;
    b.height = h;
    b.bitsAllocated = 32;
    b.bitsStored = 32;
    b.format = XPE_PIXEL_FLOAT32;
    b.data = data;
    b.dataSize = bytes;
    return b;
}

bool SameDescriptor(const XpeImageBuffer& a, const XpeImageBuffer& b) {
    return a.width == b.width && a.height == b.height && a.data == b.data && a.dataSize == b.dataSize &&
           a.format == b.format && a.bitsAllocated == b.bitsAllocated && a.bitsStored == b.bitsStored;
}

/** True when `bytes` is more than twice what the machine could ever commit: the allocation cannot succeed. */
bool CannotBeCommitted(double bytes) {
    MEMORYSTATUSEX m{};
    m.dwLength = sizeof(m);
    GlobalMemoryStatusEx(&m);
    return bytes > 2.0 * static_cast<double>(m.ullTotalPageFile);
}

template <class F>
bool Escaped(F fn, XpeErrorCode* rc) {
    try {
        *rc = fn();
        return false;
    } catch (...) {
        return true;
    }
}

}  // namespace

// ---- the guard ---------------------------------------------------------------------------------------------

TEST(ExceptionGuard, BadAllocBecomesOutOfMemory) {
    EXPECT_EQ(XPE_ERR_OUT_OF_MEMORY, xpe_guarded_call([]() -> XpeErrorCode { throw std::bad_alloc(); }));
}

TEST(ExceptionGuard, LengthErrorAndAnyStdExceptionBecomeProcessingFailed) {
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, xpe_guarded_call([]() -> XpeErrorCode { throw std::length_error("x"); }));
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, xpe_guarded_call([]() -> XpeErrorCode { throw std::runtime_error("x"); }));
}

TEST(ExceptionGuard, ANonStdExceptionBecomesProcessingFailed) {
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, xpe_guarded_call([]() -> XpeErrorCode { throw 42; }));
}

TEST(ExceptionGuard, ANormalReturnPassesThroughUnchanged) {
    EXPECT_EQ(XPE_OK, xpe_guarded_call([]() -> XpeErrorCode { return XPE_OK; }));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_guarded_call([]() -> XpeErrorCode { return XPE_ERR_INVALID_INPUT; }));
}

// ---- the three exported functions, with the inputs QA-B-179 measured ---------------------------------------

TEST(ExceptionGuard, NoiseReduceNlmWithAnImpossibleDeclaredSizeIsOutOfMemoryNotAnException) {
    const double bytes = 4.0 * static_cast<double>(1u << 20) * static_cast<double>(1u << 20);   // output(n) floats
    if (!CannotBeCommitted(bytes)) GTEST_SKIP() << "this machine's commit limit could satisfy a " << bytes << " byte request";
    float one = 1.0f;
    XpeImageBuffer img = Declared(1u << 20, 1u << 20, &one, 0);   // dataSize 0 = unspecified: accepted
    const XpeImageBuffer before = img;
    XpeNoiseReduceParams p{};
    p.mode = XPE_NOISE_NLM;
    p.search_window = 3;
    p.patch_size = 3;
    p.h_param = 1.0f;
    XpeErrorCode rc = XPE_OK;
    EXPECT_FALSE(Escaped([&] { return xpe_noise_reduce(&img, &p); }, &rc)) << "an exception left xpe_noise_reduce";
    EXPECT_EQ(XPE_ERR_OUT_OF_MEMORY, rc);
    EXPECT_TRUE(SameDescriptor(before, img));
    EXPECT_EQ(1.0f, one);
}

TEST(ExceptionGuard, NoiseReduceBilateralWithAWidthAboveIntMaxIsInvalidInput) {
    float one = 1.0f;
    XpeImageBuffer img = Declared(0x80000000u, 1, &one, 0);
    XpeNoiseReduceParams p{};
    p.mode = XPE_NOISE_BILATERAL;
    p.sigma_space = 3.0f;
    p.sigma_range = 10.0f;
    XpeErrorCode rc = XPE_OK;
    EXPECT_FALSE(Escaped([&] { return xpe_noise_reduce(&img, &p); }, &rc)) << "an exception left xpe_noise_reduce";
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, rc);
    EXPECT_EQ(1.0f, one);
}

TEST(ExceptionGuard, EdgeEnhanceWithAWidthAboveIntMaxIsInvalidInput) {
    float one = 1.0f;
    XpeImageBuffer img = Declared(0x80000000u, 1, &one, 0);
    XpeUsmParams p{0.5f, 2.0f, 10.0f};
    XpeErrorCode rc = XPE_OK;
    EXPECT_FALSE(Escaped([&] { return xpe_edge_enhance(&img, &p); }, &rc)) << "an exception left xpe_edge_enhance";
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, rc);
    EXPECT_EQ(1.0f, one);
}

TEST(ExceptionGuard, EdgeEnhanceWithAnImpossibleRowBufferIsOutOfMemoryNotAnException) {
    // The widest accepted image: the blur's row ring needs ksize * width floats (ksize <= 41 at radius 10).
    const double bytes = 41.0 * 4.0 * 2147483647.0;
    if (!CannotBeCommitted(bytes)) GTEST_SKIP() << "this machine's commit limit could satisfy a " << bytes << " byte request";
    float one = 1.0f;
    XpeImageBuffer img = Declared(0x7FFFFFFFu, 1, &one, 0);
    const XpeImageBuffer before = img;
    XpeUsmParams p{0.5f, 10.0f, 10.0f};
    XpeErrorCode rc = XPE_OK;
    EXPECT_FALSE(Escaped([&] { return xpe_edge_enhance(&img, &p); }, &rc)) << "an exception left xpe_edge_enhance";
    EXPECT_EQ(XPE_ERR_OUT_OF_MEMORY, rc);
    EXPECT_TRUE(SameDescriptor(before, img));
    EXPECT_EQ(1.0f, one);
}

TEST(ExceptionGuard, ContrastEnhanceWithATileCountThatOverflowsIntIsInvalidInput) {
    std::vector<float> px(64);
    for (size_t i = 0; i < px.size(); ++i) px[i] = static_cast<float>(i % 17);
    const std::vector<float> original = px;
    XpeImageBuffer img = Declared(8, 8, px.data(), px.size() * sizeof(float));
    XpeClaheParams p{3.0f, 0x40000000, 2};   // tile_width * 2 wrapped to INT_MIN in int and passed the window check
    XpeErrorCode rc = XPE_OK;
    EXPECT_FALSE(Escaped([&] { return xpe_contrast_enhance(&img, &p); }, &rc)) << "an exception left xpe_contrast_enhance";
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, rc);
    EXPECT_EQ(original, px);
}

// The window check still admits exactly the grids that fit: the boundary stays valid, one past it is rejected.
TEST(ExceptionGuard, ContrastEnhanceKeepsItsWindowBoundary) {
    std::vector<float> px(64);
    for (size_t i = 0; i < px.size(); ++i) px[i] = static_cast<float>(i % 17);
    XpeImageBuffer img = Declared(8, 8, px.data(), px.size() * sizeof(float));
    XpeClaheParams fits{3.0f, 4, 4};        // 4 * 2 == 8: the largest grid an 8 x 8 image allows
    EXPECT_EQ(XPE_OK, xpe_contrast_enhance(&img, &fits));
    XpeClaheParams tooBig{3.0f, 5, 4};      // 5 * 2 > 8
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_contrast_enhance(&img, &tooBig));
}

// The size rule is module-wide (it sits in validate_float32_image), so a function that never allocated is
// consistent with the rest: a dimension above INT32_MAX is invalid input, not an index that wraps negative.
TEST(ExceptionGuard, EveryEntryPointRejectsADimensionAboveIntMax) {
    float one = 1.0f;
    XpeImageBuffer wide = Declared(0x80000000u, 1, &one, 0);
    XpeImageBuffer tall = Declared(1, 0x80000000u, &one, 0);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, validate_float32_image(&wide));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, validate_float32_image(&tall));
    XpeImageBuffer widest = Declared(0x7FFFFFFFu, 1, &one, 0);
    EXPECT_EQ(XPE_OK, validate_float32_image(&widest)) << "INT32_MAX itself stays valid";
}

// ---- QA-B-181b: the tile-size arithmetic of xpe_contrast_enhance (Codex #35) --------------------------------

namespace {

/** The reference: the same quotient computed where the sum cannot overflow. */
int CeilDiv64(int size, int parts) {
    return static_cast<int>((static_cast<int64_t>(size) + parts - 1) / parts);
}

std::vector<float> Pattern(int w, int h) {
    std::vector<float> px(static_cast<size_t>(w) * h);
    uint32_t s = 12345u;
    for (size_t i = 0; i < px.size(); ++i) {
        s = s * 1664525u + 1013904223u;
        px[i] = static_cast<float>(s >> 20) + static_cast<float>(i % 97);
    }
    return px;
}

}  // namespace

// The boundary Codex named (w = INT32_MAX, 2 tiles) and its neighbourhood, no allocation, no image.
TEST(ExceptionGuard, CeilDivMatchesTheWideReferenceNearIntMax) {
    const int sizes[] = {1, 2, 3, 7, 1000, 0x3FFFFFFF, 0x40000000, 0x40000001, 0x7FFFFFFD, 0x7FFFFFFE, 0x7FFFFFFF};
    const int parts[] = {1, 2, 3, 4, 5, 8, 1000, 0x3FFFFFFF, 0x40000000};
    for (int s : sizes) {
        for (int p : parts) {
            if (p > s / 2) continue;   // the window check admits only parts <= size / 2
            EXPECT_EQ(CeilDiv64(s, p), xpe_ceil_div(s, p)) << "size=" << s << " parts=" << p;
        }
    }
    EXPECT_EQ(1073741824, xpe_ceil_div(0x7FFFFFFF, 2)) << "Codex's example: w = INT32_MAX, tile_width = 2";
    EXPECT_EQ(0x7FFFFFFF, xpe_ceil_div(0x7FFFFFFF, 1));
}

TEST(ExceptionGuard, CeilDivIsExactAtTheOrdinaryBoundaries) {
    EXPECT_EQ(3, xpe_ceil_div(9, 3));
    EXPECT_EQ(4, xpe_ceil_div(10, 3));
    EXPECT_EQ(1, xpe_ceil_div(1, 1));
    EXPECT_EQ(512, xpe_ceil_div(4096, 8));
    EXPECT_EQ(385, xpe_ceil_div(3072 + 8, 8));   // 3080 / 8 = 385 exactly
    EXPECT_EQ(386, xpe_ceil_div(3081, 8));
}

// QA-B-181c (Codex #37): the pinned FNV-1a hashes of the first version of this test were taken on one machine
// and one compiler. contrast computes float LUTs, CDFs and `val_min + frac * val_range`, so a different compiler
// or instruction set may legitimately change the last bits; a pinned hash turns that into a false red. The values
// stay in the QA-B-181b report as a record. What is checked instead holds on every machine: the tile geometry
// against the old integer formulas computed where they cannot overflow (same run), and properties of the image.

namespace {

/** Old formulas, evaluated in 64 bits so they cannot overflow: the reference for the new geometry helpers. */
void OldTileBounds(int t, int tileSize, int size, int& start, int& end) {
    const int64_t s = static_cast<int64_t>(t) * tileSize;
    start = static_cast<int>(s > size ? size : s);
    end = static_cast<int>(s + tileSize > size ? size : s + tileSize);
}

}  // namespace

TEST(ExceptionGuard, TileBoundsMatchTheOldFormulaAndPartitionTheAxis) {
    const int sizes[] = {4, 5, 8, 11, 17, 64, 101, 3072};
    for (int size : sizes) {
        for (int tiles = 2; tiles <= size / 2; tiles += (tiles < 12 ? 1 : 7)) {
            const int tileSize = xpe_ceil_div(size, tiles);
            int covered = 0;
            for (int t = 0; t < tiles; ++t) {
                int s = 0, e = 0, rs = 0, re = 0;
                xpe_tile_bounds(t, tileSize, size, s, e);
                OldTileBounds(t, tileSize, size, rs, re);
                ASSERT_EQ(rs, s) << size << "/" << tiles << " tile " << t;
                ASSERT_EQ(re, e) << size << "/" << tiles << " tile " << t;
                ASSERT_LE(s, e);
                if (s < e) {
                    ASSERT_EQ(covered, s) << "tiles must be contiguous, " << size << "/" << tiles;
                    covered = e;
                }
            }
            ASSERT_EQ(size, covered) << "the non-empty tiles must cover the axis, " << size << "/" << tiles;
            for (int pos = 0; pos < size; ++pos) {
                const int t = xpe_tile_of(pos, tileSize, tiles);
                int s = 0, e = 0;
                xpe_tile_bounds(t, tileSize, size, s, e);
                ASSERT_TRUE(s <= pos && pos < e) << "pos " << pos << " is assigned to tile " << t
                                                 << " [" << s << "," << e << ") for " << size << "/" << tiles;
            }
        }
    }
}

TEST(ExceptionGuard, TileBoundsStayInRangeNearIntMax) {
    const int size = 0x7FFFFFFF;
    const int tiles = 3;
    const int tileSize = xpe_ceil_div(size, tiles);
    int prevEnd = 0;
    for (int t = 0; t < tiles; ++t) {
        int s = 0, e = 0;
        xpe_tile_bounds(t, tileSize, size, s, e);
        EXPECT_EQ(prevEnd, s);
        EXPECT_LE(s, e);
        EXPECT_LE(e, size);
        prevEnd = e;
    }
    EXPECT_EQ(size, prevEnd);
    int s = 0, e = 0;
    xpe_tile_bounds(tiles + 5, tileSize, size, s, e);   // far past the edge: empty, clamped to it
    EXPECT_EQ(size, s);
    EXPECT_EQ(size, e);
}

TEST(ExceptionGuard, ContrastEnhanceKeepsItsImagePropertiesOnEveryTileShape) {
    struct Case { int w, h, tw, th; };
    const Case cases[] = {
        {101, 67, 8, 8},     // sizes not divisible by the tile counts
        {64, 64, 4, 4},      // divisible
        {17, 9, 2, 2},       // small, uneven
        {5, 8, 2, 4},        // w = 5 with 2 tiles: the last tile is short
        {11, 11, 5, 5},      // the grid has a tile that starts past the edge (QA-B-181c)
        {3072, 3072, 8, 8},  // a full detector frame
        {3070, 2051, 7, 9},  // a full frame with uneven tiles
    };
    for (const Case& c : cases) {
        const std::vector<float> in = Pattern(c.w, c.h);
        std::vector<float> a = in, b = in;
        XpeImageBuffer ia = Declared(static_cast<uint32_t>(c.w), static_cast<uint32_t>(c.h), a.data(), a.size() * sizeof(float));
        XpeImageBuffer ib = Declared(static_cast<uint32_t>(c.w), static_cast<uint32_t>(c.h), b.data(), b.size() * sizeof(float));
        XpeClaheParams p{3.0f, c.tw, c.th};
        ASSERT_EQ(XPE_OK, xpe_contrast_enhance(&ia, &p)) << c.w << "x" << c.h;
        ASSERT_EQ(XPE_OK, xpe_contrast_enhance(&ib, &p)) << c.w << "x" << c.h;
        EXPECT_EQ(0, std::memcmp(a.data(), b.data(), a.size() * sizeof(float)))
            << "the same input must give the same bits, run to run";

        const float lo = *std::min_element(in.begin(), in.end());
        const float hi = *std::max_element(in.begin(), in.end());
        bool finite = true, inRange = true;
        for (float v : a) {
            if (!std::isfinite(v)) finite = false;
            if (v < lo || v > hi) inRange = false;
        }
        EXPECT_TRUE(finite) << c.w << "x" << c.h;
        EXPECT_TRUE(inRange) << c.w << "x" << c.h << ": the output must stay inside the input's value range";

        // Within one tile the mapping is a monotone LUT: a brighter input never gets a darker output.
        const int tileW = xpe_ceil_div(c.w, c.tw), tileH = xpe_ceil_div(c.h, c.th);
        bool monotone = true;
        for (int ty = 0; ty < c.th && monotone; ++ty) {
            for (int tx = 0; tx < c.tw && monotone; ++tx) {
                int x0, x1, y0, y1;
                xpe_tile_bounds(tx, tileW, c.w, x0, x1);
                xpe_tile_bounds(ty, tileH, c.h, y0, y1);
                if (x0 >= x1 || y0 >= y1) continue;
                std::vector<std::pair<float, float>> pairs;
                for (int y = y0; y < y1; ++y)
                    for (int x = x0; x < x1; ++x)
                        pairs.emplace_back(in[static_cast<size_t>(y) * c.w + x], a[static_cast<size_t>(y) * c.w + x]);
                std::sort(pairs.begin(), pairs.end());
                for (size_t i = 1; i < pairs.size(); ++i)
                    if (pairs[i].first > pairs[i - 1].first && pairs[i].second < pairs[i - 1].second) {
                        monotone = false;
                        break;
                    }
            }
        }
        EXPECT_TRUE(monotone) << c.w << "x" << c.h << " tiles " << c.tw << "x" << c.th;
    }
}

// ---- QA-B-181c: a tile that starts past the edge is empty and must not be read ---------------------------------

namespace {

bool RunContrastCatchingAv(XpeImageBuffer* img, XpeClaheParams* p, XpeErrorCode* rc) {
    __try {
        *rc = xpe_contrast_enhance(img, p);
        return true;
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER
                                                                  : EXCEPTION_CONTINUE_SEARCH) {
        return false;
    }
}

/** One committed page followed by a PAGE_NOACCESS page; the pixel buffer ends exactly at the guard page. */
class GuardedPixels {
public:
    GuardedPixels() {
        base_ = static_cast<char*>(VirtualAlloc(nullptr, 2 * kPage, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        if (base_) {
            DWORD old = 0;
            VirtualProtect(base_ + kPage, kPage, PAGE_NOACCESS, &old);
        }
    }
    ~GuardedPixels() { if (base_) VirtualFree(base_, 0, MEM_RELEASE); }
    bool ok() const { return base_ != nullptr; }
    size_t capacity() const { return kPage / sizeof(float); }
    float* Place(const std::vector<float>& v) {
        float* p = reinterpret_cast<float*>(base_ + kPage) - v.size();
        std::memcpy(p, v.data(), v.size() * sizeof(float));
        return p;
    }
private:
    static constexpr size_t kPage = 4096;
    char* base_ = nullptr;
};

}  // namespace

// Codex #37: with 11 x 11 pixels and 5 x 5 tiles the tile size is 3 and the fifth tile row starts at y = 12, past
// the edge. build_tile_lut read px[y0 * w + x0] before looking at the range, one float past the 121-pixel buffer.
// The buffer here ends at a PAGE_NOACCESS page, so that read is an access violation.
TEST(ExceptionGuard, ContrastEnhanceNeverReadsPastThePixelsForAnEmptyEdgeTile) {
    GuardedPixels guarded;
    ASSERT_TRUE(guarded.ok());

    XpeErrorCode rc = XPE_OK;
    {
        const std::vector<float> in = Pattern(11, 11);
        float* px = guarded.Place(in);
        XpeImageBuffer img = Declared(11, 11, px, in.size() * sizeof(float));
        XpeClaheParams p{3.0f, 5, 5};
        ASSERT_TRUE(RunContrastCatchingAv(&img, &p, &rc)) << "the 11x11 / 5x5 case read past the buffer";
        EXPECT_EQ(XPE_OK, rc);
    }

    // Every shape whose grid has empty tiles, buffers placed flush against the guard page.
    const int dims[] = {5, 7, 9, 11, 13, 17, 23, 31};
    const int counts[] = {2, 3, 4, 5, 6, 7, 9};
    long cases = 0, withEmptyTiles = 0, violations = 0;
    for (int w : dims) {
        for (int h : dims) {
            if (static_cast<size_t>(w) * h > guarded.capacity()) continue;
            const std::vector<float> in = Pattern(w, h);
            for (int tw : counts) {
                for (int th : counts) {
                    if (w < tw * 2 || h < th * 2) continue;
                    const int tileW = xpe_ceil_div(w, tw), tileH = xpe_ceil_div(h, th);
                    if ((tw - 1) * tileW >= w || (th - 1) * tileH >= h) ++withEmptyTiles;
                    float* px = guarded.Place(in);
                    XpeImageBuffer img = Declared(static_cast<uint32_t>(w), static_cast<uint32_t>(h), px, in.size() * sizeof(float));
                    XpeClaheParams p{3.0f, tw, th};
                    XpeErrorCode r = XPE_OK;
                    ++cases;
                    if (!RunContrastCatchingAv(&img, &p, &r)) {
                        if (++violations <= 3) {
                            ADD_FAILURE() << "access violation for " << w << "x" << h << " tiles " << tw << "x" << th;
                        }
                    } else {
                        EXPECT_EQ(XPE_OK, r) << w << "x" << h << " tiles " << tw << "x" << th;
                    }
                }
            }
        }
    }
    EXPECT_GT(withEmptyTiles, 0) << "control: the sweep contains grids with an empty tile";
    EXPECT_EQ(0, violations);
    RecordProperty("cases", static_cast<int>(cases));
    RecordProperty("cases_with_empty_tiles", static_cast<int>(withEmptyTiles));
}

// ---- QA-B-181c (Codex #37): the row-band split of ForRows ---------------------------------------------------------

TEST(ExceptionGuard, ForRowsPartitionsRowsNearIntMaxWithoutOverflow) {
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

TEST(ExceptionGuard, ForRowsNeverPlansMoreBandsThanRows) {
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
