/**
 * @file test_sha256_backend_parity.cpp
 * @brief Sha256Stream (CNG or PicoSHA2) vs PicoSHA2 directly (QA-A-106, #179)
 *
 * QA-A-106 replaced the hashing behind Sha256Stream with Windows CNG where the
 * provider opens. A calibration file written before that change must still
 * verify, so the two implementations must produce the same bytes.
 *
 * PicoSHA2 is the reference here because it is the implementation the shipped
 * files were hashed with; it is called directly, not through Sha256Stream.
 * FIPS 180-4 known answers are in test_xpe_sha256.cpp -- these cases are about
 * the two implementations agreeing, including at SHA-256 block boundaries
 * (55/56 bytes = the padding split, 64/65 = one block and one over).
 */

#include <gtest/gtest.h>

#include "xpe_sha256.hpp"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::array<uint8_t, 32> PicoDigest(const uint8_t* data, size_t len) {
    picosha2::hash256_one_by_one h;
    h.init();
    if (data != nullptr && len > 0) h.process(data, data + len);
    h.finish();
    std::array<uint8_t, 32> out = {};
    h.get_hash_bytes(out.begin(), out.end());
    return out;
}

std::vector<uint8_t> Pattern(size_t len) {
    std::vector<uint8_t> v(len);
    for (size_t i = 0; i < len; ++i) v[i] = static_cast<uint8_t>((i * 31u + 7u) & 0xFF);
    return v;
}

}  // namespace

TEST(Sha256BackendTest, BackendNameIsReported) {
    const std::string name = xpe_sha256_backend_name();
    GTEST_LOG_(INFO) << "SHA-256 backend: " << name;
    EXPECT_TRUE(name == "cng" || name == "picosha2") << name;
}

TEST(Sha256BackendTest, MatchesPicoSha2AtBlockBoundaries) {
    for (size_t len : {size_t{0}, size_t{1}, size_t{55}, size_t{56}, size_t{63},
                       size_t{64}, size_t{65}, size_t{127}, size_t{128}, size_t{1000},
                       size_t{1u << 20}, size_t{(1u << 20) + 3}}) {
        const std::vector<uint8_t> data = Pattern(len);
        const auto expected = PicoDigest(data.empty() ? nullptr : data.data(), len);

        Sha256Stream s;
        s.update(data.empty() ? nullptr : data.data(), len);
        EXPECT_EQ(0, std::memcmp(expected.data(), s.digest().data(), 32))
            << "len=" << len;

        const auto one_shot = compute_sha256(data.empty() ? nullptr : data.data(), len);
        EXPECT_EQ(0, std::memcmp(expected.data(), one_shot.data(), 32)) << "len=" << len;
    }
}

// Chunking must not change the digest: the reader feeds the payload in 1 MiB
// pieces (QA-A-105), the writer in one piece.
TEST(Sha256BackendTest, ChunkedUpdatesMatchOneShot) {
    const std::vector<uint8_t> data = Pattern(3 * (1u << 20) + 517);
    const auto expected = PicoDigest(data.data(), data.size());
    for (size_t chunk : {size_t{1}, size_t{64}, size_t{4096}, size_t{1u << 20}}) {
        Sha256Stream s;
        for (size_t off = 0; off < data.size(); off += chunk) {
            s.update(data.data() + off, std::min(chunk, data.size() - off));
        }
        EXPECT_EQ(0, std::memcmp(expected.data(), s.digest().data(), 32))
            << "chunk=" << chunk;
    }
}

// compute_sha256_two_parts is what the XCal digest covers: config || payload.
TEST(Sha256BackendTest, TwoPartsMatchesTheConcatenation) {
    const std::vector<uint8_t> cfg = Pattern(37);
    const std::vector<uint8_t> pay = Pattern(4096 + 11);
    std::vector<uint8_t> joined = cfg;
    joined.insert(joined.end(), pay.begin(), pay.end());

    const auto expected = PicoDigest(joined.data(), joined.size());
    const auto got = compute_sha256_two_parts(cfg.data(), cfg.size(), pay.data(), pay.size());
    EXPECT_EQ(0, std::memcmp(expected.data(), got.data(), 32));
}

// A file whose header digest was computed by PicoSHA2 alone must still load.
TEST(Sha256BackendTest, FileHashedByPicoSha2StillVerifies) {
    const fs::path dir = fs::temp_directory_path() / "xpe_sha_backend";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const std::string path = (dir / "offset.xcal").string();

    constexpr uint32_t W = 16, H = 16;
    const std::vector<float> data(static_cast<size_t>(W) * H, 3.5f);
    const auto* bytes = reinterpret_cast<const uint8_t*>(data.data());
    const size_t len = data.size() * sizeof(float);

    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION;
    hdr.type = static_cast<uint32_t>(XCAL_TYPE_OFFSET);
    hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
    hdr.width = W; hdr.height = H;
    hdr.payload_len = len;
    hdr.config_json_len = 0;
    const auto pico = PicoDigest(bytes, len);      // written the pre-QA-A-106 way
    std::memcpy(hdr.sha256, pico.data(), 32);

    {   // header + payload, no writer involved
        std::FILE* f = nullptr;
        ASSERT_EQ(0, fopen_s(&f, path.c_str(), "wb"));
        ASSERT_NE(nullptr, f);
        ASSERT_EQ(size_t{1}, std::fwrite(&hdr, sizeof(hdr), 1, f));
        ASSERT_EQ(len, std::fwrite(bytes, 1, len, f));
        std::fclose(f);
    }

    (void)xpe_preprocess_init(nullptr);
    EXPECT_EQ(XPE_OK, xpe_calib_load_offset(path.c_str()))
        << "a file hashed by PicoSHA2 must verify under the current backend";
    xpe_preprocess_shutdown();
    fs::remove_all(dir);
}
