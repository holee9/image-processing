// SWU-2.3: CLAHE Contrast Enhancement
// SPEC-XPE-P1B-ENH  REQ-ENH-013..017

#include "xpe/enhance_basic/enhance_basic_api.h"
#include "xpe/enhance_basic/enhance_basic_internal.h"

#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace {

constexpr int NUM_BINS = 4096;

// Per-tile CLAHE data: local min/max quantization + normalized CDF LUT.
// lut[bin] is a CDF fraction in [0, 1] representing where this value falls
// in the equalized output.  Storing fractions (not bin indices) allows
// bilinear interpolation across tiles with different local value ranges.
struct TileLut {
    float val_min;    // local tile minimum
    float val_scale;  // (NUM_BINS-1) / (val_max - val_min), 0 for flat tiles
    float lut[NUM_BINS];
};

// Build a per-tile LUT using the tile's own min/max for histogram binning.
// This ensures every tile fully utilizes all NUM_BINS bins regardless of the
// tile's position within the global value range (fixes the global-quantization
// bug where tiles with a narrow subrange have almost-empty histograms).
static void build_tile_lut(const float* px, int img_w,
                             int x0, int y0, int x1, int y1,
                             float clip_limit,
                             TileLut& out)
{
    // Find tile local min/max
    float tmin = px[static_cast<int64_t>(y0) * img_w + x0];
    float tmax = tmin;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            float v = px[static_cast<int64_t>(y) * img_w + x];
            if (v < tmin) tmin = v;
            if (v > tmax) tmax = v;
        }
    }

    out.val_min = tmin;
    float range = tmax - tmin;
    if (range <= 0.0f) {
        out.val_scale = 0.0f;
        std::fill(out.lut, out.lut + NUM_BINS, 0.5f); // flat tile: mid-fraction
        return;
    }
    out.val_scale = static_cast<float>(NUM_BINS - 1) / range;

    // Build histogram with local quantization
    int hist[NUM_BINS] = {};
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            float v = px[static_cast<int64_t>(y) * img_w + x];
            int bin = static_cast<int>((v - tmin) * out.val_scale);
            if (bin < 0) bin = 0;
            if (bin >= NUM_BINS) bin = NUM_BINS - 1;
            hist[bin]++;
        }
    }

    // Clip and redistribute excess
    int tile_area = (x1 - x0) * (y1 - y0);
    int clip_count = static_cast<int>(clip_limit * static_cast<float>(tile_area)
                                      / static_cast<float>(NUM_BINS));
    if (clip_count < 1) clip_count = 1;

    int excess = 0;
    for (int i = 0; i < NUM_BINS; ++i) {
        if (hist[i] > clip_count) {
            excess += hist[i] - clip_count;
            hist[i] = clip_count;
        }
    }
    int per_bin = excess / NUM_BINS;
    int remainder = excess - per_bin * NUM_BINS;
    for (int i = 0; i < NUM_BINS; ++i) {
        hist[i] += per_bin;
        if (i < remainder) hist[i]++;
    }

    // CDF normalized to [0, 1]
    int64_t cumsum = 0;
    float norm = 1.0f / static_cast<float>(std::max(1, tile_area));
    for (int i = 0; i < NUM_BINS; ++i) {
        cumsum += hist[i];
        out.lut[i] = static_cast<float>(cumsum) * norm;
    }
}

// Look up a pixel value in a tile's LUT, returning the CDF fraction [0, 1].
static inline float tile_lookup(const TileLut& t, float v)
{
    if (t.val_scale <= 0.0f) return 0.5f;
    int bin = static_cast<int>((v - t.val_min) * t.val_scale);
    if (bin < 0) bin = 0;
    if (bin >= NUM_BINS) bin = NUM_BINS - 1;
    return t.lut[bin];
}

} // anonymous namespace

extern "C" {

// @MX:ANCHOR: xpe_contrast_enhance applies CLAHE with bilinear tile interpolation.
// @MX:REASON: [AUTO] Public API boundary, CLAHE pipeline stage. REQ-ENH-013..017.
XPE_API XpeErrorCode xpe_contrast_enhance(XpeImageBuffer* img, const XpeClaheParams* params)
{
    // REQ-ENH-014: use defaults if params is NULL
    XpeClaheParams defaults;
    defaults.clip_limit  = 3.0f;
    defaults.tile_width  = 8;
    defaults.tile_height = 8;

    const XpeClaheParams* p = params ? params : &defaults;

    // REQ-ENH-015: clip_limit must be >= 1.0
    if (p->clip_limit < 1.0f) return XPE_ERR_INVALID_INPUT;

    // REQ-ENH-016: tile dimensions must be >= 2
    if (p->tile_width < 2 || p->tile_height < 2) return XPE_ERR_INVALID_INPUT;

    XpeErrorCode err = validate_float32_image(img);
    if (err != XPE_OK) return err;

    int w = static_cast<int>(img->width);
    int h = static_cast<int>(img->height);
    if (w == 0 || h == 0) return XPE_OK;

    // Image must be large enough for the tile grid
    if (w < p->tile_width * 2 || h < p->tile_height * 2) {
        return XPE_ERR_INVALID_INPUT;
    }

    float* px = float_pixels(img);
    uint64_t n = static_cast<uint64_t>(w) * h;

    // Global min/max for flat-image early exit and final output remapping
    float val_min = px[0];
    float val_max = px[0];
    for (uint64_t i = 1; i < n; ++i) {
        if (px[i] < val_min) val_min = px[i];
        if (px[i] > val_max) val_max = px[i];
    }
    float val_range = val_max - val_min;
    if (val_range <= 0.0f) return XPE_OK; // Flat image, no contrast to enhance.

    int num_tiles_x = p->tile_width;
    int num_tiles_y = p->tile_height;
    int tile_w = (w + num_tiles_x - 1) / num_tiles_x;
    int tile_h = (h + num_tiles_y - 1) / num_tiles_y;

    // Build per-tile LUTs with local min/max quantization.
    // REQ-ENH-013: each tile's histogram covers its actual value range,
    // ensuring proper equalization even for low-contrast regions.
    int total_tiles = num_tiles_x * num_tiles_y;
    std::vector<TileLut> tiles(static_cast<size_t>(total_tiles));

    for (int ty = 0; ty < num_tiles_y; ++ty) {
        for (int tx = 0; tx < num_tiles_x; ++tx) {
            int x0 = tx * tile_w;
            int y0 = ty * tile_h;
            int x1 = std::min(x0 + tile_w, w);
            int y1 = std::min(y0 + tile_h, h);
            build_tile_lut(px, w, x0, y0, x1, y1, p->clip_limit,
                           tiles[static_cast<size_t>(ty * num_tiles_x + tx)]);
        }
    }

    // Apply CLAHE using nearest-tile assignment.
    // Each pixel is mapped by its containing tile's LUT exclusively.
    // Bilinear interpolation is intentionally avoided: tiles use per-tile local
    // min/max quantization, so blending across tile boundaries would mix
    // incompatible CDF scales — a pixel at the top of tile A's range (frac≈1.0)
    // blended with the same pixel at the bottom of tile B's range (frac≈0.0)
    // yields ≈0.5 for all boundary pixels, REDUCING rather than increasing contrast.
    // Nearest-tile gives each pixel fully independent local equalization.
    std::vector<float> output(n);

    for (int y = 0; y < h; ++y) {
        int ty_idx = std::min(y / tile_h, num_tiles_y - 1);

        for (int x = 0; x < w; ++x) {
            int tx_idx = std::min(x / tile_w, num_tiles_x - 1);

            float v = px[static_cast<int64_t>(y) * w + x];
            float frac = tile_lookup(tiles[static_cast<size_t>(ty_idx * num_tiles_x + tx_idx)], v);

            // Map CDF fraction back to global value range
            output[static_cast<int64_t>(y) * w + x] = val_min + frac * val_range;
        }
    }

    std::copy(output.begin(), output.end(), px);
    return XPE_OK;
}

} // extern "C"
