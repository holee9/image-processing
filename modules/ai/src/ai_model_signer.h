/**
 * @file ai_model_signer.h
 * @brief Model signature verification: REQ-AI-007 / REQ-AI-091 (QA-B-195 M1). Internal; not installed, not exported.
 *
 * A pure function over byte buffers: given the bytes of a model file, its sidecar (if any), the role it is loaded
 * for and a detached `.sig` file, say whether a TRUSTED key signed exactly that combination. It reads no file,
 * holds no state and is not yet called by the loading code (M3 connects it).
 *
 * THE FORMAT (design memo .moai/reports/lane-post/QA-B-195/design.md sections 3 and 3.1; reference implementation
 * tools/ai/xpe_model_signing.py, which the tests use to make their signatures):
 *
 *   message   = "XPE-MODEL-SIG-1\n"
 *               || u16le(len(role)) || role                      role = "bone_suppress" | "bodypart"
 *               || u64le(len(model)) || model
 *               || u8(has_sidecar) [ || u64le(len(sidecar)) || sidecar ]
 *   signature = ECDSA P-256 over SHA-256(message), r||s (64 bytes, the IEEE P1363 form CNG takes)
 *   .sig file = "XSIG" || u8(version = 1) || u8(algorithm = 1) || key_id (8) || signature (64)   = 78 bytes
 *   key_id    = SHA-256(X || Y, 64 bytes big-endian)[:8]
 *
 * The role is signed so that a model signed for one job cannot be put where another is expected. The sidecar is
 * signed because the labels in it change what a result means (a sidecar-only swap changed a body-part label in the
 * QA-B-195 probe). "No sidecar" and "an empty sidecar" are different messages.
 *
 * WHAT THIS DOES NOT GUARD AGAINST, stated here because the guarantee depends on it (design decision D3): the
 * trusted public keys live in the DLL and the worker executable, so an attacker who can replace THOSE can replace the
 * keys. This verifies that the model files were not changed; the protection of the executables themselves
 * (Authenticode, the install directory's permissions) is outside this module. An older, validly signed model is
 * accepted: rollback protection is not part of this format (D7).
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace xpe::ai {

/**
 * Largest model or sidecar this verifier will hash (QA-B-195 D8). THIS IS AN IMPLEMENTATION SAFETY CAP, NOT A
 * REQUIREMENT: no document gives a maximum model size. The caller reads a file into memory before verifying it, so
 * an unbounded size would let a file name decide how much memory is taken.
 */
constexpr size_t kMaxSignedFileBytes = size_t{1} << 30;

/** Exact size of a `.sig` file. */
constexpr size_t kSignatureFileBytes = 4 + 1 + 1 + 8 + 64;

/** A public key the verifier trusts: the P-256 point X||Y (big-endian, 32 bytes each) and its 8-byte key id. */
struct TrustedKey {
    uint8_t id[8];
    uint8_t xy[64];
};

/** Fill `key->id` from `key->xy` (the first 8 bytes of SHA-256 of the 64 point bytes). False if hashing is unavailable. */
bool FillKeyId(TrustedKey* key);

/** Why a verification did not succeed. Each value is a different thing a person has to do something about. */
enum class SignatureStatus {
    kOk,                  ///< a trusted key signed exactly this role, model and sidecar
    kNoSignatureFile,     ///< no `.sig` was supplied (a null pointer)
    kMalformedSignature,  ///< not 78 bytes, or the magic is not "XSIG"
    kUnsupportedVersion,  ///< a version or algorithm this verifier does not know
    kUnknownKey,          ///< the key id is not in the trusted list
    kBadSignature,        ///< the signature does not match: the model, sidecar, role or signature was changed
    kTooLarge,            ///< the model or sidecar is above kMaxSignedFileBytes
    kVerifierError,       ///< the platform crypto could not run (fail closed: never read as a pass)
};

/** A fixed, human-readable reason class for an alert or a log line. Never null. */
const char* SignatureStatusText(SignatureStatus status);

/**
 * The keys this build trusts.
 *
 * THE PRODUCTION LIST IS EMPTY, ON PURPOSE (design decision D5, user decision 2026-10-03): the production signing key
 * is chosen when the real models arrive (#243, mandatory before shipping). An empty list trusts nothing, so a build
 * without test keys REFUSES EVERY MODEL, including a validly signed one. That is the intended state while no real
 * model exists; it is not a defect to "fix" by trusting more.
 *
 * Test keys (design decision D4): ONLY in a build compiled with XPE_AI_TEST_HOOKS, the environment variable
 * XPE_AI_TEST_TRUSTED_KEYS adds keys -- one or more 128-hex-character P-256 points (X||Y), separated by commas.
 * The variable is read on every call, so a test can change what is trusted between two loads. A delivery build
 * (-DXPE_AI_TEST_HOOKS=OFF) does not contain this code at all. The committed TEST private keys are public; they are
 * never in the production list.
 */
std::vector<TrustedKey> TrustedModelKeys();

/** A byte range. `data` may be null only when `size` is 0. */
struct Bytes {
    const uint8_t* data;
    size_t size;
};

/**
 * Verify a detached signature.
 *
 * @param keys      the trusted keys (may be empty: then every signature is kUnknownKey, which is what a production
 *                  build with no production key yet does -- design decision D5)
 * @param role      the role the model is being loaded for ("bone_suppress" or "bodypart")
 * @param model     the model file's bytes
 * @param sidecar   the sidecar's bytes, or nullptr when the model has none
 * @param sigFile   the `.sig` file's bytes, or nullptr when there is none
 * @param sigSize   its size
 * @return kOk only when a key in @p keys with the signature's key id verifies the signature over exactly this
 *         role, model and sidecar. Anything else, including a failure of the platform crypto, is a refusal.
 *
 * Checks run cheapest first and nothing is hashed for a signature that cannot be right (wrong size, unknown
 * version, unknown key). Makes no operator new allocation (Windows CNG allocates inside its own provider).
 */
SignatureStatus VerifyModelSignature(const TrustedKey* keys, size_t keyCount, std::string_view role, Bytes model,
                                     const Bytes* sidecar, const uint8_t* sigFile, size_t sigSize);

}  // namespace xpe::ai
