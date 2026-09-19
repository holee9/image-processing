/**
 * @file test_presentation_lut.cpp
 * @brief Unit tests for xpe_apply_presentation_lut and xpe_gsdf_calibrate (SWU-3.3)
 * SPEC: SPEC-XPE-P1B-DISP
 * REQ-DISP-019 to REQ-DISP-028
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <vector>
#include "perf_measure.h"

#include "xpe/display/display_api.h"
#include "xpe/common/xpe_memory.h"

// =============================================================================
// Test Helpers
// =============================================================================

static XpeImageBuffer make_float32_image(uint32_t w, uint32_t h, float fill_value) {
    XpeImageBuffer img{};
    img.width         = w;
    img.height        = h;
    img.format        = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32;
    img.bitsStored    = 32;
    img.dataSize      = w * h * sizeof(float);
    img.data          = std::malloc(img.dataSize);
    float* px = static_cast<float*>(img.data);
    for (size_t i = 0; i < (size_t)w * h; ++i) px[i] = fill_value;
    return img;
}

// After presentation LUT, img->data points to uint16 buffer (function handles realloc)
static void free_image(XpeImageBuffer& img) {
    std::free(img.data);
    img.data = nullptr;
}

// Build identity LUT: index i maps to value i (for 1024 entries, scaled to uint16 range)
static void make_identity_lut(XpePresentationLutParams& p) {
    for (int i = 0; i < 1024; ++i) {
        p.lutData[i] = static_cast<uint16_t>(i);
    }
    p.gsdfEnabled = 0;
}

// Build ramp LUT: all entries map to a fixed value
static void make_constant_lut(XpePresentationLutParams& p, uint16_t val) {
    for (int i = 0; i < 1024; ++i) {
        p.lutData[i] = val;
    }
    p.gsdfEnabled = 0;
}

// =============================================================================
// REQ-DISP-019: Domain transition float32 -> uint16
// =============================================================================

TEST(PresentationLut, DomainTransition_FormatBecomesUint16) {
    // REQ-DISP-019: after call, img->format must be XPE_PIXEL_UINT16
    XpeImageBuffer img = make_float32_image(2, 2, 0.5f);
    XpePresentationLutParams params{};
    make_identity_lut(params);

    XpeErrorCode rc = xpe_apply_presentation_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_EQ(img.format, XPE_PIXEL_UINT16);
    EXPECT_EQ(img.bitsAllocated, 16u);
    EXPECT_EQ(img.bitsStored, 16u);
    EXPECT_EQ(img.dataSize, (size_t)(2 * 2 * 2)); // 4 pixels * 2 bytes
    free_image(img);
}

// =============================================================================
// REQ-DISP-020: LUT lookup index = clamp(round(input * 1023), 0, 1023)
// =============================================================================

TEST(PresentationLut, LutLookup_HalfValue) {
    // REQ-DISP-020: input=0.5 -> index=round(0.5*1023)=512 -> lutData[512]
    XpeImageBuffer img = make_float32_image(1, 1, 0.5f);
    XpePresentationLutParams params{};
    make_identity_lut(params);
    // identity: lutData[512] = 512

    XpeErrorCode rc = xpe_apply_presentation_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    uint16_t* out = static_cast<uint16_t*>(img.data);
    EXPECT_EQ(out[0], 512u);
    free_image(img);
}

TEST(PresentationLut, LutLookup_ZeroInput) {
    // REQ-DISP-020: input=0.0 -> index=0 -> lutData[0]
    XpeImageBuffer img = make_float32_image(1, 1, 0.0f);
    XpePresentationLutParams params{};
    params.lutData[0] = 999;
    for (int i = 1; i < 1024; ++i) params.lutData[i] = 0;
    params.gsdfEnabled = 0;

    XpeErrorCode rc = xpe_apply_presentation_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_EQ(static_cast<uint16_t*>(img.data)[0], 999u);
    free_image(img);
}

TEST(PresentationLut, LutLookup_OneInput) {
    // REQ-DISP-020: input=1.0 -> index=1023 -> lutData[1023]
    XpeImageBuffer img = make_float32_image(1, 1, 1.0f);
    XpePresentationLutParams params{};
    make_constant_lut(params, 0);
    params.lutData[1023] = 65535;

    XpeErrorCode rc = xpe_apply_presentation_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_EQ(static_cast<uint16_t*>(img.data)[0], 65535u);
    free_image(img);
}

// =============================================================================
// REQ-DISP-021: Input clamped to [0.0, 1.0] before lookup
// =============================================================================

TEST(PresentationLut, InputClamp_Negative) {
    // REQ-DISP-021: negative input clamped to 0.0 -> index 0
    XpeImageBuffer img = make_float32_image(1, 1, -5.0f);
    XpePresentationLutParams params{};
    make_identity_lut(params);

    XpeErrorCode rc = xpe_apply_presentation_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_EQ(static_cast<uint16_t*>(img.data)[0], 0u);
    free_image(img);
}

TEST(PresentationLut, InputClamp_AboveOne) {
    // REQ-DISP-021: input > 1.0 clamped to 1.0 -> index 1023
    XpeImageBuffer img = make_float32_image(1, 1, 2.5f);
    XpePresentationLutParams params{};
    make_identity_lut(params);

    XpeErrorCode rc = xpe_apply_presentation_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_EQ(static_cast<uint16_t*>(img.data)[0], 1023u);
    free_image(img);
}

// =============================================================================
// REQ-DISP-022 to REQ-DISP-024: Error cases
// =============================================================================

TEST(PresentationLut, Error_NullImg) {
    // REQ-DISP-022: NULL img -> XPE_ERR_INVALID_INPUT
    XpePresentationLutParams params{};
    XpeErrorCode rc = xpe_apply_presentation_lut(nullptr, &params);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
}

TEST(PresentationLut, Error_NullParams) {
    // REQ-DISP-022: NULL params -> XPE_ERR_INVALID_INPUT
    XpeImageBuffer img = make_float32_image(2, 2, 0.5f);
    XpeErrorCode rc = xpe_apply_presentation_lut(&img, nullptr);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
    free_image(img);
}

TEST(PresentationLut, Error_WrongFormat) {
    // REQ-DISP-023: format != FLOAT32 -> XPE_ERR_UNSUPPORTED_FORMAT
    uint16_t buf[4] = {0};
    XpeImageBuffer img{};
    img.width = 2; img.height = 2;
    img.format = XPE_PIXEL_UINT16;
    img.bitsAllocated = 16; img.bitsStored = 16;
    img.dataSize = 8;
    img.data = buf;

    XpePresentationLutParams params{};
    XpeErrorCode rc = xpe_apply_presentation_lut(&img, &params);
    EXPECT_EQ(rc, XPE_ERR_UNSUPPORTED_FORMAT);
}

// =============================================================================
// REQ-DISP-026: xpe_gsdf_calibrate basic functionality
// =============================================================================

TEST(PresentationLut, GsdfCalibrate_BasicOutput) {
    // REQ-DISP-026: gsdf_calibrate produces 1024-entry non-decreasing LUT
    float lum[10];
    for (int i = 0; i < 10; ++i) lum[i] = 1.0f + i * 10.0f; // 1..91 cd/m^2

    XpePresentationLutParams out{};
    XpeErrorCode rc = xpe_gsdf_calibrate(lum, 10, &out);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_EQ(out.gsdfEnabled, 1);

    // LUT must be monotonically non-decreasing
    for (int i = 1; i < 1024; ++i) {
        EXPECT_GE(out.lutData[i], out.lutData[i - 1])
            << "LUT not monotone at index " << i;
    }
}

TEST(PresentationLut, GsdfCalibrate_MinCount2) {
    // REQ-DISP-027: count >= 2 required
    float lum[2] = {1.0f, 100.0f};
    XpePresentationLutParams out{};
    XpeErrorCode rc = xpe_gsdf_calibrate(lum, 2, &out);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_EQ(out.gsdfEnabled, 1);
}

TEST(PresentationLut, GsdfCalibrate_Error_NullLuminance) {
    // REQ-DISP-027: NULL luminanceValues -> XPE_ERR_INVALID_INPUT
    XpePresentationLutParams out{};
    XpeErrorCode rc = xpe_gsdf_calibrate(nullptr, 10, &out);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
}

TEST(PresentationLut, GsdfCalibrate_Error_NullOut) {
    // REQ-DISP-027: NULL outParams -> XPE_ERR_INVALID_INPUT
    float lum[5] = {1.0f, 10.0f, 50.0f, 100.0f, 500.0f};
    XpeErrorCode rc = xpe_gsdf_calibrate(lum, 5, nullptr);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
}

TEST(PresentationLut, GsdfCalibrate_Error_CountLessThan2) {
    // REQ-DISP-027: count < 2 -> XPE_ERR_INVALID_INPUT
    float lum[1] = {1.0f};
    XpePresentationLutParams out{};
    XpeErrorCode rc = xpe_gsdf_calibrate(lum, 1, &out);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
}

// =============================================================================
// REQ-DISP-025: Performance test (<= 25ms for 3072x3072)
// =============================================================================

TEST(PresentationLut, Performance_3072x3072) {
    // REQ-DISP-025: presentation LUT <= 25ms for 3072x3072
    XpeImageBuffer img = make_float32_image(3072, 3072, 0.5f);
    XpePresentationLutParams params{};
    make_identity_lut(params);

    auto t0 = std::chrono::high_resolution_clock::now();
    XpeErrorCode rc = xpe_apply_presentation_lut(&img, &params);
    auto t1 = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    EXPECT_EQ(rc, XPE_OK);
    EXPECT_LE(ms, 30) << "PresentationLUT 3072x3072 took " << ms << "ms (limit 30ms, Release target 25ms per REQ-DISP-028)";
    free_image(img);
}

// =============================================================================
// #142 D5 — cross-DLL allocate/free round trip
//
// xpe_apply_presentation_lut() frees a buffer that xpe_common allocated and
// hands back one that xpe_display allocated, so ownership crosses the DLL
// boundary twice in one call. That is safe only while both modules use the
// same C runtime heap. QA-B-40 measured that they do: dumpbin reports
// VCRUNTIME140.dll and api-ms-win-crt-heap-l1-1-0.dll -- the shared UCRT --
// for xpe_common.dll and xpe_display.dll alike (evidence: _d5_crt.log).
//
// The measurement is a snapshot of the current link settings; this case is the
// standing check. If a future build switched either module to a static CRT the
// free below would corrupt a foreign heap, and this test is where it surfaces.
// =============================================================================
TEST(PresentationLutCrossDllTest, CommonAllocatedBufferSurvivesDisplayConversion) {
    // Allocated by xpe_common.dll.
    XpeImageBuffer img{};
    ASSERT_EQ(XPE_OK, xpe_alloc_image(16, 16, XPE_PIXEL_FLOAT32, &img));
    ASSERT_NE(nullptr, img.data);

    float* px = static_cast<float*>(img.data);
    for (uint32_t i = 0; i < 16u * 16u; ++i) {
        px[i] = static_cast<float>(i) / 255.0f;
    }

    XpePresentationLutParams params{};
    for (int i = 0; i < 1024; ++i) {
        params.lutData[i] = static_cast<uint16_t>(i * 64);
    }
    params.gsdfEnabled = 0;

    // Frees the xpe_common buffer, installs an xpe_display one.
    ASSERT_EQ(XPE_OK, xpe_apply_presentation_lut(&img, &params));
    EXPECT_EQ(XPE_PIXEL_UINT16, img.format);
    EXPECT_EQ(16u * 16u * sizeof(uint16_t), img.dataSize);
    ASSERT_NE(nullptr, img.data);

    // Freed by xpe_common.dll -- the other direction of the same crossing.
    EXPECT_EQ(XPE_OK, xpe_free_image(&img));
    EXPECT_EQ(nullptr, img.data);
}

// =============================================================================
// #142 D5 (QA-B-64) — the PREMISE, asserted rather than commented
//
// The round-trip case above checks that one allocate/free crossing survives.
// That is weaker than it looks, and the gap is the one this session has hit
// repeatedly: a thing EXISTING is not the same as it WORKING.
//
// The crossing is safe only because both modules resolve malloc/free through
// the same shared UCRT heap. QA-B-40 measured that once with dumpbin and wrote
// the result into a source comment. A comment is not a check: if a future build
// switched either module to a static CRT, the round-trip case would not fail
// cleanly -- freeing a foreign heap pointer is undefined, so it would crash, or
// corrupt quietly and still report PASS. The standing check would then be
// present and useless.
//
// So the premise itself is asserted here, by reading the import tables of the
// two loaded modules in-process: both must import a heap provider, and it must
// be the SAME one. Re-measured 2026-09-16 (QA-B-64, _d5_crt.log) -- both
// xpe_common.dll and xpe_display.dll import api-ms-win-crt-heap-l1-1-0.dll.
//
// A static-CRT switch removes that import entirely, which this case reports as
// a failure instead of leaving it to undefined behaviour.
// =============================================================================

#include <windows.h>
#include <set>
#include <string>

namespace {

// Names of the modules a given loaded DLL imports from. Walks the PE import
// directory of an already-loaded image; no file I/O, no shelling out.
std::set<std::string> ImportedModules(const char* dllName) {
    std::set<std::string> out;
    HMODULE mod = GetModuleHandleA(dllName);
    if (mod == nullptr) return out;

    auto* base = reinterpret_cast<const BYTE*>(mod);
    auto* dos  = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return out;

    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return out;

    const auto& dir = nt->OptionalHeader
                        .DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (dir.VirtualAddress == 0 || dir.Size == 0) return out;

    auto* desc = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(
                     base + dir.VirtualAddress);
    for (; desc->Name != 0; ++desc) {
        std::string name = reinterpret_cast<const char*>(base + desc->Name);
        for (char& c : name) c = static_cast<char>(::tolower(c));
        out.insert(name);
    }
    return out;
}

// The subset that provides malloc/free. A module linked against a STATIC CRT
// imports none of these -- which is exactly the regression being guarded.
std::set<std::string> HeapProviders(const std::set<std::string>& imports) {
    std::set<std::string> out;
    for (const std::string& m : imports) {
        if (m.find("crt-heap") != std::string::npos ||
            m.find("ucrtbase") != std::string::npos) {
            out.insert(m);
        }
    }
    return out;
}

std::string Join(const std::set<std::string>& s) {
    std::string out;
    for (const std::string& v : s) { if (!out.empty()) out += ", "; out += v; }
    return out.empty() ? "(none)" : out;
}

}  // namespace

TEST(PresentationLutCrossDllTest, BothModulesResolveTheHeapThroughTheSameCrt) {
    const std::set<std::string> commonImports  = ImportedModules("xpe_common.dll");
    const std::set<std::string> displayImports = ImportedModules("xpe_display.dll");

    // Proof the reader works: a module that imports NOTHING would produce an
    // empty set either because it truly imports nothing or because the walk
    // failed, and those are not the same. Both modules are known to import
    // kernel32 or an api-ms-win-core-* stub, so a non-empty set establishes the
    // walk reached real data. An empty set here means the reader broke, not
    // that the module is self-contained.
    ASSERT_FALSE(commonImports.empty())
        << "could not read xpe_common.dll's import table -- this case measures "
           "nothing until that works";
    ASSERT_FALSE(displayImports.empty())
        << "could not read xpe_display.dll's import table";

    const std::set<std::string> commonHeap  = HeapProviders(commonImports);
    const std::set<std::string> displayHeap = HeapProviders(displayImports);

    GTEST_LOG_(INFO) << "xpe_common heap providers:  " << Join(commonHeap);
    GTEST_LOG_(INFO) << "xpe_display heap providers: " << Join(displayHeap);

    // Neither may be statically linked: a static CRT imports no heap provider
    // and gets its own heap, which is precisely what breaks the free().
    EXPECT_FALSE(commonHeap.empty())
        << "xpe_common.dll imports no shared heap provider -- it appears to link "
           "a STATIC CRT, and xpe_display's free() of its buffer would corrupt a "
           "foreign heap";
    EXPECT_FALSE(displayHeap.empty())
        << "xpe_display.dll imports no shared heap provider -- static CRT";

    // And they must be the same provider, not merely both non-empty.
    EXPECT_EQ(commonHeap, displayHeap)
        << "the two modules resolve malloc/free through different runtimes: "
           "xpe_common=[" << Join(commonHeap) << "] "
           "xpe_display=[" << Join(displayHeap) << "]";
}

// ---------------------------------------------------------------------------
// #179 (QA-B-86): measure-only benchmark at the SPEC size -- no time assertion.
// The name carries BenchmarkFreeze (selected by benchmark-regression.yml -R)
// and Performance (excluded by ci.yml -E, which runs on shared runners).
// ---------------------------------------------------------------------------
// REQ-DISP-028 includes the format conversion: the call frees the float32
// buffer and installs a uint16 one (REQ-DISP-019), so every run needs a fresh
// std::malloc'd float32 image; the previous uint16 result is freed first.
TEST(PresentationLut, BenchmarkFreeze_Performance_REQ_DISP_028_Lut3072) {
    constexpr uint32_t kSize = 3072;
    const size_t n = static_cast<size_t>(kSize) * kSize;
    XpePresentationLutParams params{};
    make_identity_lut(params);
    XpeImageBuffer img{};
    auto reset = [&] {
        if (img.data) std::free(img.data);
        img = make_float32_image(kSize, kSize, 0.0f);
        float* px = static_cast<float*>(img.data);
        for (size_t i = 0; i < n; ++i)
            px[i] = static_cast<float>((i * 2654435761u) % 1024u) / 1023.0f;
    };
    perf_measure::Measure("REQ-DISP-028/xpe_apply_presentation_lut", "3072x3072", reset,
                          [&] { return xpe_apply_presentation_lut(&img, &params); });
    EXPECT_EQ(img.format, XPE_PIXEL_UINT16);
    EXPECT_NE(img.data, nullptr);
    std::free(img.data);
}

// ===========================================================================
// #155 (QA-B-146): the ordering contract, and the half of it that cannot be
// checked.
//
// REQ-DISP-029 says luminanceValues is the display's characteristic curve
// sampled at equally spaced driving levels, non-decreasing. Two halves, only
// one of them detectable from inside this function:
//
//   non-decreasing                  -- a property of the array. Checked.
//   equally spaced driving levels   -- NOT checked, and NOT checkable: no
//                                      driving level is passed in. The GUI's
//                                      {0.05, 1, 10, 100, 400} ascends and is
//                                      accepted while still being wrong input.
//
// So these cases assert that the FIRST half is enforced. They must not be read
// as "the contract is enforced".
// ===========================================================================

// The violation direction: a fall anywhere in the array is rejected, and the
// caller's params are left alone -- not half-written, not flagged enabled.
TEST(PresentationLut, GsdfCalibrate_Error_NonDecreasingViolation_155) {
    struct Case { const char* name; std::vector<float> lum; };
    const std::vector<Case> cases = {
        { "falls at the end",   {1.0f, 10.0f, 100.0f, 50.0f} },
        { "falls at the start", {10.0f, 1.0f, 100.0f, 500.0f} },
        { "fully descending",   {500.0f, 100.0f, 10.0f, 1.0f} },
        { "shuffled",           {500.0f, 3.0f, 1.0f, 111.0f, 7.0f} },
    };

    for (const Case& c : cases) {
        XpePresentationLutParams p{};
        p.gsdfEnabled = 0;
        for (int i = 0; i < 1024; ++i) p.lutData[i] = 0xBEEF;   // sentinel

        EXPECT_EQ(XPE_ERR_INVALID_INPUT,
                  xpe_gsdf_calibrate(c.lum.data(),
                                     static_cast<uint32_t>(c.lum.size()), &p))
            << c.name << ": a non-decreasing violation was accepted (REQ-DISP-029)";

        // REQ-DISP-026's shape: rejected input leaves outParams untouched. The
        // flag matters most -- a caller that only checks gsdfEnabled would
        // otherwise apply a LUT that was never computed.
        EXPECT_EQ(0, p.gsdfEnabled) << c.name << ": gsdfEnabled was set on a rejected call";
        int written = 0;
        for (int i = 0; i < 1024; ++i) if (p.lutData[i] != 0xBEEF) ++written;
        EXPECT_EQ(0, written) << c.name << ": " << written
                              << " LUT entries were written on a rejected call";
    }
}

// The other direction. Without this the case above passes just as happily on an
// implementation that rejects everything.
TEST(PresentationLut, GsdfCalibrate_AscendingIsAccepted_155) {
    const float lum[5] = {0.5f, 5.0f, 50.0f, 200.0f, 500.0f};
    XpePresentationLutParams p{};
    EXPECT_EQ(XPE_OK, xpe_gsdf_calibrate(lum, 5, &p));
    EXPECT_EQ(1, p.gsdfEnabled);
    for (int i = 1; i < 1024; ++i)
        ASSERT_LE(p.lutData[i - 1], p.lutData[i]) << "at index " << i;
}

// The boundary. "non-decreasing" is <=, not <: a real panel can be flat over a
// stretch of driving levels, so equal neighbours are input, not error.
TEST(PresentationLut, GsdfCalibrate_EqualNeighboursAreAccepted_155) {
    // Flat from DDL 16384 to 32768 (elements 1 and 2 of five).
    const float lum[5] = {0.5f, 50.0f, 50.0f, 200.0f, 500.0f};
    XpePresentationLutParams p{};
    ASSERT_EQ(XPE_OK, xpe_gsdf_calibrate(lum, 5, &p))
        << "equal neighbours were rejected -- the contract is non-decreasing (<=)";
    EXPECT_EQ(1, p.gsdfEnabled);
    for (int i = 1; i < 1024; ++i)
        ASSERT_LE(p.lutData[i - 1], p.lutData[i]) << "at index " << i;

    // THE PLATEAU RULE, pinned. Several driving levels produce 50.0 cd/m^2, so
    // the inverse of 50.0 is a range and the module must pick one. It picks the
    // LOWEST -- element 1, DDL = 1/4 * 65535 = 16384 (rounded) -- and that is a
    // decision recorded in presentation_lut.cpp, not an accident of the loop.
    // No LUT entry may land strictly inside the plateau's DDL range.
    const int plateauLo = static_cast<int>(std::lround(1.0 / 4.0 * 65535.0));
    const int plateauHi = static_cast<int>(std::lround(2.0 / 4.0 * 65535.0));
    int inside = 0;
    for (int i = 0; i < 1024; ++i) {
        const int v = static_cast<int>(p.lutData[i]);
        if (v > plateauLo && v < plateauHi) ++inside;
    }
    GTEST_LOG_(INFO) << "  plateau DDL range [" << plateauLo << ", " << plateauHi
                     << "]: entries strictly inside = " << inside;
    EXPECT_EQ(0, inside)
        << inside << " entries landed inside the flat stretch, so the module is "
           "no longer choosing the lowest driving level of the plateau";
}

// The control the card asks for: count == 2 still works, because that is what
// the GUI caller passes after GUI-C-131.
TEST(PresentationLut, GsdfCalibrate_TwoPointCurveStillWorks_155) {
    const float lum[2] = {0.05f, 400.0f};
    XpePresentationLutParams p{};
    EXPECT_EQ(XPE_OK, xpe_gsdf_calibrate(lum, 2, &p));
    EXPECT_EQ(1, p.gsdfEnabled);
    EXPECT_LT(p.lutData[0], p.lutData[1023]) << "the two-point curve produced a flat LUT";
}
