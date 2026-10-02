/**
 * @file ai_bodypart.h
 * @brief Pure helpers for xpe_bodypart_recognize: the model's input size and the image resize (QA-B-191, #130).
 *
 * Header-only and free of ONNX Runtime types so the tests can call them directly. That matters for the resize:
 * seen only through a model, a wrong resize is a different probability vector and looks like "the model
 * decided differently".
 *
 * WHAT THE MODULE DOES TO THE IMAGE, AND WHAT IT DOES NOT. It resizes to the size the model's graph declares.
 * It does NOT normalise intensity: the caller supplies pixels in the scale the model was trained for, exactly
 * as for xpe_bone_suppress (see the PIXEL SCALE note in ai_api.h).
 */
#ifndef XPE_AI_BODYPART_H
#define XPE_AI_BODYPART_H

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace xpe::ai {

/** Largest model input side the module accepts: the module's own maximum image side (validateImageBuffer). */
constexpr uint32_t kBodyPartMaxSide = 4096;

/**
 * The image size a model wants, from the shape its graph declares for its first input.
 *
 * Accepted: a fixed [1,1,H,W] (NCHW, one channel) or [1,H,W,1] (NHWC, one channel), 1 <= H,W <= 4096.
 * Not accepted: any other rank, a batch or channel count other than 1, a dynamic axis (-1) or a side outside
 * the range. The caller turns a refusal into "this model cannot be used", not into a guess.
 */
inline bool BodyPartInputSize(const std::vector<int64_t>& shape, uint32_t* height, uint32_t* width) {
    if (shape.size() != 4) return false;
    int64_t h = 0, w = 0;
    if (shape[0] == 1 && shape[1] == 1) {          // NCHW
        h = shape[2];
        w = shape[3];
    } else if (shape[0] == 1 && shape[3] == 1) {   // NHWC
        h = shape[1];
        w = shape[2];
    } else {
        return false;
    }
    if (h < 1 || w < 1 || h > kBodyPartMaxSide || w > kBodyPartMaxSide) return false;
    *height = static_cast<uint32_t>(h);
    *width = static_cast<uint32_t>(w);
    return true;
}

namespace detail {

/** Weights of one destination sample along one axis: source indices [first, first + w.size()). */
struct AxisTap {
    uint32_t first = 0;
    std::vector<double> w;
};

/**
 * One axis of the resize, as a list of taps per destination index.
 *  - same length: the identity (one tap of weight 1), so an unchanged image is copied bit for bit;
 *  - shrinking: the AREA average of the source interval [i*s/d, (i+1)*s/d), fractional edges weighted by overlap;
 *  - growing: linear interpolation at the pixel centre (i + 0.5) * s/d - 0.5, clamped to the image.
 */
inline std::vector<AxisTap> BuildAxis(uint32_t s, uint32_t d) {
    std::vector<AxisTap> taps(d);
    if (s == d) {
        for (uint32_t i = 0; i < d; ++i) taps[i] = AxisTap{i, {1.0}};
        return taps;
    }
    const double scale = static_cast<double>(s) / static_cast<double>(d);
    for (uint32_t i = 0; i < d; ++i) {
        if (s > d) {
            const double a = i * scale;
            const double b = (i + 1) * scale;
            const uint32_t lo = static_cast<uint32_t>(std::floor(a));
            uint32_t hi = static_cast<uint32_t>(std::ceil(b));
            if (hi > s) hi = s;
            AxisTap t;
            t.first = lo;
            double sum = 0.0;
            for (uint32_t k = lo; k < hi; ++k) {
                const double overlap = std::fmin(b, k + 1.0) - std::fmax(a, static_cast<double>(k));
                t.w.push_back(overlap);
                sum += overlap;
            }
            for (double& v : t.w) v /= sum;
            taps[i] = std::move(t);
        } else {
            double x = (i + 0.5) * scale - 0.5;
            if (x < 0.0) x = 0.0;
            if (x > s - 1.0) x = s - 1.0;
            const uint32_t k = static_cast<uint32_t>(std::floor(x));
            const double f = x - k;
            AxisTap t;
            t.first = k;
            if (f == 0.0 || k + 1 >= s) {
                t.w = {1.0};
            } else {
                t.w = {1.0 - f, f};
            }
            taps[i] = std::move(t);
        }
    }
    return taps;
}

}  // namespace detail

/**
 * Resize a single-channel float image from sw x sh to dw x dh (row-major, no padding).
 *
 * Separable, deterministic, and exact for the identity. Each axis shrinks by area average and grows by
 * bilinear interpolation, chosen per axis. Sizes must be >= 1 (the caller has validated the image).
 */
inline std::vector<float> ResizeImageFloat(const float* src, uint32_t sw, uint32_t sh, uint32_t dw, uint32_t dh) {
    const std::vector<detail::AxisTap> xt = detail::BuildAxis(sw, dw);
    const std::vector<detail::AxisTap> yt = detail::BuildAxis(sh, dh);

    // Horizontal pass: sh rows of dw samples.
    std::vector<float> mid(static_cast<size_t>(sh) * dw);
    for (uint32_t y = 0; y < sh; ++y) {
        const float* row = src + static_cast<size_t>(y) * sw;
        float* out = mid.data() + static_cast<size_t>(y) * dw;
        for (uint32_t x = 0; x < dw; ++x) {
            double acc = 0.0;
            const detail::AxisTap& t = xt[x];
            for (size_t k = 0; k < t.w.size(); ++k) acc += t.w[k] * row[t.first + k];
            out[x] = static_cast<float>(acc);
        }
    }
    // Vertical pass: dh rows of dw samples.
    std::vector<float> dst(static_cast<size_t>(dh) * dw);
    for (uint32_t y = 0; y < dh; ++y) {
        const detail::AxisTap& t = yt[y];
        float* out = dst.data() + static_cast<size_t>(y) * dw;
        for (uint32_t x = 0; x < dw; ++x) {
            double acc = 0.0;
            for (size_t k = 0; k < t.w.size(); ++k) acc += t.w[k] * mid[static_cast<size_t>(t.first + k) * dw + x];
            out[x] = static_cast<float>(acc);
        }
    }
    return dst;
}

}  // namespace xpe::ai

#endif  // XPE_AI_BODYPART_H
