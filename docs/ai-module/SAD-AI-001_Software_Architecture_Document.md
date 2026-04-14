# SAD-AI-001 소프트웨어 아키텍처 문서 (Software Architecture Document)

**문서 ID**: SAD-AI-001  
**모듈**: xpe_ai (AI Framework)  
**버전**: 1.0.0  
**날짜**: 2026-04-14  
**IEC 62304 Section**: §5.3 Software Architecture  
**관련 문서**: SRS-AI-001, xpe-ai-prd.md, SHA-AI-001, RTM-AI-001

---

## 1. 아키텍처 개요

### 1.1 계층 위치

```
Layer 2  ImageProcTest.exe (C# WPF)       파이프라인 오케스트레이터
           |
           | P/Invoke (C ABI)
           v
Layer 1  xpe_ai.dll  <-- 이 문서의 대상
           |
           | IPC + Named Events
           |
           v (다른 프로세스)
         xpe_ai_worker.exe  (샌드박스 워커)
           |
           | 링크
           v
Layer 0  xpe_common.dll  (타입, 메모리, 구성)
```

### 1.2 핵심 설계 원칙

1. **IPC 프록시**: xpe_ai.dll은 프록시만 담당; 실제 ONNX 추론은 xpe_ai_worker.exe에서
2. **프로세스 격리**: 워커 크래시가 주 프로세스에 영향 없음
3. **비블로킹**: 메인 파이프라인은 AI 워커 완료를 기다리지 않음
4. **Sidecar Output**: 모든 AI 결과는 JSON sidecar로만 저장
5. **Graceful Degradation**: 워커 오류 시 고전적(classical) 경로로 자동 전환

---

## 2. 컴포넌트 분해 (Component Decomposition)

### 2.1 xpe_ai.dll (메인 프로세스)

```
┌─────────────────────────────────────────────────────┐
│              xpe_ai.dll (Layer 1)                   │
│                                                     │
│  ┌──────────────────────────────────────────────┐  │
│  │   AiProxyClient                              │  │
│  │   · 요청 큐 관리                             │  │
│  │   · 비동기 IPC 전송                          │  │
│  │   · 응답 수집 스레드                         │  │
│  └──────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────┐  │
│  │   WorkerLifecycleManager                     │  │
│  │   · CreateProcess() / TerminateProcess()    │  │
│  │   · 심박 모니터링 (1Hz)                      │  │
│  │   · 자동 재시작 로직 (max 3회)              │  │
│  └──────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────┐  │
│  │   SharedMemoryChannel                        │  │
│  │   · Named shared memory 할당                 │  │
│  │   · Request/Response slot 관리               │  │
│  │   · Named events: REQUEST_READY, RESPONSE_READY │
│  └──────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────┐  │
│  │   ModelRegistry                              │  │
│  │   · SHA-256 모델 무결성 검증                │  │
│  │   · 모델 경로 관리                           │  │
│  │   · 버전 메타데이터 로드                     │  │
│  └──────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────┐  │
│  │   FallbackController                         │  │
│  │   · AI 가용성 플래그 관리                    │  │
│  │   · Classical fallback 라우팅               │  │
│  │   · OOD 감지 및 threshold 관리             │  │
│  └──────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────┘
```

#### 주요 클래스 및 인터페이스

**AiProxyClient**:
```cpp
class AiProxyClient {
public:
    // 비동기 요청 큐 관리
    void enqueue_request(int function_id, const float* image, int width, int height);
    bool poll_response(int req_id, char* json_out, int max_len);
    
    // IPC 전송
    int send_request_ipc(const RequestHeader* header);
    int wait_response_ipc(int timeout_ms);
    
private:
    std::queue<AiRequest> request_queue;
    std::map<int, AiResponse> response_cache;
    std::mutex queue_lock;
};
```

**WorkerLifecycleManager**:
```cpp
class WorkerLifecycleManager {
public:
    int start_worker();
    int monitor_heartbeat();
    int restart_worker(int retry_count);
    void shutdown_worker();
    
private:
    HANDLE worker_process;
    int heartbeat_miss_count;
    int restart_counter;
    std::chrono::steady_clock::time_point last_heartbeat;
};
```

**SharedMemoryChannel**:
```cpp
class SharedMemoryChannel {
public:
    int allocate(int total_size);
    void* get_request_slot();
    void* get_response_slot();
    HANDLE get_request_event();
    HANDLE get_response_event();
    void deallocate();
    
private:
    HANDLE shm_handle;
    void* base_address;
    HANDLE request_event;
    HANDLE response_event;
};
```

**ModelRegistry**:
```cpp
class ModelRegistry {
public:
    int verify_model_integrity(const char* model_id, const char* model_path);
    int load_model_metadata(const char* model_id);
    const char* get_model_version(const char* model_id);
    const char* get_model_hash(const char* model_id);
    
private:
    std::map<std::string, ModelMetadata> registry;
    // registry["bodypart"] = { 
    //   version: "bodypart-mobilenet-v3-20260414",
    //   expected_hash: "abc123..."
    // }
};
```

**FallbackController**:
```cpp
class FallbackController {
public:
    void set_ai_available(bool available);
    bool is_ai_available();
    int route_with_fallback(int function_id, ...);
    void handle_timeout(int req_id);
    void handle_ood_detection(float confidence, float threshold);
    
private:
    bool ai_available_flag;
    int timeout_counter;
};
```

---

### 2.2 xpe_ai_worker.exe (샌드박스 워커)

```
┌──────────────────────────────────────────────────────┐
│         xpe_ai_worker.exe (Sandboxed Process)        │
│                                                      │
│  ┌───────────────────────────────────────────────┐  │
│  │   IpcServer                                   │  │
│  │   · Named memory listener                     │  │
│  │   · Request/Response 핸들링                   │  │
│  │   · Named events 신호 대기/발신             │  │
│  └───────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────┐  │
│  │   OnnxInferenceEngine                         │  │
│  │   · ONNX Runtime C++ API                      │  │
│  │   · 모델 세션 관리                            │  │
│  │   · 메모리 할당 & 해제                       │  │
│  └───────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────┐  │
│  │   BodyPartClassifier (SWU-2.7)               │  │
│  │   · Input: 512×512 float32                   │  │
│  │   · Model: MobileNet-v3-Small                │  │
│  │   · Output: JSON {body_part, confidence, ...}│  │
│  └───────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────┐  │
│  │   CollimationRefiner (SWU-2.8-AI)            │  │
│  │   · Input: image + baseline ROI              │  │
│  │   · Model: U-Net edge detection              │  │
│  │   · Output: refined ROI (shrink only)        │  │
│  └───────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────┐  │
│  │   ImageStitcher (SWU-2.9)                    │  │
│  │   · Phase correlation alignment              │  │
│  │   · CNN seam blending                        │  │
│  │   · Output: panoramic image                  │  │
│  └───────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────┐  │
│  │   BoneSuppressionEngine (SWU-2.11)           │  │
│  │   · Input: chest image (log domain)          │  │
│  │   · Model: Residual U-Net                    │  │
│  │   · Output: derived image + confidence      │  │
│  └───────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────┐  │
│  │   DlDenoiser (SWU-2.12, Research Path)       │  │
│  │   · Model: DnCNN variant                      │  │
│  │   · Fail-closed: classical fallback on error │  │
│  └───────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────┘
```

#### 주요 클래스

**IpcServer**:
```cpp
class IpcServer {
public:
    int listen_for_requests();
    int process_request(const RequestHeader* header);
    int send_response(const ResponseSlot* response);
    void shutdown();
    
private:
    HANDLE shm_handle;
    HANDLE request_event;
    HANDLE response_event;
};
```

**OnnxInferenceEngine**:
```cpp
class OnnxInferenceEngine {
public:
    int load_model(const char* model_path, const char* model_id);
    int run_inference(const float* input, int width, int height, char* output_json);
    void unload_model(const char* model_id);
    
private:
    std::map<std::string, Ort::Session> sessions;
    Ort::Env ort_env;
};
```

**BodyPartClassifier**:
```cpp
class BodyPartClassifier {
public:
    int classify(const float* image, int width, int height, char* json_out, int max_len);
    
private:
    Ort::Session session;
    float confidence_threshold = 0.70f;
    const char* model_version = "bodypart-mobilenet-v3-20260414";
};
```

---

## 3. IPC 프로토콜 상세

### 3.1 공유 메모리 레이아웃

```
총 크기: ~10MB

┌────────────────────────────────────────────┐
│ Request Header (64 bytes)                  │
├────────────────────────────────────────────┤
│  ├─ uint32 magic_number (0xAEAEAEAE)      │
│  ├─ uint32 function_id                    │
│  │         (1: body_part, 2: collim, ...) │
│  ├─ uint32 width, height                  │
│  ├─ uint32 data_size                      │
│  ├─ uint32 timestamp_ms                   │
│  ├─ uint32 timeout_ms                     │
│  └─ uint32 request_id                     │
├────────────────────────────────────────────┤
│ Image Data (variable, max 5MB)             │
│ (이미지 픽셀 데이터)                       │
├────────────────────────────────────────────┤
│ Response Slot (1KB)                        │
│  ├─ uint32 status                         │
│  │         (0: OK, 1: ERROR, 2: TIMEOUT) │
│  ├─ uint32 error_code                     │
│  ├─ char[960] result_json (UTF-8)        │
│  └─ uint32 response_size                  │
└────────────────────────────────────────────┘
```

### 3.2 Named Events 시그널링

```
Named Event 1: XPE_AI_REQUEST_READY
├─ Set by: xpe_ai.dll
├─ Waited by: xpe_ai_worker.exe
└─ Meaning: 요청 데이터 준비 완료

Named Event 2: XPE_AI_RESPONSE_READY
├─ Set by: xpe_ai_worker.exe
├─ Waited by: xpe_ai.dll
└─ Meaning: 응답 데이터 준비 완료

Named Event 3: XPE_AI_WORKER_ALIVE
├─ Set by: xpe_ai_worker.exe (매 1초)
├─ Waited by: xpe_ai.dll heartbeat thread
└─ Meaning: 워커 프로세스가 살아있음
```

### 3.3 요청/응답 시퀀스

```
xpe_ai.dll (Client)              xpe_ai_worker.exe (Server)
    |                                     |
    | AiProxyClient::enqueue_request()   |
    | 1. Write request header to SHM     |
    | 2. Write image data to SHM         |
    |                                     |
    | 3. Set XPE_AI_REQUEST_READY        |
    |------------------------------------>|
    |                                     |
    |                        IpcServer::listen()
    |                        4. Wait for REQUEST_READY
    |                        5. Read request header
    |                        6. Read image data
    |                        7. Dispatch to appropriate handler
    |                             (BodyPartClassifier, etc.)
    |                        8. Run inference
    |                        9. Format output JSON
    |                        10. Write response to SHM
    |                        11. Set RESPONSE_READY
    |<---------------------------|
    |                            |
    | 12. Wait for RESPONSE_READY (timeout 5s)
    | 13. Read response from SHM
    | 14. Cache in response_cache
    |
    | 15. Application calls poll_response(req_id)
    | 16. Return cached JSON
```

---

## 4. 워커 라이프사이클 상태 머신

```
┌──────────────┐
│   STOPPED    │ (초기 상태)
└──────┬───────┘
       │ start_worker()
       v
┌──────────────────────┐
│ STARTING             │ (CreateProcess 중)
│ timeout: 5s          │
└──────┬───────────────┘
       │ CreateProcess OK
       v
┌──────────────────────┐
│ WAITING_HEARTBEAT    │ (첫 심박 대기)
│ timeout: 5s          │
└──────┬───────────────┘
       │ 첫 heartbeat 수신
       v
┌──────────────────────┐
│ RUNNING              │ (정상 운영)
│ heartbeat: 1Hz       │
└──────┬───────────────┘
       │ 3회 연속 heartbeat miss
       v
┌──────────────────────┐
│ DEAD                 │ (워커 사망)
└──────┬───────────────┘
       │ restart_counter < 3
       v
┌──────────────────────┐
│ RESTARTING (retry N) │
│ backoff: 100ms × N   │
└──────┬───────────────┘
       │ CreateProcess OK
       v
     (다시 STARTING으로...)
       
       또는
       
       restart_counter >= 3
       │
       v
┌──────────────────────┐
│ UNAVAILABLE          │ (이번 세션 동안 비활성)
│ AI 기능 OFF          │
└──────────────────────┘
```

---

## 5. 메모리 레이아웃 및 예산

### 5.1 xpe_ai.dll 메모리

```
┌─────────────────────────────┐
│ Static Data (~ 2MB)         │
│ ├─ Code segment             │
│ └─ ROData (설정, 문자열)    │
├─────────────────────────────┤
│ Heap (~ 5MB)                │
│ ├─ AiProxyClient 큐         │
│ ├─ WorkerLifecycleManager   │
│ ├─ SharedMemoryChannel      │
│ └─ Response cache           │
├─────────────────────────────┤
│ Stack per thread (~ 3MB)    │
│ ├─ Main thread stack        │
│ ├─ IPC send thread stack    │
│ └─ Response collect thread  │
└─────────────────────────────┘
총: ~10MB
```

### 5.2 xpe_ai_worker.exe 메모리

```
┌──────────────────────────┐
│ Code + Static (~ 5MB)    │
├──────────────────────────┤
│ ONNX Runtime (~ 50MB)    │
│ ├─ Kernel lib            │
│ └─ Execution providers   │
├──────────────────────────┤
│ Model Weights (~ 200MB)  │
│ ├─ bodypart: 5MB         │
│ ├─ collimation: 2MB      │
│ ├─ stitch: 15MB          │
│ ├─ bone_suppress: 100MB  │
│ └─ denoiser: 8MB         │
├──────────────────────────┤
│ Working Memory (~ 400MB) │
│ ├─ Input tensors         │
│ ├─ Hidden layers         │
│ └─ Output buffers        │
├──────────────────────────┤
│ Heap for allocations     │
└──────────────────────────┘
총: ~ 740MB (Job Object limit)
```

---

## 6. 오류 처리 및 복구

### 6.1 오류 코드 정의

```cpp
// xpe_common.h
#define XPE_OK                              0
#define XPE_ERR_WORKER_STARTUP_FAILED      -101
#define XPE_ERR_WORKER_TIMEOUT             -102
#define XPE_ERR_WORKER_DEAD                -103
#define XPE_ERR_MODEL_INTEGRITY_FAILED     -201
#define XPE_ERR_OOD_DETECTION              -202
#define XPE_ERR_INSUFFICIENT_OVERLAP       -301  // stitching
#define XPE_ERR_AI_UNAVAILABLE             -999
```

### 6.2 복구 전략

| 오류 | 즉시 처리 | 재시도 | 예방 |
|-----|----------|--------|------|
| Worker startup failed | 로그 + 재시작 | 최대 3회 | 주기적 heartbeat |
| IPC timeout | 워커 재시작 | 타임아웃 카운터 | 5s 타임아웃 설정 |
| Model integrity failed | AI 비활성화 | 없음 | SHA-256 검증 |
| OOD detection | "UNKNOWN" 반환 | 없음 | Confidence 임계값 |
| Memory allocation failed | fallback 사용 | 없음 | Job Object 상한 |

---

## 7. 동시성 및 스레드 안전성

### 7.1 스레드 구조

```
Main Thread (xpe_ai.dll)
├─ AiProxyClient::enqueue_request() [호출자 스레드]
│  └─ Lock request_queue → enqueue
│
Sender Thread
├─ Dequeue request
├─ Write to SHM
├─ Signal REQUEST_READY
└─ Wait for RESPONSE_READY (timeout)

Response Collect Thread
├─ Poll response slot
├─ Cache response_cache
└─ Signal application
```

### 7.2 뮤텍스 및 동기화

```cpp
// AiProxyClient
std::mutex request_queue_lock;      // request_queue 보호
std::condition_variable queue_cv;   // enqueue 신호

// SharedMemoryChannel
std::mutex shm_lock;                // SHM 접근 보호

// WorkerLifecycleManager
std::shared_mutex worker_state_lock; // worker status 보호
```

---

## 8. 진단 및 로깅

### 8.1 진단 JSON 스키마

```json
{
  "event": "ai_request_processed",
  "timestamp": "2026-04-14T12:00:00.123Z",
  "function_id": 1,
  "function_name": "body_part_recognition",
  "input_size": "512x512",
  "elapsed_ms": 245,
  "status": "ok",
  "error_code": 0,
  "result": {
    "body_part": "chest",
    "confidence": 0.92,
    "model_version": "bodypart-mobilenet-v3-20260414"
  },
  "worker_uptime_ms": 125340,
  "ipc_latency_ms": 5
}
```

### 8.2 로그 수준

| 수준 | 예 |
|-----|-----|
| ERROR | Worker startup failed, Model integrity check failed |
| WARN | Worker heartbeat miss, IPC timeout, Confidence < threshold |
| INFO | Worker started, Inference completed, AI disabled |
| DEBUG | Request enqueued, Response cached, Retry attempted |

---

## 9. 인터페이스 및 API

### 9.1 Public C ABI (xpe_ai.dll exports)

```c
// 초기화
int xpe_ai_initialize(const char* config_json);
int xpe_ai_shutdown();

// 신체 부위 인식
int xpe_ai_body_part_classify(
    const float* image, int width, int height,
    char* output_json, int max_json_len
);

// 조명 ROI 정제
int xpe_ai_refine_collimation(
    const float* image, int width, int height,
    const XpeROI* baseline_roi, XpeROI* refined_roi_out,
    char* output_json, int max_json_len
);

// 이미지 스티칭
int xpe_ai_stitch_images(
    const float** images, int num_images,
    int width, int height,
    float* stitched_out, int* out_width, int* out_height,
    char* diagnostic_json, int max_json_len
);

// 뼈 억제
int xpe_ai_suppress_bones(
    const float* image, int width, int height,
    float* suppressed_image_out,
    char* output_json, int max_json_len
);

// 상태 조회
bool xpe_ai_is_available();
int xpe_ai_get_last_error();
```

---

*SAD-AI-001 소프트웨어 아키텍처 문서 v1.0.0 끝*
