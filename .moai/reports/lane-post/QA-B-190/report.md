# QA-B-190 — #210: REQ-AI-061 · REQ-AI-092 현재 상태 (보고서 먼저, 코드·SPEC 변경 없음)

카드: QA-B-190 · 관련: #210 · 브랜치 `dev/postprocess` · 이슈 본문은 `QA-B-154`(2026-09-27) 시점의 상태라 코드부터 다시 읽었다.

## 결론 먼저

1. **REQ-AI-092(시간 예산 → fallback + 알림)는 이슈가 쓰인 뒤 일부 구현됐다.** `xpe_bone_suppress` 의 **opt-in 워커 경로**(`use_worker: true`)에서만 동작하고, 시험이 실제로 있다(이슈의 "시험 0건"은 낡았다). 기본 경로(워커 끔)와 나머지 세 진입점은 시간 예산이 없다. 상태: **일부 구현 + 일부 미완**.
2. **REQ-AI-061은 "코드가 틀린 것"이 아니라 이슈가 두 요구를 한 줄로 섞은 것이다.** `confidence_threshold` 와 `confidence = 0.0` 덮어쓰기는 **REQ-AI-012 / REQ-AI-002**(저신뢰 이벤트 → 결정론적 fallback)의 자리이고, REQ-AI-061 은 **REQ-AI-060(AI 조리개 검출)** 의 fallback 인데 AI 조리개 검출 함수가 `modules/ai` 에 아예 없다. 상태: **061 = 둘 다 미완(AI 쪽이 없다)**, **012 = 요구 맞음·코드 미완**.
3. **"REQ-AI-009 는 개명의 흔적"이라는 이슈·문서 정정 메모의 설명은 이력으로 뒷받침되지 않는다.** 009 는 정의된 적이 한 번도 없다(§2.2).
4. **그 사이 낡아진 곳이 여럿이다**: RTM·VVP 의 "092 Not implemented", `ai_ipc_bridge.cpp` 머리 주석의 "예산이 배포 빌드에서 돌지 않는다·알림 없음", SPEC 구현 상태 절의 "추론 경로가 존재하지 않습니다". 목록은 §5.

## 1. REQ-AI-061 (그리고 REQ-AI-012)

### 1.1 요구 원문

- `REQ-AI-061` (`.moai/specs/SPEC-XPE-P3-AI/spec.md`, 정의 줄 `**REQ-AI-061** (Ubiquitous):`): "AI confidence below threshold shall trigger fallback to Hough deterministic path." — 절 `4.7 AI Collimation Detection — POST-07 AI variant`, 바로 위 `REQ-AI-060`: "AI collimation detection shall augment POST-07 baseline Hough-based detection."
- `REQ-AI-012` (같은 파일, Event-driven): "When AI inference confidence is below threshold (configurable, default 0.6), the system shall emit a low-confidence event and fall back to deterministic path."
- `REQ-AI-002`: "All AI inference shall have a deterministic fallback path … when AI is disabled, fails, or confidence is below threshold."

### 1.2 코드 (이름으로 인용; 줄은 `grep -n` 으로 재확인한 현재 값)

| 질문 | 답 | 근거 |
|---|---|---|
| `confidence_threshold` 저장 위치 | `AiModuleState::confidenceThreshold`(필드), `xpe_ai_init` 의 설정 파싱이 대입 | `ai.cpp` 93, 273 줄 (이슈의 `:206` 은 낡음) |
| 읽는 곳 | **0곳.** `modules/ai/src` 에서 `confidenceThreshold` 는 선언 1줄과 대입 1줄뿐이다(나머지 `confidence_threshold` 는 허용 키 목록 2줄) | `grep -n "confidenceThreshold\|confidence_threshold" modules/ai/src modules/ai/include` |
| confidence 를 0.0 으로 덮는 줄 | **지금도 있다.** `xpe_bodypart_recognize` 안의 `if (confidenceOut) *confidenceOut = 0.0f;` (이슈의 `:418` 은 낡음, 현재 719 줄) | 같은 함수는 라벨 `"UNKNOWN"` 을 쓰고 `XPE_ERR_PROCESSING_FAILED` 를 반환한다. 추론 호출이 함수 안에 없다 |
| ONNX 경로에서 문턱 비교가 실행되는가 | **아니오** | `xpe_bodypart_recognize` 는 `XPE_AI_USE_ONNXRUNTIME` 로 갈라지지 않는다 — 두 빌드가 같은 코드다. ONNX 추론이 있는 진입점은 `xpe_bone_suppress` 뿐이고 그것은 confidence 를 만들지 않는다 |
| 스텁 경로에서 | **아니오**(같은 이유) | |
| 임시 측정 | `ci-ai`(ONNX ON) 의 `xpe_ai.dll` 에 `confidence_threshold` 0.0 / 0.6 / 1.0 과 `fallback_mode:false` 를 주고 `xpe_bodypart_recognize` 호출 → 네 번 모두 rc `-3`(PROCESSING_FAILED), 라벨 `UNKNOWN`, confidence `0.0` | `bodypart_probe_source.txt` → `bodypart_probe_out.txt`. 이 빌드가 스텁이 아니라는 대조: 같은 빌드에서 `XPE_AI_EXPECT_ONNX=1` 로 돈 시험 107개 중 스킵은 "스텁 빌드에서만 도는" 1개뿐(`budget_tests_run.txt`) |
| `fallback_mode` | 같은 모양 — `fallbackMode` 는 저장만 되고 소비하는 곳이 없다(주석 속 의사 코드뿐). `REQ-AI-FB-002`(Fallback Mode Toggle) 쪽의 같은 결함 | `ai.cpp` 의 `fallbackMode` 는 4줄 — 선언 1, 대입 2(`xpe_ai_init` 파싱, `xpe_ai_set_fallback_mode`), 주석 속 의사 코드 1, 읽기 0 |
| Hough fallback 이 존재하는가 | **`modules/ai` 안에는 없다. 저장소에는 있다** — POST-07 기준선 `xpe_detect_collimation` 이 Hough 로 구현돼 있다(`modules/enhance_advanced/src/collimation_detect.cpp`, "Hough pipeline with confidence-based fallback") | 검색 범위: `modules/` 의 `*.cpp *.h`, 단어 경계(`-w`). **함정**: 대소문자 무시 부분 문자열로 `hough` 를 찾으면 `although` 가 `modules/ai` 테스트 주석 3곳에 걸린다. 단어 경계로 찾으면 `modules/ai` 0건 |
| AI 조리개 검출이 있는가 | **없다.** `ai_api.h`·`ai.cpp` 에 collimation 이 0건(`modules/ai/include`, `modules/ai/src` 대소문자 무시 검색) | 그래서 REQ-AI-060·061 은 호출할 대상이 없다 |

### 1.3 `ConfidenceThreshold*` 시험은 동작을 보는가

**상수만 본다.** `test_ai_fallback.cpp` 의 `AiFallbackTest.ConfidenceThresholdDefaultIs06` 은 `XPE_AI_DEFAULT_CONFIDENCE_THRESHOLD == 0.6f` 를, `ConfidenceThresholdInRange` 는 같은 상수가 0..1 안에 있음을 단언한다. `confidence_threshold` 를 설정으로 주는 시험은 `test_config_warning.cpp` 의 `AiConfigWarning.FullyValidConfigIsSilent` 하나이고 "경고가 없다"만 본다. 문턱이 동작에 닿는 시험은 없다(닿는 코드가 없으므로 쓸 수도 없다).

## 2. REQ-AI-092 vs REQ-AI-009

### 2.1 정의 원문

- `REQ-AI-092` — SPEC(`spec.md`, `**REQ-AI-092** (Ubiquitous):`) "AI inference shall enforce time budget (configurable, default 5s); exceeding budget triggers fallback and alert." SRS(`docs/project/srs_ai.md`, `#### REQ-AI-092: Time Budget Enforcement`, SRS ID `SRS-AI-SEC-002`) "AI inference shall enforce a configurable time budget (default 5 s). Exceeding the budget shall trigger fallback and alert." 2026-10-01 의 "Interpretation" 인용문이 동작을 정한다(입력 그대로 반환 + 비정상 코드, 실패마다 Warning 1건, 연속 3회 실패 시 워커 중지).
- `REQ-AI-009` — **정의 없음.** 정의 줄 패턴(`^\*\*REQ-AI-NNN\*\* (kind):` / `^#### REQ-AI-NNN`)이 SPEC·SRS 어디에서도 0건. 대조군: 같은 패턴이 092 를 찾는다.

### 2.2 왜 둘이 생겼는가 — 이력 (`git log`)

- `git log -G'^\*\*REQ-AI-009\*\*|^#### REQ-AI-009' -- .moai/specs docs/project/srs_ai.md` → **0 커밋**. 같은 `-G` 가 092 는 `9ec1b7bc`(2026-04-17), `dd7c8e05`(2026-04-28) 를 찾는다(대조군).
- 저장소에서 `REQ-AI-009` 문자열이 처음 나타나는 커밋: `git log --reverse -S'REQ-AI-009'` → `dd7c8e05`(2026-04-28, 제목 "fix(post): enhance_basic MSVC warning cleanup (#53)") 이다. 그 커밋의 diff 에 RTM 행 `| REQ-AI-009 | -- | Time budget enforcement | Not implemented |` 와 `ai_ipc_bridge.cpp`·`.h`·`ai_worker_protocol.h` 의 `REQ-AI-009: Time budget enforcement (inference timeout).` 가 **한꺼번에** 들어 있다.
- 해석: **개명이 아니다.** 009 는 한 번도 정의된 적이 없고, 대량 커밋에서 문서 한 곳과 코드 세 곳에 같은 오번호로 태어났다. 이슈 본문의 "하나가 개명됐다면"과 `rtm_ai.md`·`vvp_ai.md` 정정 메모의 "개명이 코드에서만 반영되고 문서에서 끊긴 형태"는 **이력이 지지하지 않는 설명**이다(정정 메모의 사실 부분 — 정의가 없다, 번호를 정정했다 — 은 맞다). `rtm_ai.md`·`vvp_ai.md` 메모는 `spec.md:272` 를 인용하는데 현재 092 정의는 `spec.md` 의 291 줄 부근이다(줄 번호가 밀렸다 — 이름으로 인용하는 것이 안전하다).
- 이 이력은 `-S`/`-G` 가 저장소 전체 이력을 한 번 본 결과다. 그 이전 브랜치나 지워진 이력은 보지 않았다.

### 2.3 `REQ-AI-` 인용 전수 대조

스크립트 `citations_source.txt` → `citations_out.txt`. 범위(명시): git 추적 파일 중 `docs/help/generated`(doxygen 산출물), `.moai/reports`, `.moai/lanes` 를 뺀 1980개 텍스트 파일. 정의는 SPEC 의 정의 줄 **46개**(`check_req_citations.py` 의 정의 패턴과 같은 수; 대조군으로 SPEC 정의 46 과 일치).

- 인용 **343건 / 35개 파일 / 48개 ID**. 정의 없는 ID 는 2개: `REQ-AI-009`(§2.2), `REQ-AI-013`.
- **`REQ-AI-009` 는 지금 인용이 아니라 언급이다**: `spec.md` 4곳, `rtm_ai.md` 2곳, `vvp_ai.md` 2곳이 전부 정정 메모 속 산문이다. 코드와 시험에는 리터럴이 0건(QA-B-157 이 일부러 그렇게 썼다).
- **`REQ-AI-013`**(SPEC `spec.md` 도입부의 "조건부 Must 승격" 목록 `REQ-AI-013 (Data Lineage) — GMLP Principle #3`)은 정의가 없다. 데이터 계보는 `REQ-AI-024`("data lineage recorded per REQ-REG-013")가 다룬다 — `013` 은 `REG-013` 과 헷갈린 오번호일 가능성이 크지만 확정은 아니다(검증 안 함).
- **살아 있는 ID 를 틀린 뜻으로 인용한 곳**(이 검사가 못 보는 형태 — 줄마다 한 줄 문맥을 정의 원문과 대조했다):

| 자리 | 인용이 말하는 것 | 정의가 말하는 것 | 판정 |
|---|---|---|---|
| `spec.md` 도입부 "조건부 Must 승격" 목록의 `REQ-AI-010~012 (Model Card)` | 012 도 모델 카드 | 010·011 이 모델 카드, **012 는 저신뢰 이벤트** | 뜻 불일치(SPEC 산문) |
| `rtm_ai.md` §11 Thread Safety 표의 `REQ-AI-001` 행(동시 `bodypart_recognize` 4스레드), `vvp_ai.md` 의 "REQ-AI-001 (thread safety)" | 001 이 스레드 안전 | 001 = "Layer 1 dependency"(xpe_common 만 의존). 스레드 안전 조항이 AI 요구 목록에 없다 | 뜻 불일치 — 요구가 빠졌거나 행이 잘못 추적된 것. 리더 판단 필요 |
| `ai.cpp` 의 `// REQ-AI-020: Self-supervised denoising (N2N, N2S, N2V, Noise2Sim)`(`xpe_dl_denoise` 안) | 전략에 N2V | 020 은 SSL 노이즈 제거 일반, 전략 목록은 **021**: Noise2Noise, Noise2Self, **Neighbor2Neighbor**, Noise2Sim — N2V(Noise2Void) 는 목록에 없다 | 인용 번호는 맞고 괄호 속 내용이 SPEC 과 다름(주석) |
| `ai_ipc_bridge.cpp`·`.h`·`ai_worker_protocol.h` 머리 주석의 `REQ-AI-092 … Only part of REQ-AI-092 is implemented`, `budget does not run in a shipped build`, `alert NO` | 092 는 일부, 예산은 배포 빌드에서 안 돈다, 알림 없음 | §3 의 현재 코드와 다름 | **낡은 주석**(QA-B-157 이 쓴 시점에는 맞았고 QA-B-171 이 뒤에서 바꿨다 — 가드가 감싸던 것보다 오래 산 주석) |
| `test_ai_fallback.cpp` 머리 `REQ-AI-012: Low-confidence event triggers fallback.` | 이 파일이 012 를 시험한다 | 이 파일의 `ConfidenceThreshold*` 는 상수만 본다(§1.3) | 인용은 맞고 시험이 요구를 덮는다는 인상이 과하다 |

- 나머지 인용은 한 줄 문맥으로 정의와 맞았다: 001·002·003·004·005·006·008·010·011(코드·시험), 050·051, 093, 020·022(번호), GUI `GuiAiRunner.cs` 의 003. 한계: 줄마다 **한 줄 문맥**과 정의를 대조한 것이지, 인용 주변 코드가 그 요구를 실제로 구현하는지까지 읽은 것은 아니다.
- 정의되었으나 SPEC 밖에서 한 번도 인용되지 않는 ID 21개(021·023·024·031~033·041·042·052·061·062·071~073·081~083·091·101·111·112)는 미구현 요구와 대체로 겹친다. `061` 도 여기에 있다 — 코드가 061 을 인용한 적이 없다.

## 3. 시간 예산 fallback + 알림은 실제로 동작하는가

**동작한다 — opt-in 워커 경로의 `xpe_bone_suppress` 에서, ONNX 빌드에서.** 이미 시험이 있어 임시 측정이 필요 없었다. 이번에 ci-ai 를 다시 빌드해 돌렸다(`build\g190-run.bat`: `===BUILD=0===`, `===RUN=0===`, `XPE_AI_EXPECT_ONNX=1`, `budget_tests_run.txt`: `[  PASSED  ] 107 tests.`, 스킵 1개는 스텁 전용 시험).

| 확인할 것 | 시험 (이름은 `grep` 으로 대조) | 이 실행의 관측 |
|---|---|---|
| 침묵하는 워커 → 예산 후 반환, 출력은 입력 그대로, 비정상 코드 | `WorkerPathFixture.ASilentWorkerIsReportedTheInputIsReturnedAndTheNextCallRecovers` | 통과 (2546 ms). 2 s 예산으로 `took >= 1800 ms`, 출력 == 입력, `rc != OK` 를 단언 |
| 알림이 **정확히 1건**, Warning, 문구가 `REQ-AI-002`·`REQ-AI-092` 를 인용 | 같은 시험: `CountAlerts("REQ-AI-002") == 1`, `CountAlerts("REQ-AI-092") == 1`, 심각도 `XPE_ALERT_WARNING`, `AI-processed` 라벨 0건 | 통과 |
| 예산은 상수가 아니라 설정값 | `IpcDeadline.TheBudgetIsTheConfiguredValueNotAConstant` | 통과 |
| 멈춘 워커에서 출력을 건드리지 않음 | `IpcDeadline.BoneSuppressWithAStalledWorkerFailsAfterTheBudgetAndLeavesTheOutputAlone`, `IpcDeadline.AStalledWorkerLeavesAnInPlaceBufferByteForByteIntact` | 통과 (425 ms, 426 ms) |
| 워커를 죽이고 다음 호출은 새 워커 | `WorkerSupervisor.AStalledWorkerFailsThatCallIsKilledAndTheNextCallStartsAFreshOne` | 통과 (2340 ms) |
| 대조군(응답하는 워커는 예산 안에 답한다) | `IpcDeadline.ControlAWorkerThatAnswersIsAnsweredWithinTheBudget` | 통과 |
| 이 시험들이 CI 에서 실제로 도는가 | `.github/workflows/ci.yml` 의 `ai-onnx` 잡: `cmake --preset ci-ai`, `ctest --test-dir build/ci-ai` 에 `XPE_AI_EXPECT_ONNX: '1'` | 워크플로를 읽었다. 실제 CI 로그에서 이 시험 출력을 관측하지는 않았다(§6) |

**범위의 한계 — 요구가 말하는 "AI inference" 전체가 아니다:**

- **기본 경로(`use_worker` 끔, 기본값)** 의 `xpe_bone_suppress` 는 `OnnxSession::Run` 을 프로세스 안에서 부르고, `ai_onnx_session.cpp`·`.h` 에 timeout/budget/deadline 단어가 0건이다. 이 경로에는 시간 예산이 없다.
- `xpe_bodypart_recognize`·`xpe_stitch_images`·`xpe_dl_denoise` 는 추론이 없는 스텁이라 예산을 적용할 대상이 없다.
- `use_worker` 의 기본값은 `false` 다(`AiModuleState::useWorker{false}`, GUI 의 `GuiAiRunner` 는 `UseWorker` 를 설정으로 보낸다). 즉 092 는 **opt-in 조건에서만** 만족된다. REQ-AI-005(opt-in) 와는 별개의 opt-in 이다.

## 4. 결론 표

분류: **A** 요구 맞음·코드 틀림 / **B** 요구 틀림·코드 맞음 / **C** 둘 다 미완 / **D** 스텁이라 정상 / **E** 구현됨(범위 한정)

| 항목 | 분류 | 한 줄 근거 |
|---|---|---|
| REQ-AI-061 (신뢰도 미만 → Hough) | **C** | AI 조리개 검출(060)이 없다. Hough 기준선은 `enhance_advanced` 에 있다 |
| REQ-AI-012 (저신뢰 이벤트 + fallback) | **A**(요구는 맞고 코드가 미완) / 스텁 단계의 `bodypart` 는 **D** | `confidenceThreshold` 읽기 0곳, 알림 호출 0건. 스텁이 항상 실패를 반환하므로 안전하지만 요구는 도달하지 못한다 |
| REQ-AI-FB-002 / `fallback_mode` | **C** | 저장만 되고 소비 0곳 |
| REQ-AI-092 (시간 예산 + fallback + 알림) | **E**(워커 경로의 `xpe_bone_suppress`) + **C**(기본 경로·나머지 진입점) | §3 |
| REQ-AI-009 인용 | 해소 | 정의 없음 → 092 로 정정 완료(QA-B-157, 문서는 리더). 이력은 개명 아님 |
| REQ-AI-013 인용(SPEC 산문) | **오번호 의심** | 정의 없음, `024`/`REG-013` 과 혼동 추정 |
| REQ-AI-006 EP 목록 하드코딩(이슈 3번째 항목) | 해소 | `GetAvailableExecutionProviders` 가 `Ort::GetAvailableProviders()` 를 묻도록 QA-B-160 이 고쳤다. 단 세션에는 CPU EP 만 등록한다(`ai_onnx_session.cpp` 주석) — "선택 가능"은 아직 CPU 한정 |
| RTM `REQ-AI-001` 스레드 안전 행 | **뜻 불일치** | 요구 누락 또는 오추적 |

### 고칠 것 (범위 한 줄씩, 모두 제안 — 대기)

1. **주석 3곳**(`modules/ai`, post 소유): `ai_ipc_bridge.cpp` 머리 주석 `REQ-AI-092` 블록, `ai_ipc_bridge.h`·`ai_worker_protocol.h` 의 "Only part … implemented" — 현재 사실(워커 경로 구현, 기본 경로 미구현)로 한 단락씩.
2. **`ai.cpp` `xpe_dl_denoise` 주석 한 줄**: 전략 목록의 `N2V` 를 SPEC 021 과 맞추거나 목록 자체를 삭제.
3. **`REQ-AI-012` 시험 인용 한 줄**: `test_ai_fallback.cpp` 머리 주석에 "ConfidenceThreshold* 는 상수만 본다"를 적는다.
4. **새 코드는 제안하지 않는다.** `confidence_threshold` 를 소비하는 코드는 `xpe_bodypart_recognize` 의 실제 추론(#130 의 T-006)이 생길 때 같이 와야 한다.

### SPEC·문서 문안 (리더 몫; 문안만)

- `spec.md` 구현 상태 절의 세 행 표 중 `REQ-AI-061` 행: "**061 은 AI 조리개 검출(REQ-AI-060)의 fallback 이다. 060 의 AI 변형이 없어 061 은 도달 대상이 없다. `confidence_threshold`·`confidence = 0` 은 REQ-AI-012/002 의 자리이며 같은 이유(`xpe_bodypart_recognize` 가 추론 없는 스텁)로 도달하지 못한다.**" 줄 번호(`ai.cpp:206`, `:418`)는 이름으로 바꾼다.
- 같은 표 `REQ-AI-092` 행: "**일부 구현(QA-B-171·181): `xpe_bone_suppress` 의 opt-in 워커 경로에서 예산 초과 → 입력 반환 + 비정상 코드 + Warning 1건. 기본 경로(워커 끔)와 나머지 진입점은 예산 없음.** 시험: `IpcDeadline.*`, `WorkerPathFixture.ASilentWorker…`, `WorkerSupervisor.AStalledWorker…`."
- 같은 절의 "추론 경로가 **존재하지 않습니다**" 문장은 낡았다 — "`xpe_bone_suppress` 만 추론 경로가 있다(`OnnxSession::Run`, 워커 경로 포함). `xpe_bodypart_recognize`·`xpe_stitch_images`·`xpe_dl_denoise` 는 스텁이다"로. 절 끝의 지시("표의 수치를 다시 재지 않고 이 절을 지우지 마십시오")에 따라 이번 보고서 §1.2·§3 의 재측정이 그 재측정이다.
- `spec.md` 도입부의 `REQ-AI-010~012 (Model Card)` → `REQ-AI-010~011 (Model Card)`, `REQ-AI-013 (Data Lineage)` → 정의된 ID(`REQ-AI-024`)로 또는 삭제.
- `rtm_ai.md` §12 `REQ-AI-092 | -- | Time budget enforcement | Not implemented` → "Partial (xpe_bone_suppress worker path)", `vvp_ai.md` 의 "Time-budget enforcement (REQ-AI-092) … Not implemented"(그리고 G-AI-7, 체크리스트 7)도 같은 취지로. `rtm_ai.md` 의 `REQ-AI-006`("Stub mode: no ONNX Runtime")와 `vvp_ai.md` 의 "`XPE_AI_USE_ONNXRUNTIME` is `OFF` in every preset"(`ci-ai` 프리셋은 ON)도 낡았다.
- `rtm_ai.md`·`vvp_ai.md` 의 정정 메모 "**개명이 코드에서만 반영되고 문서에서 끊긴 형태**"는 이력과 맞지 않으므로 "정의된 적이 없는 번호가 한 대량 커밋에서 코드와 문서에 함께 들어갔다"로.

## 5. 낡은 곳 모아 보기

| 자리 | 현재 문구 | 현재 사실 |
|---|---|---|
| `rtm_ai.md` §12 (092 행), 본문 "4 are deferred (… REQ-AI-092 …)" | Not implemented | 일부 구현 |
| `vvp_ai.md` Not-covered 표, §performance, G-AI-7, 체크리스트 7 | "Time-budget enforcement Not implemented; no latency measurement exists" | 예산 시험이 있다. 지연 시간을 재는 `WorkerSupervisor.MeasureColdStartAgainstTheDefaultBudget` 시험이 있다(이번에는 돌리지 않았다) |
| `vvp_ai.md` "ONNX … OFF in every preset" | | `ci-ai` 는 ON |
| `ai_ipc_bridge.cpp` / `.h` / `ai_worker_protocol.h` 머리 주석 | 예산이 배포 빌드에서 안 돈다, bridge 호출 0곳, 알림 없음 | `ai_worker_supervisor.cpp` 가 `xpe_ai_ipc_bridge_create` 를 부르고 `ai.cpp` 가 알림을 낸다 |
| SPEC 구현 상태 절의 "추론 경로가 존재하지 않습니다" | | `xpe_bone_suppress` 에 있다 |
| `tasks.md` `T-013 Time budget enforcement … pending`, `T-008 Wire xpe_bone_suppress … pending` | pending | 일부 완료 |

## 6. 증거 (Evidence) 와 기준 (Baseline)

- 이슈: `gh issue view 210 --comments` 전부(본문 + 코멘트 2개: QA-B-156 의 세 번째 항목, QA-B-157). 로컬 `build/g190-issue.txt`(커밋 안 함).
- 인용 전수: `citations_source.txt` → `citations_out.txt`(343건).
- 시험 재실행: `build\g190-run.bat` → `budget_tests_run.txt`(ci-ai, 같은 트리: main 병합 후 `dev/postprocess`).
- 임시 측정: `bodypart_probe_source.txt` → `bodypart_probe_out.txt`(커밋하지 않는 일회성, 증거로만 보관).
- 이력: `git log -G/-S` 명령은 §2.2 에 그대로 적었다.

## 7. 미검증 (Gaps)

- **CI 에서 `ai-onnx` 잡이 실제로 이 시험들을 돌렸는지** 실제 CI 로그로 관측하지 않았다(워크플로 파일만 읽음). 메모리의 "프리셋 이름은 CI 잡이 아니다" 교훈에 따라 별도 확인이 필요하다.
- 인용 대조는 줄마다 한 줄 문맥을 정의와 맞춘 것이다. 인용 주변의 코드가 그 요구를 구현하는지는 읽지 않았다.
- `REQ-AI-013` 이 `REG-013`·`024` 의 오번호라는 것은 추정이다.
- `rtm_ai.md` §11 의 `REQ-AI-001` 이 스레드 안전 요구의 누락인지 오추적인지는 가르지 않았다(SRS 에 스레드 안전 조항이 없다는 사실만 확인했다).
- 이슈 본문의 "시험 0건" 이 쓰인 시점(2026-09-27)에 참이었는지는 보지 않았다 — 지금은 거짓이다.
- 기본 경로(워커 끔)에서 `OnnxSession::Run` 이 오래 걸릴 때의 동작은 측정하지 않았다. 예산이 없다는 것은 코드에서 `timeout/budget/deadline` 단어가 없다는 사실에 근거한다.
- `modules/ai` 이외의 모듈이 `xpe_ai.dll` 을 부르는 경로(GUI 의 `GuiAiRunner`)에서 092 가 기대대로 보이는지는 보지 않았다.

## 8. 잔여 위험 (Residual risk)

- 092 가 opt-in 워커 경로에서만 만족된다는 사실이 SPEC 문구에 안 적히면 "092 구현됨"으로 읽혀 기본 경로의 무예산이 가려진다.
- `confidence_threshold`·`fallback_mode` 설정이 받아들여지고 동작하지 않는다 — 사용자가 설정하고 효과를 기대할 수 있다. 설정 키 검증(허용 키로 통과)이 이를 막지 않는다.
