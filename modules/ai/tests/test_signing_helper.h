/**
 * @file test_signing_helper.h
 * @brief Test-only: sign a fixture a test creates at run time (QA-B-195 M3).
 *
 * Model loading refuses a model whose signature is not current, so a test that writes a temporary model directory --
 * a copy of a model with a different sidecar, say -- has to sign what it wrote or the loader would refuse it for a
 * reason that is not what the test is about. This signs with the committed TEST key (tests/data/signing/test_key_1.pem,
 * whose private scalar is public in this repository and which the product never trusts) over CNG.
 *
 * It builds the signed message itself, a third implementation of the format next to the Python one and the verifier.
 * That is deliberate and checked: the format is pinned by the Python-made vectors (test_ai_model_signer.cpp), and
 * ModelLoadingTrust.TheTestSigningHelper... verifies that what this writes is accepted by the verifier AND that a
 * wrong byte is not.
 *
 * Header-only; include it from a test that creates model directories. Do not include it in a file that also
 * includes signer_vectors.inc directly.
 */
#pragma once

#include <windows.h>
#include <bcrypt.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "ai_model_signer.h"

namespace xpe_test {

namespace detail {
#include "signer_vectors.inc"   // kKey1Xy, kKey1D: the TEST key 1
}  // namespace detail

inline std::vector<uint8_t> ReadBytes(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

/** The 78-byte `.sig` file for (role, model, sidecar) under the TEST key. `sidecar` null = the model has none. Empty on failure. */
inline std::vector<uint8_t> SignWithTestKey(const std::string& role, const std::vector<uint8_t>& model,
                                            const std::vector<uint8_t>* sidecar) {
    std::vector<uint8_t> msg;
    auto put = [&msg](const void* d, size_t n) {
        const uint8_t* b = static_cast<const uint8_t*>(d);
        msg.insert(msg.end(), b, b + n);
    };
    auto u64 = [&put](uint64_t v) {
        uint8_t b[8];
        for (int i = 0; i < 8; ++i) b[i] = static_cast<uint8_t>((v >> (8 * i)) & 0xFF);
        put(b, 8);
    };
    put("XPE-MODEL-SIG-1\n", 16);
    const uint8_t rl[2] = {static_cast<uint8_t>(role.size() & 0xFF), static_cast<uint8_t>(role.size() >> 8)};
    put(rl, 2);
    put(role.data(), role.size());
    u64(model.size());
    put(model.data(), model.size());
    if (sidecar == nullptr) {
        const uint8_t no = 0;
        put(&no, 1);
    } else {
        const uint8_t yes = 1;
        put(&yes, 1);
        u64(sidecar->size());
        put(sidecar->data(), sidecar->size());
    }

    uint8_t digest[32];
    BCRYPT_ALG_HANDLE sha = nullptr;
    if (BCryptOpenAlgorithmProvider(&sha, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return {};
    const LONG hs = BCryptHash(sha, nullptr, 0, msg.data(), static_cast<ULONG>(msg.size()), digest, 32);
    BCryptCloseAlgorithmProvider(sha, 0);
    if (hs < 0) return {};

    BCRYPT_ALG_HANDLE ec = nullptr;
    if (BCryptOpenAlgorithmProvider(&ec, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) < 0) return {};
    struct {
        BCRYPT_ECCKEY_BLOB header;
        uint8_t xy[64];
        uint8_t d[32];
    } blob;
    blob.header.dwMagic = BCRYPT_ECDSA_PRIVATE_P256_MAGIC;
    blob.header.cbKey = 32;
    std::memcpy(blob.xy, detail::kKey1Xy, 64);
    std::memcpy(blob.d, detail::kKey1D, 32);
    BCRYPT_KEY_HANDLE key = nullptr;
    std::vector<uint8_t> out;
    if (BCryptImportKeyPair(ec, nullptr, BCRYPT_ECCPRIVATE_BLOB, &key, reinterpret_cast<PUCHAR>(&blob),
                            static_cast<ULONG>(sizeof(blob)), 0) >= 0) {
        uint8_t sig[64];
        ULONG got = 0;
        xpe::ai::TrustedKey tk{};
        std::memcpy(tk.xy, detail::kKey1Xy, 64);
        if (BCryptSignHash(key, nullptr, digest, 32, sig, 64, &got, 0) >= 0 && got == 64 && xpe::ai::FillKeyId(&tk)) {
            out = {'X', 'S', 'I', 'G', 1, 1};
            out.insert(out.end(), tk.id, tk.id + 8);
            out.insert(out.end(), sig, sig + 64);
        }
        BCryptDestroyKey(key);
    }
    BCryptCloseAlgorithmProvider(ec, 0);
    return out;
}

/**
 * Write `<dir>/<stem>.sig` for the model `<dir>/<stem>.onnx` and the sidecar `<dir>/<stem>.json` (when there is
 * one), as it is on disk NOW. Call it after the last change to either file. The role is the stem.
 */
inline bool SignDir(const std::filesystem::path& dir, const std::string& stem) {
    const std::vector<uint8_t> model = ReadBytes(dir / (stem + ".onnx"));
    const std::filesystem::path side = dir / (stem + ".json");
    std::vector<uint8_t> sidecar;
    const bool hasSidecar = std::filesystem::exists(side);
    if (hasSidecar) sidecar = ReadBytes(side);
    const std::vector<uint8_t> sig = SignWithTestKey(stem, model, hasSidecar ? &sidecar : nullptr);
    if (sig.empty()) return false;
    std::ofstream f(dir / (stem + ".sig"), std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(sig.data()), static_cast<std::streamsize>(sig.size()));
    return static_cast<bool>(f);
}

/**
 * Copy `<srcDir>/<stem>.onnx`, its `.sig` and its sidecar `.json` (QA-B-197: every model has one now; the signature
 * covers it) into `dstDir`.
 */
inline bool CopyModelWithSignature(const std::string& srcDir, const std::string& stem, const std::string& dstDir) {
    for (const char* ext : {".onnx", ".sig", ".json"}) {
        const std::string from = srcDir + "/" + stem + ext;
        if (GetFileAttributesA(from.c_str()) == INVALID_FILE_ATTRIBUTES) {
            if (std::string(ext) == ".json") continue;   // a model without a sidecar: nothing to copy
            return false;
        }
        if (CopyFileA(from.c_str(), (dstDir + "/" + stem + ext).c_str(), FALSE) == 0) return false;
    }
    return true;
}

/** Remove the model, its signature and its sidecar that CopyModelWithSignature put in `dir`, then the directory. */
inline void RemoveModelDir(const std::string& dir, const std::string& stem) {
    DeleteFileA((dir + "/" + stem + ".onnx").c_str());
    DeleteFileA((dir + "/" + stem + ".sig").c_str());
    DeleteFileA((dir + "/" + stem + ".json").c_str());
    RemoveDirectoryA(dir.c_str());
}

/**
 * A sidecar that says what REQ-AI-008 requires (QA-B-197) and ALSO carries the keys of @p objectText, a JSON object
 * written as `{"labels": [...]}`: the five fields are put in front of its first key. For the tests that write their own
 * body-part sidecar and renew the signature.
 */
inline std::string WithMetadata(const std::string& objectText) {
    const size_t brace = objectText.find('{');
    if (brace == std::string::npos) return objectText;
    return objectText.substr(0, brace + 1) +
           "\"model_id\":\"toy_model\",\"version\":\"0.0.1\",\"pccp_scope\":\"none\","
           "\"training_data_hash\":\"none\",\"validation_metrics\":{\"m\":0}," +
           objectText.substr(brace + 1);
}

}  // namespace xpe_test
