// #145 (QA-B-61): per-call config warnings, once per configuration.
//
// QA-B-60 wired this warning into the two init entry points and left the
// per-call parsers alone with a stated reason: they run per frame, so one
// unknown key would become one alert per frame. That objection was about
// FREQUENCY, not content -- so the frequency is what this removes.
//
// What is asserted here, in the order the properties matter:
//
//   1. the flood is gone      -- 100 frames with one unknown config, 1 alert;
//   2. the silence holds      -- 100 frames with a correct config, 0 alerts;
//   3. a NEW mistake is heard -- changing the unknown key warns again, so the
//                                suppression remembers a configuration rather
//                                than simply going quiet after the first time;
//   4. values may vary freely -- the same unknown key with a different VALUE
//                                each frame is still one alert, which is why
//                                the memory compares key NAMES and not the
//                                config string;
//   5. nothing else changed   -- the processed pixels are identical with and
//                                without the warning path taken, and two
//                                threads do not disturb each other.
//
// (5) is the reentrancy question the card raised. The memory is thread_local:
// no thread can observe another's, and no result depends on it. The literal
// tension with REQ-ADV-032's "no global mutable state shall be modified during
// processing calls" is recorded in the QA-B-61 report, not argued away here --
// any warn-once behaviour needs memory that outlives a call, and the only
// choice is whether threads share it. These cases measure that they do not.

#include <gtest/gtest.h>

#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr uint32_t kW = 32, kH = 32;

std::vector<float> Frame() {
    std::vector<float> px(static_cast<size_t>(kW) * kH);
    for (size_t i = 0; i < px.size(); ++i) {
        px[i] = 500.0f + static_cast<float>(i % 97);
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

std::vector<std::string> DrainAlerts() {
    std::vector<std::string> out;
    for (int i = 0; i < 256; ++i) {
        char msg[256] = {0};
        int32_t severity = 0;
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &severity) != XPE_OK) break;
        if (msg[0] == '\0') break;
        out.emplace_back(msg);
    }
    xpe_clear_alerts();
    return out;
}

int CountMentioning(const std::vector<std::string>& alerts, const char* needle) {
    int n = 0;
    for (const auto& a : alerts) {
        if (a.find(needle) != std::string::npos) ++n;
    }
    return n;
}

// Run multiscale N times with the given config, returning the alerts raised.
std::vector<std::string> RunFrames(int n, const char* config) {
    xpe_clear_alerts();
    for (int i = 0; i < n; ++i) {
        std::vector<float> px = Frame();
        XpeImageBuffer img = Wrap(px);
        XpeImageMetadata meta{};
        EXPECT_EQ(XPE_OK, xpe_multiscale_process(&img, &meta, config));
    }
    return DrainAlerts();
}

class ConfigWarningOnce : public ::testing::Test {
protected:
    void SetUp() override    { ASSERT_EQ(XPE_OK, xpe_enhance_advanced_init(nullptr)); }
    void TearDown() override { xpe_clear_alerts(); xpe_enhance_advanced_shutdown(); }
};

}  // namespace

// (1) The flood is gone.
TEST_F(ConfigWarningOnce, HundredFramesWithOneUnknownKeyWarnOnce) {
    const auto alerts = RunFrames(100, "{\"edge_gian\": 2.0}");
    GTEST_LOG_(INFO) << "100 frames, unknown key: " << alerts.size() << " alert(s)";
    for (const auto& a : alerts) GTEST_LOG_(INFO) << "  " << a;

    EXPECT_EQ(1u, alerts.size())
        << "one unknown key produced one alert per frame; the queue now carries "
           "copies of a single fact and the next real alert is pushed out";
    EXPECT_EQ(1, CountMentioning(alerts, "edge_gian"))
        << "the misspelling must appear verbatim; that is what makes it findable";
}

// (2) The silence holds. This is the half that keeps the warning worth reading.
TEST_F(ConfigWarningOnce, HundredFramesWithValidConfigAreSilent) {
    const auto alerts = RunFrames(100, "{\"edge_gain\": 2.0, \"num_levels\": 3}");
    for (const auto& a : alerts) GTEST_LOG_(INFO) << "unexpected alert: " << a;
    EXPECT_TRUE(alerts.empty())
        << "every key here is consumed; warning on ordinary frames is how a "
           "reader learns to ignore warnings";
}

TEST_F(ConfigWarningOnce, NullAndEmptyConfigsAreSilent) {
    EXPECT_TRUE(RunFrames(50, nullptr).empty());
    EXPECT_TRUE(RunFrames(50, "{}").empty());
}

// The nested schema is consumed, so its inner keys are the ones to judge.
TEST_F(ConfigWarningOnce, NestedMfpSchemaJudgesInnerKeys) {
    const auto ok = RunFrames(10, "{\"mfp\": {\"edge_gain\": 2.0}}");
    for (const auto& a : ok) GTEST_LOG_(INFO) << "unexpected alert: " << a;
    EXPECT_TRUE(ok.empty()) << "a valid key inside the nested object was reported";

    const auto bad = RunFrames(10, "{\"mfp\": {\"edge_gian\": 2.0}}");
    EXPECT_EQ(1, CountMentioning(bad, "edge_gian"))
        << "a typo inside the nested object was not reported";
    EXPECT_EQ(0, CountMentioning(bad, "'mfp'"))
        << "the nested object key itself is consumed and must not be reported";
}

// (3) A DIFFERENT mistake must be heard. Without this, "warn once" could be
// implemented as "warn once, ever", which is quieter and much less useful.
//
// NOTE ON KEY NAMES: the memory is per-thread and lives as long as the process,
// which is what "once per configuration" means -- so a key another case already
// warned about is ALREADY REMEMBERED when this one runs, gtest running every
// case on one thread. Each case below therefore uses key names of its own. The
// first version of this file did not, and two cases failed for that reason and
// not for a defect: the suppression was working exactly as designed.
TEST_F(ConfigWarningOnce, ADifferentUnknownKeyWarnsAgain) {
    xpe_clear_alerts();
    for (int i = 0; i < 20; ++i) {
        std::vector<float> px = Frame();
        XpeImageBuffer img = Wrap(px);
        XpeImageMetadata meta{};
        const char* cfg = (i < 10) ? "{\"typo_three_a\": 2.0}" : "{\"typo_three_b\": 1.0}";
        ASSERT_EQ(XPE_OK, xpe_multiscale_process(&img, &meta, cfg));
    }
    const auto alerts = DrainAlerts();
    for (const auto& a : alerts) GTEST_LOG_(INFO) << "  " << a;

    EXPECT_EQ(1, CountMentioning(alerts, "typo_three_a"));
    EXPECT_EQ(1, CountMentioning(alerts, "typo_three_b"))
        << "the second typo was swallowed -- the suppression is remembering "
           "'already warned' rather than 'already warned about THIS'";
}

// A corrected config must reset the memory, or a caller that fixes a typo and
// later reintroduces it hears nothing the second time.
TEST_F(ConfigWarningOnce, FixingThenReintroducingTheTypoWarnsAgain) {
    xpe_clear_alerts();
    const char* seq[] = { "{\"typo_four\": 2.0}", "{\"edge_gain\": 2.0}",
                          "{\"typo_four\": 2.0}" };
    for (const char* cfg : seq) {
        std::vector<float> px = Frame();
        XpeImageBuffer img = Wrap(px);
        XpeImageMetadata meta{};
        ASSERT_EQ(XPE_OK, xpe_multiscale_process(&img, &meta, cfg));
    }
    const auto alerts = DrainAlerts();
    for (const auto& a : alerts) GTEST_LOG_(INFO) << "  " << a;
    EXPECT_EQ(2, CountMentioning(alerts, "typo_four"))
        << "the reintroduced typo was silent; a correct config must clear the "
           "memory of what was last reported";
}

// (4) Values may vary every frame. This is why the memory compares key names
// rather than the config string.
TEST_F(ConfigWarningOnce, VaryingTheValueEachFrameStillWarnsOnce) {
    xpe_clear_alerts();
    for (int i = 0; i < 50; ++i) {
        char cfg[64];
        std::snprintf(cfg, sizeof(cfg), "{\"typo_five\": %d.5}", i);
        std::vector<float> px = Frame();
        XpeImageBuffer img = Wrap(px);
        XpeImageMetadata meta{};
        ASSERT_EQ(XPE_OK, xpe_multiscale_process(&img, &meta, cfg));
    }
    const auto alerts = DrainAlerts();
    GTEST_LOG_(INFO) << "50 frames, same key with a new value each time: "
                     << alerts.size() << " alert(s)";
    EXPECT_EQ(1u, alerts.size())
        << "comparing the whole config string would warn on every frame here, "
           "which is the flood this card exists to remove";
}

// The other two per-call entry points carry the same wiring.
TEST_F(ConfigWarningOnce, FractionalAndCollimationWarnOncePerConfig) {
    xpe_clear_alerts();
    for (int i = 0; i < 30; ++i) {
        std::vector<float> px = Frame();
        XpeImageBuffer img = Wrap(px);
        ASSERT_EQ(XPE_OK, xpe_fractional_process(&img, 0.5f, "{\"step_sze\": 0.1}"));
    }
    const auto fractional = DrainAlerts();
    EXPECT_EQ(1, CountMentioning(fractional, "step_sze"));

    xpe_clear_alerts();
    for (int i = 0; i < 30; ++i) {
        std::vector<float> px = Frame();
        XpeImageBuffer img = Wrap(px);
        int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        ASSERT_EQ(XPE_OK, xpe_detect_collimation(&img, &x0, &y0, &x1, &y1,
                                                 "{\"sensitivty\": 0.5}"));
    }
    const auto collimation = DrainAlerts();
    EXPECT_EQ(1, CountMentioning(collimation, "sensitivty"));
}

// (5a) The processing result must not depend on the warning path.
TEST_F(ConfigWarningOnce, PixelsAreIdenticalWithAndWithoutTheWarning) {
    auto run = [](const char* cfg) {
        std::vector<float> px = Frame();
        XpeImageBuffer img = Wrap(px);
        XpeImageMetadata meta{};
        EXPECT_EQ(XPE_OK, xpe_multiscale_process(&img, &meta, cfg));
        return px;
    };

    // Same consumed settings; one config additionally carries an unknown key,
    // so one run takes the warning path and the other does not.
    const std::vector<float> quiet = run("{\"edge_gain\": 2.0}");
    const std::vector<float> noisy = run("{\"edge_gain\": 2.0, \"typo_six\": 9.0}");
    xpe_clear_alerts();

    EXPECT_EQ(quiet, noisy)
        << "the unknown key changed the output -- a diagnostic must not alter "
           "what it observes";
}

// (5b) Reentrancy: two threads with DIFFERENT unknown configs must each hear
// their own, and neither may silence the other. A module-global memory fails
// this by construction, which is why the memory is thread_local.
TEST_F(ConfigWarningOnce, ConcurrentThreadsDoNotEraseEachOthersWarning) {
    xpe_clear_alerts();
    std::atomic<int> ready{0};

    auto worker = [&ready](const char* cfg) {
        ready.fetch_add(1);
        while (ready.load() < 2) { /* line both threads up on the first frame */ }
        for (int i = 0; i < 40; ++i) {
            std::vector<float> px = Frame();
            XpeImageBuffer img = Wrap(px);
            XpeImageMetadata meta{};
            EXPECT_EQ(XPE_OK, xpe_multiscale_process(&img, &meta, cfg));
        }
    };

    std::thread a(worker, "{\"alpha_typo\": 1.0}");
    std::thread b(worker, "{\"beta_typo\": 2.0}");
    a.join();
    b.join();

    const auto alerts = DrainAlerts();
    for (const auto& s : alerts) GTEST_LOG_(INFO) << "  " << s;

    EXPECT_EQ(1, CountMentioning(alerts, "alpha_typo"))
        << "thread A's warning was lost or repeated";
    EXPECT_EQ(1, CountMentioning(alerts, "beta_typo"))
        << "thread B's warning was lost or repeated";
}
