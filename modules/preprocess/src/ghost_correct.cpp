/**
 * @file ghost_correct.cpp
 * @brief SWU-1.4: Ghost/Lag Correction Tier 1/2/3 — LTI/NLCSC deconvolution (PRE-04)
 *        Dual-exponential IRF model (ref: PMC3465354)
 *        Tier 1: LTI deconvolution
 *        Tier 2: Exposure-weighted LTI
 *        Tier 3: NLCSC (Nonlinear Causal Spatial Context)
 *        REQ-P1A-085 to REQ-P1A-087
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include "xpe_strict_parse.hpp"

#include <cstdlib>
#include <memory>
#include <new>
#include <string>
#include <cstring>
#include <cmath>
#include <algorithm>

// @MX:ANCHOR: [AUTO] xpe_ghost_create — resource allocation for ghost corrector
// @MX:REASON: All ghost functions fan in; handle is the invariant contract point
// @MX:SPEC: REQ-P1A-085
XpeErrorCode xpe_ghost_create(uint32_t width, uint32_t height,
                               const char* configJsonOrNull,
                               void** handleOut)
{
    if (!handleOut || width == 0 || height == 0) return XPE_ERR_INVALID_INPUT;

    // REQ-P1A-085: allocate handle with frame history buffer
    // REQ-P1A-030: configJsonOrNull for IRF coefficient override
    // REQ-P1A-031: XPE_ERR_OUT_OF_MEMORY on allocation failure
    // QA-A-202 (#233): the handle is owned by `owner` until every step has succeeded, so a refusal or an
    // exception at any step frees it (it used to leak the handle and its history buffers when a number in
    // the configuration was malformed). A malformed number is XPE_ERR_CONFIG_INVALID; nothing is handed back.
    std::unique_ptr<GhostCorrectorHandle> owner(new (std::nothrow) GhostCorrectorHandle());
    if (!owner) return XPE_ERR_OUT_OF_MEMORY;
    GhostCorrectorHandle* const handle = owner.get();

    handle->width  = width;
    handle->height = height;
    const size_t pixelCount = static_cast<size_t>(width) * height;

    try {
        // Parse config JSON for tier and IRF coefficients. An absent or empty value keeps the default. The text is
        // parsed once into its top-level members; one that is not one valid JSON object, is empty, or gives a member
        // name twice is a refusal (QA-A-209, QA-A-209b). NULL is every default.
        {
            XpeConfigDoc doc;
            XpeErrorCode rc = xpe_config_parse(configJsonOrNull, &doc);
            if (rc != XPE_OK) return rc;

            std::string v;
            if (doc.getString("tier", &v) && !v.empty()) {
                int32_t tier = 1;
                if (!xpe_strict::parse_int(v, &tier)) return XPE_ERR_CONFIG_INVALID;
                handle->tier = (tier < 1 || tier > 3) ? 1 : tier;
            }
            const struct { const char* key; double* dst; } reals[] = {
                {"alpha1", &handle->alpha1}, {"tau1", &handle->tau1}, {"alpha2", &handle->alpha2},
                {"tau2", &handle->tau2}, {"tier2Threshold", &handle->tier2Threshold}, {"nlcscBeta", &handle->nlcscBeta},
            };
            for (const auto& r : reals) {
                if (doc.getString(r.key, &v) && !v.empty() && !xpe_strict::parse_double(v, r.dst))
                    return XPE_ERR_CONFIG_INVALID;
            }
        }

        handle->hist1.assign(pixelCount, 0.0f);
        handle->hist2.assign(pixelCount, 0.0f);
        handle->next1.assign(pixelCount, 0.0f);
        handle->next2.assign(pixelCount, 0.0f);
        handle->backup.assign(pixelCount, 0.0f);
    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }

    *handleOut = owner.release();
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

    // What a tier works out about the frame as a whole. It is committed to the handle with the history, and
    // only when the frame succeeded (QA-A-202c).
    struct FrameStats {
        bool  set{false};
        float meanSignal{0.0f};
        float exposureWeight{1.0f};
    };

    // Tier 1: Standard LTI deconvolution
    XpeErrorCode ghost_tier1(GhostCorrectorHandle* gh, float* px, size_t n,
                             float decay1, float decay2, float a1, float a2, FrameStats*) {
        const float* h1 = gh->hist1.data();
        const float* h2 = gh->hist2.data();
        float* n1 = gh->next1.data();
        float* n2 = gh->next2.data();
        for (size_t i = 0; i < n; ++i) {
            const float raw = px[i];
            if (!std::isfinite(raw)) return XPE_ERR_PROCESSING_FAILED;
            float corrected = raw - a1 * h1[i] - a2 * h2[i];
            n1[i] = decay1 * h1[i] + raw;
            n2[i] = decay2 * h2[i] + raw;
            if (!std::isfinite(corrected) || !std::isfinite(n1[i]) || !std::isfinite(n2[i])) return XPE_ERR_PROCESSING_FAILED;
            px[i] = (corrected > 0.0f) ? corrected : 0.0f;
        }
        return XPE_OK;
    }

    // Tier 2: Exposure-weighted LTI deconvolution
    XpeErrorCode ghost_tier2(GhostCorrectorHandle* gh, float* px, size_t n,
                             float decay1, float decay2, float a1_base, float a2_base, FrameStats* stats) {
        // Exposure-weighted coefficients based on frame mean
        const float meanSignal = compute_frame_mean(px, n);

        // Scale alpha based on exposure level (higher exposure = stronger correction)
        // Reference: 50% of saturation = 1.0x scaling, 100% saturation = 1.5x scaling
        const float exposureWeight = 1.0f + (meanSignal / 32768.0f) * 0.5f;
        *stats = FrameStats{true, meanSignal, exposureWeight};

        const float a1 = a1_base * exposureWeight;
        const float a2 = a2_base * exposureWeight;

        const float* h1 = gh->hist1.data();
        const float* h2 = gh->hist2.data();
        float* n1 = gh->next1.data();
        float* n2 = gh->next2.data();
        for (size_t i = 0; i < n; ++i) {
            const float raw = px[i];
            if (!std::isfinite(raw)) return XPE_ERR_PROCESSING_FAILED;
            float corrected = raw - a1 * h1[i] - a2 * h2[i];
            n1[i] = decay1 * h1[i] + raw;
            n2[i] = decay2 * h2[i] + raw;
            if (!std::isfinite(corrected) || !std::isfinite(n1[i]) || !std::isfinite(n2[i])) return XPE_ERR_PROCESSING_FAILED;
            px[i] = (corrected > 0.0f) ? corrected : 0.0f;
        }
        return XPE_OK;
    }

    // Tier 3: NLCSC (Nonlinear Causal Spatial Context) with signal-dependent coefficients
    XpeErrorCode ghost_tier3(GhostCorrectorHandle* gh, float* px, size_t n,
                             float decay1, float decay2, float a1_base, float a2_base, FrameStats* stats) {
        const float meanSignal = compute_frame_mean(px, n);
        const float exposureWeight = 1.0f + (meanSignal / 32768.0f) * 0.5f;
        *stats = FrameStats{true, meanSignal, exposureWeight};

        const float* h1 = gh->hist1.data();
        const float* h2 = gh->hist2.data();
        float* n1 = gh->next1.data();
        float* n2 = gh->next2.data();

        const float beta = static_cast<float>(gh->nlcscBeta); // signal dependency parameter
        const uint32_t W = gh->width;
        const uint32_t H = gh->height;

        // QA-A-218b (#233): the 3x3 mean reads the frame AS IT CAME IN, not the pixels this loop has already overwritten.
        // The loop updates px in place, so reading px made the pixels above and to the left count with their CORRECTED
        // (and zero-clamped) values and the others with their original ones: the output depended on the scan direction
        // (a frame flipped, processed and flipped back differed in 41-91 % of its pixels, QA-A-218). xpe_ghost_correct
        // keeps a copy of the incoming frame in gh->backup for the whole call (QA-A-217), which is exactly that.
        const float* const src = gh->backup.data();

        // Apply NLCSC with signal-dependent coefficients
        for (size_t i = 0; i < n; ++i) {
            const float raw = px[i];
            if (!std::isfinite(raw)) return XPE_ERR_PROCESSING_FAILED;

            // Signal-dependent coefficient: higher signal = stronger correction
            const float signalDependence = 1.0f + beta * (raw / 32768.0f);
            const float a1 = a1_base * exposureWeight * signalDependence;
            const float a2 = a2_base * exposureWeight * signalDependence;

            float corrected = raw - a1 * h1[i] - a2 * h2[i];

            // Spatial context: blend with local neighborhood mean (3x3)
            if (W >= 3u && H >= 3u) {
                const uint32_t x = static_cast<uint32_t>(i % W);
                const uint32_t y = static_cast<uint32_t>(i / W);

                if (x > 0u && x < W - 1u && y > 0u && y < H - 1u) {
                    float localMean = 0.0f;
                    int count = 0;
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            const size_t ni = static_cast<size_t>(static_cast<int>(y) + dy) * static_cast<size_t>(W) + static_cast<size_t>(static_cast<int>(x) + dx);
                            if (ni < n && std::isfinite(src[ni])) {
                                localMean += src[ni];
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

            n1[i] = decay1 * h1[i] + raw;
            n2[i] = decay2 * h2[i] + raw;

            if (!std::isfinite(corrected) || !std::isfinite(n1[i]) || !std::isfinite(n2[i])) return XPE_ERR_PROCESSING_FAILED;
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

    // SRS-CALIB-NFR-003: one call at a time per handle (history and lastAcqTimeSec are updated in place)
    std::lock_guard<std::mutex> lock(gh->mtx);

    auto* px = static_cast<float*>(img->data);

    // QA-A-217 (#233): A NON-FINITE FRAME IS REFUSED AT THE ENTRANCE, before anything is written. The tiers used to
    // work through the frame pixel by pixel and give up with XPE_ERR_PROCESSING_FAILED at the first NaN or infinity,
    // with every pixel before it already corrected (REQ-P1A-032 asks for the output to be left unmodified on
    // failure). Same rule as the defect, binning and runtime-detection stages (QA-A-214b, QA-A-215): INVALID_INPUT,
    // the buffer and the handle untouched, one Error alert with the count and the first pixel. (The history was
    // never at risk -- it is committed only for a whole successful frame, QA-A-202c; the test pins that.)
    {
        size_t count = 0, first = 0;
        if (xpe_find_nonfinite(px, n, &count, &first)) {
            xpe_alert_nonfinite("XPE_WARN_GHOST_INPUT_NOT_FINITE:", count, first, img->width,
                                "the frame was not corrected; the buffer and the handle's history were not changed");
            return XPE_ERR_INVALID_INPUT;
        }
    }

    // REQ-P1A-033: compute time delta in units of frames (1.0 for first frame)
    const double acquisitionTimeSec = static_cast<double>(meta->acquisitionTime);
    double dt = (gh->lastAcqTimeSec > 0.0)
                ? (acquisitionTimeSec - gh->lastAcqTimeSec)
                : 1.0;
    if (dt <= 0.0) dt = 1.0; // guard against zero/negative dt
    // gh->lastAcqTimeSec is set below, with the history, once the frame has succeeded (QA-A-202c).

    const float decay1 = static_cast<float>(std::exp(-dt / gh->tau1));
    const float decay2 = static_cast<float>(std::exp(-dt / gh->tau2));
    const float a1_base = static_cast<float>(gh->alpha1);
    const float a2_base = static_cast<float>(gh->alpha2);

    // Select tier based on handle configuration
    // REQ-P1A-032: apply LTI deconvolution (Tier 1/2/3)
    XpeErrorCode result = XPE_OK;
    FrameStats stats;
    // A frame can still fail with a finite input (a corrected value, or the new history, overflows float at the
    // extremes of the range): the pixels are put back as they came in, so a failure leaves the buffer unmodified
    // (REQ-P1A-032) and the history, which only a successful frame commits, as it was. QA-A-217.
    std::memcpy(gh->backup.data(), px, n * sizeof(float));
    switch (gh->tier) {
        case 1:
            result = ghost_tier1(gh, px, n, decay1, decay2, a1_base, a2_base, &stats);
            break;
        case 2:
            result = ghost_tier2(gh, px, n, decay1, decay2, a1_base, a2_base, &stats);
            break;
        case 3:
            result = ghost_tier3(gh, px, n, decay1, decay2, a1_base, a2_base, &stats);
            break;
        default:
            result = ghost_tier1(gh, px, n, decay1, decay2, a1_base, a2_base, &stats);
            break;
    }
    if (result != XPE_OK) {
        // the history, the time and the exposure estimate are as they were; the pixels are put back
        std::memcpy(px, gh->backup.data(), n * sizeof(float));
        return result;
    }

    // The whole frame succeeded: its history, its time and its exposure estimate become the handle's.
    gh->hist1.swap(gh->next1);
    gh->hist2.swap(gh->next2);
    gh->lastAcqTimeSec = acquisitionTimeSec;
    if (stats.set) {
        gh->lastFrameMean = stats.meanSignal;
        gh->exposureWeight = stats.exposureWeight;
    }
    return XPE_OK;
}

XpeErrorCode xpe_ghost_reset(void* handle)
{
    if (!GhostCorrectorHandle::isValid(handle)) return XPE_ERR_INVALID_INPUT;
    auto* gh = static_cast<GhostCorrectorHandle*>(handle);
    std::lock_guard<std::mutex> lock(gh->mtx);   // SRS-CALIB-NFR-003
    // REQ-P1A-088: clear accumulated frame history
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
