// clahe_bin_internal.h -- the bin of a pixel in CLAHE, EXACTLY (module-internal, header-inline; plain // comments on purpose, this
// file is not part of the documented API).
//
// QA-B-207d (Codex #143, the third hold on the same requirement): the bin of a float pixel v on an image whose minimum is
// vmin and maximum vmax is
//
//     bin = floor( (v - vmin) * 4095 / (vmax - vmin) )          (0 .. 4095, v == vmax gives 4095)
//
// as a RATIONAL number, with no rounding anywhere. Two ways to get it, and a proven rule for which one may be used:
//
//   FAST   double arithmetic, valid only where the derivation below PROVES every step exact.
//   EXACT  fixed-width integer arithmetic (320 bits) built from the sign, exponent and mantissa of the three floats.
//          Integer operations only, so /fp:fast (this module is built with it) cannot touch it. Valid for every finite float.
//
// ---- Why the FAST path is exact where it is allowed -------------------------------------------------------------------
// A finite float is m * 2^e with an integer m < 2^24. Let L be an exponent such that every value involved is an integer
// multiple of 2^L. Then D = v - vmin and R = vmax - vmin are integer multiples of 2^L as well: D = D' * 2^L, R = R' * 2^L.
// If R' < 2^41 (and 0 <= D' <= R'):
//   * D and R are exactly representable in double (an integer below 2^53 times a power of two), so the double subtractions
//     `double(v) - double(vmin)` and `double(vmax) - double(vmin)` have NO rounding error;
//   * n = 4095 * D has the integer 4095 * D' < 2^12 * 2^41 = 2^53: exact. And b * R with b <= 4095 likewise: exact;
//   * so the comparison `b * R <= n` is the exact rational comparison b <= 4095 * D / R.
// The bin is then found as ESTIMATE + ONE EXACT COMPARISON: est = floor(D * inv_low) with inv_low = (4095 / R) * (1 - 1e-12).
// The reciprocal and the product carry a relative error of at most ~3e-16, the bias is 1e-12, so for q = 4095 * D / R <= 4095:
//   q * (1 - 1e-12) * (1 + 3e-16) < q  and  q - q * 1e-12 * (1 - 3e-16) > q - 1     =>   floor(q) - 1 <= est <= floor(q);
// one test `(est + 1) * R <= n` adds the missing 1. est <= 4094, so the result is <= 4095 without any clamp or mask.
//
// When is "every value is a multiple of 2^L and R' < 2^41" known WITHOUT looking at every pixel? For the image minimum vmin:
//   (A) vmin == 0:  D = v, R = vmax. No subtraction happens, so nothing can round. 4095 * v and b * vmax are products of a 24-bit
//       mantissa by a 12-bit integer (36 bits): exact. FAST is always exact.
//   (B) vmin > 0:   every pixel v >= vmin > 0 has an exponent >= vmin's, hence a last mantissa bit at least as high: every pixel
//       and vmin are multiples of L = lsb(vmin) = 2^(max(ef,1) - 150) (ef: the biased exponent field). R < vmax < 2^(msb(vmax)+1),
//       so R' < 2^(msb(vmax) + 1 - lsbexp(vmin)); FAST iff that exponent S <= 41.
//   (C) vmin < 0:   a pixel of tiny magnitude has a much lower last bit than the extremes (v = 2^-100 in an image from -1 to
//       4095), so the extremes alone prove nothing; the lowest last-bit exponent over all nonzero pixels, Lpix, is needed (one
//       extra pass, only for images with a negative minimum). Every pixel is then a multiple of 2^Lpix, and
//       R <= |vmin| + |vmax| < 2^(H + 2) with H = msb of the larger magnitude; FAST iff S = H + 2 - Lpix <= 41.
// Everything else takes the EXACT path. In practice: raw / offset-corrected images (minimum 0) and positive images whose range is
// within 2^41 last-mantissa-bits of their minimum (vmin = 1 admits vmax < 262144) stay on FAST; an image whose minimum is a tiny
// positive number next to a large maximum, or a negative-minimum image carrying tiny values, takes EXACT (about 100 ns per pixel).
//
// ---- Tier 1b: the same proof with 64-bit integers, for S in (41, 52] (QA-B-207e, Codex #145) -------------------------------------
// When R' = R / 2^L is below 2^52 but not below 2^41, doubles cannot hold the PRODUCTS exactly (4095 * D' needs up to 64 bits) but
// the integers D' = (v - vmin) / 2^L and R' themselves fit 52 bits. Then X(x) = x / 2^L is an exact int64 (mantissa << (lsbexp(x) - L),
// an integer shift, no rounding), D' = X(v) - X(vmin), R' = X(vmax) - X(vmin) are exact int64 differences, and N = 4095 * D' and
// (bin + 1) * R' are exact uint64 products (4095 * 2^52 < 2^64). The bin is the same "estimate (never high, at most one low) + one
// exact comparison" as tier 1, with the comparison done in integers. The estimate uses D' and R' converted to double: both are below
// 2^53, so the conversions are exact and the error analysis of tier 1 applies unchanged. Why it exists: an image whose values are
// integers on a range of 4095 (HU -1024..3071) has t = integer for EVERY pixel; one fractional pixel (0.001) pushes S to 46, and a
// per-pixel filter alone would send all 9.4 million pixels to the 320-bit path (measured 0.63-0.74 s). Tier 1b resolves the pixels the
// tier-2 filter cannot certify (below), so such an image costs the filter plus a cheap integer comparison per pixel.
//
// ---- Tier 2: a per-pixel error filter, for the images the derivation above does not cover (QA-B-207e) --------------
// The image-level proof is conservative: ONE small positive pixel (v = 0.0123 in an image up to 4000) makes S > 41 for the whole
// image, and sending every pixel of such an image to the EXACT path costs 12-15x (measured 0.56-0.72 s against 46 ms on 3072x3072).
// A pixel does not need the image-level proof if its quotient is provably far from an integer:
//   D^ = fl(v - vmin) = D (1 + e1),   R^ = fl(vmax - vmin) = R (1 + e2),   inv = fl(4095 / R^) = (4095 / R^)(1 + e3),
//   t^ = fl(D^ * inv) = D^ inv (1 + e4),                     |e1..e4| <= u = 2^-53   (IEEE-754 binary64, round to nearest)
// so t^ = t (1 + e) with t = 4095 D / R the exact quotient and |e| <= (1+u)^3 / (1-u) - 1 < 5u. The subtraction's rounding error is
// relative to the EXACT difference, so a small term "lost" in `v - vmin` (Codex #143: vmin = 2^-100, v = 1) is inside e1; no
// special case. With t <= 4095:   |t^ - t| <= 4095 * 5u = 2.3e-12  (kFilterAbsErr).
// If frac(t^) lies in (kFilterMargin, 1 - kFilterMargin) with kFilterMargin = 1e-9 (> 400 x the bound, a margin generous enough to
// absorb a few extra roundings), then t lies in the same open interval (k, k+1) as t^ and floor(t) = floor(t^) = k: PROVEN, with
// no assumption about the exponents of v, vmin, vmax. A pixel with frac(t^) within kFilterMargin of an integer (about 2e-9 of the
// pixels of a continuous distribution) is resolved exactly: by tier 1b when 41 < S <= 52, else by the 320-bit path. v == vmin and
// v == vmax are bins 0 and 4095 by definition and skip it. Measured: 0 of 9.4 million pixels of a continuous 3072x3072 image.
// /fp:fast: this module is compiled with it, and it licenses reassociation / distribution (v*inv - vmin*inv would cancel
// catastrophically). The error bound above counts four correctly rounded operations, so the filter is compiled in `precise` mode:
// `#pragma float_control(precise, on)` around EVERY operation the bound counts: precise_range, precise_inv (R^, inv) and
// filter_certifies / filtered_bin / fill_bins_filtered (v - vmin, the product, floor, the fraction). All are noinline (MSVC; precise
// forbids reassociation and the operations are single products / differences with nothing to contract into an FMA), so the mode
// of the /fp:fast code that calls them cannot reach their bodies. The EXACT path is integer-only. Checked in the compiled object,
// not only in the source: QA-B-207 report_f.md (disassembly: no FMA, the functions are separate, not inlined).
//
// ---- Why the EXACT path is exact --------------------------------------------------------------------------------------
// Every finite float is an integer multiple of 2^-149 below 2^128, i.e. an integer below 2^277 in that unit. OFF = 2^277 is added
// so that signed values become non-negative integers X(x) = OFF + x * 2^149 (< 2^278): subtraction of two X is then plain
// unsigned subtraction and D = X(v) - X(vmin), R = X(vmax) - X(vmin) are exact. 4095 * D < 2^290 and b * R < 2^290, so five
// 64-bit words (320 bits) hold every intermediate. bin = floor(N / R) with N = 4095 * D is found from a double estimate of the
// quotient (error far below 1) corrected by exact multiword comparisons of b * R against N.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace xpe_clahe {

inline constexpr int kBins = 4096;
// Bits a double holds exactly (53) minus the 12 bits of the factor 4095 / 4096: the largest R' = R / 2^L the FAST path may see.
inline constexpr int kFastSpanBits = 41;
// Tier 1b (QA-B-207e, Codex #145): the same proof with 64-bit INTEGERS instead of doubles. D' and R' (integers, units of 2^L) stay
// below 2^52, so 4095 * D' and (bin + 1) * R' are exact uint64 products (4095 * 2^52 < 2^64) and the exact comparison needs no
// double at all: span S in (41, 52].
inline constexpr int kInt64SpanBits = 52;
// Tier-2 filter (derivation above): relative error 5u of t^, absolute bound for t <= 4095, and the margin used.
inline constexpr double kFilterRelErr = 5.0 * 1.1102230246251565e-16;                      // 5 * 2^-53
inline constexpr double kFilterAbsErr = 4095.0 * kFilterRelErr;                              // 2.3e-12
inline constexpr double kFilterMargin = 1e-9;
static_assert(kFilterMargin >= 100.0 * kFilterAbsErr, "the filter margin must dominate the derived error bound");


// ---- float helpers --------------------------------------------------------------------------------------------------
inline uint32_t float_bits(float x) {
    uint32_t u;
    std::memcpy(&u, &x, sizeof u);
    return u;
}

// Exponent of the last mantissa bit of a finite float (denormals share the lowest one): max(ef, 1) - 150.
inline int lsb_exp(float x) {
    const uint32_t ef = (float_bits(x) >> 23) & 0xFFu;
    return static_cast<int>(ef > 1u ? ef : 1u) - 150;
}

// Lowest last-bit exponent over the nonzero pixels (zero is a multiple of everything). All zeros: a large value (no constraint).
inline int min_lsb_exp(const float* p, uint64_t n) {
    uint32_t lowest = 255u;
    for (uint64_t i = 0; i < n; ++i) {
        const uint32_t u = float_bits(p[i]) & 0x7FFFFFFFu;
        const uint32_t ef = u >> 23;
        const uint32_t e = ef > 1u ? ef : 1u;
        lowest = (u != 0u && e < lowest) ? e : lowest;
    }
    return static_cast<int>(lowest) - 150;
}

// The span S (in bits) that the proofs of cases A, B, C bound R' = R / 2^L by: R' < 2^S. 0 for case A (no subtraction, no span
// needed). min_lsb is read only when vmin < 0.
inline int proof_span_bits(float vmin, float vmax, int min_lsb) {
    if (vmin == 0.0f) return 0;                                                          // (A)
    if (vmin > 0.0f) return std::ilogb(vmax) + 1 - lsb_exp(vmin);                         // (B)
    int h = std::ilogb(std::fabs(vmin));                                                 // (C)
    if (vmax != 0.0f) h = std::max(h, std::ilogb(std::fabs(vmax)));
    return h + 2 - min_lsb;
}

// The FAST (double) path is proven exact for this image (cases A, B, C above). min_lsb is read only when vmin < 0.
inline bool fast_path_is_exact(float vmin, float vmax, int min_lsb) {
    return proof_span_bits(vmin, vmax, min_lsb) <= kFastSpanBits;
}

// The unit exponent L of tier 1b: every pixel is an integer multiple of 2^L (cases B and C).
inline int unit_exp(float vmin, int min_lsb) { return vmin > 0.0f ? lsb_exp(vmin) : min_lsb; }

// x / 2^L as an exact integer (x a multiple of 2^L, |x| < 2^52 * 2^L).
inline int64_t scaled_int(float x, int unit) {
    const uint32_t u = float_bits(x);
    const uint32_t ef = (u >> 23) & 0xFFu;
    const uint32_t frac = u & 0x7FFFFFu;
    const int64_t m = ef != 0u ? static_cast<int64_t>(frac | 0x800000u) : static_cast<int64_t>(frac);
    const int sh = static_cast<int>(ef > 1u ? ef : 1u) - 150 - unit;       // >= 0 for every nonzero pixel; m == 0 needs no shift
    const int64_t mag = m == 0 ? 0 : (m << sh);
    return (u >> 31) != 0u ? -mag : mag;
}

// ---- 320-bit unsigned integer ---------------------------------------------------------------------------------------
struct U320 {
    uint64_t w[5];
};

inline U320 u320_from_shifted(uint32_t m, int s) {   // m < 2^24, 0 <= s <= 277
    U320 r{};
    const int word = s >> 6;
    const int bit = s & 63;
    r.w[word] |= static_cast<uint64_t>(m) << bit;
    if (bit != 0 && word + 1 < 5) r.w[word + 1] |= static_cast<uint64_t>(m) >> (64 - bit);
    return r;
}

inline U320 u320_add(const U320& a, const U320& b) {
    U320 r;
    uint64_t carry = 0;
    for (int i = 0; i < 5; ++i) {
        const uint64_t s = a.w[i] + b.w[i];
        const uint64_t c1 = s < a.w[i] ? 1u : 0u;
        const uint64_t t = s + carry;
        const uint64_t c2 = t < s ? 1u : 0u;
        r.w[i] = t;
        carry = c1 + c2;
    }
    return r;
}

inline U320 u320_sub(const U320& a, const U320& b) {   // a >= b
    U320 r;
    uint64_t borrow = 0;
    for (int i = 0; i < 5; ++i) {
        const uint64_t d = a.w[i] - b.w[i];
        const uint64_t b1 = a.w[i] < b.w[i] ? 1u : 0u;
        const uint64_t t = d - borrow;
        const uint64_t b2 = d < borrow ? 1u : 0u;
        r.w[i] = t;
        borrow = b1 + b2;
    }
    return r;
}

inline U320 u320_mul_small(const U320& a, uint32_t k) {   // k < 2^20; the product must fit 320 bits
    U320 r;
    uint64_t carry = 0;
    for (int i = 0; i < 5; ++i) {
        const uint64_t lo = (a.w[i] & 0xFFFFFFFFu) * k + carry;       // 32 + 20 bits plus a small carry: no overflow
        const uint64_t hi = (a.w[i] >> 32) * k + (lo >> 32);
        r.w[i] = (lo & 0xFFFFFFFFu) | (hi << 32);
        carry = hi >> 32;
    }
    return r;
}

inline int u320_cmp(const U320& a, const U320& b) {
    for (int i = 4; i >= 0; --i) {
        if (a.w[i] != b.w[i]) return a.w[i] < b.w[i] ? -1 : 1;
    }
    return 0;
}

inline double u320_to_double(const U320& a) {   // approximate (~2^-53), used only for the estimate
    int i = 4;
    while (i > 0 && a.w[i] == 0u) --i;
    double v = static_cast<double>(a.w[i]);
    if (i > 0) v += static_cast<double>(a.w[i - 1]) * 5.421010862427522e-20;   // 2^-64
    return std::ldexp(v, 64 * i);
}

// OFF + x * 2^149 as a non-negative integer (OFF = 2^277).
inline U320 u320_from_float(float x) {
    const uint32_t u = float_bits(x);
    const uint32_t ef = (u >> 23) & 0xFFu;
    const uint32_t frac = u & 0x7FFFFFu;
    const uint32_t m = ef != 0u ? (frac | 0x800000u) : frac;
    const int shift = static_cast<int>(ef > 1u ? ef : 1u) - 1;      // lsb exponent + 149, 0 .. 253
    const U320 mag = u320_from_shifted(m, shift);
    U320 off{};
    off.w[4] = static_cast<uint64_t>(1) << 21;                      // 2^277 = word 4, bit 21
    return (u >> 31) != 0u ? u320_sub(off, mag) : u320_add(off, mag);
}

// ---- the floating-point operations the tier-2 proof relies on, compiled in `precise` mode -----------------------------------
// The 5u error bound counts R^ = fl(vmax - vmin) and inv = fl(4095 / R^) as single correctly rounded operations, exactly like the
// per-pixel ones in the filter. This module is built with /fp:fast; `#pragma float_control` is per FUNCTION, so these three are
// separate functions inside a precise region, and they are marked noinline so that no /fp:fast caller can pull their body (and
// its mode) into itself. (Codex #147: the constructor used to compute them in fast mode.) inv_low is the biased estimate of tier 1
// and tier 1b; there only "within 1e-12" matters, but it is computed here too so that no division is left in fast code.
#if defined(_MSC_VER)
#define XPE_CLAHE_NOINLINE __declspec(noinline)
#pragma float_control(precise, on, push)
#else
#define XPE_CLAHE_NOINLINE __attribute__((noinline))
#endif
XPE_CLAHE_NOINLINE inline double precise_range(float vmin, float vmax) {
    return static_cast<double>(vmax) - static_cast<double>(vmin);                      // R^ = fl(vmax - vmin)
}
XPE_CLAHE_NOINLINE inline double precise_inv(double range) {
    return static_cast<double>(kBins - 1) / range;                                       // inv = fl(4095 / R^)
}
XPE_CLAHE_NOINLINE inline double precise_inv_low(double range) {
    return static_cast<double>(kBins - 1) / range * (1.0 - 1e-12);
}
#if defined(_MSC_VER)
#pragma float_control(pop)
#endif

// ---- the binner -----------------------------------------------------------------------------------------------------
class Binner {
public:
    // min_lsb: min_lsb_exp() of the pixels; read only when vmin < 0 (pass 0 otherwise). Requires vmin < vmax, both finite.
    Binner(float vmin, float vmax, int min_lsb)
        : vmin_(static_cast<double>(vmin)),
          range_(precise_range(vmin, vmax)),
          inv_low_(precise_inv_low(range_)),
          inv_(precise_inv(range_)),
          vmin_f_(vmin),
          vmax_f_(vmax),
          fast_(fast_path_is_exact(vmin, vmax, min_lsb)),
          int64_ok_(!fast_ && proof_span_bits(vmin, vmax, min_lsb) <= kInt64SpanBits),
          unit_(unit_exp(vmin, min_lsb)),
          x_min64_(int64_ok_ ? scaled_int(vmin, unit_) : 0),
          range64_(int64_ok_ ? scaled_int(vmax, unit_) - x_min64_ : 1),
          inv_low64_(precise_inv_low(static_cast<double>(range64_))),
          x_min_(u320_from_float(vmin)),
          range_x_(u320_sub(u320_from_float(vmax), x_min_)),
          range_x_dbl_(u320_to_double(range_x_)) {}

    bool fast() const { return fast_; }
    bool int64_path() const { return int64_ok_; }      // tier 1b: proven exact with 64-bit integers (41 < S <= 52)
    double vmin() const { return vmin_; }
    double inv() const { return inv_; }
    float vmin_f() const { return vmin_f_; }
    float vmax_f() const { return vmax_f_; }

    // Valid only when fast() is true.
    int fast_bin(float v) const {
        const double d = static_cast<double>(v) - vmin_;
        const double n = d * static_cast<double>(kBins - 1);
        int bin = static_cast<int>(d * inv_low_);
        bin += (static_cast<double>(bin + 1) * range_ <= n);
        return bin;
    }

    // Valid only when int64_path() is true (tier 1b). Same estimate-and-one-exact-comparison as fast_bin, with integers.
    int int64_bin(float v) const {
        const int64_t dd = scaled_int(v, unit_) - x_min64_;                            // >= 0, < 2^52
        const uint64_t nn = static_cast<uint64_t>(dd) * static_cast<uint64_t>(kBins - 1);   // < 2^64: exact
        int bin = static_cast<int>(static_cast<double>(dd) * inv_low64_);
        bin += (static_cast<uint64_t>(bin + 1) * static_cast<uint64_t>(range64_) <= nn);
        return bin;
    }

    // Valid for every finite v in [vmin, vmax].
    int exact_bin(float v) const {
        const U320 n = u320_mul_small(u320_sub(u320_from_float(v), x_min_), static_cast<uint32_t>(kBins - 1));
        int bin = static_cast<int>(u320_to_double(n) / range_x_dbl_);
        if (bin < 0) bin = 0;
        if (bin > kBins - 1) bin = kBins - 1;
        while (bin < kBins - 1 && u320_cmp(u320_mul_small(range_x_, static_cast<uint32_t>(bin + 1)), n) <= 0) ++bin;
        while (bin > 0 && u320_cmp(u320_mul_small(range_x_, static_cast<uint32_t>(bin)), n) > 0) --bin;
        return bin;
    }

    // The exact bin of a pixel the tier-2 filter could not certify: the 64-bit integer path where it is proven (41 < S <= 52), else the
    // 320-bit one. Both are exact; the first is about 6x cheaper.
    int resolve(float v) const { return int64_ok_ ? int64_bin(v) : exact_bin(v); }

    int bin(float v) const;                                          // defined below: tier 1 where proven, else the tier-2 filter
    // The bins of n consecutive pixels; returns how many of them went to the EXACT (320-bit) path.
    size_t fill_bins(const float* row, uint16_t* out, int n) const;  // defined below

private:
    double vmin_;
    double range_;
    double inv_low_;
    double inv_;      // 4095 / range, rounded: the tier-2 filter's reciprocal (inv_low_ is biased low for tier 1)
    float  vmin_f_;
    float  vmax_f_;
    bool   fast_;
    bool   int64_ok_;
    int    unit_;
    int64_t x_min64_;
    int64_t range64_;
    double inv_low64_;
    U320   x_min_;
    U320   range_x_;
    double range_x_dbl_;
};

// ---- tier 2 (per-pixel error filter) and the Binner members that use it ---------------------------------------------------
// Compiled in `precise` mode: the error bound in the header comment counts four correctly rounded operations, which /fp:fast
// (reassociation, distribution) would not preserve.
#if defined(_MSC_VER)
#pragma float_control(precise, on, push)
#endif

// frac(t) strictly inside (margin, 1 - margin): then floor(t) is the exact bin whenever |t - exact quotient| <= kFilterAbsErr.
inline bool filter_certifies(double t, int* k) {
    const double fl = std::floor(t);
    const double frac = t - fl;
    *k = static_cast<int>(fl);
    return frac > kFilterMargin && frac < 1.0 - kFilterMargin;
}

// The bin of one pixel, valid for ANY image (no assumption about the exponents). *exact_used is set when the 320-bit path ran.
XPE_CLAHE_NOINLINE inline int filtered_bin(const Binner& b, float v, bool* exact_used) {
    const double t = (static_cast<double>(v) - b.vmin()) * b.inv();
    int k;
    if (filter_certifies(t, &k)) return k;
    if (v == b.vmin_f()) return 0;                    // D = 0
    if (v == b.vmax_f()) return kBins - 1;            // D = R
    if (exact_used != nullptr) *exact_used = true;
    return b.resolve(v);
}

XPE_CLAHE_NOINLINE inline size_t fill_bins_filtered(const Binner& b, const float* row, uint16_t* out, int n) {
    const double vmin = b.vmin(), inv = b.inv();
    const float vmin_f = b.vmin_f(), vmax_f = b.vmax_f();
    size_t exact = 0;
    for (int i = 0; i < n; ++i) {
        const double t = (static_cast<double>(row[i]) - vmin) * inv;
        int k;
        if (filter_certifies(t, &k)) {
            out[i] = static_cast<uint16_t>(k);
        } else if (row[i] == vmin_f) {
            out[i] = 0;
        } else if (row[i] == vmax_f) {
            out[i] = static_cast<uint16_t>(kBins - 1);
        } else {
            out[i] = static_cast<uint16_t>(b.resolve(row[i]));
            ++exact;
        }
    }
    return exact;
}

#if defined(_MSC_VER)
#pragma float_control(pop)
#endif

inline int Binner::bin(float v) const { return fast_ ? fast_bin(v) : filtered_bin(*this, v, nullptr); }

// The tier-1 loop. The constants are copied to locals first: read through `this` in every iteration they were reloaded after each
// store to `out` (measured +3 ms on 3072x3072), which the compiler cannot rule out as aliasing.
inline size_t Binner::fill_bins(const float* row, uint16_t* out, int n) const {
    if (!fast_) return fill_bins_filtered(*this, row, out, n);
    const double vmin = vmin_, range = range_, inv_low = inv_low_;
    for (int i = 0; i < n; ++i) {
        const double d = static_cast<double>(row[i]) - vmin;
        const double nn = d * static_cast<double>(kBins - 1);
        int bin = static_cast<int>(d * inv_low);
        bin += (static_cast<double>(bin + 1) * range <= nn);
        out[i] = static_cast<uint16_t>(bin);
    }
    return 0;
}

}  // namespace xpe_clahe
