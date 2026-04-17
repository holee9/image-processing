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
  - REQ-AI-010~012 (Model Card) — FDA Transparency 대응
  - REQ-AI-013 (Data Lineage) — GMLP Principle #3
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
