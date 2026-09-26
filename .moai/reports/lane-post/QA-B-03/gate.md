# QA-B-03 — ai 내부 IPC 심볼 내부화 (#100)

- card: QA-B-03 / issue: #100
- module: ai
- branch: dev/postprocess
- 변경: modules/ai/CMakeLists.txt (+6), modules/ai/src/ai_ipc_bridge.cpp (±10),
        tests/ai_tests/test_ai_ipc_bridge.cpp (±10)

## 결과: PASS — G5 불일치 해소, 테스트 무회귀

| 지표 | 이전 (QA-B-01) | 이후 |
|---|---|---|
| xpe_ai.dll export | 15 | **10** |
| 공개 헤더 XPE_API 선언 | 10 | 10 |
| 격차 | **+5** | **0 (완전 일치)** |
| ctest (ai 전용) | 108/108 | **108/108** |
| ctest (트리 전체) | 166/166 | **166/166** |
| build | exit 0 | exit 0, warning 0건 |

내부화된 5개: `xpe_ai_ipc_bridge_{create,connect,send,receive,destroy}`

남은 export 10개 = 공개 헤더 `ai_api.h` 선언과 1:1 일치:
`xpe_ai_get_model_card`, `xpe_ai_init`, `xpe_ai_set_fallback_mode`, `xpe_ai_shutdown`,
`xpe_ai_version`, `xpe_bodypart_recognize`, `xpe_bone_suppress`, `xpe_dl_denoise`,
`xpe_stitch_estimate_size`, `xpe_stitch_images`

## 호출자 전수 조사 (수정 전 필수 절차)

lead 지시대로 제거 전에 전 저장소를 훑었다. 결과가 예상과 달랐다.

```
$ grep -rn "xpe_ai_ipc_bridge" --include=*.cpp --include=*.h --include=*.cs --include=*.py .
modules/ai/src/ai_ipc_bridge.cpp   : 정의 5건
tests/ai_tests/test_ai_ipc_bridge.cpp : 선언 5건 + 호출 20건
(그 외 0건)
```

**프로덕션 호출자가 0건이다.** `ai.cpp` 도 `ai_worker_main.cpp` 도 이 함수들을 부르지 않는다.
내부 헤더 `ai_ipc_bridge.h` 조차 함수를 선언하지 않는다 — 구조체 `XpeAiIpcBridge` 정의만 있다.

즉 #100 의 성격은 "내부 구현이 실수로 새어나갔다"가 아니라
**"오직 자기 테스트만이 소비하는 함수가, 그 테스트를 가능하게 하려고 export 되어 있었다"** 이다.

테스트는 `xpe_ai` DLL 을 링크하면서 함수를 **자체 재선언**(`XPE_API` 붙여서)해
DLL 경계 너머로 호출하고 있었다. 그래서 단순히 `XPE_API` 만 떼면 테스트 20개 호출부가 전부 깨진다.

## 수정 3건

### F1. 정의부 export 지정자 제거 — `modules/ai/src/ai_ipc_bridge.cpp`
5개 함수 정의에서 `XPE_API`(= `__declspec(dllexport)`) 제거. DLL 밖으로 나가지 않는다.
함수 본체·시그니처·동작은 무변경.

### F2. 테스트 재선언에서도 제거 — `tests/ai_tests/test_ai_ipc_bridge.cpp`
테스트의 forward declaration 5건에서 `XPE_API` 제거. 남겨두면
`__declspec(dllimport)` 로 해석되어 존재하지 않는 import 심볼을 찾는다.

### F3. 테스트가 구현을 직접 컴파일 — `modules/ai/CMakeLists.txt`
`xpe_ai_tests` 타깃에 `src/ai_ipc_bridge.cpp` 를 소스로 추가하고 include path 에 `src/` 추가.
DLL 경계를 넘지 않고 구현을 직접 링크하므로 export 없이도 테스트가 성립한다.

이것이 F1/F2 를 가능하게 하는 전제다. F3 없이 F1 만 하면 링크 실패한다.
내부 구현을 테스트할 때의 표준 패턴이며, 새 빌드 타깃을 만들지 않는 최소 변경이다.

## 게이트 재측정

| # | Gate item | 이전 | 이후 | 근거 |
|---|---|---|---|---|
| 1 | 횡단 의존성 | PASS | PASS | 변경 없음 (링크 구성 불변) |
| 2 | GTest 100% GREEN | PASS 108/108 | **PASS 108/108** | 무회귀 |
| 3 | 누수 1000 프레임 | FAIL (10회) | **FAIL (10회)** | 이 카드 범위 밖 — lead 지시대로 손대지 않음 (#105) |
| 4 | /WX 0 warning | PASS | PASS | build_test.log warning 0건 |
| 5 | ABI 심볼 수 일치 | **FAIL +5** | **PASS 0** | export 10 == 헤더 10 |
| 6 | CODEOWNERS 경계 | PASS(공허참) | PASS | 변경 3파일 전부 Lane B 소유 (`modules/ai/`, `tests/ai_tests/`) |

## Gaps (미검증)

- **G3 누수 미측정.** lead 지시("G3 는 이 카드에 넣지 말 것")를 따랐다. #105 소관.
- **ONNX 실경로 미검증.** stub 빌드(`XPE_AI_STUB_BUILD=ON`)에서만 측정했다.
  `XPE_AI_USE_ONNXRUNTIME=ON` 경로에서 export 면이 동일한지는 확인하지 않았다.
  ai_ipc_bridge.cpp 는 ONNX 조건부 컴파일 대상이 아니므로 동일할 것으로 보이나 관측하지 않았다.
- **worker 프로세스 IPC 실동작 미검증.** `xpe_ai_worker.exe` 는 빌드되나 named pipe
  왕복 실동작은 이번에도 측정하지 않았다. 테스트는 연결 실패 경로만 검증한다.
- **DLL 내 잔존 코드 미확인.** export 를 떼도 `ai_ipc_bridge.cpp` 는 여전히 xpe_ai.dll 에
  컴파일되어 들어간다. 호출자가 없으므로 링커가 제거했을 수 있으나 확인하지 않았다.

## Residual risk

- **프로덕션 호출자 0건이라는 사실 자체가 미해결 물음이다.** REQ-AI-003(worker 격리)은
  named pipe IPC 를 요구하는데, 그 구현을 아무도 부르지 않는다. 두 해석이 가능하다 —
  (a) ONNX 실경로에서만 쓰이도록 아직 배선되지 않았다, (b) 설계가 바뀌어 사장된 코드다.
  이 카드는 export 면만 다뤘고 이 물음에는 답하지 않았다. 별도 판단이 필요하다.
- 테스트가 구현을 직접 컴파일하므로 DLL 내부 사본과 테스트 사본이 별개 인스턴스다.
  전역 상태를 공유하는 함수였다면 문제가 되나, IPC bridge 는 핸들을 인자로 받는
  무상태 API 라 해당하지 않는다.
