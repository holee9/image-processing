# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

---

## [1.4.0-governance] — 2026-04-21

### 거버넌스 세션 — 3-Lane 통합 + CI 확장 + SPEC Released

#### 통합 (Lane → main squash merge)

- **dev/postprocess → main**: `xpe_enhance_advanced` Collimation Hough 버그픽스, GTest FetchContent fallback, T606 std::memset/void* cast 수정, IEC 62304 SRS/SDD/RTM 65/65 전수 GREEN 반영
- **dev/gui → main**: TASK-GUI-IA-001 Evaluation/Calibration 탭 책임 분리 + MVVM 모듈화
  - 신규: Controls (EvaluationViewerPanel, HistogramControl), ViewModels (ViewportViewModel)
  - 신규: Models (ActiveEvaluationContext, AlgorithmChainModels, HistogramData, ViewportRenderParams, ViewportRenderStateSnapshot)
  - 신규: Services (AlgorithmChainCatalogService, EvaluationContextService, NativeEnhanceBasicPreviewService, ViewportRenderService)
  - 신규: Diagnostics (XpeEnhanceBasicLibraryLocator, XpeEnhanceBasicReadinessProbe)
  - 26파일, +5700줄 순증가

#### SPEC 승격

- **SPEC-XPE-P1B-DICOM** Draft → **Released** (v1.1.0): 46 EARS 요구사항 교차검증 완료, SWU-4.1~4.4 전체 10개 C API 일치 확인

#### CI 확장

- **benchmark-regression.yml** 신설: BP-10 degraded-mode stress 5개 시나리오 (missing_enhance_basic/advanced/display/dicom/all_optional_absent) matrix CI
- **Test-DegradedMode.ps1** 신설: DLL 제거 + smoke test 드라이버, Lane A/B `DegradedMode.*` GTest 연동 준비

#### 정리

- `.gitignore`: `build_coverage/` 빌드 아티팩트 추적 제외
- stale worktree `xpe-gui-r4` (codex/gui-preprocess-r4-e2e) 제거
- `dev-plan.md` v2.4.0: 2026-04-21 현황 반영, 완료 항목 갱신

#### Framework A 점수

82 → **83 / 100** (+1, 요구사항 완전성 15→16)

---

## [1.3.0-postprocess] — 2026-04-20

### 3-Lane 통합 세션 — SPEC-XPE-P2-ADV 완료 + M2 SIMD

#### 통합

- **dev/preprocess → main**: 테스트 202/202 전체 통과, XPE_PIX_\* → XPE_PIXEL_\* API 마이그레이션
- **dev/postprocess → main**: SPEC-XPE-P2-ADV MFP/Edge/Collimation/EI + Hough/FD 버그픽스
- **dev/gui → main**: Algorithm Validation UI, MetricsComputationService, TASK-GUI-VIEWER-001

#### 구현

- **xpe_enhance_advanced.dll**: Phase 2 고급 후처리 완료 — SWU-2.5(MFP)/2.6(FD)/2.8(Collimation)/2.10(EI) 4개 SWU, 97→65개 리팩터 후 전수 통과
- **M2 SIMD**: Offset/Gain/Defect/Detection AVX2/FMA 최적화, bit-identical parity PASSED

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
