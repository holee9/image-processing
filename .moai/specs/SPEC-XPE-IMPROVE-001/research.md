# Research Report: Phase 1A Detector Correction Algorithms

**Document ID**: RESEARCH-XPE-IMPROVE-001
**Version**: 1.0.0
**Date**: 2026-04-19
**Status**: Complete
**Research Method**: Team-based parallel analysis (researcher + analyst + architect)

---

## 1. Codebase Architecture Analysis

### 1.1 Module Structure

```
modules/preprocess/
├── include/xpe/preprocess/
│   ├── xpe_preprocess_api.h          # Public C API (18 functions)
│   ├── xpe_preprocess_internal.h     # Private C++ helpers
│   ├── xcal_format.h                  # XCal v1 format spec
│   └── simd_dispatch.h                # [NEW] SIMD dispatch interface
├── src/
│   ├── xpe_offset.cpp                 # Offset correction (scalar)
│   ├── xpe_gain.cpp                   # Gain correction (scalar)
│   ├── xpe_defect.cpp                 # Defect correction (scalar)
│   ├── offset_correct.cpp             # [REFERENCE] Scalar baseline
│   ├── gain_correct.cpp               # [REFERENCE] Scalar baseline
│   ├── defect_correct.cpp             # [REFERENCE] Scalar baseline
│   ├── xpe_calib_load_offset.cpp     # SUP-01: Offset loading
│   ├── xpe_calib_load_gain.cpp       # SUP-01: Gain loading
│   ├── xpe_calib_load_defect_map.cpp  # SUP-01: Defect map loading
│   ├── helpers.cpp                    # Interpolation helper
│   └── [NEW] simd_*.cpp               # SIMD implementations
└── tests/
    ├── test_xpe_offset.cpp
    ├── test_xpe_gain.cpp
    ├── test_xpe_defect.cpp
    └── [NEW] test_simd_parity.cpp     # Parity harness
```

### 1.2 Current Implementation Status

| Component | Status | SIMD | Benchmark | Gap |
|-----------|--------|------|-----------|-----|
| Offset Correction | ✅ Basic scalar | ❌ None | ❌ None | SIMD + parity |
| Gain Correction | ✅ Basic scalar | ❌ None | ❌ None | SIMD + parity |
| Defect Correction | ⚠️ Simple average | ❌ None | ❌ None | Edge-aware + SIMD |
| Runtime Detection | ⚠️ Mean±3σ (wrong) | ❌ None | ❌ None | Hampel + SIMD |

**Key Finding**: Current implementation uses simple pixel-wise operations without SIMD optimization. Runtime defect detection uses mean±3σ instead of required Hampel 5-sigma algorithm.

---

## 2. Calibration Infrastructure (SUP-01)

### 2.1 XCal v1 Format

**Structure**:
```cpp
struct CalibFileHeader {
    uint8_t  magic[4];       // "XPEC"
    uint32_t version;        // 1
    uint32_t width, height;
    uint32_t pixelFormat;    // XpePixelFormat
    uint64_t expiryEpochMs;
    uint32_t payloadCrc32;
    uint32_t reserved[7];
};
```

**Status**: ✅ Complete (89/90 tests passing, 1 SKIP)

### 2.2 Thread-Safe Storage

```cpp
struct CalibrationData {
    std::unique_ptr<float[]> offset_map;
    std::unique_ptr<float[]> gain_map;
    std::unique_ptr<uint8_t[]> defect_map;
    uint32_t width, height;
    // ... session metadata
};

extern CalibrationData g_calib;
extern std::mutex g_calib_mutex;
```

**Status**: ✅ Complete and production-ready

---

## 3. API Contracts Analysis

### 3.1 Function Signatures

All functions follow consistent pattern:
```cpp
XPE_API XpeErrorCode xpe_<operation>_correct(
    XpeImageBuffer* img,           // [in/out] Image to process
    const XpeImageBuffer* map,     // [in] Calibration/correction map
    const char* configJsonOrNull  // [in] Optional config
);
```

### 3.2 P/Invoke ABI Requirements

- Linkage: `extern "C"`
- Calling convention: `__cdecl`
- Struct alignment: `#pragma pack(push, 8)`
- No C++ exceptions across ABI boundary
- No standard library types in public API

**Status**: ✅ All existing functions compliant

---

## 4. SIMD Vectorization Patterns

### 4.1 Current State

**Finding**: No SIMD implementations exist despite vectorizable operations.

**Evidence**:
```cpp
// offset_correct.cpp - Scalar loop
for (size_t i = 0; i < n; ++i)
    dst[i] = (dst[i] > off[i]) ? dst[i] - off[i] : 0;
```

**Opportunity**: This loop is trivially vectorizable with `_mm256_subs_epu16`.

### 4.2 SIMD Strategy

| Operation | AVX2 Intrinsic | Parity | Rationale |
|-----------|----------------|-------|-----------|
| Offset subtract | `_mm256_subs_epu16` | Bit-identical | UINT16 saturating subtract is exact |
| Gain multiply | `_mm256_mul_ps` | 1 ULP | Float32 multiply has rounding |
| Load UINT16 | `_mm256_cvtepu16_ps` | Exact | Zero-extension is lossless |
| Store float | `_mm256_store_ps` | Exact | Memory layout preserved |

---

## 5. Memory Management Patterns

### 5.1 Buffer Ownership

**Pattern**: Smart pointer factory from xpe_common.dll
```cpp
xpe_alloc_image(width, height, format)  // Allocates
xpe_free_image(img)                       // Releases
```

**Finding**: Gain correction transfers ownership of pixel buffer (uint16→float32 reallocation).

**Risk**: SIMD implementation must preserve exact same ownership contract.

### 5.2 Contiguous Layout Requirement

All processing requires: `stride == width * element_size`

**Finding**: Validated via `xpe_is_contiguous()` helper

---

## 6. Test Patterns and Fixtures

### 6.1 Existing Test Structure

```cpp
// test_offset_correct.cpp
TEST(OffsetCorrection, BasicSubtraction) {
    // Setup test image and offset map
    // Execute correction
    // Verify results
}
```

**Status**: ✅ Google Test framework configured and working

### 6.2 Test Gaps

- ❌ No SIMD variant tests
- ❌ No parity harness (scalar vs AVX2)
- ❌ No benchmark coverage
- ❌ No Hampel detector accuracy tests

---

## 7. Error Handling Patterns

### 7.1 Return Code Convention

```cpp
try {
    // Processing logic
    return XPE_OK;
} catch (const std::bad_alloc&) {
    return XPE_ERR_OUT_OF_MEMORY;
} catch (...) {
    return XPE_ERR_PROCESSING_FAILED;
}
```

**Finding**: Consistent pattern across all functions

### 7.2 Input Validation

```cpp
if (!img || !offsetMap) return XPE_ERR_INVALID_INPUT;
if (!xpe_dims_match(img, offsetMap)) return XPE_ERR_INVALID_INPUT;
```

**Finding**: Robust validation in place

---

## 8. Algorithm Gap Analysis

### 8.1 Runtime Detection Algorithm

**Current Implementation** (defect_correct.cpp):
```cpp
// Mean + 3*sigma (INCORRECT)
double mean = ...;
double sigma = ...;
out[i] = (abs(px[i] - mean) > 3.0 * sigma) ? 1u : 0u;
```

**Required Implementation** (Hampel 5-sigma):
```cpp
// Median + MAD (CORRECT)
float m = median(neighbors);
float mad = median(|neighbor - m|);
float z = 0.6745 * (p - m) / mad;
defect = (abs(z) > lambda) ? 1 : 0;
```

**Gap**: Complete algorithm replacement required

### 8.2 Defect Interpolation Quality

**Current**: Simple 4-neighbor average
```cpp
float sum = 0.0f;
int count = 0;
// Add 4-connected neighbors
return sum / count;
```

**Required**: Edge-aware gradient-weighted interpolation
- Weight by inverse gradient magnitude
- Preserve edges while correcting defects

**Gap**: Algorithm enhancement required

---

## 9. Reference Implementations

### 9.1 Offset Correction (Keep as Baseline)

**File**: `modules/preprocess/src/offset_correct.cpp`

**Status**: ✅ Production-ready, use as scalar reference

### 9.2 Gain Correction (Keep as Baseline)

**File**: `modules/preprocess/src/gain_correct.cpp`

**Status**: ✅ Production-ready, use as scalar reference

**Note**: Preserves uint16→float32 domain transition

### 9.3 Defect Correction (Needs Enhancement)

**File**: `modules/preprocess/src/defect_correct.cpp`

**Status**: ⚠️ Basic implementation exists
- Simple interpolation: ✅ Works
- Edge-aware: ❌ Missing
- Runtime detection: ⚠️ Wrong algorithm

**Action**: Keep structure, enhance algorithm

---

## 10. Integration Points

### 10.1 SUP-01 Dependency

**Required**: Calibration data loaded before correction

**Pattern**:
```cpp
std::lock_guard<std::mutex> lock(g_calib_mutex);
if (g_calib.offset_map == nullptr) {
    return XPE_ERR_NOT_INITIALIZED;
}
```

**Status**: ✅ Integration pattern established

### 10.2 xpe_common.dll Dependency

**Required**: Layer 0 foundation (error handling, memory)

**Status**: ✅ Available and stable

---

## 11. Performance Baseline

### 11.1 Current Scalar Performance

| Operation | Image Size | Current (est.) | Target | Gap |
|-----------|------------|----------------|--------|-----|
| Offset | 3072x3072 | ~50ms | < 15ms | 3.3x |
| Gain | 3072x3072 | ~50ms | < 15ms | 3.3x |
| Defect | 3072x3072 | ~80ms | < 30ms | 2.7x |
| Runtime Detect | 3072x3072 | ~30ms | < 12ms | 2.5x |

**Finding**: Significant performance improvement possible with SIMD

### 11.2 Memory Strategy

**Challenge**: 3072x3072 float32 = 9.4MB (doesn't fit in L1/L2)

**Solution**: Tile-based processing
```cpp
constexpr size_t TILE_WIDTH = 64;
constexpr size_t TILE_HEIGHT = 48;  // 3072 pixels, 12KB (fits L1)
```

---

## 12. Recommendations

### 12.1 Implementation Priority

1. **HIGH**: SIMD dispatch infrastructure (enables all SIMD work)
2. **HIGH**: Offset correction SIMD (low risk, high reward)
3. **HIGH**: Gain correction SIMD (medium risk, high reward)
4. **MEDIUM**: Hampel detector (high risk, required for accuracy)
5. **MEDIUM**: Edge-aware defect correction (medium risk)

### 12.2 Risk Mitigation

1. **Parity harness first**: Validate SIMD correctness before optimization
2. **Keep scalar baselines**: Never delete reference implementations
3. **Hybrid approach for complex algorithms**: Scalar detection + SIMD interpolation
4. **Comprehensive testing**: Unit + integration + performance + parity

### 12.3 Success Criteria

1. **Performance**: All AVX2 targets met
2. **Parity**: 100% pass rate on parity harness
3. **Accuracy**: Hampel detector TPR ≥ 99.9%, FPR < 0.001%
4. **Coverage**: ≥ 85% statement coverage
5. **Score**: +10 points (61 → 71)

---

## 13. Conclusion

The codebase has solid infrastructure (SUP-01 complete, calibration working, test framework in place) but lacks SIMD optimization and has algorithm gaps in defect correction. The three-tier architecture (API → Dispatch → Scalar/SIMD) proposed by the architect will enable performance optimization while maintaining correctness and backward compatibility.

**Key Insight**: This is a high-value, low-risk improvement. The scalar reference implementations exist and are production-ready. Adding SIMD dispatch and AVX2 optimization is a well-understood problem with clear reference patterns.

**Estimated Effort**: 61 hours (see plan.md for breakdown)

**Expected Score Impact**: +10 points (61 → 71), unlocking Phase 1B implementation
