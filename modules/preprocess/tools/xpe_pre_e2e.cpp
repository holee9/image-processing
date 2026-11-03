/** @file xpe_pre_e2e.cpp
 * FUNC-015/019: basic-stage evidence and optional reference reporting (#245).
 */
#include "xpe_pre_e2e.hpp"
#include "xpe_pre_stripe.hpp"
#include "xpe_pre_defect_metrics.hpp"
#include "defect_ext.hpp"
#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "xpe_sha256.hpp"
#include "xcal_reader.hpp"
#include <charconv>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace xpe::pre::e2e {
namespace {
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;

std::string digestText(const std::array<uint8_t, 32>& bytes) {
    constexpr char hex[] = "0123456789abcdef";
    std::string text;
    text.reserve(64);
    for (const auto b : bytes) {
        text += hex[b >> 4];
        text += hex[b & 15];
    }
    return text;
}

std::string hashFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read input: " + path.u8string());
    Sha256Stream hash;
    std::array<char, 65536> chunk{};
    while (input.read(chunk.data(), static_cast<std::streamsize>(chunk.size())) || input.gcount() > 0)
        hash.update(reinterpret_cast<const uint8_t*>(chunk.data()), static_cast<size_t>(input.gcount()));
    if (input.bad()) throw std::runtime_error("read failed: " + path.u8string());
    return digestText(hash.digest());
}

XpeImageBuffer buffer(void* data, uint32_t width, uint32_t height, XpePixelFormat format, uint32_t bits) {
    XpeImageBuffer b{};
    b.data = data;
    b.width = width;
    b.height = height;
    b.format = format;
    b.bitsAllocated = b.bitsStored = static_cast<uint16_t>(bits);
    b.dataSize = static_cast<size_t>(width) * height * (bits / 8u);
    return b;
}

void requireOk(XpeErrorCode rc, const char* operation) {
    if (rc != XPE_OK) throw std::runtime_error(std::string(operation) + " returned " + std::to_string(rc));
}

struct Session {
    bool initialized = false;
    XpeCalibrationState maps{};
    ~Session() {
        if (!initialized) return;
        xpe_calib_state_release(&maps);
        xpe_clear_alerts();
        xpe_preprocess_shutdown();
    }
};

void gate(Json& report, const char* id, const char* result) {
    report["gates"].push_back({{"id", id}, {"result", result}});
}

double elapsedMs(const Clock::time_point& start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

Json measured(double value, bool valid) {
    return valid && std::isfinite(value) ? Json(value) : Json(nullptr);
}

Json optionalMetric(const std::optional<double>& value) {
    return value && std::isfinite(*value) ? Json(*value) : Json(nullptr);
}

void recordReference(const Options& o, const std::vector<float>& output, Json& report) {
    const auto bytes = output.size() * sizeof(float);
    if (std::filesystem::file_size(o.referenceF32) != bytes)
        throw std::runtime_error("reference must contain exactly width*height FLOAT32LE pixels");
    const auto originalHash = hashFile(o.referenceF32);
    auto& evidence = report["reference"];
    evidence = {{"path", o.referenceF32.u8string()}, {"pixel_format", "float32-le"},
        {"width", o.width}, {"height", o.height}, {"pixels", output.size()},
        {"size_bytes", bytes}, {"sha256_before", originalHash},
        {"source", "caller-selected reference; provenance and alignment not independently verified"},
        {"scope", "full-frame basic offset/gain/defect shipped pipeline output; same detector-domain ADU and alignment required; no registration or rescaling"},
        {"psnr_peak_adu", 65535}, {"psnr_status", "unmeasured"}, {"exact_match", nullptr}};
    std::ifstream input(o.referenceF32, std::ios::binary);
    if (!input) throw std::runtime_error("reference read failed");
    Sha256Stream consumed;
    std::array<float, 4096> chunk{};
    double absoluteSum = 0.0, squaredSum = 0.0, maxAbsolute = 0.0;
    size_t nonfinite = 0;
    for (size_t pos = 0; pos < output.size();) {
        const size_t count = std::min(chunk.size(), output.size() - pos);
        const auto byteCount = count * sizeof(float);
        input.read(reinterpret_cast<char*>(chunk.data()), static_cast<std::streamsize>(byteCount));
        if (!input) throw std::runtime_error("reference read failed");
        consumed.update(reinterpret_cast<const uint8_t*>(chunk.data()), byteCount);
        for (size_t j = 0; j < count; ++j) {
            if (!std::isfinite(output[pos + j]) || !std::isfinite(chunk[j])) { ++nonfinite; continue; }
            const double delta = static_cast<double>(output[pos + j]) - static_cast<double>(chunk[j]);
            absoluteSum += std::abs(delta);
            squaredSum += delta * delta;
            maxAbsolute = std::max(maxAbsolute, std::abs(delta));
        }
        pos += count;
    }
    if (input.peek() != std::char_traits<char>::eof() || input.bad())
        throw std::runtime_error("reference size changed or read failed");
    evidence["sha256_consumed"] = digestText(consumed.digest());
    evidence["nonfinite_comparison_pixels"] = nonfinite;
    if (evidence["sha256_consumed"] != originalHash)
        throw std::runtime_error("reference changed while being read");
    if (nonfinite) throw std::runtime_error("reference comparison contains nonfinite pixels; no subset metrics reported");
    const double rmse = std::sqrt(squaredSum / static_cast<double>(output.size()));
    report["metrics"]["mae_reference_adu"] = absoluteSum / static_cast<double>(output.size());
    report["metrics"]["rmse_reference"] = rmse;
    report["metrics"]["max_abs_reference_adu"] = maxAbsolute;
    evidence["exact_match"] = squaredSum == 0.0;
    if (rmse > 0.0) {
        report["metrics"]["psnr_reference_db"] = 20.0 * (std::log10(65535.0) - std::log10(rmse));
        evidence["psnr_status"] = "measured";
    } else {
        // Protocol epsilon is unspecified. Do not invent a finite perfect-match PSNR.
        evidence["psnr_status"] = "epsilon_unspecified_for_exact_match";
    }
    gate(report, "PRE-E2E-REFERENCE-METRICS", "reported");
    // No reference acceptance target or CES normalization is defined here.
}

void recordDefectOracle(const Options& o, const std::vector<float>& before,
                        const std::vector<float>& after, Json& report) {
    const size_t n = before.size();
    if (std::filesystem::file_size(o.oracleBpm) != n)
        throw std::runtime_error("oracle BPM must contain exactly width*height UINT8 bytes");
    const auto originalHash = hashFile(o.oracleBpm);
    std::vector<uint8_t> truth(n);
    {
        std::ifstream f(o.oracleBpm, std::ios::binary);
        f.read(reinterpret_cast<char*>(truth.data()), static_cast<std::streamsize>(n));
        if (!f) throw std::runtime_error("oracle BPM read failed");
    }
    if (digestText(compute_sha256(truth.data(), truth.size())) != originalHash)
        throw std::runtime_error("oracle BPM changed while reading");
    DefectXcal defect;
    requireOk(read_defect_xcal((o.calibration / "defect.xcal").string().c_str(), true, &defect),
              "read defect prediction");
    if (defect.width != o.width || defect.height != o.height || defect.map.size() != n ||
        (!defect.area.empty() && defect.area.size() != n))
        throw std::runtime_error("defect prediction geometry does not match raw frame");
    XCalFileHeader gainHeader{};
    std::vector<uint8_t> configBytes, gainPayload;
    requireOk(read_xcal_file((o.calibration / "gain.xcal").string().c_str(), gainHeader,
                            configBytes, gainPayload, true), "read gain prediction provenance");
    if (gainHeader.width != o.width || gainHeader.height != o.height)
        throw std::runtime_error("gain prediction geometry does not match raw frame");
    std::vector<uint8_t> prediction;
    const bool scalar = gainHeader.type == XCAL_TYPE_GAIN && gainHeader.pixel_format == XCAL_FMT_FLOAT32 &&
                        gainPayload.size() == n * sizeof(float);
    uint64_t outsideBad = 0;
    if (scalar) prediction.resize(n);
    for (size_t i = 0; i < n; ++i) {
        const bool inside = defect.area.empty() || defect.area[i] != 0;
        if (!inside && truth[i]) ++outsideBad;
        if (scalar) {
            float gain = 0.0f;
            std::memcpy(&gain, gainPayload.data() + i * sizeof(float), sizeof(float));
            prediction[i] = inside && (defect.map[i] != 0 || !xpe_gain_value_valid(gain)) ? 1u : 0u;
        }
    }
    const auto m = measureDefectOracle(before, after, truth, prediction, o.width, o.height);
    if (!m.valid) throw std::runtime_error("oracle metric frame geometry mismatch");
    report["oracle"] = {
        {"path", o.oracleBpm.u8string()}, {"format", "raw UINT8, nonzero=bad"},
        {"width", o.width}, {"height", o.height}, {"bytes", n},
        {"kind", o.oracleKind}, {"kind_source", "caller-selected, not acquisition-verified"},
        {"sha256_before", originalHash}, {"sha256_after", nullptr}, {"preserved", nullptr},
        {"truth_bad", m.truthBad}, {"truth_good", m.truthGood}, {"truth_bad_outside_area", outsideBad},
        {"tp", scalar ? Json(m.tp) : Json(nullptr)}, {"fp", scalar ? Json(m.fp) : Json(nullptr)},
        {"fn", scalar ? Json(m.fn) : Json(nullptr)}, {"tn", scalar ? Json(m.tn) : Json(nullptr)},
        {"prediction_measured", scalar}, {"defect_file_version", defect.format_version},
        {"prediction_scope", scalar ? "loaded scalar gain-invalid union defect mask, inside imaging area; selection accuracy, NOT repair success" :
                                       "polynomial per-frame prediction not measured; stage metrics only"},
        {"population_scope", "entire external truth, including truth-positive pixels outside imaging area"},
        {"stage_scope", "actual defect stage input and output, before stateless replay"},
        {"residual_model", "GUI-C-182: oracle-good 3x3 mean, excluding centre and oracle-bad; no neighbour excluded"},
        {"residual_pixels", m.residualPixels}, {"nonfinite_stage_pixels", m.nonfinitePixels},
        {"ratio_units", "recall and FPR are fractions, not percentages"},
        {"p99_scope", "native linear interpolation and GUI nearest-rank both reported; disagreement of <=1 ADU verdict is unmeasured"},
        {"p99_linear_verdict", m.p99Linear ? (*m.p99Linear <= 1.0 ? "pass" : "fail") : "unmeasured"},
        {"p99_nearest_verdict", m.p99Nearest ? (*m.p99Nearest <= 1.0 ? "pass" : "fail") : "unmeasured"}
    };
    report["metrics"]["defect_recall"] = optionalMetric(m.recall);
    report["metrics"]["defect_fpr"] = optionalMetric(m.fpr);
    report["metrics"]["defect_residual_adu"] = optionalMetric(m.residual);
    report["metrics"]["good_pixel_delta_p99_linear_adu"] = optionalMetric(m.p99Linear);
    report["metrics"]["good_pixel_delta_p99_nearest_adu"] = optionalMetric(m.p99Nearest);
    gate(report, "PRE-E2E-DEFECT-SELECTION", defectSelectionVerdict(m, o.oracleKind == "synthetic"));
    gate(report, "PRE-E2E-GOOD-PIXEL-CHANGE", goodPixelVerdict(m));
    // No invented residual acceptance threshold or combined normalized defect CES subscore.
}
} // namespace

Json evaluate(const Options& o) {
    Json report = {
        {"schema", "xpe-pre-e2e-report-v1"}, {"git_sha", o.gitSha},
        {"image_path", o.raw.u8string()}, {"width", o.width}, {"height", o.height},
        {"pixel_format", "uint16-le"}, {"gain_semantics", o.gainSemantics},
        {"input_kind", o.inputKind}, {"input_kind_source", "caller-selected, not acquisition-verified"},
        {"gain_semantics_source", "caller-selected, not independently verified"},
        {"evidence_scope", "basic offset/gain/defect stages, cross-checked against shipped pipeline_out"},
        {"raw_sha256_before", nullptr}, {"raw_sha256_after", nullptr}, {"input_preserved", false},
        {"input_preservation_status", "unmeasured"},
        {"calibration_paths", Json::array()}, {"calibration_files", Json::array()},
        {"stages", Json::array()}, {"gates", Json::array()},
        {"ghost_bypassed", nullptr},
        {"ghost", {{"bypass_reason", "not_executed"}, {"corrected_flag", nullptr},
                   {"tier", nullptr}, {"history_length", nullptr},
                   {"scope", "basic runner config bypass; no ghost handle, tier or history observed"}}},
        {"metrics", {{"dark_bias_adu", nullptr}, {"dsnu_adu", nullptr}, {"dark_reduction_db", nullptr},
                     {"dark_bias_signed_adu", nullptr}, {"clamp_rate", nullptr}, {"zero_output_rate", nullptr},
                     {"flat_residual_pct", nullptr}, {"invalid_gain_count", nullptr}, {"gain_coverage", nullptr},
                     {"fpn_reduction_db", nullptr}, {"defect_recall", nullptr}, {"defect_fpr", nullptr},
                     {"defect_residual_adu", nullptr}, {"good_pixel_delta_p99_linear_adu", nullptr},
                     {"good_pixel_delta_p99_nearest_adu", nullptr},
                     {"rmse_reference", nullptr}, {"psnr_reference_db", nullptr},
                     {"mae_reference_adu", nullptr}, {"max_abs_reference_adu", nullptr},
                     {"pipeline_time_ms", nullptr}, {"peak_memory_mb", nullptr},
                     {"nan_inf_count", nullptr}, {"determinism_rmse", nullptr}, {"calibration_effect_score", nullptr}}},
        {"degraded_evidence", true}, {"run_succeeded", false},
        {"phase1_pass", false}, {"release_pass", false}
    };
    ScoreEvidence scores;
    try {
        if (o.inputKind != "image" && o.inputKind != "dark" && o.inputKind != "flat")
            throw std::runtime_error("input kind must be image, dark or flat");
        if (o.gainSemantics != "normalized_gain" && o.gainSemantics != "reciprocal_gain" && o.gainSemantics != "unknown")
            throw std::runtime_error("invalid gain semantics");
        if (o.oracleKind != "reference" && o.oracleKind != "synthetic")
            throw std::runtime_error("oracle kind must be reference or synthetic");
        if (o.oracleBpm.empty() && o.oracleKind == "synthetic")
            throw std::runtime_error("synthetic oracle kind requires an independent oracle BPM");
        const uint16_t endian = 1;
        if (*reinterpret_cast<const uint8_t*>(&endian) != 1u)
            throw std::runtime_error("this evaluator requires a little-endian host");
        const uint64_t pixels = static_cast<uint64_t>(o.width) * o.height;
        if (!pixels || pixels > std::numeric_limits<uint32_t>::max() / sizeof(float))
            throw std::runtime_error("invalid or overflowing dimensions");
        if (std::filesystem::file_size(o.raw) != pixels * sizeof(uint16_t))
            throw std::runtime_error("raw byte count does not match explicit dimensions");
        const auto n = static_cast<size_t>(pixels);
        std::vector<uint16_t> raw(n);
        {
            std::ifstream input(o.raw, std::ios::binary);
            input.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(n * sizeof(uint16_t)));
            if (!input) throw std::runtime_error("raw input read failed");
        }
        const auto rawHash = digestText(compute_sha256(reinterpret_cast<const uint8_t*>(raw.data()), n * sizeof(uint16_t)));
        report["raw_sha256_before"] = rawHash;
        for (const char* name : {"offset.xcal", "gain.xcal", "defect.xcal"}) {
            const auto path = o.calibration / name;
            report["calibration_paths"].push_back(path.u8string());
            report["calibration_files"].push_back({{"path", path.u8string()}, {"sha256_before", hashFile(path)}});
        }
        Session session;
        requireOk(xpe_preprocess_init(nullptr), "init");
        session.initialized = true;
        requireOk(xpe_calib_state_load(&session.maps, o.calibration.string().c_str()), "calibration load");
        if (!session.maps.offsetLoaded || !session.maps.gainLoaded || !session.maps.defectLoaded)
            throw std::runtime_error("the basic evaluator requires all three calibration maps");
        gate(report, "PRE-E2E-CALIBRATION-LOAD", "pass");

        std::vector<uint16_t> offset(n);
        std::vector<float> gain(n), corrected(n), shipped(n);
        auto rawBuffer = buffer(raw.data(), o.width, o.height, XPE_PIXEL_UINT16, 16);
        auto offsetBuffer = buffer(offset.data(), o.width, o.height, XPE_PIXEL_UINT16, 16);
        auto gainBuffer = buffer(gain.data(), o.width, o.height, XPE_PIXEL_FLOAT32, 32);
        auto correctedBuffer = buffer(corrected.data(), o.width, o.height, XPE_PIXEL_FLOAT32, 32);
        auto shippedBuffer = buffer(shipped.data(), o.width, o.height, XPE_PIXEL_FLOAT32, 32);
        XpeImageMetadata meta{};
        const auto stage = [&](const char* name, const auto& operation) {
            const auto begin = Clock::now();
            const auto rc = operation();
            report["stages"].push_back({{"name", name}, {"enabled", "explicit"},
                {"applied", rc == XPE_OK}, {"time_ms", elapsedMs(begin)}, {"result_code", rc}});
            requireOk(rc, name);
        };
        stage("offset", [&] { return xpe_offset_correct(&rawBuffer, &offsetBuffer, &meta); });
        stage("gain", [&] { return xpe_gain_correct(&offsetBuffer, &gainBuffer, &meta); });
        stage("defect", [&] { return xpe_defect_correct(&gainBuffer, &correctedBuffer, &meta); });
        if (!o.oracleBpm.empty()) recordDefectOracle(o, gain, corrected, report);

        const char* config = "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassNonlinearity\":true,"
                             "\"bypassBinning\":true,\"bypassGhost\":true}";
        const auto begin = Clock::now();
        requireOk(xpe_preprocess_pipeline_out(&rawBuffer, &shippedBuffer, &meta, &session.maps, nullptr, config), "pipeline_out");
        report["ghost_bypassed"] = true;
        report["ghost"]["bypass_reason"] = "configured_bypass";
        report["ghost"]["corrected_flag"] = (meta.flags & XPE_FLAG_GHOST_CORRECTED) != 0;
        const double pipelineMs = elapsedMs(begin);
        report["metrics"]["pipeline_time_ms"] = pipelineMs;
        report["timing_scope"] = "stage times are separate stage API calls; pipeline time is a separate shipped pipeline_out call";
        const bool same = shippedBuffer.format == XPE_PIXEL_FLOAT32 &&
            std::memcmp(shipped.data(), corrected.data(), n * sizeof(float)) == 0;
        report["matches_shipped_pipeline"] = same;
        gate(report, "PRE-E2E-STAGE-PIPELINE-PARITY", same ? "pass" : "fail");
        scores.blockingSafetyFailure = !same;

        size_t nonfinite = 0;
        double min = std::numeric_limits<double>::infinity(), max = -min, sum = 0.0;
        for (const auto v : shipped) {
            if (!std::isfinite(v)) { ++nonfinite; continue; }
            min = std::min(min, static_cast<double>(v));
            max = std::max(max, static_cast<double>(v));
            sum += v;
        }
        scores.finiteOutput = nonfinite == 0;
        report["metrics"]["nan_inf_count"] = nonfinite;
        report["output_summary"] = {{"min", measured(min, nonfinite < n)}, {"max", measured(max, nonfinite < n)},
            {"mean", nonfinite < n ? Json(sum / static_cast<double>(n - nonfinite)) : Json(nullptr)}};
        gate(report, "PRE-E2E-NO-NAN-INF", scores.finiteOutput ? "pass" : "fail");
        if (!o.referenceF32.empty()) recordReference(o, shipped, report);

        // Protocol 5.7: repeat the same stateless CPU path, separately from stage-vs-pipeline parity.
        // Reuse the stage-output buffer after its parity check, without another full-frame allocation.
        XpeImageMetadata replayMeta{};
        const auto replayBegin = Clock::now();
        requireOk(xpe_preprocess_pipeline_out(&rawBuffer, &correctedBuffer, &replayMeta, &session.maps, nullptr, config), "determinism replay");
        const double replayMs = elapsedMs(replayBegin);
        double squaredError = 0.0;
        size_t comparisonNonfinite = 0;
        for (size_t i = 0; i < n; ++i) {
            if (!std::isfinite(shipped[i]) || !std::isfinite(corrected[i])) { ++comparisonNonfinite; continue; }
            const double difference = static_cast<double>(shipped[i]) - static_cast<double>(corrected[i]);
            squaredError += difference * difference;
        }
        const bool deterministic = comparisonNonfinite == 0 && squaredError == 0.0;
        report["metrics"]["determinism_rmse"] = measured(std::sqrt(squaredError / static_cast<double>(n)), comparisonNonfinite == 0);
        report["determinism"] = {{"pipeline_runs", 2}, {"replay_time_ms", replayMs},
            {"nonfinite_comparison_pixels", comparisonNonfinite},
            {"scope", "same raw/maps/config; basic stateless CPU pipeline_out; stateful ghost is bypassed"}};
        gate(report, "PRE-E2E-CPU-DETERMINISM", deterministic ? "pass" : "fail");
        scores.blockingSafetyFailure = scores.blockingSafetyFailure || !deterministic;

        if (o.inputKind == "dark") {
            XpeCalibrationMetrics m{};
            requireOk(xpe_verify_offset(&rawBuffer, &offsetBuffer, &meta, &m), "verify offset");
            const bool bias = (m.measured_mask & XPE_METRIC_DARK_BIAS) != 0 && std::isfinite(m.dark_bias);
            const bool reduction = (m.measured_mask & XPE_METRIC_DARK_REDUCTION) != 0 && std::isfinite(m.dark_reduction_db);
            report["metrics"]["dark_bias_adu"] = measured(m.dark_bias, bias);
            report["metrics"]["dsnu_adu"] = measured(m.dsnu_adu, bias);
            report["metrics"]["dark_reduction_db"] = measured(m.dark_reduction_db, reduction);
            report["offset_measured_mask"] = m.measured_mask;
            gate(report, "PRE-E2E-OFFSET-QUALITY", !bias ? "unmeasured" : (m.overall_pass ? "pass" : "fail"));
            if (bias) {
                const double biasScore = m.dark_bias == 0.0 ? 1.0 : std::min(1.0, 5.0 / std::abs(m.dark_bias));
                const double reductionScore = reduction ? std::clamp(m.dark_reduction_db / 10.0, 0.0, 1.0) : 0.0;
                scores.components[0] = std::max(biasScore, reductionScore);
            }
            XpeOffsetQuality q{};
            q.structSize = sizeof(q);
            requireOk(xpe_verify_offset_signed_file(&rawBuffer, (o.calibration / "offset.xcal").string().c_str(), &q), "signed offset quality");
            report["signed_offset_valid_mask"] = q.validMask;
            report["signed_offset_roi_pixels"] = q.roiPixels;
            report["metrics"]["dark_bias_signed_adu"] = measured(q.darkBiasSigned, (q.validMask & XPE_OFFSETQ_ROI_SIGNED) != 0);
            const bool clamping = (q.validMask & XPE_OFFSETQ_CLAMP_RATE) != 0;
            report["metrics"]["clamp_rate"] = measured(q.clampRatePct / 100.0, clamping);
            report["metrics"]["zero_output_rate"] = measured(q.zeroOutputRatePct / 100.0, clamping);
            const auto clampVerdict = clampReviewVerdict(clamping ? std::optional<double>(q.clampRatePct / 100.0) : std::nullopt);
            gate(report, "PRE-E2E-CLAMP-REVIEW", clampVerdict);
            report["clamp_review_required"] = std::strcmp(clampVerdict, "review_required") == 0;
            report["clamp_review_scope"] = "protocol 5.2: report/review above 5%; this is not a new signed-bias acceptance limit";
            // This extra signed measurement has no quality gate: DOC-06 / QA-A-266 contract.
        } else if (o.inputKind == "flat") {
            const auto semantics = o.gainSemantics == "normalized_gain" ? XPE_GAIN_SEMANTICS_NORMALIZED :
                o.gainSemantics == "reciprocal_gain" ? XPE_GAIN_SEMANTICS_RECIPROCAL : XPE_GAIN_SEMANTICS_UNKNOWN;
            XpeCalibrationMetrics m{};
            // XpeCalibrationState carries load flags, not map views (#117).
            // Read the original scalar file: a cached/global view has already
            // replaced out-of-range gain pixels and would hide invalid coverage.
            XCalFileHeader header{};
            std::vector<uint8_t> configBytes, payload;
            requireOk(read_xcal_file((o.calibration / "gain.xcal").string().c_str(), header, configBytes,
                                    payload, true, XCAL_TYPE_GAIN), "read scalar gain for verification");
            if (header.width != o.width || header.height != o.height || header.pixel_format != XCAL_FMT_FLOAT32 ||
                payload.size() != n * sizeof(float)) throw std::runtime_error("scalar gain layout does not match raw frame");
            std::vector<float> originalGain(n);
            std::memcpy(originalGain.data(), payload.data(), payload.size());
            auto gainMap = buffer(originalGain.data(), o.width, o.height, XPE_PIXEL_FLOAT32, 32);
            requireOk(xpe_verify_gain(&offsetBuffer, &gainBuffer, &gainMap, semantics, &m), "verify gain");
            const bool prnu = (m.measured_mask & XPE_METRIC_PRNU) != 0 && std::isfinite(m.prnu_after);
            report["metrics"]["gain_stage_flat_residual_pct"] = measured(m.prnu_after, prnu);
            report["metrics"]["gain_stage_prnu_improvement_db"] = measured(m.snr_improvement_db, (m.measured_mask & XPE_METRIC_SNR) != 0);
            // Protocol 5.3: use raw and final output on the same positive-gain ROI.
            // Gain application validity is a separate, stricter contract.
            size_t roiCount = 0, applicationInvalid = 0;
            double rawMean = 0.0, finalMean = 0.0, rawM2 = 0.0, finalM2 = 0.0;
            for (size_t i = 0; i < n; ++i) {
                if (!xpe_gain_value_valid(originalGain[i])) ++applicationInvalid;
                if (!std::isfinite(originalGain[i]) || originalGain[i] <= 0.0f) continue;
                if (!std::isfinite(shipped[i])) continue;
                ++roiCount;
                const double count = static_cast<double>(roiCount);
                const double dr = static_cast<double>(raw[i]) - rawMean;
                const double dy = static_cast<double>(shipped[i]) - finalMean;
                rawMean += dr / count;
                finalMean += dy / count;
                rawM2 += dr * (static_cast<double>(raw[i]) - rawMean);
                finalM2 += dy * (static_cast<double>(shipped[i]) - finalMean);
            }
            const bool stats = roiCount > 1 && nonfinite == 0;
            const double denominator = roiCount > 1 ? static_cast<double>(roiCount - 1) : 1.0;
            const double rawStd = std::sqrt(std::max(0.0, rawM2 / denominator));
            const double finalStd = std::sqrt(std::max(0.0, finalM2 / denominator));
            report["metrics"]["flat_residual_pct"] = measured(100.0 * finalStd / std::max(finalMean, 1e-9), stats && finalMean > 0.0);
            report["metrics"]["fpn_reduction_db"] = measured(20.0 * std::log10(std::max(rawStd, 1e-9) / std::max(finalStd, 1e-9)), stats && rawStd > 0.0);
            report["flat_statistics"] = {{"input", "raw"}, {"output", "final pipeline_out"},
                {"roi", "original gain finite and positive; no spatial exclusion"},
                {"count", roiCount}, {"std_convention", "sample N-1"}, {"epsilon", 1e-9},
                {"minimum_signal_met", stats && finalMean >= 2000.0}};
            report["metrics"]["gain_application_invalid_count"] = applicationInvalid;
            report["metrics"]["gain_application_coverage"] = static_cast<double>(n - applicationInvalid) / static_cast<double>(n);
            report["gain_validity_definitions"] = {{"verification_roi", "finite and > 0"},
                {"application", "finite and 0.1 <= gain <= 10 (shared xpe_gain_value_valid)"}};
            report["gain_measured_mask"] = m.measured_mask;
            const bool coverage = (m.measured_mask & XPE_METRIC_GAIN_COVERAGE) != 0;
            report["metrics"]["invalid_gain_count"] = coverage ? Json(m.invalid_gain_count) : Json(nullptr);
            report["metrics"]["gain_coverage"] = measured(m.gain_coverage, coverage);
            gate(report, "PRE-E2E-GAIN-STAGE-QUALITY", !prnu ? "unmeasured" : (m.overall_pass ? "pass" : "fail"));
            const auto stripe = measureStripe(shipped, originalGain, o.width, o.height);
            report["metrics"]["stripe_noise_ratio"] = stripe.ratio ? Json(*stripe.ratio) : Json(nullptr);
            report["stripe_statistics"] = {{"row_std", stripe.rowStd}, {"column_std", stripe.columnStd},
                {"row_expected", stripe.rowExpected}, {"column_expected", stripe.columnExpected},
                {"sigma_horizontal", stripe.sigmaHorizontal}, {"sigma_vertical", stripe.sigmaVertical},
                {"roi_pixels", stripe.roiPixels}, {"rows", stripe.rows}, {"columns", stripe.columns},
                {"horizontal_pairs", stripe.horizontalPairs}, {"vertical_pairs", stripe.verticalPairs},
                {"high_pass_half_window", 16}, {"std_convention", "population N"},
                {"undefined_ratio", "unmeasured; zero expected noise is not a pass"}};
            gate(report, "PRE-E2E-STRIPE-QUALITY", !stripe.ratio ? "unmeasured" : (*stripe.ratio <= 3.0 ? "pass" : "fail"));
            const auto flatVerdict = [&](double limit) {
                const auto& residual = report["metrics"]["flat_residual_pct"];
                const auto& fpn = report["metrics"]["fpn_reduction_db"];
                if (stripe.ratio && *stripe.ratio > 3.0) return "fail";
                if (stats && rawStd > 0.0 && !fpn.is_null() && fpn.get<double>() < 10.0) return "fail";
                if (stats && finalMean >= 2000.0 && !residual.is_null() && residual.get<double>() > limit) return "fail";
                if (!stripe.ratio || !stats || finalMean < 2000.0 || residual.is_null() ||
                    (rawStd > 0.0 && fpn.is_null())) return "unmeasured";
                return "pass";
            };
            gate(report, "PRE-E2E-FLAT-QUALITY", flatVerdict(1.0));
            gate(report, "PRE-E2E-FLAT-RELEASE-QUALITY", flatVerdict(0.5));
            // No invented rule combines the three flat criteria into a normalized CES component.
        }
        report["raw_sha256_after"] = hashFile(o.raw);
        const auto memoryAfter = digestText(compute_sha256(reinterpret_cast<const uint8_t*>(raw.data()), n * sizeof(uint16_t)));
        const bool preserved = rawHash == report["raw_sha256_after"].get<std::string>() && rawHash == memoryAfter;
        report["input_buffer_preserved"] = rawHash == memoryAfter;
        report["input_preserved"] = preserved;
        report["input_preservation_status"] = preserved ? "pass" : "fail";
        scores.rawProof = true;
        scores.inputPreserved = preserved;
        scores.components[6] = preserved ? 1.0 : 0.0;
        scores.gainKnown = o.gainSemantics != "unknown";
        gate(report, "PRE-E2E-RAW-PRESERVE", preserved ? "pass" : "fail");
        gate(report, "PRE-E2E-GAIN-SEMANTICS", scores.gainKnown ? "pass" : "unmeasured");
        bool mapsUnchanged = true;
        for (auto& file : report["calibration_files"]) {
            file["sha256_after"] = hashFile(std::filesystem::u8path(file.at("path").get<std::string>()));
            file["unchanged"] = file["sha256_before"] == file["sha256_after"];
            mapsUnchanged = mapsUnchanged && file["unchanged"].get<bool>();
        }
        gate(report, "PRE-E2E-CALIBRATION-PRESERVE", mapsUnchanged ? "pass" : "fail");
        scores.blockingSafetyFailure = scores.blockingSafetyFailure || !mapsUnchanged;
        bool oraclePreserved = true;
        if (!o.oracleBpm.empty()) {
            report["oracle"]["sha256_after"] = hashFile(o.oracleBpm);
            oraclePreserved = report["oracle"]["sha256_before"] == report["oracle"]["sha256_after"];
            report["oracle"]["preserved"] = oraclePreserved;
            gate(report, "PRE-E2E-ORACLE-PRESERVE", oraclePreserved ? "pass" : "fail");
            scores.blockingSafetyFailure = scores.blockingSafetyFailure || !oraclePreserved;
        }
        bool referencePreserved = true;
        if (!o.referenceF32.empty()) {
            report["reference"]["sha256_after"] = hashFile(o.referenceF32);
            referencePreserved = report["reference"]["sha256_before"] == report["reference"]["sha256_after"];
            report["reference"]["preserved"] = referencePreserved;
            gate(report, "PRE-E2E-REFERENCE-PRESERVE", referencePreserved ? "pass" : "fail");
            scores.blockingSafetyFailure = scores.blockingSafetyFailure || !referencePreserved;
        }
        report["run_succeeded"] = same && deterministic && scores.finiteOutput && preserved && mapsUnchanged && oraclePreserved && referencePreserved;
        // No fabricated performance subscore: the SRS also requires peak memory,
        // which this evidence-buffer-heavy tool does not isolate from the pipeline.
    } catch (const std::exception& e) {
        report["error"] = e.what();
        scores.blockingSafetyFailure = true;
        gate(report, "PRE-E2E-EXECUTION", "fail");
    }
    const auto summary = calculateScore(scores);
    report["metrics"]["calibration_effect_score"] = summary.value ? Json(*summary.value) : Json(nullptr);
    report["score"] = scoreJson(summary);
    report["phase1_pass"] = summary.phase1Pass;
    report["release_pass"] = summary.releasePass;
    const char* componentNames[] = {"dark", "flat", "defect", "nonlinearity", "lag_ghost", "reference", "preservation", "performance"};
    for (size_t i = 0; i < scores.components.size(); ++i)
        report["score_components"][componentNames[i]] = scores.components[i] ? Json(*scores.components[i]) : Json(nullptr);
    gate(report, "PRE-E2E-COMPLETE", summary.complete ? "pass" : "unmeasured");
    report["degraded_evidence"] = !summary.complete || !report["run_succeeded"].get<bool>() || !scores.gainKnown;
    return report;
}

bool writeReports(const std::filesystem::path& directory, const Json& report) {
    // Exclusive directory creation: existing evidence and source directories are never overwritten.
    // report.json is published last, only after both complete streams have closed.
    try {
        if (!std::filesystem::create_directory(directory)) return false;
        const auto jsonPending = directory / "report.json.pending";
        const auto mdPending = directory / "report.md.pending";
        {
            std::ofstream jsonOut(jsonPending, std::ios::binary), mdOut(mdPending, std::ios::binary);
            jsonOut << report.dump(2) << '\n';
            mdOut << markdown(report);
            jsonOut.close();
            mdOut.close();
            if (!jsonOut || !mdOut) return false;
        }
        std::filesystem::rename(mdPending, directory / "report.md");
        std::filesystem::rename(jsonPending, directory / "report.json");
        return true;
    } catch (const std::exception&) {
        return false; // a partial directory has no published report.json
    }
}
} // namespace xpe::pre::e2e

#ifdef XPE_PRE_E2E_STANDALONE
int main(int argc, char** argv) {
    using namespace xpe::pre::e2e;
    const auto help = [] {
        std::cout << "xpe_pre_e2e --raw FILE --calib DIR --width N --height N --out NEW_DIR\n"
                     "             [--input-kind image|dark|flat] [--gain-semantics unknown|normalized_gain|reciprocal_gain]\n"
                     "             [--git-sha SHA]\n"
                     "             [--oracle-bpm UINT8_FILE] [--oracle-kind reference|synthetic]\n"
                     "             [--reference-f32 FLOAT32LE_FILE]\n"
                     "Reference must match the basic output's detector-domain ADU, geometry and alignment (caller assertion).\n"
                     "Runs basic offset/gain/defect stages and cross-checks the shipped pipeline. No input files are changed.\n"
                     "Input kind and gain semantics are caller assertions. Missing measurements are null, never passes.\n"
                     "Exit 0: report written and execution succeeded; NOT full quality/promotion approval. Read all report gates.\n"
                     "Exit 2: arguments; 3: execution failed (report written); 4: report output refused/failed.\n";
    };
    try {
        Options o;
        std::filesystem::path out;
        for (int i = 1; i < argc; ++i) {
            const std::string key = argv[i];
            if (key == "--help") { help(); return 0; }
            if (++i == argc) { help(); return 2; }
            const std::string value = argv[i];
            if (key == "--raw") o.raw = value;
            else if (key == "--calib") o.calibration = value;
            else if (key == "--out") out = value;
            else if (key == "--input-kind") o.inputKind = value;
            else if (key == "--gain-semantics") o.gainSemantics = value;
            else if (key == "--git-sha") o.gitSha = value;
            else if (key == "--oracle-bpm") o.oracleBpm = value;
            else if (key == "--oracle-kind") o.oracleKind = value;
            else if (key == "--reference-f32") o.referenceF32 = value;
            else if (key == "--width" || key == "--height") {
                uint32_t n = 0;
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), n);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || !n) return 2;
                if (key == "--width") o.width = n; else o.height = n;
            } else { help(); return 2; }
        }
        if (o.raw.empty() || o.calibration.empty() || out.empty() || !o.width || !o.height) { help(); return 2; }
        if (std::filesystem::exists(out)) { std::cerr << "output must be a new directory\n"; return 4; }
        const auto report = evaluate(o);
        if (!writeReports(out, report)) { std::cerr << "report was not published\n"; return 4; }
        std::cout << "Report: " << (out / "report.json").string()
                  << "\nReadiness: incomplete (see unmeasured components and every gate)\n";
        return report.at("run_succeeded").get<bool>() ? 0 : 3;
    } catch (const std::exception& e) {
        std::cerr << "evaluator failed: " << e.what() << '\n';
        return 3;
    }
}
#endif
