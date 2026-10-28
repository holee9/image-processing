# QA-A-320 방법 (실행한 명령 그대로)

- 코드: `dev/preprocess` HEAD `a834f2a0` (시작 전 확인). 빌드: `build/ci-preprocess` (ci-preprocess 프리셋, RelWithDebInfo, Ninja, vcvars64).
  `02_build.bat.txt` = 사용한 빌드 스크립트(`build.bat <트리> <타깃> [configure]`).
- 임시 폴더: `D:/workspace-github/xpe-data/qa-320-tmp` 하나. 커밋별 빌드용 git 워크트리 하나(`wt`, `git worktree add --detach`, 커밋마다
  `git -C wt checkout --detach <sha>`) + 그 안의 빌드 폴더 하나(`wt/build/ci-preprocess`)를 재사용. 끝에서 `git worktree remove` 후 폴더 삭제.

## 1. 실패 재현 (HEAD a834f2a0)
- `cmake --build build/ci-preprocess --target xpe_preprocess_oom_tests` → rc 0.
- 실패 5개를 하나씩 `xpe_preprocess_oom_tests.exe --gtest_filter=<이름>` 으로 실행 → `01_oom5_fail_at_head_a834f2a0.txt`.
  다섯 모두 `[ RUN ]` 다음 줄 없이 프로세스 종료 코드 3 (판정 줄 없음). CI 로그에 "실패 상세 줄이 없다"는 것과 같은 모양.

## 2. 커밋별 확인
- `02_run5.sh.txt` 로 커밋마다 다섯 시험을 하나씩 실행 → `02_bisect_per_commit.txt`.
- 57ddbd2e(QA-A-296) 5/5 OK, 2b0f3674(QA-A-314) 5/5 OK, 0eee2ed0(QA-A-315) 5/5 rc=3, 6354330d(P1) 5/5 rc=3, HEAD 5/5 rc=3(1단계).
- 첫 실패 커밋 = `0eee2ed0`. 289d885b 와 d7d18fd4 단독은 빌드하지 않았다(57ddbd2e 가 통과하므로 그 앞은 경계 판정에 불필요, d7d18fd4 는 HEAD 에 포함).

## 3. 종료 지점 확인 (임시 계측, 스크래치 워크트리에만, 커밋 안 함)
- `03_temp_instrumentation.diff.txt`: 스크래치 워크트리(HEAD a834f2a0)의 `test_oom_injection.cpp` 에만 `QA320TMP` 진단을 넣었다.
  SIGABRT 처리기 + `std::set_terminate` 처리기가 DbgHelp 로 호출 스택을 찍고, 주입한 할당 순간에도 스택을 찍는다(주입 직후 무장 해제).
  (증거 파일로 옮기며 diff 의 줄 끝 공백만 지웠다 — text-lint 규칙. 내용은 같다.)
- 결과: `04_terminate_stack_defect_correct.txt`(종료 스택), `05_injection_stacks_k7_k8_defect_correct.txt`(7·8번째 할당 주입 스택),
  `06_stacks_pipeline_4tests.txt`(OomPipeline 4개의 마지막 주입 스택과 종료 스택).
- MSVC 14.44.35207 헤더 해당 줄은 `sed -n` 으로 직접 읽었다(보고서 인용).
