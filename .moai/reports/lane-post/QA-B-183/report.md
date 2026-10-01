# QA-B-183 — main CI 의 xpe_ai_tests 간헐 실패 (#162 단계, `WorkerBoneSuppress.FailuresCarryTheSameCodeAsInProcess`)

Refs #162 #130

## 1. 주장

| # | 주장 |
|---|------|
| C1 | CI 단계 "각 시험 exe 를 한 프로세스로 실행"(#162)에서만 나는 간헐 실패를 로컬에서 재현했다(같은 시험, 같은 반환 코드). |
| C2 | 원인은 제품이 아니라 **내 시험 도구(QA-B-177b 의 `StdoutCapture`)가 남긴 낡은 핸들**이다. 같은 프로세스에서 이후에 열린 파이프 핸들이 그 값을 받아, 제품 로그 줄이 워커 쪽 파이프로 흘러들었다. |
| C3 | 고쳤다: 캡처를 끝낼 때 기본 로거의 콘솔 싱크를 현재 stdout 핸들로 다시 만든다. 이를 겨눈 시험은 수정 전 6/6 빨강, 수정 후 6/6 초록. |
| C4 | 수정 후 같은 부하 조건에서 이 원인의 실패는 관측되지 않았다. 다만 부하가 센 구성(4병렬, 96회)에서 **다른 종류**의 실패 2건이 남았다(7절). |

## 2. 원인 (관측 순서대로)

1. 단일 프로세스 반복 실행에서는 안 났다(`g183-s*`: 대상 시험만 60회, 실패 0). 전체 시험을 한 프로세스로 12회 돌리자 1회 실패(`before_fix_single_process_run12_failure.txt`), 4병렬 부하에서 24회 중 1회 CI 와 같은 시험이 실패(`before_fix_loaded_run_p4_failure.txt`): `InitWorker(w, dir)` 가 `-3`(`XPE_ERR_PROCESSING_FAILED`) 또는 `-9`(`XPE_ERR_IO_FAILED`).
2. 워커 프로세스의 stdout 을 파일로 받아 보니(임시 계측), 실패한 워커는 `Client connected` 다음 곧바로 `Client disconnected or error` 로 끝났다. 그 사이에 INIT 을 받은 흔적이 없다.
3. 워커의 헤더 읽기에 임시 계측을 넣어 실패 원인을 찍었다: `ReadFile` 이 `ERROR_MORE_DATA`(234)로 40바이트를 돌려줬다. 헤더 자리에 온 바이트는 `magic=0x3230325b` = ASCII `[202`, 즉 **날짜로 시작하는 로그 줄**이다. 파이프에 남은 바이트를 들여다보니 제품 로그 문장이었다(`before_fix_worker_side_pipe_contents.txt`, 11건):
   - `…_suppress: worker path failed (-9), input returned unchanged (1 of 3 consecutive failures)…`
   - `…initialized: model_dir=D:/workspace-github/xpe-post/modules/ai/tests/data/models_missing, ep=0, …`
   → 시험 프로세스의 로그가 **파이프 핸들로 쓰였다**.
4. 쓰는 쪽을 찾았다. `test_ai_log_macros.cpp` 의 `StdoutCapture` 는 stdout(fd 1)을 파일로 바꿨다가(`_dup2` ×2) 되돌린다. `_dup2` 는 fd 1 이 들고 있던 **원래 OS 핸들을 닫는다**. spdlog 기본 로거의 콘솔 싱크는 만들어질 때 그 핸들 값을 저장해 두고 계속 쓴다. 이후 그 값은 비어 있고, 다음에 열리는 핸들(워커 시험에서는 파이프 클라이언트)이 같은 값을 받으면 로그 줄이 거기로 간다.
5. 그래서 간헐적이다: 낡은 값을 어느 핸들이 이어받는지는 그 시점에 비어 있는 핸들 번호에 달렸다. ctest 는 시험마다 프로세스를 새로 띄워 이 일이 없고, CI 의 "한 프로세스" 단계와 내가 로컬에서 한 프로세스로 돌릴 때만 닿는다. `QA-B-177b/177c` 에서 내가 추가한 시험 도구가 원인이다.

제품 코드(`ai.cpp`, 워커, 브리지)의 결함은 아니다. 워커가 `ERROR_MORE_DATA` 를 헤더 오류로 취급하는 점은 건드리지 않았다(메시지 한 개가 40바이트보다 큰 경우만 해당하고, 정상 프레임은 헤더와 페이로드가 따로 온다). 내가 처음 세운 가설 "헤더와 페이로드가 한 메시지로 합쳐져 도착한다"는 **틀렸다**: 워커를 얼려 둔 채 INIT 을 쓰는 시험(`AFrameQueuedWhile…`)이 수정 전 워커에서도 초록이었고, 실제 바이트는 로그 문장이었다. 그 시험과 워커 수정은 버렸다.

## 3. 증거

### 3.1 재현 (수정 전)

- `before_fix_single_process_run12_failure.txt`: 전체 단일 프로세스 12회 중 12번째 실패(`WorkerPathFixture…`, 같은 부류: 워커 파이프가 낡은 값을 받음).
- `before_fix_loaded_run_p4_failure.txt`: 4병렬 24회 중 1회, CI 와 같은 `WorkerBoneSuppress.FailuresCarryTheSameCodeAsInProcess`.
- `before_fix_worker_side_pipe_contents.txt`: 실패한 워커들이 파이프에서 읽은 것(로그 문장).

### 3.2 겨눈 시험 — 수정 전 빨강 (`red_before_fix_stale_handle_test.txt`)

`AiLogStdoutRedirect.ARedirectCycleDoesNotLeaveTheDefaultLoggerWritingIntoAnUnrelatedHandle`:
기본 로거를 새 콘솔 싱크로 맞춘 뒤 캡처 주기를 한 번 돌리고, 새 핸들을 열어 가며 싱크가 저장한 값과 같은 것을 찾고, 거기에 로그를 쓰고, 그 파일이 비어 있는지 본다.

```
PROBE sinkHandle=…D0 … match=…D0 valid=1 held=5
… error: Expected equality of these values:  0 / GetFileSize(match)   Which is: 71
the default logger wrote into a handle it does not own
```
6회 중 6회 빨강(로그 줄 71바이트가 낯선 핸들로 들어감).

### 3.3 수정 후

수정: `StdoutCapture::Finish()` 가 stdout 을 되돌린 직후 `XpeTestRebindDefaultLoggerStdout()`(`test_ai_log_macros_spdlog.cpp`)를 부른다. 새 콘솔 싱크를 가진 기본 로거로 바꿔 현재 핸들을 저장하게 한다.

| 확인 | 결과 |
|------|------|
| 겨눈 시험 6회 | 6/6 초록 |
| ci-post 전체 ctest (`after_full_ci_post_ctest.txt`) | `100% tests passed, 0 tests failed out of 1044` (직전 1043 + 신규 1) |
| ci-ai 전체 ctest, `XPE_AI_EXPECT_ONNX=1` (`after_full_ci_ai_ctest.txt`) | `100% tests passed, 0 tests failed out of 375` |
| 두 빌드 로그 `warning C` | 0건 |
| 시험 후 남은 `xpe_ai_worker.exe` | 0개 |
| 전체 단일 프로세스(CI 단계와 같은 필터) 2병렬 24회 | 실패 0 |
| 전체 단일 프로세스 4병렬 96회 | 실패 2 (7절) |

### 3.4 CI 에서 실패했을 때 워크플로가 찍어야 할 것 (리더 작업용)

현재 단계는 요약 줄만 찍어서 이 실패의 단서가 없었다. 실패한 exe 의 `[  FAILED  ]` 줄과 그 위의 `error:` 블록(파일:줄, `Expected/Which is`)을 출력해야 한다. 이번 원인은 그 블록의 `Which is: -3` 또는 `-9` 와 시험 이름만으로도 "InitWorker 의 첫 수신 실패" 까지는 좁혀졌다.

## 4. Baseline 귀속

- 명령: `build\ci-post\bin\xpe_ai_tests.exe "--gtest_filter=-*Performance*:*Within*ms*"` (CI 단계와 같은 필터, 한 프로세스), 반복 실행 하네스 `xargs -P 4`/`-P 2`.
- 모든 수치는 이 트리(HEAD `5767abfb` + 이 카드의 변경)에서 이번 실행에서 관측했다. 이전 카드의 수치를 가져오지 않았다.
- 빌드 반환값은 `===BUILD=0===` 로 읽었다. 임시 계측을 넣은 빌드(워커/시험 stdout 파일, 헤더 읽기 진단)는 모두 제거했고(`grep -rn TEMP-DIAG modules/` → 0건), 수정 후 수치는 계측 없는 빌드에서 얻었다.

## 5. Gaps (미검증)

- **CI 환경에서 실제로 같은 원인이었는지**는 관측하지 못했다. CI 로그에는 실패 시험 이름과 종료 코드만 있다. 근거는 "같은 시험이 같은 조건(한 프로세스, 부하)에서 같은 반환 코드로 로컬에서 재현되고, 그 로컬 원인이 파이프에서 직접 관측됐다" 는 것까지다.
- 같은 부류의 다른 시험(`WorkerPathFixture.*`) 실패가 이 원인으로 났다는 것은 로컬 재현의 같은 증상(`ChildWorkers().size()==0` 직후 워커 종료)과 시기로만 연결했고, 개별로 원인을 찍지는 않았다. 수정 후 2병렬 24회에 없었다는 것이 간접 근거다.
- 낡은 핸들이 **제품 코드 쪽에서도** 생길 수 있는 경로(제품이 stdout 을 닫는 일)는 찾지 못했다. 제품에서 `GetStdHandle`/`SetStdHandle`/`_dup2`/`freopen` 는 `modules/ai`·`modules/common/src` 에서 검색해 0건이다(텍스트 검색, 다른 모듈은 보지 않음).
- 시험 프로세스가 `StdoutCapture` 외의 방법으로 stdout 을 닫는 경우는 보지 않았다.

## 6. Residual-risk (잔여 위험)

- 다른 모듈의 시험이 stdout/stderr 을 같은 방식으로 되돌린 뒤 spdlog 를 쓰면 같은 일이 생길 수 있다. `modules/ai` 의 시험에서는 `StdoutCapture` 가 유일한 곳이었다. 다른 레인 모듈은 보지 않았다.
- 새 시험은 `CreateFile` 이 낡은 값을 이어받는지에 기댄다. 20000개까지 열어도 못 찾으면 "이어받음도 유효함도 아님" 으로 빨개지도록 했다(증명 못 함을 초록으로 두지 않기 위해). 값이 계속 유효하게 유지되도록 캡처가 바뀌면 시험이 초록(유효 분기)이 된다.
- 기본 로거를 새로 만들므로 이 exe 에서 기본 로거의 레벨/패턴을 바꿔 둔 시험이 있다면 캡처 뒤에 기본값으로 돌아간다. 이 exe 에서 그런 시험은 찾지 못했다(`SpdlogCapture` 는 자기 소멸자에서 기존 것으로 되돌린다).

## 7. 수정 후에 남은 실패 (다른 종류)

4병렬 96회 중 2회, 다른 부류: `WorkerSupervisor.FirstCallStartsAWorkerAndTheSecondReusesIt`(6256 ms), `WorkerSupervisor.AfterTheWorkerIsKilledTheNextCallSucceedsOnANewPid`(7142 ms). 평소 150ms 안팎인 시험이 수 초 걸리고 `Ping()` 이 `-3` 으로 끝났다. 로그 줄이 아니라 **워커 기동이 부하에서 제한 시간을 넘긴** 모양이다(4병렬 × 17초 스위트 + 이 기계의 다른 부하). 같은 하네스의 2병렬 24회에서는 0건. 이 모양을 CI 에서 본 적은 없고 이번 카드의 범위(CI 에서 난 시험)도 아니라 손대지 않았다. 증거: `after_fix_HIT-r1-11.txt`, `after_fix_HIT-r1-9.txt`.

## 8. 변경 파일

- `modules/ai/tests/test_ai_log_macros.cpp` — `StdoutCapture::Finish()` 에서 기본 로거 재결합, 캡처 주기 시험 보조 함수
- `modules/ai/tests/test_ai_log_macros_spdlog.cpp` — 재결합 함수, 겨눈 시험, 필요한 include
- `.moai/reports/lane-post/QA-B-183/**` — 이 보고서와 증거
