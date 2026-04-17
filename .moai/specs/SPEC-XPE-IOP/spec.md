# SPEC-XPE-IOP: Interoperability Master

---
id: SPEC-XPE-IOP
version: 1.1.0
status: Draft
created: 2026-04-17
updated: 2026-04-17
author: MoAI (manager-spec orchestration)
priority: Mixed (Must for M-08, M-09 DICOM core; Should for DICOMweb/FHIR R5/IHE)
issue_number: null
iec62304_class: B
development_mode: TDD
sprint: S-IOP-CORE (Must) + S-IOP-EXT (Should, Phase 2-3 상호운용 확장)
dependency: SPEC-XPE-MASTER v3.0.0, SPEC-XPE-P1B-DICOM (S1-B3) 선행, trend-survey-2026.md v1.1
---

## Priority Reclassification Notice (v1.1)

본 SPEC의 우선순위는 trend-survey-2026.md v1.1 엄격 재분류에 따라 혼합:

- **Must (즉시, 시장 진입 블로커)**: DICOM 3.0 Core Conformance (M-08), DICOM Conformance Statement 발행 (M-09)
- **Should (강력 권장, 조건부)**: DICOMweb (WADO-RS/STOW-RS/QIDO-RS), FHIR R5 ImagingStudy/ImagingSelection, IHE RAD Profiles (SWF.b/PDI/PIR baseline + AIR/AIRA for AI), DICOM SR for AI output
- **강등 근거**: DICOM 핵심만 있으면 병원 PACS 통합 가능. DICOMweb/FHIR/IHE는 통합 편의성·현대성 증진이나 블로커 아님
- **현재 실행 범위**: Must 부분만 즉시, DICOMweb/FHIR은 Phase 2 확장 결정 시, IHE AIR/AIRA는 Phase 3 AI 시

## HISTORY

| Version | Date       | Author       | Changes                                   |
|---------|------------|--------------|-------------------------------------------|
| 1.0.0   | 2026-04-17 | MoAI         | Initial EARS SPEC integrating DICOM 3.0 + DICOMweb + FHIR R5 + IHE RAD (SWF.b/PDI/AIR/AIRA) |

---

## 1. Scope

### 1.1 Overview

본 SPEC은 XPE의 **상호운용성 마스터 문서**로서, 병원 PACS/EMR/RIS 통합을 위한 4개 표준 스택을 정의한다:

1. **DICOM 3.0** (Must, 기존 xpe_dicom.dll 확장)
2. **DICOMweb** (Should, WADO-RS / STOW-RS / QIDO-RS REST 추가)
3. **FHIR R5 ImagingStudy + ImagingSelection** (Should, HL7 연동)
4. **IHE Radiology Profiles** (Must 3 프로필 + Should 2 프로필)

본 SPEC은 기존 SPEC-XPE-P1B-DICOM(S1-B3)을 확장하고, xpe_dicom.dll 및 새로운 인터페이스 모듈(선택적 xpe_interop.dll)을 정의한다.

### 1.2 In Scope

- DICOM Conformance Statement 발행
- DICOMweb 클라이언트 및 서버 구현 (옵션)
- FHIR R5 ImagingStudy/Selection 어댑터
- IHE RAD Profile: SWF.b, PDI, PIR (Must)
- IHE RAD Profile: AIR, AIRA (Should, AI 결과)
- DICOM SR for AI output (JSON representation)
- HL7 FHIR Observation 매핑

### 1.3 Exclusions

- Full PACS server implementation (only client-side + optional server façade)
- FHIR R5 outside ImagingStudy/ImagingSelection/Observation resources
- XDS-I.b implementation (referenced for future, Could tier)
- FHIRcast real-time sync (Could tier)
- Non-radiology IHE domains

---

## 2. Referenced Documents

| Document ID | Title | Version | Role |
|-------------|-------|---------|------|
| DICOM-PS3 | DICOM Standard PS 3.1-3.20 | 2025b current | Normative |
| DICOM-PS3.18 | DICOM PS 3.18 Web Services | 2025b | Normative (DICOMweb) |
| DICOM-PS3.2 | DICOM PS 3.2 Conformance | 2025b | Normative |
| HL7-FHIR-R5 | Fast Healthcare Interoperability Resources R5 | 5.0.0 | Normative |
| HL7-IMG-R5 | ImagingStudy + ImagingSelection | R5 | Normative |
| IHE-RAD-TF | IHE Radiology Technical Framework | Rev 23.0 (2025-08-08) | Normative |
| IHE-RAD-AIR | AI Results | Suppl | Normative (Should) |
| IHE-RAD-AIRA | AI Result Assessment | Suppl (2025-06-12) | Normative (Should) |
| SPEC-XPE-P1B-DICOM | DICOM module | v1.0 | Upstream |
| SPEC-XPE-MASTER | XPE Master | v3.0.0 | Upstream |

---

## 3. Definitions

| Term | Definition |
|------|-----------|
| DICOMweb | RESTful DICOM services (PS 3.18) |
| WADO-RS | Web Access to DICOM Objects - RESTful Services |
| STOW-RS | Store Over The Web - RESTful Services |
| QIDO-RS | Query based on ID for DICOM Objects - RESTful Services |
| UPS-RS | Unified Procedure Step - RESTful Services (partial support only in vendors 2025) |
| FHIR | Fast Healthcare Interoperability Resources |
| ImagingStudy | FHIR resource for DICOM study (Maturity Level 4 in R5) |
| ImagingSelection | FHIR resource for subset of study (R5 new) |
| SOP | Service-Object Pair (DICOM) |
| IHE | Integrating the Healthcare Enterprise |
| SWF.b | Scheduled Workflow (b revision) IHE RAD |
| PDI | Portable Data for Imaging |
| PIR | Patient Information Reconciliation |
| AIR | AI Results (IHE) |
| AIRA | AI Result Assessment (IHE, 2025-06) |
| SR | Structured Report |

---

## 4. Requirements (EARS Format)

### 4.1 DICOM 3.0 Core (Must · 기존 유지/강화)

**REQ-IOP-001** (Ubiquitous): The XPE xpe_dicom.dll shall implement DICOM 3.0 Part 5 (Data Structures and Encoding), Part 6 (Data Dictionary), and Part 10 (Media Storage and File Format).

**REQ-IOP-002** (Ubiquitous): Supported SOP Classes shall include at minimum: Digital X-Ray Image Storage - For Presentation (1.2.840.10008.5.1.4.1.1.1.1), Digital X-Ray Image Storage - For Processing (1.2.840.10008.5.1.4.1.1.1.1.1), Grayscale Softcopy Presentation State Storage (1.2.840.10008.5.1.4.1.1.11.1).

**REQ-IOP-003** (Ubiquitous): Supported Transfer Syntaxes shall include at minimum: Explicit VR Little Endian (1.2.840.10008.1.2.1), JPEG Lossless Non-Hierarchical 14 (1.2.840.10008.1.2.4.57), JPEG 2000 Lossless Only (1.2.840.10008.1.2.4.90).

**REQ-IOP-004** (Ubiquitous): DICOM Network Services shall support SCU and SCP roles for C-STORE, C-FIND, C-ECHO.

**REQ-IOP-005** (Ubiquitous): A DICOM Conformance Statement per PS 3.2 shall be published with each release identifying supported SOP Classes, Transfer Syntaxes, and extensions.

### 4.2 DICOMweb (Should · Phase 2)

**REQ-IOP-010** (Ubiquitous): The xpe_interop module (new) SHALL implement DICOMweb Client per PS 3.18.

**REQ-IOP-011** (Ubiquitous): WADO-RS shall support: retrieveStudy, retrieveSeries, retrieveInstance, retrieveFrames, retrieveBulkdata, retrieveMetadata.

**REQ-IOP-012** (Ubiquitous): STOW-RS shall support: storeInstances with multipart/related (application/dicom).

**REQ-IOP-013** (Ubiquitous): QIDO-RS shall support: searchForStudies, searchForSeries, searchForInstances with query parameter filters.

**REQ-IOP-014** (Event-driven): When DICOMweb server returns JSON media type (application/dicom+json), the client shall parse using DICOM JSON representation per PS 3.18 Annex F.

**REQ-IOP-015** (Event-driven): When HTTP 429 (Too Many Requests) or 503 (Service Unavailable) is received, the client shall implement exponential backoff retry with max 3 attempts.

**REQ-IOP-016** (Ubiquitous): DICOMweb transport shall support HTTPS with TLS 1.2+ (cross-reference SPEC-XPE-SEC §4.8).

**REQ-IOP-017** (Ubiquitous): DICOMweb authentication shall support Basic Auth (testing), Bearer Token (OAuth 2.0), and optional mTLS.

### 4.3 FHIR R5 (Should · Phase 2-3)

**REQ-IOP-020** (Ubiquitous): The xpe_interop module SHALL provide FHIR R5 ImagingStudy resource generation from DICOM study metadata.

**REQ-IOP-021** (Ubiquitous): ImagingStudy resource shall populate: subject (Patient reference), started, numberOfSeries, numberOfInstances, modality, series[*], series[*].instance[*].

**REQ-IOP-022** (Ubiquitous): ImagingSelection resource generation SHALL be supported when AI output identifies a ROI or specific frames.

**REQ-IOP-023** (Ubiquitous): FHIR Observation resource generation SHALL map DICOM Structured Report (SR) AI outputs following HL7 Imaging Integration mapping guide.

**REQ-IOP-024** (Ubiquitous): FHIR transport shall support Content-Type application/fhir+json and application/fhir+xml.

**REQ-IOP-025** (Ubiquitous): FHIR operations supported: create, read, search (patient scoped).

### 4.4 IHE Radiology Profiles

#### 4.4.1 SWF.b (Must)

**REQ-IOP-030** (Ubiquitous): The XPE system shall participate in IHE Scheduled Workflow (SWF.b) as Image Manager / Image Archive actor for the image management transactions.

**REQ-IOP-031** (Ubiquitous): The system shall support Modality Images Stored (RAD-8) transaction.

**REQ-IOP-032** (Ubiquitous): The system shall support Query Images (RAD-14) and Retrieve Images (RAD-16) transactions.

#### 4.4.2 PDI (Must)

**REQ-IOP-040** (Ubiquitous): The XPE system shall support Portable Data for Imaging (PDI) profile as Portable Media Creator or Importer.

**REQ-IOP-041** (Ubiquitous): PDI media shall conform to DICOM Part 10 file format with DICOMDIR indexing.

#### 4.4.3 PIR (Must)

**REQ-IOP-050** (Ubiquitous): The XPE system shall support Patient Information Reconciliation (PIR) for demographic corrections propagated from Patient Demographics Supplier actor.

#### 4.4.4 AIR (Should)

**REQ-IOP-060** (Ubiquitous): The XPE system SHALL participate in IHE AI Results (AIR) profile as AI Model Producer.

**REQ-IOP-061** (Ubiquitous): AI outputs from POST-09 Bone Suppression, POST-07 AI Collimation, PRE-06 ML Defect shall be encoded per AIR transaction requirements.

**REQ-IOP-062** (Ubiquitous): AI output encoding shall use DICOM SR, Segmentation, or Parametric Map as appropriate per AI Model type.

#### 4.4.5 AIRA (Should)

**REQ-IOP-070** (Ubiquitous): The XPE system SHALL support IHE AI Result Assessment (AIRA, 2025-06-12) profile for clinical user assessment feedback of AI outputs.

**REQ-IOP-071** (Ubiquitous): AIRA transaction shall record: user ID, assessment value (accepted/modified/rejected), rationale, timestamp.

**REQ-IOP-072** (Event-driven): When an AI output is assessed, the system shall persist AIRA record per site data retention policy.

### 4.5 DICOM SR for AI Output

**REQ-IOP-080** (Ubiquitous): AI module outputs shall be encodable as DICOM Structured Reports using templates consistent with DICOM Working Group 21 AI guidelines.

**REQ-IOP-081** (Ubiquitous): AI-generated SR shall include: model identifier, model version, confidence score, saliency reference (if enabled per SPEC-XPE-REG §4.3), timestamp, input image reference.

**REQ-IOP-082** (Ubiquitous): SR encoding shall support JSON representation per DICOM Supplement (JSON Representation of Structured Reports) for easier integration with modern workflows.

### 4.6 Patient Privacy and De-identification

**REQ-IOP-090** (Ubiquitous): DICOMweb exports SHALL support DICOM PS 3.15 de-identification profile selection.

**REQ-IOP-091** (Ubiquitous): FHIR export SHALL NOT include PHI beyond what is explicitly required for the clinical use case.

### 4.7 Testing and Validation

**REQ-IOP-100** (Ubiquitous): The system SHALL participate in annual IHE Connectathon for SWF.b, PDI, PIR validation.

**REQ-IOP-101** (Ubiquitous): DICOM Conformance SHALL be validated using dcm4che tools (dcm4chee, dcmqrscp).

**REQ-IOP-102** (Ubiquitous): FHIR conformance SHALL be validated using HL7 FHIR Validator (Java).

---

## 5. Acceptance Criteria

### 5.1 Conformance Statements

- [ ] DICOM Conformance Statement v1.0 published (PS 3.2 format)
- [ ] DICOMweb Conformance document (list of supported operations)
- [ ] FHIR CapabilityStatement resource for supported operations
- [ ] IHE Integration Profile Statement for SWF.b + PDI + PIR

### 5.2 Code Artifacts

- [ ] xpe_dicom.dll passes dcm4che conformance test suite
- [ ] xpe_interop.dll (new) implements DICOMweb client with ≥ 85% test coverage
- [ ] FHIR ImagingStudy resource generator with unit tests
- [ ] AI SR encoder with test fixtures for each AI module

### 5.3 Connectathon Readiness

- [ ] Tested against dcm4chee test instance
- [ ] Tested against Orthanc test server
- [ ] Tested against AWS HealthImaging demo endpoint

### 5.4 Documentation

- [ ] Integration guide for hospital IT
- [ ] SDK samples (C#, Python) for DICOMweb client

---

## 6. Out-of-Scope Clarifications

- XDS-I.b cross-enterprise sharing → Future Could
- FHIRcast real-time sync → Future Could
- Full FHIR server implementation → out of scope (client/adapter only)
- Non-imaging FHIR resources beyond minimal Patient/Observation → out of scope

---

## 7. Risks and Mitigations

| Risk | Severity | Mitigation |
|------|:--------:|-----------|
| DICOM Conformance gaps discovered late | Medium | Early Connectathon prep, automated test suite |
| DICOMweb endpoint performance issues | Medium | Benchmark harness, retry/backoff implementation |
| FHIR R5 version divergence with EHR vendors (still on R4) | Medium | Support both R4 and R5 capability statements |
| PHI leak via FHIR exports | High | Strict de-identification validation; audit on export |
| IHE AIRA profile newness (2025-06) | Medium | Monitor IHE Connectathon 2026 for early adopters |

---

## 8. Deliverables

### 8.1 Documents (8)

1. `docs/interop/dicom-conformance-statement.md`
2. `docs/interop/dicomweb-conformance.md`
3. `docs/interop/fhir-capability-statement.md`
4. `docs/interop/ihe-integration-statement.md`
5. `docs/interop/ai-sr-encoding-guide.md`
6. `docs/interop/integration-guide-for-it.md`
7. `docs/interop/fhir-observation-mapping.md` — DICOM SR → FHIR
8. `docs/interop/hospital-connectathon-runbook.md`

### 8.2 Code Modules

- `modules/dicom/` (existing, enhanced per §4.1)
- `modules/interop/` (new, contains DICOMweb client + FHIR adapter)
  - `xpe_interop.dll` Layer 1 (no lateral deps)
  - ~10-12 API functions (REST, FHIR)
- SDK samples: `samples/dicomweb-csharp/`, `samples/fhir-python/`

### 8.3 RTM Entries

- 30+ REQ-IOP-XXX entries mapped to SWU-interop.*

---

## 9. Dependencies

- **Upstream**: SPEC-XPE-P1B-DICOM v1.0 (S1-B3), SPEC-XPE-MASTER v3.0.0, SPEC-XPE-P3-AI (for AI SR)
- **Downstream**: SPEC-XPE-OPS (telemetry of interop operations)
- **External**: dcm4che (Java library) test setup, Orthanc reference server, HAPI FHIR

---

## 10. Change Control

- DICOM standard updates (annual): review every Q1
- IHE TF updates (semi-annual): review at publication
- FHIR R6 ballot: monitor

---

**본 SPEC은 XPE의 상호운용성 마스터로서 2026+ PACS/EMR 현대화 환경에서 XPE를 네트워크 일원으로 통합한다.**
