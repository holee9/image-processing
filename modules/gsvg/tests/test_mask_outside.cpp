// #189 (QA-B-108): the MaskOutside switch. The production default is Zero —
// what shipped — and the alternatives exist so the choice can be measured.
// The invariant pinned here: the choice reaches the image ONLY through the
// post-steps, so with pyramidLevels = 0 all four variants are bit-identical.
#include <gtest/gtest.h>

#include "virtual_grid.h"

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

TEST(GsvgMaskOutside, DefaultIsZero)
{
    const vg::VgSwitches sw;
    EXPECT_EQ(sw.maskOutside, vg::MaskOutside::Zero);
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
