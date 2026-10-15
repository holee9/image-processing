/**
 * @file test_bodypart_decision.cpp
 * @brief The shared body-part decision functions, called directly (QA-B-191 M4a, #130, T-006).
 *
 * These are the functions the in-process path, the worker and the parent's worker path all call (see
 * ai_bodypart_decision.h). Testing them directly pins what "the output means" independently of any model, and the
 * alert texts are written out LITERALLY: a test that rebuilt the text with the module's formatter would agree
 * with it even when both were wrong. Needs no ONNX Runtime, so it runs in the stub build too.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "ai_bodypart_decision.h"

using xpe::ai::BodyPartJudgement;
using xpe::ai::JudgeBodyPartOutput;

namespace {
constexpr float kInf = std::numeric_limits<float>::infinity();
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

xpe::ai::BodyPartVerdict Judge(const std::vector<float>& v) { return JudgeBodyPartOutput(v.data(), v.size()); }
}  // namespace

TEST(BodyPartDecision, ThePickedClassIsTheLargestAndTheConfidenceIsItsValue) {
    const auto v = Judge({0.1f, 0.7f, 0.2f});
    EXPECT_EQ(BodyPartJudgement::kOk, v.judgement);
    EXPECT_EQ(1u, v.best);
    EXPECT_EQ(0.7f, v.confidence);
}

TEST(BodyPartDecision, OnATieTheFirstClassWins) {
    const auto v = Judge({0.4f, 0.4f, 0.2f});
    EXPECT_EQ(BodyPartJudgement::kOk, v.judgement);
    EXPECT_EQ(0u, v.best) << "strictly greater replaces; equal does not";
    const auto last = Judge({0.2f, 0.4f, 0.4f});
    EXPECT_EQ(1u, last.best);
}

TEST(BodyPartDecision, TheBoundariesZeroAndOneAreProbabilities) {
    EXPECT_EQ(BodyPartJudgement::kOk, Judge({0.0f, 1.0f}).judgement);
    EXPECT_EQ(BodyPartJudgement::kOk, Judge({0.0f}).judgement) << "one class is a valid vector";
}

TEST(BodyPartDecision, AValueJustOutsideTheRangeIsRefusedWhole) {
    EXPECT_EQ(BodyPartJudgement::kOutOfRange, Judge({0.5f, std::nextafter(1.0f, 2.0f)}).judgement);
    EXPECT_EQ(BodyPartJudgement::kOutOfRange, Judge({std::nextafter(0.0f, -1.0f), 0.5f}).judgement);
    EXPECT_EQ(BodyPartJudgement::kOutOfRange, Judge({0.2f, 0.3f, -0.0001f}).judgement) << "a late value counts too";
}

TEST(BodyPartDecision, NonFiniteIsToldApartFromOutOfRangeAndCheckedFirst) {
    EXPECT_EQ(BodyPartJudgement::kNonFinite, Judge({0.5f, kNaN}).judgement);
    EXPECT_EQ(BodyPartJudgement::kNonFinite, Judge({kInf, 0.5f}).judgement)
        << "inf is also out of range, but the finite check comes first (the alert text differs)";
    EXPECT_EQ(BodyPartJudgement::kNonFinite, Judge({2.0f, -kInf}).judgement);
}

TEST(BodyPartDecision, NothingToJudgeIsEmptyNotOk) {
    EXPECT_EQ(BodyPartJudgement::kEmpty, JudgeBodyPartOutput(nullptr, 3).judgement);
    EXPECT_EQ(BodyPartJudgement::kEmpty, Judge({}).judgement);
}

TEST(BodyPartDecision, FloatTextIsTheShortestThatReadsBackAsTheSameFloat) {
    EXPECT_EQ("0.6", xpe::ai::ShortestFloatText(0.6f));
    EXPECT_EQ("0.6000001", xpe::ai::ShortestFloatText(std::nextafter(0.6f, 1.0f)));
    EXPECT_EQ("0.75", xpe::ai::ShortestFloatText(0.75f));
    EXPECT_EQ("1", xpe::ai::ShortestFloatText(1.0f));
    EXPECT_EQ("0", xpe::ai::ShortestFloatText(0.0f));
}

TEST(BodyPartDecision, TheLowConfidenceTextsAreTheContractedOnes) {
    EXPECT_EQ(
        "AI body-part confidence 0.6 is below the threshold 0.6000001 (REQ-AI-012): UNKNOWN is returned; "
        "use the deterministic body-part lookup",
        xpe::ai::LowConfidenceAlertText(0.6f, std::nextafter(0.6f, 1.0f), nullptr));
    EXPECT_EQ(
        "AI body-part confidence 0.6 is below the threshold 0.6000001 (REQ-AI-012): the label CHEST is returned "
        "because fallback_mode is off; an exposure parameter chosen from it may be wrong",
        xpe::ai::LowConfidenceAlertText(0.6f, std::nextafter(0.6f, 1.0f), "CHEST"));
}
