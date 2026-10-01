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
#include <new>
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
        // QA-A-202c (#233, Codex #27 B1): the new logger is built COMPLETELY -- sink, logger, level -- before
        // anything that belongs to the previous one is let go, and the previous logger is released only after
        // the new one is installed as the default. A failure at any step (an unusable directory, a file that
        // cannot be opened, an allocation failure) therefore leaves the caller writing to the file they had.
        // It used to drop the previous logger first (REQ-GUI-IT-030: repeat calls must not collide), so a
        // failure after that left no logger and a default that pointed at nothing useful.
        std::shared_ptr<spdlog::logger> fresh;

        if (filePath == nullptr) {
            // A dedicated null-sink so the spdlog default is always valid. spdlog::set_default_logger(
            // spdlog::default_logger()) would be a no-op when g_logger was already null, leaving the old
            // (possibly freed) logger as default; a dedicated null-sink avoids the crash in xpe_log_flush()
            // caused by a dangling default_logger_ pointer.
            fresh = std::make_shared<spdlog::logger>(
                "xpe_null_revert",
                std::make_shared<spdlog::sinks::null_sink_mt>());
            fresh->set_level(to_spdlog_level(g_currentLevel));
        } else {
            // Validate parent directory existence BEFORE touching spdlog
            // (basic_file_sink does not auto-create directories reliably on Windows
            //  and may swallow failures depending on OS/spdlog build).
            std::filesystem::path p(filePath);
            auto parent = p.parent_path();
            if (!parent.empty() && !std::filesystem::exists(parent)) {
                return XPE_ERR_IO_FAILED;
            }

            // The same path may be opened again while the previous logger still holds it: flush first so
            // nothing buffered is written after the file is truncated by the new sink. (A failure after
            // that point for the SAME path leaves the file truncated; the logger is still the previous one.)
            if (g_logger) g_logger->flush();

            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filePath, true);
            fresh = std::make_shared<spdlog::logger>("xpe_file", file_sink);
            fresh->set_level(to_spdlog_level(g_currentLevel));
            fresh->flush_on(to_spdlog_level(g_currentLevel));
        }

        // The step that installs the new logger. If it throws, the previous logger is still the default.
        spdlog::set_default_logger(fresh);

        // Installed: now the previous file logger can go. Its registry name is dropped only when the new
        // logger has a different one -- a file-to-file switch re-registered "xpe_file" for the new logger.
        std::shared_ptr<spdlog::logger> previous = std::move(g_logger);
        g_logger = (filePath != nullptr) ? fresh : nullptr;
        if (previous) {
            previous->flush();
            if (previous->name() != fresh->name()) spdlog::drop(previous->name());
        }
        return XPE_OK;
    } catch (const std::bad_alloc&) {
        // Running out of memory is not an I/O fault (QA-A-204, #233).
        return XPE_ERR_OUT_OF_MEMORY;
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
            g_logger->flush();
        } catch (...) {}
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
