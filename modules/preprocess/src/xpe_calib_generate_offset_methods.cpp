#include "xpe_calib_generate_offset_methods.hpp"

#include "xpe/preprocess/xcal_format.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace xpe::preprocess {
namespace {

// Minimal JSON field extractor. Does NOT handle escaped quotes (\") inside values.
// Safe for the current use case: method names are simple ASCII identifiers.
std::string json_get_string(const char* config_json, const char* key)
{
    if (!config_json || !key) return {};

    char needle[128];
    std::snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char* pos = std::strstr(config_json, needle);
    if (!pos) return {};

    pos += std::strlen(needle);
    while (*pos && (*pos == ' ' || *pos == '\t' || *pos == ':')) ++pos;
    if (*pos != '"') return {};

    ++pos;
    const char* end = std::strchr(pos, '"');
    if (!end) return {};

    return std::string(pos, end);
}

double json_get_double(const char* config_json, const char* key, double default_value)
{
    if (!config_json || !key) return default_value;

    char needle[128];
    std::snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char* pos = std::strstr(config_json, needle);
    if (!pos) return default_value;

    pos += std::strlen(needle);
    while (*pos && (*pos == ' ' || *pos == '\t' || *pos == ':')) ++pos;

    char* end = nullptr;
    const double val = std::strtod(pos, &end);
    return (end == pos) ? default_value : val;
}

XpeErrorCode validate_dark_frames(const XpeImageBuffer* dark_frames,
                                  int32_t num_frames,
                                  uint32_t* width_out,
                                  uint32_t* height_out,
                                  size_t* n_pixels_out) noexcept
{
    if (!dark_frames || !width_out || !height_out || !n_pixels_out) {
        return XPE_ERR_INVALID_INPUT;
    }
    if (num_frames <= 0) {
        return XPE_ERR_INVALID_INPUT;
    }

    const uint32_t width = dark_frames[0].width;
    const uint32_t height = dark_frames[0].height;
    if (width == 0 || height == 0 ||
        width > XCAL_MAX_DIM || height > XCAL_MAX_DIM) {
        return XPE_ERR_INVALID_INPUT;
    }

    const size_t n_pixels = static_cast<size_t>(width) * height;
    if (n_pixels > std::numeric_limits<size_t>::max() / sizeof(uint16_t)) {
        return XPE_ERR_INVALID_INPUT;
    }

    for (int32_t i = 0; i < num_frames; ++i) {
        const XpeImageBuffer& frame = dark_frames[i];
        if (frame.width != width || frame.height != height) {
            return XPE_ERR_INVALID_INPUT;
        }
        if (!frame.data) {
            return XPE_ERR_INVALID_INPUT;
        }
        if (frame.format != XPE_PIXEL_UINT16) {
            return XPE_ERR_UNSUPPORTED_FORMAT;
        }
        if (frame.dataSize < n_pixels * sizeof(uint16_t)) {
            return XPE_ERR_BUFFER_TOO_SMALL;
        }
    }

    *width_out = width;
    *height_out = height;
    *n_pixels_out = n_pixels;
    return XPE_OK;
}

double mean_of(const std::vector<double>& values) noexcept
{
    if (values.empty()) return 0.0;
    double sum = 0.0;
    for (double v : values) sum += v;
    return sum / static_cast<double>(values.size());
}

double stddev_of(const std::vector<double>& values, double mean) noexcept
{
    if (values.size() < 2u) return 0.0;
    double sum_sq = 0.0;
    for (double v : values) {
        const double d = v - mean;
        sum_sq += d * d;
    }
    // Sample stddev (Bessel's correction, /N-1): more accurate for small frame counts.
    return std::sqrt(sum_sq / static_cast<double>(values.size() - 1u));
}

double median_of(std::vector<double>& values) noexcept
{
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const size_t n = values.size();
    const size_t mid = n / 2u;
    if ((n % 2u) == 0u) {
        return (values[mid - 1u] + values[mid]) * 0.5;
    }
    return values[mid];
}

size_t percentile_index(double percentile, size_t n) noexcept
{
    if (n == 0u) return 0u;
    const double clamped = std::max(0.0, std::min(100.0, percentile));
    const double pos = (clamped / 100.0) * static_cast<double>(n - 1u);
    return static_cast<size_t>(std::floor(pos + 0.5));
}

double sigma_clip_value(const std::vector<double>& samples,
                        const OffsetGenerationConfig& config)
{
    std::vector<double> clipped = samples;
    for (int32_t iter = 0; iter < config.max_iter && clipped.size() > 1u; ++iter) {
        const double mean = mean_of(clipped);
        const double sigma = stddev_of(clipped, mean);
        if (sigma <= 1e-12 || !std::isfinite(sigma)) break;

        const double threshold = config.sigma * sigma;
        auto new_end = std::remove_if(clipped.begin(), clipped.end(),
            [&](double v) { return std::abs(v - mean) > threshold; });
        if (new_end == clipped.end()) break;
        clipped.erase(new_end, clipped.end());
    }

    if (clipped.empty()) {
        return mean_of(samples);
    }
    return mean_of(clipped);
}

double winsor_value(std::vector<double>& samples,
                    const OffsetGenerationConfig& config)
{
    if (samples.empty()) return 0.0;
    std::sort(samples.begin(), samples.end());
    const size_t lo_idx = percentile_index(config.lower_percentile, samples.size());
    const size_t hi_idx = percentile_index(config.upper_percentile, samples.size());
    const double lo = samples[lo_idx];
    const double hi = samples[hi_idx];

    double sum = 0.0;
    for (double v : samples) {
        sum += std::max(lo, std::min(hi, v));
    }
    return sum / static_cast<double>(samples.size());
}

uint16_t round_to_u16(float value) noexcept
{
    if (!std::isfinite(value) || value <= 0.0f) return 0u;
    if (value >= 65535.0f) return 65535u;
    return static_cast<uint16_t>(std::floor(value + 0.5f));
}

} // namespace

const char* offset_generation_method_name(OffsetGenerationMethod method) noexcept
{
    switch (method) {
    case OffsetGenerationMethod::Mean: return "mean";
    case OffsetGenerationMethod::Median: return "median";
    case OffsetGenerationMethod::SigmaClip: return "sigma_clip";
    case OffsetGenerationMethod::Winsor: return "winsor";
    }
    return "mean";
}

XpeErrorCode parse_offset_generation_config(const char* config_json,
                                            OffsetGenerationConfig* config) noexcept
{
    if (!config) return XPE_ERR_INVALID_INPUT;
    *config = OffsetGenerationConfig{};

    if (!config_json) return XPE_OK;

    const std::string method = json_get_string(config_json, "method");
    if (method.empty() || method == "mean") {
        config->method = OffsetGenerationMethod::Mean;
    } else if (method == "median") {
        config->method = OffsetGenerationMethod::Median;
    } else if (method == "sigma_clip") {
        config->method = OffsetGenerationMethod::SigmaClip;
    } else if (method == "winsor") {
        config->method = OffsetGenerationMethod::Winsor;
    } else {
        return XPE_ERR_CONFIG_INVALID;
    }

    config->sigma = json_get_double(config_json, "sigma", config->sigma);
    const double max_iter = json_get_double(config_json, "max_iter",
                                           static_cast<double>(config->max_iter));
    config->max_iter = static_cast<int32_t>(max_iter);
    config->lower_percentile = json_get_double(
        config_json, "lower_percentile", config->lower_percentile);
    config->upper_percentile = json_get_double(
        config_json, "upper_percentile", config->upper_percentile);

    if (!std::isfinite(config->sigma) || config->sigma <= 0.0) {
        return XPE_ERR_CONFIG_INVALID;
    }
    if (config->max_iter <= 0 || config->max_iter > 100) {
        return XPE_ERR_CONFIG_INVALID;
    }
    if (!std::isfinite(config->lower_percentile) ||
        !std::isfinite(config->upper_percentile) ||
        config->lower_percentile < 0.0 ||
        config->upper_percentile > 100.0 ||
        config->lower_percentile > config->upper_percentile) {
        return XPE_ERR_CONFIG_INVALID;
    }

    return XPE_OK;
}

// @MX:NOTE: [AUTO] generate_offset_values -- per-pixel statistical aggregation dispatcher
// @MX:REASON: fan_in=2 (generate_offset_to_uint16_buffer, xpe_calib_generate_offset); not noexcept — vector alloc may throw
XpeErrorCode generate_offset_values(const XpeImageBuffer* dark_frames,
                                    int32_t num_frames,
                                    const OffsetGenerationConfig& config,
                                    std::vector<float>* result_out,
                                    uint32_t* width_out,
                                    uint32_t* height_out)
{
    if (!result_out || !width_out || !height_out) {
        return XPE_ERR_INVALID_INPUT;
    }

    uint32_t width = 0;
    uint32_t height = 0;
    size_t n_pixels = 0;
    XpeErrorCode rc = validate_dark_frames(dark_frames, num_frames,
                                           &width, &height, &n_pixels);
    if (rc != XPE_OK) return rc;

    result_out->assign(n_pixels, 0.0f);

    if (config.method == OffsetGenerationMethod::Mean) {
        std::vector<double> accum(n_pixels, 0.0);
        for (int32_t i = 0; i < num_frames; ++i) {
            const uint16_t* src = static_cast<const uint16_t*>(dark_frames[i].data);
            for (size_t j = 0; j < n_pixels; ++j) {
                accum[j] += static_cast<double>(src[j]);
            }
        }

        const double inv_n = 1.0 / static_cast<double>(num_frames);
        for (size_t j = 0; j < n_pixels; ++j) {
            const float v = static_cast<float>(accum[j] * inv_n);
            (*result_out)[j] = std::isfinite(v) ? v : 0.0f;
        }
    } else {
        std::vector<double> samples(static_cast<size_t>(num_frames));
        for (size_t pixel = 0; pixel < n_pixels; ++pixel) {
            for (int32_t frame = 0; frame < num_frames; ++frame) {
                const uint16_t* src = static_cast<const uint16_t*>(dark_frames[frame].data);
                samples[static_cast<size_t>(frame)] = static_cast<double>(src[pixel]);
            }

            double value = 0.0;
            switch (config.method) {
            case OffsetGenerationMethod::Median: {
                std::vector<double> sorted = samples;
                value = median_of(sorted);
                break;
            }
            case OffsetGenerationMethod::SigmaClip:
                value = sigma_clip_value(samples, config);
                break;
            case OffsetGenerationMethod::Winsor: {
                std::vector<double> sorted = samples;
                value = winsor_value(sorted, config);
                break;
            }
            case OffsetGenerationMethod::Mean:
                value = mean_of(samples);
                break;
            }

            const float v = static_cast<float>(value);
            (*result_out)[pixel] = std::isfinite(v) ? v : 0.0f;
        }
    }

    *width_out = width;
    *height_out = height;
    return XPE_OK;
}

// noexcept: all internal exceptions caught; generate_offset_values may throw std::bad_alloc
// which is captured by catch(const std::bad_alloc&) below.
XpeErrorCode generate_offset_to_uint16_buffer(const XpeImageBuffer* dark_frames,
                                              int32_t num_frames,
                                              XpeImageBuffer* output,
                                              const char* config_json_or_null) noexcept
{
    try {
        if (!output || !output->data) {
            return XPE_ERR_INVALID_INPUT;
        }
        if (output->format != XPE_PIXEL_UINT16) {
            return XPE_ERR_UNSUPPORTED_FORMAT;
        }

        OffsetGenerationConfig config;
        XpeErrorCode rc = parse_offset_generation_config(config_json_or_null, &config);
        if (rc != XPE_OK) return rc;

        std::vector<float> result;
        uint32_t width = 0;
        uint32_t height = 0;
        rc = generate_offset_values(dark_frames, num_frames, config,
                                    &result, &width, &height);
        if (rc != XPE_OK) return rc;

        if (output->width != width || output->height != height) {
            return XPE_ERR_BUFFER_TOO_SMALL;
        }
        if (output->dataSize < result.size() * sizeof(uint16_t)) {
            return XPE_ERR_BUFFER_TOO_SMALL;
        }

        uint16_t* dst = static_cast<uint16_t*>(output->data);
        for (size_t i = 0; i < result.size(); ++i) {
            dst[i] = round_to_u16(result[i]);
        }

        output->bitsAllocated = 16u;
        output->bitsStored = 16u;
        output->format = XPE_PIXEL_UINT16;
        output->dataSize = result.size() * sizeof(uint16_t);
        return XPE_OK;
    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

} // namespace xpe::preprocess
