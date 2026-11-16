/**
 * @file test_calib_mode.cpp
 * @brief Unit tests for Calibration Mode Selection API (FUNC-031~033)
 *
 * SPEC: SAD-CALIB-001 SWU-1.12 (FUNC-031~033)
 * IEC 62304 Class B
 *
 * Test Coverage:
 * - Default mode verification (SINGLE_POINT, the only mode since QA-A-336)
 * - Mode set/get: SINGLE_POINT accepted, the removed modes 1-5 refused
 * - Quality metadata initialization
 * (The mode-to-params mapping and the 10-point cap went with the multi-point modes, QA-A-336.)
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/* =============================================================================
 * Test Fixture
 * ============================================================================ */

class CalibModeTest : public ::testing::Test {
protected:
    void SetUp() override {
        xpe_calib_set_mode(XPE_CALIB_SINGLE_POINT);
    }

    void TearDown() override {
        // Leave the module in its default mode (QA-A-89, #176).
        xpe_calib_set_mode(XPE_CALIB_SINGLE_POINT);
    }
};

/* =============================================================================
 * FUNC-031: Mode Selection Tests
 * ============================================================================ */

/**
 * @test The default mode is SINGLE_POINT, the only mode (QA-A-336)
 */
TEST_F(CalibModeTest, DefaultModeIsSinglePoint) {
    EXPECT_EQ(xpe_calib_get_mode(), XPE_CALIB_SINGLE_POINT);
}

/**
 * @test SINGLE_POINT is accepted; the removed modes (1 DUAL_POINT .. 4 MULTI_POINT_10, 5 AUTO) and any other value are
 *       refused, and a refusal leaves the mode as it was
 */
TEST_F(CalibModeTest, OnlySinglePointIsAcceptedAndARefusalChangesNothing) {
    EXPECT_EQ(xpe_calib_set_mode(XPE_CALIB_SINGLE_POINT), XPE_OK);
    EXPECT_EQ(xpe_calib_get_mode(), XPE_CALIB_SINGLE_POINT);
    for (const int v : {1, 2, 3, 4, 5, 99, -1}) {
        EXPECT_EQ(xpe_calib_set_mode(static_cast<XpeCalibrationMode>(v)), XPE_ERR_INVALID_INPUT) << "mode " << v;
        EXPECT_EQ(xpe_calib_get_mode(), XPE_CALIB_SINGLE_POINT) << "mode " << v;
    }
}

/* =============================================================================
 * FUNC-033: Quality Metadata Tests
 * ============================================================================ */

/**
 * @test Quality metadata initial state (before calibration)
 *
 * The state under test only exists before any calibration has run IN THIS
 * PROCESS, and nothing in the public API returns the store to it. So the check
 * runs in a fresh child process (QA-A-89, #176): on Windows EXPECT_EXIT always
 * re-executes this binary for just this case (there is no fork), so the child's
 * DLL globals are untouched however many generators ran before it here. The
 * style flag is not set explicitly: gtest is linked as a DLL in this build and
 * does not export FLAGS_gtest_death_test_style (measured: LNK2001).
 *
 * Until QA-A-89 the check ran in-process and FAILED under the default order of
 * a direct run (GenerateGainTest fills the store earlier in the same process:
 * calibration_mode 3, polynomial_degree 2, num_points 3). ctest runs each case
 * in its own process, which is why CI never showed it.
 */
TEST_F(CalibModeTest, QualityMeta_InitialState) {
    EXPECT_EXIT(
        {
            XpeCalibQualityMeta meta;
            std::memset(&meta, 0xFF, sizeof(meta));  // Fill with invalid values
            const XpeErrorCode rc = xpe_calib_get_quality_meta(&meta);

            // Initial state: all zeros except previous_r_squared = -1.0
            const bool ok = rc == XPE_OK &&
                            meta.calibration_mode == 0 &&
                            meta.polynomial_degree == 0 &&
                            meta.num_points == 0 &&
                            meta.r_squared == 0.0 &&
                            meta.calibration_timestamp == 0 &&
                            meta.detector_serial[0] == '\0' &&
                            meta.firmware_version[0] == '\0' &&
                            meta.calibration_pass == 0 &&
                            meta.previous_r_squared == -1.0;
            std::fprintf(stderr,
                         "%s rc=%d mode=%d degree=%d points=%d r2=%g ts=%llu pass=%d prev=%g\n",
                         ok ? "QUALITY_META_INITIAL_OK" : "QUALITY_META_NOT_INITIAL",
                         static_cast<int>(rc),
                         static_cast<int>(meta.calibration_mode),
                         static_cast<int>(meta.polynomial_degree),
                         static_cast<int>(meta.num_points),
                         meta.r_squared,
                         static_cast<unsigned long long>(meta.calibration_timestamp),
                         static_cast<int>(meta.calibration_pass),
                         meta.previous_r_squared);
            std::exit(ok ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "QUALITY_META_INITIAL_OK");
}

/**
 * @test Quality metadata NULL pointer is rejected
 */
TEST_F(CalibModeTest, QualityMeta_NullPointer_Rejected) {
    EXPECT_EQ(xpe_calib_get_quality_meta(nullptr), XPE_ERR_INVALID_INPUT);
}

/* =============================================================================
 * FUNC-033: R² Quality Gate Tests — REMOVED (QA-A-34, #120)
 *
 * Four cases lived here: R2QualityGate_Pass_WhenAboveThreshold,
 * R2QualityGate_Fail_WhenBelowThreshold,
 * PreviousCalibration_Comparison_Regression and _Stable.
 *
 * None of them called production code. Each declared local doubles and compared
 * them to each other:
 *
 *     constexpr double r_squared_good = 0.9995;
 *     EXPECT_GE(r_squared_good, 0.999);   // a constant against a constant
 *
 * The `xpe_calib_get_quality_meta(&meta)` call above each comparison read the
 * metadata and then ignored it — the assertions never touched `meta`. Four
 * green rows named after a safety gate, testing arithmetic.
 *
 * The gate they claimed to cover lived in `xpe::calib::mode::update_metadata`,
 * which QA-A-34 deleted as dead code (zero callers, zero declarations). There is
 * now NO R² quality gate in this module to point them at, so they are removed
 * rather than rewritten. Should the gate return as a reachable API, its tests
 * belong here and must call it.
 * ============================================================================ */

/* =============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
