# Software Architecture Document - Panel Defect Correction Module

**Document ID:** SAD-DEFECT-001 v1.0  
**IEC 62304 Clause:** 5.3 (Software Architectural Design)  
**Module:** `xpe_preprocess.dll`, Stage 3, Layer 1  
**Safety Classification:** Class B  
**Date:** 2026-04-14  
**Author:** XPE Architecture Team  
**Language:** Korean (descriptions), English (technical terms)  
**Approval:** __________________ Date: __________  

---

## 목차

1. [목적 및 범위](#목적-및-범위)
2. [시스템 컨텍스트](#시스템-컨텍스트)
3. [소프트웨어 단위 분해](#소프트웨어-단위-분해)
4. [데이터 흐름](#데이터-흐름)
5. [API 명세](#api-명세)
6. [메모리 레이아웃](#메모리-레이아웃)
7. [Anti-Spaghetti 규칙](#anti-spaghetti-규칙)
8. [성능 예산](#성능-예산)

---

## 목적 및 범위

### 1.1 목적

이 Software Architecture Document (SAD)는 Panel Defect Correction Module (`xpe_preprocess.dll`, Stage 3, Layer 1)의 구조적 설계를 정의합니다. SRS-DEFECT-001의 요구사항을 관리 가능한 소프트웨어 단위(SWU)로 분해하고, 책임, 인터페이스, 의존성, 데이터 흐름을 명시합니다.

### 1.2 범위

**포함**:
- Static BPM Generator (calibration-time)
- Dynamic Defect Detector (per-frame)
- Morphology Classifier
- Cluster Correctors (3×3, 5×5 ANN)
- Line Defect Corrector (Type 1, 3, 5)
- Grid/Moiré Detector & Suppressor
- FixPix Advanced Path (optional)
- Profile Manager

**제외**:
- Enhancement processing (`xpe_enhance_basic.dll`)
- Virtual grid correction (`gsvg.dll`)
- GUI implementation (`ImageProcTest.exe`)

---

## 시스템 컨텍스트

### 2.1 레이어 아키텍처

```
┌──────────────────────────────────────────────────────┐
│ Layer 2: ImageProcTest.exe (C# WPF GUI)              │
│         ↓ P/Invoke (C ABI)                           │
├──────────────────────────────────────────────────────┤
│ Layer 1: xpe_preprocess.dll ← THIS MODULE            │
│         ├─ Calibration Subsystem (Phase 1a)         │
│         └─ Panel Defect Subsystem (Phase 3)         │ ← Panel Defect
│         ↓ Link dependency                            │
├──────────────────────────────────────────────────────┤
│ Layer 0: xpe_common.dll (types, memory, error codes) │
│         ↓ Link dependency                            │
├──────────────────────────────────────────────────────┤
│ OS APIs: Win32, Memory Management, I/O              │
└──────────────────────────────────────────────────────┘
```

### 2.2 외부 의존성

| 시스템 | 프로토콜 | 방향 | 목적 |
|--------|--------|------|------|
| **xpe_common.dll** | C ABI (link-time) | Import | Types (`XpeImageBuffer`, `XpeErrorCode`), memory utils, alerts |
| **Disk I/O** | Win32 File API | Input | Static BPM, ANN weights, DWT/DCT filter coefficients 로드 |
| **Detector Driver** | Metadata via API | Input | 온도, kVp, defect 통계 등 메타데이터 |
| **ImageProcTest.exe** | P/Invoke / C ABI | Bidirectional | 설정, 프레임 처리 요청, 통계 조회 |

---

## 소프트웨어 단위 분해

### 3.1 Software Item 1: DefectManager (중앙 오케스트레이터)

**책임**: 전체 결함 검출/보정 파이프라인 조율

#### 3.1.1 Software Units (SWU)

| SWU ID | 이름 | 책임 | 의존성 | 스레드 |
|--------|------|------|--------|--------|
| **SWU-3.1** | StaticBPMGenerator | Dark/Flat frame 분석, HotPixelMask, ColdPixelMask, FlickeringPixelMask 생성 | CalibFileIO | Main (init-time) |
| **SWU-3.2** | DynamicDefectDetector | Residual map 계산, k·σ 임계값 적용, 동적 결함 후보 검출 | ProfileManager | Main |
| **SWU-3.3** | MorphologyClassifier | Connected component labeling, 결함 타입 분류 (isolated, 3×3, 5×5, line) | -- | Main |
| **SWU-3.4** | ClusterCorrector_3x3 | 3×3 클러스터 ANN (40→9) 추론 및 픽셀 교체 | CalibFileIO (ANN weights) | Main |
| **SWU-3.5** | ClusterCorrector_5x5 | 5×5 클러스터 ANN (56→25) + 선택 TMC 정제 | CalibFileIO (ANN weights) | Main |
| **SWU-3.6** | LineDefectCorrector | Type 1/3/5 라인 보정, diffVal 임계값 분류, 보간/곡선 피팅 | ProfileManager | Main (multi-threaded) |
| **SWU-3.7** | GridMoireDetector | DWT 3-level 분해, MSI 계산, 심각도 분류 | -- | Main |
| **SWU-3.8** | GridMoireSuppressor | Gaussian bandstop filter 설계/적용, 역 DWT 재구성 | GridMoireDetector | Main |
| **SWU-3.9** | FixPixMLP | 선택: 1425-param MLP 기반 bad pixel 검출/보정 | CalibFileIO (MLP weights) | Main |
| **SWU-3.10** | ProfileManager | Min/Normal/Max 프로필 선택, 임계값 관리 | -- | Main |
| **SWU-3.11** | CalibFileIO | BPM, ANN weights, DWT/DCT filter coeff 로드/검증 (무결성 확인) | xpe_common | Main (init-time) |

### 3.2 SWU 책임 상세 정의

#### SWU-3.1: StaticBPMGenerator

```
Input:
  - N=200 dark frames (3 temperature levels)
  - N=200 flat-field frames (3 temperature levels)
  - N=200 flickering detection frames
Output:
  - HotPixelMask (binary, 1 byte/pixel)
  - ColdPixelMask (binary)
  - FlickeringPixelMask (binary)
  - LineDefectMask (binary)
  - BPM (uint8, 0=good, 1-255=defect type)

Algorithm:
  1. Dark frame: SNR analysis, λ=8.0 threshold
  2. Flat-field: G normalization, cold pixel detection
  3. Flickering: CV > 5% detection
  4. Union: BPM = HotPixel ∪ ColdPixel ∪ Flickering ∪ LineDefect
  5. Quality check: <0.1% hot, <0.1% cold, <5 lines
  6. Storage: RLE compression (optional)

Error Handling:
  - Input validation: frame dimensions, data range
  - Quality failure: return XPE_ERR_BPM_QUALITY_FAILED
  - File I/O: delegate to CalibFileIO (CRC check)
```

#### SWU-3.2: DynamicDefectDetector

```
Input:
  - float32 image (3072×3072)
  - Static BPM (optional, from SWU-3.1)
  - Profile (from SWU-3.10)
Output:
  - DynamicDefectMap (binary)
  - Merged DefectMap (Static ∪ Dynamic)

Algorithm:
  1. Residual map: I_res = I - Median_5x5(I)
  2. Local σ per 5×5 window
  3. k·σ threshold: defect if |I_res| > k·σ_local
  4. k = 3.0 (Min), 4.0 (Normal), 5.0 (Max)
  5. Union with static BPM
  6. Output: final defect map

Timing: < 20 ms/frame
Performance: Single-threaded SIMD optimization (AVX)
```

#### SWU-3.3: MorphologyClassifier

```
Input:
  - DefectMap (binary, merged from SWU-3.2)
Output:
  - Labeled components (uint32, unique ID per component)
  - Classification: isolated, 3×3, 5×5, line
  - Metadata: bounding box, centroid, pixel count

Algorithm:
  1. 8-connectivity connected component labeling (flood fill)
  2. Per component: compute bounding box (x_min, x_max, y_min, y_max)
  3. Classify:
     - Single pixel (1×1) → Isolated
     - Fits in 3×3 (width ≤ 3, height ≤ 3) → 3×3 Cluster
     - Fits in 5×5 (not in 3×3) → 5×5 Cluster
     - Elongated (one dimension >> other, width 1-5) → Line Defect
  4. Store: component_id, type, bbox, pixels

Data Structure:
  struct DefectComponent {
    uint32_t component_id;
    DefectType type;  // ISOLATED, CLUSTER_3x3, CLUSTER_5x5, LINE
    int16_t x_min, x_max, y_min, y_max;  // bounding box
    uint16_t pixel_count;
    int16_t centroid_x, centroid_y;
  };
```

#### SWU-3.4 & SWU-3.5: Cluster Correctors

```
SWU-3.4 (3×3):
Input:
  - Defect component (identified as 3×3 cluster)
  - 7×7 neighborhood
  - ANN weights (40→9, single layer)
Output:
  - Corrected 3×3 block (9 float32 values)

Algorithm:
  1. Extract 7×7 neighborhood: 49 pixels
  2. Exclude center 3×3: 40-dim input vector
  3. ANN forward: y = W·x + b (no hidden layer)
  4. Clip: y_i ∈ [0, 2^14]
  5. Replace 3×3 block with clipped y

SWU-3.5 (5×5):
Input:
  - Defect component (5×5 cluster)
  - 9×9 neighborhood
  - ANN weights (56→25, hidden 64 ReLU)
  - Optional: Calibration profile (TMC enable flag)
Output:
  - Corrected 5×5 block (25 float32 values)

Algorithm (basic):
  1. Extract 9×9 neighborhood: 81 pixels
  2. Exclude center 5×5: 56-dim input
  3. ANN forward: hidden = ReLU(W_h·x + b_h), output = W_o·hidden + b_o
  4. Clip output
  5. Replace 5×5 block

Algorithm (with TMC refinement):
  6. Search 27×27 neighborhood for best-matching intact 5×5 patch
  7. Blend: final = 0.7·ANN_output + 0.3·matched_patch
  8. Return final 5×5

Weights Storage:
  - Loaded from calibration profile at init-time
  - MD5/SHA-256 hash verification
  - Size: 3×3 ANN ~0.2 KB, 5×5 ANN ~10 KB (compressed)

Fallback:
  - If ANN weights corrupted: neighbor averaging (8-neighbor or bilinear)
```

#### SWU-3.6: LineDefectCorrector

```
Input:
  - Line defect component (identified from morphology)
  - Profile (Min/Normal/Max)
  - Adjacent intact lines
Output:
  - Corrected line pixels (float32)

Algorithm:

Type 1 (diffVal > T2):
  1. diffVal = normalized intensity deviation
  2. If diffVal > T2: treat as severe damage
  3. Interpolate from two adjacent intact lines: p_avg = (p_above + p_below) / 2
  4. Gaussian smooth (σ=1.5): p_smooth = Gaussian_filter(p_avg, σ=1.5)
  5. Replace all line pixels

Type 3 (T1 < diffVal ≤ T2):
  1. Edge detection: E(i) = |∂I/∂direction|
  2. Interpolate from adjacent lines: p_interp
  3. Quadratic curve fit through adjacent lines: p_fit = a·i² + b·i + c
  4. Blend: p_final = 0.6·p_interp + 0.4·p_fit
  5. Preserve edge gradients in high-E regions

Type 5 (diffVal ≤ T1):
  1. Mode-dependent:
     - Min: light Gaussian smooth (σ=0.8)
     - Normal: minimal/no change
     - Max: no change

Line Width Handling (1-5 pixels):
  - Width=1: single interpolation
  - Width>1: process each column/row of defect band with edge consistency

Timing: < 30 ms/frame (multi-threaded over 4 cores)
```

#### SWU-3.7 & SWU-3.8: Grid/Moiré Detection & Suppression

```
SWU-3.7 (GridMoireDetector):
Input:
  - Defect-corrected float32 image
Output:
  - MSI (float32, percentage)
  - Severity class (Low/Medium/High/Critical)
  - Frequency analysis (DWT subbands energy)

Algorithm (DWT-based, baseline):
  1. 3-level 2D DWT decomposition:
     I[1] = {LL[1], LH[1], HL[1], HH[1]}
     I[2] = {LL[2], ...}
     I[3] = {LL[3], ...}
  2. Compute energy per subband: E[l, band] = Σ(coeff²)
  3. Identify grid bands: grid energy = max of (LH, HL, HH) across levels
  4. MSI = E_grid / E_total × 100%
  5. Classify:
     - MSI < 0.1: Low
     - 0.1 ≤ MSI < 0.3: Medium
     - 0.3 ≤ MSI < 0.7: High
     - MSI ≥ 0.7: Critical

SWU-3.8 (GridMoireSuppressor):
Input:
  - Defect-corrected image
  - MSI and severity class (from SWU-3.7)
  - Profile (filter strength)
Output:
  - Grid-suppressed image (float32)

Algorithm (DWT-based, baseline):
  1. If MSI < 0.1: skip suppression, return original
  2. If 0.1 ≤ MSI < 0.7:
     a. Apply adaptive Gaussian bandstop filter to grid-dominated subbands
     b. Filter: H_stop(u,v) = 1 - exp(-(u-u₀)²+(v-v₀)² / (2σ²))
     c. σ and center (u₀,v₀) depend on identified grid frequency
     d. Inverse DWT, reconstruct
  3. If MSI ≥ 0.7: escalate to DCT-based or GRD method (advanced)
  
Strength by profile:
  - Min: 2× filter strength (aggressive)
  - Normal: 1× filter strength (standard)
  - Max: 0.5× filter strength (conservative)

Output check: final MSI < 0.1 (Low target)
```

#### SWU-3.10: ProfileManager

```
Input:
  - Profile ID (MIN, NORMAL, MAX)
  - Custom parameter overrides (optional)
Output:
  - Active parameter set (thresholds, filter strengths, flags)

Parameter Sets:

Min Profile:
  k_threshold = 3.0
  T1 = 0.15, T2 = 0.50
  MSI_threshold = 0.10
  DWT_filter_strength = 2.0
  ANN_3x3_enabled = true
  ANN_5x5_enabled = true
  TMC_enabled = true
  Type1_correction = always
  Type3_correction = always
  Type5_correction = always
  
Normal Profile:
  k_threshold = 4.0
  T1 = 0.25, T2 = 0.75
  MSI_threshold = 0.20
  DWT_filter_strength = 1.0
  ANN_3x3_enabled = true
  ANN_5x5_enabled = true
  TMC_enabled = optional
  Type1_correction = always
  Type3_correction = always
  Type5_correction = minimal
  
Max Profile:
  k_threshold = 5.0
  T1 = 0.40, T2 = 1.00
  MSI_threshold = 0.40
  DWT_filter_strength = 0.5
  ANN_3x3_enabled = true
  ANN_5x5_enabled = true
  TMC_enabled = false
  Type1_correction = threshold-based
  Type3_correction = selective
  Type5_correction = false

Thread Safety: Mutex-protected parameter updates
```

#### SWU-3.11: CalibFileIO

```
Input:
  - File paths (BPM, ANN weights, DWT coeff, etc.)
Output:
  - Loaded data structures (maps, arrays)
  - Validation status

Responsibilities:
  1. File loading: open, read, verify file format
  2. Integrity check: MD5/SHA-256 hash, CRC-32 (if applicable)
  3. Data parsing: binary format interpretation
  4. Error handling: corrupted file detection, report errors
  5. Caching: in-memory caching for repeated access

File Formats:
  - BPM: uint8, 3072×3072 (or RLE compressed)
  - ANN weights: float32, variable size (0.2-10 KB compressed)
  - DWT/DCT filter coeff: float32, frequency-dependent

Error Codes:
  - XPE_ERR_FILE_NOT_FOUND
  - XPE_ERR_FILE_CORRUPTED
  - XPE_ERR_INTEGRITY_FAILED
  - XPE_ERR_CALIBRATION_EXPIRED (if timestamp check applicable)
```

---

## 데이터 흐름

### 4.1 Single-Frame Processing Pipeline

```
┌─────────────────────────────────────────────────────────┐
│ Input: float32 gain-corrected image (3072×3072)         │
│        + metadata (temperature, kVp, SID, profile)      │
└──────────────┬──────────────────────────────────────────┘
               │
               ▼
        ┌──────────────────┐
        │ SWU-3.2:         │
        │ Dynamic Defect   │
        │ Detection        │
        │ (< 20 ms)        │
        └────────┬─────────┘
                 │ output: dynamic defect map
                 │ + static BPM union
                 ▼
        ┌──────────────────┐
        │ SWU-3.3:         │
        │ Morphology       │
        │ Classification   │
        │ (< 5 ms)         │
        └────────┬─────────┘
                 │ output: isolated, 3×3, 5×5, line components
                 │
         ┌───────┴──────────┬──────────────┬──────────────┐
         │                  │              │              │
         ▼                  ▼              ▼              ▼
    [Isolated]     [3×3 Cluster]   [5×5 Cluster]   [Line Defect]
    (skip or         │               │               │
     neighbor avg)   ▼               ▼               ▼
                 SWU-3.4:        SWU-3.5:        SWU-3.6:
                 ANN 40→9        ANN 56→25       Edge-aware
                 (< 10 ms)       +TMC            Interp
                                 (< 15 ms)       (< 30 ms)
                     │               │               │
                     └───────────────┼───────────────┘
                                     │
                                     ▼
                          ┌────────────────────┐
                          │ Merged Output:     │
                          │ All pixels corrected│
                          │ (< 70 ms total)    │
                          └─────────┬──────────┘
                                    │
                                    ▼
                          ┌────────────────────┐
                          │ SWU-3.7:           │
                          │ Grid/Moiré Detection│
                          │ (MSI computation)  │
                          │ (< 10 ms)          │
                          └─────────┬──────────┘
                                    │
                              (if MSI > threshold)
                                    │
                                    ▼
                          ┌────────────────────┐
                          │ SWU-3.8:           │
                          │ Grid/Moiré         │
                          │ Suppression        │
                          │ (DWT/DCT filter)   │
                          │ (< 15 ms)          │
                          └─────────┬──────────┘
                                    │
                                    ▼
        ┌──────────────────────────────────────┐
        │ Output: float32 defect-corrected     │
        │         image (3072×3072)            │
        │         + metadata (defect stats,    │
        │           processing time, profile)  │
        └──────────────────────────────────────┘
```

### 4.2 Memory Flow Diagram

```
┌─────────────────────────────────────────────────┐
│ Input Buffer (37.7 MB)                          │
│   ├─ Raw frame (3072×3072, float32)            │
│   └─ Metadata structure                         │
└────────┬────────────────────────────────────────┘
         │
         ▼
  ┌─────────────────┐
  │ Static Data     │ (loaded once at init)
  │ ├─ BPM (9.4 MB) │
  │ ├─ ANN weights  │
  │ └─ Filters      │
  └────────┬────────┘
           │
           ▼
┌──────────────────────────────────────┐
│ Working Buffers (per-frame)          │
│ ├─ Residual map (37.7 MB)           │
│ ├─ Defect map (9.4 MB, binary)      │
│ ├─ Labeled components (18.9 MB)     │
│ ├─ Cluster temp buffers (5 MB each) │
│ └─ DWT/DCT coefficients (37.7 MB)   │
│ Total: ~150 MB (peak)                │
└──────────┬───────────────────────────┘
           │
           ▼
┌──────────────────────────────────────┐
│ Output Buffer (37.7 MB)              │
│   └─ Corrected frame (3072×3072)    │
└──────────────────────────────────────┘

Free working buffers after frame processing.
```

---

## API 명세

### 5.1 Initialization & Setup

```c
// Initialize module, load Static BPM
XPE_Error xpe_defect_init(const XPE_Config* cfg);

// Load calibration profile (BPM, ANN weights, filter coefficients)
XPE_error xpe_defect_load_calibration(const char* calibration_file_path);

// Set detection/correction profile (Min/Normal/Max)
void xpe_defect_set_profile(XPE_DefectProfile profile);

// Get current profile
XPE_DefectProfile xpe_defect_get_profile(void);
```

### 5.2 Detection

```c
// Dynamic defect detection (Step 1-2: residual + morphology)
XPE_error xpe_defect_detect_runtime(
    const XPE_ImageBuffer* img,
    XPE_DefectMask* out_mask
);

// Query defect statistics from last frame
void xpe_defect_get_detection_stats(
    XPE_DefectStats* out_stats  // isolated_count, cluster3x3_count, cluster5x5_count, line_count, etc.
);
```

### 5.3 Correction

```c
// Cluster correction (3×3, 5×5 ANN)
XPE_error xpe_defect_correct_clusters(
    const XPE_ImageBuffer* in,
    const XPE_DefectMask* mask,
    XPE_ImageBuffer* out
);

// Line defect correction (Type 1, 3, 5)
XPE_error xpe_defect_correct_lines(
    const XPE_ImageBuffer* in,
    const XPE_DefectMask* mask,
    XPE_ImageBuffer* out
);

// Grid/Moiré detection & suppression
XPE_error xpe_defect_suppress_grid(
    const XPE_ImageBuffer* in,
    float* out_msi,  // Moiré Severity Index
    XPE_ImageBuffer* out
);

// All-in-one: detect + correct (clusters + lines + grid)
XPE_error xpe_defect_process(
    const XPE_ImageBuffer* in,
    XPE_ImageBuffer* out
);
```

### 5.4 Diagnostic & Control

```c
// Set diagnostic bypass (Service Engineer only)
void xpe_defect_set_bypass(bool enabled);

// Audit log: write event to logger
void xpe_defect_log_event(
    XPE_EventType event_type,  // DETECT, CORRECT, SUPPRESS, ERROR, etc.
    const char* message,
    uint32_t num_defects,
    float msi_value
);

// Cleanup: free all allocated memory
void xpe_defect_cleanup(void);
```

---

## 메모리 레이아웃

### 6.1 Static Allocations (Init-time)

| Structure | Size | Lifetime | Allocation |
|-----------|------|----------|-----------|
| Static BPM (uint8) | 9.4 MB | Program | Init → Cleanup |
| ANN weights (3×3) | 0.2 KB | Program | Init → Cleanup |
| ANN weights (5×5) | 10 KB | Program | Init → Cleanup |
| DWT/DCT filter coeff | ~10 KB | Program | Init → Cleanup |
| **Total static** | **< 10 MB** | Program | Once |

### 6.2 Dynamic Per-Frame Allocations

| Buffer | Size | Lifetime | Purpose |
|--------|------|----------|---------|
| Residual map | 37.7 MB | Per-frame | Local median subtraction |
| Defect map (binary) | 9.4 MB | Per-frame | Marked defect pixels |
| Labeled components | 18.9 MB | Per-frame | Component IDs (uint32) |
| DWT coefficients | 37.7 MB | Per-frame | 3-level decomposition |
| Cluster temp buffer | 5 MB | Per-cluster | Neighborhood extraction |
| **Total per-frame** | **< 120 MB** | Per-frame | Freed after processing |

### 6.3 Ring Buffers (Optional)

For frame history (future lag correction):
- 8 frames × 3072×3072 × 4 bytes = ~150 MB
- Optional, not required for defect correction alone

---

## Anti-Spaghetti 규칙

### 7.1 Module Boundary Rules

**HARD Rules**:

1. **No lateral DLL dependencies**: Panel Defect (xpe_preprocess.dll) does NOT depend on or call `xpe_enhance_basic.dll` or `gsvg.dll`.
   - **Reason**: Prevents circular dependencies and enables independent unit testing.
   - **Verification**: Linker check (no cross-DLL symbol references).

2. **No global state across SWUs**: All state is passed via function parameters or encapsulated in SWU-local structures.
   - **Reason**: Enables multi-threaded processing and prevents race conditions.
   - **Verification**: Code review, thread-safety analysis.

3. **Single responsibility per SWU**: Each SWU handles one task (detection XOR correction XOR classification).
   - **Reason**: Simplifies testing and change management.
   - **Verification**: Design review, test isolation.

4. **Input validation at module boundary**: All external inputs (from ImageProcTest.exe via P/Invoke) are validated at the API entry point.
   - **Reason**: Prevents invalid data from propagating through SWUs.
   - **Verification**: Boundary test cases.

### 7.2 Data Flow Rules

1. **Unidirectional data flow**: Each stage outputs to the next stage; no backward dependencies.
   - Detection → Morphology → Correction → Grid Suppression
   - **Reason**: Simplifies debugging and enables pipelined processing.

2. **No shared mutable buffers between SWUs**: Each SWU allocates its own working buffers.
   - **Exception**: DefectMap is read-only shared (passed as const pointer).
   - **Reason**: Prevents unexpected side effects.

3. **Error propagation via return codes**: All errors bubble up to caller; no silent fallbacks within SWUs.
   - **Exception**: Fallback correctors (neighbor averaging) are logged.
   - **Reason**: Maintains audit trail and diagnostic capability.

### 7.3 Thread Safety

1. **Main thread primary**: Most processing runs on main thread. Multi-threading only for long-running tasks (line correction over multiple cores).
   - **Reason**: Simplifies synchronization, C ABI compatibility with GUI.

2. **Mutex protection for ProfileManager**: Profile changes must be atomic.
   - **Reason**: Prevents mid-frame profile switches.

3. **No lock-free primitives**: Use standard OS mutexes/critical sections.
   - **Reason**: Simplicity, standard library support.

---

## 성능 예산

### 8.1 Per-Frame Processing Timeline

| Stage | SWU | Target | Notes |
|-------|-----|--------|-------|
| **Detection** | SWU-3.2 | 20 ms | Residual + k·σ thresholding |
| **Classification** | SWU-3.3 | 5 ms | Connected component labeling |
| **3×3 Clusters** | SWU-3.4 | 10 ms | ~100 clusters avg, ANN single-layer |
| **5×5 Clusters** | SWU-3.5 | 15 ms | ~50 clusters avg, ANN + hidden layer |
| **Line Defects** | SWU-3.6 | 30 ms | ~10 lines avg, multi-threaded (4 cores) |
| **Grid Detection** | SWU-3.7 | 10 ms | 3-level DWT, energy computation |
| **Grid Suppression** | SWU-3.8 | 15 ms | Only if MSI > threshold (DWT filter) |
| **Overhead** | Management | 5 ms | Logging, metadata, synchronization |
| **TOTAL** | All stages | **< 95 ms** | 3072×3072 frame |

### 8.2 Memory Budget

| Category | Target | Notes |
|----------|--------|-------|
| **Static data (BPM, weights)** | < 10 MB | Loaded once at init |
| **Per-frame working** | < 120 MB | Allocated/freed per frame |
| **Optional frame history** | ~150 MB | For future lag correction |
| **Total system memory** | < 200 MB | On Intel i7, shared with GUI |

### 8.3 Scalability Notes

- Larger detectors (4096×4096): Budget scales with pixel count ~×1.78
- More defects (>500): Morphology labeling scales O(n log n)
- Deeper DWT: 4-level instead of 3-level adds ~5 ms

---

**Document Version**: 1.0  
**Total SWUs**: 11 software units  
**Last Updated**: 2026-04-14  
**Next**: SHA-DEFECT-001 (Software Hazard Analysis)
