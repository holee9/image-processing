/**
 * @file test_thread_determinism.cpp
 * @brief #179 (QA-B-105): gsvg output does not depend on the thread count.
 *
 * Grid suppression and the virtual grid run their passes on row (or column)
 * bands. Each band computes its outputs from inputs no band writes and in the
 * same order as the single-threaded loop, so every thread count has to produce
 * bit-identical pixels.
 */

#include "xpe/gsvg/gsvg_api.h"
#include "grid_test_tools.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace gsvg_test;

namespace {

constexpr int kW = 384, kH = 320;   // not a multiple of the thread counts used

std::vector<uint16_t> ToU16v(const Image& img) {
    std::vector<uint16_t> o(img.px.size());
    for (size_t i = 0; i < o.size(); ++i)
        o[i] = static_cast<uint16_t>(std::clamp(std::round(img.px[i]), 0.0, 65535.0));
    return o;
}

std::vector<uint16_t> GriddedScene() {
    GridSpec g;   // 103 lpi, 0.14 mm, rows, d 0.05
    return ToU16v(ApplyGrid(MakeAnatomyBackground(kW, kH, 11u, 30.0), g));
}

std::vector<uint16_t> FlatScene() {
    std::vector<uint16_t> src(static_cast<size_t>(kW) * kH);
    for (size_t i = 0; i < src.size(); ++i)
        src[i] = static_cast<uint16_t>(8000 + (i * 2654435761u) % 20000u);
    return src;
}

std::vector<uint16_t> Process(const char* cfg, const std::vector<uint16_t>& src, int threads) {
    EXPECT_EQ(xpe_gsvg_set_max_threads(threads), XPE_OK);
    EXPECT_EQ(xpe_gsvg_get_max_threads(), threads > 0 ? threads : 0);
    std::vector<uint16_t> dst(src.size(), 0);
    void* h = nullptr;
    EXPECT_EQ(xpe_gsvg_init(&h, cfg), XPE_OK) << cfg;
    EXPECT_EQ(xpe_gsvg_process(h, src.data(), src.size(), dst.data(), dst.size(), kW, kH,
                               nullptr, 0), XPE_OK);
    xpe_gsvg_shutdown(h);
    return dst;
}

std::string VgConfig() {
    return "{\"virtual_grid\": true, \"vg_table_path\": \"data/vg_table_water_csi600_victre.csv\""
           ", \"vg_kvp\": 80, \"vg_grid_ratio\": 10, \"vg_grid_frequency_per_cm\": 40,"
           " \"vg_pixel_pitch_mm\": 0.14, \"vg_air_signal\": 60000, \"vg_iterations\": 3,"
           " \"vg_pyramid_levels\": 6, \"vg_pyramid_gain\": 1.3, \"vg_denoise_k\": 2}";
}

}  // namespace

TEST(GsvgThreads, SuppressionIsBitIdenticalForEveryThreadCount)
{
    const std::vector<uint16_t> src = GriddedScene();
    const std::vector<uint16_t> one = Process("{\"grid_suppression\": true}", src, 1);
    EXPECT_NE(std::memcmp(one.data(), src.data(), src.size() * 2), 0)
        << "control: suppression did change the image";
    for (int threads : {2, 3, 4, 8, 0 /* automatic */}) {
        const std::vector<uint16_t> got = Process("{\"grid_suppression\": true}", src, threads);
        EXPECT_EQ(std::memcmp(got.data(), one.data(), one.size() * 2), 0)
            << "suppression differs at threads = " << threads;
    }
    xpe_gsvg_set_max_threads(0);
}

TEST(GsvgThreads, VirtualGridIsBitIdenticalForEveryThreadCount)
{
    const std::vector<uint16_t> src = FlatScene();
    const std::string cfg = VgConfig();
    const std::vector<uint16_t> one = Process(cfg.c_str(), src, 1);
    EXPECT_NE(std::memcmp(one.data(), src.data(), src.size() * 2), 0)
        << "control: the virtual grid did change the image";
    for (int threads : {2, 3, 4, 8, 0 /* automatic */}) {
        const std::vector<uint16_t> got = Process(cfg.c_str(), src, threads);
        EXPECT_EQ(std::memcmp(got.data(), one.data(), one.size() * 2), 0)
            << "virtual grid differs at threads = " << threads;
    }
    xpe_gsvg_set_max_threads(0);
}

TEST(GsvgThreads, SetterStoresTheRequest)
{
    EXPECT_EQ(xpe_gsvg_set_max_threads(-3), XPE_OK);
    EXPECT_EQ(xpe_gsvg_get_max_threads(), 0) << "negative is stored as automatic";
    EXPECT_EQ(xpe_gsvg_set_max_threads(7), XPE_OK);
    EXPECT_EQ(xpe_gsvg_get_max_threads(), 7);
    EXPECT_EQ(xpe_gsvg_set_max_threads(0), XPE_OK);
    EXPECT_EQ(xpe_gsvg_get_max_threads(), 0);
}

// QA-B-106: the masked entry points take the same passes, so they must be
// thread-count independent too -- and the result struct must report the same
// thing whatever the thread count.
TEST(GsvgThreads, MaskedPathsAreBitIdenticalForEveryThreadCount)
{
    const std::vector<uint16_t> src = FlatScene();
    std::vector<uint8_t> mask(src.size(), 0);
    for (int y = 40; y < kH - 30; ++y)
        for (int x = 25; x < kW - 35; ++x)
            mask[static_cast<size_t>(y) * kW + x] = 1;
    const std::string cfg = VgConfig();

    auto runMasked = [&](int threads, bool useEx, XpeGsvgResult* res) {
        EXPECT_EQ(xpe_gsvg_set_max_threads(threads), XPE_OK);
        std::vector<uint16_t> dst(src.size(), 0);
        void* h = nullptr;
        EXPECT_EQ(xpe_gsvg_init(&h, cfg.c_str()), XPE_OK);
        if (useEx) {
            res->structSize = sizeof(XpeGsvgResult);
            EXPECT_EQ(xpe_gsvg_process_ex(h, src.data(), src.size(), dst.data(), dst.size(),
                                          kW, kH, nullptr, 0, mask.data(), mask.size(), res), XPE_OK);
        } else {
            EXPECT_EQ(xpe_gsvg_process_masked(h, src.data(), src.size(), dst.data(), dst.size(),
                                              kW, kH, nullptr, 0, mask.data(), mask.size()), XPE_OK);
        }
        xpe_gsvg_shutdown(h);
        return dst;
    };

    XpeGsvgResult res1{};
    const std::vector<uint16_t> one = runMasked(1, false, &res1);
    EXPECT_NE(std::memcmp(one.data(), src.data(), src.size() * 2), 0) << "control: the mask run changed the image";
    for (int threads : {2, 3, 4, 8, 0 /* automatic */}) {
        XpeGsvgResult res{};
        EXPECT_EQ(std::memcmp(runMasked(threads, false, &res).data(), one.data(), one.size() * 2), 0)
            << "process_masked differs at threads = " << threads;
        const std::vector<uint16_t> viaEx = runMasked(threads, true, &res);
        EXPECT_EQ(std::memcmp(viaEx.data(), one.data(), one.size() * 2), 0)
            << "process_ex differs at threads = " << threads;
        EXPECT_EQ(res.virtualGridApplied, 1) << threads;
        EXPECT_EQ(res.reason, XPE_GSVG_REASON_APPLIED) << threads;
    }
    // pixels outside the mask keep the input (QA-B-96), whatever the thread count
    for (size_t i = 0; i < mask.size(); ++i)
        if (!mask[i]) ASSERT_EQ(one[i], src[i]) << i;
    xpe_gsvg_set_max_threads(0);
}

// QA-B-106: a table and a ratio the product file does not use.
TEST(GsvgThreads, SyntheticTableAndOtherRatioAreAlsoIdentical)
{
    const std::vector<uint16_t> src = FlatScene();
    const std::string cfg =
        "{\"virtual_grid\": true, \"vg_table_path\": \"tests/data/virtual_grid_synthetic_table.csv\""
        ", \"vg_kvp\": 80, \"vg_grid_ratio\": 6, \"vg_pixel_pitch_mm\": 1.0,"
        " \"vg_air_signal\": 60000, \"vg_iterations\": 5}";
    const std::vector<uint16_t> one = Process(cfg.c_str(), src, 1);
    EXPECT_NE(std::memcmp(one.data(), src.data(), src.size() * 2), 0) << "control";
    for (int threads : {2, 4, 0}) {
        EXPECT_EQ(std::memcmp(Process(cfg.c_str(), src, threads).data(), one.data(), one.size() * 2), 0)
            << "synthetic table, ratio 6, threads = " << threads;
    }
    xpe_gsvg_set_max_threads(0);
}
