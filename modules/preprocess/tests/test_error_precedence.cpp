/**
 * @file test_error_precedence.cpp
 * @brief Error-code precedence rule 1, probed on xpe_preprocess (#119, QA-A-14)
 *
 * Companion to modules/common/tests/test_error_precedence.cpp. Same contract:
 * api-spec.md "Error code precedence" rule 1 -- a NULL **required** pointer
 * yields XPE_ERR_INVALID_INPUT before any other check, regardless of whether the
 * module is initialized.
 *
 * Rule 2 is deliberately NOT asserted. The spec names this module as the
 * example that validates content before the initialization check, and calls
 * that conforming; asserting an order here would invent a contract.
 *
 * Excluded by the spec: xpe_preprocess_init(config) takes an optional pointer,
 * and the void-returning / pointerless entry points have nothing to probe.
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"

#include <cstdio>
#include <vector>

namespace {

struct ProbeRow {
    const char*  entryPoint;
    const char*  nullArgument;
    XpeErrorCode uninitialized;
    XpeErrorCode initialized;
};

std::vector<ProbeRow> g_rows;

template <typename Fn>
void probe(const char* entryPoint, const char* nullArgument, Fn call) {
    xpe_preprocess_shutdown();            // guarantee the uninitialized state
    const XpeErrorCode uninit = call();

    EXPECT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    const XpeErrorCode inited = call();
    xpe_preprocess_shutdown();

    g_rows.push_back(ProbeRow{entryPoint, nullArgument, uninit, inited});

    EXPECT_EQ(XPE_ERR_INVALID_INPUT, uninit)
        << entryPoint << " with NULL " << nullArgument << ", module uninitialized";
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, inited)
        << entryPoint << " with NULL " << nullArgument << ", module initialized";
}

// A structurally valid 4x4 UINT16 buffer, so the only fault in each probe is
// the one NULL pointer under test.
struct Frame {
    std::vector<uint16_t> pixels{std::vector<uint16_t>(16, 1000)};
    XpeImageBuffer        buf{};
    Frame() {
        buf.data = pixels.data();
        buf.width = 4; buf.height = 4;
        buf.bitsAllocated = 16; buf.bitsStored = 16;
        buf.format = XPE_PIXEL_UINT16;
        buf.dataSize = pixels.size() * sizeof(uint16_t);
    }
};

class PreprocessErrorPrecedenceTest : public ::testing::Test {
protected:
    void TearDown() override { xpe_preprocess_shutdown(); }
};

TEST_F(PreprocessErrorPrecedenceTest, NullRequiredPointerWinsOverInitializationState) {
    g_rows.clear();

    probe("xpe_calib_load_offset", "filepath",
          [] { return xpe_calib_load_offset(nullptr); });
    probe("xpe_calib_load_gain", "filepath",
          [] { return xpe_calib_load_gain(nullptr); });
    probe("xpe_calib_load_defect_map", "filepath",
          [] { return xpe_calib_load_defect_map(nullptr); });

    probe("xpe_offset_correct", "input", [] {
        Frame out; XpeImageMetadata meta{};
        return xpe_offset_correct(nullptr, &out.buf, &meta);
    });
    probe("xpe_offset_correct", "output", [] {
        Frame in; XpeImageMetadata meta{};
        return xpe_offset_correct(&in.buf, nullptr, &meta);
    });
    probe("xpe_gain_correct", "input", [] {
        Frame out; XpeImageMetadata meta{};
        return xpe_gain_correct(nullptr, &out.buf, &meta);
    });
    probe("xpe_gain_correct", "output", [] {
        Frame in; XpeImageMetadata meta{};
        return xpe_gain_correct(&in.buf, nullptr, &meta);
    });
    probe("xpe_defect_correct", "input", [] {
        Frame out; XpeImageMetadata meta{};
        return xpe_defect_correct(nullptr, &out.buf, &meta);
    });
    probe("xpe_defect_correct", "output", [] {
        Frame in; XpeImageMetadata meta{};
        return xpe_defect_correct(&in.buf, nullptr, &meta);
    });

    probe("xpe_calib_generate_offset", "dark_frames", [] {
        return xpe_calib_generate_offset(nullptr, 1, 100.0f, 25.0f, "a14_probe.xcal", nullptr);
    });
    probe("xpe_calib_generate_offset", "output_path", [] {
        Frame f;
        return xpe_calib_generate_offset(&f.buf, 1, 100.0f, 25.0f, nullptr, nullptr);
    });

    probe("xpe_calib_check_expiry", "filepath", [] {
        bool expired = false; int32_t days = 0;
        return xpe_calib_check_expiry(nullptr, &expired, &days);
    });
    probe("xpe_calib_check_expiry", "is_expired", [] {
        int32_t days = 0;
        return xpe_calib_check_expiry("a14_probe.xcal", nullptr, &days);
    });
    probe("xpe_calib_check_expiry", "remaining_days", [] {
        bool expired = false;
        return xpe_calib_check_expiry("a14_probe.xcal", &expired, nullptr);
    });

    probe("xpe_calib_save", "filepath",
          [] { return xpe_calib_save(nullptr, "offset", 0); });
    probe("xpe_calib_save", "calib_type",
          [] { return xpe_calib_save("a14_probe.xcal", nullptr, 0); });

    probe("xpe_validate_readout_artifact", "image", [] {
        XpeImageMetadata meta{}; bool a = false, b = false;
        return xpe_validate_readout_artifact(nullptr, &meta, &a, &b);
    });
    probe("xpe_validate_readout_artifact", "has_dropped_columns", [] {
        Frame f; XpeImageMetadata meta{}; bool b = false;
        return xpe_validate_readout_artifact(&f.buf, &meta, nullptr, &b);
    });

    probe("xpe_preprocess_pipeline", "img", [] {
        XpeImageMetadata meta{};
        return xpe_preprocess_pipeline(nullptr, &meta, nullptr, nullptr, nullptr);
    });
    probe("xpe_preprocess_pipeline", "meta", [] {
        Frame f;
        return xpe_preprocess_pipeline(&f.buf, nullptr, nullptr, nullptr, nullptr);
    });

    probe("xpe_calib_load_offset_cached", "filePath", [] {
        XpeImageBuffer out{};
        return xpe_calib_load_offset_cached(nullptr, &out);
    });
    probe("xpe_calib_load_offset_cached", "offsetMapOut",
          [] { return xpe_calib_load_offset_cached("a14_probe.xcal", nullptr); });
    probe("xpe_calib_load_gain_cached", "gainMapOut",
          [] { return xpe_calib_load_gain_cached("a14_probe.xcal", nullptr); });
    probe("xpe_calib_load_defect_cached", "defectMapOut",
          [] { return xpe_calib_load_defect_cached("a14_probe.xcal", nullptr); });

    probe("xpe_preprocess_get_param_range", "param_name", [] {
        float mn = 0, mx = 0;
        return xpe_preprocess_get_param_range(nullptr, &mn, &mx);
    });
    probe("xpe_preprocess_get_param_range", "min_value", [] {
        float mx = 0;
        return xpe_preprocess_get_param_range("offsetScale", nullptr, &mx);
    });

    probe("xpe_ghost_correct", "handle", [] {
        Frame f; XpeImageMetadata meta{};
        return xpe_ghost_correct(nullptr, &f.buf, &meta);
    });
    probe("xpe_ghost_reset", "handle",
          [] { return xpe_ghost_reset(nullptr); });

    std::printf("\n%-32s %-22s %-14s %-14s\n",
                "entry point", "NULL argument", "uninitialized", "initialized");
    for (const auto& r : g_rows) {
        std::printf("%-32s %-22s %-14d %-14d\n",
                    r.entryPoint, r.nullArgument,
                    static_cast<int>(r.uninitialized),
                    static_cast<int>(r.initialized));
    }
    std::printf("(XPE_ERR_INVALID_INPUT = %d, XPE_ERR_NOT_INITIALIZED = %d)\n\n",
                static_cast<int>(XPE_ERR_INVALID_INPUT),
                static_cast<int>(XPE_ERR_NOT_INITIALIZED));

    EXPECT_EQ(28u, g_rows.size()) << "every planned row must have been probed";
}

} // namespace
