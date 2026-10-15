/**
 * @file test_nonfinite_pixels.cpp
 * @brief The display LUT functions neither accept a non-finite pixel as success nor make one (QA-B-181f, #233).
 *
 * QA-B-181e measured that xpe_apply_modality_lut (TABLE), xpe_apply_presentation_lut and the LINEAR modality chain
 * answered rc=0 for NaN and infinite pixels, clamping them to a LUT end or to 0 -- an upstream fault turned into a
 * normal-looking image. The rule (leader decision): a non-finite input is refused with XPE_ERR_INVALID_INPUT and the
 * image is left exactly as it was; a request whose result would not be finite is refused the same way.
 *
 * The cases use +inf, -inf and NaN: +inf passes every range comparison in every floating-point mode, and whether a
 * `<= 0` test refuses NaN depends on /fp:fast against /fp:precise, so these inputs cannot be stopped by accident.
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

#include "xpe/display/display_api.h"

namespace {

constexpr float kInf = std::numeric_limits<float>::infinity();
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
const float kBad[] = {kNaN, kInf, -kInf};
const char* const kBadName[] = {"NaN", "+inf", "-inf"};

constexpr int kW = 16, kH = 16;

// A malloc'd float32 image (xpe_apply_presentation_lut frees and replaces the buffer on success).
struct Img {
    XpeImageBuffer b{};
    std::vector<float> before;
    explicit Img(float fill) {
        b.width = kW; b.height = kH; b.format = XPE_PIXEL_FLOAT32; b.bitsAllocated = 32; b.bitsStored = 32;
        b.dataSize = static_cast<size_t>(kW) * kH * sizeof(float);
        b.data = std::malloc(b.dataSize);
        float* p = static_cast<float*>(b.data);
        for (int i = 0; i < kW * kH; ++i) p[i] = fill + static_cast<float>(i % 7);
    }
    void Set(int i, float v) { static_cast<float*>(b.data)[i] = v; }
    void Remember() {
        const float* p = static_cast<const float*>(b.data);
        before.assign(p, p + static_cast<size_t>(kW) * kH);
    }
    bool Untouched() const {
        return b.format == XPE_PIXEL_FLOAT32 && b.dataSize == before.size() * sizeof(float) &&
               std::memcmp(b.data, before.data(), before.size() * sizeof(float)) == 0;
    }
    ~Img() { std::free(b.data); }
    Img(const Img&) = delete;
    Img& operator=(const Img&) = delete;
};

std::vector<uint16_t> Lut() {
    std::vector<uint16_t> l(1000);
    for (size_t i = 0; i < l.size(); ++i) l[i] = static_cast<uint16_t>(i);
    return l;
}

XpeModalityLutParams Table(const std::vector<uint16_t>& lut) {
    XpeModalityLutParams m{};
    m.mode = XPE_MODALITY_LUT_TABLE;
    m.lutData = lut.data();
    m.lutLength = static_cast<uint32_t>(lut.size());
    m.lutFirstMapped = 0;
    m.lutBitsStored = 16;
    return m;
}

XpeModalityLutParams Linear(float slope, float intercept) {
    XpeModalityLutParams m{};
    m.mode = XPE_MODALITY_LUT_LINEAR;
    m.rescaleSlope = slope;
    m.rescaleIntercept = intercept;
    return m;
}

XpePresentationLutParams PresLut() {
    XpePresentationLutParams p{};
    for (int i = 0; i < 1024; ++i) p.lutData[i] = static_cast<uint16_t>(i * 64);
    return p;
}

}  // namespace

// ---- xpe_apply_modality_lut ------------------------------------------------------------------------------------

TEST(DisplayNonFinite, ModalityTableRefusesANonFinitePixelAndLeavesTheImageAlone) {
    const auto lut = Lut();
    const auto m = Table(lut);
    for (int i = 0; i < 3; ++i) {
        Img img(100.0f);
        img.Set(5, kBad[i]);
        img.Remember();
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_apply_modality_lut(&img.b, &m)) << "pixel = " << kBadName[i];
        EXPECT_TRUE(img.Untouched()) << "pixel = " << kBadName[i];
    }
}

TEST(DisplayNonFinite, ModalityLinearRefusesANonFinitePixelAndLeavesTheImageAlone) {
    const auto m = Linear(2.0f, 1.0f);
    for (int i = 0; i < 3; ++i) {
        Img img(100.0f);
        img.Set(5, kBad[i]);
        img.Remember();
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_apply_modality_lut(&img.b, &m)) << "pixel = " << kBadName[i];
        EXPECT_TRUE(img.Untouched()) << "pixel = " << kBadName[i];
    }
}

TEST(DisplayNonFinite, ModalityLinearRefusesANonFiniteSlopeOrIntercept) {
    // `slope == 0` is the only test the slope had: NaN and infinity passed it and filled the image with NaN / +inf.
    struct Case { float slope, intercept; const char* what; };
    const Case cases[] = {{kNaN, 0.0f, "slope NaN"}, {kInf, 0.0f, "slope +inf"}, {-kInf, 0.0f, "slope -inf"},
                          {1.0f, kNaN, "intercept NaN"}, {1.0f, kInf, "intercept +inf"}, {1.0f, -kInf, "intercept -inf"}};
    for (const Case& c : cases) {
        Img img(100.0f);
        img.Remember();
        const auto m = Linear(c.slope, c.intercept);
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_apply_modality_lut(&img.b, &m)) << c.what;
        EXPECT_TRUE(img.Untouched()) << c.what;
    }
}

TEST(DisplayNonFinite, ModalityLinearRefusesAFiniteRequestWhoseResultWouldNotBeFinite) {
    // 100 * 1e38 leaves float. The 181e chain "modality LINEAR -> presentation" started from exactly this.
    for (float slope : {1.0e38f, -1.0e38f}) {
        Img img(100.0f);
        img.Remember();
        const auto m = Linear(slope, 0.0f);
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_apply_modality_lut(&img.b, &m)) << "slope = " << slope;
        EXPECT_TRUE(img.Untouched()) << "slope = " << slope;
    }
    Img img2(0.0f);
    img2.Remember();
    const auto m2 = Linear(1.0f, 3.0e38f);
    EXPECT_EQ(XPE_OK, xpe_apply_modality_lut(&img2.b, &m2)) << "an intercept that keeps every result finite is fine";
}

TEST(DisplayNonFinite, ModalityStillConvertsOrdinaryImages) {
    {
        Img img(10.0f);
        const auto m = Linear(2.0f, 1.0f);
        ASSERT_EQ(XPE_OK, xpe_apply_modality_lut(&img.b, &m));
        EXPECT_EQ(21.0f, static_cast<const float*>(img.b.data)[0]);
    }
    {
        const auto lut = Lut();
        Img img(10.0f);
        const auto m = Table(lut);
        ASSERT_EQ(XPE_OK, xpe_apply_modality_lut(&img.b, &m));
        EXPECT_EQ(10.0f, static_cast<const float*>(img.b.data)[0]);
    }
}

// ---- xpe_apply_presentation_lut --------------------------------------------------------------------------------

TEST(DisplayNonFinite, PresentationLutRefusesANonFinitePixelAndLeavesTheImageAlone) {
    const auto p = PresLut();
    for (int i = 0; i < 3; ++i) {
        Img img(0.25f);
        img.Set(5, kBad[i]);
        img.Remember();
        void* const dataBefore = img.b.data;
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_apply_presentation_lut(&img.b, &p)) << "pixel = " << kBadName[i];
        EXPECT_EQ(dataBefore, img.b.data) << "the buffer must not have been replaced";
        EXPECT_TRUE(img.Untouched()) << "pixel = " << kBadName[i] << " (still float32, same bytes)";
    }
}

TEST(DisplayNonFinite, PresentationLutStillConvertsALargeFiniteImage) {
    const auto p = PresLut();
    Img img(3.0e38f);
    ASSERT_EQ(XPE_OK, xpe_apply_presentation_lut(&img.b, &p)) << "huge but finite pixels saturate; they are data";
    EXPECT_EQ(XPE_PIXEL_UINT16, img.b.format);
}

TEST(DisplayNonFinite, TheModalityToPresentationChainStopsAtTheFirstStage) {
    const auto pres = PresLut();
    Img img(0.25f);
    img.Remember();
    const auto m = Linear(kInf, 0.0f);
    EXPECT_NE(XPE_OK, xpe_apply_modality_lut(&img.b, &m));
    EXPECT_TRUE(img.Untouched()) << "the first stage must not hand a non-finite image to the next one";
    EXPECT_EQ(XPE_OK, xpe_apply_presentation_lut(&img.b, &pres)) << "and the untouched image is still convertible";
}

// ---- xpe_apply_voi_lut -----------------------------------------------------------------------------------------

namespace {
XpeVoiLutParams Voi(XpeVoiLutMode mode, float center, float width, float minOut, float maxOut) {
    XpeVoiLutParams v{};
    v.mode = mode;
    v.center = center; v.width = width; v.minOut = minOut; v.maxOut = maxOut;
    return v;
}
const XpeVoiLutMode kModes[] = {XPE_VOI_LINEAR, XPE_VOI_LINEAR_EXACT, XPE_VOI_SIGMOID};
}  // namespace

TEST(DisplayNonFinite, VoiLutRefusesANonFiniteWindowParameter) {
    // `width <= 0` was the only test: it passes +inf in every mode and NaN under /fp:precise; center, minOut and
    // maxOut had none.
    for (XpeVoiLutMode mode : kModes) {
        const XpeVoiLutParams cases[] = {
            Voi(mode, kNaN, 100.0f, 0.0f, 255.0f), Voi(mode, kInf, 100.0f, 0.0f, 255.0f),
            Voi(mode, 50.0f, kNaN, 0.0f, 255.0f),  Voi(mode, 50.0f, kInf, 0.0f, 255.0f),
            Voi(mode, 50.0f, 100.0f, kNaN, 255.0f), Voi(mode, 50.0f, 100.0f, -kInf, 255.0f),
            Voi(mode, 50.0f, 100.0f, 0.0f, kNaN),   Voi(mode, 50.0f, 100.0f, 0.0f, kInf),
            Voi(mode, 50.0f, 100.0f, -3.0e38f, 3.0e38f),   // finite ends, but the range overflows
        };
        for (const XpeVoiLutParams& v : cases) {
            Img img(40.0f);
            img.Remember();
            EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_apply_voi_lut(&img.b, &v))
                << "mode " << mode << " center " << v.center << " width " << v.width << " out " << v.minOut << ".." << v.maxOut;
            EXPECT_TRUE(img.Untouched());
        }
    }
}

TEST(DisplayNonFinite, VoiLutRefusesANonFinitePixelAndLeavesTheImageAlone) {
    for (XpeVoiLutMode mode : kModes) {
        const XpeVoiLutParams v = Voi(mode, 50.0f, 100.0f, 0.0f, 255.0f);
        for (int i = 0; i < 3; ++i) {
            Img img(40.0f);
            img.Set(5, kBad[i]);
            img.Remember();
            EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_apply_voi_lut(&img.b, &v)) << "mode " << mode << " pixel " << kBadName[i];
            EXPECT_TRUE(img.Untouched()) << "mode " << mode << " pixel " << kBadName[i];
        }
    }
}

TEST(DisplayNonFinite, VoiLutStillWindowsAnOrdinaryImage) {
    for (XpeVoiLutMode mode : kModes) {
        const XpeVoiLutParams v = Voi(mode, 50.0f, 100.0f, 0.0f, 255.0f);
        Img img(40.0f);
        ASSERT_EQ(XPE_OK, xpe_apply_voi_lut(&img.b, &v)) << "mode " << mode;
    }
}

// ---- the producer side: finite input and finite parameters must give a finite image --------------------------------

TEST(DisplayNonFinite, VoiLutNeverMakesANonFinitePixelFromFiniteInput) {
    // A tiny width turns (x - center) / width into +-inf; times a zero output range that is inf * 0 = NaN, and
    // clamp() passes NaN through. Every combination below is finite input with finite, valid parameters.
    const float widths[] = {1.0f, 1.0e-30f, 1.0e-38f};
    const float ranges[][2] = {{0.0f, 255.0f}, {5.0f, 5.0f}, {-3.0e38f, 0.0f}};
    const float pixels[] = {1.0e10f, -1.0e10f, 0.0f, 50.0f};
    for (XpeVoiLutMode mode : kModes) {
        for (float w : widths) {
            for (const auto& r : ranges) {
                for (float pxv : pixels) {
                    Img img(0.0f);
                    for (int i = 0; i < kW * kH; ++i) img.Set(i, pxv);
                    const XpeVoiLutParams v = Voi(mode, 0.0f, w, r[0], r[1]);
                    const XpeErrorCode rc = xpe_apply_voi_lut(&img.b, &v);
                    if (rc != XPE_OK) continue;   // a refusal is an acceptable answer
                    for (int i = 0; i < kW * kH; ++i) {
                        const float o = static_cast<const float*>(img.b.data)[i];
                        ASSERT_TRUE(o >= r[0] && o <= r[1])
                            << "mode " << mode << " width " << w << " out " << r[0] << ".." << r[1] << " pixel " << pxv << " -> " << o;
                    }
                }
            }
        }
    }
}
