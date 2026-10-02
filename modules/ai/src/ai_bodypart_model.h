/**
 * @file ai_bodypart_model.h
 * @brief The body-part model and the sidecar that goes with it, loaded ONE way for both paths (QA-B-191 M4b, #130).
 *
 * The in-process path (ai.cpp) and the worker (ai_worker_main.cpp) both run the model, so both must agree on what
 * a usable model is: which files, which labels, which input shape. This is that one definition. A model that is
 * "usable" in one process and "unavailable" in the other would make the answer depend on the use_worker switch.
 *
 * THE MODELS THE TESTS USE ARE NOT CLASSIFIERS. What this wires and tests is the path, not how well any model
 * recognises a body part.
 */
#ifndef XPE_AI_BODYPART_MODEL_H
#define XPE_AI_BODYPART_MODEL_H

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "xpe/ai/ai_onnx_session.h"
#include "xpe/ai/ai_worker_protocol.h"
#include "ai_bodypart.h"

// The DLL defines XPE_AI_USE_NLOHMANN_JSON; the worker defines only XPE_AI_BODYPART_NLOHMANN (see CMakeLists.txt).
#if defined(XPE_AI_USE_NLOHMANN_JSON) || defined(XPE_AI_BODYPART_NLOHMANN)
#define XPE_AI_BODYPART_HAS_JSON 1
#include <nlohmann/json.hpp>
#endif

namespace xpe::ai {

/**
 * The model behind xpe_bodypart_recognize and what was read next to it: the session, the class labels from the
 * sidecar `bodypart.json`, and the input size the model's graph declares.
 */
struct BodyPartModel {
    std::unique_ptr<OnnxSession> session;
    std::vector<std::string> labels;
    uint32_t inputHeight{0};
    uint32_t inputWidth{0};
};

/** Why a model could not be used. Everything but kNone is "the model is unavailable", never a fault of the call. */
enum class BodyPartLoadFailure {
    kNone,
    kNoModelFile,       ///< bodypart.onnx is not there
    kModelUnreadable,   ///< it is there but the runtime cannot load it
    kLabels,            ///< the sidecar is missing or unusable
    kInputShape,        ///< the graph's input is not a fixed single-channel image
    kOutputSize,        ///< the graph's fixed output length differs from the label count
};

/**
 * Read `labels` from the sidecar: a JSON object with a non-empty array of non-empty strings, each short enough for
 * the worker protocol's label limit and free of the characters the worker's reply cannot carry (a quote, a
 * backslash, a control character: the reply format has no escapes). Returns the reason text, nullptr on success.
 */
inline const char* LoadBodyPartLabels(const std::string& sidecarPath, std::vector<std::string>* labels) {
#ifdef XPE_AI_BODYPART_HAS_JSON
    std::ifstream f(sidecarPath);
    if (!f) return "label sidecar bodypart.json not found";
    try {
        nlohmann::json j;
        f >> j;
        if (!j.is_object() || !j.contains("labels") || !j["labels"].is_array() || j["labels"].empty()) {
            return "label sidecar has no non-empty labels array";
        }
        for (const auto& e : j["labels"]) {
            if (!e.is_string()) return "label sidecar has a label that is not a string";
            const std::string v = e.get<std::string>();
            if (v.empty() || v.size() >= XPE_AI_MAX_BODYPART_LEN) return "label sidecar has an empty or too long label";
            for (const char c : v) {
                if (c == '"' || c == '\\' || static_cast<unsigned char>(c) < 0x20) {
                    return "label sidecar has a label with a quote, backslash or control character";
                }
            }
            labels->push_back(v);
        }
    } catch (const std::exception&) {
        labels->clear();
        return "label sidecar is not valid JSON";
    }
    return nullptr;
#else
    (void)sidecarPath;
    (void)labels;
    return "this build cannot read the label sidecar";
#endif
}

/**
 * Build the model from `<modelDir>/bodypart.onnx` and `<modelDir>/bodypart.json` (modelDir may be empty: the
 * working directory). Returns the reason text on failure (the model stays unset), nullptr on success; @p kind
 * and @p detail (the runtime's own message, for the log) are optional. A model whose output length is fixed and
 * differs from the label count is refused here; one with a dynamic output is checked against each result.
 */
inline const char* LoadBodyPartModel(const std::string& modelDir, std::unique_ptr<BodyPartModel>* out,
                                     BodyPartLoadFailure* kind = nullptr, std::string* detail = nullptr) {
    BodyPartLoadFailure ignored;
    BodyPartLoadFailure& why_kind = kind ? *kind : ignored;
    why_kind = BodyPartLoadFailure::kNone;
    const std::string base = modelDir.empty() ? std::string() : modelDir + "/";
    const std::string modelPath = base + "bodypart.onnx";
    auto m = std::make_unique<BodyPartModel>();

    OnnxSessionConfig cfg;
    cfg.model_path = modelPath;
    cfg.execution_provider = ExecutionProvider::kCpu;
    cfg.num_threads = 1;
    auto created = OnnxSession::Create(cfg);
    if (!created.has_value()) {
        if (detail) *detail = "cannot load " + modelPath + ": " + created.message;
        if (created.code == OnnxErrorCode::kInvalidModelPath) {
            why_kind = BodyPartLoadFailure::kNoModelFile;
            return "no model file";
        }
        why_kind = BodyPartLoadFailure::kModelUnreadable;
        return "the model file cannot be loaded";
    }
    m->session = std::move(created.value);

    if (const char* why = LoadBodyPartLabels(base + "bodypart.json", &m->labels)) {
        why_kind = BodyPartLoadFailure::kLabels;
        return why;
    }

    const std::vector<TensorMetadata> inputs = m->session->GetInputMetadata();
    if (inputs.empty() || !BodyPartInputSize(inputs.front().shape, &m->inputHeight, &m->inputWidth)) {
        why_kind = BodyPartLoadFailure::kInputShape;
        return "the model input shape is not a fixed single-channel image";
    }
    const std::vector<TensorMetadata> outputs = m->session->GetOutputMetadata();
    if (!outputs.empty()) {
        int64_t n = 1;
        bool fixed = true;
        for (int64_t d : outputs.front().shape) {
            if (d < 0) { fixed = false; break; }
            n *= d;
        }
        if (fixed && static_cast<size_t>(n) != m->labels.size()) {
            why_kind = BodyPartLoadFailure::kOutputSize;
            return "the model output size differs from the number of labels";
        }
    }
    *out = std::move(m);
    return nullptr;
}

}  // namespace xpe::ai

#endif  // XPE_AI_BODYPART_MODEL_H
