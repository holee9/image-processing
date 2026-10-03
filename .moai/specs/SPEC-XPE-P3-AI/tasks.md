## Task Decomposition
SPEC: SPEC-XPE-P3-AI

| Task ID | Description | Requirement | Dependencies | Planned Files | Status |
|---------|-------------|-------------|--------------|---------------|--------|
| T-001 | Named pipe IPC bridge (DLL side) | REQ-AI-003 | - | ai_ipc_bridge.cpp/h | done — 브리지 자체. REQ-AI-003 은 opt-in 워커 경로(`use_worker: true`)에서만 충족, 기본 경로는 프로세스 안 추론 |
| T-002 | Worker process skeleton | REQ-AI-003,006 | T-001 | ai_worker_main.cpp | done |
| T-003 | ONNX Runtime session manager | REQ-AI-006,008 | T-002 | ai_onnx_session.cpp/h | partial — 1.20+ 충족(1.30.0 빌드), 실행 제공자는 CPU 만 등록(요청한 CUDA/TensorRT/DirectML 은 CPU 로 대체) |
| T-004 | Model versioning/metadata parsing | REQ-AI-008 | T-003 | ai_onnx_session.cpp/h | partial — 사이드카의 다섯 필드를 읽어 `ModelMetadata` 에 저장하나 `GetModelMetadata` 호출자·시험 0건, semver·필수 필드 검증 없음. `TC-MODELCARD` 시험은 `buildStubModelCard` 상수를 단언한다 |
| T-005a | Model sidecar schema + load-time validation (입력 `<이름>.json`) | REQ-AI-008 | T-004, QA-B-195 | ai_sidecar.cpp/h (모델 사이드카 검증), 스키마 문서 | pending |
| T-005b | Model card generated from the verified sidecar; `model-card.schema.json` | REQ-AI-010,011 | T-005a | ai.cpp, `schemas/model-card.schema.json`(리더 경로 — 패치 초안) | pending — 지금 카드는 상수(풀 빌드에서도 "stub", `published_date` 2026-04-22) |
| T-005c | Inference sidecar (영상마다 나가는 출력) | REQ-AI-004 | - | ai_sidecar.cpp/h (추론 사이드카), api-spec §9 | pending — 어떤 진입점도 사이드카를 내보내지 않는다. API 형태는 결정 필요(report §2.3·§5 D5) |
| T-006 | Wire xpe_bodypart_recognize | REQ-AI-002,003,012 | T-001,T-002,T-003 | ai.cpp, ai_bodypart.h, ai_bodypart_model.h, ai_bodypart_decision.h, ai_worker_main.cpp, ai_ipc_bridge.cpp, ai_worker_supervisor.cpp | partial — (기존 메모 그대로) |
| T-007 | Wire xpe_stitch_images | REQ-AI-002,003 | T-001,T-002,T-003 | ai.cpp | pending — 입력 검증만, 본문은 스텁(`XPE_ERR_PROCESSING_FAILED`) |
| T-008 | Wire xpe_bone_suppress | REQ-AI-002,050 | T-001,T-002,T-003 | ai.cpp | partial — 추론·opt-in 워커·서명·입력 검증·결정론적 fallback 있음. REQ-AI-050/051 은 실모델이 없어 검증 불가(시험 모델은 ×2 장난감), REQ-AI-052 없음 |
| T-009 | Wire xpe_dl_denoise | REQ-AI-002,020,022 | T-001,T-002,T-003 | ai.cpp | pending — 입력·메타데이터 검증만, 본문은 스텁 |
| T-010 | Model signing/verification | REQ-AI-007,091 | T-003 | ai_model_signer.cpp/h | done — ECDSA P-256 over CNG, 적재 시 검증(QA-B-195 M1~M4). **운영 서명 키 없음: 운영 신뢰 목록이 비어 모든 모델을 거부 — 실모델 입고(#243) 전 출하 불가.** 실행 파일 교체·롤백은 막지 않음(문서화) |
| T-011 | PCCP boundary enforcement | REQ-AI-110,111,112 | T-004, T-005a | ai_onnx_session.cpp/h | pending — `pccp_scope` 를 읽는 한 줄뿐. 승인된 PCCP 의 출처 없음(AI-DSF 모듈 부재), `XPE_ERR_PCCP_EXCEEDED` 는 `xpe_common`(Lane A) 에 새 코드 필요 |
| T-012 | Input validation hardening | REQ-AI-090 | - | ai.cpp | done — QA-B-194 설계 해석대로: 크기 경계, 화소값 경계 = float 유한성(정수형은 형식이 경계), 메타데이터 검사는 `xpe_dl_denoise` 의 `XpeImageMetadata` |
| T-013 | Time budget enforcement | REQ-AI-092 | T-001 | ai_ipc_bridge.cpp/h | partial — 뼈 억제·부위 인식의 opt-in 워커 경로만. 기본 경로와 `xpe_stitch_images`·`xpe_dl_denoise` 에는 없음 |
| T-014 | XAI sidecar generation | REQ-AI-070,071,072,073 | T-003, T-005c | ai_xai.cpp/h | pending — 껍데기(옵트인·필드·고정 문구)는 모델 없이 시험 가능, 설명(Grad-CAM/SHAP)은 모델이 중간 텐서를 내놓는 규약이 있어야 의미 |
| T-015 | Conformal prediction framework | REQ-AI-080,081,082,083 | T-005c | ai_conformal.cpp/h | pending — 수식·커버리지 보장은 합성 점수로 통계 시험 가능(교환가능성 깨진 입력 포함), 보정 집합·실제 α 주장은 실데이터 필요 |
| T-016 | Drift fingerprint emission | REQ-AI-100,101 | T-005c | ai_sidecar.cpp/h | pending — REQ-AI-100 지문은 모델 없이 가능(형식은 REQ-OPS-020), REQ-AI-101 은 드리프트 경보를 받는 API 가 없어 새 API·OPS 쪽 정의가 먼저 |

> **T-008·T-013 상태 메모 (2026-10-02, `#210`, `QA-B-190`)** — 두 행은 `pending` 으로 두었습니다.
> 일부만 끝났고 `done` 이라고 적을 근거는 없기 때문입니다.
> - T-008: `xpe_bone_suppress` 는 `OnnxSession::Run` 으로 추론 경로가 이어졌고 opt-in 워커 경로도 있습니다.
>   다만 이 행이 함께 걸고 있는 `REQ-AI-050`(품질 목표)은 `rtm_ai.md` §12 에서 여전히 `Not implemented` 입니다.
> - T-013: 시간 예산은 `xpe_bone_suppress` 의 opt-in 워커 경로(`use_worker: true`)에서만 동작합니다
>   (예산 초과 → 입력 반환 + 비정상 코드 + Warning 1건). 기본 경로(워커 끔)와 나머지 세 진입점에는 예산이 없습니다.
>   (정정 2026-10-02, `QA-B-191` M4c: `xpe_bodypart_recognize` 의 opt-in 워커 경로에도 같은 예산이 있고 워커·실패 카운트를
>   뼈 억제와 공유합니다. 기본 경로와 `xpe_stitch_images`·`xpe_dl_denoise` 에는 여전히 없습니다.)
>
> **T-006 상태 메모 (2026-10-02, `#130`, `QA-B-191`)** — `done` 이 아니라 `partial` 입니다.
> - REQ-AI-012(저신뢰 이벤트)·REQ-AI-002(결정론적 fallback)는 부위 인식에 대해 코드와 시험이 있습니다(`BodyPart.*`, `BodyPartDecision.*`).
> - REQ-AI-003(워커 격리)은 opt-in 워커 경로(`use_worker: true`)에서만 충족되고, 기본값은 워커 끔이라 기본 경로는 격리되지 않습니다.
> - 시험 모델은 손으로 만든 장난감 모델(`modules/ai/tests/data/make_bodypart_models.py`)이라 실제 부위 인식 모델·정확도·지연은 측정되지 않았습니다.


> **QA-B-196 실태 대조 (2026-10-03, `#130`)** — 표를 코드·시험에 맞췄습니다(근거 `.moai/reports/lane-post/QA-B-196/`, post 레인 `be746353`).
> - T-003·T-004 는 `done` → `partial`: EP 는 CPU 만 등록, 메타데이터는 읽지만 소비처가 없고 모델 카드는 상수였습니다(T-005b 에서 바로잡음).
> - T-008·T-013 은 `partial`, T-010·T-012 는 `done`(T-010 은 운영 서명 키 `#243` 전까지 출하 불가).
> - T-005 를 a(모델 사이드카 입력 검증)·b(검증된 사이드카로 만드는 모델 카드 + 스키마)·c(추론 사이드카 출력)로 나눴습니다. c 는 소비자가 생길 때까지 보류.
> - 어떤 작업에도 걸리지 않은 REQ-AI 20개: 010·011 은 T-005b, 051·052 는 T-008, 021·023·024 는 T-009 가 흡수합니다.
>   093(최소 권한)은 신규 작업 후보, 030~033·040~042·060~062 는 대상 기능이 아직 없어 범위 밖 표시가 필요합니다(`#130` 기록).

**Total**: 18 tasks (T-005 를 a·b·c 로 분할)
**Priority**: Alternative B (Balanced) - Infrastructure + Core Inference Paths
**Complexity**: 4 High, 6 Medium, 3 Low, 3 Medium
