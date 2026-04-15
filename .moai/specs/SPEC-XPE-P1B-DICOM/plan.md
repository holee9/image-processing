# SPEC-XPE-P1B-DICOM: Implementation Plan

**SPEC ID**: SPEC-XPE-P1B-DICOM
**Version**: 1.0.0
**Date**: 2026-04-16
**Methodology**: TDD (RED-GREEN-REFACTOR)

---

## 1. Implementation Sequence

The implementation follows TDD methodology with SWUs ordered by dependency:

```
Milestone 1 (Must): SWU-4.1 DicomReader
  |
  v
Milestone 2 (Must): SWU-4.2 DicomWriter
  |                    (depends on Reader for round-trip testing)
  v
Milestone 3 (Must): SWU-4.3 DicomValidator
  |                    (depends on Reader for file parsing)
  v
Milestone 4 (Should): SWU-4.4 DicomNetworkSCU
                       (depends on Writer for C-STORE file source)
```

---

## 2. Pre-Implementation: vcpkg Dependency Setup

### Task D-0: Configure Third-Party Dependencies

**Priority**: High (blocks all implementation)

1. Add DCMTK to vcpkg.json manifest:
   - Package: `dcmtk` (provides dcmdata, dcmnet, dcmjpeg, dcmimage)
   - Features: `[core,charls]` minimum; `[network]` for SWU-4.4
2. Add OpenJPEG to vcpkg.json manifest:
   - Package: `openjpeg` (JPEG 2000 codec)
3. Verify DCMTK builds with MSVC C++17
4. Create `modules/dicom/CMakeLists.txt` with vcpkg integration

**Files**:
- `vcpkg.json` (project root, add dependencies)
- `modules/dicom/CMakeLists.txt` (new)

---

## 3. Milestone 1: DicomReader (SWU-4.1)

**Priority**: High
**REQ Coverage**: REQ-DICOM-001 through REQ-DICOM-012

### Task D-1: Header and Opaque Handle (RED-GREEN)

1. RED: Write test for `xpe_dicom_open` with valid file -> expect XPE_OK and non-NULL handle
2. GREEN: Create public header with all 4 reader function declarations. Implement `xpe_dicom_open` with DCMTK `DcmFileFormat::loadFile()`. Define internal `XpeDicomHandle` struct wrapping `DcmFileFormat*`.
3. RED: Write tests for missing file, invalid DICOM -> expect correct error codes
4. GREEN: Add file existence check and DICOM magic validation

**Files**:
- `modules/dicom/include/xpe/dicom/xpe_dicom_api.h` (public header, all 10 functions)
- `modules/dicom/src/dicom_reader.cpp`
- `modules/dicom/src/dicom_handle_internal.h` (internal, XpeDicomHandle definition)
- `modules/dicom/tests/test_dicom_reader.cpp`

### Task D-2: Pixel Data Extraction (RED-GREEN)

1. RED: Write test for `xpe_dicom_read_image` with Explicit LE file -> expect uint16 XpeImageBuffer with correct dimensions
2. GREEN: Implement pixel data extraction from DcmDataset. Use `xpe_alloc_image` for buffer. Handle Bits Allocated/Stored.
3. RED: Write test for J2K compressed file -> expect decompressed uint16 output
4. GREEN: Integrate OpenJPEG for J2K decompression; integrate DCMTK JPEG Lossless codec
5. RED: Write test for unsupported Transfer Syntax -> expect XPE_ERR_UNSUPPORTED_FORMAT
6. GREEN: Add Transfer Syntax whitelist check

**Files**:
- `modules/dicom/src/dicom_reader.cpp` (extend)
- `modules/dicom/src/dicom_pixel_codec.cpp` (J2K/JPEG decompression)
- `modules/dicom/tests/test_dicom_reader.cpp` (extend)
- `modules/dicom/tests/testdata/` (sample DICOM files: explicit_le.dcm, j2k_lossless.dcm, jpeg_lossless.dcm)

### Task D-3: Metadata Extraction (RED-GREEN)

1. RED: Write test for `xpe_dicom_get_metadata` -> expect fields populated from DICOM tags
2. GREEN: Implement tag extraction using DCMTK `DcmDataset::findAndGetString/Float32/etc`
3. RED: Write test with missing optional tags -> expect default values, no error
4. GREEN: Add default-value fallback for each tag

**Files**:
- `modules/dicom/src/dicom_reader.cpp` (extend)
- `modules/dicom/tests/test_dicom_reader.cpp` (extend)

### Task D-4: Handle Close and Edge Cases (RED-GREEN-REFACTOR)

1. RED: Write test for `xpe_dicom_close` with valid handle, then NULL handle
2. GREEN: Implement close with DCMTK cleanup; NULL no-op guard
3. REFACTOR: Extract common DCMTK wrapper utilities. Review error handling consistency.

**Files**:
- `modules/dicom/src/dicom_reader.cpp` (extend)
- `modules/dicom/src/dcmtk_utils.h` (internal helper: tag reading, error conversion)
- `modules/dicom/tests/test_dicom_reader.cpp` (extend)

---

## 4. Milestone 2: DicomWriter (SWU-4.2)

**Priority**: High
**REQ Coverage**: REQ-DICOM-013 through REQ-DICOM-022

### Task D-5: Uncompressed Write (RED-GREEN)

1. RED: Write test for `xpe_dicom_write` -> write file, then read back with `xpe_dicom_open` and verify round-trip pixel data integrity
2. GREEN: Implement DICOM dataset creation with DcmFileFormat. Set mandatory DX IOD tags. Embed pixel data as OW (Other Word).
3. RED: Write test for metadata embedding -> verify DICOM tags match input metadata
4. GREEN: Map XpeImageMetadata fields to DICOM tags

**Files**:
- `modules/dicom/src/dicom_writer.cpp`
- `modules/dicom/tests/test_dicom_writer.cpp`

### Task D-6: JPEG 2000 Lossless Write (RED-GREEN)

1. RED: Write test for `xpe_dicom_write_j2k` -> write J2K file, read back, verify bit-exact pixel match
2. GREEN: Integrate OpenJPEG encoder for lossless JPEG 2000. Create encapsulated pixel data element.
3. RED: Write test for encoder failure -> expect XPE_ERR_PROCESSING_FAILED, no partial file
4. GREEN: Add encoder error handling with file cleanup on failure

**Files**:
- `modules/dicom/src/dicom_writer.cpp` (extend)
- `modules/dicom/src/dicom_pixel_codec.cpp` (extend with encode path)
- `modules/dicom/tests/test_dicom_writer.cpp` (extend)

### Task D-7: Error Handling and Rescale (RED-GREEN-REFACTOR)

1. RED: Write tests for NULL input, IO failure, Rescale Slope/Intercept defaults
2. GREEN: Implement parameter validation, file creation error handling, Rescale tag defaults
3. REFACTOR: Extract UID generation utility. Unify metadata-to-tag mapping between read/write.

**Files**:
- `modules/dicom/src/dicom_writer.cpp` (extend)
- `modules/dicom/src/dicom_uid.cpp` (UID generation utility)
- `modules/dicom/tests/test_dicom_writer.cpp` (extend)

---

## 5. Milestone 3: DicomValidator (SWU-4.3)

**Priority**: High
**REQ Coverage**: REQ-DICOM-023 through REQ-DICOM-028

### Task D-8: Conformance Validation (RED-GREEN-REFACTOR)

1. RED: Write test for valid DX file -> expect `{"valid":true,...}`
2. GREEN: Implement validation: check preamble, Type 1 tags, UID format, pixel representation
3. RED: Write test for missing required tags -> expect errors array populated
4. GREEN: Build tag-presence checker for DX IOD Type 1/2 tags
5. RED: Write test for buffer too small -> expect XPE_ERR_BUFFER_TOO_SMALL with size hint
6. GREEN: Implement buffer size check with size reporting
7. REFACTOR: Extract JSON builder utility for report generation

**Files**:
- `modules/dicom/src/dicom_validator.cpp`
- `modules/dicom/src/json_builder.h` (lightweight JSON string builder, no nlohmann dependency)
- `modules/dicom/tests/test_dicom_validator.cpp`

---

## 6. Milestone 4: DicomNetworkSCU (SWU-4.4)

**Priority**: Medium (Should)
**REQ Coverage**: REQ-DICOM-029 through REQ-DICOM-040

### Task D-9: C-STORE Implementation (RED-GREEN)

1. RED: Write test for C-STORE success (mock/stub PACS or DCMTK storescp)
2. GREEN: Implement ACSE association + C-STORE using DCMTK `DcmSCU`. Configure AE titles, timeout.
3. RED: Write test for timeout, connection refused -> expect XPE_ERR_NETWORK_FAILED
4. GREEN: Add timeout handling and error mapping

**Files**:
- `modules/dicom/src/dicom_network_scu.cpp`
- `modules/dicom/tests/test_dicom_network.cpp`

### Task D-10: C-FIND MWL Implementation (RED-GREEN)

1. RED: Write test for C-FIND with results -> expect JSON array
2. GREEN: Implement MWL query using DCMTK C-FIND SCU. Parse `queryJson` to DcmDataset keys. Convert C-FIND responses to JSON.
3. RED: Write test for empty result -> expect `[]`
4. GREEN: Handle zero-result case
5. RED: Write test for timeout -> expect XPE_ERR_NETWORK_FAILED
6. GREEN: Add timeout handling

**Files**:
- `modules/dicom/src/dicom_network_scu.cpp` (extend)
- `modules/dicom/tests/test_dicom_network.cpp` (extend)

### Task D-11: Cancel and Thread Safety (RED-GREEN-REFACTOR)

1. RED: Write test for `xpe_dicom_cancel` during C-STORE -> expect operation terminates
2. GREEN: Implement atomic cancel flag; check flag in DIMSE send loop
3. RED: Write test for concurrent cancel from different thread
4. GREEN: Use `std::atomic<bool>` for cancel flag
5. REFACTOR: Review all network code for resource leaks. Ensure association cleanup on all paths.

**Files**:
- `modules/dicom/src/dicom_network_scu.cpp` (extend)
- `modules/dicom/tests/test_dicom_network.cpp` (extend)

---

## 7. Integration and Cross-Cutting (Task D-12)

**Priority**: High

1. RED: Write ABI export verification test (`dumpbin /exports` check for 10 functions)
2. RED: Write integration test: open DICOM -> read image -> write back -> re-open -> verify
3. RED: Write memory leak test with ASan over 100 open/read/close cycles
4. RED: Write concurrent read test (2 threads, different files)
5. GREEN: Fix any issues found
6. REFACTOR: Final code cleanup, ensure all functions have DEBUG entry/exit logging

**Files**:
- `modules/dicom/tests/test_dicom_integration.cpp`
- `modules/dicom/tests/test_dicom_boundary.cpp`

---

## 8. File Summary

### Headers (Public)
- `modules/dicom/include/xpe/dicom/xpe_dicom_api.h` -- 10 exported function declarations + XpeDicomHandle forward declaration

### Implementation (Internal)
- `modules/dicom/src/dicom_handle_internal.h` -- XpeDicomHandle struct definition
- `modules/dicom/src/dcmtk_utils.h` -- DCMTK helper utilities
- `modules/dicom/src/dicom_reader.cpp` -- SWU-4.1
- `modules/dicom/src/dicom_writer.cpp` -- SWU-4.2
- `modules/dicom/src/dicom_pixel_codec.cpp` -- J2K/JPEG encode/decode
- `modules/dicom/src/dicom_uid.cpp` -- UID generation
- `modules/dicom/src/dicom_validator.cpp` -- SWU-4.3
- `modules/dicom/src/json_builder.h` -- Lightweight JSON string builder
- `modules/dicom/src/dicom_network_scu.cpp` -- SWU-4.4

### Tests
- `modules/dicom/tests/test_dicom_reader.cpp` -- SWU-4.1 tests (>= 12 cases)
- `modules/dicom/tests/test_dicom_writer.cpp` -- SWU-4.2 tests (>= 10 cases)
- `modules/dicom/tests/test_dicom_validator.cpp` -- SWU-4.3 tests (>= 6 cases)
- `modules/dicom/tests/test_dicom_network.cpp` -- SWU-4.4 tests (>= 8 cases)
- `modules/dicom/tests/test_dicom_integration.cpp` -- Integration tests (>= 6 cases)
- `modules/dicom/tests/test_dicom_boundary.cpp` -- Boundary tests (>= 4 cases)
- `modules/dicom/tests/testdata/` -- Sample DICOM files for testing

### Build
- `modules/dicom/CMakeLists.txt` -- CMake build with DCMTK + OpenJPEG + GTest

---

## 9. Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|:-----------:|:------:|------------|
| DCMTK vcpkg build issues on MSVC | Medium | High | Pin DCMTK version; test build early in Task D-0 |
| J2K round-trip not bit-exact | Low | High | Use OpenJPEG lossless mode; verify with reference images |
| Network tests flaky (external PACS dependency) | High | Medium | Use DCMTK storescp/findscp as local mock server in tests |
| DICOM tag mapping incomplete for DX IOD | Medium | Medium | Reference DICOM PS3.3 Table C.8-72 for DX required tags |
| Large file memory pressure (3072x3072 x 2 bytes = 18MB) | Low | Low | Single allocation via xpe_alloc_image; no intermediate copies |

---

*Document End -- SPEC-XPE-P1B-DICOM plan.md v1.0.0*
