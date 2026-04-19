/**
 * @file calibration_cache.cpp
 * @brief LRU cache for calibration maps to eliminate repeated file I/O.
 *
 * Keeps recently used calibration maps in memory. Cache key is the combination
 * of file path string. Thread-safety: NOT thread-safe. Caller must synchronize
 * if sharing across threads (IEC 62304 Class B).
 *
 * SPEC: SPEC-XPE-P1A v1.0.0
 * IEC 62304 Class B
 */

#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cstdlib>
#include <cstring>
#include <list>
#include <unordered_map>
#include <string>
#include <utility>

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

    /**
     * @brief Remove all entries from the cache, freeing all buffers.
     */
    void clear() {
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
        maxSize_ = (maxSize > 0) ? maxSize : 1;
        while (lru_.size() > maxSize_ && !lru_.empty()) {
            CachedMap& oldest = lru_.back();
            std::free(oldest.buffer.data);
            index_.erase(oldest.path);
            lru_.pop_back();
        }
    }

    uint32_t getMaxSize() const { return maxSize_; }
    size_t   size()       const { return lru_.size(); }

private:
    std::list<CachedMap>                                lru_;
    std::unordered_map<std::string, ListIter>           index_;
    uint32_t                                            maxSize_{4};
};

// @MX:NOTE: [AUTO] Singleton cache instance — module-scoped, not thread-safe
CalibrationLRUCache g_calibCache;

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

    // Cache miss: load from file (caller must have pre-allocated offsetMapOut->data)
    XpeErrorCode rc = xpe_calib_load_offset(filePath, offsetMapOut);
    if (rc != XPE_OK) return rc;

    // Allocate a separate buffer for the cache and copy data
    // (The caller retains ownership of their original buffer)
    XpeImageBuffer cacheEntry{};
    cacheEntry.width         = offsetMapOut->width;
    cacheEntry.height        = offsetMapOut->height;
    cacheEntry.bitsAllocated = offsetMapOut->bitsAllocated;
    cacheEntry.bitsStored    = offsetMapOut->bitsStored;
    cacheEntry.format        = offsetMapOut->format;
    cacheEntry.dataSize      = offsetMapOut->dataSize;

    cacheEntry.data = std::malloc(cacheEntry.dataSize);
    if (!cacheEntry.data) {
        // Cache insert failed — not an error, caller still has valid data
        return XPE_OK;
    }
    std::memcpy(cacheEntry.data, offsetMapOut->data, cacheEntry.dataSize);

    g_calibCache.put(std::string(filePath), &cacheEntry);
    return XPE_OK;
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

    XpeErrorCode rc = xpe_calib_load_gain(filePath, gainMapOut);
    if (rc != XPE_OK) return rc;

    XpeImageBuffer cacheEntry{};
    cacheEntry.width         = gainMapOut->width;
    cacheEntry.height        = gainMapOut->height;
    cacheEntry.bitsAllocated = gainMapOut->bitsAllocated;
    cacheEntry.bitsStored    = gainMapOut->bitsStored;
    cacheEntry.format        = gainMapOut->format;
    cacheEntry.dataSize      = gainMapOut->dataSize;

    cacheEntry.data = std::malloc(cacheEntry.dataSize);
    if (!cacheEntry.data) return XPE_OK;

    std::memcpy(cacheEntry.data, gainMapOut->data, cacheEntry.dataSize);
    g_calibCache.put(std::string(filePath), &cacheEntry);
    return XPE_OK;
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

    XpeErrorCode rc = xpe_calib_load_defect_map(filePath, defectMapOut);
    if (rc != XPE_OK) return rc;

    XpeImageBuffer cacheEntry{};
    cacheEntry.width         = defectMapOut->width;
    cacheEntry.height        = defectMapOut->height;
    cacheEntry.bitsAllocated = defectMapOut->bitsAllocated;
    cacheEntry.bitsStored    = defectMapOut->bitsStored;
    cacheEntry.format        = defectMapOut->format;
    cacheEntry.dataSize      = defectMapOut->dataSize;

    cacheEntry.data = std::malloc(cacheEntry.dataSize);
    if (!cacheEntry.data) return XPE_OK;

    std::memcpy(cacheEntry.data, defectMapOut->data, cacheEntry.dataSize);
    g_calibCache.put(std::string(filePath), &cacheEntry);
    return XPE_OK;
}

XPE_API void xpe_calib_cache_clear(void)
{
    g_calibCache.clear();
}

XPE_API void xpe_calib_cache_set_max_size(uint32_t maxMaps)
{
    g_calibCache.setMaxSize(maxMaps);
}
