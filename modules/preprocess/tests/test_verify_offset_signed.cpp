/**
 * @file test_verify_offset_signed.cpp
 * @brief xpe_verify_offset_signed / xpe_verify_offset_signed_file (QA-A-266, Codex #232 R232-2, #233 R233-4/5)
 *
 * The expected values are computed here, from scratch, by a reference that shares no code with the library: a sorted copy for the percentile (the library uses a
 * histogram), plain double arithmetic for the statistics, and the float32 residual written out by hand. The inequalities between the signed and the UINT16-output
 * bias are tested only under the conditions under which they hold (design_signed_offset_quality.md section 6); the case in which the naive one fails is asserted
 * as a failure so that it cannot come back as a rule.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <functional>
#include <limits>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "xcal_writer.hpp"
#include "xpe_sha256.hpp"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xpe/preprocess_api.h"

namespace fs = std::filesystem;

namespace {

constexpr uint32_t kW = 64, kH = 64;
constexpr size_t kN = static_cast<size_t>(kW) * kH;

XpeImageBuffer bufU16(std::vector<uint16_t>& v, uint32_t w = kW, uint32_t h = kH) {
    XpeImageBuffer b{};
    b.width = w;
    b.height = h;
    b.bitsAllocated = 16;
    b.bitsStored = 16;
    b.format = XPE_PIXEL_UINT16;
    b.data = v.data();
    b.dataSize = v.size() * sizeof(uint16_t);
    return b;
}

XpeImageBuffer bufF32(std::vector<float>& v, uint32_t w = kW, uint32_t h = kH) {
    XpeImageBuffer b{};
    b.width = w;
    b.height = h;
    b.bitsAllocated = 32;
    b.bitsStored = 32;
    b.format = XPE_PIXEL_FLOAT32;
    b.data = v.data();
    b.dataSize = v.size() * sizeof(float);
    return b;
}

/** A raw dark with variation (1000 .. 1040) so that a darkest decile exists. */
std::vector<uint16_t> variedRaw(size_t n = kN) {
    std::vector<uint16_t> r(n);
    for (size_t i = 0; i < n; ++i) r[i] = static_cast<uint16_t>(1000 + (i * 37u) % 41u);
    return r;
}

/** A map that makes the residual x_i = raw_i - map_i equal to want[i] (float32 arithmetic, exact for the fractions used here). */
std::vector<float> mapFor(const std::vector<uint16_t>& raw, const std::vector<float>& want) {
    std::vector<float> m(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) m[i] = static_cast<float>(raw[i]) - want[i];
    return m;
}

std::vector<float> constantResidual(size_t n, float x) { return std::vector<float>(n, x); }

struct Ref {
    bool roi{false};
    size_t roiPixels{0};
    double signedMean{0}, signedStd{0}, outMean{0}, outStd{0}, rawStd{0}, clampPct{0}, zeroPct{0};
};

double meanOf(const std::vector<double>& v) {
    double s = 0;
    for (double x : v) s += x;
    return s / static_cast<double>(v.size());
}

double stdOf(const std::vector<double>& v) {
    const double m = meanOf(v);
    double s = 0;
    for (double x : v) s += (x - m) * (x - m);
    return std::sqrt(s / static_cast<double>(v.size() - 1));
}

/** The reference: written from the definition (design section 5), a sorted copy for the percentile, nothing shared with the library. */
Ref reference(const std::vector<uint16_t>& raw, const std::vector<float>& map) {
    Ref r;
    const size_t n = raw.size();
    size_t below = 0, zero = 0;
    std::vector<float> x(n);
    for (size_t i = 0; i < n; ++i) {
        x[i] = static_cast<float>(raw[i]) - map[i];
        if (x[i] < 0.0f) ++below;
        float c = std::min(std::max(x[i], 0.0f), 65535.0f);
        if (static_cast<uint16_t>(c + 0.5f) == 0) ++zero;
    }
    r.clampPct = 100.0 * static_cast<double>(below) / static_cast<double>(n);
    r.zeroPct = 100.0 * static_cast<double>(zero) / static_cast<double>(n);
    const uint16_t mn = *std::min_element(raw.begin(), raw.end()), mx = *std::max_element(raw.begin(), raw.end());
    const double mid = static_cast<double>(mn + mx) * 0.5;
    if (!(mid > 0.0 && (static_cast<double>(mx) - static_cast<double>(mn)) / mid > 0.01)) return r;
    std::vector<uint16_t> sorted = raw;
    std::sort(sorted.begin(), sorted.end());
    const uint16_t thr = sorted[static_cast<size_t>(static_cast<double>(n) * 0.1)];
    std::vector<double> sx, so, sr;
    for (size_t i = 0; i < n; ++i) {
        if (raw[i] < thr) {
            sx.push_back(static_cast<double>(x[i]));
            float c = std::min(std::max(x[i], 0.0f), 65535.0f);
            so.push_back(static_cast<double>(static_cast<uint16_t>(c + 0.5f)));
            sr.push_back(static_cast<double>(raw[i]));
        }
    }
    r.roiPixels = sx.size();
    if (sx.size() < static_cast<size_t>(static_cast<double>(n) * 0.01)) return r;
    r.roi = true;
    r.signedMean = meanOf(sx);
    r.signedStd = stdOf(sx);
    r.outMean = meanOf(so);
    r.outStd = stdOf(so);
    r.rawStd = stdOf(sr);
    return r;
}

XpeOffsetQuality fresh() {
    XpeOffsetQuality q;
    std::memset(&q, 0xCD, sizeof q);
    q.structSize = sizeof(XpeOffsetQuality);
    return q;
}

constexpr uint32_t kAllGroups = XPE_OFFSETQ_ROI_SIGNED | XPE_OFFSETQ_DARK_REDUCTION | XPE_OFFSETQ_ROI_OUTPUT | XPE_OFFSETQ_CLAMP_RATE | XPE_OFFSETQ_MAP_IDENT;

XpeOffsetQuality measure(std::vector<uint16_t>& raw, std::vector<float>& map) {
    XpeImageBuffer r = bufU16(raw), m = bufF32(map);
    XpeOffsetQuality q = fresh();
    EXPECT_EQ(XPE_OK, xpe_verify_offset_signed(&r, &m, &q));
    return q;
}

}  // namespace

TEST(VerifyOffsetSigned, ItSeesTheNegativeBiasThatTheUint16OutputHides) {
    std::vector<uint16_t> raw = variedRaw();
    std::vector<float> map = mapFor(raw, constantResidual(kN, -4.0f));   // map above raw by 4: every residual is -4
    // note the sign: x = raw - map = -4 means map = raw + 4
    const XpeOffsetQuality q = measure(raw, map);
    ASSERT_EQ(kAllGroups, q.validMask);
    EXPECT_NEAR(-4.0, q.darkBiasSigned, 1e-9) << "the signed bias is the residual before the floor";
    EXPECT_EQ(0.0, q.darkBiasOutput) << "the UINT16 output is 0 everywhere: the negative bias is invisible there";
    EXPECT_NEAR(100.0, q.clampRatePct, 1e-12);
    EXPECT_NEAR(100.0, q.zeroOutputRatePct, 1e-12);
    EXPECT_NEAR(0.0, q.dsnuSigned, 1e-9);
}

TEST(VerifyOffsetSigned, ItMatchesTheReferenceOnAMixedFrame) {
    std::mt19937 g(266);
    std::normal_distribution<float> nd(0.0f, 3.0f);
    std::uniform_int_distribution<int> ud(980, 1060);
    std::vector<uint16_t> raw(kN);
    for (auto& v : raw) v = static_cast<uint16_t>(ud(g));
    std::vector<float> want(kN), map;
    for (auto& v : want) v = nd(g) + 0.37f;
    map = mapFor(raw, want);
    const Ref ref = reference(raw, map);
    ASSERT_TRUE(ref.roi);
    const XpeOffsetQuality q = measure(raw, map);
    ASSERT_EQ(kAllGroups, q.validMask);
    EXPECT_EQ(ref.roiPixels, q.roiPixels);
    EXPECT_NEAR(ref.signedMean, q.darkBiasSigned, 1e-9);
    EXPECT_NEAR(ref.signedStd, q.dsnuSigned, 1e-9);
    EXPECT_NEAR(ref.outMean, q.darkBiasOutput, 1e-9);
    EXPECT_NEAR(ref.outStd, q.dsnuOutput, 1e-9);
    EXPECT_NEAR(20.0 * std::log10(ref.rawStd / std::max(ref.signedStd, 1e-9)), q.darkReductionDb, 1e-9);
    EXPECT_NEAR(ref.clampPct, q.clampRatePct, 1e-12);
    EXPECT_NEAR(ref.zeroPct, q.zeroOutputRatePct, 1e-12);
    EXPECT_EQ(static_cast<uint64_t>(kN), q.pixelCount);
    EXPECT_EQ(kW, q.mapWidth);
    EXPECT_EQ(kH, q.mapHeight);
}

TEST(VerifyOffsetSigned, ThePercentileFromTheHistogramEqualsTheSortedCopyForAwkwardSizesAndTies) {
    std::mt19937 g(7);
    for (const uint32_t w : {33u, 50u, 61u, 97u}) {
        for (const int span : {3, 8, 60}) {
            const uint32_t h = w + 5;
            const size_t n = static_cast<size_t>(w) * h;
            std::vector<uint16_t> raw(n);
            for (auto& v : raw) v = static_cast<uint16_t>(2000 + static_cast<int>(g() % static_cast<unsigned>(span)));
            std::vector<float> map(n, 1999.5f);
            XpeImageBuffer r = bufU16(raw, w, h), m = bufF32(map, w, h);
            XpeOffsetQuality q = fresh();
            ASSERT_EQ(XPE_OK, xpe_verify_offset_signed(&r, &m, &q));
            const Ref ref = reference(raw, map);
            EXPECT_EQ(ref.roiPixels, q.roiPixels) << "w " << w << " span " << span;
            EXPECT_EQ(ref.roi, (q.validMask & XPE_OFFSETQ_ROI_SIGNED) != 0u) << "w " << w << " span " << span;
            if (ref.roi) EXPECT_NEAR(ref.signedMean, q.darkBiasSigned, 1e-9);
        }
    }
}

TEST(VerifyOffsetSigned, TheClampRateCountsNegativeResidualsAndTheZeroOutputRateCountsRoundedZeros) {
    struct Case { float x; double clamp, zero; };
    for (const Case& c : {Case{-1.0f, 100.0, 100.0}, Case{0.0f, 0.0, 100.0}, Case{0.25f, 0.0, 100.0}, Case{0.5f, 0.0, 0.0}, Case{1.0f, 0.0, 0.0}}) {
        std::vector<uint16_t> raw = variedRaw();
        std::vector<float> map = mapFor(raw, constantResidual(kN, c.x));
        const XpeOffsetQuality q = measure(raw, map);
        EXPECT_NEAR(c.clamp, q.clampRatePct, 1e-12) << "x = " << c.x;
        EXPECT_NEAR(c.zero, q.zeroOutputRatePct, 1e-12) << "x = " << c.x;
    }
    // half and half
    std::vector<uint16_t> raw = variedRaw();
    std::vector<float> want(kN);
    for (size_t i = 0; i < kN; ++i) want[i] = (i % 2 == 0) ? -1.0f : 1.0f;
    std::vector<float> map = mapFor(raw, want);
    const XpeOffsetQuality q = measure(raw, map);
    EXPECT_NEAR(50.0, q.clampRatePct, 1e-12);
    EXPECT_NEAR(50.0, q.zeroOutputRatePct, 1e-12);
}

TEST(VerifyOffsetSigned, ARawFrameWithoutVariationOrWithoutAnROIStillGivesTheWholeFrameRates) {
    std::vector<uint16_t> constant(kN, 1000);
    std::vector<float> map(kN, 1003.0f);
    XpeOffsetQuality q = measure(constant, map);
    EXPECT_EQ(XPE_OFFSETQ_CLAMP_RATE | XPE_OFFSETQ_MAP_IDENT, q.validMask) << "no variation: no ROI, but the whole-frame rate and the map identity are valid";
    EXPECT_NEAR(100.0, q.clampRatePct, 1e-12);
    EXPECT_EQ(0.0, q.darkBiasSigned) << "a bit that is clear holds 0";
    // ties: 90 % of the pixels at 1000, 10 % at 1100 -> raw < percentile10 selects nothing (the strict < of FUNC-035)
    std::vector<uint16_t> tied(kN, 1000);
    for (size_t i = 0; i < kN / 10 + 1; ++i) tied[i] = 1100;
    std::vector<float> map2(kN, 1000.0f);
    q = measure(tied, map2);
    EXPECT_EQ(0u, q.roiPixels);
    EXPECT_EQ(XPE_OFFSETQ_CLAMP_RATE | XPE_OFFSETQ_MAP_IDENT, q.validMask);
    EXPECT_NEAR(0.0, q.clampRatePct, 1e-12) << "x is 0 or 100: nothing below 0";
}

TEST(VerifyOffsetSigned, TheSignedAndTheOutputBiasObeyOnlyTheRelationsThatHold) {
    std::mt19937 g(2661);
    std::normal_distribution<float> nd(0.0f, 3.0f);
    std::vector<uint16_t> raw = variedRaw();
    std::vector<float> want(kN);
    for (auto& v : want) v = nd(g);
    std::vector<float> map = mapFor(raw, want);
    const Ref ref = reference(raw, map);
    ASSERT_TRUE(ref.roi);
    // the clamp-only mean over the ROI, from the definition
    std::vector<uint16_t> sorted = raw;
    std::sort(sorted.begin(), sorted.end());
    const uint16_t thr = sorted[static_cast<size_t>(static_cast<double>(kN) * 0.1)];
    double mc = 0;
    size_t cnt = 0;
    for (size_t i = 0; i < kN; ++i) {
        if (raw[i] < thr) {
            mc += std::min(std::max(static_cast<double>(static_cast<float>(raw[i]) - map[i]), 0.0), 65535.0);   // m_s: with the saturation (here nothing saturates)
            ++cnt;
        }
    }
    mc /= static_cast<double>(cnt);
    const XpeOffsetQuality q = measure(raw, map);
    EXPECT_GE(mc, q.darkBiasSigned - 1e-12) << "T-A: the floor can only raise the mean (m_0 >= signed; here m_s = m_0 because no residual exceeds 65535)";
    EXPECT_LE(std::fabs(q.darkBiasOutput - mc), 0.5) << "T-B: |output - m_s| <= 0.5 (the rounding moves a pixel by at most 0.5, after the saturation)";
}

TEST(VerifyOffsetSigned, TheOutputBiasCanBeBelowTheSignedBiasARoiResidualOfPointTwoRoundsToZero) {
    // T-C: the counterexample of Codex #233 R233-5. Every residual +0.2: the UINT16 output is 0, the signed bias 0.2
    std::vector<uint16_t> raw = variedRaw();
    std::vector<float> map = mapFor(raw, constantResidual(kN, 0.2f));
    const XpeOffsetQuality q = measure(raw, map);
    EXPECT_EQ(0.0, q.darkBiasOutput);
    EXPECT_NEAR(0.2, q.darkBiasSigned, 1e-3);
    EXPECT_LT(q.darkBiasOutput, q.darkBiasSigned) << "darkBiasOutput >= darkBiasSigned is NOT a rule";
}

TEST(VerifyOffsetSigned, WithIntegerResidualsTheOutputBiasIsTheClampOnlyMeanAndAtLeastTheSignedBias) {
    // T-D: integer residuals -3 .. +5 (all <= 65535) -> floor(x + 0.5) = x for x >= 0, so the rounding adds nothing and the relation holds; above 65535 it does not (see the saturation test)
    std::vector<uint16_t> raw = variedRaw();
    std::vector<float> want(kN);
    for (size_t i = 0; i < kN; ++i) want[i] = static_cast<float>(static_cast<int>(i % 9) - 3);
    std::vector<float> map = mapFor(raw, want);
    std::vector<uint16_t> sorted = raw;
    std::sort(sorted.begin(), sorted.end());
    const uint16_t thr = sorted[static_cast<size_t>(static_cast<double>(kN) * 0.1)];
    double mc = 0;
    size_t cnt = 0;
    for (size_t i = 0; i < kN; ++i) {
        if (raw[i] < thr) {
            mc += std::max(static_cast<double>(want[i]), 0.0);
            ++cnt;
        }
    }
    mc /= static_cast<double>(cnt);
    const XpeOffsetQuality q = measure(raw, map);
    EXPECT_NEAR(mc, q.darkBiasOutput, 1e-12);
    EXPECT_GE(q.darkBiasOutput, q.darkBiasSigned - 1e-12);
}

TEST(VerifyOffsetSigned, ForNormalResidualsTheFloorRaisesTheMeanByTheNormalFormula) {
    // T-E (statistical, approximate): the clamp-only shift m_c - mean(x) against the normal formula at the ROI sample's own mean and sigma.
    // Fixed seed; a plug-in comparison, so the tolerance is the non-normality of one sample of about 1,000 pixels, 0.08 sigma.
    constexpr uint32_t w = 128, h = 128;
    constexpr size_t n = static_cast<size_t>(w) * h;
    std::mt19937 g(26600);
    std::normal_distribution<float> nd(-0.5f, 3.0f);
    std::vector<uint16_t> raw = variedRaw(n);
    std::vector<float> want(n);
    for (auto& v : want) v = nd(g);
    std::vector<float> map = mapFor(raw, want);
    XpeImageBuffer r = bufU16(raw, w, h), m = bufF32(map, w, h);
    XpeOffsetQuality q = fresh();
    ASSERT_EQ(XPE_OK, xpe_verify_offset_signed(&r, &m, &q));
    ASSERT_NE(0u, q.validMask & XPE_OFFSETQ_ROI_SIGNED);
    std::vector<uint16_t> sorted = raw;
    std::sort(sorted.begin(), sorted.end());
    const uint16_t thr = sorted[static_cast<size_t>(static_cast<double>(n) * 0.1)];
    double mc = 0;
    size_t cnt = 0;
    for (size_t i = 0; i < n; ++i) {
        if (raw[i] < thr) {
            mc += std::max(static_cast<double>(want[i]), 0.0);
            ++cnt;
        }
    }
    mc /= static_cast<double>(cnt);
    const double mu = q.darkBiasSigned, sigma = q.dsnuSigned;
    const double pdf = std::exp(-0.5 * (mu / sigma) * (mu / sigma)) / std::sqrt(2.0 * 3.14159265358979323846);
    const double cdf = 0.5 * std::erfc(-mu / sigma / std::sqrt(2.0));
    const double formula = mu * cdf + sigma * pdf - mu;
    EXPECT_NEAR(formula, mc - mu, 0.08 * sigma) << "mu " << mu << " sigma " << sigma << " cnt " << cnt;
}

TEST(VerifyOffsetSigned, TheMapIdentityIsTheSha256OfItsFloatBytesAndItsSize) {
    std::vector<uint16_t> raw = variedRaw();
    std::vector<float> map = mapFor(raw, constantResidual(kN, 1.0f));
    const XpeOffsetQuality q = measure(raw, map);
    Sha256Stream sha;
    sha.update(reinterpret_cast<const uint8_t*>(map.data()), map.size() * sizeof(float));
    EXPECT_EQ(0, std::memcmp(sha.digest().data(), q.mapSha256, 32));
    EXPECT_NE(0u, q.validMask & XPE_OFFSETQ_MAP_IDENT);
    // one float changed -> another identity
    map[100] += 0.5f;
    const XpeOffsetQuality q2 = measure(raw, map);
    EXPECT_NE(0, std::memcmp(q.mapSha256, q2.mapSha256, 32));
}

TEST(VerifyOffsetSigned, TheStructSizeRuleWritesOnlyCompleteFieldsAndZeroesTheGroupsItCannotHold) {
    // QA-A-266b (Codex #237 R237-2): what is written ends at a field boundary; a group whose fields do not all fit is invalid and its bytes inside what was written are 0
    std::vector<uint16_t> raw = variedRaw();
    std::vector<float> map = mapFor(raw, constantResidual(kN, 2.0f));
    XpeImageBuffer r = bufU16(raw), m = bufF32(map);
    XpeOffsetQuality full = fresh();
    ASSERT_EQ(XPE_OK, xpe_verify_offset_signed(&r, &m, &full));
    ASSERT_EQ(kAllGroups, full.validMask);
    struct Group { uint32_t bit, begin, end; };
    const Group groups[] = {{XPE_OFFSETQ_MAP_IDENT, 32, 64}, {XPE_OFFSETQ_ROI_SIGNED, 64, 80}, {XPE_OFFSETQ_DARK_REDUCTION, 80, 88}, {XPE_OFFSETQ_CLAMP_RATE, 88, 104}, {XPE_OFFSETQ_ROI_OUTPUT, 104, 120}};
    struct Case { uint32_t size; uint32_t take; };
    for (const Case& c : {Case{32, 32}, Case{33, 32}, Case{63, 32}, Case{64, 64}, Case{65, 64}, Case{71, 64}, Case{72, 72}, Case{79, 72}, Case{80, 80}, Case{87, 80}, Case{88, 88}, Case{95, 88},
                          Case{96, 96}, Case{103, 96}, Case{104, 104}, Case{111, 104}, Case{112, 112}, Case{119, 112}, Case{120, 120}, Case{121, 120}, Case{200, 120}}) {
        std::vector<uint8_t> buf(300, 0xCD);
        auto* q = reinterpret_cast<XpeOffsetQuality*>(buf.data());
        q->structSize = c.size;
        ASSERT_EQ(XPE_OK, xpe_verify_offset_signed(&r, &m, q)) << "size " << c.size;
        EXPECT_EQ(c.take, q->writtenSize) << "size " << c.size;
        uint32_t want = 0;
        for (const Group& g : groups) {
            if (g.end <= c.take) want |= g.bit;
        }
        EXPECT_EQ(want, q->validMask) << "size " << c.size;
        // nothing past what was written is touched, including the bytes between that boundary and the caller's structSize
        for (size_t i = c.take; i < buf.size(); ++i) {
            ASSERT_EQ(0xCD, buf[i]) << "size " << c.size << ": byte " << i << " past the written part was touched";
        }
        // inside: the head words that do not depend on the size and every complete group are the full call's; a group that did not fit is 0 (never half a double, never a stale 2.0)
        std::vector<uint8_t> expect(reinterpret_cast<const uint8_t*>(&full), reinterpret_cast<const uint8_t*>(&full) + sizeof full);
        for (const Group& g : groups) {
            if (g.end > c.take) std::memset(expect.data() + g.begin, 0, g.end - g.begin);
        }
        for (uint32_t i = 12; i < c.take; ++i) {
            ASSERT_EQ(expect[i], buf[i]) << "size " << c.size << ": byte " << i;
        }
    }
    // the case of the review: structSize 72 holds the whole of darkBiasSigned but not the group ROI_SIGNED; the field must read 0, not 2
    std::vector<uint8_t> buf(300, 0xCD);
    auto* q = reinterpret_cast<XpeOffsetQuality*>(buf.data());
    q->structSize = 72;
    ASSERT_EQ(XPE_OK, xpe_verify_offset_signed(&r, &m, q));
    EXPECT_EQ(XPE_OFFSETQ_MAP_IDENT, q->validMask);
    EXPECT_EQ(0.0, q->darkBiasSigned) << "a field whose group bit is clear holds 0";
}

TEST(VerifyOffsetSigned, AnEmptyOrTooSmallRoiIsUnmeasuredAndNeverZeroFilledAsMeasured) {
    // QA-A-266b (Codex #237 R237-1). Frames of one row of n pixels; raw is 100 where the ROI is wanted and 200 elsewhere, so the ROI is exactly the pixels at 100 (the darkest decile is raw < sorted[floor(0.1 n)]).
    auto allFinite = [](const XpeOffsetQuality& q) {
        for (double v : {q.darkBiasSigned, q.dsnuSigned, q.darkReductionDb, q.clampRatePct, q.zeroOutputRatePct, q.darkBiasOutput, q.dsnuOutput}) {
            if (!std::isfinite(v)) return false;
        }
        return true;
    };
    auto run = [&](size_t n, size_t roiCount, float mapOffset, bool distinctRoi) {
        std::vector<uint16_t> raw(n, 200);
        for (size_t i = 0; i < roiCount; ++i) raw[i] = static_cast<uint16_t>(100 + (distinctRoi ? i : 0));
        std::vector<float> map(n);
        for (size_t i = 0; i < n; ++i) map[i] = static_cast<float>(raw[i]) + mapOffset;
        XpeImageBuffer r = bufU16(raw, static_cast<uint32_t>(n), 1), mp = bufF32(map, static_cast<uint32_t>(n), 1);
        XpeOffsetQuality q = fresh();
        EXPECT_EQ(XPE_OK, xpe_verify_offset_signed(&r, &mp, &q)) << "n " << n;
        EXPECT_TRUE(allFinite(q)) << "n " << n << ": no field is ever inf or nan";
        return q;
    };
    const uint32_t kWhole = XPE_OFFSETQ_CLAMP_RATE | XPE_OFFSETQ_MAP_IDENT;
    // an empty ROI: n = 2, 9, 10, 99 with only the last pixel different
    for (const size_t n : {size_t{2}, size_t{9}, size_t{10}, size_t{99}}) {
        std::vector<uint16_t> raw(n, 100);
        raw[n - 1] = 200;
        std::vector<float> map(n);
        for (size_t i = 0; i < n; ++i) map[i] = static_cast<float>(raw[i]);
        XpeImageBuffer r = bufU16(raw, static_cast<uint32_t>(n), 1), mp = bufF32(map, static_cast<uint32_t>(n), 1);
        XpeOffsetQuality q = fresh();
        ASSERT_EQ(XPE_OK, xpe_verify_offset_signed(&r, &mp, &q)) << "n " << n;
        EXPECT_EQ(0u, q.roiPixels) << "n " << n;
        EXPECT_EQ(kWhole, q.validMask) << "n " << n << ": an empty ROI is not measured";
        EXPECT_EQ(0.0, q.darkReductionDb) << "n " << n;
        EXPECT_TRUE(allFinite(q)) << "n " << n << ": the dB was -inf";
    }
    // a ROI of one pixel is too small for a sample standard deviation, also when it is 1 % or more of the frame
    XpeOffsetQuality q = run(100, 1, 4.0f, false);
    EXPECT_EQ(1u, q.roiPixels);
    EXPECT_EQ(kWhole, q.validMask) << "1 pixel: unmeasured";
    // a ROI of 1 pixel in 101 is also under 1 %
    q = run(101, 1, 4.0f, false);
    EXPECT_EQ(kWhole, q.validMask);
    // 10 equal pixels in 100: measured (10 % of the frame), but a constant raw ROI has no spread to reduce: the bias and DSNU are valid, the dB is not
    q = run(100, 10, 4.0f, false);
    EXPECT_EQ(10u, q.roiPixels);
    EXPECT_EQ(XPE_OFFSETQ_ROI_SIGNED | XPE_OFFSETQ_ROI_OUTPUT | kWhole, q.validMask) << "no DARK_REDUCTION without a spread in the raw ROI";
    EXPECT_NEAR(-4.0, q.darkBiasSigned, 1e-9);
    EXPECT_EQ(0.0, q.dsnuSigned);
    EXPECT_EQ(0.0, q.darkReductionDb);
    // the 1 % edge by an integer comparison: 2 of 200 is exactly 1 %, 2 of 201 is under it; 3 of 300 and 3 of 301
    q = run(200, 2, 4.0f, true);
    EXPECT_EQ(2u, q.roiPixels);
    EXPECT_EQ(kAllGroups, q.validMask) << "2 of 200 = exactly 1 %, with a spread";
    q = run(201, 2, 4.0f, true);
    EXPECT_EQ(2u, q.roiPixels);
    EXPECT_EQ(kWhole, q.validMask) << "2 of 201 is under 1 %";
    q = run(300, 3, 4.0f, true);
    EXPECT_EQ(kAllGroups, q.validMask) << "3 of 300";
    q = run(301, 3, 4.0f, true);
    EXPECT_EQ(kWhole, q.validMask) << "3 of 301 is under 1 %";
}

TEST(VerifyOffsetSigned, ANullArgumentClearsAnEarlierResultLikeEveryOtherError) {
    // QA-A-266b (Codex #237 R237-3)
    std::vector<uint16_t> raw = variedRaw();
    std::vector<float> map = mapFor(raw, constantResidual(kN, 2.0f));
    XpeImageBuffer r = bufU16(raw), m = bufF32(map);
    auto headCleared = [](const XpeOffsetQuality& q) { return q.writtenSize == 0 && q.validMask == 0 && q.roiPixels == 0 && q.pixelCount == 0 && q.mapWidth == 0 && q.mapHeight == 0; };
    XpeOffsetQuality q = fresh();
    ASSERT_EQ(XPE_OK, xpe_verify_offset_signed(&r, &m, &q));
    ASSERT_EQ(kAllGroups, q.validMask);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_verify_offset_signed(nullptr, &m, &q));
    EXPECT_TRUE(headCleared(q)) << "NULL raw";
    EXPECT_EQ(sizeof(XpeOffsetQuality), q.structSize) << "the caller's structSize is kept";
    q = fresh();
    ASSERT_EQ(XPE_OK, xpe_verify_offset_signed(&r, &m, &q));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_verify_offset_signed(&r, nullptr, &q));
    EXPECT_TRUE(headCleared(q)) << "NULL map";
    q = fresh();
    ASSERT_EQ(XPE_OK, xpe_verify_offset_signed(&r, &m, &q));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_verify_offset_signed_file(&r, nullptr, &q));
    EXPECT_TRUE(headCleared(q)) << "NULL path";
    q = fresh();
    ASSERT_EQ(XPE_OK, xpe_verify_offset_signed(&r, &m, &q));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_verify_offset_signed_file(nullptr, "x.xcal", &q));
    EXPECT_TRUE(headCleared(q)) << "NULL raw, file variant";
    // a NULL quality writes nowhere; a struct that cannot hold the head is not written
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_verify_offset_signed(&r, &m, nullptr));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_verify_offset_signed_file(&r, "x.xcal", nullptr));
    q = fresh();
    ASSERT_EQ(XPE_OK, xpe_verify_offset_signed(&r, &m, &q));
    q.structSize = 31;
    XpeOffsetQuality before = q;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_verify_offset_signed(nullptr, &m, &q));
    EXPECT_EQ(0, std::memcmp(&before, &q, sizeof q)) << "structSize 31: nothing is written";
}

TEST(VerifyOffsetSigned, AResidualAbove65535SaturatesTheOutputAndTheRelationsNameTheirCondition) {
    // QA-A-266b (Codex #237 R237-4): the UINT16 output saturates at 65535. raw 100..199, map = raw - 65635, so every residual is 65635.
    std::vector<uint16_t> raw(100);
    for (size_t i = 0; i < raw.size(); ++i) raw[i] = static_cast<uint16_t>(100 + i);
    std::vector<float> map(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) map[i] = static_cast<float>(raw[i]) - 65635.0f;
    XpeImageBuffer r = bufU16(raw, 100, 1), m = bufF32(map, 100, 1);
    XpeOffsetQuality q = fresh();
    ASSERT_EQ(XPE_OK, xpe_verify_offset_signed(&r, &m, &q));
    ASSERT_NE(0u, q.validMask & XPE_OFFSETQ_ROI_SIGNED);
    const double m0 = 65635.0, ms = 65535.0;   // mean(max(x,0)) and mean(min(max(x,0),65535)) over the ROI
    EXPECT_NEAR(65635.0, q.darkBiasSigned, 1e-6);
    EXPECT_NEAR(65535.0, q.darkBiasOutput, 1e-6);
    EXPECT_GE(m0, q.darkBiasSigned - 1e-6) << "always: m_0 >= signed";
    EXPECT_LE(std::fabs(q.darkBiasOutput - ms), 0.5) << "always: |output - m_s| <= 0.5";
    EXPECT_GT(std::fabs(q.darkBiasOutput - m0), 0.5) << "|output - m_0| <= 0.5 needs x <= 65535: here it is 100";
    EXPECT_LT(q.darkBiasOutput, q.darkBiasSigned) << "and integer residuals do not give output >= signed when they exceed 65535";
}

TEST(VerifyOffsetSigned, EveryErrorHasItsCodeAndLeavesTheCallersStructAsTheContractSays) {
    std::vector<uint16_t> raw = variedRaw();
    std::vector<float> map = mapFor(raw, constantResidual(kN, 1.0f));
    XpeImageBuffer r = bufU16(raw), m = bufF32(map);
    XpeOffsetQuality q = fresh();
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_verify_offset_signed(nullptr, &m, &q));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_verify_offset_signed(&r, nullptr, &q));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_verify_offset_signed(&r, &m, nullptr));
    // structSize below the head: nothing written at all
    {
        std::vector<uint8_t> buf(sizeof(XpeOffsetQuality), 0xCD);
        auto* p = reinterpret_cast<XpeOffsetQuality*>(buf.data());
        p->structSize = 31;
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_verify_offset_signed(&r, &m, p));
        for (size_t i = 4; i < buf.size(); ++i) ASSERT_EQ(0xCD, buf[i]) << i;
    }
    auto tail_untouched = [](const XpeOffsetQuality& v) {
        const auto* b = reinterpret_cast<const uint8_t*>(&v);
        for (size_t i = 32; i < sizeof v; ++i) {
            if (b[i] != 0xCD) return false;
        }
        return true;
    };
    // wrong formats
    {
        std::vector<float> f(kN, 1.0f);
        XpeImageBuffer bad = bufF32(f);
        q = fresh();
        EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, xpe_verify_offset_signed(&bad, &m, &q));
        EXPECT_EQ(0u, q.validMask);
        EXPECT_EQ(0u, q.writtenSize);
        EXPECT_TRUE(tail_untouched(q)) << "an error zeroes the 32-byte head only";
        std::vector<uint16_t> u(kN, 5);
        XpeImageBuffer badMap = bufU16(u);
        q = fresh();
        EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, xpe_verify_offset_signed(&r, &badMap, &q));
    }
    // sizes
    {
        std::vector<float> small(kN / 2, 1.0f);
        XpeImageBuffer sm = bufF32(small, kW, kH / 2);
        q = fresh();
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_verify_offset_signed(&r, &sm, &q));
        XpeImageBuffer zero = r;
        zero.width = 0;
        q = fresh();
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_verify_offset_signed(&zero, &m, &q));
        XpeImageBuffer shortBuf = r;
        shortBuf.dataSize = 10;
        q = fresh();
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_verify_offset_signed(&shortBuf, &m, &q));
        XpeImageBuffer huge = r;
        huge.width = 0xFFFFFFFFu;
        huge.height = 0xFFFFFFFFu;
        huge.dataSize = 0;
        q = fresh();
        const XpeErrorCode rc = xpe_verify_offset_signed(&huge, &m, &q);
        EXPECT_TRUE(rc == XPE_ERR_INVALID_INPUT) << rc;
    }
    // a map that is not finite
    for (const float bad : {std::nanf(""), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()}) {
        std::vector<float> mm = map;
        mm[kN / 2] = bad;
        XpeImageBuffer bm = bufF32(mm);
        q = fresh();
        EXPECT_EQ(XPE_ERR_INVALID_CALIB_DATA, xpe_verify_offset_signed(&r, &bm, &q));
        EXPECT_EQ(0u, q.validMask);
        EXPECT_TRUE(tail_untouched(q));
    }
}

TEST(VerifyOffsetSigned, ItKeepsNoStateItNeedsNoInitAndTwoThreadsWithOtherMapsDoNotMixTheirValues) {
    std::vector<uint16_t> raw = variedRaw();
    std::vector<float> m1 = mapFor(raw, constantResidual(kN, -4.0f)), m2 = mapFor(raw, constantResidual(kN, 3.0f));
    const XpeOffsetQuality a = measure(raw, m1), b = measure(raw, m2);
    ASSERT_NEAR(-4.0, a.darkBiasSigned, 1e-9);
    ASSERT_NEAR(3.0, b.darkBiasSigned, 1e-9);
    std::vector<XpeOffsetQuality> ra(40), rb(40);
    auto run = [&](std::vector<float>& map, std::vector<XpeOffsetQuality>& out) {
        for (auto& o : out) {
            XpeImageBuffer r = bufU16(raw), m = bufF32(map);
            o = fresh();
            ASSERT_EQ(XPE_OK, xpe_verify_offset_signed(&r, &m, &o));
        }
    };
    std::thread t1(run, std::ref(m1), std::ref(ra)), t2(run, std::ref(m2), std::ref(rb));
    t1.join();
    t2.join();
    for (const auto& o : ra) {
        EXPECT_EQ(a.darkBiasSigned, o.darkBiasSigned);
        EXPECT_EQ(0, std::memcmp(a.mapSha256, o.mapSha256, 32));
    }
    for (const auto& o : rb) {
        EXPECT_EQ(b.darkBiasSigned, o.darkBiasSigned);
        EXPECT_EQ(0, std::memcmp(b.mapSha256, o.mapSha256, 32));
    }
}

class VerifyOffsetSignedFile : public ::testing::Test {
protected:
    fs::path dir_;
    void SetUp() override {
        dir_ = fs::temp_directory_path() / "xpe_a266_signed";
        std::error_code ec;
        fs::remove_all(dir_, ec);
        fs::create_directories(dir_);
        // the module is started below only by the continuity test: this class's other tests must work without it
    }
    void TearDown() override {
        xpe_preprocess_shutdown();
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }
    void writeMap(const char* name, XCalType type, const std::vector<float>& map, int64_t expiryMs = 0) {
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        std::memcpy(hdr.session_id, "a266", 5);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(type);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = kW;
        hdr.height = kH;
        hdr.expiry_epoch_ms = expiryMs;
        hdr.payload_len = map.size() * sizeof(float);
        ASSERT_EQ(XPE_OK, write_xcal_file((dir_ / name).string().c_str(), hdr, nullptr, 0, reinterpret_cast<const uint8_t*>(map.data()), map.size() * sizeof(float)));
    }
};

TEST_F(VerifyOffsetSignedFile, TheFileVariantEqualsTheBufferVariantAndDoesNotCheckExpiry) {
    std::vector<uint16_t> raw = variedRaw();
    std::vector<float> map = mapFor(raw, constantResidual(kN, -2.0f));
    writeMap("offset.xcal", XCAL_TYPE_OFFSET, map, /*expiry in the past*/ 1000);
    XpeImageBuffer r = bufU16(raw), m = bufF32(map);
    XpeOffsetQuality viaBuffer = fresh(), viaFile = fresh();
    ASSERT_EQ(XPE_OK, xpe_verify_offset_signed(&r, &m, &viaBuffer));
    ASSERT_EQ(XPE_OK, xpe_verify_offset_signed_file(&r, (dir_ / "offset.xcal").string().c_str(), &viaFile)) << "a stored map is measured, not applied: its expiry is not checked";
    EXPECT_EQ(0, std::memcmp(&viaBuffer, &viaFile, sizeof viaBuffer));
    // errors of the file route
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_verify_offset_signed_file(&r, nullptr, &viaFile));
    XpeOffsetQuality q = fresh();
    EXPECT_NE(XPE_OK, xpe_verify_offset_signed_file(&r, (dir_ / "missing.xcal").string().c_str(), &q));
    EXPECT_EQ(0u, q.validMask);
    writeMap("gain.xcal", XCAL_TYPE_GAIN, map);
    q = fresh();
    EXPECT_NE(XPE_OK, xpe_verify_offset_signed_file(&r, (dir_ / "gain.xcal").string().c_str(), &q)) << "a gain file is not an offset map";
    // a damaged file (one payload byte flipped): the reader's SHA-256 check refuses it
    {
        const std::string path = (dir_ / "offset.xcal").string();
        std::vector<uint8_t> bytes;
        {
            std::ifstream in(path, std::ios::binary);
            ASSERT_TRUE(in.good());
            bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }
        ASSERT_GT(bytes.size(), 5u);
        bytes[bytes.size() - 5] ^= 0x40;
        {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            ASSERT_TRUE(out.good());
            out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }
        q = fresh();
        EXPECT_NE(XPE_OK, xpe_verify_offset_signed_file(&r, path.c_str(), &q));
    }
}

TEST_F(VerifyOffsetSignedFile, TheOutputFiguresEqualTheOldFunctionOnTheCorrectedImageBitForBit) {
    // continuity: darkBiasOutput / dsnuOutput are dark_bias / dsnu_adu of xpe_verify_offset on what xpe_offset_correct writes with the same map
    std::mt19937 g(2662);
    std::normal_distribution<float> nd(0.0f, 3.0f);
    std::vector<uint16_t> raw = variedRaw();
    std::vector<float> want(kN);
    for (auto& v : want) v = nd(g) + 0.1f;
    // boundary residuals in the first pixels (exactly representable): the rounding rule is the correction kernel's
    const float edges[] = {-0.5f, 0.0f, 0.25f, 0.5f, 0.75f, 1.5f, 2.5f};
    for (size_t i = 0; i < sizeof edges / sizeof edges[0]; ++i) want[i] = edges[i];
    std::vector<float> map = mapFor(raw, want);
    writeMap("offset.xcal", XCAL_TYPE_OFFSET, map);
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset((dir_ / "offset.xcal").string().c_str()));
    std::vector<uint16_t> corrected(kN);
    XpeImageBuffer r = bufU16(raw), c = bufU16(corrected);
    XpeImageMetadata meta{};
    ASSERT_EQ(XPE_OK, xpe_offset_correct(&r, &c, &meta));
    XpeCalibrationMetrics old{};
    ASSERT_EQ(XPE_OK, xpe_verify_offset(&r, &c, nullptr, &old));
    XpeOffsetQuality q = fresh();
    ASSERT_EQ(XPE_OK, xpe_verify_offset_signed_file(&r, (dir_ / "offset.xcal").string().c_str(), &q));
    ASSERT_NE(0u, q.validMask & XPE_OFFSETQ_ROI_OUTPUT);
    EXPECT_EQ(old.dark_bias, q.darkBiasOutput) << "bit for bit";
    EXPECT_EQ(old.dsnu_adu, q.dsnuOutput) << "bit for bit";
    // the rounding of the boundary pixels is the kernel's: the figures of the whole frame follow from the corrected image
    size_t zeros = 0;
    for (uint16_t v : corrected) zeros += (v == 0) ? 1u : 0u;
    EXPECT_NEAR(100.0 * static_cast<double>(zeros) / static_cast<double>(kN), q.zeroOutputRatePct, 1e-12);
    EXPECT_EQ(0, corrected.empty());
}
