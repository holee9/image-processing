/**
 * @file defect_correct.cpp
 * @brief SWU-1.3: Defect pixel correction and runtime detection (PRE-06)
 *        REQ-P1A-012 (Defect Correction Bilinear + Cluster)
 * SPEC: SPEC-XPE-P1A v1.2.0  IEC 62304 Class B
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include <memory>
#include <mutex>
#include <new>

#include <cmath>
#include <cstring>
#include <limits>
#include <vector>
#include <algorithm>
#include <queue>

namespace {

// @MX:NOTE: [AUTO] Connected component analysis for defect cluster detection
// Uses BFS to find adjacent defective pixels (4-connectivity)
struct ClusterInfo {
    std::vector<uint32_t> positions; // defect pixel positions
    bool isCluster; // true if 2+ adjacent defects
};

// `visited` is W*H and all-false on entry; it is left all-false on return.
// QA-A-103 (#179): it used to be allocated (W*H) per defect pixel, which made
// the correction O(defects x W*H) -- 2.97 s for 0.1 % defects at 3072x3072.
// Every pixel marked here is pushed to `positions`, so clearing exactly those
// entries restores the invariant.
ClusterInfo analyzeCluster(const uint8_t* defectMask, uint32_t width, uint32_t height,
                           uint32_t startX, uint32_t startY,
                           std::vector<bool>& visited)
{
    ClusterInfo info;
    std::queue<uint32_t> q;

    uint32_t startIdx = startY * width + startX;
    q.push(startIdx);
    visited[startIdx] = true;

    const int dx[] = {-1, 1, 0, 0};
    const int dy[] = {0, 0, -1, 1};

    while (!q.empty()) {
        uint32_t idx = q.front();
        q.pop();
        info.positions.push_back(idx);

        uint32_t cx = idx % width;
        uint32_t cy = idx / width;

        for (int i = 0; i < 4; ++i) {
            int nx = static_cast<int>(cx) + dx[i];
            int ny = static_cast<int>(cy) + dy[i];

            if (nx >= 0 && ny >= 0 &&
                static_cast<uint32_t>(nx) < width &&
                static_cast<uint32_t>(ny) < height) {
                uint32_t nidx = static_cast<uint32_t>(ny) * width + static_cast<uint32_t>(nx);
                if (!visited[nidx] && defectMask[nidx] != 0) {
                    visited[nidx] = true;
                    q.push(nidx);
                }
            }
        }
    }

    for (uint32_t idx : info.positions) visited[idx] = false;

    info.isCluster = info.positions.size() >= 2u;
    return info;
}

// @MX:NOTE: [AUTO] 3x3 median filter for defect cluster correction
// Collects valid neighbor pixels and returns median value
float median_filter_cluster(const float* pixels, const uint8_t* defectMask,
                             uint32_t x, uint32_t y,
                             uint32_t width, uint32_t height)
{
    std::vector<float> values;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;

            int nx = static_cast<int>(x) + dx;
            int ny = static_cast<int>(y) + dy;

            if (nx >= 0 && ny >= 0 &&
                static_cast<uint32_t>(nx) < width &&
                static_cast<uint32_t>(ny) < height) {
                uint32_t idx = static_cast<uint32_t>(ny) * width + static_cast<uint32_t>(nx);
                if (defectMask[idx] == 0) { // valid pixel
                    values.push_back(pixels[idx]);
                }
            }
        }
    }

    if (values.empty()) {
        return 0.0f;
    }

    std::sort(values.begin(), values.end());
    size_t mid = values.size() / 2u;
    return values[mid];
}

} // anonymous namespace

// @MX:ANCHOR: [AUTO] xpe_defect_correct — public API entry point (new g_calib-based)
// @MX:REASON: Called after gain correction; reads g_calib.defect_map; fan_in >= 3
// @MX:SPEC: REQ-P1A-012, REQ-P1A-020
extern "C" XPE_API XpeErrorCode xpe_defect_correct(
    const XpeImageBuffer*  input,
    XpeImageBuffer*         output,
    const XpeImageMetadata* metadata) try
{
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

    std::unique_lock<std::mutex> lock(g_calib_mutex);
    // SPEC-XPE-P1A REQ-P1A-020: while the module is not initialized, every
    // processing function returns XPE_ERR_NOT_INITIALIZED. Checked explicitly --
    // before #117 decision B the missing calibration map stood in for this, which
    // is why the two states could not be told apart.
    if (!xpe_preprocess_is_initialized()) return XPE_ERR_NOT_INITIALIZED;

    // #117 decision B: the module is initialized -- what is missing is the
    // calibration map. XPE_ERR_NOT_INITIALIZED is reserved for
    // xpe_preprocess_init() not called / after shutdown (SPEC-XPE-P1A
    // REQ-P1A-020), so the caller can tell the two apart.
    if (!g_calib.defect_map) return XPE_ERR_CALIB_NOT_LOADED;
    if (g_calib.defect_width  != input->width ||
        g_calib.defect_height != input->height) return XPE_ERR_INVALID_INPUT;

    // QA-A-202 (#233): take shared ownership of the map and release the mutex before the heavy work. The
    // old code copied the whole map (9.4 MB at 3072x3072) while holding the lock; a reload during the
    // frame now replaces the pointer in the store and leaves this frame the map it started with.
    const std::shared_ptr<uint8_t[]> dm_local = g_calib.defect_map;
    lock.unlock();

    const uint32_t W  = input->width;
    const uint32_t H  = input->height;
    const float*   src = static_cast<const float*>(input->data);
    float*         dst = static_cast<float*>(output->data);
    const uint8_t* dm  = dm_local.get();

    // Copy input -> output first. SKIPPED WHEN THE CALLER PASSED ONE BUFFER:
    // std::memcpy requires non-overlapping regions, so dst == src is undefined
    // behaviour even though it happens to work on this toolchain. In-place is
    // a documented, supported call shape as of QA-A-146 (#209) -- the contract
    // must not rest on UB. The copy is also pure waste there.
    if (dst != src) {
        std::memcpy(dst, src, n * sizeof(float));
    }

    bool hasDefects = false;
    for (size_t i = 0; i < n; ++i) {
        if (dm[i] != 0) { hasDefects = true; break; }
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

    // REQ-P1A-012: cluster-aware defect correction
    std::vector<bool> processed(n, false);
    std::vector<bool> visited(n, false);   // reused by every analyzeCluster call
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            uint32_t idx = y * W + x;
            if (dm[idx] != 0 && !processed[idx]) {
                ClusterInfo cluster = analyzeCluster(dm, W, H, x, y, visited);
                if (cluster.isCluster) {
                    for (uint32_t cidx : cluster.positions) {
                        uint32_t cx = cidx % W;
                        uint32_t cy = cidx / W;
                        dst[cidx] = median_filter_cluster(source, dm, cx, cy, W, H);
                        processed[cidx] = true;
                    }
                } else {
                    dst[idx] = xpe_interpolate_pixel(source, dm, x, y, W, H);
                    processed[idx] = true;
                }
            }
        }
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

// Runtime detection implementation moved to runtime_detection.cpp (REQ-P1A-013)
// Uses Hampel 5-sigma outlier detection instead of mean+3sigma
