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
#include <spdlog/sinks/null_sink.h>
#include <filesystem>
#include <memory>
#include <mutex>

// @MX:NOTE: [AUTO] g_logMutex guards all logger state mutations — safe for concurrent callers (REQ-P0-022)

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
        // Always release any prior custom logger so its file handle is closed
        // before we open a new one (REQ-GUI-IT-030: repeat calls must not collide).
        if (g_logger) {
            g_logger->flush();
            spdlog::drop("xpe_file");
            g_logger.reset();
        }

        if (filePath == nullptr) {
            // Install a fresh null-sink so the spdlog default is always valid.
            // spdlog::set_default_logger(spdlog::default_logger()) is a no-op
            // when g_logger was already null, leaving the old (possibly freed)
            // logger as default.  Using a dedicated null-sink avoids the crash
            // in xpe_log_flush() caused by a dangling default_logger_ pointer.
            try {
                spdlog::drop("xpe_null_revert");
            } catch (...) {}
            auto null_sink = std::make_shared<spdlog::logger>(
                "xpe_null_revert",
                std::make_shared<spdlog::sinks::null_sink_mt>());
            null_sink->set_level(to_spdlog_level(g_currentLevel));
            spdlog::set_default_logger(null_sink);
            return XPE_OK;
        }

        // Validate parent directory existence BEFORE touching spdlog
        // (basic_file_sink does not auto-create directories reliably on Windows
        //  and may swallow failures depending on OS/spdlog build).
        std::filesystem::path p(filePath);
        auto parent = p.parent_path();
        if (!parent.empty() && !std::filesystem::exists(parent)) {
            return XPE_ERR_IO_FAILED;
        }

        // Create fresh file sink
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filePath, true);
        g_logger = std::make_shared<spdlog::logger>("xpe_file", file_sink);
        g_logger->set_level(to_spdlog_level(g_currentLevel));
        g_logger->flush_on(to_spdlog_level(g_currentLevel));
        spdlog::set_default_logger(g_logger);
        return XPE_OK;
    } catch (...) {
        // spdlog exceptions (file permissions, disk full, etc.)
        return XPE_ERR_IO_FAILED;
    }
}

// Internal helper invoked by xpe_shutdown to release the custom file sink so
// that callers (tests, hosts) can delete/rotate the underlying log file.
// After reset, spdlog::default_logger() points at a null-sink logger so that
// xpe_log_flush() (and any other default-logger consumer) is safe to call.
void xpe_log_internal_reset() {
    std::lock_guard<std::mutex> lock(g_logMutex);

    // Install a null-sink default FIRST so that default_logger() never holds a
    // dangling pointer to the logger we are about to drop.
    try {
        spdlog::drop("xpe_default_null");
        auto null_logger = std::make_shared<spdlog::logger>(
            "xpe_default_null",
            std::make_shared<spdlog::sinks::null_sink_mt>());
        spdlog::set_default_logger(null_logger);
    } catch (...) {
        // set_default_logger must not break the reset sequence
    }

    if (g_logger) {
        try {
            spdlog::drop("xpe_file");
        } catch (...) {}
        g_logger.reset();
    }
}

XPE_API void xpe_log_flush(void) {
    std::lock_guard<std::mutex> lock(g_logMutex);

    if (g_logger) {
        g_logger->flush();
    } else {
        // Guard: default_logger() can be null if spdlog::drop() was called
        // on the default logger name before set_default_logger() completed.
        auto def = spdlog::default_logger();
        if (def) {
            def->flush();
        }
    }
}

} // extern "C"
