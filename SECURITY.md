# Security Policy

**Project**: XPE Image Processing Engine  
**SPEC Reference**: SPEC-XPE-SEC REQ-SEC-090  
**Effective Date**: 2026-04-22

---

## Supported Versions

| Version | Supported |
|---------|-----------|
| v1.x (current development) | ✅ Security fixes accepted |
| < v1.0 (pre-release) | ❌ Not supported |

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
| Remediation plan | Within 30 days for Critical/High (CVSS ≥ 7.0) |
| Patch delivery | Within 60 days for Critical (CVSS ≥ 9.0) |
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

*SPEC-XPE-SEC REQ-SEC-090, REQ-SEC-091, REQ-SEC-092 — ISO/IEC 29147 CVD*
