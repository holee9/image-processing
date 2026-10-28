/**
 * @file defect_correct.cpp
 * @brief SWU-1.3: Defect pixel correction and runtime detection (PRE-06)
 *        REQ-P1A-012 (Defect Correction Bilinear + Cluster)
 * SPEC: SPEC-XPE-P1A v1.2.0  IEC 62304 Class B
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "defect_ext.hpp"
#include "parallel.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include <new>

#include <cmath>
#include <cstring>
#include <limits>
#include <vector>
#include <algorithm>
#include <emmintrin.h>
#include <queue>
#include <emmintrin.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace {

// The frame's size and a pixel's position as one value each: two adjacent uint32_t parameters of the same kind are easy to swap
// (bugprone-easily-swappable-parameters), and the search below takes both a size and a position.
struct Grid { uint32_t width; uint32_t height; };
struct PixelXY { uint32_t x; uint32_t y; };


// QA-A-237g (#245, SRS-CALIB-PERF-002): the masked pixels of a frame -- the loaded defect map together with the pixels the gain
// calibration classified defective (a scalar map's list in the snapshot, a polynomial map's list for this frame) -- as one set that
// answers "is this pixel masked" and "which masked pixel is this" without a frame-sized copy. The union used to be built as a plane
// of n bytes (9 MiB at 3072x3072) whenever the gain classified a single pixel; on the real CalData_6 maps (42,026 classified pixels,
// 23,505 in the defect map) that copy made the stage's peak.
//
// A frame has few masked pixels (0.25 % in the real defect map), so the set is a two-level bitmap over 64-pixel blocks: one bit a
// block says whether it holds any masked pixel (18 KB at 3072x3072), and only the non-empty blocks keep their 64 bits, with the
// number of masked pixels before them (12 bytes a non-empty block). Both questions are a few bit operations. The rank of a masked
// pixel (`slot`) is its position in raster order, which is what the cluster search's `visited` marks (QA-A-237e) and the fill-distance
// table (QA-A-213) are indexed by -- they used to be indexed by the pixel (1 bit and 1 byte a pixel; 237f first indexed them by a
// sorted list). Above the density where the bitmap costs more than the planes (12 bytes a masked pixel against the 9 MiB plane) the
// set is a plane of n bytes -- the loaded map itself when nothing is added to it, a union copy otherwise -- and `slot` is the pixel
// index, as before 237f. The pixels in the set, and every value stored and read through it, are the same in both modes.
#if defined(_MSC_VER)
inline int popcount64(uint64_t v) noexcept { return static_cast<int>(__popcnt64(v)); }
inline int countTrailingZeros64(uint64_t v) noexcept { unsigned long i = 0; _BitScanForward64(&i, v); return static_cast<int>(i); }
#else
inline int popcount64(uint64_t v) noexcept { return __builtin_popcountll(v); }
inline int countTrailingZeros64(uint64_t v) noexcept { return __builtin_ctzll(v); }
#endif

// Calls fn(index) for every non-zero byte of `mask`, ascending. Most of a frame is valid: 64 bytes are tested at a time (four
// 16-byte loads ORed) and the masked ones are found byte by byte only in a block that is not all zero.
template <class Fn>
void forEachNonZero(const uint8_t* mask, size_t n, Fn&& fn)
{
    const __m128i zero = _mm_setzero_si128();
    size_t i = 0;
    for (; i + 64 <= n; i += 64) {
        const __m128i* block = reinterpret_cast<const __m128i*>(mask + i);
        const __m128i any = _mm_or_si128(_mm_or_si128(_mm_loadu_si128(block), _mm_loadu_si128(block + 1)),
                                         _mm_or_si128(_mm_loadu_si128(block + 2), _mm_loadu_si128(block + 3)));
        if (_mm_movemask_epi8(_mm_cmpeq_epi8(any, zero)) == 0xFFFF) continue;
        for (size_t k = i; k < i + 64; ++k) {
            if (mask[k] != 0) fn(k);
        }
    }
    for (; i < n; ++i) {
        if (mask[i] != 0) fn(i);
    }
}

class MaskSet {
public:
    // `base`: the loaded map (n bytes, non-zero = masked). `extra`, `extraCount`: further masked pixels, ascending and without
    // duplicates, every index below n; read, not copied. Allocates before anything is written by the caller; may throw std::bad_alloc.
    void build(const uint8_t* base, size_t n, const uint32_t* extra, size_t extraCount)
    {
        n_ = n;
        // QA-A-315 (Refs #278): counted on several threads (a count per range, summed: the total does not depend on the split).
        std::atomic<size_t> baseTotal{0};
        xpe_parallel_chunks(n, size_t{1} << 20, [&](size_t b, size_t e) {
            size_t c = 0;
            forEachNonZero(base + b, e - b, [&](size_t) { ++c; });
            baseTotal.fetch_add(c);
        });
        const size_t baseCount = baseTotal.load();
        size_t extraNew = 0;
        for (size_t k = 0; k < extraCount; ++k) extraNew += (base[extra[k]] == 0);
        count_ = baseCount + extraNew;

        dense_ = (count_ * kBytesPerMasked > n / 4);   // QA-A-296: the plane answers test() and slot() with one load; the bitmap form is kept for sparse maps
        if (dense_) {
            if (extraCount == 0) {
                plane_ = base;
            } else {
                // QA-A-315: copied on several threads into a plane allocated without a zero fill (every byte is written).
                own_.reset(new uint8_t[n]);
                uint8_t* const plane = own_.get();
                xpe_parallel_chunks(n, size_t{1} << 20, [&](size_t b, size_t e) { std::memcpy(plane + b, base + b, e - b); });
                for (size_t k = 0; k < extraCount; ++k) plane[extra[k]] = 1;
                plane_ = plane;
            }
            return;
        }

        const size_t blocks = (n + 63) / 64;
        l1_.assign((blocks + 63) / 64, 0);
        l1Rank_.assign(l1_.size(), 0);
        // No reserve: the non-empty blocks are far fewer than the masked pixels when they cluster (7,442 blocks for 51,543 pixels in the
        // real maps), and memory reserved but not used still counts as committed.
        size_t e = 0;
        size_t seen = 0;
        size_t currentBlock = static_cast<size_t>(-1);
        uint64_t bits = 0;
        const auto flush = [&]() {
            if (currentBlock == static_cast<size_t>(-1)) return;
            blockBits_.push_back(bits);
            l1_[currentBlock >> 6] |= uint64_t{1} << (currentBlock & 63);
        };
        const auto emit = [&](size_t idx) {
            const size_t block = idx >> 6;
            if (block != currentBlock) {
                flush();
                currentBlock = block;
                bits = 0;
                blockBase_.push_back(static_cast<uint32_t>(seen));
            }
            bits |= uint64_t{1} << (idx & 63);
            ++seen;
        };
        // The ascending union of the loaded map's masked pixels and `extra`.
        forEachNonZero(base, n, [&](size_t i) {
            while (e < extraCount && extra[e] < i) emit(extra[e++]);
            if (e < extraCount && extra[e] == i) ++e;   // also in the map
            emit(i);
        });
        while (e < extraCount) emit(extra[e++]);
        flush();
        uint32_t run = 0;
        for (size_t w = 0; w < l1_.size(); ++w) {
            l1Rank_[w] = run;
            run += static_cast<uint32_t>(popcount64(l1_[w]));
        }
    }

    size_t count() const noexcept { return count_; }
    // The plane of the dense form (n bytes, non-zero = masked), nullptr in the sparse form.
    const uint8_t* densePlane() const noexcept { return dense_ ? plane_ : nullptr; }
    // How many entries a table indexed by slot() needs.
    size_t slots() const noexcept { return dense_ ? n_ : count_; }

    bool test(size_t idx) const noexcept
    {
        if (dense_) return plane_[idx] != 0;
        const size_t block = idx >> 6;
        const uint64_t word = l1_[block >> 6];
        const uint64_t bit = uint64_t{1} << (block & 63);
        if ((word & bit) == 0) return false;
        const size_t r = l1Rank_[block >> 6] + static_cast<size_t>(popcount64(word & (bit - 1)));
        return ((blockBits_[r] >> (idx & 63)) & 1u) != 0;
    }

    // The position of a MASKED pixel in raster order among the masked pixels (the pixel index in the dense form).
    size_t slot(size_t idx) const noexcept
    {
        if (dense_) return idx;
        const size_t block = idx >> 6;
        const uint64_t word = l1_[block >> 6];
        const uint64_t bit = uint64_t{1} << (block & 63);
        const size_t r = l1Rank_[block >> 6] + static_cast<size_t>(popcount64(word & (bit - 1)));
        return blockBase_[r] + static_cast<size_t>(popcount64(blockBits_[r] & ((uint64_t{1} << (idx & 63)) - 1)));
    }

    // fn(pixel index) for every masked pixel, ascending (raster order).
    template <class Fn>
    void forEach(Fn&& fn) const
    {
        if (dense_) {
            forEachNonZero(plane_, n_, fn);
            return;
        }
        size_t r = 0;
        for (size_t w = 0; w < l1_.size(); ++w) {
            uint64_t blocksInWord = l1_[w];
            while (blocksInWord != 0) {
                const size_t block = (w << 6) + static_cast<size_t>(countTrailingZeros64(blocksInWord));
                blocksInWord &= blocksInWord - 1;
                uint64_t bits = blockBits_[r++];
                while (bits != 0) {
                    fn((block << 6) + static_cast<size_t>(countTrailingZeros64(bits)));
                    bits &= bits - 1;
                }
            }
        }
    }

private:
    static constexpr size_t kBytesPerMasked = 12;   // a non-empty block of one masked pixel: 8 (bits) + 4 (rank)
    bool dense_ = true;
    size_t n_ = 0;
    size_t count_ = 0;
    const uint8_t* plane_ = nullptr;   // dense form
    std::unique_ptr<uint8_t[]> own_;   // dense form with additions: the union plane
    std::vector<uint64_t> l1_;         // sparse form: bit b of word w: block 64 * w + b holds a masked pixel
    std::vector<uint32_t> l1Rank_;     // non-empty blocks before word w
    std::vector<uint64_t> blockBits_;  // per non-empty block: the masked pixels, bit i = pixel 64 * block + i
    std::vector<uint32_t> blockBase_;  // per non-empty block: masked pixels before it
};

// QA-A-315 (Refs #278): the masked pixels in raster order, in a buffer written without a zero fill.
struct MaskedList {
    std::unique_ptr<uint32_t[]> p;
    size_t count = 0;
    size_t size() const noexcept { return count; }
    const uint32_t* data() const noexcept { return p.get(); }
    uint32_t operator[](size_t k) const noexcept { return p[k]; }
};

// QA-A-211b (#233, Codex #71): how far a masked pixel looks for a valid pixel to fill from. The pixels of a defect
// cluster (and of a blob of gain-classified pixels) can have no valid pixel in their 3x3 -- on CalData_6, 66.7% of the
// defect map's own pixels and 72.5% of defect-map-or-classified ones, at most 10 pixels from the nearest valid one
// (QA-A-211b evidence, 10_distance_to_valid.txt). They used to be filled with 0.0f. 16 is the measured 10 with margin;
// beyond it the pixel keeps its input value and the frame says so (see the caller).
constexpr int kFillMaxRadius = 16;

// QA-A-213 (#233): the Chebyshev distance of every masked pixel to the nearest valid pixel, up to kFillMaxRadius, in ONE
// pass over the masked pixels. QA-A-211b found that distance per pixel by trying ring after ring (up to 1,089 reads a
// pixel); on a 686x686 block (4.99% of a 3072x3072 frame, inside the density the SRS tolerates) that was 638 ms for the
// defect stage. The distance is a breadth-first search: layer 1 is the masked pixels that touch a valid pixel (8
// neighbours), layer k+1 the masked pixels not yet reached that touch layer k. An 8-neighbour step moves one in the
// Chebyshev metric, and every pixel on a shortest path from a masked pixel to its nearest valid pixel is masked (a
// valid one would be nearer), so the layer number IS the Chebyshev distance to the nearest valid pixel -- the very
// number the ring search found by trial. dist is 0 for a pixel not reached within kFillMaxRadius (and for valid
// pixels, which are never asked).
struct FillDistance {
    std::vector<uint8_t> dist;
    std::unique_ptr<uint8_t[]> denseDist;   // QA-A-315: buildDense()'s table (one entry a masked pixel), filled on several threads
    const uint8_t* table = nullptr;          // the table readers use: dist.data() after build(), denseDist after buildDense()
    bool byRank = false;                     // QA-A-315: buildDense()'s table is indexed by the position in `masked`, not by the slot
    bool ready = false;
    std::atomic<bool> built{false};   // QA-A-315: set (release) when `table` is complete; a reader that sees it skips call_once
    std::once_flag once;   // QA-A-296: the first pixel that needs the table builds it, whichever thread it is on

    // `dist` has one entry a MaskSet slot (QA-A-237f); it is read and written only for masked pixels.
    void build(const MaskSet& ms, const Grid& grid) {
        // QA-A-296 (Refs #278): the Chebyshev distance of every masked pixel to the nearest valid pixel, capped at kFillMaxRadius
        // (0 beyond). The layer number of the breadth-first search this replaces IS that distance (see above), and the distance
        // transform with a 3x3 mask -- a forward and a backward raster pass -- computes the same number exactly (the chessboard
        // distance, the frame edge ignored as a neighbour). It visits each masked pixel twice, in raster order, instead of
        // eight neighbours of each pixel layer after layer.
        const uint32_t width = grid.width;
        const uint32_t height = grid.height;
        constexpr uint8_t kInf = 255;
        const bool denseTable = (ms.slots() == static_cast<size_t>(width) * height);
        dist.assign(ms.slots(), denseTable ? uint8_t{0} : kInf);
        std::vector<uint32_t> order;
        order.reserve(ms.count());
        ms.forEach([&](size_t idx) { order.push_back(static_cast<uint32_t>(idx)); if (denseTable) dist[idx] = kInf; });
        // value of the neighbour at (x + dx, y + dy): 0 for a valid pixel, its distance so far for a masked one; false outside the frame
        const auto relax = [&](uint32_t idx, uint32_t x, uint32_t y, bool forward) -> int {
            int best = kInf;
            const auto look = [&](bool inside, size_t nidx) {
                if (!inside) return;
                int nd = 0;
                if (ms.test(nidx)) { nd = dist[ms.slot(nidx)]; if (nd == kInf) return; }
                if (nd + 1 < best) best = nd + 1;
            };
            if (forward) {
                look(y > 0 && x > 0, idx - width - 1);
                look(y > 0, idx - width);
                look(y > 0 && x + 1 < width, idx - width + 1);
                look(x > 0, idx - 1);
            } else {
                look(x + 1 < width, idx + 1);
                look(y + 1 < height && x > 0, idx + width - 1);
                look(y + 1 < height, idx + width);
                look(y + 1 < height && x + 1 < width, idx + width + 1);
            }
            return best;
        };
        if (ms.slots() == static_cast<size_t>(width) * height) {
            // Dense mask set (slot == pixel index): `dist` has an entry for every pixel and a valid pixel's entry is 0, so a neighbour needs
            // one load and no mask test. Same passes, same values.
            const size_t stride = width;
            for (size_t q = 0; q < order.size(); ++q) {
                const uint32_t idx = order[q];
                const uint32_t x = idx % width;
                const bool up = idx >= stride;
                int best = kInf;
                const auto take = [&](size_t nidx) { const int nd = dist[nidx]; if (nd != kInf && nd + 1 < best) best = nd + 1; };
                if (up) { if (x > 0) take(idx - stride - 1); take(idx - stride); if (x + 1 < width) take(idx - stride + 1); }
                if (x > 0) take(idx - 1);
                if (best < dist[idx]) dist[idx] = static_cast<uint8_t>(best);
            }
            for (size_t q = order.size(); q-- > 0;) {
                const uint32_t idx = order[q];
                const uint32_t x = idx % width;
                const bool down = idx / width + 1 < height;
                int best = kInf;
                const auto take = [&](size_t nidx) { const int nd = dist[nidx]; if (nd != kInf && nd + 1 < best) best = nd + 1; };
                if (x + 1 < width) take(idx + 1);
                if (down) { if (x > 0) take(idx + stride - 1); take(idx + stride); if (x + 1 < width) take(idx + stride + 1); }
                if (best < dist[idx]) dist[idx] = static_cast<uint8_t>(best);
            }
            for (const uint32_t idx : order) if (dist[idx] > kFillMaxRadius) dist[idx] = 0;
            table = dist.data();
            ready = true;
            return;
        }
        for (size_t q = 0; q < order.size(); ++q) {
            const uint32_t idx = order[q];
            const int d = relax(idx, idx % width, idx / width, true);
            uint8_t& cell = dist[ms.slot(idx)];
            if (d < cell) cell = static_cast<uint8_t>(d);
        }
        for (size_t q = order.size(); q-- > 0;) {
            const uint32_t idx = order[q];
            const int d = relax(idx, idx % width, idx / width, false);
            uint8_t& cell = dist[ms.slot(idx)];
            if (d < cell) cell = static_cast<uint8_t>(d);
        }
        for (const uint32_t idx : order) {
            uint8_t& cell = dist[ms.slot(idx)];
            if (cell > kFillMaxRadius) cell = 0;   // not reached within kFillMaxRadius (including kInf)
        }
        table = dist.data();
        ready = true;
    }

    // QA-A-315 (Refs #278): the same table for a DENSE mask set (slot == pixel index), on several threads. The Chebyshev distance is
    // separable: the distance of (x, y) to the nearest valid pixel is the minimum over the rows y' of max(|y - y'|, h(x, y')), where
    // h(x, y') is the distance along row y' from x to the nearest valid pixel of that row. h is computed row by row (rows are
    // independent), then each masked pixel takes the minimum over |y - y'| <= kFillMaxRadius; a value above kFillMaxRadius becomes 0,
    // as in build(). Every entry depends only on the mask, so the split into threads does not change the table, and the table is the
    // one the two raster passes of build() compute (the exact chessboard distance, the frame edge ignored as a neighbour).
    // `masked`: the masked pixels (any order). Both tables are allocated without a zero fill and written on several threads, so the first
    // touch of their pages is spread over the threads too (a zeroed 7.9 MB vector cost ~1.3 ms on the calling thread, QA-A-315 evidence 06).
    // The serial build() took 4.4-5.6 ms on the 236,647-368,710-pixel border bands of the 3072x2560 sets while every other thread
    // waited for it (QA-A-315 evidence 03).
    void buildDense(const MaskSet& ms, const Grid& grid, const uint32_t* masked, size_t maskedCount) {
        const uint32_t width = grid.width;
        const uint32_t height = grid.height;
        const size_t n = static_cast<size_t>(width) * height;
        constexpr int kFar = kFillMaxRadius + 1;   // farther than kFillMaxRadius
        // h is needed at the masked pixels only (a valid pixel's is 0, and the pass below tests the mask instead of reading it). The masked
        // pixels of a row are a stretch of `masked` (ascending); each run [a, b] of consecutive ones has its nearest valid pixels at a - 1 and
        // b + 1 (when inside the row), so h(x) = min(x - a + 1, b + 1 - x), capped at kFar -- the two passes along the row compute the same.
        const std::unique_ptr<uint8_t[]> h(new uint8_t[n]);
        uint8_t* const hw = h.get();
        xpe_parallel_chunks(height, 64, [&](size_t y0, size_t y1) {
            const uint32_t* q = std::lower_bound(masked, masked + maskedCount, static_cast<uint32_t>(y0 * width));
            const uint32_t* const qEnd = std::lower_bound(q, masked + maskedCount, static_cast<uint32_t>(std::min<size_t>(y1 * width, n)));
            while (q < qEnd) {
                const size_t row = static_cast<size_t>(*q / width) * width;
                const uint32_t a = static_cast<uint32_t>(*q - row);
                const uint32_t* r = q + 1;
                while (r < qEnd && *r == r[-1] + 1u && (*r - row) < width) ++r;
                const uint32_t b = static_cast<uint32_t>(r[-1] - row);
                for (uint32_t x = a; x <= b; ++x) {
                    const int left = a > 0 ? static_cast<int>(x - a + 1) : kFar;
                    const int right = b + 1 < width ? static_cast<int>(b + 1 - x) : kFar;
                    hw[row + x] = static_cast<uint8_t>(std::min(kFar, std::min(left, right)));
                }
                q = r;
            }
        });
        // One entry a masked pixel, at its position in `masked` (a pixel-sized table touched the pages of the whole frame wherever the band
        // runs down the sides; median_filter_cluster is handed the position by its caller and reads masked pixels only).
        denseDist.reset(new uint8_t[std::max<size_t>(1, maskedCount)]);
        uint8_t* const out = denseDist.get();
        const uint8_t* const hp = h.get();
        std::atomic<size_t> next{0};
        constexpr size_t kBlock = 4096;
        xpe_parallel_chunks(std::min<size_t>(XPE_PARALLEL_MAX_THREADS, std::max<size_t>(1, maskedCount / 20000)), 1, [&](size_t, size_t) {
            for (;;) {
                const size_t b = next.fetch_add(kBlock);
                if (b >= maskedCount) break;
                const size_t e = std::min(maskedCount, b + kBlock);
                for (size_t q = b; q < e; ++q) {
                    const size_t idx = masked[q];
                    const size_t y = idx / width;
                    int best = hp[idx];
                    for (int k = 1; k < best; ++k) {   // a row k away offers at least k; a valid pixel there offers exactly k
                        const size_t off = static_cast<size_t>(k) * width;
                        if (y >= static_cast<size_t>(k)) { const size_t j = idx - off; const int c = ms.test(j) ? std::max<int>(k, hp[j]) : k; if (c < best) best = c; }
                        if (y + static_cast<size_t>(k) < height) { const size_t j = idx + off; const int c = ms.test(j) ? std::max<int>(k, hp[j]) : k; if (c < best) best = c; }
                    }
                    out[q] = static_cast<uint8_t>(best > kFillMaxRadius ? 0 : best);
                }
            }
        });
        table = out;
        byRank = true;
        ready = true;
    }
};

// @MX:NOTE: [AUTO] 3x3 median filter for defect cluster correction
// Collects valid neighbor pixels and returns median value. When the 3x3 holds none (the interior of a cluster), the
// median of the valid pixels of the NEAREST Chebyshev ring that holds one is used (QA-A-211b); the ring's radius is the
// pixel's distance to the nearest valid pixel, which `fd` knows from one breadth-first pass (QA-A-213) instead of a
// trial of ring 2, 3, ... -- the set of values, and so the median, is the one the trial found. A pixel with no valid
// pixel within kFillMaxRadius keeps its own input value (`found` false), never 0.
// Only unmasked pixels are read, whatever the radius: the in-place guarantee of xpe_defect_correct_in holds.
float median_filter_cluster(const float* pixels, const MaskSet& ms,
                             uint32_t x, uint32_t y,
                             uint32_t width, uint32_t height, FillDistance& fd, std::vector<float>& values, bool* found,
                             size_t rank = 0)
{
    values.clear();   // a scratch buffer the caller reuses: no allocation per pixel (QA-A-213)
    *found = true;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;

            int nx = static_cast<int>(x) + dx;
            int ny = static_cast<int>(y) + dy;

            if (nx >= 0 && ny >= 0 &&
                static_cast<uint32_t>(nx) < width &&
                static_cast<uint32_t>(ny) < height) {
                uint32_t idx = static_cast<uint32_t>(ny) * width + static_cast<uint32_t>(nx);
                if (!ms.test(idx)) { // valid pixel
                    values.push_back(pixels[idx]);
                }
            }
        }
    }

    if (values.empty()) {
        if (!fd.built.load(std::memory_order_acquire)) {
            std::call_once(fd.once, [&]() { fd.build(ms, Grid{width, height}); fd.built.store(true, std::memory_order_release); });
        }
        // 0: none within kFillMaxRadius. `rank`: the pixel's position in the list buildDense() was given (QA-A-315), read only then.
        const int radius = fd.table[fd.byRank ? rank : ms.slot(static_cast<size_t>(y) * width + x)];
        if (radius >= 2) {
            const uint8_t* const plane = ms.densePlane();
            for (int dy = -radius; dy <= radius; ++dy) {
                const int ny = static_cast<int>(y) + dy;
                if (ny < 0 || static_cast<uint32_t>(ny) >= height) continue;
                const bool edgeRow = (dy == -radius || dy == radius);
                if (edgeRow && plane != nullptr) {
                    // QA-A-315 (Refs #278): the ring's top and bottom rows on a dense plane, 16 mask bytes at a time; the valid pixels
                    // are taken in ascending x, the order of the loop below, so `values` is the same vector.
                    const size_t rowBase = static_cast<size_t>(ny) * width;
                    const uint8_t* const mrow = plane + rowBase;
                    const float* const prow = pixels + rowBase;
                    const int x1 = std::min(static_cast<int>(x) + radius, static_cast<int>(width) - 1);
                    int xx = std::max(static_cast<int>(x) - radius, 0);
                    const __m128i zero = _mm_setzero_si128();
                    for (; xx + 16 <= x1 + 1; xx += 16) {
                        unsigned m = static_cast<unsigned>(_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(mrow + xx)), zero)));
                        while (m != 0) {
                            values.push_back(prow[xx + countTrailingZeros64(m)]);
                            m &= m - 1;
                        }
                    }
                    for (; xx <= x1; ++xx) if (mrow[xx] == 0) values.push_back(prow[xx]);
                    continue;
                }
                for (int dx = -radius; dx <= radius; dx += (edgeRow ? 1 : 2 * radius)) {
                    const int nx = static_cast<int>(x) + dx;
                    if (nx < 0 || static_cast<uint32_t>(nx) >= width) continue;
                    const size_t idx = static_cast<size_t>(ny) * width + static_cast<uint32_t>(nx);
                    if (!ms.test(idx)) values.push_back(pixels[idx]);
                }
            }
        }
    }

    if (values.empty()) {
        *found = false;
        return pixels[static_cast<size_t>(y) * width + x];
    }

    // The element a full sort would leave at size/2 -- the same value, found without sorting the rest.
    size_t mid = values.size() / 2u;
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(mid), values.end());
    return values[mid];
}


// QA-A-255 (SRS-CALIB-FUNC-039): the correction of a frame whose defect file carries its layers. Everything is READ from `src` (valid pixels) or from `dst` (pixels this pass filled, blobs only), so the result
// is the same for an in-place call. The pixels outside the imaging area are in the mask set (no source) and are never written.
constexpr int kLineReach = 8;   // how far across a line group a valid pixel is looked for

struct LayerPass {
    const float*               src;
    float*                     dst;
    const MaskSet*             ms;
    const DefectLayers*        layers;
    Grid                       grid;
    std::vector<bool>*         visited;   // by MaskSet slot: corrected, or taken into a cluster
    std::vector<uint8_t>*      cstate;    // by MaskSet slot: 0 not in the blob, 1 pending, 2 queued, 3 filled
    FillDistance*              fd;
    std::vector<float>*        scratch;
    XpeDefectCorrectionReport* rep;
    std::vector<uint8_t>*      low;       // by pixel: 1 = low confidence (may be empty = not wanted)
};

inline bool inArea(const DefectLayers& L, size_t idx) { return L.area.empty() || L.area[idx] != 0; }

struct AcrossHit {
    float value;
    int   pos;
    bool  ok;
};

// Where findAcross looks: along y (alongY) or x, at the fixed other coordinate, from `start` going `dir` (-1 / +1). A struct, so that the two
// integers and the flag/coordinate pair cannot be swapped at a call site.
struct AcrossRay {
    bool     alongY;
    uint32_t fixedCoord;
    int      start;
    int      dir;
};

// The nearest valid pixel along the ray.
AcrossHit findAcross(const LayerPass& c, const AcrossRay& ray)
{
    const bool alongY = ray.alongY;
    const uint32_t fixedCoord = ray.fixedCoord;
    const int start = ray.start, dir = ray.dir;
    const int limit = static_cast<int>(alongY ? c.grid.height : c.grid.width);
    for (int k = 0; k < kLineReach; ++k) {
        const int pos = start + dir * k;
        if (pos < 0 || pos >= limit) break;
        const size_t idx = alongY ? static_cast<size_t>(pos) * c.grid.width + fixedCoord : static_cast<size_t>(fixedCoord) * c.grid.width + static_cast<size_t>(pos);
        if (!c.ms->test(idx)) return AcrossHit{c.src[idx], pos, true};
    }
    return AcrossHit{0.0f, 0, false};
}

void markLow(LayerPass& c, size_t idx)
{
    if (!c.low->empty() && (*c.low)[idx] == 0) {
        (*c.low)[idx] = 1;
        ++c.rep->low_confidence_pixels;
    }
}

// QA-A-255b (Codex #223): a pixel of a line group of three or more lines, or of a group whose correction is REJECT, is low confidence by what it IS, not by how it was corrected. The state is set here, for
// every pixel of every such group, before any correction path runs, so a pixel the line stage skips (no valid pixel across it) or that another stage takes first keeps it.
void markLowGroups(LayerPass& c)
{
    for (const DefectExtLine& L : c.layers->lines) {
        if (!((L.width >= 3u) || (L.correction == XPE_DEFECT_LINE_CORRECT_REJECT))) continue;
        const bool rowLine = (L.orientation == 0);
        bool       any = false;
        for (const DefectExtSegment& sg : L.segments)
            for (uint32_t p = sg.start; p < sg.end; ++p) {
                const uint32_t x = rowLine ? p : sg.line, y = rowLine ? sg.line : p;
                if (x >= c.grid.width || y >= c.grid.height) continue;
                const size_t idx = static_cast<size_t>(y) * c.grid.width + x;
                if (!c.ms->test(idx) || !inArea(*c.layers, idx)) continue;
                markLow(c, idx);
                any = true;
            }
        if (any) ++c.rep->low_confidence_regions;
    }
}

void correctLines(LayerPass& c)
{
    const uint32_t W = c.grid.width;
    for (uint32_t orientation = 0; orientation < 2; ++orientation) {
        const bool rowLine = (orientation == 0);   // a row line is crossed along y
        for (const DefectExtLine& L : c.layers->lines) {
            if (L.orientation != orientation || L.segments.empty()) continue;
            const int  first = static_cast<int>(L.index), last = static_cast<int>(L.index + L.width) - 1;
            bool       any = false;
            for (const DefectExtSegment& sg : L.segments) {
                for (uint32_t p = sg.start; p < sg.end; ++p) {
                    const uint32_t x = rowLine ? p : sg.line, y = rowLine ? sg.line : p;
                    if (x >= c.grid.width || y >= c.grid.height) continue;
                    const size_t idx = static_cast<size_t>(y) * W + x;
                    if (!c.ms->test(idx) || !inArea(*c.layers, idx)) continue;
                    const size_t slot = c.ms->slot(idx);
                    if ((*c.visited)[slot]) continue;
                    const uint32_t fixedCoord = rowLine ? x : y;
                    const AcrossHit a = findAcross(c, AcrossRay{rowLine, fixedCoord, first - 1, -1});
                    const AcrossHit b = findAcross(c, AcrossRay{rowLine, fixedCoord, last + 1, +1});
                    if (!a.ok && !b.ok) continue;   // no valid pixel across: the blob stage fills it from what is around
                    float v;
                    if (a.ok && b.ok) {
                        const double coord = rowLine ? y : x;
                        v = static_cast<float>(static_cast<double>(a.value) + (static_cast<double>(b.value) - static_cast<double>(a.value)) * (coord - a.pos) / static_cast<double>(b.pos - a.pos));
                    } else {
                        v = a.ok ? a.value : b.value;
                        ++c.rep->one_sided_line_pixels;
                    }
                    c.dst[idx] = v;
                    (*c.visited)[slot] = true;
                    ++c.rep->line_pixels;
                    any = true;
                }
            }
            if (any) ++c.rep->line_groups;
        }
    }
}

// The median of the usable neighbours of the pixel `idx` (row-major index): valid pixels, and pixels of this blob that are already filled.
bool peelValue(LayerPass& c, uint32_t idx, float* out)
{
    const uint32_t W = c.grid.width, H = c.grid.height;
    const uint32_t x = idx % W, y = idx / W;
    float          vals[8];
    int            count = 0;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            const int nx = static_cast<int>(x) + dx, ny = static_cast<int>(y) + dy;
            if (nx < 0 || ny < 0 || static_cast<uint32_t>(nx) >= W || static_cast<uint32_t>(ny) >= H) continue;
            const size_t j = static_cast<size_t>(ny) * W + static_cast<uint32_t>(nx);
            if (!c.ms->test(j) || (*c.cstate)[c.ms->slot(j)] == 3) vals[count++] = c.dst[j];
        }
    if (count == 0) return false;
    std::nth_element(vals, vals + count / 2, vals + count);
    *out = vals[count / 2];
    return true;
}

// A blob of two or more pixels, filled from the outside in (see the header of this patch). `cluster` holds its pixels.
void fillBlob(LayerPass& c, const std::vector<uint32_t>& cluster)
{
    const uint32_t W = c.grid.width, H = c.grid.height;
    for (uint32_t idx : cluster) (*c.cstate)[c.ms->slot(idx)] = 1;
    std::vector<uint32_t> frontier, next;
    std::vector<float>    values;
    float                 probe;
    for (uint32_t idx : cluster)
        if (peelValue(c, idx, &probe)) {
            frontier.push_back(idx);
            (*c.cstate)[c.ms->slot(idx)] = 2;
        }
    while (!frontier.empty()) {
        values.assign(frontier.size(), 0.0f);
        for (size_t k = 0; k < frontier.size(); ++k) peelValue(c, frontier[k], &values[k]);   // from the state before this layer
        for (size_t k = 0; k < frontier.size(); ++k) {
            c.dst[frontier[k]] = values[k];
            (*c.cstate)[c.ms->slot(frontier[k])] = 3;
            ++c.rep->blob_pixels;
        }
        next.clear();
        for (uint32_t idx : frontier) {
            const int x = static_cast<int>(idx % W), y = static_cast<int>(idx / W);
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    const int nx = x + dx, ny = y + dy;
                    if (nx < 0 || ny < 0 || static_cast<uint32_t>(nx) >= W || static_cast<uint32_t>(ny) >= H) continue;
                    const size_t j = static_cast<size_t>(ny) * W + static_cast<uint32_t>(nx);
                    if (!c.ms->test(j)) continue;
                    uint8_t& st = (*c.cstate)[c.ms->slot(j)];
                    if (st == 1) {
                        st = 2;
                        next.push_back(static_cast<uint32_t>(j));
                    }
                }
        }
        frontier.swap(next);
    }
    bool anyLow = false;
    for (uint32_t idx : cluster) {
        uint8_t& st = (*c.cstate)[c.ms->slot(idx)];
        if (st == 1) {   // no chain of neighbours reaches it: the ring fill of the first edition, marked
            bool found = true;
            c.dst[idx] = median_filter_cluster(c.src, *c.ms, idx % W, idx / W, W, H, *c.fd, *c.scratch, &found);
            if (!found) ++c.rep->unfilled_pixels;
            else ++c.rep->blob_pixels;
            markLow(c, idx);
            anyLow = true;
        }
    }
    if (anyLow) ++c.rep->low_confidence_regions;
    for (uint32_t idx : cluster) (*c.cstate)[c.ms->slot(idx)] = 0;
}

void correctBlobs(LayerPass& c)
{
    const uint32_t W = c.grid.width, H = c.grid.height;
    c.ms->forEach([&](size_t idx) {
        const size_t slot = c.ms->slot(idx);
        if ((*c.visited)[slot] || !inArea(*c.layers, idx)) return;
        std::vector<uint32_t> cluster;
        std::queue<uint32_t>  q;
        q.push(static_cast<uint32_t>(idx));
        (*c.visited)[slot] = true;
        while (!q.empty()) {
            const uint32_t cur = q.front();
            q.pop();
            cluster.push_back(cur);
            const int cx = static_cast<int>(cur % W), cy = static_cast<int>(cur / W);
            static const int dx[4] = {-1, 1, 0, 0}, dy[4] = {0, 0, -1, 1};
            for (int k = 0; k < 4; ++k) {
                const int nx = cx + dx[k], ny = cy + dy[k];
                if (nx < 0 || ny < 0 || static_cast<uint32_t>(nx) >= W || static_cast<uint32_t>(ny) >= H) continue;
                const size_t j = static_cast<size_t>(ny) * W + static_cast<uint32_t>(nx);
                if (!c.ms->test(j) || !inArea(*c.layers, j)) continue;
                const size_t js = c.ms->slot(j);
                if ((*c.visited)[js]) continue;
                (*c.visited)[js] = true;
                q.push(static_cast<uint32_t>(j));
            }
        }
        if (cluster.size() == 1u) {
            const MaskSet& ms = *c.ms;
            c.dst[idx] = xpe_interpolate_pixel_masked(c.src, [&](size_t i) { return ms.test(i); }, static_cast<uint32_t>(idx % W), static_cast<uint32_t>(idx / W), W, H);
            ++c.rep->isolated_pixels;
        } else {
            fillBlob(c, cluster);
        }
    });
}

// QA-A-296 (Refs #278): the runs of masked pixels that are line defects in a version-1 map. The masked pixels come in raster order, so a row
// run is closed by the first masked pixel that is not the next one in its row; a column run is tracked per column. Each run is a line of width
// 1: the interpolation looks across it (kLineReach) past any masked pixel, so a group of adjacent runs is crossed in one step.
struct LineRun {
    uint32_t vertical;   // 0 row run, 1 column run
    uint32_t line;       // row (or column) number
    uint32_t start;      // first pixel along the line
    uint32_t end;        // one past the last
};

void classifyLineRuns(const MaskSet& ms, const Grid& g, const XpeDefectLineOptions& o, std::vector<LineRun>* out)
{
    const uint32_t minRun = o.minRun < 2u ? 2u : o.minRun;
    uint32_t rowY = 0, rowStart = 0, rowLen = 0;
    std::vector<uint32_t> colStart, colLen, colLast;
    if (o.cols) {
        colStart.assign(g.width, 0u);
        colLen.assign(g.width, 0u);
        colLast.assign(g.width, 0u);
    }
    ms.forEach([&](size_t idx) {
        const uint32_t x = static_cast<uint32_t>(idx % g.width), y = static_cast<uint32_t>(idx / g.width);
        if (o.rows) {
            if (rowLen > 0 && y == rowY && x == rowStart + rowLen) {
                ++rowLen;
            } else {
                if (rowLen >= minRun) out->push_back(LineRun{0u, rowY, rowStart, rowStart + rowLen});
                rowY = y; rowStart = x; rowLen = 1;
            }
        }
        if (o.cols) {
            if (colLen[x] > 0 && colLast[x] + 1u == y) {
                ++colLen[x];
                colLast[x] = y;
            } else {
                if (colLen[x] >= minRun) out->push_back(LineRun{1u, x, colStart[x], colStart[x] + colLen[x]});
                colLen[x] = 1; colStart[x] = y; colLast[x] = y;
            }
        }
    });
    if (o.rows && rowLen >= minRun) out->push_back(LineRun{0u, rowY, rowStart, rowStart + rowLen});
    if (o.cols)
        for (uint32_t x = 0; x < g.width; ++x)
            if (colLen[x] >= minRun) out->push_back(LineRun{1u, x, colStart[x], colStart[x] + colLen[x]});
}

// The nearest valid pixel along y (alongY) or x from `start` going `dir`, at most kLineReach steps (the same search as findAcross).
// NOLINTBEGIN(bugprone-easily-swappable-parameters): fixedCoord is the column (alongY) or row of the line's crossing pixel and start the first coordinate searched along the other axis; the two call sites pass (fixedCoord, line - 1, -1) and (fixedCoord, line + 1, +1)
bool acrossValid(const float* src, const MaskSet& ms, const Grid& g, bool alongY, uint32_t fixedCoord, int start, int dir, float* value, int* pos)
// NOLINTEND(bugprone-easily-swappable-parameters)
{
    const int limit = static_cast<int>(alongY ? g.height : g.width);
    for (int k = 0; k < kLineReach; ++k) {
        const int p = start + dir * k;
        if (p < 0 || p >= limit) break;
        const size_t idx = alongY ? static_cast<size_t>(p) * g.width + fixedCoord : static_cast<size_t>(fixedCoord) * g.width + static_cast<size_t>(p);
        if (!ms.test(idx)) { *value = src[idx]; *pos = p; return true; }
    }
    return false;
}

// One run, interpolated linearly across the line from the nearest valid pixels on both sides (the formula of correctLines); a pixel with a valid
// pixel on one side only takes its value; a pixel with none on either side is left to the cluster walk. Corrected pixels are marked in `done`.
void interpolateRun(const float* src, float* dst, const MaskSet& ms, const Grid& g, const LineRun& r, std::vector<uint8_t>& done)
{
    const bool rowLine = (r.vertical == 0);   // a row run is crossed along y
    if (rowLine) {
        // A row run is searched row by row across its whole length: the rows above and below are read left to right, which is the order they
        // lie in memory, instead of one pixel at a time down a column. The nearest valid pixel above (and below) each x is the first one found
        // going outwards, exactly what acrossValid finds for that x.
        thread_local std::vector<float> av, bv;
        thread_local std::vector<int> ap, bp;
        thread_local std::vector<uint8_t> aok, bok;
        const uint32_t x0 = r.start, x1 = std::min(r.end, g.width);
        if (r.line >= g.height || x0 >= x1) return;
        const size_t len = x1 - x0;
        for (int side = 0; side < 2; ++side) {
            std::vector<float>& val = side == 0 ? av : bv;
            std::vector<int>& pos = side == 0 ? ap : bp;
            std::vector<uint8_t>& ok = side == 0 ? aok : bok;
            val.assign(len, 0.0f); pos.assign(len, 0); ok.assign(len, 0);
            size_t remaining = len;
            for (int k = 0; k < kLineReach && remaining > 0; ++k) {
                const int py = side == 0 ? static_cast<int>(r.line) - 1 - k : static_cast<int>(r.line) + 1 + k;
                if (py < 0 || py >= static_cast<int>(g.height)) break;
                const size_t rowBase = static_cast<size_t>(py) * g.width;
                for (size_t i = 0; i < len; ++i) {
                    if (ok[i]) continue;
                    const size_t idx = rowBase + x0 + i;
                    if (!ms.test(idx)) { ok[i] = 1; val[i] = src[idx]; pos[i] = py; --remaining; }
                }
            }
        }
        for (size_t i = 0; i < len; ++i) {
            const size_t idx = static_cast<size_t>(r.line) * g.width + x0 + i;
            const size_t slot = ms.slot(idx);
            if (done[slot]) continue;
            if (!aok[i] && !bok[i]) continue;
            float v;
            if (aok[i] && bok[i]) {
                const double coord = r.line;
                v = static_cast<float>(static_cast<double>(av[i]) + (static_cast<double>(bv[i]) - static_cast<double>(av[i])) * (coord - ap[i]) / static_cast<double>(bp[i] - ap[i]));
            } else {
                v = aok[i] ? av[i] : bv[i];
            }
            dst[idx] = v;
            done[slot] = 1;
        }
        return;
    }
    for (uint32_t p = r.start; p < r.end; ++p) {
        const uint32_t x = rowLine ? p : r.line, y = rowLine ? r.line : p;
        if (x >= g.width || y >= g.height) continue;
        const size_t idx = static_cast<size_t>(y) * g.width + x;
        const size_t slot = ms.slot(idx);
        if (done[slot]) continue;
        const uint32_t fixedCoord = rowLine ? x : y;
        float av = 0.0f, bv = 0.0f;
        int ap = 0, bp = 0;
        const bool aok = acrossValid(src, ms, g, rowLine, fixedCoord, static_cast<int>(r.line) - 1, -1, &av, &ap);
        const bool bok = acrossValid(src, ms, g, rowLine, fixedCoord, static_cast<int>(r.line) + 1, +1, &bv, &bp);
        if (!aok && !bok) continue;
        float v;
        if (aok && bok) {
            const double coord = rowLine ? y : x;
            v = static_cast<float>(static_cast<double>(av) + (static_cast<double>(bv) - static_cast<double>(av)) * (coord - ap) / static_cast<double>(bp - ap));
        } else {
            v = aok ? av : bv;
        }
        dst[idx] = v;
        done[slot] = 1;
    }
}

} // anonymous namespace

// @MX:ANCHOR: [AUTO] xpe_defect_correct — public API entry point (new g_calib-based)
// @MX:REASON: Called after gain correction; reads g_calib.defect_map; fan_in >= 3
// @MX:SPEC: REQ-P1A-012, REQ-P1A-020
XpeErrorCode xpe_defect_correct_in(
    const CalibSnapshot&    calib,
    const XpeImageBuffer*  input,
    XpeImageBuffer*         output,
    const XpeImageMetadata* metadata,
    const std::vector<uint32_t>* frame_defects,
    XpeDefectCorrectionReport* report,
    std::vector<uint8_t>*   low_confidence,
    const XpeDefectLineOptions* line_options) try
{
    if (report) {
        *report = XpeDefectCorrectionReport{};
        report->size = static_cast<uint32_t>(sizeof(XpeDefectCorrectionReport));
    }
    if (!input || !output || !metadata) return XPE_ERR_INVALID_INPUT;
    if (!input->data || !output->data) return XPE_ERR_INVALID_INPUT;
    if (input->format != XPE_PIXEL_FLOAT32) return XPE_ERR_UNSUPPORTED_FORMAT;
    if (input->width == 0 || input->height == 0) return XPE_ERR_INVALID_INPUT;
    if (input->width > std::numeric_limits<size_t>::max() / input->height) return XPE_ERR_INVALID_INPUT;
    if (output->width  != input->width ||
        output->height != input->height) return XPE_ERR_BUFFER_TOO_SMALL;

    const size_t n = static_cast<size_t>(input->width) * input->height;

    // #123 dataSize input contract (docs/project/api-spec.md, 2026-09-10):
    // 0 means *unspecified* -- trust the dimensions. A non-zero value smaller
    // than the dimensions require is refused here, before the kernel reads
    // width*height pixels past the end of the allocation (QA-B-18).
    if (input->dataSize != 0 && input->dataSize < n * sizeof(float))
        return XPE_ERR_INVALID_INPUT;
    if (output->dataSize < n * sizeof(float)) return XPE_ERR_BUFFER_TOO_SMALL;

    // ALIASING CONTRACT -- QA-A-146c (#209). Exactly two shapes are supported:
    //   input->data == output->data   in-place, bit-identical to out-of-place
    //   fully disjoint ranges         the ordinary case
    // PARTIAL OVERLAP IS REFUSED HERE, before anything is written.
    //
    // It is not refused because it is known to be wrong -- it is refused
    // because nobody has ever called it that way, so nothing measures whether
    // the result would be right. Widening the contract to cover it would
    // guarantee behaviour no test observes, which is the shape of defect #207
    // (an AC promising an AVX2 path that did not exist). Left merely
    // undocumented it would instead run into UB with no signal at all; an
    // error code makes the contract violation observable to the caller.
    //
    // Compared in BYTES over the range this function actually touches
    // (n * sizeof(float) on both sides). input->dataSize is not used as the
    // basis: per the #123 contract 0 means *unspecified*, so it is not a
    // reliable length -- and where it is specified it equals this.
    {
        const auto* in_b  = static_cast<const unsigned char*>(input->data);
        const auto* out_b = static_cast<const unsigned char*>(output->data);
        const size_t bytes = n * sizeof(float);
        const bool identical = (in_b == out_b);
        const bool disjoint  = (in_b + bytes <= out_b) || (out_b + bytes <= in_b);
        if (!identical && !disjoint) return XPE_ERR_INVALID_INPUT;
    }

    // SPEC-XPE-P1A REQ-P1A-020: while the module is not initialized, every
    // processing function returns XPE_ERR_NOT_INITIALIZED. Checked explicitly --
    // before #117 decision B the missing calibration map stood in for this, which
    // is why the two states could not be told apart.
    if (!calib.initialized) return XPE_ERR_NOT_INITIALIZED;

    // #117 decision B: the module is initialized -- what is missing is the
    // calibration map. XPE_ERR_NOT_INITIALIZED is reserved for
    // xpe_preprocess_init() not called / after shutdown (SPEC-XPE-P1A
    // REQ-P1A-020), so the caller can tell the two apart.
    if (!calib.defect_map) return XPE_ERR_CALIB_NOT_LOADED;
    if (calib.defect_width  != input->width ||
        calib.defect_height != input->height) return XPE_ERR_INVALID_INPUT;

    // QA-A-214b (#233): THE FRAME MUST BE FINITE. The fill takes a median of neighbours (or a mean, for a lone defect),
    // and a NaN among them makes the result depend on the order the values were collected in -- mirroring a frame
    // changed the output of 110 of 400 test frames (QA-A-214) -- while an infinity or a NaN in a mean spreads into the
    // corrected pixel and the call still answered XPE_OK. A consumer refuses a non-finite input at its entrance and
    // does not write: the output buffer is untouched (and, called in place, so is the input), and the caller is told
    // where the first bad pixel is. The whole frame is checked, masked pixels included (their own value is what a pixel
    // with no valid neighbour keeps). Inside the pipeline this cannot fire: the stage is handed the gain stage's output
    // (at most 65535 / 0.1), a converted uint16, or binning's checked result. It is the direct callers of this public
    // function it protects (the GUI preview service, its synthetic oracle and an integration test among them).
    // One linear pass over the exponent bits; the count and the position are found only when something is wrong.
    {
        const float* const frame = static_cast<const float*>(input->data);
        constexpr uint32_t kExpMask = 0x7F800000u;   // all exponent bits set: NaN or +-infinity
        // QA-A-296: on several threads (a flag per range, or-ed: the verdict does not depend on the split)
        std::atomic<uint32_t> badFlag{0};
        xpe_parallel_chunks(n, size_t{1} << 20, [&](size_t b0, size_t b1) {
            uint32_t acc = 0;
            for (size_t i = b0; i < b1; ++i) {
                uint32_t b;
                std::memcpy(&b, frame + i, sizeof(b));
                acc |= static_cast<uint32_t>((b & kExpMask) == kExpMask);
            }
            if (acc != 0) badFlag.store(1);
        });
        const uint32_t bad = badFlag.load();
        if (bad != 0) {
            size_t count = 0, first = 0;
            for (size_t i = 0; i < n; ++i) {
                uint32_t b;
                std::memcpy(&b, frame + i, sizeof(b));
                if ((b & kExpMask) == kExpMask) { if (count == 0) first = i; ++count; }
            }
            char msg[320];
            std::snprintf(msg, sizeof(msg),
                "XPE_WARN_DEFECT_INPUT_NOT_FINITE: %zu pixel(s) of the input frame are NaN or infinite (first: index %zu, x=%zu, y=%zu); "
                "the frame was not corrected and the output was not written",
                count, first, first % input->width, first / input->width);
            msg[sizeof(msg) - 1] = '\0';
            xpe_alert_push(msg, XPE_ALERT_ERROR);
            return XPE_ERR_INVALID_INPUT;
        }
    }

    // QA-A-202 (#233): shared ownership of the map, no copy of it (9.4 MB at 3072x3072). QA-A-202d (Codex #32):
    // it comes from `calib`, the snapshot the caller took, so a reload during the frame -- or between two
    // stages of it -- leaves this call the map the frame started with.
    const std::shared_ptr<uint8_t[]>& dm_local = calib.defect_map;

    const uint32_t W  = input->width;
    const uint32_t H  = input->height;
    const float*   src = static_cast<const float*>(input->data);
    float*         dst = static_cast<float*>(output->data);
    const uint8_t* dm  = dm_local.get();

    // QA-A-211 (#233): the pixels the GAIN calibration classified defective are corrected like the map's own. The mask
    // the kernels read is the UNION of the loaded defect map, the scalar map's list kept in the snapshot and this
    // frame's list (a polynomial gain classifies per frame). Nothing is copied unless one of the lists is non-empty, so
    // a calibration whose gain classified nothing takes the path it always took. The map itself is never modified: it
    // is shared with the store.
    // QA-A-211 (#233): the mask the kernels read is the UNION of the loaded map, the scalar map's classified list and this frame's
    // list. QA-A-237g: it is a MaskSet, not a copy of the map with the extra pixels set. The map itself is never modified: it is
    // shared with the store.
    const size_t frame_extra = frame_defects ? frame_defects->size() : 0;
    const bool addsToMap = calib.gain_defect_count > 0 || frame_extra > 0;
    // The pixels added to the map, ascending, unique, each below n. The scalar map's list (a snapshot member, kept ascending by the
    // load) is read in place when it is the only list; a per-frame list, or a list that is not in that order, is merged into a
    // sorted copy.
    const uint32_t* extra = nullptr;
    size_t extraCount = 0;
    std::vector<uint32_t> extraOwn;
    if (addsToMap) {
        const uint32_t* listed = calib.gain_defect_idx.get();
        const size_t listedCount = listed ? calib.gain_defect_count : 0;
        bool ascending = true;
        for (size_t k = 1; k < listedCount && ascending; ++k) ascending = listed[k - 1] < listed[k];
        if (frame_extra == 0 && ascending) {
            extra = listed;
            extraCount = listedCount;
            if (n <= std::numeric_limits<uint32_t>::max()) {
                extraCount = static_cast<size_t>(std::lower_bound(listed, listed + listedCount, static_cast<uint32_t>(n)) - listed);
            }
        } else {
            extraOwn.reserve(listedCount + frame_extra);
            for (size_t k = 0; k < listedCount; ++k) {
                if (listed[k] < n) extraOwn.push_back(listed[k]);
            }
            if (frame_defects) {
                for (const uint32_t idx : *frame_defects) {
                    if (idx < n) extraOwn.push_back(idx);
                }
            }
            std::sort(extraOwn.begin(), extraOwn.end());
            extraOwn.erase(std::unique(extraOwn.begin(), extraOwn.end()), extraOwn.end());
            extra = extraOwn.data();
            extraCount = extraOwn.size();
        }
    }
    // QA-A-255: a version-2 map's layers. The pixels outside the imaging area join the mask set (so that no fill reads them) but are never written.
    const DefectLayers* const layers = (calib.defect_layers && calib.defect_layers->width == W && calib.defect_layers->height == H) ? calib.defect_layers.get() : nullptr;
    const size_t outsideN = layers ? layers->outside : 0u;
    std::vector<uint8_t> maskPlane;
    const uint8_t*       maskBase = dm;
    if (layers && outsideN > 0) {
        maskPlane.assign(dm, dm + n);
        for (size_t i = 0; i < n; ++i)
            if (layers->area[i] == 0) maskPlane[i] = 1;
        maskBase = maskPlane.data();
    }
    MaskSet maskSet;
    maskSet.build(maskBase, n, extra, extraCount);
    extraOwn = std::vector<uint32_t>();   // built into the set

    if (addsToMap) {
        // D1: the union is above the density SRS-CALIB-FUNC-003 tolerates -> ONE warning for the frame, never a refusal.
        const size_t u = maskSet.count() - outsideN;
        if (static_cast<double>(u) > XPE_GAIN_DEFECT_MAX_FRACTION * static_cast<double>(n)) {
            char msg[320];
            std::snprintf(msg, sizeof(msg),
                "XPE_WARN_DEFECT_UNION_OVER_LIMIT: the defect map together with the pixels classified defective by the gain "
                "calibration covers %zu of %zu pixel(s) (%.3f%%), above the %.1f%% defect density SRS-CALIB-FUNC-003 tolerates; "
                "the frame is corrected, but the correction fills a large part of it from neighbours",
                u, n, 100.0 * static_cast<double>(u) / static_cast<double>(n), 100.0 * XPE_GAIN_DEFECT_MAX_FRACTION);
            msg[sizeof(msg) - 1] = '\0';
            xpe_alert_push(msg, XPE_ALERT_WARNING);
        }
    }

    // Copy input -> output first. SKIPPED WHEN THE CALLER PASSED ONE BUFFER:
    // std::memcpy requires non-overlapping regions, so dst == src is undefined
    // behaviour even though it happens to work on this toolchain. In-place is
    // a documented, supported call shape as of QA-A-146 (#209) -- the contract
    // must not rest on UB. The copy is also pure waste there.
    if (dst != src) {
        std::memcpy(dst, src, n * sizeof(float));
    }

    const bool hasDefects = maskSet.count() > outsideN;
    if (low_confidence) low_confidence->assign(layers ? n : 0u, 0);
    if (report && layers) {
        report->layers_used = 1u;
        report->out_of_area_pixels = static_cast<uint32_t>(outsideN);
    }
    if (!hasDefects) {
        output->format        = XPE_PIXEL_FLOAT32;
        output->bitsAllocated = 32u;
        output->bitsStored    = 32u;
        output->dataSize      = n * sizeof(float);
        return XPE_OK;
    }

    // IN-PLACE IS SAFE BECAUSE READS AND WRITES NEVER TOUCH THE SAME PIXEL
    // -- QA-A-146 (#209).
    //
    // There used to be a second full-frame copy here
    // (`std::vector<float> source(src, src + n)`), kept in case a caller
    // aliased input and output: a write to dst would then be visible to a
    // later neighbour read through src, making the result scan-order
    // dependent. It cost 36 MB and 12.45 ms of an 18.45 ms call at 3072x3072
    // (QA-A-145) -- the largest single item in this function.
    //
    // The hazard it guarded cannot occur. This function WRITES only pixels
    // the defect map marks (`dm[idx] != 0`), and both correction kernels READ
    // only pixels it does not: median_filter_cluster takes a neighbour when
    // `defectMask[idx] == 0` (above, :96) and xpe_interpolate_pixel does the
    // same in try_add, on the 4-neighbour path and on the r=1..3 ring
    // fallback alike (helpers.cpp:30). The two sets are disjoint, so a read
    // can never see a corrected value -- aliased or not.
    //
    // In-place callers exist and must keep working: test_integration.cpp:100
    // and :131 call xpe_defect_correct(&buf.gainBuf, &buf.gainBuf, &buf.meta).
    // Searched every call site in the repository (all files, build/ and report
    // logs excluded); the pipeline stages into a separate vector
    // (pipeline.cpp:263) and the GUI passes distinct buffers
    // (GuiPreprocessRunner.cs:155).
    //
    // WHAT REPLACES THE COPY IS A TEST, NOT A COMMENT. The invariant above is
    // a property of the current kernels; a future kernel that reads a
    // defective neighbour would break in-place silently. DefectCorrectTest.
    // InPlaceMatchesOutOfPlace runs the same input both ways -- including a
    // solid 3x3 block that forces the ring fallback -- and compares
    // element-wise, so that change goes red instead of quiet.
    const float* const source = src;

    if (layers) {   // QA-A-255: the second edition: lines, blobs, imaging area
        XpeDefectCorrectionReport localReport{};
        std::vector<bool>         visitedL(maskSet.slots(), false);
        std::vector<uint8_t>      cstate(maskSet.slots(), 0);
        FillDistance              fdL;
        std::vector<float>        scratch;
        scratch.reserve(8u * static_cast<size_t>(kFillMaxRadius));
        std::vector<uint8_t>      lowLocal;
        std::vector<uint8_t>&     lowPlane = low_confidence ? *low_confidence : lowLocal;
        if (lowPlane.empty()) lowPlane.assign(n, 0);
        LayerPass pass{source, dst, &maskSet, layers, Grid{W, H}, &visitedL, &cstate, &fdL, &scratch, report ? report : &localReport, &lowPlane};
        markLowGroups(pass);
        correctLines(pass);
        correctBlobs(pass);
        if (!low_confidence) lowLocal.clear();
        const XpeDefectCorrectionReport& rr = report ? *report : localReport;
        if (rr.low_confidence_pixels > 0) {
            char msg[320];
            std::snprintf(msg, sizeof(msg),
                "XPE_WARN_DEFECT_LOW_CONFIDENCE: %u pixel(s) in %u region(s) were filled with low confidence (a line group of three or more lines, "
                "or a blob without an adjacent valid pixel)", rr.low_confidence_pixels, rr.low_confidence_regions);
            msg[sizeof(msg) - 1] = '\0';
            xpe_alert_push(msg, XPE_ALERT_WARNING);
        }
        if (rr.unfilled_pixels > 0) {
            char msg[320];
            std::snprintf(msg, sizeof(msg),
                "XPE_WARN_DEFECT_NO_VALID_NEIGHBOUR: %u masked pixel(s) have no valid pixel within %d pixels to fill them from and keep their input value",
                rr.unfilled_pixels, kFillMaxRadius);
            msg[sizeof(msg) - 1] = '\0';
            xpe_alert_push(msg, XPE_ALERT_WARNING);
        }
        output->format        = XPE_PIXEL_FLOAT32;
        output->bitsAllocated = 32u;
        output->bitsStored    = 32u;
        output->dataSize      = n * sizeof(float);
        return XPE_OK;
    }

    // REQ-P1A-012: cluster-aware defect correction.
    // QA-A-296 (Refs #278): the masked pixels are listed once; long runs are line defects and are interpolated across the line first
    // (marked in `done`); every other masked pixel is a cluster pixel when it has a masked, not-yet-done 4-neighbour (it was in a
    // 4-connected component of two or more) and an isolated pixel otherwise. A pixel's value depends only on valid pixels and on the
    // mask, never on another corrected pixel, so the pixels can be computed in any order and on several threads: the result is the one the
    // component-by-component walk gave. (That walk, a breadth-first search over each component, was 40-50 ms on a 368,710-pixel border band.)
    // QA-A-315 (Refs #278): a dense set is listed on several threads -- the plane is cut into kListParts fixed parts (not one a thread),
    // each part counted, the counts summed in part order and each part written at its offset -- so the list is the ascending one
    // forEach() gives, whatever the threads. The serial listing of a 368,710-pixel band took 1.0-2.9 ms (evidence 03, 06).
    MaskedList masked;
    masked.p.reset(new uint32_t[std::max<size_t>(1, maskSet.count())]);
    if (const uint8_t* const plane = maskSet.densePlane()) {
        constexpr size_t kListParts = 64;
        const size_t part = (((n + kListParts - 1) / kListParts) + 63) & ~size_t{63};
        std::vector<size_t> offset(kListParts + 1, 0);
        xpe_parallel_chunks(kListParts, 1, [&](size_t p0, size_t p1) {
            for (size_t p = p0; p < p1; ++p) {
                const size_t b = std::min(n, p * part), e = std::min(n, b + part);
                size_t c = 0;
                forEachNonZero(plane + b, e - b, [&](size_t) { ++c; });
                offset[p + 1] = c;
            }
        });
        for (size_t p = 0; p < kListParts; ++p) offset[p + 1] += offset[p];
        xpe_parallel_chunks(kListParts, 1, [&](size_t p0, size_t p1) {
            for (size_t p = p0; p < p1; ++p) {
                const size_t b = std::min(n, p * part), e = std::min(n, b + part);
                uint32_t* out = masked.p.get() + offset[p];
                forEachNonZero(plane + b, e - b, [&](size_t i) { *out++ = static_cast<uint32_t>(b + i); });
            }
        });
        masked.count = offset[kListParts];
    } else {
        maskSet.forEach([&](size_t idx) { masked.p[masked.count++] = static_cast<uint32_t>(idx); });
    }
    // By MaskSet slot: corrected by the line pass. QA-A-315 (Refs #278): allocated only when the line pass runs; without it no pixel is
    // done and the table would be slots() zero bytes (7.9 MB for a dense set at 3072x2560, ~1.3 ms to allocate, evidence 06).
    std::vector<uint8_t> done;
    const auto isDone = [&](size_t idx) { return !done.empty() && done[maskSet.slot(idx)] != 0; };

    {
        XpeDefectLineOptions lineOpts;
        if (line_options) lineOpts = *line_options; else { lineOpts.rows = false; lineOpts.cols = false; }   // nullptr = as before QA-A-296
        if (lineOpts.rows || lineOpts.cols) {
            done.assign(maskSet.slots(), 0);
            std::vector<LineRun> runs;
            classifyLineRuns(maskSet, Grid{W, H}, lineOpts, &runs);
            // rows first, then columns (a pixel of both is done by the row pass); the runs of one orientation share no pixel
            for (uint32_t vertical = 0; vertical < 2; ++vertical) {
                std::vector<const LineRun*> part;
                for (const LineRun& r : runs) if (r.vertical == vertical) part.push_back(&r);
                xpe_parallel_chunks(part.size(), 600, [&](size_t b, size_t e) {
                    for (size_t k = b; k < e; ++k) interpolateRun(source, dst, maskSet, Grid{W, H}, *part[k], done);
                });
            }
        }
    }

    FillDistance fillDistance;             // built on the first cluster pixel whose 3x3 holds no valid pixel (QA-A-213)
    // QA-A-315 (Refs #278): a dense mask set (the large border bands) builds the table here, on several threads, instead of on one thread
    // inside the loop below while the others wait; the call_once in median_filter_cluster then finds it built. The table is the same.
    if (maskSet.slots() == n && masked.size() >= 20000) {
        std::call_once(fillDistance.once, [&]() { fillDistance.buildDense(maskSet, Grid{W, H}, masked.data(), masked.size()); fillDistance.built.store(true, std::memory_order_release); });
    }
    std::atomic<size_t> unfilledCount{0};  // masked pixels with no valid pixel within kFillMaxRadius (QA-A-211b)
    // QA-A-315: the pixels are handed out in blocks of kFillBlock from a shared counter, not split into one fixed range a thread: the cost
    // of a pixel ranges from a 3x3 look to a ring of radius 16, and the fixed ranges took 11-21 ms on the same frame (evidence 04). Each
    // pixel is still computed from read-only data into its own place, so the assignment does not change the output. The thread count is
    // the one the fixed split used (masked.size() / 20000, at most XPE_PARALLEL_MAX_THREADS).
    constexpr size_t kFillBlock = 1024;
    std::atomic<size_t> nextFill{0};
    const size_t fillWorkers = std::max<size_t>(1, std::min<size_t>(XPE_PARALLEL_MAX_THREADS, masked.size() / 20000));
    xpe_parallel_chunks(fillWorkers, 1, [&](size_t, size_t) {
        std::vector<float> fillValues;     // scratch for median_filter_cluster, one per thread
        fillValues.reserve(8u * static_cast<size_t>(kFillMaxRadius));
        size_t localUnfilled = 0;
        for (;;) {
            const size_t b = nextFill.fetch_add(kFillBlock);
            if (b >= masked.size()) break;
            const size_t e = std::min(masked.size(), b + kFillBlock);
            for (size_t k = b; k < e; ++k) {
                const size_t idx = masked[k];
                if (isDone(idx)) continue;
                const uint32_t x = static_cast<uint32_t>(idx % W);
                const uint32_t y = static_cast<uint32_t>(idx / W);
                bool cluster = false;
                if (x > 0)     { const size_t n1 = idx - 1; cluster = cluster || (maskSet.test(n1) && !isDone(n1)); }
                if (x + 1 < W) { const size_t n1 = idx + 1; cluster = cluster || (maskSet.test(n1) && !isDone(n1)); }
                if (y > 0)     { const size_t n1 = idx - W; cluster = cluster || (maskSet.test(n1) && !isDone(n1)); }
                if (y + 1 < H) { const size_t n1 = idx + W; cluster = cluster || (maskSet.test(n1) && !isDone(n1)); }
                if (cluster) {
                    bool found = true;
                    dst[idx] = median_filter_cluster(source, maskSet, x, y, W, H, fillDistance, fillValues, &found, k);
                    if (!found) ++localUnfilled;
                } else {
                    dst[idx] = xpe_interpolate_pixel_masked(source, [&](size_t i) { return maskSet.test(i); }, x, y, W, H);
                }
            }
        }
        unfilledCount.fetch_add(localUnfilled);
    });
    const size_t unfilled = unfilledCount.load();

    // QA-A-211b: a masked pixel with no valid pixel within kFillMaxRadius was left as it came in. That is a statement of
    // fact the caller needs, not a correction -- one alert for the frame, with the count.
    if (unfilled > 0) {
        char msg[320];
        std::snprintf(msg, sizeof(msg),
            "XPE_WARN_DEFECT_NO_VALID_NEIGHBOUR: %zu masked pixel(s) have no valid pixel within %d pixels to fill them from and "
            "keep their input value",
            unfilled, kFillMaxRadius);
        msg[sizeof(msg) - 1] = '\0';
        xpe_alert_push(msg, XPE_ALERT_WARNING);
    }

    output->format        = XPE_PIXEL_FLOAT32;
    output->bitsAllocated = 32u;
    output->bitsStored    = 32u;
    output->dataSize      = n * sizeof(float);
    return XPE_OK;
}
catch (const std::bad_alloc&) {
    // QA-A-202 (#233): an exception must not leave a C ABI function. The lock guard is a local of the
    // try block, so it is released before this handler runs.
    return XPE_ERR_OUT_OF_MEMORY;
} catch (...) {
    return XPE_ERR_PROCESSING_FAILED;
}

extern "C" XPE_API XpeErrorCode xpe_defect_correct_ex(
    const XpeImageBuffer*   input,
    XpeImageBuffer*         output,
    const XpeImageMetadata* metadata,
    XpeDefectCorrectionReport* report,
    XpeImageBuffer*         low_confidence_out)
{
    // QA-A-255: xpe_defect_correct with a report. The same snapshot and expiry rules; see xpe_defect_correct for the try region.
    try {
        if (low_confidence_out) {
            if (!input || !low_confidence_out->data) return XPE_ERR_INVALID_INPUT;
            if (low_confidence_out->format != XPE_PIXEL_UINT8) return XPE_ERR_UNSUPPORTED_FORMAT;
            if (low_confidence_out->width != input->width || low_confidence_out->height != input->height ||
                low_confidence_out->dataSize < static_cast<size_t>(input->width) * input->height) return XPE_ERR_BUFFER_TOO_SMALL;
        }
        const CalibSnapshot calib = xpe_calib_snapshot();
        const XpeErrorCode expiry = xpe_calib_snapshot_expiry_check(
            calib, XPE_EXPIRY_DEFECT | (calib.gain_defect_count > 0 ? XPE_EXPIRY_GAIN : 0u));
        if (expiry != XPE_OK) return expiry;
        std::vector<uint8_t> low;
        const XpeErrorCode rc = xpe_defect_correct_in(calib, input, output, metadata, nullptr, report, low_confidence_out ? &low : nullptr);
        if (rc == XPE_OK && low_confidence_out) {
            const size_t n = static_cast<size_t>(input->width) * input->height;
            if (low.size() == n) std::memcpy(low_confidence_out->data, low.data(), n);
            else std::memset(low_confidence_out->data, 0, n);
        }
        return rc;
    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

// Runtime detection implementation moved to runtime_detection.cpp (REQ-P1A-013)
// Uses Hampel 5-sigma outlier detection instead of mean+3sigma

extern "C" XPE_API XpeErrorCode xpe_defect_correct(
    const XpeImageBuffer*  input,
    XpeImageBuffer*         output,
    const XpeImageMetadata* metadata)
{
    // A single-stage call uses the maps current when it is called: a snapshot of its own, taken here
    // (QA-A-202d). The set consistency across the stages of a frame is the pipeline's.
    //
    // The try region is not decoration: under /EHsc a C-linkage function with no try region has no unwind
    // information, so if xpe_defect_correct_in threw, the snapshot temporary (shared_ptr members) would not be
    // destroyed and the maps it holds would leak. xpe_defect_correct_in has C++ linkage, so this handler is kept.
    try {
        const CalibSnapshot calib = xpe_calib_snapshot();
        // SRS-CALIB-SAFE-002 (Codex #157): a map that expired after it was loaded must not correct anything, here either. The stage
        // also fills the pixels the gain map classified defective, so a gain map that carries such a list is judged too.
        const XpeErrorCode expiry = xpe_calib_snapshot_expiry_check(
            calib, XPE_EXPIRY_DEFECT | (calib.gain_defect_count > 0 ? XPE_EXPIRY_GAIN : 0u));
        if (expiry != XPE_OK) return expiry;
        return xpe_defect_correct_in(calib, input, output, metadata);
    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
