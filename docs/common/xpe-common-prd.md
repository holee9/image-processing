# X-ray FPD 공통 기반 모듈 (xpe_common.dll) 제품 요구사항 명세서

**Module**: `xpe_common.dll` (Layer 0, 기반 인프라)  
**Safety Classification**: IEC 62304 Class B  
**Document Version**: 1.0.0  
**Date**: 2026-04-14  
**Owner**: XPE Development Team  
**Normative Authority**: SPEC-XPE-MASTER v2.0.0 §3.5

---

## 목차

1. [개요](#개요)
2. [아키텍처 역할](#아키텍처-역할)
3. [7개 소프트웨어 단위 (SWU)](#7개-소프트웨어-단위-swu)
   - [SWU-5.1 MemoryPool](#swu-51-memorypool)
   - [SWU-5.2 TypeDefinitions](#swu-52-typedefinitions)
   - [SWU-5.3 ErrorHandler](#swu-53-errorhandler)
   - [SWU-5.4 NotificationSystem](#swu-54-notificationsystem)
   - [SWU-5.5 JsonConfig](#swu-55-jsonconfig)
   - [SWU-5.6 ParameterValidator](#swu-56-parametervalidator)
   - [SWU-5.7 PipelineOrchestrator](#swu-57-pipelineorchestrator)
4. [Pack=8 정렬 제약 (CRITICAL)](#pack8-정렬-제약-critical)
5. [XpeImage 구조 설계](#xpeimage-구조-설계)
6. [메모리 풀 설계](#메모리-풀-설계)
7. [에러 처리 전략](#에러-처리-전략)
8. [알림 시스템 (XPE Event System)](#알림-시스템-xpe-event-system)
9. [설정 관리 (JsonConfig)](#설정-관리-jsonconfig)
10. [매개변수 검증](#매개변수-검증)
11. [P/Invoke 브리지 (C# 상호작용)](#pinvoke-브리지-c-상호작용)
12. [성능 및 메모리 예산](#성능-및-메모리-예산)
13. [제약 조건 및 의존성](#제약-조건-및-의존성)

---

## 개요

`xpe_common.dll`은 **모든 XPE Layer 1 DLL들의 기반**이다. X-ray FPD 이미지 처리 엔진의 모든 단계에 사용되는 공유 인프라 서비스를 제공한다.

### 핵심 책임

- **메모리 관리**: 제로카피 이미지 전송을 위한 메모리 풀 할당
- **공유 데이터 타입**: Pack=8 정렬된 `XpeImage`, `XpeImageMetadata`, `XpeRect`, `PixelFormat` 정의
- **에러 처리**: 구조화된 에러 전파 및 JSON 진단
- **비동기 알림**: 스레드 안전한 이벤트 디스패처 (XPE Event System)
- **설정 관리**: JSON 기반 매개변수 로딩 및 핫-리로드
- **매개변수 검증**: 파이프라인 실행 전 안전 범위 검사
- **C# 오케스트레이션**: P/Invoke를 통한 WPF GUI 통합

### 의존성 팬-아웃

```
ImageProcTest.exe (C# WPF) ─┐
                            │ P/Invoke (Pack=8 structs)
                            v
                    xpe_common.dll
                            │
        ┌───────────────────┼───────────────────┐
        │                   │                   │
        v                   v                   v
xpe_preprocess.dll  xpe_enhance_basic.dll  xpe_enhance_advanced.dll
        │                   │                   │
        └───────────────────┼───────────────────┘
                            │ 링크 의존성
                    (다른 모든 DLL)
```

**중요**: `xpe_common.dll`은 다른 XPE DLL에 의존하지 않음. 다른 모든 DLL이 `xpe_common.dll`에 의존함. 단방향 의존성으로 순환 참조 방지.

---

## 아키텍처 역할

### Layer 0 위치 (기반 인프라)

```
Layer 2: ImageProcTest.exe (C# WPF 오케스트레이터)
           ↑ P/Invoke
Layer 1:  xpe_preprocess.dll  xpe_enhance_basic.dll  xpe_ai.dll  ...
           ↑ 링크                ↑ 링크                ↑ 링크
Layer 0:   ┌──────────────────────────────────────────────────────┐
           │         xpe_common.dll (공유 인프라)                │
           │                                                      │
           │ - 메모리 풀                                           │
           │ - 공유 타입 정의                                       │
           │ - 에러 처리 서비스                                     │
           │ - XPE Event System                          │
           │ - JSON 설정 로더                                      │
           │ - 매개변수 검증기                                      │
           │ - C# P/Invoke 브리지                                  │
           │                                                      │
           └──────────────────────────────────────────────────────┘
```

### 스파게티 방지 원칙

- **원칙**: Layer 1 DLL들은 `xpe_common.dll`을 통해서만 통신
- **구현**: 횡단 의존성 금지 (예: `xpe_preprocess` → `xpe_enhance_basic` 직접 호출 불가)
- **공유 타입**: 모든 공유 데이터는 `xpe_common.h`에 정의된 struct 사용
- **에러 전파**: `XpeError` enum과 `xpe_get_last_error_detail()` 함수로 표준화

---

## 7개 소프트웨어 단위 (SWU)

### SWU-5.1 MemoryPool

**책임**: 제로카피 이미지 전송을 위한 미리 할당된 슬래브 할당기 구현

#### 설계 목표

1. **제로카피**: 이미지 데이터는 복사되지 않음. 호출자가 풀 슬롯 포인터 수신
2. **고정 크기 슬롯**: 크기 변동 불가. 크기 불일치 시 에러
3. **참조 카운팅**: 슬롯은 파이프라인 단계 완료 시 해제됨
4. **스레드 안전**: 동시 할당/해제 가능

#### 메모리 구성

| 버퍼 유형 | 크기 | 슬롯 수 | 총 크기 | 용도 |
|----------|------|--------|--------|------|
| float32 (3072×3072×4) | 37.7 MB | 4개 | 150.8 MB | 보정된 이미지, 임시 작업 |
| uint16 (3072×3072×2) | 18.9 MB | 4개 | 75.6 MB | 원본 프레임, 중간 단계 |
| **총 메모리** | — | — | **226.4 MB** | 피크 사용량 |

#### API 함수

| 함수 | 입력 | 출력 | 반환값 |
|------|------|------|--------|
| `xpe_mempool_alloc(width, height, format)` | 이미지 차원, 형식 | `void* data` (풀 슬롯) | `XpeError` |
| `xpe_mempool_free(ptr)` | 포인터 | 참조 카운팅 감소 | `XpeError` |
| `xpe_mempool_get_stats()` | — | JSON: 할당됨/사용가능/피크 | `char* json` |

#### 에러 조건

- **XPE_ERR_POOL_EXHAUSTED**: 모든 슬롯이 사용 중
- **XPE_ERR_INVALID_SIZE**: 요청 크기 ≠ 정의된 슬롯 크기
- **XPE_ERR_INVALID_FORMAT**: `PixelFormat` 값이 범위 밖

#### 참조 카운팅 라이프사이클

```
Frame arrives
  ↓
mempool_alloc() → get_ptr + refcount=1
  ↓
Stage 1: use ptr, refcount still 1
  ↓
Stage 2: use ptr, refcount still 1
  ↓
Stage 3: done → mempool_free() → refcount=0
  ↓
[Slot available for reallocation]
```

---

### SWU-5.2 TypeDefinitions

**책임**: 모든 XPE DLL이 공유하는 데이터 타입 정의

#### CRITICAL: Pack=8 정렬

```c
#pragma pack(8)

// 모든 struct는 8바이트 정렬을 유지해야 함
// static_assert(offsetof(XpeImage, xpeFlags) == 56);
// static_assert(sizeof(XpeImage) == 64);  // 8의 배수

struct XpeImage {
    uint32_t width;                    // offset 0
    uint32_t height;                   // offset 4
    uint32_t bitsAllocated;            // offset 8  (16 or 32)
    uint32_t bitsStored;               // offset 12 (14, 16, or 32)
    PixelFormat format;                // offset 16 (enum: UINT16=0, FLOAT32=1)
    void* data;                        // offset 20 (8바이트 포인터)
    size_t dataSize;                   // offset 28
    XpeImageMetadata metadata;         // offset 36 (192 바이트)
    uint32_t xpeFlags;                 // offset 228
    uint32_t padding;                  // offset 232 (Pack=8 정렬)
};                                     // 총 크기: 240 바이트 (8의 배수)

#pragma pack()
```

#### XpeImage 필드 설명

| 필드 | 타입 | 범위 | 설명 |
|------|------|------|------|
| `width` | uint32_t | 512~4096 | 검출기 가로 픽셀 수 |
| `height` | uint32_t | 512~4096 | 검출기 세로 픽셀 수 |
| `bitsAllocated` | uint32_t | {16, 32} | 메모리에 할당된 비트 (ADC 출력) |
| `bitsStored` | uint32_t | {14, 16, 32} | 실제 데이터 비트 수 (하위 비트 사용) |
| `format` | PixelFormat | {0=UINT16, 1=FLOAT32} | 픽셀 데이터 형식 |
| `data` | void* | — | **비소유 포인터** (MemoryPool 슬롯 또는 외부) |
| `dataSize` | size_t | width×height×bytes_per_pixel | 바이트 단위 크기 |
| `metadata` | XpeImageMetadata | — | DICOM + 센서 메타데이터 (192 바이트) |
| `xpeFlags` | uint32_t | 0x0000~0x03FF | 처리 단계 비트마스크 |
| `padding` | uint32_t | 0 | Pack=8 정렬용 패딩 |

#### XpeImageMetadata 상세

```c
#pragma pack(8)

struct XpeImageMetadata {
    char bodyPart[64];           // DICOM (0018,0015) 신체 부위
    float kVp;                   // 튜브 전압 (kV) [40~150]
    float mAs;                   // 튜브 전류량 (mAs) [0.1~500]
    float sdd;                   // 소스-검출기 거리 (mm) [400~1500]
    float pixelSpacingMm;        // 검출기 픽셀 피치 (mm) [0.1~0.5]
    uint64_t acquisitionTime;    // Unix 타임스탬프 (ms)
    char detectorId[32];         // 검출기 시리얼 번호
    float temperature;           // 검출기 온도 (°C) [-10~85]
    char reserved[96];           // 향후 확장용 (0으로 초기화)
};                               // 총 크기: 192 바이트

#pragma pack()
```

#### PixelFormat Enum

```c
typedef enum {
    XPE_PIXELFORMAT_UINT16 = 0,  // 16-bit unsigned integer (원본 ADC)
    XPE_PIXELFORMAT_FLOAT32 = 1  // 32-bit float (보정된 데이터)
} PixelFormat;
```

#### XPE_FLAG_* 비트마스크

| 비트 | 플래그 | 값 | 설정 시기 | 의미 |
|-----|--------|-----|---------|------|
| 0 | XPE_FLAG_CALIBRATED | 0x0001 | Stage 1a (Gain 완료) | 캘리브레이션 완료 |
| 1 | XPE_FLAG_GHOST_CORRECTED | 0x0002 | Stage 1a (Ghost 완료) | 래그 보정 적용됨 |
| 2 | XPE_FLAG_DEFECT_CORRECTED | 0x0004 | Stage 1a (Defect 완료) | 불량 픽셀 보정됨 |
| 3 | XPE_FLAG_LOG_TRANSFORMED | 0x0008 | Stage 1b (Log 완료) | 로그 도메인 변환됨 |
| 4 | XPE_FLAG_DENOISED | 0x0010 | Stage 1b (Noise 완료) | 노이즈 제거됨 |
| 5 | XPE_FLAG_EDGE_ENHANCED | 0x0020 | Stage 1b (Edge 완료) | 엣지 강조됨 |
| 6 | XPE_FLAG_COLLIMATED | 0x0040 | Stage 2 (Collimation) | 콜리메이션 감지됨 |
| 7 | XPE_FLAG_EI_COMPUTED | 0x0080 | Stage 1b / Stage 2 | 노출 지수 계산됨 |
| 8 | XPE_FLAG_AI_PROCESSED | 0x0100 | Stage 3 (AI 완료) | AI 처리 완료 |
| 9 | XPE_FLAG_GSDF_APPLIED | 0x0200 | Stage 1b (Display) | GSDF 표시 적용됨 |

---

### SWU-5.3 ErrorHandler

**책임**: 구조화된 에러 전파 및 진단 정보 제공

#### XpeError Enum (50+ 에러 코드)

```c
typedef enum {
    // 성공
    XPE_OK = 0,
    
    // 입력 검증 (100~109)
    XPE_ERR_INVALID_INPUT = 100,           // 매개변수 범위 밖
    XPE_ERR_INVALID_SIZE = 101,            // 버퍼 크기 불일치
    XPE_ERR_INVALID_FORMAT = 102,          // PixelFormat 값 범위 밖
    XPE_ERR_NULL_POINTER = 103,            // Null 포인터 전달
    XPE_ERR_INVALID_DIMENSIONS = 104,      // 이미지 해상도 범위 밖
    
    // 메모리 (110~119)
    XPE_ERR_POOL_EXHAUSTED = 110,          // 메모리 풀 슬롯 없음
    XPE_ERR_ALLOCATION_FAILED = 111,       // malloc/new 실패
    
    // 초기화 (120~129)
    XPE_ERR_NOT_INITIALIZED = 120,         // 캘리브레이션 데이터 미로드
    XPE_ERR_ALREADY_INITIALIZED = 121,     // 중복 초기화 시도
    
    // 캘리브레이션 (130~149)
    XPE_ERR_CALIB_MISSING = 130,           // 캘리브레이션 파일 없음
    XPE_ERR_CALIBRATION_EXPIRED = 131,     // 캘리브레이션 만료됨
    XPE_ERR_CALIB_LOAD_FAILED = 132,       // 파일 읽기 실패
    XPE_ERR_CALIB_INVALID = 133,           // 캘리브레이션 데이터 손상
    XPE_ERR_CALIB_MISMATCH = 134,          // 검출기 프로필 불일치
    
    // 파이프라인 (150~159)
    XPE_ERR_PIPELINE_BYPASS_INVALID = 150, // 우회 불가능한 단계
    XPE_ERR_DEPENDENCY_NOT_MET = 151,      // 선행 단계 미실행
    
    // 설정 (160~169)
    XPE_ERR_CONFIG_LOAD_FAILED = 160,      // JSON 로딩 실패
    XPE_ERR_CONFIG_INVALID = 161,          // JSON 스키마 위반
    XPE_ERR_CONFIG_KEY_NOT_FOUND = 162,    // 필수 설정 키 없음
    
    // 파라미터 검증 (170~179)
    XPE_ERR_PARAM_OUT_OF_RANGE = 170,      // 파라미터 범위 초과
    XPE_ERR_PARAM_INVALID_COMBINATION = 171, // 파라미터 조합 불가능
    
    // I/O (180~189)
    XPE_ERR_FILE_NOT_FOUND = 180,          // 파일 없음
    XPE_ERR_FILE_READ_ERROR = 181,         // 파일 읽기 오류
    XPE_ERR_FILE_WRITE_ERROR = 182,        // 파일 쓰기 오류
    
    // 알림 시스템 (190~199)
    XPE_ERR_EVENT_QUEUE_FULL = 190,          // 알림 큐 오버플로우
    XPE_ERR_EVENT_CALLBACK_FAILED = 191,     // 콜백 함수 실행 실패
    
    // 알려지지 않은 에러 (999)
    XPE_ERR_UNKNOWN = 999
} XpeError;
```

#### 에러 컨텍스트 구조

```c
struct XpeErrorDetail {
    XpeError code;                   // 에러 코드
    char message[256];               // 사람이 읽을 수 있는 메시지
    char filename[128];              // 소스 파일명
    uint32_t lineNumber;             // 코드 라인 번호
    uint64_t timestamp;              // Unix 타임스탬프 (ms)
    char context[512];               // 추가 컨텍스트 (JSON 또는 테스트)
};
```

#### 스레드-로컬 에러 상태

```c
// 각 스레드에 고유한 에러 상태 저장 (전역 변수 사용 금지)
// __thread (GCC/Clang) 또는 thread_local (C++11) 사용

XpeErrorDetail _xpe_error_context;  // 스레드-로컬

// 전역 변수가 아닌 스레드-로컬로 구현하여 멀티스레드 안전성 확보
```

#### API 함수

| 함수 | 반환값 | 설명 |
|------|--------|------|
| `xpe_get_last_error()` | `XpeError` | 마지막 에러 코드 |
| `xpe_get_last_error_detail()` | `const XpeErrorDetail*` | 전체 에러 정보 포인터 |
| `xpe_error_to_string(code)` | `const char*` | 에러 코드 → 사람이 읽을 수 있는 문자열 |
| `xpe_clear_error()` | `void` | 현재 스레드의 에러 상태 초기화 |

---

### SWU-5.4 NotificationSystem

**책임**: 비동기 이벤트 디스패칭 및 경고 관리

#### 알림 유형 (AlertType)

```c
typedef enum {
    XPE_ALERT_INFO = 0,              // 정보성 메시지
    XPE_ALERT_WARNING = 1,           // 경고 (작업 계속 진행)
    XPE_ALERT_ERROR = 2,             // 에러 (작업 중단 권장)
    XPE_ALERT_CALIBRATION_NEEDED = 3, // 캘리브레이션 필요
    XPE_ALERT_AI_UNAVAILABLE = 4     // AI 모델 로드 실패
} AlertType;
```

#### XPE Event System

**설계**:
- **큐 기반**: 원형 버퍼 (circular buffer) 256 알림 용량
- **논블로킹**: 알림 생성은 호출자 스레드에서 큐 쓰기만 수행 (즉각 반환)
- **헌신적 스레드**: 별도 알림 스레드가 큐를 폴링하고 등록된 콜백 호출
- **스레드 안전**: mutexes로 큐 접근 동기화

#### 콜백 등록

```c
typedef void (*XpeAlertCallback)(const char* alert_json, void* userdata);

// 함수 포인터: (alert message) → void
// alert_json: {"type": "WARNING", "message": "...", "timestamp": ...}
// userdata: 콜백 호출 시 전달할 사용자 데이터
```

#### API 함수

| 함수 | 입력 | 설명 |
|------|------|------|
| `xpe_event_register_callback(callback, userdata)` | 함수 포인터, 사용자 데이터 | C# 또는 다른 DLL에서 콜백 등록 |
| `xpe_event_emit_alert(type, message)` | AlertType, 문자열 | 알림 큐에 추가 (논블로킹) |
| `xpe_event_get_queue_stats()` | — | JSON: 대기 중/최대 용량 |
| `xpe_event_flush()` | — | 모든 대기 알림 처리 (블로킹) |

#### 큐 오버플로우 처리

- 큐가 256개 알림으로 가득 차면 가장 오래된 알림 제거 (FIFO 대체)
- 삭제된 알림 카운트를 `_xpe_discarded_alerts` 메트릭에 기록
- C#에 알림: `{"type": "ERROR", "message": "Alert queue overflow, dropped N alerts"}`

---

### SWU-5.5 JsonConfig

**책임**: JSON 기반 설정 로딩, 검증, 핫-리로드

#### 설정 로딩 경로

1. **기본 경로**: `./config/xpe_config.json`
2. **환경 변수 오버라이드**: `XPE_CONFIG_PATH` (없으면 기본 경로 사용)
3. **런타임 오버라이드**: `xpe_config_set_*()` 함수로 프로그래밍 방식 설정

#### 설정 스키마

```json
{
  "pipeline": {
    "flags_enabled": ["XPE_FLAG_CALIBRATED", "XPE_FLAG_GHOST_CORRECTED"],
    "stage_timeouts": {
      "preprocess": 500,
      "enhance_basic": 300,
      "enhance_advanced": 400
    },
    "mode": "clinical"
  },
  "ai": {
    "model_paths": {
      "body_part_classifier": "./models/classifier.onnx",
      "bone_suppression": "./models/bone_supp.onnx"
    },
    "enable_gpu": true
  },
  "display": {
    "presets": {
      "chest": {"window": 500, "level": 50},
      "abdomen": {"window": 400, "level": 100}
    }
  },
  "calibration": {
    "auto_reload_interval_minutes": 30,
    "temp_drift_tolerance_c": 2.0
  }
}
```

#### 유효성 검증

- **필수 키**: `pipeline`, `pipeline.flags_enabled` 없으면 로드 실패
- **형식 검증**: JSON 구문 오류 시 `XPE_ERR_CONFIG_INVALID` 반환
- **스키마 검증**: 알려진 키만 허용, 미지의 키는 무시 (미래 호환성)

#### 핫-리로드

```c
// 파일 시스템 감시 (선택사항)
// 설정 파일 변경 감지 → 백그라운드에서 재로드
// 이전 설정과 새 설정 비교 → 안전한 원자적 교체
// 실패 시 이전 설정 유지 (롤백)

xpe_config_reload();  // 수동 리로드
```

#### API 함수

| 함수 | 입력 | 반환값 |
|------|------|--------|
| `xpe_config_load(path)` | 파일 경로 | `XpeError` |
| `xpe_config_get_string(key)` | "pipeline.mode" | `const char*` (NULL if not found) |
| `xpe_config_get_float(key)` | "calibration.temp_drift_tolerance_c" | `float` |
| `xpe_config_get_int(key)` | "pipeline.stage_timeouts.preprocess" | `int` |
| `xpe_config_reload()` | — | `XpeError` |
| `xpe_config_set_string(key, value)` | 키, 값 | `XpeError` |

---

### SWU-5.6 ParameterValidator

**책임**: 파이프라인 매개변수 범위 검증

#### 검증 대상 매개변수

| 매개변수 | 범위 | 기본값 | 예시 |
|---------|------|--------|------|
| `kVp` (튜브 전압) | [40, 150] kV | 70 | 의료용 X선 |
| `mAs` (튜브 전류량) | [0.1, 500] mAs | 100 | 노출 제어 |
| `sdd` (소스-검출기 거리) | [400, 1500] mm | 1000 | 기하학적 배율 |
| `temperature` (검출기 온도) | [-10, 85] °C | 25 | 어두운 전류 보정 |
| `pixelSpacingMm` | [0.1, 0.5] mm | 0.3 | 해상도 |
| `kVpUncertainty` | [0.0, 5.0] % | 2.0 | 측정 불확도 |

#### 매개변수 검증 함수

```c
// 이미지 메타데이터의 모든 필드 검증
XpeError xpe_validate_image_params(const XpeImage* image);

// 파이프라인 설정 검증 (설정 파일 로드 후)
XpeError xpe_validate_pipeline_config(const char* config_json);
```

#### 검증 실패 처리

- **범위 초과**: `XPE_ERR_PARAM_OUT_OF_RANGE` 반환
- **조합 불가능**: `XPE_ERR_PARAM_INVALID_COMBINATION` (예: sdd < 400mm with 100mm pinhole)
- **로깅**: Alert Queue를 통해 경고 알림 발송
- **파이프라인 중단**: 필수 검증 실패 시 파이프라인 시작 거부

---

### SWU-5.7 PipelineOrchestrator

**책임**: C# WPF 측에서 `xpe_common.dll` 초기화 및 오케스트레이션 (P/Invoke 브리지)

#### C# P/Invoke 선언

```csharp
[DllImport("xpe_common.dll", CallingConvention = CallingConvention.Cdecl)]
public static extern int xpe_mempool_alloc(uint width, uint height, int format);

[DllImport("xpe_common.dll", CallingConvention = CallingConvention.Cdecl)]
public static extern int xpe_config_load([MarshalAs(UnmanagedType.LPStr)] string path);

[DllImport("xpe_common.dll", CallingConvention = CallingConvention.Cdecl)]
public static extern int xpe_get_last_error();

// Pack=8 struct 전달
[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct XpeImage
{
    public uint Width;
    public uint Height;
    // ... (C struct와 일치)
}
```

#### 초기화 시퀀스 (C# Main)

1. `xpe_config_load("./config/xpe_config.json")`
2. `xpe_mempool_init()`
3. `xpe_event_register_callback(OnAlert, nullptr)`
4. UI 로드, 파이프라인 시작

---

## Pack=8 정렬 제약 (CRITICAL)

### 문제 배경

C#의 P/Invoke는 **정확한 메모리 레이아웃**을 필요로 한다. C++와 C#이 동일한 바이트 오프셋에서 필드를 읽어야 한다.

### 규칙

**모든 `xpe_common.h` struct는 `#pragma pack(8)`을 사용해야 함.**

```c
#pragma pack(8)

struct XpeImage {
    uint32_t width;                    // 오프셋 0, 크기 4
    uint32_t height;                   // 오프셋 4, 크기 4
    // ... 이후 필드들 8바이트 경계에 정렬
};

static_assert(sizeof(XpeImage) == 240, "XpeImage must be 240 bytes");
static_assert(offsetof(XpeImage, metadata) == 36, "metadata must start at offset 36");

#pragma pack()
```

### 정렬 검증

| Struct | 크기 | 8의배수? | C# Marshal 호환? |
|--------|------|---------|-----------------|
| XpeImage | 240 바이트 | ✓ | ✓ |
| XpeImageMetadata | 192 바이트 | ✓ | ✓ |
| XpeRect | 16 바이트 | ✓ | ✓ |

### C#측 구조체 정의

```csharp
[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct XpeImage
{
    public uint Width;           // offset 0
    public uint Height;          // offset 4
    public uint BitsAllocated;   // offset 8
    public uint BitsStored;      // offset 12
    public PixelFormat Format;   // offset 16
    public IntPtr Data;          // offset 20 (8-byte pointer)
    public ulong DataSize;       // offset 28
    [MarshalAs(UnmanagedType.Struct)]
    public XpeImageMetadata Metadata;  // offset 36
    public uint XpeFlags;        // offset 228
    public uint Padding;         // offset 232
}  // 총 크기: 240
```

---

## XpeImage 구조 설계

### 메모리 레이아웃 (바이트 맵)

```
0      4      8     12     16     20     28     36                 228   232
├──────┼──────┼──────┼──────┼──────┼──────┼──────┼─────────────────┼──────┤
| width|height| bits | bits | fmt  |  data pointer | metadata      | flags|pad |
|      |      |alloc |stored|      |      |      | (192 bytes)   |      |  |
├──────┼──────┼──────┼──────┼──────┼──────┼──────┼─────────────────┼──────┤
 0      4      8     12     16     20     28     36                 228   232   240
```

### 필드 설명 및 제약

| 필드 | 오프셋 | 크기 | 제약 | 검증 |
|------|--------|------|------|------|
| `width` | 0 | 4 | [512, 4096] | `xpe_validate_image_params` |
| `height` | 4 | 4 | [512, 4096] | `xpe_validate_image_params` |
| `bitsAllocated` | 8 | 4 | {16, 32} | 엄격 |
| `bitsStored` | 12 | 4 | {14, 16, 32} ≤ bitsAllocated | 엄격 |
| `format` | 16 | 4 | {0=UINT16, 1=FLOAT32} | 열거형 |
| `data` | 20 | 8 | 비NULL (MemoryPool 또는 외부) | XPE_ERR_NULL_POINTER |
| `dataSize` | 28 | 8 | width × height × bytes_per_pixel | 계산 검증 |
| `metadata` | 36 | 192 | 내포된 struct | 부분별 검증 |
| `xpeFlags` | 228 | 4 | 비트마스크 [0x0000, 0x03FF] | OR 연산만 |
| `padding` | 232 | 8 | 0 (미사용) | 항상 0 |

---

## 메모리 풀 설계

### 슬롯 할당 전략

**목표**: 프레임 처리 중 제로카피, 버퍼 파편화 없음.

```
초기화 시:

float32 슬롯 (4개)
  [37.7 MB] [37.7 MB] [37.7 MB] [37.7 MB]
  
uint16 슬롯 (4개)
  [18.9 MB] [18.9 MB] [18.9 MB] [18.9 MB]

총 메모리: 226.4 MB (스택 할당, 시작 시 일회)
```

### 참조 카운팅

```
Frame A arrives
  ├─ xpe_mempool_alloc(3072, 3072, FLOAT32) → Slot 0 (refcount=1)
  │
  ├─ Stage 1 (Preprocess): use Slot 0 (refcount=1)
  │
  ├─ Stage 2 (Enhance Basic): read from Slot 0 (refcount=1)
  │
  └─ Stage 3 (Display): output written to new Slot 1
       └─ xpe_mempool_free(Slot 0) → refcount=0 → Slot 0 available for next frame
```

### 에러 처리

- **XPE_ERR_POOL_EXHAUSTED**: 모든 슬롯이 사용 중 (8프레임 동시 처리?)
  - 해결책: 파이프라인 흐름 제어 (Stage N이 완료될 때까지 Stage N+1 시작 금지)
- **XPE_ERR_INVALID_SIZE**: 요청 해상도 ≠ 정의된 풀 슬롯
  - 정책: 고정 크기만 허용 (3072×3072). 다른 크기는 에러.

---

## 에러 처리 전략

### 에러 전파 규칙

1. **즉시 반환**: C 함수에서 에러 발생 → `XpeError` enum 반환
2. **스레드-로컬 저장**: 에러 컨텍스트 (메시지, 파일, 라인) `_xpe_error_context`에 저장
3. **C#에서 폴링**: `xpe_get_last_error()` → `xpe_get_last_error_detail()` 호출
4. **알림 발송**: 심각한 에러는 Alert Queue를 통해 C#에 비동기 알림

### 에러 분류

| 분류 | 예시 | 처리 |
|------|------|------|
| **입력 검증** | 범위 초과, NULL 포인터 | 즉시 거부, 경고 |
| **초기화** | 캘리브레이션 미로드 | 파이프라인 시작 차단 |
| **리소스** | 메모리 풀 고갈 | 흐름 제어, Event/Alert 알림 |
| **설정** | JSON 스키마 오류 | 기본값 사용 또는 거부 |

---

## 알림 시스템 (XPE Event System)

### 큐 메커니즘

```
┌──────────────────────────────────┐
│    Alert Emission (Fast Path)    │  ← 모든 XPE DLL에서 호출 (논블로킹)
│  xpe_event_emit_alert()            │
│  (Circular Buffer에 추가, 반환)  │
└──────────────────┬───────────────┘
                   ↓
        ┌─────────────────────┐
        │  Alert Queue (256)  │  원형 버퍼
        └─────────────────────┘
                   ↑
         ┌─────────┴──────────┐
         │                    │
  ┌──────▼─────┐       ┌──────▼─────┐
  │ Alert Thread│      │  Callback  │
  │  (Polling)  │      │ Invocation │
  └──────┬─────┘      └──────▲─────┘
         │                   │
         └───────────────────┘
                  (스레드 안전, mutex)
```

### 큐 오버플로우

- 용량 256 초과 시 가장 오래된 항목 제거
- 제거 카운트를 메트릭에 기록
- C#에 경고 알림 전송

---

## 설정 관리 (JsonConfig)

### 파일 위치 및 폴백

```
1. 환경 변수 XPE_CONFIG_PATH 확인
   ↓ (설정됨)
   로드: $XPE_CONFIG_PATH
   
   ↓ (미설정)

2. 기본 경로 시도
   로드: ./config/xpe_config.json
   
   ↓ (파일 없음)

3. 기본값 사용 (하드코딩)
   경고 알림 발송
```

### 핫-리로드 (선택)

```c
// 파일 시스템 watcher (Windows: ReadDirectoryChangesW, Unix: inotify)
// 파일 변경 감지 → 백그라운드 스레드에서 재로드
// 검증 실패 → 이전 설정 유지 (자동 롤백)

xpe_config_reload();  // 수동 트리거 (C#에서 호출 가능)
```

---

## 매개변수 검증

### 검증 타이밍

```
C# User Action (Load Image)
  ↓
xpe_process_frame(image)
  ├─ xpe_validate_image_params(image) ← 필수 검증
  │  └─ xpe_ERR_PARAM_OUT_OF_RANGE → 즉시 거부
  │
  ├─ xpe_validate_pipeline_config() ← 파이프라인 진행 전
  │  └─ xpe_ERR_CONFIG_INVALID → 설정 수정 요청
  │
  └─ [파이프라인 시작]
```

### 검증 범위

```
kVp:        [40, 150] kV
mAs:        [0.1, 500] mAs
SDD:        [400, 1500] mm
Temp:       [-10, 85] °C
Pixel Sz:   [0.1, 0.5] mm
Bits Alloc: {16, 32}
Bits Stored: {14, 16, 32} ≤ Bits Alloc
```

---

## P/Invoke 브리지 (C# 상호작용)

### 호출 규약

```c
// 모든 함수: extern "C" + CallingConvention::Cdecl (C 호출 규약)

#ifdef _WIN32
    #define XPE_API extern "C" __declspec(dllexport)
#else
    #define XPE_API extern "C"
#endif

XPE_API XpeError xpe_mempool_alloc(uint32_t width, uint32_t height, PixelFormat format);
```

### C# Marshal 설정

```csharp
[DllImport("xpe_common.dll", CallingConvention = CallingConvention.Cdecl)]
public static extern int xpe_get_last_error();

[DllImport("xpe_common.dll", CallingConvention = CallingConvention.Cdecl)]
[return: MarshalAs(UnmanagedType.LPStr)]
public static extern string xpe_error_to_string(int code);
```

### 콜백 전달 (C# → C++)

```csharp
// C# 측 콜백 정의
public delegate void XpeAlertCallback([MarshalAs(UnmanagedType.LPStr)] string json, IntPtr userdata);

// C++ 함수
[DllImport("xpe_common.dll", CallingConvention = CallingConvention.Cdecl)]
public static extern int xpe_event_register_callback(XpeAlertCallback callback, IntPtr userdata);

// 사용
var callback = new XpeAlertCallback(OnAlert);
xpe_event_register_callback(callback, IntPtr.Zero);
```

---

## 성능 및 메모리 예산

### 시간 할당

| 작업 | 예산 (ms) | 참고 |
|------|----------|------|
| 메모리 풀 초기화 | 50 | 시작 시 일회 (226.4 MB 할당) |
| 설정 로드 | 10 | JSON 파싱 |
| 매개변수 검증 | 5 | 범위 검사 |
| 에러 조회 | <1 | 스레드-로컬 조회 |
| Event/Alert 알림 발송 | <1 | 큐 쓰기 (논블로킹) |
| Event System 콜백 호출 | ~1 | 알림 스레드 |

### 메모리 할당

| 구성요소 | 크기 | 할당 | 해제 |
|---------|------|------|------|
| 메모리 풀 (float32 4개) | 150.8 MB | 초기화 | 프로세스 종료 |
| 메모리 풀 (uint16 4개) | 75.6 MB | 초기화 | 프로세스 종료 |
| 설정 JSON | ~50 KB | 로드 | 언로드 (핫-리로드) |
| 에러 컨텍스트 (TLS) | ~1 KB | 스레드 생성 | 스레드 종료 |
| **총 피크** | **~226.4 MB** | 초기화 | 프로세스 종료 |

---

## 제약 조건 및 의존성

### 구현 제약

1. **Pack=8 정렬 CRITICAL**: 모든 struct에 `#pragma pack(8)` 필수
2. **C 호출 규약 (Cdecl)**: P/Invoke 호환성
3. **스레드-로컬 에러**: 멀티스레드 안전성
4. **제로카피 설계**: 메모리 풀 슬롯 포인터 전달
5. **고정 슬롯 크기**: 동적 할당 금지

### 외부 의존성

- **없음**: `xpe_common.dll`은 다른 XPE DLL에 의존하지 않음
- **표준 라이브러리**: C 표준 라이브러리만 사용 (stdlib, stdio, string, time)
- **OS API**: Windows/Linux 타이머, 파일 I/O, 스레드 관리

### 반대 의존성 (Dependents)

- **xpe_preprocess.dll**: MemoryPool, XpeImage, ErrorHandler 사용
- **xpe_enhance_basic.dll**: 모든 SWU 사용
- **xpe_enhance_advanced.dll**: 모든 SWU 사용
- **xpe_ai.dll**: MemoryPool, XpeImage, Event System 사용
- **xpe_display.dll**: XpeImage, ErrorHandler 사용
- **xpe_dicom.dll**: XpeImage, JsonConfig 사용
- **ImageProcTest.exe (C#)**: P/Invoke를 통해 모든 SWU 사용

---

**xpe_common.dll PRD v1.0.0 끝**
