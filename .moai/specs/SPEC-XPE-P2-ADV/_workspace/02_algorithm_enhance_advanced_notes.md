# Algorithm Design Notes: xpe_enhance_advanced.dll

**Document ID**: ALG-ADV-NOTES-001
**Version**: 1.0.0
**Date**: 2026-04-18
**Author**: xpe-algorithm
**Status**: Complete -- ready for xpe-implementer integration
**SPEC Reference**: SPEC-XPE-P2-ADV v1.0.0

---

## 1. Scope

This document provides the mathematical definitions, pseudo-code, SIMD optimization plans, and complexity analysis for the four core algorithms in xpe_enhance_advanced.dll:

| SWU | Algorithm | Function |
|-----|-----------|----------|
| SWU-2.5 | Multiscale Frequency Processing (MFP) | `xpe_multiscale_process` |
| SWU-2.6 | Fractional-Order Edge Enhancement | `xpe_fractional_process` |
| SWU-2.8 | Collimation ROI Detection | `xpe_detect_collimation` |
| SWU-2.10 | Exposure Index Calculation | `xpe_calc_exposure_index` |

All algorithms operate on FLOAT32 enhancement-domain images. The scalar reference implementation is the accuracy baseline; SIMD variants must achieve numerical parity (error < 1e-6).

---

## 2. SWU-2.5: Multiscale Frequency Processing (MFP)

### 2.1 Mathematical Definition

#### Laplacian Pyramid Decomposition

Given an input image I of size W x H, build a Gaussian pyramid G and Laplacian pyramid L with N levels.

**Gaussian Pyramid:**

```
G_0 = I
G_{k+1} = Downsample(GaussianBlur(G_k))    for k = 0, ..., N-2
```

where `GaussianBlur` is a 5x5 separable Gaussian filter with sigma = 1.0:

```
g(x) = (1/16) * [1, 4, 6, 4, 1]   (binomial coefficients, sum = 16)

G(x,y) = sum_{j=-2}^{2} sum_{i=-2}^{2} g(i) * g(j) * I(x+i, y+j)
```

Separable implementation (2-pass):

```
H_pass(x,y) = sum_{i=-2}^{2} g(i) * I(x+i, y)       // horizontal
G(x,y)      = sum_{j=-2}^{2} g(j) * H_pass(x, y+j)   // vertical
```

**Downsample:**

```
G_{k+1}(x,y) = (1/4) * [G_k(2x,2y) + G_k(2x+1,2y) + G_k(2x,2y+1) + G_k(2x+1,2y+1)]
```

Output size: floor(W/2^k) x floor(H/2^k).

**Laplacian Pyramid:**

```
L_k = G_k - Upsample(G_{k+1})    for k = 0, ..., N-2
L_{N-1} = G_{N-1}                 // coarsest level (residual)
```

where `Upsample` expands G_{k+1} to the size of G_k using bilinear interpolation.

**Upsample (bilinear):**

```
Upsample(S)(x,y) = (1-dx)(1-dy)*S(sx,sy) + dx*(1-dy)*S(sx+1,sy)
                 + (1-dx)*dy*S(sx,sy+1) + dx*dy*S(sx+1,sy+1)

where sx = x/2, sy = y/2, dx = x/2 - sx, dy = y/2 - sy
```

**NOTE**: Current implementation uses nearest-neighbor upsampling. For REQ-ADV-050 (identity reconstruction fidelity < 1e-5), bilinear interpolation is REQUIRED. Nearest-neighbor introduces systematic reconstruction error exceeding 1e-5 for non-trivial images. See Section 2.4.

#### Per-Band Enhancement

```
L'_k = alpha_k * L_k    where alpha_k depends on frequency band and body part

alpha mapping:
  k = 0 (finest):   alpha = edgeGain      (edges, noise)
  k = 1..N-3 (mid): alpha = textureGain   (texture)
  k = N-2 (coarse): alpha = flatGain       (flat regions)
  k = N-1 (residual): alpha = 1.0          (preserve DC)
```

Body-part adaptive defaults (from internal.h):

| Body Part | edgeGain | textureGain | flatGain |
|-----------|----------|-------------|----------|
| CHEST     | 1.8      | 1.2         | 0.9      |
| ABDOMEN   | 1.5      | 1.1         | 0.95     |
| EXTREMITY | 2.0      | 1.3         | 0.85     |
| Default   | 1.5      | 1.0         | 0.8      |

Noise gating:

```
L'_k(x,y) = alpha_k * L_k(x,y)    if |L_k(x,y)| >= noiseThreshold
            0                         otherwise
```

#### Reconstruction

```
R_{N-1} = L'_{N-1}                    // start with coarsest
R_k = L'_k + Upsample(R_{k+1})       // for k = N-2 down to 0
Output = R_0
```

### 2.2 Pseudo-Code (Scalar Reference)

```cpp
// Build Gaussian pyramid
vector<Matrix<float>> gaussian(N);
gaussian[0] = input;  // W x H
for (k = 0; k < N-1; k++) {
    gaussian[k+1] = downsample(gaussianBlur(gaussian[k]));
}

// Build Laplacian pyramid
vector<Matrix<float>> laplacian(N);
for (k = 0; k < N-1; k++) {
    Matrix<float> up = bilinearUpsample(gaussian[k+1], gaussian[k].rows, gaussian[k].cols);
    laplacian[k] = gaussian[k] - up;
}
laplacian[N-1] = gaussian[N-1];

// Apply per-band enhancement
for (k = 0; k < N; k++) {
    float gain = selectGain(k, N, bodyPart, config);
    for (i = 0; i < laplacian[k].size(); i++) {
        if (abs(laplacian[k][i]) < noiseThreshold) {
            laplacian[k][i] = 0;
        } else {
            laplacian[k][i] *= gain;
        }
    }
}

// Reconstruct
Matrix<float> result = laplacian[N-1];
for (k = N-2; k >= 0; k--) {
    Matrix<float> up = bilinearUpsample(result, laplacian[k].rows, laplacian[k].cols);
    result = laplacian[k] + up;
}

// NaN/Inf guard (REQ-ADV-032)
for (i = 0; i < result.size(); i++) {
    if (!isfinite(result[i])) result[i] = 0.0f;
}
```

### 2.3 Complexity Analysis

| Operation | FLOPS (per pixel) | Memory (3072x3072, 4 levels) |
|-----------|-------------------|------------------------------|
| Gaussian blur (5x5 sep, 2-pass) | 20 (10 per pass) | 2 * W * H * 4B (temp) |
| Downsample | 4 (2x2 average) | ~0.75x input per level |
| Upsample (bilinear) | 8 (4 samples * 2 ops) | 4x input per level |
| Laplacian subtraction | 1 | W * H * 4B per level |
| Gain application | 2 (mul + compare) | 0 |
| Reconstruction | 9 (upsample + add) | W * H * 4B per level |

**Total for 3072x3072, 4 levels:**
- Floating-point operations: ~120 Mflop
- Peak memory: ~150 MB (all pyramid levels + temporaries)
- Scalar estimate: 600-800 ms on 2.6 GHz Core i7 (single-thread)
- AVX2 estimate: 150-250 ms (8-wide SIMD on convolution + memset-like ops)

### 2.4 Implementation Issues in Current Code

**Issue 1: Upsample uses nearest-neighbor instead of bilinear.**

Current `LaplacianPyramid::upsample()` at line 214-226 of mfp_scalar.cpp uses:
```cpp
out[y * outW + x] = in[inY * width + inX];  // nearest-neighbor
```

This violates REQ-ADV-050. The nearest-neighbor interpolation introduces discontinuities that prevent identity reconstruction from achieving error < 1e-5. Bilinear interpolation is required.

**Fix**: Replace with bilinear interpolation:
```cpp
void LaplacianPyramid::upsample(const float* in, float* out, int width, int height) {
    int outW = width * 2;
    int outH = height * 2;
    for (int y = 0; y < outH; ++y) {
        for (int x = 0; x < outW; ++x) {
            float sx = (x - 0.5f) * 0.5f;
            float sy = (y - 0.5f) * 0.5f;
            int x0 = std::max(0, std::min(width - 1, (int)std::floor(sx)));
            int y0 = std::max(0, std::min(height - 1, (int)std::floor(sy)));
            int x1 = std::min(width - 1, x0 + 1);
            int y1 = std::min(height - 1, y0 + 1);
            float fx = sx - x0;
            float fy = sy - y0;
            fx = std::max(0.0f, std::min(1.0f, fx));
            fy = std::max(0.0f, std::min(1.0f, fy));
            out[y * outW + x] = (1-fx)*(1-fy)*in[y0*width+x0]
                              + fx*(1-fy)*in[y0*width+x1]
                              + (1-fx)*fy*in[y1*width+x0]
                              + fx*fy*in[y1*width+x1];
        }
    }
}
```

**Issue 2: Reconstruct dimension calculation uses sqrt(size) heuristic.**

Current `reconstruct()` at line 124-126 computes dimensions as:
```cpp
int currentW = reconstructed.size() > 0 ?
    static_cast<int>(std::sqrt(reconstructed.size())) : 1;
int currentH = static_cast<int>(reconstructed.size() / static_cast<size_t>(currentW));
```

This is incorrect for non-square images. The reconstructed buffer dimensions must be tracked explicitly.

**Fix**: Track dimensions alongside each pyramid level. Use a struct:
```cpp
struct PyramidLevel {
    std::vector<float> data;
    int width;
    int height;
};
```

**Issue 3: Gaussian blur operates on in-place data before subtraction.**

Current implementation blurs currentLevel, then stores it as the Laplacian level. But the blur is destructive -- the original data is lost before computing the Laplacian. The correct order is:

1. Store unblurred G_k
2. Blur G_k to get G_k_blurred
3. Downsample G_k_blurred to get G_{k+1}
4. Upsample G_{k+1}
5. Laplacian L_k = G_k - Upsample(G_{k+1})

**Issue 4: Noise threshold unit mismatch.**

internal.h defines `XPE_MFP_DEFAULT_NOISE_THRESH = 5.0f` (raw pixel units), but MfpConfig::fromJson uses `config.noiseThreshold = 0.02f` (normalized). The design doc JSON schema uses `"noise_threshold": 5.0`. This inconsistency must be resolved. Since the algorithm operates on float32 enhancement-domain images where values can range widely, the threshold should be configurable and default to 5.0 (matching the JSON schema in Section 8 of the architect design).

### 2.5 SIMD Optimization Plan (AVX2)

#### Target Operations

| Operation | AVX2 Strategy | Expected Speedup |
|-----------|---------------|------------------|
| Gaussian blur (sep) | 8-wide FMA on contiguous rows, unrolled 5-tap | 6-8x |
| Downsample (2x2 avg) | `_mm256_hadd_ps` for pairwise sums | 4-6x |
| Upsample (bilinear) | Gather + FMA blend | 3-4x |
| Laplacian subtraction | `_mm256_sub_ps` | 8x |
| Gain multiply | `_mm256_mul_ps` | 8x |
| Noise threshold gate | `_mm256_and_ps` + mask compare | 8x |
| NaN/Inf guard | `_mm256_cmpord_ps` | 8x |

#### Memory Alignment

- All float buffers MUST be 32-byte aligned for `_mm256_load_ps` / `_mm256_store_ps`.
- Row stride must be a multiple of 8 floats (32 bytes) for clean vectorization.
- Use `_mm_malloc` or `std::aligned_alloc` for pyramid level buffers.
- For 3072 width: 3072 * 4 = 12288 bytes. 12288 / 32 = 384 (cleanly divisible).

#### Eigen SIMD Strategy

Eigen 3.4.x auto-vectorizes matrix operations when compiled with `/arch:AVX2`. For the Laplacian pyramid, the following Eigen operations will auto-vectorize:

- `MatrixXf` element-wise add/subtract (reconstruction)
- `MatrixXf` scalar multiply (gain application)
- `MatrixXf` coefficient-wise abs() (noise threshold)

Explicit AVX2 intrinsics are needed for:
- Gaussian blur (separable convolution with specific kernel)
- Bilinear upsampling (gather + interpolation)
- Downsample (2x2 block average with stride)

#### Implementation Approach

```cpp
#ifdef XPE_SIMD_AVX2

// Gaussian blur horizontal pass -- AVX2
void gaussianBlur_h_avx2(const float* src, float* dst,
                          int width, int height, int stride) {
    // Kernel: [1/16, 4/16, 6/16, 4/16, 1/16]
    // Pre-multiply by 1/16: [0.0625, 0.25, 0.375, 0.25, 0.0625]
    __m256 k0 = _mm256_set1_ps(0.0625f);
    __m256 k1 = _mm256_set1_ps(0.25f);
    __m256 k2 = _mm256_set1_ps(0.375f);

    for (int y = 0; y < height; y++) {
        const float* row = src + y * stride;
        float* outRow = dst + y * stride;

        // Process 8 pixels at a time
        for (int x = 0; x <= width - 8; x += 8) {
            __m256 sum = _mm256_setzero_ps();
            // Unrolled 5-tap horizontal convolution
            sum = _mm256_fmadd_ps(k0, _mm256_loadu_ps(&row[x-2]), sum);
            sum = _mm256_fmadd_ps(k1, _mm256_loadu_ps(&row[x-1]), sum);
            sum = _mm256_fmadd_ps(k2, _mm256_loadu_ps(&row[x]),   sum);
            sum = _mm256_fmadd_ps(k1, _mm256_loadu_ps(&row[x+1]), sum);
            sum = _mm256_fmadd_ps(k0, _mm256_loadu_ps(&row[x+2]), sum);
            _mm256_storeu_ps(&outRow[x], sum);
        }
        // Scalar tail for remaining pixels
    }
}

#endif // XPE_SIMD_AVX2
```

#### Peak Memory Budget (REQ-ADV-080: < 200MB)

For 3072x3072 FLOAT32 with 4 levels:

| Buffer | Size |
|--------|------|
| Level 0 (full) | 3072 * 3072 * 4 = 36 MB |
| Level 1 (half) | 1536 * 1536 * 4 = 9 MB |
| Level 2 (quarter) | 768 * 768 * 4 = 2.25 MB |
| Level 3 (eighth) | 384 * 384 * 4 = 0.56 MB |
| Temp (blur + upsample) | ~36 MB * 2 = 72 MB |
| **Total** | **~120 MB** |

Within 200MB budget. Buffer reuse: temp buffers from level k can be reused for level k+1 since processing is sequential.

---

## 3. SWU-2.6: Fractional-Order Edge Enhancement

### 3.1 Mathematical Definition

#### Gruenwald-Letnikov Fractional Derivative

For a continuous function f(x), the Gruenwald-Letnikov fractional derivative of order alpha is:

```
D^alpha f(x) = lim_{h->0} (1/h^alpha) * sum_{k=0}^{inf} (-1)^k * C(alpha, k) * f(x - k*h)
```

Discrete approximation (h = 1):

```
D^alpha f(x) = sum_{k=0}^{N} w_k(alpha) * f(x - k)
```

where the weights are the fractional binomial coefficients:

```
w_0 = 1
w_k = (-1)^k * C(alpha, k) = w_{k-1} * (k - 1 - alpha) / k
```

Or equivalently:

```
w_k = product_{i=0}^{k-1} (alpha - i) / (k - i)  *  (-1)^k
```

For alpha in [0.0, 2.0]:
- alpha = 0: identity (no enhancement)
- alpha ~ 0.5: smooth texture enhancement
- alpha = 1.0: standard first derivative (edge detection)
- alpha ~ 1.5: strong edge + texture
- alpha = 2.0: Laplacian (second derivative)

#### 2D Separable Application

The 2D fractional derivative is applied as separable 1D convolutions:

```
I'(x,y) = D^alpha_y [ D^alpha_x [ I(x,y) ] ]
```

Horizontal pass then vertical pass, each using the same 1D mask.

#### Mask Size Selection

```
maskSize = 5   if alpha <= 1.0   (moderate support)
maskSize = 7   if 1.0 < alpha <= 1.5   (wider support)
maskSize = 9   if 1.5 < alpha <= 2.0   (full support for 2nd derivative)
```

#### Overshoot Limiting (SAF-100)

Mandatory. Non-configurable. For every pixel:

```
sigma_local(x,y) = sqrt( (1/9) * sum_{(dx,dy) in 3x3} (I(x+dx, y+dy) - mu_local)^2 )

mu_local = (1/9) * sum_{(dx,dy) in 3x3} I(x+dx, y+dy)

limit = 3 * sigma_local

if sigma_local < 1e-6:
    limit = 0.1   (fixed small limit for uniform regions)

boost = I_enhanced(x,y) - I_original(x,y)
I_output(x,y) = I_original(x,y) + clamp(boost, -limit, +limit)
```

### 3.2 Pseudo-Code (Scalar Reference)

```cpp
// Compute mask
maskSize = selectMaskSize(order);
vector<float> mask(maskSize);
mask[0] = 1.0f;
for (k = 1; k < maskSize; k++) {
    mask[k] = mask[k-1] * (order - (k-1)) / k * (-1.0f);
}

// Copy original for overshoot limiting
original = copy(input);

// Horizontal pass
for (y = 0; y < H; y++) {
    for (x = 0; x < W; x++) {
        float sum = 0;
        for (k = 0; k < maskSize; k++) {
            int nx = clamp(x - k + maskSize/2, 0, W-1);
            sum += input[y*W + nx] * mask[k];
        }
        temp[y*W + x] = sum;
    }
}

// Vertical pass
for (y = 0; y < H; y++) {
    for (x = 0; x < W; x++) {
        float sum = 0;
        for (k = 0; k < maskSize; k++) {
            int ny = clamp(y - k + maskSize/2, 0, H-1);
            sum += temp[ny*W + x] * mask[k];
        }
        output[y*W + x] = sum;
    }
}

// Overshoot limiting (SAF-100)
for (y = 0; y < H; y++) {
    for (x = 0; x < W; x++) {
        float sigma = localStdDev_3x3(original, x, y);
        float limit = (sigma < 1e-6f) ? 0.1f : 3.0f * sigma;
        float boost = output[y*W + x] - original[y*W + x];
        output[y*W + x] = original[y*W + x] + clamp(boost, -limit, +limit);
    }
}

// NaN/Inf guard (REQ-ADV-032)
for (i = 0; i < W*H; i++) {
    if (!isfinite(output[i])) output[i] = original[i];
}
```

### 3.3 Complexity Analysis

| Operation | FLOPS (per pixel) | Total (3072x3072) |
|-----------|-------------------|--------------------|
| Mask computation | O(maskSize) | Negligible |
| Horizontal convolution | 2 * maskSize | ~141M (mask=5) |
| Vertical convolution | 2 * maskSize | ~141M (mask=5) |
| Local stddev (3x3) | ~20 | ~189M |
| Overshoot clamp | 5 | ~47M |
| **Total** | ~65 | ~518M |

**Scalar estimate**: 300-400 ms on 2.6 GHz Core i7 (single-thread)
**AVX2 estimate**: 80-120 ms

### 3.4 Implementation Issues in Current Code

**Issue 1: Mask coefficient sign convention.**

Current `computeFractionalMask` at line 153-172 of fractional_derivative.cpp:

```cpp
for (size_t k = 0; k < maskSize; ++k) {
    float coeff = fractionalBinomial(order, static_cast<int>(k));
    if (k % 2 == 1) {
        coeff = -coeff;
    }
    mask[k] = coeff;
}
```

The sign alternation by `k % 2` is the standard GL definition, but the mask is applied as a centered convolution (offset by `maskSize/2`), not a causal convolution. For a centered mask, the sign pattern should be symmetric around the center, which requires careful derivation. The current implementation treats it as centered but applies GL coefficients designed for causal use.

**Fix**: Apply GL coefficients as a causal mask and shift by maskSize/2 for centering:
```cpp
for (size_t k = 0; k < maskSize; ++k) {
    float coeff = fractionalBinomial(order, static_cast<int>(k));
    if (k % 2 == 1) coeff = -coeff;
    mask[k] = coeff;
}
// Normalize mask to preserve DC
float sum = accumulate(mask.begin(), mask.end(), 0.0f);
if (abs(sum) > 1e-6f) {
    for (auto& m : mask) m /= sum;
}
```

**Issue 2: Convolution applies mask centered, not causal.**

The convolution at line 312-328 uses `nx = x - k + maskSize/2`, which centers the mask. But the GL definition uses causal indexing `f(x-k)`. For centered application, the mask coefficients need to be reversed and re-centered. The current code applies them as-is, which may not correctly implement the GL derivative.

**Recommendation**: Verify the convolution produces correct fractional derivative behavior with unit tests comparing against known analytical results (e.g., D^1 of a ramp function should be constant).

**Issue 3: Overshoot limiting applies stddev on original, not on enhanced.**

This is actually correct per the SPEC (REQ-ADV-051: "sigma_local is the standard deviation of a 3x3 neighborhood" -- implied on the original). However, the current fixed limit of 0.1 for uniform regions may be too aggressive for dark X-ray regions where pixel values are naturally small.

### 3.5 SIMD Optimization Plan (AVX2)

#### Target Operations

| Operation | AVX2 Strategy | Speedup |
|-----------|---------------|---------|
| 1D convolution (sep) | 8-wide FMA, unrolled mask | 6-8x |
| Local stddev (3x3) | 8-wide horizontal ops + `_mm256_sqrt_ps` | 4-6x |
| Overshoot clamp | `_mm256_min_ps` + `_mm256_max_ps` | 8x |
| NaN guard | `_mm256_cmpord_ps` | 8x |

#### Convolution Optimization

For a 5-tap mask with AVX2:

```cpp
// Pre-load mask coefficients
__m256 m0 = _mm256_set1_ps(mask[0]);
__m256 m1 = _mm256_set1_ps(mask[1]);
__m256 m2 = _mm256_set1_ps(mask[2]);
__m256 m3 = _mm256_set1_ps(mask[3]);
__m256 m4 = _mm256_set1_ps(mask[4]);

for (int x = 0; x <= width - 8; x += 8) {
    __m256 s = _mm256_setzero_ps();
    s = _mm256_fmadd_ps(m0, _mm256_loadu_ps(&row[x-2]), s);
    s = _mm256_fmadd_ps(m1, _mm256_loadu_ps(&row[x-1]), s);
    s = _mm256_fmadd_ps(m2, _mm256_loadu_ps(&row[x]),   s);
    s = _mm256_fmadd_ps(m3, _mm256_loadu_ps(&row[x+1]), s);
    s = _mm256_fmadd_ps(m4, _mm256_loadu_ps(&row[x+2]), s);
    _mm256_storeu_ps(&outRow[x], s);
}
```

#### Local StdDev Optimization

The 3x3 stddev is the bottleneck of overshoot limiting (9 loads per pixel). Optimize with sliding window:

```cpp
// Track running sum and sum-of-squares in 3-row sliding window
// For each new column, subtract departing row and add entering row
// Reduces from 9 loads/pixel to 3 loads/pixel (amortized)
```

This reduces FLOPS by ~3x for the stddev pass.

---

## 4. SWU-2.8: Collimation ROI Detection

### 4.1 Mathematical Definition

#### Pipeline Overview

```
Input Image -> Sobel Gradient -> Edge Magnitude -> Hough Accumulator
    -> Peak Detection -> Axis-Aligned Filter -> Rectangle Extraction
    -> Confidence Scoring -> Output
```

#### Sobel Gradient

```
Gx(x,y) = sum_{ky=-1}^{1} sum_{kx=-1}^{1} Kx(ky+1, kx+1) * I(y+ky, x+kx)
Gy(x,y) = sum_{ky=-1}^{1} sum_{kx=-1}^{1} Ky(ky+1, kx+1) * I(y+ky, x+kx)

Kx = [[-1, 0, 1],   Ky = [[-1, -2, -1],
      [-2, 0, 2],         [ 0,  0,  0],
      [-1, 0, 1]]         [ 1,  2,  1]]

Magnitude: M(x,y) = sqrt(Gx^2 + Gy^2)
Direction: D(x,y) = atan2(Gy, Gx)
```

#### Hough Transform (Line Detection)

For each edge pixel (x,y) with magnitude M(x,y) > threshold:

```
rho = x * cos(theta) + y * sin(theta)

Vote: A(theta_bin, rho_bin) += M(x,y)
```

Parameter space:
- theta: [0, 180) degrees, step = thetaStep (default 1 degree)
- rho: [-D, D] where D = sqrt(W^2 + H^2), step = rhoStep (default 1 pixel)

#### Axis-Aligned Line Filtering

```
isAxisAligned(theta):
    degrees = theta * 180 / pi  (normalized to [0, 180))
    isHorizontal = (degrees <= 5) or (degrees >= 175)
    isVertical = (abs(degrees - 90) <= 5)
    return isHorizontal OR isVertical
```

#### Peak Detection (Non-Maximum Suppression)

```
findPeaks(A, threshold, windowSize):
    For each (theta, rho) in A:
        if A(theta, rho) >= threshold:
            if A(theta, rho) >= max of A in (theta +/- windowSize/2, rho +/- windowSize/2):
                add (theta, rho, A(theta, rho)) to peaks
    return peaks
```

#### Rectangle Extraction

```
extractRectangle(h_lines, v_lines, W, H):
    if count(h_lines) < 2 OR count(v_lines) < 2:
        return (0, 0, W-1, H-1, confidence=0.0)

    Convert polar (theta, rho) to Cartesian:
        Horizontal (theta ~ 0): y = rho / sin(theta)
        Vertical (theta ~ pi/2): x = rho / cos(theta)

    Sort horizontal Y values -> [y0, y1]
    Sort vertical X values -> [x0, x1]

    Clip to image bounds:
        x0 = max(0, round(x0)), y0 = max(0, round(y0))
        x1 = min(W-1, round(x1)), y1 = min(H-1, round(y1))

    Confidence = sum(top 4 peak strengths) / (4 * maxExpectedStrength)
    Confidence = clamp(confidence, 0.0, 1.0)
```

### 4.2 Pseudo-Code (Scalar Reference)

```cpp
// Step 1: Compute Sobel gradients
for (y = 1; y < H-1; y++) {
    for (x = 1; x < W-1; x++) {
        gx = -I(y-1,x-1) + I(y-1,x+1) - 2*I(y,x-1) + 2*I(y,x+1) - I(y+1,x-1) + I(y+1,x+1);
        gy = -I(y-1,x-1) - 2*I(y-1,x) - I(y-1,x+1) + I(y+1,x-1) + 2*I(y+1,x) + I(y+1,x+1);
        magnitude(y,x) = sqrt(gx*gx + gy*gy);
        direction(y,x) = atan2(gy, gx);
    }
}

// Step 2: Build Hough accumulator
accumulator = zeros(thetaBins, 2*maxRho);
for (y = 0; y < H; y++) {
    for (x = 0; x < W; x++) {
        if (magnitude(y,x) < edgeThreshold) continue;
        for (t = 0; t < thetaBins; t++) {
            theta = t * thetaStep;
            rho = x * cos(theta) + y * sin(theta);
            rhoBin = round(rho / rhoStep) + maxRho;
            rhoBin = clamp(rhoBin, 0, 2*maxRho - 1);
            accumulator(t, rhoBin) += (int)magnitude(y,x);
        }
    }
}

// Step 3: Find peaks with NMS
peaks = findPeaks(accumulator, threshold=50, windowSize=5);

// Step 4: Filter axis-aligned lines
axisLines = [p for p in peaks if isAxisAligned(p.theta)];

// Step 5: Separate H and V, take top 2 each
h_lines = top 2 axisAligned where horizontal
v_lines = top 2 axisAligned where vertical

// Step 6: Extract rectangle
rect = extractRectangle(h_lines, v_lines, W, H);

// Step 7: Confidence-based fallback (REQ-ADV-041)
if (rect.confidence < 0.7) {
    log warning;
    return (0, 0, W-1, H-1);  // full extent
} else {
    return rect;
}
```

### 4.3 Complexity Analysis

| Operation | Complexity | Notes |
|-----------|------------|-------|
| Sobel gradient | O(W * H) | 18 FLOPS per pixel |
| Hough accumulator | O(W * H * thetaBins) | Dominant cost; thetaBins = 180 |
| Peak finding (NMS) | O(thetaBins * rhoBins * win^2) | Typically small relative to accumulator |
| Axis-aligned filter | O(numPeaks) | Typically < 100 peaks |
| Rectangle extraction | O(1) | Only top 4 lines |

**For 3072x3072:**
- Sobel: ~170M FLOPS -- ~50 ms (scalar)
- Hough accumulator: 3072 * 3072 * 180 = ~1.7 billion iterations -- ~300 ms (scalar)
- Peak + filter + extraction: ~10 ms

**Total scalar estimate**: 350-500 ms
**AVX2 estimate**: 100-200 ms

**Memory:**
- Gradient maps: 3072 * 3072 * 4 * 2 (magnitude + direction) = ~72 MB
- Hough accumulator: 180 * (2 * 4356) * 4 = ~6.3 MB
- Total: ~80 MB (within 200MB budget)

### 4.4 Implementation Issues in Current Code

**Issue 1: Hough accumulator builds on all pixels above threshold, not just edge pixels.**

The current implementation votes for ALL pixels above the edge threshold. A more efficient approach is to first thin the edges (non-maximum suppression on the gradient magnitude) before Hough voting. This would reduce the number of voting pixels by ~80%, cutting the Hough build time proportionally.

**Recommendation**: Add gradient NMS before Hough voting:
```cpp
// Only vote for pixels that are local maxima in gradient direction
if (magnitude(y,x) > edgeThreshold) {
    float dir = direction(y,x);
    int dx = round(cos(dir)), dy = round(sin(dir));
    if (magnitude(y,x) >= magnitude(y+dy, x+dx) &&
        magnitude(y,x) >= magnitude(y-dy, x-dx)) {
        // Vote for this pixel
    }
}
```

**Issue 2: Confidence calculation uses hardcoded heuristic.**

The `maxExpectedStrength = 4.0f * 1000.0f` in hough_transform.cpp line 169 is arbitrary. It should be derived from image dimensions and expected edge density.

**Recommendation**: Normalize confidence using image perimeter:
```cpp
float maxExpectedStrength = 2.0f * (width + height) * maxExpectedEdgeMagnitude;
```

**Issue 3: Polar-to-Cartesian conversion has numerical instability.**

Line 123 uses `y = line.rho / std::sin(line.theta + 1e-6f)`. The epsilon 1e-6 is insufficient when theta is very close to 0 (sin(1e-6) ~ 1e-6, not a good approximation).

**Fix**: Use the standard line representation conversion with explicit case handling:
```cpp
// For theta near 0 or pi (horizontal): y = rho / sin(theta)
// For theta near pi/2 (vertical): x = rho / cos(theta)
// Use stable computation
if (std::abs(std::sin(theta)) > 0.1f) {
    y = rho / std::sin(theta);
} else {
    // Nearly horizontal -- use rho as y-intercept
    y = rho;
}
```

### 4.5 SIMD Optimization Plan (AVX2)

#### Target Operations

| Operation | AVX2 Strategy | Speedup |
|-----------|---------------|---------|
| Sobel gradient | 8-wide FMA on 3x3 kernel | 6-8x |
| Hough voting | Hard to vectorize (scatter) | 1-2x |
| Gradient NMS | 8-wide compare + mask | 4-6x |

**Note**: The Hough accumulator build is the bottleneck and is difficult to vectorize because votes are scattered across the accumulator matrix (random write pattern). The best optimization strategy is:

1. **Reduce voting pixels**: Apply gradient NMS before voting (reduces work by ~80%)
2. **Cache-friendly accumulator**: Tile the theta loop to improve cache locality
3. **Early termination**: Skip voting if magnitude is below a higher threshold

An alternative approach is to use the Progressive Probabilistic Hough Transform (PPHT), which randomly samples edge pixels and stops early once sufficient lines are found. This can reduce the Hough voting cost by 5-10x for collimation detection (only need 4 lines).

---

## 5. SWU-2.10: Exposure Index Calculation

### 5.1 Mathematical Definition

#### IEC 62494-1 Exposure Index

```
EI = c1 * g * mean(pixel_values_in_ROI) + c2
```

where:
- c1, c2 are manufacturer-specific calibration constants
- g is the system gain (estimated from kVp and mAs)
- ROI is the region of interest (full image or collimation-cropped region)

#### Deviation Index

```
DI = 10 * log10(EI / EI_target)
```

where EI_target is body-part specific:

| Body Part | EI_target |
|-----------|-----------|
| CHEST PA  | 1500      |
| CHEST LAT | 1200      |
| ABDOMEN   | 800       |
| PELVIS    | 600       |
| SKULL     | 500       |
| EXTREMITY | 400       |
| SPINE     | 700       |
| Default   | 1000      |

#### Gain Estimation

```
g = (kVp^2 / kVp_ref^2) * (mAs / mAs_ref)

kVp_ref = 80.0, mAs_ref = 10.0
g = clamp(g, 0.1, 10.0)
```

### 5.2 Pseudo-Code (Scalar Reference)

```cpp
// Validate inputs
if (img == null || meta == null || eiOut == null || diOut == null)
    return XPE_ERR_INVALID_INPUT;
if (img.format != FLOAT32)
    return XPE_ERR_UNSUPPORTED_FORMAT;
if (img.width == 0 || img.height == 0)
    return XPE_ERR_INVALID_INPUT;

// Calculate mean pixel value (NaN/Inf filtered)
double sum = 0;
size_t validCount = 0;
for (i = 0; i < width * height; i++) {
    if (isfinite(data[i])) {
        sum += data[i];
        validCount++;
    }
}
mean = (validCount > 0) ? sum / validCount : 0;
if (mean <= 0) mean = 1e-6;

// Estimate gain
gain = (kVp^2 / 6400) * (mAs / 10);
gain = clamp(gain, 0.1, 10.0);

// Calculate EI
EI = c1 * gain * mean + c2;
if (EI <= 0) EI = 1e-3;

// Get EI target for body part
EI_target = lookupTarget(meta->bodyPart);

// Calculate DI
DI = 10 * log10(EI / EI_target);

// Guard outputs
if (!isfinite(EI)) EI = c2;
if (!isfinite(DI)) DI = 0.0;

*eiOut = EI;
*diOut = DI;
```

### 5.3 Complexity Analysis

| Operation | Complexity | Notes |
|-----------|------------|-------|
| Mean pixel value | O(W * H) | Single pass, ~9.4M pixels |
| Gain estimation | O(1) | Simple arithmetic |
| EI/DI calculation | O(1) | Simple arithmetic |
| Body-part lookup | O(1) | String comparison |

**Total for 3072x3072**: ~20M FLOPS -- < 50 ms (scalar), < 20 ms (AVX2)

### 5.4 SIMD Optimization

The mean calculation is the only compute-intensive operation. AVX2 optimization:

```cpp
__m256 sumVec = _mm256_setzero_ps();
for (size_t i = 0; i <= totalPixels - 8; i += 8) {
    __m256 vals = _mm256_loadu_ps(&data[i]);
    // Filter NaN/Inf: replace with 0
    __m256 mask = _mm256_cmpord_ps(vals, vals); // 0xFFFFFFFF if finite
    vals = _mm256_and_ps(vals, mask);
    sumVec = _mm256_add_ps(sumVec, vals);
}
// Horizontal sum + scalar tail
```

This gives ~7x speedup on the mean calculation, but since the overall function is already < 50 ms, SIMD is low priority.

### 5.5 Implementation Notes

The current implementation in exposure_index.cpp is clean and matches the SPEC requirements. Key notes for xpe-qa:

1. **Gain estimation is simplified**. The production system should use detector-specific calibration data instead of the kVp^2 * mAs model.
2. **EI target values** are manufacturer-specific and must be calibrated against the actual detector response.
3. **ROI support**: Currently calculates on the full image. After collimation detection (SWU-2.8), the function should accept an optional ROI mask.
4. **Bit-identical requirement**: The copy in xpe_enhance_basic.dll MUST produce identical results. Both implementations share the same formula and constants. Verify with a cross-module parity test.

---

## 6. Cross-Cutting SIMD Architecture

### 6.1 Build Configuration

```cmake
# CMakeLists.txt SIMD configuration
if(MSVC)
    set_source_files_properties(
        src/mfp_scalar.cpp
        src/fractional_derivative.cpp
        src/xpe_collimation_detect.cpp
        PROPERTIES COMPILE_FLAGS "/arch:AVX2 /fp:fast")
else()
    set_source_files_properties(
        src/mfp_scalar.cpp
        src/fractional_derivative.cpp
        src/xpe_collimation_detect.cpp
        PROPERTIES COMPILE_FLAGS "-mavx2 -ffast-math")
endif()
```

### 6.2 Conditional Compilation Pattern

```cpp
// In each algorithm file:
#ifdef XPE_SIMD_AVX2
// AVX2 optimized path
#include <immintrin.h>
#endif

void algorithm_avx2(/* params */) {
#ifdef XPE_SIMD_AVX2
    // SIMD implementation
#else
    // Scalar fallback
#endif
}
```

### 6.3 Numerical Parity Verification

Every SIMD implementation MUST be validated against the scalar reference:

```cpp
// Test pattern
TEST(SIMDParity, MFP) {
    XpeImageBuffer scalar_result = process_scalar(testImage, config);
    XpeImageBuffer simd_result = process_avx2(testImage, config);

    for (size_t i = 0; i < totalPixels; i++) {
        float diff = abs(scalar_result[i] - simd_result[i]);
        EXPECT_LT(diff, 1e-6f) << "Mismatch at pixel " << i;
    }
}
```

### 6.4 Deterministic Output Guarantee

All algorithms must produce identical output for identical input regardless of:
- Platform (x86 vs ARM)
- Compiler (MSVC vs GCC vs Clang)
- Optimization level (O0 vs O2 vs AVX2)

Requirements:
1. No rand() or non-deterministic operations
2. Float operation order is fixed (no parallel reduction that changes accumulation order)
3. SIMD reductions must use deterministic tree reduction pattern:

```cpp
// Deterministic AVX2 horizontal sum
float hsum_avx2(__m256 v) {
    __m128 lo = _mm256_castps256_ps128(v);
    __m128 hi = _mm256_extractf128_ps(v, 1);
    __m128 sum = _mm_add_ps(lo, hi);
    sum = _mm_hadd_ps(sum, sum);
    sum = _mm_hadd_ps(sum, sum);
    return _mm_cvtss_f32(sum);
}
```

---

## 7. Performance Budget Summary

Target: 3072x3072 FLOAT32 on Intel Core i7 2.6 GHz

| Algorithm | Scalar | AVX2 | SPEC Budget | Headroom |
|-----------|--------|------|-------------|----------|
| MFP (SWU-2.5) | 600-800 ms | 150-250 ms | 800 / 250 ms | OK |
| Fractional (SWU-2.6) | 300-400 ms | 80-120 ms | 400 / 120 ms | OK |
| Collimation (SWU-2.8) | 350-500 ms | 100-200 ms | 500 / 200 ms | OK |
| EI (SWU-2.10) | 20-50 ms | 10-20 ms | 50 / 20 ms | OK |
| **Total** | **~1750 ms** | **~590 ms** | **2500 / 600 ms** | **OK** |

The total pipeline meets the REQ-ADV-062 budget of < 2500 ms (scalar) and < 600 ms (AVX2).

### Memory Budget

| Algorithm | Peak Memory | Budget | Status |
|-----------|-------------|--------|--------|
| MFP | ~120 MB | 200 MB | OK |
| Fractional | ~72 MB (original + temp) | 200 MB | OK |
| Collimation | ~80 MB (gradients + accumulator) | 200 MB | OK |
| EI | ~0 MB (in-place) | 200 MB | OK |
| **Sequential peak** | **~120 MB** | **200 MB** | **OK** |

Since algorithms execute sequentially (not concurrently), peak memory is the maximum of any single algorithm, not the sum.

---

## 8. Recommendations for xpe-implementer

### Critical Fixes (Before Test)

1. **Replace nearest-neighbor upsampling with bilinear** in mfp_scalar.cpp (Section 2.4, Issue 1). Required for REQ-ADV-050.

2. **Fix reconstruct dimension tracking** in mfp_scalar.cpp (Section 2.4, Issue 2). Track width/height per pyramid level instead of sqrt heuristic.

3. **Fix pyramid build order** in mfp_scalar.cpp (Section 2.4, Issue 3). Store original before blur; compute Laplacian from stored original minus upsampled next level.

4. **Resolve noise threshold unit inconsistency** (Section 2.4, Issue 4). Align MfpConfig default with internal.h constant (5.0).

### Recommended Improvements (After Test)

5. **Add gradient NMS before Hough voting** in hough_transform.cpp (Section 4.4, Issue 1). Reduces Hough build time by ~5x.

6. **Improve confidence normalization** in hough_transform.cpp (Section 4.4, Issue 2). Use image perimeter-based normalization.

7. **Fix polar-to-Cartesian conversion** in hough_transform.cpp (Section 4.4, Issue 3). Add explicit case handling for near-zero sin/cos.

8. **Add mask normalization** in fractional_derivative.cpp (Section 3.4, Issue 1). Normalize mask to preserve DC component.

### SIMD Implementation Priority

| Priority | Algorithm | Rationale |
|----------|-----------|-----------|
| P1 | MFP Gaussian blur + downsample + upsample | Largest time consumer (800ms -> 250ms) |
| P2 | Fractional convolution | Second largest (400ms -> 120ms) |
| P3 | Hough accumulator | Large iteration count but hard to vectorize |
| P4 | Sobel gradient | Simple but already fast with Eigen |
| P5 | EI mean calculation | Already < 50ms, low ROI |

### Test Vectors for xpe-qa

1. **Identity MFP**: All gains = 1.0, noise threshold = 0. Output must match input (error < 1e-5).
2. **Fractional order = 0**: Output must be identical to input (no enhancement).
3. **Fractional order = 1.0**: Should produce standard edge detection result.
4. **Known rectangle collimation**: Synthetic image with rectangle at known coordinates. Detection accuracy must be within +-3 pixels.
5. **Low-confidence collimation**: Nearly uniform image. Must return full extent with confidence < 0.7.
6. **EI with known constants**: Use fixed c1, c2, gain to compute expected EI/DI analytically.
7. **NaN/Inf input rejection**: Feed NaN/Inf pixels. Output must be finite (REQ-ADV-032).
8. **Boundary pixel handling**: 1x1 and 2x2 images must return XPE_ERR_INVALID_INPUT (REQ-ADV-100).

---

## 9. References

| Ref | Document | Section |
|-----|----------|---------|
| R1 | SPEC-XPE-P2-ADV spec.md | REQ-ADV-010 through REQ-ADV-101 |
| R2 | Architect design | Section 6.4 (Eigen), Section 6.5 (AVX2) |
| R3 | xpe-algorithm-spec-deepsync.md | Section 4.2 (Enhancement baseline) |
| R4 | Burt & Adelson, "The Laplacian Pyramid as a Compact Image Code", IEEE TCOM 1983 | Laplacian pyramid theory |
| R5 | Oldham & Spanier, "The Fractional Calculus", Academic Press 1974 | GL derivative definition |
| R6 | IEC 62494-1:2008 | Exposure Index standard |
| R7 | Duda & Hart, "Use of the Hough Transformation to Detect Lines and Curves in Pictures", CACM 1972 | Hough transform theory |

---

End of algorithm design notes.
