# GUI-C-154 (#225) — 행 6·11·12 구현. 행 15·16 은 남깁니다

상태: **행 6·11·12 완료**(각각 4단계 + 반증). **행 15·16 미착수** — 재귀 위험이 있어 따로 갑니다(§6).

집계: **20 → 17**(세 행). 전체 E2E: **162 통과 / 1 실패**, 그 1건은 기준선과 **같은** `P-10`(로컬 전용 판정 건).

---

## 0. 먼저 — 카드 §5 의 선행 지시에서 **제가 측정을 오염시켰습니다**

리더 지시는 *"행 6 착수 전에 E2E 전체를 한 번 돌려라"* 였습니다. 전체 실행(14분)을 배경으로 띄우고
**기다리는 동안 행 6 코드를 썼습니다.** 그 실행에서 **36건이 실패**했습니다.

원인은 회귀가 아니라 제 편집이었습니다. 픽스처에 신선도 가드가 있습니다:

```
System.InvalidOperationException : The gui app executable is older than its sources, so this run
would test the PREVIOUS build. exe ... written 09:20:28; newest source 'MainWindow.xaml' 09:31:24.
```

`ApplicationFixture.EnsureApplicationIsFresh` 가 정확히 제 역할을 했습니다 — **없었다면 낡은 exe 로 초록이 나왔을 것**입니다.

**다시 잰 방법**: 행 6 변경을 scratchpad 로 빼고 `git checkout HEAD -- <두 파일>` 로 되돌린 뒤 재빌드해
**행 4 만 들어간 상태**로 전체를 돌렸습니다 → **162 통과 / 1 실패**(그 1건이 `P-10`). 리더가 요구한
*"행 4 가 다른 시나리오를 깨뜨리는가"* 의 답은 **아니오**입니다.

교훈을 메모리에 남겼습니다: **측정이 도는 동안 그 측정이 읽는 트리에는 쓰지 않는다.**

---

## 1. 행 6 — P/Invoke 스모크

### 1.1 만든 것

`RunPInvokeSmokeTestCommand`. 프로브 셋: `xpe_alloc_image`/`xpe_free_image` 왕복, `xpe_display_version`,
`xpe_get_pending_alert_count`. 각각 따로 `try` 해서 **하나가 없다고 나머지 상태가 가려지지 않게** 했습니다.

**백엔드를 일부러 우회합니다.** 다른 네이티브 경로는 전부 `IXpeBackend` 를 지나는데 그것은 DLL 적재 실패 시
목으로 떨어집니다 — 그 추상 위에 스모크를 지으면 **아무것도 네이티브가 안 돌았는데 성공을 보고**합니다.
이 명령이 막으려는 것이 바로 그것이라 `DllImport` 를 직접 부릅니다.

### 1.2 반증 — **빨갛게 만들 수 있습니다** (카드 §2 의 핵심)

같은 앱·같은 인자로 두 팔:

| 팔 | `PInvokeSmokeTestPassed` | 보고서에 남은 사유 |
|---|---|---|
| **A** `XPE_NATIVE_DIR` = 실제 DLL 디렉터리 | **True** | `xpe_alloc_image/xpe_free_image -> ok; xpe_display_version -> '1.0.0'; xpe_get_pending_alert_count -> 0` |
| **B** `XPE_NATIVE_DIR` = **빈 디렉터리** + `XPE_NATIVE_DIR_EXCLUSIVE=1` | **False** | `xpe_alloc_image threw DllNotFoundException: Unable to load DLL 'xpe_common.dll' …` |

`'1.0.0'` 은 실제 DLL 의 버전입니다(목은 `v0.0.0-mock-display`) — **네이티브가 돌았다는 증거**입니다.

### 1.3 사유를 보고서에 실었습니다

처음엔 `PInvokeSmokeTestPassed` 하나만 넣었는데, **`false` 만으로는 다음 사람이 다시 돌려야** 합니다 —
게다가 그 실행은 재현하기 어렵습니다(빈 디렉터리를 핀으로 걸어야 함). 그래서 프로브별 결과를
`PInvokeSmokeTestDetail` 로 함께 기록합니다.

### 1.4 **게이트에는 넣지 않았습니다 — 리더 판단 사항**

`report.Passed` 에 넣으면 **CI 의 `gui-automation`(Mock) 잡이 빨개집니다.** 실측했습니다:

| 실행 | `PInvokeSmokeTestPassed` | `Passed`(전체) |
|---|---|---|
| CI Mock 잡과 같은 형태(네이티브 디렉터리 지정 없음) | **False** | True |

그 잡에는 네이티브 DLL 이 없으므로 **스모크가 빨간 것이 정상**입니다. 게이트에 넣는 것은
`#214` 에서 제가 저지른 형태(전제가 성립하지 않는 구성에서 빨강)입니다. **기록만 하고 게이트는 리더가 정하십시오.**

## 2. 행 11 — Stop Processing

### 2.1 무엇을 하는지 정확히 적었습니다

멈추는 대상은 `ApplyDisplayPipelineAsync` 의 **배경 작업**입니다(체인 + 디스플레이, 손목 슬라이스에서 2.3–2.5초).
그 안의 네이티브 호출은 동기라 **중단되지 않습니다** — 취소가 하는 일은 **결과를 적용하지 않고 버리는 것**이고,
그래서 뷰포트와 상태가 실제로 화면에 있는 프레임을 계속 기술합니다.

*"네이티브 작업을 중단한다"* 고 적었다면 `#208` 이 두 카드에 걸쳐 걷어낸 종류의 허위 보증이 됐을 겁니다.

### 2.2 반증 — 두 경로 다 관측

| 경우 | 실측 |
|---|---|
| **진행 중에 누름** | `Render stopped; the result was discarded after 14 ms of work.` · `StoppedRenderCount = 1` |
| **아무것도 안 돌 때 누름** | `Stop: no render is in flight.` |

### 2.3 관측이 **제 구현의 결함을 잡았습니다**

첫 실행에서 "아무것도 안 돌 때" 경로가 *"Stop requested; the render in flight will be discarded."* 를 냈습니다 —
**끝난 렌더를 진행 중으로 본 것**입니다. 원인: 취소 토큰 소스를 렌더가 끝날 때 비우지 않아 계속 non-null.

`finally` 에서 **자기 소스일 때만** 비우도록 고쳤고, 다시 재서 `Stop: no render is in flight.` 로 바뀐 것을 확인했습니다.
코드 주석에 그 실측을 남겼습니다 — 다음 사람이 "왜 finally 가 필요한가" 를 다시 조사하지 않도록.

### 2.4 왜 UI 클릭이 아니라 명령으로 관측했나

합성 1024 프레임의 렌더는 **약 16 ms** 인데 메뉴를 여는 데 그보다 오래 걸립니다. 메뉴로 몰면
**가끔 지는 경주**가 되고 그것은 관측이 아니라 불안정한 시험입니다. `ApplyDisplayPipelineAsync` 는
첫 `await` **이전에** 취소 소스를 만들므로, 바로 다음 줄의 Stop 은 **항상** 진행 중에 닿습니다.
자동화 스크립트 주석에 이 이유를 적었습니다.

## 3. 행 12 — Stage Timing

### 3.1 **새로 재지 않습니다**

값은 `PipelineTimings`(렌더가 쓴 것)와 `ChainStatus`(체인이 쓴 것)에서 가져옵니다. 이 명령이 따로 재면
상태바와 **어긋나는 두 번째 수치**가 생기고, 읽는 사람이 그 차이를 해소할 방법이 없습니다 —
`#201` 이 이름 붙인 형태입니다.

### 3.2 실측

```
work=18 ms; vm=1 ms; display: marshal-in=2 ms, native=3 ms, marshal-out=1 ms, preview=10 ms
  || chain: preprocess=RequestedNotApplied, gsvg=NotRequested
```

렌더 전에 누르면 `Stage timing: no render has run yet.` — *"아직 없다"* 와 *"렌더가 아무것도 보고하지 않았다"* 는
다른 상태이므로 빈 줄 대신 그렇게 말합니다.

## 4. 집계 (카드 4단계 중 넷째)

앱을 **실제로 실행**해 쟀습니다.

| | `DisabledFutureCommandCount` |
|---|---|
| 행 4 까지(= `HEAD`) | **20** |
| 행 6 추가 | **19** |
| 행 11·12 추가 | **17** |

`Passed` 는 양쪽 구성에서 **True** 유지(Native·Mock 형태 모두 실측).

## 5. 회귀 — 전체 E2E

렌더 경로(취소 배선)를 건드렸으므로 전체를 돌렸습니다.

| 실행 | 결과 |
|---|---|
| **행 4 만**(기준선, 재빌드 후) | **162 통과 / 1 실패** — `P10_ThePreviewChange_KeptTheDrawnPixels`, `hash=36fc547e253b07f1` |
| **행 6·11·12 포함** | **162 통과 / 1 실패** — **같은 시험, 같은 해시** |

**차이 0.** 그 1건은 리더가 CI 로 로컬 전용 판정한 건입니다(제 DLL 스테이징이 낡음).

빌드: `gui/ImageProcTest` 오류 0, `ImageProcTest.E2ETests` 오류 0.

## 6. 왜 행 15·16 을 남기는가

앱이 테스트 러너를 부르는 항목이라 **재귀**(CI 의 `gui-e2e-native` 가 앱을 띄우고 그 앱이 다시 E2E 를 부름)를
먼저 설계해야 합니다. 이 카드에서 세 행에 붙은 관측·수정이 적지 않아, 섞지 않고 따로 갑니다 —
카드 §1 이 *"한 번에 하나씩, 그쪽이 정한 속도로"* 라고 허용한 범위로 이해했습니다.

`IsEnabled` 는 **그대로 비활성**입니다.

## 7. 미검증 / 잔여 위험

- **CI 에서 안 돌았습니다.** 전부 로컬 — **로컬 실측, CI 상한 추정**
- **행 6 스모크는 `Passed` 게이트에 없습니다**(§1.4). 기록은 되지만 **아무것도 막지 않습니다** — `#205` 형태가 되지 않으려면 리더 판단이 필요합니다
- 행 11 의 취소는 **결과 폐기**이지 네이티브 중단이 아닙니다. 긴 네이티브 호출 중 앱을 닫는 경우는 재지 않았습니다
- 행 11 을 **연달아 여러 번** 누르거나 렌더 중 모드 전환(행 4)과 겹치는 경우는 재지 않았습니다
- 행 12 는 표시 문자열이 상태바 값과 **같은 출처**임을 코드로 보장했을 뿐, 두 화면을 나란히 놓고 대조한 E2E 는 없습니다
- 세 명령 모두 **E2E 시나리오를 추가하지 않았습니다.** 관측은 자동화 보고서(=CI 의 `gui-automation` 이 돌리는 경로)에 실려 있지만, 값이 **단언되지는 않습니다**
- `P-10` 로컬 전용 판정은 리더의 CI 관측을 인용한 것입니다

---

Refs #225
