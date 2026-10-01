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
    // QA-B-181c (Codex #37): a tile that starts at or past the image edge is empty. The first read below used to
    // happen before any range check, one pixel past the buffer for such a tile. No pixel is ever assigned to an
    // empty tile (xpe_tile_of picks a tile that holds the pixel), so its LUT is never consulted; it still gets a
    // defined neutral one.
    if (x0 >= x1 || y0 >= y1) {
        out.val_min = 0.0f;
        out.val_scale = 0.0f;
        std::fill(out.lut, out.lut + NUM_BINS, 0.5f);
        return;
    }

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
    // QA-B-181d (Codex #48): limited before the conversion. clip_limit is finite (checked by the caller) but can be
    // huge, and the product leaves int; a clip at or above the tile area never clips (no bin holds more pixels
    // than the tile has), so the cap changes no result.
    const float clipF = clip_limit * static_cast<float>(tile_area) / static_cast<float>(NUM_BINS);
    int clip_count = clipF >= static_cast<float>(tile_area) ? tile_area : static_cast<int>(clipF);
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

// QA-B-181 (QA-B-179, #233): no exception may leave an exported function. The body lives in an `extern "C++"`
// function (a helper declared inside this extern "C" block would get C linkage and the "never throws" treatment,
// which optimises the catch away -- see modules/ai/src/ai.cpp for the measurement) and the exported function is a
// try/catch around it. The handlers allocate nothing.
extern "C++" static XpeErrorCode xpe_contrast_enhance_impl(XpeImageBuffer* img, const XpeClaheParams* params);


// @MX:ANCHOR: xpe_contrast_enhance applies CLAHE with bilinear tile interpolation.
// @MX:REASON: [AUTO] Public API boundary, CLAHE pipeline stage. REQ-ENH-013..017.
XPE_API XpeErrorCode xpe_contrast_enhance(XpeImageBuffer* img, const XpeClaheParams* params)
{
    return xpe_guarded_call([&] { return xpe_contrast_enhance_impl(img, params); });
}

extern "C++" static XpeErrorCode xpe_contrast_enhance_impl(XpeImageBuffer* img, const XpeClaheParams* params)
{
    // REQ-ENH-014: use defaults if params is NULL
    XpeClaheParams defaults;
    defaults.clip_limit  = 3.0f;
    defaults.tile_width  = 8;
    defaults.tile_height = 8;

    const XpeClaheParams* p = params ? params : &defaults;

    // REQ-ENH-015: clip_limit must be >= 1.0
    // QA-B-181d: finite as well -- see the note in noise_reduce.cpp on NaN and range comparisons.
    if (!std::isfinite(p->clip_limit) || p->clip_limit < 1.0f) return XPE_ERR_INVALID_INPUT;

    // REQ-ENH-016: tile dimensions must be >= 2
    if (p->tile_width < 2 || p->tile_height < 2) return XPE_ERR_INVALID_INPUT;

    XpeErrorCode err = validate_float32_image(img);
    if (err != XPE_OK) return err;

    int w = static_cast<int>(img->width);
    int h = static_cast<int>(img->height);

    // Image must be large enough for the tile grid
    // QA-B-181: judged in 64 bits. `tile_width * 2` in int overflowed for tile_width >= 2^30 (undefined behaviour:
    // 0x40000000 wrapped negative, passed this check, and the tile table below was then sized from the wrapped
    // product), so a window that cannot fit the image was admitted instead of rejected.
    if (static_cast<int64_t>(w) < static_cast<int64_t>(p->tile_width) * 2 ||
        static_cast<int64_t>(h) < static_cast<int64_t>(p->tile_height) * 2) {
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
    // QA-B-181b (Codex #35): divide first. `(w + num_tiles_x - 1) / num_tiles_x` leaves int when w is near INT32_MAX
    // (w = INT32_MAX, tile_width = 2 passes the window check above): undefined behaviour no guard can catch.
    int tile_w = xpe_ceil_div(w, num_tiles_x);
    int tile_h = xpe_ceil_div(h, num_tiles_y);

    // Build per-tile LUTs with local min/max quantization.
    // REQ-ENH-013: each tile's histogram covers its actual value range,
    // ensuring proper equalization even for low-contrast regions.
    // 64-bit: after the window check each factor is at most 2^30, so the product can reach 2^60.
    const size_t total_tiles = static_cast<size_t>(num_tiles_x) * static_cast<size_t>(num_tiles_y);
    std::vector<TileLut> tiles(total_tiles);

    for (int ty = 0; ty < num_tiles_y; ++ty) {
        for (int tx = 0; tx < num_tiles_x; ++tx) {
            // 64-bit and clamped inside xpe_tile_bounds; a tile past the edge comes out empty.
            int x0, x1, y0, y1;
            xpe_tile_bounds(tx, tile_w, w, x0, x1);
            xpe_tile_bounds(ty, tile_h, h, y0, y1);
            build_tile_lut(px, w, x0, y0, x1, y1, p->clip_limit,
                           tiles[static_cast<size_t>(ty) * static_cast<size_t>(num_tiles_x) + static_cast<size_t>(tx)]);
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
        int ty_idx = xpe_tile_of(y, tile_h, num_tiles_y);

        for (int x = 0; x < w; ++x) {
            int tx_idx = xpe_tile_of(x, tile_w, num_tiles_x);

            float v = px[static_cast<int64_t>(y) * w + x];
            float frac = tile_lookup(tiles[static_cast<size_t>(ty_idx) * static_cast<size_t>(num_tiles_x) +
                                           static_cast<size_t>(tx_idx)], v);

            // Map CDF fraction back to global value range
            output[static_cast<int64_t>(y) * w + x] = val_min + frac * val_range;
        }
    }

    std::copy(output.begin(), output.end(), px);
    return XPE_OK;
}

} // extern "C"
