# SPEC-XPE-P1A: Acceptance Criteria

**SPEC**: SPEC-XPE-P1A v1.0.0
**Sprint**: S1-A

---

## 1. Given-When-Then Scenarios

### Scenario 1: Full Pipeline Execution

**Given** a raw 3072x3072 uint16 image from the detector, valid offset/gain/defect calibration maps loaded, and a ghost corrector handle with 5+ frames of history
**When** the pre-processing pipeline executes all 8 stages in order
**Then** the output is a float32 image with all correction flags set (`READOUT_VALIDATED | TEMP_COMPENSATED | GAIN_CORRECTED | NONLINEARITY_CORRECTED | DEFECT_CORRECTED | GHOST_CORRECTED`), total processing time is <= 500ms, and pixel values match the golden reference within 1e-6 tolerance.

### Scenario 2: Calibration CRC Integrity Failure

**Given** a calibration file with a corrupted CRC-32 checksum
**When** `xpe_calib_load_offset` is called with the corrupted file path
**Then** the function returns `XPE_ERR_IO_FAILED`, `offsetMapOut` is unchanged, and a CRITICAL alert is posted with the file path and CRC mismatch details.

### Scenario 3: Pipeline with Missing Mandatory Calibration

**Given** no offset map has been loaded (mandatory calibration)
**When** the pipeline attempts to execute offset correction (stage 1)
**Then** the pipeline halts, returns `XPE_ERR_CALIBRATION_EXPIRED`, and no subsequent stages execute.

### Scenario 4: Ghost Correction with Insufficient History

**Given** a ghost corrector handle that has processed only 1 frame
**When** `xpe_ghost_correct` is called with the second frame
**Then** the system applies single-exponential approximation (reduced correction), posts an INFO-level alert, and sets `XPE_FLAG_GHOST_CORRECTED`.

### Scenario 5: Binning Mode Bypass

**Given** the detector is operating in 1x1 mode (no binning)
**When** `xpe_binning_correct` is called with `binningMode == 1`
**Then** the function returns `XPE_OK` without modifying the image, and `XPE_FLAG_BINNING_CORRECTED` is NOT set.

### Scenario 6: Defect Pixel Cluster (No Usable Neighbors)

**Given** a defect map indicating a 6x6 contiguous cluster of bad pixels
**When** `xpe_defect_correct` processes the cluster
**Then** the center pixels (with no usable neighbors within 5x5) retain their original values, a WARNING alert is posted, and the function returns `XPE_OK`.

### Scenario 7: Temperature Sensor Unavailable

**Given** the temperature sensor reports NaN
**When** `xpe_temp_compensate` is called with `detectorTempC == NaN`
**Then** the system uses 25.0C as fallback, posts an INFO-level alert, applies compensation normally, and sets `XPE_FLAG_TEMP_COMPENSATED`.

### Scenario 8: P/Invoke Round-Trip

**Given** the C# ImageProcTest application with P/Invoke declarations for all 18 functions
**When** all 18 functions are called from C# with valid parameters
**Then** all functions return expected values, no `DllNotFoundException` or `MarshalDirectiveException` occurs, and struct layouts are verified via `sizeof` comparison.

### Scenario 9: Concurrent Image Processing

**Given** two threads each with independent image buffers, calibration maps, and ghost handles
**When** both threads execute the full pipeline simultaneously
**Then** both pipelines complete without data corruption, race conditions, or deadlocks, and results match single-threaded execution.

### Scenario 10: Nonlinearity Bypass for Linear Panel

**Given** a detector panel profile that indicates linear response (no nonlinearity coefficients)
**When** `xpe_nonlinearity_correct` is called
**Then** the function returns `XPE_OK` without modifying the image, and `XPE_FLAG_NONLINEARITY_CORRECTED` is NOT set.

---

## 2. Edge Cases

| Edge Case | Expected Behavior | REQ Trace |
|-----------|-------------------|-----------|
| 1x1 image | All corrections process correctly | REQ-P1A-069 |
| 4096x4096 image (maximum) | Processes within 64MB buffer limit | REQ-P1A-070 |
| `frameCount == 1` for offset generation | Returns single frame as offset map | REQ-P1A-071 |
| All pixels are zero after offset subtraction | Valid output (no error) | REQ-P1A-011 |
| Calibration file with expiry exactly at current time | Returns `XPE_ERR_CALIBRATION_EXPIRED` | REQ-P1A-038 |
| Ghost handle used after `xpe_ghost_destroy` | Returns `XPE_ERR_INVALID_INPUT` | REQ-P1A-058 |
| NULL pointer for any required parameter | Returns `XPE_ERR_INVALID_INPUT` | REQ-P1A-056 |
| Empty defect map (0% defect density) | No pixels modified, returns `XPE_OK` | REQ-P1A-024 |

---

## 3. Quality Gate Criteria

| Gate | Threshold | Measurement |
|------|-----------|-------------|
| Statement coverage | >= 90% | gcov/lcov report on all preprocess source files |
| Branch coverage | >= 80% | gcov/lcov report |
| Static analysis (cppcheck) | 0 warnings | `cppcheck --std=c++17 modules/preprocess/` |
| Static analysis (clang-tidy) | 0 warnings | modernize-*, performance-*, bugprone-* |
| Memory leaks | 0 leaks | ASan clean over 1000-frame cycle |
| Export count | Exactly 18 | `dumpbin /exports xpe_preprocess.dll` |
| P/Invoke compatibility | All 18 pass | C# round-trip test |

---

## 4. Definition of Done

- [ ] All 47 EARS requirements (REQ-P1A-001 through REQ-P1A-071) implemented and tested
- [ ] All 10 Given-When-Then scenarios pass
- [ ] All 8 edge cases verified
- [ ] All 7 quality gates green
- [ ] Performance benchmarks within budget (Section 4.3 of spec.md)
- [ ] Code reviewed and merged to integration branch
- [ ] IEC 62304 traceability: every REQ traced to test case in RTM

---

*Acceptance End -- SPEC-XPE-P1A v1.0.0*
