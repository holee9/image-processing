# GUI-C-176 — #225 행 17 Benchmark Runner

레인: gui · 이슈: `#225` · 구현 커밋 `f89b8c5b`(푸시 안 함, 푸시는 리더). 행 21(자리표시 문서)은 켜지 않았다.

## 결론

Benchmark Runner 메뉴를 켰다. 빌드 트리(`build/ci-post`)가 없으면 **"빌드 안 됨 + 빌드 방법"** 을 보여 주고 아무것도 실행하지 않으며, 있으면 `benchmark-regression.yml` 의 패턴으로 `ctest` 를 실행해 **종료 코드와 ctest 요약을 그대로** 보여 준다. 15·16 행과 **같은 프로세스 실행기**를 쓰고, 그 결합을 시험이 단언한다. 비활성 개수는 7→**6**, 세 축(`DisabledFutureCommandCount`·`UnimplementedMenuLeafCount`·XAML)이 6으로 일치한다.

**이 트리에서 관측하지 못한 것이 하나 있다**: 빌드 트리가 있고 자동화가 아닐 때의 **실제 `ctest` 실행**을 앱 안에서 돌려 본 적이 없다(이 트리에 빌드가 없고, 네이티브 빌드·입력 시험은 로컬 금지). §4.

## 1. 리더 결정 세 가지가 코드에서 어떻게 지켜졌나

| 결정 | 구현 | 어디서 확인했나 |
|---|---|---|
| (a) 개발 트리 전용, 빌드 없으면 "빌드 안 됨 + 방법", 빈 창·무반응 금지 | `BenchmarkRunnerService.Resolve`: `build/ci-post/CTestTestfile.cmake` 가 없으면 `Benchmark runner is not built: … Build it first: 'cmake --preset ci-post' and then 'cmake --build --preset ci-post --parallel' (see .github/workflows/benchmark-regression.yml).` 를 상태줄에 쓰고 프로세스를 시작하지 않는다 | `local_runs.txt`: A14 가 자동화 보고서에서 위 문장을 읽음. 통합 시험 2건(트리 없음 / CTest 파일 없는 디렉터리) |
| (b) ctest 종료 코드 + 요약 그대로, GUI 는 3000 ms 를 판정하지 않음, 패턴 출처를 주석에 인용 | 요약은 ctest 가 찍은 `N% tests passed` 줄부터 끝까지(`Summarize`), 통과/실패는 종료 코드 `== 0`. 3000 ms 같은 수는 GUI 코드에 없다. `TestPattern` 주석이 워크플로의 단계 이름과 `$pattern` 줄을 인용 | `BenchmarkRunnerServiceTests.ThePattern_IsTheOneTheBenchmarkWorkflowStepUses` 가 워크플로 원문의 `$pattern` 과 상수를 **같다고 단언**(반증 팔 1) |
| (c) 취소는 공용 실행기가 하는 만큼만 | 새 취소 경로 없음 | 아래 §3 |

결합 단언: `AllThreeRunnerCommands_ReachTheSameProcessExecutor` — `RunSelfCheckAsync`·`RunGuiE2EAsync` 는 `RunConsoleRunnerAsync` 를, `RunBenchmarkAsync`·`RunConsoleRunnerAsync` 는 `ExecuteRunnerAsync` 를 호출해야 하고, 파일 안에서 리다이렉트된 자식 프로세스를 시작하는 곳은 **정확히 한 곳**(`RunProcess`)이어야 한다.

## 2. 변경

- `MainWindowViewModel`: `RunConsoleRunnerAsync` 의 "실행하고 판정을 쓰는" 절반을 `ExecuteRunnerAsync` 로 뽑았다. 15·16 은 이전처럼 `RunConsoleRunnerAsync` 로 들어오고 기본 인자를 넘기므로 **상태줄 문구와 동작이 그대로**다(A04·A05 가 로컬에서 통과). `RunProcess` 가 인자 목록·작업 디렉터리·요약 선택기를 받는다(인자는 셸을 거치지 않는 목록).
- `BenchmarkRunnerService`(새): 해석과 요약. `--no-tests=error` 를 넘긴다 — **진짜 ctest 로 확인**했다: 패턴이 아무것도 못 고르면 플래그 없이는 exit 0, 있으면 exit 8 이고 이유는 stderr 의 `No tests were found!!!`. 실제로 0건을 돈 실행이 "통과" 로 보이는 것을 막는다(`ctest_captured.txt`).
- XAML: 행 17 의 `IsEnabled="False"` 를 지우고 `RunBenchmarkCommand` 를 묶었다. `MainWindow.xaml.cs` 의 이름 목록에서 이 항목을 뺐다.
- 자동화: 메뉴를 눌러 상태·검증값·억제 여부를 보고서에 싣는다. 자동화 실행에서는 **행 14·20 처럼 실행만 억제**한다(CI 가 멀티 분 단위 네이티브 벤치를 시작하지 않도록).

## 3. 취소 — 보고만 한다

공용 실행기(`ExecuteRunnerAsync` → `RunProcess`)에는 **취소 경로가 없다.** `ReadToEnd` 로 끝까지 읽고 `WaitForExit` 한다. 그래서 이 명령도 취소할 수 없다. `Stop Processing`(행 11)은 렌더를 멈추는 것이지 이 실행을 멈추지 않는다. 운영자가 도중에 닫으려면 앱을 닫는 수밖에 없다. 새 경로는 만들지 않았다(결정 c).

## 4. 미검증 · 한계

1. **앱 안에서 실제 ctest 를 돌려 보지 못했다.** 빌드 트리가 있고 자동화가 아닌 경로는 이 트리에서 실행된 적이 없다. 관측한 것은 (i) 트리 없음 → "not built"(A14, 자동화 보고서), (ii) 트리 있는 척하는 마커 파일 → "launch suppressed"(A14, 마커는 제거함), (iii) **같은 인자로 진짜 ctest 를 따로** 돌려 본 출력(통과 exit 0 · 실패 exit 8 · 0건 exit 8)과, 그 출력을 `Summarize` 시험의 입력으로 쓴 것.
2. **`ctest` 가 PATH 에 없으면** `Process.Start` 가 예외를 던지고 공용 실행기는 그것을 `could not be started: …` 와 함께 `false` 로 돌려준다 — 15·16 이 이미 하는 동작을 그대로 따른 것이라 **"실행 못 함"(null)과 "실패"(false)가 이 경우 구분되지 않는다.** 상태줄에는 이유가 나오므로 무반응은 아니다. 고치려면 공용 실행기 변경이라 이번 카드 밖.
3. **ctest 가 쓰는 DLL 경로**(`build/ci-post/bin`)는 CI 가 환경(`Use-MsvcDevShell`)을 잡은 뒤 같은 셸에서 도는 것을 가정한다. 앱이 그 환경 없이 ctest 를 띄우면 시험이 DLL 을 못 찾고 실패할 수 있다 — 관측하지 않았다.
4. **`RunProcess` 는 stdout 을 끝까지 읽은 뒤 stderr 를 읽는다**(기존 구조). `--output-on-failure` 로 stdout 이 크면 괜찮지만 stderr 가 파이프 버퍼를 채우면 멈출 수 있다. ctest 의 stderr 는 `Errors while running CTest` 정도라 이 실행에서는 위험이 작다고 판단하지만 측정하지 않았다.
5. CI 의 `gui-automation`·`gui-e2e-native` 두 잡에서 A14 가 도는 것은 **CI 가 해야 알 수 있다.** 두 잡에는 `build/ci-post` 가 없으므로 A14 는 "not built" 분기를 탈 것으로 예상하지만, 예상일 뿐이다.
6. 로컬에서 건너뛴 시험: `ErrorCodeMappingTests …(NOT_IMPLEMENTED)` 1건과 `A03_TheWiredCommands_ReportWhatTheyDid` 1건 — 둘 다 이 변경과 무관하나, A03 의 건너뛴 사유는 읽지 않았다.

## 5. 검증 결과 요약

| 무엇 | 결과 | 파일 |
|---|---|---|
| 통합 시험 `BenchmarkRunnerServiceTests` | 10/10 통과 | `local_runs.txt` |
| `Category=Functional` 전체 | 195 통과 · 건너뜀 1 · 실패 0 | `local_runs.txt` |
| E2E `AutomationReportBackendTests` 전체(A04·A05·A11·A14 포함) | 13 통과 · 건너뜀 1 · 실패 0 | `local_runs.txt` |
| 반증 3팔(패턴 드리프트 · 두 번째 실행기 · `--no-tests=error` 제거) | 각각 해당 시험이 빨강 + 빌드 성공 + 소스 복구 바이트 동일 | `falsification_arms.txt` |
| 진짜 ctest 출력 캡처 | 통과/실패/0건 × 플래그 유무 | `ctest_captured.txt` |
| text-lint(전체 트리, 파이프 없음) | exit 0 | `text_lint.txt` |

## 증거 파일

`ctest_captured.txt` · `falsification_arms.txt` · `local_runs.txt` · `text_lint.txt`

🗿 MoAI
