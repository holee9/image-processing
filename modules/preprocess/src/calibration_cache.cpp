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
 *  - the file is opened for reading once: a file whose attributes are visible but whose content
 *    cannot be opened is refused with IO_FAILED, as a miss would;
 *  - the file's 152-byte header is read again at that open and compared with the header the entry was
 *    published with (QA-A-229b, Codex #93): everything a verdict takes from the header -- type, format,
 *    dimensions, creation time, expiry, session id -- is judged from the CURRENT file, because the header
 *    is outside the SHA-256 (config || payload), so a header edit that keeps size and write time was
 *    invisible to the stamp. A header that differs cancels the hit like a changed stamp does;
 *  - the gain quality metadata of the file, kept parsed in the entry, is made current again.
 * A hit does NOT re-hash the file: an edit of the payload (or config) that keeps the size, the last-write
 * time AND the header is not noticed until xpe_calib_cache_clear() (or module shutdown, which empties the
 * cache). A file REWRITTEN with new content carries a new SHA-256 in its header, so the header comparison
 * above does notice it (QA-A-229b).
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
#include "xpe/preprocess/xcal_format.h"

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
 * Owns the pixel data buffer (allocated with operator new, freed on eviction).
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
 * The cache's pixel buffers come from operator new, not malloc: an allocation failure is then one
 * exception like every other in this file (the loaders' try blocks turn it into OUT_OF_MEMORY), and the
 * allocation-failure sweep can both fail it and count it (QA-A-203, Codex #21).
 */
void* cache_buffer_alloc(size_t bytes) { return ::operator new(bytes); }
void  cache_buffer_free(void* p) noexcept { ::operator delete(p); }

struct CacheBufferFree {
    void operator()(void* p) const noexcept { cache_buffer_free(p); }
};
/** Owns a cache pixel buffer until the cache has taken it. */
using CacheBufferPtr = std::unique_ptr<void, CacheBufferFree>;

/**
 * Whether the file can be opened for reading, the way the XCal reader opens it, and its header bytes.
 * Reading the size and the write time succeeding does not imply the open (a file can show its attributes and
 * refuse its content), and a miss would report IO_FAILED, so a hit has to try the open too (QA-A-200).
 * The header is read at the same open (QA-A-229b): `header.ok` is false when the file is shorter than a header.
 */
struct HeaderSnap {
    bool          ok{false};
    unsigned char bytes[sizeof(XCalFileHeader)]{};
};
struct OpenCheck {
    bool       openable{false};
    HeaderSnap header;
};

OpenCheck open_check(const char* path) noexcept
{
    OpenCheck oc;
    try {
        std::ifstream f(path, std::ios::binary);
        oc.openable = f.is_open();
        if (oc.openable) {
            f.read(reinterpret_cast<char*>(oc.header.bytes), static_cast<std::streamsize>(sizeof(oc.header.bytes)));
            oc.header.ok = (f.gcount() == static_cast<std::streamsize>(sizeof(oc.header.bytes)));
        }
    } catch (...) {
        oc.openable = false;
        oc.header.ok = false;
    }
    return oc;
}

int64_t now_epoch_ms() noexcept
{
    return xpe_calib_now_ms();   // the system clock; a test clock in the clock-test build (QA-A-244)
}

/**
 * What the plain loader and the file record next to the pixels. Kept in the entry so that a hit
 * can put the global store back exactly as the loader did and judge the file as the read would.
 * timestamp is zero for a defect map (QA-A-229 M4: its sessionId is kept, the consistency check needs it);
 * the quality metadata is kept for gain only.
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
    HeaderSnap  header;        ///< the file's header, read before the file was loaded (QA-A-229b); a hit re-reads and compares
    XpeCalibQualityMeta quality{};   ///< gain only: the parsed FUNC-033 metadata of the file
    bool        hasQuality{false};
    std::shared_ptr<uint32_t[]> gainDefects;   ///< gain only: the pixels classified defective at load (QA-A-211), shared and immutable
    uint32_t    gainDefectCount{0};
    bool        defectOverLimit{false};   ///< defect only: the map was above the density tolerance when it was loaded (QA-A-241e)
    uint64_t    defectMarked{0};          ///< defect only: pixels marked defective
    uint64_t    defectTotal{0};           ///< defect only: pixels in the map
    uint64_t    defectHash{0};            ///< defect only: identity of the mask when over the limit (QA-A-241f)
};

/// NeedOpenCheck / Unreadable are the two steps of the open check (QA-A-203): see get_copy().
enum class HitState { Miss, Hit, Expired, NeedOpenCheck, Unreadable };

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
     *
     * QA-A-203 (Codex #21): the plain reader opens the file BEFORE it looks at the expiry, so a file that
     * is expired and cannot be opened is IO_FAILED, and a hit must say the same. The open is file I/O and
     * is not done under the cache lock, so the lookup is two calls: with `openable == nullptr` an entry
     * that survives the stamp and kind checks answers NeedOpenCheck (nothing erased, nothing copied);
     * the caller opens the file and calls again with the answer. An unopenable file is Unreadable, and the
     * entry is left alone. Only then is the expiry judged, and an expired entry dropped. Every check is
     * made again on the second call, so an entry replaced in between is judged as it now stands.
     */
    template <typename T>
    XpeErrorCode get_copy(const std::string& path, MapKind kind, const FileStamp& now,
                          const OpenCheck* check, XpeImageBuffer* view, std::unique_ptr<T[]>* pixels,
                          EntryMeta* meta, HitState* state) {
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
        if (!check) { *state = HitState::NeedOpenCheck; return XPE_OK; }
        if (!check->openable) { *state = HitState::Unreadable; return XPE_OK; }
        // QA-A-229b (Codex #93): the header is outside the SHA-256, so a file whose header was edited without
        // changing its size or write time still matches the stamp. The header the entry was published with must
        // equal the header read just now; if it does not (or either could not be read) the entry is dropped and
        // the call is a miss -- the plain loader then reads and judges the file as it stands.
        if (!check->header.ok || !it->second->meta.header.ok ||
            std::memcmp(check->header.bytes, it->second->meta.header.bytes, sizeof(check->header.bytes)) != 0) {
            erase_locked(it);
            return XPE_OK;
        }
        // The clock is read HERE -- after the open check, at the moment of judging -- as the plain reader reads
        // it after it has opened and read the file (QA-A-203b, Codex #22). A time taken before the open would
        // let an entry that expired while a slow open was in progress through.
        if (it->second->meta.expiryMs != 0 && now_epoch_ms() > it->second->meta.expiryMs) {
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
        cache_buffer_free(it->second->buffer.data);
        lru_.erase(it->second);
        index_.erase(it);
    }

    /**
     * put(), with the caller already holding mutex_.
     *
     * QA-A-203 (Codex #21): everything that can throw -- the list node, its path string, the index slot --
     * is done FIRST, while nothing has been changed and the caller still owns the buffer. What follows
     * (dropping the replaced entry, evicting, taking the buffer, linking the node) cannot throw, so an
     * allocation failure leaves the cache exactly as it was: no entry without its index slot, no index
     * slot without its entry, and the caller's buffer still the caller's to free.
     */
    void put_locked(const std::string& path, XpeImageBuffer* buffer, const EntryMeta& meta) {
        std::list<CachedMap> staged;
        staged.emplace_back();
        staged.front().path = path;
        staged.front().meta = meta;

        auto slot = index_.find(path);
        const bool replacing = (slot != index_.end());
        if (!replacing) {
            // May throw; the slot points nowhere until the node is linked below.
            slot = index_.emplace(path, lru_.end()).first;
        }

        // From here nothing throws.
        if (replacing) {
            cache_buffer_free(slot->second->buffer.data);
            lru_.erase(slot->second);
        }
        while (lru_.size() >= maxSize_ && !lru_.empty()) {
            CachedMap& oldest = lru_.back();
            cache_buffer_free(oldest.buffer.data);
            index_.erase(oldest.path);
            lru_.pop_back();
        }

        std::memcpy(&staged.front().buffer, buffer, sizeof(XpeImageBuffer));
        // The cache owns the pixels now; clear the caller's pointer to prevent a double free.
        buffer->data = nullptr;
        buffer->dataSize = 0;

        lru_.splice(lru_.begin(), staged, staged.begin());
        slot->second = lru_.begin();
    }

public:
    /**
     * @brief Remove all entries from the cache, freeing all buffers.
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& entry : lru_) {
            cache_buffer_free(entry.buffer.data);
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
            cache_buffer_free(oldest.buffer.data);
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

    /** Every list node has its index slot, pointing back at it, and the counts agree. */
    bool consistent() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (lru_.size() != index_.size()) return false;
        for (auto n = lru_.begin(); n != lru_.end(); ++n) {
            const auto slot = index_.find(n->path);
            if (slot == index_.end() || slot->second != n) return false;
        }
        return true;
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
 * The cached loaders' hit lookup, in the order the plain reader judges a file: the stamp and kind, then
 * whether the file can be opened, then the expiry (QA-A-203, Codex #21). See get_copy().
 */
template <typename T>
XpeErrorCode lookup_hit(const char* filePath, MapKind kind, const FileStamp& stamp, XpeImageBuffer* view,
                        std::unique_ptr<T[]>* pixels, EntryMeta* meta, HitState* state)
{
    const std::string key(filePath);
    XpeErrorCode rc = g_calibCache.get_copy<T>(key, kind, stamp, nullptr, view, pixels, meta, state);
    if (rc != XPE_OK || *state != HitState::NeedOpenCheck) return rc;
    const OpenCheck oc = open_check(filePath);   // file I/O, outside the cache lock
#ifdef XPE_CACHE_TEST_HOOKS
    if (xpe_cache_after_open_check_hook) xpe_cache_after_open_check_hook();
#endif
    return g_calibCache.get_copy<T>(key, kind, stamp, &oc, view, pixels, meta, state);
}

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
    // QA-A-203 (Codex #21): the buffer has an owner from the moment it exists. Whatever throws between
    // here and the cache taking it (a string, the list node, the index slot) frees it; it used to leak
    // 37.7 MB at 3072x3072 per failed insert. An allocation failure here throws bad_alloc like every other
    // in the calling loader's try block.
    CacheBufferPtr owner(cache_buffer_alloc(static_cast<size_t>(entry.dataSize)));
    entry.data = owner.get();
    std::memcpy(entry.data, src, static_cast<size_t>(entry.dataSize));

    // Insert and read back under ONE lock (QA-A-31, #127). Splitting these into
    // put() + get() left a window in which another thread's cache_clear() or an
    // eviction could remove the entry between the two calls, turning a
    // successful load into XPE_ERR_PROCESSING_FAILED. put_and_get takes
    // ownership of entry.data and nulls it, exactly as put() did.
    const bool ok = g_calibCache.put_and_get(path, &entry, out, meta);
    if (entry.data == nullptr) { auto* const taken = owner.release(); (void)taken; }   // the cache took the buffer
    if (!ok) return XPE_ERR_PROCESSING_FAILED;
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

// QA-A-229 M4 (#245): a hit must give the verdict a miss would, and a miss runs the plain loader's session
// check against the maps loaded NOW -- so a hit runs the same check (under the same lock, before anything is
// installed). A refused hit leaves the store as it was and returns XPE_ERR_CONFIG_INVALID.
// QA-A-241c (Codex #158): a hit installs the entry's EXPIRY and the never-expires state with the map, under the same lock. Before, a hit
// replaced the map and left the previous file's expiry in the store, so a map that was about to expire could be corrected with by
// an expiry of 0 ("never") until the next plain load -- and the other way round.
XpeErrorCode install_offset(std::unique_ptr<float[]> map, const XpeImageBuffer& d, const EntryMeta& meta)
{
    bool warnSession = false;
    {
        std::lock_guard<std::mutex> lock(g_calib_mutex);
        const XpeErrorCode src = xpe_calib_session_check_locked(CalibMapKind::Offset, meta.sessionId, &warnSession);
        if (src != XPE_OK) return src;
        g_calib.offset_map       = std::move(map);
        g_calib.offset_width     = d.width;
        g_calib.offset_height    = d.height;
        g_calib.offset_timestamp = meta.timestamp;
        g_calib.offset_expiry_ms = meta.expiryMs;
        xpe_calib_note_expiry_locked(CalibMapKind::Offset, meta.expiryMs);
        copy_session(g_calib.offset_session_id, meta.sessionId);
    }
    xpe_calib_session_warn(warnSession);
    return XPE_OK;
}

XpeErrorCode install_gain(std::unique_ptr<float[]> map, const XpeImageBuffer& d, const EntryMeta& meta)
{
    // The store holds the map as a shared_ptr; the control block is allocated here, before the lock, so a
    // failure to allocate it leaves the store untouched.
    std::shared_ptr<float[]> shared(std::move(map));
    bool warnSession = false;
    {
    const XpeCalibQualityMeta* const quality = meta.hasQuality ? &meta.quality : nullptr;
    std::lock_guard<std::mutex> lock(g_calib_mutex);
    const XpeErrorCode src = xpe_calib_session_check_locked(CalibMapKind::Gain, meta.sessionId, &warnSession);
    if (src != XPE_OK) return src;
    g_calib.gain_map = std::move(shared);
    g_calib.gain_defect_idx   = meta.gainDefects;   // a hit installs the classification the load made (QA-A-211)
    g_calib.gain_defect_count = meta.gainDefectCount;
    g_calib.gain_poly_coeffs.reset();
    g_calib.gain_poly_num_coeffs = 0;
    g_calib.gain_poly_has_range  = false;
    g_calib.gain_poly_dose_min   = 0.0;
    g_calib.gain_poly_dose_max   = 0.0;
    g_calib.gain_width     = d.width;
    g_calib.gain_height    = d.height;
    g_calib.gain_timestamp = meta.timestamp;
    g_calib.gain_expiry_ms = meta.expiryMs;
    xpe_calib_note_expiry_locked(CalibMapKind::Gain, meta.expiryMs);
    copy_session(g_calib.gain_session_id, meta.sessionId);
    // The quality of THIS file, beside its map, in the same critical section (nothrow): the file-quality copy the
    // cache publishes with the map, and the record the module serves. A hit on a file with no quality metadata
    // overwrites both with "none" -- it must not leave the previous file's values behind (QA-A-202e, Codex #38 A1/A2).
    g_calib.gain_quality     = quality ? *quality : XpeCalibQualityMeta{};
    g_calib.gain_has_quality = (quality != nullptr);
    if (quality) {
        xpe_calib_commit_quality_meta_locked(*quality);
    } else {
        xpe_calib_commit_no_quality_locked();
    }
    }
    xpe_calib_session_warn(warnSession);
    return XPE_OK;
}

XpeErrorCode install_defect(std::unique_ptr<uint8_t[]> map, const XpeImageBuffer& d, const EntryMeta& meta)
{
    bool warnSession = false;
    bool warnDensity = false;
    {
        std::lock_guard<std::mutex> lock(g_calib_mutex);
        const XpeErrorCode src = xpe_calib_session_check_locked(CalibMapKind::Defect, meta.sessionId, &warnSession);
        if (src != XPE_OK) return src;
        // QA-A-241e (Codex #160): a hit installs a map without loading it. The load raised the over-limit warning once; a hit raises it
        // again only when it turns the installed state from within tolerance to over it, so A (hit) -> A (hit) stays quiet while
        // A -> normal map B -> A (hit) reports that the over-limit map is the one in use again.
        // (QA-A-241f, Codex #161) "a different over-limit map" counts as a change too: the identity is compared, not only the flag.
        warnDensity = meta.defectOverLimit && (!g_calib.defect_over_limit || g_calib.defect_hash != meta.defectHash);
        g_calib.defect_map    = std::move(map);
        g_calib.defect_width  = d.width;
        g_calib.defect_height = d.height;
        g_calib.defect_expiry_ms = meta.expiryMs;
        xpe_calib_note_expiry_locked(CalibMapKind::Defect, meta.expiryMs);
        copy_session(g_calib.defect_session_id, meta.sessionId);
        g_calib.defect_over_limit = meta.defectOverLimit;
        g_calib.defect_marked     = meta.defectMarked;
        g_calib.defect_total      = meta.defectTotal;
        g_calib.defect_hash       = meta.defectHash;
    }
    xpe_calib_session_warn(warnSession);
    if (warnDensity) xpe_calib_push_defect_over_limit(meta.defectMarked, meta.defectTotal);
    return XPE_OK;
}

} // anonymous namespace

bool xpe_calib_cache_is_consistent() { return g_calibCache.consistent(); }

#ifdef XPE_CACHE_TEST_HOOKS
void (*xpe_cache_after_open_check_hook)() = nullptr;
#endif

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
        const XpeErrorCode crc = lookup_hit<float>(filePath, MapKind::Offset, stamp, &view, &pixels, &meta, &state);
        if (crc != XPE_OK) return crc;
        if (state == HitState::Unreadable) return XPE_ERR_IO_FAILED;
        if (state == HitState::Expired) return XPE_ERR_CALIBRATION_EXPIRED;
        if (state == HitState::Hit) {
            const XpeErrorCode irc = install_offset(std::move(pixels), view, meta);
            if (irc != XPE_OK) return irc;
            std::memcpy(offsetMapOut, &view, sizeof(XpeImageBuffer));
            return XPE_OK;
        }
    }

    // Cache miss: load from file via 1-arg API (populates g_calib)
    // The header is read BEFORE the load, like the stamp: if the file changes meanwhile, the entry keeps the
    // older header and the next hit sees the difference (QA-A-229b).
    const HeaderSnap hdrBefore = open_check(filePath).header;
    XpeErrorCode rc = xpe_calib_load_offset(filePath);
    if (rc != XPE_OK) return rc;

    // Snapshot the freshly loaded map, then publish it to the cache and hand
    // back the cache's view of it (#127).
    XpeImageBuffer desc{};
    std::vector<uint8_t> staging;
    EntryMeta meta;
    meta.kind = MapKind::Offset;
    meta.stamp = stamp;
    meta.header = hdrBefore;
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
        const XpeErrorCode crc = lookup_hit<float>(filePath, MapKind::Gain, stamp, &view, &pixels, &meta, &state);
        if (crc != XPE_OK) return crc;
        if (state == HitState::Unreadable) return XPE_ERR_IO_FAILED;
        if (state == HitState::Expired) return XPE_ERR_CALIBRATION_EXPIRED;
        if (state == HitState::Hit) {
            const XpeErrorCode irc = install_gain(std::move(pixels), view, meta);
            if (irc != XPE_OK) return irc;
            std::memcpy(gainMapOut, &view, sizeof(XpeImageBuffer));
            return XPE_OK;
        }
    }

    // Cache miss: load from file via 1-arg API (populates g_calib)
    // The header is read BEFORE the load, like the stamp: if the file changes meanwhile, the entry keeps the
    // older header and the next hit sees the difference (QA-A-229b).
    const HeaderSnap hdrBefore = open_check(filePath).header;
    XpeErrorCode rc = xpe_calib_load_gain(filePath);
    if (rc != XPE_OK) return rc;

    // Snapshot the freshly loaded map, then publish it to the cache and hand
    // back the cache's view of it (#127).
    XpeImageBuffer desc{};
    std::vector<uint8_t> staging;
    EntryMeta meta;
    meta.kind = MapKind::Gain;
    meta.stamp = stamp;
    meta.header = hdrBefore;
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
        meta.gainDefects     = g_calib.gain_defect_idx;
        meta.gainDefectCount = g_calib.gain_defect_count;
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
        const XpeErrorCode crc = lookup_hit<uint8_t>(filePath, MapKind::Defect, stamp, &view, &pixels, &meta, &state);
        if (crc != XPE_OK) return crc;
        if (state == HitState::Unreadable) return XPE_ERR_IO_FAILED;
        if (state == HitState::Expired) return XPE_ERR_CALIBRATION_EXPIRED;
        if (state == HitState::Hit) {
            const XpeErrorCode irc = install_defect(std::move(pixels), view, meta);
            if (irc != XPE_OK) return irc;
            std::memcpy(defectMapOut, &view, sizeof(XpeImageBuffer));
            return XPE_OK;
        }
    }

    // Cache miss: load from file via 1-arg API (populates g_calib)
    // The header is read BEFORE the load, like the stamp: if the file changes meanwhile, the entry keeps the
    // older header and the next hit sees the difference (QA-A-229b).
    const HeaderSnap hdrBefore = open_check(filePath).header;
    XpeErrorCode rc = xpe_calib_load_defect_map(filePath);
    if (rc != XPE_OK) return rc;

    // Snapshot the freshly loaded map, then publish it to the cache and hand
    // back the cache's view of it (#127).
    XpeImageBuffer desc{};
    std::vector<uint8_t> staging;
    EntryMeta meta;
    meta.kind = MapKind::Defect;
    meta.stamp = stamp;
    meta.header = hdrBefore;
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
        std::memcpy(meta.sessionId, g_calib.defect_session_id, sizeof(meta.sessionId));
        meta.defectOverLimit = g_calib.defect_over_limit;
        meta.defectMarked    = g_calib.defect_marked;
        meta.defectTotal     = g_calib.defect_total;
        meta.defectHash      = g_calib.defect_hash;
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
