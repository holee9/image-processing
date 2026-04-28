/**
 * @file test_ai_ipc_bridge.cpp
 * @brief Google Test suite for XPE AI named pipe IPC bridge (T-001).
 *
 * Tests the IPC bridge between xpe_ai.dll (proxy) and xpe_ai_worker.exe.
 * Covers REQ-AI-003 (Worker-isolated architecture).
 *
 * Test categories:
 *   1. Connection lifecycle (create, connect, disconnect)
 *   2. Message send/receive with protocol validation
 *   3. Timeout handling
 *   4. Error handling (worker not available, pipe broken)
 *
 * @ingroup xpe_ai_tests
 */

#include <gtest/gtest.h>
#include <xpe/ai/ai_worker_protocol.h>
#include <xpe/common/xpe_error.h>
#include <thread>
#include <chrono>
#include <string>
#include <windows.h>  // For GetCurrentProcessId()

// Forward declarations for IPC bridge (to be implemented)
#ifdef __cplusplus
extern "C" {
#endif

typedef struct XpeAiIpcBridge XpeAiIpcBridge;

/**
 * @brief Create an IPC bridge instance.
 *
 * @param pipe_name Named pipe name (e.g., "\\\\.\\pipe\\xpe_ai_worker_12345")
 * @param timeout_ms Timeout in milliseconds for operations.
 * @return New bridge instance, or NULL on allocation failure.
 */
XPE_API XpeAiIpcBridge* xpe_ai_ipc_bridge_create(const char* pipe_name, uint32_t timeout_ms);

/**
 * @brief Connect to the worker process via named pipe.
 *
 * @param bridge Bridge instance.
 * @return XPE_OK on success, XPE_ERR_PROCESSING_FAILED on timeout.
 */
XPE_API XpeErrorCode xpe_ai_ipc_bridge_connect(XpeAiIpcBridge* bridge);

/**
 * @brief Send a message to the worker.
 *
 * @param bridge Bridge instance.
 * @param header Message header (32 bytes).
 * @param payload JSON payload (can be NULL for zero-length messages).
 * @param payload_size Size of payload in bytes.
 * @return XPE_OK on success, error code on failure.
 */
XPE_API XpeErrorCode xpe_ai_ipc_bridge_send(XpeAiIpcBridge* bridge,
                                             const XpeAiMessageHeader* header,
                                             const void* payload,
                                             uint32_t payload_size);

/**
 * @brief Receive a message from the worker.
 *
 * @param bridge Bridge instance.
 * @param header_out Output buffer for message header (32 bytes).
 * @param payload_out Output buffer for JSON payload (caller-allocated).
 * @param payload_size Size of payload_out buffer.
 * @param bytes_received Output: actual bytes received (including header).
 * @return XPE_OK on success, XPE_ERR_PROCESSING_FAILED on timeout.
 */
XPE_API XpeErrorCode xpe_ai_ipc_bridge_receive(XpeAiIpcBridge* bridge,
                                                XpeAiMessageHeader* header_out,
                                                void* payload_out,
                                                uint32_t payload_size,
                                                uint32_t* bytes_received);

/**
 * @brief Disconnect and destroy the IPC bridge.
 *
 * @param bridge Bridge instance (will be set to NULL).
 */
XPE_API void xpe_ai_ipc_bridge_destroy(XpeAiIpcBridge* bridge);

#ifdef __cplusplus
}
#endif

// ============================================================================
// Test Fixture
// ============================================================================

class AiIpcBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use unique pipe name per test to avoid conflicts
        pipe_name_ = "\\\\.\\pipe\\xpe_ai_test_" + std::to_string(static_cast<int>(GetCurrentProcessId())) +
                     "_" + std::to_string(test_counter_++);
        timeout_ms_ = 1000;  // 1 second timeout for tests
    }

    void TearDown() override {
        if (bridge_) {
            xpe_ai_ipc_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    std::string pipe_name_;
    uint32_t timeout_ms_;
    XpeAiIpcBridge* bridge_{nullptr};
    static inline int test_counter_{0};
};

// ============================================================================
// Test Cases: Connection Lifecycle (REQ-AI-003)
// ============================================================================

/**
 * @test T-001-001: Bridge creation succeeds with valid parameters.
 */
TEST_F(AiIpcBridgeTest, CreateBridge_ValidParameters_Succeeds) {
    // Arrange & Act
    bridge_ = xpe_ai_ipc_bridge_create(pipe_name_.c_str(), timeout_ms_);

    // Assert
    ASSERT_NE(bridge_, nullptr) << "IPC bridge creation should succeed with valid pipe name";
}

/**
 * @test T-001-002: Bridge creation fails with NULL pipe name.
 */
TEST_F(AiIpcBridgeTest, CreateBridge_NullPipeName_ReturnsNull) {
    // Arrange & Act
    bridge_ = xpe_ai_ipc_bridge_create(nullptr, timeout_ms_);

    // Assert
    EXPECT_EQ(bridge_, nullptr) << "IPC bridge creation should fail with NULL pipe name";
}

/**
 * @test T-001-003: Connection fails when worker is not available (timeout).
 */
TEST_F(AiIpcBridgeTest, Connect_NoWorkerAvailable_ReturnsTimeoutError) {
    // Arrange
    bridge_ = xpe_ai_ipc_bridge_create(pipe_name_.c_str(), timeout_ms_);
    ASSERT_NE(bridge_, nullptr);

    // Act
    XpeErrorCode result = xpe_ai_ipc_bridge_connect(bridge_);

    // Assert
    EXPECT_EQ(result, XPE_ERR_PROCESSING_FAILED)
        << "Connection should fail with timeout when worker is not available";
}

/**
 * @test T-001-004: Send fails when not connected.
 */
TEST_F(AiIpcBridgeTest, Send_NotConnected_ReturnsError) {
    // Arrange
    bridge_ = xpe_ai_ipc_bridge_create(pipe_name_.c_str(), timeout_ms_);
    ASSERT_NE(bridge_, nullptr);

    XpeAiMessageHeader header{};
    header.magic = XPE_AI_MSG_MAGIC;
    header.version = (XPE_AI_PROTOCOL_VERSION_MAJOR << 16) | XPE_AI_PROTOCOL_VERSION_MINOR;
    header.messageType = XPE_AI_MSG_INIT;
    header.requestId = 1;
    header.payloadSize = 0;

    // Act
    XpeErrorCode result = xpe_ai_ipc_bridge_send(bridge_, &header, nullptr, 0);

    // Assert
    EXPECT_NE(result, XPE_OK) << "Send should fail when bridge is not connected";
}

/**
 * @test T-001-005: Receive fails when not connected.
 */
TEST_F(AiIpcBridgeTest, Receive_NotConnected_ReturnsError) {
    // Arrange
    bridge_ = xpe_ai_ipc_bridge_create(pipe_name_.c_str(), timeout_ms_);
    ASSERT_NE(bridge_, nullptr);

    XpeAiMessageHeader header_out{};
    char payload_buffer[256];
    uint32_t bytes_received = 0;

    // Act
    XpeErrorCode result = xpe_ai_ipc_bridge_receive(bridge_, &header_out,
                                                      payload_buffer, sizeof(payload_buffer),
                                                      &bytes_received);

    // Assert
    EXPECT_NE(result, XPE_OK) << "Receive should fail when bridge is not connected";
}

// ============================================================================
// Test Cases: Protocol Validation
// ============================================================================

/**
 * @test T-001-006: Send validates magic number in header.
 */
TEST_F(AiIpcBridgeTest, Send_InvalidMagicNumber_ReturnsInvalidInput) {
    // Arrange
    bridge_ = xpe_ai_ipc_bridge_create(pipe_name_.c_str(), timeout_ms_);
    ASSERT_NE(bridge_, nullptr);

    XpeAiMessageHeader header{};
    header.magic = 0xdeadbeef;  // Invalid magic number
    header.version = (XPE_AI_PROTOCOL_VERSION_MAJOR << 16) | XPE_AI_PROTOCOL_VERSION_MINOR;
    header.messageType = XPE_AI_MSG_INIT;
    header.requestId = 1;
    header.payloadSize = 0;

    // Act
    XpeErrorCode result = xpe_ai_ipc_bridge_send(bridge_, &header, nullptr, 0);

    // Assert
    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT) << "Send should validate magic number";
}

/**
 * @test T-001-007: Send validates payload size against maximum.
 */
TEST_F(AiIpcBridgeTest, Send_PayloadSizeExceedsMaximum_ReturnsInvalidInput) {
    // Arrange
    bridge_ = xpe_ai_ipc_bridge_create(pipe_name_.c_str(), timeout_ms_);
    ASSERT_NE(bridge_, nullptr);

    XpeAiMessageHeader header{};
    header.magic = XPE_AI_MSG_MAGIC;
    header.version = (XPE_AI_PROTOCOL_VERSION_MAJOR << 16) | XPE_AI_PROTOCOL_VERSION_MINOR;
    header.messageType = XPE_AI_MSG_INIT;
    header.requestId = 1;
    header.payloadSize = XPE_AI_MAX_PAYLOAD_SIZE + 1;  // Exceed maximum

    // Act
    XpeErrorCode result = xpe_ai_ipc_bridge_send(bridge_, &header, nullptr, 0);

    // Assert
    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT) << "Send should validate payload size";
}

// ============================================================================
// Test Cases: Timeout Handling (REQ-AI-009)
// ============================================================================

/**
 * @test T-001-008: Receive returns timeout error when no response within timeout.
 */
TEST_F(AiIpcBridgeTest, Receive_NoResponseWithinTimeout_ReturnsTimeoutError) {
    // This test requires a mock worker that doesn't respond.
    // For now, we test the timeout path by not connecting at all.
    // Arrange
    bridge_ = xpe_ai_ipc_bridge_create(pipe_name_.c_str(), 100);  // 100ms timeout
    ASSERT_NE(bridge_, nullptr);

    XpeAiMessageHeader header_out{};
    char payload_buffer[256];
    uint32_t bytes_received = 0;

    // Act
    XpeErrorCode result = xpe_ai_ipc_bridge_receive(bridge_, &header_out,
                                                      payload_buffer, sizeof(payload_buffer),
                                                      &bytes_received);

    // Assert
    EXPECT_EQ(result, XPE_ERR_PROCESSING_FAILED)
        << "Receive should timeout when no worker responds";
}

// ============================================================================
// Test Cases: Resource Cleanup
// ============================================================================

/**
 * @test T-001-009: Destroy handles NULL bridge gracefully.
 */
TEST_F(AiIpcBridgeTest, Destroy_NullBridge_DoesNotCrash) {
    // Arrange & Act & Assert
    xpe_ai_ipc_bridge_destroy(nullptr);
    SUCCEED();
}

/**
 * @test T-001-010: Multiple destroy calls are safe.
 */
TEST_F(AiIpcBridgeTest, Destroy_MultipleCalls_Safe) {
    // Arrange
    bridge_ = xpe_ai_ipc_bridge_create(pipe_name_.c_str(), timeout_ms_);
    ASSERT_NE(bridge_, nullptr);

    // Act & Assert
    xpe_ai_ipc_bridge_destroy(bridge_);
    bridge_ = nullptr;

    // Second destroy should be safe
    xpe_ai_ipc_bridge_destroy(bridge_);
    SUCCEED();
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
