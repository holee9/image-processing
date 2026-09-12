// #145 (QA-B-60): a config key this module cannot read must say so.
//
// QA-B-59 measured the silence: a config naming only gridFrequency_lp_per_mm
// and virtual_grid_enabled -- both described in the documentation -- produced
// 0/4096 changed pixels, XPE_OK, and no diagnostic. The caller has no way to
// learn that the setting did nothing.
//
// The warning is the whole change. RETURN CODES ARE UNCHANGED: rejecting an
// unknown key is a behaviour change and belongs to the #145 decision about which
// side -- documentation or code -- is the one that is wrong.
//
// The other half matters as much and is asserted just as hard: a CORRECT config
// must stay quiet. A warning that fires on ordinary input trains its reader to
// skip warnings, and the next one it hides is a real one. So NULL, empty, and
// fully-valid configs are each checked for silence, and the valid case is
// checked with BOTH keys present rather than one.

#include <gtest/gtest.h>

#include "xpe/gsvg/gsvg_api.h"
#include "xpe/common/xpe_error.h"

#include <cstring>
#include <string>
#include <vector>

namespace {

// Drain whatever the queue holds, returning the messages. Draining is part of
// the fixture: an alert left behind by an earlier case would be read as this
// case's, which is the test-side version of the defect this file is about.
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
    void* handle = nullptr;
    EXPECT_EQ(XPE_OK, xpe_gsvg_init(&handle, config))
        << "the return code must not change -- only the diagnostic is new";
    xpe_gsvg_shutdown(handle);
    return DrainAlerts();
}

bool AnyMentions(const std::vector<std::string>& alerts, const char* needle) {
    for (const auto& a : alerts) {
        if (a.find(needle) != std::string::npos) return true;
    }
    return false;
}

}  // namespace

// --- the silence that must stay silent -------------------------------------

TEST(GsvgConfigWarning, NullConfigIsSilent) {
    const auto alerts = InitAndCollect(nullptr);
    for (const auto& a : alerts) GTEST_LOG_(INFO) << "unexpected alert: " << a;
    EXPECT_TRUE(alerts.empty())
        << "a NULL config means 'use the defaults', which is an ordinary call";
}

TEST(GsvgConfigWarning, EmptyConfigIsSilent) {
    const auto alerts = InitAndCollect("{}");
    for (const auto& a : alerts) GTEST_LOG_(INFO) << "unexpected alert: " << a;
    EXPECT_TRUE(alerts.empty()) << "an empty object names no key, so there is nothing to report";
}

TEST(GsvgConfigWarning, ValidConfigIsSilent) {
    const auto alerts =
        InitAndCollect("{\"vignette_correction\": true, \"grid_suppression\": false}");
    for (const auto& a : alerts) GTEST_LOG_(INFO) << "unexpected alert: " << a;
    EXPECT_TRUE(alerts.empty())
        << "both keys are read by this module; warning here would be the start of "
           "warning fatigue, which costs the next real warning";
}

// A value that happens to be a string must not be mistaken for a key. This is
// the scanner's most likely false positive, so it is asserted rather than
// assumed.
TEST(GsvgConfigWarning, StringValuesAreNotMistakenForKeys) {
    const auto alerts =
        InitAndCollect("{\"vignette_correction\": true, \"grid_suppression\": true}");
    EXPECT_TRUE(alerts.empty());

    // "true"/"false" as strings, and a value containing a colon.
    const auto alerts2 = InitAndCollect("{\"vignette_correction\": \"yes:no\"}");
    for (const auto& a : alerts2) GTEST_LOG_(INFO) << "alert: " << a;
    EXPECT_FALSE(AnyMentions(alerts2, "yes:no"))
        << "a string VALUE was reported as an unknown KEY";
}

// --- the silence that must end ---------------------------------------------

TEST(GsvgConfigWarning, UnknownKeyIsReportedByName) {
    const auto alerts = InitAndCollect("{\"gridFrequency_lp_per_mm\": 4.0}");
    for (const auto& a : alerts) GTEST_LOG_(INFO) << "alert: " << a;

    ASSERT_FALSE(alerts.empty()) << "an unreadable key produced no diagnostic";
    EXPECT_TRUE(AnyMentions(alerts, "gridFrequency_lp_per_mm"))
        << "the warning does not name the key, so a typo cannot be found from it";
}

TEST(GsvgConfigWarning, TypoOfAKnownKeyIsReportedByName) {
    // The case the naming requirement exists for: one letter off.
    const auto alerts = InitAndCollect("{\"grid_supression\": true}");
    for (const auto& a : alerts) GTEST_LOG_(INFO) << "alert: " << a;

    ASSERT_FALSE(alerts.empty());
    EXPECT_TRUE(AnyMentions(alerts, "grid_supression"))
        << "the misspelling must appear verbatim; that is what makes it findable";
}

TEST(GsvgConfigWarning, EveryUnknownKeyIsNamed) {
    const auto alerts = InitAndCollect(
        "{\"gridFrequency_lp_per_mm\": 4.0, \"virtual_grid_enabled\": 1, "
        "\"grid_suppression\": true}");
    for (const auto& a : alerts) GTEST_LOG_(INFO) << "alert: " << a;

    EXPECT_TRUE(AnyMentions(alerts, "gridFrequency_lp_per_mm"));
    EXPECT_TRUE(AnyMentions(alerts, "virtual_grid_enabled"));
    EXPECT_FALSE(AnyMentions(alerts, "grid_suppression"))
        << "a key the module DOES read was reported as unknown";
}

// Nested objects are not the top level. A key inside one belongs to whatever
// reads that object, and reporting it here would be a false positive.
TEST(GsvgConfigWarning, NestedKeysAreNotReported) {
    const auto alerts =
        InitAndCollect("{\"grid_suppression\": true, \"nested\": {\"inner_key\": 1}}");
    for (const auto& a : alerts) GTEST_LOG_(INFO) << "alert: " << a;

    EXPECT_FALSE(AnyMentions(alerts, "inner_key"))
        << "a key nested inside a value object was reported as a top-level key";
    EXPECT_TRUE(AnyMentions(alerts, "nested"))
        << "the top-level key 'nested' is itself unread by this module and should "
           "be named";
}

TEST(GsvgConfigWarning, ReturnCodeIsUnchangedForUnknownKeys) {
    void* handle = nullptr;
    EXPECT_EQ(XPE_OK, xpe_gsvg_init(&handle, "{\"nonsense_key\": 123}"))
        << "the unknown key must warn, not reject -- rejection is the #145 decision";
    EXPECT_NE(nullptr, handle);
    xpe_gsvg_shutdown(handle);
    xpe_clear_alerts();
}
