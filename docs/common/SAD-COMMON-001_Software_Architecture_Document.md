# SAD-COMMON-001: xpe_common.dll 소프트웨어 아키텍처 문서

**Document ID**: SAD-COMMON-001  
**Version**: 1.0.0  
**IEC 62304 Clause**: 5.3 — Software Architecture Design  
**Safety Classification**: Class B  
**Date**: 2026-04-14  
**Normative Reference**: SRS-COMMON-001

---

## 1. 목적 및 범위

`xpe_common.dll`의 소프트웨어 아키텍처를 정의한다. SRS의 기능/안전/성능 요구사항을 7개의 소프트웨어 단위(SWU)로 분해하고, 각 SWU의 설계, 인터페이스, 데이터 흐름, 위험 제어 메커니즘을 명시한다.

---

## 2. 아키텍처 개요

### 2.1 계층 구조 (Layering)

```
┌──────────────────────────────────────────────────────┐
│  Layer 2: ImageProcTest.exe (C# WPF 오케스트레이터) │
│           ↑ P/Invoke (Pack=8 structs)                │
├──────────────────────────────────────────────────────┤
│  Layer 1: xpe_preprocess.dll (캘리브레이션 전처리)  │
│           xpe_enhance_basic.dll                      │
│           xpe_enhance_advanced.dll                   │
│           xpe_ai.dll                                 │
│           xpe_display.dll, xpe_dicom.dll             │
│           ↑ 링크 의존성 (정적 링킹)                  │
├──────────────────────────────────────────────────────┤
│  Layer 0: xpe_common.dll (공유 기반 인프라)          │
│                                                      │
│  ┌────────────────────────────────────────────────┐ │
│  │  SWU-5.1: MemoryPool                          │ │
│  │  SWU-5.2: TypeDefinitions (Pack=8)            │ │
│  │  SWU-5.3: ErrorHandler (스레드-로컬)          │ │
│  │  SWU-5.4: NotificationSystem (XPE Event System)            │ │
│  │  SWU-5.5: JsonConfig (핫-리로드)              │ │
│  │  SWU-5.6: ParameterValidator                  │ │
│  │  SWU-5.7: PipelineOrchestrator (C# 브리지)   │ │
│  └────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────┘
```

### 2.2 안티-스파게티 원칙

- **단방향 의존성**: Layer 1 → Layer 0만 가능
- **횡단 의존성 금지**: xpe_preprocess → xpe_enhance_basic 직접 호출 불가
- **공유 타입**: 모든 공유 데이터는 `xpe_common.h`의 struct 사용
- **에러 전파**: 표준화된 `XpeError` enum

---

## 3. 소프트웨어 단위 설계 (SWU-5.*)

### 3.1 SWU-5.1: MemoryPool (메모리 풀 할당기)

#### 책임
- 제로카피 이미지 전송을 위한 메모리 슬롯 할당
- 참조 카운팅을 통한 슬롯 해제
- 스레드 안전성 보장

#### 설계 패턴: **Slab Allocator**

```
초기화 시:
  float32_slots = [Slot 0 (refcount=0), Slot 1 (refcount=0), ...]
  uint16_slots = [Slot 0 (refcount=0), Slot 1 (refcount=0), ...]
  
할당 요청:
  xpe_mempool_alloc(3072, 3072, FLOAT32)
    → Slot 0 찾기 (refcount=0)
    → Slot 0 (refcount=1)
    → return Slot 0.ptr
    
사용:
  Stage 1: read Slot 0.ptr (refcount=1)
  Stage 2: read Slot 0.ptr (refcount=1)
  
해제:
  xpe_mempool_free(Slot 0.ptr)
    → Slot 0 (refcount=0)
    → Slot 0 다시 사용 가능
```

#### 인터페이스

```c
typedef struct {
    void* ptr;           // 할당된 메모리 주소
    uint32_t width;
    uint32_t height;
    PixelFormat format;
    uint32_t refcount;   // 참조 카운트
} XpeMempoolSlot;

// API
XpeError xpe_mempool_alloc(uint32_t width, uint32_t height, PixelFormat format);
XpeError xpe_mempool_free(void* ptr);
const char* xpe_mempool_get_stats(void);  // JSON
```

#### 내부 상태

```c
static XpeMempoolSlot g_float32_slots[4];   // 각 37.7 MB
static XpeMempoolSlot g_uint16_slots[4];    // 각 18.9 MB
static pthread_mutex_t g_mempool_mutex;     // 스레드 안전성
static uint64_t g_peak_memory = 0;          // 메트릭
```

#### 위험 제어

- **이중 해제**: 포인터 유효성 검증 (할당된 슬롯 리스트 확인)
- **메모리 누수**: 프로세스 종료 시 finalizer에서 강제 해제
- **경합 조건**: mutex로 할당/해제 동기화

---

### 3.2 SWU-5.2: TypeDefinitions (공유 타입 정의)

#### 책임
- Pack=8 정렬된 struct 정의
- P/Invoke 호환성 보장
- 모든 Layer 1 DLL의 공유 타입

#### Pack=8 정렬 설계

```c
#pragma pack(8)

struct XpeImage {
    uint32_t width;                      // offset 0
    uint32_t height;                     // offset 4
    uint32_t bitsAllocated;              // offset 8
    uint32_t bitsStored;                 // offset 12
    PixelFormat format;                  // offset 16
    void* data;                          // offset 20 (8-byte pointer)
    size_t dataSize;                     // offset 28
    XpeImageMetadata metadata;           // offset 36 (192 bytes)
    uint32_t xpeFlags;                   // offset 228
    uint32_t padding;                    // offset 232 (Pack=8 alignment)
};  // 총 크기: 240 (8의 배수)

struct XpeImageMetadata {
    char bodyPart[64];
    float kVp;
    float mAs;
    float sdd;
    float pixelSpacingMm;
    uint64_t acquisitionTime;
    char detectorId[32];
    float temperature;
    char reserved[96];
};  // 총 크기: 192

#pragma pack()

// 정렬 검증
static_assert(sizeof(XpeImage) == 240, "XpeImage must be 240 bytes");
static_assert(offsetof(XpeImage, metadata) == 36, "metadata offset");
static_assert(sizeof(XpeImageMetadata) == 192, "Metadata size");
```

#### 바이트 맵 시각화

```
XpeImage 레이아웃 (총 240 바이트):

0    4    8   12   16   20   28            36                 228  232
|width|hgt |alloc|stor|fmt |data_ptr|size|metadata          |flag|pad |
|4    |4   |4   |4   |4  |8   |8   |192              |4   |8  |
|─────────────────────────────────────────────────────────────────────|
                                                                  240
```

#### 정렬 검증 (컴파일 타임)

```c
// xpe_common.h 에 정의
#define STATIC_ASSERT(cond, msg) \
    typedef char static_assertion[(cond) ? 1 : -1]

STATIC_ASSERT(sizeof(XpeImage) == 240, "XpeImage size");
STATIC_ASSERT(sizeof(XpeImageMetadata) == 192, "Metadata size");
STATIC_ASSERT(offsetof(XpeImage, xpeFlags) == 228, "xpeFlags offset");
```

---

### 3.3 SWU-5.3: ErrorHandler (에러 처리)

#### 책임
- 구조화된 에러 전파
- 스레드-로컬 에러 컨텍스트
- JSON 진단 정보 생성

#### 설계: **Thread-Local Error Context**

```c
// 스레드-로컬 저장소 (GCC __thread, C++11 thread_local)
__thread XpeErrorDetail _xpe_error_context = {
    .code = XPE_OK,
    .message = "",
    .filename = "",
    .lineNumber = 0,
    .timestamp = 0,
    .context = ""
};

// 매크로로 에러 기록 자동화
#define XPE_SET_ERROR(code, msg) do { \
    _xpe_error_context.code = (code); \
    strncpy(_xpe_error_context.message, (msg), 255); \
    _xpe_error_context.filename = __FILE__; \
    _xpe_error_context.lineNumber = __LINE__; \
    _xpe_error_context.timestamp = xpe_get_timestamp_ms(); \
} while(0)
```

#### 인터페이스

```c
// 단순 조회
XpeError xpe_get_last_error(void);

// 상세 정보
const XpeErrorDetail* xpe_get_last_error_detail(void);

// 문자열 변환
const char* xpe_error_to_string(XpeError code);

// 상태 초기화
void xpe_clear_error(void);
```

#### 에러 코드 공간 (50+ 정의)

| 범주 | 범위 | 예시 |
|------|------|------|
| 입력 검증 | 100~109 | INVALID_INPUT, NULL_POINTER |
| 메모리 | 110~119 | POOL_EXHAUSTED, ALLOCATION_FAILED |
| 초기화 | 120~129 | NOT_INITIALIZED, ALREADY_INITIALIZED |
| 캘리브레이션 | 130~149 | CALIB_MISSING, CALIBRATION_EXPIRED |
| 파이프라인 | 150~159 | BYPASS_INVALID, DEPENDENCY_NOT_MET |
| 설정 | 160~169 | CONFIG_LOAD_FAILED, CONFIG_INVALID |
| 매개변수 | 170~179 | PARAM_OUT_OF_RANGE |
| I/O | 180~189 | FILE_NOT_FOUND, FILE_READ_ERROR |
| Event System | 190~199 | EVENT_QUEUE_FULL, EVENT_CALLBACK_FAILED |

---

### 3.4 SWU-5.4: NotificationSystem (비동기 알림)

#### 책임
- 비동기 이벤트 디스패칭 (XPE Event System)
- 스레드 안전한 알림 큐
- 콜백 기반 알림 전달

#### 설계: **Producer-Consumer Queue**

```
Producer (모든 DLL)           Consumer (알림 스레드)
  │                            │
  ├─ xpe_event_emit_alert()      ├─ poll queue
  │  (큐에 추가, 논블로킹)      ├─ deserialize alert
  │  │                         ├─ call registered callbacks
  │  v                         │
  ┌──────────────────┐        └─ sleep 100ms, 반복
  │ Alert Queue      │
  │ (circular buf)   │
  │ [0][1][2]...[255]│
  └──────────────────┘
      ↑           ↓
  (write ptr) (read ptr)
```

#### 내부 구조

```c
typedef struct {
    AlertType type;
    uint64_t timestamp;
    char message[512];
} XpeAlert;

typedef struct {
    XpeAlert alerts[256];
    uint32_t write_index;     // 다음 쓰기 위치
    uint32_t read_index;      // 다음 읽기 위치
    uint32_t count;           // 대기 중인 알림
    uint64_t discarded_count; // 제거된 알림
    pthread_mutex_t lock;
    pthread_cond_t cond;      // 조건 변수 (깨우기)
} XpeAlertQueue;

// 콜백
typedef void (*XpeAlertCallback)(const char* alert_json, void* userdata);

static XpeAlertCallback g_callbacks[8];
static void* g_userdata[8];
static uint32_t g_callback_count = 0;
```

#### 알림 발송 흐름

```c
xpe_event_emit_alert(XPE_ALERT_WARNING, "Temperature drift detected")
  ├─ lock queue
  ├─ if (count < 256)
  │   ├─ alerts[write_index] = {type, timestamp, msg}
  │   ├─ write_index++
  │   ├─ count++
  │   └─ pthread_cond_signal() // 알림 스레드 깨우기
  ├─ else
  │   ├─ read_index++ // 가장 오래된 항목 제거
  │   ├─ alerts[write_index] = {type, timestamp, msg}
  │   ├─ count remains 256
  │   └─ discarded_count++
  ├─ unlock queue
  └─ return XPE_OK or XPE_ERR_EVENT_QUEUE_FULL
```

#### 콜백 호출 (알림 스레드)

```c
void* xpe_event_thread_main(void* arg) {
    while (!should_stop) {
        lock queue
        if (count > 0) {
            alert = alerts[read_index++]
            count--
        } else {
            pthread_cond_wait() // 100ms 타임아웃
        }
        unlock queue
        
        if (has_alert) {
            json = serialize_alert(alert)
            for (i=0; i<callback_count; i++) {
                callbacks[i](json, userdata[i])
            }
            free(json)
        }
    }
}
```

---

### 3.5 SWU-5.5: JsonConfig (설정 관리)

#### 책임
- JSON 파일 로드/파싱
- 설정 검증 및 기본값 처리
- 런타임 설정 변경 및 핫-리로드

#### 설계: **Configuration Store**

```c
typedef struct {
    // pipeline 섹션
    uint32_t enabled_flags;      // bitmask
    uint32_t stage_timeouts[10]; // 단계별 타임아웃
    char mode[32];               // "clinical", "diagnostic", "fluoro"
    
    // ai 섹션
    char body_part_model_path[256];
    char bone_suppression_model_path[256];
    bool enable_gpu;
    
    // display 섹션
    struct {
        int window;
        int level;
    } presets[8];
    
    // calibration 섹션
    uint32_t auto_reload_interval_minutes;
    float temp_drift_tolerance_c;
    
    // 메타데이터
    uint64_t load_time;
    char source_file[256];
} XpeConfig;

static XpeConfig g_config = {};  // 전역 설정
static pthread_rwlock_t g_config_lock;  // 읽기/쓰기 락
```

#### 로드 시퀀스

```c
xpe_config_load(path)
  ├─ check env var XPE_CONFIG_PATH
  ├─ if (not set) use default "./config/xpe_config.json"
  ├─ FILE* fp = fopen(path)
  ├─ if (fp == NULL) return XPE_ERR_FILE_NOT_FOUND
  ├─ cJSON* root = cJSON_ParseFile(fp)
  ├─ if (parse error) return XPE_ERR_CONFIG_INVALID
  ├─ validate_schema(root) // 필수 키 확인
  ├─ write_lock(g_config_lock)
  ├─ memcpy(&g_config, &parsed_config, sizeof(XpeConfig))
  ├─ write_unlock(g_config_lock)
  ├─ cJSON_Delete(root)
  ├─ fclose(fp)
  └─ return XPE_OK
```

#### 핫-리로드 (선택)

```c
xpe_config_reload()
  ├─ 임시 설정 T = {}
  ├─ xpe_config_load_into(&T)  // 새로 로드
  ├─ if (실패) return error (g_config 미변경)
  ├─ write_lock(g_config_lock)
  ├─ memcpy(&g_config, &T, sizeof(XpeConfig))  // 원자적 교체
  ├─ write_unlock(g_config_lock)
  └─ return XPE_OK
```

---

### 3.6 SWU-5.6: ParameterValidator (매개변수 검증)

#### 책임
- 이미지 메타데이터 범위 검증
- 파이프라인 설정 유효성 검증
- 매개변수 조합 검증

#### 검증 규칙 테이블

```c
typedef struct {
    const char* name;
    float min;
    float max;
    float default_val;
} XpeParameterDef;

static const XpeParameterDef g_param_defs[] = {
    {"kVp", 40.0f, 150.0f, 70.0f},
    {"mAs", 0.1f, 500.0f, 100.0f},
    {"sdd", 400.0f, 1500.0f, 1000.0f},
    {"temperature", -10.0f, 85.0f, 25.0f},
    {"pixelSpacingMm", 0.1f, 0.5f, 0.3f},
    {NULL, 0, 0, 0}
};
```

#### 검증 함수

```c
XpeError xpe_validate_image_params(const XpeImage* image) {
    if (image == NULL) return XPE_ERR_NULL_POINTER;
    
    // 해상도
    if (image->width < 512 || image->width > 4096)
        return XPE_ERR_INVALID_DIMENSIONS;
    if (image->height < 512 || image->height > 4096)
        return XPE_ERR_INVALID_DIMENSIONS;
    
    // 비트 깊이
    if (image->bitsAllocated != 16 && image->bitsAllocated != 32)
        return XPE_ERR_INVALID_INPUT;
    if (image->bitsStored > image->bitsAllocated)
        return XPE_ERR_INVALID_INPUT;
    
    // 메타데이터
    if (image->metadata.kVp < 40.0f || image->metadata.kVp > 150.0f)
        return XPE_ERR_PARAM_OUT_OF_RANGE;
    if (image->metadata.mAs < 0.1f || image->metadata.mAs > 500.0f)
        return XPE_ERR_PARAM_OUT_OF_RANGE;
    // ... 추가 범위 검사
    
    return XPE_OK;
}
```

---

### 3.7 SWU-5.7: PipelineOrchestrator (C# 브리지)

#### 책임
- C# WPF에서 `xpe_common.dll` 초기화
- P/Invoke를 통한 DLL 함수 호출
- 메시지 마샬링 (Pack=8 struct)

#### C# P/Invoke 선언 (예시)

```csharp
using System;
using System.Runtime.InteropServices;

public static class XpeCommon {
    private const string DLL = "xpe_common.dll";
    
    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern int xpe_mempool_alloc(uint width, uint height, int format);
    
    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern int xpe_mempool_free(IntPtr ptr);
    
    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern int xpe_config_load([MarshalAs(UnmanagedType.LPStr)] string path);
    
    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern int xpe_get_last_error();
    
    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public delegate void AlertCallback([MarshalAs(UnmanagedType.LPStr)] string json, IntPtr userdata);
    
    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern int xpe_event_register_callback(AlertCallback cb, IntPtr userdata);
}

// Pack=8 struct 정의
[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct XpeImage {
    public uint Width;
    public uint Height;
    public uint BitsAllocated;
    public uint BitsStored;
    public int Format;  // enum
    public IntPtr Data;
    public ulong DataSize;
    [MarshalAs(UnmanagedType.Struct)]
    public XpeImageMetadata Metadata;
    public uint XpeFlags;
    public uint Padding;
}
```

#### 초기화 시퀀스 (C# Main)

```csharp
// 1. 설정 로드
int result = XpeCommon.xpe_config_load("./config/xpe_config.json");
if (result != 0) {
    MessageBox.Show("설정 로드 실패");
    Environment.Exit(1);
}

// 2. 메모리 풀 초기화
result = XpeCommon.xpe_mempool_alloc(3072, 3072, 1);  // FLOAT32
if (result < 0) {
    MessageBox.Show("메모리 풀 초기화 실패");
    Environment.Exit(1);
}

// 3. 콜백 등록
var callback = new XpeCommon.AlertCallback(OnAlert);
XpeCommon.xpe_event_register_callback(callback, IntPtr.Zero);

// 4. UI 로드
Application.Current.MainWindow = new MainWindow();
Application.Current.MainWindow.Show();
```

---

## 4. 인터페이스 정의

### 4.1 메모리 풀 인터페이스

```c
// 초기화/정리
XpeError xpe_mempool_init(void);
void xpe_mempool_finalize(void);

// 할당/해제
XpeError xpe_mempool_alloc(uint32_t width, uint32_t height, PixelFormat format, void** out_ptr);
XpeError xpe_mempool_free(void* ptr);

// 통계
const char* xpe_mempool_get_stats(void);  // JSON
```

### 4.2 설정 관리 인터페이스

```c
XpeError xpe_config_load(const char* path);
XpeError xpe_config_reload(void);
const char* xpe_config_get_string(const char* key);
float xpe_config_get_float(const char* key);
int xpe_config_get_int(const char* key);
XpeError xpe_config_set_string(const char* key, const char* value);
XpeError xpe_config_set_float(const char* key, float value);
```

### 4.3 에러 처리 인터페이스

```c
XpeError xpe_get_last_error(void);
const XpeErrorDetail* xpe_get_last_error_detail(void);
const char* xpe_error_to_string(XpeError code);
void xpe_clear_error(void);
```

### 4.4 알림 시스템 인터페이스

```c
typedef void (*XpeAlertCallback)(const char* alert_json, void* userdata);

XpeError xpe_event_init(void);
XpeError xpe_event_register_callback(XpeAlertCallback callback, void* userdata);
XpeError xpe_event_emit_alert(AlertType type, const char* message);
const char* xpe_event_get_queue_stats(void);  // JSON
XpeError xpe_event_flush(void);
void xpe_event_finalize(void);
```

### 4.5 매개변수 검증 인터페이스

```c
XpeError xpe_validate_image_params(const XpeImage* image);
XpeError xpe_validate_pipeline_config(const char* config_json);
```

---

## 5. 데이터 흐름

### 5.1 이미지 처리 흐름

```
C# (ImageProcTest.exe)
  │
  ├─ Load raw frame (uint16 원본)
  │   ├─ 검증: xpe_validate_image_params()
  │   ├─ 할당: xpe_mempool_alloc(3072, 3072, UINT16)
  │   └─ 복사: image.data = mempool_slot_ptr
  │
  └─ 처리 (각 단계는 동일 포인터 사용)
      ├─ xpe_preprocess.dll (uint16 → float32)
      │   └─ 결과: float32 메모리 풀 슬롯
      │
      ├─ xpe_enhance_basic.dll (float32 → float32)
      │   └─ 결과: 동일 슬롯 (인플레이스)
      │
      └─ xpe_display.dll (float32 → uint8/uint16 화면)
          └─ 결과: 디스플레이 버퍼 또는 새 메모리풀 슬롯
                   └─ 원본 슬롯 해제: xpe_mempool_free()
```

### 5.2 설정 로드 흐름

```
xpe_config_load("./config/xpe_config.json")
  ├─ 파일 열기
  ├─ JSON 파싱 (cJSON)
  ├─ 스키마 검증 (필수 키 확인)
  ├─ 설정 값 추출
  ├─ write_lock(g_config_lock)
  ├─ 전역 g_config 업데이트 (원자적)
  ├─ write_unlock(g_config_lock)
  └─ 반환: XPE_OK
```

### 5.3 에러 처리 흐름

```
xpe_preprocess_stage1()
  ├─ xpe_mempool_alloc() 호출
  ├─ 실패: XPE_ERR_POOL_EXHAUSTED 반환
  ├─ 내부: _xpe_error_context.code = XPE_ERR_POOL_EXHAUSTED
  ├─ 내부: _xpe_error_context.message = "All pool slots exhausted"
  └─ 내부: _xpe_error_context.filename = "xpe_preprocess.c"
           _xpe_error_context.lineNumber = 123

C# 코드:
  int err = xpe_preprocess_stage1();
  if (err < 0) {
      const XpeErrorDetail* detail = xpe_get_last_error_detail();
      MessageBox.Show($"에러: {detail.message} (파일: {detail.filename}:{detail.lineNumber})");
  }
```

### 5.4 알림 발송 흐름

```
xpe_preprocess_stage1() (임의의 DLL)
  ├─ 온도 드리프트 감지
  ├─ xpe_event_emit_alert(XPE_ALERT_WARNING, "Temperature drift 2.5C detected")
  ├─ 내부: lock queue
  ├─ 내부: alerts[write_index++] = {...}
  ├─ 내부: pthread_cond_signal() ← 알림 스레드 깨우기
  ├─ 내부: unlock queue
  └─ 반환: XPE_OK (즉시, 논블로킹)

별도 알림 스레드:
  while (!should_stop) {
      lock queue
      if (count > 0) {
          alert = alerts[read_index++]
          json = serialize_alert(alert)
      }
      unlock queue
      
      if (json) {
          for (callback in callbacks) {
              callback(json, userdata)  ← C# 콜백 호출
          }
      }
  }

C# 콜백:
  void OnAlert(string json, IntPtr userdata) {
      JObject alert = JObject.Parse(json);
      MessageBox.Show($"경고: {alert["message"]}");
  }
```

---

## 6. 위험 제어

### 6.1 MemoryPool 위험

| 위험 | 원인 | 제어 |
|------|------|------|
| 이중 해제 | 포인터 중복 전달 | 할당된 슬롯 리스트 검증 |
| 메모리 누수 | 해제 호출 빠짐 | 파이프라인 흐름 제어 + finalizer |
| 경합 조건 | 동시 할당/해제 | mutex 보호 |
| 슬롯 고갈 | 8프레임 동시 처리 | XPE_ERR_POOL_EXHAUSTED 에러 |

### 6.2 Pack=8 정렬 위험

| 위험 | 원인 | 제어 |
|------|------|------|
| 메모리 정렬 오류 | pragma pack 미적용 | static_assert 컴파일 타임 검증 |
| P/Invoke 호환성 | C#와 오프셋 불일치 | StructLayout(Pack=8) 동일 정의 |

### 6.3 설정 핫-리로드 위험

| 위험 | 원인 | 제어 |
|------|------|------|
| 경합 조건 | 읽음/쓰기 동시 | rwlock (pthread_rwlock) |
| 부분 업데이트 | 검증 중 설정 변경 | 임시 구조체 검증 후 원자적 복사 |
| 손상된 설정 | JSON 파싱 오류 | 스키마 검증 + 이전 설정 보유 |

### 6.4 Event/Alert 알림 위험

| 위험 | 원인 | 제어 |
|------|------|------|
| 큐 오버플로우 | 생산 > 소비 | FIFO 대체 (가장 오래된 제거) |
| 메모리 누수 | 콜백 미호출 | 별도 스레드에서 강제 폴링 |
| 콜백 크래시 | 콜백 오류 | try-catch로 개별 콜백 보호 |

---

## 7. 성능 특성

### 7.1 시간 복잡도

| 작업 | 복잡도 | 예상 시간 |
|------|--------|---------|
| 메모리 할당 | O(1) | < 1 ms |
| 메모리 해제 | O(1) | < 1 ms |
| 설정 로드 | O(n) (JSON 크기) | < 20 ms |
| 매개변수 검증 | O(1) | < 5 ms |
| Event 발송 | O(1) | < 0.5 ms |

### 7.2 공간 복잡도

| 구성요소 | 크기 |
|---------|------|
| 메모리 풀 | 226.4 MB |
| 설정 메모리 | < 1 MB |
| 에러 컨텍스트 (TLS) | ~1 KB / 스레드 |
| Event Queue | ~2 KB (고정) |

---

**SAD-COMMON-001 v1.0.0 끝**
