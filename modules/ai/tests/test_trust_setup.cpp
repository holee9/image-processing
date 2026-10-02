/**
 * @file test_trust_setup.cpp
 * @brief Makes the TEST signing key trusted for the whole test process (QA-B-195 M3, design decision D4).
 *
 * Model loading refuses any model whose signature is not by a trusted key, and the production trust list is empty
 * until the production key exists (#243). A test build therefore has to be told which key to trust: the
 * environment variable XPE_AI_TEST_TRUSTED_KEYS, read by a build compiled with XPE_AI_TEST_HOOKS (see
 * ai_model_signer.h). It is set here, before main() and before any test, from the public half of the committed TEST
 * key (the same bytes the signatures in tests/data were made for). The real worker process inherits it.
 *
 * A test that needs a DIFFERENT trust set changes the variable itself and restores it.
 */

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <string>

namespace {

#include "signer_vectors.inc"   // kKey1Xy: the public half of tests/data/signing/test_key_1.pem

struct TrustTheTestKey {
    TrustTheTestKey() {
        std::string hex;
        char two[3];
        for (const uint8_t b : kKey1Xy) {
            std::snprintf(two, sizeof(two), "%02x", static_cast<unsigned>(b));
            hex += two;
        }
        SetEnvironmentVariableA("XPE_AI_TEST_TRUSTED_KEYS", hex.c_str());
    }
};

const TrustTheTestKey g_trust;

}  // namespace
