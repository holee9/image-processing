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
 *
 * @ingroup xpe_ai
 */

#ifndef XPE_AI_ONNX_SESSION_H
#define XPE_AI_ONNX_SESSION_H

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

#ifdef XPE_AI_STUB_BUILD
    /** 1 when this build has no ONNX Runtime (stub), 0 when it links the real one. */
    #define ONNX_RUNTIME_STUB_BUILD 1
#else
    /** 0 when this build links the real ONNX Runtime, 1 when it is the stub. */
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
    kOutOfMemory = 6,           ///< An allocation failed while creating the session (QA-B-194 M5): a shortage, not a bad model
    kModelNotTrusted = 7,       ///< The model or its sidecar failed signature verification (QA-B-195 M3): nothing was loaded
    kSidecarInvalid = 8,        ///< The signature verified but the sidecar does not say what REQ-AI-008 requires (QA-B-197)
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
    std::string name;               ///< Tensor name as declared in the model graph
    std::vector<int64_t> shape;     ///< Declared dimensions (-1 = the model does not constrain that axis)
    std::string type;               ///< Element type name; the session reports "float32" for every tensor
};

/**
 * @brief ONNX session configuration
 */
struct OnnxSessionConfig {
    ExecutionProvider execution_provider = ExecutionProvider::kCpu; ///< Requested EP; the one actually used may differ after fallback
    std::string model_path;                                        ///< Path of the .onnx model file
    int num_threads = 1;                                           ///< Intra-op thread count; a value below 1 is treated as 1
    LogLevel log_level = LogLevel::kWarning;                       ///< Session logging verbosity
    bool enable_profiling = false;                                 ///< Accepted and ignored: no profile is written (logs a warning)
    /**
     * The job this model is loaded for ("bone_suppress" or "bodypart"). It is part of what the signature covers
     * (QA-B-195), so a model signed for one job is refused for another. A fixture that is named for no job is
     * signed as "bone_suppress", which is also the default here.
     */
    std::string role = "bone_suppress";
};

/**
 * @brief The files of one model as they were READ and VERIFIED (QA-B-195 / QA-B-197)
 *
 * Whoever uses the model uses these bytes and no others. A sidecar that does not exist leaves has_sidecar false.
 */
struct VerifiedModelFiles {
    std::vector<uint8_t> model;     ///< the model file's bytes
    std::string sidecar_text;       ///< the sidecar's bytes as text (meaningful only when has_sidecar)
    bool has_sidecar = false;       ///< true when `<stem>.json` exists
};

/**
 * @brief Read `<stem>.onnx`, `<stem>.json` and `<stem>.sig` ONCE and verify the signature for @p role.
 *
 * The one place the files of a model are read and checked, shared by OnnxSession::Create (which builds a session from
 * the result) and the model card (which only reads the sidecar). Returns kOk, kInvalidModelPath (no model file),
 * kModelLoadFailed (the model file cannot be read) or kModelNotTrusted; @p message carries the detail for a failure.
 * Does NOT judge the sidecar's content (see ai_model_sidecar.h).
 *
 * @param model_path  Path of the model file `<stem>.onnx`; the sidecar and signature sit beside it.
 * @param role        The job the model is loaded for ("bone_suppress" or "bodypart"); part of what the signature covers.
 * @param out         Receives the verified bytes (meaningful only when the result is kOk).
 * @param message     Receives the detail of a failure.
 * @return kOk, or the code described above.
 */
OnnxErrorCode ReadVerifiedModelFiles(const std::string& model_path, const std::string& role,
                                     VerifiedModelFiles* out, std::string* message);

/**
 * @brief Result type for operations that can fail
 */
template<typename T>
struct OnnxResult {
    T value;                                    ///< Payload; meaningful only when has_value() is true
    OnnxErrorCode code = OnnxErrorCode::kOk;    ///< kOk on success, otherwise the failure reason
    std::string message;                        ///< Human-readable detail for a failure; empty on success

    /** @return true when code is kOk, i.e. value holds a usable payload. */
    bool has_value() const { return code == OnnxErrorCode::kOk; }
    /** @return Pointer to value (member access); not checked against has_value(). */
    T* operator->() { return &value; }
    /** @return Const pointer to value (member access); not checked against has_value(). */
    const T* operator->() const { return &value; }
    /** @return Reference to value; not checked against has_value(). */
    T& operator*() { return value; }
    /** @return Const reference to value; not checked against has_value(). */
    const T& operator*() const { return value; }

    /** @return Same as has_value(); explicit so a result is not silently usable as an integer. */
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

    /**
     * @brief Move constructor - takes over the session of @p other
     * @param other Source session; left empty (IsValid() is false) afterwards
     */
    OnnxSession(OnnxSession&& other) noexcept;

    /**
     * @brief Move assignment - releases this session, then takes over @p other
     * @param other Source session; left empty (IsValid() is false) afterwards
     * @return *this
     * @note Defined in the .cpp below struct Impl; see "THE RULE" at the end of this header.
     */
    OnnxSession& operator=(OnnxSession&& other) noexcept;

    /**
     * @brief Create a new ONNX session
     *
     * @param config Session configuration
     * @return OnnxResult with session pointer or error
     */
    static OnnxResult<std::unique_ptr<OnnxSession>> Create(const OnnxSessionConfig& config);

    /**
     * @brief Check if session is valid
     * @return true when the session object is in a valid state; false after being moved from.
     *         It is true in a stub build too, where no model is loaded, so it does not
     *         promise that inference can run (see IsStubBuild() and Run()).
     */
    bool IsValid() const;

    /**
     * @brief Get actual execution provider used
     *
     * May differ from requested if fallback occurred.
     *
     * @return The EP recorded for the session after any fallback. In a stub build
     *         nothing runs, so this is bookkeeping, not evidence of a running provider.
     */
    ExecutionProvider GetActualExecutionProvider() const;

    /**
     * @brief Get model metadata (REQ-AI-008)
     * @return Metadata of the loaded model; valid for the lifetime of the session
     */
    const ModelMetadata& GetModelMetadata() const;

    /**
     * @brief The sidecar `<model stem>.json` exactly as it was verified, or nullptr when the model has none.
     *
     * Create() reads the model, the sidecar and the signature ONCE, verifies them together and builds the session from
     * those bytes (QA-B-195, REQ-AI-007 / REQ-AI-091). A caller that needs the sidecar's content (the body-part
     * labels) takes it from here and never opens the file again: what was verified is what is used, with no window
     * between the check and the use in which the file could be swapped.
     * @return Pointer valid for the lifetime of the session.
     */
    const std::string* VerifiedSidecar() const;

    /**
     * @brief Get input tensor metadata
     * @return One entry per model input, in graph order
     */
    std::vector<TensorMetadata> GetInputMetadata() const;

    /**
     * @brief Get output tensor metadata
     * @return One entry per model output, in graph order
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
     * @return true when this build has no ONNX Runtime (same as ONNX_RUNTIME_STUB_BUILD)
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

// THE RULE, IN FULL (QA-B-161, QA-B-164, #130, #226):
//
//   Anything that `delete pimpl_` must be (a) in the .cpp, AND
//   (b) BELOW the definition of struct Impl.
//
// So the destructor and the move-ASSIGNMENT are both defined in the .cpp, and
// both below the struct. This is not a style choice: deleting a pointer to an
// incomplete type emits no call to ~Impl(), which in a full build leaks the
// ONNX session and env. MSVC names it C4150, "no destructor called".
//
// Half of that rule is what kept a real defect alive. The note here used to
// read only "implemented in .cpp to avoid incomplete type warning" -- and the
// destructor, which obeyed it, was still wrong because it sat ABOVE the
// struct. The product hid the warning with /wd4150 until QA-B-161's test
// target, which had no suppression, surfaced it; #226 (QA-B-164) then removed
// the suppression from both product targets after measuring 0 remaining
// occurrences, so the next one is visible rather than silent.
//
// The move CONSTRUCTOR stays inline: it only takes the pointer, never deletes,
// so an incomplete type is fine there.

inline OnnxSession::OnnxSession(OnnxSession&& other) noexcept
    : pimpl_(other.pimpl_) {
    other.pimpl_ = nullptr;
}

} // namespace xpe::ai

#endif // XPE_AI_ONNX_SESSION_H
