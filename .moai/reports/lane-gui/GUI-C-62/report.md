# GUI-C-62 — 거절 신호가 사람에게 닿는가 (#161 #149)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-62 · Refs #161 #149 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건 — 미푸시 (단언 3건; 제품 변경 없음)
- **답: 사람도 못 봅니다. 다만 이유가 제가 C-61 에 적은 것과 다릅니다**(§1·§2).
- **결과: Native 3회 0 실패 · Mock 0/58/1/59 · 통합 0/196/1/197 · slnx 0경고 0오류**

---

## 1. 먼저 정정 — 로그는 **도달 가능**합니다

C-61 에서 저는 "`LogListBox` 가 UIA 트리에 안 나타난다 · `TabItem` 0개 · **도달 불가**" 라고
적었습니다. **틀렸습니다.**

```
LOGPROBE AnalysisPanel present=True rect={X=1380,Y=396,Width=380,Height=638}
LOGPROBE 'Log' tab button present=True rect={X=1665,Y=396,Width=95,Height=43}
LOGPROBE LogListBox after pressing 'Log'=True rect={X=1392,Y=477,Width=356,Height=156}
LOGPROBE line count=6
```

**`AnalysisPanel` 안의 "Log" 버튼을 누르면 목록이 나옵니다** — 자동화도 읽고, 크기를 가진
사각형이므로 화면에도 그려집니다.

**C-61 의 프로브가 두 군데서 틀렸습니다:**

- **`TabItem` 을 찾았습니다.** 이 "탭" 은 `TabControl` 이 아닙니다 — `AnalysisPanel.xaml:23-91`
  의 **`Button` 4개**가 `SwitchAnalysisTabCommand` 를 호출하고, 각 영역이
  `Visibility="{Binding AnalysisTab, Converter={StaticResource TabEquals}, …}"` 로 나타납니다.
  `TabItem` 이 0개인 것은 **맞고, 그것이 도달 불가를 뜻하지 않습니다.**
- **`View → Logs Panel` 을 켰습니다.** 그 메뉴는 `ShowLogsPanel` 에 묶여 있는데, **그 속성을
  읽는 `Visibility` 바인딩이 하나도 없습니다**(§4). 켜도 아무 일도 안 일어납니다.

**"UIA 에 안 보인다" 에서 "도달 불가" 로 간 것이 오류입니다** — 제가 찾은 방법으로 못 찾은 것을
없는 것으로 적었습니다.

## 2. 그런데 거절 줄은 거기 없습니다 — **지워집니다**

`comparisonMode` 를 `"NotAMode"` 로 두고 띄운 뒤 로그 전체를 읽었습니다.

```
LOGPROBE LINE | [15:33:28.728] Initialized backend 'MockXpeBackend' (v0.0.0-mock).
LOGPROBE LINE | [15:33:28.728] Defect calib dir = data/calibration/defect
LOGPROBE LINE | [15:33:28.728] Gain calib dir = data/calibration/gain
LOGPROBE LINE | [15:33:28.728] Offset calib dir = data/calibration/offset
LOGPROBE LINE | [15:33:28.728] Requested backend mode = Mock
LOGPROBE LINE | [15:33:28.728] MockXpeBackend bootstrap started.
```

**6줄뿐이고, 거절 줄이 없습니다. 바로 앞줄인 `"GUI-S0 initialized."` 도 없습니다.**

코드가 이유를 말합니다 — `MainWindowViewModel` 생성자:

```csharp
Log("GUI-S0 initialized.");     // 1186행: Logs.Insert(0, …)
ReportRejectedComparisonMode(); // #161 의 거절 신호
InitializeBackend();            // 511행: Logs.Clear();  ← 둘 다 지움
```

**두 줄은 쓰이고 몇 밀리초 뒤 지워집니다.** 그래서 사람도 자동화도 볼 수 없습니다.

**카드의 두 갈래 중 "사람도 못 본다" 이고, 이유는 로그가 안 보여서가 아니라 지워져서입니다.**
#161 의 조치("거절하고 이름을 부른다")는 **현재 아무 신호도 내지 못합니다** — #157·#160 과 같은
형태입니다.

## 3. 어디로 내보낼지 — 후보와 사실 (판정은 리더 몫)

| 후보 | 사람에게 닿나 | 확인한 사실 |
|---|---|---|
| **호출 위치를 `InitializeBackend()` 뒤로** | **예** | 같은 로그 목록이고 지워진 뒤에 쓰이므로 남습니다. 한 줄 이동 |
| **알림 큐(`Alerts`)** | 예 (Alerts 패널) | `Alerts.Insert(0, new AlertEntry{Severity,Code,Message,Timestamp})` 로 이미 6곳에서 씁니다. **다만 `InitializeBackend` 이 `Alerts.Clear()` 도 합니다** — 같은 이동이 필요합니다 |
| **상태 표시줄(`StatusText`)** | 예, 그러나 한 줄 | 다음 상태가 덮어씁니다. 기동 직후 여러 상태가 지나갑니다 |
| **파일 로그** | — | **GUI 쪽에 없습니다.** `OpenRuntimeLogsMenuItem` 은 `MainWindow.xaml.cs:254` 의 "비활성이어야 한다" 검사 목록에 있습니다 |

**가장 작은 것이 가장 확실합니다** — 호출을 `InitializeBackend()` 뒤로 옮기면 지금의 로그 목록에
남고, 그 목록은 §1 에서 도달 가능함이 확인됐습니다. **고치지 않았습니다.**

알림 큐를 쓸 경우 `Severity="WARN"`, `Code` 는 이 저장소 관례상 대문자 스네이크(예:
`SETTINGS_UNSUPPORTED_VALUE`)가 맞아 보입니다 — 이것도 판정 대상입니다.

## 4. 곁에서 나온 것 — View 메뉴의 패널 토글이 아무것도 안 합니다

```
grep 'Visibility="{Binding Show'  →  0건
```

`ShowRuntimePanel` · `ShowLogsPanel` · `ShowAlertsPanel` · `ShowMetadataPanel` 은 **메뉴 체크에만
묶여 있고 어떤 레이아웃도 읽지 않습니다.** 실제 패널 가시성은 `FocusMode`/`RightPanelOpen`
(`MainWindow.xaml:496-501`)과 `AnalysisTab` 이 결정합니다.

**즉 View 메뉴의 패널 항목 4개는 눌러도 화면이 바뀌지 않습니다.** 이 카드가 고칠 것은 아니고,
제가 C-61 에서 헛짚은 원인이기도 해서 적어 둡니다.

### `TabIndex` 0건과 `TabItem` 0개는 같은 원인이 아닙니다 (한 줄 의견)

`TabItem` 0개는 **이 화면이 `TabControl` 을 안 쓰기 때문**이고(버튼 + 가시성 컨버터),
`TabIndex` 0건은 **ACCESS-001 §5.1 의 요구가 아직 구현되지 않은 것**입니다 — 서로 무관합니다.

## 5. 할 일 2 — 단언 3건 추가 (통합 194 → 197)

| 새 단언 | 무엇을 고정하나 |
|---|---|
| `ASettingsFileHoldingAnUnsupportedMode_LoadsAsTheDefault` | **실제 파일 경로**를 `AppSettingsService.Load()` 로 태웁니다. C-60 이 미검증으로 남기고 C-61 이 손으로 확인한 그 항목입니다 |
| `ASettingsFileHoldingASupportedMode_KeepsIt` | 위가 "파일을 무시하는 로더" 를 재고 있지 않음을 보장 |
| `EachMenuItem_ChecksTheModeItSelects` | 각 항목의 `ConverterParameter` 가 `CommandParameter` 와 같음 — **선택하는 모드와 체크하는 모드가 같다** |

**배포 설정 파일은 건드리지 않습니다** — 임시 디렉터리에 파일을 써서 같은 로더를 태웁니다.
C-46 이 커밋을 막았던 이유가 제거된 것이지, 그 이유가 틀렸던 것이 아닙니다.

**메뉴 체크의 "정확히 하나만 On" 은 실행으로만 봤습니다**(C-61). 그 성질은 WPF 바인딩이
계산하므로 net8.0 테스트에서 평가할 수 없어, **그것을 참으로 만드는 것**(항목이 자기 모드를
검사한다 + 여섯 모드가 서로 다르다)을 고정했습니다.

### 개수 대조가 헛도는 단언을 잡았습니다

`EachMenuItem_ChecksTheModeItSelects` 에 "이 검색이 메뉴 항목 전부를 봤는가" 대조를 넣었더니
**첫 판이 실패**했습니다 — 명령으로 세면 6이 아니라 **10**입니다. `KeyBinding` 4개가 같은 명령을
쓰기 때문입니다. 대조가 없었으면 부분집합만 검사하며 통과했을 것입니다. `InputBindings` 블록을
제외하도록 고쳤고, **C-60 의 `EveryMenuParameter_IsASupportedMode` 가 이름과 달리 메뉴와 키를
모두 본다**는 사실도 그 메시지에 적었습니다.

### 반증

| 주입 | 실패한 단언 |
|---|---|
| 한 항목의 체크 대상을 다른 모드로 바꿈 | **`EachMenuItem_ChecksTheModeItSelects` 1건만** |
| 설정 게이트 제거(raw 저장) | 파일 단언 1건 + 무효값 단언 4건 + JSON 단언 1건 = **6건**(정상 7모드 단언은 통과) |

## 6. 실측 (verbatim)

```
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개 / 오류 0개

dotnet test …E2ETests… --no-build                      (Mock)
통과!  - 실패: 0, 통과: 58, 건너뜀: 1, 전체: 59 (27 s)

dotnet test …IntegrationTests… -c Debug
통과!  - 실패: 0, 통과: 196, 건너뜀: 1, 전체: 197

Repeat-E2E.ps1 -Times 3 -Backend Native → runs with a failure: 0 / 3
run 1 71.2s · run 2 72.4s · run 3 70.1s
```

Baseline 귀속: 통합 C-60 시점 **194** → **197**. E2E 59 불변.

## 7. 미검증 (Gaps)

- **스크린샷으로 판정하지 않았습니다**(카드 지시). "사람이 볼 수 있다" 의 근거는 **요소가 UIA 에
  있고 0이 아닌 사각형을 갖는다** 까지입니다 — 가려짐·색 대비·글꼴 크기는 보지 않았습니다.
- **로그가 지워진다는 것은 관측이지만, 지우는 주체를 바꿔 보지는 않았습니다**(고치지 말라는
  지시). `Logs.Clear()` 를 빼면 줄이 남는지는 확인하지 않았습니다.
- **알림 큐가 실제로 화면에 뜨는지 이 카드에서 보지 않았습니다.** 코드에 쓰는 자리가 6곳 있다는
  것만 확인했습니다.
- **"정확히 하나만 체크" 는 여전히 실행 관측 1회**입니다(C-61). CI 에서 도는 단언은 그 성질의
  전제만 고정합니다.
- **View 메뉴 패널 토글 4개가 무효**라는 것은 grep 결과이고, 눌러서 화면이 안 바뀌는 것을 직접
  보지는 않았습니다.

## 8. 잔여 위험 (Residual risk)

- **#161 의 조치는 지금 신호를 내지 않습니다.** 설정 파일에 잘못된 모드를 둔 사용자는 아무
  안내도 받지 못하고, 다음 기동에도 같습니다.
- **정정이 두 번째입니다** — C-59 에서 "문서에 없다", 여기서 "도달 불가". **둘 다 "내가 찾은
  방법으로 못 찾았다" 를 "없다" 로 적은 것**입니다. 다음에 부재를 적을 때 찾은 범위를 같은
  문장에 적겠습니다.
- **임시 프로브가 배포된 `appsettings.json` 을 잠시 고칩니다.** 이번에도 원복했고 커밋하지
  않았지만, 중단되면 잘못된 값이 남습니다.

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug \
  --filter "FullyQualifiedName~ComparisonModeSingleSourceTests"
dotnet test clients/ImageProcTest.E2ETests/… --no-build \
  --filter "FullyQualifiedName~TempLogReachProbe" --logger "console;verbosity=detailed"   # 임시, 삭제됨
grep -rn 'Visibility="{Binding Show' gui/ImageProcTest --include=*.xaml
powershell -File clients/ImageProcTest.E2ETests/Staging/Repeat-E2E.ps1 \
  -Times 3 -Backend Native -NativeDir build/ci-common/bin -ResultsDirectory build/e2e-c62
```
