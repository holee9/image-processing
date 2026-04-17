# XPE 2026 Trend Survey & Must/Should/Could Matrix

**Document ID**: TREND-SURVEY-2026-001
**Version**: 1.1.0 (Strict Reclassification)
**Date**: 2026-04-17
**Author**: MoAI (deep-survey + brainstorming)
**Status**: Normative Input for SPEC-XPE-MASTER v3.0.0 및 신규 SPEC 4종
**Classification**: IEC 62304 Class B (Input Document)

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-04-17 | Initial survey, Must 27 / Should 19 / Could 10 |
| **1.1.0** | 2026-04-17 | **Strict Must reclassification**: Must 27→12, Should 19→34 (no duplicate). 진행 블로커 아닌 항목 전부 Should로 강등. 사용자 요구 "반드시 필수일때만 must로 분류" 반영. |

---

## 0. 서베이 방법론

### 0.1 범위와 전제

본 서베이는 2024-2026년 의료영상 소프트웨어 도메인의 다음 5개 축을 교차검증으로 분석한다:

1. **Regulatory (규제)** — FDA, EU, IMDRF, ISO, IEC
2. **Cybersecurity (보안)** — SBOM, SLSA, threat modeling, lifecycle security
3. **Interoperability (상호운용성)** — DICOMweb, FHIR R5, IHE RAD
4. **AI/ML Advancement (지능 고도화)** — SSL, diffusion, foundation models, UQ
5. **Operations/Observability (운영)** — drift detection, post-market, telemetry

각 트렌드는 **Must / Should / Could** 3-티어로 분류하며, **사용자 요구사항에 따라 "기본으로 필수인 항목은 반드시 Must"** 로 배치한다.

### 0.2 분류 기준 (v1.1 엄격 버전)

| 티어 | 의미 | 판정 기준 |
|------|------|-----------|
| **Must** | **진짜 블로커**. 없으면 출시 불가 또는 법적 처벌 | (1) 법률 의무 (Guidance 아닌 Law, 현 배포 범위에 적용), (2) 시장 진입 거절 사유, (3) IEC 62304 Class B 강제 조항, (4) 기본 방어적 안전 (입력 검증 등) |
| **Should** | **강력 권장**. 경쟁력·현대 설계·자발적 표준 | 자발적 인증(ISO 42001, IEC 81001-5-1), 최종 아닌 Draft guidance, 프로젝트 아키텍처 원칙, 범위 조건부 규제(FDA PCCP는 AI 배포 시에만 Must), 통합 편의성, 업계 베스트 프랙티스 |
| **Could** | 미래 옵션·실험 | 2027+ 대비, Phase 4 연구 모드 |

**엄격 판정 질문**: "이것이 없으면 프로젝트가 출시하지 못하거나 법적으로 거절됩니까?"
- 예 → Must
- 아니오 → Should (강력 권장이더라도)

**범위 의존 판정 예시**:
- FDA PCCP: AI-DSF 배포 시에만 Must. 결정적 전용 릴리스는 Should
- EU AI Act High-Risk: EU에 AI 판매 시에만 Must. US 결정적 전용은 Should
- IEC 81001-5-1: 자발적 표준 → 항상 Should
- ISO/IEC 42001: 자발적 인증 → 항상 Should

### 0.3 근거 수집 통계

| 축 | WebSearch 횟수 | 주요 공식 소스 |
|----|:-------------:|----------------|
| Regulatory | 9 | FDA.gov, Federal Register, IMDRF, ISO, EU Commission |
| Cybersecurity | 6 | FDA 524B, SPDX.org, CycloneDX.org, SLSA.dev, MITRE |
| Interoperability | 4 | DICOM.org, HL7.org, IHE.net |
| AI/ML | 6 | FDA, MICCAI, Nature, arXiv, GitHub bowang-lab |
| Operations | 3 | FDA, MHRA, OpenTelemetry.io |

**Total**: 28 web queries, 2024-2026 최신성 교차검증 완료.

---

## 1. Must Tier: 진짜 블로커만 (12 항목, v1.1 엄격)

**v1.0 27개 → v1.1 12개**. 15개는 Should로 강등 (§2.0 참조).

### 1.0 Must 최종 목록 (요약)

| ID | 항목 | 블로커 판정 근거 |
|:--:|------|-----------------|
| M-01 | IEC 62304 Class B | 의료기기 SW 법적 분류 (필수 증명) |
| M-02 | EU MDR 2017/745 | EU 시장 진입 법률 (Regulation, directly applicable) |
| M-03 | FDA 21 CFR 820.30 + QMSR 2026 | US 시장 진입 법률 |
| M-04 | FDA §524B Cyber Device | 2023 법률 (DICOM 네트워크 장비 = 강제 cyber device) |
| M-05 | SBOM (§524B 하위 필수) | §524B 법적 구성요소 |
| M-06 | Vulnerability Management | §524B 판매 후 법적 의무 + EU MDR Art 83 |
| M-07 | Basic Input Validation | IEC 62304 + 기본 방어 (defect prevention) |
| M-08 | DICOM 3.0 Core | 병원 PACS 통합 필수 (실질적 시장 블로커) |
| M-09 | DICOM Conformance Statement | 병원 구매 실사 법적 증빙 요구 |
| M-10 | Post-Market Surveillance | EU MDR Article 83 법률 + MHRA Regs |
| M-11 | Characterization Tests | IEC 62304 §5.4.1 강제 unit test 조항 |
| M-12 | Trackability (commits, RTM) | IEC 62304 §5.4.1 추적성 강제 |

### 1.1 Regulatory Baseline (Must · 의무)

**⚠️ [v1.1 강등 → §2.0 Should로 이동]** 이전 v1.0에서 Must로 분류했던 다음 항목들은 v1.1에서 Should로 강등되었다. 근거는 §2.0 참조.

**구 M-REG-01: FDA PCCP (Predetermined Change Control Plan)** — **Should 강등**
- **무엇**: AI-enabled 의료기기가 재승인 없이 모델 업데이트 가능하도록 사전 승인받는 프레임워크
- **근거**: FDA Final Guidance "Marketing Submission Recommendations for a Predetermined Change Control Plan for Artificial Intelligence-Enabled Device Software Functions" (2024-12-03)
- **XPE 적용**: Phase 3 AI 모듈(PRE-06 ML, POST-09 Bone Suppression)의 재훈련 경로 사전 정의
- **강등 이유**: AI-DSF 미배포 시(Phase 1/2 결정적 전용 릴리스) 블로커가 아님. AI 탑재 Phase 3 진입 시 Must로 승격 조건부

**구 M-REG-02: FDA Lifecycle Management Draft (AI-DSF TPLC)** — **Should 강등**
- **무엇**: AI 소프트웨어 기능의 Total Product Life Cycle 접근
- **근거**: FDA Draft Guidance (2025-01-07), 2025-04-07 의견 종료, Final 대기
- **강등 이유**: (1) Draft 상태 (Final 아님), (2) AI-DSF 배포 시에만 해당, (3) 결정적 전용 Phase 1/2 릴리스는 비해당

**구 M-REG-03: FDA Transparency for ML-enabled Medical Devices** — **Should 강등**
- **무엇**: 모델 성능·데이터 특성·알고리즘 설명 투명성 원칙
- **근거**: FDA Guiding Principles (2024-06) — Law가 아닌 Guidance
- **강등 이유**: ML/AI 기능에만 적용. 결정적 릴리스 블로커 아님

**구 M-REG-04: IMDRF GMLP N88 (10 Guiding Principles, 2025)** — **Should 강등**
- **무엇**: FDA+Health Canada+MHRA 공동 Good Machine Learning Practice 10대 원칙
- **근거**: IMDRF/AIML WG/N88 FINAL:2025 (2025-01-27) — Guiding Principle (Law 아님)
- **강등 이유**: AI 관련만 해당, 결정적 릴리스 비해당

**구 M-REG-05: EU AI Act High-Risk Compliance** — **Should 강등 (조건부)**
- **무엇**: MDR Class IIa/IIb/III 의료기기 내 AI 시스템 High-Risk AI 분류
- **근거**: Regulation (EU) 2024/1689, 2024-08 발효, 의료기기 AI 2027-08 전면 적용
- **강등 이유**: EU에 AI 탑재 의료기기 판매 시에만 Must. US-only 또는 결정적 전용 릴리스 비해당
- **승격 조건**: EU 시장 + Phase 3 AI 배포 계획 확정 시 Must로 승격

**M-02 (ID 이전 M-REG-06): EU MDR 2017/745 Compliance** — **Must 유지**
- **무엇**: Medical Device Regulation 준수 (Class B 의료기기 소프트웨어)
- **근거**: Regulation (EU) 2017/745, MDCG 2019-11 업데이트, MDCG 2025-4, MDCG 2025-9
- **XPE 적용**: 기존 IEC 62304 Class B 준수 유지 + MDCG 가이던스 매트릭스
- **Why Must**: EU 시장 진입 법률 (Regulation, 직접 적용)

**구 M-REG-07: ISO/IEC 42001:2023 AI Management System** — **Should 강등**
- **무엇**: 세계 최초 AI 관리 시스템 표준
- **근거**: ISO/IEC 42001:2023 (2023-12) — 자발적 인증
- **강등 이유**: 법적 의무 아님 (자발적). 강력 권장이나 없어도 출시 가능. 병원 구매 조건으로 확산 중이나 블로커 아님

**M-01 (ID 이전 M-REG-08): IEC 62304 Class B** — **Must 유지**
- **무엇**: 의료기기 소프트웨어 수명주기 프로세스
- **근거**: IEC 62304:2006/Amd.1:2015, 프로젝트 기본 준거
- **XPE 적용**: 기존 SRS, SDD, SWU, RTM 체계 유지
- **Why Must**: 의료기기 SW 분류 법적 증명 근거. Class B SW 없이는 IEC 62304 준수 불가능

**M-03 (ID 이전 M-REG-09): FDA 21 CFR 820.30 Design Controls + 2026 QMSR** — **Must 유지**
- **무엇**: 디자인 컨트롤, 리스크 관리, 검증/확인
- **근거**: 2026-02-02 QMSR 발효 (구 QSR 대체, ISO 13485 통합)
- **XPE 적용**: 디자인 히스토리 파일(DHF), 리스크 관리 파일(ISO 14971) 유지
- **Why Must**: FDA 승인 법적 의무

### 1.2 Cybersecurity Baseline (Must · 법적 의무만)

**M-04 (이전 M-SEC-01): FDA Section 524B Cybersecurity (Cyber Device)** — **Must 유지**
- **무엇**: 소프트웨어 포함 + 인터넷 연결 의료기기 프리마켓 사이버보안 의무
- **근거**: Consolidated Appropriations Act 2023 §3305 (FD&C Act §524B 신설), FDA Final Guidance (2025-06-27)
- **XPE 적용**: SPDF, 취약점 대응
- **Why Must**: 법률 (Law), XPE DICOM 네트워크 장비 = 강제 cyber device

**M-05 (이전 M-SEC-02): SBOM** — **Must 유지**
- **무엇**: SPDX 3.0 / CycloneDX 1.6 기계 판독 가능 SBOM
- **근거**: FDA §524B 하위 법적 필수 구성요소
- **Why Must**: §524B 준수의 직접 증빙

**구 M-SEC-03: IEC 81001-5-1 Secure Software Lifecycle** — **Should 강등**
- **무엇**: 64개 의료기기 SW 사이버보안 요구사항
- **근거**: IEC 81001-5-1:2021 — 자발적 표준
- **강등 이유**: FDA가 "acceptable framework"로 인용하나 법적 강제 아님. 엄격 기준 적용 시 Should

**M-06 (이전 M-SEC-04): Cybersecurity Risk Management (Postmarket Vulnerability Monitoring)** — **Must 유지**
- **무엇**: 판매 후 취약점 감시·보고·패치 프로세스
- **근거**: FDA §524B (postmarket), EU MDR Article 83 (PMS 일부)
- **Why Must**: 판매 후 법적 의무

**M-07 (이전 M-SEC-05): Basic Input Validation** — **Must 유지**
- **무엇**: 모든 외부 입력(DICOM, config, P/Invoke) 경계 검증
- **근거**: IEC 62304 §5.1.1 방어적 프로그래밍, 기본 안전 원칙
- **XPE 적용**: C ABI 경계에서 `XpeErrorCode` 반환 + 버퍼 크기 검증 + JSON 스키마 검증
- **Why Must**: defect prevention 기본 방어 (IEC 62304 강제 조항과 결합)

### 1.3 Interoperability Baseline (Must · 시장 진입)

**M-08 (이전 M-IOP-01): DICOM 3.0 Core Conformance** — **Must 유지**
- **무엇**: DICOM 3.0 표준 준수
- **근거**: NEMA DICOM standard, 병원 PACS 통합의 de facto 조건
- **Why Must**: DICOM 없이는 어떤 병원도 구매 불가 → 실질적 시장 블로커

**M-09 (이전 M-IOP-02): DICOM Conformance Statement** — **Must 유지**
- **무엇**: 장비·SW가 지원하는 DICOM 기능의 공식 선언 문서
- **근거**: DICOM PS 3.2 명세, 병원 구매 실사 증빙
- **Why Must**: 병원 구매 실사 법적 요구 문서 (구매 블로커)

**구 M-IOP-03: IHE RAD Profiles (SWF.b/PDI/PIR)** — **Should 강등**
- **무엇**: Scheduled Workflow (SWF.b), Portable Data (PDI), Patient Information Reconciliation (PIR)
- **근거**: IHE RAD Technical Framework Rev 23.0
- **강등 이유**: DICOM 핵심만 있으면 통합 가능. IHE는 통합 편의성 향상 수단이지 블로커 아님. Connectathon 참여는 권장이지 강제 아님

### 1.4 Quality Baseline (Must · IEC 62304 강제 조항만)

**M-11 (이전 M-QUAL-03): Characterization Test + Unit Test** — **Must 유지**
- **근거**: IEC 62304 §5.4.1 Unit test 강제 조항, Class B SW 증명 필수
- **XPE 적용**: Phase 1a/1b 전체 SWU에 unit/characterization test
- **Why Must**: IEC 62304 강제 조항 (법적 의무)

**M-12 (이전 Trackability): Trackability (commits, RTM, issue)** — **Must 유지**
- **근거**: IEC 62304 §5.4.1 추적성 강제 조항
- **XPE 적용**: Conventional commits, @MX:DOC 링크, PR 트레이싱, RTM
- **Why Must**: IEC 62304 강제 조항

**⚠️ [v1.1 강등]**:

**구 M-QUAL-01: TRUST 5 Framework** — **Should 강등**
- **강등 이유**: 프로젝트 자체 채택 프레임워크. 개별 구성요소(Tested, Trackable)는 별도 Must에 분산. 전체 브랜드로는 Should 수준

**구 M-QUAL-02: Deterministic Reference + SIMD Parity** — **Should 강등**
- **강등 이유**: 아키텍처 품질 원칙. 권장되나 없어도 출시 가능 (다만 품질 저하)

**구 M-QUAL-04: MX Tag System** — **Should 강등**
- **강등 이유**: 프로젝트 내부 관례. 외부 규제·고객 요구 없음

**구 M-QUAL-05: Anti-Spaghetti Architecture** — **Should 강등**
- **강등 이유**: 내부 설계 원칙. 지키지 않아도 출시 가능 (다만 유지보수 부담)

### 1.5 Operations Baseline (Must · 법적 의무만)

**M-10 (이전 M-OPS-01): Post-Market Surveillance (PMS)** — **Must 유지**
- **무엇**: 판매 후 안전·성능 추적 및 심각 사고 보고
- **근거**: EU MDR 2017/745 Article 83, MHRA PMS Regs (2025-06-16 발효)
- **XPE 적용**: Reject-analysis, DI drift telemetry 최소 구현
- **Why Must**: EU MDR Article 83 법률

**⚠️ [v1.1 강등]**:

**구 M-OPS-02: Reproducible Builds (SLSA L1+)** — **Should 강등**
- **강등 이유**: SBOM/SLSA 전제이나 법적 의무 아님. Reproducibility 없어도 SBOM은 발급 가능

**구 M-OPS-03: Version Control + Traceability**
- **통합**: Trackability로 M-12에 흡수 (IEC 62304 §5.4.1 강제 조항)

### 1.6 AI Baseline (v1.1 전체 Should 강등)

**⚠️ [v1.1 강등]** AI 모듈은 Phase 3 배포 시에만 적용. 결정적 전용 Phase 1/2 릴리스는 AI 비해당 → Must 제외.

**구 M-AI-01: Model Card Documentation** — **Should 강등**
- **강등 이유**: AI-DSF 배포 시에만 필요. FDA Transparency는 Guidance (Law 아님)

**구 M-AI-02: Data Lineage** — **Should 강등**
- **강등 이유**: AI 배포 시에만. GMLP는 Guiding Principle

**구 M-AI-03: Deterministic Fallback** — **Should 강등**
- **강등 이유**: AI 배포 시 안전 원칙. AI 미배포 시 비해당

**승격 조건**: Phase 3 AI 배포 확정 시 위 3항목은 Must로 승격.

---

## 2. Should Tier: 강력 권장 (34 항목, v1.1 확장)

### 2.0 v1.1 신규 추가 (v1.0 Must → Should 강등 15항목)

| 신규 ID | 항목 | 강등 출처 | 재배치 근거 |
|:-------:|------|:---------:|-------------|
| S-REG-03 | FDA PCCP | v1.0 M-REG-01 | AI-DSF 배포 조건부 |
| S-REG-04 | FDA AI-DSF Lifecycle (Draft) | v1.0 M-REG-02 | Draft 상태 + AI 조건부 |
| S-REG-05 | FDA Transparency for ML-MD | v1.0 M-REG-03 | Guidance + AI 조건부 |
| S-REG-06 | IMDRF GMLP N88 | v1.0 M-REG-04 | Guiding Principle |
| S-REG-07 | EU AI Act High-Risk | v1.0 M-REG-05 | EU+AI 조건부 |
| S-REG-08 | ISO/IEC 42001 | v1.0 M-REG-07 | 자발적 인증 |
| S-SEC-05 | IEC 81001-5-1 | v1.0 M-SEC-03 | 자발적 표준 |
| S-IOP-05 | IHE RAD Profiles (SWF.b/PDI/PIR) | v1.0 M-IOP-03 | 통합 편의성 |
| S-QUAL-01 | TRUST 5 Framework | v1.0 M-QUAL-01 | 프로젝트 프레임워크 |
| S-QUAL-02 | Reference + SIMD Parity | v1.0 M-QUAL-02 | 아키텍처 원칙 |
| S-QUAL-03 | MX Tag System | v1.0 M-QUAL-04 | 내부 관례 |
| S-QUAL-04 | Anti-Spaghetti 3-Layer | v1.0 M-QUAL-05 | 내부 설계 |
| S-OPS-05 | Reproducible Builds | v1.0 M-OPS-02 | SLSA 전제 |
| S-AI-06 | Model Card (구 M-AI-01) | v1.0 M-AI-01 | AI 조건부 |
| S-AI-07 | Data Lineage (구 M-AI-02) | v1.0 M-AI-02 | AI 조건부 |
| S-AI-08 | Deterministic Fallback (구 M-AI-03) | v1.0 M-AI-03 | AI 안전 조건부 |

### 2.1 Regulatory Should

**S-REG-01: NIST AI RMF Profile**
- **근거**: NIST AI 100-1 (2023-01), Gen AI Profile AI 600-1 (2024-07-26)
- **주의**: AAMI 지적 — 의료기기 리스크 모델과 직접 맞지 않음. **Profile 스타일로 부분 채택**
- **적용**: 투명성·설명성 부분만 참조 (FDA Guidance와 중첩)

**S-REG-02: Bias Analysis + Fairness Metrics**
- **근거**: FDA Transparency 2024, EU AI Act §10 (데이터 거버넌스)
- **적용**: 인구통계(성별·연령·인종) 층위 성능 리포트

### 2.2 Cybersecurity Should

**S-SEC-01: SLSA Level 3 Build Provenance**
- **무엇**: 격리된 빌드 환경에서 생성된 서명된 provenance
- **근거**: SLSA v1.0/1.1, in-toto attestation
- **적용**: GitHub Actions reusable workflow로 L2→L3 점진
- **Why Should**: FDA가 법적 요구하진 않으나, 공급망 사고 시 강력한 방어선

**S-SEC-02: STRIDE + MITRE EMB3D Threat Modeling**
- **근거**: MITRE EMB3D (2024 enhanced, IEC 62443-4-2 alignment), MDIC Playbook
- **적용**: xpe_dicom 네트워크 경계 + C# GUI P/Invoke 경계 위협 모델

**S-SEC-03: SBOM Continuous Delivery (CVE Feed 연동)**
- **근거**: NVD CVE API, GitHub Advisory DB
- **적용**: CI에서 SBOM → OSV-Scanner + Grype 자동 스캔

**S-SEC-04: Coordinated Vulnerability Disclosure (CVD)**
- **근거**: FDA 524B, ISO/IEC 29147
- **적용**: SECURITY.md, PGP 키, 개별 disclosure 프로세스

### 2.3 Interoperability Should

**S-IOP-01: DICOMweb (WADO-RS / STOW-RS / QIDO-RS)**
- **무엇**: DICOM의 RESTful 웹 전송 계층
- **근거**: DICOM PS 3.18, AWS HealthImaging (2025-05 QIDO-RS 확장), Orthanc, dcm4che
- **적용**: xpe_dicom에 DICOMweb 클라이언트/서버 선택 지원
- **Why Should**: 2026년 PACS 현대화 추세, 병원 통합 가속

**S-IOP-02: FHIR R5 ImagingStudy + ImagingSelection**
- **근거**: FHIR R5 (Maturity 4 for ImagingStudy), HL7 Imaging Integration WG
- **적용**: DICOM SR → FHIR Observation 매핑 어댑터

**S-IOP-03: IHE AIR / AIRA Profile (AI Results)**
- **근거**: IHE RAD_Suppl_AIR, IHE RAD_Suppl_AIRA (2025-06-12 발행)
- **적용**: AI 출력(Bone Suppression, Collimation)을 AIR 프로필로 전달

**S-IOP-04: DICOM Structured Reports for AI Output**
- **근거**: DICOM Supplement (JSON representation of SR)
- **적용**: AI 결과를 SR로 래핑 → PACS 통합 용이성

### 2.4 AI/ML Should (차별화 기술)

**S-AI-01: Self-Supervised Denoising (Noise2Noise family)**
- **근거**:
  - MICCAI 2025: N2D (Noise2Detail), VQ-SCD, Speckle2Self
  - Noise2Sim (PMC 2023+)
  - Neighboring Slice N2N (arXiv 2024)
- **적용**: Phase 3 POST-02 DL Denoising에 SSL 도입 (ground truth 불필요)
- **Why Should**: 라벨링 비용 대폭 절감, 규제 데이터 확보 부담 완화

**S-AI-02: Diffusion Priors for Low-Dose Enhancement**
- **근거**:
  - Diffusion Probabilistic Priors Zero-Shot (Medical Physics 2025)
  - NEED (Noise-Inspired Diffusion, 2025)
  - DiffDenoise (arXiv 2504.00264)
  - SAD (Structure-Aware Diffusion, 2024)
- **적용**: premium opt-in tier로 diffusion 기반 denoising 제공
- **Why Should**: MICCAI 2024-2025 SOTA, perceptual quality 우위

**S-AI-03: Explainable AI Sidecar (Grad-CAM + SHAP)**
- **근거**:
  - BMC Medical Imaging Systematic Review (2025)
  - FDA Transparency Guidance (2024-06)
- **주의**: Post-hoc 설명성의 한계 인지, clinical misleading 리스크
- **적용**: AI 출력에 saliency map sidecar 병행 (선택적 표시)

**S-AI-04: Conformal Prediction Uncertainty Quantification**
- **근거**: COPA 2025, Nature Digital Medicine, arXiv 2503.23819
- **적용**: Bone Suppression 등에 prediction set + coverage guarantee
- **Why Should**: FDA postmarket monitoring 친화적 통계 보증

**S-AI-05: ONNX Runtime 1.20+ with Multi-EP**
- **근거**: ONNX Runtime TensorRT EP 10.9, DirectML EP, CUDA EP
- **적용**: 배포 환경에 따라 CPU/CUDA/TensorRT/DirectML 선택
- **Why Should**: 기존 ONNX Runtime 1.17 대비 성능·호환성 개선

### 2.5 Operations Should

**S-OPS-01: Data/Concept Drift Detection**
- **근거**:
  - Nature Digital Medicine "Distribution shift detection for postmarket surveillance" (2024)
  - Nature Communications "Empirical data drift detection" (2024)
- **적용**: AI 모듈에 classifier-based + deep kernel + multi-univariate KS 탐지기

**S-OPS-02: OpenTelemetry Instrumentation**
- **근거**: OpenTelemetry 2025 (Profiling signal, edge observability)
- **적용**: xpe_common에 OTEL Tracer/Meter API 통합 (opt-in, off by default)

**S-OPS-03: Reject-Analysis Telemetry**
- **근거**: AAPM TG-151 Ongoing QC, score-plan §6
- **적용**: 재촬영 이벤트 로깅 + 패턴 분석 대시보드

**S-OPS-04: SBOM → VEX Automation**
- **근거**: CycloneDX VEX, OpenVEX, CSAF 2.0
- **적용**: SBOM 취약점 발견 시 VEX 문서 자동 생성 (exploitable/not)

---

## 3. Could Tier: 미래 옵션·실험적 (10 항목)

**C-AI-01: MedSAM / MedSAM2 Foundation Model**
- **근거**: bowang-lab MedSAM (Nature Comms 2024), MedSAM2 (2025-04), LiteMedSAM (10x 가속)
- **적용**: Phase 4 연구 모드 — collimation ROI 또는 anatomy segmentation 사전학습
- **Why Could**: 규제 경계 위험 (foundation model governance 미확립)

**C-AI-02: Federated Learning Infrastructure**
- **적용**: 다중 병원 데이터 주권 유지 학습
- **Why Could**: 연구 프로젝트 단위, post-market 전략

**C-AI-03: Generative AI (RadImageGAN)**
- **적용**: 합성 phantom, 훈련 데이터 증강
- **Why Could**: 데이터 합성 vs 규제 신뢰성 트레이드오프

**C-OPS-01: GPU Offload Production Path (CUDA/DirectML)**
- **적용**: 고해상도 CBCT 또는 실시간 형광투시
- **Why Could**: Phase 3 말 선택, 규제 재승인 리스크 고려

**C-ARCH-01: Rust Safety-Critical Module**
- **적용**: xpe_safety_core 실험 (메모리 안전)
- **Why Could**: FDA 신기술 호평, SOUP 재평가 필요

**C-ARCH-02: WebAssembly Port (Browser Review)**
- **적용**: 교육·2차 의견 수집 뷰어
- **Why Could**: 새로운 배포 형태

**C-SEC-01: Quantum-Resistant Cryptography (NIST PQC)**
- **적용**: 장기 DICOM 서명 보존
- **Why Could**: 2027+ 대비

**C-REG-01: NIST AI RMF Gen AI Profile 전체 채택**
- **적용**: 생성형 AI 기능 도입 시
- **Why Could**: 아직 의료기기 핏 부족

**C-OPS-02: Continuous Learning (Online Adaptation)**
- **Why Could**: PCCP 범위 초과 가능성 → regulatory 리스크 높음

**C-IOP-01: FHIRcast Real-Time Sync**
- **근거**: HL7 FHIRcast (radiology workflow)
- **적용**: 판독 워크스테이션 실시간 동기화
- **Why Could**: 현재 스코프 초과

---

## 4. 분류 요약 매트릭스 (v1.1 엄격 재분류)

### 4.1 최종 매트릭스

| 축 | Must (v1.1) | Should (v1.1) | Could | 합계 |
|----|:----:|:------:|:-----:|:----:|
| Regulatory | 3 (M-01, M-02, M-03) | 8 (기존 2 + 강등 6) | 1 | 12 |
| Cybersecurity | 4 (M-04, M-05, M-06, M-07) | 5 (기존 4 + 강등 1) | 1 | 10 |
| Interoperability | 2 (M-08, M-09) | 5 (기존 4 + 강등 1) | 1 | 8 |
| AI/ML | 0 | 8 (기존 5 + 강등 3) | 3 | 11 |
| Operations | 1 (M-10) | 5 (기존 4 + 강등 1) | 2 | 8 |
| Quality/Architecture | 2 (M-11, M-12) | 4 (강등) | 2 | 8 |
| **Total** | **12** | **35** | **10** | **57** |

### 4.2 v1.0 대비 변화

| 구분 | v1.0 | v1.1 | 변화 |
|------|:----:|:----:|:----:|
| Must | 27 | **12** | **-15** |
| Should | 19 | **35** | **+16** |
| Could | 10 | 10 | 0 |

(총합 증가 55→57: 중복 정리 및 M-QUAL-03/Trackability 분리)

### 4.3 원칙 (v1.1 엄격)

**Must 판정**: "이것이 없으면 출시 불가 또는 법적 거절?" → YES만 Must

**Must 12항목 특성**:
- 8개 항목은 **Law** (법률): IEC 62304, EU MDR, 21 CFR 820/QMSR, FDA §524B, SBOM(§524B 하위), Vulnerability Mgmt(§524B), PMS(EU MDR), Char. Test (IEC 62304)
- 2개 항목은 **실질적 시장 블로커**: DICOM 3.0 Core, DICOM Conformance Statement
- 2개 항목은 **기본 안전·추적성**: Basic Input Validation, Trackability

**Should 35항목**: 강력 권장이나 없어도 출시 가능. Phase 2/3 진입 시 일부는 조건부 Must 승격 (AI-DSF 배포 시, EU 판매 시 등).

---

## 5. SPEC 업그레이드 패키지 매핑 (v1.1 엄격 반영)

### 5.1 신규 SPEC 4종 (Must vs Should 혼합 스펙)

| SPEC | Must (v1.1) | Should (v1.1) | 진행 우선순위 |
|------|-------------|---------------|:------------:|
| **SPEC-XPE-REG** | M-01 (IEC 62304), M-02 (EU MDR), M-03 (21 CFR/QMSR) | S-REG-03~08 (PCCP, AI-DSF, Transparency, GMLP, EU AI Act, ISO 42001) | **Must 부분 P0** / Should 부분 P1 (AI 배포 의존) |
| **SPEC-XPE-SEC** | M-04 (§524B), M-05 (SBOM), M-06 (Vuln Mgmt), M-07 (Input Val) | S-SEC-01 (SLSA L3), S-SEC-05 (IEC 81001-5-1), 기존 S-SEC-02~04 | **Must 부분 P0** / Should 부분 P1 |
| **SPEC-XPE-IOP** | M-08 (DICOM), M-09 (Conformance) | S-IOP-01 (DICOMweb), S-IOP-02 (FHIR R5), S-IOP-03 (AIR/AIRA), S-IOP-05 (IHE Baseline) | **Must 부분 P0** / Should 부분 P2 |
| **SPEC-XPE-OPS** | M-10 (PMS) | S-OPS-01 (Drift), S-OPS-02 (OTEL), S-OPS-05 (Reproducible) 등 | **Must 부분 P0** / Should 부분 P1-P2 |

### 5.2 전략적 결론

**Must 부분은 즉시 착수** (4개 SPEC 합계 12개 Must 항목). **Should 부분은 Phase 연결 조건부**:
- S-REG-03~06 (AI 관련 규제): Phase 3 AI-DSF 배포 승인 시 Must 승격
- S-REG-07 (EU AI Act): EU + AI 판매 결정 시 Must 승격
- S-IOP-01~03: Phase 2-3 상호운용성 고도화 단계
- S-AI-06~08: AI 구현 시작 시

### 5.3 주의: SPEC frontmatter 재조정

신규 4 SPEC의 `priority` 필드는 v1.1에서 다음과 같이 재조정:
- **SPEC-XPE-REG**: `priority: Must (M-01~M-03 base) + Should (AI 관련)` 혼합
- **SPEC-XPE-SEC**: `priority: Must (M-04~M-07) + Should (L3 확장)` 혼합
- **SPEC-XPE-IOP**: `priority: Must (M-08, M-09 DICOM 핵심) + Should (DICOMweb, FHIR, IHE)` 혼합
- **SPEC-XPE-OPS**: `priority: Must (M-10 PMS) + Should (OTEL/Drift/Reproducible)` 혼합

### 5.2 기존 문서 업그레이드

| 문서 | 현재 | 목표 | 주요 변경 |
|------|:----:|:----:|----------|
| SPEC-XPE-MASTER | v2.0.0 | **v3.0.0** | 6개 신규 축 통합, Model Observer Quality Gate, Must/Should/Could 매트릭스 |
| score-improvement-plan | v2.0.0 | **v3.0.0** | 85 → 95 경로, Framework C (Future-Value) 신설 |
| sprint-roadmap | v2.0.0 | **v3.0.0** | 11 → 14 sprints (S-REG, S-SEC, S-IOP, S-OPS 추가) |
| tech.md | v1.0 | **v2.0** | ONNX 1.20+, SBOM 도구, CMake Presets v6, SLSA L3 |
| product.md | v1.0 | **v2.0** | 2026 규제 환경 + Must/Should/Could 전략 |
| 신규 SPEC-XPE-P3-AI | 없음 | **v1.0** | SSL + Diffusion + XAI + Conformal UQ + PCCP 연계 |

---

## 6. 채택 결정 매트릭스

### 6.1 Must 항목 (즉시 채택, 반론 없음)

27 Must 항목 전체: 법률 의무 또는 프로젝트 기본 요건 → **무조건 채택**.

### 6.2 Should 항목 평가

| ID | 품질 upside | 구현 가능성 | 증거 강도 | 규제 경계 | 결정 |
|----|:-----------:|:----------:|:--------:|:---------:|:----:|
| S-REG-01 NIST AI RMF Profile | 중 | 중 | 중 | 낮음 | **부분 채택** |
| S-REG-02 Bias/Fairness | 높음 | 중 | 높음 | 낮음 | **채택** |
| S-SEC-01 SLSA L3 | 높음 | 중 | 높음 | 낮음 | **채택** |
| S-SEC-02 EMB3D Threat Model | 높음 | 높음 | 높음 | 낮음 | **채택** |
| S-SEC-03 SBOM Continuous | 높음 | 높음 | 높음 | 낮음 | **채택** |
| S-SEC-04 CVD Process | 중 | 높음 | 중 | 낮음 | **채택** |
| S-IOP-01 DICOMweb | 높음 | 중 | 높음 | 낮음 | **채택 (Phase 2)** |
| S-IOP-02 FHIR R5 | 중상 | 중 | 높음 | 낮음 | **채택 (Phase 2-3)** |
| S-IOP-03 IHE AIR/AIRA | 높음 | 중 | 높음 | 낮음 | **채택 (Phase 3)** |
| S-IOP-04 DICOM SR for AI | 중 | 높음 | 높음 | 낮음 | **채택** |
| S-AI-01 SSL Denoising | 높음 | 중 | 높음 | 낮음 | **채택 (benchmark gate)** |
| S-AI-02 Diffusion Priors | 높음 | 중하 | 높음 | 중 | **채택 (opt-in premium)** |
| S-AI-03 XAI Sidecar | 중 | 높음 | 중상 | 중(post-hoc misleading) | **제한적 채택** |
| S-AI-04 Conformal UQ | 중상 | 중 | 높음 | 낮음 | **채택** |
| S-AI-05 ONNX 1.20+ | 높음 | 높음 | 높음 | 낮음 | **즉시 채택** |
| S-OPS-01 Drift Detection | 높음 | 중 | 높음 | 낮음 | **채택** |
| S-OPS-02 OpenTelemetry | 중 | 높음 | 중 | 낮음 | **채택 (opt-in)** |
| S-OPS-03 Reject-Analysis | 높음 | 높음 | 높음 | 낮음 | **채택** |
| S-OPS-04 VEX Automation | 중상 | 중 | 높음 | 낮음 | **채택** |

**Should 채택율: 19/19 (전체 채택, 일부 조건부)**

### 6.3 Could 항목 — Phase 4+ 또는 연구 모드

모든 Could 항목은 **Phase 4 Research Track** 또는 **Post-Market Evolution** 로 보류.

---

## 7. 업그레이드 점수 영향 (Framework Reconciliation, v1.1 엄격)

### 7.1 Framework A (Process/Compliance) 영향 (v1.1 재계산)

Must가 12개로 축소되어 "Must 완료 시" 기여가 감소. 하지만 Should가 35개로 확장되어 "Must+Should 완료 시" 점수는 유사하게 유지.

| 영역 | 현재 | Must (12) 완료 | Must + 주요 Should 완료 | Must + 전체 Should |
|------|:----:|:-------------:|:----------------------:|:-------------------:|
| 요구사항 완전성 | 13 | 18 | 23 | 25 |
| 문서 품질 | 17 | 18 | 19 | 20 |
| 아키텍처 설계 | 17 | 18 | 19 | 20 |
| 구현 진행도 | 6 | 12 | 17 | 19 |
| 품질 보증 | 8 | 10 | 13 | 14 |
| **합계** | **61** | **76** (was 87) | **91** | **98** |

**핵심**: Must만 완료해도 76점 달성 (블로커 해소), 주요 Should (Phase별 관련)까지 완료 시 91점, 전체 Should까지 98점.

### 7.2 Framework B (Product/Delivery) 영향 (v1.1 재계산)

| 영역 | 현재 | Must (12) 완료 | Must + 주요 Should 완료 | Must + 전체 Should |
|------|:----:|:-------------:|:----------------------:|:-------------------:|
| 기능 범위 | 26 | 28 | 31 | 34 |
| 성능·메모리 | 12 | 12 | 13 | 14 |
| 알고리즘 품질 | 11 | 12 | 15 | 18 |
| 규제·문서 | 9 | 12 | 14 | 15 |
| 운영 준비도 | 8 | 10 | 12 | 14 |
| **합계** | **66** | **74** (was 85) | **85** | **95** |

**핵심**: Must만으로 74점(블로커 해소 + 출시 가능 상태), 주요 Should까지 85점이 현실적 목표.

### 7.3 Framework C (Future-Value) 신설 제안

Could 항목의 옵션 가치 평가용 신규 프레임워크.

| 영역 | 가중 | 배점 |
|------|:----:|:---:|
| AI Foundation Readiness | 25 | MedSAM/Federated 실험 인프라 |
| Deployment Flexibility | 20 | GPU/WASM/Rust 대안 경로 |
| Long-term Security | 20 | PQC, Zero-trust 준비 |
| Advanced Interoperability | 15 | FHIRcast, breakthrough profiles |
| Research Velocity | 20 | Continuous learning, online UQ |
| **합계** | **100** | 현재 15, 목표 60+ (2027) |

---

## 8. 실행 순서 (Sprint Dependency, v1.1 우선순위 재조정)

```
기존 Critical Path: S0-A → S0-B → S1-A → S1-B1 → S2-A → S3
                                  │
신규 Parallel:  ───┬── S-REG-CORE (M-01, M-02, M-03 base docs) ── 즉시 착수
                   ├── S-SEC-CORE (M-04, M-05, M-06, M-07)    ── S0-B 후 병행
                   ├── S-IOP-CORE (M-08, M-09 DICOM base)     ── S1-B3 후 병행
                   └── S-OPS-CORE (M-10 PMS)                  ── S1-B1 후 병행

 Phase 3 진입 시 Should 부분 승격:
               ── S-REG-AI (S-REG-03~07 PCCP/GMLP 등): Phase 3 승인 후
               ── S-IOP-EXT (DICOMweb/FHIR/IHE AIR): Phase 2-3 필요 시
```

**v1.1 착수 우선순위** (Must 비중 기반):

**Wave P0 (즉시, Must만)**:
1. **S-REG-CORE** (Must 3개): IEC 62304 + EU MDR + 21 CFR baseline 문서
2. **S-SEC-CORE** (Must 4개): §524B + SBOM + Vuln Mgmt + Input Val (S0-B 후)
3. **S-OPS-CORE** (Must 1개): PMS Plan + 보고 체계 (S1-B1 후)
4. **S-IOP-CORE** (Must 2개): DICOM 3.0 + Conformance Statement (S1-B3 후)

**Wave P1 (Should 일부, 조건부)**:
5. **S-SEC-EXT** (Should): SLSA L2→L3, IEC 81001-5-1 통합
6. **S-OPS-EXT** (Should): OTEL, Drift Detection, Reproducible

**Wave P2 (Phase 확장 시)**:
7. **S-IOP-EXT** (Should): DICOMweb, FHIR R5, IHE (Phase 2 상호운용성 확장 결정 시)
8. **S-REG-AI** (Should): FDA PCCP, GMLP, Transparency (Phase 3 AI 착수 시)
9. **S-REG-EU-AI** (Should): EU AI Act (EU+AI 판매 결정 시)
10. **S-AI-GOV**: Model Card, Data Lineage, Deterministic Fallback (AI 구현 시)

**핵심 차이**: v1.0에서는 모든 4개 SPEC이 P0 (즉시)였으나, v1.1에서는 각 SPEC이 **core(Must)** 부분만 P0, **extension(Should)** 부분은 Phase 연결 조건부 진행.

---

## 9. 리스크 및 완화

| 리스크 | 완화책 |
|--------|-------|
| 신규 SPEC 4종 동시 진행으로 스코프 폭발 | EARS 요구사항 수 상한: REG ≤45, SEC ≤40, IOP ≤35, OPS ≤35 |
| 기존 Phase 1a 지연 | 신규 SPEC은 Phase 1a와 병행 가능 (문서 선행, 구현 후행) |
| AI 축 과도 투입 | Must만 Phase 1~2에 강제, Should는 Phase 3 |
| 규제 해석 오류 | 법률 자문 검토 체크포인트 5개 설정 (각 SPEC sign-off) |
| Foundation Model governance 미확립 | Could 티어로 격리, Phase 4 연구 모드 |

---

## 10. 참고 문헌 (Primary Sources)

### 10.1 Regulatory

- [FDA PCCP Final Guidance (2024-12)](https://www.fda.gov/regulatory-information/search-fda-guidance-documents/marketing-submission-recommendations-predetermined-change-control-plan-artificial-intelligence)
- [FDA AI-DSF Lifecycle Draft (2025-01)](https://www.fda.gov/regulatory-information/search-fda-guidance-documents/artificial-intelligence-enabled-device-software-functions-lifecycle-management-and-marketing)
- [FDA Transparency for ML MD (2024-06)](https://www.fda.gov/medical-devices/software-medical-device-samd/transparency-machine-learning-enabled-medical-devices-guiding-principles)
- [IMDRF GMLP N88 Final (2025-01)](https://www.imdrf.org/documents/good-machine-learning-practice-medical-device-development-guiding-principles)
- [EU AI Act Annex III](https://artificialintelligenceact.eu/annex/3/)
- [EU MDR 2017/745](https://eumdr.com/)
- [ISO/IEC 42001:2023](https://www.iso.org/standard/42001)

### 10.2 Cybersecurity

- [FDA Cybersecurity Final (2025-06)](https://www.federalregister.gov/documents/2025/06/27/2025-11669/cybersecurity-in-medical-devices-quality-system-considerations-and-content-of-premarket-submissions)
- [IEC 81001-5-1 Standard](https://www.iso.org/standard/76097.html)
- [SLSA Specification](https://slsa.dev/spec/v1.0/levels)
- [SPDX Specification](https://spdx.dev/)
- [CycloneDX Specification](https://cyclonedx.org/)
- [MITRE EMB3D](https://emb3d.mitre.org/)
- [MDIC Threat Modeling Playbook](https://www.mitre.org/sites/default/files/2021-11/Playbook-for-Threat-Modeling-Medical-Devices.pdf)

### 10.3 Interoperability

- [DICOM Standard](https://www.dicomstandard.org/)
- [DICOMweb Explained](https://en.wikipedia.org/wiki/DICOMweb)
- [FHIR R5 ImagingStudy](https://www.hl7.org/fhir/imagingstudy.html)
- [FHIR R5 ImagingSelection](https://hl7.org/fhir/R5/imagingselection.html)
- [IHE RAD Technical Framework](https://www.ihe.net/resources/technical_frameworks/)
- [IHE AI Results Assessment](https://www.ihe.net/uploadedFiles/Documents/Radiology/IHE_RAD_Suppl_AIRA.pdf)

### 10.4 AI/ML Research

- [MedSAM (Nature Comms 2024)](https://www.nature.com/articles/s41467-024-44824-z)
- [Diffusion Priors Zero-Shot LDCT (MP 2025)](https://aapm.onlinelibrary.wiley.com/doi/10.1002/mp.17431)
- [NEED — Noise-Inspired Diffusion (2025)](https://www.sciencedirect.com/science/article/abs/pii/S1361841525002579)
- [Distribution Shift Detection (Nature Digital Med 2024)](https://www.nature.com/articles/s41746-024-01085-w)
- [Empirical Data Drift Detection (Nature Comms 2024)](https://www.nature.com/articles/s41467-024-46142-w)
- [ONNX Runtime](https://onnxruntime.ai/)
- [Conformal Prediction Medical AI](https://pubmed.ncbi.nlm.nih.gov/39375270/)

### 10.5 Standards Bodies

- NEMA (DICOM), HL7 (FHIR), IHE International, IMDRF, NIST, FDA CDRH, EMA
- IEEE, IEC TC 62A, ISO TC 215, OASIS (CycloneDX), Linux Foundation (SPDX, SLSA)

---

**끝. 이 문서는 SPEC-XPE-MASTER v3.0.0의 normative input이며, 4개 신규 SPEC(REG/SEC/IOP/OPS)의 근거 문서이다.**
