/**
 * @file test_alert_queue_overflow.cpp
 * @brief Alert queue overflow policy (api-spec.md §5.17, SRS-ALERT-007, HAZ-006)
 *
 * QA-A-28 (#110). Before this card `enqueue_alert` dropped the oldest entry
 * regardless of severity and recorded nothing, so a burst of Info alerts could
 * silently discard an Error and no consumer could tell that anything was lost.
 *
 * §5.17 replaces that with two rules:
 *   1. Priority-protected FIFO eviction -- oldest Info first, then oldest
 *      Warning, then oldest Error; strictly FIFO inside one severity class.
 *   2. A guaranteed loss alert -- while the cumulative eviction count since the
 *      last xpe_clear_alerts is > 0, exactly one XPE_ALERT_ERROR entry reading
 *      "alert queue overflow: <N> alert(s) dropped" is retrievable. It is
 *      updated in place, is never evicted, and occupies one of the 64 slots.
 *
 * The capacity (64) is an implementation constant, not an exported symbol, so
 * these cases rediscover it by filling the queue rather than asserting it.
 */

#include <gtest/gtest.h>
#include "xpe/common/xpe_error.h"

#include <string>
#include <vector>

namespace {

// api-spec §5.17: "kAlertQueueMax, currently 64 entries".
constexpr int32_t kCapacity = 64;
constexpr const char* kLossPrefix = "alert queue overflow:";

struct Alert {
    std::string message;
    int32_t     severity{0};
};

// Drains the queue through the public accessors (§5.9-§5.11).
static std::vector<Alert> readAll() {
    std::vector<Alert> out;
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < n; ++i) {
        char    buf[512] = {0};
        int32_t sev = -1;
        EXPECT_EQ(XPE_OK, xpe_get_pending_alert(i, buf, sizeof(buf), &sev))
            << "index " << i << " of " << n;
        out.push_back(Alert{std::string(buf), sev});
    }
    return out;
}

static int countSeverity(const std::vector<Alert>& v, int32_t severity) {
    int n = 0;
    for (const auto& a : v)
        if (a.severity == severity && a.message.rfind(kLossPrefix, 0) != 0) ++n;
    return n;
}

static int countLossAlerts(const std::vector<Alert>& v) {
    int n = 0;
    for (const auto& a : v)
        if (a.message.rfind(kLossPrefix, 0) == 0) ++n;
    return n;
}

static const Alert* findLossAlert(const std::vector<Alert>& v) {
    for (const auto& a : v)
        if (a.message.rfind(kLossPrefix, 0) == 0) return &a;
    return nullptr;
}

static bool contains(const std::vector<Alert>& v, const std::string& msg) {
    for (const auto& a : v)
        if (a.message == msg) return true;
    return false;
}

static void pushN(int32_t count, int32_t severity, const char* tag) {
    for (int32_t i = 0; i < count; ++i) {
        const std::string msg = std::string(tag) + "#" + std::to_string(i);
        xpe_alert_push(msg.c_str(), severity);
    }
}

class AlertQueueOverflowTest : public ::testing::Test {
protected:
    void SetUp() override    { xpe_clear_alerts(); }
    void TearDown() override { xpe_clear_alerts(); }
};

// Rule 1: a Warning pushed onto a queue full of Info evicts Info, not the
// Warning itself. The loss alert takes a slot too, so two Info entries leave.
TEST_F(AlertQueueOverflowTest, WarningEvictsOldestInfoNotItself) {
    pushN(kCapacity, XPE_ALERT_INFO, "info");
    ASSERT_EQ(kCapacity, xpe_get_pending_alert_count());
    ASSERT_EQ(0, countLossAlerts(readAll())) << "no eviction has happened yet";

    xpe_alert_push("warn-A", XPE_ALERT_WARNING);

    const auto all = readAll();
    EXPECT_EQ(kCapacity, static_cast<int32_t>(all.size()))
        << "the queue stays exactly full";
    EXPECT_TRUE(contains(all, "warn-A")) << "the pushed Warning must survive";
    EXPECT_EQ(1, countLossAlerts(all));
    // One slot freed for the Warning, one for the loss alert.
    EXPECT_EQ(kCapacity - 2, countSeverity(all, XPE_ALERT_INFO));
    EXPECT_FALSE(contains(all, "info#0")) << "eviction is FIFO within Info";
    EXPECT_FALSE(contains(all, "info#1"));
    EXPECT_TRUE(contains(all, "info#2"));
}

// Rule 1: with no Info left, a Warning push evicts the oldest Warning and
// leaves the Error alone.
TEST_F(AlertQueueOverflowTest, WithoutInfoEvictsOldestWarningAndKeepsError) {
    xpe_alert_push("err-keep", XPE_ALERT_ERROR);
    pushN(kCapacity - 1, XPE_ALERT_WARNING, "warn");
    ASSERT_EQ(kCapacity, xpe_get_pending_alert_count());

    xpe_alert_push("warn-new", XPE_ALERT_WARNING);

    const auto all = readAll();
    EXPECT_TRUE(contains(all, "err-keep")) << "Error must outlive Warnings";
    EXPECT_TRUE(contains(all, "warn-new"));
    EXPECT_FALSE(contains(all, "warn#0")) << "oldest Warning goes first";
    EXPECT_FALSE(contains(all, "warn#1"));
    EXPECT_EQ(1, countLossAlerts(all));
}

// Rule 2: the loss alert is a single Error entry carrying the cumulative count,
// updated in place rather than duplicated.
TEST_F(AlertQueueOverflowTest, LossAlertIsSingleAndUpdatedInPlace) {
    pushN(kCapacity, XPE_ALERT_INFO, "info");
    xpe_alert_push("first-overflow", XPE_ALERT_INFO);

    auto all = readAll();
    const Alert* loss = findLossAlert(all);
    ASSERT_NE(nullptr, loss) << "an eviction must produce a loss alert";
    EXPECT_EQ(XPE_ALERT_ERROR, loss->severity);
    EXPECT_EQ("alert queue overflow: 2 alert(s) dropped", loss->message)
        << "one eviction for the pushed alert, one for the loss alert's slot";

    pushN(5, XPE_ALERT_INFO, "more");

    all = readAll();
    EXPECT_EQ(1, countLossAlerts(all)) << "never duplicated";
    loss = findLossAlert(all);
    ASSERT_NE(nullptr, loss);
    EXPECT_EQ("alert queue overflow: 7 alert(s) dropped", loss->message)
        << "the count is cumulative and updated in place";
    EXPECT_EQ(kCapacity, xpe_get_pending_alert_count());
}

// Rule 2: the loss alert is never selected for eviction, however long the
// overflow continues.
TEST_F(AlertQueueOverflowTest, LossAlertIsNeverEvicted) {
    pushN(kCapacity, XPE_ALERT_INFO, "info");
    pushN(200, XPE_ALERT_INFO, "flood");

    const auto all = readAll();
    EXPECT_EQ(kCapacity, static_cast<int32_t>(all.size()));
    EXPECT_EQ(1, countLossAlerts(all)) << "still exactly one, after 200 pushes";
    const Alert* loss = findLossAlert(all);
    ASSERT_NE(nullptr, loss);
    EXPECT_EQ(XPE_ALERT_ERROR, loss->severity);
}

// Rule 2: xpe_clear_alerts removes the loss alert and resets the counter, so a
// fresh overflow starts counting from 1 again.
TEST_F(AlertQueueOverflowTest, ClearAlertsResetsTheDropCounter) {
    pushN(kCapacity + 10, XPE_ALERT_INFO, "info");
    ASSERT_EQ(1, countLossAlerts(readAll()));

    xpe_clear_alerts();
    EXPECT_EQ(0, xpe_get_pending_alert_count());

    pushN(kCapacity, XPE_ALERT_INFO, "again");
    EXPECT_EQ(0, countLossAlerts(readAll()))
        << "a full queue with no eviction yet has no loss alert";

    xpe_alert_push("one-more", XPE_ALERT_INFO);
    const auto all = readAll();
    const Alert* loss = findLossAlert(all);
    ASSERT_NE(nullptr, loss);
    EXPECT_EQ("alert queue overflow: 2 alert(s) dropped", loss->message)
        << "the counter restarted after the clear";
}

// A queue that has not overflowed carries no loss alert at all -- the policy is
// invisible until it fires.
TEST_F(AlertQueueOverflowTest, NoLossAlertWithoutOverflow) {
    pushN(10, XPE_ALERT_INFO, "info");
    pushN(3, XPE_ALERT_ERROR, "err");

    const auto all = readAll();
    EXPECT_EQ(13, static_cast<int32_t>(all.size()));
    EXPECT_EQ(0, countLossAlerts(all));
}

} // namespace
