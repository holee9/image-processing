/**
 * @file test_common_oom_injection.cpp
 * @brief Allocation-failure sweeps over the xpe_common entry points that keep state (QA-A-204, #233).
 *
 * xpe_common is a shared library, and a replaced operator new in a test executable does not reach code that
 * lives in another module. This target therefore compiles the library sources into itself (the same
 * method as xpe_preprocess_oom_tests, QA-A-200) and fails exactly the K-th allocation of one call, K = 1, 2, ...,
 * until a call finishes with fewer than K allocations. Every K is held to the same three questions:
 *
 *   1. Did an exception leave the C ABI function?
 *   2. If the call returned an error, is the state as the call found it (or, for the alert queue, is every
 *      alert accounted for)?
 *   3. Is the error code the right one (an allocation failure is OUT_OF_MEMORY, never a code that
 *      names a different fault)?
 *
 * No gtest macro is evaluated while the allocator is armed: gtest allocates too.
 */

#include <gtest/gtest.h>

#include "xpe/common/xpe_common_api.h"
#include "xpe/common/xpe_error.h"

#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <new>
#include <string>
#include <vector>

/* The accessor xpe_common.cpp compiles in under XPE_COMMON_TEST_HOOKS: the exact number of alerts the
 * queue has counted as lost (the loss alert's text is only a report of it). */
uint64_t xpe_common_alerts_dropped_for_test();

/* The seam xpe_logging.cpp compiles in under XPE_COMMON_TEST_HOOKS (QA-A-202d). */
void xpe_log_adopt_logger_for_test(std::shared_ptr<spdlog::logger> logger);

/* =========================================================================
 * The injecting allocator
 * ========================================================================= */

namespace {
std::atomic<long> g_failAt{0};      // 0 = disarmed; otherwise the 1-based index of the allocation to fail
std::atomic<long> g_count{0};
std::atomic<bool> g_injected{false};

void arm(long k) {
    g_count.store(0);
    g_injected.store(false);
    g_failAt.store(k);
}
bool disarm() {
    g_failAt.store(0);
    return g_injected.load();
}
}  // namespace

void* operator new(std::size_t n) {
    const long limit = g_failAt.load(std::memory_order_relaxed);
    if (limit > 0 && g_count.fetch_add(1) + 1 == limit) {
        g_injected.store(true);
        throw std::bad_alloc();
    }
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {

using Call = std::function<XpeErrorCode()>;
using Setup = std::function<void()>;
using Check = std::function<std::string(XpeErrorCode rc, bool injected)>;

/** Fails the K-th allocation of `call` for K = 1.. until a call finishes with fewer allocations. */
void sweep(const char* label, const Setup& setup, const Call& call, const Check& check) {
    constexpr long kMax = 2000;
    long injections = 0;
    for (long k = 1; k <= kMax; ++k) {
        setup();
        bool escaped = false;
        XpeErrorCode rc = XPE_OK;
        arm(k);
        try {
            rc = call();
        } catch (...) {
            escaped = true;
        }
        const bool injected = disarm();

        ASSERT_FALSE(escaped) << label << ": an exception left the C ABI function when allocation #" << k << " failed";
        const std::string why = check(rc, injected);
        ASSERT_TRUE(why.empty()) << label << ": allocation #" << k << " failed, rc " << rc << ": " << why;
        if (!injected) {
            ASSERT_GT(injections, 0) << label << ": the sweep never reached a library allocation";
            std::printf("[sweep] %-44s %3ld allocation points covered\n", label, injections);
            return;
        }
        ++injections;
    }
    FAIL() << label << ": the sweep did not finish within " << kMax << " allocations";
}

/** Whether the library considers itself initialized, asked through a call that allocates nothing. */
bool initialized() {
    float a = 0, b = 0, c = 0;
    return xpe_get_param_range("CHEST", "gamma", &a, &b, &c) == XPE_OK;
}

constexpr const char* kLossPrefix = "alert queue overflow:";

struct QueueView {
    long nonLoss{0};
    long loss{0};
    uint64_t lossTextCount{0};
};
QueueView view() {
    QueueView v;
    for (int32_t i = 0;; ++i) {
        char buf[256];
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) != XPE_OK) break;
        if (sev == XPE_ALERT_ERROR && std::strncmp(buf, kLossPrefix, std::strlen(kLossPrefix)) == 0) {
            ++v.loss;
            v.lossTextCount = std::strtoull(buf + std::strlen(kLossPrefix), nullptr, 10);
        } else {
            ++v.nonLoss;
        }
    }
    return v;
}

// Long enough that copying it allocates (std::string keeps up to 15 characters inline).
std::string longMessage(int n) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "alert-%04d: padding so that this message does not fit in a small string", n);
    return buf;
}

const std::string kLongConfig =
    "{\"note\":\"padding so that this configuration does not fit in a small string: "
    "0123456789012345678901234567890123456789012345678901234567890123456789\"}";

class CommonOom : public ::testing::Test {
protected:
    void SetUp() override { xpe_shutdown(); xpe_clear_alerts(); }
    void TearDown() override {
        xpe_log_set_file(nullptr);
        xpe_clear_alerts();
        xpe_shutdown();
        std::remove("oom_common_log_file_name_long.txt");
        std::remove("oom_common_log_A_long_file_name.txt");
        std::remove("oom_common_log_B_long_file_name.txt");
        std::error_code ec;
        std::filesystem::remove_all("oom_common_log_dir_not_a_file", ec);
    }
};

}  // namespace

/* =========================================================================
 * Control: the harness reaches library code
 * ========================================================================= */

TEST_F(CommonOom, TheInjectionReachesAnAllocationMadeInsideTheLibrary) {
    ASSERT_EQ(XPE_OK, xpe_init(nullptr));
    const std::string m = longMessage(1);
    arm(1);
    // Through std::function: MSVC /EHsc treats a direct extern "C" call as non-throwing, so a try/catch
    // around it does not reliably see the exception.
    const std::function<void()> call = [&] { xpe_alert_push(m.c_str(), XPE_ALERT_INFO); };
    try { call(); } catch (...) {}
    const bool injected = disarm();
    EXPECT_TRUE(injected) << "the first allocation of xpe_alert_push must be one the harness can fail";
}

/* =========================================================================
 * xpe_alert_push: every alert is either in the queue or counted as lost
 * ========================================================================= */

namespace {
/** A queue holding `before` alerts of `sev`, with `pushed` receiving how many alerts were pushed in all. */
Setup queueOf(int before, int32_t sev, long* pushed) {
    return [before, sev, pushed] {
        xpe_shutdown();
        xpe_clear_alerts();
        ASSERT_EQ(XPE_OK, xpe_init(nullptr));
        for (int i = 0; i < before; ++i) xpe_alert_push(longMessage(i).c_str(), sev);
        *pushed = before;
    };
}

/**
 * The accounting invariant: pushed == alerts in the queue + alerts counted as lost, the queue never exceeds
 * its 64 slots, and there is at most one loss alert. After a further, unarmed push the loss alert's text
 * has caught up with the count.
 */
Check accounted(long* pushed) {
    return [pushed](XpeErrorCode, bool) -> std::string {
        QueueView v = view();
        const uint64_t dropped = xpe_common_alerts_dropped_for_test();
        char msg[160];
        if (static_cast<uint64_t>(v.nonLoss) + dropped != static_cast<uint64_t>(*pushed + 1)) {
            std::snprintf(msg, sizeof(msg), "%ld alerts pushed but %ld in the queue and %llu counted as lost",
                          *pushed + 1, v.nonLoss, static_cast<unsigned long long>(dropped));
            return msg;
        }
        if (v.nonLoss + v.loss > 64 || v.loss > 1) {
            std::snprintf(msg, sizeof(msg), "queue holds %ld alerts and %ld loss alerts", v.nonLoss, v.loss);
            return msg;
        }
        // The report catches up: one more push, then the loss alert states the exact count.
        xpe_alert_push(longMessage(9999).c_str(), XPE_ALERT_INFO);
        ++*pushed;
        v = view();
        const uint64_t dropped2 = xpe_common_alerts_dropped_for_test();
        if (dropped2 > 0 && (v.loss != 1 || v.lossTextCount != dropped2)) {
            std::snprintf(msg, sizeof(msg), "loss alert says %llu, the count is %llu",
                          static_cast<unsigned long long>(v.lossTextCount), static_cast<unsigned long long>(dropped2));
            return msg;
        }
        return {};
    };
}
}  // namespace

TEST_F(CommonOom, APushIntoAFullQueueAccountsForEveryAlert) {
    long pushed = 0;
    const std::string m = longMessage(5000);
    sweep("xpe_alert_push (full queue)", queueOf(64, XPE_ALERT_INFO, &pushed),
          [&] { xpe_alert_push(m.c_str(), XPE_ALERT_INFO); return XPE_OK; }, accounted(&pushed));
}

TEST_F(CommonOom, APushIntoAQueueWithRoomAccountsForEveryAlert) {
    long pushed = 0;
    const std::string m = longMessage(5000);
    sweep("xpe_alert_push (room in the queue)", queueOf(10, XPE_ALERT_WARNING, &pushed),
          [&] { xpe_alert_push(m.c_str(), XPE_ALERT_ERROR); return XPE_OK; }, accounted(&pushed));
}

TEST_F(CommonOom, APushIntoAQueueThatAlreadyHoldsALossAlertAccountsForEveryAlert) {
    long pushed = 0;
    const std::string m = longMessage(5000);
    sweep("xpe_alert_push (loss alert present)", queueOf(70, XPE_ALERT_INFO, &pushed),
          [&] { xpe_alert_push(m.c_str(), XPE_ALERT_INFO); return XPE_OK; }, accounted(&pushed));
}

/* =========================================================================
 * xpe_init: an error return leaves the library as the call found it
 * ========================================================================= */

TEST_F(CommonOom, AnInitThatFailsLeavesTheLibraryUninitialized) {
    sweep("xpe_init (first init)",
          [] { xpe_shutdown(); xpe_clear_alerts(); ASSERT_FALSE(initialized()); },
          [] { return xpe_init(kLongConfig.c_str()); },
          [](XpeErrorCode rc, bool) -> std::string {
              if (rc == XPE_OK) return initialized() ? std::string() : "returned OK but is not initialized";
              if (initialized()) return "returned an error but the library is initialized (partial commit)";
              if (rc != XPE_ERR_OUT_OF_MEMORY) return "an allocation failure must be XPE_ERR_OUT_OF_MEMORY";
              return {};
          });
}

TEST_F(CommonOom, ARe_initThatFailsLeavesTheQueuedAlertsAndTheInitializedStateAlone) {
    sweep("xpe_init (re-init)",
          [] {
              xpe_shutdown(); xpe_clear_alerts();
              ASSERT_EQ(XPE_OK, xpe_init(nullptr));
              for (int i = 0; i < 3; ++i) xpe_alert_push(longMessage(i).c_str(), XPE_ALERT_WARNING);
              ASSERT_EQ(3, xpe_get_pending_alert_count());
          },
          [] { return xpe_init(kLongConfig.c_str()); },
          [](XpeErrorCode rc, bool) -> std::string {
              if (rc == XPE_OK) return xpe_get_pending_alert_count() == 0 ? std::string() : "a successful re-init clears the queue";
              if (!initialized()) return "returned an error and left the library uninitialized";
              if (xpe_get_pending_alert_count() != 3) return "returned an error but the queued alerts were cleared (partial commit)";
              if (rc != XPE_ERR_OUT_OF_MEMORY) return "an allocation failure must be XPE_ERR_OUT_OF_MEMORY";
              return {};
          });
}

/* =========================================================================
 * xpe_configure / xpe_log_set_file: an allocation failure is OUT_OF_MEMORY, not the code of another fault
 * ========================================================================= */

TEST_F(CommonOom, AConfigureThatRunsOutOfMemoryIsNotReportedAsAnInvalidConfiguration) {
    // Controls: the codes the function does give for a bad document and for a good one.
    ASSERT_EQ(XPE_OK, xpe_init(nullptr));
    ASSERT_EQ(XPE_ERR_CONFIG_INVALID, xpe_configure("{bad"));
    ASSERT_EQ(XPE_OK, xpe_configure(kLongConfig.c_str()));
    sweep("xpe_configure",
          [] { xpe_shutdown(); ASSERT_EQ(XPE_OK, xpe_init(nullptr)); },
          [] { return xpe_configure(kLongConfig.c_str()); },
          [](XpeErrorCode rc, bool) -> std::string {
              if (rc == XPE_OK) return {};
              return rc == XPE_ERR_OUT_OF_MEMORY ? std::string() : "an allocation failure must be XPE_ERR_OUT_OF_MEMORY";
          });
}

TEST_F(CommonOom, ALogFileThatRunsOutOfMemoryIsNotReportedAsAnIoFailure) {
    // Control: a real I/O fault (a directory that does not exist) is still IO_FAILED.
    ASSERT_EQ(XPE_OK, xpe_init(nullptr));
    ASSERT_EQ(XPE_ERR_IO_FAILED, xpe_log_set_file("no_such_directory_for_xpe/oom_common_log.txt"));
    sweep("xpe_log_set_file",
          [] { xpe_log_set_file(nullptr); std::remove("oom_common_log_file_name_long.txt"); },
          [] { return xpe_log_set_file("oom_common_log_file_name_long.txt"); },
          [](XpeErrorCode rc, bool) -> std::string {
              if (rc == XPE_OK) return {};
              return rc == XPE_ERR_OUT_OF_MEMORY ? std::string() : "an allocation failure must be XPE_ERR_OUT_OF_MEMORY";
          });
}

/* =========================================================================
 * xpe_log_set_file: a switch that fails leaves the logger the caller had (QA-A-202c, Codex #27 B1)
 * ========================================================================= */

namespace {

constexpr const char* kLogA = "oom_common_log_A_long_file_name.txt";
constexpr const char* kLogB = "oom_common_log_B_long_file_name.txt";

std::string readAll(const char* path) {
    std::ifstream f(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

}  // namespace

TEST_F(CommonOom, ALogFileSwitchThatRunsOutOfMemoryKeepsWritingToThePreviousFile) {
    ASSERT_EQ(XPE_OK, xpe_init(nullptr));
    int probes = 0;
    sweep("xpe_log_set_file (A to B)",
          [] {
              xpe_log_set_file(nullptr);
              std::remove(kLogA);
              std::remove(kLogB);
              ASSERT_EQ(XPE_OK, xpe_log_set_file(kLogA));
              auto lg = spdlog::default_logger();
              ASSERT_TRUE(lg != nullptr);
              lg->info("seed line");
              lg->flush();
          },
          [] { return xpe_log_set_file(kLogB); },
          [&probes](XpeErrorCode rc, bool) -> std::string {
              const std::string marker = "probe-" + std::to_string(++probes);
              auto lg = spdlog::default_logger();
              if (!lg) return "there is no default logger after the call";
              lg->info(marker);
              lg->flush();
              const bool inA = readAll(kLogA).find(marker) != std::string::npos;
              const bool inB = readAll(kLogB).find(marker) != std::string::npos;
              if (rc == XPE_OK) return (inB && !inA) ? std::string() : "the switch succeeded but the line did not land in B";
              if (rc != XPE_ERR_OUT_OF_MEMORY) return "an allocation failure must be XPE_ERR_OUT_OF_MEMORY";
              if (!inA) return "a failed switch lost the previous logger: a line written afterwards did not reach A";
              if (inB) return "a failed switch half-installed B";
              return {};
          });
    std::remove(kLogA);
    std::remove(kLogB);
}

/* =========================================================================
 * The log file switch, step by step (QA-A-202d, Codex #32 B1)
 * ========================================================================= */

namespace {

/** A sink that keeps what it is given and whose flush can be made to fail, like a file on a full disk. */
class FlakySink : public spdlog::sinks::base_sink<std::mutex> {
public:
    std::atomic<bool> failFlush{false};
    std::vector<std::string> lines;
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t buf;
        formatter_->format(msg, buf);
        lines.emplace_back(buf.data(), buf.size());
    }
    void flush_() override {
        if (failFlush.load()) throw spdlog::spdlog_ex("injected flush failure");
    }
};

bool fileExists(const char* path) { return std::ifstream(path).good(); }

}  // namespace

// The previous logger's flush happens BEFORE anything is replaced: a flush that fails is this call's error,
// and the logger the caller has -- and the spdlog default pointing at it -- stays. (It used to flush after
// the new logger was installed, so the call reported an error and the output had already moved.)
TEST_F(CommonOom, AFlushThatFailsRefusesTheSwitchAndLeavesThePreviousLogger) {
    ASSERT_EQ(XPE_OK, xpe_init(nullptr));
    auto sink = std::make_shared<FlakySink>();
    auto previous = std::make_shared<spdlog::logger>("xpe_file", sink);
    previous->set_level(spdlog::level::trace);
    std::remove(kLogB);
    xpe_log_adopt_logger_for_test(previous);

    // Control: the seam works -- a line written through the default logger reaches the sink.
    spdlog::default_logger()->info("control-line");
    ASSERT_EQ(1u, sink->lines.size()) << "control: the adopted logger is the default";

    sink->failFlush.store(true);
    EXPECT_EQ(XPE_ERR_IO_FAILED, xpe_log_set_file(kLogB)) << "a failed flush is an error of the call";
    EXPECT_EQ(previous.get(), spdlog::default_logger().get()) << "the default logger is still the previous one";
    EXPECT_FALSE(fileExists(kLogB)) << "the replacement was not even built: nothing was created";
    spdlog::default_logger()->info("after-failed-switch");
    EXPECT_EQ(2u, sink->lines.size()) << "later lines still go to the previous logger";

    // And once the flush works again the same call succeeds and the output moves.
    sink->failFlush.store(false);
    ASSERT_EQ(XPE_OK, xpe_log_set_file(kLogB));
    spdlog::default_logger()->info("in-b");
    spdlog::default_logger()->flush();
    EXPECT_EQ(2u, sink->lines.size()) << "the previous logger no longer receives lines";
    EXPECT_NE(std::string::npos, readAll(kLogB).find("in-b"));
}

// Every other step that can fail before the install -- the directory check, opening the file, building the
// sink, the logger -- leaves the previous logger in place. Opening the file is made to fail by naming a
// directory; the allocation sweeps cover the allocations of the rest.
TEST_F(CommonOom, ALogFileThatCannotBeOpenedLeavesThePreviousLogger) {
    ASSERT_EQ(XPE_OK, xpe_init(nullptr));
    std::remove(kLogA);
    ASSERT_EQ(XPE_OK, xpe_log_set_file(kLogA));
    spdlog::default_logger()->info("seed");
    spdlog::default_logger()->flush();

    std::filesystem::create_directories("oom_common_log_dir_not_a_file");
    EXPECT_EQ(XPE_ERR_IO_FAILED, xpe_log_set_file("oom_common_log_dir_not_a_file"))
        << "control: a directory cannot be opened as a log file";
    EXPECT_EQ(XPE_ERR_IO_FAILED, xpe_log_set_file("no_such_directory_for_xpe_log/x.txt"));
    spdlog::default_logger()->info("after-refusals");
    spdlog::default_logger()->flush();
    const std::string a = readAll(kLogA);
    EXPECT_NE(std::string::npos, a.find("seed"));
    EXPECT_NE(std::string::npos, a.find("after-refusals")) << "later lines still reach the previous file";
}

// The header says the file is opened in append mode. It was opened truncating, so naming the SAME path again
// emptied the file; and a switch that failed after the sink was created left the previous logger alive over a
// file whose earlier lines were gone.
TEST_F(CommonOom, NamingTheSameLogFileAgainKeepsTheLinesAlreadyThere) {
    ASSERT_EQ(XPE_OK, xpe_init(nullptr));
    sweep("xpe_log_set_file (same path again)",
          [] {
              xpe_log_set_file(nullptr);
              std::remove(kLogA);
              ASSERT_EQ(XPE_OK, xpe_log_set_file(kLogA));
              spdlog::default_logger()->info("seed-row");
              spdlog::default_logger()->flush();
          },
          [] { return xpe_log_set_file(kLogA); },
          [](XpeErrorCode rc, bool) -> std::string {
              if (rc != XPE_OK && rc != XPE_ERR_OUT_OF_MEMORY) return "an allocation failure must be XPE_ERR_OUT_OF_MEMORY";
              auto lg = spdlog::default_logger();
              if (!lg) return "there is no default logger after the call";
              lg->info("probe-row");
              lg->flush();
              const std::string a = readAll(kLogA);
              if (a.find("seed-row") == std::string::npos) return "the line written before the call is gone from the file";
              if (a.find("probe-row") == std::string::npos) return "a line written after the call did not reach the file";
              return {};
          });
    std::remove(kLogA);
}
