// #189 (QA-B-108, QA-B-110): the MaskOutside switch. The production default is
// edge replicate (QA-B-110 lead decision); the alternatives stay as settings.
// The invariant pinned here: the choice reaches the image ONLY through the
// post-steps, so with pyramidLevels = 0 all four variants are bit-identical.
#include <gtest/gtest.h>

#include "virtual_grid.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace vg = xpe_gsvg_detail;

namespace {

vg::VgSettings BaseSettings() {
    vg::VgSettings st;
    st.kvp = 80; st.gridRatio = 10; st.gridFreqPerCm = 40; st.pixelPitchMm = 0.139;
    st.airSignal = 60000; st.iterations = 3;
    st.pyramidLevels = 0; st.pyramidGain = 1.0; st.denoiseK = 0.0;
    return st;
}

// A scene with a circular field stop, so the mask has a real boundary.
void MakeScene(int k, std::vector<double>& img, std::vector<uint8_t>& mask) {
    img.assign(static_cast<size_t>(k) * k, 0.0);
    mask.assign(img.size(), 0);
    const double cx = k / 2.0, cy = k / 2.0, r = k * 0.4;
    for (int y = 0; y < k; ++y)
        for (int x = 0; x < k; ++x) {
            const size_t i = static_cast<size_t>(y) * k + x;
            const double dx = x - cx, dy = y - cy;
            const bool in = dx * dx + dy * dy <= r * r;
            mask[i] = in ? 1 : 0;
            img[i] = in ? 9000.0 + 3000.0 * ((x / 16 + y / 16) % 2) : 300.0;
        }
}

}  // namespace

TEST(GsvgMaskOutside, DefaultIsEdgeReplicate)
{
    const vg::VgSwitches sw;
    EXPECT_EQ(sw.maskOutside, vg::MaskOutside::Replicate);
}

// #189 (QA-B-110): the artificial step the choice leaves at the field boundary.
// Measured as the ring just inside the mask against the deep interior, which
// the post-steps do not reach. On this scene the post-steps themselves are
// worth 0.3210 (pyramidLevels = 0, every variant); with six levels the choices
// measured zero 0.5670, keep 0.5335, rect-only 0.3910, replicate 0.3598
// (QA-B-108). The floor 0.45 sits between replicate and the two that pile a
// visible step on the boundary, so reverting the default to Zero fails here.
TEST(GsvgMaskOutside, BoundaryStepStaysNearThePostStepBaseline)
{
    constexpr int k = 1024;
    vg::ParamTable table;
    ASSERT_EQ(vg::LoadParamTable("data/vg_table_water_csi600_victre.csv", table), "");

    // Square field so "distance to the boundary" is exact.
    constexpr int kInset = 200;
    std::vector<double> src(static_cast<size_t>(k) * k);
    std::vector<uint8_t> mask(src.size(), 0);
    for (int y = 0; y < k; ++y)
        for (int x = 0; x < k; ++x) {
            const size_t i = static_cast<size_t>(y) * k + x;
            const bool in = x >= kInset && x < k - kInset && y >= kInset && y < k - kInset;
            mask[i] = in ? 1 : 0;
            src[i] = in ? 9000.0 + 3000.0 * ((x / 16 + y / 16) % 2) : 300.0;
        }

    vg::VgSettings st = BaseSettings();
    st.pyramidLevels = 6; st.pyramidGain = 1.3; st.denoiseK = 2.0;

    std::vector<double> img = src;
    const vg::VgReport rep = vg::RunVirtualGrid(img, k, k, table, st, vg::VgSwitches{}, mask.data());
    ASSERT_EQ(rep.error, "");

    // Mean over the ring one pixel inside the boundary, and over the deep
    // interior (>= 200 px in, beyond the six-level pyramid's reach).
    double ring = 0, deep = 0;
    size_t nRing = 0, nDeep = 0;
    for (int y = kInset; y < k - kInset; ++y)
        for (int x = kInset; x < k - kInset; ++x) {
            const size_t i = static_cast<size_t>(y) * k + x;
            const int d = std::min(std::min(x - kInset, k - kInset - 1 - x),
                                   std::min(y - kInset, k - kInset - 1 - y));
            if (d == 1) { ring += img[i]; ++nRing; }
            else if (d >= 200) { deep += img[i]; ++nDeep; }
        }
    ASSERT_GT(nRing, 0u);
    ASSERT_GT(nDeep, 0u);
    ring /= static_cast<double>(nRing);
    deep /= static_cast<double>(nDeep);
    const double step = (ring - deep) / deep;
    std::printf("MASKOUTSIDE boundary ring1=%.1f deep=%.1f step=%.4f\n", ring, deep, step);

    EXPECT_LT(step, 0.45) << "artificial step at the field boundary";
}

TEST(GsvgMaskOutside, WithoutPostStepsEveryVariantIsBitIdentical)
{
    constexpr int k = 256;
    vg::ParamTable table;
    ASSERT_EQ(vg::LoadParamTable("data/vg_table_water_csi600_victre.csv", table), "");

    std::vector<double> src;
    std::vector<uint8_t> mask;
    MakeScene(k, src, mask);

    const vg::MaskOutside variants[] = {
        vg::MaskOutside::Zero, vg::MaskOutside::Replicate,
        vg::MaskOutside::Keep, vg::MaskOutside::RectOnly,
    };
    std::vector<double> reference;
    for (const vg::MaskOutside v : variants) {
        vg::VgSwitches sw;
        sw.maskOutside = v;
        std::vector<double> img = src;
        const vg::VgReport rep = vg::RunVirtualGrid(img, k, k, table, BaseSettings(), sw, mask.data());
        ASSERT_EQ(rep.error, "");
        if (reference.empty()) {
            reference = img;
        } else {
            EXPECT_EQ(std::memcmp(reference.data(), img.data(), img.size() * sizeof(double)), 0)
                << "the mask-outside choice must not reach the image without the post-steps";
        }
    }
    // Excluded pixels keep their input value on every variant.
    for (size_t i = 0; i < src.size(); ++i)
        if (!mask[i]) ASSERT_EQ(reference[i], src[i]);
}
