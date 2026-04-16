/**
 * @file test_xpe_preprocess.cpp
 * @brief Unit tests for XPE Preprocessing Module (SPEC-XPE-P1A)
 *
 * Test Structure:
 * - Phase 1: Lifecycle (init/shutdown)
 * - Phase 2: Calibration Loading (offset/gain/defect map loading)
 * - Phase 3: Correction Algorithms (offset/gain/defect correction)
 * - Phase 4: Calibration Management (generation/expiry/save)
 * - Phase 5: Utilities (runtime detection/parameter query)
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "xpe/preprocess_api.h"

using namespace std::chrono_literals;

/**
 * @test Phase 1 - Module Initialization
 *
 * REQ-P1A-001: Module Initialization
 * REQ-P1A-002: P/Invoke ABI Compliance
 * REQ-P1A-020: Not-Initialized Guard
 * AC-LC-001: Initialization with Default Config
 * AC-LC-002: Initialization with Valid JSON Config
 * AC-LC-003: Double-Init Guard
 */
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

/**
 * @test AC-LC-001: Initialization with Default Config (NULL config)
 *
 * Given xpe_preprocess.dll is loaded
 * When xpe_preprocess_init(NULL) is called
 * Then the function returns XPE_OK
 * And the module enters initialized state
 */
TEST_F(PreprocessLifecycleTest, LC001_InitWithDefaultConfig) {
    // Act
    XpeErrorCode result = xpe_preprocess_init(NULL);

    // Assert
    EXPECT_EQ(result, XPE_OK);

    // Verify initialized state by checking that processing functions
    // would return XPE_ERR_NOT_INITIALIZED if we hadn't initialized
    // (This will be tested separately in AC-ERR-001)
}

/**
 * @test AC-LC-002: Initialization with Valid JSON Config
 *
 * Given xpe_preprocess.dll is loaded
 * When xpe_preprocess_init("{\"mode\":\"clinical\"}") is called
 * Then the function returns XPE_OK
 * And clinical mode is activated
 */
TEST_F(PreprocessLifecycleTest, LC002_InitWithJsonConfig) {
    // Arrange
    const char* config = "{\"mode\":\"clinical\"}";

    // Act
    XpeErrorCode result = xpe_preprocess_init(config);

    // Assert
    EXPECT_EQ(result, XPE_OK);
}

/**
 * @test AC-LC-002: Initialization with Empty JSON Config
 *
 * Given xpe_preprocess.dll is loaded
 * When xpe_preprocess_init("{}") is called
 * Then the function returns XPE_OK
 */
TEST_F(PreprocessLifecycleTest, LC002_InitWithEmptyJsonConfig) {
    // Arrange
    const char* config = "{}";

    // Act
    XpeErrorCode result = xpe_preprocess_init(config);

    // Assert
    EXPECT_EQ(result, XPE_OK);
}

/**
 * @test AC-LC-003: Double-Init Guard
 *
 * Given xpe_preprocess.dll is initialized
 * When xpe_preprocess_init() is called again without shutdown
 * Then the function returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessLifecycleTest, LC003_DoubleInitGuard) {
    // Arrange: First initialization
    XpeErrorCode first_init = xpe_preprocess_init(NULL);
    ASSERT_EQ(first_init, XPE_OK);

    // Act: Second initialization without shutdown
    XpeErrorCode second_init = xpe_preprocess_init(NULL);

    // Assert
    EXPECT_EQ(second_init, XPE_ERR_INVALID_INPUT);
}

/**
 * @test Initialization with Invalid JSON Config
 *
 * Given xpe_preprocess.dll is loaded
 * When xpe_preprocess_init("{invalid json}") is called
 * Then the function returns XPE_ERR_CONFIG_INVALID
 */
TEST_F(PreprocessLifecycleTest, InitWithInvalidJson) {
    // Arrange
    const char* invalid_config = "{invalid json}";

    // Act
    XpeErrorCode result = xpe_preprocess_init(invalid_config);

    // Assert
    EXPECT_EQ(result, XPE_ERR_CONFIG_INVALID);
}

/**
 * @test Shutdown Without Initialization
 *
 * Given xpe_preprocess.dll is loaded but not initialized
 * When xpe_preprocess_shutdown() is called
 * Then the function should handle gracefully (no crash)
 */
TEST_F(PreprocessLifecycleTest, ShutdownWithoutInit) {
    // Act: Should not crash
    xpe_preprocess_shutdown();

    // Assert: No assertion means success
    SUCCEED();
}

/**
 * @test Normal Shutdown After Initialization
 *
 * Given xpe_preprocess.dll is initialized
 * When xpe_preprocess_shutdown() is called
 * Then the module returns to uninitialized state cleanly
 */
TEST_F(PreprocessLifecycleTest, NormalShutdown) {
    // Arrange: Initialize first
    XpeErrorCode init_result = xpe_preprocess_init(NULL);
    ASSERT_EQ(init_result, XPE_OK);

    // Act: Shutdown
    xpe_preprocess_shutdown();

    // Assert: Should be able to initialize again
    XpeErrorCode reinit_result = xpe_preprocess_init(NULL);
    EXPECT_EQ(reinit_result, XPE_OK);
}

/**
 * @test REQ-P1A-003: Thread Safety - Concurrent Init
 *
 * Given multiple threads
 * When they call xpe_preprocess_init() concurrently
 * Then only one should succeed, others should fail
 */
TEST_F(PreprocessLifecycleTest, ThreadSafety_ConcurrentInit) {
    const int num_threads = 10;
    std::vector<XpeErrorCode> results(num_threads);
    std::vector<std::thread> threads;

    // Act: Launch concurrent initializations
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            results[i] = xpe_preprocess_init(NULL);
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    // Assert: Exactly one should succeed
    int success_count = 0;
    for (auto result : results) {
        if (result == XPE_OK) {
            success_count++;
        }
    }

    EXPECT_EQ(success_count, 1);
}

/**
 * @test REQ-P1A-003: Thread Safety - Init During Active Processing
 *
 * Given module is initialized
 * When one thread calls init() while another calls shutdown()
 * Then both operations should complete without deadlock
 */
TEST_F(PreprocessLifecycleTest, ThreadSafety_InitShutdownConcurrent) {
    // Arrange: Initialize first
    XpeErrorCode init_result = xpe_preprocess_init(NULL);
    ASSERT_EQ(init_result, XPE_OK);

    bool init_done = false;
    bool shutdown_done = false;

    // Act: Concurrent init and shutdown
    std::thread init_thread([&]() {
        // This should fail with XPE_ERR_INVALID_INPUT
        xpe_preprocess_init(NULL);
        init_done = true;
    });

    std::thread shutdown_thread([&]() {
        xpe_preprocess_shutdown();
        shutdown_done = true;
    });

    init_thread.join();
    shutdown_thread.join();

    // Assert: Both should complete without deadlock
    EXPECT_TRUE(init_done);
    EXPECT_TRUE(shutdown_done);
}

/**
 * @test REQ-P1A-031: Memory Leak - 100 Cycle Init/Shutdown
 *
 * Given 100 initialization/shutdown cycles
 * When executed in sequence
 * Then no memory leaks should occur
 */
TEST_F(PreprocessLifecycleTest, MemoryLeak_NoLeakAfter100Cycles) {
    const int cycles = 100;

    // Act: Run 100 init/shutdown cycles
    for (int i = 0; i < cycles; ++i) {
        XpeErrorCode result = xpe_preprocess_init(NULL);
        EXPECT_EQ(result, XPE_OK);
        xpe_preprocess_shutdown();
    }

    // Assert: No crash means success
    // Memory leak detection should be done with external tools (Valgrind, Dr. Memory)
    SUCCEED();
}

// Phase 2 tests will be added here
// Phase 3 tests will be added here
// Phase 4 tests will be added here
// Phase 5 tests will be added here

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
