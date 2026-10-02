/**
 * @file ai_finite.h
 * @brief Bit-pattern finiteness test for model results (QA-B-181h).
 *
 * A float is non-finite when its exponent bits are all ones (+/-inf, NaN). The test reads the bits instead
 * of calling std::isfinite or comparing against a range, the same convention the other modules use, so
 * the answer does not depend on the compiler's floating-point model.
 */
#ifndef XPE_AI_FINITE_H
#define XPE_AI_FINITE_H

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace xpe::ai {

/** True when every one of @p count floats at @p p is finite. Bytes are read with memcpy: @p p need not be aligned. */
inline bool AllFinite(const void* p, size_t count) {
    const unsigned char* b = static_cast<const unsigned char*>(p);
    uint32_t bad = 0;   // OR of the "exponent all ones" test; no early exit, so the loop vectorises
    for (size_t i = 0; i < count; ++i) {
        uint32_t u;
        std::memcpy(&u, b + i * sizeof(uint32_t), sizeof(u));
        bad |= static_cast<uint32_t>((u & 0x7F800000u) == 0x7F800000u);
    }
    return bad == 0;
}

}  // namespace xpe::ai

#endif  // XPE_AI_FINITE_H
