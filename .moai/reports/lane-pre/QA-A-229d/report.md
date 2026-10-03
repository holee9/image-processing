# QA-A-229d 보고서 — Codex #97 보류 2건

카드: QA-A-229d · Refs #245 · 브랜치 dev/preprocess

## 1. (높음) `xpe_calib_check_expiry` 가 잘못된 세션 칸을 허용

### 1.1 어떻게 틀렸는가 (한 줄)

229b 보고서는 "검사 11 이 check_expiry 에도 적용된다"고 적었지만, `xpe_calib_check_expiry.cpp` 는 헤더를 직접 읽어
magic·version 만 보고 `validate_xcal_header` 를 부르지 않는다. 호출 관계를 열어 보지 않고 `xcal_validator.cpp` 머리 주석의
낡은 "사용처" 목록("xpe_calib_check_expiry 포함")을 사실로 옮겨 적었다. 그 주석도 이번에 고쳤다.
229b 보고서의 해당 두 문장(검증 설명 끝 문장과 65행)은 정정 표시와 함께 고쳤다.

### 1.2 재현 (고치기 전, 실패하는 시험 먼저) — `evidence/10_red_check_expiry.txt`

시험 `SessionField.CheckExpiryJudgesTheSessionFieldLikeTheLoaders`(test_calib_session_consistency.cpp): 같은 파일을 세 로더 중 그 종류의 로더와
`xpe_calib_check_expiry` 에 보여 주고 같은 판정을 요구한다. 칸 값 여섯 가지(거절 넷: 종결 NUL 없음 / 종결 뒤 0 이 아닌 바이트 /
단독 연속 바이트 / 과잉 인코딩 두 바이트, 허용 둘: 올바른 UTF-8 / 빈 칸 — 허용 쪽이 대조군이다) × 맵 세 종류.
관측: 거절해야 할 4×3 = 12 조합 전부에서 로더는 `-4`(CONFIG_INVALID), check_expiry 는 `0`(OK). 허용 쪽 6 조합은 일치.

### 1.3 수정

- `xcal_validator.{hpp,cpp}`: 세션 칸 검사를 `validate_xcal_session_field(const XCalFileHeader&)` 로 꺼냈다. `validate_xcal_header` 의 검사 11 이 그것을 부른다
  (동작 변화 없음). 머리 주석의 사용처 목록을 사실에 맞췄다.
- `xpe_calib_check_expiry.cpp`: magic·version 뒤에서 `validate_xcal_session_field` 를 부른다. 다른 헤더 검사(치수·페이로드 길이)는 넣지 않았다:
  이 함수는 만료 질문 하나에 답하려고 헤더만 읽는 것이 계약이고, 거기에 치수 검사를 더하면 로더가 거절하지 않을 파일에도 반응이 달라질 수 있다.
- `preprocess_api.h`: `xpe_calib_check_expiry` 의 CONFIG_INVALID 문장을 실제 검사(magic·version·세션 칸, 헤더만)로 적었다.

### 1.4 반증 — `evidence/30_arm_A1_check_removed.txt`

check_expiry 의 공통 검증 호출을 (런타임에 거짓이 되는 조건으로) 무력화하고 전체 빌드: 시험이 12개 조합에서 빨강. 복원 뒤 초록.
(첫 시도는 `if (false && …)` 라서 `/WX` 가 빌드를 막았고, 그 실행의 "초록"은 옛 바이너리의 것이라 반증이 아니었다. 이어서 런타임 거짓 조건으로 다시 했다.)

## 2. (보통) 캐시의 헤더 확인과 설치 사이 파일 교체 경합

### 2.1 결정 (리더): 동시 파일 교체는 지원하지 않는다

경계를 헤더에 명시했다(`xpe_calib_load_{offset,gain,defect}_cached` 세 곳): "히트가 직접 로더와 같은 판정에 도달한다는 보장은 정지된 파일에 대한 것이다.
헤더 읽기와 설치 사이에 교체된 파일은 옛 헤더로 판정될 수 있다 … 지원 밖이며 어느 결과도 시험이 약속하지 않는다."
같은 블록의 낡은 문장도 고쳤다: "The session check is not repeated on a hit" 는 229b 이후 틀렸다 — 히트는 152바이트 헤더를 다시 읽어 항목이 만들어질 때의 헤더와
바이트 비교하고, 다르면 히트를 취소해 미스처럼 로드한다(세션 칸·만료가 그 헤더에 있다).

### 2.2 관측 (시험으로 보장하지 않음) — `evidence/40_interleave_observation.txt`, 소스 `41_interleave_observation_source.txt`

`xpe_cache_after_open_check_hook`(XPE_CACHE_TEST_HOOKS 가 켜진 oom 실행 파일에만 있는 훅)이 헤더 읽기와 설치 사이에서 파일을 바꾸게 했다
(크기·쓰기 시각은 되돌림). 훅 호출 1회 확인.

| 교체 내용 | 캐시 히트 응답 | 같은 파일에 대한 직접 로더 응답 |
|---|---|---|
| 세션 칸을 종결 없는 64바이트로 | 0 (OK) | -4 (CONFIG_INVALID) |
| 만료를 과거로 | 0 (OK) | -5 (만료) |

즉 이 인터리빙에서는 두 판정이 갈린다 — 리더의 결정대로 지원 밖 경계이고, 시험은 만들지 않았다. 관측 소스는 CMake 에 등록하지 않고
증거 텍스트로만 남겼다(커밋에 시험으로 들어가면 그 동작을 보장하는 것으로 읽힌다).
첫 관측 실행은 훅이 호출되지 않아(관측 시험에 내가 넣은 `xpe_preprocess_shutdown` 이 캐시를 비웠다) 두 판정이 `0`·`0` 으로 같게 나왔다. 훅 호출 횟수를 출력에 넣고
캐시를 유지한 채 다시 돌려 위 표를 얻었다. 앞의 출력은 관측이 아니라 훅이 안 돈 실행이었다.

## 3. 검증 (이번 트리, 이번 실행의 출력)

| 항목 | 관측 |
|---|---|
| 새 시험·관련 시험 | `SessionField.*:*CheckExpiry*:*Expiry*` PASSED 31 tests (`evidence/20_green_session_expiry.txt`) |
| preprocess 전체 | `xpe_preprocess_tests` PASSED 1008 tests (232 M2 의 1007 + 1), exit 0 — `evidence/60_preprocess_tests.txt` |
| oom 훑기 | `xpe_preprocess_oom_tests` PASSED 73 tests, exit 0 — `evidence/61_preprocess_oom_tests.txt` |
| 헤더 문서 | `check_header_docs.py`: `20 headers, 0 declarations skipped as unparseable, 0 findings`, exit 0 |
| doxygen 1.12.0 | exit 0 (`evidence/51_doxygen.txt`) |

## 4. Gaps / Residual-risk

- check_expiry 는 치수·페이로드 길이·체크섬을 여전히 보지 않는다(의도). "로더가 거절하는 파일은 check_expiry 도 거절한다"는 이제 magic·version·세션 칸에 대해서만 참이다.
  다른 거절 사유(치수 0, 페이로드 길이 불일치)로 거절되는 파일에는 check_expiry 가 OK 를 줄 수 있고, 시험도 그것을 검사하지 않는다.
- 인터리빙 관측은 오프셋 종류 한 가지로만 했다(게인·디펙트 히트 경로는 같은 `lookup_hit` 템플릿을 쓴다는 코드 읽기에 의존, 실행하지 않았다).
- 헤더의 "정지된 파일" 경계는 문서다. 이 경계를 어기는 호스트를 막는 장치는 없다.
