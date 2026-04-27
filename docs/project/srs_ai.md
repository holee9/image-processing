# Software Requirements Specification (SRS)

## xpe_ai.dll -- AI Inference Module

| Field | Value |
|-------|-------|
| **Document ID** | SRS-AI-001 |
| **Version** | 0.1.0 |
| **Status** | Draft (Skeleton) |
| **Date** | 2026-04-22 |
| **Author** | xpe-docs |
| **IEC 62304 Class** | B |
| **SPEC Reference** | SPEC-XPE-P3-AI v1.1 |
| **Implementation Status** | Skeleton (Stub Build) |

> **Implementation Note**: This document reflects the skeleton implementation as of 2026-04-22.
> Items marked **[SKELETON]** have API boundary and fallback logic implemented but no actual AI inference.
> Items marked **[DEFERRED]** are planned for Phase 3 full implementation.

---

## 1. Introduction

### 1.1 Purpose

This document specifies the software requirements for `xpe_ai.dll`, the XPE AI Inference Module (Layer 1, Phase 3). The module provides deep-learning inference for body-part recognition, image stitching, bone suppression, DL-based denoising, and model transparency (Model Card API). All AI functions operate with a deterministic fallback path to ensure clinical safety.

### 1.2 Scope

The module operates as an in-process C ABI proxy. Actual inference runs in a sandboxed worker process (`xpe_ai_worker.exe`) over IPC (named pipes). The module depends solely on `xpe_common.dll` (Layer 0) and does not depend on other Layer 1 modules.

Software Units (SWU) covered:

- **SWU-AI-01**: Module Lifecycle (init/shutdown/version)
- **SWU-AI-02**: Body-Part Recognition (CNN classification)
- **SWU-AI-03**: AI Image Stitching (feature-based alignment)
- **SWU-AI-04**: Stitch Size Estimation (deterministic)
- **SWU-AI-05**: Bone Suppression (U-Net)
- **SWU-AI-06**: DL Denoising (self-supervised / diffusion)
- **SWU-AI-07**: Model Card API (transparency)
- **SWU-AI-08**: Fallback Router (deterministic fallback)

### 1.3 Referenced Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| SPEC-XPE-P3-AI | AI Module SPEC | 1.1 |
| XPE-API-SPEC-001 | XPE API Specification | 1.3.0 |
| IEC 62304 | Medical Device Software Lifecycle | 2006+A1:2015 |
| model-card.schema.json | Model Card JSON Schema | 1.0 |

---

## 2. Functional Requirements

### 2.1 Architecture Principles

#### REQ-AI-001: Layer 1 Dependency

The xpe_ai module **shall** depend only on `xpe_common.dll`. No lateral dependency on any other `xpe_*.dll` is permitted.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-ARCH-001 |
| Priority | Must |
| SWU | All |
| Verification | TC-ABI-001: InitShutdownCycle, `dumpbin /dependents` |

#### REQ-AI-002: Deterministic Fallback Routing

**When** any AI inference function fails, times out, or returns confidence below the configured threshold (default 0.6), the system **shall** return `XPE_ERR_PROCESSING_FAILED` and the caller **shall** invoke the deterministic fallback path. The fallback routing **shall** be controlled by `xpe_ai_set_fallback_mode()`.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-ARCH-002 |
| Priority | Must |
| SWU | SWU-AI-08 |
| Verification | TC-FALLBACK-001~023, TC-WORKER-001~014 |

Per-function fallback paths:

| AI Function | Deterministic Fallback |
|-------------|----------------------|
| `xpe_bodypart_recognize` | DICOM metadata body-part tag, or operator selection |
| `xpe_stitch_images` | Translation-only stitching (no feature matching) |
| `xpe_bone_suppress` | Original image unchanged (optional enhancement) |
| `xpe_dl_denoise` | `xpe_noise_reduce` from enhance_basic (bilateral/NLM) |

#### REQ-AI-003: Worker-Isolated Architecture

**When** AI inference is performed, the module **shall** execute inference in a separate worker process (`xpe_ai_worker.exe`). Communication **shall** use IPC via named pipes (`\\.\pipe\xpe_ai_worker_{PID}`). The main process **shall** be immune to worker process crashes.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-ARCH-003 |
| Priority | Must |
| SWU | SWU-AI-01 |
| Verification | TC-WORKER-001~014 **[DEFERRED: requires worker process]** |

> **[SKELETON]** The IPC bridge is designed but not yet implemented. In stub mode, all inference functions return `XPE_ERR_PROCESSING_FAILED`, simulating the "worker unavailable" fallback path.

#### REQ-AI-004: Sidecar Metadata Delivery

AI metadata **shall** be delivered via sidecar JSON alongside the image, not by mutating `XpeImageMetadata`. This satisfies the SPEC-XPE-MASTER v3.0 Sidecar Contract.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-ARCH-004 |
| Priority | Must |
| SWU | All inference SWUs |
| Verification | **[DEFERRED]** |

#### REQ-AI-005: Opt-In Activation

AI execution **shall** be opt-in per pipeline configuration. The default state **shall** be off until explicit activation via `xpe_ai_init()`.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-ARCH-005 |
| Priority | Must |
| SWU | SWU-AI-01 |
| Verification | TC-ABI-001~024 (not-initialized guard tests) |

#### REQ-AI-006: ONNX Runtime 1.20+ Integration

The module **shall** use ONNX Runtime 1.20+ as the model runtime. Execution Provider selection **shall** be configurable: CPU (always available), CUDA (x86 GPU), TensorRT (NVIDIA), DirectML (Windows GPU).

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-ARCH-006 |
| Priority | Must |
| SWU | SWU-AI-01 |
| Verification | **[DEFERRED: stub build has no ONNX Runtime]** |

#### REQ-AI-007: Model Signing

Model files (.onnx) **shall** be signed (Ed25519 or ECDSA P-256) and verified at load time.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-ARCH-007 |
| Priority | Must |
| SWU | SWU-AI-01 |
| Verification | **[DEFERRED]** |

#### REQ-AI-008: Model Versioning

Model versioning **shall** follow semantic versioning. Model metadata **shall** include: `model_id`, `version`, `pccp_scope`, `training_data_hash`, `validation_metrics`.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-ARCH-008 |
| Priority | Must |
| SWU | SWU-AI-07 |
| Verification | TC-MODELCARD-001~017 |

---

### 2.2 Module Lifecycle (SWU-AI-01)

#### REQ-AI-LC-001: Module Initialization

**When** `xpe_ai_init(modelDirPath, configJsonOrNull)` is called with a valid `modelDirPath`, the module **shall** launch the sandboxed AI worker process, load ONNX models from the specified directory, and initialize the inference runtime. Calling with `NULL` config **shall** use default parameters (CPU EP, 5 s timeout, 0.6 confidence threshold). Calling with `NULL` modelDirPath **shall** return `XPE_ERR_INVALID_INPUT`.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-001 |
| Priority | Must |
| SWU | SWU-AI-01 |
| Verification | TC-ABI-002~007 |

> **[SKELETON]** In stub mode, `xpe_ai_init()` registers placeholder model IDs without launching a worker or loading .onnx files.

#### REQ-AI-LC-002: Module Shutdown

**When** `xpe_ai_shutdown()` is called, the module **shall** stop the worker process, unload all models, and release IPC resources. Shutdown **shall** be safe to call without prior initialization, and **shall** be idempotent.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-003 |
| Priority | Must |
| SWU | SWU-AI-01 |
| Verification | TC-ABI-008~010 |

#### REQ-AI-LC-003: Version String

**When** `xpe_ai_version()` is called, the module **shall** return a non-null, non-empty string in "X.Y.Z" format. The string **shall** be owned by the DLL (static storage) and **shall not** be freed by the caller.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-VER-001 |
| Priority | Must |
| SWU | SWU-AI-01 |
| Verification | TC-ABI-011~014 |

---

### 2.3 Body-Part Recognition (SWU-AI-02)

#### REQ-AI-BP-001: Body-Part Classification

**When** `xpe_bodypart_recognize(img, bodyPartOut, bufLen, confidenceOut)` is called with valid inputs, the module **shall** classify the anatomical body part in the image using a CNN classifier, write the label (e.g., "CHEST", "HAND") to `bodyPartOut`, and write the confidence score [0, 1] to `confidenceOut`.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-010 |
| Priority | Must |
| SWU | SWU-AI-02 |
| Verification | TC-FALLBACK-007~014 **[SKELETON: stub returns PROCESSING_FAILED]** |

#### REQ-AI-BP-002: Body-Part Input Validation

**When** any input parameter (`img`, `bodyPartOut`) is NULL, or `bufLen` is zero, the module **shall** return `XPE_ERR_INVALID_INPUT`. When `bufLen` is insufficient, the module **shall** return `XPE_ERR_BUFFER_TOO_SMALL`.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-010-VAL |
| Priority | Must |
| SWU | SWU-AI-02 |
| Verification | TC-FALLBACK-007~014 |

---

### 2.4 Image Stitching (SWU-AI-03)

#### REQ-AI-ST-001: AI Image Stitching

**When** `xpe_stitch_images(parts, partCount, stitchedOut, configJsonOrNull)` is called with valid inputs (partCount >= 2), the module **shall** stitch overlapping partial images using AI-based feature matching for alignment.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-020, SRS-AI-021 |
| Priority | Must |
| SWU | SWU-AI-03 |
| Verification | TC-FALLBACK-015~019 **[SKELETON]** |

#### REQ-AI-ST-002: Stitch Size Estimation

**When** `xpe_stitch_estimate_size(parts, partCount, widthOut, heightOut)` is called with valid inputs, the module **shall** estimate output dimensions using a deterministic heuristic:
- Width: `max(parts.width) * (1 + 0.7 * (partCount - 1))`, clamped to 4096
- Height: `max(parts.height)`, clamped to 4096

This function does NOT require AI inference and works regardless of worker state.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-020-EST |
| Priority | Must |
| SWU | SWU-AI-04 |
| Verification | TC-ABI-015~020 |

---

### 2.5 Bone Suppression (SWU-AI-05)

#### REQ-AI-BS-001: Bone Suppression

**When** `xpe_bone_suppress(img, softTissueOut, configJsonOrNull)` is called with valid inputs, the module **shall** produce a soft-tissue-only image using a U-Net model. The output buffer must be pre-allocated with the same dimensions as the input.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-030 |
| Priority | Must |
| SWU | SWU-AI-05 |
| Verification | TC-FALLBACK-020~023 **[SKELETON]** |

---

### 2.6 DL Denoising (SWU-AI-06)

#### REQ-AI-DN-001: DL Denoising

**When** `xpe_dl_denoise(img, meta, configJsonOrNull)` is called with valid inputs, the module **shall** apply deep-learning denoising in-place. Model variant selection is based on `meta->bodyPart` and `meta->mAs`.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-040 |
| Priority | Must |
| SWU | SWU-AI-06 |
| Verification | TC-FALLBACK-020~023 **[SKELETON]** |

---

### 2.7 Model Card API (SWU-AI-07)

#### REQ-AI-MC-001: Model Card Transparency

**When** `xpe_ai_get_model_card(modelId, buf, bufSize)` is called with a known model ID, the module **shall** return JSON containing: `intended_use`, `training_data_summary`, `demographic_performance`, `limitations`, `model_version`, `pccp_status`, `published_date`.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-MC-001 |
| Priority | Must |
| SWU | SWU-AI-07 |
| Verification | TC-MODELCARD-001~017 |

#### REQ-AI-MC-002: JSON Schema Conformance

The model card JSON **shall** conform to `schemas/model-card.schema.json`.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-MC-002 |
| Priority | Must |
| SWU | SWU-AI-07 |
| Verification | TC-MODELCARD-001~017 |

#### REQ-AI-MC-003: Model Metadata Completeness

Model metadata **shall** include: `model_id`, `version`, `pccp_scope`, `training_data_hash`, `validation_metrics`.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-MC-003 |
| Priority | Must |
| SWU | SWU-AI-07 |
| Verification | TC-MODELCARD-001~017 |

---

### 2.8 Fallback Router (SWU-AI-08)

#### REQ-AI-FB-001: Low-Confidence Event

**When** AI inference confidence is below the configured threshold (default 0.6), the system **shall** emit a low-confidence event and fall back to the deterministic path.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-FB-001 |
| Priority | Must |
| SWU | SWU-AI-08 |
| Verification | TC-FALLBACK-022 (confidence threshold default) |

#### REQ-AI-FB-002: Fallback Mode Toggle

**When** `xpe_ai_set_fallback_mode(enable)` is called, the module **shall** toggle the deterministic fallback mode. When enabled (default), all AI functions return `XPE_ERR_PROCESSING_FAILED` on low confidence. When disabled, AI functions attempt retries.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-FB-002 |
| Priority | Must |
| SWU | SWU-AI-08 |
| Verification | TC-FALLBACK-001~005 |

---

### 2.9 Self-Supervised Denoising **[DEFERRED]**

#### REQ-AI-020: SSL Denoising

**When** the POST-02 AI tier is activated, the module **shall** implement self-supervised denoising trainable without clean reference images (N2N, N2S, Neighbor2Neighbor, Noise2Sim).

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-SSL-001 |
| Priority | Should |
| SWU | SWU-AI-06 |
| Verification | **[DEFERRED: Phase 3 full implementation]** |

#### REQ-AI-022: SSL Latency Target

SSL model inference latency **shall** be <= 500 ms per 3000x3000 image on CPU, <= 100 ms on GPU (TensorRT).

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-SSL-002 |
| Priority | Should |
| SWU | SWU-AI-06 |
| Verification | **[DEFERRED]** |

---

### 2.10 Diffusion Priors Enhancement **[DEFERRED]**

#### REQ-AI-030: Diffusion Enhancement

**When** the diffusion-prior enhancement tier is activated (opt-in premium), the module **shall** provide diffusion-based image enhancement.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-DIFF-001 |
| Priority | Should |
| SWU | SWU-AI-06 |
| Verification | **[DEFERRED]** |

---

### 2.11 XAI Sidecar **[DEFERRED]**

#### REQ-AI-070: XAI Sidecar Generation

**When** XAI is enabled (opt-in per inference), the module **shall** generate Grad-CAM saliency maps and/or SHAP feature attribution as sidecar metadata.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-XAI-001 |
| Priority | Should |
| SWU | N/A |
| Verification | **[DEFERRED]** |

---

### 2.12 Conformal Prediction UQ **[DEFERRED]**

#### REQ-AI-080: Conformal Prediction

**When** classification-like AI outputs are produced, conformal prediction sets with coverage guarantee alpha (default 0.90) **shall** be provided.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-CP-001 |
| Priority | Should |
| SWU | N/A |
| Verification | **[DEFERRED]** |

---

### 2.13 PCCP Boundary Enforcement **[DEFERRED]**

#### REQ-AI-110: PCCP Boundary Check

**When** a model is loaded, PCCP metadata **shall** be verified against the deployed AI-DSF module's authorized PCCP scope. If exceeded, loading **shall** fail with `XPE_ERR_PCCP_EXCEEDED`.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-PCCP-001 |
| Priority | Must (conditional on Phase 3 deployment) |
| SWU | SWU-AI-01 |
| Verification | **[DEFERRED]** |

---

### 2.14 Adversarial Robustness **[DEFERRED]**

#### REQ-AI-090: Input Validation

AI input validation **shall** include: image dimension bounds, pixel value bounds, DICOM metadata schema check.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-SEC-001 |
| Priority | Must |
| SWU | All inference SWUs |
| Verification | **[DEFERRED: basic input validation implemented in skeleton]** |

#### REQ-AI-092: Time Budget Enforcement

AI inference **shall** enforce a configurable time budget (default 5 s). Exceeding the budget **shall** trigger fallback and alert.

| Attribute | Value |
|-----------|-------|
| SRS ID | SRS-AI-SEC-002 |
| Priority | Must |
| SWU | SWU-AI-08 |
| Verification | **[DEFERRED]** |

---

## 3. Non-Functional Requirements

### 3.1 Thread Safety

- `xpe_ai_init()` and `xpe_ai_shutdown()`: Not thread-safe; single-thread startup/shutdown
- All inference functions: Reentrant (thread-safe per-call)
- `xpe_ai_set_fallback_mode()`: Thread-safe via atomic flag
- `xpe_ai_get_model_card()`: Thread-safe (read-only model metadata)
- `xpe_ai_version()`: Thread-safe (static string)

### 3.2 Memory Safety

- No dynamic memory allocation in inference hot path (caller pre-allocates all buffers)
- DLL-owned static storage for version string and model registry
- IPC resources released on shutdown

### 3.3 Build Modes

| Mode | Flag | Behavior |
|------|------|----------|
| Stub | `XPE_AI_USE_ONNXRUNTIME=OFF` (default) | No ONNX Runtime. All inference returns `XPE_ERR_PROCESSING_FAILED`. |
| Full | `XPE_AI_USE_ONNXRUNTIME=ON` + `ONNXRUNTIME_ROOT` | Links ONNX Runtime. Full inference pipeline. |

### 3.4 Conditional Dependencies

| Dependency | Condition | Purpose |
|------------|-----------|---------|
| ONNX Runtime 1.20+ | `XPE_AI_USE_ONNXRUNTIME=ON` | Model inference |
| spdlog | `XPE_AI_USE_SPDLOG=1` | Structured logging |
| nlohmann/json | `XPE_AI_USE_NLOHMANN_JSON=1` | Config parsing |

---

## 4. Deferred Requirements Summary

The following REQs from SPEC-XPE-P3-AI are acknowledged but deferred to Phase 3 full implementation:

| SPEC REQ | SRS Mapping | Description | Status |
|----------|-------------|-------------|--------|
| REQ-AI-006 | SRS-AI-ARCH-006 | ONNX Runtime multi-EP | Stub mode only |
| REQ-AI-007 | SRS-AI-ARCH-007 | Model signing | Not implemented |
| REQ-AI-020~024 | SRS-AI-SSL-001~002 | Self-Supervised Denoising | Not implemented |
| REQ-AI-030~033 | SRS-AI-DIFF-001 | Diffusion Priors | Not implemented |
| REQ-AI-040~042 | -- | ML Defect Correction | Not implemented |
| REQ-AI-050~052 | -- | Bone Suppression quality targets | Not implemented |
| REQ-AI-060~062 | -- | AI Collimation Detection | Not implemented |
| REQ-AI-070~073 | SRS-AI-XAI-001 | XAI Sidecar | Not implemented |
| REQ-AI-080~083 | SRS-AI-CP-001 | Conformal Prediction UQ | Not implemented |
| REQ-AI-090~093 | SRS-AI-SEC-001~002 | Adversarial Robustness | Partial (input null checks) |
| REQ-AI-100~101 | -- | Drift Detection | Not implemented |
| REQ-AI-110~112 | SRS-AI-PCCP-001 | PCCP Boundary | Not implemented |

---

## 5. Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1.0 | 2026-04-22 | xpe-docs | Initial SRS for skeleton implementation. Covers REQ-AI-001~005, 008, 010~012. |

---

*This document satisfies IEC 62304 Class B requirements for software requirements specification (Section 5.2).*
