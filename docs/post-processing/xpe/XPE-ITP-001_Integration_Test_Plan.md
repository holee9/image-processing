# XPE Integration Test Plan

> **Document ID**: XPE-ITP-001 | **Version**: 1.0 | **Date**: 2026-04-14
>
> **IEC 62304 Clause**: 5.6 — Software Integration Testing
>
> **Safety Classification**: Class B
>
> **Trace Source**: XPE-SRS-001, XPE-SDD-001, XPE-SDD-002, XPE-STP-001

---

## 1. Purpose & Scope

This document consolidates integration test cases for the XPE software system as referenced in XPE-SDP-001 §3 (Deliverables), which lists XPE-ITP-001 as a required deliverable.

**Objective**: Verify that software units (SWU) communicate correctly across interfaces, data flows through the pipeline without corruption, and system-level requirements are met.

**Scope**:
- Five integration levels: SWI unit combinations from bottom-up
- 12 integration test cases (IT-001 through IT-012)
- Extracted and formalized from XPE-STP-001 §3 and XPE-VVP-001
- Thread safety, memory, and end-to-end pipeline validation

**Out of Scope**: System test with clinical data (XPE-STP-001), acceptance testing with phantom

---

## 2. Integration Strategy

### 2.1 Bottom-Up Integration Approach

```
Level 1: SWI-5 (Common/Utilities) standalone
  └─ Tests: Memory allocation, configuration, error codes (no dependencies)

Level 2: SWI-1 (Pre-Processing) + SWI-5
  └─ Tests: Offset correction, calibration loading, pipeline
  └─ Depends on: SWI-5 for memory & error handling

Level 3: SWI-2 (Core Processing) + SWI-1, SWI-5
  └─ Tests: Ghost correction, noise reduction, artifact removal
  └─ Depends on: Pre-processing output, SWI-5

Level 4: SWI-3 (Display) + SWI-4 (DICOM) + SWI-2, SWI-5
  └─ Tests: Log transform → LUT, DICOM C-STORE
  └─ Depends on: Core processing output, display pipeline

Level 5: Full Pipeline E2E
  └─ Tests: Phase 0+1a raw → corrected, Phase 1b with display
  └─ Depends on: All layers integrated
```

### 2.2 Integration Level Dependencies

| Level | SWI Focus | Depends On | Purpose |
|:---:|:---|:---|:---|
| **1** | SWI-5 | None | Baseline: memory/config stable |
| **2** | SWI-1 + SWI-5 | Level 1 | Offset/gain pipeline functional |
| **3** | SWI-2 + SWI-1, SWI-5 | Level 2 | Core enhancement pipeline |
| **4** | SWI-3, SWI-4 + all below | Level 3 | Display + DICOM output |
| **5** | Full system | All levels | E2E image pipeline |

---

## 3. Integration Test Cases

### IT-001: SWI-1 + SWI-5 Interface — Offset Correction Pipeline

**Test ID**: IT-001  
**SRS Trace**: SRS-FUNC-001, SRS-SAFE-001  
**Integration Level**: 2 (Pre-Processing + Common)

**Description**: Verify that SWI-5 (MemoryPool, CalibDataManager) correctly supplies offset map to SWI-1 (OffsetCorrection), image buffer is not destructively modified, and output is uint16.

**Test Setup**:
- Synthetic raw frame: 256×256 uint16, values 500–1000
- Offset map: 256×256 uint16, values 100–300 (temperature-compensated via SWU-1.7)
- SWI-5: Initialize with cached offset map

**Test Steps**:
1. Call `xpe_preprocess_init()` → SWI-5 loads offset map
2. Call `xpe_offset_correct(frame, offset_map)` → SWI-1 processes
3. Verify:
   - Output frame pixel = input − offset (within ±1 LSB rounding)
   - Input frame buffer unchanged (non-destructive read)
   - Output is uint16
   - Underflow clamped to 0

**Expected Outcome**:
- Frame[100,100] = 800 − 200 = 600 ✓
- Frame[200,200] = 500 − 100 = 400 ✓
- Input buffer unchanged ✓

**Pass Criteria**: All pixels match expected offset subtraction; no buffer corruption

---

### IT-002: SWI-1 + SWI-2 Pipeline — Gain Correction & Ghost Correction

**Test ID**: IT-002  
**SRS Trace**: SRS-FUNC-002, SRS-FUNC-004  
**Integration Level**: 3 (Core Processing + Pre-Processing)

**Description**: Verify offset-corrected frame flows to gain correction (format boundary uint16 → float32) and downstream ghost correction receives float32 correctly.

**Test Setup**:
- Raw frame: 512×512, synthetic
- Offset map: 512×512
- Gain map: 512×512 float32, mean=1.0 (unity gain for easy verification)
- Ghost history: 8 frames from prior exposure

**Test Steps**:
1. Offset correct → uint16 output
2. Gain correct → float32 output (format boundary)
3. Ghost correct (Tier 2) → input expected float32
4. Verify:
   - Output is float32
   - Pixel values ~ (offset_corrected / gain) ≈ input
   - Ghost correction receives float32 without type error

**Expected Outcome**:
- Format boundary crossed successfully ✓
- Ghost correction processes float32 ✓
- No memory corruption at boundary ✓

**Pass Criteria**: Format conversion correct; ghost correction accepts output without crash

---

### IT-003: SWI-2 + SWI-3 Pipeline — Log Transform → LUT

**Test ID**: IT-003  
**SRS Trace**: SRS-FUNC-020, SRS-FUNC-021  
**Integration Level**: 4 (Display)

**Description**: Verify float32 output from SWI-2 flows to SWI-3 (Display), log transform applied, and LUT mapping produces uint8 without overflow/underflow.

**Test Setup**:
- Core output: float32, range 0.1–2000 (typical dynamic range)
- Window/level: width=800, center=400 (example)
- Display LUT: 256-entry uint8

**Test Steps**:
1. Apply log transform: output_log = log10(max(pixel, 1e-5))
2. Normalize to [0, 1]: norm = (log − log_min) / (log_max − log_min)
3. Quantize to uint8: lut_index = norm × 255
4. Map through LUT → display value

**Expected Outcome**:
- No NaN/Inf in output (clipped if any)
- All pixels map to valid LUT index [0, 255] ✓
- Histogram reasonable (not all-black, not all-white)

**Pass Criteria**: Display output uint8, valid range, no artifacts

---

### IT-004: SWI-2 + SWI-4 Pipeline — Core Output to DICOM Write/Re-read

**Test ID**: IT-004  
**SRS Trace**: SRS-FUNC-030, SRS-FUNC-031  
**Integration Level**: 4 (DICOM)

**Description**: Verify float32 processed image written to DICOM file and re-read yields identical pixel values (within floating-point tolerance).

**Test Setup**:
- Processed float32 frame: 512×512, range 50–500
- DICOM file: Grayscale Standard Display Function (GSDF) compliant, float32 Pixel Data
- Metadata: Patient ID, Acquisition time, Detector model

**Test Steps**:
1. Write processed frame → DICOM file via SWI-4 C-STORE
2. Read DICOM file back → SWI-4 C-FIND/C-GET
3. Compare:
   - Pixel values match within ±1e-6 ✓
   - Metadata preserved ✓
   - DICOM tags valid ✓

**Expected Outcome**:
- Round-trip pixel values identical ✓
- Metadata preserved ✓
- File readable by standard DICOM viewers ✓

**Pass Criteria**: Pixel-perfect round-trip; metadata intact

---

### IT-005: E2E Phase 0 + Phase 1a — Raw → Corrected Image (Minimal)

**Test ID**: IT-005  
**SRS Trace**: SRS-FUNC-001..002  
**Integration Level**: 5 (Full Pipeline, minimal)

**Description**: Minimal end-to-end test: raw frame input → calibration load → offset + gain correction → float32 output.

**Test Setup**:
- Raw frame: 256×256, synthetic uint16 ADC data
- Calibration: Factory offset/gain maps
- Session: Single-detector, single frame

**Test Steps**:
1. Initialize SWI-5 (load calibration)
2. SWI-1 offset correction
3. SWI-1 gain correction (format boundary)
4. Output to float32 buffer
5. Verify output is valid corrected image

**Expected Outcome**:
- Output float32 ✓
- Pixel range reasonable (0–1000 after normalization) ✓
- No NaN/Inf ✓

**Pass Criteria**: Full minimal pipeline completes without error

---

### IT-006: E2E Phase 1b — Corrected → Displayed Image (with LUT)

**Test ID**: IT-006  
**SRS Trace**: SRS-FUNC-020..023  
**Integration Level**: 5 (Full Pipeline)

**Description**: End-to-end pipeline including display: calibrated float32 → log transform → LUT → uint8 display frame.

**Test Setup**:
- Input: Phase 1a output (corrected float32)
- Window/level: 400/800 (typical chest X-ray)
- Display preset: "Chest AP"

**Test Steps**:
1. Perform Phase 1a (offset + gain)
2. Phase 1b: Log transform → LUT
3. Apply display preset
4. Output uint8 display frame
5. Verify image quality (histogram, contrast)

**Expected Outcome**:
- Display frame uint8, valid range [0, 255] ✓
- Histogram shows reasonable distribution (not clipped) ✓
- GSDF compliance verified ✓

**Pass Criteria**: Display frame valid; GSDF compliance check passes

---

### IT-007: SWI-2 + GSVG Module — Grid Detection & Suppression

**Test ID**: IT-007  
**SRS Trace**: ALG-SPEC-001 (GSVG), SRS-FUNC-050  
**Integration Level**: 4 (Enhancement)

**Description**: Verify processed image with grid pattern detected and suppressed by GSVG module (external DLL); output grid-free.

**Test Setup**:
- Processed image: 1024×1024, with synthetic grid (5 mm pitch)
- GSVG: Initialize with grid parameters (pitch, angle)

**Test Steps**:
1. Feed processed frame to GSVG via interface
2. Detect grid lines
3. Suppress grid (interpolation)
4. Output grid-free image

**Expected Outcome**:
- Grid completely removed or < 5% residual ✓
- Image quality unchanged except grid ✓

**Pass Criteria**: Grid artifact < 5% residual

---

### IT-008: SWI-1 + SWI-2 — Multi-Frame Ghost Correction (Tier Escalation)

**Test ID**: IT-008  
**SRS Trace**: SRS-FUNC-004, ALG-SPEC-001  
**Integration Level**: 3 (Core + Pre-Processing)

**Description**: Verify 3-frame sequence with ghost correction auto-escalates from Tier 1 (LTI) → Tier 2 (exposure-weighted) → Tier 3 (NLCSC) based on frame content analysis.

**Test Setup**:
- Frame sequence: Low dose → High dose → Low dose (lag expected)
- Ghost history buffer: 8 frames, initially seeded
- Tier selection: Automatic based on exposure

**Test Steps**:
1. Process Frame 1 (low dose) → Tier 1 (LTI)
2. Process Frame 2 (high dose) → Tier 2 (exposure-weighted detected)
3. Process Frame 3 (low dose) → Tier 3 (NLCSC, signal-dependent lag detected)
4. Verify:
   - Tier escalation logic triggered ✓
   - Output artifact suppression improves each tier ✓

**Expected Outcome**:
- Frame 3 ghost artifact < 2% after Tier 3 ✓
- Each tier better than previous ✓

**Pass Criteria**: Artifact < 2% after Tier 3; escalation detected correctly

---

### IT-009: Full Pipeline — Error Recovery (Missing Calibration)

**Test ID**: IT-009  
**SRS Trace**: SRS-SAFE-001, SRS-SAFE-002  
**Integration Level**: 5 (Full system, error path)

**Description**: Verify graceful failure when calibration file missing: pipeline fails safely, error code returned, no crash, user alerted.

**Test Setup**:
- Raw frame: Valid
- Calibration: Offset map file deleted (simulated)

**Test Steps**:
1. Initialize SWI-5 → attempt to load offset
2. xpe_calib_load_offset() returns XPE_ERR_NOT_INITIALIZED
3. Pipeline init fails
4. Operator receives error alert
5. Verify no buffer corruption, no undefined behavior

**Expected Outcome**:
- Pipeline init returns error code ✓
- Alert issued to operator ✓
- No crash ✓
- No memory leaks ✓

**Pass Criteria**: Graceful failure; operator notified; no corruption

---

### IT-010: Concurrent Frame Processing — Thread Safety

**Test ID**: IT-010  
**SRS Trace**: SRS-PERF-002  
**Integration Level**: 5 (Full system, concurrency)

**Description**: Verify multiple frames processed concurrently without race conditions or memory corruption.

**Test Setup**:
- Threads: 4 concurrent processing threads
- Frames: 16 frames, randomly assigned to threads
- Shared: CalibDataManager (SWI-5), common calibration data
- Test duration: 10 seconds

**Test Steps**:
1. Spawn 4 worker threads
2. Each thread: call full pipeline (offset → gain → ghost → display)
3. Monitor for:
   - Race condition detectors (TSAN if available)
   - Memory leak detectors (valgrind)
   - Output pixel correctness
4. Verify all 16 frames complete without error

**Expected Outcome**:
- All frames complete ✓
- No TSAN warnings ✓
- No memory leaks ✓
- Pixel values consistent across runs ✓

**Pass Criteria**: No threading issues; deterministic output

---

### IT-011: Memory Stability — 1000-Frame Batch Processing

**Test ID**: IT-011  
**SRS Trace**: SRS-PERF-004  
**Integration Level**: 5 (Full system, stress)

**Description**: Verify memory usage remains stable during extended batch processing (1000 frames) with no leaks or fragmentation.

**Test Setup**:
- Batch: 1000 frames, 512×512 synthetic data
- Memory monitoring: Peak usage at start vs. end
- Tolerance: < 5% growth (some working set expected)

**Test Steps**:
1. Initialize SWI-5, load calibration
2. Loop 1000 times:
   - Process raw frame → corrected image
   - Record memory usage
3. Monitor heap fragmentation (if available)
4. Compare start vs. end memory

**Expected Outcome**:
- Memory growth < 5% ✓
- No heap fragmentation detected ✓
- Processing time per frame stable ✓

**Pass Criteria**: Memory stable; < 5% growth; no leaks

---

### IT-012: DICOM C-STORE SCU — Network Image Send

**Test ID**: IT-012  
**SRS Trace**: SRS-FUNC-031  
**Integration Level**: 4 (DICOM network)

**Description**: Verify processed image written to DICOM and sent via C-STORE to test PACS; server acknowledges.

**Test Setup**:
- Test PACS: Local DCMTK DICOM server, port 11112
- Processed image: 512×512 float32
- DICOM metadata: Valid patient, study, series IDs

**Test Steps**:
1. Create DICOM file from processed image
2. Connect to test PACS via C-STORE SCU
3. Send image
4. Verify DICOM Success (0x0000 status)
5. Query PACS for received image → C-FIND

**Expected Outcome**:
- C-STORE succeeds (0x0000) ✓
- Image stored in PACS ✓
- C-FIND retrieves image ✓

**Pass Criteria**: Network send/receive successful; image in PACS

---

## 4. Pass/Fail Criteria

### Unit-Level Pass Criteria
- All test steps execute without crash
- Output values within tolerance (or exactly as specified)
- No memory corruption (heap checks, ASAN)
- No resource leaks (file handles, memory)

### Integration-Level Pass Criteria
- Interface communication correct (data types, sizes match)
- Data flows through pipeline without loss or corruption
- Error codes and alerts issued correctly
- Concurrent/stress tests complete without race conditions

### Overall Test Pass Criteria
- **12/12 tests pass** (100% pass rate required for release)
- **No Critical or High severity defects** blocking pipeline
- **Medium/Low defects** logged in defect tracker (XPE-SPR-001)

---

## 5. Test Environment

| Aspect | Specification |
|--------|---|
| **OS** | Windows 11 Pro (x64) |
| **Compiler** | MSVC 19.x (Visual Studio 2022), C++17 |
| **Test Framework** | Google Test 1.14, Catch2 |
| **Memory Monitoring** | AddressSanitizer (ASAN), Dr. Memory |
| **Concurrency Checker** | ThreadSanitizer (TSAN) where available |
| **Test Data** | Synthetic frames (256–1024 px), phantom X-ray images (real calibration) |
| **DICOM Test Server** | DCMTK storescp (local, ephemeral) |
| **Hardware** | Modern desktop (Intel i7/Ryzen 7, 16 GB RAM) |

---

## 6. Regression Testing Strategy

### Baseline Establishment
- Run all IT tests on current stable build
- Record baseline results (pixel values, timing, memory)
- Commit baseline to version control

### Per-Integration Cycle
- Re-run all IT tests after each SWU change
- Compare output pixels to baseline (tolerance: ±0.1%)
- Flag any deviations as regression candidate
- Investigate root cause (e.g., algorithm change, calibration drift)

### Automated Regression
- CI pipeline (Gitea Actions) runs all IT tests on every commit
- Blocks merge if any test fails
- Reports to team (email/Slack)

---

## 7. Problem Resolution

**Defect Management**: Integration test failures logged in XPE-SPR-001 (Software Problem Report).

**Triage Process**:
1. Reproduce failure in controlled environment
2. Capture logs, memory dumps, core files
3. Assign to responsible developer (SWU owner)
4. Developer fixes SWU → re-run integration tests
5. Verify fix doesn't regress other tests

**Escalation**: If integration test fails repeatedly:
- Schedule design review (SDD-001/SDD-002)
- Consider interface redesign
- Update test case if specification changed

---

## 8. Traceability Summary

| Test ID | SRS Req | SWI Coverage | Test Type | Status |
|:---|:---|:---|:---|:---|
| IT-001 | SRS-FUNC-001, SAFE-001 | SWI-1, SWI-5 | L2 interface | Ready |
| IT-002 | SRS-FUNC-002, FUNC-004 | SWI-1, SWI-2 | L3 pipeline | Ready |
| IT-003 | SRS-FUNC-020..021 | SWI-2, SWI-3 | L4 display | Ready |
| IT-004 | SRS-FUNC-030..031 | SWI-2, SWI-4 | L4 DICOM | Ready |
| IT-005 | SRS-FUNC-001..002 | All (minimal) | L5 E2E | Ready |
| IT-006 | SRS-FUNC-020..023 | All (full) | L5 E2E | Ready |
| IT-007 | ALG-SPEC-001, SRS-FUNC-050 | SWI-2, GSVG | L4 enhancement | Ready |
| IT-008 | SRS-FUNC-004 | SWI-1, SWI-2 | L3 stress | Ready |
| IT-009 | SRS-SAFE-001..002 | All (error path) | L5 robustness | Ready |
| IT-010 | SRS-PERF-002 | All (concurrent) | L5 stress | Ready |
| IT-011 | SRS-PERF-004 | All (long-run) | L5 stress | Ready |
| IT-012 | SRS-FUNC-031 | SWI-4 | L4 network | Ready |

---

## 9. Document Sign-Off

**Test Plan Status**: Ready for formal execution

**Approval Checklist**:
- [ ] All test cases identified (12 total)
- [ ] Integration levels mapped to SWI dependencies
- [ ] SRS traceability complete
- [ ] Test environment specified
- [ ] Pass/fail criteria clear
- [ ] Regression strategy defined
- [ ] QA Manager approval
- [ ] Release Manager approval

---

**Document End**

*This document consolidates integration testing previously scattered across XPE-STP-001 and XPE-VVP-001 into a dedicated Integration Test Plan per IEC 62304 §5.6.*

