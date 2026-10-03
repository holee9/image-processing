# QA-A-234 M2 — 패치 초안(ASan·clang-tidy·cppcheck·커버리지)과 내 두 모듈의 지적 처리

카드 끝 절 "리더 결정"(M1 c4bbec9a 수신)을 그대로 따랐다. 워크플로(`ci.yml`)·`CMakePresets.json`·`tools/` 는 리더 소유라 **패치 초안(`.txt`)** 으로만 남겼고, 저장소의 그 파일은 만들지도 바꾸지도 않았다(만들어 `git diff` 로 뽑은 뒤 원상 복구).

## 1. 패치 초안 — `patches/`

| 파일 | 내용 | 검증 |
|---|---|---|
| `01_asan_job.patch.txt` | `CMakePresets.json` 에 `ci-asan` 구성·빌드 프리셋(`/fsanitize=address /Zi`에 `/EHsc /GR` 유지), `ci.yml` 에 `asan-tests` 잡: ASan 런타임 도달 확인 단계(없으면 분명한 메시지로 실패), 빌드, `ctest -E 'ControlLeakIsCaught\|Frame3072SquaredWithinMachineRatio'`. 제외 3개와 각 이유를 **워크플로 주석에** 적었다(조용한 제외 없음). ASan 보고는 프로세스를 중단시키므로 보고 하나라도 나오면 잡이 실패 = 처음부터 게이트 | 깨끗한 HEAD 에 `git apply --check` 통과, 적용 뒤 YAML·JSON 파싱 통과 |
| `02_static_analysis_and_coverage.patch.txt` | `ci.yml`: `clang-tidy` 잡(기준선 게이트), `cppcheck` 잡(**측정 전용**), `coverage` 트리거(main 푸시는 `coverage` 프리셋만, post·dicom 은 주 1회 cron+수동). 새 파일 `tools/ci/check_clang_tidy.py`, `tools/ci/clang_tidy_baseline_{common,preprocess}.txt` | 깨끗한 HEAD 에 단독으로, 그리고 01 적용 뒤에도 `git apply --check` 통과(처음엔 같은 삽입 위치라 01 뒤에 충돌했고, 02 의 삽입 위치를 옮겨 해결했다). 적용 뒤 YAML 파싱 통과, 잡 4개 확인 |
| `check_clang_tidy.py.txt`, `clang_tidy_baseline_*.txt`, `*_jobs.yml.txt` | 위 패치의 원본 조각 | — |

**clang-tidy 게이트의 성질**: (점검, 파일) 쌍별 개수를 세어(줄 번호는 편집마다 움직여 쓰지 않는다) 기준선 파일과 비교, 기준선보다 많거나 기준선에 없는 쌍이 있으면 실패. 기준선보다 줄면 알리기만 한다. 파싱 오류가 하나라도 나면(개수가 무의미해지므로) 실패하고, 소스가 0개여도 실패한다. clang-tidy 가 러너에 없으면 그렇게 말하고 실패한다 — 첫 CI 실행이 `windows-2025` 에 있는지 드러낸다.
**반증**: 일부러 `helpers.cpp` 의 표지 주석을 지워 새 `empty-catch` 를 만들면 `NEW: bugprone-empty-catch helpers.cpp 1 (baseline 0)` 로 종료 코드 1 (`evidence/60_gate_fires_on_new_finding.txt`). 복원 뒤 `10 findings, baseline 10`, 종료 코드 0.

## 2. 내 두 모듈의 clang-tidy 지적 처리 (M1 측정 59건 → 10건)

| 구분 | 건수 | 처리 |
|---|---|---|
| `bugprone-easily-swappable-parameters` | 25 | **끔**(`modules/*/.clang-tidy`). 공개 C ABI 가 `(width, height)` 같은 순서를 고정해 인자를 바꿀 수 없다. 설계상 허용으로 판단했고 하나씩 열어 보지는 않았다 |
| `bugprone-empty-catch` | 20(common 11 + preprocess 9) | **전부 하나씩 열어 이유를 확인**했다. 20곳 모두 C ABI 경계의 "삼키는 catch": 경보(advisory) 푸시 실패, `void` 함수(shutdown), 로그 한 줄 — 던지면 호출자의 결과를 바꾸거나 던질 곳이 없다. 고칠 것은 없었고, 각 catch 에 `// [no-throw-boundary] <이유>` 를 달고 설정의 `IgnoreCatchWithKeywords` 에 그 표지를 등록했다(이유 없는 새 빈 catch 만 걸린다) |
| 고침 | 2 | `gain_correct.cpp`: `width * height`(uint32 두 개)를 `static_cast<size_t>(width) * height` 로 — 32비트에서 곱이 넘칠 수 있었던 곳(치수가 3072 급이라 지금은 넘치지 않지만 검증에 기대지 않게). `calibration_cache.cpp`: 쓰지 않는 `release()` 반환값을 이름 붙은 변수로 받음(`(void)` 캐스트는 이 점검이 인정하지 않았다 — 실측) |
| 기준선(남김) | 10 | `implicit-widening-of-multiplication-result` 2(`xcal_reader`·`xcal_validator`의 상한 상수 비교), `incorrect-roundings` 1(`nonlinearity_correct.cpp`, 이미 0 이상으로 제한된 값에 `+ 0.5`), `misplaced-widening-cast` 1(`xpe_calib_generate_gain.cpp`, `degree + 1`), `narrowing-conversions` 4(`xpe_verify_metrics.cpp`의 `size_t`→`double` 비율), `performance-enum-size` 2(`calibration_cache.cpp`). 열어서 값 범위를 보고 결함이 아니라고 판단한 것은 narrowing·rounding 이고, `degree + 1` 은 `degree` 가 앞에서 검증되는지 확인하지 않아 **건드리지 않고** 기준선에 남겼다 |

`.clang-tidy` 의 `HeaderFilterRegex` 와 게이트 스크립트의 `/modules/` 경로 거름은 `xfilesystem_abi.h`(MSVC 표준 헤더)에서 나온 `EnumCastOutOfRange` 2건이 module 지적으로 세어지던 것을 걷어냈다.

## 3. 검증 (이번 트리, 이번 실행의 출력)

| 항목 | 관측 |
|---|---|
| common | `xpe_common_logging_tests` 15/15, `test_xpe_common` 69/69, `xpe_common_oom_tests` 12/12 (`evidence/50_m2_*.txt`) |
| preprocess | `xpe_preprocess_tests` PASSED 1008, `xpe_preprocess_oom_tests` PASSED 73 (`51_`, `52_`) |
| clang-tidy | common 0건, preprocess 10건(기준선과 같음) |
| 패치 | 위 §1 표의 `git apply --check`·YAML 파싱 |

## 4. 제품 코드 변경 (Codex 대상)

`modules/common/src/xpe_common.cpp`·`xpe_logging.cpp`(주석뿐, 코드 동작 변화 없음), `modules/preprocess/src/` 8개 파일(주석 + 위 2건의 코드 변경), `modules/common/.clang-tidy`·`modules/preprocess/.clang-tidy`(새 파일). 동작이 바뀌는 곳은 `gain_correct.cpp` 의 곱셈 형 변환과 `calibration_cache.cpp` 의 반환값 수신 둘이다.

## 5. Gaps / Residual-risk

- **패치는 실제 CI 에서 한 번도 돌지 않았다.** `git apply --check`·YAML 파싱·로컬 도구 실행까지만 확인했다. 러너에 clang-tidy·ASan 런타임·cppcheck 설치 경로가 있는지, `choco install cppcheck` 가 되는지는 첫 실행이 드러낸다(스크립트·단계가 없으면 분명한 메시지로 실패하도록 했다).
- **cppcheck 잡은 측정 전용**이다. 이 기계에 없어 지적 수를 모른다. 첫 실행의 모듈별 수로 기준선을 만들고 게이트로 바꾸는 후속이 필요하다(그 게이트 스크립트는 만들지 않았다).
- **커버리지의 xpe_common 85% 별도 게이트는 이번에 만들지 않았다**(카드 3항 마지막 문장). `coverage_check` CMake 타깃이 모듈별 값을 내는지 열어 보지 못했다. 패치는 트리거만 바꿨고 판정선은 기존 합산 85% 그대로다. 후속 카드가 필요하다.
- ASan 잡의 제외 3개의 원인은 M1 에서 추정이었다(시험 내용을 열어 확인하지 않음). 제외가 맞는지는 첫 CI 실행과 한 번의 시험 읽기로 확인해야 한다.
- 기준선 10건 중 `degree + 1` 과 xcal 상수 비교 2건은 값 범위를 끝까지 확인하지 않았다.
- `xpe_common.cpp` 에 이유 주석이 기존 주석과 겹쳐 두 줄인 곳이 있다(표지 줄 + 기존 줄). 동작은 같다.
