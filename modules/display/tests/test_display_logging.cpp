/**
 * @file test_display_logging.cpp
 * @brief REQ-DISP-031: the display functions log through xpe_common's logger (QA-B-200 M2a, #251; found by QA-B-199).
 *
 * REQ-DISP-031: "Each display function SHALL log entry/exit at DEBUG level and error conditions at ERROR level via the
 * logging subsystem (xpe_common.dll)." Until QA-B-200 the module logged nothing: the M1 reproduction wrote a control line
 * to xpe_common's log file, called five display functions, and found no line from the module.
 *
 * Measured through xpe_common's own log file (xpe_log_set_file / xpe_log_set_level), with a control line written through
 * the same logger so that "no line" means "not logged" and not "the file is not receiving anything".
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

constexpr int kLevelDebug = 1;
constexpr int kLevelInfo = 2;
constexpr int kLevelError = 4;

XpeImageBuffer MakeFloatImage(uint32_t w, uint32_t h, float fill) {
    XpeImageBuffer img{};
    img.width = w;
    img.height = h;
    img.format = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32;
    img.bitsStored = 32;
    img.dataSize = static_cast<size_t>(w) * h * sizeof(float);
    float* p = static_cast<float*>(std::malloc(img.dataSize));
    for (size_t i = 0; i < static_cast<size_t>(w) * h; ++i) p[i] = fill;
    img.data = p;
    return img;
}

/** Routes xpe_common's logger to a fresh file at @p level for the life of the object, and reads it back. */
struct LogCapture {
    std::string path;
    explicit LogCapture(int level) {
        char tmp[MAX_PATH] = {0};
        GetTempPathA(MAX_PATH, tmp);
        static int serial = 0;
        path = std::string(tmp) + "xpe_display_logging_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(serial++) + ".log";
        std::remove(path.c_str());
        EXPECT_EQ(XPE_OK, xpe_log_set_level(level));
        EXPECT_EQ(XPE_OK, xpe_log_set_file(path.c_str()));
    }
    ~LogCapture() {
        xpe_log_set_file(nullptr);
        xpe_log_set_level(kLevelInfo);
        std::remove(path.c_str());
    }
    LogCapture(const LogCapture&) = delete;
    LogCapture& operator=(const LogCapture&) = delete;
    std::string Read() const {
        xpe_log_flush();
        std::ifstream f(path, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }
};

int Count(const std::string& text, const std::string& needle) {
    int n = 0;
    for (size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + needle.size())) ++n;
    return n;
}

/** Calls every logging display function once with valid input and once with an input it refuses. */
void CallEveryFunctionOnceGoodAndOnceBad() {
    XpeModalityLutParams mp{};
    mp.mode = XPE_MODALITY_LUT_LINEAR;
    mp.rescaleSlope = 1.0f;
    XpeImageBuffer img = MakeFloatImage(4, 4, 100.0f);
    (void)xpe_apply_modality_lut(&img, &mp);
    (void)xpe_apply_modality_lut(nullptr, &mp);

    XpeVoiLutParams vp{};
    vp.mode = XPE_VOI_LINEAR;
    vp.center = 50.0f;
    vp.width = 100.0f;
    vp.maxOut = 1.0f;
    (void)xpe_apply_voi_lut(&img, &vp);
    (void)xpe_apply_voi_lut(nullptr, &vp);
    std::free(img.data);

    XpeVoiLutParams preset{};
    (void)xpe_voi_preset_create(&preset, XPE_BODY_LUNG);
    (void)xpe_voi_preset_create(nullptr, XPE_BODY_LUNG);

    float lum[10];
    for (int i = 0; i < 10; ++i) lum[i] = 1.0f + static_cast<float>(i) * 10.0f;
    XpePresentationLutParams lut{};
    (void)xpe_gsdf_calibrate(lum, 10, &lut);
    (void)xpe_gsdf_calibrate(nullptr, 10, &lut);

    XpeImageBuffer img2 = MakeFloatImage(4, 4, 0.5f);
    (void)xpe_apply_presentation_lut(&img2, &lut);
    std::free(img2.data);
    (void)xpe_apply_presentation_lut(nullptr, nullptr);
}

const char* const kFunctions[] = {"xpe_apply_modality_lut", "xpe_apply_voi_lut", "xpe_voi_preset_create", "xpe_gsdf_calibrate",
                                  "xpe_apply_presentation_lut"};

}  // namespace

TEST(DisplayLogging, AtDebugLevelEveryFunctionLogsItsEntryItsExitAndItsErrors) {
    LogCapture log(kLevelDebug);
    spdlog::default_logger()->error("LOGGING-CONTROL-LINE");   // the control: the file receives what the logger gets
    CallEveryFunctionOnceGoodAndOnceBad();
    const std::string text = log.Read();
    ASSERT_EQ(1, Count(text, "LOGGING-CONTROL-LINE")) << "control: the log file receives the logger's lines";
    for (const char* fn : kFunctions) {
        const std::string name(fn);
        EXPECT_EQ(2, Count(text, "[debug] [xpe_display] " + name + ": enter")) << name << ": one entry line per call (two calls)";
        EXPECT_EQ(1, Count(text, "[debug] [xpe_display] " + name + ": exit OK")) << name << ": one exit line, for the call that succeeded";
        EXPECT_EQ(1, Count(text, "[error] [xpe_display] " + name + " failed: code")) << name << ": one ERROR line, for the call that failed";
    }
    EXPECT_NE(std::string::npos, text.find("(Invalid input parameter)")) << "the error line names the code, not only a number";
}

TEST(DisplayLogging, AtTheDefaultLevelAnErrorIsVisibleAndASuccessIsSilent) {
    // xpe_common's default is INFO: the DEBUG entry/exit lines are below it and ERROR lines are above it.
    LogCapture log(kLevelInfo);
    CallEveryFunctionOnceGoodAndOnceBad();
    const std::string text = log.Read();
    for (const char* fn : kFunctions) {
        const std::string name(fn);
        EXPECT_EQ(1, Count(text, "[error] [xpe_display] " + name + " failed: code")) << name << ": the refusal is logged at ERROR";
    }
    EXPECT_EQ(0, Count(text, "[debug]")) << "entry and exit are DEBUG: not visible at INFO";
}

TEST(DisplayLogging, AtErrorLevelOnlyTheErrorsAreWritten) {
    LogCapture log(kLevelError);
    CallEveryFunctionOnceGoodAndOnceBad();
    const std::string text = log.Read();
    EXPECT_EQ(5, Count(text, "[error] [xpe_display]")) << "one per function (one bad call each)";
    EXPECT_EQ(0, Count(text, "[debug]"));
    EXPECT_EQ(0, Count(text, "[info]"));
}

TEST(DisplayLogging, LoggingNeverChangesWhatAFunctionReturns) {
    // Same inputs with the logger fully verbose and fully off: the same return codes.
    const int levels[] = {kLevelDebug, 5};   // 5 = OFF
    XpeErrorCode results[2][4];
    for (int k = 0; k < 2; ++k) {
        LogCapture log(levels[k]);
        XpeModalityLutParams mp{};
        mp.mode = XPE_MODALITY_LUT_LINEAR;
        mp.rescaleSlope = 1.0f;
        XpeImageBuffer img = MakeFloatImage(4, 4, 100.0f);
        results[k][0] = xpe_apply_modality_lut(&img, &mp);
        results[k][1] = xpe_apply_modality_lut(nullptr, &mp);
        mp.rescaleSlope = 0.0f;
        results[k][2] = xpe_apply_modality_lut(&img, &mp);
        results[k][3] = xpe_apply_presentation_lut(&img, nullptr);
        std::free(img.data);
    }
    for (int i = 0; i < 4; ++i) EXPECT_EQ(results[0][i], results[1][i]) << "call " << i;
    EXPECT_EQ(XPE_OK, results[0][0]);
    EXPECT_NE(XPE_OK, results[0][1]);
}
