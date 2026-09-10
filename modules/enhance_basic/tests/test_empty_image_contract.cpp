/**
 * @file test_empty_image_contract.cpp
 * @brief One empty-image contract across every enhance_basic entry point
 *        (#142 D1, QA-B-40).
 *
 * QA-B-39 found the module disagreed with itself: a zero-sized image made
 * xpe_noise_reduce, xpe_contrast_enhance and xpe_edge_enhance return XPE_OK
 * (they treated "nothing to do" as success) while xpe_noise_estimate_sigma and
 * xpe_calc_exposure_index returned XPE_ERR_INVALID_INPUT, and the two log
 * transforms returned XPE_OK by simply looping zero times. Same input, three
 * different answers depending on which function you happened to call.
 *
 * leader decided the contract (#142): width == 0, height == 0, or a NULL data
 * pointer is XPE_ERR_INVALID_INPUT everywhere. A caller that passes an empty
 * image has a bug upstream, and reporting success hides it.
 *
 * Every image-taking entry point gets all three cases, so the rule is proven
 * per path rather than assumed to follow from the shared validator.
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstdint>

#include "xpe/enhance_basic/enhance_basic_api.h"
#include "xpe/common/xpe_error.h"

namespace {

// 32x32, not something smaller: xpe_contrast_enhance rejects an image narrower
// than twice its tile grid, and the default grid is 8x8 (QA-B-39 documented
// this). An 8x8 fixture made the "valid image is still accepted" case below
// fail for a reason that had nothing to do with emptiness.
constexpr uint32_t kW = 32;
constexpr uint32_t kH = 32;

// A well-formed float32 image, then broken in exactly one way per case.
XpeImageBuffer make_valid(std::vector<float>& backing) {
    backing.assign(static_cast<size_t>(kW) * kH, 1.0f);
    XpeImageBuffer img{};
    img.width         = kW;
    img.height        = kH;
    img.format        = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32;
    img.bitsStored    = 32;
    img.dataSize      = backing.size() * sizeof(float);
    img.data          = backing.data();
    return img;
}

XpeImageBuffer make_zero_width(std::vector<float>& backing) {
    XpeImageBuffer img = make_valid(backing);
    img.width    = 0;
    img.dataSize = 0;   // 0 means "unspecified" (#123), so it is not what fails
    return img;
}

XpeImageBuffer make_zero_height(std::vector<float>& backing) {
    XpeImageBuffer img = make_valid(backing);
    img.height   = 0;
    img.dataSize = 0;
    return img;
}

XpeImageBuffer make_null_data(std::vector<float>& backing) {
    XpeImageBuffer img = make_valid(backing);
    img.data = nullptr;
    return img;
}

// Each entry point reduced to "run it against this image". The metadata and
// output arguments are valid throughout, so only the image can be at fault.
XpeErrorCode run_log_transform(XpeImageBuffer* img)  { return xpe_log_transform(img, 1.0f); }
XpeErrorCode run_log_inverse(XpeImageBuffer* img)    { return xpe_log_inverse(img, 1.0f); }

XpeErrorCode run_noise_reduce(XpeImageBuffer* img) {
    XpeNoiseReduceParams p{};
    p.mode        = XPE_NOISE_BILATERAL;
    p.sigma_space = 3.0f;
    p.sigma_range = 50.0f;
    return xpe_noise_reduce(img, &p);
}

XpeErrorCode run_noise_estimate(XpeImageBuffer* img) {
    float sigma = 0.0f;
    return xpe_noise_estimate_sigma(img, &sigma);
}

XpeErrorCode run_contrast(XpeImageBuffer* img) { return xpe_contrast_enhance(img, nullptr); }
XpeErrorCode run_edge(XpeImageBuffer* img)     { return xpe_edge_enhance(img, nullptr); }

XpeErrorCode run_exposure_index(XpeImageBuffer* img) {
    XpeImageMetadata meta{};
    float ei = 0.0f;
    float di = 0.0f;
    return xpe_calc_exposure_index(img, &meta, &ei, &di);
}

using EntryPoint = XpeErrorCode (*)(XpeImageBuffer*);

struct NamedEntryPoint {
    const char* name;
    EntryPoint  fn;
};

const NamedEntryPoint kEntryPoints[] = {
    {"xpe_log_transform",        run_log_transform},
    {"xpe_log_inverse",          run_log_inverse},
    {"xpe_noise_reduce",         run_noise_reduce},
    {"xpe_noise_estimate_sigma", run_noise_estimate},
    {"xpe_contrast_enhance",     run_contrast},
    {"xpe_edge_enhance",         run_edge},
    {"xpe_calc_exposure_index",  run_exposure_index},
};

}  // namespace

TEST(EnhanceBasicEmptyImageContract, ZeroWidthIsInvalidInputEverywhere) {
    for (const auto& ep : kEntryPoints) {
        std::vector<float> backing;
        XpeImageBuffer img = make_zero_width(backing);
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, ep.fn(&img)) << ep.name << " accepted width == 0";
    }
}

TEST(EnhanceBasicEmptyImageContract, ZeroHeightIsInvalidInputEverywhere) {
    for (const auto& ep : kEntryPoints) {
        std::vector<float> backing;
        XpeImageBuffer img = make_zero_height(backing);
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, ep.fn(&img)) << ep.name << " accepted height == 0";
    }
}

TEST(EnhanceBasicEmptyImageContract, NullDataIsInvalidInputEverywhere) {
    for (const auto& ep : kEntryPoints) {
        std::vector<float> backing;
        XpeImageBuffer img = make_null_data(backing);
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, ep.fn(&img)) << ep.name << " accepted a NULL data pointer";
    }
}

// The complement: a well-formed image must still be accepted. Without this a
// validator that rejected everything would satisfy the three cases above.
TEST(EnhanceBasicEmptyImageContract, ValidImageStillAccepted) {
    for (const auto& ep : kEntryPoints) {
        std::vector<float> backing;
        XpeImageBuffer img = make_valid(backing);
        EXPECT_NE(XPE_ERR_INVALID_INPUT, ep.fn(&img))
            << ep.name << " rejected a well-formed image";
    }
}
