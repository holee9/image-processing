/**
 * @file test_ai_model_sidecar.cpp
 * @brief The model sidecar must say what REQ-AI-008 requires, or the model is refused (QA-B-197 M1, T-005a).
 *
 * REQ-AI-008: "Model versioning shall follow semver; model metadata shall include: model_id, version, pccp_scope,
 * training_data_hash, validation_metrics." Three layers are held here:
 *   1. the parser (ParseModelSidecar): every required field, every type and format rule, each with the reason that is true;
 *   2. loading (OnnxSession::Create): a model whose sidecar was re-signed WITHOUT one required field is refused, with
 *      kSidecarInvalid -- and a sidecar that does not verify is refused for its SIGNATURE first, never parsed;
 *   3. the C ABI: the refusal behaves like a refused signature (-4, nothing loaded, one Error alert per session per
 *      role, never a worker failure) with its own alert text, and a repaired sidecar is seen at once.
 */

#include <gtest/gtest.h>

#include "ai_model_sidecar.h"
#include "test_signing_helper.h"
#include "xpe/ai/ai_api.h"
#include "xpe/ai/ai_onnx_session.h"
#include "xpe/common/xpe_error.h"

#include <windows.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#ifndef XPE_AI_TEST_DATA_DIR
#error "XPE_AI_TEST_DATA_DIR must be defined by the build (modules/ai/CMakeLists.txt)"
#endif

namespace {

namespace fs = std::filesystem;
using xpe::ai::ModelSidecar;
using xpe::ai::OnnxErrorCode;
using xpe::ai::OnnxSession;
using xpe::ai::OnnxSessionConfig;
using xpe::ai::ParseModelSidecar;

const std::string kData = XPE_AI_TEST_DATA_DIR;

using KV = std::vector<std::pair<std::string, std::string>>;   // key -> raw JSON value

KV Base() {
    return {{"model_id", "\"toy_model\""},
            {"version", "\"0.0.1\""},
            {"pccp_scope", "\"none\""},
            {"training_data_hash", "\"none\""},
            {"validation_metrics", "{\"m\":0}"}};
}
KV Without(KV kv, const std::string& key) {
    for (size_t i = 0; i < kv.size(); ++i) {
        if (kv[i].first == key) {
            kv.erase(kv.begin() + static_cast<std::ptrdiff_t>(i));
            break;
        }
    }
    return kv;
}
KV With(KV kv, const std::string& key, const std::string& raw) {
    for (auto& p : kv) {
        if (p.first == key) {
            p.second = raw;
            return kv;
        }
    }
    kv.emplace_back(key, raw);
    return kv;
}
std::string Json(const KV& kv) {
    std::string s = "{";
    for (size_t i = 0; i < kv.size(); ++i) {
        if (i) s += ",";
        s += "\"" + kv[i].first + "\":" + kv[i].second;
    }
    return s + "}";
}

/** Parse; returns "" on success, otherwise the reason. */
std::string Reason(const std::string& text) {
    ModelSidecar sc;
    std::string reason;
    return ParseModelSidecar(&text, &sc, &reason) ? std::string() : reason;
}

struct TempDir {
    fs::path path;
    explicit TempDir(const char* name) {
        path = fs::temp_directory_path() / (std::string("xpe_sidecar_") + name + "_" + std::to_string(GetCurrentProcessId()));
        std::error_code ec;
        fs::remove_all(path, ec);
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    /** A copy of tests/data/models_x2 (a signed bone model with a valid sidecar). */
    void CopyBone() const {
        for (const auto& e : fs::directory_iterator(fs::path(kData) / "models_x2")) {
            fs::copy_file(e.path(), path / e.path().filename(), fs::copy_options::overwrite_existing);
        }
    }
    fs::path operator/(const char* name) const { return path / name; }
};

void WriteText(const fs::path& p, const std::string& text) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << text;
}

OnnxErrorCode LoadBone(const fs::path& dir, std::string* message = nullptr) {
    OnnxSessionConfig c;
    c.model_path = (dir / "bone_suppress.onnx").string();
    c.role = "bone_suppress";
    const auto r = OnnxSession::Create(c);
    if (message) *message = r.message;
    return r.code;
}

std::vector<std::string> AlertTexts(std::vector<int32_t>* severities = nullptr) {
    std::vector<std::string> out;
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < n; ++i) {
        char msg[512] = {0};
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) == XPE_OK) {
            out.push_back(msg);
            if (severities) severities->push_back(sev);
        }
    }
    return out;
}

XpeErrorCode BoneCall(std::vector<float>* out) {
    std::vector<float> in(9, 1.0f);
    out->assign(9, -777.0f);
    XpeImageBuffer ib{}, ob{};
    ib.width = ib.height = ob.width = ob.height = 3;
    ib.bitsAllocated = ib.bitsStored = ob.bitsAllocated = ob.bitsStored = 32;
    ib.format = ob.format = XPE_PIXEL_FLOAT32;
    ib.data = in.data();
    ob.data = out->data();
    ib.dataSize = ob.dataSize = in.size() * sizeof(float);
    return xpe_bone_suppress(&ib, &ob, nullptr);
}

}  // namespace

/* =========================================================================
 * 1. The parser
 * ========================================================================= */

TEST(ModelSidecarParser, ControlTheFiveRequiredFieldsAloneAreAValidSidecar) {
    ModelSidecar sc;
    std::string reason;
    const std::string text = Json(Base());
    ASSERT_TRUE(ParseModelSidecar(&text, &sc, &reason)) << reason;
    EXPECT_EQ("toy_model", sc.model_id);
    EXPECT_EQ("0.0.1", sc.version);
    EXPECT_EQ("none", sc.pccp_scope);
    EXPECT_EQ("none", sc.training_data_hash);
    EXPECT_EQ("{\"m\":0}", sc.validation_metrics_json);
    EXPECT_FALSE(sc.has_intended_use || sc.has_training_data_summary || sc.has_demographic_performance ||
                 sc.has_limitations || sc.has_published_date)
        << "an optional field the sidecar does not carry is absent, never filled in";
}

TEST(ModelSidecarParser, EveryRequiredFieldMissingIsARefusalThatNamesTheField) {
    for (const char* key : {"model_id", "version", "pccp_scope", "training_data_hash", "validation_metrics"}) {
        const std::string reason = Reason(Json(Without(Base(), key)));
        EXPECT_NE(std::string::npos, reason.find(std::string("required field ") + key + " is missing"))
            << key << ": " << reason;
    }
}

TEST(ModelSidecarParser, ARequiredFieldOfTheWrongTypeOrEmptyIsARefusalThatNamesTheField) {
    struct Case { const char* key; const char* raw; const char* needle; };
    const std::vector<Case> cases = {
        {"model_id", "5", "model_id is not a string"},
        {"model_id", "null", "model_id is not a string"},
        {"model_id", "\"\"", "model_id is not 1 to 64"},
        {"model_id", "\"has space\"", "model_id is not 1 to 64"},
        {"model_id", "\"quo\\\"te\"", "model_id is not 1 to 64"},
        {"model_id", "\"back\\\\slash\"", "model_id is not 1 to 64"},
        {"version", "1", "version is not a string"},
        {"version", "\"1.0\"", "version is not a semantic version"},
        {"pccp_scope", "[]", "pccp_scope is not a string"},
        {"pccp_scope", "\"\"", "pccp_scope is empty"},
        {"training_data_hash", "true", "training_data_hash is not a string"},
        {"training_data_hash", "\"\"", "training_data_hash is empty"},
        {"validation_metrics", "\"x\"", "validation_metrics is not a JSON object"},
        {"validation_metrics", "[1]", "validation_metrics is not a JSON object"},
        {"validation_metrics", "null", "validation_metrics is not a JSON object"},
        {"validation_metrics", "{}", "validation_metrics is empty"},
    };
    for (const Case& c : cases) {
        const std::string reason = Reason(Json(With(Base(), c.key, c.raw)));
        EXPECT_NE(std::string::npos, reason.find(c.needle)) << c.key << " = " << c.raw << ": " << reason;
    }
    // the model id has a length limit of 64
    EXPECT_EQ("", Reason(Json(With(Base(), "model_id", "\"" + std::string(64, 'a') + "\""))));
    EXPECT_NE("", Reason(Json(With(Base(), "model_id", "\"" + std::string(65, 'a') + "\""))));
}

TEST(ModelSidecarParser, TheVersionMustBeASemanticVersion) {
    for (const char* ok : {"0.0.1", "1.2.3", "10.20.30", "1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-0.3.7", "1.0.0-x.7.z.92",
                           "1.0.0+20130313144700", "1.0.0-beta+exp.sha.5114f85", "1.2.3-rc.1+build.7", "0.0.0"}) {
        EXPECT_TRUE(xpe::ai::IsSemverText(ok)) << ok;
        EXPECT_EQ("", Reason(Json(With(Base(), "version", std::string("\"") + ok + "\"")))) << ok;
    }
    for (const char* bad : {"", "1", "1.2", "1.2.3.4", "01.2.3", "1.02.3", "1.2.03", "1.2.3-", "1.2.3-01", "1.2.3-a..b",
                            "1.2.3-.a", "1.2.3+", "1.2.3+a..b", "v1.2.3", " 1.2.3", "1.2.3 ", "-1.2.3", "1.2.-3",
                            "1.2.3-a_b", "1.2.3-\xCE\xB1", "1.2.3++x", "a.b.c", "1.2.3.", ".1.2.3", "1,2,3"}) {
        EXPECT_FALSE(xpe::ai::IsSemverText(bad)) << "'" << bad << "'";
    }
}

TEST(ModelSidecarParser, TheOptionalFieldsOfTheCardAreReadAndTheirTypesAreEnforced) {
    KV all = Base();
    all = With(all, "intended_use", "\"use\"");
    all = With(all, "training_data_summary", "\"summary\"");
    all = With(all, "demographic_performance", "{\"overall\":{\"n\":1}}");
    all = With(all, "limitations", "\"limits\"");
    all = With(all, "published_date", "\"2024-02-29\"");
    ModelSidecar sc;
    std::string reason;
    const std::string text = Json(all);
    ASSERT_TRUE(ParseModelSidecar(&text, &sc, &reason)) << reason;
    EXPECT_TRUE(sc.has_intended_use && sc.has_training_data_summary && sc.has_demographic_performance &&
                sc.has_limitations && sc.has_published_date);
    EXPECT_EQ("use", sc.intended_use);
    EXPECT_EQ("summary", sc.training_data_summary);
    EXPECT_EQ("{\"overall\":{\"n\":1}}", sc.demographic_performance_json);
    EXPECT_EQ("limits", sc.limitations);
    EXPECT_EQ("2024-02-29", sc.published_date);

    struct Case { const char* key; const char* raw; const char* needle; };
    for (const Case& c : std::vector<Case>{
             {"intended_use", "5", "intended_use is not a string"},
             {"training_data_summary", "[]", "training_data_summary is not a string"},
             {"limitations", "{}", "limitations is not a string"},
             {"demographic_performance", "[]", "demographic_performance is not a JSON object"},
             {"demographic_performance", "\"x\"", "demographic_performance is not a JSON object"},
             {"published_date", "20261003", "published_date is not a string"},
         }) {
        const std::string r = Reason(Json(With(Base(), c.key, c.raw)));
        EXPECT_NE(std::string::npos, r.find(c.needle)) << c.key << " = " << c.raw << ": " << r;
    }
}

TEST(ModelSidecarParser, ThePublishedDateMustBeACalendarDate) {
    for (const char* ok : {"2026-10-03", "2024-02-29", "2000-02-29", "1999-12-31", "2026-01-01"}) {
        EXPECT_TRUE(xpe::ai::IsIsoDateText(ok)) << ok;
    }
    for (const char* bad : {"2026-02-29", "1900-02-29", "2026-13-01", "2026-00-10", "2026-10-32", "2026-10-00", "26-10-03",
                            "2026/10/03", "2026-1-3", "", "2026-10-03T00:00", "2026-04-31", "abcd-ef-gh", "2026-10-3 "}) {
        EXPECT_FALSE(xpe::ai::IsIsoDateText(bad)) << "'" << bad << "'";
    }
}

TEST(ModelSidecarParser, ATextThatIsNotAJsonObjectOrIsTooLargeOrAbsentIsRefusedWithItsOwnReason) {
    EXPECT_NE(std::string::npos, Reason("").find("not valid JSON"));
    EXPECT_NE(std::string::npos, Reason("{bad").find("not valid JSON"));
    EXPECT_NE(std::string::npos, Reason("[]").find("not a JSON object"));
    EXPECT_NE(std::string::npos, Reason("5").find("not a JSON object"));
    EXPECT_NE(std::string::npos, Reason("null").find("not a JSON object"));
    EXPECT_NE(std::string::npos, Reason("\"x\"").find("not a JSON object"));
    EXPECT_NE(std::string::npos, Reason(std::string(xpe::ai::kMaxSidecarBytes + 1, ' ')).find("too large"));
    ModelSidecar sc;
    std::string reason;
    EXPECT_FALSE(ParseModelSidecar(nullptr, &sc, &reason));
    EXPECT_NE(std::string::npos, reason.find("no sidecar file")) << reason;
}

TEST(ModelSidecarParser, ANestedObjectIsCarriedIntoTheCardAsItsCompactText) {
    // The sidecar is read without building a JSON document (see ai_model_sidecar.h), so nested values are rebuilt from the
    // parser's events: commas, nesting, escapes, number literals and non-ASCII text must come out as they went in.
    const std::string metrics =
        "{\"a\":[1,2.50,{\"b\":null}],\"c\":\"x\\\"y\\\\z\",\"d\":true,\"e\":-3,\"f\":1e3,\"g\":{},\"h\":[],\"i\":\"\xEA\xB0\x80\"}";
    ModelSidecar sc;
    std::string reason;
    const std::string text = Json(With(With(Base(), "validation_metrics", metrics), "demographic_performance",
                                       "{ \"x\" : [ 1 , 2 ] }"));
    ASSERT_TRUE(ParseModelSidecar(&text, &sc, &reason)) << reason;
    EXPECT_EQ(metrics, sc.validation_metrics_json);
    EXPECT_EQ("{\"x\":[1,2]}", sc.demographic_performance_json) << "whitespace is not carried";
}

TEST(ModelSidecarParser, ATopLevelKeyThatAppearsTwiceIsRefusedWhateverItsValue) {
    EXPECT_NE(std::string::npos,
              Reason("{\"model_id\":\"a\",\"model_id\":\"b\",\"version\":\"0.0.1\",\"pccp_scope\":\"n\","
                     "\"training_data_hash\":\"n\",\"validation_metrics\":{\"m\":0}}")
                  .find("key model_id appears more than once"));
    // a key this file does not name counts too: the signed text would say two things
    KV kv = Base();
    kv.emplace_back("labels", "[\"A\"]");
    kv.emplace_back("labels", "[\"B\"]");
    EXPECT_NE(std::string::npos, Reason(Json(kv)).find("key labels appears more than once"));
    // the same name NESTED is not a top-level duplicate
    EXPECT_EQ("", Reason(Json(With(Base(), "validation_metrics", "{\"m\":0,\"x\":{\"m\":1},\"y\":{\"m\":2}}"))));
}

TEST(ModelSidecarParser, AGarbageOrTruncatedTextNeverThrowsAndIsRefused) {
    for (const char* bad : {"{", "{\"model_id\"", "{\"model_id\":", "{\"model_id\":\"a\"", "{\"a\":[1,2", "{\"a\":}", "{,}",
                            "{\"a\":1,}", "ï»¿{}", "{\"a\":\"Ã\"}", "nul", "{\"a\":01}", "{} x"}) {
        EXPECT_NE("", Reason(bad)) << "'" << bad << "'";
    }
    std::string deep(200000, '[');
    EXPECT_NE("", Reason(deep)) << "a deeply nested non-object is refused, not recursed into";
    std::string deepMember = "{\"validation_metrics\":" + std::string(20000, '[') + std::string(20000, ']') + "}";
    EXPECT_NE("", Reason(deepMember)) << "a deeply nested member is parsed without recursion and refused for its type";
}

TEST(ModelSidecarParser, KeysTheParserDoesNotNameAreNotJudgedHere) {
    // `labels` belongs to the body-part reader and `note` to nobody: neither may make the sidecar invalid.
    KV kv = With(With(Base(), "labels", "[\"CHEST\"]"), "note", "\"x\"");
    kv = With(kv, "some_future_key", "{\"a\":[1,2,3]}");
    EXPECT_EQ("", Reason(Json(kv)));
}

/* =========================================================================
 * 2. Loading: OnnxSession::Create
 * ========================================================================= */

TEST(ModelSidecarLoading, ControlTheShippedTestModelLoadsWithItsSidecar) {
    const TempDir t("control");
    t.CopyBone();
    EXPECT_EQ(OnnxErrorCode::kOk, LoadBone(t.path));
    const auto r = OnnxSession::Create([&] {
        OnnxSessionConfig c;
        c.model_path = (t / "bone_suppress.onnx").string();
        return c;
    }());
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ("bone_toy_x2", r.value->GetModelMetadata().model_id) << "the metadata comes from the verified sidecar";
    EXPECT_EQ("0.0.1", r.value->GetModelMetadata().version);
    EXPECT_EQ("none: toy model for wiring tests", r.value->GetModelMetadata().pccp_scope);
}

TEST(ModelSidecarLoading, AModelReSignedWithoutOneRequiredFieldIsRefusedWithTheFieldNamed) {
    for (const char* key : {"model_id", "version", "pccp_scope", "training_data_hash", "validation_metrics"}) {
        const TempDir t("missing_field");
        t.CopyBone();
        WriteText(t / "bone_suppress.json", Json(Without(Base(), key)));
        ASSERT_TRUE(xpe_test::SignDir(t.path, "bone_suppress"));   // a CURRENT signature: the signature is not the problem
        std::string message;
        EXPECT_EQ(OnnxErrorCode::kSidecarInvalid, LoadBone(t.path, &message)) << key;
        EXPECT_NE(std::string::npos, message.find(std::string("required field ") + key + " is missing")) << message;
        EXPECT_NE(std::string::npos, message.find("model sidecar check failed")) << message;
    }
}

TEST(ModelSidecarLoading, AModelWithNoSidecarAtAllIsRefused) {
    const TempDir t("no_sidecar");
    t.CopyBone();
    fs::remove(t / "bone_suppress.json");
    ASSERT_TRUE(xpe_test::SignDir(t.path, "bone_suppress"));   // signed as "no sidecar": the signature verifies
    std::string message;
    EXPECT_EQ(OnnxErrorCode::kSidecarInvalid, LoadBone(t.path, &message));
    EXPECT_NE(std::string::npos, message.find("no sidecar file")) << message;
}

TEST(ModelSidecarLoading, AnInvalidVersionOrInvalidJsonOrAWrongTypeIsRefusedAfterAValidSignature) {
    struct Case { const char* name; std::string text; const char* needle; };
    const std::vector<Case> cases = {
        {"bad semver", Json(With(Base(), "version", "\"1.0\"")), "semantic version"},
        {"not JSON", "{this is not json", "not valid JSON"},
        {"an array", "[1,2]", "not a JSON object"},
        {"wrong type", Json(With(Base(), "validation_metrics", "5")), "not a JSON object"},
        {"bad date", Json(With(Base(), "published_date", "\"2026-02-30\"")), "calendar date"},
    };
    for (const Case& c : cases) {
        const TempDir t("invalid_sidecar");
        t.CopyBone();
        WriteText(t / "bone_suppress.json", c.text);
        ASSERT_TRUE(xpe_test::SignDir(t.path, "bone_suppress"));
        std::string message;
        EXPECT_EQ(OnnxErrorCode::kSidecarInvalid, LoadBone(t.path, &message)) << c.name;
        EXPECT_NE(std::string::npos, message.find(c.needle)) << c.name << ": " << message;
    }
}

TEST(ModelSidecarLoading, ASidecarThatDoesNotVerifyIsRefusedForItsSignatureAndNeverParsed) {
    // The sidecar is garbage AND the signature is stale. The refusal must be the SIGNATURE's: the sidecar of a model
    // that is not trusted is never looked at, so its content cannot be a way in.
    const TempDir t("verify_first");
    t.CopyBone();
    WriteText(t / "bone_suppress.json", "{this is not json");
    std::string message;
    EXPECT_EQ(OnnxErrorCode::kModelNotTrusted, LoadBone(t.path, &message));
    EXPECT_NE(std::string::npos, message.find("signature check failed")) << message;
    EXPECT_EQ(std::string::npos, message.find("sidecar check failed")) << message;
}

TEST(ModelSidecarLoading, EveryShippedTestModelThatHasASidecarHasAValidOne) {
    // Every tests/data sidecar passes the parser: the fixtures are the sidecars the module is held to.
    size_t checked = 0;
    for (const auto& e : fs::recursive_directory_iterator(fs::path(kData))) {
        if (!e.is_regular_file() || e.path().extension() != ".json") continue;
        if (e.path().parent_path().filename() == "signing") continue;
        std::ifstream f(e.path(), std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        EXPECT_EQ("", Reason(text)) << e.path().string();
        ++checked;
    }
    EXPECT_GE(checked, 20u) << "the fixtures were found (control)";
}

/* =========================================================================
 * 3. The C ABI: the refusal behaves like a refused signature, with its own text
 * ========================================================================= */

TEST(ModelSidecarRefusal, ARefusedBoneSidecarIsMinusFourWithOneErrorAlertPerSessionAndTheReason) {
    if (OnnxSession::IsStubBuild()) GTEST_SKIP() << "stub build: xpe_bone_suppress answers before it looks for a model";
    const TempDir t("abi_bone");
    t.CopyBone();
    WriteText(t / "bone_suppress.json", Json(Without(Base(), "pccp_scope")));
    ASSERT_TRUE(xpe_test::SignDir(t.path, "bone_suppress"));
    for (int session = 0; session < 2; ++session) {
        xpe_ai_shutdown();
        xpe_clear_alerts();
        ASSERT_EQ(XPE_OK, xpe_ai_init(t.path.string().c_str(), "{}"));
        for (int i = 0; i < 4; ++i) {
            std::vector<float> out;
            EXPECT_EQ(XPE_ERR_CONFIG_INVALID, BoneCall(&out)) << "session " << session << " call " << i;
            for (const float v : out) EXPECT_EQ(-777.0f, v) << "the output is untouched";
        }
        std::vector<int32_t> sev;
        const std::vector<std::string> a = AlertTexts(&sev);
        ASSERT_EQ(1u, a.size()) << "session " << session << ": one alert for four refused calls";
        EXPECT_EQ(XPE_ALERT_ERROR, sev[0]);
        EXPECT_NE(std::string::npos, a[0].find("bone suppression")) << a[0];
        EXPECT_NE(std::string::npos, a[0].find("sidecar failed the metadata check")) << a[0];
        EXPECT_NE(std::string::npos, a[0].find("required field pccp_scope is missing")) << a[0];
        EXPECT_NE(std::string::npos, a[0].find("REQ-AI-008")) << a[0];
    }
    xpe_ai_shutdown();
    xpe_clear_alerts();
}

TEST(ModelSidecarRefusal, ARepairedSidecarIsSeenAtOnceInTheSameSession) {
    if (OnnxSession::IsStubBuild()) GTEST_SKIP() << "stub build: xpe_bone_suppress answers before it looks for a model";
    const TempDir t("abi_repair");
    t.CopyBone();
    WriteText(t / "bone_suppress.json", Json(Without(Base(), "version")));
    ASSERT_TRUE(xpe_test::SignDir(t.path, "bone_suppress"));
    xpe_ai_shutdown();
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_ai_init(t.path.string().c_str(), "{}"));
    std::vector<float> out;
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, BoneCall(&out));
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, BoneCall(&out)) << "the second call answers from the memo";
    const auto before = fs::last_write_time(t / "bone_suppress.json");
    WriteText(t / "bone_suppress.json", Json(Base()));
    fs::last_write_time(t / "bone_suppress.json", before + std::chrono::hours(1));
    ASSERT_TRUE(xpe_test::SignDir(t.path, "bone_suppress"));
    EXPECT_EQ(XPE_OK, BoneCall(&out)) << "a repaired sidecar must be seen at once, not after a restart";
    for (const float v : out) EXPECT_EQ(2.0f, v);
    xpe_ai_shutdown();
    xpe_clear_alerts();
}

TEST(ModelSidecarRefusal, ASidecarRefusalOfBoneSuppressionThroughTheWorkerIsNeverAWorkerFailure) {
    if (OnnxSession::IsStubBuild()) GTEST_SKIP() << "stub build: no worker runs a model";
    const TempDir t("abi_worker");
    t.CopyBone();
    WriteText(t / "bone_suppress.json", Json(With(Base(), "version", "\"1.0\"")));
    ASSERT_TRUE(xpe_test::SignDir(t.path, "bone_suppress"));
    xpe_ai_shutdown();
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_ai_init(t.path.string().c_str(), "{\"use_worker\": true}"));
    for (int i = 0; i < 5; ++i) {   // past the ceiling of 3
        std::vector<float> out;
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, BoneCall(&out)) << "call " << i;
        for (const float v : out) EXPECT_EQ(1.0f, v) << "the worker path returns the input unchanged";
        int32_t state = -1;
        uint32_t failures = 777;
        ASSERT_EQ(XPE_OK, xpe_ai_worker_state(&state, &failures, nullptr));
        EXPECT_EQ(XPE_AI_WORKER_ACTIVE, state) << "call " << i;
        EXPECT_EQ(0u, failures) << "call " << i << ": an invalid sidecar is 'unavailable', not a worker failure";
    }
    std::vector<int32_t> sev;
    const std::vector<std::string> a = AlertTexts(&sev);
    ASSERT_EQ(1u, a.size()) << "one Error alert, and no 'AI worker failed' Warnings";
    EXPECT_EQ(XPE_ALERT_ERROR, sev[0]);
    EXPECT_NE(std::string::npos, a[0].find("bone suppression")) << a[0];
    EXPECT_NE(std::string::npos, a[0].find("REQ-AI-008")) << a[0];
    xpe_ai_shutdown();
    xpe_clear_alerts();
}

TEST(ModelSidecarRefusal, ARefusedBodyPartSidecarIsUnknownWithOneErrorAlertOnTheInProcessPath) {
    if (OnnxSession::IsStubBuild()) GTEST_SKIP() << "stub build: xpe_bodypart_recognize answers before it looks for a model";
    const TempDir t("abi_part");
    for (const auto& e : fs::directory_iterator(fs::path(kData) / "models_bodypart_a")) {
        fs::copy_file(e.path(), t.path / e.path().filename(), fs::copy_options::overwrite_existing);
    }
    // remove one required field from a sidecar that is otherwise the toy one
    WriteText(t / "bodypart.json", "{\"model_id\":\"bodypart_toy\",\"version\":\"0.0.1\",\"pccp_scope\":\"none\","
                                   "\"validation_metrics\":{\"m\":0},\"labels\":[\"CHEST\",\"ABDOMEN\",\"SPINE\"]}");
    ASSERT_TRUE(xpe_test::SignDir(t.path, "bodypart"));
    xpe_ai_shutdown();
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_ai_init(t.path.string().c_str(), "{}"));
    std::vector<float> px(16, 0.0f);
    XpeImageBuffer ib{};
    ib.width = ib.height = 4;
    ib.bitsAllocated = ib.bitsStored = 32;
    ib.format = XPE_PIXEL_FLOAT32;
    ib.data = px.data();
    ib.dataSize = px.size() * sizeof(float);
    for (int i = 0; i < 3; ++i) {
        char label[64];
        float conf = -1.0f;
        EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, xpe_bodypart_recognize(&ib, label, sizeof(label), &conf));
        EXPECT_STREQ("UNKNOWN", label);
    }
    std::vector<int32_t> sev;
    const std::vector<std::string> a = AlertTexts(&sev);
    ASSERT_EQ(1u, a.size());
    EXPECT_EQ(XPE_ALERT_ERROR, sev[0]);
    EXPECT_NE(std::string::npos, a[0].find("body-part recognition")) << a[0];
    EXPECT_NE(std::string::npos, a[0].find("required field training_data_hash is missing")) << a[0];
    xpe_ai_shutdown();
    xpe_clear_alerts();
}

TEST(ModelSidecarRefusal, ASecondRefusalOfTheSameRoleInTheSameSessionRaisesNoSecondAlert) {
    // The memo keeps ONE refusal from being judged twice; what keeps a refusal that CHANGED from alerting again is the
    // once-per-role flag. The files change (a different field is missing now, then the signature goes stale): every call
    // is still refused, and the operator was told once. The flag is shared by the two checks.
    if (OnnxSession::IsStubBuild()) GTEST_SKIP() << "stub build: xpe_bone_suppress answers before it looks for a model";
    const TempDir t("abi_once");
    t.CopyBone();
    WriteText(t / "bone_suppress.json", Json(Without(Base(), "version")));
    ASSERT_TRUE(xpe_test::SignDir(t.path, "bone_suppress"));
    xpe_ai_shutdown();
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_ai_init(t.path.string().c_str(), "{}"));
    std::vector<float> out;
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, BoneCall(&out));
    ASSERT_EQ(1u, AlertTexts().size()) << "the first refusal alerts";

    auto bump = [&] {
        fs::last_write_time(t / "bone_suppress.json", fs::last_write_time(t / "bone_suppress.json") + std::chrono::hours(1));
    };
    WriteText(t / "bone_suppress.json", Json(Without(Base(), "pccp_scope")));   // a different reason
    bump();
    ASSERT_TRUE(xpe_test::SignDir(t.path, "bone_suppress"));
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, BoneCall(&out)) << "still refused, now for another field";
    EXPECT_EQ(1u, AlertTexts().size()) << "...and still one alert for the role";

    WriteText(t / "bone_suppress.json", Json(Base()));   // valid again, but the signature is not renewed: stale
    bump();
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, BoneCall(&out)) << "now refused for its SIGNATURE";
    EXPECT_EQ(1u, AlertTexts().size()) << "...the flag is shared by the signature check and the sidecar check";
    xpe_ai_shutdown();
    xpe_clear_alerts();
}
