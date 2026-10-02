/**
 * @file ghost_legacy_lag.h
 * @brief The lag parameters the ghost corrector used to fall back to, written out explicitly (QA-A-226, #241).
 *
 * Until QA-A-226 a ghost handle created without lag parameters corrected with built-in defaults (alpha1 0.9,
 * tau1 1, alpha2 0.05, tau2 20). They are not a calibration (QA-A-225: the implied forward system has a gain of
 * about 2.45, a constant exposure is "corrected" to nothing), so such a handle now passes frames through. The tests
 * that pin the correction FORMULAS (tiers, golden values, the pipeline's ghost stage) need a correcting handle; they
 * get these four values by configuration, which is how a real caller gets a calibration.
 */
#pragma once

#include <string>

/** `cfg` (a JSON object text, "" or nullptr = "{}") with each of alpha1/tau1/alpha2/tau2 added if it is not
 *  already there. A key the caller gave is left alone, so a test that overrides one still overrides it. */
inline std::string withLegacyLag(const char* cfg = nullptr) {
    std::string s = (cfg && *cfg) ? cfg : "{}";
    const struct { const char* key; const char* value; } lag[] = {
        {"alpha1", "0.9"}, {"tau1", "1"}, {"alpha2", "0.05"}, {"tau2", "20"},
    };
    for (const auto& k : lag) {
        if (s.find(std::string("\"") + k.key + "\"") != std::string::npos) continue;
        const size_t close = s.rfind('}');
        const bool empty = s.find_first_not_of(" \t\r\n{", 0) >= close;   // nothing but braces/space before the last }
        s.insert(close, std::string(empty ? "" : ",") + "\"" + k.key + "\":" + k.value);
    }
    return s;
}
