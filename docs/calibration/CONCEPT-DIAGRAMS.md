# Calibration Algorithm Concept Diagrams

**모듈**: `xpe_preprocess.dll` (Layer 1, Phase 1a)  
**버전**: 1.0  
**날짜**: 2026-04-19  
**목적**: 캘리브레이션 알고리즘의 개념도, 처리 메커니즘, 흐름도 시각화

---

## 목차

1. [전체 캘리브레이션 파이프라인](#1-전체-캘리브레이션-파이프라인)
2. [데이터 흐름 및 타입 변환](#2-데이터-흐름-및-타입-변환)
3. [CalibrationManager 초기화 시퀀스](#3-calibrationmanager-초기화-시퀀스)
4. [오프셋(암전류) 보정](#4-오프셋암전류-보정)
5. [게인(평탄화) 보정](#5-게인평탄화-보정)
6. [비선형성 보정](#6-비선형성-보정)
7. [결함 픽셀 보정](#7-결함-픽셀-보정)
8. [고스트/잔상 보정 (3-Tier)](#8-고스트잔상-보정-3-tier)
9. [온도 보상](#9-온도-보상)
10. [바이패스 결정 로직](#10-바이패스-결정-로직)
11. [캘리브레이션 데이터 생명주기](#11-캘리브레이션-데이터-생명주기)
12. [소프트웨어 유닛 분해도](#12-소프트웨어-유닛-분해도)

---

## 1. 전체 캘리브레이션 파이프라인

> **개념**: Raw 검출기 출력(uint16)을 9단계를 거쳐 보정된 임상 영상(float32)으로 변환

```mermaid
flowchart TD
    RAW["🔲 Raw Frame\nuint16 [3072×3072]\n14-16 bit ADC 출력"]
    META["📋 Detector Metadata\n온도 · kVp · SID · 바이닝 모드"]

    subgraph CALIB_MGR["CalibrationManager (SWU-1.5)"]
        LOAD["캘리브레이션 파일 로드\n.xpe_calib (오프셋/게인/BPM)"]
        EXPIRE["만료 검증\nCRC-32 + 타임스탬프"]
        LOAD --> EXPIRE
    end

    subgraph PIPELINE["보정 파이프라인"]
        T1["① 온도 보상\nSWU-1.6\n조건부"]
        T2["② 오프셋 보정\nSWU-1.1\n필수"]
        T3["③ 비선형성 보정\nSWU-1.7\n조건부"]
        T4["④ 게인 보정\nSWU-1.2\n필수\n⚡ uint16→float32"]
        T5["⑤ 바이닝 보정\nSWU-1.8\n조건부"]
        T6["⑥ 결함 보정\nSWU-1.3\n조건부"]
        T7["⑦ 고스트/잔상 보정\nSWU-1.4\n조건부"]
        T1 --> T2 --> T3 --> T4 --> T5 --> T6 --> T7
    end

    OUT["✅ 보정된 Frame\nfloat32 [3072×3072]\n임상 영상 품질"]
    NEXT["→ xpe_enhance_basic.dll\n(로그 변환, CLAHE 등)"]

    RAW --> CALIB_MGR
    META --> CALIB_MGR
    META --> PIPELINE
    CALIB_MGR --> PIPELINE
    T7 --> OUT
    OUT --> NEXT

    style RAW fill:#ffcccc,stroke:#cc0000
    style OUT fill:#ccffcc,stroke:#00cc00
    style T4 fill:#ffffcc,stroke:#cccc00
    style CALIB_MGR fill:#e8f4fd,stroke:#2196F3
```

**핵심 포인트**:
- `④ 게인 보정` 단계에서 **uint16 → float32** 형식 경계가 발생 (이후 모든 연산은 float32)
- 필수 단계: 오프셋 보정, 게인 보정
- 조건부 단계: 나머지 (패널 프로파일 + 획득 파라미터에 따라 활성화)

---

## 2. 데이터 흐름 및 타입 변환

> **개념**: 각 보정 단계에서의 데이터 타입, 값 범위, 형식 경계 시각화

```mermaid
flowchart LR
    subgraph INPUT["입력 도메인 (uint16)"]
        R["Raw uint16\n0~65535 ADU\n14-16 bit"]
        OFF["오프셋 맵\nuint16\n3072×3072"]
        NL["비선형 LUT\nuint16→uint16\n4096 entries"]
    end

    subgraph UINT16_OPS["uint16 연산 구간"]
        direction TB
        S1["오프셋 차감\nI_corr = I_raw - I_dark\n클램프 → 0 이상"]
        S2["비선형성 보정\nI_lin = LUT[I_raw]\n또는 다항식 f(I)"]
        S1 --> S2
    end

    BOUNDARY["⚡ 형식 경계\nuint16 → float32\n게인 보정 시"]

    subgraph FLOAT32_OPS["float32 연산 구간"]
        direction TB
        S3["게인 정규화\nI_norm = I_corr / G(x,y)\n다중 SID 지원"]
        S4["바이닝 보정\n모드별 게인/균일성 조정"]
        S5["결함 보간\n이웃/쌍선형/중앙값"]
        S6["고스트 제거\nLTI 역합성곱\n또는 NLCSC"]
        S3 --> S4 --> S5 --> S6
    end

    subgraph GAIN_DATA["게인 데이터"]
        G["게인 맵\nfloat32\n범위 [0.1, 10.0]"]
        BPM["결함 픽셀 맵\nuint8\n0=정상, 1-4=결함유형"]
    end

    OUT["보정 완료\nfloat32\n임상 사용 가능"]

    R --> INPUT
    INPUT --> UINT16_OPS
    UINT16_OPS --> BOUNDARY
    BOUNDARY --> FLOAT32_OPS
    GAIN_DATA --> FLOAT32_OPS
    S6 --> OUT

    style BOUNDARY fill:#ffffaa,stroke:#ff9900,stroke-width:3px
    style INPUT fill:#ffe0e0
    style UINT16_OPS fill:#fff0e0
    style FLOAT32_OPS fill:#e0ffe0
```

---

## 3. CalibrationManager 초기화 시퀀스

> **개념**: C# GUI → C ABI → CalibrationManager 초기화 및 보정 파일 로드 흐름

```mermaid
sequenceDiagram
    participant GUI as C# GUI<br/>(ImageProcTest.exe)
    participant API as C ABI<br/>(xpe_preprocess_api)
    participant CM as CalibrationManager<br/>(SWU-1.5)
    participant DISK as 파일 시스템<br/>(.xpe_calib)

    GUI->>API: xpe_preprocess_init(config_json)
    activate API
    API->>CM: CalibrationManager::Initialize(config)
    activate CM

    CM->>DISK: 오프셋 파일 로드
    DISK-->>CM: offset_map.xpe_calib
    CM->>CM: CRC-32 검증
    CM->>CM: 만료 타임스탬프 확인

    CM->>DISK: 게인 파일 로드
    DISK-->>CM: gain_map.xpe_calib
    CM->>CM: CRC-32 검증
    CM->>CM: 게인 범위 [0.1, 10.0] 검증

    CM->>DISK: BPM 파일 로드
    DISK-->>CM: bad_pixel_map.xpe_calib
    CM->>CM: RLE 압축 해제
    CM->>CM: 결함 밀도 ≤5% 검증

    CM-->>API: XPE_OK (또는 에러코드)
    deactivate CM
    API-->>GUI: 초기화 결과
    deactivate API

    Note over CM,DISK: 파일 미발견 → XPE_ERR_IO_FAILED<br/>CRC 불일치 → XPE_ERR_IO_FAILED<br/>만료 → XPE_ERR_CALIB_EXPIRED<br/>범위 초과 → XPE_ERR_INVALID_CALIB_DATA
```

---

## 4. 오프셋(암전류) 보정

> **개념**: 검출기의 열적 암전류 노이즈를 제거하는 기본 보정 알고리즘

```mermaid
flowchart TD
    subgraph MATH["수학적 모델"]
        M1["I_corr(x,y) = I_raw(x,y) - I_dark(x,y)"]
        M2["I_dark(T) = I₀ × exp(-Eg/2kB×T)"]
        M1 --> M2
    end

    subgraph ALGO["처리 알고리즘"]
        A1{"온도 보상\n필요?"}
        A2["온도/PREP시간\n2D 룩업 테이블 구성"]
        A3["쌍선형 보간\nI_dark(T_current)"]
        A4["픽셀별 차감\nI_raw - I_dark"]
        A5{"결과 < 0?"}
        A6["클램프 → 0\n(uint16 하한)"]
        A7["그대로 유지"]

        A1 -->|Yes| A2
        A1 -->|No| A4
        A2 --> A3 --> A4
        A4 --> A5
        A5 -->|Yes| A6
        A5 -->|No| A7
    end

    subgraph PARAMS["보정 파라미터"]
        P1["오프셋 맵\nuint16 [3072×3072]"]
        P2["현재 온도\nNTC 서미스터"]
        P3["PREP 시간\n마지막 방사 이후 경과"]
        P4["Eg = 1.12 eV (실리콘)\nkB = 8.617×10⁻⁵ eV/K"]
    end

    P1 --> ALGO
    P2 --> ALGO
    P3 --> ALGO
    P4 --> MATH

    A6 --> OUT["오프셋 보정 완료\nuint16"]
    A7 --> OUT

    style M1 fill:#e8f4fd,stroke:#2196F3
    style M2 fill:#e8f4fd,stroke:#2196F3
    style A5 fill:#fff9c4,stroke:#f9a825
    style A6 fill:#ffcdd2,stroke:#e53935
```

**물리적 배경**: 암전류는 반도체 밴드갭(Eg)에 의한 열 여기 전자로 발생. 온도 의존적 지수 모델 적용.  
**구현 파일**: `modules/preprocess/src/xpe_offset.cpp`

---

## 5. 게인(평탄화) 보정

> **개념**: 픽셀별 감도 불균일성(FPN) 및 SID 의존 강도 강하를 정규화

```mermaid
flowchart TD
    subgraph MATH["수학적 모델"]
        M1["I_norm(x,y) = I_corr(x,y) / G(x,y)"]
        M2["다중 게인 모드:\nG(x,y,E) = Σ(c_k × E^k)\n(SID별 다항식)"]
        M3["힐 효과 보정:\nWang 2013 투영 모델"]
    end

    subgraph ALGO["처리 알고리즘"]
        A1{"다중 게인\n모드?"}
        A2["SID별 다항식\n게인 계산"]
        A3["단일 게인 맵\n직접 사용"]
        A4{"G(x,y) == 0?"}
        A5["XPE_ERR_INVALID_CALIB_DATA\n반환"]
        A6["I_norm = I_corr / G\n⚡ uint16 → float32 변환"]
        A7{"힐 효과\n보정 활성?"}
        A8["Wang 2013 모델\n적용"]
        A9["결과 출력"]

        A1 -->|Yes| A2
        A1 -->|No| A3
        A2 --> A4
        A3 --> A4
        A4 -->|Yes| A5
        A4 -->|No| A6
        A6 --> A7
        A7 -->|Yes| A8
        A7 -->|No| A9
        A8 --> A9
    end

    subgraph FORMAT["형식 경계"]
        F1["입력: uint16 (오프셋 보정 완료)"]
        F2["출력: float32 ← 이 단계에서 변환"]
        F1 --> F2
    end

    subgraph PARAMS["게인 파라미터"]
        P1["게인 맵\nfloat32 [3072×3072]\n범위 [0.1, 10.0]"]
        P2["SID 값\n(Source-Image Distance)"]
        P3["kVp 에너지"]
    end

    P1 --> ALGO
    P2 --> ALGO
    P3 --> ALGO
    PARAMS -.-> MATH

    style F2 fill:#ffffaa,stroke:#ff9900,stroke-width:3px
    style A6 fill:#ffffaa,stroke:#ff9900,stroke-width:2px
    style A5 fill:#ffcdd2,stroke:#e53935
    style M1 fill:#e8f4fd,stroke:#2196F3
```

**구현 파일**: `modules/preprocess/src/xpe_gain.cpp`

---

## 6. 비선형성 보정

> **개념**: 검출기 응답의 비선형성을 LUT 또는 다항식으로 선형화

```mermaid
flowchart TD
    subgraph CONCEPT["비선형성 원인"]
        C1["전하 트래핑\n(Charge Trapping)"]
        C2["필 팩터 효과\n(Fill Factor Effect)"]
        C3["ADC 비선형성"]
        C1 & C2 & C3 --> NL["비선형 검출기 응답\nS_meas ≠ k × D_ref"]
    end

    subgraph SELECTION["방법 선택"]
        SEL{"패널 프로파일\npanel.linear?"}
        SKIP["보정 건너뜀\n(이미 선형)"]
        METHOD{"구현 방법\n선택"}
        LUT_PATH["LUT 방법\n(CPU 최적화)"]
        POLY_PATH["다항식 방법\n(MCU/FPGA)"]

        SEL -->|true| SKIP
        SEL -->|false| METHOD
        METHOD -->|"크기 4096/65536 LUT 존재"| LUT_PATH
        METHOD -->|"차수 ≤5 계수 존재"| POLY_PATH
    end

    subgraph LUT_METHOD["LUT 방법 (O(1) 조회)"]
        L1["팩토리 캘리브레이션:\nN≥10 선량 수준에서 측정"]
        L2["이상적 선형 응답 피팅:\nS_ideal = G_nominal × D"]
        L3["단조 3차 스플라인 보간\n(Fritsch-Carlson 1980)"]
        L4["LUT[I_raw] = I_lin\n직접 인덱스 조회"]
        L5["오차 ≤ 0.3% ADC 풀스케일"]
        L1 --> L2 --> L3 --> L4 --> L5
    end

    subgraph POLY_METHOD["다항식 방법 (Horner's Method)"]
        P1["I_lin = c₀ + c₁I + c₂I² + c₃I³ + c₄I⁴ + c₅I⁵"]
        P2["Horner 최적화:\nI_lin = c₀ + I(c₁ + I(c₂ + I(c₃ + I(c₄ + Ic₅))))"]
        P3["단조성 검증\n(역전 금지)"]
        P4["잔차 ≤ 0.5% ADU"]
        P1 --> P2 --> P3 --> P4
    end

    LUT_PATH --> LUT_METHOD
    POLY_PATH --> POLY_METHOD
    LUT_METHOD & POLY_METHOD --> OUT["선형화된 신호\nuint16 → 게인 보정 입력"]

    style LUT_PATH fill:#e8f4fd,stroke:#2196F3
    style POLY_PATH fill:#f3e5f5,stroke:#9c27b0
    style SKIP fill:#e0f2f1,stroke:#009688
```

**구현 파일**: `modules/preprocess/src/nonlinearity_correct.cpp`  
**SRS 참조**: `SRS-CALIB-FUNC-006-EXT`

---

## 7. 결함 픽셀 보정

> **개념**: 불량 픽셀 검출(RMM 알고리즘) 및 주변 픽셀 보간으로 결함 은폐

```mermaid
flowchart TD
    subgraph DETECTION["결함 검출 (팩토리 + 런타임)"]
        D1["팩토리 BPM 로드\nuint8 맵\n0=정상, 1=사망, 2=핫, 3=고착, 4=노이즈"]
        D2{"런타임 SNR\n검출 활성?"}
        D3["SNR 계산\n각 픽셀 대비 이웃"]
        D4{"SNR < 5 dB?"}
        D5["런타임 결함으로 임시 표시"]
        D6["BPM과 병합"]

        D1 --> D6
        D2 -->|Yes| D3
        D2 -->|No| D6
        D3 --> D4
        D4 -->|Yes| D5 --> D6
        D4 -->|No| D6
    end

    subgraph RMM["RMM 알고리즘 (팩토리 캘리브레이션)"]
        R1["다중 균일 조도 영상 취득"]
        R2["로컬 분산 맵 계산"]
        R3["강건 중앙값 추정 (λ=8.0)"]
        R4["이상점 검출:\n|σ_pixel - σ_median| > λ × MAD"]
        R5["결함 유형 분류\n사망/핫/고착/노이즈"]
        R1 --> R2 --> R3 --> R4 --> R5
    end

    subgraph INTERPOLATION["보간 방법 선택"]
        I1{"결함 유형\n및 밀도"}
        I2["이웃 평균 보간\n(소규모, 고립 결함)"]
        I3["쌍선형 보간\n(중간 밀도)"]
        I4["중앙값 필터 보간\n(고밀도, 클러스터)"]

        I1 -->|"단일 픽셀"| I2
        I1 -->|"2×2~8×8"| I3
        I1 -->|"클러스터 >8×8"| I4
    end

    D6 --> INTERPOLATION
    RMM --> D1
    I2 & I3 & I4 --> OUT["결함 보정 완료\nfloat32"]

    subgraph LIMITS["안전 한계"]
        L1["결함 밀도 ≤ 5% 전체 픽셀"]
        L2["클러스터 최대 크기 제한"]
        L3["보간 신뢰도 점수"]
    end

    style D4 fill:#fff9c4,stroke:#f9a825
    style I2 fill:#e8f4fd,stroke:#2196F3
    style I3 fill:#e8f4fd,stroke:#2196F3
    style I4 fill:#e8f4fd,stroke:#2196F3
    style RMM fill:#f9f9f9,stroke:#999
```

**구현 파일**: `modules/preprocess/src/xpe_defect.cpp`  
**관련 문서**: [`../panel-defect/SRS-DEFECT-001_Software_Requirements_Specification.md`](../panel-defect/SRS-DEFECT-001_Software_Requirements_Specification.md)

---

## 8. 고스트/잔상 보정 (3-Tier)

> **개념**: 잔류 전하 축적에 의한 잔상을 3단계 복잡도로 제거 (Tier 1 → Tier 3 순으로 정교)

### 8.1 Tier 선택 로직

```mermaid
flowchart TD
    START["새 프레임 획득"]

    subgraph TIER_SELECT["Tier 선택 기준"]
        T1{"노출 이력\n프레임 수"}
        T2{"이전 선량\n수준"}
        T3{"정밀도\n요구사항"}

        TIER1_COND["Tier 1 선택:\n• 이력 <3 프레임\n• 단순 형광 투시\n• 빠른 처리 우선"]
        TIER2_COND["Tier 2 선택:\n• 이력 3~8 프레임\n• 선량 변화 있음\n• 일반 임상"]
        TIER3_COND["Tier 3 선택:\n• 이력 ≥8 프레임\n• 고선량 이전 노출\n• DSA/고정밀 모드"]

        T1 --> TIER1_COND & TIER2_COND & TIER3_COND
        T2 --> TIER2_COND & TIER3_COND
        T3 --> TIER3_COND
    end

    TIER1["Tier 1\n이중 지수 LTI\n역합성곱"]
    TIER2["Tier 2\n노출 가중\n모드 선택"]
    TIER3["Tier 3\nNLCSC\n비선형 인과"]

    TIER1_COND --> TIER1
    TIER2_COND --> TIER2
    TIER3_COND --> TIER3

    TIER1 & TIER2 & TIER3 --> OUT["고스트 제거 완료\nfloat32"]

    style TIER1 fill:#e8f4fd,stroke:#2196F3
    style TIER2 fill:#fff9c4,stroke:#f9a825
    style TIER3 fill:#fce4ec,stroke:#e91e63
```

### 8.2 Tier 1: LTI 역합성곱

```mermaid
flowchart LR
    subgraph LTI["LTI (선형 시불변) 모델"]
        M1["잔상 모델:\nI_lag(n) = Σᵢ αᵢ × exp(-n/τᵢ)\ni=1..2 (이중 지수)"]
        M2["4개 상태 변수\nz1, z2, z3, z4"]
        M3["역합성곱:\nI_corrected = I_raw - I_lag_est"]
    end

    HIST["프레임 이력\n링 버퍼 (8 프레임)"]
    EST["잔상 추정\nΣ αᵢ × zᵢ"]
    CORR["I_corrected\n= I_raw - I_lag_est"]

    HIST --> EST
    LTI --> EST
    EST --> CORR

    note1["α₁, α₂: 진폭 계수\nτ₁, τ₂: 시간 상수\n(패널 특성치)"]
    note1 -.-> LTI
```

### 8.3 Tier 3: NLCSC (비선형 인과 공간 컨텍스트)

```mermaid
stateDiagram-v2
    [*] --> IDLE: 핸들 생성\nxpe_ghost_create()

    IDLE --> ACCUMULATING: 첫 프레임 처리
    note right of IDLE: 이력 버퍼 초기화\n노출 이력 = 0

    ACCUMULATING --> ACCUMULATING: 추가 프레임\n(이력 < 8)
    note right of ACCUMULATING: 상태 변수 업데이트\nN=4 지수 성분\n링 버퍼에 저장

    ACCUMULATING --> STEADY_STATE: 이력 = 8 프레임
    note right of STEADY_STATE: 풀 NLCSC 적용\n비선형 인과 모델\n최고 정확도

    STEADY_STATE --> STEADY_STATE: 연속 프레임 처리

    STEADY_STATE --> IDLE: xpe_ghost_reset()\n또는 모드 변경
    note left of IDLE: 모든 상태 초기화\n고선량 후 리셋 권장

    STEADY_STATE --> [*]: xpe_ghost_destroy()
    IDLE --> [*]: xpe_ghost_destroy()
```

**구현 파일**: `modules/preprocess/src/ghost_correct.cpp`  
**관련 문서**: [`../ghost-correction/srs_ghost_correction.md`](../ghost-correction/srs_ghost_correction.md)

---

## 9. 온도 보상

> **개념**: NTC 서미스터로 측정한 검출기 온도 변화에 따른 암전류 드리프트 보상

```mermaid
flowchart TD
    subgraph SENSING["온도 감지"]
        NTC["NTC 서미스터\n검출기 패널 온도 측정"]
        T_curr["현재 온도 T_current"]
        T_ref["기준 온도 T_ref\n(캘리브레이션 취득 시)"]
        NTC --> T_curr
    end

    subgraph DECISION["보상 필요성 판단"]
        DELTA["ΔT = |T_current - T_ref|"]
        BYPASS{"ΔT < 2°C?"}
        BYPASS_ACT["온도 보상 건너뜀\n(무시 가능한 드리프트)"]
        APPLY["온도 보상 적용"]
        DELTA --> BYPASS
        BYPASS -->|Yes| BYPASS_ACT
        BYPASS -->|No| APPLY
    end

    subgraph MODEL["지수 드리프트 모델"]
        EQ["I_dark(T) = I₀ × exp(-Eg / (2 × kB × T))"]
        PARAMS["물리 상수:\nEg = 1.12 eV (실리콘 밴드갭)\nkB = 8.617 × 10⁻⁵ eV/K"]
        SCALE["스케일 팩터:\nS(T) = exp[-Eg/2kB × (1/T_curr - 1/T_ref)]"]
        EQ -.-> SCALE
        PARAMS -.-> SCALE
    end

    subgraph INTERP["쌍선형 보간"]
        TABLE["2D 룩업 테이블\n축 1: 온도 (5°C 간격)\n축 2: PREP 시간 (초)"]
        BILINEAR["쌍선형 보간\n정확한 현재 조건 추정"]
        TABLE --> BILINEAR
    end

    T_curr & T_ref --> DELTA
    APPLY --> MODEL
    APPLY --> INTERP
    MODEL & INTERP --> ADJ_MAP["보정된 오프셋 맵\n현재 온도 기준"]
    ADJ_MAP --> OFFSET_STEP["→ 오프셋 보정 단계로"]

    style BYPASS fill:#fff9c4,stroke:#f9a825
    style BYPASS_ACT fill:#e0f2f1,stroke:#009688
    style EQ fill:#e8f4fd,stroke:#2196F3
```

---

## 10. 바이패스 결정 로직

> **개념**: 각 보정 단계의 활성화/비활성화를 결정하는 중앙 로직

```mermaid
flowchart TD
    subgraph INPUTS["입력 조건"]
        CFG["구성 파일\n활성화 플래그"]
        PANEL["패널 프로파일\npanel.linear, panel.has_defect_map"]
        META2["획득 메타데이터\n온도, SID, 바이닝 모드"]
        CALIB["캘리브레이션 파일\n가용성 및 만료 상태"]
    end

    subgraph BYPASS_LOGIC["단계별 우회 결정"]
        B_TEMP{"온도 보상\n필요?"}
        B_NL{"비선형성\n보정 필요?"}
        B_BINNING{"바이닝\n보정 필요?"}
        B_DEFECT{"결함\n보정 필요?"}
        B_GHOST{"고스트\n보정 필요?"}

        COND_TEMP["ΔT ≥ 2°C\n AND 온도 파일 존재"]
        COND_NL["panel.linear = false\n AND LUT/다항식 로드됨"]
        COND_BINNING["binning_mode ≠ 1×1\n AND 바이닝 프로파일 존재"]
        COND_DEFECT["결함 맵 로드됨\n OR 런타임 검출 활성"]
        COND_GHOST["ghost_enable = true\n AND 이력 버퍼 초기화됨"]

        B_TEMP -->|"조건 충족"| COND_TEMP
        B_NL -->|"조건 충족"| COND_NL
        B_BINNING -->|"조건 충족"| COND_BINNING
        B_DEFECT -->|"조건 충족"| COND_DEFECT
        B_GHOST -->|"조건 충족"| COND_GHOST
    end

    subgraph STATUS["상태 플래그 출력"]
        FLAGS["ProcessingFlags:\n• TEMP_COMPENSATED\n• NONLIN_CORRECTED\n• GAIN_APPLIED\n• BINNING_CORRECTED\n• DEFECT_CORRECTED\n• GHOST_CORRECTED"]
        LOG["진단 로그:\n• 각 단계 처리 시간\n• 우회 이유\n• 경고 메시지"]
    end

    INPUTS --> BYPASS_LOGIC
    BYPASS_LOGIC --> STATUS

    style FLAGS fill:#e8f4fd,stroke:#2196F3
    style CFG fill:#fff9c4
    style PANEL fill:#fff9c4
```

---

## 11. 캘리브레이션 데이터 생명주기

> **개념**: 팩토리 캘리브레이션 생성부터 현장 재캘리브레이션까지의 전체 생명주기

```mermaid
stateDiagram-v2
    [*] --> FACTORY_CALIB: 제조 시 팩토리 캘리브레이션

    state FACTORY_CALIB {
        [*] --> DARK_ACQ: 암전류 영상 취득
        DARK_ACQ --> FLAT_ACQ: 평탄 조도 영상 취득
        FLAT_ACQ --> BPM_GEN: BPM 생성 (RMM)
        BPM_GEN --> NL_CALIB: 비선형성 LUT 생성
        NL_CALIB --> SAVE: .xpe_calib 파일 저장\nCRC-32 + 만료 타임스탬프
    }

    FACTORY_CALIB --> DEPLOYED: 현장 배포

    state DEPLOYED {
        [*] --> VALID: 캘리브레이션 유효
        VALID --> VALID: 정상 사용\n(매 프레임 적용)
        VALID --> EXPIRY_CHECK: 만료 검사\n(24시간마다)
        EXPIRY_CHECK --> EXPIRED: 만료 시간 초과
        EXPIRY_CHECK --> VALID: 아직 유효
    }

    EXPIRED --> FIELD_RECALIB: 현장 재캘리브레이션 필요

    state FIELD_RECALIB {
        [*] --> DARK_COLLECT: 새 암전류 취득\n(IAP-CALIB-001 절차)
        DARK_COLLECT --> VALIDATE: 통계 검증\nσ/mean < 1%
        VALIDATE --> OVERWRITE: 새 파일로 덮어쓰기
        VALIDATE --> DARK_COLLECT: 검증 실패 → 재취득
    }

    FIELD_RECALIB --> DEPLOYED: 재배포

    DEPLOYED --> [*]: 시스템 폐기

    note right of FACTORY_CALIB: 취득 절차:\nIAP-CALIB-001 §3\n온도 안정화 30분 후
    note right of EXPIRED: 만료 전 경고:\nXPE_WARN_CALIB_EXPIRING_SOON\n(7일 전)
```

---

## 12. 소프트웨어 유닛 분해도

> **개념**: SWU 계층 구조 및 모듈 간 의존 관계

```mermaid
graph TD
    subgraph DLL["xpe_preprocess.dll (Layer 1)"]
        direction TB
        API["C ABI 레이어\nxpe_preprocess_api.h\n18개 내보내기 함수"]

        subgraph ORCHESTRATION["오케스트레이션"]
            CM["CalibrationManager\nSWU-1.5\n로드/검증/만료 관리"]
            PIPE["Pipeline Orchestrator\nSWU 전체 실행 순서 제어"]
        end

        subgraph CORRECTIONS["보정 유닛"]
            SWU11["SWU-1.1\n오프셋 보정\nxpe_offset.cpp"]
            SWU12["SWU-1.2\n게인 보정\nxpe_gain.cpp"]
            SWU17["SWU-1.7\n비선형성 보정\nnonlinearity_correct.cpp"]
            SWU13["SWU-1.3\n결함 보정\nxpe_defect.cpp"]
            SWU14["SWU-1.4\n고스트 보정\nghost_correct.cpp"]
        end

        subgraph SUPPORT["지원 유닛"]
            SWU16["SWU-1.6\n온도 보상\n(오프셋 내부)"]
            SWU18["SWU-1.8\n바이닝 보정\nbinning_correct.cpp"]
            SWU19["SWU-1.9\n리드아웃 검증\n(파이프라인 내부)"]
        end

        subgraph IO["I/O 유닛"]
            XCAL_R["xcal_reader.cpp\n.xpe_calib 읽기"]
            XCAL_W["xcal_writer.cpp\n.xpe_calib 쓰기"]
            XCAL_V["xcal_validator.cpp\nCRC-32/범위 검증"]
        end

        API --> CM
        API --> PIPE
        CM --> XCAL_R & XCAL_V
        PIPE --> SWU11 & SWU12 & SWU17 & SWU13 & SWU14
        PIPE --> SWU16 & SWU18 & SWU19
        SWU11 --> SWU16
        XCAL_W --> CM
    end

    COMMON["xpe_common.dll (Layer 0)\nXpeImageBuffer · XpeErrorCode\nXpeImageMetadata · Alert Queue"]
    GUI["ImageProcTest.exe (Layer 2)\nC# WPF GUI\nP/Invoke"]

    GUI -->|P/Invoke C ABI| API
    DLL -->|링크 의존| COMMON

    style DLL fill:#e3f2fd,stroke:#1565c0,stroke-width:2px
    style COMMON fill:#e8f5e9,stroke:#2e7d32
    style GUI fill:#fce4ec,stroke:#880e4f
    style CM fill:#fff9c4,stroke:#f57f17
    style API fill:#f3e5f5,stroke:#6a1b9a
```

---

## 크로스 레퍼런스

| 다이어그램 | 관련 SRS 요건 | 관련 SAD 유닛 | 소스 파일 |
|----------|------------|------------|---------|
| 파이프라인 전체 | SRS-CALIB-FUNC-001~009 | SWU-1.1~1.9 | pipeline.cpp |
| 오프셋 보정 | SRS-CALIB-FUNC-004 | SWU-1.1 | xpe_offset.cpp |
| 게인 보정 | SRS-CALIB-FUNC-005 | SWU-1.2 | xpe_gain.cpp |
| 비선형성 보정 | SRS-CALIB-FUNC-006, 006-EXT | SWU-1.7 | nonlinearity_correct.cpp |
| 결함 보정 | SRS-CALIB-FUNC-007 | SWU-1.3 | xpe_defect.cpp |
| 고스트 보정 | SRS-CALIB-FUNC-008 | SWU-1.4 | ghost_correct.cpp |
| 온도 보상 | SRS-CALIB-FUNC-009 | SWU-1.6 | xpe_offset.cpp |
| 바이패스 로직 | 전 요건 | 전 SWU | pipeline.cpp |
| 데이터 생명주기 | SRS-CALIB-FUNC-001~003 | SWU-1.5 | calibration_manager.cpp |
| SWU 분해도 | 전체 | SAD-CALIB-001 §4 | — |

---

**다음 문서**: [KNOWLEDGE-BASE.md](KNOWLEDGE-BASE.md) — 전체 캘리브레이션 지식 베이스  
**관련 문서**: [README.md](README.md) — 파이프라인 기술 개요  
**검증 가이드**: [ALGORITHM-VERIFICATION-GUIDE.md](ALGORITHM-VERIFICATION-GUIDE.md)
