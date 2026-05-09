/**
 * @file test_req_p1a_066.cpp
 * @brief REQ-P1A-066: 4 error path unit tests (IEC 62304 Class B)
 *
 *  T1 – OOM guard:       xpe_gain_correct rejects UINT32_MAX×UINT32_MAX dims before
 *                        any allocation (n > SIZE_MAX/sizeof(float) guard).
 *  T2 – IO failure:      xpe_calib_load_offset on a directory returns IO_FAILED.
 *  T3 – SHA-256 corrupt: xcal file with zeroed sha256 returns CONFIG_INVALID.
 *  T4 – Ghost multi-hdl: 4 threads each with own handle run concurrently, no crash.
 *
 * SPEC: SPEC-XPE-P1A v1.2.0  IEC 62304 Class B
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"

#include <vector>
#include <cstring>
#include <cstdint>
#include <limits>
#include <fstream>
#include <filesystem>
#include <thread>
#include <atomic>

namespace fs = std::filesystem;

namespace {

/* ============================================================================
 * T1: OOM guard — SIZE_MAX overflow protection in xpe_gain_correct
 * REQ-P1A-066 D1
 * ============================================================================ */

// UINT32_MAX × UINT32_MAX yields n ≈ 1.844e19 which exceeds SIZE_MAX/sizeof(float).
// The guard at "n > SIZE_MAX / sizeof(float)" must fire and return INVALID_INPUT
// before any heap allocation is attempted.
TEST(P1A066, T1_GainCorrect_HugeDimension_RejectsBeforeAllocation) {
    uint16_t src_stub = 0;
    float    dst_stub = 0.0f;

    XpeImageBuffer input{};
    input.data          = &src_stub;
    input.format        = XPE_PIXEL_UINT16;
    input.width         = std::numeric_limits<uint32_t>::max();
    input.height        = std::numeric_limits<uint32_t>::max();
    input.bitsAllocated = 16;
    input.bitsStored    = 16;
    input.dataSize      = 1; // won't reach dataSize check

    XpeImageBuffer output{};
    output.data         = &dst_stub;
    output.width        = input.width;
    output.height       = input.height;
    output.dataSize     = std::numeric_limits<size_t>::max();

    XpeImageMetadata meta{};

    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_gain_correct(&input, &output, &meta));
}

/* ============================================================================
 * T2: IO failure — directory path treated as file fails to open
 * REQ-P1A-066 D2
 * ============================================================================ */

class P1A066IoTest : public ::testing::Test {
protected:
    fs::path tmpPath;

    void SetUp() override {
        tmpPath = fs::temp_directory_path() / "xpe_p1a066_dir.xcal";
        fs::remove_all(tmpPath); // ensure clean start
    }

    void TearDown() override {
        fs::remove_all(tmpPath);
    }
};

// On Windows, std::ifstream cannot open a directory without FILE_FLAG_BACKUP_SEMANTICS,
// so is_open() returns false → read_xcal_file returns XPE_ERR_IO_FAILED.
TEST_F(P1A066IoTest, T2_LoadOffset_DirectoryPath_ReturnsIoFailed) {
    fs::create_directory(tmpPath);
    EXPECT_EQ(XPE_ERR_IO_FAILED,
              xpe_calib_load_offset(tmpPath.string().c_str()));
}

/* ============================================================================
 * T3: SHA-256 mismatch — corrupted xcal body returns CONFIG_INVALID
 * REQ-P1A-066 D3
 * ============================================================================ */

static void write_xcal_wrong_sha256(const fs::path& path) {
    // Build a syntactically valid 4×4 OFFSET header, but leave sha256 zeroed.
    // compute_sha256_two_parts() will compute the real hash of the payload and
    // find it differs from the zeroed sha256 field → XPE_ERR_CONFIG_INVALID.
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version         = XCAL_VERSION;
    hdr.type            = static_cast<uint32_t>(XCAL_TYPE_OFFSET);
    hdr.pixel_format    = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
    hdr.width           = 4;
    hdr.height          = 4;
    hdr.created_epoch_ms = 0;
    hdr.expiry_epoch_ms  = 0; // never expires
    std::memset(hdr.session_id, 0, sizeof(hdr.session_id));
    hdr.config_json_len = 0;
    hdr.payload_len     = static_cast<uint64_t>(4u * 4u * sizeof(float)); // 64
    std::memset(hdr.sha256, 0, sizeof(hdr.sha256)); // intentionally wrong

    std::vector<float> payload(4u * 4u, 1.0f);

    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(&hdr),
            static_cast<std::streamsize>(sizeof(hdr)));
    f.write(reinterpret_cast<const char*>(payload.data()),
            static_cast<std::streamsize>(payload.size() * sizeof(float)));
}

TEST(P1A066, T3_LoadOffset_WrongSha256_ReturnsConfigInvalid) {
    fs::path tmp = fs::temp_directory_path() / "xpe_p1a066_badsha.xcal";
    write_xcal_wrong_sha256(tmp);

    XpeErrorCode rc = xpe_calib_load_offset(tmp.string().c_str());
    fs::remove(tmp);

    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, rc);
}

/* ============================================================================
 * T4: Ghost multi-handle race — 4 independent handles on 4 threads
 * REQ-P1A-066 D4
 *
 * The ghost handle is NOT thread-safe for sharing (no internal mutex).
 * This test verifies that multiple handles owned by different threads can
 * operate concurrently without corrupting shared global state or crashing.
 * ============================================================================ */

TEST(P1A066, T4_GhostCorrect_MultiHandle_ConcurrentNoError) {
    static constexpr uint32_t W = 16;
    static constexpr uint32_t H = 16;
    static constexpr int      THREADS    = 4;
    static constexpr int      ITERATIONS = 50;

    std::atomic<int> errors{0};

    std::vector<std::thread> threads;
    threads.reserve(THREADS);

    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([t, &errors]() {
            void* handle = nullptr;
            if (xpe_ghost_create(W, H, nullptr, &handle) != XPE_OK) {
                ++errors;
                return;
            }

            std::vector<float> px(static_cast<size_t>(W) * H, 500.0f);
            XpeImageBuffer img{};
            img.data          = px.data();
            img.width         = W;
            img.height        = H;
            img.bitsAllocated = 32;
            img.bitsStored    = 32;
            img.format        = XPE_PIXEL_FLOAT32;
            img.dataSize      = px.size() * sizeof(float);

            XpeImageMetadata meta{};
            for (int i = 0; i < ITERATIONS; ++i) {
                meta.acquisitionTime = static_cast<uint64_t>(t * ITERATIONS + i);
                if (xpe_ghost_correct(handle, &img, &meta) != XPE_OK) {
                    ++errors;
                }
            }

            xpe_ghost_destroy(handle);
        });
    }

    for (auto& th : threads) th.join();

    EXPECT_EQ(0, errors.load())
        << "All " << THREADS << " independent ghost handles should correct without error";
}

} // namespace
