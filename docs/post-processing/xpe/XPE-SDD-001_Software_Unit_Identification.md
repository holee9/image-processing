# 소프트웨어 단위 식별

**문서 ID:** XPE-SDD-001 v1.0  
**IEC 62304 Clause:** 5.4.1 (Class B: unit 식별만)  
**안전 분류:** Class B  
**날짜:** 2026-04-03  
**작성자:** XPE 개발 팀  
**승인:** __________________ 날짜: __________  

---

## 1. 목적

XPE software items(XPE-SAD-001)를 software unit 수준으로 분해하여 식별한다. IEC 62304 Class B에서는 unit 식별만 필수이며, 각 unit의 detailed design은 요구되지 않는다.

> **참고:** IEC 62304 정의 — Software Unit: "다른 항목으로 분할되지 않는 SOFTWARE ITEM."

## 2. SWI-1: Pre-Processing Module — Units

| Unit ID | Unit Name | Responsibility | SRS Trace |
|---------|-----------|---------------|-----------|
| SWU-1.1 | OffsetCorrector | Dark/offset subtraction (16-bit saturated arithmetic) | SRS-FUNC-001 |
| SWU-1.2 | GainCorrector | Flat-field gain multiplication (float32) | SRS-FUNC-002 |
| SWU-1.3 | DefectPixelCorrector | Bad pixel detection (threshold ±6σ) & interpolation (4/8-neighbor, bilinear) | SRS-FUNC-003 |
| SWU-1.4 | GhostCorrector | Multi-exponential lag correction, Forward Bias parameter 연동 | SRS-FUNC-004 |
| SWU-1.5 | CalibrationManager | Calibration data load/store/validation/expiry check | SRS-FUNC-001..004, SRS-ALERT-005 |

## 3. SWI-2: Core Processing Module — Units

| Unit ID | Unit Name | Responsibility | SRS Trace | Phase |
|---------|-----------|---------------|-----------|:-----:|
| SWU-2.1 | LogTransform | Logarithmic domain conversion, ε clamping | SRS-FUNC-010 | 1 |
| SWU-2.2 | NoiseReducer | Bilateral filter, Non-Local Means, noise σ estimation (MAD) | SRS-FUNC-011 | 1 |
| SWU-2.3 | ContrastEnhancer | CLAHE (block histogram, clip, redistribute, bilinear interp) | SRS-FUNC-012 | 1 |
| SWU-2.4 | EdgeEnhancer | Unsharp masking (multi-kernel σ), gain limiting | SRS-FUNC-013 | 1 |
| SWU-2.5 | MultiscaleProcessor | Laplacian pyramid build/reconstruct, non-linear gain per level | SRS-FUNC-014 | 2 |
| SWU-2.6 | FractionalProcessor | FMP: fractional decomposition of scale transitions | SRS-FUNC-015 | 2 |
| SWU-2.7 | BodyPartRecognizer | CNN inference (MobileNet-v3 class), DICOM tag fallback | SRS-FUNC-016 | 2 |
| SWU-2.8 | CollimationDetector | Gradient + Hough transform, ROI masking | SRS-FUNC-016 | 2 |
| SWU-2.9 | ImageStitcher | Phase correlation, sub-pixel refinement, weighted blending | SRS-FUNC-017 | 2 |
| SWU-2.10 | ExposureIndexCalc | IEC 62494-1 EI/DI calculation | SRS-FUNC-016 | 2 |
| SWU-2.11 | BoneSuppressionEngine | Residual U-Net inference (ONNX), toggle control | SRS-FUNC-018 | 3 |
| SWU-2.12 | DLDenoiser | DnCNN inference for low-dose enhancement | SRS-FUNC-018 | 3 |

## 4. SWI-3: Display Processing Module — Units

| Unit ID | Unit Name | Responsibility | SRS Trace |
|---------|-----------|---------------|-----------|
| SWU-3.1 | ModalityLUT | Rescale Slope × StoredValue + Intercept | SRS-FUNC-020 |
| SWU-3.2 | VoiLUT | W/L Linear, LINEAR_EXACT, SIGMOID, LUT Sequence | SRS-FUNC-021 |
| SWU-3.3 | PresentationLUT | GSDF P-Value conversion, MONOCHROME1/2 handling | SRS-FUNC-022, 023 |
| SWU-3.4 | LUTManager | Body-part preset storage, custom LUT CRUD, auto-selection | SRS-FUNC-021 |

## 5. SWI-4: DICOM I/O Module — Units

| Unit ID | Unit Name | Responsibility | SRS Trace |
|---------|-----------|---------------|-----------|
| SWU-4.1 | DicomReader | File parsing, pixel data extraction, tag validation | SRS-FUNC-030 |
| SWU-4.2 | DicomWriter | File creation, Type 1/2 tag population, transfer syntax encoding | SRS-FUNC-030, 032 |
| SWU-4.3 | PresentationStateIO | GSPS create/apply, annotation persistence | SRS-FUNC-031 |
| SWU-4.4 | DicomNetworkSCU | C-STORE, C-FIND SCU with TLS | SRS-IF-002, 003, SRS-SEC-001 |

## 6. SWI-5: Common Infrastructure — Units

| Unit ID | Unit Name | Responsibility | SRS Trace |
|---------|-----------|---------------|-----------|
| SWU-5.1 | MemoryPool | Pre-allocated image buffer pool, read-only input enforcement | SRS-SAFE-001, SRS-PERF-004 |
| SWU-5.2 | ThreadPool | Task-based parallel execution, core count auto-scaling | SRS-PERF-001..006 |
| SWU-5.3 | ErrorHandler | Centralized error/exception, module boundary isolation | SRS-SAFE-003, SRS-ALERT-* |
| SWU-5.4 | Logger | spdlog wrapper, audit trail, log level control | SRS-SEC-003 |
| SWU-5.5 | ParameterValidator | Safe-range enforcement per body-part/algorithm | SRS-SAFE-002, 005 |
| SWU-5.6 | ConfigManager | JSON config read/write, checksum validation | SRS-SEC-002 |
| SWU-5.7 | PipelineOrchestrator | Processing stage sequencing, data flow control | SRS-PERF-002 |

## 7. Unit Count Summary

| SW Item | Unit Count | Phase 1 | Phase 2 | Phase 3 |
|---------|:---------:|:-------:|:-------:|:-------:|
| SWI-1 Pre-Processing | 5 | 5 | — | — |
| SWI-2 Core Processing | 12 | 4 | 6 | 2 |
| SWI-3 Display Processing | 4 | 4 | — | — |
| SWI-4 DICOM I/O | 4 | 4 | — | — |
| SWI-5 Infrastructure | 7 | 7 | — | — |
| **Total** | **32** | **24** | **6** | **2** |

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-03 | XPE Team | Initial release |

---

*Document End — XPE-SDD-001 v1.0*
