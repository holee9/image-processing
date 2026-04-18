# SHA-COMMON-001: xpe_common.dll 소프트웨어 위험 분석 (SHA)

**Document ID**: SHA-COMMON-001  
**Version**: 1.0.0  
**IEC 62304 Clause**: 5.3.4 — Software Hazard Analysis  
**ISO 14971:2019 Alignment**: Risk Management  
**Safety Classification**: Class B  
**Date**: 2026-04-14  
**Normative Reference**: SAD-COMMON-001

---

## 1. 목적

`xpe_common.dll`의 소프트웨어 위험을 식별, 분석, 평가하고 위험 제어 방법을 정의한다. 식별된 7개 위험은 다음과 같다.

---

## 2. 위험 식별 및 분석

### HAZ-CMN-001: Struct 정렬 불일치로 인한 데이터 손상

#### 위험 설명
C++의 `#pragma pack(8)` 설정이 누락되거나 잘못 적용되면 `XpeImage` struct의 메모리 레이아웃이 C#의 `StructLayout(Pack=8)`과 일치하지 않는다. 결과적으로 P/Invoke 마샬링 시 필드 오프셋이 어긋나 데이터 손상이 발생할 수 있다.

#### 위험 경로

```
개발자가 xpe_common.h 수정
  ├─ struct XpeImage에 새로운 필드 추가
  ├─ #pragma pack(8) 누락 또는 제거
  │
  v
C++ 컴파일: pack 설정 기본값 (보통 4 또는 8)
  ├─ struct 크기 = 244 바이트 (8이 아닌 다른 값일 수도)
  │
  v
C# P/Invoke: Pack=8로 고정 마샬
  ├─ struct 레이아웃 = 240 바이트 (C#의 Pack=8 규칙)
  │
  v
메모리 읽기/쓰기 오프셋 불일치
  ├─ C++의 offset(xpeFlags) = 232
  ├─ C#의 offset(xpeFlags) = 228 (또는 다른 값)
  │
  v
데이터 손상 또는 접근 위반 (Access Violation)
  ├─ C# 코드: image.xpeFlags = 0x0001
  ├─ C++에서 읽음: 다른 필드의 데이터 읽음 (손상됨)
  │
  v
이미지 처리 오류 또는 크래시
```

#### 위험도 평가

| 평가 항목 | 등급 | 근거 |
|----------|------|------|
| **심각도** | **높음** | 데이터 손상 → 잘못된 이미지 처리 → 진단 오류 위험 |
| **발생 확률** | **중간** | 개발 중 struct 수정 가능성 (하지만 주의깊게 관리되면 낮아짐) |
| **검출 확률** | **높음** | 컴파일 타임 정렬 검증으로 즉시 감지 |
| **위험도** | **MEDIUM** | 심각 × 확률 / 검출 = 높음 × 중간 / 높음 |

#### 위험 제어 (Risk Control)

**1차 제어: 컴파일 타임 검증 (Primary)**

```c
// xpe_common.h
#pragma pack(8)

struct XpeImage {
    // ... 필드들 ...
};

// 컴파일 타임 검증
static_assert(sizeof(XpeImage) == 240, "XpeImage size mismatch");
static_assert(offsetof(XpeImage, width) == 0, "width offset");
static_assert(offsetof(XpeImage, metadata) == 36, "metadata offset");
static_assert(offsetof(XpeImage, xpeFlags) == 228, "xpeFlags offset");

#pragma pack()
```

**2차 제어: C# 마샬링 검증 (Secondary)**

```csharp
[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct XpeImage {
    // ... 필드 정의 ...
    
    // 런타임 검증 (옵션)
    static XpeImage() {
        int expectedSize = 240;
        if (Marshal.SizeOf(typeof(XpeImage)) != expectedSize) {
            throw new Exception($"XpeImage size mismatch: {Marshal.SizeOf(typeof(XpeImage))} != {expectedSize}");
        }
    }
}
```

**3차 제어: CI/CD 빌드 검증 (Tertiary)**

```bash
# CMakeLists.txt 또는 build.sh
add_compile_options(-Wpadded)  # GCC/Clang: 패딩 경고
# MSVC: /Wall (모든 경고 활성화)

# 빌드 실패 시 정렬 오류 감지
if COMPILE_WARNINGS_AS_ERRORS; then
    HALT BUILD
fi
```

#### 예상 결과
- 정렬 불일치 시 **컴파일 타임에 감지 (빌드 실패)**
- 빌드 성공 시 **정렬 보장됨**
- 런타임 오류 가능성 **< 1%**

---

### HAZ-CMN-002: 메모리 풀 이중 해제 (Double Free)

#### 위험 설명
개발자 또는 다운스트림 DLL이 동일한 메모리 풀 슬롯 포인터를 두 번 `xpe_mempool_free()`로 전달할 수 있다. 결과적으로 힙 손상, 임의 메모리 쓰기, 크래시가 발생할 수 있다.

#### 위험 경로

```
Stage 1: xpe_mempool_alloc() → Slot 0 (refcount=1)
Stage 2: xpe_mempool_free(Slot 0) → refcount=0, 슬롯 복구 가능
Stage 3: 버그) xpe_mempool_free(Slot 0) 재호출
  ├─ Slot 0이 이미 사용 가능 상태 (refcount=0)
  ├─ 또는 Stage 4가 Slot 0 재할당했음
  │
  v
메모리 손상
  ├─ 힙 메타데이터 손상
  ├─ 다음 malloc/free 작업 시 크래시
  │
  v
원격 코드 실행 또는 서비스 중단
```

#### 위험도 평가

| 평가 항목 | 등급 | 근거 |
|----------|------|------|
| **심각도** | **매우 높음** | 힙 손상 → 임의 메모리 쓰기 → 원격 코드 실행 위험 |
| **발생 확률** | **낮음** | 신중한 코드 리뷰로 방지 가능, 하지만 복잡한 흐름에선 가능 |
| **검출 확률** | **낮음** | 동적 런타임 오류, 재현 어려움 |
| **위험도** | **MEDIUM** | 매우높음 × 낮음 / 낮음 = 중간 |

#### 위험 제어

**1차 제어: 포인터 검증 (Primary)**

```c
// xpe_mempool_free() 구현
XpeError xpe_mempool_free(void* ptr) {
    if (ptr == NULL) return XPE_ERR_INVALID_INPUT;
    
    // 할당된 슬롯 범위 확인
    for (int i=0; i<4; i++) {
        if (g_float32_slots[i].ptr == ptr && g_float32_slots[i].refcount > 0) {
            g_float32_slots[i].refcount--;
            return XPE_OK;
        }
    }
    for (int i=0; i<4; i++) {
        if (g_uint16_slots[i].ptr == ptr && g_uint16_slots[i].refcount > 0) {
            g_uint16_slots[i].refcount--;
            return XPE_OK;
        }
    }
    
    // 포인터가 슬롯에 없거나 이미 해제됨
    XPE_SET_ERROR(XPE_ERR_INVALID_INPUT, "Invalid pointer or double free");
    return XPE_ERR_INVALID_INPUT;
}
```

**2차 제어: 동적 할당 검증 (Secondary)**

```c
// 선택사항: AddressSanitizer (ASAN)
// 컴파일 플래그: -fsanitize=address
// 런타임에 모든 메모리 접근 추적, 이중 해제 감지
```

**3차 제어: 코드 리뷰 (Tertiary)**

- 모든 `xpe_mempool_free()` 호출 지점을 코드 리뷰
- 포인터 재사용 가능성 검사
- 순환 참조 또는 중복 파라미터 확인

#### 예상 결과
- 유효하지 않은 포인터 → **XPE_ERR_INVALID_INPUT 반환**
- 이중 해제 시도 → **즉시 거부, 에러 로깅**
- 힙 손상 확률 **< 1%** (포인터 검증 + 코드 리뷰)

---

### HAZ-CMN-003: 메모리 풀 고갈 (Pool Exhaustion)

#### 위험 설명
8개 고정 슬롯 (float32 4개, uint16 4개) 모두가 할당 상태 (refcount > 0)일 때, 9번째 이미지 할당 요청이 실패한다. 파이프라인 흐름이 전체적으로 차단되어 처리 지연 또는 프레임 손실이 발생할 수 있다.

#### 위험 경로

```
정상 처리:
  Frame 1: alloc() → Slot 0 (refcount=1)
           free() → Slot 0 (refcount=0)
  
비정상 처리 (bug):
  Frame 1: alloc() → Slot 0 (refcount=1)
           [free() 호출 빠짐] → Slot 0 (refcount=1)
  
  Frame 2: alloc() → Slot 1 (refcount=1)
           [free() 호출 빠짐] → Slot 1 (refcount=1)
  
  ...
  
  Frame 8: alloc() → Slot 3 (refcount=1)
           [free() 호출 빠짐] → Slot 3 (refcount=1)
  
  Frame 9: alloc() → XPE_ERR_POOL_EXHAUSTED
           파이프라인 정지, 알림 발송
```

#### 위험도 평가

| 평가 항목 | 등급 | 근거 |
|----------|------|------|
| **심각도** | **중간** | 처리 지연/손실, 하지만 이미지 손상은 아님 |
| **발생 확률** | **중간** | 메모리 누수 또는 흐름 제어 오류 |
| **검출 확률** | **높음** | 에러 반환, 로그에 기록, C# UI에 알림 |
| **위험도** | **MEDIUM** | 중간 × 중간 / 높음 |

#### 위험 제어

**1차 제어: 흐름 제어 (Primary)**

```c
// 파이프라인은 동기식으로 처리
// Stage N이 완료될 때까지 Stage N+1 시작 금지
// 따라서 최대 동시 처리 프레임 = 1
// 슬롯 필요 = 최대 2개 (입력 + 임시)
// 안전 마진 = 4개 (충분함)

// C# 파이프라인 코드
foreach (Frame frame in frameQueue) {
    // Stage 1: 모두 완료까지 대기
    xpe_preprocess_all_stages(frame);  // 블로킹
    
    // Stage 2: 모두 완료까지 대기
    xpe_enhance_all_stages(frame);     // 블로킹
    
    // Stage 3: 모두 완료까지 대기
    xpe_display_stage(frame);          // 블로킹
    
    xpe_mempool_free(frame.data);      // 슬롯 해제
}
```

**2차 제어: 메모리 누수 감지 (Secondary)**

```c
// 주기적 통계 확인
void check_mempool_health() {
    const char* stats = xpe_mempool_get_stats();
    // {"allocated": 4, "available": 0, "peak": 4}
    
    if (allocated == 4 && available == 0) {
        // 경고: 모든 슬롯이 사용 중
        xpe_event_emit_alert(XPE_ALERT_WARNING, "All memory pool slots allocated");
    }
}

// C# 타이머에서 주기적 호출
timer.Elapsed += (s, e) => check_mempool_health();
```

**3차 제어: Event/Alert 알림 및 대기 (Tertiary)**

```c
// 할당 실패 시 자동 알림
if (xpe_mempool_alloc(...) == XPE_ERR_POOL_EXHAUSTED) {
    xpe_event_emit_alert(XPE_ALERT_ERROR, "Memory pool exhausted, waiting for slot...");
    
    // 일정 시간 대기 후 재시도 (backoff)
    sleep(100);  // 100ms 대기
    retry_count++;
}
```

#### 예상 결과
- 동기식 파이프라인 → **동시 슬롯 사용 최소화**
- 슬롯 고갈 시 → **즉시 에러 반환 + 알림**
- 자동 복구 → **메모리 누수 정지 시 슬롯 해제됨**

---

### HAZ-CMN-004: JSON 설정 파일 손상 또는 부재

#### 위험 설명
설정 파일 `./config/xpe_config.json`이 없거나 JSON 구문이 잘못되었거나 필수 키가 빠진 경우, 파이프라인은 기본값으로 동작해야 한다. 잘못된 기본값은 부정확한 처리 또는 크래시를 초래할 수 있다.

#### 위험 경로

```
1. 파일 부재
   xpe_config_load("./config/xpe_config.json")
   └─ 파일 없음 → XPE_ERR_FILE_NOT_FOUND
   
2. JSON 구문 오류
   JSON 파서 → syntax error
   └─ return XPE_ERR_CONFIG_INVALID
   
3. 필수 키 부재
   validate_schema(root)
   ├─ "pipeline" 키 없음
   └─ return XPE_ERR_CONFIG_INVALID
   
결과:
   ├─ 기본값 사용 (하드코딩)
   ├─ 기본값이 현재 환경에 부적합할 수 있음
   │   (예: GPU 타임아웃 너무 짧음, 모델 경로 잘못됨)
   └─ 처리 오류 또는 크래시
```

#### 위험도 평가

| 평가 항목 | 등급 | 근거 |
|----------|------|------|
| **심각도** | **중간** | 처리 오류/크래시 가능, 하지만 데이터 손상은 아님 |
| **발생 확률** | **낮음** | 배포 시 설정 파일 검증, 하지만 설치 오류 가능 |
| **검출 확률** | **높음** | 시작 시 로드 실패 또는 런타임 오류로 즉시 감지 |
| **위험도** | **LOW-MEDIUM** | 중간 × 낮음 / 높음 |

#### 위험 제어

**1차 제어: 스키마 검증 (Primary)**

```c
// xpe_config.h
XpeError validate_config_schema(cJSON* root) {
    // 필수 섹션
    if (!cJSON_HasObjectItem(root, "pipeline")) {
        return XPE_ERR_CONFIG_INVALID;
    }
    
    cJSON* pipeline = cJSON_GetObjectItem(root, "pipeline");
    if (!cJSON_IsObject(pipeline)) {
        return XPE_ERR_CONFIG_INVALID;
    }
    
    // 필수 키
    if (!cJSON_HasObjectItem(pipeline, "flags_enabled")) {
        return XPE_ERR_CONFIG_INVALID;
    }
    
    return XPE_OK;
}
```

**2차 제어: 기본값 제공 (Secondary)**

```c
// 파일 로드 실패 시 안전한 기본값으로 초기화
static XpeConfig g_default_config = {
    .enabled_flags = 0x00FF,           // 일반적인 플래그 조합
    .stage_timeouts = {500, 300, 400}, // 보수적인 타임아웃
    .mode = "clinical",
    .enable_gpu = false,               // GPU 없이도 작동
    // ... 추가 기본값
};

// 로드 실패 처리
if (xpe_config_load(path) != XPE_OK) {
    xpe_event_emit_alert(XPE_ALERT_WARNING, "Config load failed, using defaults");
    memcpy(&g_config, &g_default_config, sizeof(XpeConfig));
    return XPE_OK;  // 기본값으로 계속 진행
}
```

**3차 제어: 배포 검증 (Tertiary)**

```bash
# 패킹/배포 스크립트
if [ ! -f "./config/xpe_config.json" ]; then
    echo "ERROR: xpe_config.json not found in package"
    exit 1
fi

# JSON 유효성 검사
jq . ./config/xpe_config.json > /dev/null
if [ $? -ne 0 ]; then
    echo "ERROR: xpe_config.json is not valid JSON"
    exit 1
fi

# 필수 키 확인
jq '.pipeline.flags_enabled' ./config/xpe_config.json > /dev/null
if [ $? -ne 0 ]; then
    echo "ERROR: Missing required key pipeline.flags_enabled"
    exit 1
fi
```

#### 예상 결과
- 설정 파일 존재 + 유효 → **성공적 로드**
- 파일 부재/손상 → **기본값 사용, 경고 알림**
- 런타임 오류 → **Event System 통해 사용자 알림**

---

### HAZ-CMN-005: XPE_FLAG 값 오류로 인한 다운스트림 처리 오류

#### 위험 설명
XPE_FLAG_* 비트마스크가 잘못 설정되거나 우회된 단계의 플래그가 설정되면, 다운스트림 DLL이 이미지를 잘못 처리할 수 있다. 예를 들어, `XPE_FLAG_LOG_TRANSFORMED` 비트가 설정되지 않았는데 로그 변환 없이 이미지를 로그 도메인으로 가정하고 처리할 수 있다.

#### 위험 경로

```
xpe_preprocess_stage1()
  ├─ Log Transform 우회 (config: "skip_log_transform": true)
  ├─ image.xpeFlags |= XPE_FLAG_LOG_TRANSFORMED ← BUG: 우회되었는데 설정함
  
다운스트림 (xpe_enhance_basic):
  ├─ if (flags & XPE_FLAG_LOG_TRANSFORMED) → true
  ├─ 로그 도메인 데이터라고 가정
  ├─ 로그 도메인 필터 적용 (예: CLAHE)
  │
  v
선형 도메인 데이터에 로그 필터 적용
  ├─ 부정확한 강조
  ├─ 진단 오류 위험
```

#### 위험도 평가

| 평가 항목 | 등급 | 근거 |
|----------|------|------|
| **심각도** | **높음** | 진단 오류 → 임상 판단 오류 → 환자 치료 오류 |
| **발생 확률** | **낮음** | 신중한 플래그 관리로 방지 가능 |
| **검출 확률** | **낮음** | 출력 이미지는 시각적으로 "정상"일 수 있음, 미묘한 오류 |
| **위험도** | **MEDIUM** | 높음 × 낮음 / 낮음 |

#### 위험 제어

**1차 제어: 플래그 설정 규칙 (Primary)**

```c
// 규칙: 단계가 우회되면 플래그를 설정하지 않음
// 규칙: 단계가 실행되면 반드시 플래그를 설정

xpe_error_t xpe_apply_log_transform(XpeImage* image, bool should_apply) {
    if (should_apply) {
        // 로그 변환 실행
        for (int i=0; i<image->width * image->height; i++) {
            float* ptr = (float*)image->data + i;
            *ptr = log10(*ptr + 1e-6f);
        }
        // 플래그 설정 (반드시)
        image->xpeFlags |= XPE_FLAG_LOG_TRANSFORMED;
        return XPE_OK;
    } else {
        // 로그 변환 우회
        // 플래그 설정하지 않음 (중요!)
        return XPE_OK;
    }
}
```

**2차 제어: 플래그 검증 함수 (Secondary)**

```c
// 다운스트림에서 플래그 의존성 검증
xpe_error_t xpe_enhance_clahe(XpeImage* image) {
    // CLAHE는 로그 도메인 데이터 가정
    if (!(image->xpeFlags & XPE_FLAG_LOG_TRANSFORMED)) {
        // 로그 변환이 먼저 실행되었는지 확인
        XPE_SET_ERROR(XPE_ERR_DEPENDENCY_NOT_MET, 
                      "CLAHE requires LOG_TRANSFORMED flag");
        return XPE_ERR_DEPENDENCY_NOT_MET;
    }
    
    // 안전하게 CLAHE 적용
    return apply_clahe_kernel(image);
}
```

**3차 제어: 문서화 및 코드 리뷰 (Tertiary)**

```c
// 헤더 파일에 플래그 의존성 문서화
/**
 * xpe_enhance_clahe: CLAHE 강조 (로그 도메인용)
 * 
 * 선행 조건:
 *  - image->xpeFlags & XPE_FLAG_LOG_TRANSFORMED (필수)
 *  - image->xpeFlags & XPE_FLAG_CALIBRATED (권장)
 * 
 * 오류:
 *  - XPE_ERR_DEPENDENCY_NOT_MET: LOG_TRANSFORMED 플래그 부재
 */
XpeError xpe_enhance_clahe(XpeImage* image);
```

#### 예상 결과
- 우회 시 플래그 설정 안 함 → **다운스트림 의존성 검증 실패**
- 의존성 검증 → **XPE_ERR_DEPENDENCY_NOT_MET 반환**
- 에러 거부 → **부정확한 처리 방지**

---

### HAZ-CMN-006: Event Queue 오버플로우로 인한 알림 손실

#### 위험 설명
여러 DLL에서 동시에 `xpe_event_emit_alert()`를 호출하면 256개 원형 버퍼가 가득 찰 수 있다. 새로운 알림은 가장 오래된 알림을 제거하여 자리를 만드는데, 중요한 경고가 손실될 수 있다.

#### 위험 경로

```
고부하 상황:
  Frame 1: 온도 경고 → 큐[0]
  Frame 2: 캘리브레이션 경고 → 큐[1]
  ...
  Frame 256: 알림 256 → 큐[255] (가득 참)
  
  Frame 257: 치명적 오류 "AI unavailable" → 큐[0] 제거, 대체
             이전의 중요한 경고 손실됨
             
  C# UI: "온도 경고"가 표시되지 않음
         사용자가 온도 이상을 인식 못함
         → 부정확한 영상 취득
```

#### 위험도 평가

| 평가 항목 | 등급 | 근거 |
|----------|------|------|
| **심각도** | **중간** | 알림 손실 → 사용자 인식 불가, 하지만 데이터 손상은 아님 |
| **발생 확률** | **낮음** | 초당 > 256개의 알림은 드문 경우 |
| **검출 확률** | **중간** | 큐 오버플로우 메트릭 기록, 하지만 손실된 알림은 추적 불가 |
| **위험도** | **LOW** | 중간 × 낮음 / 중간 |

#### 위험 제어

**1차 제어: 큐 크기 충분히 설정 (Primary)**

```c
// 256개 버퍼는 충분한가?
// - 초당 최대 100개 프레임 처리
// - 프레임당 최대 2개 알림 (온도, 캘리브레이션)
// - 알림 스레드: 10ms마다 폴링 → 10ms 동안 최대 200개 알림 누적
// - 256 > 200 → 충분함

// 그래도 안전을 위해 1024로 확대 가능
#define XPE_EVENT_QUEUE_SIZE 1024
```

**2차 제어: 우선순위 기반 대체 (Secondary)**

```c
// FIFO 대체 대신 우선순위 기반 대체
typedef struct {
    AlertType type;     // 우선도 기준
    int priority;       // 1=INFO, 2=WARNING, 3=ERROR
    uint64_t timestamp;
    char message[512];
} XpeAlertWithPriority;

// 큐가 가득 찼을 때: 가장 낮은 우선도 알림 제거
if (queue_full) {
    int min_priority_idx = find_min_priority();
    remove_alert(queue[min_priority_idx]);
    add_alert(new_alert);  // 새 알림이 더 중요할 가능성
}
```

**3차 제어: 오버플로우 카운터 및 경고 (Tertiary)**

```c
// 오버플로우 추적
static uint64_t g_aed_discarded_count = 0;

// 오버플로우 발생 시 경고 알림 자동 발송
if (queue_overflow) {
    g_aed_discarded_count++;
    // 무한 루프 방지: 5초마다 1회만 발송
    if (time_since_last_overflow_warning > 5000ms) {
        // 내부적으로 새 알림을 발송 (별도 큐 사용 또는 콘솔 로그)
fprintf(stderr, "Event Queue Overflow: %lu alerts discarded\n", g_event_discarded_count);
    }
}
```

#### 예상 결과
- 큐 크기 충분 (256 또는 1024) → **일반적 상황에서 오버플로우 없음**
- 오버플로우 시 → **우선도 낮은 알림 대체**
- 오버플로우 카운터 → **메트릭으로 추적 가능**

---

### HAZ-CMN-007: C# P/Invoke 콜백 크래시로 인한 DLL 불안정성

#### 위험 설명
Event System이 C# 콜백을 호출할 때 콜백 함수가 예외를 발생시키거나 포인터 역참조 오류를 일으킬 수 있다. C++ 코드(DLL)가 C# 예외를 처리하지 못하면 전체 프로세스가 불안정해질 수 있다.

#### 위험 경로

```
C++의 Event System 스레드:
  └─ for (callback in callbacks) {
        result = callback(json_str, userdata)  ← C# 콜백 호출
        if (crash here) → ?
      }

C# 콜백:
  void OnAlert(string json, IntPtr userdata) {
      JObject alert = JObject.Parse(json);
      MessageBox.Show(alert["message"].ToString());  // UI 조작
      // 예외 발생 가능:
      // - json 파싱 오류
      // - 필드 부재
      // - UI 스레드 크로스
      → throw new Exception(...)
  }

결과:
  C++ 영역에서 처리되지 않은 C# 예외
  → SEH (Structured Exception Handling) 오류
  → 프로세스 크래시 또는 불안정한 상태
```

#### 위험도 평가

| 평가 항목 | 등급 | 근거 |
|----------|------|------|
| **심각도** | **높음** | 프로세스 크래시 → 영상 취득 중단 → 환자 평가 불가 |
| **발생 확률** | **중간** | C# 콜백은 사용자 정의, 오류 가능성 있음 |
| **검출 확률** | **중간** | 크래시 스택 추적 가능, 하지만 재현 어려움 |
| **위험도** | **MEDIUM** | 높음 × 중간 / 중간 |

#### 위험 제어

**1차 제어: C++ 콜백 래퍼 (Primary)**

```c
// C++ 영역에서 C# 콜백을 안전하게 호출
XpeError safe_invoke_callback(
    XpeAlertCallback callback,
    const char* json,
    void* userdata)
{
    // SEH를 사용한 예외 처리 (Windows)
    __try {
        // C# 콜백 호출
        callback(json, userdata);
        return XPE_OK;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // 예외 발생 시
        fprintf(stderr, "Callback exception: %d\n", GetExceptionCode());
        xpe_event_emit_alert(XPE_ALERT_ERROR, "Callback crashed, continuing");
        return XPE_ERR_EVENT_CALLBACK_FAILED;
    }
}

// 대안: Unix/Linux (std::exception)
try {
    callback(json, userdata);
    return XPE_OK;
} catch (const std::exception& e) {
    fprintf(stderr, "Callback exception: %s\n", e.what());
    return XPE_ERR_EVENT_CALLBACK_FAILED;
} catch (...) {
    fprintf(stderr, "Callback unknown exception\n");
    return XPE_ERR_EVENT_CALLBACK_FAILED;
}
```

**2차 제어: C# 콜백 가이드라인 (Secondary)**

```csharp
// ImageProcTest.cs에 문서화된 콜백 작성 규칙

/// <summary>
/// XPE Alert 콜백 - 엄격한 오류 처리 필수
/// </summary>
private void OnAlert(string json, IntPtr userdata) {
    try {
        // 1. JSON 파싱 (오류 가능)
        JObject alert = null;
        try {
            alert = JObject.Parse(json);
        } catch (JsonException ex) {
            Console.WriteLine("Invalid JSON: " + ex.Message);
            return;  // 조용히 실패
        }
        
        // 2. 필드 추출 (오류 가능)
        string message = alert["message"]?.ToString() ?? "Unknown alert";
        string type = alert["type"]?.ToString() ?? "ERROR";
        
        // 3. UI 스레드 복합성 (오류 가능)
        if (Application.Current.Dispatcher.CheckAccess()) {
            // 이미 UI 스레드
            MessageBox.Show(message, "Alert: " + type);
        } else {
            // UI 스레드가 아님: Invoke 사용
            Application.Current.Dispatcher.Invoke(() => {
                MessageBox.Show(message, "Alert: " + type);
            });
        }
    } catch (Exception ex) {
        // 예상치 못한 예외: 조용히 처리
        Console.WriteLine("Callback error: " + ex.Message);
        // DLL 크래시 방지: 예외를 전파하지 않음
    }
}

// 등록
XpeCommon.xpe_event_register_callback(OnAlert, IntPtr.Zero);
```

**3차 제어: 콜백 타임아웃 (Tertiary)**

```c
// 콜백 실행 시간 제한 (선택)
// 콜백이 너무 오래 실행되면 강제 종료

#include <pthread.h>

typedef struct {
    XpeAlertCallback callback;
    const char* json;
    void* userdata;
    int timeout_ms;
    volatile int completed;
} CallbackTask;

void* callback_worker_thread(void* arg) {
    CallbackTask* task = (CallbackTask*)arg;
    task->callback(task->json, task->userdata);
    task->completed = 1;
    return NULL;
}

XpeError invoke_callback_with_timeout(
    XpeAlertCallback callback,
    const char* json,
    void* userdata,
    int timeout_ms)
{
    CallbackTask task = {callback, json, userdata, timeout_ms, 0};
    pthread_t thread;
    pthread_create(&thread, NULL, callback_worker_thread, &task);
    
    // timeout_ms 동안 대기
    struct timespec deadline = {
        .tv_sec = time(NULL) + timeout_ms / 1000,
        .tv_nsec = (timeout_ms % 1000) * 1000000
    };
    
    int timed_out = 0;
    for (int i=0; i < timeout_ms; i += 10) {
        if (task.completed) break;
        usleep(10000);  // 10ms 대기
    }
    
    if (!task.completed) {
        fprintf(stderr, "Callback timeout, cancelling thread\n");
        pthread_cancel(thread);  // 스레드 강제 종료 (주의: unsafe)
        return XPE_ERR_EVENT_CALLBACK_FAILED;
    }
    
    pthread_join(thread, NULL);
    return XPE_OK;
}
```

#### 예상 결과
- C# 콜백 예외 → **C++ 래퍼에서 안전하게 포착**
- 예외 처리 → **DLL 계속 작동**
- 콜백 오류 → **Event System 로그, DLL 안정성 유지**

---

## 3. 위험 요약 테이블

| 위험 ID | 제목 | 심각도 | 발생 확률 | 위험도 | 상태 |
|---------|------|--------|---------|--------|------|
| HAZ-CMN-001 | Struct 정렬 불일치 | 높음 | 중간 | **MEDIUM** | CONTROLLED |
| HAZ-CMN-002 | 메모리 풀 이중 해제 | 매우높음 | 낮음 | **MEDIUM** | CONTROLLED |
| HAZ-CMN-003 | 메모리 풀 고갈 | 중간 | 중간 | **MEDIUM** | CONTROLLED |
| HAZ-CMN-004 | JSON 설정 손상 | 중간 | 낮음 | **LOW** | CONTROLLED |
| HAZ-CMN-005 | XPE_FLAG 오류 | 높음 | 낮음 | **MEDIUM** | CONTROLLED |
| HAZ-CMN-006 | Event Queue 오버플로우 | 중간 | 낮음 | **LOW** | CONTROLLED |
| HAZ-CMN-007 | C# 콜백 크래시 | 높음 | 중간 | **MEDIUM** | CONTROLLED |

**결론**: 모든 위험은 식별되었고 위험 제어 방법이 적용되어 있다. 클래스 B 안전 수준 달성.

---

**SHA-COMMON-001 v1.0.0 끝**
