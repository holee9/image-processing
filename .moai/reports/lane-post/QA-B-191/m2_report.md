# QA-B-191 M2 — `xpe_bodypart_recognize` 프로세스 안 경로

**장난감 모델로 배선만 시험했다. 부위 인식 정확도·임상 성능·속도에 대해 이 보고서는 아무 말도 하지 않는다.** SRS-AI-010 의 "CNN 분류기"는 충족되지 않았다.

## 1. 주장 (Claim)

ONNX 빌드에서 `xpe_bodypart_recognize` 가 `<modelDir>/bodypart.onnx` 를 돌려 라벨·confidence 를 낸다(스텁 빌드는 지금과 동일). 모델이 답을 정하고(모델 교체 → 라벨 교체), 영상이 답을 정한다(윗부분 밝음 → 다른 라벨). 쓸 수 없는 모델의 모든 경우는 스텁의 결과(`UNKNOWN`, 0.0, `PROCESSING_FAILED`)이고 세션당 Warning 1건이다. 문턱 비교·저신뢰 이벤트·`fallback_mode` 는 M3 이고 이 커밋에 없다.

## 2. 바뀐 것

- `src/ai_bodypart.h`(신규, 헤더만): `BodyPartInputSize`(모델이 선언한 `[1,1,H,W]`/`[1,H,W,1]`에서 크기), `ResizeImageFloat`(줄일 때 면적 평균, 늘릴 때 이중선형, 같은 크기는 비트 동일).
- `src/ai.cpp`: `BodyPartModel`·사이드카 라벨 읽기·모델 적재, 지연 생성과 세션 소유(뼈 억제와 같은 패턴), 출력 검증(유한·[0,1]), 가장 큰 값의 라벨(동률은 앞), `extern "C++"` 구현 + `try/catch` 감싸기(QA-B-181 의 규칙). 스텁 빌드 판정은 `OnnxSession::IsStubBuild()`.
- `include/xpe/ai/ai_api.h`: 이 함수의 계약(모델·사이드카 위치, 입력 모양, 확률 출력, 실패 결과, 알림 문구 전체, "분류기가 아니다", M3 미구현 명시).
- `CMakeLists.txt` + `tests/test_bodypart_inference.cpp`(신규 30건).

## 3. 증거 (Evidence)

- **ci-ai 전체 시험**: `[  PASSED  ] 344 tests.`, 349 중 스킵 5(스텁 전용 시험), 0 실패(`m2_runs_summary.txt`). 새 30건 전부 통과, **이름에 bodypart 가 든 기존 16건도 16/16 그대로 통과**(설계 때 기준선 16/16).
- **스텁 빌드(ci-post) 전체**: `[  PASSED  ] 298 tests.`, 새 `BodyPart.*` 21건 중 1건 통과·20건 스킵(풀 빌드 전용), `BodyPartResize`/`BodyPartInputSize` 9건 통과.
- 컴파일 경고 0(두 빌드), 텍스트 린트 0 오류, doxygen 종료 코드 0 / 경고 0(헤더를 고쳤으므로).
- **변경 시험**(`m2_arms_out.txt`, `build_ok=True` 확인, 바이트 동일 복원, 대조 `CONTROL: passed=['40'] red=[]`):

| 약화 | 빨강 |
|---|---|
| B1 라벨을 항상 첫째로 | `ADifferentModelDirectoryChangesTheLabel`, `TheImageChangesTheLabel`, `ANhwc…`, `AnImageOfAnotherSize…` |
| B2 영상을 무시(모델에 0 을 줌) | `TheImageChangesTheLabel`, `ANhwc…`, `ATie…`, `AnImageOfAnotherSize…` |
| B3 크기 조정이 첫 표본만 읽음 | `BodyPartResize` 5건 |
| B4 유한 검사 무력화 | `ANonFiniteModelResult…` |
| B5 범위 검사 무력화 | `AProbabilityAboveOne…`, `ANegativeValue…` |
| B6 경고가 매 호출 반복 | 쓸 수 없는 모델 6건 |
| B7 동률이 마지막 클래스로 | `ATieGoesToTheFirstClass…` |
| B8 경고 심각도 Info | 쓸 수 없는 모델 6건 |
| B9 목표 크기를 512 로 고정 | 10건 |
| B11 사이드카 없어도 수용 | `MissingLabelsAre…` |
| **B10 적재 시 라벨 수 검사만 제거** | **빨강 없음** — 아래 §4 |

## 4. 터지지 않은 반증 하나 (B10)

적재 시점의 "출력 크기 ≠ 라벨 수" 검사를 약화해도 시험이 모두 통과한다. 같은 상황을 **실행 뒤 `out.value.size() != labels.size()` 검사가 같은 경고 문구로 잡기 때문**이다(이중 방어). 이 검사를 둘 다 빼면 라벨 배열 밖을 읽으므로 그 반증은 만들지 않았다. 앞의 검사는 "고정 출력 크기가 맞지 않는 모델을 아예 적재하지 않는다"는 이른 거절이고, 뒤의 검사는 동적 출력 크기 모델의 안전망이다 — 지우지 않는다(안 터진 반증 ≠ 중복 방어의 증거).

## 5. 구현 중에 부딪힌 것

1. **UINT16 영상이 모델 없음 경로를 깼다.** 첫 구현은 형식 검사를 모델 적재 앞에 두어 기존 시험 4건이 빨개졌다(`UNSUPPORTED_FORMAT` 이 스텁의 `PROCESSING_FAILED` 를 가로챘다). 설계의 "모델 없음 = 스텁과 동일 출력"을 지키려고 **형식 검사를 모델이 쓸 수 있다고 확인된 뒤로** 옮겼고, 시험(`WithoutAModelNoImageFormatChangesTheOutcome`)으로 못 박았다.
2. **내 인용이 인용 검사기에 걸렸다.** 헤더 주석에 SRS 전용 ID `REQ-AI-BP-001` 을 적었더니 `check_req_citations.py` 가 새 고아 인용으로 잡았다(SPEC 에 정의가 없고 SRS 에만 있다). `SRS-AI-010` 으로 고쳐 `no new orphans`.
3. **첫 프로브의 정확 비교가 0.8 에서 어긋났다**(M1): 이진으로 정확한 0.75 로 바꿨다. M2 시험도 같은 값을 쓴다.

## 6. 결정하지 않은 것 / 미검증

- **성공 알림**: 설계 표에 "통과 시 AI-processed Info 를 낼지는 결정 필요"가 있었고 리더 결정에 없다. **내지 않았다**(뼈 억제의 알림 문구는 뼈 억제 전용이라 재사용할 수 없고, 새 교차 레인 문구를 승인 없이 만들지 않았다). M3 전에 정해 주면 된다.
- 크기 조정의 **지연 시간**은 재지 않았다(PRD "≤ 300 ms"). 4096×4096 → 512×512 가 오류 없이 도는 것과 평균 보존만 시험했다.
- 실행(`Run`) 자체가 실패하는 경우는 로그만 남기고 알림이 없다(뼈 억제와 같은 선택). 반복하면 조용히 `UNKNOWN` 이다.
- 동시성은 기존 `ConcurrentBodypartRecognizeIsThreadSafe` 가 풀 빌드에서 통과하는 것 외에 새 시험이 없다(락 아래에서 세션을 만들고 돌린다).
- 워커 경로·시간 예산(092)은 M4.

## 7. 잔여 위험

- 장난감 모델의 통과가 실제 모델의 동작으로 읽힐 수 있다 — 헤더·시험 머리 주석·사이드카가 "분류기가 아니다"를 적고 있다.
- 쓸 수 없는 모델의 Warning 은 세션당 1건이라, 같은 세션에서 모델이 고쳐진 뒤 다시 깨지면 두 번째 문제는 알리지 않는다.
