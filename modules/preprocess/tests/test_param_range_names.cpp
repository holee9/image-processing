/**
 * @file test_param_range_names.cpp
 * @brief Every named preprocess parameter has a queryable range (QA-A-27, #112)
 *
 * Ported from the orphan tree `tests/preprocess_smoke/` (deleted by QA-A-27).
 * That tree was never in any build -- it included a header that no longer
 * exists (`xpe/preprocess/xpe_preprocess_api.h`) -- but it held one assertion
 * the live suites did not: it queried each of the six named parameters
 * individually, where `test_xpe_preprocess.cpp:1577` covers a single valid
 * lookup plus the invalid-argument cases.
 *
 * The six names are the table in `preprocess.cpp:20-25`. A rename that misses
 * this table's consumers shows up here rather than in whichever caller happens
 * to use the renamed parameter.
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"

namespace {

class ParamRangeNamesTest : public ::testing::Test {
protected:
    void SetUp() override    { ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr)); }
    void TearDown() override { xpe_preprocess_shutdown(); }
};

TEST_F(ParamRangeNamesTest, EveryNamedParameterReturnsAnOrderedRange) {
    // preprocess.cpp:20-25 -- the shipped table, in its declared order.
    const char* names[] = {
        "integration_time_ms",
        "temperature_c",
        "kVp",
        "mAs",
        "SID_mm",
        "pixelPitch_mm",
    };

    for (const char* name : names) {
        float minValue = 0.0f, maxValue = 0.0f;
        EXPECT_EQ(XPE_OK, xpe_preprocess_get_param_range(name, &minValue, &maxValue))
            << "parameter '" << name << "' must be queryable";
        EXPECT_LT(minValue, maxValue)
            << "parameter '" << name << "' range must be ordered";
    }
}

TEST_F(ParamRangeNamesTest, UnknownParameterIsRejected) {
    float minValue = 0.0f, maxValue = 0.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_preprocess_get_param_range("unknown_param", &minValue, &maxValue));
}

} // namespace
