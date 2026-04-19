# XPE Sprint-Level Decomposition Plan

**Document ID**: XPE-SPRINT-PLAN-001
**Version**: 1.5.0
**Date**: 2026-04-15
**Source**: SPEC-XPE-MASTER v2.1.0, api-spec.md v1.3.0, pipeline-spec.md v1.5.0, xpe-algorithm-spec-deepsync.md v3.2.0-ds4, xpe-implementation-reference.md v1.2.0, XPE-Brainstorming-DeepSync-Execution.md v1.0.0
**Total Sprints**: 29
**Changelog**:
- v1.0.0 -> v1.1.0: cross-verification corrections, EI scope corrected, appendices expanded.
- v1.1.0 -> v1.2.0: source references refreshed and canonical executable-unit total corrected from 43 to 42.
- v1.2.0 -> v1.3.0: brainstorming deep-sync added implementation-first scaffolding and parity/sidecar rules.
- v1.3.0 -> v1.4.0: cross-verification round 11 added the P0-13 benchmark manifest gate, NLCSC Tier 3 gate, and scalar-reference DoD in P1A-02.
- v1.4.0 -> v1.5.0: GUI-First restructure — SPRINT-GUI-S0 added (sprint 0, no C++ dependency), P0-07 scoped to IXpeBackend adapter + P/Invoke only (DICOM ownership stays in Phase 1b), Gates G2/G3 upgraded to dual-gate (benchmark/task-based evidence + GUI demo), GUI verification reclassified as required demo evidence (non-blocking for C++ merge).
- v1.5.0 -> v1.6.0: Test GUI evolution policy added. Early diagnostic-first health panels are transitional; release-level ImageProcTest shall be workflow-first with Diagnostics, Metrics, Reports, and Help as dedicated areas.

---

## Executive Summary

| Phase | Sprint Count | SWU Count | Native API Count | Complexity |
|-------|:-----------:|:---------:|:---------:|:----------:|
| **Phase -1: GUI Skeleton** | **1** | 1 C# (mock) | 0 (stub only) | Medium |
| Phase 0: Foundation | 7 | 7 + 1 C# | 18 | Medium-High |
| Phase 1a: Pre-Processing | 6 | 9 | 18 | High |
| Phase 1b: Enhancement + Display + DICOM | 8 | 13 + 1 C# | 28 (enhance_basic=7, display=11, dicom=10) | Medium-High |
| Phase 2: Advanced + GSVG | 4 | 3 + 4 SI | 11 (enhance_advanced=3, gsvg=8) | High |
| Phase 3: AI / Intelligence | 3 | 4 | 7 | Complex |
| **Total** | **29** | **42 + 2 C#** | **82** | -- |

---

## Brainstorm-Derived Non-Negotiables

The following rules are now mandatory across the sprint plan:

1. every major detector stage needs a scalar reference path before SIMD optimization,
2. parity and timing harnesses must exist before a stage is considered complete,
3. ROI, diagnostics, and confidence outputs must use sidecar contracts rather than generic metadata mutation,
4. benchmark manifests must freeze before premium tuning starts,
5. assistive AI cannot start before deterministic fallback behavior is testable,
6. GUI-S0 scope is fixed: Raw image viewer + settings UI + IXpeBackend mock + log/alert panel only. Real DICOM read/write is owned exclusively by xpe_dicom.dll (Phase 1b) and must not be duplicated in C# code (refs: product.md §2.1, structure.md §4.2),
7. GUI verification is **required demo evidence** at each sprint review but is **not a C++ merge-blocking gate**. Blocking gates remain: scalar reference pass, parity test, benchmark budget, performance target, and unit test coverage. GUI and C++ blocking gates are independent,
8. Phase 2 and Phase 3 quality gates require a **dual-gate**: (a) quantitative benchmark / task-based / observer evidence per Algorithm-Evaluation-Protocol.md §4.1 and §5.1, AND (b) GUI demo evidence. GUI Before/After alone is insufficient to pass Gate G2 or G3.
9. Test GUI health/readiness panels are a transitional integration aid. They shall move to a Diagnostics area as soon as the first image-processing module reaches `R3`; by release-candidate validation, Workflow, Metrics, Reports, and Help shall be the primary user-facing areas.

These rules are intended to maximize implementation feasibility, not to slow the project down.

---

## Sprint Dependency Graph

```
SPRINT-GUI-S0 (C# WPF Skeleton + IXpeBackend Mock)   <- no C++ dependency, starts immediately
    |
    |   [PARALLEL] SPRINT-P0-01 (Build System)
    |                   |
    |               SPRINT-P0-02 (Common Types + Memory + Error)
    |                   |
    |                   +---> SPRINT-P0-03 (Logging Subsystem)
    |                   |         |
    |                   |         v
    |                   +---> SPRINT-P0-04 (Config + Lifecycle + Param)
    |                   |         |
    |                   |         v
    |                   +---> SPRINT-P0-05 (Alert Subsystem)
    |                   |
    |               SPRINT-P0-06 (Thread Pool + Test Infra + CI)
    |                   |
    +-------------------+
    v
SPRINT-P0-07 (IXpeBackend.Real + Module Dirs + Full P/Invoke)
    |   [GUI-S0 Mock -> Real swap happens here]
    +==========================================+
    |                                          |
    v                                          |
SPRINT-P1A-01 (CalibManager)                   |
    |                                          |
    +---> SPRINT-P1A-02 (Offset + Gain)        |
    |         |                                |
    |         +---> SPRINT-P1A-03 (Readout + Temp + Nonlin + Binning)
    |         |                                |
    |         +---> SPRINT-P1A-04 (Defect Correction)
    |                    |                     |
    |                    v                     |
    +---> SPRINT-P1A-05 (Ghost Tier 1+2)       |
    |         |                                |
    |         v                                |
    +---> SPRINT-P1A-06 (Ghost Tier 3 + Pipeline Integration)
              |                                |
              v                                |
    +---------+----------+----------+          |
    |         |          |          |          |
    v         v          v          v          |
  P1B-ENH  P1B-DISP  P1B-DICOM  P1B-GUI <----+
  (3 spr)  (2 spr)   (2 spr)   (1 spr)
    |         |          |          |
    v         v          v          v
    +=========+==========+==========+
              |
    +---------+---------+
    |                   |
    v                   v
  P2-ADV (2 spr)    P2-GSVG (2 spr)   [DUAL GATE: benchmark + GUI demo]
    |                   |
    +-------------------+
              |
              v
         P3-AI (3 spr)                         [DUAL GATE: task-based evidence + GUI demo]
```

---

## Quality Gates Between Phases

### Gate G0 -> G1a (Phase 0 Complete) ✅ PASSED 2026-04-18

- [x] `cmake --preset release && cmake --build --preset release` succeeds
- [x] xpe_common.dll exports all 15 functions (AED 제거 후 15개, 검증 완료)
- [x] Google Test + CTest framework operational
- [x] Unit test coverage >= 85% for xpe_common (91/91 tests GREEN)
- [x] P/Invoke smoke test passes (C# loads xpe_common.dll, calls `xpe_version`)
- [x] All 9 module directories exist with stub CMakeLists.txt
- [x] Static analysis (cppcheck) reports 0 warnings
- [x] Benchmark manifest schema exists (`benchmark/BP-01-05-preprocess-manifest.md`)

### Gate G1a -> G1b (Phase 1a Complete) ✅ PASSED 2026-04-19

- [x] xpe_preprocess.dll exports all 18 functions
- [x] Pre-processing pipeline (stages 0.5-4) — `xpe_preprocess_pipeline` 통합 완료
- [x] Ghost Tier 1+2 processing완료 (NLCSC Tier 1/2 구현)
- [x] Ghost Tier 3 (NLCSC) processing 완료 (Tier 3 고도화, 14-50x 업계 우위)
- [x] Calibration CRC verification works end-to-end (SUP-01, 89/90 tests GREEN)
- [x] Unit test coverage >= 85% for xpe_preprocess (P1A: 89/90 GREEN)
- [x] P/Invoke integration test: GUI-IT 78/78 통과 (SPEC-XPE-GUI-IT 완료)
- [x] Memory leak test: 1000 frames without growth — PASSED 2026-04-19 (delta 0KB)

### Gate G1b -> G2 (Phase 1b Complete)

- [ ] Full Phase 1 pipeline < 3000ms for 3072x3072
- [ ] VOI LUT interactive latency <= 16ms
- [ ] DICOM DX IOD read/write validated
- [ ] EI/DI calculation matches IEC 62494-1 (whole-image baseline)
- [ ] GSDF compliance check operational
- [ ] Phase 1 peak memory <= 190MB
- [ ] Integration test: Raw DICOM -> Pre -> Post -> EI -> Display -> DICOM Write
- [ ] Unit test coverage >= 85% across all Phase 1b DLLs

### Gate G2 -> G3 (Phase 2 Complete)

**Quantitative gates (blocking — must pass before merge to main):**
- [ ] GSVG: Grid artifact visually imperceptible, MTF degradation <= 5%
- [ ] Virtual Grid: CNR >= 90% vs 6:1 physical grid
- [ ] EI ROI refinement operational with collimation ROI
- [ ] Phase 2 total <= 2500ms
- [ ] Phase 2 peak memory <= 440MB
- [ ] Unit test coverage >= 85%

**Dual-gate: task-based / observer evidence (blocking — per Algorithm-Evaluation-Protocol.md §4.1):**
- [ ] Virtual Grid: observer or expert review completed (anatomy preservation confirmed)
- [ ] Virtual Grid: objective image-quality score measured and documented
- [ ] Virtual Grid: anatomy-specific artifact review (no promotable degradation)
- [ ] Virtual Grid: parameter-sensitivity sweep completed (default settings are not brittle)

**Required demo evidence (non-blocking for C++ merge, required for sprint review sign-off):**
- [ ] GUI Before/After comparison shown for grid suppression and virtual grid

### Gate G3 (Phase 3 Complete)

**Quantitative gates (blocking — must pass before merge to main):**
- [ ] AI worker process isolation (crash does not take down host)
- [ ] Body Part Recognition >= 15 categories, >= 95% accuracy
- [ ] Bone Suppression PSNR >= 33dB, SSIM >= 0.97
- [ ] Deterministic fallback path works when AI unavailable
- [ ] Phase 3 total <= 3000ms, peak memory <= 740MB

**Dual-gate: task-based / observer evidence (blocking — per Algorithm-Evaluation-Protocol.md §5.1):**
- [ ] Bone Suppression: task-based detectability figure of merit documented
- [ ] Bone Suppression: observer / expert review confirms anatomy preservation
- [ ] Bone Suppression: sensitivity to parameter sweep completed
- [ ] Bone Suppression: baseline-versus-assisted comparison documented (no information hiding)
- [ ] DL Denoise: task-based detectability figure of merit documented (if clinical-use intent)
- [ ] DL Denoise: observer / expert review confirms anatomy preservation
- [ ] Degraded-mode proof: all Phase 3 features gracefully degrade when AI models absent

**Required demo evidence (non-blocking for C++ merge, required for sprint review sign-off):**
- [ ] GUI Before/After shown for bone suppression and DL denoise

---

## Phase -1: GUI Skeleton

---

### SPRINT-GUI-S0: C# WPF Test Application Skeleton + IXpeBackend Mock

**Sprint ID**: SPRINT-GUI-S0
**Sprint Goal**: Create the standalone C# WPF test application with IXpeBackend adapter interface and MockXpeBackend implementation. No C++ DLL required. Runs fully from sprint day one.
**SWU Scope**: SWU-5.7 (PipelineOrchestrator — stub scaffolding only)
**API Functions**: None (mock interface; real P/Invoke connected in SPRINT-P0-07)
**Dependencies**: None (independent of all C++ sprints — first sprint overall)
**Estimated Complexity**: Medium

**Scope Boundary [HARD]**:
- INCLUDED: Raw binary image loading (`.raw` only), stub metadata display, calibration path settings UI, log panel, alert panel, IXpeBackend mock
- EXCLUDED: Real DICOM file loading or writing — DICOM I/O is owned exclusively by `xpe_dicom.dll` (SPRINT-P1B-DICOM-01). No C# DICOM logic here.

**UI Evolution Note**:
- Diagnostic health/readiness may be visually prominent in this sprint because the goal is ABI and fallback safety.
- This layout is not the final release layout. It must evolve toward the release information architecture defined in `XPE-GUI-NATIVE-INT-READINESS-001` Section 8.

**Acceptance Criteria**:
1. `ImageProcTest.csproj` targets .NET 8, WPF framework, x64 platform
2. `IXpeBackend` interface declared in `src/gui/backend/IXpeBackend.cs` with methods: `Initialize()`, `GetVersion()`, `LoadRawImage(path, out width, out height)`, `GetAlertCount()`, `GetAlert(index)`
3. `MockXpeBackend : IXpeBackend` returns deterministic synthetic data for all methods
4. Application startup: detects `xpe_common.dll` presence; if absent, activates `MockXpeBackend` automatically (hot-swap preparation)
5. Main window shows: image canvas (WriteableBitmap), status bar with backend version, log panel, alert panel
6. "Load Raw Image" button: opens `OpenFileDialog` filter `*.raw` only (no `*.dcm`) and displays uint16 image as normalized grayscale
7. "Calibration Path Settings" dialog: 3 path fields (Offset dir, Gain dir, Defect dir) with `FolderBrowserDialog` buttons
8. Paths persist to `appsettings.json` keys: `lastRawDir`, `calibOffsetDir`, `calibGainDir`, `calibDefectDir`
9. Log panel: displays timestamped messages from active `IXpeBackend` implementation
10. Alert panel: displays alerts with severity color coding (INFO=gray, WARN=yellow, ERROR=red)

**Test Cases**:
1. Launch with no DLLs present → window opens, MockXpeBackend activates, status bar shows "v0.0.0-mock"
2. Load `test_data/synthetic_1024x1024.raw` (uint16 generated by test script) → image renders in canvas
3. Open Calibration Settings, set 3 paths, Save → `appsettings.json` updated with all 4 keys
4. Relaunch app → calibration path fields pre-populated from persisted `appsettings.json`
5. Log panel: MockXpeBackend emits 5 log lines → all 5 displayed with `[HH:MM:SS.mmm]` format
6. Alert panel: MockXpeBackend queues 3 alerts (1 INFO, 1 WARN, 1 ERROR) → color coding correct
7. `IXpeBackend.GetVersion()` on MockXpeBackend → returns non-empty string (no exception)

**Definition of Done**:
- [ ] `IXpeBackend` interface and `MockXpeBackend` in `src/gui/backend/`
- [ ] Application launches with zero unhandled exceptions on clean machine (no C++ DLLs)
- [ ] All 7 test cases pass (manual checklist or C# UI automation)
- [ ] Raw image display: uint16 → normalized [0,255] grayscale renders correctly for 3072×3072
- [ ] `appsettings.json` schema documented with all 4 keys
- [ ] **No DICOM code**: no references to DCMTK, DicomFile, or `.dcm` parsing in C# project

**GUI Demo Evidence** (non-blocking — required at sprint review):
- [ ] Live demo: launch app, load raw image, open calibration settings, show log/alert panels
- [ ] Explain which visible panels are transitional diagnostics and which are part of the final workflow.

**Risk Items**:
- WriteableBitmap performance for 3072×3072: use pixel-stride copy, avoid per-pixel loops
- appsettings.json read/write on WPF startup: use UI dispatcher, single writer

---

## Phase 0: Foundation

---

### SPRINT-P0-01: Build System Setup

**Sprint ID**: SPRINT-P0-01
**Sprint Goal**: Establish the CMake build system with vcpkg integration and compiler presets.
**SWU Scope**: None (infrastructure)
**API Functions**: None
**Dependencies**: None (first sprint)
**Estimated Complexity**: Medium

**Acceptance Criteria**:
1. Root `CMakeLists.txt` configures project with C17/C++17 standards
2. `CMakePresets.json` defines Debug, Release, and CI presets with correct compiler flags
3. `vcpkg.json` declares all SOUP dependencies (gtest, dcmtk, openjpeg, fftw3, onnxruntime)
4. `cmake/CompilerWarnings.cmake` enables `/W4 /WX` (MSVC) with suppression for third-party headers
5. `cmake/PlatformAVX2.cmake` detects and enables AVX2 intrinsics support
6. `cmake/DependencyRules.cmake` manages vcpkg + system dependency resolution
7. `cmake --preset release` configures without error on Windows x64

**Test Cases**:
1. Run `cmake --preset debug` on clean checkout -- expect 0 errors, CMakeCache.txt generated
2. Run `cmake --preset release` -- expect Release flags (`/O2 /DNDEBUG`) in compile commands
3. Run `cmake --preset ci` -- expect coverage flags enabled and sanitizers configured
4. Verify `vcpkg install` resolves all manifest dependencies -- expect all packages installed
5. Verify AVX2 detection: on AVX2-capable CPU, expect `-mavx2` or `/arch:AVX2` in flags

**Definition of Done**:
- [ ] All 3 CMake presets configure successfully
- [ ] vcpkg manifest resolves all dependencies
- [ ] cmake/ helper modules load without warning
- [ ] Empty `modules/common/CMakeLists.txt` integrates into build tree
- [ ] CI preset enables coverage instrumentation

**Risk Items**:
- vcpkg FFTW3 package may require manual triplet configuration for Windows static linking
- ONNX Runtime vcpkg port may lag behind required version (check 1.17+ availability)

---

### SPRINT-P0-02: Common Types, Memory Management, and Error Handling

**Sprint ID**: SPRINT-P0-02
**Sprint Goal**: Implement the foundational type definitions, memory pool, and error subsystem of xpe_common.dll.
**SWU Scope**: SWU-5.1 (MemoryPool), SWU-5.3 (ErrorHandler), SWU-5.5 (ParameterValidator)
**API Functions**: `xpe_alloc_image`, `xpe_free_image`, `xpe_copy_image`, `xpe_error_string`, `xpe_get_param_range`
**Dependencies**: SPRINT-P0-01 (build system)
**Estimated Complexity**: Medium

**Acceptance Criteria**:
1. `xpe_types.h` defines `XpeImageBuffer`, `XpeImageMetadata`, `XpePixelFormat` with `#pragma pack(push, 8)`
2. `xpe_error.h` defines all 11 error codes (XPE_OK through XPE_ERR_NETWORK_FAILED)
3. `xpe_alloc_image` allocates aligned memory for uint16 and float32 formats up to 4096x4096
4. `xpe_free_image` zeroes `data` and `dataSize` fields after freeing
5. `xpe_copy_image` returns `XPE_ERR_BUFFER_TOO_SMALL` when dst dimensions mismatch
6. `xpe_error_string` returns human-readable English string for all 11 codes
7. `xpe_get_param_range` returns valid ranges for "CHEST" body part with "noiseReductionStrength" parameter

**Test Cases**:
1. Alloc 3072x3072 uint16 image -> expect `out.dataSize == 3072*3072*2 == 18,874,368` bytes, `out.format == XPE_PIXEL_UINT16`
2. Alloc 3072x3072 float32 image -> expect `out.dataSize == 3072*3072*4 == 37,748,736` bytes
3. Alloc 0x0 image -> expect `XPE_ERR_INVALID_INPUT`
4. Alloc 5000x5000 image -> expect `XPE_ERR_INVALID_INPUT` (exceeds 4096x4096 max from api-spec dataSize max 64MB)
5. Free a valid image, then verify `buf.data == NULL` and `buf.dataSize == 0`
6. Copy 3072x3072 uint16 -> mismatched 1024x1024 dst -> expect `XPE_ERR_BUFFER_TOO_SMALL`
7. `xpe_error_string(XPE_ERR_CALIBRATION_EXPIRED)` -> returns non-NULL string containing "expir" (case-insensitive)
8. `xpe_get_param_range("CHEST", "noiseReductionStrength", &min, &max, &def)` -> returns `XPE_OK`, min >= 0.0, max <= 1.0

**Definition of Done**:
- [ ] All 5 API functions compile and link into xpe_common.dll
- [ ] `dumpbin /exports xpe_common.dll` shows all 5 symbols
- [ ] All 8 test cases pass
- [ ] No memory leaks detected (alloc/free 10000 cycles)
- [ ] Struct sizes match P/Invoke alignment notes: XpeImageBuffer = 40 bytes, XpeImageMetadata = 96 bytes on x64

**Risk Items**:
- Memory alignment requirements on ARM vs x64 may differ (mitigate: `#pragma pack(push, 8)`)
- Parameter range database format not yet defined (use JSON or compile-time table)

---

### SPRINT-P0-03: Logging Subsystem

**Sprint ID**: SPRINT-P0-03
**Sprint Goal**: Implement the thread-safe logging subsystem with file output and level filtering.
**SWU Scope**: SWU-5.4 (Logger)
**API Functions**: `xpe_log_set_level`, `xpe_log_set_file`, `xpe_log_flush`
**Dependencies**: SPRINT-P0-02 (error types)
**Estimated Complexity**: Simple

**Acceptance Criteria**:
1. `xpe_log_set_level(2)` sets minimum level to INFO; DEBUG messages are discarded
2. `xpe_log_set_file("test.log")` redirects output from stderr to file in append mode
3. `xpe_log_flush` forces buffered entries to disk immediately
4. Log format: `[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [TID] message` per `xpe-implementation-reference.md` Section 9.1
5. Level values: 0=TRACE, 1=DEBUG, 2=INFO, 3=WARN, 4=ERROR, 5=OFF per `xpe-implementation-reference.md` Section 9.2
6. `xpe_log_set_level(6)` returns `XPE_ERR_INVALID_INPUT`
7. JSON log mode: `xpe_init("{\"logFormat\": \"json\"}")`; each line is `{"ts":"...","lvl":"...","tid":N,"msg":"..."}` per Section 9.4

**Test Cases**:
1. Set level to WARN(3), emit INFO message, read log file -> expect message NOT present
2. Set level to DEBUG(1), emit DEBUG message, read log file -> expect message present with correct format
3. Set file to "test.log", emit 100 messages, flush, read file -> expect 100 lines
4. Set level to OFF(5) -> expect no messages written regardless of severity
5. Call `xpe_log_set_level(-1)` -> expect `XPE_ERR_INVALID_INPUT`
6. Call `xpe_log_set_file(NULL)` -> expect revert to stderr, returns `XPE_OK`

**Definition of Done**:
- [ ] 3 API functions exported from xpe_common.dll
- [ ] All 6 test cases pass
- [ ] Log file output verified with correct timestamp format
- [ ] Thread safety: concurrent writes from 4 threads produce no interleaving within single lines
- [ ] `xpe_log_flush` completes in < 10ms

**Risk Items**:
- File I/O on Windows may have buffering issues with concurrent access (mitigate: use mutex + fflush)

---

### SPRINT-P0-04: Configuration and Lifecycle Management

**Sprint ID**: SPRINT-P0-04
**Sprint Goal**: Implement library initialization, shutdown, version reporting, and runtime configuration.
**SWU Scope**: SWU-5.6 (ConfigManager)
**API Functions**: `xpe_init`, `xpe_shutdown`, `xpe_version`, `xpe_configure`
**Dependencies**: SPRINT-P0-02 (error types), SPRINT-P0-03 (logging)
**Estimated Complexity**: Medium

**Acceptance Criteria**:
1. `xpe_init(NULL)` initializes with defaults and returns `XPE_OK`
2. `xpe_init("{}")` accepts empty JSON config and returns `XPE_OK`
3. `xpe_init("invalid json")` returns `XPE_ERR_CONFIG_INVALID`
4. `xpe_shutdown()` releases all resources; calling any XPE function after returns `XPE_ERR_NOT_INITIALIZED`
5. `xpe_version()` returns non-NULL static string matching `X.Y.Z` pattern
6. `xpe_configure` updates runtime settings; unknown keys are silently ignored
7. Double `xpe_init` without intervening `xpe_shutdown` returns `XPE_ERR_CONFIG_INVALID` or handles gracefully

**Test Cases**:
1. `xpe_init(NULL)` -> `XPE_OK`; `xpe_version()` -> matches `/^\d+\.\d+\.\d+/`
2. `xpe_init("{\"logLevel\": 3}")` -> `XPE_OK`; verify log level is WARN
3. `xpe_init("not json")` -> `XPE_ERR_CONFIG_INVALID`
4. `xpe_shutdown()` then `xpe_alloc_image(...)` -> `XPE_ERR_NOT_INITIALIZED`
5. `xpe_configure("{\"unknownKey\": 42}")` -> `XPE_OK` (forward-compatible)
6. `xpe_configure(NULL)` -> `XPE_ERR_INVALID_INPUT`

**Definition of Done**:
- [ ] 4 API functions exported
- [ ] JSON parsing via cJSON or nlohmann/json (single-header)
- [ ] All 6 test cases pass
- [ ] Init/shutdown cycle 100 times without leaks
- [ ] Version string baked at compile time from CMake project version

**Risk Items**:
- JSON library selection affects binary size (cJSON is ~50KB, nlohmann ~1MB)
- Global state management for init/shutdown requires careful singleton design

---

### SPRINT-P0-05: Alert Queue

**Sprint ID**: SPRINT-P0-05
**Sprint Goal**: Implement the alert ring buffer (Alert Queue) for warnings, errors, and diagnostic events.
**SWU Scope**: SWU-5.7 (AlertQueueManager)
**API Functions**: `xpe_get_pending_alert_count`, `xpe_get_pending_alert`, `xpe_clear_alerts`
**Dependencies**: SPRINT-P0-04 (lifecycle/config)
**Estimated Complexity**: Low

**Acceptance Criteria**:
1. Alert ring buffer holds at least 64 entries
2. `xpe_get_pending_alert_count` returns accurate count atomically
3. `xpe_get_pending_alert(0, msg, 256, &severity)` copies first alert message
4. `xpe_clear_alerts` empties the ring buffer
5. Alert message format: UTF-8 JSON matching the schema in `xpe-implementation-reference.md` Section 9.3 with fields `severity`, `code`, `message`, `timestamp_ms`, `stage_id`, `stage_name`, and `frame_index`
6. Defined alert codes enumerated in Section 9.3 (CALIB_EXPIRING_SOON, GSVG_PROCESSING_FAILED, etc.)

**Test Cases**:
1. Enqueue 10 alerts -> `xpe_get_pending_alert_count()` returns 10
2. `xpe_get_pending_alert(0, buf, 5, &sev)` with too-small buffer -> `XPE_ERR_BUFFER_TOO_SMALL`
3. `xpe_clear_alerts()` -> `xpe_get_pending_alert_count()` returns 0
4. Enqueue 100 alerts (exceed ring buffer) -> oldest alerts overwritten, count <= 64
5. Parse alert JSON: `severity`, `code`, `timestamp_ms`, `stage_id` fields are present and valid types

**Definition of Done**:
- [ ] 3 API functions exported (alert queue only)
- [ ] All 5 test cases pass
- [ ] Ring buffer is lock-free for single-producer/single-consumer scenario
- [ ] `xpe_get_pending_alert_count` is thread-safe (atomic read)

**Risk Items**:
- Ring buffer overflow policy needs documentation (currently: overwrite oldest)

---

### SPRINT-P0-06: Thread Pool, Test Infrastructure, and CI

**Sprint ID**: SPRINT-P0-06
**Sprint Goal**: Implement the internal thread pool and establish Google Test + CTest infrastructure with coverage reporting.
**SWU Scope**: SWU-5.2 (ThreadPool)
**API Functions**: None (ThreadPool is internal-only)
**Dependencies**: SPRINT-P0-04 (lifecycle manages thread pool start/stop)
**Estimated Complexity**: Medium

**Acceptance Criteria**:
1. Thread pool creates `N` worker threads (default: hardware_concurrency - 1)
2. Task submission returns immediately; tasks execute asynchronously
3. Thread pool gracefully shuts down during `xpe_shutdown()` (all pending tasks complete)
4. Google Test discovers and runs all unit tests via CTest
5. Coverage report generated in HTML format via gcov/lcov or OpenCppCoverage
6. CTest integration: `ctest --preset release` runs all tests and reports pass/fail
7. Minimum threshold check: coverage report script fails build if < 85%

**Test Cases**:
1. Submit 1000 increment tasks to thread pool -> final counter == 1000 (thread safety)
2. Submit blocking task, then shutdown -> expect task completes before shutdown returns
3. Submit task after shutdown initiated -> expect task rejected gracefully
4. `ctest --preset debug` discovers >= 1 test target and runs successfully
5. Coverage HTML report generated at `build/coverage/index.html`

**Definition of Done**:
- [ ] Thread pool operational with work-stealing or simple queue design
- [ ] Google Test framework compiles and links
- [ ] CTest discovers all test executables
- [ ] Coverage report generation automated in CI preset
- [ ] Thread pool handles 10000 small tasks without crash or deadlock

**Risk Items**:
- Windows thread pool semantics differ from POSIX (mitigate: use C++ `std::thread` + `std::condition_variable`)
- Coverage tools on MSVC may require OpenCppCoverage instead of gcov

---

### SPRINT-P0-07: RealXpeBackend Adapter + Module Directories + Full P/Invoke Integration

**Sprint ID**: SPRINT-P0-07
**Sprint Goal**: Implement `RealXpeBackend : IXpeBackend` backed by real `xpe_common.dll` P/Invoke calls, establish `xpe_common_api.h` header, scaffold all module directories, and validate all 18 P/Invoke signatures. This sprint connects GUI-S0's mock layer to the live DLL.
**SWU Scope**: SWU-5.7 (PipelineOrchestrator — real backend integration)
**API Functions**: All 18 xpe_common APIs via P/Invoke
**Dependencies**: SPRINT-GUI-S0 (IXpeBackend interface defined), SPRINT-P0-05, SPRINT-P0-06 (xpe_common.dll complete)
**Estimated Complexity**: Medium

**Acceptance Criteria**:
1. `RealXpeBackend : IXpeBackend` implemented in `src/gui/backend/RealXpeBackend.cs` using P/Invoke to xpe_common.dll
2. Application startup hot-swap: when `xpe_common.dll` is present, `RealXpeBackend` activates automatically; when absent, `MockXpeBackend` remains active (no code change required)
3. P/Invoke wrapper class (`XpeNative.cs`) declares all 18 xpe_common.dll functions with correct `DllImport` and `MarshalAs` attributes
4. C# can call `xpe_init(null)`, `xpe_version()`, `xpe_alloc_image(...)`, `xpe_free_image(...)` successfully via `RealXpeBackend`
5. Status bar shows real `xpe_version()` string from DLL when `RealXpeBackend` is active
6. All 9 module directories created: `modules/preprocess/`, `modules/enhance_basic/`, `modules/enhance_advanced/`, `modules/display/`, `modules/dicom/`, `modules/ai/`, `modules/gsvg/`, `modules/common/`, `tests/`
7. Each module directory has a stub `CMakeLists.txt` with correct target name
8. `xpe_common_api.h` includes all 18 API functions and compiles as standalone header
9. `dumpbin /exports xpe_common.dll` shows exactly 18 exported symbols

**Test Cases**:
1. Launch with `xpe_common.dll` present → status bar shows real version string (matches `xpe_version()` output, format `X.Y.Z`)
2. Launch without `xpe_common.dll` → MockXpeBackend activates, status bar shows "v0.0.0-mock" (no crash)
3. P/Invoke: `xpe_alloc_image(1024, 1024, XPE_PIXEL_UINT16, out buf)` → `XPE_OK`, `buf.dataSize == 2097152`
4. P/Invoke: `xpe_init(null)` then `xpe_shutdown()` → no crash, no leak
5. P/Invoke: `xpe_log_set_level(2)` → `XPE_OK`; log panel shows DLL log output
6. Verify all 9 module dirs exist: `ls modules/*/CMakeLists.txt` returns 9 files (including common)
7. `dumpbin /exports xpe_common.dll | grep -c "xpe_"` == 15 (base functions, no AED)
8. `Marshal.SizeOf<XpeImageBuffer>()` == 40; `Marshal.SizeOf<XpeImageMetadata>()` == 96

**Definition of Done**:
- [ ] `RealXpeBackend` in `src/gui/backend/RealXpeBackend.cs` connects to live DLL
- [ ] IXpeBackend hot-swap verified: mock active without DLL, real active with DLL
- [ ] All 18 P/Invoke signatures verified (`marshal attributes correct`)
- [ ] `XpeImageBuffer` C# struct size == 40 bytes (Marshal.SizeOf)
- [ ] `XpeImageMetadata` C# struct size == 96 bytes
- [ ] All 9 module directories scaffolded
- [ ] `xpe_common_api.h` is complete and compiles as standalone header
- [ ] Phase 0 gate checklist 100% complete
- [ ] **Benchmark manifest schema defined** (`data/benchmark/schema/manifest_schema.json` present, with required `BP-01` through `BP-10` family identifiers and mandatory fields)

**GUI Demo Evidence** (non-blocking — required at sprint review):
- [ ] Live demo: launch with DLL → real version in status bar; remove DLL → mock fallback activates

**Risk Items**:
- P/Invoke struct alignment may differ between Debug/Release builds (mitigate: test with Marshal.SizeOf in both configurations)
- WPF on .NET 8 requires Windows Desktop SDK

---

## Phase 1a: Pre-Processing

---

### SPRINT-P1A-01: Calibration Manager

**Sprint ID**: SPRINT-P1A-01
**Sprint Goal**: Implement the calibration data lifecycle: load, save, validate, generate, and expiry checking.
**SWU Scope**: SWU-1.5 (CalibrationManager)
**API Functions**: `xpe_calib_load_offset`, `xpe_calib_load_gain`, `xpe_calib_load_defect_map`, `xpe_calib_generate_offset`, `xpe_calib_check_expiry`, `xpe_calib_save`
**Dependencies**: SPRINT-P0-07 (xpe_common.dll complete)
**Estimated Complexity**: Medium

**Acceptance Criteria**:
1. Load offset/gain/defect maps from raw binary files
2. `xpe_calib_check_expiry` returns `XPE_ERR_CALIBRATION_EXPIRED` for past-dated files
3. `xpe_calib_generate_offset` averages N dark frames to produce offset map
4. `xpe_calib_save` writes map with embedded expiry timestamp
5. CRC-32 embedded in calibration files; load fails on mismatch
6. Startup load of all 3 calibration maps completes within 200ms budget

**Test Cases**:
1. Save offset map with expiry 2020-01-01 -> load -> expect `XPE_ERR_CALIBRATION_EXPIRED`
2. Save offset map with expiry 2030-01-01 -> load -> expect `XPE_OK`, `expiryEpochMsOut > now`
3. Generate offset from 32 synthetic dark frames (known mean=100) -> output map mean within +/-1 of 100
4. Corrupt 1 byte in saved calibration file -> load -> expect `XPE_ERR_IO_FAILED` (CRC mismatch)
5. Load 3072x3072 offset map -> timing < 50ms
6. `xpe_calib_load_offset(NULL, &out)` -> `XPE_ERR_INVALID_INPUT`

**Definition of Done**:
- [ ] 6 API functions exported from xpe_preprocess.dll
- [ ] All 6 test cases pass
- [ ] CRC verification implemented and tested
- [ ] Calibration file format documented (header: magic + version + width + height + format + expiry + CRC + data)
- [ ] Unit test coverage >= 85%

**Risk Items**:
- Calibration file format not yet standardized (mitigate: use a simple binary header format; see `xpe-implementation-reference.md` Section 1)

---

### SPRINT-P1A-02: Offset Correction and Gain Correction

**Sprint ID**: SPRINT-P1A-02
**Sprint Goal**: Implement the two mandatory pre-processing corrections: offset subtraction and gain normalization with uint16->float32 format conversion.
**SWU Scope**: SWU-1.1 (OffsetCorrector), SWU-1.2 (GainCorrector)
**API Functions**: `xpe_offset_correct`, `xpe_gain_correct`
**Dependencies**: SPRINT-P1A-01 (calibration data available)
**Estimated Complexity**: Medium

**Acceptance Criteria**:
1. `xpe_offset_correct` subtracts offset map in-place; result clamped to 0 (no negative uint16)
2. `xpe_gain_correct` applies flat-field normalization and converts uint16 -> float32 in-place
3. Offset correction on 3072x3072 completes within 60ms
4. Gain correction on 3072x3072 completes within 60ms
5. Dimension mismatch between image and map returns `XPE_ERR_INVALID_INPUT`
6. Multi-gain polynomial selection by exposure level is handled internally within gain correction

**Test Cases**:
1. Synthetic 100x100 image (all pixels=1000) with offset map (all=50) -> after offset: all pixels == 950
2. Synthetic image pixels=500, offset=600 -> after offset: all pixels == 0 (clamped, no underflow)
3. After gain correction: output format == `XPE_PIXEL_FLOAT32`
4. Gain map all=1.0 (unity) -> gain-corrected values match offset-corrected values (float32)
5. 3072x3072 offset correction timing: < 60ms (benchmark 100 iterations, take median)
6. Mismatched dimensions (1024x1024 image vs 2048x2048 map) -> `XPE_ERR_INVALID_INPUT`

**Definition of Done**:
- [ ] 2 API functions exported
- [ ] All 6 test cases pass
- [ ] **Scalar reference implementation exists** (plain C, no intrinsics) before SIMD path
- [ ] **Parity test**: scalar vs AVX2 outputs agree within 1e-5 relative tolerance on 1000-frame set
- [ ] AVX2 SIMD used for inner loops (when available)
- [ ] Format boundary: after gain, buffer is float32 (verified in test)
- [ ] Unit test coverage >= 85%

**Risk Items**:
- SIMD optimization may introduce platform-specific behavior (mitigate: scalar reference is always the fallback)

---

### SPRINT-P1A-03: Readout Validation, Temperature Compensation, Nonlinearity, and Binning

**Sprint ID**: SPRINT-P1A-03
**Sprint Goal**: Implement the four simpler pre-processing stages: readout validation, temperature compensation, nonlinearity correction, and binning correction.
**SWU Scope**: SWU-1.9 (ReadoutArtifactValidator), SWU-1.6 (TempCompensator), SWU-1.7 (NonlinearityCorrector), SWU-1.8 (BinningCorrector)
**API Functions**: `xpe_validate_readout_artifact`, `xpe_temp_compensate`, `xpe_nonlinearity_correct`, `xpe_binning_correct`
**Dependencies**: SPRINT-P1A-02 (offset/gain available for pipeline context)
**Estimated Complexity**: Medium

**Acceptance Criteria**:
1. Readout validation detects dropped columns (>10 consecutive zero-columns) and reports artifact score
2. Temperature compensation applies exponential dark current model when |T - 25C| > 2C
3. Nonlinearity correction applies polynomial LUT to linearize detector response
4. Binning correction adjusts pixel values based on binning mode (1x1, 2x2)
5. Each stage completes within its time budget: readout=15ms, temp=10ms, nonlinear=25ms, binning=15ms
6. Bypass conditions respected: binning skips when mode==1, temp skips within tolerance

**Test Cases**:
1. Readout: synthetic image with 5 consecutive zero columns -> `artifactScoreOut > WARN_THRESHOLD`
2. Readout: normal image -> `artifactScoreOut < WARN_THRESHOLD`
3. Temp compensation at 35C (10C above nominal): pixel values decrease (dark current subtracted)
4. Temp compensation at 25C: no change applied (within tolerance)
5. Nonlinearity: apply known polynomial `f(x) = 0.001*x^2 + x` to linearize, verify inverse
6. Binning mode=2 (2x2): output values adjusted by factor of 4 (area normalization)
7. Binning mode=1 (native): no modification to image

**Definition of Done**:
- [ ] 4 API functions exported
- [ ] All 7 test cases pass
- [ ] Bypass logic tested and verified (flags NOT set when bypassed per BYP-SAFE-003)
- [ ] Performance within budgets
- [ ] Unit test coverage >= 85%

**Risk Items**:
- Nonlinearity calibration coefficients source not yet defined (mitigate: configurable via JSON)

---

### SPRINT-P1A-04: Defect Pixel Correction

**Sprint ID**: SPRINT-P1A-04
**Sprint Goal**: Implement static BPM-based defect correction with edge-aware interpolation and runtime defect detection.
**SWU Scope**: SWU-1.3 (DefectPixelCorrector)
**API Functions**: `xpe_defect_correct`, `xpe_defect_detect_runtime`
**Dependencies**: SPRINT-P1A-02 (gain correction provides float32 input)
**Estimated Complexity**: Complex

**Acceptance Criteria**:
1. `xpe_defect_correct` replaces bad pixels identified in BPM with interpolated neighbors
2. Interpolation modes: nearest, bilinear, median (selectable via config JSON)
3. Edge-aware interpolation preserves edges near defect pixels
4. `xpe_defect_detect_runtime` identifies transient defects (hot/dead pixels) in current frame
5. Runtime detection output merges with static BPM for correction
6. Processing time < 110ms for 3072x3072 with typical BPM (0.1% defect rate)
7. `XPE_FLAG_DEFECT_CORRECTED` set after correction; NOT set when bypassed

**Test Cases**:
1. 100x100 image with known 3 dead pixels in BPM -> after correction: dead pixels replaced with neighbor average
2. Single hot pixel (value=65535) in uniform field (value=1000) -> after correction: pixel ~= 1000
3. Cluster defect (3x3 block) -> bilinear interpolation fills from surrounding 1-pixel border
4. Empty BPM (zero defects) + runtime detection disabled -> no modification, completes < 5ms
5. Runtime detection on image with 2 stuck pixels (always max) -> defectMapOut marks those 2 pixels
6. Config `{"interpolation_mode": "median"}` -> verify median filter applied instead of bilinear

**Definition of Done**:
- [ ] 2 API functions exported
- [ ] All 6 test cases pass
- [ ] Edge-aware interpolation preserves gradient across defect boundary (visual/PSNR test)
- [ ] Performance < 110ms for typical defect density
- [ ] Unit test coverage >= 85%
- [ ] BYP-SAFE-005: warning alert emitted when user-requested bypass with non-empty BPM

**Risk Items**:
- Cluster defect interpolation quality depends on cluster size (mitigate: limit cluster size to 5x5 for bilinear)
- Runtime detection false positive rate needs tuning (mitigate: configurable threshold)

---

### SPRINT-P1A-05: Ghost/Lag Correction - Tier 1 and Tier 2

**Sprint ID**: SPRINT-P1A-05
**Sprint Goal**: Implement ghost artifact correction with LTI multi-exponential deconvolution (Tier 1) and exposure-weighted LTI (Tier 2).
**SWU Scope**: SWU-1.4 (GhostCorrector) - partial
**API Functions**: `xpe_ghost_create`, `xpe_ghost_correct`, `xpe_ghost_reset`, `xpe_ghost_destroy`
**Dependencies**: SPRINT-P1A-02 (float32 input from gain correction)
**Estimated Complexity**: Complex

**Acceptance Criteria**:
1. `xpe_ghost_create` allocates corrector handle with configurable IRF parameters (N=4 exponentials)
2. `xpe_ghost_correct` applies LTI deconvolution using accumulated exposure history
3. Tier 1 first-frame lag residual < 0.5% of original exposure signal
4. Tier 2 first-frame lag residual < 0.35%
5. Auto-escalation from Tier 1 to Tier 2 when residual exceeds threshold_1
6. `xpe_ghost_reset` clears history; next frame treated as first acquisition
7. Tier 1+2 combined processing < 190ms for 3072x3072
8. Exposure history ring buffer holds 8 frames (~150MB)

**Test Cases**:
1. Create ghost handle -> `handleOut != NULL`, returns `XPE_OK`
2. Single frame correction (no history) -> bypass, no modification (first frame after create/reset)
3. Inject synthetic lag signal (1% of prior frame added to current) -> after Tier 1: residual < 0.5%
4. Inject 2% lag signal -> Tier 1 insufficient -> auto-escalate to Tier 2 -> residual < 0.35%
5. Reset handle -> next correction acts as first frame (no lag subtraction)
6. Destroy handle -> subsequent `xpe_ghost_correct(handle, ...)` returns `XPE_ERR_INVALID_INPUT`
7. Timing: 3072x3072 Tier 1 < 150ms, Tier 2 < 190ms

**Definition of Done**:
- [ ] 4 API functions exported
- [ ] All 7 test cases pass
- [ ] Tier escalation logic tested with explicit threshold values
- [ ] Memory: exposure history ring buffer allocates and recycles correctly
- [ ] Unit test coverage >= 85%
- [ ] BYP-SAFE-004: auto-bypass on first frame after reset

**Risk Items**:
- IRF parameter calibration requires real detector data (mitigate: use published parameters from Starman et al. and the default values documented in `xpe-implementation-reference.md` Section 4)
- 150MB exposure history may stress memory budget (mitigate: configurable history depth)

---

### SPRINT-P1A-06: Ghost Tier 3 (NLCSC) and Pre-Processing Pipeline Integration

**Sprint ID**: SPRINT-P1A-06
**Sprint Goal**: Implement NLCSC Tier 3 ghost correction and integrate the complete pre-processing pipeline with bypass logic.
**SWU Scope**: SWU-1.4 (GhostCorrector) - complete
**API Functions**: Same as P1A-05 (extended functionality within `xpe_ghost_correct`)
**Dependencies**: SPRINT-P1A-03, SPRINT-P1A-04, SPRINT-P1A-05
**Estimated Complexity**: Complex

**Acceptance Criteria**:
1. Tier 3 NLCSC with signal-dependent coefficients achieves first-frame lag <= 0.29%
2. Tier 3 processing < 240ms for 3072x3072
3. Auto-escalation chain: Tier 1 -> Tier 2 -> Tier 3 based on residual thresholds
4. Full pre-processing pipeline (stages 0.5 through 4) completes within 500ms
5. Bypass decision flowchart (pipeline-spec 1B.3) fully implemented
6. All `XPE_FLAG_*` bits correctly set/unset based on which stages executed
7. Diagnostic JSON log captures bypass decisions per BYP-SAFE-007
8. P/Invoke integration test: C# calls full pre-processing pipeline on synthetic image

**Test Cases**:
1. Inject 4% lag signal -> Tier 1 + Tier 2 insufficient -> Tier 3 -> residual <= 0.29%
2. Full pipeline on 3072x3072 synthetic image: timing < 500ms (Tier 1 path)
3. Full pipeline with all conditional stages bypassed: timing < 200ms
4. Verify flags: after full pipeline, `XPE_FLAG_GAIN_CORRECTED` set, `XPE_FLAG_GHOST_CORRECTED` set
5. Bypass ghost (first frame): `XPE_FLAG_GHOST_CORRECTED` NOT set
6. C# P/Invoke: load calib -> preprocess full image -> verify output is float32 and reasonable values
7. Memory leak test: process 1000 frames through full pipeline -> no memory growth

**Definition of Done**:
- [ ] xpe_preprocess.dll exports all 18 functions
- [ ] All 7 test cases pass
- [ ] Full pipeline benchmark: 3072x3072 < 500ms (Tier 1)
- [ ] Bypass policy (all 8 BYP-SAFE constraints) implemented and tested
- [ ] Memory leak test passes (1000 frames)
- [ ] P/Invoke integration test passes
- [ ] Phase 1a gate checklist 100% complete
- [ ] Unit test coverage >= 85%

**Risk Items**:
- NLCSC signal-dependent coefficient fitting requires calibration data
- Pipeline integration may reveal timing issues when all stages run together

---

## Phase 1b: Basic Enhancement + Display + DICOM + EI

---

### SPRINT-P1B-ENH-01: Log Transform and Noise Reduction

**Sprint ID**: SPRINT-P1B-ENH-01
**Sprint Goal**: Implement logarithmic transform and adaptive noise reduction (bilateral + NLM).
**SWU Scope**: SWU-2.1 (LogTransform), SWU-2.2 (NoiseReducer)
**API Functions**: `xpe_log_transform`, `xpe_log_inverse`, `xpe_noise_reduce`, `xpe_noise_estimate_sigma`
**Dependencies**: SPRINT-P1A-06 (pre-processing complete, float32 input)
**Estimated Complexity**: Medium

**Acceptance Criteria**:
1. Log transform compresses dynamic range; pixel values in log domain
2. Inverse log restores original linear values within floating-point precision
3. Noise reduction (bilateral) preserves edges while reducing quantum noise
4. `xpe_noise_estimate_sigma` returns noise standard deviation within 10% of known synthetic noise
5. Log transform < 40ms, noise reduction < 180ms for 3072x3072
6. NLM mode selectable via config JSON

**Test Cases**:
1. Log transform then inverse on synthetic image: PSNR > 60dB (near-lossless roundtrip)
2. Noise estimate: add Gaussian noise sigma=10.0 to uniform image -> estimated sigma in [9.0, 11.0]
3. Bilateral noise reduction on noisy image: output SNR improvement > 6dB
4. Log transform on all-zero image: returns `XPE_ERR_INVALID_INPUT` (log(0) undefined)
5. Config `{"method": "nlm"}` -> NLM algorithm applied instead of bilateral

**Definition of Done** ✅ COMPLETED 2026-04-19:
- [x] 4 API functions exported from xpe_enhance_basic.dll (실제 8개 export)
- [x] All 5 test cases pass
- [x] Performance within budgets
- [x] Unit test coverage >= 85%

**Risk Items**:
- NLM is computationally expensive; may need tiling or approximate version for 3072x3072

---

### SPRINT-P1B-ENH-02: Contrast Enhancement, Edge Enhancement, and EI Baseline

**Sprint ID**: SPRINT-P1B-ENH-02
**Sprint Goal**: Implement CLAHE contrast enhancement, USM edge sharpening, and whole-image Exposure Index baseline.
**SWU Scope**: SWU-2.3 (ContrastEnhancer), SWU-2.4 (EdgeEnhancer), SWU-2.10 (ExposureIndexCalc - Phase 1b portion)
**API Functions**: `xpe_contrast_enhance`, `xpe_edge_enhance`, `xpe_calc_exposure_index`
**Dependencies**: SPRINT-P1B-ENH-01 (log transform precedes these operations)
**Estimated Complexity**: Medium

**Acceptance Criteria**:
1. CLAHE enhances local contrast with configurable clip limit and tile size
2. Edge enhancement via USM with configurable strength and radius
3. `xpe_calc_exposure_index` computes IEC 62494-1 EI and DI for whole-image input
4. EI selects target EIT based on `meta->bodyPart`
5. Contrast enhancement < 130ms, edge enhancement < 90ms, EI calculation < 50ms
6. DI = 10 * log10(EI / EIT) formula correct

**Test Cases**:
1. CLAHE on low-contrast image: histogram spread increases (max-min range > 2x original)
2. Edge enhancement: sharp edge response measured; MTF at Nyquist improves by >= 10%
3. EI calculation on known calibration phantom: EI within 5% of expected value
4. DI calculation: EI=200, EIT=250 -> DI = 10*log10(200/250) = -0.969 (within 0.01 tolerance)
5. EI on zero image -> `XPE_ERR_PROCESSING_FAILED` (cannot compute EI for blank image)
6. EI with bodyPart="CHEST" vs "HAND" -> different EIT selected

**Definition of Done** ✅ COMPLETED 2026-04-19:
- [x] 3 API functions exported from xpe_enhance_basic.dll (total now 7)
- [x] All 6 test cases pass
- [x] IEC 62494-1 compliance documented
- [x] Performance within budgets
- [x] Unit test coverage >= 85%
- [x] `dumpbin /exports xpe_enhance_basic.dll` shows 8 symbols (including version)

**Risk Items**:
- IEC 62494-1 EIT lookup table needs body-part-specific values (mitigate: use AAPM TG-232 values in xpe-implementation-reference.md Section 3)

---

### SPRINT-P1B-ENH-03: P/Invoke Integration for xpe_enhance_basic.dll

**Sprint ID**: SPRINT-P1B-ENH-03
**Sprint Goal**: Complete P/Invoke wrappers and integration test for the basic enhancement DLL.
**SWU Scope**: None (integration)
**API Functions**: All 7 xpe_enhance_basic APIs via P/Invoke
**Dependencies**: SPRINT-P1B-ENH-02
**Estimated Complexity**: Simple

**Acceptance Criteria**:
1. C# P/Invoke declarations for all 7 functions with correct marshaling
2. End-to-end test: float32 image -> log -> noise reduce -> contrast -> edge -> EI -> verified output
3. EI value returned to C# matches native test within floating-point tolerance
4. No memory leaks in managed/unmanaged boundary

**Test Cases**:
1. C# calls all 7 functions sequentially on test image -> all return `XPE_OK`
2. EI value from C# matches C unit test value within 0.01%
3. Process 100 images through C# pipeline -> no memory growth (GC handles + P/Invoke correct)

**Definition of Done**:
- [ ] P/Invoke wrapper class complete
- [ ] All 3 test cases pass
- [ ] Marshal.SizeOf verification for all struct parameters

**Risk Items**:
- Float precision differences between C and C# (mitigate: use tolerance-based comparison)

---

### SPRINT-P1B-DISP-01: Modality LUT, VOI LUT, and Presentation LUT

**Sprint ID**: SPRINT-P1B-DISP-01
**Sprint Goal**: Implement the DICOM LUT pipeline: Modality, VOI (Linear/Sigmoid), and Presentation LUT with GSDF compliance.
**SWU Scope**: SWU-3.1 (ModalityLUT), SWU-3.2 (VoiLUT), SWU-3.3 (PresentationLUT)
**API Functions**: `xpe_modality_lut_apply`, `xpe_voi_lut_apply`, `xpe_voi_lut_apply_fast`, `xpe_voi_lut_apply_sequence`, `xpe_presentation_lut_apply`, `xpe_presentation_lut_check_display`
**Dependencies**: SPRINT-P0-07 (xpe_common.dll)
**Estimated Complexity**: Medium

**Acceptance Criteria**:
1. Modality LUT: `output = input * slope + intercept` applied correctly
2. VOI LUT LINEAR: window center=2048, width=4096 maps [0, 4095] to full output range
3. VOI LUT SIGMOID: smooth S-curve transition at window center
4. Fast LUT: pre-computed 65536-entry 8-bit LUT for real-time display
5. VOI LUT interactive latency <= 16ms for 3072x3072
6. Presentation LUT: GSDF perceptual linearization applied
7. `xpe_presentation_lut_check_display` returns compliance score [0.0, 1.0]
8. Batch VOI apply: 10 images processed faster than 10 individual calls

**Test Cases**:
1. Modality LUT: slope=1.0, intercept=0.0 (identity) -> output == input
2. Modality LUT: slope=2.0, intercept=-1024.0 -> pixel 1024 becomes 1024
3. VOI LINEAR: WC=2048, WW=4096, pixel=0 -> output=0; pixel=4095 -> output=max
4. VOI SIGMOID: WC=2048, WW=4096 -> output at WC is 50% of max
5. Fast LUT timing: 3072x3072 < 16ms (benchmark)
6. Presentation LUT check_display: returns value in [0.0, 1.0]
7. Batch apply 10 images vs 10 individual: batch is >= 20% faster

**Definition of Done**:
- [ ] 6 API functions exported from xpe_display.dll
- [ ] All 7 test cases pass
- [ ] VOI interactive latency <= 16ms verified
- [ ] DICOM PS 3.3 C.7.6.3.1.5 function types (LINEAR, LINEAR_EXACT, SIGMOID) all implemented
- [ ] Unit test coverage >= 85%

**Risk Items**:
- GSDF compliance measurement requires display luminance data (mitigate: return 1.0 when measurement unavailable)

---

### SPRINT-P1B-DISP-02: LUT Manager and P/Invoke Integration

**Sprint ID**: SPRINT-P1B-DISP-02
**Sprint Goal**: Implement LUT preset management (CRUD + auto-select) and P/Invoke integration for xpe_display.dll.
**SWU Scope**: SWU-3.4 (LUTManager)
**API Functions**: `xpe_lut_get_preset_count`, `xpe_lut_get_preset`, `xpe_lut_add_custom_preset`, `xpe_lut_remove_custom_preset`, `xpe_lut_auto_select`
**Dependencies**: SPRINT-P1B-DISP-01
**Estimated Complexity**: Simple

**Acceptance Criteria**:
1. Built-in presets include at least: "DEFAULT", "CHEST", "BONE", "SOFT_TISSUE"
2. `xpe_lut_get_preset_count` returns >= 4 (built-in presets)
3. Add custom preset -> count increases by 1
4. Remove custom preset -> count decreases; removing built-in returns error
5. Auto-select for bodyPart="CHEST" returns appropriate LUT preset name
6. P/Invoke: all 11 xpe_display functions callable from C#

**Test Cases**:
1. `xpe_lut_get_preset_count()` >= 4
2. `xpe_lut_get_preset(0, name, 64, desc, 256)` -> returns "DEFAULT" or similar
3. Add "MY_CUSTOM" preset -> count increases by 1 -> remove -> count back to original
4. Remove "DEFAULT" (built-in) -> `XPE_ERR_INVALID_INPUT`
5. Auto-select for "CHEST" metadata -> returns non-empty preset name
6. C# P/Invoke calls all 11 functions -> all succeed

**Definition of Done**:
- [ ] 5 additional API functions exported (total 11 for xpe_display.dll)
- [ ] All 6 test cases pass
- [ ] Preset persistence to JSON file
- [ ] P/Invoke integration complete
- [ ] Unit test coverage >= 85%

**Risk Items**:
- Preset file location/format needs standardization

---

### SPRINT-P1B-DICOM-01: DICOM Reader and Writer

**Sprint ID**: SPRINT-P1B-DICOM-01
**Sprint Goal**: Implement DICOM file read/write with DX IOD support and JPEG 2000 compression.
**SWU Scope**: SWU-4.1 (DicomReader), SWU-4.2 (DicomWriter)
**API Functions**: `xpe_dicom_read`, `xpe_dicom_query_dimensions`, `xpe_dicom_read_tag_string`, `xpe_dicom_write`, `xpe_dicom_write_j2k`, `xpe_dicom_set_tag_string`
**Dependencies**: SPRINT-P0-07 (xpe_common.dll, DCMTK dependency)
**Estimated Complexity**: Medium

**Acceptance Criteria**:
1. Read DICOM DX file: extract pixel data into XpeImageBuffer + metadata into XpeImageMetadata
2. Query dimensions without full decode: returns width, height, format
3. Read/write tag string for arbitrary DICOM tags
4. Write DICOM Part 10 file with correct transfer syntax
5. J2K lossless write: compressionRatio=1.0 produces lossless output
6. Round-trip: write then read produces identical pixel data (for lossless)
7. DICOM write includes SOP Class UID for DX IOD

**Test Cases**:
1. Read sample DX DICOM file -> `imgOut.width > 0`, `metaOut.bodyPart` populated
2. Query dimensions of 3072x3072 DICOM -> returns `(3072, 3072, XPE_PIXEL_UINT16)`
3. Read tag PatientName (0010,0010) -> returns non-empty string
4. Write image to DICOM -> file size > 0, readable by DCMTK `dcmdump`
5. Write J2K lossless (ratio=1.0) -> read back -> pixel data identical (bitwise)
6. Set tag then read tag -> value matches what was set

**Definition of Done**:
- [ ] 6 API functions exported from xpe_dicom.dll
- [ ] All 6 test cases pass
- [ ] DCMTK integration working (linked via vcpkg)
- [ ] Unit test coverage >= 85%

**Risk Items**:
- DCMTK build configuration on Windows can be complex
- JPEG 2000 support requires OpenJPEG codec plugin

---

### SPRINT-P1B-DICOM-02: GSPS, Network SCU, and P/Invoke Integration

**Sprint ID**: SPRINT-P1B-DICOM-02
**Sprint Goal**: Implement GSPS presentation state and DICOM network services, plus P/Invoke integration.
**SWU Scope**: SWU-4.3 (PresentationStateIO), SWU-4.4 (DicomNetworkSCU)
**API Functions**: `xpe_gsps_create`, `xpe_gsps_apply`, `xpe_dicom_cstore`, `xpe_dicom_cfind_mwl`
**Dependencies**: SPRINT-P1B-DICOM-01
**Estimated Complexity**: Medium

**Acceptance Criteria**:
1. GSPS create: generates valid DICOM Presentation State referencing source image
2. GSPS apply: renders annotations onto image in-place
3. C-STORE: sends DICOM file to mock SCP; returns `XPE_OK` on success
4. C-FIND MWL: queries mock SCP; returns JSON array of worklist items
5. Network timeout configurable; returns `XPE_ERR_NETWORK_FAILED` on timeout
6. P/Invoke: all 10 xpe_dicom functions callable from C#

**Test Cases**:
1. GSPS create with ROI annotation JSON -> output file is valid DICOM PS
2. GSPS apply onto blank image -> ROI overlay visible (non-zero pixel region)
3. C-STORE to localhost mock SCP -> returns `XPE_OK`
4. C-STORE to unreachable host -> returns `XPE_ERR_NETWORK_FAILED` within timeout
5. C-FIND MWL with empty query -> returns JSON array (may be empty)
6. C# calls all 10 functions -> all return expected results

**Definition of Done**:
- [ ] 4 additional API functions exported (total 10 for xpe_dicom.dll)
- [ ] All 6 test cases pass
- [ ] Mock SCP test infrastructure for network tests
- [ ] P/Invoke integration complete
- [ ] Unit test coverage >= 85%

**Risk Items**:
- DICOM network testing requires mock SCP (mitigate: use DCMTK storescp/findscp)
- GSPS format complexity (mitigate: support basic ROI only in v1)

---

### SPRINT-P1B-GUI-01: C# Pipeline Orchestrator and QA Test

**Sprint ID**: SPRINT-P1B-GUI-01
**Sprint Goal**: Implement the C# pipeline orchestrator that chains all Phase 1 DLLs and basic QA constancy test.
**SWU Scope**: SWU-5.7 (PipelineOrchestrator - Phase 1 complete), SWU-6.1 (QaConstancyTest)
**API Functions**: N/A (C# orchestration code)
**Dependencies**: All Phase 1b DLL sprints complete
**Estimated Complexity**: Medium

**Acceptance Criteria**:
1. PipelineOrchestrator loads Phase 1 DLLs dynamically with error handling
2. Full pipeline execution: Raw DICOM -> Pre-process -> Enhance -> EI -> Display -> DICOM Write
3. Phase 2/3 DLLs gracefully skipped when absent (log + degrade)
4. QA constancy test: measures SNR, uniformity, defect count on calibration phantom
5. Pipeline timing < 3000ms for 3072x3072 end-to-end
6. Memory peak <= 190MB during pipeline execution
7. GUI default screen is workflow-first: fixture/load, calibration, stage controls, run controls, before/after comparison
8. Health/readiness panels are accessible through Diagnostics, not dominant on the default workflow screen
9. Metrics and Reports areas expose pass/fail gates, latency, memory, SHA-256, module mode, and report artifact paths
10. Help area explains workflow steps, Off/On/Auto stage modes, fixture rules, report interpretation, and diagnostics meaning

**Test Cases**:
1. Load all Phase 1 DLLs -> no errors; Phase 2/3 not found -> logged, pipeline continues
2. Full pipeline on test DICOM -> output DICOM written, EI value in reasonable range (100-800)
3. Pipeline timing benchmark: < 3000ms median over 10 runs
4. Memory profiling: peak < 190MB
5. QA test on uniform calibration image: SNR reported, uniformity % reported
6. Pipeline with Phase 1 DLL missing -> throws descriptive exception, does not crash

**Definition of Done**:
- [ ] PipelineOrchestrator class complete with Phase 1 integration
- [ ] All 6 test cases pass
- [ ] Phase 1b gate checklist 100% complete
- [ ] Full integration test passes end-to-end
- [ ] QA constancy test produces report

**Risk Items**:
- DLL loading order on Windows requires explicit dependency management
- .NET 8 P/Invoke may have edge cases with struct marshaling on Release builds

---

## Phase 2: Advanced Enhancement + GSVG

---

### SPRINT-P2-ADV-01: Collimation Detection

**Sprint ID**: SPRINT-P2-ADV-01
**Sprint Goal**: Implement gradient+Hough-based collimation detection for ROI extraction.
**SWU Scope**: SWU-2.8 (CollimationDetector)
**API Functions**: `xpe_detect_collimation`
**EI ROI Refinement**: After collimation detection, the C# orchestrator re-invokes `xpe_calc_exposure_index` (from xpe_enhance_basic.dll, implemented in SPRINT-P1B-ENH-02) with the ROI-cropped image. No new native API is needed for this refinement.
**Dependencies**: SPRINT-P1B-ENH-02 (EI baseline in enhance_basic), SPRINT-P1A-06 (pipeline)
**Estimated Complexity**: Complex

**Acceptance Criteria**:
1. Collimation detection returns bounding rectangle (x0,y0)-(x1,y1) for primary beam
2. Detection accuracy: IoU >= 0.90 against ground truth for typical chest/extremity images
3. `XPE_FLAG_COLLIMATION_DETECTED` set on success
4. EI ROI refinement: orchestrator crops image to collimation ROI, re-invokes `xpe_calc_exposure_index`
5. ROI-based DI is more accurate than whole-image DI (closer to expected EIT)
6. Collimation detection < 140ms, EI calculation < 50ms

**Test Cases**:
1. Chest image with known collimation -> detected ROI IoU >= 0.90 vs ground truth
2. Full-field image (no collimation) -> detection returns full image bounds
3. Rotated collimation (15 degrees) -> detection still identifies primary beam
4. ROI-cropped EI vs whole-image EI: ROI value is closer to expected EIT
5. Detection on uniform image (no edges) -> returns full image bounds (graceful fallback)

**Definition of Done**:
- [ ] 1 API function exported from xpe_enhance_advanced.dll
- [ ] All 5 test cases pass
- [ ] Performance within budgets
- [ ] Unit test coverage >= 85%

**Risk Items**:
- Hough transform performance on large images (mitigate: downscale before detection)

---

### SPRINT-P2-ADV-02: Multiscale Processing and Fractional Processing

**Sprint ID**: SPRINT-P2-ADV-02
**Sprint Goal**: Implement multi-scale frequency decomposition and fractional-order differentiation enhancement.
**SWU Scope**: SWU-2.5 (MultiscaleProcessor), SWU-2.6 (FractionalProcessor)
**API Functions**: `xpe_multiscale_process`, `xpe_fractional_process`
**Dependencies**: SPRINT-P2-ADV-01 (pipeline context)
**Estimated Complexity**: Complex

**Acceptance Criteria**:
1. Multiscale: Laplacian pyramid decomposition with per-band enhancement coefficients
2. Enhancement coefficients derived from body part metadata
3. Fractional processing: order 0.0-2.0 configurable; order ~1.0 preserves edges
4. Multiscale processing < 250ms, fractional < 200ms for 3072x3072
5. Combined Phase 2 advanced processing + Phase 1 total <= 2500ms

**Test Cases**:
1. Multiscale with unity coefficients (all=1.0) -> output matches input (identity transform)
2. Multiscale with high-frequency boost (coeff[0]=2.0) -> edge contrast increased
3. Fractional order=0.0 -> output unchanged (identity); order=2.0 -> strong texture emphasis
4. Fractional order=-1.0 -> `XPE_ERR_INVALID_INPUT` (out of range)
5. Timing: multiscale + fractional combined < 450ms

**Definition of Done**:
- [ ] 2 API functions exported (total 3 for xpe_enhance_advanced.dll with P2-ADV-01)
- [ ] All 5 test cases pass
- [ ] P/Invoke integration verified
- [ ] Performance within budgets
- [ ] Unit test coverage >= 85%

**Risk Items**:
- Laplacian pyramid reconstruction may introduce boundary artifacts (mitigate: symmetric extension)
### SPRINT-P2-GSVG-01: Grid Detection and Suppression

**Sprint ID**: SPRINT-P2-GSVG-01
**Sprint Goal**: Implement anti-scatter grid detection and DWT-based grid suppression in gsvg.dll.
**SWU Scope**: SI-001 (GridDetector), SI-002 (GridSuppressor)
**API Functions**: `gsvg_detect_grid`, `gsvg_suppress_grid`, `gsvg_process`, `gsvg_process_ex`, `gsvg_version`, `gsvg_error_string`
**Dependencies**: SPRINT-P0-01 (build system, FFTW3 dependency)
**Estimated Complexity**: Complex

**Acceptance Criteria**:
1. Grid detection identifies grid frequency (lp/mm) and angle from image content
2. Grid suppression removes grid artifact while preserving anatomical detail
3. MTF degradation <= 5% after grid suppression
4. `gsvg_process_ex` outputs diagnostic JSON with detected parameters
5. SAFE-003: on any error, original buffer returned unmodified
6. GSVG processing < 400ms for 3072x3072
7. gsvg.dll is fully independent of xpe_common.dll

**Test Cases**:
1. Synthetic image with 70 lp/cm grid pattern -> detect returns freq=70, angle=0
2. Grid suppression on synthetic grid image -> grid artifact power reduced by >= 20dB in FFT
3. MTF measurement pre/post suppression: degradation <= 5%
4. `gsvg_process(pixels, w, h, &config)` with `gridFrequency_lp_per_mm=0` (auto-detect) -> succeeds
5. `gsvg_process` on non-grid image -> returns `GSVG_ERR_GRID_NOT_DETECTED`; buffer unmodified
6. `gsvg_version()` returns non-NULL version string

**Definition of Done**:
- [ ] 6 API functions exported from gsvg.dll
- [ ] All 6 test cases pass
- [ ] FFTW3 dynamic linking verified (GPL isolation)
- [ ] SAFE-003 contract tested
- [ ] Unit test coverage >= 85%

**Risk Items**:
- FFTW3 GPL license requires dynamic linking (FROZEN: maintain gsvg.dll as independent module)
- DWT-based suppression quality depends on grid alignment accuracy

---

### SPRINT-P2-GSVG-02: Virtual Grid and Scatter LUT

**Sprint ID**: SPRINT-P2-GSVG-02
**Sprint Goal**: Implement virtual grid synthesis and scatter LUT management, plus P/Invoke integration.
**SWU Scope**: SI-003 (VirtualGridGenerator), SI-004 (ScatterLUTManager)
**API Functions**: `gsvg_virtual_grid`, `gsvg_load_scatter_lut`
**Dependencies**: SPRINT-P2-GSVG-01
**Estimated Complexity**: Medium

**Acceptance Criteria**:
1. Virtual grid improves perceived contrast for images without physical grid
2. CNR improvement >= 90% of 6:1 physical grid equivalent
3. Scatter LUT loads from file and improves suppression quality
4. `gsvg_load_scatter_lut` replaces previous LUT
5. Phase 2 total pipeline <= 2500ms
6. Phase 2 peak memory <= 440MB

**Test Cases**:
1. Virtual grid on non-grid image -> CNR improvement >= 90% vs physical grid reference
2. Scatter LUT load from valid file -> `GSVG_OK`
3. Scatter LUT load from invalid file -> `GSVG_ERR_LUT_LOAD_FAILED`
4. Virtual grid with metadata (body part, kVp) -> tuning applied
5. Phase 2 complete pipeline timing < 2500ms

**Definition of Done**:
- [ ] 2 additional API functions exported (total 8 for gsvg.dll)
- [ ] All 5 test cases pass
- [ ] P/Invoke integration for all 8 GSVG functions
- [ ] Phase 2 gate checklist 100% complete
- [ ] Unit test coverage >= 85%

**Risk Items**:
- Virtual grid quality highly dependent on scatter model accuracy
- CNR measurement methodology needs standardization

---

## Phase 3: AI / Intelligence

---

### SPRINT-P3-AI-01: AI Worker Infrastructure and Body Part Recognition

**Sprint ID**: SPRINT-P3-AI-01
**Sprint Goal**: Implement the sandboxed AI worker process, IPC mechanism, and body part recognition model.
**SWU Scope**: SWU-2.7 (BodyPartRecognizer), AI worker infrastructure
**API Functions**: `xpe_ai_init`, `xpe_ai_shutdown`, `xpe_bodypart_recognize`
**Dependencies**: SPRINT-P0-07 (xpe_common.dll), ONNX Runtime dependency
**Estimated Complexity**: Complex

**Acceptance Criteria**:
1. `xpe_ai_init` launches sandboxed worker process (`xpe_ai_worker.exe`)
2. Worker crash does not crash host process (crash isolation verified)
3. Body part recognition: >= 15 categories (CHEST, HAND, WRIST, SPINE, PELVIS, ...)
4. Classification accuracy >= 95% on validation dataset
5. Worker IPC latency < 50ms overhead per inference call
6. `xpe_ai_shutdown` cleanly terminates worker

**Test Cases**:
1. `xpe_ai_init(modelDir, NULL)` -> `XPE_OK`; worker process visible in task list
2. Kill worker process externally -> next inference call returns error; host survives
3. Recognize chest X-ray -> `bodyPartOut == "CHEST"`, `confidenceOut > 0.90`
4. Recognize hand X-ray -> `bodyPartOut == "HAND"`, `confidenceOut > 0.85`
5. `xpe_ai_shutdown` -> worker process no longer in task list
6. Call recognize without init -> `XPE_ERR_NOT_INITIALIZED`

**Definition of Done**:
- [ ] 3 API functions exported from xpe_ai.dll
- [ ] All 6 test cases pass
- [ ] Crash isolation verified (worker kill + host survival)
- [ ] IPC mechanism operational (shared memory or named pipe)
- [ ] Unit test coverage >= 80% (Phase 3 relaxed threshold)

**Risk Items**:
- ONNX Runtime version compatibility across CPU/GPU
- IPC latency on Windows named pipes may exceed budget (mitigate: use shared memory)

---

### SPRINT-P3-AI-02: Image Stitching and Bone Suppression

**Sprint ID**: SPRINT-P3-AI-02
**Sprint Goal**: Implement AI-based image stitching and U-Net bone suppression inference.
**SWU Scope**: SWU-2.9 (ImageStitcher), SWU-2.11 (BoneSuppressionEngine)
**API Functions**: `xpe_stitch_images`, `xpe_stitch_estimate_size`, `xpe_bone_suppress`
**Dependencies**: SPRINT-P3-AI-01 (AI worker infrastructure)
**Estimated Complexity**: Complex

**Acceptance Criteria**:
1. Stitching: 2+ overlapping images merged into single wide-field image
2. Stitch alignment via AI feature matching; sub-pixel accuracy
3. `xpe_stitch_estimate_size` returns correct output dimensions
4. Bone suppression: PSNR >= 33dB, SSIM >= 0.97 vs reference
5. Phase 3 total processing <= 3000ms
6. Phase 3 peak memory <= 740MB

**Test Cases**:
1. Stitch 2 overlapping 1024x2048 images -> output 1024x~3800 (overlap region blended)
2. Estimate size for 2 images -> width/height within 5% of actual stitched output
3. Stitch with non-overlapping images -> `XPE_ERR_PROCESSING_FAILED`
4. Bone suppression on chest X-ray -> output has reduced bone contrast, PSNR >= 33dB
5. Bone suppression with DL_DISABLED -> stage skipped entirely

**Definition of Done**:
- [ ] 3 API functions exported
- [ ] All 5 test cases pass
- [ ] Quality metrics (PSNR, SSIM) automated in tests
- [ ] Unit test coverage >= 80%

**Risk Items**:
- Stitching accuracy depends on overlap percentage (mitigate: require >= 10% overlap)
- Bone suppression model size (~200MB) impacts memory budget

---

### SPRINT-P3-AI-03: DL Denoiser, AI Collimation, and Full Pipeline Integration

**Sprint ID**: SPRINT-P3-AI-03
**Sprint Goal**: Implement DL denoiser, AI collimation refinement, and integrate full Phase 3 pipeline.
**SWU Scope**: SWU-2.12 (DLDenoiser), SWU-2.8 ext (AI Collimation), SWU-1.3 ext (Defect ML)
**API Functions**: `xpe_dl_denoise`
**Dependencies**: SPRINT-P3-AI-01, SPRINT-P3-AI-02
**Estimated Complexity**: Complex

**Acceptance Criteria**:
1. DL denoiser selects model variant based on body part and mAs
2. DL denoise quality: PSNR improvement >= 3dB over classical bilateral
3. AI collimation refinement improves IoU by >= 5% over gradient+Hough baseline
4. Deterministic fallback: all AI features gracefully degrade when models unavailable
5. Full pipeline (Phase 1+2+3): total <= 3000ms, memory <= 740MB
6. MISRA C:2012 exempted for AI code; clang-tidy 0 warnings instead

**Test Cases**:
1. DL denoise on noisy chest image -> PSNR improvement >= 3dB vs `xpe_noise_reduce`
2. DL denoise with bodyPart="HAND", mAs=2.0 -> different model selected vs bodyPart="CHEST"
3. AI collimation on challenging case -> IoU improves vs Phase 2 baseline
4. All AI models removed from disk -> pipeline completes successfully (fallback path)
5. Full Phase 1+2+3 pipeline timing < 3000ms
6. Phase 3 memory peak < 740MB

**Definition of Done**:
- [ ] 1 API function exported (total 7 for xpe_ai.dll)
- [ ] All 6 test cases pass
- [ ] Deterministic fallback path 100% coverage
- [ ] clang-tidy 0 warnings on all Phase 3 code
- [ ] ONNX contract tests pass
- [ ] Worker crash recovery test passes
- [ ] Phase 3 gate checklist 100% complete
- [ ] Unit test coverage >= 80%

**Risk Items**:
- DL model training data availability
- AI inference latency variance between CPU and GPU

---

## Appendix: Sprint Summary Table

| Sprint ID | Phase | SWU | API Count | Complexity | Dependencies |
|-----------|:-----:|-----|:---------:|:----------:|:------------:|
| **SPRINT-GUI-S0** | **-1** | **5.7 (stub)** | **0 (mock)** | **Medium** | **None** |
| SPRINT-P0-01 | 0 | -- | 0 | Medium | None |
| SPRINT-P0-02 | 0 | 5.1, 5.3, 5.5 | 5 | Medium | P0-01 |
| SPRINT-P0-03 | 0 | 5.4 | 3 | Simple | P0-02 |
| SPRINT-P0-04 | 0 | 5.6 | 4 | Medium | P0-02, P0-03 |
| SPRINT-P0-05 | 0 | 5.8 | 6 | Medium | P0-04 |
| SPRINT-P0-06 | 0 | 5.2 | 0 | Medium | P0-04 |
| SPRINT-P0-07 | 0 | 5.7 (real) | 18 (P/Invoke) | Medium | GUI-S0, P0-05, P0-06 |
| SPRINT-P1A-01 | 1a | 1.5 | 6 | Medium | P0-07 |
| SPRINT-P1A-02 | 1a | 1.1, 1.2 | 2 | Medium | P1A-01 |
| SPRINT-P1A-03 | 1a | 1.6-1.9 | 4 | Medium | P1A-02 |
| SPRINT-P1A-04 | 1a | 1.3 | 2 | Complex | P1A-02 |
| SPRINT-P1A-05 | 1a | 1.4 (partial) | 4 | Complex | P1A-02 |
| SPRINT-P1A-06 | 1a | 1.4 (complete) | 0 | Complex | P1A-03-05 |
| SPRINT-P1B-ENH-01 | 1b | 2.1, 2.2 | 4 | Medium | P1A-06 |
| SPRINT-P1B-ENH-02 | 1b | 2.3, 2.4, 2.10 | 3 | Medium | P1B-ENH-01 |
| SPRINT-P1B-ENH-03 | 1b | -- | 7 (P/Invoke) | Simple | P1B-ENH-02 |
| SPRINT-P1B-DISP-01 | 1b | 3.1-3.3 | 6 | Medium | P0-07 |
| SPRINT-P1B-DISP-02 | 1b | 3.4 | 5 | Simple | P1B-DISP-01 |
| SPRINT-P1B-DICOM-01 | 1b | 4.1, 4.2 | 6 | Medium | P0-07 |
| SPRINT-P1B-DICOM-02 | 1b | 4.3, 4.4 | 4 | Medium | P1B-DICOM-01 |
| SPRINT-P1B-GUI-01 | 1b | 5.7, 6.1 | N/A | Medium | All P1B DLLs |
| SPRINT-P2-ADV-01 | 2 | 2.8 | 1 | Complex | P1B-ENH-02 |
| SPRINT-P2-ADV-02 | 2 | 2.5, 2.6 | 2 | Complex | P2-ADV-01 |
| SPRINT-P2-GSVG-01 | 2 | SI-001, SI-002 | 6 | Complex | P0-01 |
| SPRINT-P2-GSVG-02 | 2 | SI-003, SI-004 | 2 | Medium | P2-GSVG-01 |
| SPRINT-P3-AI-01 | 3 | 2.7 | 3 | Complex | P0-07 |
| SPRINT-P3-AI-02 | 3 | 2.9, 2.11 | 3 | Complex | P3-AI-01 |
| SPRINT-P3-AI-03 | 3 | 2.12, 2.8 ext | 1 | Complex | P3-AI-01-02 |

---

## Appendix: Critical Path

The critical path through the project is:

```
P0-01 -> P0-02 -> P0-04 -> P0-05 -> P0-07
  -> P1A-01 -> P1A-02 -> P1A-05 -> P1A-06
    -> P1B-ENH-01 -> P1B-ENH-02 -> P1B-GUI-01
      -> P2-ADV-01 -> P2-ADV-02
        -> P3-AI-01 -> P3-AI-02 -> P3-AI-03
```

**Parallelization opportunities**:
- Phase 1b: P1B-ENH, P1B-DISP, P1B-DICOM, P1B-GUI can run in parallel (all depend only on Phase 0+1a)
- Phase 2: P2-ADV and P2-GSVG can run in parallel (GSVG is independent of xpe_enhance_advanced)
- Phase 3: All 3 sprints are sequential (AI worker infrastructure required first)

---

## Appendix D: Cumulative Regression Test Chain

Each sprint must pass not only its own test cases but also all tests from previous sprints in the same phase. This ensures forward compatibility and prevents regressions.

### Phase 0 Regression Chain

| Sprint | Own Tests | Cumulative Regression |
|--------|:---------:|:---------------------:|
| P0-01 | Build system configuration | first sprint; no prior regression chain |
| P0-02 | Memory + Error + Param | P0-01: build still works with new source files |
| P0-03 | Logging | P0-02: alloc/free/error_string still pass |
| P0-04 | Config + Lifecycle | P0-03: logging still works after init/shutdown cycle |
| P0-05 | Alert Queue | P0-04: init/shutdown cycle includes alert cleanup |
| P0-06 | Thread Pool + Test Infra | P0-05: all previous tests via CTest framework |
| P0-07 | C# P/Invoke + Scaffolding | P0-06: all native tests + new P/Invoke smoke tests |

### Phase 1a Regression Chain

| Sprint | Own Tests | Cumulative Regression |
|--------|:---------:|:---------------------:|
| P1A-01 | Calibration | P0-07: P/Invoke still loads xpe_common |
| P1A-02 | Offset + Gain | P1A-01: calib load still passes |
| P1A-03 | Readout + Temp + Nonlin + Binning | P1A-02: offset/gain pipeline still correct |
| P1A-04 | Defect | P1A-03: all 4 stage corrections still pass |
| P1A-05 | Ghost Tier 1+2 | P1A-04: defect correction unaffected |
| P1A-06 | Ghost Tier 3 + Pipeline Integration | P1A-05: full pre-processing pipeline timing + memory |

### Phase 1b Regression Chain

| Sprint | Own Tests | Cumulative Regression |
|--------|:---------:|:---------------------:|
| P1B-ENH-01 | Log + Noise | P1A-06: pre-processing pipeline still < 500ms |
| P1B-ENH-02 | Contrast + Edge + EI | P1B-ENH-01: log/noise still correct |
| P1B-ENH-03 | P/Invoke Integration | P1B-ENH-02: all 7 enhance_basic APIs via C# |
| P1B-DISP-01 | LUT Pipeline | P0-07: xpe_common P/Invoke unaffected |
| P1B-DISP-02 | LUT Manager + P/Invoke | P1B-DISP-01: all 6 display APIs still pass |
| P1B-DICOM-01 | Reader + Writer | P0-07: xpe_common P/Invoke unaffected |
| P1B-DICOM-02 | GSPS + Network + P/Invoke | P1B-DICOM-01: read/write round-trip still passes |
| P1B-GUI-01 | Full Pipeline + QA | **Gate G1b**: full Phase 1 pipeline < 3000ms, memory < 190MB |

### Cross-Phase Regression

| Gate | Regression Requirement |
|------|----------------------|
| G0 -> G1a | All P0 unit tests pass + P/Invoke smoke test |
| G1a -> G1b | Full pre-processing pipeline (P1A-06 integration test) still < 500ms |
| G1b -> G2 | Full Phase 1 pipeline (P1B-GUI-01 integration test) still < 3000ms |
| G2 -> G3 | Phase 2 additions (P2-ADV + P2-GSVG) still within budget |
| G3 | Full pipeline (Phase 1+2+3) still < 3000ms, memory < 740MB |

---

## Appendix E: Test Data Dependencies

Each sprint requires specific test data. This appendix tracks dependencies to ensure data availability before sprint start.

### Test Data Inventory

| Data | Format | Size | Required By | Generation Method |
|------|--------|------|-------------|-------------------|
| **Synthetic uniform image** | uint16 raw | 18.9 MB (3072x3072) | P0-02 (alloc), P1A-02 (offset/gain) | Programmatic: all pixels = constant |
| **Synthetic gradient image** | uint16 raw | 18.9 MB | P1A-02 (gain), P1B-ENH-01 (log) | Programmatic: linear/2D gradient |
| **Synthetic noisy image** | float32 raw | 37.7 MB | P1B-ENH-01 (noise reduce) | Gaussian noise added to uniform |
| **Offset map** | uint16 raw | 18.9 MB | P1A-01, P1A-02 | Factory dark frame (all=50..100) |
| **Gain map** | float32 raw | 37.7 MB | P1A-01, P1A-02 | Unity gain (all=1.0) or synthetic flat-field |
| **Bad Pixel Map (BPM)** | uint8 raw | 9.4 MB | P1A-04 | Sparse defect map (0.1% density) |
| **Ghost exposure history** | float32 x8 | 301.6 MB | P1A-05, P1A-06 | Synthetic lag frames |
| **Nonlinearity LUT** | JSON + float array | ~128 KB | P1A-03 | Polynomial coefficients |
| **Temperature coefficients** | JSON | ~32 KB | P1A-03 | Exponential model params |
| **Calibration file (valid)** | Custom binary | ~19 MB | P1A-01 | Save + CRC embed |
| **Calibration file (expired)** | Custom binary | ~19 MB | P1A-01 | Save with past expiry date |
| **Calibration file (corrupted)** | Custom binary | ~19 MB | P1A-01 | Flip 1 byte in valid file |
| **Sample DICOM DX** | DICOM Part 10 | ~20 MB | P1B-DICOM-01 | Clinical sample or dcmtk gen |
| **Collimation test image** | float32 raw | 37.7 MB | P2-ADV-01 | Synthetic with known ROI mask |
| **Grid artifact image** | uint16 raw | 18.9 MB | P2-GSVG-01 | Sinusoidal grid pattern |
| **ONNX body-part model** | ONNX | ~80 MB | P3-AI-01 | MobileNet-v3 fine-tuned |
| **ONNX bone suppression model** | ONNX | ~200 MB | P3-AI-02 | U-Net trained model |

### Test Data Generation Strategy

1. **Programmatic synthesis** (preferred): Most test data can be generated in test setup code. No external files needed.
2. **Calibration file generation**: Use `xpe_calib_save` from P1A-01 to create test calibration files.
3. **DICOM test files**: Use DCMTK `dcm2pnm` / `dcmodify` tools or ship a single small reference DICOM.
4. **ONNX models**: Ship placeholder models (random weights) for Phase 3 test infrastructure. Real models are separate deliverables.

---

## Appendix F: Sprint Rollback Strategy

When a sprint fails validation (quality gate, performance budget, or regression test), follow this protocol:

### Rollback Decision Matrix

| Failure Type | Severity | Action | Approval |
|-------------|:--------:|--------|:--------:|
| Unit test failure (own tests) | LOW | Fix within sprint | Developer |
| Regression test failure | HIGH | Revert last commit, re-analyze | Tech Lead |
| Performance budget exceeded | MEDIUM | Profile, optimize within sprint | Developer |
| Memory leak detected | HIGH | Block merge, fix before proceeding | Tech Lead |
| P/Invoke marshaling error | HIGH | Revert C# changes, re-verify struct layout | Tech Lead |
| Quality gate failure (end-of-phase) | CRITICAL | Full phase review, may require multiple sprint reverts | Project Lead |

### Rollback Procedure

1. **Identify**: `git bisect` or `git log` to find the breaking commit
2. **Isolate**: Create `hotfix/sprint-{ID}-rollback` branch from last known good state
3. **Fix**: Apply targeted fix on hotfix branch, run all cumulative regression tests
4. **Validate**: Full regression chain (Appendix D) must pass before merge
5. **Document**: Record root cause in sprint retrospective notes

### Sprint Failure Escalation

```
Sprint fails validation
    |
    +-- Own test failure?
    |       YES -> Fix + re-test within sprint
    |       NO  -> Escalate to regression analysis
    |
    +-- Regression failure?
            YES -> git bisect to find breaking change
                    |
                    +-- Caused by current sprint?
                    |       YES -> Revert + fix approach
                    |       NO  -> File bug against offending sprint
                    |
                    +-- Performance regression?
                            YES -> Profile + optimize
                            NO  -> Investigate root cause
```

---

*Document End -- XPE-SPRINT-PLAN-001 v1.1.0*
