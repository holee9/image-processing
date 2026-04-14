# XPE API 사양 — 완전한 내보낸 C ABI 참조

**Document ID**: XPE-API-SPEC-001  
**Version**: 1.2.0  
**Date**: 2026-04-14  
**Source Documents**: XPE-SRS-001, XPE-SAD-001, GSVG-SDD-001, xpe_types.h, xpe_error.h, xpe_memory.h, xpe_common_api.h, SPEC-XPE-MASTER v2.0.0  
**Changelog**: v1.1.0 -> v1.2.0: (1) §5.16-5.18에 AED 함수 3개 추가 (xpe_aed_configure, xpe_aed_poll_event, xpe_aed_get_status). (2) xpe_calc_exposure_index를 SPEC-XPE-MASTER v2.0.0 §3.9 EI-0 해결안에 따라 §8 (enhance_advanced)에서 §7.7 (enhance_basic)로 이동. (3) §4 요약 카운트 갱신: xpe_common=18, enhance_basic=7, enhance_advanced=3, total=82.
**Reference**: JSON 설정 스키마, 캘리브레이션 파일 형식, 신체 부위 룩업 테이블은 xpe-implementation-reference.md 참조.

---

## 1. ABI 규칙

| 규칙 | 값 |
|------|-------|
| 호출 규약 | `__cdecl` (Windows C 기본) |
| Struct 패킹 | `#pragma pack(push, 8)` — 8바이트 정렬 |
| 타입 공간 | 순수 C 타입만 (`stdint.h`, `stddef.h`) — STL 없음, RTTI 없음 |
| 링크 | 모든 내보낸 기호에 `extern "C"` |
| 내보내기 매크로 | `__declspec(dllexport)` (XPE_DLL_EXPORT 정의됨) / `__declspec(dllimport)` (소비자) |
| 반환 규약 | 모든 오류 가능 함수는 `XpeErrorCode` (`int32_t`); 안전한 함수는 `void` 또는 `const char*` |
| 오류 세부 정보 | 확장 진단이 필요한 경우 out-parameter `char* errorMsg` 버퍼 |
| 메모리 소유권 | Caller가 `xpe_alloc_image()`를 통해 할당, `xpe_free_image()`를 통해 해제 |
| 스레드 안전성 | 모든 처리 함수는 독립적인 caller 제공 버퍼를 사용하는 재진입 가능 |
| 설정 형식 | `const char*` UTF-8 JSON 문자열; `NULL`은 "기본값 사용"으로 인정 |

### P/Invoke 정렬 주의사항

- `XpeImageBuffer`: x64에서 pack=8일 때 크기 = 40바이트 (20바이트 스칼라 필드 + 4바이트 패딩 + 8 + 8). C#에서 `void* data`를 `IntPtr`로 매핑.
- `XpeImageMetadata`: 크기 = 96바이트 (64 + 4+4+4+4+8+4 + 4 패딩). 모든 필드 블릿 가능.
- `XpePixelFormat`: `int` (`[MarshalAs(UnmanagedType.I4)]`)로 마샬링.
- `XpeAlertSeverity`: `int`로 마샬링.
- 함수 포인터/콜백: 이 ABI에서 사용하지 않음 — 모든 비동기 결과는 경고 폴링 사용.
- `const char*`로 반환된 문자열은 DLL이 소유 (정적 저장소); 해제하지 말 것.
- 출력 `char*` 버퍼 (예: `xpe_get_pending_alert`의 `msg`)는 caller 할당 필수.

---

## 2. 공통 타입

`modules/common/include/xpe/common/xpe_types.h`에 정의:

```c
#pragma pack(push, 8)

typedef enum XpePixelFormat {
    XPE_PIXEL_UINT16  = 0,   /* 16비트 부호 없는 정수 픽셀 */
    XPE_PIXEL_FLOAT32 = 1    /* 32비트 IEEE 754 부동 소수점 픽셀 */
} XpePixelFormat;

typedef struct XpeImageBuffer {
    uint32_t       width;         /* 이미지 너비(픽셀) */
    uint32_t       height;        /* 이미지 높이(픽셀) */
    uint32_t       bitsAllocated; /* 저장소 비트 깊이 (예: 16) */
    uint32_t       bitsStored;    /* 유효한 비트 깊이 (예: 14) */
    XpePixelFormat format;        /* 픽셀 데이터 타입 */
    void*          data;          /* 픽셀 데이터 — xpe_alloc_image를 통해 할당 */
    size_t         dataSize;      /* data 버퍼의 바이트 크기; 최대 64MB (4096x4096x4) */
} XpeImageBuffer;

typedef struct XpeImageMetadata {
    char     bodyPart[64];     /* Null 종료 신체 부위 레이블 (예: "CHEST") */
    float    kVp;              /* 튜브 전압(킬로볼트 피크) */
    float    mAs;              /* 튜브 전류-시간 곱 (밀리암페어-초) */
    float    SID_mm;           /* Source-Image Distance(밀리미터) */
    float    pixelPitch_mm;    /* 픽셀 피치(밀리미터) */
    uint64_t acquisitionTime;  /* UNIX epoch 밀리초 */
    uint32_t flags;            /* 비트필드: XPE_FLAG_* 상수 참조 */
} XpeImageMetadata;

#pragma pack(pop)

/* flags 비트필드 값 */
#define XPE_FLAG_GHOST_CORRECTED         0x00000001u
#define XPE_FLAG_AI_PROCESSED            0x00000002u
#define XPE_FLAG_DEFECT_CORRECTED        0x00000004u
#define XPE_FLAG_GAIN_CORRECTED          0x00000008u
#define XPE_FLAG_READOUT_VALIDATED       0x00000010u
#define XPE_FLAG_TEMP_COMPENSATED        0x00000020u
#define XPE_FLAG_NONLINEARITY_CORRECTED  0x00000040u
#define XPE_FLAG_BINNING_CORRECTED       0x00000080u
#define XPE_FLAG_AED_TRIGGERED           0x00000100u
#define XPE_FLAG_COLLIMATION_DETECTED    0x00000200u
#define XPE_FLAG_STITCHED                0x00000400u
#define XPE_FLAG_BONE_SUPPRESSED         0x00000800u
#define XPE_FLAG_GSVG_SKIPPED            0x00001000u
```

### XPE 오류 코드

`modules/common/include/xpe/common/xpe_error.h`에 정의:

```c
typedef int32_t XpeErrorCode;

#define XPE_OK                       0   /* 성공 */
#define XPE_ERR_INVALID_INPUT       -1   /* NULL 포인터, 범위 벗어난 값, 잘못된 치수 */
#define XPE_ERR_OUT_OF_MEMORY       -2   /* 힙 할당 실패 */
#define XPE_ERR_PROCESSING_FAILED   -3   /* 알고리즘 내부 오류 */
#define XPE_ERR_CONFIG_INVALID      -4   /* 형식 오류 또는 지원하지 않는 JSON 설정 */
#define XPE_ERR_CALIBRATION_EXPIRED -5   /* 캘리브레이션 데이터 만료 */
#define XPE_ERR_NOT_INITIALIZED     -6   /* xpe_init() 미호출 또는 실패 */
#define XPE_ERR_UNSUPPORTED_FORMAT  -7   /* XpePixelFormat이 이 함수에서 지원하지 않음 */
#define XPE_ERR_BUFFER_TOO_SMALL    -8   /* Caller 출력 버퍼 부족 */
#define XPE_ERR_IO_FAILED           -9   /* 파일 읽기/쓰기 오류 */
#define XPE_ERR_NETWORK_FAILED      -10  /* DICOM 네트워크 (C-STORE / C-FIND) 오류 */
```

---

## 3. GSVG 타입

GSVG는 독립적으로 정의 — GSVG는 xpe_common 타입에 의존하지 않음:

```c
#pragma pack(push, 8)

typedef struct GsvgConfig {
    int32_t  gridFrequency_lp_per_mm; /* 안티 산란 격자 선 주파수 */
    float    gridAngle_deg;           /* 격자 방향각(도) */
    int32_t  algorithmMode;           /* 0=Auto, 1=Fourier, 2=Wavelet */
    float    suppressionStrength;     /* 0.0–1.0; 억제 강도 */
    int32_t  enableVirtualGrid;       /* 1 = 억제 후 가상 격자 합성 */
    char     reserved[64];           /* Zero 패딩, 향후 확장용 */
} GsvgConfig;

typedef struct GsvgImageMetadata {
    uint32_t width;            /* 이미지 너비(픽셀) */
    uint32_t height;           /* 이미지 높이(픽셀) */
    float    pixelPitch_mm;    /* 검출기 픽셀 피치(밀리미터) */
    float    kVp;              /* 획득 튜브 전압 */
    float    mAs;              /* 획득 튜브 전류-시간 곱 */
    char     bodyPart[64];     /* 신체 부위 레이블 */
    uint32_t flags;            /* GSVG_FLAG_* 비트필드 */
} GsvgImageMetadata;

#pragma pack(pop)

/* GsvgErrorCode */
typedef int32_t GsvgErrorCode;
#define GSVG_OK                      0
#define GSVG_ERR_INVALID_INPUT      -1
#define GSVG_ERR_OUT_OF_MEMORY      -2
#define GSVG_ERR_PROCESSING_FAILED  -3
#define GSVG_ERR_CONFIG_INVALID     -4
#define GSVG_ERR_GRID_NOT_DETECTED  -5
#define GSVG_ERR_LUT_LOAD_FAILED    -6
#define GSVG_ERR_UNSUPPORTED_FORMAT -7
```

---

## 4. 함수 카운트 요약

| DLL | 내보낸 함수 | v1.1.0 대비 변경 |
|-----|--------------------|--------------------|
| xpe_common.dll | 18 | +3 (AED 함수 §5.16-5.18) |
| xpe_preprocess.dll | 18 | — |
| xpe_enhance_basic.dll | 7 | +1 (xpe_calc_exposure_index SPEC v2.0.0 §3.9에 따라 enhance_advanced에서 이동) |
| xpe_enhance_advanced.dll | 3 | -1 (xpe_calc_exposure_index가 enhance_basic로 이동) |
| xpe_ai.dll | 7 | — |
| xpe_display.dll | 11 | — |
| xpe_dicom.dll | 10 | — |
| gsvg.dll | 8 | — |
| **합계** | **82** | **+3** |

---

## 5. xpe_common.dll

라이브러리 생명 주기, 메모리 관리, 설정, 매개변수 범위, 경고 폴링, 로깅을 제공합니다.

의존성: 없음 (기본 계층).

### 5.1 xpe_init

```c
XPE_API XpeErrorCode xpe_init(const char* configJsonOrNull);
```

**설명**: 모든 XPE 서브시스템을 초기화합니다. 다른 XPE 함수 호출 전에 한 번 호출해야 합니다. 기본 설정을 수락하려면 `NULL` 전달.  
**SRS**: SRS-INIT-001, SRS-INIT-002  
**스레드 안전성**: 스레드 안전하지 않음 — 시작 시 단일 스레드에서 호출.  
**오류 코드**: `XPE_OK`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_OUT_OF_MEMORY`

---

### 5.2 xpe_shutdown

```c
XPE_API void xpe_shutdown(void);
```

**설명**: 모든 XPE 서브시스템 리소스를 해제합니다. XPE의 마지막 호출이어야 하며, 반환 후 XPE 함수 호출 불가.  
**SRS**: SRS-INIT-003  
**스레드 안전성**: 스레드 안전하지 않음 — 종료 시 단일 스레드에서 호출.  
**오류 코드**: (void — 반환값 없음)

---

### 5.3 xpe_version

```c
XPE_API const char* xpe_version(void);
```

**설명**: 정적 null 종료 버전 문자열 (예: `"1.0.0-rc1"`)에 대한 포인터를 반환합니다. 반환된 버퍼는 DLL 소유; 해제하지 말 것.  
**SRS**: SRS-VER-001  
**스레드 안전성**: 스레드 안전 (읽기 전용 정적 저장소).  
**오류 코드**: (NULL이 아님)

---

### 5.4 xpe_configure

```c
XPE_API XpeErrorCode xpe_configure(const char* jsonConfig);
```

**설명**: UTF-8 JSON 문자열에서 런타임 설정 업데이트를 적용합니다. JSON에 없는 키는 변경되지 않음. 향후 호환성: 알 수 없는 키는 자동 무시.  
**SRS**: SRS-CFG-001, SRS-CFG-002  
**스레드 안전성**: 스레드 안전하지 않음 — 설정 변경을 처리 호출과 직렬화.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT` (NULL), `XPE_ERR_CONFIG_INVALID`

---

### 5.5 xpe_alloc_image

```c
XPE_API XpeErrorCode xpe_alloc_image(uint32_t width, uint32_t height,
                                      XpePixelFormat format, XpeImageBuffer* out);
```

**설명**: `out->data`에 대한 픽셀 데이터를 할당하고 `*out`의 모든 필드를 채웁니다. Caller는 결국 동일한 버퍼에 대해 `xpe_free_image`를 호출해야 합니다.  
**SRS**: SRS-MEM-001, SRS-MEM-002  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_OUT_OF_MEMORY`, `XPE_ERR_UNSUPPORTED_FORMAT`

---

### 5.6 xpe_free_image

```c
XPE_API XpeErrorCode xpe_free_image(XpeImageBuffer* buf);
```

**설명**: `xpe_alloc_image`에 의해 할당된 `data` 버퍼를 해제하고 `buf->data`와 `buf->dataSize`를 0으로 설정합니다. Zero 초기화 버퍼 전달은 no-op.  
**SRS**: SRS-MEM-003  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT` (NULL buf)

---

### 5.7 xpe_copy_image

```c
XPE_API XpeErrorCode xpe_copy_image(const XpeImageBuffer* src, XpeImageBuffer* dst);
```

**설명**: 픽셀 데이터를 `src`에서 사전 할당된 `dst`로 복사합니다. `dst`는 이미 일치하는 치수와 형식으로 할당되어 있어야 합니다.  
**SRS**: SRS-MEM-004  
**스레드 안전성**: 재진입 가능 (src와 dst가 독립적인 버퍼인 경우).  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_BUFFER_TOO_SMALL`

---

### 5.8 xpe_error_string

```c
XPE_API const char* xpe_error_string(XpeErrorCode code);
```

**설명**: `code`에 대한 정적 인간 가독성 영어 설명을 반환합니다. 알 수 없는 코드는 `"Unknown error"`를 반환합니다. 반환된 포인터는 DLL 소유.  
**SRS**: SRS-ERR-001  
**스레드 안전성**: 스레드 안전 (읽기 전용 정적 저장소).  
**오류 코드**: (NULL이 아님)

---

### 5.9 xpe_get_pending_alert_count

```c
XPE_API int32_t xpe_get_pending_alert_count(void);
```

**설명**: 내부 경고 링 버퍼에 대기 중인 읽지 않은 경고의 수를 반환합니다. 대기 중인 경고가 없으면 0을 반환.  
**SRS**: SRS-ALERT-001, SRS-ALERT-002  
**스레드 안전성**: 스레드 안전 (원자적 읽기).  
**오류 코드**: (카운트 반환; 음수 값은 내부 오류 표시)

---

### 5.10 xpe_get_pending_alert

```c
XPE_API XpeErrorCode xpe_get_pending_alert(int32_t index, char* msg, size_t msgLen,
                                            int32_t* severity);
```

**설명**: `index`의 경고 메시지를 caller 제공 `msg` 버퍼 (`msgLen` 바이트)로 복사하고, `*severity`를 `XpeAlertSeverity` 값으로 설정합니다. `index`는 0 기반; 경고를 소비하지 않음.  
**SRS**: SRS-ALERT-003, SRS-ALERT-004  
**스레드 안전성**: 스레드 안전.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_BUFFER_TOO_SMALL`

---

### 5.11 xpe_clear_alerts

```c
XPE_API void xpe_clear_alerts(void);
```

**설명**: 경고 링 버퍼의 모든 대기 중인 경고를 폐기합니다.  
**SRS**: SRS-ALERT-005, SRS-ALERT-006  
**스레드 안전성**: 스레드 안전.  
**오류 코드**: (void)

---

### 5.12 xpe_get_param_range

```c
XPE_API XpeErrorCode xpe_get_param_range(const char* bodyPart, const char* paramName,
                                          float* minVal, float* maxVal, float* defaultVal);
```

**설명**: 신체 부위 범위로 지정된 이름의 처리 매개변수에 대한 유효한 범위 및 기본값을 검색합니다. GUI 슬라이더가 사용자 입력을 하드코딩된 제한 없이 제한하는 데 사용됩니다.  
**SRS**: SRS-SAFE-002, SRS-SAFE-005  
**스레드 안전성**: 스레드 안전 (읽기 전용 룩업).  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_NOT_INITIALIZED`

---

### 5.13 xpe_log_set_level

```c
XPE_API XpeErrorCode xpe_log_set_level(int32_t level);
```

**설명**: 최소 로그 심각도 수준을 설정합니다 (0=TRACE, 1=DEBUG, 2=INFO, 3=WARN, 4=ERROR, 5=OFF). 이 수준 아래의 메시지는 폐기됩니다.  
**SRS**: SRS-LOG-001  
**스레드 안전성**: 스레드 안전하지 않음 — 처리 시작 전에 시작 시 설정.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT` (범위 0–5 벗어난 level)

---

### 5.14 xpe_log_set_file

```c
XPE_API XpeErrorCode xpe_log_set_file(const char* filePath);
```

**설명**: 로그 출력을 `filePath`의 파일로 리다이렉트합니다 (UTF-8 경로). `NULL`을 전달하면 stderr로 되돌립니다. 파일은 append 모드로 열립니다.  
**SRS**: SRS-LOG-002  
**스레드 안전성**: 스레드 안전하지 않음 — 시작 시 설정.  
**오류 코드**: `XPE_OK`, `XPE_ERR_IO_FAILED`, `XPE_ERR_INVALID_INPUT`

---

### 5.15 xpe_log_flush

```c
XPE_API void xpe_log_flush(void);
```

**설명**: 버퍼링된 로그 항목을 디스크에 즉시 플러시합니다. 프로세스 종료 또는 충돌 보고 전에 유용합니다.  
**SRS**: SRS-LOG-003  
**스레드 안전성**: 스레드 안전.  
**오류 코드**: (void)

---

### 5.16 xpe_aed_configure

```c
XPE_API XpeErrorCode xpe_aed_configure(const char* configJsonOrNull);
```

**설명**: UTF-8 JSON 문자열에서 타이밍 및 임계값 매개변수를 사용하여 Automatic Exposure Detection (AED) 서브시스템을 구성합니다. 기본 설정을 수락하려면 `NULL` 전달. `xpe_init()` 후에 호출되어야 합니다. AED는 들어오는 프레임 데이터를 모니터링하여 노출 이벤트를 생성하고 `xpe_aed_poll_event()`를 통해 소비되는 이벤트를 생성합니다.  
**SRS**: SRS-AED-001, SRS-AED-002  
**스레드 안전성**: 스레드 안전하지 않음 — 수집 시작 전에 단일 스레드에서 호출.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_NOT_INITIALIZED`

**JSON 스키마 (NULL일 경우 기본값)**:
```json
{
  "aed": {
    "trigger_threshold_adu": 500,
    "settle_time_ms": 100,
    "min_exposure_ms": 5,
    "max_exposure_ms": 5000
  }
}
```

---

### 5.17 xpe_aed_poll_event

```c
XPE_API XpeErrorCode xpe_aed_poll_event(int32_t* eventTypeOut,
                                         uint64_t* timestampOut,
                                         float* signalLevelOut);
```

**설명**: AED 이벤트 큐에서 다음 대기 중인 노출 감지 이벤트를 폴링합니다. 이벤트 타입 (0=exposure_start, 1=exposure_end, 2=exposure_trigger), timestamp (UNIX epoch ms), 감지된 신호 수준을 출력 매개변수에 씁니다. 이벤트를 사용할 수 있으면 `XPE_OK`를 반환하거나, 큐가 비어 있으면 오류 아닌 표시를 반환합니다.  
**SRS**: SRS-AED-003, SRS-AED-004  
**스레드 안전성**: 스레드 안전.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT` (NULL 포인터), `XPE_ERR_NOT_INITIALIZED`

---

### 5.18 xpe_aed_get_status

```c
XPE_API XpeErrorCode xpe_aed_get_status(int32_t* stateOut);
```

**설명**: 현재 AED 상태 머신 상태를 반환합니다. 상태는 다음 중 하나입니다: 0=IDLE (구성되지 않음 또는 노출 사이), 1=ARMED (구성됨 및 노출 대기), 2=TRIGGERED (노출 감지됨, 이벤트 대기 중).  
**SRS**: SRS-AED-005  
**스레드 안전성**: 스레드 안전 (원자적 읽기).  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT` (NULL 포인터), `XPE_ERR_NOT_INITIALIZED`

---

## 6. xpe_preprocess.dll

오프라인 캘리브레이션 (오프셋 / 게인 / 결함 맵), 런타임 보정, 고스트 아티팩트 보정을 제공합니다.

의존성: xpe_common.dll.

### 6.1 xpe_offset_correct

```c
XPE_API XpeErrorCode xpe_offset_correct(XpeImageBuffer* img,
                                         const XpeImageBuffer* offsetMap);
```

**설명**: 픽셀 단위 어두운 오프셋 맵을 `img`에서 인 플레이스로 빼기합니다. 두 버퍼는 동일한 치수와 형식을 가져야 합니다.  
**SRS**: SRS-CALIB-001  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_NOT_INITIALIZED`

---

### 6.2 xpe_gain_correct

```c
XPE_API XpeErrorCode xpe_gain_correct(XpeImageBuffer* img,
                                       const XpeImageBuffer* gainMap);
```

**설명**: 픽셀 단위 flat-field 게인 보정을 `img`에 인 플레이스로 적용합니다. 두 버퍼는 치수와 형식을 공유해야 합니다.  
**SRS**: SRS-CALIB-002  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_NOT_INITIALIZED`

---

### 6.3 xpe_defect_correct

```c
XPE_API XpeErrorCode xpe_defect_correct(XpeImageBuffer* img,
                                         const XpeImageBuffer* defectMap,
                                         const char* configJsonOrNull);
```

**설명**: `defectMap`에서 식별된 불량 픽셀 값을 보간된 인접 값으로 바꿉니다. `configJsonOrNull`은 보간 모드 (nearest/bilinear/median)를 지정할 수 있습니다.  
**SRS**: SRS-CALIB-003, SRS-CALIB-004  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_NOT_INITIALIZED`

---

### 6.4 xpe_defect_detect_runtime

```c
XPE_API XpeErrorCode xpe_defect_detect_runtime(const XpeImageBuffer* img,
                                                XpeImageBuffer* defectMapOut,
                                                const char* configJsonOrNull);
```

**설명**: 획득 시간에 `img`의 일시적 결함 픽셀을 감지하고 boolean 결함 맵을 `defectMapOut` (사전 할당, 동일한 치수)에 씁니다. 정적 캘리브레이션 결함 맵을 보완합니다.  
**SRS**: SRS-CALIB-005  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_PROCESSING_FAILED`

---

### 6.5 xpe_ghost_create

```c
XPE_API XpeErrorCode xpe_ghost_create(uint32_t width, uint32_t height,
                                       const char* configJsonOrNull,
                                       void** handleOut);
```

**설명**: 주어진 이미지 치수에 대한 opaque 고스트 보정기 핸들을 만듭니다. 보정기는 `xpe_ghost_correct` 호출 전체에 걸쳐 히스토리를 축적합니다. 핸들을 저장하고 프레임 전체에서 재사용합니다.  
**SRS**: SRS-GHOST-001, SRS-GHOST-002  
**스레드 안전성**: 재진입 가능 (각 핸들은 독립적).  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_OUT_OF_MEMORY`, `XPE_ERR_CONFIG_INVALID`

---

### 6.6 xpe_ghost_correct

```c
XPE_API XpeErrorCode xpe_ghost_correct(void* handle, XpeImageBuffer* img,
                                        const XpeImageMetadata* meta);
```

**설명**: `handle`에 축적된 히스토리를 사용하여 `img`에 고스트 (지연) 보정을 인 플레이스로 적용합니다. 후속 프레임에 대해 내부 상태를 업데이트합니다.  
**SRS**: SRS-GHOST-003, SRS-GHOST-004  
**스레드 안전성**: 핸들당 재진입 가능 (단일 핸들을 스레드 전체에서 공유하지 말 것).  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_PROCESSING_FAILED`

---

### 6.7 xpe_ghost_reset

```c
XPE_API XpeErrorCode xpe_ghost_reset(void* handle);
```

**설명**: 파괴하지 않고 `handle`의 지연 히스토리를 지웁니다. 환자 수집 사이 또는 검출기 전원 순환 후에 호출합니다.  
**SRS**: SRS-GHOST-005  
**스레드 안전성**: 핸들당 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`

---

### 6.8 xpe_ghost_destroy

```c
XPE_API void xpe_ghost_destroy(void* handle);
```

**설명**: `xpe_ghost_create`에 의해 만들어진 고스트 보정기 핸들과 관련된 모든 리소스를 해제합니다. 이 호출 후 `handle`은 유효하지 않습니다.  
**SRS**: SRS-GHOST-006  
**스레드 안전성**: 스레드 안전하지 않음 — 파괴 시 `handle`의 동시 사용이 없도록 보장.  
**오류 코드**: (void)

---

### 6.9 xpe_calib_load_offset

```c
XPE_API XpeErrorCode xpe_calib_load_offset(const char* filePath,
                                            XpeImageBuffer* offsetMapOut);
```

**설명**: `filePath`에서 오프셋 (어두운) 캘리브레이션 이미지를 사전 할당된 `offsetMapOut`으로 로드합니다. 파일 형식은 확장명으로 결정됩니다 (.raw, .dcm).  
**SRS**: SRS-CALIB-010  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_IO_FAILED`, `XPE_ERR_CALIBRATION_EXPIRED`

---

### 6.10 xpe_calib_load_gain

```c
XPE_API XpeErrorCode xpe_calib_load_gain(const char* filePath,
                                          XpeImageBuffer* gainMapOut);
```

**설명**: `filePath`에서 flat-field 게인 캘리브레이션 이미지를 사전 할당된 `gainMapOut`으로 로드합니다.  
**SRS**: SRS-CALIB-011  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_IO_FAILED`, `XPE_ERR_CALIBRATION_EXPIRED`

---

### 6.11 xpe_calib_load_defect_map

```c
XPE_API XpeErrorCode xpe_calib_load_defect_map(const char* filePath,
                                                XpeImageBuffer* defectMapOut);
```

**설명**: `filePath`에서 정적 결함 픽셀 맵을 사전 할당된 `defectMapOut`으로 로드합니다. 맵 픽셀은 결함이 존재하는 곳에서 0이 아닙니다.  
**SRS**: SRS-CALIB-012  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_IO_FAILED`

---

### 6.12 xpe_calib_generate_offset

```c
XPE_API XpeErrorCode xpe_calib_generate_offset(const XpeImageBuffer* frames,
                                                uint32_t frameCount,
                                                XpeImageBuffer* offsetMapOut,
                                                const char* configJsonOrNull);
```

**설명**: `frameCount` 어두운 필드 `frames`를 평균화하여 `offsetMapOut`의 오프셋 캘리브레이션 맵을 생성합니다. `frames`는 `XpeImageBuffer` 구조체의 연속 배열입니다.  
**SRS**: SRS-CALIB-020  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_OUT_OF_MEMORY`, `XPE_ERR_PROCESSING_FAILED`

---

### 6.13 xpe_calib_check_expiry

```c
XPE_API XpeErrorCode xpe_calib_check_expiry(const char* filePath,
                                             uint64_t* expiryEpochMsOut);
```

**설명**: `filePath`의 캘리브레이션 파일에 내장된 만료 타임스탐프를 읽고 `*expiryEpochMsOut` (UNIX epoch 밀리초)에 씁니다. 타임스탐프가 과거인 경우 `XPE_ERR_CALIBRATION_EXPIRED`를 반환합니다.  
**SRS**: SRS-CALIB-030, SRS-SAFE-010  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_CALIBRATION_EXPIRED`, `XPE_ERR_IO_FAILED`, `XPE_ERR_INVALID_INPUT`

---

### 6.14 xpe_calib_save

```c
XPE_API XpeErrorCode xpe_calib_save(const XpeImageBuffer* calibMap,
                                     const char* filePath,
                                     uint64_t expiryEpochMs,
                                     const char* configJsonOrNull);
```

**설명**: 내장된 만료 타임스탐프 `expiryEpochMs`를 사용하여 `calibMap`을 `filePath`에 씁니다. `configJsonOrNull`은 출력 형식 (raw/dcm)을 지정할 수 있습니다.  
**SRS**: SRS-CALIB-021  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_IO_FAILED`, `XPE_ERR_CONFIG_INVALID`

---

### 6.15 xpe_validate_readout_artifact

```c
XPE_API XpeErrorCode xpe_validate_readout_artifact(const XpeImageBuffer* rawImg,
                                                    int32_t* artifactScoreOut,
                                                    char* msgOut,
                                                    size_t msgLen);
```

**설명**: 보정 시작 전에 검출기 측 라인 노이즈, 삭제된 열, ADC 포화 패턴에 대한 원본 readout 프레임을 검증합니다. 정규화된 아티팩트 점수를 `*artifactScoreOut`에, 운영자 가독 요약을 `msgOut`에 씁니다. 비파괴적.  
**Traceability**: PRE-01, SRS-PERF-001  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_BUFFER_TOO_SMALL`, `XPE_ERR_PROCESSING_FAILED`

---

### 6.16 xpe_temp_compensate

```c
XPE_API XpeErrorCode xpe_temp_compensate(XpeImageBuffer* img,
                                          float detectorTempC,
                                          const char* configJsonOrNull);
```

**설명**: `configJsonOrNull`로 선택된 LUT 또는 다항식 계수를 사용하여 `img`에 검출기 온도 보정을 인 플레이스로 적용합니다. Caller는 현재 검출기 온도(섭씨)를 전달합니다.  
**Traceability**: PRE-07, SRS-PERF-001  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_PROCESSING_FAILED`

---

### 6.17 xpe_nonlinearity_correct

```c
XPE_API XpeErrorCode xpe_nonlinearity_correct(XpeImageBuffer* img,
                                               const char* configJsonOrNull);
```

**설명**: `configJsonOrNull`로 선택된 사전 특성화된 캘리브레이션 계수를 사용하여 `img`의 검출기 응답 비선형성을 인 플레이스로 보정합니다. 선택 검출기 모드 및 계수 집합.  
**Traceability**: PRE-08, SRS-PERF-001  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_PROCESSING_FAILED`

---

### 6.18 xpe_binning_correct

```c
XPE_API XpeErrorCode xpe_binning_correct(XpeImageBuffer* img,
                                          int32_t binningMode,
                                          const char* configJsonOrNull);
```

**설명**: `img`의 binned 획득 데이터에 대한 모드별 보정을 인 플레이스로 적용합니다. `binningMode`는 검출기 정의 (예: 1=`1x1`, 2=`2x2`)이고 로드된 캘리브레이션 프로필과 일치해야 합니다.  
**Traceability**: PRE-09, SRS-PERF-001  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_PROCESSING_FAILED`

---

## 7. xpe_enhance_basic.dll

기본 이미지 향상 작업을 제공합니다: 로그 변환, 노이즈 감소, 대비, 에지 향상.

의존성: xpe_common.dll.

### 7.1 xpe_log_transform

```c
XPE_API XpeErrorCode xpe_log_transform(XpeImageBuffer* img,
                                        const char* configJsonOrNull);
```

**설명**: film-screen 응답을 근사하기 위해 동적 범위를 압축하는 로그 강도 변환을 `img`에 인 플레이스로 적용합니다. `configJsonOrNull`은 base 및 offset을 지정할 수 있습니다.  
**SRS**: SRS-ENH-001  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_UNSUPPORTED_FORMAT`

---

### 7.2 xpe_log_inverse

```c
XPE_API XpeErrorCode xpe_log_inverse(XpeImageBuffer* img,
                                      const char* configJsonOrNull);
```

**설명**: 로그 변환의 역 (지수)을 `img`에 인 플레이스로 적용하여 선형 강도 값을 복원합니다. 매개변수는 전방 변환과 일치해야 합니다.  
**SRS**: SRS-ENH-002  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_UNSUPPORTED_FORMAT`

---

### 7.3 xpe_noise_reduce

```c
XPE_API XpeErrorCode xpe_noise_reduce(XpeImageBuffer* img,
                                       const char* configJsonOrNull);
```

**설명**: 적응적 필터링 (bilateral 또는 non-local means, config를 통해 선택 가능)을 사용하여 `img`의 quantum 노이즈를 인 플레이스로 감소시킵니다. 강도 및 커널 크기는 구성 가능.  
**SRS**: SRS-ENH-010, SRS-ENH-011  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_PROCESSING_FAILED`

---

### 7.4 xpe_noise_estimate_sigma

```c
XPE_API XpeErrorCode xpe_noise_estimate_sigma(const XpeImageBuffer* img,
                                               float* sigmaOut);
```

**설명**: wavelet 기반 estimator를 사용하여 `img`의 additive 노이즈 표준 편차를 추정하고 결과를 `*sigmaOut`에 씁니다. 비파괴적.  
**SRS**: SRS-ENH-012  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_PROCESSING_FAILED`

---

### 7.5 xpe_contrast_enhance

```c
XPE_API XpeErrorCode xpe_contrast_enhance(XpeImageBuffer* img,
                                           const char* configJsonOrNull);
```

**설명**: Contrast Limited Adaptive Histogram Equalization (CLAHE) 또는 유사 기술을 `img`에 인 플레이스로 적용합니다. Clip limit, tile 크기, 방법은 구성 가능.  
**SRS**: SRS-ENH-020  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_PROCESSING_FAILED`

---

### 7.6 xpe_edge_enhance

```c
XPE_API XpeErrorCode xpe_edge_enhance(XpeImageBuffer* img,
                                       const char* configJsonOrNull);
```

**설명**: unsharp masking 또는 Laplacian 향상을 통해 `img`의 에지를 인 플레이스로 샤프닝합니다. 강도 및 반경은 구성 가능.  
**SRS**: SRS-ENH-021  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CONFIG_INVALID`

---

### 7.7 xpe_calc_exposure_index

```c
XPE_API XpeErrorCode xpe_calc_exposure_index(const XpeImageBuffer* img,
                                              const XpeImageMetadata* meta,
                                              float* eiOut,
                                              float* deviationIndexOut);
```

**설명**: 검출기 도메인의 사전 프레젠테이션 이미지에 대한 IEC 62494 Exposure Index (EI) 및 Deviation Index (DI)를 계산하여 결과를 `*eiOut` 및 `*deviationIndexOut`에 씁니다. 전체 이미지 EI는 항상 지원됩니다. 유효한 collimation ROI sidecar를 사용할 수 있으면, caller에 의해 관련 이미지 영역이 해당 ROI로 제한될 수 있습니다. Exam/view metadata는 기본 `EIT`를 선택합니다. `meta->bodyPart`는 사용 가능할 때 기본값을 개선할 수 있습니다. Stitched 또는 multi-irradiation 이미지는 non-normative 입력이며 caller에 의해 거부되거나 명시적으로 플래그되어야 합니다.

**Phase 할당**: 이 함수는 xpe_enhance_basic.dll에서 구현됩니다 (Phase 1b). Phase 2에서 orchestrator는 collimation ROI로 crop된 이미지를 사용하여 이 함수를 재호출하여 ROI 인식 EI refinement를 위합니다. 별도의 API가 필요하지 않습니다.

**SRS**: SRS-ADV-030, SRS-SAFE-016, SRS-EI-001  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_PROCESSING_FAILED`

---

## 8. xpe_enhance_advanced.dll

멀티 스케일 주파수 처리, fractional calculus 향상, collimation 감지를 제공합니다.

의존성: xpe_common.dll.

실행 순서 (`xpe_log_transform` before advanced enhancement)는 caller/orchestrator에 의해 강제되며, DLL 간 의존성이 아닙니다.

**주의**: xpe_calc_exposure_index는 SPEC-XPE-MASTER v2.0.0 §3.9에 따라 xpe_enhance_basic.dll로 이동했습니다 (§7.7). Phase 2 ROI 인식 EI refinement는 orchestrator가 collimation ROI crop 이미지를 사용하여 이 함수를 재호출하여 수행됩니다.

### 8.1 xpe_multiscale_process

```c
XPE_API XpeErrorCode xpe_multiscale_process(XpeImageBuffer* img,
                                             const XpeImageMetadata* meta,
                                             const char* configJsonOrNull);
```

**설명**: `img`를 multi-scale framework (예: Laplacian pyramid)를 사용한 주파수 sub-band로 분해하고, `meta`에서 파생된 per-band 향상 계수를 적용하고, 인 플레이스로 재구성합니다.  
**SRS**: SRS-ADV-001, SRS-ADV-002  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_PROCESSING_FAILED`

---

### 8.2 xpe_fractional_process

```c
XPE_API XpeErrorCode xpe_fractional_process(XpeImageBuffer* img,
                                             float order,
                                             const char* configJsonOrNull);
```

**설명**: `order` (0.0–2.0) 차수의 fractional 미분 연산자를 `img`에 인 플레이스로 적용합니다. 1.0 근처의 값은 에지를 보존합니다; 2.0 근처의 값은 미세 질감을 강조합니다.  
**SRS**: SRS-ADV-010  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT` (order 범위 벗어남), `XPE_ERR_PROCESSING_FAILED`

---

### 8.3 xpe_detect_collimation

```c
XPE_API XpeErrorCode xpe_detect_collimation(const XpeImageBuffer* img,
                                             int32_t* x0Out, int32_t* y0Out,
                                             int32_t* x1Out, int32_t* y1Out,
                                             const char* configJsonOrNull);
```

**설명**: `img`의 collimation 경계 (primary beam edge)를 감지하고 픽셀 좌표의 경계 사각형을 `(x0,y0)–(x1,y1)`에 씁니다. 비파괴적.  
**SRS**: SRS-ADV-020, SRS-SAFE-015  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_PROCESSING_FAILED`

---

## 9. xpe_ai.dll

deep-learning 추론을 제공합니다: 신체 부위 인식, stitching, bone suppression, DL 기반 denoising.

의존성: xpe_common.dll. GPU via ONNX Runtime / TensorRT (선택 사항).

실행 모델: `xpe_ai.dll`는 in-process C ABI proxy입니다. 실제 추론은 sandboxed companion worker 프로세스 (`xpe_ai_worker.exe`)에서 IPC를 통해 실행됩니다. Worker 시작, heartbeat, crash 격리는 `xpe_ai_init` / `xpe_ai_shutdown` 내에서 처리됩니다.

### 9.1 xpe_ai_init

```c
XPE_API XpeErrorCode xpe_ai_init(const char* modelDirPath,
                                  const char* configJsonOrNull);
```

**설명**: sandboxed AI worker를 시작하거나 연결하고, `modelDirPath`에서 모델 파일을 로드하고, worker 측 추론 런타임을 초기화합니다. 다른 xpe_ai 함수 이전에 호출되어야 합니다. `configJsonOrNull`은 device (CPU/CUDA), IPC timeout, batch 설정을 선택합니다.  
**SRS**: SRS-AI-001, SRS-AI-002  
**스레드 안전성**: 스레드 안전하지 않음 — 시작 시 단일 스레드에서 호출.  
**오류 코드**: `XPE_OK`, `XPE_ERR_IO_FAILED`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_OUT_OF_MEMORY`

---

### 9.2 xpe_ai_shutdown

```c
XPE_API void xpe_ai_shutdown(void);
```

**설명**: sandboxed AI worker 세션을 중지하고, 모델을 언로드하고, IPC 리소스를 해제합니다. 이 호출 후 xpe_ai 함수 호출 불가.  
**SRS**: SRS-AI-003  
**스레드 안전성**: 스레드 안전하지 않음 — 종료 시 단일 스레드에서 호출.  
**오류 코드**: (void)

---

### 9.3 xpe_bodypart_recognize

```c
XPE_API XpeErrorCode xpe_bodypart_recognize(const XpeImageBuffer* img,
                                             char* bodyPartOut, size_t bufLen,
                                             float* confidenceOut);
```

**설명**: CNN classifier를 사용하여 `img`의 anatomical 신체 부위를 분류합니다. 레이블 (예: `"CHEST"`)을 `bodyPartOut`에, confidence 점수 [0,1]을 `*confidenceOut`에 씁니다.  
**SRS**: SRS-AI-010  
**스레드 안전성**: 재진입 가능 (호출당 스레드 안전 추론 세션).  
**오류 코드**: `XPE_OK`, `XPE_ERR_NOT_INITIALIZED`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_BUFFER_TOO_SMALL`, `XPE_ERR_PROCESSING_FAILED`

---

### 9.4 xpe_stitch_images

```c
XPE_API XpeErrorCode xpe_stitch_images(const XpeImageBuffer* parts,
                                        uint32_t partCount,
                                        XpeImageBuffer* stitchedOut,
                                        const char* configJsonOrNull);
```

**설명**: `parts` 배열의 `partCount` overlapping 부분 이미지를 `stitchedOut` (via `xpe_stitch_estimate_size` 사전 할당됨)의 단일 wide-field 이미지로 stitches합니다. 정렬은 AI 기반 feature matching을 사용합니다.  
**SRS**: SRS-AI-020, SRS-AI-021  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_PROCESSING_FAILED`, `XPE_ERR_BUFFER_TOO_SMALL`

---

### 9.5 xpe_stitch_estimate_size

```c
XPE_API XpeErrorCode xpe_stitch_estimate_size(const XpeImageBuffer* parts,
                                               uint32_t partCount,
                                               uint32_t* widthOut,
                                               uint32_t* heightOut);
```

**설명**: stitching을 수행하지 않고 stitch 작업의 출력 치수를 추정합니다. 반환된 치수를 사용하여 `xpe_stitch_images`에 대한 버퍼를 사전 할당합니다.  
**SRS**: SRS-AI-020  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_PROCESSING_FAILED`

---

### 9.6 xpe_bone_suppress

```c
XPE_API XpeErrorCode xpe_bone_suppress(const XpeImageBuffer* img,
                                        XpeImageBuffer* softTissueOut,
                                        const char* configJsonOrNull);
```

**설명**: U-Net 스타일 모델을 사용하여 bone을 억제하여 soft tissue만 보이도록 하는 이미지를 `softTissueOut`에서 생성합니다. `softTissueOut`은 `img`와 동일한 치수로 사전 할당되어야 합니다.  
**SRS**: SRS-AI-030  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_NOT_INITIALIZED`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_PROCESSING_FAILED`

---

### 9.7 xpe_dl_denoise

```c
XPE_API XpeErrorCode xpe_dl_denoise(XpeImageBuffer* img,
                                     const XpeImageMetadata* meta,
                                     const char* configJsonOrNull);
```

**설명**: deep-learning denoising 네트워크를 `img`에 인 플레이스로 적용합니다. `meta->bodyPart` 및 `meta->mAs`에 따라 모델 variant를 선택합니다. classical `xpe_noise_reduce`를 보완하고 (그리고 대체할 수 있음).  
**SRS**: SRS-AI-040  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_NOT_INITIALIZED`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_PROCESSING_FAILED`

---

## 10. xpe_display.dll

DICOM 표준 LUT 파이프라인을 제공합니다: Modality LUT, VOI LUT, Presentation LUT, preset 관리, auto-selection.

의존성: xpe_common.dll.

### 10.1 xpe_modality_lut_apply

```c
XPE_API XpeErrorCode xpe_modality_lut_apply(XpeImageBuffer* img,
                                             float rescaleSlope,
                                             float rescaleIntercept);
```

**설명**: DICOM Modality LUT linear transformation `output = input * rescaleSlope + rescaleIntercept`를 `img`의 모든 픽셀에 인 플레이스로 적용합니다.  
**SRS**: SRS-DISP-001  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`

---

### 10.2 xpe_voi_lut_apply

```c
XPE_API XpeErrorCode xpe_voi_lut_apply(XpeImageBuffer* img,
                                        float windowCenter,
                                        float windowWidth,
                                        int32_t function);
```

**설명**: VOI LUT windowing 작업을 `img`에 인 플레이스로 적용합니다. `function`은 DICOM PS 3.3 C.7.6.3.1.5에 따라: 0=LINEAR, 1=LINEAR_EXACT, 2=SIGMOID를 선택합니다.  
**SRS**: SRS-DISP-010, SRS-DISP-011  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`

---

### 10.3 xpe_voi_lut_apply_fast

```c
XPE_API XpeErrorCode xpe_voi_lut_apply_fast(XpeImageBuffer* img,
                                              float windowCenter,
                                              float windowWidth,
                                              uint8_t* lut8bit,
                                              size_t lutLen);
```

**설명**: 실시간 디스플레이 (예: panning / scrolling)를 위해 사전 계산된 8비트 출력 LUT를 `img`에 적용합니다. `lut8bit`는 16비트 입력에서 8비트 출력으로의 65536항목 lookup table입니다.  
**SRS**: SRS-DISP-012, SRS-PERF-005  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_BUFFER_TOO_SMALL`

---

### 10.4 xpe_voi_lut_apply_sequence

```c
XPE_API XpeErrorCode xpe_voi_lut_apply_sequence(XpeImageBuffer* imgs,
                                                  uint32_t imgCount,
                                                  float windowCenter,
                                                  float windowWidth,
                                                  int32_t function);
```

**설명**: 일관된 series 디스플레이를 위해 `imgCount` 이미지 배열에 동일한 VOI LUT windowing을 batch 적용합니다. 각 이미지에 `xpe_voi_lut_apply`를 호출하는 것과 동일하지만 더 효율적.  
**SRS**: SRS-DISP-013  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`

---

### 10.5 xpe_presentation_lut_apply

```c
XPE_API XpeErrorCode xpe_presentation_lut_apply(XpeImageBuffer* img,
                                                  const char* presetNameOrNull,
                                                  const char* configJsonOrNull);
```

**설명**: Presentation LUT (gamma / perceptual linearisation)을 `img`에 인 플레이스로 적용합니다. `presetNameOrNull`은 이름의 preset을 선택; sRGB 기본값에는 `NULL` 전달.  
**SRS**: SRS-DISP-020  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CONFIG_INVALID`

---

### 10.6 xpe_presentation_lut_check_display

```c
XPE_API XpeErrorCode xpe_presentation_lut_check_display(float* gsdfComplianceOut);
```

**설명**: 연결된 디스플레이의 luminance 응답을 DICOM GSDF (Grayscale Standard Display Function)에 대해 측정하고 compliance 점수 [0,1]을 `*gsdfComplianceOut`에 씁니다.  
**SRS**: SRS-DISP-021, SRS-SAFE-020  
**스레드 안전성**: 스레드 안전.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_PROCESSING_FAILED`

---

### 10.7 xpe_lut_get_preset_count

```c
XPE_API int32_t xpe_lut_get_preset_count(void);
```

**설명**: 사용 가능한 LUT preset의 총 수 (built-in + custom)를 반환합니다. 내부 오류 시 음수 값을 반환합니다.  
**SRS**: SRS-DISP-030  
**스레드 안전성**: 스레드 안전.  
**오류 코드**: (카운트 반환; 음수 = 오류)

---

### 10.8 xpe_lut_get_preset

```c
XPE_API XpeErrorCode xpe_lut_get_preset(int32_t index,
                                         char* nameOut, size_t nameBufLen,
                                         char* descriptionOut, size_t descBufLen);
```

**설명**: `index` (0 기반)의 LUT preset의 이름과 설명을 caller 제공 버퍼에 복사합니다.  
**SRS**: SRS-DISP-031  
**스레드 안전성**: 스레드 안전.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_BUFFER_TOO_SMALL`

---

### 10.9 xpe_lut_add_custom_preset

```c
XPE_API XpeErrorCode xpe_lut_add_custom_preset(const char* name,
                                                 const char* description,
                                                 const char* lutDefinitionJson);
```

**설명**: JSON 정의 문자열에서 새로운 custom LUT preset을 등록합니다. Preset은 user preset store에 지속되고 즉시 선택을 위해 사용 가능.  
**SRS**: SRS-DISP-032  
**스레드 안전성**: 스레드 안전하지 않음 — preset 수정을 직렬화.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_IO_FAILED`

---

### 10.10 xpe_lut_remove_custom_preset

```c
XPE_API XpeErrorCode xpe_lut_remove_custom_preset(const char* name);
```

**설명**: 이름으로 custom LUT preset을 제거합니다. Built-in preset은 제거될 수 없습니다 (`XPE_ERR_INVALID_INPUT` 반환).  
**SRS**: SRS-DISP-033  
**스레드 안전성**: 스레드 안전하지 않음 — preset 수정을 직렬화.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_IO_FAILED`

---

### 10.11 xpe_lut_auto_select

```c
XPE_API XpeErrorCode xpe_lut_auto_select(const XpeImageMetadata* meta,
                                           char* presetNameOut, size_t bufLen);
```

**설명**: 주어진 이미지 metadata (신체 부위, modality, 획득 매개변수)에 대한 권장 LUT preset을 선택하고 preset 이름을 `presetNameOut`에 씁니다.  
**SRS**: SRS-DISP-040  
**스레드 안전성**: 스레드 안전.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_BUFFER_TOO_SMALL`

---

## 11. xpe_dicom.dll

DICOM 파일 I/O, tag 조작, GSPS annotation, 네트워크 서비스 (C-STORE / C-FIND MWL)를 제공합니다.

의존성: xpe_common.dll.

### 11.1 xpe_dicom_read

```c
XPE_API XpeErrorCode xpe_dicom_read(const char* filePath,
                                     XpeImageBuffer* imgOut,
                                     XpeImageMetadata* metaOut);
```

**설명**: `filePath`에서 DICOM 파일을 읽고, 픽셀 데이터를 `imgOut` (caller가 사전 할당하거나 자동 할당을 위해 zeroed struct 전달)으로 디코드하고, 주요 속성으로 `metaOut`을 채웁니다.  
**SRS**: SRS-DICOM-001, SRS-DICOM-002  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_IO_FAILED`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_UNSUPPORTED_FORMAT`, `XPE_ERR_OUT_OF_MEMORY`

---

### 11.2 xpe_dicom_query_dimensions

```c
XPE_API XpeErrorCode xpe_dicom_query_dimensions(const char* filePath,
                                                 uint32_t* widthOut,
                                                 uint32_t* heightOut,
                                                 XpePixelFormat* formatOut);
```

**설명**: 픽셀 데이터를 디코드하지 않고 `filePath`에서만 이미지 차원 및 형식 tags를 읽습니다. `xpe_dicom_read` 호출 전 버퍼를 사전 할당하는 데 사용.  
**SRS**: SRS-DICOM-003  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_IO_FAILED`, `XPE_ERR_INVALID_INPUT`

---

### 11.3 xpe_dicom_read_tag_string

```c
XPE_API XpeErrorCode xpe_dicom_read_tag_string(const char* filePath,
                                                uint16_t group, uint16_t element,
                                                char* valueOut, size_t bufLen);
```

**설명**: `filePath`에서 `(group, element)`로 식별된 단일 DICOM tag를 읽고 그 문자열 표현을 `valueOut`에 씁니다. VRs 지원: LO, LT, SH, ST, UI, UN, UT.  
**SRS**: SRS-DICOM-004  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_IO_FAILED`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_BUFFER_TOO_SMALL`

---

### 11.4 xpe_dicom_write

```c
XPE_API XpeErrorCode xpe_dicom_write(const char* filePath,
                                      const XpeImageBuffer* img,
                                      const XpeImageMetadata* meta,
                                      const char* configJsonOrNull);
```

**설명**: `img` 및 `meta`를 `filePath`의 DICOM Part 10 파일로 인코드합니다. `configJsonOrNull`은 transfer syntax (Explicit Little Endian, JPEG 2000 Lossless 등)를 지정할 수 있습니다.  
**SRS**: SRS-DICOM-010, SRS-DICOM-011  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_IO_FAILED`, `XPE_ERR_CONFIG_INVALID`

---

### 11.5 xpe_dicom_write_j2k

```c
XPE_API XpeErrorCode xpe_dicom_write_j2k(const char* filePath,
                                           const XpeImageBuffer* img,
                                           const XpeImageMetadata* meta,
                                           float compressionRatio);
```

**설명**: 지정된 `compressionRatio` (1.0 = lossless)에서 JPEG 2000 압축 픽셀 데이터를 사용하여 DICOM 파일을 씁니다. J2K transfer syntax가 있는 `xpe_dicom_write`에 대한 편의 wrapper.  
**SRS**: SRS-DICOM-012  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_IO_FAILED`, `XPE_ERR_PROCESSING_FAILED`

---

### 11.6 xpe_dicom_set_tag_string

```c
XPE_API XpeErrorCode xpe_dicom_set_tag_string(const char* filePath,
                                               uint16_t group, uint16_t element,
                                               const char* value);
```

**설명**: `filePath`의 기존 파일에서 string valued DICOM tag를 업데이트하거나 삽입합니다. 파일은 인 플레이스로 수정되고; backup은 생성되지 않습니다.  
**SRS**: SRS-DICOM-005  
**스레드 안전성**: 파일 경로당 스레드 안전하지 않음 — 동일한 파일에 대한 수정을 직렬화.  
**오류 코드**: `XPE_OK`, `XPE_ERR_IO_FAILED`, `XPE_ERR_INVALID_INPUT`

---

### 11.7 xpe_gsps_create

```c
XPE_API XpeErrorCode xpe_gsps_create(const char* referencedFilePath,
                                      const char* annotationJson,
                                      const char* gspsFilePathOut,
                                      size_t gspsPathBufLen);
```

**설명**: `referencedFilePath`를 참조하고 `annotationJson` (ROI, overlay, measurement)에서 annotation을 포함하는 DICOM Grayscale Softcopy Presentation State (GSPS) 객체를 만듭니다. GSPS 파일 경로를 `gspsFilePathOut`에 씁니다.  
**SRS**: SRS-DICOM-020  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_IO_FAILED`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_BUFFER_TOO_SMALL`

---

### 11.8 xpe_gsps_apply

```c
XPE_API XpeErrorCode xpe_gsps_apply(const char* gspsFilePath,
                                     XpeImageBuffer* img,
                                     const char* configJsonOrNull);
```

**설명**: GSPS 파일의 annotation을 `img`에 렌더링합니다 (인 플레이스로 overlays burn-in). 보조 capture 또는 print 출력에 유용.  
**SRS**: SRS-DICOM-021  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_IO_FAILED`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_PROCESSING_FAILED`

---

### 11.9 xpe_dicom_cstore

```c
XPE_API XpeErrorCode xpe_dicom_cstore(const char* filePath,
                                       const char* remoteAeTitle,
                                       const char* remoteHost,
                                       uint16_t    remotePort,
                                       const char* localAeTitle);
```

**설명**: C-STORE를 통해 DICOM 파일을 원격 SCP로 보냅니다. SCP가 상태 응답을 반환하거나 timeout이 발생할 때까지 차단합니다 (configurable via `xpe_configure`).  
**SRS**: SRS-DICOM-030  
**스레드 안전성**: 재진입 가능 (각 호출은 독립적인 DICOM association 사용).  
**오류 코드**: `XPE_OK`, `XPE_ERR_NETWORK_FAILED`, `XPE_ERR_IO_FAILED`, `XPE_ERR_INVALID_INPUT`

---

### 11.10 xpe_dicom_cfind_mwl

```c
XPE_API XpeErrorCode xpe_dicom_cfind_mwl(const char* queryJson,
                                           const char* remoteAeTitle,
                                           const char* remoteHost,
                                           uint16_t    remotePort,
                                           const char* localAeTitle,
                                           char*       resultsJsonOut,
                                           size_t      resultsBufLen);
```

**설명**: C-FIND를 사용하여 Modality Worklist SCP를 쿼리합니다. `queryJson`은 query key (Patient ID, Accession Number 등)를 인코드합니다. 결과는 `resultsJsonOut`에서 일치하는 worklist 항목의 JSON 배열로 반환됩니다.  
**SRS**: SRS-DICOM-031  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `XPE_OK`, `XPE_ERR_NETWORK_FAILED`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_BUFFER_TOO_SMALL`

---

## 12. gsvg.dll

anti-scatter grid 감지 및 virtual grid suppression을 제공합니다. 독립 모듈 — xpe_common 타입에 의존하지 않음.

### 12.1 gsvg_process

```c
GSVG_API GsvgErrorCode gsvg_process(uint16_t* pixels,
                                     uint32_t  width,
                                     uint32_t  height,
                                     const GsvgConfig* config);
```

**설명**: `config`의 매개변수를 사용하여 raw 16비트 픽셀 버퍼 `pixels` (width x height, row-major)에서 anti-scatter grid artifact를 인 플레이스로 억제합니다. `config->gridFrequency_lp_per_mm == 0`인 경우 grid 주파수를 자동 감지합니다.  
**SRS**: SRS-GSVG-001, SRS-GSVG-002, GSVG-SDD-001  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `GSVG_OK`, `GSVG_ERR_INVALID_INPUT`, `GSVG_ERR_GRID_NOT_DETECTED`, `GSVG_ERR_PROCESSING_FAILED`

---

### 12.2 gsvg_process_ex

```c
GSVG_API GsvgErrorCode gsvg_process_ex(uint16_t* pixels,
                                        uint32_t  width,
                                        uint32_t  height,
                                        const GsvgConfig* config,
                                        const GsvgImageMetadata* meta,
                                        char* diagnosticJsonOut,
                                        size_t diagnosticBufLen);
```

**설명**: 신체 부위 인식 조정을 위해 이미지 metadata를 추가로 수락하고 detected grid parameter, suppression quality metric의 JSON diagnostic 보고서를 `diagnosticJsonOut`에 쓰는 `gsvg_process`의 확장 variant입니다.  
**SRS**: SRS-GSVG-003, GSVG-SDD-001 §4.2  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `GSVG_OK`, `GSVG_ERR_INVALID_INPUT`, `GSVG_ERR_GRID_NOT_DETECTED`, `GSVG_ERR_PROCESSING_FAILED`, `GSVG_ERR_BUFFER_TOO_SMALL`

---

### 12.3 gsvg_version

```c
GSVG_API const char* gsvg_version(void);
```

**설명**: static null 종료 GSVG 모듈 버전 문자열에 대한 포인터를 반환합니다 (예: `"2.1.0"`). DLL 소유; 해제하지 말 것.  
**SRS**: SRS-GSVG-VER-001  
**스레드 안전성**: 스레드 안전.  
**오류 코드**: (NULL이 아님)

---

### 12.4 gsvg_error_string

```c
GSVG_API const char* gsvg_error_string(GsvgErrorCode code);
```

**설명**: `GsvgErrorCode`에 대한 static human-readable 영어 설명을 반환합니다. 알 수 없는 코드는 `"Unknown GSVG error"`를 반환합니다. DLL 소유.  
**SRS**: SRS-GSVG-ERR-001  
**스레드 안전성**: 스레드 안전.  
**오류 코드**: (NULL이 아님)

---

### 12.5 gsvg_detect_grid

```c
GSVG_API GsvgErrorCode gsvg_detect_grid(const uint16_t* pixels,
                                          uint32_t width,
                                          uint32_t height,
                                          float pixelPitch_mm,
                                          int32_t* freqOut_lp_per_mm,
                                          float*   angleOut_deg);
```

**설명**: 이미지에서 anti-scatter grid line 주파수 및 방향각을 감지하여 결과를 `*freqOut_lp_per_mm` 및 `*angleOut_deg`에 씁니다. 비파괴적 — 결과를 사용하여 `gsvg_process`에 대한 `GsvgConfig`를 채웁니다.  
**SRS**: SRS-GSVG-010, GSVG-SDD-001 §3.1  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `GSVG_OK`, `GSVG_ERR_INVALID_INPUT`, `GSVG_ERR_GRID_NOT_DETECTED`

---

### 12.6 gsvg_suppress_grid

```c
GSVG_API GsvgErrorCode gsvg_suppress_grid(uint16_t* pixels,
                                           uint32_t  width,
                                           uint32_t  height,
                                           int32_t   freq_lp_per_mm,
                                           float     angle_deg,
                                           float     suppressionStrength);
```

**설명**: 지정된 주파수 및 각도에서 알려진 grid를 억제합니다. grid parameter이 이미 알려진 시나리오 (예: detector metadata에서)에 대해 `gsvg_process`에서 분리됩니다.  
**SRS**: SRS-GSVG-011  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `GSVG_OK`, `GSVG_ERR_INVALID_INPUT`, `GSVG_ERR_PROCESSING_FAILED`

---

### 12.7 gsvg_virtual_grid

```c
GSVG_API GsvgErrorCode gsvg_virtual_grid(uint16_t* pixels,
                                          uint32_t  width,
                                          uint32_t  height,
                                          const GsvgImageMetadata* meta,
                                          const char* configJsonOrNull);
```

**설명**: scatter suppression 후 `pixels`에 인 플레이스로 virtual grid effect를 합성하여, physical anti-scatter grid 없이 획득된 이미지에 대한 인지된 대비를 개선합니다.  
**SRS**: SRS-GSVG-020, GSVG-SDD-001 §5  
**스레드 안전성**: 재진입 가능.  
**오류 코드**: `GSVG_OK`, `GSVG_ERR_INVALID_INPUT`, `GSVG_ERR_CONFIG_INVALID`, `GSVG_ERR_PROCESSING_FAILED`

---

### 12.8 gsvg_load_scatter_lut

```c
GSVG_API GsvgErrorCode gsvg_load_scatter_lut(const char* filePath);
```

**설명**: `filePath`에서 scatter 보정 lookup table을 module 내부 저장소에 로드합니다. LUT는 알려진 detector-grid 조합에 대한 suppression 품질을 개선합니다. 이전에 로드된 모든 LUT를 대체합니다.  
**SRS**: SRS-GSVG-030  
**스레드 안전성**: 스레드 안전하지 않음 — 처리 시작 전에 호출.  
**오류 코드**: `GSVG_OK`, `GSVG_ERR_LUT_LOAD_FAILED`, `GSVG_ERR_INVALID_INPUT`

---

## 13. 부록 A — 오류 코드 교차 참조

| 코드 | 기호 | 적용 가능 DLL |
|------|--------|-----------------|
| 0 | XPE_OK | 모두 |
| -1 | XPE_ERR_INVALID_INPUT | 모두 |
| -2 | XPE_ERR_OUT_OF_MEMORY | common, preprocess, ai, dicom |
| -3 | XPE_ERR_PROCESSING_FAILED | preprocess, enhance_basic, enhance_advanced, ai, display, dicom, gsvg |
| -4 | XPE_ERR_CONFIG_INVALID | common, preprocess, enhance_basic, enhance_advanced, display, dicom |
| -5 | XPE_ERR_CALIBRATION_EXPIRED | preprocess |
| -6 | XPE_ERR_NOT_INITIALIZED | common, ai |
| -7 | XPE_ERR_UNSUPPORTED_FORMAT | common, enhance_basic, dicom |
| -8 | XPE_ERR_BUFFER_TOO_SMALL | common, display, dicom |
| -9 | XPE_ERR_IO_FAILED | preprocess, dicom |
| -10 | XPE_ERR_NETWORK_FAILED | dicom |

GSVG 오류 코드는 별도이며 Section 3에서 정의됩니다.

---

## 14. 부록 B — DLL 의존성 그래프

```
gsvg.dll          (독립)
xpe_common.dll    (기본 — XPE 의존성 없음)
xpe_preprocess.dll  -> xpe_common.dll
xpe_enhance_basic.dll  -> xpe_common.dll
xpe_enhance_advanced.dll -> xpe_common.dll
xpe_ai.dll        -> xpe_common.dll (+ IPC를 통한 xpe_ai_worker.exe)
xpe_display.dll   -> xpe_common.dll
xpe_dicom.dll     -> xpe_common.dll
```

로드 순서: `xpe_common.dll`은 다른 XPE DLL 이전에 로드되어야 합니다. `xpe_ai_worker.exe`는 `xpe_ai_init`에 의해 요청 시 시작됩니다. `gsvg.dll`은 언제든지 독립적으로 로드될 수 있습니다.
