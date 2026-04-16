# SPEC-XPE-P1A: Compact Specification

**Document ID**: SPEC-XPE-P1A
**Version**: 1.0.0
**Date**: 2026-04-16
**Status**: Planned
**Auto-Generated From**: spec.md

---

## Requirements (EARS Format)

### Ubiquitous Requirements (항상 활성)

**REQ-P1A-001**: Module Initialization
The preprocess module **shall** initialize its internal state when `xpe_preprocess_init()` is called with valid configuration, and report `XPE_OK` on success.

**REQ-P1A-002**: P/Invoke ABI Compliance
The preprocess module **shall** export all functions with `extern "C"` linkage, `__cdecl` calling convention, and `#pragma pack(push, 8)` struct alignment compatible with C# `[StructLayout(Pack = 8)]`.

**REQ-P1A-003**: Thread Safety
All exported functions **shall** be thread-safe for concurrent read operations on independent caller-supplied buffers. Write operations on shared internal state **shall** use mutex protection.

**REQ-P1A-004**: Error Code Consistency
All functions **shall** return `XpeErrorCode` (int32_t) with consistent error codes: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_OUT_OF_MEMORY`, `XPE_ERR_PROCESSING_FAILED`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_NOT_INITIALIZED`, `XPE_ERR_UNSUPPORTED_FORMAT`, `XPE_ERR_BUFFER_TOO_SMALL`, `XPE_ERR_IO_FAILED`, `XPE_ERR_CALIBRATION_EXPIRED`, `XPE_ERR_NETWORK_FAILED`.

**REQ-P1A-005**: Input Validation
All pointer parameters **shall** be validated for NULL before dereferencing. Array size mismatches **shall** return `XPE_ERR_BUFFER_TOO_SMALL`.

**REQ-P1A-030**: No Exceptions Across C ABI
No C++ exceptions **shall** propagate across the C ABI boundary. All exceptions **shall** be caught internally and converted to `XPE_ERR_PROCESSING_FAILED`.

**REQ-P1A-031**: No Memory Leak
All allocated memory **shall** be properly freed. 1000-cycle allocation/free test **shall** show no memory growth.

**REQ-P1A-032**: No Uninitialized Output
All output buffers **shall** be fully initialized with defined values (no undefined reads).

**REQ-P1A-033**: No NaN/Inf in Output
All output pixel values **shall** be finite (no NaN or Inf values) unless explicitly specified for diagnostic purposes.

### Functional Requirements (기능 요구사항)

**REQ-P1A-010**: Offset Correction Execution
The system **shall** execute offset correction using the formula: `I_offset(x,y) = max(I_raw(x,y) - I_dark(x,y), 0)` with temperature interpolation and PREP-time exponential model compensation.

**REQ-P1A-011**: Gain Correction Execution
The system **shall** execute gain correction with UINT16→FLOAT32 format conversion, divide by gain map, and validate NaN/Inf values.

**REQ-P1A-012**: Defect Correction Execution
The system **shall** execute defect correction using edge-aware bilinear interpolation with 5x5 neighborhood.

**REQ-P1A-013**: Runtime Defect Detection
The system **shall** detect transient defects at runtime using dose-dependent threshold and merge with static BPM.

**REQ-P1A-014**: Calibration File Loading (Offset)
`xpe_load_offset_map()` **shall** load XCal format offset maps, validate SHA-256, check session matching, and verify expiry.

**REQ-P1A-015**: Calibration File Loading (Gain)
`xpe_load_gain_map()` **shall** load XCal format gain maps with multi-SID interpolation support.

**REQ-P1A-016**: Calibration File Loading (Defect Map)
`xpe_load_defect_map()` **shall** load XCal format defect maps (BPM) and validate integrity.

**REQ-P1A-017**: Calibration Offset Generation
`xpe_generate_offset_map()` **shall** generate dark frames with configurable integration time and temperature.

**REQ-P1A-018**: Calibration Expiry Check
All calibration files **shall** be checked for expiry based on `expires_at` timestamp and drift scoring.

**REQ-P1A-019**: Calibration Save
`xpe_save_calibration()` **shall** save current calibration state to XCal format with SHA-256 integrity.

**REQ-P1A-020**: Not-Initialized Guard
All processing functions **shall** return `XPE_ERR_NOT_INITIALIZED` if called before `xpe_preprocess_init()`.

**REQ-P1A-021**: Dimension Mismatch Guard
All processing functions **shall** validate input/output buffer dimensions match image metadata.

**REQ-P1A-022**: Format Mismatch Guard
All processing functions **shall** validate pixel format compatibility (UINT16 vs FLOAT32).

### Performance Requirements (성능 요구사항)

**REQ-P1A-040**: SIMD Optimization
Scalar implementation **shall** be completed first. AVX2 SIMD optimization **shall** maintain bit-exact parity with scalar version (verified by parity tests).

**REQ-P1A-041**: Readout Artifact Validation
The system **shall** validate and mask readout artifacts (non-uniform gain, defective lines) before correction.

**REQ-P1A-042**: Parameter Range Query
`xpe_preprocess_get_param_range()` **shall** return valid ranges for calibration parameters.

---

## Acceptance Criteria (Given-When-Then)

### Module Lifecycle

**AC-LC-001**: Initialization with Default Config
```gherkin
Given xpe_preprocess.dll is loaded
When xpe_preprocess_init(NULL) is called
Then the function returns XPE_OK
And the module enters initialized state
```

**AC-LC-002**: Initialization with Valid JSON Config
```gherkin
Given xpe_preprocess.dll is loaded
When xpe_preprocess_init("{\"mode\":\"clinical\"}") is called
Then the function returns XPE_OK
And clinical mode is activated
```

**AC-LC-003**: Double-Init Guard
```gherkin
Given xpe_preprocess.dll is initialized
When xpe_preprocess_init() is called again without shutdown
Then the function returns XPE_ERR_INVALID_INPUT
```

### Offset Correction

**AC-OFF-001**: Basic Offset Correction
```gherkin
Given offset map is loaded and validated
When xpe_offset_correct() is called with valid raw buffer and dark reference
Then output buffer contains I_offset(x,y) = max(I_raw - I_dark, 0)
And floor-at-zero behavior is verified
```

**AC-OFF-002**: Temperature Interpolation
```gherkin
Given two offset maps at different temperatures
When xpe_offset_correct() is called with current temperature
Then interpolated dark reference is used
And interpolation coefficient α = (T_current - T1) / (T2 - T1) is applied correctly
```

**AC-OFF-003**: PREP-Time Model
```gherkin
Given detector has been reset at known PREP time
When xpe_offset_correct() is called with PREP time parameter
Then exponential decay model is applied: I_dark = I_base * exp(x1 * t + x3)
And decay compensates for dark current drift
```

### Gain Correction

**AC-GAIN-001**: UINT16 to FLOAT32 Conversion
```gherkin
Given gain map is loaded
When xpe_gain_correct() is called with UINT16 raw buffer
Then output is FLOAT32 format
And each pixel is processed: I_out = (I_raw - I_dark) * (1.0 / G(x,y))
```

**AC-GAIN-002**: Multi-SID Gain Interpolation
```gherkin
Given gain maps at multiple kVp settings (SID_60, SID_80, SID_100)
When xpe_gain_correct() is called with current kVp
Then gain map is interpolated using bilinear interpolation
And kVp-specific gain factor is applied
```

**AC-GAIN-003**: NaN/Inf Validation
```gherkin
Given gain map contains zero or extreme values
When xpe_gain_correct() is called
Then function returns XPE_ERR_CONFIG_INVALID
And error message specifies invalid gain location
```

### Defect Correction

**AC-DEF-001**: Edge-Aware Bilinear Interpolation
```gherkin
Given defect map (BPM) is loaded
When xpe_defect_correct() is called with defective pixel detected
Then 5x5 neighborhood excluding center is used
And edge-aware weights prioritize valid pixels
```

**AC-DEF-002**: Static BPM Priority
```gherkin
Given both static BPM and runtime detection are available
When xpe_defect_correct() is called
Then static BPM takes priority
And runtime detection only fills gaps in static BPM
```

**AC-DEF-003**: Transient Defect Detection
```gherkin
Given dose-dependent threshold is configured
When xpe_defect_correct() is called with sudden signal change
Then transient defect is detected
And defect map is dynamically updated
```

### Calibration Management

**AC-CAL-001**: Offset Map Loading
```gherkin
Given valid XCal format offset file exists
When xpe_load_offset_map() is called
Then offset map is loaded into memory
And SHA-256 integrity check passes
And session_id matches current calibration session
```

**AC-CAL-002**: Gain Map Loading
```gherkin
Given valid XCal format gain file exists
When xpe_load_gain_map() is called with multi-SID support
Then gain map is loaded with interpolation table
And kVp-specific gain factors are available
```

**AC-CAL-003**: Defect Map Loading
```gherkin
Given valid XCal format BPM file exists
When xpe_load_defect_map() is called
Then defect map is loaded into memory
And defect locations are validated
```

**AC-CAL-004**: Expiry Check
```gherkin
Given calibration file has expires_at timestamp
When calibration is loaded
Then current time is compared to expires_at
And XPE_ERR_CALIBRATION_EXPIRED is returned if expired
```

**AC-CAL-005**: Session Matching
```gherkin
Given multiple calibration files are loaded
When session_id values don't match
Then XPE_ERR_CONFIG_INVALID is returned
And error message specifies conflicting sessions
```

### Performance & Quality

**AC-PER-001**: Scalar Reference Baseline
```gherkin
Given scalar implementation is complete
When performance is measured for 3072x3072 image
Then offset correction completes in <55ms
And gain correction completes in <55ms
And defect correction completes in <95ms
```

**AC-PER-002**: SIMD Parity
```gherkin
Given AVX2 SIMD implementation is complete
When parity test compares scalar vs SIMD outputs
Then results are bit-exact identical
And SIMD achieves >2x speedup over scalar
```

**AC-PER-003**: Memory Safety
```gherkin
Given 1000-cycle processing loop
When memory usage is monitored
Then no memory leaks are detected
And peak memory usage stays within budget
```

### Error Handling

**AC-ERR-001**: Not-Initialized Guard
```gherkin
Given xpe_preprocess.dll is loaded but not initialized
When any processing function is called
Then XPE_ERR_NOT_INITIALIZED is returned
```

**AC-ERR-002**: Dimension Mismatch
```gherkin
Given input buffer is 3072x3072
When output buffer is 2048x2048
Then XPE_ERR_BUFFER_TOO_SMALL is returned
```

**AC-ERR-003**: Format Mismatch
```gherkin
Given input buffer is UINT16 format
When gain correction expects FLOAT32 working buffer
Then XPE_ERR_UNSUPPORTED_FORMAT is returned
```

---

## Files to Modify

**New Files:**
- `modules/preprocess/CMakeLists.txt` - Module build configuration
- `modules/preprocess/include/xpe/preprocess_api.h` - Public API header
- `modules/preprocess/src/xpe_preprocess.cpp` - Main implementation
- `modules/preprocess/src/xpe_offset.cpp` - Offset correction
- `modules/preprocess/src/xpe_gain.cpp` - Gain correction
- `modules/preprocess/src/xpe_defect.cpp` - Defect correction
- `modules/preprocess/src/xpe_calibration.cpp` - Calibration management
- `modules/preprocess/tests/test_xpe_preprocess.cpp` - Unit tests

**Modified Files:**
- `CMakeLists.txt` (root) - Add preprocess module reference (already configured)
- `.moai/specs/SPEC-XPE-P1A/*` - SPEC documents (already created)

---

## Exclusions (What NOT to Build)

The following features are explicitly OUT OF SCOPE for SPEC-XPE-P1A:

- **PRE-04/PRE-05**: Ghost/Lag Correction (SWU-1.4) → Separate SPEC (stateful handle architecture)
- **PRE-07**: Temperature Compensation (SWU-1.6) → Separate SPEC (MCU integration needed)
- **PRE-08**: Nonlinearity Correction → Separate SPEC
- **PRE-09**: Pixel Binning Correction → Separate SPEC (fluoroscopy/CBCT-specific)
- **PRE-06 ML/ViT AE**: Advanced defect correction → Phase 2 Differentiator
- **GPU offload** (CUDA/OpenCL) → Phase 3 optimization
- **ONNX Runtime** → AI module (xpe_ai.dll)
- **OpenCV dependency** → Anti-Spaghetti principle (xpe_common only)

---

*Auto-generated from spec.md v1.0.0*
