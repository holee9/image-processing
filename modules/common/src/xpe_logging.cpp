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
#include <spdlog/sinks/base_sink.h>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <new>
#include <mutex>

// @MX:NOTE: [AUTO] g_logMutex guards all logger state mutations — safe for concurrent callers (REQ-P0-022)

// @MX:ANCHOR: [AUTO] spdlog backend singleton -- REQ-P0-023
// @MX:REASON: Central logging state; shared across all xpe modules

static std::mutex g_logMutex;
static std::shared_ptr<spdlog::logger> g_logger = nullptr;
static int g_currentLevel = 2; // Default: INFO (2) -- REQ-P0-011, SRS-FUNC-040

/**
 * Convert XPE log level (0-5) to spdlog level.
 * @param level XPE log level: 0=TRACE, 1=DEBUG, 2=INFO, 3=WARN, 4=ERROR, 5=OFF
 * @return spdlog::level::level_enum
 *
 * 5 is OFF, as the header and SRS-FUNC-040 say: nothing is recorded, not even a critical line. (It used to
 * map to critical, which left the one level a caller picks to silence the library recording its worst lines.)
 */
static spdlog::level::level_enum to_spdlog_level(int level) {
    switch (level) {
        case 0: return spdlog::level::trace;
        case 1: return spdlog::level::debug;
        case 2: return spdlog::level::info;
        case 3: return spdlog::level::warn;
        case 4: return spdlog::level::err;
        case 5: return spdlog::level::off;
        default: return spdlog::level::info;
    }
}

// Writes each line through the C stream `stderr`, looked up at every write. spdlog's own stderr sink keeps the
// operating-system handle it found when it was built; a host that redirects file descriptor 2 afterwards
// (a test harness, a service wrapper) leaves that handle dead and the lines are lost with only a spdlog error
// message. Going through the stream follows the redirection.
class StderrStreamSink final : public spdlog::sinks::base_sink<std::mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        formatter_->format(msg, formatted);
        std::fwrite(formatted.data(), 1, formatted.size(), stderr);
        std::fflush(stderr);
    }
    void flush_() override { std::fflush(stderr); }
};

// The logger that writes to stderr, at the level the caller has chosen (INFO until then).
static std::shared_ptr<spdlog::logger> make_stderr_logger(int level) {
    auto logger = std::make_shared<spdlog::logger>(
        "xpe_stderr",
        std::make_shared<StderrStreamSink>());
    logger->set_level(to_spdlog_level(level));
    return logger;
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
        // QA-A-202d (#233, Codex #32 B1): the switch has two halves, and the line between them is the
        // install of the new default logger.
        //
        //   BEFORE it -- everything that can fail: the directory check, flushing the logger the caller has,
        //   opening the file, building the sink and the logger. A failure at any of these returns an error and
        //   leaves the caller's logger, and the spdlog default pointing at it, exactly as they were.
        //   AT it -- spdlog::set_default_logger, the one step that may still throw (it allocates a registry
        //   node); if it does, the previous logger is still the default.
        //   AFTER it -- only operations that cannot fail: moving shared_ptrs, releasing the previous logger.
        //   (The previous logger used to be flushed here, after the output had already moved: a flush that
        //   failed reported an error for a switch that had happened.)
        //
        // REQ-GUI-IT-030 ("repeat calls must not collide") is kept by flushing the previous logger first and
        // opening the new file in APPEND mode, as the header says: naming the same path again neither
        // truncates it nor loses the lines written so far, and two handles on one file is allowed here.
        std::filesystem::path p;
        if (filePath != nullptr) {
            // Validate parent directory existence BEFORE touching spdlog
            // (basic_file_sink does not auto-create directories reliably on Windows
            //  and may swallow failures depending on OS/spdlog build).
            p = filePath;
            auto parent = p.parent_path();
            if (!parent.empty() && !std::filesystem::exists(parent)) {
                return XPE_ERR_IO_FAILED;
            }
        }

        // The previous logger's sinks are flushed directly: spdlog::logger::flush() hands a failure to the
        // logger's error handler (which prints to stderr) and returns, so it could not tell us.
        if (g_logger) {
            for (const auto& sink : g_logger->sinks()) sink->flush();
        }

        std::shared_ptr<spdlog::logger> fresh;
        if (filePath == nullptr) {
            // NULL reverts to stderr (header, SRS-FUNC-041). A dedicated logger is installed rather than
            // spdlog::set_default_logger(spdlog::default_logger()), which would be a no-op when g_logger was
            // already null and leave the old (possibly freed) logger as default -- the dangling default_logger_
            // pointer that crashed xpe_log_flush(). (QA-A-232: this branch used to install a null sink, so a
            // caller who passed NULL to get stderr back lost every line instead.)
            fresh = make_stderr_logger(g_currentLevel);
        } else {
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filePath, /*truncate=*/false);
            fresh = std::make_shared<spdlog::logger>("xpe_file", file_sink);
            fresh->set_level(to_spdlog_level(g_currentLevel));
            fresh->flush_on(to_spdlog_level(g_currentLevel));
        }

        // The install. If it throws, nothing has changed.
        spdlog::set_default_logger(fresh);

        // Installed. From here nothing may fail (and nothing allocates: names are compared by reference).
        std::shared_ptr<spdlog::logger> previous = std::move(g_logger);
        const bool sameName = previous && previous->name() == fresh->name();
        g_logger = (filePath != nullptr) ? fresh : nullptr;
        if (previous && !sameName) {
            // spdlog (1.14, registry-inl.h) registers the new default under its name and leaves the previous
            // default's entry in the registry, so a previous logger with another name is dropped here. A
            // file-to-file switch re-registered "xpe_file" for the new logger (the insert replaced the entry),
            // so that name must NOT be dropped: drop() would erase the new logger's entry.
            try {
                spdlog::drop(previous->name());
            } catch (...) {
                // [no-throw-boundary] the dropped logger is only a registry entry; a failed drop leaves an unused name behind, nothing else
            }
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

// Internal helper invoked by xpe_init: the default destination is stderr (REQ-P0-011). A file the caller
// chose before xpe_init stays; so does a level the caller chose (the level is INFO from the start and again
// after xpe_shutdown, so "INFO" needs no reset here). Failing to install the logger (allocation) leaves the
// previous default in place; xpe_init has no error to report for it.
void xpe_log_internal_init() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logger) return;
    try {
        auto fresh = make_stderr_logger(g_currentLevel);
        auto previous = spdlog::default_logger();
        spdlog::set_default_logger(fresh);
        if (previous && previous->name() != fresh->name() && previous->name().rfind("xpe_", 0) == 0) {
            spdlog::drop(previous->name());
        }
    } catch (...) {
        // [no-throw-boundary] a failed install of the stderr default leaves the previous default logger in place; xpe_init has no error to return
    }
}

// The library's own lines (xpe_init writes one) go through the spdlog default logger, as every other module's
// lines do: the file the caller chose, or stderr, under the same level (5 = OFF writes nothing). (xpe_log_set_file
// makes its logger the default, so there is no second destination to pick between.)
void xpe_log_internal_write(int level, const char* msg) {
    try {
        std::lock_guard<std::mutex> lock(g_logMutex);
        std::shared_ptr<spdlog::logger> target = spdlog::default_logger();
        if (target) target->log(to_spdlog_level(level), msg);
    } catch (...) {
        // [no-throw-boundary] a log line must never turn into an error for the caller
        // a log line must never turn into an error for the caller
    }
}

// Internal helper invoked by xpe_shutdown to release the custom file sink so
// that callers (tests, hosts) can delete/rotate the underlying log file.
// After reset, spdlog::default_logger() points at a null-sink logger so that
// xpe_log_flush() (and any other default-logger consumer) is safe to call.
void xpe_log_internal_reset() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_currentLevel = 2;  // back to the default; a level chosen in one session does not outlive it

    // Install a null-sink default FIRST so that default_logger() never holds a
    // dangling pointer to the logger we are about to drop.
    try {
        spdlog::drop("xpe_default_null");
        auto null_logger = std::make_shared<spdlog::logger>(
            "xpe_default_null",
            std::make_shared<spdlog::sinks::null_sink_mt>());
        spdlog::set_default_logger(null_logger);
    } catch (...) {
        // [no-throw-boundary] set_default_logger must not break the reset sequence
    }

    if (g_logger) {
        try {
            g_logger->flush();
        } catch (...) {
            // [no-throw-boundary] a failed flush at shutdown cannot be reported; the file is closed next anyway
        }
        try {
            spdlog::drop("xpe_file");
        } catch (...) {
            // [no-throw-boundary] a failed drop at shutdown leaves only an unused registry name
        }
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

#ifdef XPE_COMMON_TEST_HOOKS
/* Test-only (QA-A-202d, Codex #32 B1): make `logger` both the module's file logger and the spdlog default, as
 * a successful xpe_log_set_file would, so a test can stand a sink whose flush fails behind it. Compiled only
 * into the allocation-failure executable, which defines XPE_COMMON_TEST_HOOKS; the shipped library does not
 * have it. */
void xpe_log_adopt_logger_for_test(std::shared_ptr<spdlog::logger> logger)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    spdlog::set_default_logger(logger);
    g_logger = std::move(logger);
}
#endif
