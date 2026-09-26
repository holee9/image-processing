# QA Gate — ai

- module: ai
- build: build/ci-ai (Ninja, RelWithDebInfo, /WX=ON, **XPE_AI_STUB_BUILD=ON**)
- branch: dev/postprocess
- sha: e67c125
- measured: 2026-08-28

## 문서 드리프트 (선행 보고)

lead 지시는 "xpe_ai.dll 은 dev-plan 기준 미착수 상태 — 없으면 '미착수'로 기록만"이었다.
`dev-plan.md:50` 도 `xpe_ai.dll | Post-B | - | ❌ | 미착수 (Should)` 로 적혀 있다.

**실측 결과는 다르다.** 소스 2,178줄이 존재하고, stub 모드로 **빌드·링크·테스트가 모두 통과**한다.
따라서 "미착수"로 기록하지 않고 실측했다. dev-plan §1 표의 xpe_ai 행은 갱신 대상이다.

| # | Gate item | Result | Evidence |
|---|---|---|---|
| 1 | dumpbin /dependents 횡단 의존성 | PASS | 의존 = spdlog.dll + Windows CRT 뿐. xpe_* 횡단 의존 0건 |
| 2 | GTest 100% GREEN | PASS | 108/108 (`-R "Ai"`) |
| 3 | 메모리 누수 1000 프레임 | **FAIL** | 최대 반복이 10회. 1000 프레임 테스트 부재 |
| 4 | /WX 0 warning | PASS | XPE_WARNINGS_AS_ERRORS=ON, 빌드 warning 0건, exit=0 |
| 5 | P/Invoke ABI 심볼 수 일치 | **FAIL** | export 15개 vs 공개 헤더 선언 10개 (+5 불일치) |
| 6 | CODEOWNERS 경계 | PASS | `/modules/ai/ @holee9` (CODEOWNERS:13) |

## Evidence (verbatim)

### G1. 횡단 의존성 — PASS
```
$ dumpbin /dependents build\ci-ai\bin\xpe_ai.dll

  Image has the following dependencies:

    spdlog.dll
    KERNEL32.dll
    MSVCP140.dll
    VCRUNTIME140.dll
    VCRUNTIME140_1.dll
    api-ms-win-crt-*.dll  (7건)
```
`xpe_common.dll` 조차 링크되지 않는다 — REQ-AI-001 (Layer 1 의존)을 만족하며, 오히려 더 엄격하다.

### G2. GTest — PASS
```
$ ctest --test-dir build\ci-ai -N -R "Ai"
Total Tests: 108

$ ctest --test-dir build\ci-ai -R "Ai" --output-on-failure
100% tests passed, 0 tests failed out of 108
```
전체 빌드 트리 기준으로는 166/166 통과 (xpe_common 테스트 58건 포함). 로그: `ai/tests.log`

### G3. 메모리 누수 1000 프레임 — FAIL
```
$ grep -rn "InitShutdownCycleRepeated" -A6 tests/ai_tests/
tests/ai_tests/test_ai_abi.cpp:128:TEST(AiAbi, InitShutdownCycleRepeated) {
tests/ai_tests/test_ai_abi.cpp-129-    // Verify no resource leaks across multiple init/shutdown cycles
tests/ai_tests/test_ai_abi.cpp-130-    for (int i = 0; i < 10; ++i) {
```
반복 생명주기 테스트는 존재하나 **10회**다. 게이트 기준은 1000 프레임이므로 미달.
`tests/ai_tests/` 전체에서 1000회 루프는 발견되지 않았다 (grep 결과의 1000 은 전부 픽셀값·타임아웃).
이 카드는 실측 카드이므로 테스트를 새로 작성하지 않았다.

### G4. /WX 0 warning — PASS
```
$ cmake --build build\ci-ai
===BUILD_EXIT=0===
$ grep -ci "warning" ai/tests.log
0
```
`XPE_WARNINGS_AS_ERRORS=ON` 이므로 warning 이 있었다면 빌드가 실패했을 것이다. exit=0 이 근거다.

### G5. ABI 심볼 수 — FAIL (+5)
```
$ dumpbin /exports build\ci-ai\bin\xpe_ai.dll
          15 number of functions
          15 number of names

  xpe_ai_get_model_card      xpe_ai_init            xpe_ai_set_fallback_mode
  xpe_ai_shutdown            xpe_ai_version         xpe_bodypart_recognize
  xpe_bone_suppress          xpe_dl_denoise         xpe_stitch_estimate_size
  xpe_stitch_images
  xpe_ai_ipc_bridge_connect  xpe_ai_ipc_bridge_create   xpe_ai_ipc_bridge_destroy
  xpe_ai_ipc_bridge_receive  xpe_ai_ipc_bridge_send

$ grep -c "XPE_API" modules/ai/include/xpe/ai/ai_api.h
10
```
**불일치 5건**: `xpe_ai_ipc_bridge_{create,connect,send,receive,destroy}`.
이들은 `modules/ai/src/ai_ipc_bridge.h` (**src/ 내부 헤더**)에 선언돼 있고
공개 API 헤더 `include/xpe/ai/ai_api.h` 에는 없다. 즉 **내부 구현이 DLL 밖으로 노출**돼 있다.
P/Invoke 계약 문서와 실제 export 면이 어긋나며, 내부 IPC 프로토콜이 외부에서 호출 가능한 상태다.

### G6. CODEOWNERS — PASS
```
$ grep -n "modules/ai" CODEOWNERS
13:/modules/ai/               @holee9
```
이 카드에서 `modules/ai/` 는 읽기만 했고 수정하지 않았다.

## Gaps (미검증)

- **ONNX Runtime 실경로 미검증.** `XPE_AI_STUB_BUILD=ON` (기본값) 으로만 측정했다.
  `XPE_AI_USE_ONNXRUNTIME=ON` 경로는 ONNX Runtime 1.20+ 설치가 없어 configure 조차 시도하지 않았다.
  실제 추론 경로의 게이트 6항목은 전부 미측정이다.
- **worker 프로세스 격리(REQ-AI-003) 미검증.** `xpe_ai_worker.exe` 는 빌드되었으나
  named pipe IPC 의 실동작은 이번 측정 범위 밖이다.
- G5 의 "문서와 일치" 판정에서 비교 대상을 공개 헤더의 `XPE_API` 선언 수로 잡았다.
  별도의 P/Invoke 계약 문서(`docs/`)가 있다면 그 쪽 수치와도 대조해야 하나 이번에는 확인하지 못했다.

## Residual risk

- stub 빌드에서 통과한 108건은 **ABI 형태와 에러 처리**를 검증할 뿐, 추론 정확도는 검증하지 않는다.
  stub GREEN 을 "AI 기능 동작"으로 읽으면 안 된다.
- G5 의 5개 export 는 `__declspec(dllexport)` 가 내부 헤더에 남아 발생한 것으로 보이나,
  원인을 소스에서 끝까지 추적하지는 않았다. 수정은 별도 카드가 필요하다.
- dev-plan 이 이 모듈을 "미착수"로 표기하고 있어, 문서만 보는 판단자는 이 모듈의
  실제 노출면(15개 export)을 인지하지 못한다. 문서 갱신 전까지 이 격차 자체가 리스크다.
