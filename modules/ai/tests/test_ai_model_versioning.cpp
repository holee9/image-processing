/**
 * @file test_ai_model_versioning.cpp
 * @brief Model versioning and metadata (REQ-AI-008) as the model card reports it -- SPEC-XPE-P3-AI T-004
 *
 * REQ-AI-008: Model versioning shall follow semver; model metadata shall include:
 * model_id, version, pccp_scope, training_data_hash, validation_metrics.
 *
 * WHY THIS FILE CHANGED (QA-B-197 M2, found by QA-B-196). These tests used to ask a table of constants: "bodypart_cnn_v1"
 * and three other hard-coded ids always had a card, and the tests asserted that the card contained the fields -- fields
 * the module wrote itself, whatever any model said. They also demanded things no requirement states (a "sha256:" prefix on
 * the training data hash, one of four metric names) because the constants happened to have them, and one test
 * ("ModelVersionsCanBeCompared") asserted nothing. Now the card comes from the verified sidecar of a verified model, so every
 * field is compared with an independent reading of that sidecar file, and the semantic version is checked against the
 * semantic-versioning grammar itself.
 */

#include "test_signing_helper.h"
#include "xpe/ai/ai_api.h"
#include "xpe/common/xpe_error.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <windows.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <string>
#include <vector>

#ifndef XPE_AI_TEST_DATA_DIR
#error "XPE_AI_TEST_DATA_DIR must be defined by the build (modules/ai/CMakeLists.txt)"
#endif

namespace {

namespace fs = std::filesystem;
using nlohmann::json;

const std::string kData = XPE_AI_TEST_DATA_DIR;

/** The semantic versioning 2.0.0 grammar, as published on semver.org. */
bool IsSemver(const std::string& v) {
    static const std::regex re(
        "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)(-((0|[1-9][0-9]*|[0-9]*[A-Za-z-][0-9A-Za-z-]*)"
        "(\\.(0|[1-9][0-9]*|[0-9]*[A-Za-z-][0-9A-Za-z-]*))*))?(\\+[0-9A-Za-z-]+(\\.[0-9A-Za-z-]+)*)?$");
    return std::regex_match(v, re);
}

json ReadJson(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return json::parse(std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()));
}

struct Case {
    const char* dir;
    const char* sidecarName;
    const char* id;
};
const Case kCases[] = {{"models_card_full", "bone_suppress.json", "bone_card_full_toy"},
                       {"models_x2", "bone_suppress.json", "bone_toy_x2"},
                       {"models_bodypart_a", "bodypart.json", "bodypart_toy"}};

}  // namespace

class AiModelVersioningTest : public ::testing::Test {
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
    /** The card of @p id as parsed JSON, and the sidecar file it should come from. */
    static void CardAndSidecar(const Case& c, json* card, json* sidecar) {
        xpe_ai_shutdown();
        Init(kData + "/" + c.dir);
        char buffer[8192];
        ASSERT_EQ(XPE_OK, xpe_ai_get_model_card(c.id, buffer, sizeof(buffer))) << c.id;
        *card = json::parse(buffer, nullptr, false);
        ASSERT_FALSE(card->is_discarded()) << buffer;
        *sidecar = ReadJson(fs::path(kData) / c.dir / c.sidecarName);
    }
};

/* ============================================================================
 * T-004.1: the version is the sidecar's, and a semantic version (REQ-AI-008)
 * ============================================================================ */

TEST_F(AiModelVersioningTest, TheCardVersionIsTheSidecarsVersionAndASemanticVersion) {
    for (const Case& c : kCases) {
        json card, sidecar;
        CardAndSidecar(c, &card, &sidecar);
        const std::string version = card["model_version"].get<std::string>();
        EXPECT_EQ(sidecar["version"].get<std::string>(), version) << c.id;
        EXPECT_TRUE(IsSemver(version)) << c.id << ": '" << version << "' is not a semantic version";
    }
    // the control for the grammar check above: it does refuse what semver refuses
    for (const char* bad : {"1.0", "01.0.0", "1.0.0.0", "v1.0.0", "1.0.0-", "1.0.0-01"}) EXPECT_FALSE(IsSemver(bad)) << bad;
    for (const char* good : {"0.0.1", "1.2.3-rc.1+build.7", "10.20.30"}) EXPECT_TRUE(IsSemver(good)) << good;
}

/* ============================================================================
 * T-004.2 ... T-004.6: each REQ-AI-008 field is the sidecar's
 * ============================================================================ */

TEST_F(AiModelVersioningTest, TheCardModelIdIsTheSidecarsModelId) {
    for (const Case& c : kCases) {
        json card, sidecar;
        CardAndSidecar(c, &card, &sidecar);
        EXPECT_EQ(sidecar["model_id"], card["model_id"]) << c.id;
        EXPECT_EQ(c.id, card["model_id"].get<std::string>());
    }
}

TEST_F(AiModelVersioningTest, TheCardPccpScopeIsTheSidecarsAndItsStatusIsNotEvaluated) {
    for (const Case& c : kCases) {
        json card, sidecar;
        CardAndSidecar(c, &card, &sidecar);
        EXPECT_EQ(sidecar["pccp_scope"], card["pccp_scope"]) << c.id;
        EXPECT_EQ("not_evaluated", card["pccp_status"].get<std::string>())
            << c.id << ": no authorized PCCP exists to compare the scope with, so the status is not 'within_boundary'";
    }
}

TEST_F(AiModelVersioningTest, TheCardTrainingDataHashIsTheSidecarsAndNotEmpty) {
    for (const Case& c : kCases) {
        json card, sidecar;
        CardAndSidecar(c, &card, &sidecar);
        EXPECT_EQ(sidecar["training_data_hash"], card["training_data_hash"]) << c.id;
        EXPECT_FALSE(card["training_data_hash"].get<std::string>().empty()) << c.id;
        // No algorithm prefix is demanded: no requirement says what a hash looks like.
    }
}

TEST_F(AiModelVersioningTest, TheCardValidationMetricsAreTheSidecarsObject) {
    for (const Case& c : kCases) {
        json card, sidecar;
        CardAndSidecar(c, &card, &sidecar);
        EXPECT_EQ(sidecar["validation_metrics"], card["validation_metrics"]) << c.id;
        EXPECT_TRUE(card["validation_metrics"].is_object() && !card["validation_metrics"].empty()) << c.id;
    }
}

TEST_F(AiModelVersioningTest, TheCardCarriesAllFiveRequiredMetadataFieldsWithTheSidecarsValues) {
    for (const Case& c : kCases) {
        json card, sidecar;
        CardAndSidecar(c, &card, &sidecar);
        const std::vector<std::pair<const char*, const char*>> fields = {{"model_id", "model_id"},
                                                                         {"version", "model_version"},
                                                                         {"pccp_scope", "pccp_scope"},
                                                                         {"training_data_hash", "training_data_hash"},
                                                                         {"validation_metrics", "validation_metrics"}};
        for (const auto& f : fields) {
            ASSERT_TRUE(card.contains(f.second)) << c.id << ": missing " << f.second;
            EXPECT_EQ(sidecar[f.first], card[f.second]) << c.id << ": " << f.second;
        }
    }
}

/* ============================================================================
 * T-004.7: two models, each with its own version (replaces "ModelVersionsCanBeCompared", which asserted nothing)
 * ============================================================================ */

TEST_F(AiModelVersioningTest, TwoModelsInOneDirectoryEachReportTheirOwnVersionAndId) {
    const fs::path dir = fs::temp_directory_path() / ("xpe_versions_" + std::to_string(GetCurrentProcessId()));
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    for (const auto& e : fs::directory_iterator(fs::path(kData) / "models_card_full")) {
        fs::copy_file(e.path(), dir / e.path().filename(), fs::copy_options::overwrite_existing);   // the bone model
    }
    for (const auto& e : fs::directory_iterator(fs::path(kData) / "models_bodypart_a")) {
        fs::copy_file(e.path(), dir / e.path().filename(), fs::copy_options::overwrite_existing);   // the body-part model
    }
    Init(dir.string());
    char bone[8192], part[8192];
    ASSERT_EQ(XPE_OK, xpe_ai_get_model_card("bone_card_full_toy", bone, sizeof(bone)));
    ASSERT_EQ(XPE_OK, xpe_ai_get_model_card("bodypart_toy", part, sizeof(part)));
    const json b = json::parse(bone), p = json::parse(part);
    EXPECT_EQ("1.2.3-rc.1+build.7", b["model_version"].get<std::string>());
    EXPECT_EQ("0.0.1", p["model_version"].get<std::string>());
    EXPECT_NE(b["model_version"], p["model_version"]);
    fs::remove_all(dir, ec);
}

/* ============================================================================
 * T-004.8: a model that is not there has no card
 * ============================================================================ */

TEST_F(AiModelVersioningTest, ModelCardForUnknownModelIsUnavailableWithAnErrorField) {
    Init(kData + "/models_x2");
    char buffer[4096];
    const XpeErrorCode ec = xpe_ai_get_model_card("unknown_model_xyz", buffer, sizeof(buffer));
    // QA-B-197: this was XPE_ERR_IO_FAILED with a "model_not_loaded" card; the model-less case is now "unavailable", -4.
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, ec);
    const json j = json::parse(buffer, nullptr, false);
    ASSERT_FALSE(j.is_discarded()) << buffer;
    EXPECT_EQ("model_unavailable", j["error"].get<std::string>());
}

/* ============================================================================
 * T-004.9 ... T-004.11: buffers and null arguments (unchanged by QA-B-197)
 * ============================================================================ */

TEST_F(AiModelVersioningTest, ModelCardWithTinyBufferReturnsBufferTooSmall) {
    Init(kData + "/models_x2");
    char tinyBuffer[10];
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, xpe_ai_get_model_card("bone_toy_x2", tinyBuffer, sizeof(tinyBuffer)));
    EXPECT_EQ('\0', tinyBuffer[9]) << "null-terminated even when too small";
}

TEST_F(AiModelVersioningTest, ModelCardWithNullModelIdReturnsInvalidInput) {
    Init(kData + "/models_x2");
    char buffer[4096];
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ai_get_model_card(nullptr, buffer, sizeof(buffer)));
}

TEST_F(AiModelVersioningTest, ModelCardWithNullBufferReturnsInvalidInput) {
    Init(kData + "/models_x2");
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ai_get_model_card("bone_toy_x2", nullptr, 4096));
}
