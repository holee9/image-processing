#pragma once

#include "xpe_pre_e2e_report.hpp"
#include <cstdint>
#include <filesystem>

namespace xpe::pre::e2e {
struct Options {
    std::filesystem::path raw;
    std::filesystem::path calibration;
    uint32_t width = 0;
    uint32_t height = 0;
    std::string inputKind = "image";
    std::string gainSemantics = "unknown";
    std::string gitSha = "unknown";
    std::filesystem::path oracleBpm;
    std::string oracleKind = "reference";
    std::filesystem::path referenceF32;
};

nlohmann::json evaluate(const Options& options);
bool writeReports(const std::filesystem::path& directory, const nlohmann::json& report);
} // namespace xpe::pre::e2e
