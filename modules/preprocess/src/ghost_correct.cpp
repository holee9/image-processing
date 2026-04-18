/**
 * @file ghost_correct.cpp
 * @brief SWU-1.4: Ghost/Lag Correction Tier 1/2/3 — LTI/NLCSC deconvolution (PRE-04)
 *        Dual-exponential IRF model (ref: PMC3465354)
 *        Tier 1: LTI deconvolution
 *        Tier 2: Exposure-weighted LTI
 *        Tier 3: NLCSC (Nonlinear Causal Spatial Context)
 *        REQ-P1A-029 to REQ-P1A-034
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

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

    // Parse config JSON for tier and IRF coefficients
    if (configJsonOrNull) {
        // Parse tier (default: 1)
        std::string tierStr = xpe_json_get_string(configJsonOrNull, "tier");
        if (!tierStr.empty()) {
            handle->tier = std::stoi(tierStr);
            if (handle->tier < 1 || handle->tier > 3) handle->tier = 1;
        }

        // Parse alpha1, tau1, alpha2, tau2 overrides
        std::string alpha1Str = xpe_json_get_string(configJsonOrNull, "alpha1");
        if (!alpha1Str.empty()) handle->alpha1 = std::stod(alpha1Str);

        std::string tau1Str = xpe_json_get_string(configJsonOrNull, "tau1");
        if (!tau1Str.empty()) handle->tau1 = std::stod(tau1Str);

        std::string alpha2Str = xpe_json_get_string(configJsonOrNull, "alpha2");
        if (!alpha2Str.empty()) handle->alpha2 = std::stod(alpha2Str);

        std::string tau2Str = xpe_json_get_string(configJsonOrNull, "tau2");
        if (!tau2Str.empty()) handle->tau2 = std::stod(tau2Str);

        // Parse Tier 2/3 specific parameters
        std::string thresholdStr = xpe_json_get_string(configJsonOrNull, "tier2Threshold");
        if (!thresholdStr.empty()) handle->tier2Threshold = std::stod(thresholdStr);

        std::string betaStr = xpe_json_get_string(configJsonOrNull, "nlcscBeta");
        if (!betaStr.empty()) handle->nlcscBeta = std::stod(betaStr);
    }

    *handleOut = handle;
    return XPE_OK;
}

// @MX:ANCHOR: [AUTO] xpe_ghost_correct — multi-tier ghost correction with auto-escalation
// @MX:REASON: Main correction entry point; all correction logic fans in here
// @MX:SPEC: REQ-P1A-032, REQ-P1A-033 (Tier 1/2/3)

namespace {
    // Helper: compute mean signal level for exposure estimation
    float compute_frame_mean(const float* px, size_t n) noexcept {
        if (n == 0) return 0.0f;
        double sum = 0.0;
        for (size_t i = 0; i < n; ++i) {
            if (std::isfinite(px[i])) {
                sum += static_cast<double>(px[i]);
            }
        }
        return static_cast<float>(sum / static_cast<double>(n));
    }

    // Tier 1: Standard LTI deconvolution
    XpeErrorCode ghost_tier1(GhostCorrectorHandle* gh, float* px, size_t n,
                             float decay1, float decay2, float a1, float a2) {
        for (size_t i = 0; i < n; ++i) {
            const float raw = px[i];
            if (!std::isfinite(raw)) return XPE_ERR_PROCESSING_FAILED;
            float corrected = raw - a1 * gh->hist1[i] - a2 * gh->hist2[i];
            gh->hist1[i] = decay1 * gh->hist1[i] + raw;
            gh->hist2[i] = decay2 * gh->hist2[i] + raw;
            if (!std::isfinite(corrected)) return XPE_ERR_PROCESSING_FAILED;
            px[i] = (corrected > 0.0f) ? corrected : 0.0f;
        }
        return XPE_OK;
    }

    // Tier 2: Exposure-weighted LTI deconvolution
    XpeErrorCode ghost_tier2(GhostCorrectorHandle* gh, float* px, size_t n,
                             float decay1, float decay2, float a1_base, float a2_base) {
        // Exposure-weighted coefficients based on frame mean
        const float meanSignal = compute_frame_mean(px, n);
        gh->lastFrameMean = meanSignal;

        // Scale alpha based on exposure level (higher exposure = stronger correction)
        // Reference: 50% of saturation = 1.0x scaling, 100% saturation = 1.5x scaling
        const float exposureWeight = 1.0f + (meanSignal / 32768.0f) * 0.5f;
        gh->exposureWeight = exposureWeight;

        const float a1 = a1_base * exposureWeight;
        const float a2 = a2_base * exposureWeight;

        for (size_t i = 0; i < n; ++i) {
            const float raw = px[i];
            if (!std::isfinite(raw)) return XPE_ERR_PROCESSING_FAILED;
            float corrected = raw - a1 * gh->hist1[i] - a2 * gh->hist2[i];
            gh->hist1[i] = decay1 * gh->hist1[i] + raw;
            gh->hist2[i] = decay2 * gh->hist2[i] + raw;
            if (!std::isfinite(corrected)) return XPE_ERR_PROCESSING_FAILED;
            px[i] = (corrected > 0.0f) ? corrected : 0.0f;
        }
        return XPE_OK;
    }

    // Tier 3: NLCSC (Nonlinear Causal Spatial Context) with signal-dependent coefficients
    XpeErrorCode ghost_tier3(GhostCorrectorHandle* gh, float* px, size_t n,
                             float decay1, float decay2, float a1_base, float a2_base) {
        const float meanSignal = compute_frame_mean(px, n);
        gh->lastFrameMean = meanSignal;
        const float exposureWeight = 1.0f + (meanSignal / 32768.0f) * 0.5f;
        gh->exposureWeight = exposureWeight;

        const float beta = gh->nlcscBeta; // signal dependency parameter
        const uint32_t W = gh->width;
        const uint32_t H = gh->height;

        // Apply NLCSC with signal-dependent coefficients
        for (size_t i = 0; i < n; ++i) {
            const float raw = px[i];
            if (!std::isfinite(raw)) return XPE_ERR_PROCESSING_FAILED;

            // Signal-dependent coefficient: higher signal = stronger correction
            const float signalDependence = 1.0f + beta * (raw / 32768.0f);
            const float a1 = a1_base * exposureWeight * signalDependence;
            const float a2 = a2_base * exposureWeight * signalDependence;

            float corrected = raw - a1 * gh->hist1[i] - a2 * gh->hist2[i];

            // Spatial context: blend with local neighborhood mean (3x3)
            if (W >= 3 && H >= 3) {
                const uint32_t x = static_cast<uint32_t>(i % W);
                const uint32_t y = static_cast<uint32_t>(i / W);

                if (x > 0 && x < W - 1 && y > 0 && y < H - 1) {
                    float localMean = 0.0f;
                    int count = 0;
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            const size_t ni = (y + dy) * W + (x + dx);
                            if (ni < n && std::isfinite(px[ni])) {
                                localMean += px[ni];
                                ++count;
                            }
                        }
                    }
                    if (count > 0) {
                        localMean /= static_cast<float>(count);
                        // Blend corrected with local mean (0.7 : 0.3)
                        corrected = 0.7f * corrected + 0.3f * localMean;
                    }
                }
            }

            gh->hist1[i] = decay1 * gh->hist1[i] + raw;
            gh->hist2[i] = decay2 * gh->hist2[i] + raw;

            if (!std::isfinite(corrected)) return XPE_ERR_PROCESSING_FAILED;
            px[i] = (corrected > 0.0f) ? corrected : 0.0f;
        }
        return XPE_OK;
    }
} // anonymous namespace

XpeErrorCode xpe_ghost_correct(void* handle, XpeImageBuffer* img,
                                const XpeImageMetadata* meta)
{
    if (!GhostCorrectorHandle::isValid(handle) || !img || !meta)
        return XPE_ERR_INVALID_INPUT;

    auto* gh = static_cast<GhostCorrectorHandle*>(handle);
    if (img->width != gh->width || img->height != gh->height)
        return XPE_ERR_INVALID_INPUT;
    size_t n = 0;
    if (!xpe_buffer_has_format(img, XPE_PIXEL_FLOAT32, &n)) return XPE_ERR_INVALID_INPUT;

    auto* px = static_cast<float*>(img->data);

    // REQ-P1A-033: compute time delta in units of frames (1.0 for first frame)
    const double acquisitionTimeSec = static_cast<double>(meta->acquisitionTime);
    double dt = (gh->lastAcqTimeSec > 0.0)
                ? (acquisitionTimeSec - gh->lastAcqTimeSec)
                : 1.0;
    if (dt <= 0.0) dt = 1.0; // guard against zero/negative dt
    gh->lastAcqTimeSec = acquisitionTimeSec;

    const float decay1 = static_cast<float>(std::exp(-dt / gh->tau1));
    const float decay2 = static_cast<float>(std::exp(-dt / gh->tau2));
    const float a1_base = static_cast<float>(gh->alpha1);
    const float a2_base = static_cast<float>(gh->alpha2);

    // Select tier based on handle configuration
    // REQ-P1A-032: apply LTI deconvolution (Tier 1/2/3)
    XpeErrorCode result = XPE_OK;
    switch (gh->tier) {
        case 1:
            result = ghost_tier1(gh, px, n, decay1, decay2, a1_base, a2_base);
            break;
        case 2:
            result = ghost_tier2(gh, px, n, decay1, decay2, a1_base, a2_base);
            break;
        case 3:
            result = ghost_tier3(gh, px, n, decay1, decay2, a1_base, a2_base);
            break;
        default:
            result = ghost_tier1(gh, px, n, decay1, decay2, a1_base, a2_base);
            break;
    }

    return result;
}

XpeErrorCode xpe_ghost_reset(void* handle)
{
    if (!GhostCorrectorHandle::isValid(handle)) return XPE_ERR_INVALID_INPUT;
    auto* gh = static_cast<GhostCorrectorHandle*>(handle);
    // REQ-P1A-034: clear accumulated frame history
    std::fill(gh->hist1.begin(), gh->hist1.end(), 0.0f);
    std::fill(gh->hist2.begin(), gh->hist2.end(), 0.0f);
    gh->lastAcqTimeSec = 0.0;
    gh->lastFrameMean = 0.0f;
    gh->exposureWeight = 1.0;
    return XPE_OK;
}

void xpe_ghost_destroy(void* handle)
{
    if (!GhostCorrectorHandle::isValid(handle)) return;
    auto* gh = static_cast<GhostCorrectorHandle*>(handle);
    gh->magic = 0; // invalidate before delete
    delete gh;
}
