/**
 * @file test_calib_cache_concurrency.cpp
 * @brief Cached loader vs. concurrent cache_clear (QA-A-31, #127)
 *
 * QA-A-22 left a window open: `publish_and_view()` called the cache's `put()`
 * and `get()` as two separate lock scopes, so a `xpe_calib_cache_clear()` (or an
 * eviction) landing between them removed the entry before it could be read
 * back. The loader then returned XPE_ERR_PROCESSING_FAILED for a call that had
 * actually succeeded — no leak, no double free, just a spurious failure.
 *
 * There is no test seam between the two halves, so this reproduces the race the
 * way it happens in practice: one thread repeatedly takes the miss path while
 * another clears the cache under it, and the test COUNTS spurious failures
 * rather than asserting "zero failures" on a schedule it does not control.
 *
 * Both threads are joined before the assertions run — no background load
 * outlives the case.
 *
 * Measured by temporarily reverting publish_and_view to put() + get():
 *   before  734 spurious XPE_ERR_PROCESSING_FAILED / 50000 iterations
 *   after     0 / 50000
 * Evidence: .moai/reports/lane-pre/QA-A-31/a31-race-before.log and
 * a31-race-after2.log.
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t W = 8, H = 8;
// Sensitivity matters more than speed here. At 2000 iterations with one
// clearing thread the race did NOT reproduce even against the unfixed code
// (0/2000 measured) -- a guard that cannot see the bug is not a guard. At 50000
// iterations with four clearing threads the unfixed code produced 734 spurious
// failures, and the fixed code produces 0. Those are the numbers this file is
// calibrated to; the whole case runs in under 200 ms.
constexpr int      kIterations = 50000;

class CalibCacheConcurrencyTest : public ::testing::Test {
protected:
    fs::path tmpDir, mapFile;

    void SetUp() override {
        tmpDir = fs::temp_directory_path() / "xpe_cache_concurrency";
        fs::remove_all(tmpDir);
        fs::create_directories(tmpDir);
        mapFile = tmpDir / "offset.xcal";

        const std::vector<float> data(static_cast<size_t>(W) * H, 100.0f);
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(XCAL_TYPE_OFFSET);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = W; hdr.height = H;
        hdr.payload_len = data.size() * sizeof(float);
        ASSERT_EQ(XPE_OK,
                  write_xcal_file(mapFile.string().c_str(), hdr, nullptr, 0,
                                  reinterpret_cast<const uint8_t*>(data.data()),
                                  hdr.payload_len));

        xpe_calib_cache_clear();
        xpe_calib_cache_set_max_size(4);
    }

    void TearDown() override {
        xpe_calib_cache_clear();
        fs::remove_all(tmpDir);
    }
};

// A loader running against a concurrent clear must never report a failure it
// did not have: the entry it just inserted may be gone, but the call itself
// succeeded and the caller is owed a valid view.
TEST_F(CalibCacheConcurrencyTest, LoadUnderConcurrentClearNeverReportsSpuriousFailure) {
    const std::string path = mapFile.string();

    std::atomic<bool> stopClearing{false};
    std::atomic<int>  spuriousFailures{0};
    std::atomic<int>  otherFailures{0};
    std::atomic<int>  completed{0};

    // Four clearing threads, because one was not enough to hit the window (see
    // the note on kIterations). They run only while the loader is working and
    // are joined below -- no load outlives this case.
    std::vector<std::thread> clearers;
    for (int t = 0; t < 4; ++t)
        clearers.emplace_back([&stopClearing] {
            while (!stopClearing.load(std::memory_order_relaxed)) {
                xpe_calib_cache_clear();
            }
        });

    for (int i = 0; i < kIterations; ++i) {
        XpeImageBuffer view{};
        const XpeErrorCode rc = xpe_calib_load_offset_cached(path.c_str(), &view);

        if (rc == XPE_ERR_PROCESSING_FAILED) {
            spuriousFailures.fetch_add(1, std::memory_order_relaxed);
        } else if (rc != XPE_OK) {
            otherFailures.fetch_add(1, std::memory_order_relaxed);
        } else {
            completed.fetch_add(1, std::memory_order_relaxed);
        }
    }

    stopClearing.store(true, std::memory_order_relaxed);
    for (auto& t : clearers) t.join();

    // The count IS the measurement -- reported whichever way it comes out.
    std::printf("[QA-A-31] iterations=%d ok=%d spurious_PROCESSING_FAILED=%d other_failures=%d\n",
                kIterations, completed.load(), spuriousFailures.load(),
                otherFailures.load());

    EXPECT_EQ(0, spuriousFailures.load())
        << "put and get must be one lock scope; a concurrent clear must not "
           "turn a successful load into XPE_ERR_PROCESSING_FAILED";
    EXPECT_EQ(0, otherFailures.load())
        << "no other error is expected on this path";
}

// The same shape with eviction instead of clearing: capacity 1 means every new
// key evicts the previous one, which is the other way the entry could vanish
// between insert and read-back.
TEST_F(CalibCacheConcurrencyTest, LoadUnderConcurrentEvictionNeverReportsSpuriousFailure) {
    const std::string pathA = mapFile.string();
    const std::string pathB = (tmpDir / "offset_b.xcal").string();
    {
        const std::vector<float> data(static_cast<size_t>(W) * H, 200.0f);
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(XCAL_TYPE_OFFSET);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = W; hdr.height = H;
        hdr.payload_len = data.size() * sizeof(float);
        ASSERT_EQ(XPE_OK,
                  write_xcal_file(pathB.c_str(), hdr, nullptr, 0,
                                  reinterpret_cast<const uint8_t*>(data.data()),
                                  hdr.payload_len));
    }

    xpe_calib_cache_set_max_size(1);       // every insert evicts the other key

    std::atomic<bool> stop{false};
    std::atomic<int>  spurious{0};

    std::thread other([&stop, &pathB] {
        while (!stop.load(std::memory_order_relaxed)) {
            XpeImageBuffer v{};
            xpe_calib_load_offset_cached(pathB.c_str(), &v);
        }
    });

    for (int i = 0; i < kIterations / 2; ++i) {
        XpeImageBuffer view{};
        if (xpe_calib_load_offset_cached(pathA.c_str(), &view) == XPE_ERR_PROCESSING_FAILED) {
            spurious.fetch_add(1, std::memory_order_relaxed);
        }
    }

    stop.store(true, std::memory_order_relaxed);
    other.join();

    std::printf("[QA-A-31] eviction race: iterations=%d spurious_PROCESSING_FAILED=%d\n",
                kIterations / 2, spurious.load());

    EXPECT_EQ(0, spurious.load())
        << "an eviction between insert and read-back must not surface as a failure";

    xpe_calib_cache_set_max_size(4);
}

} // namespace
