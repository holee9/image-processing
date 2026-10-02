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

// QA-A-211b (#233, Codex #71): how far a masked pixel looks for a valid pixel to fill from. The pixels of a defect
// cluster (and of a blob of gain-classified pixels) can have no valid pixel in their 3x3 -- on CalData_6, 66.7% of the
// defect map's own pixels and 72.5% of defect-map-or-classified ones, at most 10 pixels from the nearest valid one
// (QA-A-211b evidence, 10_distance_to_valid.txt). They used to be filled with 0.0f. 16 is the measured 10 with margin;
// beyond it the pixel keeps its input value and the frame says so (see the caller).
constexpr int kFillMaxRadius = 16;

// @MX:NOTE: [AUTO] 3x3 median filter for defect cluster correction
// Collects valid neighbor pixels and returns median value. When the 3x3 holds none (the interior of a cluster), the
// search widens ring by ring (Chebyshev radius 2, 3, ... kFillMaxRadius) and the median of the NEAREST ring that holds a
// valid pixel is used -- the same nearest-valid-pixel idea the single-pixel path applies in a ring of radius 1..3. A
// pixel with no valid pixel within kFillMaxRadius keeps its own input value (`found` false), never 0.
// Only unmasked pixels are read, whatever the radius: the in-place guarantee of xpe_defect_correct_in holds.
float median_filter_cluster(const float* pixels, const uint8_t* defectMask,
                             uint32_t x, uint32_t y,
                             uint32_t width, uint32_t height, bool* found)
{
    std::vector<float> values;
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
                if (defectMask[idx] == 0) { // valid pixel
                    values.push_back(pixels[idx]);
                }
            }
        }
    }

    for (int radius = 2; values.empty() && radius <= kFillMaxRadius; ++radius) {
        for (int dy = -radius; dy <= radius; ++dy) {
            const int ny = static_cast<int>(y) + dy;
            if (ny < 0 || static_cast<uint32_t>(ny) >= height) continue;
            const bool edgeRow = (dy == -radius || dy == radius);
            for (int dx = -radius; dx <= radius; dx += (edgeRow ? 1 : 2 * radius)) {
                const int nx = static_cast<int>(x) + dx;
                if (nx < 0 || static_cast<uint32_t>(nx) >= width) continue;
                const size_t idx = static_cast<size_t>(ny) * width + static_cast<uint32_t>(nx);
                if (defectMask[idx] == 0) values.push_back(pixels[idx]);
            }
        }
    }

    if (values.empty()) {
        *found = false;
        return pixels[static_cast<size_t>(y) * width + x];
    }

    std::sort(values.begin(), values.end());
    size_t mid = values.size() / 2u;
    return values[mid];
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
    const std::vector<uint32_t>* frame_defects) try
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
    std::vector<uint8_t> union_mask;
    const size_t frame_extra = frame_defects ? frame_defects->size() : 0;
    if (calib.gain_defect_count > 0 || frame_extra > 0) {
        union_mask.assign(dm, dm + n);
        if (calib.gain_defect_idx) {
            for (uint32_t k = 0; k < calib.gain_defect_count; ++k) {
                const uint32_t idx = calib.gain_defect_idx[k];
                if (idx < n) union_mask[idx] = 1;
            }
        }
        if (frame_defects) {
            for (const uint32_t idx : *frame_defects) {
                if (idx < n) union_mask[idx] = 1;
            }
        }
        dm = union_mask.data();

        // D1: the union is above the density SRS-CALIB-FUNC-003 tolerates -> ONE warning for the frame, never a refusal.
        size_t u = 0;
        for (size_t i = 0; i < n; ++i) u += (union_mask[i] != 0);
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
    size_t unfilled = 0;                   // masked pixels with no valid pixel within kFillMaxRadius (QA-A-211b)
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            uint32_t idx = y * W + x;
            if (dm[idx] != 0 && !processed[idx]) {
                ClusterInfo cluster = analyzeCluster(dm, W, H, x, y, visited);
                if (cluster.isCluster) {
                    for (uint32_t cidx : cluster.positions) {
                        uint32_t cx = cidx % W;
                        uint32_t cy = cidx / W;
                        bool found = true;
                        dst[cidx] = median_filter_cluster(source, dm, cx, cy, W, H, &found);
                        if (!found) ++unfilled;
                        processed[cidx] = true;
                    }
                } else {
                    dst[idx] = xpe_interpolate_pixel(source, dm, x, y, W, H);
                    processed[idx] = true;
                }
            }
        }
    }

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
        return xpe_defect_correct_in(xpe_calib_snapshot(), input, output, metadata);
    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
