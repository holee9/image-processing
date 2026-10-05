/**
 * @file calib_expiry_check.cpp
 * @brief The expiry check of a calibration snapshot (SRS-CALIB-SAFE-002 / FUNC-009; QA-A-241, Codex #157).
 *
 * A pure function of a snapshot and a time: no global state, so the unit tests compile this file themselves (like helpers.cpp) and
 * call it with the clock at the exact boundary, which the system clock cannot give them. The pipeline entry points and the three
 * single-stage functions all call it; see the declaration in xpe_preprocess_internal.h.
 */
#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <chrono>
#include <cstdint>
#include <cstdio>

XpeErrorCode xpe_calib_snapshot_expiry_check_at(const CalibSnapshot& calib, unsigned maps, int64_t nowMs) noexcept
{
    const struct { unsigned bit; const char* name; bool loaded; int64_t expiry; } list[] = {
        {XPE_EXPIRY_OFFSET, "offset", calib.offset_map != nullptr, calib.offset_expiry_ms},
        {XPE_EXPIRY_GAIN, "gain", calib.gain_map != nullptr || calib.gain_poly_coeffs != nullptr, calib.gain_expiry_ms},
        {XPE_EXPIRY_DEFECT, "defect", calib.defect_map != nullptr, calib.defect_expiry_ms},
    };
    for (const auto& m : list) {
        if ((maps & m.bit) == 0 || !m.loaded || m.expiry == 0 || nowMs <= m.expiry) continue;
        try {
            char msg[240];
            std::snprintf(msg, sizeof(msg),
                "XPE_ERR_CALIBRATION_EXPIRED: the loaded %s calibration expired %lld ms ago; the frame was not processed",
                m.name, static_cast<long long>(nowMs - m.expiry));
            msg[sizeof(msg) - 1] = 0;
            xpe_alert_push(msg, XPE_ALERT_ERROR);
        } catch (...) {
            // [no-throw-boundary] advisory: the refusal does not depend on the alert
        }
        return XPE_ERR_CALIBRATION_EXPIRED;
    }
    return XPE_OK;
}

#ifdef XPE_CACHE_TEST_HOOKS
int64_t (*xpe_clock_now_ms_hook)() = nullptr;
#endif

XpeErrorCode xpe_calib_snapshot_expiry_check(const CalibSnapshot& calib, unsigned maps) noexcept
{
    return xpe_calib_snapshot_expiry_check_at(calib, maps, xpe_calib_now_ms());
}
