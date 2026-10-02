/**
 * @file test_ghost_uncalibrated.cpp
 * @brief A ghost handle made without calibrated lag parameters passes frames through unchanged and says so once
 *        (QA-A-226, #241).
 *
 * Measured before the change (QA-A-225): the built-in defaults (alpha1 0.9 / tau1 1, alpha2 0.05 / tau2 20) are not a
 * calibration. The forward system they imply has a gain of about 2.45, so a constant 200-frame exposure is
 * "corrected" to 0.000 retention: a default handle destroyed the signal while reporting success. A handle with no
 * lag parameters of its own must therefore not correct at all. "Calibrated" is a fact about the configuration, not
 * about the numbers: all four of alpha1, tau1, alpha2 and tau2 were given (and non-empty).
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "preprocess_state_fixture.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr uint32_t W = 8, H = 8;
constexpr size_t N = static_cast<size_t>(W) * H;

// The text of the creation warning is a cross-lane contract (clients/ show it): prefix, then this sentence.
const char* const kPrefix = "XPE_WARN_GHOST_NOT_CALIBRATED:";
const char* const kText =
    "XPE_WARN_GHOST_NOT_CALIBRATED: ghost correction is not calibrated: the handle passes frames through unchanged "
    "until lag parameters are configured (alpha1, tau1, alpha2, tau2)";

const char* const kFull = "{\"alpha1\":0.02,\"tau1\":3,\"alpha2\":0.003,\"tau2\":30}";

XpeImageBuffer floatBuf(std::vector<float>& v) {
    XpeImageBuffer b{};
    b.data = v.data(); b.width = W; b.height = H; b.bitsAllocated = 32; b.bitsStored = 32;
    b.format = XPE_PIXEL_FLOAT32; b.dataSize = v.size() * sizeof(float);
    return b;
}

std::vector<float> frame(unsigned seed) {
    std::mt19937 g(seed);
    std::uniform_real_distribution<float> d(1000.0f, 30000.0f);
    std::vector<float> f(N);
    for (auto& x : f) x = d(g);
    return f;
}

bool sameBytes(const std::vector<float>& a, const std::vector<float>& b) {
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}

struct Alert { std::string text; int32_t severity; };
std::vector<Alert> alerts() {
    std::vector<Alert> out;
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < n; ++i) {
        char buf[1024];
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) == XPE_OK) out.push_back({buf, sev});
    }
    return out;
}

class GhostUncalibrated : public XpePreprocessStateFixture {
protected:
    void SetUp() override { XpePreprocessStateFixture::SetUp(); xpe_clear_alerts(); }
    void TearDown() override { xpe_clear_alerts(); XpePreprocessStateFixture::TearDown(); }

    void* make(const char* cfg) {
        void* h = nullptr;
        EXPECT_EQ(XPE_OK, xpe_ghost_create(W, H, cfg, &h));
        return h;
    }
    XpeImageMetadata meta{};

    // Runs `frames` frames of the same seeded content through the handle; returns the output of the last.
    std::vector<float> runFrames(void* h, int frames) {
        std::vector<float> last;
        for (int k = 0; k < frames; ++k) {
            std::vector<float> f = frame(7u + static_cast<unsigned>(k));
            XpeImageBuffer b = floatBuf(f);
            meta.acquisitionTime = 1000u + static_cast<uint64_t>(k);
            EXPECT_EQ(XPE_OK, xpe_ghost_correct(h, &b, &meta));
            last = f;
        }
        return last;
    }
};

// ---- the defect: a default handle must not correct -------------------------------------------------------

TEST_F(GhostUncalibrated, DefaultHandlePassesEveryFrameThroughBitForBit) {
    void* h = make(nullptr);
    for (int k = 0; k < 6; ++k) {
        const std::vector<float> in = frame(11u + static_cast<unsigned>(k));
        std::vector<float> f = in;
        XpeImageBuffer b = floatBuf(f);
        meta.acquisitionTime = 500u + static_cast<uint64_t>(k);
        ASSERT_EQ(XPE_OK, xpe_ghost_correct(h, &b, &meta));
        ASSERT_TRUE(sameBytes(in, f)) << "frame " << k << " was changed by an uncalibrated handle";
    }
    xpe_ghost_destroy(h);
}

TEST_F(GhostUncalibrated, ConstantExposureKeepsItsSignal) {
    // The measured failure: 200 constant frames came out as 0.000 retention.
    void* h = make(nullptr);
    std::vector<float> last;
    for (int k = 0; k < 200; ++k) {
        std::vector<float> f(N, 5000.0f);
        XpeImageBuffer b = floatBuf(f);
        meta.acquisitionTime = 100u + static_cast<uint64_t>(k);
        ASSERT_EQ(XPE_OK, xpe_ghost_correct(h, &b, &meta));
        last = f;
    }
    xpe_ghost_destroy(h);
    for (size_t i = 0; i < N; ++i) ASSERT_EQ(5000.0f, last[i]) << "pixel " << i;
}

TEST_F(GhostUncalibrated, OneWarningAtCreationAndNoneAfterwards) {
    void* h = make(nullptr);
    {
        const auto a = alerts();
        ASSERT_EQ(1u, a.size()) << "exactly one alert at creation";
        EXPECT_EQ(XPE_ALERT_WARNING, a[0].severity);
        EXPECT_EQ(std::string(kText), a[0].text);
        EXPECT_EQ(0u, a[0].text.rfind(kPrefix, 0));
    }
    runFrames(h, 25);
    EXPECT_EQ(1u, alerts().size()) << "frames add no alert: the warning is per handle, not per frame";
    xpe_ghost_destroy(h);
}

TEST_F(GhostUncalibrated, TwoHandlesWarnOnceEach) {
    void* a = make(nullptr);
    void* b = make("{\"tier\":2}");
    EXPECT_EQ(2u, alerts().size());
    xpe_ghost_destroy(a);
    xpe_ghost_destroy(b);
}

TEST_F(GhostUncalibrated, PassThroughLeavesTheHandleStateUntouched) {
    void* h = make(nullptr);
    runFrames(h, 5);
    auto* gh = static_cast<GhostCorrectorHandle*>(h);
    for (size_t i = 0; i < N; ++i) {
        ASSERT_EQ(0.0f, gh->hist1[i]);
        ASSERT_EQ(0.0f, gh->hist2[i]);
    }
    xpe_ghost_destroy(h);
}

TEST_F(GhostUncalibrated, ThreeOfFourParametersIsStillUncalibrated) {
    // "Calibrated" means all four were given. A missing one falls back to a default that is not a calibration.
    const char* cfgs[] = {
        "{\"alpha1\":0.02,\"tau1\":3,\"alpha2\":0.003}",
        "{\"alpha1\":0.02,\"tau1\":3,\"tau2\":30}",
        "{\"alpha1\":0.02,\"alpha2\":0.003,\"tau2\":30}",
        "{\"tau1\":3,\"alpha2\":0.003,\"tau2\":30}",
    };
    for (const char* c : cfgs) {
        xpe_clear_alerts();
        void* h = make(c);
        EXPECT_EQ(1u, alerts().size()) << c;
        const std::vector<float> in = frame(3);
        std::vector<float> f = in;
        XpeImageBuffer b = floatBuf(f);
        // two frames: a second frame is where an applied correction would show
        ASSERT_EQ(XPE_OK, xpe_ghost_correct(h, &b, &meta));
        f = in; b = floatBuf(f);
        ASSERT_EQ(XPE_OK, xpe_ghost_correct(h, &b, &meta));
        EXPECT_TRUE(sameBytes(in, f)) << c;
        xpe_ghost_destroy(h);
    }
}

TEST_F(GhostUncalibrated, TierAndOtherKeysAloneDoNotCalibrate) {
    const char* cfgs[] = {"{\"tier\":1}", "{\"tier\":3,\"nlcscBeta\":0.2}", "{\"tier2Threshold\":0.01}", "{}"};
    for (const char* c : cfgs) {
        xpe_clear_alerts();
        void* h = make(c);
        EXPECT_EQ(1u, alerts().size()) << c;
        xpe_ghost_destroy(h);
    }
}

TEST_F(GhostUncalibrated, EmptyValuesCountAsNotGiven) {
    // xpe_ghost_create treats an empty value as absent (it keeps the default), so it cannot calibrate either.
    void* h = make("{\"alpha1\":\"\",\"tau1\":\"\",\"alpha2\":\"\",\"tau2\":\"\"}");
    EXPECT_EQ(1u, alerts().size());
    const std::vector<float> in = frame(5);
    std::vector<float> f = in;
    XpeImageBuffer b = floatBuf(f);
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(h, &b, &meta));
    EXPECT_TRUE(sameBytes(in, f));
    xpe_ghost_destroy(h);
}

TEST_F(GhostUncalibrated, RefusedCreationRaisesNoWarning) {
    void* h = nullptr;
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_ghost_create(W, H, "{\"alpha1\":\"abc\"}", &h));
    EXPECT_EQ(nullptr, h);
    EXPECT_EQ(0u, alerts().size()) << "no handle came back, so there is nothing to warn about";
}

TEST_F(GhostUncalibrated, PassThroughStillRefusesWhatTheCorrectingPathRefuses) {
    void* h = make(nullptr);
    std::vector<float> f = frame(1);
    f[10] = std::numeric_limits<float>::quiet_NaN();
    XpeImageBuffer b = floatBuf(f);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ghost_correct(h, &b, &meta)) << "non-finite input is refused at the entrance";

    std::vector<uint16_t> u(N, 100);
    XpeImageBuffer ub{};
    ub.data = u.data(); ub.width = W; ub.height = H; ub.bitsAllocated = 16; ub.bitsStored = 16;
    ub.format = XPE_PIXEL_UINT16; ub.dataSize = u.size() * sizeof(uint16_t);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ghost_correct(h, &ub, &meta)) << "a uint16 frame is refused as before";

    XpeImageBuffer wrong = floatBuf(f);
    wrong.width = W + 1;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ghost_correct(h, &wrong, &meta)) << "a size mismatch is refused as before";
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ghost_correct(h, nullptr, &meta));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ghost_correct(h, &b, nullptr));
    xpe_ghost_destroy(h);
}

TEST_F(GhostUncalibrated, ResetStaysValid) {
    void* h = make(nullptr);
    EXPECT_EQ(XPE_OK, xpe_ghost_reset(h));
    xpe_ghost_destroy(h);
}

// ---- the other side: a configured handle behaves as it did ----------------------------------------------

TEST_F(GhostUncalibrated, ExplicitConfigurationRaisesNoWarningAndStillCorrects) {
    void* h = make(kFull);
    EXPECT_EQ(0u, alerts().size());
    // constant 1000: frame 1 passes (no history); frame 2 = 1000 - 0.02*1000 - 0.003*1000 = 977 (QA-A-226 D6 table)
    for (int k = 0; k < 2; ++k) {
        std::vector<float> f(N, 1000.0f);
        XpeImageBuffer b = floatBuf(f);
        meta.acquisitionTime = 100u + static_cast<uint64_t>(k);
        ASSERT_EQ(XPE_OK, xpe_ghost_correct(h, &b, &meta));
        const float expect = (k == 0) ? 1000.0f : 977.0f;
        for (size_t i = 0; i < N; ++i) ASSERT_NEAR(expect, f[i], 1e-3f) << "frame " << k << " pixel " << i;
    }
    EXPECT_EQ(0u, alerts().size());
    xpe_ghost_destroy(h);
}

TEST_F(GhostUncalibrated, ExplicitConfigurationInEveryTierRaisesNoWarning) {
    for (int tier = 1; tier <= 3; ++tier) {
        xpe_clear_alerts();
        const std::string cfg = "{\"tier\":" + std::to_string(tier) +
                                ",\"alpha1\":0.02,\"tau1\":3,\"alpha2\":0.003,\"tau2\":30}";
        void* h = make(cfg.c_str());
        EXPECT_EQ(0u, alerts().size()) << "tier " << tier;
        xpe_ghost_destroy(h);
    }
}

TEST_F(GhostUncalibrated, ExplicitConfigurationDiffersFromPassThroughOnASecondFrame) {
    // the control for the pass-through tests above: the same two frames through a calibrated handle DO change.
    void* h = make(kFull);
    const std::vector<float> in = frame(9);
    std::vector<float> f1 = in, f2 = in;
    XpeImageBuffer b1 = floatBuf(f1), b2 = floatBuf(f2);
    meta.acquisitionTime = 10;
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(h, &b1, &meta));
    meta.acquisitionTime = 11;
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(h, &b2, &meta));
    EXPECT_FALSE(sameBytes(in, f2));
    xpe_ghost_destroy(h);
}

} // namespace
