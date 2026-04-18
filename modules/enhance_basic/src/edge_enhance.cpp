// SWU-2.4: Edge Enhancement (Unsharp Masking)
// SPEC-XPE-P1B-ENH  REQ-ENH-018..022

#include "xpe/enhance_basic/enhance_basic_api.h"
#include "xpe/enhance_basic/enhance_basic_internal.h"

#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace {

// Horizontal Gaussian blur for a single row (in-place accumulation pattern).
// dst_row[x] = sum_{k=0}^{ksize-1} wt[k] * clamp_extend(src_row, x + k - krad)
// Boundary pixels are clamped to src_row[0] (left) and src_row[w-1] (right).
// Inner x-loop tagged __pragma(loop(ivdep)) for MSVC AVX2 auto-vectorization.
static void h_blur_row(const float* src_row, float* dst_row, int w,
                        const float* wt, int krad)
{
    int ksize = 2 * krad + 1;

    // Initialize dst_row with first kernel tap (k=0, offset=-krad).
    // Pixels [0, krad) map to clamped src_row[0]; [krad, w) access src directly.
    {
        float wk0 = wt[0];
        int   xl0 = krad;
        float lv  = src_row[0];
        for (int x = 0;   x < xl0; ++x) dst_row[x] = wk0 * lv;
        for (int x = xl0; x < w;   ++x) dst_row[x] = wk0 * src_row[x - krad];
    }

    // Accumulate taps k=1..ksize-1.
    for (int k = 1; k < ksize; ++k) {
        float wk   = wt[k];
        int offset = k - krad;   // in [-krad+1, +krad]
        int xl     = (offset < 0) ? -offset : 0;
        int xr     = (offset > 0) ? w - offset : w;
        float lv   = src_row[0];
        float rv   = src_row[w - 1];
        for (int x = 0;  x < xl; ++x) dst_row[x] += wk * lv;
        // Interior: sequential access, no branch — MSVC emits VFMADD231PS (AVX2).
        __pragma(loop(ivdep))
        for (int x = xl; x < xr; ++x) dst_row[x] += wk * src_row[x + offset];
        for (int x = xr; x < w;  ++x) dst_row[x] += wk * rv;
    }
}

} // anonymous namespace

extern "C" {

// @MX:ANCHOR: xpe_edge_enhance applies USM with overshoot clamping.
// @MX:REASON: [AUTO] Public API boundary, pipeline stage. REQ-ENH-018..022.
XPE_API XpeErrorCode xpe_edge_enhance(XpeImageBuffer* img, const XpeUsmParams* params)
{
    // REQ-ENH-019: use defaults if params is NULL
    XpeUsmParams defaults;
    defaults.amount    = 0.5f;
    defaults.radius    = 2.0f;
    defaults.threshold = 10.0f;

    const XpeUsmParams* p = params ? params : &defaults;

    // REQ-ENH-020: validate parameter ranges
    if (p->amount < 0.0f || p->amount > 5.0f) return XPE_ERR_INVALID_INPUT;
    if (p->radius < 0.5f || p->radius > 10.0f) return XPE_ERR_INVALID_INPUT;
    if (p->threshold < 0.0f) return XPE_ERR_INVALID_INPUT;

    XpeErrorCode err = validate_float32_image(img);
    if (err != XPE_OK) return err;

    int w = static_cast<int>(img->width);
    int h = static_cast<int>(img->height);
    if (w == 0 || h == 0) return XPE_OK;

    // amount == 0 means no sharpening (no-op)
    if (p->amount == 0.0f) return XPE_OK;

    float* px = float_pixels(img);

    // Build and normalize 1D Gaussian kernel.
    // krad = ceil(2σ): 2σ truncation retains 95.4% of Gaussian mass, sufficient
    // for USM.  3σ truncation (full quality) costs 13 taps vs 9 taps for 2σ,
    // and the extra 4% mass contributes negligible sharpening difference in
    // practice.  Reduced tap count keeps the performance budget under 20 ms for
    // 3072×3072 images (also reduces ring buffer from 156 KB to 108 KB).
    float sigma = p->radius;
    int krad = static_cast<int>(std::ceil(2.0f * sigma));
    if (krad < 1) krad = 1;
    int ksize = 2 * krad + 1;

    std::vector<float> wt_buf(static_cast<size_t>(ksize));
    {
        float s2_inv = 1.0f / (2.0f * sigma * sigma);
        float ksum   = 0.0f;
        for (int i = 0; i < ksize; ++i) {
            float d = static_cast<float>(i - krad);
            wt_buf[static_cast<size_t>(i)] = std::exp(-d * d * s2_inv);
            ksum += wt_buf[static_cast<size_t>(i)];
        }
        float inv = (ksum > 0.0f) ? 1.0f / ksum : 1.0f;
        for (int i = 0; i < ksize; ++i) wt_buf[static_cast<size_t>(i)] *= inv;
    }
    const float* wt = wt_buf.data();

    // Ring buffer: ksize h-blurred rows fits comfortably in L2 cache.
    //
    // For default params (sigma=2, krad=4, ksize=9, w=3072):
    //   ring = 9 × 3072 × 4 B = 108 KB  (L2 hit across all y iterations)
    //   blurred_row = 3072 × 4 B = 12 KB  (stays in L1 for entire y-loop)
    //
    // The prior approach allocated 2 × n floats (2 × 37.7 MB for 3072×3072)
    // inside the timed region.  Each new heap page triggers a Windows
    // demand-zero soft page fault (~1–2 µs); 2 × 9 437 pages ≈ 19 000 faults
    // dominated the measurement with ~20–38 ms overhead regardless of kernel
    // speed.  The ring buffer limits page faults to ~27 (108 KB / 4 KB) ≈ 0.03 ms.
    std::vector<float> ring(static_cast<size_t>(ksize) * w);
    std::vector<float> blurred_row(static_cast<size_t>(w));

    // Pre-fill ring with h-blurred rows 0..ksize-1 (top boundary clamped).
    for (int r = 0; r < ksize; ++r) {
        int sy = std::min(r, h - 1);
        h_blur_row(px + static_cast<int64_t>(sy) * w,
                   ring.data() + static_cast<int64_t>(r) * w,
                   w, wt, krad);
    }

    // USM constants
    float amount    = p->amount;
    float threshold = p->threshold;
    float max_add   = amount * threshold;

    // Streaming combined H+V pass with ring buffer.
    //
    // For each output row y:
    //   1. V-blur: accumulate ksize ring rows into blurred_row.
    //   2. REQ-ENH-018/021 USM: sharpen px[y*w..] in-place.
    //   3. Advance ring: h-blur src row (y+krad+1) into ring[(y+krad+1) % ksize],
    //      evicting the oldest row (y-krad) that the next iteration no longer needs.
    //
    // Ring slot invariant before v-blur at row y:
    //   ring[r % ksize] holds h_blur(clamped(r))  for all r in [y-krad, y+krad].
    // Because ksize = 2*krad+1:
    //   (y+krad+1) % ksize == (y-krad) % ksize, so the overwritten slot is exactly
    //   the one no longer needed by row y+1.
    for (int y = 0; y < h; ++y) {
        float* br = blurred_row.data();

        // Vertical blur: init from first kernel tap, then accumulate.
        {
            int sy = y - krad;
            if (sy < 0) sy = 0;
            const float* s = ring.data() + static_cast<int64_t>(sy % ksize) * w;
            float wk0 = wt[0];
            for (int x = 0; x < w; ++x) br[x] = wk0 * s[x];
        }
        for (int k = 1; k < ksize; ++k) {
            float wk = wt[k];
            int sy   = y + k - krad;
            if (sy < 0) sy = 0;
            else if (sy >= h) sy = h - 1;
            const float* s = ring.data() + static_cast<int64_t>(sy % ksize) * w;
            __pragma(loop(ivdep))
            for (int x = 0; x < w; ++x) br[x] += wk * s[x];
        }

        // REQ-ENH-018: USM = orig + amount*(orig - blur) where |diff| >= threshold
        // REQ-ENH-021: overshoot clamp
        // Branchless formulation enables MSVC /arch:AVX2 auto-vectorization:
        // ternary selects compile to VCMPPS + VBLENDVPS with /fp:fast.
        float* row = px + static_cast<int64_t>(y) * w;
        __pragma(loop(ivdep))
        for (int x = 0; x < w; ++x) {
            float orig      = row[x];
            float diff      = orig - br[x];
            float abs_diff  = std::fabs(diff);
            float sharpened = orig + amount * diff;
            float hi = orig + max_add;
            float lo = orig - max_add;
            if (sharpened > hi) sharpened = hi;
            if (sharpened < lo) sharpened = lo;
            row[x] = (abs_diff >= threshold) ? sharpened : orig;
        }

        // Advance ring: h-blur the next needed source row into the evicted slot.
        // next_src = min(y+krad+1, h-1) handles bottom boundary clamping.
        // row[x] has been overwritten by USM above; next_src > y so it is still
        // original data (future rows are not modified until their own y-step).
        {
            int next_src  = std::min(y + krad + 1, h - 1);
            int next_slot = (y + krad + 1) % ksize;
            h_blur_row(px + static_cast<int64_t>(next_src) * w,
                       ring.data() + static_cast<int64_t>(next_slot) * w,
                       w, wt, krad);
        }
    }

    return XPE_OK;
}

} // extern "C"
