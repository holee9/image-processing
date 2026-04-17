# SPEC-XPE-OPS: Operations, Observability & Post-Market Surveillance Master

---
id: SPEC-XPE-OPS
version: 1.1.0
status: Draft
created: 2026-04-17
updated: 2026-04-17
author: MoAI (manager-spec orchestration)
priority: Mixed (Must for M-10 PMS; Should for Drift/OTEL/Reproducible/VEX)
issue_number: null
iec62304_class: B
development_mode: TDD
sprint: S-OPS-CORE (Must) + S-OPS-EXT (Should)
dependency: SPEC-XPE-MASTER v3.0.0, SPEC-XPE-REG v1.1, trend-survey-2026.md v1.1
---

## Priority Reclassification Notice (v1.1)

본 SPEC의 우선순위는 trend-survey-2026.md v1.1 엄격 재분류에 따라 혼합:

- **Must (즉시, EU MDR 법적 의무)**: Post-Market Surveillance (PMS) Plan + 심각 사고 보고 체계 (M-10)
- **Should (강력 권장, 조건부)**: Reproducible Builds, Data/Concept Drift Detection, OpenTelemetry Instrumentation, Reject-Analysis, VEX Automation
- **강등 근거**: PMS는 EU MDR Art 83 법률 → Must. 나머지는 현대적 관측성·운영 우수성이나 법적 의무 아님
- **현재 실행 범위**: PMS 기본 체계만 즉시, 나머지는 Phase 2-3 확장

## HISTORY

| Version | Date       | Author       | Changes                                   |
|---------|------------|--------------|-------------------------------------------|
| 1.0.0   | 2026-04-17 | MoAI         | Initial EARS SPEC integrating Post-Market Surveillance + Reproducible Builds + Data/Concept Drift Detection + OpenTelemetry + Reject-Analysis + VEX automation |

---

## 1. Scope

### 1.1 Overview

본 SPEC은 XPE의 **운영·관측성·판매 후 감시 마스터 문서**로서, 의료기기 제조업체의 법적 PMS 의무(EU MDR Article 83, MHRA PMS Regs 2025-06-16 발효)와 FDA AI 판매 후 모니터링 가이던스를 통합한다.

본 SPEC이 다루는 5개 축:

1. **Post-Market Surveillance (PMS)** — Must, 법적 의무
2. **Reproducible Builds** — Must, SBOM/SLSA 기본 전제
3. **Data/Concept Drift Detection** — Should, AI 지속 모니터링
4. **OpenTelemetry Instrumentation** — Should, 현대적 관측성
5. **Reject-Analysis + VEX Automation** — Should + Must 혼합

### 1.2 In Scope

- PMS 계획서 및 보고 체계 (MDR Annex III, FDA §522 연계)
- Reject-analysis (재촬영 이벤트) 수집·분석
- Deviation Index (DI) drift telemetry (기존 score-plan §6)
- AI 모듈 data drift / concept drift 검출기
- OpenTelemetry Traces/Metrics/Logs/Profiles API 통합 (opt-in)
- Reproducible build 보증 메커니즘
- VEX 자동 생성 및 공개 파이프라인
- Customer feedback → issue tracking 연계

### 1.3 Exclusions

- Regulatory governance → SPEC-XPE-REG
- Cybersecurity incident response → SPEC-XPE-SEC §4.9
- AI model implementation → SPEC-XPE-P3-AI
- Hospital-side PACS operations → out of scope
- Personal data analytics infrastructure → hospital responsibility

---

## 2. Referenced Documents

| Document ID | Title | Version | Role |
|-------------|-------|---------|------|
| EU-MDR-Art83 | Article 83: Post-market surveillance system | 2017/745 | Normative |
| EU-MDR-Annex-III | Post-market surveillance plan | 2017/745 | Normative |
| MHRA-PMS-2025 | UK PMS Regs | 2025-06-16 발효 | Normative (UK market) |
| FDA-PMS-RFI | Methods and Tools for Effective Postmarket Monitoring of AI-Enabled MD | 2024 RFI | Informative |
| NAT-DIG-MED-2024 | Distribution shift detection for postmarket surveillance | Nature Digital Med 2024 | Informative |
| NAT-COMM-2024 | Empirical data drift detection experiments | Nature Communications 2024 | Informative |
| OTEL-SPEC | OpenTelemetry Specification | 1.x (2025) | Normative (choice) |
| CDX-VEX | CycloneDX VEX | 1.6 | Normative (choice) |
| CSAF-2.0 | Common Security Advisory Framework | 2.0 | Normative (choice) |
| REPRO-BUILDS | Reproducible Builds project | ongoing | Informative |
| AAPM-TG-151 | Ongoing QC in Digital Radiography | 2015 | Informative |
| SPEC-XPE-MASTER | XPE Master | v3.0.0 | Upstream |
| SPEC-XPE-REG | Regulatory Master | v1.0 | Upstream |
| SPEC-XPE-SEC | Cybersecurity Master | v1.0 | Upstream |

---

## 3. Definitions

| Term | Definition |
|------|-----------|
| PMS | Post-Market Surveillance |
| PSUR | Periodic Safety Update Report (MDR) |
| PSR | Periodic Summary Report (MDR Class IIa/IIb/III) |
| Data Drift | Input distribution shift post-deployment |
| Concept Drift | Relationship between input and output changes |
| Model Drift | Performance degradation (aggregate) |
| DI | Deviation Index (IEC 62494-1, exposure index) |
| Reject-Analysis | Statistical analysis of re-acquisition events |
| OTEL | OpenTelemetry |
| VEX | Vulnerability Exploitability eXchange |
| PURL | Package URL |
| Reproducible Build | Bit-identical output from identical inputs |
| SOURCE_DATE_EPOCH | Environment variable for deterministic timestamps |
| FHIR AuditEvent | FHIR resource for audit logging |
| OSCAL | Open Security Controls Assessment Language |

---

## 4. Requirements (EARS Format)

### 4.1 Post-Market Surveillance (Must)

**REQ-OPS-001** (Ubiquitous): The XPE manufacturer shall maintain a Post-Market Surveillance Plan per MDR Annex III, including: product scope, data sources, indicators, review frequency, responsibility matrix.

**REQ-OPS-002** (Ubiquitous): The XPE system shall emit audit events for: installation, configuration change, each processed study, AI model inference, error conditions, user override.

**REQ-OPS-003** (Ubiquitous): Audit events SHOULD be recorded in OpenTelemetry format and SHALL also be recordable in FHIR AuditEvent resource when enabled.

**REQ-OPS-004** (Event-driven): When a serious incident is detected or reported, the manufacturer shall initiate notification to competent authorities within MDR-specified timelines (serious: 2 days; fatality or deterioration: 10 days).

**REQ-OPS-005** (Ubiquitous): The manufacturer SHALL publish Periodic Safety Update Reports (PSUR) at frequencies: Class IIb/III annually, Class IIa biannually (per MDR Article 86).

**REQ-OPS-006** (Ubiquitous): MHRA PMS regulations (UK) shorter timelines SHALL be observed for UK deployments.

### 4.2 Reproducible Builds (Must)

**REQ-OPS-010** (Ubiquitous): Release builds of XPE DLLs and ImageProcTest.exe shall be reproducible: identical source tree + identical toolchain + identical environment produces bit-identical output.

**REQ-OPS-011** (Ubiquitous): Build scripts SHALL honor SOURCE_DATE_EPOCH environment variable for embedded timestamps.

**REQ-OPS-012** (Ubiquitous): CMake configuration SHALL disable non-deterministic features (e.g., __DATE__, __TIME__ macros) in production builds.

**REQ-OPS-013** (Ubiquitous): A verification script `scripts/verify_reproducible.sh` shall run dual-build comparison on CI and report any byte-level divergence.

**REQ-OPS-014** (Ubiquitous): Any known unavoidable non-determinism (e.g., PDB timestamps on Windows) SHALL be documented in `docs/operations/reproducibility-exceptions.md`.

### 4.3 Data/Concept Drift Detection (Should)

**REQ-OPS-020** (Ubiquitous): AI modules (PRE-06 ML, POST-02 DL, POST-07 AI, POST-09 Bone Suppression) SHALL emit input-distribution fingerprints at inference time for drift analysis.

**REQ-OPS-021** (Ubiquitous): Drift detection pipeline SHALL support at minimum: (a) Kolmogorov-Smirnov test per-feature, (b) Maximum Mean Discrepancy (deep kernel), (c) classifier-based drift detector per Nature Digital Medicine 2024.

**REQ-OPS-022** (Event-driven): When drift metric exceeds site-configured threshold, the system shall emit a drift alert in telemetry stream and optionally trigger PCCP-authorized retraining per SPEC-XPE-REG §4.1.

**REQ-OPS-023** (Ubiquitous): Drift detector shall be configurable off-device (cloud analyst) or on-device (edge) per deployment scenario.

**REQ-OPS-024** (Ubiquitous): Drift artifacts (fingerprints, scores) SHALL be encoded in privacy-preserving manner: no raw pixel data leaves the device boundary.

### 4.4 OpenTelemetry Instrumentation (Should)

**REQ-OPS-030** (Ubiquitous): xpe_common.dll SHALL provide OTEL Tracer/Meter/Logger API wrapper compatible with OpenTelemetry 1.x specification.

**REQ-OPS-031** (Ubiquitous): OTEL instrumentation shall be opt-in (off by default) controlled via configuration flag `xpe_config.observability.otel_enabled`.

**REQ-OPS-032** (Ubiquitous): When enabled, OTEL spans shall be emitted for: pipeline stage boundaries, AI inference, DICOM network transactions, errors.

**REQ-OPS-033** (Ubiquitous): OTEL metrics shall include: processing time per stage, AI confidence distribution, DICOM I/O latency, error rate per SWU.

**REQ-OPS-034** (Ubiquitous): OTEL Profiling signal (introduced 2024) SHOULD be supported for continuous profiling when enabled.

**REQ-OPS-035** (Ubiquitous): OTEL Exporter shall support OTLP over HTTP/protobuf at minimum; OTLP/gRPC and Zipkin/Jaeger as options.

**REQ-OPS-036** (Ubiquitous): OTEL span/metric names SHALL follow semantic conventions (e.g., `xpe.stage.name`, `xpe.swu.id`).

### 4.5 Reject-Analysis Telemetry (Should · score-plan 반영)

**REQ-OPS-040** (Ubiquitous): When a study is marked by a technologist as "rejected" (requiring re-acquisition), the system shall capture: reason code (positioning, exposure, motion, artifact, other), anatomical region, exposure parameters, device identifier, timestamp.

**REQ-OPS-041** (Ubiquitous): Reject-analysis data shall be aggregatable across the deployed fleet while preserving patient privacy.

**REQ-OPS-042** (Ubiquitous): Reject rate dashboard shall support breakdown by: reason, anatomy, device, technologist (opt-in per site policy).

**REQ-OPS-043** (Ubiquitous): Reject-analysis reports align with AAPM TG-151 Ongoing QC methodology.

### 4.6 Deviation Index (DI) Drift Telemetry (Should · score-plan 반영)

**REQ-OPS-050** (Ubiquitous): The xpe_enhance_basic.dll DI/EI output per IEC 62494-1 SHALL emit telemetry event per processed study.

**REQ-OPS-051** (Ubiquitous): DI drift monitor shall compute site-specific baseline distribution during first 30 days then alert on deviation beyond 2 sigma.

**REQ-OPS-052** (Ubiquitous): DI drift events SHALL be correlated with reject-analysis patterns to identify systematic mis-exposure.

### 4.7 VEX Automation (Should · cross-ref SEC)

**REQ-OPS-060** (Event-driven): When a CVE is published affecting an SBOM component of XPE, the VEX automation pipeline shall trigger within 24 hours.

**REQ-OPS-061** (Ubiquitous): VEX automation shall include: (a) affected version check, (b) exploitability analysis using upstream advisory + local code analysis, (c) VEX document generation in CycloneDX VEX, OpenVEX, and CSAF 2.0 formats.

**REQ-OPS-062** (Ubiquitous): VEX documents SHALL be published at `https://github.com/holee9/image-processing/security/advisories/` and referenced in SBOM updates.

**REQ-OPS-063** (Ubiquitous): VEX status shall be one of: `affected`, `not_affected`, `fixed`, `under_investigation`.

### 4.8 Customer Feedback Loop

**REQ-OPS-070** (Ubiquitous): A customer feedback channel SHALL be provided per MDR Annex III §1.1.

**REQ-OPS-071** (Ubiquitous): Feedback items SHALL be triaged within 10 business days and categorized: complaint, feature request, documentation, other.

**REQ-OPS-072** (Event-driven): When a complaint meeting "serious incident" definition is received, §4.1 REQ-OPS-004 notification workflow activates.

### 4.9 Operational Runbooks

**REQ-OPS-080** (Ubiquitous): Runbooks SHALL exist for: installation, upgrade, rollback, incident response, drift alert, reject rate anomaly, audit log collection.

**REQ-OPS-081** (Ubiquitous): Runbooks shall be version-controlled and reviewed at each major release.

---

## 5. Acceptance Criteria

### 5.1 PMS Artifacts

- [ ] PMS Plan document approved by Quality/Regulatory
- [ ] PSUR template prepared
- [ ] Serious incident notification workflow tested (tabletop)
- [ ] Competent authority contact list maintained

### 5.2 Reproducibility

- [ ] Two CI runs of identical tag produce bit-identical artifacts
- [ ] SLSA L2+ attestation generated (cross-ref SEC §4.5)
- [ ] Exception list for unavoidable non-determinism documented

### 5.3 Drift Detection

- [ ] Drift detector pipeline prototyped for 1 AI module (PRE-06 ML)
- [ ] Three detector algorithms (KS, MMD, classifier) implemented with unit tests
- [ ] Alert threshold calibration guide published

### 5.4 OpenTelemetry

- [ ] xpe_common exports OTEL Tracer/Meter/Logger API
- [ ] ImageProcTest demonstrates end-to-end tracing
- [ ] OTLP exporter validated against Jaeger and Grafana Tempo

### 5.5 Reject-Analysis

- [ ] Reject event schema defined (JSON schema published)
- [ ] ImageProcTest reject dialog integrated
- [ ] Aggregation dashboard prototype

### 5.6 VEX

- [ ] VEX automation workflow `.github/workflows/vex.yml` functional
- [ ] Sample VEX documents published for known CVEs

---

## 6. Out-of-Scope Clarifications

- Cloud backend for aggregation → site IT decision, XPE emits only
- Commercial observability platforms (Datadog, New Relic) → supported via OTLP, not specific integration
- Complete FHIR bulk export → SPEC-XPE-IOP scope
- Device management (MDM/EMM) → out of scope

---

## 7. Risks and Mitigations

| Risk | Severity | Mitigation |
|------|:--------:|-----------|
| OTEL overhead impacts real-time pipeline | Medium | Sampling + opt-in default off + benchmark harness |
| Reproducibility blockers on Windows (PDB, /Brepro) | Medium | Use `/Brepro` linker flag; document residual non-determinism |
| Drift alert false positive fatigue | Medium | Conservative threshold + multi-algorithm consensus |
| PMS data aggregation privacy concerns | High | Federated/anonymized metrics; no raw pixel egress |
| Reject-analysis gaming (users disable reject) | Low | Audit log immutability; technologist training |
| MHRA PMS short timelines | Medium | Automated notification pipeline with template |

---

## 8. Deliverables

### 8.1 Documents (10)

1. `docs/operations/pms-plan.md`
2. `docs/operations/psur-template.md`
3. `docs/operations/serious-incident-workflow.md`
4. `docs/operations/reproducibility-plan.md`
5. `docs/operations/reproducibility-exceptions.md`
6. `docs/operations/drift-detection-design.md`
7. `docs/operations/otel-instrumentation-guide.md`
8. `docs/operations/reject-analysis-spec.md`
9. `docs/operations/runbooks/` (8 runbooks)
10. `docs/operations/vex-automation-design.md`

### 8.2 Code Artifacts

- Header: `modules/common/include/xpe/common/xpe_otel.h`
- Header: `modules/common/include/xpe/common/xpe_telemetry.h` (drift, reject, DI events)
- Implementation: `modules/common/src/xpe_otel.cpp`, `xpe_telemetry.cpp`
- Tests: `modules/common/tests/test_otel.cpp`, `test_telemetry.cpp`
- Scripts: `scripts/verify_reproducible.sh`, `scripts/emit_vex.py`
- CI: `.github/workflows/reproducible-build.yml`, `.github/workflows/vex.yml`

### 8.3 RTM Entries

- 30+ REQ-OPS-XXX entries

---

## 9. Dependencies

- **Upstream**: SPEC-XPE-MASTER v3.0.0, SPEC-XPE-REG v1.0, SPEC-XPE-SEC v1.0, S0-B (xpe_common)
- **Downstream**: All AI modules (drift detectors), all DLL modules (OTEL instrumentation)
- **External**: OpenTelemetry Collector/Gateway, VEX registry (GitHub, OSV.dev)

---

## 10. Change Control

- PMS Plan review: every 12 months or on significant regulatory change
- Drift threshold review: quarterly based on field data
- Runbook updates: on each major release

---

**본 SPEC은 XPE의 운영·관측성·판매 후 감시 마스터로서 EU MDR Article 83과 FDA AI 판매 후 모니터링 요건을 구현한다.**
