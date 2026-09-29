/**
 * @file ai_onnx_session.h
 * @brief ONNX Runtime session manager (T-003)
 *
 * Provides ONNX Runtime session management with multi-EP support,
 * model metadata extraction, and EP fallback mechanisms.
 *
 * REQ-AI-006: ONNX Runtime 1.20+ integration with multi-EP support
 * REQ-AI-008: Model versioning and metadata
 *
 * Build modes:
 *   XPE_AI_STUB_BUILD=ON  -- Stub implementation (default)
 *   XPE_AI_USE_ONNXRUNTIME=ON -- Full ONNX Runtime integration
 */

#ifndef XPE_AI_ONNX_SESSION_H
#define XPE_AI_ONNX_SESSION_H

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

#ifdef XPE_AI_STUB_BUILD
    #define ONNX_RUNTIME_STUB_BUILD 1
#else
    #define ONNX_RUNTIME_STUB_BUILD 0
#endif

namespace xpe::ai {

// =============================================================================
// Public Types
// =============================================================================

/**
 * @brief Execution Provider types (REQ-AI-006)
 */
enum class ExecutionProvider {
    kCpu = 0,           ///< CPU execution provider (always available)
    kCuda = 1,          ///< CUDA GPU provider (x86)
    kTensorRt = 2,      ///< TensorRT provider (NVIDIA)
    kDirectMl = 3,      ///< DirectML provider (Windows GPU)
};

/**
 * @brief Log levels for session logging
 */
enum class LogLevel {
    kVerbose = 0,
    kInfo = 1,
    kWarning = 2,
    kError = 3,
};

/**
 * @brief ONNX error codes
 */
enum class OnnxErrorCode {
    kOk = 0,                    ///< Success
    kInvalidModelPath = 1,      ///< Model file not found
    kModelLoadFailed = 2,       ///< Failed to load model
    kEpNotAvailable = 3,        ///< Requested EP not available
    kSessionCreationFailed = 4, ///< Failed to create session
    kInvalidInput = 5,          ///< Invalid input data
};

/**
 * @brief Model metadata (REQ-AI-008)
 */
struct ModelMetadata {
    std::string model_id;           ///< Model identifier
    std::string version;            ///< Semver version
    std::string pccp_scope;         ///< PCCP boundary scope
    std::string training_data_hash; ///< Training dataset hash
    std::string validation_metrics; ///< JSON-encoded metrics
};

/**
 * @brief Tensor metadata for inputs/outputs
 */
struct TensorMetadata {
    std::string name;
    std::vector<int64_t> shape;
    std::string type;
};

/**
 * @brief ONNX session configuration
 */
struct OnnxSessionConfig {
    ExecutionProvider execution_provider = ExecutionProvider::kCpu;
    std::string model_path;
    int num_threads = 1;
    LogLevel log_level = LogLevel::kWarning;
    bool enable_profiling = false;
};

/**
 * @brief Result type for operations that can fail
 */
template<typename T>
struct OnnxResult {
    T value;
    OnnxErrorCode code = OnnxErrorCode::kOk;
    std::string message;

    bool has_value() const { return code == OnnxErrorCode::kOk; }
    T* operator->() { return &value; }
    const T* operator->() const { return &value; }
    T& operator*() { return value; }
    const T& operator*() const { return value; }

    explicit operator bool() const { return has_value(); }
};

// =============================================================================
// ONNX Runtime Session Manager
// =============================================================================

/**
 * @brief ONNX Runtime session manager (REQ-AI-006, REQ-AI-008)
 *
 * Manages ONNX Runtime session lifecycle, EP selection, and model metadata.
 * Supports stub mode for builds without ONNX Runtime dependency.
 */
class OnnxSession {
public:
    /**
     * @brief Destructor - releases session resources
     * @note Implementation in .cpp file to avoid incomplete type warning
     */
    ~OnnxSession();

    // Disable copy, enable move
    OnnxSession(const OnnxSession&) = delete;
    OnnxSession& operator=(const OnnxSession&) = delete;
    OnnxSession(OnnxSession&&) noexcept;
    OnnxSession& operator=(OnnxSession&&) noexcept;

    /**
     * @brief Create a new ONNX session
     *
     * @param config Session configuration
     * @return OnnxResult with session pointer or error
     */
    static OnnxResult<std::unique_ptr<OnnxSession>> Create(const OnnxSessionConfig& config);

    /**
     * @brief Check if session is valid
     */
    bool IsValid() const;

    /**
     * @brief Get actual execution provider used
     *
     * May differ from requested if fallback occurred.
     */
    ExecutionProvider GetActualExecutionProvider() const;

    /**
     * @brief Get model metadata (REQ-AI-008)
     */
    const ModelMetadata& GetModelMetadata() const;

    /**
     * @brief Get input tensor metadata
     */
    std::vector<TensorMetadata> GetInputMetadata() const;

    /**
     * @brief Get output tensor metadata
     */
    std::vector<TensorMetadata> GetOutputMetadata() const;

    /**
     * @brief Run the loaded model on one float input tensor (REQ-AI-006)
     *
     * QA-B-160 (#130). The thinnest vertical slice of the inference path:
     * one float input, one float output, first input and first output of the
     * graph. It is NOT the general inference API -- multi-input graphs,
     * non-float types and batching are not handled, and a model needing them
     * returns kInvalidInput rather than guessing.
     *
     * @param input  values for the first input tensor; its element count must
     *               equal the product of that tensor's declared dims
     * @return output tensor values, or an error code:
     *         kInvalidInput          session not valid, empty input, or a
     *                                length that does not match the model
     *         kSessionCreationFailed the runtime rejected the run
     *         kModelLoadFailed       stub build -- there is no model to run
     */
    OnnxResult<std::vector<float>> Run(const std::vector<float>& input);

    /**
     * @brief Query available execution providers
     *
     * @return List of EPs available in this build
     */
    static std::vector<ExecutionProvider> GetAvailableExecutionProviders();

    /**
     * @brief Check if running in stub mode
     */
    static bool IsStubBuild();

private:
    // Private constructor (use Create factory)
    OnnxSession();

    // PIMPL implementation
    struct Impl;
    Impl* pimpl_;
};

// =============================================================================
// Inline Implementations
// =============================================================================

// Note: Destructor implemented in .cpp to avoid incomplete type warning.
//
// QA-B-161 (#130): move-ASSIGNMENT is in the .cpp for the same reason and it
// is not cosmetic. It deletes pimpl_, and `Impl` is incomplete here, so MSVC
// raised C4150 -- "no destructor called". That is a real leak of everything
// Impl owns (in a full build, the ONNX session and env), not a style note.
//
// It went unseen because modules/ai/CMakeLists.txt compiles the product with
// /wd4150, so only a target without that suppression could see it. The first
// such target was this card's tests. The suppression is left in place and is
// NOT the fix -- moving the definition is.
//
// The move CONSTRUCTOR stays inline: it only takes the pointer, never deletes,
// so an incomplete type is fine there.

inline OnnxSession::OnnxSession(OnnxSession&& other) noexcept
    : pimpl_(other.pimpl_) {
    other.pimpl_ = nullptr;
}

} // namespace xpe::ai

#endif // XPE_AI_ONNX_SESSION_H
