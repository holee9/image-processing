/**
 * @file ghost_correct.cpp
 * @brief SWU-1.4: Ghost/Lag Correction Tier 1 — LTI deconvolution (PRE-04)
 *        Dual-exponential IRF model (ref: PMC3465354)
 *        REQ-P1A-029 to REQ-P1A-034
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cstdlib>
#include <cstring>
#include <cmath>

// @MX:ANCHOR: [AUTO] xpe_ghost_create — resource allocation for ghost corrector
// @MX:REASON: All ghost functions fan in; handle is the invariant contract point
// @MX:SPEC: REQ-P1A-029
XpeErrorCode xpe_ghost_create(uint32_t width, uint32_t height,
                               const char* configJsonOrNull,
                               void** handleOut)
{
    if (!handleOut || width == 0 || height == 0) return XPE_ERR_INVALID_INPUT;

    // REQ-P1A-029: allocate handle with frame history buffer
    // REQ-P1A-030: configJsonOrNull for IRF coefficient override
    // REQ-P1A-031: XPE_ERR_OUT_OF_MEMORY on allocation failure
    // TODO: parse IRF coefficients from configJsonOrNull, allocate hist1/hist2 buffers
    auto* handle = new (std::nothrow) GhostCorrectorHandle();
    if (!handle) return XPE_ERR_OUT_OF_MEMORY;

    handle->width  = width;
    handle->height = height;
    const size_t pixelCount = static_cast<size_t>(width) * height;

    try {
        handle->hist1.assign(pixelCount, 0.0f);
        handle->hist2.assign(pixelCount, 0.0f);
    } catch (...) {
        delete handle;
        return XPE_ERR_OUT_OF_MEMORY;
    }

    (void)configJsonOrNull; // TODO: parse alpha/tau overrides
    *handleOut = handle;
    return XPE_OK;
}

// @MX:NOTE: [AUTO] LTI lag correction using dual-exponential IRF
// @MX:SPEC: REQ-P1A-032
XpeErrorCode xpe_ghost_correct(void* handle, XpeImageBuffer* img,
                                const XpeImageMetadata* meta)
{
    if (!GhostCorrectorHandle::isValid(handle) || !img || !meta)
        return XPE_ERR_INVALID_INPUT;

    auto* gh = static_cast<GhostCorrectorHandle*>(handle);
    if (img->width != gh->width || img->height != gh->height)
        return XPE_ERR_INVALID_INPUT;

    const size_t n = static_cast<size_t>(gh->width) * gh->height;
    auto* px = static_cast<float*>(img->pixels);

    // REQ-P1A-033: compute time delta in units of frames (1.0 for first frame)
    double dt = (gh->lastAcqTimeSec > 0.0)
                ? (meta->acquisitionTime - gh->lastAcqTimeSec)
                : 1.0;
    if (dt <= 0.0) dt = 1.0; // guard against zero/negative dt
    gh->lastAcqTimeSec = meta->acquisitionTime;

    const float decay1 = static_cast<float>(std::exp(-dt / gh->tau1));
    const float decay2 = static_cast<float>(std::exp(-dt / gh->tau2));
    const float a1 = static_cast<float>(gh->alpha1);
    const float a2 = static_cast<float>(gh->alpha2);

    // REQ-P1A-032: apply LTI deconvolution: corrected = frame - alpha1*hist1 - alpha2*hist2
    // Update history AFTER correction using raw (pre-correction) frame value
    for (size_t i = 0; i < n; ++i) {
        const float raw = px[i];
        float corrected = raw - a1 * gh->hist1[i] - a2 * gh->hist2[i];
        gh->hist1[i] = decay1 * gh->hist1[i] + raw;
        gh->hist2[i] = decay2 * gh->hist2[i] + raw;
        px[i] = corrected;
    }
    return XPE_OK;
}

XpeErrorCode xpe_ghost_reset(void* handle)
{
    if (!GhostCorrectorHandle::isValid(handle)) return XPE_ERR_INVALID_INPUT;
    auto* gh = static_cast<GhostCorrectorHandle*>(handle);
    // REQ-P1A-034: clear accumulated frame history
    std::fill(gh->hist1.begin(), gh->hist1.end(), 0.0f);
    std::fill(gh->hist2.begin(), gh->hist2.end(), 0.0f);
    gh->lastAcqTimeSec = 0.0;
    return XPE_OK;
}

void xpe_ghost_destroy(void* handle)
{
    if (!GhostCorrectorHandle::isValid(handle)) return;
    auto* gh = static_cast<GhostCorrectorHandle*>(handle);
    gh->magic = 0; // invalidate before delete
    delete gh;
}
