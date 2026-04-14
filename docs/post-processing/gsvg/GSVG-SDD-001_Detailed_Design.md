# GSVG-SDD-001: Software Detailed Design

**Document ID:** GSVG-SDD-001  
**Version:** 1.0 | **Date:** 2026-04-03  
**IEC 62304 Clause:** 5.4 (Class B 선택 사항, 코드 품질을 위해 적용)  
**Safety Classification:** Class B

---

## 1. 모듈 구조

```
gsvg/
├── api/
│   ├── gsvg_api.h              # Public C API (extern "C")
│   └── gsvg_types.h            # Error codes, enums, config structs
├── pipeline/
│   ├── PipelineManager.h/cpp   # SI-001 orchestration
│   ├── GridDetector.h/cpp      # Grid presence auto-detection
│   └── ProcessingConfig.h/cpp  # JSON config parser
├── grid_suppression/
│   ├── DwtDecomposer.h/cpp     # 2D DWT forward/inverse
│   ├── GridlineDetector.h/cpp  # Sub-band energy analysis
│   └── BandStopFilter.h/cpp    # Gaussian band-stop in freq domain
├── virtual_grid/
│   ├── ThicknessEstimator.h/cpp
│   ├── SprCalculator.h/cpp
│   ├── ScatterEstimator.h/cpp  # Kernel LUT lookup + convolution
│   ├── LaplacianPyramid.h/cpp  # Decompose/process/reconstruct
│   └── Denoiser.h/cpp          # Wiener filter
├── common/
│   ├── DicomIO.h/cpp           # DICOM read/write via DCMTK
│   ├── FftUtils.h/cpp          # FFTW3 wrapper
│   ├── ImageBuffer.h/cpp       # RAII 16-bit image container
│   ├── Validator.h/cpp         # Input validation
│   └── ErrorHandler.h/cpp      # Error codes, logging
└── tests/
    ├── unit/                   # Google Test suites
    ├── integration/            # Pipeline-level tests
    └── test_data/              # Phantom images, reference data
```

---

## 2. 공개 API (gsvg_api.h)

```cpp
#pragma once
#include "gsvg_types.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Main processing entry point
// - On success: outputPixels filled with processed image, returns GSVG_OK
// - On failure: outputPixels filled with unmodified copy of input (SAFE-001/003)
GsvgErrorCode gsvg_process(
    const uint16_t* inputPixels,
    uint32_t width,
    uint32_t height,
    const GsvgConfig* config,
    uint16_t* outputPixels,
    char* errorMsg,
    size_t errorMsgLen
);

// Utility functions
const char* gsvg_version(void);
const char* gsvg_error_string(GsvgErrorCode code);

#ifdef __cplusplus
}
#endif
```

```cpp
// gsvg_types.h
#pragma once
#include <stdint.h>

typedef enum {
    GSVG_OK                        =  0,
    GSVG_ERR_INVALID_INPUT         = -1,
    GSVG_ERR_GRID_DETECTION_FAILED = -2,
    GSVG_ERR_SUPPRESSION_FAILED    = -3,
    GSVG_ERR_VIRTUAL_GRID_FAILED   = -4,
    GSVG_ERR_OUT_OF_MEMORY         = -5,
    GSVG_ERR_CONFIG_INVALID        = -6,
    GSVG_ERR_LUT_NOT_FOUND         = -7,
} GsvgErrorCode;

typedef enum {
    GSVG_MODE_AUTO           = 0,
    GSVG_MODE_GRID_SUPPRESS  = 1,
    GSVG_MODE_VIRTUAL_GRID   = 2,
} GsvgProcessingMode;

typedef struct {
    GsvgProcessingMode mode;
    float virtualGridRatio;    // 6.0, 8.0, 10.0, 12.0
    const char* configPath;    // JSON config file path (nullable)
    const char* lutPath;       // Scatter kernel LUT directory (nullable)
} GsvgConfig;
```

---

## 3. ImageBuffer (RAII 컨테이너)

```cpp
namespace gsvg {

class ImageBuffer {
public:
    ImageBuffer(uint32_t width, uint32_t height, uint16_t bitsAllocated = 16);
    ImageBuffer(const ImageBuffer& other);            // Deep copy
    ImageBuffer(ImageBuffer&& other) noexcept;        // Move
    ImageBuffer& operator=(const ImageBuffer& other);
    ImageBuffer& operator=(ImageBuffer&& other) noexcept;
    ~ImageBuffer();

    uint16_t& at(uint32_t x, uint32_t y);             // Bounds-checked
    const uint16_t& at(uint32_t x, uint32_t y) const;
    
    uint16_t* data() noexcept;
    const uint16_t* data() const noexcept;
    
    uint32_t width() const noexcept;
    uint32_t height() const noexcept;
    size_t sizeBytes() const noexcept;
    
    ImageBuffer deepCopy() const;  // Explicit deep copy for SAFE-001

private:
    std::unique_ptr<uint16_t[]> data_;
    uint32_t width_, height_;
    uint16_t bitsAllocated_;
};

} // namespace gsvg
```

---

## 4. DwtDecomposer

```cpp
namespace gsvg {

struct DwtSubBands {
    ImageBuffer LL, LH, HL, HH;
};

class DwtDecomposer {
public:
    enum class WaveletType { HAAR, DB4, DB6 };
    
    explicit DwtDecomposer(WaveletType type = WaveletType::DB4);
    
    DwtSubBands decompose(const ImageBuffer& input) const;
    ImageBuffer reconstruct(const DwtSubBands& bands) const;
    
    struct MultiLevelResult {
        std::vector<DwtSubBands> levels;
        int stopLevel;
        std::vector<bool> gridDetected;
    };
    
    MultiLevelResult decomposeMultiLevel(
        const ImageBuffer& input,
        int maxLevels,
        float energyThresholdSigma = 3.0f
    ) const;

private:
    WaveletType type_;
    std::vector<float> lo_, hi_;  // Filter coefficients
    void initFilters();
};

} // namespace gsvg
```

---

## 5. LaplacianPyramid

```cpp
namespace gsvg {

struct LaplacianLevel {
    ImageBuffer gaussian;   // g_k
    ImageBuffer laplacian;  // L_k (detail)
};

class LaplacianPyramid {
public:
    struct Config {
        float gaussianSigma = 1.0f;
        int kernelSize = 5;
        int numLevels = 0;  // 0 = auto
    };
    
    explicit LaplacianPyramid(const Config& config = {});
    
    std::vector<LaplacianLevel> decompose(const ImageBuffer& input) const;
    
    void processLevels(
        std::vector<LaplacianLevel>& levels,
        float sprCorrectionFactor,
        float contrastGain,
        float noiseSigma
    ) const;
    
    ImageBuffer reconstruct(const std::vector<LaplacianLevel>& levels) const;

private:
    Config config_;
    ImageBuffer gaussianBlur(const ImageBuffer& input) const;
    ImageBuffer downsample2x(const ImageBuffer& input) const;
    ImageBuffer upsample2x(const ImageBuffer& input) const;
};

} // namespace gsvg
```

---

## 6. ScatterEstimator

```cpp
namespace gsvg {

struct ScatterKernel {
    float a[4];      // 4-Gaussian amplitudes
    float sigma[4];  // 4-Gaussian widths
    // S(r) = Σ a_i × exp(-r² / (2×σ_i²))
};

class ScatterEstimator {
public:
    explicit ScatterEstimator(const std::string& lutDirectory);
    
    ImageBuffer estimateScatter(
        const ImageBuffer& input,
        float thicknessCm,
        float kvp,
        float fieldSizeCm,
        float airGapCm
    ) const;
    
    // SAFE-004: clamping applied internally
    ImageBuffer subtractScatter(
        const ImageBuffer& total,
        const ImageBuffer& scatter
    ) const;

private:
    using LutKey = std::tuple<int,int,int,int>;
    std::map<LutKey, ScatterKernel> lut_;
    
    ScatterKernel interpolate(
        float t, float kvp, float fov, float gap
    ) const;
    
    ImageBuffer convolve(
        const ImageBuffer& primary,
        const ScatterKernel& kernel
    ) const;
    
    static constexpr float MAX_SPR = 3.0f;  // Physical maximum clamping
};

} // namespace gsvg
```

---

## 7. 핵심 방정식 구현

### 7.1 SPR 계산

```
SPR(t, kVp, FOV) = a(kVp) × t^b(kVp) × FOV^c(kVp)

범위 조정 (SAFE-004): SPR = min(SPR, MAX_SPR=3.0)
```

### 7.2 범위 조정을 포함한 Scatter 차감

```
I_primary(x,y) = max(0, I_total(x,y) - S(x,y))
I_primary(x,y) = min(I_primary(x,y), 65535)    // SAFE-005
```

### 7.3 Gaussian Band-Stop 필터

```
H(u,v) = 1 - exp(-((u-u_g)² + (v-v_g)²) / (2σ_f²))

여기서 (u_g, v_g) = 검출된 grid 주파수, σ_f = 1.5 px
```

### 7.4 Wiener 노이즈 제거

```
G(u,v) = |H(u,v)|² / (|H(u,v)|² + σ_n²/σ_s²)
```

---

## 8. 스레드 안전성

- `gsvg_process()`는 서로 다른 input에 대해 동시 호출 가능 (재진입 가능)
- 각 호출은 독립적인 `ImageBuffer` 인스턴스 사용
- Scatter kernel LUT는 read-only로 공유 (로드 후 불변)
- FFTW3 plan은 thread-local로 생성 (`fftw_make_planner_thread_safe()`)

---

## 개정 이력

| 버전 | 날짜 | 작성자 | 설명 |
|---------|------|--------|-------------|
| 1.0 | 2026-04-03 | — | 초판 |
