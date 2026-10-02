## Task Decomposition
SPEC: SPEC-XPE-P3-AI

| Task ID | Description | Requirement | Dependencies | Planned Files | Status |
|---------|-------------|-------------|--------------|---------------|--------|
| T-001 | Named pipe IPC bridge (DLL side) | REQ-AI-003 | - | ai_ipc_bridge.cpp/h | done |
| T-002 | Worker process skeleton | REQ-AI-003,006 | T-001 | ai_worker_main.cpp | done |
| T-003 | ONNX Runtime session manager | REQ-AI-006,008 | T-002 | ai_onnx_session.cpp/h | done |
| T-004 | Model versioning/metadata parsing | REQ-AI-008 | T-003 | ai_onnx_session.cpp/h | done |
| T-005 | Sidecar metadata schema | REQ-AI-004 | - | ai_sidecar.cpp/h, model-card.schema.json | pending |
| T-006 | Wire xpe_bodypart_recognize | REQ-AI-002,003,012 | T-001,T-002,T-003 | ai.cpp, ai_bodypart.h, ai_bodypart_model.h, ai_bodypart_decision.h, ai_worker_main.cpp, ai_ipc_bridge.cpp, ai_worker_supervisor.cpp | partial — 배선 완료(장난감 모델로 시험), REQ-AI-003 은 opt-in 워커 경로만(기본 경로 미격리), 실제 모델·정확도 없음 |
| T-007 | Wire xpe_stitch_images | REQ-AI-002,003 | T-001,T-002,T-003 | ai.cpp | pending |
| T-008 | Wire xpe_bone_suppress | REQ-AI-002,050 | T-001,T-002,T-003 | ai.cpp | pending |
| T-009 | Wire xpe_dl_denoise | REQ-AI-002,020,022 | T-001,T-002,T-003 | ai.cpp | pending |
| T-010 | Model signing/verification | REQ-AI-007,091 | T-003 | ai_model_signer.cpp/h | pending |
| T-011 | PCCP boundary enforcement | REQ-AI-110,111,112 | T-004 | ai_onnx_session.cpp/h | pending |
| T-012 | Input validation hardening | REQ-AI-090 | - | ai.cpp | pending |
| T-013 | Time budget enforcement | REQ-AI-092 | T-001 | ai_ipc_bridge.cpp/h | pending |
| T-014 | XAI sidecar generation | REQ-AI-070,071,072,073 | T-003,T-005 | ai_xai.cpp/h | pending |
| T-015 | Conformal prediction framework | REQ-AI-080,081,082,083 | T-005 | ai_conformal.cpp/h | pending |
| T-016 | Drift fingerprint emission | REQ-AI-100,101 | T-005 | ai_sidecar.cpp/h | pending |

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

**Total**: 16 tasks
**Priority**: Alternative B (Balanced) - Infrastructure + Core Inference Paths
**Complexity**: 4 High, 6 Medium, 3 Low, 3 Medium
