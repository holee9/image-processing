# X-ray DICOM I/O 및 네트워크 모듈

**모듈**: `xpe_dicom.dll` (Layer 1, Phase 1b)  
**안전 등급**: IEC 62304 Class B  
**문서 버전**: 1.0.0  
**날짜**: 2026-04-14  

---

## 빠른 참조

이 README는 6개의 상호 연관된 DICOM 모듈 문서 중 하나입니다. 역할에 따라 바로 이동하세요:

| 역할 | 읽어야 할 문서 | 목적 |
|------|--------------|------|
| **소프트웨어 개발자** | 이 README → API 레퍼런스 → SAD-DICOM-001 | 모듈 구조, 인터페이스, 내부 설계 이해 |
| **QA / 테스트 엔지니어** | RTM-DICOM-001 → SRS-DICOM-001 | 요구사항, 테스트 케이스, 추적성 |
| **안전/위험 담당자** | SHA-DICOM-001 → RTM-DICOM-001 | 위험 식별, 통제, 잔존 위험 평가 |
| **의료기기 규제 담당자** | xpe-dicom-prd.md → SRS-DICOM-001 → RTM-DICOM-001 | IEC 62304 추적성 패키지 |

---

## 문서 생태계 구조

```
┌──────────────────────────────────────────────────────────────────┐
│          DICOM I/O 모듈 문서 패키지 (v1.0.0)                    │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │   xpe-dicom-prd.md  (PRD)                                 │ │
│  │   제품 요구사항 원본 · SWU 명세 · SOUP 의존성             │ │
│  └───────────┬──────────────────────────────────────────────┘ │
│              │ 파생                                            │
│    ┌─────────┼──────────────────────────────────┐             │
│    │         │                                  │             │
│    v         v                                  v             │
│  ┌──────────────────┐  ┌──────────────┐  ┌──────────────────┐│
│  │SRS-DICOM-001     │  │SAD-DICOM-001 │  │SHA-DICOM-001     ││
│  │소프트웨어       │  │소프트웨어    │  │소프트웨어        ││
│  │요건 명세서      │  │아키텍처 문서 │  │위험 분석         ││
│  │(67 요구사항)    │  │(설계)        │  │(8개 위험)        ││
│  └─────┬──────────┘  └────┬──────────┘  └────┬──────────────┘│
│        │                  │                   │              │
│        └──────────────────┼───────────────────┘              │
│                           │ 추적성                           │
│                           v                                  │
│                 ┌──────────────────────┐                    │
│                 │ RTM-DICOM-001        │                    │
│                 │ 요구사항 추적성 행렬 │                    │
│                 │ (PRD↔SRS↔SAD↔Test)  │                    │
│                 └─────────────────────┘                     │
│                                                              │
│  ▶ 이 파일 (README.md) = 모듈 기술 개요                    │
└──────────────────────────────────────────────────────────────────┘
```

---

## 목차

1. [개요](#1-개요)
2. [핵심 특성](#2-핵심-특성)
3. [소프트웨어 단위 (4개 SWU)](#3-소프트웨어-단위-4개-swu)
4. [DICOM 객체 모델](#4-dicom-객체-모델)
5. [데이터 파이프라인](#5-데이터-파이프라인)
6. [네트워크 아키텍처](#6-네트워크-아키텍처)
7. [API 레퍼런스 (간단)](#7-api-레퍼런스-간단)
8. [주요 안전 기능](#8-주요-안전-기능)
9. [성능 목표](#9-성능-목표)
10. [SOUP 의존성](#10-soup-의존성)
11. [문서 인덱스](#11-문서-인덱스)

---

## 1. 개요

`xpe_dicom.dll`은 X-ray FPD (Flat Panel Detector) 이미지 처리 엔진의 **DICOM I/O 및 네트워킹 계층**입니다.

### 주요 책임

| 기능 | 설명 |
|------|------|
| **DICOM 파일 읽기** | DX, CR, GSPS IOD 파싱, 메타데이터 추출, 픽셀 디코딩|
| **DICOM 파일 쓰기** | DX IOD 생성, JPEG 2000 Lossless 압축, 메타데이터 인코딩|
| **Presentation State** | GSPS 생성, Window/Level 프리셋 저장 및 적용|
| **DICOM 네트워크** | C-STORE SCU (PACS 전송), C-FIND SCU (MWL 쿼리)|

### 설계 원칙

- **안전 우선**: 손실 압축 자동 거부, 환자 ID 검증, 파일 무결성 확인
- **상호운용성**: DCMTK 표준 라이브러리, DICOM 표준 준수
- **신뢰성**: 네트워크 재시도, 타임아웃 관리, 명확한 에러 보고
- **성능**: 3072×3072 이미지 2초 이내 I/O

---

## 2. 핵심 특성

### 2.1 DICOM IOD 지원

```
DX (Digital Radiography)
└─ SOP Class UID: 1.2.840.10008.5.1.4.1.1.1.1
└─ 지원: 읽기, 쓰기, 메타데이터

CR (Computed Radiography)
└─ SOP Class UID: 1.2.840.10008.5.1.4.1.1.2
└─ 지원: 읽기

GSPS (Grayscale Softcopy Presentation State)
└─ SOP Class UID: 1.2.840.10008.5.1.4.1.1.11.1
└─ 지원: 생성, 읽기, 참조 검증
```

### 2.2 Transfer Syntax 지원 매트릭스

| Transfer Syntax | UID | 읽기 | 쓰기 | 참고 |
|-----------------|-----|:----:|:----:|------|
| Implicit VR LE | 1.2.840.10008.1.2 | ✓ | ✓ | 기본값 |
| Explicit VR LE | 1.2.840.10008.1.2.1 | ✓ | ✓ | 권장 |
| JPEG 2000 Lossless | 1.2.840.10008.1.2.4.90 | ✓ | ✓ | 압축 (진단급) |
| JPEG 2000 Irreversible | 1.2.840.10008.1.2.4.92 | ✗ | **✗ REJECTED** | 손실 (금지) |
| JPEG Baseline | 1.2.840.10008.1.2.4.50 | ✓ | ✗ | Legacy CR |
| RLE Lossless | 1.2.840.10008.1.2.5 | ✗ | ✗ | 미지원 |

### 2.3 포토메트릭 처리

| 인터프리테이션 | 입력 처리 | 출력 | 설명 |
|-------------|---------|------|------|
| MONOCHROME1 | 반전 (MAX - pixel) | MONOCHROME2 | 작은 값 = 밝음 |
| MONOCHROME2 | 그대로 사용 | MONOCHROME2 | 작은 값 = 어두움 |

---

## 3. 소프트웨어 단위 (4개 SWU)

### 3.1 SWU-4.1: DicomReader

**책임**: DICOM 파일 읽기 및 메타데이터 추출

```
DICOM File (disk)
    ↓
[Validation: Preamble, IOD, Format]
    ↓
[Parsing: DCMTK library]
    ↓
[Metadata: Patient, Study, Equipment]
    ↓
[Pixel Decoding: uint16 → float32]
    ↓
[Photometric Correction: MONOCHROME1 → MONOCHROME2]
    ↓
XpeImage (memory)
```

**주요 함수**:
- `xpe_dicom_read()`: 파일 읽기, 메타데이터 추출
- `xpe_dicom_query_dimensions()`: 이미지 크기 조회
- `xpe_dicom_read_tag_string()`: 특정 태그 값 읽기

**에러 처리**:
- `XPE_ERR_FILE_NOT_FOUND`: 파일 없음
- `XPE_ERR_DICOM_INVALID`: DICOM 형식 오류
- `XPE_ERR_DICOM_CORRUPTED`: 파일 손상
- `XPE_ERR_DICOM_UNSUPPORTED_TRANSFER_SYNTAX`: 미지원 TS

---

### 3.2 SWU-4.2: DicomWriter

**책임**: XpeImage를 DICOM 파일로 인코딩 및 저장

```
XpeImage (memory)
    ↓
[Validation: Metadata, Transfer Syntax, Lossy Check]
    ↓
[DX IOD Creation: Structure + UID generation]
    ↓
[Metadata Encoding: Patient, Study, Image Pixel]
    ↓
[Pixel Encoding: uint16 → DICOM binary]
    ↓
[Compression: Implicit VR LE 또는 J2K Lossless]
    ↓
[File I/O: Write + Verify]
    ↓
DICOM File (disk)
```

**CRITICAL 기능: Lossy 압축 금지**

```cpp
// 손실 JPEG 2000 (Irreversible mode) 자동 거부
IF Transfer Syntax == JPEG 2000 Irreversible:
    return XPE_ERR_LOSSY_COMPRESSION_NOT_ALLOWED
    // 이 검사는 bypass 불가능 (hard constraint)
```

**주요 함수**:
- `xpe_dicom_write()`: 파일 쓰기
- `xpe_dicom_write_j2k()`: JPEG 2000 Lossless 쓰기

---

### 3.3 SWU-4.3: PresentationStateIO

**책임**: GSPS (Presentation State) 생성 및 적용

#### GSPS 역할

GSPS는 원본 DX 이미지와 별도로 Window/Level 프리셋, ROI 주석, 회전/반전 정보를 저장합니다.

```
Primary DX Image (SOP Instance A)
    ↓
[GSPS Creation]
    ├─ Referenced SOP Instance A
    ├─ Window/Level Presets (3개)
    │   ├─ Soft Tissue (40, 400)
    │   ├─ Bone (300, 1500)
    │   └─ Lung (600, 2000)
    ├─ Graphic Annotations (ROI, optional)
    └─ Display Shutter (rotation/flip, optional)
    ↓
GSPS File (SOP Instance B)
```

**주요 함수**:
- `xpe_gsps_create()`: GSPS 생성
- `xpe_gsps_apply()`: GSPS 설정 적용

**안전성**:
- Referenced Series UID 저장 및 검증
- Window/Level 범위 검증
- ROI 바운드 검증 (이미지 영역 내)

---

### 3.4 SWU-4.4: DicomNetworkSCU

**책임**: DICOM 네트워크 통신 (Service Class User)

#### C-STORE SCU (이미지 전송 → PACS)

```
Local Application
    ↓
[DICOM Association Request]
    ├─ Local AE Title: "xpe_dicom"
    ├─ Remote AE Title: PACS (configurable)
    └─ Timeout: 30초
    ↓
[C-STORE Command]
    ├─ Transfer Syntax negotiation
    ├─ Image transmission
    └─ Status response
    ↓
PACS (Remote Storage)
```

**재시도 정책**:
- 자동 재시도: 최대 3회
- Exponential backoff: 2초, 4초, 8초
- 최종 실패 후 명확한 에러 코드 반환

#### C-FIND SCU (MWL 쿼리 → RIS)

```
Local Application
    ↓
[C-FIND Query]
    ├─ Patient ID
    ├─ Accession Number
    ├─ Scheduled Date
    └─ Modality = "DX"
    ↓
RIS (Modality Worklist)
    ↓
[C-FIND Results]
    ├─ Patient Name
    ├─ Patient ID
    ├─ DOB
    └─ Scheduled Protocol Code
    ↓
Local Application
```

**쿼리 검증**:
- 반환된 Patient ID ≠ 쿼리 키 → Alert (환자 혼동 방지)
- 최대 100개 결과
- Wildcard 지원 (선택)

---

## 4. DICOM 객체 모델

### 4.1 DX IOD 계층

```
DX Image (SOP Class: 1.2.840.10008.5.1.4.1.1.1.1)
├─ Patient Module
│  ├─ Patient ID (0010,0020) [M]
│  ├─ Patient Name (0010,0010) [M]
│  └─ Patient Birth Date (0010,0030) [O]
│
├─ Study Module
│  ├─ Study Instance UID (0020,000D) [M]
│  ├─ Study Date (0008,0020) [M]
│  └─ Study Time (0008,0030) [M]
│
├─ Series Module
│  ├─ Series Instance UID (0020,000E) [M]
│  ├─ Series Date (0008,0021) [M]
│  ├─ Modality (0008,0060) = "DX" [M]
│  └─ Series Number (0020,0011) [O]
│
├─ Image Pixel Module
│  ├─ Rows (0028,0010) [M] — 이미지 높이
│  ├─ Columns (0028,0011) [M] — 이미지 너비
│  ├─ Pixel Spacing (0028,0030) [M] — [mm/pixel]
│  ├─ Bits Allocated (0028,0100) = 16 [M]
│  ├─ Bits Stored (0028,0101) = 14 [M]
│  ├─ High Bit (0028,0102) = 13 [M]
│  ├─ Pixel Representation (0028,0103) = 0 [M] (unsigned)
│  └─ Pixel Data (7FE0,0010) [M]
│
├─ General Equipment Module
│  ├─ Manufacturer (0008,0070) [M]
│  ├─ Manufacturer Model Name (0008,1090) [O]
│  └─ Device Serial Number (0018,1000) [M]
│
├─ VOI LUT Module
│  ├─ Window Center (0028,1050) [M]
│  └─ Window Width (0028,1051) [M]
│
└─ XPE Private Block (optional)
   ├─ Private Creator (0019,0010) = "XPE"
   ├─ Processing Flags (0019,1001)
   ├─ XPE Version (0019,1002)
   └─ Calibration Date (0019,1003)

[M] = Mandatory (필수)
[O] = Optional (선택)
```

### 4.2 GSPS IOD (Presentation State)

```
GSPS Document (SOP Class: 1.2.840.10008.5.1.4.1.1.11.1)
├─ Referenced DX SOP Instance UID
├─ Referenced Series UID
│
├─ Window/Level Presets
│  ├─ Preset 1: (Center=40, Width=400) — Soft Tissue
│  ├─ Preset 2: (Center=300, Width=1500) — Bone
│  └─ Preset 3: (Center=600, Width=2000) — Lung
│
├─ Graphic Annotations (optional)
│  └─ Collimation ROI: polyline coordinates
│
└─ Display Shutter (optional)
   ├─ Rotation: 0°, 90°, 180°, 270°
   └─ Flip: Horizontal / Vertical
```

---

## 5. 데이터 파이프라인

### 5.1 읽기 파이프라인 (Read Pipeline)

```
DICOM File (disk)
    ↓
xpe_dicom_read(file_path, UINT16)
    ├─ File validation (exists, readable)
    ├─ DICOM preamble check ("DICM")
    ├─ Header parsing (DCMTK)
    ├─ Transfer Syntax detection
    ├─ IOD validation (DX, CR, GSPS)
    ├─ Metadata extraction:
    │   ├─ Patient: ID, Name, DOB
    │   ├─ Study: Date, UID
    │   ├─ Series: Date, UID, Modality
    │   ├─ Image: Rows, Columns, PixelSpacing
    │   ├─ Equipment: Manufacturer, Serial
    │   ├─ VOI: WindowCenter, WindowWidth
    │   └─ XPE: Flags (optional)
    ├─ Pixel data extraction:
    │   ├─ IF Implicit/Explicit VR LE: copy uint16
    │   ├─ IF JPEG 2000 Lossless: OpenJPEG decode
    │   └─ IF JPEG Baseline: DCMTK decode
    ├─ Format conversion (if requested):
    │   └─ IF pixel_format == FLOAT32: normalize [0.0 ~ 65535.0]
    ├─ Photometric correction:
    │   ├─ IF MONOCHROME1: invert (MAX - pixel)
    │   └─ IF MONOCHROME2: use as-is
    ├─ Populate XpeImage
    └─ Return XPE_OK
    ↓
XpeImage (memory)
    ├─ pixel_data: uint16 array
    ├─ width, height: int
    ├─ metadata: Patient, Study, Equipment
    └─ flags: XPE_FLAG_* bitmask
```

### 5.2 쓰기 파이프라인 (Write Pipeline)

```
XpeImage (memory)
    ↓
xpe_dicom_write(path, image, TS_IMPLICIT_VR_LE)
    ├─ Input validation (metadata complete)
    ├─ CRITICAL: Lossy compression check
    │   └─ IF Transfer Syntax == JPEG 2000 Irreversible:
    │       REJECT with XPE_ERR_LOSSY_COMPRESSION_NOT_ALLOWED
    ├─ DX IOD creation:
    │   ├─ SOP Class UID = 1.2.840.10008.5.1.4.1.1.1.1
    │   ├─ Generate SOP Instance UID (new)
    │   └─ Create DICOM file structure
    ├─ Metadata encoding:
    │   ├─ Patient: ID, Name
    │   ├─ Study: Date, UID
    │   ├─ Series: Date, UID, Modality=DX
    │   ├─ Image: Rows, Columns, PixelSpacing, Bits
    │   ├─ Equipment: Manufacturer, Serial
    │   ├─ VOI: WindowCenter, WindowWidth
    │   └─ XPE private block (optional)
    ├─ Pixel data encoding:
    │   ├─ IF Implicit/Explicit VR LE: encode uint16 directly
    │   └─ IF JPEG 2000 Lossless: OpenJPEG encode
    ├─ Transfer Syntax setup
    ├─ File I/O:
    │   ├─ Check disk space
    │   ├─ Write to temporary file
    │   ├─ Verify written data (hash)
    │   └─ Rename to final path
    └─ Return XPE_OK
    ↓
DICOM File (disk)
```

---

## 6. 네트워크 아키텍처

### 6.1 Association 상태 머신

```
IDLE
  │
  ├─[Request]────→ ASSOCIATION_REQUESTED
  │                    │
  │                    ├─[Accept]─→ ASSOCIATION_ESTABLISHED
  │                    │                 │
  │                    │                 ├─[C-STORE]─→ SENDING_IMAGE
  │                    │                 │               │
  │                    │                 │               └─[Complete]─→ ASSOCIATION_ESTABLISHED
  │                    │                 │
  │                    │                 ├─[C-FIND]─→ QUERYING_MWL
  │                    │                 │               │
  │                    │                 │               └─[Complete]─→ ASSOCIATION_ESTABLISHED
  │                    │                 │
  │                    │                 └─[Release]─→ CLOSED
  │                    │
  │                    └─[Reject]─→ ERROR
  │
  └─[Error]──────────→ CLOSED
```

### 6.2 C-STORE SCU 재시도 정책

```
Attempt 1 (즉시)
    │ 실패
    v
Wait 2초
    │
Attempt 2
    │ 실패
    v
Wait 4초
    │
Attempt 3
    │ 실패
    v
최종 실패 → Error 반환
```

### 6.3 네트워크 타임아웃 설정

| 타임아웃 | 기본값 | 구성 가능 | 용도 |
|---------|:-----:|:-------:|------|
| Association | 30초 | Yes | PACS/RIS 응답 대기 |
| DIMSE | 120초 | Yes | C-STORE/C-FIND 완료 |
| Inactivity | 300초 | Yes | 비활성 연결 종료 |

---

## 7. API 레퍼런스 (간단)

### 7.1 DICOM Read

```c
int xpe_dicom_read(
    const char *file_path,           // DICOM file path
    int pixel_format,                // XPE_PIXEL_UINT16 or FLOAT32
    XpeImage *out_image              // Output image (caller allocates)
);
```

**반환값**: `XPE_OK` or error code

**예시**:
```c
XpeImage image = {};
int status = xpe_dicom_read("image.dcm", XPE_PIXEL_UINT16, &image);
if (status != XPE_OK) {
    printf("Error: %d\n", status);
}
```

### 7.2 DICOM Write

```c
int xpe_dicom_write(
    const char *output_path,         // Output file path
    const XpeImage *image,           // Input image
    int transfer_syntax              // XPE_TS_IMPLICIT_VR_LE, etc.
);
```

**반환값**: `XPE_OK` or error code

**예시**:
```c
int status = xpe_dicom_write(
    "output.dcm",
    &image,
    XPE_TS_IMPLICIT_VR_LE  // Explicit VR LE로 쓰기
);
```

### 7.3 C-STORE (PACS 전송)

```c
int xpe_dicom_cstore(
    const char *pacs_hostname,       // PACS hostname/IP
    int pacs_port,                   // Port (default 104)
    const char *pacs_ae_title,       // PACS AE Title
    const char *dicom_file_path      // File to send
);
```

**반환값**: C-STORE status code (0x0000=Success, 0x0124=Failure)

### 7.4 C-FIND (MWL 쿼리)

```c
typedef struct {
    char patient_id[64];             // 필수
    char accession_number[32];       // 선택
    char modality[8];                // "DX"
} XpeMwlQuery;

typedef struct {
    char patient_name[256];
    char patient_id[64];
    char dob[16];
    char accession_number[32];
} XpeMwlResult;

int xpe_dicom_cfind_mwl(
    const char *ris_hostname,
    int ris_port,
    const char *ris_ae_title,
    const XpeMwlQuery *query,
    XpeMwlResult *out_results,      // Pre-allocated [100]
    int *out_count
);
```

---

## 8. 주요 안전 기능

### 8.1 Lossy 압축 거부 (CRITICAL)

```
시나리오: User가 JPEG 2000 Irreversible (손실) 압축 시도
    ↓
xpe_dicom_write() 호출 시 TS = JPEG 2000 Irreversible
    ↓
[자동 검증]
    IF Transfer Syntax == JPEG 2000 Irreversible:
        RETURN XPE_ERR_LOSSY_COMPRESSION_NOT_ALLOWED
        LOG: CRITICAL "Lossy J2K compression rejected"
    ↓
진단 이미지 손상 방지 ✓
```

**설계 특성:**
- Hard constraint (bypass 불가능)
- 다중 검증 층 (호출자 + 내부)
- 명확한 에러 메시지
- CRITICAL 로그 기록

### 8.2 환자 ID 검증

```
시나리오: 메타데이터와 DICOM 파일의 Patient ID 불일치
    ↓
xpe_dicom_read() / xpe_dicom_write()
    ├─ Extract Patient ID from DICOM file
    ├─ Compare with XpeImage.metadata.patient_id
    ├─ IF mismatch:
    │   RETURN XPE_ERR_PATIENT_ID_MISMATCH
    │   LOG: ALERT "Patient ID mismatch: 12345 vs 12346"
    └─ Prevent wrong patient image processing ✓
```

### 8.3 DICOM 파일 무결성

```
시나리오: DICOM 파일 손상 (네트워크 전송, 디스크 섹터)
    ↓
xpe_dicom_read()
    ├─ Validate "DICM" preamble
    ├─ Validate VR length fields
    ├─ Validate IOD structure
    ├─ IF corrupted:
    │   RETURN XPE_ERR_DICOM_CORRUPTED
    │   DO NOT return partial pixel data
    └─ Prevent downstream processing of corrupted image ✓
```

### 8.4 네트워크 안전성

```
시나리오: PACS로의 C-STORE 전송 중 네트워크 끊김
    ↓
xpe_dicom_cstore()
    ├─ Attempt 1: Failed
    ├─ Wait 2 seconds
    ├─ Attempt 2: Failed
    ├─ Wait 4 seconds
    ├─ Attempt 3: Failed
    ├─ Return: C-STORE status = 0x0124 (Failure)
    ├─ Local DICOM file: preserved (PACS 전송 실패해도 안전)
    └─ Allow user to retry manually ✓
```

---

## 9. 성능 목표

### 9.1 시간 성능

| 작업 | 목표 (ms) | 조건 |
|------|:--------:|------|
| DICOM 읽기 | ≤ 2,000 | 3072×3072 uint16 |
| DICOM 쓰기 (비압축) | ≤ 2,000 | Explicit VR LE |
| JPEG 2000 인코딩 | ≤ 5,000 | Lossless |
| C-STORE (LAN) | ≤ 10,000 | 1 Gbps, 3072×3072 |
| C-FIND 쿼리 | ≤ 5,000 | 10개 결과 |

### 9.2 메모리 사용량

| 작업 | 메모리 |
|------|:------:|
| DICOM 읽기 | ≤ 150 MB |
| DICOM 쓰기 | ≤ 100 MB |
| DCMTK 버퍼 | ~10 MB |
| 메타데이터 | ~1 MB |

### 9.3 저장소

| 파일 타입 | 크기 | 압축률 |
|----------|:----:|------:|
| Raw DICOM (uint16) | 18.9 MB | 비압축 |
| JPEG 2000 Lossless | 8-12 MB | ~50% |
| GSPS | 50 KB | 매우 작음 |

---

## 10. SOUP 의존성

### 10.1 DCMTK (DICOM Toolkit)

| 항목 | 명세 |
|------|------|
| **라이센스** | BSD 3-Clause |
| **버전** | 3.6.8 (또는 최신) |
| **용도** | DICOM parsing, encoding, network |
| **상태** | IEC 62304 Class B 의료 기기 승인됨 |
| **문서** | https://github.com/DCMTK/dcmtk |

### 10.2 OpenJPEG

| 항목 | 명세 |
|------|------|
| **라이센스** | BSD 2-Clause |
| **버전** | 2.5.0 (또는 최신) |
| **용도** | JPEG 2000 Lossless codec |
| **상태** | 검증됨 |
| **문서** | https://github.com/uclouvain/openjpeg |

### 10.3 OpenSSL (선택 — TLS)

| 항목 | 명세 |
|------|------|
| **라이센스** | Apache 2.0 |
| **버전** | 1.1.1+ (TLS 1.2+) |
| **용도** | DICOM network TLS 보안 |
| **상태** | 선택 기능 |

---

## 11. 문서 인덱스

### 11.1 이 패키지의 6개 문서

| 문서 ID | 제목 | 용도 | 길이 |
|--------|------|------|:----:|
| **xpe-dicom-prd.md** | Product Requirements Document | 제품 요구사항 원본 | ~550행 |
| **SRS-DICOM-001** | Software Requirements Specification | 소프트웨어 요건 명세 (67 요구사항) | ~850행 |
| **SAD-DICOM-001** | Software Architecture Document | 설계, 알고리즘, 인터페이스 | ~800행 |
| **SHA-DICOM-001** | Software Hazard Analysis | 위험 분석 (8개 위험, ISO 14971) | ~500행 |
| **RTM-DICOM-001** | Requirements Traceability Matrix | 추적성 행렬 (PRD↔SRS↔SAD↔Test) | ~600행 |
| **README.md (이 파일)** | Module Overview | 기술 개요, 빠른 참조 | ~400행 |

**총 페이지**: ~4,100행 (IEC 62304 Class B 완전 패키지)

### 11.2 관련 문서 (외부)

| 문서 | 경로 | 설명 |
|------|------|------|
| SPEC-XPE-MASTER | `.moai/specs/` | 전체 XPE 시스템 마스터 플랜 |
| api-spec.md | `.moai/specs/` | 모든 XPE API 함수 정의 |
| XPE-SRS-001 | `docs/project/` | XPE 전체 시스템 요구사항 |

---

## 12. 빠른 시작 (개발자용)

### 12.1 DICOM 읽기

```cpp
#include "xpe_dicom_api.h"

// 1. DICOM 파일 읽기
XpeImage image = {};
int status = xpe_dicom_read("input.dcm", XPE_PIXEL_UINT16, &image);

if (status == XPE_OK) {
    printf("Image: %d x %d pixels\n", image.width, image.height);
    printf("Patient ID: %s\n", image.metadata.patient_id);
    // image.pixel_data 처리...
} else {
    printf("Error: %d\n", status);
}
```

### 12.2 DICOM 쓰기

```cpp
// 2. 처리 후 DICOM 쓰기
image.metadata.patient_id = "12345";
image.metadata.study_date = "20260414";
// ... set metadata ...

status = xpe_dicom_write("output.dcm", &image, XPE_TS_EXPLICIT_VR_LE);

if (status == XPE_OK) {
    printf("File written successfully\n");
} else if (status == XPE_ERR_LOSSY_COMPRESSION_NOT_ALLOWED) {
    printf("Lossy compression not allowed for diagnostic images\n");
} else {
    printf("Error: %d\n", status);
}
```

### 12.3 PACS 전송

```cpp
// 3. PACS로 이미지 전송
int c_store_status = xpe_dicom_cstore(
    "pacs.hospital.local",
    104,
    "PACS-001",
    "output.dcm"
);

if (c_store_status == 0x0000) {
    printf("Image sent to PACS successfully\n");
} else {
    printf("C-STORE failed: 0x%04X\n", c_store_status);
}
```

---

## 13. 지원 및 문의

**문제 발생 시:**

1. 에러 코드 확인: `xpe_error.h` 참고
2. 로그 검토: xpe_dicom 모듈 CRITICAL/ERROR 수준 로그
3. 해당 문서 참고:
   - 기술 설계: SAD-DICOM-001
   - 요구사항: SRS-DICOM-001
   - 안전: SHA-DICOM-001

**문서 작성자**: XPE 개발팀  
**최종 검토**: 2026-04-14  
**승인 대기**: 규제 담당자  

---

**문서 끝: README.md v1.0.0**
