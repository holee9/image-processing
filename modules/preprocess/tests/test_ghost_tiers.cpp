/**
 * @file test_ghost_tiers.cpp
 * @brief Ghost correction Tier 2 / Tier 3 paths (QA-A-32, #120, REQ-P1A-032/033)
 *
 * `ghost_correct.cpp` measured 81/156 covered (0.52) in coverage run
 * 34479945843. The uncovered block was almost entirely the two higher tiers:
 * `compute_frame_mean` (87-96), `ghost_tier2` (115-138), `ghost_tier3`
 * (142-196) and the switch arms that select them (234-240). Nothing reached
 * them because every existing case creates the handle with a NULL config, and
 * `xpe_ghost_create` defaults `tier` to 1.
 *
 * These cases drive tiers 2 and 3 through `{"tier":N}` and exercise the
 * branches inside them: exposure weighting, the 3x3 spatial-context blend and
 * the small-image path that skips it, the non-finite guards, and the tier
 * clamp in `xpe_ghost_create`.
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace {

class GhostTierTest : public ::testing::Test {
protected:
    void* handle{nullptr};

    void TearDown() override {
        if (handle) { xpe_ghost_destroy(handle); handle = nullptr; }
    }

    void create(uint32_t w, uint32_t h, const char* config) {
        ASSERT_EQ(XPE_OK, xpe_ghost_create(w, h, config, &handle));
        ASSERT_NE(nullptr, handle);
    }

    // A FLOAT32 frame the correction can run over in place.
    struct Frame {
        std::vector<float> pixels;
        XpeImageBuffer     buf{};
        Frame(uint32_t w, uint32_t h, float fill) {
            pixels.assign(static_cast<size_t>(w) * h, fill);
            buf.data = pixels.data();
            buf.width = w; buf.height = h;
            buf.bitsAllocated = 32; buf.bitsStored = 32;
            buf.format = XPE_PIXEL_FLOAT32;
            buf.dataSize = pixels.size() * sizeof(float);
        }
    };
};

// Tier 2 runs and leaves a plausible frame behind. The first frame has no
// history, so the correction subtracts nothing and the values survive.
TEST_F(GhostTierTest, Tier2FirstFrameIsUnchanged) {
    create(8, 8, "{\"tier\":2}");
    Frame f(8, 8, 1000.0f);
    XpeImageMetadata meta{};

    ASSERT_EQ(XPE_OK, xpe_ghost_correct(handle, &f.buf, &meta));
    for (float v : f.pixels)
        EXPECT_FLOAT_EQ(1000.0f, v) << "no history yet, so nothing to subtract";
}

// Tier 2 weights the correction by frame mean, so a second identical frame
// comes back reduced -- the history term is now non-zero.
TEST_F(GhostTierTest, Tier2SecondFrameIsReducedByHistory) {
    create(8, 8, "{\"tier\":2}");
    XpeImageMetadata meta{};

    Frame first(8, 8, 1000.0f);
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(handle, &first.buf, &meta));

    Frame second(8, 8, 1000.0f);
    meta.acquisitionTime = 1;
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(handle, &second.buf, &meta));

    EXPECT_LT(second.pixels[0], 1000.0f)
        << "the ghost term from the first frame must be subtracted";
    EXPECT_GE(second.pixels[0], 0.0f) << "output is clamped at zero";
}

// A brighter frame gets a larger exposure weight, so its ghost subtraction is
// proportionally stronger than a dim frame's. Two handles, same history depth.
TEST_F(GhostTierTest, Tier2ExposureWeightScalesWithSignal) {
    XpeImageMetadata meta{};

    auto residualAfterTwoFrames = [&](float level) {
        void* h = nullptr;
        EXPECT_EQ(XPE_OK, xpe_ghost_create(8, 8, "{\"tier\":2}", &h));
        XpeImageMetadata m{};
        Frame a(8, 8, level);
        EXPECT_EQ(XPE_OK, xpe_ghost_correct(h, &a.buf, &m));
        Frame b(8, 8, level);
        m.acquisitionTime = 1;
        EXPECT_EQ(XPE_OK, xpe_ghost_correct(h, &b.buf, &m));
        xpe_ghost_destroy(h);
        return level - b.pixels[0];   // how much was subtracted
    };

    const float dimSubtraction    = residualAfterTwoFrames(1000.0f);
    const float brightSubtraction = residualAfterTwoFrames(20000.0f);

    // Scale out the signal level: what should differ is the *relative* pull.
    EXPECT_GT(brightSubtraction / 20000.0f, dimSubtraction / 1000.0f)
        << "exposureWeight = 1 + (mean/32768)*0.5 must strengthen the correction";
    (void)meta;
}

// Tier 3 adds a 3x3 spatial-context blend for interior pixels. An 8x8 frame has
// interior pixels, so the blend runs.
TEST_F(GhostTierTest, Tier3RunsWithSpatialContext) {
    create(8, 8, "{\"tier\":3}");
    XpeImageMetadata meta{};

    Frame first(8, 8, 1000.0f);
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(handle, &first.buf, &meta));

    Frame second(8, 8, 1000.0f);
    meta.acquisitionTime = 1;
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(handle, &second.buf, &meta));

    for (float v : second.pixels) {
        EXPECT_TRUE(std::isfinite(v));
        EXPECT_GE(v, 0.0f);
        EXPECT_LE(v, 1000.0f);
    }
}

// A 2x2 frame has no interior pixel, so tier 3 skips the blend entirely
// (the `W >= 3 && H >= 3` guard). The correction still succeeds.
TEST_F(GhostTierTest, Tier3SkipsSpatialContextOnTinyImage) {
    create(2, 2, "{\"tier\":3}");
    XpeImageMetadata meta{};

    Frame f(2, 2, 500.0f);
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(handle, &f.buf, &meta));
    for (float v : f.pixels) EXPECT_FLOAT_EQ(500.0f, v);
}

// nlcscBeta feeds tier 3's signal-dependent coefficient. A larger beta must
// pull a bright pixel down harder on the second frame.
TEST_F(GhostTierTest, Tier3BetaStrengthensSignalDependence) {
    // The level matters: at 20000 both settings over-subtract and clamp to 0,
    // which compares two floors rather than two corrections (measured). 2000
    // keeps both results above the clamp.
    auto residual = [](const char* config) {
        void* h = nullptr;
        EXPECT_EQ(XPE_OK, xpe_ghost_create(8, 8, config, &h));
        XpeImageMetadata m{};
        Frame a(8, 8, 2000.0f);
        EXPECT_EQ(XPE_OK, xpe_ghost_correct(h, &a.buf, &m));
        Frame b(8, 8, 2000.0f);
        m.acquisitionTime = 1;
        EXPECT_EQ(XPE_OK, xpe_ghost_correct(h, &b.buf, &m));
        xpe_ghost_destroy(h);
        return b.pixels[0];
    };

    const float small = residual("{\"tier\":3,\"nlcscBeta\":0.1}");
    const float large = residual("{\"tier\":3,\"nlcscBeta\":2.0}");

    ASSERT_GT(small, 0.0f) << "guard: comparing corrections, not two zero clamps";
    EXPECT_LT(large, small) << "a larger nlcscBeta subtracts more";
}

// Every tier rejects a non-finite sample rather than propagating it.
TEST_F(GhostTierTest, EachTierRejectsNonFiniteInput) {
    for (const char* config : {"{\"tier\":1}", "{\"tier\":2}", "{\"tier\":3}"}) {
        void* h = nullptr;
        ASSERT_EQ(XPE_OK, xpe_ghost_create(8, 8, config, &h));
        XpeImageMetadata meta{};

        Frame f(8, 8, 1000.0f);
        f.pixels[5] = std::numeric_limits<float>::quiet_NaN();

        EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, xpe_ghost_correct(h, &f.buf, &meta))
            << "config " << config;
        xpe_ghost_destroy(h);
    }
}

// xpe_ghost_create clamps an out-of-range tier back to 1 rather than failing,
// so the handle stays usable.
TEST_F(GhostTierTest, OutOfRangeTierFallsBackToTierOne) {
    create(8, 8, "{\"tier\":9}");
    XpeImageMetadata meta{};

    Frame first(8, 8, 1000.0f);
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(handle, &first.buf, &meta));

    // Tier 1 applies no exposure weighting, so a second identical frame is
    // reduced by exactly alpha1*hist1 + alpha2*hist2 with unscaled alphas.
    Frame second(8, 8, 1000.0f);
    meta.acquisitionTime = 1;
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(handle, &second.buf, &meta));
    EXPECT_LT(second.pixels[0], 1000.0f);
}

// The acquisitionTime delta guard: a non-increasing timestamp must not produce
// a negative or zero dt (which would make exp(-dt/tau) blow up).
TEST_F(GhostTierTest, NonIncreasingTimestampUsesGuardedDelta) {
    create(8, 8, "{\"tier\":2}");
    XpeImageMetadata meta{};
    meta.acquisitionTime = 10;

    Frame first(8, 8, 1000.0f);
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(handle, &first.buf, &meta));

    Frame second(8, 8, 1000.0f);
    meta.acquisitionTime = 5;   // earlier than the previous frame
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(handle, &second.buf, &meta));

    for (float v : second.pixels) EXPECT_TRUE(std::isfinite(v));
}

// Coefficient overrides parse and take effect: a larger alpha1 subtracts more.
TEST_F(GhostTierTest, Alpha1OverrideChangesTheCorrection) {
    auto residual = [](const char* config) {
        void* h = nullptr;
        EXPECT_EQ(XPE_OK, xpe_ghost_create(8, 8, config, &h));
        XpeImageMetadata m{};
        Frame a(8, 8, 1000.0f);
        EXPECT_EQ(XPE_OK, xpe_ghost_correct(h, &a.buf, &m));
        Frame b(8, 8, 1000.0f);
        m.acquisitionTime = 1;
        EXPECT_EQ(XPE_OK, xpe_ghost_correct(h, &b.buf, &m));
        xpe_ghost_destroy(h);
        return b.pixels[0];
    };

    EXPECT_LT(residual("{\"tier\":1,\"alpha1\":0.2}"),
              residual("{\"tier\":1,\"alpha1\":0.01}"))
        << "alpha1 scales the first history term";
}

} // namespace
