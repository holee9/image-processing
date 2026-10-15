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
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "xpe/ai/ai_onnx_session.h"
#include "xpe/ai/ai_worker_protocol.h"
#include "ai_bodypart.h"

#include <nlohmann/json.hpp>   // a required dependency of the DLL and of the worker (QA-B-194b)

namespace xpe::ai {

#ifdef XPE_AI_TEST_HOOKS
void CallBeforeLabelParseHook();   // ai_onnx_session.cpp, test builds only (QA-B-195c)
#endif

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
    kNotTrusted,        ///< the model or its sidecar failed signature verification (QA-B-195): nothing was loaded
    kSidecarInvalid,    ///< the signature verified but the sidecar does not say what REQ-AI-008 requires (QA-B-197)
    kOutOfMemory,       ///< a shortage of memory while creating the session (QA-B-194b): NOT "the model is unavailable"
};

namespace detail {

/**
 * Reads the `labels` of a body-part sidecar WITHOUT building a JSON document (QA-B-195c), as ParseModelSidecar reads the
 * rest of the sidecar: nlohmann frees a non-empty parsed document with an allocation inside a noexcept destructor, so a
 * shortage of memory at that instant terminated the process (the QA-B-194 M5 finding, left open for this file by
 * QA-B-195b). Only the array under the top-level key "labels" is looked at; a "labels" key anywhere deeper is ignored,
 * and the first label that is refused stops the parse with its reason.
 */
class LabelsSax final : public nlohmann::json::json_sax_t {
public:
    std::vector<std::string>* labels = nullptr;
    const char* reason = nullptr;     ///< why a label was refused (the parse stopped there)
    bool rootChecked = false;
    bool rootIsObject = false;
    bool labelsIsArray = false;       ///< the (last) top-level "labels" member is an array

    using number_integer_t = nlohmann::json::number_integer_t;
    using number_unsigned_t = nlohmann::json::number_unsigned_t;
    using number_float_t = nlohmann::json::number_float_t;
    using string_t = nlohmann::json::string_t;
    using binary_t = nlohmann::json::binary_t;

    bool null() override { return scalar(nullptr); }
    bool boolean(bool) override { return scalar(nullptr); }
    bool number_integer(number_integer_t) override { return scalar(nullptr); }
    bool number_unsigned(number_unsigned_t) override { return scalar(nullptr); }
    bool number_float(number_float_t, const string_t&) override { return scalar(nullptr); }
    bool string(string_t& v) override { return scalar(&v); }
    bool binary(binary_t&) override { return false; }
    bool start_object(std::size_t) override { return open(false); }
    bool start_array(std::size_t) override { return open(true); }
    bool end_object() override { --depth_; return true; }
    bool end_array() override {
        --depth_;
        if (depth_ == 1) inLabels_ = false;
        return true;
    }
    bool key(string_t& k) override {
        if (depth_ == 1) {
            isLabelsKey_ = (k == "labels");
            if (isLabelsKey_) {   // a repeated key: the last one wins, as in a parsed document
                labels->clear();
                labelsIsArray = false;
            }
        }
        return true;
    }
    bool parse_error(std::size_t, const std::string&, const nlohmann::detail::exception&) override { return false; }

private:
    size_t depth_ = 0;          ///< 0 outside the root object, 1 inside it, 2 inside the labels array
    bool isLabelsKey_ = false;  ///< the current top-level member is "labels"
    bool inLabels_ = false;     ///< inside the labels array

    bool scalar(const string_t* str) {
        if (!rootChecked) {   // the root is not an object
            rootChecked = true;
            return false;
        }
        if (inLabels_ && depth_ == 2) {
            if (str == nullptr) {
                reason = "label sidecar has a label that is not a string";
                return false;
            }
            return take(*str);
        }
        return true;   // a member this reader does not use (including "labels" given as a scalar: labelsIsArray stays false)
    }
    bool open(bool isArray) {
        if (!rootChecked) {
            rootChecked = true;
            if (isArray) return false;   // the root is not an object
            rootIsObject = true;
            depth_ = 1;
            return true;
        }
        if (inLabels_ && depth_ == 2) {   // a label that is an object or an array
            reason = "label sidecar has a label that is not a string";
            return false;
        }
        if (depth_ == 1 && isLabelsKey_ && isArray) {
            labelsIsArray = true;
            inLabels_ = true;
        }
        ++depth_;
        return true;
    }
    bool take(const std::string& v) {
        if (v.empty() || v.size() >= XPE_AI_MAX_BODYPART_LEN) {
            reason = "label sidecar has an empty or too long label";
            return false;
        }
        for (const char c : v) {
            const unsigned char u = static_cast<unsigned char>(c);
            if (c == '"' || c == '\\' || u < 0x20 || u > 0x7E) {
                reason = "label sidecar has a label with a character outside printable ASCII, or a quote or backslash";
                return false;
            }
        }
#ifdef XPE_AI_TEST_HOOKS
        CallBeforeLabelParseHook();
#endif
        labels->push_back(v);
        return true;
    }
};

}  // namespace detail

/**
 * Read `labels` from the sidecar's TEXT: a JSON object with a non-empty array of non-empty strings, each short enough
 * for the worker protocol's label limit and made only of printable ASCII (0x20-0x7E) without a double quote or a
 * backslash: the worker's reply format has no escapes and no encoding, so a label it could not carry is refused
 * by both paths alike. Returns the reason text, nullptr on success.
 *
 * The text is the sidecar AS VERIFIED (OnnxSession::VerifiedSidecar), never a second read of the file: a sidecar
 * that was swapped after the signature check cannot reach here (QA-B-195). A null pointer is "no sidecar".
 *
 * A shortage of memory is NOT a bad sidecar (QA-B-195c): std::bad_alloc is caught on its own, @p outOfMemory (when given)
 * is set, "out of memory" is returned, and the caller reports BodyPartLoadFailure::kOutOfMemory -- never "the model is
 * unavailable". It used to be swallowed by the catch for every std::exception and read as "not valid JSON".
 */
inline const char* LoadBodyPartLabels(const std::string* sidecarText, std::vector<std::string>* labels,
                                      bool* outOfMemory = nullptr) {
    if (outOfMemory) *outOfMemory = false;
    if (sidecarText == nullptr) return "label sidecar bodypart.json not found";
    detail::LabelsSax h;
    h.labels = labels;
    try {
#ifdef XPE_AI_TEST_HOOKS
        CallBeforeLabelParseHook();
#endif
        if (!nlohmann::json::sax_parse(*sidecarText, &h)) {
            labels->clear();
            if (h.reason != nullptr) return h.reason;
            if (h.rootChecked && !h.rootIsObject) return "label sidecar has no non-empty labels array";
            return "label sidecar is not valid JSON";
        }
    } catch (const std::bad_alloc&) {
        labels->clear();
        if (outOfMemory) *outOfMemory = true;
        return "out of memory";
    } catch (const std::exception&) {
        labels->clear();
        return "label sidecar is not valid JSON";
    }
    if (!h.labelsIsArray || labels->empty()) {
        labels->clear();
        return "label sidecar has no non-empty labels array";
    }
    return nullptr;
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
    cfg.role = "bodypart";   // part of what the signature covers (QA-B-195)
    cfg.execution_provider = ExecutionProvider::kCpu;
    cfg.num_threads = 1;
    auto created = OnnxSession::Create(cfg);
    if (!created.has_value()) {
        if (detail) *detail = "cannot load " + modelPath + ": " + created.message;
        if (created.code == OnnxErrorCode::kInvalidModelPath) {
            why_kind = BodyPartLoadFailure::kNoModelFile;
            return "no model file";
        }
        if (created.code == OnnxErrorCode::kModelNotTrusted) {
            why_kind = BodyPartLoadFailure::kNotTrusted;
            return "the model files failed signature verification";
        }
        if (created.code == OnnxErrorCode::kSidecarInvalid) {
            why_kind = BodyPartLoadFailure::kSidecarInvalid;
            return "the model sidecar failed the metadata check";
        }
        if (created.code == OnnxErrorCode::kOutOfMemory) {
            why_kind = BodyPartLoadFailure::kOutOfMemory;
            return "out of memory";
        }
        why_kind = BodyPartLoadFailure::kModelUnreadable;
        return "the model file cannot be loaded";
    }
    m->session = std::move(created.value);

    bool labelsOom = false;
    if (const char* why = LoadBodyPartLabels(m->session->VerifiedSidecar(), &m->labels, &labelsOom)) {
        why_kind = labelsOom ? BodyPartLoadFailure::kOutOfMemory : BodyPartLoadFailure::kLabels;
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
