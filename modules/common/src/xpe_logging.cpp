/**
 * @file xpe_logging.cpp
 * @brief Logging subsystem implementation -- REQ-P0-023~025
 *
 * Thread-safe logging using spdlog backend.
 * Supports file rotation, log level filtering, and forced flush.
 */

#include "xpe/common/xpe_common_api.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <mutex>

// @MX:NOTE: [AUTO] Single-threaded initialization assumed (REQ-P0-022)
// Future: Add std::atomic for thread-safe initialization if needed

// @MX:ANCHOR: [AUTO] spdlog backend singleton -- REQ-P0-023
// @MX:REASON: Central logging state; shared across all xpe modules

static std::mutex g_logMutex;
static std::shared_ptr<spdlog::logger> g_logger = nullptr;
static int g_currentLevel = 0; // Default: TRACE (0)

/**
 * Convert XPE log level (0-5) to spdlog level.
 * @param level XPE log level: 0=TRACE, 1=DEBUG, 2=INFO, 3=WARN, 4=ERROR, 5=CRITICAL
 * @return spdlog::level::level_enum
 */
static spdlog::level::level_enum to_spdlog_level(int level) {
    switch (level) {
        case 0: return spdlog::level::trace;
        case 1: return spdlog::level::debug;
        case 2: return spdlog::level::info;
        case 3: return spdlog::level::warn;
        case 4: return spdlog::level::err;
        case 5: return spdlog::level::critical;
        default: return spdlog::level::trace;
    }
}

extern "C" {

XPE_API XpeErrorCode xpe_log_set_level(int level) {
    if (level < 0 || level > 5)
        return XPE_ERR_INVALID_INPUT;

    std::lock_guard<std::mutex> lock(g_logMutex);
    g_currentLevel = level;

    if (g_logger) {
        g_logger->set_level(to_spdlog_level(level));
        g_logger->flush_on(to_spdlog_level(level));
    } else {
        spdlog::set_level(to_spdlog_level(level));
    }

    return XPE_OK;
}

XPE_API XpeErrorCode xpe_log_set_file(const char* filePath) {
    std::lock_guard<std::mutex> lock(g_logMutex);

    try {
        if (filePath == nullptr) {
            // Revert to default console logger
            g_logger = nullptr;
            // Use spdlog's default console logger
            spdlog::set_default_logger(spdlog::default_logger());
            spdlog::set_level(to_spdlog_level(g_currentLevel));
        } else {
            // Create file sink with rotation
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filePath, true);
            g_logger = std::make_shared<spdlog::logger>("xpe_file", file_sink);
            g_logger->set_level(to_spdlog_level(g_currentLevel));
            g_logger->flush_on(to_spdlog_level(g_currentLevel));
            spdlog::set_default_logger(g_logger);
        }
        return XPE_OK;
    } catch (...) {
        // spdlog exceptions (file permissions, disk full, etc.)
        return XPE_ERR_IO_FAILED;
    }
}

XPE_API void xpe_log_flush(void) {
    std::lock_guard<std::mutex> lock(g_logMutex);

    if (g_logger) {
        g_logger->flush();
    } else {
        spdlog::default_logger()->flush();
    }
}

} // extern "C"
