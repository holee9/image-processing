/**
 * @file temp_compensate.cpp
 * @brief SWU-1.6: Temperature compensation for dark current (PRE-07)
 *        Model: I_dark(T) = I_0 * exp(-E_g / (2 * k_B * T))
 *        REQ-P1A-005 to REQ-P1A-008
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cmath>

// Silicon bandgap energy (eV) and Boltzmann constant (eV/K)
static constexpr double kEgSi   = 1.12;          // eV
static constexpr double kBoltzV = 8.617333e-5;   // eV/K
static constexpr float  kTempFallback = 25.0f;   // Celsius fallback (REQ-P1A-007)
static constexpr float  kTempMin = -20.0f;
static constexpr float  kTempMax =  60.0f;

// @MX:ANCHOR: [AUTO] xpe_temp_compensate — temperature-dependent dark current scaling
// @MX:REASON: Public API boundary; called in main pipeline chain; fan_in >= 3
// @MX:SPEC: REQ-P1A-005
XpeErrorCode xpe_temp_compensate(XpeImageBuffer* img,
                                  float detectorTempC,
                                  const char* configJsonOrNull)
{
    if (!img) return XPE_ERR_INVALID_INPUT;
    size_t n = 0;
    if (!xpe_buffer_has_format(img, XPE_PIXEL_UINT16, &n)) return XPE_ERR_INVALID_INPUT;

    // REQ-P1A-007: NaN -> use 25.0C fallback
    if (std::isnan(detectorTempC)) detectorTempC = kTempFallback;

    // REQ-P1A-008: temp out of [-20, +60] range -> XPE_ERR_INVALID_INPUT
    if (detectorTempC < kTempMin || detectorTempC > kTempMax)
        return XPE_ERR_INVALID_INPUT;

    // Compute dark current scale factor relative to T_ref=25C
    // scale = exp(-Eg / 2kT) / exp(-Eg / 2kT_ref)
    const double T     = static_cast<double>(detectorTempC) + 273.15;
    const double T_ref = 25.0 + 273.15;
    const double exp_T   = std::exp(-kEgSi / (2.0 * kBoltzV * T));
    const double exp_ref = std::exp(-kEgSi / (2.0 * kBoltzV * T_ref));

    // Avoid division by zero (physically impossible but guard defensively)
    if (exp_ref < 1e-300) return XPE_OK;

    const float scale = static_cast<float>(exp_T / exp_ref);

    // REQ-P1A-005: apply correction — divide by scale to compensate dark current
    auto* px = static_cast<uint16_t*>(img->data);
    for (size_t i = 0; i < n; ++i) {
        const float corrected = static_cast<float>(px[i]) / (scale > 0.f ? scale : 1.f);
        px[i] = static_cast<uint16_t>(std::min(corrected, 65535.0f));
    }

    (void)configJsonOrNull;
    return XPE_OK;
}
