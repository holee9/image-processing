/**
 * @file test_repro_qa_b_200.cpp
 * @brief QA-B-200 M1: display candidate D5 of QA-B-199 (not a defect), kept as an ACTIVE record of the behaviour.
 *
 * D5 is an active test: the user decided the documents follow the code (gsdfEnabled is an annotation), and a test executable
 * in which every test is DISABLED_ runs nothing, which CI's single-process step refuses ("a run that executes nothing is
 * not a pass", QA-B-203, main b3458366). A reproduction of an unfixed defect would be DISABLED_ and run by hand
 * (--gtest_also_run_disabled_tests) and its OUTPUT is the evidence (.moai/reports/lane-post/QA-B-200/). A test that
 * FAILS reproduces a defect; one that passes shows the candidate was not one. The tests of a candidate that is
 * confirmed are enabled by the M2 that fixes it.
 *
 * D5  REQ-DISP-024: "WHEN gsdfEnabled is non-zero in the params, the system SHALL apply the GSDF-calibrated LUT entries".
 *     The candidate: xpe_apply_presentation_lut never reads gsdfEnabled. What is measured here: whether the flag changes
 *     anything the function does, and whether the entries are applied either way.
 * D9  REQ-DISP-031 (the module logged nothing) was CONFIRMED by the M1 run and FIXED in M2a: its tests are now
 *     test_display_logging.cpp and are enabled. REQ-DISP-030 was not a defect (the module has no throwing construct).
 */

#include <gtest/gtest.h>

#include <spdlog/spdlog.h>

#include "xpe/common/xpe_common_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/display/display_api.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <windows.h>

namespace {

XpeImageBuffer MakeFloatImage(uint32_t w, uint32_t h, const std::vector<float>& px) {
    XpeImageBuffer img{};
    img.width = w;
    img.height = h;
    img.format = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32;
    img.bitsStored = 32;
    img.dataSize = px.size() * sizeof(float);
    img.data = std::malloc(img.dataSize);
    std::memcpy(img.data, px.data(), img.dataSize);
    return img;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------------
// D5
// ---------------------------------------------------------------------------------------------------------------------

TEST(ReproQaB200Display, D5_GsdfEnabledDoesNotChangeWhatThePresentationLutDoes) {
    float lum[10];
    for (int i = 0; i < 10; ++i) lum[i] = 1.0f + static_cast<float>(i) * 10.0f;   // 1..91 cd/m^2
    XpePresentationLutParams on{};
    ASSERT_EQ(XPE_OK, xpe_gsdf_calibrate(lum, 10, &on));
    ASSERT_EQ(1, on.gsdfEnabled) << "control: the calibration sets the flag";
    XpePresentationLutParams off = on;
    off.gsdfEnabled = 0;

    std::vector<float> px;
    for (int i = 0; i < 64; ++i) px.push_back(static_cast<float>(i) / 63.0f);
    XpeImageBuffer a = MakeFloatImage(8, 8, px), b = MakeFloatImage(8, 8, px);
    ASSERT_EQ(XPE_OK, xpe_apply_presentation_lut(&a, &on));
    ASSERT_EQ(XPE_OK, xpe_apply_presentation_lut(&b, &off));
    ASSERT_EQ(a.dataSize, b.dataSize);
    const bool same = std::memcmp(a.data, b.data, a.dataSize) == 0;
    const uint16_t* pa = static_cast<const uint16_t*>(a.data);
    int differing = 0;
    for (size_t i = 0; i < a.dataSize / 2; ++i) differing += (pa[i] != static_cast<const uint16_t*>(b.data)[i]);
    std::printf("D5 OBSERVED: flag=1 and flag=0 give %s output (%d of %zu pixels differ); first pixels flag=1: %u %u %u, "
                "flag=0: %u %u %u\n",
                same ? "IDENTICAL" : "DIFFERENT", differing, a.dataSize / 2, pa[0], pa[1], pa[2],
                static_cast<const uint16_t*>(b.data)[0], static_cast<const uint16_t*>(b.data)[1],
                static_cast<const uint16_t*>(b.data)[2]);
    // Is the LUT applied at all? A pixel at 1.0 must come out as lutData[1023], and at 0.0 as lutData[0].
    EXPECT_EQ(on.lutData[1023], pa[63]) << "the entries ARE applied (control)";
    EXPECT_EQ(on.lutData[0], pa[0]);
    // The behaviour, as decided: REQ-DISP-024's flag is an annotation (it says the entries are GSDF-calibrated); the
    // function applies the entries it is given either way. Recorded here so a change of that behaviour is noticed.
    EXPECT_TRUE(same) << "gsdfEnabled is an annotation: the same entries are applied with the flag 0 and 1";
    std::free(a.data);
    std::free(b.data);
}
