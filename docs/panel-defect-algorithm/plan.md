1. Objectives and Scope
This R&D plan defines a CPU-only, DLL-based image-processing library for X-ray flat-panel radiography that:

Implements a Defect Detection Algorithm:

Detects bad pixels and cluster defects of size up to at least 
5
×
5
5×5.

Detects line defects with continuous width from 1 to 5 pixels (narrow direction).
​

Detects and quantifies grid / moiré artifacts originating from anti-scatter grids.

Produces determination decisions under three profiles: Min, Normal, Max.

Implements a Defect Correction Algorithm:

Corrects all adjacent defective pixels in both 
3
×
3
3×3 and 
5
×
5
5×5 blocks using CPU-feasible methods.

Corrects continuous line defects of width 1–5 pixels with correction types 1, 3, and 5, where the priority and completeness follow 1 > 3 > 5, and types 1 and 3 are mandatory.
​

Suppresses grid-line/moiré artifacts in a way that is compatible with the bad pixel corrections.

Uses only CPU-based algorithms and delivers a DLL (or shared library) API with no GUI in this phase.

A subsequent R&D plan will extend these methods to CPU + GPU; this future direction is outlined but not implemented here.

2. Technical Foundations
2.1 Bad Pixel / Cluster Correction
Lee et al. demonstrated ANN, CNN, concat-CNN, GAN methods for pixel-defect correction in flat-panel radiography for cluster sizes 
3
×
3
3×3 and 
5
×
5
5×5.

For 3×3 clusters:

A single-layer ANN (no hidden layer) using surrounding pixels achieved significantly lower MSE than traditional template-matching correction.
​

For 5×5 clusters:

Both ANN and concat-CNN performed well; concat-CNN was best but more complex, while ANN still outperformed classical methods with much lower computational overhead.
​

FixPix introduced lightweight MLP-based reconstruction and segmentation for bad pixels in general imaging; their results confirm that small MLPs can effectively reconstruct corrupted regions if the overall defect rate is modest.

2.2 Line Defects
CN104463831A describes a software-only method for X-ray FPD bad line repair:

An anomaly degree diffVal is computed from differences between defective line pixels and neighboring intact lines.
​

Two thresholds (T1, T2) determine which correction strategy to apply:

Above T2: ignore line values, interpolate from neighbors.

Between T1 and T2: edge detection + curve-fitting-based interpolation.
​

2.3 Grid / Moiré Artifacts
Stationary and crisscrossed grid artifacts can be suppressed using:

DWT-based methods, where gridline energy dominates certain wavelet sub-bands and is attenuated via band-stop filters.

DCT-based dynamic segmentation, where blocks or segments of the image are transformed and grid-related frequencies are selectively suppressed.

Grid regression/demodulation (GRD) methods that model grid components in the spatial domain and regress them out.

Studies show that when grid frequencies alias into the anatomical band (due to sampling mismatch), perfect recovery is impossible; practical algorithms aim for strong attenuation with minimal diagnostic degradation.

These facts anchor feasibility and guide algorithm choices.

3. Defect Detection Algorithm
3.1 Goals
The detection algorithm must:

Identify:

Single bad pixels.

Cluster defects (up to at least 
5
×
5
5×5).

Line defects with continuous width 1–5 pixels.

Grid / moiré artifacts and their severity.

Support Min / Normal / Max determination profiles that trade off patient safety vs panel longevity.

3.2 Detection Pipeline
Step 0: Calibration-based static defect mapping

Inputs: multiple dark-field (no X-ray) and flat-field (uniform X-ray) images.

Compute:

Offset map: average dark image.
​

Gain map: average flat image minus offset, normalized.
​

Detect static pixel defects:

Pixels whose offset or gain lies outside correction ranges become static bad pixels.
​

Store:

Static defect map (single pixels, clusters, line candidates).

Step 1: Per-image residual analysis

For each clinical image:

Subtract local median or mean (e.g., 5×5 median filter) to obtain a residual map.
​

Pixels with residual magnitude above k·σ (local) form dynamic defect candidates.

Step 2: Connected component and shape analysis

Combine static and dynamic candidates into a binary map.

Label connected components:

Single pixels or very small components → isolated defects.

Components whose bounding box fits within 
3
×
3
3×3 → 3×3 clusters.

Components whose bounding box fits within 
5
×
5
5×5 but not 3×3 → 5×5 clusters.

Components elongated along rows or columns → line defect candidates.

For line candidates:

For each row/column:

Identify continuous runs of defective pixels.

Compute width (perpendicular to line direction) and length.

Keep only segments whose width is between 1 and 5 pixels.

Step 3: Grid / moiré detection

Preprocessing with defect mask:

Use initial bad pixel corrections (or smoothing) to avoid spikes.

Replace defect pixels with local averages before grid analysis.

Frequency / multi-scale analysis:

Option A (baseline): 2D DWT:

Perform multi-level DWT on the image.

Identify sub-bands where gridline energy is strong relative to background (e.g., vertical grid → horizontal high-frequency bands).

Option B (advanced): block/segment DCT:

Split into blocks or dynamic segments, apply 2D DCT.

Search for strong coefficients at expected grid frequencies.

Moiré Severity Index (MSI):

Compute the ratio of grid-related energy to total image energy from DWT/DCT coefficients.

Use MSI to determine severity class (e.g., low, medium, high).

3.3 Min / Normal / Max Profiles (Detection)
Each mode is a parameter set for thresholds and policies:

Min (patient-focused):

Lower thresholds for residual-based pixel detection and cluster size → more defects flagged.

Lower thresholds for diffVal when classifying line defects.
​

Lower MSI threshold for grid detection; conservative about letting grid artifacts remain.

Normal:

Thresholds tuned empirically using phantom and retrospective data to balance sensitivity/specificity.

Max (panel-lifespan-focused):

Higher thresholds: only clearly harmful defects classified.

Static map updated only if defects persist across multiple images.

Higher MSI threshold before grid suppression is triggered.

The DLL exposes DetectionMode to select these configurations.

4. Defect Correction Algorithm
4.1 Goals
Correct all adjacent bad pixels within detected 
3
×
3
3×3 and 
5
×
5
5×5 cluster defects.

Correct line defects of width 1–5 pixels with:

Types 1, 3, 5, priority and completeness 1 > 3 > 5.

Types 1 and 3 are mandatory.

Suppress moiré/grid artifacts without disrupting bad pixel corrections.

4.2 Cluster Correction (3×3, 5×5)
4.2.1 3×3 clusters (Type 1 – highest priority)
Inspired by Lee et al.:

For each 3×3 defect region:

Extract a 7×7 neighborhood centered on the defect.

Form an input vector by taking all 7×7 pixels excluding the 3×3 defect (40 inputs).
​

Use a pre-trained single-layer ANN:

Input: 40-dimensional vector.

Output: 9 values for the 3×3 defect block.

Architecture: simple y = W x + b, optionally with a very small hidden layer.

Replace all 9 pixels in the 3×3 block with the ANN outputs, clipped to valid range.

This approach is CPU-feasible and experimentally shown to outperform classical template matching for 3×3 clusters.
​

4.2.2 5×5 clusters (Type 3 – mandatory, lower priority than 3×3)
Based on Lee et al.:
​

For each 5×5 defect region:

Extract a 9×9 neighborhood centered on the defect.

Form an input vector: 9×9 pixels excluding the central 5×5 block (56 inputs).

Use an ANN:

Input: 56-dimensional vector.

Output: 25 values for the 5×5 defect block.

Architecture: small (e.g., one hidden layer with 64 units) to stay fast on CPU.

Replace all 25 pixels in the 5×5 block with ANN outputs.

Optionally, for offline/high-quality modes:

Apply Template Matching Correlation (TMC):

Roughly fill defect with ANN.

Search a larger neighborhood (e.g., 27×27) for best-matching patches and refine the 5×5 values using that patch center.
​

This satisfies the requirement that all adjacent pixels in 5×5 defects are corrected.

4.3 Line Defect Correction (Width 1–5)
Based on CN104463831A, generalized for width 1–5.
​

Step 1: Anomaly degree per line (diffVal)

For each detected line segment (row or column, width 1–5):

Compare each defective pixel value against average of neighboring intact lines.

Normalize by maximum gray level and number of pixels to get diffVal.
​

Step 2: Correction type decision

Using thresholds T1 and T2 (mode-dependent):

Type 1 (severe, mandatory): diffVal > T2

Treat original line pixels as invalid.

Interpolate from immediate intact neighbor lines:

For horizontal lines: use above/below rows.

For vertical lines: use left/right columns.

Apply 1D smoothing along the line direction (e.g., small Gaussian).

Type 3 (moderate, mandatory): T1 < diffVal ≤ T2

Edge-aware correction:

Apply a Sobel filter to detect edges parallel to the line.
​

Perform quadratic curve fitting on the edge positions along the line.
​

Interpolate line pixels combining:

Neighbor-based interpolation.

Values derived from the fitted edge curve.

Type 5 (mild, optional): diffVal ≤ T1

Depending on Min/Normal/Max:

Min: may still use light interpolation to avoid subtle artifacts.

Normal: apply minimal smoothing or leave unchanged.

Max: typically leave unchanged unless part of a larger issue.

For width >1 (up to 5), process each column/row in the defective band in a coordinated way, preserving edge consistency.

4.4 Grid / Moiré Suppression
After defect correction for pixels/lines:

Step 1: Pre-processed image for grid analysis

For grid suppression, start from the image where bad pixels/lines are corrected.

Optionally smooth corrected regions slightly or replace them with local averages before transform to avoid residual spikes.

Step 2: Baseline method – DWT-based suppression

Use multi-level 2D DWT as in Tang et al.:
​

Decompose into sub-bands.

Identify sub-bands where grid energy dominates.

Apply adaptive Gaussian band-stop filters in those sub-bands.

Reconstruct the image with inverse DWT.

Step 3: Advanced method – DCT-based dynamic segmentation

Split the image into blocks or dynamic segments.

Apply 2D DCT per segment.

Attenuate coefficients near the estimated grid frequency (and harmonics).

Step 4: Optional GRD/demodulation (experimental)

For high MSI (severe moiré), optionally apply grid regression/demodulation to model and remove residual grid components in spatial domain.

4.5 Min / Normal / Max Profiles (Correction)
Min:

Always correct 3×3 and 5×5 clusters via ANN.

Always apply Type 1 and 3 line corrections; apply Type 5 where possible.

Use stronger DWT/DCT filters (larger attenuation) for grid suppression.

Normal:

Same mandatory corrections (3×3, 5×5, Type 1/3 lines).

Type 5 corrections and filter strengths tuned for balance.

Max:

Mandatory pieces unchanged for clearly severe defects (3×3, 5×5, Type 1 lines).

Type 3 and Type 5 corrections applied only when diffVal/MSI clearly indicate risk.

Weaker grid suppression to minimize impact and processing frequency.

5. Software Architecture (DLL) and Implementation Plan
5.1 API Outline
The DLL exposes C/C++ functions (no GUI):

Initialization:

InitLibrary(const Config* cfg);

LoadCalibration(const OffsetMap* offset, const GainMap* gain);

SetMode(DetectionMode mode); // MIN, NORMAL, MAX

Core processing:

DetectDefects(const Image* in, DefectMask* mask, DefectStats* stats);

CorrectDefectsAndSuppressGrid(const Image* in, const DefectMask* mask, Image* out);

Or combined:

ProcessImage(const Image* in, Image* out); // internal pipeline

5.2 Implementation Considerations
Language: C/C++.

Optimization:

Use SIMD (e.g., AVX) for ANN inference and filtering where beneficial.

Multi-thread the pipeline across tiles or blocks.

Dependencies:

Lightweight math routines or BLAS for ANN matrix operations.

FFT/DWT/DCT library (e.g., FFTW or custom) for grid suppression.

6. Future CPU+GPU Phase (Deep Research Track)
A later R&D plan will explore:

GPU-accelerated segmentation for bad pixels and grid artifacts using U-Net–like models.

Concat-CNN and GAN-based cluster correction for 3×3 and 5×5, possibly improving MSE beyond ANN.
​

Implicit neural representation and transformer-based reconstruction for joint ring/grid and defect correction.

These are explicitly deferred to a future phase; the present plan ensures all algorithms remain CPU-feasible and grounded in published methods.

7. Reference

Bad pixel / cluster / line defect 관련
Using deep learning for pixel-defect corrections in flat-panel radiography imaging

내용: DR용 FPD에서 3×3, 5×5 pixel defect를 ANN, CNN, concat-CNN, GAN으로 보정하는 방법 및 성능 비교.

Pixel-defect corrections for radiography detectors based on deep learning (SPIE)

내용: radiography detector의 pixel-defect correction을 위한 딥러닝 접근 개요.
​

FixPix: Fixing Bad Pixels using Deep Learning

내용: 일반 이미지 센서에서 segmentation + lightweight MLP/ViT AE로 bad pixel을 검출/보정하는 방법.

방사선 디텍터에서 CNN 기법을 사용한 픽셀 결함 보정 (국내 논문)

내용: X-ray 디텍터에서 CNN 기반 화소 결함 보정 알고리즘 제안.
​

CN104463831A – Method for repairing X-ray flat panel detector image bad line

내용: X-ray FPD 이미지의 bad line을 diffVal, 두 개의 threshold(T1/T2), interpolation 및 edge-aware smoothing으로 소프트웨어로 보정하는 특허.
​

How to repair the broken flat panel detector

내용: FPD 필드 고장, defect 대응(교체·수리)에 대한 실무적 설명.
​

Grid / Moiré / aliasing 관련
A new stationary gridline artifact suppression method based on the 2D discrete wavelet transform

내용: DWT 기반으로 gridline artifact를 검출·감쇠하는 방법.
​

A Study of Grid Artifacts Formation and Elimination in Computed Radiographic Images

내용: CR/DR에서 grid artifact 발생 메커니즘과 제거 방법, aliasing 한계 분석.
​

A Dynamically Segmented DCT Technique for Grid Artifact Suppression in X-ray Images (및 한국어 논문 버전)

내용: X-ray 영상에서 동적 분할 + DCT 기반 grid artifact 제거 기법.

A novel grid regression demodulation method for radiographic grid artifact correction

내용: spatial domain에서 grid 성분을 회귀 모델로 추정하고 제거하는 GRD 방식.
​

A software-based method for eliminating grid artifacts of a high resolution image detector

내용: 고해상도 이미지 디텍터의 grid artifact를 소프트웨어로 제거하는 방법.
​

Patch Based Grid Artifact Suppressing in Digital Mammography

내용: 패치 기반 grid artifact suppression.

X-ray 영상에서 그리드 아티팩트 개선을 위한 동적 분할 기반 DCT 기법

내용: 국내 DCT 기반 grid artifact 개선 방식 상세.

기타 참고 (품질/딥러닝 전반)
Deep Learning Neural Network Performance on NDT Digital X-ray Radiography Images

내용: NDT X-ray 이미지에서 딥러닝 성능에 대한 실험 연구 (이미지 품질 파라미터 영향).
​

Deep Learning Image Reconstruction for CT: Technical Principles and Clinical Prospects

내용: CT에서 딥러닝 기반 재구성·artifact 보정의 기술 및 임상적 관점.
​

Quick guide on radiology image pre-processing for deep learning applications

내용: 방사선 영상에서 딥러닝 전처리에 필요한 일반적 artifact 처리 개요.
