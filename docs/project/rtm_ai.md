# Requirements Traceability Matrix (RTM)

## xpe_ai.dll -- AI Inference Module

| Field | Value |
|-------|-------|
| **Document ID** | RTM-AI-001 |
| **Version** | 0.3.0 |
| **Status** | Draft (Skeleton) |
| **Date** | 2026-09-10 |
| **Author** | xpe-docs |
| **IEC 62304 Class** | B |
| **SPEC Reference** | SPEC-XPE-P3-AI v1.1 |
| **Implementation Status** | Skeleton (Stub Build) |

---

## 1. Traceability Overview

This matrix traces every requirement (REQ-AI-XXX) from SRS-AI-001 to:
- **Design reference**: SDD-AI-001 section
- **Implementation files**: Source code in `modules/ai/`
- **Test IDs**: Google Test cases in `modules/ai/tests/`
- **Verification status**: Written / Verified / Deferred
- **VVP Ref**: the V&V Plan section that defines the verification method for the row.
  Since v0.3.0 these point at **`XPE-VVP-AI-001` v1.0.0** (`docs/project/vvp_ai.md`), registered in
  `XPE-VVP-001` §Addendum Registry. 64 of 67 rows are mapped; 3 rows remain `—` because they have
  no test case (REQ-AI-004, REQ-AI-006, REQ-AI-007). The mapping closes IEC 62304 §5.7.4 SRS→test
  traceability for the rows that have a test; it does **not** close §5.7 for the module, because the
  inference path is a stub and levels L3–L6 carry no evidence (`XPE-VVP-AI-001` §2.3, §4.9, §10).

### Status Legend

| Status | Meaning |
|--------|---------|
| Written | Test case written, pending compilation and execution |
| Verified | Test executed and passed |
| Deferred | Test deferred to Phase 3 full implementation |

---

## 2. Architecture Principles

| Req ID | Requirement | SRS Ref | SDD Ref | Implementation Files | Test IDs | Status | **VVP Ref** |
|--------|------------|---------|---------|---------------------|----------|--------|---------|
| REQ-AI-001 | Layer 1 dependency (xpe_common only) | SRS-AI-ARCH-001 | SDD Sec 2.2, 6 | `modules/ai/src/ai.cpp` | TC-ABI-001: InitShutdownCycle | Written | XPE-VVP-AI-001 §4.1 |
| REQ-AI-002 | Deterministic fallback routing | SRS-AI-ARCH-002 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-001~023 | Written | XPE-VVP-AI-001 §4.8 |
| REQ-AI-003 | Worker-isolated architecture (IPC) | SRS-AI-ARCH-003 | SDD Sec 3.1 | `modules/ai/include/xpe/ai/ai_worker_protocol.h` | TC-WORKER-001~014 | Deferred | XPE-VVP-AI-001 §4.7 |
| REQ-AI-004 | Sidecar metadata delivery | SRS-AI-ARCH-004 | SDD Sec 4.5 | (not yet implemented) | -- | Deferred | — |
| REQ-AI-005 | Opt-in activation (default off) | SRS-AI-ARCH-005 | SDD Sec 4.2 | `modules/ai/src/ai.cpp` | TC-ABI: Not-initialized guards (6 functions) | Written | XPE-VVP-AI-001 §4.1 |
| REQ-AI-006 | ONNX Runtime 1.20+ multi-EP | SRS-AI-ARCH-006 | SDD Sec 5.1 | `modules/ai/CMakeLists.txt` | -- | Deferred | — |
| REQ-AI-007 | Model signing (Ed25519/ECDSA) | SRS-AI-ARCH-007 | SDD Sec 9 | (not yet implemented) | -- | Deferred | — |
| REQ-AI-008 | Model versioning (semver) | SRS-AI-ARCH-008 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-001~017 | Written | XPE-VVP-AI-001 §4.6 |

---

## 3. Module Lifecycle

| Req ID | Requirement | SRS Ref | SDD Ref | Implementation Files | Test IDs | Status | **VVP Ref** |
|--------|------------|---------|---------|---------------------|----------|--------|---------|
| REQ-AI-LC-001 | Init: null path returns INVALID_INPUT | SRS-AI-001 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-002: InitNullPath | Written | XPE-VVP-AI-001 §4.1 |
| REQ-AI-LC-001 | Init: valid path | SRS-AI-001 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-003: InitValidPath | Written | XPE-VVP-AI-001 §4.1 |
| REQ-AI-LC-001 | Init: null config uses defaults | SRS-AI-001 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-004: InitNullConfig | Written | XPE-VVP-AI-001 §4.1 |
| REQ-AI-LC-001 | Init: config JSON parsing | SRS-AI-001 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-005: InitConfigJson | Written | XPE-VVP-AI-001 §4.1 |
| REQ-AI-LC-001 | Init idempotent | SRS-AI-001 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-006: InitIdempotent | Written | XPE-VVP-AI-001 §4.1 |
| REQ-AI-LC-001 | Init/shutdown repeated cycle | SRS-AI-001 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-007: RepeatedCycle | Written | XPE-VVP-AI-001 §4.1 |
| REQ-AI-LC-002 | Shutdown without init safe | SRS-AI-003 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-008: ShutdownWithoutInit | Written | XPE-VVP-AI-001 §4.1 |
| REQ-AI-LC-002 | Shutdown idempotent | SRS-AI-003 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-009: ShutdownIdempotent | Written | XPE-VVP-AI-001 §4.1 |
| REQ-AI-LC-002 | Shutdown repeated safe | SRS-AI-003 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-010: RepeatedShutdown | Written | XPE-VVP-AI-001 §4.1 |
| REQ-AI-LC-003 | Version non-null | SRS-AI-VER-001 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-011: VersionNonNull | Written | XPE-VVP-AI-001 §4.1 |
| REQ-AI-LC-003 | Version non-empty | SRS-AI-VER-001 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-012: VersionNonEmpty | Written | XPE-VVP-AI-001 §4.1 |
| REQ-AI-LC-003 | Version semver format | SRS-AI-VER-001 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-013: VersionSemver | Written | XPE-VVP-AI-001 §4.1 |
| REQ-AI-LC-003 | Version deterministic | SRS-AI-VER-001 | SDD Sec 4.1 | `modules/ai/src/ai.cpp` | TC-ABI-014: VersionDeterministic | Written | XPE-VVP-AI-001 §4.1 |

---

## 4. Body-Part Recognition (SWU-AI-02)

| Req ID | Requirement | SRS Ref | SDD Ref | Implementation Files | Test IDs | Status | **VVP Ref** |
|--------|------------|---------|---------|---------------------|----------|--------|---------|
| REQ-AI-BP-001 | Body-part recognize: not initialized | SRS-AI-010 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-007: NotInitialized | Written | XPE-VVP-AI-001 §4.1 |
| REQ-AI-BP-002 | Null image returns INVALID_INPUT | SRS-AI-010-VAL | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-008: NullImage | Written | XPE-VVP-AI-001 §4.2 |
| REQ-AI-BP-002 | Null label returns INVALID_INPUT | SRS-AI-010-VAL | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-009: NullLabel | Written | XPE-VVP-AI-001 §4.2 |
| REQ-AI-BP-002 | Zero bufLen returns BUFFER_TOO_SMALL | SRS-AI-010-VAL | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-010: ZeroBufLen | Written | XPE-VVP-AI-001 §4.2 |
| REQ-AI-BP-002 | Null confidence pointer handled | SRS-AI-010-VAL | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-011: NullConf | Written | XPE-VVP-AI-001 §4.2 |
| REQ-AI-BP-002 | Invalid buffer returns BUFFER_TOO_SMALL | SRS-AI-010-VAL | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-012: InvalidBuffer | Written | XPE-VVP-AI-001 §4.2 |
| REQ-AI-BP-001 | Stub fallback returns PROCESSING_FAILED | SRS-AI-010 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-013: StubFallback | Written | XPE-VVP-AI-001 §4.3 |
| REQ-AI-BP-001 | Stub returns label and confidence | SRS-AI-010 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-014: StubLabelConf | Written | XPE-VVP-AI-001 §4.3 |

---

## 5. Image Stitching (SWU-AI-03)

| Req ID | Requirement | SRS Ref | SDD Ref | Implementation Files | Test IDs | Status | **VVP Ref** |
|--------|------------|---------|---------|---------------------|----------|--------|---------|
| REQ-AI-ST-001 | Stitch: not initialized | SRS-AI-020 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-015: NotInitialized | Written | XPE-VVP-AI-001 §4.1 |
| REQ-AI-ST-001 | Null parts returns INVALID_INPUT | SRS-AI-020 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-016: NullParts | Written | XPE-VVP-AI-001 §4.2 |
| REQ-AI-ST-001 | Count=1 returns INVALID_INPUT | SRS-AI-020 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-017: SinglePart | Written | XPE-VVP-AI-001 §4.2 |
| REQ-AI-ST-001 | Null output returns INVALID_INPUT | SRS-AI-020 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-018: NullOutput | Written | XPE-VVP-AI-001 §4.2 |
| REQ-AI-ST-001 | Null output data returns INVALID_INPUT | SRS-AI-020 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-019: NullOutputData | Written | XPE-VVP-AI-001 §4.2 |

---

## 6. Stitch Size Estimation (SWU-AI-04)

| Req ID | Requirement | SRS Ref | SDD Ref | Implementation Files | Test IDs | Status | **VVP Ref** |
|--------|------------|---------|---------|---------------------|----------|--------|---------|
| REQ-AI-ST-002 | Deterministic output | SRS-AI-020-EST | SDD Sec 4.4 | `modules/ai/src/ai.cpp` | TC-ABI-015: DeterministicSize | Written | XPE-VVP-AI-001 §4.5 |
| REQ-AI-ST-002 | Valid dimensions | SRS-AI-020-EST | SDD Sec 4.4 | `modules/ai/src/ai.cpp` | TC-ABI-016: ValidDims | Written | XPE-VVP-AI-001 §4.5 |
| REQ-AI-ST-002 | Null parts returns INVALID_INPUT | SRS-AI-020-EST | SDD Sec 4.4 | `modules/ai/src/ai.cpp` | TC-ABI-017: NullParts | Written | XPE-VVP-AI-001 §4.5 |
| REQ-AI-ST-002 | Null output returns INVALID_INPUT | SRS-AI-020-EST | SDD Sec 4.4 | `modules/ai/src/ai.cpp` | TC-ABI-018: NullOutputs | Written | XPE-VVP-AI-001 §4.5 |
| REQ-AI-ST-002 | Single part handling | SRS-AI-020-EST | SDD Sec 4.4 | `modules/ai/src/ai.cpp` | TC-ABI-019: SinglePart | Written | XPE-VVP-AI-001 §4.5 |
| REQ-AI-ST-002 | 4096 clamp applied | SRS-AI-020-EST | SDD Sec 4.4 | `modules/ai/src/ai.cpp` | TC-ABI-020: Clamp4096 | Written | XPE-VVP-AI-001 §4.5 |

---

## 7. Bone Suppression (SWU-AI-05)

| Req ID | Requirement | SRS Ref | SDD Ref | Implementation Files | Test IDs | Status | **VVP Ref** |
|--------|------------|---------|---------|---------------------|----------|--------|---------|
| REQ-AI-BS-001 | Not initialized | SRS-AI-030 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-020: NotInitialized | Written | XPE-VVP-AI-001 §4.1 |
| REQ-AI-BS-001 | Null image returns INVALID_INPUT | SRS-AI-030 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-021: NullImage | Written | XPE-VVP-AI-001 §4.2 |
| REQ-AI-BS-001 | Null output returns INVALID_INPUT | SRS-AI-030 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-022: NullOutput | Written | XPE-VVP-AI-001 §4.2 |
| REQ-AI-BS-001 | Dimension mismatch | SRS-AI-030 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-023: DimMismatch | Written | XPE-VVP-AI-001 §4.2 |

---

## 8. DL Denoising (SWU-AI-06)

| Req ID | Requirement | SRS Ref | SDD Ref | Implementation Files | Test IDs | Status | **VVP Ref** |
|--------|------------|---------|---------|---------------------|----------|--------|---------|
| REQ-AI-DN-001 | Not initialized | SRS-AI-040 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-020: NotInitialized | Written | XPE-VVP-AI-001 §4.1 |
| REQ-AI-DN-001 | Null image returns INVALID_INPUT | SRS-AI-040 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-021: NullImage | Written | XPE-VVP-AI-001 §4.2 |
| REQ-AI-DN-001 | Null meta returns INVALID_INPUT | SRS-AI-040 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-022: NullMeta | Written | XPE-VVP-AI-001 §4.2 |

---

## 9. Model Card API (SWU-AI-07)

| Req ID | Requirement | SRS Ref | SDD Ref | Implementation Files | Test IDs | Status | **VVP Ref** |
|--------|------------|---------|---------|---------------------|----------|--------|---------|
| REQ-AI-MC-001 | Known model: model_id field | SRS-AI-MC-001 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-001: ModelId | Written | XPE-VVP-AI-001 §4.6 |
| REQ-AI-MC-001 | Known model: version field | SRS-AI-MC-001 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-002: Version | Written | XPE-VVP-AI-001 §4.6 |
| REQ-AI-MC-001 | Known model: pccp_status field | SRS-AI-MC-001 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-003: PccpStatus | Written | XPE-VVP-AI-001 §4.6 |
| REQ-AI-MC-001 | Known model: intended_use field | SRS-AI-MC-001 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-004: IntendedUse | Written | XPE-VVP-AI-001 §4.6 |
| REQ-AI-MC-001 | Known model: training_data_summary | SRS-AI-MC-001 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-005: TrainingDataSummary | Written | XPE-VVP-AI-001 §4.6 |
| REQ-AI-MC-001 | Known model: demographic_perf | SRS-AI-MC-001 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-006: DemographicPerf | Written | XPE-VVP-AI-001 §4.6 |
| REQ-AI-MC-001 | Known model: limitations field | SRS-AI-MC-001 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-007: Limitations | Written | XPE-VVP-AI-001 §4.6 |
| REQ-AI-MC-001 | Known model: published_date field | SRS-AI-MC-001 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-008: PublishedDate | Written | XPE-VVP-AI-001 §4.6 |
| REQ-AI-MC-001 | Different model returns different card | SRS-AI-MC-001 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-009~011: DifferentModels | Written | XPE-VVP-AI-001 §4.6 |
| REQ-AI-MC-001 | Unknown model returns IO_FAILED | SRS-AI-MC-001 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-012~014: UnknownModel | Written | XPE-VVP-AI-001 §4.6 |
| REQ-AI-MC-002 | Null modelId returns INVALID_INPUT | SRS-AI-MC-002 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-015: NullModelId | Written | XPE-VVP-AI-001 §4.6 |
| REQ-AI-MC-002 | Null buffer returns INVALID_INPUT | SRS-AI-MC-002 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-016: NullBuffer | Written | XPE-VVP-AI-001 §4.6 |
| REQ-AI-MC-002 | Small buffer returns BUFFER_TOO_SMALL | SRS-AI-MC-002 | SDD Sec 4.5 | `modules/ai/src/ai.cpp` | TC-MODELCARD-017: BufferTooSmall | Written | XPE-VVP-AI-001 §4.6 |

---

## 10. Fallback Router (SWU-AI-08)

| Req ID | Requirement | SRS Ref | SDD Ref | Implementation Files | Test IDs | Status | **VVP Ref** |
|--------|------------|---------|---------|---------------------|----------|--------|---------|
| REQ-AI-FB-002 | Enable fallback mode | SRS-AI-FB-002 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-001: EnableFallback | Written | XPE-VVP-AI-001 §4.8 |
| REQ-AI-FB-002 | Disable fallback mode | SRS-AI-FB-002 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-002: DisableFallback | Written | XPE-VVP-AI-001 §4.8 |
| REQ-AI-FB-002 | Repeated toggle | SRS-AI-FB-002 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-003: RepeatedToggle | Written | XPE-VVP-AI-001 §4.8 |
| REQ-AI-FB-002 | Non-zero values enable | SRS-AI-FB-002 | SDD Sec 4.3 | `modules/ai/src/ai.cpp` | TC-FALLBACK-004: NonZeroEnables | Written | XPE-VVP-AI-001 §4.8 |
| REQ-AI-FB-001 | Confidence threshold default (0.6) | SRS-AI-FB-001 | SDD Sec 4.2 | `modules/ai/src/ai.cpp` | TC-FALLBACK-022: ConfThresholdDefault | Written | XPE-VVP-AI-001 §4.8 |

---

## 11. Thread Safety

| Req ID | Requirement | SRS Ref | SDD Ref | Implementation Files | Test IDs | Status | **VVP Ref** |
|--------|------------|---------|---------|---------------------|----------|--------|---------|
| REQ-AI-001 | Concurrent bodypart_recognize (4 threads, 100 calls) | SRS-AI-ARCH-001 | SDD Sec 7.2 | `modules/ai/src/ai.cpp` | TC-WORKER-005~007: ConcurrentBodyPart | Written | XPE-VVP-AI-001 §4.7 |
| REQ-AI-002 | Concurrent set_fallback_mode (4 threads, 400 calls) | SRS-AI-ARCH-002 | SDD Sec 7.2 | `modules/ai/src/ai.cpp` | TC-WORKER-008~010: ConcurrentFallback | Written | XPE-VVP-AI-001 §4.7 |

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

Counts below are the actual `TEST` / `TEST_F` macro counts in `modules/ai/tests/`, enumerated
2026-09-10. They supersede the 4-file / 78-case figures carried in v0.1.0, which predated
`test_ai_ipc_bridge.cpp`, `test_ai_model_versioning.cpp`, and the suites added by QA-B-19~B-22.

| Area | Cases | Verification section |
|------|------:|----------------------|
| C ABI: version / init / shutdown | 12 | `XPE-VVP-AI-001 §4.1` |
| Not-initialized guards (6 entry points) | 6 | `§4.1` |
| Stitch size estimation | 7 | `§4.5` |
| Fallback-mode toggle + confidence threshold | 6 | `§4.8` |
| Stub inference returns (4 entry points) | 6 | `§4.3`, `§4.4` |
| Input validation | 14 | `§4.2` |
| Error-code precedence (#119) | 10 | `§4.2` |
| `dataSize` guard (#123, QA-B-21) | 10 | `§4.2` |
| 1000-cycle endurance (G3, #105) | 1 | `§3.3.4` |
| Worker isolation: stub fallback, thread safety, protocol constants, cycling | 16 | `§4.7` |
| Model Card API | 20 | `§4.6` |
| Model versioning / metadata | 11 | `§4.6` |
| IPC bridge (named pipe, negative paths only) | 10 | `§4.7` |
| **Total** | **129** | — |

Requirement coverage is unchanged from v0.1.0: **22 of 26** SPEC requirements have at least one
test; 4 are deferred (REQ-AI-004, REQ-AI-006, REQ-AI-007, REQ-AI-009 — see §12). Requirement counts
were not re-derived in this revision; only the test-case counts were re-measured.

### Test File Summary

| Test File | Cases | Focus |
|-----------|------:|-------|
| `test_ai_abi.cpp` | 25 | C ABI boundary: version, init, shutdown, stitch_estimate_size, not-initialized guards |
| `test_ai_fallback.cpp` | 47 | Fallback routing, stub behaviour, input validation, error precedence, `dataSize` guard, endurance |
| `test_ai_model_card.cpp` | 20 | Model Card JSON API, schema validation, buffer handling |
| `test_ai_worker_isolation.cpp` | 16 | Stub fallback, thread safety, protocol constants, init/shutdown cycling |
| `test_ai_model_versioning.cpp` | 11 | Semver, PCCP scope, training-data hash, validation metrics |
| `test_ai_ipc_bridge.cpp` | 10 | Named-pipe transport: creation, connect timeout, frame validation, destruction |
| **Total (6 files)** | **129** | |

All six files are registered in `modules/ai/CMakeLists.txt` (`XPE_AI_TEST_SOURCES`) and build into
the `xpe_ai_tests` target. Execution record: 129/129 pass, Lane B worktree, 2026-09-10
(`XPE-VVP-AI-001 §3.3.3`). No coverage figure exists for this module — `BUILD_AI=OFF` in every
coverage preset (issue #124).

---

## 14. Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1.0 | 2026-04-22 | xpe-docs | Initial RTM for skeleton implementation. 78 test cases covering REQ-AI-001~005, 008, 010~012. |
| 0.2.0 | 2026-09-10 | xpe-docs (issue #59) | Added a **VVP Ref** column to §2–§11. All 67 requirement rows are `—`: no VVP addendum exists for `xpe_ai`, so no verification-plan section covers any AI requirement. Recorded as an explicit gap rather than invented coverage. |
| 0.3.0 | 2026-09-10 | xpe-docs (issue #125) | **VVP Ref** filled for 64 of 67 rows against the new `XPE-VVP-AI-001` v1.0.0 (`docs/project/vvp_ai.md`). The 3 rows still `—` (REQ-AI-004 sidecar metadata, REQ-AI-006 ONNX Runtime multi-EP, REQ-AI-007 model signing) have no test case at all — their Test IDs column already reads `--`. §13 re-measured: the v0.1.0 "4 files / 78 cases" figures were stale; the actual inventory is **6 files / 129 `TEST`/`TEST_F` cases** (counted 2026-09-10), with `test_ai_ipc_bridge.cpp` (10) and `test_ai_model_versioning.cpp` (11) missing entirely from the old table and the four listed files undercounted. Added the execution record (129/129, Lane B worktree, 2026-09-10) and the coverage-absence note (#124). |

---

*This document satisfies IEC 62304 Class B requirements for requirements traceability (Section 5.7).*
