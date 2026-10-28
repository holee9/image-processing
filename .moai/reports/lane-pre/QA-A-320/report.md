[2026-10-29 08:33 KST] [xpe-leader 대행 QA-A-320]

# QA-A-320 — dev/preprocess CI 빨강 잡 해소 (Refs #278)

목적: 다음 push 에서 CI 를 초록으로 되돌린다. 범위: OOM 시험 5개의 실패 원문·첫 실패 커밋·원인·최소 수정, clang-tidy NEW 4건 코드 해소, text-lint, 확인. push 하지 않는다.

- 코드: `dev/preprocess` HEAD `a834f2a0` → `1e3630e5`. 커밋 `e9289fdb`(실패 증거와 커밋별 확인), `59f9f0e9`(수정), `1e3630e5`(확인 증거). 하네스가 작업자의 보고서 쓰기를 막아 본문은 리더가 옮기고 §7 을 덧붙였다.
- 증거: `.moai/reports/lane-pre/QA-A-320/evidence/`(방법 `00_method.md`).

## KR 상태
| KR | 상태 | 근거 |
|---|---|---|
| OOM 5개 원인과 수정 | 완료(로컬) | 01, 02, 04~06, 10, 13 |
| clang-tidy 4건 | 완료(로컬, 두 검사만) / CI 기준선 검사 전체 미확인 | 16 |
| text-lint | 카드 2줄 완료 + 리더가 잔여 17줄 처리(§7) | 22 |
| 출력 비트 동일 | 완료 15/15 | 17 |
| CI 초록 | push 후 확인 | — |

## 1. 확인된 사실

### 1.1 OOM 실패 원문 (HEAD a834f2a0, `01_`)
다섯 시험을 하나씩 돌렸다. 모두 `[ RUN      ] <이름>` 다음에 판정 줄 없이 프로세스 종료 코드 3 으로 끝났다(CI 로그에 상세 줄이 없던 것과 같은 모양).
- `OomInjection.ADefectCorrectionThatFailsLeavesTheStoreUntouchedAndTheLockFree`
- `OomPipeline.PipelineExThatRunsOutOfMemoryLeavesEverythingAsItWas`
- `OomPipeline.PipelineThatRunsOutOfMemoryLeavesEverythingAsItWas`
- `OomPipeline.PipelineBatchThatRunsOutOfMemoryLeavesEveryFrameWholeOrUntouched`
- `OomPipeline.AFailedPipelineThatStartedOnAnEmptyStoreLeavesNothingOrTheWholeVerifiedSet`

### 1.2 첫 실패 커밋 (`02_`)
| 커밋 | 결과 |
|---|---|
| 57ddbd2e (QA-A-296) | 5/5 OK |
| 2b0f3674 (QA-A-314) | 5/5 OK |
| **0eee2ed0 (QA-A-315)** | **5/5 종료 코드 3** |
| 6354330d (P1) | 5/5 종료 코드 3 |
| a834f2a0 (HEAD) | 5/5 종료 코드 3 |

스크래치 워크트리 하나와 그 안의 빌드 폴더 하나를 커밋마다 다시 썼다.

### 1.3 원인 (임시 계측 스택 `04_`~`06_`, MSVC 14.44.35207 헤더; 계측은 스크래치에만, diff `03_`)
1. `defect_correct.cpp:1172` `xpe_parallel_chunks(kListParts, 1, ...)` 는 0eee2ed0 에서 들어왔다(git blame). minPerThread 1 이라 작은 시험 영상에서도 여러 범위를 실행한다.
2. `parallel.hpp:63` `std::async(std::launch::async, body)` → `_Task_async_state` → `create_task` → `_ScheduleTask` → `pplwin.h:85` `new _PPLTaskChore` 에서 주입 할당이 실패한다(defect_correct 시험 8번째, pipeline 시험 40·72번째 할당).
3. `ppltasks.h:1774-1784` `_ScheduleTask` 가 이 예외를 `_CATCH_ALL` 로 잡아 작업 안에 저장한다. std::async 는 정상 반환하고 그 범위의 `body` 는 실행되지 않는다.
4. `parallel.hpp:69` `task.wait()` → `ppltasks.h:1622` `_RethrowUserException` 이 bad_alloc 을 다시 던진다.
5. 되감기 중 `vector<future>` 소멸자 → `future:628` `~_Task_async_state() noexcept { _Wait(); }` 가 같은 예외를 또 던지고, noexcept 라 std::terminate(`04_` 프레임 #12~#17).

다섯 시험 모두 같은 경로(`06_`). 0eee2ed0 이전 std::thread 방식은 2b0f3674 에서 5/5 통과.

### 1.4 수정 (`59f9f0e9`)
- `parallel.hpp`: 범위를 Windows 스레드 풀 작업으로 실행(`CreateThreadpoolWork`/`SubmitThreadpoolWork`/`WaitForThreadpoolWorkCallbacks`). 작업을 못 만들면 그 범위를 호출 스레드에서 실행. 범위마다의 operator new 할당은 작업 객체 하나(`make_unique`)뿐이고, 범위 시작 전에 실패하면 그대로 던진다. 작업 객체 소멸자가 범위 종료를 기다린다. `errors` 를 `pool` 보다 먼저 선언하는 P1 순서는 유지. 분할과 결과 동일. 비 Windows 는 std::thread 대체 경로.
- `defect_correct.cpp:362`: `static_cast<size_t>(*q / width) * width`(값 동일, 곱 결과 ≤ `*q`).
- `acrossValid`, `xpe_sat_histogram`, `xpe_sat_apply`: 모듈 관례(NOLINT 22곳)대로 정의를 `NOLINTBEGIN/END(bugprone-easily-swappable-parameters)` 로 감싸고 인자 역할을 적었다. 기준선 파일은 바꾸지 않았다.
- `QA-A-314/evidence/70_temp_tool_rows_patch.txt`: 7·12행 공백 제거.

### 1.5 확인 결과
| 확인 | 결과 | 증거 |
|---|---|---|
| 실패하던 5개(하나씩) | 5/5 OK | `10_` |
| `xpe_preprocess_oom_tests.exe` 전체 | 99/99 PASSED | `13_` |
| `xpe_preprocess_parallel_unwind_tests.exe` | 1/1 PASSED | `11_` |
| 반증: 스크래치에서 `errors` 를 `pool` 뒤에 선언 | 같은 시험이 `use after free` 로 FAILED — 새 실행 방식에서도 순서 결함을 잡는다 | `12_` |
| ParallelDeterminism, DefectCorrect, GainCorrect, Pipeline, SaturationTest, DefectLine | 143/143 | `14_` |
| Saturation* | 33 통과, 3 SKIPPED(SaturationRealData) | `15_` |
| clang-tidy 19.1.5(VS 내장), 두 검사, 두 파일 | 수정 전 4건(CI 와 같은 줄 362·773·17·198) → 수정 뒤 0건 | `16_` |
| clang-tidy 전체 설정, 4개 TU, 수정 뒤 | 모듈 소스 경고 2건(pipeline.cpp, 둘 다 기준선에 있음). parallel.hpp 0 | `16_` |
| QA-A-315 출력 15개 sha256 | 15/15 동일 | `17_` |

## 2. 추정
- s1_um300 "all" 시간 수정 뒤 1회 12.922 ms(QA-A-315 13.089 ms, `18_`). 1회·부하 미기록이라 참고값.
- std::async 방식은 예약에 실패한 범위를 실행하지 않으므로, 실제 메모리 부족에서도 같은 종료가 났을 것(재현은 안 함).

## 3. 미확인
- CI 의 clang-tidy 기준선 검사 전체(`check_clang_tidy.py`) 로컬 미실행(compdb 래퍼로 rc 1), CI 러너 버전 차이 가능 → CI 에서 확인.
- asan-tests 로컬 미실행. 289d885b·d7d18fd4 단독 빌드 안 함(경계 판정에는 불필요). check_req_citations 로컬 rc 1(`0 spec files`, `.moai/specs` 가 작업 트리에 없음, 이번 변경과 무관). 비 Windows 대체 경로 미빌드.

## 4. 임시 폴더
- 지운 것: `D:/workspace-github/xpe-data/qa-320-tmp`(스크래치 워크트리 `git worktree remove` 후 폴더 삭제).
- 남긴 증거: `.moai/reports/lane-pre/QA-A-320/evidence/`(텍스트만). 남은 프로세스 0(2026-10-24 ninja PID 26956 은 무관).

## 7. 리더 확인과 처리 (2026-10-29 08:35)
- 커밋 3개 확인, 인덱스 61개 유지. `02_bisect_per_commit.txt` 에서 57ddbd2e·2b0f3674 OK, `13_` `[  PASSED  ] 99 tests.`, `17_` "empty = 15/15 identical" 을 직접 읽었다.
- 작업자가 차단으로 보고한 text-lint 잔여 17줄(QA-A-318 `01_`·`02_`·`21_`, QA-A-319 `12_`)은 리더가 줄 끝 공백만 지웠다(17 삽입 / 17 삭제). `12_temp_instrumentation.patch.txt` 는 diff 의 빈 문맥 줄 공백까지 지워져 그대로 적용되는 패치는 아니게 됐다 — 증거 열람용이며 계측은 커밋되지 않았다.
- 리더 로컬 실행: `tools/ci/Test-TrackedTextFiles.ps1` → `Tracked text file validation passed.`(rc 0).
- 판정: 수정을 받아들이고 push 해서 CI 로 clang-tidy 기준선 검사·asan 을 확인한다.
