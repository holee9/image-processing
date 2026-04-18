/**
 * @file gain_correct.cpp
 * @brief SWU-1.2: Per-pixel flat-field gain normalization (PRE-03)
 *        Domain transition: uint16 -> float32 occurs in this stage.
 *        REQ-P1A-016 to REQ-P1A-019
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cstdlib>
#include <cstdint>

// @MX:ANCHOR: [AUTO] xpe_gain_correct — public API entry point, domain transition
// @MX:REASON: uint16->float32 domain transition happens here; all downstream funcs expect float32
// @MX:SPEC: REQ-P1A-016
// @MX:NOTE: [AUTO] Allocates new float buffer; caller takes ownership of img->pixels after call
XpeErrorCode xpe_gain_correct(XpeImageBuffer* img,
                               const XpeImageBuffer* gainMap)
{
    if (!img || !gainMap) return XPE_ERR_INVALID_INPUT;
    if (img->width == 0 || img->height == 0) return XPE_ERR_INVALID_INPUT;
    if (!xpe_dims_match(img, gainMap)) return XPE_ERR_INVALID_INPUT;
    // REQ-P1A-017: Only contiguous buffers supported; reject padded (non-contiguous) layouts.
    if (!xpe_is_contiguous(img, sizeof(uint16_t))) return XPE_ERR_INVALID_INPUT;
    if (!xpe_is_contiguous(gainMap, sizeof(float))) return XPE_ERR_INVALID_INPUT;

    // REQ-P1A-016: corrected[i] = img[i] * gainMap[i]  (float32 output)
    // REQ-P1A-017: uint16 input -> float32 output (domain transition — only stage that changes format)
    // REQ-P1A-018: gainMap is float32
    // REQ-P1A-019: result stored back in img->pixels as float32
    //
    // NOTE: float32 (4B) > uint16 (2B), so in-place conversion would overflow the source buffer.
    // A new float buffer is allocated and stored in img->pixels; ownership transfers to the caller.
    //
    // Overflow guard: img->width and img->height are uint32_t; their product fits in size_t
    // (64-bit) for all realistic image sizes, but we guard explicitly.
    if (img->width > SIZE_MAX / img->height) return XPE_ERR_INVALID_INPUT;
    const size_t n    = static_cast<size_t>(img->width) * img->height;
    const auto*  u16  = static_cast<const uint16_t*>(img->pixels);
    const auto*  gain = static_cast<const float*>(gainMap->pixels);

    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): pipeline manages lifetime
    float* dst = static_cast<float*>(std::malloc(n * sizeof(float)));
    if (!dst) return XPE_ERR_OUT_OF_MEMORY;

    for (size_t i = 0; i < n; ++i)
        dst[i] = static_cast<float>(u16[i]) * gain[i];

    img->pixels      = dst;
    img->pixelFormat = XPE_PIXEL_FORMAT_FLOAT32;
    img->stride      = img->width * sizeof(float);
    return XPE_OK;
}
