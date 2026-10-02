/**
 * @file test_ai_model_loading_trust.cpp
 * @brief Model loading verifies signatures (QA-B-195 M3, REQ-AI-007 / REQ-AI-091).
 *
 * OnnxSession::Create reads the model, its sidecar and its signature ONCE, verifies them together under a trusted key
 * and the role the model is loaded for, and builds the session from those bytes. These tests hold that at the session
 * layer (every path to a model goes through it: the in-process bone and body-part loaders and the worker) and at the C
 * ABI:
 *   - a signed model loads under its own role and only under its own role;
 *   - a changed model, sidecar or signature, a missing signature, or a key nobody trusts is refused with the reason that
 *     is true, and nothing is loaded -- in a stub build as well (a stub loads nothing, but the refusal must not depend
 *     on the build);
 *   - a build whose trust list is empty (the production build until the production key exists, #243) refuses EVERY model;
 *   - what was verified is what is used: files changed between the check and the load do not reach the session.
 *
 * The signatures of tests/data were made for the TEST key, which every test process trusts through
 * XPE_AI_TEST_TRUSTED_KEYS (test_trust_setup.cpp). Tests that need another trust set change that variable and restore it.
 */

#include <gtest/gtest.h>

#include "ai_model_signer.h"
#include "test_signing_helper.h"
#include "xpe/ai/ai_api.h"
#include "xpe/ai/ai_onnx_session.h"
#include "xpe/common/xpe_error.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifndef XPE_AI_TEST_DATA_DIR
#error "XPE_AI_TEST_DATA_DIR must be defined by the build (modules/ai/CMakeLists.txt)"
#endif

#ifdef XPE_AI_TEST_HOOKS
namespace xpe::ai {
void TestSetAfterVerifyHook(void (*hook)(const std::string& modelPath));   // ai_onnx_session.cpp, test builds only
}
// The DLL's own copy of the same hook, for the end-to-end tests that go through the C ABI (ai.cpp, test builds only).
extern "C" __declspec(dllimport) void xpe_ai_test_set_after_verify_hook(void (*hook)(const char* modelPath));
#endif

namespace {

namespace fs = std::filesystem;
using xpe::ai::OnnxErrorCode;
using xpe::ai::OnnxSession;
using xpe::ai::OnnxSessionConfig;
using xpe::ai::SignatureStatus;
using xpe::ai::SignatureStatusText;

const std::string kData = XPE_AI_TEST_DATA_DIR;
const char* const kEnv = "XPE_AI_TEST_TRUSTED_KEYS";

std::string Hex(const uint8_t* b, size_t n) {
    std::string s;
    char two[3];
    for (size_t i = 0; i < n; ++i) {
        std::snprintf(two, sizeof(two), "%02x", static_cast<unsigned>(b[i]));
        s += two;
    }
    return s;
}

/** Changes what the process trusts for the life of the object, then puts it back. `nullptr` removes the variable. */
struct TrustScope {
    std::string saved;
    bool had{false};
    explicit TrustScope(const char* value) {
        char buf[8192];
        const DWORD n = GetEnvironmentVariableA(kEnv, buf, sizeof(buf));
        had = n > 0 && n < sizeof(buf);
        if (had) saved.assign(buf, n);
        SetEnvironmentVariableA(kEnv, value);
    }
    ~TrustScope() { SetEnvironmentVariableA(kEnv, had ? saved.c_str() : nullptr); }
    TrustScope(const TrustScope&) = delete;
    TrustScope& operator=(const TrustScope&) = delete;
};

struct TempDir {
    fs::path path;
    explicit TempDir(const char* name) {
        path = fs::temp_directory_path() / (std::string("xpe_trust_") + name + "_" + std::to_string(GetCurrentProcessId()));
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
    /** A copy of every file of a tests/data model directory. */
    void CopyFrom(const char* dataDir) const {
        for (const auto& e : fs::directory_iterator(fs::path(kData) / dataDir)) {
            fs::copy_file(e.path(), path / e.path().filename(), fs::copy_options::overwrite_existing);
        }
    }
    fs::path operator/(const char* name) const { return path / name; }
};

void Write(const fs::path& p, const std::vector<uint8_t>& b) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

OnnxSessionConfig Cfg(const fs::path& model, const char* role = "bone_suppress") {
    OnnxSessionConfig c;
    c.model_path = model.string();
    c.role = role;
    return c;
}

/** The code, and whether the message names this reason. */
struct Outcome {
    OnnxErrorCode code;
    std::string message;
    bool Names(SignatureStatus s) const { return message.find(SignatureStatusText(s)) != std::string::npos; }
};
Outcome Load(const fs::path& model, const char* role = "bone_suppress") {
    const auto r = OnnxSession::Create(Cfg(model, role));
    return {r.code, r.message};
}

bool IsStub() { return OnnxSession::IsStubBuild(); }

}  // namespace

// ===== the controls =========================================================================================

TEST(ModelLoadingTrust, TheTestSigningHelperWritesWhatTheVerifierAcceptsAndOnlyThat) {
    // The helper signs fixtures other tests create; this is what shows it is a signer and not a stamp.
    const TempDir t("helper");
    t.CopyFrom("models_bodypart_a");
    fs::remove(t / "bodypart.sig");
    ASSERT_TRUE(xpe_test::SignDir(t.path, "bodypart"));
    EXPECT_EQ(OnnxErrorCode::kOk, Load(t / "bodypart.onnx", "bodypart").code) << "freshly signed";
    auto side = xpe_test::ReadBytes(t / "bodypart.json");
    side[side.size() / 2] ^= 0x01;
    Write(t / "bodypart.json", side);
    EXPECT_EQ(OnnxErrorCode::kModelNotTrusted, Load(t / "bodypart.onnx", "bodypart").code) << "a changed sidecar, same signature";
    ASSERT_TRUE(xpe_test::SignDir(t.path, "bodypart"));
    EXPECT_EQ(OnnxErrorCode::kOk, Load(t / "bodypart.onnx", "bodypart").code) << "renewed";
}

TEST(ModelLoadingTrust, ASignedModelLoadsUnderItsOwnRoleAndOnlyUnderIt) {
    const fs::path bone = fs::path(kData) / "models_x2" / "bone_suppress.onnx";
    const fs::path part = fs::path(kData) / "models_bodypart_a" / "bodypart.onnx";
    EXPECT_EQ(OnnxErrorCode::kOk, Load(bone).code) << "bone model, bone role";
    EXPECT_EQ(OnnxErrorCode::kOk, Load(part, "bodypart").code) << "body-part model with its sidecar, body-part role";
    const Outcome swapped = Load(part, "bone_suppress");
    EXPECT_EQ(OnnxErrorCode::kModelNotTrusted, swapped.code) << "a body-part model asked for as a bone model";
    EXPECT_TRUE(swapped.Names(SignatureStatus::kBadSignature)) << swapped.message;
    EXPECT_EQ(OnnxErrorCode::kModelNotTrusted, Load(bone, "bodypart").code) << "a bone model asked for as a body-part model";
}

// ===== a changed model is not trusted, whatever the change =====================================================

TEST(ModelLoadingTrust, AChangedModelIsNotTrustedWhateverTheChangeAndNothingIsParsedFirst) {
    const TempDir t("model");
    t.CopyFrom("models_x2");
    const std::vector<uint8_t> original = xpe_test::ReadBytes(t / "bone_suppress.onnx");
    ASSERT_EQ(OnnxErrorCode::kOk, Load(t / "bone_suppress.onnx").code) << "the control";

    // every single byte
    for (size_t i = 0; i < original.size(); ++i) {
        std::vector<uint8_t> b = original;
        b[i] ^= 0x01;
        Write(t / "bone_suppress.onnx", b);
        const Outcome o = Load(t / "bone_suppress.onnx");
        EXPECT_EQ(OnnxErrorCode::kModelNotTrusted, o.code) << "byte " << i;
        EXPECT_TRUE(o.Names(SignatureStatus::kBadSignature)) << "byte " << i << ": " << o.message;
    }
    // the shapes that the parser used to be the only defence against: now the signature answers FIRST, with the real reason
    std::vector<uint8_t> half(original.begin(), original.begin() + original.size() / 2);
    std::vector<uint8_t> longer = original;
    longer.insert(longer.end(), 64, 'Z');
    const std::vector<std::vector<uint8_t>> shapes = {half, longer, std::vector<uint8_t>()};
    for (const std::vector<uint8_t>& bytes : shapes) {
        Write(t / "bone_suppress.onnx", bytes);
        const Outcome o = Load(t / "bone_suppress.onnx");
        EXPECT_EQ(OnnxErrorCode::kModelNotTrusted, o.code) << "size " << bytes.size();
        EXPECT_TRUE(o.Names(SignatureStatus::kBadSignature)) << o.message;
    }
    // another valid model of the same size
    Write(t / "bone_suppress.onnx", xpe_test::ReadBytes(fs::path(kData) / "models_x3" / "bone_suppress.onnx"));
    EXPECT_EQ(OnnxErrorCode::kModelNotTrusted, Load(t / "bone_suppress.onnx").code) << "a different valid model, the old signature";
    Write(t / "bone_suppress.onnx", original);
    EXPECT_EQ(OnnxErrorCode::kOk, Load(t / "bone_suppress.onnx").code) << "restored";
}

// ===== a missing or damaged signature or sidecar, and a key nobody trusts ======================================

TEST(ModelLoadingTrust, AMissingOrDamagedSignatureIsNotTrustedWithItsOwnReason) {
    const TempDir t("sig");
    t.CopyFrom("models_x2");
    const std::vector<uint8_t> sig = xpe_test::ReadBytes(t / "bone_suppress.sig");
    ASSERT_EQ(78u, sig.size());

    fs::remove(t / "bone_suppress.sig");
    Outcome o = Load(t / "bone_suppress.onnx");
    EXPECT_EQ(OnnxErrorCode::kModelNotTrusted, o.code);
    EXPECT_TRUE(o.Names(SignatureStatus::kNoSignatureFile)) << o.message;

    Write(t / "bone_suppress.sig", std::vector<uint8_t>());
    o = Load(t / "bone_suppress.onnx");
    EXPECT_TRUE(o.Names(SignatureStatus::kMalformedSignature)) << "an EMPTY signature file is present but malformed, not 'missing': " << o.message;

    Write(t / "bone_suppress.sig", std::vector<uint8_t>(sig.begin(), sig.end() - 1));
    EXPECT_TRUE(Load(t / "bone_suppress.onnx").Names(SignatureStatus::kMalformedSignature)) << "one byte short";

    std::vector<uint8_t> wrongKey = sig;
    wrongKey[6] ^= 0x01;   // the key id
    Write(t / "bone_suppress.sig", wrongKey);
    EXPECT_TRUE(Load(t / "bone_suppress.onnx").Names(SignatureStatus::kUnknownKey)) << "a key id nobody trusts";

    std::vector<uint8_t> flipped = sig;
    flipped[40] ^= 0x01;   // r
    Write(t / "bone_suppress.sig", flipped);
    EXPECT_TRUE(Load(t / "bone_suppress.onnx").Names(SignatureStatus::kBadSignature)) << "a changed signature";

    Write(t / "bone_suppress.sig", sig);
    EXPECT_EQ(OnnxErrorCode::kOk, Load(t / "bone_suppress.onnx").code) << "restored";
}

TEST(ModelLoadingTrust, ASidecarThatWasAddedChangedOrRemovedIsNotTrusted) {
    // A sidecar is signed with the model, the labels in it change what a result means: this is the "sidecar only"
    // attack the QA-B-195 probe showed passing silently.
    {
        const TempDir t("sidecar_bone");
        t.CopyFrom("models_x2");
        Write(t / "bone_suppress.json", {'{', '}'});   // the model was signed WITHOUT one
        EXPECT_TRUE(Load(t / "bone_suppress.onnx").Names(SignatureStatus::kBadSignature)) << "a sidecar added to a model signed without one";
    }
    const TempDir t("sidecar_part");
    t.CopyFrom("models_bodypart_a");
    ASSERT_EQ(OnnxErrorCode::kOk, Load(t / "bodypart.onnx", "bodypart").code) << "the control";
    const std::vector<uint8_t> side = xpe_test::ReadBytes(t / "bodypart.json");
    const std::string hands = "{\"labels\": [\"HAND\", \"HAND\", \"HAND\"]}";
    Write(t / "bodypart.json", std::vector<uint8_t>(hands.begin(), hands.end()));
    EXPECT_TRUE(Load(t / "bodypart.onnx", "bodypart").Names(SignatureStatus::kBadSignature)) << "the labels replaced";
    std::vector<uint8_t> crlf;
    for (const uint8_t b : side) {
        if (b == '\n') crlf.push_back('\r');
        crlf.push_back(b);
    }
    Write(t / "bodypart.json", crlf);
    EXPECT_TRUE(Load(t / "bodypart.onnx", "bodypart").Names(SignatureStatus::kBadSignature)) << "the same text with CRLF line endings";
    fs::remove(t / "bodypart.json");
    EXPECT_TRUE(Load(t / "bodypart.onnx", "bodypart").Names(SignatureStatus::kBadSignature)) << "the sidecar removed";
    Write(t / "bodypart.json", side);
    EXPECT_EQ(OnnxErrorCode::kOk, Load(t / "bodypart.onnx", "bodypart").code) << "restored";
}

// ===== who is trusted ========================================================================================

TEST(ModelLoadingTrust, ABuildThatTrustsNoKeyRefusesEveryModel) {
    // The production build until the production key exists (#243). Not a defect: nothing real is signed yet.
    const fs::path bone = fs::path(kData) / "models_x2" / "bone_suppress.onnx";
    {
        const TrustScope none(nullptr);
        EXPECT_TRUE(xpe::ai::TrustedModelKeys().empty());
        const Outcome o = Load(bone);
        EXPECT_EQ(OnnxErrorCode::kModelNotTrusted, o.code) << "a validly signed model, no trusted key";
        EXPECT_TRUE(o.Names(SignatureStatus::kUnknownKey)) << o.message;
    }
    {
        const TrustScope other(Hex(xpe_test::detail::kKey2Xy, 64).c_str());
        EXPECT_EQ(1u, xpe::ai::TrustedModelKeys().size());
        EXPECT_TRUE(Load(bone).Names(SignatureStatus::kUnknownKey)) << "signed by a key that is not the trusted one";
    }
    {
        const std::string both = Hex(xpe_test::detail::kKey2Xy, 64) + "," + Hex(xpe_test::detail::kKey1Xy, 64);
        const TrustScope two(both.c_str());
        EXPECT_EQ(2u, xpe::ai::TrustedModelKeys().size());
        EXPECT_EQ(OnnxErrorCode::kOk, Load(bone).code) << "trusted among others";
    }
    EXPECT_EQ(OnnxErrorCode::kOk, Load(bone).code) << "the scopes restored the test key";
}

TEST(ModelLoadingTrust, AnUnreadableTrustEntryIsIgnoredNeverGuessedAt) {
    const std::string good = Hex(xpe_test::detail::kKey1Xy, 64);
    for (const std::string& bad : {good.substr(0, 127), good + "0", std::string(128, 'g'), std::string(" ") + good,
                                   std::string(","), std::string("not a key")}) {
        const TrustScope scope(bad.c_str());
        EXPECT_TRUE(xpe::ai::TrustedModelKeys().empty()) << "entry of " << bad.size() << " characters";
    }
    const std::string mixed = std::string("zz,") + good + ",";
    const TrustScope scope(mixed.c_str());
    EXPECT_EQ(1u, xpe::ai::TrustedModelKeys().size()) << "one good entry among bad ones";
}

// ===== what was verified is what is used ======================================================================

#ifdef XPE_AI_TEST_HOOKS
namespace {
std::string g_swapModelTo;       // the file the hook puts in place of the model
std::string g_swapSidecarTo;     // and in place of the sidecar ("" = leave it)
void SwapFiles(const std::string& modelPath) {
    const fs::path m(modelPath);
    Write(m, xpe_test::ReadBytes(g_swapModelTo));
    if (!g_swapSidecarTo.empty()) {
        fs::path side = m;
        side.replace_extension(".json");
        std::ofstream f(side, std::ios::binary | std::ios::trunc);
        f << g_swapSidecarTo;
    }
}
}  // namespace

TEST(ModelLoadingTrust, FilesChangedBetweenTheCheckAndTheLoadDoNotReachTheSession) {
    if (IsStub()) GTEST_SKIP() << "stub build: no session is built from the bytes, so there is nothing to swap under";
    const TempDir t("toctou");
    t.CopyFrom("models_x2");
    g_swapModelTo = (fs::path(kData) / "models_x3" / "bone_suppress.onnx").string();   // another VALID model: x3
    g_swapSidecarTo.clear();
    xpe::ai::TestSetAfterVerifyHook(&SwapFiles);
    auto created = OnnxSession::Create(Cfg(t / "bone_suppress.onnx"));
    xpe::ai::TestSetAfterVerifyHook(nullptr);
    ASSERT_TRUE(created.has_value()) << created.message;
    // the file on disk IS the other model now ...
    EXPECT_EQ(xpe_test::ReadBytes(t / "bone_suppress.onnx"), xpe_test::ReadBytes(g_swapModelTo));
    // ... and the session is still the verified one: x2 doubles its input, x3 would triple it
    const std::vector<float> in = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    const auto out = created.value->Run(in);
    ASSERT_TRUE(out.has_value()) << out.message;
    ASSERT_EQ(in.size(), out.value.size());
    for (size_t i = 0; i < in.size(); ++i) EXPECT_EQ(in[i] * 2.0f, out.value[i]) << "pixel " << i;
}

TEST(ModelLoadingTrust, ASwappedModelFileThatIsGarbageDoesNotBreakAVerifiedLoadEither) {
    if (IsStub()) GTEST_SKIP() << "stub build: no session is built from the bytes";
    const TempDir t("toctou_garbage");
    t.CopyFrom("models_x2");
    const fs::path garbage = t.path / "garbage.bin";
    Write(garbage, std::vector<uint8_t>(64, 'Z'));
    g_swapModelTo = garbage.string();
    g_swapSidecarTo.clear();
    xpe::ai::TestSetAfterVerifyHook(&SwapFiles);
    auto created = OnnxSession::Create(Cfg(t / "bone_suppress.onnx"));
    xpe::ai::TestSetAfterVerifyHook(nullptr);
    EXPECT_TRUE(created.has_value()) << "the load used the verified bytes, so what is on disk afterwards cannot fail it";
}

TEST(ModelLoadingTrust, TheSidecarTheCallerReadsIsTheOneThatWasVerified) {
    if (IsStub()) GTEST_SKIP() << "stub build: the hook sits before the runtime arm only in a full build";
    const TempDir t("toctou_sidecar");
    t.CopyFrom("models_bodypart_a");
    const std::vector<uint8_t> original = xpe_test::ReadBytes(t / "bodypart.json");
    g_swapModelTo = (fs::path(kData) / "models_bodypart_a" / "bodypart.onnx").string();   // the model stays
    g_swapSidecarTo = "{\"labels\": [\"HAND\", \"HAND\", \"HAND\"]}";
    xpe::ai::TestSetAfterVerifyHook(&SwapFiles);
    auto created = OnnxSession::Create(Cfg(t / "bodypart.onnx", "bodypart"));
    xpe::ai::TestSetAfterVerifyHook(nullptr);
    ASSERT_TRUE(created.has_value()) << created.message;
    const std::string* side = created.value->VerifiedSidecar();
    ASSERT_NE(nullptr, side);
    EXPECT_EQ(std::string(original.begin(), original.end()), *side) << "the verified text, not the file as it is now";
    const std::vector<uint8_t> now = xpe_test::ReadBytes(t / "bodypart.json");
    EXPECT_NE(original, now) << "the swap really happened";
}
#endif  // XPE_AI_TEST_HOOKS

// ===== at the C ABI (the interim behaviour; QA-B-195 M4 makes the reason visible) ===============================

TEST(ModelLoadingTrust, ATamperedBoneModelIsRefusedAtTheCAbiAndTheOutputIsNotTouched) {
    const TempDir t("abi_bone");
    t.CopyFrom("models_x2");
    std::vector<uint8_t> model = xpe_test::ReadBytes(t / "bone_suppress.onnx");
    model[model.size() / 2] ^= 0x01;
    Write(t / "bone_suppress.onnx", model);

    xpe_ai_shutdown();
    ASSERT_EQ(XPE_OK, xpe_ai_init(t.path.string().c_str(), "{}"));
    std::vector<float> in(9, 1.0f), out(9, -777.0f);
    XpeImageBuffer ib{}, ob{};
    ib.width = ib.height = ob.width = ob.height = 3;
    ib.bitsAllocated = ib.bitsStored = ob.bitsAllocated = ob.bitsStored = 32;
    ib.format = ob.format = XPE_PIXEL_FLOAT32;
    ib.data = in.data();
    ob.data = out.data();
    ib.dataSize = ob.dataSize = in.size() * sizeof(float);
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_bone_suppress(&ib, &ob, nullptr)) << "the code of 'no usable model'";
    for (const float v : out) EXPECT_EQ(-777.0f, v);
    xpe_ai_shutdown();
}

namespace {
struct PartResult {
    XpeErrorCode rc;
    std::string label;
    float confidence;
};
PartResult Recognize() {
    std::vector<float> px(16, 0.0f);
    XpeImageBuffer ib{};
    ib.width = ib.height = 4;
    ib.bitsAllocated = ib.bitsStored = 32;
    ib.format = XPE_PIXEL_FLOAT32;
    ib.data = px.data();
    ib.dataSize = px.size() * sizeof(float);
    char label[64];
    std::memset(label, 'x', sizeof(label));
    float conf = -1.0f;
    PartResult r;
    r.rc = xpe_bodypart_recognize(&ib, label, sizeof(label), &conf);
    r.label = std::string(label, strnlen(label, sizeof(label)));
    r.confidence = conf;
    return r;
}
}  // namespace

TEST(ModelLoadingTrust, ATamperedBodyPartSidecarIsUnavailableOnBothPathsAndNeverAWorkerFailure) {
    if (IsStub()) GTEST_SKIP() << "stub build: xpe_bodypart_recognize answers before it looks for a model";
    const TempDir t("abi_part");
    t.CopyFrom("models_bodypart_a");
    const std::string hands = "{\"labels\": [\"HAND\", \"HAND\", \"HAND\"]}";
    Write(t / "bodypart.json", std::vector<uint8_t>(hands.begin(), hands.end()));   // the signature is now stale

    for (const bool worker : {false, true}) {
        xpe_ai_shutdown();
        xpe_clear_alerts();
        ASSERT_EQ(XPE_OK, xpe_ai_init(t.path.string().c_str(), worker ? "{\"use_worker\": true}" : "{}"));
        for (int i = 0; i < 4; ++i) {   // past the ceiling of 3
            const PartResult r = Recognize();
            EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, r.rc) << "worker=" << worker << " call " << i << ": the documented fallback signal";
            EXPECT_EQ("UNKNOWN", r.label) << "never the replaced label 'HAND'";
        }
        if (worker) {
            int32_t state = -1;
            uint32_t failures = 777;
            ASSERT_EQ(XPE_OK, xpe_ai_worker_state(&state, &failures, nullptr));
            EXPECT_EQ(XPE_AI_WORKER_ACTIVE, state);
            EXPECT_EQ(0u, failures) << "a model that is not trusted is 'unavailable', not a worker failure";
        }
    }
    xpe_ai_shutdown();
    xpe_clear_alerts();

    // the control: the same directory, signature renewed, answers with the label that is in it
    ASSERT_TRUE(xpe_test::SignDir(t.path, "bodypart"));
    for (const bool worker : {false, true}) {
        xpe_ai_shutdown();
        ASSERT_EQ(XPE_OK, xpe_ai_init(t.path.string().c_str(), worker ? "{\"use_worker\": true}" : "{}"));
        const PartResult r = Recognize();
        EXPECT_EQ(XPE_OK, r.rc) << "worker=" << worker;
        EXPECT_EQ("HAND", r.label) << "worker=" << worker << ": the sidecar's own label, once its signature is current";
    }
    xpe_ai_shutdown();
}

#ifdef XPE_AI_TEST_HOOKS
namespace {
std::string g_abiSwapModelTo;     // "" = leave the model
std::string g_abiSwapSidecarTo;   // "" = leave the sidecar
void AbiSwap(const char* modelPath) {
    const fs::path m(modelPath);
    if (!g_abiSwapModelTo.empty()) Write(m, xpe_test::ReadBytes(g_abiSwapModelTo));
    if (!g_abiSwapSidecarTo.empty()) {
        fs::path side = m;
        side.replace_extension(".json");
        std::ofstream f(side, std::ios::binary | std::ios::trunc);
        f << g_abiSwapSidecarTo;
    }
}
}  // namespace

TEST(ModelLoadingTrust, EndToEndTheBoneModelThatRunsIsTheVerifiedOneEvenIfTheFileIsSwappedAfterTheCheck) {
    if (IsStub()) GTEST_SKIP() << "stub build: no model runs";
    const TempDir t("abi_toctou_bone");
    t.CopyFrom("models_x2");
    g_abiSwapModelTo = (fs::path(kData) / "models_x3" / "bone_suppress.onnx").string();   // a valid model that TRIPLES
    g_abiSwapSidecarTo.clear();
    xpe_ai_shutdown();
    ASSERT_EQ(XPE_OK, xpe_ai_init(t.path.string().c_str(), "{}"));
    xpe_ai_test_set_after_verify_hook(&AbiSwap);
    std::vector<float> in(9, 1.0f), out(9, -777.0f);
    XpeImageBuffer ib{}, ob{};
    ib.width = ib.height = ob.width = ob.height = 3;
    ib.bitsAllocated = ib.bitsStored = ob.bitsAllocated = ob.bitsStored = 32;
    ib.format = ob.format = XPE_PIXEL_FLOAT32;
    ib.data = in.data();
    ob.data = out.data();
    ib.dataSize = ob.dataSize = in.size() * sizeof(float);
    const XpeErrorCode rc = xpe_bone_suppress(&ib, &ob, nullptr);
    xpe_ai_test_set_after_verify_hook(nullptr);
    xpe_ai_shutdown();
    EXPECT_EQ(XPE_OK, rc);
    EXPECT_EQ(xpe_test::ReadBytes(t / "bone_suppress.onnx"), xpe_test::ReadBytes(g_abiSwapModelTo)) << "the swap really happened";
    for (const float v : out) EXPECT_EQ(2.0f, v) << "the x2 model that was verified, not the x3 that was put on disk afterwards";
}

TEST(ModelLoadingTrust, EndToEndTheLabelsThatAreReturnedAreTheVerifiedOnesEvenIfTheSidecarIsSwappedAfterTheCheck) {
    if (IsStub()) GTEST_SKIP() << "stub build: xpe_bodypart_recognize answers before it looks for a model";
    const TempDir t("abi_toctou_part");
    t.CopyFrom("models_bodypart_a");
    g_abiSwapModelTo.clear();
    g_abiSwapSidecarTo = "{\"labels\": [\"HAND\", \"HAND\", \"HAND\"]}";
    xpe_ai_shutdown();
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_ai_init(t.path.string().c_str(), "{}"));
    xpe_ai_test_set_after_verify_hook(&AbiSwap);
    const PartResult r = Recognize();
    xpe_ai_test_set_after_verify_hook(nullptr);
    xpe_ai_shutdown();
    xpe_clear_alerts();
    const std::vector<uint8_t> now = xpe_test::ReadBytes(t / "bodypart.json");
    EXPECT_EQ(g_abiSwapSidecarTo, std::string(now.begin(), now.end())) << "the swap really happened";
    EXPECT_EQ(XPE_OK, r.rc);
    EXPECT_EQ("CHEST", r.label) << "the sidecar that was verified, not the one with 'HAND' put on disk afterwards";
}
#endif  // XPE_AI_TEST_HOOKS

TEST(ModelLoadingTrust, ASidecarOrSignatureThatExistsButCannotBeReadIsRefusedNotSkipped) {
    // A directory where the file should be: it exists, and it cannot be read. "I could not read it" must never turn
    // into "there is nothing to check".
    {
        const TempDir t("unreadable_sig");
        t.CopyFrom("models_x2");
        fs::remove(t / "bone_suppress.sig");
        fs::create_directory(t / "bone_suppress.sig");
        const Outcome o = Load(t / "bone_suppress.onnx");
        EXPECT_EQ(OnnxErrorCode::kModelNotTrusted, o.code);
        EXPECT_TRUE(o.Names(SignatureStatus::kVerifierError)) << o.message;
    }
    const TempDir t("unreadable_sidecar");
    t.CopyFrom("models_x2");
    fs::create_directory(t / "bone_suppress.json");
    const Outcome o = Load(t / "bone_suppress.onnx");
    EXPECT_EQ(OnnxErrorCode::kModelNotTrusted, o.code);
    EXPECT_TRUE(o.Names(SignatureStatus::kVerifierError)) << o.message;
}
