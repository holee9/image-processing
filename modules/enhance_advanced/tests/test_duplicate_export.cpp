// #142 (QA-B-54): two DLLs export xpe_calc_exposure_index, and a caller that
// links both cannot tell which one it reached.
//
// api-spec §4 says the symbol "moved to enhance_basic" / "moved from
// enhance_advanced". It did not move: xpe_enhance_basic.dll and
// xpe_enhance_advanced.dll both export it (dumpbin, QA-B-54), with signatures
// that differ only in parameter names, so a translation unit including both
// headers -- tests/e2e_post_pipeline/test_e2e_full_pipeline.cpp does exactly
// that -- compiles, links, and calls ONE of them. Which one is decided by link
// order, and nothing in the source says which.
//
// Measured: that binary imports the symbol from xpe_enhance_basic.dll
// (dumpbin /imports, _imports.log). So enhance_advanced's copy is unreachable
// there -- not by design, by link order.
//
// This test does not argue about which should win. It asks the question the
// documentation answers wrongly and the linker answers silently: DO THE TWO
// IMPLEMENTATIONS AGREE? Each DLL is opened by name, so both are reached in one
// process and the link-order question is bypassed entirely.
//
// THEY DO NOT AGREE. For one uniform input, with both modules properly
// initialised:
//
//     xpe_enhance_basic.dll     rc=0  EI=200     DI=0
//     xpe_enhance_advanced.dll  rc=0  EI=100000  DI=26.0206
//
// Same symbol, same signature, different clinical numbers -- and which one a
// caller gets is decided by link order. They also differ in PRECONDITION: the
// advanced copy returns XPE_ERR_NOT_INITIALIZED until xpe_enhance_advanced_init
// has run, the basic copy has no such requirement.
//
// Nothing is fixed here: removing an export, or reconciling two algorithms, is
// an API decision and belongs to a decision, not to a test. KnownDivergence_
// records today's state so the decision shows up as a change. The assertion is
// on the DISAGREEMENT rather than on the two values, because the values are
// algorithm constants that may legitimately move -- what must not move quietly
// is the fact that the two answers differ.

#include <gtest/gtest.h>

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#if defined(_WIN32)
#  include <windows.h>
#endif

#include <cmath>
#include <cstdint>
#include <vector>

#if defined(_WIN32)

namespace {

using CalcEiFn = XpeErrorCode (*)(const XpeImageBuffer*, const XpeImageMetadata*,
                                  float*, float*);
using InitFn   = XpeErrorCode (*)(const char*);
using ShutFn   = void (*)(void);

struct LoadedFn {
    HMODULE  mod = nullptr;
    CalcEiFn fn  = nullptr;
};

LoadedFn Load(const char* dll) {
    LoadedFn r;
    r.mod = LoadLibraryA(dll);
    if (r.mod == nullptr) return r;
    r.fn = reinterpret_cast<CalcEiFn>(
        reinterpret_cast<void*>(GetProcAddress(r.mod, "xpe_calc_exposure_index")));
    return r;
}

}  // namespace

TEST(DuplicateExportTest, KnownDivergence_CalcExposureIndexDiffersBetweenTwoDlls) {
    LoadedFn basic = Load("xpe_enhance_basic.dll");
    LoadedFn adv   = Load("xpe_enhance_advanced.dll");

    ASSERT_NE(nullptr, basic.mod) << "xpe_enhance_basic.dll did not load";
    ASSERT_NE(nullptr, adv.mod)   << "xpe_enhance_advanced.dll did not load";

    // The claim under test is that BOTH export it. If either lookup fails, the
    // symbol really did move and api-spec §4 is right after all -- say so.
    ASSERT_NE(nullptr, basic.fn) << "xpe_enhance_basic.dll does not export it";
    ASSERT_NE(nullptr, adv.fn)
        << "xpe_enhance_advanced.dll does not export it -- the symbol DID move "
           "and the QA-B-54 finding needs revisiting";

    EXPECT_NE(basic.fn, adv.fn)
        << "both handles resolved to the same address, so the two DLLs are not "
           "actually carrying separate implementations";

    // A mid-grey image: the exposure index is a function of the mean, so a
    // uniform image makes the expected value easy to reason about and any
    // disagreement easy to see.
    constexpr uint32_t kW = 64, kH = 64;
    std::vector<float> pixels(static_cast<size_t>(kW) * kH, 1000.0f);
    XpeImageBuffer img{};
    img.width = kW;
    img.height = kH;
    img.format = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32;
    img.bitsStored = 32;
    img.data = pixels.data();
    img.dataSize = static_cast<uint32_t>(pixels.size() * sizeof(float));

    XpeImageMetadata meta{};

    // enhance_advanced is a module with an init/shutdown lifecycle; calling it
    // cold returns XPE_ERR_NOT_INITIALIZED. Initialising it first is what makes
    // the comparison about the two IMPLEMENTATIONS rather than about a step this
    // test forgot -- the same control discipline QA-B-52 needed. The difference
    // in precondition is itself recorded below, because it is real: the two
    // exports are not interchangeable even when both are present.
    auto advInit = reinterpret_cast<InitFn>(
        reinterpret_cast<void*>(GetProcAddress(adv.mod, "xpe_enhance_advanced_init")));
    auto advShut = reinterpret_cast<ShutFn>(
        reinterpret_cast<void*>(GetProcAddress(adv.mod, "xpe_enhance_advanced_shutdown")));
    ASSERT_NE(nullptr, advInit);
    ASSERT_NE(nullptr, advShut);

    // Cold call first: this is the precondition difference, measured.
    float eiCold = 0.0f, diCold = 0.0f;
    const XpeErrorCode rcCold = adv.fn(&img, &meta, &eiCold, &diCold);
    GTEST_LOG_(INFO) << "enhance_advanced BEFORE init rc=" << rcCold;
    EXPECT_EQ(XPE_ERR_NOT_INITIALIZED, rcCold)
        << "recorded: the advanced copy requires xpe_enhance_advanced_init and the "
           "basic copy does not, so the two are not interchangeable";

    ASSERT_EQ(XPE_OK, advInit(nullptr));

    float eiBasic = 0.0f, diBasic = 0.0f;
    float eiAdv = 0.0f, diAdv = 0.0f;
    const XpeErrorCode rcBasic = basic.fn(&img, &meta, &eiBasic, &diBasic);
    const XpeErrorCode rcAdv   = adv.fn(&img, &meta, &eiAdv, &diAdv);

    GTEST_LOG_(INFO) << "enhance_basic    rc=" << rcBasic
                     << " EI=" << eiBasic << " DI=" << diBasic;
    GTEST_LOG_(INFO) << "enhance_advanced rc=" << rcAdv
                     << " EI=" << eiAdv << " DI=" << diAdv;

    ASSERT_EQ(XPE_OK, rcBasic);
    ASSERT_EQ(XPE_OK, rcAdv) << "both are initialised, so both should now compute";

    // The record. If this starts failing, the two exports have been reconciled
    // or one has been withdrawn -- both are outcomes worth noticing, so update
    // the case and the QA-B-54 record rather than deleting it.
    EXPECT_NE(eiBasic, eiAdv)
        << "the two exports now AGREE on the exposure index -- the divergence "
           "QA-B-54 recorded has been resolved; say how, and retire this case";

    advShut();
    FreeLibrary(basic.mod);
    FreeLibrary(adv.mod);
}

#endif  // _WIN32
