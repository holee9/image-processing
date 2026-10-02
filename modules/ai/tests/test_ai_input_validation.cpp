/**
 * @file test_ai_input_validation.cpp
 * @brief REQ-AI-090 input validation at the entrance of the xpe_ai exports (QA-B-194, #130, T-012).
 *
 * M1: a float image that holds NaN or infinity is refused BEFORE anything is written -- XPE_ERR_INVALID_INPUT, every
 * output untouched, ONE XPE_ALERT_ERROR naming the count and the first pixel and blaming the INPUT -- by all four
 * functions that take pixels; and the size validator knows the pixel size of every format (a UINT8 image that
 * declares 4000 x 4000 and supplies one byte used to pass).
 *
 * Nothing here needs a model: every refusal happens before the model is looked at, so the tests run in the stub
 * build and in the ONNX build alike. The alert texts are written out in the test (only the numbers are computed
 * from the test's own input), so a test that rebuilt the text with the module's formatter cannot agree with it by
 * being wrong the same way.
 */

#include <gtest/gtest.h>

#include "xpe/ai/ai_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"

#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#ifndef XPE_AI_TEST_DATA_DIR
#error "XPE_AI_TEST_DATA_DIR must be defined by the build (modules/ai/CMakeLists.txt)"
#endif

namespace {

const std::string kData = XPE_AI_TEST_DATA_DIR;
const float kNaN = std::numeric_limits<float>::quiet_NaN();
const float kInf = std::numeric_limits<float>::infinity();

/** A float image with a known good ramp; the XpeImageBuffer is built on demand (a stored one dangles after a copy). */
struct Img {
    uint32_t w, h;
    std::vector<float> v;
    XpePixelFormat format = XPE_PIXEL_FLOAT32;
    Img(uint32_t width, uint32_t height, float base = 0.25f) : w(width), h(height), v(static_cast<size_t>(width) * height) {
        for (size_t i = 0; i < v.size(); ++i) v[i] = base + 0.01f * static_cast<float>(i % 16);
    }
    XpeImageBuffer Buffer() const {
        XpeImageBuffer b{};
        b.width = w;
        b.height = h;
        b.bitsAllocated = 32;
        b.bitsStored = 32;
        b.format = format;
        b.data = const_cast<float*>(v.data());
        b.dataSize = v.size() * sizeof(float);
        return b;
    }
};

std::vector<std::string> Alerts(std::vector<int32_t>* severities = nullptr) {
    std::vector<std::string> out;
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < n; ++i) {
        char msg[512] = {0};
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) == XPE_OK) {
            out.push_back(msg);
            if (severities) severities->push_back(sev);
        }
    }
    return out;
}

XpeImageMetadata Meta() {
    XpeImageMetadata m{};
    strcpy_s(m.bodyPart, sizeof(m.bodyPart), "CHEST");
    m.kVp = 80.0f;
    m.mAs = 5.0f;
    m.SID_mm = 1000.0f;
    m.pixelPitch_mm = 0.15f;
    return m;
}

struct AiInputValidation : public ::testing::Test {
    void SetUp() override {
        xpe_ai_shutdown();
        xpe_clear_alerts();
        ASSERT_EQ(XPE_OK, xpe_ai_init((kData + "/models_x2").c_str(), "{}"));
        xpe_clear_alerts();
    }
    void TearDown() override {
        xpe_ai_shutdown();
        xpe_clear_alerts();
    }
};

/** What one function call did: its code and whether anything it owns was written. */
struct Outcome {
    XpeErrorCode rc;
    bool outputsUntouched;
};

constexpr float kSentinel = -12345.0f;

/** One entry point under test, called with @p img as its input. */
struct Subject {
    const char* name;
    const char* prefix;
    const char* tail;
    const char* what;   // how the alert names the frame
    std::function<Outcome(Img&)> call;
};

const std::vector<Subject>& Subjects() {
    static const std::vector<Subject> s = {
        {"xpe_bone_suppress", "XPE_WARN_BONE_SUPPRESS_INPUT_NOT_FINITE:",
         "the image was not processed and the output buffer was not changed", "the input frame",
         [](Img& in) {
             Img out(in.w, in.h);
             std::fill(out.v.begin(), out.v.end(), kSentinel);
             const XpeImageBuffer ib = in.Buffer();
             XpeImageBuffer ob = out.Buffer();
             Outcome o;
             o.rc = xpe_bone_suppress(&ib, &ob, nullptr);
             o.outputsUntouched = std::all_of(out.v.begin(), out.v.end(), [](float x) { return x == kSentinel; });
             return o;
         }},
        {"xpe_bodypart_recognize", "XPE_WARN_BODYPART_INPUT_NOT_FINITE:",
         "the image was not classified and the label and confidence were not written", "the input frame",
         [](Img& in) {
             char label[64];
             std::memset(label, 'x', sizeof(label));
             float conf = kSentinel;
             const XpeImageBuffer ib = in.Buffer();
             Outcome o;
             o.rc = xpe_bodypart_recognize(&ib, label, sizeof(label), &conf);
             bool labelUntouched = true;
             for (char ch : label) labelUntouched = labelUntouched && ch == 'x';
             o.outputsUntouched = labelUntouched && conf == kSentinel;
             return o;
         }},
        {"xpe_dl_denoise", "XPE_WARN_DL_DENOISE_INPUT_NOT_FINITE:",
         "the image was not denoised and the buffer was not changed", "the input frame",
         [](Img& in) {
             const std::vector<float> before = in.v;
             XpeImageBuffer ib = in.Buffer();
             const XpeImageMetadata meta = Meta();
             Outcome o;
             o.rc = xpe_dl_denoise(&ib, &meta, nullptr);
             // NaN compares unequal to itself, so the comparison is bytewise
             o.outputsUntouched = std::memcmp(before.data(), in.v.data(), before.size() * sizeof(float)) == 0;
             return o;
         }},
        // the bad frame is part 0; part 1 is clean, so the alert must name part 0
        {"xpe_stitch_images", "XPE_WARN_STITCH_INPUT_NOT_FINITE:",
         "the parts were not stitched and the output buffer was not written", "input part 0",
         [](Img& in) {
             Img other(in.w, in.h);
             const XpeImageBuffer parts[2] = {in.Buffer(), other.Buffer()};
             Img out(in.w * 2, in.h);
             std::fill(out.v.begin(), out.v.end(), kSentinel);
             XpeImageBuffer ob = out.Buffer();
             Outcome o;
             o.rc = xpe_stitch_images(parts, 2, &ob, nullptr);
             o.outputsUntouched = std::all_of(out.v.begin(), out.v.end(), [](float x) { return x == kSentinel; });
             return o;
         }},
    };
    return s;
}

std::string Expected(const Subject& s, size_t count, size_t index, uint32_t width) {
    const char* what = s.what;
    return std::string(s.prefix) + " " + std::to_string(count) + " pixel(s) of " + what +
           " are NaN or infinite (first: index " + std::to_string(index) + ", x=" + std::to_string(index % width) +
           ", y=" + std::to_string(index / width) + "); " + s.tail;
}

}  // namespace

// ===== the entrance refuses a non-finite frame, and says whose fault it is ===============================

TEST_F(AiInputValidation, ControlAFiniteFrameIsNotRefusedAndRaisesNoNonFiniteAlert) {
    // Without this, every refusal below could be "the function refuses everything". The extreme finite value is
    // the control that matters most: FLT_MAX is large but it is a number.
    for (const Subject& s : Subjects()) {
        for (const float base : {0.25f, FLT_MAX, -FLT_MAX, 0.0f}) {
            Img img(8, 8, base);
            if (base == FLT_MAX || base == -FLT_MAX) {
                for (float& x : img.v) x = base;
            }
            xpe_clear_alerts();
            const Outcome o = s.call(img);
            EXPECT_NE(XPE_ERR_INVALID_INPUT, o.rc) << s.name << " base=" << base;
            for (const std::string& a : Alerts()) {
                EXPECT_EQ(std::string::npos, a.find("INPUT_NOT_FINITE")) << s.name << ": " << a;
            }
        }
    }
}

TEST_F(AiInputValidation, ANonFiniteFrameIsRefusedBeforeAnythingIsWrittenByEveryFunctionThatTakesPixels) {
    struct Where { const char* name; size_t (*index)(size_t n); };
    const Where wheres[] = {
        {"first pixel", [](size_t) -> size_t { return 0; }},
        {"middle", [](size_t n) -> size_t { return n / 2; }},
        {"last pixel", [](size_t n) -> size_t { return n - 1; }},
    };
    for (const Subject& s : Subjects()) {
        for (const float bad : {kNaN, kInf, -kInf}) {
            for (const Where& w : wheres) {
                Img img(8, 6);
                const size_t at = w.index(img.v.size());
                img.v[at] = bad;
                xpe_clear_alerts();
                const Outcome o = s.call(img);
                const std::string tag = std::string(s.name) + " " + (std::isnan(bad) ? "NaN" : bad > 0 ? "+Inf" : "-Inf") + " at " + w.name;
                EXPECT_EQ(XPE_ERR_INVALID_INPUT, o.rc) << tag;
                EXPECT_TRUE(o.outputsUntouched) << tag << ": the refusal must write nothing";
                std::vector<int32_t> sev;
                const std::vector<std::string> a = Alerts(&sev);
                ASSERT_EQ(1u, a.size()) << tag << ": exactly one alert";
                EXPECT_EQ(XPE_ALERT_ERROR, sev[0]) << tag;
                EXPECT_EQ(Expected(s, 1, at, img.w), a[0]) << tag;
                EXPECT_EQ(std::string::npos, a[0].find("model output")) << tag << ": the INPUT is at fault, not the model";
            }
        }
    }
}

TEST_F(AiInputValidation, TheAlertCountsEveryNonFinitePixelAndNamesTheFirst) {
    for (const Subject& s : Subjects()) {
        Img img(7, 5);   // 7 wide, so index 16 is x=2, y=2
        img.v[16] = kNaN;
        img.v[20] = kInf;
        img.v[34] = -kInf;
        xpe_clear_alerts();
        const Outcome o = s.call(img);
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, o.rc) << s.name;
        const std::vector<std::string> a = Alerts();
        ASSERT_EQ(1u, a.size()) << s.name;
        EXPECT_EQ(Expected(s, 3, 16, img.w), a[0]) << s.name;
    }
}

TEST_F(AiInputValidation, IntegerFramesAreNeverScanned) {
    // An integer pixel cannot be NaN. The same bytes that read as NaN in a float frame are only numbers in a UINT16
    // frame, so a UINT16 image must not be refused (nor alerted) on account of them.
    for (const Subject& s : Subjects()) {
        std::vector<uint16_t> px(8 * 6, 0xFFFFu);   // as float pairs these would be NaN
        XpeImageBuffer b{};
        b.width = 8;
        b.height = 6;
        b.bitsAllocated = 16;
        b.bitsStored = 16;
        b.format = XPE_PIXEL_UINT16;
        b.data = px.data();
        b.dataSize = px.size() * sizeof(uint16_t);
        xpe_clear_alerts();
        if (std::string(s.name) == "xpe_dl_denoise") {
            const XpeImageMetadata meta = Meta();
            EXPECT_NE(XPE_ERR_INVALID_INPUT, xpe_dl_denoise(&b, &meta, nullptr)) << s.name;
        } else if (std::string(s.name) == "xpe_bodypart_recognize") {
            char label[64];
            float conf = 0.0f;
            EXPECT_NE(XPE_ERR_INVALID_INPUT, xpe_bodypart_recognize(&b, label, sizeof(label), &conf)) << s.name;
        } else {
            continue;   // xpe_bone_suppress takes float only: its UINT16 refusal is UNSUPPORTED_FORMAT, tested elsewhere
        }
        // (a body-part call with no model raises its own, unrelated "unavailable" Warning: only the non-finite alerts
        // are in question here)
        for (const std::string& a : Alerts()) {
            EXPECT_EQ(std::string::npos, a.find("INPUT_NOT_FINITE")) << s.name << ": " << a;
            EXPECT_EQ(std::string::npos, a.find("non-finite")) << s.name << ": " << a;
        }
    }
}

TEST_F(AiInputValidation, StitchNamesTheFirstPartThatFails) {
    Img a(6, 6), b(6, 6), c(6, 6);
    b.v[7] = kNaN;
    c.v[1] = kInf;   // a later part is also bad: the first one is the one named
    const XpeImageBuffer parts[3] = {a.Buffer(), b.Buffer(), c.Buffer()};
    Img out(20, 6);
    std::fill(out.v.begin(), out.v.end(), kSentinel);
    XpeImageBuffer ob = out.Buffer();
    xpe_clear_alerts();
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_stitch_images(parts, 3, &ob, nullptr));
    EXPECT_TRUE(std::all_of(out.v.begin(), out.v.end(), [](float x) { return x == kSentinel; }));
    const std::vector<std::string> alerts = Alerts();
    ASSERT_EQ(1u, alerts.size());
    EXPECT_EQ("XPE_WARN_STITCH_INPUT_NOT_FINITE: 1 pixel(s) of input part 1 are NaN or infinite (first: index 7, x=1, y=1); "
              "the parts were not stitched and the output buffer was not written",
              alerts[0]);
}

TEST_F(AiInputValidation, TheEstimateReadsNoPixelsSoItHasNothingToRefuse) {
    // xpe_stitch_estimate_size uses the dimensions only. A NaN in the pixels cannot change its answer and is not its
    // business; this pins that it neither refuses nor alerts, so nobody "fixes" it into a scan it does not need.
    Img a(6, 6), b(6, 6);
    a.v[3] = kNaN;
    const XpeImageBuffer parts[2] = {a.Buffer(), b.Buffer()};
    uint32_t w = 0, h = 0;
    xpe_clear_alerts();
    EXPECT_EQ(XPE_OK, xpe_stitch_estimate_size(parts, 2, &w, &h));
    EXPECT_EQ(10u, w);   // 6 * (1 + 0.7) = 10.2
    EXPECT_EQ(6u, h);
    EXPECT_TRUE(Alerts().empty());
}

TEST_F(AiInputValidation, ABodyPartRequestIsRefusedWhetherOrNotAModelExists) {
    // models_x2 holds no body-part model: a finite image gets the documented fallback (UNKNOWN), a non-finite one is
    // refused -- the caller's fault is not the "no usable answer" the fallback is for, and the label stays unwritten.
    char label[64];
    std::memset(label, 'x', sizeof(label));
    float conf = kSentinel;
    Img finite(8, 8);
    const XpeImageBuffer fb = finite.Buffer();
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, xpe_bodypart_recognize(&fb, label, sizeof(label), &conf)) << "the control";
    EXPECT_STREQ("UNKNOWN", label);
    EXPECT_EQ(0.0f, conf);

    std::memset(label, 'x', sizeof(label));
    conf = kSentinel;
    Img bad(8, 8);
    bad.v[9] = kNaN;
    const XpeImageBuffer bb = bad.Buffer();
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_bodypart_recognize(&bb, label, sizeof(label), &conf));
    EXPECT_EQ('x', label[0]) << "not even UNKNOWN is written";
    EXPECT_EQ(kSentinel, conf) << "and the confidence is not reset";
}

TEST(AiInputValidationWorker, ANonFiniteBoneFrameIsRefusedBeforeAnyWorkerIsStarted) {
    // The refusal sits before the lock and the worker: no process is started, nothing is counted against the worker,
    // and the alert is the input's, not an "AI worker failed" alert.
    xpe_ai_shutdown();
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_ai_init((kData + "/models_x2").c_str(), "{\"use_worker\": true}"));
    xpe_clear_alerts();
    Img in(3, 3), out(3, 3);
    in.v[4] = kNaN;
    std::fill(out.v.begin(), out.v.end(), kSentinel);
    const XpeImageBuffer ib = in.Buffer();
    XpeImageBuffer ob = out.Buffer();
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_bone_suppress(&ib, &ob, nullptr));
    int32_t state = -1;
    uint32_t failures = 777, ceiling = 0;
    ASSERT_EQ(XPE_OK, xpe_ai_worker_state(&state, &failures, &ceiling));
    EXPECT_EQ(XPE_AI_WORKER_ACTIVE, state);
    EXPECT_EQ(0u, failures);
    const std::vector<std::string> a = Alerts();
    ASSERT_EQ(1u, a.size());
    EXPECT_NE(std::string::npos, a[0].find("XPE_WARN_BONE_SUPPRESS_INPUT_NOT_FINITE:"));
    EXPECT_EQ(std::string::npos, a[0].find("AI worker"));
    // no worker process was started
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    ASSERT_NE(INVALID_HANDLE_VALUE, snap);
    PROCESSENTRY32 pe{};
    pe.dwSize = sizeof(pe);
    int workers = 0;
    for (BOOL ok = Process32First(snap, &pe); ok; ok = Process32Next(snap, &pe)) {
        if (pe.th32ParentProcessID == GetCurrentProcessId() && _stricmp(pe.szExeFile, "xpe_ai_worker.exe") == 0) ++workers;
    }
    CloseHandle(snap);
    EXPECT_EQ(0, workers);
    xpe_ai_shutdown();
    xpe_clear_alerts();
}

// ===== the size validator knows every format ==============================================================

TEST_F(AiInputValidation, AUint8ImageThatDeclaresMoreThanItSuppliesIsRefused) {
    // Measured in QA-B-194: UINT8 4000 x 4000 with dataSize 1 passed xpe_dl_denoise's validation (bpp was 0 for UINT8
    // and the size check was skipped).
    unsigned char one = 7;
    XpeImageBuffer u8{};
    u8.width = 4000;
    u8.height = 4000;
    u8.bitsAllocated = 8;
    u8.bitsStored = 8;
    u8.format = XPE_PIXEL_UINT8;
    u8.data = &one;
    u8.dataSize = 1;
    const XpeImageMetadata meta = Meta();
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dl_denoise(&u8, &meta, nullptr));

    // The controls: the size the image declares, and "unspecified" (dataSize 0, #123), are not refused for size.
    std::vector<unsigned char> px(16 * 16, 3);
    u8.width = 16;
    u8.height = 16;
    u8.data = px.data();
    u8.dataSize = px.size();
    EXPECT_NE(XPE_ERR_INVALID_INPUT, xpe_dl_denoise(&u8, &meta, nullptr)) << "exactly as many bytes as declared";
    u8.dataSize = 0;
    EXPECT_NE(XPE_ERR_INVALID_INPUT, xpe_dl_denoise(&u8, &meta, nullptr)) << "dataSize 0 means unspecified";
    u8.dataSize = px.size() - 1;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dl_denoise(&u8, &meta, nullptr)) << "one byte short";
}

TEST_F(AiInputValidation, AFormatValueThatIsNotOneOfTheThreeIsNotAnImage) {
    Img img(8, 8);
    img.format = static_cast<XpePixelFormat>(7);
    const XpeImageMetadata meta = Meta();
    XpeImageBuffer b = img.Buffer();
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dl_denoise(&b, &meta, nullptr));
    char label[64];
    float conf = 0.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_bodypart_recognize(&b, label, sizeof(label), &conf));
    Img out(8, 8);
    XpeImageBuffer ob = out.Buffer();
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_bone_suppress(&b, &ob, nullptr));
    const XpeImageBuffer parts[2] = {b, img.Buffer()};
    uint32_t w = 0, h = 0;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_stitch_estimate_size(parts, 2, &w, &h));
}

TEST_F(AiInputValidation, TheDeclaredSizeOfEveryFormatIsBoundedByTheModuleMaximum) {
    // 64 MB is the module maximum for every format: 64M UINT8 pixels, 32M UINT16, 16M float. Called through
    // xpe_stitch_estimate_size because it validates the dimensions and then reads NO pixels: the data pointer here
    // is one byte, and a function that scanned a 16M-pixel float frame through it would be reading unowned memory.
    unsigned char one = 0;
    XpeImageBuffer parts[2] = {};
    struct Case { XpePixelFormat f; uint32_t atMax; };
    for (const Case c : {Case{XPE_PIXEL_UINT8, 67108864u}, Case{XPE_PIXEL_UINT16, 33554432u}, Case{XPE_PIXEL_FLOAT32, 16777216u}}) {
        for (XpeImageBuffer& b : parts) {
            b.height = 1;
            b.data = &one;
            b.dataSize = 0;
            b.format = c.f;
            b.width = c.atMax;
        }
        uint32_t w = 0, h = 0;
        EXPECT_EQ(XPE_OK, xpe_stitch_estimate_size(parts, 2, &w, &h)) << "format " << c.f << ": exactly the maximum";
        parts[1].width = c.atMax + 1;
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_stitch_estimate_size(parts, 2, &w, &h)) << "format " << c.f << ": one pixel over";
    }
}

// ===== M2: the parts of one stitch ==========================================================================

namespace {

/** @p n valid float parts of the same size. */
std::vector<Img> Parts(size_t n, uint32_t w = 6, uint32_t h = 6) {
    std::vector<Img> v;
    for (size_t i = 0; i < n; ++i) v.emplace_back(w, h);
    return v;
}

std::vector<XpeImageBuffer> Buffers(const std::vector<Img>& imgs) {
    std::vector<XpeImageBuffer> b;
    for (const Img& i : imgs) b.push_back(i.Buffer());
    return b;
}

}  // namespace

TEST_F(AiInputValidation, ThePartCountCapIsSixteenAndIsAnImplementationSafetyCap) {
    // The value is pinned so a change is a decision, not an accident (QA-B-194 D5: no product value exists; the
    // header says so). The behaviour at the cap is below.
    static_assert(XPE_AI_MAX_STITCH_PARTS == 16u, "the cap is a documented implementation value");
    EXPECT_EQ(16u, XPE_AI_MAX_STITCH_PARTS);
}

TEST_F(AiInputValidation, SixteenPartsAreAcceptedAndSeventeenAreRefusedByBothStitchFunctions) {
    // 17 parts exist in memory, so even a function that wrongly accepted 17 reads only valid parts.
    const std::vector<Img> imgs = Parts(17);
    const std::vector<XpeImageBuffer> b = Buffers(imgs);
    Img out(64, 6);
    XpeImageBuffer ob = out.Buffer();
    for (const uint32_t n : {2u, 15u, 16u}) {
        uint32_t w = 0, h = 0;
        EXPECT_EQ(XPE_OK, xpe_stitch_estimate_size(b.data(), n, &w, &h)) << "estimate, " << n << " parts";
        EXPECT_NE(XPE_ERR_INVALID_INPUT, xpe_stitch_images(b.data(), n, &ob, nullptr)) << "stitch, " << n << " parts";
    }
    uint32_t w = 777, h = 888;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_stitch_estimate_size(b.data(), 17, &w, &h));
    EXPECT_EQ(777u, w) << "a refused estimate leaves its outputs alone";
    EXPECT_EQ(888u, h);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_stitch_images(b.data(), 17, &ob, nullptr));
}

TEST_F(AiInputValidation, AWildPartCountIsRefusedWithoutReadingTheArray) {
    // 2 parts supplied, a count of 4 billion: the cap is judged before any parts[i] is read. (Before M2 this walked
    // billions of XpeImageBuffers off the end of the array.)
    const std::vector<Img> imgs = Parts(2);
    const std::vector<XpeImageBuffer> b = Buffers(imgs);
    uint32_t w = 0, h = 0;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_stitch_estimate_size(b.data(), 4000000000u, &w, &h));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_stitch_estimate_size(b.data(), 0xFFFFFFFFu, &w, &h));
    Img out(32, 6);
    XpeImageBuffer ob = out.Buffer();
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_stitch_images(b.data(), 4000000000u, &ob, nullptr));
}

TEST_F(AiInputValidation, ThePartsOfOneStitchHaveOnePixelFormat) {
    // Measured in QA-B-194: a UINT16 part among float parts was accepted (estimate 19 x 8).
    const std::vector<Img> imgs = Parts(3);
    const std::vector<XpeImageBuffer> b = Buffers(imgs);
    std::vector<uint16_t> u16(6 * 6, 5);
    XpeImageBuffer odd{};
    odd.width = 6;
    odd.height = 6;
    odd.bitsAllocated = 16;
    odd.bitsStored = 16;
    odd.format = XPE_PIXEL_UINT16;
    odd.data = u16.data();
    odd.dataSize = u16.size() * sizeof(uint16_t);

    Img out(32, 6);
    XpeImageBuffer ob = out.Buffer();
    xpe_clear_alerts();
    for (size_t at = 0; at < b.size(); ++at) {   // the odd one in every position, first and last included
        std::vector<XpeImageBuffer> mixed = b;
        mixed[at] = odd;
        uint32_t w = 777, h = 888;
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_stitch_estimate_size(mixed.data(), 3, &w, &h)) << "odd part at " << at;
        EXPECT_EQ(777u, w) << "odd part at " << at;
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_stitch_images(mixed.data(), 3, &ob, nullptr)) << "odd part at " << at;
    }
    // the control: all UINT16 is a consistent set and passes the entrance (a stub then fails the stitch itself)
    const std::vector<XpeImageBuffer> all16(3, odd);
    uint32_t w = 0, h = 0;
    EXPECT_EQ(XPE_OK, xpe_stitch_estimate_size(all16.data(), 3, &w, &h));
    EXPECT_NE(XPE_ERR_INVALID_INPUT, xpe_stitch_images(all16.data(), 3, &ob, nullptr));
    // and the format refusal raised no alert of its own: it is a plain INVALID_INPUT
    EXPECT_TRUE(Alerts().empty());
}

TEST_F(AiInputValidation, NoSizeRuleBetweenPartsIsInventedBecauseTheAlgorithmDoesNotExistYet) {
    // D6: parts of different sizes are accepted. How the parts of a stitch relate in size depends on a stitching
    // algorithm nobody has written; a rule made up now would be a requirement nobody gave. This test pins the
    // ABSENCE so a future tidy-up that adds one has to change this test and say why.
    const std::vector<Img> a = {Img(6, 6), Img(9, 4), Img(3, 11)};
    const std::vector<XpeImageBuffer> b = Buffers(a);
    uint32_t w = 0, h = 0;
    EXPECT_EQ(XPE_OK, xpe_stitch_estimate_size(b.data(), 3, &w, &h));
    EXPECT_GT(w, 0u);
    EXPECT_GT(h, 0u);
}

// ===== M3: the denoise metadata and the model identifier ====================================================

namespace {

/** xpe_dl_denoise on a clean 8 x 8 frame with @p meta; also reports whether the frame's bytes were left alone. */
XpeErrorCode DenoiseWith(const XpeImageMetadata& meta, bool* frameUntouched = nullptr) {
    Img img(8, 8);
    const std::vector<float> before = img.v;
    XpeImageBuffer b = img.Buffer();
    const XpeErrorCode rc = xpe_dl_denoise(&b, &meta, nullptr);
    if (frameUntouched) *frameUntouched = std::memcmp(before.data(), img.v.data(), before.size() * sizeof(float)) == 0;
    return rc;
}

}  // namespace

TEST_F(AiInputValidation, ControlMetadataThatIsMerelyUnusualIsNotRefused) {
    // Without these, every refusal below could be "denoise refuses all metadata". They also pin what is NOT judged:
    // zero (= unknown, as everywhere in the metadata), an empty body part, an extreme but finite dose, any time and
    // any flag bits. No clinical range is invented.
    XpeImageMetadata m = Meta();
    EXPECT_NE(XPE_ERR_INVALID_INPUT, DenoiseWith(m)) << "the ordinary case";

    XpeImageMetadata zeros{};
    EXPECT_NE(XPE_ERR_INVALID_INPUT, DenoiseWith(zeros)) << "all zero = all unknown";

    m = Meta();
    m.bodyPart[0] = '\0';
    EXPECT_NE(XPE_ERR_INVALID_INPUT, DenoiseWith(m)) << "an empty body part is a string";

    m = Meta();
    m.kVp = FLT_MAX;
    m.mAs = FLT_MAX;
    m.SID_mm = FLT_MAX;
    m.pixelPitch_mm = FLT_MAX;
    EXPECT_NE(XPE_ERR_INVALID_INPUT, DenoiseWith(m)) << "huge is not invalid: no range is invented";

    m = Meta();
    m.acquisitionTime = 0xFFFFFFFFFFFFFFFFull;
    m.flags = 0xFFFFFFFFu;
    EXPECT_NE(XPE_ERR_INVALID_INPUT, DenoiseWith(m)) << "time and flags are not judged";

    m = Meta();
    std::memset(m.bodyPart, 'A', sizeof(m.bodyPart) - 1);   // 63 characters + the terminator already there
    m.bodyPart[sizeof(m.bodyPart) - 1] = '\0';
    EXPECT_NE(XPE_ERR_INVALID_INPUT, DenoiseWith(m)) << "63 characters and a terminator fit";
}

TEST_F(AiInputValidation, ADoseOrGeometryThatIsNotAFiniteNonNegativeNumberIsRefusedAndTheFrameIsLeftAlone) {
    struct Field { const char* name; float XpeImageMetadata::*member; };
    const Field fields[] = {
        {"kVp", &XpeImageMetadata::kVp},
        {"mAs", &XpeImageMetadata::mAs},
        {"SID_mm", &XpeImageMetadata::SID_mm},
        {"pixelPitch_mm", &XpeImageMetadata::pixelPitch_mm},
    };
    for (const Field& f : fields) {
        for (const float bad : {kNaN, kInf, -kInf, -1.0f, -0.0001f, -FLT_MAX}) {
            XpeImageMetadata m = Meta();
            m.*(f.member) = bad;
            xpe_clear_alerts();
            bool untouched = false;
            EXPECT_EQ(XPE_ERR_INVALID_INPUT, DenoiseWith(m, &untouched)) << f.name << " = " << bad;
            EXPECT_TRUE(untouched) << f.name << " = " << bad << ": a refused call writes nothing";
            EXPECT_TRUE(Alerts().empty()) << f.name << " = " << bad << ": a plain INVALID_INPUT, like the size checks";
        }
    }
}

TEST_F(AiInputValidation, ABodyPartThatIsNotATerminatedStringIsRefused) {
    // 64 bytes of 'A' and no NUL: the field would be read past its end as a C string.
    XpeImageMetadata m = Meta();
    std::memset(m.bodyPart, 'A', sizeof(m.bodyPart));
    bool untouched = false;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, DenoiseWith(m, &untouched));
    EXPECT_TRUE(untouched);
}

TEST_F(AiInputValidation, TheMetadataIsJudgedBeforeThePixelsAreScanned) {
    // Both are wrong: the cheap metadata check answers first, and no NaN alert is raised for a frame that was never
    // looked at. (Pinned so the order is a decision: the scan is the expensive one.)
    Img img(8, 8);
    img.v[3] = kNaN;
    XpeImageMetadata m = Meta();
    m.mAs = kNaN;
    XpeImageBuffer b = img.Buffer();
    xpe_clear_alerts();
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dl_denoise(&b, &m, nullptr));
    EXPECT_TRUE(Alerts().empty());
}

namespace {

struct Card {
    XpeErrorCode rc;
    std::string text;
    bool bufferUntouched;
};

Card CardFor(const std::string& id) {
    char buf[1024];
    std::memset(buf, 'x', sizeof(buf));
    Card c;
    c.rc = xpe_ai_get_model_card(id.c_str(), buf, sizeof(buf));
    c.bufferUntouched = true;
    for (char ch : buf) c.bufferUntouched = c.bufferUntouched && ch == 'x';
    c.text = std::string(buf, strnlen(buf, sizeof(buf)));
    return c;
}

}  // namespace

TEST_F(AiInputValidation, AModelIdentifierIsOneToSixtyFourOfLettersDigitsDotUnderscoreHyphen) {
    // Controls first: the identifiers the product uses, the shortest and the longest legal one.
    EXPECT_EQ(XPE_OK, CardFor("bodypart_cnn_v1").rc);
    EXPECT_NE(XPE_ERR_INVALID_INPUT, CardFor("a").rc);
    EXPECT_NE(XPE_ERR_INVALID_INPUT, CardFor(std::string(64, 'm')).rc);
    EXPECT_NE(XPE_ERR_INVALID_INPUT, CardFor("Model-1.2_final").rc);

    const std::string refused[] = {
        "",
        std::string(65, 'm'),
        std::string("a\"b"),           // a quote: made the card invalid JSON (measured)
        std::string("a\\b"),           // a backslash
        std::string("a b"),            // a space
        std::string("a/b"),
        std::string("a\nb"),
        std::string("a\tb"),
        std::string("a\x01" "b"),
        std::string("caf\xC3\xA9"),    // not ASCII
        std::string("a{b}"),
        std::string("a,b"),
        std::string("a:b"),
    };
    for (const std::string& id : refused) {
        const Card c = CardFor(id);
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, c.rc) << "id of " << id.size() << " bytes, first '" << (id.empty() ? '?' : id[0]) << "'";
        EXPECT_TRUE(c.bufferUntouched) << "a refused identifier writes nothing to the caller's buffer";
    }
}

TEST_F(AiInputValidation, AWellFormedButUnknownIdentifierStillGetsAWellFormedUnavailableCard) {
    // The grammar must not turn "not loaded" into "invalid": an unknown identifier that is legal is a model that is
    // not loaded, the documented IO_FAILED, with a card that says so -- and the card is JSON a parser accepts, which
    // is what the quote in the identifier used to break.
    const Card c = CardFor("no_such_model.v2-x");
    EXPECT_EQ(XPE_ERR_IO_FAILED, c.rc);
    EXPECT_NE(std::string::npos, c.text.find("\"model_id\":\"no_such_model.v2-x\""));
    EXPECT_NE(std::string::npos, c.text.find("\"error\":\"model_not_loaded\""));
    EXPECT_EQ('{', c.text.front());
    EXPECT_EQ('}', c.text.back());
    EXPECT_EQ(std::string::npos, c.text.find('\\')) << "nothing in the card needs escaping";
}

// ===== M4: init and config ====================================================================================
//
// These tests own the module's lifecycle (no fixture: each one shuts down first and last), because what they
// measure is what xpe_ai_init does when called -- the first time, with a bad argument, with a bad config, a second time.
// The rollback after an allocation failure is NOT here: it cannot be provoked from outside, and M5's allocation
// sweep (xpe_ai_oom_tests) is the proof of it.

namespace {

struct InitResult {
    XpeErrorCode rc;
    std::vector<std::string> alerts;
    std::vector<int32_t> severities;
};

InitResult InitWith(const char* dir, const char* config) {
    xpe_clear_alerts();
    InitResult r;
    r.rc = xpe_ai_init(dir, config);
    r.alerts = Alerts(&r.severities);
    return r;
}

bool IsInitialised() {
    int32_t state = -1;
    return xpe_ai_worker_state(&state, nullptr, nullptr) != XPE_ERR_NOT_INITIALIZED;
}

struct Pristine {
    Pristine() { xpe_ai_shutdown(); xpe_clear_alerts(); }
    ~Pristine() { xpe_ai_shutdown(); xpe_clear_alerts(); }
};

const std::string kDir = kData + "/models_x2";

}  // namespace

TEST(AiInitHardening, AnEmptyModelDirectoryIsAMissingArgumentAndLeavesTheModuleUntouched) {
    const Pristine p;
    const InitResult r = InitWith("", nullptr);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, r.rc);
    EXPECT_FALSE(IsInitialised()) << "a refused init initialises nothing";
    EXPECT_TRUE(r.alerts.empty()) << "a plain INVALID_INPUT, like a NULL directory";
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ai_init(nullptr, nullptr)) << "the NULL control is unchanged";

    // the control that makes the refusal mean something: a real directory initialises
    EXPECT_EQ(XPE_OK, xpe_ai_init(kDir.c_str(), nullptr));
    EXPECT_TRUE(IsInitialised());
}

TEST(AiInitHardening, AnEmptyModelDirectoryIsRefusedWhetherOrNotTheModuleIsAlreadyInitialised) {
    // An argument is wrong whatever the module's state is: the check sits before the "already initialised, ignore"
    // short-circuit. Without that, init("") on a live module returned OK and looked like a success.
    const Pristine p;
    ASSERT_EQ(XPE_OK, xpe_ai_init(kDir.c_str(), nullptr));
    xpe_clear_alerts();
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ai_init("", nullptr));
    EXPECT_TRUE(IsInitialised()) << "the live session is not touched by the refused call";
    EXPECT_TRUE(Alerts().empty());
}

TEST(AiInitHardening, AConfigThatCannotBeUsedSaysSoAndTheCallStillSucceeds) {
    struct Case { const char* config; const char* mention; };
    const Case cases[] = {
        {"{bad", "not valid JSON"},
        {"", "not valid JSON"},               // an empty text is not JSON either
        {"[]", "not an object"},
        {"[1,2]", "not an object"},
        {"5", "not an object"},
        {"\"x\"", "not an object"},
        {"null", "not an object"},
        {"true", "not an object"},
    };
    for (const Case& c : cases) {
        const Pristine p;
        const InitResult r = InitWith(kDir.c_str(), c.config);
        EXPECT_EQ(XPE_OK, r.rc) << "config '" << c.config << "': the return code is the #145 line, unchanged";
        EXPECT_TRUE(IsInitialised()) << c.config;
        ASSERT_EQ(1u, r.alerts.size()) << "config '" << c.config << "': exactly one alert";
        EXPECT_EQ(XPE_ALERT_WARNING, r.severities[0]) << c.config;
        EXPECT_NE(std::string::npos, r.alerts[0].find(c.mention)) << "config '" << c.config << "': " << r.alerts[0];
        EXPECT_NE(std::string::npos, r.alerts[0].find("every setting uses its default")) << r.alerts[0];
    }
}

TEST(AiInitHardening, ATimeoutOutsideZeroToTwoToTheThirtyOneIsIgnoredWithAnAlert) {
    struct Case { const char* config; const char* value; };
    const Case cases[] = {
        {"{\"timeout_ms\": -1}", "-1"},
        {"{\"timeout_ms\": -2147483648}", "-2147483648"},
        {"{\"timeout_ms\": 2147483648}", "2147483648"},
        {"{\"timeout_ms\": 10000000000}", "10000000000"},
        {"{\"timeout_ms\": 18446744073709551615}", "18446744073709551615"},   // above int64: the unsigned branch
    };
    for (const Case& c : cases) {
        const Pristine p;
        const InitResult r = InitWith(kDir.c_str(), c.config);
        EXPECT_EQ(XPE_OK, r.rc) << c.config;
        ASSERT_EQ(1u, r.alerts.size()) << c.config << ": one alert -- not also an 'unexpected type'";
        EXPECT_EQ(XPE_ALERT_WARNING, r.severities[0]) << c.config;
        EXPECT_NE(std::string::npos, r.alerts[0].find("ai config key 'timeout_ms' is out of range (0 to 2147483647 ms)")) << r.alerts[0];
        EXPECT_NE(std::string::npos, r.alerts[0].find(std::string("(value: ") + c.value + ")")) << r.alerts[0];
    }
}

TEST(AiInitHardening, TheTimeoutBoundsThemselvesAreAcceptedSilently) {
    // The controls: the edges of the legal range, and a floating value that keeps its own, older alert.
    for (const char* config : {"{\"timeout_ms\": 0}", "{\"timeout_ms\": 1}", "{\"timeout_ms\": 5000}",
                               "{\"timeout_ms\": 2147483647}"}) {
        const Pristine p;
        const InitResult r = InitWith(kDir.c_str(), config);
        EXPECT_EQ(XPE_OK, r.rc) << config;
        EXPECT_TRUE(r.alerts.empty()) << config << ": " << (r.alerts.empty() ? "" : r.alerts[0]);
    }
    const Pristine p;
    const InitResult r = InitWith(kDir.c_str(), "{\"timeout_ms\": 1.5}");
    EXPECT_EQ(XPE_OK, r.rc);
    ASSERT_EQ(1u, r.alerts.size()) << "a non-integer is the #145 'unexpected type' alert, and only that one";
    EXPECT_NE(std::string::npos, r.alerts[0].find("has an unexpected type")) << r.alerts[0];
}

TEST(AiInitHardening, ASecondInitIsIgnoredAndSaysSoOnlyWhenItAskedForSomethingDifferent) {
    const Pristine p;
    ASSERT_EQ(XPE_OK, xpe_ai_init(kDir.c_str(), "{\"use_worker\": true}"));
    int32_t state = -1;
    ASSERT_EQ(XPE_OK, xpe_ai_worker_state(&state, nullptr, nullptr));
    ASSERT_EQ(XPE_AI_WORKER_ACTIVE, state);

    // identical: silent, still OK
    InitResult r = InitWith(kDir.c_str(), "{\"use_worker\": true}");
    EXPECT_EQ(XPE_OK, r.rc);
    EXPECT_TRUE(r.alerts.empty()) << "the same call again is not news";

    // a different config: OK, ignored, one Warning
    r = InitWith(kDir.c_str(), "{}");
    EXPECT_EQ(XPE_OK, r.rc) << "a client test holds 'second init is OK and ignored' as the contract";
    ASSERT_EQ(1u, r.alerts.size());
    EXPECT_EQ(XPE_ALERT_WARNING, r.severities[0]);
    EXPECT_NE(std::string::npos, r.alerts[0].find("different model directory or config")) << r.alerts[0];
    EXPECT_NE(std::string::npos, r.alerts[0].find("call xpe_ai_shutdown first")) << r.alerts[0];
    ASSERT_EQ(XPE_OK, xpe_ai_worker_state(&state, nullptr, nullptr));
    EXPECT_EQ(XPE_AI_WORKER_ACTIVE, state) << "ignored means the FIRST settings stay in effect";

    // a different directory: the same
    r = InitWith((kData + "/some_other_models").c_str(), "{\"use_worker\": true}");
    EXPECT_EQ(XPE_OK, r.rc);
    ASSERT_EQ(1u, r.alerts.size());
    EXPECT_NE(std::string::npos, r.alerts[0].find("different model directory or config")) << r.alerts[0];

    // NULL config and an empty config text are the same request (both mean "defaults"): shutdown, then check
    xpe_ai_shutdown();
    ASSERT_EQ(XPE_OK, xpe_ai_init(kDir.c_str(), nullptr));
    r = InitWith(kDir.c_str(), "");
    EXPECT_EQ(XPE_OK, r.rc);
    // "" is not valid JSON and a first init with it would warn, but as a SECOND init it is judged as 'the same'
    EXPECT_TRUE(r.alerts.empty()) << (r.alerts.empty() ? "" : r.alerts[0]);
}
