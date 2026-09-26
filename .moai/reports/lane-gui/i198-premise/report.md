# `#198` 전제 훑기 — **① 은 문자 그대로 아직 참인데, 증상은 아니다**

> 고치기 전 전제 확인. **구현하지 않았다.** `#198` 은 두 주장이었고, 지금 상태는 둘 다
> 단순 참/거짓이 아니다.

## 1. 요약

| 주장 | 지금 |
|---|---|
| ① `Alerts` 는 **표시 요소가 없다** | **문자 그대로 참** — `Alerts` 바인딩 0건 |
| ① 의 증상: *"알림이 사용자에게 닿지 않는다"*(이슈 제목) | **거짓** — 로그로 닿는다, 두 번 측정 |
| ② 네이티브 알림을 GUI 가 꺼내지 않는다 | **거짓** — 이슈 본문이 이미 정정(`GUI-C-126`), `GUI-C-132` 가 재확인 |

**주장과 증상이 갈렸다.** ① 의 문장은 참이고 ① 이 만든 증상은 해소됐다 — `GUI-C-125` 가
`Alerts` 에 표시 자리를 준 것이 아니라 **알림을 로그로 흘려보냈기** 때문이다.

## 2. ① 을 다시 쟀다 — 대조군을 붙여서

```
grep -rn "Alerts" gui/ImageProcTest/Views/*.xaml gui/ImageProcTest/MainWindow.xaml
  MainWindow.xaml:161   주석 (#165, 옛 Alerts Panel 언급)
  MainWindow.xaml:200-203  ClearAlertsMenuItem  → ClearAlertsCommand
  MainWindow.xaml:482-485  ClearAlertsButton    → ClearAlertsCommand
  → Alerts 컬렉션 자체에 바인딩된 표시 요소: 0건

대조군 (같은 검색으로 Logs 는 잡히는가)
  AnalysisPanel.xaml : 6건
  MainWindow.xaml    : 15건
```

**대조군이 잡히므로 0 은 "검색이 헛돌았다" 가 아니다.** (`#198` 의 ② 가 정확히 그
함정으로 틀렸고, 오늘 `GUI-C-131` 에서도 한 번 더 걸렸다.)

## 3. 증상이 해소된 경로 — 로그

`RaiseAlert` 가 **두 곳**에 넣는다.

```
MainWindowViewModel.cs:1963  private void RaiseAlert(AlertEntry alert)
                     :1965      Alerts.Insert(0, alert);                       ← 보이지 않는 곳
                     :1966      Log($"ALERT {alert.Severity} {alert.Code}: …")  ← 보이는 곳
```

로그는 `LogListBox` 로 화면에 있고, **실제로 도달하는 것을 두 번 측정했다**:

```
GUI-C-129 (#194): ALERT WARN NATIVE_ALERT: 869755 pixel(s) fell outside the gain …
GUI-C-132 (#196): ALERT WARN NATIVE_ALERT: nonlinearity correction did nothing …
```

## 4. 남은 것 — **정확히 무엇인가**

### (a) `Clear Alerts` 는 사용자에게 아무 변화도 만들지 않는다

```
MainWindowViewModel.cs:121  ClearAlertsCommand = new RelayCommand(() => Alerts.Clear());
```

**보이지 않는 컬렉션만 지운다.** 로그 줄은 그대로 남는다. 즉 버튼·메뉴 항목을 눌러도
**관측 가능한 결과가 없다.** `#198` 이 지적한 비대칭(*"지우는 버튼은 있는데 보여 주는
곳이 없다"*)이 **방향만 바뀐 채 남아 있다** — 이제는 "보여 주는 곳은 있는데(로그)
지우는 버튼이 그것을 지우지 않는다".

이것은 이슈 본문에 없는 **새 문장**이다. 원래 ① 이 고쳐지는 과정에서 생겼다.

### (b) 로그는 알림을 **구별하지 않는다**

알림은 `ALERT <SEV> <CODE>:` 접두어를 단 로그 한 줄이다. 그래서:

- 심각도가 **글자**다 — 색·아이콘·필터가 없다
- 일반 로그와 **섞인다.** `GUI-C-132` 의 실행에서 로그 44줄 중 알림은 6줄이었다
- 알림 개수·미확인 표시가 없다

**이것이 결함인지 설계인지는 이 훑기가 판정할 것이 아니다.** 다만 `#198` 을 닫을 때
"알림이 닿는다" 와 "알림이 구별된다" 가 같은 주장이 아님을 적어 둔다.

## 5. `#199` 와의 결합도 — **묶여 있지 않다** (측정)

lead 물음: *축출이 실제로 사용자에게 보이는 증상을 만드는가.*

**아니다.** 근거는 드레인 시점과 건수다.

```
RealXpeBackend.cs:236   InvokeNative<T>(call) => NativeAlertDrain.InvokeWithDrain(call, DrainNativeAlerts)
NativeAlertDrain.cs:109 try { return call(); } finally { drain(); }
```

**모든 네이티브 호출 뒤 `finally` 에서 드레인한다** — 예외 경로까지 포함(`GUI-C-24`).
그리고 한 호출이 남기는 건수를 쟀다:

```
Drained 2 native alert(s)        ← 전처리 체인 1회 실행
```

전처리 체인 전체(offset → nonlinearity → gain → defect)가 **한 번의 `InvokeNative`** 이고,
그 안에서 `#196` 1건 + `#194` 1건 = **2건**이 쌓인 뒤 즉시 드레인된다. 상한은 64 다.

**즉 `#199`(프레임마다 래치가 풀림)는 큐를 채우지 못한다** — 프레임당 1건이 늘어나는 것이
아니라, 프레임당 1건이 **그 프레임의 드레인에서 바로 빠진다.**

**축출이 보이려면**: 한 번의 `InvokeNative` 안에서 **64건을 넘겨야** 한다. 타일·행 단위로
미는 단계가 생기면 그렇게 되는데, 측정한 두 알림은 프레임당/조건당 1건이다.

**그래서 `#198` 은 `#199` 를 기다릴 필요가 없다.** (`#199` 자체는 여전히 고칠 값이 있다 —
GUI 가 아닌 호스트, 또는 세분화된 알림이 생기면 그때 살아난다.)

## 6. 측정한 것과 안 한 것

```
Native (ClampAlertOnScreenScenarios): 통과 1  → "Drained 2 native alert(s)"
정적 census: Alerts 바인딩 0건 / 대조군 Logs 6+15건
```

## 7. 미검증 (Gaps)

- **64건 축출을 실제로 보지 않았다.** 한 호출에 2건이라는 것을 재고 상한 64를 읽어
  **추론**했다. 64건을 한 호출에 밀어 넣는 수단이 없어 관측하지 못했다.
- **`Clear Alerts` 를 눌러 본 것이 아니다.** 코드에서 `Alerts.Clear()` 만 부르는 것을
  읽었고, 화면에서 "아무 일도 안 일어난다" 를 자동화로 관측하지는 않았다.
- **알림이 로그 44줄에 섞여 스크롤로 사라지는 것을 재지 않았다.** `GUI-C-122` 가 다룬
  축이지만 여기서 다시 재지는 않았다.
- **Mock 에서는 재지 않았다.** `Drained 2` 는 Native 측정이고, Mock 에는 네이티브 큐가
  없어 이 경로가 존재하지 않는다.
- ②의 정정은 **이슈 본문과 `GUI-C-126`·`GUI-C-132` 를 인용한 것**이고, 이 훑기에서
  다시 측정하지는 않았다.

## 8. 닫을 수 있는가 — 판단 재료

| `#198` 을 이렇게 읽으면 | 상태 |
|---|---|
| "알림이 사용자에게 **닿지 않는다**" (제목) | **해소** — 두 알림 화면 도달 측정 |
| "`Alerts` 컬렉션에 **표시 요소가 없다**" (① 문장) | **아직 참** — 바인딩 0건 |
| "지우는 버튼과 보여 주는 곳이 **맞지 않는다**" | **아직 참, 방향이 바뀜** (§4a) |

**닫을지, §4 를 새 이슈로 떼어낼지는 lead 결정이다.** 이 훑기의 결론은
**`#199` 는 걸림돌이 아니다**(§5)와 **§4a 는 이슈 본문에 없던 새 문장이다** 두 개다.

🗿 MoAI
