// SWU-2.3: CLAHE Contrast Enhancement
// SPEC-XPE-P1B-ENH  REQ-ENH-013..017

#include "xpe/enhance_basic/enhance_basic_api.h"
#include "xpe/enhance_basic/enhance_basic_internal.h"

#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace {

constexpr int NUM_BINS = 4096;

// QA-B-207 M2 (E6, user decision on #251): the standard definition of CLAHE (Zuiderveld, Graphics Gems IV).
//
//   1. ONE intensity scale for the whole image: the global minimum and maximum cut the value range into NUM_BINS bins.
//   2. Per tile: the histogram on that scale, clipped at `clip_limit * tile_area / NUM_BINS` with the excess spread over
//      all bins, then its cumulative distribution -- a table `bin -> fraction in [0, 1]`.
//   3. Per pixel: the fractions of the tables of the (up to) four tiles whose CENTRES surround the pixel, blended
//      bilinearly by the pixel's distance to those centres. A pixel beside the border sees two tiles, one in a corner.
//   4. The blended fraction is mapped back onto the global value range.
//
// What this replaces: every tile used its OWN minimum and maximum as its scale and each pixel took the table of the tile
// that contains it. The tables of neighbouring tiles then described different value scales, so the output stepped at every
// tile boundary (QA-B-204 E6: on a smooth ramp the jump across a boundary was 33 times the jump elsewhere). The old code
// avoided interpolation for that reason; with one shared scale the tables are comparable and interpolation is correct,
// which is also what removes the staircase.
//
// A tile whose pixels span only a few bins now has a sparse histogram -- that is the standard behaviour (the clip limit is
// what bounds the contrast a tile may add) and it is why a low-contrast tile is no longer stretched to the full range on
// its own.

// The tiles on one axis that hold at least one pixel, with the position of their centres.
struct Axis {
    std::vector<int>   tile;     // original tile index (into the tile_count grid) of each non-empty tile
    std::vector<float> centre;   // centre of that tile in pixel coordinates (pixel i is centred on i)
    std::vector<int>   lo;       // per pixel position: the non-empty tile (index into `tile`) at the lower side
    std::vector<int>   hi;       // ... and at the upper side
    std::vector<float> weight;   // per pixel position: weight of the upper tile, 0..1
};

static void build_axis(int size, int tile_size, int tile_count, Axis& a)
{
    for (int t = 0; t < tile_count; ++t) {
        int s, e;
        xpe_tile_bounds(t, tile_size, size, s, e);
        if (s >= e) continue;   // a tile past the edge is empty and has no centre (ceil-division can leave one)
        a.tile.push_back(t);
        a.centre.push_back(0.5f * static_cast<float>(s + e - 1));
    }
    const int n = static_cast<int>(a.tile.size());
    a.lo.resize(static_cast<size_t>(size));
    a.hi.resize(static_cast<size_t>(size));
    a.weight.resize(static_cast<size_t>(size));
    int k = 0;   // the lower tile of the current position; positions ascend
    for (int p = 0; p < size; ++p) {
        const float fp = static_cast<float>(p);
        while (k + 1 < n && a.centre[static_cast<size_t>(k + 1)] <= fp) ++k;
        if (fp <= a.centre[0]) {                                   // before the first centre: that tile alone
            a.lo[static_cast<size_t>(p)] = a.hi[static_cast<size_t>(p)] = 0;
            a.weight[static_cast<size_t>(p)] = 0.0f;
        } else if (k + 1 >= n) {                                   // at or after the last centre: that tile alone
            a.lo[static_cast<size_t>(p)] = a.hi[static_cast<size_t>(p)] = n - 1;
            a.weight[static_cast<size_t>(p)] = 0.0f;
        } else {                                                   // between two centres
            const float c0 = a.centre[static_cast<size_t>(k)];
            const float c1 = a.centre[static_cast<size_t>(k + 1)];
            a.lo[static_cast<size_t>(p)] = k;
            a.hi[static_cast<size_t>(p)] = k + 1;
            a.weight[static_cast<size_t>(p)] = (fp - c0) / (c1 - c0);
        }
    }
}

// The bin of a value on the global scale.
//
// QA-B-207b (Codex #120): scale and bin are computed in DOUBLE. `4095.0f / range` overflows float for every range below
// 4095 / FLT_MAX = 1.2034e-35, and the conversion of the resulting Inf/NaN product to int is undefined (MSVC gives INT_MIN,
// so every bin landed on 0 and the call returned XPE_OK with wrong pixels, measured at 97 % of the value range off for
// 0 .. 1e-37f). In double the scale is finite for EVERY float range: the narrowest positive range is one denormal step,
// 1.4e-45, and 4095 / 1.4e-45 = 2.9e48 is far below DBL_MAX (1.8e308). `v - vmin` is exact in double and lies in
// [0, range], so the product is at most 4095 (+ rounding), so `bin` never leaves [0, 4095] before the clamp, which only
// absorbs that last rounding. No float range is therefore treated as "flat" except an exactly zero one.
static inline int bin_of(float v, float vmin, double scale)
{
    int bin = static_cast<int>((static_cast<double>(v) - static_cast<double>(vmin)) * scale);
    if (bin < 0) bin = 0;
    if (bin >= NUM_BINS) bin = NUM_BINS - 1;
    return bin;
}

// Table of one tile: clipped histogram on the global scale -> cumulative fraction.
static void build_tile_lut(const float* px, int img_w,
                             int x0, int y0, int x1, int y1,
                             float vmin, double scale, float clip_limit,
                             float* lut)
{
    std::vector<int64_t> hist(static_cast<size_t>(NUM_BINS), 0);
    for (int y = y0; y < y1; ++y) {
        const float* row = px + static_cast<int64_t>(y) * img_w;
        for (int x = x0; x < x1; ++x) {
            hist[static_cast<size_t>(bin_of(row[x], vmin, scale))]++;
        }
    }

    // Clip and redistribute excess
    const int64_t tile_area = static_cast<int64_t>(x1 - x0) * static_cast<int64_t>(y1 - y0);
    // QA-B-181d (Codex #48): limited before the conversion. clip_limit is finite (checked by the caller) but can be
    // huge, and the product leaves int; a clip at or above the tile area never clips (no bin holds more pixels
    // than the tile has), so the cap changes no result.
    const double clipF = static_cast<double>(clip_limit) * static_cast<double>(tile_area) / static_cast<double>(NUM_BINS);
    int64_t clip_count = clipF >= static_cast<double>(tile_area) ? tile_area : static_cast<int64_t>(clipF);
    if (clip_count < 1) clip_count = 1;

    int64_t excess = 0;
    for (int i = 0; i < NUM_BINS; ++i) {
        if (hist[static_cast<size_t>(i)] > clip_count) {
            excess += hist[static_cast<size_t>(i)] - clip_count;
            hist[static_cast<size_t>(i)] = clip_count;
        }
    }
    const int64_t per_bin = excess / NUM_BINS;
    int64_t remainder = excess - per_bin * NUM_BINS;
    for (int i = 0; i < NUM_BINS; ++i) hist[static_cast<size_t>(i)] += per_bin;
    // The part of the excess that does not divide evenly is spread at a fixed stride over the WHOLE scale (Zuiderveld,
    // and the same as OpenCV's residualStep). Giving it to the first `remainder` bins instead pushes it all to the low
    // end of the global scale: for a small tile (area below NUM_BINS the remainder IS most of the excess) the tile's
    // table then depends on where the tile sits on the scale -- a tile of high values saw its whole table pre-filled
    // below its own values and came out nearly constant (the AC-04 ramp collapsed from stddev 1.02 to 0.0066).
    if (remainder > 0) {
        const int stride = static_cast<int>(std::max<int64_t>(NUM_BINS / remainder, 1));
        for (int i = 0; i < NUM_BINS && remainder > 0; i += stride, --remainder) hist[static_cast<size_t>(i)]++;
    }

    // CDF normalized to [0, 1]
    int64_t cumsum = 0;
    const double norm = 1.0 / static_cast<double>(std::max<int64_t>(1, tile_area));
    for (int i = 0; i < NUM_BINS; ++i) {
        cumsum += hist[static_cast<size_t>(i)];
        lut[i] = static_cast<float>(static_cast<double>(cumsum) * norm);
    }
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
    // QA-B-181f (#233): the finiteness test rides on this pass, which reads every pixel anyway, so it costs no extra
    // pass over memory (a separate scan measured +3.9 ms on 3072x3072, about 8%). A non-finite pixel poisons the tile
    // range (+inf makes it infinite and the whole image non-finite) or goes through float->int conversions that are
    // undefined (NaN), both with rc=0 (QA-B-181e). The image is refused and left untouched -- nothing has been
    // written yet, the result is built in a separate buffer -- and it is not repaired.
    float val_min = px[0];
    float val_max = px[0];
    uint32_t nonfinite = 0;
    for (uint64_t i = 0; i < n; ++i) {
        uint32_t u;
        std::memcpy(&u, px + i, sizeof u);
        nonfinite |= static_cast<uint32_t>((u & 0x7F800000u) == 0x7F800000u);
        if (px[i] < val_min) val_min = px[i];
        if (px[i] > val_max) val_max = px[i];
    }
    if (nonfinite != 0) return XPE_ERR_INVALID_INPUT;
    float val_range = val_max - val_min;
    // QA-B-181f (#233): finite pixels whose range overflows float (-3e38 and 3e38) made every output pixel
    // `val_min + frac * inf`, non-finite, with rc=0 (QA-B-181e). Refused like the non-finite input: the result of
    // this call could not be finite, and the image is untouched.
    if (!xpe_float_is_finite(val_range)) return XPE_ERR_INVALID_INPUT;
    // QA-B-207b: the flat test and the scale use the exact double difference (never 0 for two different floats: denormals
    // are gradual), so an image whose values differ only at 1e-37 is not mistaken for a flat one -- see bin_of.
    const double range_d = static_cast<double>(val_max) - static_cast<double>(val_min);
    if (!(range_d > 0.0)) return XPE_OK; // Flat image, no contrast to enhance.

    int num_tiles_x = p->tile_width;
    int num_tiles_y = p->tile_height;
    // QA-B-181b (Codex #35): divide first. `(w + num_tiles_x - 1) / num_tiles_x` leaves int when w is near INT32_MAX
    // (w = INT32_MAX, tile_width = 2 passes the window check above): undefined behaviour no guard can catch.
    int tile_w = xpe_ceil_div(w, num_tiles_x);
    int tile_h = xpe_ceil_div(h, num_tiles_y);

    // The one intensity scale of the whole image (step 1).
    const double scale = static_cast<double>(NUM_BINS - 1) / range_d;

    // The non-empty tiles of each axis, their centres, and for every pixel position its two neighbouring tiles and
    // the weight between them (step 3).
    Axis ax, ay;
    build_axis(w, tile_w, num_tiles_x, ax);
    build_axis(h, tile_h, num_tiles_y, ay);
    const size_t nx = ax.tile.size();
    const size_t ny = ay.tile.size();

    // One table per non-empty tile (step 2). 64-bit: after the window check each tile count is at most 2^30, so
    // the product can reach 2^60; the allocation throws std::bad_alloc if it cannot be had (OUT_OF_MEMORY).
    std::vector<float> luts(nx * ny * static_cast<size_t>(NUM_BINS));
    for (size_t j = 0; j < ny; ++j) {
        int y0, y1;
        xpe_tile_bounds(ay.tile[j], tile_h, h, y0, y1);
        for (size_t i = 0; i < nx; ++i) {
            int x0, x1;
            xpe_tile_bounds(ax.tile[i], tile_w, w, x0, x1);
            build_tile_lut(px, w, x0, y0, x1, y1, val_min, scale, p->clip_limit,
                           luts.data() + (j * nx + i) * static_cast<size_t>(NUM_BINS));
        }
    }

    // Step 3 and 4: bilinear blend of the four surrounding tables, mapped back to the global value range.
    // The result is built in a separate buffer so that a failure above leaves the image untouched.
    std::vector<float> output(n);

    for (int y = 0; y < h; ++y) {
        const size_t j0 = static_cast<size_t>(ay.lo[static_cast<size_t>(y)]);
        const size_t j1 = static_cast<size_t>(ay.hi[static_cast<size_t>(y)]);
        const float  wy = ay.weight[static_cast<size_t>(y)];
        const float* row = px + static_cast<int64_t>(y) * w;
        float* out = output.data() + static_cast<int64_t>(y) * w;
        const float* lutRow0 = luts.data() + j0 * nx * static_cast<size_t>(NUM_BINS);
        const float* lutRow1 = luts.data() + j1 * nx * static_cast<size_t>(NUM_BINS);

        for (int x = 0; x < w; ++x) {
            const size_t i0 = static_cast<size_t>(ax.lo[static_cast<size_t>(x)]) * static_cast<size_t>(NUM_BINS);
            const size_t i1 = static_cast<size_t>(ax.hi[static_cast<size_t>(x)]) * static_cast<size_t>(NUM_BINS);
            const float  wx = ax.weight[static_cast<size_t>(x)];
            const size_t b = static_cast<size_t>(bin_of(row[x], val_min, scale));

            const float top = lutRow0[i0 + b] + wx * (lutRow0[i1 + b] - lutRow0[i0 + b]);
            const float bot = lutRow1[i0 + b] + wx * (lutRow1[i1 + b] - lutRow1[i0 + b]);
            const float frac = top + wy * (bot - top);

            // Map the fraction back to the global value range
            out[x] = val_min + frac * val_range;
        }
    }

    std::copy(output.begin(), output.end(), px);
    return XPE_OK;
}

} // extern "C"
