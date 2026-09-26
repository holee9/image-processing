# GUI-C-122 — 한 번 놓치면 못 찾던 것을 고친다 (#173)

## 1. (a) 세 자리를 앱에서 쟀다

코드를 읽은 것이 아니라 **깨진 설정 파일을 앞에 두고 앱을 띄워** 측정했다.

| 자리 | 결과 |
|---|---|
| `Alerts` | **화면에 표시하는 요소가 없다.** `AlertsList`·`AlertListBox`·`AlertsPanel` 모두 트리에 없고 `ClearAlertsButton` 만 있다 → `#198` |
| `StatusText` | **다음 동작에 덮인다.** 측정: Apply 후 `'Display pipeline requires a loaded raw image.'` |
| 로그 | **살아남는다.** View ▸ Logs(기본 Off) + Log 탭을 거치면 보인다 |

그러나 살아남는 그 자리가 카드의 두 번째 요구를 못 지켰다:

```
LogListBox patterns: text=False value=False
```

`ListBox` 항목이라 **경로를 복사할 수 없었다** — 읽어서 옮겨 적어야 했다. 그래서 (a) 를 보고하고 멈췄고, 리더가 방향을 정했다(ⓐ, 단 렌더링은 그대로).

## 2. (b) 구현 — 항목 렌더링은 그대로

**모든 로그 줄의 렌더링을 바꾸지 않았다.** `DataTemplate` 은 손대지 않았고, 다음 둘만 더했다.

- `ListBox` 에 `SelectedItem="{Binding SelectedLog}"`
- 기존 `Clear` 옆에 `Copy` 버튼(`CopyLogLineButton`) + `CopySelectedLogCommand`

클립보드 실패는 삼키지 않고 상태 표시줄에 적는다 — 작동한 것처럼 보이는 버튼보다 낫다.

## 3. (c) 반증 — 경고가 사라진 **뒤에** 찾을 수 있는가

### 초록 — 찾고, 복사된다

```
status bar after an action: 'Display pipeline requires a loaded raw image.'   ← 경고 사라짐
found line: [12:15:13.085] Your saved settings could not be read, ... kept at '...unreadable-20260919-121513'.
clipboard : [12:15:13.085] Your saved settings could not be read, ... kept at '...unreadable-20260919-121513'.
```

클립보드 내용이 **실제로 존재하는 구조본 파일 이름**을 담는지까지 단언한다.

### 반대 방향 — 정상 실행에서는 그 자리가 비어 있다

```
log lines: 6        (로그는 차 있고, 그중 경고를 담은 줄은 0)
```

로그가 비어 있지 않다는 대조를 같이 단언한다 — 죽은 패널에서 통과하지 않도록.

### 반증

`CopyLogLineButton` 의 AutomationId 를 바꿔 복사 수단을 없앴다.

```
RED_EXIT=1   통과: 1   실패: 1
There is no way to copy the selected line, so the path must be typed by hand.
```

## 4. 재초기화 — 측정 결과 (단언 아님)

```
before re-initialise: 7 lines, 1 carrying the path
after  re-initialise: 6 lines, 0 carrying the path
MEASURED: a re-initialise empties the last place holding the rescued path (#198).
```

`InitializeBackend` 가 `Logs.Clear()`·`Alerts.Clear()` 를 하는 것은 설계다(#161, GUI-C-63 — 재초기화가 이전 실행이 아니라 자기 실행을 보고하도록). 그 설계의 **이 메시지에 대한 결과**가 위와 같다. **어디에 두어야 하는가는 `#198` 의 결정**이므로 이 시험은 사실만 기록하고 고쳐지지 않은 것을 단언하지 않는다.

## 5. 빌드·시험

```
BUILD_EXIT=0
Mock  E2E 전체   : 통과 121, 건너뜀 21, 실패 0 (5m 14s)
IntegrationTests : 통과 254, 건너뜀 1, 실패 0
```

## 6. 미검증 (Gaps)

- **클립보드 실패 경로**(다른 프로세스가 점유)는 재지 않았다. 상태 표시줄에 사유를 적게 해 두었지만 실행하지 않았다.
- **기본 Off 와 두 단계 이동은 건드리지 않았다**(카드 지시). 즉 사용자는 여전히 View ▸ Logs 를 켜고 Log 탭으로 가야 한다 — "찾을 수 있다" 는 **그 경로를 아는 사용자** 기준이다.
- 복사되는 것은 **줄 전체**(타임스탬프·문장 포함)이지 경로만이 아니다. 붙여 넣는 곳이 경로만 받는다면 다듬어야 한다.
- `Alerts` 가 보이지 않는 것과 네이티브 알림이 전혀 읽히지 않는 것은 `#198` 로 넘겼고 **이 카드에서 건드리지 않았다.**

## 7. 잔여 위험

- 로그는 여전히 **재초기화 한 번으로 비는** 자리다. 이 카드는 "복사할 수 있게" 만들었을 뿐 **오래 남게** 만들지 않았다.
- `Copy` 버튼은 **선택된 줄**을 복사한다. 아무것도 선택하지 않으면 아무 일도 일어나지 않는다(상태 표시줄도 안 바뀐다) — 조용한 무동작이므로, 이 점은 `#198` 설계 때 같이 볼 값어치가 있다.

🗿 MoAI
