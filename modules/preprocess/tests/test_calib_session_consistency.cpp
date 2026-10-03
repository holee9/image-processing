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
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
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

// =============================================================================================================
// QA-A-229b (Codex #93): what a cache hit judges from, and the format of the session field.
// =============================================================================================================

namespace {
namespace fs = std::filesystem;

constexpr std::streamoff kExpiryOffset = static_cast<std::streamoff>(offsetof(XCalFileHeader, expiry_epoch_ms));
constexpr std::streamoff kSessionOffset = static_cast<std::streamoff>(offsetof(XCalFileHeader, session_id));

// The three map kinds, through one set of calls.
enum Kind { kOffset = 0, kGain = 1, kDefect = 2 };
const char* kindName(int k) { return k == kOffset ? "offset" : k == kGain ? "gain" : "defect"; }
XpeErrorCode plainLoad(int k, const std::string& p) {
    return k == kOffset ? xpe_calib_load_offset(p.c_str())
         : k == kGain   ? xpe_calib_load_gain(p.c_str())
                        : xpe_calib_load_defect_map(p.c_str());
}
XpeErrorCode cachedLoad(int k, const std::string& p) {
    XpeImageBuffer v{};
    return k == kOffset ? xpe_calib_load_offset_cached(p.c_str(), &v)
         : k == kGain   ? xpe_calib_load_gain_cached(p.c_str(), &v)
                        : xpe_calib_load_defect_cached(p.c_str(), &v);
}

// Overwrite bytes of the file in place, then put the write time back: size and write time are what they were.
void patchBytes(const std::string& path, std::streamoff at, const void* bytes, size_t n) {
    const auto original = fs::last_write_time(path);
    {
        std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
        ASSERT_TRUE(f.good()) << path;
        f.seekp(at);
        f.write(static_cast<const char*>(bytes), static_cast<std::streamsize>(n));
        ASSERT_TRUE(f.good());
    }
    fs::last_write_time(path, original);
}
void patchSession(const std::string& path, const void* raw64) { patchBytes(path, kSessionOffset, raw64, 64); }
void patchSessionText(const std::string& path, const char* text) {
    char field[64] = {};
    std::memcpy(field, text, std::strlen(text));
    patchSession(path, field);
}
}  // namespace

class HeaderOnHit : public SessionConsistency {
protected:
    std::string make(int k, const char* session, const char* name) {
        return k == kOffset ? off(session, name) : k == kGain ? gain(session, name) : def(session, name);
    }
};

// ---- finding 1: a hit takes what it judges from the CURRENT file's header ---------------------------------

// The header is outside the SHA-256, so an edit of the session field that keeps size and write time is invisible
// to the stamp. Before: the plain loader said CONFIG_INVALID and the cached hit said OK from the entry's old id.
TEST_F(HeaderOnHit, ACachedHitJudgesTheSessionOfTheFileAsItStands) {
    for (int k = 0; k < 3; ++k) {
        SCOPED_TRACE(kindName(k));
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        xpe_clear_alerts();
        const int other = (k + 1) % 3;
        ASSERT_EQ(XPE_OK, plainLoad(other, make(other, "A1", "other.xcal")));      // the store holds a map of session A1
        const std::string f = make(k, "A1", "target.xcal");
        ASSERT_EQ(XPE_OK, cachedLoad(k, f));                                         // miss: cached, installed

        char changed[64] = {};
        std::memcpy(changed, "B1", 2);
        patchSession(f, changed);                                                    // same length, same size, same write time

        const XpeErrorCode plain = plainLoad(k, f);
        const XpeErrorCode hit = cachedLoad(k, f);
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, plain) << "the file now says B1 against a loaded A1 map";
        EXPECT_EQ(plain, hit) << "a cache hit must judge the session of the file as it is now (OK = the entry's old id)";
    }
}

TEST_F(HeaderOnHit, ACachedHitJudgesTheExpiryOfTheFileAsItStands) {
    for (int k = 0; k < 3; ++k) {
        SCOPED_TRACE(kindName(k));
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        xpe_clear_alerts();
        const std::string f = make(k, "A1", "target.xcal");
        ASSERT_EQ(XPE_OK, cachedLoad(k, f));

        const int64_t past = 1;                                                      // 1 ms after the epoch
        patchBytes(f, kExpiryOffset, &past, sizeof(past));

        const XpeErrorCode plain = plainLoad(k, f);
        const XpeErrorCode hit = cachedLoad(k, f);
        EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED, plain);
        EXPECT_EQ(plain, hit) << "a cache hit must judge the expiry written in the file now";
    }
}

// The same for the fields that make the file a different file.
TEST_F(HeaderOnHit, ACachedHitRefusesAFileWhoseTypeWasEditedLikeTheMissDoes) {
    for (int k = 0; k < 3; ++k) {
        SCOPED_TRACE(kindName(k));
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        xpe_clear_alerts();
        const std::string f = make(k, "A1", "target.xcal");
        ASSERT_EQ(XPE_OK, cachedLoad(k, f));
        const uint32_t otherType = static_cast<uint32_t>(k == kDefect ? XCAL_TYPE_OFFSET : XCAL_TYPE_DEFECT);
        patchBytes(f, static_cast<std::streamoff>(offsetof(XCalFileHeader, type)), &otherType, sizeof(otherType));
        const XpeErrorCode plain = plainLoad(k, f);
        const XpeErrorCode hit = cachedLoad(k, f);
        EXPECT_NE(XPE_OK, plain);
        EXPECT_EQ(plain, hit);
    }
}

// An unchanged header still hits: the pointer the cache hands back is the one it handed the first time.
TEST_F(HeaderOnHit, AnUnchangedHeaderStillHits) {
    const std::string f = off("A1", "target.xcal");
    XpeImageBuffer first{}, second{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(f.c_str(), &first));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(f.c_str(), &second));
    EXPECT_EQ(first.data, second.data) << "the second call must be a hit (the cache-owned view), not a reload";
}

// ---- finding 3: the format of the 64-byte session field ---------------------------------------------------

class SessionField : public SessionConsistency {
protected:
    std::string make(int k, const char* session, const char* name) {
        return k == kOffset ? off(session, name) : k == kGain ? gain(session, name) : def(session, name);
    }
    // A file of kind k whose session field was then overwritten with `raw64`; the store is empty.
    XpeErrorCode loadWithField(int k, const char (&raw64)[64], bool cached) {
        xpe_preprocess_shutdown();
        EXPECT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        xpe_calib_cache_clear();
        const std::string f = make(k, "A1", "field.xcal");
        patchSession(f, raw64);
        const XpeErrorCode rc = cached ? cachedLoad(k, f) : plainLoad(k, f);
        xpe_clear_alerts();
        return rc;
    }
};

TEST_F(SessionField, SixtyFourBytesWithoutATerminatorAreRefused) {
    char field[64];
    std::memset(field, 'A', sizeof(field));
    for (int k = 0; k < 3; ++k) {
        SCOPED_TRACE(kindName(k));
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, loadWithField(k, field, false));
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, loadWithField(k, field, true));
    }
}

// Two ids of 64 bytes that share their first 63 and differ in the last: the loaders copied 63 bytes, so they
// compared equal. They are refused as a format error, whichever the last byte is.
TEST_F(SessionField, TwoSixtyFourByteIdsThatDifferOnlyInTheLastByteAreBothRefused) {
    char a[64], b[64];
    std::memset(a, 'A', sizeof(a));
    std::memset(b, 'A', sizeof(b));
    a[63] = 'x';
    b[63] = 'y';
    for (int k = 0; k < 3; ++k) {
        SCOPED_TRACE(kindName(k));
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, loadWithField(k, a, false));
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, loadWithField(k, b, false));
    }
}

// The positive control: 63-byte ids (a terminator in byte 64) that differ only in their last character are
// valid and DIFFERENT -- the comparison reads all 63.
TEST_F(SessionField, TwoSixtyThreeByteIdsThatDifferOnlyInTheLastCharacterAreDifferent) {
    char a[64] = {}, b[64] = {};
    std::memset(a, 'A', 63);
    std::memset(b, 'A', 63);
    a[62] = 'x';
    b[62] = 'y';
    for (int k = 0; k < 3; ++k) {
        SCOPED_TRACE(kindName(k));
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        const int other = (k + 1) % 3;
        const std::string first = make(other, "A1", "first.xcal");
        patchSession(first, a);
        ASSERT_EQ(XPE_OK, plainLoad(other, first)) << "a 63-byte id alone is valid";
        const std::string second = make(k, "A1", "second.xcal");
        patchSession(second, b);
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, plainLoad(k, second)) << "and different from the other map's";
        const std::string same = make(k, "A1", "same.xcal");
        patchSession(same, a);
        EXPECT_EQ(XPE_OK, plainLoad(k, same)) << "the same 63 bytes agree";
    }
}

TEST_F(SessionField, BytesAfterTheTerminatorAreRefused) {
    char field[64] = {};
    field[0] = 'A';
    field[1] = 0;
    field[2] = 'B';   // not zero-padded
    for (int k = 0; k < 3; ++k) {
        SCOPED_TRACE(kindName(k));
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, loadWithField(k, field, false));
    }
}

TEST_F(SessionField, TextThatIsNotWellFormedUtf8IsRefused) {
    const std::vector<std::vector<unsigned char>> bad = {
        {0xC0, 0x80},                 // overlong NUL
        {0xFF},                       // never valid
        {0x80},                       // continuation byte on its own
        {0xED, 0xA0, 0x80},           // a surrogate
        {0xE4, 0xB8},                 // truncated three-byte sequence
        {0xF4, 0x90, 0x80, 0x80},     // above U+10FFFF
    };
    for (size_t i = 0; i < bad.size(); ++i) {
        char field[64] = {};
        std::memcpy(field, bad[i].data(), bad[i].size());
        for (int k = 0; k < 3; ++k) {
            SCOPED_TRACE(std::string(kindName(k)) + " sequence " + std::to_string(i));
            EXPECT_EQ(XPE_ERR_CONFIG_INVALID, loadWithField(k, field, false));
        }
    }
}

TEST_F(SessionField, WellFormedUtf8IsAcceptedAndCompared) {
    const char* korean = "\xEC\x84\xB8\xEC\x85\x98-1";   // "세션-1"
    const char* other = "\xEC\x84\xB8\xEC\x85\x98-2";    // "세션-2"
    char a[64] = {}, b[64] = {};
    std::memcpy(a, korean, std::strlen(korean));
    std::memcpy(b, other, std::strlen(other));
    for (int k = 0; k < 3; ++k) {
        SCOPED_TRACE(kindName(k));
        EXPECT_EQ(XPE_OK, loadWithField(k, a, false));
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        const int o = (k + 1) % 3;
        const std::string first = make(o, "A1", "first.xcal");
        patchSession(first, a);
        ASSERT_EQ(XPE_OK, plainLoad(o, first));
        const std::string second = make(k, "A1", "second.xcal");
        patchSession(second, b);
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, plainLoad(k, second)) << "two well-formed but different ids conflict";
    }
}

TEST_F(SessionField, AnEmptyFieldAndTheGeneratorsLiteralStillLoad) {
    char empty[64] = {};
    char generated[64] = {};
    std::memcpy(generated, "generated", 9);
    for (int k = 0; k < 3; ++k) {
        SCOPED_TRACE(kindName(k));
        EXPECT_EQ(XPE_OK, loadWithField(k, empty, false));
        EXPECT_EQ(XPE_OK, loadWithField(k, generated, false));
    }
}
