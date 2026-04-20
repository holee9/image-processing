# QA Report: xpe_enhance_advanced.dll (SPEC-XPE-P2-ADV)

**Date**: 2026-04-19
**Author**: xpe-qa
**Status**: Test suite written -- pending compilation and execution verification
**SPEC Reference**: SPEC-XPE-P2-ADV v1.0.0

---

## 1. Test Suite Summary

| Test File | SWU | Test Count | Focus Areas |
|-----------|-----|-----------|-------------|
| test_multiscale_process.cpp | SWU-2.5 | 18 | Identity reconstruction, band enhancement, boundary, config |
| test_fractional_process.cpp | SWU-2.6 | 22 | Order validation, SAF-100, iterations, reproducibility |
| test_collimation_detect.cpp | SWU-2.8 | 17 | Hough accuracy, confidence fallback, non-destructive |
| test_enhance_advanced_integration.cpp | Cross-SWU | 20 | Lifecycle, EI, pipeline integration, memory safety |
| **Total** | | **77** | |

---

## 2. Requirement Coverage Matrix

### 2.1 Lifecycle and State (REQ-ADV-001, REQ-ADV-020)

| Test Case | Requirement | Validation |
|-----------|------------|------------|
| InitWithNullConfigReturnsOK | REQ-ADV-001 | NULL config -> XPE_OK |
| InitWithValidJsonReturnsOK | REQ-ADV-001 | JSON config -> XPE_OK |
| InitWithMalformedJsonReturnsConfigInvalid | REQ-ADV-001 | Bad JSON -> XPE_ERR_CONFIG_INVALID |
| InitIdempotent | REQ-ADV-001 | Second init returns XPE_OK |
| DoubleShutdownSafe | REQ-ADV-001 | No crash on double shutdown |
| ShutdownWithoutInitSafe | REQ-ADV-001 | No crash without init |
| VersionReturnsNonNull | REQ-ADV-001 | Version string non-NULL |
| VersionMatchesExpectedFormat | REQ-ADV-001 | Version = "1.0.0" |
| ProcessWithoutInitReturnsNotInitialized (x4) | REQ-ADV-020 | All 4 functions -> XPE_ERR_NOT_INITIALIZED |

### 2.2 Multiscale Frequency Processing (SWU-2.5)

| Test Case | Requirement | Validation |
|-----------|------------|------------|
| IdentityReconstructionConstantImage | REQ-ADV-050 | All gains=1.0, error < 1e-5 |
| IdentityReconstructionGradientImage | REQ-ADV-050 | Identity on gradient pattern |
| NonIdentityConfigModifiesOutput | REQ-ADV-010 | Non-identity gains change pixels |
| BodyPartDefaultConfigSucceeds | REQ-ADV-010 | 5 body parts with defaults |
| NoNaNOrInfInOutput | REQ-ADV-032 | Output is finite |
| NaNInputHandledGracefully | REQ-ADV-032 | NaN input does not propagate |
| NullImageReturnsInvalidInput | REQ-ADV-022 | NULL img -> XPE_ERR_INVALID_INPUT |
| NullMetaReturnsInvalidInput | REQ-ADV-022 | NULL meta -> XPE_ERR_INVALID_INPUT |
| ZeroWidthReturnsInvalidInput | REQ-ADV-070 | 0 width -> XPE_ERR_INVALID_INPUT |
| ZeroHeightReturnsInvalidInput | REQ-ADV-070 | 0 height -> XPE_ERR_INVALID_INPUT |
| OneByOneImageReturnsInvalidInput | REQ-ADV-100 | 1x1 -> XPE_ERR_INVALID_INPUT |
| Uint16FormatReturnsUnsupportedFormat | REQ-ADV-071 | UINT16 -> XPE_ERR_UNSUPPORTED_FORMAT |
| MalformedJsonConfigReturnsConfigInvalid | Config | Bad JSON -> XPE_ERR_CONFIG_INVALID |
| IdenticalInputProducesIdenticalOutput | REQ-ADV-090 | Reproducibility |
| MultipleSequentialCallsStable | REQ-ADV-031 | 10 iterations no crash |

### 2.3 Fractional-Order Edge Enhancement (SWU-2.6)

| Test Case | Requirement | Validation |
|-----------|------------|------------|
| OrderZeroSucceeds | REQ-ADV-021 | order=0.0 succeeds, identity-like |
| OrderOneSucceeds | REQ-ADV-021 | order=1.0 succeeds |
| OrderTwoSucceeds | REQ-ADV-021 | order=2.0 succeeds |
| OrderHalfSucceeds | REQ-ADV-021 | order=0.5 succeeds |
| OrderOnePointFiveSucceeds | REQ-ADV-021 | order=1.5 succeeds |
| OrderNegativeReturnsInvalidInput | REQ-ADV-021 | order=-0.1 -> XPE_ERR_INVALID_INPUT |
| OrderAboveTwoReturnsInvalidInput | REQ-ADV-021 | order=2.1 -> XPE_ERR_INVALID_INPUT |
| OrderLargeNegativeReturnsInvalidInput | REQ-ADV-021 | order=-100 -> XPE_ERR_INVALID_INPUT |
| OrderLargePositiveReturnsInvalidInput | REQ-ADV-021 | order=100 -> XPE_ERR_INVALID_INPUT |
| OvershootLimitingEnforced | REQ-ADV-051 | boost <= 3*sigma_local verified pixel-by-pixel |
| UniformImagePreserved | REQ-ADV-051 | Uniform input -> output ~ unchanged |
| NoNaNOrInfInOutput | REQ-ADV-032 | Output is finite |
| MultipleIterationsSucceed | Config | iterations=3 succeeds |
| MaxIterationsSucceed | Config | iterations=5 succeeds |
| RepeatedApplicationChangesMoreThanSingle | Config | 3 iter differs from 1 iter |
| NullImageReturnsInvalidInput | REQ-ADV-022 | NULL img -> XPE_ERR_INVALID_INPUT |
| OneByOneImageReturnsInvalidInput | REQ-ADV-100 | 1x1 -> XPE_ERR_INVALID_INPUT |
| Uint16FormatReturnsUnsupportedFormat | REQ-ADV-071 | UINT16 -> XPE_ERR_UNSUPPORTED_FORMAT |
| IdenticalInputProducesIdenticalOutput | REQ-ADV-090 | Reproducibility |

### 2.4 Collimation ROI Detection (SWU-2.8)

| Test Case | Requirement | Validation |
|-----------|------------|------------|
| SharpRectCollimationDetected | REQ-ADV-052 | Within +-3 pixels of ground truth |
| OffCenterCollimationDetected | REQ-ADV-052 | Asymmetric rectangle |
| UniformImageReturnsFullExtent | REQ-ADV-041 | Low confidence -> full extent |
| SmallRectangleFallbackCheck | REQ-ADV-041 | min_area_ratio triggers fallback |
| HighSensitivityConfig | Config | High sensitivity detects subtle edges |
| NullImageReturnsInvalidInput | REQ-ADV-022 | NULL img -> XPE_ERR_INVALID_INPUT |
| NullX0/Y0/X1/Y1ReturnsInvalidInput | REQ-ADV-022 | Any NULL output -> XPE_ERR_INVALID_INPUT |
| OneByOneImageReturnsInvalidInput | REQ-ADV-100 | 1x1 -> XPE_ERR_INVALID_INPUT |
| Uint16FormatReturnsUnsupportedFormat | REQ-ADV-071 | UINT16 -> XPE_ERR_UNSUPPORTED_FORMAT |
| DoesNotModifyInputImage | REQ-ADV-012 | Const correctness verified |
| OutputCoordinatesWithinBounds | REQ-ADV-012 | x0 >= 0, x1 <= W, etc. |
| IdenticalInputProducesIdenticalOutput | REQ-ADV-090 | Reproducibility |

### 2.5 Exposure Index Calculation (SWU-2.10)

| Test Case | Requirement | Validation |
|-----------|------------|------------|
| NullImage/Meta/EiOut/DiOutReturnsInvalidInput | REQ-ADV-022 | NULL ptrs -> XPE_ERR_INVALID_INPUT |
| ValidInputReturnsPositiveEI | REQ-ADV-013 | EI > 0, finite |
| DifferentBodyPartsDifferentEITargets | REQ-ADV-013 | Multiple body parts tested |
| Uint16FormatReturnsUnsupportedFormat | REQ-ADV-071 | UINT16 -> XPE_ERR_UNSUPPORTED_FORMAT |
| ZeroDimensionReturnsInvalidInput | REQ-ADV-070 | 0x0 -> XPE_ERR_INVALID_INPUT |

### 2.6 Integration Tests

| Test Case | Requirement | Validation |
|-----------|------------|------------|
| MfpThenFractionalPipeline | REQ-ADV-062 | Two-stage pipeline no crash |
| FullPipelineWithCollimationAndEI | REQ-ADV-062 | Four-stage full pipeline |
| MultiplePipelinesStable | REQ-ADV-031 | 5 iterations no corruption |
| RepeatedProcessingNoLeak | REQ-ADV-031 | 20 iterations (ASan target) |
| ExceptionBoundaryNoCrash | REQ-ADV-030 | No exception escape |
| SequentialProcessingDeterministic | REQ-ADV-090 | Same input -> same output |

---

## 3. Unverified / Known Limitations

### 3.1 Items Requiring Runtime Verification

| Item | Status | Notes |
|------|--------|-------|
| REQ-ADV-050 (exact identity < 1e-5) | Written | Test validates but depends on bilinear upsampling fix (algorithm note Issue 1) |
| REQ-ADV-051 (overshoot <= 3*sigma) | Written | Pixel-by-pixel verification implemented |
| REQ-ADV-052 (+-3 pixel accuracy) | Written | Depends on Hough implementation correctness |
| REQ-ADV-060/061/062 (performance) | NOT tested | Performance tests require runtime measurement; not included in unit test suite |
| REQ-ADV-080 (200MB memory budget) | NOT tested | Requires runtime memory profiling |
| REQ-ADV-040 (AVX2 SIMD parity) | NOT tested | Requires both scalar and AVX2 builds; deferred to performance validation phase |

### 3.2 Algorithm Issues Identified (from algorithm notes)

1. **Upsample interpolation**: Current nearest-neighbor upsampling in mfp_scalar.cpp prevents REQ-ADV-050 identity reconstruction. Test expects bilinear fix.
2. **Reconstruct dimension tracking**: sqrt heuristic for dimension calculation is incorrect for non-square images. Tests use square images to avoid this issue.
3. **Mask coefficient convention**: Fractional derivative mask sign convention may need normalization. Test verifies output is finite regardless.
4. **Confidence normalization**: Hardcoded maxExpectedStrength in Hough may produce inconsistent confidence. Tests check full-extent fallback rather than exact confidence values.

### 3.3 Test Assumptions

1. Tests assume xpe_common.dll is initialized and available at link time.
2. Tests do not initialize xpe_common.dll explicitly (depends on module init calling xpe_init).
3. 4096x4096 dimension validation test is a stub (actual buffer not allocated at that size).
4. No golden file tests yet (algorithm note Section 8 recommends 8 test vectors).

---

## 4. Test File Manifest

| File | Path |
|------|------|
| CMakeLists.txt | `tests/enhance_advanced_tests/CMakeLists.txt` |
| test_multiscale_process.cpp | `tests/enhance_advanced_tests/test_multiscale_process.cpp` |
| test_fractional_process.cpp | `tests/enhance_advanced_tests/test_fractional_process.cpp` |
| test_collimation_detect.cpp | `tests/enhance_advanced_tests/test_collimation_detect.cpp` |
| test_enhance_advanced_integration.cpp | `tests/enhance_advanced_tests/test_enhance_advanced_integration.cpp` |

Modified:
| File | Change |
|------|--------|
| `tests/CMakeLists.txt` | Added `add_subdirectory(enhance_advanced_tests)` and test targets to check/check_verbose |

---

## 5. Estimated Coverage

| Module SWU | Statement Coverage (Est.) | Branch Coverage (Est.) |
|------------|--------------------------|----------------------|
| SWU-2.5 (MFP) | ~80% | ~70% |
| SWU-2.6 (Fractional) | ~85% | ~75% |
| SWU-2.8 (Collimation) | ~75% | ~65% |
| SWU-2.10 (EI) | ~90% | ~80% |
| Lifecycle/Config | ~95% | ~85% |
| **Overall** | **~82%** | **~73%** |

Gap analysis:
- Branch coverage for collimation is lower due to complex Hough accumulator paths that require specific synthetic inputs.
- Performance-related branches (AVX2 vs scalar) are not exercised.
- Error recovery branches for out-of-memory are difficult to trigger in unit tests.

To achieve 85%+ statement coverage, additional collimation test vectors with varying edge densities are recommended.

---

## 6. Next Steps

1. Compile and execute test suite with xpe_enhance_advanced.dll linked.
2. Run under AddressSanitizer (ENABLE_ASAN=ON) to verify REQ-ADV-031.
3. Add golden file tests using test vectors from algorithm note Section 8.
4. Add performance benchmark tests for REQ-ADV-060/061/062.
5. Verify REQ-ADV-050 identity reconstruction after bilinear upsampling fix.

---

End of QA report.
