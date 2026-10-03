# SPEC-XPE-P0: Phase 0 Foundation

**Document ID**: SPEC-XPE-P0
**Version**: 1.3.1
**Date**: 2026-10-03
**Status**: Completed -- All deliverables implemented

> **Status note (2026-10-03, QA-A-231, #253)**: implementation and tests are ahead of this document. `xpe_common.dll` exports 16 functions (fresh DLL export table 16 = header `XPE_API` 16) and the 81 tests in `modules/common/tests` pass in CI common-build. v1.3.0 aligns the export count (formerly written as 15/18/20 across documents), the REQ wording that named non-existent error codes or described behaviour the code does not have, and the logging behaviour changed by QA-A-232. Items not yet settled are marked in place (v1.3.1: REQ-P0-023 level 5 settled as OFF, user decision 2026-10-03): REQ-P0-033 dicom version function (to be added, post QA-B-200 M2a), coverage/static analysis/ASan (to be introduced into CI, pre QA-A-234), REQ-P0-026~028a missing bodies, and the §3 acceptance items with no CI evidence.

**Changelog**:
- v1.0.0 -> v1.1.0: REQ-P0-009 corrected. REQ-P0-026~028 corrected to match api-spec.md v1.2.0 normative signatures. REQ-P0-028a added (XPE_STATUS_NO_EVENT). Per Cross-Validation Report v5.0 Round 8 findings R8-01, R8-02.
- v1.1.0 -> v1.2.0: All deliverables completed (11/11; originally written as 12/12, but §7 lists 11 deliverables — corrected 2026-10-03). Implementation summary added. Status changed to Completed.
- v1.2.0 -> v1.3.0 (2026-10-03, QA-A-231 / #253): export count unified to 16; REQ-P0-003/009/011/012/014/016/019/020/022/024/030/032 wording aligned to the code; notes added to REQ-P0-006/023/033, §2.5 (REQ-P0-026~028a bodies missing), §3, §8. Logging defaults aligned to QA-A-232 (default INFO, stderr logger installed by `xpe_init`, level 5 = OFF in code; QA-A-232 is on `dev/preprocess`, pending merge to main).
- v1.3.0 -> v1.3.1 (2026-10-03, QA-A-233 / #245 user decisions): REQ-P0-023 level 5 corrected CRITICAL -> OFF (decision 1); REQ-P0-006 and §3.2/§3.4/§3.5 notes now read "CI 도입 예정 — pre QA-A-234" (decision 6); REQ-P0-033 note records the decision to add `xpe_dicom_version` (post QA-B-200 M2a, decision 7). Change log: `docs/project/REQ-CHANGE-LOG-2026-10-03-WORDING.md`.
**Parent**: SPEC-XPE-MASTER v2.0.0
**Classification**: IEC 62304 Class B
**Sprint**: S0-A, S0-B, S0-C (parallel where possible)

---

## 1. Scope

Phase 0 establishes the foundation for all subsequent phases:

1. Complete build infrastructure with test framework
2. Full xpe_common.dll implementation (16 API functions)
3. C# WPF GUI scaffolding with P/Invoke bridge
4. Module directory scaffolding for all 8 DLLs
5. CI/CD pipeline with automated quality gates

**Out of Scope**: Any image processing algorithm, pipeline logic, calibration data handling.

---

## 2. EARS Format Requirements

### 2.1 Build System

**REQ-P0-001**: The system SHALL provide a CMake-based build system that compiles all module targets (xpe_common, xpe_preprocess, xpe_enhance_basic, xpe_enhance_advanced, xpe_ai, xpe_display, xpe_dicom, gsvg) from a single root CMakeLists.txt.

**REQ-P0-002**: The build system SHALL support at minimum 4 presets: Debug, Release, CI (RelWithDebInfo), and ci-common (common module only).

**REQ-P0-003**: The build system SHALL manage dependencies through vcpkg manifest mode with the following minimum dependencies: spdlog, nlohmann-json, fmt, gtest, dcmtk, openjpeg. Eigen3 is resolved by the root CMakeLists.txt (`find_package`, falling back to `FetchContent`); OpenCV is not used.

> Corrected 2026-10-03 (QA-A-231, #253). Previous wording listed "spdlog, nlohmann-json, fmt, opencv4, eigen3, dcmtk, gtest"; `vcpkg.json` contains neither opencv4 nor eigen3, and OpenCV appears in no build file.

**REQ-P0-004**: When a module directory is present, CMake SHALL include it; when absent, CMake SHALL skip it without error (optional subdirectory pattern).

### 2.2 Test Framework

**REQ-P0-005**: The test framework SHALL use Google Test (gtest) as the testing library, integrated with CTest for test discovery and execution.

**REQ-P0-006**: The test framework SHALL support coverage reporting (gcov/lcov) with a minimum statement coverage threshold of 85% for xpe_common.dll.

> **Coverage threshold per DLL (decision 2026-09-11, #120)** — `xpe_common.dll` and `xpe_preprocess.dll` (preset `coverage`) and the post-processing DLLs (preset `coverage-post`) are gated at **0.85**. `xpe_dicom.dll` (preset `coverage-dicom`) is gated at **0.80**: of its 229 uncovered instrumented lines at the 7th measurement (0.804), 75 are `catch(...)` handlers in `DicomValidator.cpp`/`dicom.cpp` proven unreachable through the public API (QA-B-34), 21 in `DicomNetworkSCU.cpp` are cancel-race or negotiation-impossible paths (QA-B-34 §6; the 19 dead `responseToJson` lines counted there were deleted by QA-B-35, which lowers the denominator rather than raising coverage), and most of the rest are openjpeg failure callbacks reachable only by fault injection. The requirement text itself binds only `xpe_common.dll`; the per-DLL gates above are the project policy. Revisit when fault injection is introduced.

> **Execution note (2026-10-03, QA-A-231, #253)** — the coverage gate is **manual only**: the `coverage` job in `ci.yml` runs on `workflow_dispatch`, not on push or pull request. The tool is OpenCppCoverage (Cobertura output), not gcov/lcov. The last manual run (2026-09-12, run 34662146043) measured line-rate 0.886 for the `coverage` preset, which aggregates every DLL in that preset; an `xpe_common.dll`-only figure has not been measured.

> **Status note (2026-10-03, QA-A-233 decision 6)** — CI 도입 예정 — pre QA-A-234, 사용자 결정 2026-10-03 (#245 코멘트). The user chose to bring coverage (on push/PR), cppcheck, clang-tidy, MISRA checks and ASan into CI rather than lower this text; the requirement text is unchanged. Until QA-A-234 lands, the execution note above describes the current state.

**REQ-P0-007**: Each API function SHALL have at minimum: (a) a happy-path test, (b) a null/invalid parameter test, and (c) a boundary condition test.

### 2.3 xpe_common.dll API

**REQ-P0-008**: xpe_common.dll SHALL export exactly 16 functions with C linkage (extern "C") and __declspec(dllexport) using the XPE_API macro.

**REQ-P0-009**: All functions SHALL use Pack=8 blittable types for P/Invoke compatibility. Struct types: XpeImageBuffer, XpeImageMetadata. Enum types: XpePixelFormat, XpeAlertSeverity. Error codes: `XpeErrorCode` is an `int32_t` alias whose values are the `XPE_OK` / `XPE_ERR_*` constants.

> Corrected 2026-10-03 (QA-A-231, #253). Previous wording listed XpeErrorCode under "Enum types"; `xpe_error.h` defines it as `typedef int32_t` plus `#define` constants.

**REQ-P0-010**: The error reporting mechanism SHALL use XpeErrorCode enum with xpe_error_string() for human-readable messages. Functions SHALL NOT throw C++ exceptions across the C ABI boundary.

### 2.4 Core Functions (12 existing)

**REQ-P0-011**: `xpe_init` SHALL initialize the library, set default logging to stderr at INFO level, and return XPE_OK on success.

> Corrected 2026-10-03 (QA-A-231 / QA-A-232, #253). `XPE_ERR_OK` does not exist; the success code is `XPE_OK`. The logging part matches the code as changed by QA-A-232 (commits `259e7810`, `183d1522`, merged to main on 2026-10-03 with the dev/preprocess batch): the default level is INFO (previously TRACE), `xpe_init` installs a stderr logger unless a log file is already in use, a level chosen before `xpe_init` is kept, and `xpe_shutdown` returns the level to INFO. The library's own init line goes through the same logger, so it follows `xpe_log_set_level` / `xpe_log_set_file`.

**REQ-P0-012**: `xpe_shutdown` SHALL release all library resources and flush logs. It returns `void` and SHALL be safe to call without a prior `xpe_init` (no crash).

> Corrected 2026-10-03 (QA-A-231, #253). Previous wording required a return code (`XPE_ERR_OK`, and `XPE_ERR_NOT_INITIALIZED` when not initialized); the function is declared `void xpe_shutdown(void)`. The test named `ShutdownReturnsNotInitializedWhenNotInit` asserts only that the call does not crash; its name is a lane-owned follow-up.

**REQ-P0-013**: `xpe_version` SHALL return a null-terminated UTF-8 string in format "X.Y.Z" (semantic versioning).

**REQ-P0-014**: `xpe_configure` SHALL accept a JSON configuration string and apply settings. Invalid JSON SHALL return XPE_ERR_CONFIG_INVALID.

> Corrected 2026-10-03 (QA-A-231, #253). `XPE_ERR_INVALID_PARAM` does not exist; the code returns `XPE_ERR_CONFIG_INVALID`, which the test `ConfigureWithInvalidJsonReturnsInvalid` asserts.

**REQ-P0-015**: `xpe_alloc_image` SHALL allocate an XpeImageBuffer with specified width, height, and pixel format. Allocated memory SHALL be zero-initialized.

**REQ-P0-016**: `xpe_free_image` SHALL deallocate an XpeImageBuffer. Calling it again on an already-freed buffer (`data == NULL`) SHALL return XPE_OK without crashing.

> Corrected 2026-10-03 (QA-A-231, #253). Previous wording required `XPE_ERR_INVALID_PARAM` (a non-existent code) on double free; the code returns `XPE_OK`, which the test `FreeImageNullDataDoesNotCrash` asserts. If `XPE_OK` is not the intended behaviour, the change belongs to a code card, not to this text.

**REQ-P0-017**: `xpe_copy_image` SHALL deep-copy source to destination buffer. Destination SHALL be allocated by caller.

**REQ-P0-018**: `xpe_error_string` SHALL return a static const char* description for any valid XpeErrorCode.

**REQ-P0-019**: `xpe_get_pending_alert_count` SHALL return the number of alerts in the alert queue. Reading an alert does not reduce the count.

**REQ-P0-020**: `xpe_get_pending_alert` SHALL copy the alert at the given index into the provided XpeAlertEntry. The queue is not modified; `xpe_clear_alerts` empties it.

> Corrected 2026-10-03 (QA-A-231, #253). Previous wording described "unread" alerts and a pop of the oldest alert; the header declares an `int32_t index` parameter and states the queue is unchanged, and the test `AlertQueueFifoOrder` reads by index 0 and 1.

**REQ-P0-021**: `xpe_clear_alerts` SHALL remove all alerts from the queue.

**REQ-P0-022**: `xpe_get_param_range` SHALL return the valid range and default value for the parameter identified by `(bodyPart, paramName)`.

> Corrected 2026-10-03 (QA-A-231, #253). Previous wording said "for the specified parameter ID"; the signature takes `(bodyPart, paramName, …)`.

### 2.5 Logging Subsystem (3 new functions)

**REQ-P0-023**: `xpe_log_set_level` SHALL set the minimum log level (TRACE=0, DEBUG=1, INFO=2, WARN=3, ERROR=4, OFF=5). Messages below the threshold SHALL be silently discarded; level 5 (OFF) discards every message.

> Corrected 2026-10-03 (QA-A-233 decision 1, user approval #245 comment). Previous wording ended the scale with "CRITICAL=5"; SRS-FUNC-040, the header `xpe_common_api.h` (`5=OFF`), `api-spec.md` and the code (QA-A-232 M1) all define level 5 as OFF. Recorded in `docs/project/REQ-CHANGE-LOG-2026-10-03-WORDING.md`.

**REQ-P0-024**: `xpe_log_set_file` SHALL redirect log output to the specified file path. If the file cannot be opened, the function SHALL return XPE_ERR_IO_FAILED and retain the previous output destination.

> Corrected 2026-10-03 (QA-A-231, #253). `XPE_ERR_FILE_IO` does not exist; the code returns `XPE_ERR_IO_FAILED`. Retention of the previous destination is asserted by the `CommonOom` test `ALogFileThatCannotBeOpenedLeavesThePreviousLogger`.

**REQ-P0-025**: `xpe_log_flush` SHALL force-flush all buffered log messages to the current output destination (file or stderr).

> **REQ-P0-026, REQ-P0-027, REQ-P0-028, REQ-P0-028a — body missing (note 2026-10-03, QA-A-231, #253)**: the v1.1.0 changelog records "REQ-P0-026~028 corrected" and "REQ-P0-028a added (XPE_STATUS_NO_EVENT)", and `tasks.md` lists these IDs, but no requirement text exists in this document (there is no §2.7). No other document under `.moai/specs` defines them, and no code or test cites them. The bodies are not reconstructed here; restoring them from `docs/project/api-spec.md` v1.2.0 or recording them as withdrawn is an open decision. Until then these IDs have no requirement text.

### 2.6 C# GUI Integration

**REQ-P0-029**: The ImageProcTest WPF project SHALL compile to a standalone .NET executable that loads xpe_common.dll via P/Invoke at runtime.

**REQ-P0-030**: The P/Invoke wrapper class SHALL declare all 16 xpe_common.dll functions with matching signatures. Struct layouts SHALL use [StructLayout(LayoutKind.Sequential, Pack=8)].

**REQ-P0-031**: On startup, the GUI SHALL call xpe_init() and display the version string from xpe_version(). On shutdown, the GUI SHALL call xpe_shutdown().

### 2.8 Module Scaffolding

**REQ-P0-032**: Each of the 7 module directories other than modules/common (modules/preprocess, modules/enhance_basic, modules/enhance_advanced, modules/ai, modules/display, modules/dicom, gsvg) SHALL contain a CMakeLists.txt with a minimal shared library target.

> Corrected 2026-10-03 (QA-A-231, #253). Previous wording said "Each of the 8 module directories" but listed 7; the eighth DLL is xpe_common, covered by §2.3.

**REQ-P0-033**: Each scaffolded module SHALL export a placeholder version function (e.g., xpe_preprocess_version) to verify DLL load.

> Status note (2026-10-03, QA-A-231, #253): not met for modules/dicom — no `xpe_dicom_version` exists (the other six modules have their version function). The requirement text is unchanged.
>
> 상태 메모 (2026-10-03, QA-A-233 결정 7): dicom 버전 함수 추가 예정 — post QA-B-200 M2a, 사용자 결정 2026-10-03 (#245 코멘트). 요구는 그대로 두고 코드를 요구에 맞춘다.

---

## 3. Acceptance Criteria

### 3.1 Build System Acceptance

- [ ] `cmake --preset release && cmake --build --preset release` succeeds with 0 errors
- [ ] All 8 module DLLs compile (xpe_common.dll + 6 XPE DLLs + gsvg.dll)
- [x] CTest discovers Google Test suite
- [ ] Coverage report generation works (`lcov --capture ...`)

### 3.2 xpe_common.dll Acceptance

- [x] `dumpbin /exports xpe_common.dll` lists exactly 16 functions
- [ ] All 16 functions have unit tests with >= 85% statement coverage
- [ ] Logging: file output + level filtering verified via test
- [ ] No memory leaks in 1000-cycle init/shutdown test (ASan clean) (CI 도입 예정 — pre QA-A-234, 사용자 결정 2026-10-03)
- [x] All structs verified Pack=8 via static_assert(sizeof == expected)

### 3.3 C# Integration Acceptance

- [ ] ImageProcTest.exe builds and runs
- [ ] xpe_common.dll loads via P/Invoke without DllNotFoundException
- [ ] xpe_version() returns correct string displayed in GUI
- [ ] xpe_init() / xpe_shutdown() lifecycle works from C#

### 3.4 Quality Gate Acceptance

- [ ] Static analysis: cppcheck --std=c++17 reports 0 warnings (not yet run by any CI workflow; CI 도입 예정 — pre QA-A-234, 사용자 결정 2026-10-03)
- [ ] clang-tidy: modernize-*, performance-*, bugprone-* reports 0 warnings (not yet run by any CI workflow; CI 도입 예정 — pre QA-A-234, 사용자 결정 2026-10-03)
- [ ] MISRA C:2012 Advisory: Pass (where applicable) (not yet run by any CI workflow; CI 도입 예정 — pre QA-A-234, 사용자 결정 2026-10-03)
  - Scope note (2026-10-03, lead): full MISRA C:2012 conformance checking requires a commercial checker, which the project does not have. QA-A-234 brings in cppcheck and clang-tidy only; any MISRA coverage is limited to what those free checkers report, and this item stays unchecked until a commercial checker is chosen.
- [ ] CI pipeline: all checks green on main branch

### 3.5 Acceptance evidence (note 2026-10-03, QA-A-231, #253)

Checked items and their evidence:
- CTest discovery: CI common-build runs the `modules/common/tests` gtest suites through ctest, 81/81 passed (run 37090327999).
- Export count: export table of a freshly built `xpe_common.dll` (local `ci-preprocess` build, 2026-10-03) lists 16 = 16 `XPE_API` declarations in the header, no mismatch. The count is now 16, not 15 (see REQ-P0-008). CI has no export-count step; the measurement read the PE export table directly because `dumpbin` was not installed.
- Pack=8: `xpe_types.h` has `#pragma pack(push, 8)` and 8 `static_assert`s (`XpeImageBuffer` = 40, `XpeImageMetadata` = 96, `XpePixelFormat` = 4, five offsets).

Unchecked items, with the reason:
- `cmake --preset release` / all 8 DLLs: no CI job builds the `release` preset or a configuration that builds all 8 modules at once (`ci-fullstack` is used by no workflow).
- Coverage report: the coverage job is manual (`workflow_dispatch`) and uses OpenCppCoverage, not lcov; see REQ-P0-006.
- ≥ 85% per function set: only an aggregated preset figure exists (0.886, 2026-09-12); an `xpe_common.dll`-only figure has not been measured.
- Logging file output + level filtering: no test on main reads log content after filtering. QA-A-232 adds `xpe_common_logging_tests` (stderr/file capture), merged to main on 2026-10-03; check this item once a CI run on main shows it passing.
- ASan: `MemoryLeakTestThousandCycles` runs under ctest, but no preset or CI job enables a sanitizer yet (CI 도입 예정 — pre QA-A-234, 사용자 결정 2026-10-03).
- §3.3 C# items: the `dotnet-tests` job succeeded (run 37090327999) but its log was not read; display of the version string was not observed.
- §3.4 static analysis: cppcheck, clang-tidy and MISRA appear in no workflow, preset or `tools/ci` script yet. The user decided on 2026-10-03 (#245 comment) to introduce them into CI rather than lower the text: CI 도입 예정 — pre QA-A-234, 사용자 결정 2026-10-03.

---

## 4. Implementation Dependencies

### 4.1 BLOCKER: api-spec.md v1.2.0

Before implementing REQ-P0-023 through REQ-P0-025 (Logging), the following must be published:

1. **api-spec.md v1.2.0** with:
   - Section 5.13-5.15: Logging function signatures and behavior (already exists)
   - Section 4: Updated function count (xpe_common = 16; corrected 2026-10-03 from 15, #253)
   - Correct Total = 82 (not 79)

2. **xpe_common_api.h** must declare:
   - Existing 12 functions (already present)
   - 3 Logging functions (already present: xpe_log_set_level, xpe_log_set_file, xpe_log_flush)
   - `xpe_alert_push` (renamed from `xpe_test_inject_alert`, #111) — a production export called by other modules; 12 + 3 + 1 = 16 (added 2026-10-03, #253)

### 4.2 Parallel Work (No Blockers)

The following tasks can start immediately:

- S0A-05: Google Test + CTest integration
- S0A-06: Module directory scaffolding
- S0A-07: CI pipeline
- S0C-01~04: C# WPF scaffolding

### 4.3 Blocked Work (Awaiting api-spec v1.2.0)

- S0B-01: Complete xpe_common_api.h (16 declarations)
- S0B-02: Logging subsystem implementation
- S0B-07: Full unit tests for all 16 APIs
- S0B-08: P/Invoke compatibility test

---

## 5. Test Plan

### 5.1 Unit Test Requirements

| Function Category | Test Count | Coverage Target |
|-------------------|:----------:|:---------------:|
| Core lifecycle (init/shutdown/version/configure) | >= 12 | >= 90% |
| Memory (alloc/free/copy) | >= 9 | >= 90% |
| Error + Alert (error_string, 3x alert) | >= 8 | >= 85% |
| Parameter validation | >= 4 | >= 85% |
| Logging (set_level/set_file/flush) | >= 6 | >= 85% |
| **Total** | **>= 39** | **>= 85%** |

### 5.2 Integration Test Requirements

| Test | Description |
|------|-------------|
| P/Invoke round-trip | C# calls all 16 functions, verifies return values |
| Memory lifecycle | Allocate 1000 images, verify no leak |
| Logging file output | Set file, log at each level, verify file contents |
| Concurrent access | 2 threads calling init/shutdown alternately |

### 5.3 Regression Test Requirements

| Test | Description |
|------|-------------|
| ABI compatibility | C# struct sizeof matches C++ struct sizeof |
| Export verification | dumpbin output matches expected 16-function list |
| Build preset validation | All 4 presets compile cleanly |

---

## 6. Risks

| Risk | Impact | Mitigation |
|------|--------|-----------|
| api-spec v1.2.0 delayed | Blocks 6 API implementations | Start parallel work (S0A, S0C) immediately |
| Pack=8 struct mismatch between C/C# | P/Invoke crash | Static_assert on C++ side, [StructLayout] on C# side |
| spdlog version incompatibility | Build failure | Pin version in vcpkg.json |
| Google Test integration complexity | CI failure | Use FetchContent for gtest, verify locally first |

---

## 7. Deliverables Checklist

| ID | Deliverable | REQ Ref | Sprint | Status |
|----|------------|---------|--------|:------:|
| P0-01 | CMake root build system | REQ-P0-001,004 | S0-A | ✅ DONE |
| P0-02 | CMakePresets.json | REQ-P0-002 | S0-A | ✅ DONE |
| P0-03 | vcpkg.json SOUP manifest | REQ-P0-003 | S0-A | ✅ DONE |
| P0-04 | cmake/ helpers | REQ-P0-001 | S0-A | ✅ DONE |
| P0-05 | xpe_common.dll 16 API | REQ-P0-008 to 028 | S0-B | ✅ DONE |
| P0-06 | Google Test + CTest + coverage | REQ-P0-005,006,007 | S0-A | ✅ DONE |
| P0-07 | ImageProcTest WPF scaffolding | REQ-P0-029,030,031 | S0-C | ✅ DONE |
| P0-08 | CI pipeline | REQ-P0-001 | S0-A | ✅ DONE |
| P0-09 | Module directory scaffolding | REQ-P0-032,033 | S0-A | ✅ DONE |
| P0-10 | xpe_common_api.h complete header | REQ-P0-008,009 | S0-B | ✅ DONE |
| P0-11 | Logging subsystem | REQ-P0-023,024,025 | S0-B | ✅ DONE |

---

## 8. Implementation Summary

### 8.1 Completion Status

All 11 deliverables for SPEC-XPE-P0 have been successfully implemented (count corrected 2026-10-03 from 12; §7 lists 11):

**Build Infrastructure** (P0-01 ~ P0-04):
- CMake 3.25+ based build system with 4 presets (Debug, Release, CI, ci-common)
- vcpkg manifest mode dependency management
- Optional subdirectory pattern for 8 modules (7 XPE + 1 GSVG)
- cmake/ helper scripts for cross-platform builds

**Core Implementation** (P0-05, P0-10 ~ P0-11):
- xpe_common.dll with 16 exported API functions (C linkage, Pack=8 structs)
- Complete API coverage: lifecycle (4), memory (3), error/alert (5, including `xpe_alert_push`), logging (3), param (1)
- xpe_common_api.h unified header with all declarations
- P/Invoke compatibility verified via static_assert on struct sizes

**Testing Infrastructure** (P0-06):
- Google Test 1.14.0 integrated via FetchContent
- CTest integration with custom targets (check, check_verbose)
- Coverage support (OpenCPPCoverage on Windows, gcov/lcov on Unix)
- Test structure: tests/common/ (unit tests) + tests/common_smoke/ (integration) — superseded: both directories were removed on 2026-09-10 (`bf6348f7`, their two unique assertions moved); the tests live in `modules/common/tests` (note 2026-10-03, #253)

**C# Integration** (P0-07):
- ImageProcTest WPF application (.NET 8)
- P/Invoke wrapper with [StructLayout(LayoutKind.Sequential, Pack=8)]
- All 16 functions declared with correct signatures
- Version display and lifecycle management (init/shutdown)

**Module Scaffolding** (P0-09):
- 8 module directories with CMakeLists.txt
- Placeholder version functions for all modules except modules/dicom (no `xpe_dicom_version`; see REQ-P0-033 note, 2026-10-03)
- Optional subdirectory pattern verified

**CI/CD Pipeline** (P0-08):
- GitHub Actions workflow (.github/workflows/ci.yml)
- Multi-stage pipeline: Configure → Build → Test → Coverage (the Coverage stage runs only on manual `workflow_dispatch`, not on push; note 2026-10-03, #253)
- Artifact upload for test results and coverage reports

### 8.2 Key Technical Decisions

1. **C++ Standard**: C++17 (unified from root CMakeLists.txt, removing C++23 override in modules/common)
2. **Build Generator**: Ninja (faster parallel builds)
3. **Package Manager**: vcpkg manifest mode with third_party/ overrides
4. **Testing Strategy**: Google Test + CTest with coverage reporting
5. **ABI Compatibility**: Pack=8 structs with compile-time size verification
6. **Logging**: spdlog integration with file output and level filtering

### 8.3 Quality Metrics

- **API Count**: 16 functions (exactly as specified in REQ-P0-008; 15 public API + `xpe_alert_push` (renamed from `xpe_test_inject_alert`, #111), a production export called by `enhance_basic/src/exposure_index.cpp`, not a test-only symbol. Measured: dumpbin 16 == header 16, #111; re-measured 16 == 16 on a fresh DLL 2026-10-03, QA-A-231)
- **Test Coverage**: Infrastructure ready for 85%+ coverage
- **Export Verification**: export table confirms 16 exports (15 public API + `xpe_alert_push`)
- **P/Invoke Compatibility**: static_assert ensures C#/C++ ABI match
- **Documentation**: IEC 62304 Class B compliant documentation set

### 8.4 Next Steps

Phase 0 foundation is complete. Recommended next phases:
- **SPEC-XPE-P1A**: Pre-processing module (Gain/Offset correction, Bad pixel correction)
- **SPEC-XPE-P1B**: Basic enhancement (CLAHE, Noise reduction)
- **SPEC-XPE-P2**: Advanced enhancement (Ghost correction, AI processing)

---

## Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0.0 | 2026-04-14 | MoAI | Initial Phase 0 Sub-SPEC from cross-validated master plan |
| 1.1.0 | 2026-04-16 | MoAI | All deliverables completed (11/11). Implementation summary added. Status changed to Completed. |
| 1.3.0 | 2026-10-03 | lead | QA-A-231 / #253: export count unified to 16; REQ-P0-003/009/011/012/014/016/019/020/022/024/030/032 aligned to the code; status notes on REQ-P0-006/023/033 and the missing REQ-P0-026~028a bodies; §3 checked where evidence exists (§3.5); §8 corrected. Logging defaults aligned to QA-A-232. Version labels unified (header previously 1.2.0, this table and the footer 1.1.0; the 1.1.0 row above describes the change the header changelog calls v1.2.0). |
| 1.3.1 | 2026-10-03 | lead | QA-A-233 / #245 user decisions: REQ-P0-023 level 5 = OFF (decision 1); coverage/cppcheck/clang-tidy/MISRA/ASan notes changed from "planned" to "CI 도입 예정 — pre QA-A-234" (decision 6); REQ-P0-033 dicom version function to be added (post QA-B-200 M2a, decision 7). |

---

*Document End -- SPEC-XPE-P0 v1.3.1*
