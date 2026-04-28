# XAML / MVVM Architecture Guide

**Document ID**: XPE-GUI-ARCH-001
**Version**: 1.1.0
**Date**: 2026-04-28
**Status**: Controlled Draft
**Canonical Scope**: `docs/project/`
**Target**: `clients/ImageProcTest/` WPF test GUI (.NET 8, C# 12, x64)
**Related Specs**: SPEC-XPE-GUI-IT v1.2.0, SPEC-XPE-P1B-DISP, XPE-GUI-MENU-001, XPE-GUI-DISP-INT-001

---

## 1. Purpose

본 문서는 `ImageProcTest.exe` WPF 애플리케이션의 **XAML / MVVM 아키텍처 헌법(Constitution)**을 정의한다. 모든 View/ViewModel/Service 추가 및 변경은 본 문서의 계약을 따른다.

적용 범위:

- XAML 레이어링 및 바인딩 패턴
- ViewModel 구성 및 상태 관리
- P/Invoke 네이티브 호출의 스레드 마샬링 규칙
- 명령(Command) 라우팅 (RoutedCommand + ICommand)
- 의존성 주입(DI) 전략
- Mock/Real 백엔드 토글

본 문서는 **구현 코드를 수정하지 않는다**. 구현은 Codex(XAML/바인딩)와 Claude(서비스/알고리즘 경계)가 본 문서 계약에 따라 수행한다.

---

## 2. Architecture Layers (4-Tier)

```
┌──────────────────────────────────────────────────────────────────┐
│ Tier 1: View (XAML)                                              │
│   MainWindow.xaml, Panels/*.xaml, Dialogs/*.xaml                 │
│   - UI 요소 선언, Binding, 스타일, 트리거                         │
│   - code-behind는 InitializeComponent + 필수 이벤트만 (mvvm-safe) │
└──────────────────┬───────────────────────────────────────────────┘
                   │ {Binding} / ICommand
┌──────────────────▼───────────────────────────────────────────────┐
│ Tier 2: ViewModel (INotifyPropertyChanged)                       │
│   MainWindowViewModel, *ViewModel                                │
│   - 화면 상태, 명령 노출, View 무관 프로퍼티                      │
│   - UI 스레드 affinity (Dispatcher.CurrentDispatcher)             │
└──────────────────┬───────────────────────────────────────────────┘
                   │ await Service.MethodAsync(...)
┌──────────────────▼───────────────────────────────────────────────┐
│ Tier 3: Service / Model (IXpeBackend, IAppSettingsService, ...)  │
│   - 도메인 로직, 데이터 변환, 세팅 지속성                         │
│   - 스레드 중립 (thread-agnostic), 비즈니스 상태 관리              │
└──────────────────┬───────────────────────────────────────────────┘
                   │ P/Invoke (Cdecl, Pack=8)
┌──────────────────▼───────────────────────────────────────────────┐
│ Tier 4: Native Interop (xpe_common.dll, xpe_preprocess.dll, ...) │
│   XpeCommonApi, XpeDisplayNative, XpePreprocessNative            │
│   - [DllImport], Marshal.PtrToStructure, 버퍼 Pinning             │
│   - 네이티브 워커 스레드 (Task.Run → 네이티브 호출)                │
└──────────────────────────────────────────────────────────────────┘
```

### 2.1 Tier 별 책임 (HARD 규칙)

- [HARD] **View**는 **비즈니스 로직을 포함하지 않는다**. code-behind는 `InitializeComponent()`, 필수 라우팅 이벤트(`Drop`, `Closing`), Control 전용 animation 트리거에 한정한다.
- [HARD] **ViewModel**은 **`System.Windows.Controls.*`, `System.Windows.Shapes.*`를 참조하지 않는다**. `ImageSource`, `BitmapSource`는 허용 (DataBinding 편의성).
- [HARD] **Service**는 **`System.Windows.*`, `INotifyPropertyChanged`를 참조하지 않는다**. POCO + async API만 제공.
- [HARD] **Native Interop**은 **`[DllImport]`, `[StructLayout(Pack=8)]`, `Marshal.*`만 포함**. 상위 레이어 참조 금지.

---

## 3. MVVM Implementation Standard

### 3.1 INotifyPropertyChanged — CommunityToolkit.Mvvm 우선

본 프로젝트는 `CommunityToolkit.Mvvm` 소스 제너레이터를 사용한다.

- Source: https://learn.microsoft.com/en-us/dotnet/communitytoolkit/mvvm/ (2024-11-07 Microsoft Learn)
- 주요 생성 속성: `[ObservableProperty]`, `[RelayCommand]`, `[NotifyCanExecuteChangedFor]`, `[NotifyPropertyChangedFor]`

**표준 ViewModel 패턴:**

```csharp
// ViewModel — 소스 제너레이터 사용 권장 패턴
public partial class MainWindowViewModel : ObservableObject
{
    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(HasImage))]
    private LoadedImageFrame? _currentFrame;

    public bool HasImage => CurrentFrame?.Preview != null;

    [RelayCommand(CanExecute = nameof(CanLoadImage))]
    private async Task LoadImageAsync(string? path)
    {
        // IXpeBackend 호출
    }

    private bool CanLoadImage(string? path) => !string.IsNullOrEmpty(path);
}
```

**수동 구현 허용 조건 (레거시/특수):**

- 소스 제너레이터가 적용되지 않는 .NET Framework 구성 요소 (해당 없음)
- `ObservableValidator`를 상속하는 검증용 ViewModel (필요 시)

### 3.2 Commanding — RoutedCommand + ICommand

**전략**: 메뉴/툴바 공유 명령은 `RoutedCommand`, ViewModel 전용 명령은 `IRelayCommand`를 사용한다.

- Source: https://learn.microsoft.com/dotnet/desktop/wpf/advanced/commanding-overview

**메뉴 ↔ 툴바 ↔ 단축키 공유 명령 (RoutedCommand):**

```xml
<!-- MainWindow.xaml.cs -->
public static readonly RoutedCommand LoadImageCommand = new(nameof(LoadImageCommand));

<!-- MainWindow.xaml -->
<Window.CommandBindings>
  <CommandBinding Command="{x:Static local:MainWindow.LoadImageCommand}"
                  Executed="OnLoadImageExecuted"
                  CanExecute="OnLoadImageCanExecute"/>
</Window.CommandBindings>
<Window.InputBindings>
  <KeyBinding Command="{x:Static local:MainWindow.LoadImageCommand}"
              Gesture="Ctrl+O"/>
</Window.InputBindings>
<MenuItem Header="Open Raw..." Command="{x:Static local:MainWindow.LoadImageCommand}"/>
<Button Command="{x:Static local:MainWindow.LoadImageCommand}">Open</Button>
```

**ViewModel 전용 명령 (RelayCommand):**

```csharp
[RelayCommand]
private void CopyToClipboard()
{
    // ViewModel 내 상태 접근, View 요소 없음
}
```

**선택 기준:**

| 조건 | Command 유형 |
|------|--------------|
| 메뉴 + 툴바 + 단축키 + 컨텍스트 메뉴 공유 | `RoutedCommand` in code-behind, bound to ViewModel via CommandBinding.Executed |
| 단일 ViewModel 내부 동작 | `[RelayCommand]` 소스 제너레이터 |
| async 작업 | `[RelayCommand]` `async Task` 메서드에 적용 (자동 `AsyncRelayCommand` 생성) |

---

## 4. Threading and P/Invoke Marshaling

### 4.1 WPF 스레드 모델

- Source: https://learn.microsoft.com/en-us/dotnet/desktop/wpf/advanced/threading-model (2025-08-27)
- WPF는 **STA 단일 UI 스레드 + Dispatcher** 모델을 사용.
- `DependencyObject`, `ImageSource`, `BitmapSource`는 **생성 스레드 affinity**를 가진다.

### 4.2 네이티브 호출 규칙 (HARD)

- [HARD] `XpeCommonApi.*`, `XpeDisplayNative.*` 등 P/Invoke 호출은 **UI 스레드에서 금지** (>100ms 작업). 반드시 `Task.Run(...)`으로 워커 스레드에 오프로드.
- [HARD] 워커 스레드에서 반환된 결과는 **`Dispatcher.InvokeAsync(DispatcherPriority.DataBind)`**으로 UI 스레드에 마샬링 후 `ObservableProperty` 대입.
- [HARD] `BitmapSource` 생성은 워커 스레드에서 수행 후 **`Freeze()` 호출**하여 cross-thread 사용을 허용한다. Freeze 없이는 UI 스레드에만 사용 가능.
- [HARD] P/Invoke 호출이 `IntPtr` 버퍼를 받는 경우 **`GCHandle.Alloc(buffer, GCHandleType.Pinned)`** 또는 `fixed` 블록으로 pinning. `finally`에서 `Free()` 보장.

### 4.3 표준 async 패턴

```csharp
public partial class MainWindowViewModel : ObservableObject
{
    [ObservableProperty] private bool _isProcessing;
    [ObservableProperty] private ImageSource? _processedImage;

    [RelayCommand(CanExecute = nameof(CanProcess))]
    private async Task ApplyDisplayPipelineAsync()
    {
        IsProcessing = true;
        try
        {
            // 1. 워커 스레드에서 네이티브 호출
            var result = await Task.Run(() =>
            {
                // P/Invoke — Dispatcher 금지
                return _backend.ApplyDisplayPipeline(_currentFrame, _settings);
            });

            // 2. BitmapSource freeze (cross-thread 안전)
            result.ProcessedPreview?.Freeze();

            // 3. UI 스레드에 마샬링 (RelayCommand는 자동 처리)
            ProcessedImage = result.ProcessedPreview;
        }
        catch (DllNotFoundException ex)
        {
            // 사용자-친화 에러 표시 (Layer 3 Error Policy 참조)
            _alertService.ShowError("xpe_display.dll을 찾을 수 없습니다.", ex);
        }
        finally
        {
            IsProcessing = false;
        }
    }

    private bool CanProcess() => !IsProcessing && _currentFrame != null;
}
```

### 4.4 INotifyPropertyChanged 스레드 안전성

- **원칙**: `PropertyChanged` 이벤트는 **이벤트 발생 스레드에서 동기적으로 호출**된다. 워커 스레드에서 발생 시 WPF는 대부분의 바인딩에 대해 자동으로 Dispatcher에 마샬링하나 **Collection 바인딩은 예외**이다.
- [HARD] `ObservableCollection<T>` 변경은 **반드시 UI 스레드에서 수행**한다. 필요 시 `BindingOperations.EnableCollectionSynchronization(coll, _lock)` 사용.

---

## 5. State Management and Backend Toggle

### 5.1 IXpeBackend Composite Pattern

현재 구현된 계층:

- `IXpeBackend` — 추상 인터페이스
- `MockXpeBackend` — 합성 백엔드 (네이티브 DLL 불필요, 개발/E2E 모드)
- `RealXpeBackend` — P/Invoke 백엔드 (xpe_common.dll 필수)
- `CompositeXpeBackend` — Mock + Real 전환 어댑터

### 5.2 토글 규칙 (HARD)

- [HARD] **통합 테스트(SPEC-XPE-GUI-IT)**는 `MockXpeBackend`를 사용하면 실패 (REQ-GUI-IT-007)
- [HARD] **개발 런타임**은 DLL 부재 시 자동 fallback to Mock, 단 Runtime Panel에 "**Mock mode (DLL not found)**" 경고 표시
- [HARD] **E2E 테스트 (FlaUI)**는 `MOAI_XPE_BACKEND_MODE=Mock` 환경변수로 강제 Mock 모드

### 5.3 설정 페시스턴스

- `IAppSettingsService` → `appsettings.json` (per-user roaming AppData 또는 프로젝트 root)
- [HARD] 설정 저장은 `Debounce(500ms)` 적용 — 연속 변경 시 I/O 스팸 방지
- [HARD] 민감 데이터(경로, 임상 식별자) 저장 금지

---

## 6. Dependency Injection Strategy

### 6.1 채택 방침

본 프로젝트는 **Constructor Injection + Manual Composition Root** 전략을 사용한다. 외부 DI 컨테이너(Microsoft.Extensions.DependencyInjection 등) 도입 여부는 **Phase 1b 이후 재검토**.

**Composition Root**: `App.xaml.cs` 의 `OnStartup`

```csharp
public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);

        // 1. Services (singletons)
        var settingsService = new AppSettingsService();
        var helpBundle = new HelpBundleService();
        var alertService = new AlertService();

        // 2. Backend selection
        IXpeBackend backend = XpeBackendFactory.Create(
            settingsService.Current.BackendMode);

        // 3. ViewModel
        var mainVm = new MainWindowViewModel(
            backend, settingsService, helpBundle, alertService);

        // 4. View
        var mainWindow = new MainWindow { DataContext = mainVm };
        mainWindow.Show();
    }
}
```

### 6.2 금지 사항

- [HARD] **ServiceLocator 패턴 금지** (`CommonServiceLocator`, 전역 static 주입 방지)
- [HARD] **ViewModel → View 참조 금지** (`Application.Current.MainWindow` 접근 금지)
- [HARD] ViewModel 내부 `new` 생성자 호출은 DTO/POCO에 한정 — 서비스는 생성자 주입

---

## 7. XAML Styling and ResourceDictionary

### 7.1 테마 전략

- **Default theme**: 라이트 (병원 워크스테이션 표준)
- **Dark theme**: Phase 3 이후 선택적
- **Accent color**: WPF `SystemColors` 준수, 커스텀 팔레트는 `Themes/*.xaml`

### 7.2 Resource 조직

```
clients/ImageProcTest/
├── Themes/
│   ├── Light.xaml              (기본 ResourceDictionary)
│   ├── Colors.xaml             (팔레트, ICC 호환)
│   ├── Typography.xaml         (폰트, 임상 가독성 기준)
│   └── Icons.xaml              (Path Geometry, vector only)
├── Styles/
│   ├── ButtonStyles.xaml
│   ├── MenuStyles.xaml
│   └── ImageViewerStyles.xaml
└── App.xaml (MergedDictionaries)
```

### 7.3 Pack URI 규칙

- Source: https://learn.microsoft.com/en-us/dotnet/desktop/wpf/app-development/pack-uris-in-wpf (2025-08-27)

- [HARD] Resource 참조는 **`pack://application:,,,/Themes/Light.xaml`** 절대 URI 사용
- [HARD] 어셈블리 간 참조는 **`pack://application:,,,/AssemblyName;component/...`** 사용
- [HARD] Site-of-origin 리소스 금지 (런타임 파일 시스템 의존성 방지)

---

## 8. Error Handling and User Feedback

### 8.1 에러 분류 (3 Tier)

| Severity | UX 패턴 | 예시 |
|----------|---------|------|
| **Fatal** | 모달 다이얼로그 + 애플리케이션 종료 | DLL load failure, OOM |
| **Error** | Inline alert panel + 재시도 버튼 | P/Invoke `XPE_ERR_*` 반환 |
| **Warning** | Toast notification (3s auto-dismiss) | Calibration file mismatch |
| **Info** | Status bar message | Backend mode transition |

### 8.2 예외 마샬링 규칙 (HARD)

- [HARD] `AccessViolationException`, `SEHException`은 **ViewModel로 전파 금지**. Native Interop 레이어에서 catch → `XpeErrorCode.InternalError` 또는 `XpeBoundaryException`(custom)로 재포장
- [HARD] 모든 `Task.Run` 호출은 **`try/catch`에 감싸져야 한다** — unobserved exception은 프로세스 종료 위험

---

## 9. Testing Hooks

### 9.1 AutomationProperties (UI Automation / FlaUI)

- [HARD] **모든 interactive UI 요소**는 `AutomationProperties.Name`, `AutomationProperties.AutomationId`를 선언한다
- AutomationId 명명 규칙: `XPE_{Area}_{Action}_{Kind}` (예: `XPE_Main_LoadImage_Button`)
- XPE-GUI-ACCESS-001 § 3 Accessibility 문서 참조

### 9.2 Headless 자동화 모드

- `ImageProcTest.exe --automation` 플래그로 실행 시 **자동 스크립트 모드** 진입
- 인자로 YAML 시나리오 파일 지정 가능 (`--scenario smoke.yaml`)
- 결과는 `gui-e2e-reports/YYYYMMDD-HHmmss/` 디렉토리에 TRX + 스크린샷 저장

### 9.3 ViewModel Testability

- [HARD] 모든 ViewModel은 **parameterless constructor가 없어야 한다** (의존성 명시화)
- [HARD] ViewModel 단위 테스트는 xUnit `ImageProcTest.UnitTests` (계획) 프로젝트에서 수행. WPF runtime (Application, Dispatcher) 의존 최소화를 위해 ViewModel은 `SynchronizationContext`-agnostic으로 작성

---

## 10. File Organization

```
clients/ImageProcTest/
├── App.xaml / App.xaml.cs                    (Composition Root)
├── MainWindow.xaml / .xaml.cs                (Shell)
├── ViewModels/
│   ├── MainWindowViewModel.cs
│   ├── Display/
│   │   ├── DisplayPipelineViewModel.cs       (Phase 1b)
│   │   ├── VoiLutViewModel.cs
│   │   └── ModalityLutViewModel.cs
│   ├── Preprocess/
│   │   └── PreprocessPipelineViewModel.cs
│   └── Diagnostics/
│       ├── RuntimeInfoViewModel.cs
│       ├── AlertsPanelViewModel.cs
│       └── LogsPanelViewModel.cs
├── Views/Panels/
│   ├── DisplaySettingsPanel.xaml
│   ├── PreprocessSettingsPanel.xaml
│   └── DiagnosticsPanel.xaml
├── Services/
│   ├── IXpeBackend.cs
│   ├── MockXpeBackend.cs
│   ├── RealXpeBackend.cs                     (Phase 1b)
│   ├── CompositeXpeBackend.cs
│   ├── XpeBackendFactory.cs
│   ├── AppSettingsService.cs
│   ├── HelpBundleService.cs
│   ├── AlertService.cs
│   ├── Native/
│   │   ├── XpeCommonApi.cs                   (기존)
│   │   ├── XpeDisplayNative.cs               (Phase 1b NEW)
│   │   └── XpePreprocessNative.cs            (Phase 1a NEW)
│   └── Diagnostics/
│       ├── XpePreprocessSyntheticOracle.cs
│       └── DllStagingResolver.cs
├── Models/
│   ├── AppSettings.cs
│   ├── LoadedImageFrame.cs
│   ├── BackendRuntimeInfo.cs
│   └── VoiPreset.cs                          (Phase 1b NEW)
├── Themes/ / Styles/
└── Resources/                                (RESX for Localization)
```

---

## 11. Rejected Alternatives

| Option | Decision | Reason |
|--------|----------|--------|
| Prism / MVVMLight | Rejected | Overkill for test GUI; CommunityToolkit.Mvvm covers 95%+ needs |
| ReactiveUI | Rejected | Learning curve 부담, 네이티브 호출과 Observable 결합 복잡도 |
| Code-behind only (no MVVM) | Rejected | 테스트 불가, UI automation 어려움, 장기 유지보수성 |
| MAUI / Avalonia | Rejected | 프로젝트는 Windows WPF 전용 (Flat Panel Detector 드라이버 호환성) |
| Microsoft.Extensions.DependencyInjection | Deferred to Phase 2 | 현 규모에서 Composition Root 수동 구성으로 충분 |

---

## 12. Sources (Verified 2026-04-18)

1. Microsoft Learn, CommunityToolkit.Mvvm (2024-11-07): https://learn.microsoft.com/en-us/dotnet/communitytoolkit/mvvm/
2. Microsoft Learn, WPF Threading Model (2025-08-27): https://learn.microsoft.com/en-us/dotnet/desktop/wpf/advanced/threading-model
3. Microsoft Learn, Commanding Overview: https://learn.microsoft.com/dotnet/desktop/wpf/advanced/commanding-overview
4. Microsoft Learn, Pack URIs in WPF (2025-08-27): https://learn.microsoft.com/en-us/dotnet/desktop/wpf/app-development/pack-uris-in-wpf
5. Microsoft Learn, WPF Menu Overview: https://learn.microsoft.com/en-us/dotnet/desktop/wpf/controls/menu-overview
6. Microsoft Learn, Windows Commanding (2025): https://learn.microsoft.com/en-us/windows/apps/develop/ui/controls/commanding

---

## 13. Maintenance

- Owner: GUI Lane (dev/gui branch)
- Review cadence: Phase milestone (Phase 1a, 1b, 2, 3)
- Change log: HISTORY 테이블 업데이트, SPEC-XPE-GUI-IT 의 Referenced Documents 및 XPE-GUI-NATIVE-INT-READINESS-001 cross-ref 동기화

---

## 14. Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0 | 2026-04-18 | manager-spec (GUI Lane) | Initial MVVM architecture constitution |
| 1.1.0 | 2026-04-28 | manager-docs (GUI Lane) | Phase 1b 구현 완료 반영, DisplayPipelineViewModel 추가, 네이티브 인터롭 계약 확장 |

---

*Document End — XPE-GUI-ARCH-001 v1.1.0*
