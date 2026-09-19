/**
 * @file preprocess_state_fixture.h
 * @brief Base fixture that restores only what it raised -- QA-A-113 (#176)
 *
 * THE CONVENTION, IN ONE SENTENCE: a test restores the global state it changed,
 * and nothing else.
 *
 * "Nothing else" is the part that is easy to get wrong. A fixture that calls
 * xpe_preprocess_shutdown() unconditionally in TearDown will shut down a module
 * that was already up when the test started -- which is just as much a change to
 * global state as leaving one up, and the hygiene guard
 * (test_global_state_hygiene.cpp) reports it as one.
 *
 * xpe_preprocess_init() is not idempotent: a second call while the module is up
 * returns XPE_ERR_INVALID_INPUT by design. So the fixture records whether ITS
 * init was the one that brought the module up, and tears down only then.
 *
 * Use it by inheriting instead of ::testing::Test. A derived fixture that needs
 * its own SetUp/TearDown calls this one first and last:
 *
 *     class MyTest : public XpePreprocessStateFixture {
 *     protected:
 *         void SetUp() override {
 *             XpePreprocessStateFixture::SetUp();
 *             ... own setup ...
 *         }
 *         void TearDown() override {
 *             ... own teardown ...
 *             XpePreprocessStateFixture::TearDown();
 *         }
 *     };
 *
 * NOT covered here: the calibration quality metadata. No public call restores
 * it (see the guard's comment), so no fixture can, however it is written.
 */

#ifndef XPE_PREPROCESS_STATE_FIXTURE_H
#define XPE_PREPROCESS_STATE_FIXTURE_H

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"

class XpePreprocessStateFixture : public ::testing::Test {
protected:
    void SetUp() override {
        mode_on_entry_ = xpe_calib_get_mode();
        initialized_here_ = (xpe_preprocess_init(nullptr) == XPE_OK);
    }

    void TearDown() override {
        if (xpe_calib_get_mode() != mode_on_entry_) {
            (void)xpe_calib_set_mode(mode_on_entry_);
        }
        if (initialized_here_) {
            xpe_preprocess_shutdown();
            initialized_here_ = false;
        }
    }

    /** True when THIS fixture's init was the one that brought the module up. */
    bool initialized_here_ = false;
    XpeCalibrationMode mode_on_entry_{};
};

#endif /* XPE_PREPROCESS_STATE_FIXTURE_H */
