# Requirements Traceability Matrix — GUI Layer

**Document ID**: RTM-GUI-001
**Version**: 1.1.0
**Date**: 2026-10-03
**Status**: Controlled Draft
**IEC 62304 Clause**: 5.1.1 (Software Development Planning), 5.2 (Software Requirements Analysis), 5.5 (Software Unit Implementation)
**Safety Classification**: Class B (mixed A/B for GUI)
**Parent Documents**: XPE-RTM-001 (system-wide RTM), SPEC-XPE-GUI-IT v1.3.0, SHA-GUI-001

---

## 1. Purpose

본 문서는 SPEC-XPE-GUI-IT v1.3.0의 **36 GUI requirements (REQ-GUI-IT-001~010, 020~031, 040~043, 050~053, 060~065)**에 대한 양방향 추적 매트릭스를 제공한다:

- SPEC requirement → SRS 매핑
- SPEC requirement → SDD (소프트웨어 상세 설계)
- SPEC requirement → 구현 코드 파일
- SPEC requirement → 검증 테스트 클래스
- SPEC requirement → Hazard 완화 (SHA-GUI-001)

IEC 62304 Class B 준수를 위한 trace 증거이다.

---

## 2. Requirements Coverage Summary

> **2026-10-03 교체 (GUI-C-207, #249).** 이전 표(Event-Driven 15, Total 39, Verified 37, 95%)는 SPEC 에 정의가 없는 REQ-032~034 를 셌고, "부분"까지 "검증됨"으로 셌다. 아래는 시험이 요구 문구를 실제로 단언하는지로 다시 센 것이다(구현됨 = 독립된 기대값으로 단언 · 부분 = 문구 일부만 단언 · 없음 = 단언하는 시험 없음). "부분"은 검증됨으로 보지 않는다.

| Category | Count | Implemented | Partial | None |
|----------|:-----:|-------------|---------|------|
| Ubiquitous (001~010) | 10 | 4 (001 은 빌드 속성, 002, 003, 005) | 6 (004, 006, 007, 008, 009, 010) | 0 |
| Event-Driven (020~031) | 12 | 10 (021~030) | 2 (020, 031) | 0 |
| State-Driven (040~043) | 4 | 1 (040) | 3 (041, 042, 043) | 0 |
| Unwanted Behavior (050~053) | 4 | 2 (051, 053) | 2 (050, 052) | 0 |
| Optional (060~065) | 6 | 2 (060, 062) | 1 (061) | 3 (063, 064, 065) |
| **Total** | **36** | **19** | **14** | **3** |

*Note*: 060~062 는 CI 가 xpe_preprocess.dll 을 올려 실제로 돈다(9건). 063~065 는 선택 요구(Optional) — 미구현, 구현 계획 없음 (#249).

---

## 3. Full Traceability Matrix

> **2026-10-03 정정 (#249).** Test Class 열의 이전 이름은 `AbiLayoutTests`·`LeakEnduranceTests` 두 클래스 외에는 코드에 존재하지 않았고, 메서드 이름은 한 행도 실재하지 않았다. 아래 Test Class 열은 `Resources/requirement-matrix.json` 과 시험 소스의 실제 이름이다(dev/gui `efbc74f8`, GUI-C-207 `verified_claims_out.txt` 로 grep 대조). 판정(구현됨/부분/없음)은 §2 와 SPEC-XPE-GUI-IT §4.6 을 따른다.

### 3.1 Ubiquitous Requirements

| REQ ID | Title | SRS Section | SDD Unit | Code File | Test Class | Hazard |
|--------|-------|-------------|----------|-----------|------------|--------|
| REQ-GUI-IT-001 | xUnit Framework | SRS §4.1 | SDD §2.1 Project Setup | `ImageProcTest.IntegrationTests.csproj` | (build gate) | — |
| REQ-GUI-IT-002 | Pack=8 ABI Size Parity | SRS §4.2 | SDD §3.1 Interop Layer | `PInvoke/XpeCommonNative.cs` | `AbiLayoutTests.XpeImageBuffer_MarshalSize_Is40Bytes` / `XpeImageMetadata_MarshalSize_Is96Bytes` | HAZ-GUI-002 |
| REQ-GUI-IT-003 | Blittable Field Parity | SRS §4.2 | SDD §3.1 | `PInvoke/XpeCommonNative.cs` | `AbiLayoutTests.XpeImageBuffer_FieldOffsets_MatchNativeLayout` | HAZ-GUI-002 |
| REQ-GUI-IT-004 | ANSI BodyPart Fixed Buffer | SRS §4.2 | SDD §3.1 | `PInvoke/XpeCommonNative.cs` | `AbiLayoutTests.XpeImageMetadata_BodyPart_63CharAscii_RoundTrips` | HAZ-GUI-002 |
| REQ-GUI-IT-005 | IntPtr Lifetime Contract | SRS §4.3 | SDD §3.2 | `XpeCommonApi.cs` | `DllLoadSmokeTests.XpeVersion_CalledTwice_ReturnsSamePointer` | HAZ-GUI-001 |
| REQ-GUI-IT-006 | No Managed Exception on Error | SRS §4.4 | SDD §3.2 Exception Boundary | `XpeCommonApi.cs`, test fixtures | `NativeErrorTranslationTests.*` | HAZ-GUI-001 |
| REQ-GUI-IT-007 | Mock Backend Exclusion | SRS §4.5 | SDD §2.3 Test Isolation | test project config | `MockBlockingTests.*` | — |
| REQ-GUI-IT-008 | Resolved DLL Path in Build Tree | SRS §4.6 | SDD §3.3 DLL Resolution | `DllStagingFixture.cs` | `DllSearchPathSafetyTests.*` | HAZ-GUI-006 |
| REQ-GUI-IT-009 | Error Code Enum Parity | SRS §4.7 | SDD §3.4 Enum Mapping | `PInvoke/XpeCommonNative.cs` | `ErrorCodeMappingTests.*` | HAZ-GUI-002 |
| REQ-GUI-IT-010 | No Handle Leak After Test Run | SRS §4.8 | SDD §3.5 Resource Management | test infrastructure | `LeakEnduranceTests.AfterTests_NoOutstandingPinnedHandles` (핸들을 세지 않음 — 부분, #249) | HAZ-GUI-008 |

### 3.2 Event-Driven Requirements

| REQ ID | Title | SRS Section | SDD Unit | Code File | Test Class | Hazard |
|--------|-------|-------------|----------|-----------|------------|--------|
| REQ-GUI-IT-020 | Library Load Success | SRS §5.1 | SDD §3.3 | `NativeLibraryFixture.cs` | `DllLoadSmokeTests.XpeVersion_WhenDllLoaded_ReturnsSemverString` | HAZ-GUI-006 |
| REQ-GUI-IT-021 | Init Success Path | SRS §5.2 | SDD §4.1 Lifecycle | `XpeCommonApi.Init` | `ImageBufferLifecycleTests.Init_Success_AlertCountIsNonNegative` | — |
| REQ-GUI-IT-022 | Configure JSON Roundtrip | SRS §5.3 | SDD §4.2 Configure | `XpeCommonApi.Configure` | `MetadataMarshallingTests.Configure_*` | HAZ-GUI-007 |
| REQ-GUI-IT-023 | Alloc/Free Roundtrip | SRS §5.4 | SDD §4.3 Memory | `XpeCommonApi.AllocImage/FreeImage` | `ImageBufferLifecycleTests.AllocImage_*` / `FreeImage_*` | HAZ-GUI-008 |
| REQ-GUI-IT-024 | Alloc with Invalid Format | SRS §5.4 | SDD §4.3 | `XpeCommonApi.AllocImage` | `ImageBufferLifecycleTests.AllocImage_*` | HAZ-GUI-007 |
| REQ-GUI-IT-025 | Copy Image | SRS §5.4 | SDD §4.3 | `XpeCommonApi.CopyImage` | `ImageBufferLifecycleTests.CopyImage_*` | HAZ-GUI-002 |
| REQ-GUI-IT-026 | Param Range Query | SRS §5.5 | SDD §4.4 Parameters | `XpeCommonApi.GetParamRange` | `MetadataMarshallingTests.GetParamRange_*` | HAZ-GUI-007 |
| REQ-GUI-IT-027 | Alert Queue Contract | SRS §5.6 | SDD §4.5 Alerts | `XpeCommonApi.GetPendingAlert*` | `AlertCallbackTests.*` | HAZ-GUI-008 |
| REQ-GUI-IT-028 | Clear Alerts Idempotent | SRS §5.6 | SDD §4.5 | `XpeCommonApi.ClearAlerts` | `AlertCallbackTests.*` | HAZ-GUI-008 |
| REQ-GUI-IT-029 | Log Level Bounds | SRS §5.7 | SDD §4.6 Logging | `XpeCommonApi.LogSetLevel` | `LoggingHandlerTests.*` | HAZ-GUI-007 |
| REQ-GUI-IT-030 | Log Redirect to Temp File | SRS §5.7 | SDD §4.6 | `XpeCommonApi.LogSetFile` | `LoggingHandlerTests.*` | HAZ-GUI-007 |
| REQ-GUI-IT-031 | Log Flush No-Throw | SRS §5.7 | SDD §4.6 | `XpeCommonApi.LogFlush` | `LoggingHandlerTests.*` | HAZ-GUI-001 |
| REQ-GUI-IT-032 | Detector-firmware callback — Configure Default *(out of XPE scope; ABI smoke only)* | SRS §5.8 | SDD §4.7 Detector-firmware boundary | existing native symbol *(retained from Phase 0 scaffolding)* | existing test class *(see SPEC-XPE-GUI-IT §5.1 rows 16–18)* | — |
| REQ-GUI-IT-033 | Detector-firmware callback — Configure Invalid JSON *(out of XPE scope)* | SRS §5.8 | SDD §4.7 | existing native symbol | existing test class | HAZ-GUI-007 |
| REQ-GUI-IT-034 | Detector-firmware callback — Poll Empty Queue *(out of XPE scope)* | SRS §5.8 | SDD §4.7 | existing native symbol | existing test class | HAZ-GUI-008 |

> **정의 상태 주석 (2026-10-03, #249):** REQ-GUI-IT-032·033·034 는 SPEC-XPE-GUI-IT 에 **정의가 없다**(SPEC 의 Event-Driven 은 020~031). 이 세 행은 SPEC 요구로 세지 않으며(§2 총계 36 에서 제외), 행 자체는 이력 보존을 위해 남긴다. "SPEC-XPE-GUI-IT §5.1 rows 16–18" 도 존재하지 않는다(§5.1 표는 15행). 별도 비-SPEC 표로 옮길지는 결정 대기다.

> **Scope Note (2026-04-18):** Per project commit `6b33a35 — docs: AED 용어 혼용 수정 — detector 고유 기능과 SW 인프라 분리`, detector hardware features (exposure-end signals, detector trigger thresholds, detector state machine) belong to **detector firmware**, not to the XPE image-processing engine. XPE scope = preprocess + postprocess + display + DICOM + AI. The three rows above exist solely because Phase 0 foundation scaffolding placed ABI-boundary smoke tests against native symbols that happen to live in `xpe_common.dll`; GUI-layer specifications, hazards, menus, and user-facing docs created in this RTM and its companions do **not** define detector behavior, detector state machines, or detector status indicators. "Exposure Index" (IEC 62494-1, computed post-processing output) remains in XPE scope and is unaffected by this boundary.

### 3.3 State-Driven Requirements

| REQ ID | Title | SRS Section | SDD Unit | Code File | Test Class | Hazard |
|--------|-------|-------------|----------|-----------|------------|--------|
| REQ-GUI-IT-040 | Uninitialized Guard | SRS §6.1 | SDD §4.1 Lifecycle States | `XpeCommonApi.*` | `NativeErrorTranslationTests.GetParamRange_BeforeInit_ReturnsNotInitialized` | HAZ-GUI-001 |
| REQ-GUI-IT-041 | Missing DLL Fails Deterministically | SRS §6.2 | SDD §3.3 DLL Resolution | `DllStagingFixture.cs` | `DllLoadSmokeTests.*` | HAZ-GUI-005, HAZ-GUI-006 |
| REQ-GUI-IT-042 | Architecture Mismatch Detection | SRS §6.3 | SDD §3.3 | `DllStagingFixture.cs` | `DllLoadSmokeTests.*` (`BadImageFormatException` 을 일으키는 시험 없음 — 부분, #249) | HAZ-GUI-006 |
| REQ-GUI-IT-043 | Platform Mismatch Diagnostic | SRS §6.4 | SDD §3.3 | (platform probe) | `PlatformDetectionTests.*` | — |

### 3.4 Unwanted Behavior Requirements

| REQ ID | Title | SRS Section | SDD Unit | Code File | Test Class | Hazard |
|--------|-------|-------------|----------|-----------|------------|--------|
| REQ-GUI-IT-050 | No AccessViolation Across Boundary | SRS §7.1 | SDD §3.2 Exception Boundary | `XpeCommonApi.cs` (exception-safe) | `NativeErrorTranslationTests.*` (부정 입력 3 + 초기화 전 3, 원문 "20+ scenarios" — #249) | **HAZ-GUI-001** |
| REQ-GUI-IT-051 | No Memory Leak After 1000 Cycles | SRS §7.2 | SDD §4.1 Lifecycle | `XpeCommonApi.Init/Shutdown` | `LeakEnduranceTests.InitShutdown_1000Cycles_NoLeak` | HAZ-GUI-008 |
| REQ-GUI-IT-052 | No Marshalling Exception Without Translation | SRS §7.3 | SDD §3.2 Marshaling | `XpeCommonApi.cs` | `NativeErrorTranslationTests.*` | HAZ-GUI-002 |
| REQ-GUI-IT-053 | No Silent Version-Skew | SRS §7.4 | SDD §3.4 Version Pinning | `Resources/expected-versions.json` | `VersionPinTests.XpeVersion_MajorMatchesPinnedVersion` | **HAZ-GUI-006** |

### 3.5 Optional Requirements (Conditional)

| REQ ID | Title | SRS Section | SDD Unit | Code File | Test Class | Activation |
|--------|-------|-------------|----------|-----------|------------|------------|
| REQ-GUI-IT-060 | Optional xpe_preprocess Lifecycle | SRS §8.1 | SDD §5.1 (P1A) | `XpePreprocessNative.cs` (planned) | `PreprocessHandshakeTests.*` | CI 에서 실행 |
| REQ-GUI-IT-061 | Optional Synthetic Adapter Chain | SRS §8.2 | SDD §5.2 (P1A) | `XpePreprocessSyntheticOracle.cs` | `PreprocessCorrectionChainSmokeTests.*` | CI 에서 실행 |
| REQ-GUI-IT-062 | Optional Calibration Loader Contract | SRS §8.3 | SDD §5.3 (P1A) | `XpePreprocessNative.cs` | `CalibrationCheckExpirySmokeTests.*` | CI 에서 실행 |
| REQ-GUI-IT-063 | Optional ETW/Diagnostic Run | SRS §8.4 | SDD §6.1 Diagnostics | (env-gated) | Not implemented — 선택 요구(Optional), 미구현, 구현 계획 없음 (#249) | ENV var |
| REQ-GUI-IT-064 | Optional .NET 9 Target Verification | SRS §8.5 | SDD §2.1 | csproj multi-target | Not implemented — 선택 요구(Optional), 미구현, 구현 계획 없음 (#249) | .NET 9 SDK present |
| REQ-GUI-IT-065 | Optional ARM64 Diagnostic | SRS §8.6 | SDD §3.3 | — | Not implemented — 선택 요구(Optional), 미구현, 구현 계획 없음 (#249) | ARM64 host |

> 2026-10-03 정정(#249): 이전 기술 "Optional tests (060-063): xpe_preprocess.dll 미스테이징으로 skip" 은 낡았다. 060~062 는 CI 가 xpe_preprocess.dll 을 올려 실제로 돈다(9건). 063 은 코드가 없다.

---

## 4. Extended GUI Requirements (Non-SPEC Documents)

SPEC-XPE-GUI-IT 범위 외 GUI 요구사항 추적:

### 4.1 Architecture (XPE-GUI-ARCH-001)

| REQ | Source | Implementation | Verification |
|-----|--------|----------------|--------------|
| MVVM 4-Tier 분리 | ARCH-001 §2 | `ViewModels/`, `Services/`, `Models/` | Code review |
| CommunityToolkit.Mvvm 사용 | ARCH-001 §3.1 | ObservableObject base classes | Build output |
| Async P/Invoke 패턴 | ARCH-001 §4 | `[RelayCommand] async Task` | ~~XPE-GUI-E2E-001 W-04~~ 검증 없음 — W-04 는 코드에 없고, 네이티브 호출 중 UI 응답성을 단언하는 E2E 도 없다 (#249) |
| Composition Root DI | ARCH-001 §6.1 | `App.xaml.cs OnStartup` | Code review |

### 4.2 Accessibility (XPE-GUI-ACCESS-001)

| REQ | Source | Implementation | Verification |
|-----|--------|----------------|--------------|
| AutomationProperties 필수 | ACCESS-001 §3.2 | All XAML interactive elements | 검증 없음 — 현재 `A01` 은 About 시험이며, AutomationId 없는 대화형 XAML 요소를 세는 시험은 없다 (#249) |
| WCAG 4.5:1 대비 | ACCESS-001 §4 | Themes/Colors.xaml | Manual + pixel analysis |
| 24x24 target size | ACCESS-001 §6 | Button styles MinWidth/Height | Manual |
| Keyboard shortcut matrix | ACCESS-001 §5.2 | KeyBindings in XAML | E2E scenarios |

### 4.3 E2E Testing (XPE-GUI-E2E-001)

| REQ | Source | Implementation | Verification |
|-----|--------|----------------|--------------|
| FlaUI framework 채택 | E2E-001 §2 | `ImageProcTest.E2ETests.csproj` | Build gate |
| Mock-mode CI 실행 | E2E-001 §5 | ENV `XPE_E2E_BACKEND` (원문 `MOAI_XPE_BACKEND_MODE=Mock` 은 문서에만 있는 이름 — #249) | CI script (`gui-automation`) |
| Smoke suite < 30s | E2E-001 §4.1 | xUnit filter `Category=Smoke` | 검증 없음 — Category 로 거르는 CI 단계가 없다. 프로젝트 전체가 `gui-automation`·`gui-e2e-native` 에서 필터 없이 돈다 (#249) |

### 4.4 Display Integration (XPE-GUI-DISP-INT-001)

| REQ | Source | Implementation | Verification |
|-----|--------|----------------|--------------|
| RealXpeBackend DisplayPipeline | DISP-INT-001 §3 Gap 1 | `RealXpeBackend.ApplyDisplayPipeline` | ~~E2E W-06~~ W-06 은 존재하지 않음(HAZ-GUI-004 참조). 실제 시나리오: E2E W01b, W02, W20~W28 (#249) |
| Display version in Runtime | DISP-INT-001 §3 Gap 4 | `BackendRuntimeInfo.DisplayVersion` | E2E S-05 |
| VOI Preset UI | DISP-INT-001 §4 | `DisplaySettingsPanel.xaml` | E2E W-07 |
| GSDF toggle | DISP-INT-001 §4 | AppSettings.GsdfEnabled | ~~E2E W-06~~ W-06 은 존재하지 않음. 실제 시나리오: E2E W01b, W02, W20~W28 (#249) |

### 4.5 Menu & Command (XPE-GUI-MENU-001)

| REQ | Source | Implementation | Verification |
|-----|--------|----------------|--------------|
| 6 메뉴 그룹 | MENU-001 §3 | MainWindow.xaml Menu | E2E S-02 |
| Pipeline 메뉴 활성화 타이밍 | MENU-001 §5 | Command CanExecute 로직 | E2E `AiMenuAvailabilityScenarios`, `DeterministicBaselineScenarios`, `ProcessingChainScenarios` (원문 "Code review"; XPE-GUI-MENU-001 정정안은 GUI-C-203 — #249) |
| 오프라인 Help | MENU-001 §4.6 | HelpBundleService | E2E S-04 |

### 4.6 Localization (XPE-GUI-L10N-001)

| REQ | Source | Implementation | Verification |
|-----|--------|----------------|--------------|
| ko-KR primary, en-US fallback | L10N-001 §3 | Strings.resx + Strings.ko-KR.resx — **미구현: `.resx` 파일이 하나도 없다 (#249)** | Manual |
| AutomationId locale-independent | L10N-001 §12 | AutomationProperties 영문 상수 | 검증 없음 — 현재 `A01` 은 About 시험이다 (#249) |
| ISO 8601 파일명 | L10N-001 §7.1 | `yyyyMMdd_HHmmss` | Code review |

---

## 5. Hazard-to-Requirement Coverage

SHA-GUI-001 각 hazard에 대한 완화 요구사항 추적:

| Hazard | Mitigation REQs | Verification |
|--------|-----------------|--------------|
| HAZ-GUI-001 (Native exception propagation) | REQ-GUI-IT-005, 006, 031, 040, 050, 052 | `NativeErrorTranslationTests`, `DllLoadSmokeTests`, `LoggingHandlerTests` (원문 `NoManagedExceptionTests`·`UninitializedGuardTests` 는 존재하지 않음 — #249) |
| HAZ-GUI-002 (Struct mismatch) | REQ-GUI-IT-002, 003, 004, 009, 025 | `AbiLayoutTests`, `ErrorCodeMappingTests`, `ImageBufferLifecycleTests` (원문 `EnumParityTests` 는 존재하지 않음 — #249) |
| HAZ-GUI-003 (UI freeze) | ARCH-001 §4.2 async rule | **검증 없음** — 원문 "E2E W-04" 는 코드에 없다 (#249) |
| HAZ-GUI-004 (Stale preview) | ARCH-001 §4.3, DISP-INT-001 §4.3 | **E2E W-20~W-26** (메인·분리 뷰어, Mock+Native, CI `3537a13`) — 이전 표기 "W-06/W-07" 의 W-06 은 존재하지 않았음 (#171); 이 통제들을 증명하는 결함 주입 장치는 출하 빌드에 없음 — CI `gui-shipped-build` (GUI-C-193) |
| HAZ-GUI-005 (Misleading diagnostics) | ACCESS-001 §7, persistent warning | **E2E E-01a~d** (Native + 빈 DLL 폴더, 대조군 포함, CI `d0737d8`) — 한때 통제와 E-01 이 코드에 없었음 (#175) |
| HAZ-GUI-006 (Version skew) | REQ-GUI-IT-008, 041, 042, 053 | `VersionPinTests.XpeVersion_MajorMatchesPinnedVersion`, `DllSearchPathSafetyTests`, `DllLoadSmokeTests` (원문 `EnumParityTests.VersionPin`·`DllResolutionTests` 는 존재하지 않음 — #249) |
| HAZ-GUI-007 (Use error) | REQ-GUI-IT-022, 024, 026, 029, IEC 62366 formative (REQ-033 은 SPEC 에 정의가 없어 제외 — #249) | `NativeErrorTranslationTests`, `MetadataMarshallingTests`, formative evaluation |
| HAZ-GUI-008 (Alert queue overflow) | REQ-GUI-IT-010, 027, 028, 051 (REQ-034 는 SPEC 에 정의가 없어 제외 — #249) | `AlertCallbackTests`, `LeakEnduranceTests` |
| HAZ-GUI-009 (Accessibility failure) | ACCESS-001 §3, §5 | **검증 없음** — 접근성 시험 묶음이 없다. 현재 `A01`~`A03` 은 About·자동화 보고서 시험이다 (#249) |
| HAZ-GUI-010 (Localization failure) | L10N-001 §11 CI gate | **검증 없음** — pseudo-localization CI 단계가 없고 `.resx` 도 없다 (#249) |

---

## 6. IEC 62304 Class B Compliance Table

| Clause | Requirement | Evidence | Status |
|--------|-------------|----------|--------|
| 5.1.1 Software development planning | SDP covers GUI layer | XPE-SDP-001, ARCH-001 | ✓ |
| 5.2.2 Software requirements content | Functional, performance, interface defined | SPEC-XPE-GUI-IT v1.3.0 | ✓ |
| 5.2.6 Requirements verified | 33 of 36 have tests; 14 only in part; REQ-063..065 have none | This RTM §2, §3 | Partial (2026-10-03 정정, 원문 "All REQ-GUI-IT-* have tests ✓" — #249) |
| 5.3 Software architectural design | Architecture documented | ARCH-001, SAD-001 | ✓ |
| 5.4 Software detailed design | Unit-level design | SDD-001, SDD-002 (system); GUI-specific in ARCH-001 §10 | Partial (GUI SDD addendum planned) |
| 5.5 Software unit implementation | Code exists per design | `clients/ImageProcTest*/` | ✓ |
| 5.6 Software integration | Integration plan | XPE-ITP-001, SPEC-XPE-GUI-IT | ✓ |
| 5.7 Software system testing | Test execution | IntegrationTests 650 passed / 1 skipped (2026-10-03); E2E suites run in CI (`gui-automation`, `gui-e2e-native`) (원문 "78/78 xUnit pass (2026-04-18), E2E planned" — #249) | ✓ |
| 7.1 Hazard analysis | Hazards identified | XPE-SHA-001, SHA-GUI-001 | ✓ |
| 7.3 Risk control measures | Controls implemented and verified | This RTM §5 | ✓ |

---

## 7. Gaps and Open Items

### 7.1 Identified Gaps

1. **GUI SDD Addendum**: SDD-001은 system-wide. GUI-specific 유닛 설계는 ARCH-001 §10에 산재. Phase 1b 시 `XPE-SDD-GUI-001` 신설 권장.
2. **Optional Tests (060-063)**: ~~현재 xpe_preprocess.dll 미스테이징으로 해당 테스트 skip 상태. P1A 완료 시 활성화 필요.~~ 2026-10-03 정정(#249): 060~062 는 CI 가 xpe_preprocess.dll 을 올려 실제로 돈다(9건). 063 은 코드가 없다(선택 요구 — 미구현, 구현 계획 없음).
3. **E2E Suite 미구현**: XPE-GUI-E2E-001 정의는 완료, 실제 `ImageProcTest.E2ETests` 프로젝트 및 FlaUI 테스트 구현은 Phase 1a/1b에서 수행 예정.
4. **Summative Usability Evaluation (IEC 62366)**: Production GUI 승격 시 필수 — test GUI 단계에서는 formative만 수행.

### 7.2 Pending Items

- [ ] GUI SDD 상세 설계 문서 작성 (Phase 1b)
- [ ] FlaUI E2E 프로젝트 구현 (Phase 1a)
- [ ] xpe_preprocess.dll staging 후 Optional 테스트 활성화
- [ ] xpe_display.dll P/Invoke 구현 (Phase 1b, XPE-GUI-DISP-INT-001 §3 gaps)
- [ ] RESX 리소스 파일 신설 + 번역 (Phase 1b 이후)
- [ ] Accessibility 수동 감사 (Phase 2 진입 전)

---

## 8. Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.1.0 | 2026-10-03 | lead (GUI-C-207, #249) | 실태 대조 정정: 정의된 REQ 수 53/39 → 36, §2 표를 구현됨 19 · 부분 14 · 없음 3 으로 교체("부분"을 검증됨으로 세지 않음), §3 Test Class 열을 실재하는 이름으로 교체, REQ-032~034 는 SPEC 정의 없음으로 표기(행은 보존), 063~065 는 선택 요구 미구현 표기. §4·§5 에서 코드·CI 에 없는 근거(W-04, W-06, 접근성 묶음, pseudo-localization, `MOAI_XPE_BACKEND_MODE`, `.resx`)에 기댄 ✓ 를 실태로 바꿈. §6 5.2.6·5.7 정정. 근거: `xpe-gui` `.moai/reports/lane-gui/GUI-C-207/report.md`·`rtm_correction_draft.txt` (dev/gui `efbc74f8`) |
| 1.0.0 | 2026-04-18 | manager-spec (GUI Lane) | Initial creation — 39 REQ-GUI-IT-* traced, 10 hazards mapped, IEC 62304 Class B compliance table |

---

## 9. Sign-off

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Software Lead | (TBD) | YYYY-MM-DD | ___ |
| QA Lead | (TBD) | YYYY-MM-DD | ___ |
| Risk Management | (TBD) | YYYY-MM-DD | ___ |

---

## 10. References

- SPEC-XPE-GUI-IT v1.3.0 (primary requirements source)
- SHA-GUI-001 v1.0.0 (hazard source)
- XPE-GUI-ARCH-001 v1.0.0 (architecture)
- XPE-GUI-ACCESS-001 v1.0.0 (accessibility)
- XPE-GUI-E2E-001 v1.0.0 (E2E testing)
- XPE-GUI-L10N-001 v1.0.0 (localization)
- XPE-GUI-DISP-INT-001 v2.0.0 (display integration, upgrade pending)
- XPE-GUI-MENU-001 v1.1.0 (menu strategy, upgrade pending)
- XPE-SHA-001 v2.0 (parent system SHA)
- XPE-RTM-001 (parent system RTM)

---

*Document End — RTM-GUI-001 v1.1.0*
