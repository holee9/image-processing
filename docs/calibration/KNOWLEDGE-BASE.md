# Calibration Knowledge Base

**모듈**: `xpe_preprocess.dll` (Layer 1, Phase 1a)  
**버전**: 1.0  
**날짜**: 2026-04-19  
**목적**: 캘리브레이션 관련 모든 문서, 소스 코드, 사양을 연결하는 중앙 지식 허브

---

## 목차

1. [알고리즘별 지식 맵](#1-알고리즘별-지식-맵)
2. [문서 유형별 인덱스](#2-문서-유형별-인덱스)
3. [역할별 탐색 가이드](#3-역할별-탐색-가이드)
4. [소스 코드 ↔ 문서 매핑](#4-소스-코드--문서-매핑)
5. [교차 모듈 의존 관계](#5-교차-모듈-의존-관계)
6. [수학적 모델 레퍼런스](#6-수학적-모델-레퍼런스)
7. [에러 코드 레퍼런스](#7-에러-코드-레퍼런스)
8. [API 레퍼런스 인덱스](#8-api-레퍼런스-인덱스)
9. [테스트 커버리지 맵](#9-테스트-커버리지-맵)
10. [알려진 제약 및 미구현 항목](#10-알려진-제약-및-미구현-항목)

---

## 1. 알고리즘별 지식 맵

### 1.1 오프셋(암전류) 보정 — SWU-1.1

| 항목 | 내용 |
|------|------|
| **목적** | 검출기 픽셀의 열적 암전류(dark current) 제거 |
| **수식** | `I_corr = I_raw - I_dark(T, t_prep)` |
| **PRD 요건** | [xray-detector-calibration-prd.md §3.1](xray-detector-calibration-prd.md) |
| **SRS 요건** | [SRS-CALIB-001 §2.2 SRS-CALIB-FUNC-004](SRS-CALIB-001_Software_Requirements_Specification.md) |
| **SAD 설계** | [SAD-CALIB-001 §4.1 SWU-1.1](SAD-CALIB-001_Software_Architecture_Document.md) |
| **개념도** | [CONCEPT-DIAGRAMS.md §4](CONCEPT-DIAGRAMS.md#4-오프셋암전류-보정) |
| **구현** | `modules/preprocess/src/xpe_offset.cpp` (253줄) |
| **테스트** | `modules/preprocess/tests/test_offset_correct.cpp` |
| **취득 절차** | [IAP-CALIB-001 §3.1 Dark Frame 취득](IAP-CALIB-001_Image_Acquisition_Protocol.md) |
| **위험 분석** | [SHA-CALIB-001 H-001](SHA-CALIB-001_Software_Hazard_Analysis.md) |

### 1.2 게인(평탄화) 보정 — SWU-1.2

| 항목 | 내용 |
|------|------|
| **목적** | 픽셀 감도 불균일성(FPN) 정규화, uint16→float32 변환 |
| **수식** | `I_norm = I_corr / G(x,y)`, 다중 SID: `G(E) = Σ cₖEᵏ` |
| **PRD 요건** | [xray-detector-calibration-prd.md §3.2](xray-detector-calibration-prd.md) |
| **SRS 요건** | [SRS-CALIB-001 §2.2 SRS-CALIB-FUNC-005](SRS-CALIB-001_Software_Requirements_Specification.md) |
| **SAD 설계** | [SAD-CALIB-001 §4.2 SWU-1.2](SAD-CALIB-001_Software_Architecture_Document.md) |
| **개념도** | [CONCEPT-DIAGRAMS.md §5](CONCEPT-DIAGRAMS.md#5-게인평탄화-보정) |
| **구현** | `modules/preprocess/src/xpe_gain.cpp` (229줄) |
| **테스트** | `modules/preprocess/tests/test_gain_correct.cpp` |
| **형식 경계** | **이 단계에서 uint16 → float32 변환** (파이프라인 핵심 전환점) |

### 1.3 결함 픽셀 보정 — SWU-1.3

| 항목 | 내용 |
|------|------|
| **목적** | 불량 픽셀(사망/핫/고착/노이즈) 검출 및 보간 은폐 |
| **알고리즘** | RMM (Robust Mask Maker, λ=8.0) + 3가지 보간 모드 |
| **PRD 요건** | [xray-detector-calibration-prd.md §3.4](xray-detector-calibration-prd.md) |
| **SRS 요건** | [SRS-CALIB-001 §2.2 SRS-CALIB-FUNC-007](SRS-CALIB-001_Software_Requirements_Specification.md) |
| **상세 SRS** | [SRS-DEFECT-001](../panel-defect/SRS-DEFECT-001_Software_Requirements_Specification.md) |
| **SAD 설계** | [SAD-DEFECT-001](../panel-defect/SAD-DEFECT-001_Software_Architecture_Document.md) |
| **개념도** | [CONCEPT-DIAGRAMS.md §7](CONCEPT-DIAGRAMS.md#7-결함-픽셀-보정) |
| **구현** | `modules/preprocess/src/xpe_defect.cpp` (301줄) |
| **테스트** | `modules/preprocess/tests/test_defect_correct.cpp` |
| **취득 절차** | [IAP-DEFECT-001](../panel-defect/IAP-DEFECT-001_Image_Acquisition_Protocol.md) |
| **한계** | 결함 밀도 ≤5%, 클러스터 최대 크기 제한 |

### 1.4 고스트/잔상 보정 — SWU-1.4

| 항목 | 내용 |
|------|------|
| **목적** | 이전 노출에 의한 잔류 전하 잔상(lag) 제거 |
| **알고리즘** | 3-Tier: LTI 역합성곱 → 노출 가중 → NLCSC (N=4 지수) |
| **PRD 요건** | [sw_lag_correction_prd_v2.md](../ghost-correction/sw_lag_correction_prd_v2.md) |
| **SRS 요건** | [srs_ghost_correction.md](../ghost-correction/srs_ghost_correction.md) |
| **SAD 설계** | [sad_ghost_correction.md](../ghost-correction/sad_ghost_correction.md) |
| **SDD 설계** | [sdd_ghost_correction.md](../ghost-correction/sdd_ghost_correction.md) |
| **개념도** | [CONCEPT-DIAGRAMS.md §8](CONCEPT-DIAGRAMS.md#8-고스트잔상-보정-3-tier) |
| **구현** | `modules/preprocess/src/ghost_correct.cpp` (266줄) |
| **테스트** | `modules/preprocess/tests/test_ghost_correct.cpp` |
| **취득 절차** | [IAP-GHOST-001](../ghost-correction/IAP-GHOST-001_Image_Acquisition_Protocol.md) |
| **특이사항** | **스테이트풀** — `xpe_ghost_create/correct/reset/destroy` 수명주기 관리 필수 |

### 1.5 비선형성 보정 — SWU-1.7

| 항목 | 내용 |
|------|------|
| **목적** | 검출기 응답 비선형성(전하 트래핑, 필 팩터) 선형화 |
| **알고리즘** | LUT (4096 entries, Fritsch-Carlson 스플라인) 또는 다항식 (차수 ≤5, Horner) |
| **SRS 요건** | [SRS-CALIB-001 SRS-CALIB-FUNC-006, 006-EXT](SRS-CALIB-001_Software_Requirements_Specification.md) |
| **개념도** | [CONCEPT-DIAGRAMS.md §6](CONCEPT-DIAGRAMS.md#6-비선형성-보정) |
| **구현** | `modules/preprocess/src/nonlinearity_correct.cpp` (117줄) |
| **오차 한계** | LUT: ≤0.3% ADC 풀스케일 / 다항식: ≤0.5% ADU |
| **실행 순서** | **오프셋 보정 이후, 게인 보정 이전** (선형화 후 정규화) |

### 1.6 온도 보상 — SWU-1.6

| 항목 | 내용 |
|------|------|
| **목적** | 온도 변화에 따른 암전류 드리프트 보상 |
| **수식** | `I_dark(T) = I₀ × exp(-Eg / 2kBT)`, Eg=1.12eV (Si) |
| **SRS 요건** | [SRS-CALIB-001 SRS-CALIB-FUNC-009](SRS-CALIB-001_Software_Requirements_Specification.md) |
| **개념도** | [CONCEPT-DIAGRAMS.md §9](CONCEPT-DIAGRAMS.md#9-온도-보상) |
| **구현** | `modules/preprocess/src/xpe_offset.cpp` (온도 보상 통합) |
| **바이패스 조건** | ΔT < 2°C → 보상 건너뜀 |
| **센서** | NTC 서미스터 (패널 탑재) |

### 1.7 캘리브레이션 데이터 관리 — SWU-1.5

| 항목 | 내용 |
|------|------|
| **목적** | 캘리브레이션 파일 로드/저장/검증/만료 관리 |
| **파일 형식** | `.xpe_calib` — 34 byte 헤더 + CRC-32(4) + 픽셀 데이터 |
| **SRS 요건** | [SRS-CALIB-001 SRS-CALIB-FUNC-001~003](SRS-CALIB-001_Software_Requirements_Specification.md) |
| **개념도** | [CONCEPT-DIAGRAMS.md §11](CONCEPT-DIAGRAMS.md#11-캘리브레이션-데이터-생명주기) |
| **구현** | `modules/preprocess/src/calibration_manager.cpp` (250줄) |
| **I/O** | `xcal_reader.cpp` / `xcal_writer.cpp` / `xcal_validator.cpp` |
| **취득 절차** | [IAP-CALIB-001](IAP-CALIB-001_Image_Acquisition_Protocol.md) |
| **테스트 데이터** | [TDS-CALIB-001](TDS-CALIB-001_Test_Dataset_Specification.md) |

### 1.8 바이닝 보정 — SWU-1.8

| 항목 | 내용 |
|------|------|
| **목적** | 바이닝 모드(1×1 제외)에서 게인/균일성 차이 보상 |
| **구현** | `modules/preprocess/src/binning_correct.cpp` (104줄) |
| **바이패스 조건** | binning_mode == 1×1 → 건너뜀 |
| **상태** | ⚠️ 바이닝 모드 프로파일 상세 미문서화 |

---

## 2. 문서 유형별 인덱스

### 2.1 요구사항 문서 (PRD/SRS)

| 문서 | 경로 | 범위 | IEC 62304 |
|------|------|------|-----------|
| 시스템 PRD | [xray-detector-calibration-prd.md](xray-detector-calibration-prd.md) | 10개 알고리즘, 9단계 절차 | — |
| SRS (캘리브레이션) | [SRS-CALIB-001](SRS-CALIB-001_Software_Requirements_Specification.md) | 25+ 기능/안전/성능 요건 | 조항 5.2 |
| SRS (고스트) | [srs_ghost_correction.md](../ghost-correction/srs_ghost_correction.md) | Tier 1/2/3 요건, 4지수 모델 | 조항 5.2 |
| SRS (결함) | [SRS-DEFECT-001](../panel-defect/SRS-DEFECT-001_Software_Requirements_Specification.md) | BPM 생성, 보간 알고리즘 | 조항 5.2 |
| SRS (향상) | [SRS-ENHANCE-BASIC-001](../enhance-basic/SRS-ENHANCE-BASIC-001_Software_Requirements_Specification.md) | CLAHE, 로그 변환 (다운스트림) | 조항 5.2 |

### 2.2 아키텍처 문서 (SAD)

| 문서 | 경로 | 범위 |
|------|------|------|
| SAD (캘리브레이션) | [SAD-CALIB-001](SAD-CALIB-001_Software_Architecture_Document.md) | SWU-1.1~1.9, 데이터 흐름, 인터페이스 |
| SAD (고스트) | [sad_ghost_correction.md](../ghost-correction/sad_ghost_correction.md) | 고스트 보정 서브시스템 아키텍처 |
| SAD (결함) | [SAD-DEFECT-001](../panel-defect/SAD-DEFECT-001_Software_Architecture_Document.md) | 결함 보정 아키텍처 |
| SDD (고스트) | [sdd_ghost_correction.md](../ghost-correction/sdd_ghost_correction.md) | 상세 설계 |

### 2.3 위험 분석 (SHA)

| 문서 | 경로 | 위험 수 | 표준 |
|------|------|---------|------|
| SHA (캘리브레이션) | [SHA-CALIB-001](SHA-CALIB-001_Software_Hazard_Analysis.md) | 7개 | ISO 14971 |
| SHA (결함) | [SHA-DEFECT-001](../panel-defect/SHA-DEFECT-001_Software_Hazard_Analysis.md) | — | ISO 14971 |

### 2.4 추적성 문서 (RTM)

| 문서 | 경로 | 추적 방향 |
|------|------|---------|
| RTM (캘리브레이션) | [RTM-CALIB-001](RTM-CALIB-001_Requirements_Traceability_Matrix.md) | SRS↔SAD↔테스트↔위험 양방향 |
| RTM (고스트) | [rtm_ghost_correction.md](../ghost-correction/rtm_ghost_correction.md) | SRS↔테스트 |
| RTM (결함) | [RTM-DEFECT-001](../panel-defect/RTM-DEFECT-001_Requirements_Traceability_Matrix.md) | SRS↔테스트 |

### 2.5 취득 프로토콜 (IAP)

| 문서 | 경로 | 대상 독자 |
|------|------|---------|
| IAP (캘리브레이션) | [IAP-CALIB-001](IAP-CALIB-001_Image_Acquisition_Protocol.md) | 캘리브레이션 엔지니어, 운영자 |
| IAP (고스트) | [IAP-GHOST-001](../ghost-correction/IAP-GHOST-001_Image_Acquisition_Protocol.md) | 캘리브레이션 엔지니어 |
| IAP (결함) | [IAP-DEFECT-001](../panel-defect/IAP-DEFECT-001_Image_Acquisition_Protocol.md) | 캘리브레이션 엔지니어 |

### 2.6 테스트 데이터 명세 (TDS)

| 문서 | 경로 | 범위 |
|------|------|------|
| TDS (캘리브레이션) | [TDS-CALIB-001](TDS-CALIB-001_Test_Dataset_Specification.md) | 10개 알고리즘 데이터셋 |
| TDS (고스트) | [TDS-GHOST-001](../ghost-correction/TDS-GHOST-001_Test_Dataset_Specification.md) | 고스트 테스트 데이터 |
| TDS (결함) | [TDS-DEFECT-001](../panel-defect/TDS-DEFECT-001_Test_Dataset_Specification.md) | BPM 테스트 데이터 |

### 2.7 개념도 및 검증 문서

| 문서 | 경로 | 범위 |
|------|------|------|
| **개념도 (신규)** | **[CONCEPT-DIAGRAMS.md](CONCEPT-DIAGRAMS.md)** | **12개 Mermaid 다이어그램, 처리 메커니즘** |
| 알고리즘 검증 가이드 | [ALGORITHM-VERIFICATION-GUIDE.md](ALGORITHM-VERIFICATION-GUIDE.md) | Spec↔구현 교차검증, 수학적 검증 |
| 교차 검증 보고서 | [../../.moai/specs/SPEC-XPE-MASTER/cross-verification-report.md](../../.moai/specs/SPEC-XPE-MASTER/) | PRD↔SRS↔구현 검증 |

---

## 3. 역할별 탐색 가이드

### 소프트웨어 개발자

```
1. 시작: README.md (파이프라인 전체 구조)
2. 아키텍처: SAD-CALIB-001 (SWU 분해, 인터페이스)
3. 시각화: CONCEPT-DIAGRAMS.md (처리 메커니즘 다이어그램)
4. API: modules/preprocess/include/xpe/preprocess/xpe_preprocess_api.h
5. 구현: modules/preprocess/src/ (알고리즘별 .cpp)
6. 테스트 작성: modules/preprocess/tests/ + TDS-CALIB-001
```

### 알고리즘/검증 엔지니어

```
1. 수학 모델: CONCEPT-DIAGRAMS.md §수학적 모델
2. 사양 검증: ALGORITHM-VERIFICATION-GUIDE.md
3. 요건 상세: SRS-CALIB-001 (SRS-CALIB-FUNC-XXX)
4. 교차 검증: .moai/specs/SPEC-XPE-MASTER/cross-validation-report-v5.md
5. 벤치마크: docs/project/Algorithm-Benchmark-Pack-Spec.md
```

### QA / 테스트 엔지니어

```
1. 추적성: RTM-CALIB-001 (SRS↔테스트 매핑)
2. 테스트 데이터: TDS-CALIB-001 (데이터셋 명세)
3. 합격 기준: SRS-CALIB-001 §검증 컬럼
4. 테스트 파일: modules/preprocess/tests/
5. E2E 프로토콜: docs/project/Preprocessing-E2E-Automated-Evaluation-Protocol.md
```

### 캘리브레이션/현장 엔지니어

```
1. 취득 절차: IAP-CALIB-001 (Dark/Flat/BPM/비선형/Lag 프로토콜)
2. 고스트 취득: IAP-GHOST-001
3. 결함 취득: IAP-DEFECT-001
4. 데이터 생명주기: CONCEPT-DIAGRAMS.md §11
```

### 안전/위험 담당자

```
1. 위험 분석: SHA-CALIB-001 (7개 위험, ISO 14971)
2. 바이패스 안전 규칙: SHA-CALIB-001 §BYP-SAFE-001~008
3. 추적성: RTM-CALIB-001 (위험↔요건↔테스트)
4. 결함 위험: SHA-DEFECT-001
```

### 의료기기 규제 담당자 (IEC 62304)

```
1. SRS-CALIB-001 (조항 5.2 — 소프트웨어 요건 명세)
2. SAD-CALIB-001 (조항 5.3 — 소프트웨어 아키텍처 설계)
3. SHA-CALIB-001 (ISO 14971 — 위험 관리)
4. RTM-CALIB-001 (추적성 매트릭스)
5. 시스템 준수 문서: docs/post-processing/xpe/XPE-62304-MAP-001_Compliance_Matrix.md
```

---

## 4. 소스 코드 ↔ 문서 매핑

| 소스 파일 | 줄수 | SWU | SRS 요건 | 테스트 파일 |
|----------|-----|-----|---------|-----------|
| `src/xpe_offset.cpp` | 253 | 1.1, 1.6 | FUNC-004, 009 | test_offset_correct.cpp |
| `src/offset_correct.cpp` | 112 | 1.1 | FUNC-004 | test_offset_correct.cpp |
| `src/xpe_gain.cpp` | 229 | 1.2 | FUNC-005 | test_gain_correct.cpp |
| `src/gain_correct.cpp` | 112 | 1.2 | FUNC-005 | test_gain_correct.cpp |
| `src/nonlinearity_correct.cpp` | 117 | 1.7 | FUNC-006, 006-EXT | (테스트 필요) |
| `src/xpe_defect.cpp` | 301 | 1.3 | FUNC-007 | test_defect_correct.cpp |
| `src/defect_correct.cpp` | 112 | 1.3 | FUNC-007 | test_defect_correct.cpp |
| `src/ghost_correct.cpp` | 266 | 1.4 | FUNC-008 | test_ghost_correct.cpp |
| `src/binning_correct.cpp` | 104 | 1.8 | FUNC-010 | (테스트 필요) |
| `src/calibration_manager.cpp` | 250 | 1.5 | FUNC-001~003 | test_calibration_manager.cpp |
| `src/xpe_calibration.cpp` | 112 | 1.5 | FUNC-001~003 | test_calibration_manager.cpp |
| `src/xpe_calib_load_offset.cpp` | 129 | 1.5 | FUNC-001 | test_xpe_calib_load.cpp |
| `src/xpe_calib_load_gain.cpp` | 124 | 1.5 | FUNC-002 | test_xpe_calib_load.cpp |
| `src/xpe_calib_load_defect_map.cpp` | 102 | 1.5 | FUNC-003 | test_xpe_calib_load.cpp |
| `src/xpe_calib_save.cpp` | 116 | 1.5 | FUNC-001~003 | test_xpe_calib_save.cpp |
| `src/xpe_calib_generate_offset.cpp` | 127 | 1.5 | FUNC-001 | test_xpe_calib_generate_offset.cpp |
| `src/xpe_calib_check_expiry.cpp` | 96 | 1.5 | FUNC-001~003 | test_xpe_calib_check_expiry.cpp |
| `src/xcal_reader.cpp` | 114 | I/O | FUNC-001~003 | — |
| `src/xcal_writer.cpp` | 104 | I/O | FUNC-001~003 | — |
| `src/xcal_validator.cpp` | 121 | I/O | FUNC-001~003 | — |
| `src/pipeline.cpp` | 200 | 전체 | 전체 | test_xpe_preprocess_calibration.cpp |
| `src/xpe_preprocess.cpp` | 287 | 전체 | 전체 | test_xpe_preprocess_calibration.cpp |

**헤더 파일**:
- `include/xpe/preprocess/xpe_preprocess_api.h` — 18개 내보내기 함수 공개 C ABI
- `include/xpe/preprocess/xpe_preprocess_internal.h` — 내부 구조체 (GhostCorrectorHandle 등)
- `include/xpe/preprocess/xcal_format.h` — .xpe_calib 파일 형식 명세

---

## 5. 교차 모듈 의존 관계

```
xpe_preprocess.dll (캘리브레이션 모듈)
    │
    ├── 의존 (업스트림)
    │   └── xpe_common.dll
    │       ├── XpeImageBuffer (이미지 버퍼 구조체)
    │       ├── XpeImageMetadata (온도, kVp, SID, 바이닝 모드)
    │       ├── XpeErrorCode (에러 코드 정의)
    │       └── Alert Queue (경고/에러 알림)
    │
    ├── 다운스트림 소비자
    │   └── xpe_enhance_basic.dll
    │       ├── float32 보정 프레임 수신 (이 모듈 출력)
    │       ├── CLAHE (대비 제한 적응형 히스토그램 평활화)
    │       └── 로그 변환, 엣지 향상
    │
    ├── 병렬 모듈 (독립, 교차 의존 없음)
    │   ├── gsvg.dll — 그리드 억제, 가상 그리드
    │   ├── xpe_enhance_adv.dll — 고급 향상
    │   └── xpe_dicom.dll — DICOM I/O
    │
    └── GUI 호출자
        └── ImageProcTest.exe (C# WPF)
            └── P/Invoke via C ABI
```

**크로스 모듈 SRS 인터페이스**:
- `xpe_enhance_basic` 입력 요건: [SRS-ENHANCE-BASIC-001 §3.1](../enhance-basic/SRS-ENHANCE-BASIC-001_Software_Requirements_Specification.md) — float32 보정 프레임 필요
- `xpe_common` 타입: [SAD-COMMON-001](../common/SAD-COMMON-001_Software_Architecture_Document.md) (경로 확인 필요)

---

## 6. 수학적 모델 레퍼런스

| 알고리즘 | 수식 | 파라미터 | SRS 위치 |
|---------|------|---------|---------|
| 오프셋 보정 | `I_corr = I_raw - I_dark` | I_dark: 캘리브레이션 맵 | SRS-CALIB-FUNC-004 |
| 온도 드리프트 | `I_dark(T) = I₀ × exp(-Eg/2kBT)` | Eg=1.12eV, kB=8.617×10⁻⁵eV/K | SRS-CALIB-FUNC-009 |
| 게인 보정 | `I_norm = I_corr / G(x,y)` | G: [0.1, 10.0] float32 맵 | SRS-CALIB-FUNC-005 |
| 다중 SID 게인 | `G(E) = Σ cₖEᵏ, k=0..4` | cₖ: 에너지 다항식 계수 | SRS-CALIB-FUNC-005 |
| 비선형 LUT | `I_lin = LUT[I_raw]` | 4096 or 65536 entries | SRS-CALIB-FUNC-006-EXT |
| 비선형 다항식 | `I_lin = Σ aₖIᵏ (Horner)` | 차수 ≤5, 단조성 검증 | SRS-CALIB-FUNC-006-EXT |
| RMM 결함 검출 | `\|σ_px - σ_med\| > λ×MAD` | λ=8.0 | SRS-CALIB-FUNC-007 |
| 고스트 Tier 1 | `I_lag = Σᵢ αᵢ exp(-n/τᵢ)` | αᵢ, τᵢ: 패널 특성 | srs_ghost_correction.md |
| 고스트 NLCSC | N=4 지수 상태 변수 모델 | α₁..₄, τ₁..₄ | srs_ghost_correction.md §NLCSC |
| DQE 허용 저하 | DQE 손실 ≤ 5% | — | PRD §6 메트릭 |
| 균일성 | `σ/mean < 1%` (재캘리브레이션 후) | 3072×3072 ROI | TDS-CALIB-001 |

---

## 7. 에러 코드 레퍼런스

| 에러 코드 | 의미 | 발생 단계 | 대응 방법 |
|---------|-----|---------|---------|
| `XPE_ERR_IO_FAILED` | 파일 I/O 실패 또는 CRC-32 불일치 | 캘리브레이션 로드 | 파일 재취득, 저장 경로 확인 |
| `XPE_ERR_INVALID_CALIB_DATA` | 게인값 범위 [0.1,10.0] 초과 또는 0 나눗셈 | 게인 보정 | 캘리브레이션 파일 재생성 |
| `XPE_ERR_CALIB_EXPIRED` | 만료 타임스탬프 초과 | 초기화 | 현장 재캘리브레이션 실행 |
| `XPE_WARN_CALIB_EXPIRING_SOON` | 만료 7일 이내 | 만료 검사 | 재캘리브레이션 예약 |
| `XPE_ERR_DEFECT_DENSITY_EXCEEDED` | 결함 밀도 >5% | BPM 로드 | 패널 교체 검토 |

---

## 8. API 레퍼런스 인덱스

공개 C ABI 전체 명세: `modules/preprocess/include/xpe/preprocess/xpe_preprocess_api.h`

| 함수 | 범주 | SWU | 설명 |
|------|------|-----|------|
| `xpe_offset_correct()` | 보정 | 1.1 | 오프셋(암전류) 보정 |
| `xpe_gain_correct()` | 보정 | 1.2 | 게인(평탄화) 보정 + uint16→float32 |
| `xpe_nonlinearity_correct()` | 보정 | 1.7 | 비선형성 LUT/다항식 보정 |
| `xpe_defect_correct()` | 보정 | 1.3 | 결함 픽셀 보간 보정 |
| `xpe_defect_detect_runtime()` | 보정 | 1.3 | 런타임 SNR 기반 결함 검출 |
| `xpe_ghost_create()` | 고스트 | 1.4 | 고스트 보정기 핸들 생성 |
| `xpe_ghost_correct()` | 고스트 | 1.4 | 고스트/잔상 보정 적용 |
| `xpe_ghost_reset()` | 고스트 | 1.4 | 이력 버퍼 초기화 |
| `xpe_ghost_destroy()` | 고스트 | 1.4 | 핸들 해제 |
| `xpe_temp_compensate()` | 온도 | 1.6 | 온도 보상 적용 |
| `xpe_calib_load_offset()` | I/O | 1.5 | 오프셋 캘리브레이션 로드 |
| `xpe_calib_load_gain()` | I/O | 1.5 | 게인 캘리브레이션 로드 |
| `xpe_calib_load_defect_map()` | I/O | 1.5 | BPM 로드 |
| `xpe_calib_save()` | I/O | 1.5 | 캘리브레이션 저장 + CRC-32 |
| `xpe_calib_generate_offset()` | 생성 | 1.5 | 암전류 프레임에서 오프셋 맵 생성 |
| `xpe_calib_check_expiry()` | 검증 | 1.5 | 만료 타임스탬프 검사 |
| `xpe_validate_readout_artifact()` | 검증 | 1.9 | 리드아웃 아티팩트 검증 |
| `xpe_preprocess_pipeline()` | 파이프라인 | 전체 | 전체 파이프라인 실행 |

---

## 9. 테스트 커버리지 맵

| 알고리즘 | 단위 테스트 파일 | 테스트 케이스 수 (추정) | 커버리지 상태 |
|---------|--------------|-------------------|------------|
| 오프셋 보정 | test_offset_correct.cpp | ~180줄 | ✅ 정상 |
| 게인 보정 | test_gain_correct.cpp | ~200줄 | ✅ 정상 |
| 결함 보정 | test_defect_correct.cpp | ~220줄 | ✅ 정상 |
| 고스트 보정 | test_ghost_correct.cpp | ~250줄 | ✅ 정상 |
| 캘리브레이션 로드 | test_xpe_calib_load.cpp | ~150줄 | ✅ 정상 |
| 캘리브레이션 저장 | test_xpe_calib_save.cpp | ~140줄 | ✅ 정상 |
| 오프셋 맵 생성 | test_xpe_calib_generate_offset.cpp | ~160줄 | ✅ 정상 |
| 만료 검사 | test_xpe_calib_check_expiry.cpp | ~120줄 | ✅ 정상 |
| 내구성 테스트 | test_xpe_calib_endurance.cpp | ~180줄 | ✅ 정상 |
| 파이프라인 통합 | test_xpe_preprocess_calibration.cpp | ~200줄 | ✅ 정상 |
| 비선형성 보정 | ⚠️ 전용 테스트 없음 | — | ⚠️ 보강 필요 |
| 바이닝 보정 | ⚠️ 전용 테스트 없음 | — | ⚠️ 보강 필요 |
| Round-trip | test_calibration_roundtrip.cpp | ~200줄 | ✅ 정상 |

**전체 목표**: 85% 커버리지 (TRUST 5 기준)  
**현재 상태**: ~70-75% (비선형성/바이닝 테스트 부재)

---

## 10. 알려진 제약 및 미구현 항목

### 10.1 미구현 알고리즘 (PRD에 정의됨)

| 알고리즘 | PRD 항목 | 상태 | 메모 |
|---------|---------|------|------|
| 산란 보정 (Scatter) | PRD §5.5 CAL-05 | 📋 문서만 존재 | 그리드 없는 이미징 산란 제거 |
| 모아레/앨리어싱 보정 | PRD §5.6 CAL-06 | 📋 문서만 존재 | 산란 억제 그리드 간섭 제거 |

### 10.2 문서화 갭

| 항목 | 상태 | 권고 조치 |
|------|------|---------|
| 바이닝 모드 프로파일 상세 | ⚠️ 미상세 | binning_correct.cpp 리버스 문서화 |
| SIMD/AVX2 최적화 노트 | ⚠️ 없음 | 성능 최적화 문서 추가 |
| 현장 재캘리브레이션 주기/드리프트 기준 | ⚠️ 미상세 | IAP-CALIB-001 보강 필요 |
| 리드아웃 아티팩트 임계값 근거 | ⚠️ 미상세 | 측정 데이터 기반 임계값 정의 |

### 10.3 테스트 갭

| 항목 | 상태 |
|------|------|
| nonlinearity_correct.cpp 단위 테스트 | ⚠️ 없음 |
| binning_correct.cpp 단위 테스트 | ⚠️ 없음 |
| xcal_reader/writer 단위 테스트 | ⚠️ 없음 |

---

## 관련 문서 빠른 링크

**이 지식 베이스의 상위 문서**: [README.md](README.md)  
**개념도**: [CONCEPT-DIAGRAMS.md](CONCEPT-DIAGRAMS.md)  
**검증 가이드**: [ALGORITHM-VERIFICATION-GUIDE.md](ALGORITHM-VERIFICATION-GUIDE.md)  
**알고리즘 사양 (규범)**: [../../.moai/specs/xpe-algorithm-spec-deepsync.md](../../.moai/specs/xpe-algorithm-spec-deepsync.md)  
**시스템 마스터 SPEC**: [../../docs/project/SPEC-XPE-MASTER.md](../../docs/project/SPEC-XPE-MASTER.md)
