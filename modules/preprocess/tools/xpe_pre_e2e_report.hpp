#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <optional>
#include <sstream>
#include <string>
#include <nlohmann/json.hpp>

namespace xpe::pre::e2e {

inline const char* clampReviewVerdict(std::optional<double> fraction) {
    if (!fraction || !std::isfinite(*fraction) || *fraction < 0.0 || *fraction > 1.0) return "unmeasured";
    return *fraction > 0.05 ? "review_required" : "reported";
}

// Protocol section 6, in order: dark, flat, defect, nonlinearity,
// lag/ghost, reference, preservation, performance. An absent score is NOT zero.
struct ScoreEvidence {
    std::array<std::optional<double>, 8> components{};
    bool rawProof = false;
    bool inputPreserved = false;
    bool gainKnown = false;
    bool finiteOutput = false;
    bool blockingSafetyFailure = false;
};

struct ScoreSummary {
    std::optional<double> value;
    double cap = 100.0;
    bool complete = false;
    bool phase1Pass = false;
    bool releasePass = false;
};

inline ScoreSummary calculateScore(const ScoreEvidence& e) {
    ScoreSummary s;
    if (!e.rawProof || !e.gainKnown || !e.finiteOutput) s.cap = 70.0;
    if (e.blockingSafetyFailure || (e.rawProof && !e.inputPreserved)) s.cap = 60.0;
    constexpr std::array<double, 8> weights{0.18, 0.18, 0.14, 0.12, 0.12, 0.10, 0.08, 0.08};
    double total = 0.0;
    for (size_t i = 0; i < weights.size(); ++i) {
        if (!e.components[i] || !std::isfinite(*e.components[i])) return s;
        total += weights[i] * std::clamp(*e.components[i], 0.0, 1.0);
    }
    s.complete = true;
    s.value = std::min(s.cap, 100.0 * total);
    const bool safe = e.rawProof && e.inputPreserved && e.gainKnown && e.finiteOutput && !e.blockingSafetyFailure;
    s.phase1Pass = safe && *s.value >= 85.0;
    s.releasePass = safe && *s.value >= 92.0;
    return s;
}

inline nlohmann::json scoreJson(const ScoreSummary& s) {
    nlohmann::json j = {{"calibration_effect_score", nullptr}, {"score_cap", s.cap},
                        {"score_complete", s.complete}, {"phase1_pass", s.phase1Pass},
                        {"release_pass", s.releasePass}};
    if (s.value) j["calibration_effect_score"] = *s.value;
    return j;
}

inline std::string markdownCell(std::string text) {
    std::string result;
    for (const char c : text) {
        if (c == '|') result += "\\|";
        else if (c == '\n' || c == '\r') result += ' ';
        else result += c;
    }
    return result;
}

inline std::string markdown(const nlohmann::json& report) {
    std::ostringstream out;
    out << "# Preprocessing E2E evidence\n\nSchema: " << report.value("schema", "")
        << "\n\nImage: " << markdownCell(report.value("image_path", ""))
        << "\n\nEvidence: " << (report.value("degraded_evidence", true) ? "degraded / incomplete" : "complete")
        << "\n\nUnmeasured metrics are not zero measurements. Input kind is caller-selected, not acquisition-verified.\n"
        << "\n| Metric | Value |\n|---|---|\n";
    const auto metrics = report.value("metrics", nlohmann::json::object());
    for (auto it = metrics.begin(); it != metrics.end(); ++it)
        out << "| " << markdownCell(it.key()) << " | "
            << (it.value().is_null() ? "unmeasured" : markdownCell(it.value().dump())) << " |\n";
    out << "\n| Gate | Result |\n|---|---|\n";
    for (const auto& gate : report.value("gates", nlohmann::json::array()))
        out << "| " << markdownCell(gate.value("id", "")) << " | "
            << markdownCell(gate.value("result", "unmeasured")) << " |\n";
    if (report.contains("ghost")) {
        const auto& ghost = report.at("ghost");
        out << "\nGhost bypass: " << markdownCell(ghost.value("bypass_reason", "not_executed"))
            << ". " << markdownCell(ghost.value("scope", "unmeasured")) << ".\n";
    }
    return out.str();
}

} // namespace xpe::pre::e2e
