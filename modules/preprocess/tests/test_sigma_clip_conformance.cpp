/**
 * @file test_sigma_clip_conformance.cpp
 * @brief SigmaClip conformance to XPE-ALG-001 §9.8.2.1 (#97, QA-A-26)
 *
 * §9.8.2.1 specifies iterative sigma clipping as, per pixel and per iteration:
 *
 *   mu    = (1/|S|) * sum_{k in S} F_k
 *   sigma = sqrt( (1/|S|) * sum_{k in S} (F_k - mu)^2 )      <- population, /|S|
 *   reject = { k in S : |F_k - mu| > kappa * sigma }
 *   defaults kappa = 3.0, max_iter = 5
 *   N_min = max(3, floor(N/4)); if |S| < N_min the pixel is marked a static defect
 *   Cal_Map = (1/|S_final|) * sum_{k in S_final} F_k
 *
 * Two clauses, two different states in this tree:
 *
 *  1. The stddev definition CONFORMS. `stddev_of` divides by |S|
 *     (xpe_calib_generate_offset_methods.cpp:124). Commit 7fd3436 (2026-08-18)
 *     replaced Bessel's correction with it, which is what closed the numeric
 *     half of #97: the sample form inflated sigma as |S| shrank and kept
 *     clipping valid frames. The first case below pins that.
 *
 *  2. The N_min clause is IMPLEMENTED as of QA-A-38 (#138, leader decision (a)):
 *     `generate_offset_values` reports a static-defect mask alongside the map,
 *     and the DLL-side caller OR-merges it into the global defect map. The
 *     second case below is no longer a characterization of a divergence -- it
 *     pins the half that decision (a) deliberately did NOT change: the marked
 *     pixel's calibration value stays the clipped mean, exactly as §9.8.3's
 *     reference implementation computes it. The marking itself is covered by
 *     test_sigma_clip_nmin.cpp.
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe_calib_generate_offset_methods.hpp"

#include <cstring>
#include <vector>

namespace {

constexpr uint32_t W = 2, H = 2;
constexpr size_t   N = static_cast<size_t>(W) * H;

struct Frame {
    std::vector<uint16_t> pixels;
    XpeImageBuffer        buf{};
    Frame(uint16_t fill) {
        pixels.assign(N, fill);
        buf.data = pixels.data();
        buf.width = W; buf.height = H;
        buf.bitsAllocated = 16; buf.bitsStored = 16;
        buf.format = XPE_PIXEL_UINT16;
        buf.dataSize = pixels.size() * sizeof(uint16_t);
    }
};

// Runs sigma clipping over the shipped path and returns the float result, so
// the assertion is not blunted by the uint16 rounding the public entry point
// applies on the way out.
static double clipTo(const std::vector<uint16_t>& values, double kappa) {
    std::vector<Frame> frames;
    frames.reserve(values.size());
    for (uint16_t v : values) frames.emplace_back(v);

    std::vector<XpeImageBuffer> bufs;
    bufs.reserve(frames.size());
    for (const auto& f : frames) bufs.push_back(f.buf);

    xpe::preprocess::OffsetGenerationConfig config;
    config.method = xpe::preprocess::OffsetGenerationMethod::SigmaClip;
    config.sigma  = kappa;

    std::vector<float> result;
    uint32_t w = 0, h = 0;
    EXPECT_EQ(XPE_OK, xpe::preprocess::generate_offset_values(
        bufs.data(), static_cast<int32_t>(bufs.size()), config, &result, &w, &h));
    EXPECT_EQ(N, result.size());
    return result.empty() ? 0.0 : static_cast<double>(result[0]);
}

class SigmaClipConformanceTest : public ::testing::Test {};

// Clause 1 -- population stddev. The #97 scenario: {100, 110, 105, 108, 500}
// with kappa = 1.0.
//
//   iter 1  mu = 184.6   sigma = 157.74 (population)   -> 500 rejected
//   iter 2  mu = 105.75  sigma = 3.7666                -> 100 and 110 rejected
//   iter 3  mu = 106.5   sigma = 1.5    |105-106.5| = 1.5 is NOT > 1.5 -> stable
//
// Result 106.5. Under the retired sample stddev, iteration 2's sigma would be
// 4.3493 instead, 110 would survive, and the run converged on 109 -- the number
// #97 reported. Pinning 106.5 therefore pins the definition, not just a value.
TEST_F(SigmaClipConformanceTest, PopulationStddevReproducesTheSpecifiedResult) {
    EXPECT_DOUBLE_EQ(106.5, clipTo({100, 110, 105, 108, 500}, 1.0))
        << "sigma must divide by |S|, not |S|-1 (XPE-ALG-001 9.8.2.1)";
}

// Default kappa = 3.0 rejects NOTHING from the same set, because the outlier
// inflates sigma enough to cover itself: sigma = 157.74, so the 3-sigma
// threshold is 473.2 while |500 - 184.6| is only 315.4. The first iteration
// removes nothing, the loop breaks, and the result is the plain mean 184.6.
//
// This is the single-outlier masking effect, and it is why the #97 scenario
// passes kappa = 1.0 rather than the default. Measured, not assumed: an earlier
// draft of this case expected 105.75 and the run returned 184.6.
TEST_F(SigmaClipConformanceTest, DefaultKappaIsMaskedByASingleLargeOutlier) {
    EXPECT_FLOAT_EQ(184.6f, static_cast<float>(clipTo({100, 110, 105, 108, 500}, 3.0)))
        << "at kappa = 3 the outlier sits inside its own inflated sigma";
}

// A set with no outlier must come out as the plain mean -- clipping that
// rejects nothing is the identity.
TEST_F(SigmaClipConformanceTest, NoOutlierIsPlainMean) {
    EXPECT_DOUBLE_EQ(104.0, clipTo({100, 102, 104, 106, 108}, 3.0));
}

// Clause 2 -- N_min marks the pixel WITHOUT changing its value.
//
// For N = 5 the spec sets N_min = max(3, floor(5/4)) = 3. The kappa = 1.0 run
// above ends with |S| = 2, which is below that floor, so §9.8.2.1 marks the
// pixel a static defect. §9.8.3 computes cal_map for every pixel with
// max(valid_count, 1) and never substitutes a sentinel for a marked one, so the
// value this function returns is unchanged by QA-A-38.
//
// 106.5 is reachable only from {105, 108}: it is the observable evidence that
// |S| = 2. Keeping it pinned here is what makes a future "zero out defective
// pixels" change fail loudly instead of silently altering every offset map.
// The mark itself is asserted in test_sigma_clip_nmin.cpp.
TEST_F(SigmaClipConformanceTest, NMinMarkDoesNotAlterTheClippedMean) {
    const double value = clipTo({100, 110, 105, 108, 500}, 1.0);

    EXPECT_DOUBLE_EQ(106.5, value)
        << "106.5 is the mean of {105, 108} -- i.e. |S| = 2";

    RecordProperty("spec_clause", "XPE-ALG-001 9.8.2.1 N_min");
    RecordProperty("spec_requires", "static defect mark, value unchanged");
    RecordProperty("implemented_by", "QA-A-38 (#138 decision (a))");
}

} // namespace
