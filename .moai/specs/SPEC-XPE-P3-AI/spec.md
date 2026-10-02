# SPEC-XPE-P3-AI: AI Module v1.0 (Self-Supervised + Diffusion + XAI + Conformal UQ)

---
id: SPEC-XPE-P3-AI
version: 1.1.0
status: Draft
created: 2026-04-17
updated: 2026-04-17
author: MoAI (manager-spec orchestration)
priority: Should (전체). Phase 3 AI-DSF 배포 승인 시 baseline governance 부분만 조건부 Must 승격
issue_number: null
iec62304_class: B
development_mode: TDD
sprint: S3 (Phase 3 AI, 조건부 진입)
dependency: SPEC-XPE-MASTER v3.0.0, SPEC-XPE-REG v1.1, SPEC-XPE-SEC v1.1, SPEC-XPE-OPS v1.1, SPEC-XPE-P2-ADV v1.0, S2-A 완료
---

## Priority Reclassification Notice (v1.1)

본 SPEC 전체는 trend-survey-2026.md v1.1에서 **Should**로 재분류:

- **강등 근거**: AI 모듈(xpe_ai.dll)은 Phase 3 기능. 결정적 전용 Phase 1/2 릴리스는 본 SPEC 비해당 → 출시 블로커 아님
- **조건부 Must 승격**: Phase 3 AI-DSF 배포를 공식 결정하고 FDA/EU 제출 계획이 확정되면 다음 항목은 Must로 승격:
  - REQ-AI-010~011 (Model Card) — FDA Transparency 대응 (정정 2026-10-02 `#210`: 예전 표기 `010~012` 의 `REQ-AI-012` 는 모델 카드가 아니라 저신뢰 이벤트 요구입니다)
  - REQ-AI-024 (Data Lineage) — GMLP Principle #3 (정정 2026-10-02 `#210`: 예전 표기 `REQ-AI-013` 은 이 SPEC 에 정의가 없습니다. 데이터 계보를 다루는 정의된 요구는 `REQ-AI-024` 입니다)
  - REQ-AI-002 (Deterministic Fallback) — 의료기기 안전 기본
  - REQ-AI-110~112 (PCCP Boundary) — FDA PCCP 대응
- **현재 실행 범위**: Phase 2 완료 후 Phase 3 진입 결정 시 재평가

## HISTORY

| Version | Date       | Author       | Changes                                   |
|---------|------------|--------------|-------------------------------------------|
| 1.0.0   | 2026-04-17 | MoAI         | Initial SPEC replacing legacy S3-AI draft. SSL denoising + Diffusion priors + XAI sidecar + Conformal UQ + PCCP linkage |

---

## 1. Scope

### 1.1 Overview

본 SPEC은 XPE Phase 3 AI 모듈(xpe_ai.dll)의 구현 명세이다. SPEC-XPE-P2-ADV에서 확립된 deterministic premium tier 위에 AI 계층을 assistive 역할로 추가한다. 모든 AI 기능은 **deterministic fallback 필수**, **opt-in 기본 비활성**, **sidecar 메타데이터 전달**, **PCCP 범위 내 변경만 허용** 원칙을 준수한다.

### 1.2 In Scope — Baseline AI (Must)

- **AI-B-01**: Model Card API (REQ-REG-020~023 구현)
- **AI-B-02**: Data Lineage recorder
- **AI-B-03**: Deterministic fallback router (REQ-REG-AI-03 구현)
- **AI-B-04**: ONNX Runtime 1.20+ integration with multi-EP (CPU, CUDA, TensorRT, DirectML)
- **AI-B-05**: Model versioning + PCCP boundary enforcement
- **AI-B-06**: Input validation + adversarial robustness basic

### 1.3 In Scope — Differentiator AI (Should)

- **AI-D-01**: Self-Supervised Denoising (Noise2Noise / Noise2Self / Neighbor2Neighbor family) — POST-02
- **AI-D-02**: Diffusion Priors for Low-Dose Enhancement (NEED / DiffDenoise / SAD) — POST-02 premium
- **AI-D-03**: ML Defect Correction (PRE-06 ML) with ViT AE
- **AI-D-04**: Bone Suppression U-Net (POST-09)
- **AI-D-05**: AI Collimation Detection (POST-07 AI variant)
- **AI-D-06**: XAI Sidecar (Grad-CAM saliency, SHAP feature attribution)
- **AI-D-07**: Conformal Prediction Uncertainty Quantification

### 1.4 Exclusions

- Foundation model fine-tuning (MedSAM, RadImageGAN) → Could tier / Phase 4
- Federated learning → Could / Post-market
- Generative data augmentation → Could / research mode
- Continuous online learning → regulatory boundary risk, excluded
- LLM-based report generation → out of scope
- Non-radiographic AI (CT, MRI specific) → out of scope

---

## 2. Referenced Documents

| Document ID | Title | Role |
|-------------|-------|------|
| SPEC-XPE-REG | Regulatory Master | Governance normative |
| SPEC-XPE-SEC | Cybersecurity Master | Security normative |
| SPEC-XPE-OPS | Operations Master | Drift normative |
| SPEC-XPE-IOP | Interoperability Master | AI SR encoding |
| SPEC-XPE-P2-ADV | Advanced Post-Processing | Upstream pipeline |
| SPEC-XPE-MASTER | XPE Master v3.0.0 | Upstream |
| Noise2Sim (PMC) | Similarity-based SSL denoising | Reference |
| Noise2Detail (MICCAI 2025) | Multistage N2N | Reference |
| NEED (2025) | Noise-inspired Diffusion | Reference |
| SAD (2024) | Structure-Aware Diffusion | Reference |
| DiffDenoise (2025) | Conditional Diffusion denoising | Reference |
| MedSAM2 (2025-04) | Segment Anything Medical | Reference (informative) |
| Conformal Prediction Medical AI | Nature Digital Med 2024 | Reference |

---

## 3. Definitions

| Term | Definition |
|------|-----------|
| SSL | Self-Supervised Learning |
| N2N | Noise2Noise |
| N2S | Noise2Self |
| N2V | Noise2Void |
| ViT | Vision Transformer |
| AE | Autoencoder |
| CP | Conformal Prediction |
| XAI | Explainable AI |
| SHAP | Shapley Additive Explanations |
| Grad-CAM | Gradient-weighted Class Activation Mapping |
| Sidecar | Auxiliary metadata alongside image |
| PCCP Boundary | Allowed retraining scope without new FDA submission |
| Execution Provider (EP) | ONNX Runtime backend (CPU, CUDA, TensorRT, DirectML) |
| Worker Isolation | AI inference runs in isolated process |
| Assistive AI | AI augments deterministic path, never replaces |

---

## Implementation Status — **이 SPEC 의 요구 46건은 대부분 미구현입니다** (신설 2026-09-28, `#210`/`#130`)

> **먼저 읽으십시오.** 아래 `## 4. Requirements` 의 46개 요구는 **무엇을 만들 것인가**를 적은 것이지, 무엇이 만들어졌는지가 아닙니다. 이 SPEC 에는 요구별 `Status` 필드가 **0건**이어서 *"요구되고 구현됨"* 과 *"요구되고 미착수"* 를 구별할 수단이 없었습니다. 이 절이 그 구별입니다.

### 측정된 현재 상태 (2026-09-28)

`modules/ai` 는 **스텁 빌드**입니다. 기본 옵션이 `XPE_AI_USE_ONNXRUNTIME=OFF`·`XPE_AI_STUB_BUILD=ON` 이고, **풀 빌드 가지도 스텁입니다**(`ai_onnx_session.cpp` — *"For now, use stub implementation even in full build"*).

| 검색 | `modules/` 전체 |
|---|---|
| `Ort::` (ONNX C++ API) | **0** |
| `onnxruntime_c_api` · `OrtApi` · `OrtSession` · `OrtGetApiBase` · `OrtEnv` (C API) | **0** |
| **대조군** `spdlog` (같은 파일) | 4 |
| **대조군** `nlohmann` (같은 파일) | 2 |
| **대조군** `XpeErrorCode` (`modules/` 전체) | 841 |

~~추론 경로가 **존재하지 않습니다.** ONNX Runtime 을 조달해 링크해도 그것을 호출하는 코드가 없습니다.~~

> **재측정 2026-10-02 (`QA-B-190`, `#210`)** — 위 표와 문장은 2026-09-28 의 상태이고 지금은 낡았습니다.
> 추론 경로는 **`xpe_bone_suppress` 에만 있습니다**(`OnnxSession::Run`, opt-in 워커 경로 포함).
> `xpe_bodypart_recognize`·`xpe_stitch_images`·`xpe_dl_denoise` 는 여전히 추론이 없는 스텁입니다.
> ONNX 빌드는 `ci-ai` 프리셋(`XPE_AI_USE_ONNXRUNTIME=ON`, `XPE_AI_STUB_BUILD=OFF`)이고, 등록하는 EP 는 CPU 뿐입니다.
> 아래 "이 절을 고칠 조건" 이 요구한 재측정이 이것입니다(`QA-B-190` 보고서 §1.2·§3).

> **재측정 2026-10-02 (`QA-B-191`, `#130`)** — 추론 경로는 이제 `xpe_bone_suppress` 와 `xpe_bodypart_recognize` 에 있습니다.
> `xpe_stitch_images`·`xpe_dl_denoise` 는 여전히 스텁입니다. `xpe_bodypart_recognize` 는 `{modelDir}/bodypart.onnx` 와
> 라벨 사이드카 `bodypart.json` 을 읽고, 시험은 손으로 만든 장난감 모델(분류기가 아님)로 **배선**만 증명합니다.
> 실제 부위 인식 모델·정확도·지연·적재 시간은 이 저장소에 없고 측정되지 않았습니다(`SRS-AI-010` 의 "CNN classifier" 는 충족되지 않았습니다).
> 근거: `modules/ai/src/ai.cpp` `xpe_bodypart_recognize_impl`, 시험 `BodyPart.*`(`test_bodypart_inference.cpp`),
> 모델 생성기 `modules/ai/tests/data/make_bodypart_models.py`.
>
> - **`REQ-AI-003`**(워커 격리): 부위 인식도 opt-in 워커 경로를 가집니다. 두 경로(프로세스 안·워커)가 같은 모델 적재(`ai_bodypart_model.h`)와
>   같은 판정(`ai_bodypart_decision.h`)을 쓰며, 모델 디렉터리 14개(모델이 없는 디렉터리 1개 포함) × 영상 6장에서 같은 답을 냅니다:
>   `WorkerBodyPartAgreement.TheRealWorkerAndTheInProcessPathSayTheSameThingForEveryModelAndImage`.
>   **기본값은 워커 끔**이므로 기본 경로의 격리는 여전히 충족되지 않습니다(아래 `REQ-AI-092` 와 같은 한계).
> - **`REQ-AI-002`**(결정론적 fallback): 부위 인식이 쓸 수 있는 답을 못 줄 때의 결과는 한 가지입니다 — `XPE_ERR_PROCESSING_FAILED`,
>   라벨 `"UNKNOWN"`, confidence 0.0(저신뢰일 때는 측정된 confidence). 원인 목록은 `ai_api.h` `xpe_bodypart_recognize` 의
>   "WHEN THERE IS NO USABLE ANSWER" 절. 시험: `BodyPart.NoModelFileIsTheStubsOutcomeWithOneWarning` 외 `BodyPart.*` 의
>   "…TheStubsOutcomeWithOneWarning" 군.

### 시험 156건이 보장하는 것

`QA-B-154` 가 셌습니다:

| 갈래 | 건수 |
|---|---|
| 스텁 계약을 단언 (전부 `REQ-AI-002` 결정론적 fallback) | **19** (12%) |
| 스텁이라 검증 불가 | 0 |
| **AI 와 무관** — null 인자·버퍼 길이·오류 우선순위·프로토콜 상수·모델카드 JSON·기하 헬퍼 | **137** (88%) |

**"AI 시험 156건 초록" 은 AI 에 대해 거의 아무것도 말하지 않습니다.** `#205` 이후 이 시험들은 CI 에서 실제로 돌지만(`ci-post` `BUILD_AI=ON`), 도는 것과 보장하는 것은 다릅니다.

### 개별 확인된 세 건 (`#210`)

| 요구 | 실재 |
|---|---|
| **`REQ-AI-006`** ONNX 1.20+ · multi-EP 선택 가능 | EP 목록이 **하드코딩**입니다 — 조회가 아닙니다. **그 기계에 없는 EP 를 있다고 답할 수 있습니다.** 그리고 런타임 호출 자체가 0건 |
| **`REQ-AI-061`** 신뢰도 문턱 미만 → Hough fallback | **미구현 — 두 쪽 다 없습니다**(정정 2026-10-02, `QA-B-190`). 061 은 AI 조리개 검출(`REQ-AI-060`)의 fallback 인데, `modules/ai` 에 AI 조리개 검출 함수가 없어(`ai_api.h`·`ai.cpp` 에 `collimation` 0건) 061 은 도달할 대상이 없습니다. Hough 기준선은 저장소에 있으나 `modules/enhance_advanced` 의 POST-07(`xpe_detect_collimation`)이고 `modules/ai` 에서는 단어 단위 `Hough` 0건입니다. `confidence_threshold` 와 confidence 를 `0.0` 으로 덮는 줄은 **061 이 아니라 `REQ-AI-012`/`REQ-AI-002`(저신뢰 이벤트 → 결정론적 fallback)의 자리**이고, 그 요구는 맞으나 코드가 미완입니다 — `AiModuleState::confidenceThreshold` 는 `xpe_ai_init` 이 대입만 하고 읽는 곳이 없으며, `xpe_bodypart_recognize` 는 추론 없이 `if (confidenceOut) *confidenceOut = 0.0f;` 로 덮고 실패를 반환합니다. `ConfidenceThreshold*` 두 시험은 **헤더 상수값만** 확인합니다. (정정 2026-10-02, `QA-B-191`: `confidenceThreshold` 는 이제 `xpe_bodypart_recognize` 가 읽습니다(`decideBodyPart`). `REQ-AI-012` 는 부위 인식에 대해 구현됨 — 문턱 미만이면 저신뢰 이벤트 Warning 1건, `fallback_mode` 켬(기본)이면 `UNKNOWN` 과 측정된 confidence 로 `XPE_ERR_PROCESSING_FAILED`, 끔이면 `XPE_OK` 로 라벨을 돌려줍니다. 다른 세 진입점(`xpe_stitch_images`·`xpe_dl_denoise`·`xpe_bone_suppress`)에는 문턱이 없습니다. `ConfidenceThreshold*` 두 시험은 여전히 헤더 상수값만 확인합니다. 시험: `BodyPart.AConfidenceExactlyAtTheThresholdPasses`, `BodyPart.AConfidenceOneFloatBelowTheThresholdIsLowAndFallsBack`, `BodyPart.WithFallbackModeOffTheLowConfidenceLabelIsReturnedWithAWarning`, `BodyPart.FallbackModeCanBeToggledAtRunTimeAndTheNextCallFollows`, `BodyPart.TheThresholdIsTheConfiguredOneNotAConstant`, `BodyPartDecision.TheLowConfidenceTextsAreTheContractedOnes`) |
| **`REQ-AI-092`** 시간 예산(기본 5s) → fallback + 알림 | **일부 구현**(정정 2026-10-02, `QA-B-171`·`QA-B-181`, 확인 `QA-B-190`). `xpe_bone_suppress` 의 **opt-in 워커 경로**(`use_worker: true`, 기본값은 끔)에서 예산을 넘으면 입력을 그대로 반환하고 비정상 코드를 돌려주며 Warning 알림을 정확히 1건 냅니다. 시험: `IpcDeadline.*`, `WorkerPathFixture.ASilentWorkerIsReportedTheInputIsReturnedAndTheNextCallRecovers`, `WorkerSupervisor.AStalledWorkerFailsThatCallIsKilledAndTheNextCallStartsAFreshOne`. **기본 경로(워커 끔)와 `xpe_stitch_images`·`xpe_dl_denoise` 에는 예산이 없습니다.** `xpe_bodypart_recognize` 의 opt-in 워커 경로(`use_worker: true`)에는 같은 예산이 있고 뼈 억제와 워커·실패 카운트를 공유합니다(정정 2026-10-02, `QA-B-191` M4c). 시험: `BodyPartWorkerPath.ASilentWorkerIsGivenUpOnAtTheBudgetAndTheNextCallRecoversOnANewWorker`. 예전 `REQ-AI-009` 인용은 아래 처리 완료 메모 참조 |

### 판정

**요구가 틀린 것이 아니라 아직 안 만든 것입니다.** 세 요구 모두 Phase 3 의 정당한 목표이고, 문구를 고칠 이유가 없습니다.

결함은 **그 사실이 어디에도 기록되지 않아** 시험 이름과 요구 목록이 구현된 것처럼 읽힌 것입니다. `#207` 에서 `AC-SIMD-003` 이 없는 AVX2 를 보증하던 것과 같은 형태이고, 그때와 같은 방식으로 **실재를 적어 해소**합니다.

~~**`REQ-AI-009` 인용만 실제 오류입니다** — 존재하지 않는 번호이므로 `REQ-AI-092` 로 고쳐야 합니다. 코드 주석이라 `modules/ai` 소유이고 별도 카드로 냅니다.~~

> **처리 완료 2026-09-30 (`#210`)** — 두 곳이었고 둘 다 닫혔습니다.
>
> | 자리 | 처리 |
> |---|---|
> | `modules/ai` 코드 주석·시험 | `QA-B-157` 이 정정 — `REQ-AI-009` 리터럴 **0건** |
> | `docs/project/vvp_ai.md` 4곳 · `rtm_ai.md` 2곳 | 리더가 정정(`#210`), 각 문서에 옛 번호와 사유를 주석으로 남김 |
>
> **문서 쪽은 이 줄이 예상하지 못한 자리입니다.** 이 줄은 오류를 *코드 주석*으로만
> 보았고 실제로는 문서 여섯 곳에도 같은 번호가 있었습니다 —
> 하나를 고치면 다른 인용도 같이 봐야 한다는 것을 이 줄 자신이 적어 두었는데
> (*"하나가 개명됐다면 다른 인용도 같이 끊겨 있을 수 있습니다"*, `#210` 본문),
> 범위는 코드로 좁혀 적혀 있었습니다.
>
> **정정 2026-10-02 (`QA-B-190`)** — 이 메모가 처음에 적은 "개명이 코드에서만 반영되고
> 문서에서 끊긴 형태" 라는 설명은 이력이 뒷받침하지 않습니다. `REQ-AI-009` 는 정의된 적이
> 한 번도 없는 번호이고(정의 줄 패턴으로 찾는 `git log -G` 0 커밋, 대조군으로 같은 패턴이
> `092` 는 찾음), 저장소에 처음 나타난 곳은 대량 커밋 `dd7c8e05`(2026-04-28)입니다 — 그
> 커밋이 RTM 행과 코드 세 곳의 주석에 같은 오번호를 **한꺼번에** 넣었습니다. 개명이 아니라,
> 정의된 적이 없는 번호가 한 대량 커밋에서 코드와 문서에 함께 들어간 것입니다.
>
> 정의 존재 대조군: 같은 정의 패턴이 `REQ-AI-092` 를 `srs_ai.md` 의
> `#### REQ-AI-092: Time Budget Enforcement` 와 이 SPEC 의 `**REQ-AI-092** (Ubiquitous):`
> 두 곳에서 찾고 `REQ-AI-009` 는 **0건**입니다. (예전 줄번호 인용 `spec.md:272` 는 이 문서가
> 늘어나면서 밀렸으므로 이름으로 인용합니다.)
>
> **`REQ-AI-061` 의 미구현은 그대로 열려 있습니다.** `REQ-AI-092` 는 그 뒤 일부 구현됐습니다
> (워커 경로의 `xpe_bone_suppress` 만, 위 표 참조) — 번호 정리와는 별개로 생긴 것이고,
> 기본 경로와 나머지 진입점은 여전히 열려 있습니다.

### 이 절을 고칠 조건

`#130`(추론 경로 구현)이 진행되면 위 표가 낡습니다. **표의 수치를 다시 재지 않고 이 절을 지우지 마십시오** — 지우는 쪽이 더 나쁩니다. `tools/docs/api_requirement_census.py` 와 같은 형태의 재측정이 먼저입니다.

### 미검증

- 나머지 43개 요구는 **건별로 보지 않았습니다.** 위 세 건은 `#210` 이 지목한 것이고, 스텁 상태로 미루어 대부분 미구현이겠으나 **그것은 추정입니다**
- `ai_onnx_session.cpp` 의 `#if ONNX_RUNTIME_STUB_BUILD` 가 CMake 옵션과 올바로 이어진다는 것은 확인했습니다(헤더 `:24-28`) — 양쪽 가지가 둘 다 스텁이라 **시험으로는 구별할 수 없습니다**


## 4. Requirements

### 4.1 Architecture Principles (Must)

**REQ-AI-001** (Ubiquitous): The xpe_ai.dll shall be Layer 1 (depends only on xpe_common.dll); no lateral Layer 1 dependencies.

**REQ-AI-002** (Ubiquitous): All AI inference shall have a deterministic fallback path that maintains clinical usability when AI is disabled, fails, or confidence is below threshold.

**REQ-AI-003** (Ubiquitous): AI modules shall run in worker-isolated architecture: inference in separate process, IPC via shared memory or pipe, main process crash-immune.

**REQ-AI-004** (Ubiquitous): AI metadata SHALL be delivered via sidecar (not mutating XpeImageMetadata) per SPEC-XPE-MASTER v3.0 Sidecar Contract.

**REQ-AI-005** (Ubiquitous): AI execution shall be opt-in per pipeline configuration; default off until explicit activation.

**REQ-AI-006** (Ubiquitous): ONNX Runtime 1.20+ SHALL be the model runtime. Execution Provider selection shall be configurable: CPU (always), CUDA (x86 GPU), TensorRT (NVIDIA), DirectML (Windows GPU).

**REQ-AI-007** (Ubiquitous): Model files (.onnx) SHALL be signed (Ed25519 or ECDSA P-256) and verified at load time.

**REQ-AI-008** (Ubiquitous): Model versioning shall follow semver; model metadata shall include: model_id, version, pccp_scope, training_data_hash, validation_metrics.

### 4.2 Model Card & Transparency (Must · cross-ref REG)

**REQ-AI-010** (Ubiquitous): For each AI model, `xpe_ai_get_model_card(model_id, buf, buf_size)` shall return JSON with: intended_use, training_data_summary, demographic_performance, limitations, model_version, pccp_status, published_date.

**REQ-AI-011** (Ubiquitous): Model card JSON shall conform to schema `schemas/model-card.schema.json`.

**REQ-AI-012** (Event-driven): When AI inference confidence is below threshold (configurable, default 0.6), the system shall emit a low-confidence event and fall back to deterministic path.

### 4.3 Self-Supervised Denoising (Should · POST-02)

**REQ-AI-020** (Ubiquitous): The POST-02 AI tier shall implement a self-supervised denoising model trainable without clean reference images.

**REQ-AI-021** (Ubiquitous): Supported SSL training strategies: Noise2Noise (N2N), Noise2Self (N2S), Neighbor2Neighbor, Noise2Sim (similarity-based).

**REQ-AI-022** (Ubiquitous): SSL model inference latency target: ≤ 500 ms per 3000x3000 image on CPU, ≤ 100 ms on GPU (TensorRT).

**REQ-AI-023** (Ubiquitous): SSL model quality shall be benchmarked against deterministic MFP (SPEC-XPE-P2-ADV §4.1) on matched phantom sets; pass criteria PSNR ≥ matched, SSIM ≥ matched, FROC AUC ≥ 0.95 of matched.

**REQ-AI-024** (Ubiquitous): SSL training data shall include phantom and anonymized clinical images per site agreement; data lineage recorded per REQ-REG-013.

### 4.4 Diffusion Priors Enhancement (Should · POST-02 premium)

**REQ-AI-030** (Ubiquitous): Optional diffusion-prior enhancement tier SHALL be provided as opt-in premium path.

**REQ-AI-031** (Ubiquitous): Supported strategies: NEED (Noise-Inspired Diffusion), SAD (Structure-Aware Diffusion), DiffDenoise (Conditional Diffusion).

**REQ-AI-032** (Ubiquitous): Diffusion tier latency target: ≤ 5 s per image on GPU (configurable DDIM steps).

**REQ-AI-033** (Ubiquitous): Diffusion tier shall document perceptual benefit over SSL baseline via user study or radiologist reader study (reference: Medical Physics 2025 methodology).

### 4.5 ML Defect Correction — PRE-06 (Should)

**REQ-AI-040** (Ubiquitous): ML defect correction shall use ViT Autoencoder-based model per existing Panel Defect PRD.

**REQ-AI-041** (Ubiquitous): ML tier quality target: NMSE ≥ 14x improvement over basic interpolation (existing Panel Defect PRD target).

**REQ-AI-042** (Ubiquitous): ML defect correction shall emit class-aware routing decision (defect_type: hot, dead, cluster) as sidecar metadata.

### 4.6 Bone Suppression — POST-09 (Should)

**REQ-AI-050** (Ubiquitous): Bone Suppression shall use U-Net architecture trained on DES (Dual Energy Subtraction) paired data.

**REQ-AI-051** (Ubiquitous): Bone Suppression quality target: pulmonary nodule sensitivity +16.8% over non-suppressed (Phase 2 brainstorming target).

**REQ-AI-052** (Ubiquitous): Bone Suppression shall emit IHE AIR-compatible DICOM SR (cross-ref SPEC-XPE-IOP §4.4.4).

### 4.7 AI Collimation Detection — POST-07 AI variant (Should)

**REQ-AI-060** (Ubiquitous): AI collimation detection shall augment POST-07 baseline Hough-based detection.

**REQ-AI-061** (Ubiquitous): AI confidence below threshold shall trigger fallback to Hough deterministic path.

**REQ-AI-062** (Ubiquitous): AI-refined ROI shall be delivered via sidecar JSON (cross-ref SPEC-XPE-MASTER v3.0).

### 4.8 Explainable AI Sidecar (Should · cross-ref REG)

**REQ-AI-070** (Ubiquitous): XAI sidecar generation shall be opt-in per inference.

**REQ-AI-071** (Ubiquitous): Supported XAI methods: Grad-CAM (gradient-based saliency), SHAP (feature attribution for tabular-features AI).

**REQ-AI-072** (Ubiquitous): XAI sidecar JSON shall include: method, model_id, version, saliency_map_reference (DICOM UID or file path), confidence, disclaimer text.

**REQ-AI-073** (Ubiquitous): XAI disclaimer shall warn: "Post-hoc explanations may not reflect actual decision process; use as hint, not diagnostic justification" per BMC Medical Imaging Systematic Review 2025.

### 4.9 Conformal Prediction UQ (Should)

**REQ-AI-080** (Ubiquitous): For classification-like AI outputs (bone vs no-bone, defect class), conformal prediction sets with coverage guarantee α (default 0.90) shall be provided.

**REQ-AI-081** (Ubiquitous): CP calibration set shall be maintained separately from training and independent test sets.

**REQ-AI-082** (Ubiquitous): Prediction set size and coverage shall be reported per inference in sidecar metadata.

**REQ-AI-083** (Ubiquitous): Conformal Ordinal variant (arXiv 2207.02238) SHALL be supported for severity rating tasks.

### 4.10 Adversarial Robustness & Security (Must · cross-ref SEC)

**REQ-AI-090** (Ubiquitous): AI input validation shall include: image dimension bounds, pixel value bounds, DICOM metadata schema check.

**REQ-AI-091** (Ubiquitous): AI model loading shall verify Ed25519/ECDSA signature (REQ-AI-007).

**REQ-AI-092** (Ubiquitous): AI inference shall enforce time budget (configurable, default 5s); exceeding budget triggers fallback and alert.

**REQ-AI-093** (Ubiquitous): AI inference process shall run with minimum privilege (no network, no file write except sidecar scratch).

### 4.11 Drift Detection (Should · cross-ref OPS)

**REQ-AI-100** (Ubiquitous): AI modules shall emit input fingerprints per REQ-OPS-020.

**REQ-AI-101** (Event-driven): When drift alert triggers per REQ-OPS-022, AI modules shall tag subsequent inferences with `drift_flagged: true` in sidecar.

### 4.12 PCCP Boundary Enforcement (Must · cross-ref REG)

**REQ-AI-110** (Ubiquitous): At model load time, PCCP metadata shall be verified against deployed AI-DSF module's authorized PCCP.

**REQ-AI-111** (Event-driven): If a model's PCCP scope exceeds the authorized one, the model shall fail to load with error `XPE_ERR_PCCP_EXCEEDED`.

**REQ-AI-112** (Ubiquitous): Model audit event (per REQ-REG-007) shall be emitted at each model load/unload.

---

## 5. Acceptance Criteria

### 5.1 Core Infrastructure

- [ ] xpe_ai.dll Layer 1 conformance verified
- [ ] Worker-isolated inference architecture functional
- [ ] ONNX Runtime 1.20+ integration with 4 EPs
- [ ] Model signing/verification pipeline
- [ ] Deterministic fallback router tested for 100% coverage

### 5.2 SSL Denoising

- [ ] Noise2Noise baseline model trained on phantom + clinical
- [ ] Benchmark report vs. MFP baseline published
- [ ] Inference latency meets REQ-AI-022

### 5.3 Diffusion

- [ ] At least one diffusion backend (NEED/DiffDenoise) deployed
- [ ] Reader study protocol drafted
- [ ] Opt-in activation tested

### 5.4 ML Defect / Bone Suppression / AI Collimation

- [ ] Each module passes quality thresholds (REQ-AI-041, 051, 061)
- [ ] Each module emits sidecar metadata
- [ ] Each module integrates AIR DICOM SR (cross-ref IOP)

### 5.5 XAI + UQ

- [ ] Grad-CAM sidecar generation functional
- [ ] Conformal prediction sets with validated coverage α=0.90 ± 0.02

### 5.6 Regulatory & Security

- [ ] Model Card API conformant to schema
- [ ] PCCP enforcement tested with boundary violations
- [ ] Adversarial input fuzzing passes

### 5.7 Post-Market

- [ ] Drift fingerprint emission on every inference
- [ ] Audit log integrity verified

---

## 6. Out-of-Scope Clarifications

- Training infrastructure (GPU farm) → separate ops concern
- Model repository/registry → separate ops concern (MLOps)
- Cloud inference endpoints → not in initial scope
- Multi-modal AI (text + image) → not in scope
- Real-time continuous training → excluded per §1.4

---

## 7. Risks and Mitigations

| Risk | Severity | Mitigation |
|------|:--------:|-----------|
| Model performance regression post-PCCP update | High | Canary validation, regression test gate, auto-rollback |
| XAI misleads clinician (false explanation) | Medium | Disclaimer + user education; limit to supplementary |
| Diffusion hallucination risk | High | Benchmark gate + degraded mode; restrict to premium opt-in |
| Adversarial attacks | Medium | Input validation + signed models + isolated worker |
| Training data leakage | High | Data lineage audit + de-identification verification |
| TensorRT EP instability on new drivers | Medium | Fall back to CUDA EP or CPU EP; multi-EP test matrix |
| Large model file distribution | Medium | CDN + signature verification; SBOM inclusion |

---

## 8. Deliverables

### 8.1 Code Artifacts

- `modules/ai/include/xpe/ai/xpe_ai_api.h` (~15 API functions)
- `modules/ai/src/xpe_ai_core.cpp`
- `modules/ai/src/xpe_ai_ssl_denoise.cpp`
- `modules/ai/src/xpe_ai_diffusion.cpp`
- `modules/ai/src/xpe_ai_ml_defect.cpp`
- `modules/ai/src/xpe_ai_bone_suppress.cpp`
- `modules/ai/src/xpe_ai_collimation.cpp`
- `modules/ai/src/xpe_ai_xai.cpp`
- `modules/ai/src/xpe_ai_cp.cpp`
- `modules/ai/src/xpe_ai_worker.cpp` (isolation)
- `modules/ai/tests/` (≥ 80% coverage)
- `models/` directory with signed .onnx files

### 8.2 Training Recipes

- `training/ssl/noise2noise_recipe.py`
- `training/ssl/noise2self_recipe.py`
- `training/diffusion/need_recipe.py`
- `training/ml_defect/vit_ae_recipe.py`
- `training/bone_suppression/unet_recipe.py`
- `training/conformal/calibration_recipe.py`

### 8.3 Documents

- `docs/ai/ai-architecture.md`
- `docs/ai/ai-quality-benchmarks.md`
- `docs/ai/xai-usage-guide.md`
- `docs/ai/conformal-prediction-guide.md`
- `docs/ai/reader-study-protocol.md`

---

## 9. Dependencies

- **Upstream (hard)**: SPEC-XPE-MASTER v3.0.0, SPEC-XPE-REG v1.0, SPEC-XPE-SEC v1.0, SPEC-XPE-OPS v1.0, SPEC-XPE-P2-ADV v1.0 완료, S0-B (xpe_common) 완료
- **Upstream (soft)**: SPEC-XPE-IOP v1.0 (AI SR encoding), training data availability
- **External**: ONNX Runtime 1.20+, PyTorch 2.x training, GPU infra

---

## 10. Change Control

- Model updates: per PCCP §4.12
- New AI module addition: new SPEC variant (P3-AI-vN)
- XAI method addition: Should-tier, requires clinical justification

---

**본 SPEC은 XPE의 Phase 3 AI 구현 마스터로서 PCCP/GMLP/EU AI Act 준수와 최신 2024-2026 SSL·Diffusion·UQ 트렌드를 반영한다.**
