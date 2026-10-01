# GUI-C-177 — "러너를 못 띄움" 과 "돌았고 실패" 를 공용 실행기에서 구별한다

레인: gui · 이슈: `#225` · GUI-C-176 보고서 §4-2 의 부수 한계를 고쳤다. 푸시는 리더가 한다(로컬 커밋만).

## 결론

공용 실행기가 세 가지를 서로 다른 문구로 말한다. 행 15·16·17 은 같은 실행기를 통하므로 한 곳에서 고쳤다.

| 결과 | 검증값 | 상태줄 | 언제 |
|---|---|---|---|
| **못 띄움** | `null` ("did not run") | `<라벨> did not run: '<exe>' could not be started (<사유>). Nothing was executed.` | 실행 파일이 없음, `ctest` 가 PATH 에 없음, 텍스트 파일을 프로그램으로 지정, 시작 예외 |
| **돌았고 실패** | `false` | `<라벨> FAILED (exit N): <줄>` | 시작했고 종료 코드 ≠ 0 |
| **돌았고 통과** | `true` | `<라벨> passed in N ms.`(행 17 은 ctest 요약이 이어짐) | 종료 코드 0 (문구는 전과 같다) |

그리고 시작한 뒤 출력을 읽다 예외가 나면 `ended abnormally`(false) — "돌았다" 쪽이다. PATH 에 없는 이름이면 사유에 *"the file was not found; a bare name such as 'ctest' has to be on PATH"* 가 붙는다(시험으로 관측).

## 변경

- `RunnerProcess`(새 서비스): `MainWindowViewModel.RunProcess` 를 옮겼다. `Run` 은 시작 실패를 `RunnerOutcome.DidNotStart` 로 돌려주고, `Describe` 가 문구의 **유일한 출처**다. 실제 프로세스를 띄워 시험하려고 WPF 밖으로 뺐다.
- `ExecuteRunnerAsync`: 문구를 직접 쓰지 않고 `RunnerProcess.Run`/`Describe` 를 부르고 `outcome.Verdict` 를 돌려준다. `RunConsoleRunnerAsync`(15·16)와 `RunBenchmarkAsync`(17)는 이전과 같은 경로로 들어온다 — 재구현 없음. 15·16 의 호출과 통과 문구는 불변.

## 시험

| 시험 | 무엇을 단언하나 |
|---|---|
| `RunnerProcessTests` 12건 | 실제 프로세스: 없는 파일 · 텍스트 파일 · PATH 에 없는 이름은 `Started=false`·검증값 null, `cmd /c exit 3` 은 `Started`·false, `exit 0` 은 true |
| 같은 클래스, 세 라벨 각각 | `Self-check` · `GUI E2E` · `Benchmark runner` 모두 "못 띄움" 문구와 "실패" 문구가 **서로 다르고**(`did not run` ⟷ `FAILED (exit 3)`), 서로의 낱말을 포함하지 않는다. 세 라벨이 view model 이 실제로 넘기는 값임을 소스에서 읽어 단언 |
| 결합 조사 | `ExecuteRunnerAsync` 가 `RunnerProcess.Run`·`Describe` 를 호출하고, view model 안의 `StatusText = …` 가 두 문구를 스스로 갖지 않으며, `RunnerProcess` 에 각 문구가 정확히 한 번뿐 — 조사 자체를 변이 복사본 3종으로 검증 |
| `BenchmarkRunnerServiceTests` | C-176 의 결합 단언을 `RunProcess` 대신 `RunnerProcess` 를 따라가도록 갱신(view model 에 리다이렉트 프로세스 시작 0곳, `RunnerProcess` 에 정확히 1곳) |
| E2E A15 (신규) | 앱을 `--automation-selfcheck-exe <텍스트 파일>` 로 띄우면 `SelfCheckPassed` 가 null 이고 상태줄에 `did not run` 이 있고 `FAILED` 는 없다 |
| E2E A05 (보강) | 돌고 죽는 러너는 `FAILED` 가 있고 `did not run` 은 없다 |

로컬: `Functional` 206 통과 · 건너뜀 1(`ErrorCodeMapping NOT_IMPLEMENTED`, 무관) · 실패 0. `AutomationReportBackendTests` 14 통과 · 건너뜀 1(A03, 기존) · 실패 0. (`local_runs.txt`)

## 반증 — 구별을 지우면 시험이 빨개지는가 (실제 소스, 4팔)

| 팔 | 망가뜨린 것 | 결과 |
|---|---|---|
| same-wording | 시작 실패 문구를 `FAILED (exit …)` 와 같게 | `RunnerProcessTests` 12건 중 **5건 빨강**: 세 라벨 행(`Self-check` · `GUI E2E` · `Benchmark runner`)이 **각각** 빨갛고, 결합 조사 2건도 빨갛다(재실행으로 전체 목록 확인, `falsification_arms.txt` 끝) |
| start-failure-becomes-a-run | 시작 실패를 `Started=true, exit -1` 로 둔갑 | 없는 파일·텍스트 파일 시험 빨강 |
| executor-collapses-null-to-false | 실행기가 `Verdict ?? false` 로 null 을 false 로 | **A15(E2E) 빨강** — 앱 수준에서만 보이는 붕괴 |
| second-wording-in-view-model | view model 에 `StatusText = … FAILED (exit …)` 를 하나 더 | 결합 조사 빨강 |

네 팔 모두 빌드는 성공했고 소스는 바이트 동일하게 복구했다(`falsification_arms.txt`).

## C-176 푸시의 CI (요청받은 항목, run `36845607471`, 전체 success)

- Mock: 180 결과, 건너뜀 34(설계상 Mock 건너뜀), **`input-gated-skips=0`**(잡 자신의 줄), A14·R01·R02 Passed. 자동화 보고서 JSON 에 A14 가 읽은 문장이 있다: `Benchmark runner is not built: build/ci-post has no CTestTestfile.cmake. …`, `BenchmarkPassed=null`, `BenchmarkLaunchSuppressed=false`, 개수 6=6 — **Mock 잡은 "not built" 분기를 실제로 탔다.**
- Native: **180/180 Passed**, A14 Passed. **Native 잡이 어느 분기를 탔는지는 읽지 않았다**(시험은 두 분기 어느 쪽이든 통과). (`GUI-C-176/ci_run_36845607471.txt`)

## 사건 기록 (두 가지, 둘 다 제품 결함 아님)

1. **줄바꿈 검사를 잘못했다.** 앞서 "CRLF 보존을 확인했다"고 쓴 검사(`grep -c $'\015'`)는 이 셸에서 패턴이 빈 문자열로 평가돼 **모든 줄을 센 무의미한 검사**였다. 파이썬으로 바이트를 세어 보니 저장소는 처음부터 LF(`core.autocrlf=input`)였고, C-176 에서 내가 새로 만든 두 파일만 작업 사본이 CRLF 였다(커밋된 내용은 git 이 LF 로 정규화해 영향 없음). 두 파일의 작업 사본을 LF 로 맞췄고(HEAD 와 바이트 동일), 이번 카드의 모든 줄바꿈 확인은 파이썬 바이트 계수로 했다.
2. **반증 직후 E2E A04·A06 이 잠시 빨갰다.** 원인은 코드가 아니라 빌드 신선도 가드: 반증 팔이 소스를 같은 내용으로 복구하며 수정 시각이 새로워져 러너가 "앱 사본이 소스보다 오래됨"이라고 거절했다. GUI 를 다시 빌드하자 클래스 전체가 통과했다.

## 미검증 · 한계

1. **행 16·17 의 "못 띄움" 은 앱을 통해 끝까지 관측하지 못했다.** E2E 로 직접 만들 수 있는 것은 행 15 뿐이다(`--automation-selfcheck-exe` 가 있는 유일한 러너). 행 16 은 오버라이드가 없고, 행 17 은 자동화에서 실행이 억제된다. 두 명령에 대해서는 **같은 실행기·같은 문구 함수**를 쓴다는 결합 단언과 세 라벨 각각의 문구 단언에 의존한다.
2. **행 17 의 실제 ctest 실행 경로는 여전히 앱 안에서 관측되지 않았다**(C-176 §4-1 그대로).
3. `RunnerOutcome.Verdict` 가 null 일 때 `SelfCheckPassed`/`GuiE2EPassed`/`BenchmarkPassed` 가 null 로 남는 것은 의도이나, 이 값을 소비하는 다른 UI 가 null 을 "아직 안 돌았다"로만 읽는지는 전수 조사하지 않았다(검색한 범위: `SelfCheckPassed`·`GuiE2EPassed` 를 읽는 자동화 보고서와 시험).
4. 시작 뒤 출력 읽기 실패(`ended abnormally`)는 시험하지 않았다 — 의도적으로 만들 방법이 없었다.

## 증거 파일

`falsification_arms.txt` · `local_runs.txt` · `text_lint.txt` · (C-176 폴더) `ci_run_36845607471.txt`

🗿 MoAI
