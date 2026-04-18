# SPEC-XPE-GUI-IT Research Report

**Document ID**: SPEC-XPE-GUI-IT-RESEARCH
**Version**: 1.0.0
**Date**: 2026-04-18
**Author**: manager-spec (MoAI)
**Scope**: C# ImageProcTest ↔ XPE Native DLL 경계 통합 테스트 SPEC을 위한 코드베이스 분석

---

## 1. 분석 대상

C# WPF 클라이언트 `clients/ImageProcTest/`가 P/Invoke로 호출하는 XPE 네이티브 DLL(Layer 0~1) 경계를 자동화된 cross-language 통합 테스트로 검증하기 위한 선행 조사.

---

## 2. 소스 파일 인벤토리 (clients/ImageProcTest/)

### 2.1 프로젝트 루트

| 파일 | 역할 |
|------|------|
| `ImageProcTest.csproj` | .NET 8 (`net8.0-windows`), WPF, Nullable enable, ImplicitUsings enable. PackageReference 없음 — 표준 BCL 및 WPF만 사용 |
| `App.xaml.cs` | Application 시작. `--probe-native-readiness` 인자 시 headless 프로브 모드(exit code 0/1/2). 일반 실행 시 MainWindow 표시 |
| `MainWindow.xaml.cs` | UI 오케스트레이션. `CompositeXpeBackend(RealXpeCommonBackend, MockXpeBackend)`로 네이티브/모의 폴백 구성 |

### 2.2 Backends/ (4 파일)

| 파일 | 역할 |
|------|------|
| `IXpeBackend.cs` | `CheckHealth()` / `Shutdown()` 계약 |
| `RealXpeCommonBackend.cs` | `XpeCommonApi.xpe_init/version/error_string/get_param_range/alloc_image/free_image/get_pending_alert_count/aed_get_status` 실호출. `DllNotFoundException`, `EntryPointNotFoundException`, `BadImageFormatException` 예외 변환 |
| `MockXpeBackend.cs` | 네이티브 로드 실패 시 GUI 워이어링 유지용 폴백 |
| `CompositeXpeBackend.cs` | Real → Mock 폴백, Details 병합 |
| `BackendHealthResult.cs` | 결과 record (`IsNativeReady` 포함 11 필드) |

### 2.3 Diagnostics/ (4 파일)

| 파일 | 역할 |
|------|------|
| `NativeReadinessProbe.cs` | `xpe-native-readiness-v1` 스키마 JSON 리포트 생성. ABI 크기 `Marshal.SizeOf<XpeImageBuffer/XpeImageMetadata>()` 기록 |
| `XpeDisplayVersionProbe.cs` | `xpe_display.dll` `xpe_display_version` export-only 탐지 (UnmanagedFunctionPointer 델리게이트) |
| `XpePreprocessReadinessProbe.cs` | `xpe_preprocess.dll` 9개 필수 export + 1개 실행 export 검증 + synthetic oracle 트리거 |
| `XpePreprocessSyntheticOracle.cs` | 16x16 합성 이미지로 offset→gain→defect 3단 어댑터 체인 실행 + determinism 검증 (RMSE). `GCHandle.Alloc(Pinned)`로 ushort[]/float[] 핀. `xpe_preprocess_init/shutdown` 델리게이트 로드 |

### 2.4 Services/ (4 파일)

| 파일 | 역할 |
|------|------|
| `ModuleReadinessService.cs` | 8개 모듈(common/display/preprocess/enhance_basic/dicom/enhance_advanced/gsvg/ai)의 readiness level R0~R3 평가 |
| `FixtureCatalogService.cs` | `tests/test_data/calibration_cases/*/images/*.raw` + `calibration/*.raw` 디렉토리 스캔 |
| `RawPreviewService.cs` | uint16 raw → WriteableBitmap Gray8 미리보기. 차원 추론은 KnownDimensions 16개 + square fallback |
| `GuiE2eReportService.cs` | `gui-e2e-scaffold-v1` JSON + Markdown 리포트 출력 (`gui-e2e-reports/gui-e2e-{yyyyMMdd-HHmmss}`) |

### 2.5 Models/ (4 파일)

| 파일 | 역할 |
|------|------|
| `ModuleReadinessSnapshot.cs` | record (ModuleName, Level, Status, Evidence, NextAction, ProcessingEnabled) |
| `RawFileDescriptor.cs` | 파일 메타 (Name, Path, Length, DisplayName). FormatBytes 유틸 |
| `FixtureCaseInfo.cs` | 케이스 (Name, RootPath, Images, CalibrationFiles) |
| `RawPreviewResult.cs` | record (FilePath, FileSizeBytes, Width, Height, PreviewWidth, PreviewHeight, SampleStride, Min/MaxValue, Sha256, WriteableBitmap) |

### 2.6 PInvokeWrapper.cs (중요)

1개 정적 internal 클래스 `XpeCommonApi` — 이하 Section 3.1에 상세 기술.

---

## 3. 네이티브 심볼 참조 전수 조사

### 3.1 PInvokeWrapper.cs: `[DllImport]` 선언 (17개, xpe_common.dll)

| # | C# 시그니처 요약 | 네이티브 심볼 | 헤더 위치 |
|---|------------------|----------------|-----------|
| 1 | `xpe_init(string? configJsonOrNull) -> XpeErrorCode` | `xpe_init` | xpe_common_api.h:13 |
| 2 | `xpe_shutdown() -> void` | `xpe_shutdown` | xpe_common_api.h:14 |
| 3 | `xpe_version() -> IntPtr` (ANSI 문자열) | `xpe_version` | xpe_common_api.h:15 |
| 4 | `xpe_configure(string configJson) -> XpeErrorCode` | `xpe_configure` | xpe_common_api.h:18 |
| 5 | `xpe_get_param_range(string, string, out float, out float, out float) -> XpeErrorCode` | `xpe_get_param_range` | xpe_common_api.h:21 |
| 6 | `xpe_error_string(XpeErrorCode) -> IntPtr` | `xpe_error_string` | xpe_error.h |
| 7 | `xpe_get_pending_alert_count() -> int` | `xpe_get_pending_alert_count` | api-spec 5.9 |
| 8 | `xpe_get_pending_alert(int, StringBuilder, UIntPtr, out int) -> XpeErrorCode` | `xpe_get_pending_alert` | api-spec 5.10 |
| 9 | `xpe_clear_alerts() -> void` | `xpe_clear_alerts` | api-spec 5.11 |
| 10 | `xpe_alloc_image(uint, uint, XpePixelFormat, out XpeImageBuffer) -> XpeErrorCode` | `xpe_alloc_image` | xpe_memory.h |
| 11 | `xpe_free_image(ref XpeImageBuffer) -> XpeErrorCode` | `xpe_free_image` | xpe_memory.h |
| 12 | `xpe_copy_image(ref XpeImageBuffer, ref XpeImageBuffer) -> XpeErrorCode` | `xpe_copy_image` | xpe_memory.h |
| 13 | `xpe_log_set_level(int) -> XpeErrorCode` | `xpe_log_set_level` | xpe_common_api.h:25 |
| 14 | `xpe_log_set_file(string) -> XpeErrorCode` | `xpe_log_set_file` | xpe_common_api.h:26 |
| 15 | `xpe_log_flush() -> void` | `xpe_log_flush` | xpe_common_api.h:27 |
| 16 | `xpe_aed_configure(string) -> XpeErrorCode` | `xpe_aed_configure` | xpe_common_api.h:30 |
| 17 | `xpe_aed_poll_event(out int, out ulong, out float) -> XpeErrorCode` | `xpe_aed_poll_event` | xpe_common_api.h:31 |
| 18 | `xpe_aed_get_status(out int) -> XpeErrorCode` | `xpe_aed_get_status` | xpe_common_api.h:32 |

총 **18개 함수 = api-spec.md v1.3.0 Section 5 xpe_common.dll 함수 목록과 일치** (SPEC-XPE-P0 REQ-P0-008 충족 확인).

### 3.2 NativeLibrary.TryGetExport 동적 바인딩 (별도 경로)

| 호출처 | DLL | 심볼 | 목적 |
|--------|-----|------|------|
| `XpeDisplayVersionProbe.cs:30` | `xpe_display.dll` | `xpe_display_version` | version-only probe |
| `XpePreprocessReadinessProbe.cs` (RequiredExports[0]) | `xpe_preprocess.dll` | `xpe_preprocess_version` | version |
| 동 (1) | 동 | `xpe_preprocess_init` | lifecycle |
| 동 (2) | 동 | `xpe_preprocess_shutdown` | lifecycle |
| 동 (3) | 동 | `xpe_calib_load_offset` | calibration |
| 동 (4) | 동 | `xpe_calib_load_gain` | calibration |
| 동 (5) | 동 | `xpe_calib_load_defect_map` | calibration |
| 동 (6) | 동 | `xpe_offset_correct` | correction |
| 동 (7) | 동 | `xpe_gain_correct` | correction |
| 동 (8) | 동 | `xpe_defect_correct` | correction |
| `ExecutionExports[0]` | 동 | `xpe_preprocess_apply_pipeline` | pipeline (SPEC-P1A 범위 외) |

`XpePreprocessSyntheticOracle.cs`에서 위 9개 중 init/shutdown/offset/gain/defect (5개)를 `Marshal.GetDelegateForFunctionPointer`로 델리게이트화해 직접 호출.

### 3.3 NativeLibrary.SetDllImportResolver (커스텀 경로)

`XpeCommonApi.cs:18`에 설정. 탐색 경로 우선순위:
1. `AppContext.BaseDirectory/xpe_common.dll`
2. `<repo>/build/ci-common/bin/xpe_common.dll`
3. `<repo>/build/ci-common/bin/Debug/xpe_common.dll`
4. `<repo>/build/default/bin/xpe_common.dll`
5. `<repo>/build/default/bin/Debug/xpe_common.dll`
6. `<repo>/build/readiness-display-vs/bin/Debug/xpe_common.dll`
7. `<repo>/build/readiness-preprocess-vs/bin/Debug/xpe_common.dll`
8. `<repo>/gui/ImageProcTest/bin/Debug/net8.0-windows/xpe_common.dll` (구 디렉토리 호환)

마지막 fallback으로 OS 기본 검색 (`NativeLibrary.TryLoad(libraryName, assembly, searchPath)`).

### 3.4 요약: 테스트 표면

- **정적 P/Invoke 대상**: 18 (`xpe_common.dll` 전량, Section 3.1)
- **동적 로드 대상**: 10+ (`xpe_preprocess.dll`, `xpe_display.dll`)
- **합성 오라클 대상**: 5 (`xpe_preprocess` lifecycle 2 + correction 3)
- **ABI 경계 구조체**: `XpeImageBuffer` (Pack=8), `XpeImageMetadata` (Pack=8, ANSI BodyPart[64])
- **ABI 경계 enum**: `XpePixelFormat` (uint), `XpeErrorCode` (int), `XpeAlertSeverity` (int)

---

## 4. 빌드 플로우 (현재 상태)

### 4.1 DLL 빌드 → GUI 탑재

```
CMake preset (ci-common/default)
  → <repo>/build/<preset>/bin/Debug/xpe_common.dll
  → <repo>/build/<preset>/bin/Debug/xpe_preprocess.dll (existence 불확실)
  → <repo>/build/<preset>/bin/Debug/xpe_display.dll
```

C# 프로젝트 (`ImageProcTest.csproj`)는 **DLL을 복사하지 않음**. 런타임에 `NativeLibrary.SetDllImportResolver` 콜백으로 repo root에서 탐색. 이는 개발 편의성은 높지만 **CI 격리 실행에는 취약** (빌드 디렉토리 prerequisite 있음).

### 4.2 csproj가 참조하지 않는 것

- PackageReference 없음 (xUnit, NUnit, MSTest 전부 부재)
- Content/None Include로 DLL 복사 타겟 없음
- RuntimeIdentifier 지정 없음 (x64 강제 아님) — **BadImageFormatException 리스크 존재**

### 4.3 GUI 실행 모드

| 모드 | 기동 방식 | 산출물 |
|------|----------|--------|
| 일반 WPF | `dotnet run` | MainWindow 표시 |
| Headless probe | `ImageProcTest.exe --probe-native-readiness` | `native-readiness-report.json` + 종료코드 0/1/2 |

Headless probe는 **사실상 scaffold 수준 통합 테스트**이나 xUnit 등 표준 테스트 러너로는 수집되지 않음.

---

## 5. 기존 테스트 인프라

### 5.1 C# 측

- `clients/ImageProcTest.*.Tests` 프로젝트: **없음** (Glob 조회 결과 0)
- `gui/ImageProcTest.Tests/QaConstancyTests.cs`: structure.md에는 참조되지만 실제 파일 **확인되지 않음** (본 조사 범위 외 / 추후 확인 필요)
- 따라서 C# 레이어의 cross-language 통합 테스트는 **공식 xUnit 러너 기반으로 존재하지 않음**

### 5.2 C++ 측 (참고)

- `tests/common/` Google Test 단위 테스트 (SPEC-XPE-P0 P0-06에서 구축)
- `tests/common_smoke/` 통합 스모크 (C++ 내부)
- CTest discovery로 통합되나 **C#/C++ 경계는 검증하지 않음**

### 5.3 결론

현 저장소에 **C#↔native cross-language 자동화 통합 테스트 프로젝트가 없음**. `App.xaml.cs`의 `--probe-native-readiness` 경로는 ad-hoc 스크립트에 가깝고, 이를 xUnit 기반 결정적 테스트 스위트로 승격하는 것이 본 SPEC의 과제.

---

## 6. 리스크 분석

### 6.1 ABI 리스크

| # | 리스크 | 증거 | 영향 | 완화 방향 |
|---|--------|------|------|-----------|
| R-01 | `XpeImageBuffer` Pack=8 mismatch (C#40B vs C++ >40B) | api-spec.md §1 P/Invoke Note: 40 bytes on x64. C# `nuint DataSize` / C++ `size_t dataSize` 플랫폼별 크기 | AccessViolation, 랜덤 데이터 손상 | `Marshal.SizeOf<XpeImageBuffer>()` vs 컴파일된 DLL의 export된 상수 또는 `xpe_abi_get_buffer_size()` 비교 assertion |
| R-02 | `XpeImageMetadata.BodyPart` ANSI vs UTF-8 불일치 | C# `[MarshalAs(UnmanagedType.ByValTStr, SizeConst=64)]` + `CharSet.Ansi`. api-spec은 UTF-8 명시 안함, 헤더 주석은 "Null-terminated body part label" | 다국어 bodyPart 손상, 64바이트 초과 truncation 무고지 | ASCII-only 계약 검증 테스트, 경계 바이트 검증 |
| R-03 | x86/x64 build mismatch | `ImageProcTest.csproj`에 `<Platforms>` / `<RuntimeIdentifier>` 없음 | `BadImageFormatException` (`RealXpeCommonBackend.cs:60`에서 catch) | 테스트에서 `RuntimeInformation.ProcessArchitecture`와 DLL 아키텍처 assertion |
| R-04 | DLL search path drift | 8개 hardcoded 후보 경로가 코드에 박혀 있음 (`XpeCommonApi.cs:215-224`) | 빌드 디렉토리 레이아웃 변경 시 조용한 실패 → Mock으로 fallback | 테스트에서 `XpeCommonApi.ResolvedDllPath` 검증, CI에서는 predictable한 DLL staging 전략 채택 |

### 6.2 생명주기/상태 리스크

| # | 리스크 | 증거 | 완화 |
|---|--------|------|------|
| R-05 | init 없이 호출 시 `XPE_ERR_NOT_INITIALIZED`가 돌아오지 않고 crash | api-spec 5.1: "Not thread-safe; call from a single thread at startup" | Negative test: init 미호출 상태에서 각 함수 호출 시 에러 코드 확인 (void 반환 함수는 제외) |
| R-06 | 1000회 init/shutdown 사이클에서 누적 메모리 누수 | SPEC-XPE-P0 Acceptance 3.2: "No memory leaks in 1000-cycle init/shutdown test (ASan clean)" — C++ 측만 검증됨 | C# 레이어에서도 동일 회차 반복하여 Process working set delta 확인 |
| R-07 | AED 상태 IDLE/ARMED/TRIGGERED 전이 누락 | REQ-P0-028: state 0/1/2 | `xpe_aed_configure(null)` → `xpe_aed_get_status` → ARMED 확인. 미초기화 시 IDLE |

### 6.3 예외/마샬링 리스크

| # | 리스크 | 증거 | 완화 |
|---|--------|------|------|
| R-08 | C++ 예외가 ABI 경계를 넘어 managed로 전파 → 프로세스 종료 | REQ-P1A-030 (유사 원칙), C ABI 계약상 금지 | 비정상 JSON, 거대 버퍼 등 악성 입력을 주입하여 managed AccessViolationException/SEHException 부재 검증 |
| R-09 | `StringBuilder` 기반 alert msg 버퍼 너무 작을 때 silent truncation | `xpe_get_pending_alert(int, StringBuilder, UIntPtr, out int)` | 작은 StringBuilder로 호출 시 `BUFFER_TOO_SMALL` 반환 확인 |
| R-10 | `Marshal.PtrToStringAnsi`가 반환한 문자열을 DLL이 나중에 dealloc해도 C# 쪽에는 유효하다고 가정 | api-spec §1: "Strings returned as const char* are owned by the DLL (static storage); do NOT free them" | `xpe_version()`/`xpe_error_string()` 결과를 여러 번 호출 후에도 같은지 확인 (static storage 불변성 smoke) |

### 6.4 버전 스큐 리스크

| # | 리스크 | 증거 | 완화 |
|---|--------|------|------|
| R-11 | DLL 버전과 C# `XpeErrorCode` enum 값 drift | 현재 C# enum은 -1..-10만 정의, `XPE_STATUS_NO_EVENT = 1` (REQ-P0-028a) 미반영 | 테스트에서 `xpe_error_string`을 모든 enum 값에 대해 호출해 non-NULL 확인 + `XPE_STATUS_NO_EVENT` 대응 강제 |
| R-12 | platform mismatch 시 조용한 Mock fallback | `CompositeXpeBackend.CheckHealth`가 native 실패 시 Mock 결과 반환 | 통합 테스트는 **Mock fallback을 테스트 실패로 간주**해야 하며 `IsNativeReady=true` assertion 필수 |

---

## 7. 참조 패턴 (본 SPEC 구현 시 채택 권장)

### 7.1 프로젝트 구조

- `clients/ImageProcTest.IntegrationTests/` (신규) — **xUnit 2.9+** 기반 C# 8.0 테스트 프로젝트
- `TargetFramework`: `net8.0-windows` (WPF 의존성 없음이면 `net8.0` 가능하나, `XpeCommonApi` 내부에서 Reflection 기반 resolver를 써 WPF와 무관 — `net8.0`만으로 충분)
- `PlatformTarget=x64` 강제 (`BadImageFormatException` 예방)
- `CopyLocalLockFileAssemblies=true` + MSBuild Target으로 `build/**/bin/Debug/*.dll` → 테스트 output 디렉토리 복사

### 7.2 테스트 카테고리 (Trait 기반 분류)

- `[Trait("Category", "Smoke")]` — 10초 이하: DLL 로드, version, init/shutdown
- `[Trait("Category", "Functional")]` — 1분 이하: 모든 18 P/Invoke 함수 contract
- `[Trait("Category", "Safety")]` — 3분 이하: null/invalid/boundary/1000-cycle
- `[Trait("Category", "ErrorMapping")]` — 초: XpeErrorCode ↔ xpe_error_string
- `[Trait("Category", "Lifecycle")]` — 3분 이하: init-many, aed-transition

### 7.3 DLL 스테이징 전략 (2안 중 택1)

- **Option A (권장)**: MSBuild `<Target Name="CopyXpeDlls" BeforeTargets="Build">`로 최신 `build/ci-common/bin/Debug/*.dll`을 `$(OutputPath)`로 복사. 빌드 의존성은 CMake 측이 선행.
- **Option B**: `NativeLibrary.SetDllImportResolver` 패턴을 테스트 프로젝트에도 도입해 `XpeCommonApi`와 동일 탐색 로직 유지. (현 상태 유지 — 더 적은 기반 작업, 더 높은 경로 drift 리스크)

### 7.4 결정성/재현성 원칙

- 난수 사용 시 `Random(seed=0)` 고정
- 합성 이미지는 `XpePreprocessSyntheticOracle.RunChain`의 `(ushort)(1000 + (index % 97))` 패턴을 참고
- 성능 임계값은 env-var (`XPE_GUI_IT_SLOW_GATE_MS=30000`)로 조정 가능

### 7.5 Mock 방지

- `CompositeXpeBackend` 구조를 **테스트에서는 사용하지 않음**. 테스트는 `RealXpeCommonBackend` 단독 구성 또는 `XpeCommonApi` 직호출만 사용
- 테스트 시작 시 `XpeCommonApi.ResolvedDllPath`가 `repo/build/**` 하위인지 assertion — 그 외는 stale/wrong DLL 신호

---

## 8. SPEC-XPE-P0 / SPEC-XPE-P1A와의 관계

### 8.1 중복 방지

- P0는 xpe_common.dll **네이티브 구현** 및 **C++ 단위/통합 테스트** 범위. C#↔native 경계 검증은 P0 수용 기준 §3.3에 "ImageProcTest.exe builds and runs", "DLL loads via P/Invoke without DllNotFoundException", "version returned correct" 3개 항목만 scaffold 수준으로 존재.
- **P0는 P/Invoke round-trip을 "C# 측 테스트 프레임워크로" 검증하지 않음** — 이것이 본 SPEC의 고유 범위.
- P1A는 xpe_preprocess.dll 14개 함수 **native 구현 + C++ TDD**. GUI 경계 테스트는 P1A 밖.

### 8.2 본 SPEC의 고유 범위

1. C# xUnit 기반 **cross-language** P/Invoke 통합 테스트 스위트
2. Pack=8 ABI 크기 parity runtime assertion
3. 에러 코드 enum parity 전수 매핑
4. 1000-cycle init/shutdown을 **managed runtime에서** 검증
5. Mock backend가 통합 테스트에서 활성화되지 못하게 하는 가드
6. DLL search path drift 감지

### 8.3 미래 연계 (P1A 완료 후)

- 본 SPEC의 preprocess 대상 테스트는 **선택적 (Optional, Where)** 으로 구성 → P1A 산출물이 준비되면 on 되는 훅 제공
- `XpePreprocessSyntheticOracle`를 xUnit `[Fact]`로 승격하여 동일한 adapter-chain을 통합 테스트가 수용

---

## 9. 공개 질문 / 확인 필요 사항

1. **테스트 프로젝트 경로**: `clients/ImageProcTest.IntegrationTests/` 로 통일할지, 또는 structure.md가 예고하는 `gui/ImageProcTest.Tests/`로 갈지 (정책 판단 → SPEC 본문은 `clients/` 전제)
2. **ARM64 테스트 요구 여부**: tech.md는 ARM64 "지연" 상태 → Optional 요구사항으로 다룸
3. **hardware detector 연결 여부**: 본 SPEC은 Excluded로 고정 (실 panel, GPU, AED 실 이벤트 생성 제외)
4. **최소 coverage 대상 단위**: "PinInvoke surface" = 18 xpe_common 함수 + dynamic 10+ preprocess 심볼. P/Invoke 선언 한 줄당 한 개 이상의 Functional 테스트 존재 여부를 coverage 기준으로 삼는다

---

## 10. 결론

본 SPEC-XPE-GUI-IT는 **구현 가능**하며, 선행조건(`xpe_common.dll` 빌드 산출물 존재)은 P0 완료로 이미 충족되어 있다. P1A 완료 전까지는 preprocess-대상 테스트는 Optional로 구성, 완료 후 자동 활성화되는 구조를 설계한다. C# 레이어에 신규 xUnit 테스트 프로젝트(`clients/ImageProcTest.IntegrationTests/`)를 추가하고, DLL 스테이징은 MSBuild Target으로 명시적 복사를 권장한다.

*Document End — SPEC-XPE-GUI-IT-RESEARCH v1.0.0*
