# X-ray FPD 공통 기반 모듈 (xpe_common.dll) 문서 패키지

**Module**: `xpe_common.dll` (Layer 0, 기반 인프라)  
**Safety Classification**: IEC 62304 Class B  
**Document Package Version**: 1.0.0  
**Date**: 2026-04-14  
**Language**: 한국어 (Technical Terms: English)  
**Normative Authority**: SPEC-XPE-MASTER v2.0.0 §3.5

---

## 목차

1. [문서 패키지 개요](#문서-패키지-개요)
2. [역할별 읽기 경로](#역할별-읽기-경로)
3. [문서 간 의존성](#문서-간-의존성)
4. [xpe_common.dll 아키텍처](#xpe_commondll-아키텍처)
5. [XpeImage 구조체 설계](#xpeimage-구조체-설계)
6. [메모리 풀 구조 및 생명주기](#메모리-풀-구조-및-생명주기)
7. [AED (비동기 이벤트 디스패처) 흐름](#aed-비동기-이벤트-디스패처-흐름)
8. [P/Invoke 브리지 (C# 상호작용)](#pinvoke-브리지-c-상호작용)
9. [7개 소프트웨어 단위 (SWU) 요약](#7개-소프트웨어-단위-swu-요약)
10. [XPE_FLAG_* 비트마스크 참조](#xpe_flag-비트마스크-참조)
11. [Pack=8 정렬 검증](#pack8-정렬-검증)
12. [성능 및 메모리 예산](#성능-및-메모리-예산)
13. [주요 위험 및 제어](#주요-위험-및-제어)
14. [빠른 시작](#빠른-시작)

---

## 문서 패키지 개요

`xpe_common.dll`은 X-ray FPD (평판 검출기) 이미지 처리 엔진의 **Layer 0 기반 인프라** DLL이다. 모든 Layer 1 DLL (`xpe_preprocess`, `xpe_enhance_basic`, `xpe_ai` 등)이 이 DLL의 공유 서비스를 사용한다.

### 문서 생태계

```
xpe_common.dll 문서 패키지 (IEC 62304 Class B)
│
├─ xpe-common-prd.md (PRD)
│  └─ 7개 SWU의 원본 요구사항 및 설계
│     · SWU-5.1 MemoryPool
│     · SWU-5.2 TypeDefinitions (Pack=8)
│     · SWU-5.3 ErrorHandler
│     · SWU-5.4 NotificationSystem (AED)
│     · SWU-5.5 JsonConfig
│     · SWU-5.6 ParameterValidator
│     · SWU-5.7 PipelineOrchestrator (C# 브리지)
│
├─ SRS-COMMON-001.md (소프트웨어 요구사항 명세)
│  └─ 42개 기능/안전/성능 요구사항 (FR-CMN-*, SAF-CMN-*, PERF-CMN-*)
│
├─ SAD-COMMON-001.md (소프트웨어 아키텍처 문서)
│  └─ 7개 SWU 설계, 인터페이스, 데이터 흐름
│
├─ SHA-COMMON-001.md (소프트웨어 위험 분석)
│  └─ 7개 위험 식별 및 제어 (HAZ-CMN-001~007)
│
├─ RTM-COMMON-001.md (요구사항 추적 행렬)
│  └─ SRS → SWU → 테스트 → 위험 양방향 추적
│
└─ README.md (이 파일)
   └─ 아키텍처 개요, 빠른 참조, 다이어그램
```

### 문서 선택 기준

| 역할 | 필독 | 참고 | 선택 |
|------|------|------|------|
| **개발자** | PRD, SRS, SAD | RTM | SHA |
| **테스트** | SRS, SAD, RTM | SHA | PRD |
| **안전/규제** | SRS, SHA, RTM | SAD | PRD |
| **아키텍트** | PRD, SAD | SRS | SHA, RTM |
| **프로젝트 관리** | RTM | — | SRS, SAD, SHA |

---

## 역할별 읽기 경로

### 경로 1: 소프트웨어 개발자

```
1. README.md (이 문서) — 아키텍처 개요 (15분)
2. PRD (xpe-common-prd.md) — 전체 설계 (45분)
3. SAD (SAD-COMMON-001.md) — 구현 상세 (60분)
4. SRS (SRS-COMMON-001.md) — 요구사항 검증 (30분)
└─ 개발 시작
```

**목표**: xpe_common DLL 구현, 다른 Layer 1 DLL 통합

---

### 경로 2: QA / 테스트 엔지니어

```
1. README.md (이 문서) — 아키텍처 개요
2. SRS (SRS-COMMON-001.md) — 요구사항 이해
3. RTM (RTM-COMMON-001.md) — 테스트 케이스 맵핑 (60분)
4. SAD (SAD-COMMON-001.md) — 구현 상세 (참고)
└─ 테스트 케이스 작성 및 실행
```

**목표**: 40개 테스트 케이스 작성, 커버리지 100% 달성

---

### 경로 3: 안전/규제 담당자

```
1. SRS (SRS-COMMON-001.md) — 요구사항 검증
2. SHA (SHA-COMMON-001.md) — 위험 분석 및 제어 (75분)
3. RTM (RTM-COMMON-001.md) — 양방향 추적성 확인
4. PRD, SAD (참고) — 구현 확인
└─ IEC 62304 compliance 평가
```

**목표**: 7개 위험 제어 검증, Class B 분류 확인

---

### 경로 4: 소프트웨어 아키텍트

```
1. README.md (이 문서) — Layer 0 역할 이해
2. PRD (xpe-common-prd.md) — 전체 시스템 설계
3. SAD (SAD-COMMON-001.md) — 컴포넌트 상호작용
4. RTM (RTM-COMMON-001.md) — 추적성 검증
└─ 아키텍처 리뷰 및 개선 제안
```

**목표**: 다른 Layer 1 DLL과의 통합 지점 확인, 반복 개선

---

## 문서 간 의존성

```
xpe-common-prd.md (원본)
    ↓ 파생 (Derives)
    ├─ SRS-COMMON-001.md (요구사항 명세)
    │   ├─ 42개 요구사항 추출
    │   └─ SAD 입력
    │
    ├─ SAD-COMMON-001.md (아키텍처 설계)
    │   ├─ 7개 SWU로 분해
    │   ├─ 인터페이스 정의
    │   └─ SHA 입력
    │
    └─ SHA-COMMON-001.md (위험 분석)
        ├─ 7개 위험 식별
        ├─ 제어 메커니즘 설계
        └─ RTM 입력

RTM-COMMON-001.md (추적성)
    ├─ SRS ↔ SWU ↔ 테스트 양방향 맵핑
    ├─ 위험 제어 검증
    └─ 요구사항 커버리지 확인
```

---

## xpe_common.dll 아키텍처

### Layer 0 위치

```
┌─────────────────────────────────────────────────────┐
│ Layer 2: ImageProcTest.exe (C# WPF 오케스트레이터) │
│          ↑ P/Invoke (Pack=8 structs)                │
└─────────────┬───────────────────────────────────────┘
              │ 호출
┌─────────────▼───────────────────────────────────────┐
│ Layer 1: XPE Processing Modules                    │
│  • xpe_preprocess.dll   (캘리브레이션 전처리)       │
│  • xpe_enhance_basic.dll   (기본 향상)              │
│  • xpe_enhance_advanced.dll (고급 향상)             │
│  • xpe_ai.dll              (AI 추론)                │
│  • xpe_display.dll         (디스플레이 변환)        │
│  • xpe_dicom.dll           (DICOM I/O)             │
│  ↑ 링크 의존성 (모두)                               │
└─────────────┬───────────────────────────────────────┘
              │ 의존
┌─────────────▼───────────────────────────────────────┐
│ Layer 0: xpe_common.dll (공유 기반 인프라)          │
│                                                     │
│  ┌────────────────────────────────────────────┐   │
│  │ SWU-5.1: MemoryPool                       │   │
│  │  • Pre-allocated slabs (226.4 MB)         │   │
│  │  • Reference counting                     │   │
│  │  • Zero-copy image transfer               │   │
│  │                                            │   │
│  │ SWU-5.2: TypeDefinitions                  │   │
│  │  • XpeImage, XpeImageMetadata             │   │
│  │  • Pack=8 정렬 (P/Invoke 호환)            │   │
│  │  • XPE_FLAG_* 비트마스크                  │   │
│  │                                            │   │
│  │ SWU-5.3: ErrorHandler                     │   │
│  │  • 50+ 에러 코드                          │   │
│  │  • 스레드-로컬 컨텍스트                  │   │
│  │  • JSON 진단 정보                         │   │
│  │                                            │   │
│  │ SWU-5.4: NotificationSystem (AED)         │   │
│  │  • 비동기 이벤트 디스패처                │   │
│  │  • 256-deep 원형 버퍼                     │   │
│  │  • 콜백 기반 알림 전달                   │   │
│  │                                            │   │
│  │ SWU-5.5: JsonConfig                       │   │
│  │  • 설정 로드/검증                        │   │
│  │  • 핫-리로드 지원                        │   │
│  │  • 기본값 폴백                           │   │
│  │                                            │   │
│  │ SWU-5.6: ParameterValidator               │   │
│  │  • 범위 검증 (kVp, mAs, sdd, temp, ...)  │   │
│  │  • 매개변수 조합 검증                    │   │
│  │                                            │   │
│  │ SWU-5.7: PipelineOrchestrator             │   │
│  │  • C# P/Invoke 브리지                     │   │
│  │  • C ABI 함수 노출                        │   │
│  │                                            │   │
│  └────────────────────────────────────────────┘   │
│                                                     │
│  의존성: 없음 (다른 XPE DLL 미의존)               │
│                                                     │
└─────────────────────────────────────────────────────┘
```

### 안티-스파게티 원칙

```
정상 (단방향):
  xpe_preprocess.dll ──┐
  xpe_enhance_basic.dll ├─ xpe_common.dll
  xpe_ai.dll ──────────┘

비정상 (횡단 의존성):
  xpe_preprocess.dll ──✗── xpe_enhance_basic.dll  (금지)
  xpe_ai.dll ────────┬────── xpe_display.dll (금지)
                     │
                     └─ 모든 통신은 xpe_common.dll 경유
```

---

## XpeImage 구조체 설계

### 전체 레이아웃 (240 바이트, Pack=8)

```c
typedef struct {
    // 오프셋   크기   필드 이름           설명
    // ─────   ────   ──────────────────  ──────────────────────
    //    0      4    uint32_t width      검출기 가로 픽셀 [512~4096]
    //    4      4    uint32_t height     검출기 세로 픽셀 [512~4096]
    //    8      4    uint32_t bitsAllocated  {16, 32}
    //   12      4    uint32_t bitsStored {14, 16, 32}
    //   16      4    PixelFormat format  {0=UINT16, 1=FLOAT32}
    //   20      8    void* data          비소유 포인터
    //   28      8    size_t dataSize     바이트 단위 크기
    //   36    192    XpeImageMetadata    DICOM + 센서 메타데이터
    //  228      4    uint32_t xpeFlags   처리 단계 비트마스크
    //  232      8    uint32_t padding    정렬용 패딩
    // ─────   ────
    //  240 = 8의 배수 (Pack=8 호환)
} XpeImage;

// 정렬 검증
static_assert(sizeof(XpeImage) == 240, "XpeImage must be 240 bytes");
static_assert(offsetof(XpeImage, metadata) == 36, "metadata offset");
```

### 바이트 맵 시각화

```
0    4    8   12   16   20   28            36                 228  232
|width|hgt |alloc|stor|fmt |data_ptr|size|metadata          |flag|pad |
|4    |4   |4   |4   |4  |8   |8   |192              |4   |8  |
|──────────────────────────────────────────────────────────────────────|
 0                                                                    240
 
<────── 정렬 경계 ──────────────────────────────────────────────────>
8바이트 경계 (Pack=8 규칙)
```

### XpeImageMetadata 구조체 (192 바이트)

```c
struct XpeImageMetadata {
    char bodyPart[64];              // DICOM (0018,0015) 신체 부위
    float kVp;                      // 튜브 전압 (kV) [40~150]
    float mAs;                      // 튜브 전류량 (mAs) [0.1~500]
    float sdd;                      // 소스-검출기 거리 (mm) [400~1500]
    float pixelSpacingMm;           // 검출기 픽셀 피치 (mm) [0.1~0.5]
    uint64_t acquisitionTime;       // Unix 타임스탬프 (ms)
    char detectorId[32];            // 검출기 시리얼 번호
    float temperature;              // 검출기 온도 (°C) [-10~85]
    char reserved[96];              // 향후 확장용
};  // 총 192 바이트
```

---

## 메모리 풀 구조 및 생명주기

### 초기화 (시작 시, 일회)

```
xpe_mempool_init()
  ├─ float32 슬롯 4개 할당
  │  ├─ Slot 0: 37.7 MB (3072×3072×4)
  │  ├─ Slot 1: 37.7 MB
  │  ├─ Slot 2: 37.7 MB
  │  └─ Slot 3: 37.7 MB
  │
  ├─ uint16 슬롯 4개 할당
  │  ├─ Slot 0: 18.9 MB (3072×3072×2)
  │  ├─ Slot 1: 18.9 MB
  │  ├─ Slot 2: 18.9 MB
  │  └─ Slot 3: 18.9 MB
  │
  └─ 총 메모리: 226.4 MB (프로세스 시작 시 고정 할당)
```

### 할당 수명주기

```
Frame 1 도착
  ├─ xpe_mempool_alloc(3072, 3072, UINT16)
  │  └─ uint16 Slot 0 반환 (refcount=1)
  │
  ├─ Stage 1: Slot 0 사용 (refcount=1)
  ├─ Stage 2: Slot 0 사용 (refcount=1)
  │
  ├─ xpe_mempool_free(Slot 0)
  │  └─ refcount=0, 슬롯 가능 상태
  │
Frame 2 도착
  ├─ xpe_mempool_alloc(3072, 3072, UINT16)
  │  └─ uint16 Slot 0 재할당 (refcount=1)
  │     [제로카피: Slot 0의 메모리 주소는 변하지 않음]
```

### 메모리 할당 정책

| 항목 | 크기 | 용도 |
|------|------|------|
| float32 슬롯 | 4 × 37.7 MB | 보정된 이미지, 임시 작업 |
| uint16 슬롯 | 4 × 18.9 MB | 원본 ADC, 중간 단계 |
| 예약 | — | 버퍼 관리 오버헤드 |
| **총 피크** | **226.4 MB** | 프로세스 수명 내 |

### 참조 카운팅

```
Slot 상태 전이:

┌──────────────┐
│ 가능 (ref=0) │
└──────┬───────┘
       │ alloc()
       v
┌──────────────┐
│ 사용 중(ref>0)│
└──────┬───────┘
       │ free()
       v
┌──────────────┐
│ 가능 (ref=0) │
└──────────────┘
```

---

## AED (비동기 이벤트 디스패처) 흐름

### 큐 기반 아키텍처

```
생산자 (모든 XPE DLL)    소비자 (AED 스레드)
    │                          │
    ├─ xpe_aed_emit_alert()   │
    │  (큐에 추가, 논블로킹)   │
    │  ├─ lock queue          │
    │  ├─ add to buffer       │
    │  └─ signal()            ├─ poll queue (10ms 간격)
    │     └─ 스레드 깨우기     ├─ 알림 역직렬화
    │                         ├─ for each callback
    v                         │    callback(json, userdata)
┌──────────────┐            └─ sleep(10ms)
│ Alert Queue  │
│ (원형 버퍼)  │
│ 용량: 256    │
└──────────────┘
    ↑        ↓
write_ptr read_ptr
```

### 알림 발송 시퀀스

```c
1. 생산자 (DLL)
   ├─ xpe_aed_emit_alert(XPE_ALERT_WARNING, "온도 드리프트 감지")
   ├─ 내부: queue[write_index] = {type, timestamp, msg}
   ├─ 내부: write_index++ (범위: 0~255)
   └─ 반환: XPE_OK (즉시)

2. AED 스레드 (백그라운드)
   ├─ 10ms마다 조사
   ├─ queue[read_index] 읽기
   ├─ read_index++ (범위: 0~255)
   ├─ for (callback in callbacks)
   │   ├─ json = serialize(alert)
   │   └─ callback(json, userdata)  ← C# 콜백
   └─ 반복
```

### 큐 오버플로우 처리

```
정상 상황:
  read_index=50, write_index=100, count=50 (< 256)

오버플로우:
  read_index=200, write_index=255, count=256 (= 256)
  
  새 알림 도착:
  ├─ read_index++ (FIFO 대체)
  ├─ queue[255] = new_alert
  ├─ count remains 256
  └─ discarded_count++
```

### 알림 우선도

```
AlertType 우선도:
  INFO              (1) 최저
  WARNING           (2)
  ERROR             (3)
  CALIBRATION_NEEDED (4)
  AI_UNAVAILABLE    (5) 최고
```

---

## P/Invoke 브리지 (C# 상호작용)

### C++ 선언 (xpe_common.h)

```c
#ifdef _WIN32
    #define XPE_API extern "C" __declspec(dllexport)
#else
    #define XPE_API extern "C"
#endif

// 메모리 풀
XPE_API XpeError xpe_mempool_alloc(uint32_t w, uint32_t h, PixelFormat fmt, void** ptr);
XPE_API XpeError xpe_mempool_free(void* ptr);

// 설정
XPE_API XpeError xpe_config_load(const char* path);
XPE_API const char* xpe_config_get_string(const char* key);

// 에러
XPE_API XpeError xpe_get_last_error(void);
XPE_API const XpeErrorDetail* xpe_get_last_error_detail(void);

// AED
XPE_API XpeError xpe_aed_register_callback(XpeAlertCallback cb, void* userdata);
XPE_API XpeError xpe_aed_emit_alert(AlertType type, const char* msg);
```

### C# 선언 (P/Invoke)

```csharp
using System.Runtime.InteropServices;

public static class XpeCommon {
    private const string DLL = "xpe_common.dll";
    
    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern int xpe_mempool_alloc(uint width, uint height, int format);
    
    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern int xpe_config_load([MarshalAs(UnmanagedType.LPStr)] string path);
    
    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern int xpe_get_last_error();
    
    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public delegate void AlertCallback([MarshalAs(UnmanagedType.LPStr)] string json, IntPtr userdata);
    
    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern int xpe_aed_register_callback(AlertCallback cb, IntPtr userdata);
}

// Pack=8 struct 정의
[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct XpeImage {
    public uint Width;
    public uint Height;
    public uint BitsAllocated;
    public uint BitsStored;
    public int Format;
    public IntPtr Data;
    public ulong DataSize;
    [MarshalAs(UnmanagedType.Struct)]
    public XpeImageMetadata Metadata;
    public uint XpeFlags;
    public uint Padding;
}
```

### 호출 규약 (Calling Convention)

```
extern "C" ──────┐
                 ├─→ Cdecl (C Calling Convention)
CallingConvention: Cdecl ───┤
                         ├─→ 스택 정리: 호출자 책임
                         ├─→ 반환: EAX (32-bit) or RAX (64-bit)
                         └─→ 매개변수: 우->좌 순서로 스택 푸시
```

---

## 7개 소프트웨어 단위 (SWU) 요약

| SWU ID | 이름 | 책임 | 주요 함수 | API 수 |
|--------|------|------|---------|--------|
| **5.1** | **MemoryPool** | 메모리 슬롯 할당 (226.4 MB) | `xpe_mempool_alloc`, `free`, `get_stats` | 3 |
| **5.2** | **TypeDefinitions** | 공유 타입 (Pack=8) | `XpeImage`, `XpeImageMetadata`, `XpeRect` | 40+ |
| **5.3** | **ErrorHandler** | 에러 처리 (스레드-로컬) | `xpe_get_last_error`, `get_detail`, `error_to_string` | 4 |
| **5.4** | **NotificationSystem** | 비동기 알림 (AED) | `xpe_aed_emit_alert`, `register_callback`, `get_stats` | 5 |
| **5.5** | **JsonConfig** | 설정 관리 (핫-리로드) | `xpe_config_load`, `get_*`, `set_*`, `reload` | 6 |
| **5.6** | **ParameterValidator** | 매개변수 범위 검증 | `xpe_validate_image_params`, `validate_pipeline_config` | 2 |
| **5.7** | **PipelineOrchestrator** | C# P/Invoke 브리지 | extern "C" declarations | — |

---

## XPE_FLAG_* 비트마스크 참조

### 플래그 정의 (비트마스크)

| 비트 | 플래그 | 값 | 16진수 | 설정 단계 | 의미 |
|-----|--------|-----|--------|----------|------|
| 0 | XPE_FLAG_CALIBRATED | 0x0001 | 0x0001 | Stage 1a (Gain) | 캘리브레이션 완료 |
| 1 | XPE_FLAG_GHOST_CORRECTED | 0x0002 | 0x0002 | Stage 1a (Ghost) | 래그 보정 적용 |
| 2 | XPE_FLAG_DEFECT_CORRECTED | 0x0004 | 0x0004 | Stage 1a (Defect) | 불량 픽셀 보정 |
| 3 | XPE_FLAG_LOG_TRANSFORMED | 0x0008 | 0x0008 | Stage 1b (Log) | 로그 도메인 |
| 4 | XPE_FLAG_DENOISED | 0x0010 | 0x0010 | Stage 1b (Noise) | 노이즈 제거 |
| 5 | XPE_FLAG_EDGE_ENHANCED | 0x0020 | 0x0020 | Stage 1b (Edge) | 엣지 강조 |
| 6 | XPE_FLAG_COLLIMATED | 0x0040 | 0x0040 | Stage 2 | 콜리메이션 감지 |
| 7 | XPE_FLAG_EI_COMPUTED | 0x0080 | 0x0080 | Stage 1b/2 | 노출 지수 계산 |
| 8 | XPE_FLAG_AI_PROCESSED | 0x0100 | 0x0100 | Stage 3 | AI 처리 완료 |
| 9 | XPE_FLAG_GSDF_APPLIED | 0x0200 | 0x0200 | Stage 1b | GSDF 표시 적용 |

### 사용 예시

```c
// 플래그 설정 (비트 OR)
image->xpeFlags |= XPE_FLAG_CALIBRATED;
image->xpeFlags |= XPE_FLAG_LOG_TRANSFORMED;

// 플래그 확인 (비트 AND)
if (image->xpeFlags & XPE_FLAG_CALIBRATED) {
    // 캘리브레이션된 데이터
}

// 여러 플래그 확인
if ((image->xpeFlags & (XPE_FLAG_CALIBRATED | XPE_FLAG_DENOISED)) 
    == (XPE_FLAG_CALIBRATED | XPE_FLAG_DENOISED)) {
    // 캘리브레이션 + 노이즈 제거됨
}

// 우회된 단계는 플래그를 설정하지 않음
if (skip_log_transform) {
    // 로그 변환 우회
    // image->xpeFlags |= XPE_FLAG_LOG_TRANSFORMED; ← 하지 않음
}
```

### 플래그 유효성 검증

```c
// 다운스트림: 플래그 의존성 확인
XpeError xpe_enhance_clahe(XpeImage* image) {
    // CLAHE는 로그 도메인 필요
    if (!(image->xpeFlags & XPE_FLAG_LOG_TRANSFORMED)) {
        return XPE_ERR_DEPENDENCY_NOT_MET;  // 로그 변환 먼저!
    }
    // ... CLAHE 적용
}
```

---

## Pack=8 정렬 검증

### 컴파일 타임 검증 (권장)

```c
// xpe_common.h
#pragma pack(8)

struct XpeImage {
    // ... 필드들 ...
};

// 정렬 검증 (컴파일 타임)
static_assert(sizeof(XpeImage) == 240, "XpeImage size");
static_assert(offsetof(XpeImage, width) == 0, "width offset");
static_assert(offsetof(XpeImage, metadata) == 36, "metadata offset");
static_assert(offsetof(XpeImage, xpeFlags) == 228, "xpeFlags offset");

#pragma pack()
```

### 런타임 검증 (옵션)

```csharp
// C# 측 검증
[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct XpeImage {
    // ... 필드 정의 ...
    
    static XpeImage() {
        int expectedSize = 240;
        int actualSize = Marshal.SizeOf(typeof(XpeImage));
        if (actualSize != expectedSize) {
            throw new Exception($"XpeImage size mismatch: {actualSize} != {expectedSize}");
        }
    }
}
```

### 빌드 검증

```cmake
# CMakeLists.txt
add_compile_options(-Wpadded)  # 패딩 경고 활성화
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Wall")  # MSVC

# 빌드 실패 시 정렬 오류 감지
if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(-Werror=padded)  # GCC/Clang: 경고를 오류로
endif()
```

---

## 성능 및 메모리 예산

### 시간 할당 (ms, 프레임당)

| 작업 | 예산 | 예상 | 여유 | 상태 |
|------|------|------|------|------|
| 메모리 할당 | 1 | 0.5 | 50% | ✓ OK |
| 설정 로드 | 20 | 15 | 25% | ✓ OK |
| 매개변수 검증 | 5 | 3 | 40% | ✓ OK |
| AED 발송 | 1 | 0.3 | 70% | ✓ OK |
| 에러 조회 | 0.1 | 0.05 | 50% | ✓ OK |

### 메모리 할당 (MB, 피크)

| 구성요소 | 크기 | 할당 시점 | 해제 시점 | 상태 |
|---------|------|---------|---------|------|
| float32 풀 (4 슬롯) | 150.8 | 시작 | 프로세스 종료 | ✓ 고정 |
| uint16 풀 (4 슬롯) | 75.6 | 시작 | 프로세스 종료 | ✓ 고정 |
| 설정 메모리 | < 1 | 로드 | 언로드 | ✓ 제한 |
| 에러 컨텍스트 (TLS) | ~1 / 스레드 | 스레드 생성 | 스레드 종료 | ✓ 스케일 가능 |
| AED 큐 | ~2 | 시작 | 프로세스 종료 | ✓ 고정 |
| **총 피크** | **~226.4** | 시작 | 프로세스 종료 | ✓ OK |

---

## 주요 위험 및 제어

### 7개 식별된 위험

| 위험 ID | 제목 | 심각도 | 위험도 | 제어 방법 |
|--------|------|--------|--------|----------|
| HAZ-001 | Struct 정렬 불일치 | 높음 | **MEDIUM** | static_assert + 런타임 검증 |
| HAZ-002 | 메모리 풀 이중 해제 | 매우높음 | **MEDIUM** | 포인터 유효성 검증 |
| HAZ-003 | 메모리 풀 고갈 | 중간 | **MEDIUM** | 흐름 제어 (동기식 파이프라인) |
| HAZ-004 | JSON 설정 손상 | 중간 | **LOW** | 스키마 검증 + 기본값 폴백 |
| HAZ-005 | XPE_FLAG 오류 | 높음 | **MEDIUM** | 플래그 설정 규칙 + 의존성 검증 |
| HAZ-006 | AED 큐 오버플로우 | 중간 | **LOW** | FIFO 대체 + 우선도 기반 선택 |
| HAZ-007 | C# 콜백 크래시 | 높음 | **MEDIUM** | 예외 처리 래퍼 (SEH/try-catch) |

### 위험 제어 검증

```
모든 위험 제어: 3계층 모델
  ├─ 1차 (주): 설계/구현 수준 제어
  ├─ 2차 (부): 동적 런타임 제어
  └─ 3차 (보): 코드 리뷰 + 테스트
```

---

## 빠른 시작

### 1. 개발자: xpe_common.dll 구현

#### 단계 1: 타입 정의 구현 (SWU-5.2)
```bash
$ vi xpe_common.h
#pragma pack(8)
struct XpeImage { ... };
static_assert(sizeof(XpeImage) == 240);  // ← 필수
#pragma pack()
```

#### 단계 2: 메모리 풀 구현 (SWU-5.1)
```bash
$ vi xpe_mempool.cpp
- 8개 슬롯 사전 할당 (226.4 MB)
- 참조 카운팅 로직
- mutex 동기화
```

#### 단계 3: 에러 처리 구현 (SWU-5.3)
```bash
$ vi xpe_error.cpp
- 스레드-로컬 에러 컨텍스트 (__thread)
- 50+ 에러 코드
- 에러 문자열 변환 테이블
```

#### 단계 4: 기타 모듈 구현 (SWU-5.4~7)
```bash
$ vi xpe_aed.cpp xpe_config.cpp xpe_params.cpp
```

#### 단계 5: P/Invoke 브리지 (SWU-5.7)
```csharp
$ vi ImageProcTest/XpeCommon.cs
[DllImport("xpe_common.dll", CallingConvention = CallingConvention.Cdecl)]
```

---

### 2. QA 엔지니어: 테스트 케이스 작성

#### 단계 1: 단위 테스트 (UT-*)
```bash
$ mkdir tests/unit
$ vi tests/unit/test_mempool.cpp
$ vi tests/unit/test_types.cpp
$ vi tests/unit/test_error.cpp
```

#### 단계 2: 통합 테스트 (IT-*)
```bash
$ mkdir tests/integration
$ vi tests/integration/test_mempool_multithread.cpp
$ vi tests/integration/test_pinvoke.cpp  # P/Invoke 테스트
```

#### 단계 3: 성능 벤치마크 (PERF-*)
```bash
$ mkdir tests/benchmark
$ vi tests/benchmark/bench_mempool_alloc.cpp
$ vi tests/benchmark/bench_config_load.cpp
```

---

### 3. 아키텍트: 통합 점검

#### 질문 1: 다른 Layer 1 DLL과의 호환성?
```
xpe_preprocess.dll:
  ├─ XpeImage 사용 (Pack=8 호환) ✓
  ├─ xpe_mempool_alloc 호출 ✓
  ├─ xpe_get_last_error 호출 ✓
  └─ xpe_aed_emit_alert 호출 ✓

→ 모든 호출이 Layer 0에만 의존 ✓
```

#### 질문 2: 메모리 누수 위험?
```
프로세스 시작:
  └─ xpe_mempool_init() → 226.4 MB 할당 (고정)

프로세스 수명:
  ├─ Frame 할당/해제 반복
  └─ 메모리 누수 없음 (슬롯 재사용)

프로세스 종료:
  └─ 모든 할당 해제 (OS가 관리)

→ 메모리 누수 위험 최소 ✓
```

---

### 4. 프로젝트 관리자: 추적성 검증

#### RTM 체크리스트

- [ ] SRS 42개 요구사항 모두 구현 (커버리지 100%)
- [ ] SAD 7개 SWU 모두 설계 (SWU-5.1~5.7)
- [ ] SHA 7개 위험 모두 제어 (HAZ-CMN-001~007)
- [ ] 40개 테스트 케이스 모두 통과
- [ ] 양방향 추적성 검증 완료
- [ ] 성능 요구사항 달성 (모든 PERF-CMN-* 합격)
- [ ] 메모리 요구사항 달성 (≤ 226.4 MB)

---

## 참고 문서

| 문서 | 경로 | 용도 |
|------|------|------|
| **PRD** | xpe-common-prd.md | 설계 상세 |
| **SRS** | SRS-COMMON-001.md | 요구사항 명세 |
| **SAD** | SAD-COMMON-001.md | 아키텍처 설계 |
| **SHA** | SHA-COMMON-001.md | 위험 분석 |
| **RTM** | RTM-COMMON-001.md | 추적성 행렬 |
| **SPEC** | ../../.moai/specs/SPEC-XPE-MASTER.md | 프로젝트 마스터 스펙 |

---

## 추가 자료

### 상위 문서
- X-ray FPD 이미지 처리 엔진 마스터 스펙: SPEC-XPE-MASTER v2.0.0
- 캘리브레이션 모듈 README: docs/calibration/README.md

### 관련 모듈
- xpe_preprocess.dll (Layer 1) — 캘리브레이션 전처리
- xpe_enhance_basic.dll (Layer 1) — 기본 향상
- ImageProcTest.exe (Layer 2) — C# WPF 오케스트레이터

---

**xpe_common.dll 문서 패키지 v1.0.0 완료**

*이 README는 모든 문서 패키지의 출입문입니다. 역할에 따라 위의 "역할별 읽기 경로"를 따라 필요한 문서를 읽으세요.*
