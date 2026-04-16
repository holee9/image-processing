# Calibration Raw Fixture Cases

This folder is a local test-data staging area for calibration/preprocessing validation.
Large `.raw` files are intentionally ignored by Git and must be copied from local source media.

## Raw Format

- Pixel format: unsigned 16-bit raw, little-endian assumption
- Inferred dimensions for copied files: 3072 x 3072
- Expected file size: 18,874,368 bytes per full-frame image

The dimension is inferred from `3072 * 3072 * 2 = 18,874,368` bytes.

## Imported Cases

| Case ID | Purpose | Source root | Calibration data | Image samples |
|---|---|---|---:|---:|
| `aed_shock_had1717mc` | AED/shock and dark-calibration workflow checks | `E:\documents\10.제품관련문서\HAD1717MC\AED_Dev\test_image\AED_shock_test` | 3 files | 4 files |
| `auradr_release_line_trg` | AuraDR line-trigger calibration and image pairing | `E:\backup\인수인계\인수인계_김재민대리\Coding\AuraDR_Manager\Release` | 8 files | 3 files |
| `corner_blemish_17a06b1` | Defect/blemish comparison cases with matching calibration artifacts | `E:\backup\인수인계\인수인계_김재민대리\Coding\MATLAB\Project\Corner Blemish\_Images` | 4 files | 3 files |

## Directory Contract

Each case uses this structure:

```text
<case-id>/
  calibration/
    *.raw
  images/
    *.raw
```

Test code should treat the `calibration/` folder as the matching calibration context for all images in the same case.
Do not mix calibration data across cases unless a test explicitly validates mismatch handling.

## E2E Evaluation Use

These fixtures are the local real-data input for `docs/project/Preprocessing-E2E-Automated-Evaluation-Protocol.md`.

Required automated checks:

- `PRE-E2E-0`: scan every case, compute file size and SHA-256, infer 3072 x 3072 uint16 geometry, and prove raw payloads are ignored by Git.
- `PRE-E2E-2`: run matching image/calibration pairs and compute detector-domain calibration-effect metrics.
- `PRE-E2E-3`: compare against known reference outputs only after reference semantics are confirmed. Example candidate: `aed_shock_had1717mc/images/230811_img_oc.raw`.
- `PRE-E2E-5`: intentionally mismatch image/calibration folders and require a warning or hard failure.

The test runner must preserve input raw bytes. Record `sha256(raw_before) == sha256(raw_after)` in every report.

## File Inventory

### `aed_shock_had1717mc`

Calibration files:

- `calibration/BPM.raw`
- `calibration/cDark.raw`
- `calibration/cDark_AverMasDark.raw`

Image files:

- `images/230811_img.raw`
- `images/230811_img_oc.raw`
- `images/230814_aed_th4_shock_off.raw`
- `images/230814_aed_th4_shock_3000.raw`

### `auradr_release_line_trg`

Calibration files:

- `calibration/BPM.raw`
- `calibration/cDark.raw`
- `calibration/CalSet00_00001.raw`
- `calibration/CalSet01_03073.raw`
- `calibration/CalSet02_06145.raw`
- `calibration/cBr_00001A.raw`
- `calibration/cBr_03073A.raw`
- `calibration/cBr_06145A.raw`

Image files:

- `images/21412.raw`
- `images/Specify.raw`
- `images/Specify Name.raw`

### `corner_blemish_17a06b1`

Calibration files:

- `calibration/BPM.raw`
- `calibration/CalSet01_02559.raw`
- `calibration/CalSet01_02559_BPM.raw`
- `calibration/CalSet01_02559_Dif.raw`

Image files:

- `images/Good_1.raw`
- `images/Normal_1.raw`
- `images/Bad_1.raw`

## Verification Snapshot

- `aed_shock_had1717mc`: 7 raw files, about 126 MB
- `auradr_release_line_trg`: 11 raw files, about 198 MB
- `corner_blemish_17a06b1`: 7 raw files, about 126 MB
- Total copied payload: 25 raw files, about 450 MB

## Git Policy

The raw files are local-only binary fixtures. Keep this README and `.gitignore` under version control, but do not commit the copied `.raw` payloads.
