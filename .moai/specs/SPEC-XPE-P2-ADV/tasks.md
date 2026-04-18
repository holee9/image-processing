# Task Decomposition

SPEC: SPEC-XPE-P2-ADV
Version: 1.0.0
Status: Planned
Created: 2026-04-17

---

## Phase 0: Prerequisites

| Task ID | Description | Requirement | Dependencies | Planned Files | Status |
|---------|-------------|-------------|--------------|---------------|--------|
| T-001 | Add XPE_ERR_SAFETY_VIOLATION error code to xpe_common error definitions | REQ-ADV-051 (SAF-100) | - | `modules/common/include/xpe/common/xpe_error.h` | pending |
| T-002 | Update root CMakeLists.txt to include enhance_advanced subdirectory | REQ-ADV-001 | - | `CMakeLists.txt` (root) | pending |
| T-003 | Add Eigen 3.4.x via vcpkg manifest or FetchContent to enhance_advanced build | REQ-ADV-001, REQ-ADV-010 | T-002 | `modules/enhance_advanced/CMakeLists.txt`, `vcpkg.json` | pending |

---

## Phase 1: Foundation (M1)

| Task ID | Description | Requirement | Dependencies | Planned Files | Status |
|---------|-------------|-------------|--------------|---------------|--------|
| T-010 | Rewrite enhance_advanced CMakeLists.txt with proper xpe_common link, Eigen dependency, source file enumeration | REQ-ADV-001, REQ-ADV-002 | T-002, T-003 | `modules/enhance_advanced/CMakeLists.txt` | pending |
| T-011 | Create xpe_enhance_advanced_api.h with 4 function declarations, extern "C" linkage, __cdecl convention | REQ-ADV-002, REQ-ADV-010, REQ-ADV-011, REQ-ADV-012, REQ-ADV-013 | T-010 | `modules/enhance_advanced/include/xpe/enhance_advanced/xpe_enhance_advanced_api.h` | pending |
| T-012 | Implement lifecycle functions: init, shutdown, version with g_initialized flag and std::mutex guard | REQ-ADV-001, REQ-ADV-020, REQ-ADV-090 | T-010, T-011 | `modules/enhance_advanced/src/xpe_enhance_advanced.cpp` | pending |
| T-013 | Add not-initialized guard to all 4 processing functions (stub returning XPE_ERR_NOT_INITIALIZED) | REQ-ADV-020 | T-012 | `modules/enhance_advanced/src/xpe_enhance_advanced.cpp` | pending |
| T-014 | Verify build produces xpe_enhance_advanced.dll with correct exports via dumpbin | REQ-ADV-002 | T-013 | Build verification (no new files) | pending |
| T-015 | Create test CMakeLists.txt and scaffolding for enhance_advanced tests with Google Test | REQ-ADV-001 | T-010 | `modules/enhance_advanced/tests/CMakeLists.txt` | pending |

---

## Phase 2A: MFP Scalar Reference (M2)

| Task ID | Description | Requirement | Dependencies | Planned Files | Status |
|---------|-------------|-------------|--------------|---------------|--------|
| T-020 | Create laplacian_pyramid.h declaring Gaussian pyramid, Laplacian pyramid, and reconstruction interfaces | REQ-ADV-010, REQ-ADV-050 | T-012 | `modules/enhance_advanced/src/detail/laplacian_pyramid.h` | pending |
| T-021 | Implement Gaussian pyramid level computation (downsample + Gaussian blur) using Eigen | REQ-ADV-010 | T-020 | `modules/enhance_advanced/src/detail/laplacian_pyramid.cpp` | pending |
| T-022 | Implement Laplacian level computation (subtract-expand from Gaussian pyramid) | REQ-ADV-010 | T-021 | `modules/enhance_advanced/src/detail/laplacian_pyramid.cpp` | pending |
| T-023 | Implement reconstruction from Laplacian pyramid (collapse levels) | REQ-ADV-010, REQ-ADV-050 | T-022 | `modules/enhance_advanced/src/detail/laplacian_pyramid.cpp` | pending |
| T-024 | Write identity reconstruction test: alpha_k=1.0 -> output == input within FLOAT32 precision (max error < 1e-5) | REQ-ADV-050, AC-MFP-001 | T-023 | `modules/enhance_advanced/tests/test_multiscale_process.cpp` | pending |
| T-025 | Implement xpe_multiscale_process with per-band enhancement coefficient logic, NULL guards, format validation | REQ-ADV-010, REQ-ADV-022, REQ-ADV-070, REQ-ADV-071 | T-023 | `modules/enhance_advanced/src/xpe_multiscale_process.cpp` | pending |
| T-026 | Implement body-part adaptive parameter selection (CHEST, EXTREMITY, PELVIS, etc. alpha_k lookup) | REQ-ADV-010 | T-025 | `modules/enhance_advanced/src/xpe_multiscale_process.cpp` | pending |
| T-027 | Add NaN/Inf validation in MFP reconstruction path, clamp invalid float values | REQ-ADV-032 | T-025 | `modules/enhance_advanced/src/xpe_multiscale_process.cpp` | pending |
| T-028 | Write MFP input validation tests: NULL img, zero dimensions, wrong pixel format, NULL meta | REQ-ADV-022, REQ-ADV-070, REQ-ADV-071, REQ-ADV-100, AC-MFP-003, AC-MFP-004 | T-025 | `modules/enhance_advanced/tests/test_multiscale_process.cpp` | pending |
| T-029 | Write MFP NaN/Inf edge case test with extreme pixel values (1e30, -1e30, 0.0) | REQ-ADV-032, AC-MFP-005 | T-027 | `modules/enhance_advanced/tests/test_multiscale_process.cpp` | pending |
| T-030 | Write MFP enhancement effect test: verify mid-frequency amplification with non-identity coefficients | REQ-ADV-010, AC-MFP-002 | T-026 | `modules/enhance_advanced/tests/test_multiscale_process.cpp` | pending |

---

## Phase 2B: Collimation Detection (M4)

| Task ID | Description | Requirement | Dependencies | Planned Files | Status |
|---------|-------------|-------------|--------------|---------------|--------|
| T-040 | Create edge_detection.h declaring Sobel gradient computation interface | REQ-ADV-012 | T-012 | `modules/enhance_advanced/src/detail/edge_detection.h` | pending |
| T-041 | Implement Sobel gradient magnitude computation for edge map generation | REQ-ADV-012 | T-040 | `modules/enhance_advanced/src/detail/edge_detection.cpp` | pending |
| T-042 | Create hough_transform.h declaring accumulator, peak detection, axis-aligned filtering interfaces | REQ-ADV-012 | T-012 | `modules/enhance_advanced/src/detail/hough_transform.h` | pending |
| T-043 | Implement Hough accumulator construction (theta step 1 deg, rho step 1 pixel) | REQ-ADV-012 | T-041, T-042 | `modules/enhance_advanced/src/detail/hough_transform.cpp` | pending |
| T-044 | Implement peak detection and axis-aligned line filtering (theta in [-5,+5] union [85,95] degrees) | REQ-ADV-012 | T-043 | `modules/enhance_advanced/src/detail/hough_transform.cpp` | pending |
| T-045 | Implement xpe_detect_collimation with ROI rectangle extraction, NULL pointer guards, format validation | REQ-ADV-012, REQ-ADV-022, REQ-ADV-071 | T-044 | `modules/enhance_advanced/src/xpe_collimation_detect.cpp` | pending |
| T-046 | Implement confidence scoring: sum(4 peak values) / (4 * max_accumulator_value) | REQ-ADV-041, REQ-ADV-052 | T-044 | `modules/enhance_advanced/src/xpe_collimation_detect.cpp` | pending |
| T-047 | Implement fallback logic: confidence < 0.7 -> return full image extent with warning log | REQ-ADV-041 | T-046 | `modules/enhance_advanced/src/xpe_collimation_detect.cpp` | pending |
| T-048 | Write collimation input validation tests: NULL output pointers, wrong format, zero dimensions | REQ-ADV-022, REQ-ADV-070, REQ-ADV-071, AC-COL-003 | T-045 | `modules/enhance_advanced/tests/test_collimation_detect.cpp` | pending |
| T-049 | Write synthetic collimation detection test with known borders, verify +-3 pixel accuracy | REQ-ADV-052, AC-COL-001 | T-045, T-046 | `modules/enhance_advanced/tests/test_collimation_detect.cpp` | pending |
| T-050 | Write no-collimation fallback test: uniform image -> full extent, confidence < 0.7, warning logged | REQ-ADV-041, AC-COL-002 | T-047 | `modules/enhance_advanced/tests/test_collimation_detect.cpp` | pending |
| T-051 | Write boundary clipping test: detected borders extending beyond image edges clipped to [0, dim) | REQ-ADV-052, AC-COL-004 | T-045 | `modules/enhance_advanced/tests/test_collimation_detect.cpp` | pending |

---

## Phase 3A: Edge Enhancement (M3)

| Task ID | Description | Requirement | Dependencies | Planned Files | Status |
|---------|-------------|-------------|--------------|---------------|--------|
| T-060 | Create fractional_derivative.h declaring Gruenwald-Letnikov fractional-order differentiation interface | REQ-ADV-011 | T-012 | `modules/enhance_advanced/src/detail/fractional_derivative.h` | pending |
| T-061 | Implement Gruenwald-Letnikov fractional derivative for order alpha in [0, 2] | REQ-ADV-011 | T-060 | `modules/enhance_advanced/src/detail/fractional_derivative.cpp` | pending |
| T-062 | Create overshoot_limiter.h declaring +-3*sigma_local mandatory clipping interface | REQ-ADV-051 (SAF-100) | T-012 | `modules/enhance_advanced/src/detail/overshoot_limiter.h` | pending |
| T-063 | Implement overshoot limiter: compute 3x3 neighborhood sigma, clamp boost to +-3*sigma | REQ-ADV-051 (SAF-100) | T-062 | `modules/enhance_advanced/src/detail/overshoot_limiter.cpp` | pending |
| T-064 | Implement xpe_fractional_process integrating fractional derivative + overshoot limiter + order validation [0.0, 2.0] | REQ-ADV-011, REQ-ADV-021, REQ-ADV-022, REQ-ADV-070, REQ-ADV-071 | T-061, T-063 | `modules/enhance_advanced/src/xpe_fractional_process.cpp` | pending |
| T-065 | Write edge enhancement input validation tests: NULL img, order < 0, order > 2, wrong format | REQ-ADV-021, REQ-ADV-022, AC-EDGE-002, AC-EDGE-003 | T-064 | `modules/enhance_advanced/tests/test_fractional_process.cpp` | pending |
| T-066 | Write fractional derivative order 1.0 test with step edge input | REQ-ADV-011, AC-EDGE-001 | T-064 | `modules/enhance_advanced/tests/test_fractional_process.cpp` | pending |
| T-067 | Write overshoot limiting active test: high-contrast edge -> boost clipped to +-3*sigma_local | REQ-ADV-051, AC-EDGE-004 | T-063, T-064 | `modules/enhance_advanced/tests/test_overshoot_safety.cpp` | pending |
| T-068 | Write overshoot disable attempt blocked test: config JSON with "overshoot_limit_enabled": false -> XPE_ERR_SAFETY_VIOLATION | REQ-ADV-051 (SAF-100), AC-EDGE-005 | T-001, T-064 | `modules/enhance_advanced/tests/test_overshoot_safety.cpp` | pending |

---

## Phase 3B: Exposure Index (M5)

| Task ID | Description | Requirement | Dependencies | Planned Files | Status |
|---------|-------------|-------------|--------------|---------------|--------|
| T-070 | Create ei_lookup_table.h/cpp with body-part EI target values per IEC 62494-1 | REQ-ADV-013 | T-012 | `modules/enhance_advanced/src/detail/ei_lookup_table.h`, `modules/enhance_advanced/src/detail/ei_lookup_table.cpp` | pending |
| T-071 | Implement xpe_calc_exposure_index with EI computation per IEC 62494-1 formula | REQ-ADV-013 | T-070 | `modules/enhance_advanced/src/xpe_exposure_index.cpp` | pending |
| T-072 | Implement DI computation: 10 * log10(EI / EI_target) | REQ-ADV-013 | T-071 | `modules/enhance_advanced/src/xpe_exposure_index.cpp` | pending |
| T-073 | Implement QC alert logging for |DI| > 3 | REQ-ADV-013 | T-072 | `modules/enhance_advanced/src/xpe_exposure_index.cpp` | pending |
| T-074 | Implement ROI-aware masking: use collimation ROI from caller context when available | REQ-ADV-013 | T-071 | `modules/enhance_advanced/src/xpe_exposure_index.cpp` | pending |
| T-075 | Write EI input validation tests: NULL img, NULL eiOut, NULL meta, zero dimensions, wrong format | REQ-ADV-022, REQ-ADV-070, REQ-ADV-071, AC-EI-003 | T-071 | `modules/enhance_advanced/tests/test_exposure_index.cpp` | pending |
| T-076 | Write basic EI/DI calculation test: uniform pixel value 1000.0, bodyPart = "CHEST" | REQ-ADV-013, AC-EI-001 | T-072 | `modules/enhance_advanced/tests/test_exposure_index.cpp` | pending |
| T-077 | Write QC alert test: extremely high pixel values -> |DI| > 3, alert logged | REQ-ADV-013, AC-EI-002 | T-073 | `modules/enhance_advanced/tests/test_exposure_index.cpp` | pending |
| T-078 | Write zero image values test: all pixels = 0.0 -> no NaN/Inf in ei or di | REQ-ADV-013, REQ-ADV-032, AC-EI-004 | T-071 | `modules/enhance_advanced/tests/test_exposure_index.cpp` | pending |

---

## Phase 4: Integration and Cross-Cutting (M6)

| Task ID | Description | Requirement | Dependencies | Planned Files | Status |
|---------|-------------|-------------|--------------|---------------|--------|
| T-090 | Implement exception boundary guard: try-catch in all 4 exported functions converting exceptions to XPE_ERR codes | REQ-ADV-030, AC-IEC-001 | T-025, T-064, T-045, T-071 | `modules/enhance_advanced/src/xpe_enhance_advanced.cpp`, `modules/enhance_advanced/src/xpe_multiscale_process.cpp`, `modules/enhance_advanced/src/xpe_fractional_process.cpp`, `modules/enhance_advanced/src/xpe_collimation_detect.cpp`, `modules/enhance_advanced/src/xpe_exposure_index.cpp` | complete |
| T-091 | Implement diagnostic logging: per-stage execution time, parameters, decision points to spdlog | REQ-ADV-091 | T-025, T-064, T-045, T-071 | All src/*.cpp files | complete |
| T-092 | Write full pipeline integration test: MFP + Edge + Collimation + EI in sequence on 3072x3072 image | REQ-ADV-062, AC-PIPE-001 | T-030, T-066, T-049, T-076 | `modules/enhance_advanced/tests/test_integration.cpp` | complete |
| T-093 | Write independent function calling test: each function called standalone without pipeline ordering | REQ-ADV-090, AC-PIPE-002 | T-092 | `modules/enhance_advanced/tests/test_integration.cpp` | complete |
| T-094 | Write 1000-cycle memory leak endurance test with memory baseline verification | REQ-ADV-031, REQ-ADV-081, AC-IEC-002 | T-092 | `modules/enhance_advanced/tests/test_integration.cpp` | complete |
| T-095 | Write concurrent access safety test: 4 threads calling xpe_multiscale_process with independent buffers | REQ-ADV-090, AC-IEC-003 | T-025 | `modules/enhance_advanced/tests/test_integration.cpp` | complete |
| T-096 | Write 1x1 image rejection test across all 4 functions | REQ-ADV-100 | T-025, T-064, T-045, T-071 | `modules/enhance_advanced/tests/test_integration.cpp` | complete |
| T-097 | Write 4096x4096 maximum image size test verifying 1.5x performance budget | REQ-ADV-101, REQ-ADV-080 | T-092 | `modules/enhance_advanced/tests/test_integration.cpp` | complete |
| T-098 | Add diagnostic logging verification tests: check log output format and content | REQ-ADV-091 | T-091 | `modules/enhance_advanced/tests/test_integration.cpp` | complete |
| T-099 | Measure and verify test coverage >= 85% statement, >= 70% branch | Quality Gate | T-092 through T-098 | Coverage report | pending |

---

## Phase 5: SIMD Optimization (M7)

| Task ID | Description | Requirement | Dependencies | Planned Files | Status |
|---------|-------------|-------------|--------------|---------------|--------|
| T-100 | Implement xpe_simd_dispatch.cpp: CPUID-based AVX2 runtime feature detection | REQ-ADV-040 | T-012 | `modules/enhance_advanced/src/simd/xpe_simd_dispatch.cpp` | pending |
| T-101 | Implement AVX2 Gaussian blur and Laplacian pyramid computation in xpe_mfp_avx2.cpp | REQ-ADV-040 | T-023, T-100 | `modules/enhance_advanced/src/simd/xpe_mfp_avx2.cpp` | pending |
| T-102 | Implement AVX2 fractional differentiation and convolution kernels in xpe_edge_avx2.cpp | REQ-ADV-040 | T-061, T-100 | `modules/enhance_advanced/src/simd/xpe_edge_avx2.cpp` | pending |
| T-103 | Implement AVX2 Sobel gradient and Hough accumulator in xpe_hough_avx2.cpp | REQ-ADV-040 | T-041, T-043, T-100 | `modules/enhance_advanced/src/simd/xpe_hough_avx2.cpp` | pending |
| T-104 | Write SIMD parity tests: scalar vs AVX2 equivalence for MFP, edge, and collimation (50 random images, error < 1e-6) | REQ-ADV-040, AC-SIMD-001, AC-SIMD-002, AC-SIMD-003 | T-101, T-102, T-103 | `modules/enhance_advanced/tests/test_simd_parity.cpp` | pending |
| T-105 | Write performance benchmark tests for 3072x3072: verify scalar and AVX2 targets | REQ-ADV-060, REQ-ADV-061, REQ-ADV-062, PERF-ADV-001 through PERF-ADV-006 | T-104 | `modules/enhance_advanced/tests/test_integration_pipeline.cpp` | pending |

---

## Requirement Coverage Matrix

| Requirement | Task(s) | Verification |
|-------------|---------|-------------|
| REQ-ADV-001 (Init) | T-012, T-015 | Lifecycle unit tests |
| REQ-ADV-002 (ABI) | T-011, T-014 | dumpbin /exports, static_assert |
| REQ-ADV-010 (MFP) | T-021, T-022, T-023, T-025, T-026, T-030 | MFP tests |
| REQ-ADV-011 (Fractional) | T-061, T-064, T-066 | Edge enhancement tests |
| REQ-ADV-012 (Collimation) | T-041, T-043, T-044, T-045, T-049 | Collimation tests |
| REQ-ADV-013 (EI/DI) | T-071, T-072, T-073, T-076 | EI tests |
| REQ-ADV-020 (Not-Init Guard) | T-013 | Lifecycle unit tests |
| REQ-ADV-021 (Order Validation) | T-064, T-065 | Edge validation tests |
| REQ-ADV-022 (NULL Guard) | T-025, T-045, T-064, T-071, T-028, T-048, T-065, T-075 | Negative tests |
| REQ-ADV-030 (No Exceptions) | T-090 | Exception boundary test |
| REQ-ADV-031 (No Leak) | T-094 | 1000-cycle endurance test |
| REQ-ADV-032 (No NaN/Inf) | T-027, T-029, T-078 | NaN/Inf tests |
| REQ-ADV-040 (AVX2 SIMD) | T-100, T-101, T-102, T-103, T-104 | SIMD parity tests |
| REQ-ADV-041 (Confidence Fallback) | T-046, T-047, T-050 | Fallback test |
| REQ-ADV-050 (Identity Recon) | T-024 | Identity reconstruction test |
| REQ-ADV-051 (Overshoot SAF-100) | T-063, T-067, T-068 | Safety tests |
| REQ-ADV-052 (Collimation Accuracy) | T-049, T-051 | Ground truth test |
| REQ-ADV-060 (MFP Perf) | T-105 | Performance benchmark |
| REQ-ADV-061 (Edge Perf) | T-105 | Performance benchmark |
| REQ-ADV-062 (Pipeline Perf) | T-092, T-105 | Integration + benchmark |
| REQ-ADV-070 (Dim Validation) | T-028, T-048, T-065, T-075, T-096 | Dimension tests |
| REQ-ADV-071 (Format Validation) | T-028, T-048, T-065, T-075 | Format tests |
| REQ-ADV-080 (Memory Budget) | T-097 | Max image test |
| REQ-ADV-081 (Memory Release) | T-094 | Endurance test |
| REQ-ADV-090 (Thread Safety) | T-093, T-095 | Concurrency tests |
| REQ-ADV-091 (Diagnostic Log) | T-091, T-098 | Log verification tests |
| REQ-ADV-100 (1x1 Rejection) | T-096 | Boundary test |
| REQ-ADV-101 (4096x4096) | T-097 | Max image test |

---

## Acceptance Criteria Coverage

| Criteria | Task(s) |
|----------|---------|
| AC-LC-001 (Init Default) | T-012 |
| AC-LC-002 (Process Before Init) | T-013 |
| AC-LC-003 (Shutdown) | T-012 |
| AC-MFP-001 (Identity Recon) | T-024 |
| AC-MFP-002 (Enhancement Effect) | T-030 |
| AC-MFP-003 (NULL Rejection) | T-028 |
| AC-MFP-004 (Format Rejection) | T-028 |
| AC-MFP-005 (NaN/Inf Free) | T-029 |
| AC-MFP-006 (Large Image Perf) | T-105 |
| AC-EDGE-001 (Order 1.0) | T-066 |
| AC-EDGE-002 (Order > 2 Rejection) | T-065 |
| AC-EDGE-003 (Order < 0 Rejection) | T-065 |
| AC-EDGE-004 (Overshoot Active) | T-067 |
| AC-EDGE-005 (Overshoot Disable Blocked) | T-068 |
| AC-COL-001 (Synthetic Detection) | T-049 |
| AC-COL-002 (No Collimation Fallback) | T-050 |
| AC-COL-003 (NULL Output Rejection) | T-048 |
| AC-COL-004 (Boundary Clipping) | T-051 |
| AC-EI-001 (Basic EI Calc) | T-076 |
| AC-EI-002 (QC Alert) | T-077 |
| AC-EI-003 (NULL EI Rejection) | T-075 |
| AC-EI-004 (Zero Image Values) | T-078 |
| AC-PIPE-001 (Full Pipeline) | T-092 |
| AC-PIPE-002 (Independent Call) | T-093 |
| AC-SIMD-001 (MFP Parity) | T-104 |
| AC-SIMD-002 (Edge Parity) | T-104 |
| AC-SIMD-003 (Collimation Determinism) | T-104 |
| AC-IEC-001 (No Exception Propagation) | T-090 |
| AC-IEC-002 (Memory Endurance) | T-094 |
| AC-IEC-003 (Concurrent Safety) | T-095 |
| AC-PIN-001 (C# Struct Marshal) | T-011 (static_assert) |
| AC-PIN-002 (Full Pipeline C#) | T-092 (C++ basis) |

---

## Execution Dependency Graph

```
Phase 0: T-001, T-002, T-003 (independent, parallel)
    |
Phase 1: T-010 -> T-011 -> T-012 -> T-013 -> T-014
                                              T-015 (after T-010)
    |
    +---> Phase 2A: T-020 -> T-021 -> T-022 -> T-023 -> T-024
    |                                    T-025 -> T-026 -> T-030
    |                                    T-025 -> T-027 -> T-029
    |                                    T-025 -> T-028
    |
    +---> Phase 2B: T-040 -> T-041
    |              T-042 -> T-043 -> T-044 -> T-045 -> T-046 -> T-047 -> T-050
    |                                              T-045 -> T-048
    |                                              T-045 -> T-049
    |                                              T-045 -> T-051
    |
    +---> Phase 3A: T-060 -> T-061
    |              T-062 -> T-063
    |              T-061 + T-063 -> T-064 -> T-065, T-066
    |                                          T-063 -> T-067
    |                                          T-064 -> T-068
    |
    +---> Phase 3B: T-070 -> T-071 -> T-072 -> T-073 -> T-077
    |                            T-071 -> T-074
    |                            T-071 -> T-075, T-076, T-078
    |
Phase 4: T-090, T-091 (after all Phase 2/3)
         T-092 -> T-093 -> T-094 -> T-099
         T-092 -> T-095, T-096, T-097, T-098
    |
Phase 5: T-100 -> T-101, T-102, T-103 -> T-104 -> T-105
```

---

## Summary Statistics

| Phase | Task Count | Description |
|-------|-----------|-------------|
| Phase 0: Prerequisites | 3 | Error code, build config, Eigen dependency |
| Phase 1: Foundation | 6 | CMake, API header, lifecycle, build verify, test scaffold |
| Phase 2A: MFP Scalar | 11 | Laplacian pyramid, multiscale process, MFP tests |
| Phase 2B: Collimation | 12 | Edge detection, Hough transform, collimation detect, tests |
| Phase 3A: Edge Enhancement | 9 | Fractional derivative, overshoot limiter, safety tests |
| Phase 3B: Exposure Index | 9 | EI lookup, EI/DI computation, QC alert, tests |
| Phase 4: Integration | 10 | Exception guard, logging, pipeline, endurance, coverage |
| Phase 5: SIMD | 6 | AVX2 dispatch, MFP/edge/Hough SIMD, parity, benchmark |
| **Total** | **66** | |

---

*Document End - SPEC-XPE-P2-ADV Tasks v1.0.0*
