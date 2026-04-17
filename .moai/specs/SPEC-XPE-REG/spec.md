# SPEC-XPE-REG: Regulatory Master Compliance

---
id: SPEC-XPE-REG
version: 1.1.0
status: Draft
created: 2026-04-17
updated: 2026-04-17
author: MoAI (manager-spec orchestration)
priority: Mixed (Must for M-01~M-03 core; Should for AI-related REG-03~07 deferred to Phase 3)
issue_number: null
iec62304_class: B
development_mode: TDD
sprint: S-REG-CORE (Must) + S-REG-AI (Should, Phase 3 승인 시)
dependency: SPEC-XPE-MASTER v3.0.0, trend-survey-2026.md v1.1
---

## Priority Reclassification Notice (v1.1)

본 SPEC의 우선순위는 trend-survey-2026.md v1.1 엄격 재분류에 따라 다음과 같이 혼합:

- **Must (즉시 착수)**: IEC 62304 Class B 문서, EU MDR 준수, 21 CFR 820.30 + QMSR 2026
- **Should (Phase 3 AI 승인 시 Must 승격)**: FDA PCCP, FDA AI-DSF Lifecycle Draft, FDA Transparency, IMDRF GMLP, EU AI Act High-Risk, ISO/IEC 42001 AIMS
- **현재 실행 범위**: Must 부분만 즉시, Should 부분은 Phase 3 진입 결정 시 본 SPEC이 전체 Must로 승격

## HISTORY

| Version | Date       | Author       | Changes                                   |
|---------|------------|--------------|-------------------------------------------|
| 1.0.0   | 2026-04-17 | MoAI         | Initial EARS SPEC integrating FDA PCCP / AI-DSF Lifecycle / Transparency / IMDRF GMLP / EU AI Act / ISO 42001 / IEC 62304/81001-5-1 / QMSR |

---

## 1. Scope

### 1.1 Overview

본 SPEC은 XPE 프로젝트의 **규제 준수 마스터 문서**로서, 2024-2026년 발효·시행된 AI/ML 의료기기 규제를 단일 통합 프레임워크로 정리한다. 본 문서는 IEC 62304(기본 소프트웨어 라이프사이클)에 **상위 오버레이**로 기능하며, 4개 하위 레거시 SPEC(P0/P1A/P2-ADV/P3-AI)을 규제 축으로 재구성한다.

본 SPEC이 다루는 7개 규제 축:

1. **FDA PCCP** (Predetermined Change Control Plan) — Final Guidance 2024-12
2. **FDA AI-DSF Lifecycle** (Total Product Life Cycle) — Draft Guidance 2025-01
3. **FDA Transparency for ML-MD** — Guiding Principles 2024-06
4. **IMDRF GMLP N88** (10 Guiding Principles) — Final 2025-01
5. **EU AI Act** (Regulation 2024/1689) — High-Risk Medical AI Compliance
6. **ISO/IEC 42001** (AI Management System) — 2023-12
7. **QMSR 2026** (Quality Management System Regulation, ISO 13485 통합)

### 1.2 In Scope

- Regulatory Traceability Matrix (XPE SWU ↔ FDA/EU/ISO 요구사항 매핑)
- Model Card 및 Transparency Artifact 스펙
- PCCP 프레임워크 (재훈련 sanction 경로)
- AI Management System (ISO 42001) 프로세스 정의
- 인간-AI 상호작용 설계 원칙 (GMLP #5, #6)
- Data Lineage 및 Bias Analysis 프로토콜
- QMSR 2026 전환 gap analysis
- Submission Package Outline (FDA 510(k)/PMA + CE MDR)

### 1.3 Exclusions

- **Cybersecurity**: SPEC-XPE-SEC 범위 (FDA §524B, IEC 81001-5-1)
- **Interoperability**: SPEC-XPE-IOP 범위 (DICOM, FHIR, IHE)
- **Operations/Post-Market Telemetry**: SPEC-XPE-OPS 범위
- **AI Implementation Details**: SPEC-XPE-P3-AI 범위 (본 SPEC은 governance만)
- **Cybersecurity Risk Management**: IEC 81001-5-1 항목은 SEC로 위임

---

## 2. Referenced Documents

| Document ID | Title | Version | Role |
|-------------|-------|---------|------|
| FDA-PCCP-2024 | Marketing Submission Recommendations for PCCP for AI-DSF | Final 2024-12-03 | Normative |
| FDA-AIDSF-2025 | AI-DSF Lifecycle Management and Marketing Submission Recommendations | Draft 2025-01-07 | Normative (공개 의견 기간 종료 2025-04-07) |
| FDA-TRANS-2024 | Transparency for ML-Enabled MD: Guiding Principles | 2024-06 | Normative |
| IMDRF-N88 | Good Machine Learning Practice for Medical Device Development: Guiding Principles | Final 2025-01-27 | Normative |
| EU-2024-1689 | EU Artificial Intelligence Act | 2024-08-01 발효 | Normative (Medical AI 2027-08 전면) |
| EU-2017-745 | Medical Devices Regulation (MDR) | 2017/745 + MDCG 2025 amendments | Normative |
| ISO-42001 | AI Management System | 2023-12 | Normative |
| IEC-62304 | Medical Device Software Life Cycle Processes | 2006 + Amd.1:2015 | Normative (기존) |
| ISO-13485 | Medical Devices QMS | 2016 | Normative (QMSR 2026 통합) |
| ISO-14971 | Risk Management for Medical Devices | 2019 | Normative |
| NIST-AI-100-1 | AI Risk Management Framework | 1.0 (2023-01) | Informative Reference |
| NIST-AI-600-1 | Gen AI Profile | 2024-07-26 | Informative Reference |
| SPEC-XPE-MASTER | XPE Master Implementation Plan | v3.0.0 | Upstream |
| TREND-SURVEY-2026-001 | XPE Trend Survey & Matrix | v1.0.0 | Upstream |

---

## 3. Definitions and Acronyms

| Term | Definition |
|------|-----------|
| PCCP | Predetermined Change Control Plan — AI 모델 재훈련에 대한 사전 승인 계획 |
| AI-DSF | Artificial Intelligence-Enabled Device Software Function (FDA 2024-12 이후 용어) |
| ML-DSF | Machine Learning-Enabled Device Software Function (구 용어, AI-DSF로 대체) |
| TPLC | Total Product Life Cycle — FDA의 전생애주기 관리 접근 |
| GMLP | Good Machine Learning Practice — IMDRF/FDA 10대 원칙 |
| AIMS | Artificial Intelligence Management System — ISO 42001 기반 |
| QMSR | Quality Management System Regulation — 2026 FDA ISO 13485 통합 |
| High-Risk AI | EU AI Act Annex III/Article 6 — 의료기기 AI 자동 분류 |
| Model Card | 모델 특성·성능·한계 투명성 문서 |
| Data Lineage | 데이터 출처, 수집, 가공, 분할 이력 |
| Substantial Modification | EU AI Act 재평가 트리거 |
| Bias Analysis | 인구통계·장비별 성능 편차 분석 |
| RTM | Requirements Traceability Matrix |

---

## 4. Requirements (EARS Format)

### 4.1 FDA PCCP Requirements

**REQ-REG-001** (Ubiquitous): The XPE system shall maintain a Predetermined Change Control Plan document for each AI-DSF module (PRE-06 ML, POST-02 DL, POST-07 AI, POST-09 Bone Suppression).

**REQ-REG-002** (Ubiquitous): The PCCP document shall contain three components: (1) Description of Modifications, (2) Modification Protocol, (3) Impact Assessment.

**REQ-REG-003** (State-driven): When an AI-DSF is subject to modification within the PCCP-authorized scope, the system shall not require a new 510(k) or PMA supplement.

**REQ-REG-004** (State-driven): When an AI-DSF modification exceeds the PCCP-authorized scope, the system shall require new FDA submission.

**REQ-REG-005** (Ubiquitous): The XPE device labeling shall state clearly that the device incorporates machine learning and has an authorized PCCP.

**REQ-REG-006** (Ubiquitous): The PCCP shall specify test data, performance criteria, and regression boundaries for each permitted modification.

**REQ-REG-007** (Event-driven): When a PCCP-authorized modification is executed, the system shall generate an audit record with version, training data snapshot hash, validation results, and deployment timestamp.

### 4.2 FDA AI-DSF Lifecycle Requirements

**REQ-REG-010** (Ubiquitous): Each AI-DSF shall have a Device Description document covering intended use, user, clinical environment, and patient population.

**REQ-REG-011** (Ubiquitous): Each AI-DSF shall have a Model Description document covering architecture, inputs, outputs, training strategy.

**REQ-REG-012** (Ubiquitous): Each AI-DSF shall document Data Management including collection, curation, labeling, splitting, and quality assurance processes.

**REQ-REG-013** (Ubiquitous): Each AI-DSF shall maintain Data Lineage records identifying all training, validation, and test data sources.

**REQ-REG-014** (Ubiquitous): Each AI-DSF submission shall include performance metrics tied to explicit clinical claims (sensitivity, specificity, AUC with 95% CI).

**REQ-REG-015** (Ubiquitous): Each AI-DSF shall include a Bias Analysis across demographics (age, sex, race/ethnicity where available), equipment vendors, and acquisition protocols.

**REQ-REG-016** (Ubiquitous): Each AI-DSF shall document Human-AI Interaction including user role, decision authority, override mechanism, and alert design.

**REQ-REG-017** (Ubiquitous): Each AI-DSF shall include a Monitoring Plan identifying performance indicators to track post-deployment.

### 4.3 FDA Transparency Requirements

**REQ-REG-020** (Ubiquitous): The XPE system shall expose a Model Card API endpoint returning: intended use, training data summary, demographic performance, limitations, and model version.

**REQ-REG-021** (Ubiquitous): The ImageProcTest GUI and end-user display shall present a Device Information screen showing AI components, model versions, and PCCP status.

**REQ-REG-022** (Event-driven): When AI processing fails or confidence is below threshold, the system shall display a human-readable explanation including failure mode, fallback behavior, and recommended action.

**REQ-REG-023** (Ubiquitous): Per-case XAI sidecar (if enabled) shall include model identifier, confidence score, and saliency map reference.

### 4.4 IMDRF GMLP Requirements (10 Principles Mapping)

**REQ-REG-030** (GMLP #1): The XPE system shall apply multi-disciplinary expertise throughout the total product lifecycle.

**REQ-REG-031** (GMLP #2): Each AI-DSF shall implement good software engineering and security practices per IEC 62304 and IEC 81001-5-1.

**REQ-REG-032** (GMLP #3): Clinical study participants and data sets shall be representative of the intended patient population.

**REQ-REG-033** (GMLP #4): Training data sets shall be independent of test sets.

**REQ-REG-034** (GMLP #5): Selected reference datasets shall be based upon best available methods.

**REQ-REG-035** (GMLP #6): Model design shall be tailored to available data and reflect the intended use.

**REQ-REG-036** (GMLP #7): Focus of the Human-AI Team is to perform well in the intended workflow.

**REQ-REG-037** (GMLP #8): Testing shall demonstrate device performance during clinically relevant conditions.

**REQ-REG-038** (GMLP #9): Users shall be provided clear, essential information about AI system performance and limitations.

**REQ-REG-039** (GMLP #10): Deployed models shall be monitored for performance and re-training risks shall be managed.

### 4.5 EU AI Act High-Risk Requirements

**REQ-REG-050** (Ubiquitous): Each AI-DSF classified as High-Risk AI System under EU AI Act Article 6 shall comply with Chapter III Section 2 obligations.

**REQ-REG-051** (Ubiquitous): The XPE provider shall maintain a Risk Management System per Article 9 (separate from ISO 14971 but harmonized).

**REQ-REG-052** (Ubiquitous): Data governance per Article 10 shall ensure training/validation/test sets are relevant, representative, free of errors, and complete for intended purpose.

**REQ-REG-053** (Ubiquitous): Technical documentation per Article 11 and Annex IV shall be maintained and updated through the lifecycle.

**REQ-REG-054** (Ubiquitous): Record-keeping capabilities per Article 12 shall enable automatic recording of events throughout the lifetime of the system.

**REQ-REG-055** (Ubiquitous): Transparency and information to users per Article 13 shall be provided via Instructions for Use.

**REQ-REG-056** (Ubiquitous): Human oversight per Article 14 shall enable users to: (a) fully understand capacities and limitations, (b) remain aware of automation bias, (c) correctly interpret output, (d) decide not to use output, (e) reverse output, (f) interrupt the system.

**REQ-REG-057** (Ubiquitous): Accuracy, robustness and cybersecurity per Article 15 shall be declared and continuously maintained.

**REQ-REG-058** (Ubiquitous): The CE marking per Article 16 shall indicate conformity with both MDR and AI Act.

### 4.6 ISO/IEC 42001 AI Management System Requirements

**REQ-REG-070** (Ubiquitous): The organization shall establish an AI Management System (AIMS) per ISO/IEC 42001 Clauses 4-10.

**REQ-REG-071** (Ubiquitous): AIMS shall be integrated with existing ISO 13485 QMS per Johner Institute dual-certification pattern.

**REQ-REG-072** (Ubiquitous): AI-specific risk assessment shall identify opportunities and risks, and define risk-minimizing measures per ISO/IEC 42001 Clause 6.1.

**REQ-REG-073** (Ubiquitous): AIMS shall document AI system lifecycle per ISO/IEC 42001 Annex B (36 controls).

### 4.7 QMSR 2026 Transition Requirements

**REQ-REG-080** (Ubiquitous): The XPE project shall complete a gap analysis from 21 CFR 820 QSR to QMSR (effective 2026-02-02) before FDA submission.

**REQ-REG-081** (Ubiquitous): QMSR compliance shall include ISO 13485:2016 Clause 4-8 requirements.

**REQ-REG-082** (Ubiquitous): Configuration management per ISO 13485 §7.5.9 shall explicitly frame SBOM as part of device configuration.

### 4.8 Submission Package Requirements

**REQ-REG-090** (Event-driven): When preparing FDA 510(k) or PMA submission, the submission package shall include: Device Description, Predicate Comparison (if 510(k)), PCCP, Model Card for each AI-DSF, Bias Analysis, Data Lineage, Validation Report, Cybersecurity Documentation (pointer to SEC SPEC), Usability Engineering File.

**REQ-REG-091** (Event-driven): When preparing EU MDR Technical Documentation, the Annex II package shall include all Section 4.5 EU AI Act artifacts in addition to MDR Annex II contents.

**REQ-REG-092** (Ubiquitous): Traceability Matrix (XPE-RTM-001) shall map each AI-DSF SWU to applicable FDA/EU/ISO requirements in this SPEC.

---

## 5. Acceptance Criteria

### 5.1 Documentation Completeness

- [ ] PCCP document exists for each AI-DSF (4 documents minimum: PRE-06, POST-02, POST-07, POST-09)
- [ ] Model Card for each AI-DSF following FDA Transparency template
- [ ] Data Lineage records for each training dataset
- [ ] Bias Analysis report for each AI-DSF
- [ ] EU AI Act Annex IV technical documentation
- [ ] ISO/IEC 42001 AIMS Manual
- [ ] QMSR gap analysis report
- [ ] Updated RTM (XPE-RTM-001 v1.2) with REG mappings

### 5.2 Process Gates

- [ ] Legal counsel sign-off on regulatory interpretation
- [ ] Clinical SME review of Model Cards
- [ ] Data governance committee approval of data lineage

### 5.3 API/Runtime Conformance

- [ ] `xpe_ai_get_model_card()` C API returning Model Card JSON
- [ ] `xpe_ai_get_version_info()` returning current model version, PCCP status
- [ ] ImageProcTest GUI "AI Info" tab displays required information
- [ ] Audit log records every AI-DSF modification event

---

## 6. Out-of-Scope Clarifications

- Cybersecurity implementation (IEC 81001-5-1, §524B) → SPEC-XPE-SEC
- DICOM/FHIR/IHE interoperability → SPEC-XPE-IOP
- Post-market surveillance telemetry → SPEC-XPE-OPS
- AI model code/training scripts → SPEC-XPE-P3-AI
- Risk management file implementation → existing SRS-ENHANCE-ADV-001 + ISO 14971 process

---

## 7. Risks and Mitigations

| Risk | Severity | Mitigation |
|------|:--------:|-----------|
| Legal interpretation error across FDA/EU/ISO | High | Legal counsel sign-off checkpoint at each SPEC-REG milestone |
| EU AI Act 2027 timeline pressure | High | Start REG implementation in Sprint S-REG parallel to Phase 1a |
| PCCP scope too restrictive or too broad | Medium | Iterate PCCP scope through regulatory pre-submission consultation (Q-Sub) |
| QMSR transition surprises | Medium | Early gap analysis; external QMS audit before transition |
| Bias analysis data unavailable | Medium | Use representative phantom datasets + limitation statement in Model Card |
| AIMS vs QMS integration complexity | Medium | Follow Johner Institute dual-certification pattern |

---

## 8. Deliverables

### 8.1 Documents (15)

1. `docs/regulatory/pccp-pre-06.md` — PCCP for ML Defect Correction
2. `docs/regulatory/pccp-post-02.md` — PCCP for DL Denoising
3. `docs/regulatory/pccp-post-07.md` — PCCP for AI Collimation
4. `docs/regulatory/pccp-post-09.md` — PCCP for Bone Suppression
5. `docs/regulatory/model-card-template.md` — Template per FDA 2024-06
6. `docs/regulatory/data-lineage-protocol.md`
7. `docs/regulatory/bias-analysis-protocol.md`
8. `docs/regulatory/eu-ai-act-annex-iv.md` — Technical documentation
9. `docs/regulatory/iso-42001-aims-manual.md`
10. `docs/regulatory/qmsr-gap-analysis.md`
11. `docs/regulatory/human-ai-interaction-design.md`
12. `docs/regulatory/fda-submission-outline.md` — 510(k)/PMA
13. `docs/regulatory/ce-mdr-submission-outline.md`
14. `docs/regulatory/regulatory-traceability-matrix.md` — Master RTM
15. `docs/regulatory/substantial-modification-policy.md` — EU AI Act trigger

### 8.2 Code Artifacts (minimal)

- Header: `modules/common/include/xpe/common/xpe_model_card.h`
- Implementation: `modules/common/src/xpe_model_card.cpp`
- Test: `modules/common/tests/test_model_card.cpp`
- GUI: `gui/ImageProcTest/Views/AiInfoTab.xaml` + code-behind

### 8.3 RTM Entries

- 40+ REQ-REG-XXX entries mapped to SWU-ai.* and SWU-5.x

---

## 9. Dependencies

- **Upstream**: SPEC-XPE-MASTER v3.0.0, trend-survey-2026.md
- **Downstream**: SPEC-XPE-P3-AI v1.0 (AI 구현이 REG governance 준수), SPEC-XPE-SEC (cybersecurity 세부), SPEC-XPE-OPS (post-market)
- **External**: FDA pre-submission (Q-Sub) meeting, ISO 42001 certification body engagement

---

## 10. Change Control

변경 사유:
1. FDA guidance update (subscription monitoring required)
2. EU AI Act 이행법 변경
3. Audit finding
4. Clinical validation outcome

변경 승인:
- Minor (editorial): Author
- Major (정책): Legal counsel + Regulatory Affairs
- Critical (제출 후): FDA pre-submission consult

---

**본 SPEC은 XPE 프로젝트의 최상위 규제 정박점이다. 모든 AI-DSF 개발은 본 문서의 REQ-REG-XXX를 만족해야 한다.**
