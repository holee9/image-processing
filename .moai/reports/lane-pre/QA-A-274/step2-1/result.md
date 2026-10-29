[2026-10-21 10:09 KST]
# QA-A-274 2-1 결과 (A2+A3)

- 커밋: `e404b70e` (부모는 인벤토리 커밋 `2eb77995`). 빌드한 HEAD 는 `head-sha.txt` 에 있다.
- 올린 것: 파일 18개 전체(`files.txt`)와 `modules/preprocess/CMakeLists.txt` 의 `preprocess.legacy_ghost_fingerprints` hunk 1개(5줄). `xpe_pre_e2e` 타깃 등 A4 hunk 4개는 올리지 않았다. `git apply --cached` 와 `git add` 대신 패치만 썼고 작업 트리는 바꾸지 않았다.
- 작업 트리 불변: 커밋 전 dirty 118개의 SHA-256(`dirty-sha-before.json`)과 커밋 후 값이 모두 같다(변경 0). 18개 파일은 `git diff HEAD` 가 비어 있고 CMakeLists 에는 A4 24줄만 남았다.
- 480 기준선(`QA-A-273/run-20261020T230045Z/worktree.txt`): 항목 480개 중 사라진 것 0.
- HEAD 단독 빌드: `git archive HEAD` 를 `D:\xpe-h274a` 에 풀어 `ci-preprocess` 프리셋으로 구성(CONFIGURE_EXIT=0)·빌드(312 단계, BUILD_EXIT=0, 경고 0).
- 시험(ctest -j1, 한 번에 1개): Ghost·OOM·OutputUnchanged·legacy_ghost·저장·XCal·PipelineStage·PreprocessCalibration 계열 360개 통과 / 실패 0, 비활성 3개(원래 Disabled). `preprocess.legacy_ghost_fingerprints` 통과.
- 미실행: 전체 스위트(CI 몫), 시험 이름 정규식에 걸리지 않은 시험, CI 환경(컴파일러 버전 차이).
- 남은 프로세스: ctest·cl·ninja·cmake 0(보호 GUI 3개 PID 1868·17364·19184 는 그대로 살아 있음). 빌드 폴더 `D:\xpe-h274a` 는 보존한다.
