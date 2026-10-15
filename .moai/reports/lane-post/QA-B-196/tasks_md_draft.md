# tasks.md 수정안 (초안) — SPEC-XPE-P3-AI

`.moai/specs/` 는 리더 소유라 이 파일은 **초안**이다. 근거는 같은 디렉터리의 `report.md` §1 과 `evidence_greps.txt`. 트리: `dev/postprocess` 7304a6f0.

## 표

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

## 작업이 없는 REQ (제안)

정의된 REQ-AI 46개 중 20개가 어떤 작업에도 없다(`req_coverage_out.txt`). 리더 결정 필요(report §5 D2):

| REQ | 제안 |
|---|---|
| 010, 011 | T-005b 로 흡수(위 표) |
| 093 (최소 권한) | 새 작업 T-017 — 워커는 kill-on-close 작업 객체뿐(`ai_worker_supervisor.cpp:201~205`), 네트워크·쓰기 제한 없음(코드 읽기) |
| 001, 005 | 구현됨(계층 구조, `xpe_ai_init` opt-in) — 작업 없이 시험으로 닫혀 있는지 대조 필요 |
| 021, 023, 024 | T-009 의 REQ 칸에 추가(디노이즈) |
| 030~033, 040~042, 060~062 | 대상 기능(스티칭 알고리즘·조리개 검출 등)이 아직 없음 — 범위 밖 표시 또는 작업 신설 |
| 051, 052 | T-008 의 REQ 칸에 추가(실모델 의존) |

## 메모 문안 (추가 제안)

> **T-003·T-004 상태 메모 (QA-B-196)** — 두 행은 `done` 에서 `partial` 로 바꿉니다.
> - T-003: REQ-AI-006 의 "EP 선택 가능"은 목록 조회만 되고 CPU 외 제공자는 등록하지 않습니다.
> - T-004: 사이드카의 메타데이터를 읽어 저장하지만 소비처가 없고, `xpe_ai_get_model_card` 는 모델과 무관한 상수 카드를 돌려줍니다. `TC-MODELCARD` 시험은 그 상수를 단언합니다.
