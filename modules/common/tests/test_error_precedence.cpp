/**
 * @file test_error_precedence.cpp
 * @brief Error-code precedence rule 1, probed on xpe_common (#119, QA-A-14)
 *
 * api-spec.md "Error code precedence (normative, narrowed 2026-09-09 per #119)"
 * rule 1: a NULL **required** pointer yields XPE_ERR_INVALID_INPUT before any
 * other check, so an uninitialized module never dereferences caller memory.
 * Rule 2 (NOT_INITIALIZED versus content validation) is explicitly
 * implementation-defined and is NOT asserted here -- `common` happens to check
 * initialization first and `preprocess` happens to validate content first, and
 * the spec calls both conforming.
 *
 * Each row below therefore calls one entry point with one NULL required pointer
 * TWICE -- once on an uninitialized module, once on an initialized one -- and
 * expects XPE_ERR_INVALID_INPUT from both. The two states are what makes this a
 * precedence probe rather than a plain null-argument test: rule 1 says the
 * answer must not depend on initialization state.
 *
 * Excluded by the spec, not by omission:
 *   - xpe_init(configJsonOrNull), xpe_log_set_file(filePath): NULL is a
 *     documented "use defaults" / "reset to stderr" value, so these are
 *     optional pointers and rule 1 does not apply
 *   - xpe_version, xpe_error_string: no XpeErrorCode return to observe
 *   - xpe_alert_push: void return
 *   - xpe_shutdown, xpe_clear_alerts, xpe_log_flush,
 *     xpe_get_pending_alert_count, xpe_log_set_level: no pointer arguments
 */

#include <gtest/gtest.h>
#include "xpe/common/xpe_common_api.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

struct ProbeRow {
    const char*  entryPoint;
    const char*  nullArgument;
    XpeErrorCode uninitialized;
    XpeErrorCode initialized;
};

std::vector<ProbeRow> g_rows;

// Runs one probe in both module states and records the pair.
template <typename Fn>
void probe(const char* entryPoint, const char* nullArgument, Fn call) {
    xpe_shutdown();                       // guarantee the uninitialized state
    const XpeErrorCode uninit = call();

    EXPECT_EQ(XPE_OK, xpe_init(nullptr));
    const XpeErrorCode inited = call();
    xpe_shutdown();

    g_rows.push_back(ProbeRow{entryPoint, nullArgument, uninit, inited});

    EXPECT_EQ(XPE_ERR_INVALID_INPUT, uninit)
        << entryPoint << " with NULL " << nullArgument << ", module uninitialized";
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, inited)
        << entryPoint << " with NULL " << nullArgument << ", module initialized";
}

class CommonErrorPrecedenceTest : public ::testing::Test {
protected:
    void TearDown() override { xpe_shutdown(); }
};

TEST_F(CommonErrorPrecedenceTest, NullRequiredPointerWinsOverInitializationState) {
    g_rows.clear();

    probe("xpe_configure", "jsonConfig",
          [] { return xpe_configure(nullptr); });

    probe("xpe_get_param_range", "bodyPart", [] {
        float mn = 0, mx = 0, df = 0;
        return xpe_get_param_range(nullptr, "windowWidth", &mn, &mx, &df);
    });
    probe("xpe_get_param_range", "paramName", [] {
        float mn = 0, mx = 0, df = 0;
        return xpe_get_param_range("CHEST", nullptr, &mn, &mx, &df);
    });
    probe("xpe_get_param_range", "minValue", [] {
        float mx = 0, df = 0;
        return xpe_get_param_range("CHEST", "windowWidth", nullptr, &mx, &df);
    });

    probe("xpe_get_pending_alert", "message", [] {
        int32_t severity = 0;
        return xpe_get_pending_alert(0, nullptr, 64, &severity);
    });
    probe("xpe_get_pending_alert", "severity", [] {
        char buf[64] = {0};
        return xpe_get_pending_alert(0, buf, sizeof(buf), nullptr);
    });

    probe("xpe_alloc_image", "out", [] {
        return xpe_alloc_image(4, 4, XPE_PIXEL_UINT16, nullptr);
    });
    probe("xpe_free_image", "buffer", [] {
        return xpe_free_image(nullptr);
    });
    probe("xpe_copy_image", "source", [] {
        XpeImageBuffer dst{};
        return xpe_copy_image(nullptr, &dst);
    });
    probe("xpe_copy_image", "target", [] {
        XpeImageBuffer src{};
        return xpe_copy_image(&src, nullptr);
    });

    // The table is the deliverable, so print it whether or not a row failed.
    std::printf("\n%-24s %-14s %-14s %-14s\n",
                "entry point", "NULL argument", "uninitialized", "initialized");
    for (const auto& r : g_rows) {
        std::printf("%-24s %-14s %-14d %-14d\n",
                    r.entryPoint, r.nullArgument,
                    static_cast<int>(r.uninitialized),
                    static_cast<int>(r.initialized));
    }
    std::printf("(XPE_ERR_INVALID_INPUT = %d, XPE_ERR_NOT_INITIALIZED = %d)\n\n",
                static_cast<int>(XPE_ERR_INVALID_INPUT),
                static_cast<int>(XPE_ERR_NOT_INITIALIZED));

    EXPECT_EQ(10u, g_rows.size()) << "every planned row must have been probed";
}

} // namespace
