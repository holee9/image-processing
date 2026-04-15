# SPEC-XPE-P1B-DICOM: Acceptance Criteria

**SPEC ID**: SPEC-XPE-P1B-DICOM
**Version**: 1.0.0
**Date**: 2026-04-16

---

## 1. Functional Acceptance

### AC-01: DICOM Read/Write Round-Trip (REQ-DICOM-001, 006, 013)

**Given** a valid DICOM DX file (Explicit VR Little Endian, 3072x3072, uint16)
**When** the file is opened with `xpe_dicom_open`, pixel data extracted with `xpe_dicom_read_image`, then written back with `xpe_dicom_write` to a new file path
**Then** re-opening the written file and extracting pixel data SHALL produce bit-identical uint16 values to the original extraction.

### AC-02: JPEG 2000 Lossless Round-Trip (REQ-DICOM-019, 020)

**Given** an XpeImageBuffer containing a 3072x3072 uint16 image and valid XpeImageMetadata
**When** the image is written with `xpe_dicom_write_j2k` and the output file is read back with `xpe_dicom_open` + `xpe_dicom_read_image`
**Then** the decompressed pixel data SHALL be bit-identical to the original XpeImageBuffer pixel data (lossless round-trip).

### AC-03: Metadata Preservation (REQ-DICOM-009, 015)

**Given** an XpeImageMetadata with all fields populated (bodyPart="CHEST", kVp=80.0, mAs=2.5, SID_mm=1800.0, pixelPitch_mm=0.148)
**When** written via `xpe_dicom_write` and read back via `xpe_dicom_get_metadata`
**Then** all numeric fields SHALL match within float32 precision and bodyPart SHALL match exactly.

### AC-04: Transfer Syntax Support (REQ-DICOM-004, 005)

**Given** DICOM files encoded in each of the three supported Transfer Syntaxes (Explicit LE, JPEG 2000 Lossless, JPEG Lossless First-Order)
**When** each file is opened with `xpe_dicom_open` and pixel data read with `xpe_dicom_read_image`
**Then** all three SHALL succeed with `XPE_OK` and produce valid uint16 XpeImageBuffer output.

**Given** a DICOM file encoded with Implicit VR Little Endian (unsupported)
**When** opened with `xpe_dicom_open`
**Then** the system SHALL return `XPE_ERR_UNSUPPORTED_FORMAT`.

### AC-05: DICOM Conformance Validation (REQ-DICOM-023, 024, 025)

**Given** a DICOM DX file written by `xpe_dicom_write` (known-conformant)
**When** validated with `xpe_dicom_validate`
**Then** the JSON report SHALL contain `"valid": true` with empty errors array.

**Given** a DICOM file with Patient ID tag removed
**When** validated with `xpe_dicom_validate`
**Then** the JSON report SHALL contain `"valid": false` with at least one error referencing tag `"0010,0020"`.

### AC-06: C-STORE to PACS (REQ-DICOM-029, 031, 033)

**Given** a local DCMTK storescp mock running on localhost:11112 with AE title "TESTPACS"
**When** `xpe_dicom_cstore` is called with a valid DICOM file, host="localhost", port=11112, aet="TESTSCU", timeoutMs=5000
**Then** the function SHALL return `XPE_OK` and the mock server SHALL have received the file.

**Given** no server running on the target host:port
**When** `xpe_dicom_cstore` is called with timeoutMs=1000
**Then** the function SHALL return `XPE_ERR_NETWORK_FAILED` within 2000ms.

### AC-07: C-FIND Modality Worklist (REQ-DICOM-034, 035, 036, 037)

**Given** a local DCMTK wlmscpfs mock with 3 worklist entries matching Modality="DX"
**When** `xpe_dicom_cfind_mwl` is called with queryJson=`{"Modality":"DX"}`
**Then** `outJson` SHALL contain a JSON array with 3 elements, each containing at minimum PatientID and PatientName keys.

**Given** a query that matches no worklist entries
**When** `xpe_dicom_cfind_mwl` is called
**Then** `outJson` SHALL contain `[]` and the function SHALL return `XPE_OK`.

### AC-08: Cancel Network Operation (REQ-DICOM-039, 040)

**Given** a C-STORE operation in progress on thread A (sending a large file to a slow mock server)
**When** `xpe_dicom_cancel` is called from thread B
**Then** the C-STORE operation on thread A SHALL terminate and return `XPE_ERR_PROCESSING_FAILED` within 1000ms of the cancel call.

---

## 2. Error Handling Acceptance

### AC-09: NULL Parameter Protection (REQ-DICOM-002, 012, 018, 028)

**Given** NULL values for required parameters in each of the 10 exported functions
**When** the function is called
**Then** functions returning `xpe_error_t` SHALL return `XPE_ERR_INVALID_INPUT`; `xpe_dicom_close(NULL)` and `xpe_dicom_cancel()` SHALL be no-ops without crash.

### AC-10: Invalid DICOM File (REQ-DICOM-003)

**Given** a file that is not DICOM (e.g., a PNG file renamed to .dcm)
**When** opened with `xpe_dicom_open`
**Then** the system SHALL return `XPE_ERR_DICOM_INVALID` and `*outHandle` SHALL be NULL.

### AC-11: Buffer Too Small for Validation Report (REQ-DICOM-027)

**Given** a buffer of 10 bytes for `outReportJson` and a DICOM file with multiple validation errors
**When** `xpe_dicom_validate` is called
**Then** the system SHALL return `XPE_ERR_BUFFER_TOO_SMALL`.

---

## 3. Quality Gate Acceptance

### AC-12: ABI Export Verification

**Given** the built `xpe_dicom.dll`
**When** `dumpbin /exports xpe_dicom.dll` is executed
**Then** exactly 10 exported functions SHALL be listed matching the API surface table.

### AC-13: P/Invoke Compatibility

**Given** a C# test harness calling all 10 exported functions via P/Invoke
**When** executed on .NET 6+
**Then** all calls SHALL complete without marshalling errors. XpeImageBuffer and XpeImageMetadata struct layouts SHALL match between C++ (Pack=8) and C# (StructLayout Pack=8).

### AC-14: No Memory Leaks

**Given** a test loop performing 100 cycles of: open -> read_image -> get_metadata -> close
**When** executed under AddressSanitizer (ASan)
**Then** zero memory leaks SHALL be reported.

### AC-15: No C++ Exceptions Across ABI (REQ-DICOM-042)

**Given** deliberately corrupted DICOM files and invalid network endpoints
**When** all 10 functions are called with these inputs
**Then** no C++ exception SHALL propagate to the caller. All errors SHALL be reported via `XpeErrorCode` return values.

### AC-16: Concurrent Read Safety (REQ-DICOM-045)

**Given** two DICOM files and two threads
**When** each thread concurrently opens, reads, and closes a different DICOM file
**Then** both operations SHALL complete without data corruption, race conditions, or crashes.

---

## 4. Performance Acceptance

### AC-17: File I/O Performance

- [ ] DICOM open + parse <= 100ms for 3072x3072 uncompressed
- [ ] Pixel data extraction <= 50ms for 3072x3072 uncompressed
- [ ] J2K decompression <= 500ms for 3072x3072
- [ ] DICOM write (uncompressed) <= 100ms for 3072x3072
- [ ] DICOM write (J2K) <= 1000ms for 3072x3072
- [ ] Validation <= 200ms for 3072x3072

### AC-18: Network Performance (LAN)

- [ ] C-STORE <= 2000ms for 3072x3072 (excluding file read, LAN conditions)
- [ ] C-FIND MWL <= 1000ms (LAN conditions)

---

## 5. Test Plan Summary

| Test File | SWU | Min Cases | Coverage Target |
|-----------|-----|:---------:|:---------------:|
| test_dicom_reader.cpp | SWU-4.1 | >= 12 | >= 90% |
| test_dicom_writer.cpp | SWU-4.2 | >= 10 | >= 90% |
| test_dicom_validator.cpp | SWU-4.3 | >= 6 | >= 90% |
| test_dicom_network.cpp | SWU-4.4 | >= 8 | >= 80% |
| test_dicom_integration.cpp | Cross-cutting | >= 6 | >= 80% |
| test_dicom_boundary.cpp | Boundary | >= 4 | >= 80% |
| **Total** | | **>= 46** | **>= 90%** |

---

## 6. Definition of Done

- [ ] All 10 C API functions exported and callable
- [ ] All 46 EARS requirements (REQ-DICOM-001..046) implemented and tested
- [ ] >= 90% statement coverage, >= 80% branch coverage
- [ ] Round-trip (read-write-read) pixel-exact for uncompressed and J2K lossless
- [ ] DICOM DX IOD conformance verified by validator
- [ ] C-STORE and C-FIND MWL functional with DCMTK mock server
- [ ] Zero memory leaks under ASan (100-cycle test)
- [ ] Zero C++ exceptions crossing DLL boundary
- [ ] `dumpbin /exports` confirms exactly 10 functions
- [ ] P/Invoke round-trip test passing from C#
- [ ] All performance budgets met on reference hardware
- [ ] IEC 62304 traceability: every REQ mapped to test case

---

*Document End -- SPEC-XPE-P1B-DICOM acceptance.md v1.0.0*
