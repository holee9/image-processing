# Software Design Description (SDD)

## xpe_ai.dll -- AI Inference Module

| Field | Value |
|-------|-------|
| **Document ID** | SDD-AI-001 |
| **Version** | 0.1.0 |
| **Status** | Draft (Skeleton) |
| **Date** | 2026-04-22 |
| **Author** | xpe-docs |
| **IEC 62304 Class** | B |
| **SPEC Reference** | SPEC-XPE-P3-AI v1.1 |
| **Implementation Status** | Skeleton (Stub Build) |

> **Implementation Note**: This document reflects the skeleton implementation as of 2026-04-22.
> Components marked **[SKELETON]** have structural scaffolding but no full runtime logic.
> Components marked **[DEFERRED]** are planned for Phase 3 full implementation.

---

## 1. Introduction

### 1.1 Purpose

This document describes the software architecture and detailed design for `xpe_ai.dll`. It satisfies IEC 62304 Class B requirements for software design description (Section 5.3).

### 1.2 Design Goals

- Worker process isolation for crash-safe AI inference
- Deterministic fallback for all AI operations (REQ-AI-002)
- No lateral DLL dependencies (depends only on `xpe_common.dll`)
- Opt-in activation (default off) for safety
- ONNX Runtime multi-EP support (CPU, CUDA, TensorRT, DirectML)
- C ABI contract with exception-free boundary

---

## 2. Architecture Overview

### 2.1 Module Position in XPE Pipeline

```
xpe_common.dll (Layer 0)
     |
     +-- xpe_enhance_basic.dll (Layer 1, Phase 1)
     |       log_transform, CLAHE, noise_reduce
     |
     +-- xpe_enhance_advanced.dll (Layer 1, Phase 2)
     |       MFP, fractional edge, collimation, EI
     |
     +-- xpe_ai.dll (Layer 1, Phase 3)            <== THIS MODULE
     |       AI inference proxy + worker process
     |
     +-- xpe_display.dll (Layer 1)
     |       VOI LUT, Presentation LUT
     |
     +-- xpe_dicom.dll (Layer 1)
             DICOM I/O, network
```

### 2.2 Dependency Graph

```
xpe_ai.dll
  +-- xpe_common.dll              (memory, types, error codes, logging)
  +-- ONNX Runtime 1.20+          (model inference, optional via CMake flag)
  +-- spdlog 1.13.x               (diagnostic logging, conditional)
  +-- nlohmann/json 3.11.x        (JSON config parsing, conditional)
  +-- fmt 10.x                    (string formatting)

xpe_ai_worker.exe                 (separate process, linked via IPC)
  +-- ONNX Runtime 1.20+          (actual inference execution)
  +-- xpe_common.dll              (shared types and error codes)
```

Forbidden dependencies: OpenCV, DCMTK, FFTW3, any other `xpe_*.dll`.

### 2.3 Layer Architecture

The module follows a four-layer architecture:

```
+-----------------------------------------------------------+
|  C ABI Layer (extern "C" exported functions)               |
|  ai_api.h                                                   |
|  - Input validation, init guard, config parsing             |
|  - Exception boundary (try/catch -> XpeErrorCode)           |
|  - Thread safety with atomics (fallbackMode, initialized)   |
+-----------------------------------------------------------+
|  Fallback Router Layer                                      |
|  ai.cpp (routing logic)                                     |
|  - Confidence threshold check                               |
|  - Fallback mode toggle                                     |
|  - Stub vs full build dispatch                              |
+-----------------------------------------------------------+
|  IPC Bridge Layer                                           |
|  ai_worker_protocol.h (protocol definitions)                |
|  - Named pipe communication                                 |
|  - Message envelope (32-byte header + JSON payload)         |
|  - Request/response serialization                           |
|  **[SKELETON: protocol defined, bridge not implemented]**   |
+-----------------------------------------------------------+
|  Worker Process Layer                                       |
|  xpe_ai_worker.exe (separate binary)                        |
|  - ONNX Runtime session management                          |
|  - Model loading and inference execution                    |
|  - Heartbeat monitoring                                     |
|  **[DEFERRED: not built in stub mode]**                      |
+-----------------------------------------------------------+
```

---

## 3. Process Architecture

### 3.1 Worker Isolation Model

```
+-------------------+     Named Pipe      +--------------------+
|  xpe_ai.dll       | <-- IPC (JSON) --> |  xpe_ai_worker.exe |
|  (in-process)     |   \\.\pipe\        |  (isolated process) |
|                   |   xpe_ai_worker_   |                    |
|  - C ABI proxy    |   {PID}            |  - ONNX Runtime    |
|  - Input validation|                   |  - Model loading   |
|  - Fallback router |                   |  - Inference exec  |
|  - Model registry  |                   |  - Crash isolation |
+-------------------+                     +--------------------+
       |                                          |
       v                                          v
  Caller process                            GPU / TensorRT
  (ImageProcTest.exe                        (via ONNX EP)
   or orchestrator)
```

### 3.2 IPC Protocol

**Message Envelope (32-byte fixed header)**:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | magic | 0x58504541 ("XPEA") |
| 4 | 4 | version | Protocol version (major << 16 \| minor) |
| 8 | 4 | messageType | XpeAiMessageType enum value |
| 12 | 4 | requestId | Monotonically increasing request counter |
| 16 | 4 | payloadSize | Size of following JSON/binary payload |
| 20 | 4 | flags | Message flags (low_confidence, timeout, etc.) |
| 24 | 8 | timestamp | POSIX timestamp (milliseconds) |
| 32 | 8 | reserved | Must be zero |

**Message Types**:

| Type | Value | Direction | Description |
|------|-------|-----------|-------------|
| INIT | 1 | DLL -> Worker | Initialize ONNX Runtime |
| INIT_RESPONSE | 2 | Worker -> DLL | Init result + loaded models |
| SHUTDOWN | 3 | DLL -> Worker | Graceful shutdown |
| HEARTBEAT | 4 | Worker -> DLL | Heartbeat ping |
| HEARTBEAT_ACK | 5 | DLL -> Worker | Heartbeat acknowledgement |
| BODYPART_RECOGNIZE | 10 | DLL -> Worker | Body part classification request |
| BODYPART_RECOGNIZE_RESP | 11 | Worker -> DLL | Classification result |
| STITCH_IMAGES | 12 | DLL -> Worker | Stitching request |
| STITCH_IMAGES_RESP | 13 | Worker -> DLL | Stitching result |
| BONE_SUPPRESS | 14 | DLL -> Worker | Bone suppression request |
| BONE_SUPPRESS_RESP | 15 | Worker -> DLL | Suppression result |
| DL_DENOISE | 16 | DLL -> Worker | DL denoising request |
| DL_DENOISE_RESP | 17 | Worker -> DLL | Denoising result |
| GET_MODEL_CARD | 20 | DLL -> Worker | Model card query |
| GET_MODEL_CARD_RESP | 21 | Worker -> DLL | Model card JSON |
| ERROR | 99 | Worker -> DLL | Generic error |

**Protocol Constants**:

| Constant | Value | Purpose |
|----------|-------|---------|
| XPE_AI_MAX_PAYLOAD_SIZE | 64 MB | Maximum payload per message |
| XPE_AI_DEFAULT_TIMEOUT_MS | 5000 | Default IPC timeout |
| XPE_AI_DEFAULT_CONFIDENCE_THRESHOLD | 0.6 | Fallback confidence threshold |
| XPE_AI_PIPE_BUFFER_SIZE | 65536 | Named pipe buffer size |

### 3.3 Worker State Machine

```
IDLE --> BUSY --> IDLE           (normal operation)
  |                  |
  v                  v
LOADING            ERROR        (model loading / error states)
  |                  |
  v                  v
IDLE              SHUTTING_DOWN  (graceful termination)
```

---

## 4. Detailed Design

### 4.1 C ABI Layer

**Header**: `modules/ai/include/xpe/ai/ai_api.h`

Exported functions (9 total):

| # | Function | Category | Thread Safety |
|---|----------|----------|---------------|
| 1 | `xpe_ai_version()` | Lifecycle | Thread-safe (static) |
| 2 | `xpe_ai_init()` | Lifecycle | Not thread-safe |
| 3 | `xpe_ai_shutdown()` | Lifecycle | Not thread-safe |
| 4 | `xpe_bodypart_recognize()` | Inference | Reentrant |
| 5 | `xpe_stitch_images()` | Inference | Reentrant |
| 6 | `xpe_stitch_estimate_size()` | Utility | Reentrant |
| 7 | `xpe_bone_suppress()` | Inference | Reentrant |
| 8 | `xpe_dl_denoise()` | Inference | Reentrant |
| 9 | `xpe_ai_get_model_card()` | Transparency | Thread-safe (read-only) |
| 10 | `xpe_ai_set_fallback_mode()` | Configuration | Thread-safe (atomic) |

### 4.2 Internal State

```cpp
// Module-level state (ai.cpp internal linkage)
static std::atomic<bool> g_initialized{false};
static std::atomic<bool> g_fallbackMode{true};
static std::mutex g_initMutex;
static float g_confidenceThreshold{0.6f};
static std::unordered_map<std::string, ModelInfo> g_modelRegistry;
```

- `g_initialized`: Atomic flag checked by all inference functions without lock
- `g_fallbackMode`: Atomic flag controlling fallback routing behavior
- `g_initMutex`: Protects init/shutdown and model registry access only
- `g_confidenceThreshold`: Default 0.6, configurable via init JSON
- `g_modelRegistry`: Model ID -> metadata mapping (thread-safe reads)

### 4.3 Fallback Routing Design

```
xpe_bodypart_recognize(img, out, bufLen, conf)
    |
    +-- if (!g_initialized) return NOT_INITIALIZED
    +-- if (img == null || out == null) return INVALID_INPUT
    +-- if (bufLen == 0) return BUFFER_TOO_SMALL
    |
    +-- if (STUB_BUILD)
    |       return PROCESSING_FAILED    // triggers caller fallback
    |
    +-- [FULL BUILD]
    |   +-- Send IPC request to worker
    |   +-- Wait for response (timeout: 5s)
    |   +-- if (timeout) return PROCESSING_FAILED
    |   +-- if (worker crash) return PROCESSING_FAILED
    |   +-- if (confidence < threshold):
    |   |       if (g_fallbackMode) return PROCESSING_FAILED
    |   |       else return result + LOW_CONFIDENCE flag
    |   +-- return result
```

### 4.4 Stitch Size Estimation (Deterministic)

This function operates independently of AI inference:

```
xpe_stitch_estimate_size(parts, count, w, h):
    maxW = max(parts[i].width)
    maxH = max(parts[i].height)
    *w = min(maxW * (1 + 0.7 * (count - 1)), 4096)
    *h = min(maxH, 4096)
```

No worker process, no IPC, no ONNX Runtime needed. Always available.

### 4.5 Model Card JSON Structure

```json
{
  "model_id": "bone_suppress_v1",
  "model_version": "1.0.0",
  "intended_use": "Soft-tissue visualization by suppressing bony structures",
  "training_data_summary": "DES paired data, 5000 images, 3 institutions",
  "demographic_performance": {
    "overall_sensitivity": 0.95,
    "age_groups": { "18-40": 0.96, "41-65": 0.94, "65+": 0.93 }
  },
  "limitations": "Not validated for pediatric imaging",
  "pccp_status": "within_boundary",
  "published_date": "2026-04-01",
  "training_data_hash": "sha256:...",
  "validation_metrics": { "psnr": 42.1, "ssim": 0.98 }
}
```

**[SKELETON]** In stub mode, model card returns placeholder values with `pccp_status: "stub"`.

---

## 5. Build System Design

### 5.1 CMake Configuration

**File**: `modules/ai/CMakeLists.txt`

```
Option                         Default    Effect
------------------------------- ---------- -----------------------------------
XPE_AI_USE_ONNXRUNTIME         OFF        Enable ONNX Runtime linkage
XPE_AI_USE_SPDLOG              AUTO       Use spdlog for logging
XPE_AI_USE_NLOHMANN_JSON       AUTO       Use nlohmann/json for parsing

Derived:
XPE_AI_STUB_BUILD              1          Set when ONNX Runtime unavailable
```

### 5.2 Build Targets

| Target | Type | Built When | Links |
|--------|------|------------|-------|
| `xpe_ai` | SHARED DLL | Always | xpe_common, (conditional: onnxruntime, spdlog, nlohmann_json) |
| `xpe_ai_worker` | EXECUTABLE | ONNX Runtime available | xpe_common, onnxruntime |

### 5.3 Test Targets

| Target | Type | Location |
|--------|------|----------|
| `test_ai` | EXECUTABLE | `tests/ai_tests/` |
| Links: xpe_ai, xpe_common, GTest::gtest, GTest::gtest_main |

---

## 6. Module Dependency Diagram

```
                    +-----------------+
                    | xpe_common.dll  |
                    | (Layer 0)       |
                    +--------+--------+
                             |
              +--------------+---------------+
              |                              |
    +---------+----------+         +---------+----------+
    | xpe_ai.dll         |         | xpe_ai_worker.exe  |
    | (Layer 1, proxy)   |  <IPC>  | (isolated process) |
    |                     |         |                     |
    | - C ABI boundary   |         | - ONNX Runtime     |
    | - Fallback router  |         | - Model loading    |
    | - Model registry   |         | - Inference exec   |
    | - Input validation |         | - Heartbeat        |
    +---------+----------+         +---------+----------+
              |                              |
              v                              v
    +-------------------+          +-------------------+
    | Caller process    |          | GPU (via EP)      |
    | ImageProcTest.exe |          | CPU / CUDA /      |
    | or orchestrator   |          | TensorRT / DML    |
    +-------------------+          +-------------------+

Dependencies (xpe_ai.dll only):
  xpe_common.dll     -- Always
  onnxruntime         -- XPE_AI_USE_ONNXRUNTIME=ON
  spdlog              -- XPE_AI_USE_SPDLOG=1
  nlohmann_json       -- XPE_AI_USE_NLOHMANN_JSON=1
  fmt                 -- Always (via xpe_common)
```

---

## 7. Thread Safety Design

### 7.1 Concurrency Model

| Component | Protection | Rationale |
|-----------|-----------|-----------|
| `g_initialized` | `std::atomic<bool>` | Lock-free check in hot path |
| `g_fallbackMode` | `std::atomic<bool>` | Lock-free toggle, thread-safe |
| `g_modelRegistry` | `std::mutex` (init/shutdown only) | Write-once, read-many |
| Inference functions | Reentrant by design | No shared mutable state |
| IPC channel | Per-request serialization | One request at a time per pipe |

### 7.2 Thread Safety Test Results (Stub Mode)

| Test | Threads | Calls | Result |
|------|---------|-------|--------|
| Concurrent bodypart_recognize | 4 | 100 (25x4) | All return PROCESSING_FAILED |
| Concurrent set_fallback_mode | 4 | 400 (100x4) | All return XPE_OK |

---

## 8. Error Handling Design

### 8.1 Error Code Mapping

| Condition | Error Code | Fallback? |
|-----------|-----------|-----------|
| Module not initialized | `XPE_ERR_NOT_INITIALIZED` | No (caller error) |
| NULL input parameter | `XPE_ERR_INVALID_INPUT` | No (caller error) |
| Buffer too small | `XPE_ERR_BUFFER_TOO_SMALL` | No (caller error) |
| Worker unavailable (stub) | `XPE_ERR_PROCESSING_FAILED` | Yes |
| Worker timeout | `XPE_ERR_PROCESSING_FAILED` | Yes |
| Worker crash | `XPE_ERR_PROCESSING_FAILED` | Yes |
| Low confidence | `XPE_ERR_PROCESSING_FAILED` | Yes (if fallback mode) |
| Model not found | `XPE_ERR_IO_FAILED` | No (configuration error) |

### 8.2 Exception Boundary

All C ABI functions wrap their implementation in try/catch:

```cpp
extern "C" XPE_API XpeErrorCode xpe_bodypart_recognize(...) {
    try {
        // implementation
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
```

---

## 9. Deferred Design Elements

The following design elements are planned but not yet implemented:

| Element | Priority | Planned For |
|---------|----------|-------------|
| IPC Bridge (named pipe) | Must | Phase 3 |
| Worker process (xpe_ai_worker.exe) | Must | Phase 3 |
| ONNX Runtime session management | Must | Phase 3 |
| Model signing/verification (Ed25519/ECDSA) | Must | Phase 3 |
| Heartbeat monitoring | Should | Phase 3 |
| Crash recovery (automatic worker restart) | Must | Phase 3 |
| Model loading from disk | Must | Phase 3 |
| XAI sidecar (Grad-CAM, SHAP) | Should | Phase 3+ |
| Conformal Prediction UQ | Should | Phase 3+ |
| PCCP boundary enforcement | Must (conditional) | Phase 3 |
| Drift detection integration | Should | Phase 3+ |
| SSL denoising models | Should | Phase 3 |
| Diffusion prior models | Should | Phase 3+ |

---

## 10. File Structure

```
modules/ai/
  include/xpe/ai/
    ai_api.h              -- Public C API (9 exported functions)
    ai_worker_protocol.h  -- IPC protocol definitions
  src/
    ai.cpp                -- Skeleton implementation (stub + routing)
  CMakeLists.txt          -- Build configuration (stub/full modes)

tests/ai_tests/
  CMakeLists.txt           -- Test target registration
  test_ai_abi.cpp          -- 24 ABI smoke tests
  test_ai_fallback.cpp     -- 23 Fallback routing tests
  test_ai_model_card.cpp   -- 17 Model Card API tests
  test_ai_worker_isolation.cpp -- 14 Process isolation tests
```

---

## 11. Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1.0 | 2026-04-22 | xpe-docs | Initial SDD for skeleton implementation. Architecture, IPC protocol, fallback routing design. |

---

*This document satisfies IEC 62304 Class B requirements for software architectural design (Section 5.3) and detailed design (Section 5.4).*
