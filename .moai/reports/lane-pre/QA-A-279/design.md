[2026-10-22 00:22 KST]
# QA-A-279 — W15b 진단 계측 보강 설계 (개정 2판, Reviewer #267 F267-1~5 반영)

Refs #278. 이 문서는 설계다. 코드·시험·제품 파일을 바꾸지 않았고 아무것도 실행하지 않았다. 리더와 Reviewer(#268) 판정 뒤에 구현한다.

## 개정 이력

| 판 | commit | 내용 |
|---|---|---|
| 1판 | 932010f4 | 초안: 200 ms 배경 writer, UTC 시간창 귀속, 추가 GUI 팔(역순·영점), 단순 단위 시험 |
| 2판 | 이 커밋 | 리더 결정 ①~③ 과 F267-1~5 반영. 배경 writer 폐기 → 측정 중에는 크기가 정해진 메모리 기록만, 관측 종료 뒤 내보내기. 추가 GUI 팔 삭제(4.1 관찰 표만). 새 단언 없음(COMPLETE/INCOMPLETE 별도 표시). 귀속 필드 확장과 귀속 한계 명시. 완결 프로토콜(헤더·꼬리표) 정의. 비 GUI 시험의 링크 전제와 변이 의미 명시 |

1판에서 다음이 틀렸거나 근거가 없었다: ① "측정 구간을 건드리지 않는다"(배경 스레드·메모리 기록도 간섭이다), ② 기록 0건 = "주입 없음"으로 읽는 것(변수 미전달·파일 실패·미완료와 구별되지 않음), ③ "IntegrationTests 가 이미 FaultInjectingBackend 를 소스 링크한다"(해당 csproj 에 그 링크가 없다. 다른 시험은 소스를 파일로 읽어 검사할 뿐이다), ④ 추가 앱 수 계산(4앱 vs 3앱 불일치). 이 판에서 정정한다.

## 0. 불변 조건

- 사전조건 `proxyShift >= 0.8 * 1500` (1200 ms)와 그 단언 식·문구, 화면 단언, 주입량 1500 ms, 비교 대상 proxy 중앙값, 각 군 3회, 앱 시작 방식, 원 단언의 실행 위치와 fail-fast 경로를 바꾸지 않는다. 지금처럼 두 앱을 순서대로 열고 닫은 뒤 `proxyShift` 를 계산하고 곧바로 `Assert.True(proxyShift …)` 를 실행한다.
- W15b 게이트에 새 단언을 넣지 않는다. 진단이 유효한지는 `COMPLETE`/`INCOMPLETE` 문구로만 출력하고 판정에 쓰지 않는다.
- `Describe()` 문자열(`faultInjection=off`, `faultInjection=display-pipeline-after:2 calls=1` 등)과 기존 `_calls` 의 의미를 바꾸지 않는다. 문자열이 같다고 동작이 같다는 뜻은 아니므로(F267), 기존 호출 경로의 분기 하나(기록 객체 null 검사)가 늘어난다는 점을 인정하고 6절에서 별도로 확인한다.
- U01 수용, #266 HOLD, 작업 트리 미커밋 gui 제품 변경 8개, `.claude/settings.json`·`.bak-*` 는 건드리지 않는다.

## 1. 근거 (QA-W15B-1 에서 가져온 사실만)

- 실패 실행(CI 39602814368)의 chain/display 중앙값 차이는 +1479 ms 로 주입 1500 ms 가 거의 그대로 들어갔다. proxy 중앙값 차이는 1006 ms 였다(1200 ms 에 194 ms 부족).
- 무지연 3회: proxy 2557 / 3855 / 2051 ms. 두 번째 열기의 prepare 가 1836 ms(다른 5회는 8~59 ms)였고 첫 열기가 더 무거웠다. 정렬 두 번째 값이 첫 열기의 2557 ms 가 되어 기준 중앙값이 올라갔다.
- 직전 실행(39591918725)은 무지연 중앙값 2005 ms, 지연 3784 ms, shift 1779 ms 였다.
- 두 보존 로그에는 주입 sleep 의 실제 호출 횟수와 시간이 없다. 그래서 "주입이 소실됐는가"와 "기준군이 흔들렸는가"를 로그만으로 가를 수 없다.
- 이 설계의 목적은 다음 실패에서 그 두 가지를 로그로 가를 수 있게 하는 것이다. 원인이 무엇이었는지는 아직 입증되지 않았다.

## 2. 범위 확인과 결정이 필요한 곳

| 파일 | 소유 | 미커밋 변경 | 변경 내용 |
|---|---|---|---|
| `gui/ImageProcTest/Services/FaultInjectingBackend.cs` | gui | 없음(확인함) | 기록 객체, 기록·내보내기, 읽기·분류기. 모두 `#if XPE_TEST_FAULTS` 안 |
| `clients/ImageProcTest.E2ETests/Scenarios/Operator/QuickStartWalkthroughScenarios.cs` | clients | 없음(확인함) | W15b 출력, `GuiApp.Launch` 의 선택 인수, 지연 앱의 정상 종료 보조 |
| `clients/ImageProcTest.IntegrationTests/Functional/` 의 새 시험 파일 1개 | clients | 새 파일 | 비 GUI 시험 |

**범위 밖으로 드러난 것 (리더에게 묻는다):** 새 비 GUI 시험이 `FaultInjectingBackend` 를 실제로 만들고 부르려면 `ImageProcTest.IntegrationTests.csproj` 에 소스 링크 줄이 필요하다. 이 csproj 에는 지금 `FaultInjectingBackend.cs` 링크가 없다(Compile 줄 확인함). 따라서 "파일 2개 + 새 시험 파일 1개" 에 csproj 변경이 더해진다. 필요한 링크의 후보는 8절에 적었다. 허가가 없으면 비 GUI 시험은 만들 수 없고, 그 경우 계측 변이 증거는 W15b 의 CI 로그뿐이 된다(9절 결정 3).

귀속에 대해서는 4절에 한계를 적었다: 이 범위(앱이 proxy 의 시작·끝 시각을 내놓지 않음)에서는 sleep 이 **proxy 구간 안에** 있었다고 증명할 수 없다. 증명하려면 앱이 proxy 시작·끝 단조 시각을 내놓는 새 seam(`MainWindowViewModel.cs`, 미커밋 변경 8개 중)이 필요하다. 새 seam 은 추가하지 않는다.

## 3. 기록 수단 (F267-1, F267-3)

### 3.1 수명

1. **켜짐 조건:** 환경 변수 `XPE_TEST_FAULT_LOG`(파일 경로)가 있을 때만 기록 객체를 만든다. 없으면 기록 객체 자체를 만들지 않고, `DelayIfArmed()` 는 지금과 같은 `Thread.Sleep` 만 한다(차이는 null 검사 분기 하나).
2. **시작 시(측정 전):** 래퍼 생성 때 파일을 새로 만들고 헤더 한 줄을 쓴다(앱 시작 중이며 측정된 열기 이전). 헤더가 있으면 "변수가 전달됐고 기록이 켜졌다"가 확정된다.
3. **측정 중:** sleep 마다 크기가 정해진 배열(예: 256칸)에 구조체 하나를 쓴다. 파일 쓰기·스레드 생성·잠금 대기·문자열 만들기를 하지 않는다. 칸이 모자라면 `dropped` 만 센다.
4. **내보내기:** 측정된 열기와 기준 화면 관측이 모두 끝난 뒤, 래퍼의 `Shutdown()` 또는 프로세스 종료 훅(`AppDomain.ProcessExit`) 중 먼저 오는 쪽에서 한 번만(`Interlocked`) 배열을 파일 뒤에 덧붙이고 꼬리표를 쓴 뒤 `Flush(true)` 한다. 이 시점은 W15b 의 측정이 끝난 뒤다(7절 순서 참고).
5. **비용:** 켜졌을 때 남는 비용은 sleep 마다 `Stopwatch.GetTimestamp()` 2회와 배열 쓰기 1회, 그리고 null 검사 분기다. 0 이 아니다. 크기를 실측하지 않았으므로 숫자를 주장하지 않는다. 구현 뒤 CI 로그의 열기 시간을 직전 실행과 비교해 변화를 본다(미확인 항목).
6. **비대칭:** 무지연 앱은 `Wrap` 이 내부 backend 를 그대로 돌려주므로 래퍼도 기록도 없다. 무지연 군에 래퍼를 씌우면 측정 조건이 달라지므로 씌우지 않는다. 그 결과 지연 앱에만 기록 비용(위 5번)이 있다. 이 비대칭은 남는다. 배경 스레드와 파일 쓰기는 없으므로 1판의 "측정 중 CPU·IO 경쟁" 문제는 제거되지만, 비용 0 이라고 쓰지 않는다.

### 3.2 기록 항목 (F267-2)

| 필드 | 의미 |
|---|---|
| `run` | 시험이 정한 고유 run 번호(환경 변수 `XPE_TEST_FAULT_RUN`, 시험 시작마다 새 GUID) |
| `app` | 시험이 정한 앱 이름(예: `delay-1500`) — 환경 변수 `XPE_TEST_FAULT_APP` |
| `pid` | 프로세스 번호 |
| `wrapper` | 래퍼 생성 때 만든 GUID 하나 |
| `call` | 호출 시점의 지역 변수로 잡은 호출 번호(`Interlocked.Increment` 결과). 나중에 필드를 다시 읽지 않는다 |
| `overload` | `chain` 또는 `displayInput` |
| `requestedMs` | 요청한 지연 |
| `startTicks`, `endTicks` | `Stopwatch.GetTimestamp()` 값(단조 시계). `frequency` 는 헤더에 한 번 |
| `order` | 배열에 들어간 순서 |

UTC 시각은 항목마다 찍지 않는다(비용). 헤더에 `(utcAnchor, ticksAnchor)` 쌍 한 번을 적어 사람에게 보일 때만 환산한다.

헤더: `run`, `app`, `pid`, `wrapper`, `frequency`, `armed`(요청 지연값), `capacity`, `utcAnchor`, `ticksAnchor`.

꼬리표: `calls`(래퍼가 센 전체 display pipeline 호출 수), `events`(기록된 sleep 수), `dropped`, `errors`, `exportedBy`(`Shutdown` 또는 `ProcessExit`), 마지막 줄 `END`.

### 3.3 오류 정책

- 기록·내보내기 안의 모든 오류는 그 안에서 잡아 `errors` 로 센다. 원 backend 호출로 예외를 전파하지 않고, 원 backend 예외나 `CountAndMaybeFail` 의 의도된 예외를 가리지도 않는다. sleep 은 기록과 상관없이 지금처럼 요청한 만큼 한다.
- 기록 객체 생성(헤더 쓰기) 실패는 기록을 끄고 앱을 정상 시작한다. 시험은 헤더 부재를 보고 `INCOMPLETE` 로 읽는다.

## 4. 귀속과 읽기 (F267-2, F267-3)

### 4.1 환경 전달

`GuiApp.Launch` 는 이미 자식 프로세스마다 `ProcessStartInfo.Environment` 에 값을 넣는다(`XPE_NATIVE_DIR` 등, 같은 파일 1518~1519행). 시험 프로세스 전체 환경을 바꾸지 않고, **이 자식의** `info.Environment` 에만 `XPE_TEST_FAULT_LOG`(실행마다 새 임시 폴더 + 앱 이름의 새 파일), `XPE_TEST_FAULT_RUN`, `XPE_TEST_FAULT_APP` 을 넣는다. 이를 위해 `Launch` 에 선택 인수(`environment`)를 하나 더한다. 기존 호출부는 그대로다. 새 CLI 인수와 `AutomationArgs` 변경은 하지 않는다. 실제 전달은 CI 에서 확인한다(미확인).

### 4.2 내보내기를 기다리는 방법 (고정 sleep 금지)

`GuiApp.Dispose()` 는 `Close()` 직후 곧바로 `Kill()` 하므로(1806~1807행), 지금 그대로면 내보내기가 끝나기 전에 앱이 죽을 수 있다. 그래서 같은 파일의 `GuiApp` 에 `CloseGracefully(timeout)` 를 하나 더한다: `Close()` 후 **프로세스 종료를 기다린다**(`WaitForExit(timeout)`, 제한 시간 10초). 고정 sleep 으로 완료를 가정하지 않는다. 시간 안에 안 끝나면 그대로 두고 기존 `Dispose()` 가 하던 대로 `Kill()` 한다(진단 `INCOMPLETE`).

- 이 호출은 **지연 앱의 측정(`MeasureOpens`)이 끝난 직후, 그 `using` 블록 안에서만** 한다. 기준 화면 관측은 `MeasureOpens` 안에서 끝나므로 측정 뒤다.
- 앱 닫기는 `MainWindow.xaml.cs:202` 의 `BeginShutdown` 을 거쳐 backend `Shutdown()` 에 도달하는 것으로 읽었다. 실제로 도달하는지는 CI 에서 확인한다(미확인). 도달하지 않거나 시간이 초과되면 `ProcessExit` 훅이 대신 내보낸다. 둘 다 실패하면 꼬리표가 없는 파일이 남고 시험은 `INCOMPLETE` 를 출력한다.
- 이 단계는 예외를 잡아 삼키고 `Dispose()` 와 같은 정리 경로를 유지한다. 원 게이트의 단언 위치와 판정은 바뀌지 않는다.

### 4.3 완결 판정 (COMPLETE / INCOMPLETE)

시험은 파일을 읽어 다음 중 정확히 하나로 분류하고 한 줄로 출력한다. 단언하지 않는다.

| 분류 | 조건 | 뜻 |
|---|---|---|
| `INCOMPLETE: no-file` | 파일 없음 | 변수가 전달되지 않았거나 기록이 꺼졌거나 생성 실패 |
| `INCOMPLETE: no-header` | 파일은 있으나 헤더 없음/해석 불가 | 같은 이유 또는 손상 |
| `INCOMPLETE: stale` | 헤더의 `run`/`app`/`pid` 가 시험이 정한 값·실제 프로세스와 다름 | 이전 실행의 파일 |
| `INCOMPLETE: no-footer` | 헤더는 있고 꼬리표(`END`) 없음 | 미완료·강제 종료·내보내기 실패 |
| `INCOMPLETE: dropped=N` / `errors=N` | 꼬리표에 dropped·errors > 0 | 기록 손실 가능 |
| `INCOMPLETE: inconsistent` | 항목 수 ≠ 꼬리표의 `events`, 호출 번호 중복, 순서 어긋남 | 파일 내용 불일치 |
| `COMPLETE: events=N calls=M` (N ≥ 1) | 위 조건 모두 아님 | 기록이 온전하며 sleep 이 N 번 있었다 |
| `COMPLETE-EMPTY: events=0 calls=M` | 꼬리표 있고 N = 0 | 기대대로 비어 있음. `armed>0` 인데 `calls>0, events=0` 이면 "호출은 있었으나 sleep 이 없었다"는 **주입 소실의 직접 증거**가 된다. `armed=0` 이면 지연 0 설정의 기대 결과 |

이로써 "기록 0건"과 "빠짐·미완"을 구별한다. 이 분류는 비 GUI 시험에서 자동 단언하고(8절) W15b 에서는 출력만 한다.

### 4.4 열기와의 대응과 그 한계

- 시험은 열기마다 자기 프로세스의 `Stopwatch.GetTimestamp()` 를 두 번 잡는다: 파일 대화상자 확인 직전(`beforeConfirm` 콜백 자리)과 `FrameTimingText` 에서 `open-to-2nd-tick (proxy) #i:` 를 발견한 직후. 같은 기기에서 `Stopwatch` 는 `QueryPerformanceCounter` 이므로 두 프로세스의 틱을 같은 기준으로 비교할 수 있다는 것이 전제다. 이 전제는 헤더의 `(utcAnchor, ticksAnchor)` 와 시험 쪽에서 잡은 같은 쌍을 비교해 어긋남을 출력하는 것으로 검사한다(미검증).
- 항목마다 다음으로 분류해 출력한다: `inside`(시작·끝 모두 열기 창 안), `straddle-start`, `straddle-end`, `outside`. 열기마다 항목 수, 서로 겹치는 항목 쌍 유무, 합계를 출력한다. **예상 1건을 강제하지 않는다.**
- 이 열기 창은 proxy 구간보다 넓다(proxy 의 시작은 read 시작, 끝은 두 번째 렌더 틱이며 앱이 그 시각을 내놓지 않는다). 그래서 `inside` 라도 "proxy 구간 안"이 아니라 "시험이 본 열기 창 안"이라는 뜻이다.
- **차감값 규칙:** 어떤 열기에서 항목이 모두 `inside`, 서로 겹치지 않음, 개수 ≥ 1 이면 `proxy − Σ(sleep 경과)` 를 `참고치(관찰 전용, proxy 구간 포함은 증명되지 않음)` 라는 표지와 함께 출력한다. 하나라도 `straddle`/`outside`/겹침/음수이면 `N/A: AMBIGUOUS(<이유>)` 를 출력한다. 이 값을 "주입 외 시간"으로 부르지 않고 원 게이트를 대체하지 않는다.

## 5. 표본: 관찰 표만 (리더 결정 ②)

추가 GUI 팔(역순 ABBA, 지연 0 팔)은 넣지 않는다. 기존 6회가 끝난 뒤, 원 `Assert` 전에 유한 계산과 출력만 한다. 진단 계산·출력의 오류는 별도 상태 줄(`DIAGNOSTIC-ERROR: …`)로만 남기고 원 단언을 가리지 않는다.

- 열기 번호별 원값과 단계 분해: proxy, read, prepare, chain/display, pre, enhance, ui, render(지금 `FrameTimingText` 에서 이미 읽는 항목. `OpenMeasurement` 레코드에 필드를 더하되 기존 필드·순서는 유지).
- 각 군의 중앙값 표(proxy, chain/display). 게이트 값과 같은 식(정렬한 두 번째 값).
- 열기 번호별 짝 차이 `delayed[i] − baseline[i]`: 서로 다른 프로세스의 같은 번호 열기를 나란히 둔 표이며 인과 비교가 아니라고 표에 적는다.
- 첫 열기는 `cold/first-open, 게이트에 포함됨` 으로 표시한다. 제외 표본처럼 보이는 "warm-up" 이름을 쓰지 않는다. 열기 2·3 만의 평균은 필요하면 `DISCLOSURE ONLY` 로 출력하고 판정·표본 교체에 쓰지 않는다.
- 표본 수 3 은 바꾸지 않는다. 중앙값의 정의를 바꾸는 일이라 이 카드의 범위가 아니다.

추가 GUI 팔의 비용 불일치(1판의 4앱/9회 vs 3앱/9회)는 팔을 삭제하면서 해소됐다.

## 6. 불변 확인 방법 (구현 diff 에서)

구현 diff 에서 다음을 줄 단위로 확인해 보고서에 적는다.

1. W15b 본문의 `Median` 식, `proxyShift` 계산, 두 `Assert.True` 의 식·문구·위치·순서가 바이트 단위로 같다.
2. 두 `MeasureOpens` 호출의 인수(`opens: 3`)와 앱 시작 순서(무지연 → 지연)가 같다.
3. `Describe()` 와 `_calls` 증가 위치가 같다. `CountAndMaybeFail()` 호출 순서가 같다.
4. 기록 객체가 null(변수 없음)일 때 `DelayIfArmed()` 가 이전과 같은 `Thread.Sleep` 하나만 한다.
5. 출하 구성(`XPE_TEST_FAULTS` 미정의)에 새 형식이 컴파일되지 않는다. 출하 binary 검증은 아니다. 기존 `FaultSeamCompiledOutTests` 는 소스 문자열을 검사하는 시험임을 구분해서 적는다.

## 7. W15b 실행 순서 (원 단언은 그대로)

1. 무지연 앱 시작 → `MeasureOpens` 3회 → 닫기(지금 그대로).
2. 지연 앱 시작(기록 변수 포함) → `MeasureOpens` 3회 → **`CloseGracefully`** → 닫기.
3. 진단 읽기·분류·출력(4절, 5절).
4. 지금 그대로의 `proxyShift` 계산, 출력, `Assert.True(proxyShift …)`, 화면 단언.

3번은 `try/catch` 로 오류를 `DIAGNOSTIC-ERROR` 한 줄로 남기며 4번을 막지 않는다. 단언이 실패해도 3번의 출력은 이미 남아 있다(단언 위치를 옮기지 않으므로 추가 GUI 팔 때의 위험이 없다).

## 8. 비 GUI 시험 (F267-5)

### 8.1 링크 전제

`FaultInjectingBackend.cs` 는 `IXpeBackend`, `IAiSessionBackend`, `LoadedImageFrame`, `ChainResult`, `StageRequest`, `AppSettings`, `BackendRuntimeInfo`, `VoiPreset`, `TelemetrySnapshot`, `XpeBodyPartEnum`, `AiWorkerStatus`, `AiRestartResult`, `AutomationArgs` 에 의존한다. 이 csproj 에 이미 링크된 것: `ProcessingChain.cs`(`StageRequest`·`ChainResult`), `AppSettings.cs`, `AiBoneSuppressionStage.cs`(`AiWorkerStatus`·`AiRestartResult`), `BackendTelemetry.cs`(`TelemetrySnapshot`), `XpeBodyPartEnum.cs`, `AutomationArgs.cs`. 추가로 필요한 후보: `IXpeBackend.cs`, `IAiSessionBackend.cs`, `LoadedImageFrame.cs`, `VoiPreset.cs`, `BackendRuntimeInfo` 가 정의된 파일, 그리고 `FaultInjectingBackend.cs` 자체. 이 후보 줄 수와 연쇄 의존(예: `LoadedImageFrame` 이 끌고 오는 형식)은 컴파일해 봐야 확정된다. 확정 방식은 QA-A-277c 와 같다: 컴파일 오류가 없어질 때까지 필요한 링크만 더한다. 이 csproj 는 `Debug` 에서 `XPE_TEST_FAULTS` 를 정의한다(15행 주석). 새 시험 파일은 같은 `#if XPE_TEST_FAULTS` 로 감싸 Release 에서는 비어 있게 한다.

연쇄 의존이 네이티브 상호 운용 형식까지 커지면 이 접근을 멈추고 리더에게 다시 묻는다(새 seam 을 임의로 만들지 않는다).

### 8.2 시험 항목과 변이가 증명하는 것

가짜 `IXpeBackend`(호출만 세고 즉시 반환)를 `FaultInjectingBackend.Wrap` 으로 감싸 직접 부른다. 두 overload 를 모두 쓴다.

| # | 입력 | 단언 | 이 항목이 증명하는 것 |
|---|---|---|---|
| T1 | 지연 50 ms, `chain` overload 1회 + `displayInput` overload 1회, 내보내기 | `COMPLETE`, `events=2`, `calls=2`, `call` 이 1·2, `overload` 가 각각 맞음, `order` 증가 | 기록의 귀속 필드와 순서가 정확하다 |
| T2 | T1 과 같고 호출 앞뒤를 시험이 자기 `Stopwatch` 로 잰다 | 각 항목의 `[start,end]` 가 시험이 잰 호출 구간에 **포함**되고, `(end−start)/frequency` 가 기록된 경과와 일치하며, 요청값과의 차이가 허용 폭 이내 | 기록 시각이 실제 sleep 을 따라간다(자기 비교가 아니라 독립 기준: 바깥 시계) |
| T3 | 지연 0 (`display-delay:0`), 2회 호출 | `COMPLETE-EMPTY`, `events=0`, `calls=2`, `armed=0` | 지연 0 은 "기록 없음"이 아니라 "완결된 빈 기록"으로 보인다 |
| T4 | 지연 설정 없음(`Wrap` 이 그대로 반환) | 래퍼가 아님, 파일 없음 | 꺼진 경로는 기존과 같다 |
| T5 | 지연 50, 내보내기 호출 안 함 | `INCOMPLETE: no-footer` | 미완료를 "빈 기록"으로 오인하지 않는다 |
| T6 | 다른 `run` 의 헤더가 든 낡은 파일 | `INCOMPLETE: stale` | 이전 실행 파일을 읽지 않는다 |
| T7 | 배열 용량보다 많은 sleep | `INCOMPLETE: dropped=N` | 손실을 숨기지 않는다 |
| T8 | 지연 50 으로 두 번째 래퍼를 만들어(80 ms) 같은 폴더에 | 두 파일이 섞이지 않고 각자 `requestedMs` 50/80 | 래퍼·run 식별이 서로 다른 기록을 가른다 |

허용 폭은 아직 정하지 않았다. 구현 때 근거를 정해 실행 전에 보고서에 적는다: 기록된 두 단조 시각이 실제로 일관한지(T2 의 일치 검사), 바깥 시계와의 포함 관계, 그리고 `Thread.Sleep` 이 요청보다 조금 짧게 돌아올 수 있다는 점을 고정 숫자로 가정하지 않고 측정해서 정한다. 이 허용 폭은 W15b 의 1200 ms 기준을 완화하는 근거로 쓰지 않는다.

### 8.3 변이 (각각이 무엇을 잡는가)

| 변이 | 바꾸는 곳 | 실패해야 하는 항목 | 증명하는 것 |
|---|---|---|---|
| M1 지연 50 → 0 (무주입 대조) | 시험 입력 | T1·T2 의 positive 기대 | 시험이 "주입 없음"을 구별한다. 계측 결함이 아니라 입력의 대조 |
| M2 sleep 은 50 ms 로 하되 기록 경과를 0 으로 쓴다 | 기록 코드 | T2 | 기록된 경과가 실제 sleep 과 따로 놀면 독립 기준(바깥 시계)이 잡는다 |
| M3 첫 항목의 경과를 이후 항목에 재사용 | 기록 코드 | T8, T1 | 낡은 값 재사용을 잡는다 |
| M4 꼬리표를 쓰지 않는다 | 내보내기 | T5 와 모든 `COMPLETE` 기대 | 완결 표지가 실제로 판정에 쓰인다 |
| M5 `call` 번호를 항상 1 로 기록 | 기록 코드 | T1 | 호출 번호 귀속 |

변이는 임시 사본에서만 한다. 실행은 사용자 승인 뒤 비 GUI 시험 하나씩(동시 1개, 남은 프로세스 확인)만 하고, 승인 전에는 실행하지 않는다.

## 9. 리더에게 묻는 것

1. **csproj 링크 허가:** 8.1절의 링크 줄(`ImageProcTest.IntegrationTests.csproj`)이 범위 안에 들어가는가. 없으면 8절 시험을 만들 수 없다.
2. **앱 정상 종료 보조(`CloseGracefully`):** 4.2절처럼 `GuiApp` 에 보조 메서드를 더하는 것이 "기존 파일 2개" 범위의 변경으로 인정되는가. 지연 앱의 측정이 끝난 뒤에만 쓰며 원 단언 위치는 불변이다.
3. 8절 시험을 만들 수 없을 때(링크 불허 또는 연쇄 의존 폭증) 계측 검증을 W15b 의 CI 로그(`COMPLETE`/`INCOMPLETE` 출력)만으로 하는 것을 받아들이는가.
4. 비 GUI 시험 실행은 사용자 승인이 필요하다. 구현 지시 때 실행 직전에 스크립트·예상 변경·HEAD 를 보여 드리고 그때 한 번 받는다. 리더의 허가는 사용자 승인이 아니다.

## 10. 하지 않는 것

- 임계값 1200 ms, 주입 1500 ms, 두 원 단언, proxy 비교, 각 군 3회, 원 단언의 위치·순서를 바꾸지 않는다. 추가 GUI 팔을 넣지 않는다. 새 seam(앱의 proxy 시각, 자동화 트리 항목, 새 CLI 인수)을 넣지 않는다.
- 원인을 확정하지 않는다. 이 계측은 다음 실패에서 "주입 소실"과 "기준군 변동"을 가를 자료를 만들 뿐이다.
- 로컬 GUI E2E, 빌드, 시험, push 를 하지 않는다.

## 11. 미검증

- 앱 닫기가 backend `Shutdown()` 에 도달하는지와 `ProcessExit` 훅이 그 경로에서 실제로 도는지.
- 환경 변수가 자식 프로세스에 전달되는지(코드상 `info.Environment` 사용은 확인했으나 동작은 실행 전).
- 두 프로세스의 `Stopwatch` 틱이 같은 기준인지(4.4절 앵커 비교로 검사 예정).
- 기록 비용이 열기 시간에 주는 영향의 크기.
- 열기 하나에서 display pipeline 이 몇 번 불리는지(4.4절은 개수를 강제하지 않고 관찰한다).
- 8.1절의 연쇄 의존 범위.

## 정정 (구현 결과 반영, 원문은 위에 그대로 둔다)

QA-A-279c 구현의 1회 승인 실행(2026-10-22, HEAD 3718666f 사본)에서 변이 5개가 모두 시험에 걸렸다. 8.3절 표의 예상과 실제가 다른 곳을 적는다.

- **M4 (꼬리표 없음):** 8.3절은 실패 대상을 "T5 와 모든 COMPLETE 기대"로 적었다. 실제 실패는 T4·T12 를 뺀 10개(T1, T2, T3, T5, T6, T7, T8, T9, T10, T11)였다. T5 가 실패한 이유는 구현된 T5 의 **후반부**("정상 export 는 Complete 로 읽힌다")가 꼬리표를 요구하기 때문이다. T5 의 전반부(내보내기 호출 안 함 → `INCOMPLETE: no-footer`)는 M4 로 바뀌지 않는다. 따라서 "M4 는 T5 의 음성 대조 부분을 깬다"고 읽으면 틀리고, "M4 는 T5 의 양성 확인 부분과 모든 COMPLETE 기대를 깬다"가 맞다.
- **M1 (Begin 무기록):** T9(요청·호출은 있는데 기록 없음 → ANOMALY)는 통과했다. 변이 상태가 바로 T9 의 입력 상황이므로 기대대로다. 실패한 것은 T1, T2, T5, T7, T8, T10, T11 이다.
- **M2·M3:** T2 만 실패했다. 메시지는 각각 `call 1: recorded 0.0 ms for a real sleep of 60 ms`, `call 2: recorded 69.6 ms for a real sleep of 100 ms` 이다.
- **M5 (call 항상 1):** T1, T2 가 실패했다(T2 는 중복 호출 번호로 `Incomplete`).
- E2E 쪽 빌드는 시나리오 파일에 `using ImageProcTest.Services;` 가 없어 CS0103 9건으로 실패했고(`build/qa279c/logs/e2e-debug.log`), 한 줄 추가 뒤 재확인 전까지 E2E·W15b 코드는 컴파일이 확인되지 않은 상태다. 래퍼 배선은 계속 LOGGING-UNVERIFIED(배선)이다.
