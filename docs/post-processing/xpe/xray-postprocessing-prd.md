# PRD: Medical X-ray Post-Processing Engine (XPE)

> **SUPERSEDED FOR EXECUTION PLANNING** (2026-04-13)  
> This document is superseded by `XPE-PRD-002_Detailed_Project_Execution_PRD.md` for phase structure, execution planning, and runtime packaging.  
> For normative algorithm behavior, refer to `ALG-SPEC-001` (xpe-algorithm-spec-deepsync.md v2.0.0-ds1).  
> Safety class has been re-evaluated to **Class B** (pending hazard analysis confirmation). See XPE-PRD-002 Section 14.1.  
> This document remains valid as the original requirements baseline and algorithm reference.  
> Cross-Verification: XPE-XVER-001 (2026-04-13)

**Document ID:** XPE-PRD-2026-001 v1.0  
**Date:** 2026-04-02  
**Classification:** Confidential  
**Regulatory Scope:** IEC 62304 Class C, FDA 21 CFR 820.30, EU MDR 2017/745, ISO 14971  

---

## 1. Executive Summary

본 문서는 의료용 X-ray 촬영 소프트웨어에 탑재되는 Post-Processing 엔진(XPE)의 요구사항과 개발 전략을 정의한다. FPD(Flat Panel Detector)에서 획득된 Raw 이미지로부터 진단 품질의 최종 출력까지, **3단계 개발 전략**으로 영상처리 파이프라인 전체를 구축한다.

| Phase | 명칭 | 기간 | 핵심 목표 |
|-------|------|------|-----------|
| Phase 1 | **Foundation** | 24주 | 최소 필수 파이프라인 (Offset/Gain → W/L → DICOM 출력) |
| Phase 2 | **Clinical** | 20주 | 멀티스케일 처리, Body-Part Adaptive, Stitching |
| Phase 3 | **Intelligence** | 16주 | DL 기반 Bone Suppression, CAD Integration, AI-DES |

---

## 2. System Architecture Overview

### 2.1 Image Processing Pipeline (DICOM Grayscale Pipeline 준수)

```
Raw Detector Data (14-16bit)
    │
    ├─ [Pre-Processing] ──────────────── Phase 1
    │   ├─ Dark/Offset Correction
    │   ├─ Gain/Flat-Field Correction
    │   ├─ Defective Pixel Correction
    │   └─ Ghost/Lag Correction
    │
    ├─ [Core Processing] ─────────────── Phase 1-2
    │   ├─ Logarithmic Transform
    │   ├─ Noise Reduction (Spatial/Freq)
    │   ├─ Contrast Enhancement
    │   ├─ Edge Enhancement
    │   └─ Multiscale Frequency Processing
    │
    ├─ [Advanced Processing] ──────────── Phase 2-3
    │   ├─ Body-Part Recognition
    │   ├─ ROI Auto-Detection
    │   ├─ Image Stitching (Long-Leg/Spine)
    │   ├─ Bone Suppression (DL)
    │   └─ Virtual Grid / Scatter Correction
    │
    └─ [Display Processing] ──────────── Phase 1
        ├─ Modality LUT (Rescale Slope/Intercept)
        ├─ VOI LUT (Window/Level, SIGMOID)
        ├─ Presentation LUT (GSDF P-Values)
        └─ DICOM Grayscale Softcopy Presentation State
```

### 2.2 데이터 흐름 규격

| Parameter | Specification |
|-----------|--------------|
| Input Bit Depth | 14-bit (16,384 levels), 16-bit (65,536 levels) |
| Internal Processing | 32-bit float (IEEE 754) |
| Output Bit Depth | 12-bit (DICOM FOR PRESENTATION) / 16-bit (FOR PROCESSING) |
| Matrix Size | 최대 4096 × 4096 (17M pixels) |
| Pixel Pitch | 100-200 μm (detector dependent) |
| Throughput Target | ≤ 3초 (3072×3072, Phase 1), ≤ 5초 (full pipeline) |

---

## 3. Phase 1: Foundation — 최소 필수 파이프라인

**목표:** Raw 이미지를 진단 가능한 DICOM 영상으로 변환하는 핵심 경로 확립  
**기간:** 24주 (6 Sprint × 4주)

### 3.1 Pre-Processing Module

#### 3.1.1 REQ-PP-001: Dark/Offset Correction

**목적:** Detector의 X-ray 미조사 시 고유 신호(dark current) 제거

```
Corrected(x,y) = Raw(x,y) - Offset(x,y)
```

| Requirement | Specification |
|-------------|--------------|
| Offset Map 생성 | N ≥ 16 dark frames 평균, outlier rejection (3σ) |
| 갱신 주기 | 매 calibration cycle 또는 온도 변화 ≥ 2°C |
| 연산 정밀도 | 16-bit integer saturated arithmetic (negative → 0 clamp) |
| 성능 | ≤ 50ms (3072×3072, FPGA offload 가능) |

**참조:** Offset correction은 detector lag signal을 dark image에 포함시킬 수 있으므로, 촬영 직후가 아닌 안정화 후 수행해야 한다.

#### 3.1.2 REQ-PP-002: Gain/Flat-Field Correction

**목적:** Pixel 간 감도 차이 및 X-ray beam non-uniformity(Heel Effect) 보정

```
Corrected(x,y) = [Raw(x,y) - Offset(x,y)] × GainMap(x,y)
GainMap(x,y) = MeanFlood / [Flood(x,y) - Offset(x,y)]
```

| Requirement | Specification |
|-------------|--------------|
| Flood Image 취득 | 동일 kVp/mA 조건, N ≥ 8 frames 평균 |
| 지원 SID | 100/110/130/180 cm (SID별 개별 gain map) |
| Heel Effect 보정 | Large-kernel Gaussian 기반 분리 또는 다항식 fitting |
| 연산 | Float32 multiply, 결과 clamping to [0, 2^16-1] |

#### 3.1.3 REQ-PP-003: Defective Pixel Correction

**목적:** TFT 제조 결함 및 열화에 의한 불량 화소 보정

| Defect Type | Detection Method | Correction Method |
|-------------|-----------------|-------------------|
| Point Defect | Threshold (± 6σ from mean) | 4/8-neighbor interpolation |
| Cluster Defect (≤ 5×5) | Connected-component labeling | Bilinear interpolation from edge pixels |
| Line Defect (row/column) | Row/Column mean deviation | Adjacent row/column interpolation |

| Requirement | Specification |
|-------------|--------------|
| Bad Pixel Map 관리 | Factory map + runtime detection (periodic update) |
| 최대 허용 불량률 | Point: 0.01%, Cluster: 10개/panel, Line: 3 lines/panel |
| 실시간 검출 | Flat-field 취득 시 자동 갱신 |
| Edge 보존 | Template Matching Correction (TMC) 기반 알고리즘 우선 적용 |

#### 3.1.4 REQ-PP-004: Ghost/Lag Correction

**목적:** 이전 exposure의 잔류 신호(image lag) 제거

| Requirement | Specification |
|-------------|--------------|
| 보정 방식 | Recursive temporal filtering (exponential decay model) |
| Lag Model | Multi-exponential: `Lag(t) = Σ αᵢ × exp(-t/τᵢ)`, i=1..3 |
| Parameter | Panel-specific (a-Si: τ₁=0.3s, τ₂=2s, τ₃=30s typical) |
| Target | ≥ 90% ghost signal removal (1st frame after high-dose exposure) |
| Forward Bias 연동 | AFE2256 TP-β Dual Timing Profile 지원 |

> **Note:** 현재 AUO R1717 + NT39565D + AFE2256GR 조합에서 Forward Bias 적용 시 ~90% ghost removal 확인됨 (ghost-correction repo 참조)

### 3.2 Core Processing Module

#### 3.2.1 REQ-CP-001: Logarithmic Transform

**목적:** Detector 선형 응답을 optical density 도메인으로 변환

```
LogImage(x,y) = -ln[Corrected(x,y) / I₀]
```

| Requirement | Specification |
|-------------|--------------|
| I₀ 결정 | Unattenuated region 자동 검출 또는 exposure parameter 기반 |
| Zero/Negative 처리 | ε clamping (1e-6) before ln() |
| 출력 범위 | [0.0, 4.0] OD equivalent, 매핑 to 16-bit |

#### 3.2.2 REQ-CP-002: Noise Reduction

| Algorithm | Use Case | Parameters |
|-----------|----------|------------|
| Gaussian Low-pass | Global smoothing baseline | σ = 0.5-2.0 pixels |
| Median Filter (3×3, 5×5) | Salt-and-pepper noise | Kernel size adaptive |
| Bilateral Filter | Edge-preserving denoising | σ_spatial=2.0, σ_range=0.1 |
| Non-Local Means (NLM) | High-quality denoising | Patch=7×7, Search=21×21, h=auto |

| Requirement | Specification |
|-------------|--------------|
| Default | Bilateral Filter (속도-품질 균형) |
| High-Quality Mode | NLM (throughput trade-off 허용 시) |
| Noise Estimation | MAD (Median Absolute Deviation) 기반 자동 σ 추정 |
| 성능 | Bilateral ≤ 200ms, NLM ≤ 800ms (3072×3072) |

#### 3.2.3 REQ-CP-003: Contrast Enhancement

| Algorithm | Description | Priority |
|-----------|-------------|----------|
| Histogram Equalization | Global contrast stretch | Baseline (validation용) |
| CLAHE | Block 단위 adaptive equalization | **Primary (Phase 1)** |
| Unsharp Masking (USM) | High-pass + original blending | **Primary (Phase 1)** |

**CLAHE Parameters:**

| Parameter | Default | Range |
|-----------|---------|-------|
| Block Size | 8×8 | 4×4 - 16×16 |
| Clip Limit | 2.0 | 1.0 - 4.0 |
| Output Bins | 256 | 128 - 1024 |

**Unsharp Masking:**

```
Enhanced(x,y) = Original(x,y) + k × [Original(x,y) - Blurred(x,y)]
```

| Parameter | Default | Range |
|-----------|---------|-------|
| Kernel σ | 2.0 | 0.5 - 5.0 |
| Gain (k) | 1.5 | 0.5 - 3.0 |
| Threshold | 10 (gray levels) | 0 - 50 |

#### 3.2.4 REQ-CP-004: Edge Enhancement

| Requirement | Specification |
|-------------|--------------|
| Scalable Edge Enhancement | Frequency-selective sharpening (fine/medium/coarse) |
| Overshoot Control | Gain limiting per frequency band to prevent halo artifacts |
| Body-Part Preset | Chest: low gain / Extremity: high gain / Spine: medium |

### 3.3 Display Processing Module (DICOM Grayscale Pipeline)

#### 3.3.1 REQ-DP-001: Modality LUT

```
Output = Stored_Value × Rescale_Slope + Rescale_Intercept
```

| Requirement | Specification |
|-------------|--------------|
| Rescale Slope/Intercept | DICOM tag (0028,1053) / (0028,1052) 준수 |
| Pixel Representation | Unsigned (0000H) for DR |
| Rescale Type | Unspecified (detector-dependent) |

#### 3.3.2 REQ-DP-002: VOI LUT (Window/Level)

| Function | Formula | Use Case |
|----------|---------|----------|
| LINEAR | `y = ((x - c) / w + 0.5) × (ymax - ymin) + ymin` | 기본 표시 |
| LINEAR_EXACT | 동일 (boundary behavior 차이) | 정밀 제어 |
| SIGMOID | `y = ymax / (1 + exp(-4(x-c)/w))` | Film-like H&D curve 시뮬레이션 |

| Requirement | Specification |
|-------------|--------------|
| 사전정의 Preset | Body-part별 최소 20개 (Chest, Abdomen, Bone, Soft-tissue 등) |
| 사용자 조정 | 실시간 W/L drag (≤ 16ms latency) |
| Multi-VOI | DICOM multi-value W/L 지원 (alternative view) |
| DICOM 저장 | Window Center (0028,1050), Window Width (0028,1051), VOI LUT Function (0028,1056) |

#### 3.3.3 REQ-DP-003: Presentation LUT / GSDF

| Requirement | Specification |
|-------------|--------------|
| DICOM Part 14 | Grayscale Standard Display Function (GSDF) 준수 |
| P-Value 출력 | Perceptually linear luminance space |
| Photometric Interpretation | MONOCHROME1 (bone=dark) / MONOCHROME2 (bone=bright) 자동 처리 |
| Presentation LUT Shape | IDENTITY / INVERSE 지원 |
| Calibration | Display 장치별 GSDF LUT 생성 도구 포함 |

#### 3.3.4 REQ-DP-004: DICOM Compliance

| Requirement | Specification |
|-------------|--------------|
| IOD | Digital X-Ray Image (DX) — SOP Class 1.2.840.10008.5.1.4.1.1.1.1 |
| Presentation State | Grayscale Softcopy Presentation State Storage |
| Transfer Syntax | Explicit VR Little Endian, JPEG 2000 Lossless |
| Mandatory Tags | All Type 1/2 per DICOM PS3.3 DX IOD |
| FOR PROCESSING / FOR PRESENTATION | 양방향 지원, Presentation Intent Type (0008,0068) |

### 3.4 Look-Up Table (LUT) 관리 시스템

| Requirement | Specification |
|-------------|--------------|
| Exam-Type LUT | Body-part / projection 별 최적화 LUT 세트 |
| LUT 저장 형식 | JSON + binary (16-bit entry), DICOM LUT Sequence 호환 |
| 사용자 커스텀 LUT | GUI에서 생성/편집/저장/내보내기 |
| 자동 LUT 선택 | Exam code → LUT mapping (DICOM Scheduled Procedure Step) |

---

## 4. Phase 2: Clinical — 고급 영상처리

**목표:** 상용 시스템 수준의 영상 품질 달성 (MUSICA-class MFP)  
**기간:** 20주 (5 Sprint × 4주)  
**전제:** Phase 1 완료, IEC 62304 Unit Test Coverage ≥ 80%

### 4.1 Multiscale Frequency Processing (MFP)

#### 4.1.1 REQ-MFP-001: Laplacian Pyramid Decomposition

**목적:** 영상을 다중 주파수 대역으로 분리하여 각 scale 독립 처리

```
Image = Σ(k=0..N) Laplacian_k + Residual_N
```

| Requirement | Specification |
|-------------|--------------|
| Decomposition Levels | 8-12 (pixel pitch에 따라 조정) |
| Pyramid 방식 | Laplacian (Burt-Adelson) 또는 Wavelet (Haar/Daubechies) |
| 각 Level 처리 | Non-linear gain function: `g(x) = a × sign(x) × |x|^p` |
| Noise Suppression | Level-dependent: 미세 scale은 suppression, 거친 scale은 boost |
| Dynamic Range Compression | Residual (저주파)에 compressive nonlinearity 적용 |

#### 4.1.2 REQ-MFP-002: Fractional Multiscale Processing (FMP)

**목적:** 급격한 밀도 전환부(bone-soft tissue boundary)에서 artifact 없는 렌더링

| Requirement | Specification |
|-------------|--------------|
| Fractional Decomposition | Scale 간 intermediate fraction 분해 |
| Transition Zone 처리 | Soft-tissue / bone boundary에서 graded processing |
| Implant Handling | Metal 주변 shadow artifact suppression |
| 결과 | Trabecular bone sharpness + soft tissue transparency 동시 달성 |

#### 4.1.3 REQ-MFP-003: Auto-Optimization

| Requirement | Specification |
|-------------|--------------|
| Body-Part Independent | 입력 영상 자체에서 최적 parameter 자동 결정 |
| Histogram Analysis | CNR mask 기반 relevant region 검출 |
| Parameter 자동 조정 | Contrast / Brightness / Sharpness 3-axis |
| Fallback | Manual override 항상 가능 |

### 4.2 Body-Part Recognition & Adaptive Processing

#### 4.2.1 REQ-BPR-001: Automatic Body-Part Recognition

| Requirement | Specification |
|-------------|--------------|
| Classification | 최소 15 categories (Chest PA/Lat, C-spine, T-spine, L-spine, Pelvis, Hip, Knee, Ankle, Foot, Hand, Wrist, Elbow, Shoulder, Abdomen, Skull) |
| 방법 | CNN classifier (MobileNet-v3 class) |
| 정확도 | ≥ 95% Top-1 accuracy |
| Fallback | DICOM Body Part Examined (0018,0015) tag 참조 |
| 출력 | Processing parameter set 자동 선택 |

#### 4.2.2 REQ-BPR-002: Collimation Detection

| Requirement | Specification |
|-------------|--------------|
| 검출 대상 | X-ray 조사야 경계 (lead masking / collimator edge) |
| 방법 | Gradient-based edge detection + Hough transform |
| 기능 | 조사야 외 영역 자동 마스킹 (black / white) |
| 정확도 | ≥ 98% (rectangular collimation) |

#### 4.2.3 REQ-BPR-003: Exposure Index Calculation

| Requirement | Specification |
|-------------|--------------|
| 표준 | IEC 62494-1 (Exposure Index, Target EI, Deviation Index) |
| ROI 결정 | Collimation 영역 내 relevant region 자동 검출 |
| 출력 | EI, EI_target, DI (DICOM tags 포함) |

### 4.3 Image Stitching (Panoramic)

#### 4.3.1 REQ-STI-001: Full-Spine Stitching

| Requirement | Specification |
|-------------|--------------|
| 입력 | 2-4장 연속 촬영 (10-30% overlap) |
| Registration | Phase correlation → sub-pixel refinement |
| Blending | Multi-band weighted blending (seam-free) |
| 밝기 보정 | Overlap 영역 기반 gain/offset alignment |
| 정확도 | Scoliosis Cobb angle 오차 ≤ 2° |
| 시간 | ≤ 5초 (3-image stitch) |

#### 4.3.2 REQ-STI-002: Long-Leg Stitching

| Requirement | Specification |
|-------------|--------------|
| 입력 | 3장 (Hip + Knee + Ankle) |
| Registration | Feature-based (Canny edge + bone edge alignment) |
| HKA Angle | Stitched image에서 자동 측정 제공 |
| 정확도 | HKA 오차 ≤ 1° |

### 4.4 Measurement & Annotation Tools

| Tool | Specification |
|------|--------------|
| Distance | 2-point linear measurement (mm), calibrated by pixel pitch |
| Angle (Cobb) | 3-point angle, auto snap to vertebral endplate |
| ROI Statistics | Rectangle/Ellipse/Freehand — mean, std, min, max, area |
| Arrow/Text | Free annotation with persistence via DICOM Presentation State |
| Magnification | 2x, 4x, pixel-level zoom with interpolation method selection |

---

## 5. Phase 3: Intelligence — AI/DL 기반 고급 기능

**목표:** DL 기반 진단 보조 기능 탑재  
**기간:** 16주 (4 Sprint × 4주)  
**전제:** Phase 2 완료, FDA SaMD regulatory pathway 확보

### 5.1 DL-Based Bone Suppression

#### 5.1.1 REQ-DL-001: Virtual Dual-Energy Subtraction

| Requirement | Specification |
|-------------|--------------|
| 방법 | Single-shot CXR → Virtual Soft-Tissue Image 생성 |
| Architecture | Residual U-Net (encoder-decoder with skip connections) |
| Training Data | DES CXR paired dataset (≥ 500 cases) 또는 DRR 기반 합성 |
| 품질 지표 | PSNR ≥ 33 dB, SSIM ≥ 0.97 (vs real DES) |
| Bone Suppression Ratio | ≥ 80% (lung field region) |
| 추론 시간 | ≤ 2초 (GPU), ≤ 10초 (CPU fallback) |
| Toggle | Soft-tissue / Bone / Original 3-view 전환 |

### 5.2 DL-Based Denoising

#### 5.2.1 REQ-DL-002: Low-Dose Enhancement

| Requirement | Specification |
|-------------|--------------|
| 목적 | 저선량 촬영 영상의 noise 제거 (ALARA 원칙 지원) |
| Architecture | DnCNN 또는 Noise2Noise variant |
| Training | Paired low-dose / standard-dose dataset (≥ 1000 cases) |
| 품질 | PSNR improvement ≥ 5 dB over input |
| 안전성 | Pathological feature 보존 검증 필수 (reader study) |

### 5.3 Super-Resolution

#### 5.3.1 REQ-DL-003: Resolution Enhancement

| Requirement | Specification |
|-------------|--------------|
| Scale Factor | 2× (200μm → 100μm equivalent) |
| Architecture | SRGAN / Real-ESRGAN variant (medical domain fine-tuned) |
| 제한 | Research / 참고 목적, 진단 primary interpretation에는 미사용 |
| Disclaimer | "AI-enhanced resolution" 워터마크 표시 |

### 5.4 CAD Integration Framework

#### 5.4.1 REQ-CAD-001: Plugin Architecture

| Requirement | Specification |
|-------------|--------------|
| Interface | Standardized REST API + DICOM SR output |
| 지원 결과 타입 | Bounding Box, Heatmap, Probability Score, Structured Report |
| Worklist 연동 | DICOM Worklist → AI Routing → Result overlay |
| 3rd Party | ONNX Runtime 기반 모델 로딩 (vendor-agnostic) |
| FDA Pathway | SaMD Pre-cert 또는 510(k) per algorithm |

#### 5.4.2 REQ-CAD-002: Supported AI Tasks (Plug-in)

| Task | Clinical Application | Regulatory Class |
|------|---------------------|-----------------|
| Pneumothorax Detection | ER triage | FDA Class II (QIH) |
| Lung Nodule Detection | Screening support | FDA Class II (QIH) |
| Fracture Detection | Extremity / Spine | FDA Class II (MYN) |
| Cardiomegaly | Cardiac screening | FDA Class II |
| Tube/Line Detection | ICU monitoring | FDA Class II |

---

## 6. Non-Functional Requirements

### 6.1 Performance

| Metric | Target | Measurement |
|--------|--------|-------------|
| Pre-Processing Latency | ≤ 500ms | 3072×3072, CPU single-thread |
| Full Pipeline (Phase 1) | ≤ 3s | End-to-end, i7-12th gen |
| Full Pipeline (Phase 2) | ≤ 5s | MFP 포함 |
| DL Inference (Phase 3) | ≤ 2s | NVIDIA RTX 3060+ |
| W/L Interactive | ≤ 16ms | Display refresh |
| Memory Usage | ≤ 2GB | Peak, per image processing |

### 6.2 Quality Metrics

| Metric | Definition | Acceptance Criteria |
|--------|-----------|-------------------|
| SNR | Signal-to-Noise Ratio | ≥ detector theoretical DQE limit의 90% 유지 |
| CNR | Contrast-to-Noise Ratio | Enhancement 후 ≥ 1.5× baseline |
| MTF Preservation | Modulation Transfer Function | ≥ 90% at Nyquist/2 after processing |
| Artifact Freedom | Visual artifact assessment | Reader score ≥ 4/5 (5-point scale) |

### 6.3 Regulatory & Standards Compliance

| Standard | Scope |
|----------|-------|
| IEC 62304:2015 | SW development lifecycle (Class C) |
| ISO 14971:2019 | Risk management |
| IEC 62366-1:2015 | Usability engineering |
| DICOM PS3.3/3.4/3.14 | Image IOD, Service Classes, GSDF |
| FDA 21 CFR 820.30 | Design controls |
| EU MDR 2017/745 | CE marking (Class IIa) |
| IEC 62563-1 | Display GSDF calibration |

### 6.4 Software Architecture Constraints

| Constraint | Specification |
|-----------|--------------|
| Language | C++ 17 (engine core), C# (GUI/WPF integration) |
| Build System | CMake 3.20+ |
| GPU Acceleration | CUDA 12+ (NVIDIA), OpenCL 3.0 fallback |
| SIMD | AVX2/AVX-512 (x86-64), NEON (ARM) |
| Threading | Thread pool, 논리 코어 수 기반 자동 scaling |
| Memory | Zero-copy pipeline (shared buffer pool) |
| API | C ABI export (DLL/SO) → C# P/Invoke |
| Test | Google Test + CTest, Coverage ≥ 80% |
| CI/CD | Gitea + GitHub Actions (cross-platform build) |

---

## 7. Risk Analysis (Top-Level)

| Risk ID | Risk | Severity | Mitigation |
|---------|------|----------|------------|
| R-001 | DL bone suppression이 pathology를 함께 제거 | Critical | Mandatory reader study (N≥30), toggle 기능 필수 |
| R-002 | MFP parameter 오류로 진단 정보 왜곡 | High | Body-part preset validation, undo/원본 복원 필수 |
| R-003 | Defective pixel 미검출로 진단 오류 | High | Dual-detection (factory + runtime), periodic QC |
| R-004 | Image stitching 정합 오류로 측정 부정확 | High | 정합 confidence score 표시, 수동 보정 UI |
| R-005 | Ghost artifact가 병변으로 오인 | High | Forward Bias + SW correction 이중 보정 |
| R-006 | GSDF 미준수로 미묘한 병변 비가시 | Medium | Display calibration tool 제공, 주기적 검증 |

---

## 8. Algorithm Specification Details

### 8.1 CLAHE (Phase 1) — Detailed Specification

```
Algorithm: Contrast Limited Adaptive Histogram Equalization

Input:  Image I[M×N], 16-bit unsigned
Params: BlockSize (Bx, By), ClipLimit (CL), NumBins (NB)

1. Divide I into (M/Bx) × (N/By) contextual regions
2. For each region:
   a. Compute histogram H[0..NB-1]
   b. Clip: excess = Σ max(0, H[k] - CL × Bx×By/NB)
   c. Redistribute: H[k] += excess / NB
   d. Compute CDF → mapping function T_region[k]
3. For each pixel (x,y):
   a. Determine 4 nearest region centers
   b. Bilinear interpolation of 4 mapping functions
   c. Output = interpolated_T(I[x,y])
```

### 8.2 Multiscale Decomposition (Phase 2)

```
Algorithm: Laplacian Pyramid with Non-Linear Amplification

1. Build Gaussian pyramid: G₀=I, Gₖ₊₁ = Reduce(Gₖ)
2. Build Laplacian pyramid: Lₖ = Gₖ - Expand(Gₖ₊₁)
3. For each level k (0..N-1):
   a. Estimate noise σₖ (from MAD of Lₖ)
   b. Apply non-linear gain:
      Lₖ'(x,y) = gₖ(Lₖ(x,y))
      where gₖ(x) = aₖ × sign(x) × |x|^pₖ  (pₖ < 1 for boost, > 1 for suppress)
   c. Noise gate: if |Lₖ(x,y)| < tₖ×σₖ, suppress
4. Reconstruct: I' = Σₖ Lₖ' + G_N (optionally compressed)
```

### 8.3 Bone Suppression U-Net (Phase 3)

```
Architecture: Residual U-Net

Encoder:
  Conv3×3(1,64) → BN → ReLU → Conv3×3 → BN → ReLU → MaxPool2×2
  Conv3×3(64,128) → ... → MaxPool2×2
  Conv3×3(128,256) → ... → MaxPool2×2
  Conv3×3(256,512) → ... → MaxPool2×2

Bottleneck:
  Conv3×3(512,1024) → BN → ReLU → Conv3×3 → BN → ReLU

Decoder (with skip connections + residual):
  UpConv2×2(1024,512) → Concat(skip) → Conv3×3 → ... 
  UpConv2×2(512,256)  → Concat(skip) → Conv3×3 → ...
  UpConv2×2(256,128)  → Concat(skip) → Conv3×3 → ...
  UpConv2×2(128,64)   → Concat(skip) → Conv3×3 → ...

Output:
  Conv1×1(64,1) → Sigmoid (bone probability map)

Loss: L1 + 0.1 × Perceptual(VGG16_features) + 0.01 × SSIM
```

---

## 9. Validation & Verification Strategy

### 9.1 Unit Testing

| Module | Coverage Target | Framework |
|--------|----------------|-----------|
| Pre-Processing | ≥ 90% | Google Test |
| Core Processing | ≥ 85% | Google Test |
| Display Processing | ≥ 90% | Google Test + pixel comparison |
| DICOM I/O | ≥ 85% | Google Test + DICOM validator |

### 9.2 Integration Testing

| Test Case | Method |
|-----------|--------|
| Full Pipeline Regression | 50+ phantom images, automated PSNR/SSIM 비교 |
| DICOM Conformance | DVTk / dcm4che validation suite |
| Cross-Platform | Windows 11 + Linux (Docker) build & test |

### 9.3 Clinical Validation

| Phase | Validation |
|-------|-----------|
| Phase 1 | Phantom study (CDRAD 2.0) + 100 clinical images reader study |
| Phase 2 | 300 clinical images, 3 radiologists, 5-point IQ scoring |
| Phase 3 (DL) | Multi-reader multi-case (MRMC) study, N≥30 readers, N≥300 cases |

---

## 10. Development Schedule

| Sprint | Weeks | Deliverables |
|--------|-------|-------------|
| **Phase 1** | | |
| S1.1 | W1-4 | Offset/Gain/Defect Pixel Correction |
| S1.2 | W5-8 | Ghost Correction + Log Transform |
| S1.3 | W9-12 | Noise Reduction (Bilateral + NLM) |
| S1.4 | W13-16 | CLAHE + USM + Edge Enhancement |
| S1.5 | W17-20 | Modality LUT + VOI LUT + GSDF |
| S1.6 | W21-24 | DICOM I/O + LUT Management + Phase 1 V&V |
| **Phase 2** | | |
| S2.1 | W25-28 | Laplacian Pyramid MFP (8-level) |
| S2.2 | W29-32 | FMP + Auto-Optimization |
| S2.3 | W33-36 | Body-Part Recognition + Collimation Detection |
| S2.4 | W37-40 | Image Stitching (Spine + Leg) |
| S2.5 | W41-44 | Measurement Tools + Phase 2 V&V |
| **Phase 3** | | |
| S3.1 | W45-48 | Bone Suppression U-Net Training + Integration |
| S3.2 | W49-52 | DL Denoising + Super-Resolution |
| S3.3 | W53-56 | CAD Plugin Framework + ONNX Runtime |
| S3.4 | W57-60 | Phase 3 V&V + MRMC Study |

---

## 11. Quality Metrics & Acceptance Criteria Summary

| Criterion | Phase 1 | Phase 2 | Phase 3 |
|-----------|---------|---------|---------|
| Pipeline Latency | ≤ 3s | ≤ 5s | ≤ 7s (DL incl.) |
| IQ Score (reader) | ≥ 3.5/5 | ≥ 4.0/5 | ≥ 4.2/5 |
| DICOM Conformance | 100% Type 1/2 | + Presentation State | + SR |
| Unit Test Coverage | ≥ 80% | ≥ 85% | ≥ 80% |
| Defect Density | ≤ 5/KLOC | ≤ 3/KLOC | ≤ 3/KLOC |
| Regulatory | IEC 62304 Class C plan | 510(k) submission ready | SaMD pathway |

---

## 12. References

1. Seeram E. "Image Postprocessing in Digital Radiology—A Primer for Technologists." JMIRS, 2008.
2. Notohamiprodjo S, et al. "Advances in multiscale image processing and its effects on image quality in skeletal radiography." Sci Rep 12, 4726 (2022).
3. AGFA HealthCare. "MUSICA Fractional Multiscale Processing." Technical Whitepaper, 2013.
4. Walz-Flannigan A, et al. "Artifacts in Digital Radiography." AJR 198:156-161, 2012.
5. Hong E, et al. "Using deep learning for pixel-defect corrections in flat-panel radiography imaging." J Med Imaging 8(1), 2021.
6. Kim JH, et al. "Feasibility study of deep-learning-based bone suppression incorporated with SEMD technique in chest X-rays." Br J Radiol 95, 2022.
7. Sivakumar R, et al. "FDA Approval of AI/ML Devices in Radiology: A Systematic Review." JAMA Netw Open 8(11), 2025.
8. DICOM PS3.3, PS3.4, PS3.14 — NEMA Standards.
9. Yang C, et al. "Improvement of automated image stitching system for DR X-ray images." Comput Biol Med 71, 2016.
10. IEC 62304:2015, ISO 14971:2019, IEC 62366-1:2015.

---

*Document End — XPE-PRD-2026-001 v1.0*
