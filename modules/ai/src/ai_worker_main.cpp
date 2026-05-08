/**
 * @file ai_worker_main.cpp
 * @brief Worker process entry point for AI inference (REQ-AI-003, REQ-AI-006)
 * @date 2026-04-23
 *
 * TDD Implementation: T-002 Worker Process Skeleton
 *
 * Lifecycle:
 * 1. Launch: Create named pipe server
 * 2. Init: Wait for INIT message, respond with INIT_ACK
 * 3. Heartbeat: Respond to HEARTBEAT with HEARTBEAT_ACK
 * 4. Shutdown: Handle SHUTDOWN message, exit cleanly
 *
 * Build modes:
 * - STUB (default): No ONNX Runtime linked
 * - FULL: ONNX Runtime linked for actual inference
 */

// @MX:NOTE: Worker process uses Windows named pipes for IPC. Maximum message size is 512 bytes.
// @MX:NOTE: Single-client design - only one pipe instance allowed per worker.

#include <windows.h>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <memory>

// Protocol definitions
#include "xpe/ai/ai_worker_protocol.h"

// Constants
namespace {
    constexpr DWORD PIPE_BUFFER_SIZE = 512;
    constexpr DWORD PIPE_TIMEOUT_MS = 0;
    constexpr const char* STUB_VERSION_STRING = "0.1.0-stub";
    constexpr const char* XPE_AI_WORKER_PIPE_NAME = "\\\\.\\pipe\\xpe_ai_worker";

    // Stub worker protocol version
    constexpr uint32_t WORKER_PROTOCOL_VERSION = 0x00010000;  // 1.0.0

    // Stub message types for worker (simplified)
    enum class MessageType : uint32_t {
        INIT = 1,
        INIT_ACK = 2,
        HEARTBEAT = 3,
        HEARTBEAT_ACK = 4,
        SHUTDOWN = 5,
        SHUTDOWN_ACK = 6,
        ERROR_RESPONSE = 99
    };

    // Stub worker status
    enum class WorkerStatus : uint32_t {
        IDLE = 0,
        BUSY = 1,
        FAILED = 2
    };

    // Stub error codes
    enum class ErrorCode : uint32_t {
        UNKNOWN_MESSAGE = 1
    };

    // Stub worker message structure
    struct WorkerMessage {
        MessageType type;

        // Use separate members instead of anonymous union to avoid compiler issues
        struct {
            uint32_t protocol_version;
            char version[32];
            uint32_t capabilities;
        } init_ack;

        struct {
            uint32_t status;
        } heartbeat_ack;

        struct {
            uint32_t exit_code;
        } shutdown_ack;

        struct {
            uint32_t code;
            char message[256];
        } error;
    };
}

/**
 * @class WorkerServer
 * @brief Named pipe server for worker process communication
 */
class WorkerServer {
public:
    /**
     * @brief Construct worker server
     * @param pipe_name Named pipe name
     */
    explicit WorkerServer(const std::string& pipe_name)
        : pipe_name_(pipe_name)
        , pipe_handle_(INVALID_HANDLE_VALUE)
        , running_(false)
    {
    }

    /**
     * @brief Destructor - cleanup resources
     */
    ~WorkerServer() {
        Stop();
    }

    /**
     * @brief Start server and wait for client connection
     * @return true if successful
     * @MX:ANCHOR: Creates named pipe server for IPC. Called by main() once at startup.
     * @MX:REASON: Single client design - max_instances=1 prevents concurrent connections.
     */
    bool Start() {
        // Create named pipe
        pipe_handle_ = CreateNamedPipeA(
            pipe_name_.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,                          // Max instances (single client)
            PIPE_BUFFER_SIZE,           // Output buffer size
            PIPE_BUFFER_SIZE,           // Input buffer size
            PIPE_TIMEOUT_MS,            // Default timeout
            nullptr                     // Default security
        );

        if (pipe_handle_ == INVALID_HANDLE_VALUE) {
            std::cerr << "CreateNamedPipe failed: " << GetLastError() << std::endl;
            return false;
        }

        std::cout << "[Worker] Pipe created: " << pipe_name_ << std::endl;
        running_ = true;
        return true;
    }

    /**
     * @brief Wait for client to connect
     * @return true if connection successful
     */
    bool WaitForConnection() {
        if (pipe_handle_ == INVALID_HANDLE_VALUE) {
            std::cerr << "[Worker] Invalid pipe handle" << std::endl;
            return false;
        }

        std::cout << "[Worker] Waiting for client connection..." << std::endl;

        BOOL result = ConnectNamedPipe(pipe_handle_, nullptr);
        if (!result) {
            DWORD error = GetLastError();
            if (error == ERROR_PIPE_CONNECTED) {
                // Client already connected - this is OK
                std::cout << "[Worker] Client already connected" << std::endl;
                return true;
            }
            std::cerr << "[Worker] ConnectNamedPipe failed: " << error << std::endl;
            return false;
        }

        std::cout << "[Worker] Client connected" << std::endl;
        return true;
    }

    /**
     * @brief Main message loop
     */
    void Run() {
        while (running_) {
            WorkerMessage msg;
            DWORD bytes_read = 0;

            // Read message from pipe
            BOOL success = ReadFile(
                pipe_handle_,
                &msg,
                sizeof(msg),
                &bytes_read,
                nullptr
            );

            if (!success || bytes_read == 0) {
                if (running_) {
                    std::cout << "[Worker] Client disconnected or error" << std::endl;
                }
                break;
            }

            // Dispatch message
            HandleMessage(msg);
        }
    }

    /**
     * @brief Stop server and cleanup
     */
    void Stop() {
        if (running_) {
            running_ = false;

            if (pipe_handle_ != INVALID_HANDLE_VALUE) {
                FlushFileBuffers(pipe_handle_);
                DisconnectNamedPipe(pipe_handle_);
                CloseHandle(pipe_handle_);
                pipe_handle_ = INVALID_HANDLE_VALUE;
            }
        }
    }

private:
    /**
     * @brief Handle incoming message
     * @param msg Message received from client
     */
    void HandleMessage(const WorkerMessage& msg) {
        switch (msg.type) {
            case MessageType::INIT:
                HandleInit(msg);
                break;

            case MessageType::HEARTBEAT:
                HandleHeartbeat(msg);
                break;

            case MessageType::SHUTDOWN:
                HandleShutdown(msg);
                break;

            default:
                HandleUnknown(msg);
                break;
        }
    }

    /**
     * @brief Handle INIT message
     * @MX:ANCHOR: Initializes worker protocol session. Called once per client connection.
     * @MX:REASON: Protocol handshake - must respond with version and capabilities.
     */
    void HandleInit(const WorkerMessage&) {
        std::cout << "[Worker] Received INIT message" << std::endl;

        // Build INIT_ACK response
        WorkerMessage response{};
        response.type = MessageType::INIT_ACK;
        response.init_ack.protocol_version = WORKER_PROTOCOL_VERSION;

        std::strncpy(response.init_ack.version, STUB_VERSION_STRING,
                     sizeof(response.init_ack.version) - 1);
        response.init_ack.version[sizeof(response.init_ack.version) - 1] = '\0';

        response.init_ack.capabilities = 0;  // No capabilities in STUB mode

        SendMessage(response);
    }

    /**
     * @brief Handle HEARTBEAT message
     * @MX:ANCHOR: Health check for worker process. Called periodically by host process.
     * @MX:REASON: Liveness check - must respond quickly to indicate worker is alive.
     */
    void HandleHeartbeat(const WorkerMessage&) {
        std::cout << "[Worker] Received HEARTBEAT" << std::endl;

        WorkerMessage response{};
        response.type = MessageType::HEARTBEAT_ACK;
        response.heartbeat_ack.status = static_cast<uint32_t>(WorkerStatus::IDLE);

        SendMessage(response);
    }

    /**
     * @brief Handle SHUTDOWN message
     * @MX:ANCHOR: Graceful shutdown sequence. Sets running_ flag to false to exit Run() loop.
     * @MX:REASON: Clean shutdown - must acknowledge before terminating to prevent data loss.
     */
    void HandleShutdown(const WorkerMessage&) {
        std::cout << "[Worker] Received SHUTDOWN" << std::endl;

        WorkerMessage response{};
        response.type = MessageType::SHUTDOWN_ACK;
        response.shutdown_ack.exit_code = 0;

        SendMessage(response);

        // Stop the server
        running_ = false;
    }

    /**
     * @brief Handle unknown message types
     */
    void HandleUnknown(const WorkerMessage& msg) {
        std::cerr << "[Worker] Unknown message type: " << static_cast<int>(msg.type) << std::endl;

        WorkerMessage response{};
        response.type = MessageType::ERROR_RESPONSE;
        response.error.code = static_cast<uint32_t>(ErrorCode::UNKNOWN_MESSAGE);
        std::strncpy(response.error.message, "Unknown message type",
                     sizeof(response.error.message) - 1);
        response.error.message[sizeof(response.error.message) - 1] = '\0';

        SendMessage(response);
    }

    /**
     * @brief Send message to client
     */
    void SendMessage(const WorkerMessage& msg) {
        DWORD bytes_written = 0;
        BOOL success = WriteFile(
            pipe_handle_,
            &msg,
            sizeof(msg),
            &bytes_written,
            nullptr
        );

        if (!success) {
            std::cerr << "[Worker] WriteFile failed: " << GetLastError() << std::endl;
        }
    }

    std::string pipe_name_;
    HANDLE pipe_handle_;
    bool running_;
};

/**
 * @brief Main entry point for worker process
 */
int main(int argc, char* argv[]) {
    std::cout << "[Worker] XPE AI Worker Process v0.1.0 (STUB)" << std::endl;
    std::cout << "[Worker] Built: " << __DATE__ << " " << __TIME__ << std::endl;

#ifdef XPE_AI_STUB_BUILD
    std::cout << "[Worker] Mode: STUB (no ONNX Runtime)" << std::endl;
#else
    std::cout << "[Worker] Mode: FULL (with ONNX Runtime)" << std::endl;
#endif

    // Use default pipe name
    std::string pipe_name = XPE_AI_WORKER_PIPE_NAME;

    // Allow override via command line
    if (argc > 1) {
        pipe_name = argv[1];
    }

    std::cout << "[Worker] Pipe: " << pipe_name << std::endl;

    // Create and start server
    WorkerServer server(pipe_name);

    if (!server.Start()) {
        std::cerr << "[Worker] Failed to start server" << std::endl;
        return 1;
    }

    if (!server.WaitForConnection()) {
        std::cerr << "[Worker] Failed to wait for connection" << std::endl;
        return 1;
    }

    // Run message loop
    server.Run();

    std::cout << "[Worker] Exiting cleanly" << std::endl;
    return 0;
}
