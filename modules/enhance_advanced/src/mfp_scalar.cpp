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

    // @MX:NOTE: Gaussian pyramid -- blur applied to COPY before downsample, not in-place.
    // @MX:REASON: In-place blur corrupted G(i) before Laplacian subtraction, breaking
    //             the reconstruction identity G(i) = upsample(G(i+1)) + L(i).
    //             With blur-on-copy, L(i) = G(i) - upsample(G(i+1)) where G(i) is the
    //             unblurred level, so round-trip with gain=1 is mathematically exact.
    // @MX:SPEC: REQ-ADV-050 identity reconstruction fidelity
    levels_.resize(numLevels);

    // Build Gaussian pyramid first
    std::vector<std::vector<float>> gaussianPyramid(numLevels);

    // Level 0: Original image (unmodified)
    gaussianPyramid[0].resize(width * height);
    std::memcpy(gaussianPyramid[0].data(), data, width * height * sizeof(float));

    // Build Gaussian pyramid by blurring (on copy) and downsampling
    for (int level = 0; level < numLevels - 1; ++level) {
        int currentW = width >> level;
        int currentH = height >> level;

        // Blur a copy -- do NOT modify gaussianPyramid[level] in place
        std::vector<float> blurred = gaussianPyramid[level];
        gaussianBlur(blurred.data(), currentW, currentH);

        // Downsample the blurred copy for next level
        int nextW = std::max(1, currentW / 2);
        int nextH = std::max(1, currentH / 2);
        gaussianPyramid[level + 1].resize(nextW * nextH);
        downsample(blurred.data(), gaussianPyramid[level + 1].data(),
                   currentW, currentH);
    }

    // Convert Gaussian pyramid to Laplacian pyramid
    // L(i) = G(i) - upsample(G(i+1))
    // Since G(i) is the unblurred level, this captures both blur loss and
    // downsample interpolation loss in L(i), enabling exact reconstruction.
    for (int level = 0; level < numLevels - 1; ++level) {
        int currentW = width >> level;
        int currentH = height >> level;
        int nextW = std::max(1, currentW / 2);
        int nextH = std::max(1, currentH / 2);

        // Allocate Laplacian level
        levels_[level].resize(currentW * currentH);

        // Upsample next Gaussian level
        std::vector<float> upsampled(currentW * currentH);
        upsample(gaussianPyramid[level + 1].data(), upsampled.data(), nextW, nextH);

        // Compute Laplacian: G(i) - upsampled(G(i+1))
        for (int i = 0; i < currentW * currentH; ++i) {
            levels_[level][i] = gaussianPyramid[level][i] - upsampled[i];
        }
    }

    // Store coarsest level (Gaussian, not Laplacian)
    int coarsestW = std::max(1, width >> (numLevels - 1));
    int coarsestH = std::max(1, height >> (numLevels - 1));
    levels_[numLevels_ - 1].resize(coarsestW * coarsestH);
    std::memcpy(levels_[numLevels_ - 1].data(), gaussianPyramid[numLevels - 1].data(),
                coarsestW * coarsestH * sizeof(float));
}

void LaplacianPyramid::reconstruct(const MfpConfig& config, float* outData) {
    // Start with coarsest level (Gaussian, not Laplacian)
    std::vector<float> reconstructed = levels_[numLevels_ - 1];

    // Reconstruct from coarsest to finest
    for (int level = numLevels_ - 2; level >= 0; --level) {
        int w = width_ >> level;
        int h = height_ >> level;

        // Calculate current reconstruction dimensions (coarser level)
        int currentW = std::max(1, width_ >> (level + 1));
        int currentH = std::max(1, height_ >> (level + 1));

        // Upsample current reconstruction to target size
        std::vector<float> upsampled(w * h);
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
    // @MX:NOTE: Bilinear interpolation upsampling -- pseudo-inverse of 2x2 average downsample
    // @MX:REASON: Nearest-neighbor was NOT the inverse of downsample; bilinear preserves
    //             the Laplacian pyramid reconstruction identity G(i) = expand(G(i+1)) + L(i)
    // @MX:SPEC: REQ-ADV-050 identity reconstruction fidelity
    int outW = width * 2;
    int outH = height * 2;

    for (int y = 0; y < outH; ++y) {
        for (int x = 0; x < outW; ++x) {
            // Map output pixel center to input coordinates
            // Output pixel (x,y) center maps to ((x-0.5)/2, (y-0.5)/2) in input space
            // which aligns the upsample grid as the pseudo-inverse of 2x2 averaging
            float srcX = (static_cast<float>(x) - 0.5f) * 0.5f;
            float srcY = (static_cast<float>(y) - 0.5f) * 0.5f;

            // Compute integer source coordinates with boundary clamping
            int x0 = std::max(0, std::min(width - 1, static_cast<int>(std::floor(srcX))));
            int y0 = std::max(0, std::min(height - 1, static_cast<int>(std::floor(srcY))));
            int x1 = std::min(width - 1, x0 + 1);
            int y1 = std::min(height - 1, y0 + 1);

            // Fractional parts clamped to [0, 1]
            float fx = std::max(0.0f, std::min(1.0f, srcX - static_cast<float>(x0)));
            float fy = std::max(0.0f, std::min(1.0f, srcY - static_cast<float>(y0)));

            // Bilinear interpolation weights
            float w00 = (1.0f - fx) * (1.0f - fy);
            float w10 = fx * (1.0f - fy);
            float w01 = (1.0f - fx) * fy;
            float w11 = fx * fy;

            out[y * outW + x] =
                w00 * in[y0 * width + x0] +
                w10 * in[y0 * width + x1] +
                w01 * in[y1 * width + x0] +
                w11 * in[y1 * width + x1];
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

    if (img->width == 0 || img->height == 0) {
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
