# GUI-C-76 — 토큰 `:170` · README 정리 · 낡은 문서 4건 대조 (#170 #165 #163)

- 카드: GUI-C-76 · Refs #170 #165 #166 #163 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건 — 미푸시 (파일 2개: `WindowReacquireTests.cs`, `gui/ImageProcTest/README.md`)
- **토큰 `:170` — 반증 포함 확인**(§1). **README 는 없는 것 3건을 지우고 기록을 남겼습니다**(§2).
- **README 를 보다가 나온 것**: 문서가 돌리라고 안내하는 **SelfCheck 와 옛 E2E 러너가 지금 실패합니다**(§2.3).
- **문서 4건 대조: 17건 + 해석 필요 1건**(§4). **그중 하나는 위해 분석의 위험 통제입니다**(§4.2).
- `#163` 에 사실 코멘트 1건(§5).
- **결과: Mock 0/71/1/72 · 통합 0/200/1/201 · slnx 0경고 0오류**

---

## 1. 토큰 — `XPE-SKIP-ALLOWED:170`

C-75 와 같은 방식으로, **main `3c89919` 의 `ci.yml` 에서 게이트 스크립트를 그대로 뽑아**(501–553행,
trx 경로만 교체) 로컬에서 돌렸습니다. 건너뛰기는 픽스처를 잠시 `simulateUnreadableChecks: 1` 로
무장해 결정적으로 만들었고 원복했습니다.

| 토큰 | 게이트 출력 | exit |
|---|---|---|
| `:170` | `SKIPPED (accounted, #170): …ReadableWindow… - window re-acquisition PropertyNotSupportedException, ~7% per launch (UIA code 30011, not an issue number)` | **0** |
| `:99999` | `NOT RUN (unexplained): …` + `1 E2E case(s) did not run … with no accounted reason` | **1** |

**em dash 도 이제 제대로 나옵니다** — 리더가 넣은 `[Console]::OutputEncoding = UTF8` 줄 이후
`… fired on this launch — The launched window …` 로 찍혔습니다(C-75 에서는 `?`).

## 2. `gui/ImageProcTest/README.md`

### 2.1 지운 것 — 그리고 지웠다는 기록

| 주장 | 실제 | 근거 |
|---|---|---|
| "log panel, **alert panel**" | Alerts 패널 없음. 알림은 수집·`Clear Alerts` 는 되지만 **화면 어디에도 표시되지 않음** | `Alerts` 를 바인딩하는 XAML **0건**, `MainWindow.xaml:148` 제거 기록 |
| "**resizable diagnostics layout** for Logs and Alerts" | 없음 | `GridSplitter` **0건**(gui XAML 전체) |
| "reports the DLL detection state in the **Runtime panel**" | Runtime 패널 없음. 감지 상태는 **Backend → Native Diagnostics** 가 상태 표시줄·로그로 알림 | `ShowNativeDiagnostics()` (`MainWindowViewModel.cs`), `RuntimeInfo` 를 바인딩하는 XAML 0건 |

README 에 **"Removed from the list above (GUI-C-76, #165)"** 절을 두어 **무엇을 왜 지웠는지** 한 줄씩
남겼습니다.

**덤으로 드러난 것**: 자동화 보고서의 `ResizableDiagnosticsLayoutDetected` 는
**`MainWindow.xaml.cs:265` 에서 무조건 `true`** 입니다. 없는 레이아웃을 "감지했다" 고 보고하고,
`Passed` 판정(376행)이 그 값에 기댑니다. **C-66/C-67 census 가 문자열만 봐서 놓친 같은 종류의
거짓**입니다 — **불리언 필드**라서요. **고치지 않았습니다**(카드 범위 밖, 제 소유) — README 에는
사실만 적었습니다.

### 2.2 §3.5.5 에 맞춘 것 — 얼린 값·목록을 "어디를 보면 되는지" 로

| 전 | 후 |
|---|---|
| VOI 기본값 `C=32768, W=65535, intercept=0` 산문 | `Models/AppSettings.cs` 를 가리킴 |
| 프리셋 값 4쌍 산문 | Mock: `Services/MockXpeBackend.cs` · Native: `xpe_voi_preset_create` |
| `appsettings.json` 키 **29개** 목록 | `Models/AppSettings.cs` 의 `[JsonPropertyName]` 을 가리킴 — **실제는 38개였습니다.** 목록은 지우고 지운 이유를 적었습니다 |
| View 메뉴 "panel visibility and layout reset" | 현재 항목의 종류를 적고 `MainWindow.xaml` 을 가리킴 |
| detached viewer | "#166 이후 열림" + 그것을 재는 시나리오 두 파일을 가리킴 |

### 2.3 README 가 돌리라고 안내하는 도구 둘이 **지금 실패합니다**

**실제로 돌렸습니다.**

```
dotnet run --project gui/ImageProcTest.SelfCheck/…  → exit≠0
  Unhandled exception. System.InvalidOperationException: VOI window center should default to Abdomen preset.
     at … gui/ImageProcTest.SelfCheck/Program.cs:line 58

gui/ImageProcTest.E2E/bin/Debug/net8.0-windows/ImageProcTest.E2E.exe  → exit≠0
  (같은 예외 — 러너가 SelfCheck 를 먼저 돌림, Program.cs:15 → :38)
```

- SelfCheck 는 **옛 CT 값(40/400)** 을 단언합니다. **픽스처 템플릿 `appsettings.template.json` 도
  40/400** 이고, **앱 기본값(`AppSettings.cs`)은 32768/65535** 입니다. README 옛 문구는 SelfCheck 가
  32768 을 검증한다고 적었습니다 — **셋이 서로 다릅니다.**
- 옛 E2E 러너는 `ShowRuntimePanelMenuItem`(#165 에서 제거)도 찾습니다(`Program.cs:147`). **그
  단계까지 가지 못해** 그것이 다음 실패인지는 **재지 못했습니다.**
- **둘 다 CI 에서 돌지 않습니다**(`ci.yml` 에 `SelfCheck`·`ImageProcTest.E2E` 0건).
- README 두 절에 **"Status (measured GUI-C-76): currently fails"** 를 붙이고, 목록은 "무엇을
  검사하려고 썼는가" 로 바꿨습니다. **도구는 고치지 않았습니다**(범위 밖, 제 소유).

### 2.4 `clients/README.md` — **틀린 것 0건**

네 주장을 코드에 대조했습니다.

| 주장 | 근거 |
|---|---|
| 기본 탐색은 앱 폴더 + `XPE_NATIVE_DIR` 두 곳 | `NativeModuleLibraryLocator.cs:15,17`, 개발 탐색은 `:31` 에서 차단 |
| 형제 체크아웃은 `XPE_NATIVE_DEV_SEARCH=1` 에서만 | `NativeSearchPolicy.cs:28`, **통합 테스트 `ByDefault_NoBuildDirectoryOrSiblingCheckoutIsOffered` 가 고정**(200 통과에 포함) |
| `ModuleReadinessSnapshot.ResolvedDllPath` 와 `native modules: …` 한 줄 | `ModuleReadinessSnapshot.cs:12,64` |
| `XPE_NATIVE_DIR_EXCLUSIVE=1` 은 테스트 격리용 | `NativeSearchPolicy.cs:25`, `ApplicationFixture.cs:134` |

**관측 하나**: 이 저장소에는 **WPF 앱 프로젝트가 둘** 있습니다 — `clients/ImageProcTest`(네이티브
진단·백엔드)와 `gui/ImageProcTest`(E2E 가 띄우는 UI). `clients/README.md` 의 제목 "WPF app" 은
전자를 말하고 틀리지 않지만, **두 앱이 있다는 사실은 어느 README 에도 없습니다.** 적지 않았습니다.

## 3. 대조 방법 — 무엇을 보고 무엇을 못 봤는가

1. main 의 네 문서를 뽑아(`git show origin/main:…`) **백틱으로 감싼 식별자·파일명**을 모두 추출
2. `gui/`·`clients/` 의 `*.cs *.xaml *.json *.csproj *.html *.slnx` 에서 **존재 여부** 검색
3. **없는 것마다 문서의 그 줄을 읽어** 현재형 주장인지, 계획·권고·금지·예시인지 갈랐습니다.
   **계획·권고·금지는 세지 않았습니다.**
4. 현재형 주장이면 코드에서 실제 모습을 찾아 "없음 / 다름" 으로 분류

**이 방법의 사거리**: 백틱 없는 **산문 속 동작 주장**은 대부분 잡히지 않습니다. 네 문서
~1,900줄을 줄마다 읽지 않았습니다. **0건이 아닌 곳에서 멈춘 것이지, 나머지가 맞다는 뜻이 아닙니다.**

## 4. 문서 4건 — **17건 + 해석 필요 1건**

### 4.1 ARCH-001 (04-28) — **9건**

| § | 문서의 현재형 주장 | 실제 | 분류 |
|---|---|---|---|
| 3.1 | "본 프로젝트는 `CommunityToolkit.Mvvm` 소스 제너레이터를 **사용한다**" | csproj 참조 **0건**. 손으로 쓴 `ObservableObject`·`RelayCommand` | 다름 |
| 3.2 | 메뉴/툴바 공유 명령은 `RoutedCommand` | 사용 **0건**. 같은 `ICommand` 인스턴스를 공유(`ToolbarMenuCommandParity` 가 `ReferenceEquals` 로 확인) | 다름 |
| 2 | Tier 1: `Panels/*.xaml, Dialogs/*.xaml` | `Views/*.xaml`. 두 폴더 없음 | 다름 |
| 2, 5 | `IAppSettingsService` | 인터페이스 없음. 구체 클래스 `AppSettingsService` | 없음 |
| 6 | `Themes/*.xaml` + [HARD] `pack://…/Themes/Light.xaml` 절대 URI | `Themes/` 없음 | 없음 |
| 8 | AutomationId 규칙 `XPE_{Area}_{Action}_{Kind}` | `XPE_` id **0건**. **E2E-001 §4.1 이 "채택하지 않는다" 로 이미 정정**(C-29) — **문서끼리 모순** | 다름 |
| 8.2 | `XpeBoundaryException` 으로 변환 | 없음(`XpeErrorCode` 는 있음) | 없음 |
| 4.3 | [HARD] `Dispatcher.InvokeAsync(DispatcherPriority.DataBind)` | `InvokeAsync` **0건**, `Dispatcher.Invoke/BeginInvoke` 2건. **마샬링이 달리 올바른지는 재지 않았습니다** | 다름 |
| 머리 | Target: `clients/ImageProcTest/` | E2E 가 띄우는 XAML UI 는 `gui/ImageProcTest`(XAML 9 vs 4). **앱이 둘이라 판정은 리더 몫** | 다름 |

세지 않은 것: `ImageProcTest.UnitTests` 는 "(계획)" 표기 · `CommonServiceLocator` 는 **금지** 조항.

### 4.2 DISP-INT-001 (04-19) — **4건**, 그중 첫 줄이 위해 통제입니다

§18–21 은 스스로 **"구현해야 할 계약"** 이라고 적으므로 미구현 자체는 세지 않았습니다. 다만:

| § | 주장 | 실제 | 분류 |
|---|---|---|---|
| **19** | **[HARD] `IsPreviewStale` + 경고 오버레이 — "HAZ-GUI-004 control"** | **코드 전체에 0건**(`stale` 대소문자 무시) | **없음** |
| 21 | "E2E 테스트 **W-06 이** 이 필드(`StageTimings`)를 **검증**" — 현재형 | **W-06 은 스위트에 없습니다.** `StageTimings` 도 없음(`Stopwatch` 는 `PipelineOrchestrator` 에 있음) | 없음 |
| 18 | 계약 파일 `clients/ImageProcTest/Services/Native/DisplayNativeWrapper.cs` | 없음. 디스플레이 interop 은 `gui/ImageProcTest/Services/Native/XpeDisplayInterop.cs` + `RealXpeBackend` | 다름 |
| 19 | `ViewModels/Display/DisplayPipelineViewModel.cs` | 없음. 파이프라인은 `MainWindowViewModel.ApplyDisplayPipelineAsync` | 다름 |

**첫 줄이 이 카드에서 가장 무겁습니다.** 이 통제는 다른 두 문서가 기대고 있습니다 —
**범위 밖이지만 인용된 줄만 읽었습니다**:

```
SHA-GUI-001:96   HAZ-GUI-004: Stale Preview — 오래된 데이터 표시
         Harm: Misinterpretation (Serious 가능) · Initial Risk: Medium
         Risk Control (Mitigation): (1) ViewModel에서 IsPreviewStale 플래그 + overlay indicator,
                                    (2) ImageSource.Freeze(), (3) 이전 결과 clear 후 새 계산
         Residual Risk: Low (with visual indicator of stale state)
SHA-GUI-001:252  HAZ-GUI-004 | W-06, W-07 (Display pipeline refresh)
RTM-GUI-001:178  HAZ-GUI-004 (Stale preview) | ARCH-001 §4.3, DISP-INT-001 §4.3 | E2E W-06/W-07
```

- **(1) 표시기: 코드에 없습니다.** **잔여 위험 "Low" 는 그 표시기를 조건으로 적혀 있습니다.**
- **(2) `Freeze()`: 5건 있습니다**(어느 객체에 쓰였는지는 안 봤습니다).
- **(3) "이전 결과 clear 후 새 계산"**: `MainWindowViewModel.cs:866` 이
  `ProcessedImage = result.ProcessedPreview ?? ProcessedImage;` — **새 결과가 없으면 이전 것을
  유지하는** 모양입니다. **읽은 것이고 실행으로 재지 않았습니다.**
- **추적된 검증 W-06 은 없고, W-07 은 "바디파트를 고르면 프리셋이 VOI 에 적용된다" 를 봅니다** —
  갱신 지연·오래된 미리보기를 보지 않습니다.

**IEC 62304 Class B 의 위해 분석 표가 존재하지 않는 통제와 존재하지 않는 테스트를 가리키고
있습니다.** 판정은 리더 몫입니다.

### 4.3 L10N-001 (04-19) — **1건 + 해석 필요 1건**

문서가 **"현재 테스트 GUI는 영문 하드코딩 상태"** 라고 스스로 적으므로 resx 구조·키는 세지 않았습니다.

| § | 주장 | 실제 | 분류 |
|---|---|---|---|
| 머리 | Target: `clients/ImageProcTest/` | ARCH-001 과 같음 | 다름 |
| 6 | [HARD] 로그 파일·파일명·교환 포맷은 ISO 8601 `yyyy-MM-ddTHH:mm:ssZ` | 앱 로그 줄은 `[HH:mm:ss.fff]`(날짜·시간대 없음, 로컬 시각) — `MockXpeBackend.cs:341`, `PipelineOrchestrator.cs:49`, `AlertEntry.cs:14` | **해석 필요** — 규칙은 "로그 **파일**" 이고 이 줄들은 화면의 로그 영역입니다. GUI 가 로그 파일을 쓰는지는 안 봤습니다 |

**결함으로 세지 않은 것 — 부재가 위반은 아닙니다**: `CultureInfo.InvariantCulture` 사용 **0건**이지만,
찾은 파싱은 자동화 인자의 `int.TryParse` 와 `Enum.Parse` 뿐이고 문화권에 민감한 실수 파싱은
찾지 못했습니다(검색 범위: `gui/ImageProcTest` 의 `.Parse(`·`TryParse(`·`Convert.ToDouble/Single`).

### 4.4 NATIVE-INT-READINESS-001 (04-17) — **3건**

전부 "shall" / "Recommended Next Work Items" 입니다. **만들어진 것이 문서의 정책과 어긋나는 곳만**
셌습니다.

| § | 문서 | 만들어진 것 | 분류 |
|---|---|---|---|
| 6 | 디스플레이 최소 export 에 `xpe_display_apply_pipeline` 포함 | 팩토리 게이트(`RealXpeBackend.cs` 필수 목록)는 **그것이 없고** `xpe_voi_preset_create`·`xpe_gsdf_calibrate` 가 **더 있음** — **게이트가 둘이고 서로 다름** | 다름 |
| 7 | "must not treat the backend as all-or-nothing", `CompositeXpeBackend`·`RealXpeDisplayBackend`·`RealXpePreprocessBackend` | 셋 다 없음. `XpeBackendFactory` 앵커: *"both DLLs present, and all required exports; any failure → MockXpeBackend"* — **디스플레이 쪽은 전부-아니면-없음**. 다른 곳(`clients` 의 `ModuleReadinessGrading`)이 혼합 모드를 하는지는 **재지 않았습니다** | 다름 |
| 5.2 | 전처리 어댑터가 `schema = xpe-pre-e2e-report-v1` 과 `raw_sha256_before/after` 등을 emit 또는 synthesize | 스키마 문자열 **0건**. `sha256` 은 gui 서비스에 2건 있으나 **이 스키마인지 안 봤습니다** | 없음 |

## 5. `#163` — 사실 코멘트 1건

https://github.com/holee9/image-processing/issues/163#issuecomment-5706515462

```
**Lane C — 사실 기록 (GUI-C-76, §3.5.2)**
- 이 이슈를 대상으로 실행한 측정: 없음 (GUI-C-61 이후 카드 지시로 추적 보류)
- Lane C 보고서 중 이 이슈 언급: 1건 — GUI-C-65/report.md:97
  - 검색 명령: grep -rln "#163" .moai/reports/lane-gui/*/report.md
- 원인: 미확정
```

**해석·판정 문장은 넣지 않았습니다**(§3.5.2).

## 6. 실측 (verbatim)

```
[게이트, main 3c89919 ci.yml 501-553 행]
:170   → SKIPPED (accounted, #170) … exit=0
:99999 → NOT RUN (unexplained) … exit=1

python audit76.py docs76
== ARCH-001      | identifiers 35 absent 14 | file refs 4 absent 2
== DISP-INT-001  | identifiers 71 absent 8  | file refs 29 absent 2
== L10N-001      | identifiers 17 absent 6  | file refs 0 absent 0
== NATIVE-INT-…  | identifiers 27 absent 10 | file refs 0 absent 0
(이 표는 후보이고, 각 줄을 읽어 계획·금지·예시를 빼고 §4 의 수가 남았습니다)

CommunityToolkit refs: 0 · RoutedCommand uses: 0 · Dispatcher.InvokeAsync: 0 · Panels/Dialogs/Themes dirs: 0
IAppSettingsService interface: 0 · XPE_ AutomationIds: 0 · resx files: 0 · InvariantCulture uses: 0
W06 in E2E: 0 · IsPreviewStale/stale: 0 · Freeze(): 5 · xpe-pre-e2e-report-v1: 0

dotnet run --project gui/ImageProcTest.SelfCheck → InvalidOperationException: VOI window center should default to Abdomen preset.
ImageProcTest.E2E.exe → 같은 예외 (Program.cs:15 → :38)

dotnet build clients/ImageProcTest.slnx -c Debug     경고 0개 / 오류 0개
dotnet test …E2ETests… --no-build (Mock)   통과! 실패 0, 통과 71,  건너뜀 1, 전체 72 (59 s)
dotnet test …IntegrationTests… --no-build  통과! 실패 0, 통과 200, 건너뜀 1, 전체 201
```

## 7. 미검증 (Gaps)

- **문서 4건을 줄마다 읽지 않았습니다**(§3). 백틱 없는 산문 속 주장은 대부분 사거리 밖입니다.
- **HAZ-GUI-004 의 통제 (3) "이전 결과 clear"** 는 코드 한 줄을 읽은 것이고 실행하지 않았습니다.
- **SHA-GUI-001 · RTM-GUI-001 은 인용된 줄만 읽었습니다**(범위 밖 문서). 다른 위해 항목이 같은
  모양인지는 모릅니다.
- **`clients` 쪽이 혼합 모드를 구현하는지**(NATIVE-INT §7) 재지 않았습니다.
- **L10N 로그 규칙이 이 로그에 적용되는지**는 해석 문제이고, GUI 가 로그 파일을 쓰는지 안 봤습니다.
- **옛 E2E 러너의 두 번째 실패**(`ShowRuntimePanelMenuItem`)는 도달하지 못해 재지 못했습니다.
- **`ResizableDiagnosticsLayoutDetected = true` 가 CI 의 `gui-automation` 판정에 쓰이는지**는
  확인하지 않았습니다(그 잡은 자동화 보고서의 `Passed` 를 읽습니다 — C-69 시점 `ci.yml` 345행).
- **게이트 `:170` 은 CI 에서 돌려 보지 못했습니다.** 로컬에서 같은 스크립트를 돌린 것입니다.

## 8. 잔여 위험 (Residual risk)

- **HAZ-GUI-004 의 잔여 위험 "Low" 가 존재하지 않는 통제를 전제합니다**(§4.2).
- **자동화 보고서가 없는 레이아웃을 "감지" 로 보고합니다**(§2.1) — `Passed` 가 그 값에 기댑니다.
- **README 가 안내하던 SelfCheck·옛 E2E 가 실패 중입니다**(§2.3). README 는 이제 그렇게 말하지만
  도구는 그대로입니다.
- **ARCH-001 과 E2E-001 이 AutomationId 규칙에서 서로 반대를 말합니다**(§4.1).

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| 토큰 `:170` + `:99999` 반증 | GUI-C-76 §2 — 완료 |
| `gui/ImageProcTest/README.md` 정리 + 기록 | GUI-C-76 §3 — 완료 |
| `clients/README.md` 확인 | GUI-C-76 §3 — 완료(0건) |
| 문서 4건 내용 대조(고치지 않음) | GUI-C-76 §4 — 완료(17 + 1) |
| `#163` 사실 한 줄 | GUI-C-76 §5 — 완료 |
| HAZ-GUI-004 통제 부재 | **새 카드 후보** — 리더 판정 |
| `ResizableDiagnosticsLayoutDetected` 무조건 true | **새 카드 후보** — 리더 판정 |
| SelfCheck·옛 E2E 러너 실패 | **새 카드 후보** — 리더 판정 |

## 부록 — 사용한 명령

```bash
MSYS_NO_PATHCONV=1 git show "origin/main:.github/workflows/ci.yml" > ci-main.yml
sed -n '501,553p' ci-main.yml | sed 's/^          //' | sed 's|build/e2e-native-results|build/c76|' > gate76.ps1
MSYS_NO_PATHCONV=1 git show "origin/main:docs/project/<doc>.md" > docs76/<doc>.md
python audit76.py docs76
grep -rn "GridSplitter\|Binding Alerts\|RuntimeInfo" gui/ImageProcTest --include=*.xaml
gh issue comment 163 --body-file c163.md
```
