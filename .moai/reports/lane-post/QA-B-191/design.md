# QA-B-191 — #130 T-006 `xpe_bodypart_recognize` 실연결: 설계 메모

카드: QA-B-191 (설계 메모만, 코드 없음) · 관련: #130, #210(REQ-AI-061 은 별개로 열려 있음) · 브랜치 `dev/postprocess`
근거 확인 파일: `facts_check.txt`(이 메모가 기대는 사실 확인 명령과 출력, 대조군 병기).

## 결론 먼저

1. **지금 할 수 있다 — 단, 실제 모델 없이 "배선"까지만.** 부위 인식 ONNX 모델은 저장소에도 조달 기록에도 없다. 시험은 손으로 쓴 작은 결정적 모델(입력→고정 출력)로 하고, 그 한계(정확도·임상 성능에 대해 아무 말도 못 함)를 코드·헤더에 적는다. 이것은 T-008(`xpe_bone_suppress`)이 걸은 길과 같다.
2. **코드에 들어가기 전에 리더가 정해야 할 것이 다섯 있다(§7 D1~D5).** 그중 가장 무거운 둘은 **저신뢰일 때 계약이 문서 세 곳에서 서로 다르다는 것(D3)** 과 **워커 경로를 이 카드에 넣을지(D5)** 다.
3. **소비자 위험은 낮다.** `xpe_bodypart_recognize` 를 부르는 C# 호출 0곳(대조군: `xpe_bone_suppress` 는 4곳). 헤더 계약을 바꿔도 `clients`·`gui` 가 깨질 자리가 없다.
4. 마일스톤은 M1~M5(§6). **M2 까지가 실연결의 핵심이고, M4(워커 경로)가 가장 크다.**

## 1. 모델

| 질문 | 답 | 근거 |
|---|---|---|
| 부위 인식 모델이 저장소에 있는가 | **없다** | 추적되는 `*.onnx` 6개가 전부 장난감(`min_scale2/3`, `models_x2/x3/bone_suppress.onnx`, `models_broken/…`, `not_a_model.onnx`). 부위 인식 모델 0 |
| 조달 경로에 있는가 | **없다** | #130 의 QA-B-152 코멘트가 "모델이 없습니다 — 의존을 다 구해도 추론은 못 섭니다"로 기록. QA-B-160 이 조달한 것은 ONNX Runtime 1.30.0 뿐이고 모델은 아니다 |
| 요구가 모델에 대해 말하는 것 | PRD(`docs/ai-module/xpe-ai-prd.md`): MobileNet-v3-Small, INT8, < 5 MB, 입력 512×512 그레이스케일 float32, 15개 이상 부위, 최대 신뢰도 < 0.70 → `UNKNOWN`. `REQ-AI-007` 은 모델 서명 검증을 요구하나 미구현 | PRD, SPEC |
| 시험은 무엇으로 | **결정적 장난감 모델** | 아래 |

**제안 (D1)**: 모델 위치는 `<modelDir>/bodypart.onnx`, 라벨은 같은 이름의 **사이드카 `bodypart.json`**(`OnnxSession::Create` 가 이미 `<model>.json` 을 읽어 `ModelMetadata` 를 채운다 — `replace_extension(".json")`)에 `"labels": [...]` 를 추가한다. 파일 이름은 `bone_suppress.onnx` 의 선례를 따른다(모델 카드 ID `bodypart_cnn_v1` 과는 다른 축이다 — 뼈 억제도 파일명 `bone_suppress.onnx` ↔ 카드 ID `bone_suppress_unet_v1`).

**장난감 모델 설계** (`tests/data/make_min_models.py` 의 stdlib protobuf 인코더를 확장; 필요한 연산은 `Flatten`·`MatMul`·`Add` 뿐이고 `onnx` 패키지가 필요 없다):
- 입력 `[1,1,H,W]` float32(H=W=4), 출력 `[1,N]` float32 — **모델이 확률을 직접 낸다**(소프트맥스를 모듈이 적용하지 않는다, D3 에서 이유).
- **상수 모델** (`W=0`, `B=[0.6, 0.3, 0.1]`): 입력과 무관하게 확률이 고정이므로 **문턱 경계**(정확히 0.6 = 문턱)를 만들 수 있다. 서로 다른 `B` 를 가진 두 벌로 "모델을 바꾸면 라벨이 바뀐다"를 단언한다(QA-B-160 이 `Y=2X`/`Y=3X` 로 한 것과 같은 반증).
- **입력 의존 모델** (`W` 가 윗절반 평균·아랫절반 평균을 클래스 0·1 로 보낸다): 윗부분이 밝은 영상과 아랫부분이 밝은 영상이 **다른 라벨**이 되어야 한다. 상수 모델만 있으면 입력을 무시하는 구현도 통과한다(#154·#155 가 보여 준 "계산되고 실행되고 답에서 약분되는" 형태).
- **불량 모델들**: 출력에 NaN/inf 를 만드는 모델(`W` 에 inf), 범위를 벗어나는 확률(1.5, −0.1)을 내는 모델, 클래스 수가 라벨 수와 다른 모델.

**한계(코드·헤더에 그대로 적을 문장)**: 장난감 모델로는 라벨 의미·정확도·지연 시간을 알 수 없다. 이 카드가 만드는 것은 "모델 입력 → 모델 출력 → 라벨·신뢰도·문턱 판정·알림" 배선이고, "부위를 맞춘다"가 아니다.

## 2. 입출력 계약

### 2.1 지금의 계약

- 시그니처: `XpeErrorCode xpe_bodypart_recognize(const XpeImageBuffer* img, char* bodyPartOut, size_t bufLen, float* confidenceOut)` (`ai_api.h`, SRS-AI-010).
- 반환(헤더): `OK`(ONNX 빌드만), `NOT_INITIALIZED`, `INVALID_INPUT`(NULL, 잘못된 버퍼, `bufLen` 0), `BUFFER_TOO_SMALL`(라벨이 안 들어감, 잘라 쓰지 않는다), `PROCESSING_FAILED`(추론 실패 — 스텁은 무조건 이것이고 `"UNKNOWN"` 과 confidence 0.0 을 쓴다).
- 현재 구현은 `g_aiState` 도 세션도 읽지 않는다(함수 본문에서 `g_aiState`·`OnnxSession`·`Run(` 0건, `facts_check.txt`).
- 요구 원문: `REQ-AI-002` "All AI inference shall have a deterministic fallback path … when AI is disabled, fails, or confidence is below threshold." · `REQ-AI-003` "AI modules shall run in worker-isolated architecture …" · `REQ-AI-012` "When AI inference confidence is below threshold (configurable, default 0.6), the system shall emit a low-confidence event and fall back to deterministic path." SRS `REQ-AI-BP-001`: 라벨과 confidence [0,1] 을 쓴다. `REQ-AI-BP-002`: NULL·`bufLen` 0 → INVALID_INPUT, 부족 → BUFFER_TOO_SMALL (이미 구현·시험됨).

### 2.2 전처리는 누가 하는가 (D6)

선례: `xpe_bone_suppress` 의 **PIXEL SCALE** 절(QA-B-173) — 호출자가 모델이 훈련된 스케일로 화소를 주고 모듈은 정규화·클램프·이동을 하지 않는다. 그러나 **부위 인식은 크기 조정이 불가피하다**(PRD 입력 512×512, 영상은 최대 4096×4096). 제안:

- **크기 조정만 모듈이 한다. 강도 정규화는 하지 않는다**(뼈 억제와 같은 규칙, 헤더에 같은 문단).
- 목표 크기는 하드코딩이 아니라 **모델 그래프가 선언한 입력 형상에서 읽는다**(`GetInputMetadata()`; `[1,1,H,W]` 또는 `[1,H,W,1]`, H·W 가 고정이어야 함. 동적이거나 다른 랭크면 `PROCESSING_FAILED` + 알림). 그러면 장난감 모델(4×4)과 실제 모델(512×512)이 같은 코드를 쓴다.
- 알고리즘: 줄일 때 **면적 평균**(박스), 늘릴 때 이중선형. 결정적이어야 한다. **크기 조정은 내부 도우미로 두고 시험이 직접 부른다** — 모델을 통해서만 보면 크기 조정 오류가 확률에 묻혀 보이지 않는다.
- 입력 형식: **FLOAT32 만**, 그 밖은 `XPE_ERR_UNSUPPORTED_FORMAT` (뼈 억제의 같은 검사를 따른다). 지금 스텁은 형식을 검사하지 않으므로, 이 검사는 스텁 빌드의 동작을 바꾸지 않도록 **ONNX 경로 안에만** 둔다(스텁의 39개 단언이 그대로여야 한다).
- 입력이 비유한 값을 담으면 뼈 억제와 같이 **검사하지 않는다**(헤더가 이미 "not even for NaN"). 출력 쪽 비유한은 §5.

### 2.3 출력 클래스와 confidence

- **클래스 목록은 모델과 함께 오는 것이다**(D1: 사이드카 `labels`). 모듈에 목록을 박지 않는다.
- **어휘가 세 곳에서 다르다(D2)**: PRD 15개(Chest, Abdomen, Spine, Pelvis, Extremity-Arm, Extremity-Leg, Hand, Foot, Skull, Facial, Shoulder, Knee, Ankle, Wrist, Cervical) · `xpe_get_param_range` 의 화이트리스트 7개(`CHEST ABDOMEN PELVIS SPINE SKULL HEAD EXTREMITY`, 대소문자 구분) · 노출 지수 표(`exposure_index.cpp`, 대소문자 무시, `CHEST HAND FOOT ABDOMEN PELVIS SPINE SKULL …`). 이 카드는 **라벨을 모델이 정한 그대로 통과**시키고(대소문자 변환 없음), 모듈이 어휘를 강제하지 않는다. 소비자가 매핑한다. 다만 `HAND`·`FOOT` 같은 라벨은 `xpe_get_param_range` 가 거부한다는 사실은 리더가 알아야 한다(이 카드 범위 밖).
- **confidence = 최대 클래스 확률**이다(SRS: "confidence score [0, 1]"). 확률 벡터는 모델이 소프트맥스를 이미 적용해 낸 것으로 본다(D3).

## 3. 문턱과 저신뢰 이벤트 (REQ-AI-012) — 결정 D3

### 3.1 지금 문서 세 곳이 다르게 말한다

| 출처 | 문턱 | 문턱 미만일 때 |
|---|---|---|
| SPEC `REQ-AI-012` (코드 기본값 `XPE_AI_DEFAULT_CONFIDENCE_THRESHOLD = 0.6f`) | 0.6, 설정 가능 | **저신뢰 이벤트를 내고** 결정론적 경로로 fallback |
| SRS `REQ-AI-FB-001/FB-002` | 0.6 | FB-001: 이벤트 + fallback. FB-002: fallback_mode 켬(기본) → `PROCESSING_FAILED`; 끔 → "AI functions attempt retries"(재시도가 무엇인지 정의 없음) |
| SDD `sdd_ai.md` §4.3 | 0.6 | 풀 빌드: `fallbackMode` 켬 → `PROCESSING_FAILED`; 끔 → "결과 + `LOW_CONFIDENCE` 플래그" — **API 에 그 플래그를 담을 곳이 없다** |
| PRD | **0.70** | `UNKNOWN` 라벨을 **반환**(오류 아님), 결과가 sidecar JSON |

### 3.2 제안

1. **문턱은 0.6**(SPEC·코드·SRS·SDD 네 곳이 일치, PRD 만 0.70 으로 오래된 문서 — 리더가 PRD 를 정렬). 비교는 **`confidence >= threshold` 이면 통과**(문턱과 정확히 같으면 저신뢰가 아니다). 이 경계는 상수 모델(0.6)로 시험한다.
2. **`fallback_mode` 켬(기본)이고 저신뢰**: 반환 `XPE_ERR_PROCESSING_FAILED`(헤더가 이미 약속), `bodyPartOut = "UNKNOWN"`, `confidenceOut` = **실제 최대 확률**(0.0 이 아니라 — 호출자가 왜 fallback 됐는지 알 수 있다), 알림 1건.
3. **`fallback_mode` 끔이고 저신뢰**: `XPE_OK` + 모델의 최상위 라벨 + 실제 confidence, 알림 1건(이벤트는 여전히 낸다). SDD 의 `LOW_CONFIDENCE 플래그`는 **API 에 없는 필드**이므로 쓰지 않고 알림이 그 역할을 한다. SRS 의 "재시도"는 구현하지 않는다(정의가 없다 — SRS 문구 정정을 리더에게 요청).
4. **알림(저신뢰 이벤트)**: `xpe_alert_push` 1건. **심각도는 결정 필요**. 근거를 둘로 갈라 적는다: 저신뢰는 장애가 아니라 정상 운영의 한 갈래(OOD 영상)이므로 호출마다 Warning 을 쌓으면 64개 알림 큐를 밀어 다른 경고를 내쫓는다(워커 경로 정책이 막으려 한 것과 같은 문제, SRS-ALERT-007). 제안은 **Info**, 문구는 문턱과 값을 담은 한 줄(교차 레인 계약 — 클라이언트가 매칭할 수 있으므로 문구 전체를 헤더에 박는다). 저신뢰는 워커·전송 장애가 아니므로 **연속 실패 카운트에 넣지 않는다**(비유한 결과와 같은 취급 — 유효한 응답이다).
5. **소프트맥스는 모듈이 하지 않는다.** 모듈이 로짓을 소프트맥스로 바꾸면 "confidence" 의 뜻이 모델 구현과 모듈에 나뉘고, 장난감 모델의 경계 시험(정확히 0.6)이 불가능해진다. 모델이 확률을 낸다는 것을 헤더에 적고, **출력이 [0,1] 밖이면 유효하지 않은 출력으로 보아 `PROCESSING_FAILED`**(비유한 출력과 같은 정책, §5).

## 4. 경로 (REQ-AI-003, REQ-AI-092) — 결정 D5

- **재사용할 것**: `OnnxSession::Run`(입력 1·출력 1·float32, 입력 길이가 모델의 고정 형상과 같아야 함 — 크기 조정 후라서 맞는다), 세션 소유·지연 생성·모델 디렉터리 변경 시 재생성(`boneSuppressSession`/`boneSuppressSessionDir` 의 쌍을 `bodyPartSession` 으로 복제), 비유한 출력 정책(`AllFinite`).
- **워커 경로는 오늘 없다**: 워커는 `INIT`/`HEARTBEAT`/`SHUTDOWN`/`BONE_SUPPRESS` 만 다루고(`ai_worker_main.cpp` 의 `case` 4개), `BODYPART_RECOGNIZE`(10) 은 프로토콜 헤더에 정의돼 있지만 워커가 **오류 응답**으로 떨어뜨린다. `WorkerSupervisor` 의 진입점도 `BoneSuppress` 하나다. 부위 인식을 워커로 보내려면 워커·브리지·감독자·`ai.cpp` 분기·시험을 `BONE_SUPPRESS` 만큼 다시 해야 한다.
- **시간 예산(092)**: 워커 경로에만 있다(QA-B-190). 기본(프로세스 안) 경로는 예산이 없다. 부위 인식도 같다.
- **제안(D5)**: **두 단계로 나눈다.** M2(프로세스 안, 기본 경로)로 실연결과 012 를 닫고, M4(워커 경로 + 092)는 별도 마일스톤으로 둔다. 이유: (a) 뼈 억제가 정확히 이 순서로 갔다(QA-B-161 → 170 → 171), (b) M2 만으로 CI 에서 모델이 답을 정한다는 것을 관측할 수 있다, (c) M4 는 프로토콜 정합·실패 카운트 공유 여부라는 별개 결정을 끌고 온다. **M4 의 결정거리**: 연속 실패 카운트를 `xpe_bone_suppress` 와 **한 세션에서 공유**할지(워커는 하나이므로 공유가 자연스럽다) 함수별로 나눌지 — SRS 해석문은 "세션당 워커 하나"를 전제로 쓰였다.

## 5. 실패 경로 한 표 (제안)

모든 행에서 `bodyPartOut`/`confidenceOut` 이 스텁과 같은 값(`"UNKNOWN"`, 해당 시 0.0)을 가지는 것이 원칙이다 — **헤더가 약속한 fallback 신호(`PROCESSING_FAILED`)는 모델 문제 어느 쪽에서도 같다.**

| 상황 | 반환 | `bodyPartOut` / `confidenceOut` | 알림 |
|---|---|---|---|
| 스텁 빌드 | `PROCESSING_FAILED` | `UNKNOWN` / 0.0 | 없음(지금과 같음, **변경 없음**) |
| ONNX 빌드, 모델 파일 없음·읽을 수 없음·라벨 사이드카 없음 또는 클래스 수 불일치 | `PROCESSING_FAILED` | `UNKNOWN` / 0.0 | **init 한 번당 첫 실패에만 Warning 1건**(호출마다가 아님: 모델이 없는 환경에서 AI 를 켠 사용자가 알 수 있되 큐를 도배하지 않게). 결정 D4 |
| 모델 입력 형상이 지원하지 않는 모양 | `PROCESSING_FAILED` | 같음 | 위와 같은 정책 |
| 출력이 비유한 | `PROCESSING_FAILED` | `UNKNOWN` / 0.0 | 기존 비유한 알림(`pushNonFiniteResultAlert`)과 같은 문구 재사용 — 문구가 이미 "this image was not AI-processed" 로 함수에 무관하다 |
| 출력이 [0,1] 밖 | `PROCESSING_FAILED` | `UNKNOWN` / 0.0 | 새 Warning 1건(문구 계약) |
| 저신뢰, fallback 켬 | `PROCESSING_FAILED` | `UNKNOWN` / **실제 값** | 저신뢰 이벤트 Info 1건 |
| 저신뢰, fallback 끔 | `OK` | 최상위 라벨 / 실제 값 | 저신뢰 이벤트 Info 1건 |
| 통과 | `OK` | 최상위 라벨 / 실제 값 | `SRS-ALERT-004` 의 "AI-processed" Info — 뼈 억제와 같은 성공 알림을 부위 인식에도 낼지는 **결정 필요**(제안: 낸다. 이 알림은 "AI 가 처리했다"는 라벨이고 호출 단위로 한 번) |
| 라벨이 `bufLen` 에 안 들어감 | `BUFFER_TOO_SMALL` | 쓰지 않는다(잘라 쓰지 않음) | 없음 |

**기존 시험과의 호환 (측정함)**: 이름에 `bodypart` 가 든 시험 16개를 지금의 `ci-ai`(ONNX) 빌드에서 돌렸다: `[  PASSED  ] 16 tests.`(`bodypart_baseline_ci_ai.txt`). 이 시험들을 담은 파일 6개는 전부 모델 디렉터리로 `"dummy_model_dir"`(없는 경로)를 쓰므로, 위 표의 "모델 없음" 행에 걸리고 그 행의 출력이 스텁과 같으면 그대로 통과해야 한다. 알림 큐를 단언하는 줄이 있는 `test_bone_suppress_worker_path.cpp` 안의 부위 인식 호출 두 곳(961·975 줄 부근)은 `INVALID_INPUT` 경로로 모델에 닿기 전에 반환하므로 "init 당 Warning 1건"이 닿지 않는다. 구현 뒤 같은 16개가 통과하는지 다시 본다.

## 6. 시험 계획

빌드: **스텁**(`ci-post`)과 **ONNX**(`ci-ai`, CI 의 `ai-onnx` 잡) 둘 다. ONNX 가 필요한 시험은 `GTEST_SKIP` 하되 `XPE_AI_EXPECT_ONNX=1` 이면 스텁에서 **빨강**(QA-B-160 의 가드).

| 영역 | 시험 | 반증(한 곳씩 약화) |
|---|---|---|
| 배선 | 상수 모델 두 벌: 같은 입력에 **다른 라벨** | 라벨을 항상 `labels[0]` 로 → 빨강 |
| 입력이 답을 정한다 | 윗부분이 밝은 영상 vs 아랫부분이 밝은 영상 → 다른 라벨 | 입력을 무시(상수) → 빨강 |
| 문턱 경계 | 상수 모델 확률 정확히 0.6 = 문턱 → 통과. **다음 float 아래**(0.59999994) → 저신뢰. 문턱을 0.0·1.0 으로 설정한 경우 | `>=` → `>` → 정확히 같은 칸만 빨강 |
| 저신뢰 | fallback 켬/끔 두 갈래의 반환·라벨·confidence·알림 정확히 1건(문구 전체)·`OK` 알림 없음 | `fallback_mode` 무시 → 한 갈래 빨강 |
| 비유한 출력 | `W` 에 inf 를 넣은 모델 → `PROCESSING_FAILED`, 출력 버퍼는 `UNKNOWN`/0.0, 비유한 알림 1건, 확률이 새지 않음 | 유한 검사 제거 → 빨강 |
| 범위 밖 출력 | 확률 1.5, −0.1 → `PROCESSING_FAILED` | 범위 검사 제거 → 빨강 |
| 모델 없음·깨짐·라벨 불일치 | 스텁과 같은 출력, init 당 첫 실패에만 알림 1건, 둘째 호출은 알림 0 | 알림을 매 호출로 → 빨강 |
| 크기 조정 | 내부 도우미를 직접 시험: 4×4→2×2 면적 평균 정확값, 같은 크기는 항등, 비정사각, 4096×4096→작게(오버플로 없음), 늘리기 | 최근접으로 교체 → 빨강 |
| 입력 형식·NULL·`bufLen` | `UINT16` → `UNSUPPORTED_FORMAT`(ONNX 경로만), 기존 `INVALID_INPUT`/`BUFFER_TOO_SMALL` 우선순위 유지 | — |
| 동시성 | 기존 `ConcurrentBodypartRecognizeIsThreadSafe` 가 실모델에서도 통과하는지 | — |
| 스텁 불변 | 스텁 빌드의 기존 단언 전부 불변 | — |

추정 시험 수: M2 ≈ 25, M3 ≈ 12, M4 ≈ 20 (건수는 계획용 추정이고 약속이 아니다).

## 7. 리더에게 묻는 결정

| # | 결정 | 제안 |
|---|---|---|
| **D1** | 모델 파일·라벨 위치 | `<modelDir>/bodypart.onnx` + 사이드카 `bodypart.json` 의 `labels`. 실제 모델 조달·서명(REQ-AI-007)은 이 카드 밖, 별건 |
| **D2** | 라벨 어휘 | 모듈은 모델이 준 라벨을 그대로 통과. PRD 15개 / 공통 화이트리스트 7개 / 노출 지수 표 사이의 불일치는 리더가 어휘 정본을 정함(별건) |
| **D3** | 저신뢰 계약 | 문턱 0.6, `>=` 통과, fallback 켬 → `PROCESSING_FAILED` + `UNKNOWN` + 실제 confidence, 끔 → `OK` + 최상위 라벨, 둘 다 이벤트 1건, 소프트맥스는 모델 몫. PRD 0.70 정렬·SRS "재시도" 문구 정정·SDD `LOW_CONFIDENCE` 플래그 삭제는 리더 문서 |
| **D4** | 모델 없음·손상 시 알림 정책 | init 한 번당 첫 실패에만 Warning, 반환·출력은 스텁과 동일. 저신뢰 이벤트 심각도 **Info**(Warning 은 큐 도배) — 둘 다 결정 필요 |
| **D5** | 워커 경로 | M2(프로세스 안) 먼저, M4(워커+092)는 분리. 연속 실패 카운트를 뼈 억제와 세션에서 공유할지는 M4 시작 전 결정 |
| **D6** | 전처리 | 크기 조정만(모델 선언 형상에서 목표 크기, 면적 평균/이중선형), 강도 정규화 없음, FLOAT32 만 |

D1·D3·D5 가 코드 모양을 바꾼다. D2·D4·D6 은 기본값을 제안했고 이견이 없으면 그대로 진행한다.

## 8. 마일스톤과 규모

| M | 내용 | 제품 파일 | 시험·자산 | 선행 |
|---|---|---|---|---|
| **M1** | 장난감 모델 생성기 확장 + 모델 디렉터리 5~6개(`Flatten`/`MatMul`/`Add`) | 0 | `make_min_models.py` + 모델·사이드카 자산, 모델 자체 검증용 독립 프로브(QA-B-160 방식) | D1 |
| **M2** | 프로세스 안 경로: 크기 조정 도우미, 세션 소유, 라벨 사이드카, 출력 검증, 반환·라벨·confidence | `ai.cpp`(+ 새 내부 헤더 1개), 필요하면 `ai_onnx_session` 의 사이드카 파싱에 `labels` 1필드 | 새 시험 파일 1(≈25건) + 기존 스텁 시험 기준선 대조 | M1, D2·D6 |
| **M3** | 문턱 소비 + 저신뢰 이벤트 + `fallback_mode` 소비(FB-002 의 절반) + 알림 문구 계약 | `ai.cpp`, `ai_api.h` 헤더 문서 | 시험 ≈12건 | M2, D3·D4 |
| **M4** | 워커 경로: `BODYPART_RECOGNIZE` 프레임, 워커·브리지·감독자·`ai.cpp` 분기, 예산(092) 적용, 실패 카운트 정책 | 4~5 | 시험 파일 2(≈20건) | M3, D5 |
| **M5** | 문서 정합(리더): api-spec §9.3, SRS `BP-001` 상태, RTM/VVP, PRD 0.70, SDD 플래그 | 0 | — | M3/M4 |

각 M 의 검증: ci-ai 풀 빌드 + 스텁 빌드 양쪽 시험, 반증(§6)을 한 곳씩, 헤더 변경이 있으면 doxygen.

## 9. 증거 (Evidence) 와 기준 (Baseline)

- `facts_check.txt`: 모델 부재(대조군 포함), 모델 파일 규약, 사이드카 JSON 선례, 워커가 다루는 요청, 감독자 진입점, 함수 본문이 세션을 안 읽음, 부위 인식 관련 시험 16개 이름, C# 소비자 0, 어휘 세 곳, 문턱 값 세 곳.
- 읽은 문서: SPEC `REQ-AI-002/003/012`, SRS `REQ-AI-BP-001/002`·`FB-001/002`, `sdd_ai.md` §4.3, `xpe-ai-prd.md` SWU-2.7, #130 의 QA-B-152·160·167 코멘트, `ai_api.h` 의 `xpe_bodypart_recognize`·`xpe_bone_suppress` 계약.
- 이 메모는 설계이고 코드·측정이 없다. 기준: 코드 변경 0줄.

## 10. 미검증 (Gaps)

- **ONNX Runtime 이 `Flatten`·`MatMul`·`Add` 로 이루어진 손으로 쓴 모델을 읽고 기대한 수를 내는지** 확인하지 않았다. QA-B-160 의 `Mul` 모델은 확인됐지만 이 연산들은 새 것이다 — M1 의 첫 독립 프로브가 그 확인이다.
- 알림 큐 단언이 있는 파일 하나(`test_bone_suppress_worker_path.cpp`)의 부위 인식 호출은 `INVALID_INPUT` 경로뿐이라는 것은 확인했다. **다른 파일의 알림 단언**은 `xpe_get_pending_alert_count`·`CountAlerts`·`xpe_clear_alerts` 를 이름으로 찾은 범위(부위 인식을 부르는 6개 파일)에서만 봤다.
- `OnnxSession::Create` 가 사이드카 JSON 에서 읽는 필드와 `labels` 를 추가할 때의 파싱 실패 처리를 코드로 읽지 않았다(사이드카를 읽는 줄이 있다는 사실만 확인).
- 크기 조정 알고리즘 두 가지(면적 평균/이중선형)의 지연 시간은 재지 않았다. PRD 의 "≤ 300 ms(3072×3072, 리사이즈 포함)"는 이 설계로 만족되는지 모른다 — 실제 모델이 없어 추론 시간은 재볼 수 없고 크기 조정 시간만 잴 수 있다.
- 워커 경로(M4)의 프로토콜 정합(`QA-B-167` 이 지적한 헤더와 워커의 열거형 번호 차이가 `BONE_SUPPRESS` 에서 어떻게 해소됐는지)을 다시 읽지 않았다. M4 시작 전 확인이 필요하다.
- GUI·clients 가 앞으로 부위 인식을 부를 때 기대할 라벨·알림 문구는 알 수 없다(현재 소비자 0).

## 11. 잔여 위험 (Residual risk)

- **"배선이 됐다"가 "부위를 맞춘다"로 읽힐 위험.** 이 카드의 모든 시험은 장난감 모델 위에 있다. 헤더·SPEC 구현 상태 절에 "정확도·임상 성능은 측정되지 않았다"를 같은 자리에 적어야 한다(QA-B-173 이 뼈 억제에 한 것처럼).
- 저신뢰 계약(D3)을 지금 정하지 않고 구현하면, 호출자가 생긴 뒤 계약을 바꾸는 것이 `UNKNOWN`·confidence 값의 의미를 바꾸는 호환성 문제가 된다. 소비자가 0 인 지금이 가장 싸다.
- 라벨 어휘(D2)가 정해지지 않으면 호출자가 라벨 문자열 매핑을 각자 만든다.
