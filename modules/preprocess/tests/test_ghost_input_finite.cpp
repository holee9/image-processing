/**
 * @file test_ghost_input_finite.cpp
 * @brief xpe_ghost_correct refuses a non-finite frame at the entrance and leaves a failed frame's pixels as they came
 *        in (QA-A-217, #233).
 *
 * Measured before the change (QA-A-216): a NaN or an infinity in the frame answered XPE_ERR_PROCESSING_FAILED with
 * every pixel before it already corrected (35 of 64, 63 of 64 changed), which REQ-P1A-032 does not allow ("leave the
 * output unmodified or zero-fill it"). The handle's history was never at risk -- it is committed only for a whole
 * successful frame (QA-A-202c) -- and these tests pin that too. A second defect: two frames of finite values near the
 * top of the float range made the NEW history overflow to infinity, which was committed, and every later frame
 * failed until xpe_ghost_reset.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "preprocess_state_fixture.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr uint32_t W = 8, H = 8;
constexpr size_t N = static_cast<size_t>(W) * H;
const float kNaN = std::numeric_limits<float>::quiet_NaN();
const float kInf = std::numeric_limits<float>::infinity();

XpeImageBuffer floatBuf(std::vector<float>& v) {
    XpeImageBuffer b{};
    b.data = v.data(); b.width = W; b.height = H; b.bitsAllocated = 32; b.bitsStored = 32;
    b.format = XPE_PIXEL_FLOAT32; b.dataSize = v.size() * sizeof(float);
    return b;
}

std::string findAlert(const std::string& prefix) {
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < n; ++i) {
        char buf[1024];
        int32_t sev = 0;
        if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) == XPE_OK && std::string(buf).rfind(prefix, 0) == 0) return buf;
    }
    return {};
}

struct State {
    std::vector<float> h1, h2;
    double lastAcq{0.0}, lastMean{0.0}, weight{0.0};
    bool operator==(const State& o) const {
        return h1 == o.h1 && h2 == o.h2 && lastAcq == o.lastAcq && lastMean == o.lastMean && weight == o.weight;
    }
};
State stateOf(void* handle) {
    auto* gh = static_cast<GhostCorrectorHandle*>(handle);
    return State{gh->hist1, gh->hist2, gh->lastAcqTimeSec, gh->lastFrameMean, gh->exposureWeight};
}

bool sameBytes(const std::vector<float>& a, const std::vector<float>& b) {
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}

std::vector<float> frame(unsigned seed) {
    std::mt19937 g(seed);
    std::uniform_real_distribution<float> d(1000.0f, 30000.0f);
    std::vector<float> f(N);
    for (auto& x : f) x = d(g);
    return f;
}

class GhostInputFinite : public XpePreprocessStateFixture {
protected:
    void SetUp() override { XpePreprocessStateFixture::SetUp(); xpe_clear_alerts(); }
    void TearDown() override {
        for (void* h : handles_) xpe_ghost_destroy(h);
        xpe_clear_alerts();
        XpePreprocessStateFixture::TearDown();
    }
    void* create(int tier, const char* extra = "") {
        std::string cfg = "{\"tier\":\"" + std::to_string(tier) + "\"" + extra + "}";
        void* h = nullptr;
        EXPECT_EQ(XPE_OK, xpe_ghost_create(W, H, cfg.c_str(), &h));
        handles_.push_back(h);
        return h;
    }
    XpeErrorCode feed(void* h, std::vector<float>& f, uint64_t t) {
        XpeImageBuffer b = floatBuf(f);
        XpeImageMetadata m{};
        m.acquisitionTime = t;
        return xpe_ghost_correct(h, &b, &m);
    }
    std::vector<void*> handles_;
};

}  // namespace

TEST_F(GhostInputFinite, ANonFiniteFrameIsRefusedWithNothingWrittenAndTheHandleUntouched) {
    struct Case { const char* name; size_t at; float v; bool all; };
    const Case cases[] = {{"NaN at pixel 0", 0, kNaN, false}, {"NaN in the middle", 35, kNaN, false},
                          {"+inf at the last pixel", N - 1, kInf, false}, {"-inf in the middle", 35, -kInf, false},
                          {"every pixel NaN", 0, kNaN, true}};
    for (const int tier : {1, 2, 3}) {
        for (const bool seeded : {false, true}) {
            for (const Case& c : cases) {
                SCOPED_TRACE(std::string(c.name) + ", tier " + std::to_string(tier) + (seeded ? ", after 2 good frames" : ", first frame"));
                void* h = create(tier);
                void* control = create(tier);    // never offered the bad frame
                uint64_t t = 1;
                if (seeded) {
                    for (unsigned s : {1u, 2u}) {
                        std::vector<float> a = frame(s), b = a;
                        ASSERT_EQ(XPE_OK, feed(h, a, t));
                        ASSERT_EQ(XPE_OK, feed(control, b, t));
                        ++t;
                    }
                }
                std::vector<float> bad = frame(7);
                if (c.all) std::fill(bad.begin(), bad.end(), c.v); else bad[c.at] = c.v;
                const std::vector<float> badBefore = bad;
                const State before = stateOf(h);
                xpe_clear_alerts();
                EXPECT_EQ(XPE_ERR_INVALID_INPUT, feed(h, bad, t));
                EXPECT_TRUE(sameBytes(badBefore, bad)) << "no pixel of the refused frame was corrected";
                EXPECT_TRUE(stateOf(h) == before) << "the history, the time and the exposure estimate are as they were";
                EXPECT_FALSE(findAlert("XPE_WARN_GHOST_INPUT_NOT_FINITE:").empty());

                // the next good frame comes out bit for bit as if the refused one had never been offered
                std::vector<float> g1 = frame(9), g2 = g1;
                ASSERT_EQ(XPE_OK, feed(h, g1, t + 1));
                ASSERT_EQ(XPE_OK, feed(control, g2, t + 1));
                EXPECT_TRUE(sameBytes(g1, g2));
            }
        }
    }
}

TEST_F(GhostInputFinite, TheAlertNamesTheCountAndTheFirstPixel) {
    void* h = create(1);
    std::vector<float> bad = frame(3);
    bad[13] = kInf; bad[40] = kNaN;
    xpe_clear_alerts();
    ASSERT_EQ(XPE_ERR_INVALID_INPUT, feed(h, bad, 1));
    EXPECT_EQ("XPE_WARN_GHOST_INPUT_NOT_FINITE: 2 pixel(s) of the input frame are NaN or infinite (first: index 13, x=5, y=1); "
              "the frame was not corrected; the buffer and the handle's history were not changed",
              findAlert("XPE_WARN_GHOST_INPUT_NOT_FINITE:"));
}

TEST_F(GhostInputFinite, AFiniteFrameThatOverflowsTheHistoryFailsWithoutCommittingItAndTheNextFrameSucceeds) {
    // Frames of 3e38 (finite): the new history decay*h + raw reaches infinity after a frame or two (tiers 1 and 2: the
    // second frame; tier 3: its neighbour mean overflows on the first). Before QA-A-217 an infinite history was
    // committed and every later frame failed until xpe_ghost_reset.
    for (const int tier : {1, 2, 3}) {
        SCOPED_TRACE("tier " + std::to_string(tier));
        void* h = create(tier);
        void* control = create(tier);
        uint64_t t = 1;
        bool failed = false;
        for (int i = 0; i < 4 && !failed; ++i, ++t) {
            std::vector<float> big(N, 3.0e38f);
            const std::vector<float> before = big;
            const State stateBefore = stateOf(h);
            const XpeErrorCode rc = feed(h, big, t);
            if (rc == XPE_OK) {
                std::vector<float> same = before;
                ASSERT_EQ(XPE_OK, feed(control, same, t));      // the control sees the same successful frames
                continue;
            }
            failed = true;
            EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, rc);
            EXPECT_TRUE(sameBytes(before, big)) << "a failed frame leaves its pixels as they came in";
            EXPECT_TRUE(stateOf(h) == stateBefore) << "and does not commit its history";
        }
        ASSERT_TRUE(failed) << "control: the extreme frames do overflow";

        // The next frame is an all-zero one: after a SUCCESSFUL extreme frame the history is legitimately ~3e38 and an
        // ordinary frame would overflow on the control handle too, but a zero frame is fine on both -- and on a handle
        // that had committed an infinite history it fails (0 - alpha * inf).
        std::vector<float> next(N, 0.0f), nextCtl = next;
        EXPECT_EQ(XPE_OK, feed(h, next, t)) << "the next frame succeeds without a reset";
        ASSERT_EQ(XPE_OK, feed(control, nextCtl, t));
        EXPECT_TRUE(sameBytes(next, nextCtl)) << "and comes out as if the failed frame had never been offered";
        EXPECT_TRUE(stateOf(h) == stateOf(control));
    }
}

TEST_F(GhostInputFinite, AFrameThatFailsHalfWayThroughPutsItsPixelsBack) {
    // tier 3 with a huge nlcscBeta: the product coefficient * history overflows for one bright pixel in the middle of
    // the frame and for no other (same device as test_ghost_failed_frame.cpp), so pixels before it were already corrected
    void* h = create(3, ",\"nlcscBeta\":\"1e37\"");
    std::vector<float> seed(N, 450.0f);
    ASSERT_EQ(XPE_OK, feed(h, seed, 100));
    std::vector<float> bad(N, 450.0f);
    bad[N / 2] = 29950.0f;
    const std::vector<float> before = bad;
    const State stateBefore = stateOf(h);
    ASSERT_EQ(XPE_ERR_PROCESSING_FAILED, feed(h, bad, 101)) << "control: the bright pixel overflows";
    EXPECT_TRUE(sameBytes(before, bad)) << "pixels corrected before the overflow were put back";
    EXPECT_TRUE(stateOf(h) == stateBefore);
}

TEST_F(GhostInputFinite, AFiniteFrameStillComesOutAsItAlwaysDidAndRaisesNothing) {
    // the same frames through two handles of the same tier give the same bytes, and nothing is alerted
    for (const int tier : {1, 2, 3}) {
        void* a = create(tier);
        void* b = create(tier);
        for (unsigned s = 1; s <= 4; ++s) {
            std::vector<float> fa = frame(s), fb = fa;
            xpe_clear_alerts();
            ASSERT_EQ(XPE_OK, feed(a, fa, s));
            ASSERT_EQ(XPE_OK, feed(b, fb, s));
            EXPECT_TRUE(sameBytes(fa, fb));
            for (float x : fa) ASSERT_TRUE(std::isfinite(x));
            EXPECT_EQ(0, xpe_get_pending_alert_count());
        }
    }
}

TEST_F(GhostInputFinite, TheOlderRefusalsKeepTheirPrecedenceOverThePixels) {
    void* h = create(1);
    std::vector<float> bad = frame(3);
    bad[5] = kNaN;
    XpeImageBuffer b = floatBuf(bad);
    XpeImageMetadata m{};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ghost_correct(nullptr, &b, &m));
    b.format = XPE_PIXEL_UINT16;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ghost_correct(h, &b, &m)) << "wrong format";
    EXPECT_EQ(0, xpe_get_pending_alert_count()) << "the pixel scan was not reached for these";
}

TEST_F(GhostInputFinite, AnOverflowOfTheSlowHistoryAloneIsCaughtToo) {
    // tier 1, history 3e38 after a first frame; then raw = 2e38: the fast history decay1*h + raw = 3.1e38 is finite, the
    // slow one decay2*h + raw = 4.9e38 is not. Each history plane is checked, not only the first.
    void* h = create(1);
    std::vector<float> first(N, 3.0e38f);
    ASSERT_EQ(XPE_OK, feed(h, first, 1));
    std::vector<float> second(N, 2.0e38f);
    const std::vector<float> before = second;
    const State stateBefore = stateOf(h);
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, feed(h, second, 2));
    EXPECT_TRUE(sameBytes(before, second));
    EXPECT_TRUE(stateOf(h) == stateBefore);
}
