#pragma once

#include "xpe/preprocess_api.h"

#include <cstdint>
#include <vector>

#if defined(_WIN32)
#define XPE_PREPROCESS_INTERNAL_API
#else
#define XPE_PREPROCESS_INTERNAL_API __attribute__((visibility("hidden")))
#endif

namespace xpe::preprocess {

enum class OffsetGenerationMethod {
    Mean,
    Median,
    SigmaClip,
    Winsor
};

struct OffsetGenerationConfig {
    OffsetGenerationMethod method{OffsetGenerationMethod::Mean};
    double sigma{3.0};
    int32_t max_iter{5};
    double lower_percentile{5.0};
    double upper_percentile{95.0};
};

XPE_PREPROCESS_INTERNAL_API const char* offset_generation_method_name(
    OffsetGenerationMethod method) noexcept;

XPE_PREPROCESS_INTERNAL_API XpeErrorCode parse_offset_generation_config(
    const char* config_json,
    OffsetGenerationConfig* config) noexcept;

/**
 * @brief XPE-ALG-001 9.8.2.1 minimum-frame floor: N_min = max(3, floor(N/4)).
 *
 * 9.8.5 clamps it for a very small series ("N < 4 -> min_frames = N"), which is
 * a no-op at N = 3 (max(3, 0) = 3 = N) and only bites at N < 3.
 */
XPE_PREPROCESS_INTERNAL_API size_t sigma_clip_min_frames(size_t num_frames) noexcept;

/**
 * @brief Compute the offset map, and optionally the 9.8.2.1 static-defect mask.
 *
 * @param defect_mask_out When non-null AND the method is SigmaClip, receives one
 *        byte per pixel: 1 where the surviving set fell below N_min, else 0.
 *        Cleared to all-zero for every other method -- no other method has an
 *        N_min clause, so inventing marks for them would be a fabrication.
 */
XPE_PREPROCESS_INTERNAL_API XpeErrorCode generate_offset_values(
    const XpeImageBuffer* dark_frames,
    int32_t num_frames,
    const OffsetGenerationConfig& config,
    std::vector<float>* result_out,
    uint32_t* width_out,
    uint32_t* height_out,
    std::vector<uint8_t>* defect_mask_out = nullptr);

/**
 * @brief OR the set bits of @p mask into @p dst, leaving every other bit alone.
 *
 * The merge half of leader decision #138 (a), kept free of the global store so
 * it is reachable from the test binary: g_calib lives in the DLL and is not
 * exported, so a function that touches it cannot be linked from a test that
 * compiles this TU directly (measured -- LNK2001 on g_calib and g_calib_mutex).
 *
 * @return the number of bits this call newly set.
 */
XPE_PREPROCESS_INTERNAL_API size_t or_merge_defect_bits(
    uint8_t* dst,
    const std::vector<uint8_t>& mask,
    size_t n_pixels) noexcept;

/**
 * @brief OR-merge a static-defect mask into the global calibration store.
 *
 * Leader decision #138 (a). An absent map is created zero-filled first; an
 * existing map of the same dimensions keeps every bit it already had. A map of
 * DIFFERENT dimensions is a mismatch, not something to resize silently --
 * reported as XPE_ERR_CONFIG_INVALID. A mask marking no pixel is a no-op: a run
 * that finds nothing must not conjure a defect map where the caller had none.
 *
 * Defined in xpe_calib_generate_offset.cpp (DLL only) for the linkage reason
 * above.
 */
XPE_PREPROCESS_INTERNAL_API XpeErrorCode merge_static_defect_mask(
    const std::vector<uint8_t>& mask,
    uint32_t width,
    uint32_t height) noexcept;

/**
 * @brief Generate an offset map straight into a caller-supplied UINT16 buffer.
 *
 * @param defect_mask_out When non-null, receives the 9.8.2.1 static-defect mask
 *        (see generate_offset_values). Merging it into the global store is the
 *        DLL-side caller's job -- see merge_static_defect_mask.
 */
XPE_PREPROCESS_INTERNAL_API XpeErrorCode generate_offset_to_uint16_buffer(
    const XpeImageBuffer* dark_frames,
    int32_t num_frames,
    XpeImageBuffer* output,
    const char* config_json_or_null,
    std::vector<uint8_t>* defect_mask_out = nullptr) noexcept;

} // namespace xpe::preprocess

// Intentionally undefine after declarations: prevents macro leaking into including TUs.
// All declarations using this macro are complete above; consumers use the declared symbols only.
#undef XPE_PREPROCESS_INTERNAL_API
