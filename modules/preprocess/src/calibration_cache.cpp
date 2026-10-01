/**
 * @file calibration_cache.cpp
 * @brief LRU cache for calibration maps to eliminate repeated file I/O.
 *
 * Keeps recently used calibration maps in memory. The cache key is the file
 * path string exactly as the caller passed it: a file that changes on disk is
 * not noticed until xpe_calib_cache_clear() (or module shutdown, which empties
 * the cache). A hit still leaves the module-global calibration store holding the
 * cached map, as a miss does (QA-A-193, #216). Thread-safety: all LRU/index
 * mutations are protected by an internal mutex (IEC 62304 Class B).
 *
 * SPEC: SPEC-XPE-P1A v1.0.0
 * IEC 62304 Class B
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cstdlib>
#include <cstring>
#include <list>
#include <memory>
#include <mutex>
#include <new>
#include <unordered_map>
#include <string>
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
struct CachedMap {
    XpeImageBuffer buffer;  ///< Image buffer (owns data pointer)
    std::string    path;    ///< File path used as cache key
    // What the plain loader records next to the pixels (offset and gain files only; zero for a
    // defect map). Kept so that a hit can put the global store back exactly as the loader did.
    int64_t        timestamp{0};
    char           sessionId[64]{};
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
     */
    template <typename T>
    XpeErrorCode get_copy(const std::string& path, XpeImageBuffer* view, std::unique_ptr<T[]>* pixels,
                          int64_t* timestamp, char* sessionId64, bool* hit) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = index_.find(path);
        *hit = (it != index_.end());
        if (!*hit) return XPE_OK;

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
        *timestamp = entry.timestamp;
        std::memcpy(sessionId64, entry.sessionId, sizeof(entry.sessionId));
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
        put_locked(path, buffer, 0, nullptr);
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
                     XpeImageBuffer* out, int64_t timestamp, const char* sessionId64) {
        if (!buffer || !buffer->data) return false;

        std::lock_guard<std::mutex> lock(mutex_);
        put_locked(path, buffer, timestamp, sessionId64);

        auto it = index_.find(path);
        if (it == index_.end()) return false;
        if (out) {
            std::memcpy(out, &it->second->buffer, sizeof(XpeImageBuffer));
        }
        return true;
    }

private:
    /** put(), with the caller already holding mutex_. */
    void put_locked(const std::string& path, XpeImageBuffer* buffer, int64_t timestamp,
                    const char* sessionId64) {
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
        entry.timestamp = timestamp;
        if (sessionId64) std::memcpy(entry.sessionId, sessionId64, sizeof(entry.sessionId));
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
                              int64_t timestamp,
                              const char* sessionId64)
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
    if (!g_calibCache.put_and_get(path, &entry, out, timestamp, sessionId64)) {
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
 * Not restored on a hit: the expiry / session / SHA-256 checks of the file read, and the quality
 * metadata the gain loader parses from the file's config JSON (the entry does not carry them).
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
{
    if (!filePath || !offsetMapOut) return XPE_ERR_INVALID_INPUT;

    // Cache hit: return the cached view AND leave the global store holding that map (QA-A-193).
    {
        XpeImageBuffer view{};
        std::unique_ptr<float[]> pixels;
        int64_t timestamp = 0;
        char sessionId[64] = {};
        bool hit = false;
        const XpeErrorCode crc = g_calibCache.get_copy<float>(std::string(filePath), &view, &pixels,
                                                           &timestamp, sessionId, &hit);
        if (crc != XPE_OK) return crc;
        if (hit) {
            install_offset(std::move(pixels), view, timestamp, sessionId);
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
    int64_t timestamp = 0;
    char sessionId[64] = {};
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
        timestamp = g_calib.offset_timestamp;
        std::memcpy(sessionId, g_calib.offset_session_id, sizeof(sessionId));
    }

    return publish_and_view(std::string(filePath), desc, staging.data(), offsetMapOut, timestamp, sessionId);
}

// @MX:ANCHOR: [AUTO] xpe_calib_load_gain_cached — cached gain map loader
// @MX:REASON: Pipeline calls this per-frame; caching eliminates repeated file I/O
// @MX:SPEC: REQ-P1A-016
XPE_API XpeErrorCode xpe_calib_load_gain_cached(const char* filePath,
                                                  XpeImageBuffer* gainMapOut)
{
    if (!filePath || !gainMapOut) return XPE_ERR_INVALID_INPUT;

    {
        XpeImageBuffer view{};
        std::unique_ptr<float[]> pixels;
        int64_t timestamp = 0;
        char sessionId[64] = {};
        bool hit = false;
        const XpeErrorCode crc = g_calibCache.get_copy<float>(std::string(filePath), &view, &pixels,
                                                           &timestamp, sessionId, &hit);
        if (crc != XPE_OK) return crc;
        if (hit) {
            install_gain(std::move(pixels), view, timestamp, sessionId);
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
    int64_t timestamp = 0;
    char sessionId[64] = {};
    {
        std::lock_guard<std::mutex> lock(g_calib_mutex);
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
        timestamp = g_calib.gain_timestamp;
        std::memcpy(sessionId, g_calib.gain_session_id, sizeof(sessionId));
    }

    return publish_and_view(std::string(filePath), desc, staging.data(), gainMapOut, timestamp, sessionId);
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
{
    if (!filePath || !defectMapOut) return XPE_ERR_INVALID_INPUT;

    {
        XpeImageBuffer view{};
        std::unique_ptr<uint8_t[]> pixels;
        int64_t timestamp = 0;
        char sessionId[64] = {};
        bool hit = false;
        const XpeErrorCode crc = g_calibCache.get_copy<uint8_t>(std::string(filePath), &view, &pixels,
                                                           &timestamp, sessionId, &hit);
        if (crc != XPE_OK) return crc;
        if (hit) {
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
    }

    return publish_and_view(std::string(filePath), desc, staging.data(), defectMapOut, 0, nullptr);
}

XPE_API void xpe_calib_cache_clear(void)
{
    g_calibCache.clear();
}

XPE_API void xpe_calib_cache_set_max_size(uint32_t maxMaps)
{
    g_calibCache.setMaxSize(maxMaps);
}
