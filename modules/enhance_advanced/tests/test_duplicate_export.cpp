// #153 (QA-B-55): the two exposure-index implementations, after the rename.
//
// QA-B-54 measured this: xpe_enhance_basic.dll and xpe_enhance_advanced.dll
// both exported xpe_calc_exposure_index, with identical signatures and
// different answers -- EI 200 against EI 100000 for one uniform input, a factor
// of 500. A translation unit that includes both headers (there is one:
// tests/e2e_post_pipeline/test_e2e_full_pipeline.cpp) compiled, linked, and
// called ONE of them, chosen by link order, with no diagnostic anywhere.
//
// The advanced export is now xpe_adv_calc_exposure_index (#153). basic is
// untouched, because the C# side binds xpe_calc_exposure_index by name and all
// of those bindings mean basic.
//
// Two things are asserted here, and they are different claims:
//
//   1. THE COLLISION IS GONE. Neither DLL exports the other's name. This is the
//      regression guard, and it is the ONLY one that fires: measured by
//      re-adding the old name as an exported alias, this test failed while the
//      build stayed green (BUILD=0, QA-B-55 _falsify.log). Nothing in the
//      compiler catches a re-collision -- two headers declaring one name with
//      one signature is a legal redeclaration, which is how the original
//      collision survived unnoticed.
//   2. THE TWO IMPLEMENTATIONS STILL DISAGREE. Renaming made the choice
//      visible; it did not make the answers equal. Which of the two satisfies
//      REQ-ENH-030 / REQ-ADV-013 is a separate question this card does not
//      answer, so the disagreement stays recorded rather than resolved.
//
// The assertion is on the DISAGREEMENT rather than on the two values, because
// the values are algorithm constants that may legitimately move -- what must
// not move quietly is the fact that the two answers differ.
//
// Each DLL is opened by name, so both are reached in one process and link order
// plays no part in what this test observes.

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

constexpr const char* kBasicSymbol = "xpe_calc_exposure_index";
constexpr const char* kAdvSymbol   = "xpe_adv_calc_exposure_index";

struct LoadedFn {
    HMODULE  mod = nullptr;
    CalcEiFn fn  = nullptr;
};

LoadedFn Load(const char* dll, const char* symbol) {
    LoadedFn r;
    r.mod = LoadLibraryA(dll);
    if (r.mod == nullptr) return r;
    r.fn = reinterpret_cast<CalcEiFn>(
        reinterpret_cast<void*>(GetProcAddress(r.mod, symbol)));
    return r;
}

}  // namespace

TEST(DuplicateExportTest, KnownDivergence_RenamedExportsStillDisagree) {
    LoadedFn basic = Load("xpe_enhance_basic.dll", kBasicSymbol);
    LoadedFn adv   = Load("xpe_enhance_advanced.dll", kAdvSymbol);

    ASSERT_NE(nullptr, basic.mod) << "xpe_enhance_basic.dll did not load";
    ASSERT_NE(nullptr, adv.mod)   << "xpe_enhance_advanced.dll did not load";

    ASSERT_NE(nullptr, basic.fn) << "xpe_enhance_basic.dll does not export "
                                 << kBasicSymbol;
    ASSERT_NE(nullptr, adv.fn)   << "xpe_enhance_advanced.dll does not export "
                                 << kAdvSymbol;

    // Claim 1: the collision is gone. Neither DLL answers to the other's name.
    EXPECT_EQ(nullptr, GetProcAddress(adv.mod, kBasicSymbol))
        << "xpe_enhance_advanced.dll exports " << kBasicSymbol << " again -- the "
           "#153 collision is back, and a C++ caller that includes both headers "
           "once more gets whichever the linker picked, silently";
    EXPECT_EQ(nullptr, GetProcAddress(basic.mod, kAdvSymbol))
        << "xpe_enhance_basic.dll exports " << kAdvSymbol << ", which would make "
           "the new name ambiguous in its turn";

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
