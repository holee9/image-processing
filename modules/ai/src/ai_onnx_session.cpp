/**
 * @file ai_onnx_session.cpp
 * @brief ONNX Runtime session manager implementation (T-003)
 *
 * The full arm now runs a real ONNX Runtime session (QA-B-160, #130). Until
 * that card it did not: BOTH arms of every `#if ONNX_RUNTIME_STUB_BUILD` took
 * the stub path, the `#else` arm saying so itself ("For now, use stub
 * implementation even in full build"), and `Ort::` was 0 across modules/.
 * That history is kept because the file still has two arms and a reader has
 * to know which one their build took.
 *
 * WHAT THE FULL ARM DOES, and only this (the thinnest vertical slice #130 was
 * split into): load a model, read the first input/output tensor metadata,
 * run one float tensor through it, hand back the output values, and turn a
 * failure into an OnnxErrorCode. Verified against ONNX Runtime 1.30.0.
 *
 * WHAT IT STILL DOES NOT DO -- do not read "full build" as more than the above:
 *   - Only the CPU EP is registered on the session. GetAvailableExecutionProviders()
 *     now ASKS the runtime (it used to assert CUDA/TensorRT/DirectML blindly),
 *     but registering those providers needs their DLLs and is not done here.
 *   - One input, one output, float32 only. Anything else returns kInvalidInput
 *     instead of guessing.
 *   - Worker-process isolation (REQ-AI-003) does not go through this class.
 *   - `enable_profiling` is accepted and ignored.
 *
 * STUB ARM: no model is loaded and Run() returns kModelLoadFailed. It does NOT
 * echo the input -- an echo is what lets a green stub read as a working
 * inference path (#205, QA-B-154).
 *
 * @MX:TODO: register non-CPU execution providers, and widen Run() past the
 *           single float in / single float out shape
 * @MX:SPEC: SPEC-XPE-P3-AI REQ-AI-006
 *
 * REQ-AI-006: ONNX Runtime 1.20+ integration with multi-EP support
 *   -- the 1.20+ half is met (built against 1.30.0); multi-EP is not.
 * REQ-AI-008: Model versioning and metadata
 */

#include "xpe/ai/ai_onnx_session.h"
#include "xpe/common/xpe_error.h"

#if !ONNX_RUNTIME_STUB_BUILD
    #include <onnxruntime_cxx_api.h>
#endif

#include <fstream>
#include <sstream>
#include <filesystem>
#include <system_error>
#include <algorithm>

// Optional dependencies
#include "ai_log.h"
#ifdef XPE_AI_USE_SPDLOG
    // QA-B-177b: these run on the xpe_bone_suppress path (some inside catch bodies), so they go through the
    // never-throwing, null-tolerant LogText instead of calling spdlog directly (Codex #24 A1).
    #define LOG_INFO(msg) AI_LOG_TEXT(spdlog::level::info, msg)
    #define LOG_WARN(msg) AI_LOG_TEXT(spdlog::level::warn, msg)
    #define LOG_ERROR(msg) AI_LOG_TEXT(spdlog::level::err, msg)
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

struct OnnxSession::Impl {
    OnnxSessionConfig config;
    ExecutionProvider actual_ep;
    ModelMetadata metadata;
    std::vector<TensorMetadata> inputs;
    std::vector<TensorMetadata> outputs;
    bool is_valid;

#if !ONNX_RUNTIME_STUB_BUILD
    // QA-B-160 (#130). Held here rather than in the header so the header does
    // not need the ONNX headers -- a stub-build consumer must keep compiling.
    // Env must outlive Session; declaration order is the destruction contract.
    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::Session> session;
    std::string input_name;
    std::string output_name;
#endif

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
    // QA-B-181 (QA-B-179, #233): converting the std::string to an fs::path uses the process ANSI code page and
    // THROWS std::system_error for bytes it cannot convert, and the overloads without an error_code throw on
    // stat failures too. A path that cannot name a file is simply "not found" here, so it takes the same
    // kInvalidModelPath route as a missing file (the caller reports XPE_ERR_IO_FAILED either way). Only
    // system_error (which includes filesystem_error) is turned into false: bad_alloc is not "not found" and
    // still propagates to the exported function's guard.
    try {
        const fs::path p(path);
        std::error_code ec;
        return fs::exists(p, ec) && fs::is_regular_file(p, ec);
    } catch (const std::system_error&) {
        return false;
    }
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

// THE RULE, IN FULL -- both halves, because half of it is what let a real
// defect live here (QA-B-161, QA-B-164, #226):
//
//   Anything that `delete pimpl_` must be (a) in the .cpp, AND
//   (b) BELOW the definition of struct Impl.
//
// The comment that used to sit here said only "must be in .cpp for PIMPL".
// The destructor obeyed that and was still wrong: it was defined ABOVE the
// struct, where Impl is incomplete, so the compiler emitted no call to
// ~Impl(). In a full build that leaks the ONNX session and env -- everything
// Impl owns. MSVC says so as C4150, "no destructor called".
//
// It stayed invisible because modules/ai/CMakeLists.txt compiled the product
// with /wd4150. The first target without that suppression was the test target
// added in QA-B-161, under the ci-ai preset's warnings-as-errors.
// #226 (QA-B-164) then removed the suppression from both product targets
// after measuring 0 remaining occurrences -- so a future C4150 here will be
// visible instead of silent.
OnnxSession::~OnnxSession() {
    delete pimpl_;
}

OnnxSession::OnnxSession()
    : pimpl_(new Impl()) {
}

OnnxSession& OnnxSession::operator=(OnnxSession&& other) noexcept {
    if (this != &other) {
        delete pimpl_;
        pimpl_ = other.pimpl_;
        other.pimpl_ = nullptr;
    }
    return *this;
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
    // Full build: load the model into a real ONNX Runtime session (QA-B-160).
    //
    // A failure here is reported, never swallowed: a session that could not
    // load must not come back is_valid, because every caller reads that flag
    // as "the model is ready".
    try {
        session->pimpl_->env.reset(new Ort::Env(
            config.log_level == LogLevel::kVerbose ? ORT_LOGGING_LEVEL_VERBOSE :
            config.log_level == LogLevel::kInfo    ? ORT_LOGGING_LEVEL_INFO :
            config.log_level == LogLevel::kError   ? ORT_LOGGING_LEVEL_ERROR :
                                                     ORT_LOGGING_LEVEL_WARNING,
            "xpe_ai"));

        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(config.num_threads > 0 ? config.num_threads : 1);
        if (config.enable_profiling) {
            // Left unset on purpose: profiling writes a file whose path is a
            // policy question (REQ-AI-093 restricts writes), not a detail to
            // decide here. The flag is accepted and has no effect yet.
            LOG_WARN("enable_profiling is accepted but not wired (QA-B-160)");
        }

        // Only the CPU EP is registered. GetAvailableExecutionProviders()
        // reports what this build could offer; registering CUDA/DirectML needs
        // their provider DLLs and is out of this card's scope.
#ifdef _WIN32
        const std::wstring wide(config.model_path.begin(), config.model_path.end());
        session->pimpl_->session.reset(new Ort::Session(*session->pimpl_->env, wide.c_str(), opts));
#else
        session->pimpl_->session.reset(new Ort::Session(*session->pimpl_->env, config.model_path.c_str(), opts));
#endif

        Ort::AllocatorWithDefaultOptions alloc;
        Ort::Session& s = *session->pimpl_->session;

        for (size_t i = 0; i < s.GetInputCount(); ++i) {
            auto nameHolder = s.GetInputNameAllocated(i, alloc);
            const auto info = s.GetInputTypeInfo(i).GetTensorTypeAndShapeInfo();
            TensorMetadata m;
            m.name = nameHolder.get();
            m.shape = info.GetShape();
            m.type = "float32";
            session->pimpl_->inputs.push_back(std::move(m));
            if (i == 0) session->pimpl_->input_name = nameHolder.get();
        }
        for (size_t i = 0; i < s.GetOutputCount(); ++i) {
            auto nameHolder = s.GetOutputNameAllocated(i, alloc);
            const auto info = s.GetOutputTypeInfo(i).GetTensorTypeAndShapeInfo();
            TensorMetadata m;
            m.name = nameHolder.get();
            m.shape = info.GetShape();
            m.type = "float32";
            session->pimpl_->outputs.push_back(std::move(m));
            if (i == 0) session->pimpl_->output_name = nameHolder.get();
        }

        session->pimpl_->is_valid = true;
        LOG_INFO("ONNX session created: " + config.model_path);
    } catch (const Ort::Exception& e) {
        session->pimpl_->is_valid = false;
        result.code = OnnxErrorCode::kModelLoadFailed;
        result.message = std::string("ONNX Runtime rejected the model: ") + e.what();
        LOG_ERROR(result.message);
        return result;               // result.value stays null
    } catch (const std::bad_alloc&) {
        // QA-B-194 M5: a shortage of memory is not "the session could not be created" in the sense of a bad model or
        // a refused provider, and the C ABI must say so (XPE_ERR_OUT_OF_MEMORY). The sweep found this one: the
        // generic handler below turned a bad_alloc into kSessionCreationFailed, and xpe_bone_suppress into
        // XPE_ERR_PROCESSING_FAILED. The message is a literal: building a longer one here could itself throw.
        session->pimpl_->is_valid = false;
        result.code = OnnxErrorCode::kOutOfMemory;
        result.message = "out of memory while creating the session";
        return result;
    } catch (const std::exception& e) {
        session->pimpl_->is_valid = false;
        result.code = OnnxErrorCode::kSessionCreationFailed;
        result.message = std::string("session creation failed: ") + e.what();
        LOG_ERROR(result.message);
        return result;
    }
#endif

    result.code = OnnxErrorCode::kOk;
    result.value = std::move(session);
    return result;
}

OnnxResult<std::vector<float>> OnnxSession::Run(const std::vector<float>& input) {
    OnnxResult<std::vector<float>> result;

    if (!pimpl_ || !pimpl_->is_valid) {
        result.code = OnnxErrorCode::kInvalidInput;
        result.message = "session is not valid";
        return result;
    }
    if (input.empty()) {
        result.code = OnnxErrorCode::kInvalidInput;
        result.message = "input is empty";
        return result;
    }

#if ONNX_RUNTIME_STUB_BUILD
    // A stub build has no model, so there is nothing to run. It returns an
    // ERROR rather than echoing the input: an echo would let a caller -- and a
    // test -- mistake the stub for a working inference path, which is the
    // failure this module already shipped once (#205, QA-B-154).
    result.code = OnnxErrorCode::kModelLoadFailed;
    result.message = "stub build: no ONNX Runtime, so no model was run";
    return result;
#else
    // The declared element count of the first input, when the model fixes it.
    // A dynamic dim (-1) means the model does not constrain that axis, so the
    // caller's length is accepted for it.
    size_t expected = 1;
    bool fixed = true;
    if (!pimpl_->inputs.empty()) {
        for (const int64_t d : pimpl_->inputs.front().shape) {
            if (d < 0) { fixed = false; break; }
            expected *= static_cast<size_t>(d);
        }
    } else {
        fixed = false;
    }
    if (fixed && expected != input.size()) {
        result.code = OnnxErrorCode::kInvalidInput;
        result.message = "input has " + std::to_string(input.size()) +
                         " values but the model declares " + std::to_string(expected);
        return result;
    }

    try {
        std::vector<int64_t> shape;
        if (fixed) {
            shape = pimpl_->inputs.front().shape;
        } else {
            shape.push_back(static_cast<int64_t>(input.size()));
        }

        auto mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        // CreateTensor does not copy: `input` must outlive the Run call, and it
        // does -- it is the caller's argument and Run is synchronous.
        std::vector<float> scratch(input);
        Ort::Value in = Ort::Value::CreateTensor<float>(
            mem, scratch.data(), scratch.size(), shape.data(), shape.size());

        const char* inNames[]  = {pimpl_->input_name.c_str()};
        const char* outNames[] = {pimpl_->output_name.c_str()};
        auto outs = pimpl_->session->Run(Ort::RunOptions{nullptr},
                                         inNames, &in, 1, outNames, 1);
        if (outs.empty() || !outs.front().IsTensor()) {
            result.code = OnnxErrorCode::kSessionCreationFailed;
            result.message = "the model produced no output tensor";
            return result;
        }

        const auto info = outs.front().GetTensorTypeAndShapeInfo();
        const size_t n = info.GetElementCount();
        const float* data = outs.front().GetTensorData<float>();
        result.value.assign(data, data + n);
        result.code = OnnxErrorCode::kOk;
        return result;
    } catch (const Ort::Exception& e) {
        result.code = OnnxErrorCode::kSessionCreationFailed;
        result.message = std::string("ONNX Runtime failed the run: ") + e.what();
        LOG_ERROR(result.message);
        return result;
    }
#endif
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
    // QA-B-160 (#130): ASK the runtime instead of asserting. This used to
    // push kCuda/kTensorRt/kDirectMl unconditionally under a "TODO: Query
    // ONNX Runtime", so it named providers that may not exist on the machine
    // -- a caller checking availability got a yes that meant nothing.
    for (const std::string& name : Ort::GetAvailableProviders()) {
        if (name == "CUDAExecutionProvider")        eps.push_back(ExecutionProvider::kCuda);
        else if (name == "TensorrtExecutionProvider") eps.push_back(ExecutionProvider::kTensorRt);
        else if (name == "DmlExecutionProvider")      eps.push_back(ExecutionProvider::kDirectMl);
        // CPU is already in the list; any other provider has no enum here and
        // is deliberately not reported rather than mapped to something near.
    }
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
