# FlaUI E2E Testing Plan

**Document ID**: XPE-GUI-E2E-001
**Version**: 1.0.0
**Date**: 2026-04-18
**Status**: Controlled Draft
**Canonical Scope**: `docs/project/`
**Target**: `clients/ImageProcTest/` WPF test GUI end-to-end automation
**Related Specs**: SPEC-XPE-GUI-IT v1.2.0, XPE-GUI-ARCH-001, XPE-GUI-ACCESS-001

---

## 1. Purpose

본 문서는 `ImageProcTest.exe` GUI의 **end-to-end(E2E) 자동화 테스트 전략**을 정의한다. SPEC-XPE-GUI-IT가 다루지 않는 **UI 계층 자동화** — MainWindow 상호작용, 메뉴 커맨드 실행, 비교 뷰포트, fixture 로딩, 오류 복구 경로 — 를 포괄한다.

**구분:**

| 범주 | Owner | 프레임워크 | 범위 |
|------|-------|------------|------|
| Cross-language IT | SPEC-XPE-GUI-IT | xUnit | P/Invoke 경계, 네이티브 ABI |
| Unit | ImageProcTest.UnitTests (계획) | xUnit | ViewModel 로직 |
| **E2E** | **본 문서** | **FlaUI (UIA3)** | **UI interaction + workflow** |

---

## 2. Framework Selection Rationale

### 2.1 후보 비교

| 프레임워크 | 장점 | 단점 | 결정 |
|-----------|------|------|------|
| **FlaUI (UIA3)** | WPF 최적, 순수 .NET, COM 기반 Windows 표준 UIA, CI 친화(headless 지원), AppVeyor 검증됨, 경량 | Windows 전용, 학습자료 영문 위주 | **채택** |
| WinAppDriver | Microsoft 공식, Appium 호환 | 2023 이후 유지보수 정체, WPF DataGrid 버그, 설치 복잡 | Rejected |
| TestStack.White | 단순한 API | 2018년 deprecated | Rejected |
| Playwright | 최신 웹 표준 | **WPF 미지원 (브라우저 전용)** — native Windows 앱 불가 | Rejected |
| Coded UI | Visual Studio 통합 | 2019 deprecated | Rejected |

### 2.2 FlaUI 채택 근거

- Source: https://github.com/FlaUI/FlaUI (2024 활발히 유지보수, UIA2/UIA3 지원)
- Microsoft UI Automation (UIA3)은 Windows Automation API 3.0의 공식 인터페이스
- Source: https://learn.microsoft.com/en-us/windows/win32/winauto/entry-uiauto-win32 (2025-07-14)
- WPF는 `AutomationPeer`/`AutomationProperties`를 통해 UIA와 네이티브 통합 — 별도 어댑터 불필요
- Source: https://learn.microsoft.com/en-us/dotnet/framework/ui-automation/ui-automation-fundamentals

### 2.3 UIA3 vs UIA2 선택

- [HARD] **UIA3를 기본 채택** — WPF/Store Apps에 최적화된 최신 API
- [HARD] **UIA2는 fallback** — 레거시 컴포넌트 호환 필요 시에만
- FlaUI는 두 백엔드를 모두 지원하며 런타임에 `using var app = FlaUI.Core.Application.Launch(...)` + `AutomationBase automation = new UIA3Automation()` 선택

---

## 3. Test Project Layout

```
clients/
└── ImageProcTest.E2ETests/                   (NEW - this document)
    ├── ImageProcTest.E2ETests.csproj         (net8.0, x64, NOT packable)
    ├── Fixtures/
    │   ├── ApplicationFixture.cs             (IAsyncLifetime, launch/dispose app)
    │   ├── AutomationFixture.cs              (UIA3Automation singleton per test class)
    │   └── TestDataFixture.cs                (fixture catalog paths)
    ├── PageObjects/
    │   ├── MainWindowPage.cs                 (FlaUI Window wrapper)
    │   ├── MenuPage.cs
    │   ├── ToolbarPage.cs
    │   ├── DisplayPanel.cs                   (Phase 1b)
    │   ├── PreprocessPanel.cs                (Phase 1a)
    │   └── AlertsPanel.cs
    ├── Scenarios/
    │   ├── Smoke/
    │   │   ├── Launch_HasMainWindow.cs
    │   │   ├── Menu_AllGroupsPresent.cs
    │   │   └── Help_OpensOffline.cs
    │   ├── Workflows/
    │   │   ├── LoadRawImage_DisplaysInViewport.cs
    │   │   ├── RunPreprocess_ShowsProcessedImage.cs
    │   │   ├── ToggleBackendMode_UpdatesRuntime.cs
    │   │   └── CompareMode_SwitchesLayout.cs
    │   ├── ErrorRecovery/
    │   │   ├── DllMissing_ShowsAlert.cs
    │   │   ├── InvalidImage_ShowsError.cs
    │   │   └── BackendCrash_RecoversGracefully.cs
    │   └── Accessibility/
    │       ├── AllControlsHaveAutomationId.cs
    │       ├── KeyboardNavigation_TabOrder.cs
    │       └── ScreenReader_NameProperty.cs
    ├── Utilities/
    │   ├── FixtureResolver.cs
    │   ├── ScreenshotCapture.cs
    │   └── AutomationRetry.cs                (polling helpers)
    └── Resources/
        └── scenarios/                        (YAML automation scripts for headless mode)
```

---

## 4. Test Scenarios (IEC 62304 Class B Traceability)

### 4.1 Smoke Test Suite (Gate: < 30s)

| # | Scenario | AutomationId(s) | Expected |
|---|----------|------------------|----------|
| S-01 | 앱 실행 후 MainWindow 가시 | `XPE_Main_Window` | Window 있음, title 포함 "ImageProcTest" |
| S-02 | 메뉴바 6개 그룹 존재 | `XPE_Menu_File/Backend/View/Pipeline/Tools/Help` | 6 MenuItem 확인 |
| S-03 | 툴바 기본 버튼 클릭 가능 | `XPE_Toolbar_Open/Run/Reset` | IsEnabled=true for mock mode |
| S-04 | Help Home 오프라인 열림 | `XPE_Menu_Help_Home` | HTML 뷰어 팝업 또는 inline |
| S-05 | Runtime Panel 버전 표시 | `XPE_Runtime_CommonVersion` | non-empty semver |

### 4.2 Workflow Suite (Gate: < 3min)

| # | Scenario | Action Sequence | Verification |
|---|----------|-----------------|--------------|
| W-01 | Raw 이미지 로드 | File→Open Raw → fixture 선택 → OK | `XPE_Viewport_Source` has image |
| W-02 | Preprocess 실행 (P1A) | Pipeline→Run Preprocessing | `XPE_Viewport_Processed` updated, log entry |
| W-03 | 백엔드 모드 전환 | Backend→Backend Mode→Mock↔Real | Runtime Panel `Backend Mode` 표시 |
| W-04 | 비교 모드 Swipe | View→Compare Mode→Swipe | Viewport overlay 표시 |
| W-05 | 비교 모드 Difference | View→Compare Mode→Difference | Difference 이미지 표시 |
| W-06 | Display Pipeline 적용 (P1B) | View→Display LUT→GSDF On | Processed image re-rendered |
| W-07 | VOI Preset 변경 (P1B) | Display Panel→Body Part→Lung | Window width/center 프리셋 값 적용 |
| W-08 | Settings 저장/복원 | 설정 변경 → 재시작 → 값 유지 | appsettings.json 읽기 |
| W-09 | Automation report export | Tools→Export Automation Report | TRX 파일 생성 |
| W-10 | Fixture calibration 로드 | Tools→Fixture Manager→Select | fixture SHA-256 표시 |

### 4.3 Error Recovery Suite (IEC 62304 Class B Safety)

| # | Scenario | Injection | Expected Recovery |
|---|----------|-----------|-------------------|
| E-01 | xpe_common.dll 부재 | DLL 제거 후 시작 | Mock mode 자동 전환 + 경고 배너 |
| E-02 | 잘못된 RAW 파일 | 텍스트 파일을 `.raw`로 선택 | 알림 패널 "Invalid format" |
| E-03 | 백엔드 호출 중 예외 | Mock가 throw | Alert toast + 로그 기록, 앱 유지 |
| E-04 | P/Invoke 시간 초과 | 장시간 연산 중 cancel | 진행 취소 가능, UI 응답성 유지 |
| E-05 | DLL 버전 스큐 | expected-versions.json mismatch | Runtime Panel 경고 + 차단 |
| E-06 | Disk full during log | 로그 디렉토리에 read-only | Alert 없이 graceful degradation |

### 4.4 Accessibility Suite

자세한 기준은 XPE-GUI-ACCESS-001 참조.

| # | Scenario | Check |
|---|----------|-------|
| A-01 | 모든 인터랙티브 요소가 AutomationId 보유 | FlaUI `descendants.All(e => !string.IsNullOrEmpty(e.AutomationId))` |
| A-02 | Tab 순서 논리적 진행 | `TabIndex`로 시각적 순서 검증 |
| A-03 | `AutomationProperties.Name` 비어있지 않음 | UIA `NameProperty` != empty |
| A-04 | 4.5:1 대비 (Light theme) | 스크린샷 픽셀 분석 (선택적) |

---

## 5. Mock-Mode E2E for CI

### 5.1 전략

- [HARD] CI는 **기본적으로 Mock 백엔드**로 E2E 실행 (네이티브 DLL 빌드 의존성 없음)
- 환경변수: `MOAI_XPE_BACKEND_MODE=Mock`
- `RealXpeBackend` 검증 시나리오는 nightly 작업에서 DLL 스테이징 후 실행

### 5.2 Mock 특수 시나리오

- `MockXpeBackend`는 모든 `Process*` 메서드에 대해 **결정적 합성 이미지** 반환
- Fixture 카탈로그의 synthetic patterns (seed=0) 활용
- SPEC-XPE-GUI-IT의 `XpePreprocessSyntheticOracle` 재사용

### 5.3 CI 매트릭스

| Pipeline | Mode | Frequency | Duration Gate |
|----------|------|-----------|---------------|
| PR E2E (Smoke) | Mock | 모든 PR | < 30s |
| PR E2E (Workflow) | Mock | 모든 PR | < 3min |
| Nightly E2E (Full) | Mock + Real | 매일 00:00 UTC | < 15min |
| Release E2E | Real (하드웨어 없음) | Release 태그 시 | < 30min |

---

## 6. Test Data Strategy

### 6.1 Fixture Catalog Reuse

본 E2E는 SPEC-XPE-P1A/P1B의 fixture catalog를 재사용한다:

- `tests/fixtures/preprocess/*.raw` (정적 합성 이미지)
- `tests/fixtures/display/*.raw` (LUT 검증용)
- `tests/fixtures/calibration/*.xcal` (XCal 파일)

### 6.2 E2E 전용 fixture

| Fixture | 용도 | 위치 |
|---------|------|------|
| `e2e-small-16x16-seed0.raw` | Smoke 로드 | `tests/fixtures/e2e/` |
| `e2e-2048x2048-uniform.raw` | Performance / rendering | `tests/fixtures/e2e/` |
| `e2e-malformed.raw` | Error path | `tests/fixtures/e2e/` |
| `e2e-scenarios/smoke.yaml` | Headless automation 스크립트 | `tests/fixtures/e2e/scenarios/` |

### 6.3 [HARD] 금지 사항

- [HARD] **실제 환자 이미지 (PHI) 사용 금지**
- [HARD] **네트워크 DICOM 서버 사용 금지**
- [HARD] **하드웨어 FPD 패널 의존성 금지** — 모든 fixture는 합성

---

## 7. Integration with /moai e2e Workflow

### 7.1 명령 통합

`/moai e2e SPEC-XPE-GUI-IT`는 다음을 실행:

1. `dotnet build clients/ImageProcTest` (GUI 빌드)
2. `dotnet build clients/ImageProcTest.E2ETests`
3. `dotnet test clients/ImageProcTest.E2ETests --filter "Category=Smoke" --logger "trx;LogFileName=smoke.trx"`
4. (Optional) `--filter "Category=Workflow"` full 실행
5. TRX + 스크린샷을 `.moai/reports/e2e-{DATE}/` 저장

### 7.2 xUnit Trait 명명 규칙

```csharp
[Trait("Category", "Smoke")]        // < 30s gate
[Trait("Category", "Workflow")]     // < 3min gate
[Trait("Category", "ErrorRecovery")]
[Trait("Category", "Accessibility")]
[Trait("Phase", "P1A")]             // 조건부 — P1A ready 시만
[Trait("Phase", "P1B")]             // 조건부 — P1B ready 시만
[Trait("RequiresDll", "xpe_common")] // DLL 의존성
```

### 7.3 Skip 규칙

- DLL 부재 시: `Skip.If(!DllStagingResolver.IsAvailable("xpe_preprocess.dll"), "preprocess DLL not staged")`
- Mock 강제 모드 시: `Skip.If(BackendMode == "Mock" && requiresReal, "Real backend required")`

---

## 8. Automation Utilities

### 8.1 Wait/Retry 패턴

UIA는 **비동기 이벤트 기반** — FlaUI element discovery는 간헐적 `StaleElementException` 발생 가능. 모든 `FindFirst`/`FindAll` 호출은 retry wrapper 경유:

```csharp
public static T RetryFindFirst<T>(Func<T> finder, TimeSpan timeout)
{
    var deadline = DateTime.UtcNow + timeout;
    Exception? last = null;
    while (DateTime.UtcNow < deadline)
    {
        try { var r = finder(); if (r != null) return r; }
        catch (Exception ex) { last = ex; }
        Thread.Sleep(100);
    }
    throw new TimeoutException($"Element not found within {timeout}", last);
}
```

### 8.2 Screenshot Capture

실패 시 자동 스크린샷 저장:

- 경로: `gui-e2e-reports/{timestamp}/{TestName}_failure.png`
- FlaUI의 `Capture.Screen()` 또는 `Capture.Element(element)` 사용
- xUnit 3.x 지원 시 `IAsyncLifetime.DisposeAsync`에서 실패 감지

---

## 9. Quality Gates

| Gate | Threshold | Action on Fail |
|------|-----------|----------------|
| Smoke pass rate | 100% | Block PR merge |
| Workflow pass rate | >= 95% | Block PR merge (flaky tolerated once) |
| Error recovery pass | 100% | Block (safety-critical) |
| Accessibility ID coverage | 100% | Block |
| Smoke duration | < 30s | Warn |
| Full E2E duration | < 15min | Warn |

---

## 10. Known Limitations

1. **FlaUI on CI without display**: Windows Server Core (headless)에서도 UIA3는 **데스크탑 세션 필요**. GitHub Actions Windows runner는 interactive session 제공하므로 문제없음
2. **Screen resolution dependency**: Viewport 기반 검증은 최소 1920x1080 보장. CI runner 설정에 명시
3. **Localization**: E2E 스크립트는 `en-US` 기준 string matching. 다국어 지원 시 AutomationId 기반 검색으로 전환 (`XPE_Menu_Help` vs "Help")
4. **Hardware-dependent tests**: 실제 FPD 패널 연결 테스트는 out-of-scope — 별도 manual validation

---

## 11. Sources (Verified 2026-04-18)

1. FlaUI GitHub Project (2024 active): https://github.com/FlaUI/FlaUI
2. Microsoft UI Automation - Win32 (2025-07-14): https://learn.microsoft.com/en-us/windows/win32/winauto/entry-uiauto-win32
3. .NET UI Automation Fundamentals: https://learn.microsoft.com/en-us/dotnet/framework/ui-automation/ui-automation-fundamentals
4. WPF Automation Peers: https://learn.microsoft.com/en-us/dotnet/framework/ui-automation/ui-automation-overview
5. WCAG 2.2 Quick Reference: https://www.w3.org/WAI/WCAG22/quickref/

---

## 12. Rejected Alternatives

| Alternative | Reason |
|-------------|--------|
| Playwright for .NET | WPF 미지원 — 브라우저 전용 |
| Selenium + WinAppDriver | 유지보수 정체, WPF DataGrid/TreeView 버그 |
| Pixel-diff based UI tests | 폰트 렌더링, DPI, 테마 변화에 취약 |
| Manual test only | 회귀 발견 지연, CI 불가능 |
| MSTest integration | xUnit와 일관성 유지 (GUI-IT와 통일) |

---

## 13. Maintenance

- Owner: GUI Lane (dev/gui branch)
- First implementation: Phase 1a 진입 시 Smoke + Workflow suite
- Full coverage: Phase 1b 완료 시점
- Review: Phase milestone마다 scenario backlog 갱신

---

*Document End — XPE-GUI-E2E-001 v1.0.0*
