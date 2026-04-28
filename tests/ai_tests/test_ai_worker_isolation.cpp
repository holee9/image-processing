/**
 * @file test_ai_worker_isolation.cpp
 * @brief Worker process isolation tests for xpe_ai.dll -- SPEC-XPE-P3-AI
 *
 * Validates crash isolation and fallback behavior in stub mode.
 * In the full implementation, these tests will verify actual worker process
 * crash recovery. In stub mode, all inference functions fall back
 * deterministically.
 *
 * REQ-AI-003: Worker-isolated architecture (IPC via named pipe).
 * REQ-AI-002: Deterministic fallback for all AI functions.
 *
 * @ingroup xpe_ai_tests
 */

#include "xpe/ai/ai_api.h"
#include "xpe/ai/ai_worker_protocol.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include <gtest/gtest.h>

#include <cstring>
#include <vector>
#include <thread>
#include <atomic>
#include <cstdint>

/* ============================================================================
 * Test Fixture: initialize AI module before each test
 * ============================================================================ */

class AiWorkerIsolationTest : public ::testing::Test {
protected:
    void SetUp() override {
        xpe_ai_shutdown();
        ASSERT_EQ(xpe_ai_init("dummy_model_dir", nullptr), XPE_OK);
    }

    void TearDown() override {
        xpe_ai_shutdown();
    }
};

/* ============================================================================
 * Helper: create a valid XpeImageBuffer for testing
 * ============================================================================ */

static XpeImageBuffer makeTestBuffer(uint32_t w, uint32_t h,
                                      std::vector<uint16_t>& storage)
{
    storage.assign(w * h, 1000);
    return XpeImageBuffer{
        w, h,
        16,    // bitsAllocated
        12,    // bitsStored
        XPE_PIXEL_UINT16,
        storage.data(),
        static_cast<size_t>(w * h * 2)
    };
}

/* ============================================================================
 * Worker Crash Simulation (Stub Mode)
 *
 * In stub mode, the worker process does not exist. All inference functions
 * should gracefully fall back to XPE_ERR_PROCESSING_FAILED without crashing
 * the host process.
 * ============================================================================ */

TEST_F(AiWorkerIsolationTest, StubModeBodypartRecognizeFallsBackGracefully) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, storage);
    char label[64] = {};
    float conf = 0.0f;

    // In stub mode, worker does not exist. Function must not crash.
    XpeErrorCode ec = xpe_bodypart_recognize(&img, label, sizeof(label), &conf);
    EXPECT_EQ(ec, XPE_ERR_PROCESSING_FAILED);

    // Label should still be written (UNKNOWN in stub mode)
    EXPECT_STREQ(label, "UNKNOWN");
}

TEST_F(AiWorkerIsolationTest, StubModeStitchImagesFallsBackGracefully) {
    std::vector<uint16_t> s1, s2, s3;
    XpeImageBuffer parts[2];
    parts[0] = makeTestBuffer(256, 512, s1);
    parts[1] = makeTestBuffer(256, 512, s2);
    XpeImageBuffer out = makeTestBuffer(1024, 512, s3);

    XpeErrorCode ec = xpe_stitch_images(parts, 2, &out, nullptr);
    EXPECT_EQ(ec, XPE_ERR_PROCESSING_FAILED);
}

TEST_F(AiWorkerIsolationTest, StubModeBoneSuppressFallsBackGracefully) {
    std::vector<uint16_t> in_storage, out_storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, in_storage);
    XpeImageBuffer out = makeTestBuffer(64, 64, out_storage);

    XpeErrorCode ec = xpe_bone_suppress(&img, &out, nullptr);
    EXPECT_EQ(ec, XPE_ERR_PROCESSING_FAILED);
}

TEST_F(AiWorkerIsolationTest, StubModeDlDenoiseFallsBackGracefully) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, storage);
    XpeImageMetadata meta{};
    std::strcpy(meta.bodyPart, "CHEST");

    XpeErrorCode ec = xpe_dl_denoise(&img, &meta, nullptr);
    EXPECT_EQ(ec, XPE_ERR_PROCESSING_FAILED);
}

/* ============================================================================
 * Repeated Fallback Consistency
 *
 * Multiple calls should consistently fall back without state corruption.
 * ============================================================================ */

TEST_F(AiWorkerIsolationTest, RepeatedFallbackIsConsistent) {
    std::vector<uint16_t> storage;
    XpeImageBuffer img = makeTestBuffer(64, 64, storage);

    for (int i = 0; i < 50; ++i) {
        char label[64] = {};
        float conf = 1.0f;
        XpeErrorCode ec = xpe_bodypart_recognize(&img, label, sizeof(label),
                                                  &conf);
        ASSERT_EQ(ec, XPE_ERR_PROCESSING_FAILED);
        ASSERT_STREQ(label, "UNKNOWN");
        ASSERT_FLOAT_EQ(conf, 0.0f);
    }
}

/* ============================================================================
 * Thread Safety: Concurrent Fallback Calls
 *
 * Multiple threads calling AI functions simultaneously should all fall back
 * safely without data races or crashes.
 * ============================================================================ */

TEST_F(AiWorkerIsolationTest, ConcurrentBodypartRecognizeIsThreadSafe) {
    constexpr int kNumThreads = 4;
    constexpr int kCallsPerThread = 25;

    std::atomic<int> successCount{0};
    std::atomic<int> fallbackCount{0};

    auto worker = [&]() {
        for (int i = 0; i < kCallsPerThread; ++i) {
            std::vector<uint16_t> storage;
            XpeImageBuffer img = makeTestBuffer(64, 64, storage);
            char label[64] = {};
            float conf = 0.0f;

            XpeErrorCode ec = xpe_bodypart_recognize(&img, label, sizeof(label),
                                                      &conf);

            if (ec == XPE_ERR_PROCESSING_FAILED) {
                fallbackCount.fetch_add(1, std::memory_order_relaxed);
            } else if (ec == XPE_OK) {
                successCount.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(kNumThreads);
    for (int t = 0; t < kNumThreads; ++t) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    // In stub mode, all calls should fall back
    EXPECT_EQ(fallbackCount.load(), kNumThreads * kCallsPerThread);
    EXPECT_EQ(successCount.load(), 0);
}

TEST_F(AiWorkerIsolationTest, ConcurrentSetFallbackModeIsThreadSafe) {
    constexpr int kNumThreads = 4;
    constexpr int kTogglesPerThread = 100;

    std::atomic<int> okCount{0};

    auto worker = [&]() {
        for (int i = 0; i < kTogglesPerThread; ++i) {
            XpeErrorCode ec = xpe_ai_set_fallback_mode(i % 2);
            if (ec == XPE_OK) {
                okCount.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(kNumThreads);
    for (int t = 0; t < kNumThreads; ++t) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    // All toggles should succeed (set_fallback_mode only modifies an atomic)
    EXPECT_EQ(okCount.load(), kNumThreads * kTogglesPerThread);
}

/* ============================================================================
 * Protocol Constants Validation
 * ============================================================================ */

TEST_F(AiWorkerIsolationTest, ProtocolVersionIsDefined) {
    EXPECT_GT(XPE_AI_PROTOCOL_VERSION_MAJOR, 0);
    EXPECT_GE(XPE_AI_PROTOCOL_VERSION_MINOR, 0);
}

TEST_F(AiWorkerIsolationTest, MessageMagicIsValid) {
    EXPECT_EQ(XPE_AI_MSG_MAGIC, 0x58504541u);  // "XPEA"
}

TEST_F(AiWorkerIsolationTest, TimeoutDefaultIsPositive) {
    EXPECT_GT(XPE_AI_DEFAULT_TIMEOUT_MS, 0u);
}

TEST_F(AiWorkerIsolationTest, MaxPayloadSizeIsReasonable) {
    // Maximum payload should be 64 MB
    EXPECT_EQ(XPE_AI_MAX_PAYLOAD_SIZE, 64u * 1024 * 1024);
}

TEST_F(AiWorkerIsolationTest, PipeBufferSizeIsPositive) {
    EXPECT_GT(XPE_AI_PIPE_BUFFER_SIZE, 0u);
}

TEST_F(AiWorkerIsolationTest, MaxModelIdLengthIsPositive) {
    EXPECT_GT(XPE_AI_MAX_MODEL_ID_LEN, 0u);
}

TEST_F(AiWorkerIsolationTest, MaxBodypartLengthIsPositive) {
    EXPECT_GT(XPE_AI_MAX_BODYPART_LEN, 0u);
}

/* ============================================================================
 * Init-Shutdown Stress (Worker Process Lifecycle Simulation)
 * ============================================================================ */

TEST_F(AiWorkerIsolationTest, RapidInitShutdownCycleDoesNotCrash) {
    // Simulate rapid worker process start/stop cycles
    for (int i = 0; i < 20; ++i) {
        xpe_ai_shutdown();
        ASSERT_EQ(xpe_ai_init("dummy_model_dir", nullptr), XPE_OK);

        // Try a quick inference call
        std::vector<uint16_t> storage;
        XpeImageBuffer img = makeTestBuffer(32, 32, storage);
        char label[64] = {};
        float conf = 0.0f;
        xpe_bodypart_recognize(&img, label, sizeof(label), &conf);
    }
    xpe_ai_shutdown();
}

TEST_F(AiWorkerIsolationTest, InitShutdownWithInferenceInBetween) {
    // Full lifecycle: init -> inference -> shutdown -> repeat
    for (int cycle = 0; cycle < 5; ++cycle) {
        xpe_ai_shutdown();
        ASSERT_EQ(xpe_ai_init("dummy_model_dir", nullptr), XPE_OK);

        // Multiple inference calls between init/shutdown
        for (int call = 0; call < 5; ++call) {
            std::vector<uint16_t> storage;
            XpeImageBuffer img = makeTestBuffer(64, 64, storage);
            char label[64] = {};
            float conf = 0.0f;

            XpeErrorCode ec = xpe_bodypart_recognize(&img, label,
                                                      sizeof(label), &conf);
            EXPECT_EQ(ec, XPE_ERR_PROCESSING_FAILED);
        }
    }
    xpe_ai_shutdown();
}
