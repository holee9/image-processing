# SPEC-XPE-MASTER: X-ray Image Processing Engine Master Implementation Plan

**Document ID**: SPEC-XPE-MASTER
**Version**: 2.0.0
**Date**: 2026-04-14
**Status**: Draft — Pending Review
**Author**: MoAI (3-Round Deep Cross-Verification v2.0)
**Classification**: IEC 62304 Class B
**Source**: product.md, tech.md, structure.md, pipeline-spec.md, api-spec.md, xpe-algorithm-spec-deepsync.md, PRD-FPD-CAL-001, Ghost PRD v2, Panel Defect Plan, XPE-SRS-001, GSVG-SRS-001, XPE-SDD-001, xray_fpd_tech_classification_final.md

---

## Changelog (v1.0.0 → v2.0.0)

| Change | Detail |
|--------|--------|
| 12 NEW issues resolved | Deep verification found 3 CRITICAL + 3 MAJOR + 4 MEDIUM + 2 LOW issues |
| §1 upgraded | v2.0 교차검증 결과 전체 반영, Document Update Matrix 추가 |
| §3.5 corrected | Infrastructure SWU = **7** (not 8). SWU-5.7은 §3.7 C# GUI에 분류 |
| §3.8 corrected | Summary 테이블 Infrastructure 행 8→7 수정 |
| §3.9 NEW | EI-0 Phase Assignment Resolution 추가 |
| §4 Phase 0 enhanced | P0-09~P0-12 신규 deliverable 추가 (로깅, AED, 테스트 인프라, 모듈 스캐폴딩) |
| §5 upgraded | Document Update Matrix v2.0으로 확장 (8개 문서) |
| §7 upgraded | Quality Gate Phase 3 면제 사유 추가 |
| §8 upgraded | 3개 부차적 리스크 추가 (총 8개) |

---

## 1. Cross-Verification Summary (v2.0)

3회 독립 병렬 교차검증으로 발견된 문서 간 불일치와 갭을 정리. v1.0.0 대비 12건 신규 이슈 발견.

### 1.1 v1.0.0 Issues Resolution Status

| ID | Issue | Status |
|----|-------|--------|
| C1 | Pipeline 순서 충돌 | **RESOLVED** — Pipeline Spec §1.2 normative |
| C2 | SDD 6개 SWU 누락 | **OPEN** — SDD v1.1 개정 대기 |
| C3 | Ghost PRD v2 독립 API | **RESOLVED** — XPE API normative |
| C4 | Panel Defect Plan 독립 API | **RESOLVED** — Phase 1 basic + Phase 3 AI |
| C5 | 성능 목표 불일치 | **RESOLVED** — Pipeline per-stage normative |
| C6 | 메모리 목표 불일치 | **RESOLVED** — Pipeline phase별 예산 normative |

### 1.2 v2.0 NEW Critical Issues

| ID | Severity | Issue | Resolution |
|----|----------|-------|-----------|
| **N4** | CRITICAL | AED 3개 함수 헤더 미선언 + 미구현 | Phase 0 구현 시 xpe_common_api.h에 선언 추가. api-spec.md v1.2.0에 §5.16~5.18 추가 |
| **N5** | CRITICAL | Logging 3개 함수 헤더 미선언 | Phase 0 구현 시 xpe_common_api.h (또는 xpe_log.h)에 선언 추가 |
| **N11** | CRITICAL | EI-0 (Whole-image EI baseline) Phase 1b에 SWU 미할당 | **본 SPEC §3.9에서 해결**: xpe_calc_exposure_index를 xpe_enhance_basic.dll Phase 1b로 이동. Phase 2에서는 ROI-aware refinement 기능 추가 호출 |

### 1.3 v2.0 NEW Major/Medium Issues

| ID | Severity | Issue | Resolution |
|----|----------|-------|-----------|
| N1 | MAJOR | api-spec.md 함수 카운트 79→82 미갱신 | api-spec.md v1.2.0 개정 |
| N12 | MAJOR | SPEC §3.8 Infrastructure 카운트 8→7 오류 | **본 SPEC §3.8에서 수정** |
| N13 | MAJOR | Quality Gate Phase 3 면제 근거 부족 | **본 SPEC §7에서 면제 사유 추가** |
| N2 | MEDIUM | RTM v1.1 동시 개정 필요 | SDD v1.1과 동시 릴리즈 계획 |
| N3 | MEDIUM | api-spec.md AED 함수 문서 미작성 | api-spec.md v1.2.0 |
| N6 | HIGH | xpe_common_api.h 불완전 | Phase 0 구현에서 해결 |
| N7 | HIGH | 테스트 인프라 비표준 | **Phase 0 deliverable P0-06 강화** |
| N8 | MEDIUM | api-spec.md §4 카운트 테이블 미갱신 | api-spec.md v1.2.0 |
| N9 | MEDIUM | 모듈 디렉토리 스캐폴딩 미완 | **Phase 0 deliverable P0-09 추가** |

### 1.4 Document Update Matrix

| Document | Current | Target | Priority |
|----------|---------|--------|----------|
| **SPEC-XPE-MASTER** | v1.0.0 | **v2.0.0 (본 문서)** | Done |
| **api-spec.md** | v1.3.0 | **v1.3.0** | Done |
| **XPE-SDD-001** | v1.0 | **v1.1** | P2 |
| **XPE-SRS-001** | v1.0 | **v1.1** | P2 |
| **XPE-RTM-001** | v1.0 | **v1.1** | P2 |
| **pipeline-spec.md** | v1.5.0 | **v1.5.0** | Done |
| **product.md** | v1.2.0 | **v1.2.0** | Done |
| **Ghost SRS** | v1.0 | **v1.0.1** | P3 |

---

## 2. Architecture Confirmation

### 2.1 Anti-Spaghetti 3-Layer Design (Confirmed)

```
Layer 0: xpe_common.dll          -- Types, Memory, Config, Logger, Alert, AED
Layer 1: xpe_preprocess.dll      -- PRE-01~09 (Calibration + Correction)
         xpe_enhance_basic.dll   -- POST-01~04, EI baseline (LOG, Noise, Contrast, Edge, EI)
         xpe_enhance_advanced.dll-- POST-05,07 (Multiscale, Collimation, EI ROI refinement)
         xpe_display.dll         -- POST-12 (Modality/VOI/Presentation LUT)
         xpe_dicom.dll           -- SUP-04 (DICOM I/O + Network)
         xpe_ai.dll              -- POST-06,08,09 + PRE-06 ML (AI inference)
Layer 1-G: gsvg.dll              -- POST-10,11 (Grid Suppression / Virtual Grid)
Layer 2: ImageProcTest.exe (C#)  -- SWU-5.7 PipelineOrchestrator, SWU-6.1 QA
```

**v2.0 변경**: xpe_enhance_basic.dll에 EI baseline 기능 추가 (§3.9 해결안 반영).

### 2.2 Canonical Pipeline (Normative)

```
Raw Frame
 -> (0)   CalibManager Load                    [Startup]
 -> (0.5) Readout Artifact Validation (PRE-01) [Phase 1a]
 -> (0.7) Temperature Compensation (PRE-07)    [Phase 1a]
 -> (1)   Offset Correction (PRE-02)           [Phase 1a]
 -> (1.5) Nonlinearity Correction (PRE-08)     [Phase 1a]
 -> (2)   Gain Correction (PRE-03)             [Phase 1a]  <- uint16->float32
 -> (2.5) Binning Correction (PRE-09)          [Phase 1a, conditional]
 -> (3)   Defect Correction (PRE-06)           [Phase 1a]
 -> (4)   Ghost/Lag Correction (PRE-04/05)     [Phase 1a]
 -> (EI-0) Whole-image EI baseline (SUP-03)    [Phase 1b] *RESOLVED: xpe_enhance_basic.dll*
 -> (5)   Log Transform (POST-01)              [Phase 1b]
 -> (5a)  Body Part Recognition (POST-06)      [Phase 3]
 -> (5b)  Collimation Detection (POST-07)      [Phase 2 baseline]
 -> (5c)  AI Collimation Refinement            [Phase 3]
 -> (EI-1) ROI-aware EI refinement             [Phase 2] *xpe_enhance_advanced.dll*
 -> (6)   Noise Reduction (POST-02)            [Phase 1b]
 -> (7)   Contrast Enhancement (POST-03)       [Phase 1b]
 -> (8)   Edge Enhancement (POST-04)           [Phase 1b]
 -> (9)   GSVG (POST-10/11)                    [Phase 2]
 -> (10)  Multiscale Processing (POST-05)      [Phase 2]
 -> (11)  Fractional Processing                [Phase 2]
 -> (12)  Image Stitching (POST-08)            [Phase 3, conditional]
 -> (13)  Bone Suppression (POST-09)           [Phase 3, optional]
 -> (14)  Modality LUT (POST-12a)              [Phase 1b]
 -> (15)  VOI LUT (POST-12b)                   [Phase 1b]
 -> (16)  Presentation LUT (POST-12c)          [Phase 1b] <- float32->uint16
 -> (17)  DICOM Write (SUP-04)                 [Phase 1b]
```

---

## 3. Complete Executable Unit Inventory (42 Units)

### 3.1 SWI-1: Pre-Processing Module (9 SWU) -- xpe_preprocess.dll

| Unit ID | Unit Name | Research ID | API Function(s) | Phase |
|---------|-----------|-------------|-----------------|:-----:|
| SWU-1.1 | OffsetCorrector | PRE-02 | `xpe_offset_correct` | 1a |
| SWU-1.2 | GainCorrector | PRE-03 | `xpe_gain_correct` | 1a |
| SWU-1.3 | DefectPixelCorrector | PRE-06 (basic) | `xpe_defect_correct`, `xpe_defect_detect_runtime` | 1a |
| SWU-1.4 | GhostCorrector | PRE-04, PRE-05 | `xpe_ghost_create/correct/reset/destroy` | 1a |
| SWU-1.5 | CalibrationManager | SUP-01 | `xpe_calib_*` (6 functions) | 1a |
| SWU-1.6 | TempCompensator | PRE-07 | `xpe_temp_compensate` | 1a |
| SWU-1.7 | NonlinearityCorrector | PRE-08 | `xpe_nonlinearity_correct` | 1a |
| SWU-1.8 | BinningCorrector | PRE-09 | `xpe_binning_correct` | 1a |
| SWU-1.9 | ReadoutArtifactValidator | PRE-01 | `xpe_validate_readout_artifact` | 1a |

### 3.2 SWI-2: Core Processing Module (12 SWU)

| Unit ID | Unit Name | Research ID | API Function(s) | DLL | Phase |
|---------|-----------|-------------|-----------------|-----|:-----:|
| SWU-2.1 | LogTransform | POST-01 | `xpe_log_transform`, `xpe_log_inverse` | enhance_basic | 1b |
| SWU-2.2 | NoiseReducer | POST-02 (basic) | `xpe_noise_reduce`, `xpe_noise_estimate_sigma` | enhance_basic | 1b |
| SWU-2.3 | ContrastEnhancer | POST-03 (basic) | `xpe_contrast_enhance` | enhance_basic | 1b |
| SWU-2.4 | EdgeEnhancer | POST-04 | `xpe_edge_enhance` | enhance_basic | 1b |
| SWU-2.5 | MultiscaleProcessor | POST-05 | `xpe_multiscale_process` | enhance_advanced | 2 |
| SWU-2.6 | FractionalProcessor | -- | `xpe_fractional_process` | enhance_advanced | 2 |
| SWU-2.7 | BodyPartRecognizer | POST-06 | `xpe_bodypart_recognize` | ai | 3 |
| SWU-2.8 | CollimationDetector | POST-07 | `xpe_detect_collimation` | enhance_advanced | 2 |
| SWU-2.9 | ImageStitcher | POST-08 | `xpe_stitch_images`, `xpe_stitch_estimate_size` | ai | 3 |
| **SWU-2.10** | **ExposureIndexCalc** | **SUP-03** | **`xpe_calc_exposure_index`** | **enhance_basic (Phase 1b) + enhance_advanced (Phase 2 ROI refinement)** | **1b/2** |
| SWU-2.11 | BoneSuppressionEngine | POST-09 | `xpe_bone_suppress` | ai | 3 |
| SWU-2.12 | DLDenoiser | POST-02 (DL) | `xpe_dl_denoise` | ai | 3 |

**v2.0 변경**: SWU-2.10 ExposureIndexCalc의 Phase를 "2" → "**1b/2**"로 변경. xpe_calc_exposure_index 함수는 xpe_enhance_basic.dll에서 Phase 1b 시점에 whole-image EI baseline을 제공. Phase 2에서 xpe_enhance_advanced.dll이 ROI-aware refinement를 수행할 때 동일 함수를 collimation ROI와 함께 재호출.

### 3.3 SWI-3: Display Processing Module (4 SWU) -- xpe_display.dll

| Unit ID | Unit Name | API Function(s) | Phase |
|---------|-----------|-----------------|:-----:|
| SWU-3.1 | ModalityLUT | `xpe_modality_lut_apply` | 1b |
| SWU-3.2 | VoiLUT | `xpe_voi_lut_apply/fast/sequence` | 1b |
| SWU-3.3 | PresentationLUT | `xpe_presentation_lut_apply/check_display` | 1b |
| SWU-3.4 | LUTManager | `xpe_lut_get_preset*/add/remove/auto_select` | 1b |

### 3.4 SWI-4: DICOM I/O Module (4 SWU) -- xpe_dicom.dll

| Unit ID | Unit Name | API Function(s) | Phase |
|---------|-----------|-----------------|:-----:|
| SWU-4.1 | DicomReader | `xpe_dicom_read/query_dimensions/read_tag_string` | 1b |
| SWU-4.2 | DicomWriter | `xpe_dicom_write/write_j2k/set_tag_string` | 1b |
| SWU-4.3 | PresentationStateIO | `xpe_gsps_create/apply` | 1b |
| SWU-4.4 | DicomNetworkSCU | `xpe_dicom_cstore/cfind_mwl` | 1b |

### 3.5 SWI-5: Common Infrastructure (7 SWU) -- xpe_common.dll

| Unit ID | Unit Name | API Function(s) | Phase |
|---------|-----------|-----------------|:-----:|
| SWU-5.1 | MemoryPool | `xpe_alloc_image/free_image/copy_image` | 0 |
| SWU-5.2 | ThreadPool | (internal) | 0 |
| SWU-5.3 | ErrorHandler | `xpe_error_string` | 0 |
| SWU-5.4 | Logger | `xpe_log_set_level/set_file/flush` | 0 |
| SWU-5.5 | ParameterValidator | `xpe_get_param_range` | 0 |
| SWU-5.6 | ConfigManager | `xpe_init/shutdown/version/configure` | 0 |
| SWU-5.8 | AedEventInterface | `xpe_aed_configure/poll_event/get_status` | 0 |

**v2.0 수정**: Infrastructure SWU = **7** (SWU-5.1~5.6 + SWU-5.8). SWU-5.7 PipelineOrchestrator는 C# Layer 2 (§3.7).

### 3.6 SWI-6: GSVG Module (4 SI) -- gsvg.dll (independent)

| Unit ID | Unit Name | API Function(s) | Phase |
|---------|-----------|-----------------|:-----:|
| SI-001 | GridDetector | `gsvg_detect_grid` | 2 |
| SI-002 | GridSuppressor | `gsvg_suppress_grid`, `gsvg_process/process_ex` | 2 |
| SI-003 | VirtualGridGenerator | `gsvg_virtual_grid` | 2 |
| SI-004 | ScatterLUTManager | `gsvg_load_scatter_lut`, `gsvg_version/error_string` | 2 |

### 3.7 Layer 2: C# GUI (2 SWU)

| Unit ID | Unit Name | Implementation | Phase |
|---------|-----------|---------------|:-----:|
| SWU-5.7 | PipelineOrchestrator | C# WPF, P/Invoke | 0+ |
| SWU-6.1 | QaConstancyTest | C# WPF | 1b+ |

### 3.8 Summary (v2.0 CORRECTED)

| Category | SWU Count | Phase 0 | Phase 1a | Phase 1b | Phase 2 | Phase 3 |
|----------|:---------:|:-------:|:--------:|:--------:|:-------:|:-------:|
| Pre-Processing | 9 | -- | **9** | -- | -- | -- |
| Core Processing | 12 | -- | -- | **5** | 3 | 4 |
| Display | 4 | -- | -- | **4** | -- | -- |
| DICOM I/O | 4 | -- | -- | **4** | -- | -- |
| Infrastructure | **7** | **7** | -- | -- | -- | -- |
| GSVG | 4 | -- | -- | -- | **4** | -- |
| C# GUI | 2 | 1 | -- | 1 | -- | -- |
| **Total** | **42** | **8** | **9** | **14** | **7** | **4** |

**v2.0 변경사항**:
- Infrastructure: 8 → **7** (SWU-5.7은 C# GUI에 분류)
- Phase 0: 9 → **8** (Infrastructure 7 + C# GUI 1)
- Phase 1b Core Processing: 4 → **5** (SWU-2.10 EI baseline 추가)
- Phase 2 Core Processing: 4 → **3** (SWU-2.10 Phase 1b로 이동, Collimation/Multiscale/Fractional 유지)

### 3.9 EI-0 Phase Assignment Resolution (NEW in v2.0)

**Problem**: Pipeline stage (EI-0) "Whole-image EI baseline"은 Phase 1b에 실행되지만, SWU-2.10 ExposureIndexCalc은 Phase 2 (xpe_enhance_advanced.dll)에만 할당.

**Resolution**: `xpe_calc_exposure_index` 함수를 **xpe_enhance_basic.dll** (Phase 1b)에 구현.

- **Phase 1b**: xpe_enhance_basic.dll에서 `xpe_calc_exposure_index(img, meta, &ei, &di)` 호출. Whole-image EI baseline 제공. `meta->bodyPart`를 사용하여 EIT 선택.
- **Phase 2**: xpe_enhance_advanced.dll이 collimation ROI 확보 후, 동일 함수를 ROI-cropped 영상으로 재호출하여 ROI-aware EI refinement 수행.
- **DLL 배치**: `xpe_calc_exposure_index`는 xpe_enhance_basic.dll에서 export. xpe_enhance_advanced.dll은 별도 wrapper 없이 orchestrator(C#)가 ROI와 함께 재호출.
- **API 변경 없음**: 기존 `xpe_calc_exposure_index` 시그니처 유지. Caller가 전체 이미지 또는 ROI-cropped 이미지를 전달하여 baseline/refinement 구분.

**결과**: xpe_enhance_basic.dll의 API 수 = 6 → **7** (EI 함수 1개 추가).

---

## 4. Implementation Phase Plan

### Phase 0: Foundation (xpe_common.dll + Build Infrastructure)

**Goal**: 빌드 시스템 구축, 공통 라이브러리 완성, C# GUI 스캐폴딩

**Deliverables**:

| ID | Deliverable | SWU | Priority |
|----|------------|-----|----------|
| P0-01 | CMake 빌드 시스템 (root + modules/ + gsvg/ + tests/) | -- | Must |
| P0-02 | CMakePresets.json (Debug/Release/CI) | -- | Must |
| P0-03 | vcpkg.json SOUP 매니페스트 | -- | Must |
| P0-04 | cmake/ 헬퍼 (CompilerWarnings, Platform/AVX2, DependencyRules) | -- | Must |
| P0-05 | xpe_common.dll 완전 구현 (**18 API**: 기존 15 + AED 3) | SWU-5.1~5.6, 5.8 | Must |
| P0-06 | **Google Test 프레임워크 설정 + CTest 통합 + 커버리지 리포팅** | -- | Must |
| P0-07 | ImageProcTest C# WPF 프로젝트 스캐폴딩 | SWU-5.7 | Must |
| P0-08 | CI 파이프라인 (CMake build + CTest + coverage) | -- | Should |
| **P0-09** | **모든 모듈 디렉토리 스캐폴딩** (preprocess/, enhance_basic/, enhance_advanced/, ai/, display/, dicom/, gsvg/) | -- | **Must** |
| **P0-10** | **xpe_common_api.h 통합 헤더 구조 확립** (모든 18 API 선언 또는 sub-header include) | -- | **Must** |
| **P0-11** | **Logging 서브시스템 구현** (xpe_log_set_level/set_file/flush) | SWU-5.4 | **Must** |
| **P0-12** | **AED 서브시스템 구현** (xpe_aed_configure/poll_event/get_status) | SWU-5.8 | **Must** |

**Acceptance Criteria**:
- [ ] `cmake --preset release && cmake --build --preset release` 성공
- [ ] xpe_common.dll의 **18개** 함수 모두 export 확인 (dumpbin /exports)
- [ ] P/Invoke 호환성 테스트 통과 (C# <-> C ABI)
- [ ] **Google Test + CTest로 자동화된** unit test coverage >= 85% (xpe_common)
- [ ] ImageProcTest.exe 실행 + xpe_common.dll 로드 확인
- [ ] **모든 9개 모듈 디렉토리 존재** + 빈 CMakeLists.txt 생성
- [ ] **Logging 함수 3개 동작 확인** (파일 출력 + 레벨 필터링)
- [ ] **AED 함수 3개 동작 확인** (configure + poll + status)

### Phase 1a: Pre-Processing (xpe_preprocess.dll)

**Goal**: Raw -> Clean Image 전처리 파이프라인 완성

**Deliverables**:

| ID | Deliverable | SWU | Research ID | Priority |
|----|------------|-----|-------------|----------|
| P1a-01 | Offset Correction | SWU-1.1 | PRE-02 | Must |
| P1a-02 | Gain Correction (+ uint16->float32 변환) | SWU-1.2 | PRE-03 | Must |
| P1a-03 | Defect Pixel Correction (기본 보간) | SWU-1.3 | PRE-06 basic | Must |
| P1a-04 | Ghost/Lag Correction (Tier 1 LTI) | SWU-1.4 | PRE-04 LTI | Must |
| P1a-05 | Ghost/Lag Correction (Tier 2/3 NLCSC) | SWU-1.4 | PRE-04 NLCSC | Should |
| P1a-06 | Calibration Manager (load/save/validate/expiry) | SWU-1.5 | SUP-01 | Must |
| P1a-07 | Temperature Compensator | SWU-1.6 | PRE-07 | Must |
| P1a-08 | Nonlinearity Corrector | SWU-1.7 | PRE-08 | Must |
| P1a-09 | Binning Corrector (conditional) | SWU-1.8 | PRE-09 | Should |
| P1a-10 | Readout Artifact Validator | SWU-1.9 | PRE-01 | Must |

**Acceptance Criteria**:
- [ ] xpe_preprocess.dll의 18개 함수 모두 export 확인
- [ ] Offset/Gain/Defect unit test 통과
- [ ] Ghost Tier 1+2 처리 시간 < 150ms (3072x3072)
- [ ] 전처리 전체 < 500ms (Pipeline Spec §5.1)
- [ ] Unit test coverage >= 85%
- [ ] Calibration CRC 검증 동작
- [ ] P/Invoke 통합 테스트 (C# <-> xpe_preprocess)

### Phase 1b: Basic Enhancement + Display + DICOM + EI Baseline

**Goal**: Clean Image -> Diagnostic-Ready Image (기본 후처리 + 디스플레이 + DICOM I/O + EI baseline)

**Deliverables**:

| ID | Deliverable | SWU | Research ID | Priority |
|----|------------|-----|-------------|----------|
| P1b-01 | Log Transform / Inverse | SWU-2.1 | POST-01 | Must |
| P1b-02 | Noise Reduction (Bilateral + NLM) | SWU-2.2 | POST-02 basic | Must |
| P1b-03 | Contrast Enhancement (CLAHE) | SWU-2.3 | POST-03 basic | Must |
| P1b-04 | Edge Enhancement (USM) | SWU-2.4 | POST-04 | Must |
| **P1b-04a** | **Exposure Index Baseline (whole-image EI/DI)** | **SWU-2.10** | **SUP-03** | **Must** |
| P1b-05 | Modality LUT | SWU-3.1 | POST-12a | Must |
| P1b-06 | VOI LUT (Linear/Sigmoid + presets) | SWU-3.2 | POST-12b | Must |
| P1b-07 | Presentation LUT / GSDF | SWU-3.3 | POST-12c | Must |
| P1b-08 | LUT Manager (preset CRUD + auto-select) | SWU-3.4 | POST-12 | Must |
| P1b-09 | DICOM Reader | SWU-4.1 | SUP-04 | Must |
| P1b-10 | DICOM Writer (+ J2K) | SWU-4.2 | SUP-04 | Must |
| P1b-11 | GSPS (Presentation State) | SWU-4.3 | SUP-04 | Should |
| P1b-12 | DICOM Network SCU (C-STORE/C-FIND) | SWU-4.4 | SUP-04 | Should |
| P1b-13 | QA Constancy Test (C#) | SWU-6.1 | SUP-05 | Should |

**v2.0 변경**: P1b-04a (EI Baseline) 추가. xpe_enhance_basic.dll 함수 수 = 6 -> **7**.

**Acceptance Criteria**:
- [ ] Phase 1 전체 파이프라인 < 3000ms (Pipeline Spec §5.1)
- [ ] VOI LUT interactive latency <= 16ms
- [ ] DICOM DX IOD 읽기/쓰기 검증
- [ ] GSDF compliance 확인 (xpe_presentation_lut_check_display)
- [ ] Phase 1 peak memory <= 190MB
- [ ] **EI/DI 계산: IEC 62494-1 준수 (detector-domain, single-irradiation only)**
- [ ] 통합 테스트: Raw DICOM -> 전처리 -> 후처리 -> EI -> Display -> DICOM Write 파이프라인

### Phase 2: Advanced Enhancement + GSVG

**Goal**: 차별화 기술 (Multiscale, Collimation, GSVG, EI ROI refinement)

**Deliverables**:

| ID | Deliverable | SWU/SI | Research ID | Priority |
|----|------------|--------|-------------|----------|
| P2-01 | Multiscale Frequency Processing | SWU-2.5 | POST-05 | Should |
| P2-02 | Fractional Processing | SWU-2.6 | -- | Should |
| P2-03 | Collimation Detection (Gradient+Hough) | SWU-2.8 | POST-07 baseline | Must |
| **P2-04** | **Exposure Index ROI Refinement** | **SWU-2.10 (재호출)** | **SUP-03** | **Must** |
| P2-05 | Grid Suppression (DWT-based) | SI-001, SI-002 | POST-10 | Must (grid 사용 시) |
| P2-06 | Virtual Grid (scatter correction) | SI-003, SI-004 | POST-11 | Should |

**v2.0 변경**: P2-04는 SWU-2.10의 ROI-aware refinement로 재정의. Orchestrator가 collimation ROI와 함께 xpe_calc_exposure_index를 재호출.

**Acceptance Criteria**:
- [ ] GSVG: Grid artifact 시각적 인지 불가, MTF 저하 <= 5%
- [ ] Virtual Grid: CNR >= 90% (동일 조건 6:1 physical grid 대비)
- [ ] **EI ROI refinement: Collimation ROI 기반 DI 재계산 동작**
- [ ] EI/DI: IEC 62494-1 준수 (single-irradiation only)
- [ ] Phase 2 total <= 2500ms
- [ ] Phase 2 peak memory <= 440MB

### Phase 3: AI / Intelligence

**Goal**: DL 기반 고급 기능 (Body Part, Stitching, Bone Suppression, DL Denoise)

**Deliverables**:

| ID | Deliverable | SWU | Research ID | Priority |
|----|------------|-----|-------------|----------|
| P3-01 | AI Worker Process (xpe_ai_worker.exe) | -- | -- | Must |
| P3-02 | Body Part Recognition (MobileNet-v3) | SWU-2.7 | POST-06 | Should |
| P3-03 | AI Collimation Refinement | SWU-2.8 ext | POST-07 AI | Could |
| P3-04 | Image Stitching (phase correlation + AI) | SWU-2.9 | POST-08 | Should |
| P3-05 | Bone Suppression (Residual U-Net) | SWU-2.11 | POST-09 | Could |
| P3-06 | DL Denoiser (DnCNN) | SWU-2.12 | POST-02 DL | Could |
| P3-07 | Defect Correction ML (ANN/ViT AE) | SWU-1.3 ext | PRE-06 ML | Could |

**Acceptance Criteria**:
- [ ] AI worker 프로세스 격리 (crash isolation)
- [ ] Body Part >= 15 categories, >= 95% accuracy
- [ ] Bone Suppression PSNR >= 33dB, SSIM >= 0.97
- [ ] AI 미가용 시 deterministic fallback 경로 정상 동작
- [ ] Phase 3 total <= 3000ms
- [ ] Phase 3 peak memory <= 740MB

---

## 5. Document Upgrade Actions (v2.0 Extended)

| Document | Current | Required Action | Priority |
|----------|---------|----------------|----------|
| **api-spec.md** | v1.3.0 | **DONE**: API count = 82, AED functions documented, EI function owned by `xpe_enhance_basic.dll` | **Done** |
| **XPE-SDD-001** | v1.0 | **v1.1**: SWU-1.6~1.9, SWU-5.8 추가 (5개 SWU). SWU-6.1은 별도 GUI SPEC | P2 |
| **XPE-SRS-001** | v1.0 | **v1.1**: SRS-AED-001~003 요구사항 추가. SRS-EI-001에 Phase 1b baseline 명시 | P2 |
| **XPE-RTM-001** | v1.0 | **v1.1**: SWU-1.6~1.9, SWU-5.8 추적 행 추가 (SDD v1.1과 동시 릴리즈) | P2 |
| **pipeline-spec.md** | v1.5.0 | **DONE**: EI baseline stage restored, AED gate added, Phase 2/3 ownership normalized | Done |
| **product.md** | v1.2.0 | **DONE**: canonical total fixed to 42 executable units, binary boundary clarified | Done |
| **structure.md** | v1.2.0 | **DONE**: module-to-binary table normalized to 42 executable units and 82 native APIs | Done |
| **Ghost SRS** | v1.0 | **v1.0.1**: FR-701에 "XPE 전체 파이프라인 순서와의 관계" 주석 추가 | P3 |

---

## 6. Implementation SPEC Breakdown (Sub-SPECs)

| SPEC ID | Scope | Phase | DLL | SWU Count | API Count |
|---------|-------|-------|-----|:---------:|:---------:|
| SPEC-XPE-P0 | Foundation + Build + Common | 0 | xpe_common | 7+1(C#) | **18** |
| SPEC-XPE-P1A | Pre-Processing | 1a | xpe_preprocess | 9 | 18 |
| SPEC-XPE-P1B-ENH | Basic Enhancement + EI Baseline | 1b | xpe_enhance_basic | **5** | **7** |
| SPEC-XPE-P1B-DISP | Display Processing | 1b | xpe_display | 4 | 11 |
| SPEC-XPE-P1B-DICOM | DICOM I/O | 1b | xpe_dicom | 4 | 10 |
| SPEC-XPE-P1B-GUI | C# GUI Orchestrator | 1b | ImageProcTest | 1(+1 QA) | N/A |
| SPEC-XPE-P2-ADV | Advanced Enhancement + EI ROI | 2 | xpe_enhance_advanced | **3** | **3** |
| SPEC-XPE-P2-GSVG | Grid Suppression / Virtual Grid | 2 | gsvg | 4 | 8 |
| SPEC-XPE-P3-AI | AI Inference | 3 | xpe_ai + worker | 4 | 7 |

**v2.0 변경**:
- P0: 18 API (기존 15 + AED 3)
- P1B-ENH: SWU 4 -> **5** (EI 추가), API 6 -> **7**
- P2-ADV: SWU 4 -> **3** (EI가 P1B로 이동)

**API Total**: 18+18+7+11+10+3+8+7 = **82**

> Note: v1.0.0 total was 79 (pre-AED, pre-EI-move). v2.0.0 adds +3 AED functions to common and moves xpe_calc_exposure_index from enhance_advanced to enhance_basic (net 0 for total). Total = 79 + 3 = 82.

**Execution Order** (의존성 기반):

```
SPEC-XPE-P0  -->  SPEC-XPE-P1A  -->  SPEC-XPE-P1B-ENH  -->  SPEC-XPE-P2-ADV
                                 -->  SPEC-XPE-P1B-DISP      SPEC-XPE-P2-GSVG
                                 -->  SPEC-XPE-P1B-DICOM
                                 -->  SPEC-XPE-P1B-GUI  -->  SPEC-XPE-P3-AI
```

Phase 1b의 4개 Sub-SPEC은 병렬 실행 가능 (xpe_common.dll에만 의존).

---

## 7. Quality Gates per Phase (v2.0 Enhanced)

| Gate | Phase 0 | Phase 1a | Phase 1b | Phase 2 | Phase 3 |
|------|---------|----------|----------|---------|---------|
| Unit Test Coverage | >= 85% | >= 85% | >= 85% | >= 85% | >= 80% |
| Branch Coverage | >= 70% | >= 70% | >= 70% | >= 70% | >= 60% (1) |
| Static Analysis | 0 warnings (2) | 0 warnings | 0 warnings | 0 warnings | 0 warnings |
| MISRA C:2012 Advisory | Pass | Pass | Pass | Pass | Exempt (3) |
| Integration Test | P/Invoke | Pre-proc pipeline | Full P1 pipeline | P2 pipeline | P3 pipeline |
| Memory Leak (1000 frames) | Pass | Pass | Pass | Pass | Pass |
| Performance Budget | N/A | < 500ms | < 3000ms | < 2500ms | < 3000ms |
| IEC 62304 Docs Updated | Yes | Yes | Yes | Yes | Yes |

**v2.0 면제 사유 (NEW)**:

(1) **Phase 3 Branch Coverage 60%**: AI 모듈(xpe_ai.dll)은 ONNX Runtime 내부 코드의 branch coverage 측정이 불가능. xpe_ai.dll 자체 코드(IPC proxy, error handling)는 >= 70% 유지하되, ONNX inference 경로는 contract test + golden-data regression test로 대체.

(2) **Static Analysis 도구**: cppcheck `--std=c++17` 모드 사용. 추가로 clang-tidy (modernize-*, performance-*, bugprone-*) 병행 권장. Phase 3 AI 코드는 clang-tidy를 primary로 사용.

(3) **Phase 3 MISRA C:2012 면제**: xpe_ai.dll은 ONNX Runtime C++ API를 사용하므로 MISRA C:2012 적용 불가. 대체 품질 게이트: (a) clang-tidy 0 warnings, (b) ONNX contract test 100% pass, (c) Worker crash recovery test pass, (d) Deterministic fallback path 100% test coverage.

---

## 8. Risk Summary (v2.0 Extended)

| Risk | Impact | Likelihood | Mitigation |
|------|--------|-----------|------------|
| Ghost PRD v2의 fixed-point 알고리즘이 XPE float32 파이프라인과 불일치 | High | Medium | Phase 1a에서 float32 구현 후 fixed-point 최적화는 FPGA 이관 시 수행 |
| GSVG FFTW3 GPL 라이선스 오염 | High | Low | Dynamic linking 유지, GSVG를 독립 DLL로 분리 (현재 설계 유지) |
| AI worker 프로세스 IPC 지연 | Medium | Medium | Phase 3에서 shared memory IPC 채택, 동기/비동기 모드 지원 |
| Panel Defect ANN 모델의 학습 데이터 부족 | Medium | High | Phase 1에서 기본 보간 우선 구현, Phase 3에서 데이터 확보 후 ML 도입 |
| 3072x3072 이미지 연속 처리 시 메모리 압박 | Medium | Medium | MemoryPool (SWU-5.1) 사전 할당 + double buffering 설계 |
| **캘리브레이션 데이터 만료 미감지** (NEW) | Medium | Low | xpe_calib_check_expiry API로 만료 시점 사전 감지. CalibrationManager가 startup 시 자동 검증 |
| **온도 센서 장애 시 보정 불가** (NEW) | Low | Low | Pipeline stage (0.7) fallback: 센서 미응답 시 25C 기본값 사용 + alert 발생 |
| **DICOM 네트워크 타임아웃** (NEW) | Low | Medium | xpe_dicom_cstore/cfind_mwl에 configurable timeout. XPE_ERR_NETWORK_FAILED 반환 시 재시도 로직은 caller 책임 |

---

## Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0.0 | 2026-04-14 | MoAI | Initial release -- 3-round cross-verification |
| **2.0.0** | **2026-04-14** | **MoAI** | **Deep verification v2.0: 12 new issues resolved. EI-0 Phase 1b assignment. Infrastructure SWU corrected (8->7). Quality Gate justifications. Extended risks.** |

---

*Document End -- SPEC-XPE-MASTER v2.0.0*
