/**
 * @file ai_model_signer.cpp
 * @brief Model signature verification over Windows CNG (QA-B-195 M1). See ai_model_signer.h for the format.
 *
 * CNG, because the module already depends on Windows (the worker pipe, GetModuleHandleEx) and because CNG opens the
 * ECDSA P-256 provider with no new dependency -- it does not open Ed25519 (measured, QA-B-195: 0xC0000225), which is
 * why decision D1 chose P-256. Allocation inside CNG goes through CNG's own allocator, not operator new; this file
 * itself allocates nothing on the heap.
 */

#include "ai_model_signer.h"

#include <windows.h>
#include <bcrypt.h>

#include <cstring>
#include <string>

#pragma comment(lib, "bcrypt.lib")

namespace xpe::ai {

namespace {

constexpr char kPrefix[] = "XPE-MODEL-SIG-1\n";
constexpr size_t kPrefixLen = sizeof(kPrefix) - 1;
constexpr uint8_t kVersion = 1;
constexpr uint8_t kAlgorithmEcdsaP256Sha256 = 1;
constexpr LONG kStatusInvalidSignature = static_cast<LONG>(0xC000A000);   // STATUS_INVALID_SIGNATURE

bool Ok(LONG status) { return status >= 0; }

/** RAII over a CNG algorithm provider. */
struct Provider {
    BCRYPT_ALG_HANDLE handle{nullptr};
    explicit Provider(LPCWSTR algorithm) {
        if (!Ok(BCryptOpenAlgorithmProvider(&handle, algorithm, nullptr, 0))) handle = nullptr;
    }
    ~Provider() {
        if (handle) BCryptCloseAlgorithmProvider(handle, 0);
    }
    Provider(const Provider&) = delete;
    Provider& operator=(const Provider&) = delete;
};

/** RAII over a CNG hash. Data is fed in chunks: CNG takes a 32-bit length. */
struct Sha256 {
    Provider provider{BCRYPT_SHA256_ALGORITHM};
    BCRYPT_HASH_HANDLE handle{nullptr};
    bool ok{false};

    Sha256() {
        if (provider.handle && Ok(BCryptCreateHash(provider.handle, &handle, nullptr, 0, nullptr, 0, 0))) ok = true;
    }
    ~Sha256() {
        if (handle) BCryptDestroyHash(handle);
    }
    Sha256(const Sha256&) = delete;
    Sha256& operator=(const Sha256&) = delete;

    void Update(const void* data, size_t size) {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        constexpr size_t kChunk = size_t{1} << 26;   // 64 MiB
        while (ok && size > 0) {
            const size_t n = size < kChunk ? size : kChunk;
            if (!Ok(BCryptHashData(handle, const_cast<PUCHAR>(p), static_cast<ULONG>(n), 0))) ok = false;
            p += n;
            size -= n;
        }
    }
    void U16(uint16_t v) {
        const uint8_t b[2] = {static_cast<uint8_t>(v & 0xFF), static_cast<uint8_t>(v >> 8)};
        Update(b, sizeof(b));
    }
    void U64(uint64_t v) {
        uint8_t b[8];
        for (int i = 0; i < 8; ++i) b[i] = static_cast<uint8_t>((v >> (8 * i)) & 0xFF);
        Update(b, sizeof(b));
    }
    void U8(uint8_t v) { Update(&v, 1); }
    bool Finish(uint8_t out[32]) {
        return ok && Ok(BCryptFinishHash(handle, out, 32, 0));
    }
};

#ifdef XPE_AI_TEST_HOOKS
int HexValue(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

/** Add the keys named by XPE_AI_TEST_TRUSTED_KEYS. An entry that is not exactly 128 hex characters is ignored (fewer keys, never more). */
void AddTestKeysFromEnvironment(std::vector<TrustedKey>* keys) {
    char buf[8192];
    const DWORD n = GetEnvironmentVariableA("XPE_AI_TEST_TRUSTED_KEYS", buf, sizeof(buf));
    if (n == 0 || n >= sizeof(buf)) return;
    const std::string text(buf, n);
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t end = text.find(',', pos);
        if (end == std::string::npos) end = text.size();
        const std::string entry = text.substr(pos, end - pos);
        pos = end + 1;
        if (entry.size() != 128) continue;
        TrustedKey k{};
        bool ok = true;
        for (size_t i = 0; i < 64 && ok; ++i) {
            const int hi = HexValue(entry[2 * i]);
            const int lo = HexValue(entry[2 * i + 1]);
            if (hi < 0 || lo < 0) ok = false;
            else k.xy[i] = static_cast<uint8_t>(hi * 16 + lo);
        }
        if (ok && FillKeyId(&k)) keys->push_back(k);
    }
}
#endif

}  // namespace

bool FillKeyId(TrustedKey* key) {
    Sha256 h;
    h.Update(key->xy, sizeof(key->xy));
    uint8_t digest[32];
    if (!h.Finish(digest)) return false;
    std::memcpy(key->id, digest, sizeof(key->id));
    return true;
}

std::vector<TrustedKey> TrustedModelKeys() {
    std::vector<TrustedKey> keys;
    // The production list. EMPTY until the production key exists (#243); see ai_model_signer.h.
#ifdef XPE_AI_TEST_HOOKS
    AddTestKeysFromEnvironment(&keys);
#endif
    return keys;
}

const char* SignatureStatusText(SignatureStatus status) {
    switch (status) {
        case SignatureStatus::kOk: return "signature verified";
        case SignatureStatus::kNoSignatureFile: return "no signature file";
        case SignatureStatus::kMalformedSignature: return "the signature file is malformed";
        case SignatureStatus::kUnsupportedVersion: return "the signature file has an unsupported version or algorithm";
        case SignatureStatus::kUnknownKey: return "the signature was made by a key that is not trusted";
        case SignatureStatus::kBadSignature: return "the signature does not match the model files";
        case SignatureStatus::kTooLarge: return "the model files are larger than the verifier accepts";
        case SignatureStatus::kVerifierError: return "the signature check could not be run";
    }
    return "unknown";
}

SignatureStatus VerifyModelSignature(const TrustedKey* keys, size_t keyCount, std::string_view role, Bytes model,
                                     const Bytes* sidecar, const uint8_t* sigFile, size_t sigSize) {
    // 1. The signature file itself, cheapest checks first.
    if (sigFile == nullptr) return SignatureStatus::kNoSignatureFile;
    if (sigSize != kSignatureFileBytes || std::memcmp(sigFile, "XSIG", 4) != 0) return SignatureStatus::kMalformedSignature;
    if (sigFile[4] != kVersion || sigFile[5] != kAlgorithmEcdsaP256Sha256) return SignatureStatus::kUnsupportedVersion;

    // 2. Sizes: nothing is hashed above the cap, and a role that cannot be length-prefixed is not a role.
    if (model.size > kMaxSignedFileBytes || (sidecar && sidecar->size > kMaxSignedFileBytes)) return SignatureStatus::kTooLarge;
    if (role.size() > 0xFFFF) return SignatureStatus::kBadSignature;

    // 3. The key: by id, before any hashing.
    const uint8_t* keyId = sigFile + 6;
    const TrustedKey* key = nullptr;
    for (size_t i = 0; i < keyCount; ++i) {
        if (std::memcmp(keys[i].id, keyId, sizeof(keys[i].id)) == 0) {
            key = &keys[i];
            break;
        }
    }
    if (key == nullptr) return SignatureStatus::kUnknownKey;

    // 4. The digest of exactly the message the format defines.
    Sha256 h;
    h.Update(kPrefix, kPrefixLen);
    h.U16(static_cast<uint16_t>(role.size()));
    h.Update(role.data(), role.size());
    h.U64(model.size);
    h.Update(model.data, model.size);
    if (sidecar == nullptr) {
        h.U8(0);
    } else {
        h.U8(1);
        h.U64(sidecar->size);
        h.Update(sidecar->data, sidecar->size);
    }
    uint8_t digest[32];
    if (!h.Finish(digest)) return SignatureStatus::kVerifierError;

    // 5. The signature, under the key the id named.
    Provider ecdsa(BCRYPT_ECDSA_P256_ALGORITHM);
    if (!ecdsa.handle) return SignatureStatus::kVerifierError;
    struct {
        BCRYPT_ECCKEY_BLOB header;
        uint8_t xy[64];
    } blob;
    blob.header.dwMagic = BCRYPT_ECDSA_PUBLIC_P256_MAGIC;
    blob.header.cbKey = 32;
    std::memcpy(blob.xy, key->xy, sizeof(blob.xy));
    BCRYPT_KEY_HANDLE hKey = nullptr;
    if (!Ok(BCryptImportKeyPair(ecdsa.handle, nullptr, BCRYPT_ECCPUBLIC_BLOB, &hKey, reinterpret_cast<PUCHAR>(&blob),
                                static_cast<ULONG>(sizeof(blob)), 0))) {
        return SignatureStatus::kVerifierError;   // a malformed key in OUR list: refuse, never skip the check
    }
    const LONG vs = BCryptVerifySignature(hKey, nullptr, digest, sizeof(digest), const_cast<PUCHAR>(sigFile + 14), 64, 0);
    BCryptDestroyKey(hKey);
    if (vs == kStatusInvalidSignature) return SignatureStatus::kBadSignature;
    if (!Ok(vs)) return SignatureStatus::kVerifierError;
    return SignatureStatus::kOk;
}

}  // namespace xpe::ai
