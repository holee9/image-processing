# Calibration vs Preprocessing Algorithms

**Version**: 1.0.0 | **Created**: 2026-04-19 | **Status**: Final

**Purpose**: 명확한 기술 분류 - Calibration(보정 데이터 관리)과 Preprocessing Algorithms(전처리 알고리즘)의 차이

---

## 1. 정의 (Definition)

### 1.1 Calibration (SUP-01)

**Calibration**은 **detector 고유의 보정 데이터(Offset, Gain, Defect Map)를 관리하고 적용하는 기능**입니다.

- **Research ID**: SUP-01
- **SWU 매핑**: SWU-1.5 (Calibration Manager), SWU-5.6 (Calibration Parameter Management)
- **목적**: Detector 특성별 보정 데이터의 생성, 저장, 로딩, 검증
- **분류**: Support 기술 (필수)

### 1.2 Preprocessing Algorithms (PRE-01~09)

**Preprocessing Algorithms**은 **Raw detector 데이터를 보정된 이미지로 변환하는 9가지 전처리 기술**입니다.

- **Research ID**: PRE-01~PRE-09
- **SWU 매핑**: SWU-1.1~1.9
- **목적**: Raw detector 이미지의 각종 아티팩트 제거 및 신호 정규화
- **분류**: Pre-processing 기술 (필수)

---

## 2. 기술 분류 (Taxonomy)

```
XPE Preprocessing Module
├── Calibration (SUP-01, SWU-1.5/5.6)
│   ├── Calibration Data Management
│   │   ├── XCal v1 File Format
│   │   ├── Offset Map Loading/Saving
│   │   ├── Gain Map Loading/Saving
│   │   └── Defect Map Loading/Saving
│   └── Calibration Data Validation
│       ├── SHA-256 Integrity Check
│       ├── Expiry Date Validation
│       └── Session ID Validation
│
└── Preprocessing Algorithms (PRE-01~09, SWU-1.1~1.9)
    ├── PRE-01: Readout Artifact Validation (SWU-1.9)
    ├── PRE-02: Offset/Dark Correction (SWU-1.1)
    ├── PRE-03: Gain/Flat-Field Correction (SWU-1.2)
    ├── PRE-04: Lag Correction (SWU-1.4)
    ├── PRE-05: Ghost/Gain Ghosting Correction (SWU-1.4)
    ├── PRE-06: Defective Pixel Correction (SWU-1.3)
    ├── PRE-07: Temperature Compensation (SWU-1.5)
    ├── PRE-08: Non-linearity Correction (SWU-1.7)
    └── PRE-09: Pixel Binning Correction (SWU-1.8)
```

---

## 3. API 매핑 (API Mapping)

### 3.1 Calibration API (SUP-01)

| 함수 | 목적 | 분류 |
|------|------|------|
| `xpe_calib_load_offset()` | Offset map 로딩 | Calibration |
| `xpe_calib_load_gain()` | Gain map 로딩 | Calibration |
| `xpe_calib_load_defect_map()` | Defect map 로딩 | Calibration |
| `xpe_calib_generate_offset()` | Offset map 생성 | Calibration |
| `xpe_calib_save()` | Calibration data 저장 | Calibration |
| `xpe_calib_check_expiry()` | Expiry date 검증 | Calibration |

### 3.2 Preprocessing Algorithm API (PRE-01~09)

| 함수 | Research ID | SWU | 목적 | 분류 |
|------|-------------|-----|------|------|
| `xpe_validate_readout_artifact()` | PRE-01 | SWU-1.9 | Readout artifact 검증 | Preprocessing |
| `xpe_offset_correct()` | PRE-02 | SWU-1.1 | Offset/dark 보정 | Preprocessing |
| `xpe_gain_correct()` | PRE-03 | SWU-1.2 | Gain/flat-field 보정 | Preprocessing |
| `xpe_ghost_create/correct/reset/destroy()` | PRE-04/05 | SWU-1.4 | Lag/ghost 보정 | Preprocessing |
| `xpe_defect_correct()` | PRE-06 | SWU-1.3 | Defective pixel 보정 | Preprocessing |
| `xpe_defect_detect_runtime()` | PRE-06 | SWU-1.3 | Runtime defect 검출 | Preprocessing |
| `xpe_temp_compensate()` | PRE-07 | SWU-1.5 | Temperature 보상 | Preprocessing |
| `xpe_nonlinearity_correct()` | PRE-08 | SWU-1.7 | Non-linearity 보정 | Preprocessing |
| `xpe_binning_correct()` | PRE-09 | SWU-1.8 | Binning 보정 | Preprocessing |

---

## 4. XCal v1 File Format (Calibration Only)

**XCal v1**은 Calibration data만을 위한 파일 포맷입니다.

```c
// XCal Type Codes
typedef enum XCalType {
    XCAL_TYPE_OFFSET = 0,  // Dark/offset map (Calibration)
    XCAL_TYPE_GAIN   = 1,  // Gain/flat-field map (Calibration)
    XCAL_TYPE_DEFECT = 2   // Defect pixel map (Calibration)
} XCalType;
```

**Header Layout** (152 bytes, pack=1):
- Magic: "XCAL" (4 bytes)
- Version: uint32_t = 1
- Type: XCalType (0=OFFSET, 1=GAIN, 2=DEFECT)
- Pixel Format: UINT16/FLOAT32/UINT8_MASK
- Dimensions: width × height
- Timestamps: created_epoch_ms, expiry_epoch_ms
- Session ID: 64-byte UTF-8 string
- Config JSON: variable length
- Payload: pixel data
- SHA-256: hash of (config_json || payload)

---

## 5. 데이터 흐름 (Data Flow)

```
[Calibration Phase - SUP-01]
       ↓
XCal File Loading
  ├── offset.xcal → Offset Map
  ├── gain.xcal → Gain Map
  └── defect.xcal → Defect Map
       ↓
Calibration Data Validation (SHA-256, Expiry, Session)
       ↓
[Preprocessing Phase - PRE-01~09]
       ↓
Raw Detector Image (uint16)
       ↓
PRE-01: Readout Artifact Validation
       ↓
PRE-07: Temperature Compensation
       ↓
PRE-02: Offset Correction (uses Offset Map from Calibration)
       ↓
PRE-08: Non-linearity Correction
       ↓
PRE-03: Gain Correction (uses Gain Map from Calibration, uint16→float32)
       ↓
PRE-09: Binning Correction (conditional)
       ↓
PRE-06: Defect Correction (uses Defect Map from Calibration)
       ↓
PRE-04/05: Ghost/Lag Correction (stateful)
       ↓
Clean Image (float32)
```

---

## 6. SWU 매핑 테이블 (SWU Mapping Table)

| SWU | Research ID | 기능 | Module | 분류 |
|-----|-------------|------|--------|------|
| SWU-1.1 | PRE-02 | Offset/Dark Correction | xpe_preprocess | Preprocessing |
| SWU-1.2 | PRE-03 | Gain/Flat-Field Correction | xpe_preprocess | Preprocessing |
| SWU-1.3 | PRE-06 | Defective Pixel Correction | xpe_preprocess | Preprocessing |
| SWU-1.4 | PRE-04/05 | Ghost/Lag Correction | xpe_preprocess | Preprocessing |
| **SWU-1.5** | **SUP-01** | **Calibration Manager** | **xpe_preprocess** | **Calibration** |
| SWU-1.5 | PRE-07 | Temperature Compensation | xpe_preprocess | Preprocessing |
| SWU-1.7 | PRE-08 | Non-linearity Correction | xpe_preprocess | Preprocessing |
| SWU-1.8 | PRE-09 | Binning Correction | xpe_preprocess | Preprocessing |
| SWU-1.9 | PRE-01 | Readout Artifact Validation | xpe_preprocess | Preprocessing |
| SWU-5.6 | SUP-01 | Calibration Parameter Management | xpe_preprocess | Calibration |

**주의**: SWU-1.5는 두 가지 기능에 할당됨
- SUP-01: Calibration Manager
- PRE-07: Temperature Compensation

---

## 7. 파일 매핑 (File Mapping)

### 7.1 Calibration Files (SUP-01)

| 파일 | 목적 | 포맷 |
|------|------|------|
| `xcal_reader.cpp` | XCal 파일 파싱 및 로딩 | XCal v1 |
| `xcal_writer.cpp` | XCal 파일 생성 및 저장 | XCal v1 |
| `xcal_validator.cpp` | XCal 포맷 및 무결성 검증 | XCal v1 |
| `xpe_calib_load_offset.cpp` | Offset map 로딩 | XCal v1 |
| `xpe_calib_load_gain.cpp` | Gain map 로딩 | XCal v1 |
| `xpe_calib_load_defect_map.cpp` | Defect map 로딩 | XCal v1 |
| `xpe_calib_generate_offset.cpp` | Offset map 생성 | N/A |
| `xpe_calib_save.cpp` | Calibration data 저장 | XCal v1 |
| `xpe_calib_check_expiry.cpp` | Expiry date 검증 | N/A |
| `calibration_manager.cpp` | Calibration 상태 관리 | N/A |

### 7.2 Preprocessing Algorithm Files (PRE-01~09)

| 파일 | Research ID | SWU | 기능 |
|------|-------------|-----|------|
| `readout_validate.cpp` | PRE-01 | SWU-1.9 | Readout artifact 검증 |
| `offset_correct.cpp` | PRE-02 | SWU-1.1 | Offset/dark 보정 |
| `gain_correct.cpp` | PRE-03 | SWU-1.2 | Gain/flat-field 보정 |
| `defect_correct.cpp` | PRE-06 | SWU-1.3 | Defective pixel 보정 |
| `ghost_correct.cpp` | PRE-04/05 | SWU-1.4 | Ghost/lag 보정 |
| `temp_compensate.cpp` | PRE-07 | SWU-1.5 | Temperature 보상 |
| `nonlinearity_correct.cpp` | PRE-08 | SWU-1.7 | Non-linearity 보정 |
| `binning_correct.cpp` | PRE-09 | SWU-1.8 | Binning 보정 |

---

## 8. 헤더 파일 매핑 (Header File Mapping)

### 8.1 Calibration Headers (SUP-01)

| 헤더 | 목적 |
|------|------|
| `xcal_format.h` | XCal v1 file format definition |
| `calibration_manager.h` | Calibration state management |

### 8.2 Preprocessing Headers (PRE-01~09)

| 헤더 | 목적 |
|------|------|
| `xpe_preprocess_api.h` | Public C API for all preprocessing functions |
| `readout_validate.h` | PRE-01 API |
| `offset_correct.h` | PRE-02 API |
| `gain_correct.h` | PRE-03 API |
| `defect_correct.h` | PRE-06 API |
| `ghost_correct.h` | PRE-04/05 API |
| `temp_compensate.h` | PRE-07 API |
| `nonlinearity_correct.h` | PRE-08 API |
| `binning_correct.h` | PRE-09 API |

---

## 9. 핵심 차이점 (Key Differences)

| 항목 | Calibration (SUP-01) | Preprocessing Algorithms (PRE-01~09) |
|------|---------------------|-----------------------------------|
| **목적** | Detector 보정 데이터 관리 | Raw 이미지 → Clean 이미지 변환 |
| **입력** | XCal 파일 | Raw detector 이미지 + Calibration maps |
| **출력** | Calibration maps (Offset, Gain, Defect) | 보정된 이미지 (float32) |
| **파일 포맷** | XCal v1 | N/A (처리 함수) |
| **상태 유지** | Stateless (로딩/저장만) | Stateful (ghost correction) |
| **파이프라인 위치** | 사전 준비 단계 | 메인 처리 단계 |
| **SWU** | SWU-1.5, SWU-5.6 | SWU-1.1~1.9 |

---

## 10. 테스트 커버리지 (Test Coverage)

### 10.1 Calibration Tests (SUP-01)

| 테스트 파일 | 테스트 수 | 커버리지 |
|-----------|----------|---------|
| `test_xcal_validator.cpp` | 18 | XCal format validation |
| `test_xcal_reader.cpp` | 8 | Calibration loading |
| `test_xcal_writer.cpp` | 8 | Calibration saving |
| **Total** | **34** | **SUP-01** |

### 10.2 Preprocessing Algorithm Tests (PRE-01~09)

| 테스트 파일 | Research ID | 테스트 수 |
|-----------|-------------|----------|
| `test_readout_validate.cpp` | PRE-01 | 9 |
| `test_offset_correct.cpp` | PRE-02 | 8 |
| `test_gain_correct.cpp` | PRE-03 | 8 |
| `test_defect_correct.cpp` | PRE-06 | 15 |
| `test_ghost_correct.cpp` | PRE-04/05 | 12 |
| `test_temp_compensate.cpp` | PRE-07 | 6 |
| `test_nonlinearity_correct.cpp` | PRE-08 | 6 |
| `test_binning_correct.cpp` | PRE-09 | 6 |
| **Total** | **PRE-01~09** | **70** |

---

## 11. 레퍼런스 (References)

### 11.1 문서 (Documentation)

- `.moai/project/product.md` - Product overview
- `.moai/project/tech.md` - Technical stack
- `.moai/project/structure.md` - Code structure
- `modules/preprocess/include/xpe/preprocess/xpe_preprocess_api.h` - Public API
- `modules/preprocess/include/xpe/preprocess/xcal_format.h` - XCal format

### 11.2 SPEC (Specifications)

- **SPEC-XPE-P1A**: Phase 1a 전처리 모듈 사양
- **SUP-01**: Calibration Parameter Management
- **PRE-01~09**: Preprocessing algorithm specifications

---

## 12. 버전_history (Version History)

| 버전 | 날짜 | 변경 내용 |
|------|------|----------|
| 1.0.0 | 2026-04-19 | 초기 버전 - Calibration vs Preprocessing 분류 명확화 |

---

**Status**: ✅ Final - Approved for XPE project use
