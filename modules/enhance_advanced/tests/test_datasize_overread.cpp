// #142 / #120 (QA-B-52): what happens when dataSize is smaller than the image
// the descriptor declares?
//
// QA-B-41 settled the EMPTY image (width/height 0, data NULL) across every post
// module. The other shape -- non-zero but inconsistent -- was only ever checked
// inside dicom. api-spec "XpeImageBuffer.dataSize on input" (#123) says a
// non-zero dataSize smaller than width * height * bytesPerPixel is
// XPE_ERR_INVALID_INPUT, and enhance_basic and display each carry a
// xpe_data_size_is_consistent() helper that enforces it. enhance_advanced,
// ai, and gsvg carry no such check. Whether that matters is not a reading
// question: the point of the probe below is that it RUNS.
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

#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#if defined(_WIN32)
#  include <windows.h>
#endif

#include <cstdint>
#include <cstring>

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

TEST(EnhanceAdvancedDataSizeProbe, ShortDataSizeIsRejectedWithoutOverreading) {
    ASSERT_EQ(XPE_OK, xpe_enhance_advanced_init(nullptr));

    // Each lambda is stateless so it converts to a plain function pointer, which
    // keeps the __try frame free of C++ objects.
    const struct {
        const char* name;
        ProbeResult (*run)(XpeImageBuffer*);
    } cases[] = {
        {"xpe_multiscale_process", [](XpeImageBuffer* im) {
            return RunProbe([im] {
                XpeImageMetadata meta{};
                return xpe_multiscale_process(im, &meta, nullptr);
            });
        }},
        {"xpe_fractional_process", [](XpeImageBuffer* im) {
            return RunProbe([im] { return xpe_fractional_process(im, 0.5f, nullptr); });
        }},
        {"xpe_detect_collimation", [](XpeImageBuffer* im) {
            return RunProbe([im] {
                int32_t a = 0, b = 0, c = 0, d = 0;
                return xpe_detect_collimation(im, &a, &b, &c, &d, nullptr);
            });
        }},
        {"xpe_calc_exposure_index", [](XpeImageBuffer* im) {
            return RunProbe([im] {
                XpeImageMetadata meta{};
                float ei = 0.0f, di = 0.0f;
                return xpe_calc_exposure_index(im, &meta, &ei, &di);
            });
        }},
    };

    for (const auto& c : cases) {
        SCOPED_TRACE(c.name);

        // CONTROL FIRST. A function that rejects every input would pass the
        // short-dataSize assertion for the wrong reason -- the QA-B-40 lesson,
        // where an 8x8 fixture was rejected on size and nearly credited the
        // validator. So the same descriptor with a CONSISTENT dataSize is run
        // first, and the short case is only meaningful if this one is accepted.
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
        ASSERT_TRUE(short_.buf.valid()) << "guard allocation failed; the probe would be blind";

        const ProbeResult r = c.run(&short_.img);
        GTEST_LOG_(INFO) << c.name
                         << " declared=" << kDeclaredBytes << "B"
                         << " dataSize=" << kShortBytes << "B"
                         << " rc=" << r.rc
                         << " overread=" << (r.overread ? "YES" : "no");

        EXPECT_FALSE(r.overread)
            << c.name << " read past dataSize -- api-spec #123 says a short "
                         "dataSize is XPE_ERR_INVALID_INPUT, not a licence to "
                         "read the image the descriptor claims";
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, r.rc)
            << c.name << " accepted a buffer too small for the image it declares";
    }

    xpe_enhance_advanced_shutdown();
}

#endif  // _WIN32
