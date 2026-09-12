// #155 (QA-B-58): does each parameter reach the output? -- enhance_advanced.
//
// Same question as the enhance_basic and display sweeps: vary one input, hold
// the rest, require the answer to move by more than a rounding step. This is the
// assertion shape that #154 and #155 would both have failed, and that every
// characterisation test in the tree passed.
//
// enhance_advanced is where #154 partly lives, so two of its inputs are already
// known quantities: the acquisition parameters feed a gain model the source
// calls "simplified" (exposure_index.cpp:165), and that model multiplies the
// answer directly. Whether it actually arrives is measured here rather than read.

#include <gtest/gtest.h>

#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

constexpr uint32_t kW = 64, kH = 64;
constexpr size_t   kN = static_cast<size_t>(kW) * kH;
constexpr double   kMeaningful = 1e-4;

std::vector<float> Structured() {
    std::vector<float> px(kN);
    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            const float ramp  = 100.0f + 800.0f * (static_cast<float>(x) / (kW - 1));
            const float edge  = (x > kW / 2) ? 400.0f : 0.0f;
            const float noise = ((x * 7919u + y * 104729u) % 97u) * 1.5f;
            px[y * kW + x] = ramp + edge + noise;
        }
    }
    return px;
}

XpeImageBuffer Wrap(std::vector<float>& px) {
    XpeImageBuffer img{};
    img.width         = kW;
    img.height        = kH;
    img.format        = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32;
    img.bitsStored    = 32;
    img.data          = px.data();
    img.dataSize      = static_cast<uint32_t>(px.size() * sizeof(float));
    return img;
}

double MaxDiff(const std::vector<float>& a, const std::vector<float>& b) {
    double m = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        m = std::max(m, static_cast<double>(std::fabs(a[i] - b[i])));
    }
    return m;
}

class AdvParamDependency : public ::testing::Test {
protected:
    void SetUp() override    { ASSERT_EQ(XPE_OK, xpe_enhance_advanced_init(nullptr)); }
    void TearDown() override { xpe_enhance_advanced_shutdown(); }
};

}  // namespace

TEST_F(AdvParamDependency, FractionalProcess_OrderReachesTheOutput) {
    auto run = [](float order) {
        std::vector<float> px = Structured();
        XpeImageBuffer img = Wrap(px);
        EXPECT_EQ(XPE_OK, xpe_fractional_process(&img, order, nullptr));
        return px;
    };
    const double d = MaxDiff(run(0.3f), run(0.9f));
    GTEST_LOG_(INFO) << "fractional order 0.3->0.9 maxdiff=" << d;
    EXPECT_GT(d, kMeaningful) << "the fractional order does not reach the output";
}

TEST_F(AdvParamDependency, MultiscaleProcess_ConfigGainsReachTheOutput) {
    auto run = [](const char* cfg) {
        std::vector<float> px = Structured();
        XpeImageBuffer img = Wrap(px);
        XpeImageMetadata meta{};
        EXPECT_EQ(XPE_OK, xpe_multiscale_process(&img, &meta, cfg));
        return px;
    };

    const std::vector<float> base = run(nullptr);
    const std::vector<float> edge =
        run("{\"edge_gain\": 3.0, \"texture_gain\": 1.0, \"flat_gain\": 1.0}");
    const std::vector<float> flat =
        run("{\"edge_gain\": 1.0, \"texture_gain\": 1.0, \"flat_gain\": 0.2}");
    const std::vector<float> levels = run("{\"levels\": 2}");

    GTEST_LOG_(INFO) << "multiscale edge_gain maxdiff=" << MaxDiff(base, edge)
                     << " flat_gain=" << MaxDiff(base, flat)
                     << " levels=" << MaxDiff(base, levels);

    EXPECT_GT(MaxDiff(base, edge), kMeaningful) << "edge_gain does not reach the output";
    EXPECT_GT(MaxDiff(base, flat), kMeaningful) << "flat_gain does not reach the output";
    EXPECT_GT(MaxDiff(base, levels), kMeaningful) << "levels does not reach the output";
}

// The acquisition parameters feed estimateGain() -- kVp^2 * mAs, normalised at
// 80 kVp / 10 mAs and clamped to [0.1, 10] (exposure_index.cpp:150-180) -- and
// the gain multiplies mean directly in EI = c1 * g * mean + c2. #154 measured
// this function only with an empty meta, where the gain falls back to 1.0, so
// whether the acquisition parameters arrive was never actually observed.
TEST_F(AdvParamDependency, ExposureIndex_AcquisitionParametersReachTheOutput) {
    auto run = [](float kvp, float mas) {
        std::vector<float> px = Structured();
        XpeImageBuffer img = Wrap(px);
        XpeImageMetadata meta{};
        meta.kVp = kvp;
        meta.mAs = mas;
        float ei = 0.0f, di = 0.0f;
        EXPECT_EQ(XPE_OK, xpe_adv_calc_exposure_index(&img, &meta, &ei, &di));
        return ei;
    };

    const float ref  = run(80.0f, 10.0f);    // the model's reference point, g = 1
    const float hikv = run(120.0f, 10.0f);
    const float himas = run(80.0f, 40.0f);

    GTEST_LOG_(INFO) << "adv EI at 80kVp/10mAs=" << ref
                     << " 120kVp/10mAs=" << hikv
                     << " 80kVp/40mAs=" << himas;

    EXPECT_GT(std::fabs(hikv - ref), kMeaningful) << "kVp does not reach the output";
    EXPECT_GT(std::fabs(himas - ref), kMeaningful) << "mAs does not reach the output";
}

// The body-part target must reach DI. This is the enhance_advanced side of the
// #154 question: there the target cancelled out of DI entirely, and the same
// measurement is run here rather than assumed to differ.
TEST_F(AdvParamDependency, ExposureIndex_BodyPartReachesTheDeviationIndex) {
    auto run = [](const char* bodyPart) {
        std::vector<float> px = Structured();
        XpeImageBuffer img = Wrap(px);
        XpeImageMetadata meta{};
        std::snprintf(meta.bodyPart, sizeof(meta.bodyPart), "%s", bodyPart);
        float ei = 0.0f, di = 0.0f;
        EXPECT_EQ(XPE_OK, xpe_adv_calc_exposure_index(&img, &meta, &ei, &di));
        return std::pair<float, float>{ei, di};
    };

    const auto chest = run("CHEST");
    const auto skull = run("SKULL");

    GTEST_LOG_(INFO) << "adv CHEST EI=" << chest.first << " DI=" << chest.second
                     << " | SKULL EI=" << skull.first << " DI=" << skull.second;

    EXPECT_FLOAT_EQ(chest.first, skull.first)
        << "EI depends on the body part here too -- that is the #154 shape, and "
           "it would mean the advanced path has the same defect";
    EXPECT_GT(std::fabs(chest.second - skull.second), kMeaningful)
        << "DI does not move with the body part -- the target cancels out, which "
           "is exactly the #154 defect appearing on this path as well";
}

// Collimation detection has two documented fallbacks that return the FULL image
// extent -- low ROI confidence and a below-minimum area ratio
// (collimation_detect.cpp:177-192, :204-210). So "the box did not move" is only
// a finding if the fixture actually clears those thresholds; below them, an
// unchanged box is the function doing what it says.
//
// The first attempt used the shared 64x64 frame with an 8-pixel border and got
// [0,0,63,63] for both the plain and the bordered image. That is not evidence of
// a defect -- it is a fixture under the detector's own floor, the same trap the
// CLAHE case fell into in this sweep. This version uses a 256x256 frame with a
// wide, high-contrast border, which is what the detection is for.
TEST_F(AdvParamDependency, DetectCollimation_RespondsToTheImage) {
    static constexpr uint32_t kBigW = 256, kBigH = 256;

    auto run = [](bool withBorder) {
        std::vector<float> px(static_cast<size_t>(kBigW) * kBigH);
        for (uint32_t y = 0; y < kBigH; ++y) {
            for (uint32_t x = 0; x < kBigW; ++x) {
                const bool border = withBorder &&
                                    (x < 48 || x >= kBigW - 48 || y < 48 || y >= kBigH - 48);
                px[y * kBigW + x] = border
                    ? 0.0f
                    : 800.0f + 400.0f * (static_cast<float>(x) / (kBigW - 1));
            }
        }
        XpeImageBuffer img{};
        img.width         = kBigW;
        img.height        = kBigH;
        img.format        = XPE_PIXEL_FLOAT32;
        img.bitsAllocated = 32;
        img.bitsStored    = 32;
        img.data          = px.data();
        img.dataSize      = static_cast<uint32_t>(px.size() * sizeof(float));

        int32_t x0 = -1, y0 = -1, x1 = -1, y1 = -1;
        EXPECT_EQ(XPE_OK, xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr));
        return std::vector<int32_t>{x0, y0, x1, y1};
    };

    const std::vector<int32_t> plain    = run(false);
    const std::vector<int32_t> bordered = run(true);
    GTEST_LOG_(INFO) << "collimation 256x256 plain=[" << plain[0] << "," << plain[1]
                     << "," << plain[2] << "," << plain[3] << "] bordered=["
                     << bordered[0] << "," << bordered[1] << "," << bordered[2]
                     << "," << bordered[3] << "]";

    EXPECT_NE(plain, bordered)
        << "a 48-pixel black collimation border on a 256x256 frame produces the "
           "same box as an uncollimated frame -- either the image does not reach "
           "the result, or both runs fell through to the full-extent fallback, "
           "which the log line above distinguishes";
}
