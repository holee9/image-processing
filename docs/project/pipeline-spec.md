# X-Ray Image Processing Pipeline Specification (X선 이미지 처리 파이프라인 사양)

**Document ID**: PIPE-SPEC-001  
**Version**: 1.3.0  
**Date**: 2026-04-14  
**Project**: ImageProcTest — X-Ray Image Processing Engine (Modular DLL Architecture)  
**Changelog**: v1.1.0 -> v1.2.0: 심층 연구 교차검증 수행. 전처리 순서 연구로 검증됨. Multi-gain 모델을 stage (2)에 통합. Tier 2/3 ghost escalation 시간 예산 명확화. Calibration drift 감지 단계 참고사항 추가. EI-0 단계 할당 해결. v1.2.0 -> v1.3.0: Section 1A (데이터 흐름, 종속성 매트릭스, 종속성 유형을 포함한 종속성 그래프) 및 Section 1B (분류, 의사결정 흐름도, 설정 인터페이스, 안전 제약, 형식 경계 분석, 진단 모드를 포함한 우회(Bypass) 정책) 추가.

---

## 1. Pipeline Overview (파이프라인 개요)

C# GUI 애플리케이션 `ImageProcTest`는 17-단계 이미지 처리 파이프라인을 오케스트레이션합니다. 각 단계는 런타임에 로드되는 모듈식 DLL로 구현됩니다. 실행은 DLL 가용성에 따라 세 단계(phase)로 나뉩니다.

규범적 알고리즘 소유권, 품질 게이트, DeepSync 충돌 해결은 `.moai/specs/xpe-algorithm-spec-deepsync.md`에 정의되어 있습니다.

### 1.1 Standard Pipeline Sequence

```
Raw Frame
  -> (1)  Offset Correction
  -> (2)  Gain Correction
  -> (3)  Defect Correction
  -> (4)  Ghost Artifact Removal
  -> (5)  Log Transform
  -> (5a) Body Part Recognition      [Phase 3]
  -> (5b) Collimation Detection      [Phase 2 baseline, Phase 3 AI refinement]
  -> (6)  Noise Reduction
  -> (7)  Contrast Enhancement
  -> (8)  Edge Enhancement
  -> (9)  GSVG (Grid Suppression / Virtual Grid)  [Phase 2]
  -> (10) Multiscale Processing      [Phase 2]
  -> (11) Fractional Processing      [Phase 2]
  -> (12) Image Stitching            [Phase 3, conditional]
  -> (13) Bone Suppression           [Phase 3]
  -> (14) Modality LUT
  -> (15) VOI LUT
  -> (16) Presentation LUT
  -> (17) DICOM Write
```

### 1.2 Enhanced Pre-Processing Order (강화된 전처리 순서)

전처리 단계의 연구 검증 순서입니다. 각 단계는 물리적 원칙과 동료 검증 문헌에 대해 검증되었습니다(ALG-SPEC-001 v3.0.0-ds2 Section 4.2 참조).

```
(0)   CalibManager Load              [startup-only, 200ms budget]
(0.5) Readout Artifact Validation    [non-mutating, flag + alert only]
(0.7) Temperature Compensation       [exponential dark current model, EP2148500A1]
(1)   Offset Correction              [I_corr = I_raw - I_dark, dynamic interpolation]
(1.5) Nonlinearity Correction        [BEFORE gain: linearize response for valid normalization]
(2)   Gain Correction                [flat-field normalization + multi-gain polynomial internal]
(2.5) Binning Correction             [conditional — only if binning mode active]
(3)   Defect Correction              [edge-aware interpolation, BPM-based + runtime detection]
(4)   Ghost Artifact Removal         [3-tier: LTI -> Exposure-Weighted -> NLCSC]
```

**순서 설정 근거** (연구로 검증됨):
- Nonlinearity (1.5) BEFORE Gain (2): 이득(gain) 정규화는 선형 검출기 반응을 가정합니다. 선형화는 flat-field 보정(NUC 2점 캘리브레이션 원칙)보다 먼저 수행되어야 합니다.
- Defect (3) AFTER Gain (2): 이득 보정된 균일한 배경이 결함 감지 및 보간 품질을 향상시킵니다.
- Ghost/Lag (4) LAST: NLCSC는 노출 이력과 비교하기 위해 완전히 보정된 현재 프레임이 필요합니다.

**Multi-gain 모델**: Multi-gain 다항식 보정 G(x,y,E) = sum(c_k * E^k)는 stage (2)의 내부입니다. 노출 수준별 이득 맵 선택은 xpe_gain_correct() 내에서 처리됩니다. 별도의 파이프라인 단계가 아닙니다.

**Calibration drift 모니터링**: 런타임 calibration drift 평가(온도 델타, 경과 시간, flat-field 잔차)는 stage (0) 시작 시 비동기적으로 실행되며, 유휴 중에 주기적으로 실행됩니다. 프레임당 지연 시간을 증가시키지 않습니다.

---

## 1A. Pre-Processing Dependency Graph (전처리 종속성 그래프)

### 1A.1 Data Flow and Dependency Chain (데이터 흐름 및 종속성 체인)

```
                          ┌──────────────────┐
                          │  (0) CalibManager │
                          │  Load (startup)   │
                          └──────┬───────────┘
                     ┌───────────┼───────────────────────┐
                     │           │                       │
              ┌──────▼──┐  ┌────▼────┐  ┌──────────────▼───────┐
              │offsetMap │  │ gainMap │  │ BPM (defectMap)      │
              └──────┬──┘  └────┬────┘  │ + NLCSC coefficients │
                     │          │       └──────────┬───────────┘
  ┌──────────────────┼──────────┼──────────────────┤
  │                  │          │                   │
  ▼                  │          │                   │
┌──────────────┐     │          │                   │
│ (0.5) Readout│     │          │                   │
│  Validation  │  [non-mutating, advisory]          │
│  uint16 → uint16   │          │                   │
└──────┬───────┘     │          │                   │
       │             │          │                   │
       ▼             │          │                   │
┌──────────────┐     │          │                   │
│ (0.7) Temp   │     │          │                   │
│ Compensation │  [temperature metadata required]   │
│  uint16 → uint16   │          │                   │
└──────┬───────┘     │          │                   │
       │             │          │                   │
       ▼             ▼          │                   │
┌──────────────────────┐        │                   │
│ (1) Offset Correct   │◄───offsetMap               │
│ I_corr = I_raw-I_dark│        │                   │
│  uint16 → uint16     │  [MANDATORY]               │
└──────┬───────────────┘        │                   │
       │                        │                   │
       ▼                        │                   │
┌──────────────────────┐        │                   │
│(1.5) Nonlinearity    │        │                   │
│ LUT/polynomial       │        │                   │
│  uint16 → uint16     │ [CONDITIONAL bypass]       │
└──────┬───────────────┘        │                   │
       │                        │                   │
       ▼                        ▼                   │
┌──────────────────────────────────┐                │
│ (2) Gain Correction              │◄───gainMap     │
│ I_corr = I_off / G(x,y)         │                │
│ uint16 → float32 FORMAT BOUNDARY │                │
│         [MANDATORY]              │                │
└──────┬───────────────────────────┘                │
       │                                            │
       ▼                                            │
┌──────────────────────┐                            │
│(2.5) Binning Correct │                            │
│  float32 → float32   │ [CONDITIONAL: binning only]│
└──────┬───────────────┘                            │
       │                                            │
       ▼                                            ▼
┌──────────────────────────────────┐
│ (3) Defect Correction            │◄───BPM
│ Interpolate bad pixels           │
│  float32 → float32              │  [CONDITIONAL bypass]
└──────┬───────────────────────────┘
       │
       ▼
┌──────────────────────────────────┐
│ (4) Ghost / Lag Correction       │◄───exposureHistory + NLCSC coefficients
│ 3-tier: LTI → Weighted → NLCSC  │
│  float32 → float32              │  [CONDITIONAL bypass]
└──────┬───────────────────────────┘
       │
       ▼
  [To Enhancement Domain: stage (5) Log Transform]
```

### 1A.2 Dependency Matrix (종속성 매트릭스)

각 셀은 행(ROW) 단계가 열(COLUMN) 단계에 의존하는지 여부를 나타냅니다.

| Stage ↓ depends on → | (0) Calib | (0.5) Readout | (0.7) Temp | (1) Offset | (1.5) Nonlin | (2) Gain | (2.5) Bin | (3) Defect | (4) Ghost |
|----------------------|:---------:|:-------------:|:----------:|:----------:|:------------:|:--------:|:---------:|:----------:|:---------:|
| **(0) CalibManager** | — | | | | | | | | |
| **(0.5) Readout**    | | — | | | | | | | |
| **(0.7) Temp**       | | | — | | | | | | |
| **(1) Offset**       | DATA | | | — | | | | | |
| **(1.5) Nonlin**     | DATA | | | ORDER | — | | | | |
| **(2) Gain**         | DATA | | | ORDER | ORDER | — | | | |
| **(2.5) Binning**    | | | | | | FORMAT | — | | |
| **(3) Defect**       | DATA | | | | | FORMAT | | — | |
| **(4) Ghost**        | DATA | | | | | FORMAT | | ORDER | — |

범례:
- **DATA**: 열 단계에 의해 로드된 캘리브레이션 데이터가 필요합니다
- **ORDER**: 정확성(물리적 제약) 때문에 열 단계 후에 실행되어야 합니다
- **FORMAT**: stage (2)에서 수행된 형식 변환(uint16 -> float32)에 따라 다릅니다
- 공백: 종속성 없음

### 1A.3 Dependency Types (종속성 유형)

| Type | Description (설명) | Violation Consequence (위반 결과) |
|------|-------------|----------------------|
| **DATA** | 시작 시 로드된 캘리브레이션 데이터가 필요합니다 | Hard fail: `XPE_ERR_NOT_INITIALIZED` 또는 `XPE_ERR_CALIBRATION_EXPIRED` |
| **ORDER** | 실행 순서에 대한 물리적/수학적 제약 | Silent quality degradation: 부정확한 보정 결과 |
| **FORMAT** | stage (2)에서 생성된 데이터 유형 경계(uint16 -> float32) | Hard fail: 유형 불일치 충돌 또는 버퍼 손상 |
| **STATE** | 런타임 누적 상태(노출 이력) | Graceful degradation: 계층 다운그레이드 또는 우회 |

---

## 1B. Pre-Processing Stage Bypass (On/Off) Policy (전처리 단계 우회(Bypass) 정책)

### 1B.1 Bypass Classification (우회 분류)

각 전처리 단계는 다음 세 가지 우회 범주 중 하나로 분류됩니다:

| Category | Symbol | Description (설명) |
|----------|:------:|-------------|
| **MANDATORY** | `M` | 우회 불가. 없으면 파이프라인이 hard fail 합니다. |
| **CONDITIONAL** | `C` | 특정하고 문서화된 조건 하에서 우회할 수 있습니다. |
| **ADVISORY** | `A` | Non-mutating 단계. 우회는 이미지 데이터에 영향을 주지 않습니다. |

### 1B.2 Stage Bypass Table (단계 우회 테이블)

| Stage | Category | Can Bypass? | Bypass Condition (우회 조건) | Safety Impact (안전 영향) | Flag on Bypass |
|-------|:--------:|:-----------:|------------------|:-------------:|---------------|
| **(0) CalibManager** | `M` | NO | N/A — 파이프라인이 시작될 수 없음 | FATAL | N/A |
| **(0.5) Readout Validation** | `A` | YES | 항상 안전하게 건너뛸 수 있습니다 | NONE | 플래그 미설정 (검증 건너뜀) |
| **(0.7) Temp Compensation** | `C` | YES | 온도 센서 미사용 또는 검출기가 명목 온도(25C +/-2C)에 있음 | LOW | `XPE_FLAG_TEMP_COMPENSATED` 미설정 |
| **(1) Offset Correction** | `M` | NO | N/A — 암전류 바이어스가 모든 하위 단계를 손상시킴 | CRITICAL | offsetMap 부재 시 Hard fail |
| **(1.5) Nonlinearity** | `C` | YES | 검출기 프로필이 선형 반응을 선언 (config에서 `panel.linear = true`) | LOW | `XPE_FLAG_NONLINEARITY_CORRECTED` 미설정 |
| **(2) Gain Correction** | `M` | NO | N/A — uint16->float32 형식 변환 + 픽셀 정규화 제공 | CRITICAL | gainMap 부재 시 Hard fail |
| **(2.5) Binning** | `C` | YES | Binning 모드 비활성(`binningMode == 1` 즉, 1x1 native) | NONE | `XPE_FLAG_BINNING_CORRECTED` 미설정 |
| **(3) Defect Correction** | `C` | YES | BPM이 비어있음(결함 픽셀 감지 안함) 또는 진단/raw-export 모드 | MEDIUM | `XPE_FLAG_DEFECT_CORRECTED` 미설정 |
| **(4) Ghost Correction** | `C` | YES | 단일 샷 모드(이전 노출 없음) 또는 검출기 전원 온 후 첫 프레임 또는 노출 이력 비어있음 | MEDIUM | `XPE_FLAG_GHOST_CORRECTED` 미설정 |

### 1B.3 Bypass Decision Flowchart (우회 의사결정 흐름도)

```
각 전처리 프레임에 대해:

  (0) CalibManager 로드됨?
       NO  → 파이프라인 시작 중단
       YES ↓

  (0.5) Readout Validation이 config에서 활성화?
       NO  → 건너뛰기 (advisory, 이미지 변경 없음)
       YES → xpe_validate_readout_artifact() 실행
              If artifactScore > CRITICAL_THRESHOLD → 프레임 중단
              If artifactScore > WARN_THRESHOLD → 플래그 + 계속
              Else → 계속 ↓

  (0.7) 온도 센서 사용 가능?
       NO  → 건너뛰기 (명목값 25C 사용, 알림 발송)
       YES → abs(detectorTemp - nominalTemp) > 2.0C?
              NO  → 건너뛰기 (허용범위 내, 보정 불필요)
              YES → xpe_temp_compensate() 실행 ↓

  (1) Offset 보정 → 항상 실행
       offsetMap 로드됨? NO → HARD FAIL
       xpe_offset_correct() 실행 ↓

  (1.5) 검출기 프로필 선형?
       YES → 건너뛰기 (config에서 panel.linear = true)
       NO  → xpe_nonlinearity_correct() 실행 ↓

  (2) Gain 보정 → 항상 실행
       gainMap 로드됨? NO → HARD FAIL
       xpe_gain_correct() 실행
       [uint16 → float32 변환이 여기서 일어남] ↓

  (2.5) binningMode == 1 (native)?
       YES → 건너뛰기 (binning 미활성)
       NO  → xpe_binning_correct(binningMode) 실행 ↓

  (3) BPM에 항목이 0개 AND 런타임 감지 비활성화?
       YES → 건너뛰기 (보정할 결함 없음)
       NO  → xpe_defect_correct() 실행
              런타임 감지가 활성화되면:
                xpe_defect_detect_runtime() → 정적 BPM과 병합 ↓

  (4) 노출 이력 비어있음 OR 단일 샷 모드?
       YES → 건너뛰기 (보정할 lag 없음, 첫 프레임)
       NO  → xpe_ghost_correct() 실행
              필요에 따라 Tier 1 → 2 → 3 자동 escalation ↓

  → stage (5) Log Transform으로 계속
```

### 1B.4 Bypass Configuration Interface (우회 설정 인터페이스)

우회 제어는 `xpe_configure()` JSON 및 프레임별 메타데이터를 통해 노출됩니다:

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
      "interpolation_mode": "bilinear"
    },
    "ghost_correction": {
      "enabled": true,
      "max_tier": 3,
      "bypass_single_shot": true,
      "min_history_frames": 1
    }
  }
}
```

### 1B.5 Bypass Safety Constraints (우회 안전 제약)

| Constraint ID | Rule | Rationale (근거) |
|--------------|------|-----------|
| **BYP-SAFE-001** | Offset 보정(stage 1)은 설정을 통해 우회할 수 없어야 합니다. | 암전류 바이어스는 항상 존재하며 모든 하위 처리를 손상시킵니다. |
| **BYP-SAFE-002** | Gain 보정(stage 2)은 설정을 통해 우회할 수 없어야 합니다. | Gain 단계는 모든 하위 단계에서 필요로 하는 critical uint16 -> float32 형식 변환을 수행합니다. |
| **BYP-SAFE-003** | 모든 CONDITIONAL 단계가 우회될 때, 메타데이터에서 해당하는 `XPE_FLAG_*` 비트를 설정하지 않아야 합니다. | 하위 단계 및 QA 시스템은 어떤 보정이 적용되었는지 감지할 수 있어야 합니다. |
| **BYP-SAFE-004** | Ghost 보정 우회는 `xpe_ghost_reset()` 후 첫 번째 프레임 또는 검출기 전원 온 시 자동으로 트리거되어야 합니다. | 노출 이력이 없으므로 보정을 시도하면 garbage가 생성됩니다. |
| **BYP-SAFE-005** | Defect 보정 우회는 BPM에 > 0개 항목이 있고 우회가 사용자 요청인 경우 경고 알림을 발송해야 합니다. | 알려진 결함 보정을 의도적으로 건너뛰는 것은 비정상이며 로깅되어야 합니다. |
| **BYP-SAFE-006** | Nonlinearity 우회는 검출기 프로필이 명시적으로 `panel.linear = true`를 선언할 때만 허용되어야 합니다. | 프로필 검증 없는 silent 우회는 미감지 nonlinearity 아티팩트의 위험이 있습니다. |
| **BYP-SAFE-007** | 모든 우회 결정은 진단 JSON에 단계 이름, 이유, 프레임 ID와 함께 로깅되어야 합니다. | IEC 62304 준수 및 사후(post-hoc) QA 분석을 위한 추적성 |
| **BYP-SAFE-008** | 진단/raw-export 모드에서, (0), (1), (2) 제외한 모든 단계는 우회될 수 있습니다. Offset과 gain은 필수 상태로 유지됩니다. | Raw export도 유효한 출력을 위해 형식 변환 및 기본 보정이 필요합니다. |

### 1B.6 Format Boundary Impact (형식 경계 영향)

Stage (2) Gain Correction은 전처리 파이프라인의 **유일한 형식 경계**입니다:

```
  Stages (0.5) → (1.5):  uint16 domain
                          ───────────────
  Stage (2):              uint16 → float32 conversion  [FORMAT BOUNDARY]
                          ───────────────
  Stages (2.5) → (4):    float32 domain
```

이는 우회에 대한 critical 의미를 갖습니다:

- Stages (0.5), (0.7), (1), (1.5)는 `uint16` 데이터에서 작동합니다. 이들의 우회는 데이터 형식에 영향을 주지 않습니다.
- Stage (2)는 우회될 수 없습니다. stages (2.5)-(4)가 `float32` 입력을 필요로 하기 때문입니다. gain normalization이 필요하지 않더라도(가설적), 형식 변환은 발생해야 합니다.
- Stages (2.5), (3), (4)는 `float32` 데이터에서 작동합니다. 이들의 우회는 형식 안전합니다.

### 1B.7 Diagnostic / Raw Export Mode (진단/Raw Export 모드)

특수한 `raw_export` 모드는 디버깅 및 QA를 위해 최대 우회를 허용합니다:

| Stage | Normal Mode | Raw Export Mode |
|-------|:-----------:|:---------------:|
| (0) CalibManager | Mandatory | Mandatory |
| (0.5) Readout Validation | Configurable | SKIP |
| (0.7) Temperature Compensation | Configurable | SKIP |
| (1) Offset Correction | Mandatory | **Mandatory** (암전류 바이어스 제거) |
| (1.5) Nonlinearity | Configurable | SKIP |
| (2) Gain Correction | Mandatory | **Mandatory** (형식 변환) |
| (2.5) Binning | Conditional | SKIP |
| (3) Defect Correction | Configurable | SKIP |
| (4) Ghost Correction | Configurable | SKIP |

raw export 모드에서, 두 개의 필수 보정(offset + gain)만 적용되어 외부 분석 도구에 적합한 최소 보정된 float32 프레임을 생성합니다.

---

## 2. ImageBuffer State Transitions (ImageBuffer 상태 전환)

### 2.1 Format Transitions (형식 전환)

| Stage Range | Buffer Format | Notes (참고사항) |
|-------------|--------------|-------|
| Raw Frame input | `uint16` | 센서 ADC 출력 |
| After stage (2) Gain Correction | `float32` | 부동점 산술을 위해 변환됨 |
| After stage (16) Presentation LUT | `uint16` | 출력/디스플레이를 위해 다시 변환됨 |

### 2.2 Buffer Size (버퍼 크기)

| Parameter | Value |
|-----------|-------|
| Maximum dimensions (최대 치수) | 4096 x 4096 pixels |
| Typical dimensions (일반적인 치수) | 3072 x 3072 pixels |
| float32 buffer size (3072x3072) | ~37.7 MB |
| uint16 buffer size (3072x3072) | ~18.9 MB |

### 2.3 Metadata Flags (메타데이터 플래그)

파이프라인은 `xpe_types.h` / `api-spec.md`에 정의된 안정적인 `XPE_FLAG_*` 비트필드를 사용합니다. 모든 단계가 persistent metadata 비트를 필요로 하지는 않습니다; orchestrator의 단계 순서는 여전히 offset/log 변환에 대한 무효한 재진입을 방지합니다.

| Flag | Set by Stage | Purpose (목적) |
|------|-------------|---------|
| `XPE_FLAG_READOUT_VALIDATED` | (0.5) Readout Validation | Raw frame 무결성이 보정 전에 확인됨 |
| `XPE_FLAG_TEMP_COMPENSATED` | (0.7) Temperature Compensation | Temperature LUT/다항식 보정이 적용됨 |
| `XPE_FLAG_GAIN_CORRECTED` | (2) Gain | Float32 변환 및 gain normalization 완료 |
| `XPE_FLAG_BINNING_CORRECTED` | (2.5) Binning | Binning 모드 보정이 적용됨 |
| `XPE_FLAG_DEFECT_CORRECTED` | (3) Defect | BPM 보정 완료 |
| `XPE_FLAG_GHOST_CORRECTED` | (4) Ghost | Ghost 계층이 적용되고 기록됨 |
| `XPE_FLAG_COLLIMATION_DETECTED` | (5b) Collimation | ROI 좌표가 metadata sidecar에 저장됨 |
| `XPE_FLAG_STITCHED` | (12) Stitching | Multi-exposure 병합 완료 |
| `XPE_FLAG_BONE_SUPPRESSED` | (13) Bone Suppression | Bone suppression 결과 생성됨 |
| `XPE_FLAG_GSVG_SKIPPED` | (9) GSVG fallback | SAFE-003 경로 선택됨, 원본 버퍼 보존, 실패 사유가 alert queue를 통해 발송됨 |

---

## 3. Branching Points (분기점)

6개의 조건부 분기점이 런타임에 처리 경로를 결정합니다.

### BP-1: Body Part Recognition (Stage 5a)

- **Trigger**: Phase 3 AI 컴포넌트가 로드되었을 때만 Log Transform 후에 실행됩니다.
- **Input**: Log 변환된 float32 버퍼.
- **Output**: Body part 라벨 (예, `CHEST`, `HAND`, `SPINE`) + 신뢰도 점수.
- **Downstream effect**: stages (6)–(11)의 알고리즘 파라미터를 설정합니다: 노이즈 감소 강도, contrast enhancement 곡선, edge sharpening 계수.
- **Fallback**: Phase 3을 사용할 수 없거나 인식에 실패하면 기본 파라미터 세트가 사용됩니다.

### BP-2: Collimation Detection (Stage 5b)

- **Trigger**: Phase 2 DLL이 로드되었을 때 Log Transform 후에 실행됩니다. Phase 3은 baseline ROI를 개선할 수 있지만 Phase 2는 필수 결정론적 경로로 유지됩니다.
- **Input**: Log 변환된 float32 버퍼.
- **Output**: ROI bounding box가 orchestration sidecar / result 객체에 저장됩니다, `XpeImageMetadata`에는 아닙니다.
- **Downstream effect**:
  - Exposure Index (EI) 계산이 ROI로 제한됩니다.
  - Display windowing이 ROI 영역으로 제한됩니다.
- **Fallback**: 감지에 실패하면 전체 이미지가 EI 및 디스플레이에 사용됩니다.

### BP-3: GSVG Grid Detection (Stage 9)

- **Trigger**: Phase 2 DLL이 로드되었을 때 GSVG 단계 내에서 실행됩니다.
- **Decision**:
  - Grid 감지됨 → `GridSuppression` 경로 (anti-scatter grid 아티팩트 제거).
  - Grid 미감지됨 → `VirtualGrid` 경로 (디스플레이 선호도를 위해 합성 grid 텍스처 추가).
- **Input format**: `float32` 또는 `uint16` 허용; GSVG는 필요에 따라 내부 변환을 수행합니다.
- **Error behavior**: GSVG 처리 오류 시, 원본 미수정 버퍼가 반환됩니다. GSVG SAFE-003 참조.

### BP-4: Ghost Tier Escalation (Stage 4)

자동 escalation을 갖춘 3-계층 ghost 아티팩트 제거. Starman et al. 2012 (PMC3465354) 및 Pang et al. 2006 (PMC5722609)에 의해 연구로 검증됨.

| Tier | Algorithm | Trigger Condition (트리거 조건) | Performance Target (성능 목표) | Time Budget |
|------|-----------|-------------------|-------------------|-------------|
| Tier 1 | LTI multi-exponential (N=4) deconvolution | Default 경로. Residual < threshold_1 | 1st frame lag < 0.5% | 150 ms |
| Tier 2 | Exposure-weighted LTI with intensity-matched 계수 | Tier 1 불충분: artifact >= threshold_1 | 1st frame lag < 0.35% | 190 ms |
| Tier 3 | NLCSC with signal-dependent 계수 | Tier 2 불충분: artifact >= threshold_2 | 1st frame lag <= 0.29% | 240 ms |

**Lag vs Ghosting**: Lag (charge trapping에서의 신호 지속성, 1-4% 크기)와 ghosting (이전 노출로 인한 민감도 변화, ~0.1% 크기)은 동일한 단계에서 보정되는 서로 다른 현상입니다. Lag은 임상 용량에서 indirect-conversion FPD에 대해 지배적입니다.

- **State required**: `exposureHistory` (이전 8개 프레임의 ring buffer, ~150 MB) + NLCSC 보정 계수.
- **Fallback**: Tier 1은 항상 사용 가능; Tier 2/3은 충분한 노출 이력 항목이 필요합니다.
- **Tier downgrade**: 노출 이력 또는 계수가 불충분할 때 진단에서 명시적이어야 합니다.

### BP-5: Image Stitching (Stage 12)

- **Trigger**: Multi-exposure acquisition이 수행되었고 Phase 3 AI 컴포넌트가 로드되었을 때만 활성화됩니다.
- **Input**: 별도 노출에서의 두 개 이상의 float32 프레임 버퍼.
- **Output**: 확대된 시야를 갖춘 단일 병합된 float32 버퍼.
- **Skip condition**: 단일 노출 acquisition — 단계는 no-op이고, 버퍼는 미수정으로 통과합니다.

### BP-6: DL Processing Toggle (Stage 13)

- **Reference**: SRS-SAFE-009.
- **Trigger**: `ImageProcTest` GUI의 사용자 제어 토글.
- **States**:
  - `DL_ENABLED`: Bone suppression 추론이 ONNX 모델을 사용하여 실행됩니다.
  - `DL_DISABLED`: Stage 13이 완전히 건너뛰어짐; 버퍼는 미수정으로 통과합니다.
- **DLL requirement**: `DL_ENABLED`이 작동하려면 Phase 3 `xpe_ai.dll`이 로드되어야 합니다.

---

## 4. Stateful vs. Stateless Stage Classification (상태 유지 vs 상태 비유지 단계 분류)

### Stateful Stages (상태 유지 단계)

호출 간에 내부 상태를 유지하거나 외부 캘리브레이션 데이터에 의존하는 단계.

| Stage | State Dependency | State Type (상태 유형) |
|-------|-----------------|------------|
| (1) Offset Correction | `calibMap` (offset 캘리브레이션 맵) | Calibration 파일, 시작 시 로드됨 |
| (2) Gain Correction | `calibMap` (gain 캘리브레이션 맵) | Calibration 파일, 시작 시 로드됨 |
| (3) Defect Correction | BPM (Bad Pixel Map) | Calibration 파일, 시작 시 로드됨 |
| (4) Ghost Removal | `exposureHistory` ring buffer + NLCSC 계수 | Runtime 누적 |
| (5a) Body Part Recognition | ONNX 모델 weights | 모델 파일, Phase 2와 함께 로드됨 |
| (9) GSVG | `scatterLUT` (scatter lookup table) | Calibration 파일, Phase 2와 함께 로드됨 |
| (13) Bone Suppression | ONNX 모델 weights | 모델 파일, Phase 3과 함께 로드됨 |
| (16) / Display | LUT presets (사용자 선택 가능) | Configuration, 사용자 설정 가능 |

### Stateless Stages (상태 비유지 단계)

persistent 상태가 없는 단계; 출력은 입력의 결정론적 함수입니다.

| Stage | Notes (참고사항) |
|-------|-------|
| (0.5) Readout Artifact Validation | 픽셀 패턴을 검증, 상태 없음 |
| (0.7) Temperature Compensation | 현재 프레임 메타데이터의 센서 온도 판독값 사용 |
| (1.5) Nonlinearity Correction | 캘리브레이션 로드에서 고정된 다항식 계수 사용 |
| (2.5) Binning Correction | Conditional; 고정된 보정 계수 |
| (5) Log Transform | 수학적 변환, 상태 없음 |
| (5b) Collimation Detection | Stateless 프레임별 감지 |
| (6) Noise Reduction | 필터 기반, 상태 없음 |
| (7) Contrast Enhancement | 곡선 기반, 상태 없음 |
| (8) Edge Enhancement | 커널 기반, 상태 없음 |
| (10) Multiscale Processing | Decomposition/reconstruction, 상태 없음 |
| (11) Fractional Processing | 수학적, 상태 없음 |
| (12) Stitching | 프레임 결합, persistent 상태 없음 |
| (14) Modality LUT | LUT 적용, 상태 없음 |
| (15) VOI LUT | LUT 적용, 상태 없음 |
| (17) DICOM Write | File I/O, processing 상태 없음 |

---

## 5. Performance Budget (성능 예산)

**Reference**: SRS-PERF-001, SRS-PERF-002

### 5.1 Phase Time Budgets (단계 시간 예산)

| Scope | Time Budget | Status (상태) |
|-------|------------|--------|
| Startup calibration load | 200 ms one-time | `CalibManager Load`는 startup 전용이며 프레임당 지연 예산에서 제외됨 |
| Pre-processing subset (0.5–4) | 500 ms / frame | 추정 ~390 ms (Tier 1) / ~430–480 ms (Tier 2/3) |
| Phase 1 per-frame total | 3000 ms / frame | 추정 ~1005 ms (margin: ~1995 ms) |
| Phase 2 additions | Phase 1 + optional 모듈 | 추정 ~2205–2505 ms 전체 |
| Phase 3 additions | Phase 2 + AI 추론 | 추정 ~2655–2955 ms 전체 |

### 5.2 Per-Stage Time Allocation

| Stage | Allocated (ms) | Phase 1 Estimate (ms) | Phase 2 Estimate (ms) |
|-------|---------------|----------------------|----------------------|
| (0) CalibManager Load | 200 | 200 (startup-only) | — |
| (0.5) Readout Artifact Validation | 15 | 10 | — |
| (0.7) Temperature Compensation | 10 | 5 | — |
| (1) Offset Correction | 60 | 55 | — |
| (1.5) Nonlinearity Correction | 25 | 20 | — |
| (2) Gain Correction | 60 | 55 | — |
| (2.5) Binning Correction | 15 | 10 | — |
| (3) Defect Correction | 110 | 95 | — |
| (4) Ghost Removal (Tier 1: LTI) | 150 | 140 | — |
| (4) Ghost Escalation Tier 2 (Exposure-Weighted) | +40 | +40 | — |
| (4) Ghost Escalation Tier 3 (NLCSC) | +90 | +90 | — |
| (5) Log Transform | 40 | 35 | — |
| (5a) Body Part Recognition | 300 | — | 280 |
| (5b) Collimation Detection | 140 | — | 130 |
| (6) Noise Reduction | 180 | 170 | — |
| (7) Contrast Enhancement | 130 | 120 | — |
| (8) Edge Enhancement | 90 | 80 | — |
| (9) GSVG | 400 | — | 380 |
| (10) Multiscale Processing | 250 | — | 230 |
| (11) Fractional Processing | 200 | — | 180 |
| (12) Stitching (conditional) | 300 | — | 0–300 |
| (13) Bone Suppression | 500 | — | 450 |
| (14) Modality LUT | 25 | 20 | — |
| (15) VOI LUT | 25 | 20 | — |
| (16) Presentation LUT | 25 | 20 | — |
| (17) DICOM Write | 150 | 150 | — |
| **Pre-processing subtotal (0.5–4)** | **500** | **~390** | — |
| **Phase 1 per-frame total** | **3000** | **~1005** | — |
| **Phase 2 total** | **Phase 1 + optional** | — | **~2205–2505** |
| **Phase 3 total** | **Phase 2 + AI** | — | **~2655–2955** |

---

## 6. GSVG Integration Point (GSVG 통합점)

### 6.1 Position in Pipeline (파이프라인의 위치)

GSVG는 stage (9)에서 실행되며, 기본 이미지 enhancement stages (6)–(8) 후이고 advanced 처리 단계 (10)–(11) 전입니다.

```
... -> (8) Edge Enhancement -> (9) GSVG -> (10) Multiscale -> ...
```

### 6.2 Interface (인터페이스)

| Property | Value |
|----------|-------|
| Input formats | `float32` 또는 `uint16` |
| Format handling | 내부 변환이 자동으로 적용됨 |
| Output format | Input 형식과 일치 |
| Grid detection | 자동 (GridSuppression vs VirtualGrid 경로 결정) |
| Error behavior | 원본 미수정 버퍼 반환 (GSVG SAFE-003) |

### 6.3 GSVG SAFE-003 Contract

GSVG 처리 중 내부 오류 발생 시:
1. 전체 컨텍스트(프레임 ID, 오류 유형, 타임스탬프)와 함께 오류를 로깅합니다.
2. 원본 `ImageBuffer`를 파이프라인으로 미수정으로 반환합니다.
3. 메타데이터 플래그 `XPE_FLAG_GSVG_SKIPPED`를 오류 코드로 설정합니다.
4. 파이프라인이 정상적으로 계속 진행됩니다 — GSVG 실패는 non-fatal입니다.

---

## 7. Phase-Gated Stage Loading (단계별 제어 단계 로드)

### 7.1 DLL Phase Assignment (DLL 단계 할당)

| Phase | DLLs | Availability (가용성) |
|-------|------|-------------|
| Phase 1 | `xpe_common.dll`, `xpe_preprocess.dll`, `xpe_enhance_basic.dll`, `xpe_display.dll`, `xpe_dicom.dll` | REQUIRED — 이 없으면 파이프라인이 시작될 수 없습니다 |
| Phase 2 | `gsvg.dll`, `xpe_enhance_advanced.dll` | OPTIONAL — 부재 시 graceful 열화 |
| Phase 3 | `xpe_ai.dll`, `xpe_ai_worker.exe` | OPTIONAL — 부재 시 graceful 열화 |

### 7.2 DLL Loading Strategy (DLL 로드 전략)

```csharp
// Phase 1 — 필수, 실패 시 throw
var common = NativeLibrary.Load("xpe_common.dll");
var preprocess = NativeLibrary.Load("xpe_preprocess.dll");
var enhanceBasic = NativeLibrary.Load("xpe_enhance_basic.dll");
var display = NativeLibrary.Load("xpe_display.dll");
var dicom = NativeLibrary.Load("xpe_dicom.dll");

// Phase 2 — optional, 로그 및 열화
bool phase2Available = TryLoad("gsvg.dll", out var gsvg)
                    && TryLoad("xpe_enhance_advanced.dll", out var enhanceAdvanced);

// Phase 3 — optional, 로그 및 열화
bool phase3Available = TryLoad("xpe_ai.dll", out var ai);
// 활성화되면, xpe_ai_init()는 샌드박스된 동반 worker xpe_ai_worker.exe를 시작합니다.
```

### 7.3 Stage Execution per Phase Availability (단계 가용성별 단계 실행)

| Stage | Phase 1 Only | Phase 1+2 | Phase 1+2+3 |
|-------|:-----------:|:---------:|:-----------:|
| (0)–(4) Pre-processing | Yes | Yes | Yes |
| (5) Log Transform | Yes | Yes | Yes |
| (5a) Body Part Recognition | Skip | Skip | Yes |
| (5b) Collimation Detection | Skip | Yes (baseline) | Yes (baseline + optional AI refinement) |
| (6)–(8) Basic Enhancement | Yes | Yes | Yes |
| (9) GSVG | Skip | Yes | Yes |
| (10)–(11) Advanced Enhancement | Skip | Yes | Yes |
| (12) Stitching | Skip | Skip | Yes (conditional) |
| (13) Bone Suppression | Skip | Skip | Yes (if DL_ENABLED) |
| (14)–(17) LUT + DICOM | Yes | Yes | Yes |

---

## 8. Appendices (부록)

### Appendix A: Interface Summary (인터페이스 요약)

| Interface ID | Name | Caller -> Callee | Key Parameters |
|-------------|------|-----------------|---------------|
| IF-INT-001 | PreprocessInterface | GUI -> xpe_preprocess.dll | `ImageBuffer*`, `CalibData*`, `PreprocessFlags` |
| IF-INT-002 | EnhanceBasicInterface | GUI -> xpe_enhance_basic.dll | `ImageBuffer*`, `EnhanceParams`, `BodyPartLabel` |
| IF-INT-003 | EnhanceAdvancedInterface | GUI -> xpe_enhance_advanced.dll | `ImageBuffer*`, `AdvancedParams`, `MultiscaleConfig` |
| IF-INT-004 | DisplayInterface | GUI -> xpe_display.dll | `ImageBuffer*`, `LUTPreset`, `WindowLevel` |
| IF-GSVG-001 | GSVGInterface | GUI -> gsvg.dll | `ImageBuffer*`, `ScatterLUT*`, `GSVGMode` |
| IF-AI-001 | AIInterface | GUI -> xpe_ai.dll -> xpe_ai_worker.exe | `ImageBuffer*`, `ONNXModelHandle`, `InferenceConfig` |

### Appendix B: Checksum Verification Points (체크섬 검증점)

품질 체크섬 검증은 데이터 손상 또는 처리 오류를 감지하기 위해 5개 파이프라인 체크포인트에서 수행됩니다.

| Checkpoint | Location (위치) | Verification Target (검증 대상) |
|-----------|----------|-------------------|
| CK-1 | After raw frame acquisition | Raw 픽셀 데이터 무결성 (CRC-32) |
| CK-2 | After Gain Correction (stage 2) | float32 버퍼 범위 확인 (NaN/Inf 값 없음) |
| CK-3 | After Defect Correction (stage 3) | Bad pixel 개수가 예상 범위 내 |
| CK-4 | After Log Transform (stage 5) | Histogram 분포가 예상 범위 내 |
| CK-5 | After Presentation LUT (stage 16) | uint16 출력 범위 [0, 65535] 완전히 채워짐 |

### Appendix C: Memory Budget (메모리 예산)

파이프라인 단계별 peak 메모리 사용량 추정값.

| Phase Configuration | Peak Memory Usage |
|--------------------|------------------|
| Phase 1 only | ~190 MB |
| Phase 1 + Phase 2 | ~440 MB |
| Phase 1 + Phase 2 + Phase 3 | ~740 MB |

**메모리 분류 요소:**
- Raw uint16 프레임 버퍼: ~18.9 MB (3072x3072)
- Working float32 버퍼: ~37.7 MB (3072x3072)
- Calibration 맵 (offset + gain + BPM): ~60 MB
- GSVG scatter LUT: ~50 MB
- Ghost 노출 이력 (ring buffer, 8 프레임): ~150 MB
- Body Part ONNX 모델: ~80 MB
- Bone Suppression ONNX 모델: ~200 MB
- DICOM 출력 버퍼 + 메타데이터: ~25 MB

---

*End of Pipeline Specification*
