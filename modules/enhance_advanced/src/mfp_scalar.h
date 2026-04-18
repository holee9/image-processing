#ifndef MFP_SCALAR_H
#define MFP_SCALAR_H

#include "xpe/common/xpe_types.h"
#include <vector>
#include <memory>

namespace xpe {
namespace enhance_advanced {

/**
 * MFP Configuration parameters
 * SWU-2.5: Multiscale Frequency Processing
 */
struct MfpConfig {
    int numLevels = 4;              // Number of pyramid levels (default: 4)
    float edgeGain = 1.5f;         // Enhancement coefficient for high frequencies
    float textureGain = 1.2f;      // Enhancement coefficient for mid frequencies
    float flatGain = 1.0f;         // Preservation coefficient for low frequencies (should be 1.0 for identity)
    float noiseThreshold = 0.02f;  // Noise floor for small signals

    // Parse from JSON string
    static MfpConfig fromJson(const char* jsonConfig);
};

/**
 * Laplacian Pyramid for Multiscale Frequency Processing
 * REQ-ADV-010: MFP execution
 * REQ-ADV-050: Identity reconstruction fidelity
 */
class LaplacianPyramid {
public:
    /**
     * Build Laplacian pyramid from input image
     * @param data Input image data (row-major FLOAT32)
     * @param width Image width
     * @param height Image height
     * @param numLevels Number of pyramid levels to build
     */
    LaplacianPyramid(const float* data, int width, int height, int numLevels);

    /**
     * Reconstruct image from enhanced pyramid
     * @param config Enhancement configuration
     * @param outData Output buffer (must be pre-allocated to width * height)
     */
    void reconstruct(const MfpConfig& config, float* outData);

    /**
     * Get original dimensions
     */
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

private:
    /**
     * Gaussian blur (5x5 separable kernel)
     * @param data Input/output buffer
     * @param width Image width
     * @param height Image height
     */
    static void gaussianBlur(float* data, int width, int height);

    /**
     * Downsample image by factor of 2
     * @param in Input buffer
     * @param out Output buffer (will be width/2 x height/2)
     * @param width Input width
     * @param height Input height
     */
    static void downsample(const float* in, float* out, int width, int height);

    /**
     * Upsample image by factor of 2
     * @param in Input buffer
     * @param out Output buffer (will be width*2 x height*2)
     * @param width Input width
     * @param height Input height
     */
    static void upsample(const float* in, float* out, int width, int height);

    int width_;
    int height_;
    int numLevels_;

    // Pyramid levels: levels_[0] is finest (Laplacian), levels_[n-1] is coarsest (Gaussian)
    std::vector<std::vector<float>> levels_;
};

/**
 * Apply Multiscale Frequency Processing to image
 * T-201: Basic MFP execution
 * T-206: Custom configuration support
 * T-207: Identity reconstruction
 *
 * @param img Input/output image buffer
 * @param config MFP configuration
 * @return XPE_OK on success, error code on failure
 */
XpeErrorCode applyMfpScalar(XpeImageBuffer* img, const MfpConfig& config);

} // namespace enhance_advanced
} // namespace xpe

#endif /* MFP_SCALAR_H */
