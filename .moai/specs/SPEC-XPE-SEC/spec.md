# SPEC-XPE-SEC: Cybersecurity Master

---
id: SPEC-XPE-SEC
version: 1.1.0
status: Draft
created: 2026-04-17
updated: 2026-04-17
author: MoAI (manager-spec orchestration)
priority: Mixed (Must for M-04~M-07 core; Should for SLSA L3, IEC 81001-5-1, threat model 확장)
issue_number: null
iec62304_class: B
development_mode: TDD
sprint: S-SEC-CORE (Must) + S-SEC-EXT (Should)
dependency: SPEC-XPE-MASTER v3.0.0, trend-survey-2026.md v1.1, S0-B
---

## Priority Reclassification Notice (v1.1)

본 SPEC의 우선순위는 trend-survey-2026.md v1.1 엄격 재분류에 따라 혼합:

- **Must (즉시 착수, §524B 법적 의무)**: FDA Section 524B cyber device 요건, SBOM (SPDX 3.0 + CycloneDX 1.6) 발급, Vulnerability Management, Basic Input Validation
- **Should (권장, Phase별 조건부)**: SLSA L2→L3 upgrade, IEC 81001-5-1 SDLC 통합, STRIDE + MITRE EMB3D 고도화, CVD 공개 절차, 재현 가능 빌드
- **현재 실행 범위**: Must 부분(§524B 필수)은 즉시, Should 부분은 Phase 2에 점진적 확장

## HISTORY

| Version | Date       | Author       | Changes                                   |
|---------|------------|--------------|-------------------------------------------|
| 1.0.0   | 2026-04-17 | MoAI         | Initial EARS SPEC integrating FDA §524B, SBOM (SPDX 3.0/CycloneDX 1.6), IEC 81001-5-1, SLSA L3, STRIDE+EMB3D, CVD |

---

## 1. Scope

### 1.1 Overview

본 SPEC은 XPE 프로젝트의 **사이버보안 마스터 문서**로서, FDA Section 524B 의무와 IEC 81001-5-1 Secure Software Lifecycle을 단일 통합 프레임워크로 정리한다. 본 문서는 IEC 62304 및 SPEC-XPE-REG에 **오버레이**로 기능한다.

본 SPEC이 다루는 6개 보안 축:

1. **FDA Section 524B** (Cyber Device premarket cybersecurity, 2023 법제화, 2025-06 final guidance)
2. **SBOM** (SPDX 3.0 / CycloneDX 1.6, FDA NTIA 최소 요소)
3. **IEC 81001-5-1** (Secure Software Lifecycle, 64 cybersecurity requirements)
4. **SLSA Levels 1→3** (Build provenance and supply chain integrity)
5. **Threat Modeling** (STRIDE + MITRE EMB3D + MDIC Playbook)
6. **Coordinated Vulnerability Disclosure** (CVD, ISO/IEC 29147)

### 1.2 In Scope

- Secure Product Development Framework (SPDF) per §524B
- SBOM generation, distribution, continuous VEX tracking
- Threat model for each Layer 1 DLL + GUI + network boundaries
- Vulnerability management and coordinated disclosure process
- Build attestation (SLSA L3 with in-toto provenance)
- Security testing (SAST/DAST/fuzzing/dependency scanning)
- Incident response plan
- Cryptographic controls (transport, at-rest, signature)
- Security-related user documentation

### 1.3 Exclusions

- Regulatory governance: SPEC-XPE-REG
- Interoperability (DICOM TLS specifics): SPEC-XPE-IOP
- AI-specific security (model poisoning, adversarial): SPEC-XPE-P3-AI §8
- Physical device security: out of scope (SW only)
- Privacy/PHI beyond DICOM de-identification: out of scope

---

## 2. Referenced Documents

| Document ID | Title | Version | Role |
|-------------|-------|---------|------|
| FDA-524B | FD&C Act Section 524B (Consolidated Appropriations Act 2023 §3305) | 2023 | Normative (law) |
| FDA-CYBER-2025 | Cybersecurity in Medical Devices: QS Considerations and Premarket Submissions | Final 2025-06-27 | Normative |
| IEC-81001-5-1 | Health software security lifecycle | 2021 + Interpretation Sheet 2025-12 | Normative |
| SPDX-3.0 | Software Package Data Exchange | 3.0 (2024) | Normative (choice) |
| CycloneDX-1.6 | OWASP CycloneDX | 1.6 (2024) | Normative (choice) |
| SLSA-v1.1 | Supply Chain Levels for Software Artifacts | v1.1 | Normative |
| NIST-SSDF | Secure Software Development Framework | SP 800-218 | Informative |
| NIST-CSWP-27 | Recommended Minimum Standard for Vendor or Developer Verification | 2021 | Informative |
| MITRE-EMB3D | Embedded Device Threat Model | 2024 enhanced with IEC 62443-4-2 | Informative |
| MDIC-PTM | Playbook for Threat Modeling Medical Devices | 2021 | Informative |
| ISO-IEC-29147 | Vulnerability disclosure | 2018 | Normative |
| ISO-IEC-30111 | Vulnerability handling processes | 2019 | Normative |
| OWASP-ASVS | Application Security Verification Standard | 4.0.3 | Informative |
| SPEC-XPE-REG | Regulatory Master | v1.0.0 | Upstream |
| SPEC-XPE-MASTER | XPE Master Implementation Plan | v3.0.0 | Upstream |

---

## 3. Definitions

| Term | Definition |
|------|-----------|
| Cyber Device | §524B: SW-enabled + internet-capable medical device |
| SPDF | Secure Product Development Framework |
| SBOM | Software Bill of Materials |
| VEX | Vulnerability Exploitability eXchange |
| SLSA | Supply-chain Levels for Software Artifacts |
| CVD | Coordinated Vulnerability Disclosure |
| SAST | Static Application Security Testing |
| DAST | Dynamic Application Security Testing |
| STRIDE | Spoofing, Tampering, Repudiation, Information disclosure, Denial of service, Elevation of privilege |
| EMB3D | MITRE Embedded Device Threat Model |
| in-toto | Supply chain attestation framework |
| CSAF | Common Security Advisory Framework |
| SOUP | Software of Unknown Provenance (IEC 62304) |
| CWE | Common Weakness Enumeration |
| CAPEC | Common Attack Pattern Enumeration and Classification |

---

## 4. Requirements (EARS Format)

### 4.1 Cyber Device Classification

**REQ-SEC-001** (Ubiquitous): The XPE system shall determine its classification as Cyber Device per §524B based on: (1) inclusion of software, (2) ability to connect to internet, (3) presence of vulnerable technological characteristics.

**REQ-SEC-002** (State-driven): If XPE is deployed in a networked environment with DICOM Network SCU/SCP, the system shall be classified as Cyber Device subject to §524B.

### 4.2 Secure Product Development Framework (SPDF)

**REQ-SEC-010** (Ubiquitous): The XPE project shall implement SPDF practices aligned with IEC 81001-5-1.

**REQ-SEC-011** (Ubiquitous): Each software unit (SWU) shall undergo threat modeling per STRIDE and cross-reference with MITRE EMB3D.

**REQ-SEC-012** (Ubiquitous): Security requirements shall be traced to design, implementation, and test artifacts.

**REQ-SEC-013** (Event-driven): When a new SOUP is introduced, the system shall perform security risk assessment including known vulnerabilities, support lifecycle, and attack surface impact.

**REQ-SEC-014** (Ubiquitous): Secure coding guidelines shall be enforced for C++ (CERT C++), C ABI boundary hardening, and C# (OWASP .NET).

### 4.3 SBOM Requirements

**REQ-SEC-020** (Ubiquitous): The XPE build shall generate a SBOM in both SPDX 3.0 AND CycloneDX 1.6 formats (machine-readable).

**REQ-SEC-021** (Ubiquitous): The SBOM shall contain NTIA minimum elements: supplier name, component name, version string, unique identifiers, dependency relationships, author, timestamp.

**REQ-SEC-022** (Ubiquitous): The SBOM shall list all commercial, open-source, and off-the-shelf software components integrated into the device.

**REQ-SEC-023** (Ubiquitous): For each SOUP component, the SBOM shall include: CPE or PURL identifier, support end date, license identifier (SPDX).

**REQ-SEC-024** (Event-driven): When any SOUP version changes, the SBOM shall be regenerated and a new version recorded.

**REQ-SEC-025** (Ubiquitous): The SBOM shall be distributed with each release through machine-readable channel (public URL, API, or distribution package).

**REQ-SEC-026** (Ubiquitous): SBOM versions shall be retained per device configuration for the expected support window (10+ years for medical devices).

### 4.4 Vulnerability Management

**REQ-SEC-030** (Ubiquitous): The XPE project shall monitor published vulnerabilities (NVD CVE, GitHub Advisory Database, OSV) for all SBOM components.

**REQ-SEC-031** (Event-driven): When a vulnerability is published affecting an SBOM component, the system shall perform exploitability analysis within 48 hours.

**REQ-SEC-032** (Event-driven): When exploitability analysis completes, a VEX document (CycloneDX VEX or OpenVEX or CSAF 2.0) shall be generated with status: affected, not_affected, fixed, or under_investigation.

**REQ-SEC-033** (Ubiquitous): Critical CVEs (CVSS ≥ 9.0) shall have a mitigation plan within 7 days and patch within 60 days.

**REQ-SEC-034** (Ubiquitous): The project shall publish a Security Advisory for each confirmed exploitable vulnerability per ISO/IEC 29147.

### 4.5 Supply Chain Security (SLSA)

**REQ-SEC-040** (Ubiquitous): Builds shall achieve SLSA Level 1 minimum: provenance exists describing how the package was built.

**REQ-SEC-041** (Ubiquitous): Release builds shall achieve SLSA Level 2: provenance is digitally signed by the build platform.

**REQ-SEC-042** (Ubiquitous): Release builds SHALL achieve SLSA Level 3 (Phase 2 target): build steps run in isolated environment; provenance is unforgeable.

**REQ-SEC-043** (Ubiquitous): Builds shall generate in-toto attestations alongside artifacts.

**REQ-SEC-044** (Ubiquitous): Release artifacts shall be reproducible: identical source + build environment produces bit-identical output (SOURCE_DATE_EPOCH for CMake/Ninja).

### 4.6 Threat Model

**REQ-SEC-050** (Ubiquitous): A STRIDE-based threat model shall be maintained for each trust boundary: (a) DICOM network SCU/SCP, (b) P/Invoke C ABI boundary, (c) File system I/O (config, images, logs), (d) C# GUI process boundary, (e) vcpkg SOUP consumption.

**REQ-SEC-051** (Ubiquitous): Each identified threat shall be mapped to MITRE ATT&CK technique and MITRE EMB3D property.

**REQ-SEC-052** (Ubiquitous): Each identified threat shall have tiered mitigation: foundational, intermediate, leading (per EMB3D 2024 enhancement).

**REQ-SEC-053** (Ubiquitous): The threat model shall be reviewed at each major release (v1.0, v2.0, etc.) and when significant architecture changes occur.

### 4.7 Secure Coding and Testing

**REQ-SEC-060** (Ubiquitous): All C ABI boundary functions shall validate input buffer sizes before dereferencing.

**REQ-SEC-061** (Ubiquitous): All C ABI boundary functions shall validate JSON config against schema before use.

**REQ-SEC-062** (Ubiquitous): All DICOM parsing shall use DCMTK with strict mode enabled and fuzz-tested against malformed inputs.

**REQ-SEC-063** (Ubiquitous): Static analysis shall run in CI with rules from CERT C++, MISRA C++:2008 (subset), and CWE Top 25.

**REQ-SEC-064** (Ubiquitous): Fuzz testing shall run against: (a) DICOM parser, (b) JSON config parser, (c) image header validators.

**REQ-SEC-065** (Ubiquitous): Dependency scanning (OSV-Scanner, Grype) shall run on every CI build.

**REQ-SEC-066** (Ubiquitous): Secrets scanning (gitleaks, trufflehog) shall run on every commit.

### 4.8 Cryptographic Controls

**REQ-SEC-070** (Ubiquitous): DICOM Network transport shall support TLS 1.2+ with recommended cipher suites per NIST SP 800-52 Rev. 2.

**REQ-SEC-071** (Ubiquitous): Audit logs shall be hash-chained (SHA-256) to provide tamper evidence.

**REQ-SEC-072** (Ubiquitous): Configuration files may be digitally signed (Ed25519 or ECDSA P-256) when tamper detection is required.

**REQ-SEC-073** (Ubiquitous): Release binaries shall be signed (Authenticode for Windows).

### 4.9 Incident Response

**REQ-SEC-080** (Event-driven): When a security incident is detected or reported, an incident response workflow per ISO/IEC 30111 shall activate.

**REQ-SEC-081** (Ubiquitous): Incident response shall include: acknowledgment (within 72 hours per FDA §524B), triage, containment, eradication, recovery, lessons learned.

**REQ-SEC-082** (Ubiquitous): Customer notification for confirmed exploitable vulnerabilities shall follow FDA §524B and EU MDR Article 83.

### 4.10 Coordinated Vulnerability Disclosure

**REQ-SEC-090** (Ubiquitous): The project shall maintain a public SECURITY.md with disclosure policy, contact email, PGP key, and response SLA.

**REQ-SEC-091** (Ubiquitous): A vulnerability disclosure tracking system shall be maintained (private issue tracker).

**REQ-SEC-092** (Ubiquitous): Disclosure shall follow coordinated timeline: 90-day default, extendable by mutual agreement, per ISO/IEC 29147.

---

## 5. Acceptance Criteria

### 5.1 SBOM Artifacts

- [ ] `sbom/xpe-{version}.spdx.json` generated per release
- [ ] `sbom/xpe-{version}.cdx.json` generated per release
- [ ] SBOM includes all 8 XPE DLLs + GSVG + vcpkg SOUP
- [ ] SBOM published at `https://github.com/holee9/image-processing/releases/download/{version}/sbom-xpe-{version}.*`

### 5.2 Provenance & Attestation

- [ ] SLSA L2 attestation present on every release
- [ ] SLSA L3 attestation present on every release from v2.0+
- [ ] Reproducible build verified (bit-identical hash across 2+ runs)

### 5.3 Vulnerability & Scanning

- [ ] OSV-Scanner reports 0 critical unresolved
- [ ] Grype reports 0 critical unresolved (or VEX-justified)
- [ ] gitleaks reports 0 secrets on main branch
- [ ] CodeQL/SonarQube static analysis passes with 0 criticals

### 5.4 Threat Model

- [ ] 5 trust boundary threat models documented
- [ ] ATT&CK + EMB3D mapping for each threat
- [ ] Tiered mitigations specified

### 5.5 Process

- [ ] SECURITY.md published
- [ ] VDP contact verified
- [ ] Incident response runbook tested with tabletop exercise
- [ ] IEC 81001-5-1 audit checklist green

---

## 6. Out-of-Scope Clarifications

- Privacy (HIPAA, GDPR) beyond DICOM de-identification → hospital IT domain
- Physical tamper resistance → hardware team
- Network-level security (firewall rules) → hospital IT
- Federated learning security → SPEC-XPE-P3-AI §8 (separate)

---

## 7. Risks and Mitigations

| Risk | Severity | Mitigation |
|------|:--------:|-----------|
| FFTW3 GPL license contamination (GSVG) | Medium | Dynamic link only, GSVG as separate DLL, documented in SBOM |
| OpenSSL (DCMTK transitive) export restrictions | Medium | Per-jurisdiction legal review, documented in tech.md |
| SLSA L3 infra not available | High | Use GitHub Actions hosted runners with SLSA generator reusable workflow |
| SOUP with no active maintenance | High | Replace or fork + support internally; mark in SBOM |
| Adversarial model attacks (AI modules) | Medium | Defer to SPEC-XPE-P3-AI §8 (specialized) |
| Zero-day in C++ runtime | High | Rapid patch SLA (60d critical), coordinate with Microsoft |
| Shadow IT P/Invoke misuse | Low | Sign ImageProcTest.exe with Authenticode, enforce loader |

---

## 8. Deliverables

### 8.1 Documents (11)

1. `docs/security/spdf-plan.md` — Secure Product Development Framework
2. `docs/security/threat-model-dicom.md`
3. `docs/security/threat-model-cabi.md`
4. `docs/security/threat-model-fsio.md`
5. `docs/security/threat-model-gui.md`
6. `docs/security/threat-model-soup.md`
7. `docs/security/vulnerability-management-plan.md`
8. `docs/security/incident-response-plan.md`
9. `docs/security/cryptographic-controls.md`
10. `SECURITY.md` — Public disclosure policy
11. `docs/security/iec-81001-5-1-compliance-matrix.md`

### 8.2 Automation/Code Artifacts

- `.github/workflows/sbom.yml` — SPDX + CycloneDX generation
- `.github/workflows/slsa.yml` — SLSA L2→L3 attestation
- `.github/workflows/scan.yml` — OSV-Scanner + Grype + gitleaks + CodeQL
- `scripts/gen_sbom.sh` — Local SBOM generation
- `scripts/verify_reproducible.sh` — Reproducibility check
- Header: `modules/common/include/xpe/common/xpe_secure.h` (crypto helpers)

### 8.3 RTM Entries

- 40+ REQ-SEC-XXX entries mapped to security controls

---

## 9. Dependencies

- **Upstream**: SPEC-XPE-MASTER v3.0.0, SPEC-XPE-REG v1.0, S0-B (xpe_common) completion
- **Downstream**: All Layer 1 DLL implementations (P1A, P1B*, P2, P3), SPEC-XPE-P3-AI §8
- **External**: GitHub Actions (CI), SLSA generator, OSV.dev

---

## 10. Change Control

- Security policy changes require Security Lead approval
- New threats discovered: add to threat model within 7 days
- CVE published affecting XPE: immediate incident response

---

**본 SPEC은 XPE 프로젝트의 사이버보안 마스터로서 FDA §524B 법적 의무와 IEC 81001-5-1 표준을 구현한다.**
