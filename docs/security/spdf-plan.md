# XPE Secure Product Development Framework (SPDF) Plan

**Document ID**: XPE-SEC-SPDF-001
**Version**: 1.0.0
**Date**: 2026-04-22
**SPEC Reference**: SPEC-XPE-SEC REQ-SEC-010~014, REQ-SEC-040~046
**Regulatory Basis**: FDA Section 524B, IEC 81001-5-1
**IEC 62304 Class**: B

---

## HISTORY

| Version | Date | Changes |
|---------|------|---------|
| 0.1.0 | 2026-04-22 | Initial — Must 계층(§524B SPDF, SBOM 목록, 취약점 SLA, Phase 1 테스트) |
| 1.0.0 | 2026-04-22 | Major upgrade — 위협 모델 5개 경계 완성, SLSA L2/L3 로드맵 구체화, IEC 81001-5-1 준수 매트릭스 추가 |

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

### 3.1 위협 모델링 (STRIDE + LINDDUN)

각 신뢰 경계(Trust Boundary)에 대해 STRIDE 분석 완료:

#### TB-1: DICOM Network SCU/SCP 경계

| ID | 위협 | 유형 | 영향 | 완화대책 | 상태 |
|----|------|------|------|---------|------|
| T-1.1 | 비인가 AE가 DICOM C-STORE 전송 | Spoofing | 위조 영상 주입 | AE Title 검증 + IP 화이트리스트 | Phase 2 |
| T-1.2 | 전송 중 DICOM 데이터 변조 | Tampering | 진단 영상 변조 | DICOM digital signature (PS3.15) | Phase 3 |
| T-1.3 | 대량 C-FIND 요청으로 자원 고갈 | DoS | 서비스 거부 | 연결 제한 + 타임아웃(REQ-DICOM-031) | 구현됨 |
| T-1.4 | 네트워크 패킷 캡처로 환자 정보 노출 | Info Disclosure | PHI 유출 | TLS 전송 암호화 (DCMTK) | Phase 2 |

#### TB-2: C ABI / P/Invoke 경계

| ID | 위협 | 유형 | 영향 | 완화대책 | 상태 |
|----|------|------|------|---------|------|
| T-2.1 | P/Invoke 마샬링 버퍼 오버플로우 | Tampering | 힙 오염, 코드 실행 | blittable 타입만 사용 + Pack=8 | 구현됨 |
| T-2.2 | C#에서 NULL 포인터 전달 | Tampering | 네이티브 크래시 | 모든 API NULL 체크 (REQ-SEC-060) | 구현됨 |
| T-2.3 | 이미지 버퍼 크기 불일치 | Tampering | 버퍼 오버리드 | width*height*format 검증 | 구현됨 |
| T-2.4 | DLL 교체 공격 | Spoofing | 악성 DLL 로드 | 디지털 서명 검증 | Phase 2 |

#### TB-3: 파일시스템 I/O 경계

| ID | 위협 | 유형 | 영향 | 완화대책 | 상태 |
|----|------|------|------|---------|------|
| T-3.1 | 설정 파일(JSON) 변조 | Tampering | 잘못된 처리 파라미터 | 스키마 검증 (REQ-SEC-061) | 구현됨 |
| T-3.2 | 캘리브레이션 파일 변조 | Tampering | 영상 품질 저하 | SHA-256 무결성 검증 (XCal v1) | 구현됨 |
| T-3.3 | 로그 파일에 민감 정보 기록 | Info Disclosure | PHI 유출 | 로그에 환자 정보 제외 | 구현됨 |
| T-3.4 | 출력 DICOM 파일 변조 | Tampering | 진단 영상 변조 | 쓰기 후 읽기 검증 | Phase 2 |

#### TB-4: C# GUI 프로세스 경계

| ID | 위협 | 유형 | 영향 | 완화대책 | 상태 |
|----|------|------|------|---------|------|
| T-4.1 | 권한 상승 (관리자 권한 없이 DLL 로드) | Elevation | 무단 기능 실행 | 최소 권한 실행 | 구현됨 |
| T-4.2 | GUI 크래시 시 처리 중 데이터 손실 | Repudiation | 감사 추적 단절 | 크래시 전 상태 로깅 | Phase 2 |
| T-4.3 | UI 스푸핑 (화면 캡처) | Info Disclosure | 환자 영상 유출 | 화면 보호 기능 안내 | Phase 3 |

#### TB-5: vcpkg SOUP 의존성 공급망

| ID | 위협 | 유형 | 영향 | 완화대책 | 상태 |
|----|------|------|------|---------|------|
| T-5.1 | 악의적인 vcpkg 포트 업데이트 | Supply Chain | 백도어 삽입 | 고정 버전 + 해시 검증 | 구현됨 |
| T-5.2 | DCMTK/OpenJPEG 알려진 취약점 | Supply Chain | 익스플로잇 가능 | SBOM + CVE 모니터링 | 구현됨 |
| T-5.3 | FFTW3 GPL-2.0 라이선스 위반 | Legal | 법적 리스크 | 동적 링크만 허용, gsvg.dll 격리 | 구현됨 |
| T-5.4 | 빌드 환경 오염 | Supply Chain | 재현 불가 빌드 | SLSA L2+ provenance | Phase 2 |

위협모델 요약:
- **총 위협**: 19개
- **구현됨 (완화됨)**: 11개
- **Phase 2 예정**: 6개
- **Phase 3 예정**: 2개

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

### 7.1 SLSA 로드맵

| Level | 요구사항 | 구현 계획 | 상태 |
|-------|---------|----------|------|
| **L1** | Provenance 문서 존재 | CI 빌드 로그 + 아티팩트 해시 보존 | 구현됨 |
| **L2** | 서명된 Provenance | GitHub Actions SLSA generator (`slsa-framework/slsa-github-generator`) | Phase 2 |
| **L3** | Isolated, non-falsifiable build | GitHub-hosted runner + in-toto attestation | Phase 3 |

### 7.2 SLSA L2 구현 계획 (Phase 2)

```
.github/workflows/build.yml
  → MSBuild (Release/x64)
  → Google Test 실행
  → hash-of-artifacts 계산
  → slsa-framework/slsa-github-generator@v2
    → provenance SLSA v1.0 JSON 생성
    → Sigstore cosign 서명
    → GitHub Release에 attestation 업로드
```

아티팩트별 provenance:
| 아티팩트 | 서명 방식 | 검증 방식 |
|----------|----------|----------|
| xpe_*.dll | Sigstore cosign | `cosign verify-blob` |
| ImageProcTest.exe | Sigstore cosign | `cosign verify-blob` |
| SBOM (SPDX) | Inline JSON signature | `slsa-verifier` |
| VEX (CycloneDX) | Inline JSON signature | `slsa-verifier` |

### 7.3 SLSA L3 구현 계획 (Phase 3)

- GitHub-hosted runner (self-hosted runner 금지)
- in-toto layout 정의 (빌드 단계별 attestation 체인)
- Hermetic build (외부 네트워크 접근 차단)
- 재현 가능 빌드 (reproducible build) 검증 스크립트

스크립트: `scripts/verify_reproducible.sh`
CI 워크플로: `.github/workflows/slsa.yml`

---

## 8. IEC 81001-5-1 준수 매트릭스

IEC 81001-5-1 Clause 5~9 핵심 요건에 대한 준수 상태:

| Clause | 요건 | 준수 수준 | 증빙 |
|--------|------|:---------:|------|
| §5.1 | Security risk management process | 구현됨 | 본 문서 §3 (STRIDE 위협 모델) |
| §5.2 | Security requirements specification | 구현됨 | SPEC-XPE-SEC REQ-SEC-001~092 |
| §5.3 | Architecture security design | 구현됨 | 모듈 독립성 (xpe-module-principles.md), ABI 경계 검증 |
| §5.4 | Secure coding standards | 구현됨 | MSVC `/W4` + `/analyze`, CodeQL (Phase 2) |
| §5.5 | Security testing | Partial | L1 SAST 구현, L2 Fuzzing Phase 2 |
| §5.6 | Vulnerability management | 구현됨 | SECURITY.md v1.0 (CVD + SLA) |
| §5.7 | Security update process | Partial | SPDF Plan 본 문서, 자동화 Phase 2 |
| §6.1 | Threat modeling | 구현됨 | 본 문서 §3.1 (5개 경계, 19개 위협) |
| §6.2 | Attack surface analysis | Partial | DLL 경계 분석 완료, 네트워크 경계 Phase 2 |
| §7.1 | SOUP vulnerability monitoring | 구현됨 | SBOM (§4), OSV-Scanner |
| §7.2 | Patch management | 구현됨 | 패치 SLA (§5.2) |
| §7.3 | Static code analysis | 구현됨 | MSVC `/analyze`, CodeQL (Phase 2) |
| §8.1 | Incident response | 구현됨 | SECURITY.md §Incident Response |
| §8.2 | Communications | 구현됨 | SECURITY.md §Communication Matrix |
| §9.1 | Security labeling | Partial | SBOM + VEX, 라벨링 Phase 2 |
| §9.2 | End-of-life security | Planned | 미정의 (Phase 3) |

준수율: **11/16 완료 (69%), 4/16 Partial, 1/16 Planned**

---

## 9. 변경 이력

| 버전 | 날짜 | 내용 |
|------|------|------|
| 0.1.0 | 2026-04-22 | 초기 작성 — Must 계층(§524B SPDF, SBOM 목록, 취약점 SLA, Phase 1 테스트) |
| 1.0.0 | 2026-04-22 | Major upgrade — STRIDE 위협 모델 5개 경계 19개 위협 완성, SLSA L2/L3 로드맵 구체화, IEC 81001-5-1 준수 매트릭스 추가 (69% 준수) |

---

*SPEC-XPE-SEC REQ-SEC-010~014, REQ-SEC-020~026, REQ-SEC-030~034, REQ-SEC-040~046*
