/**
 * @file test_xpe_preprocess_init.cpp
 * @brief Lifecycle tests for XPE Preprocessing Module
 *
 * Tests module initialization and shutdown behavior.
 * Covers REQ-P1A-001, REQ-P1A-002, REQ-P1A-003, REQ-P1A-020.
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <cstdio>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "fixtures/make_xcal.hpp"

using namespace std::chrono_literals;

// =============================================================================
// Test Fixtures
// =============================================================================

class PreprocessLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure clean state before each test
        xpe_preprocess_shutdown();
    }

    void TearDown() override {
        // Clean state after each test
        xpe_preprocess_shutdown();
    }
};

// =============================================================================
// Initialization Tests
// =============================================================================

/**
 * @test LC001_InitWithDefaultConfig
 *
 * Given xpe_preprocess.dll is loaded
 * When xpe_preprocess_init(NULL) is called
 * Then the function returns XPE_OK
 * And the module enters initialized state
 */
TEST_F(PreprocessLifecycleTest, LC001_InitWithDefaultConfig) {
    XpeErrorCode result = xpe_preprocess_init(NULL);
    EXPECT_EQ(result, XPE_OK);
}

/**
 * @test LC002_InitWithJsonConfig
 *
 * Given xpe_preprocess.dll is loaded
 * When xpe_preprocess_init("{\"mode\":\"clinical\"}") is called
 * Then the function returns XPE_OK
 */
TEST_F(PreprocessLifecycleTest, LC002_InitWithJsonConfig) {
    const char* config = "{\"mode\":\"clinical\"}";
    XpeErrorCode result = xpe_preprocess_init(config);
    EXPECT_EQ(result, XPE_OK);
}

/**
 * @test LC002_InitWithEmptyJsonConfig
 *
 * Given xpe_preprocess.dll is loaded
 * When xpe_preprocess_init("{}") is called
 * Then the function returns XPE_OK
 */
TEST_F(PreprocessLifecycleTest, LC002_InitWithEmptyJsonConfig) {
    const char* config = "{}";
    XpeErrorCode result = xpe_preprocess_init(config);
    EXPECT_EQ(result, XPE_OK);
}

/**
 * @test LC002_InitWithResearchMode
 *
 * Given xpe_preprocess.dll is loaded
 * When xpe_preprocess_init("{\"mode\":\"research\"}") is called
 * Then the function returns XPE_OK
 */
TEST_F(PreprocessLifecycleTest, LC002_InitWithResearchMode) {
    const char* config = "{\"mode\":\"research\"}";
    XpeErrorCode result = xpe_preprocess_init(config);
    EXPECT_EQ(result, XPE_OK);
}

/**
 * @test LC002_InitWithLogLevel
 *
 * Given xpe_preprocess.dll is loaded
 * When xpe_preprocess_init("{\"log_level\":2}") is called
 * Then the function returns XPE_OK
 */
TEST_F(PreprocessLifecycleTest, LC002_InitWithLogLevel) {
    const char* config = "{\"log_level\":2}";
    XpeErrorCode result = xpe_preprocess_init(config);
    EXPECT_EQ(result, XPE_OK);
}

/**
 * @test LC003_DoubleInitGuard
 *
 * Given xpe_preprocess.dll is initialized
 * When xpe_preprocess_init() is called again without shutdown
 * Then the function returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessLifecycleTest, LC003_DoubleInitGuard) {
    XpeErrorCode first_init = xpe_preprocess_init(NULL);
    ASSERT_EQ(first_init, XPE_OK);

    XpeErrorCode second_init = xpe_preprocess_init(NULL);
    EXPECT_EQ(second_init, XPE_ERR_INVALID_INPUT);
}

/**
 * @test InitWithInvalidJson
 *
 * Given xpe_preprocess.dll is loaded
 * When xpe_preprocess_init("{invalid json}") is called
 * Then the function returns XPE_ERR_CONFIG_INVALID
 */
TEST_F(PreprocessLifecycleTest, InitWithInvalidJson) {
    const char* invalid_config = "{invalid json}";
    XpeErrorCode result = xpe_preprocess_init(invalid_config);
    EXPECT_EQ(result, XPE_ERR_CONFIG_INVALID);
}

/**
 * @test InitWithMalformedJson
 *
 * Given xpe_preprocess.dll is loaded
 * When xpe_preprocess_init("{\"mode\":}") is called (malformed JSON)
 * Then the function returns XPE_ERR_CONFIG_INVALID
 */
TEST_F(PreprocessLifecycleTest, InitWithMalformedJson) {
    const char* malformed_config = "{\"mode\":}";
    XpeErrorCode result = xpe_preprocess_init(malformed_config);
    EXPECT_EQ(result, XPE_ERR_CONFIG_INVALID);
}

// =============================================================================
// Shutdown Tests
// =============================================================================

/**
 * @test ShutdownWithoutInit
 *
 * Given xpe_preprocess.dll is loaded but not initialized
 * When xpe_preprocess_shutdown() is called
 * Then the function should handle gracefully (no crash)
 */
TEST_F(PreprocessLifecycleTest, ShutdownWithoutInit) {
    // Should not crash
    xpe_preprocess_shutdown();
    SUCCEED();
}

/**
 * @test NormalShutdown
 *
 * Given xpe_preprocess.dll is initialized
 * When xpe_preprocess_shutdown() is called
 * Then the module returns to uninitialized state cleanly
 */
TEST_F(PreprocessLifecycleTest, NormalShutdown) {
    XpeErrorCode init_result = xpe_preprocess_init(NULL);
    ASSERT_EQ(init_result, XPE_OK);

    xpe_preprocess_shutdown();

    // Should be able to initialize again
    XpeErrorCode reinit_result = xpe_preprocess_init(NULL);
    EXPECT_EQ(reinit_result, XPE_OK);
}

/**
 * @test MultipleShutdownCalls
 *
 * Given xpe_preprocess.dll is initialized
 * When xpe_preprocess_shutdown() is called multiple times
 * Then all calls should complete without error
 */
TEST_F(PreprocessLifecycleTest, MultipleShutdownCalls) {
    XpeErrorCode init_result = xpe_preprocess_init(NULL);
    ASSERT_EQ(init_result, XPE_OK);

    // Multiple shutdowns should be safe
    xpe_preprocess_shutdown();
    xpe_preprocess_shutdown();
    xpe_preprocess_shutdown();

    SUCCEED();
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

/**
 * @test ThreadSafety_ConcurrentInit
 *
 * Given multiple threads
 * When they call xpe_preprocess_init() concurrently
 * Then only one should succeed, others should fail
 */
TEST_F(PreprocessLifecycleTest, ThreadSafety_ConcurrentInit) {
    const int num_threads = 10;
    std::vector<XpeErrorCode> results(num_threads);
    std::vector<std::thread> threads;

    // Launch concurrent initializations
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            results[i] = xpe_preprocess_init(NULL);
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    // Exactly one should succeed
    int success_count = 0;
    for (auto result : results) {
        if (result == XPE_OK) {
            success_count++;
        }
    }

    EXPECT_EQ(success_count, 1);
}

/**
 * @test ThreadSafety_InitShutdownConcurrent
 *
 * Given module is initialized
 * When one thread calls init() while another calls shutdown()
 * Then both operations should complete without deadlock
 */
TEST_F(PreprocessLifecycleTest, ThreadSafety_InitShutdownConcurrent) {
    XpeErrorCode init_result = xpe_preprocess_init(NULL);
    ASSERT_EQ(init_result, XPE_OK);

    bool init_done = false;
    bool shutdown_done = false;

    // Concurrent init and shutdown
    std::thread init_thread([&]() {
        xpe_preprocess_init(NULL);  // Should fail with XPE_ERR_INVALID_INPUT
        init_done = true;
    });

    std::thread shutdown_thread([&]() {
        xpe_preprocess_shutdown();
        shutdown_done = true;
    });

    init_thread.join();
    shutdown_thread.join();

    // Both should complete without deadlock
    EXPECT_TRUE(init_done);
    EXPECT_TRUE(shutdown_done);
}

/**
 * @test ThreadSafety_MultipleShutdownConcurrent
 *
 * Given module is initialized
 * When multiple threads call shutdown() concurrently
 * Then all should complete without deadlock
 */
TEST_F(PreprocessLifecycleTest, ThreadSafety_MultipleShutdownConcurrent) {
    XpeErrorCode init_result = xpe_preprocess_init(NULL);
    ASSERT_EQ(init_result, XPE_OK);

    const int num_threads = 5;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            xpe_preprocess_shutdown();
        });
    }

    // All should complete without deadlock
    for (auto& t : threads) {
        t.join();
    }

    SUCCEED();
}

// =============================================================================
// Reinitialization Tests
// =============================================================================

/**
 * @test ReinitializationAfterShutdown
 *
 * Given module is initialized and shutdown
 * When xpe_preprocess_init() is called again
 * Then the function returns XPE_OK
 */
TEST_F(PreprocessLifecycleTest, ReinitializationAfterShutdown) {
    // First initialization
    XpeErrorCode init1 = xpe_preprocess_init(NULL);
    ASSERT_EQ(init1, XPE_OK);

    // Shutdown
    xpe_preprocess_shutdown();

    // Second initialization
    XpeErrorCode init2 = xpe_preprocess_init(NULL);
    EXPECT_EQ(init2, XPE_OK);
}

/**
 * @test MultipleInitShutdownCycles
 *
 * Given module supports multiple cycles
 * When init/shutdown is called repeatedly
 * Then all cycles should complete successfully
 */
TEST_F(PreprocessLifecycleTest, MultipleInitShutdownCycles) {
    const int num_cycles = 5;

    for (int i = 0; i < num_cycles; ++i) {
        XpeErrorCode init_result = xpe_preprocess_init(NULL);
        EXPECT_EQ(init_result, XPE_OK);

        xpe_preprocess_shutdown();
    }

    SUCCEED();
}

// =============================================================================
// Configuration Tests
// =============================================================================

/**
 * @test InitWithDifferentModes
 *
 * Given module supports different modes
 * When initialized with different mode values
 * Then all should return XPE_OK
 */
TEST_F(PreprocessLifecycleTest, InitWithDifferentModes) {
    // Test default mode
    XpeErrorCode result1 = xpe_preprocess_init(NULL);
    EXPECT_EQ(result1, XPE_OK);
    xpe_preprocess_shutdown();

    // Test clinical mode
    XpeErrorCode result2 = xpe_preprocess_init("{\"mode\":\"clinical\"}");
    EXPECT_EQ(result2, XPE_OK);
    xpe_preprocess_shutdown();

    // Test research mode
    XpeErrorCode result3 = xpe_preprocess_init("{\"mode\":\"research\"}");
    EXPECT_EQ(result3, XPE_OK);
    xpe_preprocess_shutdown();
}

/**
 * @test InitWithComplexConfig
 *
 * Given module supports multiple config parameters
 * When initialized with complex JSON config
 * Then the function returns XPE_OK
 */
TEST_F(PreprocessLifecycleTest, InitWithComplexConfig) {
    const char* config = "{\"mode\":\"clinical\",\"log_level\":1,\"debug\":true}";
    XpeErrorCode result = xpe_preprocess_init(config);
    EXPECT_EQ(result, XPE_OK);
}

// =============================================================================
// Shutdown on an uninitialized module (QA-A-90, #176)
//
// None of the five calibration loader sources checks the initialized flag, so a
// map can be loaded before xpe_preprocess_init. These two cases pin what
// xpe_preprocess_shutdown then does: it releases that map although the module
// was never initialized. The public header used to call such a shutdown a
// no-op; the fixture above already relied on the opposite ("ensure clean state"
// before any init), and the release is what a shutdown documented to "release
// resources" (REQ-P1A-031) has to do.
//
// The pair is the evidence: the control shows the map WOULD still be there at
// init, so the shutdown in between is what removed it.
// =============================================================================

namespace {

constexpr uint32_t kShutdownW = 4;
constexpr uint32_t kShutdownH = 4;
const char* const  kShutdownOffsetPath = "qa_a90_shutdown_offset.xcal";

// Offset-corrects one kShutdownW x kShutdownH frame and returns the code.
XpeErrorCode CorrectOneSmallFrame() {
    std::vector<uint16_t> in(static_cast<size_t>(kShutdownW) * kShutdownH, 1000u);
    std::vector<uint16_t> out(in.size(), 0u);
    XpeImageBuffer inBuf{};
    inBuf.data = in.data();
    inBuf.width = kShutdownW;
    inBuf.height = kShutdownH;
    inBuf.bitsAllocated = 16;
    inBuf.bitsStored = 16;
    inBuf.format = XPE_PIXEL_UINT16;
    inBuf.dataSize = in.size() * sizeof(uint16_t);
    XpeImageBuffer outBuf = inBuf;
    outBuf.data = out.data();
    XpeImageMetadata meta{};
    return xpe_offset_correct(&inBuf, &outBuf, &meta);
}

}  // namespace

TEST_F(PreprocessLifecycleTest, ShutdownWithoutInitReleasesMapsLoadedWithoutInit) {
    // The fixture's SetUp already shut down; initialize and shut down once more
    // so the starting clear does not rely on the behaviour under test.
    (void)xpe_preprocess_init(NULL);
    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, MakeOffsetXCal(kShutdownOffsetPath, kShutdownW, kShutdownH, 1.0f));

    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(kShutdownOffsetPath))
        << "precondition: a map can be loaded before xpe_preprocess_init";

    xpe_preprocess_shutdown();   // the module was never initialized

    ASSERT_EQ(XPE_OK, xpe_preprocess_init(NULL));
    EXPECT_EQ(XPE_ERR_CALIB_NOT_LOADED, CorrectOneSmallFrame())
        << "shutdown on an uninitialized module must release the map loaded before it";

    std::remove(kShutdownOffsetPath);
}

TEST_F(PreprocessLifecycleTest, MapLoadedWithoutInitSurvivesInitWithoutShutdown) {
    (void)xpe_preprocess_init(NULL);
    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, MakeOffsetXCal(kShutdownOffsetPath, kShutdownW, kShutdownH, 1.0f));

    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(kShutdownOffsetPath));

    ASSERT_EQ(XPE_OK, xpe_preprocess_init(NULL));
    EXPECT_EQ(XPE_OK, CorrectOneSmallFrame())
        << "control: with no shutdown in between, the map loaded before init is used";

    std::remove(kShutdownOffsetPath);
}

// =============================================================================
// Main Test Runner
// =============================================================================

