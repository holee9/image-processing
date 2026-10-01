/**
 * @file test_ai_log_format.cpp
 * @brief The real log call sites of xpe_ai print their values, not printf placeholders (QA-B-177).
 *
 * ai.cpp's AI_LOG_* macros forwarded printf-style calls to spdlog, which formats with {} and not %, so
 * "model_dir=%s, ep=%d" reached the log literally and the worker failure code (the 860th line of
 * ai.cpp: "worker path failed (%d)") was never in any log. These tests read the text the DLL's own
 * spdlog actually emitted (xpe_ai_test_set_log_capture, a test-only hook) while the real functions run.
 *
 * Needs a build with XPE_AI_TEST_HOOKS and spdlog; otherwise the tests skip.
 */
#include <gtest/gtest.h>

#include <mutex>
#include <regex>
#include <string>
#include <vector>

#include "xpe/ai/ai_api.h"
#include "xpe/common/xpe_types.h"

#ifdef XPE_AI_TEST_LOG_CAPTURE
extern "C" __declspec(dllimport) void xpe_ai_test_set_log_capture(void (*cb)(int level, const char* message));

namespace {

std::mutex g_mu;
std::vector<std::string> g_lines;

void OnLog(int /*level*/, const char* message) {
    std::lock_guard<std::mutex> lock(g_mu);
    g_lines.emplace_back(message);
}

/** Captures the DLL's log text for its lifetime. */
class LogCapture {
public:
    LogCapture() {
        std::lock_guard<std::mutex> lock(g_mu);
        g_lines.clear();
    }
    void Start() { xpe_ai_test_set_log_capture(&OnLog); }
    ~LogCapture() { xpe_ai_test_set_log_capture(nullptr); }
    std::vector<std::string> Lines() const {
        std::lock_guard<std::mutex> lock(g_mu);
        return g_lines;
    }
    /** The first captured line containing @p needle, or empty. */
    std::string Find(const std::string& needle) const {
        for (const auto& l : Lines()) {
            if (l.find(needle) != std::string::npos) return l;
        }
        return std::string();
    }
};

const std::string kDirMissing = std::string(XPE_AI_TEST_DATA_DIR) + "/models_missing";

struct Frame {
    float in[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    float out[9] = {0};
    XpeImageBuffer a{}, b{};
    Frame() {
        a.width = b.width = 3;
        a.height = b.height = 3;
        a.bitsAllocated = b.bitsAllocated = 32;
        a.bitsStored = b.bitsStored = 32;
        a.format = b.format = XPE_PIXEL_FLOAT32;
        a.data = in;
        b.data = out;
        a.dataSize = b.dataSize = sizeof(in);
    }
};

/** A printf conversion that survived into a log line: the signature of the defect. */
bool HasPlaceholder(const std::string& line, std::string* which = nullptr) {
    static const std::regex re(R"(%[-+ #0]*[0-9.*]*(hh|h|ll|l|z|j|t|L)?[diuoxXfFeEgGaAcspn])");
    std::smatch m;
    if (std::regex_search(line, m, re)) {
        if (which) *which = m.str();
        return true;
    }
    return false;
}

struct AiLogFixture : public ::testing::Test {
    void SetUp() override { xpe_ai_shutdown(); }
    void TearDown() override { xpe_ai_shutdown(); }
};

}  // namespace

TEST_F(AiLogFixture, InitLogsTheRealModelDirectoryAndSettingsNotPlaceholders) {
    LogCapture cap;
    cap.Start();
    ASSERT_EQ(XPE_OK, xpe_ai_init("C:/xpe_log_probe/models_abc", "{\"timeout_ms\": 1234}"));
    const std::string line = cap.Find("xpe_ai initialized");
    ASSERT_FALSE(line.empty()) << "the init message was not captured at all";
    EXPECT_TRUE(std::regex_search(
        line, std::regex(R"(xpe_ai initialized: model_dir=C:/xpe_log_probe/models_abc, ep=\d+, timeout=1234 ms)")))
        << "got: " << line;
}

TEST_F(AiLogFixture, ShutdownLogsTheRealWorkerPid) {
    ASSERT_EQ(XPE_OK, xpe_ai_init("C:/xpe_log_probe/models_abc", nullptr));
    LogCapture cap;
    cap.Start();
    xpe_ai_shutdown();
    const std::string line = cap.Find("xpe_ai shutdown");
    ASSERT_FALSE(line.empty());
    EXPECT_TRUE(std::regex_search(line, std::regex(R"(xpe_ai shutdown: worker_pid=\d+$)"))) << "got: " << line;
}

TEST_F(AiLogFixture, FallbackModeMessageNamesTheState) {
    ASSERT_EQ(XPE_OK, xpe_ai_init("C:/xpe_log_probe/models_abc", nullptr));
    LogCapture cap;
    cap.Start();
    ASSERT_EQ(XPE_OK, xpe_ai_set_fallback_mode(1));
    EXPECT_FALSE(cap.Find("Fallback mode enabled").empty()) << "no 'Fallback mode enabled' line";
    ASSERT_EQ(XPE_OK, xpe_ai_set_fallback_mode(0));
    EXPECT_FALSE(cap.Find("Fallback mode disabled").empty()) << "no 'Fallback mode disabled' line";
}

// The line that matters most: a worker-path failure logs its code and its place in the consecutive run.
TEST_F(AiLogFixture, WorkerPathFailureLogCarriesTheCodeAndTheCount) {
    ASSERT_EQ(XPE_OK, xpe_ai_init(kDirMissing.c_str(), "{\"use_worker\": true}"));
    LogCapture cap;
    cap.Start();
    Frame f;
    ASSERT_EQ(XPE_ERR_IO_FAILED, xpe_bone_suppress(&f.a, &f.b, nullptr));   // no model: the worker says -9
    EXPECT_FALSE(cap.Find("bone_suppress: worker path failed (-9), input returned unchanged "
                          "(1 of 3 consecutive failures)").empty())
        << "lines: " << ::testing::PrintToString(cap.Lines());
    ASSERT_EQ(XPE_ERR_IO_FAILED, xpe_bone_suppress(&f.a, &f.b, nullptr));
    EXPECT_FALSE(cap.Find("(2 of 3 consecutive failures)").empty());
}

TEST_F(AiLogFixture, InProcessErrorLogNamesThePath) {
    ASSERT_EQ(XPE_OK, xpe_ai_init(kDirMissing.c_str(), "{\"use_worker\": false}"));
    LogCapture cap;
    cap.Start();
    Frame f;
    ASSERT_EQ(XPE_ERR_IO_FAILED, xpe_bone_suppress(&f.a, &f.b, nullptr));
    EXPECT_FALSE(cap.Find("bone_suppress: no model at " + kDirMissing + "/bone_suppress.onnx").empty())
        << "lines: " << ::testing::PrintToString(cap.Lines());
}

// A guard for call sites nobody has written yet: exercise the paths, then require that NO line carries a
// printf conversion. A control first, so an empty capture cannot pass this vacuously.
TEST_F(AiLogFixture, NoPrintfPlaceholderSurvivesAnyLoggedPath) {
    LogCapture cap;
    cap.Start();
    ASSERT_EQ(XPE_OK, xpe_ai_init(kDirMissing.c_str(), "{\"use_worker\": true, \"timeout_ms\": 2000}"));
    ASSERT_EQ(XPE_OK, xpe_ai_init(kDirMissing.c_str(), nullptr));   // already initialised: a warning
    ASSERT_EQ(XPE_OK, xpe_ai_set_fallback_mode(1));
    ASSERT_EQ(XPE_OK, xpe_ai_set_fallback_mode(0));
    Frame f;
    for (int i = 0; i < 4; ++i) xpe_bone_suppress(&f.a, &f.b, nullptr);   // worker failures, then switched off
    xpe_ai_shutdown();
    ASSERT_EQ(XPE_OK, xpe_ai_init(kDirMissing.c_str(), "{\"use_worker\": false}"));
    xpe_bone_suppress(&f.a, &f.b, nullptr);   // in-process error
    xpe_ai_shutdown();

    const auto lines = cap.Lines();
    ASSERT_GE(lines.size(), 8u) << "control: the capture saw too little to prove anything";
    for (const auto& l : lines) {
        std::string which;
        EXPECT_FALSE(HasPlaceholder(l, &which)) << "unformatted '" << which << "' in: " << l;
    }
}

#else   // XPE_AI_TEST_LOG_CAPTURE

TEST(AiLogFormat, SkippedWithoutTestHooksOrSpdlog) {
    GTEST_SKIP() << "built without XPE_AI_TEST_HOOKS or spdlog: the DLL's log text cannot be captured";
}

#endif
