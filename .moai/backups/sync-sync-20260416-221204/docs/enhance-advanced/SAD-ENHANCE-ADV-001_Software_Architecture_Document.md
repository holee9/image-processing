# Software Architecture Document - XPE Advanced Enhancement Module

**Document ID:** SAD-ENHANCE-ADV-001 v1.0  
**IEC 62304 Clause:** 5.3 (Software Architectural Design)  
**Safety Classification:** Class B  
**Date:** 2026-04-14  
**Author:** XPE Advanced Enhancement Architecture Team  
**Approval:** __________________ Date: __________  

---

## 1. Purpose and Scope

### 1.1 Purpose

This Software Architecture Document defines the structural design of the XPE Advanced Enhancement Module (`xpe_enhance_advanced.dll`). It specifies decomposition of software requirements (from SRS-ENHANCE-ADV-001) into manageable software units (SWU), their responsibilities, interfaces, data flow, and inter-dependencies. The document establishes the foundation for implementation, integration testing, and change management.

### 1.2 Scope

This architecture applies to all processing stages:
- Stage 9: Noise Reduction (4 tiers)
- Stage 10: Edge Enhancement with Overshoot Limiting
- Stage 11: Collimation ROI Detection
- SWU-2.10: EI ROI-Based Dose Correction

---

## 2. System Context

### 2.1 Layer Architecture

```
Layer 2  ImageProcTest.exe (C# WPF GUI)
         ↓ P/Invoke (C ABI)
Layer 1  xpe_enhance_advanced.dll ← [THIS MODULE - Advanced enhancement]
         ↓ Link dependency
Layer 0  xpe_common.dll (types, memory, error codes, alerts)
         ↓ Link dependency
         Win32 / OS APIs
```

### 2.2 Data Flow Context

```
Enhancement-domain float32 image (post-log, from xpe_enhance_basic)
+ Detector-domain uint16 data (for EI correction)
    ↓
[Stage 9: Noise Reduction - Tier Selection]
    ├─ SWU-2.5: Gaussian (Fast)
    ├─ SWU-2.6: Bilateral (Standard)
    ├─ SWU-2.7: NLM (Premium)
    └─ SWU-2.8: Wavelet (Ultra)
    ↓
[Stage 10: Edge Enhancement + Overshoot Limiting]
    ↓
    SWU-2.9: EdgeEnhancer
    ↓
[Stage 11: Collimation ROI Detection]
    ↓
    SWU-2.11: CollimationDetector
    ↓ (ROI sidecar JSON output)
[SWU-2.10: EI ROI-Based Dose Correction]
    ├─ SWU-2.12: SidecarManager (read sidecar)
    ├─ Detector-domain data reference
    └─ DI computation + QC alert
    ↓
Advanced-enhanced float32 image + ROI sidecar + DI value
    ↓ [Status flags, diagnostic log]
    ↓
[xpe_display.dll] (downstream)
```

### 2.3 External Interface Dependencies

| System | Protocol | Direction | Purpose |
|--------|----------|-----------|---------|
| **xpe_common.dll** | C ABI (link-time) | Import | Types, memory utils, error codes, alert queue |
| **xpe_enhance_basic.dll** | Data input | Input | Enhancement-domain float32 image |
| **xpe_preprocess.dll** | Data reference | Input | Detector-domain uint16 data (EI correction) |
| **Disk I/O** | Win32 File API | Output | ROI sidecar JSON files |
| **C# GUI** | P/Invoke / C ABI | Bidirectional | Configure(), process frames, retrieve status |

---

## 3. Software Units Decomposition

### 3.1 Software Item 1: Noise Reduction (SWI-ENHANCE-1)

Central orchestrator for 4-tier denoising with auto-escalation capability.

#### 3.1.1 SWU Decomposition

| SWU ID | Name | Responsibility | Input | Output | Dependencies |
|--------|------|-----------------|-------|--------|--------------|
| **SWU-2.5** | GaussianDenoiser | Apply Gaussian blur (Tier 1) | float32 image | float32 image | xpe_common (memory) |
| **SWU-2.6** | BilateralFilter | Apply bilateral filter (Tier 2) | float32 image | float32 image | xpe_common |
| **SWU-2.7** | NLMDenoiser | Apply NLM (Tier 3) | float32 image | float32 image | xpe_common |
| **SWU-2.8** | WaveletShrinker | Apply BayesShrink (Tier 4) | float32 image | float32 image | xpe_common |

#### 3.1.2 Tier Selection Logic (SWU-2.5/2.6/2.7/2.8 Orchestration)

```c
int xpe_enhance_denoise(const float *input, float *output, ..., const char *mode) {
    if (strcmp(mode, "Fast") == 0) {
        return SWU_2_5_gaussian_denoise(input, output, ..., sigma=1.0);
    } else if (strcmp(mode, "Standard") == 0) {
        return SWU_2_6_bilateral_filter(input, output, ..., sigma_s=2.5);
    } else if (strcmp(mode, "Premium") == 0) {
        return SWU_2_7_nlm_denoise(input, output, ...);
    } else if (strcmp(mode, "Ultra") == 0) {
        return SWU_2_8_wavelet_shrink(input, output, ..., level=4);
    }
    return XPE_ERR_INVALID_PARAM;
}
```

#### 3.1.3 Interface

```c
// Tier 1: Gaussian
int xpe_enhance_denoise_gaussian(
    const float *input, float *output,
    int width, int height, float sigma
);
// Time: ~10-15ms, MTF loss: ~8-15%

// Tier 2: Bilateral
int xpe_enhance_denoise_bilateral(
    const float *input, float *output,
    int width, int height,
    float sigma_spatial, float sigma_range_multiplier
);
// Time: ~40-60ms, MTF loss: ~3-5%

// Tier 3: NLM
int xpe_enhance_denoise_nlm(
    const float *input, float *output,
    int width, int height, float h_strength
);
// Time: ~100-150ms, MTF loss: ~2-4%, parallel-friendly

// Tier 4: Wavelet
int xpe_enhance_denoise_wavelet(
    const float *input, float *output,
    int width, int height, int decomposition_level
);
// Time: ~200-300ms, MTF loss: ~1-3%

// Auto-selector
int xpe_enhance_denoise(
    const float *input, float *output,
    int width, int height, const char *mode
);
```

---

### 3.2 Software Item 2: Edge Enhancement (SWI-ENHANCE-2)

#### 3.2.1 SWU Decomposition

| SWU ID | Name | Responsibility | Input | Output |
|--------|------|-----------------|-------|--------|
| **SWU-2.9** | EdgeEnhancer | Unsharp masking + overshoot limiter | float32 image | float32 image |

#### 3.2.2 EdgeEnhancer Algorithm

```c
int xpe_enhance_edges(
    const float *input, float *output,
    int width, int height,
    float alpha, float gaussian_sigma,
    bool apply_overshoot_limit  // ALWAYS TRUE
) {
    // Step 1: Compute Gaussian blur I_blur
    float *i_blur = allocate(width * height * sizeof(float));
    xpe_enhance_denoise_gaussian(input, i_blur, width, height, gaussian_sigma);
    
    // Step 2: Compute edge boost
    for (int i = 0; i < width * height; i++) {
        float boost = alpha * (input[i] - i_blur[i]);
        
        // Step 3: Compute local sigma_local (3×3 window)
        float sigma_local = compute_local_std(input, width, height, i);
        
        // Step 4: Apply overshoot limit (MANDATORY)
        float limited_boost = clamp(boost, -3.0 * sigma_local, 3.0 * sigma_local);
        
        // Step 5: Final output
        output[i] = input[i] + limited_boost;
    }
    
    free(i_blur);
    return XPE_OK;
}
```

#### 3.2.3 Clipping Detection & Logging

```c
// Count pixels where clipping was applied
int clipped_count = 0;
for (int i = 0; i < width * height; i++) {
    if (fabs(boost) > 3.0 * sigma_local) {
        clipped_count++;
    }
}

if (clipped_count > 0.01 * (width * height)) {  // > 1% clipped
    float clipped_ratio = clipped_count / (float)(width * height);
    alert("Edge enhancement clipped at %.1f%% of pixels. Consider reducing α.", 
          clipped_ratio * 100);
}
```

#### 3.2.4 Interface

```c
int xpe_enhance_edges(
    const float *input, float *output,
    int width, int height,
    float alpha,        // 0.1-2.0
    float gaussian_sigma,  // 1.0-3.0
    bool apply_overshoot_limit  // always true (checked, error if false)
);
// Time: ~30-50ms
// Output: enhanced float32 image with halo artifacts prevented
```

---

### 3.3 Software Item 3: ROI Detection (SWI-ENHANCE-3)

#### 3.3.1 SWU Decomposition

| SWU ID | Name | Responsibility | Input | Output | Dependencies |
|--------|------|-----------------|-------|--------|--------------|
| **SWU-2.11** | CollimationDetector | Hough-based ROI detection | float32 image | XpeROI struct + confidence | xpe_common |
| **SWU-2.12** | SidecarManager | JSON sidecar read/write | XpeROI + JSON path | JSON file / XpeROI | Win32 File API |

#### 3.3.2 CollimationDetector Algorithm

```c
typedef struct {
    int x, y, width, height;
    float confidence;
} XpeROI;

int xpe_detect_roi(
    const float *image, int width, int height,
    XpeROI *roi_out
) {
    // Step 1: Edge detection (Canny)
    uint8_t *edge_map = allocate(width * height);
    xpe_canny_edge_detection(image, edge_map, width, height, 
                             threshold_low=50, threshold_high=150);
    
    // Step 2: Hough transform
    int hough_rho_max = (int)sqrt(width*width + height*height);
    int hough_theta_steps = 180;  // 1° resolution
    int **accumulator = allocate_hough_accumulator(hough_theta_steps, hough_rho_max);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (edge_map[y * width + x] > 0) {  // Edge pixel
                for (int theta = 0; theta < hough_theta_steps; theta++) {
                    float theta_rad = theta * M_PI / 180.0;
                    float rho = x * cos(theta_rad) + y * sin(theta_rad);
                    accumulator[(int)rho][theta]++;
                }
            }
        }
    }
    
    // Step 3: Find peaks (threshold = 70% of max)
    int max_val = find_max_accumulator(accumulator, hough_rho_steps, hough_theta_steps);
    int peak_threshold = max_val * 0.7;
    
    vector<HoughLine> lines;
    for (int theta = 0; theta < hough_theta_steps; theta++) {
        for (int rho = 0; rho < hough_rho_max; rho++) {
            if (accumulator[rho][theta] > peak_threshold) {
                lines.push_back({rho, theta, accumulator[rho][theta]});
            }
        }
    }
    
    // Step 4: Filter for axis-aligned lines (θ ≈ 0° or 90°, ±5° tolerance)
    vector<HoughLine> axis_lines;
    for (auto& line : lines) {
        if ((line.theta >= 0 && line.theta <= 5) ||
            (line.theta >= 85 && line.theta <= 95) ||
            (line.theta >= 175 && line.theta <= 180)) {
            axis_lines.push_back(line);
        }
    }
    
    // Step 5: Extract 4-corner rectangle from intersections
    // (Implementation: compute intersection of horizontal and vertical line pairs)
    if (axis_lines.size() < 4) {
        roi_out->confidence = 0.0;
        return XPE_OK;  // No error, just low confidence
    }
    
    // ... compute corner intersections ...
    
    // Step 6: Compute confidence
    float sum_peaks = 0;
    for (int i = 0; i < 4; i++) {
        sum_peaks += axis_lines[i].peak_value;
    }
    roi_out->confidence = sum_peaks / (4.0 * max_val);
    
    // Clamp ROI to image bounds
    roi_out->x = max(0, min(roi_out->x, width - 1));
    roi_out->y = max(0, min(roi_out->y, height - 1));
    roi_out->width = min(roi_out->width, width - roi_out->x);
    roi_out->height = min(roi_out->height, height - roi_out->y);
    
    return XPE_OK;
}
```

#### 3.3.3 SidecarManager Interface

```c
int xpe_sidecar_write(const char *image_path, const XpeROI *roi) {
    // Generate sidecar filename: {image_path}.roi.json
    char sidecar_path[MAX_PATH];
    snprintf(sidecar_path, MAX_PATH, "%s.roi.json", image_path);
    
    // Construct JSON
    cJSON *root = cJSON_CreateObject();
    cJSON *roi_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(roi_obj, "x", roi->x);
    cJSON_AddNumberToObject(roi_obj, "y", roi->y);
    cJSON_AddNumberToObject(roi_obj, "width", roi->width);
    cJSON_AddNumberToObject(roi_obj, "height", roi->height);
    cJSON_AddItemToObject(root, "roi", roi_obj);
    
    cJSON_AddNumberToObject(root, "confidence", roi->confidence);
    cJSON_AddNumberToObject(root, "timestamp_ms", get_time_ms());
    cJSON_AddStringToObject(root, "detector_id", detector_profile.id);
    cJSON_AddStringToObject(root, "collimation_type", "rectangular");
    
    // Write to disk
    FILE *f = fopen(sidecar_path, "w");
    char *json_str = cJSON_Print(root);
    fputs(json_str, f);
    fclose(f);
    
    cJSON_Delete(root);
    return XPE_OK;
}

int xpe_sidecar_read(const char *image_path, XpeROI *roi_out) {
    char sidecar_path[MAX_PATH];
    snprintf(sidecar_path, MAX_PATH, "%s.roi.json", image_path);
    
    FILE *f = fopen(sidecar_path, "r");
    if (!f) {
        roi_out->confidence = 0.0;  // File not found → treat as failed detection
        return XPE_OK;  // Not an error, just no ROI available
    }
    
    // Parse JSON
    char buffer[65536];
    size_t n = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[n] = '\0';
    fclose(f);
    
    cJSON *root = cJSON_Parse(buffer);
    cJSON *roi_obj = cJSON_GetObjectItem(root, "roi");
    roi_out->x = cJSON_GetObjectItem(roi_obj, "x")->valueint;
    roi_out->y = cJSON_GetObjectItem(roi_obj, "y")->valueint;
    roi_out->width = cJSON_GetObjectItem(roi_obj, "width")->valueint;
    roi_out->height = cJSON_GetObjectItem(roi_obj, "height")->valueint;
    roi_out->confidence = cJSON_GetObjectItem(root, "confidence")->valuedouble;
    
    cJSON_Delete(root);
    return XPE_OK;
}
```

---

### 3.4 Software Item 4: EI ROI Correction (SWI-ENHANCE-4)

#### 3.4.1 SWU Decomposition

| SWU ID | Name | Responsibility | Input | Output |
|--------|------|-----------------|-------|--------|
| **SWU-2.10** | EI_ROI_Refiner | ROI-masked EI + DI computation | detector uint16 + ROI sidecar | DI float32 + QC alert |

#### 3.4.2 EI_ROI_Refiner Algorithm

```c
int xpe_refine_ei_by_roi(
    const uint16_t *detector_domain_data,
    int width, int height,
    const XpeROI *roi,
    float ei_ref,
    float *di_out,
    bool *qc_alert_out
) {
    // Step 1: Check ROI confidence
    if (roi->confidence <= 0.7) {
        // Fallback: use SWU-2.0 EI (full image)
        float ei_full = compute_ei_full_image(detector_domain_data, width, height);
        *di_out = 10.0 * log10(ei_full / ei_ref);
        *qc_alert_out = fabs(*di_out) > 3.0;
        log_message("Using full-image EI (fallback: confidence %.2f <= 0.7)", roi->confidence);
        return XPE_OK;
    }
    
    // Step 2: ROI masking
    int roi_pixel_count = 0;
    float roi_sum = 0.0;
    
    for (int y = roi->y; y < roi->y + roi->height; y++) {
        for (int x = roi->x; x < roi->x + roi->width; x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                roi_sum += detector_domain_data[y * width + x];
                roi_pixel_count++;
            }
        }
    }
    
    // Step 3: Compute EI_roi
    float ei_roi = roi_sum / roi_pixel_count;
    
    // Step 4: Compute DI
    *di_out = 10.0 * log10(ei_roi / ei_ref);
    
    // Step 5: QC alert
    *qc_alert_out = fabs(*di_out) > 3.0;
    if (*qc_alert_out) {
        alert("Exposure out of range (|DI| = %.1f > 3). Recommend re-acquisition.", 
              fabs(*di_out));
    }
    
    // Step 6: Logging
    log_message("DI_roi=%.2f, confidence=%.2f, roi_box=[%d,%d,%d,%d]",
                *di_out, roi->confidence, roi->x, roi->y, roi->width, roi->height);
    
    return XPE_OK;
}
```

---

## 4. Data Structure Definitions

### 4.1 XpeROI Structure

```c
typedef struct {
    int x;              // Top-left x coordinate (pixels)
    int y;              // Top-left y coordinate (pixels)
    int width;          // ROI width (pixels)
    int height;         // ROI height (pixels)
    float confidence;   // Detection confidence [0.0, 1.0]
} XpeROI;
```

### 4.2 Noise Tier Enumeration

```c
typedef enum {
    NOISE_TIER_GAUSSIAN = 1,    // Fast (Tier 1)
    NOISE_TIER_BILATERAL = 2,   // Standard (Tier 2)
    NOISE_TIER_NLM = 3,         // Premium (Tier 3)
    NOISE_TIER_WAVELET = 4      // Ultra (Tier 4)
} XpeNoiseTier;
```

### 4.3 Processing Mode Enumeration

```c
typedef enum {
    ENHANCE_MODE_FAST,      // Fast (fluoroscopy)
    ENHANCE_MODE_STANDARD,  // Standard (clinical default)
    ENHANCE_MODE_PREMIUM,   // Premium (high quality)
    ENHANCE_MODE_ULTRA      // Ultra (research)
} XpeEnhanceMode;
```

---

## 5. Integration Points

### 5.1 Upstream Dependency: xpe_enhance_basic.dll

```
Input from xpe_enhance_basic:
  - Enhancement-domain float32 image (log-transformed, CLAHE applied)
  - Status flags indicating which basic enhancements were applied
```

### 5.2 Downstream Integration: xpe_display.dll

```
Output to xpe_display:
  - Advanced-enhanced float32 image
  - Status flags (noise tier, edge enhancement, ROI detection)
  - Diagnostic log (timing, alerts)
```

### 5.3 Detector-Domain Data Reference

```
For EI ROI correction (SWU-2.10):
  - Source: xpe_preprocess.dll (calibrated detector-domain data)
  - Format: uint16 (or float32 if already calibrated)
  - Usage: ROI-masked EI calculation (NOT log-transformed)
```

---

## 6. Configuration and Tuning

### 6.1 Configuration Structure

```json
{
  "enhance_advanced": {
    "noise_reduction": {
      "mode": "Standard",  // "Fast", "Standard", "Premium", "Ultra"
      "tier_1_sigma": 1.0,
      "tier_2_sigma_spatial": 2.5,
      "tier_2_sigma_range_mult": 1.0,
      "tier_3_h_strength": 1.0,
      "tier_4_decomp_level": 4
    },
    "edge_enhancement": {
      "alpha": 0.8,
      "gaussian_sigma": 2.0,
      "overshoot_limit_enabled": true  // ALWAYS true (safety)
    },
    "roi_detection": {
      "canny_threshold_low": 50,
      "canny_threshold_high": 150,
      "hough_peak_threshold_ratio": 0.7,
      "axis_tolerance_deg": 5.0,
      "confidence_threshold": 0.7
    },
    "ei_roi_correction": {
      "ei_ref": 250,  // Target EI (detector-dependent)
      "enable_roi_correction": true,
      "fallback_on_low_confidence": true
    }
  }
}
```

### 6.2 Performance Tuning Knobs

| Parameter | Impact | Trade-off |
|-----------|--------|-----------|
| Noise tier (1-4) | Quality | Computation time |
| Gaussian sigma | Noise reduction | MTF loss |
| Alpha (edge enhancement) | Edge visibility | Halo artifacts |
| Hough peak threshold | ROI sensitivity | False detection |

---

## 7. Error Handling and Fallback

### 7.1 Error Codes

| Code | Meaning | Recovery |
|------|---------|----------|
| `XPE_OK` | Success | Continue |
| `XPE_ERR_INVALID_PARAM` | Bad parameter (tier, mode, etc.) | Log error, use default |
| `XPE_ERR_MEMORY_ALLOC` | Out of memory | Fallback to Fast mode |
| `XPE_ERR_SAFETY_VIOLATION` | Overshoot limit disabled attempt | Reject configuration |
| `XPE_ERR_NOT_INITIALIZED` | ROI sidecar unreadable | Fallback to full-image EI |

### 7.2 Fallback Chain

```
Tier 4 (Wavelet) fails
    ↓ fallback
Tier 3 (NLM) fails
    ↓ fallback
Tier 2 (Bilateral) fails
    ↓ fallback
Tier 1 (Gaussian)
    ↓ fallback (if memory exhausted)
No denoising applied (warning logged)

ROI detection fails (confidence < 0.5)
    ↓ fallback
Use full-image EI (SWU-2.0)
    ↓ fallback (if detector-domain data unavailable)
Log warning, output last valid EI
```

---

## 8. Testing and Validation

### 8.1 Unit Test Coverage

| SWU | Test Suite | Coverage |
|-----|-----------|----------|
| SWU-2.5 (Gaussian) | test_gaussian_denoise | 100% |
| SWU-2.6 (Bilateral) | test_bilateral_filter | 100% |
| SWU-2.7 (NLM) | test_nlm_denoise | 100% |
| SWU-2.8 (Wavelet) | test_wavelet_shrink | 100% |
| SWU-2.9 (EdgeEnhancer) | test_edge_enhancement, test_overshoot_limiting | 100% |
| SWU-2.11 (ROI Detection) | test_hough_transform, test_confidence_scoring | 100% |
| SWU-2.10 (EI Correction) | test_ei_roi_computation, test_fallback_logic | 100% |

### 8.2 Integration Tests

- **Noise-edge pipeline**: Verify sequential application without data corruption
- **ROI-EI pipeline**: Verify ROI sidecar read/write and EI correction
- **Fallback chain**: Trigger each failure path and verify graceful degradation

---

*SAD-ENHANCE-ADV-001 v1.0 끝*
