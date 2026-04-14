# 소프트웨어 아키텍처 문서

**문서 ID:** XPE-SAD-001 v1.0  
**IEC 62304 Clause:** 5.3.1 — 5.3.6  
**안전 분류:** Class B  
**날짜:** 2026-04-03  
**작성자:** XPE 개발 팀  
**승인:** __________________ 날짜: __________  

---

## 1. 목적

XPE 소프트웨어 시스템의 아키텍처를 정의한다. SW requirements(XPE-SRS-001)를 software items로 분해하고, 인터페이스, SOUP 요구사항, risk control을 위한 segregation을 명시한다.

## 2. 아키텍처 개요 (5.3.1)

XPE는 **Pipeline Architecture** 패턴을 사용한다. 논리적 software item은 IEC 62304 관점의 SWI 기준으로 유지하되, 물리 배포는 DLL 단위로 분리한다. 특히 SWI-2 Core Processing은 `xpe_enhance_basic.dll`, `xpe_enhance_advanced.dll`, `xpe_ai.dll`의 3개 구현 파티션으로 분할되며, `xpe_ai.dll`은 sandbox worker process(`xpe_ai_worker.exe`)에 대한 IPC proxy 역할만 수행한다.

```
┌─────────────────────────────────────────────────────────┐
│                  XPE Software System                     │
│                                                         │
│  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌─────────┐│
│  │   Pre-    │→│   Core    │→│  Display  │→│  DICOM  ││
│  │Processing │ │Processing │ │Processing │ │   I/O   ││
│  │  (SWI-1)  │ │  (SWI-2)  │ │  (SWI-3)  │ │ (SWI-4) ││
│  └───────────┘ └───────────┘ └───────────┘ └─────────┘│
│        ↑              ↑              ↑           ↑      │
│  ┌──────────────────────────────────────────────────┐  │
│  │           Common Infrastructure (SWI-5)           │  │
│  │    Memory Pool │ Thread Pool │ Error Handler      │  │
│  │    Logger │ Parameter Validator │ Config Manager   │  │
│  └──────────────────────────────────────────────────┘  │
│        ↑                                                │
│  ┌──────────────────────────────────────────────────┐  │
│  │            SOUP Components (SWI-6)                │  │
│  │    OpenCV │ dcmtk │ ONNX Runtime │ Eigen │ spdlog │  │
│  └──────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

## 3. Software Items

| SW Item ID | Name | Safety Class | SRS Coverage |
|-----------|------|:----------:|-------------|
| SWI-1 | Pre-Processing Module | B | SRS-FUNC-001..004 |
| SWI-2 | Core Processing Module | B | SRS-FUNC-010..018 |
| SWI-3 | Display Processing Module | B | SRS-FUNC-020..023 |
| SWI-4 | DICOM I/O Module | B | SRS-FUNC-030..032 |
| SWI-5 | Common Infrastructure | B | SRS-SAFE-001..009, SRS-PERF-* |
| SWI-6 | SOUP Components | — | See XPE-SOUP-001 |

### 3.1 SWI-1: Pre-Processing Module

**책임:** Detector raw data로부터 물리적 결함을 제거하여 clean image를 생성한다.

| Sub-Item | Function | Input | Output |
|----------|----------|-------|--------|
| Offset Correction | Dark current 제거 | Raw + Offset Map | Offset-corrected image |
| Gain Correction | Sensitivity 균일화 | Above + Gain Map | Flat-field corrected image |
| Defect Pixel Correction | Bad pixel 보간 | Above + Bad Pixel Map | Clean image |
| Ghost Correction | Lag signal 제거 | Above + previous frame info | Final pre-processed image |

### 3.2 SWI-2: Core Processing Module

**책임:** Pre-processed image에 영상 품질 개선 알고리즘을 적용한다.

| Sub-Item | Function | Phase | Implementation Partition |
|----------|----------|:-----:|--------------------------|
| Log Transform | Linear → OD domain | 1 | `xpe_enhance_basic.dll` |
| Noise Reducer | Edge-preserving denoising | 1 | `xpe_enhance_basic.dll` |
| Contrast Enhancer | CLAHE | 1 | `xpe_enhance_basic.dll` |
| Edge Enhancer | Unsharp masking | 1 | `xpe_enhance_basic.dll` |
| Multiscale Processor | Laplacian pyramid MFP | 2 | `xpe_enhance_advanced.dll` |
| Fractional Processor | Fractional multiscale enhancement | 2 | `xpe_enhance_advanced.dll` |
| Collimation Detector | Beam edge / ROI detection | 2 | `xpe_enhance_advanced.dll` |
| Exposure Index Calculator | IEC 62494-1 EI/DI, ROI-aware refinement | 2 | `xpe_enhance_advanced.dll` |
| Body-Part Recognizer | CNN classifier | 2 | `xpe_ai.dll` -> `xpe_ai_worker.exe` |
| Image Stitcher | Panoramic stitching | 2 | `xpe_ai.dll` -> `xpe_ai_worker.exe` |
| Bone Suppression Engine | DL U-Net inference | 3 | `xpe_ai.dll` -> `xpe_ai_worker.exe` |
| DL Denoiser | Learned low-dose denoising | 3 | `xpe_ai.dll` -> `xpe_ai_worker.exe` |

### 3.3 SWI-3: Display Processing Module

**책임:** DICOM Grayscale Pipeline에 따라 presentation-ready 출력을 생성한다.

| Sub-Item | Function | DICOM Reference |
|----------|----------|-----------------|
| Modality LUT | Rescale Slope/Intercept | (0028,1053)/(0028,1052) |
| VOI LUT | W/L (Linear, Sigmoid, LUT Seq) | (0028,1050)/(0028,1051) |
| Presentation LUT | GSDF P-Values | PS3.14 |
| LUT Manager | Preset storage/selection | — |

### 3.4 SWI-4: DICOM I/O Module

**책임:** DICOM 파일 읽기/쓰기, 네트워크 전송, Presentation State 관리.

### 3.5 SWI-5: Common Infrastructure

**책임:** 모든 module이 공유하는 cross-cutting concerns.

| Sub-Item | Function | Safety Relevance |
|----------|----------|-----------------|
| MemoryPool | Pre-allocated image buffer pool | SRS-SAFE-001 (원본 보존) |
| ThreadPool | Task-based parallel execution | SRS-PERF-* |
| ErrorHandler | Centralized error management | SRS-SAFE-003, SRS-ALERT-* |
| Logger | Audit trail (spdlog) | SRS-SEC-003 |
| ParameterValidator | Safe-range enforcement | SRS-SAFE-002, 005 |
| ConfigManager | System/user config persistence | — |

## 4. Interface Specification (5.3.2)

### 4.1 Internal Interfaces

| IF ID | From → To | Data | Mechanism |
|-------|-----------|------|-----------|
| IF-INT-001 | SWI-1 → SWI-2 | ImageBuffer (float32) | Shared memory pointer |
| IF-INT-002 | SWI-2 → SWI-3 | ImageBuffer (float32) | Shared memory pointer |
| IF-INT-003 | SWI-3 → SWI-4 | ImageBuffer (uint16) + metadata | Shared memory + struct |
| IF-INT-004 | SWI-5 ↔ All | Error codes, log entries, config | Function call / callback |

### 4.2 ImageBuffer Specification

```cpp
struct ImageBuffer {
    uint32_t width;
    uint32_t height;
    uint32_t bitsAllocated;   // 16 or 32
    uint32_t bitsStored;      // 14, 16, or 32
    PixelFormat format;       // UINT16, FLOAT32
    void* data;               // pixel data pointer (non-owning)
    size_t dataSize;          // bytes
    ImageMetadata metadata;   // exposure, body part, detector info
};

struct ImageMetadata {
    std::string bodyPart;     // DICOM (0018,0015)
    float kVp;
    float mAs;
    float SID_mm;
    float pixelPitch_mm;
    uint64_t acquisitionTime; // epoch ms
    bool ghostCorrectionApplied;
    bool aiProcessed;
};
```

### 4.3 External Interfaces

| IF ID | Interface | Protocol | Error Handling |
|-------|-----------|----------|---------------|
| IF-EXT-001 | Detector SDK | Vendor C API | Timeout 3× retry → error state |
| IF-EXT-002 | PACS (C-STORE) | DICOM v3.0 SCU | Association fail → queue + retry |
| IF-EXT-003 | GUI | C ABI DLL | Exception → error code return |
| IF-EXT-004 | CAD Plugin | REST + ONNX | Timeout → fallback (non-AI) |

## 5. SOUP Requirements (5.3.3, 5.3.4)

상세 내용은 XPE-SOUP-001 참조. 요약:

| SOUP | Functional Requirement | HW/SW Requirement |
|------|----------------------|-------------------|
| OpenCV 4.9 | bilateralFilter, CLAHE, pyrDown/Up | x86-64(AVX2), ARM(NEON) |
| dcmtk 3.6.8 | DX IOD read/write, C-STORE, J2K | OpenSSL |
| ONNX Runtime 1.17 | Model load + inference | CUDA 12 (optional) |
| Eigen 3.4 | Matrix ops, FFT | Cross-platform |

## 6. Segregation for Risk Control (5.3.5)

| Risk Control | Segregation Method |
|-------------|-------------------|
| 원본 data 보존 | SWI-1은 input buffer를 read-only 접근. 별도 output buffer 사용. MemoryPool이 ownership 관리. |
| Processing error isolation | 각 SWI는 독립 error domain. Exception이 module boundary를 넘지 않음. |
| DL processing 격리 | `xpe_ai.dll`은 in-process proxy만 담당하고 실제 추론은 별도 process(sandbox) `xpe_ai_worker.exe`에서 실행. IPC로 결과 전달. AI process crash 시 main pipeline 영향 없음. |
| Parameter validation | 모든 parameter는 SWI-5 ParameterValidator를 통해 safe range 검증 후 적용. |
| Data integrity | Pipeline 각 stage 출력에 checksum 기록. 다음 stage 입력 시 검증. |

## 7. Architecture Verification (5.3.6)

| Verification Item | Method | Pass Criteria |
|-------------------|--------|---------------|
| 모든 SRS req → architecture 매핑 | RTM review | 100% coverage |
| Interface 정의 완전성 | Formal review | 모든 data flow documented |
| SOUP 적합성 | XPE-SOUP-001 review | 모든 SOUP 요구사항 충족 |
| Risk control 반영 | Design review | 모든 SRS-SAFE-xxx → architecture에 반영 |
| RadiConsole™ GUI 연동 | Interface review | WPF/C# P/Invoke 호환 확인 |
| Reviewer sign-off | Formal review | ≥ 2 reviewers approve |

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-03 | XPE Team | Initial release |

---

*Document End — XPE-SAD-001 v1.0*
