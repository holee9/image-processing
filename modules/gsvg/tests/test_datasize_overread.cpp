// #142 / #120 (QA-B-52): gsvg and the short-buffer question.
//
// The other post modules take an XpeImageBuffer, so api-spec #123 applies to
// them: a non-zero dataSize smaller than width * height * bytesPerPixel is
// XPE_ERR_INVALID_INPUT, and each of those modules has a
// data_size_is_consistent() helper enforcing it.
//
// gsvg takes loose pointers and dimensions -- there is no dataSize field to be
// inconsistent with, so #123 has nothing to bind to here. Its header already
// states the consequence for one of the three buffers:
//
//   "Its length is NOT validated -- it is trusted to hold width * height
//    entries, and a shorter map is read past its end."  (gsvg_api.h, gainMap)
//
// A documented gap is still a gap, and a sentence in a header is not a
// measurement. This file measures it: how far does a short buffer actually get
// read? The guard page turns the answer into a signal instead of a guess.
//
// NOTHING IS FIXED HERE. Making gsvg length-aware changes its public signature,
// which is a contract decision outside this card; the QA-B-52 report carries the
// numbers to that decision.

#include <gtest/gtest.h>

#include "xpe/gsvg/gsvg_api.h"
#include "xpe/common/xpe_error.h"

#if defined(_WIN32)
#  include <windows.h>
#endif

#include <cstdint>
#include <cstring>
#include <vector>

namespace {

#if defined(_WIN32)

// A buffer of exactly `bytes` usable bytes, followed immediately by a page that
// faults on any access. Duplicated per module test file on purpose: module test
// directories share no utility target, and xpe_common belongs to another lane.
class GuardedBuffer {
public:
    explicit GuardedBuffer(size_t bytes) {
        SYSTEM_INFO si{};
        GetSystemInfo(&si);
        const size_t page = si.dwPageSize;
        const size_t usable = ((bytes + page - 1) / page) * page;
        m_base = static_cast<uint8_t*>(
            VirtualAlloc(nullptr, usable + page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        if (m_base == nullptr) return;
        DWORD old = 0;
        VirtualProtect(m_base + usable, page, PAGE_NOACCESS, &old);
        m_data = m_base + (usable - bytes);
        std::memset(m_data, 0, bytes);
    }
    ~GuardedBuffer() { if (m_base) VirtualFree(m_base, 0, MEM_RELEASE); }
    GuardedBuffer(const GuardedBuffer&) = delete;
    GuardedBuffer& operator=(const GuardedBuffer&) = delete;

    void* data() const { return m_data; }
    bool  valid() const { return m_base != nullptr; }

private:
    uint8_t* m_base = nullptr;
    uint8_t* m_data = nullptr;
};

struct ProbeResult {
    bool         overread = false;
    XpeErrorCode rc = XPE_OK;
};

template <typename Fn>
ProbeResult RunProbe(Fn fn) {
    ProbeResult r;
    __try {
        r.rc = fn();
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
                    ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        r.overread = true;
    }
    return r;
}

constexpr int kW = 64;
constexpr int kH = 64;
constexpr size_t kPixels = static_cast<size_t>(kW) * kH;

#endif  // _WIN32

}  // namespace

#if defined(_WIN32)

// Control: full-length buffers process normally. Without this, an over-read
// below could be blamed on the fixture rather than on the module.
TEST(GsvgDataSizeProbe, FullLengthBuffersProcessNormally) {
    void* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_gsvg_init(&handle, nullptr));

    GuardedBuffer src(kPixels * sizeof(uint16_t));
    GuardedBuffer dst(kPixels * sizeof(uint16_t));
    ASSERT_TRUE(src.valid() && dst.valid());

    const ProbeResult r = RunProbe([&] {
        return xpe_gsvg_process(handle, static_cast<const uint16_t*>(src.data()),
                                static_cast<uint16_t*>(dst.data()), kW, kH, nullptr);
    });
    GTEST_LOG_(INFO) << "gsvg CONTROL full-length rc=" << r.rc
                     << " overread=" << (r.overread ? "YES" : "no");
    EXPECT_FALSE(r.overread);
    EXPECT_EQ(XPE_OK, r.rc);

    xpe_gsvg_shutdown(handle);
}

// KnownDivergence_ (QA-B-52): gsvg trusts src to hold width * height entries.
// Handing it half that reads past the end -- recorded, not fixed. The module has
// no length parameter to check against, so this is a signature-level contract
// question, raised in the report.
TEST(GsvgDataSizeProbe, KnownDivergence_ShortSourceBufferIsReadPastItsEnd) {
    void* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_gsvg_init(&handle, nullptr));

    GuardedBuffer src(kPixels * sizeof(uint16_t) / 2u);   // half the promised pixels
    GuardedBuffer dst(kPixels * sizeof(uint16_t));
    ASSERT_TRUE(src.valid() && dst.valid());

    const ProbeResult r = RunProbe([&] {
        return xpe_gsvg_process(handle, static_cast<const uint16_t*>(src.data()),
                                static_cast<uint16_t*>(dst.data()), kW, kH, nullptr);
    });
    GTEST_LOG_(INFO) << "gsvg short-src pixels_promised=" << kPixels
                     << " pixels_mapped=" << (kPixels / 2)
                     << " rc=" << r.rc
                     << " overread=" << (r.overread ? "YES" : "no");

    // Today's behaviour, pinned so a future length check shows up as a change.
    EXPECT_TRUE(r.overread)
        << "if this now passes, gsvg has gained a length check -- update the "
           "expectation and the QA-B-52 record rather than deleting the case";

    xpe_gsvg_shutdown(handle);
}

// Same question for the gainMap, which the header calls out by name.
//
// First attempt used a DEFAULT handle and observed no over-read -- which would
// have read as "the header is wrong". It is not: the vignette step runs only
// when BOTH the config flag is set and a gainMap is supplied
// (gsvg.cpp:218), and `vignette_correction` defaults to false (gsvg.cpp:190),
// so a default handle never touches the map. The header's sentence is true
// under the condition it does not state. The probe therefore enables the step
// explicitly; enabling it is what makes the measurement about gsvg rather than
// about the default config.
TEST(GsvgDataSizeProbe, KnownDivergence_ShortGainMapIsReadPastItsEnd) {
    void* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_gsvg_init(&handle, "{\"vignette_correction\": true}"));

    GuardedBuffer src(kPixels * sizeof(uint16_t));
    GuardedBuffer dst(kPixels * sizeof(uint16_t));
    GuardedBuffer gain(kPixels * sizeof(float) / 2u);     // half the promised entries
    ASSERT_TRUE(src.valid() && dst.valid() && gain.valid());

    const ProbeResult r = RunProbe([&] {
        return xpe_gsvg_process(handle, static_cast<const uint16_t*>(src.data()),
                                static_cast<uint16_t*>(dst.data()), kW, kH,
                                static_cast<const float*>(gain.data()));
    });
    GTEST_LOG_(INFO) << "gsvg short-gainMap entries_promised=" << kPixels
                     << " entries_mapped=" << (kPixels / 2)
                     << " rc=" << r.rc
                     << " overread=" << (r.overread ? "YES" : "no");

    // The header states this outcome; the assertion is what keeps the statement
    // true, or tells us when it stops being.
    EXPECT_TRUE(r.overread)
        << "the header says a shorter gainMap is read past its end; if that is "
           "no longer so, the header and this case both need updating";

    xpe_gsvg_shutdown(handle);
}

#endif  // _WIN32
