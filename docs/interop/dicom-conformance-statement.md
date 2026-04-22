# XPE DICOM Conformance Statement

**Document ID**: XPE-DICOM-CS-001  
**Version**: 1.0.0 (Released)  
**Date**: 2026-04-22  
**SPEC Reference**: SPEC-XPE-IOP REQ-IOP-005  
**IEC 62304 Class**: B  
**Format**: Per DICOM PS 3.2

---

## 1. Conformance Statement Overview

This DICOM Conformance Statement describes the DICOM capabilities of the **XPE Image Processing Engine** (`xpe_dicom.dll` v1.0.x).

XPE is an X-ray image processing software library for flat-panel detector (FPD) systems.
It provides DICOM read/write capability for Digital Radiography (DR) images.

### 1.1 Implementation Model

| Component | Role | Version |
|-----------|------|---------|
| `xpe_dicom.dll` | DICOM Storage SCU/SCP + Query/Retrieve SCU | 1.0.x |
| DCMTK | Underlying DICOM toolkit | 3.6.8 |
| Network Transport | TCP/IP + TLS 1.2 | OS-provided |

### 1.2 AE Specifications Summary

| AE Title (default) | Role | Supported Services |
|--------------------|------|--------------------|
| `XPE_STORE` | Storage SCU | C-STORE (push to PACS) |
| `XPE_SCP` | Storage SCP | C-STORE (receive from modality) |
| `XPE_QR` | Query/Retrieve SCU | C-FIND, C-MOVE, C-GET |
| `XPE_ECHO` | Verification | C-ECHO SCU + SCP |

AE Titles are configurable via `xpe_config.json`.

---

## 2. Implementation Model

### 2.1 Application Data Flow

```
Modality → [C-STORE SCU] → XPE_SCP → xpe_dicom.dll → Internal Processing
Internal Processing → xpe_dicom.dll → [C-STORE SCU] → PACS / Archive
```

### 2.2 Sequencing Constraints

- C-ECHO Verification shall succeed before any storage operation.
- C-STORE SCU operations use the Transfer Syntax negotiated during Association.

---

## 3. AE Specifications

### 3.1 XPE_STORE — Storage SCU

**Purpose**: Push processed images to a PACS archive.

#### 3.1.1 SOP Classes (Storage SCU)

| SOP Class Name | SOP Class UID |
|----------------|---------------|
| Digital X-Ray Image Storage – For Presentation | 1.2.840.10008.5.1.4.1.1.1.1 |
| Digital X-Ray Image Storage – For Processing | 1.2.840.10008.5.1.4.1.1.1.1.1 |
| Grayscale Softcopy Presentation State Storage | 1.2.840.10008.5.1.4.1.1.11.1 |
| Secondary Capture Image Storage | 1.2.840.10008.5.1.4.1.1.7 |

#### 3.1.2 Transfer Syntaxes Proposed (Storage SCU)

| Transfer Syntax Name | UID | Role |
|----------------------|-----|------|
| Explicit VR Little Endian | 1.2.840.10008.1.2.1 | Default (proposed first) |
| JPEG Lossless, Non-Hierarchical, First-Order Prediction (14) | 1.2.840.10008.1.2.4.70 | Optional |
| JPEG 2000 Image Compression (Lossless Only) | 1.2.840.10008.1.2.4.90 | Optional |
| Implicit VR Little Endian | 1.2.840.10008.1.2 | Fallback only |

#### 3.1.3 Association Establishment

- Maximum PDU Size (SCU): 32,768 bytes
- Called AE Title: configurable
- Calling AE Title: `XPE_STORE` (default, configurable)
- Association Timeout: 30 seconds (configurable)
- Idle Association Release: after 60 seconds idle

---

### 3.2 XPE_SCP — Storage SCP

**Purpose**: Receive DICOM instances from upstream modalities.

#### 3.2.1 SOP Classes (Storage SCP)

| SOP Class Name | SOP Class UID |
|----------------|---------------|
| Digital X-Ray Image Storage – For Presentation | 1.2.840.10008.5.1.4.1.1.1.1 |
| Digital X-Ray Image Storage – For Processing | 1.2.840.10008.5.1.4.1.1.1.1.1 |

#### 3.2.2 Transfer Syntaxes Accepted (Storage SCP)

| Transfer Syntax Name | UID | Supported |
|----------------------|-----|-----------|
| Explicit VR Little Endian | 1.2.840.10008.1.2.1 | ✅ Required |
| Implicit VR Little Endian | 1.2.840.10008.1.2 | ✅ Required |
| JPEG 2000 Image Compression (Lossless Only) | 1.2.840.10008.1.2.4.90 | ✅ |

#### 3.2.3 Attribute Handling

| Attribute | Tag | Requirement |
|-----------|-----|-------------|
| Study Instance UID | (0020,000D) | Required |
| Series Instance UID | (0020,000E) | Required |
| SOP Instance UID | (0008,0018) | Required |
| Pixel Data | (7FE0,0010) | Required |
| Modality | (0008,0060) | Required |
| Patient ID | (0010,0020) | Required |
| Acquisition Date | (0008,0022) | Conditionally required |
| KVP | (0018,0060) | Optional |
| Exposure | (0018,1152) | Optional |

Unrecognized private attributes are preserved without modification.

---

### 3.3 XPE_QR — Query/Retrieve SCU

**Purpose**: Retrieve prior studies for comparison workflows.

#### 3.3.1 Service Classes

| Service | Role | SOP Class UID |
|---------|------|---------------|
| Study Root Query/Retrieve – FIND | SCU | 1.2.840.10008.5.1.4.1.2.2.1 |
| Study Root Query/Retrieve – MOVE | SCU | 1.2.840.10008.5.1.4.1.2.2.2 |

#### 3.3.2 Query Keys

| Level | Mandatory Keys | Optional Keys |
|-------|---------------|---------------|
| STUDY | StudyInstanceUID, PatientID, StudyDate | AccessionNumber, Modality |
| SERIES | SeriesInstanceUID, Modality | SeriesNumber, SeriesDate |
| IMAGE | SOPInstanceUID | InstanceNumber |

---

## 4. Communication Profiles

### 4.1 TCP/IP Stack

| Feature | Support |
|---------|---------|
| IPv4 | ✅ |
| IPv6 | ✅ |
| TLS 1.2 | ✅ |
| TLS 1.3 | ✅ (via DCMTK + OpenSSL) |
| Mutual TLS (client cert) | Optional |

Cipher suites follow NIST SP 800-52 Rev. 2 recommendations (AES-256-GCM, ECDHE key exchange).

### 4.2 Port Configuration

| Service | Default Port | Configurable |
|---------|-------------|--------------|
| DICOM (plain) | 104 | Yes |
| DICOM over TLS | 2762 | Yes |

---

## 5. Extended Negotiation

| Extension | Support |
|-----------|---------|
| Asynchronous Operations Window | SCU: 1 outstanding (synchronous) |
| SCP/SCU Role Selection | As specified per AE above |
| Extended Negotiation (Storage Commitment) | Not supported in v1.0 |

---

## 6. Configuration

XPE DICOM behavior is configured via `xpe_config.json` `dicom` section:

```json
{
  "dicom": {
    "ae_title": "XPE_STORE",
    "local_port": 104,
    "max_pdu_size": 32768,
    "tls_enabled": false,
    "tls_cert_file": "",
    "tls_key_file": "",
    "tls_ca_file": "",
    "association_timeout_sec": 30,
    "called_ae_title": "PACS_AE"
  }
}
```

---

## 7. Verification Procedures (v1.0 신규 추가)

### 7.1 Conformance Testing Methods

**Automated Test Suite:**
- DVTk (DICOM Validation Toolkit) 상호운용성 테스트
- dcm4che 테스트 프레임워크 활용
- CI/CD 파이프라인 통합 테스트

**수동 검증 절차:**
- 실제 PACS 연동 테스트
- 네트워크 부하 테스트
- 보안 프로토콜 검증

### 7.2 Test Categories and Methods

| 테스트 종류 | 방법 | 주기 | 책임자 |
|-------------|------|------|--------|
| 기능 테스트 | 단위 테스트, 통합 테스트 | 개발 시마다 | 개발팀 |
| 상호운용성 테스트 | 다양한 PACS 벤더 연동 테스트 | 배포 전 | QA팀 |
| 성능 테스트 | 부하 테스트, 스트레스 테스트 | 주기적 | 개발팀 |
| 보안 테스트 | 침투 테스트, 보안 점검 | 분기별 | 보안팀 |
| 호환성 테스트 | 다양한 OS 환경 테스트 | 주기적 | QA팀 |

### 7.3 Test Environment Requirements

**하드웨어 요구사항:**
- 테스트 서버: 최소 8코어 CPU, 16GB RAM
- 네트워크: 1Gbps 이상 대역폭
- 저장소: 1TB SSD (테스트 데이터 저장)

**소프트웨어 요구사항:**
- 테스트용 DICOM 서버 (DCMTK, dcm4chee)
- 시뮬레이션된 모달리티 디바이스
- 모니터링 및 로깅 도구
- 자동화 테스트 프레임워크

### 7.4 Acceptance Criteria

**상호운용성 기준:**
- 성공 연결률: 100% (주요 PACS 벤더 5곱)
- 속도 기준: 연결 시간 < 5초, 전송 시간 < 지정된 타임아웃
- 데이터 무결성: 수신 데이터 손실률 0%

**기능 기준:**
- 모든 지원 SOP 클래스 정상 동작
- 모든 지원 전송 구문 정상 처리
- 에러 처리 예외 없음

---

## 8. Maintenance Section (v1.0 신규 추가)

### 8.1 Document Maintenance Policy

**유지보수 주기:**
- 주요 업데이트 시: 즉시 갱신
- 분기별 검토: 업데이트 필요성 평가
- 연간 주기: 전면 검토 및 개선

**유지보수 절차:**
1. 변경 요청 접수
2. 영향 평가 수행
3. 업데이트 계획 수립
4. 검증 테스트 수행
5. 문서 업데이트 발행
6. 변경 이력 기록

### 8.2 Change Control Process

**변경 분류:**
- 소변경: 오타 수정, 서식 변경
- 중변경: 기능 추가/수정, 구조 변경
- 대변경: 아키텍처 변경, 프로토콜 변경

**변경 심사 절차:**
- 요청 검토: 변경 필요성 검증
- 기술 검토: 구현 가능성 검증
- 규제 검토: 규제 준수 검증
- 문서 검토: 문서 변경 검토
- 승인: 관련 담당자 승인
- 배포: 예정된 버전에 배포

### 8.3 Version Control System

**버전 관리 전략:**
- 주요 버전: 주요 기능 변경 시 (v1.0, v2.0)
- 부 버전: 기능 추가 시 (v1.1, v1.2)
- 패치 버전: 버그 수정 시 (v1.0.1, v1.0.2)

**분기 관리:**
- `main` 브랜치: 안정된 버전 유지
- `develop` 브랜치: 개발 버전 관리
- `feature/*` 브랜치: 기능 개발용
- `release/*` 브랜치: 릴리즈 준비용

**태깅 정책:**
- `v1.0.0`: 릴리즈된 버전
- `v1.0.0-rc1`: 릴리즈 후보 버전
- `v1.0.0-hotfix1`: 긴급 수정 버전

---

## 9. Version Control Section (v1.0 신규 추가)

### 9.1 Document Version History

| 버전 | 날짜 | 변경 내용 | 변경자 | 상태 |
|------|------|----------|--------|------|
| 1.0.0 | 2026-04-22 | v1.0 정식 릴리즈 — 검증 절차 섹션 추가, 유지보수 섹션 추가, 버전 관리 섹션 추가, 테스트 방법론 참조 추가 | Technical Team | Released |
| 0.1.0 | 2026-04-22 | 초기 초안 — DICOM 3.0 핵심 기능 (SCU/SCP 역할, SOP 클래스, 전송 구문, TLS) | Technical Team | Draft |

### 9.2 Branch Management Strategy

**브랜치 유형:**
- `main`: 안정된 배포 버전 (Protected)
- `develop`: 개용 개발 버전
- `feature/*`: 새 기능 개발
- `release/*`: 배포 준비
- `hotfix/*`: 긴급 수정

**머지 정책:**
- `feature/` → `develop`: PR 검토 후 병합
- `develop` → `release`: 배포 전 테스트
- `release/` → `main`: 최종 배포
- `hotfix/` → `main` + `develop`: 긴급 배포

### 9.3 Release Process

**릴리즈 절차:**
1. 릴리즈 브랜치 생성 (`release/v1.0.0`)
2. 최종 테스트 수행
3. 문서 검증 및 업데이트
4. 릴리즈 노트 작성
5. 주요 브랜치에 병합
6. 태그 생성 (`v1.0.0`)
7. 배포 패키지 생성

**배포 전 검사리스트:**
- [ ] 모든 테스트 통과
- [ ] 문서 업데이트 완료
- [ ] 버그 수정 사항 검토
- [ ] 성능 검증 완료
- [ ] 보안 검증 완료
- [ ] 규제 준수 검토

---

## 10. Test Methodology References (v1.0 신규 추가)

### 10.1 Standard Test Methodologies

**DICOM 표준 테스트 방법:**
- DICOM Conformance Testing (PS 3.10)
- DICOM Network Communication Testing (PS 3.8)
- DICOM Media Storage Testing (PS 3.11)

**국제 표준 준수 테스트:**
- IEC 62304:2006+AMD1:2012 복합성 테스트
- ISO 14971:2019 위험 분석 테스트
- EN ISO 13485:2016 품질 관리 시스템 테스트

### 10.2 Internal Test Methodologies

**단위 테스트:**
- 기능별 독립 테스트
- Mock 객체를 활용한 테스트
- 코드 커버리지 목표: ≥ 85%

**통합 테스트:**
- 컴포넌트 간 상호작용 테스트
- 인터페이스 호환성 테스트
- 시나리오 기반 테스트

**시스템 테스트:**
- 전체 시스템 기능 테스트
- 성능 테스트 (부하, 스트레스)
- 보안 테스트 (침투 테스트)

### 10.3 External Test Resources

**외부 테스트 도구:**
- DVTk (DICOM Validation Toolkit): 상호운용성 테스트
- dcm4chee: DICOM 서버 테스트
- Orthanc: DICOM 서버 대체 테스트
- Wireshark: 네트워크 프로토콜 분석

**인증 테스트:**
- Notified Body 인증 테스트
- FDA 510(k) 지원 테스트
- CE Marking 준비 테스트

### 10.4 Test Environment Setup

**개발 환경:**
- CI/CD 파이프라인 통합 테스트
- 자동화 테스트 스위트
- 컨테이너 기반 테스트 환경

**운영 환경:**
- 스테이징 환경 테스트
- 프로덕션 모니터링 테스트
- 실제 환경 모니터링

### 10.5 Test Documentation

**테스트 문서:**
- `docs/interop/dicom-test-plan.md`: 상세 테스트 계획
- `docs/interop/dicom-test-cases.md`: 개별 테스트 케이스
- `docs/interop/dicom-test-results.md`: 테스트 결과 보고서
- `tests/integration/dicom/`: 자동화 테스트 소스 코드

**테스트 결과 관리:**
- 테스트 결과 데이터베이스
- 실패 사건 추적 시스템
- 성능 지표 모니터링
- 자동 보고 생성 시스템

---

## 11. Limitations and Conformance Exceptions

| Limitation | Detail |
|------------|--------|
| Media Storage | DICOMDIR creation: not supported in v1.0 (PDI profile: Phase 2) |
| WADO-RS / STOW-RS | Not supported in v1.0 (DICOMweb: Phase 2, SPEC-XPE-IOP §4.2) |
| FHIR ImagingStudy | Not supported in v1.0 (Phase 2-3) |
| Compressed pixel encoding | JPEG 2000 lossless only; lossy compression not supported |
| Structured Reporting (AI SR) | Not supported in v1.0 (Phase 3, SPEC-XPE-IOP §4.5) |

---

## 12. Change History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-04-22 | v1.0 업그레이드 — 검증 절차 섹션 추가, 유지보수 섹션 추가, 버전 관리 섹션 추가, 테스트 방법론 참조 추가 |
| 0.1.0 | 2026-04-22 | Initial draft per SPEC-XPE-IOP REQ-IOP-005. Covers DICOM 3.0 core (Must tier): SCU/SCP roles, SOP classes, Transfer Syntaxes, TLS. DICOMweb/FHIR/IHE deferred to later versions. |

---

*This document conforms to DICOM PS 3.2 (Conformance Statement format).*  
*Document owner: XPE development team. Review cycle: per major release.*  
*Test methodology references: DICOM PS 3.10, IEC 62304:2006+AMD1:2012, ISO 14971:2019*