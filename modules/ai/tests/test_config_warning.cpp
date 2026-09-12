// #145 (QA-B-60): an ai config key this module cannot read must say so.
//
// Same change as the gsvg side, on the other module in this lane that takes a
// JSON config at init. Each key is read conditionally, so a typo lands in none
// of the branches and xpe_ai_init still returns XPE_OK -- the silence QA-B-59
// measured in gsvg, with a different parser behind it.
//
// Two things are asserted, and the second is the one that keeps the first
// useful:
//   - an unread key is named verbatim, so a typo can be found from the message;
//   - a CORRECT config stays silent. A warning that fires on ordinary input
//     trains its reader to skip warnings, and the next one it hides is real.
//
// Return codes are unchanged: rejecting an unknown key is the #145 decision.
//
// One case here has no gsvg counterpart. This parser accepts a key only when
// the VALUE has the expected type, so "timeout_ms": "500" is silently
// unconsumed -- a mistake that looks right at a glance. It is reported with its
// own wording.

#include <gtest/gtest.h>

#include "xpe/ai/ai_api.h"
#include "xpe/common/xpe_error.h"

#include <string>
#include <vector>

namespace {

std::vector<std::string> DrainAlerts() {
    std::vector<std::string> out;
    for (int i = 0; i < 64; ++i) {
        char msg[256] = {0};
        int32_t severity = 0;
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &severity) != XPE_OK) break;
        if (msg[0] == '\0') break;
        out.emplace_back(msg);
    }
    xpe_clear_alerts();
    return out;
}

std::vector<std::string> InitAndCollect(const char* config) {
    xpe_clear_alerts();
    EXPECT_EQ(XPE_OK, xpe_ai_init("dummy_model_dir", config))
        << "the return code must not change -- only the diagnostic is new";
    xpe_ai_shutdown();
    return DrainAlerts();
}

bool AnyMentions(const std::vector<std::string>& alerts, const char* needle) {
    for (const auto& a : alerts) {
        if (a.find(needle) != std::string::npos) return true;
    }
    return false;
}

}  // namespace

TEST(AiConfigWarning, NullConfigIsSilent) {
    const auto alerts = InitAndCollect(nullptr);
    for (const auto& a : alerts) GTEST_LOG_(INFO) << "unexpected alert: " << a;
    EXPECT_TRUE(alerts.empty()) << "a NULL config means 'use the defaults'";
}

TEST(AiConfigWarning, EmptyObjectIsSilent) {
    const auto alerts = InitAndCollect("{}");
    for (const auto& a : alerts) GTEST_LOG_(INFO) << "unexpected alert: " << a;
    EXPECT_TRUE(alerts.empty());
}

TEST(AiConfigWarning, FullyValidConfigIsSilent) {
    const auto alerts = InitAndCollect(
        "{\"execution_provider\": \"cpu\", \"timeout_ms\": 500, "
        "\"confidence_threshold\": 0.8, \"fallback_mode\": true}");
    for (const auto& a : alerts) GTEST_LOG_(INFO) << "unexpected alert: " << a;
    EXPECT_TRUE(alerts.empty())
        << "every key here is consumed; a warning would be the first step toward "
           "warning fatigue";
}

TEST(AiConfigWarning, UnknownKeyIsReportedByName) {
    const auto alerts = InitAndCollect("{\"gpu_memory_limit_mb\": 2048}");
    for (const auto& a : alerts) GTEST_LOG_(INFO) << "alert: " << a;
    ASSERT_FALSE(alerts.empty());
    EXPECT_TRUE(AnyMentions(alerts, "gpu_memory_limit_mb"))
        << "the key is not named, so a typo cannot be found from the message";
}

TEST(AiConfigWarning, TypoOfAKnownKeyIsReportedByName) {
    const auto alerts = InitAndCollect("{\"timout_ms\": 500}");
    for (const auto& a : alerts) GTEST_LOG_(INFO) << "alert: " << a;
    ASSERT_FALSE(alerts.empty());
    EXPECT_TRUE(AnyMentions(alerts, "timout_ms"));
}

// The mistake that looks correct: right key, wrong type. The parser's type
// guard drops it, and before this change nothing said so.
TEST(AiConfigWarning, KnownKeyWithWrongTypeIsReported) {
    const auto alerts = InitAndCollect("{\"timeout_ms\": \"500\"}");
    for (const auto& a : alerts) GTEST_LOG_(INFO) << "alert: " << a;
    ASSERT_FALSE(alerts.empty())
        << "a known key with a string value was accepted in silence";
    EXPECT_TRUE(AnyMentions(alerts, "timeout_ms"));
    EXPECT_TRUE(AnyMentions(alerts, "type"))
        << "the message should distinguish a wrong type from an unknown name";
}

TEST(AiConfigWarning, MixedConfigNamesOnlyTheUnconsumed) {
    const auto alerts = InitAndCollect(
        "{\"timeout_ms\": 500, \"nonsense\": 1, \"fallback_mode\": false}");
    for (const auto& a : alerts) GTEST_LOG_(INFO) << "alert: " << a;
    EXPECT_TRUE(AnyMentions(alerts, "nonsense"));
    EXPECT_FALSE(AnyMentions(alerts, "timeout_ms"))
        << "a consumed key was reported";
    EXPECT_FALSE(AnyMentions(alerts, "fallback_mode"))
        << "a consumed key was reported";
}

TEST(AiConfigWarning, ReturnCodeIsUnchangedForUnknownKeys) {
    xpe_clear_alerts();
    EXPECT_EQ(XPE_OK, xpe_ai_init("dummy_model_dir", "{\"nonsense_key\": 123}"))
        << "the unknown key must warn, not reject";
    xpe_ai_shutdown();
    xpe_clear_alerts();
}
