# Software Architecture Document - XPE Basic Enhancement Module

**문서 ID**: SAD-ENHANCE-BASIC-001 v1.0  
**IEC 62304 절**: 5.3 (소프트웨어 아키텍처 설계)  
**안전 등급**: Class B  
**날짜**: 2026-04-14  
**저자**: XPE 아키텍처 팀  
**승인**: __________________ 날짜: __________

---

## 1. 목적 및 범위

### 1.1 목적

이 소프트웨어 아키텍처 문서는 XPE 기본 강화 모듈(`xpe_enhance_basic.dll`)의 구조적 설계를 정의합니다. SRS-ENHANCE-BASIC-001의 요건을 관리 가능한 소프트웨어 단위(SWU)로 분해하고, 책임, 인터페이스, 데이터 흐름, 의존성을 명시합니다.

### 1.2 범위

이 아키텍처는 강화 관련 소프트웨어 단위에만 적용됩니다:
- EI_Baseline (SWU-2.0)
- LogTransform (Stage 5)
- CLAHEProcessor (Stage 6)
- WindowLevelMapper (Stage 7)
- ExamProfileManager (프리셋 관리)

---

## 2. 시스템 컨텍스트

### 2.1 레이어 아키텍처

```
Layer 2  ImageProcTest.exe (C# WPF GUI)
         ↓ P/Invoke (C ABI)
Layer 1  xpe_enhance_basic.dll ← [이 모듈]
         ↓ 링크 의존성
         xpe_preprocess.dll ← 입력 (보정된 float32)
         ↓
Layer 0  xpe_common.dll (타입, 메모리, 에러, 알림)
```

### 2.2 데이터 흐름

```
float32 검출기 도메인 (xpe_preprocess.dll 출력)
    ↓ [메타데이터: 검사 유형, 온도]
    ↓
[EI_Baseline: SWU-2.0]
    ↓ [EI, DI, 우회 플래그]
    ↓
[LogTransform: Stage 5]
    ↓ [log(I + ε)]
    ↓ [강화 도메인 시작]
[CLAHEProcessor: Stage 6]
    ↓ [타일 히스토그램, 클립, 보간]
    ↓
[WindowLevelMapper: Stage 7]
    ↓ [WC/WW 매핑]
    ↓
float32 강화 도메인 (Phase 2 입력)
```

### 2.3 외부 인터페이스 의존성

| 시스템 | 프로토콜 | 방향 | 목적 |
|--------|---------|------|------|
| **xpe_common.dll** | C ABI (링크 시) | Import | 타입, 메모리 유틸, 에러 코드, 알림 큐 |
| **xpe_preprocess.dll** | 데이터 파이프라인 | Input | float32 검출기 도메인 이미지 |
| **구성 파일** | JSON | Input | 검사 프로필, CLAHE 파라미터, Window 프리셋 |
| **C# GUI (ImageProcTest)** | P/Invoke / C ABI | Bidirectional | 처리 요청, 상태 조회, 메타데이터 반환 |

---

## 3. 소프트웨어 항목 및 단위

### 3.1 소프트웨어 항목 1: EnhanceBasicProcessor (SWI-EB-1)

중앙 오케스트레이터로서 모든 강화 단계를 조율합니다.

#### 3.1.1 소프트웨어 단위 (SWU) 분해

| SWU ID | 이름 | 책임 | 의존성 | 스레드 |
|--------|------|------|--------|--------|
| **SWU-2.0** | EI_Baseline | EI 계산 (K_gain × Q_mean), DI (로그), 우회 로직 | 구성, 입력 메타데이터 | Main |
| **SWU-2.1** | LogTransform | 로그 변환: `log(I + ε)` | 입력 이미지 | Main |
| **SWU-2.2** | CLAHEProcessor | 타일 기반 히스토그램 균등화, 클립, 보간 | 구성, 입력 이미지 | Main |
| **SWU-2.3** | WindowLevelMapper | VOI LUT 매핑: 선형/비선형 windowing | 구성 (프리셋), 입력 이미지 | Main |
| **SWU-2.4** | ExamProfileManager | 검사 프로필 로드, 프리셋 선택 | 구성 파일 | Main (초기화) |

#### 3.1.2 책임 및 동작

**SWU-2.0 EI_Baseline**:
- 입력: float32 이미지 + K_gain + EI_T + 메타데이터 (검사 유형, 스티치 플래그)
- 연산: `EI = K_gain × mean(I)`, `DI = 10 × log10(EI / EI_T)`
- 출력: EI 값, DI 값, 우회 플래그
- 우회 조건: 스티치 이미지, 다중 조사 → EI 계산 건너뜀, 플래그 설정
- 에러 처리: K_gain 없음 → `XPE_ERR_NOT_INITIALIZED`, EI_T 없음 → 기본값 사용

**SWU-2.1 LogTransform**:
- 입력: float32 이미지
- 연산: 픽셀별 `log(pixel + ε)`, ε = max(1e-6 × range, 1e-3)
- 출력: float32 로그 변환 이미지
- 에러 처리: ε <= 0 → 오류, 오버플로우 → clamp to 3.4e38

**SWU-2.2 CLAHEProcessor**:
- 입력: float32 이미지 + 구성 (tile_size, clip_limit)
- 연산: 타일 분할 → 로컬 히스토그램 → 클립 → CDF → 쌍선형 보간
- 출력: float32 CLAHE 처리 이미지
- 우회 조건: 구성 `clahe_enabled = false` → 입력 통과
- 에러 처리: 타일 크기 유효성 → `XPE_ERR_INVALID_PARAM`

**SWU-2.3 WindowLevelMapper**:
- 입력: float32 이미지 + WC, WW, 모드 (선형/비선형)
- 연산: 선형: `(I - (WC - WW/2)) / WW` → clip [0, 1], 비선형: sigmoid
- 출력: float32 window-level 이미지 [0, 1]
- 우회 조건: 구성 `windowing_enabled = false` → 입력 통과
- 에러 처리: WW <= 0 → `XPE_ERR_INVALID_PARAM`

**SWU-2.4 ExamProfileManager**:
- 입력: 구성 파일 (JSON), 검사 유형
- 연산: JSON 파싱, 검사 프로필 선택, CLAHE/Window 파라미터 추출
- 출력: 병합된 구성 (tile_size, clip_limit, WC, WW)
- 에러 처리: 프로필 없음 → 기본값 사용, JSON 오류 → `XPE_ERR_INVALID_PARAM`

---

## 4. 인터페이스 명세

### 4.1 메인 API

```c
// 메인 처리 함수
XpeErrorCode xpe_enhance_basic_process(
    XpeImageBuffer* input_image,           // 입력 (검출기 도메인)
    const XpeEnhanceConfig* config,         // 구성
    XpeImageBuffer* output_image,           // 출력 (강화 도메인)
    XpeImageMetadata* metadata              // 메타데이터 (EI, DI 포함)
);
```

**반환값**:
- `XPE_OK`: 성공
- `XPE_ERR_NOT_INITIALIZED`: 구성 누락
- `XPE_ERR_INVALID_PARAM`: 파라미터 범위 외
- `XPE_ERR_INVALID_CALIB_DATA`: K_gain/EI_T 없음

### 4.2 SWU 레벨 함수

```c
// EI 계산
XpeErrorCode xpe_ei_compute_baseline(
    const XpeImageBuffer* input,
    float k_gain,
    float ei_t,
    const XpeImageMetadata* meta,
    float* out_ei, float* out_di,
    uint32_t* out_flags
);

// Log Transform
XpeErrorCode xpe_log_transform(
    XpeImageBuffer* image,
    float epsilon
);

// CLAHE
XpeErrorCode xpe_clahe_process(
    XpeImageBuffer* image,
    uint32_t tile_size,
    float clip_limit
);

// Window/Level
XpeErrorCode xpe_window_level_apply(
    XpeImageBuffer* image,
    float wc, float ww,
    XpeWindowMode mode  // LINEAR, SIGMOID
);
```

### 4.3 구성 JSON 스키마

```json
{
  "enhance_basic": {
    "ei_baseline": {
      "enabled": true,
      "k_gain_file": "detector_k_gain.json",
      "ei_t_defaults": {
        "chest": 100.0,
        "skeletal": 120.0,
        "abdomen": 110.0
      },
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
        "chest": {"lung": {"wc": -400, "ww": 1500}, "mediastinum": {"wc": 40, "ww": 400}},
        "skeletal": {"bone": {"wc": 300, "ww": 1500}}
      }
    }
  }
}
```

---

## 5. 메모리 레이아웃 및 버퍼 관리

### 5.1 버퍼 소유권

| 버퍼 | 크기 | 소유자 | 생명 주기 |
|------|------|--------|----------|
| 입력 (float32) | 37.7 MB | 호출자 | 함수 호출 전 할당, 호출 후 해제 |
| 출력 (float32) | 37.7 MB | 호출자 | 함수 호출 전 할당, 호출 후 해제 |
| CLAHE 작업 | < 10 MB | 모듈 (임시) | 함수 호출 중만 할당, 종료 후 해제 |

### 5.2 메모리 할당 전략

- **스택**: 작은 임시 (< 1 MB)
- **힙**: 타일 히스토그램, CDF 테이블 (함수 종료 시 해제)
- **메모리 누수 예방**: 모든 할당에 대응하는 해제 필수

---

## 6. 에러 처리 및 복구

| 에러 | SWU | 원인 | 복구 |
|------|-----|------|------|
| `XPE_ERR_NOT_INITIALIZED` | SWU-2.0 | K_gain/EI_T 파일 없음 | 기본값 사용 또는 파이프라인 실패 |
| `XPE_ERR_INVALID_PARAM` | 모두 | 파라미터 범위 외 | 에러 로그 + 함수 반환 |
| `XPE_ERR_INVALID_CALIB_DATA` | SWU-2.0 | K_gain 범위 외 (0.001~1.0) | 파이프라인 실패 |

---

## 7. 안티-스파게티 규칙

**의존성 제약**:
- `xpe_enhance_basic.dll`은 `xpe_preprocess.dll`과 `xpe_common.dll`에만 의존
- 다른 Phase 1b 모듈(`gsvg.dll` 등)과 횡단 의존성 없음
- 모든 공유 타입과 유틸은 `xpe_common.dll` 통해 전달

---

## 8. 단위 분해 기준

| 기준 | 적용 |
|------|------|
| **책임 분리** | SWU별로 한 가지 목적 (EI, Log, CLAHE, Window) |
| **재사용성** | 각 SWU는 독립적으로 호출 가능 |
| **테스트 가능성** | 모든 SWU는 단위 테스트 가능 |
| **통합 용이성** | 명확한 입/출력 인터페이스 |

---

## 부록 A: 타일 크기 계산 예시

```
이미지 크기: 3072 × 3072
타일 크기: 64
타일 수: (3072 / 64) × (3072 / 64) = 48 × 48 = 2304 타일
히스토그램당 크기: 256 × 4 바이트 (float) = 1 KB
총 히스토그램 메모리: 2304 × 1 KB = ~2.3 MB
```

---

**문서 끝**

작성자: XPE 아키텍처 팀  
승인: __________________ 날짜: __________
