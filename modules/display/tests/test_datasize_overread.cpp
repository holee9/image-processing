// #142 / #120 (QA-B-52): what happens when dataSize is smaller than the image
// the descriptor declares?
//
// QA-B-41 settled the EMPTY image (width/height 0, data NULL) across every post
// module. The other shape -- non-zero but inconsistent -- was only ever checked
// inside dicom. api-spec "XpeImageBuffer.dataSize on input" (#123) says a
// non-zero dataSize smaller than width * height * bytesPerPixel is
// XPE_ERR_INVALID_INPUT, and enhance_basic and display each carry a
// xpe_data_size_is_consistent() helper. display routes all three LUT entry
// points through xpe_validate_float32(), which calls it -- so this file is
// expected to confirm rather than to find. It runs anyway: a check that exists
// in the source is not a check that fires at runtime, and only one of those two
// statements can be tested.
//
// The probe places the buffer so that the first byte past dataSize is an
// unmapped guard page. A function that respects dataSize returns a code; one
// that reads the image it was promised walks into the guard page and raises an
// access violation, which __except catches and reports as an over-read rather
// than taking the test process down with it.
//
// The helper is duplicated per module test file on purpose: module test
// directories share no utility target, and xpe_common belongs to another lane.
// Four short copies beat one cross-lane dependency.

#include <gtest/gtest.h>

#include "xpe/display/display_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#if defined(_WIN32)
#  include <windows.h>
#endif

#include <cstdint>
#include <cstring>
#include <cstdlib>

namespace {

#if defined(_WIN32)

// A buffer of exactly `bytes` usable bytes, followed immediately by a page that
// faults on any access.
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
        // Right-align the requested size against the guard page, so byte
        // `bytes` is the first faulting address.
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

// Outcome of one probe call.
struct ProbeResult {
    bool        overread = false;   // faulted past dataSize
    XpeErrorCode rc = XPE_OK;
};

// __except cannot live in a frame that needs C++ unwinding, so the call is
// wrapped in its own minimal function.
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

constexpr uint32_t kW = 64;
constexpr uint32_t kH = 64;
constexpr uint32_t kDeclaredBytes = kW * kH * 4u;     // FLOAT32
constexpr uint32_t kShortBytes    = kDeclaredBytes / 2u;

// An image whose descriptor promises kW x kH FLOAT32 while dataSize -- and the
// mapping behind it -- covers only `bytes` of that. With bytes == kDeclaredBytes
// it is a perfectly ordinary image, which is what makes it usable as the control.
struct ProbeImage {
    GuardedBuffer  buf;
    XpeImageBuffer img{};
    explicit ProbeImage(uint32_t bytes) : buf(bytes) {
        img.width         = kW;
        img.height        = kH;
        img.format        = XPE_PIXEL_FLOAT32;
        img.bitsAllocated = 32;
        img.bitsStored    = 32;
        img.data          = buf.data();
        img.dataSize      = bytes;
    }
};

#endif  // _WIN32

}  // namespace

#if defined(_WIN32)

TEST(DisplayDataSizeProbe, ShortDataSizeIsRejectedWithoutOverreading) {
    const struct {
        const char* name;
        ProbeResult (*run)(XpeImageBuffer*);
    } cases[] = {
        {"xpe_apply_modality_lut", [](XpeImageBuffer* im) {
            return RunProbe([im] {
                XpeModalityLutParams p{};
                p.rescaleSlope = 1.0f;
                p.rescaleIntercept = 0.0f;
                return xpe_apply_modality_lut(im, &p);
            });
        }},
        {"xpe_apply_voi_lut", [](XpeImageBuffer* im) {
            return RunProbe([im] {
                XpeVoiLutParams p{};
                p.mode   = XPE_VOI_LINEAR;
                p.center = 0.5f;
                p.width  = 1.0f;
                p.minOut = 0.0f;
                p.maxOut = 1.0f;
                return xpe_apply_voi_lut(im, &p);
            });
        }},
    };

    for (const auto& c : cases) {
        SCOPED_TRACE(c.name);

        // Control first -- see the enhance_advanced probe for why (QA-B-40).
        ProbeImage full(kDeclaredBytes);
        ASSERT_TRUE(full.buf.valid()) << "guard allocation failed; the probe would be blind";
        const ProbeResult ctrl = c.run(&full.img);
        GTEST_LOG_(INFO) << c.name << " CONTROL dataSize=" << kDeclaredBytes << "B"
                         << " rc=" << ctrl.rc
                         << " overread=" << (ctrl.overread ? "YES" : "no");
        ASSERT_FALSE(ctrl.overread) << c.name << " over-read a CONSISTENT buffer";
        ASSERT_NE(XPE_ERR_INVALID_INPUT, ctrl.rc)
            << c.name << " rejects a well-formed image too, so a rejection of the "
                         "short one says nothing about the dataSize check";

        ProbeImage short_(kShortBytes);
        ASSERT_TRUE(short_.buf.valid());
        const ProbeResult r = c.run(&short_.img);
        GTEST_LOG_(INFO) << c.name
                         << " declared=" << kDeclaredBytes << "B"
                         << " dataSize=" << kShortBytes << "B"
                         << " rc=" << r.rc
                         << " overread=" << (r.overread ? "YES" : "no");

        EXPECT_FALSE(r.overread)
            << c.name << " read past dataSize";
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, r.rc)
            << c.name << " accepted a buffer too small for the image it declares";
    }
}

#endif  // _WIN32

// xpe_apply_presentation_lut is probed separately and WITHOUT the guard page.
//
// REQ-DISP-019 has it change the image's domain in place: it std::malloc()s a
// uint16 buffer, std::free()s the float32 one, and installs the new pointer
// (presentation_lut.cpp:32-61). A VirtualAlloc-backed buffer handed to
// std::free() is undefined behaviour, so the guard-page probe cannot be pointed
// at this function -- the crash it produced would be the probe's fault, not the
// module's. The trade is explicit: this case checks the RETURN CODE only, and
// says nothing about whether a short buffer would be read past. That limit is
// recorded in the QA-B-52 report rather than papered over.
TEST(DisplayDataSizeProbe, PresentationLutShortDataSizeReturnsInvalidInput_RcOnly) {
    auto run = [](uint32_t bytes) {
        XpeImageBuffer img{};
        img.width         = kW;
        img.height        = kH;
        img.format        = XPE_PIXEL_FLOAT32;
        img.bitsAllocated = 32;
        img.bitsStored    = 32;
        img.dataSize      = bytes;
        img.data          = std::calloc(1, kDeclaredBytes);   // always fully mapped
        XpePresentationLutParams p{};
        for (int i = 0; i < 1024; ++i) {
            p.lutData[i] = static_cast<uint16_t>(i * 64);
        }
        p.gsdfEnabled = 0;
        const XpeErrorCode rc = xpe_apply_presentation_lut(&img, &p);
        std::free(img.data);
        return rc;
    };

    const XpeErrorCode ctrl = run(kDeclaredBytes);
    GTEST_LOG_(INFO) << "xpe_apply_presentation_lut CONTROL rc=" << ctrl;
    ASSERT_NE(XPE_ERR_INVALID_INPUT, ctrl)
        << "rejects a well-formed image too, so the short-case result would say nothing";

    const XpeErrorCode rc = run(kShortBytes);
    GTEST_LOG_(INFO) << "xpe_apply_presentation_lut declared=" << kDeclaredBytes
                     << "B dataSize=" << kShortBytes << "B rc=" << rc
                     << " overread=NOT MEASURED (allocator conflict, see comment)";
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, rc)
        << "accepted a buffer too small for the image it declares";
}
