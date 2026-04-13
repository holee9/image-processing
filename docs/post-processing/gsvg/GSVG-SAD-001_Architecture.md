# GSVG-SAD-001: Software Architecture Design

**Document ID:** GSVG-SAD-001  
**Version:** 1.0 | **Date:** 2026-04-03  
**IEC 62304 Clause:** 5.3  
**Safety Classification:** Class B

---

## 1. System Context

```mermaid
graph TB
    subgraph "X-ray FPD System"
        ACQ[Image Acquisition<br/>FPGA/Firmware] --> RAW[Raw Image<br/>16-bit DICOM]
        RAW --> GSVG[GSVG Module]
        GSVG --> PROC[Processed Image<br/>16-bit DICOM]
        PROC --> CONSOLE[Diagnostic Console<br/>RadiConsole™]
    end
    
    subgraph "External Inputs"
        CONFIG[Configuration<br/>JSON] --> GSVG
        LUT[Scatter Kernel<br/>LUT Files] --> GSVG
    end
    
    subgraph "External Outputs"
        GSVG --> LOG[Processing Log<br/>JSON]
    end
```

---

## 2. Top-Level Architecture

```mermaid
graph TD
    API[API Layer<br/>gsvg_api.h / gsvg_types.h]
    
    subgraph "SI-001: Image Pipeline Manager"
        PM[PipelineManager]
        DETECT[GridDetector]
        CFG[ProcessingConfig]
    end
    
    subgraph "SI-002: Grid Suppression Engine"
        DWT[DwtDecomposer]
        GDET[GridlineDetector]
        BSF[BandStopFilter]
    end
    
    subgraph "SI-003: Virtual Grid Engine"
        THICK[ThicknessEstimator]
        SPR[SprCalculator]
        SCAT[ScatterEstimator]
        LAP[LaplacianPyramid]
        DENOISE[Denoiser]
    end
    
    subgraph "SI-004: Common Utilities"
        DICOM_IO[DicomIO]
        FFT_UTIL[FftUtils]
        IMG_BUF[ImageBuffer]
        VALID[Validator]
        ERR[ErrorHandler]
    end
    
    API --> PM
    PM --> DETECT
    DETECT -->|grid detected| DWT
    DETECT -->|no grid| THICK
    
    DWT --> GDET --> BSF
    THICK --> SPR --> SCAT
    SCAT --> LAP --> DENOISE
    
    DWT -.-> FFT_UTIL
    BSF -.-> FFT_UTIL
    LAP -.-> IMG_BUF
    SCAT -.-> FFT_UTIL
    PM -.-> DICOM_IO
    PM -.-> VALID
    PM -.-> ERR
```

---

## 3. Software Items

| Item ID | Name | Description | Safety Class | SOUP Dependencies |
|---------|------|-------------|--------------|-------------------|
| SI-001 | Image Pipeline Manager | 영상 입출력 라우팅, grid 유무 판정, 에러 처리, config 관리 | B | DCMTK, nlohmann/json |
| SI-002 | Grid Suppression Engine | DWT 기반 grid artifact 검출 및 Gaussian band-stop filter 제거 | B | FFTW3, OpenCV |
| SI-003 | Virtual Grid Engine | Scatter estimation(kernel LUT) + Laplacian Pyramid contrast enhancement + denoising | B | FFTW3, OpenCV, Eigen |
| SI-004 | Common Utilities | DICOM I/O, FFT wrapper, ImageBuffer(RAII), validation, error handling | B | DCMTK, OpenCV, FFTW3 |

---

## 4. SI-002: Grid Suppression Engine — Data Flow

```mermaid
flowchart LR
    IN[Input Image<br/>M×N 16-bit] --> DWT_PROC[2D DWT<br/>Db4 wavelet]
    
    DWT_PROC --> LL[LL<br/>Approximation]
    DWT_PROC --> LH[LH<br/>Horiz detail]
    DWT_PROC --> HL[HL<br/>Vert detail]
    DWT_PROC --> HH[HH<br/>Diag detail]
    
    LL -->|recursive decomposition<br/>until grid detected| DWT_PROC
    
    LH --> ENERGY[Sub-band Energy<br/>Analysis]
    HL --> ENERGY
    HH --> ENERGY
    
    ENERGY -->|energy > 3σ threshold| BSF_APPLY[Gaussian<br/>Band-Stop Filter]
    ENERGY -->|below threshold| PASS[Pass-through]
    
    BSF_APPLY --> RECON[Inverse DWT<br/>Reconstruction]
    PASS --> RECON
    
    RECON --> OUT[Grid-free Image]
```

**Algorithm Parameters (교차검증 근거):**

| Parameter | Value | Source |
|-----------|-------|--------|
| Wavelet basis | Daubechies-4 (db4) | Tang 2015, Medical Physics |
| Max decomposition levels | `log₂(min(M,N)) - 4` | Adaptive to image size |
| Gridline energy threshold | 3σ above mean sub-band energy | Tang 2015 |
| Band-stop filter bandwidth | ±2 pixels in frequency domain | Lin 2006, J Digital Imaging |
| Band-stop Gaussian σ | 1.5 pixels | Empirical calibration |

---

## 5. SI-003: Virtual Grid Engine — Data Flow

```mermaid
flowchart TD
    IN[Input Image + DICOM metadata<br/>kVp, mAs, SID, field size] --> THICK_EST[Thickness Estimation<br/>exposure parameters → t_eq cm]
    
    subgraph "Scatter Estimation Pipeline"
        THICK_EST --> SPR_CALC[SPR Calculation<br/>SPR = f&lpar;t, kVp, FOV&rpar;]
        SPR_CALC --> KERNEL[Kernel Selection<br/>from MC-based LUT]
        KERNEL --> SMAP[Scatter Map<br/>S = K ⊗ I_primary_est]
    end
    
    IN --> SUBTRACT[Scatter Subtraction<br/>I_p = I_total - S]
    SMAP --> SUBTRACT
    
    subgraph "Laplacian Pyramid Processing"
        SUBTRACT --> LP_DEC[LP Decomposition<br/>n levels]
        LP_DEC --> LOW[Low-freq bands<br/>De-scatter residual]
        LP_DEC --> HIGH[High-freq bands<br/>Contrast enhance + Denoise]
        LOW --> LP_REC[LP Reconstruction]
        HIGH --> LP_REC
    end
    
    LP_REC --> CLAMP[Output Clamping<br/>0 ~ 65535]
    CLAMP --> OUT[Virtual Grid Image]
```

**Laplacian Pyramid Processing (US8064676B2):**

```
Decomposition:
  g_{k+1} = [g_k * G_σ]↓2          (Gaussian convolution + 2x downsample)
  L_k     = g_k - [g_{k+1}]↑2 * G_σ  (Detail = original - upsampled approx)
  σ = 1.0, kernel = 5×5, n = log(N)/log(2) - 0.5

Per-band Processing:
  Low-freq:  g'_n = g_n × (1 + α × SPR_correction)
  High-freq: L'_k = β_k × L_k - WienerFilter(noise_k)
             β_k = contrast_gain_table[grid_ratio][level_k]

Reconstruction:
  g'_k = [g'_{k+1}]↑2 * G_σ + L'_k
```

**Scatter Kernel LUT Structure:**

| Axis | Range | Step | Unit |
|------|-------|------|------|
| Thickness | 5–35 | 1 | cm (water-equivalent) |
| kVp | 40–150 | 10 | kV |
| Field size | 10×10 – 43×43 | 5 | cm |
| Air gap | 0–20 | 5 | cm |

LUT 생성: GATE (Geant4) MC simulation → 4-Gaussian kernel model fitting per condition.

---

## 6. SOUP Interfaces

```mermaid
graph LR
    subgraph "GSVG Software Items"
        SI002[SI-002 Grid Suppression]
        SI003[SI-003 Virtual Grid]
        SI004[SI-004 Utilities]
    end
    
    subgraph "SOUP Components"
        OCV[OpenCV 4.9<br/>Apache 2.0]
        FFTW[FFTW3 3.3.10<br/>GPL v2+]
        EIGEN[Eigen 3.4<br/>MPL 2.0]
        DCMTK[DCMTK 3.6.8<br/>BSD-like]
        JSON[nlohmann/json 3.11<br/>MIT]
    end
    
    SI002 --> FFTW
    SI002 --> OCV
    SI003 --> OCV
    SI003 --> EIGEN
    SI003 --> FFTW
    SI004 --> DCMTK
    SI004 --> OCV
    SI004 --> FFTW
    SI004 --> JSON
```

상세 SOUP 분석은 GSVG-SOUP-001 참조.

---

## 7. Error Handling Architecture

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Processing: gsvg_process() called
    
    Processing --> InputValidation
    InputValidation --> GridDetection: valid
    InputValidation --> ErrorReturn: invalid input
    
    GridDetection --> GridSuppression: grid found
    GridDetection --> VirtualGrid: no grid
    GridDetection --> ErrorReturn: detection exception
    
    GridSuppression --> OutputValidation: success
    GridSuppression --> ErrorReturn: algorithm failure
    
    VirtualGrid --> OutputValidation: success
    VirtualGrid --> ErrorReturn: algorithm failure
    
    OutputValidation --> Success: output valid
    OutputValidation --> ErrorReturn: output out of range
    
    ErrorReturn --> Idle: return original image + error code
    Success --> Idle: return processed image
```

**SAFE-001/003 구현 원칙:**
- `gsvg_process()` 진입 시 `ImageBuffer::deepCopy()`로 원본 보관
- 모든 처리 단계를 try-catch로 wrapping
- 어떤 exception/error에서든 보관된 원본을 output buffer에 복사 후 에러 코드 반환

---

## 8. Architecture Verification Checklist

IEC 62304:2015 §5.3.6 기준:

| Item | Status |
|------|--------|
| Software items 식별 완료 | ✓ (SI-001 ~ SI-004) |
| Software items 간 interfaces 정의 | ✓ (Section 2 diagram) |
| SOUP 식별 및 functional/performance requirements 정의 | ✓ (Section 6 + GSVG-SOUP-001) |
| Risk control measures가 architecture에 반영 | ✓ (Section 7 — error handling) |
| Architecture가 SRS requirements를 완전히 커버 | ✓ (GSVG-RTM-001에서 추적) |

---

## Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0 | 2026-04-03 | — | Initial release |
