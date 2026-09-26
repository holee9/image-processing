# GUI-C-125 — 알림을 사용자가 볼 수 있는 곳에 놓는다 (#198 ①)

## 1. 구별 수단 — **줄 텍스트의 `ALERT` + 심각도 + 코드**, 그리고 이유

```
[14:34:35.825] ALERT WARN SETTINGS_UNREADABLE: Your saved settings could not be read, ...
```

- **색·아이콘을 쓰지 않았다.** 둘 다 `DataTemplate` 을 건드려야 하는데, GUI-C-122 가 다른 로그 줄의 렌더링을 바꾸지 않기로 하고 거기서 멈췄다. 그 결정을 이 카드가 되돌릴 이유가 없다.
- **자동화가 읽을 수 있어야 한다.** 색은 UIA 트리에 없다. 단언할 수 없는 구별은 다음 사람이 지워도 아무 시험도 울지 않는다.
- **복사에 딸려 가야 한다.** C-122·C-123 이 만든 복사가 이 카드의 결과물을 실어 나른다 — 클립보드에 `ALERT WARN SETTINGS_UNREADABLE` 이 그대로 들어간다(측정함).
- 코드까지 넣은 이유: `ALERT` 만으로는 "경고성 문장" 과 구별되지만 **무엇에 대한 경고인지**는 안 남는다. `#194`·`#196` 이 넣은 알림들이 ② 층에서 들어오면 코드로 구분된다.

**구현은 한 자리에 모았다.** `Alerts.Insert(0, …)` 8곳을 `RaiseAlert(alert)` 하나로 바꾸고, 그 메서드가 컬렉션과 로그 양쪽에 쓴다. 앞으로 알림이 추가돼도 보이는 자리에 자동으로 실린다 — 호출부가 기억해야 하는 규칙이면 언젠가 잊힌다.

`Alerts` 컬렉션과 `ClearAlertsButton` 은 **그대로 두었다**(② 에서 네이티브 알림이 들어올 자리).

중복 하나를 없앴다: 설정 경고가 `Alerts.Insert` + `Log(message)` 를 각각 하고 있었으므로, 이제 같은 문장이 접두사 있는 줄과 없는 줄로 두 번 남을 뻔했다.

## 2. `#161` 의 근거 — **읽어 보니 근거가 거기 없다**

카드는 "`#161` 이 지우기로 했다" 를 전제했는데, **코드가 말하는 것은 다르다.**

`#161` 이 붙어 있는 자리는 **순서에 대한 주석**이다(MainWindowViewModel.cs:142):

> `// AFTER the backend, not before (#161, GUI-C-63). InitializeBackend clears Logs and Alerts, so anything said before it is written and erased within the same constructor — measured in GUI-C-62, where a run with a rejected mode showed six log lines, none of them the rejection and none of them the "GUI-S0 initialized." line written immediately before it. The whole of that moment was gone, not just one line.`

즉 **`#161` 은 지우기를 결정한 것이 아니라 지우기를 피해 간 것**이다. 지우기 자체의 출처는 git blame 으로 확인했다:

```
d5432d26 (drake.lee 2026-04-16)  Alerts.Clear();
d5432d26 (drake.lee 2026-04-16)  Logs.Clear();
커밋 메시지 전문: "GUI 메뉴와 오프라인 도움말 추가"
```

**그 커밋 어디에도 지우는 이유가 없다.** 본문이 한 줄이고 그 한 줄은 다른 일을 말한다.

### 의견 — 백엔드 상태를 지우는 것과 사용자가 본 기록을 지우는 것

**같은 것이 아니라고 본다.** 근거는 두 가지다.

**(1) 지우기가 무엇을 위한 것인지 코드 자체가 나눠서 말한다.** 같은 블록에서 지우는 것들은 성격이 다르다:

```csharp
Alerts.Clear();
Logs.Clear();
_drainedBackendLogCount = 0;
_drainedBackendAlertCount = 0;
```

뒤의 두 줄은 **드레인 커서**다 — 새 백엔드에서 다시 0부터 읽어야 하므로 반드시 0 이어야 한다. **백엔드 상태다.** 앞의 두 줄은 **사용자가 이미 본 화면**이다. 커서를 되감는 것과 사용자가 본 것을 지우는 것이 한 줄 간격으로 붙어 있을 뿐, 같은 필요에서 나온 것이라는 근거는 코드에 없다.

**(2) `#161` 이 이미 그 대가를 치렀다.** GUI-C-62 에서 측정된 것은 "거부 사유가 사라졌다" 가 아니라 **그 순간 전체가 사라졌다**(여섯 줄 중 아무것도 남지 않음)는 것이고, 대응은 **말하는 순서를 옮기는 것**이었다. 지우기가 옳았다면 순서를 옮길 필요가 없었을 것이다 — 순서 이동은 **지우기를 건드릴 수 없다는 전제 아래의 우회**다.

**다만 단정하지 않는다.** 내가 확인하지 못한 것이 있다: 재초기화가 백엔드를 **교체**하므로, 옛 백엔드가 남긴 알림이 새 백엔드의 상태를 말하는 것처럼 읽히는 위험이 있다. 그것이 원래 의도였다면 지우기에는 근거가 있고, 그 경우에도 **"사용자가 본 것을 지운다" 는 대가**는 그대로 남으므로 — 구분선을 넣고 이어 쓰는 쪽(예: `--- backend re-initialised ---`)이 양쪽을 만족시킬 수 있다고 본다. **판정은 리더 몫이고 이 카드에서 아무것도 바꾸지 않았다.**

## 3. 단언 — 양방향

### 초록

```
alert line: [14:34:35.825] ALERT WARN SETTINGS_UNREADABLE: Your saved settings could not be read, ...
ordinary lines: 6                          ← 평범한 줄에는 표지가 없다
clipboard : (같은 줄, ALERT·코드 포함)
```

띄운 알림은 **GUI 가 직접 내는** `SETTINGS_UNREADABLE` 이다. 카드가 예로 든 `#194`·`#196` 알림은 **네이티브 모듈**이 내는 것이고 GUI 가 아직 꺼내지 않으므로(② 층) 띄울 수 없다 — 대신 그 알림들이 통과할 **같은 경로**(`RaiseAlert`)를 지난다.

C-122·C-123 이 세운 것도 같은 케이스에서 확인한다 — 선택 없으면 Copy 비활성, 선택하면 활성, 복사된 내용에 표지가 실린다.

### 대조군

```
log lines: 6 (비어 있지 않음), SETTINGS_UNREADABLE 0건
```

## 4. 반증 — 관측을 단언보다 앞에

`RaiseAlert` 의 `Log(...)` 를 지워 컬렉션에만 넣게 되돌렸다. 시험이 **줄 목록을 먼저 출력**하므로 기록에 "보이지 않음" 이 먼저 남는다:

```
log lines: 6
  [14:35:11.328] Initialized backend 'MockXpeBackend' (v0.0.0-mock).
  ...                                    ← ALERT 줄이 없다
→ No alert is on screen; the alert went into a collection nothing displays (#198 ①).
```

## 5. 빌드·시험

```
BUILD_EXIT=0
Mock  E2E 전체   : 통과 124, 건너뜀 21, 실패 0 (5m 19s)
IntegrationTests : 통과 254, 건너뜀 1, 실패 0
```

## 6. 미검증 (Gaps)

- **네이티브 알림(② 층)은 띄우지 못했다.** GUI 가 `xpe_alert*` 를 꺼내지 않으므로 이 카드에서 확인 불가다. 확인한 것은 **경로가 하나로 모였다**는 것뿐이고, 드레인이 붙었을 때 실제로 보일지는 그 카드가 잴 일이다.
- **`NativeAlertDrain` 경유 알림**(`RaiseAlert(alert)` 루프)은 실행으로 재지 않았다. 코드 경로는 같지만 띄우지 못했다.
- 알림이 **많을 때**의 가독성은 보지 않았다. 로그와 섞이므로 알림만 걸러 보는 수단은 없다.
- 로그는 여전히 **View ▸ Logs(기본 Off) + Log 탭** 두 단계 뒤에 있다. 이 카드는 "볼 수 있는 곳" 으로 옮겼을 뿐 **눈에 띄게** 만들지 않았다.
- 재초기화가 지우는 문제는 **그대로다**(카드 [HARD] 지시).

## 7. 잔여 위험

- `ALERT` 는 평범한 문자열이다. 어떤 로그 메시지가 우연히 그 단어를 포함하면 시험의 ③(평범한 줄에는 표지가 없다)이 흔들린다 — 현재 6줄에는 없다.
- 알림이 로그에 **한 줄로** 들어가므로, 메시지가 길면 줄이 길어진다. 줄바꿈은 템플릿의 `TextWrapping` 이 처리하지만 스크롤 부담은 늘어난다.

🗿 MoAI
