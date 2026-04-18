# SPEC-XPE-MASTER v3.0.0 Upgrade Addendum

**Document ID**: SPEC-XPE-MASTER (v3 addendum)
**Version**: 3.0.0
**Date**: 2026-04-17
**Status**: Draft — Upgrades v2.0.0
**Author**: MoAI (manager-spec orchestration, integrating trend-survey-2026.md)
**Classification**: IEC 62304 Class B
**Relationship**: This addendum EXTENDS SPEC-XPE-MASTER/spec.md (v2.0.0); the two documents must be read together. v3.0.0 does not supersede v2.0.0 except where explicitly marked.

---

## A. Changelog (v2.0.0 → v3.0.0)

| Area | v2.0.0 | v3.0.0 | Rationale |
|------|--------|--------|-----------|
| SPEC count | 4 (MASTER, P0, P1A, P2-ADV) | **9** (+ REG, SEC, IOP, OPS, P3-AI) | 2024-2026 규제·보안·상호운용성 축 통합 |
| Must/Should/Could matrix | implicit | **explicit 55 items** | 사용자 요구 "기본 필수는 반드시 Must" 반영 |
| Total SWU | 43 | **48 (+5 governance SWU)** | xpe_interop (3), AI governance (2) 신설 |
| Total APIs | 82 | **~105** | DICOMweb (8), FHIR (6), OTEL (5), XAI (4) 등 |
| Total EARS REQ | ~223 | **~330** | REG 40 + SEC 35 + IOP 30 + OPS 30 = 135 추가 |
| Sprints | 11 | **14** | S-REG, S-SEC, S-IOP, S-OPS 추가 (S3 변경) |
| Quality Gates | TRUST 5 + LSP | **+ Model Observer TG-270** | Task-based image quality 도입 |
| 규제 커버리지 | IEC 62304 Class B만 | **+ FDA PCCP/AI-DSF + EU AI Act + ISO 42001 + QMSR** | 시장 출시 필수 |

---

## B. Must / Should / Could Matrix Integration (v1.1 Strict)

전체 57 항목은 `.moai/project/trend-survey-2026.md` v1.1에 상세 정의됨. 본 MASTER에는 **요약 매트릭스**와 **SWU/REQ 매핑**만 기록.

**v1.1 Strict Reclassification** (2026-04-17 재평가): Must 27 → **12**, Should 19 → **35**, Could 10. Must는 "법률 의무 + 시장 진입 블로커 + IEC 62304 강제 조항"으로 엄격 한정.

### B.1 Must Tier Summary (12 items, v1.1)

| Axis | Count | IDs | Normative SPEC |
|------|:-----:|-----|----------------|
| Regulatory | 3 | M-01 (IEC 62304), M-02 (EU MDR), M-03 (21 CFR/QMSR) | SPEC-XPE-REG (core 부분) |
| Cybersecurity | 4 | M-04 (§524B), M-05 (SBOM), M-06 (Vuln Mgmt), M-07 (Input Val) | SPEC-XPE-SEC (core 부분) |
| Interoperability | 2 | M-08 (DICOM 3.0), M-09 (Conformance Stmt) | SPEC-XPE-IOP (core 부분) |
| Operations | 1 | M-10 (PMS) | SPEC-XPE-OPS (core 부분) |
| Quality/Arch | 2 | M-11 (Char. Test IEC §5.4.1), M-12 (Trackability IEC §5.4.1) | 기존 P1A/P1B/P2-ADV |

**판정 기준 (엄격)**: 법률(Law) OR 시장 진입 거절 사유 OR IEC 62304 강제 조항

### B.2 Should Tier Summary (35 items, v1.1)

v1.0 Must → Should 강등 (15): PCCP, AI-DSF Lifecycle Draft, Transparency, GMLP, EU AI Act, ISO 42001, IEC 81001-5-1, IHE RAD Baseline, TRUST 5, Reference+SIMD Parity, MX Tag, Anti-Spaghetti, Reproducible Builds, Model Card, Data Lineage, Deterministic Fallback

v1.0 Should 유지 (19): SLSA L3, DICOMweb, FHIR R5, IHE AIR/AIRA, SSL Denoising, Diffusion Priors, Conformal UQ, XAI, ONNX 1.20+, Drift Detection, OpenTelemetry, VEX Auto, Reject-Analysis 등

Representative:
- S-REG-03~08: AI 관련 규제 (AI-DSF 배포 조건부)
- S-SEC-01 (SLSA L3), S-SEC-05 (IEC 81001-5-1)
- S-IOP-01~03 (DICOMweb/FHIR R5/IHE AIR/AIRA)
- S-AI-01~08 (SSL+Diffusion+XAI+UQ+Governance)
- S-OPS-01~05 (Drift+OTEL+VEX+Reject+Reproducible)

### B.3 Could Tier Summary (10 items, 유지)

모두 Phase 4 Research Track 또는 Post-Market Evolution으로 격리.

### B.4 Conditional Upgrade Rules (v1.1 신규)

다음 조건 충족 시 Should → Must 승격:
- **S-REG-03 FDA PCCP**: Phase 3 AI-DSF 배포 승인 → Must
- **S-REG-07 EU AI Act**: EU 시장 + AI 탑재 판매 → Must
- **S-AI-06/07/08 (Model Card/Lineage/Fallback)**: Phase 3 AI 개발 시작 → Must
- **S-IOP-05 IHE Baseline**: Connectathon 참여 결정 → Must

조건부 승격 체크포인트: 각 Phase 진입 전 Gate Review에서 재평가.

---

## C. Updated Architecture Diagram (Layer View)

```
┌─────────────────────────────────────────────────────────────────────┐
│  Layer 2: ImageProcTest.exe (C# WPF)                                │
│  - SWU-5.7 PipelineOrchestrator                                      │
│  - SWU-6.1 QaConstancyTest                                           │
│  - NEW: SWU-6.2 AiInfoDisplay (Model Card UI, XAI sidecar viewer)    │
└────────────────────────────────┬────────────────────────────────────┘
                                 │ P/Invoke
         ┌───────────────────────┼───────────────────────┐
         │                       │                       │
┌────────▼─────────┐   ┌─────────▼────────┐   ┌──────────▼──────────┐
│ Layer 1: XPE DLLs │   │ Layer 1-G: GSVG  │   │ NEW: xpe_interop    │
│ (preprocess, basic,│   │ (independent)    │   │ - DICOMweb client   │
│  advanced, display,│   │                  │   │ - FHIR R5 adapter   │
│  dicom, ai)        │   │                  │   │ - IHE AIR encoder   │
└────────┬─────────┘   └──────────────────┘   └──────────┬──────────┘
         │                                                │
         └────────────────┬───────────────────────────────┘
                          │
┌─────────────────────────▼─────────────────────────────────────────┐
│ Layer 0: xpe_common.dll                                            │
│ - Types, Memory, Config, Logger, Alert                             │
│ - NEW: xpe_otel (OpenTelemetry wrapper)                            │
│ - NEW: xpe_telemetry (drift/reject/DI events)                      │
│ - NEW: xpe_model_card (AI governance)                              │
│ - NEW: xpe_secure (crypto helpers, signature verify)               │
└───────────────────────────────────────────────────────────────────┘
```

---

## D. Updated SWU Inventory Summary (48 units)

### D.1 Delta from v2.0.0 (43 → 48)

**New SWUs (5)**:

| ID | Name | DLL | Phase | SPEC |
|----|------|-----|:-----:|------|
| SWU-0.8 | OtelInstrumentation | xpe_common | 0 | OPS |
| SWU-0.9 | TelemetryEventBus | xpe_common | 0 | OPS |
| SWU-0.10 | ModelCardAPI | xpe_common | 0 | REG |
| SWU-0.11 | SecurePrimitives | xpe_common | 0 | SEC |
| SWU-interop.1 | DicomwebClient | xpe_interop | 2 | IOP |
| SWU-interop.2 | FhirR5Adapter | xpe_interop | 2 | IOP |
| SWU-interop.3 | IheAiResultsEncoder | xpe_interop | 3 | IOP |

**Note**: SWU-interop.1 and SWU-interop.2 count as Phase 2 additions (bringing Phase 2 SWU from 8 to 10).

### D.2 Coverage Thresholds (Harmonized v3.0)

| Category | Coverage Target | Rationale |
|----------|:---------------:|-----------|
| Layer 0 core | 90% | Foundational safety |
| Preprocess (Phase 1a) | 90% | Clinical safety-critical |
| Basic enhance/display/dicom (Phase 1b) | 85% | Diagnostic path |
| Advanced (Phase 2) | 85% | Differentiator |
| AI (Phase 3) | 80% | Complex ML; supplemental test |
| Interop (Phase 2-3) | 85% | Standard compliance |
| Governance (Layer 0) | 90% | Audit critical |

---

## E. Quality Gates v3.0

### E.1 Retained from v2.0

- TRUST 5 (Tested, Readable, Unified, Secured, Trackable)
- LSP gates (zero errors, bounded warnings)
- Coverage thresholds (per category)
- Branch coverage
- Static analysis
- Memory leak detection
- Performance budgets

### E.2 NEW in v3.0

**Model Observer Quality Gate (AAPM TG-270 Family Integration)**

For AI and enhancement tiers that produce diagnostic images, introduce task-based image quality gates:

- **Gate M-IQ-01**: For each AI-enhanced output, compute mathematical model observer metrics (e.g., Channelized Hotelling Observer for detection tasks) against baseline deterministic path
- **Gate M-IQ-02**: AI output shall not decrease model observer AUC by more than 0.02 compared to deterministic reference
- **Gate M-IQ-03**: Perceptual metrics (LPIPS, SSIM) shall be reported alongside MSE/PSNR
- **Gate M-IQ-04**: For diagnostic-relevant AI outputs, radiologist reader study result shall be appended to acceptance package before Phase 3 release

**Regulatory Gate**

- **Gate M-REG-G-01**: Each AI-DSF shall have an approved PCCP document before Phase 3 release
- **Gate M-REG-G-02**: Each AI-DSF shall have a signed-off Model Card before release
- **Gate M-REG-G-03**: Data lineage audit trail verified complete

**Security Gate**

- **Gate M-SEC-G-01**: SBOM published in both SPDX 3.0 and CycloneDX 1.6 formats
- **Gate M-SEC-G-02**: SLSA L2 attestation present (L3 by Phase 2)
- **Gate M-SEC-G-03**: Zero critical unresolved CVE (or VEX-justified)
- **Gate M-SEC-G-04**: Threat model review within 3 months of release

**Reproducibility Gate**

- **Gate M-OPS-G-01**: Release build reproducibility verified (bit-identical from identical source+env)

---

## F. Updated Sprint Structure (14 Sprints)

### F.1 Sprint Matrix v3.0

| Sprint | SPEC | Phase | DLL/Module | Priority | Dependency |
|--------|------|:-----:|-----------|:--------:|-----------|
| S0-A | SPEC-XPE-P0 | 0 | Build/CMake | Must | None |
| S0-B | SPEC-XPE-P0 | 0 | xpe_common | Must | S0-A |
| S0-C | SPEC-XPE-P0 | 0 | ImageProcTest | Must | S0-A |
| S1-A | SPEC-XPE-P1A | 1a | xpe_preprocess | Must | S0-B |
| S1-B1 | SPEC-XPE-P1B-ENH | 1b | xpe_enhance_basic | Must | S1-A |
| S1-B2 | SPEC-XPE-P1B-DISP | 1b | xpe_display | Must | S1-A |
| S1-B3 | SPEC-XPE-P1B-DICOM | 1b | xpe_dicom | Must | S1-A |
| S1-B4 | SPEC-XPE-P1B-GUI | 1b | ImageProcTest | Should | S1-B1 |
| **S-REG** | **SPEC-XPE-REG** | **parallel** | (docs + Layer 0 additions) | **Must** | **S0-A (can start early)** |
| **S-SEC** | **SPEC-XPE-SEC** | **parallel** | (CI/CD + Layer 0) | **Must** | **S0-B** |
| S2-A | SPEC-XPE-P2-ADV | 2 | xpe_enhance_advanced | Must | S1-B1 |
| S2-B | SPEC-XPE-P2-GSVG | 2 | gsvg | Must | S1-A |
| **S-IOP** | **SPEC-XPE-IOP** | **2** | **xpe_interop (new)** | **Must (baseline) + Should (ext)** | **S1-B3, S2-A** |
| **S-OPS** | **SPEC-XPE-OPS** | **parallel** | (xpe_common + CI + docs) | **Must** | **S1-B1** |
| S3 | **SPEC-XPE-P3-AI** | 3 | xpe_ai + worker | Should | S2-A, S1-B4, S-REG complete |

**Total**: 14 sprints (was 11), **3 cross-cutting parallel sprints** (S-REG, S-SEC, S-OPS) can run concurrently with core build sprints after their dependency is satisfied.

### F.2 Updated Critical Path

```
                                    ┌─────────────────────────────────┐
                                    │ S-REG (regulatory docs, parallel)│
                                    └─────────────────────────────────┘
                                    ┌─────────────────────────────────┐
                                    │ S-SEC (security infra, parallel) │
                                    └─────────────────────────────────┘
                                                  │
S0-A ──┬── S0-B ──┬── S1-A ──┬── S1-B1 ──┬── S2-A ──┬── S3 (Must REG complete)
       │           │           │           │         │
       └── S0-C    │           ├── S1-B2   │         │
                   │           ├── S1-B3 ──┼── S-IOP (ext) ─┘
                   │           └── S1-B4   │
                   └────────── S-OPS ──────┘
                                           │
                                           └── S2-B (parallel with S2-A)
```

**Critical Path (Must)**: S0-A → S0-B → S1-A → S1-B1 → S2-A → S3  
**Parallel Must Tracks**: S-REG (start after S0-A), S-SEC (after S0-B), S-OPS (after S1-B1)

### F.3 Pre-Sprint Checklist Additions

**Before S-REG**:
- [ ] Legal counsel engaged
- [ ] Regulatory Affairs lead assigned
- [ ] FDA pre-submission (Q-Sub) booking prepared
- [ ] Notified Body contact established (EU)

**Before S-SEC**:
- [ ] Security Lead assigned
- [ ] Threat modeling training completed
- [ ] SLSA generator GitHub Actions workflow template prepared

**Before S-OPS**:
- [ ] PMS data flow design approved
- [ ] OTEL Collector endpoint configured

**Before S-IOP**:
- [ ] dcm4che test setup verified
- [ ] Orthanc reference server configured
- [ ] IHE Connectathon calendar checked

**Before S3 (updated)**:
- [ ] All S-REG deliverables approved (PCCP, Model Cards templates)
- [ ] SPEC-XPE-SEC §4.6 Threat Model completed for AI boundary
- [ ] Training data governance approved
- [ ] ONNX Runtime 1.20+ environment validated

---

## G. Pipeline Update (v3.0)

```
Raw Frame
 -> (0)   CalibManager Load
 -> (0.5) PRE-01 Readout Artifact Validation (Phase 1a)
 -> (0.7) PRE-07 Temperature Compensation (Phase 1a)
 -> (1)   PRE-02 Offset Correction (Phase 1a)
 -> (1.5) PRE-08 Nonlinearity Correction (Phase 1a)
 -> (2)   PRE-03 Gain Correction (Phase 1a)
 -> (2.5) PRE-09 Binning Correction (Phase 1a, conditional)
 -> (3)   PRE-06 basic Defect Correction (Phase 1a)
      [AI-tier: PRE-06 ML ViT AE (Phase 3, opt-in, worker-isolated)]
 -> (4)   PRE-04/05 Ghost/Lag Correction (Phase 1a)
 -> (EI-0) Whole-image EI baseline (Phase 1b)
 -> (5)   POST-01 Log Transform (Phase 1b)
 -> (5a)  POST-06 Body Part Recognition (Phase 3)
 -> (5b)  POST-07 baseline Collimation (Phase 2)
      [AI-tier: POST-07 AI Collimation (Phase 3, opt-in)]
 -> (EI-1) ROI-aware EI refinement (Phase 2)
 -> (6)   POST-02 Noise Reduction (Phase 1b)
      [AI-tier: POST-02 SSL Denoising (Phase 3)]
      [premium: POST-02 Diffusion Priors (Phase 3, opt-in)]
 -> (7)   POST-03 Contrast Enhancement (Phase 1b)
 -> (8)   POST-04 Edge Enhancement (Phase 1b)
 -> (9)   GSVG POST-10/11 (Phase 2, independent DLL)
 -> (10)  POST-05 Multiscale Processing (Phase 2)
 -> (11)  Fractional Processing (Phase 2)
 -> (12)  POST-08 Image Stitching (Phase 3, conditional)
 -> (13)  POST-09 Bone Suppression (Phase 3, optional)
 -> (14)  POST-12a Modality LUT (Phase 1b)
 -> (15)  POST-12b VOI LUT (Phase 1b)
 -> (16)  POST-12c Presentation LUT (Phase 1b)
 -> (17)  SUP-04 DICOM Write (Phase 1b)
 *** NEW parallel sidecars (emitted at each AI-tier) ***
 -> Sidecar Emitter: model_id, confidence, saliency_ref, uq_set, drift_fingerprint
 -> OTEL Span Boundary: each stage wraps span if opt-in enabled
 -> Telemetry Events: DI, reject-flag, drift alerts
 *** NEW parallel export (optional) ***
 -> DICOMweb STOW-RS to remote PACS (SPEC-XPE-IOP)
 -> FHIR ImagingStudy/ImagingSelection to EHR
 -> IHE AIR SR to reporting system
```

---

## H. Document Update Matrix v3.0

| Document | v2.0.0 | v3.0.0 Target | Priority |
|----------|:------:|:-------------:|:--------:|
| SPEC-XPE-MASTER | v2.0.0 | **v3.0.0 (본 addendum)** | Done |
| trend-survey-2026 | — | **v1.0.0** | Done |
| SPEC-XPE-REG | — | **v1.0.0** | Done |
| SPEC-XPE-SEC | — | **v1.0.0** | Done |
| SPEC-XPE-IOP | — | **v1.0.0** | Done |
| SPEC-XPE-OPS | — | **v1.0.0** | Done |
| SPEC-XPE-P3-AI | — | **v1.0.0** | Done |
| score-improvement-plan | v2.0.0 | **v3.0.0** | Done (companion) |
| sprint-execution-roadmap | v2.0.0 | **v3.0.0** | Pending |
| product.md | v1.0 | **v2.0** | Pending |
| tech.md | v1.0 | **v2.0** | Pending |
| api-spec.md | v1.2 (planned) | **v1.3** | Pending |
| XPE-SDD-001 | v1.0 | **v1.2** | Pending |
| XPE-SRS-001 | v1.0 | **v1.2** | Pending |
| XPE-RTM-001 | v1.0 | **v1.2** | Pending |
| pipeline-spec | v1.1 | **v1.2** | Pending |

---

## I. Risk Addendum (v3.0 Global)

| ID | Risk | Severity | Mitigation |
|----|------|:--------:|-----------|
| R-M3-01 | 병행 3개 sprint (REG/SEC/OPS) 자원 경쟁 | Medium | Role matrix: Regulatory, Security, DevOps leads 각각 할당 |
| R-M3-02 | EU AI Act 2027 deadline pressure | High | S-REG 즉시 착수; pre-submission consultation Q1-2027 |
| R-M3-03 | AI governance가 Phase 1a 지연 야기 | Medium | 문서 작업은 S-REG parallel track, 구현은 Phase 3 전 완료 |
| R-M3-04 | Model Observer Quality Gate 측정 인프라 부재 | Medium | Phase 2에 IQ measurement pipeline 구축 |
| R-M3-05 | SPDX/CycloneDX 동시 지원 복잡도 | Low | `syft` + `cyclonedx-cpp-maker` 툴체인 표준화 |
| R-M3-06 | DICOMweb 성능이 기존 DIMSE보다 느림 | Medium | 옵션 제공, 사이트별 선택; 성능 벤치마크 harness |
| R-M3-07 | IHE AIRA 신규 프로필 (2025-06) 미검증 | Medium | Connectathon 2026 early adoption plan |

---

## J. Versioning Governance

- **v3.0.0**: 본 addendum 승인 시 (사용자 승인 필요)
- **v3.x (minor)**: 신규 SPEC 추가 또는 기존 SPEC 개정
- **v4.0.0**: Phase 4 Research Track (Could 티어) 전환 시점

---

**본 Addendum은 v2.0.0 마스터 문서와 병행하여 읽어야 하며, 2024-2026 규제·보안·상호운용성·AI 트렌드를 통합한 최종 상위 설계이다.**
