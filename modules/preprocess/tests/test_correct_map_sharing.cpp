/**
 * @file test_correct_map_sharing.cpp
 * @brief xpe_offset_correct / xpe_defect_correct read the loaded map without copying it (QA-A-202, #233).
 *
 * They used to copy the whole map (37.7 MB at 3072x3072) on every frame, under the calibration lock. They now
 * take shared ownership of the map under the lock and read it outside the lock. A concurrent reload replaces
 * the map in the store; the frame in flight keeps the map it started with, so every output frame is wholly the
 * result of one map -- never a mixture, never a read of freed memory.
 *
 * Maps are large (1024x1024, 4 MB / 1 MB) on purpose: a block that size is returned to the operating system when
 * it is freed, so a read through a pointer that no longer owns it faults instead of quietly reading stale bytes.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr uint32_t W = 1024, H = 1024;
constexpr size_t N = static_cast<size_t>(W) * H;

XpeImageBuffer buf(void* d, XpePixelFormat f, uint32_t bits) {
    XpeImageBuffer b{};
    b.data = d; b.width = W; b.height = H; b.bitsAllocated = bits; b.bitsStored = bits; b.format = f;
    b.dataSize = static_cast<uint32_t>(N * (bits / 8));
    return b;
}

void writeFile(const char* path, uint32_t type, uint32_t fmt, const void* data, size_t bytes) {
    std::remove(path);
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION; hdr.type = type; hdr.pixel_format = fmt;
    hdr.width = W; hdr.height = H; hdr.payload_len = bytes;
    ASSERT_EQ(XPE_OK, write_xcal_file(path, hdr, reinterpret_cast<const uint8_t*>("{}"), 2,
                                      static_cast<const uint8_t*>(data), bytes));
}

constexpr const char* kFiles[] = {"cms_off_a.xcal", "cms_off_b.xcal", "cms_def_a.xcal", "cms_def_b.xcal"};

class CorrectMapSharing : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr)); }
    void TearDown() override {
        for (const char* f : kFiles) std::remove(f);
        xpe_clear_alerts();
        xpe_preprocess_shutdown();
    }
};

/** Runs `corrector` on one thread while another reloads the map between two files, for about `ms` ms. */
template <typename Corrector, typename ReloadA, typename ReloadB>
long hammer(int ms, Corrector corrector, ReloadA reloadA, ReloadB reloadB, long* frames) {
    std::atomic<bool> stop{false};
    std::atomic<long> bad{0};
    std::thread reloader([&] {
        while (!stop.load()) {
            reloadA();
            reloadB();
        }
    });
    long n = 0;
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < until) {
        if (!corrector()) bad.fetch_add(1);
        ++n;
    }
    stop.store(true);
    reloader.join();
    *frames = n;
    return bad.load();
}

}  // namespace

TEST_F(CorrectMapSharing, OffsetCorrectionDuringAReloadIsWhollyOneMapOrTheOther) {
    std::vector<float> a(N, 100.0f), b(N, 300.0f);
    writeFile("cms_off_a.xcal", XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32, a.data(), a.size() * 4);
    writeFile("cms_off_b.xcal", XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32, b.data(), b.size() * 4);
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset("cms_off_a.xcal"));

    std::vector<uint16_t> in(N, 1000), out(N, 0);
    long frames = 0;
    const long bad = hammer(
        1500,
        [&] {
            XpeImageBuffer i = buf(in.data(), XPE_PIXEL_UINT16, 16), o = buf(out.data(), XPE_PIXEL_UINT16, 16);
            XpeImageMetadata meta{};
            if (xpe_offset_correct(&i, &o, &meta) != XPE_OK) return false;
            const uint16_t first = out[0];                    // 900 (map A) or 700 (map B)
            if (first != 900 && first != 700) return false;
            return std::all_of(out.begin(), out.end(), [first](uint16_t v) { return v == first; });
        },
        [] { xpe_calib_load_offset("cms_off_a.xcal"); },
        [] { xpe_calib_load_offset("cms_off_b.xcal"); },
        &frames);
    EXPECT_GT(frames, 5) << "the corrector must have run during the reloads";
    EXPECT_EQ(0, bad) << bad << " of " << frames << " frames were not wholly one map's result";
}

TEST_F(CorrectMapSharing, DefectCorrectionDuringAReloadIsWhollyOneMapOrTheOther) {
    std::vector<uint8_t> a(N, 0), b(N, 0);
    a[1 * W + 1] = 1;                                  // map A flags (1,1); map B flags nothing
    writeFile("cms_def_a.xcal", XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, a.data(), a.size());
    writeFile("cms_def_b.xcal", XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, b.data(), b.size());
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map("cms_def_a.xcal"));

    std::vector<float> in(N, 1000.0f), out(N, 0.0f);
    in[1 * W + 1] = 5000.0f;                           // a spike at the pixel map A flags
    long frames = 0;
    const long bad = hammer(
        1500,
        [&] {
            XpeImageBuffer i = buf(in.data(), XPE_PIXEL_FLOAT32, 32), o = buf(out.data(), XPE_PIXEL_FLOAT32, 32);
            XpeImageMetadata meta{};
            if (xpe_defect_correct(&i, &o, &meta) != XPE_OK) return false;
            const float spike = out[1 * W + 1];            // repaired (~1000, map A) or left alone (5000, map B)
            const bool repaired = std::fabs(spike - 1000.0f) < 1.0f;
            if (!repaired && spike != 5000.0f) return false;
            for (size_t p = 0; p < N; ++p) {
                if (p == 1 * W + 1) continue;
                if (out[p] != 1000.0f) return false;
            }
            return true;
        },
        [] { xpe_calib_load_defect_map("cms_def_a.xcal"); },
        [] { xpe_calib_load_defect_map("cms_def_b.xcal"); },
        &frames);
    EXPECT_GT(frames, 5) << "the corrector must have run during the reloads";
    EXPECT_EQ(0, bad) << bad << " of " << frames << " frames were not wholly one map's result";
}
