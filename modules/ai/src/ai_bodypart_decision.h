/**
 * @file ai_bodypart_decision.h
 * @brief The body-part decision, as pure functions both paths call (QA-B-191 M4a, #130, T-006).
 *
 * WHY THIS FILE EXISTS. xpe_bodypart_recognize has two ways to get the model's answer: the model runs in this
 * process, or it runs in the worker process (M4b/M4c). If each way carried its own copy of "is this output a
 * probability vector, which class won, what does a low confidence say", the two would drift: the same image and
 * model would give different labels, or different alert text, depending on a configuration switch. These are the
 * functions that decide; the in-process path, the worker and the parent's worker path all call them, so there is
 * ONE answer to "what does this output mean".
 *
 * Header-only and free of ONNX Runtime types, so a test can call them directly.
 */
#ifndef XPE_AI_BODYPART_DECISION_H
#define XPE_AI_BODYPART_DECISION_H

#include <charconv>
#include <cstddef>
#include <string>

#include "ai_finite.h"

namespace xpe::ai {

/** What a model output is, judged as a probability vector. */
enum class BodyPartJudgement {
    kOk,          ///< finite, every value in [0, 1], at least one value
    kEmpty,       ///< no values at all
    kNonFinite,   ///< a value is inf or NaN (a refusal of this image, not a fault of the model's host)
    kOutOfRange,  ///< a finite value below 0 or above 1: not a probability
};

struct BodyPartVerdict {
    BodyPartJudgement judgement = BodyPartJudgement::kEmpty;
    size_t best = 0;          ///< index of the largest value (the first on a tie); meaningful only when kOk
    float confidence = 0.0f;  ///< the largest value; meaningful only when kOk
};

/**
 * Judge a model output: finite first, then the range, then the most probable class.
 * The module applies no softmax: the model emits probabilities, and a vector that is not one is refused whole,
 * so a refusal never leaks a partial value.
 */
inline BodyPartVerdict JudgeBodyPartOutput(const float* values, size_t count) {
    BodyPartVerdict v;
    if (count == 0 || values == nullptr) {
        v.judgement = BodyPartJudgement::kEmpty;
        return v;
    }
    if (!AllFinite(values, count)) {
        v.judgement = BodyPartJudgement::kNonFinite;
        return v;
    }
    for (size_t i = 0; i < count; ++i) {
        if (values[i] < 0.0f || values[i] > 1.0f) {
            v.judgement = BodyPartJudgement::kOutOfRange;
            return v;
        }
    }
    size_t best = 0;
    for (size_t i = 1; i < count; ++i) {
        if (values[i] > values[best]) best = i;   // strictly greater: on a tie the first class wins
    }
    v.judgement = BodyPartJudgement::kOk;
    v.best = best;
    v.confidence = values[best];
    return v;
}

/** A float as the SHORTEST text that reads back as the same float (0.6f is "0.6", the next float up is "0.6000001"). */
inline std::string ShortestFloatText(float v) {
    char buf[32];
    const auto r = std::to_chars(buf, buf + sizeof(buf), v);
    return std::string(buf, r.ptr);
}

/**
 * The text of the REQ-AI-012 low-confidence event. @p labelUsed is the label returned anyway (fallback_mode off),
 * or null when UNKNOWN is returned. CROSS-LANE CONTRACT: clients may match it; ai_api.h records it whole.
 */
inline std::string LowConfidenceAlertText(float confidence, float threshold, const char* labelUsed) {
    std::string msg = "AI body-part confidence " + ShortestFloatText(confidence) + " is below the threshold " +
                      ShortestFloatText(threshold) + " (REQ-AI-012): ";
    if (labelUsed) {
        msg += std::string("the label ") + labelUsed +
               " is returned because fallback_mode is off; an exposure parameter chosen from it may be wrong";
    } else {
        msg += "UNKNOWN is returned; use the deterministic body-part lookup";
    }
    return msg;
}

}  // namespace xpe::ai

#endif  // XPE_AI_BODYPART_DECISION_H
