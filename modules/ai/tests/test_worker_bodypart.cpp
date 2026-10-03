/**
 * @file test_worker_bodypart.cpp
 * @brief BODYPART_RECOGNIZE through the worker process: the strict reply parser and the agreement with the
 *        in-process path (QA-B-191 M4b, #130, T-006).
 *
 * THE MODELS ARE TOY MODELS (tests/data/make_bodypart_models.py): what is tested is the path and its refusals,
 * not how well any model recognises a body part.
 *
 * Two halves:
 *   1. A fake worker (tests/fake_worker_main.cpp) sends a reply of the test's choosing, so every deviation the
 *      protocol forbids is actually sent and refused. Needs no ONNX Runtime.
 *   2. The REAL worker and the in-process path are given the same model directory and the same images, and what
 *      each says is reduced to one comparable string. The two paths share their judging code, so this is the test
 *      that fails when someone forks one of them.
 */

#include <gtest/gtest.h>

#include "ai_bodypart_decision.h"
#include "error_frame_cases.h"
#include "ai_worker_supervisor.h"
#include "xpe/ai/ai_api.h"
#include "xpe/ai/ai_onnx_session.h"
#include "xpe/ai/ai_worker_protocol.h"
#include "xpe/common/xpe_error.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "test_signing_helper.h"

using xpe::ai::BodyPartJudgement;
using xpe::ai::BodyPartReply;
using xpe::ai::WorkerSupervisor;
using xpe::ai::WorkerSupervisorConfig;

#ifndef XPE_AI_TEST_DATA_DIR
#error "XPE_AI_TEST_DATA_DIR must be defined by the build (modules/ai/CMakeLists.txt)"
#endif
#ifndef XPE_AI_FAKE_WORKER_EXE
#error "XPE_AI_FAKE_WORKER_EXE must be defined by the build (modules/ai/CMakeLists.txt)"
#endif
#ifndef XPE_AI_WORKER_EXE
#error "XPE_AI_WORKER_EXE must be defined by the build (modules/ai/CMakeLists.txt)"
#endif

namespace {

constexpr uint32_t kBudgetMs = 8000;
const std::string kData = XPE_AI_TEST_DATA_DIR;
std::string Dir(const char* name) { return kData + "/" + name; }
bool IsStub() { return xpe::ai::OnnxSession::IsStubBuild(); }

/**
 * A w x h float image whose value is computed from the pixel position. The XpeImageBuffer is built on demand by
 * Buffer(), never stored: a stored copy would keep pointing at the pixels of the object it was copied from.
 */
struct Pix {
    uint32_t w, h;
    std::vector<float> v;
    template <typename F>
    Pix(uint32_t width, uint32_t height, F value) : w(width), h(height), v(static_cast<size_t>(width) * height) {
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) v[static_cast<size_t>(y) * w + x] = value(x, y);
        }
    }
    XpeImageBuffer Buffer() const {
        XpeImageBuffer b{};
        b.width = w;
        b.height = h;
        b.bitsAllocated = 32;
        b.bitsStored = 32;
        b.format = XPE_PIXEL_FLOAT32;
        b.data = const_cast<float*>(v.data());
        b.dataSize = v.size() * sizeof(float);
        return b;
    }
};

Pix Flat(float value, uint32_t w = 4, uint32_t h = 4) {
    return Pix(w, h, [value](uint32_t, uint32_t) { return value; });
}
Pix TopBright(uint32_t w, uint32_t h) {
    return Pix(w, h, [h](uint32_t, uint32_t y) { return y < h / 2 ? 0.75f : 0.0f; });
}
Pix BottomBright(uint32_t w, uint32_t h) {
    return Pix(w, h, [h](uint32_t, uint32_t y) { return y >= h / 2 ? 0.75f : 0.0f; });
}

/** Sets the fake worker's mode and the JSON it will send for the duration of a test. */
class FakeReply {
public:
    FakeReply(const char* mode, const std::string& json, uint32_t flags = 0) {
        _putenv_s("XPE_FAKE_WORKER_MODE", mode);
        _putenv_s("XPE_FAKE_WORKER_JSON", json.c_str());
        _putenv_s("XPE_FAKE_WORKER_FLAGS", std::to_string(flags).c_str());
    }
    ~FakeReply() {
        _putenv_s("XPE_FAKE_WORKER_MODE", "");
        _putenv_s("XPE_FAKE_WORKER_JSON", "");
        _putenv_s("XPE_FAKE_WORKER_FLAGS", "");
    }
};

WorkerSupervisorConfig FakeCfg() {
    WorkerSupervisorConfig c;
    c.worker_exe = XPE_AI_FAKE_WORKER_EXE;
    c.timeout_ms = kBudgetMs;
    return c;
}

struct Outcome {
    XpeErrorCode rc;
    BodyPartReply reply;
    bool unavailable;
    bool workerKept;
};

/** One call to a fake worker that answers with @p json as a RESP frame (or as an ERROR frame). */
Outcome AskFake(const char* mode, const std::string& json, uint32_t flags = 0) {
    FakeReply f(mode, json, flags);
    WorkerSupervisor sup(FakeCfg());
    const Pix image = Flat(0.5f);
    Outcome o{};
    o.rc = sup.BodyPartRecognize(image.w, image.h, image.v.data(), &o.reply);
    o.unavailable = sup.LastModelUnavailable();
    o.workerKept = sup.WorkerPid() != 0;
    return o;
}

}  // namespace

// ===== half 1: the strict parser, fed by a fake worker ====================================================

TEST(WorkerBodyPartReply, ControlAValidReplyIsAcceptedAndKeepsTheWorker) {
    // Without this, every refusal below could be red for a reason unrelated to the reply's content.
    const Outcome o = AskFake("bodypart_raw", R"({"success":true,"outcome":"ok","body_part":"CHEST","confidence":0.75})");
    EXPECT_EQ(XPE_OK, o.rc);
    EXPECT_EQ(BodyPartJudgement::kOk, o.reply.judgement);
    EXPECT_STREQ("CHEST", o.reply.label);
    EXPECT_EQ(0.75f, o.reply.confidence);
    EXPECT_TRUE(o.workerKept);
    EXPECT_FALSE(o.unavailable);
}

TEST(WorkerBodyPartReply, ConfidenceIsReadBackAsExactlyTheFloatTheWorkerPrinted) {
    struct Case { const char* text; float value; };
    for (const Case c : {Case{"0", 0.0f}, Case{"1", 1.0f}, Case{"0.6", 0.6f}, Case{"0.6000001", 0.6000001f},
                         Case{"1e-05", 1e-05f}, Case{"9.999999e-01", 9.999999e-01f}}) {
        const Outcome o = AskFake("bodypart_raw", std::string(R"({"success":true,"outcome":"ok","body_part":"X","confidence":)") +
                                                      c.text + "}");
        EXPECT_EQ(XPE_OK, o.rc) << c.text;
        EXPECT_EQ(c.value, o.reply.confidence) << c.text;
    }
}

TEST(WorkerBodyPartReply, ALabelOfTheLongestAllowedLengthIsAccepted) {
    const std::string longest(XPE_AI_MAX_BODYPART_LEN - 1, 'A');
    const Outcome o = AskFake("bodypart_raw", R"({"success":true,"outcome":"ok","body_part":")" + longest +
                                                  R"(","confidence":0.5})");
    EXPECT_EQ(XPE_OK, o.rc);
    EXPECT_EQ(longest, std::string(o.reply.label));
}

TEST(WorkerBodyPartReply, ARefusalOfTheModelsOutputIsAValidReplyThatCarriesNoAnswer) {
    for (const char* kind : {"non_finite", "out_of_range"}) {
        const Outcome o = AskFake("bodypart_raw", std::string(R"({"success":true,"outcome":")") + kind + R"("})");
        EXPECT_EQ(XPE_OK, o.rc) << kind;
        EXPECT_EQ(std::string(kind) == "non_finite" ? BodyPartJudgement::kNonFinite : BodyPartJudgement::kOutOfRange,
                  o.reply.judgement) << kind;
        EXPECT_STREQ("", o.reply.label) << kind;
        EXPECT_EQ(0.0f, o.reply.confidence) << kind;
        EXPECT_TRUE(o.workerKept) << "the worker answered correctly: " << kind;
    }
}

TEST(WorkerBodyPartReply, AKeyTheProtocolDoesNotKnowIsIgnored) {
    const Outcome o = AskFake("bodypart_raw",
                              R"({"success":true,"outcome":"ok","body_part":"CHEST","confidence":0.5,"model_id":"x"})");
    EXPECT_EQ(XPE_OK, o.rc);
}

TEST(WorkerBodyPartReply, EveryReplyTheProtocolForbidsIsAProtocolFaultAndTheWorkerIsDiscarded) {
    const std::vector<std::pair<const char*, std::string>> forbidden = {
        {"no success", R"({"outcome":"ok","body_part":"C","confidence":0.5})"},
        {"success false", R"({"success":false,"outcome":"ok","body_part":"C","confidence":0.5})"},
        {"success a string", R"({"success":"true","outcome":"ok","body_part":"C","confidence":0.5})"},
        {"no outcome", R"({"success":true,"body_part":"C","confidence":0.5})"},
        {"outcome not a string", R"({"success":true,"outcome":1,"body_part":"C","confidence":0.5})"},
        {"unknown outcome", R"({"success":true,"outcome":"weird"})"},
        {"ok without a label", R"({"success":true,"outcome":"ok","confidence":0.5})"},
        {"ok without a confidence", R"({"success":true,"outcome":"ok","body_part":"C"})"},
        {"empty label", R"({"success":true,"outcome":"ok","body_part":"","confidence":0.5})"},
        {"label of 64 bytes",
         R"({"success":true,"outcome":"ok","body_part":")" + std::string(XPE_AI_MAX_BODYPART_LEN, 'A') +
             R"(","confidence":0.5})"},
        {"label not a string", R"({"success":true,"outcome":"ok","body_part":5,"confidence":0.5})"},
        {"label with DEL", std::string(R"({"success":true,"outcome":"ok","body_part":"AB)") + "\x7f" + R"(C","confidence":0.5})"},
        {"label with a non-ASCII byte", std::string(R"({"success":true,"outcome":"ok","body_part":"AB)") + "\xc3\xa9" + R"(C","confidence":0.5})"},
        {"label with a control character", std::string(R"({"success":true,"outcome":"ok","body_part":"AB)") + "\x07" + R"(C","confidence":0.5})"},
        {"label with an escaped quote", R"({"success":true,"outcome":"ok","body_part":"A\"B","confidence":0.5})"},
        {"label with an escaped backslash", R"({"success":true,"outcome":"ok","body_part":"A\\B","confidence":0.5})"},
        {"confidence a string", R"({"success":true,"outcome":"ok","body_part":"C","confidence":"0.5"})"},
        {"confidence true", R"({"success":true,"outcome":"ok","body_part":"C","confidence":true})"},
        {"confidence null", R"({"success":true,"outcome":"ok","body_part":"C","confidence":null})"},
        {"confidence above 1", R"({"success":true,"outcome":"ok","body_part":"C","confidence":1.5})"},
        {"confidence just above 1", R"({"success":true,"outcome":"ok","body_part":"C","confidence":1.0000001})"},
        {"confidence below 0", R"({"success":true,"outcome":"ok","body_part":"C","confidence":-0.1})"},
        {"confidence with junk", R"({"success":true,"outcome":"ok","body_part":"C","confidence":0.5abc})"},
        {"confidence nan", R"({"success":true,"outcome":"ok","body_part":"C","confidence":nan})"},
        {"confidence inf", R"({"success":true,"outcome":"ok","body_part":"C","confidence":1e999})"},
        {"refusal with a label", R"({"success":true,"outcome":"non_finite","body_part":"C"})"},
        {"refusal with a confidence", R"({"success":true,"outcome":"out_of_range","confidence":0.5})"},
        {"trailing junk", R"({"success":true,"outcome":"non_finite"}x)"},
        {"duplicate key", R"({"success":true,"success":true,"outcome":"non_finite"})"},
        {"nested object", R"({"success":true,"outcome":"non_finite","x":{"a":1}})"},
        {"empty object", "{}"},
        {"not JSON", "ok"},
        {"empty body", ""},
    };
    for (const auto& c : forbidden) {
        const Outcome o = AskFake("bodypart_raw", c.second);
        EXPECT_EQ(XPE_ERR_IO_FAILED, o.rc) << c.first << ": " << c.second;
        EXPECT_FALSE(o.workerKept) << c.first << ": a worker that sent a forbidden reply was kept";
        EXPECT_FALSE(o.unavailable) << c.first;
        EXPECT_EQ(BodyPartJudgement::kEmpty, o.reply.judgement) << c.first << ": nothing is reported from a bad reply";
        EXPECT_STREQ("", o.reply.label) << c.first;
    }
}

TEST(WorkerBodyPartReply, AnErrorFrameThatSaysTheModelIsUnavailableIsNotAFault) {
    const Outcome o = AskFake("bodypart_error_raw",
                              R"({"error_code":-4,"model_unavailable":true,"error_message":"no model file"})");
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, o.rc) << "an error frame is passed through as the worker's own code";
    EXPECT_TRUE(o.unavailable);
    EXPECT_TRUE(o.workerKept) << "the worker answered: it is healthy";
}

TEST(WorkerBodyPartReply, AnErrorFrameWithoutTheFlagIsAnOrdinaryErrorAndTheWorkerIsKept) {
    const Outcome o = AskFake("bodypart_error_raw", R"({"error_code":-3,"error_message":"run failed"})");
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, o.rc);
    EXPECT_FALSE(o.unavailable);
    EXPECT_TRUE(o.workerKept);
}

TEST(WorkerBodyPartReply, TheFlagIsParsedNotSearchedSoTextThatLooksLikeItIsOnlyText) {
    // The model's error text can hold anything. The flag is read from the parsed object, so a message that contains
    // the characters "model_unavailable":true (escaped, as the worker writes it) is only a message.
    const Outcome text = AskFake("bodypart_error_raw",
                                 R"({"error_code":-4,"error_message":"a \"model_unavailable\":true b"})");
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, text.rc);
    EXPECT_FALSE(text.unavailable) << "text that looks like the flag must not set it";
    EXPECT_TRUE(text.workerKept);
    // The order of the keys does not matter: the real flag is believed wherever it is, with a code it may go with.
    const Outcome before = AskFake("bodypart_error_raw",
                                   R"({"error_code":-4,"model_unavailable":true,"error_message":"x"})");
    const Outcome after = AskFake("bodypart_error_raw",
                                  R"({"error_code":-4,"error_message":"x","model_unavailable":true})");
    EXPECT_TRUE(before.unavailable);
    EXPECT_TRUE(after.unavailable);
}

TEST(WorkerBodyPartReply, AnErrorFrameTheProtocolAllowsIsAcceptedAndKeepsTheWorker) {
    struct Case { const char* name; std::string json; int code; bool unavailable; };
    const std::vector<Case> allowed = {
        {"minimal", R"({"error_code":-3})", -3, false},
        {"with a message", R"({"error_code":-3,"error_message":"run failed: x"})", -3, false},
        {"a quote and a backslash in the message", R"({"error_code":-3,"error_message":"a \"b\" c\\d"})", -3, false},
        {"flag false", R"({"error_code":-3,"model_unavailable":false,"error_message":"x"})", -3, false},
        {"flag true with IO_FAILED", R"({"error_code":-9,"model_unavailable":true,"error_message":"no model file"})", -9, true},
        {"flag true with CONFIG_INVALID", R"({"error_code":-4,"model_unavailable":true,"error_message":"x"})", -4, true},
        {"a key the protocol does not know", R"({"error_code":-3,"error_message":"x","detail":"y"})", -3, false},
        {"the largest code", R"({"error_code":-99})", -99, false},
        {"whitespace between tokens", R"( { "error_code" : -3 , "model_unavailable" : false } )", -3, false},
    };
    for (const Case& c : allowed) {
        const Outcome o = AskFake("bodypart_error_raw", c.json);
        EXPECT_EQ(static_cast<XpeErrorCode>(c.code), o.rc) << c.name;
        EXPECT_EQ(c.unavailable, o.unavailable) << c.name;
        EXPECT_TRUE(o.workerKept) << c.name << ": an ERROR frame the protocol allows leaves the worker in place";
    }
}

TEST(WorkerBodyPartReply, EveryErrorFrameTheProtocolForbidsIsAProtocolFaultAndTheWorkerIsDiscarded) {
    // Codex #77 (QA-B-191 M4f): the flag changes what the host counts, so the frame is judged like a success reply.
    const auto forbidden = error_frame_cases::ForbiddenEverywhere();
    for (const auto& c : forbidden) {
        const Outcome o = AskFake("bodypart_error_raw", c.second);
        EXPECT_EQ(XPE_ERR_IO_FAILED, o.rc) << c.first << ": " << c.second;
        EXPECT_FALSE(o.workerKept) << c.first << ": a worker that sent a forbidden ERROR frame was kept";
        EXPECT_FALSE(o.unavailable) << c.first << ": nothing is believed from a bad frame";
    }
}

TEST(WorkerBodyPartReply, ThreeContradictoryFramesInARowAreThreeCountedFailuresNeverAnUnavailableAnswer) {
    // Codex #77: believing a contradictory flag reset the host's failure count each time, so three of them in a row
    // never reached the ceiling. The host counts a call as a failure unless it returned a code AND the supervisor
    // reports "model unavailable" (ai.cpp bodyPartViaWorker). Three calls to a worker that answers
    // {"error_code":-3,"model_unavailable":true,...} must each be a failure on that test.
    FakeReply f("bodypart_error_raw", R"({"error_code":-3,"model_unavailable":true,"error_message":"contradiction"})");
    const Pix image = Flat(0.5f);
    for (int i = 0; i < 3; ++i) {
        WorkerSupervisor sup(FakeCfg());   // the faulty worker is discarded each time, so each call starts one
        BodyPartReply reply;
        const XpeErrorCode rc = sup.BodyPartRecognize(image.w, image.h, image.v.data(), &reply);
        EXPECT_NE(XPE_OK, rc) << i;
        EXPECT_FALSE(sup.LastModelUnavailable()) << i << ": the contradiction must not be taken for an unavailable model";
        EXPECT_EQ(0u, sup.WorkerPid()) << i << ": the worker that contradicted itself is discarded";
    }
}

TEST(WorkerBodyPartReply, AnErrorFrameWithNoUsableCodeIsAFaultAndTheWorkerIsDiscarded) {
    const Outcome o = AskFake("bodypart_error_raw", R"({"error_message":"no code"})");
    EXPECT_NE(XPE_OK, o.rc);
    EXPECT_FALSE(o.workerKept);
    EXPECT_FALSE(o.unavailable);
}

// ===== half 2: the real worker and the in-process path, given the same inputs ===========================

namespace {

#define REQUIRE_ONNX() \
    do { if (IsStub()) GTEST_SKIP() << "needs ONNX Runtime: the full build only"; } while (0)

bool AlertsContain(const char* needle) {
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < n; ++i) {
        char msg[512] = {0};
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) == XPE_OK && std::string(msg).find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

/** What the in-process path says about @p image under @p dir, in the vocabulary of Describe(worker...). */
std::string DescribeInProcess(const std::string& dir, const Pix& image) {
    xpe_ai_shutdown();
    xpe_clear_alerts();
    // threshold 0.0: no confidence is low, so this is what the MODEL said, not what the decision made of it.
    EXPECT_EQ(XPE_OK, xpe_ai_init(dir.c_str(), "{\"confidence_threshold\": 0.0}"));
    xpe_clear_alerts();
    char label[64];
    std::fill(label, label + sizeof(label), 'x');
    float confidence = -1.0f;
    const XpeImageBuffer buffer = image.Buffer();
    const XpeErrorCode rc = xpe_bodypart_recognize(&buffer, label, sizeof(label), &confidence);
    std::string out;
    if (AlertsContain("is unavailable")) {
        out = "unavailable";
    } else if (AlertsContain("non-finite")) {
        out = "non_finite";
    } else if (AlertsContain("not a probability vector")) {
        out = "out_of_range";
    } else if (rc == XPE_OK) {
        out = std::string("ok:") + label + ":" + xpe::ai::ShortestFloatText(confidence);
    } else {
        out = "error";
    }
    xpe_ai_shutdown();
    return out;
}

/** What the worker says about @p image under @p dir. */
std::string DescribeWorker(const std::string& dir, const Pix& image, uint32_t* startCount = nullptr) {
    WorkerSupervisorConfig c;
    c.worker_exe = XPE_AI_WORKER_EXE;
    c.model_dir = dir;
    c.timeout_ms = kBudgetMs;
    WorkerSupervisor sup(c);
    BodyPartReply reply;
    const XpeErrorCode rc = sup.BodyPartRecognize(image.w, image.h, image.v.data(), &reply);
    if (startCount) *startCount = sup.StartCount();
    if (sup.LastModelUnavailable()) return "unavailable";
    if (rc != XPE_OK) return "error";
    switch (reply.judgement) {
        case BodyPartJudgement::kNonFinite: return "non_finite";
        case BodyPartJudgement::kOutOfRange: return "out_of_range";
        case BodyPartJudgement::kOk:
            return std::string("ok:") + reply.label + ":" + xpe::ai::ShortestFloatText(reply.confidence);
        default: return "error";
    }
}

}  // namespace

TEST(WorkerBodyPartAgreement, TheRealWorkerAndTheInProcessPathSayTheSameThingForEveryModelAndImage) {
    REQUIRE_ONNX();
    const std::vector<Pix> images = {Flat(0.0f), Flat(1.0f), TopBright(4, 4), BottomBright(16, 16), Flat(0.5f, 7, 5),
                                     TopBright(64, 32)};
    // Models that answer, that refuse the output, and that cannot be used at all.
    const char* dirs[] = {"models_bodypart_a", "models_bodypart_b", "models_bodypart_dep", "models_bodypart_nhwc",
                          "models_bodypart_nonfinite", "models_bodypart_range_high", "models_bodypart_range_low",
                          "models_bodypart_labels_mismatch", "models_bodypart_no_labels", "models_bodypart_broken",
                          "models_bodypart_rank2", "models_bodypart_dynamic", "models_bodypart_runfail", "models_missing"};
    int ok = 0, nonFinite = 0, outOfRange = 0, unavailable = 0, error = 0;
    for (const char* d : dirs) {
        for (const Pix& image : images) {
            const std::string inProcess = DescribeInProcess(Dir(d), image);
            const std::string worker = DescribeWorker(Dir(d), image);
            EXPECT_EQ(inProcess, worker) << d << " " << image.w << "x" << image.h;
            if (worker.rfind("ok:", 0) == 0) ++ok;
            else if (worker == "non_finite") ++nonFinite;
            else if (worker == "out_of_range") ++outOfRange;
            else if (worker == "unavailable") ++unavailable;
            else if (worker == "error") ++error;
        }
    }
    // The control: the matrix really covered every kind of outcome, so "they agree" is not agreement on one kind.
    EXPECT_GT(ok, 0);
    EXPECT_GT(nonFinite, 0);
    EXPECT_GT(outOfRange, 0);
    EXPECT_GT(unavailable, 0);
    EXPECT_GT(error, 0) << "a model that exists and fails to run is its own outcome";
}

TEST(WorkerBodyPartAgreement, TheWorkerResizesTheImageToTheModelsInputAndTheAnswerFollowsTheImage) {
    REQUIRE_ONNX();
    // models_bodypart_dep answers from the image's content: a bright top and a bright bottom give different labels
    // or confidences. Sent at sizes the model does not take, the worker has to resize, and the answer has to follow.
    const std::string top = DescribeWorker(Dir("models_bodypart_dep"), TopBright(64, 64));
    const std::string bottom = DescribeWorker(Dir("models_bodypart_dep"), BottomBright(64, 64));
    EXPECT_EQ(0u, top.rfind("ok:", 0)) << top;
    EXPECT_EQ(0u, bottom.rfind("ok:", 0)) << bottom;
    EXPECT_EQ(DescribeInProcess(Dir("models_bodypart_dep"), TopBright(64, 64)), top);
    EXPECT_EQ(DescribeInProcess(Dir("models_bodypart_dep"), BottomBright(64, 64)), bottom);
}

TEST(WorkerBodyPartAgreement, AnUnavailableModelKeepsTheWorkerAndIsNotAFault) {
    REQUIRE_ONNX();
    WorkerSupervisorConfig c;
    c.worker_exe = XPE_AI_WORKER_EXE;
    c.model_dir = Dir("models_missing");
    c.timeout_ms = kBudgetMs;
    WorkerSupervisor sup(c);
    const Pix image = Flat(0.5f);
    BodyPartReply reply;
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(XPE_ERR_IO_FAILED, sup.BodyPartRecognize(image.w, image.h, image.v.data(), &reply)) << i;
        EXPECT_TRUE(sup.LastModelUnavailable()) << i;
    }
    EXPECT_EQ(1u, sup.StartCount()) << "an unavailable model is not a reason to restart the worker";
    EXPECT_NE(0u, sup.WorkerPid());
}

TEST(WorkerBodyPartAgreement, ALabelOutsidePrintableAsciiOrWithAQuoteOrBackslashIsUnavailableInBothPaths) {
    // The worker's reply has no escapes and no encoding, so the loader both paths share refuses a label it could
    // not carry: anything but printable ASCII 0x20-0x7E, and the double quote and backslash among those.
    REQUIRE_ONNX();
    namespace fs = std::filesystem;
    const fs::path tmp = fs::temp_directory_path() / "xpe_bodypart_quote_label";
    fs::remove_all(tmp);
    fs::create_directories(tmp);
    fs::copy_file(fs::path(Dir("models_bodypart_a")) / "bodypart.onnx", tmp / "bodypart.onnx");
    const Pix image = Flat(0.0f);
    const char* refused[] = {
        "{\"labels\": [\"CH\\\"EST\", \"ABDOMEN\", \"SPINE\"]}",        // a double quote
        "{\"labels\": [\"CH\\\\EST\", \"ABDOMEN\", \"SPINE\"]}",       // a backslash
        "{\"labels\": [\"CH\\u0007EST\", \"ABDOMEN\", \"SPINE\"]}",      // a control character (BEL)
        "{\"labels\": [\"CH\\u00c9ST\", \"ABDOMEN\", \"SPINE\"]}",       // a non-ASCII letter (U+00C9)
        "{\"labels\": [\"\\u80f8\", \"ABDOMEN\", \"SPINE\"]}",           // a non-ASCII label (U+80F8)
        "{\"labels\": [\"CHEST\\u007f\", \"ABDOMEN\", \"SPINE\"]}",      // DEL (0x7F)
    };
    for (const char* sidecar : refused) {
        {
            std::ofstream j(tmp / "bodypart.json");
            j << sidecar;
        }
        xpe_test::SignDir(tmp, "bodypart");   // QA-B-195: the sidecar changed, so the signature is renewed
        EXPECT_EQ("unavailable", DescribeWorker(tmp.string(), image)) << sidecar;
        EXPECT_EQ("unavailable", DescribeInProcess(tmp.string(), image)) << sidecar;
    }
    // The edges that ARE allowed: space (0x20) and tilde (0x7E).
    {
        std::ofstream j(tmp / "bodypart.json");
        j << "{\"labels\": [\"UPPER ARM~\", \"ABDOMEN\", \"SPINE\"]}";
    }
    xpe_test::SignDir(tmp, "bodypart");   // QA-B-195: the sidecar changed, so the signature is renewed
    EXPECT_EQ("ok:UPPER ARM~:0.6", DescribeWorker(tmp.string(), image));
    EXPECT_EQ("ok:UPPER ARM~:0.6", DescribeInProcess(tmp.string(), image));
    // The control: the same model with a plain label is usable, so the character is what made the difference.
    {
        std::ofstream j(tmp / "bodypart.json");
        j << "{\"labels\": [\"CHEST\", \"ABDOMEN\", \"SPINE\"]}";
    }
    xpe_test::SignDir(tmp, "bodypart");   // QA-B-195: the sidecar changed, so the signature is renewed
    EXPECT_EQ(0u, DescribeWorker(tmp.string(), image).rfind("ok:", 0));
    fs::remove_all(tmp);
}

// ===== bone suppression's ERROR frames: the SAME parser (QA-B-193) =========================================

namespace {
/** One BONE_SUPPRESS call to a fake worker that answers with @p json as an ERROR frame. */
struct BoneOutcome {
    XpeErrorCode rc;
    bool workerKept;
    bool unavailableFlagSeen;
};
BoneOutcome AskFakeBone(const std::string& json, uint32_t flags = 0) {
    FakeReply f("bone_error_raw", json, flags);
    WorkerSupervisor sup(FakeCfg());
    const float in[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    float out[9];
    for (float& v : out) v = -777.0f;
    BoneOutcome o{};
    o.rc = sup.BoneSuppress(3, 3, in, out);
    o.workerKept = sup.WorkerPid() != 0;
    o.unavailableFlagSeen = sup.LastModelUnavailable();   // the supervisor's last flag for this call (QA-B-195 D6: bone too)
    for (float v : out) EXPECT_EQ(-777.0f, v) << json << ": a failed call leaves the output untouched";
    return o;
}
}  // namespace

TEST(WorkerBoneErrorFrame, EveryFrameTheProtocolForbidsEverywhereIsAFaultForBoneSuppressionToo) {
    for (const auto& c : error_frame_cases::ForbiddenEverywhere()) {
        const BoneOutcome o = AskFakeBone(c.second);
        EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, o.rc) << c.first << ": " << c.second;
        EXPECT_FALSE(o.workerKept) << c.first << ": a worker that sent a forbidden ERROR frame was kept";
    }
}

// QA-B-195 D6: THE RULE THIS TEST USED TO GUARD CHANGED, because its ground changed. QA-B-193 held that bone suppression
// has no notion of "the model cannot be used", so "model_unavailable" in its ERROR frame was a protocol fault in any
// form. Signature verification gave bone suppression that notion (a model refused for its signature is unavailable,
// and must not count toward switching the worker off), so the flag is now believed for bone suppression under the SAME
// condition as for body-part requests. What the old test protected -- that a frame which contradicts itself or is
// malformed is never believed -- is still protected, for bone suppression too:
//   * a flag with any code other than -4 / -9, a non-boolean flag, a repeated flag: ForbiddenEverywhere() above runs on
//     bone suppression (EveryFrameTheProtocolForbidsEverywhereIsAFaultForBoneSuppressionToo) and must stay green;
//   * a flag that is a string: the third case below, a fault with the worker dropped.
TEST(WorkerBoneErrorFrame, TheUnavailableFlagIsBelievedForBoneSuppressUnderTheBodyPartRuleAndNothingElseChanged) {
    struct Case { const char* name; std::string json; XpeErrorCode rc; bool flag; };
    const std::vector<Case> believed = {
        {"true with IO_FAILED", R"({"error_code":-9,"model_unavailable":true,"error_message":"x"})", XPE_ERR_IO_FAILED, true},
        {"true with CONFIG_INVALID", R"({"error_code":-4,"model_unavailable":true})", XPE_ERR_CONFIG_INVALID, true},
        {"false", R"({"error_code":-9,"model_unavailable":false})", XPE_ERR_IO_FAILED, false},
    };
    for (const Case& c : believed) {
        const BoneOutcome o = AskFakeBone(c.json);
        EXPECT_EQ(c.rc, o.rc) << c.name << ": " << c.json;
        EXPECT_TRUE(o.workerKept) << c.name << ": a valid ERROR frame leaves the worker in place";
        EXPECT_EQ(c.flag, o.unavailableFlagSeen) << c.name;
    }
    const BoneOutcome bad = AskFakeBone(R"({"error_code":-9,"model_unavailable":"x"})");
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, bad.rc) << "a string flag is a fault";
    EXPECT_FALSE(bad.workerKept);
    EXPECT_FALSE(bad.unavailableFlagSeen) << "a flag from a frame that is a fault is never believed";
}

TEST(WorkerBoneErrorFrame, EveryFrameTheProtocolAllowsIsPassedThroughAndKeepsTheWorker) {
    struct Case { const char* name; std::string json; int code; };
    const std::vector<Case> allowed = {
        {"minimal", R"({"error_code":-9})", -9},
        {"with a message", R"({"error_code":-4,"error_message":"model unreadable: x"})", -4},
        {"a quote and a backslash in the message", R"({"error_code":-9,"error_message":"no model at C:\\m \"x\""})", -9},
        {"a key the protocol does not know", R"({"error_code":-3,"error_message":"x","detail":"y"})", -3},
        {"the largest code", R"({"error_code":-99})", -99},
        {"whitespace between tokens", R"( { "error_code" : -3 } )", -3},
    };
    for (const Case& c : allowed) {
        const BoneOutcome o = AskFakeBone(c.json);
        EXPECT_EQ(static_cast<XpeErrorCode>(c.code), o.rc) << c.name;
        EXPECT_TRUE(o.workerKept) << c.name << ": an ERROR frame the protocol allows leaves the worker in place";
        EXPECT_FALSE(o.unavailableFlagSeen) << c.name;
    }
}

TEST(WorkerBoneErrorFrame, TheRealWorkersOwnErrorFramesAreAcceptedAndKeepTheWorker) {
    // Compatibility: the frames the REAL worker sends for a bone request it cannot serve must pass the parser,
    // or every such answer would become a protocol fault and a restart. models_missing has no bone model (the
    // worker answers IO_FAILED, its message holds a path); models_broken has a file that is not a model
    // (CONFIG_INVALID). Two calls each: the same worker must serve the second.
    if (IsStub()) GTEST_SKIP() << "needs ONNX Runtime: the full build only";
    struct Case { const char* dir; XpeErrorCode code; };
    for (const Case c : {Case{"models_missing", XPE_ERR_IO_FAILED}, Case{"models_broken", XPE_ERR_CONFIG_INVALID}}) {
        WorkerSupervisorConfig cfg;
        cfg.worker_exe = XPE_AI_WORKER_EXE;
        cfg.model_dir = Dir(c.dir);
        cfg.timeout_ms = kBudgetMs;
        WorkerSupervisor sup(cfg);
        const float in[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
        float out[9];
        for (int i = 0; i < 2; ++i) {
            EXPECT_EQ(c.code, sup.BoneSuppress(3, 3, in, out)) << c.dir << " call " << i
                << ": the worker's own code, not a fault";
        }
        EXPECT_EQ(1u, sup.StartCount()) << c.dir << ": an error frame is a healthy answer, the worker is kept";
        EXPECT_NE(0u, sup.WorkerPid()) << c.dir;
    }
}

// ===== the binary-payload bit on an ERROR frame (QA-B-193b, Codex #79) =====================================

TEST(WorkerErrorFrameFlags, AnErrorFrameIsJsonOnlySoTheBinaryBitIsAFaultForBothRequestTypes) {
    // The JSON below is exactly the frame the unavailable-model rule believes -- with flags 0 it is accepted and
    // keeps the worker. The ONLY difference in the second call is the header bit.
    const std::string unavailable = R"({"error_code":-9,"model_unavailable":true,"error_message":"no model file"})";
    const Outcome control = AskFake("bodypart_error_raw", unavailable, 0);
    EXPECT_EQ(XPE_ERR_IO_FAILED, control.rc) << "the control: the frame is valid with flags 0";
    EXPECT_TRUE(control.unavailable);
    EXPECT_TRUE(control.workerKept);

    const Outcome bp = AskFake("bodypart_error_raw", unavailable, XPE_AI_FLAG_HAS_BINARY_PAYLOAD);
    EXPECT_EQ(XPE_ERR_IO_FAILED, bp.rc);
    EXPECT_FALSE(bp.unavailable) << "a frame already wrong in its header must not be believed";
    EXPECT_FALSE(bp.workerKept) << "protocol fault: the connection is dropped and the worker discarded";

    const std::string plain = R"({"error_code":-9,"error_message":"no model"})";
    const BoneOutcome boneControl = AskFakeBone(plain, 0);
    EXPECT_EQ(XPE_ERR_IO_FAILED, boneControl.rc) << "the control: the worker's own code with flags 0";
    EXPECT_TRUE(boneControl.workerKept);
    const BoneOutcome bone = AskFakeBone(plain, XPE_AI_FLAG_HAS_BINARY_PAYLOAD);
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, bone.rc) << "bone suppression's fault code, as for any unreadable ERROR frame";
    EXPECT_FALSE(bone.workerKept);
}

TEST(WorkerErrorFrameFlags, TheBinaryBitCombinedWithReservedBitsIsStillAFault) {
    const std::string json = R"({"error_code":-3,"error_message":"x"})";
    for (const uint32_t flags : {0x1u | 0x2u, 0x1u | 0x4u, 0x1u | 0x8u, 0x1u | 0x2u | 0x4u | 0x8u}) {
        EXPECT_FALSE(AskFake("bodypart_error_raw", json, flags).workerKept) << flags;
        EXPECT_FALSE(AskFakeBone(json, flags).workerKept) << flags;
    }
}

TEST(WorkerErrorFrameFlags, TheReservedBitsAreNotRefusedOnAnErrorFrame) {
    // 0x2, 0x4 and 0x8 are reserved and unused (QA-B-192): nothing reads them, no receiver rejects unknown bits.
    const std::string json = R"({"error_code":-3,"error_message":"x"})";
    for (const uint32_t flags : {0x2u, 0x4u, 0x8u, 0x2u | 0x4u | 0x8u}) {
        const Outcome bp = AskFake("bodypart_error_raw", json, flags);
        EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, bp.rc) << flags;
        EXPECT_TRUE(bp.workerKept) << flags;
        const BoneOutcome bone = AskFakeBone(json, flags);
        EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, bone.rc) << flags;
        EXPECT_TRUE(bone.workerKept) << flags;
    }
}
