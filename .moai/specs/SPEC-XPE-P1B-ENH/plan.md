# SPEC-XPE-P1B-ENH: Implementation Plan

**SPEC**: SPEC-XPE-P1B-ENH v1.0.0
**Sprint**: S1-B
**DLL**: xpe_enhance_basic.dll
**Dependency**: S0-B (xpe_common.dll) complete, S1-A (xpe_preprocess.dll) complete

---

## 1. Implementation Strategy

TDD methodology (RED-GREEN-REFACTOR) per project quality.yaml. Each SWU implemented as an independent compilation unit with its own test file. EI computation (SWU-2.10) is implemented first in pipeline execution order but can be developed in any sequence since all SWUs are independent at the code level.

### 1.1 File Structure

```
modules/enhance_basic/
  include/xpe/enhance_basic/
    enhance_basic_api.h        -- 7 function declarations + 3 param structs + 1 enum
    enhance_basic_internal.h   -- internal helpers (not exported)
  src/
    log_transform.cpp          -- SWU-2.1: xpe_log_transform, xpe_log_inverse
    noise_reduce.cpp           -- SWU-2.2: xpe_noise_reduce, xpe_noise_estimate_sigma
    contrast_enhance.cpp       -- SWU-2.3: xpe_contrast_enhance
    edge_enhance.cpp           -- SWU-2.4: xpe_edge_enhance
    exposure_index.cpp         -- SWU-2.10: xpe_calc_exposure_index
  tests/
    test_log_transform.cpp     -- Round-trip fidelity, zero/negative handling, perf
    test_noise_reduce.cpp      -- Bilateral, NLM, sigma estimation, param validation
    test_contrast_enhance.cpp  -- CLAHE correctness, tile edge blending, param validation
    test_edge_enhance.cpp      -- USM correctness, overshoot clamping, param validation
    test_exposure_index.cpp    -- EI/DI computation, bodyPart lookup, alert on DI deviation
    test_enhance_integration.cpp -- Full pipeline, thread safety, P/Invoke compat
  CMakeLists.txt
```

### 1.2 Header Design

The existing stub `enhance_basic_api.h` will be expanded to declare:
- 3 parameter structs (XpeNoiseReduceParams, XpeClaheParams, XpeUsmParams)
- 1 enum (XpeNoiseReduceMode)
- 7 API functions with full Doxygen documentation
- Pattern follows `xpe_preprocess_api.h` exactly (SWU section headers, REQ range comments)

---

## 2. Milestones (Priority-Based)

### Milestone 1: Log Transform + EI Baseline (Priority High)

**Rationale**: Log transform is the pipeline entry point for enhance_basic. EI baseline is a critical clinical metric (IEC 62494-1) and must be computed in detector-domain before log transform.

- SWU-2.10: Exposure Index computation (EI/DI, bodyPart-based EIT lookup)
- SWU-2.1: Log Transform and Inverse (simplest enhancement, validates infrastructure)
- Unit tests for both SWUs
- RED: Write tests for round-trip fidelity (log/inverse), EI/DI accuracy
- GREEN: Implement minimal passing code
- REFACTOR: Extract EIT lookup table, optimize loop

### Milestone 2: Noise Reduction (Priority High)

**Rationale**: Noise reduction is the most computationally expensive SWU and the primary clinical quality driver.

- SWU-2.2: Bilateral filter + NLM + sigma estimation
- Unit tests covering both filter modes and param validation
- RED: Write tests for SNR improvement, edge preservation, sigma estimation accuracy
- GREEN: Implement bilateral filter first (simpler), then NLM
- REFACTOR: Optimize kernel loops for cache locality

### Milestone 3: Contrast + Edge Enhancement (Priority High)

**Rationale**: CLAHE and USM complete the enhancement pipeline.

- SWU-2.3: CLAHE implementation
- SWU-2.4: USM implementation with overshoot clamping
- Unit tests for both SWUs
- RED: Write tests for local contrast improvement, overshoot bounds
- GREEN: Implement tile-based CLAHE, then USM with threshold gating
- REFACTOR: Optimize tile histogram computation, Gaussian blur for USM

### Milestone 4: Integration + Quality (Priority High)

- Full pipeline integration test (EI -> log -> noise -> contrast -> edge)
- P/Invoke compatibility test (C# round-trip for all 7 functions + 3 structs)
- Performance benchmark suite (individual + total pipeline)
- Thread safety test (concurrent independent buffers)
- Memory leak testing (ASan, 1000-iteration cycle)
- Coverage verification (>= 85% statement, >= 70% branch)

---

## 3. Task Breakdown per SWU

### SWU-2.10: ExposureIndexCalc

| Task | Description | REQ Trace |
|------|-------------|-----------|
| T-EI-01 | Define EIT lookup table (bodyPart -> EIT value) | REQ-ENH-025 |
| T-EI-02 | Implement mean pixel value computation | REQ-ENH-023 |
| T-EI-03 | Implement EI = EIT * (mean / S0) formula | REQ-ENH-023 |
| T-EI-04 | Implement DI = 10 * log10(EI / EIT) | REQ-ENH-024 |
| T-EI-05 | Add DI range alert (outside +/-3) | REQ-ENH-026 |
| T-EI-06 | Validate NULL/empty inputs | REQ-ENH-027, REQ-ENH-028 |
| T-EI-07 | Handle zero/negative mean pixel value | REQ-ENH-030 |

### SWU-2.1: LogTransform

| Task | Description | REQ Trace |
|------|-------------|-----------|
| T-LT-01 | Implement forward log transform loop | REQ-ENH-001 |
| T-LT-02 | Implement negative pixel clamping | REQ-ENH-002 |
| T-LT-03 | Validate normFactor | REQ-ENH-003 |
| T-LT-04 | Implement inverse transform loop | REQ-ENH-004 |
| T-LT-05 | Performance optimization (vectorizable loop) | REQ-ENH-006 |

### SWU-2.2: NoiseReducer

| Task | Description | REQ Trace |
|------|-------------|-----------|
| T-NR-01 | Implement bilateral filter kernel | REQ-ENH-007 |
| T-NR-02 | Implement NLM filter kernel | REQ-ENH-008 |
| T-NR-03 | Implement parameter validation | REQ-ENH-009, REQ-ENH-010 |
| T-NR-04 | Implement sigma estimation (MAD method) | REQ-ENH-011 |
| T-NR-05 | Performance optimization (separable bilateral) | REQ-ENH-012 |

### SWU-2.3: ContrastEnhancer

| Task | Description | REQ Trace |
|------|-------------|-----------|
| T-CE-01 | Implement tile-based histogram computation | REQ-ENH-013 |
| T-CE-02 | Implement histogram clipping + redistribution | REQ-ENH-013 |
| T-CE-03 | Implement bilinear tile interpolation | REQ-ENH-013 |
| T-CE-04 | Implement NULL params default behavior | REQ-ENH-014 |
| T-CE-05 | Validate clip_limit and tile dimensions | REQ-ENH-015, REQ-ENH-016 |

### SWU-2.4: EdgeEnhancer

| Task | Description | REQ Trace |
|------|-------------|-----------|
| T-EE-01 | Implement Gaussian blur for unsharp mask | REQ-ENH-018 |
| T-EE-02 | Implement USM with threshold gating | REQ-ENH-018 |
| T-EE-03 | Implement overshoot clamping | REQ-ENH-021 |
| T-EE-04 | Implement NULL params default behavior | REQ-ENH-019 |
| T-EE-05 | Validate amount/radius/threshold ranges | REQ-ENH-020 |

---

## 4. Dependencies Between SWUs

```
SWU-2.10 (EI) -----> independent (computed on detector-domain, before log)
SWU-2.1 (Log)  -----> independent (first enhancement stage)
SWU-2.2 (Noise) ----> depends on SWU-2.1 output (log-domain input)
SWU-2.3 (CLAHE) ----> depends on SWU-2.2 output (denoised input)
SWU-2.4 (USM) ------> depends on SWU-2.3 output (contrast-enhanced input)
```

Pipeline execution order:
1. `xpe_calc_exposure_index` (detector-domain, before any enhancement)
2. `xpe_log_transform` (detector-domain -> log-domain)
3. `xpe_noise_reduce` (log-domain)
4. `xpe_contrast_enhance` (log-domain)
5. `xpe_edge_enhance` (log-domain)

Note: Each SWU is independently compilable and testable. Pipeline-level dependencies are runtime ordering, not build-time dependencies. All SWUs link only to xpe_common.

---

## 5. Risks

| Risk | Impact | Mitigation |
|------|--------|-----------|
| NLM 100ms budget too tight for 3072x3072 | Performance gate failure | Use separable approximation; fallback to bilateral-only if NLM exceeds budget |
| CLAHE float32 histogram quantization | Histogram precision loss | Use 4096-bin quantization with linear interpolation for float32 range |
| EIT lookup table incomplete for rare bodyParts | Missing EIT for exotic exams | Default EIT fallback; log INFO alert for unknown bodyPart |
| Overshoot clamping threshold too aggressive | Clinical detail loss | Parameterize clamp formula; validate with radiologist test images |
| Thread safety regression | Data corruption in multi-threaded use | No global state; all functions pure (input buffer -> output in same buffer) |

---
