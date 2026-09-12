// #142 / #120 (QA-B-52): what happens when dataSize is smaller than the image
// the descriptor declares?
//
// QA-B-41 settled the EMPTY image (width/height 0, data NULL) across every post
// module. The other shape -- non-zero but inconsistent -- was only ever checked
// inside dicom. api-spec "XpeImageBuffer.dataSize on input" (#123) says a
// non-zero dataSize smaller than width * height * bytesPerPixel is
// XPE_ERR_INVALID_INPUT, and enhance_basic and display each carry a
// size check. ai carries its own copy inline in validateImageBuffer()
// (ai.cpp:158-171) rather than as a named helper, which is why a grep for the
// helper name does not find it -- a reminder that a census by symbol name is
// not a census by behaviour. The probe below runs the exports instead.
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

#include "xpe/ai/ai_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_memory.h"

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

TEST(AiDataSizeProbe, ShortDataSizeIsRejectedWithoutOverreading) {
    ASSERT_EQ(XPE_OK, xpe_ai_init("dummy_model_dir", nullptr));

    const struct {
        const char* name;
        ProbeResult (*run)(XpeImageBuffer*);
    } cases[] = {
        {"xpe_bodypart_recognize", [](XpeImageBuffer* im) {
            return RunProbe([im] {
                char part[64] = {0};
                float conf = 0.0f;
                return xpe_bodypart_recognize(im, part, sizeof(part), &conf);
            });
        }},
        {"xpe_dl_denoise", [](XpeImageBuffer* im) {
            return RunProbe([im] {
                XpeImageMetadata meta{};
                return xpe_dl_denoise(im, &meta, nullptr);
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

        EXPECT_FALSE(r.overread) << c.name << " read past dataSize";
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, r.rc)
            << c.name << " accepted a buffer too small for the image it declares";
    }

    xpe_ai_shutdown();
}

// xpe_bone_suppress and xpe_stitch_images both take an OUTPUT image as well, so
// they are probed separately: the short buffer goes in as the INPUT, and the
// output is a full, ordinary buffer. Mixing the two in one case would leave it
// ambiguous which of the two the rejection came from.
TEST(AiDataSizeProbe, ShortInputRejectedWhenOutputIsWellFormed) {
    ASSERT_EQ(XPE_OK, xpe_ai_init("dummy_model_dir", nullptr));

    ProbeImage in(kShortBytes);
    ASSERT_TRUE(in.buf.valid());

    XpeImageBuffer out{};
    ASSERT_EQ(XPE_OK, xpe_alloc_image(kW, kH, XPE_PIXEL_FLOAT32, &out));

    const ProbeResult bone = RunProbe([&] {
        return xpe_bone_suppress(&in.img, &out, nullptr);
    });
    GTEST_LOG_(INFO) << "xpe_bone_suppress declared=" << kDeclaredBytes
                     << "B dataSize=" << kShortBytes << "B rc=" << bone.rc
                     << " overread=" << (bone.overread ? "YES" : "no");
    EXPECT_FALSE(bone.overread);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, bone.rc);

    // partCount must be >= 2 (ai.cpp:404), so the array pairs a well-formed part
    // with the short one -- a single-part call would be rejected on the count and
    // tell us nothing about dataSize.
    ProbeImage good(kDeclaredBytes);
    ASSERT_TRUE(good.buf.valid());
    XpeImageBuffer parts[2] = { good.img, in.img };

    const ProbeResult stitchCtrl = RunProbe([&] {
        XpeImageBuffer bothGood[2] = { good.img, good.img };
        uint32_t w = 0, h = 0;
        return xpe_stitch_estimate_size(bothGood, 2u, &w, &h);
    });
    GTEST_LOG_(INFO) << "xpe_stitch_estimate_size CONTROL rc=" << stitchCtrl.rc;
    ASSERT_NE(XPE_ERR_INVALID_INPUT, stitchCtrl.rc)
        << "two well-formed parts are rejected too, so the short-part result "
           "would say nothing about the dataSize check";

    const ProbeResult estimate = RunProbe([&] {
        uint32_t w = 0, h = 0;
        return xpe_stitch_estimate_size(parts, 2u, &w, &h);
    });
    GTEST_LOG_(INFO) << "xpe_stitch_estimate_size declared=" << kDeclaredBytes
                     << "B dataSize=" << kShortBytes << "B rc=" << estimate.rc
                     << " overread=" << (estimate.overread ? "YES" : "no");
    EXPECT_FALSE(estimate.overread);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, estimate.rc);

    const ProbeResult stitch = RunProbe([&] {
        return xpe_stitch_images(parts, 2u, &out, nullptr);
    });
    GTEST_LOG_(INFO) << "xpe_stitch_images declared=" << kDeclaredBytes
                     << "B dataSize=" << kShortBytes << "B rc=" << stitch.rc
                     << " overread=" << (stitch.overread ? "YES" : "no");
    EXPECT_FALSE(stitch.overread);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, stitch.rc);

    xpe_free_image(&out);
    xpe_ai_shutdown();
}

#endif  // _WIN32
