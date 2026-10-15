# QA-B-192 — 쓰이지 않는 워커 프로토콜 플래그 정리

카드: QA-B-192 · 관련: #130 · 근거: QA-B-191 M4d 초안의 발견 ③

## 주장 (Claim)

`XPE_AI_FLAG_LOW_CONFIDENCE`(0x4)와 `XPE_AI_FLAG_FALLBACK_MODE`(0x8)를 쓰는 코드는 저장소에 없었다. 두 매크로를 지우고 "예약됨, 쓰지 않음" 주석으로 바꿨다. 동작·시험 수·와이어 형식은 바뀌지 않았다.

## 증거 (Evidence)

1. 정의·인용 전수 검색: `XPE_AI_FLAG_|LOW_CONFIDENCE|FALLBACK_MODE|skips retries` 를 `modules`, `clients`, `gui`, `tools`, `docs`, `cmake`, `tests` 에서 검색(코드·시험·문서), 추가로 `.moai/specs`, `.moai/project` 에서 두 이름만 검색.

| 이름 | 정의 | 읽는 곳 | 쓰는 곳 | 인용 문서 |
|---|---|---|---|---|
| `XPE_AI_FLAG_LOW_CONFIDENCE` | `ai_worker_protocol.h:157` | 0 | 0 | `sdd_ai.md:253` 가 "LOW_CONFIDENCE flag" 라고 서술(매크로 이름은 아님) |
| `XPE_AI_FLAG_FALLBACK_MODE` | `ai_worker_protocol.h:160` | 0 | 0 | 0 |
| **대조군** `XPE_AI_FLAG_HAS_BINARY_PAYLOAD` | `ai_worker_protocol.h:151` | `ai_ipc_bridge.cpp`(2곳), `ai_worker_main.cpp`(1곳) | `ai_ipc_bridge.cpp`(2곳), `ai_worker_main.cpp`(1곳), 시험·가짜 워커 | 헤더 주석 |

   같은 검색식이 실제로 쓰이는 플래그는 소스 6건(`ai_ipc_bridge.cpp` 4, `ai_worker_main.cpp` 2)·시험 10건, 합 16건의 사용을 찾아냈으므로, 두 플래그의 0건은 검색이 눈먼 것이 아니다.
2. `flags` 필드를 다루는 모든 곳(`modules/ai/src` 에서 `\bflags\b`): 송신은 `header.flags = HAS_BINARY_PAYLOAD` 또는 호출자 인자, 수신은 `HAS_BINARY_PAYLOAD` 비트만 검사한다. **알 수 없는 비트를 거절하는 곳은 없다.** 시험도 0x4·0x8 을 숫자로 쓰지 않는다.
3. 빌드: ci-ai 392 통과·스킵 5·실패 0, 스텁 316 통과·스킵 81·실패 0, 컴파일 경고 0(두 빌드). 변경 전과 시험 수가 같다(392 / 316).
4. 191 의 응답이 같은 정보를 이미 전달한다: 부위 인식 워커 응답의 `outcome`(ok / non_finite / out_of_range), `body_part`, `confidence`(`ai_worker_protocol.h` BODYPART 절, `WorkerBodyPartReply.*`), 저신뢰는 호스트가 문턱·`fallback_mode` 로 판단한다(`BodyPartWorkerPath.ALowConfidenceFromTheWorkerGetsTheDecisionOfThisProcess`).

## 제거인가 예약인가 — 판단과 근거

매크로는 **제거**하고 비트 값은 **예약**으로 남겼다.
- 제거 이유: 두 주석("caller should use fallback", "worker skips retries")이 프로토콜이 따르지 않는 동작을 약속하고 있었다. 실제로 SDD 가 그 이름을 API 반환값처럼 서술하는 데까지 번졌다.
- 값을 비워 두는 이유: 와이어 헤더의 `flags` 는 `uint32` 이고 헤더 주석이 와이어 형식의 유일한 정의다. 비트를 그냥 지우면 다음 사람이 0x4 를 다른 뜻으로 쓰고, 옛 문서를 읽은 상대와 어긋날 수 있다. 와이어 형식(헤더 크기 40바이트, 필드 위치)은 바뀌지 않았다.
- 한 가지 한계: 같은 종류의 쓰이지 않는 플래그가 하나 더 있다. `XPE_AI_FLAG_TIMEOUT`(0x2, "inference timed out; worker sends partial result")도 읽고 쓰는 곳이 0이다. 카드가 두 플래그만 지정해서 건드리지 않았다. 같은 방식으로 정리할지는 리더 판단이다.

## 문서 정정 초안 (리더가 191 병합 때 옮긴다)

`docs/project/sdd_ai.md` §4.3. M4d 초안(C1)의 의사코드를 쓰면 `LOW_CONFIDENCE flag` 줄이 사라진다. 그 아래 붙일 정리 문장은 M4d 초안의 문장을 이렇게 바꾼다:

- 현재(M4d 초안 C1): "`LOW_CONFIDENCE` 는 공개 API 의 필드가 아니다. 같은 이름의 워커 프로토콜 플래그(`XPE_AI_FLAG_LOW_CONFIDENCE`)는 헤더에 정의만 있고 어느 메시지도 쓰지 않는다. 저신뢰는 반환 코드·라벨·알림의 조합으로 표현된다."
- 바꿀 문장: "`LOW_CONFIDENCE` 는 공개 API 의 필드가 아니다. 설계 초안에 있던 같은 이름의 워커 프로토콜 플래그는 QA-B-192 에서 제거되었다(비트 0x4·0x8 은 예약, 쓰지 않음). 저신뢰는 반환 코드·라벨·알림의 조합으로 표현되고, 워커의 응답은 `outcome`·`body_part`·`confidence` 로 모델이 말한 것만 전달한다."
- 근거: `ai_worker_protocol.h` Message Flags 절의 예약 주석, 위 증거 1·2.

## 미검증 (Gaps)

- 저장소 밖(다른 저장소, 배포된 문서)의 인용은 보지 않았다.
- `git log -S` 로 두 플래그의 도입 경위(누가 어떤 설계로 넣었는지)는 조사하지 않았다. 주석 문구에서 "이전 설계의 흔적" 이라고 읽은 것은 리더 카드의 서술이다.
- doxygen 은 이 PC 에 없어서 CI 의 `doxygen-headers` 가 판정한다(헤더 주석을 바꿨다).

## 잔여 위험

- 외부 구현이 0x4·0x8 을 보내거나 기대한다면 알 수 없다. 저장소 안에서는 보내는 쪽도 받는 쪽도 없고, 수신 쪽이 알 수 없는 비트를 거절하지 않으므로 와이어에서는 달라지는 것이 없다.
