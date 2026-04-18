# SIMD Parity Harness Specification

**Document ID**: SPEC-XPE-P1A-SIMD-PARITY
**Version**: 1.0.0
**Date**: 2026-04-18
**Status**: Normative (companion to SPEC-XPE-P1A v1.2.0)
**Parent SPEC**: SPEC-XPE-P1A (Pre-processing Module)
**Author**: manager-spec (Pre Lane upgrade)
**IEC 62304 Class**: B

---

## HISTORY

| Version | Date       | Author         | Changes |
|---------|------------|----------------|---------|
| 1.0.0   | 2026-04-18 | manager-spec   | Initial specification extracted from spec.md Section 4.6 and research.md v2.0.0 Section 9 |

---

## 1. Purpose

This document formalises the scalar-vs-AVX2 equivalence test harness required by REQ-P1A-040. It specifies:

1. The feature-detection (CPUID) contract used to dispatch between scalar and AVX2 paths
2. The parity rules (bit-identical vs 1 ULP) for each operation in SPEC-XPE-P1A
3. The deterministic random input generation protocol (100 inputs per operation)
4. The Google Test harness structure (`test_simd_parity.cpp`)
5. The dispatch override mechanism (config + environment variable)

This harness is the acceptance test for REQ-P1A-040 and the +5-point Score Plan Step 1 (Pre Lane scalar + SIMD parity).

---

## 2. CPUID / Feature Detection Contract

### 2.1 Runtime Detection

Feature detection runs once at `xpe_preprocess_init()` and caches the result in the module's internal state.

```
bool cpu_supports_avx2(void)
{
    // Step 1: CPUID leaf 1, ECX bit 27 (OSXSAVE enabled)
    int cpuid_leaf1[4];
    __cpuid(cpuid_leaf1, 1);
    if ((cpuid_leaf1[2] & (1 << 27)) == 0) return false;

    // Step 2: CPUID leaf 7, subleaf 0, EBX bit 5 (AVX2 instruction support)
    int cpuid_leaf7[4];
    __cpuidex(cpuid_leaf7, 7, 0);
    if ((cpuid_leaf7[1] & (1 << 5)) == 0) return false;

    // Step 3: XGETBV bits 1 and 2 (OS saves YMM registers on context switch)
    unsigned long long xcr_mask = _xgetbv(0);
    return ((xcr_mask & 0x06) == 0x06);
}
```

### 2.2 Dispatch Override

Dispatch priority (highest wins):

1. Environment variable `XPE_FORCE_SCALAR=1` — forces scalar path unconditionally (useful for CI regression across hardware)
2. Config JSON key `"force_scalar": true` passed to `xpe_preprocess_init(configJsonOrNull)` — forces scalar path for the module lifetime
3. Runtime CPUID detection (default) — AVX2 if supported, else scalar

Rationale: deterministic CI runs on machines without AVX2 (or for parity testing) must be able to force scalar path without recompilation.

### 2.3 Dispatch Telemetry

On init, the module logs the selected path:

```
[xpe_preprocess] SIMD dispatch: AVX2      (CPUID=ok, override=none)
[xpe_preprocess] SIMD dispatch: scalar    (CPUID=not-supported)
[xpe_preprocess] SIMD dispatch: scalar    (override=config.force_scalar)
[xpe_preprocess] SIMD dispatch: scalar    (override=env.XPE_FORCE_SCALAR)
```

---

## 3. Parity Rules

| Operation | REQ | Scalar Baseline | AVX2 Path | Parity Rule |
|-----------|-----|-----------------|-----------|-------------|
| Offset subtract (UINT16) | REQ-P1A-010 | `max(a - b, 0)` via branch | `_mm256_subs_epu16` | **Bit-identical** |
| Gain correct (FLOAT32 reciprocal) | REQ-P1A-011 | `a * (1.0f / b)` | `_mm256_mul_ps(a, recip_b)` | **1 ULP** |
| Gain correct (FLOAT32 polynomial) | REQ-P1A-011 | Horner: `c0 + x*(c1 + x*c2)` scalar | `_mm256_fmadd_ps` chain | **1 ULP** |
| Defect interp (UINT16 bilinear) | REQ-P1A-012 | pure-C bilinear weighted sum | AVX2 gather + weighted average | **Bit-identical** |
| Defect interp (UINT16 median) | REQ-P1A-012 | sort-9 + median | AVX2 sorting network | **Bit-identical** |
| Runtime detect (MAD UINT16) | REQ-P1A-013 | median-of-8 + MAD + threshold | AVX2 sort + compare | **Bit-identical** |

### 3.1 "Bit-identical" Rule

Applies to all integer operations. Defined as: `memcmp(scalar_out, avx2_out, size) == 0`.

### 3.2 "1 ULP" Rule

Applies to FLOAT32 operations where FMA fusion changes rounding. Defined as:

```
for each (s, a) in zip(scalar_out, avx2_out):
    if isnan(s) and isnan(a): continue                    # both NaN is OK
    if isinf(s) and isinf(a) and sign(s)==sign(a): continue
    assert fabsf(s - a) <= 1 * ULP(max(|s|, |a|))
```

where `ULP(x)` is `nextafterf(x, INFINITY) - x` for positive x.

Rationale: FMA combines multiply + add into a single rounded operation (IEEE-754-2019 standard). A separate multiply-then-add path rounds twice. The difference is bounded by 1 ULP.

### 3.3 NaN / Inf Handling

Both paths must handle edge inputs identically:
- `a - b` where `a < b` on UINT16: both paths must produce 0 (no wraparound)
- `a / b` where `b == 0` on FLOAT32: both paths must produce the same Inf or NaN result (or both must return a predetermined clamp value — REQ-P1A-033)

---

## 4. Deterministic Random Input Generation

### 4.1 Seed Policy

Master seed: `CRC32("XPE-SIMD-PARITY-v1")` = `0xA1B2C3D4` (compute once at test-suite init).

Per-operation sub-seeds: `sub_seed = CRC32(master_seed || operation_name)`.
- Offset subtract: `CRC32(0xA1B2C3D4 || "offset_subtract")`
- Gain reciprocal: `CRC32(0xA1B2C3D4 || "gain_reciprocal")`
- Gain polynomial: `CRC32(0xA1B2C3D4 || "gain_polynomial")`
- Defect bilinear: `CRC32(0xA1B2C3D4 || "defect_bilinear")`
- Defect median: `CRC32(0xA1B2C3D4 || "defect_median")`
- Runtime detect: `CRC32(0xA1B2C3D4 || "runtime_detect")`

Rationale: fixed seeds make CI failures reproducible across runs and machines.

### 4.2 Input Shapes

Each operation tests 3 shapes, 100 inputs each (total 300 per operation):

| Shape | Purpose |
|-------|---------|
| 512x512 | Smallest frame (fast CI iteration) |
| 1024x1024 | Medium frame (cache boundary stress) |
| 3072x3072 | Production frame (performance budget verification) |

### 4.3 Input Distributions

| Data type | Distribution | Rationale |
|-----------|--------------|-----------|
| UINT16 raw pixel | Uniform `[0, 65535]` | Covers full dynamic range |
| UINT16 offset map | Uniform `[0, 32767]` | Typical dark levels |
| FLOAT32 gain map | Normal `(mean=1.0, sigma=0.05)` clipped to `[0.5, 2.0]` | Typical gain factor range |
| FLOAT32 image | Normal `(mean=0.5, sigma=0.3)` clipped to `[0, 1]` | Normalized radiograph range |
| Defect map | Bernoulli `p=0.001` | Typical 0.1% defect rate |

### 4.4 Edge Cases (Additional to Random)

Each operation also tests 5 hand-crafted edge cases:

1. All-zero input
2. All-max input (UINT16=65535, FLOAT32=1.0)
3. Single-pixel hotspot (one pixel at max, rest at 0)
4. Checkerboard pattern
5. Linear ramp

Total tests per operation: 300 (random) + 5 (edge) = 305.

---

## 5. Google Test Harness Structure

### 5.1 Test File Layout

Location: `modules/preprocess/tests/test_simd_parity.cpp`

```cpp
// SPDX-License-Identifier: MIT
// REQ-P1A-040: SIMD Parity Harness (see simd-parity-harness.md)

#include <gtest/gtest.h>
#include "xpe/preprocess/xpe_preprocess_api.h"
#include "detail/simd_parity_helpers.hpp"   // CRC32 seed, ULP check

class SimdParityTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure module is initialized
        ASSERT_EQ(xpe_preprocess_init(nullptr), XPE_OK);
    }
    void TearDown() override {
        xpe_preprocess_shutdown();
    }
};

TEST_F(SimdParityTest, OffsetSubtract_BitIdentical_512) {
    RunParityTest(OffsetSubtract, Shape{512, 512},
                  Seed("offset_subtract"), /*N=*/100, Rule::BitIdentical);
}

TEST_F(SimdParityTest, OffsetSubtract_BitIdentical_3072) {
    RunParityTest(OffsetSubtract, Shape{3072, 3072},
                  Seed("offset_subtract"), /*N=*/100, Rule::BitIdentical);
}

TEST_F(SimdParityTest, GainCorrectReciprocal_1ULP_1024) {
    RunParityTest(GainCorrectReciprocal, Shape{1024, 1024},
                  Seed("gain_reciprocal"), /*N=*/100, Rule::OneULP);
}

// ... 18 test cases total (6 operations x 3 shapes)
// Plus edge case suite: 30 tests (6 operations x 5 edge cases)
```

### 5.2 Harness Primitives

Expected utility functions in `detail/simd_parity_helpers.hpp`:

```cpp
namespace xpe::parity {

// Reproducible seed derivation
uint32_t DeriveSeed(const char* operation_name);

// Deterministic RNG (Mersenne-Twister seeded from DeriveSeed)
class DeterministicRNG {
    std::mt19937 gen_;
public:
    explicit DeterministicRNG(uint32_t seed) : gen_(seed) {}
    std::vector<uint16_t> GenerateUint16Uniform(size_t count, uint16_t min=0, uint16_t max=65535);
    std::vector<float> GenerateFloatNormal(size_t count, float mean, float sigma, float clip_lo, float clip_hi);
    std::vector<uint8_t> GenerateDefectBernoulli(size_t count, float p=0.001f);
};

// Parity assertions
void AssertBitIdentical(const void* scalar, const void* avx2, size_t bytes);
void AssertOneULP(const float* scalar, const float* avx2, size_t count);

// Dispatch control for testing
void ForceScalarPath();   // Sets XPE_FORCE_SCALAR env for duration of test
void ClearForceOverride();

}  // namespace xpe::parity
```

### 5.3 Test Matrix

| Test name | Operation | Shape | Rule | Count |
|-----------|-----------|-------|------|-------|
| OffsetSubtract_512 | REQ-P1A-010 | 512x512 | BitIdentical | 100 |
| OffsetSubtract_1024 | REQ-P1A-010 | 1024x1024 | BitIdentical | 100 |
| OffsetSubtract_3072 | REQ-P1A-010 | 3072x3072 | BitIdentical | 100 |
| GainReciprocal_512 | REQ-P1A-011 | 512x512 | OneULP | 100 |
| GainReciprocal_1024 | REQ-P1A-011 | 1024x1024 | OneULP | 100 |
| GainReciprocal_3072 | REQ-P1A-011 | 3072x3072 | OneULP | 100 |
| GainPolynomial_* | REQ-P1A-011 | 3 shapes | OneULP | 300 |
| DefectBilinear_* | REQ-P1A-012 | 3 shapes | BitIdentical | 300 |
| DefectMedian_* | REQ-P1A-012 | 3 shapes | BitIdentical | 300 |
| RuntimeDetect_* | REQ-P1A-013 | 3 shapes | BitIdentical | 300 |
| EdgeCases_* | All 6 ops | 512x512 | Respective | 30 |
| **Total** | | | | **1830** |

### 5.4 CI Integration

The full harness runs on every Pre Lane PR touching `modules/preprocess/`. Fail criteria:

- Any parity assertion fails: BLOCKER (PR cannot merge)
- Any test skipped (e.g., AVX2 not available on runner): WARNING (documented, not blocking)
- Runtime > 5 minutes total: WARNING (optimize tests, not a blocker)

---

## 6. Acceptance Criteria

Harness is ACCEPTED when:

- [ ] `test_simd_parity.cpp` exists under `modules/preprocess/tests/`
- [ ] All 18 baseline tests (6 ops x 3 shapes, 100 inputs each) pass on AVX2-capable hardware
- [ ] All 30 edge-case tests pass
- [ ] Forced-scalar mode runs identically (both paths run scalar, trivially parity-identical)
- [ ] CI pipeline includes the harness
- [ ] SPEC-XPE-P1A acceptance.md AC-SIMD-001/002/003 are replaced with references to this harness (see plan.md update in next iteration)

---

## 7. Relationship to Other Documents

- **SPEC-XPE-P1A spec.md v1.2.0 Section 4.6** — high-level parity contract
- **SPEC-XPE-P1A research.md v2.0.0 Section 9** — architectural rationale and references
- **SPEC-XPE-P1A acceptance.md** — AC-SIMD-001/002/003 scenarios (to be refined in next iteration to reference this harness)
- **docs/project/XPE-SVVP-001 Section 2** — L1 unit verification level (parity falls under L1)
- **docs/post-processing/xpe/XPE-VVP-001** — Pre Lane-specific VV plan

---

## 8. References

- Intel Intrinsics Guide (2023) — AVX2 instruction semantics, FMA rounding
- IEEE-754-2019 — Floating-point arithmetic and ULP definition
- github.com/ermig1979/Simd (2024) — Open-source reference for bit-exact AVX2 integer ops
- Agner Fog Optimization Resources (2024) — CPUID feature detection patterns
- Starman et al., PMC3465354 — Cited for lag correction parity considerations (cross-reference, out of scope)

---

*Document End - SIMD Parity Harness v1.0.0*
