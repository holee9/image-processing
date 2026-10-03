/**
 * @file test_repro_qa_b_200.cpp
 * @brief QA-B-200 M1: the two display candidates of QA-B-199 (D5, D9).
 *
 * D5 is NOT a defect (the user decided the documents follow the code: gsdfEnabled is an annotation) and is an ACTIVE test
 * that records the behaviour. D9 reproduces a defect that is not fixed yet and stays DISABLED_: built, never run by ctest;
 * run by hand with --gtest_also_run_disabled_tests, and its OUTPUT is the evidence (.moai/reports/lane-post/QA-B-200/).
 * A test that FAILS reproduces a defect; one that passes shows the candidate was not one. The tests of a candidate that is
 * confirmed are enabled by the M2 that fixes it.
 *
 * A test executable in which every test is DISABLED_ runs nothing, and CI's single-process step refuses "a run that
 * executes nothing" (QA-B-203, main b3458366): that is why D5 is active.
 *
 * D5  REQ-DISP-024: "WHEN gsdfEnabled is non-zero in the params, the system SHALL apply the GSDF-calibrated LUT entries".
 *     The candidate: xpe_apply_presentation_lut never reads gsdfEnabled. What is measured here: whether the flag changes
 *     anything the function does, and whether the entries are applied either way.
 * D9  REQ-DISP-031: "Each display function SHALL log entry/exit at DEBUG level and error conditions at ERROR level via
 *     the logging subsystem (xpe_common.dll)." The candidate: the module logs nothing. Measured through xpe_common's own
 *     log file, with a control line that proves the file receives what the logger gets.
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

// ---------------------------------------------------------------------------------------------------------------------
// D9
// ---------------------------------------------------------------------------------------------------------------------

namespace {

std::string ReadAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

int CountLines(const std::string& text, const char* needle) {
    int n = 0;
    size_t at = 0;
    while ((at = text.find(needle, at)) != std::string::npos) {
        ++n;
        at += std::strlen(needle);
    }
    return n;
}

}  // namespace

TEST(ReproQaB200Display, DISABLED_D9_TheModuleLogsEntryExitAndErrorsThroughTheCommonLogger) {
    char tmp[MAX_PATH] = {0};
    GetTempPathA(MAX_PATH, tmp);
    const std::string logPath = std::string(tmp) + "xpe_display_repro_d9_" + std::to_string(GetCurrentProcessId()) + ".log";
    std::remove(logPath.c_str());
    ASSERT_EQ(XPE_OK, xpe_log_set_level(1)) << "level 1 = DEBUG, the level REQ-DISP-031 names (see xpe_common_api.h)";
    ASSERT_EQ(XPE_OK, xpe_log_set_file(logPath.c_str()));

    // control: a line written through the same default logger reaches the file, so an empty result means "not logged"
    spdlog::default_logger()->error("D9-CONTROL-LINE");

    // one successful call and one error call of every function that has an entry point
    XpeModalityLutParams mp{};
    mp.mode = XPE_MODALITY_LUT_LINEAR;
    mp.rescaleSlope = 1.0f;
    mp.rescaleIntercept = 0.0f;
    std::vector<float> px(16, 100.0f);
    XpeImageBuffer img = MakeFloatImage(4, 4, px);
    (void)xpe_apply_modality_lut(&img, &mp);                 // success
    (void)xpe_apply_modality_lut(nullptr, &mp);              // error: NULL image
    XpeVoiLutParams vp{};
    vp.mode = XPE_VOI_LINEAR;
    vp.center = 50.0f;
    vp.width = 100.0f;
    vp.minOut = 0.0f;
    vp.maxOut = 1.0f;
    (void)xpe_apply_voi_lut(&img, &vp);                      // success or refusal, either is a call
    (void)xpe_apply_voi_lut(nullptr, &vp);                   // error
    (void)xpe_apply_presentation_lut(nullptr, nullptr);      // error
    xpe_log_flush();
    std::free(img.data);

    const std::string text = ReadAll(logPath);
    const int control = CountLines(text, "D9-CONTROL-LINE");
    const int fromDisplay = CountLines(text, "xpe_apply_") + CountLines(text, "modality") + CountLines(text, "voi_lut") +
                            CountLines(text, "presentation") + CountLines(text, "gsdf");
    std::printf("D9 OBSERVED: log file has %zu bytes; the control line appears %d time(s); lines that name a display "
                "function: %d\n",
                text.size(), control, fromDisplay);
    std::printf("D9 LOG FILE CONTENT BEGIN\n%s\nD9 LOG FILE CONTENT END\n", text.c_str());
    ASSERT_EQ(1, control) << "the control: the log file receives what the logger gets";
    EXPECT_GT(fromDisplay, 0) << "REQ-DISP-031: the display functions log entry/exit at DEBUG and errors at ERROR";
    xpe_log_set_file(nullptr);
    std::remove(logPath.c_str());
}
