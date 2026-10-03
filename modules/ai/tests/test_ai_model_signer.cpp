/**
 * @file test_ai_model_signer.cpp
 * @brief The model signature verifier (QA-B-195 M1, REQ-AI-007 / REQ-AI-091).
 *
 * The signatures come from the INDEPENDENT implementation (tools/ai/xpe_model_signing.py: Python `cryptography` /
 * OpenSSL, committed as signer_vectors.inc by tests/data/signing/make_signer_vectors.py) and are checked by the
 * verifier under test (Windows CNG). A signature made here by the verifier's own code could be accepted by it while
 * wrong in the same way on both sides (DER versus r||s, byte order, how the message is assembled); two
 * implementations agreeing is the evidence.
 *
 * What is held:
 *   - every vector verifies (the controls that make every refusal below mean something);
 *   - ANY single changed byte of the model, the sidecar, the signature, or a changed role, sidecar presence or
 *     boundary between model and sidecar, is refused with the right reason;
 *   - the signature file's own defects (size, magic, version, algorithm, key id) are refused with their own reason,
 *     and a key that is not in the trusted list does not verify;
 *   - a failure of the verifier itself (here: a malformed key in the list) is a refusal, never a pass.
 * Nothing here loads a model: the verifier is not yet connected to the loading code (M3).
 */

#include <gtest/gtest.h>

#include "ai_model_signer.h"

#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {

using xpe::ai::Bytes;
using xpe::ai::SignatureStatus;
using xpe::ai::TrustedKey;

#include "signer_vectors.inc"

TrustedKey MakeKey(const uint8_t xy[64]) {
    TrustedKey k{};
    std::memcpy(k.xy, xy, 64);
    EXPECT_TRUE(xpe::ai::FillKeyId(&k));
    return k;
}

/** The model bytes of a case: its own bytes, or the formula the generator used. */
std::vector<uint8_t> ModelOf(const SigCase& c) {
    if (c.model) return std::vector<uint8_t>(c.model, c.model + c.modelSize);
    std::vector<uint8_t> v(c.modelSize);
    for (size_t i = 0; i < v.size(); ++i) v[i] = static_cast<uint8_t>((i * 31 + 7) & 0xFF);
    return v;
}

const SigCase& Case(const char* name) {
    for (const SigCase& c : kCases) {
        if (std::string(c.name) == name) return c;
    }
    ADD_FAILURE() << "no vector named " << name;
    return kCases[0];
}

const uint8_t kOne = 0;   // a valid pointer for an empty range

struct Subject {
    std::vector<TrustedKey> keys;
    Subject() : keys{MakeKey(kKey1Xy), MakeKey(kKey2Xy)} {}

    SignatureStatus Verify(const char* role, const std::vector<uint8_t>& model, const std::vector<uint8_t>* sidecar,
                           const uint8_t* sig, size_t sigSize) const {
        const Bytes m{model.empty() ? &kOne : model.data(), model.size()};
        Bytes s{};
        if (sidecar) s = Bytes{sidecar->empty() ? &kOne : sidecar->data(), sidecar->size()};
        return xpe::ai::VerifyModelSignature(keys.data(), keys.size(), role, m, sidecar ? &s : nullptr, sig, sigSize);
    }
    SignatureStatus VerifyCase(const SigCase& c) const {
        const std::vector<uint8_t> model = ModelOf(c);
        const std::vector<uint8_t> sc = c.hasSidecar ? std::vector<uint8_t>(c.sidecar ? c.sidecar : &kOne, (c.sidecar ? c.sidecar : &kOne) + c.sidecarSize) : std::vector<uint8_t>();
        return Verify(c.role, model, c.hasSidecar ? &sc : nullptr, c.sig, xpe::ai::kSignatureFileBytes);
    }
};

std::vector<uint8_t> SidecarOf(const SigCase& c) {
    return std::vector<uint8_t>(c.sidecar ? c.sidecar : &kOne, (c.sidecar ? c.sidecar : &kOne) + c.sidecarSize);
}

std::vector<uint8_t> SigOf(const SigCase& c) { return std::vector<uint8_t>(c.sig, c.sig + xpe::ai::kSignatureFileBytes); }

}  // namespace

// ===== the controls ==========================================================================================

TEST(ModelSigner, EverySignatureMadeByTheIndependentImplementationVerifies) {
    const Subject s;
    for (const SigCase& c : kCases) EXPECT_EQ(SignatureStatus::kOk, s.VerifyCase(c)) << c.name;
}

TEST(ModelSigner, TheKeyIdTheVerifierComputesIsTheOneTheOtherImplementationWroteIntoTheSignature) {
    // bytes 6..13 of a .sig are the key id the Python side computed; FillKeyId is the C++ side's computation.
    const TrustedKey k1 = MakeKey(kKey1Xy);
    const TrustedKey k2 = MakeKey(kKey2Xy);
    EXPECT_EQ(0, std::memcmp(k1.id, Case("bone_with_sidecar").sig + 6, 8));
    EXPECT_EQ(0, std::memcmp(k2.id, Case("bone_by_key2").sig + 6, 8));
    EXPECT_NE(0, std::memcmp(k1.id, k2.id, 8)) << "two keys, two ids";
}

TEST(ModelSigner, ASignatureVerifiesOnlyUnderTheKeyThatMadeItAndOnlyWhenThatKeyIsTrusted) {
    const SigCase& by2 = Case("bone_by_key2");
    const std::vector<uint8_t> model = ModelOf(by2);
    const std::vector<uint8_t> sc = SidecarOf(by2);
    const Bytes m{model.data(), model.size()};
    const Bytes sb{sc.data(), sc.size()};

    const TrustedKey only1[] = {MakeKey(kKey1Xy)};
    const TrustedKey only2[] = {MakeKey(kKey2Xy)};
    EXPECT_EQ(SignatureStatus::kUnknownKey, xpe::ai::VerifyModelSignature(only1, 1, by2.role, m, &sb, by2.sig, 78))
        << "a validly signed model whose key is not trusted";
    EXPECT_EQ(SignatureStatus::kOk, xpe::ai::VerifyModelSignature(only2, 1, by2.role, m, &sb, by2.sig, 78));
    EXPECT_EQ(SignatureStatus::kUnknownKey, xpe::ai::VerifyModelSignature(nullptr, 0, by2.role, m, &sb, by2.sig, 78))
        << "an empty trust list (a production build with no production key yet) trusts nothing";
}

// ===== a changed byte, anywhere, is refused ====================================================================

TEST(ModelSigner, AnySingleChangedByteOfTheModelIsRefused) {
    const Subject s;
    const SigCase& c = Case("bone_with_sidecar");
    const std::vector<uint8_t> sc = SidecarOf(c);
    std::vector<uint8_t> model = ModelOf(c);
    for (size_t i = 0; i < model.size(); ++i) {
        model[i] ^= 0x01;
        EXPECT_EQ(SignatureStatus::kBadSignature, s.Verify(c.role, model, &sc, c.sig, 78)) << "model byte " << i;
        model[i] ^= 0x01;
    }
    EXPECT_EQ(SignatureStatus::kOk, s.Verify(c.role, model, &sc, c.sig, 78)) << "restored";
}

TEST(ModelSigner, AnySingleChangedByteOfTheSidecarIsRefused) {
    const Subject s;
    const SigCase& c = Case("bone_with_sidecar");
    const std::vector<uint8_t> model = ModelOf(c);
    std::vector<uint8_t> sc = SidecarOf(c);
    for (size_t i = 0; i < sc.size(); ++i) {
        sc[i] ^= 0x01;
        EXPECT_EQ(SignatureStatus::kBadSignature, s.Verify(c.role, model, &sc, c.sig, 78)) << "sidecar byte " << i;
        sc[i] ^= 0x01;
    }
}

TEST(ModelSigner, AnySingleChangedByteOfTheSignatureItselfIsRefusedWithTheRightReason) {
    const Subject s;
    const SigCase& c = Case("bone_with_sidecar");
    const std::vector<uint8_t> model = ModelOf(c);
    const std::vector<uint8_t> sc = SidecarOf(c);
    for (size_t i = 0; i < xpe::ai::kSignatureFileBytes; ++i) {
        std::vector<uint8_t> sig = SigOf(c);
        sig[i] ^= 0x01;
        const SignatureStatus got = s.Verify(c.role, model, &sc, sig.data(), sig.size());
        SignatureStatus want = SignatureStatus::kBadSignature;                 // r and s (bytes 14..77)
        if (i < 4) want = SignatureStatus::kMalformedSignature;                // the magic
        else if (i < 6) want = SignatureStatus::kUnsupportedVersion;           // version, algorithm
        else if (i < 14) want = SignatureStatus::kUnknownKey;                  // the key id names a key nobody trusts
        EXPECT_EQ(want, got) << "signature byte " << i;
    }
}

TEST(ModelSigner, ASignatureFileOfAnyOtherSizeIsMalformedAndAMissingOneIsSaidToBeMissing) {
    const Subject s;
    const SigCase& c = Case("bone_with_sidecar");
    const std::vector<uint8_t> model = ModelOf(c);
    const std::vector<uint8_t> sc = SidecarOf(c);
    std::vector<uint8_t> longer = SigOf(c);
    longer.push_back(0);
    for (const size_t n : {size_t{0}, size_t{1}, size_t{4}, size_t{77}}) {
        EXPECT_EQ(SignatureStatus::kMalformedSignature, s.Verify(c.role, model, &sc, c.sig, n)) << "size " << n;
    }
    EXPECT_EQ(SignatureStatus::kMalformedSignature, s.Verify(c.role, model, &sc, longer.data(), longer.size())) << "size 79";
    EXPECT_EQ(SignatureStatus::kNoSignatureFile, s.Verify(c.role, model, &sc, nullptr, 0));
}

// ===== what the signature binds besides the bytes ==============================================================

TEST(ModelSigner, TheRoleIsPartOfWhatIsSigned) {
    const Subject s;
    const SigCase& c = Case("bone_with_sidecar");
    const std::vector<uint8_t> model = ModelOf(c);
    const std::vector<uint8_t> sc = SidecarOf(c);
    // A model signed for one job must not pass as another's: this is the "role swap" attack.
    for (const char* role : {"bodypart", "Bone_suppress", "bone_suppres", "bone_suppress ", "bone_suppress\n", "", "x"}) {
        EXPECT_EQ(SignatureStatus::kBadSignature, s.Verify(role, model, &sc, c.sig, 78)) << "role '" << role << "'";
    }
}

TEST(ModelSigner, NoSidecarAndAnEmptySidecarAndASidecarAreThreeDifferentThings) {
    const Subject s;
    const std::vector<uint8_t> model = ModelOf(Case("part_no_sidecar"));
    const std::vector<uint8_t> empty;
    const std::vector<uint8_t> some = {'x'};
    const SigCase& none = Case("part_no_sidecar");
    const SigCase& blank = Case("part_empty_sidecar");

    EXPECT_EQ(SignatureStatus::kOk, s.Verify("bodypart", model, nullptr, none.sig, 78));
    EXPECT_EQ(SignatureStatus::kBadSignature, s.Verify("bodypart", model, &empty, none.sig, 78)) << "signed with none, presented with an empty one";
    EXPECT_EQ(SignatureStatus::kBadSignature, s.Verify("bodypart", model, &some, none.sig, 78)) << "a sidecar added";

    EXPECT_EQ(SignatureStatus::kOk, s.Verify("bodypart", model, &empty, blank.sig, 78));
    EXPECT_EQ(SignatureStatus::kBadSignature, s.Verify("bodypart", model, nullptr, blank.sig, 78)) << "signed with an empty one, presented with none";
    EXPECT_EQ(SignatureStatus::kBadSignature, s.Verify("bodypart", model, &some, blank.sig, 78));

    const SigCase& withSc = Case("bone_with_sidecar");
    EXPECT_EQ(SignatureStatus::kBadSignature, s.Verify(withSc.role, ModelOf(withSc), nullptr, withSc.sig, 78)) << "a sidecar dropped";
}

TEST(ModelSigner, TheBoundaryBetweenModelAndSidecarIsPartOfWhatIsSigned) {
    // Moving a byte across the boundary leaves model || sidecar unchanged; only the length prefixes tell the
    // two apart. Without them these two would verify as each other.
    const Subject s;
    const SigCase& abc = Case("boundary_ab_c");
    const SigCase& a_bc = Case("boundary_a_bc");
    const std::vector<uint8_t> ab = {'A', 'B'}, c = {'C'}, a = {'A'}, bc = {'B', 'C'};
    EXPECT_EQ(SignatureStatus::kOk, s.Verify("bone_suppress", ab, &c, abc.sig, 78));
    EXPECT_EQ(SignatureStatus::kOk, s.Verify("bone_suppress", a, &bc, a_bc.sig, 78));
    EXPECT_EQ(SignatureStatus::kBadSignature, s.Verify("bone_suppress", ab, &c, a_bc.sig, 78));
    EXPECT_EQ(SignatureStatus::kBadSignature, s.Verify("bone_suppress", a, &bc, abc.sig, 78));
}

TEST(ModelSigner, AModelThatIsShorterOrLongerByOneByteIsRefused) {
    const Subject s;
    const SigCase& c = Case("bone_with_sidecar");
    const std::vector<uint8_t> sc = SidecarOf(c);
    std::vector<uint8_t> model = ModelOf(c);
    model.push_back(0);
    EXPECT_EQ(SignatureStatus::kBadSignature, s.Verify(c.role, model, &sc, c.sig, 78)) << "a byte appended";
    model.pop_back();
    model.pop_back();
    EXPECT_EQ(SignatureStatus::kBadSignature, s.Verify(c.role, model, &sc, c.sig, 78)) << "a byte missing";
    EXPECT_EQ(SignatureStatus::kBadSignature, s.Verify(c.role, std::vector<uint8_t>(), &sc, c.sig, 78)) << "an empty model";
}

// ===== size ===================================================================================================

TEST(ModelSigner, ALargeModelAndOneAcrossTheHashingChunkBoundaryVerify) {
    // The verifier hashes in 64 MiB chunks (CNG takes a 32-bit length). 3 MiB is one chunk; the second vector is a
    // few bytes past one chunk and so exercises the loop's second turn.
    const Subject s;
    EXPECT_EQ(SignatureStatus::kOk, s.VerifyCase(Case("large_model")));
    const SigCase& chunk = Case("chunk_model");
    std::vector<uint8_t> model = ModelOf(chunk);
    EXPECT_EQ(SignatureStatus::kOk, s.Verify(chunk.role, model, nullptr, chunk.sig, 78));
    model.back() ^= 0x01;
    EXPECT_EQ(SignatureStatus::kBadSignature, s.Verify(chunk.role, model, nullptr, chunk.sig, 78)) << "the last byte, in the second chunk";
    model.back() ^= 0x01;
    model[64u * 1024u * 1024u] ^= 0x01;
    EXPECT_EQ(SignatureStatus::kBadSignature, s.Verify(chunk.role, model, nullptr, chunk.sig, 78)) << "the first byte of the second chunk";
}

TEST(ModelSigner, AFileAboveTheSafetyCapIsRefusedBeforeAnythingIsRead) {
    // The pointers are one byte long but the sizes claim more than the cap: a verifier that hashed instead of refusing
    // would read far past them. (The cap is an implementation safety value, not a requirement: D8.)
    const Subject s;
    const SigCase& c = Case("bone_with_sidecar");
    const Bytes huge{&kOne, xpe::ai::kMaxSignedFileBytes + 1};
    const Bytes small{&kOne, 1};
    EXPECT_EQ(SignatureStatus::kTooLarge,
              xpe::ai::VerifyModelSignature(s.keys.data(), s.keys.size(), c.role, huge, nullptr, c.sig, 78));
    EXPECT_EQ(SignatureStatus::kTooLarge,
              xpe::ai::VerifyModelSignature(s.keys.data(), s.keys.size(), c.role, small, &huge, c.sig, 78));
}

// ===== failing closed =========================================================================================

TEST(ModelSigner, AMalformedKeyInTheTrustedListIsARefusalNotASkip) {
    // A key whose bytes are not a point on the curve cannot be imported. The verifier must report that it could not
    // run, not move on and not answer "ok". (The id is set to the signature's so the key is the one selected.)
    const SigCase& c = Case("bone_with_sidecar");
    TrustedKey bad{};
    std::memcpy(bad.id, c.sig + 6, 8);
    const std::vector<uint8_t> model = ModelOf(c);
    const std::vector<uint8_t> sc = SidecarOf(c);
    const Bytes m{model.data(), model.size()};
    const Bytes sb{sc.data(), sc.size()};
    EXPECT_EQ(SignatureStatus::kVerifierError, xpe::ai::VerifyModelSignature(&bad, 1, c.role, m, &sb, c.sig, 78));
}

TEST(ModelSigner, EveryStatusHasItsOwnNonEmptyReason) {
    std::set<std::string> texts;
    for (const SignatureStatus st : {SignatureStatus::kOk, SignatureStatus::kNoSignatureFile, SignatureStatus::kMalformedSignature,
                                     SignatureStatus::kUnsupportedVersion, SignatureStatus::kUnknownKey, SignatureStatus::kBadSignature,
                                     SignatureStatus::kTooLarge, SignatureStatus::kVerifierError}) {
        const char* t = xpe::ai::SignatureStatusText(st);
        ASSERT_NE(nullptr, t);
        EXPECT_GT(std::strlen(t), 0u);
        texts.insert(t);
    }
    EXPECT_EQ(8u, texts.size()) << "an alert that names the reason class needs the classes to read differently";
}
