# GUI-C-189 — 네이티브 C-09 첫 관측: 여섯 번 실패해도 워커가 꺼지지 않음 (main CI 빨강) (#225 행 10, #130)

레인: gui · 푸시 없음(푸시는 리더). 카드: 메인 `.moai/lanes/gui/inbox/GUI-C-189.md`. 5절 형식(주장 / 증거 / 기준 귀속 / 미검증 / 잔여 위험). 이 위에 GUI-C-186f(`a5e2954e`)가 있다.

## 1. 주장

1. **원인은 아직 모른다.** 로컬에서 네이티브를 돌릴 수 없고(#98), 코드를 읽어서는 두 가설 — (a) 모듈이 워커 경로가 아니었다 / (b) 모듈은 세었는데 화면이 못 보여 줬다 — 를 가를 수 없었다. 가를 수 있는 관측을 **먼저 설계하고** 앱과 C-09 에 넣었다. 다음 네이티브 CI 한 번이 원인을 가른다.
2. **단언은 느슨하게 하지 않았다.** C-09 는 여전히 표시를, 모듈이 보고한 두 수가 같음을, 재시작 뒤 `worker=Active; failures=0` 을 요구한다. 진단은 기록하고 실패 메시지에 싣기만 하며, 어떤 단언도 그것을 읽지 않는다(시험이 대조한다).
3. 리더 가설(다른 경로가 먼저 init 해서 워커 설정이 무시됨)은 **이 저장소의 코드로는 뒷받침되지 않는다**(§2-2). 확정은 아니다: 같은 프로세스의 다른 로더가 부르지 않았다고 증명한 것은 아니다. 그래서 init 직전의 모듈 상태를 직접 묻는 점검을 넣었다.

## 2. 증거

### 2-1. 관측 (리더가 CI 로그에서 읽음)

main `53ec370a` CI run 36895299404, 잡 `gui-e2e-native`: C-09 만 실패. 여섯 번 모두 `ai_bone_suppress=RequestedNotApplied … NOT applied (code -3)`, 시간 47·15·8·9·8·9 ms. `-3` = `XPE_ERR_PROCESSING_FAILED`. (나는 CI 로그를 직접 읽지 않았다.)

### 2-2. 코드로 확인한 것

- **워커 경로의 모든 실패는 세어진다**: `ai.cpp:890-925` — `useWorker` 가 참이면 non-OK 결과마다 `workerConsecutiveFailures` 가 오르고 3번째에 `workerDisabled`; 이후 호출은 즉시 `-3`(`:891-893`). 상태는 호출이 끝날 때 `publishWorkerState` 로 공개된다(`:513-517`). 따라서 워커 경로였다면 3번째 호출 뒤에 `xpe_ai_worker_state` 가 Disabled 를 답해야 한다.
- **`useWorker` 가 거짓인 호출은 세지 않는다**: 같은 함수의 동일 위치 아래 in-process 경로(`:927-`)에는 실패 계수도 알림도 없다. 스텁 빌드에서는 그 경로가 "매 호출 -3"(`ai.cpp` 주석 "A stub build lands here every time")이다.
- **설정 해석은 두 파서 모두 `{"use_worker":true}` 를 받는다**: nlohmann 쪽 `ai.cpp:281`, 최소 파서 쪽 `:337-343`. GUI 는 `System.Text.Json` 으로 `[JsonPropertyName("use_worker")]` 를 직렬화한다(`GuiAiRunner.cs` `AiConfig`). 즉 "키 철자나 형식이 틀려 무시됨"은 코드상 배제된다(실행해 본 것은 아님).
- **init 호출자**: 이 저장소에서 `xpe_ai_init` 를 **부르는** 비시험 코드는 `GuiAiRunner.cs:117` 하나다(검색: `modules`·`gui`·`clients` 의 `*.cpp *.h *.hpp *.cs`, 시험 제외; 나머지는 정의·헤더·주석). C-08 은 모델 파일이 없어 GUI 의 파일 확인에서 멈추므로(`CheckModelFileAt`) 모듈을 먼저 초기화하지 않는다(C-08 소스와 주석).
- **네이티브 E2E 가 쓰는 `xpe_ai.dll` 은 post 빌드 산출물이다**: `ci.yml` 스테이징이 `xpe-ci-{common,preprocess,post}-binaries` 를 받고 AI DLL·worker 는 post 쪽에 있다(스테이징 주석 "Both ship in xpe-ci-post-binaries already"). `ci-post` 프리셋은 `BUILD_AI=ON` 이고 ONNX 옵션이 없어 모듈 CMake 의 기본값(`XPE_AI_USE_ONNXRUNTIME=OFF`, `XPE_AI_STUB_BUILD=ON`)이 적용된다 → **스텁 빌드일 가능성이 높다**. DLL 의 import 표를 열어 확인한 것은 아니다. (ONNX 가 링크된 것은 별도 잡 `ci-ai` 이고 그 산출물은 이 E2E 에 오지 않는다.)
- **시간은 가르지 못한다(추론)**: 2~6번째 호출의 8~9 ms 가 거의 같다. 워커를 매번 띄우다 실패했다면 2·3번째가 더 걸렸을 것이라 "모듈이 매번 즉시 반환"쪽을 약하게 지지한다. 그러나 3072²×4 바이트 버퍼 둘의 할당·복사만으로도 이 정도가 나올 수 있어(재 보지 않았다) 근거로 쓰지 않는다.

### 2-3. 무엇을 찍으면 가설이 갈리는가 (넣기 전에 적은 표)

| 찍는 것 | 값 | 의미 |
|---|---|---|
| 첫 `xpe_ai_init` **직전** `xpe_ai_worker_state` 코드 | `-6`(미초기화) | 우리 init 전엔 비어 있었음 → "다른 경로가 먼저 init" 기각 |
| 〃 | `0` | 누군가 먼저 초기화함 → 리더 가설 확정, 그때의 `state` 가 단서 |
| init 호출 번호·설정 JSON·반환 코드 | `{"use_worker":true}`, 코드 0 | GUI 가 설정을 정상 전달함 |
| 호출 뒤 읽기마다 `worker_state` 원본 `code/state/failures/ceiling` | `state=0`(NOT_USED) | 모듈이 워커 경로가 아님 → **모듈 쪽**(post) |
| 〃 | `state=1`, failures 가 1·2·3 으로 증가 뒤 `state=2` | 모듈은 정상, 화면(배너) 문제 → **GUI** |
| 〃 | `state=1`, failures 가 계속 0 | 호출이 세어지지 않음 |
| 〃 | `code≠0` | 모듈이 상태 조회를 거부 → GUI 는 Unknown 으로 읽음 |

### 2-4. 넣은 것

| 조각 | 위치 | 내용 |
|---|---|---|
| 모듈의 자기 보고 | `GuiAiRunner.cs` `GuiAiSession` | `InitCore` 가 init 직전에 `xpe_ai_worker_state` 를 한 번 묻고(읽기 전용, 같은 잠금 안), init 호출을 `init#N dir=… config=… before-init … -> code=C` 로 기록; `QueryWorkerState` 가 읽기마다 원본 `read#N worker_state code= state= failures= ceiling=` 를 상태에 싣는다 |
| 상태 | `AiWorkerStatus` | 선택 필드 `Diagnostics`(요약·표시·배너는 이것을 읽지 않음; 시험이 대조) |
| 자동화 표면 | `AnalysisPanel.xaml` | AI 체크박스의 `AutomationProperties.ItemStatus` 에 바인딩(보이는 요소 아님; HelpText 의 `worker=` 요약 형식은 그대로) |
| C-09 | `ProcessingChainScenarios.cs` | 시도마다 요약·진단·배너 유무를 출력하고 실패 메시지에 모두 싣는다. 읽기 실패는 삼켜 "(unreadable …)" 로 적는다(진단이 다른 이유로 빨갛게 만들지 않게). 도우미는 Mock 의 C-08 에서 매번 실행된다 |

`xpe_ai_worker_state` 호출 지점이 둘이 됐다(조회 + init 사전 점검). 기존 구조 시험("한 곳, 잠금 안")을 "정확히 둘, 둘 다 잠금 안"으로 바꿨다 — 사전 점검은 `InitCore` 의 잠금 람다 안에 직접 넣었다(별도 도우미로 빼면 시험이 렉시컬하게 잠금 안임을 보증하지 못한다).

### 2-5. 시험 (`AiDiagnosticsTests` 5건, 소스·순수)

점검이 init **앞**에 있고 같은 잠금 안·기록이 뒤, 읽기가 원본 값을 싣고 "GUI 가 스스로 답함"을 구별, 진단이 체크박스 `ItemStatus` 로 나가고 보이는 요소가 아님, `Diagnostics` 가 요약·표시·배너를 바꾸지 않음, **C-09 가 여전히 표시·두 수의 동일·재시작 뒤 Active 를 단언하고 어떤 단언도 진단을 읽지 않음**.

### 2-6. 반증 5팔 (실제 소스, 빌드 성공, 소스 바이트 동일 복구)

| 팔 | 빨강 |
|---|---|
| 점검이 init 뒤로 | 시험 1건 |
| 읽기가 원본 값을 안 실음 | 1건 |
| `ItemStatus` 바인딩 제거 | 1건 |
| C-09 가 표시 단언을 포기(`Assert.True(true, …)`) | 1건 |
| 단언이 진단을 읽기 시작 | 1건 |

(`falsification_arms.txt`; 시작·끝 소스 해시 `4356771fe7a0 3658a97b3102 184767ac2903` 동일.)

### 2-7. 로컬 실행

두 솔루션 빌드 0 오류. `Category=Functional` **351 통과 · 건너뜀 1(기존) · 실패 0**. E2E(Mock): **23 통과 · 건너뜀 7(Native 전용 등, 기존) · 실패 0**, 앱 프로세스 잔류 0. C-08 이 `AiDiagnostics` 도우미(빈 문자열)를 Mock 에서 실행해 통과했다. 이 E2E 는 실행 중 소스를 건드리지 않았다.

## 3. 기준 귀속 (측정 대상)

모든 숫자는 이 트리(`dev/gui` `a5e2954e` 위 작업 트리)에서 이 실행으로 얻은 것이다. 네이티브 값은 하나도 관측하지 못했다.

## 4. 미검증

1. **원인.** 위 표의 어느 칸인지는 다음 네이티브 CI 가 말한다. 그때까지 C-09 는 빨갛게 남는다(단언을 풀지 않았다).
2. **`ItemStatus` 가 네이티브 실행에서 값을 싣는 것**은 못 봤다. Mock 에서는 빈 문자열이 읽힘을 C-08 이 확인하지만 비어 있지 않은 값이 UI Automation 으로 나가는지는 네이티브 CI 가 처음이다. 읽기 실패는 메시지에 "(unreadable …)" 로 남는다.
3. **post 빌드가 스텁이라는 것은 프리셋·CMake 기본값을 읽은 추론**이다(DLL 을 열어 보지 않았다). 스텁이라면 워커 경로가 어떻게 동작해야 하는지(스텁 워커가 오류 프레임을 보내는지, 아예 못 뜨는지)도 읽지 않았다.
4. 사전 점검 호출(`xpe_ai_worker_state` 를 init 전에)이 미초기화 모듈에서 안전하다는 것은 헤더와 `ai.cpp`(`checkInitialized` 가 `-6`)를 읽은 것이고 실행해 보지 않았다.
5. 2~6번째 호출의 8~9 ms 를 GUI 버퍼 작업으로 설명한 것은 재지 않은 추론이다.

## 5. 잔여 위험

- **진단 표면이 제품 코드에 남는다.** 자동화 트리의 항목 상태(운영자에게 안 보임)와 읽기 때 값 한 줄 만드는 비용이다. 원인을 찾은 뒤 지울지는 리더가 정한다. 지운다면 `AiWorkerStatus.Diagnostics`·`GuiAiSession` 의 세 필드·`AiWorkerDiagnostics`·XAML 한 줄·C-09 진단 줄·`AiDiagnosticsTests` 를 같이 지우면 된다(구조 시험의 "둘" 은 "하나"로 되돌린다).
- 읽기마다 문자열이 달라(`read#N`) 상태 비교가 매번 "다름"이 되어 화면 갱신 알림이 매 읽기마다 나간다. 비용은 작지만 이전에는 같은 상태면 건너뛰었다.
- 원인이 모듈 쪽이면 이 카드는 고칠 수 없고 post 에게 근거와 함께 넘겨야 한다. 원인이 GUI 쪽이면 다음 카드에서 고친다.

## 증거 파일

`falsification_arms.txt` · `local_runs.txt` · `text_lint.txt`

🗿 MoAI
