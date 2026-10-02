/**
 * @file test_ai_model_assets_signed.cpp
 * @brief Every AI test model carries a current signature (QA-B-195 M2, REQ-AI-007 / REQ-AI-091).
 *
 * Once model loading verifies signatures (M3), a model that was changed without its signature being renewed would
 * stop loading and the test that uses it would fail for a reason that has nothing to do with what it tests. This
 * test makes that failure early and specific: it walks tests/data, verifies every `.onnx` against its `.sig` with
 * the C++ verifier and the committed TEST key, and names the file that is stale.
 *
 * The rules repeat tools/ai/xpe_model_signing.py on purpose (the C++ side must not depend on Python):
 *   role     = the file stem when it is "bone_suppress" or "bodypart", otherwise "bone_suppress"
 *   sidecar  = <stem>.json beside the model, when it exists (its ABSENCE is part of what was signed)
 *   sig file = <stem>.sig beside the model
 * Renew after changing a model or sidecar:  python tools/ai/xpe_model_signing.py sign-test-assets
 * (the generators make_min_models.py / make_bodypart_models.py do it themselves).
 */

#include <gtest/gtest.h>

#include "ai_model_signer.h"

#include <cstdint>
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
using xpe::ai::Bytes;
using xpe::ai::SignatureStatus;
using xpe::ai::TrustedKey;

#include "signer_vectors.inc"   // kKey1Xy: the public half of the committed TEST key (test_key_1.pem)

struct Asset {
    fs::path model;
    std::string role;
    bool hasSidecar{false};
    std::vector<uint8_t> modelBytes;
    std::vector<uint8_t> sidecarBytes;
    fs::path sigPath;
};

std::vector<uint8_t> ReadAll(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

bool Skipped(const fs::path& p) {
    for (const auto& part : p) {
        if (part == "signing" || part == "__pycache__") return true;
    }
    return false;
}

std::vector<Asset> Assets() {
    std::vector<Asset> out;
    const fs::path root = XPE_AI_TEST_DATA_DIR;
    for (const auto& e : fs::recursive_directory_iterator(root)) {
        if (!e.is_regular_file() || e.path().extension() != ".onnx" || Skipped(fs::relative(e.path(), root))) continue;
        Asset a;
        a.model = e.path();
        const std::string stem = e.path().stem().string();
        a.role = (stem == "bone_suppress" || stem == "bodypart") ? stem : "bone_suppress";
        const fs::path side = e.path().parent_path() / (stem + ".json");
        a.hasSidecar = fs::exists(side);
        a.modelBytes = ReadAll(e.path());
        if (a.hasSidecar) a.sidecarBytes = ReadAll(side);
        a.sigPath = e.path().parent_path() / (stem + ".sig");
        out.push_back(std::move(a));
    }
    return out;
}

TrustedKey TestKey() {
    TrustedKey k{};
    std::memcpy(k.xy, kKey1Xy, 64);
    EXPECT_TRUE(xpe::ai::FillKeyId(&k));
    return k;
}

const uint8_t kOne = 0;

SignatureStatus Check(const Asset& a, const std::string& role, const std::vector<uint8_t>& model, bool withSidecar,
                      const std::vector<uint8_t>& sig) {
    const TrustedKey key = TestKey();
    const Bytes m{model.empty() ? &kOne : model.data(), model.size()};
    Bytes s{a.sidecarBytes.empty() ? &kOne : a.sidecarBytes.data(), a.sidecarBytes.size()};
    return xpe::ai::VerifyModelSignature(&key, 1, role, m, withSidecar ? &s : nullptr, sig.empty() ? nullptr : sig.data(), sig.size());
}

}  // namespace

TEST(ModelAssets, EveryTestModelHasACurrentValidSignature) {
    const std::vector<Asset> assets = Assets();
    // The control that makes "all of them verify" mean something: the walk found the assets at all.
    ASSERT_GE(assets.size(), 19u) << "tests/data holds 19 models as of QA-B-195; the walk found fewer";
    for (const Asset& a : assets) {
        const std::string name = fs::relative(a.model, XPE_AI_TEST_DATA_DIR).generic_string();
        if (!fs::exists(a.sigPath)) {
            ADD_FAILURE() << name << ": no signature file beside it (python tools/ai/xpe_model_signing.py sign-test-assets)";
            continue;
        }
        EXPECT_EQ(SignatureStatus::kOk, Check(a, a.role, a.modelBytes, a.hasSidecar, ReadAll(a.sigPath)))
            << name << ": the signature is stale -- the model or its sidecar changed without the signature being renewed";
    }
}

TEST(ModelAssets, NoSignatureFileIsLeftBehindWithoutItsModel) {
    const fs::path root = XPE_AI_TEST_DATA_DIR;
    size_t sigs = 0;
    for (const auto& e : fs::recursive_directory_iterator(root)) {
        if (!e.is_regular_file() || e.path().extension() != ".sig" || Skipped(fs::relative(e.path(), root))) continue;
        ++sigs;
        const fs::path model = e.path().parent_path() / (e.path().stem().string() + ".onnx");
        EXPECT_TRUE(fs::exists(model)) << fs::relative(e.path(), root).generic_string() << ": a signature with no model beside it";
    }
    EXPECT_GE(sigs, 19u) << "the walk found fewer signature files than there are models";
}

TEST(ModelAssets, TheCheckAboveCanFail) {
    // Without this, "every signature verifies" could be a helper that verifies nothing. Each of these is a staleness
    // the walk must be able to see: a changed model byte, a changed role, a sidecar dropped, a signature of another model.
    const std::vector<Asset> assets = Assets();
    ASSERT_GE(assets.size(), 19u);
    const Asset* withSidecar = nullptr;
    const Asset* other = nullptr;
    for (const Asset& a : assets) {
        if (a.hasSidecar && !withSidecar) withSidecar = &a;
        else if (withSidecar && !other && a.modelBytes != withSidecar->modelBytes) other = &a;
    }
    ASSERT_NE(nullptr, withSidecar);
    ASSERT_NE(nullptr, other);
    const std::vector<uint8_t> sig = ReadAll(withSidecar->sigPath);
    ASSERT_EQ(SignatureStatus::kOk, Check(*withSidecar, withSidecar->role, withSidecar->modelBytes, true, sig)) << "the control";

    std::vector<uint8_t> changed = withSidecar->modelBytes;
    changed[changed.size() / 2] ^= 0x01;
    EXPECT_EQ(SignatureStatus::kBadSignature, Check(*withSidecar, withSidecar->role, changed, true, sig)) << "a model byte changed";
    EXPECT_EQ(SignatureStatus::kBadSignature,
              Check(*withSidecar, withSidecar->role == "bodypart" ? "bone_suppress" : "bodypart", withSidecar->modelBytes, true, sig))
        << "a different role";
    EXPECT_EQ(SignatureStatus::kBadSignature, Check(*withSidecar, withSidecar->role, withSidecar->modelBytes, false, sig))
        << "the sidecar dropped";
    EXPECT_EQ(SignatureStatus::kBadSignature,
              Check(*withSidecar, withSidecar->role, withSidecar->modelBytes, true, ReadAll(other->sigPath)))
        << "the signature of another model";
}
