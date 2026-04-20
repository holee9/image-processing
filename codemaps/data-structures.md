# XPE Data Structures Reference

## Core Data Types

### XpeImage

```cpp
typedef struct {
    uint32_t width;        // Image width in pixels
    uint32_t height;       // Image height in pixels
    uint32_t stride;       // Bytes per row (>= width * bytes_per_pixel)
    XpePixelFormat format; // Pixel format
    void* data;            // Pointer to pixel data
} XpeImage;
```

**Properties**:
- `stride` may be larger than `width * bytes_per_pixel` (alignment padding)
- `data` must be allocated with `xpe_alloc_image()` for proper alignment
- Row i starts at `data + i * stride`

**Pixel Formats**:
```cpp
typedef enum {
    XPE_PIX_UNKNOWN = 0,
    XPE_PIX_MONO8   = 1,   // 8-bit grayscale
    XPE_PIX_MONO16  = 2,   // 16-bit grayscale
    XPE_PIX_RGB24   = 3,   // 24-bit RGB (8 bits per channel)
    XPE_PIX_RGB32   = 4,   // 32-bit RGB (8 bits per channel + padding)
    XPE_PIX_RGBA32  = 5    // 32-bit RGBA (8 bits per channel)
} XpePixelFormat;
```

**Bytes Per Pixel**:
- MONO8: 1 byte
- MONO16: 2 bytes
- RGB24: 3 bytes
- RGB32: 4 bytes
- RGBA32: 4 bytes

### XpeErrorCode

```cpp
typedef enum {
    // Success and status codes
    XPE_OK = 0,
    XPE_STATUS_NO_EVENT = 1,

    // Error codes (negative)
    XPE_ERR_INVALID_INPUT = -1,
    XPE_ERR_OUT_OF_MEMORY = -2,
    XPE_ERR_CONFIG_INVALID = -3,
    XPE_ERR_IO_FAILED = -4,
    XPE_ERR_NOT_INITIALIZED = -5,
    XPE_ERR_NOT_SUPPORTED = -6,
    XPE_ERR_TIMEOUT = -7,
    XPE_ERR_BUFFER_TOO_SMALL = -8,
    XPE_ERR calibration_EXPIRED = -9,
    XPE_ERR_DETECTION_FAILED = -10,

    // Module-specific errors
    XPE_ERR_PREPROCESS_GAIN_MISSING = -100,
    XPE_ERR_PREPROCESS_OFFSET_MISSING = -101,
    XPE_ERR_PREPROCESS_DEFECT_MAP_MISSING = -102,
    XPE_ERR_ENHANCE_CLAHE_FAILED = -200,
    XPE_ERR_DICOM_PARSE_FAILED = -300,
    XPE_ERR_DICOM_WRITE_FAILED = -301
} XpeErrorCode;
```

**Error String Conversion**:
```cpp
const char* xpe_error_string(XpeErrorCode code);
```

### XpeAlert

```cpp
typedef enum {
    XPE_ALERT_INFO = 0,
    XPE_ALERT_WARNING = 1,
    XPE_ALERT_ERROR = 2,
    XPE_ALERT_CRITICAL = 3
} XpeAlertSeverity;

typedef struct {
    XpeAlertSeverity severity;
    char message[512];
    uint64_t timestamp_ms;
    const char* source_module;  // Module that generated the alert
} XpeAlert;
```

**Alert Management**:
```cpp
XPE_API int xpe_get_pending_alert_count(void);
XPE_API XpeErrorCode xpe_get_pending_alert(int index, XpeAlert* alert);
XPE_API void xpe_clear_alerts(void);
```

## Configuration Structures

### JSON Configuration Schema

**Root Config**:
```json
{
  "log_level": 2,
  "log_file": "xpe.log",
  "threads": 4,
  "aed": {
    "trigger_threshold_adu": 500,
    "settle_time_ms": 100,
    "min_exposure_ms": 5,
    "max_exposure_ms": 5000
  }
}
```

**Module Config Example** (Enhance Basic):
```json
{
  "clahe_enable": true,
  "clahe_clip_limit": 2.0,
  "clahe_grid_size": 8,
  "noise_reduce_enable": true,
  "noise_reduce_sigma_spatial": 2.0,
  "noise_reduce_sigma_range": 50.0
}
```

## Processing Parameters

### CLAHE Parameters

```json
{
  "clahe_enable": true,
  "clahe_clip_limit": 2.0,      // Contrast limiting threshold (1.0-10.0)
  "clahe_grid_size": 8,         // Grid size for histogram equalization (2-16)
  "clahe_brightness_bias": 0.0  // Brightness adjustment (-1.0 to 1.0)
}
```

### Noise Reduction Parameters

```json
{
  "noise_reduce_enable": true,
  "noise_reduce_sigma_spatial": 2.0,  // Spatial sigma (0.5-10.0)
  "noise_reduce_sigma_range": 50.0,   // Range sigma (1.0-100.0)
  "noise_reduce_iterations": 1        // Filter iterations (1-5)
}
```

### Window/Level Parameters

```json
{
  "window_center": 128,
  "window_width": 256,
  "voi_lut_function": "LINEAR"  // LINEAR or SIGMOID
}
```

## Calibration Data Structures

### XCAL Format (XML-based)

```xml
<XCalibration>
  <Header>
    <Version>1.0</Version>
    <Timestamp>2026-04-19T10:30:00Z</Timestamp>
    <DetectorId>DETECTOR_001</DetectorId>
  </Header>

  <GainMap>
    <Width>2048</Width>
    <Height>2048</Height>
    <DataFormat>FLOAT32</DataFormat>
    <Data encoding="base64">...</Data>
  </GainMap>

  <OffsetMap>
    <Width>2048</Width>
    <Height>2048</Height>
    <DataFormat>UINT16</DataFormat>
    <Data encoding="base64">...</Data>
  </OffsetMap>

  <DefectMap>
    <PixelCount>42</PixelCount>
    <DefectPixels>
      <Pixel x="1024" y="768" type="dead" />
      <Pixel x="1536" y="512" type="hot" />
      <!-- ... more pixels ... -->
    </DefectPixels>
  </DefectMap>
</XCalibration>
```

### Calibration Expiry

```cpp
typedef struct {
    uint32_t gain_map_days;      // Gain map validity period (days)
    uint32_t offset_map_days;    // Offset map validity period (days)
    uint32_t defect_map_days;    // Defect map validity period (days)
    time_t last_calibration;     // Last calibration timestamp
} XpeCalibrationValidity;
```

## Display Structures

### Modality LUT

```cpp
typedef struct {
    double rescale_slope;       // DICOM rescale slope
    double rescale_intercept;   // DICOM rescale intercept
} XpeModalityLUT;
```

**Transformation**: `output = input * rescale_slope + rescale_intercept`

### Presentation LUT

```cpp
typedef enum {
    XPE_PRESENTATION_LINEAR = 0,
    XPE_PRESENTATION_GAMMA = 1,
    XPE_PRESENTATION_SIGMOID = 2
} XpePresentationLUTType;

typedef struct {
    XpePresentationLUTType type;
    double gamma;               // Gamma value (if type == GAMMA)
    double sigmoid_a;           // Sigmoid parameters (if type == SIGMOID)
    double sigmoid_b;
} XpePresentationLUT;
```

### VOI LUT (Window/Level)

```cpp
typedef enum {
    XPE_VOI_LINEAR = 0,
    XPE_VOI_SIGMOID = 1,
    XPE_VOI_LINEAR_EXACT = 2
} XpeVOILUTFunction;

typedef struct {
    double window_center;       // Window center
    double window_width;        // Window width
    XpeVOILUTFunction function; // VOI function type
} XpeVOILUT;
```

## DICOM Structures

### DICOM Tags (Key Attributes)

```cpp
// DICOM tags used in XPE
#define DICOM_TAG_SOP_CLASS_UID           0x00080016
#define DICOM_TAG_SOP_INSTANCE_UID        0x00080018
#define DICOM_TAG_TRANSFER_SYNTAX_UID     0x00080020
#define DICOM_TAG_ROWS                    0x00280010
#define DICOM_TAG_COLUMNS                 0x00280011
#define DICOM_TAG_BITS_ALLOCATED          0x00280100
#define DICOM_TAG_BITS_STORED             0x00280101
#define DICOM_TAG_HIGH_BIT                0x00280102
#define DICOM_TAG_PIXEL_REPRESENTATION    0x00280103
#define DICOM_TAG_PIXEL_DATA              0x7FE00010
#define DICOM_TAG_RESCALE_SLOPE           0x00281053
#define DICOM_TAG_RESCALE_INTERCEPT       0x00281052
```

## Internal Structures (Not in C API)

### XpeImageInternal (C++ only)

```cpp
// Internal C++ class (not exported)
class XpeImageInternal {
public:
    std::shared_ptr<void> data_;     // Reference-counted data
    uint32_t width_;
    uint32_t height_;
    uint32_t stride_;
    XpePixelFormat format_;

    // RAII destructor
    ~XpeImageInternal();
};
```

### Module State (C++ only)

```cpp
// Per-module state (not exported)
struct XpeEnhanceBasicState {
    CLAHEProcessor* clahe_;
    NoiseReducer* noise_reducer_;
    LogTransformer* log_transform_;
    bool initialized_;
};
```

## Memory Alignment Requirements

**SIMD Alignment**:
- Image buffers: 32-byte aligned (AVX2)
- Gain/Offset maps: 32-byte aligned
- Intermediate buffers: 32-byte aligned

**Allocation via**:
```cpp
XPE_API XpeErrorCode xpe_alloc_image(
    uint32_t width,
    uint32_t height,
    XpePixelFormat format,
    XpeImage** imageOut
);
```

## Data Validation Rules

### Image Validation

**Preconditions**:
- `width > 0 && height > 0`
- `stride >= width * bytes_per_pixel`
- `data != NULL` (unless processing will allocate)
- `format` is valid enum value

**Postconditions** (after processing):
- Output image dimensions match input (unless documented otherwise)
- Output image format matches input (unless documented otherwise)
- Output image data is properly aligned

### Parameter Validation

**Range Checks**:
- CLAHE clip limit: [1.0, 10.0]
- CLAHE grid size: [2, 16] (power of 2)
- Window width: [1, 65535]
- Window center: [-32768, 32767]

**Type Checks**:
- JSON values must match expected types
- Enum values must be valid
- Pointer arguments must not be NULL (unless documented)

---

**Last Updated**: 2026-04-19
**Specification Version**: 0.1.0
