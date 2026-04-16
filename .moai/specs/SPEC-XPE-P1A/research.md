# Research Report: SPEC-XPE-P1A Pre-processing Module

**Document ID**: SPEC-XPE-P1A-RESEARCH
**Version**: 1.0.0
**Date**: 2026-04-16
**Status**: Complete
**Researcher**: Explore subagent (MoAI)

---

## Executive Summary

Comprehensive codebase analysis for creating SPEC-XPE-P1A (Pre-processing module). The XPE project follows a 3-Layer architecture with xpe_common.dll as the foundation. The preprocess module is currently empty (greenfield development opportunity) and requires implementation of 9 SWUs including Gain/Offset correction, Bad Pixel correction, and advanced algorithms.

---

## Architecture Analysis

### Foundation: xpe_common.dll Implementation

**File Path**: `modules/common/src/xpe_common.cpp`

**Key Findings:**
- **18 API functions** exported with C linkage extern "C"
- **Pack=8 structs** verified via static_assert (XpeImageBuffer: 36 bytes, XpeImageMetadata: 92 bytes)
- **Thread-safe alert queue** for cross-module communication
- **IEC 62304 Class B compliance** - no C++ exceptions across C ABI boundary

**API Functions Available:**
```cpp
// Lifecycle (REQ-P0-011, REQ-P0-012)
XpeErrorCode xpe_init(const char* configJsonOrNull);
void         xpe_shutdown(void);
const char*  xpe_version(void);

// Configuration (REQ-P0-014)
XpeErrorCode xpe_configure(const char* jsonConfig);
XpeErrorCode xpe_get_param_range(const char* bodyPart, const char* paramName,
                                 float* minVal, float* maxVal, float* defaultVal);

// Alert Queue (REQ-P0-019~021)
int32_t     xpe_get_pending_alert_count(void);
XpeErrorCode xpe_get_pending_alert(int32_t index, char* msg, size_t msgLen, int32_t* severity);
void         xpe_clear_alerts(void);

// Logging (REQ-P0-023~025)
XpeErrorCode xpe_log_set_level(int level);
XpeErrorCode xpe_log_set_file(const char* filePath);
void         xpe_log_flush(void);

// AED (REQ-P0-026~028)
XpeErrorCode xpe_aed_configure(const char* jsonConfig);
XpeErrorCode xpe_aed_poll_event(int32_t* eventTypeOut, uint64_t* timestampOut, float* signalLevelOut);
XpeErrorCode xpe_aed_get_status(int32_t* stateOut);
```

### Memory Management Infrastructure

**File Path**: `modules/common/src/xpe_memory.cpp`

**Key Features:**
- **Image buffer allocation** with size limits (4096x4096 max, 64MB max)
- **C++17 RAII pattern** with explicit allocation/deallocation
- **Memory copy functions** with buffer size validation
- **Supports both UINT16 and FLOAT32 pixel formats**

### Pre-processing Module Current State

**Directory Structure**: `modules/preprocess/`
- **Status**: Empty module directory with only build_test subdirectory
- **No implementation files**: No CMakeLists.txt, no source files, no headers
- **Integration**: Referenced in root CMakeLists.txt via `xpe_add_optional_subdirectory()`

---

## Algorithm References (XPE-ALG-001)

### Mathematical Formulas Identified

**Offset Correction (SWU-1.1)**:
```
I_offset(x,y) = max(I_raw(x,y) - I_dark(x,y), 0)
```

**Gain Correction (SWU-1.2)**:
```
G(x,y) = mean(I_flat) / (I_flat(x,y) - I_dark(x,y))
```

**Defect Correction (SWU-1.3)**:
- **Baseline**: Edge-aware bilinear interpolation
- **Advanced**: FixPix MLP (1425 parameters)
- **Research**: CNN/ViT approaches

### SIMD Optimization Strategies

**AVX2 Optimizations Identified:**
- **Saturating subtraction**: `_mm256_subs_epu16` for floor-at-zero operations
- **Parallel reductions**: Min/max, sum/variance calculations
- **Type conversion**: `_mm256_cvtepu16_epi32` -> `_mm256_cvtepi32_ps` for uint16->float32
- **FMA chains**: For polynomial evaluations in gain correction

### Calibration File Formats

**Format Structure**:
```json
{
  "magic": "XCal",
  "version": "1.0",
  "type": 0-5 (offset/gain/BPM/nonlinearity/NLCSC/binning),
  "detector_serial": "32-char string",
  "session_id": 64-bit session linking,
  "created_at": "Unix epoch",
  "expires_at": "Unix epoch",
  "kVp": 32-bit float,
  "mAs": 32-bit float,
  "temperature_C": 32-bit float,
  "pixel_format": uint8,
  "compression": uint8 (0=none, 1=LZ4, 2=ZSTD),
  "payload_size": 64-bit,
  "sha256_payload": 256-bit,
  "sha256_header": 256-bit
}
```

---

## Testing Infrastructure

### Google Test Integration

**File Path**: `modules/common/tests/test_xpe_common.cpp`

**Testing Patterns Identified:**
- **Coverage target**: ≥85% statement coverage
- **Test categories**: Unit tests, integration tests, concurrency tests
- **Mock injection**: Test-only functions for alert/AED injection
- **Concurrency validation**: Multi-threaded access testing
- **Memory leak detection**: 1000-cycle allocation/free tests

### P/Invoke Compatibility Testing

**Validation Approach:**
- Pack=8 static_assert in struct definitions
- Struct offset verification via static_assert
- Buffer size validation in copy operations
- Error code consistency across C/C++ boundaries

---

## Build System Integration

### CMake Configuration

**File Path**: `CMakeLists.txt` (root)

**Key Integration Points:**
- **Root CMakeLists.txt**: Uses `xpe_add_optional_subdirectory()` for preprocess module
- **Module structure**: Each module has its own CMakeLists.txt
- **Dependency management**: vcpkg for spdlog, nlohmann_json, fmt
- **Build options**: BUILD_SHARED_LIBS, BUILD_TESTS, BUILD_GSVG
- **Output directories**: bin/, lib/, lib/ for runtime, library, and archive outputs

### Version Requirements

- **CMake**: Minimum 3.25
- **C++ Standard**: C++17 enforced across all modules
- **Compiler warnings**: Treated as errors via XPE_WARNINGS_AS_ERRORS option

---

## 3-Layer Architecture Context

Based on analysis, XPE follows a **3-layer architecture**:

### Layer 0: Foundation
- **xpe_common.dll**: Core services (18 functions)
- **Purpose**: Lifecycle, configuration, memory management, error handling

### Layer 1: Domain Modules
- **xpe_preprocess.dll**: Target module (9 SWUs: PRE-01 through PRE-09)
- **Dependencies**: xpe_common.dll
- **Functions**: ~18 API functions matching xpe_common pattern

### Layer 1-G: Independent Modules
- **GSVG module**: Grid suppression & virtual grid (independent)
- **Status**: Optional build via BUILD_GSVG flag

---

## Existing Patterns and Code Conventions

### API Design Patterns

**Naming Convention:**
- All functions prefixed with `xpe_`
- Consistent parameter ordering (input/output, output last)
- Error codes as int32_t with XPE_ERR_* constants

### Memory Management

**RAII Pattern:**
```cpp
// Allocation
XpeImageBuffer buf;
xpe_alloc_image(width, height, format, &buf);

// Usage
// ... process image ...

// Deallocation
xpe_free_image(&buf);
```

**Pack=8 Compliance:**
```cpp
#pragma pack(push, 8)
typedef struct XpeImageBuffer {
    uint32_t width;       // 4 bytes
    uint32_t height;      // 4 bytes
    // ... total 36 bytes
} XpeImageBuffer;
#pragma pack(pop)

static_assert(sizeof(XpeImageBuffer) == 36, "P/Invoke Pack=8 compatibility");
```

### Error Handling

**Consistent Error Codes:**
```cpp
#define XPE_OK                       0
#define XPE_ERR_INVALID_INPUT       -1
#define XPE_ERR_OUT_OF_MEMORY       -2
#define XPE_ERR_PROCESSING_FAILED   -3
// ... 10 total error codes
```

---

## Reference Implementations

### Similar Functions in xpe_common

**Pattern Example**: Alert queue implementation
```cpp
// Thread-safe singleton with mutex protection
static std::mutex g_alertMutex;
static std::queue<AlertEntry> g_alertQueue;

// P/Invoke compatible API
XPE_API int32_t xpe_get_pending_alert_count(void) {
    std::lock_guard<std::mutex> lock(g_alertMutex);
    return static_cast<int32_t>(g_alertQueue.size());
}
```

### Algorithm Implementation Reference

**Gain Correction Pattern** (from XPE-ALG-001):
```cpp
// uint16 -> float32 format boundary conversion
XPE_API XpeErrorCode xpe_gain_correction(
    const XpeImageBuffer* raw,
    const XpeImageBuffer* dark,
    const float* gain_map,
    XpeImageBuffer* out
) {
    // AVX2-optimized processing
    // Format conversion: uint16 -> float32
    // Normalization: divide by gain map
    // NaN/Inf validation
}
```

---

## Risks and Constraints

### ABI Compatibility Requirements

**P/Invoke Constraints:**
- Pack=8 struct alignment mandatory
- No virtual functions or C++ exceptions in exported APIs
- Fixed-size arrays only, no std::string in structs
- Memory management caller-responsibility model

### IEC 62304 Compliance

**Safety Class**: Class B medical device software
**Requirements:**
- No C++ exceptions across C ABI boundary
- Thread-safe concurrent access
- Comprehensive error handling
- Memory safety guarantees

### Performance Constraints

**Time Budgets** (from XPE-ALG-001):
- Offset Correction: < 55ms/frame
- Gain Correction: < 55ms/frame
- Defect Correction: < 95ms/frame
- Total Pre-processing: < 500ms/frame

**Memory Budgets:**
- Maximum image size: 4096x4096x4 = 64MB
- Calibration files: Multi-gain sets, total ~500MB
- Runtime buffers: History rings, temporary storage

---

## Recommendations

### Implementation Approach for Gain/Offset/Defect Corrections

#### 1. Gain Correction Implementation
**Priority**: HIGH (format boundary conversion)
**Approach:**
- Implement `xpe_gain_correction()` first
- Use AVX2 for uint16->float32 conversion
- Include reciprocal gain map optimization
- Add NaN/Inf validation

**API Design:**
```cpp
XPE_API XpeErrorCode xpe_gain_correction(
    const XpeImageBuffer* raw,      // Input: UINT16 raw frame
    const XpeImageBuffer* dark,      // Input: Dark reference
    const float* gain_map,          // Input: Gain calibration map
    float* output_buffer,           // Output: FLOAT32 working buffer
    uint32_t width, uint32_t height  // Dimensions
);
```

#### 2. Offset Correction Implementation
**Priority**: HIGH (first processing stage)
**Approach:**
- Implement temperature/PREP-time interpolation
- Use `_mm256_subs_epu16` for saturating subtraction
- Add residual non-uniformity correction
- Include floor-at-zero validation

**Key Algorithms:**
```cpp
// Temperature interpolation
float alpha = (current_temp - temp1) / (temp2 - temp1);
XpeImageBuffer* dark_interpolated = alpha * dark1 + (1-alpha) * dark2;

// PREP-time exponential model
float time_factor = x1 * exp(x2 * prep_time + x3);
XpeImageBuffer* dark_corrected = dark_interpolated * time_factor;

// Core subtraction
I_offset = max(I_raw - dark_corrected, 0);
```

#### 3. Defect Correction Implementation
**Priority**: MEDIUM (quality improvement)
**Approach:**
- Implement FixPix MLP for advanced correction
- Include edge-aware bilinear fallback
- Add runtime transient defect detection
- Maintain static BPM + runtime detection merge

**MLP Structure:**
```cpp
// FixPix MLP: 5x5 patch -> 24 inputs -> hidden (8 neurons) -> 1 output
// Total parameters: 24*8 + 8*1 + 8 + 1 = 1425
float defect_correction_mlp(uint16_t* patch) {
    // Extract 5x5 neighborhood (24 pixels excluding center)
    // Compute weighted sum through two layers
    // Apply sigmoid activation for output
}
```

#### 4. Calibration Data Management
**Priority**: CRITICAL (foundation for all corrections)
**Approach:**
- Implement SHA-256 integrity validation
- Add session grouping (offset/gain/BPM must match)
- Include expiry + drift scoring
- Support hierarchical loading (mandatory vs optional)

### Integration Strategy

#### Phase 1: Foundation (0-3 months)
1. Implement calibration data loading/validation
2. Develop offset correction with temperature interpolation
3. Create gain correction with format conversion
4. Establish testing infrastructure (≥85% coverage)

#### Phase 2: Advanced Features (3-6 months)
1. Implement defect correction (FixPix MLP)
2. Add ghost/lag correction (3-tier NLCSC)
3. Include nonlinearity correction
4. Add binning correction support

#### Phase 3: Optimization (6-9 months)
1. AVX2 SIMD optimization across all stages
2. Multi-threading for throughput scaling
3. GPU offload for compute-intensive stages
4. Performance benchmarking and validation

### Testing Strategy

#### Unit Testing Approach
- **Golden reference datasets**: Synthetic test cases with known outputs
- **Edge case validation**: Extreme temperatures, large defects, corrupted data
- **Performance regression**: Continuous benchmarking against targets
- **Concurrency testing**: Multi-threaded access validation

#### Integration Testing
- **Pipeline validation**: End-to-end processing with real clinical data
- **Calibration lifecycle**: Load/use/update/validate calibration sequences
- **Error handling**: Graceful degradation on calibration failure
- **Memory safety**: Leak detection, bounds checking, invalid access prevention

### Critical Success Factors

1. **Pack=8 Compliance**: Strict adherence to P/Invoke struct alignment
2. **IEC 62304 Class B**: No exceptions across C ABI, comprehensive error handling
3. **Performance Budgets**: Meet time targets for 3072x3072 processing
4. **Quality Metrics**: Achieve algorithm performance targets from XPE-ALG-001
5. **Testing Coverage**: Maintain ≥85% statement coverage across all code

---

## Conclusion

The XPE codebase provides a solid foundation with xpe_common.dll establishing the API patterns and P/Invoke compatibility requirements. The preprocess module represents a greenfield opportunity to implement state-of-the-art pre-processing algorithms while maintaining compatibility with the existing infrastructure.

**Key Success Indicators:**
- Implementation of 9 SWUs with algorithmic performance matching commercial systems
- Maintaining P/Invoke compatibility for C# integration
- Achieving performance targets while ensuring IEC 62304 Class B compliance
- Establishing comprehensive testing infrastructure with ≥85% coverage

The recommendations provided here establish a clear implementation path that balances technical excellence with practical development constraints, ensuring the pre-processing module meets the rigorous requirements of medical imaging software.

---

*Document End - SPEC-XPE-P1A Research*
