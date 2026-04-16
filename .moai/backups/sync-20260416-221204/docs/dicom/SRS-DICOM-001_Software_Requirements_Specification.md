# DICOM I/O 모듈 — 소프트웨어 요건 명세서 (SRS)

**문서 ID**: SRS-DICOM-001  
**버전**: 1.0.0  
**날짜**: 2026-04-14  
**IEC 62304 절**: 5.2.1 — Software Requirements Analysis  
**안전 등급**: Class B  

---

## 목차

1. [소개](#1-소개)
2. [기능 요구사항 (FR)](#2-기능-요구사항-fr)
3. [안전 요구사항 (SR)](#3-안전-요구사항-sr)
4. [성능 요구사항 (PR)](#4-성능-요구사항-pr)
5. [인터페이스 요구사항 (IR)](#5-인터페이스-요구사항-ir)
6. [추적성](#6-추적성)

---

## 1. 소개

이 문서는 `xpe_dicom.dll` (DICOM I/O 및 네트워크 모듈)의 소프트웨어 요구사항을 명시합니다.

### 1.1 범위

- DICOM 파일 읽기/쓰기 (DX, CR, GSPS IOD)
- Transfer Syntax 협상 및 변환
- DICOM 네트워크 (C-STORE SCU, C-FIND SCU)
- 메타데이터 검증 및 추적

### 1.2 관련 문서

| 문서 | 용도 |
|------|------|
| xpe-dicom-prd.md | 제품 요구사항 원본 |
| SAD-DICOM-001 | 아키텍처 설계 |
| API-SPEC.md | API 인터페이스 |

---

## 2. 기능 요구사항 (FR)

### 2.1 DICOM 파일 읽기

#### FR-DCM-101: Implicit VR Little Endian Transfer Syntax 지원

- **Description**: 1.2.840.10008.1.2 Transfer Syntax 파일 읽기
- **Acceptance Criteria**:
  - Implicit VR LE로 인코딩된 DICOM 파일 파싱 성공
  - 모든 태그가 올바르게 해석됨
  - 픽셀 데이터 정확성 100% (byte-for-byte 일치)
- **Priority**: M (Mandatory)
- **Related SWU**: SWU-4.1 DicomReader

#### FR-DCM-102: Explicit VR Little Endian Transfer Syntax 지원

- **Description**: 1.2.840.10008.1.2.1 Transfer Syntax 파일 읽기
- **Acceptance Criteria**:
  - Explicit VR LE로 인코딩된 DICOM 파일 파싱 성공
  - VR 길이 필드 올바르게 처리
  - 픽셀 데이터 정확성 100%
- **Priority**: M

#### FR-DCM-103: JPEG 2000 Lossless Transfer Syntax 읽기

- **Description**: 1.2.840.10008.1.2.4.90 Transfer Syntax 파일 읽기 및 디코딩
- **Acceptance Criteria**:
  - JPEG 2000 Lossless 압축 이미지 디코딩 성공
  - OpenJPEG 라이브러리 사용
  - 복호화 후 픽셀 데이터 정확성 100% (무손실)
  - 최대 4096×4096 해상도 지원
- **Priority**: M

#### FR-DCM-104: JPEG Baseline (legacy) 읽기

- **Description**: 1.2.840.10008.1.2.4.50 JPEG Baseline Process 1 지원
- **Acceptance Criteria**:
  - 레거시 JPEG로 압축된 파일 읽기
  - CR (Computed Radiography) 이미지 호환성
- **Priority**: O (Optional)

#### FR-DCM-105: DX IOD 파싱

- **Description**: Digital Radiography IOD (1.2.840.10008.5.1.4.1.1.1.1) 파싱
- **Acceptance Criteria**:
  - SOP Class UID 확인
  - 필수 모듈 검증 (Patient, Study, Series, Image Pixel)
  - IOD 검증 실패 → XPE_ERR_DICOM_INVALID_IOD
- **Priority**: M

#### FR-DCM-106: CR IOD 파싱

- **Description**: Computed Radiography IOD (1.2.840.10008.5.1.4.1.1.2) 파싱
- **Priority**: M

#### FR-DCM-107: GSPS IOD 읽기

- **Description**: Grayscale Softcopy Presentation State IOD (1.2.840.10008.5.1.4.1.1.11.1) 파싱
- **Acceptance Criteria**:
  - Referenced Series Sequence 검증
  - 원본 DX/CR 이미지 UID 추출
  - Window/Level 프리셋 파싱
- **Priority**: M

#### FR-DCM-108: 픽셀 데이터 추출 (Rows/Columns)

- **Description**: 이미지 크기 태그 (0028,0010)/(0028,0011) 추출
- **Acceptance Criteria**:
  - 높이(Rows) 값 추출
  - 너비(Columns) 값 추출
  - 범위 검증: 1 ≤ width, height ≤ 4096
  - 범위 초과 → XPE_ERR_DICOM_INVALID_DIMENSION
- **Priority**: M

#### FR-DCM-109: 포토메트릭 인터프리테이션

- **Description**: 이미지 밝기 표현 읽기 (0028,0004)
- **Acceptance Criteria**:
  - MONOCHROME1 (작은 값 = 밝음): 자동 반전 (MAX - pixel)
  - MONOCHROME2 (작은 값 = 어두움): 그대로 사용
  - 다른 값 → XPE_ERR_DICOM_UNSUPPORTED_PHOTOMETRIC
- **Priority**: M

#### FR-DCM-110: 픽셀 간격 읽기

- **Description**: (0028,0030) Image Pixel Spacing [mm/pixel] 추출
- **Acceptance Criteria**:
  - 값 파싱: [row_spacing, column_spacing]
  - 범위 검증: 0.01 ≤ spacing ≤ 1.0 mm
  - 범위 초과 경고 발행 (에러 아님)
- **Priority**: M

#### FR-DCM-111: Window/Level 읽기

- **Description**: VOI LUT tags (0028,1050)/(0028,1051) 추출
- **Acceptance Criteria**:
  - Window Center (0028,1050) 추출
  - Window Width (0028,1051) 추출
  - VOI LUT Sequence 대체 지원 (선택)
- **Priority**: M

#### FR-DCM-112: 환자 정보 추출

- **Description**: 환자 ID (0010,0020), 이름 (0010,0010), 생년월일 (0010,0030) 추출
- **Acceptance Criteria**:
  - Patient ID: 공백 제거, 길이 ≤ 64 문자
  - Patient Name: Unicode 지원, ≤ 256 문자
  - DOB: YYYYMMDD 형식 검증
- **Priority**: M

#### FR-DCM-113: 촬영 정보 추출

- **Description**: Study Date (0008,0020), Series Date (0008,0021), Content Date (0008,0023) 추출
- **Acceptance Criteria**:
  - 날짜 포맷 검증: YYYYMMDD
  - 타임스탐프 추출 (선택): HHMMSS
- **Priority**: M

#### FR-DCM-114: 검출기 정보 추출

- **Description**: Manufacturer (0008,0070), Device Serial Number (0018,1000) 추출
- **Acceptance Criteria**:
  - 장비 제조사 문자열 추출
  - 시리얼 번호 길이 ≤ 32 문자
- **Priority**: M

#### FR-DCM-115: 신체 부위 추출

- **Description**: Body Part Examined (0018,0015) 추출
- **Acceptance Criteria**:
  - 값 추출: "CHEST", "ABDOMEN", "EXTREMITY", etc.
  - 표준화된 DICOM value set 검증 (권장)
- **Priority**: M

#### FR-DCM-116: XPE Private Block 읽기

- **Description**: XPE 처리 플래그 저장 (0019,xx00)
- **Acceptance Criteria**:
  - Private Creator tag (0019,0010) 확인: "XPE"
  - Processing Flags (0019,1001) 읽기
  - XPE Version (0019,1002) 읽기
  - Calibration Date (0019,1003) 읽기
- **Priority**: O

#### FR-DCM-117: 파일 존재 검증

- **Description**: 파일 경로 유효성 확인
- **Acceptance Criteria**:
  - 파일 없음 → XPE_ERR_FILE_NOT_FOUND
  - 파일 열기 실패 → XPE_ERR_FILE_READ_FAILED
  - Timeout: 2초 이내 응답
- **Priority**: M

#### FR-DCM-118: DICOM 형식 검증

- **Description**: DICOM preamble 및 헤더 검증
- **Acceptance Criteria**:
  - "DICM" 시그니처 확인
  - VR 길이 필드 일관성 검증
  - 손상된 DICOM → XPE_ERR_DICOM_CORRUPTED
- **Priority**: M

#### FR-DCM-119: Unsupported Transfer Syntax 감지

- **Description**: 지원되지 않는 Transfer Syntax 거부
- **Acceptance Criteria**:
  - Transfer Syntax UID 확인
  - 지원 목록: Implicit VR LE, Explicit VR LE, J2K Lossless, JPEG Baseline
  - 지원되지 않는 값 → XPE_ERR_DICOM_UNSUPPORTED_TRANSFER_SYNTAX
- **Priority**: M

#### FR-DCM-120: 메모리 효율 (읽기)

- **Description**: 메모리 사용량 제한
- **Acceptance Criteria**:
  - 최대 이미지: 3072×3072 uint16
  - 메모리 사용: ≤ 150 MB
  - 메모리 초과 → XPE_ERR_OUT_OF_MEMORY
- **Priority**: M

---

### 2.2 DICOM 파일 쓰기

#### FR-DCM-201: DX IOD 생성

- **Description**: Digital Radiography IOD (1.2.840.10008.5.1.4.1.1.1.1) 작성
- **Acceptance Criteria**:
  - SOP Class UID = 1.2.840.10008.5.1.4.1.1.1.1
  - 필수 모듈 포함: Patient, Study, Series, Image Pixel, Equipment
  - IOD 준수 검증
- **Priority**: M

#### FR-DCM-202: Explicit VR Little Endian 쓰기 (기본값)

- **Description**: 1.2.840.10008.1.2.1 Transfer Syntax로 인코딩
- **Acceptance Criteria**:
  - VR 필드 포함
  - 길이 필드 올바름
  - 픽셀 데이터 정확성 100%
  - 기본 Transfer Syntax로 자동 선택
- **Priority**: M

#### FR-DCM-203: JPEG 2000 Lossless 쓰기

- **Description**: 1.2.840.10008.1.2.4.90 Transfer Syntax로 인코딩 (선택)
- **Acceptance Criteria**:
  - OpenJPEG로 pixelData 인코딩
  - Lossless 모드 고정
  - 압축률 목표: 50~60%
  - 인코딩 시간 ≤ 5초 (3072×3072)
- **Priority**: M

#### FR-DCM-204: 픽셀 데이터 쓰기 (uint16)

- **Description**: 픽셀 값 uint16 (14-bit stored in 16-bit allocated) 인코딩
- **Acceptance Criteria**:
  - BitsAllocated = 16
  - BitsStored = 14 (또는 12)
  - HighBit = 13 (또는 11)
  - PixelRepresentation = 0 (unsigned)
- **Priority**: M

#### FR-DCM-205: 포토메트릭 고정 (MONOCHROME2)

- **Description**: 모든 출력 이미지 MONOCHROME2로 표준화
- **Acceptance Criteria**:
  - (0028,0004) PhotometricInterpretation = "MONOCHROME2"
  - 자동 정규화 (MONOCHROME1 입력일 경우)
- **Priority**: M

#### FR-DCM-206: 필수 Type 1 태그 쓰기

- **Description**: 필수 메타데이터 인코딩
- **Acceptance Criteria**:
  - SOPClassUID = 1.2.840.10008.5.1.4.1.1.1.1
  - SOPInstanceUID = 고유 UUID (DICOM format)
  - StudyInstanceUID = 입력에서 추출 또는 생성
  - SeriesInstanceUID = 입력에서 추출 또는 생성
  - 태그 누락 → XPE_ERR_DICOM_MISSING_REQUIRED_TAG
- **Priority**: M

#### FR-DCM-207: 환자 정보 쓰기

- **Description**: 환자 ID, 이름, 생년월일 인코딩
- **Acceptance Criteria**:
  - PatientID (0010,0020) 쓰기
  - PatientName (0010,0010) 쓰기
  - PatientBirthDate (0010,0030) 쓰기 (선택)
  - 데이터 검증: 공백 제거, 길이 확인
- **Priority**: M

#### FR-DCM-208: 촬영 정보 쓰기

- **Description**: Study Date, Series Date, Content Date 쓰기
- **Acceptance Criteria**:
  - StudyDate (0008,0020)
  - SeriesDate (0008,0021)
  - ContentDate (0008,0023)
  - Modality (0008,0060) = "DX" (고정)
  - 날짜 포맷 검증: YYYYMMDD
- **Priority**: M

#### FR-DCM-209: 검출기 정보 쓰기

- **Description**: Manufacturer, Device Serial Number 쓰기
- **Acceptance Criteria**:
  - Manufacturer (0008,0070)
  - DeviceSerialNumber (0018,1000)
  - 데이터 소스: xpe_common 구성 또는 입력 파라미터
- **Priority**: M

#### FR-DCM-210: Window/Level 저장

- **Description**: VOI LUT 프리셋 저장
- **Acceptance Criteria**:
  - WindowCenter (0028,1050)
  - WindowWidth (0028,1051)
  - 기본 프리셋: Soft Tissue (40, 400), Bone (300, 1500), Lung (600, 2000)
  - 프리셋 3개 모두 저장
- **Priority**: M

#### FR-DCM-211: XPE Private Block 쓰기

- **Description**: XPE 처리 이력 저장 (0019,xx00)
- **Acceptance Criteria**:
  - Private Creator (0019,0010) = "XPE"
  - Processing Flags (0019,1001) = xpe_image.flags
  - XPE Version (0019,1002) = "1.0.0"
  - Calibration Date (0019,1003) = 캘리브레이션 날짜
  - 태그 위치: (0019,1001), (0019,1002), (0019,1003)
- **Priority**: O

#### FR-DCM-212: Lossy 압축 금지 (CRITICAL)

- **Description**: 손실 JPEG 2000 압축 거부
- **Acceptance Criteria**:
  - J2K Irreversible mode (1.2.840.10008.1.2.4.92) 감지
  - 자동 거부 → XPE_ERR_LOSSY_COMPRESSION_NOT_ALLOWED
  - 에러 메시지: "Lossy J2K compression is not allowed for diagnostic images"
  - 이 조건은 우회 불가 (hard constraint)
- **Priority**: **M (CRITICAL)**

#### FR-DCM-213: UID 생성 (고유성)

- **Description**: SOPInstanceUID 고유성 보증
- **Acceptance Criteria**:
  - 각 쓰기 작업마다 새로운 UUID 생성
  - DICOM UID format 준수: X.X.X.X... (numeric only)
  - UUID uniqueness 시간당 ≤ 1 충돌 확률 (per DCMTK)
- **Priority**: M

#### FR-DCM-214: 메타데이터 검증 (쓰기)

- **Description**: 필수 태그 검증 및 데이터 무결성 확인
- **Acceptance Criteria**:
  - 모든 필수 태그 존재 확인
  - 태그 값 범위 검증 (예: 날짜 형식)
  - 데이터 타입 일치 검증
  - 검증 실패 → XPE_ERR_DICOM_INVALID_TAG
- **Priority**: M

#### FR-DCM-215: 파일 쓰기 실패 처리

- **Description**: 디스크 I/O 에러 처리
- **Acceptance Criteria**:
  - 디스크 공간 부족 → XPE_ERR_DISK_FULL
  - 권한 거부 → XPE_ERR_PERMISSION_DENIED
  - I/O 에러 → XPE_ERR_FILE_WRITE_FAILED
  - Partial write 시 파일 삭제 또는 롤백
- **Priority**: M

#### FR-DCM-216: 성능 (비압축 쓰기)

- **Description**: DICOM 쓰기 시간 요구사항
- **Acceptance Criteria**:
  - 3072×3072 uint16 이미지: ≤ 2초
  - 메타데이터 인코딩 포함
  - Explicit VR LE Transfer Syntax
- **Priority**: M

#### FR-DCM-217: 성능 (JPEG 2000 쓰기)

- **Description**: JPEG 2000 Lossless 인코딩 시간
- **Acceptance Criteria**:
  - 3072×3072 이미지: ≤ 5초
  - OpenJPEG 라이브러리 성능
- **Priority**: O

---

### 2.3 Presentation State (GSPS)

#### FR-DCM-301: GSPS IOD 생성

- **Description**: Grayscale Softcopy Presentation State IOD 작성
- **Acceptance Criteria**:
  - SOP Class UID = 1.2.840.10008.5.1.4.1.1.11.1
  - Referenced Series Sequence 포함
- **Priority**: M

#### FR-DCM-302: Referenced Series 설정

- **Description**: 원본 DX/CR 이미지 참조
- **Acceptance Criteria**:
  - Referenced Series Sequence (0008,1115) 설정
  - Referenced SOP Class UID (원본)
  - Referenced SOP Instance UID (원본)
  - 참조 무결성 검증
- **Priority**: M

#### FR-DCM-303: Graphic Annotation (ROI)

- **Description**: Collimation ROI 저장 (선택)
- **Acceptance Criteria**:
  - Graphic Data (ROI 좌표) 인코딩
  - Graphic Type = "POLYLINE" 또는 "POLYGON"
  - 바운드 검증: 이미지 영역 내
- **Priority**: O

#### FR-DCM-304: Display Shutter

- **Description**: 회전/반전 상태 저장 (선택)
- **Acceptance Criteria**:
  - Display Shutter Shape Code (0018,1600)
  - Rotation 각도 저장 (0, 90, 180, 270)
  - Flip 상태 저장 (H/V)
- **Priority**: O

#### FR-DCM-305: Window/Level 프리셋 저장

- **Description**: VOI LUT 프리셋 저장
- **Acceptance Criteria**:
  - 프리셋 3개: Soft Tissue, Bone, Lung
  - 각 프리셋 (Center, Width) 저장
  - Display Pipeline에서 적용 가능
- **Priority**: M

#### FR-DCM-306: GSPS 적용

- **Description**: GSPS 설정을 원본 이미지에 적용
- **Acceptance Criteria**:
  - Window/Level 오버레이 적용
  - ROI 표시 (있을 경우)
  - 회전/반전 적용 (있을 경우)
- **Priority**: M

---

### 2.4 DICOM 네트워크

#### FR-DCM-401: C-STORE SCU 구현

- **Description**: DICOM Service Class User (C-STORE) 구현
- **Acceptance Criteria**:
  - DICOM Association 협상
  - Image Transfer 수행
  - Response Status 수신
  - 표준 DICOM protocol 준수
- **Priority**: M

#### FR-DCM-402: AE Title 구성

- **Description**: Application Entity (AE) Title 설정
- **Acceptance Criteria**:
  - 로컬 AE Title: "xpe_dicom" (고정)
  - 원격 PACS AE Title: 구성 가능
  - 길이 ≤ 16 문자
- **Priority**: M

#### FR-DCM-403: 호스트명/포트 구성

- **Description**: PACS 연결 정보 설정
- **Acceptance Criteria**:
  - 호스트명 또는 IP 주소
  - 포트번호 (기본: 104)
  - 입력 검증: DNS 해석 가능, 포트 유효
- **Priority**: M

#### FR-DCM-404: Association Timeout

- **Description**: DICOM Association 타임아웃 설정
- **Acceptance Criteria**:
  - 기본값: 30초
  - 구성 가능: 10~120초
  - Timeout → Connection refused error
- **Priority**: M

#### FR-DCM-405: Transfer Syntax 협상

- **Description**: 지원되는 Transfer Syntax 협상
- **Acceptance Criteria**:
  - Implicit VR LE, Explicit VR LE 지원
  - JPEG 2000 Lossless (선택)
  - 협상 실패 → Association 거부
- **Priority**: M

#### FR-DCM-406: 전송 실패 처리

- **Description**: Network failure 재시도
- **Acceptance Criteria**:
  - 최대 3회 재시도
  - Exponential backoff: 2초, 4초, 8초
  - 최종 실패 후 에러 반환
- **Priority**: M

#### FR-DCM-407: C-STORE Status 반환

- **Description**: PACS로부터 Status 수신 및 반환
- **Acceptance Criteria**:
  - Success: 0x0000
  - Warning: 0x0122 (Out of Resources)
  - Failure: 0x0124 (SOP Class not supported)
  - Caller에게 Status 전달
- **Priority**: M

#### FR-DCM-408: TLS 1.2+ (선택)

- **Description**: 안전한 DICOM 통신 (TLS)
- **Acceptance Criteria**:
  - TLS 1.2 이상 지원
  - Mutual authentication (클라이언트 + 서버)
  - Certificate validation: CN 검증
  - 구성 가능: enable/disable
- **Priority**: O

#### FR-DCM-409: C-STORE 성능

- **Description**: 이미지 전송 시간 요구사항
- **Acceptance Criteria**:
  - 3072×3072 이미지: ≤ 10초 (1 Gbps LAN)
  - WAN 환경 고려: ≤ 30초 (10 Mbps)
- **Priority**: M

#### FR-DCM-410: C-FIND SCU (MWL)

- **Description**: Modality Worklist 쿼리 (RIS)
- **Acceptance Criteria**:
  - C-FIND SCU 구현
  - MWL SOP Class 쿼리
  - DICOM protocol 준수
- **Priority**: M

#### FR-DCM-411: MWL 쿼리 키

- **Description**: Worklist 쿼리 검색 조건
- **Acceptance Criteria**:
  - 쿼리 키: Patient ID, Accession Number, Scheduled Date, Modality
  - 입력 검증: 최소 1개 키 필요
  - Wildcard 지원 (선택): "*", "PAT*"
- **Priority**: M

#### FR-DCM-412: MWL 반환 정보

- **Description**: RIS로부터 Worklist 정보 수신
- **Acceptance Criteria**:
  - 반환 키: PatientName, PatientID, DOB, AccessionNumber, ScheduledProtocolCode
  - 최대 100개 결과
  - 응답 타임아웃: 30초
- **Priority**: M

#### FR-DCM-413: C-FIND Status 반환

- **Description**: MWL 쿼리 Status 반환
- **Acceptance Criteria**:
  - Success: 0x0000
  - Warning: 0x0122
  - Failure: 0x0124
  - 결과 개수 포함
- **Priority**: M

---

## 3. 안전 요구사항 (SR)

#### SR-DCM-001: Lossy 압축 금지

- **Requirement**: 진단 이미지에 손실 압축 금지
- **Verification Method**:
  - J2K Irreversible mode 감지 → 거부
  - 반환 코드: XPE_ERR_LOSSY_COMPRESSION_NOT_ALLOWED
- **Evidence Required**: 단위 테스트 (손실 압축 거부), 통합 테스트 (Lossless 만 통과)

#### SR-DCM-002: 환자 ID 검증

- **Requirement**: 메타데이터 환자 ID 일치성 확인
- **Verification Method**:
  - DICOM 파일 읽기 시 Patient ID 추출
  - XpeImage 구조체 Patient ID와 비교
  - 불일치 → XPE_ERR_PATIENT_ID_MISMATCH
- **Evidence Required**: 단위 테스트 (ID 불일치 케이스)

#### SR-DCM-003: DICOM 파일 무결성

- **Requirement**: 손상된 파일 감지 및 거부
- **Verification Method**:
  - DICOM preamble 검증
  - VR/길이 필드 일관성 확인
  - 손상 감지 → XPE_ERR_DICOM_CORRUPTED
- **Evidence Required**: 단위 테스트 (손상된 파일 입력)

#### SR-DCM-004: 네트워크 장애 처리

- **Requirement**: 전송 실패 시 로컬 파일 보호
- **Verification Method**:
  - C-STORE 실패 후 로컬 파일 정상 유지 확인
  - 재시도 로직 검증
  - 최종 실패 후 error 반환
- **Evidence Required**: 통합 테스트 (네트워크 끊김 시뮬레이션)

#### SR-DCM-005: MWL 환자 검증

- **Requirement**: 잘못된 환자 선택 방지
- **Verification Method**:
  - MWL 쿼리 결과 Patient ID 검증
  - 일치하지 않으면 alert 발행
- **Evidence Required**: 통합 테스트 (RIS 쿼리)

#### SR-DCM-006: GSPS 참조 무결성

- **Requirement**: GSPS와 원본 이미지 링크 검증
- **Verification Method**:
  - Referenced Series UID 확인
  - Graphic annotation bounds 검증
- **Evidence Required**: 단위 테스트 (GSPS 읽기)

#### SR-DCM-007: Unsupported Transfer Syntax 감지

- **Requirement**: 지원되지 않는 형식 즉시 거부
- **Verification Method**:
  - Transfer Syntax UID 확인
  - 지원 목록에 없으면 거부
  - 반환: XPE_ERR_DICOM_UNSUPPORTED_TRANSFER_SYNTAX
- **Evidence Required**: 단위 테스트 (MPEG-2, RLE 등 지원되지 않는 TS)

#### SR-DCM-008: 감사 로깅

- **Requirement**: 모든 DICOM I/O 작업 기록
- **Verification Method**:
  - 로그 항목에 파일 경로, Patient ID, Status 포함
  - 민감 정보 마스킹 (Patient Name 부분 마스킹)
- **Evidence Required**: 로그 리뷰

---

## 4. 성능 요구사항 (PR)

#### PR-DCM-001: DICOM 파일 읽기 시간

- **Requirement**: 3072×3072 uint16 이미지 읽기 ≤ 2초
- **Acceptance Criteria**: 벤치마크 환경에서 95th percentile ≤ 2.0초
- **Test Method**: Timing instrumentation, 반복 실행 (n=100)

#### PR-DCM-002: DICOM 파일 쓰기 시간

- **Requirement**: 3072×3072 uint16 이미지 쓰기 (비압축) ≤ 2초
- **Acceptance Criteria**: Explicit VR LE, 95th percentile ≤ 2.0초

#### PR-DCM-003: JPEG 2000 인코딩 시간

- **Requirement**: 3072×3072 이미지 J2K Lossless 인코딩 ≤ 5초
- **Acceptance Criteria**: OpenJPEG 성능, 95th percentile ≤ 5.0초

#### PR-DCM-004: C-STORE 전송 시간

- **Requirement**: 3072×3072 이미지 PACS 전송 ≤ 10초 (1 Gbps)
- **Acceptance Criteria**: LAN 환경, 1개 이미지 기준

#### PR-DCM-005: C-FIND 응답 시간

- **Requirement**: MWL 쿼리 응답 ≤ 5초
- **Acceptance Criteria**: 10개 결과 반환 기준

#### PR-DCM-006: 메모리 사용량 (읽기)

- **Requirement**: 3072×3072 uint16 DICOM 읽기 메모리 ≤ 150 MB
- **Acceptance Criteria**: Peak memory usage 측정

#### PR-DCM-007: 메모리 사용량 (쓰기)

- **Requirement**: 3072×3072 uint16 DICOM 쓰기 메모리 ≤ 100 MB
- **Acceptance Criteria**: Peak memory 측정

#### PR-DCM-008: 파일 크기

- **Requirement**: 
  - Explicit VR LE: ~18.9 MB (3072×3072 uint16)
  - JPEG 2000 Lossless: ~8-12 MB (50~60% 압축)
- **Acceptance Criteria**: 실제 파일 크기 측정

---

## 5. 인터페이스 요구사항 (IR)

### 5.1 C ABI 함수 인터페이스

#### IR-DCM-001: xpe_dicom_read()

```c
int xpe_dicom_read(
    const char *file_path,
    int pixel_format,  // XPE_PIXEL_UINT16 또는 XPE_PIXEL_FLOAT32
    XpeImage *out_image
);
```

- **Return**: XPE_OK 또는 에러 코드
- **Output**: out_image에 메타데이터 + 픽셀 데이터 채워짐

#### IR-DCM-002: xpe_dicom_write()

```c
int xpe_dicom_write(
    const char *output_path,
    const XpeImage *image,
    int transfer_syntax  // XPE_TS_IMPLICIT_VR_LE 등
);
```

- **Return**: XPE_OK 또는 에러 코드

#### IR-DCM-003: xpe_dicom_cstore()

```c
int xpe_dicom_cstore(
    const char *pacs_hostname,
    int pacs_port,
    const char *pacs_ae_title,
    const char *dicom_file_path
);
```

- **Return**: DICOM C-STORE status code

#### IR-DCM-004: xpe_dicom_cfind_mwl()

```c
int xpe_dicom_cfind_mwl(
    const char *ris_hostname,
    int ris_port,
    const char *ris_ae_title,
    const XpeMwlQuery *query,
    XpeMwlResult **out_results,
    int *out_count
);
```

- **Return**: XPE_OK 또는 에러 코드
- **Output**: out_results, out_count 채워짐

#### IR-DCM-005: xpe_gsps_create()

```c
int xpe_gsps_create(
    const XpeImage *primary_image,
    const XpeWindowLevel *window_level,
    XpeImage *out_gsps
);
```

- **Return**: XPE_OK 또는 에러 코드

---

### 5.2 에러 코드

| 에러 코드 | 설명 |
|----------|------|
| `XPE_OK` | 성공 |
| `XPE_ERR_FILE_NOT_FOUND` | 파일 없음 |
| `XPE_ERR_FILE_READ_FAILED` | 파일 읽기 실패 |
| `XPE_ERR_FILE_WRITE_FAILED` | 파일 쓰기 실패 |
| `XPE_ERR_DICOM_INVALID` | DICOM 형식 오류 |
| `XPE_ERR_DICOM_CORRUPTED` | DICOM 파일 손상 |
| `XPE_ERR_DICOM_UNSUPPORTED_TRANSFER_SYNTAX` | 지원되지 않는 Transfer Syntax |
| `XPE_ERR_DICOM_INVALID_IOD` | IOD 검증 실패 |
| `XPE_ERR_DICOM_INVALID_DIMENSION` | 이미지 크기 범위 초과 |
| `XPE_ERR_DICOM_MISSING_REQUIRED_TAG` | 필수 태그 누락 |
| `XPE_ERR_DICOM_INVALID_TAG` | 태그 값 오류 |
| `XPE_ERR_LOSSY_COMPRESSION_NOT_ALLOWED` | 손실 압축 거부 (CRITICAL) |
| `XPE_ERR_PATIENT_ID_MISMATCH` | 환자 ID 불일치 |
| `XPE_ERR_OUT_OF_MEMORY` | 메모리 부족 |
| `XPE_ERR_DISK_FULL` | 디스크 공간 부족 |
| `XPE_ERR_PERMISSION_DENIED` | 파일 접근 권한 거부 |
| `XPE_ERR_NETWORK_FAILURE` | 네트워크 오류 |

---

## 6. 추적성

### 6.1 PRD → SRS 매핑

| PRD 절 | SRS 요구사항 |
|--------|------------|
| §3.1 (DicomReader) | FR-DCM-101~120 |
| §3.2 (DicomWriter) | FR-DCM-201~217 |
| §3.3 (PresentationStateIO) | FR-DCM-301~306 |
| §3.4 (DicomNetworkSCU) | FR-DCM-401~413 |
| §7 (Safety) | SR-DCM-001~008 |
| §8 (Performance) | PR-DCM-001~008 |

---

**문서 끝: SRS-DICOM-001 v1.0.0**
