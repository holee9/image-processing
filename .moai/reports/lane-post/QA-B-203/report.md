# QA-B-203 — 모든 시험이 `DISABLED_` 인 재현 실행 파일이 0개를 실행해 CI 게이트에 걸림 (#251)

카드: `.moai/lanes/post/inbox/QA-B-203.md`. 관측(리더): main `b3458366` post-build — "Run post test binaries as one process each (#162)" 단계에서 `test_display_repro_qa_b_200.exe: exit=0 ran=0 … ran 0 tests - a run that executes nothing is not a pass`.

## 0. 결과

| 항목 | 내용 |
|---|---|
| main 기준 수정 | 커밋 **`e08ff374`**(브랜치 `WT-qa-b-203-fix`, 부모 main `b3458366`, 임시 워크트리 `D:/workspace-github/xpe-post-203`). `modules/display/tests/test_repro_qa_b_200.cpp`, `modules/dicom/tests/test_repro_qa_b_200.cpp` 두 파일만, CMake 변경 없음. main 위에서 만들었으므로 main 에 그대로 cherry-pick 된다. |
| 내 브랜치 | 같은 뜻의 수정을 내 브랜치의 display 파일에도 적용했다(이 커밋). 내 브랜치도 M2a 이후 display 재현 파일에 D5(DISABLED_) 하나만 남아 **같은 0개 실행**이었다(게이트로 확인, §2). dicom 재현 파일은 M2a 에서 `test_dicom_failure_paths.cpp` 로 대체되어 내 브랜치에는 없다. |
| 확인 | 두 실행 파일이 main 기준 빌드에서 각각 시험 1개를 실행해 통과(`run_main_based_*.txt`). 게이트를 로컬에서 돌려 post 시험 실행 파일 18개 전부 실행 개수가 0 보다 큼(§2). |

## 1. 시험마다 처분

| 시험 | 처분 | 근거 |
|---|---|---|
| display `D5_…` | (b) **활성 시험으로**, 단언을 현재 동작으로 뒤집음 | D5 는 결함이 아니라고 판정(사용자 결정: 문서를 코드에 맞춘다, `gsdfEnabled` 는 주석 성격). 같은 입력에서 플래그 0·1 의 출력이 같고 항목이 적용된다는 것을 단언한다(`D5 OBSERVED: … IDENTICAL (0 of 64 pixels differ)`). 이름도 `D5_GsdfEnabledDoesNotChangeWhatThePresentationLutDoes` 로. |
| display `DISABLED_D9_…` (main 에서만) | (c) DISABLED_ 유지 | 아직 main 에서 고치지 않은 결함의 재현(M2a 가 고치고 그때 그 시험이 `test_display_logging.cpp` 로 대체되며 이 파일에서 빠진다). 이 실행 파일에는 활성 시험(D5)이 있어 0개가 아니다. |
| dicom `DISABLED_C1_…`(2), `DISABLED_C9_…`, `DISABLED_C7_…`(2) (main 에서만) | (c) DISABLED_ 유지 | 아직 main 에 없는 수정(M2a~M2a4)의 재현. 실행 파일에 활성 대조 시험 `ControlTheModuleReadsBackWhatItWrote`(모듈이 쓴 파일을 그대로 읽음, 없는 파일은 −1)를 추가했다 — 재현들이 공유하는 전제(하니스가 모듈을 볼 수 있음)이고 이미 C1 재현의 선행 단언으로 쓰이던 것. |
| enhance_basic `DISABLED_E1_…` | 변경 없음 | 파일이 독립 실행 파일이 아니라 큰 `xpe_enhance_basic_tests` 안에 있어 같은 실행 파일에 활성 시험이 160개(게이트 출력) 있다. 내 브랜치에서는 M2b 가 이미 활성 시험으로 바꿨다. |

대체된 시험(a): 내 브랜치에서는 C1·C7·C9 재현이 `test_dicom_failure_paths.cpp` 의 `DicomExposureTag.*`·`DicomFailurePaths.*` 로, D9 가 `test_display_logging.cpp` 로 대체됐다 — 그러나 그 수정들이 main 에 아직 없으므로 main 기준 수정에서는 지우지 않고 DISABLED_ 로 두었다.

## 2. 게이트를 로컬에서 도는 방법과 결과 (`gate_local.py.txt`)

워크플로의 단계와 같은 일을 한다: `ctest --show-only=json-v1` 로 ctest 가 이름 붙인 시험 실행 파일을 모아 각각 `--gtest_filter=-*Performance*:*Within*ms*` 로 돌리고, 요약 줄이 없거나 실행 개수가 0 이하이거나 종료 코드가 0 이 아니면 실패로 센다. 사용: `python build/g203_gate.py <빌드 폴더(build/ci-post)> <출력 파일>` (ctest 가 PATH 에 있어야 함).

| 실행 | 결과 |
|---|---|
| **수정 전, 내 트리**(`gate_before_fix_my_tree.txt`) | `test_display_repro_qa_b_200.exe ran=0 → RAN 0 TESTS (executes nothing)` — CI 의 실패를 재현. (`xpe_ai_tests.exe` 는 루트 `schemas/` 가 없는 이 브랜치의 알려진 사정으로 빨강) |
| **수정 후, 내 트리**(`gate_after_fix_my_tree.txt`, 루트 `schemas/` 를 임시로 놓았을 때) | 18개 실행 파일 모두 `ran>0`, 게이트 실패 0 |
| main 기준 빌드의 두 재현 실행 파일 | display: `[  PASSED  ] 1 test.`, dicom: 시험 1 통과 + `YOU HAVE 5 DISABLED TESTS` (`run_main_based_display.txt`, `run_main_based_dicom.txt`) |

post 의 다른 0개 실행 파일은 없다(18개 전부 확인). dicom 은 `ci-post` 에 들어 있지 않고 별도 `dicom-build` 잡이 보통 `ctest` 와 "등록된 시험 수 > 0"만 검사하므로 이 게이트에 걸리지 않았을 뿐 같은 모양이었다(워크플로를 읽은 것).

## 3. 내 브랜치와의 병합 — 충돌 두 곳 (`merge_tree_main203_into_my_branch.txt`)

`git merge-tree --write-tree e08ff374 <내 브랜치 끝>` 의 결과: 충돌 2건.
- `modules/dicom/tests/test_repro_qa_b_200.cpp` — modify/delete(내 브랜치는 M2a 에서 `test_dicom_failure_paths.cpp` 로 대체해 지웠고 main 쪽 수정은 파일을 고쳤다). **해결: 삭제 유지**(대체한 시험이 이미 있다).
- `modules/display/tests/test_repro_qa_b_200.cpp` — 내용 충돌(머리말을 양쪽이 다르게 고쳤다). **해결: 내 브랜치 쪽을 택한다**(이 커밋이 같은 뜻의 수정을 이미 담고 있고 D9 관련 문구가 M2a 로 갱신돼 있다).
병합은 리더 결정이므로 하지 않았다. 병합할 때 위 두 해결을 쓰면 된다.

## 4. Gap / 잔여 위험
Gap
- GitHub 러너의 실제 post-build 단계를 돌리지는 못했다. 로컬 게이트는 워크플로 단계의 파싱을 옮긴 것이다(같은 정규식·필터, 단 PowerShell 이 아니라 Python).
- main 기준 빌드는 두 실행 파일만 지었다(전체 main 빌드가 아니다). 나머지 post 실행 파일의 개수는 내 트리의 빌드(수정 전후 같음)에서 확인했다.
- dicom 의 활성 대조 시험이 dicom CI 잡에서 도는지는 이번에 확인하지 않았다(로컬에서 통과).

잔여 위험
- 앞으로 `DISABLED_` 재현만 있는 새 실행 파일을 추가하면 같은 게이트에 걸린다. 재현을 쓸 때는 활성 시험을 하나 두거나 기존 실행 파일에 합칠 것(각 재현 파일 머리말에 적었다).
