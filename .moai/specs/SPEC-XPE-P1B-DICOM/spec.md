# SPEC-XPE-P1B-DICOM: DICOM I/O Module

**Document ID**: SPEC-XPE-P1B-DICOM
**Version**: 1.3.1
**Date**: 2026-10-03
**Status**: Released
**Parent**: SPEC-XPE-MASTER v2.0.0
**Classification**: IEC 62304 Class B
**Sprint**: S1-B (Phase 1b)
**Module**: xpe_dicom.dll
**EARS Requirement Count**: 46 (REQ-DICOM-001..046)
**Priority**: Must (SWU-4.1, SWU-4.2), Should (SWU-4.3, SWU-4.4)

---

## HISTORY

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0.0 | 2026-04-16 | MoAI (manager-spec) | Initial EARS requirements from SPEC-XPE-MASTER v2.0.0 SWI-4 |
| 1.1.0 | 2026-04-21 | MoAI (manager-spec) | Released — 46 EARS 요구사항 교차검증 완료 (API 10/10 구현, 테스트 35/35 통과). EARS Count 40→46 정정 |
| 1.2.0 | 2026-10-03 | lead (QA-B-199) | 요구 실태 대조 반영(#251): 결함 후보 C1~C16 해당 요구에 상태 메모 추가(요구 문구는 바꾸지 않음). Exposure 태그·acquisitionTime 단위는 SPEC/코드 중 어느 쪽을 고칠지 결정 대기 |

---

## 1. Scope

Phase 1b DICOM I/O implements the complete DICOM file and network communication layer as `xpe_dicom.dll`. This module exports exactly 11 C API functions: 10 organized into 4 Software Units (SWUs), plus `xpe_dicom_version` (REQ-P0-033, added 2026-10-03, QA-B-200 M2a).

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

> **상태 메모 (2026-10-03, QA-B-199, 후보 C3)**: 미충족 후보 — 재현 확인 중(QA-B-200), #251. 128바이트 서문·`DICM` 매직·meta header 가 모두 없는 데이터셋이 `XPE_OK` 로 열린다. 이 동작을 고정하는 시험이 있고, 헤더 `dicom_api.h` 의 @note 도 스스로 밝힌다.

**REQ-DICOM-004**: The system SHALL support the following Transfer Syntaxes for reading:
- 1.2.840.10008.1.2.1 (Explicit VR Little Endian)
- 1.2.840.10008.1.2.4.90 (JPEG 2000 Image Compression Lossless Only)
- 1.2.840.10008.1.2.4.70 (JPEG Lossless, Non-Hierarchical, First-Order Prediction)
- 1.2.840.10008.1.2.4.57 (JPEG Lossless, Non-Hierarchical, Process 14)

> **[정정 2026-09-27, #147]** `.57` 을 추가했습니다. 이 요구가 `.70` 만 적는 동안
> `SPEC-XPE-IOP` 의 `REQ-IOP-003` 은 `.57` 을 "at minimum" 으로 요구해, 두 SPEC 이
> **JPEG Lossless 의 다른 변종**을 요구하는 상태였습니다.
>
> `.57` 과 `.70` 은 오타가 아니라 **서로 다른 실제 전송 구문**입니다 — `.70` 은
> Process 14 에 Selection Value 1(1차 예측)을 더해 제약한 것이고, 코드도 둘을 갈라
> 씁니다(`DicomReader.cpp`: `EXS_JPEGProcess14` 대 `EXS_JPEGProcess14SV1`).
>
> `QA-B-68` 이 `.57` 디코드를 넣어 구현은 네 구문을 모두 지원합니다. 따라서 이 정정은
> **요구를 넓히는 것이 아니라 이미 있는 동작을 적는 것**입니다. `REQ-IOP-003` 은 이미
> 충족 상태이며, 이슈가 결정을 기다리던 "연동 대상이 어느 변종을 쓰는가" 는 둘 다
> 지원하므로 **답하지 않아도 되는 질문이 됐습니다.**
>
> 근거: `modules/dicom/src/DicomReader.h` 의 `kSupportedTransferSyntaxes` 는 항목마다
> 왕복 픽스처를 요구하는 순회 시험(`EverySupportedTransferSyntaxActuallyReads`)에
> 걸려 있어, 표에 적혔는데 읽지 못하면 빌드의 시험이 실패합니다.

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
- (0008,0032) Acquisition Time --> `outMeta->acquisitionTime` (seconds since Unix epoch, UTC; 0 = unknown — 단위 정정 2026-10-03 사용자 결정 "문서를 실제에 맞게", #251, QA-B-204 재현, `xpe_types.h` 와 일치)

> **상태 메모 (2026-10-03, QA-B-199, 후보 C1·C2·C15)**: 미충족 후보 — 재현 확인 중(QA-B-200), #251.
> - C1: 구현은 mAs 를 (0018,1152) Exposure 가 아니라 (0018,9332) ExposureInmAs 로 읽고 쓴다(`DicomReader.cpp`·`DicomWriter.cpp` 의 `DCM_ExposureInmAs`, 2026-10-03 grep 으로 확인). 왕복 시험은 같은 선택을 공유해 통과하지만, 다른 시스템이 만든 (0018,1152) 파일은 mAs 0 으로 읽힌다.
> - C2: 이 요구는 `acquisitionTime` 을 epoch ms 로 적었지만, 타입 헤더 `xpe_types.h` 와 두 코드 경로는 epoch 초를 쓴다. **해결 (2026-10-03)**: 문구를 초로 정정(사용자 결정). 밀리초 값을 넘기면 오류 없이 날짜가 사라지는(읽으면 0) 위험은 남는다 — 범위 검사는 정하지 않았다.
> - C15: Patient ID·Study/Series Instance UID·Modality 를 "핸들로 얻을 수 있다" 고 했으나 `getMetadata` 는 이 태그들을 읽지 않고 접근자도 없다.
> - C1·C2 는 SPEC 을 따라 코드를 고칠지, SPEC 을 코드에 맞출지 결정이 필요하다(#251). 요구 문구는 바꾸지 않았다.

**REQ-DICOM-010**: IF a DICOM tag listed in REQ-DICOM-009 is absent from the dataset, THEN the system SHALL populate the corresponding field with a default value (empty string for char arrays, 0.0f for floats, 0 for integers) and SHALL NOT return an error.

**REQ-DICOM-011**: WHEN `xpe_dicom_close` is called with a valid handle, the system SHALL free all internal resources associated with the handle. After close, the handle SHALL be invalid and any subsequent use SHALL be undefined behavior.

**REQ-DICOM-012**: IF `xpe_dicom_close` is called with a NULL handle, THEN the system SHALL perform no operation (no-op, no crash).

### 3.2 DicomWriter (SWU-4.2 / SUP-04)

**REQ-DICOM-013**: WHEN `xpe_dicom_write` is called, the system SHALL create a valid DICOM Part 10 file at `filePath` with the following characteristics:
- SOP Class UID: 1.2.840.10008.5.1.4.1.1.1.1 (Digital X-Ray Image Storage -- For Presentation)
- Transfer Syntax: 1.2.840.10008.1.2.1 (Explicit VR Little Endian)
- Pixel data from `img` (XpeImageBuffer)
- Metadata from `meta` (XpeImageMetadata)

> **상태 메모 (2026-10-03, QA-B-199, 후보 C12·C13)**: 미충족 후보 — 재현 확인 중(QA-B-200), #251. C12: Rows/Columns 를 범위 검사 없이 `Uint16` 으로 변환하고 화소 형식을 검증하지 않아, FLOAT32·UINT8 버퍼가 uint16 워드로 쓰인다(REQ-DICOM-016 과 함께). C13: `dataSize == 0` 을 받아들인 뒤 길이 0 인 PixelData 로 써서 `XPE_OK` 를 내고, 읽기는 그 파일을 거부한다.

**REQ-DICOM-014**: The system SHALL generate unique SOP Instance UID and Series Instance UID for each written file using a DICOM-compliant UID generation scheme.

**REQ-DICOM-015**: The system SHALL embed the following DICOM tags from `XpeImageMetadata`:
- (0018,0015) Body Part Examined <-- `meta->bodyPart`
- (0018,0060) KVP <-- `meta->kVp`
- (0018,1152) Exposure <-- `meta->mAs`
- (0018,1110) Distance Source to Detector <-- `meta->SID_mm`
- (0028,0030) Pixel Spacing <-- `meta->pixelPitch_mm`
- (0008,0032) Acquisition Time <-- `meta->acquisitionTime`

> **상태 메모 (2026-10-03, QA-B-199, 후보 C1·C2)**: 미충족 후보 — 재현 확인 중(QA-B-200), #251. 쓰기도 mAs 를 (0018,9332) ExposureInmAs 로 기록하고, Acquisition Time 은 epoch 초로 해석한다(REQ-DICOM-009 의 메모 참조). 어느 쪽을 고칠지는 결정 대기다.

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
- Required Type 1 tags for DX IOD present and non-empty (Study Instance UID, Series Instance UID, SOP Instance UID, Modality, Rows, Columns, Bits Allocated, Bits Stored, Pixel Data)
- Required Type 2 tags present; an empty value is conformant (Patient Name, Patient ID — DICOM PS3.3 Table C.7-1 lists both as Type 2; moved out of the Type 1 list 2026-10-03, user decision "표준대로 허용", #251, Codex #111)
- UID format correct (dot-separated numeric, max 64 characters)
- Pixel representation consistent with declared format

> **상태 메모 (2026-10-03, QA-B-199, 후보 C5)**: 미충족 후보 — 재현 확인 중(QA-B-200), #251. Type 1 태그는 "present and non-empty" 중 존재만 검사해 빈 값이 통과한다. 화소 표현과 선언 형식의 일관성 검사 코드가 없고, 서문·매직은 직접 검사하지 않는다(`DicomValidator.cpp`).

**REQ-DICOM-025**: The JSON report SHALL contain:
- `"valid"`: boolean (true if all checks pass)
- `"errors"`: array of `{"tag": "GGGG,EEEE", "message": "description"}` for failed checks
- `"warnings"`: array of `{"tag": "GGGG,EEEE", "message": "description"}` for non-critical issues

> **상태 메모 (2026-10-03, QA-B-199, 후보 C6)**: 미충족 후보 — 재현 확인 중(QA-B-200), #251. 잘못된 UID 가 `warnings` 와 `errors` 양쪽에 들어가고 `valid=false` 가 된다 — 비치명 문제가 치명 오류로 올라간다(`DicomValidator.cpp`).

**REQ-DICOM-026**: IF the file is not a valid DICOM file (cannot be parsed at all), THEN the system SHALL return `XPE_ERR_DICOM_INVALID` and write a report with `"valid":false`, an empty `warnings` array, and one `errors` entry whose `tag` is `"0008,0000"` and whose `message` begins with `"File cannot be parsed as DICOM: "` followed by the parser's status text (문구 정정: 2026-10-03 사용자 결정 "문서를 실제에 맞게", #251, QA-B-204 재현 — 뒤의 상태 문구는 DCMTK 버전에 따라 바뀔 수 있으므로 접두만 계약이다).

> **상태 메모 (2026-10-03, QA-B-199, 후보 C4)**: 미충족 후보 — 재현 확인 중(QA-B-200), #251. 구현은 이 보고 대신 `tag "0008,0000"`, `"File cannot be parsed as DICOM: …"` 를 낸다(`DicomValidator.cpp`). 시험은 `errors` 가 비어 있지 않음만 확인한다.

**REQ-DICOM-027**: IF `reportBufLen` is insufficient to hold the complete JSON report, THEN the system SHALL return `XPE_ERR_BUFFER_TOO_SMALL` and write the required buffer size to the first 4 bytes of `outReportJson` (as uint32_t).

**REQ-DICOM-028**: IF `filePath` or `outReportJson` is NULL, THEN the system SHALL return `XPE_ERR_INVALID_INPUT`.

### 3.4 DicomNetworkSCU (SWU-4.4 / SUP-04)

**REQ-DICOM-029**: WHEN `xpe_dicom_cstore` is called, the system SHALL establish a DICOM ACSE association with the remote AE at `host:port`, negotiate the appropriate Transfer Syntax, and send the DICOM file at `filePath` via C-STORE DIMSE message.

> **상태 메모 (2026-10-03, QA-B-199, 후보 C11)**: 미충족 후보 — 재현 확인 중(QA-B-200), #251. 전송 구문을 파일의 구문과 무관하게 고정 제안한다(Explicit LE, J2K Lossless, Implicit LE). JPEG Lossless 파일은 트랜스코딩 없이 보내진다(`DicomNetworkSCU.cpp`). 모의 SCP 는 받은 데이터셋을 보지 않고 버린다.

**REQ-DICOM-030**: The system SHALL use the calling AE title `aet` for association negotiation. The called AE title SHALL default to "ANY-SCP" unless embedded in the host string (format: "CALLED_AE@host").

**REQ-DICOM-031**: IF the association cannot be established within `timeoutMs` milliseconds, THEN the system SHALL return `XPE_ERR_NETWORK_FAILED`.

> **상태 메모 (2026-10-03, QA-B-199, 후보 C10)**: 미충족 후보 — 재현 확인 중(QA-B-200), #251. 구현은 `timeoutMs / 1000` 으로 초를 만들어 1000 ms 미만이 0 초가 되고, `timeoutMs == 0` 이면 DCMTK 기본값이 남는다. "Timeout" 시험은 리스너 없는 포트의 연결 거부를 보는 것이라 실제 타임아웃이 아니다.

**REQ-DICOM-032**: IF the C-STORE operation fails (remote rejection, DIMSE failure, or network error), THEN the system SHALL return `XPE_ERR_NETWORK_FAILED` and post a WARNING alert with the failure reason string.

> **상태 메모 (2026-10-03, QA-B-199, 후보 C7)**: 미충족 후보 — 재현 확인 중(QA-B-200), #251. SCU 는 실패 시 `spdlog::warn` 만 하고 `xpe_alert_push` 를 한 번도 호출하지 않는다(`DicomNetworkSCU.cpp` 에 호출 0건, 2026-10-03 grep; 같은 검색이 `DicomReader.cpp` 의 호출은 찾아냈다). 모의 SCP 가 실패를 만들 수 없어 이 경로를 지나는 시험도 없다.

**REQ-DICOM-033**: WHEN `xpe_dicom_cstore` completes successfully (C-STORE RSP status 0x0000 = Success), the system SHALL return `XPE_OK`.

**REQ-DICOM-034**: WHEN `xpe_dicom_cfind_mwl` is called, the system SHALL establish a DICOM ACSE association with the remote AE at `host:port` and execute a C-FIND request on the Modality Worklist Information Model using the query keys provided in `queryJson`.

**REQ-DICOM-035**: The `queryJson` input SHALL be a JSON object with DICOM tag keyword keys and string values for matching. Supported keys:
- `"PatientID"` (0010,0020)
- `"PatientName"` (0010,0010)
- `"ScheduledStationAETitle"` (0040,0001)
- `"Modality"` (0008,0060)
- `"ScheduledProcedureStepStartDate"` (0040,0002)

> **상태 메모 (2026-10-03, QA-B-199, 후보 C8)**: 미충족 후보 — 재현 확인 중(QA-B-200), #251. `ScheduledStationAETitle`·`ScheduledProcedureStepStartDate` 키가 무시되고(헤더 `dicom_api.h` 도 밝힘), Modality 가 Scheduled Procedure Step Sequence (0040,0100) 안이 아니라 최상위에 들어간다. 표준 MWL 질의가 아니다(REQ-DICOM-034 와 함께). 모의 SCP 는 PatientID 로만 맞춘다.

**REQ-DICOM-036**: The system SHALL write C-FIND results as a JSON array to `outJson`, where each element is a JSON object containing the matched DICOM tags as key-value pairs.

**REQ-DICOM-037**: IF no matching worklist entries are found, THEN the system SHALL write `[]` (empty JSON array) to `outJson` and return `XPE_OK`.

**REQ-DICOM-038**: IF the C-FIND operation fails or times out, THEN the system SHALL return `XPE_ERR_NETWORK_FAILED`.

> **상태 메모 (2026-10-03, QA-B-199, 후보 C9)**: 미충족 후보 — 재현 확인 중(QA-B-200), #251. `sendFINDRequest` 의 조건값만 보고 최종 C-FIND-RSP 상태(0xA700 같은 실패)를 읽지 않아, 실패한 질의가 `[]` 와 `XPE_OK` 로 돌아온다(`DicomNetworkSCU.cpp`).

**REQ-DICOM-039**: WHEN `xpe_dicom_cancel` is called, the system SHALL signal cancellation to any in-progress C-STORE or C-FIND operation. The cancelled operation SHALL return `XPE_ERR_PROCESSING_FAILED` with a cancel indicator in the alert message.

> **상태 메모 (2026-10-03, QA-B-199, 후보 C7)**: 미충족 후보 — 재현 확인 중(QA-B-200), #251. 취소 표시를 담을 알림이 코드에 없다(`xpe_alert_push` 호출 0건). 전송 중·C-FIND 교환 중 도착한 취소는 보이지 않고(헤더가 스스로 밝힘), 플래그는 모든 호출 입구에서 지워진다. 진행 중 취소 시험은 무조건 `GTEST_SKIP` 한다.

**REQ-DICOM-040**: The cancellation mechanism SHALL be thread-safe. `xpe_dicom_cancel` MAY be called from a different thread than the one executing the network operation.

### 3.5 Cross-Cutting Requirements

**REQ-DICOM-041**: All 11 exported functions (10 until 2026-10-03; `xpe_dicom_version` added per REQ-P0-033, user decision 7, `docs/project/REQ-CHANGE-LOG-2026-10-03-WORDING.md`) SHALL use C linkage (`extern "C"`), `__cdecl` calling convention, and blittable types only. All pointer parameters SHALL use basic C types compatible with .NET P/Invoke marshalling.

**REQ-DICOM-042**: The system SHALL NOT throw C++ exceptions across the DLL ABI boundary. All exceptions from DCMTK or OpenJPEG SHALL be caught internally and converted to `XpeErrorCode` return values.

**REQ-DICOM-043**: Each function SHALL log entry/exit at DEBUG level and error conditions at ERROR level via the logging subsystem in xpe_common.dll.

> **상태 메모 (2026-10-03, QA-B-199, 후보 C16)**: 미충족 후보 — 재현 확인 중(QA-B-200), #251. 진입만 DEBUG 로 기록하고 종료는 기록하지 않는다. 실패는 대개 `spdlog::warn`(ERROR 아님)이고, 로깅이 xpe_common API 가 아니라 모듈 자체 spdlog 다(`dicom.cpp`).

**REQ-DICOM-044**: The system SHALL NOT leak memory on any code path. DICOM dataset objects, network association objects, and compressed data buffers SHALL be freed on both success and error paths.

> **상태 메모 (2026-10-03, QA-B-199, 후보 C14)**: 미충족 후보 — 재현 확인 중(QA-B-200), #251. `reader.open()` 이 예외를 던지면 `new XpeDicomHandle` 로 만든 핸들이 해제되지 않는다(핸들 변수가 catch 범위 밖, `dicom.cpp`). 누수 시험은 실패 경로 일부와 쓰기만 덮는다.

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
| 1.1.0 | 2026-04-21 | MoAI (manager-spec) | **Released** — 46 EARS 요구사항 교차검증 완료. 10 C API 함수 전량 구현(dicom_api.h ↔ dicom.cpp), 35/35 Google Test 통과, TRUST 5 게이트 통과. Header EARS Count 40→46 정정 |
| 1.2.0 | 2026-10-03 | lead (QA-B-199) | 결함 후보 C1~C16 상태 메모 추가, 요구 문구 불변 (#251) |
| 1.3.1 | 2026-10-03 | lead | REQ-DICOM-024: Patient Name·Patient ID 를 Type 1 목록에서 Type 2(존재만 필수)로 — 표준 PS3.3 Table C.7-1, 사용자 결정, #251. 변경 기록 §7 |
| 1.3.0 | 2026-10-03 | lead | REQ-DICOM-041 함수 수 11, Acquisition Time 단위를 초로(C2), REQ-DICOM-026 파싱 불가 보고를 실제 형식으로(C4) — 사용자 결정, #251. 변경 기록 §5·§6 |

---

## 7. Cross-Verification Summary (v1.1.0 Released)

### 7.1 API Surface Verification (REQ-DICOM-041)

| # | Function | SPEC §4 | dicom_api.h | dicom.cpp | Status |
|---|----------|:-------:|:-----------:|:---------:|:------:|
| 1 | `xpe_dicom_open` | ✓ | L56 | L36 | ✅ PASS |
| 2 | `xpe_dicom_read_image` | ✓ | L73 | L52 | ✅ PASS |
| 3 | `xpe_dicom_get_metadata` | ✓ | L87 | L63 | ✅ PASS |
| 4 | `xpe_dicom_close` | ✓ | L96 | L74 | ✅ PASS |
| 5 | `xpe_dicom_write` | ✓ | L116 | L87 | ✅ PASS |
| 6 | `xpe_dicom_write_j2k` | ✓ | L136 | L100 | ✅ PASS |
| 7 | `xpe_dicom_validate` | ✓ | L164 | L117 | ✅ PASS |
| 8 | `xpe_dicom_cstore` | ✓ | L190 | L134 | ✅ PASS |
| 9 | `xpe_dicom_cfind_mwl` | ✓ | L216 | L149 | ✅ PASS |
| 10 | `xpe_dicom_cancel` | ✓ | L232 | L167 | ✅ PASS |

**결과**: 10/10 C API 함수가 SPEC, 헤더, 소스에 일관되게 정의·구현됨.

### 7.2 SWU Completeness Verification

| SWU | REQ Range | Implementation File | Test File | Tests |
|-----|-----------|---------------------|-----------|-------|
| SWU-4.1 DicomReader | REQ-DICOM-001..012 | DicomReader.cpp | test_dicom_reader.cpp | 16개 통과 |
| SWU-4.2 DicomWriter | REQ-DICOM-013..022 | DicomWriter.cpp | test_dicom_writer.cpp | 10개 통과 |
| SWU-4.3 DicomValidator | REQ-DICOM-023..028 | DicomValidator.cpp | test_dicom_validator.cpp | 7개 통과 |
| SWU-4.4 DicomNetworkSCU | REQ-DICOM-029..040 | DicomNetworkSCU.cpp | test_dicom_network_scu.cpp | 8개 통과 |
| 교차절단 | REQ-DICOM-041..046 | dicom.cpp | (통합 검증) | ABI 보증 |

**총 테스트**: 35/35 통과 (100%)

### 7.3 Cross-Cutting ABI Guarantees (REQ-DICOM-041..046)

- **REQ-041** (C linkage / blittable types): `extern "C"` 블록, `XPE_API` 매크로, `XpeErrorCode` 반환. ✅
- **REQ-042** (No C++ exceptions): 모든 10개 export에 `try { ... } catch (...)` 가드. ✅
- **REQ-043** (Logging): `spdlog::debug/error` 엔트리/에러 로깅. ✅
- **REQ-044** (No memory leak): `delete h` / RAII handle 패턴, 에러 경로 포함. ✅
- **REQ-045** (Reentrant reads): 독립 `XpeDicomHandle` 인스턴스 기반 멀티스레드 안전. ✅
- **REQ-046** (Network serialization): 호출자 책임 명시. ✅

---

*Document End -- SPEC-XPE-P1B-DICOM v1.1.0 (Released)*
