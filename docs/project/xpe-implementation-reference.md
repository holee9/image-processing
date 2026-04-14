# XPE Implementation Reference

**Document ID**: XPE-IMPL-REF-001  
**Version**: 1.2.0  
**Date**: 2026-04-15  
**Status**: Controlled Draft  
**Purpose**: Provide implementation-ready detail that complements `api-spec.md`, `sprint-plan.md`, and `pipeline-spec.md` so contributors can implement against a stable reference without inventing missing contracts.  
**Source Documents**: `api-spec.md v1.3.0`, `sprint-plan.md v1.4.0`, `pipeline-spec.md v1.5.0`, `SPEC-XPE-MASTER v2.1.0`, `xpe-algorithm-spec-deepsync.md v3.2.0-ds4`

**Changelog**: v1.0.0 -> v1.1.0 added logging format, alert schema, LUT file format, GSDF validation formulas, AI worker IPC, model-storage notes, and calibration `session_id` rules. v1.1.0 -> v1.2.0 rewrote the document into a clean canonical reference and removed legacy encoding debt.

---

## 1. Calibration File Binary Format

### 1.1 File extensions

| Extension | Meaning |
|---|---|
| `.xpe_calib` | native XPE calibration binary |
| `.dcm` | DICOM calibration image loaded through the DICOM stack |

### 1.2 Binary header layout

All multi-byte fields are little-endian. Header packing is `#pragma pack(push, 1)`.

| Offset | Size | Field | Type | Description |
|---:|---:|---|---|---|
| 0 | 4 | `magic` | `char[4]` | must equal `XPEC` |
| 4 | 2 | `version` | `uint16_t` | current format version = 1 |
| 6 | 4 | `width` | `uint32_t` | image width |
| 10 | 4 | `height` | `uint32_t` | image height |
| 14 | 2 | `pixelFormat` | `uint16_t` | `0=uint16`, `1=float32` |
| 16 | 1 | `calibType` | `uint8_t` | `0=offset`, `1=gain`, `2=defect` |
| 17 | 1 | `reserved` | `uint8_t` | must be zero |
| 18 | 8 | `expiryEpochMs` | `uint64_t` | `0` means no expiry |
| 26 | 4 | `crc32` | `uint32_t` | CRC-32 of `data[]` only |
| 30 | 4 | `dataLength` | `uint32_t` | byte length of `data[]` |
| 34 | var | `data` | `uint8_t[]` | row-major payload |

### 1.3 Validation rules

- reject files whose `magic` is not `XPEC`
- reject unsupported `version`
- reject `width` or `height` above the API maximum
- verify `crc32` on load; mismatch returns `XPE_ERR_IO_FAILED`
- reject expired files with `XPE_ERR_CALIBRATION_EXPIRED`

---

## 2. Calibration Path Contract

The library follows an explicit-path model.

- `xpe_init()` does not accept calibration directories
- the host decides calibration roots
- every load call receives a concrete absolute file path
- caller policy determines how “latest calibration” is chosen

Recommended directory split:

```text
data/
  calibration/
    offset/
    gain/
    defect/
```

---

## 3. Benchmark Manifest Minimum Schema

The benchmark manifest is the minimum gate for premium algorithm promotion.

Required top-level fields:

- `pack_id`
- `version`
- `created_utc`
- `owner`
- `dataset_family`
- `case_list`
- `metrics`
- `acceptance`
- `hash_policy`

Required dataset families:

- `BP-01` temperature sweep
- `BP-02` multi-gain linearity
- `BP-03` heel-effect and SID variation
- `BP-04` sparse defect injection
- `BP-05` clustered defect cases
- `BP-06` lag history sequences
- `BP-07` grid and no-grid pairs
- `BP-08` stitched and multi-irradiation exclusion cases
- `BP-09` degraded-mode and worker-failure scenarios
- `BP-10` end-to-end performance and memory runs

---

## 4. Ghost IRF Default Parameters

### 4.1 Default N=4 exponential seed model

Reference model:

`h(t) = sum(a_n * exp(-t / tau_n)) for n = 1..4`

Seed values for indirect-conversion a-Si flat panel detectors:

| Component | Amplitude `a_n` | Time constant `tau_n` | Practical meaning |
|---:|---:|---:|---|
| 1 | 0.015 | 50 ms | prompt shallow-trap release |
| 2 | 0.008 | 200 ms | medium-depth trap release |
| 3 | 0.003 | 1000 ms | deep trap release |
| 4 | 0.001 | 5000 ms | very deep residual tail |

These are starting seeds only. Site tuning is expected.

### 4.2 Tier targets

| Tier | Algorithm family | Residual target | Incremental time budget |
|---|---|---:|---:|
| Tier 1 | fixed-coefficient LTI | < 0.5% | <= 150 ms |
| Tier 2 | exposure-weighted LTI | < 0.35% | <= 190 ms total |
| Tier 3 | NLCSC | <= 0.29% | <= 240 ms total |

### 4.3 Escalation rule

- start from Tier 1
- escalate only when residual stays above the configured threshold
- bypass or downgrade cleanly on first frame, empty history, or single-shot mode

---

## 5. Metadata and DICOM Mapping

| XPE field | Source | Notes |
|---|---|---|
| `bodyPart` | host policy or assistive AI output | AI result must remain advisory |
| `kVp` | DICOM acquisition metadata | detector-domain QA input |
| `mAs` | DICOM acquisition metadata | detector-domain QA input |
| `SID_mm` | DICOM acquisition metadata | required for geometry-sensitive corrections |
| `pixelPitch_mm` | detector configuration or DICOM | required for detector metrics |
| `acquisitionTime` | DICOM date-time or host timestamp | convert to UNIX epoch ms |
| `flags` | pipeline state only | no embedded error reasons |

Detector-domain metrics must never be computed from presentation-domain images.

---

## 6. GUI Path Persistence

`ImageProcTest.exe` persists operator convenience paths in `appsettings.json`.

Recommended keys:

- `lastImageDir`
- `lastDicomDir`
- `calibOffsetDir`
- `calibGainDir`
- `calibDefectDir`

Rules:

- empty strings are invalid persisted values
- missing keys fall back to a controlled default root
- persistence is host-only behavior, not part of the native ABI

---

## 7. Path Management Rules

- use absolute paths at all ABI boundaries
- do not infer working-directory-relative calibration paths inside DLLs
- keep runtime logs, processed images, and calibration data in separate roots
- do not silently create calibration files on behalf of the caller

Recommended runtime layout:

```text
runtime/
  logs/
  processed_images/
  calibration/
  models/
```

---

## 8. Error and Degraded-Mode Conventions

| Mode | Meaning | Example |
|---|---|---|
| `FAIL` | stop the requested operation | corrupted calibration CRC |
| `BYPASS` | skip optional stage and continue | missing grid LUT |
| `DOWNGRADE` | switch to lower-complexity deterministic tier | lag Tier 3 unavailable |
| `DEGRADE` | disable assistive branch and continue | AI worker unavailable |

Every degraded case must emit either:

- an alert queue message, or
- a structured diagnostic record.

Flags alone are not sufficient to explain the reason.

---

## 9. Logging and Alerts

### 9.1 Plain-text log format

Required format:

```text
[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [TID] message
```

Example:

```text
[2026-04-15 09:23:45.123] [INFO] [00042] xpe_init: config loaded
```

### 9.2 Severity levels

| Value | Label | Intended use |
|---:|---|---|
| 0 | `TRACE` | dense developer diagnostics |
| 1 | `DEBUG` | debug detail |
| 2 | `INFO` | normal lifecycle and stage events |
| 3 | `WARN` | degraded but recoverable operation |
| 4 | `ERROR` | stage failure or rejected request |
| 5 | `OFF` | logging disabled |

### 9.3 Alert JSON schema

`xpe_get_pending_alert()` should return UTF-8 JSON matching this shape:

```json
{
  "severity": "WARN",
  "code": "GSVG_PROCESSING_FAILED",
  "message": "gsvg.dll unavailable; deterministic path preserved",
  "timestamp_ms": 1776245025123,
  "stage_id": 9,
  "stage_name": "gsvg",
  "frame_index": 42
}
```

Required fields:

- `severity`
- `code`
- `message`
- `timestamp_ms`
- `stage_id`
- `stage_name`
- `frame_index`

### 9.4 JSON log mode

When `xpe_init()` enables `"logFormat": "json"`, each log line should be a single JSON object:

```json
{"ts":"2026-04-15T09:23:45.123Z","lvl":"INFO","tid":42,"msg":"xpe_init: config loaded"}
```

---

## 10. LUT File Format

Use a compact binary LUT file with this header:

| Offset | Size | Field | Type |
|---:|---:|---|---|
| 0 | 4 | `magic` | `char[4]` = `XPLT` |
| 4 | 2 | `version` | `uint16_t` |
| 6 | 2 | `lutType` | `uint16_t` |
| 8 | 4 | `numEntries` | `uint32_t` |
| 12 | 4 | `bitsStored` | `uint32_t` |
| 16 | 4 | `firstStoredValue` | `int32_t` |
| 20 | 4 | `crc32` | `uint32_t` of payload |
| 24 | var | `data` | array payload |

Rules:

- `numEntries` must be one of `256`, `1024`, `4096`, or `65536`
- payload must be monotonic for presentation LUT use cases
- CRC mismatch returns `XPE_ERR_IO_FAILED`

---

## 11. GSDF Compliance Verification

Minimum GSDF QA contract:

- sample at 18 luminance checkpoints,
- measure deviation from target JND progression,
- pass criterion: each checkpoint within +/-10% of target response,
- failure result shall request display recalibration, not detector recalibration.

This section supports QA workflows and does not redefine detector-domain quality metrics.

---

## 12. AI Worker IPC Contract

Phase 3 communication between `xpe_ai.dll` and `xpe_ai_worker.exe` uses:

- a named pipe for control messages,
- shared memory for image payloads.

### 12.1 Transport

| Item | Value |
|---|---|
| control channel | `\\\\.\\pipe\\xpe_ai_worker_{PID}` |
| message size cap | 64 KB per control message |
| image payload | `Local\\xpe_ai_shm_{PID}_{FrameID}` shared memory |
| per-message timeout | 50 ms default |

### 12.2 Message framing

```text
[LEN: uint32 little-endian][TYPE: 4 ASCII bytes][BODY: LEN bytes UTF-8 JSON]
```

### 12.3 Message types

| Type | Direction | Meaning |
|---|---|---|
| `INFT` | DLL -> worker | inference request |
| `RSPN` | worker -> DLL | inference response |
| `PING` | DLL -> worker | liveness probe |
| `PONG` | worker -> DLL | liveness response |
| `KILL` | DLL -> worker | orderly shutdown request |
| `ERRO` | worker -> DLL | structured failure report |

### 12.4 Failure handling

- restart the worker after timeout or crash,
- retry budget is limited,
- after repeated failure, disable Phase 3 and emit `AI_MODEL_UNAVAILABLE`,
- deterministic Phase 1 and Phase 2 outputs must continue.

---

## 13. Model Storage Contract

Recommended model packaging:

```text
data/
  models/
    manifest.json
    body_part_recognizer_int8.onnx
    bone_suppression_int8.onnx
    dl_denoiser_int8.onnx
```

`manifest.json` should define:

- model name,
- semantic version,
- SHA-256,
- quantization mode,
- expected input tensor shape,
- allowed body-part scope,
- approval status.

CPU-friendly INT8 packaging is the default assumption for release candidates.

---

## 14. Calibration `session_id` Format

Recommended canonical format:

```text
{detector_serial}_{yyyymmdd}_{calib_type}_{sha256_prefix8}
```

Example:

```text
AU1234567_20260415_offset_a3f2b1c9
```

Rules:

- `detector_serial`: alphanumeric, max 16 chars
- `yyyymmdd`: valid calendar date
- `calib_type`: `offset`, `gain`, or `defect`
- `sha256_prefix8`: first 8 lowercase hex chars of the payload hash

This identifier is for traceability and deduplication. It does not replace `expiryEpochMs`.

---

*Document End -- XPE-IMPL-REF-001 v1.2.0*
