/**
 * @file ghost_stable_lag.h
 * @brief A calibrated lag set for tests that need a correcting ghost handle (QA-A-226, QA-A-226b, #241).
 *
 * A ghost handle corrects only when its configuration gives all four of alpha1, tau1, alpha2 and tau2 (QA-A-226), and
 * a set whose steady-state gain S = alpha1/(1-e^(-1/tau1)) + alpha2/(1-e^(-1/tau2)) is >= 1 is refused (QA-A-226b).
 * The built-in values the corrector used to fall back to (0.9 / 1 / 0.05 / 20, S = 2.449) are such a set, so the tests
 * that pin the correction FORMULAS (tiers, golden values, the pipeline's ghost stage) take these instead:
 * alpha1 0.1, tau1 1, alpha2 0.01, tau2 20 -- S = 0.158 + 0.205 = 0.363, and S times the tier 2 weight (at most 1.5 at
 * a frame mean of 32768) stays below 1, so every tier corrects at the signal levels the tests use.
 */
#pragma once

#include <string>

/** `cfg` (a JSON object text, "" or nullptr = "{}") with each of alpha1/tau1/alpha2/tau2 added if it is not
 *  already there. A key the caller gave is left alone, so a test that overrides one still overrides it. */
inline std::string withStableLag(const char* cfg = nullptr) {
    std::string s = (cfg && *cfg) ? cfg : "{}";
    const struct { const char* key; const char* value; } lag[] = {
        {"alpha1", "0.1"}, {"tau1", "1"}, {"alpha2", "0.01"}, {"tau2", "20"},
    };
    for (const auto& k : lag) {
        if (s.find(std::string("\"") + k.key + "\"") != std::string::npos) continue;
        const size_t close = s.rfind('}');
        const bool empty = s.find_first_not_of(" \t\r\n{", 0) >= close;   // nothing but braces/space before the last }
        s.insert(close, std::string(empty ? "" : ",") + "\"" + k.key + "\":" + k.value);
    }
    return s;
}
