# QA-A-199 (#216) — xpe_preprocess_version 계약 시험 (REQ-P1A-106 초안의 Verification)

시작 HEAD `71847891`. 요청(리더): 비-NULL, 호출마다 같은 포인터·같은 내용, 초기화 전·후·shutdown 후 같은 값, 값은 고정하지 않고 모양만. 반증: 초기화 상태에 따라 값이 달라지게 한 팔.

## 결과

시험 4건 (`modules/preprocess/tests/test_preprocess_version.cpp`, `PreprocessVersion.*`) — 전체 **750 통과, 0 실패** (직전 746), 셔플(시드 19900) 종료 0, 새 시험 20회 반복 통과, `ctest -N` 총계 867·DISABLED 40, 프리셋 점검 OK. 제품 코드는 바꾸지 않았다 (`xpe_preprocess_version` 은 그대로).

| 시험 | 고정하는 것 |
|---|---|
| `IsNeverNullAndNonEmpty` | NULL 이 아니고 길이 > 0 (초기화 전 상태에서) |
| `IsStaticSameAddressAndSameContentOnEveryCall` | 100회 호출이 같은 주소·같은 내용 |
| `DoesNotDependOnTheInitializationState` | 초기화 전·초기화 후·shutdown 후 같은 주소·같은 내용. 각 단계 앞에서 `xpe_preprocess_is_initialized()` 로 상태를 단언 (시험이 실제로 두 상태를 지났음을 확인) |
| `HasTheShapeDigitsDotDigitsDotDigits` | 값은 `숫자.숫자.숫자` 모양. 값("0.1.0")은 고정하지 않음. 같은 시험 안에서 모양 검사가 `v0.1.0`·`0.1`·`0.1.0.4`·`0..1`·빈 문자열을 거절하고 `12.0.345` 를 받아들이는지 대조 |

각 시험은 `SetUp` 에서 `xpe_preprocess_shutdown()` 으로 초기화 전 상태에서 시작한다 (앞선 시험이 모듈을 켜 둔 채 끝났을 수 있다).

## 구현 전 빨강이 없는 이유

`xpe_preprocess_version` 은 이미 이 계약을 지키는 상수 반환이다. 그래서 시험은 도착 때 초록이고, "빨강 먼저"는 해당하지 않는다. 판별력은 아래 반증 팔이 보인다 — 계약을 지키지 않는 구현을 만들어 시험이 빨개지는지 본다.

## 반증 팔 (`evidence/03_arms_summary.txt`, 매 팔 `BUILD_EXIT=0`, 원복 후 파일 동일)

| 팔 | 함수를 이렇게 바꿈 | 빨개진 시험 |
|---|---|---|
| V1 | 초기화 상태이면 `"0.1.1"`, 아니면 `"0.1.0"` (요청된 팔) | `DoesNotDependOnTheInitializationState` 만 |
| V2 | 초기화 전에는 `NULL` | 4건 전부 |
| V3 | 호출마다 다른 문자열 상수(주소·내용이 번갈아) | 정적성 시험, 초기화 무관 시험, 모양 시험 (세 건) |
| V4 | `"v0.1.0"` | 모양 시험만 |
| V5 | `""` | 비어 있지 않음 시험, 모양 시험 |

V1 이 요청된 팔이고 그 시험 하나만 빨갛다 — 초기화 무관 시험이 다른 세 시험으로 가려지지 않는다는 뜻이다.

## QA-A-198 문안 반영

`QA-A-198/spec_text_final.md`: REQ-P1A-106 의 Verification 을 "없음"에서 이 시험으로 바꾸고, 구현 상태 표의 `Implemented (no test)` 를 `Implemented` 로, 근거 표의 "코드 읽기만 (시험 없음)" 칸을 시험·반증 팔 요약으로 바꿨다.

## Gaps / Residual-risk

- 값의 모양 검사는 `숫자.숫자.숫자` 만 본다. `+빌드메타` 나 `-rc1` 같은 접미사가 붙는 버전 체계로 바꾸면 이 시험이 빨개진다 — 그때 시험과 REQ-P1A-106 의 "모양" 서술을 함께 고친다.
- "프로세스 수명 동안 유효"는 같은 주소가 반복된다는 것으로 보았다. 다른 스레드에서의 호출, DLL 언로드 뒤의 사용은 시험하지 않았다.
- `"0.1.0"` 과 CMake 프로젝트 버전 `1.0.0` 의 불일치는 이 카드의 범위가 아니라 요구에 값을 고정하지 않기로 한 결정 사항이다 (그대로).
