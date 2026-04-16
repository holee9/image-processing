# X-ray FPD 디스플레이 처리 모듈 제품 요구사항 문서

**모듈**: `xpe_display.dll` (Layer 1, Phase 1b)  
**소유자 DLL**: `xpe_display.dll`  
**의존성**: `xpe_enhance_advanced.dll` (Layer 1)  
**안전 등급**: IEC 62304 Class B  
**문서 버전**: 1.0  
**날짜**: 2026-04-14  
**규범 사양**: [SPEC-XPE-MASTER v2.0.0](../../.moai/specs/SPEC-XPE-MASTER.md)

---

## 1. 개요

`xpe_display.dll`은 X-ray FPD 이미지 처리 엔진의 표시 처리 모듈입니다. 강화 도메인 float32 이미지를 입력받아 **DICOM Grayscale Standard Display Function (GSDF)** 파이프라인을 통해 표시 준비 완료 uint16 출력으로 변환합니다.

### 핵심 특징

- **4개 소프트웨어 단위** (SWU-3.1 ~ SWU-3.4)
- **3개 의무 처리 단계**: Modality LUT → VOI LUT → Presentation LUT
- **1개 핵심 형식 경계**: float32 → uint16 (Presentation LUT에서)
- **LUT 프리셋 라이브러리**: 신체 부위별 자동 선택 (흉부, 골격, 복부, 소아, 투시)
- **DICOM PS3.14 GSDF 준수**: JND 기반 광도 매핑

### 지원되는 검사 유형

| 검사 유형 | 신체 부위 | 권장 프리셋 | 용도 |
|----------|---------|-----------|------|
| 흉부 정면 | Chest | Chest_PA | 폐 결절, 종격동 |
| 흉부 측면 | Chest | Chest_Lateral | 심장 윤곽, 척추 |
| 사지 | Extremity | Extremity | 뼈 미세한 구조 |
| 척추 | Spine | Spine | 척추체, 디스크 |
| 복부 | Abdomen | Abdomen | 간, 신장, 비장 |
| 소아 | Pediatric | Pediatric | 낮은 선량 노이즈 |
| 투시 (형광) | Fluoroscopy | Fluoroscopy | 실시간 표시 |

---

## 2. 데이터 도메인 규칙

### 2.1 입력/출력 형식

```
┌──────────────────────────────────────────────────────────┐
│                                                          │
│  입력: float32 강화 도메인                              │
│  (xpe_enhance_advanced.dll 출력)                         │
│  범위: [0.0, 4095.0] (GSDF p-value 범위)               │
│                                                          │
│  ┌────────────────────────────────────┐                 │
│  │ SWU-3.1 ModalityLUT                │                 │
│  │ Input: float32, Output: float32     │                 │
│  │ DICOM (0028,3000) Modality LUT Seq │                 │
│  └────────────┬───────────────────────┘                 │
│               │                                          │
│               v                                          │
│  ┌────────────────────────────────────┐                 │
│  │ SWU-3.2 VoiLUT                     │                 │
│  │ Input: float32, Output: float32     │                 │
│  │ Window/Level (선형, 시그모이드)    │                 │
│  │ DICOM (0028,1050)/(0028,1051)     │                 │
│  └────────────┬───────────────────────┘                 │
│               │                                          │
│               v                                          │
│  ┌────────────────────────────────────┐                 │
│  │ SWU-3.3 PresentationLUT/GSDF       │                 │
│  │ Input: float32, Output: uint16      │                 │
│  │ DICOM PS3.14 GSDF 매핑             │                 │
│  │ 광도 (JND) → 표시 p-value          │                 │
│  └────────────┬───────────────────────┘                 │
│               │                                          │
│               v                                          │
│  출력: uint16 표시 도메인                               │
│  범위: [0, 65535] (16-bit)                              │
│  준비 완료: DICOM 인코딩, 디스플레이 렌더링            │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

### 2.2 데이터 도메인 제약

```
(비선택) 원본 영상
    ↓
(검출기 도메인)  ← 검출기 신호 [0, 65535]
    ↓ xpe_preprocess.dll
(캘리브레이션 도메인) ← 물리 보정 [음수 ~ 양수]
    ↓ xpe_enhance_basic.dll
(강화 도메인) ← 시각적 강화 float32 [0.0~4095.0]
    ↓ xpe_display.dll  ← THIS MODULE
(표시 도메인) ← 디스플레이 준비 uint16 [0~65535]
    ↓ DICOM 인코딩 / 렌더링
(최종 출력)
```

**중요**: 각 단계에서 데이터 도메인이 변경됩니다. 잘못된 단계 순서는 **진단 오류**로 이어집니다.

---

## 3. SWU-3.1 ModalityLUT — Vendor-Independent 값 매핑

### 3.1.1 목적

원본 픽셀 값 또는 calibrated value를 **vendor-independent linear value**로 변환합니다. CT의 경우 Hounsfield Unit (HU), 일반 X선의 경우 vendor-independent exposure의 선형 함수입니다.

### 3.1.2 알고리즘

**공식**:
```
Linear_Value = slope × PixelValue + intercept
```

**입력**:
- `pixel_value`: float32 (강화 도메인)
- `slope`: Rescale Slope (DICOM 0028,1053)
- `intercept`: Rescale Intercept (DICOM 0028,1052)

**출력**:
- `linear_value`: float32 (선형화된 값)

### 3.1.3 요구사항

| ID | 요구사항 | 기술 | 영향 |
|----|---------|------|------|
| REQ-MODAL-001 | Slope/Intercept 정확도 | slope ≥ 14-bit 정확도 | HU 정확도 ≤ ±2 |
| REQ-MODAL-002 | 음수 처리 | HU < 0 (물, 지방) 지원 | 해부학적 정확도 |
| REQ-MODAL-003 | 범위 검증 | Slope ≤ 65535, Intercept ≤ 1000 | 안전한 수치 범위 |
| REQ-MODAL-004 | LUT 테이블 지원 | 최대 65536개 항목, 선형 보간 | 호환성 |
| REQ-MODAL-005 | DICOM 태그 읽기 | (0028,3000) Modality LUT Sequence | 표준 준수 |
| REQ-MODAL-006 | 성능 | ≤ 5ms (3072×3072) | 실시간 성능 |

### 3.1.4 구현 상세

```cpp
// Slope/Intercept 방식
XpeErrorCode xpe_modality_lut_apply(
    XpeImageBuffer* image,           // 입력 (강화 도메인)
    float slope,
    float intercept,
    XpeImageBuffer* output           // 출력 (선형화)
);

// LUT 테이블 방식
XpeErrorCode xpe_modality_lut_apply_table(
    XpeImageBuffer* image,
    float* lut_table,                // 크기: lut_size
    uint32_t lut_size,
    XpeImageBuffer* output
);
```

### 3.1.5 검증 기준

| 검증 항목 | 합격 기준 | 방법 |
|----------|---------|------|
| Linearity | R² > 0.9999 | Golden Reference 비교 |
| Precision | ±1 LSB (16-bit) | 부동소수점 오차 ≤ 1/65536 |
| Dynamic Range | ≤ 2000 (HU) | 공기 ~ 뼈 범위 |
| Null Slope | slope=0 거부 | 에러 반환 |

---

## 4. SWU-3.2 VoiLUT — Window/Level 처리

### 4.1 목적

**Modality LUT** 출력을 **VOI (Value of Interest)** 범위로 매핑합니다. Window Center(WC)와 Window Width(WW)로 표시된 관심 영역을 강조합니다.

### 4.2 알고리즘 — 3가지 모드

#### 4.2.1 선형 모드 (Linear)

**공식**:
```
if (input <= (WC - WW/2)):
    output = 0
elif (input >= (WC + WW/2)):
    output = MAX_VALUE
else:
    output = ((input - (WC - WW/2)) / WW) * MAX_VALUE
```

**특징**:
- 가장 일반적
- 빠른 계산 (조건 분기 + 나눗셈)
- 급격한 색감 변화 (비선형 지각)

**적용 검사**:
- 흉부 (폐): WC=-400, WW=1500
- 골격: WC=300, WW=1500
- 복부: WC=40, WW=350

#### 4.2.2 시그모이드 모드 (Sigmoid)

**공식**:
```
output = MAX_VALUE / (1 + exp(-4 * (input - WC) / WW))
```

**특징**:
- 부드러운 색감 전환
- 가장자리 클리핑 완화
- 계산 비용 높음 (exp)

**적용 검사**:
- 투시 (형광) — 실시간 보기 선호
- 미묘한 조직 대비가 필요한 경우

#### 4.2.3 LUT 시퀀스 모드

**공식**:
```
VOI_LUT_Sequence를 DICOM 파일에서 읽어 pixel value → output 매핑 적용
```

**특징**:
- DICOM (0028,3010) VOI LUT Sequence 사용
- 사용자 정의 매핑 지원
- 최대 65536개 항목

### 4.3 요구사항

| ID | 요구사항 | 기술 | 영향 |
|----|---------|------|------|
| REQ-VOI-001 | 선형 모드 | 위 수식 정확 구현 | 표준 호환 |
| REQ-VOI-002 | 시그모이드 모드 | exp 정확도 ≤ 0.1% | 부드러운 전환 |
| REQ-VOI-003 | Preset 라이브러리 | 최소 6가지 신체 부위 | 임상 편의성 |
| REQ-VOI-004 | 동적 윈도우 | 마우스 드래그로 WC/WW 실시간 변경 | 의사 상호작용 |
| REQ-VOI-005 | 자동 윈도우 | EI-derived 통계에서 계산 | 초기 최적화 |
| REQ-VOI-006 | 범위 검증 | WW > 0, WC ≤ 4096 | 안전성 |
| REQ-VOI-007 | 성능 | ≤ 10ms (선형), ≤ 30ms (시그모이드) | 실시간 |
| REQ-VOI-008 | DICOM 태그 | (0028,1050)/(0028,1051) 읽기 | 표준 준수 |

### 4.4 Preset 라이브러리

```json
{
  "chest_pa": {"wc": -400, "ww": 1500, "mode": "linear"},
  "chest_lateral": {"wc": 40, "ww": 400, "mode": "linear"},
  "extremity": {"wc": 300, "ww": 1500, "mode": "linear"},
  "spine": {"wc": 350, "ww": 1800, "mode": "linear"},
  "abdomen": {"wc": 40, "ww": 350, "mode": "linear"},
  "pediatric": {"wc": 100, "ww": 500, "mode": "sigmoid"},
  "fluoroscopy": {"wc": 100, "ww": 500, "mode": "sigmoid"}
}
```

### 4.5 구현 상세

```cpp
// 선형 모드
XpeErrorCode xpe_voi_lut_apply_linear(
    XpeImageBuffer* image,
    float wc,        // Window Center
    float ww,        // Window Width
    XpeImageBuffer* output
);

// 시그모이드 모드
XpeErrorCode xpe_voi_lut_apply_sigmoid(
    XpeImageBuffer* image,
    float wc,
    float ww,
    XpeImageBuffer* output
);

// LUT 시퀀스 모드
XpeErrorCode xpe_voi_lut_apply_sequence(
    XpeImageBuffer* image,
    uint32_t* lut_sequence,      // 크기: 65536
    XpeImageBuffer* output
);

// 고속 경로 (미리 계산된 LUT)
XpeErrorCode xpe_voi_lut_apply_fast(
    XpeImageBuffer* image,
    float* precomputed_lut,      // 크기: 4096
    XpeImageBuffer* output
);
```

---

## 5. SWU-3.3 PresentationLUT/GSDF — DICOM PS3.14

### 5.1 목적

**Presentation LUT**는 medical display에서의 광도(luminance) 응답을 표준화합니다. DICOM Grayscale Standard Display Function (GSDF) PS3.14를 따릅니다.

### 5.2 GSDF 수학 공식

GSDF는 **의료용 display의 광도 특성**을 정의합니다:

**역함수** (p-value → luminance):
```
log10(L) = a + c*ln(j) + e*(ln(j))² + g*(ln(j))³ + m*(ln(j))⁴

여기서:
  L = 광도 (cd/m²)
  j = JND (Just Noticeable Difference) index
  a = -2.0, c = 2.4, e = -0.0525, g = -0.0205, m = 0.0099
```

**순함수** (luminance → p-value):
```
위 역함수를 수치 반복으로 풀이 (Newton-Raphson)
```

### 5.3 알고리즘 상세

#### 5.3.1 GSDF LUT 생성

```
1. 의료용 display 목표 광도:
   - 최소: L_min = 0.05 cd/m²  (매우 어두움)
   - 최대: L_max = 4000 cd/m²  (매우 밝음)
   - 보정: 실제 display calibration (선택)

2. JND index j = 0 ~ 1023 (1024 레벨)

3. 각 j에 대해:
   - GSDF 역함수로 L 계산
   - p-value (0~4095) 정규화: p = 4095 * (L - L_min) / (L_max - L_min)
   - LUT[j] = p

4. 사용자 display calibration 파라미터:
   - Ambient illumination (lux)
   - Display gamma (보정)
   - Peak luminance (실제 측정)
```

#### 5.3.2 표시 p-value 매핑

**입력**: float32 p-value (0~4095)  
**출력**: uint16 display code (0~65535)

```
display_code = (uint16)((p_value / 4095.0) * 65535)
```

### 5.4 요구사항

| ID | 요구사항 | 기술 | 영향 |
|----|---------|------|------|
| REQ-GSDF-001 | GSDF 준수 | DICOM PS3.14 공식 정확히 | 표준 호환 |
| REQ-GSDF-002 | JND 정확도 | luminance 오차 ≤ 10% | 임상 정확도 |
| REQ-GSDF-003 | 광도 범위 | 0.05 ~ 4000 cd/m² | 의료 display 표준 |
| REQ-GSDF-004 | Inverse GSDF | p-value → luminance | 검증용 역계산 |
| REQ-GSDF-005 | Display Calibration | 실측 보정 지원 | 임상 정확도 향상 |
| REQ-GSDF-006 | 성능 | ≤ 5ms (LUT 생성, 일회성) | 시작 시에만 |
| REQ-GSDF-007 | Gamma Fallback | 의료 display 없을 때 γ=2.2 | 일반 display 호환 |
| REQ-GSDF-008 | 메모리 | ≤ 32KB (1024-entry LUT) | 경제적 |

### 5.5 구현 상세

```cpp
// GSDF LUT 생성
XpeErrorCode xpe_gsdf_create_lut(
    float l_min_cd_m2,           // 최소 광도
    float l_max_cd_m2,           // 최대 광도
    float* lut_output,           // 크기: 1024
    uint32_t lut_size
);

// Display calibration 파라미터 설정
XpeErrorCode xpe_gsdf_set_display_params(
    float peak_luminance_cd_m2,
    float ambient_illumination_lux,
    float gamma
);

// Presentation LUT 적용
XpeErrorCode xpe_presentation_lut_apply(
    XpeImageBuffer* image,       // 입력 (float32, p-value)
    float* gsdf_lut,             // GSDF LUT (1024)
    XpeImageBuffer* output       // 출력 (uint16)
);

// Display capability 검증
XpeErrorCode xpe_gsdf_check_display_capability(
    float measured_peak_luminance_cd_m2,
    float* deviation_percent     // 출력: 목표로부터 편차
);
```

### 5.6 검증 기준

| 검증 항목 | 합격 기준 | 방법 |
|----------|---------|------|
| GSDF 수식 | 논문 구현과 ≤ 1% 오차 | Barten 1999 참고 |
| 광도 매핑 | JND 특성 (1.7:1 비율) | 연속 밝기 비교 |
| Gamma fallback | γ=2.2 ±0.1 | sRGB 표준 |
| LUT 크기 | 1024 JND 레벨 충분 | 의료 display 표준 |

---

## 6. SWU-3.4 LUTManager — Preset 관리 및 자동 선택

### 6.1 목적

Modality LUT, VOI LUT, Presentation LUT의 **프리셋**을 저장/조회/자동 선택합니다.

### 6.2 LUT 프리셋 구조

```json
{
  "lut_id": "chest_pa_bone_window",
  "body_part": "chest",
  "exam_type": "pa",
  "anatomy_focus": "bone",
  "modality_lut": {
    "slope": 1.0,
    "intercept": -1000
  },
  "voi_lut": {
    "wc": 500,
    "ww": 2000,
    "mode": "linear"
  },
  "presentation_lut": {
    "type": "gsdf",
    "l_min_cd_m2": 0.05,
    "l_max_cd_m2": 4000
  },
  "metadata": {
    "source": "factory",
    "created_date": "2026-01-01",
    "version": "1.0"
  }
}
```

### 6.3 요구사항

| ID | 요구사항 | 기술 | 영향 |
|----|---------|------|------|
| REQ-LUT-001 | CRUD 기능 | 생성, 읽기, 수정, 삭제 | 프리셋 관리 |
| REQ-LUT-002 | 자동 선택 | `xpe_lut_auto_select(body_part)` → LUT ID | 임상 편의성 |
| REQ-LUT-003 | 보간 지원 | 앵커 포인트 간 cubic spline | 부드러운 전환 |
| REQ-LUT-004 | 지속성 | JSON 형식 `~/.xpe/luts/` | 프리셋 저장 |
| REQ-LUT-005 | Factory Presets | 읽기 전용 기본값 | 안전성 |
| REQ-LUT-006 | 사용자 프리셋 | 임상의가 저장한 커스텀 LUT | 개인화 |
| REQ-LUT-007 | 성능 | ≤ 1ms (선택), ≤ 5ms (보간) | 실시간 |
| REQ-LUT-008 | 버전 관리 | 프리셋 버전 추적 | 호환성 |

### 6.4 신체 부위 자동 선택 매핑

| Body Part | 권장 VOI Preset | Modality | Presentation |
|----------|-----------------|----------|----------------|
| chest | chest_pa (PA), chest_lateral (LAT) | slope=1.0 | GSDF |
| extremity | extremity_bone | slope=1.0 | GSDF |
| spine | spine_vertebral | slope=1.0 | GSDF |
| abdomen | abdomen_liver | slope=1.0 | GSDF |
| pelvis | pelvis_bone | slope=1.0 | GSDF |
| pediatric | pediatric_low_dose | slope=0.8 | GSDF (gamma fallback) |
| fluoroscopy | fluoroscopy_realtime | slope=1.0 | Gamma 2.2 |

### 6.5 구현 상세

```cpp
// Preset 저장
XpeErrorCode xpe_lut_add_preset(
    const XpeLutPreset* preset,
    const char* lut_id
);

// Preset 조회
XpeErrorCode xpe_lut_get_preset(
    const char* lut_id,
    XpeLutPreset* output
);

// Preset 삭제
XpeErrorCode xpe_lut_remove_preset(
    const char* lut_id
);

// 자동 선택
XpeErrorCode xpe_lut_auto_select(
    const char* body_part,
    char* output_lut_id             // 출력: 선택된 LUT ID
);

// Preset 리스트
XpeErrorCode xpe_lut_list_presets(
    XpeLutPresetInfo* list,         // 출력 배열
    uint32_t* count                 // 입력/출력: 개수
);

// Interpolation (보간)
XpeErrorCode xpe_lut_interpolate(
    const XpeLutPreset* preset_low,
    const XpeLutPreset* preset_high,
    float t,                        // 보간 계수 [0, 1]
    XpeLutPreset* output
);
```

### 6.6 프리셋 저장 위치

```
~/.xpe/luts/
├── factory/                       (Factory presets, read-only)
│   ├── chest_pa.json
│   ├── chest_lateral.json
│   ├── extremity.json
│   ├── spine.json
│   ├── abdomen.json
│   ├── pediatric.json
│   └── fluoroscopy.json
├── user/                          (사용자 정의)
│   ├── my_custom_chest.json
│   └── my_custom_abdomen.json
└── cache/                         (계산 캐시)
    └── gsdf_lut_display_1.bin
```

---

## 7. 통합 처리 파이프라인

### 7.1 완전한 처리 흐름

```
입력: float32 강화 도메인 (3072 × 3072)

┌───────────────────────────────────┐
│ Step 1: SWU-3.1 ModalityLUT       │
│ · 입력: float32 강화                │
│ · slope/intercept 또는 LUT 테이블   │
│ · 출력: float32 선형화              │
│ · 시간: 5ms                         │
└───────────┬───────────────────────┘
            │
            v
┌───────────────────────────────────┐
│ Step 2: SWU-3.2 VoiLUT            │
│ · 입력: float32 선형화              │
│ · Window/Level (선형/시그모이드)   │
│ · Preset 또는 사용자 입력           │
│ · 출력: float32 VOI 범위            │
│ · 시간: 10~30ms                    │
└───────────┬───────────────────────┘
            │
            v
┌───────────────────────────────────┐
│ Step 3: SWU-3.3 PresentationLUT   │
│ · 입력: float32 VOI (0~4095)       │
│ · GSDF 매핑 (p-value → 광도)      │
│ · 의료 display 또는 gamma=2.2     │
│ · 출력: uint16 표시 준비            │
│ · 시간: 5ms                        │
│ · FORMAT BOUNDARY: float32→uint16  │
└───────────┬───────────────────────┘
            │
            v
출력: uint16 표시 도메인 (0~65535)

준비 완료: DICOM 인코딩, 디스플레이 렌더링
```

### 7.2 성능 예산

| 단계 | 예산 (ms) | 예상 (ms) | 참고 |
|------|:---------:|:---------:|------|
| SWU-3.1 ModalityLUT | 10 | 5 | slope/intercept 또는 LUT |
| SWU-3.2 VoiLUT | 15 | 10 (선형) / 25 (시그모이드) | Window/Level 매핑 |
| SWU-3.3 PresentationLUT | 10 | 5 | GSDF LUT 적용 |
| SWU-3.4 LUTManager | 5 | 1-2 | Preset 선택/보간 |
| **합계** | **40** | **21~37ms** | **Phase 1b 예산 내** |

### 7.3 메모리 예산

| 구성요소 | 크기 | 참고 |
|---------|------|------|
| 입력 이미지 (float32) | 37.7 MB | 3072 × 3072 |
| 출력 이미지 (uint16) | 18.9 MB | 3072 × 3072 |
| GSDF LUT (1024 entry) | 4 KB | 정적, 일회 생성 |
| Modality LUT (최대) | 256 KB | 65536 entry × 4 bytes |
| VOI LUT (최대) | 256 KB | 65536 entry × 4 bytes |
| Preset 캐시 (메모리) | < 1 MB | JSON 역직렬화 |
| **최고 총합** | **< 60 MB** | 표시 처리만 |

---

## 8. 안전 제약 조건

### 8.1 형식 경계 검증

```
FORMAT BOUNDARY 직전 (SWU-3.3 입력):
  · float32 범위: [0.0, 4095.0]
  · 무한대/NaN 검증: 거부
  · Negative 값: 거부 (0.0으로 clamp)

FORMAT BOUNDARY 후 (출력):
  · uint16 범위: [0, 65535]
  · Precision 손실: 최대 1 LSB (4095.0 / 65535)
  · Determinism: 동일 입력 → 동일 uint16 출력
```

### 8.2 DICOM 표준 준수

| 항목 | 요구사항 |
|------|---------|
| Modality LUT | (0028,3000) Modality LUT Sequence 또는 (0028,1053)/(0028,1052) |
| VOI LUT | (0028,1050)/(0028,1051) Window Center/Width 또는 (0028,3010) VOI LUT Sequence |
| Presentation LUT | DICOM PS3.14 GSDF 준수 |
| Output IOD | US (Ultrasound) IOD 또는 XC (일반 사진) IOD |

### 8.3 임상 안전성

| 위험 | 통제 방법 |
|------|----------|
| 잘못된 Window/Level | 기본값 제공 + 임상의 재정의 가능 |
| GSDF 비준수 | PS3.14 공식 감시, 검증 테스트 |
| Clipping (포화도) | 메타데이터 플래그 설정 (XPE_FLAG_CLIPPED) |
| Float32 → uint16 precision 손실 | 수용 가능 (의료 display 표준) |

---

## 9. 의존성 및 통합

### 9.1 입력 의존성

| 의존성 | 출처 | 용도 |
|--------|------|------|
| float32 이미지 | `xpe_enhance_advanced.dll` | 입력 데이터 |
| 신체 부위 정보 | 메타데이터 (DICOM 0018,0015) | 자동 LUT 선택 |
| EI-derived 통계 | `xpe_enhance_basic.dll` | 자동 Window 계산 (선택) |
| DICOM 태그 | `xpe_dicom.dll` | Modality/VOI LUT 읽기 |

### 9.2 출력 의존성

| 소비자 | 용도 |
|--------|------|
| `xpe_dicom.dll` | uint16 이미지를 DICOM 파일로 인코딩 |
| GUI 렌더러 | 디스플레이 출력 |
| PACS | 저장 및 전송 |

---

## 10. 요구사항 요약 테이블

| SWU | 요구사항 ID | 기술 | 합격 기준 |
|-----|------------|------|---------|
| SWU-3.1 | REQ-MODAL-001..006 | Modality LUT (Slope/Intercept, LUT 테이블) | ±1 LSB 정확도 |
| SWU-3.2 | REQ-VOI-001..008 | VOI LUT (선형/시그모이드/LUT) | ±5% 오차 |
| SWU-3.3 | REQ-GSDF-001..008 | Presentation LUT / GSDF | 광도 편차 ≤ 10% |
| SWU-3.4 | REQ-LUT-001..008 | LUT Manager (CRUD, 자동 선택, 보간) | ≤ 1ms 선택 |

---

## 참고문헌

### DICOM 표준

| 표준 | 관련 섹션 | 용도 |
|------|---------|------|
| DICOM PS3.3 (Information Object Definitions) | (0028,3000) Modality LUT Sequence | Modality 매핑 |
| DICOM PS3.3 | (0028,1050), (0028,1051) Window Center/Width | VOI LUT |
| DICOM PS3.3 | (0028,3010) VOI LUT Sequence | VOI LUT 테이블 |
| DICOM PS3.14 (Grayscale Rendering) | §3.1 GSDF | Presentation LUT |
| DICOM PS3.4 (Service-Object Pair Definitions) | DX IOD, XC IOD | 출력 형식 |

### 논문 및 표준

| 문헌 | 저자/연도 | 용도 |
|------|---------|------|
| GSDF 원본 | Barten 1999 | DICOM PS3.14 기반 논문 |
| CLAHE 및 대비 강화 | Pizer et al. 1987 | 참고 기술 |
| 의료 디스플레이 보정 | AAPM TG-30 | 광도 측정 기준 |
| Color & Imaging 표준 | sRGB 1996 | Gamma fallback (γ=2.2) |

### 프로젝트 문서

| 문서 | 설명 |
|------|------|
| SPEC-XPE-MASTER v2.0.0 | Master 사양 (SWU-3.x 정의) |
| xpe-enhance-advanced-prd.md | Phase 2 처리 (입력 데이터) |
| xpe-dicom-prd.md | DICOM I/O (출력 인코딩) |
| README.md | 기술 개요 |

---

**문서 끝**  
*xpe-display-prd.md v1.0*
