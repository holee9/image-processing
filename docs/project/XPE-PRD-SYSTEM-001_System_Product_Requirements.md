# System Product Requirements Document

**Document ID**: XPE-PRD-SYSTEM-001  
**Version**: 1.0.0  
**Date**: 2026-04-15  
**Status**: Controlled Draft  
**Classification**: Internal / Execution Baseline  
**Author**: XPE Program Management  
**Approval**: __________________ Date: __________  
**Safety Classification**: **IEC 62304 Class B** (confirmed — see §3.1)  
**Canonical Scope**: `docs/project/`  
**Parent**: XPE-MRD-001 v1.0.0  
**Supersedes for system-level requirements**: `docs/project/product.md` (XPE-PRODUCT-001 v1.2.0)  
**Integrates content from**: `docs/project/product.md`, `docs/post-processing/xpe/XPE-PRD-002_Detailed_Project_Execution_PRD.md`  
**Cross-Verification**: XPE-XVER-CONSOLIDATED-001 v2.0.0 (2026-04-14), CV-002/CV-005 resolved herein  

---

## 1. Executive Summary

XPE(X-ray Image Processing Engine)는 FPD(Flat Panel Detector) 기반 의료 X-ray 시스템에 탑재되는 영상처리 SW 컴포넌트다. Raw 센서 프레임을 진단 가능한 DICOM 영상으로 변환하는 전체 파이프라인을 제공하며, 42개 실행 단위(38 XPE SWU + 4 GSVG SI)로 구성된다.

본 문서는 다음을 규정한다:
1. 무엇을 만들 것인가 (제품 범위, 구성)
2. 어떤 순서로 만들 것인가 (Phase 정의 — normative)
3. 각 Phase가 끝났다고 판단하는 기준 (Release Gate)
4. Safety Class와 지연시간 예산 결정사항 (CV-002, CV-005 해소)

---

## 2. 제품 정의 (Product Definition)

### 2.1 제품 목적

XPE는 FPD 기반 X-ray 시스템에서 다음을 제공한다:
- Detector-domain correction (Pre-processing): Raw → Clean Image
- Enhancement and display processing (Post-processing): Clean → Diagnostic DICOM
- DICOM I/O and network: 표준 DICOM 파일 및 네트워크 SCU
- QA and calibration: 교정 파라미터 관리, 일관성 테스트
- Optional assistive AI (Phase 3): 신체 부위 인식, Bone Suppression (폴백 필수)

### 2.2 제품 경계

XPE는 독립적인 의료기기가 아니다. 의료기기 시스템에 통합되는 SW 컴포넌트(IEC 62304 범위)다.

**XPE의 범위 내:**
- 모든 영상처리 알고리즘 (Pre, Core, Advanced, Display)
- DICOM I/O (파일, 네트워크 SCU)
- 교정 파라미터 관리 (Offset/Gain/BadPixel Map)
- AED 이벤트 처리 인터페이스
- QA / Constancy Test
- ImageProcTest.exe (통합 테스트 GUI)

**XPE의 범위 외:**
- X-ray 발생기 제어
- FPD HW 드라이버 및 FPGA 펌웨어
- PACS / RIS 시스템
- 네트워크 보안 인프라 (방화벽, VPN)
- 방사선 선량 계산 (별도 의료기기 기능)

---

## 3. Safety Class 확정 (CV-002 해소)

### 3.1 Safety Class 결정: **IEC 62304 Class B**

**결정 날짜**: 2026-04-15  
**결정 근거**:

| 평가 항목 | 평가 결과 |
|---------|---------|
| XPE 출력이 환자에게 직접 적용되는가? | 아니오 — 의료 전문가(방사선사/의사)가 검토 후 사용 |
| 단일 SW 고장이 직접 환자 해를 초래하는가? | 아니오 — 이미지 품질 저하이나 진단 불가 시 재촬영 가능 |
| AI 기능(Phase 3)이 자율적 의사 결정을 내리는가? | 아니오 — 보조 도구, 의사가 최종 판단 |
| 위험 제어 기능이 SW에 구현되는가? | 예 (Raw 보존, 오류 경고 등) — Class B 적합 |

**조건부 예외**: Phase 3 AI 기능이 임상 의사 결정에 더 직접적으로 활용될 경우 재평가 필요.

**ACTION REQUIRED (Phase 1a gate 전 필수):**
- [ ] ISO 14971 기반 시스템 Hazard Analysis 수행 및 서명
- [ ] 이 결정이 번복될 경우 Class C 패키지로 업그레이드 (SDP, SRS, SDD, RTM, VVP 전체 개정)
- [ ] XPE-PRD-001(xray-postprocessing-prd.md)의 Class C 표기에 공식 Deprecation Notice 추가

---

## 4. Phase 정의 — Normative (CV-004 해소)

모든 하위 문서(모듈 PRD, SRS, 구현 계획)는 본 섹션의 Phase 정의를 따른다. 이전 문서의 상이한 Phase 정의는 본 문서에 의해 대체된다.

### 4.1 Phase 구조

| Phase | 명칭 | 핵심 목표 | 주요 DLL |
|-------|------|---------|---------|
| Phase 0 | Foundation | 공통 인프라, 테스트 스캐폴딩 | `xpe_common.dll`, `ImageProcTest.exe` |
| Phase 1a | Detector Correction | Deterministic pre-processing | `xpe_preprocess.dll` |
| Phase 1b | Core Pipeline | Enhancement, display, DICOM | `xpe_enhance_basic.dll`, `xpe_display.dll`, `xpe_dicom.dll` |
| Phase 2 | Premium Processing | Advanced enhancement, GSVG | `xpe_enhance_advanced.dll`, `gsvg.dll` |
| Phase 3 | Assistive AI | Body-part, stitching, bone suppression | `xpe_ai.dll`, `xpe_ai_worker.exe` |

### 4.2 Phase별 필수 산출물

**Phase 0:**
- xpe_common.dll: 메모리/스레드 풀, 로거, 설정, AED 인터페이스, 파라미터 검증기
- ImageProcTest.exe: DLL 로드/P/Invoke, 파이프라인 빌더, DICOM 뷰어
- CI 파이프라인: Gitea Actions, Google Test, 커버리지 리포트

**Phase 1a:**
- xpe_preprocess.dll: SWU-1.1~1.9 (Offset, Gain, Defect, Ghost/Lag, Temp, Linearity, Binning, Readout, Calibration Mgr)
- 단위 테스트 커버리지 ≥ 80%
- 통합 테스트 IT-001~IT-007 통과

**Phase 1b:**
- xpe_enhance_basic.dll: SWU-2.1~2.10 (Log, NR, CE, EE, EI, Collimation baseline)
- xpe_display.dll: SWU-3.1~3.3 (Modality LUT, VOI LUT, GSDF)
- xpe_dicom.dll: SWU-4.1~4.4 (Reader, Writer, SCU, GSPS)
- 시스템 테스트 ST-001~ST-PERF-004 통과
- DICOM DVTk Full Pass

**Phase 2:**
- xpe_enhance_advanced.dll: SWU-2.11~2.12 (Multiscale, Fractional, ROI-aware EI)
- gsvg.dll: SI-1~SI-4 (Grid Suppression, Virtual Grid, 독립 IEC 62304)

**Phase 3:**
- xpe_ai.dll + xpe_ai_worker.exe: 신체 부위 인식, 이미지 스티칭, Bone Suppression, DL Denoiser
- AI 폴백 테스트 MP-IT-004 통과
- 임상 검증 완료 (N=50)

---

## 5. 이진 산출물 목록 (Binary Deliverables)

| 바이너리 | 유형 | Phase | SWU/SI 수 | 규제 패키지 |
|--------|------|-------|----------|----------|
| `xpe_common.dll` | Native DLL | 0 | 7 SWU | XPE IEC 62304 |
| `xpe_preprocess.dll` | Native DLL | 1a | 9 SWU | XPE IEC 62304 |
| `xpe_enhance_basic.dll` | Native DLL | 1b | ~10 SWU | XPE IEC 62304 |
| `xpe_display.dll` | Native DLL | 1b | 4 SWU | XPE IEC 62304 |
| `xpe_dicom.dll` | Native DLL | 1b | 4 SWU | XPE IEC 62304 |
| `xpe_enhance_advanced.dll` | Native DLL | 2 | ~4 SWU | XPE IEC 62304 |
| `gsvg.dll` | Native DLL | 2 | 4 SI | GSVG IEC 62304 (독립) |
| `xpe_ai.dll` | Native DLL | 3 | ~2 SWU | XPE IEC 62304 (AI 조항) |
| `xpe_ai_worker.exe` | Native EXE | 3 | ~2 SWU | XPE IEC 62304 (AI 조항) |
| `ImageProcTest.exe` | C# WPF EXE | 0+ | 2 SWU | XPE IEC 62304 |
| **합계** | | | **42 단위** | |

**정규 총계**: **42 실행 단위** (38 XPE SWU + 4 GSVG SI)  
이전 문서의 43 카운트는 본 문서에 의해 42로 정정됨.

---

## 6. 시스템 요구사항 (PR-xxx)

### 6.1 기능 요구사항 (PR-FUNC)

| PR ID | 요구사항 | 우선순위 | 상위 MR | Phase |
|-------|---------|---------|--------|-------|
| PR-FUNC-001 | Dark/Offset 보정: Offset map ≥ 16 dark frames 평균, negative clamp | Must | MR-FUNC-001 | 1a |
| PR-FUNC-002 | Gain 보정: GainMap(x,y) = MeanFlood / [Flood - Offset], SID별 지원 | Must | MR-FUNC-001 | 1a |
| PR-FUNC-003 | 결함 픽셀 보정: Factory map + runtime 갱신, 4/8-neighbor 보간 | Must | MR-FUNC-001 | 1a |
| PR-FUNC-004 | Ghost/Lag 보정: multi-exponential decay [Σ αᵢ×exp(-t/τᵢ)], ≥90% removal | Must | MR-FUNC-001 | 1a |
| PR-FUNC-010 | Log 변환: LogImage = -ln(Corrected/I₀), zero clamp ε=1e-6 | Must | MR-FUNC-002 | 1b |
| PR-FUNC-011 | Noise Reduction: Bilateral filter (기본), NLM (고품질) | Must | MR-FUNC-002 | 1b |
| PR-FUNC-012 | Contrast Enhancement: CLAHE (8×8 block, clip 2.0, 256 bins) | Must | MR-FUNC-002 | 1b |
| PR-FUNC-013 | Edge Enhancement: body-part safe range 내 gain 제한 | Must | MR-FUNC-002 | 1b |
| PR-FUNC-014 | Multiscale Processing: Laplacian pyramid ≥ 8 level | Should | MR-FUNC-008 | 2 |
| PR-FUNC-015 | Fractional Multiscale: density transition artifact 제거 | Should | MR-FUNC-008 | 2 |
| PR-FUNC-016 | Body-Part Recognition: CNN ≥15 카테고리, ≥95% accuracy | Should | MR-FUNC-010 | 3 |
| PR-FUNC-017 | Image Stitching: 2-4 images, 10-30% overlap, Cobb angle ≤2° | Should | MR-FUNC-010 | 3 |
| PR-FUNC-018 | Bone Suppression: DL, PSNR≥33dB, SSIM≥0.97 | Could | MR-FUNC-011 | 3 |
| PR-FUNC-020 | Modality LUT: Rescale Slope/Intercept, DICOM tag (0028,1053)/(0028,1052) | Must | MR-FUNC-003 | 1b |
| PR-FUNC-021 | VOI LUT: W/L preset, LINEAR/SIGMOID/SIGMOID_NORM | Must | MR-FUNC-003 | 1b |
| PR-FUNC-022 | Presentation LUT: GSDF P-value, Δ JND ≤ 1% | Must | MR-FUNC-003 | 1b |
| PR-FUNC-023 | GSPS: DICOM Grayscale Softcopy Presentation State 생성/적용 | Must | MR-FUNC-003 | 1b |
| PR-FUNC-030 | DICOM 파일 읽기/쓰기: FOR PRESENTATION / FOR PROCESSING | Must | MR-FUNC-004 | 1b |
| PR-FUNC-031 | DICOM 네트워크 SCU: C-STORE, C-ECHO | Must | MR-FUNC-004 | 1b |
| PR-FUNC-032 | DICOM Conformance: DVTk Full Pass | Must | MR-COMPAT-001 | 1b |
| PR-FUNC-040 | 교정 파라미터 관리: Offset/Gain/BadPixel Map, SID별 선택 | Must | MR-FUNC-005 | 1a |
| PR-FUNC-041 | EI 계산: IEC 62494-1 기준 EI/DI, ROI-aware (Phase 2) | Must | MR-FUNC-006 | 1b/2 |
| PR-FUNC-042 | AED 이벤트: xpe_aed_register_callback, 노출 시작/종료 통지 | Must | MR-FUNC-007 | 0 |
| PR-FUNC-050 | GSVG: Grid Suppression + Virtual Grid, 독립 IEC 62304 패키지 | Should | MR-FUNC-009 | 2 |

### 6.2 안전 요구사항 (PR-SAFE)

| PR ID | 요구사항 | 우선순위 | 상위 MR | Phase |
|-------|---------|---------|--------|-------|
| PR-SAFE-001 | Raw 이미지 비파괴 보존: 처리 후에도 원본 Raw 데이터 접근 가능 | Must | — | 0 |
| PR-SAFE-002 | 파라미터 검증: 범위 초과 파라미터는 경고 후 거부 | Must | — | 0 |
| PR-SAFE-003 | 결함 보정 실패 경고: 2초 이내 사용자 알림 | Must | — | 1a |
| PR-SAFE-004 | DICOM 태그 무결성: 처리 후 필수 태그 변경 금지 | Must | — | 1b |
| PR-SAFE-008 | AI 처리 표시: AI 출력 이미지에 "AI-processed" 명시 표시 | Must | MR-REG-005 | 3 |
| PR-SAFE-009 | 원본/처리 전환: 100ms 이내 즉시 전환 | Must | — | 1b |
| PR-SAFE-010 | AI 폴백: AI 실패 시 Phase 1/2 결정론적 결과 자동 반환 | Must | MR-FUNC-012 | 3 |

### 6.3 성능 요구사항 (PR-PERF)

| PR ID | 요구사항 | 목표값 | 상위 MR | Phase |
|-------|---------|-------|--------|-------|
| PR-PERF-001 | Pre-processing 시간 | ≤ 500ms (3072×3072) | MR-PERF-001 | 1a |
| PR-PERF-002 | 전체 파이프라인 시간 | ≤ 3초 (3072×3072) | MR-PERF-002 | 1b |
| PR-PERF-003 | W/L 응답 시간 | ≤ 16ms (60fps) | MR-PERF-003 | 1b |
| PR-PERF-004 | 최대 메모리 | ≤ 2GB (4096×4096) | MR-PERF-004 | 1b |
| PR-PERF-005 | 장기 안정성 | 1000회 후 RSS +5% 이하 | MR-PERF-005 | 1b |

### 6.4 호환성 요구사항 (PR-COMPAT)

| PR ID | 요구사항 | 상위 MR |
|-------|---------|--------|
| PR-COMPAT-001 | C ABI(C89 호환) 인터페이스: C/C++/C#/Python 호출 가능 | MR-COMPAT-002 |
| PR-COMPAT-002 | Windows 10/11 64-bit 동작 | MR-COMPAT-003 |

### 6.5 규제 요구사항 (PR-REG)

| PR ID | 요구사항 | 상위 MR |
|-------|---------|--------|
| PR-REG-001 | IEC 62304 Class B 문서 패키지 완비 | MR-REG-001 |
| PR-REG-002 | ISO 14971 SRM 작성 및 유지 | MR-REG-002 |
| PR-REG-003 | FDA 21 CFR 820.30 Design Control 준수 | MR-REG-003 |
| PR-REG-004 | EU MDR 2017/745 Annex I 필수 성능 충족 | MR-REG-004 |

### 6.6 사업 요구사항 (PR-BUSI)

| PR ID | 요구사항 | 상위 MR |
|-------|---------|--------|
| PR-BUSI-001 | 모듈별 라이선스 분리 가능 (DLL 단위) | MR-BUSI-001 |
| PR-BUSI-002 | 교정 파라미터 갱신만으로 신규 FPD 모델 지원 | MR-BUSI-002 |
| PR-BUSI-003 | 규제 문서 패키지 고객 제공 가능 형태 유지 | MR-BUSI-003 |

---

## 7. 지연시간 예산 (CV-005 해소)

다음이 normative 지연시간 예산이다. 이전 문서(Pipeline-spec v1.1.0, Ghost PRD v2)의 충돌하는 예산은 본 섹션에 의해 대체된다.

### 7.1 Phase 1a Pre-processing 예산 (총 500ms, 표준 모드)

| 처리 단계 | 예산 | Ghost Tier | 비고 |
|---------|-----|-----------|-----|
| Offset/Dark Correction | ≤ 80ms | — | |
| Gain/Flat-Field Correction | ≤ 80ms | — | |
| Defective Pixel Correction | ≤ 50ms | — | |
| Ghost/Lag Correction | ≤ 100ms | **Tier 1** (기본 단순 모델) | 기존 Pipeline-spec 150ms에서 조정 |
| Ghost/Lag Correction | ≤ 200ms | **Tier 2** (표준 multi-exponential) | Ghost PRD v2 기준 채택 |
| Temperature Compensation | ≤ 30ms | — | |
| Non-linearity Correction | ≤ 30ms | — | |
| Binning / Readout | ≤ 30ms | — | 조건부 활성화 |
| **Phase 1a 합계 (Tier 1)** | **≤ 500ms** | | **SRS-PERF-001 기준** |
| **Phase 1a 합계 (Tier 2)** | **≤ 600ms** | | **Ghost PRD 기준** |

### 7.2 Tier 3 (NLCSC 비선형 Ghost 보정) — 확장 모드

Tier 3는 사용자가 명시적으로 활성화한 경우에만 적용되며, 예산 초과가 허용된다.

| 처리 단계 | 예산 | 비고 |
|---------|-----|-----|
| Ghost/Lag Correction (NLCSC) | ≤ 400ms | 14-50x 정확도 우위, 처리 시간 연장 허용 |
| **Tier 3 Pre-processing 합계** | **≤ 700ms** | 사용자 명시 활성화 조건 |

### 7.3 Phase 1b 이후 예산

| Phase | 처리 단계 | 예산 | SRS 기준 |
|-------|---------|-----|---------|
| 1b | Core Processing | ≤ 1,500ms | SRS-PERF-002 (전체 3초 내) |
| 1b | Display Pipeline | ≤ 100ms | |
| 1b | DICOM 출력 | ≤ 500ms | |
| 1b | **전체 합계 (Tier 1)** | **≤ 3,000ms** | SRS-PERF-002 |

---

## 8. 하드웨어-소프트웨어 경계

| 기능 | SW 구현 | HW(FPGA) 이관 가능 | 비고 |
|------|--------|-----------------|-----|
| Readout Artifact 검출 | SW 검증만 | FPGA 전담 | HW팀 책임 |
| Offset/Dark Correction | SW-first | FPGA 이관 가능 | 동일 C ABI |
| Gain/Flat-Field | SW-first | FPGA 이관 가능 | 동일 C ABI |
| Defective Pixel (기본) | SW-first | FPGA 이관 가능 | 동일 C ABI |
| Defective Pixel (ML) | SW-only | 이관 불가 | Phase 3 |
| Ghost/Lag (기본/NLCSC) | SW-only | 이관 불가 | 알고리즘 고도화 |
| Temperature Compensation | SW-first | MCU 이관 가능 | |
| Non-linearity Correction | SW-first | FPGA 이관 가능 | |

---

## 9. 고객 인수 기준 (Customer Acceptance Criteria)

### 9.1 Phase 1b 최소 릴리즈 기준 (Mandatory Baseline)

고객에게 제공 가능한 최소 릴리즈는 Phase 1b 완료를 기준으로 한다.

| 기준 | 목표 | 검증 방법 |
|------|------|---------|
| Must-Have 기능 완전성 | PR-FUNC-001~004, 010~013, 020~023, 030~032 모두 동작 | 시스템 테스트 |
| Pre-processing 처리 시간 | ≤ 500ms (Tier 1), ≤ 600ms (Tier 2) | 자동화 성능 테스트 |
| 전체 파이프라인 처리 시간 | ≤ 3초 | 자동화 성능 테스트 |
| DICOM 적합성 | DVTk Full Pass | DVTk 자동 검증 |
| IEC 62304 Class B 문서 | SDP, SRS, SDD, SAD, RTM, VVP, SHA, SOUP, SRM 완비 | 문서 체크리스트 |
| 임상 이미지 품질 | IQ ≥ 3.5/5 (파일럿 N=10) | 방사선사 평가 |
| 단위 테스트 커버리지 | Statement ≥ 80% per SWU | gcov/lcov |
| 메모리 안정성 | 100회 연속 처리 후 RSS +5% 이하 | 자동화 안정성 테스트 |

### 9.2 Phase 3 최종 릴리즈 추가 기준

| 기준 | 목표 | 검증 방법 |
|------|------|---------|
| AI 폴백 기능 | AI 실패 시 Phase 1/2 결과 반환 (100ms 이내) | MP-IT-004 |
| AI 처리 표시 | "AI-processed" 명시 표시 | UI 검증 |
| 임상 검증 완료 | IQ ≥ 3.5/5 (N=50) | 방사선사 독자 평가 |

---

## 10. 모듈 PRD 참조 목록

| 모듈 | PRD 문서 | 범위 |
|------|---------|------|
| XPE 전체 (실행 기준) | `docs/post-processing/xpe/XPE-PRD-002_Detailed_Project_Execution_PRD.md` | Phase 구조, backlog |
| XPE Backlog | `docs/post-processing/xpe/XPE-PRD-003_PRD_Decomposition_and_Backlog.md` | Sprint 분해 |
| Calibration | `docs/calibration/xray-detector-calibration-prd.md` | Offset/Gain/Defect |
| Ghost Correction | `docs/ghost-correction/sw_lag_correction_prd_v2.md` | Lag/Ghost Tier 1~3 |
| Panel Defect | `docs/panel-defect/xray-panel-defect-prd.md` | Defective Pixel |
| Enhance Basic | `docs/enhance-basic/xpe-enhance-basic-prd.md` | Log, NR, CE, EE |
| Enhance Advanced | `docs/enhance-advanced/xpe-enhance-advanced-prd.md` | MFP, Fractional |
| Display | `docs/display/xpe-display-prd.md` | Modality/VOI/GSDF |
| DICOM | `docs/dicom/xpe-dicom-prd.md` | DICOM I/O |
| Common | `docs/common/xpe-common-prd.md` | Infrastructure |
| GSVG | `docs/post-processing/gsvg/GSVG-SDP-001_Development_Plan.md` | Grid Suppression |
| AI Module | `docs/ai-module/xpe-ai-prd.md` | Body-Part, Bone Supp |

---

## 11. 아키텍처 원칙

### 11.1 Anti-Spaghetti 3-Layer

```
Layer 0: xpe_common.dll    — 공통 타입/메모리 (최하위)
Layer 1: Algorithm DLLs    — 상호 의존 금지, Layer 0에만 의존
  ├── xpe_preprocess.dll
  ├── xpe_enhance_basic.dll
  ├── xpe_enhance_advanced.dll
  ├── xpe_display.dll
  ├── xpe_dicom.dll
  └── xpe_ai.dll
Layer 1-G: gsvg.dll         — 독립 IEC 62304, xpe_common 비의존
Layer 2: ImageProcTest.exe  — C# WPF Orchestrator (P/Invoke)
```

### 11.2 핵심 설계 원칙

- C ABI: 모든 DLL은 C89 호환 인터페이스 노출
- Non-destructive: Raw 이미지 원본 보존 필수
- Tier-downgrade: Ghost/Lag는 NLCSC → standard → basic 자동 다운그레이드 지원
- AI fallback: Phase 3 AI 실패 시 Phase 1/2 결과 자동 반환
- Benchmark-first: 성능 우위 주장은 BP-01~BP-10 벤치마크 증거 필수

---

## 12. PR → MR 추적성 테이블

| PR ID | 상위 MR ID | 설명 |
|-------|-----------|------|
| PR-FUNC-001~004 | MR-FUNC-001 | Pre-processing 4종 보정 |
| PR-FUNC-010~013 | MR-FUNC-002 | Core 처리 4종 |
| PR-FUNC-014~015 | MR-FUNC-008 | Multiscale 처리 |
| PR-FUNC-016~017 | MR-FUNC-010 | AI 신체 인식, 스티칭 |
| PR-FUNC-018 | MR-FUNC-011 | Bone Suppression |
| PR-FUNC-020~023 | MR-FUNC-003 | DICOM Display Pipeline |
| PR-FUNC-030~032 | MR-FUNC-004 | DICOM I/O |
| PR-FUNC-040 | MR-FUNC-005 | 교정 파라미터 |
| PR-FUNC-041 | MR-FUNC-006 | EI/DI |
| PR-FUNC-042 | MR-FUNC-007 | AED |
| PR-FUNC-050 | MR-FUNC-009 | GSVG |
| PR-SAFE-008 | MR-REG-005 | AI 처리 표시 |
| PR-SAFE-010 | MR-FUNC-012 | AI 폴백 |
| PR-PERF-001~005 | MR-PERF-001~005 | 성능 요구사항 |
| PR-REG-001~004 | MR-REG-001~004 | 규제 요구사항 |
| PR-COMPAT-001~002 | MR-COMPAT-002~003 | 호환성 요구사항 |
| PR-BUSI-001~003 | MR-BUSI-001~003 | 사업 요구사항 |

---

## 13. 개정 이력

| Rev | 날짜 | 저자 | 설명 |
|-----|------|------|------|
| 1.0.0 | 2026-04-15 | MoAI (SPEC-DOC-001 구현) | 초안 — product.md 통합, CV-002/CV-005 해소, MRD 추적성 추가 |

---

*Document End — XPE-PRD-SYSTEM-001 v1.0.0*
