# Security Policy

**Project**: XPE Image Processing Engine
**Document ID**: XPE-SECURITY-001
**Version**: 1.0.0
**SPEC Reference**: SPEC-XPE-SEC REQ-SEC-090~092, REQ-SEC-070~074
**Effective Date**: 2026-04-22
**IEC 62304 Class**: B

---

## HISTORY

| Version | Date | Changes |
|---------|------|---------|
| 0.1.0 | 2026-04-22 | Initial — CVD policy, reporting SLA, scope |
| 1.0.0 | 2026-04-22 | Major upgrade — incident response procedure, security testing plan, regulatory coordination |

---

## Supported Versions

| Version | Supported |
|---------|-----------|
| v1.x (current development) | Security fixes accepted |
| < v1.0 (pre-release) | Not supported |

---

## Reporting a Vulnerability

**Please do not open a public GitHub issue for security vulnerabilities.**

Report vulnerabilities via **private disclosure**:

- **Email**: hnabyz2023@gmail.com
- **Subject line**: `[XPE SECURITY] Brief description`
- **PGP**: Available on request (contact email above)

### What to include

- Description of the vulnerability
- Steps to reproduce
- Affected version(s)
- Impact assessment (if known)
- Proof-of-concept (if available — do not include exploit code targeting production systems)

---

## Response SLA

| Stage | Timeline |
|-------|----------|
| Acknowledgment | Within 72 hours |
| Initial triage | Within 7 business days |
| Remediation plan | Within 30 days for Critical/High (CVSS >= 7.0) |
| Patch delivery | Within 60 days for Critical (CVSS >= 9.0) |
| Coordinated disclosure | 90 days from acknowledgment (extendable by mutual agreement) |

Per FDA Section 524B and ISO/IEC 29147.

---

## Coordinated Disclosure Policy

We follow **Coordinated Vulnerability Disclosure (CVD)** per ISO/IEC 29147:

1. Reporter submits vulnerability privately.
2. XPE team acknowledges within 72 hours.
3. XPE team investigates and develops a fix.
4. Coordinated public disclosure after patch is available (default: 90 days).
5. Reporter credited in the Security Advisory (unless anonymity requested).

If we cannot reproduce the issue or determine it is not a vulnerability, we will explain our reasoning.

---

## Incident Response Procedure (REQ-SEC-070~074)

### Severity Classification

| Level | CVSS | Patient Safety Impact | Example |
|-------|:----:|----------------------|---------|
| **P1 Critical** | >= 9.0 | Immediate — altered diagnosis | RCE in DICOM parser, image data tampering |
| **P2 High** | 7.0~8.9 | Potential — wrong exposure/display | Buffer overflow in image pipeline, auth bypass |
| **P3 Medium** | 4.0~6.9 | Indirect — degraded availability | DoS via malformed DICOM, log injection |
| **P4 Low** | < 4.0 | Minimal | Info disclosure of non-clinical metadata |

### Incident Response Workflow

```
Detection → Triage → Containment → Analysis → Remediation → Verification → Closure
```

**Phase 1: Detection (T+0)**
- Source: External report (CVD), internal scan (SAST/DAST/SBOM), anomaly alert
- Action: Log incident in `docs/security/incidents/YYYY-MMDD-INC-NNN.md`
- Owner: Security Lead

**Phase 2: Triage (T+72h for P1/P2)**
- Assign severity level (CVSS + patient safety override)
- Identify affected modules (DLL boundaries)
- Determine exploitability: local/remote, authenticated/unauthenticated
- Decision: Is clinical operation affected? → If yes, activate containment immediately

**Phase 3: Containment (T+7d for P1)**
- Option A: Disable affected module via configuration (graceful degradation)
- Option B: Network isolation (DICOM SCU/SCP)
- Option C: Hotfix patch deployment
- Notify affected customers via security advisory

**Phase 4: Analysis (concurrent with containment)**
- Root cause analysis (RCA)
- Attack vector identification
- Scope assessment: which DLL boundaries are affected
- Patient safety impact assessment per IMDRF guidance

**Phase 5: Remediation (per SLA)**
- Develop fix on dedicated security branch
- Code review by Security Lead + domain expert
- Regression test: full module test suite + new security test case
- SBOM update if SOUP dependency is involved

**Phase 6: Verification**
- Verify fix against original vulnerability reproduction
- Run full regression suite (all module tests)
- Verify no new vulnerabilities introduced (SAST scan)
- Performance regression check (pipeline timing)

**Phase 7: Closure**
- Publish Security Advisory (GitHub Advisory + VEX document)
- Update threat model if new attack vector discovered
- Post-incident review within 30 days
- Archive incident record

### Regulatory Coordination

| Trigger | Action | Regulatory Basis |
|---------|--------|-----------------|
| P1/P2 vulnerability in deployed device | Notify FDA via eMDR (21 CFR 803) | FDA 524B |
| P1/P2 vulnerability affecting EU market | Notify EU competent authority | EU MDR Art 83, Art 87 |
| Vulnerability requiring field safety corrective action | Field Safety Notice (FSN) | EU MDR Art 89 |
| Vulnerability affecting SOUP dependency | Update SOUP analysis record | IEC 62304 §7.1.3 |

### Incident Communication Matrix

| Stakeholder | P1 | P2 | P3 | P4 |
|-------------|:--:|:--:|:--:|:--:|
| Security Lead | Immediate | T+72h | Weekly review | Monthly review |
| Development Team | T+24h | T+7d | Sprint backlog | Backlog |
| QA Team | T+24h | T+7d | Sprint backlog | Backlog |
| Customers (deployed) | T+7d advisory | T+30d advisory | Release notes | Release notes |
| FDA / Competent Authority | Per 524B timeline | Per MDR timeline | N/A | N/A |

---

## Security Testing Plan (REQ-SEC-060~066)

### Testing Pyramid

| Level | Test Type | Tools | Frequency | Scope |
|-------|-----------|-------|-----------|-------|
| **L1 Static** | SAST | MSVC `/analyze`, CodeQL | Every PR | All C++ sources |
| **L1 Static** | Secrets scanning | gitleaks, trufflehog | Every commit | Full repository |
| **L1 Static** | Dependency scan | OSV-Scanner, Grype | Every CI build | vcpkg SOUP |
| **L2 Dynamic** | Fuzz testing | libFuzzer | Weekly CI | DICOM parser, JSON config |
| **L2 Dynamic** | ABI boundary test | Google Test | Every PR | C ABI input validation |
| **L3 Integration** | Network security | DICOM conformance test | Release cycle | DICOM SCU/SCP |
| **L3 Integration** | Memory safety | AddressSanitizer | Every PR | All DLLs |
| **L4 Penetration** | Simulated attack | Manual + automated | Annual | Full system |

### L1: Static Analysis (Current Implementation)

| Tool | Configuration | Blocking |
|------|---------------|:--------:|
| MSVC `/analyze` | `/WX` — zero warnings | Yes |
| MSVC `/W4` | All modules compiled at W4 | Yes |
| CodeQL | `security-and-quality` query suite | Yes (Phase 2) |
| gitleaks | Pre-commit hook | Yes |

### L1: Input Validation Test Matrix

| Module | Boundary | Test File | REQ |
|--------|----------|-----------|-----|
| xpe_common | XpeImageBuffer size validation | test_xpe_common.cpp | REQ-SEC-060 |
| xpe_preprocess | Offset/Gain/Defect map dimension check | test_boundary.cpp | REQ-SEC-060 |
| xpe_enhance_basic | CLAHE parameter range (clip_limit, tile_size) | test_contrast_enhance.cpp | REQ-SEC-061 |
| xpe_display | VOI LUT width > 0, LUT index bounds | test_voi_lut.cpp | REQ-SEC-060 |
| xpe_dicom | DICOM preamble validation, pixel buffer size | test_dicom_read.cpp | REQ-SEC-060 |
| xpe_dicom | JSON config schema validation | test_config_validation.cpp | REQ-SEC-061 |
| gsvg | Grid map dimension validation | test_gsvg_degraded.cpp | REQ-SEC-060 |
| ImageProcTest | P/Invoke buffer size marshalling | IntegrationTests | REQ-SEC-060 |

### L2: Dynamic Testing (Phase 2 Target)

| Target | Fuzzer | Seed Corpus | Oracle |
|--------|--------|-------------|--------|
| DICOM parser | libFuzzer | DICOM Part 10 sample files | No crash + valid error code |
| JSON config | libFuzzer | Valid config files | Schema validation + no crash |
| Image buffer | libFuzzer | Synthetic 3072x3072 images | No buffer overflow + graceful error |

### L3: Memory Safety (Current Implementation)

All modules tested with AddressSanitizer (ASan):
- Zero memory leaks on all code paths
- Zero buffer overflows detected
- Zero use-after-free detected
- 1000-frame continuous processing without leak growth

---

## Scope

**In scope**:
- `xpe_common.dll`, `xpe_preprocess.dll`, `xpe_enhance_basic.dll`, `xpe_enhance_advanced.dll`
- `xpe_display.dll`, `xpe_dicom.dll`, `gsvg.dll`
- `ImageProcTest.exe` (C# WPF test client)
- CI/CD pipelines and build artifacts
- SBOM components with known vulnerabilities affecting XPE

**Out of scope**:
- Vulnerabilities in hospital PACS/RIS systems
- Physical security of the imaging device
- Hospital network configuration
- Vulnerabilities requiring physical access to the deployment environment

---

## Security Advisories

Security advisories will be published at:
`https://github.com/holee9/image-processing/security/advisories/`

SBOM and VEX documents will be published alongside release artifacts.

---

## Regulatory Notice

XPE is intended for use in medical imaging workflows. Security vulnerabilities may have patient safety implications. We take all reports seriously and will coordinate with applicable regulatory bodies (FDA, competent authorities) when required by FDA Section 524B and EU MDR Article 83.

---

*SPEC-XPE-SEC REQ-SEC-060~074, REQ-SEC-090~092 — ISO/IEC 29147 CVD, FDA 524B*
