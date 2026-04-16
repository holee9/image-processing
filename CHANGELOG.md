# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

---

## [1.0.0-display] — 2026-04-16

### SPEC-XPE-P1B-DISP v1.0.0 — xpe_display.dll (IEC 62304 Class B)

#### Added

- **xpe_display.dll** — DICOM display processing DLL (Sprint S1-B)
- **SWU-3.1 Modality LUT** (`xpe_apply_modality_lut`)
  - LINEAR mode: output = input × slope + intercept (DICOM PS3.3 C.11.1)
  - TABLE mode: LUT lookup with index clamping and firstMapped offset
  - Requirement coverage: REQ-DISP-001..008
- **SWU-3.2 VOI LUT** (`xpe_apply_voi_lut`, `xpe_voi_preset_create`)
  - LINEAR windowing (standard DICOM half-value offset convention)
  - LINEAR_EXACT windowing (DICOM PS3.3 C.11.2.1.3)
  - SIGMOID windowing (smooth contrast transition)
  - 4 body-part presets: BONE, LUNG, ABDOMEN, HEAD
  - Requirement coverage: REQ-DISP-009..018
- **SWU-3.3 Presentation LUT + GSDF** (`xpe_apply_presentation_lut`, `xpe_gsdf_calibrate`)
  - 1024-entry LUT with float32→uint16 domain transition
  - DICOM PS3.14 GSDF calibration using Barten model approximation
  - Monotonically non-decreasing LUT enforcement
  - Requirement coverage: REQ-DISP-019..028
- **Integration** (REQ-DISP-029..035)
  - Full Modality→VOI→Presentation pipeline validation
  - Thread-safety verification (independent buffer access)
  - 1×1 and large image edge cases

#### Test Coverage

- 48 Google Test cases (11 modality + 15 VOI + 12 presentation + 10 integration)
- REQ-DISP-001..035 full traceability
- Performance targets: ModalityLUT ≤20ms, VOI LUT ≤16ms, PresentationLUT ≤25ms (3072×3072)

#### Known Issues / Pending Validation

- GSDF Barten model constants (71.498, -94.593, 41.912, 9.8212) require clinical validation
  against DICOM PS3.14 test vectors (`@MX:WARN` placed on `xpe_gsdf_calibrate`)
- SWU-3.4 (deferred): not in scope for this SPEC

#### Implementation Notes

- C ABI (`extern "C"`, `__cdecl`) for P/Invoke compatibility from C# host
- CMakeLists.txt: SHARED library + FetchContent GTest fallback (v1.14.0)
- Files: `display_api.h`, `display_internal.h`, `modality_lut.cpp`, `voi_lut.cpp`,
  `presentation_lut.cpp`, `display_helpers.cpp`, `display.cpp`
- `@MX:ANCHOR` on all 5 public API functions; `@MX:WARN` on GSDF calibration

---

## Previous Releases

- **xpe_enhance_basic.dll** — SPEC-XPE-P1B-ENH (Sprint S1-B): 67/67 tests passing
- **xpe_preprocess.dll** — SPEC-XPE-P1A-PRE (Sprint S1-A)
- **xpe_common.dll** — SPEC-XPE-S0-B (Sprint S0-B)
