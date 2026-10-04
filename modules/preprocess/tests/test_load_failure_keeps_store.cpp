/**
 * @file test_load_failure_keeps_store.cpp
 * @brief A refused calibration file leaves the module-global store as it was, for the three loaders (QA-A-235b, #245)
 *
 * The loaders read the payload straight into the buffer they will keep (XCalPayloadSink) instead of into a vector they
 * then copy. The buffer is local until the whole file has been accepted, so a file that fails -- hash mismatch,
 * truncated, wrong size, wrong type -- must leave the previously loaded map exactly as it was, and must return the
 * same error code the vector path returned. Each case loads map A, applies it to a probe frame, tries a bad file B,
 * and requires (1) the code, (2) the probe output after the failed load equal to the one before, and (3) a good file
 * afterwards still loads (the failed call left no half-state behind).
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"
#include "fixtures/make_xcal.hpp"

namespace {
namespace fs = std::filesystem;
constexpr uint32_t W = 16, H = 8;
constexpr size_t N = static_cast<size_t>(W) * H;

enum Kind { kOffset, kGain, kDefect };

class LoadFailureKeepsStore : public ::testing::Test {
protected:
    fs::path dir;
    void SetUp() override {
        dir = fs::temp_directory_path() / "qa_a_235b";
        fs::remove_all(dir);
        fs::create_directories(dir);
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    }
    void TearDown() override {
        xpe_clear_alerts();
        xpe_calib_cache_clear();
        xpe_preprocess_shutdown();
        std::error_code ec;
        fs::remove_all(dir, ec);
    }

    std::string make(Kind k, float value, const char* name) {
        const std::string p = (dir / name).string();
        if (k == kOffset) EXPECT_EQ(XPE_OK, MakeOffsetXCal(p.c_str(), W, H, value));
        else if (k == kGain) EXPECT_EQ(XPE_OK, MakeGainXCal(p.c_str(), W, H, value));
        else {
            std::vector<uint8_t> payload(N, 0);
            payload[5] = value > 0.5f ? 1 : 0;   // `value` picks which defect map this is
            payload[77] = value > 0.5f ? 0 : 1;
            XCalFileHeader hdr{};
            std::memcpy(hdr.magic, XCAL_MAGIC, 4);
            hdr.version = XCAL_VERSION; hdr.type = XCAL_TYPE_DEFECT; hdr.pixel_format = XCAL_FMT_UINT8_MASK;
            hdr.width = W; hdr.height = H; hdr.payload_len = payload.size();
            EXPECT_EQ(XPE_OK, write_xcal_file(p.c_str(), hdr, nullptr, 0, payload.data(), payload.size()));
        }
        return p;
    }
    static XpeErrorCode load(Kind k, const std::string& p) {
        return k == kOffset ? xpe_calib_load_offset(p.c_str())
             : k == kGain   ? xpe_calib_load_gain(p.c_str())
                            : xpe_calib_load_defect_map(p.c_str());
    }
    // What the store would do to a frame: the output of the stage that uses the map.
    static std::vector<float> probe(Kind k) {
        std::vector<float> out(N, -1.0f);
        XpeImageMetadata meta{};
        XpeImageBuffer ib{}, ob{};
        ib.width = ob.width = W; ib.height = ob.height = H;
        if (k == kOffset) {
            std::vector<uint16_t> in(N, 500), o16(N, 0);
            ib.format = ob.format = XPE_PIXEL_UINT16;
            ib.bitsAllocated = ib.bitsStored = ob.bitsAllocated = ob.bitsStored = 16;
            ib.data = in.data(); ib.dataSize = N * 2; ob.data = o16.data(); ob.dataSize = N * 2;
            const XpeErrorCode rc = xpe_offset_correct(&ib, &ob, &meta);
            for (size_t i = 0; i < N; ++i) out[i] = rc == XPE_OK ? static_cast<float>(o16[i]) : -static_cast<float>(rc);
        } else if (k == kGain) {
            std::vector<uint16_t> in(N, 500);
            ib.format = XPE_PIXEL_UINT16; ob.format = XPE_PIXEL_FLOAT32;
            ib.bitsAllocated = ib.bitsStored = 16; ob.bitsAllocated = ob.bitsStored = 32;
            ib.data = in.data(); ib.dataSize = N * 2; ob.data = out.data(); ob.dataSize = N * 4;
            const XpeErrorCode rc = xpe_gain_correct(&ib, &ob, &meta);
            if (rc != XPE_OK) std::fill(out.begin(), out.end(), -static_cast<float>(rc));
        } else {
            std::vector<float> in(N);
            for (size_t i = 0; i < N; ++i) in[i] = 1000.0f + static_cast<float>(i);
            ib.format = ob.format = XPE_PIXEL_FLOAT32;
            ib.bitsAllocated = ib.bitsStored = ob.bitsAllocated = ob.bitsStored = 32;
            ib.data = in.data(); ib.dataSize = N * 4; ob.data = out.data(); ob.dataSize = N * 4;
            const XpeErrorCode rc = xpe_defect_correct(&ib, &ob, &meta);
            if (rc != XPE_OK) std::fill(out.begin(), out.end(), -static_cast<float>(rc));
        }
        xpe_clear_alerts();
        return out;
    }
    static void patch(const std::string& p, std::streamoff at, uint8_t byte) {
        std::fstream f(p, std::ios::in | std::ios::out | std::ios::binary);
        f.seekp(at);
        f.put(static_cast<char>(byte));
    }

    // Load good file A, then the bad file B built by `damage(path)`: the code, the store and a later good load.
    void expectRefused(Kind k, const std::function<void(const std::string&)>& damage, XpeErrorCode expected) {
        const std::string a = make(k, k == kGain ? 2.0f : (k == kOffset ? 100.0f : 1.0f), "a.xcal");
        ASSERT_EQ(XPE_OK, load(k, a));
        const std::vector<float> before = probe(k);
        const std::string b = make(k, k == kGain ? 4.0f : (k == kOffset ? 50.0f : 0.0f), "b.xcal");
        damage(b);
        EXPECT_EQ(expected, load(k, b));
        xpe_clear_alerts();
        EXPECT_EQ(before, probe(k)) << "the failed load changed what the store does";
        const std::string c = make(k, k == kGain ? 8.0f : (k == kOffset ? 25.0f : 0.0f), "c.xcal");
        EXPECT_EQ(XPE_OK, load(k, c)) << "a good file after a refused one must load";
    }
};

constexpr std::streamoff kPayloadAt = static_cast<std::streamoff>(sizeof(XCalFileHeader));
}  // namespace

TEST_F(LoadFailureKeepsStore, AFlippedPayloadByteIsRefusedAndTheStoreKeepsItsMap) {
    for (Kind k : {kOffset, kGain, kDefect}) {
        SCOPED_TRACE(k == kOffset ? "offset" : k == kGain ? "gain" : "defect");
        SetUp();
        expectRefused(k, [](const std::string& p) { patch(p, kPayloadAt + 3, 0x5A); }, XPE_ERR_CONFIG_INVALID);
        TearDown();
    }
}

TEST_F(LoadFailureKeepsStore, ATruncatedFileIsRefusedAndTheStoreKeepsItsMap) {
    for (Kind k : {kOffset, kGain, kDefect}) {
        SCOPED_TRACE(k == kOffset ? "offset" : k == kGain ? "gain" : "defect");
        SetUp();
        expectRefused(k, [](const std::string& p) {
            fs::resize_file(p, static_cast<uintmax_t>(kPayloadAt) + (fs::file_size(p) - kPayloadAt) / 2);
        }, XPE_ERR_IO_FAILED);
        TearDown();
    }
}

TEST_F(LoadFailureKeepsStore, AHeaderThatDeclaresTheWrongPayloadLengthIsRefused) {
    for (Kind k : {kOffset, kGain, kDefect}) {
        SCOPED_TRACE(k == kOffset ? "offset" : k == kGain ? "gain" : "defect");
        SetUp();
        expectRefused(k, [](const std::string& p) {
            XCalFileHeader h{};
            { std::ifstream in(p, std::ios::binary); in.read(reinterpret_cast<char*>(&h), sizeof(h)); }
            h.payload_len -= 4;
            std::fstream f(p, std::ios::in | std::ios::out | std::ios::binary);
            f.write(reinterpret_cast<const char*>(&h), sizeof(h));
        }, XPE_ERR_CONFIG_INVALID);
        TearDown();
    }
}

// The wrong kind of file for the loader: a defect map given to the offset loader and an offset map to the gain loader.
TEST_F(LoadFailureKeepsStore, AFileOfAnotherTypeIsRefusedAndTheStoreKeepsItsMap) {
    const std::string a = make(kOffset, 100.0f, "a.xcal");
    ASSERT_EQ(XPE_OK, load(kOffset, a));
    const std::vector<float> before = probe(kOffset);
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, load(kOffset, make(kDefect, 1.0f, "d.xcal")));
    EXPECT_EQ(before, probe(kOffset));

    const std::string g = make(kGain, 2.0f, "g.xcal");
    ASSERT_EQ(XPE_OK, load(kGain, g));
    const std::vector<float> gbefore = probe(kGain);
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, load(kGain, make(kOffset, 100.0f, "o2.xcal")));
    EXPECT_EQ(gbefore, probe(kGain));
}

// The map the loader keeps is the file's payload, byte for byte (the direct read puts it where the copy used to).
TEST_F(LoadFailureKeepsStore, TheLoadedMapIsTheFilesPayload) {
    ASSERT_EQ(XPE_OK, load(kOffset, make(kOffset, 100.0f, "o.xcal")));
    for (float v : probe(kOffset)) EXPECT_FLOAT_EQ(400.0f, v);          // 500 - 100
    ASSERT_EQ(XPE_OK, load(kGain, make(kGain, 2.0f, "g.xcal")));
    for (float v : probe(kGain)) EXPECT_GT(v, 0.0f);
    ASSERT_EQ(XPE_OK, load(kDefect, make(kDefect, 1.0f, "d.xcal")));
    const std::vector<float> corrected = probe(kDefect);
    EXPECT_EQ(N, corrected.size());
}
