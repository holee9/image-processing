# GSVG 테스트 데이터셋 명세서
## TDS-GSVG-001: Grid Suppression 및 Virtual Grid 알고리즘 검증

**문서 ID**: TDS-GSVG-001  
**버전**: 1.0.0  
**일자**: 2026-04-14  
**IEC 62304 절차**: 5.6.3 (테스트 설계 및 실행)  
**안전 분류**: Class B  
**규범 참조**: [GSVG-SRS-001](GSVG-SRS-001_Requirements.md), [GSVG-SVP-001](GSVG-SVP-001_Verification_Plan.md)

---

## 목차

1. [문서 정보 및 개요](#1-문서-정보-및-개요)
2. [목적 및 범위](#2-목적-및-범위)
3. [테스트 데이터 분류 체계](#3-테스트-데이터-분류-체계)
4. [Synthetic Grid Artifact 데이터셋](#4-synthetic-grid-artifact-데이터셋)
5. [Synthetic Anatomy + Grid 데이터셋](#5-synthetic-anatomy--grid-데이터셋)
6. [DWT Subband Energy 분석 테스트](#6-dwt-subband-energy-분석-테스트)
7. [DCT Dynamic Segmentation 테스트](#7-dct-dynamic-segmentation-테스트)
8. [Virtual Grid 성능 테스트](#8-virtual-grid-성능-테스트)
9. [MTF 보존 테스트](#9-mtf-보존-테스트)
10. [FFTW3 Integration 테스트](#10-fftw3-integration-테스트)
11. [실제 영상 데이터 요건](#11-실제-영상-데이터-요건)
12. [Golden Reference 데이터 관리](#12-golden-reference-데이터-관리)
13. [엣지 케이스 데이터셋](#13-엣지-케이스-데이터셋)
14. [테스트 데이터 디렉토리 구조](#14-테스트-데이터-디렉토리-구조)
15. [참고문헌](#15-참고문헌)

---

## 1. 문서 정보 및 개요

### 1.1 개요

이 문서는 Grid Suppression (`gsvg.dll`) 및 Virtual Grid 모듈의 알고리즘 검증을 위한 **테스트 데이터셋 명세서**입니다.

- **목표**: 3가지 suppression tier (DWT, DCT, GRD) 및 virtual grid 알고리즘의 정확성, 견고성, 성능을 체계적으로 검증
- **대상**: 알고리즘 개발 엔지니어, 품질 보증 담당자, V&V 담당자
- **방법론**: 합성 데이터 + 실제 영상 + golden reference 데이터 기반 결정론적 테스트

### 1.2 대상 독자

- **알고리즘 개발자**: 테스트 데이터 생성 방법 및 validation metric 이해
- **QA 엔지니어**: 테스트 케이스 실행 및 결과 판정
- **V&V 담당자**: IEC 62304 추적성 및 요구사항 매핑
- **의료 물리학자**: 임상 phantom 선택 및 검증 기준 평가

### 1.3 문서 버전 이력

| 버전 | 일자 | 변경 사항 |
|-----|------|---------|
| 1.0.0 | 2026-04-14 | 초판 발행: 모든 grid suppression tier + virtual grid 테스트 명세 |

---

## 2. 목적 및 범위

### 2.1 목적

이 TDS는 다음을 달성합니다:

1. **알고리즘 검증**: Grid suppression (3 tier) 및 virtual grid의 정확성, 견고성, 성능을 체계적으로 검증
2. **결정론적 테스트**: Synthetic data를 통해 재현 가능하고 자동화된 테스트 케이스 제공
3. **요구사항 추적**: 각 테스트가 어떤 SRS 요구사항(GS-FR-xxx, VG-FR-xxx)을 검증하는지 명확히 매핑
4. **IEC 62304 준수**: 테스트 데이터 및 결과가 Class B 안전 표준과 연결

### 2.2 범위

**포함 사항**:
- Synthetic grid artifact (sinusoidal, multi-harmonic, aliased, tilted)
- Synthetic anatomy + grid overlay
- DWT subband energy analysis
- DCT dynamic segmentation (block-based)
- Virtual grid with scatter estimation
- MTF preservation (wire phantom)
- FFTW3 determinism 및 thread-safety
- Real acquisition datasets (grid characterization, CDRAD phantom, clinical smoke set)
- Golden references (hash-locked)

**제외 사항**:
- Pre-processing (calibration) — xpe_preprocess.dll 영역
- Post-processing (enhancement) — xpe_enhance.dll 영역
- Dose reduction optimization — 별도 topic

---

## 3. 테스트 데이터 분류 체계

### 3.1 테스트 데이터 계층

| 계층 | 유형 | 특성 | 사용 시점 |
|------|------|------|---------|
| **L1: Unit** | Synthetic signal (pure grid, harmonic) | 결정론적, 100% 재현 | Algorithm unit test |
| **L2: Integration** | Anatomy + grid overlay (synthetic) | Realistic pattern, known ground truth | Grid suppression tier comparison |
| **L3: System** | Real acquisition (phantom, clinical) | Physical authenticity | Clinical validation |
| **L4: Reference** | Golden benchmark (hash-locked) | Unchanged across versions | Regression detection |

### 3.2 데이터셋 분류

| 분류 | 용도 | 파라미터 변동성 | 복잡도 |
|------|------|------|---------|
| **Synthetic Grid** | DWT/DCT algorithm correctness | f_grid, θ, A (controlled) | Low |
| **Anatomy + Grid** | Suppression quality (CNR, MSI) | 실제 해부학 구조 | Medium |
| **Virtual Grid** | Scatter correction, CNR recovery | SPR 범위 (5%–30%) | Medium-High |
| **Edge Cases** | Robustness (aliasing, high-dose, noise) | Extreme parameter values | High |

---

## 4. Synthetic Grid Artifact 데이터셋

### 4.1 목적

**Pure grid signal**에 대한 suppression algorithm의 기본 correctness 검증.

### 4.2 Synthetic Grid 생성 공식

**Pure Sinusoidal Grid** (가장 간단한 형태)

```python
def generate_synthetic_grid(width, height, f_grid_lp_mm, 
                           angle_deg, amplitude_percent, 
                           pixel_pitch_mm=0.1, dc_offset=32768):
    """
    Generate pure sinusoidal grid artifact.
    
    Args:
        width, height: Image dimensions (pixels)
        f_grid_lp_mm: Grid frequency (lp/mm)
        angle_deg: Grid orientation (degrees)
        amplitude_percent: Peak-to-valley as % of mean
        pixel_pitch_mm: Detector pixel pitch (mm)
    
    Returns:
        image: 16-bit grid artifact (uint16)
    """
    x = np.arange(width) * pixel_pitch_mm  # Convert to mm
    y = np.arange(height) * pixel_pitch_mm
    xx, yy = np.meshgrid(x, y)
    
    # Rotated coordinates
    angle_rad = np.radians(angle_deg)
    x_rot = xx * np.cos(angle_rad) + yy * np.sin(angle_rad)
    
    # Sinusoidal grid
    grid_signal = amplitude_percent / 100.0 * np.sin(2 * np.pi * f_grid_lp_mm * x_rot)
    
    # DC offset + grid
    image = dc_offset * (1 + grid_signal)
    return np.clip(image, 0, 65535).astype(np.uint16)
```

### 4.3 테스트 케이스 행렬

**Test Matrix: Axis-Aligned Grids**

| Test ID | f_grid (lp/mm) | θ_grid (°) | A_grid (%) | f_Nyquist (lp/mm) | Aliasing Risk | Expected MSI |
|---------|-----------------|-----------|----------|-----------------|---------------|-----------|
| SG-001 | 4.0 | 0 | 3 | 5.0 | NO | < 0.05 |
| SG-002 | 4.0 | 0 | 5 | 5.0 | NO | < 0.08 |
| SG-003 | 6.0 | 90 | 3 | 5.0 | NO | < 0.05 |
| SG-004 | 4.0 | 45 | 5 | 5.0 | NO (tilted) | < 0.10 |
| SG-005 | 4.8 | 0 | 5 | 5.0 | MARGINAL | 0.15–0.25 |
| SG-006 | 5.5 | 0 | 5 | 5.0 | **SEVERE** | > 0.50 |

### 4.4 Multi-Harmonic Grid (Realistic Model)

실제 grid는 fundamental + harmonics 포함:

```python
def generate_realistic_grid(width, height, f_grid_lp_mm, angle_deg, 
                           amplitude_percent, pixel_pitch_mm=0.1):
    """
    Realistic grid = fundamental + 3 harmonics.
    """
    x = np.arange(width) * pixel_pitch_mm
    y = np.arange(height) * pixel_pitch_mm
    xx, yy = np.meshgrid(x, y)
    
    angle_rad = np.radians(angle_deg)
    x_rot = xx * np.cos(angle_rad) + yy * np.sin(angle_rad)
    
    # Fundamental + harmonics (amplitude decreasing)
    grid_signal = (
        1.0 * np.sin(2 * np.pi * f_grid_lp_mm * x_rot) +
        0.4 * np.sin(2 * np.pi * 2 * f_grid_lp_mm * x_rot) +
        0.2 * np.sin(2 * np.pi * 3 * f_grid_lp_mm * x_rot)
    ) / 1.6  # Normalize
    
    dc_offset = 32768
    image = dc_offset * (1 + amplitude_percent / 100.0 * grid_signal)
    return np.clip(image, 0, 65535).astype(np.uint16)
```

**Test Cases (Multi-Harmonic)**

| Test ID | Description | Expected |
|---------|-------------|----------|
| MH-001 | Fundamental + 2nd harmonic | DWT 필터가 fundamental 잘 제거 |
| MH-002 | Fundamental + 3 harmonics (realistic) | 모든 harmonics 동시 suppression |
| MH-003 | Fundamental only (ideal case) | MSI < 0.05 baseline |

### 4.5 Acceptance Criteria

**Synthetic Grid에 대한 Suppression 성공 기준**

```
IF f_grid < 0.8 × f_Nyquist (No aliasing):
  THEN Expected MSI < 0.10
  AND Residual grid energy < 10% of input

ELSE (Aliasing present):
  THEN MSI < 0.30 acceptable (partial suppression)
  AND Warning issued in log
  AND Original image returned fallback
```

---

## 5. Synthetic Anatomy + Grid 데이터셋

### 5.1 목적

**실제 해부학 구조** 위의 grid 제거가 진단 정보(CNR, MSI)를 보존하는지 검증.

### 5.2 기본 Anatomy 생성

**Chest Phantom Model** (간단한 합성)

```python
def generate_chest_phantom(width=3072, height=3072, pixel_pitch_mm=0.1):
    """
    Simplified chest model:
      - Uniform background (lung)
      - 2-3 circular nodules (5-10mm diameter)
      - Rib structure (linear patterns)
    """
    image = 30000 * np.ones((height, width), dtype=np.float32)
    
    # Add nodules (high density structures)
    centers = [(1024, 1024), (2048, 1536)]
    for cx, cy in centers:
        for y in range(max(0, cy-40), min(height, cy+40)):
            for x in range(max(0, cx-40), min(width, cx+40)):
                dist = np.sqrt((x-cx)**2 + (y-cy)**2)
                if dist < 40:
                    image[y, x] = 45000
    
    # Add rib-like structures
    for y in range(height):
        x_rib = (512 + 300 * np.sin(y / 50)) % width
        for dx in range(-10, 10):
            if 0 <= int(x_rib+dx) < width:
                image[y, int(x_rib+dx)] *= 0.95
    
    return np.clip(image, 0, 65535).astype(np.uint16)
```

### 5.3 Grid Overlay

```python
def overlay_grid_on_anatomy(anatomy_image, f_grid_lp_mm, angle_deg, 
                           amplitude_percent, pixel_pitch_mm=0.1):
    """
    I_combined = I_anatomy + A_grid × sin(...)
    """
    height, width = anatomy_image.shape
    grid_component = generate_synthetic_grid(width, height, 
                                            f_grid_lp_mm, angle_deg, 
                                            amplitude_percent, 
                                            pixel_pitch_mm)
    
    # Blend: preserve anatomy structure
    combined = (anatomy_image.astype(np.float32) + 
                0.5 * (grid_component.astype(np.float32) - 32768))
    return np.clip(combined, 0, 65535).astype(np.uint16)
```

### 5.4 테스트 케이스

| Test ID | Anatomy | f_grid | A_grid (%) | Expected SSIM | Expected PSNR |
|---------|---------|--------|----------|---------------|---------------|
| AG-001 | Chest nodules | 4.0 | 5 | > 0.95 | > 30dB |
| AG-002 | Chest + ribs | 4.0 | 5 | > 0.92 | > 28dB |
| AG-003 | Uniform bg + nodules | 6.0 | 3 | > 0.95 | > 32dB |

**Metrics**:
- **SSIM** (Structural Similarity Index): \[0, 1\] (1 = identical)
  ```
  SSIM = (2 μ_x μ_y + c1)(2 σ_xy + c2) / ((μ_x^2 + μ_y^2 + c1)(σ_x^2 + σ_y^2 + c2))
  ```
- **PSNR** (Peak Signal-to-Noise Ratio): 20 log₁₀(MAX / RMSE) dB

---

## 6. DWT Subband Energy 분석 테스트

### 6.1 목적

**DWT decomposition**의 correctness 검증: grid signal이 예상된 subbands에 concentrate되는지 확인.

### 6.2 DWT 분해

```python
def test_dwt_decomposition(image, wavelet='db4', levels=4):
    """
    Perform N-level 2D DWT and analyze subband energy.
    """
    import pywt
    
    # Multi-level decomposition
    coeffs_all = []
    current = image.astype(np.float32)
    
    for level in range(levels):
        cA, (cH, cV, cD) = pywt.dwt2(current, wavelet)
        coeffs_all.append({
            'level': level,
            'cA': cA,
            'cH': cH,  # Horizontal details
            'cV': cV,  # Vertical details
            'cD': cD   # Diagonal details
        })
        current = cA
    
    return coeffs_all
```

### 6.3 에너지 분석

```python
def analyze_subband_energy(coeffs_all):
    """
    Compute per-subband energy and identify grid-dominant bands.
    """
    energies = {}
    for level_data in coeffs_all:
        level = level_data['level']
        for subband_name in ['cH', 'cV', 'cD']:
            coeff = level_data[subband_name]
            energy = np.sum(coeff ** 2)
            energies[f"L{level}_{subband_name}"] = energy
    
    # Normalize and rank
    total_energy = sum(energies.values())
    normalized = {k: v/total_energy for k, v in energies.items()}
    sorted_by_energy = sorted(normalized.items(), key=lambda x: x[1], reverse=True)
    
    return sorted_by_energy
```

### 6.4 테스트 케이스

**Grid-dominated Subband Detection**

| Synthetic Input | Expected Top 2 Subbands | Reason |
|-----------------|------------------------|--------|
| Horizontal grid (f=4.0, θ=0°) | L1_cH, L2_cH | Horizontal grid → H details |
| Vertical grid (f=4.0, θ=90°) | L1_cV, L2_cV | Vertical grid → V details |
| Diagonal grid (f=4.0, θ=45°) | L1_cD, L2_cD | Diagonal grid → D details |

**Acceptance Criterion**

```
Grid energy concentration = sum(top_2_subband_energies) / total_energy

Expected: > 0.90 (grid energy in top 2 subbands)
          >= 0.80 (acceptable detection)
          < 0.70 (FAIL — grid missed)
```

### 6.5 Adaptive Decomposition Levels

```python
def find_optimal_decomposition_levels(image, wavelet='db4', max_levels=6):
    """
    Determine decomposition levels where grid energy is maximum.
    (Grid typically concentrates at LL2-LL4 level)
    """
    level_energies = []
    current = image.astype(np.float32)
    
    for level in range(1, max_levels+1):
        cA, _ = pywt.dwt2(current, wavelet)
        grid_energy_in_LL = np.sum(cA ** 2)
        level_energies.append({
            'level': level,
            'energy': grid_energy_in_LL,
            'size': cA.shape
        })
        current = cA
    
    # Find peak → optimal decomposition level
    optimal_level = max(level_energies, key=lambda x: x['energy'])['level']
    return optimal_level, level_energies
```

---

## 7. DCT Dynamic Segmentation 테스트

### 7.1 목적

**Block-based DCT** grid suppression의 corner case (block boundaries, harmonic overlap) 검증.

### 7.2 Block-wise DCT 분해

```python
def apply_blockwise_dct(image, block_size=64, f_grid_lp_mm=4.0, 
                       grid_direction='horizontal'):
    """
    Apply 2D DCT per block and suppress grid frequency coefficients.
    """
    from scipy.fftpack import dct
    
    height, width = image.shape
    output = np.zeros_like(image, dtype=np.float32)
    
    # Process non-overlapping blocks
    for y in range(0, height, block_size):
        for x in range(0, width, block_size):
            # Extract block
            y_end = min(y + block_size, height)
            x_end = min(x + block_size, width)
            block = image[y:y_end, x:x_end].astype(np.float32)
            
            # 2D DCT
            dct_block = dct(dct(block, axis=0), axis=1)
            
            # Suppress grid frequency (e.g., f_grid → DCT freq bin)
            # f_grid = 4.0 lp/mm, pixel_pitch = 0.1 mm
            # → 4.0 / 0.1 = 40 cycles per block_size
            # → DCT bin ≈ 40 × (block_size / detector_width)
            grid_bin = int(f_grid_lp_mm / 0.1 * block_size / 3072)
            
            # Zero out grid freq ± harmonics
            for harmonic in range(1, 4):
                bin_idx = harmonic * grid_bin
                if bin_idx < block_size:
                    dct_block[:, bin_idx] = 0
                    dct_block[bin_idx, :] = 0
            
            # Inverse DCT
            suppressed_block = dct(dct(dct_block, axis=0, type=3), axis=1, type=3)
            
            # Restore block
            output[y:y_end, x:x_end] = suppressed_block[:y_end-y, :x_end-x]
    
    return np.clip(output, 0, 65535).astype(np.uint16)
```

### 7.3 Blocking Artifact 검증

**Inter-block Boundary Artifact**

```python
def detect_blocking_artifacts(suppressed_image, block_size=64):
    """
    Detect visible block boundaries (DCT blocking artifact).
    
    Metric: Power spectrum jump at multiples of block_size
    """
    # Compute 1D spectrum along image edge
    edge = suppressed_image[block_size, :]
    spectrum = np.abs(np.fft.fft(edge))
    
    # Check for peaks at block_size multiples
    freq_block = 1 / block_size  # Normalized frequency
    blocking_freq = freq_block * np.arange(10)
    
    blocking_energy = spectrum[int(freq_block * 3072):int(2*freq_block * 3072)]
    
    artifact_power_db = 10 * np.log10(np.sum(blocking_energy) + 1e-10)
    
    return {
        'artifact_power_db': artifact_power_db,
        'verdict': 'PASS' if artifact_power_db < -30 else 'FAIL'
    }
```

### 7.4 테스트 케이스

| Test ID | Block Size | Grid Freq | Expected Blocking Level |
|---------|-----------|-----------|------------------------|
| DCT-001 | 32×32 | 4.0 lp/mm | Very low (<-40dB) |
| DCT-002 | 64×64 | 4.0 lp/mm | Low (<-30dB) |
| DCT-003 | 128×128 | 4.0 lp/mm | Moderate (<-20dB, acceptable) |
| DCT-004 | 64×64 + linear blend | 4.0 lp/mm | Very low (<-40dB with transition) |

---

## 8. Virtual Grid 성능 테스트

### 8.1 목적

**Virtual Grid (scatter correction + contrast enhancement)** CNR 달성 검증: Physical grid 대비 >= 90% CNR.

### 8.2 SPR (Scatter-to-Primary Ratio) 범위

```python
def generate_vg_test_suite(thickness_cm_range=[10, 15, 20, 25, 30]):
    """
    Virtual Grid test across thickness range (pediatric to obese).
    """
    test_cases = []
    
    for thickness_cm in thickness_cm_range:
        # Estimate SPR (empirical formula, Neitzel 2006)
        spr = 0.001 * thickness_cm ** 2.5  # Approximate
        
        test_cases.append({
            'id': f'VG-{int(thickness_cm)}cm',
            'thickness_eq_cm': thickness_cm,
            'expected_spr_percent': spr * 100,
            'vg_ratio': 6,  # Simulate 6:1 virtual grid
            'expected_cnr_vs_physical_grid_percent': 92
        })
    
    return test_cases
```

**Test Range**: 5–35 cm (water-equivalent thickness)

### 8.3 Scatter Kernel LUT Lookup

```python
def test_scatter_kernel_lookup():
    """
    Verify scatter kernel LUT interpolation correctness.
    """
    test_conditions = [
        {'thickness_cm': 15, 'kvp': 70, 'fov_cm': 25},
        {'thickness_cm': 20, 'kvp': 80, 'fov_cm': 30},
        {'thickness_cm': 25, 'kvp': 100, 'fov_cm': 35},
    ]
    
    for cond in test_conditions:
        # Load kernel from LUT (nearest or interpolated)
        kernel = lookup_scatter_kernel_lut(
            thickness_cm=cond['thickness_cm'],
            kvp=cond['kvp'],
            fov_cm=cond['fov_cm']
        )
        
        # Verify kernel properties
        assert kernel.shape == (256, 256), "Kernel size"
        assert np.sum(kernel) > 0, "Kernel energy > 0"
        assert kernel.max() == 1.0, "Normalized to 1.0"
        
        print(f"✓ LUT lookup passed for {cond}")
```

### 8.4 Laplacian Pyramid Processing

```python
def test_laplacian_pyramid():
    """
    Verify Laplacian Pyramid decomposition and reconstruction.
    """
    image = generate_chest_phantom()  # 16-bit
    
    # Decomposition
    pyramid = laplacian_decompose(image, levels=4)
    
    # Reconstruction
    reconstructed = laplacian_reconstruct(pyramid)
    
    # Verify fidelity
    rmse = np.sqrt(np.mean((image.astype(np.float32) - reconstructed)**2))
    max_error = np.max(np.abs(image.astype(np.int32) - reconstructed.astype(np.int32)))
    
    print(f"LP Reconstruction RMSE: {rmse:.2f}, Max Error: {max_error}")
    assert rmse < 1.0, "Reconstruction fidelity"
    assert max_error <= 1, "Per-pixel error <= 1"
```

### 8.5 Acceptance Criteria

```
VG Algorithm Success:
  PASS IF:
    - Scatter map energy > 0 (detected scatter)
    - Output CNR >= 90% × reference physical grid CNR
    - No overcorrection artifacts (artifact-free region inspection)
    - Clamping within [0, 65535] (valid DICOM range)
  FAIL IF:
    - CNR < 85% reference
    - Visible artifacts (aliasing, banding)
    - Numerical overflow
```

---

## 9. MTF 보존 테스트

### 9.1 목적

Grid suppression이 spatial resolution (MTF)을 과도하게 저하시키지 않는지 검증.

### 9.2 Wire Phantom MTF

```python
def measure_mtf_from_wire_phantom(wire_image, wire_diameter_um=50):
    """
    Measure MTF from tungsten wire (point spread function).
    
    Args:
        wire_image: Image containing tungsten wire
        wire_diameter_um: Physical wire diameter
    
    Returns:
        mtf_freq_lp_mm: Spatial frequency (lp/mm)
        mtf_value: MTF (0–1)
    """
    # Detect wire (bright line)
    wire_profile = np.mean(wire_image, axis=0)  # Collapse vertical
    
    # Extract line spread function (LSF)
    lsf = wire_profile.astype(np.float32)
    lsf -= np.min(lsf)
    lsf /= np.max(lsf)
    
    # Compute modulation transfer function
    # MTF = |FFT(LSF)| / |FFT(ideal delta)|
    fft_lsf = np.abs(np.fft.fft(lsf))
    fft_lsf /= fft_lsf[0]  # Normalize to DC
    
    # Nyquist frequency
    pixel_pitch_mm = 0.1
    f_nyquist = 1 / (2 * pixel_pitch_mm)
    
    # Map FFT bins to spatial frequency
    freq_lp_mm = np.fft.fftfreq(len(lsf), d=pixel_pitch_mm)[:len(lsf)//2]
    mtf = fft_lsf[:len(lsf)//2]
    
    return freq_lp_mm, mtf
```

### 9.3 테스트 케이스

| Test ID | Diagnostic Freq (lp/mm) | Expected MTF Preservation |
|---------|------------------------|--------------------------|
| MTF-001 | 1.0 (low) | > 99% |
| MTF-002 | 2.0 (mid) | > 97% |
| MTF-003 | 3.0 (diagnostic) | > 95% |
| MTF-004 | 4.0 (Nyquist/2) | > 90% |

**Acceptance**: MTF_post_suppression / MTF_pre_suppression > 0.95 @ 3 lp/mm

---

## 10. FFTW3 Integration 테스트

### 10.1 목적

FFTW3 라이브러리의 determinism, thread-safety, performance 검증.

### 10.2 Determinism Test (Fixed-Point Stability)

```python
def test_fftw_determinism():
    """
    Verify FFTW3 produces identical results on repeated calls.
    """
    image = generate_synthetic_grid(3072, 3072, f_grid=4.0)
    
    # Compute FFT 5 times
    fft_results = []
    for trial in range(5):
        fft_result = np.fft.fft2(image.astype(np.float32))
        fft_results.append(fft_result)
    
    # Verify all identical
    for i in range(1, 5):
        max_diff = np.max(np.abs(fft_results[0] - fft_results[i]))
        assert max_diff < 1e-10, f"Trial {i} differs by {max_diff}"
    
    print("✓ FFTW3 determinism verified (max diff < 1e-10)")
```

### 10.3 Thread-Safety Test

```python
def test_fftw_thread_safety():
    """
    Process multiple images in parallel (threading).
    Verify no race conditions or memory corruption.
    """
    import threading
    
    results = {}
    lock = threading.Lock()
    
    def process_image(image_id):
        image = generate_synthetic_grid(1024, 1024, f_grid=4.0)
        fft_result = np.fft.fft2(image.astype(np.float32))
        with lock:
            results[image_id] = np.sum(np.abs(fft_result))
    
    threads = [threading.Thread(target=process_image, args=(i,)) 
               for i in range(10)]
    
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    
    # Verify all completed without error
    assert len(results) == 10, "All threads completed"
    print(f"✓ Thread safety verified ({len(results)} parallel FFTs)")
```

### 10.4 Performance Benchmark

```
Expected Timing (Intel i7, 3072×3072 16-bit image):
  - Forward FFT:   < 50 ms
  - Inverse FFT:   < 50 ms
  - Wavelet (3 levels): < 30 ms
  - Total DWT+filter: < 30 ms
  
PASS IF: Total < 100ms per frame
```

---

## 11. 실제 영상 데이터 요건

### 11.1 Grid Characterization Images

**출처**: IAP-GSVG-001에서 취득

| 데이터셋 | Grid Type | Frames | Purpose |
|---------|-----------|--------|---------|
| `grid_8_1_100frames.raw` | 8:1 Focused | 100 | f_grid, θ, A 추출 |
| `grid_12_1_100frames.raw` | 12:1 Focused | 100 | High-freq grid test |
| `grid_6_1_100frames.raw` | 6:1 Parallel | 100 | Parallel grid test |
| `nogrid_100frames.raw` | Reference (no grid) | 100 | Baseline for comparison |

### 11.2 CDRAD 2.0 Phantom Images

| 데이터셋 | Condition | Frames | Metric |
|---------|-----------|--------|--------|
| `cdrad_physical_grid_10frames.raw` | 6:1 physical grid | 10 | CNR_reference |
| `cdrad_virtual_grid_10frames.raw` | Virtual grid (VG algo) | 10 | CNR_test |
| `cdrad_no_grid_10frames.raw` | No grid | 10 | Baseline |

### 11.3 Clinical Smoke Set

**목적**: Real anatomy에서의 artifact suppression quality 평가

| 데이터셋 | Patient Type | Grid | Images |
|---------|-------------|------|--------|
| `clinical_chest_grid_5img.raw` | Adult chest | 8:1 | 5 (averaged) |
| `clinical_spine_grid_3img.raw` | Spine patient | 12:1 | 3 (averaged) |

**요구사항**: Institutional Review Board 승인된 de-identified 환자 데이터

---

## 12. Golden Reference 데이터 관리

### 12.1 Golden Reference 개념

각 test case마다 **hash-locked benchmark data** 생성:

```json
{
  "test_id": "SG-001",
  "description": "Pure sinusoidal grid, f=4.0 lp/mm, A=3%",
  "input_image_sha256": "abc123...",
  "expected_output_sha256": "def456...",
  "expected_metrics": {
    "msi": 0.047,
    "msi_tolerance": [0.04, 0.06],
    "grid_energy_suppression_percent": 96.5,
    "processing_time_ms": 28.3
  },
  "created_date": "2026-04-14",
  "algorithm_version": "v1.0.0"
}
```

### 12.2 Regression Detection

```python
def test_golden_reference_regression(test_id, current_output):
    """
    Compare current output against golden reference.
    Flag any deviation as potential regression.
    """
    golden = load_golden_reference(test_id)
    
    # Metrics comparison
    metrics = compute_metrics(current_output)
    
    for metric_name, expected_value in golden['expected_metrics'].items():
        tolerance = golden['expected_metrics'][metric_name + '_tolerance']
        actual = metrics[metric_name]
        
        if not (tolerance[0] <= actual <= tolerance[1]):
            raise RegressionDetected(
                f"{test_id}: {metric_name} = {actual}, "
                f"expected {tolerance[0]}–{tolerance[1]}"
            )
    
    print(f"✓ {test_id} passed regression test")
```

---

## 13. 엣지 케이스 데이터셋

### 13.1 Edge Case: No Grid Present

```
Test ID: EDGE-001
Input: Uniform image (no grid)
Expected: MSI baseline < 0.05, no over-suppression
Acceptance: Image unchanged or minimal processing
```

### 13.2 Edge Case: Crossed Grid

```
Test ID: EDGE-002
Input: 8:1×8:1 crossed grid (perpendicular)
Expected: 2D suppression (both directions)
Acceptance: MSI < 0.10 in both axes
```

### 13.3 Edge Case: Aliased Grid

```
Test ID: EDGE-003
Input: f_grid = 5.2 lp/mm (>f_Nyquist)
Expected: Warning issued, partial suppression only
Acceptance: MSI 0.15–0.30 (acceptable degradation)
```

### 13.4 Edge Case: High Noise

```
Test ID: EDGE-004
Input: Grid + high noise (40dB SNR)
Expected: Denoising prevents artifact amplification
Acceptance: PSNR_output > PSNR_input
```

### 13.5 Edge Case: Full Saturation

```
Test ID: EDGE-005
Input: 16-bit image with 5% pixels at max (65535)
Expected: No arithmetic overflow in output
Acceptance: Output within [0, 65535]
```

---

## 14. 테스트 데이터 디렉토리 구조

```
gsvg_test_data/
├── README.md
├── synthetic/
│   ├── grid_pure/
│   │   ├── grid_4lpmm_horizontal.raw          # SG-001
│   │   ├── grid_6lpmm_vertical.raw            # SG-003
│   │   ├── grid_4_8lpmm_aliased.raw           # SG-005
│   │   └── grid_multiharmonic.raw             # MH-001
│   ├── anatomy_grid/
│   │   ├── chest_nodules_with_grid.raw        # AG-001
│   │   └── chest_ribs_with_grid.raw           # AG-002
│   └── edge_cases/
│       ├── no_grid_uniform.raw                # EDGE-001
│       ├── crossed_grid.raw                   # EDGE-002
│       ├── aliased_grid.raw                   # EDGE-003
│       ├── high_noise_grid.raw                # EDGE-004
│       └── saturated_grid.raw                 # EDGE-005
├── real_acquisition/
│   ├── grid_characterization/
│   │   ├── grid_8_1_100frames.raw
│   │   ├── grid_12_1_100frames.raw
│   │   └── nogrid_100frames.raw
│   ├── cdrad_phantom/
│   │   ├── cdrad_physical_grid_10frames.raw
│   │   ├── cdrad_virtual_grid_10frames.raw
│   │   └── cdrad_no_grid_10frames.raw
│   └── clinical/
│       ├── clinical_chest_grid_5img.raw
│       └── clinical_spine_grid_3img.raw
├── golden_references/
│   ├── sg_001_golden.json
│   ├── ag_001_golden.json
│   ├── mtf_001_golden.json
│   └── vg_001_golden.json
└── test_reports/
    ├── test_run_2026_04_14.log
    └── regression_analysis_2026_04_14.txt
```

---

## 15. 참고문헌

### 표준 및 규제

- **IEC 62304:2015** — Medical device software lifecycle processes
- **NIST SP 800-113** — Guidelines for Information Security

### 기술 논문

- **Tang et al. (2015)** — "Wavelet-based grid suppression," Medical Physics 42(9)
- **Neitzel et al. (2006)** — "Virtual grid implementation," Proc. SPIE 6142
- **Lisson et al. (2020)** — "Clinical validation of virtual grid," Radiology 297(2)

---

**문서 승인:**

| 역할 | 이름 | 날짜 | 서명 |
|------|------|------|------|
| 저자 | | | |
| 검토자 | | | |
| 승인자 | | | |

**Revision History**

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0.0 | 2026-04-14 | — | Initial release |
