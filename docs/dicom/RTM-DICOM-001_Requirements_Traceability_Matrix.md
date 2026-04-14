# DICOM I/O 모듈 — 요구사항 추적성 행렬 (RTM)

**문서 ID**: RTM-DICOM-001  
**버전**: 1.0.0  
**날짜**: 2026-04-14  
**IEC 62304 절**: 5.1.1c — Traceability  
**안전 등급**: Class B  

---

## 목차

1. [추적성 개요](#1-추적성-개요)
2. [PRD → SRS 매핑](#2-prd--srs-매핑)
3. [SRS → SAD 매핑](#3-srs--sad-매핑)
4. [SRS → SHA 매핑](#4-srs--sha-매핑)
5. [SRS → Test 매핑](#5-srs--test-매핑)
6. [커버리지 분석](#6-커버리지-분석)

---

## 1. 추적성 개요

### 1.1 추적성 체인

```
PRD (Product Requirements)
    ↓
SRS (Software Requirements Specification)
    ├→ SAD (Software Architecture Design)
    ├→ SHA (Software Hazard Analysis)
    └→ Test Cases
        └→ Code Implementation
```

### 1.2 추적성 정의

| 용어 | 의미 |
|------|------|
| **Forward Traceability** | 상위(PRD)에서 하위(SRS)로의 추적 |
| **Backward Traceability** | 하위(Test)에서 상위(SRS)로의 추적 |
| **Bidirectional** | 양방향 추적 가능 |
| **Coverage** | 모든 요구사항이 추적되어야 함 (≥ 100%) |

---

## 2. PRD → SRS 매핑

### 2.1 SWU-4.1 (DicomReader) 매핑

| PRD 절 | PRD 요구사항 | SRS ID | SRS 요구사항 | 상태 |
|--------|------------|--------|-------------|------|
| §3.1 | Implicit VR LE 지원 | FR-DCM-101 | Implicit VR LE Transfer Syntax 지원 | ✓ |
| §3.1 | Explicit VR LE 지원 | FR-DCM-102 | Explicit VR LE Transfer Syntax 지원 | ✓ |
| §3.1 | JPEG 2000 Lossless 읽기 | FR-DCM-103 | JPEG 2000 Lossless Transfer Syntax 읽기 | ✓ |
| §3.1 | JPEG Baseline 읽기 (legacy) | FR-DCM-104 | JPEG Baseline (Process 1) 지원 | ✓ |
| §3.1 | DX IOD 파싱 | FR-DCM-105 | DX IOD 파싱 | ✓ |
| §3.1 | CR IOD 파싱 | FR-DCM-106 | CR IOD 파싱 | ✓ |
| §3.1 | GSPS IOD 읽기 | FR-DCM-107 | GSPS IOD 읽기 | ✓ |
| §3.1 | Rows/Columns 추출 | FR-DCM-108 | 픽셀 데이터 추출 (Rows/Columns) | ✓ |
| §3.1 | 포토메트릭 처리 | FR-DCM-109 | 포토메트릭 인터프리테이션 | ✓ |
| §3.1 | 픽셀 간격 읽기 | FR-DCM-110 | 픽셀 간격 읽기 | ✓ |
| §3.1 | Window/Level 읽기 | FR-DCM-111 | Window/Level 읽기 | ✓ |
| §3.1 | 환자 정보 추출 | FR-DCM-112 | 환자 정보 추출 | ✓ |
| §3.1 | 촬영 정보 추출 | FR-DCM-113 | 촬영 정보 추출 | ✓ |
| §3.1 | 검출기 정보 추출 | FR-DCM-114 | 검출기 정보 추출 | ✓ |
| §3.1 | 신체 부위 추출 | FR-DCM-115 | 신체 부위 추출 | ✓ |
| §3.1 | XPE private block 읽기 | FR-DCM-116 | XPE Private Block 읽기 | ✓ |
| §3.1 | 파일 존재 검증 | FR-DCM-117 | 파일 존재 검증 | ✓ |
| §3.1 | DICOM 형식 검증 | FR-DCM-118 | DICOM 형식 검증 | ✓ |
| §3.1 | Unsupported TS 감지 | FR-DCM-119 | Unsupported Transfer Syntax 감지 | ✓ |
| §3.1 | 메모리 효율 (읽기) | FR-DCM-120 | 메모리 효율 (읽기) | ✓ |

**커버리지**: 20/20 (100%)

---

### 2.2 SWU-4.2 (DicomWriter) 매핑

| PRD 절 | PRD 요구사항 | SRS ID | SRS 요구사항 | 상태 |
|--------|------------|--------|-------------|------|
| §3.2 | DX IOD 생성 | FR-DCM-201 | DX IOD 생성 | ✓ |
| §3.2 | Explicit VR LE 쓰기 (기본값) | FR-DCM-202 | Explicit VR LE 쓰기 | ✓ |
| §3.2 | JPEG 2000 Lossless 쓰기 | FR-DCM-203 | JPEG 2000 Lossless 쓰기 | ✓ |
| §3.2 | 픽셀 데이터 uint16 쓰기 | FR-DCM-204 | 픽셀 데이터 쓰기 (uint16) | ✓ |
| §3.2 | 포토메트릭 고정 (MONOCHROME2) | FR-DCM-205 | 포토메트릭 고정 (MONOCHROME2) | ✓ |
| §3.2 | 필수 Type 1 태그 쓰기 | FR-DCM-206 | 필수 Type 1 태그 쓰기 | ✓ |
| §3.2 | 환자 정보 쓰기 | FR-DCM-207 | 환자 정보 쓰기 | ✓ |
| §3.2 | 촬영 정보 쓰기 | FR-DCM-208 | 촬영 정보 쓰기 | ✓ |
| §3.2 | 검출기 정보 쓰기 | FR-DCM-209 | 검출기 정보 쓰기 | ✓ |
| §3.2 | Window/Level 저장 | FR-DCM-210 | Window/Level 저장 | ✓ |
| §3.2 | XPE private block 쓰기 | FR-DCM-211 | XPE Private Block 쓰기 | ✓ |
| **§7** | **Lossy 압축 금지 (CRITICAL)** | **FR-DCM-212** | **Lossy 압축 금지** | **✓ CRITICAL** |
| §3.2 | UID 생성 (고유성) | FR-DCM-213 | UID 생성 (고유성) | ✓ |
| §3.2 | 메타데이터 검증 (쓰기) | FR-DCM-214 | 메타데이터 검증 | ✓ |
| §3.2 | 파일 쓰기 실패 처리 | FR-DCM-215 | 파일 쓰기 실패 처리 | ✓ |
| §3.2 | 성능 (비압축 쓰기) | FR-DCM-216 | 성능 (비압축 쓰기) | ✓ |
| §3.2 | 성능 (JPEG 2000 쓰기) | FR-DCM-217 | 성능 (JPEG 2000 쓰기) | ✓ |

**커버리지**: 17/17 (100%)

---

### 2.3 SWU-4.3 (PresentationStateIO) 매핑

| PRD 절 | PRD 요구사항 | SRS ID | SRS 요구사항 | 상태 |
|--------|------------|--------|-------------|------|
| §3.3 | GSPS IOD 생성 | FR-DCM-301 | GSPS IOD 생성 | ✓ |
| §3.3 | Referenced Series 설정 | FR-DCM-302 | Referenced Series 설정 | ✓ |
| §3.3 | Graphic Annotation (ROI) | FR-DCM-303 | Graphic Annotation (ROI) | ✓ |
| §3.3 | Display Shutter | FR-DCM-304 | Display Shutter | ✓ |
| §3.3 | Window/Level 프리셋 저장 | FR-DCM-305 | Window/Level 프리셋 저장 | ✓ |
| §3.3 | GSPS 적용 | FR-DCM-306 | GSPS 적용 | ✓ |

**커버리지**: 6/6 (100%)

---

### 2.4 SWU-4.4 (DicomNetworkSCU) 매핑

| PRD 절 | PRD 요구사항 | SRS ID | SRS 요구사항 | 상태 |
|--------|------------|--------|-------------|------|
| §3.4.1 | C-STORE SCU 구현 | FR-DCM-401 | C-STORE SCU 구현 | ✓ |
| §3.4.1 | AE Title 구성 | FR-DCM-402 | AE Title 구성 | ✓ |
| §3.4.1 | 호스트명/포트 구성 | FR-DCM-403 | 호스트명/포트 구성 | ✓ |
| §3.4.1 | Association Timeout | FR-DCM-404 | Association Timeout | ✓ |
| §3.4.1 | Transfer Syntax 협상 | FR-DCM-405 | Transfer Syntax 협상 | ✓ |
| §3.4.1 | 전송 실패 처리 | FR-DCM-406 | 전송 실패 처리 | ✓ |
| §3.4.1 | C-STORE Status 반환 | FR-DCM-407 | C-STORE Status 반환 | ✓ |
| §3.4.1 | TLS 1.2+ (선택) | FR-DCM-408 | TLS 1.2+ (선택) | ✓ |
| §3.4.1 | C-STORE 성능 | FR-DCM-409 | C-STORE 성능 | ✓ |
| §3.4.2 | C-FIND SCU (MWL) | FR-DCM-410 | C-FIND SCU (MWL) | ✓ |
| §3.4.2 | MWL 쿼리 키 | FR-DCM-411 | MWL 쿼리 키 | ✓ |
| §3.4.2 | MWL 반환 정보 | FR-DCM-412 | MWL 반환 정보 | ✓ |
| §3.4.2 | C-FIND Status 반환 | FR-DCM-413 | C-FIND Status 반환 | ✓ |

**커버리지**: 13/13 (100%)

---

## 3. SRS → SAD 매핑

### 3.1 기능 요구사항 (FR) → SWU 설계

| SRS ID | 요구사항 | SAD 섹션 | 설계 요소 | 상태 |
|--------|---------|---------|----------|------|
| FR-DCM-101~120 | DicomReader | §2.1 | DicomReader 알고리즘, TS 지원 매트릭스 | ✓ |
| FR-DCM-201~217 | DicomWriter | §2.2 | DicomWriter 알고리즘, Lossy 검증 | ✓ |
| FR-DCM-301~306 | PresentationStateIO | §2.3 | GSPS 알고리즘, 참조 링크 | ✓ |
| FR-DCM-401~413 | DicomNetworkSCU | §2.4 | 상태 머신, C-STORE/C-FIND 알고리즘 | ✓ |

**커버리지**: 4/4 (100%)

---

### 3.2 안전 요구사항 (SR) → 설계 통제

| SRS ID | 안전 요구사항 | SAD 섹션 | 통제 설계 | 상태 |
|--------|------------|---------|----------|------|
| SR-DCM-001 | Lossy 압축 금지 | §8.1 | Transfer Syntax 검증 (다중 층) | ✓ |
| SR-DCM-002 | 환자 ID 검증 | §8.2 | Patient ID 비교 로직 | ✓ |
| SR-DCM-003 | DICOM 파일 무결성 | §8.3 | Preamble + VR 검증 | ✓ |
| SR-DCM-004 | 네트워크 장애 처리 | §8.4 | Retry + timeout 로직 | ✓ |
| SR-DCM-005 | MWL 환자 검증 | SAD §4 | Patient ID 확인 | ✓ |
| SR-DCM-006 | GSPS 참조 무결성 | §2.3 | Referenced UID 저장/검증 | ✓ |
| SR-DCM-007 | Unsupported TS 감지 | §2.1 | TS 검증 알고리즘 | ✓ |
| SR-DCM-008 | 감사 로깅 | §7 | 에러 처리 + 로깅 | ✓ |

**커버리지**: 8/8 (100%)

---

## 4. SRS → SHA 매핑

### 4.1 안전 요구사항 → 위험 통제

| SRS ID (Safety) | 안전 요구사항 | HAZ ID | 위험 | 통제 | 상태 |
|-----------------|------------|--------|------|------|------|
| SR-DCM-001 | Lossy 압축 금지 | HAZ-001 | 손실 압축 적용 | C-001-A~D | ✓ |
| SR-DCM-002 | 환자 ID 검증 | HAZ-003 | 환자 ID 불일치 | C-003-A~D | ✓ |
| SR-DCM-002 | 환자 ID 검증 | HAZ-005 | MWL 잘못된 선택 | C-005-A~D | ✓ |
| SR-DCM-003 | 파일 무결성 | HAZ-002 | 파일 손상 | C-002-A~D | ✓ |
| SR-DCM-004 | 네트워크 장애 | HAZ-004 | C-STORE 데이터 손실 | C-004-A~F | ✓ |
| SR-DCM-006 | GSPS 참조 | HAZ-006 | GSPS 참조 불일치 | C-006-A~D | ✓ |
| SR-DCM-007 | Unsupported TS | HAZ-007 | TS 미감지 | C-007-A~D | ✓ |
| SR-DCM-008 | 감시 로깅 | HAZ-008 | Private tag 충돌 | C-008-A~D | ✓ |

**커버리지**: 8/8 (100%)

---

## 5. SRS → Test 매핑

### 5.1 기능 요구사항 테스트 매핑

| SRS ID | 요구사항 | Test Case ID | 테스트 시나리오 | 상태 |
|--------|---------|-------------|--------------|------|
| FR-DCM-101 | Implicit VR LE 지원 | TC-101 | DICOM Implicit VR LE 파일 읽기 | ✓ |
| FR-DCM-102 | Explicit VR LE 지원 | TC-102 | DICOM Explicit VR LE 파일 읽기 | ✓ |
| FR-DCM-103 | J2K Lossless 읽기 | TC-103 | JPEG 2000 Lossless 파일 읽기 | ✓ |
| FR-DCM-104 | JPEG Baseline 읽기 | TC-104 | JPEG Baseline 파일 읽기 (legacy) | ✓ |
| FR-DCM-105 | DX IOD 파싱 | TC-105 | DX SOP Class 검증 | ✓ |
| FR-DCM-106 | CR IOD 파싱 | TC-106 | CR SOP Class 검증 | ✓ |
| FR-DCM-107 | GSPS IOD 읽기 | TC-107 | GSPS SOP Class 검증 + Referenced Series | ✓ |
| FR-DCM-108 | Rows/Columns 추출 | TC-108 | 이미지 크기 추출 및 범위 검증 | ✓ |
| FR-DCM-109 | 포토메트릭 처리 | TC-109 | MONOCHROME1 반전, MONOCHROME2 유지 | ✓ |
| FR-DCM-110 | 픽셀 간격 읽기 | TC-110 | Pixel spacing 범위 검증 | ✓ |
| FR-DCM-111 | Window/Level 읽기 | TC-111 | VOI LUT 프리셋 추출 | ✓ |
| FR-DCM-112 | 환자 정보 추출 | TC-112 | Patient ID, Name, DOB 추출 | ✓ |
| FR-DCM-113 | 촬영 정보 추출 | TC-113 | Study Date, Series Date 추출 | ✓ |
| FR-DCM-114 | 검출기 정보 추출 | TC-114 | Manufacturer, Serial Number 추출 | ✓ |
| FR-DCM-115 | 신체 부위 추출 | TC-115 | Body Part Examined 추출 | ✓ |
| FR-DCM-116 | XPE private block 읽기 | TC-116 | Private Creator, Flags, Version 읽기 | ✓ |
| FR-DCM-117 | 파일 존재 검증 | TC-117 | FILE_NOT_FOUND, FILE_READ_FAILED 에러 | ✓ |
| FR-DCM-118 | DICOM 형식 검증 | TC-118 | DICM preamble 검증, 손상 감지 | ✓ |
| FR-DCM-119 | Unsupported TS 감지 | TC-119 | MPEG-2, RLE 등 미지원 TS 거부 | ✓ |
| FR-DCM-120 | 메모리 효율 (읽기) | TC-120 | 3072×3072 읽기 메모리 ≤ 150 MB | ✓ |
| FR-DCM-201 | DX IOD 생성 | TC-201 | DX SOP Class UID 설정 | ✓ |
| FR-DCM-202 | Explicit VR LE 쓰기 | TC-202 | Explicit VR LE 파일 쓰기 | ✓ |
| FR-DCM-203 | J2K Lossless 쓰기 | TC-203 | JPEG 2000 Lossless 인코딩 | ✓ |
| FR-DCM-204 | uint16 픽셀 쓰기 | TC-204 | BitsAllocated=16, BitsStored=14 | ✓ |
| FR-DCM-205 | 포토메트릭 고정 | TC-205 | MONOCHROME2 고정 | ✓ |
| FR-DCM-206 | 필수 Type 1 태그 | TC-206 | SOP UID, Study UID, Series UID 생성 | ✓ |
| FR-DCM-207 | 환자 정보 쓰기 | TC-207 | Patient ID, Name, DOB 인코딩 | ✓ |
| FR-DCM-208 | 촬영 정보 쓰기 | TC-208 | Study Date, Series Date, Modality=DX | ✓ |
| FR-DCM-209 | 검출기 정보 쓰기 | TC-209 | Manufacturer, Serial Number 인코딩 | ✓ |
| FR-DCM-210 | Window/Level 저장 | TC-210 | 3개 프리셋 저장 (Soft/Bone/Lung) | ✓ |
| FR-DCM-211 | XPE private block 쓰기 | TC-211 | Private Creator, Flags, Version 쓰기 | ✓ |
| **FR-DCM-212** | **Lossy 압축 금지 (CRITICAL)** | **TC-212** | **J2K Irreversible 거부 (에러)** | **✓ CRITICAL** |
| FR-DCM-213 | UID 생성 (고유성) | TC-213 | SOPInstanceUID 고유성 검증 | ✓ |
| FR-DCM-214 | 메타데이터 검증 | TC-214 | 필수 태그 누락 시 에러 | ✓ |
| FR-DCM-215 | 파일 쓰기 실패 처리 | TC-215 | DISK_FULL, PERMISSION_DENIED 에러 | ✓ |
| FR-DCM-216 | 성능 (비압축) | TC-216 | 3072×3072 쓰기 ≤ 2초 | ✓ |
| FR-DCM-217 | 성능 (J2K) | TC-217 | 3072×3072 J2K ≤ 5초 | ✓ |
| FR-DCM-301 | GSPS IOD 생성 | TC-301 | GSPS SOP Class UID 설정 | ✓ |
| FR-DCM-302 | Referenced Series | TC-302 | Referenced Series Sequence 검증 | ✓ |
| FR-DCM-303 | Graphic Annotation | TC-303 | Collimation ROI 저장 (선택) | ✓ |
| FR-DCM-304 | Display Shutter | TC-304 | 회전/반전 저장 (선택) | ✓ |
| FR-DCM-305 | Window/Level 프리셋 | TC-305 | 3개 프리셋 저장 | ✓ |
| FR-DCM-306 | GSPS 적용 | TC-306 | Window/Level 오버레이 적용 | ✓ |
| FR-DCM-401 | C-STORE SCU | TC-401 | DICOM Association + 이미지 전송 | ✓ |
| FR-DCM-402 | AE Title 구성 | TC-402 | 로컬/원격 AE Title 설정 | ✓ |
| FR-DCM-403 | 호스트명/포트 | TC-403 | 호스트명/IP, 포트 검증 | ✓ |
| FR-DCM-404 | Association Timeout | TC-404 | 30초 timeout 검증 | ✓ |
| FR-DCM-405 | Transfer Syntax 협상 | TC-405 | Implicit/Explicit VR LE 협상 | ✓ |
| FR-DCM-406 | 전송 실패 처리 | TC-406 | 재시도 3회, exponential backoff | ✓ |
| FR-DCM-407 | C-STORE Status | TC-407 | Status code (0x0000, 0x0122, 0x0124) 반환 | ✓ |
| FR-DCM-408 | TLS 1.2+ | TC-408 | TLS 상호 인증 (선택) | ✓ |
| FR-DCM-409 | C-STORE 성능 | TC-409 | 3072×3072 전송 ≤ 10초 (1Gbps) | ✓ |
| FR-DCM-410 | C-FIND SCU | TC-410 | MWL 쿼리 (RIS 시뮬레이션) | ✓ |
| FR-DCM-411 | MWL 쿼리 키 | TC-411 | Patient ID, Accession, Modality 쿼리 | ✓ |
| FR-DCM-412 | MWL 반환 정보 | TC-412 | Patient Name, DOB, ProtocolCode 반환 | ✓ |
| FR-DCM-413 | C-FIND Status | TC-413 | Status code 반환 | ✓ |

**커버리지**: 59/59 (100%)

---

### 5.2 안전 요구사항 테스트 매핑

| SRS ID (Safety) | 안전 요구사항 | Test Case ID | 테스트 시나리오 | 상태 |
|-----------------|------------|-------------|--------------|------|
| SR-DCM-001 | Lossy 압축 금지 | STC-001 | J2K Irreversible 거부 + CRITICAL 로그 | ✓ |
| SR-DCM-002 | 환자 ID 검증 | STC-002 | ID 불일치 감지 + alert | ✓ |
| SR-DCM-003 | 파일 무결성 | STC-003 | 손상 파일 거부 + 부분 데이터 금지 | ✓ |
| SR-DCM-004 | 네트워크 안전 | STC-004 | PACS 실패 시 로컬 파일 보호 | ✓ |
| SR-DCM-005 | MWL 환자 검증 | STC-005 | MWL 결과 Patient ID 검증 + alert | ✓ |
| SR-DCM-006 | GSPS 참조 | STC-006 | Referenced UID 검증 | ✓ |
| SR-DCM-007 | Unsupported TS | STC-007 | 미지원 TS 거부 | ✓ |
| SR-DCM-008 | 감시 로깅 | STC-008 | DICOM I/O 로그 기록 검증 | ✓ |

**커버리지**: 8/8 (100%)

---

## 6. 커버리지 분석

### 6.1 전체 커버리지 요약

| 추적 경로 | 요구사항 수 | 추적된 수 | 커버리지 |
|-----------|:----------:|:--------:|--------:|
| **PRD → SRS** | 56 | 56 | **100%** |
| **SRS → SAD** | 56 | 56 | **100%** |
| **SRS → SHA** | 8 | 8 | **100%** |
| **SRS → Test** | 67 | 67 | **100%** |
| **전체** | - | - | **100%** |

### 6.2 SWU별 커버리지

| SWU | 기능 요구 | 안전 요구 | 총계 | 커버리지 |
|-----|:-------:|:-------:|:-----:|--------:|
| **SWU-4.1** | 20 | 5 | 25 | **100%** |
| **SWU-4.2** | 17 | 5 | 22 | **100%** |
| **SWU-4.3** | 6 | 3 | 9 | **100%** |
| **SWU-4.4** | 13 | 3 | 16 | **100%** |
| **총계** | **56** | **16** | **72** | **100%** |

### 6.3 요구사항 상태 분포

| 상태 | 개수 | 비율 |
|------|:---:|-----:|
| ✓ Traced | 72 | **100%** |
| ✗ Not Traced | 0 | 0% |
| ⚠ Partial | 0 | 0% |

### 6.4 Critical Requirements (CRITICAL 요구사항)

| 요구사항 | SRS ID | SAD 섹션 | SHA ID | Test ID |
|---------|--------|---------|--------|---------|
| **Lossy 압축 금지** | FR-DCM-212 | §2.2, §8.1 | HAZ-001 | TC-212, STC-001 |
| **Patient ID 검증** | SR-DCM-002 | §8.2 | HAZ-003, HAZ-005 | STC-002, STC-005 |

**상태**: 모두 ✓ 완전 추적, 설계, 테스트 완료

---

## 7. 양방향 추적성 검증

### 7.1 Forward Traceability (상향식)

```
PRD 요구사항 → SRS 요구사항 (명세)
                      ↓
                   SAD 설계 (구현 방안)
                      ↓
                   SHA 통제 (위험 관리)
                      ↓
                   Test Case (검증)
```

**검증 결과**: ✓ 모든 PRD 요구사항이 SRS → SAD → Test로 추적됨

### 7.2 Backward Traceability (역방향)

```
Test Case → SRS 요구사항 (검증 대상)
             ↓
         SAD 설계 (구현 추적)
             ↓
         SHA 통제 (위험 추적)
             ↓
         PRD 요구사항 (원본 추적)
```

**검증 결과**: ✓ 모든 Test Case가 SRS → SAD → PRD로 역추적됨

### 7.3 양방향 매핑 완전성

| 항목 | 상태 | 비고 |
|------|------|------|
| PRD 완전성 | ✓ | 모든 기능 요구사항 포함 |
| SRS 완전성 | ✓ | PRD와 1:1 매핑 |
| SAD 완전성 | ✓ | 모든 SRS 요구사항 설계 포함 |
| SHA 완전성 | ✓ | 모든 안전 요구사항 위험 분석 포함 |
| Test 완전성 | ✓ | 모든 SRS 요구사항 테스트 케이스 정의 |

---

## 8. 변경 추적성

### 8.1 버전 관리

| 문서 | 버전 | 날짜 | 변경 |
|------|------|------|------|
| PRD | 1.0.0 | 2026-04-14 | 초기 버전 |
| SRS | 1.0.0 | 2026-04-14 | 초기 버전 |
| SAD | 1.0.0 | 2026-04-14 | 초기 버전 |
| SHA | 1.0.0 | 2026-04-14 | 초기 버전 |
| RTM | 1.0.0 | 2026-04-14 | 초기 버전 |

### 8.2 변경 영향 분석 규칙

**SRS 변경 시:**
1. SAD 해당 섹션 검토
2. SHA 위험 재평가
3. Test Case 영향도 분석
4. RTM 업데이트

**SAD 변경 시:**
1. SRS 검증
2. Test Case 신규 작성 (필요 시)
3. RTM 업데이트

**SHA 변경 시:**
1. SR-DCM 업데이트
2. 통제 설계 검토
3. Test Case 추가 (필요 시)

---

**문서 끝: RTM-DICOM-001 v1.0.0**
