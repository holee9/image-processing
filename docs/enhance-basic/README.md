# X-ray FPD 기본 강화 모듈 (xpe_enhance_basic.dll)

**모듈**: `xpe_enhance_basic.dll`  
**레이어**: Layer 1, Phase 1b  
**안전 등급**: IEC 62304 Class B  
**버전**: 1.0  
**날짜**: 2026-04-14

---

## 개요

`xpe_enhance_basic.dll`은 X-ray FPD 이미지 처리 엔진의 기본 강화 모듈입니다. 캘리브레이션 전처리(`xpe_preprocess.dll`) 출력인 float32 검출기 도메인 이미지를 받아서 4개 처리 단계를 거쳐 float32 강화 도메인 이미지로 변환합니다.

### 핵심 기능

1. **EI 기준선 (SWU-2.0)**: IEC 62494-1 노출 지수 계산 (검출기 도메인에서만)
2. **Log Transform (Stage 5)**: 동적 범위 압축 및 시각적 선형성 개선
3. **CLAHE (Stage 6)**: Pizer 1987 적응형 히스토그램 균등화로 국소 대비 강화
4. **Window/Level (Stage 7)**: DICOM VOI LUT 호환 조직 최적화

### 데이터 흐름

```
float32 검출기 도메인 (보정됨)
    ↓ [SWU-2.0: EI/DI 계산]
    ↓ [Stage 5: log(I + ε)]
    ↓ [강화 도메인 시작]
    ↓ [Stage 6: CLAHE 타일 처리]
    ↓ [Stage 7: Window/Level]
float32 강화 도메인 (Phase 2 입력)
```

---

## 임계 규칙 (반드시 준수)

### Rule 1: EI는 검출기 도메인에서만 계산

```
❌ WRONG: Log transform 후 EI 계산
✓ CORRECT: Log transform 전 EI 계산 (SWU-2.0)
```

**근거**: IEC 62494-1 표준에서 EI는 검출기의 원시 신호 레벨을 기반으로 정의. 로그 변환 후 계산하면 지수 압축으로 인해 EI가 과다 추정됨.

### Rule 2: Log Transform은 필수 단계

```
❌ WRONG: CLAHE 없이 Log Transform 우회 가능
✓ CORRECT: Log Transform은 항상 실행 (우회 불가)
```

**근거**: Log Transform은 강화 파이프라인의 기초. CLAHE와 Window 처리가 로그 영역에서 동작하도록 설계됨.

### Rule 3: CLAHE 클립 한계는 [0.01, 0.1] 범위

```
clip_limit = 0.01: 보수적 (약한 강화, 노이즈 최소)
clip_limit = 0.03: 기본 (균형잡힌 강화)
clip_limit = 0.10: 공격적 (강한 강화, 노이즈 증폭 위험)
범위 외: 오류 반환
```

### Rule 4: 단조성 보증

Log 함수는 단조증가: `I1 < I2 ⟹ log(I1) < log(I2)`

대비 반전 없음. 픽셀 순서는 항상 보존됨.

---

## 성능 예산

### 처리 시간 (Phase 1b 전체 < 80ms/프레임)

| 단계 | 예산 (ms) | 예상 (ms) |
|------|:---------:|:---------:|
| SWU-2.0 (EI) | 15 | 8 |
| Stage 5 (Log) | 20 | 15 |
| Stage 6 (CLAHE) | 50 | 35 |
| Stage 7 (Window) | 10 | 5 |
| **합계** | **95** | **63** |

### 메모리 (최대 < 100MB)

| 구성 | 크기 |
|------|------|
| 입력 버퍼 (float32) | 37.7 MB |
| 출력 버퍼 (float32) | 37.7 MB |
| CLAHE 작업 | < 10 MB |
| **합계** | < 100 MB |

---

## API 레퍼런스

### 메인 처리 함수

```c
XpeErrorCode xpe_enhance_basic_process(
    XpeImageBuffer* input_image,    // 입력 (검출기 도메인)
    const XpeEnhanceConfig* config,  // 구성
    XpeImageBuffer* output_image,    // 출력 (강화 도메인)
    XpeImageMetadata* metadata       // 메타데이터 (EI, DI)
);
```

**반환값**:
- `XPE_OK`: 성공
- `XPE_ERR_NOT_INITIALIZED`: 구성 누락
- `XPE_ERR_INVALID_PARAM`: 파라미터 범위 외

### SWU 레벨 함수

```c
// EI 계산
xpe_ei_compute_baseline(input, k_gain, ei_t, meta, &ei, &di, &flags);

// Log Transform
xpe_log_transform(image, epsilon);

// CLAHE
xpe_clahe_process(image, tile_size, clip_limit);

// Window/Level
xpe_window_level_apply(image, wc, ww, mode);
```

---

## 구성 (JSON)

```json
{
  "enhance_basic": {
    "ei_baseline": {
      "enabled": true,
      "k_gain_file": "detector_k_gain.json",
      "suppress_stitched": true,
      "alert_di_threshold": 3.0
    },
    "log_transform": {
      "enabled": true,
      "epsilon_fraction": 1e-6
    },
    "clahe": {
      "enabled": true,
      "exam_presets": {
        "chest": {"tile_size": 64, "clip_limit": 0.02},
        "skeletal": {"tile_size": 48, "clip_limit": 0.04},
        "abdomen": {"tile_size": 64, "clip_limit": 0.03}
      }
    },
    "windowing": {
      "enabled": true,
      "mode": "linear",
      "exam_presets": {
        "chest": {
          "lung": {"wc": -400, "ww": 1500},
          "mediastinum": {"wc": 40, "ww": 400}
        }
      }
    }
  }
}
```

---

## EI/DI 해석 가이드

### DI (용량 지수) 대역

```
DI < -3: 저선량 경고 (이미지 노이즈 증가)
-1 ≤ DI ≤ +1: 수락 범위 (표준 선량)
DI > +3: 과다 선량 경고 (환자 선량 증가)
```

### 임상 의미

| DI | 의미 | 조치 |
|-----|------|------|
| < -3 | 저선량 | 검사 반복 권고 |
| -1~0 | 약간 저선량 | 수락 가능 |
| 0 | 표준 | 최적 |
| 0~+1 | 약간 과다 | 수락 가능 |
| > +3 | 과다 선량 | 프로토콜 검토 |

---

## CLAHE 파라미터 가이드

### 검사별 권장 설정

| 검사 | 타일 크기 | 클립 한계 | 용도 |
|------|---------|----------|------|
| **흉부** | 64 | 0.02 | 폐 세부, 미묘한 결절 |
| **골격** | 48 | 0.04 | 뼈 세부, 골다공증 |
| **복부** | 64 | 0.03 | 간 세부, 간경변증 |
| **소아** | 32 | 0.01 | 노이즈 민감도 (낮음) |

### 클립 한계의 효과

```
clip_limit = 0.01: 보수적 → 노이즈 최소, 강화 약함
clip_limit = 0.03: 균형 → 대비 강화 + 노이즈 수용 가능
clip_limit = 0.10: 공격적 → 강한 강화, 노이즈 눈에 띔
```

---

## Window 프리셋 테이블

### 흉부

| 용도 | WC | WW | 설명 |
|------|-----|-----|------|
| 폐 | -400 | 1500 | 폐 결절, 미묘한 결절 감지 |
| 종격동 | 40 | 400 | 종격동 윤곽, 심장 경계 |
| 뼈 | 500 | 2000 | 늑골, 척추 골다공증 |

### 골격

| 용도 | WC | WW |
|------|-----|-----|
| 뼈 | 300 | 1500 |
| 치아 | 250 | 1000 |

### 복부

| 용도 | WC | WW |
|------|-----|-----|
| 간 | 60 | 150 |
| 신장 | 40 | 350 |
| 뼈 | 400 | 2000 |

### 투시 (형광)

| 용도 | WC | WW |
|------|-----|-----|
| 통상 | 100 | 500 |

---

## 우회 정책

| 단계 | 우회 가능? | 구성 | 안전성 |
|------|:---------:|------|--------|
| SWU-2.0 (EI) | ✓ | `ei_enabled: false` | 낮음 (선량 지표만) |
| Stage 5 (Log) | **✗** | -- | 필수 (기초) |
| Stage 6 (CLAHE) | ✓ | `clahe_enabled: false` | 중간 (대비 저하) |
| Stage 7 (Window) | ✓ | `windowing_enabled: false` | 높음 (선형 출력) |

**주의**: Log Transform 우회는 불가능합니다. 파이프라인의 기초입니다.

---

## 안전 제약 조건

### 데이터 도메인 규칙 (필수)

```
검출기 도메인 (xpe_preprocess.dll 출력)
    └─ EI/DI만 계산 ← SWU-2.0
    └─ QC 메트릭 계산

강화 도메인 (Stage 5 로그 변환 후)
    └─ CLAHE, Window/Level 처리 ← Stage 6, 7
    └─ 임상 해석용

프리젠테이션 도메인 (gsvg.dll 이후)
    └─ GSDF/LUT 적용
    └─ 디스플레이 렌더링
```

**위반 시 결과**:
- ❌ 검출기 도메인에서 CLAHE 적용 → EI 과다 추정
- ❌ 강화 후 QC 메트릭 계산 → 부정확한 DQE/MTF
- ❌ 프리젠테이션 후 Window 적용 → 진단 오류

---

## 참고문헌

### 표준

- **IEC 62494-1:2022**: Exposure Index
- **DICOM PS3.4**: VOI LUT (Window)
- **IEC 62304**: Software Lifecycle (Class B)

### 논문

- **Pizer et al. 1987**: CLAHE 원본 연구
- **Samei et al. 2015**: 강화 효과와 진단 신뢰도
- **Niemann et al. 2019**: 윈도우 프리셋 임상 유효성
- **AAPM TG-232**: EI/DI 임상 응용

---

## 문서 패키지

| 문서 | 대상 | 내용 |
|------|------|------|
| **xpe-enhance-basic-prd.md** | 개발자 | 알고리즘 요구사항, 공식, 절차 |
| **SRS-ENHANCE-BASIC-001** | 개발자 | 기능/안전/성능 요건 (40개+) |
| **SAD-ENHANCE-BASIC-001** | 개발자 | 아키텍처, SWU, 인터페이스 |
| **SHA-ENHANCE-BASIC-001** | 안전담당자 | 위험 식별, 통제 (7개 위험) |
| **RTM-ENHANCE-BASIC-001** | QA | 요건 추적 (SRS↔SAD↔테스트↔위험) |
| **IAP-ENHANCE-BASIC-001** | 캘리브레이션 | 영상 취득 프로토콜 |
| **TDS-ENHANCE-BASIC-001** | QA | 테스트 데이터, Golden Reference |
| **README.md (이 파일)** | 모두 | 기술 개요, 가이드 |

---

## 연락처

- **개발**: XPE 강화팀
- **품질**: QA팀
- **안전**: 안전팀

---

**문서 끝**

버전: 1.0  
작성 날짜: 2026-04-14
