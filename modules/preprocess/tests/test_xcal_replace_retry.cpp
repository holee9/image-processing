/**
 * @file test_xcal_replace_retry.cpp
 * @brief Writing an XCal file over an existing one when another process has the destination open (QA-A-212b, #233).
 *
 * QA-A-212 found why write_xcal_file failed now and then with XPE_ERR_IO_FAILED: the closing MoveFileEx answered
 * ERROR_ACCESS_DENIED (5) because a scanner / indexer / watcher had the destination open without FILE_SHARE_DELETE
 * for an instant. The writer now repeats the move for up to ~100 ms when the error is 5 or 32, and a move that
 * fails for good raises an alert naming the Windows error and the number of retries.
 *
 * The "other process" is a thread of this test that opens the destination for READ without FILE_SHARE_DELETE and
 * holds it for a stated time -- the mechanism QA-A-212 reproduced with a separate program (1565 of 3000 replaces
 * failed with 5), made deterministic: the hold is long enough to fail a writer that does not retry and short enough
 * for one that does.
 *   - cleared within the budget -> the write succeeds, the file is the new one, nothing is raised
 *   - held past the budget -> XPE_ERR_IO_FAILED, the old file intact, no temp file left, one alert with the error
 *   - read-only destination (ACCESS_DENIED that never clears) -> the same, and the time stays bounded
 *   - the public xpe_calib_save goes the same way (all file writers pass write_xcal_file)
 * Time assertions are UPPER bounds only, generous for a loaded CI machine.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess/xcal_format.h"
#include "fixtures/make_xcal.hpp"
#include "preprocess_state_fixture.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace fs = std::filesystem;

#ifndef _WIN32
TEST(XcalReplaceRetry, OnlyWindowsRetriesTheMove) {
    GTEST_SKIP() << "std::rename replaces atomically on POSIX and does not meet this; the Windows path is the one under test";
}
#else

namespace {

constexpr uint32_t W = 8, H = 8;

std::string findAlert(const std::string& prefix) {
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < n; ++i) {
        char buf[2048];
        int32_t sev = 0;
        if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) == XPE_OK && std::string(buf).rfind(prefix, 0) == 0) return buf;
    }
    return {};
}

float firstGain(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    XCalFileHeader hdr{};
    f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    f.seekg(static_cast<std::streamoff>(sizeof(hdr) + hdr.config_json_len));
    float v = -1.0f;
    f.read(reinterpret_cast<char*>(&v), sizeof(v));
    return v;
}

/** Another "process": opens the file for read WITHOUT FILE_SHARE_DELETE and keeps it for `holdMs`. */
class Holder {
public:
    Holder(const std::string& path, unsigned holdMs) {
        t_ = std::thread([this, path, holdMs] {
            HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
            opened_ = (h != INVALID_HANDLE_VALUE);
            held_ = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(holdMs));
            if (opened_) CloseHandle(h);
        });
        while (!held_) std::this_thread::yield();
    }
    ~Holder() { if (t_.joinable()) t_.join(); }
    bool opened() const { return opened_; }
    void release() { if (t_.joinable()) t_.join(); }
private:
    std::atomic<bool> held_{false};
    std::atomic<bool> opened_{false};
    std::thread t_;
};

/** QA-A-212c: another "process" holding the TEMPORARY file (created if absent) without FILE_SHARE_DELETE until release(). */
class TempHolder {
public:
    /** `share` is what the holder lets others do; `lockRange` also takes a byte-range lock on the first MiB (writes by any
     *  other handle into it fail with ERROR_LOCK_VIOLATION -- a real operating-system write failure, QA-A-212d). */
    explicit TempHolder(const std::string& path, DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE, bool lockRange = false) {
        h_ = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, share, nullptr, OPEN_ALWAYS, 0, nullptr);
        if (lockRange && h_ != INVALID_HANDLE_VALUE) locked_ = (LockFile(h_, 0, 0, 1u << 20, 0) != 0);
    }
    ~TempHolder() { release(); }
    bool opened() const { return h_ != INVALID_HANDLE_VALUE; }
    bool locked() const { return locked_; }
    void release() { if (h_ != INVALID_HANDLE_VALUE) { CloseHandle(h_); h_ = INVALID_HANDLE_VALUE; } }
private:
    HANDLE h_ = INVALID_HANDLE_VALUE;
    bool locked_ = false;
};

class XcalReplaceRetryTest : public XpePreprocessStateFixture {
protected:
    void SetUp() override {
        XpePreprocessStateFixture::SetUp();
        dir_ = fs::temp_directory_path() / ("xpe_xrr_" + std::to_string(counter_++));
        fs::remove_all(dir_);
        fs::create_directories(dir_);
        xpe_clear_alerts();
    }
    void TearDown() override {
        xpe_clear_alerts();
        std::error_code ec;
        for (const auto& e : fs::directory_iterator(dir_, ec)) SetFileAttributesA(e.path().string().c_str(), FILE_ATTRIBUTE_NORMAL);
        fs::remove_all(dir_, ec);
        XpePreprocessStateFixture::TearDown();
    }
    std::string p(const std::string& n) const { return (dir_ / n).string(); }
    std::string dest() const { return p("cal.xcal"); }
    std::string tmp() const { return p("cal.xcal.tmp"); }
    int64_t elapsedMs(const std::chrono::steady_clock::time_point& t0) const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    }

    fs::path dir_;
    static inline int counter_ = 0;
};

}  // namespace

TEST_F(XcalReplaceRetryTest, ControlTheHoldReallyMakesAPlainMoveFailWithAccessDenied) {
    ASSERT_EQ(XPE_OK, MakeGainXCal(dest().c_str(), W, H, 2.0f));
    ASSERT_EQ(XPE_OK, MakeGainXCal(p("other.xcal").c_str(), W, H, 5.0f));
    Holder hold(dest(), 200);
    ASSERT_TRUE(hold.opened());
    const BOOL ok = MoveFileExA(p("other.xcal").c_str(), dest().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    EXPECT_FALSE(ok) << "control: a destination held without FILE_SHARE_DELETE refuses the move";
    EXPECT_EQ(static_cast<DWORD>(ERROR_ACCESS_DENIED), GetLastError()) << "and with the error QA-A-212 saw in the field";
}

TEST_F(XcalReplaceRetryTest, ADestinationBusyForAWhileWithinTheBudgetIsReplacedAndNothingIsRaised) {
    ASSERT_EQ(XPE_OK, MakeGainXCal(dest().c_str(), W, H, 2.0f));
    xpe_clear_alerts();
    {
        Holder hold(dest(), 30);   // well inside the ~100 ms budget, long enough that a first attempt cannot succeed
        ASSERT_TRUE(hold.opened());
        EXPECT_EQ(XPE_OK, MakeGainXCal(dest().c_str(), W, H, 5.0f)) << "the move was repeated until the destination was free";
    }
    EXPECT_FLOAT_EQ(5.0f, firstGain(dest())) << "the file is the new one";
    EXPECT_FALSE(fs::exists(tmp())) << "no temporary file is left";
    EXPECT_EQ(0, xpe_get_pending_alert_count()) << "a write that succeeds after retrying raises nothing";
}

TEST_F(XcalReplaceRetryTest, ADestinationHeldPastTheBudgetFailsWithTheReasonAndTheOldFileIntact) {
    ASSERT_EQ(XPE_OK, MakeGainXCal(dest().c_str(), W, H, 2.0f));
    xpe_clear_alerts();
    XpeErrorCode rc = XPE_OK;
    int64_t took = 0;
    {
        Holder hold(dest(), 700);   // far past the budget
        ASSERT_TRUE(hold.opened());
        const auto t0 = std::chrono::steady_clock::now();
        rc = MakeGainXCal(dest().c_str(), W, H, 5.0f);
        took = elapsedMs(t0);
        EXPECT_FALSE(fs::exists(tmp())) << "the temporary file is removed even though the move failed";
    }
    EXPECT_EQ(XPE_ERR_IO_FAILED, rc);
    EXPECT_LT(took, 5000) << "the retrying is bounded (upper bound only; the budget is ~100 ms)";
    EXPECT_FLOAT_EQ(2.0f, firstGain(dest())) << "the previous file is untouched";

    const std::string a = findAlert("XPE_WARN_XCAL_REPLACE_FAILED:");
    ASSERT_FALSE(a.empty()) << "a failure that was retried to the end says why";
    EXPECT_NE(std::string::npos, a.find("Windows error 5")) << a;
    EXPECT_NE(std::string::npos, a.find("cal.xcal")) << a;
    const size_t at = a.find(" after ");
    ASSERT_NE(std::string::npos, at) << a;
    EXPECT_GT(std::atoi(a.c_str() + at + 7), 0) << "the number of retries is in it and the move was retried: " << a;
    EXPECT_NE(std::string::npos, a.find("the temporary file was removed")) << "QA-A-212c: it was removed, and the alert says so: " << a;
    EXPECT_EQ(std::string::npos, a.find("could NOT be removed")) << a;
}

TEST_F(XcalReplaceRetryTest, AReadOnlyDestinationFailsAfterTheBudgetNotForever) {
    ASSERT_EQ(XPE_OK, MakeGainXCal(dest().c_str(), W, H, 2.0f));
    ASSERT_TRUE(SetFileAttributesA(dest().c_str(), FILE_ATTRIBUTE_READONLY));
    xpe_clear_alerts();
    const auto t0 = std::chrono::steady_clock::now();
    const XpeErrorCode rc = MakeGainXCal(dest().c_str(), W, H, 5.0f);
    const int64_t took = elapsedMs(t0);
    EXPECT_EQ(XPE_ERR_IO_FAILED, rc) << "ACCESS_DENIED that never clears ends in the failure it always was";
    EXPECT_LT(took, 5000) << "after the bounded retrying";
    EXPECT_FALSE(fs::exists(tmp())) << "the temporary file is removed";
    SetFileAttributesA(dest().c_str(), FILE_ATTRIBUTE_NORMAL);
    EXPECT_FLOAT_EQ(2.0f, firstGain(dest())) << "the previous file is untouched";
    const std::string a = findAlert("XPE_WARN_XCAL_REPLACE_FAILED:");
    ASSERT_FALSE(a.empty());
    EXPECT_NE(std::string::npos, a.find("Windows error 5")) << a;
}

// QA-A-212c (Codex #74): the 32 that fails the move can be the TEMPORARY file being open elsewhere. That handle forbids the
// delete too, so the temp file stays -- and the alert used to say "the temporary file was removed" regardless.
TEST_F(XcalReplaceRetryTest, ATemporaryFileHeldElsewhereIsReportedAsLeftBehindAndTheNextSaveStillWorks) {
    ASSERT_EQ(XPE_OK, MakeGainXCal(dest().c_str(), W, H, 2.0f));
    xpe_clear_alerts();
    XpeErrorCode rc = XPE_OK;
    {
        TempHolder hold(tmp());
        ASSERT_TRUE(hold.opened());
        rc = MakeGainXCal(dest().c_str(), W, H, 5.0f);
        EXPECT_TRUE(fs::exists(tmp())) << "control: the held temporary file could not be deleted";
        EXPECT_EQ(XPE_ERR_IO_FAILED, rc);
        EXPECT_FLOAT_EQ(2.0f, firstGain(dest())) << "the previous file is untouched";

        const std::string a = findAlert("XPE_WARN_XCAL_REPLACE_FAILED:");
        ASSERT_FALSE(a.empty());
        EXPECT_NE(std::string::npos, a.find("could NOT be removed")) << "the alert says the file is still there: " << a;
        EXPECT_NE(std::string::npos, a.find("cal.xcal.tmp")) << "and names it: " << a;
        EXPECT_EQ(std::string::npos, a.find("the temporary file was removed")) << "and does not claim the opposite: " << a;
        EXPECT_NE(std::string::npos, a.find("was left behind")) << a;
        EXPECT_NE(std::string::npos, a.find("could NOT be removed (Windows error 32)")) << "the error of the failed delete is the sharing violation: " << a;
        EXPECT_NE(std::string::npos, a.find("the destination or the temporary file stayed open")) << "and the cause names the temporary file too: " << a;
    }
    // the holder let go: the leftover does not stand in the way -- the next save opens the same name with trunc
    xpe_clear_alerts();
    EXPECT_EQ(XPE_OK, MakeGainXCal(dest().c_str(), W, H, 7.0f)) << "a leftover temporary file does not block the next save";
    EXPECT_FLOAT_EQ(7.0f, firstGain(dest())) << "and the file is the new one";
    EXPECT_FALSE(fs::exists(tmp())) << "the leftover was consumed by the move";
    EXPECT_EQ(0, xpe_get_pending_alert_count());
}

TEST_F(XcalReplaceRetryTest, AStaleTemporaryFileFromEarlierIsOverwrittenBySave) {
    // a leftover that nobody holds (an earlier failed save, a crash): it is overwritten, not an obstacle
    ASSERT_EQ(XPE_OK, MakeGainXCal(dest().c_str(), W, H, 2.0f));
    { std::ofstream f(tmp(), std::ios::binary); f << "stale bytes that are not an xcal file"; }
    ASSERT_TRUE(fs::exists(tmp()));
    xpe_clear_alerts();
    EXPECT_EQ(XPE_OK, MakeGainXCal(dest().c_str(), W, H, 9.0f));
    EXPECT_FLOAT_EQ(9.0f, firstGain(dest()));
    EXPECT_FALSE(fs::exists(tmp()));
    EXPECT_EQ(0, xpe_get_pending_alert_count());
}

// ---------------------------------------------------------------------------------------------------------------------
// QA-A-212d (#233): the two steps BEFORE the move are no longer silent.
//   - the temporary file cannot be created  -> XPE_WARN_XCAL_TEMP_OPEN_FAILED
//   - writing the temporary file fails      -> XPE_WARN_XCAL_TEMP_WRITE_FAILED, and the half-written .tmp is removed
//     (or reported as left behind, as in 212c)
// A write failure is made by the operating system, not by a test hook in the product: another handle holds a byte-range
// lock on the first MiB of the temporary file, so WriteFile by the writer fails with ERROR_LOCK_VIOLATION. (A small
// volume would need administrator rights; a hook would test the hook.)
// ---------------------------------------------------------------------------------------------------------------------
TEST_F(XcalReplaceRetryTest, ATemporaryFileThatCannotBeCreatedIsReportedWithItsReason) {
    const std::string badDest = p("no_such_dir") + "\\cal.xcal";
    const XpeErrorCode rc = MakeGainXCal(badDest.c_str(), W, H, 5.0f);
    EXPECT_EQ(XPE_ERR_IO_FAILED, rc);
    const std::string a = findAlert("XPE_WARN_XCAL_TEMP_OPEN_FAILED:");
    ASSERT_FALSE(a.empty()) << "a failure that used to be silent now says why";
    EXPECT_NE(std::string::npos, a.find("no_such_dir")) << "it names the path: " << a;
    EXPECT_NE(std::string::npos, a.find("cal.xcal.tmp")) << "and the temporary file: " << a;
    EXPECT_NE(std::string::npos, a.find("No such file or directory")) << "and the reason the system gave: " << a;
    EXPECT_NE(std::string::npos, a.find("Nothing was written")) << a;
    EXPECT_TRUE(findAlert("XPE_WARN_XCAL_REPLACE_FAILED:").empty()) << "the move was never reached";
    EXPECT_TRUE(findAlert("XPE_WARN_XCAL_TEMP_WRITE_FAILED:").empty());
    EXPECT_FALSE(fs::exists(fs::path(p("no_such_dir")))) << "no directory was made";
}

TEST_F(XcalReplaceRetryTest, ATemporaryFileHeldExclusivelyCannotBeOpenedAndTheNextSaveWorksOnceItIsFree) {
    ASSERT_EQ(XPE_OK, MakeGainXCal(dest().c_str(), W, H, 2.0f));
    xpe_clear_alerts();
    {
        TempHolder hold(tmp(), 0 /* share nothing */);
        ASSERT_TRUE(hold.opened());
        EXPECT_EQ(XPE_ERR_IO_FAILED, MakeGainXCal(dest().c_str(), W, H, 5.0f));
        const std::string a = findAlert("XPE_WARN_XCAL_TEMP_OPEN_FAILED:");
        ASSERT_FALSE(a.empty()) << "an exclusively held temporary file is reported, not swallowed";
        EXPECT_NE(std::string::npos, a.find("cal.xcal.tmp")) << a;
        EXPECT_FLOAT_EQ(2.0f, firstGain(dest())) << "the previous file is untouched";
    }
    xpe_clear_alerts();
    EXPECT_EQ(XPE_OK, MakeGainXCal(dest().c_str(), W, H, 7.0f)) << "and the leftover does not block the next save";
    EXPECT_FLOAT_EQ(7.0f, firstGain(dest()));
    EXPECT_EQ(0, xpe_get_pending_alert_count());
}

TEST_F(XcalReplaceRetryTest, AWriteThatFailsHalfWayRemovesTheTemporaryFileAndSaysWhy) {
    ASSERT_EQ(XPE_OK, MakeGainXCal(dest().c_str(), W, H, 2.0f));
    xpe_clear_alerts();
    XpeErrorCode rc = XPE_OK;
    {
        // the holder allows deleting (FILE_SHARE_DELETE) but locks the first MiB: writing fails, deleting does not
        TempHolder hold(tmp(), FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, true);
        ASSERT_TRUE(hold.opened());
        ASSERT_TRUE(hold.locked());
        rc = MakeGainXCal(dest().c_str(), W, H, 5.0f);
        EXPECT_EQ(XPE_ERR_IO_FAILED, rc);
        const std::string a = findAlert("XPE_WARN_XCAL_TEMP_WRITE_FAILED:");
        ASSERT_FALSE(a.empty()) << "a failed write used to return IO_FAILED with nothing to say why";
        EXPECT_NE(std::string::npos, a.find("cal.xcal.tmp")) << a;
        EXPECT_NE(std::string::npos, a.find("Windows error 33")) << "the system said: the file is locked by another process: " << a;
        EXPECT_NE(std::string::npos, a.find("the temporary file was removed")) << "and it was removed: " << a;
        EXPECT_EQ(std::string::npos, a.find("could NOT be removed")) << a;
        EXPECT_TRUE(findAlert("XPE_WARN_XCAL_REPLACE_FAILED:").empty()) << "the move was never reached";
        EXPECT_FLOAT_EQ(2.0f, firstGain(dest())) << "the previous file is untouched";
    }
    EXPECT_FALSE(fs::exists(tmp())) << "no half-written temporary file stays behind";
    xpe_clear_alerts();
    EXPECT_EQ(XPE_OK, MakeGainXCal(dest().c_str(), W, H, 7.0f)) << "the next save works";
    EXPECT_FLOAT_EQ(7.0f, firstGain(dest()));
    EXPECT_EQ(0, xpe_get_pending_alert_count());
}

TEST_F(XcalReplaceRetryTest, AWriteThatFailsWhileTheTemporaryFileCannotBeDeletedSaysItWasLeftBehind) {
    ASSERT_EQ(XPE_OK, MakeGainXCal(dest().c_str(), W, H, 2.0f));
    xpe_clear_alerts();
    {
        TempHolder hold(tmp(), FILE_SHARE_READ | FILE_SHARE_WRITE, true);   // no FILE_SHARE_DELETE: it cannot be deleted either
        ASSERT_TRUE(hold.opened());
        ASSERT_TRUE(hold.locked());
        EXPECT_EQ(XPE_ERR_IO_FAILED, MakeGainXCal(dest().c_str(), W, H, 5.0f));
        EXPECT_TRUE(fs::exists(tmp())) << "control: the held file could not be deleted";
        const std::string a = findAlert("XPE_WARN_XCAL_TEMP_WRITE_FAILED:");
        ASSERT_FALSE(a.empty());
        EXPECT_NE(std::string::npos, a.find("could NOT be removed (Windows error 32)")) << a;
        EXPECT_NE(std::string::npos, a.find("was left behind")) << a;
        EXPECT_EQ(std::string::npos, a.find("the temporary file was removed")) << "and does not claim the opposite: " << a;
    }
    xpe_clear_alerts();
    EXPECT_EQ(XPE_OK, MakeGainXCal(dest().c_str(), W, H, 7.0f)) << "a leftover half-written file does not block the next save";
    EXPECT_FLOAT_EQ(7.0f, firstGain(dest()));
    EXPECT_FALSE(fs::exists(tmp())) << "the leftover was overwritten and consumed by the move";
    EXPECT_EQ(0, xpe_get_pending_alert_count());
}

TEST_F(XcalReplaceRetryTest, ThePublicSaveGoesTheSameWay) {
    // xpe_calib_save and every generator end in write_xcal_file; the public save is checked end to end.
    ASSERT_EQ(XPE_OK, MakeOffsetXCal(p("offset_in.xcal").c_str(), W, H, 100.0f));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(p("offset_in.xcal").c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_save(dest().c_str(), "offset", 0));
    xpe_clear_alerts();
    {
        Holder hold(dest(), 30);
        ASSERT_TRUE(hold.opened());
        EXPECT_EQ(XPE_OK, xpe_calib_save(dest().c_str(), "offset", 0)) << "a save onto a briefly busy destination succeeds";
    }
    EXPECT_EQ(0, xpe_get_pending_alert_count());
    {
        Holder hold(dest(), 700);
        ASSERT_TRUE(hold.opened());
        EXPECT_EQ(XPE_ERR_IO_FAILED, xpe_calib_save(dest().c_str(), "offset", 0)) << "and one onto a destination held past the budget fails";
        EXPECT_FALSE(fs::exists(tmp()));
    }
    EXPECT_FALSE(findAlert("XPE_WARN_XCAL_REPLACE_FAILED:").empty());
}

#endif  // _WIN32
