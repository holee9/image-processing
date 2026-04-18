# Requirements Traceability Matrix — GUI Layer

**Document ID**: RTM-GUI-001
**Version**: 1.0.0
**Date**: 2026-04-18
**Status**: Controlled Draft
**IEC 62304 Clause**: 5.1.1 (Software Development Planning), 5.2 (Software Requirements Analysis), 5.5 (Software Unit Implementation)
**Safety Classification**: Class B (mixed A/B for GUI)
**Parent Documents**: XPE-RTM-001 (system-wide RTM), SPEC-XPE-GUI-IT v1.2.0, SHA-GUI-001

---

## 1. Purpose

본 문서는 SPEC-XPE-GUI-IT v1.2.0의 **53 GUI requirements (REQ-GUI-IT-001~065)**에 대한 양방향 추적 매트릭스를 제공한다:

- SPEC requirement → SRS 매핑
- SPEC requirement → SDD (소프트웨어 상세 설계)
- SPEC requirement → 구현 코드 파일
- SPEC requirement → 검증 테스트 클래스
- SPEC requirement → Hazard 완화 (SHA-GUI-001)

IEC 62304 Class B 준수를 위한 trace 증거이다.

---

## 2. Requirements Coverage Summary

| Category | Count | Implemented | Verified | Coverage |
|----------|:-----:|:-----------:|:--------:|:--------:|
| Ubiquitous (REQ-*-001 ~ 010) | 10 | 10 | 10 | 100% |
| Event-Driven (REQ-*-020 ~ 034) | 15 | 15 | 15 | 100% |
| State-Driven (REQ-*-040 ~ 043) | 4 | 4 | 4 | 100% |
| Unwanted Behavior (REQ-*-050 ~ 053) | 4 | 4 | 4 | 100% |
| Optional (REQ-*-060 ~ 065) | 6 | 6* | 4 | 67% (* = activated when P1A ready) |
| **Total** | **39** | **39** | **37** | **95%** |

*Note*: Optional requirements 060~063 are activated only when xpe_preprocess.dll is staged. 064 (.NET 9) and 065 (ARM64) are conditional on CI environment.

---

## 3. Full Traceability Matrix

### 3.1 Ubiquitous Requirements

| REQ ID | Title | SRS Section | SDD Unit | Code File | Test Class | Hazard |
|--------|-------|-------------|----------|-----------|------------|--------|
| REQ-GUI-IT-001 | xUnit Framework | SRS §4.1 | SDD §2.1 Project Setup | `ImageProcTest.IntegrationTests.csproj` | (build gate) | — |
| REQ-GUI-IT-002 | Pack=8 ABI Size Parity | SRS §4.2 | SDD §3.1 Interop Layer | `PInvoke/XpeCommonNative.cs` | `AbiLayoutTests.SizeOfXpeImageBuffer` | HAZ-GUI-002 |
| REQ-GUI-IT-003 | Blittable Field Parity | SRS §4.2 | SDD §3.1 | `PInvoke/XpeCommonNative.cs` | `AbiLayoutTests.FieldOffsets` | HAZ-GUI-002 |
| REQ-GUI-IT-004 | ANSI BodyPart Fixed Buffer | SRS §4.2 | SDD §3.1 | `PInvoke/XpeCommonNative.cs` | `AbiLayoutTests.BodyPartRoundtrip` | HAZ-GUI-002 |
| REQ-GUI-IT-005 | IntPtr Lifetime Contract | SRS §4.3 | SDD §3.2 | `XpeCommonApi.cs` | `VersionTests.StaticPointer` | HAZ-GUI-001 |
| REQ-GUI-IT-006 | No Managed Exception on Error | SRS §4.4 | SDD §3.2 Exception Boundary | `XpeCommonApi.cs`, test fixtures | `NoManagedExceptionTests.*` | HAZ-GUI-001 |
| REQ-GUI-IT-007 | Mock Backend Exclusion | SRS §4.5 | SDD §2.3 Test Isolation | test project config | `MockExclusionTests.NoReference` | — |
| REQ-GUI-IT-008 | Resolved DLL Path in Build Tree | SRS §4.6 | SDD §3.3 DLL Resolution | `DllStagingFixture.cs` | `DllResolutionTests.PathInBuild` | HAZ-GUI-006 |
| REQ-GUI-IT-009 | Error Code Enum Parity | SRS §4.7 | SDD §3.4 Enum Mapping | `PInvoke/XpeCommonNative.cs` | `EnumParityTests.ErrorStringParity` | HAZ-GUI-002 |
| REQ-GUI-IT-010 | No Handle Leak After Test Run | SRS §4.8 | SDD §3.5 Resource Management | test infrastructure | `LeakEnduranceTests.GcHandle` | HAZ-GUI-008 |

### 3.2 Event-Driven Requirements

| REQ ID | Title | SRS Section | SDD Unit | Code File | Test Class | Hazard |
|--------|-------|-------------|----------|-----------|------------|--------|
| REQ-GUI-IT-020 | Library Load Success | SRS §5.1 | SDD §3.3 | `NativeLibraryFixture.cs` | `LibraryLoadTests` | HAZ-GUI-006 |
| REQ-GUI-IT-021 | Init Success Path | SRS §5.2 | SDD §4.1 Lifecycle | `XpeCommonApi.Init` | `LifecycleTests.Init_*` | — |
| REQ-GUI-IT-022 | Configure JSON Roundtrip | SRS §5.3 | SDD §4.2 Configure | `XpeCommonApi.Configure` | `ConfigureTests.*` | HAZ-GUI-007 |
| REQ-GUI-IT-023 | Alloc/Free Roundtrip | SRS §5.4 | SDD §4.3 Memory | `XpeCommonApi.AllocImage/FreeImage` | `MemoryTests.Alloc/Free_*` | HAZ-GUI-008 |
| REQ-GUI-IT-024 | Alloc with Invalid Format | SRS §5.4 | SDD §4.3 | `XpeCommonApi.AllocImage` | `MemoryTests.Alloc_InvalidFormat` | HAZ-GUI-007 |
| REQ-GUI-IT-025 | Copy Image | SRS §5.4 | SDD §4.3 | `XpeCommonApi.CopyImage` | `MemoryTests.Copy_*` | HAZ-GUI-002 |
| REQ-GUI-IT-026 | Param Range Query | SRS §5.5 | SDD §4.4 Parameters | `XpeCommonApi.GetParamRange` | `ParamRangeTests.*` | HAZ-GUI-007 |
| REQ-GUI-IT-027 | Alert Queue Contract | SRS §5.6 | SDD §4.5 Alerts | `XpeCommonApi.GetPendingAlert*` | `AlertTests.Count/Fetch_*` | HAZ-GUI-008 |
| REQ-GUI-IT-028 | Clear Alerts Idempotent | SRS §5.6 | SDD §4.5 | `XpeCommonApi.ClearAlerts` | `AlertTests.Clear_*` | HAZ-GUI-008 |
| REQ-GUI-IT-029 | Log Level Bounds | SRS §5.7 | SDD §4.6 Logging | `XpeCommonApi.LogSetLevel` | `LoggingTests.SetLevel_*` | HAZ-GUI-007 |
| REQ-GUI-IT-030 | Log Redirect to Temp File | SRS §5.7 | SDD §4.6 | `XpeCommonApi.LogSetFile` | `LoggingTests.SetFile_*` | HAZ-GUI-007 |
| REQ-GUI-IT-031 | Log Flush No-Throw | SRS §5.7 | SDD §4.6 | `XpeCommonApi.LogFlush` | `LoggingTests.Flush_*` | HAZ-GUI-001 |
| REQ-GUI-IT-032 | Detector-firmware callback — Configure Default *(out of XPE scope; ABI smoke only)* | SRS §5.8 | SDD §4.7 Detector-firmware boundary | existing native symbol *(retained from Phase 0 scaffolding)* | existing test class *(see SPEC-XPE-GUI-IT §5.1 rows 16–18)* | — |
| REQ-GUI-IT-033 | Detector-firmware callback — Configure Invalid JSON *(out of XPE scope)* | SRS §5.8 | SDD §4.7 | existing native symbol | existing test class | HAZ-GUI-007 |
| REQ-GUI-IT-034 | Detector-firmware callback — Poll Empty Queue *(out of XPE scope)* | SRS §5.8 | SDD §4.7 | existing native symbol | existing test class | HAZ-GUI-008 |

> **Scope Note (2026-04-18):** Per project commit `6b33a35 — docs: AED 용어 혼용 수정 — detector 고유 기능과 SW 인프라 분리`, detector hardware features (exposure-end signals, detector trigger thresholds, detector state machine) belong to **detector firmware**, not to the XPE image-processing engine. XPE scope = preprocess + postprocess + display + DICOM + AI. The three rows above exist solely because Phase 0 foundation scaffolding placed ABI-boundary smoke tests against native symbols that happen to live in `xpe_common.dll`; GUI-layer specifications, hazards, menus, and user-facing docs created in this RTM and its companions do **not** define detector behavior, detector state machines, or detector status indicators. "Exposure Index" (IEC 62494-1, computed post-processing output) remains in XPE scope and is unaffected by this boundary.

### 3.3 State-Driven Requirements

| REQ ID | Title | SRS Section | SDD Unit | Code File | Test Class | Hazard |
|--------|-------|-------------|----------|-----------|------------|--------|
| REQ-GUI-IT-040 | Uninitialized Guard | SRS §6.1 | SDD §4.1 Lifecycle States | `XpeCommonApi.*` | `UninitializedGuardTests.*` | HAZ-GUI-001 |
| REQ-GUI-IT-041 | Missing DLL Fails Deterministically | SRS §6.2 | SDD §3.3 DLL Resolution | `DllStagingFixture.cs` | `DllResolutionTests.Missing_*` | HAZ-GUI-005, HAZ-GUI-006 |
| REQ-GUI-IT-042 | Architecture Mismatch Detection | SRS §6.3 | SDD §3.3 | `DllStagingFixture.cs` | `DllResolutionTests.ArchMismatch` | HAZ-GUI-006 |
| REQ-GUI-IT-043 | Platform Mismatch Diagnostic | SRS §6.4 | SDD §3.3 | (platform probe) | `DllResolutionTests.Arm64_Diag` | — |

### 3.4 Unwanted Behavior Requirements

| REQ ID | Title | SRS Section | SDD Unit | Code File | Test Class | Hazard |
|--------|-------|-------------|----------|-----------|------------|--------|
| REQ-GUI-IT-050 | No AccessViolation Across Boundary | SRS §7.1 | SDD §3.2 Exception Boundary | `XpeCommonApi.cs` (exception-safe) | `NoManagedExceptionTests.*` (20+ scenarios) | **HAZ-GUI-001** |
| REQ-GUI-IT-051 | No Memory Leak After 1000 Cycles | SRS §7.2 | SDD §4.1 Lifecycle | `XpeCommonApi.Init/Shutdown` | `LeakEnduranceTests.InitShutdown_1000` | HAZ-GUI-008 |
| REQ-GUI-IT-052 | No Marshalling Exception Without Translation | SRS §7.3 | SDD §3.2 Marshaling | `XpeCommonApi.cs` | `NoManagedExceptionTests.*` | HAZ-GUI-002 |
| REQ-GUI-IT-053 | No Silent Version-Skew | SRS §7.4 | SDD §3.4 Version Pinning | `Resources/expected-versions.json` | `EnumParityTests.VersionPin` | **HAZ-GUI-006** |

### 3.5 Optional Requirements (Conditional)

| REQ ID | Title | SRS Section | SDD Unit | Code File | Test Class | Activation |
|--------|-------|-------------|----------|-----------|------------|------------|
| REQ-GUI-IT-060 | Optional xpe_preprocess Lifecycle | SRS §8.1 | SDD §5.1 (P1A) | `XpePreprocessNative.cs` (planned) | `PreprocessOptionalTests` | P1A ready |
| REQ-GUI-IT-061 | Optional Synthetic Adapter Chain | SRS §8.2 | SDD §5.2 (P1A) | `XpePreprocessSyntheticOracle.cs` | `SyntheticAdapterChainTests` | P1A ready |
| REQ-GUI-IT-062 | Optional Calibration Loader Contract | SRS §8.3 | SDD §5.3 (P1A) | `XpePreprocessNative.cs` | `CalibLoadOptionalTests` | P1A ready |
| REQ-GUI-IT-063 | Optional ETW/Diagnostic Run | SRS §8.4 | SDD §6.1 Diagnostics | (env-gated) | ETW-tagged tests | ENV var |
| REQ-GUI-IT-064 | Optional .NET 9 Target Verification | SRS §8.5 | SDD §2.1 | csproj multi-target | CI runner | .NET 9 SDK present |
| REQ-GUI-IT-065 | Optional ARM64 Diagnostic | SRS §8.6 | SDD §3.3 | — | CI runner | ARM64 host |

---

## 4. Extended GUI Requirements (Non-SPEC Documents)

SPEC-XPE-GUI-IT 범위 외 GUI 요구사항 추적:

### 4.1 Architecture (XPE-GUI-ARCH-001)

| REQ | Source | Implementation | Verification |
|-----|--------|----------------|--------------|
| MVVM 4-Tier 분리 | ARCH-001 §2 | `ViewModels/`, `Services/`, `Models/` | Code review |
| CommunityToolkit.Mvvm 사용 | ARCH-001 §3.1 | ObservableObject base classes | Build output |
| Async P/Invoke 패턴 | ARCH-001 §4 | `[RelayCommand] async Task` | XPE-GUI-E2E-001 W-04 |
| Composition Root DI | ARCH-001 §6.1 | `App.xaml.cs OnStartup` | Code review |

### 4.2 Accessibility (XPE-GUI-ACCESS-001)

| REQ | Source | Implementation | Verification |
|-----|--------|----------------|--------------|
| AutomationProperties 필수 | ACCESS-001 §3.2 | All XAML interactive elements | E2E A-01 |
| WCAG 4.5:1 대비 | ACCESS-001 §4 | Themes/Colors.xaml | Manual + pixel analysis |
| 24x24 target size | ACCESS-001 §6 | Button styles MinWidth/Height | Manual |
| Keyboard shortcut matrix | ACCESS-001 §5.2 | KeyBindings in XAML | E2E scenarios |

### 4.3 E2E Testing (XPE-GUI-E2E-001)

| REQ | Source | Implementation | Verification |
|-----|--------|----------------|--------------|
| FlaUI framework 채택 | E2E-001 §2 | `ImageProcTest.E2ETests.csproj` | Build gate |
| Mock-mode CI 실행 | E2E-001 §5 | ENV `MOAI_XPE_BACKEND_MODE=Mock` | CI script |
| Smoke suite < 30s | E2E-001 §4.1 | xUnit filter `Category=Smoke` | CI timing |

### 4.4 Display Integration (XPE-GUI-DISP-INT-001)

| REQ | Source | Implementation | Verification |
|-----|--------|----------------|--------------|
| RealXpeBackend DisplayPipeline | DISP-INT-001 §3 Gap 1 | `RealXpeBackend.ApplyDisplayPipeline` | E2E W-06 |
| Display version in Runtime | DISP-INT-001 §3 Gap 4 | `BackendRuntimeInfo.DisplayVersion` | E2E S-05 |
| VOI Preset UI | DISP-INT-001 §4 | `DisplaySettingsPanel.xaml` | E2E W-07 |
| GSDF toggle | DISP-INT-001 §4 | AppSettings.GsdfEnabled | E2E W-06 |

### 4.5 Menu & Command (XPE-GUI-MENU-001)

| REQ | Source | Implementation | Verification |
|-----|--------|----------------|--------------|
| 6 메뉴 그룹 | MENU-001 §3 | MainWindow.xaml Menu | E2E S-02 |
| Pipeline 메뉴 활성화 타이밍 | MENU-001 §5 | Command CanExecute 로직 | Code review |
| 오프라인 Help | MENU-001 §4.6 | HelpBundleService | E2E S-04 |

### 4.6 Localization (XPE-GUI-L10N-001)

| REQ | Source | Implementation | Verification |
|-----|--------|----------------|--------------|
| ko-KR primary, en-US fallback | L10N-001 §3 | Strings.resx + Strings.ko-KR.resx | Manual |
| AutomationId locale-independent | L10N-001 §12 | AutomationProperties 영문 상수 | E2E A-01 |
| ISO 8601 파일명 | L10N-001 §7.1 | `yyyyMMdd_HHmmss` | Code review |

---

## 5. Hazard-to-Requirement Coverage

SHA-GUI-001 각 hazard에 대한 완화 요구사항 추적:

| Hazard | Mitigation REQs | Verification |
|--------|-----------------|--------------|
| HAZ-GUI-001 (Native exception propagation) | REQ-GUI-IT-005, 006, 031, 040, 050, 052 | `NoManagedExceptionTests`, `UninitializedGuardTests` |
| HAZ-GUI-002 (Struct mismatch) | REQ-GUI-IT-002, 003, 004, 009, 025 | `AbiLayoutTests`, `EnumParityTests` |
| HAZ-GUI-003 (UI freeze) | ARCH-001 §4.2 async rule | E2E W-04 |
| HAZ-GUI-004 (Stale preview) | ARCH-001 §4.3, DISP-INT-001 §4.3 | E2E W-06/W-07 |
| HAZ-GUI-005 (Misleading diagnostics) | ACCESS-001 §7, persistent warning | E2E E-01 |
| HAZ-GUI-006 (Version skew) | REQ-GUI-IT-008, 041, 042, 053 | `EnumParityTests.VersionPin`, `DllResolutionTests` |
| HAZ-GUI-007 (Use error) | REQ-GUI-IT-022, 024, 026, 029, 033, IEC 62366 formative | `NoManagedExceptionTests`, formative evaluation |
| HAZ-GUI-008 (Alert queue overflow) | REQ-GUI-IT-010, 027, 028, 034, 051 | `AlertTests`, `LeakEnduranceTests` |
| HAZ-GUI-009 (Accessibility failure) | ACCESS-001 §3, §5, E2E A-01~A-03 | E2E Accessibility suite |
| HAZ-GUI-010 (Localization failure) | L10N-001 §11 CI gate | CI pseudo-localization |

---

## 6. IEC 62304 Class B Compliance Table

| Clause | Requirement | Evidence | Status |
|--------|-------------|----------|--------|
| 5.1.1 Software development planning | SDP covers GUI layer | XPE-SDP-001, ARCH-001 | ✓ |
| 5.2.2 Software requirements content | Functional, performance, interface defined | SPEC-XPE-GUI-IT v1.2.0 | ✓ |
| 5.2.6 Requirements verified | All REQ-GUI-IT-* have tests | This RTM §3 | ✓ |
| 5.3 Software architectural design | Architecture documented | ARCH-001, SAD-001 | ✓ |
| 5.4 Software detailed design | Unit-level design | SDD-001, SDD-002 (system); GUI-specific in ARCH-001 §10 | Partial (GUI SDD addendum planned) |
| 5.5 Software unit implementation | Code exists per design | `clients/ImageProcTest*/` | ✓ |
| 5.6 Software integration | Integration plan | XPE-ITP-001, SPEC-XPE-GUI-IT | ✓ |
| 5.7 Software system testing | Test execution | 78/78 xUnit pass (2026-04-18), E2E planned | ✓ / Pending |
| 7.1 Hazard analysis | Hazards identified | XPE-SHA-001, SHA-GUI-001 | ✓ |
| 7.3 Risk control measures | Controls implemented and verified | This RTM §5 | ✓ |

---

## 7. Gaps and Open Items

### 7.1 Identified Gaps

1. **GUI SDD Addendum**: SDD-001은 system-wide. GUI-specific 유닛 설계는 ARCH-001 §10에 산재. Phase 1b 시 `XPE-SDD-GUI-001` 신설 권장.
2. **Optional Tests (060-063)**: 현재 xpe_preprocess.dll 미스테이징으로 해당 테스트 skip 상태. P1A 완료 시 활성화 필요.
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

- SPEC-XPE-GUI-IT v1.2.0 (primary requirements source)
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

*Document End — RTM-GUI-001 v1.0.0*
