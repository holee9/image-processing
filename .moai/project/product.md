# 제품 개요: X-ray Image Processing Engine (XPE) (v2.1)

**Version**: 2.1.0 | **Updated**: 2026-04-19
**Changes from v2.0**: Ghost Correction Tier 1/2/3 구현 완료, 전처리 파이프라인 통합 (xpe_preprocess_pipeline), NLCSC 알고리즘 고도화, Phase 1a 완료 상태 반영. See `.moai/project/trend-survey-2026.md`.

---

- **이름**: X-ray Image Processing Engine (XPE)
- **저장소**: https://github.com/holee9/image-processing
- **분야**: 의료 기기 소프트웨어 (X-ray Flat Panel Detector)
- **규제**: IEC 62304 Class B/C, FDA 21 CFR 820.30, EU MDR 2017/745, **FDA §524B Cyber Device (2025-06 Final)**, **FDA PCCP AI-DSF (2024-12 Final)**, **FDA AI-DSF Lifecycle (2025-01 Draft)**, **EU Regulation 2024/1689 AI Act (2027-08 전면 발효)**, **ISO/IEC 42001:2023 AIMS**, **IEC 81001-5-1:2021 Secure SW Lifecycle**, **QMSR 2026-02-02 발효**
- **개발 상태**: Phase 1a 완료 (2026-04-19), Phase 1b 및 Phase 2 준비 중

XPE is a modular X-ray flat panel detector image-processing engine. It converts detector-domain raw frames into diagnostic-ready DICOM images while preserving a regulated boundary between detector correction, enhancement, presentation, and optional AI-assisted functions.

The delivery plan is staged:

- **Phase 0**: foundation, common ABI, orchestration, QA scaffolding (now fixture-calibrated native preprocessing via XCal conversion)
- **Phase 1a**: deterministic detector correction
- **Phase 1b**: deterministic enhancement, display, DICOM, whole-image EI
- **Phase 2**: deterministic premium processing and GSVG
- **Phase 3**: assistive AI features with worker isolation

---

## 2. Product Boundary

**Pre-Processing (Raw → Clean Image) — 9개 기술 (PRE-01~09)** | **Phase 1a 완료**

| Research ID | 기술 | 분류 | 구현 상태 |
|-------------|------|------|----------|
| PRE-01 | Readout Artifact Validation | 필수 (HW-only FPGA, SW는 validation) | ✅ 완료 |
| PRE-02 | Offset/Dark Correction | 필수 (SW-first → HW) | ✅ 완료 |
| PRE-03 | Gain/Flat-Field Correction | 필수 (SW-first → HW) | ✅ 완료 |
| PRE-04 | Lag Correction (LTI 기본) | 필수 (SW-only) | ✅ 완료 |
| PRE-04 | Lag Correction (NLCSC 비선형) | 차별화 — 14-50x 업계 우위 | ✅ 완료 (Tier 1/2/3) |
| PRE-05 | Ghost/Gain Ghosting Correction | 필수 (SW-only) | ✅ 완료 |
| PRE-06 | Defective Pixel Correction (기본 보간) | 필수 (SW-first → HW) | ✅ 완료 |
| PRE-06 | Defective Pixel Correction (ML/ViT AE) | 차별화 — 14.2x NMSE 우위 | 🔄 진행 중 |
| PRE-07 | Temperature Compensation | 필수 (SW-first → MCU) | ✅ 완료 |
| PRE-08 | Non-linearity Correction | 필수 (SW-first → HW) | ✅ 완료 |
| PRE-09 | Pixel Binning Correction | 조건부 필수 (형광투시/CBCT 시) | ✅ 완료 |

- `xpe_common.dll`
- `xpe_preprocess.dll`
- `xpe_enhance_basic.dll`
- `xpe_display.dll`
- `xpe_dicom.dll`
- `ImageProcTest.exe`

| Research ID | 기술 | SWU 매핑 | 분류 | 구현 상태 |
|-------------|------|-----------|------|----------|
| SUP-01 | Calibration Parameter Management | SWU-1.5, SWU-5.6 | 필수 | ✅ 완료 |
| SUP-03 | Exposure Index (IEC 62494-1) | SWU-2.10 ExposureIndexCalc | 필수 | ✅ 완료 |
| SUP-04 | DICOM Conformance | SWU-4.1~4.4 | 필수 | ✅ 완료 |
| SUP-05 | Quality Assurance / Constancy Test | SWU-6.1 QaConstancyTest (C#) | 필수 | ✅ 완료 |

**Post-Processing (Clean → Diagnostic-Ready Image)** | **Phase 1b 진행 중**

- Log Transform, Noise Reduction, Contrast Enhancement (CLAHE), Edge Enhancement
- Multiscale Frequency Processing, Collimation Detection, Exposure Index
- Body-Part Recognition (CNN), Bone Suppression (U-Net), DL Denoiser
- Grid Suppression Virtual Grid (GSVG) - 독립 모듈
- DICOM Grayscale Display Pipeline (Modality/VOI/Presentation LUT)

### 2.2 Optional deterministic premium scope

Phase 2 remains optional at deployment time but deterministic in behavior:

- `xpe_enhance_advanced.dll`
- `gsvg.dll`

Phase 2 adds:

- baseline collimation detection,
- ROI-aware EI refinement by re-invoking `xpe_calc_exposure_index`,
- multiscale processing,
- fractional processing,
- grid suppression / virtual grid.

### 2.3 Optional AI assistive scope

Phase 3 is assistive only and must never block baseline image delivery:

- `xpe_ai.dll`
- `xpe_ai_worker.exe`

Phase 3 adds:

- body-part recognition,
- AI collimation refinement,
- image stitching,
- bone suppression,
- DL denoising.

If AI fails, the product shall fall back to deterministic Phase 1/2 output.

---

## 3. Canonical Unit Count

### 3.1 Counting rule

This project uses two unit types:

- **SWU**: XPE software units governed inside the XPE architecture
- **SI**: GSVG software items governed by the independent GSVG package

**Canonical total**: **42 executable units**

- **38 XPE SWU**
- **4 GSVG SI**

The previous `43` total is retired by this revision.

### 3.2 Unit summary

| Category | Count | Notes |
|---|---:|---|
| Common infrastructure | 7 SWU | `xpe_common.dll` |
| Pre-processing | 9 SWU | `xpe_preprocess.dll` |
| Core processing | 12 SWU | `xpe_enhance_basic.dll`, `xpe_enhance_advanced.dll`, `xpe_ai.dll` |
| Display | 4 SWU | `xpe_display.dll` |
| DICOM I/O | 4 SWU | `xpe_dicom.dll` |
| C# orchestration and QA | 2 SWU | `ImageProcTest.exe` |
| GSVG | 4 SI | `gsvg.dll` |
| **Total** | **42 units** | **38 SWU + 4 SI** |

**신규**: 전처리 파이프라인 통합 (xpe_preprocess_pipeline 함수)
- 단일 함수로 전처리 단계 0.5~4 통합 처리
- 형식 변환 체크포인트 자동 관리 (uint16 → float32)
- NLCSC 고스트 보정 Tier 1/2/3 지원

---

## 4. Binary Deliverables

| Binary | Type | Phase | Responsibility |
|---|---|:---:|---|
| `xpe_common.dll` | Native DLL | 0 | ABI types, lifecycle, alerts, logging |
| `xpe_preprocess.dll` | Native DLL | 1a | detector correction and calibration application |
| `xpe_enhance_basic.dll` | Native DLL | 1b | log, noise, contrast, edge, whole-image EI |
| `xpe_display.dll` | Native DLL | 1b | modality/VOI/presentation LUT |
| `xpe_dicom.dll` | Native DLL | 1b | DICOM IO and network SCU |
| `xpe_enhance_advanced.dll` | Native DLL | 2 | collimation baseline, multiscale, fractional |
| `gsvg.dll` | Native DLL | 2 | grid suppression and virtual grid |
| `xpe_ai.dll` | Native DLL | 3 | in-process assistive proxy |
| `xpe_ai_worker.exe` | Native EXE | 3 | sandboxed inference worker |
| `ImageProcTest.exe` | C# WPF EXE | 0+ | orchestration, QA, integration harness |

### 1. X-ray 이미지 처리 파이프라인
- **입력**: Raw FPD 데이터 (DICOM 형식, UINT16/FLOAT32)
- **전처리**: Offset/Dark 보정, Gain 보정, 불량 픽셀 보정 등 (17단계 완료)
- **후처리**: 노이즈 감소, 대비 향상, 윤곽선 강화 등
- **출력**: 진단 가능한 의료 영상 (DICOM 표준 준수)

---

## 5. ImageProcTest Product Role

`ImageProcTest.exe` is the release validation console for the XPE pipeline. Its early implementation may expose health and readiness prominently, but the release-level application shall be workflow-first.

The final application shall support:

- raw/calibration fixture selection with automatic calibration role inference (Offset/Gain/Defect/Reference);
- stage Off/On/Auto control per preprocessing stage;
- fixture-calibrated native preprocessing: raw calibration files are converted to XCal format and loaded into `xpe_preprocess.dll`;
- before/after comparison with swipe, zoom, pan, and pixel inspection;
- quantitative Before/After delta metrics (meanAbsDelta, RMSE, maxAbsDelta, changed pixel ratio);
- JSON/Markdown preprocessing test report generation (schema `xpe-preprocess-gui-test-v1`);
- embedded help/manual content;
- a diagnostics area for DLL readiness, ABI, version, export, fallback, and alert triage.

The diagnostics area is a permanent support feature, not a temporary feature. It shall be demoted from the primary screen once module workflows are mature, but it shall remain available for release validation and field troubleshooting.

The final Test GUI shall not present mock, fallback, identity, or version-only health output as native image-processing output.

### 현재 단계
**Phase 1a 완료** (2026-04-19): 전처리 모듈 및 기반 인프라 구현 완료
- **Phase 0 완료**: 인프라, 공통 라이브러리, 테스트 프레임워크, GUI 프로토타입, CI/CD
- **Phase 1a 완료**: 전처리(PRE-01~09) 및 기반 후처리(POST-01~04, POST-07) 구현
- **Phase 1b 예정**: 고급 후처리(POST-05~06, POST-08~09) 구현
- **목표**: 의료 기기 인증 (IEC 62304 Class B)

### 최근 개발 성과 (2026-04-19)
- **SPEC-XPE-P1A**: 전처리 모듈 (Gain/Offset/Defect Correction) 구현 완료
- **Ghost Correction Tier 1/2/3**: NLCSC 알고리즘 고도화 완료 (14-50x 성능 향상)
- **통합 테스트 GUI**: ImageProcTest WPF GUI로 DLL 모듈 통합 테스트 지원
- **전처리 파이프라인**: xpe_preprocess_pipeline 함수로 단일 통합 처리 지원
- **API 명세**: 완전한 C ABI 규격 83개 함수 구현 (파이프라인 통합으로 +1)

### 로드맵 (v2.1 Updated)

**Phase 1b (진행 예정)**: 고급 후처리 알고리즘 구현
- POST-05 Multiscale Frequency Processing
- POST-06 Fractional Processing
- POST-08 Collimation Detection
- POST-09 Exposure Index Calculation

**Phase 2 (Differentiator)**: AI 기반 고급 알고리즘 도입 (deterministic 중심)
- PRE-04 NLCSC Lag Correction (14-50x 업계 우위) ✅ 완료
- PRE-06 ML Defect Correction (14.2x NMSE) — Phase 3 AI tier와 통합
- POST-05 Multiscale Frequency Processing
- POST-09 Bone Suppression — Phase 3 AI tier로 분류
- POST-11 Virtual Grid (GSVG, 별도 IEC 62304 패키지)

**Phase 4 Research Track (Could, 2027+)**: MedSAM fine-tuning, Federated Learning, GPU production path, Rust safety modules.

## 6. Key Feature Map

| Research / Product ID | Canonical ownership | Delivery rule |
|---|---|---|
| `PRE-01` Readout validation | `SWU-1.9` | advisory only, no pixel mutation |
| `PRE-02/03/06/07/08/09` detector correction | `SWU-1.1~1.8` | deterministic release baseline |
| `PRE-04/05` lag and ghost correction | `SWU-1.4` | deterministic release baseline with tier downgrade |
| `SUP-01` calibration management | `SWU-1.5`, `SWU-5.6` | release baseline |
| `SUP-02` Exposure Index | `SWU-2.10` | one unit reused across Phase 1b baseline and Phase 2 ROI refinement |
| `SUP-04` DICOM conformance | `SWU-4.1~4.4` | release baseline |
| `SUP-05` QA / constancy | `SWU-6.1` | release baseline validation surface |
| `POST-05/07/10/11` advanced deterministic features | `SWU-2.5`, `SWU-2.8`, `SI-001~004` | optional Phase 2 |
| `POST-06/08/09` AI features | `SWU-2.7`, `SWU-2.9`, `SWU-2.11`, `SWU-2.12` | optional Phase 3, assistive only |

---

## 7. Product Rules That Shall Not Change

1. `SWU-2.10` is the only canonical EI unit identifier.
2. EI/DI apply only to detector-domain, single-irradiation images.
3. Body-part recognition and stitching are Phase 3 features, not Phase 2.
4. `XPE_FLAG_*` values are state bits only. Error details go to alerts or diagnostic JSON.
5. GSVG may fail open. When skipped, the pipeline records `XPE_FLAG_GSVG_SKIPPED` and a diagnostic reason, but continues with non-GSVG output.
6. AI modules are assistive. Their failure must not block the deterministic path.
7. `docs/project/` is the only canonical documentation tree for this architecture.
8. `ImageProcTest.exe` shall evolve from diagnostic-first to workflow-first; diagnostics remain available but shall not dominate the release validation flow.

---

## 8. Product Success Criteria

| Area | Minimum release criterion |
|---|---|
| Phase 1a latency | pre-processing completes within 500 ms for 3072x3072 |
| Phase 1 total latency | deterministic baseline completes within 3000 ms |
| Memory discipline | no unbounded frame-to-frame growth in steady-state loops |
| Traceability | every planned SWU/SI maps to owner binary, API contract, and validation evidence |
| Degraded operation | missing Phase 2/3 binaries degrade gracefully with explicit diagnostics |
| Regulatory boundary | release claims stay inside deterministic enhancement and assistive AI boundaries documented in `Regulatory-Feature-Boundary-Matrix.md` |
| Test GUI usability | release validation starts from workflow, metrics, reports, and help; diagnostics are accessible but secondary |
