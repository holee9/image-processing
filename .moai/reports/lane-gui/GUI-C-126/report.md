# GUI-C-126 — 네이티브 알림 (#198 ②) 과 재초기화 구분선

## 1. 가장 먼저 — **② 는 이미 배선돼 있었고, 실행으로 확인했다**

카드의 전제는 *"`gui/ImageProcTest` 전체에서 `xpe_alert*`·`alert_pop`·`alert_drain` 0건 → 네이티브 알림을 GUI 가 한 번도 읽지 않는다"* 였다. **전제가 틀렸다.**

grep 이 0건을 낸 것 자체는 맞다 — **GUI 가 쓰는 이름이 그 패턴에 안 걸린다:**

| 실제 사용 이름 | `xpe_alert*` 에 걸리나 |
|---|---|
| `xpe_get_pending_alert_count` | 아니오 |
| `xpe_get_pending_alert` | 아니오 |
| `xpe_clear_alerts` | 아니오 |
| `xpe_alert_push` (모듈이 미는 쪽) | 예 — GUI 는 안 쓴다 |

배선은 셋 다 있었다:

```
XpeDisplayInterop.cs:92-102   세 함수 DllImport 선언 (주석: "#110 alert queue ... only a binding gui lacked")
RealXpeBackend.cs:221         InvokeNative -> NativeAlertDrain.InvokeWithDrain(call, DrainNativeAlerts)
RealXpeBackend.cs:225-252     count -> Drain -> _alerts -> xpe_clear_alerts()
MainWindowViewModel.cs:1679   _drainedBackendAlertCount 이후분을 RaiseAlert 로
```

**그리고 실행으로 확인했다.** 가상 그리드를 네이티브로 돌린 직후 로그:

```
[15:02:35.250] ALERT WARN NATIVE_ALERT: gsvg virtual grid: no collimation field mask; scatter outside the field is treated as object
[15:02:35.232] Drained 1 native alert(s) from the xpe_common queue.
```

`modules/gsvg/src/gsvg.cpp:463` 이 `xpe_alert_push` 로 민 것이 화면까지 왔다. **C-125 가 만든 `RaiseAlert` 자리가 그것을 보이게 했다** — 그 전에는 같은 알림이 `Alerts` 컬렉션에만 들어가 보이지 않았다.

**모듈 쪽에 필요한 변경은 없다.** `modules/**` 는 읽기만 했다.

### 그래서 이 카드가 실제로 한 것

배선을 만드는 대신 **그 배선을 지키는 시험**을 세웠다. 없으면 배선을 지워도 아무 시험도 울지 않는다 — 오늘 여러 번 만난 형태다.

## 2. (a) API — 소유권과 파괴성

`modules/common/include/xpe/common/xpe_error.h` 를 읽었다.

| 함수 | 성질 |
|---|---|
| `xpe_get_pending_alert_count()` | 비파괴. 큐 개수만 |
| `xpe_get_pending_alert(index, msg, msgLen, severity)` | **비파괴** — "The alert queue is not modified". 버퍼는 **호출자 소유**(우리가 `StringBuilder` 로 준다), 잘림 없이 `XPE_ERR_BUFFER_TOO_SMALL` 로 거부 |
| `xpe_clear_alerts()` | **파괴적.** 드레인은 이 호출로만 일어난다 |

**즉 읽기는 비파괴이고 파괴는 우리가 명시적으로 부르는 시점에만 일어난다.** 문자열 해제 책임은 우리에게 없다(모듈이 복사해 준다). 큐가 넘치면 **몇 개 버려졌는지 말하는 합성 알림 1개**가 슬롯 하나를 차지한다(헤더 주석).

**드레인 시점**: 기존 설계 그대로 두었다 — **네이티브 호출마다**(`InvokeNative` 가 감싼다). 이유는 코드에 적혀 있고 타당하다: *"alert drain is a property of 'calling native' rather than of one call site"*. 주기적 폴링이면 조용한 구간이 생기고, 파이프라인 끝에만 걸면 다른 경로(프리셋·버전 조회)가 민 알림을 놓친다.

**카드가 경고한 소진 함정**: 읽기가 비파괴이고 `xpe_clear_alerts` 는 드레인 직후에만 불린다. 큐는 **프로세스별**이고 픽스처마다 앱을 새로 띄우므로, 한 실행이 다른 실행의 알림을 먹을 수 없다. 그 사실을 시험 주석에 적었다.

## 3. (b) 구분선 — 리더 판정대로

`InitializeBackend` 가 `Logs.Clear()`·`Alerts.Clear()` 하던 것을 **지우지 않고 경계를 쓰는** 것으로 바꿨다.

```csharp
if (Logs.Count > 0 || Alerts.Count > 0)
{
    Log("--- backend re-initialised ---");
}

_drainedBackendLogCount = 0;      // 그대로 — 백엔드 상태
_drainedBackendAlertCount = 0;
```

**낡은 주석 둘도 고쳤다.** 생성자의 `#161` 주석이 *"InitializeBackend clears Logs and Alerts"* 라고 말하고 있었는데 이제 거짓이다. C-122 의 측정 케이스 주석도 같은 이유로 갱신했다(그 케이스가 재는 값이 뒤집혔다).

### `#161` 소스 가드가 정확히 잡았다

```
StartupRejectionSurvivesTests.InitializeBackend_IsWhatClearsTheLog [FAIL]
Not found: "Logs.Clear();"
```

그 시험의 주석이 스스로 적어 둔 말 그대로다 — *"A guard whose reason has quietly evaporated is worse than no guard."* **지우지 않고 뒤집었다:**

- 전제 단언 → `InitializeBackend_MarksABoundaryRatherThanErasingTheRecord`: `Logs.Clear()`·`Alerts.Clear()` 가 **없을 것**, `backend re-initialised` 가 **있을 것**, 그리고 **대조군으로 드레인 커서 두 줄은 여전히 0 으로 리셋될 것**
- 순서 단언 → 유지하되 사유를 바꿨다. 이제 지워지지는 않지만, `InitializeBackend` 보다 먼저 말하면 **구분선 위**에 놓여 "이전 백엔드의 것" 으로 읽힌다(첫 초기화에는 이전 백엔드가 없다)

## 4. 단언 — 양방향

**양성** (`NativeAlertScenarios`, 네이티브 전용):

```
before: 39 lines, 1 carrying the gsvg alert
after : 50 lines, 2 carrying the gsvg alert
native alert line: [15:10:08.577] ALERT WARN NATIVE_ALERT: gsvg virtual grid: no collimation field mask; ...
drain line       : [15:10:08.562] Drained 1 native alert(s) from the xpe_common queue.
```

**증가분으로 단언한다.** 처음에는 "이전 0건" 으로 썼다가 빨강이 났다 — 픽스처를 공유하므로 앞선 케이스가 남긴 줄이 있었다. 0 을 요구하면 **다른 케이스의 순서에 시험이 좌우된다.** 이 실행이 새로 만들었는지를 묻는 것이 원래 물어야 할 질문이다.

**음성·대조군**: 가상 그리드 전에는 그 줄이 안 늘고, **로그 자체는 차 있다**.

**재초기화**: 구분선이 들어가고 **그 위의 네이티브 알림이 살아 있다**.

**C-125 가 안 깨진다**: 같은 줄이 `ALERT` 와 `NATIVE_ALERT` 를 달고 온다.

## 5. 반증 — 관측을 앞에

`NativeAlertDrain.Drain(...)` 결과를 빈 목록으로 바꿔 배선을 끊었다.

```
before: 23 lines, 0 carrying the gsvg alert
after : 33 lines, 0 carrying the gsvg alert
native alert line: (none)
→ The native module pushed an alert and nothing new reached the screen: 0 -> 0 (#198 ②).
```

줄 수는 23 → 33 으로 늘었다 — **다른 로그는 그대로 흐르는데 알림만 사라진다**는 것이 기록에 남는다.

## 6. 빌드·시험

```
BUILD_EXIT=0
Mock  E2E 전체   : 통과 124, 건너뜀 23, 실패 0 (5m 13s)
Native 관련 8건  : 통과 8, 실패 0
IntegrationTests : 통과 254, 건너뜀 1, 실패 0
```

건너뜀이 21 → 23 인 것은 이번에 더한 네이티브 전용 2건이다.

## 7. 미검증 (Gaps)

- **`#194`·`#196` 이 넣은 알림 자체는 띄우지 못했다.** 쓴 것은 `modules/gsvg` 가 미는 알림이다. 그 둘은 `modules/preprocess` 이고, GUI 의 전처리 경로는 교정 파일이 있어야 돌며 스테이징된 DLL 이 그 알림을 포함하는지도 확인하지 않았다. **경로가 같다는 것은 보였지만 그 두 알림으로 보이지는 않았다.**
- **큐 넘침의 합성 알림**은 재지 않았다(헤더에만 근거).
- `xpe_get_pending_alert` 가 `XPE_ERR_BUFFER_TOO_SMALL` 을 내는 경우는 재지 않았다 — 버퍼 길이는 기존 코드의 값을 그대로 썼다.
- **구분선이 여러 번 쌓일 때**의 모습은 보지 않았다(재초기화 2회 이상).
- 로그는 여전히 **View ▸ Logs(기본 Off) + Log 탭** 두 단계 뒤다.

## 8. 잔여 위험

- 알림과 일반 로그가 한 목록에 섞인다. **알림만 거르는 수단은 없다** — 많아지면 `#198` 이 다시 볼 자리다.
- 구분선은 `Logs` 에만 들어간다. `Alerts` 컬렉션에는 경계가 없다 — 그 컬렉션을 보여 주는 화면이 생기면 같이 봐야 한다.

🗿 MoAI
