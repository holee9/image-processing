# QA-B-195c — Codex #90 보류 2건: 라벨 로딩의 메모리 부족 오분류, 사이드카 중복 키 O(n²)

카드: QA-B-195c · 관련: #130 · 근거: Codex #90 · 선행: QA-B-197 M2 `b090efd5`

## 주장 (Claim)

1. **라벨 로딩의 `bad_alloc` 은 이제 "메모리 부족"이다.** `LoadBodyPartLabels` 는 JSON 파싱과 라벨 벡터 구축 전체를 `catch (const std::exception&)` 로 감싸 `bad_alloc` 이 "label sidecar is not valid JSON" 이 됐다. 이제 `bad_alloc` 을 따로 잡아 `outOfMemory` 로 알리고(`"out of memory"`), `LoadBodyPartModel` 은 이를 `BodyPartLoadFailure::kOutOfMemory` 로 돌려준다. 워커는 -2 ERROR 프레임(`model_unavailable` 없음), 프로세스 안 경로는 `XPE_ERR_OUT_OF_MEMORY`, 알림 없음, 다음 호출에서 다시 적재한다.
2. **라벨 파서도 이벤트 파서(SAX)** 다(`detail::LabelsSax`). JSON 문서를 만들지 않으므로 비어 있지 않은 문서의 소멸자 할당 실패로 프로세스가 끝나는 위험(195b 보고서가 남긴 것)이 이 경로에서 닫혔다. 최상위 `labels` 배열만 보며, 깊은 곳의 `labels` 키는 무시하고, 첫 거부 라벨에서 이유를 남기고 멈춘다. 거부된 사이드카는 라벨을 남기지 않는다(옛 구현은 거부된 라벨 앞의 라벨을 남겼다 — 유일한 의도적 차이).
3. **같은 모양의 catch 목록**(`catch (const std::exception&)` / `catch (...)` 를 `modules/ai/src` 전체에서 grep, `ai_log.h` 의 로그 삼킴은 제외):

   | 위치 | 모양 | 판정 |
   |---|---|---|
   | `ai_bodypart_model.h` 라벨 읽기 | `catch (const std::exception&)` 가 `bad_alloc` 포함 | **결함, 고침** |
   | `ai_onnx_session.cpp` 세션 생성 | `catch (const std::exception&)` | 바로 앞에 `catch (const std::bad_alloc&)` 가 있어 영향 없음 |
   | `ai_onnx_session.cpp` `Create` | `catch (const std::bad_alloc&)` | 이미 분리됨(195b) |
   | `ai.cpp` 두 곳(`boneSuppressViaWorker`, 워커 부위 인식) | `catch (...)` → `XPE_ERR_OUT_OF_MEMORY` | 반대 방향(모든 예외를 -2 로): -4 오분류 아님. 이번 카드 범위 밖 |
   | `ai.cpp` 공개 함수 4곳, `ai_worker_main.cpp`, `ai_ipc_bridge.cpp` | `bad_alloc` 먼저, 그다음 `catch (...)` | 분리됨 |

4. **사이드카 중복 키 검사가 선형이다.** 최상위 키마다 지금까지의 키를 선형 탐색하던 것을 해시 집합(`std::unordered_set`)으로 바꿨고, 첫 중복에서 파싱을 중단한다(`key()` 가 false). 중단하면 `sax_parse` 가 false 를 돌려주므로 중복 판정을 "not valid JSON" 판정보다 먼저 한다. 이유 문구("the key X appears more than once")와 첫 중복을 지목하는 동작은 그대로다.

## 증거 (Evidence)

- ci-ai(`build\g191-ai.bat *`): `[  PASSED  ] 518 tests.`(197 M2 의 506 → +12), 스킵 5, 실패 0, 컴파일 경고 0. `xpe_ai_oom_tests`: `[  PASSED  ] 13 tests.`(12 → +1). 스텁: `xpe_ai_tests` 411 통과(스킵 112), `xpe_ai_oom_tests` 6 통과 + 건너뜀 7. 로컬 doxygen 1.12.0 `exit=0`, 경고 0줄, `check_header_docs.py`: `20 headers, 0 declarations skipped as unparseable, 0 findings`.
- 새 시험(`test_ai_label_sidecar.cpp` 10건, `test_ai_worker_boundary.cpp` 2건, `test_ai_oom_injection.cpp` 1건):
  - **옛 구현과의 비교**: 옛 문서 기반 읽기를 시험 안에 그대로 두고(`ReferenceLabels`), 36개 입력(허용·라벨 배열 없음·거부 라벨·JSON 아님)에서 이유가 같고 허용되면 라벨이 같다. 거부되면 새 구현은 라벨을 남기지 않는다. 대조군(`ControlTheComparisonSeesADifference`): 거부된 사이드카에서 이유 문구가 정확히 나오고 라벨이 비는지를 따로 확인한다.
  - **메모리 부족**(시험 전용 훅 `TestSetBeforeLabelParseHook`, 파싱 시작과 라벨 저장마다 호출): 라벨 읽기 단위 — 호출 1~4 각각에서 `"out of memory"`, 예외가 밖으로 안 나감, 라벨 목록 비어 있음, 대조군은 훅이 1+3번 불리고 통과. 모델 적재 단위 — `kind == kOutOfMemory`, 모델이 반쯤 만들어져 남지 않음. 프로세스 안 C ABI(`xpe_bodypart_recognize`) — 호출 1~4 각각에서 -2, 알림 0, 다음 호출이 `CHEST`. **진짜 워커**(환경변수 `XPE_AI_TEST_FAIL_LABELS=k`) — -2, `LastModelUnavailable()` 거짓, 같은 PID, 시작 횟수 1, 다음 요청 성공(k = 1~4). C ABI 로 실패 수 1 → 다음 성공이 0.
  - **중복 키 선형성**: 상한 안에 꽉 찬 사이드카 — 고유 키 96325개(1048570 바이트) 파싱 **18.9 ms**, 한 키 174743번 반복 **0.0 ms**(거부, 이유 보존), 라벨 115966개 **8.7 ms**. 첫 중복 지목, 필수 키 중복 거부, 멤버 안쪽 중복은 최상위 중복이 아님.
  - **시간 상한은 "상한 추정"** 이다: 3000 ms. 근거 — 로컬 측정값(18.9 / 0.0 / 8.7 ms)의 100배 이상이면서, 선형 탐색으로 되돌린 반증이 같은 기계에서 **35239 ms** 였으므로 그 아래다. CI 기계는 로컬보다 느릴 수 있으니 측정값이 아니라 상한 추정으로 읽을 것.
- 반증(`arms_out.txt`), 매번 빌드 성공, 소스 바이트 동일 복원, 대조군(변경 없음 → 41 + oom 13 통과) 초록:
  - **L1** `bad_alloc` 을 따로 보고하지 않음(옛 catch 모양) → 5건 빨강(라벨 단위 1, 모델 적재 단위 1, 프로세스 안 1, 워커 2)
  - **L2** 적재 쪽이 라벨 메모리 부족을 무시하고 `kLabels` 로 보고 → 4건 빨강(라벨 단위 시험은 초록 — 층별로 따로 잡힘)
  - **L3** 중복 검사를 선형 탐색으로 되돌림 → 고유 키 시험 1건 빨강(35.2 s)

## 기준 (Baseline)

`dev/postprocess`, QA-B-197 M2 `b090efd5` 다음 트리. 위 명령들의 이번 실행 출력.

## 미검증 (Gaps)

- **"첫 중복에서 즉시 멈춤"은 따로 반증하지 않았다.** 해시 집합만으로 한 키 174743번 반복도 선형이라 결과와 시간으로는 멈춤 여부가 구분되지 않는다(멈춤이 없어도 이유와 시간이 같다). 멈춤은 구현에 있고 시험으로는 관측하지 않는다.
- L3 반증은 `std::unordered_set` 을 전부 도는 형태라 옛 `std::vector` 선형 탐색과 모양이 똑같지는 않다(35 s 는 그 형태의 값이다). 옛 형태의 로컬 측정값은 없다.
- 실제 운영체제 수준의 메모리 부족은 일으키지 않았고 훅이 `bad_alloc` 을 그 지점에서 던진다. 라벨 파서 위의 `std::string` 복사(`take()` 의 비교 문자열)나 sax 내부 할당이 던지는 경우는 훅이 아니라 `LabelSidecarOom` 의 네 지점으로만 덮었다.
- 라벨 읽기에는 할당 실패 스윕을 걸지 않았다. 사이드카 문서가 없어졌으니 이제 가능하지만(M5 스윕의 한계 문단은 고쳤다) 이번 카드에서는 하지 않았다.
- `ai.cpp` 의 `catch (...)` 두 곳은 모든 예외를 -2 로 돌려준다. 이번 결함의 반대 방향이라 건드리지 않았다.
- 문서(`sdd_ai.md` 등)의 라벨 읽기 서술은 리더 소유라 확인하지 않았다. `release` 프리셋 전체 빌드는 하지 않았다.

## 잔여 위험

- 시간 상한 3000 ms 는 매우 느린 러너에서 오탐할 수 있다(로컬의 약 160배 여유).
- `XPE_AI_TEST_FAIL_LABELS` 와 훅은 `XPE_AI_TEST_HOOKS` 빌드에만 있다(출하 빌드에서 코드로 확인했을 뿐 빌드하지 않았다).
