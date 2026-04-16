# X-ray DICOM I/O 및 네트워크 모듈 — 제품 요구사항 문서 (PRD)

**문서 ID**: xpe-dicom-prd  
**모듈**: `xpe_dicom.dll` (Layer 1, Phase 1b)  
**안전 등급**: IEC 62304 Class B  
**문서 버전**: 1.0.0  
**날짜**: 2026-04-14  
**저자**: XPE 개발팀  
**규범 사양**: SPEC-XPE-MASTER v2.0.0, api-spec.md v1.2.0

---

## 목차

1. [개요](#1-개요)
2. [범위 및 경계](#2-범위-및-경계)
3. [소프트웨어 단위별 요구사항](#3-소프트웨어-단위별-요구사항)
4. [DICOM 객체 모델](#4-dicom-객체-모델)
5. [네트워크 아키텍처](#5-네트워크-아키텍처)
6. [기능 요구사항 명세](#6-기능-요구사항-명세)
7. [안전 요구사항](#7-안전-요구사항)
8. [성능 및 용량 요구사항](#8-성능-및-용량-요구사항)
9. [SOUP 의존성](#9-soup-의존성)
10. [참고문헌](#10-참고문헌)

---

## 1. 개요

`xpe_dicom.dll`은 X-ray FPD 이미지 처리 엔진의 DICOM I/O 및 네트워킹 계층입니다. 다음의 역할을 수행합니다:

- **DICOM 파일 I/O**: Digital Radiography (DX), Computed Radiography (CR), Presentation State (GSPS) IOD 읽기/쓰기
- **DICOM 네트워크**: C-STORE SCU (이미지 전송), C-FIND SCU (MWL 쿼리) 지원
- **메타데이터 관리**: 환자, 촬영, 장비, 처리 정보 인코딩/추출
- **픽셀 데이터 변환**: JPEG 2000 압축, Transfer Syntax 협상
- **Presentation State**: GSPS 링크, Window/Level 저장

### 주요 특성

| 특성 | 설명 |
|------|------|
| **IOD 지원** | DX (1.2.840.10008.5.1.4.1.1.1.1), CR (1.2.840.10008.5.1.4.1.1.2), GSPS (1.2.840.10008.5.1.4.1.1.11.1) |
| **Transfer Syntax** | Implicit VR LE, Explicit VR LE, JPEG 2000 Lossless, JPEG Baseline |
| **픽셀 포맷** | UINT16 (원본), FLOAT32 (처리됨) |
| **포토메트릭** | MONOCHROME1, MONOCHROME2 (자동 반전) |
| **네트워크** | DICOM C-STORE, C-FIND, TLS 1.2+ 상호 인증 |
| **메타데이터** | 환자 ID, 촬영 날짜, Window/Level, XPE 처리 플래그 (private block) |

---

## 2. 범위 및 경계

### 2.1 포함 범위

- DICOM 파일 읽기: 메타데이터 추출, 픽셀 데이터 디코딩
- DICOM 파일 쓰기: DX IOD 생성, JPEG 2000 lossless 인코딩
- Presentation State (GSPS) 생성 및 적용
- DICOM 네트워크 통신: C-STORE, C-FIND MWL
- 환자 ID 검증, 메타데이터 무결성 확인
- XPE 처리 이력 (private tag 0019,xx00)

### 2.2 제외 범위

- DICOM 스토리지 관리 (PACS는 외부 시스템)
- RIS/HIS 통합 (MWL 쿼리만 지원)
- SR (Structured Report) 또는 KO (Key Object Note) 생성
- Multiframe 또는 3D 볼륨 (2D radiography만)
- 영상 재구성 (전처리 이후 원본 데이터 사용 안 함)

### 2.3 아키텍처 경계

```
┌─────────────────────────────────────────┐
│         ImageProcTest.exe (C#)          │
│       (DICOM 명령 오케스트레이션)       │
└─────────────┬───────────────────────────┘
              │ P/Invoke
              v
┌─────────────────────────────────────────┐
│        xpe_dicom.dll (이 모듈)          │
│  ┌──────────────────────────────────┐   │
│  │ SWU-4.1 DicomReader             │   │
│  │ SWU-4.2 DicomWriter             │   │
│  │ SWU-4.3 PresentationStateIO      │   │
│  │ SWU-4.4 DicomNetworkSCU          │   │
│  └──────────────────────────────────┘   │
└─────────────┬───────────────────────────┘
              │ 링크 의존성
              v
┌─────────────────────────────────────────┐
│    xpe_common.dll (Layer 0)             │
│  (Types, Memory, Config, Logger, Error) │
└─────────────────────────────────────────┘
              │
              v
┌─────────────────────────────────────────┐
│        SOUP: DCMTK, OpenJPEG             │
└─────────────────────────────────────────┘
```

---

## 3. 소프트웨어 단위별 요구사항

### 3.1 SWU-4.1: DicomReader (DICOM 파일 읽기)

**책임**: DICOM 파일을 읽고, 메타데이터를 추출하고, 픽셀 데이터를 디코딩합니다.

#### 입력

- DICOM 파일 경로 (문자열, 절대 또는 상대)
- 픽셀 포맷 모드: UINT16 또는 FLOAT32
- 메타데이터 필터 (선택)

#### 출력

- `XpeImage` 구조체:
  - `width`, `height` (픽셀 단위)
  - `pixelData` (uint16 또는 float32 포인터)
  - `pixelSpacing` (mm/pixel)
  - `windowCenter`, `windowWidth` (DICOM VOI LUT)
  - `metadata` (환자 ID, 촬영 날짜, 장비 정보, SOP Class/Instance UID)

#### 요구사항

| ID | 요구사항 | 우선순위 |
|----|---------|:--------:|
| **REQ-4.1.1** | Implicit VR Little Endian Transfer Syntax 지원 | M |
| **REQ-4.1.2** | Explicit VR Little Endian Transfer Syntax 지원 | M |
| **REQ-4.1.3** | JPEG 2000 Lossless Transfer Syntax 읽기 | M |
| **REQ-4.1.4** | JPEG Baseline (Process 1) Transfer Syntax 읽기 (legacy) | O |
| **REQ-4.1.5** | DX (1.2.840.10008.5.1.4.1.1.1.1) IOD 파싱 | M |
| **REQ-4.1.6** | CR (1.2.840.10008.5.1.4.1.1.2) IOD 파싱 | M |
| **REQ-4.1.7** | GSPS (1.2.840.10008.5.1.4.1.1.11.1) IOD 읽기 (Presentation State 참조 확인) | M |
| **REQ-4.1.8** | 픽셀 데이터 추출: (0028,0010) Rows, (0028,0011) Columns 읽기 | M |
| **REQ-4.1.9** | 포토메트릭 인터프리테이션 읽기: MONOCHROME1 (자동 반전), MONOCHROME2 | M |
| **REQ-4.1.10** | 픽셀 간격 읽기: (0028,0030) Image Pixel Spacing [mm/pixel] | M |
| **REQ-4.1.11** | Window/Level 읽기: (0028,1050) Window Center, (0028,1051) Window Width | M |
| **REQ-4.1.12** | 환자 정보 추출: Patient ID (0010,0020), Patient Name (0010,0010), DOB (0010,0030) | M |
| **REQ-4.1.13** | 촬영 정보 추출: Study Date (0008,0020), Series Date (0008,0021), Content Date (0008,0023) | M |
| **REQ-4.1.14** | 검출기 정보 추출: Manufacturer (0008,0070), Device Serial Number (0018,1000) | M |
| **REQ-4.1.15** | 신체 부위 추출: Body Part Examined (0018,0015) 태그 | M |
| **REQ-4.1.16** | XPE private block (0019,xx00) 읽기: XPE 처리 플래그, 버전, 캘리브레이션 날짜 | O |
| **REQ-4.1.17** | 파일 무결성 검증: 파일 없음 → `XPE_ERR_FILE_NOT_FOUND` | M |
| **REQ-4.1.18** | DICOM 형식 검증: 잘못된 헤더 → `XPE_ERR_DICOM_INVALID` | M |
| **REQ-4.1.19** | Unsupported Transfer Syntax → `XPE_ERR_DICOM_UNSUPPORTED_TRANSFER_SYNTAX` | M |
| **REQ-4.1.20** | 메모리 효율: 최대 3072×3072 픽셀 이미지 로드 ≤ 150 MB 메모리 | M |

---

### 3.2 SWU-4.2: DicomWriter (DICOM 파일 쓰기)

**책임**: 처리된 이미지를 DICOM DX IOD로 인코딩하고 파일에 씁니다.

#### 입력

- `XpeImage` 구조체 (float32 픽셀 데이터, 메타데이터)
- 출력 파일 경로
- Transfer Syntax (기본값: Explicit VR LE, 선택: JPEG 2000 Lossless)
- 환자/촬영 메타데이터 (선택적 오버라이드)

#### 출력

- DICOM 파일 (바이너리)
- 반환 상태: `XPE_OK` 또는 에러 코드

#### 요구사항

| ID | 요구사항 | 우선순위 |
|----|---------|:--------:|
| **REQ-4.2.1** | DX IOD (1.2.840.10008.5.1.4.1.1.1.1) 생성 | M |
| **REQ-4.2.2** | Explicit VR Little Endian Transfer Syntax 쓰기 (기본값) | M |
| **REQ-4.2.3** | JPEG 2000 Lossless Transfer Syntax 쓰기 (선택) | M |
| **REQ-4.2.4** | 픽셀 데이터 쓰기: uint16 (14비트 stored in 16비트 allocated) | M |
| **REQ-4.2.5** | 포토메트릭: MONOCHROME2 고정 (자동 정규화) | M |
| **REQ-4.2.6** | 필수 Type 1 태그: SOPClassUID, SOPInstanceUID, StudyInstanceUID, SeriesInstanceUID | M |
| **REQ-4.2.7** | 환자 정보 쓰기: Patient ID, Patient Name, Patient DOB | M |
| **REQ-4.2.8** | 촬영 정보 쓰기: Study Date, Series Date, Content Date, Modality = "DX" | M |
| **REQ-4.2.9** | 검출기 정보 쓰기: Manufacturer, Device Serial Number | M |
| **REQ-4.2.10** | Window/Level 저장: (0028,1050)/(0028,1051) | M |
| **REQ-4.2.11** | XPE private block (0019,xx00) 쓰기: 처리 플래그, XPE 버전, 캘리브레이션 날짜 | O |
| **REQ-4.2.12** | 픽셀 데이터 로스리스 J2K 압축: 손실 J2K 금지 (진단 이미지) | M |
| **REQ-4.2.13** | Lossy 압축 거부: J2K irreversible mode 감지 → `XPE_ERR_LOSSY_COMPRESSION_NOT_ALLOWED` | **CRITICAL** |
| **REQ-4.2.14** | UID 생성: SOPInstanceUID는 고유 UUID (DICOM UID format) | M |
| **REQ-4.2.15** | 메타데이터 검증: 필수 태그 누락 → `XPE_ERR_DICOM_MISSING_REQUIRED_TAG` | M |
| **REQ-4.2.16** | 파일 쓰기 실패 처리: 디스크 공간 부족 → `XPE_ERR_FILE_WRITE_FAILED` | M |
| **REQ-4.2.17** | 성능: 3072×3072 uint16 이미지 DICOM 쓰기 ≤ 2 초 (비압축) | M |
| **REQ-4.2.18** | 성능: JPEG 2000 Lossless 압축 ≤ 5 초 | O |

---

### 3.3 SWU-4.3: PresentationStateIO (DICOM GSPS)

**책임**: DICOM Grayscale Softcopy Presentation State (GSPS) 생성, 저장, 적용합니다.

#### 입력

- 원본 DX 이미지의 SOP Instance UID
- Window/Level 설정
- Collimation ROI (선택)
- 사용자 회전/반전 상태 (선택)

#### 출력

- GSPS DICOM 파일
- GSPS SOP Instance UID

#### 요구사항

| ID | 요구사항 | 우선순위 |
|----|---------|:--------:|
| **REQ-4.3.1** | GSPS IOD (1.2.840.10008.5.1.4.1.1.11.1) 생성 | M |
| **REQ-4.3.2** | Referenced Series Sequence (0008,1115) 설정: 원본 DX SOP 참조 | M |
| **REQ-4.3.3** | Graphic Annotation (collimation ROI) 저장 (선택) | O |
| **REQ-4.3.4** | Display Shutter (회전/반전) 저장 (선택) | O |
| **REQ-4.3.5** | Window/Level 프리셋 저장 | M |
| **REQ-4.3.6** | GSPS 적용: 원본 이미지에 Window/Level 오버레이 | M |
| **REQ-4.3.7** | GSPS 파일 이름: `{original_uid}.gsps.dcm` 형식 | M |

---

### 3.4 SWU-4.4: DicomNetworkSCU (DICOM 네트워크)

**책임**: DICOM Service Class User (SCU) 역할로 PACS/RIS와 통신합니다.

#### 3.4.1 C-STORE SCU (이미지 전송)

**기능**: 처리된 DX 이미지를 PACS로 전송합니다.

| 요구사항 | 우선순위 |
|---------|:--------:|
| **REQ-4.4.1.1** | C-STORE SCU 구현: DICOM Association 협상, 이미지 전송 | M |
| **REQ-4.4.1.2** | AE Title 구성: xpe_dicom (로컬), PACS AE Title (원격) | M |
| **REQ-4.4.1.3** | 호스트명, 포트번호 구성 | M |
| **REQ-4.4.1.4** | Association timeout: 기본 30초 (구성 가능) | M |
| **REQ-4.4.1.5** | Implicit VR LE + Explicit VR LE Transfer Syntax 지원 | M |
| **REQ-4.4.1.6** | 전송 실패 처리: retry up to 3회, exponential backoff | M |
| **REQ-4.4.1.7** | 반환값: C-STORE Status (0x0000 Success, 0x0122 Warning, 0x0124 Failure) | M |
| **REQ-4.4.1.8** | TLS 1.2+ 상호 인증 (선택) | O |
| **REQ-4.4.1.9** | 성능: 3072×3072 이미지 전송 ≤ 10초 (1 Gbps LAN) | M |

#### 3.4.2 C-FIND SCU (MWL 쿼리)

**기능**: Modality Worklist (MWL)을 RIS에서 쿼리합니다.

| 요구사항 | 우선순위 |
|---------|:--------:|
| **REQ-4.4.2.1** | C-FIND SCU 구현: MWL SOP Class 쿼리 | M |
| **REQ-4.4.2.2** | 쿼리 키: Patient ID, Accession Number, Scheduled Date, Modality (DX) | M |
| **REQ-4.4.2.3** | 반환 키: PatientName, PatientID, PatientBirthDate, AccessionNumber, ScheduledProtocolCode | M |
| **REQ-4.4.2.4** | Wildcard 쿼리 지원: "*" (all), "PAT*" (partial match) | O |
| **REQ-4.4.2.5** | 응답 타임아웃: 기본 30초 | M |
| **REQ-4.4.2.6** | 응답 개수 제한: 최대 100개 결과 | M |
| **REQ-4.4.2.7** | C-FIND Status 반환: 0x0000 Success, 0x0122 Warning | M |

---

## 4. DICOM 객체 모델

### 4.1 IOD 계층 구조

```
DICOM SOP Class
├── DX (1.2.840.10008.5.1.4.1.1.1.1)
│   ├── Patient Module (환자 정보)
│   ├── Study Module (촬영 정보)
│   ├── Series Module (시리즈 정보)
│   ├── Image Pixel Module (픽셀 데이터)
│   └── Image-related General Equipment Module
│
├── CR (1.2.840.10008.5.1.4.1.1.2)
│   └── [DX와 동일 구조]
│
└── GSPS (1.2.840.10008.5.1.4.1.1.11.1)
    ├── Patient Module
    ├── Study Module
    ├── Series Module
    ├── Equipment Module
    ├── Referenced Image Sequence
    └── Presentation Label Sequence
```

### 4.2 핵심 속성 (Tags)

| 그룹,요소 | 이름 | 필수 | 설명 |
|-----------|------|:---:|------|
| (0008,0016) | SOPClassUID | M | SOP 클래스 UID |
| (0008,0018) | SOPInstanceUID | M | 고유 인스턴스 UID |
| (0008,0020) | StudyDate | M | 촬영 날짜 (YYYYMMDD) |
| (0008,0070) | Manufacturer | M | 장비 제조사 |
| (0010,0020) | PatientID | M | 환자 ID |
| (0010,0010) | PatientName | M | 환자 이름 |
| (0028,0010) | Rows | M | 이미지 높이 (픽셀) |
| (0028,0011) | Columns | M | 이미지 너비 (픽셀) |
| (0028,0030) | PixelSpacing | M | 픽셀 간격 [mm] |
| (0028,0100) | BitsAllocated | M | 16 (uint16) |
| (0028,0101) | BitsStored | M | 14 (또는 12) |
| (0028,0102) | HighBit | M | 13 (또는 11) |
| (0028,1050) | WindowCenter | M | Window Center (VOI LUT) |
| (0028,1051) | WindowWidth | M | Window Width (VOI LUT) |
| (0018,0015) | BodyPartExamined | M | 검사 신체 부위 |
| (0008,1115) | ReferencedSeriesSequence | M (GSPS) | 원본 Series 참조 |
| (0019,xx00) | XPE_ProcessingFlags | O | XPE private block |

### 4.3 Presentation State 객체 모델

```
GSPS Document
├── Referenced DX SOP Instance UID
├── Window/Level Presets
│   ├── Preset 1: (Center=40, Width=400) — Soft Tissue
│   ├── Preset 2: (Center=300, Width=1500) — Bone
│   └── Preset 3: (Center=600, Width=2000) — Lung
├── Graphic Annotations
│   └── Collimation ROI (선택)
└── Display Shutter
    └── Rotation/Flip state (선택)
```

---

## 5. 네트워크 아키텍처

### 5.1 DICOM Association 상태 머신

```
START
  │
  ├─ [Request] ─→ ASSOCIATION_REQUESTED
  │                 │
  │                 ├─ [Accept] ─→ ASSOCIATION_ESTABLISHED
  │                 │                 │
  │                 │                 ├─ C-STORE ─→ IMAGE_SENDING
  │                 │                 │               │
  │                 │                 │               └─ [Complete] ─→ ASSOCIATION_ESTABLISHED
  │                 │                 │
  │                 │                 ├─ C-FIND ─→ QUERYING_MWL
  │                 │                 │               │
  │                 │                 │               └─ [Complete] ─→ ASSOCIATION_ESTABLISHED
  │                 │                 │
  │                 │                 └─ [Release] ─→ ASSOCIATION_CLOSED
  │                 │
  │                 └─ [Reject] ─→ ASSOCIATION_REJECTED (error)
  │
  └─ CLOSED
```

### 5.2 네트워크 타임아웃 정책

| 타임아웃 | 기본값 | 구성 가능 | 설명 |
|----------|:-----:|:-------:|--------|
| **Association Timeout** | 30 s | Yes | PACS 응답 대기 |
| **DIMSE Timeout** | 120 s | Yes | C-STORE/C-FIND 명령 완료 대기 |
| **Inactivity Timeout** | 300 s | Yes | 비활성 연결 자동 종료 |
| **Retry Interval** | 2, 4, 8 s | Yes | Exponential backoff (max 3 재시도) |

### 5.3 TLS/SSL 보안 (선택)

- **Protocol**: TLS 1.2+
- **Mutual Authentication**: 클라이언트 + 서버 인증서
- **Certificate Validation**: CN (Common Name) 검증
- **Configuration**: `xpe_configure()` → DICOM Network 섹션

---

## 6. 기능 요구사항 명세

### 6.1 파일 I/O 요구사항

| ID | 요구사항 | 조건 | 제약사항 |
|----|---------|------|--------|
| **FR-DCM-101** | DICOM 파일 읽기 | 파일 존재 | Timeout ≤ 2초 (3072×3072 이미지) |
| **FR-DCM-102** | Transfer Syntax 협상 | 수신 Syntax 검증 | Unsupported → error |
| **FR-DCM-103** | Pixel data 추출 | Format 변환 (uint16 또는 float32) | 메모리 효율 ≤ 150 MB |
| **FR-DCM-104** | DICOM 파일 쓰기 | 경로 유효성 검증 | 디스크 공간 확인 |
| **FR-DCM-105** | UID 생성 | SOPInstanceUID 고유성 | DICOM UID format 준수 |
| **FR-DCM-106** | JPEG 2000 인코딩 | Lossless 모드만 | Irreversible 거부 |

### 6.2 메타데이터 요구사항

| ID | 요구사항 | 조건 | 제약사항 |
|----|---------|------|--------|
| **FR-DCM-201** | 환자 ID 검증 | 파일 읽을 때 | 공백 허용 안 함 |
| **FR-DCM-202** | 환자 ID 추적 | 쓰기 시 검증 | 원본과 일치 확인 |
| **FR-DCM-203** | 촬영 메타데이터 보존 | 읽기 → 쓰기 | 손실 없음 |
| **FR-DCM-204** | XPE private block 쓰기 | 처리 이력 저장 | Block (0019,xx00) |
| **FR-DCM-205** | Window/Level 저장 | GSPS에서 | 프리셋 3개 (Soft/Bone/Lung) |

### 6.3 네트워크 요구사항

| ID | 요구사항 | 조건 | 제약사항 |
|----|---------|------|--------|
| **FR-DCM-301** | C-STORE SCU | PACS 연결 | Timeout 30초, 재시도 3회 |
| **FR-DCM-302** | C-FIND SCU (MWL) | RIS 연결 | 결과 최대 100개 |
| **FR-DCM-303** | Association 관리 | 명시적 Release | 자동 cleanup ≤ 5초 |
| **FR-DCM-304** | TLS 1.2+ | 선택 기능 | Mutual auth 지원 |
| **FR-DCM-305** | Network error handling | 연결 실패 | Retry + exponential backoff |

---

## 7. 안전 요구사항

### 7.1 Lossy 압축 거부 (CRITICAL)

**SAF-DCM-001**: 진단 이미지에 손실 압축 금지

```
IF Transfer Syntax = JPEG 2000 Irreversible (1.2.840.10008.1.2.4.92) THEN
  FAIL with XPE_ERR_LOSSY_COMPRESSION_NOT_ALLOWED
ENDIF
```

**검증 방법**: DICOM 파일 작성 전 Transfer Syntax 확인, compression parameters 검증

---

### 7.2 환자 ID 검증 (CRITICAL)

**SAF-DCM-002**: 메타데이터 일치성 확인

```
IF PatientID_in_file != PatientID_in_metadata THEN
  FAIL with XPE_ERR_PATIENT_ID_MISMATCH
  ALERT: "Patient ID mismatch detected"
ENDIF
```

---

### 7.3 파일 무결성 확인

**SAF-DCM-003**: 손상된 DICOM 파일 거부

| 조건 | 동작 |
|------|------|
| DICOM 헤더 손상 | Error, partial data 반환 금지 |
| 픽셀 데이터 누락 | Error, 대체 데이터 사용 금지 |
| Rows/Columns 불일치 | Error, 크기 추론 금지 |

---

### 7.4 네트워크 장애 처리

**SAF-DCM-004**: 전송 실패 시 로컬 파일 보호

```
IF C-STORE failed THEN
  local_dicom_file remains unchanged
  error returned to caller
  retry offered (max 3 times)
ENDIF
```

---

### 7.5 MWL 쿼리 환자 검증

**SAF-DCM-005**: 잘못된 환자 선택 방지

```
IF C-FIND query returns results THEN
  verify PatientID matches requested value
  prevent automatic selection of wrong patient
ENDIF
```

---

### 7.6 GSPS 참조 검증

**SAF-DCM-006**: GSPS와 원본 이미지 링크 무결성

```
IF GSPS references primary image THEN
  verify Referenced Series UID matches original
  validate all graphic annotations within image bounds
ENDIF
```

---

### 7.7 Unsupported Transfer Syntax 감지

**SAF-DCM-007**: 지원되지 않는 형식 즉시 거부

```
IF Transfer Syntax NOT in {Implicit VR LE, Explicit VR LE, J2K Lossless, JPEG Baseline} THEN
  return XPE_ERR_DICOM_UNSUPPORTED_TRANSFER_SYNTAX
ENDIF
```

---

### 7.8 감사 로깅

**SAF-DCM-008**: 모든 DICOM I/O 작업 기록

| 이벤트 | 로그 정보 |
|--------|---------|
| 파일 읽기 | 경로, Patient ID, Study Date, 상태 |
| 파일 쓰기 | 경로, SOP Instance UID, 파일 크기, 상태 |
| C-STORE | PACS AE Title, Image Count, Status |
| C-FIND | RIS AE Title, Query Keys, Results Count |
| Error | Error Code, Context, 시정 방법 |

---

## 8. 성능 및 용량 요구사항

### 8.1 시간 성능

| 작업 | 목표 (ms) | 최악 (ms) | 조건 |
|------|:--------:|:--------:|------|
| DICOM 파일 읽기 | 1,500 | 2,000 | 3072×3072 uint16 |
| Pixel data 디코딩 | 500 | 800 | uint16 → float32 변환 포함 |
| DICOM 파일 쓰기 (비압축) | 1,200 | 1,500 | Explicit VR LE |
| JPEG 2000 인코딩 | 4,000 | 5,000 | Lossless, 3072×3072 |
| C-STORE (LAN) | 8,000 | 10,000 | 1 Gbps, 3072×3072 이미지 |
| C-FIND 쿼리 | 2,000 | 5,000 | 10개 결과 반환 |

### 8.2 메모리 할당

| 구성요소 | 크기 | 참고 |
|---------|:---:|------|
| DICOM 파일 버퍼 (uint16) | 18.9 MB | 3072×3072 |
| 픽셀 데이터 버퍼 (float32) | 37.7 MB | 변환 후 |
| DCMTK 내부 버퍼 | ~10 MB | JPEG 2000 codec |
| 메타데이터 구조 | ~1 MB | 최대 1000개 태그 |
| 네트워크 버퍼 | 1 MB | TCP/IP |
| **최고 메모리** | **~70 MB** | 동시 I/O 작업 없음 |

### 8.3 저장소 요구사항

| 파일 타입 | 크기 | 압축 | 참고 |
|----------|:---:|:----:|------|
| Raw DICOM (uint16) | 18.9 MB | 비압축 | 3072×3072 |
| JPEG 2000 Lossless | 8-12 MB | 50% 압축률 | 일반적인 X-ray 이미지 |
| GSPS | 50 KB | 매우 작음 | 메타데이터만 |

---

## 9. SOUP 의존성

### 9.1 DCMTK (DICOM Toolkit)

| 항목 | 명세 |
|------|------|
| **라이센스** | BSD 3-Clause |
| **버전** | 3.6.8 (또는 최신) |
| **사용 모듈** | dcmdata, dcmimgle, dcmjpeg2k, dcmnet |
| **용도** | DICOM 파일 읽기/쓰기, Network |
| **상태** | 검증됨, 의료 기기 승인 |
| **링크** | https://github.com/DCMTK/dcmtk |

### 9.2 OpenJPEG

| 항목 | 명세 |
|------|------|
| **라이센스** | BSD 2-Clause |
| **버전** | 2.5.0 (또는 최신) |
| **사용 모듈** | openjp2 (JPEG 2000 codec) |
| **용도** | Lossless J2K 인코딩/디코딩 |
| **상태** | 검증됨 |
| **링크** | https://github.com/uclouvain/openjpeg |

### 9.3 OpenSSL (TLS, optional)

| 항목 | 명세 |
|------|------|
| **라이센스** | Apache 2.0 |
| **버전** | 1.1.1+ (TLS 1.2+) |
| **용도** | DICOM Network TLS 보안 |
| **상태** | 선택 기능 |

---

## 10. 참고문헌

### 표준

| 표준 | 제목 | 관련성 |
|------|------|--------|
| DICOM PS3.1-2023 | Introduction & Overview | 기본 개념 |
| DICOM PS3.3-2023 | Information Object Definitions | IOD 정의 |
| DICOM PS3.5-2023 | Data Structure and Encoding | Encoding 규칙 |
| DICOM PS3.6-2023 | Data Dictionary | Tag 정의 |
| DICOM PS3.7-2023 | Message Exchange | Network 메시지 |
| DICOM PS3.8-2023 | Network Communication | DIMSE 프로토콜 |
| DICOM PS3.14-2023 | Grayscale Standard Display Function | Presentation |
| IEC 62304:2006+A1:2015 | Medical device software lifecycle | 소프트웨어 등급 |
| ISO 14971:2019 | Medical device risk management | 위험 관리 |

### 프로젝트 문서

| 문서 | 경로 |
|------|------|
| SPEC-XPE-MASTER v2.0.0 | `.moai/specs/SPEC-XPE-MASTER.md` |
| api-spec.md v1.2.0 | `.moai/specs/api-spec.md` |
| XPE-SRS-001 v1.0 | `docs/project/XPE-SRS-001.md` |
| SAD-DICOM-001 (본 패키지의 문서 3) | `docs/dicom/SAD-DICOM-001_Software_Architecture_Document.md` |

---

**문서 끝: xpe-dicom-prd v1.0.0**
