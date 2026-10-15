# QA-B-191 M4a — 판정·결정 함수 추출 (동작 불변)

## 주장 (Claim)
부위 인식의 출력 판정(유한 → [0,1] → 최대 클래스)과 저신뢰 알림 문구를 `modules/ai/src/ai_bodypart_decision.h` 의 순수 함수로 뽑았고, `ai.cpp` 의 프로세스 안 경로가 그것을 쓴다. 동작은 바뀌지 않았다. 워커 경로(M4b·M4c)는 같은 함수를 부를 것이다.

## 증거 (Evidence)
- 추출 전 기준선: ci-ai 361개 중 356 통과·5 스킵, 스텁 298 통과 (M3b 커밋 시점)
- 추출 뒤 ci-ai: 369개 중 364 통과·5 스킵, 실패 0 (`build/g191-m4a-full.txt` 의 `[  PASSED  ] 364 tests.`)
- 추출 뒤 스텁: 306 통과, 실패 0
- 늘어난 시험은 새 `BodyPartDecision.*` 8개뿐 (ci-ai 361→369, 스텁 298→306). 기존 시험은 전부 그대로 통과
- 컴파일 경고 0 (두 빌드), `python build/lint-tracked.py` 0 error
- 반증: 동점 규칙을 `>` → `>=` 로 약화하고 재빌드(BUILD=0) → `BodyPartDecision.OnATieTheFirstClassWins`, `BodyPart.ATieGoesToTheFirstClassAndAFullScaleProbabilityIsAccepted`, `BodyPart.TheThresholdIsTheConfiguredOneNotAConstant` 빨강. 헤더를 바이트 동일하게 복원(`cmp` 일치)

## 기준 (Baseline)
같은 트리 `dev/postprocess`, `cmake --build --preset ci-ai` 와 `ci-post`, 이 실행의 출력.

## 미검증 (Gaps)
- 워커 경로는 아직 없다. "두 경로가 같은 함수를 부른다"는 M4b·M4c 에서 시험으로 단언한다
- 알림 문구 중 "unavailable"·비유한·범위 밖 알림은 아직 `ai.cpp` 에 남아 있다(워커 경로가 같은 문구를 쓰도록 M4c 에서 옮길지 정한다)
- 동점 외의 판정 분기(범위·비유한)는 새 직접 시험과 기존 M2 시험이 함께 덮는다. 둘을 따로 반증하지는 않았다

## 잔여 위험
- 헤더가 `ai_finite.h` 에 기댄다. 워커 타깃이 이 헤더를 쓰려면 M4b 에서 include 경로·링크를 확인해야 한다.
