/**
 * @file test_ai_trust_recheck.cpp
 * @brief A refused model is verified again on every call, and a card lookup does not hold the module lock while it reads
 *        (QA-B-198b, Codex #95, #130).
 *
 * MEMO REMOVED. A refused load used to be remembered under a stamp of the three model files' size and write time, and the
 * next call with the same stamp answered "refused" without reading anything. A signature file put back as it was, with its
 * write time restored, kept the refusal for the rest of the session ("the refusal side is the safe side" is only true of a
 * wrong APPROVAL; a wrong refusal of a good model is a failure too). There is no memo now. The tests:
 *   - the files are read again on every refused call (counted through the read hook), and
 *   - a repaired model whose size and write time are exactly those of the refused one is accepted on the next call.
 * The cost of verifying again is in the QA-B-198b report.
 *
 * CARD LOOKUP WITHOUT THE LOCK. xpe_ai_get_model_card read and hashed the model under the module mutex (768 MiB: 1.17 s), the
 * same mutex inference takes. It now copies the model directory under the lock and reads outside it. A hook that makes the
 * file reads slow on ONE thread shows it: a second call must not wait for the slow one.
 * NOT TESTED: the branch that finds the module pointed at another directory after the read (it needs an init while another
 * thread is inside a call, which the module's contract rules out).
 */

#include <gtest/gtest.h>

#include "test_signing_helper.h"
#include "xpe/ai/ai_api.h"
#include "xpe/ai/ai_onnx_session.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"

#include <windows.h>

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

#ifndef XPE_AI_TEST_DATA_DIR
#error "XPE_AI_TEST_DATA_DIR must be defined by the build (modules/ai/CMakeLists.txt)"
#endif

#ifdef XPE_AI_TEST_HOOKS

extern "C" __declspec(dllimport) void xpe_ai_test_set_before_file_read_hook(void (*hook)(void));

namespace {

namespace fs = std::filesystem;
using xpe::ai::OnnxSession;

const std::string kData = XPE_AI_TEST_DATA_DIR;

struct TempDir {
    fs::path path;
    explicit TempDir(const char* name) {
        path = fs::temp_directory_path() / (std::string("xpe_recheck_") + name + "_" + std::to_string(GetCurrentProcessId()));
        std::error_code ec;
        fs::remove_all(path, ec);
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    void CopyFrom(const char* dataDir) const {
        for (const auto& e : fs::directory_iterator(fs::path(kData) / dataDir)) {
            fs::copy_file(e.path(), path / e.path().filename(), fs::copy_options::overwrite_existing);
        }
    }
    fs::path operator/(const char* name) const { return path / name; }
};

void WriteBytes(const fs::path& p, const std::vector<uint8_t>& b) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

std::atomic<int> g_reads{0};
void CountRead() { ++g_reads; }

struct ReadCounter {
    ReadCounter() {
        g_reads = 0;
        xpe_ai_test_set_before_file_read_hook(&CountRead);
    }
    ~ReadCounter() { xpe_ai_test_set_before_file_read_hook(nullptr); }
    ReadCounter(const ReadCounter&) = delete;
    ReadCounter& operator=(const ReadCounter&) = delete;
};

thread_local bool t_slow = false;
void SlowOnTheMarkedThread() {
    if (t_slow) Sleep(400);
}

struct SlowReads {
    SlowReads() { xpe_ai_test_set_before_file_read_hook(&SlowOnTheMarkedThread); }
    ~SlowReads() { xpe_ai_test_set_before_file_read_hook(nullptr); }
    SlowReads(const SlowReads&) = delete;
    SlowReads& operator=(const SlowReads&) = delete;
};

/** One xpe_bone_suppress call on a 3x3 frame of ones; the output starts as -777. */
XpeErrorCode BoneCall(std::vector<float>* out) {
    std::vector<float> in(9, 1.0f);
    out->assign(9, -777.0f);
    XpeImageBuffer ib{}, ob{};
    ib.width = ib.height = ob.width = ob.height = 3;
    ib.bitsAllocated = ib.bitsStored = ob.bitsAllocated = ob.bitsStored = 32;
    ib.format = ob.format = XPE_PIXEL_FLOAT32;
    ib.data = in.data();
    ob.data = out->data();
    ib.dataSize = ob.dataSize = in.size() * sizeof(float);
    return xpe_bone_suppress(&ib, &ob, nullptr);
}

std::string RecognizeLabel() {
    std::vector<float> px(16, 0.0f);
    XpeImageBuffer ib{};
    ib.width = ib.height = 4;
    ib.bitsAllocated = ib.bitsStored = 32;
    ib.format = XPE_PIXEL_FLOAT32;
    ib.data = px.data();
    ib.dataSize = px.size() * sizeof(float);
    char label[64];
    std::memset(label, 'x', sizeof(label));
    float conf = -1.0f;
    (void)xpe_bodypart_recognize(&ib, label, sizeof(label), &conf);
    return std::string(label, strnlen(label, sizeof(label)));
}

using Clock = std::chrono::steady_clock;
long long MsSince(Clock::time_point t) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t).count();
}

}  // namespace

TEST(TrustRecheck, ARefusedBoneModelIsReadAndVerifiedAgainOnEveryCall) {
    if (OnnxSession::IsStubBuild()) GTEST_SKIP() << "stub build: xpe_bone_suppress answers before it looks for a model";
    const TempDir t("count");
    t.CopyFrom("models_x2");
    std::vector<uint8_t> m = xpe_test::ReadBytes(t / "bone_suppress.onnx");
    m[m.size() / 2] ^= 0x01;
    WriteBytes(t / "bone_suppress.onnx", m);
    xpe_ai_shutdown();
    ASSERT_EQ(XPE_OK, xpe_ai_init(t.path.string().c_str(), "{}"));
    const ReadCounter counter;
    std::vector<float> out;
    ASSERT_EQ(XPE_ERR_CONFIG_INVALID, BoneCall(&out));
    const int first = g_reads.load();
    ASSERT_GT(first, 0) << "a refused call reads the model files (the control: the counter sees reads at all)";
    ASSERT_EQ(XPE_ERR_CONFIG_INVALID, BoneCall(&out));
    EXPECT_EQ(2 * first, g_reads.load()) << "was: the second refused call read nothing (it answered from a memo)";
    ASSERT_EQ(XPE_ERR_CONFIG_INVALID, BoneCall(&out));
    EXPECT_EQ(3 * first, g_reads.load());
    xpe_ai_shutdown();
    xpe_clear_alerts();
}

TEST(TrustRecheck, ARepairedBoneModelWithTheSizeAndWriteTimeOfTheRefusedOneIsAcceptedAtOnce) {
    if (OnnxSession::IsStubBuild()) GTEST_SKIP() << "stub build: xpe_bone_suppress answers before it looks for a model";
    const TempDir t("bone_repair");
    t.CopyFrom("models_x2");
    const fs::path model = t / "bone_suppress.onnx";
    const std::vector<uint8_t> original = xpe_test::ReadBytes(model);
    const auto t0 = fs::last_write_time(model);
    std::vector<uint8_t> bad = original;
    bad[bad.size() / 2] ^= 0x01;   // same size
    WriteBytes(model, bad);
    fs::last_write_time(model, t0);   // same write time
    xpe_ai_shutdown();
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_ai_init(t.path.string().c_str(), "{}"));
    std::vector<float> out;
    ASSERT_EQ(XPE_ERR_CONFIG_INVALID, BoneCall(&out)) << "the control: the tampered model is refused";

    WriteBytes(model, original);      // put back exactly as it was ...
    fs::last_write_time(model, t0);   // ... with its time: size and write time are those of the refused file
    EXPECT_EQ(XPE_OK, BoneCall(&out)) << "was: still refused -- a good model stayed refused for the session";
    for (const float v : out) EXPECT_EQ(2.0f, v);
    xpe_ai_shutdown();
    xpe_clear_alerts();
}

TEST(TrustRecheck, ARepairedBodyPartSidecarWithTheSizeAndWriteTimeOfTheRefusedOneIsAcceptedAtOnce) {
    if (OnnxSession::IsStubBuild()) GTEST_SKIP() << "stub build: xpe_bodypart_recognize answers before it looks for a model";
    const TempDir t("part_repair");
    t.CopyFrom("models_bodypart_a");
    const fs::path sidecar = t / "bodypart.json";
    const std::vector<uint8_t> original = xpe_test::ReadBytes(sidecar);
    const auto t0 = fs::last_write_time(sidecar);
    std::string text(original.begin(), original.end());
    const size_t at = text.find("CHEST");
    ASSERT_NE(std::string::npos, at);
    text.replace(at, 5, "CHESS");   // same size, and the signature is now stale
    WriteBytes(sidecar, std::vector<uint8_t>(text.begin(), text.end()));
    fs::last_write_time(sidecar, t0);
    xpe_ai_shutdown();
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_ai_init(t.path.string().c_str(), "{}"));
    ASSERT_EQ("UNKNOWN", RecognizeLabel()) << "the control: the tampered sidecar is refused";

    WriteBytes(sidecar, original);
    fs::last_write_time(sidecar, t0);
    EXPECT_EQ("CHEST", RecognizeLabel()) << "was: UNKNOWN -- a good model stayed refused for the session";
    xpe_ai_shutdown();
    xpe_clear_alerts();
}

TEST(TrustRecheck, ASlowCardLookupDoesNotKeepAnotherCardLookupWaiting) {
    const TempDir t("card_lock");
    t.CopyFrom("models_x2");
    xpe_ai_shutdown();
    ASSERT_EQ(XPE_OK, xpe_ai_init(t.path.string().c_str(), "{}"));
    const SlowReads slow;
    long long slowMs = 0, otherMs = 0;
    XpeErrorCode slowRc = XPE_ERR_PROCESSING_FAILED, otherRc = XPE_ERR_PROCESSING_FAILED;
    std::thread a([&] {
        t_slow = true;   // only this thread's reads are slow: the stand-in for a large model
        char buf[8192];
        const auto start = Clock::now();
        slowRc = xpe_ai_get_model_card("bone_toy_x2", buf, sizeof(buf));
        slowMs = MsSince(start);
    });
    Sleep(250);   // the slow lookup is now inside its first read
    std::thread b([&] {
        char buf[8192];
        const auto start = Clock::now();
        otherRc = xpe_ai_get_model_card("bone_toy_x2", buf, sizeof(buf));
        otherMs = MsSince(start);
    });
    a.join();
    b.join();
    EXPECT_EQ(XPE_OK, slowRc);
    EXPECT_EQ(XPE_OK, otherRc);
    EXPECT_GE(slowMs, 1000) << "the control: the marked lookup really took about 3 x 400 ms";
    EXPECT_LT(otherMs, 400) << "was: about " << (slowMs - 250) << " ms -- the second call waited for the slow read's lock";
    xpe_ai_shutdown();
}

TEST(TrustRecheck, AnInferenceIsNotKeptOutByASlowCardLookup) {
    if (OnnxSession::IsStubBuild()) GTEST_SKIP() << "stub build: no inference takes the module lock";
    const TempDir t("card_vs_infer");
    t.CopyFrom("models_x2");
    xpe_ai_shutdown();
    ASSERT_EQ(XPE_OK, xpe_ai_init(t.path.string().c_str(), "{}"));
    std::vector<float> out;
    ASSERT_EQ(XPE_OK, BoneCall(&out)) << "warm: the session is loaded, so the next call reads no file";
    const SlowReads slow;
    long long slowMs = 0, inferMs = 0;
    XpeErrorCode inferRc = XPE_ERR_PROCESSING_FAILED;
    std::thread a([&] {
        t_slow = true;
        char buf[8192];
        const auto start = Clock::now();
        (void)xpe_ai_get_model_card("bone_toy_x2", buf, sizeof(buf));
        slowMs = MsSince(start);
    });
    Sleep(250);
    std::vector<float> out2;
    const auto start = Clock::now();
    inferRc = BoneCall(&out2);
    inferMs = MsSince(start);
    a.join();
    EXPECT_EQ(XPE_OK, inferRc);
    EXPECT_GE(slowMs, 1000) << "the control: the lookup was slow";
    EXPECT_LT(inferMs, 400) << "was: about " << (slowMs - 250) << " ms -- the inference waited for the lookup's read";
    xpe_ai_shutdown();
}

/**
 * MEASUREMENT, not a check (run with --gtest_also_run_disabled_tests --gtest_filter=TrustRecheck.DISABLED_*): what a REFUSED
 * call costs now that nothing is remembered -- the model is read and hashed and the signature refused, every call. The model is
 * padded to the size and one byte of it is changed. Prints milliseconds per call; asserts only that the call is refused.
 */
TEST(TrustRecheck, DISABLED_MeasureTheCostOfARefusedCall) {
    for (const size_t mib : {1u, 64u, 256u}) {
        const TempDir t("cost");
        t.CopyFrom("models_x2");
        std::vector<uint8_t> m = xpe_test::ReadBytes(t / "bone_suppress.onnx");
        m.resize(mib * 1024u * 1024u, 0);
        m[m.size() / 2] ^= 0x01;
        WriteBytes(t / "bone_suppress.onnx", m);
        xpe_ai_shutdown();
        ASSERT_EQ(XPE_OK, xpe_ai_init(t.path.string().c_str(), "{}"));
        std::vector<float> out;
        double total = 0;
        const int calls = 5;
        for (int i = 0; i < calls; ++i) {
            const auto start = Clock::now();
            ASSERT_EQ(XPE_ERR_CONFIG_INVALID, BoneCall(&out));
            total += static_cast<double>(MsSince(start));
        }
        std::printf("REFUSED-CALL COST: model %3zu MiB -> %.1f ms per call (mean of %d)\n", mib, total / calls, calls);
        xpe_ai_shutdown();
        xpe_clear_alerts();
    }
}

#endif  // XPE_AI_TEST_HOOKS
