/**
 * @file test_calib_session_consistency.cpp
 * @brief QA-A-229 M4 (#245): maps loaded together must come from the same session (SRS-CALIB-FUNC-011, S1).
 *
 * Before this check the file's session_id was read, kept and written back but compared by nobody, while the
 * header promised XPE_ERR_CONFIG_INVALID on a session mismatch. The rule now: of the offset / gain / defect
 * maps in the store, two that both carry a session id (non-empty, not "generated") must carry the same one.
 * A map that conflicts is the one refused; the maps already loaded stay and nothing else changes. An
 * unspecified id is left out of the comparison, and the module says so with one warning per mixed state
 * (not per load: the pipeline re-reads its three files on every call).
 */

#include <gtest/gtest.h>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "fixtures/make_xcal.hpp"

namespace {
constexpr uint32_t W = 8, H = 8;
constexpr const char* kWarn = "XPE_WARN_CALIB_SESSION_UNSPECIFIED";

int warningCount() {
    int n = 0;
    char msg[512];
    int32_t sev = -1;
    const int32_t count = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < count; ++i) {
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) != XPE_OK) continue;
        if (std::string(msg).find(kWarn) != std::string::npos) ++n;
    }
    return n;
}

class SessionConsistency : public ::testing::Test {
protected:
    std::filesystem::path dir;
    void SetUp() override {
        dir = std::filesystem::temp_directory_path() / "qa_a_229_session";
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        xpe_clear_alerts();
    }
    void TearDown() override {
        xpe_clear_alerts();
        xpe_calib_cache_clear();
        xpe_preprocess_shutdown();
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }
    std::string off(const char* s, const char* name = "offset.xcal") {
        const std::string p = (dir / name).string();
        EXPECT_EQ(XPE_OK, MakeOffsetXCal(p.c_str(), W, H, 100.0f, 0, s ? s : ""));
        return p;
    }
    std::string gain(const char* s, const char* name = "gain.xcal") {
        const std::string p = (dir / name).string();
        EXPECT_EQ(XPE_OK, MakeGainXCal(p.c_str(), W, H, 2.0f, 0, s ? s : ""));
        return p;
    }
    std::string def(const char* s, const char* name = "defect.xcal") {
        const std::string p = (dir / name).string();
        EXPECT_EQ(XPE_OK, MakeDefectXCal(p.c_str(), W, H, 0, s ? s : ""));
        return p;
    }

    // What is in the store, observed through the processing functions (not through the loader's own return).
    static XpeErrorCode offsetState() {
        std::vector<uint16_t> in(W * H, 500), out(W * H, 0);
        XpeImageBuffer ib{}, ob{};
        ib.width = ob.width = W; ib.height = ob.height = H;
        ib.format = ob.format = XPE_PIXEL_UINT16;
        ib.bitsAllocated = ib.bitsStored = ob.bitsAllocated = ob.bitsStored = 16;
        ib.data = in.data(); ib.dataSize = in.size() * 2;
        ob.data = out.data(); ob.dataSize = out.size() * 2;
        XpeImageMetadata m{};
        const XpeErrorCode rc = xpe_offset_correct(&ib, &ob, &m);
        xpe_clear_alerts();
        return rc;
    }
    static XpeErrorCode gainState() {
        std::vector<uint16_t> in(W * H, 500);
        std::vector<float> out(W * H, 0.0f);
        XpeImageBuffer ib{}, ob{};
        ib.width = ob.width = W; ib.height = ob.height = H;
        ib.format = XPE_PIXEL_UINT16; ob.format = XPE_PIXEL_FLOAT32;
        ib.bitsAllocated = ib.bitsStored = 16; ob.bitsAllocated = ob.bitsStored = 32;
        ib.data = in.data(); ib.dataSize = in.size() * 2;
        ob.data = out.data(); ob.dataSize = out.size() * 4;
        XpeImageMetadata m{};
        const XpeErrorCode rc = xpe_gain_correct(&ib, &ob, &m);
        xpe_clear_alerts();
        return rc;
    }
};
}  // namespace

TEST_F(SessionConsistency, SameSessionLoadsAllThreeWithoutWarning) {
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(off("S1").c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gain("S1").c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(def("S1").c_str()));
    EXPECT_EQ(0, warningCount());
}

// The map that arrives second is the one refused; the first stays. Both orders, so "which one stays" is a
// measurement, not an assumption.
TEST_F(SessionConsistency, ConflictingGainIsRefusedAndTheOffsetStays) {
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(off("S1").c_str()));
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_load_gain(gain("S2").c_str()));
    EXPECT_EQ(XPE_OK, offsetState()) << "the map loaded first must stay";
    EXPECT_EQ(XPE_ERR_CALIB_NOT_LOADED, gainState()) << "the refused map must not be installed";
}

TEST_F(SessionConsistency, ConflictingOffsetIsRefusedAndTheGainStays) {
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gain("S2").c_str()));
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_load_offset(off("S1").c_str()));
    EXPECT_EQ(XPE_OK, gainState());
    EXPECT_EQ(XPE_ERR_CALIB_NOT_LOADED, offsetState());
}

TEST_F(SessionConsistency, ConflictingDefectMapIsRefused) {
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(off("S1").c_str()));
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_load_defect_map(def("S2").c_str()));
    // A refusal does not poison the store: the matching file loads right after.
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_map(def("S1", "defect_s1.xcal").c_str()));
}

TEST_F(SessionConsistency, DefectMapSessionIsComparedAgainstOffsetAndGain) {
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(def("S1").c_str()));
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_load_offset(off("S2").c_str()));
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_load_gain(gain("S2").c_str()));
}

TEST_F(SessionConsistency, ReloadingTheOnlyLoadedMapWithAnotherSessionIsAccepted) {
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(off("S1").c_str()));
    EXPECT_EQ(XPE_OK, xpe_calib_load_offset(off("S2", "offset_s2.xcal").c_str()))
        << "there is no other map to disagree with";
}

// The consequence of "the later one is refused", pinned so it is a decision and not a surprise: once two maps
// of session S1 are loaded, a new offset of S2 cannot replace the S1 offset until the set is cleared.
TEST_F(SessionConsistency, SwitchingSessionNeedsTheStoreToBeCleared) {
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(off("S1").c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gain("S1").c_str()));
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_load_offset(off("S2", "offset_s2.xcal").c_str()));
    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    EXPECT_EQ(XPE_OK, xpe_calib_load_offset(off("S2", "offset_s2.xcal").c_str()));
    EXPECT_EQ(XPE_OK, xpe_calib_load_gain(gain("S2", "gain_s2.xcal").c_str()));
}

TEST_F(SessionConsistency, UnspecifiedSessionIsLeftOutAndWarnedOnce) {
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(off("S1").c_str()));
    EXPECT_EQ(0, warningCount()) << "a single loaded map has nothing to be mixed with";
    // empty id: every file written before the check
    EXPECT_EQ(XPE_OK, xpe_calib_load_gain(gain(nullptr).c_str()));
    EXPECT_EQ(1, warningCount());
}

TEST_F(SessionConsistency, TheGeneratorsLiteralCountsAsUnspecified) {
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(off("S1").c_str()));
    EXPECT_EQ(XPE_OK, xpe_calib_load_gain(gain("generated").c_str()));
    EXPECT_EQ(1, warningCount());
    // and it never conflicts, whatever the specified one says
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_map(def("generated").c_str()));
}

TEST_F(SessionConsistency, UnspecifiedDoesNotHideAConflictBetweenTheSpecifiedOnes) {
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(off("S1").c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gain("").c_str()));
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_load_defect_map(def("S2").c_str()));
}

// One warning per mixed state: the pipeline reloads its files every call, and a warning per load fills the
// 64-entry alert queue with one repeated sentence (the Endurance loops showed it: 64 alerts).
TEST_F(SessionConsistency, TheWarningIsNotRepeatedWhileTheStateStaysMixed) {
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(off(nullptr).c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gain(nullptr).c_str()));
    EXPECT_EQ(1, warningCount());
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(def(nullptr).c_str()));
    for (int i = 0; i < 20; ++i) {
        ASSERT_EQ(XPE_OK, xpe_calib_load_offset(off(nullptr).c_str()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gain(nullptr).c_str()));
    }
    EXPECT_EQ(1, warningCount()) << "22 loads in one mixed state raised one warning";
}

TEST_F(SessionConsistency, TheWarningReturnsAfterTheStateWasNotMixed) {
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(off("S1").c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gain(nullptr).c_str()));
    EXPECT_EQ(1, warningCount());
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gain("S1", "gain_s1.xcal").c_str()));   // both specified, same: not mixed
    EXPECT_EQ(0, warningCount());
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gain(nullptr, "gain_none.xcal").c_str()));  // mixed again
    EXPECT_EQ(1, warningCount());
}

TEST_F(SessionConsistency, TheWarningStateEndsAtShutdown) {
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(off(nullptr).c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gain(nullptr).c_str()));
    EXPECT_EQ(1, warningCount());
    xpe_clear_alerts();
    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(off(nullptr).c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gain(nullptr).c_str()));
    EXPECT_EQ(1, warningCount()) << "a new session of use warns again";
}

// A cache hit must give the verdict a miss gives. xpe_preprocess_shutdown clears the cache too, so the store is
// changed WITHOUT shutdown: the S1 file is cached, then another map of the store moves to session S2 through the
// plain loader (which leaves the cache alone), and the next cached call for the S1 file is a genuine hit.
TEST_F(SessionConsistency, CachedOffsetHitRefusesLikeTheMiss) {
    const std::string offS1 = off("S1");
    XpeImageBuffer view{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(offS1.c_str(), &view));                    // miss: cached
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(off("S2", "offset_s2.xcal").c_str()));            // store: offset S2
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gain("S2").c_str()));                               // store: gain S2

    const XpeErrorCode plain = xpe_calib_load_offset(offS1.c_str());                         // the miss path's check
    const XpeErrorCode hit = xpe_calib_load_offset_cached(offS1.c_str(), &view);             // a hit
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, plain);
    EXPECT_EQ(plain, hit) << "a cache hit must not skip the session check";
    EXPECT_EQ(XPE_OK, offsetState()) << "the offset of session S2 is still the one installed";
}

TEST_F(SessionConsistency, CachedGainHitRefusesLikeTheMiss) {
    const std::string gainS1 = gain("S1");
    XpeImageBuffer view{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached(gainS1.c_str(), &view));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gain("S2", "gain_s2.xcal").c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(off("S2").c_str()));

    const XpeErrorCode plain = xpe_calib_load_gain(gainS1.c_str());
    const XpeErrorCode hit = xpe_calib_load_gain_cached(gainS1.c_str(), &view);
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, plain);
    EXPECT_EQ(plain, hit) << "a cache hit must not skip the session check";
    EXPECT_EQ(XPE_OK, gainState());
}

TEST_F(SessionConsistency, CachedDefectHitRefusesLikeTheMiss) {
    const std::string defS1 = def("S1");
    XpeImageBuffer view{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_cached(defS1.c_str(), &view));
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(def("S2", "defect_s2.xcal").c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(off("S2").c_str()));

    const XpeErrorCode plain = xpe_calib_load_defect_map(defS1.c_str());
    const XpeErrorCode hit = xpe_calib_load_defect_cached(defS1.c_str(), &view);
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, plain);
    EXPECT_EQ(plain, hit) << "a cache hit must not skip the session check";
}

// And a hit that agrees is accepted -- the check is a comparison, not a refusal of every hit.
TEST_F(SessionConsistency, CachedHitOfTheSameSessionIsAccepted) {
    const std::string offS1 = off("S1");
    XpeImageBuffer view{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(offS1.c_str(), &view));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gain("S1").c_str()));
    EXPECT_EQ(XPE_OK, xpe_calib_load_offset_cached(offS1.c_str(), &view));
}

// The pipeline loads the three files of a directory as one set: a disagreement among them refuses the set.
TEST_F(SessionConsistency, PipelineSetWithConflictingSessionsIsRefused) {
    off("S1");
    gain("S2");
    def("S1");
    std::vector<uint16_t> px(W * H, 500);
    XpeImageBuffer img{};
    img.width = W; img.height = H; img.format = XPE_PIXEL_UINT16;
    img.bitsAllocated = img.bitsStored = 16;
    img.data = px.data(); img.dataSize = px.size() * 2;
    XpeImageMetadata meta{};
    meta.pixelPitch_mm = 0.14f;
    const std::vector<uint16_t> before = px;
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID,
              xpe_preprocess_pipeline(&img, &meta, dir.string().c_str(), nullptr, nullptr));
    EXPECT_EQ(before, px) << "a refused set leaves the frame unprocessed";
    EXPECT_EQ(XPE_ERR_CALIB_NOT_LOADED, offsetState()) << "and installs none of the three maps";
}
