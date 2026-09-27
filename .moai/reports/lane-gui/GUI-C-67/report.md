# GUI-C-67 — 로그가 사실만 말하게 한다 (#165 후속)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-67 · Refs #165 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건 — 미푸시
- **제 새 단언이 기존 단언 하나를 깨뜨렸고, 그것도 이 카드의 결과입니다**(§3).
- **결과: Native 3회 0 실패(90.5 s) · Mock 0/68/1/69 · 통합 0/198/1/199 · slnx 0경고 0오류**

---

## 1. 할 일 2 를 먼저 — 다시 쟀습니다

카드가 "**'아무도 안 읽는다'는 C-63 의 grep 결과이지 이번에 다시 잰 것이 아니다**" 라고 짚었으므로
다시 셌습니다. 제거 다섯·비활성 둘 이후의 현재 상태입니다.

| `ResetLayout` 이 쓰는 것 | 지금 그 값을 읽는 곳 | 화면이 바뀌나 |
|---|---|---|
| `ShowLogsPanel` | 메뉴 체크 · 자동화 보고서 · **`AnalysisPanel` 의 MultiBinding** | **예** — 로그 영역 |
| `ShowCalibrationPanel` | **비활성 메뉴 항목의 체크** · 보고서 · `ShowCalibrationSettings` 가 씀 | **예** — 체크 표시 |
| `Settings.ShowDisplayPanel` | **비활성 메뉴 항목의 체크** · 보고서 ×2 | **예** — 체크 표시 |
| `ShowRuntimePanel` | 보고서만 | 아니오 |
| `ShowRawSettingsPanel` | 보고서만 | 아니오 |
| `ShowImageSummaryPanel` | 보고서만 | 아니오 |
| `ShowMetadataPanel` | 보고서만 | 아니오 |
| `ShowAlertsPanel` | 보고서만 | 아니오 |
| `ResetComparisonView()` | — | **예** — 줌·팬·모드 |

**여덟 중 셋이 무언가를 바꿉니다**(C-65 이전에는 하나였습니다).

### 그래서 전자를 골랐습니다 — 근거는 "지워도 아무 일이 없다"

아래 다섯은 **메뉴 항목이 사라진 뒤로 `ResetLayout` 이 유일한 writer** 이고, 초기값이 전부
`true` 입니다. 즉 **영구히 `true` 인 값에 `true` 를 다시 쓰는 것**이라 지워도 **관측 가능한
변화가 없습니다** — 보고서의 해당 필드도 값이 달라지지 않습니다.

```
ShowRuntimePanel · ShowRawSettingsPanel · ShowImageSummaryPanel
ShowMetadataPanel · ShowAlertsPanel                       ← 다섯, 제거
```

**남긴 셋은 전부 무언가를 바꿉니다.** 그래서 `"Layout reset."` 이 이제 참입니다 — 문구를 고치지
않고 동작을 맞췄습니다.

**되돌릴 자리를 적어 뒀습니다**: 제거한 패널 중 하나라도 실제로 만들어지면 그 플래그는 이
목록으로 돌아와야 합니다. 나간 이유는 플래그가 아니라 **없는 패널**이었습니다.

## 2. 할 일 1 — 문구를 선례대로

```
StatusText = "Calibration settings: no panel implemented (#165)."
Log("Menu command: calibration settings — not implemented; no panel is shown (#165).")
```

**명령은 남겼습니다**(`MENU-001` §4 · §9.1 S0·Always). **화면은 만들지 않았습니다**(C-64 에서
멈춘 것과 같은 이유). 형식은 이 저장소의 선례
`"RunOnAllQueuedCommand: not implemented (Slice 7)."` 를 따랐습니다.

## 3. 단언 2건 — 그리고 제 단언이 기존 단언을 깨뜨렸습니다

| 시나리오 | 주장 |
|---|---|
| `S09_CalibrationSettings_DoesNotClaimAPanelAppeared` | 그 명령을 실행해도 로그에 "panel shown/visible" 이 **없고**, 미구현 고지가 **있다** |
| `S10_ResetLayout_RestoresTheState` | 토글을 끈 뒤 초기화하면 **로그 영역이 돌아온다** |

**둘 다 소스가 아니라 사용자가 읽는 로그·화면에서 확인합니다.**

### 처음 판이 S08 을 깨뜨렸습니다

새 시나리오 둘을 넣자 **기존 `S08` 이 실패**했습니다. 같은 앱을 공유하는데 **S08 이 토글의
기본 상태(OFF)를 가정**했고, S09·S10 이 먼저 돌면 켜진 채로 넘어왔기 때문입니다.

```
S10 descendants shown=155 off=139 afterReset=157     ← 첫 판, 157 ≠ 155 로 실패
```

두 가지를 고쳤습니다:

- **S08 이 시작 상태를 스스로 만듭니다**(켜져 있으면 끄고 시작). **다른 테스트가 남긴 것에
  의존하는 테스트는 토글이 아니라 실행 순서를 재는 것**입니다.
- **S10 은 전체 자손 수를 쓰지 않습니다.** 초기화가 비교 뷰도 되돌리므로 그 수가 **두 가지
  이유로** 움직입니다(155 → 157). 대신 **로그 영역 자체의 유무**를 봅니다. **S08 은 계속 자손
  수를 씁니다** — 거기서는 움직이는 이유가 하나뿐이기 때문입니다.

고친 뒤 **연속 2회 10/10**.

### 반증

| 주입 | 실패한 단언 |
|---|---|
| 옛 문구("panel visible/shown") 복원 | **S09 1건만** |
| `ResetLayout` 이 `ShowLogsPanel` 을 안 쓰게 | **S10 1건만** |

## 4. 할 일 3 — census 확장: **새로운 거짓은 없습니다**

C-66 이 `StatusText`/`Log` 리터럴만 봤으므로 나머지를 봤습니다.

| 범위 | 개수 | 결과 |
|---|---|---|
| `AutomationProperties.HelpText` | **1** | `ViewportShell` ← `Settings.ComparisonMode`. **참** — C-60 의 게이트 이후 그 속성은 지원 모드만 담고, 뷰포트도 같은 값을 그립니다. C-62 가 본 `NotAMode` 보고는 그 게이트 이전 상태였습니다 |
| `ToolTip` | **24종** | 전부 `"Enabled when …"` · `"Requires …"` 류의 **미래 고지**입니다. "지금 그렇다" 고 주장하는 것이 없습니다 |
| 보간 `StatusText`/`Log`(`$"…"`) | **29종** | 전부 실제로 일어난 일을 보고합니다. 실패 경로는 `catch` 에서 실패로 적습니다 |
| 메뉴 `Header` | — | 라벨이고 상태 주장이 아닙니다. `"(Phase 1a)"`·`"(Phase 1b)"` 는 이번에 붙인 정직한 표시입니다 |

### 경계 항목 하나 — 보고서의 필드 이름

자동화 보고서가 `visiblePanels` 라는 이름으로 여덟 플래그를 씁니다
(`MainWindowViewModel.cs:599-608`). **그중 다섯은 존재하지 않는 패널의 플래그**이고, 이제 값이
영구히 `true` 입니다. **문장이 아니라 필드 이름이라 census 의 원래 기준 밖**이지만, 읽는 사람이
"패널 다섯이 보인다" 로 받을 수 있습니다. **고치지 않았습니다** — 보고서 스키마는 배포 산출물이고,
필드를 지우는 것은 소비자를 확인한 뒤의 일입니다(현재 소비자: 생성된 json 외에 없음).

## 5. 실측 (verbatim)

```
dotnet test …E2ETests… --no-build --filter "…PanelToggleScenarios"   (2회)
통과!  - 실패: 0, 통과: 10, 건너뜀: 0, 전체: 10  (22 s / 21 s)

[A old-wording] 실패! - 실패: 1, 통과: 9   ← S09 만
[B no-restore]  실패! - 실패: 1, 통과: 9   ← S10 만

dotnet test …E2ETests… --no-build                      (Mock)
통과!  - 실패: 0, 통과: 68, 건너뜀: 1, 전체: 69 (49 s)

dotnet test …IntegrationTests… -c Debug
통과!  - 실패: 0, 통과: 198, 건너뜀: 1, 전체: 199

Repeat-E2E.ps1 -Times 3 -Backend Native → runs with a failure: 0 / 3
run 1 90.6s · run 2 90.6s · run 3 90.4s
```

**시간 예산**: Native **81 s → 90.5 s**(+9.5 s, 시나리오 2건 + S08/S10 의 상태 정리 단계).
게이트 180 s 대비 여유 약 **89.5 s**. **한 카드에서 가장 크게 늘었습니다** — 메뉴를 여닫는
시나리오는 대기 시간이 길고, S10 은 상태를 만들었다 되돌리기까지 합니다.

Baseline 귀속: E2E C-66 시점 **67** → **69**(2건). 통합 199 불변.

## 6. 미검증 (Gaps)

- **`ShowCalibrationPanel`·`ShowDisplayPanel` 이 "체크 표시를 바꾼다" 는 것을 실행으로 재지
  않았습니다.** 바인딩을 읽어 적었습니다 — 비활성 항목이라 눌러서 확인할 수 없고, 초기화 후
  체크 상태를 UIA 로 읽는 시나리오는 만들지 않았습니다.
- **다섯 개 쓰기를 지운 것이 자동화 보고서를 바꾸지 않는다는 것**은 "그 값이 영구히 true 다" 라는
  추론에 기댑니다. **보고서를 전후로 비교하지는 않았습니다.**
- **`"Layout reset."` 이 이제 참**이라는 것은 §1 표에 근거하고, S10 이 그중 하나(로그 영역)만
  실행으로 봅니다.
- **census 는 여전히 정적 검색입니다.** 각 문구가 실제 상황에서 언제 나오는지 실행으로 확인한
  것은 이번에 단언한 둘뿐입니다.
- **보고서 필드 이름 문제는 재지 않고 적기만 했습니다**(§4).

## 7. 잔여 위험 (Residual risk)

- **Native 시간이 90.5 s 로 늘었습니다.** 여유 89.5 s. 이 속도로 시나리오가 늘면 다음 두어
  카드에서 게이트가 문제가 됩니다.
- **공유 앱을 쓰는 시나리오가 늘수록 순서 의존이 생깁니다.** 이번에 한 번 났고 고쳤지만, 같은
  컬렉션에 계속 쌓는 방식 자체가 그 위험을 키웁니다.
- **`ResetLayout` 이 더 이상 다섯 플래그를 되돌리지 않습니다.** 그 패널들이 생기면 되돌려
  놓아야 하고, 잊으면 "초기화가 일부만 한다" 가 됩니다 — 코드에 그 조건을 적어 뒀습니다.
- **보고서의 `visiblePanels` 가 존재하지 않는 패널을 보고합니다**(§4).

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
grep -rn "\bShowRuntimePanel\b" gui/ImageProcTest clients --include=*.cs --include=*.xaml
grep -rn "AutomationProperties.HelpText" gui/ImageProcTest --include=*.xaml --include=*.cs
grep -rhoE 'ToolTip="[^"]*"' gui/ImageProcTest --include=*.xaml | sort -u
dotnet test clients/ImageProcTest.E2ETests/… --no-build --filter "…PanelToggleScenarios"
powershell -File clients/ImageProcTest.E2ETests/Staging/Repeat-E2E.ps1 \
  -Times 3 -Backend Native -NativeDir build/ci-common/bin -ResultsDirectory build/e2e-c67
```
