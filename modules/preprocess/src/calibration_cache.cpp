/**
 * @file calibration_cache.cpp
 * @brief LRU cache for calibration maps to eliminate repeated file I/O.
 *
 * Keeps recently used calibration maps in memory. The cache key is the file
 * path string exactly as the caller passed it. A hit leaves the module-global
 * calibration store holding the cached map, as a miss does (QA-A-193, #216), and
 * reaches the verdict a miss would reach (QA-A-196, #216):
 *  - expiry is re-checked on every hit from the entry's stored expiry;
 *  - the file's size and last-write time are compared with what the entry recorded, and a
 *    difference (or a file that cannot be examined) cancels the hit so the miss path reloads
 *    and re-hashes it;
 *  - the file is opened for reading once (opening only): a file whose attributes are visible but
 *    whose content cannot be opened is refused with IO_FAILED, as a miss would;
 *  - the gain quality metadata of the file, kept parsed in the entry, is made current again.
 * A hit does NOT re-hash the file: a change that keeps both the size and the last-write time is
 * not noticed until xpe_calib_cache_clear() (or module shutdown, which empties the cache).
 * Concurrent writers are not supported: the file must not be written while a calibration load is in
 * progress (a second look at the attributes just before the install would not close every such race,
 * so none is made -- QA-A-200).
 * Thread-safety: all LRU/index mutations are protected by an internal mutex (IEC 62304 Class B).
 *
 * SPEC: SPEC-XPE-P1A v1.0.0
 * IEC 62304 Class B
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <list>
#include <memory>
#include <mutex>
#include <new>
#include <unordered_map>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

/* =========================================================================
 * Internal cache structures
 * ========================================================================= */

namespace {

/**
 * @brief Cached calibration map entry.
 *
 * Owns the pixel data buffer (allocated via malloc, freed on eviction).
 */
/** Size and last-write time of a file, as the cache compares them on a hit. */
struct FileStamp {
    bool     ok{false};
    uint64_t size{0};
    int64_t  mtime{0};
    bool operator==(const FileStamp& o) const noexcept {
        return ok && o.ok && size == o.size && mtime == o.mtime;
    }
};

FileStamp stamp_of(const char* path) noexcept
{
    FileStamp st;
    try {
        std::error_code ec;
        const std::filesystem::path p(path);
        const auto size = std::filesystem::file_size(p, ec);
        if (ec) return st;
        const auto mtime = std::filesystem::last_write_time(p, ec);
        if (ec) return st;
        st.size  = static_cast<uint64_t>(size);
        st.mtime = static_cast<int64_t>(mtime.time_since_epoch().count());
        st.ok    = true;
    } catch (...) {
        st.ok = false;
    }
    return st;
}

/**
 * Whether the file can be opened for reading, the way the XCal reader opens it. Reading the size and
 * the write time succeeding does not imply this (a file can show its attributes and refuse its
 * content), and a miss would report IO_FAILED, so a hit has to try the open too (QA-A-200).
 */
bool can_open_for_read(const char* path) noexcept
{
    try {
        std::ifstream f(path, std::ios::binary);
        return f.is_open();
    } catch (...) {
        return false;
    }
}

int64_t now_epoch_ms() noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

/**
 * What the plain loader and the file record next to the pixels. Kept in the entry so that a hit
 * can put the global store back exactly as the loader did and judge the file as the read would.
 * timestamp / sessionId are zero for a defect map; the quality metadata is kept for gain only.
 * `kind` records the map kind the plain loader validated (QA-A-197): the key is the path alone, and a
 * file holds one kind of map, so a hit from a loader of another kind must be refused as a miss would.
 */
enum class MapKind { None, Offset, Gain, Defect };

struct EntryMeta {
    MapKind     kind{MapKind::None};  ///< which cached loader published it = which XCal type was validated
    int64_t     timestamp{0};
    char        sessionId[64]{};
    int64_t     expiryMs{0};   ///< file's expiry_epoch_ms, 0 = never expires
    FileStamp   stamp;         ///< size and last-write time taken before the file was read
    XpeCalibQualityMeta quality{};   ///< gain only: the parsed FUNC-033 metadata of the file
    bool        hasQuality{false};
};

enum class HitState { Miss, Hit, Expired };

struct CachedMap {
    XpeImageBuffer buffer;  ///< Image buffer (owns data pointer)
    std::string    path;    ///< File path used as cache key
    EntryMeta      meta;
};

/**
 * @brief LRU cache for calibration maps.
 *
 * Uses std::list for LRU ordering and unordered_map for O(1) lookup.
 * Maximum capacity is configurable via xpe_calib_cache_set_max_size().
 */
class CalibrationLRUCache {
public:
    using ListIter = std::list<CachedMap>::iterator;

    CalibrationLRUCache() = default;

    ~CalibrationLRUCache() { clear(); }

    // Non-copyable, non-movable
    CalibrationLRUCache(const CalibrationLRUCache&) = delete;
    CalibrationLRUCache& operator=(const CalibrationLRUCache&) = delete;

    /**
     * @brief Look up a cached map by file path.
     * @param path File path string
     * @param out  [out] Populated with cached buffer data on hit
     * @return true on cache hit, false on miss
     */
    bool get(const std::string& path, XpeImageBuffer* out) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = index_.find(path);
        if (it == index_.end()) return false;

        // Move to front (most recently used)
        lru_.splice(lru_.begin(), lru_, it->second);

        const CachedMap& entry = *it->second;

        // Deep copy the buffer metadata; caller gets their own view of shared data
        // Note: The data pointer is shared — callers must NOT free it.
        if (out) {
            std::memcpy(out, &entry.buffer, sizeof(XpeImageBuffer));
        }
        return true;
    }

    /**
     * @brief get(), plus a private copy of the entry's pixels and metadata taken under the same lock.
     *
     * QA-A-193 (#216): a hit must leave the module-global calibration store holding this map. The
     * install happens outside the cache lock (g_calib_mutex is never taken while this mutex is
     * held, so the two cannot deadlock), which is why the pixels are copied out here: the cache's
     * own buffer may be evicted the moment the lock is released.
     *
     * QA-A-196 (#216): a hit must reach the verdict a miss would. `now` is the file's current
     * size and write time: when it differs from the entry's (or either could not be read), the entry
     * is dropped and the result is Miss, so the caller reloads the file. When the entry's expiry has
     * passed, the entry is dropped and the result is Expired. When the entry holds a map of another
     * kind than `kind` (QA-A-197), the result is Miss and the entry stays.
     */
    template <typename T>
    XpeErrorCode get_copy(const std::string& path, MapKind kind, const FileStamp& now, int64_t nowMs,
                          XpeImageBuffer* view, std::unique_ptr<T[]>* pixels, EntryMeta* meta,
                          HitState* state) {
        std::lock_guard<std::mutex> lock(mutex_);
        *state = HitState::Miss;
        auto it = index_.find(path);
        if (it == index_.end()) return XPE_OK;

        if (!(it->second->meta.stamp == now)) {
            erase_locked(it);
            return XPE_OK;
        }
        // Another kind of loader asked for a path whose entry holds a different kind of map: the plain
        // loader of the asking kind would refuse that file (wrong XCal type), so this is a miss for it.
        // The entry is left alone -- its own loader still hits it.
        if (it->second->meta.kind != kind) return XPE_OK;
        if (it->second->meta.expiryMs != 0 && nowMs > it->second->meta.expiryMs) {
            erase_locked(it);
            *state = HitState::Expired;
            return XPE_OK;
        }

        lru_.splice(lru_.begin(), lru_, it->second);
        const CachedMap& entry = *it->second;

        // One copy, straight into the array the global store will own: a 3072x3072 float map is
        // 37.7 MB, and a second copy cost as much as the first (measured, QA-A-193).
        const size_t bytes = static_cast<size_t>(entry.buffer.dataSize);
        const size_t n = static_cast<size_t>(entry.buffer.width) * entry.buffer.height;
        if (n == 0 || bytes != n * sizeof(T)) return XPE_ERR_PROCESSING_FAILED;
        std::unique_ptr<T[]> copy(new (std::nothrow) T[n]);
        if (!copy) return XPE_ERR_OUT_OF_MEMORY;
        std::memcpy(copy.get(), entry.buffer.data, bytes);

        std::memcpy(view, &entry.buffer, sizeof(XpeImageBuffer));
        *pixels = std::move(copy);
        *meta = entry.meta;
        *state = HitState::Hit;
        return XPE_OK;
    }

    /**
     * @brief Insert a new entry into the cache.
     *
     * If the key already exists, the old entry is replaced.
     * If the cache is full, the least recently used entry is evicted.
     *
     * @param path   File path (cache key)
     * @param buffer Image buffer to cache (takes ownership of buffer.data)
     */
    void put(const std::string& path, XpeImageBuffer* buffer) {
        if (!buffer || !buffer->data) return;

        std::lock_guard<std::mutex> lock(mutex_);
        put_locked(path, buffer, EntryMeta{});
    }

    /**
     * @brief Insert an entry and return the cache's view of it, atomically.
     *
     * QA-A-31 (#127): the miss path used to call put() and then get() as two
     * separate lock scopes. A cache_clear() or an eviction landing between them
     * left nothing to read back, and the loader returned
     * XPE_ERR_PROCESSING_FAILED for a call that had in fact succeeded. Holding
     * the lock across both halves closes that window.
     *
     * @param path   File path (cache key)
     * @param buffer Image buffer to cache (takes ownership of buffer.data)
     * @param out    [out] Populated with the cache-owned view on success
     * @return true when the entry was inserted and read back
     */
    bool put_and_get(const std::string& path, XpeImageBuffer* buffer,
                     XpeImageBuffer* out, const EntryMeta& meta) {
        if (!buffer || !buffer->data) return false;

        std::lock_guard<std::mutex> lock(mutex_);
        put_locked(path, buffer, meta);

        auto it = index_.find(path);
        if (it == index_.end()) return false;
        if (out) {
            std::memcpy(out, &it->second->buffer, sizeof(XpeImageBuffer));
        }
        return true;
    }

private:
    /** Frees and unlinks one entry; the caller holds mutex_. */
    void erase_locked(std::unordered_map<std::string, ListIter>::iterator it) {
        std::free(it->second->buffer.data);
        lru_.erase(it->second);
        index_.erase(it);
    }

    /** put(), with the caller already holding mutex_. */
    void put_locked(const std::string& path, XpeImageBuffer* buffer, const EntryMeta& meta) {
        // Check if already cached — replace
        auto it = index_.find(path);
        if (it != index_.end()) {
            // Free old data
            std::free(it->second->buffer.data);
            lru_.erase(it->second);
            index_.erase(it);
        }

        // Evict LRU if at capacity
        while (lru_.size() >= maxSize_ && !lru_.empty()) {
            CachedMap& oldest = lru_.back();
            std::free(oldest.buffer.data);
            index_.erase(oldest.path);
            lru_.pop_back();
        }

        // Insert new entry at front
        CachedMap entry;
        entry.path = path;
        entry.meta = meta;
        // Transfer ownership of buffer data to cache
        std::memcpy(&entry.buffer, buffer, sizeof(XpeImageBuffer));
        // Clear the caller's pointer to prevent double-free
        buffer->data = nullptr;
        buffer->dataSize = 0;

        lru_.push_front(std::move(entry));
        index_[path] = lru_.begin();
    }

public:
    /**
     * @brief Remove all entries from the cache, freeing all buffers.
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& entry : lru_) {
            std::free(entry.buffer.data);
        }
        lru_.clear();
        index_.clear();
    }

    /**
     * @brief Set the maximum number of cached maps.
     *
     * If the new size is smaller than the current number of entries,
     * excess entries are evicted (LRU first).
     */
    void setMaxSize(uint32_t maxSize) {
        std::lock_guard<std::mutex> lock(mutex_);
        maxSize_ = (maxSize > 0) ? maxSize : 1;
        while (lru_.size() > maxSize_ && !lru_.empty()) {
            CachedMap& oldest = lru_.back();
            std::free(oldest.buffer.data);
            index_.erase(oldest.path);
            lru_.pop_back();
        }
    }

    uint32_t getMaxSize() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return maxSize_;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return lru_.size();
    }

private:
    mutable std::mutex                                  mutex_;
    std::list<CachedMap>                                lru_;
    std::unordered_map<std::string, ListIter>           index_;
    uint32_t                                            maxSize_{4};
};

// @MX:NOTE: [AUTO] Singleton cache instance — module-scoped, not thread-safe
CalibrationLRUCache g_calibCache;

/**
 * @brief Completes a cache miss under the cache-owned-view contract (#127).
 *
 * Allocates exactly one buffer, hands ownership to the cache, then reads the
 * entry back so the caller receives the CACHE's pointer -- the same pointer a
 * later hit returns.
 *
 * The caller's XpeImageBuffer is filled by value only. Its incoming `data` is
 * never freed or reallocated: under this contract the caller never owned one,
 * and a stale value there may be another live cache entry's pointer.
 *
 * @MX:ANCHOR: [AUTO] single ownership seam for all three cached loaders
 * @MX:REASON: the previous per-loader miss path reallocated the caller pointer,
 *             so hit and miss returned differently-owned buffers (#127).
 * @MX:SPEC: api-spec.md 6 "Cached loaders - ownership"
 */
XpeErrorCode publish_and_view(const std::string& path,
                              const XpeImageBuffer& desc,
                              const void* src,
                              XpeImageBuffer* out,
                              const EntryMeta& meta)
{
    XpeImageBuffer entry = desc;
    entry.data = std::malloc(static_cast<size_t>(entry.dataSize));
    if (!entry.data) return XPE_ERR_OUT_OF_MEMORY;
    std::memcpy(entry.data, src, static_cast<size_t>(entry.dataSize));

    // Insert and read back under ONE lock (QA-A-31, #127). Splitting these into
    // put() + get() left a window in which another thread's cache_clear() or an
    // eviction could remove the entry between the two calls, turning a
    // successful load into XPE_ERR_PROCESSING_FAILED. put_and_get takes
    // ownership of entry.data and nulls it, exactly as put() did.
    if (!g_calibCache.put_and_get(path, &entry, out, meta)) {
        return XPE_ERR_PROCESSING_FAILED;
    }
    return XPE_OK;
}

/* -------------------------------------------------------------------------
 * Installing a cached map into the module-global calibration store (QA-A-193, #216).
 *
 * A miss gets there through the plain loader. A hit has no file read to lean on, so it puts the
 * cached pixels and metadata where the loader would have put them -- the same fields, written under
 * g_calib_mutex, and for gain the same "scalar map and polynomial are alternatives" reset.
 * What a hit re-checks instead of the file read (QA-A-196): expiry from the entry, the file's size and
 * write time against the entry's. Not repeated on a hit: the SHA-256 and session checks.
 * ----------------------------------------------------------------------- */
void copy_session(char* dst64, const char* src64) noexcept
{
    std::memset(dst64, 0, 64);
    std::memcpy(dst64, src64, 63);
}

void install_offset(std::unique_ptr<float[]> map, const XpeImageBuffer& d,
                    int64_t timestamp, const char* sessionId64)
{
    std::lock_guard<std::mutex> lock(g_calib_mutex);
    g_calib.offset_map       = std::move(map);
    g_calib.offset_width     = d.width;
    g_calib.offset_height    = d.height;
    g_calib.offset_timestamp = timestamp;
    copy_session(g_calib.offset_session_id, sessionId64);
}

void install_gain(std::unique_ptr<float[]> map, const XpeImageBuffer& d,
                  int64_t timestamp, const char* sessionId64)
{
    std::lock_guard<std::mutex> lock(g_calib_mutex);
    g_calib.gain_map = std::move(map);
    g_calib.gain_poly_coeffs.reset();
    g_calib.gain_poly_num_coeffs = 0;
    g_calib.gain_poly_has_range  = false;
    g_calib.gain_poly_dose_min   = 0.0;
    g_calib.gain_poly_dose_max   = 0.0;
    g_calib.gain_width     = d.width;
    g_calib.gain_height    = d.height;
    g_calib.gain_timestamp = timestamp;
    copy_session(g_calib.gain_session_id, sessionId64);
}

void install_defect(std::unique_ptr<uint8_t[]> map, const XpeImageBuffer& d)
{
    std::lock_guard<std::mutex> lock(g_calib_mutex);
    g_calib.defect_map    = std::move(map);
    g_calib.defect_width  = d.width;
    g_calib.defect_height = d.height;
}

} // anonymous namespace

/* =========================================================================
 * Public cached load API
 * ========================================================================= */

// @MX:ANCHOR: [AUTO] xpe_calib_load_offset_cached — cached offset map loader
// @MX:REASON: Pipeline calls this per-frame; caching eliminates repeated file I/O
// @MX:SPEC: REQ-P1A-014
XPE_API XpeErrorCode xpe_calib_load_offset_cached(const char* filePath,
                                                    XpeImageBuffer* offsetMapOut)
try
{
    if (!filePath || !offsetMapOut) return XPE_ERR_INVALID_INPUT;

    // Taken BEFORE the lookup and before any file read: if the file changes while it is read, the
    // entry keeps the older stamp and the next call sees the difference and reloads (QA-A-196).
    const FileStamp stamp = stamp_of(filePath);

    // Cache hit: return the cached view AND leave the global store holding that map (QA-A-193),
    // with the verdict a miss would give (QA-A-196).
    {
        XpeImageBuffer view{};
        std::unique_ptr<float[]> pixels;
        EntryMeta meta;
        HitState state = HitState::Miss;
        const XpeErrorCode crc = g_calibCache.get_copy<float>(std::string(filePath), MapKind::Offset, stamp, now_epoch_ms(),
                                                           &view, &pixels, &meta, &state);
        if (crc != XPE_OK) return crc;
        if (state == HitState::Expired) return XPE_ERR_CALIBRATION_EXPIRED;
        if (state == HitState::Hit) {
            if (!can_open_for_read(filePath)) return XPE_ERR_IO_FAILED;
            install_offset(std::move(pixels), view, meta.timestamp, meta.sessionId);
            std::memcpy(offsetMapOut, &view, sizeof(XpeImageBuffer));
            return XPE_OK;
        }
    }

    // Cache miss: load from file via 1-arg API (populates g_calib)
    XpeErrorCode rc = xpe_calib_load_offset(filePath);
    if (rc != XPE_OK) return rc;

    // Snapshot the freshly loaded map, then publish it to the cache and hand
    // back the cache's view of it (#127).
    XpeImageBuffer desc{};
    std::vector<uint8_t> staging;
    EntryMeta meta;
    meta.kind = MapKind::Offset;
    meta.stamp = stamp;
    {
        std::lock_guard<std::mutex> lock(g_calib_mutex);
        if (!g_calib.offset_map || g_calib.offset_width == 0) return XPE_ERR_NOT_INITIALIZED;

        const size_t pixelCount = static_cast<size_t>(g_calib.offset_width) * g_calib.offset_height;
        desc.width         = g_calib.offset_width;
        desc.height        = g_calib.offset_height;
        desc.bitsAllocated = 32u;
        desc.bitsStored    = 32u;
        desc.format        = XPE_PIXEL_FLOAT32;
        desc.dataSize      = pixelCount * sizeof(float);

        staging.resize(static_cast<size_t>(desc.dataSize));
        std::memcpy(staging.data(), g_calib.offset_map.get(), staging.size());
        meta.timestamp = g_calib.offset_timestamp;
        meta.expiryMs  = g_calib.offset_expiry_ms;
        std::memcpy(meta.sessionId, g_calib.offset_session_id, sizeof(meta.sessionId));
    }

    return publish_and_view(std::string(filePath), desc, staging.data(), offsetMapOut, meta);
}
catch (const std::bad_alloc&)
{
    // No exception may leave a C ABI function (QA-A-200): the strings, vectors and the cache's own
    // containers all allocate.
    return XPE_ERR_OUT_OF_MEMORY;
}
catch (...)
{
    return XPE_ERR_PROCESSING_FAILED;
}

// @MX:ANCHOR: [AUTO] xpe_calib_load_gain_cached — cached gain map loader
// @MX:REASON: Pipeline calls this per-frame; caching eliminates repeated file I/O
// @MX:SPEC: REQ-P1A-016
XPE_API XpeErrorCode xpe_calib_load_gain_cached(const char* filePath,
                                                  XpeImageBuffer* gainMapOut)
try
{
    if (!filePath || !gainMapOut) return XPE_ERR_INVALID_INPUT;

    const FileStamp stamp = stamp_of(filePath);

    {
        XpeImageBuffer view{};
        std::unique_ptr<float[]> pixels;
        EntryMeta meta;
        HitState state = HitState::Miss;
        const XpeErrorCode crc = g_calibCache.get_copy<float>(std::string(filePath), MapKind::Gain, stamp, now_epoch_ms(),
                                                           &view, &pixels, &meta, &state);
        if (crc != XPE_OK) return crc;
        if (state == HitState::Expired) return XPE_ERR_CALIBRATION_EXPIRED;
        if (state == HitState::Hit) {
            if (!can_open_for_read(filePath)) return XPE_ERR_IO_FAILED;
            install_gain(std::move(pixels), view, meta.timestamp, meta.sessionId);
            // The quality metadata a load makes current: made current again, the same way (nothrow).
            if (meta.hasQuality) xpe_calib_commit_quality_meta(meta.quality);
            std::memcpy(gainMapOut, &view, sizeof(XpeImageBuffer));
            return XPE_OK;
        }
    }

    // Cache miss: load from file via 1-arg API (populates g_calib)
    XpeErrorCode rc = xpe_calib_load_gain(filePath);
    if (rc != XPE_OK) return rc;

    // Snapshot the freshly loaded map, then publish it to the cache and hand
    // back the cache's view of it (#127).
    XpeImageBuffer desc{};
    std::vector<uint8_t> staging;
    EntryMeta meta;
    meta.kind = MapKind::Gain;
    meta.stamp = stamp;
    {
        std::lock_guard<std::mutex> lock(g_calib_mutex);
        // A gain POLYNOMIAL file loaded: the store holds it and xpe_gain_correct() uses it, but there
        // is no scalar map to hand back and nothing is cached (QA-A-196). The load succeeded, so the
        // call reports success with an empty buffer rather than an error.
        if (!g_calib.gain_map && g_calib.gain_poly_coeffs && g_calib.gain_poly_num_coeffs != 0) {
            std::memset(gainMapOut, 0, sizeof(XpeImageBuffer));
            return XPE_OK;
        }
        if (!g_calib.gain_map || g_calib.gain_width == 0) return XPE_ERR_NOT_INITIALIZED;

        const size_t pixelCount = static_cast<size_t>(g_calib.gain_width) * g_calib.gain_height;
        desc.width         = g_calib.gain_width;
        desc.height        = g_calib.gain_height;
        desc.bitsAllocated = 32u;
        desc.bitsStored    = 32u;
        desc.format        = XPE_PIXEL_FLOAT32;
        desc.dataSize      = pixelCount * sizeof(float);

        staging.resize(static_cast<size_t>(desc.dataSize));
        std::memcpy(staging.data(), g_calib.gain_map.get(), staging.size());
        meta.timestamp  = g_calib.gain_timestamp;
        meta.expiryMs   = g_calib.gain_expiry_ms;
        meta.quality    = g_calib.gain_quality;
        meta.hasQuality = g_calib.gain_has_quality;
        std::memcpy(meta.sessionId, g_calib.gain_session_id, sizeof(meta.sessionId));
    }

    return publish_and_view(std::string(filePath), desc, staging.data(), gainMapOut, meta);
}
catch (const std::bad_alloc&)
{
    // No exception may leave a C ABI function (QA-A-200): the strings, vectors and the cache's own
    // containers all allocate.
    return XPE_ERR_OUT_OF_MEMORY;
}
catch (...)
{
    return XPE_ERR_PROCESSING_FAILED;
}

// @MX:ANCHOR: [AUTO] xpe_calib_load_defect_cached — cached defect map loader
// @MX:REASON: Pipeline calls this per-frame; caching eliminates repeated file I/O
// @MX:SPEC: REQ-P1A-016
// QA-A-149 (#211): this cited old 024, which was NOT merely a stale number --
// old 024 was defect *correction* (replace bad pixels by interpolation), while
// this function *loads* the defect map. The loader's requirement was old 037,
// now REQ-P1A-016. Corrected to what the function does, not by renumbering.
// (Old numbers are written bare here on purpose: spelling them with the
// REQ-P1A- prefix would re-register them as live citations in
// tools/docs/check_req_citations.py.)
XPE_API XpeErrorCode xpe_calib_load_defect_cached(const char* filePath,
                                                    XpeImageBuffer* defectMapOut)
try
{
    if (!filePath || !defectMapOut) return XPE_ERR_INVALID_INPUT;

    const FileStamp stamp = stamp_of(filePath);

    {
        XpeImageBuffer view{};
        std::unique_ptr<uint8_t[]> pixels;
        EntryMeta meta;
        HitState state = HitState::Miss;
        const XpeErrorCode crc = g_calibCache.get_copy<uint8_t>(std::string(filePath), MapKind::Defect, stamp, now_epoch_ms(),
                                                           &view, &pixels, &meta, &state);
        if (crc != XPE_OK) return crc;
        if (state == HitState::Expired) return XPE_ERR_CALIBRATION_EXPIRED;
        if (state == HitState::Hit) {
            if (!can_open_for_read(filePath)) return XPE_ERR_IO_FAILED;
            install_defect(std::move(pixels), view);
            std::memcpy(defectMapOut, &view, sizeof(XpeImageBuffer));
            return XPE_OK;
        }
    }

    // Cache miss: load from file via 1-arg API (populates g_calib)
    XpeErrorCode rc = xpe_calib_load_defect_map(filePath);
    if (rc != XPE_OK) return rc;

    // Snapshot the freshly loaded map, then publish it to the cache and hand
    // back the cache's view of it (#127).
    XpeImageBuffer desc{};
    std::vector<uint8_t> staging;
    EntryMeta meta;
    meta.kind = MapKind::Defect;
    meta.stamp = stamp;
    {
        std::lock_guard<std::mutex> lock(g_calib_mutex);
        if (!g_calib.defect_map || g_calib.defect_width == 0) return XPE_ERR_NOT_INITIALIZED;

        const size_t pixelCount = static_cast<size_t>(g_calib.defect_width) * g_calib.defect_height;
        desc.width         = g_calib.defect_width;
        desc.height        = g_calib.defect_height;
        desc.bitsAllocated = 8u;
        desc.bitsStored    = 8u;
        desc.format        = XPE_PIXEL_UINT8;
        desc.dataSize      = pixelCount * sizeof(uint8_t);

        staging.resize(static_cast<size_t>(desc.dataSize));
        std::memcpy(staging.data(), g_calib.defect_map.get(), staging.size());
        meta.expiryMs = g_calib.defect_expiry_ms;
    }

    return publish_and_view(std::string(filePath), desc, staging.data(), defectMapOut, meta);
}
catch (const std::bad_alloc&)
{
    // No exception may leave a C ABI function (QA-A-200): the strings, vectors and the cache's own
    // containers all allocate.
    return XPE_ERR_OUT_OF_MEMORY;
}
catch (...)
{
    return XPE_ERR_PROCESSING_FAILED;
}

XPE_API void xpe_calib_cache_clear(void)
{
    g_calibCache.clear();
}

XPE_API void xpe_calib_cache_set_max_size(uint32_t maxMaps)
{
    g_calibCache.setMaxSize(maxMaps);
}
