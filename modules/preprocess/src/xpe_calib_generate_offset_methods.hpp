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

XPE_PREPROCESS_INTERNAL_API XpeErrorCode generate_offset_values(
    const XpeImageBuffer* dark_frames,
    int32_t num_frames,
    const OffsetGenerationConfig& config,
    std::vector<float>* result_out,
    uint32_t* width_out,
    uint32_t* height_out);

XPE_PREPROCESS_INTERNAL_API XpeErrorCode generate_offset_to_uint16_buffer(
    const XpeImageBuffer* dark_frames,
    int32_t num_frames,
    XpeImageBuffer* output,
    const char* config_json_or_null) noexcept;

} // namespace xpe::preprocess

// Intentionally undefine after declarations: prevents macro leaking into including TUs.
// All declarations using this macro are complete above; consumers use the declared symbols only.
#undef XPE_PREPROCESS_INTERNAL_API
