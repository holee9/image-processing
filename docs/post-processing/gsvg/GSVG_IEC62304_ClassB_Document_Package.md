# X-ray Grid Suppression & Virtual Grid Software

## IEC 62304 Class B Compliant Document Package

**Document ID:** GSVG-PKG-001  
**Version:** 1.0  
**Date:** 2026-04-02  
**Safety Classification:** IEC 62304 Class B (Non-serious injury possible)  
**Applicable Standards:** IEC 62304:2015, ISO 14971:2019, IEC 62366-1:2015

---

# Document Index

| Doc ID | Title | IEC 62304 Clause |
|--------|-------|------------------|
| GSVG-SDP-001 | Software Development Plan | 5.1 |
| GSVG-SRS-001 | Software Requirements Specification | 5.2 |
| GSVG-SAD-001 | Software Architecture Design | 5.3 |
| GSVG-SDD-001 | Software Detailed Design | 5.4 (voluntary for Class B) |
| GSVG-SVP-001 | Software Verification Plan | 5.5–5.7 |
| GSVG-SOUP-001 | SOUP Analysis | 5.3.3 |
| GSVG-SHA-001 | Software Hazard Analysis | 7 |
| GSVG-RTM-001 | Requirements Traceability Matrix | 5.7 |

---

# GSVG-SDP-001: Software Development Plan

## 1. Purpose

본 문서는 X-ray FPD 시스템의 Grid Suppression 및 Virtual Grid 소프트웨어 모듈 개발을 위한 IEC 62304 Class B 준수 개발 계획을 정의한다.

## 2. Software Safety Classification

**Class B** — 소프트웨어 오동작 시 non-serious injury 가능.

근거:
- Grid suppression 실패 시: grid artifact가 잔류하여 진단 정보 일부 가려짐 → 재촬영 필요
- Virtual grid 실패 시: scatter 미보정으로 contrast 저하 → 미세 병변 가시성 감소
- 직접적 방사선 과다 노출이나 치료 영향은 없음 (영상 표시 목적)
- 하드웨어 안전 장치 (exposure interlock)가 독립적으로 존재

## 3. Lifecycle Model

V-Model 적용 (IEC 62304 권장 구조)

```mermaid
graph LR
    A[System Requirements] --> B[Software Requirements<br/>SRS]
    B --> C[Architecture Design<br/>SAD]
    C --> D[Detailed Design<br/>SDD]
    D --> E[Implementation]
    E --> F[Unit Testing]
    F --> G[Integration Testing]
    G --> H[System Testing]
    H --> I[Release]
    
    B -.->|traces to| H
    C -.->|traces to| G
    D -.->|traces to| F
```

## 4. Class B Required Activities

```mermaid
graph TD
    subgraph "IEC 62304 Class B Requirements"
        P[5.1 Development Planning] --> R[5.2 Requirements Analysis]
        R --> A[5.3 Architecture Design]
        A --> I[5.5 Unit Implementation]
        I --> IT[5.6 Integration Testing]
        IT --> ST[5.7 System Testing]
        ST --> REL[5.8 Release]
    end
    
    subgraph "Class B Mandatory Deliverables"
        D1[SRS Document]
        D2[Architecture Document]
        D3[SOUP List & Analysis]
        D4[Integration Test Records]
        D5[System Test Records]
        D6[Release Notes]
    end
    
    subgraph "Voluntary but Recommended"
        D7[Detailed Design Document]
        D8[Unit Test Records]
        D9[Code Review Records]
    end
    
    R --> D1
    A --> D2
    A --> D3
    IT --> D4
    ST --> D5
    REL --> D6
```

## 5. Development Tools & Environment

| Category | Tool | Version | Purpose |
|----------|------|---------|---------|
| Language | C++ | C++17 | Core algorithm implementation |
| Language | Python | 3.11+ | Prototyping, test automation |
| Build | CMake | 3.25+ | Cross-platform build |
| VCS | Git / Gitea | Latest | Configuration management |
| CI/CD | Gitea Actions | Latest | Automated build & test |
| Testing | Google Test | 1.14+ | Unit & integration testing |
| Testing | pytest | 8.0+ | Python test automation |
| Static Analysis | cppcheck, clang-tidy | Latest | MISRA-like rule checking |
| Documentation | Markdown + Mermaid | — | All design documents |
| Image Processing | OpenCV | 4.9+ | SOUP — image I/O, basic ops |
| FFT | FFTW3 | 3.3.10 | SOUP — frequency domain ops |
| Math | Eigen | 3.4+ | SOUP — linear algebra |

## 6. Configuration Management

- Branch strategy: `main` (release) / `develop` (integration) / `feature/*` (개발)
- Commit message: Conventional Commits (`feat:`, `fix:`, `test:`, `docs:`)
- Tag: Semantic versioning `vMAJOR.MINOR.PATCH`
- Code review: All merge to `develop` requires 1+ reviewer approval
- Baseline: 각 milestone에서 tag 생성 및 문서 동결

## 7. Problem Resolution

- Gitea Issues로 모든 anomaly/defect 추적
- Severity: Critical / Major / Minor / Cosmetic
- Safety-related issue는 SHA(GSVG-SHA-001)와 cross-reference
- 미해결 anomaly는 release 시 잔여 위험으로 문서화

## 8. Schedule

```mermaid
gantt
    title GSVG Development Schedule
    dateFormat  YYYY-MM-DD
    
    section Phase 1: Grid Suppression
    SRS & Architecture           :p1a, 2026-04-07, 14d
    Core Algorithm (DWT+BandStop):p1b, after p1a, 21d
    Unit & Integration Test      :p1c, after p1b, 14d
    System Test & Validation     :p1d, after p1c, 7d
    
    section Phase 2: Virtual Grid
    SRS & Architecture           :p2a, 2026-04-14, 14d
    Scatter Model Implementation :p2b, after p2a, 28d
    Laplacian Pyramid Pipeline   :p2c, after p2b, 14d
    Unit & Integration Test      :p2d, after p2c, 14d
    System Test & Validation     :p2e, after p2d, 7d
    
    section Phase 3: Integration
    Combined Pipeline            :p3a, after p2e, 14d
    Final System Test            :p3b, after p3a, 7d
    Release                      :milestone, after p3b, 0d
```

---

# GSVG-SRS-001: Software Requirements Specification

## 1. Scope

X-ray flat panel detector 시스템 영상에 대한 두 가지 독립 기능:
1. **Grid Suppression (GS)**: 물리적 anti-scatter grid 사용 영상의 grid line artifact 제거
2. **Virtual Grid (VG)**: Grid 미사용 영상의 scatter radiation 소프트웨어 보정

## 2. Functional Requirements — Grid Suppression

| ID | Requirement | Verification |
|----|-------------|--------------|
| GS-FR-001 | 시스템은 DICOM 헤더 및 grid specification으로부터 grid line frequency를 자동 계산해야 한다 | Test |
| GS-FR-002 | 시스템은 2D DWT (Discrete Wavelet Transform)를 사용하여 입력 영상을 multi-scale sub-band로 분해해야 한다 | Test |
| GS-FR-003 | 시스템은 각 sub-band에서 gridline signal energy가 threshold를 초과하는지 자동 검출해야 한다 | Test |
| GS-FR-004 | 검출된 sub-band에 Gaussian band-stop filter를 적용하여 gridline signal을 제거해야 한다 | Test |
| GS-FR-005 | Inverse DWT로 복원된 영상은 gridline artifact가 시각적으로 인지 불가해야 한다 | Test + Review |
| GS-FR-006 | 처리 후 영상의 MTF 저하는 원본 대비 5% 이내여야 한다 | Test |
| GS-FR-007 | 60~200 lines/inch 범위의 grid에 대해 동작해야 한다 | Test |
| GS-FR-008 | Moiré pattern (aliasing artifact) 제거를 지원해야 한다 | Test |

## 3. Functional Requirements — Virtual Grid

| ID | Requirement | Verification |
|----|-------------|--------------|
| VG-FR-001 | 시스템은 입력 영상과 촬영 조건(kVp, mAs, SID, field size)으로부터 body equivalent thickness를 추정해야 한다 | Test |
| VG-FR-002 | 시스템은 추정된 thickness와 촬영 조건으로부터 Scatter-to-Primary Ratio (SPR)를 계산해야 한다 | Test |
| VG-FR-003 | 시스템은 사전 계산된 scatter kernel LUT를 사용하여 scatter distribution을 추정해야 한다 | Test |
| VG-FR-004 | 추정된 scatter를 원본 영상에서 차감하여 primary-only 영상을 생성해야 한다 | Test |
| VG-FR-005 | Laplacian Pyramid decomposition으로 multi-scale contrast enhancement를 수행해야 한다 | Test |
| VG-FR-006 | 고주파 band에 대해 de-noising을 수행해야 한다 | Test |
| VG-FR-007 | 출력 영상의 CNR은 동일 조건 6:1 physical grid 영상 대비 90% 이상이어야 한다 | Test |
| VG-FR-008 | 사용자가 가상 grid ratio (6:1, 8:1, 10:1, 12:1)를 선택할 수 있어야 한다 | Test |
| VG-FR-009 | 10cm~30cm acrylic thickness 범위에서 동작해야 한다 | Test |
| VG-FR-010 | 처리 후 인체 구조물에 인위적 artifact가 생성되지 않아야 한다 | Test + Review |

## 4. Performance Requirements

| ID | Requirement | Verification |
|----|-------------|--------------|
| PERF-001 | 3072×3072 16-bit 영상 처리 시간 ≤ 1.0초 (target HW: Intel i7 또는 동급) | Test |
| PERF-002 | Peak memory usage ≤ 512MB per frame | Test |
| PERF-003 | Batch mode에서 연속 100 frame 처리 시 memory leak 없음 | Test |
| PERF-004 | 출력 영상 bit depth는 입력과 동일 (16-bit) | Test |

## 5. Interface Requirements

| ID | Requirement | Verification |
|----|-------------|--------------|
| IF-001 | 입력: DICOM 형식 또는 Raw pixel array + metadata | Test |
| IF-002 | 출력: 동일 format의 처리된 영상 + processing log | Test |
| IF-003 | 에러 발생 시 원본 영상을 unmodified로 pass-through | Test |
| IF-004 | Processing parameters를 JSON config로 설정 가능 | Test |
| IF-005 | API는 C++ shared library (.so/.dll) 형태로 제공 | Test |

## 6. Safety Requirements

| ID | Requirement | Hazard Ref | Verification |
|----|-------------|------------|--------------|
| SAFE-001 | 알고리즘 실패 시 원본 영상을 훼손하지 않아야 한다 | HAZ-001 | Test |
| SAFE-002 | 처리 영상에 "Processed" marking을 DICOM tag에 기록해야 한다 | HAZ-002 | Test |
| SAFE-003 | 처리 실패 시 에러 코드와 함께 원본 영상을 반환해야 한다 | HAZ-001 | Test |
| SAFE-004 | Scatter correction 강도가 물리적으로 불가능한 값을 초과하지 않도록 clamping | HAZ-003 | Test |
| SAFE-005 | 출력 영상의 pixel value 범위는 유효 DICOM 범위 내여야 한다 | HAZ-004 | Test |

---

# GSVG-SAD-001: Software Architecture Design

## 1. System Context

```mermaid
graph TB
    subgraph "X-ray FPD System"
        ACQ[Image Acquisition<br/>FPGA/Firmware] --> RAW[Raw Image<br/>16-bit DICOM]
        RAW --> GSVG[Grid Suppression &<br/>Virtual Grid Module]
        GSVG --> PROC[Processed Image<br/>16-bit DICOM]
        PROC --> CONSOLE[Diagnostic Console<br/>RadiConsole™]
    end
    
    subgraph "External"
        CONFIG[Configuration<br/>JSON] --> GSVG
        LUT[Scatter Kernel<br/>LUT Files] --> GSVG
        LOG[Processing Log] --> GSVG
    end
```

## 2. Software Architecture — Top Level

```mermaid
graph TD
    subgraph "GSVG Software System"
        API[API Layer<br/>gsvg_api.h]
        
        subgraph "SI-001: Image Pipeline Manager"
            PM[Pipeline Manager]
            DETECT[Grid Detection Module]
        end
        
        subgraph "SI-002: Grid Suppression Engine"
            DWT[2D DWT Decomposer]
            GDET[Gridline Detector]
            BSF[Gaussian Band-Stop Filter]
            IDWT[Inverse DWT Reconstructor]
        end
        
        subgraph "SI-003: Virtual Grid Engine"
            THICK[Thickness Estimator]
            SPR[SPR Calculator]
            SCAT[Scatter Estimator<br/>(Kernel LUT)]
            SUB[Scatter Subtractor]
            LAP[Laplacian Pyramid<br/>Contrast Enhancer]
            DENOISE[Denoising Module]
        end
        
        subgraph "SI-004: Common Utilities"
            DICOM_IO[DICOM I/O]
            FFT_UTIL[FFT Utilities]
            IMG_UTIL[Image Utilities]
            VALID[Input Validator]
            ERR[Error Handler]
        end
    end
    
    API --> PM
    PM --> DETECT
    DETECT -->|grid detected| DWT
    DETECT -->|no grid| THICK
    
    DWT --> GDET
    GDET --> BSF
    BSF --> IDWT
    
    THICK --> SPR
    SPR --> SCAT
    SCAT --> SUB
    SUB --> LAP
    LAP --> DENOISE
    
    DWT -.-> FFT_UTIL
    BSF -.-> FFT_UTIL
    LAP -.-> IMG_UTIL
    PM -.-> DICOM_IO
    PM -.-> VALID
    PM -.-> ERR
```

## 3. Software Items Definition

| Item ID | Name | Description | Safety Class |
|---------|------|-------------|--------------|
| SI-001 | Image Pipeline Manager | 영상 입출력, 파이프라인 라우팅, 에러 처리 | B |
| SI-002 | Grid Suppression Engine | DWT 기반 grid artifact 검출 및 제거 | B |
| SI-003 | Virtual Grid Engine | Scatter estimation 및 contrast enhancement | B |
| SI-004 | Common Utilities | DICOM I/O, FFT, validation, error handling | B |

## 4. Grid Suppression Engine — Data Flow

```mermaid
flowchart LR
    IN[Input Image<br/>M×N, 16-bit] --> DWT_PROC[2D DWT<br/>Haar/Db4]
    
    DWT_PROC --> LH[LH sub-band<br/>M/2 × N/2]
    DWT_PROC --> HL[HL sub-band<br/>M/2 × N/2]
    DWT_PROC --> HH[HH sub-band<br/>M/2 × N/2]
    DWT_PROC --> LL[LL sub-band<br/>M/2 × N/2]
    
    LL -->|recursive| DWT_PROC
    
    LH --> ENERGY[Energy<br/>Detection]
    HL --> ENERGY
    HH --> ENERGY
    
    ENERGY -->|threshold exceeded| FILTER[Gaussian<br/>Band-Stop<br/>Filter]
    ENERGY -->|below threshold| PASS[Pass-through]
    
    FILTER --> RECON[Inverse DWT<br/>Reconstruction]
    PASS --> RECON
    
    RECON --> OUT[Output Image<br/>Grid-free]
    
    style FILTER fill:#ff9,stroke:#333
    style ENERGY fill:#9cf,stroke:#333
```

**핵심 알고리즘 파라미터:**

| Parameter | Value | Source |
|-----------|-------|--------|
| Wavelet basis | Daubechies-4 (db4) | Tang 2015, Med Phys |
| Max decomposition levels | `log₂(min(M,N)) - 4` | Adaptive |
| Gridline energy threshold | 3σ above mean sub-band energy | Tang 2015 |
| Band-stop filter bandwidth | ±2 pixels in frequency domain | Lin 2006 |
| Band-stop filter shape | Gaussian, σ = 1.5 pixels | Empirical |

## 5. Virtual Grid Engine — Data Flow

```mermaid
flowchart TD
    IN[Input Image<br/>+ DICOM metadata] --> THICK_EST[Thickness<br/>Estimation]
    
    subgraph "Scatter Estimation"
        THICK_EST --> SPR_CALC[SPR Calculation<br/>SPR = f(t, kVp, FOV)]
        SPR_CALC --> KERNEL[Scatter Kernel<br/>Selection from LUT]
        KERNEL --> SCATTER_MAP[Scatter Map<br/>Generation<br/>S = K ⊗ I_primary]
    end
    
    IN --> SUBTRACT[Scatter<br/>Subtraction<br/>I_p = I_total - S]
    SCATTER_MAP --> SUBTRACT
    
    subgraph "Laplacian Pyramid Processing"
        SUBTRACT --> LP_DEC[Laplacian Pyramid<br/>Decomposition<br/>n levels]
        LP_DEC --> LOW_BANDS[Low-freq bands:<br/>De-scatter<br/>residual correction]
        LP_DEC --> HIGH_BANDS[High-freq bands:<br/>Contrast enhance<br/>+ Denoise]
        LOW_BANDS --> LP_REC[Laplacian Pyramid<br/>Reconstruction]
        HIGH_BANDS --> LP_REC
    end
    
    LP_REC --> CLAMP[Output Clamping<br/>0 ~ 2¹⁶-1]
    CLAMP --> OUT[Output Image]
    
    style SCATTER_MAP fill:#fcc,stroke:#333
    style LP_DEC fill:#cfc,stroke:#333
```

**Laplacian Pyramid 구현 (US8064676B2 기반):**

```
Decomposition:
  g_{k+1}(x,y) = [g_k(x,y) * G_σ(x,y)]↓2     # Gaussian convolution + downsample
  L_k(x,y) = g_k(x,y) - [g_{k+1}(x,y)]↑2 * G_σ  # Differential (detail) image
  
  where: σ = 1.0, kernel = 5×5, n = log(N)/log(2) - 0.5

Processing:
  Low-freq (g_n):   g'_n = g_n × (1 + α × SPR_correction_factor)
  High-freq (L_k):  L'_k = L_k × β_k - noise_k
                     β_k = contrast_gain[grid_ratio][k]
                     noise_k = WienerFilter(L_k, σ_noise)

Reconstruction:
  g'_k = [g'_{k+1}]↑2 * G_σ + L'_k
```

**Scatter Kernel LUT 구조:**

| Axis | Range | Step | Description |
|------|-------|------|-------------|
| Thickness (t) | 5~35 cm | 1 cm | Water-equivalent thickness |
| kVp | 40~150 kVp | 10 kVp | Tube voltage |
| Field size | 10×10 ~ 43×43 cm | 5 cm step | Collimated field |
| Air gap | 0~20 cm | 5 cm | ODD (Object-to-Detector Distance) |

LUT 생성: GATE (Geant4) Monte Carlo simulation으로 사전 계산. 각 조건에서 scatter kernel을 4-Gaussian model로 fitting하여 저장.

## 6. SOUP Interfaces

```mermaid
graph LR
    subgraph "GSVG System"
        SI002[SI-002<br/>Grid Suppression]
        SI003[SI-003<br/>Virtual Grid]
        SI004[SI-004<br/>Utilities]
    end
    
    subgraph "SOUP Components"
        OCV[OpenCV 4.9<br/>Image I/O, resize,<br/>basic operations]
        FFTW[FFTW3 3.3.10<br/>FFT forward/inverse]
        EIGEN[Eigen 3.4<br/>Matrix operations]
        DCMTK[DCMTK 3.6.8<br/>DICOM read/write]
    end
    
    SI002 --> FFTW
    SI002 --> OCV
    SI003 --> OCV
    SI003 --> EIGEN
    SI004 --> DCMTK
    SI004 --> OCV
    SI004 --> FFTW
```

## 7. Error Handling Strategy

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Processing: processImage()
    Processing --> GridDetection: detect grid presence
    GridDetection --> GridSuppression: grid found
    GridDetection --> VirtualGrid: no grid
    GridDetection --> ErrorState: detection failure
    GridSuppression --> Success: OK
    GridSuppression --> ErrorState: algorithm failure
    VirtualGrid --> Success: OK
    VirtualGrid --> ErrorState: algorithm failure
    ErrorState --> PassThrough: return original image + error code
    PassThrough --> Idle
    Success --> Idle
```

**SAFE-001/003 구현**: 모든 알고리즘 진입점에서 원본 영상의 deep copy를 보관. 어떤 예외/에러 발생 시에도 원본 반환.

---

# GSVG-SDD-001: Software Detailed Design

> Note: IEC 62304 Class B에서 Detailed Design은 필수가 아니나, 코드 품질과 유지보수성을 위해 작성.

## 1. Module Structure (C++ Namespace)

```
gsvg/
├── api/
│   ├── gsvg_api.h              # Public C API
│   └── gsvg_types.h            # Common types, error codes
├── pipeline/
│   ├── PipelineManager.h/cpp   # SI-001
│   ├── GridDetector.h/cpp      # Grid presence detection
│   └── ProcessingConfig.h      # JSON config parser
├── grid_suppression/
│   ├── DwtDecomposer.h/cpp     # 2D DWT forward/inverse
│   ├── GridlineDetector.h/cpp  # Sub-band energy analysis
│   └── BandStopFilter.h/cpp    # Gaussian band-stop
├── virtual_grid/
│   ├── ThicknessEstimator.h/cpp
│   ├── SprCalculator.h/cpp
│   ├── ScatterEstimator.h/cpp  # Kernel LUT lookup + convolution
│   ├── LaplacianPyramid.h/cpp  # Decompose/reconstruct
│   └── Denoiser.h/cpp          # Wiener / bilateral
├── common/
│   ├── DicomIO.h/cpp
│   ├── FftUtils.h/cpp
│   ├── ImageBuffer.h/cpp       # RAII 16-bit image container
│   ├── Validator.h/cpp
│   └── ErrorHandler.h/cpp
└── tests/
    ├── unit/
    ├── integration/
    └── test_data/
```

## 2. Core Class Design

### 2.1 ImageBuffer (RAII Container)

```cpp
// gsvg/common/ImageBuffer.h
namespace gsvg {

class ImageBuffer {
public:
    ImageBuffer(uint32_t width, uint32_t height, uint16_t bitsAllocated = 16);
    ImageBuffer(const ImageBuffer& other);           // Deep copy
    ImageBuffer(ImageBuffer&& other) noexcept;       // Move
    ~ImageBuffer();
    
    // Pixel access with bounds checking
    uint16_t& at(uint32_t x, uint32_t y);
    const uint16_t& at(uint32_t x, uint32_t y) const;
    
    // Raw data access (for SOUP library interop)
    uint16_t* data() noexcept;
    const uint16_t* data() const noexcept;
    
    uint32_t width() const noexcept;
    uint32_t height() const noexcept;
    size_t sizeBytes() const noexcept;
    
    // Deep copy for safety requirement SAFE-001
    ImageBuffer deepCopy() const;

private:
    std::unique_ptr<uint16_t[]> data_;
    uint32_t width_, height_;
    uint16_t bitsAllocated_;
};

} // namespace gsvg
```

### 2.2 Processing Pipeline (Entry Point)

```cpp
// gsvg/api/gsvg_api.h
extern "C" {

typedef enum {
    GSVG_OK = 0,
    GSVG_ERR_INVALID_INPUT = -1,
    GSVG_ERR_GRID_DETECTION_FAILED = -2,
    GSVG_ERR_SUPPRESSION_FAILED = -3,
    GSVG_ERR_VIRTUAL_GRID_FAILED = -4,
    GSVG_ERR_OUT_OF_MEMORY = -5,
    GSVG_ERR_CONFIG_INVALID = -6,
} GsvgErrorCode;

typedef enum {
    GSVG_MODE_AUTO = 0,        // Auto-detect grid presence
    GSVG_MODE_GRID_SUPPRESS,   // Force grid suppression
    GSVG_MODE_VIRTUAL_GRID,    // Force virtual grid
} GsvgProcessingMode;

typedef struct {
    GsvgProcessingMode mode;
    float virtualGridRatio;     // 6.0, 8.0, 10.0, 12.0
    const char* configPath;     // JSON config file path
    const char* lutPath;        // Scatter kernel LUT directory
} GsvgConfig;

// Main processing function
// Returns GSVG_OK on success, error code on failure
// On failure, output buffer contains unmodified copy of input (SAFE-001/003)
GsvgErrorCode gsvg_process(
    const uint16_t* inputPixels,
    uint32_t width, uint32_t height,
    const GsvgConfig* config,
    uint16_t* outputPixels,     // Pre-allocated output buffer
    char* errorMsg,             // Error message buffer (256 chars)
    size_t errorMsgLen
);

const char* gsvg_version(void);
const char* gsvg_error_string(GsvgErrorCode code);

} // extern "C"
```

### 2.3 DWT Decomposer

```cpp
// gsvg/grid_suppression/DwtDecomposer.h
namespace gsvg {

struct DwtSubBands {
    ImageBuffer LL;  // Approximation
    ImageBuffer LH;  // Horizontal detail
    ImageBuffer HL;  // Vertical detail
    ImageBuffer HH;  // Diagonal detail
};

class DwtDecomposer {
public:
    enum class WaveletType { HAAR, DB4, DB6 };
    
    explicit DwtDecomposer(WaveletType type = WaveletType::DB4);
    
    // Forward 2D DWT — one level
    DwtSubBands decompose(const ImageBuffer& input) const;
    
    // Inverse 2D DWT — one level
    ImageBuffer reconstruct(const DwtSubBands& bands) const;
    
    // Multi-level decomposition with auto-stop
    // Stops when gridline energy exceeds threshold in any sub-band
    struct MultiLevelResult {
        std::vector<DwtSubBands> levels;
        int stopLevel;               // Level where grid detected (-1 if not found)
        std::vector<bool> gridDetected;  // Per-level detection flag
    };
    MultiLevelResult decomposeMultiLevel(
        const ImageBuffer& input, 
        int maxLevels,
        float energyThreshold = 3.0f   // σ multiplier
    ) const;

private:
    WaveletType type_;
    std::vector<float> lowPassFilter_;
    std::vector<float> highPassFilter_;
    
    void initFilters();
    std::vector<float> convolveAndDecimate(
        const std::vector<float>& signal,
        const std::vector<float>& filter
    ) const;
};

} // namespace gsvg
```

### 2.4 Laplacian Pyramid (Virtual Grid Core)

```cpp
// gsvg/virtual_grid/LaplacianPyramid.h
namespace gsvg {

struct LaplacianLevel {
    ImageBuffer gaussian;     // g_k (low-pass approximation)
    ImageBuffer laplacian;    // L_k (detail / differential)
};

class LaplacianPyramid {
public:
    struct Config {
        float gaussianSigma = 1.0f;
        int kernelSize = 5;
        int numLevels = 0;      // 0 = auto: log2(N) - 0.5
    };
    
    explicit LaplacianPyramid(const Config& config = {});
    
    // Decompose into Gaussian + Laplacian pyramid
    std::vector<LaplacianLevel> decompose(const ImageBuffer& input) const;
    
    // Apply scatter correction to low-freq and contrast/denoise to high-freq
    void processLevels(
        std::vector<LaplacianLevel>& levels,
        float sprCorrectionFactor,      // From SPR calculation
        float contrastGain,             // From grid ratio selection
        float noiseSigma                // Estimated noise level
    ) const;
    
    // Reconstruct from processed pyramid
    ImageBuffer reconstruct(const std::vector<LaplacianLevel>& levels) const;

private:
    Config config_;
    
    ImageBuffer gaussianBlur(const ImageBuffer& input) const;
    ImageBuffer downsample2x(const ImageBuffer& input) const;
    ImageBuffer upsample2x(const ImageBuffer& input) const;
};

} // namespace gsvg
```

### 2.5 Scatter Estimator

```cpp
// gsvg/virtual_grid/ScatterEstimator.h
namespace gsvg {

// 4-Gaussian scatter kernel model (per Bhatia 2017)
struct ScatterKernel {
    float a[4];    // Amplitudes
    float sigma[4]; // Widths (Gaussian σ)
    // S(r) = Σ a_i × exp(-r² / (2σ_i²))
};

class ScatterEstimator {
public:
    // Load pre-computed scatter kernel LUT from directory
    explicit ScatterEstimator(const std::string& lutDirectory);
    
    // Estimate scatter distribution for given conditions
    ImageBuffer estimateScatter(
        const ImageBuffer& inputImage,
        float thicknessCm,
        float kvp,
        float fieldSizeCm,
        float airGapCm
    ) const;
    
    // Subtract scatter from input (with clamping for SAFE-004)
    ImageBuffer subtractScatter(
        const ImageBuffer& totalImage,
        const ImageBuffer& scatterMap
    ) const;

private:
    // LUT indexed by [thickness][kvp][fieldSize][airGap]
    std::map<std::tuple<int,int,int,int>, ScatterKernel> kernelLut_;
    
    // Interpolate kernel for non-LUT-point conditions
    ScatterKernel interpolateKernel(
        float thickness, float kvp, float fieldSize, float airGap
    ) const;
    
    // Convolve primary estimate with scatter kernel
    ImageBuffer convolveWithKernel(
        const ImageBuffer& primary,
        const ScatterKernel& kernel
    ) const;
};

} // namespace gsvg
```

---

# GSVG-SVP-001: Software Verification Plan

## 1. Verification Strategy

```mermaid
graph BT
    UT[Unit Testing<br/>Google Test<br/>≥ 90% line coverage] --> IT[Integration Testing<br/>Module interaction<br/>Data flow validation]
    IT --> ST[System Testing<br/>End-to-end<br/>Clinical image sets]
    ST --> REG[Regression Testing<br/>Golden reference<br/>Automated CI]
    
    style UT fill:#9f9
    style IT fill:#9cf
    style ST fill:#fc9
    style REG fill:#f9f
```

## 2. Unit Test Plan

| Test ID | Module | Test Description | Pass Criteria |
|---------|--------|-----------------|---------------|
| UT-GS-001 | DwtDecomposer | Known synthetic signal → verify perfect reconstruction | PSNR > 100 dB |
| UT-GS-002 | DwtDecomposer | Sine wave at grid frequency → verify energy concentration in correct sub-band | Energy ratio > 10× |
| UT-GS-003 | GridlineDetector | Synthetic grid pattern → detect correct frequency ± 0.1 lp/mm | Frequency match |
| UT-GS-004 | BandStopFilter | Apply to known spectrum → verify target frequency suppressed > 40 dB | Attenuation check |
| UT-GS-005 | BandStopFilter | Apply to non-grid image → verify PSNR > 45 dB vs original | Minimal degradation |
| UT-VG-001 | ThicknessEstimator | Known phantom → verify thickness ± 1 cm accuracy | Within tolerance |
| UT-VG-002 | SprCalculator | Reference SPR data (Kyriakou 2007) → verify ± 10% | Match reference |
| UT-VG-003 | ScatterEstimator | Uniform field → verify scatter map symmetry and smoothness | Visual + SSIM |
| UT-VG-004 | LaplacianPyramid | Perfect reconstruction test (no processing) | PSNR > 100 dB |
| UT-VG-005 | LaplacianPyramid | Apply contrast gain → verify CNR improvement | CNR increase > 0 |
| UT-VG-006 | Denoiser | Known noise level → verify noise reduction > 50% | NPS measurement |
| UT-CM-001 | ImageBuffer | Boundary access → exception thrown | Exception caught |
| UT-CM-002 | ImageBuffer | Deep copy independence | Modify copy, check original unchanged |
| UT-CM-003 | Validator | Invalid DICOM → return error code | Error code match |
| UT-SF-001 | PipelineManager | Algorithm failure → original image returned | Pixel-exact match |
| UT-SF-002 | ScatterEstimator | SPR > physical max → clamping applied | Output within range |

## 3. Integration Test Plan

| Test ID | Modules | Test Description | Pass Criteria |
|---------|---------|-----------------|---------------|
| IT-001 | SI-001 + SI-002 | Full grid suppression pipeline with synthetic grid image | Grid artifact invisible, MTF < 5% loss |
| IT-002 | SI-001 + SI-003 | Full virtual grid pipeline with scatter-corrupted phantom | CNR within 90% of physical grid |
| IT-003 | SI-001 + SI-002 + SI-003 | Auto-detection: grid image → GS path, non-grid → VG path | Correct routing |
| IT-004 | SI-004 (all SOUP) | DICOM read → process → DICOM write round-trip | Metadata preserved |
| IT-005 | All | 100 consecutive frames, check memory stability | No leak (Valgrind) |

## 4. System Test Plan

| Test ID | Description | Test Data | Pass Criteria |
|---------|-------------|-----------|---------------|
| ST-001 | Grid suppression — 103 LP/inch grid, chest phantom | JPI grid + RANDO phantom | Radiologist VGA score ≥ 4/5 |
| ST-002 | Grid suppression — 150 LP/inch grid, extremity | Grid + hand phantom | No visible grid lines |
| ST-003 | Virtual grid — chest, 20cm equivalent | Non-grid chest DICOM | CNR ≥ 90% of 6:1 grid image |
| ST-004 | Virtual grid — pelvis, 25cm equivalent | Non-grid pelvis DICOM | CNR ≥ 85% of 8:1 grid image |
| ST-005 | Virtual grid — pediatric, 10cm equivalent | Non-grid pediatric DICOM | No overcorrection artifact |
| ST-006 | Performance — 3072×3072 | Clinical size image | Processing time ≤ 1.0s |
| ST-007 | Safety — corrupted input | Truncated DICOM file | Graceful error, no crash |
| ST-008 | Safety — extreme values | All-zero / all-max image | Valid output, no NaN/Inf |

## 5. Code Quality Metrics

| Metric | Target | Tool |
|--------|--------|------|
| Line coverage | ≥ 90% | gcov + lcov |
| Branch coverage | ≥ 80% | gcov |
| Static analysis warnings | 0 critical, 0 major | cppcheck, clang-tidy |
| Cyclomatic complexity | ≤ 15 per function | lizard |
| MISRA C++:2023 compliance | No mandatory rule violations | clang-tidy (MISRA checks) |

---

# GSVG-SOUP-001: SOUP Analysis

## 1. SOUP Component List

| ID | Component | Version | License | Function | Risk Mitigation |
|----|-----------|---------|---------|----------|-----------------|
| SOUP-001 | OpenCV | 4.9.0 | Apache 2.0 | Image I/O, resize, basic filtering | Published anomaly list reviewed; integration tests cover all used APIs |
| SOUP-002 | FFTW3 | 3.3.10 | GPL v2+ | Forward/inverse FFT | Well-validated (20+ years); unit tests verify known transforms |
| SOUP-003 | Eigen | 3.4.0 | MPL 2.0 | Matrix/vector operations | Widely used in production; unit tests verify matrix ops accuracy |
| SOUP-004 | DCMTK | 3.6.8 | BSD-like | DICOM read/write | FDA-recognized; integration tests verify DICOM conformance |
| SOUP-005 | Google Test | 1.14.0 | BSD-3 | Unit test framework | Test-only, not deployed in production |
| SOUP-006 | nlohmann/json | 3.11.3 | MIT | JSON config parsing | Input validation wraps all parsed values |

## 2. SOUP Risk Assessment

```mermaid
graph TD
    subgraph "SOUP Failure Impact Analysis"
        OCV_FAIL[OpenCV failure<br/>→ Image I/O error] -->|Mitigation| SAFE003[SAFE-003: Return original<br/>image + error code]
        FFTW_FAIL[FFTW failure<br/>→ Incorrect FFT result] -->|Mitigation| UT_FFT[Unit test: verify<br/>known FFT pairs]
        EIGEN_FAIL[Eigen failure<br/>→ Calculation error] -->|Mitigation| UT_MAT[Unit test: verify<br/>known matrix results]
        DCMTK_FAIL[DCMTK failure<br/>→ DICOM parse error] -->|Mitigation| VALID_IN[Input validator<br/>rejects malformed DICOM]
    end
```

## 3. SOUP Functional Requirements

| SOUP ID | Required Functionality | Performance Requirement |
|---------|----------------------|------------------------|
| SOUP-001 | `cv::imread`, `cv::imwrite`, `cv::resize`, `cv::GaussianBlur` | 3072×3072 ops < 100ms |
| SOUP-002 | `fftw_plan_dft_r2c_2d`, `fftw_plan_dft_c2r_2d`, `fftw_execute` | 3072×3072 FFT < 200ms |
| SOUP-003 | Matrix multiply, SVD, element-wise ops | 3072×3072 matrix ops < 50ms |
| SOUP-004 | Read/write DICOM Part 10 files, tag manipulation | Single file I/O < 200ms |

---

# GSVG-SHA-001: Software Hazard Analysis

## 1. Hazard Identification

| Hazard ID | Hazardous Situation | Cause | Severity | Probability | Risk Level |
|-----------|--------------------|----|----------|-------------|------------|
| HAZ-001 | 원본 영상 손실/훼손 | Algorithm crash, memory corruption | Medium | Low | Medium |
| HAZ-002 | 처리된 영상을 원본으로 오인 | Processing marking 누락 | Low | Medium | Low |
| HAZ-003 | Scatter 과보정으로 인체 구조물 소실 | SPR 과대추정, clamping 미적용 | Medium | Low | Medium |
| HAZ-004 | Pixel overflow/underflow로 영상 왜곡 | Arithmetic overflow in 16-bit | Medium | Low | Medium |
| HAZ-005 | Grid artifact 잔류로 병변 가려짐 | Grid frequency 오검출 | Low | Low | Low |
| HAZ-006 | 처리 지연으로 긴급 진단 지체 | Performance 미달 | Low | Low | Low |

## 2. Risk Control Measures

```mermaid
graph LR
    HAZ001[HAZ-001<br/>Image corruption] --> RC1[SAFE-001: Deep copy<br/>before processing]
    HAZ001 --> RC2[SAFE-003: Error →<br/>return original]
    
    HAZ002[HAZ-002<br/>Misidentification] --> RC3[SAFE-002: DICOM tag<br/>'Processed' marker]
    
    HAZ003[HAZ-003<br/>Overcorrection] --> RC4[SAFE-004: SPR<br/>physical max clamping]
    HAZ003 --> RC5[VG-FR-010: Artifact<br/>absence verification]
    
    HAZ004[HAZ-004<br/>Pixel overflow] --> RC6[SAFE-005: Output<br/>range clamping<br/>0 ~ 2¹⁶-1]
    
    HAZ005[HAZ-005<br/>Residual grid] --> RC7[GS-FR-005: Visual<br/>inspection test]
    HAZ005 --> RC8[GS-FR-006: MTF<br/>degradation < 5%]
    
    HAZ006[HAZ-006<br/>Latency] --> RC9[PERF-001: < 1.0s<br/>processing time]
```

## 3. Residual Risk Assessment

모든 hazard에 대해 risk control 적용 후:
- **HAZ-001~004**: Risk reduced to **Acceptable** (원본 보존 + clamping으로 worst case = 미처리 영상 반환)
- **HAZ-005~006**: Risk reduced to **Acceptable** (성능/정확도 요구사항으로 검증)

---

# GSVG-RTM-001: Requirements Traceability Matrix

## Grid Suppression Traceability

```mermaid
graph LR
    subgraph Requirements
        GS001[GS-FR-001<br/>Freq calculation]
        GS002[GS-FR-002<br/>2D DWT]
        GS003[GS-FR-003<br/>Energy detection]
        GS004[GS-FR-004<br/>Band-stop filter]
        GS005[GS-FR-005<br/>Artifact-free output]
        GS006[GS-FR-006<br/>MTF < 5% loss]
    end
    
    subgraph Architecture
        DWT_M[DwtDecomposer]
        GDET_M[GridlineDetector]
        BSF_M[BandStopFilter]
    end
    
    subgraph Tests
        UT002[UT-GS-002]
        UT003[UT-GS-003]
        UT004[UT-GS-004]
        UT005[UT-GS-005]
        IT001[IT-001]
        ST001[ST-001]
        ST002[ST-002]
    end
    
    GS001 --> GDET_M --> UT003
    GS002 --> DWT_M --> UT002
    GS003 --> GDET_M --> UT003
    GS004 --> BSF_M --> UT004
    GS005 --> IT001 --> ST001
    GS006 --> UT005 --> ST002
```

## Virtual Grid Traceability

| Requirement | Architecture Module | Unit Test | Integration Test | System Test |
|-------------|-------------------|-----------|-----------------|-------------|
| VG-FR-001 | ThicknessEstimator | UT-VG-001 | IT-002 | ST-003 |
| VG-FR-002 | SprCalculator | UT-VG-002 | IT-002 | ST-003 |
| VG-FR-003 | ScatterEstimator | UT-VG-003 | IT-002 | ST-003 |
| VG-FR-004 | ScatterEstimator | UT-VG-003 | IT-002 | ST-003 |
| VG-FR-005 | LaplacianPyramid | UT-VG-004 | IT-002 | ST-003 |
| VG-FR-006 | Denoiser | UT-VG-006 | IT-002 | ST-003 |
| VG-FR-007 | — (system level) | — | IT-002 | ST-003, ST-004 |
| VG-FR-008 | PipelineManager | — | IT-003 | ST-003 |
| VG-FR-009 | ThicknessEstimator | UT-VG-001 | IT-002 | ST-003~005 |
| VG-FR-010 | — (system level) | — | — | ST-003~005 |

## Safety Requirements Traceability

| Safety Req | Hazard | Architecture | Test | Risk Control |
|------------|--------|-------------|------|-------------|
| SAFE-001 | HAZ-001 | ImageBuffer::deepCopy() | UT-SF-001 | Deep copy before processing |
| SAFE-002 | HAZ-002 | DicomIO::markProcessed() | IT-004 | DICOM tag insertion |
| SAFE-003 | HAZ-001 | PipelineManager error path | UT-SF-001 | Original returned on error |
| SAFE-004 | HAZ-003 | ScatterEstimator::clamping | UT-SF-002 | SPR max physical limit |
| SAFE-005 | HAZ-004 | Output clamping in pipeline | UT-CM-001 | 0~65535 range enforcement |

---

# Appendix A: Algorithm Reference Summary

## A.1 Grid Suppression — 교차검증된 알고리즘 선택 근거

| Approach | Pros | Cons | Reference | 선택 |
|----------|------|------|-----------|------|
| 1D FFT + blur kernel | Simple | Image blur | Barski 1999 | ✗ |
| 1D notch filter | Fast | Ringing artifact | Belykh 2001 | ✗ |
| Gaussian band-stop (freq domain) | Effective | Grid freq 사전 지식 필요 | Lin 2006 | △ (sub-component) |
| Homomorphic filtering | Good for rotated grids | Complex, a-Se specific | Kim 2013 | ✗ |
| **2D DWT + Gaussian band-stop** | **Preserve info, auto-stop** | **Moderate complexity** | **Tang 2015** | **✓ Selected** |
| NSCT + band-pass | Good for Moiré | Higher complexity | Kim 2023 | ✗ (future option) |
| Mixed-norm regularization | Crisscross grid support | Iterative, slow | Jeon 2022 | ✗ (future option) |
| Deep learning hybrid | High accuracy | Training data needed | 2024 | ✗ (future option) |

**선택 근거**: 2D DWT 기반 방법은 (1) auto-stop condition으로 over-decomposition 방지, (2) sub-band 레벨에서 targeted filtering으로 원본 정보 보존 최대화, (3) Tang 2015에서 Medical Physics 게재로 peer-reviewed 검증 완료.

## A.2 Virtual Grid — 교차검증된 알고리즘 선택 근거

| Approach | Pros | Cons | Reference | 선택 |
|----------|------|------|-----------|------|
| **Laplacian Pyramid** | **특허 공개 구현, 검증됨** | **Scatter model 별도 필요** | **US8064676B2** | **✓ Core framework** |
| **Scatter Kernel LUT** | **MC 정확도, real-time** | **LUT 생성 필요** | **Philips SkyFlow Plus** | **✓ Scatter estimation** |
| Monte Carlo real-time | Most accurate | Too slow for clinical | Various | ✗ |
| Deep learning (U-Net) | Fast, adaptive | Training data, validation burden | Lee 2018 | ✗ (Phase 2 option) |
| GAN noise reduction | Noise handling | Additional complexity | Lim 2023 | ✗ (future option) |
| Beam stopper array | Direct measurement | Extra hardware needed | Nature 2023 | ✗ (hardware method) |

**선택 근거**: Laplacian Pyramid + Scatter Kernel LUT 조합은 (1) US8064676B2에 완전한 구현이 공개되어 구현 위험 최소, (2) Philips SkyFlow Plus가 동일 원리로 임상 검증 완료, (3) MC-based LUT로 물리적 정확도 확보, (4) Real-time 처리 가능.

---

# Appendix B: Key Equations Quick Reference

## B.1 Scatter-to-Primary Ratio

```
SPR(t, kVp, FOV) = a(kVp) × t^b(kVp) × FOV^c(kVp)

where:
  t = water-equivalent thickness [cm]
  FOV = field size [cm²]
  a, b, c = empirical coefficients from MC LUT fitting
  
Typical values (80 kVp, 35×43 cm):
  10 cm → SPR ≈ 0.4
  20 cm → SPR ≈ 1.0
  30 cm → SPR ≈ 2.0
```

## B.2 Grid Effect Emulation

```
I_grid_like = I_primary × (1 + α × GR / (GR + 1))

where:
  I_primary = I_total / (1 + SPR)    # Scatter subtracted image
  GR = selected grid ratio (6, 8, 10, 12)
  α = contrast enhancement coefficient (calibrated per anatomy)
```

## B.3 Gaussian Band-Stop Filter

```
H(u,v) = 1 - exp(-((u - u_g)² + (v - v_g)²) / (2σ²))

where:
  (u_g, v_g) = grid artifact frequency location [cycles/pixel]
  σ = filter bandwidth [cycles/pixel]
  Applied in frequency domain after DWT sub-band FFT
```

## B.4 Wiener Denoising

```
G(u,v) = H*(u,v) / (|H(u,v)|² + σ_n² / σ_s²)

where:
  H(u,v) = imaging system OTF (approximated)
  σ_n = noise standard deviation (estimated from scatter correction residual)
  σ_s = signal power spectral density
```

---

*Document End*  
*Next Action: Phase 1 implementation of Grid Suppression Engine starting with DwtDecomposer and unit tests.*
