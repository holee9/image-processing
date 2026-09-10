/**
 * @file calibration_cache.cpp
 * @brief LRU cache for calibration maps to eliminate repeated file I/O.
 *
 * Keeps recently used calibration maps in memory. Cache key is the combination
 * of file path string. Thread-safety: all LRU/index mutations are protected by
 * an internal mutex (IEC 62304 Class B).
 *
 * SPEC: SPEC-XPE-P1A v1.0.0
 * IEC 62304 Class B
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cstdlib>
#include <cstring>
#include <list>
#include <mutex>
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
        put_locked(path, buffer);
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
                     XpeImageBuffer* out) {
        if (!buffer || !buffer->data) return false;

        std::lock_guard<std::mutex> lock(mutex_);
        put_locked(path, buffer);

        auto it = index_.find(path);
        if (it == index_.end()) return false;
        if (out) {
            std::memcpy(out, &it->second->buffer, sizeof(XpeImageBuffer));
        }
        return true;
    }

private:
    /** put(), with the caller already holding mutex_. */
    void put_locked(const std::string& path, XpeImageBuffer* buffer) {
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
                              XpeImageBuffer* out)
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
    if (!g_calibCache.put_and_get(path, &entry, out)) {
        return XPE_ERR_PROCESSING_FAILED;
    }
    return XPE_OK;
}

} // anonymous namespace

/* =========================================================================
 * Public cached load API
 * ========================================================================= */

// @MX:ANCHOR: [AUTO] xpe_calib_load_offset_cached — cached offset map loader
// @MX:REASON: Pipeline calls this per-frame; caching eliminates repeated file I/O
// @MX:SPEC: REQ-P1A-035
XPE_API XpeErrorCode xpe_calib_load_offset_cached(const char* filePath,
                                                    XpeImageBuffer* offsetMapOut)
{
    if (!filePath || !offsetMapOut) return XPE_ERR_INVALID_INPUT;

    // Cache hit: return cached data
    if (g_calibCache.get(std::string(filePath), offsetMapOut)) {
        return XPE_OK;
    }

    // Cache miss: load from file via 1-arg API (populates g_calib)
    XpeErrorCode rc = xpe_calib_load_offset(filePath);
    if (rc != XPE_OK) return rc;

    // Snapshot the freshly loaded map, then publish it to the cache and hand
    // back the cache's view of it (#127).
    XpeImageBuffer desc{};
    std::vector<uint8_t> staging;
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
    }

    return publish_and_view(std::string(filePath), desc, staging.data(), offsetMapOut);
}

// @MX:ANCHOR: [AUTO] xpe_calib_load_gain_cached — cached gain map loader
// @MX:REASON: Pipeline calls this per-frame; caching eliminates repeated file I/O
// @MX:SPEC: REQ-P1A-016
XPE_API XpeErrorCode xpe_calib_load_gain_cached(const char* filePath,
                                                  XpeImageBuffer* gainMapOut)
{
    if (!filePath || !gainMapOut) return XPE_ERR_INVALID_INPUT;

    if (g_calibCache.get(std::string(filePath), gainMapOut)) {
        return XPE_OK;
    }

    // Cache miss: load from file via 1-arg API (populates g_calib)
    XpeErrorCode rc = xpe_calib_load_gain(filePath);
    if (rc != XPE_OK) return rc;

    // Snapshot the freshly loaded map, then publish it to the cache and hand
    // back the cache's view of it (#127).
    XpeImageBuffer desc{};
    std::vector<uint8_t> staging;
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
    }

    return publish_and_view(std::string(filePath), desc, staging.data(), gainMapOut);
}

// @MX:ANCHOR: [AUTO] xpe_calib_load_defect_cached — cached defect map loader
// @MX:REASON: Pipeline calls this per-frame; caching eliminates repeated file I/O
// @MX:SPEC: REQ-P1A-024
XPE_API XpeErrorCode xpe_calib_load_defect_cached(const char* filePath,
                                                    XpeImageBuffer* defectMapOut)
{
    if (!filePath || !defectMapOut) return XPE_ERR_INVALID_INPUT;

    if (g_calibCache.get(std::string(filePath), defectMapOut)) {
        return XPE_OK;
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

    return publish_and_view(std::string(filePath), desc, staging.data(), defectMapOut);
}

XPE_API void xpe_calib_cache_clear(void)
{
    g_calibCache.clear();
}

XPE_API void xpe_calib_cache_set_max_size(uint32_t maxMaps)
{
    g_calibCache.setMaxSize(maxMaps);
}
