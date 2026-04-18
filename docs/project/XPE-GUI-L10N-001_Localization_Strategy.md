# GUI Localization Strategy

**Document ID**: XPE-GUI-L10N-001
**Version**: 1.0.0
**Date**: 2026-04-18
**Status**: Controlled Draft
**Canonical Scope**: `docs/project/`
**Target**: `clients/ImageProcTest/` and future production clinical GUI
**Primary Locale**: ko-KR (Korean)
**Fallback Locale**: en-US (English)

---

## 1. Purpose

본 문서는 `ImageProcTest.exe` 및 향후 production GUI의 **다국어(L10N) 전략**을 정의한다. 현재 테스트 GUI는 영문 하드코딩 상태이나, 임상 사용자를 위한 production GUI는 **한국어 primary / 영문 fallback** 구조를 따른다.

---

## 2. Scope

### 2.1 In Scope

- XAML 및 code-behind의 UI 문자열 외부화
- RESX 리소스 파일 기반 satellite assembly
- CultureInfo 관리 및 런타임 언어 전환
- Date / number / currency 형식
- 아이콘 / 이미지 locale 정책
- 번역 워크플로우

### 2.2 Out of Scope

- Help HTML 번들 (XPE-GUI-ACCESS-001 §8에서 다룸)
- 알고리즘 로그 메시지 (항상 영문 — 엔지니어링 도구)
- IEC 62304 / IEC 62366 normative docs (한영 이중 버전 권장, 별도 문서)
- Codex 주도 XAML 수정 (번역 작업은 Codex suitable)

---

## 3. Target Locales

| Locale | Primary? | UI Text | Date Format | Number Format | Notes |
|--------|:--------:|---------|-------------|---------------|-------|
| **ko-KR** | **Yes** | 한국어 | `yyyy-MM-dd HH:mm` | `1,234.56` | Primary clinical users |
| **en-US** | Fallback | English | `yyyy-MM-dd HH:mm` | `1,234.56` | Developer tools, engineering logs |
| (Future) ja-JP | No | 日本語 | `yyyy年MM月dd日` | `1,234.56` | Export 대비 |
| (Future) zh-CN | No | 中文 | `yyyy-MM-dd` | `1,234.56` | Export 대비 |

[HARD] Fallback 정책: 현재 locale에 번역 키 없으면 en-US 값 사용, 그래도 없으면 AutomationId 또는 리소스 키 이름 표시 (버그 탐지 용이).

---

## 4. Resource File Strategy (RESX)

### 4.1 디렉토리 구조

```
clients/ImageProcTest/
├── Resources/
│   ├── Strings.resx              (en-US default, en=invariant)
│   ├── Strings.ko-KR.resx        (Korean translation)
│   ├── Strings.ja-JP.resx        (future)
│   ├── Strings.zh-CN.resx        (future)
│   ├── Errors.resx               (에러 메시지 분리)
│   ├── Errors.ko-KR.resx
│   ├── Menu.resx                 (메뉴 헤더 전용)
│   ├── Menu.ko-KR.resx
│   └── Designer.cs               (자동 생성 — do not edit)
└── Properties/
    └── AssemblyInfo.cs
        // [assembly: NeutralResourcesLanguage("en-US", UltimateResourceFallbackLocation.Satellite)]
```

Source: https://learn.microsoft.com/en-us/dotnet/desktop/wpf/advanced/walkthrough-localizing-a-hybrid-application

### 4.2 RESX Key 명명 규칙

형식: `{Area}_{Kind}` — snake case 유지 불필요, PascalCase 권장

| Key | Value (en-US) | Value (ko-KR) |
|-----|---------------|---------------|
| `Menu_File` | `_File` | `_파일` |
| `Menu_File_OpenRaw` | `_Open Raw...` | `RAW 열기(_O)...` |
| `Menu_Pipeline` | `_Pipeline` | `_파이프라인` |
| `Menu_Pipeline_Run` | `_Run Preprocessing` | `전처리 실행(_R)` |
| `Toolbar_Load` | `Load` | `불러오기` |
| `Toolbar_Run` | `Run` | `실행` |
| `Alert_DllMissing_Title` | `Native DLL Not Found` | `네이티브 DLL 없음` |
| `Alert_DllMissing_Body` | `xpe_common.dll could not be located.` | `xpe_common.dll을 찾을 수 없습니다.` |

[HARD] Key에 **locale-specific text 포함 금지** — `Button_OK` (O), `Button_확인` (X)

### 4.3 Mnemonic 처리

- 영문: `_Open` → Alt+O
- 한국어: `열기(_O)` 또는 `열기(_O)...` — 한글 자체에는 mnemonic이 불가능하므로 괄호 내 영문자 사용

---

## 5. XAML Binding to Resources

### 5.1 권장 패턴 (x:Static)

```xml
<Window xmlns:res="clr-namespace:ImageProcTest.Resources">
  <Menu>
    <MenuItem Header="{x:Static res:Strings.Menu_File}">
      <MenuItem Header="{x:Static res:Strings.Menu_File_OpenRaw}"
                Command="{x:Static local:MainWindow.LoadImageCommand}"/>
    </MenuItem>
  </Menu>
</Window>
```

### 5.2 Dynamic 전환용 Markup Extension

`x:Static`은 컴파일 타임 바인딩이어서 런타임 언어 전환이 어렵다. 전환이 필요한 경우 **커스텀 Markup Extension** 또는 **WPFLocalizeExtension** 도입 고려:

```xml
<MenuItem Header="{lex:Loc Menu_File}"/>
```

런타임 전환 필요 없으면 x:Static 단순 패턴 유지.

---

## 6. CultureInfo Management

### 6.1 Startup 전략

```csharp
// App.xaml.cs OnStartup
protected override void OnStartup(StartupEventArgs e)
{
    // 1. 설정에서 선호 locale 읽기
    var preferred = _settingsService.Current.Locale ?? "ko-KR";

    // 2. 시스템 locale fallback
    var culture = CultureInfo.GetCultureInfo(preferred);
    Thread.CurrentThread.CurrentCulture = culture;
    Thread.CurrentThread.CurrentUICulture = culture;
    CultureInfo.DefaultThreadCurrentCulture = culture;
    CultureInfo.DefaultThreadCurrentUICulture = culture;

    // 3. WPF FlowDirection (RTL locales; 현재는 LTR 전용)
    FrameworkElement.LanguageProperty.OverrideMetadata(
        typeof(FrameworkElement),
        new FrameworkPropertyMetadata(
            XmlLanguage.GetLanguage(culture.IetfLanguageTag)));

    base.OnStartup(e);
}
```

### 6.2 Runtime Switching

- Phase 2+ feature — 설정 변경 시 애플리케이션 재시작 필요 (간단한 구현)
- 재시작 없이 전환 필요 시 `INotifyPropertyChanged` 기반 `LocalizationManager` 싱글턴 구현 (DynamicResource 활용)

---

## 7. Date / Number / Time Formatting

### 7.1 원칙

- [HARD] **UI 표시**: 사용자 locale
- [HARD] **로그 파일 / 파일명 / 교환 포맷**: ISO 8601 (`yyyy-MM-ddTHH:mm:ssZ`) — locale-independent
- [HARD] **알고리즘 내부 / 네이티브 interop**: `CultureInfo.InvariantCulture`

### 7.2 예시

```csharp
// UI 표시
var display = timestamp.ToString("g", CultureInfo.CurrentCulture);
// ko-KR: "2026. 4. 18. 오후 3:42"
// en-US: "4/18/2026 3:42 PM"

// 파일명
var filename = $"report_{DateTime.UtcNow:yyyyMMdd_HHmmss}.trx";
// 항상 "report_20260418_154230.trx"

// 네이티브 JSON 설정
var config = JsonSerializer.Serialize(settings,
    new JsonSerializerOptions { /* culture-independent */ });
```

### 7.3 의료 영상 메타데이터

- DICOM 태그 값: 원본 유지 (locale 변환 금지)
- 표시용: locale 형식으로 변환 (예: 환자 이름 렌더링)
- [HARD] **환자 식별 정보(PHI)는 본 GUI에 표시 금지** (테스트 GUI 원칙)

---

## 8. Image / Icon Localization Policy

### 8.1 원칙

- [HARD] **아이콘은 locale-independent** — 벡터(Path Geometry) 또는 locale-neutral PNG 사용
- [HARD] 텍스트가 포함된 이미지는 **생성 금지** — XAML TextBlock + 아이콘 조합으로 대체

### 8.2 예외 처리

Flag 아이콘 등 국가 상징이 필요한 경우:

```
clients/ImageProcTest/Resources/Images/
├── flag-default.png       (fallback)
├── ko-KR/flag-korea.png   (culture-specific)
└── en-US/flag-usa.png
```

Pack URI 로 locale-aware loading:

```csharp
var uri = new Uri($"pack://application:,,,/Resources/Images/{culture.Name}/flag.png");
```

---

## 9. Translation Workflow

### 9.1 담당 분배

| 작업 | Owner | 도구 |
|------|-------|------|
| Key 추가 / Strings.resx 편집 | 개발자 (Claude) | Visual Studio RESX editor |
| 번역 작업 | Translator 또는 Codex 제안 | RESX editor, CSV export |
| Review / Approval | UX + QA | - |
| Commit | 개발자 | Git |

### 9.2 Codex 활용

Codex는 XAML에서 하드코딩 문자열 추출 및 RESX key 생성 작업에 적합:

```
codex "MainWindow.xaml의 하드코딩된 영문 문자열을
      Strings.resx 키로 추출하고 x:Static 바인딩으로 교체해줘"
```

[HARD] Codex 번역 결과는 **반드시 Claude + 의료 도메인 reviewer 검토** 필수.

### 9.3 번역 품질 기준

- 의료 용어: KS 표준 또는 대한영상의학회 권고 용어 준수
- Mnemonic 유지 (`_파일` 형식)
- 줄바꿈 유지 (XAML `\r\n` 또는 `&#10;`)
- Plurals: .NET은 PluralRules 미지원, 번역자가 수동 처리 (예: "1 file" / "2 files" → "파일 1개" / "파일 2개")

---

## 10. Testing

### 10.1 Automated

- FlaUI E2E 테스트는 **AutomationId 기반** — locale 무관 (XPE-GUI-E2E-001 §4.4 A-01)
- 리소스 키 누락 감지: 빌드 warning 또는 단위 테스트로 `ResourceManager.GetString(key, ko)` != null 확인

### 10.2 Manual

- 한국어 locale로 실행 후 모든 주요 메뉴/버튼/알림 확인
- 한글 truncation 확인 (영문 대비 텍스트 길이 변화)
- 포맷 문자열 확인 (날짜, 숫자)

### 10.3 Pseudo-Localization

개발 중 번역 누락 감지를 위한 `qps-ploc` 의사 번역:

- 모든 문자열을 `[!!!текст!!!]`으로 감싸 표시
- 번역 누락된 키는 영문 그대로 → 개발자에게 즉시 가시

---

## 11. CI/CD Integration

### 11.1 빌드 파이프라인

- RESX satellite assembly 자동 생성 (`dotnet build`가 기본 처리)
- 출력 구조:
  ```
  bin/Debug/net8.0/
  ├── ImageProcTest.exe
  ├── ImageProcTest.dll
  ├── ko-KR/
  │   └── ImageProcTest.resources.dll
  └── (en-US는 기본 어셈블리에 포함)
  ```

### 11.2 품질 게이트

- [HARD] **신규 UI 문자열은 en-US + ko-KR 동시 추가** 필수 (PR 차단 규칙)
- 번역 누락 검출: pre-commit hook에서 RESX 비교 스크립트 실행

---

## 12. Accessibility Interaction (XPE-GUI-ACCESS-001 Cross-ref)

- [HARD] `AutomationProperties.Name`도 locale-specific 문자열로 **동일 RESX 소스** 사용
- Screen reader는 CurrentUICulture 기반 발화 — locale 변경 시 즉시 반영

```xml
<Button AutomationProperties.Name="{x:Static res:Strings.Toolbar_Load_Accessible}"/>
```

---

## 13. Rejected Alternatives

| Option | Reason |
|--------|--------|
| gettext (.po) | .NET 생태계에 RESX가 표준 |
| JSON-based i18n | 타입 안전성 약함, designer.cs 자동 생성 없음 |
| WPFLocalizeExtension (phase 1) | Phase 1 복잡도 과다 — Phase 2 이후 필요 시 도입 |
| Runtime-only 영문 고정 | 임상 사용자 대상 한국어 필수 |
| XAML의 `x:Uid` + LocBaml | 워크플로우 복잡, RESX 대비 이점 없음 |

---

## 14. Sources (Verified 2026-04-18)

1. Microsoft Learn, WPF Localization Walkthrough: https://learn.microsoft.com/en-us/dotnet/desktop/wpf/advanced/walkthrough-localizing-a-hybrid-application
2. Microsoft Learn, Pack URIs in WPF (locale-aware resource loading): https://learn.microsoft.com/en-us/dotnet/desktop/wpf/app-development/pack-uris-in-wpf
3. Microsoft Learn, CultureInfo Class: https://learn.microsoft.com/en-us/dotnet/api/system.globalization.cultureinfo

---

## 15. Maintenance

- Owner: GUI Lane (dev/gui branch) + Translator
- Initial implementation: Phase 1b 진입 시 en-US + ko-KR baseline
- Coordination: XPE-GUI-ACCESS-001, XPE-GUI-E2E-001와 항상 동기화
- Future locale 추가: ja-JP, zh-CN은 export 요청 시 활성화

---

*Document End — XPE-GUI-L10N-001 v1.0.0*
