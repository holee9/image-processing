/**
 * @file test_runtime_detection_map_length.cpp
 * @brief QA-A-63 (#144): DetectFrame does not write outside the length it was given.
 *
 * WHAT THIS ASSERTS, and what it deliberately does not.
 *
 * The weak assertion -- the one that looks like a test and is not -- is "a
 * length argument was passed". That is satisfied by a function that takes the
 * argument and ignores it. What matters is the other sentence: the function does
 * not touch memory beyond @p mapCount. Only the hardware can answer that, so the
 * map lives immediately in front of a PAGE_NOACCESS page and any byte written
 * past the promised end raises an access violation instead of corrupting the
 * heap and passing.
 *
 * The hazard being guarded is new, and QA-A-62 is what created it. Before that
 * card the caller cleared the map, so the size lived with the caller who knew
 * it; QA-A-62 moved the clear into DetectFrame and left the size behind. From
 * then until this card, the function wrote width * height bytes into a buffer
 * whose length it had no way to learn.
 *
 * REJECT, NOT TRUNCATE. A short map could also be "handled" by clearing and
 * filling only the first mapCount bytes. That returns normally and leaves the
 * tail holding whatever it held, which reads exactly like a detection -- the
 * shape #150 already cost this repository once. So the test below requires the
 * short call to leave the buffer ALONE, not to fill part of it.
 *
 * The guard-page fixture is the one QA-B-53 used for the same question in gsvg
 * (modules/gsvg/tests/test_datasize_overread.cpp), duplicated rather than
 * shared: module test directories have no common utility target, and xpe_common
 * belongs to another lane.
 *
 * NOT COVERED (stated rather than implied): a caller that LIES -- passes
 * mapCount == width * height while the buffer is shorter. No in-function check
 * can catch that; the length is the caller's assertion about its own memory.
 *
 * SPEC: XPE-ALG-001 section 9.8.  Refs #144 #143
 */

#include <gtest/gtest.h>

#include "runtime_detection.h"
#include "xpe/common/xpe_types.h"

#if defined(_WIN32)
#  include <windows.h>
#endif

#include <cstdint>
#include <cstring>
#include <vector>

namespace {

using xpe::preprocess::internal::DetectFrame;

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
        m_data = m_base + (usable - bytes);
        std::memset(m_data, 0, bytes);
    }
    ~GuardedBuffer() { if (m_base) VirtualFree(m_base, 0, MEM_RELEASE); }
    GuardedBuffer(const GuardedBuffer&) = delete;
    GuardedBuffer& operator=(const GuardedBuffer&) = delete;

    uint8_t* data() const { return m_data; }
    bool     valid() const { return m_base != nullptr; }

private:
    uint8_t* m_base = nullptr;
    uint8_t* m_data = nullptr;
};

// __try/__except may not coexist with objects needing unwinding in one frame,
// so the call is isolated here and the caller keeps its C++ objects outside.
bool CallFaulted(const XpeImageBuffer* img,
                 RuntimeDetectionConfig cfg,
                 uint8_t* map,
                 size_t mapCount) {
    __try {
        DetectFrame(img, cfg, map, mapCount);
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
                    ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        return true;
    }
    return false;
}

constexpr uint32_t kW = 64u;
constexpr uint32_t kH = 64u;
constexpr size_t kPixels = static_cast<size_t>(kW) * kH;

std::vector<float> MakeFrame() {
    std::vector<float> f(kPixels, 3000.0f);
    for (size_t i = 0; i < kPixels; ++i) f[i] += static_cast<float>(i % 7u);
    for (size_t i = 13; i < kPixels; i += 101) f[i] += 400.0f;   // real outliers
    return f;
}

XpeImageBuffer Wrap(std::vector<float>& px) {
    XpeImageBuffer img{};
    img.data = px.data();
    img.width = kW;
    img.height = kH;
    img.bitsAllocated = 32;
    img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.dataSize = px.size() * sizeof(float);
    return img;
}

#endif  // _WIN32

}  // namespace

#if defined(_WIN32)

// Control. Without it, a "no fault" below could be the fixture never arming
// rather than the function behaving -- and the exact-fit case is also where an
// off-by-one in the check (<= instead of <) would show up as a fault.
TEST(MapLengthTest, ExactLengthMapIsFilledAndDoesNotFault) {
    std::vector<float> frame = MakeFrame();
    XpeImageBuffer img = Wrap(frame);

    GuardedBuffer map(kPixels);
    ASSERT_TRUE(map.valid());

    RuntimeDetectionConfig cfg = RuntimeDetection_DefaultConfig();
    cfg.threadCount = 1;

    ASSERT_FALSE(CallFaulted(&img, cfg, map.data(), kPixels))
        << "an exactly-sized map must be accepted; the check is < needed, not <=";

    size_t flagged = 0;
    for (size_t i = 0; i < kPixels; ++i) if (map.data()[i]) ++flagged;
    EXPECT_GT(flagged, 0u) << "an all-zero map would make the short case below meaningless";
}

// The assertion this card exists for: a map shorter than width * height is left
// untouched. A function that took mapCount and ignored it would fault here on
// the memset; one that truncated would pass the fault check and fail the
// untouched check.
TEST(MapLengthTest, ShortMapIsRejectedWithoutBeingWritten) {
    std::vector<float> frame = MakeFrame();
    XpeImageBuffer img = Wrap(frame);

    const size_t shortCount = kPixels / 2u;
    GuardedBuffer map(shortCount);
    ASSERT_TRUE(map.valid());
    std::memset(map.data(), 0xCD, shortCount);          // a value the function never writes

    RuntimeDetectionConfig cfg = RuntimeDetection_DefaultConfig();
    cfg.threadCount = 1;

    EXPECT_FALSE(CallFaulted(&img, cfg, map.data(), shortCount))
        << "wrote past the " << shortCount << " elements it was given (frame needs "
        << kPixels << ")";

    size_t touched = 0;
    for (size_t i = 0; i < shortCount; ++i) if (map.data()[i] != 0xCDu) ++touched;
    EXPECT_EQ(0u, touched)
        << touched << " of " << shortCount
        << " bytes changed: the call was truncated rather than rejected";
}

// Threaded path, same question. The rejection is ordered before the split, so a
// thread count must not be able to route around it.
TEST(MapLengthTest, ShortMapIsRejectedOnTheThreadedPathToo) {
    std::vector<float> frame = MakeFrame();
    XpeImageBuffer img = Wrap(frame);

    const size_t shortCount = kPixels - 1u;             // one element short
    GuardedBuffer map(shortCount);
    ASSERT_TRUE(map.valid());
    std::memset(map.data(), 0xCD, shortCount);

    RuntimeDetectionConfig cfg = RuntimeDetection_DefaultConfig();
    cfg.threadCount = 8;

    EXPECT_FALSE(CallFaulted(&img, cfg, map.data(), shortCount))
        << "one element short still wrote past the end at 8 threads";

    size_t touched = 0;
    for (size_t i = 0; i < shortCount; ++i) if (map.data()[i] != 0xCDu) ++touched;
    EXPECT_EQ(0u, touched) << "truncated rather than rejected at 8 threads";
}

// A longer-than-needed map is legitimate -- a caller reusing one buffer across
// frame sizes -- and only the first width * height elements may be touched.
TEST(MapLengthTest, SurplusBeyondWidthTimesHeightIsNotTouched) {
    std::vector<float> frame = MakeFrame();
    XpeImageBuffer img = Wrap(frame);

    const size_t surplus = 4096u;
    std::vector<uint8_t> map(kPixels + surplus, 0xCDu);

    RuntimeDetectionConfig cfg = RuntimeDetection_DefaultConfig();
    cfg.threadCount = 1;
    DetectFrame(&img, cfg, map.data(), map.size());

    size_t touchedTail = 0;
    for (size_t i = kPixels; i < map.size(); ++i) if (map[i] != 0xCDu) ++touchedTail;
    EXPECT_EQ(0u, touchedTail)
        << touchedTail << " of " << surplus
        << " surplus bytes were written; the clear must be sized by the frame, "
           "not by the buffer";
}

#else

TEST(MapLengthTest, GuardPageFixtureIsWindowsOnly) {
    GTEST_SKIP() << "the guard-page fixture uses VirtualAlloc/VirtualProtect";
}

#endif  // _WIN32
