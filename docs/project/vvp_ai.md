# Validation and Verification Plan for xpe_ai.dll

**Document ID**: XPE-VVP-AI-001  
**Version**: 1.0.0  
**Date**: 2026-09-10  
**Status**: Controlled Draft  
**Classification**: Internal / IEC 62304 Compliance  
**Safety Classification**: IEC 62304 Class B  
**Module**: xpe_ai.dll (**STUB build** — see §1.2)  
**Parent Specification**: SPEC-XPE-P3-AI v1.1  
**Related Documents**:
  - Software Requirements Specification (SRS): `docs/project/srs_ai.md` (SRS-AI-001 v0.1.0)
  - Software Design Description (SDD): `docs/project/sdd_ai.md` (SDD-AI-001 v0.1.0)
  - Requirements Traceability Matrix (RTM): `docs/project/rtm_ai.md` (RTM-AI-001)
  - AI Regulatory Strategy: `docs/project/XPE-AI-REG-001_AI_Regulatory_Strategy.md`
  - System V&V Plan: `docs/project/XPE-SVVP-001_System_Verification_Validation_Plan.md`
  - Parent V&V Plan: `docs/post-processing/xpe/XPE-VVP-001_Verification_Validation_Plan.md`

---

## 1. Introduction

### 1.1 Purpose

This document defines the verification and validation (V&V) strategy for `xpe_ai.dll`, the XPE AI
Inference Module (Phase 3, Layer 1). It is written against the module **as it exists today**, not
against the Phase 3 target. Its purpose is therefore twofold:

1. Record the verification actually performed on the shipped stub — the C ABI contract, the input
   validation, the deterministic fallback path, the model-card API, and the worker IPC bridge.
2. Record, explicitly and without euphemism, the verification that **has not** been performed —
   above all the inference path itself, which does not exist in this build.

### 1.2 Scope — and the stub boundary

`xpe_ai.dll` is built in **stub mode**. `modules/ai/CMakeLists.txt:27-28` declares
`XPE_AI_USE_ONNXRUNTIME` default `OFF` and `XPE_AI_STUB_BUILD` default `ON`; every project preset
builds it that way. In consequence, each inference entry point performs its argument validation and
then returns `XPE_ERR_PROCESSING_FAILED` without running a model:

| Entry point | Source anchor | Terminal return in stub mode |
|---|---|---|
| `xpe_bodypart_recognize` | `modules/ai/src/ai.cpp:385` | `XPE_ERR_PROCESSING_FAILED` (after writing `"UNKNOWN"` / confidence `0.0f`) |
| `xpe_stitch_images` | `modules/ai/src/ai.cpp:419` | `XPE_ERR_PROCESSING_FAILED` |
| `xpe_bone_suppress` | `modules/ai/src/ai.cpp:501` | `XPE_ERR_PROCESSING_FAILED` |
| `xpe_dl_denoise` | `modules/ai/src/ai.cpp:531` | `XPE_ERR_PROCESSING_FAILED` |

Two entry points are **not** stubs and produce real results: `xpe_stitch_estimate_size`
(`ai.cpp:422-465`, a deterministic overlap heuristic clamped to 4096) and `xpe_ai_get_model_card`
(`ai.cpp:534`).

This plan covers:

- **Module**: xpe_ai.dll (native C++ DLL) + `xpe_ai_worker.exe` (built, not exercised end-to-end)
- **Software Units**: SWU-AI-01 (Module Lifecycle), SWU-AI-02 (Body-Part Recognition),
  SWU-AI-03 (Image Stitching), SWU-AI-04 (Stitch Size Estimation), SWU-AI-05 (Bone Suppression),
  SWU-AI-06 (DL Denoising), SWU-AI-07 (Model Card API), SWU-AI-08 (Fallback Router)
  — unit list per `docs/project/srs_ai.md:34-41`
- **Unit Test Inventory**: 129 `TEST`/`TEST_F` cases across 6 files in `modules/ai/tests/`
  (counted 2026-09-10; see §3.1)
- **Test binary**: `xpe_ai_tests`, declared in `modules/ai/CMakeLists.txt` (test sources live in
  `modules/ai/tests/` (moved from `modules/ai/tests/` by QA-B-33, 2026-09-10); the IPC bridge implementation is compiled directly
  into the test binary because it is not exported from the DLL)
- **Coverage**: **none measured** — see §3.3
- **Classification**: IEC 62304 Class B medical device software

### 1.3 Referenced Documents

| Document ID | Title | Version | Status |
|-------------|-------|---------|--------|
| SPEC-XPE-P3-AI | AI Inference Module Specification | 1.1 | Draft |
| SRS-AI-001 | Software Requirements Specification (AI) | 0.1.0 | Draft (Skeleton) |
| SDD-AI-001 | Software Design Description (AI) | 0.1.0 | Draft (Skeleton) |
| RTM-AI-001 | Requirements Traceability Matrix (AI) | 0.2.0 | Draft (Skeleton) |
| XPE-VVP-001 | XPE Verification and Validation Plan (parent) | 1.3 | Controlled Draft |
| XPE-AI-REG-001 | AI Regulatory Strategy | — | see file |
| IEC 62304:2006 | Medical Device Software Lifecycle Processes | +A1:2015 | Normative |

---

## 2. V&V Strategy

### 2.1 Six-Level Verification and Validation Hierarchy

The XPE-SVVP-001 six-level framework applies to this module, but only the first two levels currently
carry evidence. The table states the level, and states plainly where no evidence exists.

| Level | Scope | Evidence in this build | Responsibility |
|-------|-------|------------------------|-----------------|
| **L1** | Unit Verification | 129 unit-test cases (§3.1) covering ABI, validation, fallback, model card, versioning, IPC bridge | Developer + QA |
| **L2** | Integration Verification | Partial — IPC bridge negative paths only (§4.7). No worker round-trip, no P/Invoke marshalling suite | Integration QA |
| **L3** | System Verification | **None.** `xpe_ai` is in no CI pipeline preset and no end-to-end pipeline test | System QA |
| **L4** | Feature Verification | **None.** Algorithm validation is impossible without an inference implementation | Algorithm QA |
| **L5** | Validation (Clinical) | **None.** Deferred to Phase 3 (see XPE-AI-REG-001) | Clinical Review |
| **L6** | Field Performance | **None.** Module is not released | Field QA |

### 2.2 V&V Principles

1. **Verify the contract, not the intent**: in stub mode the only verifiable behaviour is the C ABI
   contract — argument validation, error-code precedence, buffer discipline, lifecycle. Tests assert
   that contract and nothing beyond it.
2. **A stub failure is a specified result**: `XPE_ERR_PROCESSING_FAILED` from an inference entry
   point is the *specified* stub behaviour (REQ-AI-002 deterministic fallback routing). Tests assert
   it as a pass criterion, and callers are required to fall back deterministically.
3. **Error-code precedence is normative**: required-pointer NULL checks run before the
   initialisation guard (`ai.cpp:348-357`, api-spec precedence contract, issue #119). This ordering
   is itself verified (§4.2).
4. **No fabricated evidence**: no section of this plan claims a measurement that has not been taken.
   Absent measurements are named in §3.3 and §10.
5. **Opt-in by default**: the module defaults to off; verification must confirm that a caller that
   never initialises it observes `XPE_ERR_NOT_INITIALIZED`, not a crash (§4.1).

### 2.3 IEC 62304 Clause Mapping of the Six-Level Hierarchy

The six V&V levels of §2.1 are a project-internal decomposition. This subsection maps them onto the
normative IEC 62304:2006+A1:2015 clauses so that each level has an identified regulatory home.
Class B requires 5.5, 5.6 and 5.7 in full; 5.8 (release) and 6.x (maintenance) are referenced for the
post-release levels.

| Level (§2.1) | IEC 62304 Clause | Clause Title | Evidence Produced by This Plan |
|---|---|---|---|
| **L1** Unit Verification | **5.5.2 / 5.5.3 / 5.5.5** | Software unit verification process, acceptance criteria, unit verification | §3.1 unit-test inventory, §4.1–§4.8 unit-test suites |
| **L2** Integration Verification | **5.6.1 / 5.6.2 / 5.6.3** | Integrate software units, verify integration, test integrated software | §4.7 IPC bridge negative-path tests — **partial**, gap recorded in §10 |
| **L3** System Verification | **5.7.1 / 5.7.4 / 5.7.5** | Establish tests for software requirements, verify test procedures, test record contents | **No evidence** — recorded as a gap (§10) |
| **L4** Feature Verification | **5.7.1** (algorithm-level system test) | Establish tests for software requirements | **No evidence** — no inference implementation (§10) |
| **L5** Validation (Clinical) | **5.7.1 + 5.8.x** | System testing feeding software release | **No evidence** — deferred, XPE-AI-REG-001 |
| **L6** Field Performance | **6.1 / 6.2 (maintenance), 5.6.4 (regression)** | Software maintenance process, regression testing | **No evidence** — module not released |

Cross-cutting clauses applied where evidence exists: **5.5.4** (unit acceptance criteria applied
before integration), **5.6.5** (test record contents), **5.6.6 / 5.7.2** (problem resolution),
**5.6.7** (test-procedure verification), **5.7.3** (retest after change), and **7.3.3** (risk-control
verification traceability, recorded in `docs/project/rtm_ai.md`).

Because L3–L6 carry no evidence, **IEC 62304 §5.7 is not satisfied for this module** and it is not
release-eligible. This plan records that state; it does not close it.

---

## 3. Test Coverage Requirements

### 3.1 Unit Test Inventory

Counts below are the actual `TEST` / `TEST_F` macro counts in `modules/ai/tests/`, enumerated
2026-09-10. They supersede the 4-file / 78-case figures previously carried in RTM-AI-001 v0.1.0.

| Area | Test File | Cases | Primary SWU |
|------|-----------|------:|-------------|
| C ABI boundary | `test_ai_abi.cpp` | 25 | SWU-AI-01, SWU-AI-04 |
| Fallback, validation, error precedence, dataSize guard, endurance | `test_ai_fallback.cpp` | 47 | SWU-AI-02/03/05/06/08 |
| Worker isolation, thread safety, protocol constants | `test_ai_worker_isolation.cpp` | 16 | SWU-AI-01, SWU-AI-08 |
| Model Card API | `test_ai_model_card.cpp` | 20 | SWU-AI-07 |
| Model versioning / metadata | `test_ai_model_versioning.cpp` | 11 | SWU-AI-07 |
| IPC bridge (named pipe) | `test_ai_ipc_bridge.cpp` | 10 | SWU-AI-01 (worker transport) |
| **Total** | **6 files** | **129** | — |

### 3.2 Test Organization

Actual file inventory with per-file case counts and the fixture/suite names, 6 files, 129 cases:

```
modules/ai/tests/
  test_ai_abi.cpp                 (25)  AiAbi
                                        - version (4), init (6), shutdown (2),
                                          stitch_estimate_size (7), not-initialized guards (6)
  test_ai_fallback.cpp            (47)  AiFallbackTest, AiErrorPrecedenceTest, AiEndurance
                                        - fallback-mode toggle (4), stub returns (6),
                                          confidence threshold (2), input validation (14),
                                          error precedence (10), dataSize guard (10),
                                          1000-cycle endurance (1)
  test_ai_worker_isolation.cpp    (16)  AiWorkerIsolationTest
                                        - stub graceful fallback (5), thread safety (2),
                                          protocol constants (7), init/shutdown cycling (2)
  test_ai_model_card.cpp          (20)  AiModelCardTest
  test_ai_model_versioning.cpp    (11)  AiModelVersioningTest
  test_ai_ipc_bridge.cpp          (10)  AiIpcBridgeTest

Total: 129 TEST/TEST_F cases
```

All six files are registered in `modules/ai/CMakeLists.txt` (`XPE_AI_TEST_SOURCES`) and built into
the single `xpe_ai_tests` binary. The CMake registration carries
`LABELS "unit;ai;SPEC-XPE-P3-AI"` and `TIMEOUT 30`; the semicolons are escaped because an
unescaped list collapses to its first element (the defect and its fix are recorded in the
`gtest_discover_tests` comment block, issue #113 / QA-B-07).

### 3.3 Coverage Metrics — measured state

#### 3.3.1 The gate

| Metric | Threshold | Measurement Method |
|--------|-----------|-------------------|
| Line Coverage (gate) | >= 85% | `XPE_COVERAGE_MIN` = 0.85 per DLL, checked by the `coverage_check` target (`cmake/XpeCoverage.cmake:22`, REQ-P0-006) |

#### 3.3.2 Measured coverage for `xpe_ai`: **none**

This is a measurement of absence, not a target. `xpe_ai` is built in **no coverage preset**:

| Preset | `BUILD_AI` | Source |
|--------|-----------|--------|
| `coverage` | `OFF` | `CMakePresets.json` (`coverage` cacheVariables) |
| `coverage-post` | `OFF` | `CMakePresets.json` (`coverage-post` cacheVariables) |
| `ci-fullstack` | `OFF` | `CMakePresets.json` (`ci-fullstack` cacheVariables) |

Consequently **no line-rate figure exists for `xpe_ai`**, the 0.85 gate has never been evaluated
against it, and the CI coverage run `34414537575` (2026-09-10) that produced the `coverage` 0.649
and `coverage-post` 0.898 figures did not include this module. The same absence is recorded in
`XPE-VVP-001` §Verification Gate Status ("xpe_ai — 미측정"; xpe_dicom 은 2026-09-10 coverage-dicom 으로 첫 측정 0.696). Closing it is tracked as
**issue #124**.

No statement anywhere in this plan may be read as asserting a coverage percentage for `xpe_ai`.

#### 3.3.3 Test execution evidence

The 129 cases are executed by lane QA under a locally-defined `ci-ai` configure preset. This preset
is **not** present in the repository `CMakePresets.json`; it exists only in the Lane B worktree, so
the execution is reproducible by that lane and not by a clean clone. That is itself a gap (§10).

Recorded results (leader verdicts in `.moai/lanes/post/inbox/`, all 2026-09-10):

| Card | Recorded ai-suite result | Verdict line |
|------|--------------------------|--------------|
| QA-B-22 (#105) | `129` | `[leader 판정 · 2026-09-10] PASS — 커밋 496211b`, `_verify.log 443/129/48` |
| QA-B-23 (#105) | `129` | `[leader 판정 · 2026-09-10] PASS — 커밋 149384d`, `_verify.log 444/129/48` |
| QA-B-24 (#111) | `129/129` | `[leader 판정 · 2026-09-10] PASS — 커밋 4d80b6b`, `_verify.log ... 129/129, 48/48` |

The claim this plan makes is exactly the claim those records support: **129 of 129 cases passed in
the Lane B worktree on 2026-09-10**. No CI run of this suite exists.

#### 3.3.4 Memory-leak gate (G3)

Gate G3 (issue #105, closed 2026-09-10) applies to all 7 modules, `xpe_ai` included. Its case in
this module is `AiEndurance.ThousandCycles_MemoryGrowthUnderOneMB`
(`modules/ai/tests/test_ai_fallback.cpp:546`); `psapi` is linked into `xpe_ai_tests` for
`GetProcessMemoryInfo` (`modules/ai/CMakeLists.txt`, comment cites #105 G3).

| Step | Definition |
|------|------------|
| Warm-up | 100 process cycles executed and discarded |
| Baseline | Working-set size sampled after warm-up |
| Measurement | 1000 further process cycles |
| Pass criterion | Working-set growth over the 1000 cycles < 1 MB |
| Sensitivity probe | A deliberate 4096 B/cycle leak is injected and must be detected by the same harness |

Module-specific note recorded in the QA-B-22 card: because `xpe_ai` is a stub, the cycle exercised is
the `PROCESSING_FAILED` path — init/shutdown and validation allocations are covered; an inference
allocation path is not, because none exists.

#### 3.3.5 AddressSanitizer status

ASan was **not** run against `xpe_ai`. The QA-B-21 leader verdict (2026-09-10) records the gap
explicitly: "Gap 접수: ai ASan(스텁, 소스 판독)" — the ai guards were verified by source reading and
by RED→GREEN test transition rather than by a sanitizer run, on the reasoning that the stub does not
read pixel data before the guard. Residual risk is assessed there as low; it is not zero, and it is
carried into §10.

---

## 4. Verification Methods per SWU

### 4.1 SWU-AI-01: Module Lifecycle

**Purpose**: init / shutdown / version, and the not-initialized guard on every other entry point.

**Requirements Addressed**: REQ-AI-LC-001..003, REQ-AI-005 (opt-in, default off), REQ-AI-001

#### Verification Method V4.1.1: Unit Test Suite (L1)

| Group | Cases | Assertions |
|---|---:|---|
| Version | 4 | non-null, non-empty, semantic-version format, deterministic across calls (`AiAbi.Version*`) |
| Init | 6 | null model dir → `INVALID_INPUT`; valid path → OK; null config → defaults; config JSON accepted; idempotent; repeated init/shutdown cycle |
| Shutdown | 2 | shutdown without init is a no-op; shutdown is idempotent |
| Not-initialized guard | 6 | `bodypart_recognize`, `stitch_images`, `bone_suppress`, `dl_denoise`, `get_model_card`, `set_fallback_mode` each return `NOT_INITIALIZED` before init |
| Cycling robustness | 2 | `AiWorkerIsolationTest.RapidInitShutdownCycleDoesNotCrash`, `.InitShutdownWithInferenceInBetween` |

**Acceptance**: all 20 cases pass; no crash on any ordering of init/shutdown.

#### RTM Cross-Reference (SWU-AI-01)

Traced in `docs/project/rtm_ai.md` **§3 Module Lifecycle** (13 rows, REQ-AI-LC-001..003) and
**§2 Architecture Principles** rows REQ-AI-001 and REQ-AI-005. Those rows carry
`XPE-VVP-AI-001 §4.1` in their **VVP Ref** column; the link is bidirectional and any change to this
section requires the matching RTM rows to be re-checked.

---

### 4.2 Cross-cutting: Input validation, error-code precedence, and the dataSize guard

**Purpose**: verify the C ABI argument contract shared by every inference entry point.

**Requirements Addressed**: REQ-AI-BP-002, REQ-AI-ST-001, REQ-AI-BS-001, REQ-AI-DN-001,
REQ-AI-090~093 (adversarial robustness — **partial**: null and size checks only)

#### Verification Method V4.2.1: Input Validation (L1)

14 cases in `test_ai_fallback.cpp` assert per-entry-point rejection: null image, null output, null
label, null metadata, zero buffer length (`BUFFER_TOO_SMALL`), null confidence pointer (accepted),
part count of one, dimension mismatch, invalid buffer.

#### Verification Method V4.2.2: Error-Code Precedence (L1)

10 `AiErrorPrecedenceTest` cases assert the ordering fixed by issue #119: a required-pointer NULL
outranks `NOT_INITIALIZED`, and valid arguments on an uninitialised module reach `NOT_INITIALIZED`.
Both directions are asserted for `bodypart_recognize`, `stitch_images`, `bone_suppress` and
`get_model_card`. Implementation anchor: `modules/ai/src/ai.cpp:348-357`.

#### Verification Method V4.2.3: `dataSize` Guard (L1) — added by QA-B-21 (#123)

10 cases, `AiFallbackTest.DataSizeGuard_*`, five entry points × two directions:

| Entry point | Short `dataSize` | `dataSize == 0` |
|---|---|---|
| `xpe_bodypart_recognize` | `..._ShortReturnsInvalid` → `INVALID_INPUT` | `..._ZeroAccepted` → passes the gate |
| `xpe_stitch_images` | `..._ShortReturnsInvalid` | `..._ZeroAccepted` |
| `xpe_stitch_estimate_size` | `..._ShortReturnsInvalid` | `..._ZeroAccepted` |
| `xpe_bone_suppress` | `..._ShortReturnsInvalid` | `..._ZeroAccepted` |
| `xpe_dl_denoise` | `..._ShortReturnsInvalid` | `..._ZeroAccepted` |

These cases exist because a guard that has never fired is not evidence. The QA-B-21 leader verdict
(2026-09-10, commit `fd18357`) records that the ai guard was proven by a RED transition — the ai
suite showed `[  FAILED  ]` on 5 cases before the fix — and that the second condition of
`validateImageBuffer` (`bpp != 0 && dataSize != 0`, `modules/ai/src/ai.cpp:165`) was found never to
have behaved correctly on the zero path until this card. The contract that `dataSize == 0` means
*unspecified* and must be **accepted** is stated at `modules/ai/src/ai.cpp:150-152` and in
`docs/project/api-spec.md`.

#### RTM Cross-Reference (cross-cutting)

Traced in `docs/project/rtm_ai.md` §4 (REQ-AI-BP-002 rows), §5 (REQ-AI-ST-001 rows), §7
(REQ-AI-BS-001 rows) and §8 (REQ-AI-DN-001 rows). Those rows carry `XPE-VVP-AI-001 §4.2`.

---

### 4.3 SWU-AI-02: Body-Part Recognition

**Purpose**: CNN body-part classification. **Not implemented** — stub.

**Requirements Addressed**: REQ-AI-BP-001, REQ-AI-BP-002

#### Verification Method V4.3.1: Stub-Contract Unit Tests (L1)

| Case | Assertion |
|---|---|
| `AiFallbackTest.BodypartRecognizeStubReturnsProcessingFailed` | returns `XPE_ERR_PROCESSING_FAILED` |
| `AiFallbackTest.BodypartRecognizeSetsConfidenceToZero` | `*confidenceOut == 0.0f` |
| `AiFallbackTest.BodypartRecognizeSetsUnknownLabel` | label buffer receives `"UNKNOWN"`, NUL-terminated |
| `AiWorkerIsolationTest.StubModeBodypartRecognizeFallsBackGracefully` | no crash, deterministic error |
| `AiWorkerIsolationTest.RepeatedFallbackIsConsistent` | repeated calls give the same result |
| + 5 validation cases (§4.2) | null/short-buffer rejection |

**Not verified**: classification accuracy, confidence calibration, demographic performance,
inference latency. None of these is measurable in this build (§4.9).

#### RTM Cross-Reference (SWU-AI-02)

`docs/project/rtm_ai.md` **§4 Body-Part Recognition** — REQ-AI-BP-001 rows carry
`XPE-VVP-AI-001 §4.3`; REQ-AI-BP-002 rows carry `§4.2`.

---

### 4.4 SWU-AI-03 / SWU-AI-05 / SWU-AI-06: Stitching, Bone Suppression, DL Denoising

**Purpose**: three further inference entry points. **All three are stubs.**

**Requirements Addressed**: REQ-AI-ST-001, REQ-AI-BS-001, REQ-AI-DN-001

#### Verification Method V4.4.1: Stub-Contract Unit Tests (L1)

| SWU | Stub-return case | Graceful-fallback case |
|---|---|---|
| SWU-AI-03 stitching | `AiFallbackTest.StitchImagesStubReturnsProcessingFailed` | `AiWorkerIsolationTest.StubModeStitchImagesFallsBackGracefully` |
| SWU-AI-05 bone suppression | `AiFallbackTest.BoneSuppressStubReturnsProcessingFailed` | `AiWorkerIsolationTest.StubModeBoneSuppressFallsBackGracefully` |
| SWU-AI-06 DL denoising | `AiFallbackTest.DlDenoiseStubReturnsProcessingFailed` | `AiWorkerIsolationTest.StubModeDlDenoiseFallsBackGracefully` |

Validation cases for these three entry points are covered by §4.2 (9 validation cases + 4 precedence
cases + 6 dataSize cases).

**Not verified**: stitch seam accuracy, bone-suppression image quality, denoising SNR — no
implementation exists.

#### RTM Cross-Reference

`docs/project/rtm_ai.md` **§5 Image Stitching**, **§7 Bone Suppression**, **§8 DL Denoising** —
stub-behaviour rows carry `XPE-VVP-AI-001 §4.4`, pure-validation rows carry `§4.2`.

---

### 4.5 SWU-AI-04: Stitch Size Estimation

**Purpose**: deterministic output-size estimate. **This unit is fully implemented** —
`modules/ai/src/ai.cpp:422-465`, a max-dimension × overlap-factor heuristic clamped to 4096.

**Requirements Addressed**: REQ-AI-ST-002

#### Verification Method V4.5.1: Unit Test Suite (L1)

| Case | Assertion | Acceptance |
|---|---|---|
| `AiAbi.StitchEstimateSizeDeterministic` | identical inputs give identical outputs | byte-identical |
| `AiAbi.StitchEstimateSizeReturnsValidDimensions` | width/height are non-zero and plausible | within bounds |
| `AiAbi.StitchEstimateSizeNullPartsReturnsInvalid` | null parts rejected | `INVALID_INPUT` |
| `AiAbi.StitchEstimateSizeNullOutputsReturnInvalid` | null width/height out rejected | `INVALID_INPUT` |
| `AiAbi.StitchEstimateSizeSinglePartReturnsInvalid` | `partCount < 2` rejected | `INVALID_INPUT` |
| `AiAbi.StitchEstimateSizeInvalidBufferReturnsInvalid` | malformed part buffer rejected | `INVALID_INPUT` |
| `AiAbi.StitchEstimateSizeClampsToMax4096` | oversize estimate clamped | `<= 4096` both axes |
| + 2 `DataSizeGuard_StitchEstimateSize_*` (§4.2) | short / zero `dataSize` | per §4.2 |

This is the one inference-adjacent unit whose behaviour is verified against its specified algorithm,
because the algorithm is deterministic and present.

#### RTM Cross-Reference (SWU-AI-04)

`docs/project/rtm_ai.md` **§6 Stitch Size Estimation** — all REQ-AI-ST-002 rows carry
`XPE-VVP-AI-001 §4.5`.

---

### 4.6 SWU-AI-07: Model Card API and Model Versioning

**Purpose**: transparency metadata for every model the module can host. **Implemented**
(`modules/ai/src/ai.cpp:534`).

**Requirements Addressed**: REQ-AI-MC-001, REQ-AI-MC-002, REQ-AI-008 (semver model versioning)

#### Verification Method V4.6.1: Model Card Schema (L1) — `test_ai_model_card.cpp`, 20 cases

| Group | Cases | Assertions |
|---|---:|---|
| Field presence | 8 | `model_id`, `model_version`, `intended_use`, `limitations`, `pccp_status`, `published_date`, `training_data_summary`, plus `GetModelCardForKnownModelReturnsOk` |
| JSON well-formedness | 1 | `GetModelCardOutputIsValidJson` |
| Per-model differentiation | 3 | stitch / bone-suppress / denoise models return distinct cards |
| Unknown model | 3 | returns `IO_FAILED`, still writes JSON, JSON contains an error field |
| Buffer discipline | 5 | null modelId, null buffer, zero size, small buffer (`BUFFER_TOO_SMALL`), exact-size buffer succeeds |

#### Verification Method V4.6.2: Model Versioning Metadata (L1) — `test_ai_model_versioning.cpp`, 11 cases

Semver-format version string, model id, PCCP scope, training-data hash, validation metrics, presence
of all required metadata fields, version comparability, unknown-model error, tiny-buffer
`BUFFER_TOO_SMALL`, null modelId and null buffer rejection.

**Not verified**: that a card's content corresponds to a model that exists and was actually trained
as described — there are no models in this build. Card content is static data.
Model **signing** (REQ-AI-007) is not implemented and not tested.

#### RTM Cross-Reference (SWU-AI-07)

`docs/project/rtm_ai.md` **§9 Model Card API** (REQ-AI-MC-001, REQ-AI-MC-002) and **§2** row
REQ-AI-008 — those rows carry `XPE-VVP-AI-001 §4.6`.

---

### 4.7 Worker isolation and the IPC bridge

**Purpose**: REQ-AI-003 requires inference to run in a sandboxed `xpe_ai_worker.exe` reached over a
named pipe. The worker builds; the transport is implemented (`modules/ai/src/ai_ipc_bridge.cpp`);
**no end-to-end round trip is exercised anywhere.**

**Requirements Addressed**: REQ-AI-003 (partial), REQ-AI-001 (thread safety)

#### Verification Method V4.7.1: IPC Bridge Negative Paths (L2) — `test_ai_ipc_bridge.cpp`, 10 cases

| Case | Assertion |
|---|---|
| `CreateBridge_ValidParameters_Succeeds` | handle created |
| `CreateBridge_NullPipeName_ReturnsNull` | null name rejected |
| `Connect_NoWorkerAvailable_ReturnsTimeoutError` | connect without a peer times out cleanly |
| `Send_NotConnected_ReturnsError` / `Receive_NotConnected_ReturnsError` | transport state enforced |
| `Send_InvalidMagicNumber_ReturnsInvalidInput` | frame magic validated |
| `Send_PayloadSizeExceedsMaximum_ReturnsInvalidInput` | payload bound enforced |
| `Receive_NoResponseWithinTimeout_ReturnsTimeoutError` | receive timeout honoured |
| `Destroy_NullBridge_DoesNotCrash` / `Destroy_MultipleCalls_Safe` | destruction is safe and idempotent |

Every case is a negative or lifecycle path. **There is no positive-path test**: no worker is
launched, no request is answered, no response is parsed.

#### Verification Method V4.7.2: Protocol Constants (L1) — 7 cases

`AiWorkerIsolationTest` asserts that the protocol version, message magic, default timeout, maximum
payload size, pipe buffer size, maximum model-id length and maximum body-part length are defined and
within sane bounds. These verify the header contract, not the transport.

#### Verification Method V4.7.3: Thread Safety (L1) — 2 cases

`ConcurrentBodypartRecognizeIsThreadSafe` (4 threads × 100 calls) and
`ConcurrentSetFallbackModeIsThreadSafe` (4 threads × 400 calls). Because the inference path is a
stub, these verify that the guards and the shared state are race-free, not that inference is.

#### RTM Cross-Reference

`docs/project/rtm_ai.md` **§2** row REQ-AI-003 and **§11 Thread Safety** — those rows carry
`XPE-VVP-AI-001 §4.7`.

---

### 4.8 SWU-AI-08: Fallback Router

**Purpose**: deterministic fallback when AI is unavailable or low-confidence.

**Requirements Addressed**: REQ-AI-002, REQ-AI-FB-001, REQ-AI-FB-002

#### Verification Method V4.8.1: Unit Test Suite (L1)

| Case | Assertion |
|---|---|
| `AiFallbackTest.SetFallbackModeEnableReturnsOk` / `...DisableReturnsOk` | both toggles accepted |
| `AiFallbackTest.SetFallbackModeToggleRepeated` | repeated toggling is stable |
| `AiFallbackTest.SetFallbackModeNonZeroEnables` | any non-zero value enables |
| `AiFallbackTest.ConfidenceThresholdDefaultIs06` | default threshold is 0.6 |
| `AiFallbackTest.ConfidenceThresholdInRange` | threshold stays in `[0, 1]` |
| `AiWorkerIsolationTest.RepeatedFallbackIsConsistent` | fallback decision is deterministic |
| `AiAbi.SetFallbackModeWithoutInitReturnsNotInitialized` | guarded before init |

**Not verified**: the confidence-driven branch of the router. The stub never produces a confidence
value from a model, so the "confidence below threshold" arm of REQ-AI-002 (`ai.cpp:370-373`,
comment) has never executed.

#### RTM Cross-Reference (SWU-AI-08)

`docs/project/rtm_ai.md` **§10 Fallback Router** and **§2** row REQ-AI-002 — those rows carry
`XPE-VVP-AI-001 §4.8`.

---

### 4.9 Explicitly NOT verified

This subsection is normative. Nothing below may be represented as verified.

| Area | Why no evidence exists |
|---|---|
| **Inference path (all models)** | Not implemented. Every inference entry point returns `XPE_ERR_PROCESSING_FAILED` before touching a model (§1.2). |
| **ONNX Runtime integration (REQ-AI-006)** | `XPE_AI_USE_ONNXRUNTIME` is `OFF` in every preset; `modules/ai/src/ai_onnx_session.cpp` is compiled but its runtime path is unreachable in stub mode. No execution provider (CPU/CUDA/TensorRT/DirectML) has been exercised. |
| **Worker round trip (REQ-AI-003)** | No test launches `xpe_ai_worker.exe`; only negative transport paths are covered (§4.7). |
| **Model signing (REQ-AI-007)** | Not implemented. |
| **Time-budget enforcement (REQ-AI-009)** | Not implemented; no latency measurement exists. |
| **Coverage** | `xpe_ai` is in no coverage preset (§3.3.2, issue #124). |
| **CI execution** | No CI preset builds or runs `xpe_ai_tests`; the `ci-ai` preset is worktree-local (§3.3.3). |
| **AddressSanitizer** | Not run against this module (§3.3.5). |
| **Clinical / algorithmic validation** | Requires an inference implementation; deferred to Phase 3 per XPE-AI-REG-001. |
| **Sidecar metadata (REQ-AI-004), XAI, conformal prediction, drift detection, PCCP boundary** | Not implemented; enumerated in `docs/project/rtm_ai.md` §12 Deferred Requirements. |

---

## 5. Validation Evidence

### 5.1 Test Execution and Results

| Evidence Type | Location | Acceptance Criteria | State 2026-09-10 |
|---|---|---|---|
| Google Test output (`xpe_ai_tests`) | Lane B worktree `_verify.log` (QA-B-22/23/24) | 129/129 pass | **Recorded as 129/129** (§3.3.3) |
| Code coverage report | — | >= 85% line-rate | **Not produced** (§3.3.2) |
| G3 endurance result | `AiEndurance.ThousandCycles_MemoryGrowthUnderOneMB` | < 1 MB working-set growth over 1000 cycles | Case present and included in the 129 (§3.3.4) |
| Benchmark output | — | — | **None** — no AI benchmark pack exists |
| Performance log | — | — | **None** — no latency budget is enforced |

### 5.2 Algorithm Validation Evidence

**None.** Algorithm validation for SWU-AI-02/03/05/06 is impossible against a stub and is deferred
in full to Phase 3.

### 5.3 Clinical Validation

Deferred. The regulatory pathway, PCCP framing and evidence expectations for AI functions are held
in `docs/project/XPE-AI-REG-001_AI_Regulatory_Strategy.md`. No clinical evidence exists for this
module and none is claimed.

---

## 6. IEC 62304 Traceability

### 6.1 Requirements-to-Test Traceability

The authoritative requirement → implementation → test matrix for this module is
`docs/project/rtm_ai.md` (RTM-AI-001). From RTM-AI-001 v0.3.0 each requirement row carries a
**VVP Ref** column naming the section of this document that verifies it, which closes the
IEC 62304 §5.7.4 SRS→test mapping loop in both directions **for the rows that have a test**.

Rows that remain `—` are rows for which no test exists in this build. They are not an omission of
this plan; they are the recorded verification gap of the module, and they are listed in
RTM-AI-001 §12 Deferred Requirements.

### 6.2 Quality Attributes Traceability

| Quality Attribute | TRUST 5 Pillar | Verification Method | State |
|---|---|---|---|
| Correctness | Tested | 129 unit-test cases (§3.1) | Contract only — no algorithm under test |
| Clarity | Readable | Code review, Doxygen comments | In place |
| Consistency | Unified | clang-format, C ABI conventions shared with the other six modules | In place |
| Security | Secured | Input validation, buffer bounds, `dataSize` guard (§4.2) | Partial — no ASan run (§3.3.5); model signing absent |
| Traceability | Trackable | Conventional commits, this V&V plan, RTM-AI-001 | In place |

---

## 7. Performance Validation

**No performance budget is verified for this module.** REQ-AI-009 (time-budget enforcement) is not
implemented (`docs/project/rtm_ai.md` §12), and a stub that returns before inference cannot produce a
meaningful latency figure. Any budget in SPEC-XPE-P3-AI applies to the Phase 3 implementation and
must be verified by an amended version of this plan at that time.

The timing-budget exclusion regex applied to coverage runs project-wide
(`XPE_COVERAGE_EXCLUDE_TESTS`, `cmake/XpeCoverage.cmake:29`) is moot here, because this module has no
coverage run and no timing tests.

---

## 8. Error Handling and Robustness

### 8.1 Error Code Verification

| Error Condition | Expected Return Code | Verified By |
|---|---|---|
| NULL required pointer (any entry point) | `XPE_ERR_INVALID_INPUT` | §4.2.1, §4.2.2 |
| Call before `xpe_ai_init` | `XPE_ERR_NOT_INITIALIZED` | §4.1 (6 guard cases) |
| NULL pointer **and** uninitialised | `XPE_ERR_INVALID_INPUT` (pointer outranks) | §4.2.2 (10 precedence cases, #119) |
| Zero output buffer length | `XPE_ERR_BUFFER_TOO_SMALL` | §4.2.1, §4.6.1 |
| Non-zero `dataSize` smaller than dimensions require | `XPE_ERR_INVALID_INPUT` | §4.2.3 (5 cases) |
| `dataSize == 0` (unspecified) | accepted | §4.2.3 (5 cases) |
| `dataSize` above 64 MB (`4096×4096×4`) | `XPE_ERR_INVALID_INPUT` | `modules/ai/src/ai.cpp:156` — **guard present, no dedicated case** |
| Unknown model id | `XPE_ERR_IO_FAILED` | §4.6.1 (3 cases) |
| Inference requested in stub mode | `XPE_ERR_PROCESSING_FAILED` | §4.3, §4.4 |

### 8.2 Graceful Degradation

| Failure Scenario | Expected Behaviour | Verified By |
|---|---|---|
| AI unavailable (stub build) | every inference entry point returns `PROCESSING_FAILED`; caller falls back deterministically | §4.3, §4.4 (`StubMode*FallsBackGracefully`) |
| Worker unreachable | bridge connect returns a timeout error rather than blocking or crashing | §4.7.1 |
| Repeated failure | result is consistent across calls | `AiWorkerIsolationTest.RepeatedFallbackIsConsistent` |
| Concurrent use during failure | no data race on module state | §4.7.3 |
| Low-confidence model result | fall back per REQ-AI-002 | **Not verified** — unreachable in stub mode (§4.8) |

---

## 9. Test Case Reference

### 9.1 Test File Locations

See §3.2 for the authoritative per-file inventory.

| Area | File | Cases |
|---|---|---:|
| C ABI | `modules/ai/tests/test_ai_abi.cpp` | 25 |
| Fallback / validation / precedence / dataSize / endurance | `modules/ai/tests/test_ai_fallback.cpp` | 47 |
| Model card | `modules/ai/tests/test_ai_model_card.cpp` | 20 |
| Worker isolation | `modules/ai/tests/test_ai_worker_isolation.cpp` | 16 |
| Model versioning | `modules/ai/tests/test_ai_model_versioning.cpp` | 11 |
| IPC bridge | `modules/ai/tests/test_ai_ipc_bridge.cpp` | 10 |
| **Total** | **6 files** | **129** |

### 9.2 Test Execution Command

The suite is built by `modules/ai/CMakeLists.txt` into the `xpe_ai_tests` target, which requires
`BUILD_AI=ON` and `BUILD_TESTS=ON`. No repository preset sets `BUILD_AI=ON`; a configure preset must
be supplied (Lane B uses a worktree-local `ci-ai`, §3.3.3). Once configured:

```bash
ctest --test-dir <build-dir> --output-on-failure -L ai
```

The `ai` label is attached by `gtest_discover_tests` (`LABELS "unit;ai;SPEC-XPE-P3-AI"`).

### 9.3 Coverage Report Generation

**Not available.** No coverage preset builds this module (§3.3.2). Adding `xpe_ai` to a coverage
preset is issue **#124**; until that lands, §9.3 has no procedure to document.

---

## 10. Open Gaps

Recorded so that no reader mistakes silence for satisfaction.

| # | Gap | Owner / tracking |
|---|---|---|
| G-AI-1 | No coverage measurement — `BUILD_AI=OFF` in every coverage preset | issue **#124** |
| G-AI-2 | No CI execution of `xpe_ai_tests`; the `ci-ai` preset is worktree-local, not in `CMakePresets.json` | this plan §3.3.3 |
| G-AI-3 | No inference implementation — L3/L4/L5 have no evidence; IEC 62304 §5.7 unsatisfied | Phase 3 |
| G-AI-4 | No worker round-trip (positive-path) test for REQ-AI-003 | Phase 3 |
| G-AI-5 | AddressSanitizer never run against `xpe_ai` | QA-B-21 verdict, 2026-09-10 |
| G-AI-6 | No case exercises the 64 MB `dataSize` ceiling (`ai.cpp:156`) | §8.1 |
| G-AI-7 | Model signing (REQ-AI-007) and time budget (REQ-AI-009) not implemented, not tested | RTM-AI-001 §12 |
| G-AI-8 | Confidence-threshold arm of the fallback router unreachable, therefore untested | §4.8 |

---

## 11. Sign-Off Criteria

Verification and validation of `xpe_ai.dll` is complete when **all** of the following hold. None of
items 3–8 holds today.

1. ✓ All 129 unit-test cases pass (recorded 2026-09-10, §3.3.3)
2. ✓ Memory-leak gate G3 case present and passing for this module (§3.3.4)
3. ☐ `xpe_ai` included in a coverage preset and line-rate >= `XPE_COVERAGE_MIN` (0.85) — issue #124
4. ☐ `xpe_ai_tests` executed by CI on every change
5. ☐ Inference path implemented and algorithm validation (L4) evidence collected
6. ☐ Worker round-trip integration test (L2 positive path) passing
7. ☐ Performance budget defined and measured (REQ-AI-009)
8. ☐ Clinical validation evidence per XPE-AI-REG-001

Until items 3–8 are closed, `xpe_ai.dll` is a development artefact and **must not be represented as
verified medical device software**.

---

## Document History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0.0 | 2026-09-10 | xpe-docs (issue #125) | Initial V&V Plan for `xpe_ai.dll`, authored against the stub build. §1.2 records the stub boundary with source anchors; §2.3 maps the six-level hierarchy onto IEC 62304 §5.5/5.6/5.7 and states that L3–L6 carry no evidence; §3.1/§3.2 give the measured test inventory (6 files, 129 `TEST`/`TEST_F` cases, counted 2026-09-10); §3.3.2 records that `xpe_ai` is in **no** coverage preset (issue #124) so no line-rate exists; §3.3.3 cites the lane execution records (129/129, 2026-09-10) and notes the `ci-ai` preset is worktree-local; §4.1–§4.8 give per-SWU verification methods with RTM cross-references; §4.9 and §10 enumerate what is explicitly not verified. Registered in `XPE-VVP-001` Addendum Registry as XPE-VVP-AI-001. |

---

**End of Document — XPE-VVP-AI-001 v1.0.0**
