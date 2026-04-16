> **ARCHIVED**: This document is a derivative summary of normative documents (product.md, pipeline-spec.md, SPEC-XPE-MASTER.md). Refer to those normative sources directly. Kept for audit trail only.

# X-ray Image Processing Engine - Implementation Analysis Report

**Document ID**: XPE-IMPL-ANALYSIS-001
**Version**: 1.0.0
**Date**: 2026-04-14
**Author**: MoAI
**Source**: SPEC-XPE-MASTER v2.0.0, PIPE-SPEC-001 v1.3.0, XPE-API-SPEC-001 v1.1.0, SPEC-XPE-P0 v1.1.0, XPE-SRS-001 v1.0, product.md, structure.md

---

## 1. Project Overview

X-ray Flat Panel Detector(FPD)에서 획득한 Raw 영상 데이터를 진단 가능한 의료 영상으로 변환하는 영상처리 엔진.

- **Domain**: Medical Device Software (X-ray Flat Panel Detector)
- **Regulatory**: IEC 62304 Class B, FDA 21 CFR 820.30, EU MDR 2017/745
- **Architecture**: Anti-Spaghetti 3-Layer Design (C ABI DLL + C# WPF Orchestrator)

---

## 2. Pipeline Operation Flow

구현 완료 시 다음과 같은 17단계 영상처리 파이프라인이 동작합니다.

### 2.1 Full Pipeline (17 Stages)

```
[X-ray Detector] -> Raw Frame (uint16, 3072x3072)
    |
    v Phase 0: Foundation
    |-- CalibManager Load (offset/gain/BPM calibration data)
    |
    v Phase 1a: Pre-Processing (Raw -> Clean Image, <500ms)
    |-- (0.5) Readout Artifact Validation (non-mutating, flag + alert only)
    |-- (0.7) Temperature Compensation (sensor-based dark current correction)
    |-- (1)   Offset Correction (dark current subtraction)         [MANDATORY]
    |-- (1.5) Nonlinearity Correction (LUT/polynomial)
    |-- (2)   Gain Correction (uint16->float32 conversion)         [MANDATORY, FORMAT BOUNDARY]
    |-- (2.5) Binning Correction (fluoroscopy/CBCT only)
    |-- (3)   Defect Pixel Correction (BPM-based interpolation)
    |-- (4)   Ghost/Lag Correction (3-tier auto-escalation)
    |
    v Phase 1b: Enhancement + Display + DICOM (<3000ms total)
    |-- (EI-0) Exposure Index Baseline (IEC 62494-1)
    |-- (5)   Log Transform (linear -> log domain)
    |-- (6)   Noise Reduction (Bilateral + NLM)
    |-- (7)   Contrast Enhancement (CLAHE)
    |-- (8)   Edge Enhancement (USM)
    |-- (14)  Modality LUT -> (15) VOI LUT -> (16) Presentation LUT
    |-- (17)  DICOM Write (DX IOD)
    |
    v Phase 2: Advanced Enhancement (<2500ms additional)
    |-- (5b)  Collimation Detection (Gradient+Hough)
    |-- (EI-1) ROI-aware EI Refinement
    |-- (9)   GSVG (Grid Suppression / Virtual Grid)
    |-- (10)  Multiscale Frequency Processing
    |-- (11)  Fractional Processing
    |
    v Phase 3: AI/Intelligence (<3000ms additional)
    |-- (5a)  Body Part Recognition (MobileNet-v3)
    |-- (5c)  AI Collimation Refinement
    |-- (12)  Image Stitching (full spine / long leg)
    |-- (13)  Bone Suppression (Residual U-Net)
    |-- DL Denoiser (DnCNN)
    |
    v Output
    +-- Diagnostic-Ready DICOM Image
```

### 2.2 Phase-Gated DLL Loading

| Phase | DLLs | Required | Fallback |
|-------|------|:--------:|----------|
| **Phase 1** | `xpe_common` + `xpe_preprocess` + `xpe_enhance_basic` + `xpe_display` + `xpe_dicom` | YES | Pipeline cannot start without these |
| **Phase 2** | `gsvg` + `xpe_enhance_advanced` | NO | Stages skipped, graceful degradation |
| **Phase 3** | `xpe_ai` + `xpe_ai_worker.exe` | NO | AI disabled, deterministic fallback path |

### 2.3 Pre-Processing Dependency Chain

```
                    (0) CalibManager Load [startup]
                     |
         +-----------+-----------+
         |           |           |
     offsetMap    gainMap    BPM + NLCSC
         |           |           |
  (0.5) Readout Validation [advisory, non-mutating]
         |
  (0.7) Temperature Compensation [conditional]
         |
  (1) Offset Correction [MANDATORY]             uint16 domain
         |
  (1.5) Nonlinearity Correction [conditional]
         |
  (2) Gain Correction [MANDATORY]               uint16 -> float32
  ============= FORMAT BOUNDARY ==================
         |
  (2.5) Binning Correction [conditional]        float32 domain
         |
  (3) Defect Pixel Correction [conditional]
         |
  (4) Ghost/Lag Correction [conditional]
         |
  -> Enhancement Domain (stage 5+)
```

### 2.4 Branching Points

| Branch Point | Trigger | Paths |
|-------------|---------|-------|
| **BP-1**: Body Part Recognition | Phase 3 loaded | Sets parameters for stages (6)-(11) |
| **BP-2**: Collimation Detection | Phase 2 loaded | ROI for EI calculation and display |
| **BP-3**: GSVG Grid Detection | Phase 2 loaded | Grid detected -> Suppression / Not detected -> Virtual Grid |
| **BP-4**: Ghost Tier Escalation | Always | Tier 1 (LTI) -> Tier 2 (Weighted) -> Tier 3 (NLCSC) |
| **BP-5**: Image Stitching | Multi-exposure + Phase 3 | Merge frames / Pass through |
| **BP-6**: DL Toggle | User control | DL_ENABLED -> Bone suppression / DL_DISABLED -> Skip |

---

## 3. User-Facing Features

### 3.1 Phase 1 - Core Features (Regulatory Mandatory)

| Feature | User Value | Regulatory Requirement |
|---------|-----------|----------------------|
| **Raw-to-Clean Pre-Processing** | Removes sensor noise, dark current, defective pixels, hardware artifacts | IEC 62304 mandatory |
| **Ghost/Lag Correction** | Eliminates residual signal from consecutive exposures (1st frame lag < 0.5%) | Safety-related (HAZ-004) |
| **Image Enhancement** (Log/Noise/Contrast/Edge) | Produces diagnostic-quality images | IEC mandatory |
| **Exposure Index (EI/DI)** | Automatic exposure adequacy assessment per IEC 62494-1 | IEC mandatory |
| **DICOM Display Pipeline** | Modality/VOI/Presentation LUT for standard image display | DICOM compliance |
| **DICOM I/O** | DX IOD read/write, JPEG 2000 Lossless, Network C-STORE/C-FIND | DICOM mandatory |
| **Real-time W/L Adjustment** | Window/Level drag response within 16ms | Usability |
| **QA Constancy Test** | AAPM TG-151, IEC 61223 quality management | Regulatory mandatory |

### 3.2 Phase 2 - Differentiating Features

| Feature | User Value |
|---------|-----------|
| **GSVG** (Grid Suppression + Virtual Grid) | Scatter correction without physical grid, grid artifact removal |
| **Multiscale Frequency Processing** | Fine-grained image adjustment via 8-level Laplacian pyramid |
| **Automatic Collimation Detection** | ROI-based EI refinement, automatic unnecessary area masking |
| **Fractional Processing** | Density transition zone artifact removal |

### 3.3 Phase 3 - AI Premium Features

| Feature | User Value | Performance Target |
|---------|-----------|-------------------|
| **Body Part Recognition** | Automatic parameter optimization | 15+ categories, >= 95% accuracy |
| **Bone Suppression** | Virtual dual-energy subtraction for lung nodule detection | PSNR >= 33dB, SSIM >= 0.97 |
| **Image Stitching** | Full spine/long leg panorama | Cobb angle error <= 2 degrees |
| **DL Denoiser** | Low-dose image quality improvement | DnCNN-based |

---

## 4. Architecture After Implementation

### 4.1 Anti-Spaghetti 3-Layer Design

```
+---------------------------------------------------+
|  Layer 2: C# WPF GUI (ImageProcTest.exe)          |
|  - PipelineOrchestrator (SWU-5.7)                 |
|  - QA Constancy Test (SWU-6.1)                    |
|  - P/Invoke for all DLL calls                     |
+------------------------+--------------------------+
                         | P/Invoke (Pack=8, __cdecl)
+------------------------+--------------------------+
|  Layer 1: Algorithm DLLs (no lateral dependencies) |
|                                                     |
|  xpe_preprocess.dll    xpe_enhance_basic.dll       |
|  xpe_display.dll       xpe_dicom.dll               |
|  xpe_enhance_advanced.dll (Phase 2)                |
|  xpe_ai.dll + xpe_ai_worker.exe (Phase 3)         |
|                                                     |
|  Layer 1-G: gsvg.dll (fully independent)           |
+------------------------+--------------------------+
                         | Link dependency
+------------------------+--------------------------+
|  Layer 0: xpe_common.dll                           |
|  Types, Memory Pool, Thread Pool, Logger,          |
|  Config, Error Handler, Alert, Auto Exposure Detection |
+---------------------------------------------------+
```

### 4.2 Numerical Summary

| Item | Count |
|------|:-----:|
| Total SWU (Software Units) | **43** (C/C++ 41 + C# 2) |
| Total DLL/EXE | **8 DLLs** + 1 Worker EXE + 1 GUI EXE |
| Total API Functions (C ABI exported) | **83** |
| Total Sub-SPECs | **9** |
| IEC 62304 Document Packages | **XPE 11 docs** + **GSVG 9 docs** |

### 4.3 Module-to-DLL Mapping

| Module | DLL Output | Layer | Phase | SWU Count | API Count |
|--------|-----------|:-----:|:-----:|:---------:|:---------:|
| modules/common/ | xpe_common.dll | 0 | 0 | 7 | 18 |
| modules/preprocess/ | xpe_preprocess.dll | 1 | 1a | 9 | 18 |
| modules/enhance_basic/ | xpe_enhance_basic.dll | 1 | 1b | 5 | 7 |
| modules/display/ | xpe_display.dll | 1 | 1b | 4 | 11 |
| modules/dicom/ | xpe_dicom.dll | 1 | 1b | 4 | 10 |
| modules/enhance_advanced/ | xpe_enhance_advanced.dll | 1 | 2 | 3 | 4 |
| gsvg/ | gsvg.dll | 1-G | 2 | 4 | 8 |
| modules/ai/ | xpe_ai.dll + worker | 1 | 3 | 4 | 7 |
| gui/ | ImageProcTest.exe | 2 | 0+ | 2 | N/A |
| **Total** | | | | **43** | **83** |

### 4.4 Complete SWU Inventory (43 Units)

#### SWI-1: Pre-Processing (9 SWU) -- xpe_preprocess.dll

| Unit ID | Name | Research ID | Key APIs | Phase |
|---------|------|:-----------:|----------|:-----:|
| SWU-1.1 | OffsetCorrector | PRE-02 | `xpe_offset_correct` | 1a |
| SWU-1.2 | GainCorrector | PRE-03 | `xpe_gain_correct` | 1a |
| SWU-1.3 | DefectPixelCorrector | PRE-06 | `xpe_defect_correct`, `xpe_defect_detect_runtime` | 1a |
| SWU-1.4 | GhostCorrector | PRE-04/05 | `xpe_ghost_create/correct/reset/destroy` | 1a |
| SWU-1.5 | CalibrationManager | SUP-01 | `xpe_calib_*` (6 functions) | 1a |
| SWU-1.6 | TempCompensator | PRE-07 | `xpe_temp_compensate` | 1a |
| SWU-1.7 | NonlinearityCorrector | PRE-08 | `xpe_nonlinearity_correct` | 1a |
| SWU-1.8 | BinningCorrector | PRE-09 | `xpe_binning_correct` | 1a |
| SWU-1.9 | ReadoutArtifactValidator | PRE-01 | `xpe_validate_readout_artifact` | 1a |

#### SWI-2: Core Processing (12 SWU)

| Unit ID | Name | Research ID | DLL | Phase |
|---------|------|:-----------:|-----|:-----:|
| SWU-2.1 | LogTransform | POST-01 | enhance_basic | 1b |
| SWU-2.2 | NoiseReducer | POST-02 | enhance_basic | 1b |
| SWU-2.3 | ContrastEnhancer | POST-03 | enhance_basic | 1b |
| SWU-2.4 | EdgeEnhancer | POST-04 | enhance_basic | 1b |
| SWU-2.5 | MultiscaleProcessor | POST-05 | enhance_advanced | 2 |
| SWU-2.6 | FractionalProcessor | -- | enhance_advanced | 2 |
| SWU-2.7 | BodyPartRecognizer | POST-06 | ai | 3 |
| SWU-2.8 | CollimationDetector | POST-07 | enhance_advanced | 2 |
| SWU-2.9 | ImageStitcher | POST-08 | ai | 3 |
| SWU-2.10 | ExposureIndexCalc | SUP-03 | enhance_basic (1b) + enhance_advanced (2) | 1b/2 |
| SWU-2.11 | BoneSuppressionEngine | POST-09 | ai | 3 |
| SWU-2.12 | DLDenoiser | POST-02 DL | ai | 3 |

#### SWI-3: Display Processing (4 SWU) -- xpe_display.dll

| Unit ID | Name | Phase |
|---------|------|:-----:|
| SWU-3.1 | ModalityLUT | 1b |
| SWU-3.2 | VoiLUT | 1b |
| SWU-3.3 | PresentationLUT | 1b |
| SWU-3.4 | LUTManager | 1b |

#### SWI-4: DICOM I/O (4 SWU) -- xpe_dicom.dll

| Unit ID | Name | Phase |
|---------|------|:-----:|
| SWU-4.1 | DicomReader | 1b |
| SWU-4.2 | DicomWriter | 1b |
| SWU-4.3 | PresentationStateIO | 1b |
| SWU-4.4 | DicomNetworkSCU | 1b |

#### SWI-5: Common Infrastructure (7 SWU) -- xpe_common.dll

| Unit ID | Name | Phase |
|---------|------|:-----:|
| SWU-5.1 | MemoryPool | 0 |
| SWU-5.2 | ThreadPool | 0 |
| SWU-5.3 | ErrorHandler | 0 |
| SWU-5.4 | Logger | 0 |
| SWU-5.5 | ParameterValidator | 0 |
| SWU-5.6 | ConfigManager | 0 |
| SWU-5.8 | AedEventInterface | 0 |

#### SWI-6: GSVG (4 SI) -- gsvg.dll (independent)

| Unit ID | Name | Phase |
|---------|------|:-----:|
| SI-001 | GridDetector | 2 |
| SI-002 | GridSuppressor | 2 |
| SI-003 | VirtualGridGenerator | 2 |
| SI-004 | ScatterLUTManager | 2 |

#### Layer 2: C# GUI (2 SWU)

| Unit ID | Name | Phase |
|---------|------|:-----:|
| SWU-5.7 | PipelineOrchestrator | 0+ |
| SWU-6.1 | QaConstancyTest | 1b+ |

---

## 5. Implementation Impact Analysis

### 5.1 Regulatory Compliance Impact

| Impact | Detail |
|--------|--------|
| **IEC 62304 Class B Compliance** | 43 SWU-level traceability, SRS/SDD/RTM complete |
| **FDA 510(k) Readiness** | Full pipeline verification with test infrastructure |
| **EU MDR Compliance** | Hazard analysis (SHA), SOUP analysis complete |
| **Reproducibility Guarantee** | Deterministic pipeline with AI fallback paths |

### 5.2 Technical Impact

| Impact | Detail |
|--------|--------|
| **Modularity** | Independent DLL replacement, phase-by-phase incremental deployment |
| **Performance** | Phase 1 total < 3s/frame, VOI LUT < 16ms real-time |
| **Memory Efficiency** | Phase 1 only 190MB, max 740MB (Phase 3 with AI) |
| **Scalability** | Phase 1 operates independently without Phase 2/3 DLLs |
| **Cross-Platform ABI** | C ABI (Pack=8, __cdecl), full C# P/Invoke compatibility |
| **HW Migration Ready** | SW-first design; PRE-02/03/06/08/09 can migrate to FPGA |

### 5.3 Competitive Advantage

| Differentiating Feature | Industry Advantage |
|------------------------|-------------------|
| **NLCSC Ghost Correction** (Tier 3) | 14-50x performance advantage (1st frame lag 0.29%) |
| **ML Defect Pixel Correction** | 14.2x NMSE improvement |
| **Virtual Grid** | No physical grid needed, CNR >= 90% |
| **Bone Suppression** | PSNR >= 33dB, SSIM >= 0.97 vs real DES |
| **Auto Body Part Recognition** | Automatic parameter optimization (15+ categories) |

### 5.4 Safety Mechanisms

| Safety Mechanism | Effect |
|-----------------|--------|
| **3-tier Ghost Auto-Escalation** | Auto-promotes Tier 1 -> 2 -> 3 when insufficient |
| **GSVG SAFE-003** | On GSVG failure, returns original unmodified buffer |
| **AI Deterministic Fallback** | Deterministic alternative path when AI unavailable |
| **Bypass Safety Constraints** | 8 BYP-SAFE rules prevent dangerous stage bypass |
| **Checksum Verification** (5 points) | CK-1 through CK-5 ensure pipeline data integrity |
| **Calibration Expiry Detection** | Automatic CRC verification + expiry pre-notification |

### 5.5 Performance Budget

| Scope | Time Budget | Memory Budget |
|-------|:----------:|:------------:|
| Startup calibration load | 200ms (one-time) | -- |
| Pre-processing (stages 0.5-4) | < 500ms / frame | -- |
| Phase 1 per-frame total | < 3000ms / frame | <= 190 MB |
| Phase 2 additions | < 2500ms additional | <= 440 MB |
| Phase 3 additions | < 3000ms additional | <= 740 MB |
| VOI LUT interactive | <= 16ms | -- |

---

## 6. Risk Summary

| Risk | Impact | Likelihood | Mitigation |
|------|--------|:----------:|-----------|
| Ghost PRD fixed-point mismatch with XPE float32 pipeline | High | Medium | float32 implementation first; fixed-point optimization on FPGA migration |
| GSVG FFTW3 GPL license contamination | High | Low | Independent DLL + dynamic linking maintained |
| AI worker process IPC latency | Medium | Medium | Shared memory IPC with sync/async mode support |
| Panel Defect ANN training data shortage | Medium | High | Basic interpolation first (Phase 1), ML after data acquisition (Phase 3) |
| 3072x3072 continuous processing memory pressure | Medium | Medium | MemoryPool pre-allocation + double buffering |
| Calibration data expiry undetected | Medium | Low | `xpe_calib_check_expiry` API with auto-verification at startup |
| Temperature sensor failure | Low | Low | Pipeline fallback: 25C default + alert on sensor non-response |
| DICOM network timeout | Low | Medium | Configurable timeout in C-STORE/C-FIND; retry logic is caller responsibility |

---

## 7. Implementation Roadmap

### 7.1 Phase Dependency Graph

```
SPEC-XPE-P0 (Foundation)         <-- Current Position
    |
    v
SPEC-XPE-P1A (Pre-Processing)    9 SWU, 18 API
    |
    +---> SPEC-XPE-P1B-ENH  (Enhancement)  5 SWU, 7 API    \
    +---> SPEC-XPE-P1B-DISP (Display)      4 SWU, 11 API    |-- Parallel
    +---> SPEC-XPE-P1B-DICOM (DICOM I/O)   4 SWU, 10 API    |
    +---> SPEC-XPE-P1B-GUI  (C# GUI)       2 SWU            /
              |
              v
    SPEC-XPE-P2-ADV  (Advanced)     3 SWU, 4 API
    SPEC-XPE-P2-GSVG (GSVG)        4 SWU, 8 API
              |
              v
    SPEC-XPE-P3-AI   (AI)          4 SWU, 7 API
```

### 7.2 Current Implementation Status

| Component | Status | Detail |
|-----------|:------:|--------|
| CMake build system | DONE | Root + modules/common |
| CMakePresets.json (4 presets) | DONE | Debug/Release/CI/ci-common |
| vcpkg.json SOUP manifest | DONE | spdlog, nlohmann-json, fmt, etc. |
| cmake/ helpers | DONE | CompilerWarnings, Platform, DependencyRules |
| xpe_common.dll | PARTIAL | 5/18 API declared, 2 source files |
| modules/common/ directory | EXISTS | Source files present |
| modules/ remaining 6 dirs | NOT CREATED | Scaffolding pending (P0-09) |
| gsvg/ directory | NOT CREATED | Phase 2 scope |
| gui/ (C# WPF) | NOT CREATED | Scaffolding pending (P0-07) |
| Google Test + CTest | PARTIAL | Test directory exists, framework incomplete |
| CI pipeline | PARTIAL | Basic workflows exist |

### 7.3 Quality Gates per Phase

| Gate | Phase 0 | Phase 1a | Phase 1b | Phase 2 | Phase 3 |
|------|:-------:|:--------:|:--------:|:-------:|:-------:|
| Unit Test Coverage | >= 85% | >= 85% | >= 85% | >= 85% | >= 80% |
| Branch Coverage | >= 70% | >= 70% | >= 70% | >= 70% | >= 60% |
| Static Analysis | 0 warnings | 0 warnings | 0 warnings | 0 warnings | 0 warnings |
| MISRA C:2012 | Pass | Pass | Pass | Pass | Exempt |
| Integration Test | P/Invoke | Pre-proc pipeline | Full P1 | P2 pipeline | P3 pipeline |
| Memory Leak (1000 frames) | Pass | Pass | Pass | Pass | Pass |
| Performance Budget | N/A | < 500ms | < 3000ms | < 2500ms | < 3000ms |
| IEC 62304 Docs Updated | Yes | Yes | Yes | Yes | Yes |

---

## 8. Key Design Decisions

### 8.1 Bypass Policy

9 pre-processing stages have 3 bypass categories:

- **MANDATORY** (M): Cannot bypass. Pipeline hard-fails. (Offset, Gain, CalibManager)
- **CONDITIONAL** (C): Bypass under documented conditions. (Temp, Nonlinearity, Binning, Defect, Ghost)
- **ADVISORY** (A): Non-mutating stage. (Readout Validation)

### 8.2 EI Phase Assignment Resolution

`xpe_calc_exposure_index` is implemented in `xpe_enhance_basic.dll` (Phase 1b) for whole-image EI baseline. In Phase 2, the orchestrator re-calls the same function with collimation ROI for ROI-aware EI refinement. No API signature change needed.

### 8.3 Ghost Correction 3-Tier Escalation

| Tier | Algorithm | Performance | Time Budget |
|------|-----------|------------|:-----------:|
| Tier 1 | LTI multi-exponential (N=4) | 1st frame lag < 0.5% | 150ms |
| Tier 2 | Exposure-weighted LTI | 1st frame lag < 0.35% | 190ms |
| Tier 3 | NLCSC signal-dependent | 1st frame lag <= 0.29% | 240ms |

---

## Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0.0 | 2026-04-14 | MoAI | Initial implementation analysis from SPEC-XPE-MASTER v2.0.0 |

---

*Document End -- XPE-IMPL-ANALYSIS-001 v1.0.0*
