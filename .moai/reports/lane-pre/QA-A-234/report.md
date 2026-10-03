# QA-A-234 M1 — 커버리지 자동 실행·정적 분석 CI 도입 설계

카드: QA-A-234 M1 · Refs #253 · 코드·워크플로 변경 없음(설계 메모와 측정). 사용자 결정(2026-10-03): "CI 에 실제로 도입".

## 0. 권고 단계

| 순서 | 무엇 | 게이트 | 근거 |
|---|---|---|---|
| 1 | ASan 시험 잡(common 전체, preprocess 는 3개 제외) | 처음부터 실패 게이트 | 이 트리에서 돌려 AddressSanitizer 보고 0건(§3) |
| 2 | cppcheck·clang-tidy 를 측정 전용으로 먼저, 이어 기준선 | 기준선(새 지적만 실패) | clang-tidy 59건(§2), cppcheck 은 미측정 |
| 3 | 커버리지를 main 푸시마다 자동 + post·dicom 은 주기 | 합산 85% 유지 + xpe_common 별도 확인 | 지난 수동 실행 0.886 통과 |
| 4 | MISRA | 선택 | 전체 MISRA 는 상용 도구 영역 |

## 1. 커버리지

- 지금: `coverage` 잡은 `workflow_dispatch` 전용. 마지막 수동 실행 run 34662146043(2026-09-12): `Coverage check PASSED: line-rate 0.8861908265881776 >= 0.85` (`coverage` 프리셋 = common+preprocess 합산).
- 요구 문구 REQ-P0-006: "…minimum statement coverage threshold of 85% for xpe_common.dll" — 범위는 xpe_common 하나. 합산 게이트는 그보다 넓고, xpe_common 만의 값은 로그에 없다.
- 지난 실행 시간(`gh api …/jobs`): coverage(common+preprocess) 약 16분 15초(빌드 65초, 시험·계측 14분), coverage-post 약 5분, coverage-dicom 약 24분(vcpkg 설정 21분, 캐시가 빈 첫 실행). 2026-09-12 한 번의 값이고 그 뒤 시험이 늘었다.
- 선택: push 마다(16분 x 푸시 수, 연속 푸시 취소와 겹치면 결과가 사라짐) / **main 푸시만(권고)** / 주기 cron(post·dicom 에 맞음).
- 85% 아래인 모듈: **모릅니다.** 로그에 합산값만 있고 아티팩트는 14일 보존이 지났다. 로컬 재측정(약 14분)과 `coverage_check` 타깃의 모듈별 출력 여부 확인은 하지 않았다(M2 의 첫 일).

## 2. 정적 분석

### clang-tidy (MSVC 툴체인)
- 설치: VS 2022 Professional 의 `VC/Tools/Llvm/x64/bin/clang-tidy.exe`, LLVM 19.1.5(`evidence/10_clang_tidy_version.txt`). GitHub 러너 VS 에 같은 경로가 있는지는 확인하지 않았다.
- 그대로는 안 돈다(실측 두 걸림): (1) `xpe_types.h` 의 `static_assert(offsetof…)` 5개가 clang 에서 오류 — `--extra-arg=-D_CRT_USE_BUILTIN_OFFSETOF` 로 0건이 됨(`/clang:-fno-ms-compatibility` 는 효과 없음). (2) `build/ci-preprocess/compile_commands.json` 에 preprocess 항목이 없어 `ninja -t compdb` 로 새로 뽑았다. CI 는 `CMAKE_EXPORT_COMPILE_COMMANDS=ON` 이 필요.
- 측정(`bugprone-*,clang-analyzer-*,performance-*`, `modules/<모듈>/src/*.cpp`, 파싱 오류 0 확인 뒤):

| 모듈 | 파일 | 지적 | 내역 |
|---|---|---|---|
| common | 3 | 11 | 전부 `bugprone-empty-catch`(경계에서 예외를 삼키는 의도된 catch) |
| preprocess | 33 | 48 | easily-swappable-parameters 25, empty-catch 9, narrowing-conversions 4, implicit-widening-of-multiplication-result 3, EnumCastOutOfRange 2, enum-size 2, unused-return-value 1, incorrect-roundings 1, misplaced-widening-cast 1 |
| post·gui 소유 모듈 | — | 측정 못 함 | 이 기계에 그 프리셋의 컴파일 DB 가 없다 |

  실행 시간: preprocess 33개 파일 약 52초. 증거 `evidence/20_clang_tidy_common.txt`, `21_clang_tidy_preprocess.txt`.
- 권고: 기준선 방식. `swappable-parameters`(25건, C ABI 의 `(width, height)` 류 소음)는 끄고, `empty-catch` 는 사유 주석 또는 제외를 정해야 한다(이 결정이 건수의 절반 이상을 좌우). 진짜 후보는 narrowing·widening·EnumCastOutOfRange·unused-return-value 약 11건이나 결함인지는 열어 보지 않았다(점검 이름으로만 센 것).

### cppcheck
이 기계에 없다. 설치는 개발자 기계를 바꾸므로 하지 않았다. **지적 수 미측정.** 러너 설치는 ninja 에 쓰는 `choco install … -y --no-progress` 패턴(ci.yml)을 따르면 된다. 첫 CI 실행을 측정 전용(실패 안 함, 결과 아티팩트)으로 하고 기준선을 고정한 뒤 게이트로 바꾼다.

### MISRA
전체 MISRA C++ 검사는 상용 도구(예: Helix QAC, Polyspace) 영역이다. 무료로는 cppcheck 의 MISRA 애드온이 일부 규칙을 보는 정도로 알고 있으나 돌려 보지 않았고 규칙 본문 라이선스도 확인하지 않았다. CI 가 MISRA 준수를 보증한다고 문서에 쓸 수는 없다.

## 3. ASan (MSVC `/fsanitize=address`)

기존 `build/asan-a17`(플래그 `/fsanitize=address /Zi`, RelWithDebInfo, 2026-09-10)을 현재 트리로 증분 빌드: 71초. 흠: 플래그를 덮어써 `/EHsc` 가 빠져 `C4530` 경고 — CI 프리셋은 `/EHsc` 를 유지해야 한다.

| 실행 | 결과 | 시간 | AddressSanitizer 보고 |
|---|---|---|---|
| test_xpe_common | 69/69 통과 | 약 0.2초 | 0건 |
| xpe_preprocess_tests | 1005 통과, 3 실패 | 5분 6초 | 0건 |

증거 `evidence/30_asan_run.txt`, `31_asan_common_tests.txt`, `32_asan_preprocess_tests.txt`. 실패 3개는 ASan 이 찾은 결함이 아니라 계측 아래에서 성질이 바뀌는 시험으로 추정한다(내용은 열어 확인하지 않았다): `EnduranceTest.LoadCycles_ControlLeakIsCaught`, `XpePreprocessEndurance.ControlLeakIsCaught`(누수를 힙 크기로 재는 대조 시험), `RuntimeDetectionPerformanceGateTest.Frame3072SquaredWithinMachineRatio`(54,969 ms, 시간 게이트).
권고: 이 3개를 `--gtest_filter` 로 제외(post-build 잡의 "timing-budget cases excluded" 선례), 처음부터 실패 게이트. oom 시험과 다른 실행 파일은 돌리지 않았다.

## 4. 다른 레인 모듈
post·gui 모듈의 도구 지적은 측정하지 못했다(§2). 모듈별 단계로 분리해 두면 그 레인의 지적이 내 게이트를 막지 않는다.

## 5. M2 제안 (리더가 정한 뒤)
1. ASan 잡 패치 초안(`.txt`) + 제외 3개 + `/EHsc` 유지 프리셋.
2. clang-tidy: 정의 `-D_CRT_USE_BUILTIN_OFFSETOF`, 컴파일 DB 생성, 점검 집합 결정, 내 두 모듈 지적 처리, 기준선 파일.
3. cppcheck: 측정 전용 → 기준선 → 게이트.
4. 커버리지: `coverage_check` 의 모듈별 출력 확인, main 푸시 트리거 패치 초안.

## 6. Gaps / Residual-risk
- 미측정: cppcheck 수, 커버리지 모듈별 값, post·gui 지적.
- "59건"은 내가 고른 점검 집합(`bugprone`·`clang-analyzer`·`performance`)에서의 수이고 지적이 곧 결함은 아니다.
- 러너에 clang-tidy·LLVM 경로가 있는지, ASan 런타임 DLL 이 러너 PATH 에 있는지 확인하지 않았다(로컬은 vcvars 가 채운다).
- ASan 제외 3개의 원인은 추정이다.

## 7. 재개 메모 (리더 결정 반영: 2026-10-03)

결정: ASan 게이트(제외 3개는 이유를 주석으로), cppcheck·clang-tidy 기준선, 커버리지 main 자동, MISRA 안 함.
다음 세션이 할 일 (M2, 패치 초안은 `.txt`, ci.yml 은 리더 소유):
1. ASan 잡 패치 초안: 프리셋은 `/EHsc` 유지, `build/asan-a17` 구성 참고. 제외 필터는 `EnduranceTest.LoadCycles_ControlLeakIsCaught`, `XpePreprocessEndurance.ControlLeakIsCaught`, `RuntimeDetectionPerformanceGateTest.Frame3072SquaredWithinMachineRatio`(이유 주석 필수).
2. clang-tidy 기준선: `-D_CRT_USE_BUILTIN_OFFSETOF`, `CMAKE_EXPORT_COMPILE_COMMANDS=ON`(또는 `ninja -t compdb`), 점검 집합 결정, 내 두 모듈 59건 중 실제 결함 후보(narrowing·widening·EnumCastOutOfRange·unused-return-value) 확인과 처리. 측정 도구 스크립트는 세션 스크래치패드에 있었으나 사라질 수 있다 — `evidence/20_`, `21_` 의 점검별 수로 재현한다.
3. cppcheck: 러너에서 `choco install cppcheck` 로 측정 전용 첫 실행 → 기준선. 이 기계에는 없다.
4. 커버리지: `coverage_check` 타깃의 모듈별 출력 확인(xpe_common 85% 별도), main 푸시 트리거 패치 초안.
상태: HEAD 는 이 메모 커밋. 푸시 안 함. 미푸시 커밋은 `git log origin/dev/preprocess..HEAD` 로 확인.
