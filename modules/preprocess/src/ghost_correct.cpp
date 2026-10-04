/**
 * @file ghost_correct.cpp
 * @brief SWU-1.4: Ghost/Lag Correction Tier 1/2/3 — LTI/NLCSC deconvolution (PRE-04)
 *        Dual-exponential IRF model (ref: PMC3465354)
 *        Tier 1: LTI deconvolution
 *        Tier 2: Exposure-weighted LTI
 *        Tier 3: NLCSC (Nonlinear Causal Spatial Context)
 *        REQ-P1A-085 to REQ-P1A-087
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include "xpe_strict_parse.hpp"

#include <cstdlib>
#include <memory>
#include <new>
#include <string>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <mutex>
#include <unordered_set>
#if defined(__AVX2__) || defined(_MSC_VER)
#include <immintrin.h>
#endif

// @MX:ANCHOR: [AUTO] xpe_ghost_create — resource allocation for ghost corrector
// @MX:REASON: All ghost functions fan in; handle is the invariant contract point
// @MX:SPEC: REQ-P1A-085
// QA-A-229 M5 (#245, REQ-P1A-086): the registry of live handles. Heap-allocated and never freed on purpose: a
// handle may be destroyed while the library is being torn down, after a function-local static would be gone.
namespace {
struct GhostHandleRegistry {
    std::mutex m;
    std::unordered_set<const void*> live;
};

GhostHandleRegistry& ghost_registry()
{
    static GhostHandleRegistry* r = new GhostHandleRegistry();
    return *r;
}

/** false on allocation failure (nothing registered). */
bool ghost_register(const void* h) noexcept
{
    try {
        GhostHandleRegistry& r = ghost_registry();
        std::lock_guard<std::mutex> lock(r.m);
        r.live.insert(h);
        return true;
    } catch (...) {
        return false;
    }
}

/** true for exactly one caller per registered handle: the one that removed it. */
bool ghost_unregister(const void* h) noexcept
{
    try {
        GhostHandleRegistry& r = ghost_registry();
        std::lock_guard<std::mutex> lock(r.m);
        return r.live.erase(h) != 0;
    } catch (...) {
        return false;
    }
}
}  // namespace

bool GhostCorrectorHandle::isValid(const void* h) noexcept
{
    if (!h) return false;
    try {
        GhostHandleRegistry& r = ghost_registry();
        std::lock_guard<std::mutex> lock(r.m);
        return r.live.find(h) != r.live.end();
    } catch (...) {
        return false;
    }
}

XpeErrorCode xpe_ghost_create(uint32_t width, uint32_t height,
                               const char* configJsonOrNull,
                               void** handleOut)
{
    if (!handleOut || width == 0 || height == 0) return XPE_ERR_INVALID_INPUT;

    // REQ-P1A-085: allocate handle with frame history buffer
    // REQ-P1A-030: configJsonOrNull for IRF coefficient override
    // REQ-P1A-031: XPE_ERR_OUT_OF_MEMORY on allocation failure
    // QA-A-202 (#233): the handle is owned by `owner` until every step has succeeded, so a refusal or an
    // exception at any step frees it (it used to leak the handle and its history buffers when a number in
    // the configuration was malformed). A malformed number is XPE_ERR_CONFIG_INVALID; nothing is handed back.
    std::unique_ptr<GhostCorrectorHandle> owner(new (std::nothrow) GhostCorrectorHandle());
    if (!owner) return XPE_ERR_OUT_OF_MEMORY;
    GhostCorrectorHandle* const handle = owner.get();

    handle->width  = width;
    handle->height = height;
    const size_t pixelCount = static_cast<size_t>(width) * height;

    try {
        // Parse config JSON for tier and IRF coefficients. An absent or empty value keeps the default. The text is
        // parsed once into its top-level members; one that is not one valid JSON object, is empty, or gives a member
        // name twice is a refusal (QA-A-209, QA-A-209b). NULL is every default.
        {
            XpeConfigDoc doc;
            XpeErrorCode rc = xpe_config_parse(configJsonOrNull, &doc);
            if (rc != XPE_OK) return rc;

            std::string v;
            if (doc.getString("tier", &v) && !v.empty()) {
                int32_t tier = 1;
                if (!xpe_strict::parse_int(v, &tier)) return XPE_ERR_CONFIG_INVALID;
                handle->tier = (tier < 1 || tier > 3) ? 1 : tier;
            }
            const struct { const char* key; double* dst; } reals[] = {
                {"alpha1", &handle->alpha1}, {"tau1", &handle->tau1}, {"alpha2", &handle->alpha2},
                {"tau2", &handle->tau2}, {"tier2Threshold", &handle->tier2Threshold}, {"nlcscBeta", &handle->nlcscBeta},
            };
            for (const auto& r : reals) {
                if (doc.getString(r.key, &v) && !v.empty() && !xpe_strict::parse_double(v, r.dst))
                    return XPE_ERR_CONFIG_INVALID;
            }

            // QA-A-226 (#241): "calibrated" is a fact about the configuration -- all four lag parameters were given
            // (non-empty; an empty value keeps the default above, so it does not count). Not about their values.
            const char* const lagKeys[] = {"alpha1", "tau1", "alpha2", "tau2"};
            bool all = true;
            for (const char* k : lagKeys) all = all && doc.getString(k, &v) && !v.empty();
            handle->calibrated = all;

            // QA-A-226b/226c (#241): a calibrated set must be a forward system that can exist.
            //  - each alpha is >= 0 (a negative one ADDS signal: 1000 -> 1100 -> 1137, QA-A-226b) and each tau is > 0
            //    (zero or negative makes the decay factor exp(-1/tau) 0 or above 1, so the history grows). tau is
            //    finite by the notation rules ("inf" and "nan" are refused when the number is read).
            //  - the history the corrector subtracts has the steady-state gain
            //    S = alpha1/(1-exp(-1/tau1)) + alpha2/(1-exp(-1/tau2)) (one frame per step); a constant input comes out
            //    as input*(1-S) clamped at 0, and a real lag y = x/(1-S) exists only for S < 1, so S >= 1 is refused.
            //    Each term is alpha/(1-exp(-1/tau)) >= alpha, so alpha < 1 follows from S < 1 and needs no check of
            //    its own. With the ranges above no term is negative and none is NaN (an alpha of 0 is taken as 0
            //    whatever its tau: a tau so large that exp(-1/tau) is 1.0 makes the denominator 0, and 0/0 would be
            //    NaN); a positive alpha over a zero denominator is +inf, which S >= 1 refuses. So S is never NaN or
            //    -inf here and "S is not finite" needs no check either.
            //  Only this weight-free S is checked: the tier 2/3 exposure weight has no grounded ceiling (QA-A-226
            //  option (c), not taken).
            if (all) {
                if (!(handle->alpha1 >= 0.0) || !(handle->alpha2 >= 0.0) || !(handle->tau1 > 0.0) || !(handle->tau2 > 0.0))
                    return XPE_ERR_CONFIG_INVALID;
                const auto term = [](double alpha, double tau) {
                    return alpha == 0.0 ? 0.0 : alpha / (1.0 - std::exp(-1.0 / tau));
                };
                const double s = term(handle->alpha1, handle->tau1) + term(handle->alpha2, handle->tau2);
                if (s >= 1.0) return XPE_ERR_CONFIG_INVALID;
            }
        }

        handle->hist1.assign(pixelCount, 0.0f);
        handle->hist2.assign(pixelCount, 0.0f);
        // QA-A-237b: tier 3 keeps two rows of the incoming frame for its in-place frames (the 3x3 mean reads them).
        if (handle->tier == 3) {
            handle->rowA.assign(width, 0.0f);
            handle->rowB.assign(width, 0.0f);
        }
    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }

    // QA-A-226 (#241): one Warning per handle, at creation (not per frame). The sentence is a cross-lane contract.
    if (!handle->calibrated) {
        xpe_alert_push("XPE_WARN_GHOST_NOT_CALIBRATED: ghost correction is not calibrated: the handle passes frames "
                       "through unchanged until lag parameters are configured (alpha1, tau1, alpha2, tau2)",
                       XPE_ALERT_WARNING);
    }

    // Registered last, once nothing else can fail: a failure here frees the handle through `owner`.
    if (!ghost_register(owner.get())) return XPE_ERR_OUT_OF_MEMORY;
    *handleOut = owner.release();
    return XPE_OK;
}

bool xpe_ghost_is_calibrated(const void* handle) noexcept
{
    if (!GhostCorrectorHandle::isValid(const_cast<void*>(handle))) return false;
    return static_cast<const GhostCorrectorHandle*>(handle)->calibrated;
}

// @MX:ANCHOR: [AUTO] xpe_ghost_correct — multi-tier ghost correction with auto-escalation
// @MX:REASON: Main correction entry point; all correction logic fans in here
// @MX:SPEC: REQ-P1A-032, REQ-P1A-033 (Tier 1/2/3)

#ifdef XPE_CACHE_TEST_HOOKS
// Test-only (QA-A-225 M4, #238), see xpe_preprocess_internal.h.
XpeGhostTier3Mix xpe_ghost_tier3_mix = {0.7f, 0.3f};
bool xpe_ghost_force_slow_path = false;
unsigned long xpe_ghost_in_place_frames = 0;
#endif

namespace {
    // Helper: compute mean signal level for exposure estimation
    float compute_frame_mean(const float* px, size_t n) noexcept {
        if (n == 0) return 0.0f;
        double sum = 0.0;
        for (size_t i = 0; i < n; ++i) {
            if (std::isfinite(px[i])) {
                sum += static_cast<double>(px[i]);
            }
        }
        return static_cast<float>(sum / static_cast<double>(n));
    }

    // QA-A-237b (#245): the largest |value| of a frame whose values are all finite (the entrance check has run).
    float max_abs_finite(const float* px, size_t n) noexcept {
        size_t i = 0;
        float best = 0.0f;
#if defined(__AVX2__) || defined(_MSC_VER)
        const __m256 mask = _mm256_castsi256_ps(_mm256_set1_epi32(0x7fffffff));
        __m256 m = _mm256_setzero_ps();
        for (; i + 8 <= n; i += 8) m = _mm256_max_ps(m, _mm256_and_ps(_mm256_loadu_ps(px + i), mask));
        alignas(32) float lane[8];
        _mm256_store_ps(lane, m);
        for (int k = 0; k < 8; ++k) best = (lane[k] > best) ? lane[k] : best;
#endif
        for (; i < n; ++i) {
            const float a = std::fabs(px[i]);
            best = (a > best) ? a : best;
        }
        return best;
    }

    // What a tier works out about the frame as a whole. It is committed to the handle with the history, and
    // only when the frame succeeded (QA-A-202c).
    struct FrameStats {
        bool  set{false};
        float meanSignal{0.0f};
        float exposureWeight{1.0f};
    };

    // QA-A-237b (#245): the planes a tier reads and writes. A frame reads h1/h2 and writes the NEW history to n1/n2: scratch
    // planes (the route with an undo; `frameCopy` is then the incoming frame) or the history planes themselves (in place:
    // n1 == h1 and n2 == h2, used only for a frame that provably cannot fail; `frameCopy` is then null). A pixel reads its
    // own h1/h2 before it writes n1/n2, so one plane serves as both.
    struct Planes {
        const float* h1;
        const float* h2;
        float* n1;
        float* n2;
    };

    // The per-frame constants of the three tiers, bundled so that no function takes four floats in a row (clang-tidy
    // bugprone-easily-swappable-parameters): the decay of each history plane and the base coefficient of each.
    struct TierCoeffs {
        float decay1;
        float decay2;
        float a1;
        float a2;
    };

    // The weights of the tier-3 blend (the shipped constants, or the test seam's variables).
    struct Tier3Mix {
        float keep;
        float local;
    };

    // Tier 1: Standard LTI deconvolution
    XpeErrorCode ghost_tier1(GhostCorrectorHandle*, float* px, size_t n, const TierCoeffs& k, FrameStats*,
                             const Planes& pl, const float*) {
        const float* h1 = pl.h1;
        const float* h2 = pl.h2;
        float* n1 = pl.n1;
        float* n2 = pl.n2;
        const float a1 = k.a1, a2 = k.a2, decay1 = k.decay1, decay2 = k.decay2;
        for (size_t i = 0; i < n; ++i) {
            const float raw = px[i];
            if (!std::isfinite(raw)) return XPE_ERR_PROCESSING_FAILED;
            float corrected = raw - a1 * h1[i] - a2 * h2[i];
            n1[i] = decay1 * h1[i] + raw;
            n2[i] = decay2 * h2[i] + raw;
            if (!std::isfinite(corrected) || !std::isfinite(n1[i]) || !std::isfinite(n2[i])) return XPE_ERR_PROCESSING_FAILED;
            px[i] = (corrected > 0.0f) ? corrected : 0.0f;
        }
        return XPE_OK;
    }

    // Tier 2: Exposure-weighted LTI deconvolution
    XpeErrorCode ghost_tier2(GhostCorrectorHandle*, float* px, size_t n, const TierCoeffs& k, FrameStats* stats,
                             const Planes& pl, const float*) {
        const float decay1 = k.decay1, decay2 = k.decay2, a1_base = k.a1, a2_base = k.a2;
        // Exposure-weighted coefficients based on frame mean
        const float meanSignal = compute_frame_mean(px, n);

        // Scale alpha based on exposure level (higher exposure = stronger correction)
        // Reference: 50% of saturation = 1.0x scaling, 100% saturation = 1.5x scaling
        const float exposureWeight = 1.0f + (meanSignal / 32768.0f) * 0.5f;
        *stats = FrameStats{true, meanSignal, exposureWeight};

        const float a1 = a1_base * exposureWeight;
        const float a2 = a2_base * exposureWeight;

        const float* h1 = pl.h1;
        const float* h2 = pl.h2;
        float* n1 = pl.n1;
        float* n2 = pl.n2;
        for (size_t i = 0; i < n; ++i) {
            const float raw = px[i];
            if (!std::isfinite(raw)) return XPE_ERR_PROCESSING_FAILED;
            float corrected = raw - a1 * h1[i] - a2 * h2[i];
            n1[i] = decay1 * h1[i] + raw;
            n2[i] = decay2 * h2[i] + raw;
            if (!std::isfinite(corrected) || !std::isfinite(n1[i]) || !std::isfinite(n2[i])) return XPE_ERR_PROCESSING_FAILED;
            px[i] = (corrected > 0.0f) ? corrected : 0.0f;
        }
        return XPE_OK;
    }

    // Tier 3: NLCSC (Nonlinear Causal Spatial Context) with signal-dependent coefficients
    XpeErrorCode ghost_tier3(GhostCorrectorHandle* gh, float* px, size_t n, const TierCoeffs& k, FrameStats* stats,
                             const Planes& pl, const float* frameCopy) {
        const float decay1 = k.decay1, decay2 = k.decay2, a1_base = k.a1, a2_base = k.a2;
        const float meanSignal = compute_frame_mean(px, n);
        const float exposureWeight = 1.0f + (meanSignal / 32768.0f) * 0.5f;
        *stats = FrameStats{true, meanSignal, exposureWeight};

        const float* h1 = pl.h1;
        const float* h2 = pl.h2;
        float* n1 = pl.n1;
        float* n2 = pl.n2;

        const float beta = static_cast<float>(gh->nlcscBeta); // signal dependency parameter
        const uint32_t W = gh->width;
        const uint32_t H = gh->height;

        // QA-A-218b (#233): the 3x3 mean reads the frame AS IT CAME IN, not the pixels this loop has already overwritten.
        // The loop updates px in place, so reading px made the pixels above and to the left count with their CORRECTED
        // (and zero-clamped) values and the others with their original ones: the output depended on the scan direction
        // (a frame flipped, processed and flipped back differed in 41-91 % of its pixels, QA-A-218).
        // QA-A-237b: the incoming frame is `frameCopy` (the route with an undo keeps a whole copy) or, for an in-place frame,
        // the rows of px that have not been written yet (y + 1 and below) plus two saved rows (y - 1 and y).
        float* savedPrev = gh->rowA.data();
        float* savedCur = gh->rowB.data();

        // The blend weights of the interior pixels. A constant in the shipped library; the test seam reads them from a variable
        // (QA-A-225 M4), with the shipped values as defaults.
#ifdef XPE_CACHE_TEST_HOOKS
        const float mixKeep = xpe_ghost_tier3_mix.keep, mixLocal = xpe_ghost_tier3_mix.local;
#else
        constexpr float mixKeep = 0.7f, mixLocal = 0.3f;
#endif

        // QA-A-227 (#244): EVERY pixel is blended, the mean taken over the neighbours that exist in the frame (nine inside, six
        // on an edge, four in a corner -- the module's "valid neighbours" convention, as in the defect stage). A frame below
        // 3x3 has no spatial context and is not blended. The blend used to be applied only where all eight neighbours exist
        // (the index arithmetic wrapped to the next row at x = 0), which left the one-pixel border without it: on a uniform
        // frame the interior sat above the border by mixLocal * (raw - corrected value), a ring.
        const bool blend = (W >= 3u && H >= 3u);

        for (size_t y = 0; y < H; ++y) {
            const float* rowPrev = nullptr;   // the incoming frame's rows y - 1, y, y + 1 (null outside the frame)
            const float* rowCur = nullptr;
            const float* rowNext = nullptr;
            if (blend) {
                if (frameCopy != nullptr) {
                    rowPrev = (y > 0) ? frameCopy + (y - 1) * W : nullptr;
                    rowCur = frameCopy + y * W;
                    rowNext = (y + 1 < H) ? frameCopy + (y + 1) * W : nullptr;
                } else {
                    std::memcpy(savedCur, px + y * W, static_cast<size_t>(W) * sizeof(float));
                    rowPrev = (y > 0) ? savedPrev : nullptr;
                    rowCur = savedCur;
                    rowNext = (y + 1 < H) ? px + (y + 1) * W : nullptr;
                }
            }

            for (size_t x = 0; x < W; ++x) {
                const size_t i = y * W + x;
                const float raw = px[i];
                if (!std::isfinite(raw)) return XPE_ERR_PROCESSING_FAILED;

                // Signal-dependent coefficient: higher signal = stronger correction
                const float signalDependence = 1.0f + beta * (raw / 32768.0f);
                const float a1 = a1_base * exposureWeight * signalDependence;
                const float a2 = a2_base * exposureWeight * signalDependence;

                float corrected = raw - a1 * h1[i] - a2 * h2[i];

                // Spatial context: blend with local neighborhood mean (3x3).
                if (blend) {
                    // QA-A-227b: unsigned coordinates and bounds (see xpe_ghost_neighbour_span) -- a frame axis above INT_MAX
                    // used to turn negative in an int cast and put every neighbour outside the frame.
                    size_t x0, x1, y0, y1;
                    xpe_ghost_neighbour_span(x, W, &x0, &x1);
                    xpe_ghost_neighbour_span(y, H, &y0, &y1);
                    float localMean = 0.0f;
                    int count = 0;
                    for (size_t yy = y0; yy <= y1; ++yy) {
                        const float* row = (yy < y) ? rowPrev : (yy == y) ? rowCur : rowNext;
                        for (size_t xx = x0; xx <= x1; ++xx) {
                            const float v = row[xx];
                            if (std::isfinite(v)) {
                                localMean += v;
                                ++count;
                            }
                        }
                    }
                    if (count > 0) {
                        localMean /= static_cast<float>(count);
                        // Blend corrected with local mean (0.7 : 0.3)
                        corrected = mixKeep * corrected + mixLocal * localMean;
                    }
                }

                n1[i] = decay1 * h1[i] + raw;
                n2[i] = decay2 * h2[i] + raw;

                if (!std::isfinite(corrected) || !std::isfinite(n1[i]) || !std::isfinite(n2[i])) return XPE_ERR_PROCESSING_FAILED;
                px[i] = (corrected > 0.0f) ? corrected : 0.0f;
            }

            if (blend && frameCopy == nullptr) {
                float* t = savedPrev;   // the row just finished is now the "previous" row
                savedPrev = savedCur;
                savedCur = t;
            }
        }
        return XPE_OK;
    }

    // QA-A-237b (#245), QA-A-237d (Codex #142): whether the frame's arithmetic provably stays inside the float range, so that no
    // pixel can fail and an in-place update has nothing to undo. The check EVALUATES THE TIERS' OWN EXPRESSIONS, in float and in
    // the tiers' own order, at the worst case of every input: M (the frame's largest |value|) stands for the pixel and, since
    // |mean| <= M, for the mean; the handle's bounds stand for the history planes; coefficients enter as magnitudes. An
    // expression of the tier that can overflow in float is evaluated the same way here and tested for finiteness -- the
    // coefficient a1 = a1_base * exposureWeight * signalDependence among them, which overflows to +Inf long before a1 * h1 does
    // (the first version bounded the product a1 * h1 in double and missed it: a small history, then a large frame).
    // Every value must be finite and at most kLimit (1e37, a thirty-fourth of FLT_MAX), which covers the rounding of the real run.
    // NaN and Inf answer false. The correspondence (any change to a tier expression changes its row here):
    //
    //   tier code                                                  evaluated here
    //   exposureWeight = 1 + (mean / 32768) * 0.5     (tiers 2,3)  ew   = 1 + (M / 32768) * 0.5
    //   signalDependence = 1 + beta * (raw / 32768)   (tier 3)     sd   = 1 + |beta| * (M / 32768)
    //   a1 = a1_base * exposureWeight * signalDependence           a1w  = |a1_base| * ew * sd      (same order)
    //   a2 = a2_base * exposureWeight * signalDependence           a2w  = |a2_base| * ew * sd
    //   corrected = raw - a1 * h1 - a2 * h2                        corr = M + a1w * H1 + a2w * H2   (and each product)
    //   localMean: running sum of up to 9 neighbours  (tier 3)     sum9 = 9 * M
    //   corrected = keep * corrected + local * localMean (tier 3)  blend = |keep| * corr + |local| * M
    //   n1 = decay1 * h1 + raw ; n2 = decay2 * h2 + raw            n1w  = decay1 * H1 + M ; n2w = decay2 * H2 + M
    bool frame_cannot_leave_float_range(const GhostCorrectorHandle& gh, float M, const TierCoeffs& k,
                                        const Tier3Mix& mix) noexcept {
        constexpr double kLimitD = 1.0e37;
        constexpr float kLimit = 1.0e37f;
        // The history bounds are doubles kept by the handle; past the limit already answers "no" (and keeps the float cast defined).
        if (!(gh.histBound1 <= kLimitD) || !(gh.histBound2 <= kLimitD)) return false;
        const float H1 = static_cast<float>(gh.histBound1);
        const float H2 = static_cast<float>(gh.histBound2);
        const auto fits = [kLimit](float v) { return std::isfinite(v) && std::fabs(v) <= kLimit; };
        if (!fits(M)) return false;

        float ew = 1.0f, sd = 1.0f;
        if (gh.tier == 2 || gh.tier == 3) ew = 1.0f + (M / 32768.0f) * 0.5f;
        if (gh.tier == 3) sd = 1.0f + std::fabs(static_cast<float>(gh.nlcscBeta)) * (M / 32768.0f);
        const float a1w = std::fabs(k.a1) * ew * sd;
        const float a2w = std::fabs(k.a2) * ew * sd;
        const float t1 = a1w * H1;
        const float t2 = a2w * H2;
        const float corr = M + t1 + t2;
        const float n1w = k.decay1 * H1 + M;
        const float n2w = k.decay2 * H2 + M;
        if (!(fits(ew) && fits(sd) && fits(a1w) && fits(a2w) && fits(t1) && fits(t2) && fits(corr) && fits(n1w) && fits(n2w))) return false;
        if (gh.tier == 3) {
            const float sum9 = 9.0f * M;
            const float blend = std::fabs(mix.keep) * corr + std::fabs(mix.local) * M;
            if (!(fits(sum9) && fits(blend))) return false;
        }
        return true;
    }
} // anonymous namespace

XpeErrorCode xpe_ghost_correct(void* handle, XpeImageBuffer* img,
                                const XpeImageMetadata* meta)
{
    if (!GhostCorrectorHandle::isValid(handle) || !img || !meta)
        return XPE_ERR_INVALID_INPUT;

    auto* gh = static_cast<GhostCorrectorHandle*>(handle);
    if (img->width != gh->width || img->height != gh->height)
        return XPE_ERR_INVALID_INPUT;
    size_t n = 0;
    if (!xpe_buffer_has_format(img, XPE_PIXEL_FLOAT32, &n)) return XPE_ERR_INVALID_INPUT;

    // SRS-CALIB-NFR-003: one call at a time per handle (the history is updated in place)
    std::lock_guard<std::mutex> lock(gh->mtx);

    auto* px = static_cast<float*>(img->data);

    // QA-A-217 (#233): A NON-FINITE FRAME IS REFUSED AT THE ENTRANCE, before anything is written. The tiers used to
    // work through the frame pixel by pixel and give up with XPE_ERR_PROCESSING_FAILED at the first NaN or infinity,
    // with every pixel before it already corrected (REQ-P1A-032 asks for the output to be left unmodified on
    // failure). Same rule as the defect, binning and runtime-detection stages (QA-A-214b, QA-A-215): INVALID_INPUT,
    // the buffer and the handle untouched, one Error alert with the count and the first pixel. (The history was
    // never at risk -- it is committed only for a whole successful frame, QA-A-202c; the test pins that.)
    {
        size_t count = 0, first = 0;
        if (xpe_find_nonfinite(px, n, &count, &first)) {
            xpe_alert_nonfinite("XPE_WARN_GHOST_INPUT_NOT_FINITE:", count, first, img->width,
                                "the frame was not corrected; the buffer and the handle's history were not changed");
            return XPE_ERR_INVALID_INPUT;
        }
    }

    // QA-A-226 (#241): a handle without calibrated lag parameters does not correct. It is after the entrance checks
    // (so a bad frame is refused exactly as before) and before anything is written: the pixels, the history and the
    // time of the last frame stay as they are.
    if (!gh->calibrated) return XPE_OK;

    // REQ-P1A-033, QA-A-226b (#241): tau is in FRAMES. Every successful call is one step (dt = 1); acquisitionTime is
    // not used. Before this the step was the difference of the integer-second times when both were given and 1
    // otherwise, so the same tau was seconds or frames depending on the input, and the gap between two frames was
    // applied one frame late. A break in the sequence is the caller's xpe_ghost_reset().
    const float decay1 = static_cast<float>(std::exp(-1.0 / gh->tau1));
    const float decay2 = static_cast<float>(std::exp(-1.0 / gh->tau2));
    const float a1_base = static_cast<float>(gh->alpha1);
    const float a2_base = static_cast<float>(gh->alpha2);

    // A frame can still fail with a finite input (a corrected value, or the new history, overflows float at the
    // extremes of the range): REQ-P1A-032 asks for the pixels to be put back as they came in, and the history, which only
    // a successful frame commits, to stay as it was (QA-A-217, QA-A-202c).
    // QA-A-237b (#245): a frame whose arithmetic provably cannot overflow is processed in place on the history planes (it
    // cannot fail, so there is nothing to put back); every other frame takes the route the handle used to take for all of
    // them, with scratch planes that live for this call only.
    const float maxAbs = max_abs_finite(px, n);
#ifdef XPE_CACHE_TEST_HOOKS
    const float mixKeep = xpe_ghost_tier3_mix.keep, mixLocal = xpe_ghost_tier3_mix.local;
#else
    constexpr float mixKeep = 0.7f, mixLocal = 0.3f;
#endif
    const TierCoeffs coeffs{decay1, decay2, a1_base, a2_base};
    bool inPlace = frame_cannot_leave_float_range(*gh, maxAbs, coeffs, Tier3Mix{mixKeep, mixLocal});
#ifdef XPE_CACHE_TEST_HOOKS
    if (xpe_ghost_force_slow_path) inPlace = false;
#endif

    XpeErrorCode result = XPE_OK;
    FrameStats stats;
    const auto run_tier = [&](const Planes& pl, const float* frameCopy) {
        switch (gh->tier) {
            case 1: return ghost_tier1(gh, px, n, coeffs, &stats, pl, frameCopy);
            case 2: return ghost_tier2(gh, px, n, coeffs, &stats, pl, frameCopy);
            case 3: return ghost_tier3(gh, px, n, coeffs, &stats, pl, frameCopy);
            default: return ghost_tier1(gh, px, n, coeffs, &stats, pl, frameCopy);
        }
    };
    if (inPlace) {
        const Planes pl{gh->hist1.data(), gh->hist2.data(), gh->hist1.data(), gh->hist2.data()};
        result = run_tier(pl, nullptr);
        if (result != XPE_OK) {
            // Not reachable: the bound above proves no pixel fails. If it ever were, the history is half written and the
            // pixels cannot be put back; leave a consistent (empty) state and say so rather than carry a mixed history on.
            std::fill(gh->hist1.begin(), gh->hist1.end(), 0.0f);
            std::fill(gh->hist2.begin(), gh->hist2.end(), 0.0f);
            gh->histBound1 = 0.0;
            gh->histBound2 = 0.0;
            return result;
        }
#ifdef XPE_CACHE_TEST_HOOKS
        ++xpe_ghost_in_place_frames;
#endif
    } else {
        std::vector<float> next1, next2, backup;
        try {
            next1.assign(n, 0.0f);
            next2.assign(n, 0.0f);
            backup.assign(n, 0.0f);
        } catch (const std::bad_alloc&) {
            return XPE_ERR_OUT_OF_MEMORY;   // nothing has been written
        }
        std::memcpy(backup.data(), px, n * sizeof(float));
        const Planes pl{gh->hist1.data(), gh->hist2.data(), next1.data(), next2.data()};
        result = run_tier(pl, backup.data());
        if (result != XPE_OK) {
            // the history, the time and the exposure estimate are as they were; the pixels are put back
            std::memcpy(px, backup.data(), n * sizeof(float));
            return result;
        }
        gh->hist1.swap(next1);
        gh->hist2.swap(next2);
    }

    // The whole frame succeeded: its history, its time and its exposure estimate become the handle's.
    constexpr double kSlack = 1.0 + 1.0e-6;   // above the float rounding of one history update (about 1.2e-7)
    gh->histBound1 = (static_cast<double>(decay1) * gh->histBound1 + static_cast<double>(maxAbs)) * kSlack;
    gh->histBound2 = (static_cast<double>(decay2) * gh->histBound2 + static_cast<double>(maxAbs)) * kSlack;
    if (stats.set) {
        gh->lastFrameMean = stats.meanSignal;
        gh->exposureWeight = stats.exposureWeight;
    }
    return XPE_OK;
}

XpeErrorCode xpe_ghost_reset(void* handle)
{
    if (!GhostCorrectorHandle::isValid(handle)) return XPE_ERR_INVALID_INPUT;
    auto* gh = static_cast<GhostCorrectorHandle*>(handle);
    std::lock_guard<std::mutex> lock(gh->mtx);   // SRS-CALIB-NFR-003
    // REQ-P1A-088: clear accumulated frame history
    std::fill(gh->hist1.begin(), gh->hist1.end(), 0.0f);
    std::fill(gh->hist2.begin(), gh->hist2.end(), 0.0f);
    gh->histBound1 = 0.0;   // QA-A-237b: an empty history is bounded by zero
    gh->histBound2 = 0.0;
    gh->lastFrameMean = 0.0f;
    gh->exposureWeight = 1.0;
    return XPE_OK;
}

void xpe_ghost_destroy(void* handle)
{
    // The removal is the decision: only the caller that takes the handle out of the registry may free it, so a
    // handle destroyed twice, or by several threads at once, is freed once; a pointer that was never handed out
    // (or is not a handle at all) is left alone.
    if (!ghost_unregister(handle)) return;
    auto* gh = static_cast<GhostCorrectorHandle*>(handle);
    gh->magic = 0; // invalidate before delete
    delete gh;
}
