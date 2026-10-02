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
    FakeReply(const char* mode, const std::string& json) {
        _putenv_s("XPE_FAKE_WORKER_MODE", mode);
        _putenv_s("XPE_FAKE_WORKER_JSON", json.c_str());
    }
    ~FakeReply() {
        _putenv_s("XPE_FAKE_WORKER_MODE", "");
        _putenv_s("XPE_FAKE_WORKER_JSON", "");
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
Outcome AskFake(const char* mode, const std::string& json) {
    FakeReply f(mode, json);
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

TEST(WorkerBodyPartReply, TheFlagCountsOnlyBeforeTheFreeTextMessage) {
    // The message is the model's own text; a flag that follows it could have come from that text.
    const Outcome after = AskFake("bodypart_error_raw",
                                  R"({"error_code":-3,"error_message":"x","model_unavailable":true})");
    EXPECT_FALSE(after.unavailable);
    const Outcome before = AskFake("bodypart_error_raw",
                                   R"({"error_code":-3,"model_unavailable":true,"error_message":"x"})");
    EXPECT_TRUE(before.unavailable) << "the control: the same two keys in the other order";
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
        EXPECT_EQ("unavailable", DescribeWorker(tmp.string(), image)) << sidecar;
        EXPECT_EQ("unavailable", DescribeInProcess(tmp.string(), image)) << sidecar;
    }
    // The edges that ARE allowed: space (0x20) and tilde (0x7E).
    {
        std::ofstream j(tmp / "bodypart.json");
        j << "{\"labels\": [\"UPPER ARM~\", \"ABDOMEN\", \"SPINE\"]}";
    }
    EXPECT_EQ("ok:UPPER ARM~:0.6", DescribeWorker(tmp.string(), image));
    EXPECT_EQ("ok:UPPER ARM~:0.6", DescribeInProcess(tmp.string(), image));
    // The control: the same model with a plain label is usable, so the character is what made the difference.
    {
        std::ofstream j(tmp / "bodypart.json");
        j << "{\"labels\": [\"CHEST\", \"ABDOMEN\", \"SPINE\"]}";
    }
    EXPECT_EQ(0u, DescribeWorker(tmp.string(), image).rfind("ok:", 0));
    fs::remove_all(tmp);
}
