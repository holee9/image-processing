/**
 * @file test_gsvg_result.cpp
 * @brief #180 (QA-B-101): xpe_gsvg_process_ex reports what a call did.
 *
 * The older entry points return XPE_OK and leave the image unchanged when no
 * grid is found, so a caller cannot tell "suppressed" from "nothing to do".
 * These tests pin the result struct's layout and each reason code to an image
 * that produces it.
 */

#include "xpe/gsvg/gsvg_api.h"
#include "grid_test_tools.h"

#include "gtest/gtest.h"

#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

using namespace gsvg_test;

namespace {

constexpr int kN = 512;
constexpr const char* kTablePath = "tests/data/virtual_grid_synthetic_table.csv";

std::vector<uint16_t> ToU16(const Image& img) {
    std::vector<uint16_t> out(img.px.size());
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<uint16_t>(std::clamp(std::round(img.px[i]), 0.0, 65535.0));
    return out;
}

std::string VgConfig(double kvp, const std::string& extra = "") {
    char kv[32];
    std::snprintf(kv, sizeof(kv), "%g", kvp);
    return std::string("{\"virtual_grid\": true, \"vg_table_path\": \"") + kTablePath +
           "\", \"vg_kvp\": " + kv + ", \"vg_grid_ratio\": 10, \"vg_pixel_pitch_mm\": 1.0,"
           " \"vg_air_signal\": 60000, \"vg_iterations\": 3" + extra + "}";
}

struct Outcome {
    XpeErrorCode code = XPE_OK;
    XpeGsvgResult res{};
    std::vector<uint16_t> dst;
};

Outcome Process(const char* cfg, const std::vector<uint16_t>& src, int w, int h,
            const std::vector<float>* gain = nullptr, bool inPlace = false) {
    Outcome r;
    void* handle = nullptr;
    EXPECT_EQ(xpe_gsvg_init(&handle, cfg), XPE_OK) << (cfg ? cfg : "(null)");
    r.dst = src;
    std::vector<uint16_t> out(src.size(), 0);
    uint16_t* dst = inPlace ? r.dst.data() : out.data();
    r.res.structSize = sizeof(XpeGsvgResult);
    r.res.reason = -1;
    r.code = xpe_gsvg_process_ex(handle, r.dst.data(), r.dst.size(), dst, src.size(), w, h,
                                 gain ? gain->data() : nullptr, gain ? gain->size() : 0,
                                 nullptr, 0, &r.res);
    if (!inPlace) r.dst = out;
    xpe_gsvg_shutdown(handle);
    return r;
}

std::vector<uint16_t> GridFree(int n = kN) { return ToU16(MakeAnatomyBackground(n, n, 101u, 30.0)); }

std::vector<uint16_t> WithGrid(int n = kN) {
    GridSpec g;   // 103 lpi, 0.14 mm, rows, d 0.05
    return ToU16(ApplyGrid(MakeAnatomyBackground(n, n, 101u, 30.0), g));
}

}  // namespace

TEST(GsvgResult, LayoutIsFixed)
{
    EXPECT_EQ(sizeof(XpeGsvgResult), 24u);
    EXPECT_EQ(offsetof(XpeGsvgResult, structSize), 0u);
    EXPECT_EQ(offsetof(XpeGsvgResult, vignetteApplied), 4u);
    EXPECT_EQ(offsetof(XpeGsvgResult, gridSuppressed), 8u);
    EXPECT_EQ(offsetof(XpeGsvgResult, virtualGridApplied), 12u);
    EXPECT_EQ(offsetof(XpeGsvgResult, restoredOriginal), 16u);
    EXPECT_EQ(offsetof(XpeGsvgResult, reason), 20u);
    EXPECT_EQ(XPE_GSVG_REASON_APPLIED, 0);
    EXPECT_EQ(XPE_GSVG_REASON_NOT_CONFIGURED, 1);
    EXPECT_EQ(XPE_GSVG_REASON_IMAGE_TOO_SMALL, 2);
    EXPECT_EQ(XPE_GSVG_REASON_NO_GRID_DETECTED, 3);
    EXPECT_EQ(XPE_GSVG_REASON_GRID_NOT_IN_SUBBANDS, 4);
    EXPECT_EQ(XPE_GSVG_REASON_VG_REFUSED, 5);
}

TEST(GsvgResult, GridFreeImageIsNotSuppressed)
{
    const auto src = GridFree();
    const Outcome r = Process("{\"grid_suppression\": true}", src, kN, kN);
    EXPECT_EQ(r.code, XPE_OK);
    EXPECT_EQ(r.res.gridSuppressed, 0);
    EXPECT_EQ(r.res.reason, XPE_GSVG_REASON_NO_GRID_DETECTED);
    EXPECT_EQ(r.res.restoredOriginal, 0);
    EXPECT_EQ(r.res.structSize, sizeof(XpeGsvgResult));
    EXPECT_EQ(r.dst, src);
}

TEST(GsvgResult, GriddedImageIsSuppressed)
{
    const auto src = WithGrid();
    const Outcome r = Process("{\"grid_suppression\": true}", src, kN, kN);
    EXPECT_EQ(r.code, XPE_OK);
    EXPECT_EQ(r.res.gridSuppressed, 1);
    EXPECT_EQ(r.res.reason, XPE_GSVG_REASON_APPLIED);
    EXPECT_EQ(r.res.virtualGridApplied, 0);
    EXPECT_NE(r.dst, src);
}

TEST(GsvgResult, SmallImageIsReportedAsTooSmall)
{
    const auto src = WithGrid(16);
    const Outcome r = Process("{\"grid_suppression\": true}", src, 16, 16);
    EXPECT_EQ(r.code, XPE_OK);
    EXPECT_EQ(r.res.gridSuppressed, 0);
    EXPECT_EQ(r.res.reason, XPE_GSVG_REASON_IMAGE_TOO_SMALL);
    EXPECT_EQ(r.dst, src);
}

TEST(GsvgResult, NothingConfiguredAndVignetteOnly)
{
    const auto src = GridFree(64);
    Outcome r = Process(nullptr, src, 64, 64);
    EXPECT_EQ(r.code, XPE_OK);
    EXPECT_EQ(r.res.reason, XPE_GSVG_REASON_NOT_CONFIGURED);
    EXPECT_EQ(r.res.vignetteApplied + r.res.gridSuppressed + r.res.virtualGridApplied + r.res.restoredOriginal, 0);

    const std::vector<float> gain(src.size(), 1.5f);
    r = Process("{\"vignette_correction\": true}", src, 64, 64, &gain);
    EXPECT_EQ(r.res.vignetteApplied, 1);
    EXPECT_EQ(r.res.reason, XPE_GSVG_REASON_NOT_CONFIGURED);
    EXPECT_NE(r.dst, src);
}

TEST(GsvgResult, VirtualGridAppliedAndRefused)
{
    const std::vector<uint16_t> src(static_cast<size_t>(kN) * kN, 20000);
    Outcome r = Process(VgConfig(80).c_str(), src, kN, kN);
    EXPECT_EQ(r.code, XPE_OK);
    EXPECT_EQ(r.res.virtualGridApplied, 1);
    EXPECT_EQ(r.res.reason, XPE_GSVG_REASON_APPLIED);
    EXPECT_NE(r.dst, src);

    // kVp outside the table: refused, original pixels back, reason given.
    r = Process(VgConfig(140).c_str(), src, kN, kN);
    EXPECT_EQ(r.code, XPE_ERR_CONFIG_INVALID);
    EXPECT_EQ(r.res.virtualGridApplied, 0);
    EXPECT_EQ(r.res.restoredOriginal, 1);
    EXPECT_EQ(r.res.reason, XPE_GSVG_REASON_VG_REFUSED);
    EXPECT_EQ(r.dst, src);

    // Same, in place with the vignette step on: the restore undoes the gain too.
    const std::vector<float> gain(src.size(), 1.5f);
    r = Process(VgConfig(140, ", \"vignette_correction\": true").c_str(), src, kN, kN, &gain, true);
    EXPECT_EQ(r.code, XPE_ERR_CONFIG_INVALID);
    EXPECT_EQ(r.res.vignetteApplied, 0);
    EXPECT_EQ(r.res.restoredOriginal, 1);
    EXPECT_EQ(r.dst, src);
}

TEST(GsvgResult, ResultIsRequiredAndSized)
{
    const auto src = GridFree(64);
    std::vector<uint16_t> dst(src.size(), 7);
    void* handle = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&handle, "{\"grid_suppression\": true}"), XPE_OK);
    EXPECT_EQ(xpe_gsvg_process_ex(handle, src.data(), src.size(), dst.data(), dst.size(), 64, 64,
                                  nullptr, 0, nullptr, 0, nullptr), XPE_ERR_INVALID_INPUT);
    XpeGsvgResult res{};
    res.structSize = sizeof(XpeGsvgResult) - 1;
    EXPECT_EQ(xpe_gsvg_process_ex(handle, src.data(), src.size(), dst.data(), dst.size(), 64, 64,
                                  nullptr, 0, nullptr, 0, &res), XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(dst, std::vector<uint16_t>(src.size(), 7));   // nothing processed
    res.structSize = sizeof(XpeGsvgResult);   // control
    EXPECT_EQ(xpe_gsvg_process_ex(handle, src.data(), src.size(), dst.data(), dst.size(), 64, 64,
                                  nullptr, 0, nullptr, 0, &res), XPE_OK);
    EXPECT_EQ(dst, src);
    xpe_gsvg_shutdown(handle);
}
