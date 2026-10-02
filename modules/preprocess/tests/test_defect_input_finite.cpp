/**
 * @file test_defect_input_finite.cpp
 * @brief xpe_defect_correct refuses a non-finite frame at its entrance (QA-A-214b, #233).
 *
 * The defect fill takes a median (or a mean) of neighbours; QA-A-214 measured that a NaN among them makes the result
 * depend on the order the values were collected in (110 of 400 frames changed when mirrored) and that the call still
 * answered XPE_OK with a NaN in the output. Callers of the public function -- the GUI preview service among them --
 * can hand it such a frame; the pipeline cannot (its input is finite by construction, test_defect_fill_equivalence.cpp).
 *
 * The rule: a non-finite frame -> XPE_ERR_INVALID_INPUT, one XPE_ALERT_ERROR naming the count and the first pixel, and
 * NOTHING written: the output buffer keeps its bytes, and called in place the input keeps its bytes.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"
#include "preprocess_state_fixture.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t W = 12, H = 10;
constexpr size_t N = static_cast<size_t>(W) * H;

std::string findAlert(const std::string& prefix) {
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < n; ++i) {
        char buf[1024];
        int32_t sev = 0;
        if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) == XPE_OK && std::string(buf).rfind(prefix, 0) == 0) return buf;
    }
    return {};
}

class DefectInputFiniteTest : public XpePreprocessStateFixture {
protected:
    void SetUp() override {
        XpePreprocessStateFixture::SetUp();
        dir_ = (fs::temp_directory_path() / ("xpe_dif_" + std::to_string(counter_++))).string();
        fs::create_directories(dir_);
        xpe_clear_alerts();
        std::vector<uint8_t> m(N, 0);
        m[3 * W + 3] = 1;                                  // a lone defect
        m[6 * W + 6] = m[6 * W + 7] = m[7 * W + 6] = 1;    // a cluster
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION; hdr.type = static_cast<uint32_t>(XCAL_TYPE_DEFECT); hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT8_MASK);
        hdr.width = W; hdr.height = H; hdr.payload_len = m.size();
        const std::string path = (fs::path(dir_) / "m.xcal").string();
        ASSERT_EQ(XPE_OK, write_xcal_file(path.c_str(), hdr, nullptr, 0, m.data(), m.size()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(path.c_str()));
    }
    void TearDown() override {
        xpe_clear_alerts();
        std::error_code ec;
        fs::remove_all(dir_, ec);
        XpePreprocessStateFixture::TearDown();
    }

    static XpeImageBuffer buf(std::vector<float>& v) {
        XpeImageBuffer b{};
        b.data = v.data(); b.width = W; b.height = H; b.bitsAllocated = 32; b.bitsStored = 32; b.format = XPE_PIXEL_FLOAT32; b.dataSize = v.size() * 4;
        return b;
    }
    /** a frame of 100 with a defect value of 5000 at the masked pixels */
    static std::vector<float> frame() {
        std::vector<float> f(N, 100.0f);
        f[3 * W + 3] = 5000.0f; f[6 * W + 6] = f[6 * W + 7] = f[7 * W + 6] = 5000.0f;
        return f;
    }

    std::string dir_;
    static inline int counter_ = 0;
};

}  // namespace

TEST_F(DefectInputFiniteTest, ANonFiniteFrameIsRefusedWithInvalidInputAndTheOutputBufferIsLeftAlone) {
    const float nan = std::numeric_limits<float>::quiet_NaN(), inf = std::numeric_limits<float>::infinity();
    struct Case { const char* name; size_t at; float v; };
    const Case cases[] = {
        {"NaN in a valid pixel", 2 * W + 9, nan}, {"+inf in a valid pixel", 2 * W + 9, inf}, {"-inf in a valid pixel", 2 * W + 9, -inf},
        {"NaN in the lone defect itself", 3 * W + 3, nan}, {"+inf in a cluster pixel", 6 * W + 6, inf},
        {"NaN in pixel 0", 0, nan}, {"-inf in the last pixel", N - 1, -inf},
    };
    for (const Case& c : cases) {
        SCOPED_TRACE(c.name);
        std::vector<float> in = frame();
        in[c.at] = c.v;
        const std::vector<float> inBefore = in;
        std::vector<float> out(N);
        for (size_t i = 0; i < N; ++i) out[i] = -12345.0f + static_cast<float>(i);   // a pattern the call must not disturb
        const std::vector<float> outBefore = out;
        XpeImageBuffer ib = buf(in), ob = buf(out);
        XpeImageMetadata meta{};
        xpe_clear_alerts();
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_defect_correct(&ib, &ob, &meta));
        EXPECT_EQ(0, std::memcmp(outBefore.data(), out.data(), N * sizeof(float))) << "the output buffer must keep its bytes";
        EXPECT_EQ(0, std::memcmp(inBefore.data(), in.data(), N * sizeof(float))) << "the input is not touched either";
        EXPECT_FALSE(findAlert("XPE_WARN_DEFECT_INPUT_NOT_FINITE:").empty());
    }
}

TEST_F(DefectInputFiniteTest, CalledInPlaceARefusedFrameLeavesTheInputBytesAsTheyWere) {
    std::vector<float> f = frame();
    f[2 * W + 9] = std::numeric_limits<float>::quiet_NaN();
    const std::vector<float> before = f;
    XpeImageBuffer b = buf(f);
    XpeImageMetadata meta{};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_defect_correct(&b, &b, &meta));
    // NaN != NaN, so the bytes are compared, not the values
    EXPECT_EQ(0, std::memcmp(before.data(), f.data(), N * sizeof(float))) << "in place: nothing was corrected, nothing was written";
}

TEST_F(DefectInputFiniteTest, TheAlertNamesTheCountAndTheFirstPixel) {
    std::vector<float> f = frame();
    f[4 * W + 5] = std::numeric_limits<float>::infinity();
    f[8 * W + 1] = std::numeric_limits<float>::quiet_NaN();
    f[9 * W + 11] = -std::numeric_limits<float>::infinity();
    std::vector<float> out(N, 0.0f);
    XpeImageBuffer ib = buf(f), ob = buf(out);
    XpeImageMetadata meta{};
    xpe_clear_alerts();
    ASSERT_EQ(XPE_ERR_INVALID_INPUT, xpe_defect_correct(&ib, &ob, &meta));
    EXPECT_EQ("XPE_WARN_DEFECT_INPUT_NOT_FINITE: 3 pixel(s) of the input frame are NaN or infinite (first: index 53, x=5, y=4); "
              "the frame was not corrected and the output was not written",
              findAlert("XPE_WARN_DEFECT_INPUT_NOT_FINITE:"));
}

TEST_F(DefectInputFiniteTest, AFiniteFrameIsCorrectedExactlyAsBeforeAndRaisesNothing) {
    std::vector<float> in = frame(), out(N, -1.0f);
    // the largest and smallest finite floats are finite: they pass
    in[0] = std::numeric_limits<float>::max(); in[1] = std::numeric_limits<float>::lowest(); in[2] = std::numeric_limits<float>::denorm_min();
    XpeImageBuffer ib = buf(in), ob = buf(out);
    XpeImageMetadata meta{};
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&ib, &ob, &meta));
    EXPECT_TRUE(findAlert("XPE_WARN_DEFECT_INPUT_NOT_FINITE:").empty());
    EXPECT_FLOAT_EQ(100.0f, out[3 * W + 3]) << "the lone defect: mean of its four 100 neighbours";
    EXPECT_FLOAT_EQ(100.0f, out[6 * W + 6]) << "the cluster: median of its valid 3x3 neighbours";
    EXPECT_EQ(in[0], out[0]);   // untouched pixels are copied bit for bit
    EXPECT_EQ(in[1], out[1]);
    EXPECT_EQ(in[2], out[2]);
}

TEST_F(DefectInputFiniteTest, ThePrecedenceOfTheOlderErrorsIsKept) {
    // a non-finite frame with a bad argument still answers the argument error first
    std::vector<float> f = frame();
    f[5] = std::numeric_limits<float>::quiet_NaN();
    std::vector<float> out(N, 0.0f);
    XpeImageBuffer ib = buf(f), ob = buf(out);
    XpeImageMetadata meta{};
    ob.width = W - 1;
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, xpe_defect_correct(&ib, &ob, &meta)) << "dimensions are checked before the pixels";
    ob = buf(out);
    ib.format = XPE_PIXEL_UINT16;
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, xpe_defect_correct(&ib, &ob, &meta)) << "the format before the pixels";
}
