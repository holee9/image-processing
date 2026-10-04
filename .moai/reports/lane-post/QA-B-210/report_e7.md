# QA-B-210 E7 — 양방향 필터 sigma_space 상한 (사용자 결정, #251)

기준: `dev/postprocess` 84ba478f 위. Refs #251. (QA-B-210 의 C11, Codex #119 낮음은 아직 시작하지 않았다 — 리더가 QA-B-207f 를 먼저 지시.)

## 상한의 유도
`noise_reduce.cpp` `apply_bilateral`: `maxRad = min(15, min(w,h)/2 - 1)`, `radius = (sigma >= 0.5*maxRad) ? maxRad : ceil(2*sigma)`. 2σ 범위가 반경 상한 15 안에 다 들어가는 것은 `ceil(2*sigma) <= 15`, 즉 **sigma_space <= 7.5** (float 로 정확) — 이것이 상한이다. 7.5 를 넘으면 반경이 15 로 고정되어 2σ 보다 작다 — 공간 가중치 `exp(-0.5 d²/σ²)` 는 요청한 σ 를 계속 따르지만(거리 10 에서 σ=7.5 면 0.411, σ=8 이면 0.458) 커널이 2σ 전에 잘려, 매개변수가 요구하는 가우시안 모양이 아니다. (E7b 정정: 처음에는 "7.5 처럼 동작한다·지정값이 무시된다" 고 적었으나 코드와 달랐다. 가중치는 계속 바뀐다.) 아주 작은 영상은 반경이 `min(w,h)/2 - 1` 로 더 제한되는데 이는 커널이 영상에 안 들어가는 것이지 매개변수 문제가 아니므로 그대로 둔다.

## 재현 → 수정
- 수정 전: 상한 초과 sigma(7.5+ε, 8, 100, 1e6, FLT_MAX)가 `XPE_OK` 로 처리됨(반경만 15 로 잘리고 가중치는 σ 를 따름) → 새 시험 `BilateralFilter_SigmaSpaceAboveTheCap_...` 빨강 (`e7_red.txt`). 대조: 상한 이하 시험은 초록.
- 수정: 상수 `kMaxBilateralRadius = 15`, `kMaxSigmaSpace = 0.5f * 15 = 7.5f`. 검증 단계(영상을 건드리기 전)에서 `sigma_space > 7.5` 이면 `XPE_ERR_INVALID_INPUT`, 버퍼 불변. 헤더: 필드 주석 `0 < s <= 7.5 (default 3.0)`, `@return` 에 상한과 근거.

## 시험
- 새: 상한 초과(7.5+ε, 8, 100, 1e6, FLT_MAX) → INVALID_INPUT + 버퍼 바이트 불변. 상한 이하(0.1, 3, 7.4999995, 7.5) → OK + 영상이 실제로 필터됨.
- 바꾼 기존 시험 2건(상한과 모순): `test_exception_guard` 의 "큰 유한 값도 합법" → 상한 이하 OK / 초과 INVALID_INPUT + 영상 유한(불변), `test_parameter_dependency` 의 sigma_space 3→8 을 3→7 로(8 은 이제 거부).
- 상한 이하 출력 불변: 옛 DLL(상한 도입 전) 과 새 DLL 로 36회 실행(영상 4종 x sigma 9개, 7.5 포함) 비교 → **407,304 float 전부 바이트 동일** (`e7_output_unchanged.txt`).

## 증거
| 주장 | 산출 | 관측 |
|---|---|---|
| 수정 전 빨강 | `e7_red.txt` | 상한 초과 시험 FAILED |
| ci-post 전체 | `e7_ctest_post.txt` | 1451/1451 passed |
| 상한 이하 출력 불변 | `e7_output_unchanged.txt` | byte-identical: True, 0 differing |
| Doxygen | `e7_doxygen.txt` | exit 0 |

## 소비처 (grep 결과, 리더 확인 필요)
- **`clients/ImageProcTest/MainWindow.xaml.cs:2202`**: `NoiseSigmaSpaceTextBox` 를 `ReadFloat(..., min: 0.1f, max: 100f)` 로 읽는다 → 사용자가 **7.5 초과 100 이하**를 입력하면 이제 `XPE_ERR_INVALID_INPUT` 이다(이전에는 7.5 처럼 동작). 클라이언트 입력 상한을 7.5 로 맞춰야 한다(클라이언트 레인 소유).
- `clients/.../EnhanceBasicStageTests.cs:203`: 헤더 줄에 `sigma_space;[^\n]*\(default 3\.0\)` 정규식 — 필드 주석의 `(default 3.0)` 를 그대로 둬서 계속 맞는다.
- `gui`: `BaselineParameters.NoiseSigmaSpace = 3.0` 고정, 영향 없음. `tests/e2e_post_pipeline`: 3.0, 영향 없음. 레인 안 시험의 다른 호출은 모두 3.0 이하 확인.

## Gaps
- `clients` 의 UI 입력 한도(100)는 코드를 읽기만 했고 실행하지 않았다. 7.5~100 입력 시 UI 가 어떤 오류를 보이는지는 미관측.
- 요구 문구(REQ-ENH-007/020)는 리더 몫.
