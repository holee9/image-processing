[2026-10-21 10:12 KST]
# QA-A-274 2-2 결과 (A4, xpe_pre_e2e)

- 커밋: `a114f642` (부모 `6aad00cf`). 빌드한 HEAD 는 `head-sha.txt`.
- 올린 것: 파일 8개(`files.txt`: `tools/xpe_pre_e2e.cpp/.hpp`, `xpe_pre_e2e_report.hpp`, `xpe_pre_defect_metrics.hpp`, `xpe_pre_stripe.hpp`, `xpe_pre_test_evidence.py`, `tests/test_pre_e2e.cpp`, `tests/test_pre_test_evidence.py`)와 `modules/preprocess/CMakeLists.txt` 의 A4 hunk 4개(실행 파일 타깃, 시험 목록 2곳, 경로 헤더 생성). 평가 프로토콜 문서는 넣지 않았다. `git apply --cached`/`git add` 로 index 에만 올렸고 작업 트리는 바꾸지 않았다.
- 작업 트리 불변: 커밋 전 dirty 100개의 SHA-256(`dirty-sha-before.json.txt`)과 커밋 후 값이 모두 같다(변경 0). 올린 파일과 CMakeLists 는 `git diff HEAD` 가 비어 있다. 480 기준선에서 사라진 항목 0.
- HEAD 단독 빌드(`git archive HEAD` → `D:\xpe-h274b`, `ci-preprocess`): 구성 exit 0, 빌드 317단계 exit 0. `build/ci-preprocess/bin/xpe_pre_e2e.exe` 생성 확인.
- 시험(한 번에 1개): gtest 계열 PreDefectOracle 5 + PreE2EReport 10 + PreE2ERun 30 + PreStripe 4 = 49개 통과 / 실패 0(`head-test2.log.txt`). 첫 실행은 정규식이 PreDefectOracle 을 놓쳐 46개였고(`head-test.log.txt`) 두 번째로 보완했다. `test_pre_test_evidence.py` 는 CMake 에 등록되어 있지 않아 `python -I -X utf8` 로 직접 실행: 6개 통과(`py-test.log.txt`).
- 미실행: 전체 스위트(CI 몫). `xpe_pre_e2e.exe` 를 실제 영상으로 돌리지 않았다(시험 안의 합성 입력만).
- 남은 빌드·시험 프로세스 0(보호 GUI 3개는 그대로).
