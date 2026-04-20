# Implementer Notes: xpe_enhance_advanced.dll (SPEC-XPE-P2-ADV)

**Date**: 2026-04-18
**Implementer**: xpe-implementer
**Status**: Implementation complete -- ready for xpe-qa review

---

## 1. Implementation Summary

### Files Created (new)

| File | SWU | Purpose |
|------|-----|---------|
| `src/multiscale_process.cpp` | SWU-2.5 | Public API for MFP: input validation, config parsing, dispatch to LaplacianPyramid |
| `src/fractional_process.cpp` | SWU-2.6 | Public API for fractional edge enhancement: order validation, iterative application |
| `src/collimation_detect.cpp` | SWU-2.8 | Public API for collimation detection: sensitivity-based Hough, confidence fallback |
| `src/enhance_advanced_helpers.cpp` | -- | internal.h config parser implementations (parse_mfp_config, parse_fractional_config, parse_collimation_config) |

### Files Modified (existing)

| File | Change |
|------|--------|
| `src/xpe_enhance_advanced.cpp` | Rewritten: lifecycle + xpe_calc_exposure_index. g_initialized/g_initMutex moved from anonymous namespace to global scope for external linkage with internal.h |
| `src/enhance_advanced.cpp` | Removed (replaced by xpe_enhance_advanced.cpp) |
| `include/xpe/enhance_advanced/internal.h` | Added `isModuleInitialized()` declaration |

### Files Unchanged (verified compatible)

| File | Status |
|------|--------|
| `src/mfp_scalar.cpp` + `mfp_scalar.h` | Existing LaplacianPyramid implementation. multiscale_process.cpp delegates to `applyMfpScalar()` |
| `src/fractional_derivative.cpp` + `detail/fractional_derivative.h` | Existing GL derivative with SAF-100. fractional_process.cpp delegates to `applyFractionalDerivative()` |
| `src/xpe_collimation_detect.cpp` | **Not compiled** -- replaced by `collimation_detect.cpp` which uses internal.h parsers |
| `src/exposure_index.cpp` + `detail/exposure_index.h` | Existing EI/DI calculation. Called from xpe_enhance_advanced.cpp |
| `src/detail/edge_detection.cpp` + `.h` | Sobel gradient computation (used by collimation) |
| `src/detail/hough_transform.cpp` + `.h` | Hough accumulator and line detection (used by collimation) |

---

## 2. Architecture Decisions

### 2.1 Dispatch Layer Pattern

Each public API function is implemented in its own `.cpp` file (dispatch layer):
1. **Input validation** -- NULL checks, format checks, dimension checks
2. **Init guard** -- check `g_initialized` under mutex
3. **Config parsing** -- via internal.h `parse_*_config()` functions
4. **Algorithm dispatch** -- delegate to existing implementation classes
5. **Exception boundary** -- catch all exceptions, return `XPE_ERR_INTERNAL`

This separates the C ABI contract from the C++ algorithm implementation.

### 2.2 Config Parser Consolidation

`enhance_advanced_helpers.cpp` implements the three config parsers declared in `internal.h`:
- `parse_mfp_config()` -- flat JSON schema per design doc Section 8
- `parse_fractional_config()` -- iterations + step_size
- `parse_collimation_config()` -- sensitivity + min_area_ratio + border_margin

All parsers follow the same pattern:
1. Apply internal.h defaults
2. Early return on NULL input
3. Parse JSON with `nlohmann::json::parse(json, nullptr, false)` (non-throwing)
4. Clamp values to internal.h ranges
5. Return true/false for success/failure

### 2.3 State Visibility

`g_initialized` and `g_initMutex` are defined in `xpe_enhance_advanced.cpp` at global scope (not anonymous namespace) so they can be referenced via `extern` declarations in the dispatch files and in `internal.h`.

### 2.4 Collimation Detection Enhancement

The new `collimation_detect.cpp` improves on the old `xpe_collimation_detect.cpp`:
- Uses internal.h config parser instead of local CollimationConfig struct
- Derives Hough theta resolution from sensitivity parameter
- Applies border_margin to output coordinates
- Validates minimum area ratio (REQ-ADV-041 extension)
- Confidence threshold derived from sensitivity: `0.7 + 0.3 * sensitivity`

---

## 3. Verification Points for xpe-qa

### 3.1 Critical Tests

| Test Case | Requirement | Validation |
|-----------|------------|------------|
| Init with NULL config | REQ-ADV-001 | Returns XPE_OK |
| Init with empty string | REQ-ADV-001 | Returns XPE_ERR_CONFIG_INVALID |
| Init idempotent | REQ-ADV-001 | Second init returns XPE_OK |
| Process without init | REQ-ADV-020 | Returns XPE_ERR_NOT_INITIALIZED |
| MFP with default config | REQ-ADV-010 | Identity-like output (flatGain=0.8) |
| Fractional order 0.0 | REQ-ADV-021 | No-op, returns XPE_OK |
| Fractional order 2.1 | REQ-ADV-021 | Returns XPE_ERR_INVALID_INPUT |
| Collimation fallback | REQ-ADV-041 | Low confidence -> bordered full-image extent |
| EI calculation | REQ-ADV-013 | Finite positive EI, finite DI |
| SAF-100 overshoot | REQ-ADV-051 | Output boost <= 3*sigma_local |

### 3.2 CMake Integration Notes

- `xpe_collimation_detect.cpp` and `enhance_advanced.cpp` should be **removed** from CMakeLists.txt SRCS list (replaced by `collimation_detect.cpp` and existing `xpe_enhance_advanced.cpp`)
- New files to add: `multiscale_process.cpp`, `fractional_process.cpp`, `collimation_detect.cpp`, `enhance_advanced_helpers.cpp`

### 3.3 Potential Issues

1. **Multiple definition risk**: `xpe_collimation_detect.cpp` and `collimation_detect.cpp` both define `xpe_detect_collimation`. Only one should be compiled. Recommend removing `xpe_collimation_detect.cpp` from the build.

2. **Multiple definition risk**: Old `enhance_advanced.cpp` (now deleted) and `xpe_enhance_advanced.cpp` both defined lifecycle functions. Only `xpe_enhance_advanced.cpp` should remain.

3. **MfpConfig field mismatch**: `mfp_scalar.h::MfpConfig` uses `noiseThreshold = 0.02f` while `internal.h` defines `XPE_MFP_DEFAULT_NOISE_THRESH = 5.0f`. The dispatch layer passes the internal.h default, which overrides the MfpConfig struct default. This is intentional -- the internal.h value (5.0) is the documented default per design doc Section 8.

4. **flatGain identity**: Design doc Section 8 specifies default flatGain = 0.8, but identity reconstruction (REQ-ADV-050) requires all gains = 1.0. The current default of 0.8 means MFP with default config is NOT identity. This matches the design spec but may need clarification.

5. **Thread safety**: Processing functions use `std::lock_guard<std::mutex>` for init check only. The actual algorithm runs without the mutex held, ensuring reentrancy on independent buffers.

---

## 4. Dependency Status

| Dependency | Status | Notes |
|-----------|--------|-------|
| xpe_common.dll | Required | xpe_init() called during module init |
| nlohmann/json | Required | Config JSON parsing |
| spdlog | Required | Diagnostic logging |
| Eigen 3.4.x | Required | Hough accumulator, Sobel gradients |
| FFTW3 | NOT used | Confirmed: no FFTW3 includes in any file |

---

End of implementer notes.
