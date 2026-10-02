# QA-B-191 M3 — 문턱, 저신뢰 이벤트(REQ-AI-012), `fallback_mode`

**장난감 모델로 배선만 시험했다. 부위 인식 정확도·임상 성능·속도에 대해 이 보고서는 아무 말도 하지 않는다.**

## 1. 주장 (Claim)

성공한 결과의 confidence 가 문턱(설정 `confidence_threshold`, 기본 0.6)보다 **작으면**(같으면 통과) 저신뢰 이벤트로 Warning 1건을 영상마다 내고, `fallback_mode` 켬(기본)이면 `PROCESSING_FAILED` + `UNKNOWN` + **측정된** confidence, 끔이면 `OK` + 최상위 라벨 + 같은 Warning(끔 문구). 설정으로도 런타임(`xpe_ai_set_fallback_mode`)으로도 바뀐다. 지금까지 저장만 되던 `confidence_threshold`·`fallback_mode` 가 이 함수에서 처음 소비된다(`fallback_mode` 는 다른 함수에서는 여전히 소비되지 않는다).

리더 결정(D3, 설계 승인 2026-10-02)을 그대로 따랐다: 이벤트는 Info 가 아니라 **Warning**.

## 2. 바뀐 것

- `src/ai.cpp`: `pushLowConfidenceAlert`, `shortestFloatText`(`std::to_chars` 최단 표기 — 0.6f 는 `0.6`, 다음 float 는 `0.6000001`), 문턱 비교와 `fallbackMode` 소비, 저신뢰 분기.
- `include/xpe/ai/ai_api.h`: 문턱·이벤트·`fallback_mode` 계약과 알림 문구 전체, `confidenceOut` 설명 갱신.
- `tests/test_bodypart_inference.cpp`: 시험 11건 추가(총 41건).

알림 문구(교차 레인 계약, 헤더에 전체 기록):
- 켬: `AI body-part confidence {c} is below the threshold {t} (REQ-AI-012): UNKNOWN is returned; use the deterministic body-part lookup`
- 끔: `AI body-part confidence {c} is below the threshold {t} (REQ-AI-012): the label {LABEL} is returned because fallback_mode is off; an exposure parameter chosen from it may be wrong`

## 3. 증거 (Evidence)

- **ci-ai 전체**: `[==========] 360 tests`, `[  PASSED  ] 355`, 스킵 5(스텁 전용), 0 실패. `BodyPart*` 계열 41건, 이름에 bodypart 가 든 기존 시험 16건 모두 통과(`m3_runs_summary.txt`).
- **스텁 빌드 전체**: `[  PASSED  ] 298`, 0 실패. 새 `BodyPart.*` 32건 중 1건 통과·31건 스킵(풀 빌드 전용), 순수 도우미 시험 9건 통과 — 스텁 빌드의 동작은 M2 와 같다(문턱 코드는 모델이 있어야 도달한다).
- 컴파일 경고 0(두 빌드), 텍스트 린트 0, doxygen 종료 코드 0 / 경고 0, `check_req_citations`: `no new orphans`.
- **알림 문구는 시험에 문자 그대로 적었다**(모듈의 포매터로 다시 만들지 않았다 — 자기 자신과 비교하면 둘이 함께 틀려도 통과한다). 숫자 표기 `0.6` / `0.6000001` 은 파이썬에서 float32 로 독립 계산해 확인했다.
- **변경 시험**(`m3_arms_out.txt`, 모두 `build_ok=True`, 바이트 동일 복원, 대조 `CONTROL: passed=['51'] red=[]`):

| 약화 | 빨강 |
|---|---|
| C1 비교를 `<=` 로(같으면 저신뢰) | `AConfidenceExactlyAtTheThresholdPasses` 외 7건 |
| C2 `fallback_mode` 를 읽지 않음(늘 켬) | 끔 갈래 3건 |
| C3 문턱을 설정이 아닌 기본 상수로 | 9건(경계·설정·1.0 문턱 포함) |
| C4 이벤트를 올리지 않음 | 9건 |
| C5 저신뢰 fallback 이 측정값 대신 0.0 | `AConfidenceOneFloatBelow…`, `…NeedsRoomForUnknown…` |
| C6 알림 문구 한 단어 변경 | 3건 |
| C7 이벤트가 Warning 대신 Info | 6건 |
| C8 숫자를 3자리로 출력(한 float 차이를 잃음) | 5건 |

## 4. 구현 중에 정해야 했던 것

1. **저신뢰 + `bufLen` 7(UNKNOWN 이 안 들어감)**: `BUFFER_TOO_SMALL`, 아무것도 쓰지 않고 confidence 는 0.0 이다. **이벤트는 그래도 올린다**(이벤트는 영상의 것이지 호출자 버퍼의 것이 아니다). 시험으로 못 박았다.
2. **fallback 끔 + 라벨이 안 들어감**: `BUFFER_TOO_SMALL` + 이벤트 1건.
3. **문턱 범위 검사 없음**: 0.0 은 모두 통과, 1.0 초과는 모두 저신뢰. 설정을 그대로 쓰고 헤더에 적었다. 문자열 `"0.99"` 는 숫자가 아니므로 무시되고 기본 0.6 이 적용된다(시험).
4. **저신뢰는 "쓸 수 없는 모델"이 아니다**: "unavailable" 경고를 쓰지도 쏘지도 않는다(시험: 저신뢰 뒤에도 모델이 없는 세션은 여전히 그 경고를 1건 받는다).

## 5. 미검증 / 열린 것

- **성공 알림(`SRS-ALERT-004` 의 부위 인식 판)**은 여전히 결정되지 않았고 내지 않았다(M2 보고서 §6).
- `fallback_mode` 는 **이 함수에서만** 소비된다. `xpe_bone_suppress` 등은 읽지 않으며(SRS `FB-002` 는 "모든 AI 함수"), 이 카드의 범위가 아니다. SRS 문구의 "끄면 재시도"는 구현하지 않았다(정의가 없다; 설계 §3.2 에서 리더에게 정정 요청).
- 문턱과 `fallback_mode` 의 **동시** 변경(다른 스레드에서 `xpe_ai_set_fallback_mode` 를 호출하는 중의 `xpe_bodypart_recognize`)은 시험하지 않았다. `fallbackMode` 는 원자 값이고 문턱은 호출 중 락 아래에서 읽는 값이다.
- 알림 큐 도배: 저신뢰는 영상당 1건이다(리더 결정). 연속 저신뢰 영상이 많이 오면 큐의 오래된 Info 부터 밀려난다는 큐 정책(SRS-ALERT-007)은 이 카드에서 확인하지 않았다.
- 워커 경로·시간 예산(092)은 M4(별도 지시).

## 6. 잔여 위험

- 알림 문구 두 개가 새 교차 레인 계약이다. `clients`/`gui` 가 이 함수를 부르지 않으므로 지금은 소비자가 없지만, 소비자가 생기면 문구를 정규식으로 잡기보다 `UNKNOWN`+confidence 의 반환값을 신호로 쓰게 하는 편이 낫다.
- 장난감 모델의 통과가 실제 모델의 동작으로 읽힐 수 있다 — 헤더·시험 머리 주석이 "분류기가 아니다"를 적고 있다.
