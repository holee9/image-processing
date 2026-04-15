# SPEC-XPE-P1B-ENH: Acceptance Criteria

**SPEC**: SPEC-XPE-P1B-ENH v1.0.0
**Sprint**: S1-B

---

## 1. Given-When-Then Scenarios

### Scenario 1: Log Transform Round-Trip Fidelity

**Given** a 3072x3072 float32 image with pixel values in range [0.0, 65535.0] and normFactor = 1000.0
**When** `xpe_log_transform` is applied followed by `xpe_log_inverse` with the same normFactor
**Then** all pixel values are restored within 1e-4 relative error of the original values, and the function returns `XPE_OK` for both calls.

### Scenario 2: Log Transform Zero and Negative Pixel Handling

**Given** a float32 image containing pixels with values {-5.0, 0.0, 1.0, 100.0}
**When** `xpe_log_transform` is called with normFactor = 1.0
**Then** the negative pixel is clamped to 0.0 before log (output = 0.0), the zero pixel outputs 0.0, and positive pixels output `log10(value + 1.0)`.

### Scenario 3: Bilateral Filter Noise Reduction

**Given** a 3072x3072 float32 image with additive Gaussian noise (sigma = 30.0) applied to a known clean reference
**When** `xpe_noise_reduce` is called with bilateral mode (sigma_space=3.0, sigma_range=50.0)
**Then** the output SNR improves by >= 3 dB compared to the noisy input, edge gradient magnitude is preserved within 90% at strong edges, and processing completes within 100ms.

### Scenario 4: NLM Denoising Mode

**Given** a 512x512 float32 test image with additive Gaussian noise (sigma = 20.0)
**When** `xpe_noise_reduce` is called with NLM mode (search_window=21, patch_size=7, h_param=10.0)
**Then** the output SNR improves compared to bilateral mode for repetitive texture regions, and the function returns `XPE_OK`.

### Scenario 5: Noise Sigma Estimation

**Given** a 256x256 float32 image with known additive Gaussian noise (true sigma = 25.0)
**When** `xpe_noise_estimate_sigma` is called
**Then** the estimated sigma is within +/- 20% of the true sigma (i.e., 20.0 <= outSigma <= 30.0).

### Scenario 6: CLAHE Contrast Enhancement

**Given** a 3072x3072 float32 image with a low-contrast region (contrast ratio < 1.05)
**When** `xpe_contrast_enhance` is called with clip_limit=3.0, tile_width=8, tile_height=8
**Then** the local contrast ratio in the low-contrast region improves by >= 20%, no tile boundary artifacts are visible (bilinear interpolation), and processing completes within 50ms.

### Scenario 7: CLAHE with NULL Params (Defaults)

**Given** a valid float32 image
**When** `xpe_contrast_enhance` is called with params = NULL
**Then** the system uses default parameters (clip_limit=3.0, tile_width=8, tile_height=8) and returns `XPE_OK`.

### Scenario 8: USM Edge Enhancement with Overshoot Clamping

**Given** a float32 image containing a sharp edge (step from 100.0 to 1000.0)
**When** `xpe_edge_enhance` is called with amount=2.0, radius=2.0, threshold=5.0
**Then** edge sharpness increases, overshoot at the edge boundary does not exceed `max(original * 2.0, original + amount * threshold)`, and no ringing artifacts appear beyond 3 pixels from the edge.

### Scenario 9: EI/DI Computation for Known Phantom

**Given** a 3072x3072 float32 detector-domain image of a standard test phantom with known mean pixel value = 500.0, bodyPart = "CHEST", known EIT(CHEST) and S0_reference
**When** `xpe_calc_exposure_index` is called
**Then** `outEI = EIT_CHEST * (500.0 / S0_reference)`, `outDI = 10.0 * log10(outEI / EIT_CHEST)`, and both values match within 0.1% of the expected reference calculation.

### Scenario 10: EI DI Warning Alert for Over-Exposure

**Given** a detector-domain image where the computed DI = +4.5 (outside acceptable range)
**When** `xpe_calc_exposure_index` computes the DI
**Then** a WARNING-level alert is posted via the alert queue, the function returns `XPE_OK`, and `outEI` and `outDI` contain the computed values (not suppressed).

### Scenario 11: EI with Unknown Body Part

**Given** metadata with bodyPart = "UNKNOWN_REGION"
**When** `xpe_calc_exposure_index` is called
**Then** the system uses the default general radiography EIT, computes EI/DI normally, and returns `XPE_OK`.

### Scenario 12: Full Enhancement Pipeline

**Given** a 3072x3072 float32 detector-domain image (output from xpe_preprocess.dll)
**When** the full enhance_basic pipeline executes in order: EI -> log_transform -> noise_reduce -> contrast_enhance -> edge_enhance
**Then** total processing time is <= 200ms, each stage returns `XPE_OK`, the output image has improved diagnostic quality (SNR up, local contrast up, edges sharper), and EI/DI values are recorded.

### Scenario 13: P/Invoke Compatibility

**Given** C# P/Invoke declarations for all 7 functions and 3 parameter structs (XpeNoiseReduceParams, XpeClaheParams, XpeUsmParams)
**When** all 7 functions are called from C# with valid parameters
**Then** no `DllNotFoundException`, no `MarshalDirectiveException`, struct layouts match (verified via `sizeof` comparison), and return values are correct.

### Scenario 14: Thread Safety -- Concurrent Processing

**Given** two threads, each with an independent 1024x1024 float32 image buffer
**When** both threads execute `xpe_noise_reduce` (bilateral) simultaneously
**Then** both calls complete without data corruption, race conditions, or deadlocks, and results match single-threaded execution.

---

## 2. Edge Cases

| Edge Case | Expected Behavior | REQ Trace |
|-----------|-------------------|-----------|
| 1x1 float32 image through all functions | All functions return XPE_OK, trivial processing | REQ-ENH-CC-002 |
| 4096x4096 image (maximum supported) | All functions complete within 2x budget (scaled) | REQ-ENH-006, 012, 017, 022 |
| normFactor = FLT_MAX | Log transform produces valid output (no overflow) | REQ-ENH-001 |
| normFactor = FLT_MIN (positive) | Log transform produces very small values (valid) | REQ-ENH-001 |
| All pixels identical (flat field) | Noise reduce: no change; CLAHE: no change; USM: no change | REQ-ENH-007, 013, 018 |
| Image with NaN pixels | Functions return XPE_ERR_PROCESSING_FAILED or handle gracefully | REQ-ENH-CC-002 |
| Image with +Inf pixels | Functions return XPE_ERR_PROCESSING_FAILED or clamp | REQ-ENH-CC-002 |
| CLAHE with image smaller than tile grid | Returns XPE_ERR_INVALID_INPUT | REQ-ENH-016 |
| USM amount = 0.0 | No-op, output equals input | REQ-ENH-018 |
| EI with all-zero image | Returns XPE_ERR_PROCESSING_FAILED, outEI=0, outDI=0 | REQ-ENH-030 |
| EI with ROI-cropped 100x100 sub-buffer | Computes EI/DI correctly on smaller buffer | REQ-ENH-029 |
| Bilateral with sigma_space = 0.001 (near zero) | Returns XPE_ERR_INVALID_INPUT (non-positive check) | REQ-ENH-010 |

---

## 3. Quality Gate Criteria

| Gate | Threshold | Measurement Method |
|------|-----------|-------------------|
| Unit Test Coverage (statement) | >= 85% | gcov + lcov |
| Unit Test Coverage (branch) | >= 70% | gcov + lcov |
| Performance -- individual functions | Each within per-function budget | google-benchmark, 10-run median |
| Performance -- total pipeline | <= 200ms | google-benchmark, 10-run median |
| Memory safety | Zero leaks, zero out-of-bounds | ASan + 1000-iteration cycle test |
| Thread safety | Zero data races | TSan on concurrent test scenario |
| P/Invoke struct alignment | sizeof(C++) == sizeof(C#) | Cross-language sizeof comparison |
| Zero compiler warnings | /W4 (MSVC) | Build log analysis |

---

## 4. Definition of Done

- [ ] All 7 API functions implemented and exported from xpe_enhance_basic.dll
- [ ] 3 parameter structs (XpeNoiseReduceParams, XpeClaheParams, XpeUsmParams) defined in header
- [ ] 6 test files + 1 integration test file created with total >= 50 test cases
- [ ] All 30 EARS requirements (REQ-ENH-001..030) traceable to at least one test
- [ ] All 5 cross-cutting requirements (REQ-ENH-CC-001..005) verified
- [ ] Performance budgets met (individual + total pipeline)
- [ ] No compiler warnings at /W4 level (MSVC)
- [ ] ASan clean (1000-iteration cycle, zero leaks)
- [ ] TSan clean (concurrent execution test, zero data races)
- [ ] Coverage >= 85% statement, >= 70% branch
- [ ] CMakeLists.txt builds xpe_enhance_basic as SHARED library linking xpe_common
- [ ] EI/DI computation matches IEC 62494-1 reference within 0.1%
- [ ] P/Invoke round-trip test passes for all 7 functions and 3 structs

---

## 5. Milestone Completion Gates

### Gate 1: Log Transform + EI Baseline

- [ ] SWU-2.10 and SWU-2.1 implemented
- [ ] test_log_transform.cpp and test_exposure_index.cpp passing
- [ ] Log round-trip fidelity within 1e-4
- [ ] EI/DI matches reference within 0.1%

### Gate 2: Noise Reduction

- [ ] SWU-2.2 implemented (bilateral + NLM + sigma estimation)
- [ ] test_noise_reduce.cpp passing
- [ ] SNR improvement >= 3 dB (bilateral)
- [ ] Processing time <= 100ms

### Gate 3: Contrast + Edge Enhancement

- [ ] SWU-2.3 and SWU-2.4 implemented
- [ ] test_contrast_enhance.cpp and test_edge_enhance.cpp passing
- [ ] CLAHE contrast improvement >= 20%
- [ ] USM overshoot within clamp bounds

### Gate 4: Integration + Quality

- [ ] test_enhance_integration.cpp passing
- [ ] Total pipeline <= 200ms
- [ ] ASan + TSan clean
- [ ] Coverage gates met
- [ ] P/Invoke compatibility verified

---
