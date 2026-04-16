# SAD-DISPLAY-001: 소프트웨어 아키텍처 문서

**문서 ID**: SAD-DISPLAY-001  
**IEC 62304 절**: 5.3 소프트웨어 아키텍처  
**안전 분류**: Class B  
**모듈**: `xpe_display.dll`  
**버전**: 1.0  
**날짜**: 2026-04-14  
**작성자**: XPE 디스플레이팀  
**승인**: __________________ 날짜: __________

---

## 1. 목적

`xpe_display.dll`의 소프트웨어 아키텍처를 정의합니다. SRS-DISPLAY-001의 요구사항을 구현 설계로 분해하며, 4개 소프트웨어 단위(SWU)의 책임, 인터페이스, 데이터 흐름을 명시합니다.

---

## 2. 아키텍처 개요

### 2.1 모듈 위치

```
Layer 2: ImageProcTest.exe (C# GUI)
  └─ P/Invoke
     ↓
Layer 1: xpe_display.dll  ← THIS MODULE
  ├─ SWU-3.1 ModalityLUT
  ├─ SWU-3.2 VoiLUT
  ├─ SWU-3.3 PresentationLUT
  └─ SWU-3.4 LUTManager
  
  의존성:
  ├─ xpe_enhance_advanced.dll (입력 이미지)
  ├─ xpe_common.dll (메모리, 에러, 로깅)
  └─ dcmtk (DICOM 태그 읽기/쓰기)
     
Layer 0: xpe_common.dll
```

### 2.2 처리 흐름 아키텍처

```
┌────────────────────────────────────────────────────────────┐
│                  DISPLAY PIPELINE                         │
│                                                            │
│  Input: float32 [0~4095]                                 │
│           (강화 도메인, 3072×3072)                       │
│                                                            │
│    v                                                       │
│  ┌──────────────────────────────────┐                    │
│  │  SWU-3.1 ModalityLUT             │                    │
│  │  · Slope/Intercept 또는 LUT      │                    │
│  │  · input → linear_value          │                    │
│  │  · float32 → float32             │                    │
│  │  · ~5ms                          │                    │
│  └──────────────────────────────────┘                    │
│    v                                                       │
│  ┌──────────────────────────────────┐                    │
│  │  SWU-3.2 VoiLUT                  │                    │
│  │  · Window/Level (Linear/Sigmoid) │                    │
│  │  · linear_value → voi_range      │                    │
│  │  · float32 → float32             │                    │
│  │  · ~10-30ms                      │                    │
│  └──────────────────────────────────┘                    │
│    v                                                       │
│  ┌──────────────────────────────────┐                    │
│  │  SWU-3.3 PresentationLUT/GSDF    │                    │
│  │  · DICOM PS3.14 GSDF             │                    │
│  │  · voi_range → p_value           │                    │
│  │  · float32 → uint16              │                    │
│  │  · FORMAT BOUNDARY               │                    │
│  │  · ~5ms                          │                    │
│  └──────────────────────────────────┘                    │
│    v                                                       │
│  ┌──────────────────────────────────┐                    │
│  │  SWU-3.4 LUTManager              │                    │
│  │  · Preset selection/caching      │                    │
│  │  · auto_select(body_part)        │                    │
│  │  · ~1-5ms                        │                    │
│  └──────────────────────────────────┘                    │
│    v                                                       │
│  Output: uint16 [0~65535]                                │
│           (표시 준비 완료, 3072×3072)                    │
│                                                            │
│  준비: DICOM 인코딩, 디스플레이 렌더링                   │
└────────────────────────────────────────────────────────────┘
```

---

## 3. 소프트웨어 단위 (SWU) 정의

### 3.1 SWU-3.1 ModalityLUT

**책임**: 원본 이미지를 vendor-independent linear value로 변환합니다.

#### 3.1.1 인터페이스

```cpp
// Slope/Intercept 방식
XpeErrorCode xpe_modality_lut_apply(
    XpeImageBuffer* input,       // float32 입력
    float slope,                 // DICOM (0028,1053)
    float intercept,             // DICOM (0028,1052)
    XpeImageBuffer* output       // float32 출력
);

// LUT 테이블 방식
XpeErrorCode xpe_modality_lut_apply_table(
    XpeImageBuffer* input,
    float* lut_table,
    uint32_t lut_size,
    XpeImageBuffer* output
);
```

#### 3.1.2 구현 상세

```cpp
// Pseudocode
for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
        float pixel = input[y][x];
        
        // Slope/Intercept 모드
        float output_pixel = slope * pixel + intercept;
        
        // 범위 검증 (선택)
        // output_pixel = clamp(output_pixel, MIN_LINEAR, MAX_LINEAR)
        
        output[y][x] = output_pixel;
    }
}
```

#### 3.1.3 데이터 흐름

```
slope, intercept (또는 lut_table)
     ↓
┌────────────────────┐
│ Validation        │
│ · slope > 0       │
│ · |intercept| ≤ 1000
│ · lut_size valid  │
└────────────────────┘
     ↓
┌────────────────────┐
│ Apply Transform   │
│ · Per-pixel op    │
│ · Linear mode     │
│ · LUT mode        │
└────────────────────┘
     ↓
float32 linear value (output)
```

---

### 3.2 SWU-3.2 VoiLUT

**책임**: Modality LUT 출력을 Window/Level로 매핑합니다.

#### 3.2.1 인터페이스

```cpp
// 선형 모드
XpeErrorCode xpe_voi_lut_apply_linear(
    XpeImageBuffer* input,
    float wc,  // Window Center
    float ww,  // Window Width
    XpeImageBuffer* output
);

// 시그모이드 모드
XpeErrorCode xpe_voi_lut_apply_sigmoid(
    XpeImageBuffer* input,
    float wc,
    float ww,
    XpeImageBuffer* output
);

// 고속 경로 (미리 계산된 LUT)
XpeErrorCode xpe_voi_lut_apply_fast(
    XpeImageBuffer* input,
    float* precomputed_lut,      // [0~4095] → output
    XpeImageBuffer* output
);
```

#### 3.2.2 알고리즘 상세

**선형 모드**:
```cpp
float level_min = wc - ww / 2.0f;
float level_max = wc + ww / 2.0f;
float range = ww;

for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
        float input_pixel = input[y][x];
        
        if (input_pixel <= level_min) {
            output[y][x] = 0.0f;
        } else if (input_pixel >= level_max) {
            output[y][x] = MAX_VALUE;
        } else {
            float normalized = (input_pixel - level_min) / range;
            output[y][x] = normalized * MAX_VALUE;
        }
    }
}
```

**시그모이드 모드**:
```cpp
for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
        float input_pixel = input[y][x];
        float exponent = -4.0f * (input_pixel - wc) / ww;
        float sigmoid = 1.0f / (1.0f + expf(exponent));
        output[y][x] = sigmoid * MAX_VALUE;
    }
}
```

#### 3.2.3 Preset 자동 선택

```cpp
XpeErrorCode xpe_voi_lut_auto_select(
    const char* body_part,       // "chest", "extremity", ...
    char* output_lut_id
) {
    // Mapping table
    std::map<std::string, std::string> mapping = {
        {"chest", "chest_pa"},
        {"extremity", "extremity_bone"},
        {"spine", "spine_vertebral"},
        {"abdomen", "abdomen_liver"},
        {"pediatric", "pediatric_low_dose"}
    };
    
    if (mapping.find(body_part) != mapping.end()) {
        strcpy(output_lut_id, mapping[body_part].c_str());
        return XPE_OK;
    }
    return XPE_ERR_INVALID_PARAM;
}
```

---

### 3.3 SWU-3.3 PresentationLUT/GSDF

**책임**: VOI LUT 출력을 의료 display 광도 매핑(GSDF)으로 변환합니다.

#### 3.3.1 인터페이스

```cpp
// GSDF LUT 생성
XpeErrorCode xpe_gsdf_create_lut(
    float l_min_cd_m2,           // 최소 광도
    float l_max_cd_m2,           // 최대 광도
    float* lut_output,           // [1024] 출력 LUT
    uint32_t lut_size
);

// Display 파라미터 설정
XpeErrorCode xpe_gsdf_set_display_params(
    float peak_luminance_cd_m2,
    float ambient_illumination_lux,
    float gamma
);

// Presentation LUT 적용
XpeErrorCode xpe_presentation_lut_apply(
    XpeImageBuffer* input,       // float32 [0~4095]
    float* gsdf_lut,             // [1024]
    XpeImageBuffer* output       // uint16 [0~65535]
);

// Display 능력 검증
XpeErrorCode xpe_gsdf_check_display_capability(
    float measured_peak_luminance_cd_m2,
    float* deviation_percent     // 출력: 편차%
);
```

#### 3.3.2 GSDF 역함수 (p-value → luminance)

```cpp
float gsdf_inverse(uint32_t j) {
    // DICOM PS3.14 역함수
    // log10(L) = a + c*ln(j) + e*(ln(j))^2 + g*(ln(j))^3 + m*(ln(j))^4
    
    const float a = -2.0f, c = 2.4f, e = -0.0525f;
    const float g = -0.0205f, m = 0.0099f;
    
    if (j < 1) return 0.05f;  // 최소 광도
    
    float ln_j = logf((float)j);
    float log10_L = a + c*ln_j + e*ln_j*ln_j + g*ln_j*ln_j*ln_j + m*ln_j*ln_j*ln_j*ln_j;
    
    return powf(10.0f, log10_L);  // luminance cd/m²
}
```

#### 3.3.3 GSDF 순함수 (luminance → p-value)

```cpp
uint32_t gsdf_forward(float L) {
    // Newton-Raphson 수치 역계산
    // 목표: log10(L)를 만족하는 j 찾기
    
    float target_log10_L = log10f(L);
    float j = 500.0f;  // 초기값
    
    for (int iter = 0; iter < 10; iter++) {
        float ln_j = logf(j);
        float residual = calculate_residual(j);  // 위 역함수와의 차이
        
        if (fabsf(residual) < 1e-6f) break;
        
        // 미분값 계산 후 Newton-Raphson 스텝
        j = j - residual / derivative_at_j(j);
    }
    
    return (uint32_t)clampf(j, 0, 1023);
}
```

#### 3.3.4 FORMAT BOUNDARY (float32 → uint16)

```cpp
// Presentation LUT 적용 (최종 스텝)
for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
        float p_value = input[y][x];  // [0~4095]
        
        // 범위 검증
        if (isnan(p_value) || isinf(p_value)) {
            // 에러 처리
            metadata.flags |= XPE_FLAG_INVALID_FLOAT;
            continue;
        }
        
        p_value = clampf(p_value, 0.0f, 4095.0f);
        
        // float32 → uint16 변환
        // p_value [0, 4095] → uint16 [0, 65535]
        uint16_t display_code = (uint16_t)((p_value / 4095.0f) * 65535.0f);
        
        output[y][x] = display_code;
    }
}

metadata.flags |= XPE_FLAG_PRESENTATION_APPLIED;
```

---

### 3.4 SWU-3.4 LUTManager

**책임**: Preset 저장, 조회, 자동 선택, 보간을 관리합니다.

#### 3.4.1 Preset 저장 구조

```cpp
struct XpeLutPreset {
    char lut_id[64];
    char body_part[32];      // "chest", "extremity", ...
    char exam_type[32];      // "pa", "lateral", ...
    
    struct {
        float slope;
        float intercept;
    } modality_lut;
    
    struct {
        float wc, ww;
        const char* mode;    // "linear", "sigmoid"
    } voi_lut;
    
    struct {
        float l_min_cd_m2;
        float l_max_cd_m2;
    } presentation_lut;
    
    struct {
        char source[32];     // "factory", "user"
        char created_date[16];
        char version[8];
    } metadata;
};
```

#### 3.4.2 인터페이스

```cpp
// CRUD
XpeErrorCode xpe_lut_add_preset(const XpeLutPreset* preset, const char* lut_id);
XpeErrorCode xpe_lut_get_preset(const char* lut_id, XpeLutPreset* output);
XpeErrorCode xpe_lut_remove_preset(const char* lut_id);
XpeErrorCode xpe_lut_list_presets(XpeLutPresetInfo* list, uint32_t* count);

// 자동 선택
XpeErrorCode xpe_lut_auto_select(const char* body_part, char* output_lut_id);

// 보간
XpeErrorCode xpe_lut_interpolate(
    const XpeLutPreset* preset_low,
    const XpeLutPreset* preset_high,
    float t,                    // [0, 1]
    XpeLutPreset* output
);
```

#### 3.4.3 저장 경로 및 persistence

```
~/.xpe/luts/
├── factory/                  (Factory, read-only)
│   ├── chest_pa.json
│   ├── chest_lateral.json
│   ├── extremity.json
│   └── ...
├── user/                     (사용자, read-write)
│   └── my_custom_chest.json
└── cache/                    (계산 캐시)
    └── gsdf_lut_display_1.bin
```

#### 3.4.4 구현: JSON 직렬화

```cpp
// JSON 저장 (외부 라이브러리, 예: nlohmann/json)
nlohmann::json preset_json;
preset_json["lut_id"] = preset->lut_id;
preset_json["body_part"] = preset->body_part;
preset_json["modality_lut"]["slope"] = preset->modality_lut.slope;
preset_json["modality_lut"]["intercept"] = preset->modality_lut.intercept;
preset_json["voi_lut"]["wc"] = preset->voi_lut.wc;
preset_json["voi_lut"]["ww"] = preset->voi_lut.ww;
// ... 나머지 필드

std::ofstream file(filepath);
file << preset_json.dump(4);  // 4-space indent
file.close();
```

---

## 4. 데이터 흐름 및 타입

### 4.1 핵심 데이터 타입

```cpp
// XpeImageBuffer: 이미지 버퍼 (공유 포인터)
struct XpeImageBuffer {
    uint32_t width;
    uint32_t height;
    uint32_t bitsAllocated;      // 16 or 32
    uint32_t bitsStored;         // 14, 16, or 32
    PixelFormat format;          // UINT16, FLOAT32
    void* data;                  // non-owning pointer
    size_t dataSize;
    XpeImageMetadata metadata;
};

// XpeImageMetadata: 메타데이터
struct XpeImageMetadata {
    std::string bodyPart;        // DICOM (0018,0015)
    float kVp, mAs;
    float SID_mm, pixelPitch_mm;
    uint64_t acquisitionTime;
    uint32_t flags;              // XPE_FLAG_*
    char applied_lut_id[64];     // SWU-3.4로부터
};
```

### 4.2 메타데이터 플래그 생명주기

```
flags = 0x00000000  (입력)
  ↓
(SWU-3.1 실행)
  ├─ slope/intercept 설정 → 플래그 없음 (무조건 실행)
  ↓
(SWU-3.2 실행)
  ├─ VOI LUT 적용 → 플래그 없음 (무조건 실행)
  ↓
(SWU-3.3 실행)
  ├─ GSDF 적용
  └─ flags |= XPE_FLAG_PRESENTATION_APPLIED  (0x0100)
  ↓
(SWU-3.4 실행)
  ├─ LUT ID 저장 → metadata.applied_lut_id
  ├─ Clipping 감지 → flags |= XPE_FLAG_CLIPPED (0x0200)
  ↓
flags = 0x0100 | 0x0200 (또는 조합)
```

---

## 5. 통합 아키텍처

### 5.1 외부 의존성

```
xpe_display.dll
├─ xpe_enhance_advanced.dll
│  └─ 입력: float32 이미지 + 메타데이터 (body_part, EI)
├─ xpe_common.dll
│  ├─ 메모리 관리 (alloc/free)
│  ├─ 에러 처리 (error codes)
│  ├─ 로깅 (spdlog)
│  └─ 파라미터 검증
└─ dcmtk (라이브러리)
   └─ DICOM 태그 읽기
```

### 5.2 내부 인터페이스 계약

**Input Contract**:
- `input != NULL` and `input->data != NULL`
- `input->format == FLOAT32`
- `input->width * input->height ≤ 4096 * 4096`

**Output Contract**:
- `output` is allocated by caller (또는 SWU에서 alloc)
- `output->format == UINT16` (SWU-3.3 후)
- `output->dataSize >= width * height * 2 bytes`

**Error Handling**:
- 모든 함수는 `XpeErrorCode` 반환
- null pointer 확인 → `XPE_ERR_NULL_POINTER`
- 범위 검증 실패 → `XPE_ERR_INVALID_PARAM`
- 메모리 할당 실패 → `XPE_ERR_MEMORY`

---

## 6. 모듈 간 인터페이스 (IF)

### 6.1 IF-DISPLAY-001: 입력 인터페이스

```
Source: xpe_enhance_advanced.dll
Data: XpeImageBuffer (float32, 강화 도메인) + XpeImageMetadata
```

### 6.2 IF-DISPLAY-002: 출력 인터페이스

```
Consumer: xpe_dicom.dll (DICOM 인코딩) / GUI (렌더링)
Data: XpeImageBuffer (uint16, 표시 도메인) + XpeImageMetadata
```

### 6.3 IF-DISPLAY-003: 보조 인터페이스

```
Source: xpe_common.dll
Services:
  · MemoryPool (image buffer alloc/free)
  · ErrorHandler (error code mapping)
  · Logger (audit trail)
  · ParameterValidator (range checking)
```

---

## 7. 설계 결정 및 근거

| 결정 | 근거 |
|------|------|
| 4개 순차 SWU | Modality → VOI → Presentation 순서는 DICOM PS3.14 표준 |
| float32 중간 형식 | 정확성과 유연성 균형 (uint16 직접 변환은 precision 손실) |
| GSDF LUT 캐시 | 1024-entry 고정 크기로 메모리 효율적 |
| LUT JSON 저장 | 텍스트 기반, 사람이 읽을 수 있는 형식 |
| SWU-3.4 분리 | Preset 관리를 별도 단위로 모듈화 (재사용성) |

---

## 8. 아키텍처 검증

| 검증 항목 | 방법 | 합격 기준 |
|----------|------|---------|
| SRS 매핑 | RTM-DISPLAY-001 | 모든 SRS → 설계 매핑 |
| 인터페이스 완전성 | 형식 검토 | 모든 데이터 흐름 문서화 |
| 위험 통제 | SHA-DISPLAY-001 | 모든 위험 → 설계 반영 |
| 성능 | Profiling | 모든 SWU ≤ 예산 |

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-14 | XPE Display Team | Initial release |

---

*문서 끝 — SAD-DISPLAY-001 v1.0*
