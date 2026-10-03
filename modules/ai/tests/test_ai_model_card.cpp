/**
 * @file test_ai_model_card.cpp
 * @brief The model card is made from the verified sidecar of a verified model and from nothing else (QA-B-197 M2, T-005b).
 *
 * REQ-AI-010: xpe_ai_get_model_card returns JSON with intended_use, training_data_summary, demographic_performance,
 *             limitations, model_version, pccp_status, published_date.
 * REQ-AI-011: the JSON conforms to a schema (tests/data/schemas/model-card.schema.json).
 * REQ-AI-008: model_id, version, pccp_scope, training_data_hash, validation_metrics.
 *
 * WHAT CHANGED, AND WHY THESE TESTS CHANGED WITH IT (QA-B-197). Until now the card was a table of constants: four
 * hard-coded model ids answered with "0.1.0-stub", "N/A (stub)" and a published_date of 2026-04-22, whether or not any
 * model existed, and these tests asserted that the constants were there. They passed for a card that said nothing true
 * about any model -- and a real model would have been described as a stub. The card now comes only from the sidecar of a
 * model that passed the checks a load applies (signature, then REQ-AI-008). The tests below assert that:
 *   "card == the sidecar's content" (compared with an independent reading of the same file), "no model -> no card
 *   (-4)", "no constant of the module in a card", "a field the sidecar does not carry is null", plus the buffer rules,
 *   which did not change.
 */

#include "json_schema_mini.h"
#include "test_signing_helper.h"
#include "xpe/ai/ai_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#ifndef XPE_AI_TEST_DATA_DIR
#error "XPE_AI_TEST_DATA_DIR must be defined by the build (modules/ai/CMakeLists.txt)"
#endif

namespace {

namespace fs = std::filesystem;
using nlohmann::json;

const std::string kData = XPE_AI_TEST_DATA_DIR;

std::string ReadText(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

void WriteText(const fs::path& p, const std::string& text) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << text;
}

struct TempDir {
    fs::path path;
    explicit TempDir(const char* name) {
        path = fs::temp_directory_path() / (std::string("xpe_card_") + name + "_" + std::to_string(GetCurrentProcessId()));
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
    /** A copy of tests/data/<dirName>. */
    void CopyFrom(const char* dirName) const {
        for (const auto& e : fs::directory_iterator(fs::path(kData) / dirName)) {
            fs::copy_file(e.path(), path / e.path().filename(), fs::copy_options::overwrite_existing);
        }
    }
};

/** A sidecar whose five required fields are valid, with @p modelId; @p extra is more JSON members (without a leading comma). */
std::string SidecarText(const std::string& modelId, const std::string& extra = std::string()) {
    return "{\"model_id\":" + json(modelId).dump() +
           ",\"version\":\"1.0.0\",\"pccp_scope\":\"none\",\"training_data_hash\":\"none\",\"validation_metrics\":{\"m\":1}" +
           (extra.empty() ? "" : "," + extra) + "}";
}

struct Card {
    XpeErrorCode rc;
    std::string text;
};
Card GetCard(const char* id, size_t bufSize = 16384) {
    std::vector<char> buf(bufSize, 'x');
    Card c;
    c.rc = xpe_ai_get_model_card(id, buf.data(), buf.size());
    c.text = std::string(buf.data(), strnlen(buf.data(), buf.size()));
    return c;
}

json Parsed(const Card& c) {
    json j = json::parse(c.text, nullptr, false);
    EXPECT_FALSE(j.is_discarded()) << "the card is not valid JSON: " << c.text;
    return j;
}

}  // namespace

class AiModelCardTest : public ::testing::Test {
protected:
    void SetUp() override {
        xpe_ai_shutdown();
        xpe_clear_alerts();
    }
    void TearDown() override {
        xpe_ai_shutdown();
        xpe_clear_alerts();
    }
    static void Init(const std::string& dir) { ASSERT_EQ(XPE_OK, xpe_ai_init(dir.c_str(), nullptr)); }
    static std::string Data(const char* dir) { return kData + "/" + dir; }
};

/* ============================================================================
 * The card IS the sidecar
 * ============================================================================ */

TEST_F(AiModelCardTest, TheCardOfAVerifiedModelCarriesWhatItsSidecarSaysAndNothingElse) {
    Init(Data("models_card_full"));
    const Card c = GetCard("bone_card_full_toy");
    ASSERT_EQ(XPE_OK, c.rc);
    const json card = Parsed(c);
    // The expected values come from reading the SAME file with a different parser, not from the module.
    const json sidecar = json::parse(ReadText(fs::path(kData) / "models_card_full" / "bone_suppress.json"));
    EXPECT_EQ(sidecar["model_id"], card["model_id"]);
    EXPECT_EQ(sidecar["version"], card["model_version"]);
    EXPECT_EQ("1.2.3-rc.1+build.7", card["model_version"].get<std::string>());
    EXPECT_EQ(sidecar["intended_use"], card["intended_use"]);
    EXPECT_EQ(sidecar["training_data_summary"], card["training_data_summary"]);
    EXPECT_EQ(sidecar["demographic_performance"], card["demographic_performance"]);
    EXPECT_EQ(sidecar["limitations"], card["limitations"]);
    EXPECT_EQ(sidecar["published_date"], card["published_date"]);
    EXPECT_EQ(sidecar["pccp_scope"], card["pccp_scope"]);
    EXPECT_EQ(sidecar["training_data_hash"], card["training_data_hash"]);
    EXPECT_EQ(sidecar["validation_metrics"], card["validation_metrics"]);
    EXPECT_EQ(11u, card.size()) << "exactly the REQ-AI-008/010 fields: " << c.text;
}

TEST_F(AiModelCardTest, AFieldTheSidecarDoesNotCarryIsNullAndIsNeverInvented) {
    Init(Data("models_x2"));
    const Card c = GetCard("bone_toy_x2");
    ASSERT_EQ(XPE_OK, c.rc);
    const json card = Parsed(c);
    for (const char* key : {"intended_use", "training_data_summary", "demographic_performance", "limitations", "published_date"}) {
        ASSERT_TRUE(card.contains(key)) << key << " is a REQ-AI-010 field: it is in the card even when empty";
        EXPECT_TRUE(card[key].is_null()) << key << " = " << card[key].dump();
    }
    EXPECT_EQ("bone_toy_x2", card["model_id"].get<std::string>());
    EXPECT_EQ("0.0.1", card["model_version"].get<std::string>());
}

TEST_F(AiModelCardTest, ThePccpStatusSaysThatTheModuleDidNotEvaluateTheScope) {
    // No authorized PCCP exists to compare the scope with (T-005's sibling T-011); "within_boundary" or "not_applicable"
    // would be a claim the module cannot make.
    Init(Data("models_card_full"));
    const json card = Parsed(GetCard("bone_card_full_toy"));
    EXPECT_EQ("not_evaluated", card["pccp_status"].get<std::string>());
    EXPECT_EQ("none: toy model for wiring tests", card["pccp_scope"].get<std::string>()) << "the scope itself is the sidecar's";
}

TEST_F(AiModelCardTest, TheCardOfABodyPartModelComesFromItsSidecarToo) {
    Init(Data("models_bodypart_a"));
    const Card c = GetCard("bodypart_toy");
    ASSERT_EQ(XPE_OK, c.rc);
    const json card = Parsed(c);
    EXPECT_EQ("bodypart_toy", card["model_id"].get<std::string>());
    EXPECT_EQ(0u, c.text.find("{")) << c.text;
    EXPECT_EQ(std::string::npos, c.text.find("labels")) << "the labels are the body-part reader's, not part of the card";
}

TEST_F(AiModelCardTest, NoConstantOfTheOldStubCardRemainsInAnyCard) {
    for (const char* dir : {"models_x2", "models_card_full", "models_bodypart_a"}) {
        xpe_ai_shutdown();
        Init(Data(dir));
        const char* id = std::string(dir) == "models_x2" ? "bone_toy_x2"
                         : std::string(dir) == "models_card_full" ? "bone_card_full_toy" : "bodypart_toy";
        const Card c = GetCard(id);
        ASSERT_EQ(XPE_OK, c.rc) << dir;
        EXPECT_EQ(std::string::npos, c.text.find("stub")) << dir << ": " << c.text;
        EXPECT_EQ(std::string::npos, c.text.find("2026-04-22")) << dir << ": " << c.text;
        EXPECT_EQ(std::string::npos, c.text.find("N/A")) << dir << ": " << c.text;
        EXPECT_EQ(std::string::npos, c.text.find("ONNX Runtime")) << dir << ": " << c.text;
    }
}

TEST_F(AiModelCardTest, ACardIsTheSameOnEveryCall) {
    Init(Data("models_card_full"));
    const Card first = GetCard("bone_card_full_toy");
    const Card second = GetCard("bone_card_full_toy");
    ASSERT_EQ(XPE_OK, first.rc);
    EXPECT_EQ(first.text, second.text);
}

/* ============================================================================
 * No verified model, no card
 * ============================================================================ */

TEST_F(AiModelCardTest, AModelIdThatNoVerifiedModelHasIsUnavailableAndTheAnswerSaysNothingElse) {
    Init(Data("models_x2"));
    const Card c = GetCard("nonexistent_model_v99");
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, c.rc);
    const json j = Parsed(c);
    EXPECT_EQ("nonexistent_model_v99", j["model_id"].get<std::string>());
    EXPECT_EQ("model_unavailable", j["error"].get<std::string>());
    EXPECT_TRUE(j.contains("reason"));
    EXPECT_EQ(3u, j.size()) << "no model_version, no limitations: nothing is made up for a model that is not there: " << c.text;
}

TEST_F(AiModelCardTest, TheFourIdsTheOldStubCardAnsweredToAreUnavailableNow) {
    Init(Data("models_x2"));
    for (const char* id : {"bodypart_cnn_v1", "stitch_feature_match_v1", "bone_suppress_unet_v1", "dl_denoise_ssl_v1"}) {
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, GetCard(id).rc) << id << " has no model behind it";
    }
}

TEST_F(AiModelCardTest, WithNoModelDirectoryThereIsNoCardAtAll) {
    Init("dummy_model_dir");
    const Card c = GetCard("bone_toy_x2");
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, c.rc);
    EXPECT_NE(std::string::npos, c.text.find("model_unavailable"));
}

TEST_F(AiModelCardTest, AModelThatDoesNotVerifyHasNoCard) {
    const TempDir t("tampered");
    t.CopyFrom("models_x2");
    Init(t.path.string());
    ASSERT_EQ(XPE_OK, GetCard("bone_toy_x2").rc) << "the control: the copy has a card";
    // change one byte of the model: the signature no longer matches
    std::string model = ReadText(t.path / "bone_suppress.onnx");
    model[model.size() / 2] ^= 0x01;
    WriteText(t.path / "bone_suppress.onnx", model);
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, GetCard("bone_toy_x2").rc);
}

TEST_F(AiModelCardTest, AModelWhoseSidecarIsInvalidHasNoCard) {
    const TempDir t("invalid_sidecar");
    t.CopyFrom("models_x2");
    // a valid signature over a sidecar that lacks pccp_scope
    WriteText(t.path / "bone_suppress.json",
              "{\"model_id\":\"bone_toy_x2\",\"version\":\"1.0.0\",\"training_data_hash\":\"n\",\"validation_metrics\":{\"m\":1}}");
    ASSERT_TRUE(xpe_test::SignDir(t.path, "bone_suppress"));
    Init(t.path.string());
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, GetCard("bone_toy_x2").rc);
}

TEST_F(AiModelCardTest, ACardFollowsTheFilesWhenTheyChangeInTheSameSession) {
    const TempDir t("follows");
    t.CopyFrom("models_x2");
    Init(t.path.string());
    ASSERT_EQ(XPE_OK, GetCard("bone_toy_x2").rc);
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, GetCard("renamed_model").rc);

    const auto before = fs::last_write_time(t.path / "bone_suppress.json");
    WriteText(t.path / "bone_suppress.json", SidecarText("renamed_model", "\"limitations\":\"changed\""));
    fs::last_write_time(t.path / "bone_suppress.json", before + std::chrono::hours(1));
    ASSERT_TRUE(xpe_test::SignDir(t.path, "bone_suppress"));
    const Card now = GetCard("renamed_model");
    ASSERT_EQ(XPE_OK, now.rc) << "a model whose files changed is seen on the next call";
    EXPECT_EQ("changed", Parsed(now)["limitations"].get<std::string>());
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, GetCard("bone_toy_x2").rc) << "...and its old id is gone";
}

// QA-B-195d (Codex #92). A first version kept each card while the size and write time of the three files stayed as they were
// when it was made. The test above puts the write time ONE HOUR FORWARD, which is exactly the case such a cache sees; the two
// tests below are the cases it does not: the SAME size, and the write time put back. Every lookup now reads and verifies.
TEST_F(AiModelCardTest, ASidecarChangedToTheSameSizeWithItsWriteTimePutBackIsRefusedNotServedFromMemory) {
    const TempDir t("same_size_tamper");
    t.CopyFrom("models_x2");
    Init(t.path.string());
    ASSERT_EQ(XPE_OK, GetCard("bone_toy_x2").rc) << "the control: the model has a card (and a cache, if there were one, is warm)";

    const fs::path json = t.path / "bone_suppress.json";
    const auto writeTime = fs::last_write_time(json);
    std::string text = ReadText(json);
    const size_t at = text.find("wiring");
    ASSERT_NE(std::string::npos, at);
    text[at] = 'W';                                  // one character, same length: the signature no longer matches
    WriteText(json, text);
    fs::last_write_time(json, writeTime);            // ...and the write time is put back
    ASSERT_EQ(text.size(), static_cast<size_t>(fs::file_size(json)));

    const Card now = GetCard("bone_toy_x2");
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, now.rc) << "a sidecar that no longer verifies has no card: " << now.text;
    EXPECT_NE(std::string::npos, now.text.find("model_unavailable"));
}

TEST_F(AiModelCardTest, AValidSidecarAndSignatureOfTheSameSizeAndTimeAreSeenAtOnce) {
    const TempDir t("same_size_valid");
    t.CopyFrom("models_x2");
    Init(t.path.string());
    const Card before = GetCard("bone_toy_x2");
    ASSERT_EQ(XPE_OK, before.rc);
    ASSERT_EQ("0.0.1", Parsed(before)["model_version"].get<std::string>());

    const fs::path json = t.path / "bone_suppress.json";
    const fs::path sig = t.path / "bone_suppress.sig";
    const auto jsonTime = fs::last_write_time(json);
    const auto sigTime = fs::last_write_time(sig);
    const auto jsonSize = fs::file_size(json);
    const auto sigSize = fs::file_size(sig);
    std::string text = ReadText(json);
    const size_t at = text.find("0.0.1");
    ASSERT_NE(std::string::npos, at);
    text.replace(at, 5, "0.0.2");                    // same length
    WriteText(json, text);
    ASSERT_TRUE(xpe_test::SignDir(t.path, "bone_suppress"));   // a valid pair again
    fs::last_write_time(json, jsonTime);
    fs::last_write_time(sig, sigTime);
    ASSERT_EQ(jsonSize, fs::file_size(json));
    ASSERT_EQ(sigSize, fs::file_size(sig)) << "the premise: size and write time of every file are what they were";

    const Card now = GetCard("bone_toy_x2");
    ASSERT_EQ(XPE_OK, now.rc) << now.text;
    EXPECT_EQ("0.0.2", Parsed(now)["model_version"].get<std::string>()) << "the new card, not the one made before";
}

TEST_F(AiModelCardTest, ATextWithQuotesBackslashesControlCharactersAndNonAsciiSurvivesIntoValidJson) {
    const TempDir t("escapes");
    t.CopyFrom("models_x2");
    const std::string tricky = std::string("quote\" backslash\\ newline\n tab\t bell\x07 korean \xEA\xB0\x80 euro \xE2\x82\xAC");
    WriteText(t.path / "bone_suppress.json", SidecarText("escape_toy", "\"intended_use\":" + json(tricky).dump()));
    ASSERT_TRUE(xpe_test::SignDir(t.path, "bone_suppress"));
    Init(t.path.string());
    const Card c = GetCard("escape_toy");
    ASSERT_EQ(XPE_OK, c.rc);
    const json card = Parsed(c);
    EXPECT_EQ(tricky, card["intended_use"].get<std::string>()) << "the text comes back exactly";
}

/* ============================================================================
 * The schema (REQ-AI-011)
 * ============================================================================ */

TEST_F(AiModelCardTest, EveryCardAndEveryUnavailableAnswerConformsToTheSchema) {
    const std::string schema = ReadText(fs::path(kData) / "schemas" / "model-card.schema.json");
    ASSERT_FALSE(schema.empty());
    struct Case { const char* dir; const char* id; XpeErrorCode rc; };
    for (const Case& c : {Case{"models_card_full", "bone_card_full_toy", XPE_OK}, Case{"models_x2", "bone_toy_x2", XPE_OK},
                          Case{"models_bodypart_a", "bodypart_toy", XPE_OK},
                          Case{"models_x2", "no_such_model", XPE_ERR_CONFIG_INVALID}}) {
        xpe_ai_shutdown();
        Init(Data(c.dir));
        const Card card = GetCard(c.id);
        EXPECT_EQ(c.rc, card.rc) << c.dir << " " << c.id;
        const std::vector<std::string> errors = xpe_test::SchemaErrors(schema, card.text);
        for (const std::string& e : errors) ADD_FAILURE() << c.dir << " " << c.id << ": " << e << "  in " << card.text;
        EXPECT_TRUE(errors.empty());
    }
}

TEST(ModelCardSchema, ControlTheSchemaRefusesWhatItShould) {
    // Without these the schema test above could pass for a schema that accepts anything.
    const std::string schema = ReadText(fs::path(kData) / "schemas" / "model-card.schema.json");
    const std::string good =
        "{\"model_id\":\"a\",\"model_version\":\"1.0.0\",\"intended_use\":null,\"training_data_summary\":null,"
        "\"demographic_performance\":null,\"limitations\":null,\"pccp_status\":\"not_evaluated\",\"published_date\":null,"
        "\"pccp_scope\":\"x\",\"training_data_hash\":\"y\",\"validation_metrics\":{\"m\":1}}";
    EXPECT_TRUE(xpe_test::SchemaErrors(schema, good).empty());
    auto without = [&](const char* key) {
        json j = json::parse(good);
        j.erase(key);
        return j.dump();
    };
    for (const char* key : {"model_id", "model_version", "intended_use", "limitations", "pccp_status", "published_date",
                            "pccp_scope", "training_data_hash", "validation_metrics"}) {
        EXPECT_FALSE(xpe_test::SchemaErrors(schema, without(key)).empty()) << "missing " << key << " must not conform";
    }
    auto with = [&](const char* key, const json& v) {
        json j = json::parse(good);
        j[key] = v;
        return j.dump();
    };
    EXPECT_FALSE(xpe_test::SchemaErrors(schema, with("model_version", "1.0")).empty());
    EXPECT_FALSE(xpe_test::SchemaErrors(schema, with("model_version", "01.0.0")).empty());
    EXPECT_FALSE(xpe_test::SchemaErrors(schema, with("pccp_status", "fine")).empty());
    EXPECT_FALSE(xpe_test::SchemaErrors(schema, with("published_date", "2026/10/03")).empty());
    EXPECT_FALSE(xpe_test::SchemaErrors(schema, with("validation_metrics", json::object())).empty());
    EXPECT_FALSE(xpe_test::SchemaErrors(schema, with("intended_use", 5)).empty());
    EXPECT_FALSE(xpe_test::SchemaErrors(schema, with("extra_field", 1)).empty()) << "additionalProperties is false";
    EXPECT_FALSE(xpe_test::SchemaErrors(schema, "{\"model_id\":\"a\",\"error\":\"other\",\"reason\":\"x\"}").empty());
    EXPECT_FALSE(xpe_test::SchemaErrors(schema, "[]").empty());
    // the checker refuses a schema keyword it does not know
    EXPECT_FALSE(xpe_test::SchemaErrors("{\"type\":\"object\",\"dependentRequired\":{}}", "{}").empty());
}

TEST(ModelSidecarSchema, EveryShippedFixtureSidecarConformsAndTheSchemaRefusesWhatTheModuleRefuses) {
    const std::string schema = ReadText(fs::path(kData) / "schemas" / "model-sidecar.schema.json");
    ASSERT_FALSE(schema.empty());
    size_t checked = 0;
    for (const auto& e : fs::recursive_directory_iterator(fs::path(kData))) {
        if (!e.is_regular_file() || e.path().extension() != ".json") continue;
        const std::string parent = e.path().parent_path().filename().string();
        if (parent == "signing" || parent == "schemas") continue;
        const std::vector<std::string> errors = xpe_test::SchemaErrors(schema, ReadText(e.path()));
        for (const std::string& m : errors) ADD_FAILURE() << e.path().string() << ": " << m;
        ++checked;
    }
    EXPECT_GE(checked, 20u) << "the fixtures were found (control)";

    // The controls: a schema that accepted anything would pass the loop above.
    const std::string good = SidecarText("a");
    EXPECT_TRUE(xpe_test::SchemaErrors(schema, good).empty());
    for (const char* key : {"model_id", "version", "pccp_scope", "training_data_hash", "validation_metrics"}) {
        json j = json::parse(good);
        j.erase(key);
        EXPECT_FALSE(xpe_test::SchemaErrors(schema, j.dump()).empty()) << "without " << key;
    }
    for (const char* bad : {"1.0", "01.0.0", "1.0.0-01"}) {
        json j = json::parse(good);
        j["version"] = bad;
        EXPECT_FALSE(xpe_test::SchemaErrors(schema, j.dump()).empty()) << bad;
    }
    json empty = json::parse(good);
    empty["validation_metrics"] = json::object();
    EXPECT_FALSE(xpe_test::SchemaErrors(schema, empty.dump()).empty());
    json wrong = json::parse(good);
    wrong["limitations"] = 5;
    EXPECT_FALSE(xpe_test::SchemaErrors(schema, wrong.dump()).empty());
}

/* ============================================================================
 * Buffer handling (unchanged by QA-B-197)
 * ============================================================================ */

TEST_F(AiModelCardTest, GetModelCardNullModelIdReturnsInvalid) {
    Init(Data("models_x2"));
    char buf[4096] = {};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ai_get_model_card(nullptr, buf, sizeof(buf)));
}

TEST_F(AiModelCardTest, GetModelCardNullBufReturnsInvalid) {
    Init(Data("models_x2"));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ai_get_model_card("bone_toy_x2", nullptr, 4096));
}

// Renamed and re-asserted by QA-B-42: a zero-length buffer is a missing argument (#142), not a short one.
TEST_F(AiModelCardTest, GetModelCardZeroBufSizeReturnsInvalidInput) {
    Init(Data("models_x2"));
    char buf[1] = {};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ai_get_model_card("bone_toy_x2", buf, 0));
}

TEST_F(AiModelCardTest, GetModelCardSmallBufferReturnsTooSmallAndStillHoldsTruncatedJson) {
    Init(Data("models_x2"));
    char buf[10] = {};
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, xpe_ai_get_model_card("bone_toy_x2", buf, sizeof(buf)));
    EXPECT_EQ(9u, std::strlen(buf)) << "the truncated, null-terminated start of the card";
    EXPECT_EQ(0, std::strncmp(buf, "{\"model_id", 9));
}

TEST_F(AiModelCardTest, GetModelCardExactSizeBufferSucceedsAndOneLessDoesNot) {
    Init(Data("models_x2"));
    const Card probe = GetCard("bone_toy_x2");
    ASSERT_EQ(XPE_OK, probe.rc);
    const size_t exact = probe.text.size() + 1;
    std::vector<char> exactBuf(exact);
    EXPECT_EQ(XPE_OK, xpe_ai_get_model_card("bone_toy_x2", exactBuf.data(), exactBuf.size()));
    std::vector<char> shortBuf(exact - 1);
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, xpe_ai_get_model_card("bone_toy_x2", shortBuf.data(), shortBuf.size()));
}

TEST(ModelCardNotInitialized, WithoutInitThereIsNoCardAndTheCodeSaysSo) {
    xpe_ai_shutdown();
    char buf[4096] = {};
    EXPECT_EQ(XPE_ERR_NOT_INITIALIZED, xpe_ai_get_model_card("bone_toy_x2", buf, sizeof(buf)));
}

/* ============================================================================
 * What a lookup costs now that it reads and verifies every time (QA-B-195d)
 * ============================================================================ */

namespace {

/** A model directory whose model file is @p mib MiB of bytes (never parsed as ONNX by a card lookup), signed. */
void MakeSizedModelDir(const fs::path& dir, size_t mib) {
    fs::create_directories(dir);
    {
        std::ofstream f(dir / "bone_suppress.onnx", std::ios::binary | std::ios::trunc);
        std::vector<char> block(1u << 20);
        uint32_t x = 12345u;
        for (size_t m = 0; m < mib; ++m) {
            for (char& c : block) {
                x = x * 1664525u + 1013904223u;
                c = static_cast<char>(x >> 24);
            }
            f.write(block.data(), static_cast<std::streamsize>(block.size()));
        }
    }
    WriteText(dir / "bone_suppress.json", SidecarText("cost_toy"));
    ASSERT_TRUE(xpe_test::SignDir(dir, "bone_suppress"));
}

double MillisOf(const std::chrono::steady_clock::time_point& t0) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

}  // namespace

// The cost of one lookup of the bone-suppression model: one read of the model file and one SHA-256 + ECDSA verification of it.
// Sizes: XPE_CARD_COST_MIB (comma separated, default "1,16"); the QA-B-195d report was measured with "1,16,64,256,768".
// The bound is an UPPER-BOUND ESTIMATE for a shared CI machine, chosen far above the local figure printed here.
TEST(ModelCardCost, ALookupOfTheModelItFindsIsReadAndVerifiedEveryTimeAndStaysWithinABound) {
    char env[64] = {0};
    const std::string sizes = GetEnvironmentVariableA("XPE_CARD_COST_MIB", env, sizeof(env)) > 0 ? std::string(env) : std::string("1,16");
    size_t pos = 0;
    while (pos < sizes.size()) {
        const size_t comma = sizes.find(',', pos);
        const size_t mib = static_cast<size_t>(std::atoi(sizes.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos).c_str()));
        pos = comma == std::string::npos ? sizes.size() : comma + 1;
        if (mib == 0) continue;

        const TempDir t(("cost_" + std::to_string(mib)).c_str());
        fs::remove_all(t.path);
        MakeSizedModelDir(t.path, mib);
        xpe_ai_shutdown();
        ASSERT_EQ(XPE_OK, xpe_ai_init(t.path.string().c_str(), nullptr));
        std::vector<double> ms;
        for (int i = 0; i < 5; ++i) {
            const auto t0 = std::chrono::steady_clock::now();
            const Card c = GetCard("cost_toy");
            ms.push_back(MillisOf(t0));
            ASSERT_EQ(XPE_OK, c.rc) << mib << " MiB: " << c.text;
        }
        std::vector<double> sorted = ms;
        std::sort(sorted.begin(), sorted.end());
        const double bound = 5000.0 + 100.0 * static_cast<double>(mib);   // an upper-bound estimate, not a measurement
        std::printf("[ measured ] a card lookup, %zu MiB model: first %.1f ms, median of 5 %.1f ms, max %.1f ms (bound %.0f ms)\n",
                    mib, ms.front(), sorted[2], sorted.back(), bound);
        EXPECT_LT(sorted.back(), bound) << mib << " MiB";
        xpe_ai_shutdown();
    }
}
