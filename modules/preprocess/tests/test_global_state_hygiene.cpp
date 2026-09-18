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
 * WHAT IS CHECKED, AND WHY EXACTLY THESE THREE.
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
 * All three have side-effect-free getters. The calibration MAPS are not checked
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
    bool captured = false;
};

Baseline g_baseline;

/** Axes a test CAN restore with the public API -- these fail the run. */
std::vector<std::string> g_offenders;



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

/** The mode getter is the second axis, and it reads back what was set. */
TEST(GlobalStateHygieneTest, TheProbeDetectsAChangedCalibrationMode) {
    const XpeCalibrationMode before = xpe_calib_get_mode();

    ASSERT_EQ(XPE_OK, xpe_calib_set_mode(XPE_CALIB_SINGLE_POINT));
    EXPECT_EQ(XPE_CALIB_SINGLE_POINT, xpe_calib_get_mode());

    // Restore, which is exactly what every fixture that touches the mode owes.
    ASSERT_EQ(XPE_OK, xpe_calib_set_mode(before));
    EXPECT_EQ(before, xpe_calib_get_mode());
}
