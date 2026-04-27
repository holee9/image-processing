/**
 * @file defect_correct.cpp
 * @brief SWU-1.3: Defect pixel correction and runtime detection (PRE-06)
 *        REQ-P1A-012 (Defect Correction Bilinear + Cluster)
 * SPEC: SPEC-XPE-P1A v1.2.0  IEC 62304 Class B
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include <mutex>

#include <cmath>
#include <cstring>
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

ClusterInfo analyzeCluster(const uint8_t* defectMask, uint32_t width, uint32_t height,
                           uint32_t startX, uint32_t startY)
{
    ClusterInfo info;
    std::vector<bool> visited(width * height, false);
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

    if (values.empty()) return 0.0f;

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
    const XpeImageMetadata* metadata)
{
    if (!input || !output || !metadata) return XPE_ERR_INVALID_INPUT;
    if (!input->data || !output->data) return XPE_ERR_INVALID_INPUT;
    if (input->format != XPE_PIXEL_FLOAT32) return XPE_ERR_UNSUPPORTED_FORMAT;
    if (input->width == 0 || input->height == 0) return XPE_ERR_INVALID_INPUT;
    if (output->width  != input->width ||
        output->height != input->height) return XPE_ERR_BUFFER_TOO_SMALL;

    const size_t n = static_cast<size_t>(input->width) * input->height;
    if (output->dataSize < n * sizeof(float)) return XPE_ERR_BUFFER_TOO_SMALL;

    std::unique_lock<std::mutex> lock(g_calib_mutex);
    if (!g_calib.defect_map) return XPE_ERR_NOT_INITIALIZED;
    if (g_calib.defect_width  != input->width ||
        g_calib.defect_height != input->height) return XPE_ERR_BUFFER_TOO_SMALL;

    // Copy defect map locally so we can release the mutex before heavy processing
    std::vector<uint8_t> dm_local(g_calib.defect_map.get(),
                                   g_calib.defect_map.get() + n);
    lock.unlock();

    const uint32_t W  = input->width;
    const uint32_t H  = input->height;
    const float*   src = static_cast<const float*>(input->data);
    float*         dst = static_cast<float*>(output->data);
    const uint8_t* dm  = dm_local.data();

    // Copy input → output first
    std::memcpy(dst, src, n * sizeof(float));

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

    // Use a source snapshot for neighbor lookups during correction
    std::vector<float> source(src, src + n);

    // REQ-P1A-012: cluster-aware defect correction
    std::vector<bool> processed(n, false);
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            uint32_t idx = y * W + x;
            if (dm[idx] != 0 && !processed[idx]) {
                ClusterInfo cluster = analyzeCluster(dm, W, H, x, y);
                if (cluster.isCluster) {
                    for (uint32_t cidx : cluster.positions) {
                        uint32_t cx = cidx % W;
                        uint32_t cy = cidx / W;
                        dst[cidx] = median_filter_cluster(source.data(), dm, cx, cy, W, H);
                        processed[cidx] = true;
                    }
                } else {
                    dst[idx] = xpe_interpolate_pixel(source.data(), dm, x, y, W, H);
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

// Runtime detection implementation moved to runtime_detection.cpp (REQ-P1A-013)
// Uses Hampel 5-sigma outlier detection instead of mean+3sigma
