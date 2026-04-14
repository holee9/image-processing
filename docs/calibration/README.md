# X-ray FPD 캘리브레이션 전처리 모듈

**모듈**: `xpe_preprocess.dll` (Layer 1, Phase 1a)  
**소유자 DLL**: `xpe_preprocess.dll`  
**의존성**: `xpe_common.dll` (Layer 0)  
**안전 등급**: IEC 62304 Class B  
**문서 버전**: 1.1.0  
**날짜**: 2026-04-14  
**규범 사양**: [ALG-SPEC-001 v3.0.0-ds2](../../.moai/specs/xpe-algorithm-spec-deepsync.md)

---

## 캘리브레이션 문서 패키지 빠른 참조

이 README는 7개의 상호 연관된 캘리브레이션 문서 중 하나입니다. 역할에 따라 바로 이동하세요:

| 역할 | 읽어야 할 문서 | 목적 |
|------|--------------|------|
| **소프트웨어 개발자** | 이 README → SRS → SAD | 파이프라인 구조, API, 알고리즘 이해 |
| **캘리브레이션 엔지니어** | IAP-CALIB-001 | 영상 취득 절차 (Dark/Flat/BPM/Lag) |
| **QA / 테스트 엔지니어** | TDS-CALIB-001 → RTM | 테스트 데이터 구성, 합격 기준 |
| **안전/위험 담당자** | SHA-CALIB-001 → RTM | 위험 식별, 리스크 관리 |
| **의료기기 규제 담당자** | SRS → RTM → SHA → SAD | IEC 62304 추적성 패키지 |

### 문서 생태계 구조

```
┌─────────────────────────────────────────────────────────────────────┐
│               캘리브레이션 모듈 문서 패키지 (v1.1)                 │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │   xray-detector-calibration-prd.md  (PRD)                   │  │
│  │   알고리즘 요구사항 원본 · 9단계 캘리브레이션 절차 · 평가 기준│  │
│  └───────────────────┬──────────────────────────────────────────┘  │
│                      │ 파생                                         │
│          ┌───────────┼────────────────────┐                        │
│          │           │                    │                        │
│          v           v                    v                        │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────────────────┐   │
│  │SRS-CALIB-001 │ │SAD-CALIB-001 │ │SHA-CALIB-001             │   │
│  │소프트웨어    │ │소프트웨어    │ │소프트웨어 위험 분석      │   │
│  │요건 명세서   │ │아키텍처 문서 │ │(7개 위험, ISO 14971)     │   │
│  └──────┬───────┘ └──────┬───────┘ └──────────┬───────────────┘   │
│         │                │                     │                   │
│         └────────────────┼─────────────────────┘                   │
│                          │ 추적                                     │
│                          v                                         │
│                 ┌──────────────────┐                               │
│                 │  RTM-CALIB-001   │                               │
│                 │  요구사항 추적   │                               │
│                 │  행렬 (SRS↔Test) │                               │
│                 └────────┬─────────┘                               │
│                          │ 테스트 입력                             │
│          ┌───────────────┼──────────────────┐                     │
│          v               v                  v                     │
│  ┌──────────────┐ ┌──────────────┐          │                     │
│  │IAP-CALIB-001 │ │TDS-CALIB-001 │          │                     │
│  │영상 취득     │ │테스트 데이터 │          │                     │
│  │프로토콜      │ │셋 명세서     │──────────┘                     │
│  │(운영자용)    │ │(개발자/QA용) │                                 │
│  └──────────────┘ └──────────────┘                                 │
│                                                                     │
│  ▶ 이 파일 (README.md) = 소프트웨어 파이프라인 기술 개요           │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 목차

1. [개요](#1-개요)
2. [아키텍처](#2-아키텍처)
3. [파이프라인 단계](#3-파이프라인-단계)
4. [단계 의존성 그래프](#4-단계-의존성-그래프)
5. [데이터 흐름 및 형식 변환](#5-데이터-흐름-및-형식-변환)
6. [단계 우회(켜짐/꺼짐) 정책](#6-단계-우회-켜짐-꺼짐-정책)
7. [우회 구성](#7-우회-구성)
8. [캘리브레이션 데이터 관리](#8-캘리브레이션-데이터-관리)
9. [API 레퍼런스](#9-api-레퍼런스)
10. [성능 예산](#10-성능-예산)
11. [안전 제약 조건](#11-안전-제약-조건)
12. [참고문헌](#12-참고문헌)

---

## 1. 개요

`xpe_preprocess.dll`은 X-ray Flat Panel Detector (FPD) 이미지 처리를 위한 캘리브레이션 전처리 엔진입니다. 어두운 전류, 픽셀 감도 불균일성, 불량/핫 픽셀, 전하 갇힘 래그, 온도 드리프트, 응답 비선형성 등 물리적 검출기 아티팩트를 보정하여 원본 ADC 센서 출력을 깨끗하고 캘리브레이션된 `float32` 이미지로 변환합니다.

### 주요 특성

- **9개 처리 단계** (0~4, 부분 단계 포함)
- **18개 내보낸 C ABI 함수** (보정, 캘리브레이션 데이터 I/O, 고스트 상태 관리용)
- **3개 필수 단계** (어떤 경우에도 우회 불가)
- **6개 조건부 우회 가능 단계** (문서화된 안전 제약 포함)
- **1개 핵심 형식 경계**: Gain Correction (단계 2)에서의 `uint16` ~ `float32` 변환
- **3단계 고스트 보정** (자동 에스컬레이션: LTI -> Exposure-Weighted -> NLCSC)

### 지원되는 검출기 유형

| 검출기 | 변환 방식 | 대표 모델 | 캘리브레이션 참고 사항 |
|----------|-----------|---------------|-------------------|
| a-Si TFT FPD | Indirect (CsI:Tl, GOS) | Varex XRD 4343N | 래그 보정 필수, 높은 온도 민감도 |
| CMOS FPD | Indirect/Direct | Vieworks VIVIX-S | 낮은 래그, 높은 동적 범위 |
| Perovskite FPD | Direct | 연구 단계 | 특수 비선형성 보정 |
| Se/CdTe Direct FPD | Direct | Siemens, Philips | 고유한 결함 패턴 |

---

## 2. 아키텍처

### 레이어 위치

```
Layer 2  ImageProcTest.exe (C# WPF)       파이프라인 오케스트레이터
           |
           | P/Invoke (C ABI)
           v
Layer 1  xpe_preprocess.dll  <-- 이 모듈
           |
           | 링크 의존성
           v
Layer 0  xpe_common.dll                    타입, 메모리, 구성, 에러, 알림
```

### 안티-스파게티 규칙

- `xpe_preprocess.dll`은 `xpe_common.dll`에만 의존
- 다른 Layer 1 DLL(`xpe_enhance_basic`, `gsvg` 등)과 횡단 의존성 없음
- 모든 공유 타입과 유틸리티는 `xpe_common.dll`을 통해 전달

### 소프트웨어 단위 (SWU)

| SWU ID | 이름 | 단계 | 설명 |
|--------|------|:-----:|-------------|
| SWU-1.1 | OffsetCorrection | (1) | 동적 보간을 이용한 어두운 전류 차감 |
| SWU-1.2 | GainCorrection | (2) | 플랫-필드 정규화 + 다중 게인 다항식 |
| SWU-1.3 | DefectCorrection | (3) | 불량 픽셀 검출 및 보간 |
| SWU-1.4 | GhostCorrection | (4) | 3단계 래그/고스팅 제거 |
| SWU-1.5 | CalibDataManager | (0) | 캘리브레이션 파일 I/O, 만료 검증, 버전 관리 |
| SWU-1.6 | ReadoutValidator | (0.5) | 원본 프레임 무결성 검증 |
| SWU-1.7 | TempCompensation | (0.7) | 온도 의존 어두운 전류 보상 |
| SWU-1.8 | NonlinearityCorrection | (1.5) | 검출기 응답 선형화 |
| SWU-1.9 | BinningCorrection | (2.5) | 바이닝 모드 보상 |

---

## 3. 파이프라인 단계

### 3.1 완전한 전처리 시퀀스

```
원본 프레임 (uint16, 14/16-bit ADC)
  |
  v
+================================================================+
|  (0) CalibManager 로드                     [시작 시에만] |
|  오프셋 맵, 게인 맵, BPM, NLCSC 계수 로드            |
|  예산: 200 ms (일회)                                     |
+================================================================+
  |
  v
+----------------------------------------------------------------+
|  (0.5) Readout Artifact Validation               [ADVISORY]    |
|  Function: xpe_validate_readout_artifact()                     |
|  Checks: stuck rows/cols, ADC saturation, dropped lines        |
|  Mutates image: NO (flag + alert only)                         |
|  Bypass: Always safe                                           |
|  Flag: XPE_FLAG_READOUT_VALIDATED                              |
+----------------------------------------------------------------+
  |
  v
+----------------------------------------------------------------+
|  (0.7) Temperature Compensation                 [CONDITIONAL]  |
|  Function: xpe_temp_compensate()                               |
|  Model: I_dark(T) = I0 * exp(-Eg / 2*kB*T)                    |
|  Input: detector temperature from NTC sensor                   |
|  Bypass: sensor unavailable OR within +/-2C of nominal         |
|  Flag: XPE_FLAG_TEMP_COMPENSATED                               |
+----------------------------------------------------------------+
  |
  v  uint16
+=================================================================+
|| (1) Offset Correction                          [MANDATORY]   ||
|| Function: xpe_offset_correct()                               ||
|| Formula: I_corr(x,y) = I_raw(x,y) - I_dark(x,y)            ||
|| Dynamic: bilinear interpolation by temperature + PREP time   ||
|| Bypass: NEVER (dark current corrupts all downstream)         ||
|| Hard fail: offsetMap absent -> XPE_ERR_NOT_INITIALIZED       ||
+=================================================================+
  |
  v  uint16
+----------------------------------------------------------------+
|  (1.5) Nonlinearity Correction                  [CONDITIONAL]  |
|  Function: xpe_nonlinearity_correct()                          |
|  Model: LUT or monotonic polynomial linearization              |
|  Bypass: panel.linear = true in detector profile               |
|  Rationale: MUST precede Gain (linearize before normalize)     |
|  Flag: XPE_FLAG_NONLINEARITY_CORRECTED                         |
+----------------------------------------------------------------+
  |
  v  uint16
+=================================================================+
|| (2) Gain Correction                            [MANDATORY]   ||
|| Function: xpe_gain_correct()                                 ||
|| Formula: I_corr(x,y) = I_off(x,y) / G(x,y)                 ||
|| Multi-gain: G(x,y,E) = sum(c_k * E^k), internal selection   ||
|| Heel effect: Duo-SID projection (Wang 2013)                  ||
|| Bypass: NEVER (format conversion + normalization)            ||
|| Hard fail: gainMap absent -> XPE_ERR_NOT_INITIALIZED         ||
||                                                              ||
|| >>> FORMAT BOUNDARY: uint16 -> float32 <<<                   ||
+=================================================================+
  |
  v  float32
+----------------------------------------------------------------+
|  (2.5) Binning Correction                       [CONDITIONAL]  |
|  Function: xpe_binning_correct()                               |
|  Trigger: binningMode != 1 (not native 1x1)                   |
|  Bypass: binning mode inactive                                 |
|  Flag: XPE_FLAG_BINNING_CORRECTED                              |
+----------------------------------------------------------------+
  |
  v  float32
+----------------------------------------------------------------+
|  (3) Defect Correction                          [CONDITIONAL]  |
|  Function: xpe_defect_correct()                                |
|  Detection: RMM (Robust Mask Maker), lambda=8.0               |
|  Baseline: edge-aware bilinear interpolation                   |
|  Advanced: FixPix MLP (1425 params, FPGA-friendly)             |
|  Optional: xpe_defect_detect_runtime() for transient defects   |
|  Bypass: BPM empty AND runtime detection disabled              |
|  Flag: XPE_FLAG_DEFECT_CORRECTED                               |
+----------------------------------------------------------------+
  |
  v  float32
+----------------------------------------------------------------+
|  (4) Ghost / Lag Correction                     [CONDITIONAL]  |
|  Functions: xpe_ghost_create/correct/reset/destroy()           |
|  Tier 1: LTI multi-exponential (N=4) deconvolution            |
|  Tier 2: Exposure-weighted LTI                                 |
|  Tier 3: NLCSC (signal-dependent coefficients)                 |
|  State: exposureHistory ring buffer (8 frames, ~150 MB)        |
|  Bypass: first frame, single-shot, no history                  |
|  Flag: XPE_FLAG_GHOST_CORRECTED                                |
+----------------------------------------------------------------+
  |
  v  float32 (calibrated)
  |
  [To Enhancement Domain: stage (5) Log Transform]
```

### 3.2 단계 요약 테이블

| # | 단계 | SWU | 유형 | 입력 | 출력 | 캘리브 데이터 | 상태 유지 |
|---|-------|-----|:----:|:-----:|:------:|:----------:|:--------:|
| 0 | CalibManager 로드 | 1.5 | 시작 | 파일 | 맵 | 모두 | 예 |
| 0.5 | 리드아웃 검증 | 1.6 | 권고 | uint16 | uint16 | 없음 | 아니오 |
| 0.7 | 온도 보상 | 1.7 | 조건부 | uint16 | uint16 | LUT/poly | 아니오 |
| 1 | 오프셋 보정 | 1.1 | **필수** | uint16 | uint16 | offsetMap | 예 |
| 1.5 | 비선형성 | 1.8 | 조건부 | uint16 | uint16 | LUT/poly | 아니오 |
| 2 | 게인 보정 | 1.2 | **필수** | uint16 | **float32** | gainMap | 예 |
| 2.5 | 바이닝 보정 | 1.9 | 조건부 | float32 | float32 | 구성 | 아니오 |
| 3 | 결함 보정 | 1.3 | 조건부 | float32 | float32 | BPM | 예 |
| 4 | 고스트 보정 | 1.4 | 조건부 | float32 | float32 | 이력+계수 | 예 |

---

## 4. 단계 의존성 그래프

### 4.1 시각적 의존성 맵

```
                    ┌──────────────────────────────────────────────────────────┐
                    │                  (0) CalibManager                        │
                    │         Load calibration data at startup                 │
                    └────┬──────────┬───────────┬──────────┬──────────────────┘
                    DATA |     DATA |      DATA |     DATA |
                         v          v           v          v
  ┌────────────────────────────────────────────────────────────────────────┐
  │                        CALIBRATION DATA POOL                          │
  │   offsetMap    gainMap    BPM    NLCSC coefficients    Temp LUT       │
  └────┬───────────┬─────────┬──────┬───────────────────┬────────────────┘
       |           |         |      |                   |
       |           |         |      |        ┌──────────┘
       |           |         |      |        |
  ┌────▼───┐       |         |      |   ┌────▼────┐
  │  (0.5) │       |         |      |   │  (0.7)  │
  │Readout │       |         |      |   │  Temp   │
  │  Valid  │       |         |      |   │ Comp    │
  │ [A]    │       |         |      |   │ [C]     │
  └────┬───┘       |         |      |   └────┬────┘
       │           |         |      |        │
       └─────┐     |         |      |   ┌────┘
             v     |         |      |   v
          ┌════▼═══╪═════════╪══════╪═══▼════════════════════════┐
          ║  (1) Offset Correction                    [MANDATORY] ║
          ║  I_corr = I_raw - I_dark                              ║
          ║  REQUIRES: offsetMap                                   ║
          ╚════════════╤══════╪══════╪════════════════════════════╝
                       │      |      |
                ORDER  v      |      |
          ┌────────────────┐  |      |
          │ (1.5) Nonlin   │  |      |
          │ Correction [C] │  |      |
          └───────┬────────┘  |      |
                  │           |      |
            ORDER v      DATA v      |
          ┌════════════════════════╗  |
          ║  (2) Gain Correction   ║  |
          ║  I_corr = I_off / G    ║  |
          ║  [MANDATORY]           ║  |
          ║                        ║  |
          ║  <<< uint16 -> float32 ║  |
          ║      FORMAT BOUNDARY>>>║  |
          ╚═══════════╤════════════╝  |
                      │               |
               FORMAT v               |
          ┌──────────────────┐        |
          │ (2.5) Binning [C]│        |
          └───────┬──────────┘        |
                  │                   |
           FORMAT v              DATA v
          ┌──────────────────────────────┐
          │ (3) Defect Correction    [C] │
          │ REQUIRES: BPM + float32 data │
          └───────┬──────────────────────┘
                  │
            ORDER v
          ┌──────────────────────────────┐
          │ (4) Ghost/Lag Correction [C] │
          │ REQUIRES: exposure history   │
          │ + fully corrected frame      │
          └───────┬──────────────────────┘
                  │
                  v
          [Calibrated float32 output]

Legend:
  ═══  MANDATORY stage (double border)
  ───  CONDITIONAL stage (single border)
  [A]  ADVISORY (non-mutating)
  [C]  CONDITIONAL (bypassable)
  DATA  Requires calibration data
  ORDER Execution order constraint (physical/mathematical)
  FORMAT Requires float32 format from stage (2)
```

### 4.2 의존성 매트릭스

각 셀은 **행** 단계가 **열** 단계에 의존하는지 여부와 의존성 유형을 보여줍니다.

|  | (0) Calib | (0.5) Read | (0.7) Temp | (1) Offset | (1.5) NL | (2) Gain | (2.5) Bin | (3) Defect | (4) Ghost |
|--|:---------:|:----------:|:----------:|:----------:|:--------:|:--------:|:---------:|:----------:|:---------:|
| **(0) CalibManager** | -- | | | | | | | | |
| **(0.5) Readout** | | -- | | | | | | | |
| **(0.7) Temp** | | | -- | | | | | | |
| **(1) Offset** | `DATA` | | | -- | | | | | |
| **(1.5) Nonlin** | `DATA` | | | `ORDER` | -- | | | | |
| **(2) Gain** | `DATA` | | | `ORDER` | `ORDER` | -- | | | |
| **(2.5) Binning** | | | | | | `FMT` | -- | | |
| **(3) Defect** | `DATA` | | | | | `FMT` | | -- | |
| **(4) Ghost** | `DATA` | | | | | `FMT` | | `ORDER` | -- |

| 키 | 의미 | 위반 영향 |
|-----|---------|-----------------|
| `DATA` | CalibManager에서 로드한 캘리브레이션 데이터 필요 | **하드 실패**: `XPE_ERR_NOT_INITIALIZED` |
| `ORDER` | 선행 단계 이후 실행 필요 (물리적 제약) | **무음 손상**: 부정확한 보정 |
| `FMT` | Gain 단계에서 생성된 `float32` 형식 필요 | **충돌**: 타입 불일치 또는 버퍼 손상 |

### 4.3 임계 경로 분석

전처리의 **최소 필수 경로**:

```
(0) CalibManager -> (1) Offset -> (2) Gain -> [출력]
```

이 경로는 항상 실행되어야 합니다. 최소한으로 보정된 `float32` 이미지를 생성합니다. 다른 모든 단계는 이미지 품질을 개선하지만 유효한 출력을 위해 엄격히 필요하지 않은 선택적 개선입니다.

---

## 5. 데이터 흐름 및 형식 변환

### 5.1 형식 도메인

```
 ┌──────────────────────────────────────────────────────────────────┐
 │                                                                  │
 │   uint16 도메인                float32 도메인                   │
 │   (원본 / 정수)               (정규화 / 부동소수점)    │
 │                                                                  │
 │   (0.5) 리드아웃      ║         (2.5) 바이닝                    │
 │   (0.7) 온도         ║         (3)   결함                     │
 │   (1)   오프셋       ║         (4)   고스트                      │
 │   (1.5) 비선형성 ║                                          │
 │                      ║                                          │
 │              ┌═══════╩════════════┐                              │
 │              ║  (2) 게인 보정    ║                              │
 │              ║  형식 경계      ║                              │
 │              ║  uint16 -> float32 ║                              │
 │              ╚════════════════════╝                              │
 │                                                                  │
 └──────────────────────────────────────────────────────────────────┘
```

### 5.2 버퍼 사양

| 매개변수 | 값 |
|-----------|-------|
| 최대 크기 | 4096 x 4096 |
| 일반 크기 | 3072 x 3072 |
| uint16 버퍼 | ~18.9 MB (3072 x 3072 x 2 바이트) |
| float32 버퍼 | ~37.7 MB (3072 x 3072 x 4 바이트) |
| 캘리브레이션 맵 | ~60 MB (오프셋 + 게인 + BPM) |
| 고스트 이력 (8 프레임) | ~150 MB |
| 최고 메모리 (전처리) | ~190 MB |

### 5.3 메타데이터 플래그 생명주기

각 단계는 성공적인 실행 시 `XpeImageMetadata.flags`의 해당 플래그 비트를 설정합니다. 우회된 단계는 플래그를 **설정하지 않습니다**.

```
flags = 0x00000000  (원본 프레임)

(0.5) 후:  flags |= XPE_FLAG_READOUT_VALIDATED       0x0010
(0.7) 후:  flags |= XPE_FLAG_TEMP_COMPENSATED        0x0020
(1) 후:    [전용 플래그 없음 - 항상 실행]
(1.5) 후:  flags |= XPE_FLAG_NONLINEARITY_CORRECTED  0x0040
(2) 후:    flags |= XPE_FLAG_GAIN_CORRECTED           0x0008
(2.5) 후:  flags |= XPE_FLAG_BINNING_CORRECTED        0x0080
(3) 후:    flags |= XPE_FLAG_DEFECT_CORRECTED         0x0004
(4) 후:    flags |= XPE_FLAG_GHOST_CORRECTED          0x0001

예: 완전한 전처리 적용, 바이닝 없음
  flags = 0x007D = READOUT | TEMP | NONLIN | GAIN | DEFECT | GHOST

예: 최소 (오프셋 + 게인만, 원본 내보내기 모드)
  flags = 0x0008 = GAIN
```

---

## 6. 단계 우회(켜짐/꺼짐) 정책

### 6.1 우회 분류

| 기호 | 범주 | 설명 |
|:------:|----------|-------------|
| `M` | **필수** | 우회 불가. 없으면 파이프라인 실패. |
| `C` | **조건부** | 문서화된 조건에서 우회 가능. |
| `A` | **권고** | 비변경. 항상 안전하게 건너뛸 수 있음. |

### 6.2 우회 결정 테이블

| 단계 | 범주 | 끌 수 있나? | 우회 조건 | 안전 영향 | 다운스트림 영향 |
|-------|:----:|:--------:|------------------|:-------------:|-------------------|
| **(0) CalibManager** | `M` | 아니오 | -- | 치명적 | 캘리브레이션 데이터 없음 |
| **(0.5) Readout** | `A` | 예 | 구성: `readout_validation.enabled = false` | 없음 | 무결성 검사 없음 |
| **(0.7) Temp** | `C` | 예 | 센서 없음 또는 공칭값의 +/-2C 내 온도 | 낮음 | 미소한 어두운 드리프트 |
| **(1) Offset** | `M` | 아니오 | -- | 치명적 | 모든 픽셀의 어두운 바이어스 |
| **(1.5) Nonlinearity** | `C` | 예 | 검출기 프로필에서 `panel.linear = true` | 낮음 | 미소한 응답 곡선 오류 |
| **(2) Gain** | `M` | 아니오 | -- | 치명적 | 정규화 없음 + float32 변환 없음 |
| **(2.5) Binning** | `C` | 예 | `binningMode == 1` (원본) | 없음 | 적용 불가 |
| **(3) Defect** | `C` | 예 | BPM 비어있음 또는 진단 모드 | 중간 | 결함 아티팩트 표시 |
| **(4) Ghost** | `C` | 예 | 단일 샷 또는 첫 프레임 또는 이력 없음 | 중간 | 래그 아티팩트 표시 |

### 6.3 Bypass Decision Flowchart

```
                        START: New raw frame acquired
                                    |
                    +===============v================+
                    |   (0) CalibManager loaded?      |
                    +================================+
                        |                    |
                       YES                  NO
                        |                    |
                        v               ABORT PIPELINE
                +-------v--------+
                | (0.5) Readout  |
                | enabled?       |
                +----------------+
                  |           |
                 YES         NO -----> [SKIP: no mutation, safe]
                  |
                  v
            xpe_validate_readout_artifact()
                  |
            score > CRITICAL? --YES--> ABORT FRAME
                  |
                 NO
                  v
                +-------v--------+
                | (0.7) Temp     |
                | sensor avail?  |
                +----------------+
                  |           |
                 YES         NO -----> [SKIP: use nominal 25C + alert]
                  |
            |temp - 25C| > 2.0?
                  |           |
                 YES         NO -----> [SKIP: within tolerance]
                  |
                  v
            xpe_temp_compensate()
                  |
                  v
            +=========v==========+
            || (1) Offset       ||
            || ALWAYS EXECUTE   ||
            +====================+
            offsetMap loaded?
              |           |
             YES         NO -----> HARD FAIL: XPE_ERR_NOT_INITIALIZED
              |
              v
            xpe_offset_correct()
              |
              v
                +-------v--------+
                | (1.5) Nonlin   |
                | panel.linear?  |
                +----------------+
                  |           |
                 NO (apply)  YES (linear) -> [SKIP: profile says linear]
                  |
                  v
            xpe_nonlinearity_correct()
              |
              v
            +=========v==========+
            || (2) Gain         ||
            || ALWAYS EXECUTE   ||
            || uint16 -> float32||
            +====================+
            gainMap loaded?
              |           |
             YES         NO -----> HARD FAIL: XPE_ERR_NOT_INITIALIZED
              |
              v
            xpe_gain_correct()
              |
              v  [now float32]
                +-------v--------+
                | (2.5) Binning  |
                | mode != 1x1?  |
                +----------------+
                  |           |
                 YES         NO -----> [SKIP: native resolution]
                  |
                  v
            xpe_binning_correct()
              |
              v
                +-------v--------+
                | (3) Defect     |
                | BPM non-empty? |
                +----------------+
                  |           |
                 YES         NO -----> [SKIP: no defects]
                  |
            diagnostic mode?
                  |           |
                 NO          YES -----> [SKIP: raw export]
                  |
                  v
            xpe_defect_correct()
              |
              v
                +-------v--------+
                | (4) Ghost      |
                | history avail? |
                +----------------+
                  |           |
                 YES         NO -----> [SKIP: first frame / single-shot]
                  |
            single-shot mode?
                  |           |
                 NO          YES -----> [SKIP: no temporal correction]
                  |
                  v
            xpe_ghost_correct()
            [auto-escalate Tier 1->2->3]
              |
              v
            CALIBRATED float32 OUTPUT
```

### 6.4 Operating Modes

| Mode | Stages Executed | Use Case |
|------|:--------------:|----------|
| **Full Clinical** | All 9 stages | Normal clinical imaging |
| **Minimal Clinical** | (0), (1), (2), (3) | Fast acquisition, linear detector, no lag concern |
| **Diagnostic / Raw Export** | (0), (1), (2) only | External analysis tools, QA investigation |
| **Single-Shot** | All except (4) | First frame after power-on |
| **Fluoro / Continuous** | All 9 + Tier 2/3 ghost | Real-time fluoroscopy with lag correction |

---

## 7. 우회 구성

### 7.1 JSON 구성 스키마

`xpe_configure()`를 통해 우회 동작 구성:

```json
{
  "preprocess": {
    "readout_validation": {
      "enabled": true,
      "critical_threshold": 500,
      "warn_threshold": 200
    },
    "temp_compensation": {
      "enabled": true,
      "auto_bypass_tolerance_c": 2.0,
      "nominal_temp_c": 25.0
    },
    "nonlinearity": {
      "enabled": true,
      "bypass_if_linear_profile": true
    },
    "binning": {
      "enabled": true
    },
    "defect_correction": {
      "enabled": true,
      "runtime_detection": false,
      "interpolation_mode": "bilinear",
      "advanced_mode": "none"
    },
    "ghost_correction": {
      "enabled": true,
      "max_tier": 3,
      "bypass_single_shot": true,
      "min_history_frames": 1
    },
    "mode": "clinical"
  }
}
```

### 7.2 모드 프리셋

| 구성 키 | `"clinical"` | `"diagnostic"` | `"fluoro"` |
|------------|:------------:|:--------------:|:----------:|
| readout_validation | 켜짐 | 꺼짐 | 켜짐 |
| temp_compensation | 켜짐 (자동) | 꺼짐 | 켜짐 |
| offset_correction | **켜짐** | **켜짐** | **켜짐** |
| nonlinearity | 켜짐 (프로필) | 꺼짐 | 켜짐 |
| gain_correction | **켜짐** | **켜짐** | **켜짐** |
| binning | 켜짐 (자동) | 꺼짐 | 켜짐 |
| defect_correction | 켜짐 | 꺼짐 | 켜짐 |
| ghost_correction | 켜짐 (Tier 1-3) | 꺼짐 | 켜짐 (Tier 2-3) |

---

## 8. 캘리브레이션 데이터 관리

### 8.1 필수 캘리브레이션 파일

| 데이터 | 파일 형식 | 생성자 | 로드 함수 | 필수 |
|------|------------|-------------|---------------|:---------:|
| 오프셋 (어두운) 맵 | `.raw` / `.dcm` | `xpe_calib_generate_offset()` | `xpe_calib_load_offset()` | 예 |
| 게인 (플랫-필드) 맵 | `.raw` / `.dcm` | 외부 캘리브레이션 도구 | `xpe_calib_load_gain()` | 예 |
| 불량 픽셀 맵 (BPM) | `.raw` / `.dcm` | 외부 검출 도구 | `xpe_calib_load_defect_map()` | 예 |
| NLCSC 계수 | JSON 구성 | 캘리브레이션 세션 | `xpe_ghost_create()` 구성 | 아니오 |
| 온도 LUT | JSON 구성 | 공장 캘리브레이션 | `xpe_configure()` | 아니오 |
| 비선형성 곡선 | JSON 구성 | 공장 캘리브레이션 | `xpe_configure()` | 아니오 |

### 8.2 캘리브레이션 영상 취득 요약

> 상세 절차는 **IAP-CALIB-001** (Image Acquisition Protocol)을 참조하세요.

각 캘리브레이션 데이터는 특정 촬영 프로토콜을 통해 취득된 원시 영상에서 생성됩니다:

| 캘리브 데이터 | 필요 영상 유형 | 취득 조건 | IAP 섹션 |
|-------------|-------------|---------|---------|
| 오프셋 맵 (offsetMap) | Dark Frame — X선 OFF | 온도 6단계 × PREP 7단계 × 100프레임 | IAP §6.1 |
| 게인 맵 (gainMap) | Flat-field — RQA-5 균일 조사 | 40~60% 포화도, Duo-SID (110/150 cm) | IAP §6.2 |
| 불량 픽셀 맵 (BPM) | Dark + Flat-field 각 200프레임 | 온도 3단계, λ=8.0 임계값 | IAP §6.3 |
| 비선형성 계수 | 다중 포화도 Flat-field | 5%~90% 범위 최소 5단계, 각 16프레임 | IAP §6.4 |
| Lag/Ghost 계수 (NLCSC) | FSRF + RSRF 시퀀스 | 9개 포화도 수준, 6,030프레임 총계 | IAP §6.5 |

**공장 vs. 현장 취득 비교**:

| 항목 | 공장 (Factory) | 현장 (Field) |
|------|:-------------:|:-----------:|
| 총 프레임 수 | ~12,000 프레임 | ~500 프레임 |
| 소요 시간 | ~8시간 | ~2시간 |
| 온도 조건 | 6단계 (10~40°C) | 2~3단계 |
| PREP 시간 | 7단계 (1~30초) | 3~4단계 |

### 8.3 캘리브레이션 데이터 생명주기

```
공장 캘리브레이션 ──> [offsetMap, gainMap, BPM, 온도 LUT, NL 곡선]
       |                               |
       |    임상 사이트에 설치         |
       v                               v
현장 캘리브레이션 ──> [현장 오프셋 업데이트, 새로운 BPM 항목]
       |                               |
       |    주기적 QA / 드리프트        |
       v                               v
런타임 모니터링 ──> [드리프트 검출, 만료 확인, 재캘리브레이션 알림]
       |                               |
       |    연간 / 긴급              |
       v                               v
재캘리브레이션 ──> [완전한 공장 새로고침 또는 현장 업데이트]
```

### 8.4 만료 및 드리프트 검출

| 트리거 | 임계값 | 조치 |
|---------|-----------|--------|
| 캘리브레이션 파일 만료됨 | `xpe_calib_check_expiry()`가 `XPE_ERR_CALIBRATION_EXPIRED` 반환 | 파이프라인 시작 차단 |
| 온도 드리프트 | `abs(현재 - 참조) > 3.0 C` | 자동 현장 어두운 업데이트 |
| 경과 시간 | 마지막 캘리브레이션 이후 `> 30분` | 자동 현장 어두운 업데이트 |
| 플랫-필드 잔차 | `sigma/mean > 1.5%` | 긴급 재캘리브레이션 알림 |
| SNR 저하 | 95% 신뢰도 구간 밖 | 재캘리브레이션 권장 |

---

## 9. API 레퍼런스

### 9.1 보정 함수

| 함수 | 단계 | 인플레이스 | 스레드 안전 | 필수 |
|----------|:-----:|:--------:|:-----------:|:---------:|
| `xpe_validate_readout_artifact()` | 0.5 | 아니오 (읽기 전용) | 예 | 아니오 |
| `xpe_temp_compensate()` | 0.7 | 예 | 예 | 아니오 |
| `xpe_offset_correct()` | 1 | 예 | 예 | **예** |
| `xpe_nonlinearity_correct()` | 1.5 | 예 | 예 | 아니오 |
| `xpe_gain_correct()` | 2 | 예 | 예 | **예** |
| `xpe_binning_correct()` | 2.5 | 예 | 예 | 아니오 |
| `xpe_defect_correct()` | 3 | 예 | 예 | 아니오 |
| `xpe_defect_detect_runtime()` | 3 | 아니오 (출력 맵) | 예 | 아니오 |
| `xpe_ghost_correct()` | 4 | 예 | 핸들별 | 아니오 |

### 9.2 고스트 상태 관리

| 함수 | 목적 | 호출 패턴 |
|----------|---------|-------------|
| `xpe_ghost_create()` | 보정기 핸들 생성 | 시작 시 한 번 |
| `xpe_ghost_correct()` | 래그 보정 적용 | 매 프레임 |
| `xpe_ghost_reset()` | 노출 이력 초기화 | 환자 간 |
| `xpe_ghost_destroy()` | 리소스 해제 | 종료 시 |

### 9.3 캘리브레이션 I/O

| 함수 | 목적 | 반환값 |
|----------|---------|---------|
| `xpe_calib_load_offset()` | 파일에서 어두운 맵 로드 | 만료됨: `XPE_ERR_CALIBRATION_EXPIRED` |
| `xpe_calib_load_gain()` | 파일에서 게인 맵 로드 | 만료됨: `XPE_ERR_CALIBRATION_EXPIRED` |
| `xpe_calib_load_defect_map()` | 파일에서 BPM 로드 | `XPE_OK` |
| `xpe_calib_generate_offset()` | 어두운 프레임에서 오프셋 생성 | `XPE_OK` |
| `xpe_calib_save()` | 만료 시간과 함께 캘리브레이션 저장 | `XPE_OK` |
| `xpe_calib_check_expiry()` | 캘리브레이션 최신성 검증 | `XPE_ERR_CALIBRATION_EXPIRED` |

---

## 10. 성능 예산

### 10.1 단계별 시간 할당

| 단계 | 예산 (ms) | 예상 (ms) | 참고 |
|-------|:-----------:|:--------------:|-------|
| (0) CalibManager | 200 | 200 | 시작 시만, 프레임별 제외 |
| (0.5) 리드아웃 검증 | 15 | 10 | 읽기 전용 스캔 |
| (0.7) 온도 보상 | 10 | 5 | LUT 조회 |
| (1) 오프셋 보정 | 60 | 55 | 픽셀단위 차감 |
| (1.5) 비선형성 | 25 | 20 | LUT/다항식 평가 |
| (2) 게인 보정 | 60 | 55 | 픽셀단위 나눗셈 + 형식 변환 |
| (2.5) 바이닝 보정 | 15 | 10 | 조건부 곱셈 |
| (3) 결함 보정 | 110 | 95 | BPM 스캔 + 보간 |
| (4) 고스트 Tier 1 | 150 | 140 | 재귀 역컨볼루션 |
| (4) 고스트 Tier 2 | +40 | +40 | 노출 가중 선택 |
| (4) 고스트 Tier 3 | +90 | +90 | NLCSC 전체 알고리즘 |
| **전처리 총합** | **500** | **~390 (T1)** | 하드 상한선: 500 ms/프레임 |

### 10.2 메모리 예산

| 구성요소 | 크기 | 참고 |
|-----------|:----:|-------|
| 오프셋 맵 (uint16) | 18.9 MB | 3072 x 3072 |
| 게인 맵 (float32) | 37.7 MB | 3072 x 3072 |
| BPM (uint8) | 9.4 MB | 3072 x 3072 |
| 작업 버퍼 (float32) | 37.7 MB | 출력 프레임 |
| 고스트 이력 (8 x float32) | 150 MB | 링 버퍼 |
| **최고 총합** | **~190 MB** | Phase 1만 |

---

## 11. 안전 제약 조건

### 11.1 우회 안전 규칙 (BYP-SAFE)

| ID | 규칙 | 근거 |
|----|------|-----------|
| BYP-SAFE-001 | 오프셋 (1)은 구성으로 우회 불가 | 어두운 바이어스 항상 존재 |
| BYP-SAFE-002 | 게인 (2)은 구성으로 우회 불가 | 다운스트림에서 필요한 형식 변환 (uint16->float32) |
| BYP-SAFE-003 | 우회된 단계는 `XPE_FLAG_*` 비트 설정 금지 | 다운스트림과 QA는 적용된 항목을 알아야 함 |
| BYP-SAFE-004 | 고스트 우회는 재설정 후 첫 프레임에 자동 트리거 | 이력 없음 = 쓰레기 보정 |
| BYP-SAFE-005 | 비어있지 않은 BPM으로 결함 우회는 경고 알림 방출 | 알려진 결함 건너뛰기는 비정상 |
| BYP-SAFE-006 | 비선형성 우회는 명시적 `panel.linear = true` 필요 | 무음 건너뛰기는 감지되지 않은 아티팩트 위험 |
| BYP-SAFE-007 | 모든 우회 결정 진단 JSON에 로깅 | IEC 62304 추적성 |
| BYP-SAFE-008 | 진단 모드: 필수 (0), (1), (2)만 | 유효한 float32 출력을 위한 최소 보정 |

### 11.2 IEC 62304 준수 참고 사항

- 모든 캘리브레이션 함수는 처리 전에 null 포인터, 버퍼 크기, 형식 검증
- `XPE_ERR_CALIBRATION_EXPIRED`는 오래된 캘리브레이션 데이터 사용 방지
- 알림 큐는 이미지 전달을 차단하지 않으면서 모든 이상 상황 캡처
- 결정론적 출력: 동일 바이너리 + 구성 + 입력 = 동일 출력 해시
- 프레임별 핫 경로에서 무한 힙 할당 없음

---

## 12. 참고문헌

### 표준

| 표준 | 관련성 |
|----------|-----------|
| IEC 62220-1-1:2015 | DQE 측정 (오프셋/게인 품질 검증) |
| IEC 62494-1 | 노출 지수 (검출기 도메인 데이터 필요) |
| IEC 62304:2006+A1:2015 | 소프트웨어 생명주기 (안전 분류) |
| ISO 14971:2019 | 위험 관리 |

### 연구 논문

| 인용 | 주제 | 이 모듈에 미치는 영향 |
|----------|-------|----------------------|
| [Starman et al. 2012](https://pmc.ncbi.nlm.nih.gov/articles/PMC3465354/) | NLCSC 래그 보정 | Tier 3 고스트 알고리즘 |
| [Pang et al. 2006](https://pmc.ncbi.nlm.nih.gov/articles/PMC5722609/) | 래그 vs 고스팅 모델 | 래그/고스트 구별 |
| [Ranger et al. 2014](https://pmc.ncbi.nlm.nih.gov/articles/PMC3965338/) | 게인/오프셋 SNR 캘리브레이션 | 드리프트 검출 임계값 |
| [Jeon et al. 2021](https://pmc.ncbi.nlm.nih.gov/articles/PMC7930811/) | DL 결함 보정 | 고급 결함 수리 |
| [FixPix 2023](https://arxiv.org/html/2310.11637v2) | MLP 불량 픽셀 보정 | FixPix MLP 아키텍처 |
| [Wang 2013](https://www.math.union.edu/~wangj/papers/Wang13.Heel%20Effect%20%5BMed%20Phys%5D.pdf) | Duo-SID 힐 효과 | 게인 맵 힐 보상 |
| [EP2148500A1](https://patents.google.com/patent/EP2148500A1/en) | 동적 어두운 보정 | 온도/PREP 시간 모델 |

### 프로젝트 문서

#### IEC 62304 규제 문서 패키지

| 문서 ID | 제목 | 경로 | 대상 | 설명 |
|--------|------|------|------|------|
| **PRD** | 캘리브레이션 PRD | `docs/calibration/xray-detector-calibration-prd.md` | 개발자 | 9단계 알고리즘 요구사항 원본 · 수식 · 절차 전체 |
| **SRS-CALIB-001** | 소프트웨어 요건 명세서 | `docs/calibration/SRS-CALIB-001_Software_Requirements_Specification.md` | 개발자 | 25개 기능/안전/성능 요건 (IEC 62304 §5.2) |
| **SAD-CALIB-001** | 소프트웨어 아키텍처 문서 | `docs/calibration/SAD-CALIB-001_Software_Architecture_Document.md` | 개발자 | 9개 SWU 설계, 인터페이스, 데이터 흐름 (IEC 62304 §5.3) |
| **SHA-CALIB-001** | 소프트웨어 위험 분석 | `docs/calibration/SHA-CALIB-001_Software_Hazard_Analysis.md` | 안전 담당자 | 7개 위험 식별 · ISO 14971 리스크 평가 · 통제 |
| **RTM-CALIB-001** | 요건 추적 행렬 | `docs/calibration/RTM-CALIB-001_Requirements_Traceability_Matrix.md` | QA / 개발자 | SRS ↔ 아키텍처 ↔ 테스트 ↔ 위험 양방향 추적 (IEC 62304 §5.1.1c) |
| **IAP-CALIB-001** | **영상 취득 프로토콜** | `docs/calibration/IAP-CALIB-001_Image_Acquisition_Protocol.md` | **캘리브레이션 엔지니어** | Dark/Flat-field/BPM/Lag/Nonlinearity 촬영 절차 · 합격 기준 · 장비 요건 |
| **TDS-CALIB-001** | **테스트 데이터셋 명세서** | `docs/calibration/TDS-CALIB-001_Test_Dataset_Specification.md` | **QA / 개발자** | 알고리즘별 합성·실제 테스트 데이터 규격 · Golden Reference 관리 |

#### 규범 알고리즘 사양

| 문서 | 경로 | 설명 |
|------|------|------|
| ALG-SPEC-001 (규범) | `.moai/specs/xpe-algorithm-spec-deepsync.md` | 모든 알고리즘의 최종 권위 문서 |
| 고스트 보정 SRS | `docs/ghost-correction/srs_ghost_correction.md` | 3단계 래그 보정 상세 요건 |
| 교차 검증 보고서 | `.moai/specs/SPEC-XPE-MASTER/cross-verification-report.md` | PRD ↔ SRS ↔ 구현 교차검증 |

#### 문서 간 의존성 요약

```
PRD (원본)
  └─▶ SRS-CALIB-001 (요건 명세)
        ├─▶ SAD-CALIB-001 (아키텍처 설계)
        ├─▶ SHA-CALIB-001 (위험 분석)
        └─▶ RTM-CALIB-001 (추적 행렬)
              ├─▶ IAP-CALIB-001 (취득 절차 — 테스트 입력 생성)
              └─▶ TDS-CALIB-001 (테스트 데이터 명세 — 테스트 케이스 정의)
```

---

*캘리브레이션 모듈 README v1.1.0 끝*
