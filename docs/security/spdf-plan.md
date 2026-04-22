# XPE Secure Product Development Framework (SPDF) Plan

**Document ID**: XPE-SEC-SPDF-001  
**Version**: 0.1.0 (Draft)  
**Date**: 2026-04-22  
**SPEC Reference**: SPEC-XPE-SEC REQ-SEC-010~014  
**Regulatory Basis**: FDA Section 524B, IEC 81001-5-1  
**IEC 62304 Class**: B

---

## 1. 목적 및 범위

본 문서는 XPE 이미지 처리 엔진의 **Secure Product Development Framework (SPDF)** 계획을 정의한다.

대상 컴포넌트: `xpe_common`, `xpe_preprocess`, `xpe_enhance_basic`, `xpe_enhance_advanced`, `xpe_display`, `xpe_dicom`, `gsvg`, `ImageProcTest`

FDA Section 524B는 사이버 디바이스(Cyber Device) 요건으로 SPDF 수립을 법적으로 요구한다.
IEC 81001-5-1은 SPDF를 64개 보안 요건으로 세분화하고 있다.

---

## 2. SPDF 구성요소

### 2.1 보안 조직 및 책임

| 역할 | 책임 |
|------|------|
| Security Lead | SPDF 전체 감독, 위협모델 승인, CVE 대응 조율 |
| Developer (모든 개발자) | Secure coding 준수, SAST 결과 처리, SOUP 평가 |
| QA | Security test 검증, SBOM 검토, VEX 확인 |
| Release Manager | SLSA attestation 생성, SBOM 배포, 릴리즈 서명 |

### 2.2 보안 교육

| 교육 항목 | 주기 | 대상 |
|-----------|------|------|
| Secure C++ Coding (CERT C++ 핵심 20개 규칙) | 신규 합류 시 + 연 1회 | 개발자 전원 |
| OWASP .NET for C# GUI | 신규 합류 시 + 연 1회 | GUI 개발자 |
| DICOM Security Awareness | 연 1회 | 개발자 + QA |
| IEC 81001-5-1 Awareness | 연 1회 | 전체 |

---

## 3. 보안 요구사항 도출

### 3.1 위협 모델링 (STRIDE + EMB3D)

각 신뢰 경계(Trust Boundary)에 대해 STRIDE 분석 수행:

| 경계 | 위협 유형 | 상태 |
|------|-----------|------|
| DICOM Network SCU/SCP | Spoofing, Tampering, DoS | 🔴 미완성 (Phase 2) |
| C ABI P/Invoke 경계 | Tampering, Info Disclosure | 🔴 미완성 (Phase 2) |
| 파일시스템 I/O (config, image, log) | Tampering, Info Disclosure | 🔴 미완성 (Phase 2) |
| C# GUI 프로세스 경계 | Elevation of Privilege, Repudiation | 🔴 미완성 (Phase 2) |
| vcpkg SOUP 의존성 | Supply Chain Tampering | 🔴 미완성 (Phase 2) |

위협모델 상세 문서: `docs/security/threat-model-*.md` (Phase 2 작성 예정)

### 3.2 보안 요구사항 → SPEC 추적

| 요구사항 | 출처 | SPEC-XPE-SEC REQ |
|---------|------|-----------------|
| C ABI 경계 입력 버퍼 크기 검증 | §524B SPDF | REQ-SEC-060 |
| JSON config 스키마 검증 | §524B SPDF | REQ-SEC-061 |
| DCMTK strict mode + fuzz 테스트 | §524B SPDF | REQ-SEC-062 |
| SAST (CERT C++, CWE Top 25) | IEC 81001-5-1 §7.3 | REQ-SEC-063 |
| 의존성 취약점 스캐닝 | §524B SPDF | REQ-SEC-065 |
| Secrets 스캐닝 | NIST SSDF | REQ-SEC-066 |

---

## 4. SBOM 관리

### 4.1 SBOM 생성

| 포맷 | 도구 | 트리거 |
|------|------|--------|
| SPDX 3.0 | `syft` | CI 빌드마다 |
| CycloneDX 1.6 | `cyclonedx-bom` (vcpkg 통합) | CI 빌드마다 |

스크립트: `scripts/gen_sbom.sh`  
CI 워크플로: `.github/workflows/sbom.yml` (Phase 2 구현)

### 4.2 SBOM 컴포넌트 목록 (v1.0 기준)

| 컴포넌트 | 버전 | 라이선스 | CPE/PURL |
|---------|------|---------|---------|
| DCMTK | 3.6.8 | BSD-like | `pkg:github/dcmtk/dcmtk@3.6.8` |
| OpenJPEG | 2.5.x | BSD-2-Clause | `pkg:github/uclouvain/openjpeg@2.5.0` |
| spdlog | 1.13.x | MIT | `pkg:github/gabime/spdlog@1.13.0` |
| fmt | 10.x | MIT | `pkg:github/fmtlib/fmt@10.1.0` |
| nlohmann/json | 3.11.x | MIT | `pkg:github/nlohmann/json@3.11.2` |
| picosha2 | 1.0.0 | MIT | `pkg:github/okdshin/picosha2@1.0.0` |
| FFTW3 | 3.3.10 | GPL-2.0 | `pkg:github/fftw/fftw3@3.3.10` |
| GoogleTest | 1.14.0 | BSD-3-Clause | `pkg:github/google/googletest@1.14.0` |

> **주의**: FFTW3는 GPL-2.0 라이선스. `gsvg.dll`에서 동적 링크만 허용 (SPEC-XPE-SEC §7 위험 참조).

### 4.3 VEX 워크플로

CVE 발생 시 자동화 파이프라인 (Phase 2 구현 목표):

```
NVD/GitHub Advisory → GitHub Actions (vex.yml) → 영향도 분석 →
VEX 문서 생성 (CycloneDX VEX) → GitHub Security Advisory 발행
```

---

## 5. 취약점 관리 프로세스

### 5.1 정기 스캐닝

| 스캔 종류 | 도구 | 주기 |
|-----------|------|------|
| 의존성 취약점 | OSV-Scanner, Grype | 매 CI 빌드 |
| Secrets 탐지 | gitleaks, trufflehog | 매 commit |
| SAST (정적 분석) | CodeQL | PR 마다 |
| Fuzz 테스트 | libFuzzer (DICOM, JSON) | 주간 CI |

### 5.2 패치 SLA

| 심각도 (CVSS) | 분석 완료 | 패치 배포 |
|--------------|-----------|---------|
| Critical (≥9.0) | 48시간 내 | 60일 내 |
| High (7.0~8.9) | 7일 내 | 90일 내 |
| Medium (4.0~6.9) | 30일 내 | 다음 minor 릴리즈 |
| Low (<4.0) | 다음 quarterly 검토 | 다음 major 릴리즈 |

---

## 6. 보안 테스트 계획

### 6.1 Phase 1 (현재)

- [x] 기본 SAST: MSVC `/analyze` + `/WX` 경고 없음
- [ ] CodeQL 워크플로 추가 (`.github/workflows/scan.yml`)
- [ ] C ABI 경계 입력 검증 GTest 추가 (REQ-SEC-060, REQ-SEC-061)

### 6.2 Phase 2

- [ ] DCMTK fuzz 테스트 harness 구축
- [ ] JSON config fuzzing (libFuzzer)
- [ ] OSV-Scanner + Grype CI 통합
- [ ] gitleaks 시크릿 스캐닝 CI 통합

### 6.3 Phase 3

- [ ] 위협 모델 5개 경계 완성
- [ ] SLSA L3 attestation 구현
- [ ] IEC 81001-5-1 64개 항목 준수 매트릭스 완성

---

## 7. 빌드 보안 (SLSA)

| 단계 | 목표 | 상태 |
|------|------|------|
| SLSA L1: Provenance 존재 | Build log retained | 🟡 CI 로그 보존 (기본) |
| SLSA L2: 서명된 Provenance | GitHub Actions SLSA generator | 🔴 미구현 (Phase 2) |
| SLSA L3: Isolated build | GitHub-hosted runner + in-toto | 🔴 미구현 (Phase 3) |

스크립트: `scripts/verify_reproducible.sh` (Phase 2)  
CI 워크플로: `.github/workflows/slsa.yml` (Phase 2)

---

## 8. 변경 이력

| 버전 | 날짜 | 내용 |
|------|------|------|
| 0.1.0 | 2026-04-22 | 초기 작성 — Must 계층(§524B SPDF, SBOM 목록, 취약점 SLA, Phase 1 테스트) |

---

*SPEC-XPE-SEC REQ-SEC-010~014, REQ-SEC-020~026, REQ-SEC-030~034 구현 계획*
