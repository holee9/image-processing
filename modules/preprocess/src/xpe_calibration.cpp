/**
 * @file xpe_calibration.cpp
 * @brief XPE Calibration Management Implementation (Phase 2 & 4)
 *
 * REQ-P1A-014: Load offset calibration from XCal file
 * REQ-P1A-015: Load gain calibration from XCal file
 * REQ-P1A-016: Load defect map from XCal file
 * REQ-P1A-017: Generate offset calibration from dark frames
 * REQ-P1A-018: Check calibration expiry
 * REQ-P1A-019: Save calibration to XCal format
 */

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_memory.h"
#include <mutex>
#include <cstring>
#include <fstream>
#include <memory>

// =============================================================================
// Internal Calibration State
// =============================================================================

namespace {

/**
 * @brief Global calibration data storage
 *
 * REQ-P1A-014~016: Thread-safe calibration storage
 * REQ-P1A-031: RAII for automatic cleanup
 */
struct CalibrationData {
    // Offset calibration map
    std::unique_ptr<float[]> offset_map;
    uint32_t offset_width = 0;
    uint32_t offset_height = 0;

    // Gain calibration map
    std::unique_ptr<float[]> gain_map;
    uint32_t gain_width = 0;
    uint32_t gain_height = 0;

    // Defect map (BPM format)
    std::unique_ptr<uint8_t[]> defect_map;
    uint32_t defect_width = 0;
    uint32_t defect_height = 0;

    // Metadata
    char offset_session_id[64] = {0};
    char gain_session_id[64] = {0};
    int64_t offset_timestamp = 0;
    int64_t gain_timestamp = 0;

} g_calib;

std::mutex g_calib_mutex;  // Protects all calibration data

/**
 * @brief XCal file format header
 *
 * XCal Format Specification:
 * - Magic: "XCAL" (4 bytes)
 * - Version: uint32_t (currently 1)
 * - Type: uint32_t (0=offset, 1=gain, 2=defect)
 * - Width: uint32_t
 * - Height: uint32_t
 * - Timestamp: int64_t
 * - SessionID: char[64]
 * - DataSize: uint64_t
 * - SHA256: uint8_t[32]
 * - Data follows
 */
#pragma pack(push, 8)
struct XCalHeader {
    char     magic[4];       // "XCAL"
    uint32_t version;
    uint32_t type;           // 0=offset, 1=gain, 2=defect
    uint32_t width;
    uint32_t height;
    int64_t  timestamp;
    char     session_id[64];
    uint64_t data_size;
    uint8_t  sha256[32];
};
#pragma pack(pop)

static_assert(sizeof(XCalHeader) == 120, "XCalHeader must be 120 bytes");

/**
 * @brief Validate XCal file header
 */
bool validate_xcal_header(const XCalHeader& header, uint32_t expected_type) {
    // Check magic
    if (std::memcmp(header.magic, "XCAL", 4) != 0) {
        return false;
    }

    // Check version
    if (header.version != 1) {
        return false;
    }

    // Check type
    if (header.type != expected_type) {
        return false;
    }

    // Check dimensions
    if (header.width == 0 || header.height == 0 ||
        header.width > 4096 || header.height > 4096) {
        return false;
    }

    return true;
}

} // anonymous namespace

// =============================================================================
// Phase 2: Calibration Loading Functions
// =============================================================================

/**
 * @brief Load offset calibration map from XCal file
 *
 * REQ-P1A-014: Load XCal format offset maps
 * AC-CAL-001: Validate SHA-256, check session matching, verify expiry
 * REQ-P1A-030: No exceptions across C ABI boundary
 */
extern "C" XPE_API XpeErrorCode xpe_calib_load_offset(const char* filepath) {
    try {
        // Validate input
        if (filepath == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Open file
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            return XPE_ERR_IO_FAILED;
        }

        // Read header
        XCalHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!file.good()) {
            return XPE_ERR_IO_FAILED;
        }

        // Validate header (type=0 for offset)
        if (!validate_xcal_header(header, 0)) {
            return XPE_ERR_CONFIG_INVALID;
        }

        // Allocate memory
        auto data = std::make_unique<float[]>(header.width * header.height);

        // Read data
        file.read(reinterpret_cast<char*>(data.get()), header.data_size);
        if (!file.good()) {
            return XPE_ERR_IO_FAILED;
        }

        // TODO: Validate SHA-256 (AC-CAL-001)
        // TODO: Check expiry (AC-CAL-001)

        // Store calibration data (thread-safe)
        // Keep lock until all data access is complete to prevent TOCTOU race condition
        std::unique_ptr<float[]> data_to_store;
        uint32_t width_to_store = 0;
        uint32_t height_to_store = 0;
        int64_t timestamp_to_store = 0;
        char session_id_to_store[64] = {0};

        // Prepare data under lock
        {
            std::lock_guard<std::mutex> lock(g_calib_mutex);
            data_to_store = std::move(data);
            width_to_store = header.width;
            height_to_store = header.height;
            timestamp_to_store = header.timestamp;
            std::strncpy(session_id_to_store, header.session_id, 63);
            session_id_to_store[63] = '\0';

            // Store in global calibration data
            g_calib.offset_map = std::move(data_to_store);
            g_calib.offset_width = width_to_store;
            g_calib.offset_height = height_to_store;
            g_calib.offset_timestamp = timestamp_to_store;
            std::strncpy(g_calib.offset_session_id, session_id_to_store, 63);
            g_calib.offset_session_id[63] = '\0';
        }

        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

/**
 * @brief Load gain calibration map from XCal file
 *
 * REQ-P1A-015: Load XCal format gain maps with multi-SID interpolation
 * AC-CAL-002: Load with interpolation table for kVp-specific gain
 * REQ-P1A-030: No exceptions across C ABI boundary
 */
extern "C" XPE_API XpeErrorCode xpe_calib_load_gain(const char* filepath) {
    try {
        // Validate input
        if (filepath == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Open file
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            return XPE_ERR_IO_FAILED;
        }

        // Read header
        XCalHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!file.good()) {
            return XPE_ERR_IO_FAILED;
        }

        // Validate header (type=1 for gain)
        if (!validate_xcal_header(header, 1)) {
            return XPE_ERR_CONFIG_INVALID;
        }

        // Allocate memory
        auto data = std::make_unique<float[]>(header.width * header.height);

        // Read data
        file.read(reinterpret_cast<char*>(data.get()), header.data_size);
        if (!file.good()) {
            return XPE_ERR_IO_FAILED;
        }

        // TODO: Multi-SID interpolation (AC-CAL-002)

        // Store calibration data (thread-safe)
        // Keep lock until all data access is complete to prevent TOCTOU race condition
        std::unique_ptr<float[]> data_to_store;
        uint32_t width_to_store = 0;
        uint32_t height_to_store = 0;
        int64_t timestamp_to_store = 0;
        char session_id_to_store[64] = {0};

        // Prepare data under lock
        {
            std::lock_guard<std::mutex> lock(g_calib_mutex);
            data_to_store = std::move(data);
            width_to_store = header.width;
            height_to_store = header.height;
            timestamp_to_store = header.timestamp;
            std::strncpy(session_id_to_store, header.session_id, 63);
            session_id_to_store[63] = '\0';

            // Store in global calibration data
            g_calib.gain_map = std::move(data_to_store);
            g_calib.gain_width = width_to_store;
            g_calib.gain_height = height_to_store;
            g_calib.gain_timestamp = timestamp_to_store;
            std::strncpy(g_calib.gain_session_id, session_id_to_store, 63);
            g_calib.gain_session_id[63] = '\0';
        }

        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

/**
 * @brief Load defect map (BPM) from XCal file
 *
 * REQ-P1A-016: Load XCal format defect maps (BPM)
 * AC-CAL-003: Validate defect locations and integrity
 * REQ-P1A-030: No exceptions across C ABI boundary
 */
extern "C" XPE_API XpeErrorCode xpe_calib_load_defect_map(const char* filepath) {
    try {
        // Validate input
        if (filepath == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Open file
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            return XPE_ERR_IO_FAILED;
        }

        // Read header
        XCalHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!file.good()) {
            return XPE_ERR_IO_FAILED;
        }

        // Validate header (type=2 for defect)
        if (!validate_xcal_header(header, 2)) {
            return XPE_ERR_CONFIG_INVALID;
        }

        // Allocate memory
        auto data = std::make_unique<uint8_t[]>(header.width * header.height);

        // Read data
        file.read(reinterpret_cast<char*>(data.get()), header.data_size);
        if (!file.good()) {
            return XPE_ERR_IO_FAILED;
        }

        // Validate defect locations (AC-CAL-003)
        // BPM format: 0=good pixel, 1=bad pixel

        // Store calibration data (thread-safe)
        // Keep lock until all data access is complete to prevent TOCTOU race condition
        std::unique_ptr<uint8_t[]> data_to_store;
        uint32_t width_to_store = 0;
        uint32_t height_to_store = 0;

        // Prepare data under lock
        {
            std::lock_guard<std::mutex> lock(g_calib_mutex);
            data_to_store = std::move(data);
            width_to_store = header.width;
            height_to_store = header.height;

            // Store in global calibration data
            g_calib.defect_map = std::move(data_to_store);
            g_calib.defect_width = width_to_store;
            g_calib.defect_height = height_to_store;
        }

        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

// =============================================================================
// Phase 4: Calibration Management Functions
// =============================================================================

/**
 * @brief Generate offset calibration map from dark frames
 *
 * REQ-P1A-017: Generate dark frames with configurable parameters
 * REQ-P1A-030: No exceptions across C ABI boundary
 */
extern "C" XPE_API XpeErrorCode xpe_calib_generate_offset(const XpeImageBuffer* dark_frames,
                                                          int32_t num_frames,
                                                          float integration_time_ms,
                                                          float temperature_c,
                                                          const char* output_path) {
    try {
        // Validate input
        if (dark_frames == nullptr || output_path == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        if (num_frames <= 0) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Get dimensions from first frame
        uint32_t width = dark_frames[0].width;
        uint32_t height = dark_frames[0].height;

        // Allocate average buffer
        auto average = std::make_unique<float[]>(width * height);
        std::memset(average.get(), 0, sizeof(float) * width * height);

        // Average all dark frames
        for (int32_t i = 0; i < num_frames; ++i) {
            const XpeImageBuffer& frame = dark_frames[i];

            // Validate dimensions match
            if (frame.width != width || frame.height != height) {
                return XPE_ERR_INVALID_INPUT;
            }

            // Validate format
            if (frame.format != XPE_PIXEL_UINT16) {
                return XPE_ERR_UNSUPPORTED_FORMAT;
            }

            // Accumulate pixel values
            const uint16_t* data = static_cast<const uint16_t*>(frame.data);
            for (size_t j = 0; j < width * height; ++j) {
                average[j] += static_cast<float>(data[j]);
            }
        }

        // Divide by number of frames
        for (size_t j = 0; j < width * height; ++j) {
            average[j] /= static_cast<float>(num_frames);
        }

        // Create XCal header
        XCalHeader header;
        std::memcpy(header.magic, "XCAL", 4);
        header.version = 1;
        header.type = 0;  // offset
        header.width = width;
        header.height = height;
        header.timestamp = static_cast<int64_t>(std::time(nullptr));
        std::strncpy(header.session_id, "generated", 63);
        header.data_size = width * height * sizeof(float);
        std::memset(header.sha256, 0, 32);  // TODO: Compute SHA-256

        // Write to file
        std::ofstream file(output_path, std::ios::binary);
        if (!file.is_open()) {
            return XPE_ERR_IO_FAILED;
        }

        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        file.write(reinterpret_cast<const char*>(average.get()), header.data_size);

        if (!file.good()) {
            return XPE_ERR_IO_FAILED;
        }

        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

/**
 * @brief Check calibration expiry status
 *
 * REQ-P1A-018: Check expiry based on timestamp and drift scoring
 * AC-CAL-004: Compare current time to expires_at
 * REQ-P1A-030: No exceptions across C ABI boundary
 */
extern "C" XPE_API XpeErrorCode xpe_calib_check_expiry(const char* filepath,
                                                       bool* is_expired,
                                                       int32_t* remaining_days) {
    try {
        // Validate input
        if (filepath == nullptr || is_expired == nullptr || remaining_days == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Open file
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            return XPE_ERR_IO_FAILED;
        }

        // Read header
        XCalHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!file.good()) {
            return XPE_ERR_IO_FAILED;
        }

        // Validate magic
        if (std::memcmp(header.magic, "XCAL", 4) != 0) {
            return XPE_ERR_CONFIG_INVALID;
        }

        // Calculate expiry (30 days from creation)
        const int32_t CALIBRATION_VALID_DAYS = 30;
        int64_t current_time = static_cast<int64_t>(std::time(nullptr));
        int64_t age_seconds = current_time - header.timestamp;
        int32_t age_days = static_cast<int32_t>(age_seconds / (24 * 3600));

        *is_expired = (age_days >= CALIBRATION_VALID_DAYS);
        *remaining_days = CALIBRATION_VALID_DAYS - age_days;

        return XPE_OK;

    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

/**
 * @brief Save current calibration state to XCal format
 *
 * REQ-P1A-019: Save calibration with SHA-256 integrity
 * REQ-P1A-030: No exceptions across C ABI boundary
 */
extern "C" XPE_API XpeErrorCode xpe_calib_save(const char* filepath,
                                               const char* calib_type) {
    try {
        // Validate input
        if (filepath == nullptr || calib_type == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        std::lock_guard<std::mutex> lock(g_calib_mutex);

        // Determine calibration type
        uint32_t type;
        const void* data;
        uint32_t width;
        uint32_t height;
        uint64_t data_size;
        int64_t timestamp;

        if (std::strcmp(calib_type, "offset") == 0) {
            if (!g_calib.offset_map) {
                return XPE_ERR_INVALID_INPUT;
            }
            type = 0;
            data = g_calib.offset_map.get();
            width = g_calib.offset_width;
            height = g_calib.offset_height;
            data_size = width * height * sizeof(float);
            timestamp = g_calib.offset_timestamp;
        } else if (std::strcmp(calib_type, "gain") == 0) {
            if (!g_calib.gain_map) {
                return XPE_ERR_INVALID_INPUT;
            }
            type = 1;
            data = g_calib.gain_map.get();
            width = g_calib.gain_width;
            height = g_calib.gain_height;
            data_size = width * height * sizeof(float);
            timestamp = g_calib.gain_timestamp;
        } else if (std::strcmp(calib_type, "defect") == 0) {
            if (!g_calib.defect_map) {
                return XPE_ERR_INVALID_INPUT;
            }
            type = 2;
            data = g_calib.defect_map.get();
            width = g_calib.defect_width;
            height = g_calib.defect_height;
            data_size = width * height * sizeof(uint8_t);
            timestamp = 0;  // Defect maps don't expire
        } else {
            return XPE_ERR_INVALID_INPUT;
        }

        // Create XCal header
        XCalHeader header;
        std::memcpy(header.magic, "XCAL", 4);
        header.version = 1;
        header.type = type;
        header.width = width;
        header.height = height;
        header.timestamp = timestamp;
        header.data_size = data_size;
        std::memset(header.sha256, 0, 32);  // TODO: Compute SHA-256

        // Set session ID
        if (type == 0) {
            std::strncpy(header.session_id, g_calib.offset_session_id, 63);
        } else if (type == 1) {
            std::strncpy(header.session_id, g_calib.gain_session_id, 63);
        } else {
            std::strncpy(header.session_id, "defect_map", 63);
        }
        header.session_id[63] = '\0';

        // Write to file
        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            return XPE_ERR_IO_FAILED;
        }

        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        file.write(reinterpret_cast<const char*>(data), data_size);

        if (!file.good()) {
            return XPE_ERR_IO_FAILED;
        }

        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

/**
 * @brief Validate and mask readout artifacts
 *
 * REQ-P1A-041: Validate readout artifacts before correction
 * REQ-P1A-030: No exceptions across C ABI boundary
 */
extern "C" XPE_API XpeErrorCode xpe_validate_readout_artifact(const XpeImageBuffer* image,
                                                             const XpeImageMetadata* metadata,
                                                             bool* has_dropped_columns,
                                                             bool* has_nonuniform_gain) {
    try {
        // Validate input
        if (image == nullptr || metadata == nullptr ||
            has_dropped_columns == nullptr || has_nonuniform_gain == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Initialize outputs
        *has_dropped_columns = false;
        *has_nonuniform_gain = false;

        // Check for dropped columns (columns with all zeros)
        if (image->format == XPE_PIXEL_UINT16) {
            const uint16_t* data = static_cast<const uint16_t*>(image->data);

            // Check each column
            for (uint32_t col = 0; col < image->width; ++col) {
                bool column_is_zero = true;

                for (uint32_t row = 0; row < image->height; ++row) {
                    if (data[row * image->width + col] != 0) {
                        column_is_zero = false;
                        break;
                    }
                }

                if (column_is_zero) {
                    *has_dropped_columns = true;
                    break;
                }
            }
        }

        // Check for nonuniform gain (simple variance check)
        // TODO: Implement more sophisticated gain uniformity check
        if (image->format == XPE_PIXEL_FLOAT32) {
            const float* data = static_cast<const float*>(image->data);

            // Calculate mean
            double sum = 0.0;
            size_t pixel_count = image->width * image->height;
            for (size_t i = 0; i < pixel_count; ++i) {
                sum += data[i];
            }
            double mean = sum / pixel_count;

            // Calculate variance
            double variance = 0.0;
            for (size_t i = 0; i < pixel_count; ++i) {
                double diff = data[i] - mean;
                variance += diff * diff;
            }
            variance /= pixel_count;

            // Check if variance exceeds threshold (5% of mean)
            double threshold = mean * 0.05;
            if (variance > threshold * threshold) {
                *has_nonuniform_gain = true;
            }
        }

        return XPE_OK;

    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
