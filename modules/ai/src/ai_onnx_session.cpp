/**
 * @file ai_onnx_session.cpp
 * @brief ONNX Runtime session manager implementation (T-003)
 *
 * Implements session management with stub/full mode support.
 * Stub mode provides functional API without ONNX Runtime dependency.
 *
 * REQ-AI-006: ONNX Runtime 1.20+ integration with multi-EP support
 * REQ-AI-008: Model versioning and metadata
 */

#include "xpe/ai/ai_onnx_session.h"
#include "xpe/common/xpe_error.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

// Optional dependencies
#ifdef XPE_AI_USE_SPDLOG
    #include <spdlog/spdlog.h>
    #define LOG_INFO(msg) spdlog::info(msg)
    #define LOG_WARN(msg) spdlog::warn(msg)
    #define LOG_ERROR(msg) spdlog::error(msg)
#else
    #define LOG_INFO(msg) ((void)0)
    #define LOG_WARN(msg) ((void)0)
    #define LOG_ERROR(msg) ((void)0)
#endif

#ifdef XPE_AI_USE_NLOHMANN_JSON
    #include <nlohmann/json.hpp>
    using json = nlohmann::json;
#endif

namespace fs = std::filesystem;

namespace xpe::ai {

// =============================================================================
// PIMPL Implementation
// =============================================================================

// Destructor implementation (must be in .cpp for PIMPL)
OnnxSession::~OnnxSession() {
    delete pimpl_;
}

struct OnnxSession::Impl {
    OnnxSessionConfig config;
    ExecutionProvider actual_ep;
    ModelMetadata metadata;
    std::vector<TensorMetadata> inputs;
    std::vector<TensorMetadata> outputs;
    bool is_valid;

    Impl() : actual_ep(ExecutionProvider::kCpu), is_valid(false) {}
};

// =============================================================================
// Helper Functions
// =============================================================================

namespace {

/**
 * @brief Check if a file exists
 */
bool FileExists(const std::string& path) {
    return fs::exists(path) && fs::is_regular_file(path);
}

/**
 * @brief Load JSON metadata from file
 */
std::optional<ModelMetadata> LoadMetadataFromFile(const fs::path& metadata_path) {
#ifdef XPE_AI_USE_NLOHMANN_JSON
    if (!fs::exists(metadata_path)) {
        return std::nullopt;
    }

    try {
        std::ifstream meta_file(metadata_path);
        json j;
        meta_file >> j;

        ModelMetadata meta;
        if (j.contains("model_id")) {
            meta.model_id = j["model_id"].get<std::string>();
        }
        if (j.contains("version")) {
            meta.version = j["version"].get<std::string>();
        }
        if (j.contains("pccp_scope")) {
            meta.pccp_scope = j["pccp_scope"].get<std::string>();
        }
        if (j.contains("training_data_hash")) {
            meta.training_data_hash = j["training_data_hash"].get<std::string>();
        }
        if (j.contains("validation_metrics")) {
            meta.validation_metrics = j["validation_metrics"].dump();
        }

        return meta;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Failed to load metadata: ") + e.what());
        return std::nullopt;
    }
#else
    // Fallback: simple key-value parsing without JSON library
    (void)metadata_path;
    return std::nullopt;
#endif
}

/**
 * @brief Get EP name for logging
 */
std::string EpToString(ExecutionProvider ep) {
    switch (ep) {
        case ExecutionProvider::kCpu: return "CPU";
        case ExecutionProvider::kCuda: return "CUDA";
        case ExecutionProvider::kTensorRt: return "TensorRT";
        case ExecutionProvider::kDirectMl: return "DirectML";
        default: return "Unknown";
    }
}

} // anonymous namespace

// =============================================================================
// OnnxSession Implementation
// =============================================================================

OnnxSession::OnnxSession()
    : pimpl_(new Impl()) {
}

OnnxResult<std::unique_ptr<OnnxSession>> OnnxSession::Create(
        const OnnxSessionConfig& config) {

    OnnxResult<std::unique_ptr<OnnxSession>> result;
    result.value = nullptr;

    // Validate model path
    if (!FileExists(config.model_path)) {
        result.code = OnnxErrorCode::kInvalidModelPath;
        result.message = "Model file not found: " + config.model_path;
        LOG_ERROR(result.message);
        return result;
    }

    // Create session instance
    auto session = std::unique_ptr<OnnxSession>(new OnnxSession());
    session->pimpl_->config = config;

    // Determine available EPs and select actual EP
    auto available_eps = GetAvailableExecutionProviders();
    ExecutionProvider actual_ep = config.execution_provider;

    // Check if requested EP is available
    bool ep_available = std::find(available_eps.begin(), available_eps.end(),
                                   actual_ep) != available_eps.end();

    if (!ep_available) {
        LOG_WARN(std::string("Requested EP ") + EpToString(actual_ep) +
                 " not available, falling back to CPU");
        actual_ep = ExecutionProvider::kCpu;
    }

    session->pimpl_->actual_ep = actual_ep;

    // Load metadata from JSON file if present
    fs::path model_path(config.model_path);
    fs::path metadata_path = model_path;
    metadata_path.replace_extension(".json");

    auto metadata_opt = LoadMetadataFromFile(metadata_path);
    if (metadata_opt.has_value()) {
        session->pimpl_->metadata = std::move(metadata_opt.value());
        LOG_INFO("Loaded model metadata: " + session->pimpl_->metadata.model_id);
    } else {
        // Use default empty metadata
        session->pimpl_->metadata = ModelMetadata{};
    }

#if ONNX_RUNTIME_STUB_BUILD
    // Stub mode: Create session without actual ONNX Runtime
    LOG_INFO("Creating ONNX session in STUB mode");
    session->pimpl_->is_valid = true;

    // In stub mode, input/output metadata are empty
    session->pimpl_->inputs.clear();
    session->pimpl_->outputs.clear();
#else
    // Full build mode: Create actual ONNX Runtime session
    // TODO: Implement actual ONNX Runtime session creation
    // For now, use stub implementation even in full build
    LOG_INFO("Creating ONNX session (ONNX Runtime integration TODO)");
    session->pimpl_->is_valid = true;

    // TODO: Extract actual input/output metadata from model
    session->pimpl_->inputs.clear();
    session->pimpl_->outputs.clear();
#endif

    result.code = OnnxErrorCode::kOk;
    result.value = std::move(session);
    return result;
}

bool OnnxSession::IsValid() const {
    return pimpl_ && pimpl_->is_valid;
}

ExecutionProvider OnnxSession::GetActualExecutionProvider() const {
    return pimpl_ ? pimpl_->actual_ep : ExecutionProvider::kCpu;
}

const ModelMetadata& OnnxSession::GetModelMetadata() const {
    static const ModelMetadata empty_metadata{};
    return pimpl_ ? pimpl_->metadata : empty_metadata;
}

std::vector<TensorMetadata> OnnxSession::GetInputMetadata() const {
    return pimpl_ ? pimpl_->inputs : std::vector<TensorMetadata>{};
}

std::vector<TensorMetadata> OnnxSession::GetOutputMetadata() const {
    return pimpl_ ? pimpl_->outputs : std::vector<TensorMetadata>{};
}

std::vector<ExecutionProvider> OnnxSession::GetAvailableExecutionProviders() {
    std::vector<ExecutionProvider> eps;

    // CPU EP is always available
    eps.push_back(ExecutionProvider::kCpu);

#if ONNX_RUNTIME_STUB_BUILD
    // Stub mode: Only CPU is available
    return eps;
#else
    // Full build: Check which EPs are actually available
    // TODO: Query ONNX Runtime for available EPs
    // For now, assume all EPs are available (will be validated at runtime)
    eps.push_back(ExecutionProvider::kCuda);
    eps.push_back(ExecutionProvider::kTensorRt);
    eps.push_back(ExecutionProvider::kDirectMl);
    return eps;
#endif
}

bool OnnxSession::IsStubBuild() {
#if ONNX_RUNTIME_STUB_BUILD
    return true;
#else
    return false;
#endif
}

} // namespace xpe::ai
