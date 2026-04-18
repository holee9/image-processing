# SPEC-XPE-P1B-DICOM: DICOM I/O Module

**Document ID**: SPEC-XPE-P1B-DICOM
**Version**: 1.0.0
**Date**: 2026-04-16
**Status**: Draft
**Parent**: SPEC-XPE-MASTER v2.0.0
**Classification**: IEC 62304 Class B
**Sprint**: S1-B (Phase 1b)
**Module**: xpe_dicom.dll
**EARS Requirement Count**: 40
**Priority**: Must (SWU-4.1, SWU-4.2), Should (SWU-4.3, SWU-4.4)

---

## HISTORY

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0.0 | 2026-04-16 | MoAI (manager-spec) | Initial EARS requirements from SPEC-XPE-MASTER v2.0.0 SWI-4 |

---

## 1. Scope

Phase 1b DICOM I/O implements the complete DICOM file and network communication layer as `xpe_dicom.dll`. This module exports exactly 10 C API functions organized into 4 Software Units (SWUs).

### 1.1 In Scope

1. DICOM Part 10 file reading with DX IOD support (SWU-4.1)
2. DICOM file writing with uncompressed and JPEG 2000 Lossless transfer syntaxes (SWU-4.2)
3. DICOM conformance validation with JSON report output (SWU-4.3)
4. DICOM Network SCU: C-STORE to PACS and C-FIND Modality Worklist (SWU-4.4)
5. Opaque handle pattern for stateful DICOM reader session
6. P/Invoke-compatible C ABI with blittable types only

### 1.2 Out of Scope (Exclusions -- What NOT to Build)

- DICOM SCP (Storage/Worklist Provider) -- the module is SCU-only
- DICOM TLS/encryption -- plain ACSE association in Phase 1b; TLS deferred to Phase 2+
- GSPS (Grayscale Softcopy Presentation State, SWU-4.3 in MASTER) -- separated into SPEC-XPE-P1B-GSPS if needed
- DICOM Structured Report (SR) creation or parsing
- DICOM Query/Retrieve (C-MOVE, C-GET) -- only C-STORE and C-FIND MWL
- DICOM media (DICOMDIR / CD burning)
- Lossy JPEG 2000 compression -- only lossless J2K
- Multi-frame DICOM objects -- single-frame DX IOD only
- DICOM print (N-ACTION print management)

### 1.3 Dependencies

- **xpe_common.dll** (SPEC-XPE-P0): XpeImageBuffer, XpeImageMetadata, XpeErrorCode, xpe_alloc_image/xpe_free_image, logging subsystem
- **xpe_display.dll** (SPEC-XPE-P1B-DISP): Provides processed pixel data for DICOM write; RescaleSlope/Intercept from display pipeline
- **Third-party**: DCMTK (recommended) via vcpkg for DICOM parsing/network; OpenJPEG via vcpkg for J2K compression

### 1.4 New Types

- `XpeDicomHandle`: Opaque struct (forward-declared pointer). Allocated by `xpe_dicom_open`, freed by `xpe_dicom_close`.

### 1.5 New Error Codes

| Code | Name | Value | Description |
|------|------|:-----:|-------------|
| XPE_ERR_DICOM_INVALID | Malformed DICOM | -11 | File is not valid DICOM Part 10 or required tags missing |
| XPE_ERR_DICOM_CONFORMANCE | Conformance failure | -12 | DICOM conformance validation failed |

Existing error codes reused:
- `XPE_ERR_NETWORK_FAILED` (-10): Network/DIMSE failure (timeout, connection refused)
- `XPE_ERR_IO_FAILED` (-9): File I/O failure
- `XPE_ERR_INVALID_INPUT` (-1): NULL pointer or invalid parameter
- `XPE_ERR_BUFFER_TOO_SMALL` (-8): Output buffer insufficient
- `XPE_ERR_UNSUPPORTED_FORMAT` (-7): Unsupported transfer syntax

---

## 2. Architecture

### 2.1 Module Structure

```
xpe_dicom.dll
  |
  +-- SWU-4.1 DicomReader    (xpe_dicom_open, xpe_dicom_read_image, xpe_dicom_get_metadata, xpe_dicom_close)
  +-- SWU-4.2 DicomWriter    (xpe_dicom_write, xpe_dicom_write_j2k)
  +-- SWU-4.3 DicomValidator  (xpe_dicom_validate)
  +-- SWU-4.4 DicomNetworkSCU (xpe_dicom_cstore, xpe_dicom_cfind_mwl, xpe_dicom_cancel)
```

### 2.2 Handle Lifecycle

```
xpe_dicom_open(path) --> XpeDicomHandle*
  |
  +-- xpe_dicom_read_image(handle, outImg)     [extract pixel data]
  +-- xpe_dicom_get_metadata(handle, outMeta)  [extract metadata]
  |
xpe_dicom_close(handle) --> free all resources
```

### 2.3 Data Flow

```
[DICOM File on disk]
  --> xpe_dicom_open (parse Part 10, decompress if needed)
  --> xpe_dicom_read_image (extract uint16 pixel data into XpeImageBuffer)
  --> xpe_dicom_get_metadata (extract patient/study/series into XpeImageMetadata)
  --> [Pre-processing pipeline: xpe_preprocess.dll]
  --> [Enhancement pipeline: xpe_enhance_basic.dll]
  --> [Display pipeline: xpe_display.dll]
  --> xpe_dicom_write / xpe_dicom_write_j2k (embed processed image + metadata)
  --> xpe_dicom_cstore (send to PACS)
```

---

## 3. EARS Format Requirements

### 3.1 DicomReader (SWU-4.1 / SUP-04)

**REQ-DICOM-001**: WHEN `xpe_dicom_open` is called with a valid DICOM Part 10 file path, the system SHALL parse the preamble (128 bytes + "DICM" magic), meta-information header, and dataset, and return an opaque `XpeDicomHandle` pointer via `outHandle`.

**REQ-DICOM-002**: IF the file does not exist or cannot be opened for reading, THEN the system SHALL return `XPE_ERR_IO_FAILED` and set `*outHandle` to NULL.

**REQ-DICOM-003**: IF the file is not a valid DICOM Part 10 file (missing preamble, invalid magic bytes, or corrupted meta-information), THEN the system SHALL return `XPE_ERR_DICOM_INVALID` and set `*outHandle` to NULL.

**REQ-DICOM-004**: The system SHALL support the following Transfer Syntaxes for reading:
- 1.2.840.10008.1.2.1 (Explicit VR Little Endian)
- 1.2.840.10008.1.2.4.90 (JPEG 2000 Image Compression Lossless Only)
- 1.2.840.10008.1.2.4.70 (JPEG Lossless, Non-Hierarchical, First-Order Prediction)

**REQ-DICOM-005**: IF the file uses an unsupported Transfer Syntax, THEN the system SHALL return `XPE_ERR_UNSUPPORTED_FORMAT`.

**REQ-DICOM-006**: WHEN `xpe_dicom_read_image` is called with a valid handle, the system SHALL extract pixel data from the DICOM dataset and populate `outImg` as an `XpeImageBuffer` with format `XPE_PIXEL_UINT16`, setting width, height, bitsAllocated, and bitsStored from the DICOM Pixel Data attributes (0028,0010), (0028,0011), (0028,0100), (0028,0101).

**REQ-DICOM-007**: The system SHALL allocate the pixel data buffer internally via `xpe_alloc_image` and transfer ownership to the caller. The caller SHALL free the buffer via `xpe_free_image`.

**REQ-DICOM-008**: IF the DICOM file contains JPEG 2000 or JPEG Lossless compressed pixel data, THEN the system SHALL decompress the data to raw uint16 before populating `outImg`.

**REQ-DICOM-009**: WHEN `xpe_dicom_get_metadata` is called with a valid handle, the system SHALL extract the following DICOM tags and populate the `XpeImageMetadata` struct:
- (0010,0020) Patient ID --> stored internally (not in XpeImageMetadata; available via handle)
- (0020,000D) Study Instance UID --> stored internally
- (0020,000E) Series Instance UID --> stored internally
- (0008,0060) Modality --> stored internally
- (0018,0015) Body Part Examined --> `outMeta->bodyPart`
- (0018,0060) KVP --> `outMeta->kVp`
- (0018,1152) Exposure (mAs) --> `outMeta->mAs`
- (0018,1110) Distance Source to Detector (SID) --> `outMeta->SID_mm`
- (0028,0030) Pixel Spacing --> `outMeta->pixelPitch_mm` (first value)
- (0008,0032) Acquisition Time --> `outMeta->acquisitionTime` (epoch ms)

**REQ-DICOM-010**: IF a DICOM tag listed in REQ-DICOM-009 is absent from the dataset, THEN the system SHALL populate the corresponding field with a default value (empty string for char arrays, 0.0f for floats, 0 for integers) and SHALL NOT return an error.

**REQ-DICOM-011**: WHEN `xpe_dicom_close` is called with a valid handle, the system SHALL free all internal resources associated with the handle. After close, the handle SHALL be invalid and any subsequent use SHALL be undefined behavior.

**REQ-DICOM-012**: IF `xpe_dicom_close` is called with a NULL handle, THEN the system SHALL perform no operation (no-op, no crash).

### 3.2 DicomWriter (SWU-4.2 / SUP-04)

**REQ-DICOM-013**: WHEN `xpe_dicom_write` is called, the system SHALL create a valid DICOM Part 10 file at `filePath` with the following characteristics:
- SOP Class UID: 1.2.840.10008.5.1.4.1.1.1.1 (Digital X-Ray Image Storage -- For Presentation)
- Transfer Syntax: 1.2.840.10008.1.2.1 (Explicit VR Little Endian)
- Pixel data from `img` (XpeImageBuffer)
- Metadata from `meta` (XpeImageMetadata)

**REQ-DICOM-014**: The system SHALL generate unique SOP Instance UID and Series Instance UID for each written file using a DICOM-compliant UID generation scheme.

**REQ-DICOM-015**: The system SHALL embed the following DICOM tags from `XpeImageMetadata`:
- (0018,0015) Body Part Examined <-- `meta->bodyPart`
- (0018,0060) KVP <-- `meta->kVp`
- (0018,1152) Exposure <-- `meta->mAs`
- (0018,1110) Distance Source to Detector <-- `meta->SID_mm`
- (0028,0030) Pixel Spacing <-- `meta->pixelPitch_mm`
- (0008,0032) Acquisition Time <-- `meta->acquisitionTime`

**REQ-DICOM-016**: The system SHALL set Pixel Data attributes (Rows, Columns, Bits Allocated, Bits Stored, High Bit, Pixel Representation, Samples Per Pixel, Photometric Interpretation) correctly based on `img` properties.

**REQ-DICOM-017**: IF `filePath` cannot be created or written, THEN the system SHALL return `XPE_ERR_IO_FAILED`.

**REQ-DICOM-018**: IF `img` or `meta` is NULL, THEN the system SHALL return `XPE_ERR_INVALID_INPUT`.

**REQ-DICOM-019**: WHEN `xpe_dicom_write_j2k` is called, the system SHALL create a DICOM Part 10 file with:
- Transfer Syntax: 1.2.840.10008.1.2.4.90 (JPEG 2000 Image Compression Lossless Only)
- Pixel data compressed using JPEG 2000 lossless via OpenJPEG
- All other attributes identical to `xpe_dicom_write`

**REQ-DICOM-020**: The JPEG 2000 lossless compression SHALL produce bit-exact reconstruction when decompressed; the system SHALL verify round-trip integrity during development testing.

**REQ-DICOM-021**: IF JPEG 2000 compression fails (encoder error), THEN the system SHALL return `XPE_ERR_PROCESSING_FAILED` and SHALL NOT create a partial file.

**REQ-DICOM-022**: The system SHALL set Rescale Slope (0028,1053) and Rescale Intercept (0028,1052) tags to 1.0 and 0.0 respectively (identity mapping) unless the caller provides override values via the metadata path.

### 3.3 DicomValidator (SWU-4.3 / SUP-04)

**REQ-DICOM-023**: WHEN `xpe_dicom_validate` is called with a valid DICOM file path, the system SHALL check the file for DICOM DX IOD conformance and write a JSON validation report to `outReportJson`.

**REQ-DICOM-024**: The validation SHALL check the following conformance criteria:
- DICOM Part 10 preamble and magic present
- Required Type 1 tags for DX IOD present and non-empty (Patient Name, Patient ID, Study Instance UID, Series Instance UID, SOP Instance UID, Modality, Rows, Columns, Bits Allocated, Bits Stored, Pixel Data)
- UID format correct (dot-separated numeric, max 64 characters)
- Pixel representation consistent with declared format

**REQ-DICOM-025**: The JSON report SHALL contain:
- `"valid"`: boolean (true if all checks pass)
- `"errors"`: array of `{"tag": "GGGG,EEEE", "message": "description"}` for failed checks
- `"warnings"`: array of `{"tag": "GGGG,EEEE", "message": "description"}` for non-critical issues

**REQ-DICOM-026**: IF the file is not a valid DICOM file (cannot be parsed at all), THEN the system SHALL return `XPE_ERR_DICOM_INVALID` and write `{"valid":false,"errors":[{"tag":"","message":"Not a valid DICOM file"}],"warnings":[]}` to the report buffer.

**REQ-DICOM-027**: IF `reportBufLen` is insufficient to hold the complete JSON report, THEN the system SHALL return `XPE_ERR_BUFFER_TOO_SMALL` and write the required buffer size to the first 4 bytes of `outReportJson` (as uint32_t).

**REQ-DICOM-028**: IF `filePath` or `outReportJson` is NULL, THEN the system SHALL return `XPE_ERR_INVALID_INPUT`.

### 3.4 DicomNetworkSCU (SWU-4.4 / SUP-04)

**REQ-DICOM-029**: WHEN `xpe_dicom_cstore` is called, the system SHALL establish a DICOM ACSE association with the remote AE at `host:port`, negotiate the appropriate Transfer Syntax, and send the DICOM file at `filePath` via C-STORE DIMSE message.

**REQ-DICOM-030**: The system SHALL use the calling AE title `aet` for association negotiation. The called AE title SHALL default to "ANY-SCP" unless embedded in the host string (format: "CALLED_AE@host").

**REQ-DICOM-031**: IF the association cannot be established within `timeoutMs` milliseconds, THEN the system SHALL return `XPE_ERR_NETWORK_FAILED`.

**REQ-DICOM-032**: IF the C-STORE operation fails (remote rejection, DIMSE failure, or network error), THEN the system SHALL return `XPE_ERR_NETWORK_FAILED` and post a WARNING alert with the failure reason string.

**REQ-DICOM-033**: WHEN `xpe_dicom_cstore` completes successfully (C-STORE RSP status 0x0000 = Success), the system SHALL return `XPE_OK`.

**REQ-DICOM-034**: WHEN `xpe_dicom_cfind_mwl` is called, the system SHALL establish a DICOM ACSE association with the remote AE at `host:port` and execute a C-FIND request on the Modality Worklist Information Model using the query keys provided in `queryJson`.

**REQ-DICOM-035**: The `queryJson` input SHALL be a JSON object with DICOM tag keyword keys and string values for matching. Supported keys:
- `"PatientID"` (0010,0020)
- `"PatientName"` (0010,0010)
- `"ScheduledStationAETitle"` (0040,0001)
- `"Modality"` (0008,0060)
- `"ScheduledProcedureStepStartDate"` (0040,0002)

**REQ-DICOM-036**: The system SHALL write C-FIND results as a JSON array to `outJson`, where each element is a JSON object containing the matched DICOM tags as key-value pairs.

**REQ-DICOM-037**: IF no matching worklist entries are found, THEN the system SHALL write `[]` (empty JSON array) to `outJson` and return `XPE_OK`.

**REQ-DICOM-038**: IF the C-FIND operation fails or times out, THEN the system SHALL return `XPE_ERR_NETWORK_FAILED`.

**REQ-DICOM-039**: WHEN `xpe_dicom_cancel` is called, the system SHALL signal cancellation to any in-progress C-STORE or C-FIND operation. The cancelled operation SHALL return `XPE_ERR_PROCESSING_FAILED` with a cancel indicator in the alert message.

**REQ-DICOM-040**: The cancellation mechanism SHALL be thread-safe. `xpe_dicom_cancel` MAY be called from a different thread than the one executing the network operation.

### 3.5 Cross-Cutting Requirements

**REQ-DICOM-041**: All 10 exported functions SHALL use C linkage (`extern "C"`), `__cdecl` calling convention, and blittable types only. All pointer parameters SHALL use basic C types compatible with .NET P/Invoke marshalling.

**REQ-DICOM-042**: The system SHALL NOT throw C++ exceptions across the DLL ABI boundary. All exceptions from DCMTK or OpenJPEG SHALL be caught internally and converted to `XpeErrorCode` return values.

**REQ-DICOM-043**: Each function SHALL log entry/exit at DEBUG level and error conditions at ERROR level via the logging subsystem in xpe_common.dll.

**REQ-DICOM-044**: The system SHALL NOT leak memory on any code path. DICOM dataset objects, network association objects, and compressed data buffers SHALL be freed on both success and error paths.

**REQ-DICOM-045**: DicomReader functions (`xpe_dicom_read_image`, `xpe_dicom_get_metadata`) SHALL be reentrant when called with independent handles. Two threads MAY read different DICOM files concurrently.

**REQ-DICOM-046**: DicomNetworkSCU functions (`xpe_dicom_cstore`, `xpe_dicom_cfind_mwl`) SHALL NOT be called concurrently on the same association. The caller is responsible for serializing network operations.

---

## 4. API Surface (10 functions)

`xpe_dicom.dll` SHALL export exactly 10 functions with C linkage:

| # | Function | SWU | Category | Return |
|---|----------|-----|----------|--------|
| 1 | `xpe_dicom_open` | SWU-4.1 | Reader | `xpe_error_t` |
| 2 | `xpe_dicom_read_image` | SWU-4.1 | Reader | `xpe_error_t` |
| 3 | `xpe_dicom_get_metadata` | SWU-4.1 | Reader | `xpe_error_t` |
| 4 | `xpe_dicom_close` | SWU-4.1 | Reader | `void` |
| 5 | `xpe_dicom_write` | SWU-4.2 | Writer | `xpe_error_t` |
| 6 | `xpe_dicom_write_j2k` | SWU-4.2 | Writer | `xpe_error_t` |
| 7 | `xpe_dicom_validate` | SWU-4.3 | Validator | `xpe_error_t` |
| 8 | `xpe_dicom_cstore` | SWU-4.4 | Network | `xpe_error_t` |
| 9 | `xpe_dicom_cfind_mwl` | SWU-4.4 | Network | `xpe_error_t` |
| 10 | `xpe_dicom_cancel` | SWU-4.4 | Network | `void` |

### 4.1 Function Signatures (Normative)

```c
// SWU-4.1: DicomReader
XPE_API xpe_error_t xpe_dicom_open(const char* filePath, XpeDicomHandle** outHandle);
XPE_API xpe_error_t xpe_dicom_read_image(XpeDicomHandle* handle, XpeImageBuffer* outImg);
XPE_API xpe_error_t xpe_dicom_get_metadata(XpeDicomHandle* handle, XpeImageMetadata* outMeta);
XPE_API void        xpe_dicom_close(XpeDicomHandle* handle);

// SWU-4.2: DicomWriter
XPE_API xpe_error_t xpe_dicom_write(const char* filePath, const XpeImageBuffer* img, const XpeImageMetadata* meta);
XPE_API xpe_error_t xpe_dicom_write_j2k(const char* filePath, const XpeImageBuffer* img, const XpeImageMetadata* meta);

// SWU-4.3: DicomValidator
XPE_API xpe_error_t xpe_dicom_validate(const char* filePath, char* outReportJson, uint32_t reportBufLen);

// SWU-4.4: DicomNetworkSCU
XPE_API xpe_error_t xpe_dicom_cstore(const char* host, uint16_t port, const char* aet, const char* filePath, uint32_t timeoutMs);
XPE_API xpe_error_t xpe_dicom_cfind_mwl(const char* host, uint16_t port, const char* aet, const char* queryJson, char* outJson, uint32_t outBufLen, uint32_t timeoutMs);
XPE_API void        xpe_dicom_cancel(void);
```

---

## 5. Performance Budgets

| Operation | Target | Image Size | REQ Trace |
|-----------|--------|:----------:|-----------|
| DICOM file open + parse | <= 100ms | 3072x3072 | REQ-DICOM-001 |
| Pixel data extraction (uncompressed) | <= 50ms | 3072x3072 | REQ-DICOM-006 |
| Pixel data extraction (J2K decompress) | <= 500ms | 3072x3072 | REQ-DICOM-008 |
| Metadata extraction | <= 5ms | N/A | REQ-DICOM-009 |
| DICOM write (uncompressed) | <= 100ms | 3072x3072 | REQ-DICOM-013 |
| DICOM write (J2K compress) | <= 1000ms | 3072x3072 | REQ-DICOM-019 |
| DICOM validation | <= 200ms | 3072x3072 | REQ-DICOM-023 |
| C-STORE (LAN, excluding file read) | <= 2000ms | 3072x3072 | REQ-DICOM-029 |
| C-FIND MWL (LAN) | <= 1000ms | N/A | REQ-DICOM-034 |

---

## 6. IEC 62304 Traceability

### 6.1 SWI to SWU Mapping

| SWI | SWU | REQ Range | Phase | Priority |
|-----|-----|-----------|:-----:|:--------:|
| SWI-4 | SWU-4.1 DicomReader | REQ-DICOM-001..012 | 1b | Must (P1b-09) |
| SWI-4 | SWU-4.2 DicomWriter | REQ-DICOM-013..022 | 1b | Must (P1b-10) |
| SWI-4 | SWU-4.3 DicomValidator | REQ-DICOM-023..028 | 1b | Must |
| SWI-4 | SWU-4.4 DicomNetworkSCU | REQ-DICOM-029..040 | 1b | Should (P1b-12) |

### 6.2 Requirement to Test Matrix

| REQ ID | Description | Test File | Test Case(s) |
|--------|-------------|-----------|--------------|
| REQ-DICOM-001..003 | File open (valid, missing, invalid) | test_dicom_reader.cpp | OpenValid, OpenMissing, OpenInvalid |
| REQ-DICOM-004..005 | Transfer Syntax support | test_dicom_reader.cpp | ReadExplicitLE, ReadJ2K, ReadJPEGLL, UnsupportedTS |
| REQ-DICOM-006..008 | Pixel data extraction | test_dicom_reader.cpp | ReadImageUint16, ReadImageJ2KDecompress |
| REQ-DICOM-009..010 | Metadata extraction | test_dicom_reader.cpp | GetMetadata, GetMetadataMissingTags |
| REQ-DICOM-011..012 | Handle close | test_dicom_reader.cpp | CloseValid, CloseNull |
| REQ-DICOM-013..018 | Write uncompressed | test_dicom_writer.cpp | WriteBasic, WriteMetadata, WriteIOError, WriteNull |
| REQ-DICOM-019..022 | Write J2K | test_dicom_writer.cpp | WriteJ2KRoundTrip, J2KEncoderError, RescaleDefaults |
| REQ-DICOM-023..028 | Validation | test_dicom_validator.cpp | ValidateConformant, ValidateMissingTags, ValidateBadUID, BufferTooSmall |
| REQ-DICOM-029..033 | C-STORE | test_dicom_network.cpp | CStoreSuccess, CStoreTimeout, CStoreReject |
| REQ-DICOM-034..038 | C-FIND MWL | test_dicom_network.cpp | CFindResults, CFindEmpty, CFindTimeout |
| REQ-DICOM-039..040 | Cancel | test_dicom_network.cpp | CancelCStore, CancelThreadSafety |
| REQ-DICOM-041..046 | Cross-cutting | test_dicom_integration.cpp | ABIExport, NoExceptions, MemoryLeak, ConcurrentRead |

---

## Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0.0 | 2026-04-16 | MoAI (manager-spec) | Initial EARS requirements (46 REQs) for Sprint S1-B DICOM module |

---

*Document End -- SPEC-XPE-P1B-DICOM v1.0.0*
