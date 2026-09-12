// #155 (QA-B-58): does each parameter reach the output? -- enhance_basic.
//
// The shape this looks for is the one behind #154 and #155: an input is read,
// a model is computed from it, and the input cancels out of the expression that
// produces the answer. Tests that pin the output's SHAPE (monotone, in range,
// "the curve looks like this") survive that intact, which is how both got past.
//
// So: vary one parameter, hold the rest, require the output to move -- by more
// than a rounding step, because a one-ulp difference means the parameter reached
// a floating-point association, not the answer.
//
// Not fixed where it does not move; these outputs are clinical values and
// changing them is a decision (#154 / #155 precedent).

#include <gtest/gtest.h>

#include "xpe/enhance_basic/enhance_basic_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr uint32_t kW = 64, kH = 64;
constexpr size_t   kN = static_cast<size_t>(kW) * kH;
constexpr double   kMeaningful = 1e-4;

// Structured content, not a flat field: a uniform image is invariant under most
// spatial filters, so it hides exactly the parameters this sweep is looking for
// (the QA-B-?? synthetic-data lesson -- a uniform frame made a detector's
// threshold untestable).
std::vector<float> Structured() {
    std::vector<float> px(kN);
    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            const float ramp  = 100.0f + 800.0f * (static_cast<float>(x) / (kW - 1));
            const float edge  = (x > kW / 2) ? 400.0f : 0.0f;      // a step to sharpen
            const float noise = ((x * 7919u + y * 104729u) % 97) * 1.5f;  // deterministic
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

}  // namespace

TEST(EnhanceBasicParameterDependency, LogTransform_NormFactorReachesTheOutput) {
    auto run = [](float normFactor) {
        std::vector<float> px = Structured();
        XpeImageBuffer img = Wrap(px);
        EXPECT_EQ(XPE_OK, xpe_log_transform(&img, normFactor));
        return px;
    };
    const double d = MaxDiff(run(1000.0f), run(4000.0f));
    GTEST_LOG_(INFO) << "log_transform normFactor 1000->4000 maxdiff=" << d;
    EXPECT_GT(d, kMeaningful) << "normFactor does not reach the output";
}

TEST(EnhanceBasicParameterDependency, LogInverse_NormFactorReachesTheOutput) {
    auto run = [](float normFactor) {
        std::vector<float> px = Structured();
        for (auto& v : px) v = v / 4096.0f;   // log-domain-ish input in [0,1)
        XpeImageBuffer img = Wrap(px);
        EXPECT_EQ(XPE_OK, xpe_log_inverse(&img, normFactor));
        return px;
    };
    const double d = MaxDiff(run(1000.0f), run(4000.0f));
    GTEST_LOG_(INFO) << "log_inverse normFactor 1000->4000 maxdiff=" << d;
    EXPECT_GT(d, kMeaningful) << "normFactor does not reach the output";
}

TEST(EnhanceBasicParameterDependency, NoiseReduce_EveryParameterReachesTheOutput) {
    auto run = [](XpeNoiseReduceMode mode, float sigmaSpace, float sigmaRange,
                  int32_t searchWindow, int32_t patchSize, float h) {
        std::vector<float> px = Structured();
        XpeImageBuffer img = Wrap(px);
        XpeNoiseReduceParams p{};
        p.mode          = mode;
        p.sigma_space   = sigmaSpace;
        p.sigma_range   = sigmaRange;
        p.search_window = searchWindow;
        p.patch_size    = patchSize;
        p.h_param       = h;
        EXPECT_EQ(XPE_OK, xpe_noise_reduce(&img, &p));
        return px;
    };

    const std::vector<float> base  = run(XPE_NOISE_BILATERAL, 3.0f, 50.0f, 21, 7, 10.0f);
    const std::vector<float> space = run(XPE_NOISE_BILATERAL, 8.0f, 50.0f, 21, 7, 10.0f);
    const std::vector<float> range = run(XPE_NOISE_BILATERAL, 3.0f, 200.0f, 21, 7, 10.0f);
    const std::vector<float> nlm   = run(XPE_NOISE_NLM,       3.0f, 50.0f, 21, 7, 10.0f);

    GTEST_LOG_(INFO) << "noise_reduce sigma_space 3->8 maxdiff=" << MaxDiff(base, space)
                     << " sigma_range 50->200=" << MaxDiff(base, range)
                     << " BILATERAL->NLM=" << MaxDiff(base, nlm);

    EXPECT_GT(MaxDiff(base, space), kMeaningful) << "sigma_space does not reach the output";
    EXPECT_GT(MaxDiff(base, range), kMeaningful) << "sigma_range does not reach the output";
    EXPECT_GT(MaxDiff(base, nlm),   kMeaningful) << "the mode selector does not reach the output";
}

TEST(EnhanceBasicParameterDependency, NoiseReduce_NlmParametersReachTheOutput) {
    auto run = [](int32_t searchWindow, int32_t patchSize, float h) {
        std::vector<float> px = Structured();
        XpeImageBuffer img = Wrap(px);
        XpeNoiseReduceParams p{};
        p.mode          = XPE_NOISE_NLM;
        p.sigma_space   = 3.0f;
        p.sigma_range   = 50.0f;
        p.search_window = searchWindow;
        p.patch_size    = patchSize;
        p.h_param       = h;
        EXPECT_EQ(XPE_OK, xpe_noise_reduce(&img, &p));
        return px;
    };

    const std::vector<float> base   = run(21, 7, 10.0f);
    const std::vector<float> window = run(7,  7, 10.0f);
    const std::vector<float> patch  = run(21, 3, 10.0f);
    const std::vector<float> hParam = run(21, 7, 40.0f);

    GTEST_LOG_(INFO) << "NLM search_window 21->7 maxdiff=" << MaxDiff(base, window)
                     << " patch_size 7->3=" << MaxDiff(base, patch)
                     << " h_param 10->40=" << MaxDiff(base, hParam);

    EXPECT_GT(MaxDiff(base, window), kMeaningful) << "search_window does not reach the output";
    EXPECT_GT(MaxDiff(base, patch),  kMeaningful) << "patch_size does not reach the output";
    EXPECT_GT(MaxDiff(base, hParam), kMeaningful) << "h_param does not reach the output";
}

// CLAHE needs its own fixture, and why is itself worth recording.
//
// clip_count = clip_limit * tile_area / NUM_BINS, with NUM_BINS = 4096
// (contrast_enhance.cpp:14, :69). On the shared 64x64 image with an 8x8 grid a
// tile holds 64 pixels spread over 4096 bins, so clip_count clamps to 1 at both
// clip_limit 3 and clip_limit 10, and no bin in a spread fixture reaches even
// that. The measured difference was exactly 0 -- and reporting THAT as
// "clip_limit does not reach the output" would have been a defect claim caused
// by the fixture. QA-B-52's control lesson, arriving from the other side: there
// a rejection was credited to a guard that had not run; here an absence of
// effect would have been credited to code that never got the chance to act.
//
// This fixture gives the clip something to clip: 256x256 with an 8x8 grid means
// 32x32 tiles (1024 px), and the content is packed into a few values so single
// bins hold hundreds of pixels -- far above clip_count at either setting.
TEST(EnhanceBasicParameterDependency, ContrastEnhance_EveryParameterReachesTheOutput) {
    static constexpr uint32_t kBigW = 256, kBigH = 256;
    static constexpr size_t   kBigN = static_cast<size_t>(kBigW) * kBigH;

    auto concentrated = []() {
        std::vector<float> px(kBigN);
        for (uint32_t y = 0; y < kBigH; ++y) {
            for (uint32_t x = 0; x < kBigW; ++x) {
                // A narrow band plus a sparse bright minority: a tall histogram
                // peak is the thing a clip limit acts on.
                const bool bright = ((x * 7919u + y * 104729u) % 17u) == 0u;
                px[y * kBigW + x] = bright ? 3000.0f : (500.0f + (x % 3u) * 2.0f);
            }
        }
        return px;
    };

    auto run = [&concentrated](float clip, int32_t tw, int32_t th) {
        std::vector<float> px = concentrated();
        XpeImageBuffer img{};
        img.width         = kBigW;
        img.height        = kBigH;
        img.format        = XPE_PIXEL_FLOAT32;
        img.bitsAllocated = 32;
        img.bitsStored    = 32;
        img.data          = px.data();
        img.dataSize      = static_cast<uint32_t>(px.size() * sizeof(float));

        XpeClaheParams p{};
        p.clip_limit  = clip;
        p.tile_width  = tw;
        p.tile_height = th;
        EXPECT_EQ(XPE_OK, xpe_contrast_enhance(&img, &p));
        return px;
    };

    const std::vector<float> base = run(3.0f, 8, 8);
    const std::vector<float> clip = run(40.0f, 8, 8);
    const std::vector<float> tile = run(3.0f, 2, 2);

    GTEST_LOG_(INFO) << "clahe clip_limit 3->40 maxdiff=" << MaxDiff(base, clip)
                     << " tiles 8x8->2x2=" << MaxDiff(base, tile);

    EXPECT_GT(MaxDiff(base, clip), kMeaningful) << "clip_limit does not reach the output";
    EXPECT_GT(MaxDiff(base, tile), kMeaningful) << "the tile grid does not reach the output";
}

TEST(EnhanceBasicParameterDependency, EdgeEnhance_EveryParameterReachesTheOutput) {
    auto run = [](float amount, float radius, float threshold) {
        std::vector<float> px = Structured();
        XpeImageBuffer img = Wrap(px);
        XpeUsmParams p{};
        p.amount    = amount;
        p.radius    = radius;
        p.threshold = threshold;
        EXPECT_EQ(XPE_OK, xpe_edge_enhance(&img, &p));
        return px;
    };

    const std::vector<float> base   = run(0.5f, 2.0f, 10.0f);
    const std::vector<float> amount = run(2.0f, 2.0f, 10.0f);
    const std::vector<float> radius = run(0.5f, 6.0f, 10.0f);
    const std::vector<float> thresh = run(0.5f, 2.0f, 300.0f);

    GTEST_LOG_(INFO) << "usm amount 0.5->2 maxdiff=" << MaxDiff(base, amount)
                     << " radius 2->6=" << MaxDiff(base, radius)
                     << " threshold 10->300=" << MaxDiff(base, thresh);

    EXPECT_GT(MaxDiff(base, amount), kMeaningful) << "amount does not reach the output";
    EXPECT_GT(MaxDiff(base, radius), kMeaningful) << "radius does not reach the output";
    EXPECT_GT(MaxDiff(base, thresh), kMeaningful) << "threshold does not reach the output";
}
