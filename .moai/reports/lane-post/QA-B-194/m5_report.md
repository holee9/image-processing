# QA-B-194 M5 — 할당 실패 스윕 (`xpe_ai_oom_tests`, REQ-AI-090)

카드: QA-B-194 M5 · 관련: #130 · 설계: `design.md` §5 M5
장난감 모델·스텁 기준이다. 이 스윕이 증명하는 것은 "모듈 **자신의** 할당이 실패했을 때의 처리"이고, 아래 "증명하지 못한 것"이 그 경계다.

## 주장 (Claim)

1. 새 실행 파일 `xpe_ai_oom_tests` 는 모듈 소스(`ai.cpp`·`ai_ipc_bridge.cpp`·`ai_worker_supervisor.cpp`·`ai_onnx_session.cpp`)를 DLL 과 같은 정의로 직접 컴파일하고 `operator new` 를 교체해, 한 호출의 **K 번째 할당만** 실패시키며 K = 1, 2, … 를 호출이 더 적게 할당할 때까지 돌린다(`xpe_preprocess_oom_tests`·`xpe_common_oom_tests` 와 같은 방법). 모든 K 가 네 질문을 받는다: 예외가 C ABI 를 넘었나 / 오류로 끝났다면 모듈이 호출 전 그대로이고 새는 것이 없나(살아 있는 할당 블록 수가 호출과 정리 뒤 호출 전과 같은가) / 코드가 `XPE_ERR_OUT_OF_MEMORY` 인가 / 이후에도 쓸 수 있나.
2. 스윕 대상과 결과(ci-ai, 매 K 마다 위 네 질문 통과):
   - `xpe_ai_init`, 설정 없음: 할당 지점 10곳 — `OUT_OF_MEMORY` 9, 로그 줄 안에서 삼켜진 실패 1(성공)
   - `xpe_ai_init`, 설정 `{}`: 15곳 — `OUT_OF_MEMORY` 14, 삼켜진 1
   - `xpe_ai_init` 뒤 같은 인자의 실패 없는 init: 15곳, 모든 K 에서 두 번째 init 이 성공(실패한 init 이 상태를 오염시키지 않음)
   - `xpe_ai_get_model_card`(로드된 모델): 2곳, (로드되지 않은 모델): 2곳
   - `xpe_bone_suppress`(프로세스 안, 세션을 처음 만드는 호출): 26곳 — `OUT_OF_MEMORY` 25, 삼켜진 1
   - `xpe_bodypart_recognize`(프로세스 안, 모델이 이미 올라간 호출): 16곳
3. **스윕이 찾은 결함 하나를 고쳤다**: `OnnxSession::Create` 의 `catch (const std::exception&)` 가 `bad_alloc` 까지 "세션 생성 실패(`kSessionCreationFailed`)"로 바꿨고, `xpe_bone_suppress` 는 그것을 `XPE_ERR_PROCESSING_FAILED` 로 돌려줬다(K=13~19, 7곳). 메모리 부족이 "추론 실패" 로 읽혔다. `bad_alloc` 을 먼저 잡아 새 코드 `OnnxErrorCode::kOutOfMemory` 로 구분하고 `ai.cpp` 가 `XPE_ERR_OUT_OF_MEMORY` 로 매핑한다. 고친 뒤 25곳 모두 `OUT_OF_MEMORY`.
4. **M4 의 롤백이 실행으로 증명되었다**: `xpe_ai_init` 의 실패 K 마다 모듈은 초기화되지 않은 채이고(`xpe_ai_worker_state` 가 NOT_INITIALIZED) 살아 있는 할당이 0 이다(반증 S3 이 그 누수 검출을 확인).

## 발견 — 스윕으로 닿을 수 없는 곳 (결정이 필요하다)

**nlohmann::json 3.11.3 은 비어 있지 않은 JSON 문서를 파괴할 때 할당을 한다.** `~basic_json() noexcept` → `json_value::destroy` 가 자식을 `std::vector` 로 옮기려고 `stack.reserve(...)` 를 부르는데, 이 소멸자는 `noexcept` 라서 그 할당이 실패하면 **`std::terminate`** 다. 우리 코드는 이 지점에서 예외를 잡을 수 없다. 첫 실행이 정확히 이렇게 끝났다: 키가 있는 설정을 받은 `xpe_ai_init` 의 25번째 할당에서 프로세스가 종료됨. 스택(임시 `std::set_terminate` + DbgHelp 로 캡처, 커밋된 시험에는 없다): `operator new ← std::vector<json>::reserve ← json_value::destroy(json.hpp:580) ← ~basic_json ← parseConfig` (`m5_nlohmann_terminate_stack.txt`).

모듈에서 JSON 문서(DOM)를 쓰는 곳은 세 곳이다: `parseConfig`(설정), `ai_bodypart_model.h` 의 라벨 파일 읽기(처음 부위 인식을 부를 때), `ai_onnx_session.cpp` 의 모델 메타데이터 파일 읽기. 세 곳 모두 메모리가 바로 그 순간 바닥나면 호출자 프로세스가 끝난다. 실제 확률은 극히 낮다(수십 바이트 할당이 실패하는 상황이면 프로세스가 이미 위험하다). 그래도 "호출이 `OUT_OF_MEMORY` 를 돌려준다" 는 약속은 이 세 곳에서 지켜지지 않는다.

그래서 스윕은 이 지점을 경계로 갈라 짰다(시험 파일 머리글에도 적었다): 설정 없음·빈 객체 `{}`(빈 DOM 의 소멸자는 할당하지 않는다), 라벨이 이미 올라간 부위 인식. **설정에 키가 있는 init 과 부위 인식의 첫 호출은 어떤 스윕으로도 증명되지 않았다.** 헤더(`ai_api.h`)에 "OUT OF MEMORY" 절을 넣어 같은 내용을 계약으로 적었다.

고치는 길은 있다 — DOM 대신 `nlohmann::json::sax_parse` 로 읽으면 소멸자 할당이 없다. 다만 세 곳을 다 바꿔야 약속이 완결되고 `parseConfig` 의 알림 순서(지금은 키 사전순) 등을 보존해야 해서 이 카드의 범위를 넘는다고 보고 **고치지 않았다**. 리더가 정할 일: (a) 문서화된 한계로 둔다(제 권고 — 위험이 낮고 비용이 크다), (b) 별도 카드로 SAX 전환.

## 증거 (Evidence)

- ci-ai: `xpe_ai_tests` `[  PASSED  ] 431 tests.`(M4 와 동일, 스킵 5), `xpe_ai_oom_tests` `[  PASSED  ] 8 tests.`. 컴파일 경고 0. 스텁(ci-post): `xpe_ai_tests` `[  PASSED  ] 353 tests.`(스킵 83), `xpe_ai_oom_tests` `[  PASSED  ] 6 tests.` + `[  SKIPPED  ] 2`(뼈 억제·부위 인식은 모델이 필요해 스텁에서는 건너뜀, 사유가 시험에 적혀 있다). 경고 0.
- 로컬 doxygen 1.12.0(CI 순서, `WARN_AS_ERROR`): `exit=0`, `: (warning|error)` 줄 0. `check_header_docs.py` 0 findings. 린트 0 error.
- 시험 8건(`test_ai_oom_injection.cpp`): 하네스 대조군(첫 할당을 실패시키면 실제로 모듈 코드에서 발화), 위 스윕 7개. 각 스윕은 호출 전에 실패 없는 한 바퀴를 먼저 돌려(로거·ONNX Runtime 환경처럼 한 번 만들어져 남는 것) 기준선을 만든다.
- 반증 7개(`m5_arms_out.txt`), 매번 빌드 성공, 원본 바이트 동일 복원, 대조군 8/8:
  - S1 init 의 `bad_alloc` 매핑을 틀린 코드로 → init 스윕 둘 빨강
  - S2 init 의 try/catch 제거 → init 스윕 둘 + "init 뒤 다시 init" 빨강(예외가 새어 나옴을 검출)
  - S3 init 이 상태를 마지막 단계까지 소유하지 않음(M4 이전의 누수 모양) → 같은 셋 빨강(누수 검출이 작동함)
  - S4 모델 카드의 매핑 오류 → 모델 카드 스윕 둘 빨강
  - S5 부위 인식의 매핑 오류 → 부위 인식 스윕만 빨강
  - S6 세션 생성이 메모리 부족을 세션 실패로 보고(발견한 결함 되돌림) → 뼈 억제 스윕만 빨강
  - S7 `ai.cpp` 가 `kOutOfMemory` 를 매핑하지 않음 → 뼈 억제 스윕만 빨강

## 기준 (Baseline)

같은 트리 `dev/postprocess`(M4 646ab547 + 3be3eb80 위), `cmake --build --preset ci-ai` 와 `ci-post`, 이 실행의 출력. 고치기 전 뼈 억제 스윕: `rc -3 x7; rc -2 x18; rc 0 x2` (`build/g194m5-5.txt`), 고친 뒤 `rc -2 x25; rc 0 x2`.

## 바뀌는 반환 코드와 호출자

- `xpe_bone_suppress`: 프로세스 안 경로에서 세션 생성 중 메모리 부족이 `XPE_ERR_PROCESSING_FAILED`(-3) → `XPE_ERR_OUT_OF_MEMORY`(-2). 실제로는 메모리 부족일 때만 달라진다. GUI 가 이 코드를 어떻게 읽는지는 확인하지 않았다(두 코드 모두 오류).
- `OnnxErrorCode::kOutOfMemory = 6` 추가(`ai_onnx_session.h`, 모듈 내부 C++ 클래스의 열거형). 워커(`ai_worker_main.cpp`)와 `ai_bodypart_model.h` 의 `switch` 는 `default` 가 있어 새 값은 기존의 "그 밖" 가지로 간다 — 이 두 곳의 동작은 바뀌지 않았다(부위 인식의 처음 호출과 워커의 메모리 부족은 여전히 "모델을 쓸 수 없음"/일반 오류로 보고된다. 스윕으로 닿지 못하는 영역이라 확인하지 못했다).

## 미검증 (Gaps) — 증명하지 못한 것

- **위 "발견"의 세 곳**: 키가 있는 설정, 부위 인식 라벨 파일, 모델 메타데이터 파일에서의 할당 실패. 프로세스 종료로 이어질 수 있다.
- **부위 인식의 첫 호출(cold)**: 라벨 파일(JSON)을 읽기 때문에 스윕하지 못했다. 이 경로에서 `bad_alloc` 이 어떤 코드·알림("모델을 쓸 수 없음" 같은 거짓 보고)으로 나오는지 모른다.
- **ONNX Runtime 자신의 할당과 `xpe_common`(알림 큐) 의 할당**은 실패시키지 않았다(각자의 런타임 할당기를 쓴다). ORT 안의 세션 생성 실패는 이 방법으로 증명되지 않는다 — 그쪽은 기존의 "ORT 예외를 오류 코드로 매핑" 시험의 일이다.
- **워커 경로**(`use_worker`)와 워커 프로세스 자체의 할당 실패는 스윕하지 않았다(프로세스 밖).
- 스윕은 Debug 빌드(ci-ai)에서 돌렸다. K 의 개수는 로그·표준 라이브러리 구현에 따라 달라질 수 있어, 시험은 개수가 아니라 "모든 K 가 네 질문을 통과" 를 단언한다.
- 삼켜진 실패(결과 `rc 0`)는 로그 줄 안의 할당이 실패해 줄만 사라진 경우다(로그 매크로가 의도적으로 삼킨다). 이 경우의 성공 결과가 올바른지는 시험이 확인한다(뼈 억제는 픽셀, 부위 인식은 라벨).
- 살아 있는 할당 블록 수는 이 실행 파일의 `operator new` 만 센다.
- "init 뒤 다시 init" 시험의 엄격한 단언(실패한 K 마다 다음 init 이 성공: `tried == worked`)은 반증을 거치지 않았다. 반증 7개는 이 단언을 강화하기 전에 돌렸고(강화는 단언을 더 엄격하게만 만든다), 이 단언만 빨갛게 만드는 변이(실패한 init 이 반쯤 초기화된 상태를 남김)는 만들지 못했다. 현재 이 시험이 반증으로 확인된 것은 S2·S3 에서 빨개진다는 점뿐이다.

## 잔여 위험

- 위 "발견"의 종료 가능성(극히 낮은 확률, 높은 영향).
- 모듈 밖에서 `kOutOfMemory` 를 새로 받는 코드는 없다(내부 열거형). 그러나 `OnnxSession` 을 쓰는 새 호출 지점이 생기면 이 값을 매핑해야 한다 — 매핑하지 않으면 `default` 가지의 "일반 실패"가 된다.
