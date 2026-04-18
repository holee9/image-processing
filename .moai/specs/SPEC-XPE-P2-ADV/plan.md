# Implementation Plan: SPEC-XPE-P2-ADV

---
spec_id: SPEC-XPE-P2-ADV
version: 1.0.0
status: Planned
created: 2026-04-17
updated: 2026-04-17
author: manager-spec (MoAI)
---

## 1. Implementation Overview

### 1.1 Architecture Position

```
Layer 0: xpe_common.dll (completed in S0-B)
    | (depends)
Layer 1: xpe_enhance_basic.dll (completed in S1-B1)
    | (input: log-transformed float32 image)
    v
Layer 1: xpe_enhance_advanced.dll (THIS SPEC)
    | (depends only on xpe_common)
    v
Layer 2: C# ImageProcTest GUI (P/Invoke)
```

xpe_enhance_advanced.dll은 xpe_common.dll(Layer 0)에만 의존한다. xpe_enhance_basic.dll과는 DLL-to-DLL 의존이 없으며, 호출자(orchestrator)가 실행 순서를 보장한다. 다른 Layer 1 DLL과는 상호 의존하지 않는다(Anti-Spaghetti 원칙).

### 1.2 Technology Stack

| Component        | Version     | Purpose                           |
|------------------|-------------|-----------------------------------|
| C++ Standard     | C++17       | Module implementation language    |
| C ABI            | C11         | DLL export boundary               |
| CMake            | >= 3.25     | Build system                      |
| Eigen            | 3.4.x       | Matrix operations, FFT, pyramid   |
| Google Test      | 1.14.x      | Unit/integration test framework   |
| spdlog           | 1.13.x      | Async logging (via xpe_common)    |
| nlohmann/json    | 3.11.x      | JSON config parsing               |
| fmt              | 10.x        | String formatting                 |
| AVX2 Intrinsics  | immintrin.h | SIMD optimization                 |
| vcpkg            | manifest    | SOUP dependency management        |

### 1.3 File Structure Plan

```
modules/enhance_advanced/
    CMakeLists.txt                              # Build config (Eigen link, xpe_common)
    include/xpe/enhance_advanced/
        xpe_enhance_advanced_api.h              # API declarations (4 functions)
    src/
        xpe_enhance_advanced.cpp                # Lifecycle, utility implementation
        xpe_multiscale_process.cpp              # MFP (SWU-2.5)
        xpe_fractional_process.cpp              # Fractional edge enhancement (SWU-2.6)
        xpe_collimation_detect.cpp              # ROI detection (SWU-2.8)
        xpe_exposure_index.cpp                  # EI/DI calculation (SWU-2.10)
        simd/
            xpe_mfp_avx2.cpp                    # AVX2 MFP optimization
            xpe_edge_avx2.cpp                   # AVX2 edge enhancement
            xpe_hough_avx2.cpp                  # AVX2 Hough transform
            xpe_simd_dispatch.cpp               # Runtime SIMD feature detection
        detail/
            laplacian_pyramid.h                 # Pyramid decomposition/reconstruction
            laplacian_pyramid.cpp               # Pyramid implementation
            hough_transform.h                   # Hough line detection
            hough_transform.cpp                 # Hough implementation
            edge_detection.h                    # Canny/Sobel operators
            edge_detection.cpp                  # Edge detection implementation
            overshoot_limiter.h                 # Mandatory overshoot clipping
            overshoot_limiter.cpp               # Overshoot limiting implementation
            fractional_derivative.h             # Fractional-order differentiation
            fractional_derivative.cpp           # Fractional derivative implementation
            ei_lookup_table.h                   # Body-part EI target values
            ei_lookup_table.cpp                 # EI target lookup implementation
    tests/
        CMakeLists.txt                          # Test build config
        test_multiscale_process.cpp             # MFP tests
        test_fractional_process.cpp             # Edge enhancement tests
        test_collimation_detect.cpp             # ROI detection tests
        test_exposure_index.cpp                 # EI/DI tests
        test_simd_parity.cpp                    # Scalar vs SIMD equivalence tests
        test_integration_pipeline.cpp           # Full pipeline integration test
        test_overshoot_safety.cpp               # Mandatory safety tests
        test_data/                              # Test data (synthetic)
            collimation_synthetic_512x512.raw
            collimation_synthetic_1024x1024.raw
            wire_phantom_3072x3072.raw
            flat_field_3072x3072.raw
```

---

## 2. Milestone Decomposition

### Milestone M1: Foundation (Priority: High)

**Goal**: Module scaffolding, build integration, API headers, lifecycle functions

| Task | Description                                                | Dependency  |
|------|------------------------------------------------------------|-------------|
| M1-1 | `CMakeLists.txt` 작성 (xpe_common 링크, Eigen 의존성)       | None        |
| M1-2 | `xpe_enhance_advanced_api.h` 헤더 작성 (4개 함수 선언)      | None        |
| M1-3 | Lifecycle 구현 (init/shutdown/version)                      | M1-1, M1-2  |
| M1-4 | 빌드 통합 테스트 (root CMakeLists.txt 인식 확인)            | M1-3        |
| M1-5 | `test_integration_pipeline.cpp` 스캐폴딩                    | M1-4        |

**Deliverable**: Buildable xpe_enhance_advanced.dll (empty functions), P/Invoke loadable

### Milestone M2: MFP Scalar Reference (Priority: High)

**Goal**: Laplacian pyramid decomposition and reconstruction without SIMD

| Task | Description                                                         | Dependency |
|------|---------------------------------------------------------------------|------------|
| M2-1 | `laplacian_pyramid.h/cpp` Gaussian pyramid level computation         | M1         |
| M2-2 | `laplacian_pyramid.h/cpp` Laplacian level computation (subtract-expand) | M2-1 |
| M2-3 | `laplacian_pyramid.h/cpp` Reconstruction from Laplacian pyramid      | M2-2       |
| M2-4 | `xpe_multiscale_process.cpp` Per-band enhancement coefficient logic  | M2-3       |
| M2-5 | Body-part adaptive parameter selection (CHEST, EXTREMITY, etc.)      | M2-4       |
| M2-6 | Identity reconstruction test (alpha_k=1.0 -> output == input)       | M2-3       |
| M2-7 | NaN/Inf validation in reconstruction path                           | M2-4       |

**Algorithm Reference** (PRD, XPE-ALG-001):
- Gaussian pyramid: `G_{k+1} = Downsample(GaussianBlur(G_k))`
- Laplacian: `L_k = G_k - Upsample(G_{k+1})`
- Enhancement: `L'_k = alpha_k(bodyPart) * L_k`
- Reconstruction: `G'_k = L'_k + Upsample(G'_{k+1})`

### Milestone M3: Edge Enhancement (Priority: High)

**Goal**: Fractional-order differentiation with mandatory overshoot limiting

| Task | Description                                                        | Dependency |
|------|--------------------------------------------------------------------|------------|
| M3-1 | `fractional_derivative.h/cpp` Gruenwald-Letnikov implementation     | M1         |
| M3-2 | `overshoot_limiter.h/cpp` +-3sigma local clipping                  | M3-1       |
| M3-3 | `xpe_fractional_process.cpp` Integration with overshoot limiter     | M3-1, M3-2 |
| M3-4 | Safety test: verify XPE_ERR_SAFETY_VIOLATION on disable attempt     | M3-2       |
| M3-5 | Order parameter validation [0.0, 2.0]                               | M3-3       |

**Algorithm Reference** (PRD):
- Fractional derivative: Gruenwald-Letnikov definition, order alpha in [0, 2]
- Overshoot: `limited = clamp(boost, -3*sigma_3x3, +3*sigma_3x3)` (mandatory)

### Milestone M4: Collimation Detection (Priority: High)

**Goal**: Hough transform based collimation ROI detection

| Task | Description                                                        | Dependency |
|------|--------------------------------------------------------------------|------------|
| M4-1 | `edge_detection.h/cpp` Sobel gradient computation                  | M1         |
| M4-2 | `hough_transform.h/cpp` Accumulator construction                   | M4-1       |
| M4-3 | `hough_transform.h/cpp` Peak detection and axis-aligned filtering  | M4-2       |
| M4-4 | `xpe_collimation_detect.cpp` ROI rectangle extraction              | M4-3       |
| M4-5 | Confidence scoring implementation                                   | M4-3       |
| M4-6 | Fallback logic (confidence < 0.7 -> full image extent)             | M4-5       |
| M4-7 | Synthetic collimation test data generation                         | M4-4       |

**Algorithm Reference** (SRS-ENHANCE-ADV-001, FR-1100):
- Edge map via Sobel gradient magnitude
- Hough accumulator: theta step 1 deg, rho step 1 pixel
- Axis filter: theta in [-5, +5] union [85, 95] degrees
- Confidence: `sum(4 peak values) / (4 * max_accumulator_value)`

### Milestone M5: Exposure Index (Priority: High)

**Goal**: IEC 62494-1 compliant EI/DI calculation

| Task | Description                                                        | Dependency |
|------|--------------------------------------------------------------------|------------|
| M5-1 | `ei_lookup_table.h/cpp` Body-part EI target values                 | M1         |
| M5-2 | `xpe_exposure_index.cpp` EI computation per IEC 62494-1            | M5-1       |
| M5-3 | DI computation: `10 * log10(EI / EI_target)`                       | M5-2       |
| M5-4 | QC alert for |DI| > 3                                             | M5-3       |
| M5-5 | ROI-aware masking (if sidecar ROI available from caller)            | M5-2       |

**Algorithm Reference** (IEC 62494-1):
- `EI = c1 * g * mean(pixel_values) + c2`
- `DI = 10 * log10(EI / EI_target)`
- EI_target from body-part lookup table

### Milestone M6: Test Infrastructure (Priority: High)

**Goal**: TDD based comprehensive testing, 85% coverage

| Task | Description                                                        | Dependency |
|------|--------------------------------------------------------------------|------------|
| M6-1 | `test_multiscale_process.cpp` golden reference + edge cases         | M2         |
| M6-2 | `test_fractional_process.cpp` parameter validation + safety         | M3         |
| M6-3 | `test_collimation_detect.cpp` synthetic ROI tests                   | M4         |
| M6-4 | `test_exposure_index.cpp` IEC 62494-1 conformance                  | M5         |
| M6-5 | `test_overshoot_safety.cpp` mandatory safety verification           | M3         |
| M6-6 | `test_integration_pipeline.cpp` full pipeline timing test           | M2-M5      |
| M6-7 | SIMD parity test harness preparation                               | M2         |
| M6-8 | 1000-cycle memory leak test                                        | M2-M5      |
| M6-9 | Coverage measurement and 85% target verification                   | M6-1~M6-8  |

**Test Data Strategy**:
- Synthetic collimation images with known borders (ground truth +-3 pixel tolerance)
- Wire phantom for MTF validation
- Flat-field phantom for EI verification
- Random FLOAT32 images for numerical stability tests

### Milestone M7: SIMD Optimization (Priority: Medium)

**Goal**: AVX2 optimization with numerical parity verification

| Task | Description                                                        | Dependency |
|------|--------------------------------------------------------------------|------------|
| M7-1 | `xpe_simd_dispatch.cpp` CPUID based AVX2 runtime detection         | M1         |
| M7-2 | `xpe_mfp_avx2.cpp` AVX2 Gaussian blur and pyramid computation      | M2, M7-1   |
| M7-3 | `xpe_edge_avx2.cpp` AVX2 fractional differentiation                | M3, M7-1   |
| M7-4 | `xpe_hough_avx2.cpp` AVX2 gradient and accumulator                 | M4, M7-1   |
| M7-5 | `test_simd_parity.cpp` Scalar vs AVX2 equivalence tests            | M7-2~M7-4  |
| M7-6 | Performance benchmark (3072x3072 target verification)               | M7-5       |

---

## 3. Technical Constraints

### 3.1 IEC 62304 Class B Compliance

| Item                | Requirement                                     | Verification          |
|---------------------|-------------------------------------------------|-----------------------|
| Exception safety    | C++ exceptions shall not cross C ABI boundary    | Code review, test     |
| Memory safety       | Allocation/deallocation pairs guaranteed         | 1000-cycle test       |
| Thread safety       | Global mutable state protected by mutex          | Concurrency test      |
| Input validation    | All pointer/dimension validation                 | Negative test         |
| Traceability        | REQ -> SWU -> DLL function -> Test 1:1 mapping   | Test matrix           |
| Mandatory safety    | Overshoot limiting cannot be disabled            | Code review + test    |

### 3.2 Dependency Constraints

- **Allowed**: xpe_common.dll, Eigen, spdlog, nlohmann_json, fmt, Google Test
- **Forbidden**: OpenCV, ONNX Runtime, DCMTK, FFTW3
- **Reason**: Module decoupling (Anti-Spaghetti principle)

### 3.3 Build Constraints

- CMake >= 3.25, C++17 standard
- Warnings as errors support (XPE_WARNINGS_AS_ERRORS)
- vcpkg manifest mode for SOUP version pinning
- BUILD_SHARED_LIBS=ON (DLL build)

---

## 4. Risk Analysis

### 4.1 Technical Risks

| Risk ID | Risk                                   | Probability | Impact | Mitigation                                              |
|---------|----------------------------------------|:-----------:|:------:|---------------------------------------------------------|
| R-01    | Laplacian pyramid numerical instability| Medium      | High   | Identity reconstruction test (alpha=1.0), NaN/Inf guards |
| R-02    | SIMD parity mismatch (scalar != AVX2)  | Medium      | High   | M7-5 parity test, tolerance-based verification          |
| R-03    | Performance target not met (< 2500ms)  | Low         | High   | M7-6 benchmark, scalar fallback guaranteed              |
| R-04    | Hough transform accuracy on low-contrast images | Medium | Medium | Confidence scoring, fallback to full-image ROI          |
| R-05    | Overshoot limiting bypass via config   | Low         | High   | Hardcoded enforcement, code review gate (SAF-100)       |
| R-06    | Eigen dependency version conflict      | Low         | Medium | vcpkg manifest pinning, static linking option           |

### 4.2 Cross-Validation References

| ID   | Issue                    | Resolution                                        |
|------|--------------------------|---------------------------------------------------|
| R8-04| NLM performance risk     | MFP-based approach prioritized; NLM deferred. AVX2 SIMD optimization for all compute-intensive kernels |

---

## 5. Testing Strategy

### 5.1 Test Categories

| Category                  | Test Count (Est.) | Coverage Target |
|---------------------------|:-----------------:|:---------------:|
| Unit - MFP                | 15+               | 90%             |
| Unit - Edge Enhancement   | 15+               | 90%             |
| Unit - Collimation        | 20+               | 85%             |
| Unit - Exposure Index     | 15+               | 85%             |
| Integration - Pipeline    | 10+               | 80%             |
| Safety - Overshoot        | 8+                | 100% path       |
| SIMD Parity               | 15+               | 100% path       |
| Performance               | 6+                | N/A             |
| Memory Safety             | 4+                | N/A             |
| **Total**                 | **108+**          | **>= 85%**      |

### 5.2 SIMD Parity Harness

```
For each processing function:
  1. Generate random FLOAT32 image (512x512, 1024x1024, 3072x3072)
  2. Execute scalar implementation -> result_scalar
  3. Execute AVX2 implementation -> result_avx2
  4. Assert numerical equivalence: max|result_scalar - result_avx2| < 1e-6
  5. Repeat for 100 random inputs
```

---

## 6. Milestone Execution Order

```
M1 (Foundation)
    |
    +---> M2 (MFP Scalar Reference) <--- TDD RED-GREEN-REFACTOR
    |         |
    +---> M3 (Edge Enhancement) <------- TDD RED-GREEN-REFACTOR
    |         |
    +---> M4 (Collimation Detection) <--- TDD RED-GREEN-REFACTOR
    |         |
    +---> M5 (Exposure Index) <----------- TDD RED-GREEN-REFACTOR
              |
              v
         M6 (Test Infrastructure) <--- M2-M5와 병행, TDD cycle 내
              |
              v
         M7 (SIMD Optimization) <--- Scalar 검증 후
```

**Note**: TDD methodology applies RED-GREEN-REFACTOR cycle for each M2-M5 task. M6 runs in parallel with M2-M5 within the TDD cycle.

---

## 7. Acceptance Criteria Summary

| #  | Criterion                                         | Verification Method          |
|----|---------------------------------------------------|------------------------------|
| A1 | 4개 함수 모두 빌드 및 export 확인                   | `dumpbin /exports`           |
| A2 | MFP identity reconstruction passes (error < 1e-5) | Unit test                    |
| A3 | Overshoot limiting cannot be disabled              | Safety test (SAF-100)        |
| A4 | Collimation detection accuracy +-3 pixels          | Synthetic ground truth test  |
| A5 | Performance target < 2500ms total                  | Benchmark test               |
| A6 | Test coverage >= 85%                               | gcov/llvm-cov                |
| A7 | P/Invoke 호환 (C#에서 DLL 로드 성공)                | Integration test             |
| A8 | IEC 62304 Class B 준수 (no exceptions, no leaks)   | Code review + 1000-cycle     |
| A9 | AVX2 numerical parity (error < 1e-6)               | Parity test                  |

---

*Document End - SPEC-XPE-P2-ADV Plan v1.0.0*
