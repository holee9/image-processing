// SWU-2.2: Noise Reduction (Bilateral + NLM)
// SPEC-XPE-P1B-ENH  REQ-ENH-007..012

#include "xpe/enhance_basic/enhance_basic_api.h"
#include "xpe/enhance_basic/enhance_basic_internal.h"

#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace {

// ---------------------------------------------------------------------------
// Separable bilateral filter — ring buffer + SVML-vectorized exp()
//
// Classical separable approximation: horizontal pass first, vertical second.
//
// Performance strategy (same as edge_enhance.cpp):
//   1. Loop swap: for each tap k, iterate over all x in one vectorizable pass
//      instead of iterating over x and doing all taps per pixel.  MSVC with
//      /arch:AVX2 + /fp:fast then emits SVML vexp8 instead of scalar exp().
//   2. Ring buffer: ksize × w floats (~156 KB for ksize=13, w=3072) replaces
//      the 2 × 37.7 MB per-image allocations that triggered ~19 000 Windows
//      demand-zero soft page faults inside the timed region.
//   3. 2σ truncation: krad = ceil(2σ), ksize=13 for σ=3, vs ksize=19 for 3σ.
//      95.4% of Gaussian mass retained — sufficient for bilateral denoising.
// ---------------------------------------------------------------------------

// Accumulate one horizontal-bilateral tap into wsum[]/vsum[] arrays.
// Called ksize times per row: for each tap k (k = 0..ksize-1, offset dx = k-radius).
// Inner x-loop tagged __pragma(loop(ivdep)) so MSVC/AVX2+fp:fast emits SVML
// vexpf8 (8-wide) for the range-kernel exp() call.
static void bilateral_h_tap(const float* src_row, float* wsum, float* vsum,
                              int w, float sp_w, int dx,
                              float minus_half_sr2_inv)
{
    // Split into left boundary / interior / right boundary to avoid
    // clamping branches inside the vectorizable inner loop.
    int xl = (dx < 0) ? -dx : 0;      // pixels [0, xl) clamp src to src_row[0]
    int xr = (dx > 0) ? w - dx : w;   // pixels [xr, w) clamp src to src_row[w-1]
    float lv = src_row[0];
    float rv = src_row[w - 1];

    // Left boundary
    for (int x = 0; x < xl; ++x) {
        float diff   = src_row[x] - lv;
        float rw     = std::exp(minus_half_sr2_inv * diff * diff);
        float weight = sp_w * rw;
        wsum[x] += weight;
        vsum[x] += weight * lv;
    }
    // Interior: both src_row[x] and src_row[x+dx] are sequential reads — vectorizable
    __pragma(loop(ivdep))
    for (int x = xl; x < xr; ++x) {
        float nbr    = src_row[x + dx];
        float diff   = src_row[x] - nbr;
        float rw     = std::exp(minus_half_sr2_inv * diff * diff);
        float weight = sp_w * rw;
        wsum[x] += weight;
        vsum[x] += weight * nbr;
    }
    // Right boundary
    for (int x = xr; x < w; ++x) {
        float diff   = src_row[x] - rv;
        float rw     = std::exp(minus_half_sr2_inv * diff * diff);
        float weight = sp_w * rw;
        wsum[x] += weight;
        vsum[x] += weight * rv;
    }
}

// Compute horizontal-bilateral blur for one source row → write into dst_row.
// wsum_buf / vsum_buf are caller-allocated scratch buffers of length w;
// they are zeroed inside this function.
static void bilateral_h_row(const float* src_row, float* dst_row, int w,
                              float* wsum_buf, float* vsum_buf,
                              const float* sp_w, int radius,
                              float minus_half_sr2_inv)
{
    int ksize = 2 * radius + 1;

    std::fill(wsum_buf, wsum_buf + w, 0.0f);
    std::fill(vsum_buf, vsum_buf + w, 0.0f);

    for (int k = 0; k < ksize; ++k) {
        bilateral_h_tap(src_row, wsum_buf, vsum_buf,
                         w, sp_w[k], k - radius, minus_half_sr2_inv);
    }

    for (int x = 0; x < w; ++x) {
        dst_row[x] = (wsum_buf[x] > 0.0f) ? vsum_buf[x] / wsum_buf[x] : src_row[x];
    }
}

static XpeErrorCode apply_bilateral(XpeImageBuffer* img, float sigma_space, float sigma_range)
{
    int w = static_cast<int>(img->width);
    int h = static_cast<int>(img->height);
    float* px = float_pixels(img);

    // 2σ truncation: ksize=13 for sigma_space=3 — retains 95.4% of Gaussian mass.
    // 3σ (ksize=19) adds negligible denoising quality for 31% extra compute cost.
    int radius = static_cast<int>(std::ceil(2.0f * sigma_space));
    int maxRad = std::min(15, std::min(w, h) / 2 - 1);
    if (maxRad < 1) maxRad = 1;
    if (radius > maxRad) radius = maxRad;
    if (radius < 1) radius = 1;
    int ksize = 2 * radius + 1;

    float ss2_inv            = 1.0f / (sigma_space * sigma_space);
    float minus_half_sr2_inv = -0.5f / (sigma_range * sigma_range);

    // Spatial weight LUT: exp(-0.5 * d^2 / sigma_space^2) for d = 0..ksize-1
    std::vector<float> sp_w(static_cast<size_t>(ksize));
    for (int k = 0; k < ksize; ++k) {
        int d = k - radius;
        sp_w[static_cast<size_t>(k)] = std::exp(-0.5f * static_cast<float>(d * d) * ss2_inv);
    }

    // Ring buffer: ksize h-blurred rows.
    // For default integration params (sigma_space=3, ksize=13, w=3072):
    //   ring = 13 × 3072 × 4 B = 156 KB — fits in L2 cache.
    // Replaces the 2 × 37.7 MB allocations that triggered ~19 000 page faults.
    std::vector<float> ring(static_cast<size_t>(ksize) * w);
    std::vector<float> wsum_buf(static_cast<size_t>(w));
    std::vector<float> vsum_buf(static_cast<size_t>(w));

    // Pre-fill ring with h-blurred rows 0..ksize-1 (top boundary clamped)
    for (int r = 0; r < ksize; ++r) {
        int sy = std::min(r, h - 1);
        bilateral_h_row(px + static_cast<int64_t>(sy) * w,
                         ring.data() + static_cast<int64_t>(r) * w,
                         w, wsum_buf.data(), vsum_buf.data(),
                         sp_w.data(), radius, minus_half_sr2_inv);
    }

    // Streaming vertical bilateral pass with ring buffer.
    //
    // For each output row y:
    //   1. Accumulate ksize bilateral-weighted ring rows → wsum/vsum.
    //   2. Normalize → write to px[y*w..].
    //   3. Advance ring: h-blur the next source row (y+radius+1, clamped) into
    //      the evicted slot, preparing for row y+1.
    //
    // Ring slot invariant (same as edge_enhance.cpp): before v-blur at row y,
    // ring[r % ksize] holds h_bilateral(clamped(r)) for r in [y-radius, y+radius].
    for (int y = 0; y < h; ++y) {
        const float* center_row = ring.data() + static_cast<int64_t>(y % ksize) * w;
        float* ws = wsum_buf.data();
        float* vs = vsum_buf.data();

        std::fill(ws, ws + w, 0.0f);
        std::fill(vs, vs + w, 0.0f);

        // Vertical bilateral accumulation — restructured to k outer, x inner
        // so the inner x-loop can be auto-vectorized with SVML exp8.
        for (int k = 0; k < ksize; ++k) {
            float sw = sp_w[static_cast<size_t>(k)];
            int sy   = y + k - radius;
            if (sy < 0) sy = 0;
            else if (sy >= h) sy = h - 1;
            const float* nbr = ring.data() + static_cast<int64_t>(sy % ksize) * w;

            __pragma(loop(ivdep))
            for (int x = 0; x < w; ++x) {
                float diff   = center_row[x] - nbr[x];
                float rw     = std::exp(minus_half_sr2_inv * diff * diff);
                float weight = sw * rw;
                ws[x] += weight;
                vs[x] += weight * nbr[x];
            }
        }

        // Normalize and write output back to px in-place
        float* out = px + static_cast<int64_t>(y) * w;
        for (int x = 0; x < w; ++x) {
            out[x] = (ws[x] > 0.0f) ? vs[x] / ws[x] : center_row[x];
        }

        // Advance ring: h-blur the next needed source row.
        // next_src is clamped to h-1 for bottom boundary.
        // Since next_src = y+radius+1 > y, the source row is still original data
        // (vertical output only modifies rows 0..y, already processed).
        {
            int next_src  = std::min(y + radius + 1, h - 1);
            int next_slot = (y + radius + 1) % ksize;
            bilateral_h_row(px + static_cast<int64_t>(next_src) * w,
                             ring.data() + static_cast<int64_t>(next_slot) * w,
                             w, wsum_buf.data(), vsum_buf.data(),
                             sp_w.data(), radius, minus_half_sr2_inv);
        }
    }

    return XPE_OK;
}

// ---------------------------------------------------------------------------
// Non-Local Means
// ---------------------------------------------------------------------------
static XpeErrorCode apply_nlm(XpeImageBuffer* img,
                               int search_window, int patch_size, float h_param)
{
    int w = static_cast<int>(img->width);
    int h = static_cast<int>(img->height);
    float* px = float_pixels(img);
    uint64_t n = static_cast<uint64_t>(w) * h;

    int half_search = search_window / 2;
    int half_patch = patch_size / 2;
    float h2_inv = 1.0f / (h_param * h_param);

    std::vector<float> output(n);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float wsum = 0.0f;
            float vsum = 0.0f;

            // Search window bounds (clamped to image)
            int sy0 = std::max(0, y - half_search);
            int sy1 = std::min(h - 1, y + half_search);
            int sx0 = std::max(0, x - half_search);
            int sx1 = std::min(w - 1, x + half_search);

            for (int sy = sy0; sy <= sy1; ++sy) {
                for (int sx = sx0; sx <= sx1; ++sx) {
                    // Compute patch distance
                    float dist2 = 0.0f;
                    int count = 0;
                    for (int py = -half_patch; py <= half_patch; ++py) {
                        int ry = y + py;
                        int sry = sy + py;
                        if (ry < 0 || ry >= h || sry < 0 || sry >= h) continue;
                        for (int ppx = -half_patch; ppx <= half_patch; ++ppx) {
                            int rx = x + ppx;
                            int srx = sx + ppx;
                            if (rx < 0 || rx >= w || srx < 0 || srx >= w) continue;
                            float diff = px[static_cast<int64_t>(ry) * w + rx]
                                       - px[static_cast<int64_t>(sry) * w + srx];
                            dist2 += diff * diff;
                            ++count;
                        }
                    }
                    if (count > 0) {
                        dist2 /= static_cast<float>(count);
                    }

                    float weight = std::exp(-dist2 * h2_inv);
                    wsum += weight;
                    vsum += weight * px[static_cast<int64_t>(sy) * w + sx];
                }
            }

            output[static_cast<int64_t>(y) * w + x] = (wsum > 0.0f) ? vsum / wsum
                                                                       : px[static_cast<int64_t>(y) * w + x];
        }
    }

    std::copy(output.begin(), output.end(), px);
    return XPE_OK;
}

} // anonymous namespace

extern "C" {

// @MX:ANCHOR: xpe_noise_reduce applies bilateral or NLM denoising in-place.
// @MX:REASON: [AUTO] Public API boundary, key pipeline stage. REQ-ENH-007..012.
XPE_API XpeErrorCode xpe_noise_reduce(XpeImageBuffer* img, const XpeNoiseReduceParams* params)
{
    // REQ-ENH-009: NULL params check
    if (!params) return XPE_ERR_INVALID_INPUT;

    XpeErrorCode err = validate_float32_image(img);
    if (err != XPE_OK) return err;

    if (img->width == 0 || img->height == 0) return XPE_OK;

    if (params->mode == XPE_NOISE_BILATERAL) {
        // REQ-ENH-010: sigma_space and sigma_range must be positive
        if (params->sigma_space <= 0.0f || params->sigma_range <= 0.0f) {
            return XPE_ERR_INVALID_INPUT;
        }
        return apply_bilateral(img, params->sigma_space, params->sigma_range);
    }
    else if (params->mode == XPE_NOISE_NLM) {
        // Validate NLM params: odd and positive
        if (params->search_window < 1 || (params->search_window % 2) == 0) {
            return XPE_ERR_INVALID_INPUT;
        }
        if (params->patch_size < 1 || (params->patch_size % 2) == 0) {
            return XPE_ERR_INVALID_INPUT;
        }
        if (params->h_param <= 0.0f) {
            return XPE_ERR_INVALID_INPUT;
        }
        return apply_nlm(img, params->search_window, params->patch_size, params->h_param);
    }

    return XPE_ERR_INVALID_INPUT;
}

// @MX:NOTE: Noise sigma estimation via MAD on center ROI.
// @MX:SPEC: REQ-ENH-011
XPE_API XpeErrorCode xpe_noise_estimate_sigma(const XpeImageBuffer* img, float* outSigma)
{
    if (!outSigma) return XPE_ERR_INVALID_INPUT;

    XpeErrorCode err = validate_float32_image(img);
    if (err != XPE_OK) return err;

    int w = static_cast<int>(img->width);
    int h = static_cast<int>(img->height);
    if (w == 0 || h == 0) return XPE_ERR_INVALID_INPUT;

    const float* px = const_float_pixels(img);

    // Extract center ROI: 10% of min(width, height) square
    int roi_side = std::max(1, static_cast<int>(0.1f * static_cast<float>(std::min(w, h))));
    int cx = w / 2;
    int cy = h / 2;
    int rx0 = cx - roi_side / 2;
    int ry0 = cy - roi_side / 2;
    if (rx0 < 0) rx0 = 0;
    if (ry0 < 0) ry0 = 0;
    int rx1 = std::min(w, rx0 + roi_side);
    int ry1 = std::min(h, ry0 + roi_side);

    std::vector<float> roi_pixels;
    roi_pixels.reserve(static_cast<size_t>(roi_side) * roi_side);
    for (int y = ry0; y < ry1; ++y) {
        for (int x = rx0; x < rx1; ++x) {
            roi_pixels.push_back(px[static_cast<int64_t>(y) * w + x]);
        }
    }

    if (roi_pixels.empty()) {
        *outSigma = 0.0f;
        return XPE_OK;
    }

    // Compute median
    size_t mid = roi_pixels.size() / 2;
    std::nth_element(roi_pixels.begin(), roi_pixels.begin() + static_cast<ptrdiff_t>(mid), roi_pixels.end());
    float median = roi_pixels[mid];

    // Compute MAD (median of absolute deviations)
    std::vector<float> abs_devs(roi_pixels.size());
    for (size_t i = 0; i < roi_pixels.size(); ++i) {
        abs_devs[i] = std::fabs(roi_pixels[i] - median);
    }
    mid = abs_devs.size() / 2;
    std::nth_element(abs_devs.begin(), abs_devs.begin() + static_cast<ptrdiff_t>(mid), abs_devs.end());
    float mad = abs_devs[mid];

    // sigma = 1.4826 * MAD
    *outSigma = 1.4826f * mad;

    return XPE_OK;
}

} // extern "C"
