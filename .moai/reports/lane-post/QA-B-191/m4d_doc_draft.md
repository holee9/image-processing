# QA-B-191 M4d — 문서 초안 (리더가 옮긴다)

카드: QA-B-191 M4d · 관련: #130 · 문서는 리더 소유이므로 이 보고서는 **초안**이다. 각 위치마다 현재 문장, 바꿀 문장, 근거(코드 이름·시험 이름)를 적었다.
근거로 든 시험 이름은 `modules/ai/tests/` 에서 `TEST` 정의를 열거해 대조한 것이다(§9 대조 방법). 시험에 쓴 모델은 전부 장난감 모델이며, 아래 어떤 문장도 실제 모델의 정확도·지연·적재 시간을 주장하지 않는다.

## 0. 먼저 결정할 것 (초안이 의존하는 판단)

| # | 판단 | 이유 |
|---|---|---|
| J1 | 문턱의 정본은 **0.6** (`XPE_AI_DEFAULT_CONFIDENCE_THRESHOLD 0.6f`, SPEC `REQ-AI-012`, `srs_ai.md` REQ-AI-002·FB-001). `docs/ai-module/` 의 **0.70** 은 정정 대상 | 리더 지시. 단, `SHA-AI-001` 의 위험 통제 항목이 "OOD 탐지 임계값 (0.70) — Implemented" 로 적혀 있어(§7) 값 변경은 위험 분석의 재평가를 요구한다. 이 초안은 값만 바꾸는 문장을 적고 위험 재평가는 §7 에 따로 올렸다 |
| J2 | `SRS-ALERT-004` 는 부위 인식에 **적용하지 않는다** | `XPE-SRS-001.md:102` 는 SRS-ALERT-004 를 "DL processing 적용됨 / Info / AI-processed label 표시" 로 정의한다. 부위 인식은 영상을 바꾸지 않고(PRD: JSON sidecar only) 리더가 성공 알림을 내지 않기로 결정했다(M3 보고서) |
| J3 | 상태 어휘: 지금 `tasks.md` 는 `done`/`pending` 만 쓴다. T-006 은 둘 다 아니므로 `partial` 을 제안한다 | 아래 A1 |

## A. SPEC-XPE-P3-AI

### A1. `.moai/specs/SPEC-XPE-P3-AI/tasks.md` T-006 행

- 현재: `| T-006 | Wire xpe_bodypart_recognize | REQ-AI-002,003,012 | T-001,T-002,T-003 | ai.cpp | pending |`
- 바꿀 문장: `| T-006 | Wire xpe_bodypart_recognize | REQ-AI-002,003,012 | T-001,T-002,T-003 | ai.cpp, ai_bodypart.h, ai_bodypart_model.h, ai_bodypart_decision.h, ai_worker_main.cpp, ai_ipc_bridge.cpp, ai_worker_supervisor.cpp | partial — 배선 완료(장난감 모델로 시험), 실제 모델·정확도·REQ-AI-004 sidecar 없음 |`
- 근거: `xpe_bodypart_recognize` 가 모델을 돌린다(`BodyPart.ConstantModelGivesItsLabelAndItsProbability`, `BodyPart.TheImageChangesTheLabel`), 워커 경로(`BodyPartWorkerPath.ControlTheModelRunsInTheWorkerAndTheAnswerComesBack`). 파일 이름은 `modules/ai/src/` 의 실재 파일.
- 다른 행: T-013(시간 예산, `pending`)은 `xpe_bone_suppress` 와 `xpe_bodypart_recognize` 의 워커 경로에서 이미 동작한다(아래 A3). 이 행을 `partial` 로 올릴지는 리더 판단(T-013 의 "ai_ipc_bridge.cpp/h" 는 실재하고 시험이 있음).

### A2. `spec.md` "재측정" 인용 블록 (134~136행 부근)

- 현재: "추론 경로는 **`xpe_bone_suppress` 에만 있습니다** … `xpe_bodypart_recognize`·`xpe_stitch_images`·`xpe_dl_denoise` 는 여전히 추론이 없는 스텁입니다."
- 바꿀 문장 (아래에 새 인용 블록을 덧붙이고 위 블록은 보존):
  > **재측정 2026-10-02 (`QA-B-191`, `#130`)** — 추론 경로는 이제 `xpe_bone_suppress` 와 `xpe_bodypart_recognize` 에 있습니다. `xpe_stitch_images`·`xpe_dl_denoise` 는 여전히 스텁입니다. `xpe_bodypart_recognize` 는 `{modelDir}/bodypart.onnx` 와 라벨 사이드카 `bodypart.json` 을 읽고, 시험은 손으로 만든 장난감 모델(분류기가 아님)로 **배선**만 증명합니다. 실제 부위 인식 모델·정확도·지연·적재 시간은 이 저장소에 없고 측정되지 않았습니다(`SRS-AI-010` 의 "CNN classifier" 는 충족되지 않았습니다).
- 근거: `modules/ai/src/ai.cpp` `xpe_bodypart_recognize_impl`; 시험 `BodyPart.*`(`test_bodypart_inference.cpp`), 모델 생성기 `tests/data/make_bodypart_models.py`.

### A3. `spec.md` 표 행 (150~158행 "개별 확인된 세 건")

**`REQ-AI-061` 행** 안의 "`xpe_bodypart_recognize` 는 추론 없이 `if (confidenceOut) *confidenceOut = 0.0f;` 로 덮고 실패를 반환합니다. `ConfidenceThreshold*` 두 시험은 헤더 상수값만 확인합니다" 와 "`AiModuleState::confidenceThreshold` 는 … 읽는 곳이 없으며" 는 **낡았다**.
- 바꿀 문장 (그 문장들 뒤에 덧붙임): `(정정 2026-10-02, QA-B-191: confidenceThreshold 는 이제 xpe_bodypart_recognize 가 읽는다(decideBodyPart). REQ-AI-012 는 부위 인식에 대해 구현됨 — 문턱 미만이면 저신뢰 이벤트 Warning 1건, fallback_mode 켬(기본)이면 UNKNOWN 과 측정된 confidence 로 XPE_ERR_PROCESSING_FAILED, 끔이면 XPE_OK 로 라벨을 돌려줌. 다른 세 진입점(stitch_images·dl_denoise·bone_suppress)에는 문턱이 없음. ConfidenceThreshold* 두 시험은 여전히 헤더 상수값만 확인함)`
- 근거 시험: `BodyPart.AConfidenceExactlyAtTheThresholdPasses`, `BodyPart.AConfidenceOneFloatBelowTheThresholdIsLowAndFallsBack`, `BodyPart.WithFallbackModeOffTheLowConfidenceLabelIsReturnedWithAWarning`, `BodyPart.FallbackModeCanBeToggledAtRunTimeAndTheNextCallFollows`, `BodyPart.TheThresholdIsTheConfiguredOneNotAConstant`, `BodyPartDecision.TheLowConfidenceTextsAreTheContractedOnes`. 코드: `ai.cpp` `decideBodyPart`, `ai_bodypart_decision.h` `LowConfidenceAlertText`.

**`REQ-AI-092` 행**: 현재 "…기본 경로(워커 끔)와 나머지 세 진입점(`xpe_bodypart_recognize`·`xpe_stitch_images`·`xpe_dl_denoise`)에는 예산이 없습니다."
- 바꿀 문장: "…기본 경로(워커 끔)와 `xpe_stitch_images`·`xpe_dl_denoise` 에는 예산이 없습니다. **`xpe_bodypart_recognize` 의 opt-in 워커 경로**(`use_worker: true`)에는 같은 예산이 있고 뼈 억제와 워커·실패 카운트를 공유합니다(정정 2026-10-02, `QA-B-191` M4c). 시험: `BodyPartWorkerPath.ASilentWorkerIsGivenUpOnAtTheBudgetAndTheNextCallRecoversOnANewWorker`."
- 근거: `ai.cpp` `bodyPartViaWorker`, `WorkerSupervisor::BodyPartRecognize`, `xpe_ai_ipc_bridge_bodypart`.

### A4. `spec.md` 에 새 항목으로 적을 만한 것 (REQ-AI-002·003)

- **`REQ-AI-003`** (워커 격리): 부위 인식도 opt-in 워커 경로를 가진다. 두 경로(프로세스 안·워커)가 같은 모델 적재(`ai_bodypart_model.h`)와 같은 판정(`ai_bodypart_decision.h`)을 쓰며, 13개 모델 디렉터리 × 6개 영상에서 같은 말을 한다: `WorkerBodyPartAgreement.TheRealWorkerAndTheInProcessPathSayTheSameThingForEveryModelAndImage`. **기본값은 워커 끔**이므로 기본 경로의 격리는 여전히 충족되지 않는다(A3 의 092 와 같은 한계).
- **`REQ-AI-002`** (결정론적 fallback): 부위 인식이 쓸 수 있는 답을 못 줄 때의 결과는 한 가지다 — `XPE_ERR_PROCESSING_FAILED`, 라벨 `"UNKNOWN"`, confidence 0.0(저신뢰는 측정된 confidence). 원인 목록(모델 없음·읽기 실패·라벨 사이드카 문제·출력 개수 불일치·입력 모양·실행 실패·비유한·범위 밖)은 `ai_api.h` `xpe_bodypart_recognize` 의 "WHEN THERE IS NO USABLE ANSWER" 절. 시험: `BodyPart.NoModelFileIsTheStubsOutcomeWithOneWarning` 외 `BodyPart.*` 의 "…TheStubsOutcomeWithOneWarning" 군.

## B. SRS (`docs/project/srs_ai.md`)

### B1. REQ-AI-BP-001 (약 164~176행 부근 "Body-Part Classification")

- 현재 Verification 칸: `TC-FALLBACK-007~014 **[SKELETON: stub returns PROCESSING_FAILED]**`
- 바꿀 문장: `TC-FALLBACK-007~014 (스텁 계약) + BodyPart.* / BodyPartWorkerPath.* / WorkerBodyPart*.* (장난감 모델로 배선 검증). **[PARTIAL: 정확도·지연은 측정되지 않음 — 실제 모델 없음]**`
- 요구 본문 "using a CNN classifier" 는 그대로 둔다(요구는 맞고 미충족).
- 근거: A1.

### B2. REQ-AI-FB-001 "Low-Confidence Event" (약 323~333행)

- 현재: "…below the configured threshold (default 0.6), the system **shall** emit a low-confidence event and fall back to the deterministic path." (Verification: `TC-FALLBACK-022 (confidence threshold default)`)
- 바꿀 문장 (본문 뒤에 구현 문장 추가): "구현(부위 인식): 문턱은 `xpe_ai_init` 의 `confidence_threshold`(기본 0.6, 같으면 통과). 문턱 미만이면 이미지당 Warning 1건 — fallback_mode 켬: `AI body-part confidence {c} is below the threshold {t} (REQ-AI-012): UNKNOWN is returned; use the deterministic body-part lookup`, 끔: `AI body-part confidence {c} is below the threshold {t} (REQ-AI-012): the label {LABEL} is returned because fallback_mode is off; an exposure parameter chosen from it may be wrong`. `{c}`·`{t}` 는 같은 float 으로 읽히는 가장 짧은 문자열."
- Verification 칸 추가: 위 A3 의 시험 6건.
- 근거: `ai_bodypart_decision.h` `LowConfidenceAlertText`; `BodyPartDecision.TheLowConfidenceTextsAreTheContractedOnes`(문구 전체 일치), `BodyPart.EveryLowConfidenceImageRaisesItsOwnEventButAPassingOneRaisesNone`.

### B3. REQ-AI-FB-002 "Fallback Mode Toggle" (약 334~345행) — **문장이 구현과 어긋난다**

- 현재: "When enabled (default), all AI functions return `XPE_ERR_PROCESSING_FAILED` on low confidence. When disabled, AI functions attempt retries."
- 바꿀 문장: "When enabled (default), `xpe_bodypart_recognize` returns `XPE_ERR_PROCESSING_FAILED` with label `UNKNOWN` and the measured confidence on low confidence. When disabled, it returns `XPE_OK` with the model's most probable label and its confidence, and posts the low-confidence Warning. 재시도는 하지 않는다. 문턱을 읽는 함수는 현재 `xpe_bodypart_recognize` 하나다."
- 근거: `BodyPart.WithFallbackModeOffTheLowConfidenceLabelIsReturnedWithAWarning`, `BodyPart.FallbackModeCanBeToggledAtRunTimeAndTheNextCallFollows`; 코드 `decideBodyPart`. **"retries" 를 구현하는 코드는 `modules/ai/src` 에서 찾지 못했다** — 확인 방법: 이 문장의 "retries" 를 근거로 하는 시험·코드가 있는지 리더가 한 번 더 보라(`modules/ai/src` 에서 `fallbackMode` 를 쓰는 곳은 `ai.cpp` 의 초기화 파싱(297행), `decideBodyPart`(628행), `xpe_ai_set_fallback_mode`(1477행) 셋뿐이고, `retry` 는 워커 시작 중 파이프 연결을 다시 시도하는 두 곳(`ai_ipc_bridge.cpp`, `ai_worker_supervisor.cpp`)뿐이다 — 추론을 다시 돌리는 코드는 없다. 프로토콜 헤더의 `XPE_AI_FLAG_FALLBACK_MODE` 주석 "worker skips retries" 도 정의만 있다).

### B4. 워커 실패·중단 알림 (새 요구 항목 제안 또는 REQ-AI-092 설명)

- 연속 3회 실패하면 워커를 세션 동안 끈다(카운트는 뼈 억제와 공유). 실패마다 Warning:
  - 부위 인식: `AI worker failed (code {n}, failure {k} of 3): body-part recognition returns UNKNOWN; use the deterministic body-part lookup (REQ-AI-002, REQ-AI-092)`
  - 뼈 억제: `AI worker failed (code {n}, failure {k} of 3): the input image is returned unchanged (REQ-AI-002, REQ-AI-092)`
  - 3회째(중단, 두 기능 공통): `AI worker failed (code {n}, failure 3 of 3) during {cause} and is disabled for this session: body-part recognition returns UNKNOWN, bone suppression returns the input image unchanged (REQ-AI-002, REQ-AI-092)` — `{cause}` 는 `body-part recognition` 또는 `bone suppression`
  - "부위 모델을 쓸 수 없음" 은 실패가 아니다(세지 않고 카운트를 0 으로): `AI body-part recognition is unavailable (the AI worker has no usable body-part model): UNKNOWN is returned; use the deterministic body-part lookup (REQ-AI-002)` — 세션당 1건
- 근거: `BodyPartWorkerPath.AModelThatExistsAndFailsToRunIsCountedAndThirdFailureSwitchesTheWorkerOffForBothFunctions`, `…BoneSuppressionFailuresSwitchOffBodyPartRecognitionToo`, `…AnUnavailableBodyPartModelIsOneWarningAndNeverCountedAndNeverSwitchesTheWorkerOff`, `…WithNoBodyPartModelBoneSuppressionIsNotSkippedByTheBodyPartCalls`. 결정 기록은 `docs/project/REQ-CHANGE-LOG-P3-AI.md` 에 **7행으로 추가해야 한다**(§8).

### B5. `SRS-ALERT-004` 미적용 사유 (REQ-AI-BP-001 속성 아래 한 줄)

- 바꿀 문장: "`SRS-ALERT-004`(DL processing 적용됨, Info — `XPE-SRS-001.md:102`)는 `xpe_bodypart_recognize` 에 적용하지 않는다. 이 함수는 영상을 바꾸지 않고(결과는 라벨과 confidence 뿐), 성공 알림은 영상이 AI 로 처리됐음을 표시하는 것이기 때문이다(리더 결정, QA-B-191 M3). 성공한 호출은 알림을 내지 않는다."
- 근거: `BodyPart.ConstantModelGivesItsLabelAndItsProbability`(`EXPECT_TRUE(Alerts().empty())`), `BodyPartWorkerPath.ControlTheModelRunsInTheWorkerAndTheAnswerComesBack`.

## C. SDD

### C1. `docs/project/sdd_ai.md` §4.3 (236~260행) — `LOW_CONFIDENCE` 플래그

- 현재: `|   |       else return result + LOW_CONFIDENCE flag` 와 블록 전체(스텁 아니면 "Send IPC request to worker / Wait for response (timeout: 5s)" 만 적혀 있고 워커 끔(기본)·프로세스 안 경로가 없다).
- 사실: **API 에 `LOW_CONFIDENCE` 필드가 없다** — `xpe_bodypart_recognize(img, bodyPartOut, bufLen, confidenceOut)` 는 라벨·confidence 만 돌려주고, "저신뢰" 는 (a) fallback_mode 켬: 반환 코드 `XPE_ERR_PROCESSING_FAILED` + `UNKNOWN` + 측정된 confidence, (b) 끔: `XPE_OK` + 라벨 + Warning 알림으로 표현된다. `modules/` 에서 `LOW_CONFIDENCE` 는 `ai_worker_protocol.h:157` 의 **워커 프로토콜 헤더 플래그** `XPE_AI_FLAG_LOW_CONFIDENCE`(0x4) 한 곳뿐이고 코드에서 쓰는 곳이 없다(`XPE_AI_FLAG_FALLBACK_MODE` 0x8 도 같다 — 정의만 있다). 문서에서는 `sdd_ai.md:253` 한 곳. 즉 SDD 가 말한 "LOW_CONFIDENCE flag" 는 공개 API 의 반환값이 아니라 쓰이지 않는 프로토콜 플래그 이름이다.
- 바꿀 블록(의사코드):

```
xpe_bodypart_recognize(img, out, bufLen, conf)
    +-- NULL 검사 -> INVALID_INPUT, 초기화 안 됨 -> NOT_INITIALIZED, bufLen == 0 -> INVALID_INPUT
    +-- if (STUB_BUILD) return PROCESSING_FAILED + "UNKNOWN" + 0.0
    +-- if (use_worker)      // opt-in, 기본 끔
    |       형식이 FLOAT32 가 아니면 UNSUPPORTED_FORMAT
    |       워커에 요청(예산 timeout_ms, 기본 5 s) -> 라벨·confidence 또는 거절(비유한/범위 밖) 또는 "모델 사용 불가"
    |       실패(무응답·종료·잘못된 응답·실행 실패) -> 연속 실패 +1, 3회째에 워커 중단(뼈 억제와 공유)
    +-- else                 // 프로세스 안
    |       모델 적재(bodypart.onnx + bodypart.json), 크기 조정, 추론
    +-- 출력 판정: 유한 -> [0,1] -> 최대 클래스(동점은 첫 클래스)
    +-- if (confidence < threshold):
    |       Warning(저신뢰 이벤트)
    |       if (fallback_mode) return PROCESSING_FAILED + "UNKNOWN" + 측정된 confidence
    |       else return OK + 라벨 + confidence
    +-- return OK + 라벨 + confidence
```

- "정리 문장" (§4.3 아래): "`LOW_CONFIDENCE` 는 공개 API 의 필드가 아니다. 같은 이름의 워커 프로토콜 플래그(`XPE_AI_FLAG_LOW_CONFIDENCE`)는 헤더에 정의만 있고 어느 메시지도 쓰지 않는다. 저신뢰는 반환 코드·라벨·알림의 조합으로 표현된다."
- 근거: `ai.cpp` `xpe_bodypart_recognize_impl`·`decideBodyPart`·`bodyPartViaWorker`; 시험 `BodyPart.WithFallbackModeOffTheLowConfidenceLabelIsReturnedWithAWarning` 등.
- 별도 정리 대상: `ai_worker_protocol.h:156-160` 의 두 플래그 주석("low confidence result; caller should use fallback", "worker skips retries")은 아무 코드도 쓰지 않는 정의다. 플래그를 지우거나 "예약됨, 미사용" 으로 적는 것은 `modules/ai` 소유의 코드 주석 변경이라 이 보고서에서는 하지 않았다.

### C2. `docs/post-processing/xpe/XPE-SDD-002_Software_Detailed_Design.md` §3.7 (724~766행) — **세 곳이 구현·SRS 와 어긋난다**

| 현재 | 구현 | 제안 |
|---|---|---|
| 의사코드 2: `Resize input to 224x224 (MobileNet-v3 input size)` | 크기는 **모델이 선언한 입력**(`[1,1,H,W]` 또는 `[1,H,W,1]`, 1≤H,W≤4096)을 따른다(`BodyPartInputSize`). 224 는 코드에 없다 | "모델의 선언된 입력 크기로 줄인다(면적 평균, 키울 때 선형 보간)" |
| 의사코드 3: `Normalize to [0, 1] range` | **정규화하지 않는다** — 모델의 학습 스케일 그대로 화소를 준다(헤더 PIXEL SCALE) | "정규화는 호출자 몫" |
| 의사코드 5: `IF confidence >= 0.95 ... ELSE DICOM tag (0018,0015) fallback` | 문턱은 설정값(기본 **0.6**), 미만이면 `UNKNOWN`·PROCESSING_FAILED. **DICOM 태그 fallback 은 이 모듈에 없다**(호출자의 "결정론적 부위 조회") | 0.95 → 설정 가능 기본 0.6, "DICOM 태그 fallback" → "호출자가 결정론적 부위 조회(DICOM 태그 등)를 한다" |
| 에지 케이스 표 "Low confidence (<0.95) → Use DICOM tag" | 같음 | "<문턱(기본 0.6) → UNKNOWN 반환, 호출자가 조회" |
| 함수 이름 `xpe_recognize_bodypart` | 실제 이름은 `xpe_bodypart_recognize` | 이름 정정 |

- 근거: `ai_bodypart.h`(`BodyPartInputSize`, `ResizeImageFloat`), `ai_api.h` PIXEL SCALE AND SIZE 절, `XPE_AI_DEFAULT_CONFIDENCE_THRESHOLD`. 시험: `BodyPart.AnImageOfAnotherSizeIsResizedToTheModelsInputBeforeItIsRun`, `BodyPartResize.*`.
- **0.95 대 0.70 대 0.6 — 세 문서가 세 값을 말한다**(SDD-002 0.95, ai-module 0.70, srs_ai·SPEC·코드 0.6). 리더의 J1 이 0.6 을 정본으로 정했으므로 SDD-002 도 같이 맞추자고 제안한다. 0.95 가 의도된 값이었는지는 이 저장소에서 확인되지 않는다.

## D. PRD·ai-module 문서의 0.70

0.70 이 **부위 인식의 신뢰도 문턱** 으로 적힌 곳(해당 문장을 읽고 고른 것만):

| 파일:줄 | 현재 | 바꿀 문장 |
|---|---|---|
| `docs/ai-module/xpe-ai-prd.md:54` | `OOD(Out-Of-Distribution) 탐지: 최대 신뢰도 < 0.70 → body_part = "UNKNOWN" 반환` | `… < 0.6(설정 가능, 기본값) → …` |
| `xpe-ai-prd.md:58` | `신뢰도 < 0.70인 경우 자동으로 "UNKNOWN" 반환` | `신뢰도 < 문턱(기본 0.6)인 경우 …` |
| `xpe-ai-prd.md:278` | `최대 신뢰도 임계값: 0.70 (SWU-2.7)` | `최대 신뢰도 임계값: 0.6 기본값, 설정 가능 (SWU-2.7)` |
| `docs/ai-module/SRS-AI-001_….md:93, 98, 325, 428` | `신뢰도 < 0.70 → "UNKNOWN"`, `… 시 자동 "UNKNOWN" 반환`, `confidence ≥ 0.70`, JSON 예 `"confidence_threshold": 0.70` | 0.70 → 0.6 (428행은 예시 값) |
| `docs/ai-module/SAD-AI-001_….md:267` | `float confidence_threshold = 0.70f;` | `0.6f` |
| `docs/ai-module/README.md:276, 356` | `confidence ≥ 0.70` | `≥ 0.6` |
| `docs/ai-module/RTM-AI-001_….md:134` | `TC-AI-130-03 / OOD 탐지 / confidence < 0.70 → "UNKNOWN"` | `< 0.6` |

- 근거: `XPE_AI_DEFAULT_CONFIDENCE_THRESHOLD 0.6f`(`ai_worker_protocol.h:62`), 시험 `AiFallbackTest.ConfidenceThresholdDefaultIs06`(헤더 상수만 확인), `BodyPart.AConfidenceExactlyAtTheThresholdPasses`(실제 판정이 0.6 기본값으로 동작: 모델 답 0.6f 가 기본 문턱을 통과).
- **지연 문장은 그대로 둔다**: PRD 의 `≤ 300ms (3072×3072, 리사이즈 + 추론)` 는 판정할 수 없다. 측정한 것은 크기 조정뿐이다(3072²→512² 7.1~7.3 ms, `QA-B-191/m3b_report.md`, 장난감 모델, 한 대의 PC).
- 이 문서들이 **이 파일 집합이 맞는지** 는 `grep -n "0\.70" docs/ai-module/*.md` 로 열거한 결과이고, 본 보고서의 나머지 파일(`docs/quality-eval/*`, `XPE-ALG-001` 등의 0.70)은 부위 인식 문턱이 아니라서 뺐다.

## E. rtm_ai · vvp_ai

### E1. `docs/project/rtm_ai.md` §4 (77~90행)에 추가할 행

기존 BP-001 의 "Stub …" 행 두 개(87·88행) 아래에 추가. 열: Req ID | Requirement | SRS Ref | SDD Ref | Implementation Files | Test IDs | Status | VVP Ref.
(기존 표기는 `TC-FALLBACK-0xx: 이름` 인데 새 시험은 TC 번호가 없다. 번호를 새로 짓지 않고 **시험 이름**을 쓴다. `check_spec_test_refs.py` 가 이름 인용을 검사하므로 아래 이름은 모두 실재한다.)

| Req ID | Requirement | Implementation Files | Test IDs |
|---|---|---|---|
| REQ-AI-BP-001 | 모델의 출력이 라벨·confidence 가 된다 | `ai.cpp`, `ai_bodypart_model.h` | `BodyPart.ConstantModelGivesItsLabelAndItsProbability`, `BodyPart.ADifferentModelDirectoryChangesTheLabel`, `BodyPart.TheImageChangesTheLabel`, `BodyPart.ATieGoesToTheFirstClassAndAFullScaleProbabilityIsAccepted`, `BodyPart.ANhwcModelGivesTheSameAnswersAsTheNchwOne` |
| REQ-AI-BP-001 | 입력 영상을 모델 입력 크기로 줄이거나 키운다 | `ai_bodypart.h` | `BodyPart.AnImageOfAnotherSizeIsResizedToTheModelsInputBeforeItIsRun`, `BodyPartResize.ShrinkingAveragesTheArea`, `BodyPartResize.EnlargingInterpolatesAndStaysInsideTheSourceRange`, `BodyPartInputSize.TheShapesTheModuleWillFeed` |
| REQ-AI-002 | 쓸 수 있는 답이 없으면 `UNKNOWN`·0.0·PROCESSING_FAILED (+세션당 Warning 1건) | `ai.cpp` | `BodyPart.NoModelFileIsTheStubsOutcomeWithOneWarning`, `BodyPart.ABrokenModelFileIsTheStubsOutcomeWithOneWarning`, `BodyPart.MissingLabelsAreTheStubsOutcomeWithOneWarning`, `BodyPart.MoreOutputsThanLabelsAreTheStubsOutcomeWithOneWarning`, `BodyPart.ARankTwoInputIsTheStubsOutcomeWithOneWarning`, `BodyPart.ADynamicInputIsTheStubsOutcomeWithOneWarning`, `BodyPart.WithoutAModelNoImageFormatChangesTheOutcome` |
| REQ-AI-002 | 모델 출력이 확률 벡터가 아니면 거절 | `ai_bodypart_decision.h` | `BodyPart.ANonFiniteModelResultIsRefusedWithTheNonFiniteAlert`, `BodyPart.AProbabilityAboveOneIsRefused`, `BodyPart.ANegativeValueIsRefusedEvenWhenTheLargestIsInRange`, `BodyPartDecision.AValueJustOutsideTheRangeIsRefusedWhole`, `BodyPartDecision.NonFiniteIsToldApartFromOutOfRangeAndCheckedFirst` |
| REQ-AI-FB-001 (=REQ-AI-012) | 문턱 미만 → 저신뢰 이벤트와 fallback | `ai.cpp` `decideBodyPart` | `BodyPart.AConfidenceExactlyAtTheThresholdPasses`, `BodyPart.AConfidenceOneFloatBelowTheThresholdIsLowAndFallsBack`, `BodyPart.TheThresholdIsTheConfiguredOneNotAConstant`, `BodyPart.AThresholdOfOneAcceptsOnlyAFullScaleConfidence`, `BodyPart.EveryLowConfidenceImageRaisesItsOwnEventButAPassingOneRaisesNone`, `BodyPart.ALowConfidenceFallbackNeedsRoomForUnknownAndStillRaisesTheEvent` |
| REQ-AI-FB-002 | fallback_mode 끔 → 라벨 반환 + Warning, 런타임 전환 | `ai.cpp` | `BodyPart.WithFallbackModeOffTheLowConfidenceLabelIsReturnedWithAWarning`, `BodyPart.FallbackModeCanBeToggledAtRunTimeAndTheNextCallFollows`, `BodyPart.WithFallbackModeOffALabelThatDoesNotFitIsStillBufferTooSmall` |
| REQ-AI-003 | 워커 프로토콜: 엄격한 응답 파싱 | `ai_ipc_bridge.cpp`, `ai_worker_main.cpp` | `WorkerBodyPartReply.EveryReplyTheProtocolForbidsIsAProtocolFaultAndTheWorkerIsDiscarded`, `WorkerBodyPartReply.ControlAValidReplyIsAcceptedAndKeepsTheWorker`, `WorkerBodyPartReply.TheFlagCountsOnlyBeforeTheFreeTextMessage`, `WorkerBodyPartReply.ARefusalOfTheModelsOutputIsAValidReplyThatCarriesNoAnswer` |
| REQ-AI-003 | 워커 경로와 프로세스 안 경로가 같은 말을 한다 | `ai_bodypart_model.h`, `ai_bodypart_decision.h` | `WorkerBodyPartAgreement.TheRealWorkerAndTheInProcessPathSayTheSameThingForEveryModelAndImage`, `WorkerBodyPartAgreement.TheWorkerResizesTheImageToTheModelsInputAndTheAnswerFollowsTheImage`, `WorkerBodyPartAgreement.ALabelOutsidePrintableAsciiOrWithAQuoteOrBackslashIsUnavailableInBothPaths` |
| REQ-AI-092 | 무응답 워커는 예산에서 포기, 다음 호출은 새 워커 | `ai_worker_supervisor.cpp` | `BodyPartWorkerPath.ASilentWorkerIsGivenUpOnAtTheBudgetAndTheNextCallRecoversOnANewWorker` |
| REQ-AI-002 | 연속 실패 3회 → 워커 중단(두 기능 공유), 사용 불가는 세지 않음 | `ai.cpp` `bodyPartViaWorker` | `BodyPartWorkerPath.AModelThatExistsAndFailsToRunIsCountedAndThirdFailureSwitchesTheWorkerOffForBothFunctions`, `BodyPartWorkerPath.BoneSuppressionFailuresSwitchOffBodyPartRecognitionToo`, `BodyPartWorkerPath.AnUnavailableBodyPartModelIsOneWarningAndNeverCountedAndNeverSwitchesTheWorkerOff`, `BodyPartWorkerPath.WithNoBodyPartModelBoneSuppressionIsNotSkippedByTheBodyPartCalls`, `BodyPartWorkerPath.AnUnavailableAnswerEndsARunOfBoneSuppressionFailures`, `BodyPartWorkerPath.ShutdownThenInitRecoversAWorkerSwitchedOffByBodyPartFailures` |

- Status 열: `Written`(시험이 있고 풀 빌드 `ci-ai` 에서 통과, 스텁 빌드는 풀 전용 시험을 건너뜀). 코드가 풀 빌드에서만 도는 것은 사실이므로 비고에 적어야 한다.
- `rtm_ai.md` 의 "Current (Skeleton / Stub Build)" 커버리지 요약(218행~)과 "Test File Summary"(247행~)에 새 시험 파일 4개 추가: `test_bodypart_inference.cpp`, `test_bodypart_decision.cpp`, `test_worker_bodypart.cpp`, `test_bodypart_worker_path.cpp`.
- **기존 행의 낡은 인용**: 166행 `TC-FALLBACK-022: ConfThresholdDefault` 에 해당하는 시험 이름은 `ConfThresholdDefault` 가 아니라 `AiFallbackTest.ConfidenceThresholdDefaultIs06` 이다(`modules/ai/tests/test_ai_fallback.cpp:188`; `ConfThresholdDefault` 로 검색한 결과는 시험 소스에 0건). 별개로 123행·134행도 같은 `TC-FALLBACK-022` 번호를 쓴다(번호 중복).

### E2. `docs/project/vvp_ai.md` §4.3 (334~357행)

- 현재: "**Purpose**: CNN body-part classification. **Not implemented** — stub." + V4.3.1 스텁 계약 표 + "**Not verified**: classification accuracy, confidence calibration, demographic performance, inference latency. None of these is measurable in this build (§4.9)."
- 바꿀 문장: Purpose → "CNN 부위 분류. **배선 구현됨, 실제 모델 없음.** 스텁 빌드는 스텁 계약, 풀 빌드(`ci-ai`)는 장난감 모델로 배선을 시험한다." V4.3.1(스텁 계약)은 보존하고 **V4.3.2 장난감 모델 시험(L1)** 을 추가: E1 표의 시험 이름들을 그룹별로(답·크기·fallback·문턱·거절). **V4.3.3 워커 경로(L1)**: `WorkerBodyPartReply.*`(가짜 워커로 엄격 파서), `WorkerBodyPartAgreement.*`(실제 워커 대 프로세스 안), `BodyPartWorkerPath.*`(예산·카운트·중단). 반증 기록: `.moai/reports/lane-post/QA-B-191/m4b_arms_run2_out.txt`, `m4c_arms_out.txt`, `m4e_arms_out.txt`.
- "Not verified" 문장 유지·보강: "분류 정확도, confidence 보정, 인구통계 성능, 실제 모델의 추론·적재 지연은 측정되지 않았다. 시험 모델은 분류기가 아니다(`tests/data/make_bodypart_models.py`). 측정된 것: 크기 조정 시간(3072²→512² 7.1~7.3 ms, 한 대의 PC)과 워커 첫 호출(중앙값 약 80 ms, 느린 꼬리 약 650~740 ms 가 약 10회 중 1회, 시작·연결 단계)."
- 현재 RTM 교차 참조 문장(`REQ-AI-BP-001 rows carry XPE-VVP-AI-001 §4.3`)은 그대로 맞다.

## F. `docs/project/api-spec.md`

### F1. §9.3 `xpe_bodypart_recognize` (1128~1147행)

- 현재 Description: "Classifies the anatomical body part in `img` using a CNN classifier. Writes the label … and the confidence score [0,1]…" / Error codes 에 `XPE_ERR_UNSUPPORTED_FORMAT` 없음.
- 바꿀 문장: Description 뒤에 추가 — "Runs `{modelDir}/bodypart.onnx` (labels in `{modelDir}/bodypart.json`; printable ASCII 0x20–0x7E without `\"` or `\\`, each < 64 bytes). The model must emit probabilities (no softmax is applied). A confidence below `confidence_threshold` (default 0.6) is a low-confidence event: with `fallback_mode` on (default) the call returns `XPE_ERR_PROCESSING_FAILED`, label `\"UNKNOWN\"` and the measured confidence; with it off, `XPE_OK` and the label with a Warning. With no usable answer (no model, unreadable model, bad sidecar, run failure, non-finite or out-of-range output) the outcome is `XPE_ERR_PROCESSING_FAILED`, `\"UNKNOWN\"`, 0.0. `use_worker: true` runs the model in `xpe_ai_worker.exe` (time budget `timeout_ms`; failure count shared with `xpe_bone_suppress`). The full contract is the header comment in `ai_api.h`. 시험에 쓴 모델은 장난감 모델이며 정확도는 주장하지 않는다."
- Error codes 에 `XPE_ERR_UNSUPPORTED_FORMAT`(모델이 쓸 수 있는데 영상이 FLOAT32 가 아님; `use_worker` 에서는 모델 유무와 무관) 추가.
- 근거: `ai_api.h` `xpe_bodypart_recognize` 문서 블록, 시험 `BodyPart.ANonFloatImageIsUnsupportedWhenAModelIsThere`, `BodyPartWorkerPath.ANonFloatImageIsRefusedBeforeTheWorkerIsAskedWhateverTheModel`.

### F2. §9.1 `xpe_ai_init` 키 표 (1108~1114행)

- 현재: 네 키(`execution_provider`, `timeout_ms`, `confidence_threshold`, `fallback_mode`)가 "normative" 로 적혀 있고 **`use_worker` 가 없다**(같은 문서 §9.8 설명에는 나온다).
- 바꿀 문장: 표에 행 추가 — `| use_worker | boolean | routes xpe_bone_suppress and xpe_bodypart_recognize through the worker process (default false) |` 와 `confidence_threshold` 행 설명을 `confidence floor for xpe_bodypart_recognize (default 0.6; a confidence equal to it passes)` 로.
- 근거: `ai.cpp` 의 `use_worker` 파싱(`state->useWorker`), `BodyPartWorkerPath.*`. 이 표가 `use_worker` 를 빼먹은 것은 이 초안 작업 중 발견한 별개의 누락이다.

### F3. §9.8 `xpe_ai_worker_state` (1214~)

- 현재: "Read-only query of the **bone-suppression** worker path for the current session."
- 바꿀 문장: "…of the worker path (shared by `xpe_bone_suppress` and `xpe_bodypart_recognize`) for the current session." 연속 실패 카운트가 두 함수의 합이라는 한 줄.
- 근거: `BodyPartWorkerPath.BoneSuppressionFailuresSwitchOffBodyPartRecognitionToo`, `ai_api.h` `xpe_ai_worker_state` 문서의 "The count is shared …".

## G. SHA 관련 (판단이 필요한 것)

`docs/ai-module/SHA-AI-001_….md:39, 44` 는 "confidence < 0.70 → UNKNOWN 자동 반환", 통제 목록의 "OOD 탐지 임계값 (0.70) — Preventive — **Implemented**" 이다. 같은 값이 `docs/post-processing/xpe/XPE-SHA-001.md:236, 265`(HAZ-010 의 보호 조치 문구)에도 있다.
- 값을 0.6 으로 바꾸면 이 통제가 막는 오분류의 비율이 달라질 수 있다 — **위험 분석의 재평가가 필요한 결정**이라 문장을 적지 않고 리더에게 올린다.
- `Implemented` 상태의 정확성: 문턱 판정 코드는 이제 있고 시험된다(`BodyPart.*`). 그러나 통제의 효과(실제 모델의 신뢰도가 오분류와 상관이 있는지)는 측정할 수 없다 — 실제 모델이 없다. 이 문장은 "구현됨(배선), 효과 미검증" 으로 적어야 정확하다고 본다.

## H. 문서 간 어긋남 목록 (이 초안 작업 중 발견, 위 어느 항목에도 속하지 않는 것)

1. 문턱이 세 값이다: 0.95(XPE-SDD-002 §3.7), 0.70(ai-module 문서군·SHA·XPE-SHA-001), 0.6(코드·SPEC·srs_ai). 위 §C2·§D·§G.
2. `srs_ai.md` REQ-AI-FB-002 의 "retries"(§B3).
3. `api-spec.md` §9.1 키 표에 `use_worker` 없음(§F2).
4. `rtm_ai.md` 166행 시험 이름 `ConfThresholdDefault` ↔ 실제 `ConfidenceThresholdDefaultIs06`(§E1).
5. `REQ-CHANGE-LOG-P3-AI.md` 의 "열린 항목" 둘(SRS-AI-001:447 의 classical fallback, SHA-AI-001:176-178 의 파이프라인 정지)은 이 카드로 닫히지 않았다.

## I. `REQ-CHANGE-LOG-P3-AI.md` 에 추가할 행 (7행, 초안)

`REQ-AI-092` 표(1~6행) 아래, 승인 칸은 리더가 채운다.

| # | 날짜 | 이슈 · 카드 | 결정 | 근거 |
|---|---|---|---|---|
| 7 | 2026-10-02 | #130 · `QA-B-191` M4 | **워커 실패 카운트는 `xpe_bone_suppress` 와 `xpe_bodypart_recognize` 가 하나를 공유한다. 단 "부위 모델을 쓸 수 없음"(`model_unavailable` 오류 프레임)은 실패로 세지 않고 카운트를 0 으로 되돌린다.** 모델이 있는데 실행이 실패하면 센다. 뼈 억제의 규칙(4행)은 바꾸지 않는다. 3회째 중단 알림은 두 기능의 효과와 원인 기능을 말한다 | 워커 프로세스가 하나라 건강도도 하나다. 순수 공유는 부위 모델이 없는 배치에서 부위 인식 호출 3번이 뼈 억제를 끈다(`BodyPartWorkerPath.WithNoBodyPartModelBoneSuppressionIsNotSkippedByTheBodyPartCalls`). 설계 `m4_design.md` §4, 리더 승인 2026-10-02(D7, D10 대체) |

## 9. 대조 방법과 미검증

- 시험 이름: `modules/ai/tests/test_bodypart_inference.cpp`, `test_bodypart_decision.cpp`, `test_worker_bodypart.cpp`, `test_bodypart_worker_path.cpp` 에서 `TEST`/`TEST_F` 정의를 정규식으로 열거해 이 보고서에 쓴 이름과 비교했다. `AiFallbackTest.ConfidenceThresholdDefaultIs06`, `AiFallbackTest.Bodypart…` 이름은 `test_ai_fallback.cpp` 의 `TEST_F` 줄을 읽어 확인했다. **기계 대조 결과**: `modules/ai/tests/*.cpp` 의 `TEST`/`TEST_F` 정의 400개를 열거해 이 보고서가 백틱으로 인용한 시험 이름 49개를 비교했고 누락은 0건이다(대조군: 없는 이름 `BodyPart.NoSuchTest` 는 목록에 없음). 이 초안의 이름이 문서에 옮겨진 뒤에는 `python tools/docs/check_spec_test_refs.py` 가 인용을 검사한다.
- 줄 번호는 이 트리(`dev/postprocess`, main 병합 `94ebc29d` 위)에서 읽은 값이고, 리더가 옮길 때 문서가 바뀌었을 수 있다. 줄 번호 대신 인용한 현재 문장으로 찾아야 한다.
- **미검증**: ① `srs_ai.md` 의 REQ-AI-BP-001 줄 번호는 추정이 아니라 읽은 값이나 REQ-AI-FB-001·FB-002 의 정확한 줄은 "약"으로 적었다. ② 이 초안의 새 문장을 적용한 뒤 doxygen·문서 린트·`check_spec_test_refs.py` 를 돌리지 않았다(문서를 바꾸지 않았다). ③ `XPE-SRS-001.md`·`XPE-SDD-001.md` 에도 부위 인식 관련 요구(`SRS-FUNC-016`, ≥95% 정확도)가 있으나 코드·시험이 아직 만족시키지 못해 초안에서 제외했다 — 상태 열을 어떻게 적을지는 리더 판단. ④ `docs/project/sdd_ai.md` 의 §4.3 외 다른 절(예: 최상위 구조도)에 워커 경로·부위 인식 문장이 더 있는지는 전체를 읽지 않았다(키워드 검색만).
- **잔여 위험**: 문서가 코드보다 먼저 "구현됨" 을 말하면 이번 세션이 여러 번 만난 "문서가 본 것을 코드가 따라 한다" 오류가 된다. 이 초안은 모든 "구현됨" 문장에 시험 이름을 붙였고, "정확도·지연·실제 모델" 은 모든 곳에서 미측정으로 적었다.
