/**
 * @file test_global_state_hygiene.cpp
 * @brief Mechanical guard: no test may leave global module state changed -- QA-A-113 (#176)
 *
 * WHY THIS EXISTS.
 *
 * `ctest` runs each test in its own process (`gtest_discover_tests`), so a test
 * that leaves global state behind still passes there -- forever. #176 measured
 * the consequence: running the binary directly failed 1 test in default order
 * and 3 under shuffle, none of which CI had ever seen. QA-A-112 then added a
 * fourth of exactly the same shape.
 *
 * Counting `init` / `shutdown` occurrences in the sources cannot decide this:
 * the calls sit inside loops, helpers and deliberate negative cases. Only
 * running can. So this file checks the state AFTER EVERY TEST, in the same
 * process, and names whoever left it dirty.
 *
 * FALSIFYING A GUARD IN THIS REPOSITORY -- READ BEFORE WRITING ONE.
 *
 * To show a guard actually fires, disable what it guards and confirm the red.
 * The condition you disable it with MUST be one the compiler cannot fold.
 * `if (false && ...)`, `if (kSomeConstexpr == 0)` and friends raise a
 * constant-conditional / unreachable-code warning, `/WX` turns that into
 * BUILD_EXIT=1, and the STALE binary then runs and passes -- so the
 * falsification reads as "the guard was not needed" when in fact it never ran.
 * QA-A-136 and QA-A-137 each hit this, consecutively. Use a runtime-false
 * expression instead (`::testing::UnitTest::GetInstance() == nullptr` works),
 * and read BUILD_EXIT before believing any falsification result.
 *
 * WHAT IS CHECKED, AND WHY EXACTLY THESE FOUR.
 *
 *   1. module initialization -- `xpe_preprocess_init()` refuses a second call by
 *      design, so a test that leaves the module up breaks the NEXT fixture that
 *      asserts on its own init (the QA-A-112 failure, seed 9).
 *   2. calibration mode -- `xpe_preprocess_shutdown()` deliberately does NOT
 *      reset it (preprocess_api.h says so), so it survives a fixture that
 *      believes teardown covered it (#176 row 1, `CalibModeTest`).
 *   3. quality metadata -- same contract, same hazard. This axis was REPORTED
 *      ONLY until QA-A-120 (#176 item 2), because nothing public restored it
 *      and failing a test for it would have blamed the test for a missing
 *      capability. xpe_preprocess_shutdown() now clears it along with every
 *      other module global, so a fixture that pairs its own init/shutdown
 *      restores this axis for free -- and the reported count went from 30 to 0
 *      the moment that landed. It fails like the others now.
 *
 *   4. pending alert queue -- QA-A-138. This axis was MISSING, not excluded:
 *      the note above enumerated three axes and gave an explicit reason for
 *      leaving the calibration maps out, while the queue appeared in neither
 *      list. QA-A-137 measured the cost. An alert pushed by one test survived
 *      into the next and made its NEGATIVE assertion ("this file raises no
 *      such alert") fail -- green under ctest, red in a single process. The
 *      guard exists for exactly that shape and did not see it.
 *      Like the calibration mode, the queue is deliberately NOT cleared by
 *      xpe_preprocess_shutdown(): it is a common-module global shared across
 *      modules, and emptying it when one module goes down would discard
 *      another's undelivered alerts. Draining it is the consumer's job,
 *      through the public xpe_clear_alerts() -- so a test CAN restore this
 *      axis, which is what makes it fair to fail one that does not.
 *      QA-A-139 widened this axis from the COUNT to the queue's identity: a
 *      test that drains one alert and pushes another kept the size unchanged
 *      and passed, while handing its successor a queue it never raised. Both
 *      are reported, because "2 left behind" and "2 replaced" send the reader
 *      to different lines. Measured cost of the identity read: see below.
 *
 * All four have side-effect-free getters. The calibration MAPS are not checked
 * here: there is no read-only query for them, and the only way to clear them is
 * shutdown, which is not this guard's to call -- a suite that loads maps once in
 * SetUpTestSuite (test_xpe_calib_endurance.cpp) legitimately holds them across
 * its own tests.
 *
 * HOW A FAILURE READS. The offenders are collected during the run and reported
 * once at the end, by the environment's TearDown. A listener cannot fail the
 * test it is observing without attributing the failure to the wrong place, and
 * the report is more useful as one list than as N scattered failures.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"

#include <cstring>
#include <string>
#include <vector>

namespace {

/** What the process looked like before any test ran. */
struct Baseline {
    XpeCalibrationMode mode{};
    XpeCalibQualityMeta meta{};
    bool initialized = false;
    int32_t alerts = 0;
    uint64_t alert_fp = 0;
    bool captured = false;
};

Baseline g_baseline;

/** Axes a test CAN restore with the public API -- these fail the run. */
std::vector<std::string> g_offenders;



/**
 * QA-A-139: the queue's IDENTITY, not just its size.
 *
 * The count alone admits a false negative: a test that drains one alert and
 * pushes another leaves the size unchanged while the queue's contents are
 * entirely different, and the next test's negative assertion then reads
 * someone else's alert. The count is kept as a separate field because the two
 * numbers answer different questions -- "how many were left" is what an
 * offender needs to hear, and a hash cannot say it.
 *
 * Read through the public getter, which api-spec.md 5.17 defines as leaving
 * the queue unmodified -- there is no test-only accessor here, and none was
 * added. Severity joins the message because two alerts can share text and
 * differ in how loudly they are reported.
 *
 * FNV-1a over (message bytes, severity) in queue order. Order matters: a queue
 * holding the same two alerts in the other order is a different queue, and
 * reordering is itself something a test should not do to its successor.
 */
uint64_t AlertFingerprint() {
    uint64_t h = 1469598103934665603ull;  // FNV-1a 64 offset basis
    const int32_t n = xpe_get_pending_alert_count();
    char msg[512];
    for (int32_t i = 0; i < n; ++i) {
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) != XPE_OK) {
            // A message longer than the buffer, or a racing drain. Fold the
            // index in so the difference is still visible rather than silently
            // hashing to the same value as a shorter queue.
            h = (h ^ static_cast<uint64_t>(0xFFu + i)) * 1099511628211ull;
            continue;
        }
        for (const char* c = msg; *c; ++c) {
            h = (h ^ static_cast<uint64_t>(static_cast<unsigned char>(*c))) *
                1099511628211ull;
        }
        h = (h ^ static_cast<uint64_t>(static_cast<uint32_t>(sev))) *
            1099511628211ull;
    }
    return h;
}

bool SameMeta(const XpeCalibQualityMeta& a, const XpeCalibQualityMeta& b) {
    return std::memcmp(&a, &b, sizeof(XpeCalibQualityMeta)) == 0;
}

class HygieneListener : public ::testing::EmptyTestEventListener {
public:
    /**
     * The baseline is taken per TEST, not once per program.
     *
     * Against a program-start baseline the guard names every test that runs
     * AFTER the state was dirtied, because nothing puts it back -- the first
     * run of this guard reported 158 tests for one actual cause. Comparing each
     * test against the state it inherited names only the test that changed it.
     */
    void OnTestStart(const ::testing::TestInfo&) override {
        g_baseline.mode = xpe_calib_get_mode();
        (void)xpe_calib_get_quality_meta(&g_baseline.meta);
        g_baseline.initialized = xpe_preprocess_is_initialized();
        g_baseline.alerts = xpe_get_pending_alert_count();
        g_baseline.alert_fp = AlertFingerprint();
        g_baseline.captured = true;
    }

    void OnTestEnd(const ::testing::TestInfo& info) override {
        if (!g_baseline.captured) return;
        // A test that already failed is not also reported for hygiene: its
        // teardown may not have run, and the first failure is the useful one.
        if (info.result() != nullptr && info.result()->Failed()) return;

        std::string why;
        if (xpe_preprocess_is_initialized() != g_baseline.initialized) {
            why += xpe_preprocess_is_initialized()
                       ? " module left initialized;"
                       : " module left shut down (it was up on entry);";
        }
        if (xpe_calib_get_mode() != g_baseline.mode) {
            why += " calibration mode changed;";
        }
        const std::string full =
            std::string(info.test_suite_name()) + "." + info.name();

        XpeCalibQualityMeta meta{};
        (void)xpe_calib_get_quality_meta(&meta);
        if (!SameMeta(meta, g_baseline.meta)) {
            why += " quality metadata changed;";
        }
        // QA-A-138. The count, not the text: a test that pushes and drains the
        // same number of alerts has left the queue as it found it, which is the
        // contract. Reported with both numbers because "3 left behind" sends the
        // reader to a different line than "the queue was drained under you".
        const int32_t alerts_now = xpe_get_pending_alert_count();
        const uint64_t fp_now = AlertFingerprint();
        if (alerts_now != g_baseline.alerts) {
            why += " pending alerts " + std::to_string(g_baseline.alerts) +
                   " -> " + std::to_string(alerts_now) +
                   " (drain with xpe_clear_alerts());";
        } else if (fp_now != g_baseline.alert_fp) {
            // QA-A-139: same size, different queue. The count cannot see this,
            // and it is the shape that reaches the NEXT test's assertion.
            why += " pending alert queue replaced (" +
                   std::to_string(alerts_now) +
                   " alerts, different contents; drain with xpe_clear_alerts());";
        }
        if (!why.empty()) {
            g_offenders.push_back(full + " --" + why);
        }
    }
};

/** Reports the collected offenders once, as a single failure. */
class HygieneEnvironment : public ::testing::Environment {
public:
    void TearDown() override {
        if (g_offenders.empty()) return;
        std::string msg =
            "Tests left global module state changed. Each of these has to undo\n"
            "what it did -- restore only what the fixture itself raised, in\n"
            "TearDown. Leaving state behind passes under ctest (one process per\n"
            "test) and fails whenever the binary is run directly or shuffled.\n";
        for (const auto& o : g_offenders) msg += "  " + o + "\n";
        ADD_FAILURE() << msg;
    }
};

const bool kRegistered = [] {
    ::testing::UnitTest::GetInstance()->listeners().Append(new HygieneListener);
    ::testing::AddGlobalTestEnvironment(new HygieneEnvironment);
    return true;
}();

}  // namespace

/**
 * The guard's own control: it must be able to SEE a dirty state, otherwise an
 * empty offender list means nothing. This test deliberately leaves the module
 * initialized, confirms the probe reports it, and then cleans up -- so it is
 * not itself an offender.
 */
TEST(GlobalStateHygieneTest, TheProbeDetectsAnInitializedModule) {
    ASSERT_FALSE(xpe_preprocess_is_initialized())
        << "a previous test left the module up; the offender list will name it";

    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    EXPECT_TRUE(xpe_preprocess_is_initialized());

    // A second init is refused by design -- this is the mechanism behind the
    // failure mode the guard exists to catch.
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_preprocess_init(nullptr));

    xpe_preprocess_shutdown();
    EXPECT_FALSE(xpe_preprocess_is_initialized());
}

/**
 * QA-A-138: the fourth axis's probe, on the same pattern as the two above --
 * raise it, confirm the getter sees it, put it back. An empty offender list
 * for this axis means nothing unless the probe can read a non-empty queue.
 */
TEST(GlobalStateHygieneTest, TheProbeDetectsAPendingAlert) {
    const int32_t before = xpe_get_pending_alert_count();

    xpe_alert_push("QA-A-138 hygiene probe", XPE_ALERT_INFO);
    EXPECT_GT(xpe_get_pending_alert_count(), before);

    // Restore through the product's own drain -- the same call the guard's
    // failure message tells an offender to make. Clearing empties the whole
    // queue, so this test asserts the axis is back to zero rather than to
    // `before`; it inherits an empty queue from its own predecessor's cleanup.
    xpe_clear_alerts();
    EXPECT_EQ(0, xpe_get_pending_alert_count());
}

/** The mode getter is the second axis, and it reads back what was set. */
TEST(GlobalStateHygieneTest, TheProbeDetectsAChangedCalibrationMode) {
    const XpeCalibrationMode before = xpe_calib_get_mode();

    ASSERT_EQ(XPE_OK, xpe_calib_set_mode(XPE_CALIB_SINGLE_POINT));
    EXPECT_EQ(XPE_CALIB_SINGLE_POINT, xpe_calib_get_mode());

    // Restore, which is exactly what every fixture that touches the mode owes.
    ASSERT_EQ(XPE_OK, xpe_calib_set_mode(before));
    EXPECT_EQ(before, xpe_calib_get_mode());
}
