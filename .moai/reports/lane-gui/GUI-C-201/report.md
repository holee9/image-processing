# GUI-C-201 — GUI-C-200 의 선택 개선 두 가지 (#130)

증거: `falsification_arms.txt`, `after_fix.txt`(이 폴더).

## 1. 서명 거부(-4) 갈래 분리

- `AiBoneSuppressionStage`: 상수 `ConfigInvalid = -4`, 새 분류 `AiCallClass.ModelUnavailable`, `Classify(-4)` = ModelUnavailable(전에는 Failed). `Interpret` 의 새 갈래 문구:
  "AI bone suppression NOT applied (code -4): the model in the model directory cannot be used by the module (the file is damaged or is not a model, or its signature did not verify; the alert list names the cause when the module raised one); the original image is shown. Whether this call counts toward the AI worker's failure total is the module's decision; the AI worker mark shows when it has switched off."
- **의도적으로 "세지 않는다"고도 "센다"고도 쓰지 않았다.** post 의 코드를 읽으면 같은 -4 가 원인에 따라 다르다: 읽을 수 없는 모델(`kModelLoadFailed`)은 호스트가 센다, 서명 거부(195, 아직 main 에 없음)는 세지 않는다. 한쪽을 단정하면 다른 원인에서 거짓이 된다. 그래서 "연속 실패가 워커를 끈다"는 일반 문장만 이 갈래에서 뺐고, 판단은 모듈에 맡긴다고 적었다.
- 접두사 `AI bone suppression NOT applied (code -4)` 는 그대로라 C09 의 정규식(`(NOT applied|not attempted) \(code -?\d+`)과 C08 의 `no model at`/`was not found` 부재 단언에 걸리지 않는다. 다른 코드(-3·-9 등)의 문구는 변하지 않는다(시험이 고정).
- 시험(`AiBoneSuppressionStageTests`): 분류 표의 -4 를 `ModelUnavailable` 로, 새 시험 1건(문구 — 접두사·"cannot be used by the module"·signature·원본 표시·**"consecutive failures switch" 와 "not counted" 둘 다 없음**·C08/C09 가 보는 문구 부재·다른 코드는 일반 문구 유지).
- "195 가 main 에 없으니 지금 -4 를 돌려주는 기존 경로로 시험" — 그 경로(가짜 모델 파일)를 새로 빌드한 현재 main 소스 모듈로 C09 가 계속 통과(아래 결과)함으로 확인했다. 서명 거부 경로 자체는 아직 실행하지 못했다.

## 2. init 철자 유지

- `AiSessionTracker.DirectoryToSend(requested)`: 같은 디렉터리(대소문자 무시, `NeedsNewSession` 과 같은 비교)의 세션이 실행 중이면 **그 세션이 시작된 철자**를, 아니면 요청 철자를 돌려준다.
- `GuiAiSession.InitCore`: 다른 디렉터리 때문에 세션을 끝낸 **뒤**, `xpe_ai_init` 을 부르기 **전에** `directory = Tracker.DirectoryToSend(directory)`. 성공 시 트래커는 보낸 철자를 기억한다(`InitSucceeded(directory)`). 같은 세션이면 init 은 여전히 불린다(멱등) — 다만 모듈이 보는 문자열이 첫 호출과 바이트 단위로 같으므로 D8 의 "다른 디렉터리" 경고가 생기지 않는다. 호출을 아예 생략하는 쪽(카드의 대안)은 택하지 않았다: 호출 생략은 `xpe_ai_worker_state` 사전 확인·진단 문자열 등 InitCore 의 다른 부수 동작을 건너뛰게 한다.
- 시험(기능, 새 2건): 트래커의 철자 규칙(미시작=요청 그대로 / 실행 중 같은 디렉터리 = 첫 철자, 대소문자 3가지 / 다른 디렉터리 = 요청 / `Stopped` 뒤 = 요청), 소스 고정(철자 선택이 세션 종료 뒤·모듈 호출 앞, 트래커가 보낸 철자를 기억).
- 시험(E2E **C10**, Native): 임시 디렉터리에 가짜 `bone_suppress.onnx` 를 두고 AI 를 실행 → 모델 디렉터리 입력을 **대문자로만** 바꾸어 다시 실행 → 앱이 게시하는 init 진단(`init#N dir='…'`)에서 두 번째 init 이 **실제로 불렸고**(카운터 증가 — 같음이 공허하지 않게) 같은 철자를 보냈음을 확인. 진단을 읽는 방식인 이유: 모듈의 D8 경고 자체는 194 M4 이후 빌드에만 있고(main 에 아직 없음) 이 시험은 그 경고가 아니라 **앱이 보내는 값**을 단정한다. 로컬 실측: `init#2 dir='…xpe-ai-spell-…'` 두 번 모두 첫 철자.

## 결과 (`after_fix.txt`)

- 전체 IntegrationTests 어셈블리 **640 통과 / 0 실패 / 1 건너뜀**. E2E Mock(자동화 보고서+체인+AI 메뉴) **25 / 0 / 11 건너뜀**. 쉘 E2E·SelfCheck(16) 통과.
- E2E Native — **현재 main 소스로 이 기계에서 새로 빌드한 `xpe_ai.dll`**(`xpe_ai_worker_state` 수출, 워커 exe 포함)과 조립 폴더: C08·**C09**·C10·AI 메뉴 시험 **7 통과 / 0 실패**. (이전 카드들의 로컬 `xpe_ai.dll` 은 `xpe_ai_init`/`worker_state` 가 없는 옛 빌드라 C09 를 돌리지 못했는데, 이번에 처음 C09 가 로컬에서 통과했다.)
- 반증(`falsification_arms.txt`): ① -4 를 일반 실패로 되돌림 → 분류 표 + 문구 시험 2건 빨강 ② 철자 선택 줄 제거 → 소스 고정 + **E2E C10** 빨강 ③ 트래커가 항상 요청 철자 반환 → 규칙 시험 + **C10** 빨강. 전부 바이트 동일 복원.

## 미검증·한계

1. **서명 거부 -4 경로는 실행하지 못했다**(195 가 main 에 없음). 새 문구가 그 경로에서 어떻게 보이는지는 코드로만 안다. 지금 -4 를 돌려주는 경로(가짜 모델 파일)에서의 텍스트는 C09 가 실행으로 본다.
2. C10 은 앱이 **보내는 철자**를 보며, 모듈의 D8 경고가 실제로 사라졌는지는 보지 못했다(D8 이 들어간 빌드가 없음). 194 M4 가 main 에 들어오면 같은 시나리오에서 알림 목록에 "called again with a different model directory" 가 없는지 한 줄 단언을 더할 수 있다.
3. 로컬 모듈은 이 기계에서 빌드한 스텁 AI 빌드다(ONNX 없음). C10 이 보는 것(init 호출 문자열)은 빌드와 무관하다.
4. post 가 알려 준 "워커 exe 가 없는 디렉터리 → -9 가 1,2,3 으로 세어져 꺼짐" 관측은 이 카드 범위 밖이라 쓰지 않았다(C09 이전 카드 몫).
5. 새 빌드(`build/c201-ai`, 조립 폴더 `$TEMP/c201_native`)는 `.gitignore`/임시 위치에 있으며 커밋하지 않았다.
