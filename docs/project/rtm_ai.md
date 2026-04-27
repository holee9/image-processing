# Requirements Traceability Matrix (RTM)

## xpe_ai.dll -- AI Inference Module

| Field | Value |
|-------|-------|
| **Document ID** | RTM-AI-001 |
| **Version** | 0.1.0 |
| **Status** | Draft (Skeleton) |
| **Date** | 2026-04-22 |
| **Author** | xpe-docs |
| **IEC 62304 Class** | B |
| **SPEC Reference** | SPEC-XPE-P3-AI v1.1 |
| **Implementation Status** | Skeleton (Stub Build) |

---

## 1. Traceability Overview

This matrix traces every requirement (REQ-AI-XXX) from SRS-AI-001 to:
- **Design reference**: SDD-AI-001 section
- **Implementation files**: Source code in `modules/ai/`
- **Test IDs**: Google Test cases in `tests/ai_tests/`
- **Verification status**: Written / Verified / Deferred

### Status Legend

| Status | Meaning |
|--------|---------|
| Written | Test case written, pending compilation and execution |
| Verified | Test executed and passed |
| Deferred | Test deferred to Phase 3 full implementation |

---

## 2. Architecture Principles

| Req ID | Requirement | SRS Ref | SDD Ref | Implementation Files | Test IDs | Status |
|--------|------------|---------|---------|---------------------|----------|--------|
| REQ-AI-001 | Layer 1 dependency (xpe_common only) | SRS-AI-ARCH-001 | SDD Sec 2.2, 6 | `modules/ai/src/ai.cpp` | TC-ABI-001: InitShutdownCycle | Written |
| REQ-AI-002 | Deterministic fallback routing | SRS-AI-ARCH-002 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-001~023 | Written |
| REQ-AI-003 | Worker-isolated architecture (IPC) | SRS-AI-ARCH-003 | SDD Sec 3.1 | `modules/ai/include/xpe/ai/ai_worker_protocol.h` | TC-WORKER-001~014 | Deferred |
| REQ-AI-004 | Sidecar metadata delivery | SRS-AI-ARCH-004 | SDD Sec 4.5 | (not yet implemented) | -- | Deferred |
| REQ-AI-005 | Opt-in activation (default off) | SRS-AI-ARCH-005 | SDD Sec 4.2 | `modules/ai/src/ai.cpp` | TC-ABI: Not-initialized guards (6 functions) | Written |
| REQ-AI-006 | ONNX Runtime 1.20+ multi-EP | SRS-AI-ARCH-006 | SDD Sec 5.1 | `modules/ai/CMakeLists.txt` | -- | Deferred |
| REQ-AI-007 | Model signing (Ed25519/ECDSA) | SRS-AI-ARCH-007 | SDD Sec 9 | (not yet implemented) | -- | Deferred |
| REQ-AI-008 | Model versioning (semver) | SRS-AI-ARCH-008 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-001~017 | Written |

---

## 3. Module Lifecycle

| Req ID | Requirement | SRS Ref | SDD Ref | Implementation Files | Test IDs | Status |
|--------|------------|---------|---------|---------------------|----------|--------|
| REQ-AI-LC-001 | Init: null path returns INVALID_INPUT | SRS-AI-001 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-002: InitNullPath | Written |
| REQ-AI-LC-001 | Init: valid path | SRS-AI-001 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-003: InitValidPath | Written |
| REQ-AI-LC-001 | Init: null config uses defaults | SRS-AI-001 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-004: InitNullConfig | Written |
| REQ-AI-LC-001 | Init: config JSON parsing | SRS-AI-001 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-005: InitConfigJson | Written |
| REQ-AI-LC-001 | Init idempotent | SRS-AI-001 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-006: InitIdempotent | Written |
| REQ-AI-LC-001 | Init/shutdown repeated cycle | SRS-AI-001 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-007: RepeatedCycle | Written |
| REQ-AI-LC-002 | Shutdown without init safe | SRS-AI-003 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-008: ShutdownWithoutInit | Written |
| REQ-AI-LC-002 | Shutdown idempotent | SRS-AI-003 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-009: ShutdownIdempotent | Written |
| REQ-AI-LC-002 | Shutdown repeated safe | SRS-AI-003 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-010: RepeatedShutdown | Written |
| REQ-AI-LC-003 | Version non-null | SRS-AI-VER-001 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-011: VersionNonNull | Written |
| REQ-AI-LC-003 | Version non-empty | SRS-AI-VER-001 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-012: VersionNonEmpty | Written |
| REQ-AI-LC-003 | Version semver format | SRS-AI-VER-001 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-013: VersionSemver | Written |
| REQ-AI-LC-003 | Version deterministic | SRS-AI-VER-001 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-014: VersionDeterministic | Written |

---

## 4. Body-Part Recognition (SWU-AI-02)

| Req ID | Requirement | SRS Ref | SDD Ref | Implementation Files | Test IDs | Status |
|--------|------------|---------|---------|---------------------|----------|--------|
| REQ-AI-BP-001 | Body-part recognize: not initialized | SRS-AI-010 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-007: NotInitialized | Written |
| REQ-AI-BP-002 | Null image returns INVALID_INPUT | SRS-AI-010-VAL | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-008: NullImage | Written |
| REQ-AI-BP-002 | Null label returns INVALID_INPUT | SRS-AI-010-VAL | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-009: NullLabel | Written |
| REQ-AI-BP-002 | Zero bufLen returns BUFFER_TOO_SMALL | SRS-AI-010-VAL | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-010: ZeroBufLen | Written |
| REQ-AI-BP-002 | Null confidence pointer handled | SRS-AI-010-VAL | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-011: NullConf | Written |
| REQ-AI-BP-002 | Invalid buffer returns BUFFER_TOO_SMALL | SRS-AI-010-VAL | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-012: InvalidBuffer | Written |
| REQ-AI-BP-001 | Stub fallback returns PROCESSING_FAILED | SRS-AI-010 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-013: StubFallback | Written |
| REQ-AI-BP-001 | Stub returns label and confidence | SRS-AI-010 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-014: StubLabelConf | Written |

---

## 5. Image Stitching (SWU-AI-03)

| Req ID | Requirement | SRS Ref | SDD Ref | Implementation Files | Test IDs | Status |
|--------|------------|---------|---------|---------------------|----------|--------|
| REQ-AI-ST-001 | Stitch: not initialized | SRS-AI-020 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-015: NotInitialized | Written |
| REQ-AI-ST-001 | Null parts returns INVALID_INPUT | SRS-AI-020 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-016: NullParts | Written |
| REQ-AI-ST-001 | Count=1 returns INVALID_INPUT | SRS-AI-020 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-017: SinglePart | Written |
| REQ-AI-ST-001 | Null output returns INVALID_INPUT | SRS-AI-020 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-018: NullOutput | Written |
| REQ-AI-ST-001 | Null output data returns INVALID_INPUT | SRS-AI-020 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-019: NullOutputData | Written |

---

## 6. Stitch Size Estimation (SWU-AI-04)

| Req ID | Requirement | SRS Ref | SDD Ref | Implementation Files | Test IDs | Status |
|--------|------------|---------|---------|---------------------|----------|--------|
| REQ-AI-ST-002 | Deterministic output | SRS-AI-020-EST | SDD Sec 4.4 | `modules/ai/src/ai.cpp` | TC-ABI-015: DeterministicSize | Written |
| REQ-AI-ST-002 | Valid dimensions | SRS-AI-020-EST | SDD Sec 4.4 | `modules/ai/src/ai.cpp` | TC-ABI-016: ValidDims | Written |
| REQ-AI-ST-002 | Null parts returns INVALID_INPUT | SRS-AI-020-EST | SDD Sec 4.4 | `modules/ai/src/ai.cpp` | TC-ABI-017: NullParts | Written |
| REQ-AI-ST-002 | Null output returns INVALID_INPUT | SRS-AI-020-EST | SDD Sec 4.4 | `modules/ai/src/ai.cpp` | TC-ABI-018: NullOutputs | Written |
| REQ-AI-ST-002 | Single part handling | SRS-AI-020-EST | SDD Sec 4.4 | `modules/ai/src/ai.cpp` | TC-ABI-019: SinglePart | Written |
| REQ-AI-ST-002 | 4096 clamp applied | SRS-AI-020-EST | SDD Sec 4.4 | `modules/ai/src/ai.cpp` | TC-ABI-020: Clamp4096 | Written |

---

## 7. Bone Suppression (SWU-AI-05)

| Req ID | Requirement | SRS Ref | SDD Ref | Implementation Files | Test IDs | Status |
|--------|------------|---------|---------|---------------------|----------|--------|
| REQ-AI-BS-001 | Not initialized | SRS-AI-030 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-020: NotInitialized | Written |
| REQ-AI-BS-001 | Null image returns INVALID_INPUT | SRS-AI-030 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-021: NullImage | Written |
| REQ-AI-BS-001 | Null output returns INVALID_INPUT | SRS-AI-030 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-022: NullOutput | Written |
| REQ-AI-BS-001 | Dimension mismatch | SRS-AI-030 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-023: DimMismatch | Written |

---

## 8. DL Denoising (SWU-AI-06)

| Req ID | Requirement | SRS Ref | SDD Ref | Implementation Files | Test IDs | Status |
|--------|------------|---------|---------|---------------------|----------|--------|
| REQ-AI-DN-001 | Not initialized | SRS-AI-040 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-020: NotInitialized | Written |
| REQ-AI-DN-001 | Null image returns INVALID_INPUT | SRS-AI-040 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-021: NullImage | Written |
| REQ-AI-DN-001 | Null meta returns INVALID_INPUT | SRS-AI-040 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-022: NullMeta | Written |

---

## 9. Model Card API (SWU-AI-07)

| Req ID | Requirement | SRS Ref | SDD Ref | Implementation Files | Test IDs | Status |
|--------|------------|---------|---------|---------------------|----------|--------|
| REQ-AI-MC-001 | Known model: model_id field | SRS-AI-MC-001 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-001: ModelId | Written |
| REQ-AI-MC-001 | Known model: version field | SRS-AI-MC-001 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-002: Version | Written |
| REQ-AI-MC-001 | Known model: pccp_status field | SRS-AI-MC-001 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-003: PccpStatus | Written |
| REQ-AI-MC-001 | Known model: intended_use field | SRS-AI-MC-001 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-004: IntendedUse | Written |
| REQ-AI-MC-001 | Known model: training_data_summary | SRS-AI-MC-001 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-005: TrainingDataSummary | Written |
| REQ-AI-MC-001 | Known model: demographic_perf | SRS-AI-MC-001 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-006: DemographicPerf | Written |
| REQ-AI-MC-001 | Known model: limitations field | SRS-AI-MC-001 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-007: Limitations | Written |
| REQ-AI-MC-001 | Known model: published_date field | SRS-AI-MC-001 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-008: PublishedDate | Written |
| REQ-AI-MC-001 | Different model returns different card | SRS-AI-MC-001 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-009~011: DifferentModels | Written |
| REQ-AI-MC-001 | Unknown model returns IO_FAILED | SRS-AI-MC-001 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-012~014: UnknownModel | Written |
| REQ-AI-MC-002 | Null modelId returns INVALID_INPUT | SRS-AI-MC-002 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-015: NullModelId | Written |
| REQ-AI-MC-002 | Null buffer returns INVALID_INPUT | SRS-AI-MC-002 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-016: NullBuffer | Written |
| REQ-AI-MC-002 | Small buffer returns BUFFER_TOO_SMALL | SRS-AI-MC-002 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-017: BufferTooSmall | Written |

---

## 10. Fallback Router (SWU-AI-08)

| Req ID | Requirement | SRS Ref | SDD Ref | Implementation Files | Test IDs | Status |
|--------|------------|---------|---------|---------------------|----------|--------|
| REQ-AI-FB-002 | Enable fallback mode | SRS-AI-FB-002 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-001: EnableFallback | Written |
| REQ-AI-FB-002 | Disable fallback mode | SRS-AI-FB-002 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-002: DisableFallback | Written |
| REQ-AI-FB-002 | Repeated toggle | SRS-AI-FB-002 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-003: RepeatedToggle | Written |
| REQ-AI-FB-002 | Non-zero values enable | SRS-AI-FB-002 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-004: NonZeroEnables | Written |
| REQ-AI-FB-001 | Confidence threshold default (0.6) | SRS-AI-FB-001 | SDD Sec 4.2 | `modules/ai/src/ai.cpp` | TC-FALLBACK-022: ConfThresholdDefault | Written |

---

## 11. Thread Safety

| Req ID | Requirement | SRS Ref | SDD Ref | Implementation Files | Test IDs | Status |
|--------|------------|---------|---------|---------------------|----------|--------|
| REQ-AI-001 | Concurrent bodypart_recognize (4 threads, 100 calls) | SRS-AI-ARCH-001 | SDD Sec 7.2 | `modules/ai/src/ai.cpp` | TC-WORKER-005~007: ConcurrentBodyPart | Written |
| REQ-AI-002 | Concurrent set_fallback_mode (4 threads, 400 calls) | SRS-AI-ARCH-002 | SDD Sec 7.2 | `modules/ai/src/ai.cpp` | TC-WORKER-008~010: ConcurrentFallback | Written |

---

## 12. Deferred Requirements

The following SPEC requirements have no test coverage in the current skeleton:

| SPEC REQ | SRS Mapping | Description | Reason |
|----------|-------------|-------------|--------|
| REQ-AI-006 | SRS-AI-ARCH-006 | ONNX Runtime multi-EP | Stub mode: no ONNX Runtime |
| REQ-AI-007 | SRS-AI-ARCH-007 | Model signing | Not implemented |
| REQ-AI-009 | -- | Time budget enforcement | Not implemented |
| REQ-AI-020~024 | SRS-AI-SSL-001~002 | Self-Supervised Denoising | Not implemented |
| REQ-AI-030~033 | SRS-AI-DIFF-001 | Diffusion Priors | Not implemented |
| REQ-AI-040~042 | -- | ML Defect Correction | Not implemented |
| REQ-AI-050~052 | -- | Bone Suppression quality targets | Not implemented |
| REQ-AI-060~062 | -- | AI Collimation Detection | Not implemented |
| REQ-AI-070~073 | SRS-AI-XAI-001 | XAI Sidecar | Not implemented |
| REQ-AI-080~083 | SRS-AI-CP-001 | Conformal Prediction UQ | Not implemented |
| REQ-AI-090~093 | SRS-AI-SEC-001~002 | Adversarial Robustness | Partial (null checks only) |
| REQ-AI-100~101 | -- | Drift Detection | Not implemented |
| REQ-AI-110~112 | SRS-AI-PCCP-001 | PCCP Boundary | Not implemented |

---

## 13. Coverage Summary

### Current (Skeleton / Stub Build)

| Category | REQs Covered | REQs Deferred | Test Cases | Status |
|----------|-------------|---------------|------------|--------|
| Architecture | 5/9 | 4 | 24 (ABI) | Written |
| Lifecycle | 3/3 | 0 | 13 | Written |
| Inference (BodyPart) | 2/2 | 0 | 8 | Written |
| Inference (Stitch) | 2/2 | 0 | 5 | Written |
| Utility (Stitch Est.) | 1/1 | 0 | 6 | Written |
| Inference (Bone Sup.) | 1/1 | 0 | 4 | Written |
| Inference (DL Denoise) | 1/1 | 0 | 3 | Written |
| Model Card | 3/3 | 0 | 17 | Written |
| Fallback Router | 2/2 | 0 | 5 | Written |
| Thread Safety | 2/2 | 0 | 2 | Written |
| **Total** | **22/26** | **4** | **78** | **Written** |

### Test File Summary

| Test File | Tests | Focus |
|-----------|-------|-------|
| `test_ai_abi.cpp` | 24 | C ABI boundary: version, init, shutdown, stitch_estimate_size |
| `test_ai_fallback.cpp` | 23 | Fallback routing, stub behavior, input validation |
| `test_ai_model_card.cpp` | 17 | Model Card JSON API, schema validation, buffer handling |
| `test_ai_worker_isolation.cpp` | 14 | Worker crash simulation, thread safety, protocol constants |
| **Total** | **78** | |

---

## 14. Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1.0 | 2026-04-22 | xpe-docs | Initial RTM for skeleton implementation. 78 test cases covering REQ-AI-001~005, 008, 010~012. |

---

*This document satisfies IEC 62304 Class B requirements for requirements traceability (Section 5.7).*
