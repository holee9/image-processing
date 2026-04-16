/**
 * @file xpe_preprocess.cpp
 * @brief XPE Preprocessing Module Implementation - Phase 1: Lifecycle
 *
 * REQ-P1A-001: Module Initialization
 * REQ-P1A-002: P/Invoke ABI Compliance
 * REQ-P1A-003: Thread Safety
 * REQ-P1A-030: No Exceptions Across C ABI
 * REQ-P1A-031: No Memory Leak
 */

#include "xpe/preprocess_api.h"
#include <mutex>
#include <atomic>
#include <cstring>
#include <cmath>

// =============================================================================
// Internal State Management
// =============================================================================

namespace {

/**
 * @brief Global module state
 *
 * REQ-P1A-003: Thread-safe initialization using atomic flag
 * REQ-P1A-031: RAII for resource management
 */
struct PreprocessState {
    std::atomic<bool> initialized{false};
    std::mutex init_mutex;  // Protects initialization/shutdown

    // Configuration
    char mode[32] = "default";  // "clinical", "research", "default"
    int log_level = 0;

} g_state;

} // anonymous namespace

// =============================================================================
// Phase 1: Lifecycle Functions
// =============================================================================

/**
 * @brief Initialize preprocessing module with configuration
 *
 * REQ-P1A-001: Module Initialization
 * AC-LC-001: Accept NULL config for default settings
 * AC-LC-002: Accept JSON config string for custom settings
 * AC-LC-003: Return XPE_ERR_INVALID_INPUT on double-init
 * REQ-P1A-003: Thread-safe for concurrent initialization attempts
 * REQ-P1A-005: Input validation (NULL config is valid)
 * REQ-P1A-030: No exceptions across C ABI boundary
 */
extern "C" XPE_API XpeErrorCode xpe_preprocess_init(const char* config) {
    // REQ-P1A-030: Catch all C++ exceptions and convert to error codes
    try {
        // REQ-P1A-003: Thread-safe initialization with mutex
        std::lock_guard<std::mutex> lock(g_state.init_mutex);

        // AC-LC-003: Double-init guard
        if (g_state.initialized.load(std::memory_order_acquire)) {
            return XPE_ERR_INVALID_INPUT;
        }

        // AC-LC-001: Default configuration when NULL
        if (config == nullptr) {
            // Use default settings
            std::strncpy(g_state.mode, "default", sizeof(g_state.mode) - 1);
            g_state.log_level = 0;
        }
        // AC-LC-002: Parse JSON configuration
        else {
            // Simple JSON parsing for {"mode":"clinical"}
            // For MVP, we only support mode parameter
            if (std::strstr(config, "\"mode\"") != nullptr &&
                std::strstr(config, "\"clinical\"") != nullptr) {
                std::strncpy(g_state.mode, "clinical", sizeof(g_state.mode) - 1);
            } else if (std::strstr(config, "\"mode\"") != nullptr &&
                       std::strstr(config, "\"research\"") != nullptr) {
                std::strncpy(g_state.mode, "research", sizeof(g_state.mode) - 1);
            } else {
                std::strncpy(g_state.mode, "default", sizeof(g_state.mode) - 1);
            }

            // Extract log_level if present
            const char* log_level_str = std::strstr(config, "\"log_level\"");
            if (log_level_str != nullptr) {
                int level = 0;
                if (std::sscanf(log_level_str, "\"log_level\"%*[: ] %d", &level) == 1) {
                    g_state.log_level = level;
                }
            }
        }

        // Null-terminate strings
        g_state.mode[sizeof(g_state.mode) - 1] = '\0';

        // Mark as initialized
        g_state.initialized.store(true, std::memory_order_release);

        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        // REQ-P1A-030: Catch all exceptions and convert to error code
        return XPE_ERR_PROCESSING_FAILED;
    }
}

/**
 * @brief Shutdown preprocessing module and release resources
 *
 * REQ-P1A-031: No memory leak after shutdown
 * REQ-P1A-003: Thread-safe for concurrent shutdown
 */
extern "C" XPE_API void xpe_preprocess_shutdown(void) {
    // REQ-P1A-030: No exceptions allowed across C ABI
    try {
        // REQ-P1A-003: Thread-safe shutdown with mutex
        std::lock_guard<std::mutex> lock(g_state.init_mutex);

        // REQ-P1A-031: Release all resources
        // Reset state to uninitialized
        g_state.initialized.store(false, std::memory_order_release);

        // Reset configuration
        std::memset(g_state.mode, 0, sizeof(g_state.mode));
        g_state.log_level = 0;

        // Note: No dynamic memory allocation in Phase 1
        // Future phases will need proper cleanup here

    } catch (...) {
        // REQ-P1A-030: Suppress exceptions in shutdown
        // Cannot return error code from void function
    }
}

// =============================================================================
// Phase 2-5: Stub Implementations (to be implemented in TDD cycles)
// ============================================================================

extern "C" XPE_API XpeErrorCode xpe_calib_load_offset(const char* filepath) {
    (void)filepath;  // Suppress unused parameter warning
    return XPE_ERR_NOT_INITIALIZED;
}

extern "C" XPE_API XpeErrorCode xpe_calib_load_gain(const char* filepath) {
    (void)filepath;
    return XPE_ERR_NOT_INITIALIZED;
}

extern "C" XPE_API XpeErrorCode xpe_calib_load_defect_map(const char* filepath) {
    (void)filepath;
    return XPE_ERR_NOT_INITIALIZED;
}

extern "C" XPE_API XpeErrorCode xpe_offset_correct(const XpeImageBuffer* input,
                                                   XpeImageBuffer* output,
                                                   const XpeImageMetadata* metadata) {
    (void)input;
    (void)output;
    (void)metadata;
    return XPE_ERR_NOT_INITIALIZED;
}

extern "C" XPE_API XpeErrorCode xpe_gain_correct(const XpeImageBuffer* input,
                                                 XpeImageBuffer* output,
                                                 const XpeImageMetadata* metadata) {
    (void)input;
    (void)output;
    (void)metadata;
    return XPE_ERR_NOT_INITIALIZED;
}

extern "C" XPE_API XpeErrorCode xpe_defect_correct(const XpeImageBuffer* input,
                                                   XpeImageBuffer* output,
                                                   const XpeImageMetadata* metadata) {
    (void)input;
    (void)output;
    (void)metadata;
    return XPE_ERR_NOT_INITIALIZED;
}

extern "C" XPE_API XpeErrorCode xpe_calib_generate_offset(const XpeImageBuffer* dark_frames,
                                                          int32_t num_frames,
                                                          float integration_time_ms,
                                                          float temperature_c,
                                                          const char* output_path) {
    (void)dark_frames;
    (void)num_frames;
    (void)integration_time_ms;
    (void)temperature_c;
    (void)output_path;
    return XPE_ERR_NOT_INITIALIZED;
}

extern "C" XPE_API XpeErrorCode xpe_calib_check_expiry(const char* filepath,
                                                       bool* is_expired,
                                                       int32_t* remaining_days) {
    (void)filepath;
    (void)is_expired;
    (void)remaining_days;
    return XPE_ERR_NOT_INITIALIZED;
}

extern "C" XPE_API XpeErrorCode xpe_calib_save(const char* filepath,
                                               const char* calib_type) {
    (void)filepath;
    (void)calib_type;
    return XPE_ERR_NOT_INITIALIZED;
}

extern "C" XPE_API XpeErrorCode xpe_validate_readout_artifact(const XpeImageBuffer* image,
                                                             const XpeImageMetadata* metadata,
                                                             bool* has_dropped_columns,
                                                             bool* has_nonuniform_gain) {
    (void)image;
    (void)metadata;
    (void)has_dropped_columns;
    (void)has_nonuniform_gain;
    return XPE_ERR_NOT_INITIALIZED;
}

/**
 * @brief Detect transient defects at runtime
 *
 * REQ-P1A-013: Runtime defect detection with dose-dependent threshold
 * AC-DEF-003: Merge with static BPM
 * REQ-P1A-030: No exceptions across C ABI boundary
 */
extern "C" XPE_API XpeErrorCode xpe_defect_detect_runtime(const XpeImageBuffer* image,
                                                          const XpeImageMetadata* metadata,
                                                          XpeImageBuffer* defect_map_output) {
    try {
        // Validate input
        if (image == nullptr || metadata == nullptr || defect_map_output == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Validate format
        if (image->format != XPE_PIXEL_FLOAT32) {
            return XPE_ERR_UNSUPPORTED_FORMAT;
        }

        // Validate output dimensions
        if (defect_map_output->width != image->width ||
            defect_map_output->height != image->height ||
            defect_map_output->format != XPE_PIXEL_UINT16) {
            return XPE_ERR_BUFFER_TOO_SMALL;
        }

        const float* data = static_cast<const float*>(image->data);
        uint8_t* defect_map = static_cast<uint8_t*>(defect_map_output->data);

        // Initialize defect map to all good (0)
        std::memset(defect_map, 0, image->width * image->height);

        // Calculate statistics for outlier detection
        double sum = 0.0;
        double sum_sq = 0.0;
        size_t pixel_count = image->width * image->height;

        for (size_t i = 0; i < pixel_count; ++i) {
            sum += data[i];
            sum_sq += data[i] * data[i];
        }

        double mean = sum / pixel_count;
        double variance = (sum_sq / pixel_count) - (mean * mean);
        double stddev = std::sqrt(variance);

        // AC-DEF-003: Dose-dependent threshold
        // Higher dose (mAs) → higher threshold for defect detection
        float dose_factor = (metadata->mAs > 0) ? metadata->mAs : 1.0f;
        float threshold = static_cast<float>(mean + 3.0 * stddev * dose_factor);

        // Detect outliers (potential defects)
        for (size_t i = 0; i < pixel_count; ++i) {
            if (data[i] > threshold || data[i] < 0.0f) {
                defect_map[i] = 1;  // Mark as defective
            }
        }

        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

/**
 * @brief Query valid parameter ranges for calibration
 *
 * REQ-P1A-042: Return valid ranges for calibration parameters
 * REQ-P1A-030: No exceptions across C ABI boundary
 */
extern "C" XPE_API XpeErrorCode xpe_preprocess_get_param_range(const char* param_name,
                                                               float* min_value,
                                                               float* max_value) {
    try {
        // Validate input
        if (param_name == nullptr || min_value == nullptr || max_value == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Define valid ranges for calibration parameters
        struct ParamRange {
            const char* name;
            float min_val;
            float max_val;
        };

        static const ParamRange ranges[] = {
            {"integration_time_ms", 1.0f, 10000.0f},
            {"temperature_c", -10.0f, 50.0f},
            {"kVp", 40.0f, 150.0f},
            {"mAs", 0.1f, 1000.0f},
            {"SID_mm", 1000.0f, 2000.0f},
            {"pixelPitch_mm", 0.1f, 0.5f}
        };

        // Search for parameter
        for (const auto& range : ranges) {
            if (std::strcmp(param_name, range.name) == 0) {
                *min_value = range.min_val;
                *max_value = range.max_val;
                return XPE_OK;
            }
        }

        // Parameter not found
        return XPE_ERR_INVALID_INPUT;

    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
