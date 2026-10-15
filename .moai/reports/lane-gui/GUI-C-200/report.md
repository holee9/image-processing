# GUI-C-200 — post 의 AI 변경이 gui 에 닿는 지점 대조 (코드 읽기, 변경 없음)

범위: main(e8e14166)의 `modules/ai` 와 post 작업 트리(`xpe-post`: 로컬 커밋 QA-B-194 M3~M5·QA-B-195 M1~M3, 아직 main 에 없음)의 `ai.cpp`·`ai_onnx_session.cpp`·`ai_worker_main.cpp` 를, gui 가 부르는 AI 수출 4개(`xpe_ai_init`·`xpe_ai_shutdown`·`xpe_ai_worker_state`·`xpe_bone_suppress`)와 대조했다.

**방법의 한계(먼저)**: 전부 **코드 읽기**다. 194 M3~M5·195 를 담은 `xpe_ai.dll` 이 이 기계에 없어(가진 것은 그 전 빌드) 새 동작을 실행으로 관측하지 못했다. 아래 "확인함"은 "소스를 읽어 확인함"이다. post 작업 트리의 코드는 병합 전이라 바뀔 수 있다.

## 1. 반환 코드

gui 의 코드표(`AiBoneSuppressionStage`): `Ok 0`, `InvalidInput -1`, `ProcessingFailed -3`, `NotInitialized -6`, `UnsupportedFormat -7`. `Classify`: 0 → 성공, {-1, -6, -7} → NotAttempted("시도하지 않음", 코드별 뜻 문구), 그 밖 전부 → Failed. 시험 `AiBoneSuppressionStageTests` 의 `InlineData(-1, "NotAttempted")` 가 -1 의 분류를 고정한다.

| 변화 | gui 에서 | 판단 |
|---|---|---|
| `xpe_bone_suppress` 비유한 입력 -3 → -1 (194 M1, main 에 있음) | 입력이 비유한일 수 **없다**: 모듈에 가는 float 버퍼는 `ToFloat(ushort[])` = `p / 65535f` 하나뿐이고(`GuiAiRunner.cs:280`, `xpe_bone_suppress` 호출처는 `GuiAiRunner.cs:314` 한 곳) 입력은 항상 `ushort` 체인 출력이다. 유한 정수/65535 는 유한. 모델 **출력**의 비유한은 모듈이 그대로 -3 이고, 코드 0 인데 비유한이면 gui 의 `TryToUInt16` 가 거절한다. 설령 -1 이 오더라도 `Classify(-1)` = NotAttempted, 문구는 "the input or its size was rejected." | **영향 없음** (post 판단 확인) |
| 입구 크기 검사: 64MB 상한(4096×4096×4 B), 폭·높이 ≥ 1 (194 M1) | gui 의 AI 입력은 float32 이므로 4096² 가 정확히 상한. 그보다 큰 프레임은 -1 → NotAttempted "the input or its size was rejected." (이 경우 알림은 없음: 알림은 비유한에만 붙는다). 지금 gui 의 프레임은 이 상한 아래(번들 3072²) | **영향 없음** (문구도 맞게 읽힘) |
| `xpe_ai_init("")` → INVALID_INPUT (194 M4) | gui 는 빈 문자열을 **보낼 수 없다**: `NormalizeDirectory` 가 빈 값·공백을 기본 `data/models` 로 바꾼 뒤 절대 경로로 만든다(`AiBoneSuppressionStage.cs:595-605`). 만에 하나 오더라도 `InterpretInit` 이 "xpe_ai_init refused the configuration (code -1)" | **영향 없음** |
| 설정 경고(194 M4: 잘못된 JSON·객체 아님·모르는 키·형식 틀린 키·범위 밖 timeout_ms) | gui 가 보내는 설정은 `{"use_worker":true}` 한 개뿐(`AiConfig`, 객체를 직렬화). 모듈은 `use_worker` + boolean 을 "소비한 키"로 세므로 경고가 나지 않는다 | **영향 없음** |
| `xpe_ai_init` 두 번째 호출이 **다른** 디렉터리·설정 문자열이면 경고 알림 1건(194 M4 D8, 호출은 OK·무시) | gui 는 AI 실행 때마다 같은 정규화 문자열로 `xpe_ai_init` 을 부른다(멱등). 문자열이 같으면 알림 없음. **예외**: 사용자가 모델 디렉터리 설정에서 **대소문자만** 바꾸면, gui 의 `NeedsNewSession` 은 대소문자를 무시하고 "같은 세션"으로 보아 종료·재시작 없이 새 철자로 `xpe_ai_init` 을 부르고, 모듈은 바이트 비교라 "다른 디렉터리" 경고를 **그 실행마다** 올린다(모듈은 첫 철자를 계속 쓰므로 동작은 같다) | **문구만(알림 한 건이 반복)**, 드문 경로. gui 수정: 한 줄 범위(세션이 시작된 철자를 기억해 그걸 넘기거나, 철자가 다르면 재시작) — 선택 |
| 195 서명 거부 → `xpe_bone_suppress` 가 -4(CONFIG_INVALID) | gui 코드표에 -4 는 없고 `Classify(-4)` = **Failed** → 상태줄/체인 이유: "AI bone suppression NOT applied (code -4): the original image is shown. A failed call returns the input unchanged; consecutive failures switch the AI worker off for this session (the AI worker mark shows when that has happened). The code is the module's and is not read further here…". 라벨 "AI-processed" 는 붙지 않는다 | **문구만**, 아래 "서명 거부" 참조 |

## 2. 새 알림 문구

gui 가 알림을 거르는 곳(`grep` 대상: `Severity ==`, `StartsWith(`, `Contains(` 로 알림 본문·심각도를 읽는 코드):

- `NativeAlertDrain` — 심각도를 0/1/2 → INFO/WARN/ERROR 로 바꾸고(알 수 없는 값은 ERROR), 본문은 `AlertDisplayFormatter.FormatMessage` 로 보낸다.
- `AlertDisplayFormatter` — **접두사 `alert queue overflow:` 하나만** 특별 취급(셈 문장으로 바꿈). 그 밖 본문은 **그대로 통과**.
- 긴 본문: 초기 버퍼 256 자, `-8`(버퍼 부족)이면 8192 자로 한 번 재시도(`NativeAlertDrain.RetryBufferLength`, 시험 `BufferTooSmall_RetriesOnceWithALargerBuffer`). 새 문구는 320 자 안팎이라 이 재시도로 읽힌다. 공용 모듈은 본문을 자르지 않는다(`enqueue_alert` 는 문자열을 그대로 저장).
- 이 저장소의 `clients/`·`gui/` 에는 위 새 문구(`AI worker failed`, `XPE_WARN_*_INPUT_NOT_FINITE`, `failed signature verification`, `ai config …`)를 문자열로 맞추는 코드·시험이 **없다**(grep 0건). AI 워커 마크·배너는 알림이 아니라 `xpe_ai_worker_state` 의 상태에서 만든다.

| 알림 | gui 에서 | 판단 |
|---|---|---|
| 워커 중단 알림(191 M4e): "AI worker failed (code %d, failure N of M) during <cause> and is disabled for this session: body-part recognition returns UNKNOWN, bone suppression returns the input image unchanged …" (WARN) | 그대로 알림 목록에 WARN 으로 표시. **문구가 gui 가 쓰지 않는 기능을 말한다**: gui 는 `xpe_bodypart_recognize` 등 다른 AI 수출을 부르지 않는다(grep 0건) | **문구만 달라짐**(사용자가 "body-part recognition" 을 보게 됨). gui 수정 불필요 |
| 입구 거부(`XPE_WARN_BONE_SUPPRESS_INPUT_NOT_FINITE: N pixel(s) of the input frame are NaN or infinite …`, ERROR) | gui 입력으로는 **발생하지 않는다**(1 항). 발생하면 ERROR 로 목록에 그대로 | **영향 없음** |
| 모델 서명 실패(195, ERROR, 세션·역할당 1회): "AI bone suppression is unavailable: its model failed signature verification (<reason>) and nothing was loaded (REQ-AI-007, REQ-AI-091)" | ERROR 알림으로 그대로 표시. 아무 접두사·심각도 거름이 없다 | **영향 없음**(표시됨) |
| 설정 경고, `xpe_ai_init` 두 번째 호출 경고(194 M4) | 위 1 의 표 | 영향 없음 / 대소문자 예외 |

## 3. 운영 빌드가 실모델을 거부하는 상태(#243: 신뢰 목록이 비어 모든 모델이 거부됨)

post 코드 읽기(`ai_onnx_session.cpp` 서명 검사 → `kModelNotTrusted`, 워커 `ai_worker_main.cpp`, 호스트 `ai.cpp` 의 D6)로 따라간 경로:

1. 사용자가 AI 메뉴를 누름(DLL 이 보이면 켜져 있다 — GUI-C-198).
2. gui 가 모델 **파일 존재**만 확인(`CheckModelFile`) — 파일이 있으니 통과. 서명은 gui 가 보지 않는다.
3. `xpe_ai_init` OK(서명은 첫 추론에서 검사). `xpe_bone_suppress`(워커 경로) → 워커가 서명 실패를 **-4 + `model_unavailable`** 로 답한다. 호스트는 출력=입력, **연속 실패 횟수를 0 으로 되돌리고**(워커 고장으로 세지 않음), ERROR 알림을 세션당 한 번 올리고 -4 를 돌려준다.
4. 사용자가 화면에서 보는 것:
   - 체인/상태줄: `ai_bone_suppress=RequestedNotApplied` + 위 1 표의 Failed 문구("NOT applied (code -4) … consecutive failures switch the AI worker off …"). **원본 영상**이 표시되고 "AI-processed" 라벨 없음.
   - 알림 목록: 서명 실패 ERROR 알림 1건(첫 실행에서만).
   - **워커 마크·재시작 버튼은 나타나지 않는다**: 실패 횟수가 매번 0 으로 돌아가므로 워커 상태가 Disabled 가 되지 않는다. 사용자에게 보이는 원인 설명은 알림 목록의 그 한 줄뿐이다.
5. 문제: gui 의 Failed 문구는 "연속 실패가 AI 워커를 끈다"고 말하는데 이 거부는 그 횟수에 **세지 않는다**(모듈 사양). 그리고 -4 는 "모델은 있는데 모듈이 쓰지 않는다"(깨진 파일이든 서명 실패든)로 gui 가 따로 풀어 주지 않는다 — 코드의 이유(`CONFIG_INVALID` = 파일은 있으나 적재 불가/신뢰 불가)는 모듈 헤더에 있고 gui 는 "코드는 모듈의 것이며 더 읽지 않는다"로 둔다(GUI-C-185 의 의도적 결정: 같은 코드에 원인이 여럿).

**C09(E2E)에 미칠 영향 — 이 대조에서 나온 가장 구체적인 위험**: C09 는 `bone_suppress.onnx` 라는 이름의 **가짜 파일("not a model")** 로 워커를 계속 실패시켜 6 번 안에 "워커 꺼짐 마크"가 뜨는 것을 확인한다. 195 이후 그런 파일은 서명이 없어 **서명 거부 → 실패로 세지 않음 → 마크가 영영 안 뜬다** (워커의 `kModelNotTrusted` 분기, 호스트의 D6 리셋). 195 가 main 에 병합되면 CI 의 `gui-e2e-native` 에서 C09 가 빨강이 될 가능성이 높다. **실행으로는 확인하지 못했다**(195 DLL 없음, 그리고 CI 의 `xpe_ai.dll` 이 스텁 빌드인지 ONNX 빌드인지에 따라 분기가 달라진다 — 스텁이어도 서명 검사는 빌드와 무관하게 먼저 돈다는 post 의 주석이 있으나 이를 시험으로 본 것은 아니다).

## 4. 결론 표

| 항목 | 분류 | gui 수정 필요 시 범위 |
|---|---|---|
| 비유한 입력 -3 → -1 | 영향 없음 | — |
| 입구 크기 상한·폭높이 검사 | 영향 없음 | — |
| `xpe_ai_init("")` 거부 | 영향 없음 | — |
| 설정 경고(194 M4) | 영향 없음 | — |
| init 두 번째 호출 다른-설정 경고(D8) | 문구만(대소문자만 바꾼 디렉터리에서 실행마다 1건) | 선택, 한 줄 범위: 시작된 철자를 넘기거나 철자가 다르면 재시작 (`GuiAiRunner.InitCore`/`NeedsNewSession`) |
| 서명 거부 -4 의 상태줄 문구 | 문구만 달라짐(실제로는 "실패 문구"가 약간 잘못 안내) | 선택, 한 줄 범위: `Classify`/`Interpret` 에 -4 를 "모델은 있으나 모듈이 쓰지 않음(깨졌거나 서명 검증 실패 — 알림 목록 참조)" 갈래로 추가하고 "연속 실패가 워커를 끈다" 문장을 이 갈래에서 뺌 (`AiBoneSuppressionStage.cs`) |
| 워커 중단 알림 문구 | 문구만(gui 가 쓰지 않는 body-part 를 언급) | 없음 |
| 입구 거부·서명 실패·설정 경고 알림 표시 | 영향 없음(그대로 표시, 거르는 코드 없음) | — |
| #243 상태(모든 모델 거부) 화면 | 문구만: 원본 표시 + Failed 문구 + 알림 1건, 워커 마크 없음 | 위 -4 행과 같음 |
| **C09(E2E)** | **시험 수정 필요(195 병합 시)** | 시험 쪽: 가짜 파일로 실패시키는 방식은 서명 거부로 바뀌어 실패 횟수에 안 센다. 195 가 서명한 **추론이 실패하는 시험용 모델**(post 의 `tests/data` 자산 중 하나)이나 워커가 실제로 세는 실패를 만드는 방법이 필요 → **post 와 합의 필요** |

## 미검증

1. 새 모듈 동작은 소스로만 읽었다(194 M3~M5·195 를 담은 DLL 로 실행하지 않음). post 코드는 병합 전이라 달라질 수 있다.
2. D8(대소문자 예외)과 서명 거부 경로의 화면은 gui 코드와 post 코드를 이어 읽은 추론이다. 실제 화면은 보지 못했다.
3. C09 가 실제로 빨개지는지, CI 의 `xpe_ai.dll` 빌드 종류(스텁/ONNX)는 확인하지 못했다.
4. 이 저장소에 없는 클라이언트(다른 레인·문서의 알림 문구 매칭)는 보지 않았다. 범위는 `gui/`·`clients/`.
