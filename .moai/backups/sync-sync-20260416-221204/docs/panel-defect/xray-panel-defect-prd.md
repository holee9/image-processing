# X-ray Panel Defect Correction - Product Requirements Document

**Document ID:** xray-panel-defect-prd v1.0  
**Module:** `xpe_preprocess.dll` (Stage 3, Layer 1)  
**Safety Classification:** IEC 62304 Class B  
**Date:** 2026-04-14  
**Author:** XPE Panel Defect Development Team  
**Language:** Korean (user-facing), English (technical specifications)  
**Approval:** __________________ Date: __________  

---

## 목차

1. [개요](#개요)
2. [불량 픽셀 분류체계](#불량-픽셀-분류체계)
3. [검출 알고리즘 수학 명세](#검출-알고리즘-수학-명세)
4. [보정 알고리즘 수학 명세](#보정-알고리즘-수학-명세)
5. [Min/Normal/Max 프로필 파라미터](#minnormalmax-프로필-파라미터)
6. [성능 목표](#성능-목표)
7. [참고문헌](#참고문헌)

---

## 개요

### 목표 및 범위

Panel Defect Correction Module (`xpe_preprocess.dll`, Stage 3, Layer 1)은 X-ray 플랫-패널 방사선 촬영에서 다음 네 가지 결함 유형을 검출하고 보정합니다:

- **고립 픽셀 결함** (단일 픽셀): 데드 픽셀, 핫 픽셀, 깜빡이는 픽셀, 스텍 픽셀
- **클러스터 결함**: 3×3 및 5×5 블록 크기 내 인접 불량 픽셀
- **라인 결함**: 1~5 픽셀 폭의 연속 행/열 결함 (Type 1, 3, 5 분류)
- **그리드/모아레 아티팩트**: Anti-scatter grid로 인한 주기적 패턴 및 고주파 잡음

### 검출기 사양

| 항목 | 값 |
|------|-----|
| **Detector** | AUO R1717 |
| **해상도** | 3072 × 3072 pixels |
| **변환 방식** | Indirect (CsI:Tl scintillator + a-Si TFT) |
| **입력 형식** | float32 (Gain Correction 후) |
| **출력 형식** | float32 (Defect 보정 후) |
| **처리 시간** | < 95 ms/frame (3072×3072) |

### 알고리즘 파이프라인

```
Step 0: Static BPM Generation (Calibration-time)
   ├─ Dark frame analysis (HotPixelMask)
   ├─ Flat-field analysis (ColdPixelMask)
   ├─ Temporal CV analysis (FlickeringPixelMask)
   ├─ Line defect map generation
   └─ Cluster detection

Step 1: Dynamic Defect Detection (Per-frame)
   ├─ Residual map computation (local median subtraction)
   ├─ k·σ local thresholding
   └─ Defect candidate map generation

Step 2: Morphology Classification
   ├─ Connected component labeling
   ├─ Bounding box analysis
   └─ Defect type classification (isolated, 3×3, 5×5, line)

Step 3: Cluster Correction (3×3, 5×5)
   ├─ 3×3: ANN (40→9) correction
   ├─ 5×5: ANN (56→25) + optional TMC refinement
   └─ All 9/25 cluster pixels replaced

Step 4: Line Defect Correction (Type 1, 3, 5)
   ├─ Type 1 (diffVal > T2): Direct interpolation from adjacent lines
   ├─ Type 3 (T1 < diffVal ≤ T2): Edge-aware + quadratic fitting
   └─ Type 5 (diffVal ≤ T1): Light smoothing or no change

Step 5: Grid/Moiré Detection & Suppression
   ├─ DWT-based energy analysis (Moiré Severity Index)
   ├─ Optional: DCT dynamic segmentation
   └─ Suppression via bandstop filter + GRD (experimental)

Step 6: Output
   └─ float32 defect-corrected image
```

---

## 불량 픽셀 분류체계

### 1. 고립 픽셀 (Isolated Pixels)

**정의**: 연결된 구성 요소 (Connected Component) 크기 = 1 pixel

**타입**:

| 타입 | 신호 특성 | 원인 | 예상 빈도 |
|------|---------|------|---------|
| **Dead Pixel** | No signal (< 10% of local mean) | a-Si defect, open circuit | 0.01-0.1% of panel |
| **Hot Pixel** | Excessive signal (> 3σ above local mean) | Leakage current, trapped charge | 0.05-0.2% of panel |
| **Flickering Pixel** | High temporal CV (CV > 5%) | Unstable TFT, intermittent contact | 0.001-0.01% of panel |
| **Stuck Pixel** | Constant value independent of exposure | TFT saturation, latch-up | 0.0001-0.001% of panel |

**검출 방법**: RMM (Robust Mask Maker) with λ=8.0
$$\text{SNR}(i) = \frac{|x(i) - \hat{\mu}|}{\hat{\sigma}} > \lambda \implies \text{defect candidate}$$

where $\hat{\mu}$ and $\hat{\sigma}$ are robust mean/std estimated from dark frame or residual map.

### 2. 클러스터 결함 (Cluster Defects)

**정의**: Connected component size > 1 but fits within bounding box

| 타입 | Bounding Box | Pixel Count | Correction |
|------|-------------|------------|-----------|
| **3×3 Cluster** | ≤ 3×3 | ≤ 9 | ANN (40→9) |
| **5×5 Cluster** | ≤ 5×5 | ≤ 25 | ANN (56→25) + TMC |

**ANN 아키텍처**:

- **3×3 Cluster**:
  - Input: 40-dim vector (7×7 neighborhood minus center 3×3)
  - Hidden: none (single-layer)
  - Output: 9-dim vector (3×3 defect block)
  - Activation: Linear (y = Wx + b)
  - Reference: Jeon et al., PMC7930811

- **5×5 Cluster**:
  - Input: 56-dim vector (9×9 neighborhood minus center 5×5)
  - Hidden: 64 units with ReLU activation
  - Output: 25-dim vector (5×5 defect block)
  - Reference: Lee et al. FPD defect correction study
  - Optional: Template Matching Correlation (TMC) from 27×27 region

### 3. 라인 결함 (Line Defects)

**정의**: Elongated connected component (width 1-5 pixels, length >> width) along row or column

**이상도(Anomaly Degree, diffVal) 계산**:

$$\text{diffVal}(i) = \frac{1}{L} \sum_{j=1}^{L} \frac{|p_{\text{defect}}(i,j) - \bar{p}_{\text{adjacent}}(j)|}{\text{max}(p_{\text{adjacent}}(j))} \times 100\%$$

where:
- $p_{\text{defect}}(i,j)$ = pixel value at defect line
- $\bar{p}_{\text{adjacent}}(j)$ = mean of adjacent intact lines
- $L$ = line length

**Type Classification**:

| Type | diffVal Range | Correction Strategy | 우선순위 |
|------|---------------|-------------------|---------|
| **Type 1** | diffVal > T2 | Direct interpolation from adjacent intact lines | Essential (1st) |
| **Type 3** | T1 < diffVal ≤ T2 | Edge-aware + quadratic curve fitting | Essential (2nd) |
| **Type 5** | diffVal ≤ T1 | Light smoothing or no change (mode-dependent) | Optional (3rd) |

**Threshold 값 (Normal 모드 예시)**:
- T1 = 0.25 (25% intensity deviation)
- T2 = 0.75 (75% intensity deviation)

### 4. 그리드/모아레 아티팩트 (Grid/Moiré Artifacts)

**정의**: Periodic high-frequency pattern from anti-scatter grid or aliasing effects

**Moiré Severity Index (MSI) 정의**:

$$\text{MSI} = \frac{E_{\text{grid}}}{E_{\text{total}}} \times 100\%$$

where:
- $E_{\text{grid}}$ = energy in grid-related frequency bands (DWT subbands or DCT coefficients)
- $E_{\text{total}}$ = total image energy

**심각도 분류**:

| MSI Range | 심각도 | 가시도 | 조치 |
|-----------|-------|-------|------|
| MSI < 0.1 | Low | Minimal artifact | Skip suppression |
| 0.1 ≤ MSI < 0.3 | Medium | Visible but tolerable | Standard DWT filter |
| 0.3 ≤ MSI < 0.7 | High | Prominent artifact | DCT dynamic segmentation |
| MSI ≥ 0.7 | Critical | Severe, diagnostic impact | DWT + GRD (experimental) |

---

## 검출 알고리즘 수학 명세

### Step 0: Static BPM Generation (Calibration-time)

#### 0.1 Dark Frame Analysis (HotPixelMask)

**입력**: N = 200 dark frames (X-ray OFF), 온도별 3단계

**연산**:

$$\mu_{\text{dark}}(i,j) = \frac{1}{N} \sum_{k=1}^{N} I_{\text{dark}, k}(i,j)$$

$$\sigma_{\text{dark}}(i,j) = \sqrt{\frac{1}{N} \sum_{k=1}^{N} (I_{\text{dark}, k}(i,j) - \mu_{\text{dark}}(i,j))^2}$$

**Hot pixel detection**:

$$\text{HotPixel}(i,j) = 1 \iff \frac{|I_{\text{dark}}(i,j) - \mu_{\text{dark}}|}{\sigma_{\text{dark}}} > \lambda$$

with $\lambda = 8.0$ (factory recommendation, reference: CN104463831A)

#### 0.2 Flat-Field Analysis (ColdPixelMask)

**입력**: N = 200 flat-field frames (RQA-5: 70 kVp, 21 mm Al), 온도별 3단계

**연산**:

$$I_{\text{corrected}}(i,j) = I_{\text{flat}}(i,j) - \mu_{\text{dark}}(i,j)$$

$$G(i,j) = \frac{I_{\text{corrected}}(i,j)}{\overline{I}_{\text{corrected}}}$$

where $\overline{I}_{\text{corrected}}$ = spatial mean

**Cold pixel detection**:

$$\text{ColdPixel}(i,j) = 1 \iff G(i,j) < G_{\text{mean}} - 4\sigma_G$$

#### 0.3 Flickering Pixel Analysis (FlickeringPixelMask)

**입력**: N = 200 frames, continuous acquisition at 1 fps

**연산**:

$$\text{CV}(i,j) = \frac{\sigma(i,j)}{\mu(i,j)} \times 100\%$$

**Flickering detection**:

$$\text{FlickeringPixel}(i,j) = 1 \iff \text{CV}(i,j) > 5\%$$

#### 0.4 Static BPM Consolidation

$$\text{BPM}_{\text{static}}(i,j) = \begin{cases}
1 & \text{if HotPixelMask OR ColdPixelMask OR FlickeringPixelMask} \\
0 & \text{otherwise}
\end{cases}$$

**Quality acceptance criteria**:
- Hot pixel rate < 0.1% of total pixels
- Cold pixel rate < 0.1% of total pixels
- Line defect count < 5 per image

### Step 1: Dynamic Defect Detection (Per-frame)

#### 1.1 Residual Map Computation

$$\text{Residual}(i,j) = I(i,j) - \text{Median}_{5×5}(I(i,j))$$

(Local 5×5 median subtraction to remove smooth background)

#### 1.2 k·σ Local Thresholding

$$\text{DynamicDefect}(i,j) = 1 \iff |\text{Residual}(i,j)| > k \cdot \sigma_{\text{local}}$$

where:
- $\sigma_{\text{local}} = $ standard deviation in 5×5 neighborhood
- $k = 4.0$ (Normal mode, tuned empirically)

#### 1.3 Defect Candidate Map

$$\text{DefectMap} = \text{BPM}_{\text{static}} \cup \text{DynamicDefectMap}$$

(Union of static and dynamic defect detections)

### Step 2: Morphology Classification

**Connected Component Labeling** (8-connectivity):

1. Label all defect pixels via flood-fill
2. Compute bounding box for each component
3. Classify based on bounding box size:

| Bounding Box | Classification |
|------------|----------------|
| 1×1 (single pixel) | Isolated |
| ≤ 3×3 | 3×3 Cluster |
| ≤ 5×5 (not ≤ 3×3) | 5×5 Cluster |
| Elongated (width 1-5, length >> width) | Line Defect |

### Step 3: Grid/Moiré Detection

#### 3.1 DWT-based MSI (Baseline)

**Multi-level 2D DWT decomposition**:

$$I[l] = \{LL[l], LH[l], HL[l], HH[l]\}$$

where $l = 1, 2, 3$ (3 levels recommended for 3072×3072)

**Energy analysis**:

$$E_{\text{grid}}[l] = \text{max}(|LH[l]|^2 + |HL[l]|^2 + |HH[l]|^2)$$

$$E_{\text{total}} = \sum_{l} (|LL[l]|^2 + |LH[l]|^2 + |HL[l]|^2 + |HH[l]|^2)$$

$$\text{MSI} = \frac{E_{\text{grid}}}{E_{\text{total}}} \times 100\%$$

#### 3.2 DCT-based Dynamic Segmentation (Advanced)

Image divided into 64×64 or 128×128 blocks. Per-block 2D DCT:

$$F[u,v] = \sum_{x=0}^{63} \sum_{y=0}^{63} I[x,y] \cos\left(\frac{\pi(2x+1)u}{128}\right) \cos\left(\frac{\pi(2y+1)v}{128}\right)$$

Grid-related frequencies identified and suppressed selectively.

---

## 보정 알고리즘 수학 명세

### Step 4: Cluster Correction (3×3, 5×5)

#### 4.1 3×3 Cluster Correction (ANN)

**절차**:

1. Extract 7×7 neighborhood around 3×3 defect block center
2. Exclude center 3×3, form 40-dim input vector
3. Forward pass through trained ANN: $\vec{y} = W\vec{x} + \vec{b}$
4. Replace all 9 pixels of defect block with clipped output: $\hat{p}_i = \text{clip}(y_i, 0, 2^{14})$

**ANN weights**: Pre-trained on synthetic defect patches from flat-field images. Stored in calibration profile.

#### 4.2 5×5 Cluster Correction (ANN + TMC)

**Basic ANN path**:

1. Extract 9×9 neighborhood around 5×5 defect block
2. Exclude center 5×5, form 56-dim input vector
3. Forward pass: Hidden layer (64 units, ReLU) → Output (25-dim, Linear)
4. Replace all 25 pixels: $\hat{p}_i = \text{clip}(y_i, 0, 2^{14})$

**Optional TMC (Template Matching Correlation) refinement**:

1. After ANN correction, search 27×27 neighborhood for best-matching intact patch
2. Blend ANN output with matched patch:
   $$\hat{p}_i^{\text{final}} = 0.7 \cdot \hat{p}_i^{\text{ANN}} + 0.3 \cdot p_i^{\text{matched}}$$

### Step 5: Line Defect Correction

#### 5.1 Type 1: diffVal > T2 (Direct Interpolation)

**알고리즘**:

1. For each defect line segment:
   - Use two adjacent intact lines (above/below for horizontal, left/right for vertical)
   - Average values from adjacent lines
   - Apply 1D Gaussian smoothing along line direction (σ=1.5 pixels)

$$\hat{p}_{\text{defect}}(i) = \frac{p_{\text{line1}}(i) + p_{\text{line2}}(i)}{2} * G(σ=1.5)$$

**Correctness**: All pixels in defect line replaced. No edge preservation needed (severe damage).

#### 5.2 Type 3: T1 < diffVal ≤ T2 (Edge-Aware + Curve Fitting)

**알고리즘**:

1. **Edge detection** along line direction (Sobel filter):
   $$E(i) = \left| \frac{\partial I}{\partial \text{direction}} \right|$$

2. **Edge-aware interpolation**:
   - For pixels in edge regions: preserve edge gradient direction
   - For pixels in smooth regions: use neighbor averaging

3. **Quadratic curve fitting** through intact adjacent lines:
   $$\hat{p}_{\text{defect}}(i) = a \cdot i^2 + b \cdot i + c$$
   
   (Fit parameters $a, b, c$ from adjacent intact lines)

4. **Blending**:
   $$\hat{p}_{\text{final}}(i) = 0.6 \cdot \hat{p}_{\text{interp}}(i) + 0.4 \cdot \hat{p}_{\text{fit}}(i)$$

**Correctness**: Balances neighbor-based and context-based corrections.

#### 5.3 Type 5: diffVal ≤ T1 (Conditional Smoothing)

**Mode-dependent policy**:

| Mode | Action |
|------|--------|
| **Min** | Apply light Gaussian smoothing (σ=0.8) to minimize residual artifact |
| **Normal** | Minimal smoothing or no change (defect subtle) |
| **Max** | No change (preserve panel longevity) |

### Step 6: Grid/Moiré Suppression

#### 6.1 DWT-based Bandstop Filter (Baseline)

**알고리즘**:

1. Apply 3-level 2D DWT to defect-corrected image
2. Identify grid-dominated subbands (e.g., horizontal grid → LH subband)
3. Design adaptive Gaussian bandstop filter:
   $$H_{\text{stop}}(u,v) = 1 - \exp\left(-\frac{(u-u_0)^2 + (v-v_0)^2}{2\sigma_{\text{filter}}^2}\right)$$
   
   where $(u_0, v_0)$ = grid frequency center

4. Apply filter: $F_{\text{filtered}} = F \cdot H_{\text{stop}}$
5. Inverse DWT to spatial domain

#### 6.2 DCT-based Dynamic Segmentation (Advanced)

**알고리즘**:

1. Divide image into 64×64 or 128×128 blocks
2. Compute 2D DCT per block
3. Identify and suppress grid-related frequency components
4. Inverse DCT, blend blocks (avoid edge artifacts)

#### 6.3 GRD (Grid Regression/Demodulation) - Experimental

For severe moiré (MSI ≥ 0.7), fit spatial grid model:

$$G(x,y) = A + B \cos(f_x \cdot x) + C \sin(f_x \cdot x) + D \cos(f_y \cdot y) + E \sin(f_y \cdot y)$$

Subtract estimated grid component from image.

---

## Min/Normal/Max 프로필 파라미터

모든 파라미터는 세 가지 프로필 모드로 구성 가능합니다:

| 파라미터 | Min (Patient-Centric) | Normal (Balanced) | Max (Panel Lifespan) |
|---------|----------------------|------------------|----------------------|
| **k (σ threshold)** | 3.0 (detect more) | 4.0 (balanced) | 5.0 (detect fewer) |
| **Hot pixel σ threshold** | 6.0 | 8.0 | 10.0 |
| **T1 (Type 5 boundary)** | 0.15 | 0.25 | 0.40 |
| **T2 (Type 1 boundary)** | 0.50 | 0.75 | 1.00 |
| **MSI threshold (suppress)** | 0.10 | 0.20 | 0.40 |
| **DWT filter strength** | Strong (2×) | Standard (1×) | Weak (0.5×) |
| **3×3 Cluster: ANN enabled** | Yes | Yes | Yes |
| **5×5 Cluster: ANN enabled** | Yes | Yes | Yes |
| **5×5 Cluster: TMC enabled** | Yes | Optional | No |
| **Type 1 Line correction** | Always | Always | Yes (diffVal threshold) |
| **Type 3 Line correction** | Always | Always | Selective |
| **Type 5 Line correction** | Always | Minimal | No |

---

## 성능 목표

### 검출 성능

| 메트릭 | 목표 | 근거 |
|-------|------|------|
| **3×3 Cluster Detection Rate** | > 95% | Clinical false negative unacceptable (artifact visible) |
| **5×5 Cluster Detection Rate** | > 90% | Larger defects easier to detect |
| **Line Defect Type 1 Detection** | > 98% | Severe defects must be caught |
| **False Positive Rate** | < 5% | Overcorrection creates artifacts |
| **Grid MSI Estimation Error** | < 10% (absolute) | MSI < 0.1 vs. < 0.2 affects filter strength |

### 보정 품질 (NMSE 메트릭)

NMSE (Normalized Mean Squared Error) 정의:

$$\text{NMSE} = \frac{\sum (p_{\text{corrected}} - p_{\text{ground-truth}})^2}{\sum p_{\text{ground-truth}}^2}$$

| 보정 타입 | NMSE 목표 | 근거 |
|---------|-----------|------|
| **3×3 Cluster (ANN)** | < 0.14 | Jeon 2021 baseline: ANN NMSE ~0.14 vs. bilinear ~0.35 |
| **5×5 Cluster (ANN)** | < 0.20 | Larger region, more challenging |
| **5×5 Cluster (ANN+TMC)** | < 0.10 | TMC refinement improves to 0.10 |
| **Type 1 Line (interpolation)** | < 0.15 | Direct interpolation acceptable |
| **Type 3 Line (edge-aware)** | < 0.25 | Edge preservation may increase error |
| **Grid Suppression (MSI reduction)** | MSI_out < 0.1 | From 0.3-0.7 input → < 0.1 output |

### 처리 성능

| 파라미터 | 목표 | 환경 |
|---------|------|------|
| **Total Processing Time** | < 95 ms / frame | Intel Core i7, 3072×3072 |
| **Static BPM Generation** | < 500 ms | One-time calibration setup |
| **Memory Peak Usage** | < 100 MB | Per-frame working buffers |

---

## 참고문헌

### Bad Pixel / Cluster / Line Defect

1. **Jeon et al. (2021)** - PMC7930811
   - 제목: "Using Deep Learning for Pixel-Defect Corrections in Flat-Panel Radiography Imaging"
   - 내용: FPD에서 3×3, 5×5 픽셀 결함을 ANN, CNN, concat-CNN, GAN으로 보정하는 방법 및 성능 비교
   - 기여: 3×3 및 5×5 ANN 아키텍처 및 성능 기준값 제공

2. **Lee et al.** - "Pixel-defect corrections for radiography detectors based on deep learning" (SPIE)
   - 내용: Radiography detector의 pixel-defect correction을 위한 딥러닝 접근 개요
   - 기여: 5×5 클러스터 보정 concat-CNN 방법론

3. **FixPix (2023)** - arXiv:2310.11637
   - 제목: "FixPix: Fixing Bad Pixels using Deep Learning"
   - 내용: 일반 이미징 센서에서 segmentation + lightweight MLP/ViT AE로 bad pixel을 검출/보정
   - 기여: 고급 MLP 기반 재구성 (1425 parameters, FPGA-friendly)

4. **CN104463831A** - Method for repairing X-ray FPD image bad line
   - 내용: X-ray FPD 이미지의 bad line을 diffVal, 두 개의 threshold (T1/T2), interpolation 및 edge-aware smoothing으로 소프트웨어로 보정
   - 기여: Type 1, 3, 5 라인 분류 및 보정 임계값 정의

5. **Tang et al. (2012)** - "A new stationary gridline artifact suppression method based on the 2D discrete wavelet transform"
   - 내용: DWT 기반으로 gridline artifact를 검출·감쇠하는 방법
   - 기여: DWT 기반 그리드 아티팩트 억제 알고리즘

### Grid / Moiré / Aliasing

6. **Wang et al. (2013)** - "A Study of Grid Artifacts Formation and Elimination in Computed Radiographic Images"
   - 내용: CR/DR에서 grid artifact 발생 메커니즘과 제거 방법, aliasing 한계 분석
   - 기여: 모아레 메커니즘 및 이론적 한계

7. **Park et al.** - "A Dynamically Segmented DCT Technique for Grid Artifact Suppression in X-ray Images"
   - 내용: X-ray 영상에서 동적 분할 + DCT 기반 grid artifact 제거 기법
   - 기여: DCT 동적 분할 방법론

8. **GRD Method** - "A novel grid regression demodulation method for radiographic grid artifact correction"
   - 내용: Spatial domain에서 grid 성분을 회귀 모델로 추정하고 제거하는 GRD 방식
   - 기여: 고급 그리드 억제 알고리즘

### 의료기기 표준 및 규격

9. **IEC 62304:2006 (amended 2015)** - Medical device software lifecycle processes
   - 관련 조항: §5 (Software Requirements), §7 (Software Risk Management)

10. **ISO 14971:2019** - Risk Management for medical devices
    - 관련 조항: 위험 식별, 심각도/확률 평가, 위험 통제

11. **IEC 62220-1-1:2015** - Medical Electrical Equipment - Determination of the Detective Quantum Efficiency
    - 내용: FPD 영상품질 평가 표준
    - 기여: Defect correction 결과 영상품질 검증 기준

---

**Document Version**: 1.0  
**Last Updated**: 2026-04-14  
**Classification**: Technical Specification (의료기기 설계 기술)  
**Next**: SRS-DEFECT-001 (Software Requirements Specification)
