/**
 * @file test_xpe_sha256.cpp
 * @brief SHA-256 wrapper tests with FIPS 180-4 known-answer test vectors (T-003)
 *
 * SPEC-XPE-P1A SUP-01 -- REQ-P1A-014 (SHA-256 integrity)
 *
 * FIPS 180-4 test vectors used:
 *   1. Empty string:
 *      e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
 *   2. "abc":
 *      ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
 *   3. "abcdbcde...nopq" (448-bit / 56-byte message):
 *      248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1
 */

#include <gtest/gtest.h>
#include <cstring>
#include <string>
#include "xpe_sha256.hpp"

namespace {

// Convert hex string to 32-byte array for comparison
std::array<uint8_t, 32> HexToDigest(const char* hex) {
    std::array<uint8_t, 32> out = {};
    for (int i = 0; i < 32; ++i) {
        unsigned int byte = 0;
        std::sscanf(hex + 2 * i, "%02x", &byte);
        out[i] = static_cast<uint8_t>(byte);
    }
    return out;
}

// Hex representation of digest for EXPECT message
std::string DigestToHex(const std::array<uint8_t, 32>& d) {
    char buf[65] = {};
    for (int i = 0; i < 32; ++i) {
        std::snprintf(buf + 2 * i, 3, "%02x", d[i]);
    }
    return std::string(buf);
}

} // anonymous namespace

// =============================================================================
// FIPS 180-4 KAT 1: empty string
// =============================================================================
TEST(Sha256Test, EmptyString_KnownAnswer) {
    const char* expected_hex =
        "e3b0c44298fc1c149afbf4c8996fb924"
        "27ae41e4649b934ca495991b7852b855";

    auto digest = compute_sha256(nullptr, 0);
    auto expected = HexToDigest(expected_hex);

    EXPECT_EQ(digest, expected)
        << "Got: " << DigestToHex(digest);
}

// =============================================================================
// FIPS 180-4 KAT 2: "abc"
// =============================================================================
TEST(Sha256Test, ABC_KnownAnswer) {
    const char* expected_hex =
        "ba7816bf8f01cfea414140de5dae2223"
        "b00361a396177a9cb410ff61f20015ad";

    const uint8_t msg[] = {'a', 'b', 'c'};
    auto digest = compute_sha256(msg, 3);
    auto expected = HexToDigest(expected_hex);

    EXPECT_EQ(digest, expected)
        << "Got: " << DigestToHex(digest);
}

// =============================================================================
// FIPS 180-4 KAT 3: "abcdbcde...nopq" (56-byte message)
// =============================================================================
TEST(Sha256Test, ABCDBCDE56_KnownAnswer) {
    const char* expected_hex =
        "248d6a61d20638b8e5c026930c3e6039"
        "a33ce45964ff2167f6ecedd419db06c1";

    // "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
    const char* msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    auto digest = compute_sha256(reinterpret_cast<const uint8_t*>(msg), strlen(msg));
    auto expected = HexToDigest(expected_hex);

    EXPECT_EQ(digest, expected)
        << "Got: " << DigestToHex(digest);
}

// =============================================================================
// Two-part SHA-256: SHA-256("abc") == SHA-256_two_parts("a","bc")
// =============================================================================
TEST(Sha256Test, TwoParts_SplitABC_MatchesCombined) {
    const uint8_t full[] = {'a', 'b', 'c'};
    const uint8_t part1[] = {'a'};
    const uint8_t part2[] = {'b', 'c'};

    auto combined = compute_sha256(full, 3);
    auto two_part = compute_sha256_two_parts(part1, 1, part2, 2);

    EXPECT_EQ(combined, two_part);
}

// =============================================================================
// Two-part SHA-256: empty second part
// =============================================================================
TEST(Sha256Test, TwoParts_EmptySecondPart) {
    const uint8_t msg[] = {'a', 'b', 'c'};
    auto single = compute_sha256(msg, 3);
    auto two_part = compute_sha256_two_parts(msg, 3, nullptr, 0);
    EXPECT_EQ(single, two_part);
}

// =============================================================================
// Two-part SHA-256: empty first part
// =============================================================================
TEST(Sha256Test, TwoParts_EmptyFirstPart) {
    const uint8_t msg[] = {'a', 'b', 'c'};
    auto single = compute_sha256(msg, 3);
    auto two_part = compute_sha256_two_parts(nullptr, 0, msg, 3);
    EXPECT_EQ(single, two_part);
}

// =============================================================================
// Two-part SHA-256: both parts empty == empty string digest
// =============================================================================
TEST(Sha256Test, TwoParts_BothEmpty_EqualsEmptyDigest) {
    auto empty_digest = compute_sha256(nullptr, 0);
    auto two_part = compute_sha256_two_parts(nullptr, 0, nullptr, 0);
    EXPECT_EQ(empty_digest, two_part);
}
