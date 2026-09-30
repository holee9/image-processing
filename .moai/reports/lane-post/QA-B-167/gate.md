# QA-B-167 (`#130`) 1단계 — 범위 확정. **둘 다 "없음" 이고, 크기는 작지 않습니다**

## 1. 주장

1. **워커는 추론을 하지 않습니다 — 추론 메시지 자체가 없습니다.** 워커의 메시지
   종류는 `INIT`/`HEARTBEAT`/`SHUTDOWN` 셋뿐이고, 프로토콜 헤더가 정의한 추론
   메시지 8종은 **워커가 다루지 않습니다**. §2
2. **더 나아가 워커는 프로토콜 헤더를 쓰지 않습니다.** 헤더를 `#include` 해 놓고
   **자기 익명 네임스페이스 안에 별도 `enum` 을 정의해 그것을 씁니다** — 두
   enum 의 번호가 서로 다릅니다. 지금 상태로는 **생명주기 메시지조차 호환되지
   않습니다**. §2.2
3. **IPC 브리지는 제품 코드에서 호출되지 않습니다.** 호출처는 브리지 자신과
   시험뿐 — `modules`·`clients`·`gui` 전체에서 **제품 호출 0건**. §2.3
4. **`AiIpcBridgeTest` 는 실경로를 단언하지 않습니다.** 스텁·풀 두 구성에서
   **결과가 같습니다**(10/10 통과, 빌드모드 분기 0건). 카드가 적은 "11건" 은
   실측 **10건**입니다. §3
5. **`SRS-ALERT-004` 의 정본은 SRS 입니다.** SDD 의 실패 문구는 같은 SDD 안에서도
   고립돼 있습니다 — 같은 블록과 같은 표가 실패를 `SRS-SAFE-008` 로 돌립니다. §4
6. **발신 주체는 모듈입니다.** `xpe_alert_push` 제품 호출 20건이 전부
   `modules/` 안이고, `clients/` 는 읽기와 시험뿐입니다. §4.2
7. **2단계는 작지 않습니다.** 프로토콜 정합 + 워커 추론 + DLL 쪽 호출 경로까지
   묶여 있어 구현 전에 리더 판단이 필요합니다. §5

---

## 2. `ai_worker` 는 오늘 무엇을 합니까

### 2.1 반환하는 것 — 생명주기 응답 3종뿐

`ai_worker_main.cpp` 의 `HandleMessage` 가 가진 갈래 전부:

| 받는 것 | 돌려주는 것 |
|---|---|
| `INIT` | `INIT_ACK` — 프로토콜 버전, 버전 문자열 **`"0.1.0-stub"` 고정**, `capabilities = 0` |
| `HEARTBEAT` | `HEARTBEAT_ACK` — `status = IDLE` 고정 |
| `SHUTDOWN` | `SHUTDOWN_ACK` — `exit_code = 0` |
| 그 밖 전부 | `ERROR_RESPONSE` — `"Unknown message type"` |

**영상을 받는 경로가 없습니다.** "고정값을 반환한다" 도 아니고 "입력을 그대로
돌려준다" 도 아닙니다 — **추론 요청을 받을 메시지 자체가 정의돼 있지 않아**
모든 추론 요청은 마지막 줄의 `ERROR_RESPONSE` 로 떨어집니다.

`STUB_VERSION_STRING = "0.1.0-stub"` 은 `#ifdef` 밖의 상수라 **풀 빌드에서도
`"0.1.0-stub"` 을 응답합니다.** 시작 로그만 `Mode: FULL` 로 갈립니다.

### 2.2 워커가 프로토콜 헤더를 쓰지 않습니다 (예상 못 한 발견)

워커는 `#include "xpe/ai/ai_worker_protocol.h"` 를 해 놓고, 바로 아래 익명
네임스페이스에서 **자기 `enum class MessageType` 을 새로 정의하고 그것만
씁니다.** 두 정의의 번호가 어긋납니다:

| 메시지 | 프로토콜 헤더 `XpeAiMessageType` | 워커 내부 `MessageType` |
|---|---|---|
| INIT | 1 | 1 |
| INIT 응답 | **2** (`INIT_RESPONSE`) | 2 (`INIT_ACK`) |
| SHUTDOWN | **3** | **5** |
| HEARTBEAT | **4** | **3** |
| HEARTBEAT_ACK | **5** | **4** |
| 추론 8종 (10~17) | 정의됨 | **없음** |
| 모델 관리 4종 (20~23) | 정의됨 | **없음** |

메시지 **본문 형식도 다릅니다** — 헤더는 32바이트 `XpeAiMessageHeader` + JSON
페이로드 규약인데, 워커는 `struct WorkerMessage` 를 통째로 `WriteFile` 합니다.

> 즉 워커와 브리지는 **오늘 서로 말이 통하지 않습니다.** 브리지가 헤더대로
> `HEARTBEAT`(4) 를 보내면 워커는 그것을 `HEARTBEAT_ACK`(4) 로 읽습니다.
> 2단계 크기 추정에 이것이 들어갑니다.

### 2.3 풀 빌드에서 IPC 추론이 일어납니까 — **일어날 수 없습니다**

브리지 함수(`xpe_ai_ipc_bridge_*`)와 타입(`XpeAiIpcBridge`)의 **전체 참조**:

```
modules/ai/src/ai_ipc_bridge.cpp    7   (구현 자신)
modules/ai/src/ai_ipc_bridge.h      2   (선언)
modules/ai/tests/test_ai_ipc_bridge.cpp  26   (시험)
modules · clients · gui 그 밖       0
```

**제품 코드에서 브리지를 부르는 곳이 0 입니다.** `xpe_bone_suppress` 는
`QA-B-161` 에서 **모듈 안(in-process) `OnnxSession`** 에 붙였고, IPC 를 거치지
않습니다. 즉 `REQ-AI-003`(워커 격리)은 **선이 끊긴 채** 있습니다.

브리지 자신의 머리말도 이미 그렇게 적고 있고(`ai_ipc_bridge.cpp:30` —
*"the bridge is never wired into the inference path"*), 시험 주석도
같은 말을 합니다(`test_ai_ipc_bridge.cpp:252-253`).

### 2.4 대조군 — 같은 방법이 실재하는 추론을 찾아냅니까

카드 §4 가 요구한 것입니다. 같은 `grep` 으로 `Ort::`·`OnnxSession` 을 셌습니다:

| 파일 | 건수 |
|---|---|
| `modules/ai/src/ai_onnx_session.cpp` | **33** |
| `modules/ai/src/ai.cpp` | **4** |
| `modules/ai/src/ai_worker_main.cpp` | **0** |
| `modules/ai/src/ai_ipc_bridge.cpp` | **0** |

**검색은 작동합니다** — DLL 쪽 추론을 찾아냅니다. 워커의 0 은 도구 부재가
아니라 관측입니다.

한 가지 더: 풀 빌드에서 `CMakeLists.txt:286` 이 **워커에 `onnxruntime` 을
링크합니다.** 링크는 있고 쓰는 코드가 없습니다 — 그래서 빌드는 통과하고,
"풀 빌드가 된다" 가 "추론이 된다" 로 읽힐 수 있는 자리입니다.

---

## 3. `AiIpcBridgeTest` 는 무엇을 단언합니까 — 그리고 스텁에서도 통과합니다

시험 **10건**(카드의 "11건" 은 실측과 다릅니다). 전부 **연결 실패·인자 검증·
수명**입니다:

| 단언하는 것 | 건수 |
|---|---|
| 생성/파괴 (null, 중복 호출) | 4 |
| 워커 없음 → 타임아웃/오류 | 3 |
| 헤더 매직·페이로드 크기 검증 | 2 |
| 응답 없음 → 타임아웃 | 1 |

**모델을 바꾸면 출력이 바뀌는 단언은 0건입니다.** `QA-B-162` 의
`RunningADifferentModelChangesTheOutput` 같은 종류가 IPC 경로에는 없습니다 —
있을 수 없습니다. 추론 요청을 보내는 시험이 하나도 없습니다.

### 반증 — 두 구성에서 돌렸습니다

```
===STUB_CFG=0===   ===STUB_BUILD=0===   ===STUB_IPC=0===
===FULL_CFG=0===   ===FULL_BUILD=0===   ===FULL_IPC=0===

STUB: [==========] 10 tests from 1 test suite ran. (8 ms total)
      [  PASSED  ] 10 tests.
FULL: [==========] 10 tests from 1 test suite ran. (0 ms total)
      [  PASSED  ] 10 tests.
```

소스에 `XPE_AI_STUB_BUILD`·`GTEST_SKIP` **0건** — 빌드모드 분기가 없습니다.
**두 구성의 결과가 같다는 것이 곧 이 시험들이 실경로를 단언하지 않는다는
증거입니다.** 종료 코드는 파이프 없이 받았습니다.

---

## 4. `SRS-ALERT-004`

### 4.1 정본은 **SRS** 입니다

전수 4건 — 전부 문서(`docs/` 3건 + `.moai/project/` 사본 1건), 코드 0건.

| 출처 | 말하는 것 |
|---|---|
| `XPE-SRS-001:102` | `DL processing 적용됨` / **Info** / `"AI-processed" label 표시` |
| `XPE-SDD-002:874` | `AI worker failure → return input unchanged + SRS-ALERT-004` |
| `api-spec.md:362` | `xpe_get_pending_alert` 의 `SRS:` 인용 (극성 주장 없음) |

**SDD 쪽이 고립돼 있습니다.** 같은 SDD 문서가 두 곳에서 다르게 말합니다:

```
XPE-SDD-002:868   7. Tag output as AI-processed (SRS-SAFE-008)   <- 성공 태깅은 SAFE-008
XPE-SDD-002:874   AI worker failure → ... + SRS-ALERT-004        <- 실패를 ALERT-004 로
XPE-SDD-002:882   | Worker crash | AI failure | Return input + alert | SRS-SAFE-008 / HAZ-008 |
                                                                  <- 실패는 다시 SAFE-008
```

즉 **SDD 자신의 본문과 표가 실패를 `SRS-SAFE-008` 로 돌리고**, 오직 의사코드
`Fallback:` 한 줄만 `ALERT-004` 를 씁니다. 그 한 줄이 어긋난 것입니다.

**세 번째 근거 — 심각도.** SRS 알림 표에서 실패 조건은 전부 Warning/Error
입니다(`001` Warning, `005` Error, `006` Error). `004` 만 **Info** 인데,
"worker failure" 가 Info 일 수는 없습니다. **심각도가 SRS 해석과만 맞습니다.**

> **처분**: `SRS-ALERT-004` 는 **성공 알림**입니다 — DL 처리가 적용됐을 때
> Info 로 `"AI-processed"` 를 알립니다. SDD:874 한 줄이 낡았습니다.
> **문서 정정은 리더 소유이므로 손대지 않았습니다.**

### 4.2 발신 주체 관례 — **모듈**

`xpe_alert_push` 제품 호출(시험·P/Invoke 선언 제외) 전수:

| 위치 | 건수 |
|---|---|
| `modules/preprocess/` | 12 |
| `modules/gsvg/` | 4 |
| `modules/enhance_advanced/` | 2 |
| `modules/enhance_basic/` | 1 |
| **`modules/ai/`** | **1** (`ai.cpp:272` — config 키 경고, WARNING) |
| `clients/` · `gui/` | **0** (읽기 `xpe_get_pending_alert` 와 시험뿐) |

**예외 없이 모듈이 냅니다.** GUI 는 큐를 읽어 표시만 합니다. Info 선례도
있습니다 — `xpe_calib_generate_gain.cpp:208,436` 이 `XPE_ALERT_INFO` 를 씁니다.

### 4.3 없는 것

- `modules/ai` 안에 **성공 경로 Info 알림 0건**. `xpe_bone_suppress` 가 성공해도
  아무것도 큐에 넣지 않습니다
- `"AI-processed"` 문자열 **코드 0건** — 태깅 수단 자체가 없습니다
- `SRS-SAFE-008`(실패 시 입력 보존 + 알림)도 `modules/ai` 코드에 **0건**

---

## 5. 2단계 크기 추정

```
ai_worker 실추론
  파일   4+   modules/ai/src/ai_worker_main.cpp   (프로토콜 교체 + 추론 핸들러)
              modules/ai/src/ai_ipc_bridge.cpp     (요청/응답 왕복)
              modules/ai/src/ai.cpp                (DLL 이 IPC 를 타게 할지 결정)
              modules/ai/tests/test_ai_ipc_bridge.cpp (실경로 단언 신규)
  성격   설계 결정 포함 — 작지 않음

SRS-ALERT-004
  파일   1~2  modules/ai/src/ai.cpp  (+ 시험 1)
  성격   기계적 — 관례가 명확하고 Info 선례도 있음
```

**둘의 성격이 다릅니다.** ALERT-004 는 바로 할 만합니다. 워커 실추론은
그렇지 않고, **구현 전에 리더 판단이 필요한 결정이 최소 둘** 있습니다:

1. **`xpe_bone_suppress` 를 IPC 로 옮길 것인가.** 지금은 `QA-B-161` 이 붙인
   in-process 경로로 동작하고 CI 초록(`ai-onnx 248/248`)입니다. IPC 로 옮기면
   **동작하는 경로를 바꾸는 것**이고, 놔두면 워커는 계속 아무도 안 부릅니다
2. **워커의 프로토콜을 헤더에 맞출 것인가.** §2.2 의 불일치는 추론 이전에
   생명주기부터 깨져 있어 **선행 작업**입니다. 이것만으로도 별도 카드 크기입니다

> **제 권고**: ALERT-004 를 먼저 이 카드에서 잇고, 워커는 위 두 결정 뒤에
> 별도로. 다만 **어느 쪽을 할지는 리더 판단**이고 저는 착수하지 않았습니다.

---

## 6. 검증

```
IPC 시험 스텁/풀:  ===STUB_IPC=0===  ===FULL_IPC=0===   각 10/10 통과
빌드:              ===STUB_BUILD=0===  ===FULL_BUILD=0===
대조군(추론 검색):  ai_onnx_session.cpp 33 · ai.cpp 4 · worker 0 · bridge 0
브리지 제품 호출:   0 (modules·clients·gui 전수)
ALERT-004 전수:     4건 전부 문서, 코드 0
xpe_alert_push:     제품 20건 전부 modules/, clients 0
구현 변경:          없음 (카드 §5)
```

종료 코드는 전부 파이프 없이 받았습니다. 스크립트: `_ipc.bat`.

## 7. 미검증 · 잔여 위험

- **워커를 실행해 보지 않았습니다.** §2.1 은 소스를 읽은 결과이고, 실제로
  추론 메시지를 보내 `ERROR_RESPONSE` 가 오는 것을 **관측하지는 않았습니다**
- **§2.2 의 불일치가 실제로 오작동을 내는지 재지 않았습니다.** 브리지를 부르는
  제품 코드가 없어서 **오늘은 아무 증상도 없습니다** — 이어 붙이는 순간
  드러날 것이라는 추론입니다
- **`SRS-ALERT-004` 정본 판정은 문서 세 곳의 내부 일관성에 근거합니다.**
  원 결정 기록(이슈·커밋)까지 거슬러 올라가지는 않았습니다
- **크기 추정의 "파일 4+" 는 제 읽기입니다.** 설계를 정하면 늘 수 있습니다
- 다른 추론 메시지 6종(`BODYPART`·`STITCH`·`DL_DENOISE`·모델 관리)은
  `xpe_bone_suppress` 와 같은 상태인지 **개별로 확인하지 않았습니다**

---

Refs #130
