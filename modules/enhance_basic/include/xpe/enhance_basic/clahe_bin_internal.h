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
// ---- Why the EXACT path is exact --------------------------------------------------------------------------------------
// Every finite float is an integer multiple of 2^-149 below 2^128, i.e. an integer below 2^277 in that unit. OFF = 2^277 is added
// so that signed values become non-negative integers X(x) = OFF + x * 2^149 (< 2^278): subtraction of two X is then plain
// unsigned subtraction and D = X(v) - X(vmin), R = X(vmax) - X(vmin) are exact. 4095 * D < 2^290 and b * R < 2^290, so five
// 64-bit words (320 bits) hold every intermediate. bin = floor(N / R) with N = 4095 * D is found from a double estimate of the
// quotient (error far below 1) corrected by exact multiword comparisons of b * R against N.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace xpe_clahe {

inline constexpr int kBins = 4096;
// Bits a double holds exactly (53) minus the 12 bits of the factor 4095 / 4096: the largest R' = R / 2^L the FAST path may see.
inline constexpr int kFastSpanBits = 41;

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

// The FAST path is proven exact for this image (cases A, B, C above). min_lsb is read only when vmin < 0.
inline bool fast_path_is_exact(float vmin, float vmax, int min_lsb) {
    if (vmin == 0.0f) return true;                                                       // (A)
    if (vmin > 0.0f) return std::ilogb(vmax) + 1 - lsb_exp(vmin) <= kFastSpanBits;      // (B)
    int h = std::ilogb(std::fabs(vmin));                                                 // (C)
    if (vmax != 0.0f) h = std::max(h, std::ilogb(std::fabs(vmax)));
    return h + 2 - min_lsb <= kFastSpanBits;
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

// ---- the binner -----------------------------------------------------------------------------------------------------
class Binner {
public:
    // min_lsb: min_lsb_exp() of the pixels; read only when vmin < 0 (pass 0 otherwise). Requires vmin < vmax, both finite.
    Binner(float vmin, float vmax, int min_lsb)
        : vmin_(static_cast<double>(vmin)),
          range_(static_cast<double>(vmax) - static_cast<double>(vmin)),
          inv_low_(static_cast<double>(kBins - 1) / (static_cast<double>(vmax) - static_cast<double>(vmin)) * (1.0 - 1e-12)),
          fast_(fast_path_is_exact(vmin, vmax, min_lsb)),
          x_min_(u320_from_float(vmin)),
          range_x_(u320_sub(u320_from_float(vmax), x_min_)),
          range_x_dbl_(u320_to_double(range_x_)) {}

    bool fast() const { return fast_; }

    // Valid only when fast() is true.
    int fast_bin(float v) const {
        const double d = static_cast<double>(v) - vmin_;
        const double n = d * static_cast<double>(kBins - 1);
        int bin = static_cast<int>(d * inv_low_);
        bin += (static_cast<double>(bin + 1) * range_ <= n);
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

    int bin(float v) const { return fast_ ? fast_bin(v) : exact_bin(v); }

    // The bins of n consecutive pixels. The constants are copied to locals first: read through `this` in every iteration they
    // were reloaded after each store to `out` (measured +3 ms on 3072x3072), which the compiler cannot rule out as aliasing.
    void fill_bins(const float* row, uint16_t* out, int n) const {
        if (fast_) {
            const double vmin = vmin_, range = range_, inv_low = inv_low_;
            for (int i = 0; i < n; ++i) {
                const double d = static_cast<double>(row[i]) - vmin;
                const double nn = d * static_cast<double>(kBins - 1);
                int bin = static_cast<int>(d * inv_low);
                bin += (static_cast<double>(bin + 1) * range <= nn);
                out[i] = static_cast<uint16_t>(bin);
            }
        } else {
            for (int i = 0; i < n; ++i) out[i] = static_cast<uint16_t>(exact_bin(row[i]));
        }
    }

private:
    double vmin_;
    double range_;
    double inv_low_;
    bool   fast_;
    U320   x_min_;
    U320   range_x_;
    double range_x_dbl_;
};

}  // namespace xpe_clahe
