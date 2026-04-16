# DICOM I/O 모듈 — 소프트웨어 아키텍처 문서 (SAD)

**문서 ID**: SAD-DICOM-001  
**버전**: 1.0.0  
**날짜**: 2026-04-14  
**IEC 62304 절**: 5.3.1 — 5.3.6 Software Design  
**안전 등급**: Class B  

---

## 목차

1. [아키텍처 개요](#1-아키텍처-개요)
2. [소프트웨어 단위 설계](#2-소프트웨어-단위-설계)
3. [컴포넌트 분해](#3-컴포넌트-분해)
4. [인터페이스 정의](#4-인터페이스-정의)
5. [데이터 흐름](#5-데이터-흐름)
6. [SOUP 통합](#6-soup-통합)
7. [에러 처리](#7-에러-처리)
8. [리스크 통제](#8-리스크-통제)

---

## 1. 아키텍처 개요

### 1.1 계층 구조

```
┌───────────────────────────────────────────────┐
│          ImageProcTest.exe (C#)               │
│     (DICOM Command Orchestrator)              │
└──────────────┬────────────────────────────────┘
               │ P/Invoke
               v
┌───────────────────────────────────────────────┐
│        xpe_dicom.dll (Layer 1)                │
│  ┌─────────────────────────────────────────┐  │
│  │ SWU-4.1: DicomReader                    │  │
│  │ (Read, Parse, Decode, Extract)          │  │
│  ├─────────────────────────────────────────┤  │
│  │ SWU-4.2: DicomWriter                    │  │
│  │ (Create, Encode, Compress, Write)       │  │
│  ├─────────────────────────────────────────┤  │
│  │ SWU-4.3: PresentationStateIO             │  │
│  │ (GSPS Create, Apply, Link)               │  │
│  ├─────────────────────────────────────────┤  │
│  │ SWU-4.4: DicomNetworkSCU                │  │
│  │ (C-STORE, C-FIND, Association Mgmt)     │  │
│  └─────────────────────────────────────────┘  │
└──────────────┬────────────────────────────────┘
               │ Link Dependency
               v
┌───────────────────────────────────────────────┐
│    xpe_common.dll (Layer 0)                   │
│  Types | Memory | Config | Logger | Alert    │
└──────────────┬────────────────────────────────┘
               │
               v
┌───────────────────────────────────────────────┐
│ SOUP: DCMTK 3.6.8 | OpenJPEG 2.5.0           │
│      OpenSSL 1.1.1+ (optional TLS)           │
└───────────────────────────────────────────────┘
```

### 1.2 패키지 다이어그램

```
xpe_dicom.dll

┌─────────────────────────────────────────────┐
│ Public API (xpe_dicom_api.h)                │
│ - xpe_dicom_read()                          │
│ - xpe_dicom_write()                         │
│ - xpe_dicom_cstore()                        │
│ - xpe_dicom_cfind_mwl()                     │
│ - xpe_gsps_create()                         │
└──────────────────┬──────────────────────────┘
                   │
        ┌──────────┴──────────┐
        │                     │
        v                     v
┌──────────────────┐  ┌──────────────────┐
│ Internal Reader  │  │ Internal Writer  │
│ (xpe_dicom_     │  │ (xpe_dicom_     │
│  reader.cpp)    │  │  writer.cpp)    │
└────────┬─────────┘  └────────┬─────────┘
         │                     │
         v                     v
┌───────────────────────────────────────┐
│ DCMTK Wrapper Layer                  │
│ (xpe_dcmtk.cpp)                      │
│ - dcmtkDataset handling              │
│ - Transfer Syntax negotiation        │
│ - Pixel data codec bridging          │
└───────────┬───────────────────────────┘
            │
            v
┌───────────────────────────────────────┐
│ SOUP: DCMTK, OpenJPEG, OpenSSL       │
└───────────────────────────────────────┘
```

---

## 2. 소프트웨어 단위 설계

### 2.1 SWU-4.1: DicomReader

**책임**: DICOM 파일 읽기, 메타데이터 추출, 픽셀 데이터 디코딩

#### 2.1.1 내부 구조

```cpp
// xpe_dicom_reader.cpp

namespace xpe::dicom::reader {

class DicomFileReader {
  public:
    // Main API
    int read(const char* file_path, int pixel_format, XpeImage* out_image);
    
  private:
    // DCMTK interface
    DcmFileFormat* dcm_file_;
    DcmDataset* dataset_;
    
    // Helper methods
    int validate_preamble_();
    int detect_transfer_syntax_();
    int extract_pixel_data_(int pixel_format);
    int extract_metadata_();
    int validate_iod_();
};

}  // namespace
```

#### 2.1.2 알고리즘

**DICOM Read 알고리즘:**

```
Input: file_path, pixel_format
Output: XpeImage (populated)

1. VALIDATE file exists
   IF NOT FOUND → return XPE_ERR_FILE_NOT_FOUND

2. OPEN file with DcmFileFormat
   IF OPEN FAILED → return XPE_ERR_FILE_READ_FAILED

3. READ DICOM preamble (128 bytes + "DICM")
   IF INVALID → return XPE_ERR_DICOM_CORRUPTED

4. PARSE DICOM dataset
   DcmDataset = DcmFileFormat.getDataset()
   IF PARSE FAILED → return XPE_ERR_DICOM_INVALID

5. DETECT Transfer Syntax
   ts_uid = dataset.getTransferSyntax()
   IF NOT SUPPORTED → return XPE_ERR_DICOM_UNSUPPORTED_TRANSFER_SYNTAX

6. IDENTIFY SOP Class UID
   sop_class = dataset.get(0x0008, 0x0016)
   IF NOT IN {DX, CR, GSPS} → return XPE_ERR_DICOM_INVALID_IOD

7. VALIDATE IOD structure
   check mandatory modules: Patient, Study, Image Pixel
   IF MISSING → return XPE_ERR_DICOM_MISSING_REQUIRED_TAG

8. EXTRACT metadata
   - Patient: ID, Name, DOB
   - Study: Date, Time
   - Series: Date
   - Image: Rows, Columns, PixelSpacing, WindowCenter/Width
   - Equipment: Manufacturer, SerialNumber
   - BodyPart: (0018,0015)
   - XPE private: (0019,xx00) [optional]

9. EXTRACT pixel data
   IF pixel_format == UINT16:
     extract raw uint16 pixels from dataset
   ELSE IF pixel_format == FLOAT32:
     decode to uint16, then convert to float32 [0.0~65535.0]

10. VALIDATE photometric interpretation
    photo = dataset.get(0x0028, 0x0004)
    IF MONOCHROME1:
      invert pixels: p_out = MAX_VALUE - p_in
    ELSE IF MONOCHROME2:
      use as-is
    ELSE:
      return XPE_ERR_DICOM_UNSUPPORTED_PHOTOMETRIC

11. POPULATE XpeImage
    image.width = Columns
    image.height = Rows
    image.pixel_data = [extracted data]
    image.metadata = [extracted tags]
    image.pixel_format = [output format]
    
12. RETURN XPE_OK
```

#### 2.1.3 Transfer Syntax 지원 매트릭스

| Transfer Syntax UID | 이름 | DCMTK 지원 | 상태 |
|-------------------|------|:--------:|--------|
| 1.2.840.10008.1.2 | Implicit VR LE | 예 | ✓ 지원 |
| 1.2.840.10008.1.2.1 | Explicit VR LE | 예 | ✓ 지원 |
| 1.2.840.10008.1.2.4.90 | JPEG 2000 Lossless | 예 | ✓ 지원 |
| 1.2.840.10008.1.2.4.50 | JPEG Baseline | 예 | ✓ 지원 (legacy) |
| 1.2.840.10008.1.2.4.91 | JPEG 2000 Irreversible | 예 | ✗ 거부 |
| 1.2.840.10008.1.2.5 | RLE Lossless | 예 | ✗ 미지원 (에러) |

---

### 2.2 SWU-4.2: DicomWriter

**책임**: XpeImage를 DICOM DX IOD로 인코딩하고 파일에 저장

#### 2.2.1 내부 구조

```cpp
// xpe_dicom_writer.cpp

namespace xpe::dicom::writer {

class DicomFileWriter {
  public:
    int write(
        const char* output_path,
        const XpeImage* image,
        int transfer_syntax
    );
    
  private:
    DcmFileFormat* dcm_file_;
    DcmDataset* dataset_;
    
    int create_dx_iod_();
    int populate_metadata_(const XpeImage* image);
    int encode_pixel_data_(const XpeImage* image, int ts);
    int validate_lossy_compression_(int ts);
    int generate_uids_();
};

}
```

#### 2.2.2 알고리즘

**DICOM Write 알고리즘:**

```
Input: output_path, XpeImage, transfer_syntax
Output: DICOM file on disk

1. VALIDATE input
   IF image NULL → return XPE_ERR_INVALID_ARGUMENT
   IF pixel_data NULL → return XPE_ERR_INVALID_ARGUMENT
   IF output_path empty → return XPE_ERR_INVALID_ARGUMENT

2. CHECK lossy compression (CRITICAL)
   IF transfer_syntax == JPEG 2000 Irreversible (1.2.840.10008.1.2.4.92):
     return XPE_ERR_LOSSY_COMPRESSION_NOT_ALLOWED
     ALERT: "Lossy compression not allowed for diagnostic images"
   [This check cannot be bypassed]

3. CREATE DX IOD structure
   dcm_file = new DcmFileFormat()
   dataset = dcm_file.getDataset()
   
   // SOP Class/Instance UIDs
   dataset.put(0x0008, 0x0016, "1.2.840.10008.5.1.4.1.1.1.1")  // DX SOP Class
   dataset.put(0x0008, 0x0018, generate_uid())  // SOP Instance (new)

4. POPULATE mandatory metadata
   // Patient Module
   dataset.put(0x0010, 0x0020, image.patient_id)  // Patient ID
   dataset.put(0x0010, 0x0010, image.patient_name)  // Patient Name
   
   // Study Module
   dataset.put(0x0008, 0x0020, image.study_date)  // Study Date
   dataset.put(0x0008, 0x0030, image.study_time)  // Study Time
   dataset.put(0x0020, 0x000D, image.study_uid)  // Study UID
   
   // Series Module
   dataset.put(0x0008, 0x0021, image.series_date)  // Series Date
   dataset.put(0x0020, 0x000E, image.series_uid)  // Series UID
   
   // General Equipment
   dataset.put(0x0008, 0x0070, "XPE System")  // Manufacturer
   dataset.put(0x0018, 0x1000, "XPE-1000")  // Device Serial Number
   
   // Image Pixel Module (dimensions)
   dataset.put(0x0028, 0x0010, image.height)  // Rows
   dataset.put(0x0028, 0x0011, image.width)  // Columns
   dataset.put(0x0028, 0x0030, {image.pixel_spacing_y, image.pixel_spacing_x})
   
   // Bits
   dataset.put(0x0028, 0x0100, 16)  // BitsAllocated
   dataset.put(0x0028, 0x0101, 14)  // BitsStored (14-bit for XPE)
   dataset.put(0x0028, 0x0102, 13)  // HighBit
   dataset.put(0x0028, 0x0103, 0)   // PixelRepresentation (unsigned)
   
   // Photometric
   dataset.put(0x0028, 0x0004, "MONOCHROME2")
   
   // VOI LUT (Window/Level)
   dataset.put(0x0028, 0x1050, image.window_center)
   dataset.put(0x0028, 0x1051, image.window_width)
   
   // Content Date/Time
   dataset.put(0x0008, 0x0023, <now_date>)
   dataset.put(0x0008, 0x0033, <now_time>)
   
   // Modality (fixed)
   dataset.put(0x0008, 0x0060, "DX")

5. POPULATE optional metadata
   IF image.body_part_examined:
     dataset.put(0x0018, 0x0015, image.body_part_examined)
   IF image.accession_number:
     dataset.put(0x0008, 0x0050, image.accession_number)

6. POPULATE XPE private block (optional)
   IF xpe_flags_enabled:
     dataset.put(0x0019, 0x0010, "XPE")  // Private Creator
     dataset.put(0x0019, 0x1001, image.xpe_flags)  // Processing Flags
     dataset.put(0x0019, 0x1002, "1.0.0")  // XPE Version
     dataset.put(0x0019, 0x1003, <calib_date>)  // Calibration Date

7. ENCODE pixel data
   IF transfer_syntax == Implicit VR LE:
     encode_implicit_vr_(dataset, image.pixel_data, image.width, image.height)
   ELSE IF transfer_syntax == Explicit VR LE:
     encode_explicit_vr_(dataset, image.pixel_data, image.width, image.height)
   ELSE IF transfer_syntax == JPEG 2000 Lossless:
     encode_j2k_lossless_(dataset, image.pixel_data, image.width, image.height)
     [Use OpenJPEG library]
   ELSE:
     return XPE_ERR_UNSUPPORTED_TRANSFER_SYNTAX

8. SET Transfer Syntax
   dataset.setTransferSyntax(transfer_syntax)

9. WRITE file
   IF write_permission NOT granted:
     return XPE_ERR_PERMISSION_DENIED
   
   IF disk_space < file_size:
     return XPE_ERR_DISK_FULL
   
   status = dcm_file.saveFile(output_path)
   IF status != EC_Normal:
     return XPE_ERR_FILE_WRITE_FAILED

10. VALIDATE file
    verify_written_file_(output_path)
    IF VERIFICATION FAILED:
      delete output_file
      return XPE_ERR_FILE_WRITE_FAILED

11. RETURN XPE_OK
```

#### 2.2.3 Transfer Syntax 선택 로직

```
IF user_specified_ts:
  use transfer_syntax = user_specified_ts
ELSE:
  default transfer_syntax = Implicit VR LE (1.2.840.10008.1.2)
  
IF lossy_compression_detected(transfer_syntax):
  REJECT with XPE_ERR_LOSSY_COMPRESSION_NOT_ALLOWED
```

---

### 2.3 SWU-4.3: PresentationStateIO

**책임**: GSPS (Grayscale Softcopy Presentation State) 생성, 저장, 적용

#### 2.3.1 GSPS 객체 모델

```cpp
struct XpeGspsState {
    // Referenced image
    char primary_sop_class_uid[64];
    char primary_sop_instance_uid[64];
    char referenced_series_uid[64];
    
    // Window/Level presets (3개)
    struct {
        float center;
        float width;
        char description[32];
    } presets[3];  // Soft Tissue, Bone, Lung
    
    // Graphic annotations (선택)
    struct {
        uint32_t roi_count;
        XpeRoi rois[MAX_ROIS];  // Collimation ROI
    } annotations;
    
    // Display shutter (선택)
    struct {
        int rotation;  // 0, 90, 180, 270
        bool flip_h;
        bool flip_v;
    } shutter;
};
```

#### 2.3.2 GSPS 생성 알고리즘

```
Input: primary_image (DX), window_level, output_path
Output: GSPS file

1. VALIDATE input
   IF primary_image NULL → error
   IF primary_image.sop_class != DX → error

2. CREATE GSPS IOD
   gsps_file = new DcmFileFormat()
   gsps_dataset = gsps_file.getDataset()
   
   // SOP Class/Instance
   gsps_dataset.put(0x0008, 0x0016, "1.2.840.10008.5.1.4.1.1.11.1")
   gsps_sop_uid = generate_uid()
   gsps_dataset.put(0x0008, 0x0018, gsps_sop_uid)

3. COPY patient/study/series info
   gsps_dataset.put(0x0010, 0x0020, primary.patient_id)
   gsps_dataset.put(0x0020, 0x000D, primary.study_uid)
   gsps_dataset.put(0x0020, 0x000E, primary.series_uid)

4. SET Referenced Series Sequence
   ref_seq = new DcmSequenceOfItems()
   ref_item = new DcmItem()
   ref_item.put(0x0008, 0x1150, primary.sop_class_uid)  // Referenced SOP Class
   ref_item.put(0x0008, 0x1155, primary.sop_instance_uid)  // Referenced SOP Instance
   ref_seq.append(ref_item)
   gsps_dataset.put(0x0008, 0x1115, ref_seq)

5. ADD Window/Level Presets
   // Preset 1: Soft Tissue
   gsps_dataset.put(0x0028, 0x1050, 40.0)  // Window Center
   gsps_dataset.put(0x0028, 0x1051, 400.0)  // Window Width
   gsps_dataset.put(0x0028, 0x3002, "Soft Tissue")  // Presentation Label
   
   [Repeat for Bone, Lung presets]

6. ADD Graphic Annotations (if ROI provided)
   IF roi_count > 0:
     graphic_items = new DcmSequenceOfItems()
     FOR each roi:
       graphic_item.put(0x0070, 0x0105, "POLYLINE")  // Graphic Type
       graphic_item.put(0x0070, 0x0106, roi.point_count)
       graphic_item.put(0x0070, 0x0108, roi.points)  // Coordinates
     gsps_dataset.put(0x0070, 0x0009, graphic_items)

7. ADD Display Shutter (if flip/rotate)
   IF flip_h OR flip_v:
     gsps_dataset.put(0x0018, 0x1600, "RECTANGULAR")  // Shutter Shape
     [encode flip/rotate state]

8. SET Transfer Syntax
   gsps_file.setTransferSyntax(Implicit VR LE)

9. WRITE GSPS file
   gsps_path = primary_path + ".gsps.dcm"
   status = gsps_file.saveFile(gsps_path)
   IF FAILED → return error

10. RETURN gsps_sop_uid, XPE_OK
```

---

### 2.4 SWU-4.4: DicomNetworkSCU

**책임**: DICOM 네트워크 통신 (C-STORE, C-FIND), Association 관리

#### 2.4.1 Association 상태 머신

```cpp
enum AssociationState {
    IDLE,
    ASSOCIATION_REQUESTED,
    ASSOCIATION_ESTABLISHED,
    SENDING_IMAGE,
    QUERYING_MWL,
    WAITING_RESPONSE,
    ASSOCIATION_CLOSED,
    ERROR
};
```

#### 2.4.2 C-STORE SCU 알고리즘

```
Input: pacs_hostname, pacs_port, pacs_ae_title, dicom_file_path
Output: C-STORE status code

1. VALIDATE input
   IF hostname empty → return error
   IF port out of range (1-65535) → return error
   IF file not found → return error

2. RESOLVE hostname
   ip_addr = dns_resolve(hostname)
   IF RESOLUTION FAILED → return XPE_ERR_NETWORK_FAILURE

3. CREATE Association
   association = DcmServiceUserAssociation()
   association.setAETitle("xpe_dicom")
   association.setRemoteAETitle(pacs_ae_title)
   association.setTimeout(30 seconds)

4. REQUEST Association
   state = ASSOCIATION_REQUESTED
   status = association.openConnection(ip_addr, port)
   IF FAILED:
     IF retry_count < 3:
       sleep(2^retry_count seconds)  // exponential backoff
       GOTO 4  // retry
     ELSE:
       return XPE_ERR_NETWORK_FAILURE

5. NEGOTIATE Transfer Syntax
   supported_ts = [Implicit VR LE, Explicit VR LE, JPEG 2000 Lossless]
   association.addPresentationContext(DX_SOP_CLASS, supported_ts)

6. VERIFY Association
   IF association.isConnected():
     state = ASSOCIATION_ESTABLISHED
   ELSE:
     return XPE_ERR_NETWORK_FAILURE

7. READ DICOM file
   image = xpe_dicom_read(dicom_file_path, UINT16)
   IF FAILED → return error

8. SEND C-STORE request
   state = SENDING_IMAGE
   send_status = association.sendObject(image.pixel_data, image.metadata)
   IF FAILED → return error

9. RECEIVE C-STORE response
   c_store_status = association.waitResponse(120 seconds)
   
   IF c_store_status == 0x0000:  // Success
     state = ASSOCIATION_ESTABLISHED
     GOTO 11
   ELSE IF c_store_status == 0x0122:  // Warning
     log_warning("PACS returned warning status")
     state = ASSOCIATION_ESTABLISHED
     GOTO 11
   ELSE IF c_store_status == 0x0124:  // Failure
     log_error("C-STORE failed: SOP Class not supported")
     state = ASSOCIATION_ESTABLISHED
     GOTO 11

10. CHECK for more images
    IF more_images_to_send:
      GOTO 7  // repeat

11. RELEASE Association
    state = ASSOCIATION_CLOSED
    association.closeConnection()

12. RETURN c_store_status
```

#### 2.4.3 C-FIND SCU (MWL) 알고리즘

```
Input: ris_hostname, ris_port, ris_ae_title, MwlQuery
Output: MwlResult array, result count

1. [Association establishment: 같음 (2-6단계)]

2. CREATE C-FIND request
   query_dataset = new DcmDataset()
   
   // Add query keys (at least 1 required)
   IF query.patient_id:
     query_dataset.put(0x0010, 0x0020, query.patient_id)
   IF query.accession_number:
     query_dataset.put(0x0008, 0x0050, query.accession_number)
   IF query.scheduled_date:
     query_dataset.put(0x0040, 0x0100, query.scheduled_date)
   IF query.modality:
     query_dataset.put(0x0008, 0x0060, "DX")  // Query for DX only

3. VALIDATE query
   IF no query keys:
     return XPE_ERR_INVALID_ARGUMENT

4. SEND C-FIND request
   state = QUERYING_MWL
   find_status = association.sendFindRequest(query_dataset)
   IF FAILED → return error

5. RECEIVE C-FIND responses (multiple)
   results = new MwlResult[MAX_RESULTS]
   result_count = 0
   
   WHILE result_count < 100:  // max 100 results
     response = association.waitResponse(30 seconds)
     IF response == NULL:
       break  // timeout or completion
     
     // Parse response
     result.patient_name = response.get(0x0010, 0x0010)
     result.patient_id = response.get(0x0010, 0x0020)
     result.patient_dob = response.get(0x0010, 0x0030)
     result.accession_number = response.get(0x0008, 0x0050)
     result.scheduled_protocol_code = response.get(0x0040, 0x0008)
     
     results[result_count++] = result

6. [Association release: 11단계]

7. RETURN results, result_count, XPE_OK
```

---

## 3. 컴포넌트 분해

### 3.1 모듈 의존성 그래프

```
xpe_dicom.dll

┌─────────────────────────────────────────────────────┐
│ Public API Layer                                    │
│ (xpe_dicom_api.h, xpe_dicom_api.cpp)              │
│ - xpe_dicom_read()                                 │
│ - xpe_dicom_write()                                │
│ - xpe_dicom_cstore()                               │
│ - xpe_dicom_cfind_mwl()                            │
│ - xpe_gsps_create()                                │
└──────────────┬─────────────────────────────────────┘
               │
        ┌──────┴──────┐
        │             │
        v             v
┌──────────────────┐  ┌──────────────────┐
│ Reader Module    │  │ Writer Module    │
│ (xpe_dicom_     │  │ (xpe_dicom_     │
│  reader.cpp)    │  │  writer.cpp)    │
└────────┬─────────┘  └────────┬─────────┘
         │                     │
         └──────────┬──────────┘
                    │
                    v
        ┌─────────────────────┐
        │ DCMTK Wrapper       │
        │ (xpe_dcmtk.cpp)     │
        └──────────┬──────────┘
                   │
        ┌──────────┴──────────┐
        │                     │
        v                     v
    ┌─────────┐          ┌──────────────┐
    │ DCMTK   │          │ Network Mgr  │
    │         │          │ (network.cpp)│
    └────┬────┘          └──────┬───────┘
         │                      │
         └──────────┬───────────┘
                    │
                    v
        ┌────────────────────────┐
        │ xpe_common.dll (Layer 0)│
        │ Types, Memory, Logger  │
        └────────────────────────┘
```

### 3.2 파일 구조

```
xpe_dicom.dll/
├── include/
│   ├── xpe_dicom_api.h         [Public API]
│   ├── xpe_dicom_types.h       [Internal types]
│   ├── xpe_dicom_reader.h      [SWU-4.1]
│   ├── xpe_dicom_writer.h      [SWU-4.2]
│   ├── xpe_gsps.h              [SWU-4.3]
│   ├── xpe_dicom_network.h     [SWU-4.4]
│   └── xpe_dcmtk.h             [DCMTK wrapper]
│
├── src/
│   ├── xpe_dicom_api.cpp       [API entry points]
│   ├── xpe_dicom_reader.cpp    [DICOM reading logic]
│   ├── xpe_dicom_writer.cpp    [DICOM writing logic]
│   ├── xpe_gsps.cpp            [GSPS creation/apply]
│   ├── xpe_dicom_network.cpp   [Network SCU logic]
│   ├── xpe_dcmtk.cpp           [DCMTK wrappers]
│   ├── xpe_dicom_error.cpp     [Error handling]
│   └── xpe_dicom_log.cpp       [Logging]
│
├── test/
│   ├── test_reader.cpp
│   ├── test_writer.cpp
│   ├── test_gsps.cpp
│   ├── test_network.cpp
│   └── test_data/              [Sample DICOM files]
│
└── CMakeLists.txt              [Build configuration]
```

---

## 4. 인터페이스 정의

### 4.1 Public C ABI Interface

#### 4.1.1 DICOM Read

```c
/**
 * xpe_dicom_read
 * Read DICOM file and extract image data + metadata
 *
 * @param file_path: DICOM file path (absolute or relative)
 * @param pixel_format: XPE_PIXEL_UINT16 or XPE_PIXEL_FLOAT32
 * @param out_image: Output XpeImage structure (allocated by caller)
 * @return: XPE_OK on success, error code on failure
 */
int xpe_dicom_read(
    const char *file_path,
    int pixel_format,
    XpeImage *out_image
);
```

**오류 처리:**
- `XPE_ERR_FILE_NOT_FOUND`: 파일 없음
- `XPE_ERR_FILE_READ_FAILED`: 파일 읽기 실패
- `XPE_ERR_DICOM_INVALID`: DICOM 형식 오류
- `XPE_ERR_DICOM_CORRUPTED`: 파일 손상
- `XPE_ERR_DICOM_UNSUPPORTED_TRANSFER_SYNTAX`: 지원되지 않는 TS
- `XPE_ERR_DICOM_INVALID_DIMENSION`: 이미지 크기 초과
- `XPE_ERR_OUT_OF_MEMORY`: 메모리 부족

---

#### 4.1.2 DICOM Write

```c
/**
 * xpe_dicom_write
 * Write XpeImage to DICOM file
 *
 * @param output_path: Output file path
 * @param image: Input XpeImage (const)
 * @param transfer_syntax: XPE_TS_IMPLICIT_VR_LE, etc.
 * @return: XPE_OK on success, error code on failure
 *
 * CRITICAL: Lossy compression (J2K Irreversible) automatically rejected
 */
int xpe_dicom_write(
    const char *output_path,
    const XpeImage *image,
    int transfer_syntax
);
```

**오류 처리:**
- `XPE_ERR_DICOM_MISSING_REQUIRED_TAG`: 필수 메타데이터 누락
- `XPE_ERR_LOSSY_COMPRESSION_NOT_ALLOWED`: 손실 압축 거부 (CRITICAL)
- `XPE_ERR_FILE_WRITE_FAILED`: 파일 쓰기 실패
- `XPE_ERR_DISK_FULL`: 디스크 공간 부족
- `XPE_ERR_PERMISSION_DENIED`: 권한 거부

---

#### 4.1.3 C-STORE SCU

```c
/**
 * xpe_dicom_cstore
 * Send DICOM image to PACS via C-STORE SCU
 *
 * @param pacs_hostname: PACS hostname or IP
 * @param pacs_port: PACS port (default 104)
 * @param pacs_ae_title: PACS AE Title
 * @param dicom_file_path: File to send
 * @return: DICOM C-STORE status (0x0000=Success, 0x0124=Failure)
 */
int xpe_dicom_cstore(
    const char *pacs_hostname,
    int pacs_port,
    const char *pacs_ae_title,
    const char *dicom_file_path
);
```

**Retry Policy:**
- Automatic retry: 3 times
- Backoff: 2s, 4s, 8s

---

#### 4.1.4 C-FIND SCU (MWL)

```c
/**
 * xpe_dicom_cfind_mwl
 * Query Modality Worklist from RIS via C-FIND SCU
 *
 * @param ris_hostname: RIS hostname
 * @param ris_port: RIS port (default 104)
 * @param ris_ae_title: RIS AE Title
 * @param query: MWL query criteria
 * @param out_results: Output array of MwlResult (allocate before call)
 * @param out_count: Number of results returned
 * @return: XPE_OK on success, error code on failure
 */
int xpe_dicom_cfind_mwl(
    const char *ris_hostname,
    int ris_port,
    const char *ris_ae_title,
    const XpeMwlQuery *query,
    XpeMwlResult *out_results,  // caller allocates array[100]
    int *out_count
);
```

---

#### 4.1.5 GSPS Create

```c
/**
 * xpe_gsps_create
 * Create GSPS (Presentation State) linked to primary image
 *
 * @param primary_image: DX image to reference
 * @param window_level: Window/Level settings
 * @param gsps_output_path: Output GSPS file path
 * @return: XPE_OK on success, error code on failure
 */
int xpe_gsps_create(
    const XpeImage *primary_image,
    const XpeWindowLevel *window_level,
    const char *gsps_output_path
);
```

---

## 5. 데이터 흐름

### 5.1 Read 데이터 흐름

```
DICOM File (disk)
    ↓
[Validation]
    ├─ File existence
    ├─ DICOM preamble ("DICM")
    └─ Header integrity
    ↓
[DCMTK Parsing]
    ├─ Read dataset
    ├─ Detect Transfer Syntax
    └─ Validate IOD
    ↓
[Metadata Extraction]
    ├─ Patient (ID, Name, DOB)
    ├─ Study (Date, UID)
    ├─ Image (Rows, Columns, PixelSpacing, Window/Level)
    └─ Equipment (Manufacturer, Serial)
    ↓
[Pixel Data Decoding]
    ├─ IF Implicit/Explicit VR LE: copy raw data
    ├─ IF JPEG 2000 Lossless: OpenJPEG decode
    └─ IF JPEG Baseline: DCMTK decode
    ↓
[Format Conversion]
    ├─ IF pixel_format == UINT16: use as-is
    └─ IF pixel_format == FLOAT32: normalize [0.0 ~ 65535.0]
    ↓
[Photometric Correction]
    ├─ IF MONOCHROME1: invert (MAX - pixel)
    └─ IF MONOCHROME2: use as-is
    ↓
XpeImage (memory)
```

### 5.2 Write 데이터 흐름

```
XpeImage (memory)
    ↓
[Validation]
    ├─ Metadata completeness
    ├─ Lossy compression check (CRITICAL reject)
    └─ Transfer Syntax validation
    ↓
[DX IOD Creation]
    ├─ Create DICOM file structure
    ├─ Generate SOP Instance UID
    └─ Set Transfer Syntax
    ↓
[Metadata Encoding]
    ├─ Patient, Study, Series modules
    ├─ Image Pixel module (Rows, Columns, BitsAllocated)
    ├─ Window/Level presets
    └─ XPE private block (optional)
    ↓
[Pixel Data Encoding]
    ├─ IF Implicit/Explicit VR LE: encode uint16 directly
    ├─ IF JPEG 2000 Lossless: OpenJPEG encode
    └─ Dataset.put(0x7FE0, 0x0010, encoded_data)
    ↓
[File I/O]
    ├─ Check disk space
    ├─ Write to temporary file
    ├─ Verify written data
    └─ Rename to final path
    ↓
DICOM File (disk)
```

---

## 6. SOUP 통합

### 6.1 DCMTK 통합 계층

```cpp
// xpe_dcmtk.cpp/h
namespace xpe::dcmtk {

// Wrapper for DcmFileFormat
class DcmFileWrapper {
  DcmFileFormat file_;
  
  public:
    int read(const char* path);
    int write(const char* path);
    DcmDataset* getDataset();
    const char* getTransferSyntax();
};

// Transfer Syntax negotiation
int selectTransferSyntax(int xpe_ts, DcmXferSyntax& dcm_ts);

// Error code mapping
int dcmtkErrorToCpp(OFCondition cond);

}
```

### 6.2 OpenJPEG 통합

```cpp
// xpe_jpeg2k.cpp/h
namespace xpe::jpeg2k {

// J2K Decode
int decode(
    const uint8_t* compressed_data,
    size_t compressed_size,
    uint16_t** out_pixels,  // Allocated by function
    uint32_t* out_width,
    uint32_t* out_height
);

// J2K Encode (Lossless only)
int encode_lossless(
    const uint16_t* pixels,
    uint32_t width,
    uint32_t height,
    uint8_t** out_compressed,  // Allocated by function
    size_t* out_size
);

// Validate encoding is truly lossless
bool isLosslessMode(const char* j2k_header);

}
```

---

## 7. 에러 처리

### 7.1 에러 분류

| 레벨 | 유형 | 예 | 처리 |
|------|------|-----|------|
| **CRITICAL** | 진단 무결성 영향 | Lossy compression detected | Immediate hard rejection |
| **MAJOR** | 기능 장애 | DICOM parse error | Error code + log + alert |
| **MINOR** | 성능/편의성 | Unsupported TS | Graceful degradation |
| **ADVISORY** | 정보성 | Metadata missing | Log + continue |

### 7.2 에러 전파

```
DCMTK Library
    ↓ OFCondition
Internal xpe_dcmtk wrapper
    ↓ dcmtkErrorToCpp()
xpe_dicom_* functions
    ↓ int (XPE_ERR_*)
Public API
    ↓ Caller
ImageProcTest.exe
    ↓ User alert / log
```

---

## 8. 리스크 통제

### 8.1 Lossy Compression 방지

**설계 원칙:**
- Transfer Syntax 검증은 다중 층에서 수행
- 호출자가 손실 압축을 의도해도 자동 거부
- 에러 메시지 명확: "Lossy compression not allowed for diagnostic images"

**구현:**
```cpp
// xpe_dicom_writer.cpp
if (transfer_syntax == JPEG_2000_IRREVERSIBLE) {
    log_critical("Lossy J2K compression detected - REJECTING");
    return XPE_ERR_LOSSY_COMPRESSION_NOT_ALLOWED;
}
// This check CANNOT be bypassed by any parameter
```

### 8.2 환자 ID 검증

**설계:**
- DICOM 파일의 Patient ID와 XpeImage 메타데이터 비교
- 불일치 시 에러 + alert 발행

**구현:**
```cpp
if (dicom_patient_id != metadata_patient_id) {
    log_alert("Patient ID mismatch: %s vs %s", 
              dicom_patient_id, metadata_patient_id);
    return XPE_ERR_PATIENT_ID_MISMATCH;
}
```

### 8.3 DICOM 파일 무결성

**설계:**
- Preamble 검증
- VR/길이 필드 일관성 확인
- 손상 감지 → 에러, partial data 반환 금지

### 8.4 네트워크 안전성

**설계:**
- C-STORE 실패 시 로컬 파일 보호
- 재시도 로직 (exponential backoff)
- 최종 실패 후 명확한 에러 코드

---

**문서 끝: SAD-DICOM-001 v1.0.0**
