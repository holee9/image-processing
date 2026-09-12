// #156 (QA-B-59): does each input reach the output? -- dicom.
//
// The question the B-58 sweep asked of processing parameters has a different
// shape here: dicom's write entry points take no tuning parameters at all
// (QA-B-54 measured the three-argument form; the compressionRatio and
// configJsonOrNull the documentation described were never in the header). What
// they do take is an IMAGE and a METADATA struct, and the same failure mode
// applies to those -- a field that is read, carried, and then not written.
//
// So: change one input, hold the rest, write the file, read it back, and require
// the difference to survive the round trip. A metadata field that never reaches
// the file is the #154 / #155 / #156 shape wearing DICOM clothes, and it would
// be invisible to every test that only checks the return code.
//
// The floor check (QA-B-58 §3) applies here too and is cheap to state: the round
// trip must reproduce an UNCHANGED input first. If it cannot, a difference that
// fails to survive says nothing about the field.

#include <gtest/gtest.h>

#include "xpe/dicom/dicom_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_memory.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t kW = 64, kH = 64;

struct RoundTrip {
    XpeErrorCode      rc = XPE_OK;
    uint32_t          width = 0;
    uint32_t          height = 0;
    std::vector<uint16_t> pixels;
    XpeImageMetadata  meta{};
};

// Write an image + metadata, read both back. The file is the channel; anything
// that does not survive it did not reach the output.
RoundTrip WriteThenRead(const fs::path& path, uint16_t fill,
                        const XpeImageMetadata& meta) {
    XpeImageBuffer img{};
    if (xpe_alloc_image(kW, kH, XPE_PIXEL_UINT16, &img) != XPE_OK) {
        RoundTrip bad; bad.rc = XPE_ERR_OUT_OF_MEMORY; return bad;
    }
    auto* px = static_cast<uint16_t*>(img.data);
    for (uint32_t i = 0; i < kW * kH; ++i) {
        px[i] = static_cast<uint16_t>(fill + (i % 13));
    }

    RoundTrip r;
    r.rc = xpe_dicom_write(path.string().c_str(), &img, &meta);
    xpe_free_image(&img);
    if (r.rc != XPE_OK) return r;

    XpeDicomHandle* handle = nullptr;
    r.rc = xpe_dicom_open(path.string().c_str(), &handle);
    if (r.rc != XPE_OK) return r;

    XpeImageBuffer back{};
    r.rc = xpe_dicom_read_image(handle, &back);
    if (r.rc == XPE_OK) {
        r.width  = back.width;
        r.height = back.height;
        const auto* bpx = static_cast<const uint16_t*>(back.data);
        r.pixels.assign(bpx, bpx + static_cast<size_t>(back.width) * back.height);
        xpe_free_image(&back);
    }
    xpe_dicom_get_metadata(handle, &r.meta);
    xpe_dicom_close(handle);
    return r;
}

fs::path TempDir() {
    const auto d = fs::temp_directory_path() / "xpe_dicom_param_dependency";
    fs::create_directories(d);
    return d;
}

}  // namespace

// The floor: an unchanged input must survive the round trip. Everything below
// reads a difference out of this channel, so the channel is checked first.
TEST(DicomParameterDependency, RoundTripReproducesTheInput) {
    XpeImageMetadata meta{};
    std::snprintf(meta.bodyPart, sizeof(meta.bodyPart), "%s", "CHEST");
    meta.kVp           = 80.0f;
    meta.mAs           = 2.5f;
    meta.pixelPitch_mm = 0.148f;

    const RoundTrip a = WriteThenRead(TempDir() / "floor_a.dcm", 1000, meta);
    const RoundTrip b = WriteThenRead(TempDir() / "floor_b.dcm", 1000, meta);

    ASSERT_EQ(XPE_OK, a.rc);
    ASSERT_EQ(XPE_OK, b.rc);
    ASSERT_EQ(kW, a.width);
    ASSERT_EQ(kH, a.height);
    EXPECT_EQ(a.pixels, b.pixels)
        << "two identical writes read back differently; the channel is not stable "
           "enough to read a parameter's effect out of";
}

TEST(DicomParameterDependency, PixelDataReachesTheFile) {
    XpeImageMetadata meta{};
    std::snprintf(meta.bodyPart, sizeof(meta.bodyPart), "%s", "CHEST");

    const RoundTrip low  = WriteThenRead(TempDir() / "px_low.dcm", 1000, meta);
    const RoundTrip high = WriteThenRead(TempDir() / "px_high.dcm", 4000, meta);

    ASSERT_EQ(XPE_OK, low.rc);
    ASSERT_EQ(XPE_OK, high.rc);
    GTEST_LOG_(INFO) << "pixel fill 1000 -> first sample " << low.pixels.front()
                     << ", fill 4000 -> " << high.pixels.front();
    EXPECT_NE(low.pixels, high.pixels) << "the pixel data does not reach the file";
}

TEST(DicomParameterDependency, ImageDimensionsReachTheFile) {
    XpeImageMetadata meta{};
    XpeImageBuffer img{};
    ASSERT_EQ(XPE_OK, xpe_alloc_image(32, 96, XPE_PIXEL_UINT16, &img));
    std::memset(img.data, 0x11, img.dataSize);
    const auto path = TempDir() / "dims.dcm";
    ASSERT_EQ(XPE_OK, xpe_dicom_write(path.string().c_str(), &img, &meta));
    xpe_free_image(&img);

    XpeDicomHandle* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(path.string().c_str(), &handle));
    XpeImageBuffer back{};
    ASSERT_EQ(XPE_OK, xpe_dicom_read_image(handle, &back));
    GTEST_LOG_(INFO) << "wrote 32x96, read back " << back.width << "x" << back.height;
    EXPECT_EQ(32u, back.width)  << "the width does not reach the file";
    EXPECT_EQ(96u, back.height) << "the height does not reach the file";
    xpe_free_image(&back);
    xpe_dicom_close(handle);
}

// Metadata fields, one at a time. Each is written, read back, and compared --
// a field that is accepted by the writer but never stored is exactly the shape
// this sweep exists to find.
TEST(DicomParameterDependency, MetadataFieldsReachTheFile) {
    auto writeWith = [](const char* name, const XpeImageMetadata& meta) {
        return WriteThenRead(TempDir() / (std::string("meta_") + name + ".dcm"), 1500, meta);
    };

    XpeImageMetadata base{};
    std::snprintf(base.bodyPart, sizeof(base.bodyPart), "%s", "CHEST");
    base.kVp           = 80.0f;
    base.mAs           = 2.5f;
    base.SID_mm        = 1800.0f;
    base.pixelPitch_mm = 0.148f;

    XpeImageMetadata other = base;
    std::snprintf(other.bodyPart, sizeof(other.bodyPart), "%s", "ABDOMEN");
    other.kVp           = 120.0f;
    other.mAs           = 10.0f;
    other.SID_mm        = 1000.0f;
    other.pixelPitch_mm = 0.200f;

    const RoundTrip a = writeWith("base", base);
    const RoundTrip b = writeWith("other", other);
    ASSERT_EQ(XPE_OK, a.rc);
    ASSERT_EQ(XPE_OK, b.rc);

    GTEST_LOG_(INFO) << "bodyPart '" << a.meta.bodyPart << "' vs '" << b.meta.bodyPart << "'"
                     << " | kVp " << a.meta.kVp << " vs " << b.meta.kVp
                     << " | mAs " << a.meta.mAs << " vs " << b.meta.mAs
                     << " | SID " << a.meta.SID_mm << " vs " << b.meta.SID_mm
                     << " | pitch " << a.meta.pixelPitch_mm << " vs " << b.meta.pixelPitch_mm;

    EXPECT_STRNE(a.meta.bodyPart, b.meta.bodyPart) << "bodyPart does not reach the file";
    EXPECT_NE(a.meta.kVp, b.meta.kVp)              << "kVp does not reach the file";
    EXPECT_NE(a.meta.mAs, b.meta.mAs)              << "mAs does not reach the file";
    EXPECT_NE(a.meta.SID_mm, b.meta.SID_mm)        << "SID_mm does not reach the file";
    EXPECT_NE(a.meta.pixelPitch_mm, b.meta.pixelPitch_mm)
        << "pixelPitch_mm does not reach the file";
}
