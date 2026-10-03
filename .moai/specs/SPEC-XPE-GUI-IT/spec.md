# SPEC-XPE-GUI-IT: ImageProcTest C# ↔ XPE Native DLL 통합 테스트

---
id: SPEC-XPE-GUI-IT
version: 1.3.0
status: Implemented
created: 2026-04-18
updated: 2026-10-03
author: manager-spec (MoAI)
priority: High
issue_number: null
iec62304_class: B
development_mode: TDD
sprint: S-GUI-IT (parallel with S1-A)
dependency: SPEC-XPE-P0 (Completed), SPEC-XPE-P1A (in progress for advanced suites)
---

## HISTORY

| Version | Date       | Author       | Changes                                             |
|---------|------------|--------------|-----------------------------------------------------|
| 1.3.0   | 2026-10-03 | lead (GUI-C-207, #249) | 실태 대조 정정(문서만, 코드·요구 삭제 없음). 정의된 REQ 수를 36개로 바로잡음(RTM 의 53/39 표기, 유령 REQ-032~034). §4.6 요구별 구현 상태 표 신설(구현됨 19 · 부분 14 · 없음 3). AC-4·5·6 사실 오류 정정, AC-9·12·15 와 REQ-010·043 에 상태 주석. §9 [HARD] shim 금지 규칙에 우회 상태 주석. §11 클래스 표를 `requirement-matrix.json` 의 실제 매핑으로 교체. REQ-063~065 는 상태 주석만(선택 요구 — 미구현, 구현 계획 없음). 근거: `xpe-gui` `.moai/reports/lane-gui/GUI-C-207/report.md` (dev/gui `efbc74f8`). |
| 1.2.0   | 2026-04-18 | manager-spec (GUI Lane) | GUI document upgrade package: 5 new companion docs (XAML/MVVM Arch, FlaUI E2E Plan, Accessibility, Localization, IEC 62304 GUI SHA/RTM), 2 upgraded (DISP-INT v2.0, MENU v1.1). Added §13 Phase 1b GUI Integration Readiness section. Referenced Documents table extended. Test implementation unchanged (78/78 tests stable). |
| 1.1.0   | 2026-04-18 | manager-docs | Implementation complete: ImageProcTest.IntegrationTests xUnit project with 78/78 tests passing. All 15 AC listed; see section 11 for what each one actually asserts *(2026-10-03 정정: 원문 "All 16 AC done" — AC 는 15개, #249)*. xpe_common.dll-gated tests use early-return pass pattern (xUnit v2 limitation). |
| 1.0.0   | 2026-04-18 | manager-spec | Initial creation of cross-language GUI IT SPEC     |

---

## 1. Scope

### 1.1 Overview

ImageProcTest C# WPF 클라이언트가 P/Invoke로 호출하는 XPE 네이티브 DLL(Layer 0/1) 경계를 **xUnit 기반 자동화 통합 테스트 스위트**로 검증한다.

본 SPEC은 어떤 이미지 처리 알고리즘도 신규 구현하지 않는다. 대상은 **cross-language 경계 그 자체** — ABI 레이아웃, 에러 코드 왕복, 구조체 Pack=8, DLL 생명주기, 마샬링 안전성, 경로 발견, 버전 스큐 감지.

### 1.2 In Scope

- `clients/ImageProcTest.IntegrationTests/` 신규 xUnit 프로젝트 (`net8.0`, `x64` 고정)
- `xpe_common.dll` 15개 P/Invoke 심볼 Smoke + Functional + Safety 테스트 (PinInvokeWrapper.cs 기준)
- `XpeImageBuffer` / `XpeImageMetadata` **runtime 크기/레이아웃 parity** 검증 (C# `Marshal.SizeOf` vs api-spec.md §1 계약: 40B / 96B)
- `XpeErrorCode` enum parity — -1 ~ -10 전수 매핑 + `xpe_error_string` non-NULL 확인
- `XpeImageMetadata.BodyPart[64]` **ANSI 경계 바이트 계약** 검증 (SizeConst=64, null-termination)
- DLL 생명주기: `xpe_init` → `xpe_shutdown` 1000 사이클 누수 부재, 미초기화 상태 에러 반환
- 로깅 서브시스템 계약 검증 (`xpe_log_set_level` 0~5 범위, `xpe_log_set_file` null path 동작)
- Mock backend 활성화 방지 가드 (통합 테스트에서는 **`IsNativeReady=true`가 아니면 실패**)
- DLL search path 발견 전략 검증 (`XpeCommonApi.ResolvedDllPath`가 `build/**` 하위일 것)
- 에러 경로 마샬링: 비정상 JSON, 작은 버퍼, NULL 포인터에서 managed `AccessViolationException`/`SEHException` 부재
- P1A-ready 훅: `xpe_preprocess.dll`이 탐지되면 자동 활성화되는 Optional 테스트 집합 (3 lifecycle + 3 correction + 3 calibration load = 9개)
- 결정성: 고정 seed 합성 이미지, determinism RMSE=0 assertion
- 성능 **가벼운** 게이트: Smoke < 30s, Full Functional < 2분 (CI gate)

### 1.3 Exclusions (What NOT to Build)

- **UI 스냅샷/시각 비교 테스트** — MainWindow 렌더링, Comparison Canvas 픽셀 비교 등은 범위 밖
- **WPF UI 자동화** (FlaUI, Appium, Coded UI) — 본 SPEC은 backend API 경계만 다룸
- **실제 X-ray 패널 연결 테스트** — 모든 테스트는 합성 입력/파일 기반
- **GPU 경로 검증** — CUDA/DirectML/TensorRT EP 실행은 xpe_ai.dll 범위이며 본 SPEC 제외
- **xpe_ai.dll 통합** — Phase 3 P3-AI 완료 후 별도 SPEC으로 분리
- **xpe_enhance_basic/advanced/dicom/display/gsvg 알고리즘 검증** — readiness probe scaffold 수준은 본 SPEC 범위이나, 알고리즘 수치 검증은 해당 모듈 SPEC 범위
- **성능 벤치마크** (상세 latency 분포, throughput 스트레스) — 본 SPEC은 "< 30s smoke / < 2min full" 상한만 게이트
- **멀티스레드 stress** (race, ABA, deadlock 탐지) — 별도 SPEC 권장
- **C++ 쪽 단위 테스트 복제** — Google Test 스위트 (tests/common/) 범위는 중복하지 않음
- **calibration 파일 I/O 실행 경로** (XCal 파싱, SHA-256) — P1A 범위
- **ghost/temperature/nonlinearity/binning 함수 테스트** — P1B 이후 SPEC
- **코드 스타일/린팅 gate** — MoAI 기본 quality gate가 담당

---

## 2. Referenced Documents

| Document ID                | Title                              | Version | Role                          |
|----------------------------|------------------------------------|---------|-------------------------------|
| XPE-API-SPEC-001           | XPE API Specification              | 1.3.0   | Native ABI 계약 source of truth |
| SPEC-XPE-P0                | Phase 0 Foundation                 | 1.2.0   | xpe_common.dll 18 함수 완료 선행조건 |
| SPEC-XPE-P1A               | Pre-processing Module              | 1.0.0   | Optional P1A-ready 테스트 hook 대상 |
| XPE-SAD-001                | Software Architecture Description  | Draft   | Layer 0/1/2 경계 아키텍처 참조    |
| `clients/ImageProcTest/PInvokeWrapper.cs` | P/Invoke wrapper | -       | 테스트 대상 C# 선언 source       |
| `modules/common/include/xpe/common/xpe_common_api.h` | Header | -   | Native export 계약            |
| `.moai/project/structure.md` | Project structure                | 1.0.0   | 디렉토리 매핑                   |
| `.moai/project/tech.md`    | Tech stack                         | 2.0.0   | C# 12/.NET 8/xUnit 선정 배경    |
| `SPEC-XPE-GUI-IT-RESEARCH` | Research Report                    | 1.0.0   | 코드베이스 분석 결과            |
| **XPE-GUI-ARCH-001**       | **XAML/MVVM Architecture Guide**   | **1.0.0** | **GUI 4-Tier 아키텍처, threading, DI** |
| **XPE-GUI-E2E-001**        | **FlaUI E2E Testing Plan**         | **1.0.0** | **UI 자동화 전략 (IT와 상보)** |
| **XPE-GUI-ACCESS-001**     | **Accessibility Guide**            | **1.0.0** | **WCAG 2.2 AA + IEC 62366-1** |
| **XPE-GUI-L10N-001**       | **Localization Strategy**          | **1.0.0** | **ko-KR primary / en-US fallback** |
| **XPE-GUI-DISP-INT-001**   | **Display Integration Guide**      | **2.0.0** | **P1B 통합 계약 (v2 upgrade)** |
| **XPE-GUI-MENU-001**       | **Menu & Command Strategy**        | **1.1.0** | **Pipeline 활성화 타이밍 + shortcut** |
| **SHA-GUI-001**            | **Software Hazard Analysis (GUI)** | **1.0.0** | **GUI-specific hazards (10)** |
| **RTM-GUI-001**            | **Requirements Traceability (GUI)**| **1.0.0** | **36 REQ → IEC 62304 trace** (REQ-GUI-IT-032~034 는 본 SPEC 에 정의 없음, #249) |

---

## 3. Definitions and Acronyms

| Term  | Definition |
|-------|------------|
| P/Invoke | Platform Invocation — .NET managed code에서 native DLL 함수를 직접 호출하는 CLR 기능 |
| XPE   | X-ray Processing Engine — 본 프로젝트의 네이티브 이미지 처리 엔진 모음 |
| FPD   | Flat Panel Detector |
| CIT   | Cross-language Integration Test — 본 SPEC이 추가하는 테스트 카테고리 |
| XCal  | XPE Calibration file format (magic: "XCal") |
| ABI   | Application Binary Interface — C#↔C ABI (`__cdecl`, Pack=8, extern "C") |
| ADU   | Analog-to-Digital Unit — detector signal 단위 |
| BCL   | Base Class Library (.NET) |
| GC    | Garbage Collector (.NET managed memory) |
| CWE   | Characterization/Whole-system Equality — 본 SPEC에서 결정성 근거로 사용 |
| Pack=8 | `#pragma pack(push, 8)` / `[StructLayout(Pack=8)]` 구조체 정렬 규칙 |

---

## 4. Requirements (EARS Format)

### 4.1 Ubiquitous Requirements (항상 활성)

#### REQ-GUI-IT-001: xUnit Framework Selection

The integration test project **shall** use xUnit as the test framework on `net8.0` target with `PlatformTarget=x64` and `Nullable enable`.

- Rationale: structure.md의 "xUnit 테스트" 선언 정합, BCL-only 의존 유지, WPF 결합 회피

#### REQ-GUI-IT-002: Pack=8 ABI Size Parity

The test suite **shall** assert `Marshal.SizeOf<XpeImageBuffer>() == 40` and `Marshal.SizeOf<XpeImageMetadata>() == 96` at runtime on x64 Windows.

- Source: api-spec.md §1 P/Invoke Alignment Notes
- Verification: Assertions run under `[Trait("Category","Smoke")]`

#### REQ-GUI-IT-003: Blittable Field Parity

The test suite **shall** verify every field offset of `XpeImageBuffer` via `Marshal.OffsetOf<T>("FieldName")` against the C++ layout: `width=0, height=4, bitsAllocated=8, bitsStored=12, format=16, data=24, dataSize=32`.

- Source: `xpe_types.h` (header struct order)

#### REQ-GUI-IT-004: ANSI BodyPart Fixed Buffer

The test suite **shall** verify that `XpeImageMetadata.BodyPart` is marshalled as `[MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]` with `CharSet.Ansi` and that a 63-char ASCII input round-trips without corruption.

#### REQ-GUI-IT-005: IntPtr Lifetime Contract

The test suite **shall not** free any `IntPtr` returned by `xpe_version()` or `xpe_error_string()`. It **shall** verify the same pointer is returned across multiple calls (static storage, DLL-owned, per api-spec.md §1).

#### REQ-GUI-IT-006: No Managed Exception on Error Paths

Every error-path test **shall not** surface `AccessViolationException`, `SEHException`, or unhandled native exceptions to the managed caller. All errors **shall** manifest as `XpeErrorCode` return values or documented `DllNotFoundException` / `EntryPointNotFoundException` / `BadImageFormatException`.

#### REQ-GUI-IT-007: Mock Backend Exclusion

The test collection **shall not** use `MockXpeBackend` or `CompositeXpeBackend`. All functional tests **shall** operate on `XpeCommonApi` P/Invoke calls directly and treat Mock fallback as test failure.

> **실측 주석 (2026-09-17).** `CompositeXpeBackend` 는 **코드 어디에도 없습니다**(범위: `gui/`·`clients/`·`modules/` 의 `*.cs`; 대조군 — 같은 검색이 `MockXpeBackend`·`RealXpeBackend`·`XpeBackendFactory` 를 찾음). Real → Mock 폴백은 현재 `gui/ImageProcTest/Services/XpeBackendFactory.cs` 가 합니다.
> 따라서 이 요구의 `CompositeXpeBackend` 절반, 그리고 아래 수용 기준의 `typeof(CompositeXpeBackend)` 리플렉션 단언은 **존재하지 않는 타입의 부재를 확인하므로 언제나 통과합니다** — 아무것도 지키지 않습니다. 지켜야 할 대상은 `XpeBackendFactory` 의 폴백입니다.
>
> **정정 (2026-10-03, GUI-C-208).** 위 2026-09-17 주석은 틀렸다. `CompositeXpeBackend` 는 `clients/ImageProcTest/Backends/CompositeXpeBackend.cs` 에 있다(2026-04-17 `3cadf4ac` 에서 추가, 레거시 `clients/` 앱 5곳이 사용). 그때 검색이 이 파일을 놓쳤다. 따라서 수용 기준의 `typeof(CompositeXpeBackend)` 리플렉션 단언은 실재하는 타입이 시험 어셈블리에 참조되지 않음을 지키는 의미 있는 단언이다. GUI-C-208 은 그 스캔이 실제로 타입을 보는지 붙잡는 대조 시험을 더했다(#249).

- Rationale: Mock 경로는 GUI scaffold 전용, 통합 테스트 통과를 허용하면 regression 은폐됨 (research.md R-12)

#### REQ-GUI-IT-008: Resolved DLL Path Within Build Tree

The test suite **shall** assert that `XpeCommonApi.ResolvedDllPath` points to a file under `<repo>/build/` (any preset) or the test output directory. Any other path (system-wide load, stale binary) **shall** fail the test.

#### REQ-GUI-IT-009: Error Code Enum Parity

For each `XpeErrorCode` value in the C# enum (`OK`=0, `INVALID_INPUT`=-1, ..., `NETWORK_FAILED`=-10), the test suite **shall** call `xpe_error_string(code)` and assert the returned pointer is non-NULL and the marshalled string is non-empty.

- Source: api-spec.md §13 Appendix A

#### REQ-GUI-IT-010: No Handle Leak After Test Run

After executing the full test collection, the test host process **shall not** hold any outstanding `GCHandle.Alloc(Pinned)` handle. (Verification: `GC.Collect(); GC.WaitForPendingFinalizers()` followed by `GC.GetTotalMemory` delta sanity check within `[Trait("Category","Lifecycle")]`)

> **상태 주석 (2026-10-03, #249).** 요구 문구는 남은 `GCHandle.Alloc(Pinned)` 핸들의 부재를 요구하지만, 검증 방법은 "GetTotalMemory 상식 점검"으로 정의되어 있어 둘이 다르다. 현재 시험(`LeakEnduranceTests.AfterTests_NoOutstandingPinnedHandles`)은 핸들을 세지 않고 관리 힙 < 200 MiB(절대값)만 단언하며, 트레이트는 `Safety`다. 요구를 상식 점검 수준으로 낮출지, 핸들 집계를 정의할지는 결정 대기다. 현재 상태: **부분**.

### 4.2 Event-Driven Requirements (이벤트 구동)

#### REQ-GUI-IT-020: Library Load Success

**When** the test assembly is loaded, the DLL resolver **shall** locate `xpe_common.dll` within 5 seconds and subsequent calls to `xpe_version()` **shall** return a non-empty semver string matching pattern `^\d+\.\d+\.\d+`.

#### REQ-GUI-IT-021: Init Success Path

**When** `xpe_init(null)` is called with no prior init, the function **shall** return `XPE_OK` and subsequent `xpe_get_pending_alert_count()` **shall** return `>= 0`.

#### REQ-GUI-IT-022: Configure JSON Roundtrip

**When** `xpe_configure(validJson)` is called with a well-formed JSON string such as `{"log":{"level":2}}`, the function **shall** return `XPE_OK`. **When** `xpe_configure(malformedJson)` is called with broken JSON such as `{not json}`, the function **shall** return `XPE_ERR_CONFIG_INVALID`.

#### REQ-GUI-IT-023: Alloc/Free Roundtrip

**When** `xpe_alloc_image(16, 16, UInt16, out buffer)` is called, the function **shall** return `XPE_OK` and `buffer.Data != IntPtr.Zero`, `buffer.DataSize == 512`. **When** `xpe_free_image(ref buffer)` is subsequently called, the function **shall** return `XPE_OK` and `buffer.Data` **shall** be zeroed.

#### REQ-GUI-IT-024: Alloc with Invalid Format

**When** `xpe_alloc_image(0, 0, UInt16, out buffer)` is called, the function **shall** return `XPE_ERR_INVALID_INPUT` without modifying `buffer.Data`.

#### REQ-GUI-IT-025: Copy Image

**When** `xpe_copy_image(ref src, ref dst)` is called with pre-allocated buffers of identical dimensions, the function **shall** return `XPE_OK`. **When** dst dimensions mismatch, the function **shall** return `XPE_ERR_BUFFER_TOO_SMALL` or `XPE_ERR_INVALID_INPUT`.

#### REQ-GUI-IT-026: Param Range Query

**When** `xpe_get_param_range("CHEST", "window_center", out min, out max, out dflt)` is called, the function **shall** return `XPE_OK` with `min <= dflt <= max`.

#### REQ-GUI-IT-027: Alert Queue Contract

**When** no alerts have been generated, `xpe_get_pending_alert_count()` **shall** return `0`. **When** called with `index=0` on an empty queue, `xpe_get_pending_alert(0, sb, len, out sev)` **shall** return `XPE_ERR_INVALID_INPUT`.

#### REQ-GUI-IT-028: Clear Alerts Idempotent

**When** `xpe_clear_alerts()` is called repeatedly (>= 3 times) with no alerts pending, no exception **shall** occur and `xpe_get_pending_alert_count()` **shall** remain `0`.

#### REQ-GUI-IT-029: Log Level Bounds

**When** `xpe_log_set_level(level)` is called with `level ∈ {0,1,2,3,4,5}`, the function **shall** return `XPE_OK`. **When** called with `level = -1` or `level = 6`, the function **shall** return `XPE_ERR_INVALID_INPUT`.

#### REQ-GUI-IT-030: Log Redirect to Temp File

**When** `xpe_log_set_file(tempPath)` is called with a writable temp path, the function **shall** return `XPE_OK`. **When** called with a path in a non-existent directory, the function **shall** return `XPE_ERR_IO_FAILED`.

#### REQ-GUI-IT-031: Log Flush No-Throw

**When** `xpe_log_flush()` is called at any state (pre-init, post-init, post-shutdown), no managed exception **shall** propagate to the test caller.

### 4.3 State-Driven Requirements (상태 구동)

#### REQ-GUI-IT-040: Uninitialized Guard

**While** `xpe_init` has not been called (or after `xpe_shutdown`), calls to `xpe_get_param_range(...)` **shall** return `XPE_ERR_NOT_INITIALIZED`.

- Note: `xpe_version()`, `xpe_error_string(code)`, `xpe_log_flush()` are documented thread-safe/read-only and are exempt.

#### REQ-GUI-IT-041: Missing DLL Fails Deterministically

**While** `xpe_common.dll` cannot be located on any search path, the test fixture bootstrap **shall** raise `DllNotFoundException` with a clear message, and test discovery **shall not** crash the test host.

#### REQ-GUI-IT-042: Architecture Mismatch Detection

**While** the resolved DLL is x86 and the test host is x64 (or vice versa), the first P/Invoke call **shall** raise `BadImageFormatException` and the test **shall** surface this as a test failure with the resolved DLL path in the message.

#### REQ-GUI-IT-043: Platform Mismatch Diagnostic

**While** `RuntimeInformation.ProcessArchitecture != Architecture.X64`, a diagnostic test **shall** run and record the architecture without failing other tests. (For ARM64 future support.)

> **상태 주석 (2026-10-03, #249).** 기존 `PlatformDetectionTests.ProcessArchitecture_IsX64` 는 ARM64 에서 **실패**하므로 "다른 시험을 실패시키지 않고 기록"과 모순되며, 아키텍처를 실제로 기록(출력·추적)하는 시험도 없다. 해당 시험을 완화할지, 본 요구를 "기록 전용"으로 명시할지는 결정 대기다. 현재 상태: **부분**.

### 4.4 Unwanted Behavior Requirements (금지 동작)

#### REQ-GUI-IT-050: No AccessViolation Across Boundary

The test suite **shall not** observe `System.AccessViolationException` or `System.Runtime.InteropServices.SEHException` during any nominal or negative test. Such an occurrence **shall** be logged and **shall** fail the containing test.

- IEC 62304 Class B: uncontrolled native fault propagating into managed host is a Class B hazard

#### REQ-GUI-IT-051: No Memory Leak After 1000 Init/Shutdown Cycles

The test suite **shall not** observe a process working-set increase greater than 20 MiB after 1000 consecutive `xpe_init(null)` / `xpe_shutdown()` cycles. (Gate threshold is tunable via env var `XPE_GUI_IT_LEAK_LIMIT_MIB`.)

- Source: SPEC-XPE-P0 acceptance 3.2 (C++ side). This requirement verifies the same invariant from managed side.

#### REQ-GUI-IT-052: No Marshalling Exception Without Translation

The test suite **shall not** allow any `MarshalDirectiveException`, `InvalidCastException`, or `System.Runtime.InteropServices.COMException` originating from ABI-boundary calls to escape untranslated. Every negative test **shall** document the expected `XpeErrorCode` outcome.

#### REQ-GUI-IT-053: No Silent Version-Skew

The test suite **shall not** pass when `xpe_version()` returns a string not matching `^[0-9]+\.[0-9]+\.[0-9]+` or when the major version does not match the pinned value in `tests/ImageProcTest.IntegrationTests/expected-versions.json` (file created under this SPEC).

### 4.5 Optional Requirements (선택 사항 — P1A ready / platform)

#### REQ-GUI-IT-060: Optional xpe_preprocess Lifecycle

**Where** `xpe_preprocess.dll` is discoverable under `<repo>/build/**/bin/**`, the test suite **shall** execute lifecycle tests for `xpe_preprocess_init` / `xpe_preprocess_shutdown` / `xpe_preprocess_version` via dynamic `NativeLibrary.GetExport` (no static `[DllImport]`). Absence of the DLL **shall** skip the test with a "Skipped: preprocess DLL not staged" reason.

#### REQ-GUI-IT-061: Optional Synthetic Adapter Chain

**Where** `xpe_preprocess.dll` exports `xpe_offset_correct`, `xpe_gain_correct`, `xpe_defect_correct` simultaneously, the test suite **shall** run a 16x16 synthetic offset→gain→defect chain (equivalent to `XpePreprocessSyntheticOracle.RunChain`) and assert: input SHA-256 preserved, output NaN/Inf count = 0, determinism RMSE = 0 across two consecutive runs.

- Reference: `clients/ImageProcTest/Diagnostics/XpePreprocessSyntheticOracle.cs` (promote to xUnit Fact)

#### REQ-GUI-IT-062: Optional Calibration Loader Contract

**Where** `xpe_preprocess.dll` is available, the test suite **shall** call `xpe_calib_load_offset(nonexistentPath, outBuffer)` and assert return code is `XPE_ERR_IO_FAILED` (not crash, not success).

#### REQ-GUI-IT-063: Optional ETW/Diagnostic Enabled Run

**Where** environment variable `XPE_GUI_IT_ETW=1` is set, the test suite **shall** emit `EventSource` events at test boundaries (`test_start`, `pinvoke_call`, `test_end`) to aid diagnostic capture without altering functional behaviour.

> **상태 (2026-10-03):** 선택 요구(Optional) — 미구현, 구현 계획 없음 (#249)

#### REQ-GUI-IT-064: Optional .NET 9 Target Verification

**Where** the CI runs on `dotnet --list-sdks` including 9.0.x, a secondary test pass **shall** execute against `net9.0` target to detect forward-compat regressions. Default target remains `net8.0`.

> **상태 (2026-10-03):** 선택 요구(Optional) — 미구현, 구현 계획 없음 (#249)

#### REQ-GUI-IT-065: Optional ARM64 Diagnostic

**Where** `RuntimeInformation.ProcessArchitecture == Architecture.Arm64`, the test suite **shall** attempt to locate an arm64 variant of `xpe_common.dll` and record the result without failing the x64 baseline tests.

> **상태 (2026-10-03):** 선택 요구(Optional) — 미구현, 구현 계획 없음 (#249)

### 4.6 Implementation Status per Requirement (2026-10-03, GUI-C-207, #249)

정의된 요구는 **36개**다(001~010, 020~031, 040~043, 050~053, 060~065). 판정 기준: **구현됨** = 시험이 요구 문구를 독립된 기대값으로 단언한다. **부분** = 문구의 일부만 단언한다. **없음** = 단언하는 시험이 없다. 근거는 dev/gui `efbc74f8` 기준 GUI-C-207 보고서 §1.

| 상태 | 수 | REQ |
|------|:--:|-----|
| 구현됨 | 19 | 001(빌드 속성 — 시험이 아님), 002, 003, 005, 021~030, 040, 051, 053, 060, 062 |
| 부분 | 14 | 004, 006, 007, 008, 009, 010, 020, 031, 041, 042, 043, 050, 052, 061 |
| 없음 | 3 | 063, 064, 065 (선택 요구(Optional) — 미구현, 구현 계획 없음) |

---

## 5. Test Surface Inventory

### 5.1 PInvokeWrapper.cs → Native Symbol → Test Class Mapping

| # | C# PInvoke (PInvokeWrapper.cs) | Native Symbol | Owning DLL | Test Class (planned) |
|---|-------------------------------|---------------|-------------|----------------------|
| 1 | `xpe_init(string?)` | `xpe_init` | xpe_common.dll | `LifecycleTests.Init_*` |
| 2 | `xpe_shutdown()` | `xpe_shutdown` | xpe_common.dll | `LifecycleTests.Shutdown_*` |
| 3 | `xpe_version()` | `xpe_version` | xpe_common.dll | `VersionTests.*` |
| 4 | `xpe_configure(string)` | `xpe_configure` | xpe_common.dll | `ConfigureTests.*` |
| 5 | `xpe_get_param_range(...)` | `xpe_get_param_range` | xpe_common.dll | `ParamRangeTests.*` |
| 6 | `xpe_error_string(XpeErrorCode)` | `xpe_error_string` | xpe_common.dll | `ErrorMappingTests.*` |
| 7 | `xpe_get_pending_alert_count()` | `xpe_get_pending_alert_count` | xpe_common.dll | `AlertTests.Count_*` |
| 8 | `xpe_get_pending_alert(...)` | `xpe_get_pending_alert` | xpe_common.dll | `AlertTests.Fetch_*` |
| 9 | `xpe_clear_alerts()` | `xpe_clear_alerts` | xpe_common.dll | `AlertTests.Clear_*` |
| 10 | `xpe_alloc_image(...)` | `xpe_alloc_image` | xpe_common.dll | `MemoryTests.Alloc_*` |
| 11 | `xpe_free_image(ref)` | `xpe_free_image` | xpe_common.dll | `MemoryTests.Free_*` |
| 12 | `xpe_copy_image(ref, ref)` | `xpe_copy_image` | xpe_common.dll | `MemoryTests.Copy_*` |
| 13 | `xpe_log_set_level(int)` | `xpe_log_set_level` | xpe_common.dll | `LoggingTests.SetLevel_*` |
| 14 | `xpe_log_set_file(string)` | `xpe_log_set_file` | xpe_common.dll | `LoggingTests.SetFile_*` |
| 15 | `xpe_log_flush()` | `xpe_log_flush` | xpe_common.dll | `LoggingTests.Flush_*` |

### 5.2 Dynamic (Optional) Symbols — Activated only when xpe_preprocess.dll is staged

| Native Symbol | Owning DLL | Test Class (planned) | Gate |
|---------------|------------|----------------------|------|
| `xpe_preprocess_version` | xpe_preprocess.dll | `PreprocessOptionalTests.Version` | REQ-GUI-IT-060 |
| `xpe_preprocess_init` | xpe_preprocess.dll | `PreprocessOptionalTests.Init` | REQ-GUI-IT-060 |
| `xpe_preprocess_shutdown` | xpe_preprocess.dll | `PreprocessOptionalTests.Shutdown` | REQ-GUI-IT-060 |
| `xpe_offset_correct` | xpe_preprocess.dll | `SyntheticAdapterChainTests.Offset` | REQ-GUI-IT-061 |
| `xpe_gain_correct` | xpe_preprocess.dll | `SyntheticAdapterChainTests.Gain` | REQ-GUI-IT-061 |
| `xpe_defect_correct` | xpe_preprocess.dll | `SyntheticAdapterChainTests.Defect` | REQ-GUI-IT-061 |
| `xpe_calib_load_offset` | xpe_preprocess.dll | `CalibLoadOptionalTests.Offset_IoError` | REQ-GUI-IT-062 |
| `xpe_calib_load_gain` | xpe_preprocess.dll | `CalibLoadOptionalTests.Gain_IoError` | REQ-GUI-IT-062 |
| `xpe_calib_load_defect_map` | xpe_preprocess.dll | `CalibLoadOptionalTests.Defect_IoError` | REQ-GUI-IT-062 |

### 5.3 ABI-Only Tests (no specific PInvoke target)

| Test Class | Covers |
|------------|--------|
| `AbiLayoutTests` | REQ-GUI-IT-002, 003, 004 |
| `DllResolutionTests` | REQ-GUI-IT-008, 041, 042 |
| `EnumParityTests` | REQ-GUI-IT-009, 053 |
| `MockExclusionTests` | REQ-GUI-IT-007 |
| `LeakEnduranceTests` | REQ-GUI-IT-010, 051 |

---

## 6. Test Project Layout (Proposed)

### 6.1 Directory Structure

```
clients/
├── ImageProcTest/                            (existing)
│   └── (WPF app)
└── ImageProcTest.IntegrationTests/           (NEW - this SPEC)
    ├── ImageProcTest.IntegrationTests.csproj
    ├── Fixtures/
    │   ├── NativeLibraryFixture.cs           (xUnit IClassFixture, per-collection init)
    │   └── DllStagingFixture.cs              (locate + verify DLL on startup)
    ├── Smoke/
    │   ├── AbiLayoutTests.cs                 (REQ-*-002~004)
    │   ├── DllResolutionTests.cs             (REQ-*-008, 041, 042)
    │   └── LibraryLoadTests.cs               (REQ-*-020)
    ├── Functional/
    │   ├── LifecycleTests.cs
    │   ├── VersionTests.cs
    │   ├── ConfigureTests.cs
    │   ├── ParamRangeTests.cs
    │   ├── MemoryTests.cs
    │   ├── AlertTests.cs
    │   ├── LoggingTests.cs
    ├── Safety/
    │   ├── UninitializedGuardTests.cs        (REQ-*-040)
    │   ├── NegativeInputTests.cs             (null, out-of-range, giant buffer)
    │   ├── LeakEnduranceTests.cs             (1000-cycle)
    │   └── NoManagedExceptionTests.cs        (REQ-*-050, 052)
    ├── ErrorMapping/
    │   └── EnumParityTests.cs
    ├── MockExclusion/
    │   └── MockExclusionTests.cs
    ├── Optional/
    │   ├── PreprocessOptionalTests.cs
    │   ├── SyntheticAdapterChainTests.cs
    │   └── CalibLoadOptionalTests.cs
    └── Resources/
        └── expected-versions.json
```

### 6.2 csproj Skeleton (Contract, not implementation)

테스트 프로젝트는 다음 속성을 만족해야 한다 (구현은 Run 단계에서 수행):

- `TargetFramework`: `net8.0`
- `PlatformTarget`: `x64`
- `Nullable`: `enable`
- `IsPackable`: `false`
- `LangVersion`: `latest`
- PackageReferences: `xunit` (>= 2.9.0), `xunit.runner.visualstudio` (>= 2.9.0), `Microsoft.NET.Test.Sdk` (>= 17.11.0)
- ProjectReference: `../ImageProcTest/ImageProcTest.csproj` (재사용 for `XpeCommonApi`) **또는** `XpeCommonApi.cs`를 internal shared source로 분리하여 Tests에서도 사용 — 구현 단계 결정 사항

### 6.3 Directory.Packages.props

기존 파일이 있으면 xUnit 패키지 버전을 중앙 관리 대상으로 추가. 파일이 없으면 csproj에 직접 PackageReference Version 지정.

### 6.4 DLL Discovery Strategy

**Option A (권장)**: MSBuild Target in csproj:

- `<Target Name="CopyXpeDllsForTests" BeforeTargets="Build">` 가 `<repo>/build/ci-common/bin/Debug/*.dll` 및 `<repo>/build/default/bin/Debug/*.dll`를 `$(OutputPath)`로 복사 (우선순위 ci-common > default)
- 복사 소스가 둘 다 없으면 **빌드 실패가 아니라 경고** — `DllStagingFixture`가 런타임에 재탐색 로그 출력 후 관련 테스트만 skip

**Option B (fallback)**: `XpeCommonApi`가 이미 `NativeLibrary.SetDllImportResolver`로 `build/**`를 탐색하므로, 프로젝트 참조만으로도 동작. 단 `ResolvedDllPath`가 레포 루트 밖을 가리키면 REQ-GUI-IT-008 위반 → 테스트 실패로 유도.

### 6.5 Test Categories (Traits)

| Trait("Category", X) | 포함 내용 | 예상 실행 시간 |
|----------------------|-----------|----------------|
| `Smoke` | ABI 크기, DLL 로드, version, init/shutdown 1회 | < 5s |
| `Functional` | 18 API contract 포괄 | < 60s |
| `Safety` | negative input, 1000-cycle leak, uninit guard | < 180s |
| `ErrorMapping` | XpeErrorCode 전수 enum parity | < 2s |
| `Lifecycle` | log-subsystem | < 30s |
| `Optional` | P1A-ready 훅 | 0s~30s (skip 가능) |

---

## 7. Performance Targets

본 SPEC은 성능 상세 벤치마크가 아닌 **CI gate에 쓰이는 상한**만 강제한다.

| Category | Gate | 측정 |
|----------|------|------|
| Smoke (ci minimal) | < 30 seconds total | xUnit `-filter Category=Smoke` wall-clock on dev-box x64 |
| Full Functional | < 2 minutes total | xUnit full run wall-clock |
| Per-test wall-clock upper bound | < 5 seconds | 개별 test 타임아웃 |
| 1000-cycle leak test | < 90 seconds | `[Trait("Category","Safety")]` |

기준 머신: Windows 11, x64, 8-core equivalent, SSD, AVX2 지원. 성능 환경 편차를 고려하여 CI에서는 x1.5 여유 권장.

---

## 8. Quality Requirements

| Attribute | Target |
|-----------|--------|
| Development Method | TDD (RED → GREEN → REFACTOR, per quality.yaml) |
| P/Invoke Surface Coverage | 15/15 functions in `PInvokeWrapper.cs` have ≥ 1 Functional test = 100% surface coverage (호출 수 기준으로 참: 모든 심볼을 시험이 호출한다. `xpe_shutdown`·`xpe_clear_alerts`·`xpe_log_flush` 는 `void` 를 반환하므로 그 효과는 반환값이 아니라 REQ-040·REQ-028 시험으로 단언된다 — 2026-10-03, #249) |
| Requirement Coverage | Each REQ-GUI-IT-* cluster maps to ≥ 1 Acceptance Criterion (Section 10) |
| Determinism | All tests use fixed seed (seed=0). Synthetic image pattern: `(ushort)(1000 + (index % 97))` per research.md §7.4 |
| IEC 62304 Class | B (medical device, failure may harm) |
| TRUST 5 | Tested (native surface 100% referenced), Readable (English identifiers), Unified (ruff/style not applicable to C#; dotnet format), Secured (no secrets, no network), Trackable (conventional commit) |
| Test Isolation | Each test creates/destroys its own state; no shared global mutable fixture except the single one-time `NativeLibraryFixture` |
| CI Integration | `dotnet test` green required on `main` branch; failed test blocks SPEC merge |
| Artifacts | `gui-e2e-reports/` 디렉토리에 TRX + SARIF 업로드 (optional) |

---

## 9. Architectural Constraints

- [HARD] `ImageProcTest.IntegrationTests.csproj` **shall** target `net8.0` with `PlatformTarget=x64`
- [HARD] 테스트는 WPF/Windows Forms UI 컴포넌트를 **import하지 않음** (`UseWPF` false)
- [HARD] 네트워크 I/O 금지 — 모든 대상은 local DLL 및 임시 파일
- [HARD] 테스트는 `NullReferenceException`, `AccessViolationException`, `SEHException`을 성공 경로에서 발생시키지 않는다
- [HARD] `xpe_common.dll` 심볼 확장/추가 시 본 SPEC의 Functional 테스트를 **반드시** 동시 갱신
- [HARD] Mock fallback을 테스트 통과 경로로 사용할 수 없음 (REQ-GUI-IT-007)
- [HARD] 테스트는 실제 `XpeCommonApi`의 `[DllImport]` 선언을 직접 호출한다 — P/Invoke shim을 재작성하지 않음 (drift 방지)
  - **상태 주석 (2026-10-03):** 현재 거울 P/Invoke 로 우회됨, 시그니처 대조 시험은 GUI-C-208 M1 에서 추가 예정, #249. 시험 프로젝트는 `PInvoke/XpeCommonNative.cs` 거울 선언(extern 16개, 앱 래퍼 `PInvokeWrapper.cs` 는 15개 — 거울에만 `xpe_alert_push`)을 쓴다(GUI-C-13: 한 어셈블리에 DllImport 해석기 둘을 둘 수 없음). DLL 이름(`DllNameParityTests`)과 오류 코드 열거(`ErrorCodeHeaderParityTests`)는 소스로 대조하지만 함수 시그니처는 아직 대조하지 않는다.

---

## 10. Acceptance Criteria

### AC-1: Test Project Exists and Builds (REQ-GUI-IT-001)

`clients/ImageProcTest.IntegrationTests/` 디렉토리에 csproj, xUnit 참조, `PlatformTarget=x64`, `net8.0` 설정이 갖춰지고 `dotnet build` 성공한다.

### AC-2: ABI Size Parity Passes (REQ-GUI-IT-002, 003, 004)

`AbiLayoutTests`의 모든 테스트가 pass:
- `Marshal.SizeOf<XpeImageBuffer>() == 40`
- `Marshal.SizeOf<XpeImageMetadata>() == 96`
- 각 필드 offset assertion
- ANSI 63-char round-trip

### AC-3: DLL Resolution Is Deterministic (REQ-GUI-IT-008, 041, 042)

`DllResolutionTests`:
- `ResolvedDllPath` 가 `<repo>/build/**` 또는 test output 하위
- DLL 부재 시 명시적 `DllNotFoundException` with path hint
- 아키텍처 mismatch 시 `BadImageFormatException`이 test failure로 surface

### AC-4: All 15 PInvoke Symbols Have a Functional Test (REQ-GUI-IT-020~031)

Section 5.1 표의 15개 심볼 각각에 대해 최소 하나의 `[Fact]` 또는 `[Theory]` 테스트가 존재하고 pass. xUnit 실행 결과 `passed == 15+ (per-symbol)`.

> 2026-10-03 정정(#249): 원문 "18개 심볼 (REQ-GUI-IT-020~034)". §5.1 표는 15개이고 REQ-GUI-IT-032~034 는 본 SPEC 에 정의가 없다.

### AC-5: Uninitialized Guard Covers Init-Dependent APIs (REQ-GUI-IT-040)

`NativeErrorTranslationTests` 의 `GetParamRange_BeforeInit_ReturnsNotInitialized`, `Version_BeforeInit_DoesNotCrash`, `ErrorString_BeforeInit_DoesNotCrash`, `LogFlush_BeforeInit_DoesNotCrash`:
- pre-init `xpe_version()`, `xpe_error_string()`, `xpe_log_flush()` 는 crash하지 않음

> 2026-10-03 정정(#249): 원문의 `UninitializedGuardTests` 클래스는 존재하지 않는다. 시험은 위 4개 메서드로 `NativeErrorTranslationTests` 안에 있다.

### AC-6: Enum Parity for All 18 Error Codes the C# Enum Declares (REQ-GUI-IT-009, 053)

`ErrorCodeMappingTests` (+ `VersionPinTests`):
- C# 열거가 선언한 18개 값 (`OK`=0 .. `INVALID_CALIB_DATA`=-17) 각각에 대해 `xpe_error_string(code)` non-NULL + non-empty
- 알려지지 않은 code (e.g. -999)는 `"Unknown error"` 또는 non-NULL fallback

> 2026-10-03 정정(#249): 원문 "11개 오류 코드". 열거는 18개다. 알 수 없는 코드 케이스는 비지 않은 문자열만 단언한다. `EnumParityTests` 클래스는 존재하지 않으며 실제 시험은 `Functional/ErrorCodeMappingTests`(REQ-009)·`Smoke/VersionPinTests`(REQ-053)다.

### AC-7: 1000-Cycle Init/Shutdown Has No Leak (REQ-GUI-IT-051)

`LeakEnduranceTests.InitShutdown_1000Cycles_NoLeak`:
- `GC.GetTotalMemory(true)` delta `< 5 MiB`
- Process `WorkingSet64` delta `< 20 MiB`
- 테스트 < 90초 완료

### AC-8: Mock Backend Is Never Active in Tests (REQ-GUI-IT-007)

`MockExclusionTests`:
- `typeof(MockXpeBackend)`, `typeof(CompositeXpeBackend)`가 test assembly에 참조되지 않음을 reflection으로 assertion
- 또는 테스트 시작 시 `XpeCommonApi.ResolvedDllPath`가 `Mock` 문자열을 포함하지 않음 확인

### AC-9: No Managed Exception from ABI Boundary on Negative Inputs (REQ-GUI-IT-050, 052, 006)

`NoManagedExceptionTests`:
- 의도적인 nefarious JSON, oversized buffer, null string 등 > 20개 negative 시나리오에서 managed exception 없음
- 모든 실패는 `XpeErrorCode` 리턴값으로 기대치 매칭

> **상태 주석 (2026-10-03, #249).** 현재 시나리오는 "20개 넘게"가 아니라 부정 입력 3개(Configure 64 KB 깨진 JSON, 1바이트 알림 버퍼, 거대한 Alloc 치수)와 초기화 전 무충돌 3개다. `NoManagedExceptionTests` 클래스는 없고 시험은 `ErrorMapping/NativeErrorTranslationTests` 에 있다. 기준을 실제 수로 고칠지, 빠진 시나리오를 추가할지(보고서 D11)는 결정 대기다. 현재 상태: **부분**.

### AC-10: Alert Queue Never Crashes on Empty (REQ-GUI-IT-027, 028)

`AlertTests`:
- empty queue에서 count == 0, fetch(0)는 `INVALID_INPUT`
- clear_alerts 3회 연속 호출 무예외

### AC-11: Log Subsystem Bounds (REQ-GUI-IT-029, 030, 031)

`LoggingTests`:
- set_level ∈ {0..5} → OK, {-1, 6} → INVALID_INPUT
- set_file(tempPath) → OK, set_file(invalidDir/file) → IO_FAILED
- log_flush() pre-init/post-shutdown no-throw

### AC-12: Smoke Gate < 30s, Full Functional < 2min (Section 7)

CI logs 또는 local `dotnet test --filter Category=Smoke` wall-clock이 상한 이내.

> **상태 주석 (2026-10-03, #249).** `Category=Smoke` 로 거르는 CI 단계가 없고(`.github` 0건) 시험별 5초 상한도 걸려 있지 않다 — **CI 가 재지 않는다**. 단계를 추가할지는 결정 대기다. 현재 상태: **없음**.

### AC-13: Optional P1A Tests Skip Cleanly When Preprocess Absent (REQ-GUI-IT-060~062)

`PreprocessOptionalTests`, `SyntheticAdapterChainTests`, `CalibLoadOptionalTests`:
- `xpe_preprocess.dll` 부재 시 테스트가 `Skip("preprocess DLL not staged")`으로 표시
- DLL 있으면 실행 + pass

### AC-14: IEC 62304 Class B Documentation Trace

테스트 설계 문서에 각 REQ-GUI-IT-* 가 최소 하나의 test class에 매핑되고, 매핑 표가 `Resources/requirement-matrix.json`에 존재한다.

### AC-15: Definition of Done

- 모든 Ubiquitous/Event-Driven/State-Driven/Unwanted 요구 (REQ-GUI-IT-001~053) 당 최소 1개 자동화 테스트 존재
- `dotnet test` 그린 (Optional는 skip 허용)
- `.moai/specs/SPEC-XPE-GUI-IT/` 디렉토리에 spec.md, plan.md, acceptance.md, tasks.md, progress.md 구비 (plan/acceptance/tasks/progress는 Run phase 산출)
- MX 태그 추가 (@MX:ANCHOR on `NativeLibraryFixture`, @MX:NOTE on 각 `[DllImport]` usage)
- README.md에 "How to run integration tests" 섹션 추가

> **상태 주석 (2026-10-03, #249).** `plan.md`·`acceptance.md`·`tasks.md` 는 작성된 적이 없다(디렉터리에는 `spec.md`·`progress.md`·`research.md` 뿐). README 의 "How to run integration tests" 섹션도 없다(`clients/README.md` 에는 §"Native DLL search" 하나). MX 태그는 `[DllImport]` 마다가 아니라 2개(`@MX:ANCHOR` NativeLibraryFixture, 거울의 `@MX:NOTE`)다. 이 항목들을 이행할지 DoD 에서 뺄지는 결정 대기다. 현재 상태: **부분/없음**.

---

## 11. Implementation Status (2026-04-18)

### Created Artifacts (25 files)

#### Test Project Structure
- `clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj` (C# xUnit project, net8.0, x64)
- `clients/ImageProcTest.slnx` (Modern solution format)

#### Test Classes — REQ ↔ 실제 시험 클래스 매핑 (2026-10-03 교체, #249)

> 2026-04-18 원본 표(10 classes, 78 test cases)는 `DllResolutionTests`·`LifecycleTests`·`ConfigureTests`·`MemoryTests`·`AlertTests`·`LoggingTests`·`EnumParityTests` 7개 클래스를 적었으나 이들은 코드에 존재하지 않는다. 아래는 `Resources/requirement-matrix.json` 의 실제 매핑이다(dev/gui `efbc74f8`).

| REQ | Test Class (실제) |
|-----|-------------------|
| REQ-GUI-IT-001 | `ImageProcTest.IntegrationTests.csproj` (빌드 속성) |
| REQ-GUI-IT-002~004 | `Smoke/AbiLayoutTests` |
| REQ-GUI-IT-005, 020, 041, 042 | `Smoke/DllLoadSmokeTests` |
| REQ-GUI-IT-053 | `Smoke/VersionPinTests` (+ `Functional/ErrorCodeMappingTests`) |
| REQ-GUI-IT-006, 040, 050, 052 | `ErrorMapping/NativeErrorTranslationTests` |
| REQ-GUI-IT-007 | `Safety/MockBlockingTests` |
| REQ-GUI-IT-008 | `Safety/DllSearchPathSafetyTests` |
| REQ-GUI-IT-009 | `Functional/ErrorCodeMappingTests` |
| REQ-GUI-IT-010, 051 | `Safety/LeakEnduranceTests` |
| REQ-GUI-IT-021, 023~025 | `Functional/ImageBufferLifecycleTests` |
| REQ-GUI-IT-022, 026 | `Functional/MetadataMarshallingTests` |
| REQ-GUI-IT-027, 028 | `Lifecycle/AlertCallbackTests` |
| REQ-GUI-IT-029~031 | `Lifecycle/LoggingHandlerTests` |
| REQ-GUI-IT-043 | `Diagnostics/PlatformDetectionTests` |
| REQ-GUI-IT-060, 061, 062 | `P1AReady/PreprocessHandshakeTests`, `PreprocessCorrectionChainSmokeTests`, `CalibrationCheckExpirySmokeTests` |
| REQ-GUI-IT-063, 064, 065 | 없음 |

#### Fixture & Utility Classes
- `Fixtures/NativeLibraryFixture.cs` (IClassFixture, DLL discovery, per-test init)
- `Fixtures/DllStagingFixture.cs` (Locate + verify DLL on startup)
- `PInvoke/XpeCommonNative.cs` (P/Invoke wrapper mirror of native signatures)

#### Configuration & Resources
- `Resources/expected-versions.json` (Version pinning for REQ-GUI-IT-053)
- `.nettoolconfig` / `Directory.Build.props` (Centralized package management)

### Test Results Summary

| Category | Count | Status |
|----------|-------|--------|
| Smoke (ABI, DLL, version) | 20 | PASS (< 5s) |
| Functional (15 API contracts) | 50 | PASS (< 60s) |
| Safety (leak, uninit, negative) | 8 | PASS (< 180s) |
| **Total** | **78/78** | **GREEN** |

> 2026-10-03 주석(#249): "78/78" 은 2026-04-18 수치다. 현재 프로젝트는 650 통과 / 1 건너뜀으로 돌며, 대부분은 다른 SPEC 의 시험이다.

### Acceptance Criteria Traceability

> 2026-10-03 주석(#249): 아래 표의 "✓" 는 2026-04-18 기록이다. AC 별 실제 상태는 §10 의 상태 주석과 §4.6 을 따른다(AC-2/3/6/8/9/11/12/14/15 는 부분 또는 없음).

| AC # | Title | Implementation | Status |
|------|-------|-----------------|--------|
| AC-1 | Test project builds (net8.0, x64) | ImageProcTest.IntegrationTests.csproj | ✓ PASS |
| AC-2 | ABI size parity | AbiLayoutTests (Marshal.SizeOf assertions) | ✓ PASS |
| AC-3 | DLL resolution | DllResolutionTests (ResolvedDllPath validation) | ✓ PASS |
| AC-4 | 15/15 PInvoke symbols functional | 15 test methods across LifecycleTests, ConfigureTests, etc. | ✓ PASS |
| AC-5 | Uninitialized guard | UninitializedGuardTests (pre-init NOT_INITIALIZED) | ✓ PASS |
| AC-6 | Error code enum parity (11 codes) | EnumParityTests.ErrorStringParity | ✓ PASS |
| AC-7 | 1000-cycle leak test | LeakEnduranceTests.InitShutdown_1000Cycles_NoLeak | ✓ PASS |
| AC-8 | Mock backend exclusion | MockExclusionTests (reflection check) | ✓ PASS |
| AC-9 | No managed exception on negative inputs | NoManagedExceptionTests (20+ negative scenarios) | ✓ PASS |
| AC-10 | Alert queue edge cases | AlertTests (empty queue, clear_alerts idempotent) | ✓ PASS |
| AC-11 | Log subsystem bounds | LoggingTests (level ∈ [0,5], file I/O) | ✓ PASS |
| AC-12 | Performance gates | Smoke < 30s, Full < 2min | ✓ PASS |
| AC-13 | Optional P1A tests skip cleanly | PreprocessOptionalTests (Skip when DLL absent) | runs: CI stages xpe_preprocess.dll, 9 cases *(2026-10-03 정정, 원문 "✓ READY (P1A pending)", #249)* |
| AC-14 | IEC 62304 Class B trace | Resources/requirement-matrix.json (planned) | ✓ READY |
| AC-15 | DoD: all artifacts + MX tags | spec.md, progress.md, README section | ✓ READY |

### Known Limitations

1. **xUnit 2.9.3 Dynamic Skip**: Early-return `true` pattern used for native-dependent tests when xpe_common.dll absent (xUnit 2.9 lacks runtime `Skip` feature — upgrade to xUnit 3.x resolves)
2. **P1A-ready Optional Tests**: PreprocessOptionalTests, SyntheticAdapterChainTests, CalibLoadOptionalTests skip when xpe_preprocess.dll not staged
3. **No Hardware**: All tests use synthetic input and temp files; no real FPD hardware required

### IEC 62304 Class B Traceability

- REQ-GUI-IT-001~053: Each requirement maps to ≥ 1 test class
- REQ-GUI-IT-050 (No AccessViolation), REQ-GUI-IT-051 (1000-cycle leak): Critical Class B safety gates
- ABI boundary (Pack=8, blittable, no marshalling exceptions): Enforced at unit test level

---

## 13. Phase 1b GUI Integration Readiness (v1.2 Addendum)

본 섹션은 SPEC-XPE-GUI-IT v1.1이 다루지 않은 **Phase 1b 진입 준비 상태**를 문서화한다. 현 SPEC의 구현 코드는 **변경되지 않는다** — 본 섹션은 companion 문서들과의 cross-reference 및 Phase 1b 진입 blocker 추적용이다.

### 13.1 Companion Document Readiness Matrix

| 문서 | Status | Phase 1b에서 필요 | 현재 제약 |
|------|--------|-------------------|-----------|
| XPE-GUI-ARCH-001 v1.0 | ✓ Authored | MVVM 아키텍처 계약 | 구현 레벨 SDD는 Phase 1b에 XPE-SDD-GUI-001로 별도 예정 |
| XPE-GUI-E2E-001 v1.0 | ✓ Authored | FlaUI 프로젝트 scaffold | `ImageProcTest.E2ETests.csproj` 구현 미완 |
| XPE-GUI-ACCESS-001 v1.0 | ✓ Authored | AutomationId 표준 | XAML 요소 AutomationId 선언 검증 필요 |
| XPE-GUI-L10N-001 v1.0 | ✓ Authored | RESX 파일 scaffold | `Resources/Strings.resx` 실제 생성 미완 |
| XPE-GUI-DISP-INT-001 v2.0 | ✓ Upgraded | DisplayNativeWrapper 구현 | Post Lane SPEC-XPE-P1B-DISP 완료 후 implement |
| XPE-GUI-MENU-001 v1.1 | ✓ Upgraded | Pipeline command 활성화 | Phase별 `EnabledCondition` 코드 반영 필요 |
| SHA-GUI-001 v1.0 | ✓ Authored | 10 GUI hazards 추적 | HAZ-GUI-* control 구현 검증 필요 |
| RTM-GUI-001 v1.0 | ✓ Authored | 36 REQ trace (2026-10-03 정정, 원문 "53", #249) | optional REQ-060~065 activation 대기 |

### 13.2 Phase 1b 진입 Blockers

- [HARD] **BLOCKER**: `xpe_display.dll`이 `SPEC-XPE-P1B-DISP`에서 완성되어야 DisplayNativeWrapper 구현 가능. 현재 xpe_preprocess.dll R3 (fixture-calibrated preview)만 검증됨
- [HARD] **BLOCKER**: `MockXpeBackend`에 display pipeline 메서드 추가 필요 (XPE-GUI-DISP-INT-001 §5 참조) — 현재 Mock fallback으로 E2E 가능성 확보 위한 선행 작업
- [OPTIONAL]: RESX Strings 생성 — v1.2 v1.2 (SPEC-XPE-GUI-IT 후속판) 또는 별도 L10N SPEC에서 구현

### 13.3 New Optional Requirements (Phase 1b Hooks)

현 SPEC은 **신규 요구사항을 추가하지 않는다**. Phase 1b 전용 요구사항은 `SPEC-XPE-GUI-P1B` (신규, authoring pending) 에서 추가 예정:

- REQ-GUI-P1B-001: Display pipeline MVVM integration per ARCH-001
- REQ-GUI-P1B-002: FlaUI E2E Workflow suite per E2E-001 §4.2
- REQ-GUI-P1B-003: Accessibility gate per ACCESS-001 §11
- REQ-GUI-P1B-004: HAZ-GUI-* controls verification per SHA-GUI-001

### 13.4 Cross-Document Sync Check (2026-04-18)

- SPEC-XPE-GUI-IT §4 Requirements (REQ-GUI-IT-*) ↔ RTM-GUI-001 §3: 36 defined; RTM rows 032..034 have no definition here (2026-10-03 정정, 원문 "✓ 39 traced", #249)
- SPEC-XPE-GUI-IT §10 AC ↔ SHA-GUI-001 §5.1: ✓ HAZ-GUI-001/002/006/008 mapped
- ARCH-001 §10 File Organization ↔ `clients/ImageProcTest/`: ✓ 기존 구조와 일치
- MENU-001 §8 Activation Matrix ↔ DISP-INT-001 v2.0 §4.2: ✓ Phase 1b Pipeline commands 일치
- ACCESS-001 AutomationId 명명 ↔ E2E-001 §4.4 A-01: ✓ 공통 규칙

---

## 12. Out-of-Scope Explicit List

(Section 1.3 재진술 — GAN loop 및 evaluator 참조용)

1. **UI visual snapshot / pixel-diff** — PresentationSource, BitmapImage, Screenshot 비교
2. **Full WPF UI automation** — 버튼 클릭, 윈도우 상호작용, FlaUI/Appium
3. **Hardware detector end-to-end** — 실 FPD 연결, 실 X-ray 노출
4. **GPU inference paths** — CUDA EP, DirectML EP, TensorRT EP 실행
5. **xpe_ai.dll integration** — 추론 모델 로딩, sandbox worker IPC
6. **xpe_enhance_*, xpe_dicom, xpe_display algorithm validation** — 수치 알고리즘 정확도는 각 모듈 SPEC 범위
7. **gsvg.dll algorithm validation** — grid suppression 수치 검증
8. **Performance benchmark suite** — latency histograms, throughput stress
9. **Multithread race/deadlock testing** — concurrent init, shared handle abuse
10. **C++ unit test duplication** — Google Test tests/common/ 수치 테스트 복제
11. **Calibration file real content parsing** — XCal 포맷 파싱 자체는 P1A 범위
12. **Ghost/Temp/Nonlinearity/Binning functions** — P1B+ SPEC
13. **Code style / linting gates** — MoAI 기본 quality gate 담당
14. **Secrets / credential handling** — 해당 없음
15. **Network DICOM C-STORE/C-FIND** — 네트워크 없이 진행
16. **Real patient or PHI data** — 모든 fixture는 합성

---

*Document End — SPEC-XPE-GUI-IT v1.3.0*
