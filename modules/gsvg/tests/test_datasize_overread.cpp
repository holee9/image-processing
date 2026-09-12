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

// Control: full-length buffers process normally. Without this, a rejection
// below could be blamed on the fixture rather than on the module.
TEST(GsvgDataSizeProbe, FullLengthBuffersProcessNormally) {
    void* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_gsvg_init(&handle, nullptr));

    GuardedBuffer src(kPixels * sizeof(uint16_t));
    GuardedBuffer dst(kPixels * sizeof(uint16_t));
    ASSERT_TRUE(src.valid() && dst.valid());

    const ProbeResult r = RunProbe([&] {
        return xpe_gsvg_process(handle, static_cast<const uint16_t*>(src.data()), kPixels,
                                static_cast<uint16_t*>(dst.data()), kPixels,
                                kW, kH, nullptr, 0u);
    });
    GTEST_LOG_(INFO) << "gsvg CONTROL full-length rc=" << r.rc
                     << " overread=" << (r.overread ? "YES" : "no");
    EXPECT_FALSE(r.overread);
    EXPECT_EQ(XPE_OK, r.rc);

    xpe_gsvg_shutdown(handle);
}

// ---------------------------------------------------------------------------
// #152 (QA-B-53): a short src is now REJECTED, and the guard page confirms it is
// not read on the way to the rejection.
//
// This case existed in QA-B-52 as KnownDivergence_ShortSourceBufferIsReadPast-
// ItsEnd, asserting rc == XPE_OK with overread == YES. That record was not
// wrong when it was written: the function had no length argument, so there was
// nothing to check against and nothing to fix without changing the signature.
// The signature changed (#152), so the record is REPLACED -- not corrected.
//
// The return code alone would not settle this. A check placed after the first
// read returns the right code and still over-reads, so the second assertion is
// the one that carries the claim.
// ---------------------------------------------------------------------------
TEST(GsvgDataSizeProbe, ShortSourceBufferIsRejectedWithoutBeingRead) {
    void* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_gsvg_init(&handle, nullptr));

    GuardedBuffer src(kPixels * sizeof(uint16_t) / 2u);   // half the promised pixels
    GuardedBuffer dst(kPixels * sizeof(uint16_t));
    ASSERT_TRUE(src.valid() && dst.valid());

    const ProbeResult r = RunProbe([&] {
        return xpe_gsvg_process(handle, static_cast<const uint16_t*>(src.data()), kPixels / 2u,
                                static_cast<uint16_t*>(dst.data()), kPixels,
                                kW, kH, nullptr, 0u);
    });
    GTEST_LOG_(INFO) << "gsvg short-src pixels_promised=" << kPixels
                     << " srcCount=" << (kPixels / 2)
                     << " rc=" << r.rc
                     << " overread=" << (r.overread ? "YES" : "no");

    EXPECT_EQ(XPE_ERR_INVALID_INPUT, r.rc);
    EXPECT_FALSE(r.overread) << "rejected, but only after reading past the buffer";

    xpe_gsvg_shutdown(handle);
}

// Same for the gainMap. The vignette step runs only when the config flag is set
// (gsvg.cpp:218) and the flag defaults to false (gsvg.cpp:190), so a default
// handle never touches the map -- QA-B-52 nearly read that as "the header is
// wrong". The flag is enabled here so the measurement is about gsvg rather than
// about the default config.
TEST(GsvgDataSizeProbe, ShortGainMapIsRejectedWithoutBeingRead) {
    void* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_gsvg_init(&handle, "{\"vignette_correction\": true}"));

    GuardedBuffer src(kPixels * sizeof(uint16_t));
    GuardedBuffer dst(kPixels * sizeof(uint16_t));
    GuardedBuffer gain(kPixels * sizeof(float) / 2u);     // half the promised entries
    ASSERT_TRUE(src.valid() && dst.valid() && gain.valid());

    const ProbeResult r = RunProbe([&] {
        return xpe_gsvg_process(handle, static_cast<const uint16_t*>(src.data()), kPixels,
                                static_cast<uint16_t*>(dst.data()), kPixels,
                                kW, kH,
                                static_cast<const float*>(gain.data()), kPixels / 2u);
    });
    GTEST_LOG_(INFO) << "gsvg short-gainMap entries_promised=" << kPixels
                     << " gainCount=" << (kPixels / 2)
                     << " rc=" << r.rc
                     << " overread=" << (r.overread ? "YES" : "no");

    EXPECT_EQ(XPE_ERR_INVALID_INPUT, r.rc);
    EXPECT_FALSE(r.overread) << "rejected, but only after reading past the map";

    xpe_gsvg_shutdown(handle);
}

// A NULL gainMap carries no length, and passing 0 for it must stay a success --
// otherwise every caller that skips the vignette step would have to invent a
// number to get past the new check.
TEST(GsvgDataSizeProbe, NullGainMapWithZeroLengthIsAccepted) {
    void* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_gsvg_init(&handle, "{\"vignette_correction\": true}"));

    GuardedBuffer src(kPixels * sizeof(uint16_t));
    GuardedBuffer dst(kPixels * sizeof(uint16_t));
    ASSERT_TRUE(src.valid() && dst.valid());

    const ProbeResult r = RunProbe([&] {
        return xpe_gsvg_process(handle, static_cast<const uint16_t*>(src.data()), kPixels,
                                static_cast<uint16_t*>(dst.data()), kPixels,
                                kW, kH, nullptr, 0u);
    });
    GTEST_LOG_(INFO) << "gsvg null-gainMap gainCount=0 rc=" << r.rc
                     << " overread=" << (r.overread ? "YES" : "no");
    EXPECT_EQ(XPE_OK, r.rc) << "a length of 0 for a buffer that does not exist is not a fault";
    EXPECT_FALSE(r.overread);

    xpe_gsvg_shutdown(handle);
}

// ---------------------------------------------------------------------------
// KnownDivergence_ (QA-B-53): what the length argument does NOT fix.
//
// A caller that MISSTATES the length -- hands over a half-size buffer while
// claiming the full count -- is back where QA-B-52 started: the function is told
// the buffer is long enough, believes it, and reads past the end. This is not a
// gap in the check; it is the boundary of what any length parameter can do. It
// is asserted so the limit lives in the suite rather than only in prose, and so
// that a future bounds mechanism that DOES catch it shows up as a change.
// ---------------------------------------------------------------------------
TEST(GsvgDataSizeProbe, KnownDivergence_MisstatedLengthIsStillReadPastItsEnd) {
    void* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_gsvg_init(&handle, nullptr));

    GuardedBuffer src(kPixels * sizeof(uint16_t) / 2u);   // half the pixels...
    GuardedBuffer dst(kPixels * sizeof(uint16_t));
    ASSERT_TRUE(src.valid() && dst.valid());

    const ProbeResult r = RunProbe([&] {
        // ...but the caller claims the full count.
        return xpe_gsvg_process(handle, static_cast<const uint16_t*>(src.data()), kPixels,
                                static_cast<uint16_t*>(dst.data()), kPixels,
                                kW, kH, nullptr, 0u);
    });
    GTEST_LOG_(INFO) << "gsvg misstated-length claimed=" << kPixels
                     << " actually_mapped=" << (kPixels / 2)
                     << " rc=" << r.rc
                     << " overread=" << (r.overread ? "YES" : "no");

    EXPECT_TRUE(r.overread)
        << "if this now passes, something other than the declared length is "
           "bounding the read -- record what, rather than deleting the case";

    xpe_gsvg_shutdown(handle);
}

#endif  // _WIN32
