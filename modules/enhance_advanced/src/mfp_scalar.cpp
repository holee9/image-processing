#include "mfp_scalar.h"
#include "xpe/common/xpe_error.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace xpe {
namespace enhance_advanced {

// ============================================================================
// MfpConfig Implementation
// ============================================================================

MfpConfig MfpConfig::fromJson(const char* jsonConfig) {
    MfpConfig config;  // Use defaults

    if (jsonConfig == nullptr) {
        return config;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(jsonConfig);

        // Parse MFP section if present
        if (j.contains("mfp")) {
            auto mfp = j["mfp"];

            if (mfp.contains("num_levels")) {
                config.numLevels = mfp["num_levels"];
                // Clamp to reasonable range [1, 6]
                config.numLevels = std::max(1, std::min(6, config.numLevels));
            }

            if (mfp.contains("edge_gain")) {
                config.edgeGain = mfp["edge_gain"];
            }

            if (mfp.contains("texture_gain")) {
                config.textureGain = mfp["texture_gain"];
            }

            if (mfp.contains("flat_gain")) {
                config.flatGain = mfp["flat_gain"];
            }

            if (mfp.contains("noise_threshold")) {
                config.noiseThreshold = mfp["noise_threshold"];
            }
        }
    } catch (const nlohmann::json::exception&) {
        // Return defaults on JSON parse error
    }

    return config;
}

// ============================================================================
// LaplacianPyramid Implementation
// ============================================================================

LaplacianPyramid::LaplacianPyramid(const float* data, int width, int height, int numLevels)
    : width_(width), height_(height), numLevels_(numLevels) {

    levels_.resize(numLevels);

    // Level 0: Store original (will become Laplacian after processing)
    levels_[0].resize(width * height);
    std::memcpy(levels_[0].data(), data, width * height * sizeof(float));

    // Build pyramid: Gaussian blur + downsample for each level
    std::vector<float> currentLevel = levels_[0];

    for (int level = 0; level < numLevels - 1; ++level) {
        int currentW = width >> level;
        int currentH = height >> level;

        // Apply Gaussian blur
        gaussianBlur(currentLevel.data(), currentW, currentH);

        // Store Laplacian (detail - blurred)
        levels_[level] = currentLevel;

        // Downsample for next level
        int nextW = std::max(1, currentW / 2);
        int nextH = std::max(1, currentH / 2);
        levels_[level + 1].resize(nextW * nextH);

        downsample(currentLevel.data(), levels_[level + 1].data(), currentW, currentH);

        // Prepare for next iteration
        currentLevel = levels_[level + 1];
    }

    // Convert to Laplacian pyramid: L(i) = G(i) - expand(G(i+1))
    for (int level = 0; level < numLevels - 1; ++level) {
        int w = width >> level;
        int h = height >> level;
        int nextW = std::max(1, w / 2);
        int nextH = std::max(1, h / 2);

        // Upsample next level
        std::vector<float> upsampled(w * h);
        upsample(levels_[level + 1].data(), upsampled.data(), nextW, nextH);

        // Subtract to get Laplacian
        for (int i = 0; i < w * h; ++i) {
            levels_[level][i] -= upsampled[i];
        }
    }
}

void LaplacianPyramid::reconstruct(const MfpConfig& config, float* outData) {
    // Start with coarsest level (Gaussian, not Laplacian)
    std::vector<float> reconstructed = levels_[numLevels_ - 1];

    // Reconstruct from coarsest to finest
    for (int level = numLevels_ - 2; level >= 0; --level) {
        int w = width_ >> level;
        int h = height_ >> level;

        // Upsample current reconstruction
        std::vector<float> upsampled(w * h);
        int currentW = reconstructed.size() > 0 ?
            static_cast<int>(std::sqrt(reconstructed.size())) : 1;
        int currentH = reconstructed.size() / currentW;

        upsample(reconstructed.data(), upsampled.data(), currentW, currentH);

        // Add enhanced Laplacian detail
        float gain = 1.0f;
        if (level == numLevels_ - 2) {
            gain = config.flatGain;      // Coarsest details
        } else if (level == 0) {
            gain = config.edgeGain;      // Finest details (edges)
        } else {
            gain = config.textureGain;   // Mid-level details (texture)
        }

        for (int i = 0; i < w * h; ++i) {
            // Apply noise threshold
            float detail = levels_[level][i];
            if (std::abs(detail) < config.noiseThreshold) {
                detail = 0.0f;
            }

            upsampled[i] += detail * gain;
        }

        reconstructed = upsampled;
    }

    // Copy to output
    std::memcpy(outData, reconstructed.data(), width_ * height_ * sizeof(float));
}

void LaplacianPyramid::gaussianBlur(float* data, int width, int height) {
    // 5x5 Gaussian kernel (separable)
    // Kernel: [1/16, 1/4, 3/8, 1/4, 1/16] (normalized)
    const float kernel[5] = {0.0625f, 0.25f, 0.375f, 0.25f, 0.0625f};

    std::vector<float> temp(width * height);

    // Horizontal pass
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float sum = 0.0f;
            for (int k = -2; k <= 2; ++k) {
                int xk = std::min(width - 1, std::max(0, x + k));
                sum += data[y * width + xk] * kernel[k + 2];
            }
            temp[y * width + x] = sum;
        }
    }

    // Vertical pass
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float sum = 0.0f;
            for (int k = -2; k <= 2; ++k) {
                int yk = std::min(height - 1, std::max(0, y + k));
                sum += temp[yk * width + x] * kernel[k + 2];
            }
            data[y * width + x] = sum;
        }
    }
}

void LaplacianPyramid::downsample(const float* in, float* out, int width, int height) {
    int outW = std::max(1, width / 2);
    int outH = std::max(1, height / 2);

    for (int y = 0; y < outH; ++y) {
        for (int x = 0; x < outW; ++x) {
            // Average 2x2 block
            int inX = x * 2;
            int inY = y * 2;

            float sum = 0.0f;
            int count = 0;

            for (int dy = 0; dy < 2 && inY + dy < height; ++dy) {
                for (int dx = 0; dx < 2 && inX + dx < width; ++dx) {
                    sum += in[(inY + dy) * width + (inX + dx)];
                    ++count;
                }
            }

            out[y * outW + x] = sum / static_cast<float>(count);
        }
    }
}

void LaplacianPyramid::upsample(const float* in, float* out, int width, int height) {
    int outW = width * 2;
    int outH = height * 2;

    for (int y = 0; y < outH; ++y) {
        for (int x = 0; x < outW; ++x) {
            // Nearest neighbor upsampling
            int inX = std::min(width - 1, x / 2);
            int inY = std::min(height - 1, y / 2);
            out[y * outW + x] = in[inY * width + inX];
        }
    }
}

// ============================================================================
// Public API Implementation
// ============================================================================

XpeErrorCode applyMfpScalar(XpeImageBuffer* img, const MfpConfig& config) {
    if (img == nullptr || img->data == nullptr) {
        return XPE_ERR_INVALID_INPUT;
    }

    if (img->format != XPE_PIXEL_FLOAT32) {
        return XPE_ERR_UNSUPPORTED_FORMAT;
    }

    if (img->width <= 0 || img->height <= 0) {
        return XPE_ERR_INVALID_INPUT;
    }

    try {
        // Build Laplacian pyramid
        LaplacianPyramid pyramid(
            static_cast<const float*>(img->data),
            img->width,
            img->height,
            config.numLevels
        );

        // Reconstruct with enhancement
        pyramid.reconstruct(config, static_cast<float*>(img->data));

        return XPE_OK;
    } catch (const std::exception&) {
        return XPE_ERR_INTERNAL;
    }
}

} // namespace enhance_advanced
} // namespace xpe
