# QA-A-229 M2 — 헤더가 하지 않는 약속 걷기 (D4, #245)

## 1. 결과

`xpe_calib_load_gain`·`xpe_gain_correct` 헤더(`preprocess_api.h`)에서 kVp·SID 보간 약속을 "미구현 — REQ-015, #245" 로 바꿨다. 요구(REQ-P1A-015)는 지우지 않았다. 걷어낸 자리에 지금 kVp 입력이 하는 일을 한 줄 적었고, 그 한 줄을 시험으로 고정했다.

## 2. 바뀐 문장

| 위치 | 이전 | 이후 |
|---|---|---|
| `xpe_calib_load_gain` REQ-015 / AC-CAL-002 | "multi-SID interpolation", "interpolation table for kVp-specific gain" | 미구현 — REQ-015, #245. 로더는 이득 맵 하나(또는 선량 다항식 하나)만 읽고 kVp·SID 로 색인한 표는 없다. 지금 kVp 는 아무 일도 하지 않는다 |
| `xpe_gain_correct` AC-GAIN-002 | "Multi-SID gain interpolation" | 미구현 — REQ-015, #245, metadata 의 kVp / SID_mm 는 쓰이지 않는다 |
| `xpe_gain_correct` `@param metadata` | "including kVp and SID" | 비어 있지 않아야 하며 kVp·SID_mm 는 결과를 바꾸지 않는다 |

## 3. 걷어낸 문장이 사실이라는 증거

- 코드: `src/gain_correct.cpp`·`offset_correct.cpp`·`defect_correct.cpp`·`pipeline*.cpp`·`preprocess.cpp` 에서 `metadata->` / `metadata.` 로 필드를 읽는 줄이 0건(grep). metadata 는 NULL 검사만 받는다. 이 grep 은 "읽는 줄 0" 만 보이며, 대조군(같은 명령으로 실재하는 필드 읽기를 찾는가)은 다른 모듈의 `->kVp` 를 찾는 것으로 별도로 두지 않았다 — 그래서 아래 시험이 주 증거다.
- 시험(신규): `GainPolyNotAppliedTest.KvpAndSidDoNotChangeTheCorrection` — 같은 프레임을 kVp/SID 4쌍(80/1000, 40/1800, 150/600, 0/0)으로 보정해 모든 화소가 입력/2(독립 계산)이고 네 결과가 바이트 단위로 같음을 단언한다.
- 반증: b1 제품이 `kVp == 150` 일 때 화소 3 을 1% 바꿈 → 빨강(305.5 vs 308.55), b2 제품이 `SID_mm == 0` 일 때 화소 0 에 1 을 더함 → 빨강(250 vs 251). 두 손상 모두 원본으로 복원(`git status` 에 헤더·시험만 수정). 전체 실행 960 통과·8 건너뜀·종료 0 (M1 의 959 + 신규 1).

## 4. 같은 부류로 보였으나 손대지 않은 것 (리더 판단)

같은 헤더의 `xpe_offset_correct` 주석(209~211행)에도 "temperature interpolation between two offset maps"(AC-OFF-002), "PREP-time exponential decay model"(AC-OFF-003) 가 있다. QA-A-21 이 온도·적분시간 필드가 없음을 확인했고, 이번에 offset 쪽도 metadata 를 읽는 줄이 0건임을 grep 으로 봤다. 카드 범위가 kVp 라 걷지 않았다. 같은 처리(미구현 표기 + 현재 동작 고정)를 하려면 카드 한 장이다.

## 5. 미검증 (Gaps) · 잔여 위험

- 신규 시험은 이득 보정(스칼라 맵)만 고정한다. offset 보정과 파이프라인이 kVp 를 읽지 않는다는 것은 코드 grep 까지만 확인했다(시험 없음).
- 다항식 이득 맵 경로에서 kVp 가 무관한지는 시험하지 않았다(코드에서 metadata 를 읽는 줄이 없다는 것이 근거).
- doxygen 1.12.0(CI 와 같은 버전, `Doxyfile` 그대로, awesome-css 는 xpe-post 의 복사본)을 로컬에서 돌렸다: 종료 0, `warning` 0건(`evidence/doxygen_run.txt`). CI 잡 `doxygen-headers` 를 직접 관측한 것은 아니다.
