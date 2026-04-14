# Panel Defect Correction Module - Technical Overview

**Module**: `xpe_preprocess.dll` (Stage 3, Layer 1)  
**Owner**: XPE Panel Defect Development Team  
**Dependencies**: `xpe_common.dll` (Layer 0)  
**Safety Classification**: IEC 62304 Class B  
**Document Version**: 1.0  
**Date**: 2026-04-14  
**Specification Reference**: [xray-panel-defect-prd.md](xray-panel-defect-prd.md)

---

## 목차

1. [개요](#개요)
2. [검출기 컨텍스트](#검출기-컨텍스트)
3. [파이프라인 단계](#파이프라인-단계)
4. [ANN 보정 아키텍처](#ann-보정-아키텍처)
5. [Min/Normal/Max 프로필](#minnormalmax-프로필)
6. [우회 정책](#우회-정책)
7. [캘리브레이션 데이터](#캘리브레이션-데이터)
8. [API 레퍼런스](#api-레퍼런스)
9. [성능 예산](#성능-예산)
10. [안전 제약](#안전-제약)
11. [참고문헌](#참고문헌)

---

## 개요

### 목적

Panel Defect Correction Module은 X-ray 플랫-패널 방사선 촬영(FPD)에서 발생하는 네 가지 결함을 검출하고 보정합니다:

1. **고립 픽셀** (Dead, Hot, Flickering, Stuck)
2. **클러스터 결함** (3×3, 5×5 인접 불량)
3. **라인 결함** (1-5 픽셀 폭, Type 1/3/5)
4. **그리드/모아레 아티팩트** (주기적 고주파 패턴)

### 가치

- **검출**: RMM (Robust Mask Maker) λ=8.0으로 정적 BPM 생성
- **보정**: ANN (Artificial Neural Network) 기반 클러스터 보정
  - 3×3: 40→9 단층 (Lee et al.)
  - 5×5: 56→25 + 숨겨진 계층 (concat-CNN baseline)
  - 템플릿 매칭 정제 옵션 (TMC)
- **라인**: Type 분류 (diffVal) + 모드별 보정 전략
- **그리드**: DWT 대역 저지 또는 DCT 동적 분할

### 임상 영향

- 임상 영상 품질 향상 (artifact 제거)
- 진단 정확도 개선 (noise, stripe 감소)
- 환자 피폭량 최소화 (재촬영 감소)

---

## 검출기 컨텍스트

### 하드웨어 사양

| 항목 | 값 |
|------|-----|
| **Detector Model** | AUO R1717 |
| **Resolution** | 3072 × 3072 pixels |
| **Pixel Pitch** | ~127 μm |
| **Detection Type** | Indirect (CsI:Tl scintillator + a-Si TFT) |
| **Typical Defect Rate** | 0.1-0.3% (hot + cold + flickering) |
| **Typical Line Defects** | 1-5 per detector (manufacturing, aging) |

### a-Si TFT 결함 메커니즘

| 결함 타입 | 원인 | 주파수 |
|---------|------|--------|
| **Dead Pixel** | TFT open circuit, scintillator defect | ~0.01% |
| **Hot Pixel** | Leakage current, trapped charge | ~0.05-0.2% |
| **Flickering** | Unstable TFT contact, intermittent short | ~0.001% |
| **Stuck Pixel** | TFT latch-up, permanent saturation | ~0.0001% |
| **Line Defect** | Row/column shorts, manufacturing error | ~5 lines/panel |
| **Cluster** | Manufacturing defects (microbumps, solder) | ~1% |

---

## 파이프라인 단계

### Step 0: Static BPM Generation (Calibration-time)

```
Dark Frames (200×3 temps)    Flat-Field Frames (200×3 temps)
        │                              │
        ▼                              ▼
  [RMM λ=8.0]                   [Gain Analysis]
   HotPixelMask                  ColdPixelMask
        │                              │
        │        Flickering (200 fr)   │
        │               │              │
        └──────────┬────┴──────────────┘
                   │
                   ▼
            [Connected Components]
            LineDefectMask
                   │
                   ▼
         [Union of all masks]
                   │
                   ▼
        Static BPM (uint8)
        9.4 MB (or <1 MB RLE)
```

**특징**:
- 캘리브레이션 시에만 한 번 생성
- 3개 온도 레벨 (20/30/40°C) 평균
- RLE 압축 지원 (메모리 절약)

### Step 1-2: Dynamic Detection (Per-frame)

```
Input: float32 gain-corrected image
       (from xpe_preprocess.dll Phase 1-2)
           │
           ▼
    [Residual Map]
    5×5 median subtraction
           │
           ▼
    [k·σ Local Thresholding]
    k = 3.0 (Min), 4.0 (Normal), 5.0 (Max)
           │
           ▼
    [Merge with Static BPM]
    Union: dynamic ∪ static
           │
           ▼
    Defect Map (binary)
```

**시간**: < 20 ms

### Step 3: Cluster Correction (3×3, 5×5)

```
Defect Map → [Connected Components] → Morphology Classification
                                            │
                ┌───────────┬───────────────┼───────────┐
                │           │               │           │
                ▼           ▼               ▼           ▼
            Isolated      3×3 Cluster   5×5 Cluster    Line
            (skip/avg)    │             │              │
                          ▼             ▼              ▼
                       [ANN 40→9]   [ANN 56→25]   [Type/diffVal]
                          │          + optional      │
                          │           TMC            ▼
                          │             │        [Type 1/3/5
                          │             │         Correction]
                          └──────┬──────┴─────────────┤
                                 │                    │
                                 ▼                    ▼
                          [Merge Results]
                               │
                               ▼
                          Corrected Image
```

**시간**: < 55 ms (3×3 + 5×5 + line 합산)

### Step 4: Grid/Moiré Suppression

```
Corrected Image
       │
       ▼
[3-level 2D DWT]
       │
       ▼
[Energy Analysis]
Identify grid subbands
       │
       ▼
[Gaussian Bandstop Filter]
Applied to grid-dominated subbands
       │
       ▼
[Inverse DWT]
       │
       ▼
Grid-Suppressed Image
(MSI target: < 0.1)
```

**시간**: < 15 ms (only if MSI > threshold)

---

## ANN 보정 아키텍처

### 3×3 Cluster (Single-Layer ANN)

```
Input Vector (40-dim):
  ┌───────────────────────────────┐
  │  7×7 neighborhood             │
  │  minus center 3×3             │
  │  = 49 - 9 = 40 pixels         │
  └───────────────────────────────┘
           │
           ▼
  ┌─────────────────────────────┐
  │  Linear Layer:              │
  │  y = W·x + b                │
  │  W: (9, 40)                 │
  │  b: (9,)                    │
  │  No hidden layer            │
  │  No activation              │
  └─────────────────────────────┘
           │
           ▼
  ┌───────────────────────────────┐
  │  Output Vector (9-dim):       │
  │  3×3 corrected values         │
  │  Clipped to [0, 2^14]         │
  └───────────────────────────────┘

NMSE Target: < 0.14 (vs. Jeon 2021 baseline)
```

### 5×5 Cluster (Hidden Layer ANN)

```
Input Vector (56-dim):
  ┌───────────────────────────────┐
  │  9×9 neighborhood             │
  │  minus center 5×5             │
  │  = 81 - 25 = 56 pixels        │
  └───────────────────────────────┘
           │
           ▼
  ┌─────────────────────────────┐
  │  Hidden Layer:              │
  │  h = ReLU(W_h·x + b_h)      │
  │  W_h: (64, 56)              │
  │  b_h: (64,)                 │
  │  64 hidden units (ReLU)     │
  └─────────────────────────────┘
           │
           ▼
  ┌─────────────────────────────┐
  │  Output Layer:              │
  │  y = W_o·h + b_o            │
  │  W_o: (25, 64)              │
  │  b_o: (25,)                 │
  │  Linear activation          │
  └─────────────────────────────┘
           │
           ▼
  ┌───────────────────────────────┐
  │  Optional: TMC Refinement     │
  │  Search 27×27 neighborhood    │
  │  Blend: 0.7·ANN + 0.3·matched│
  └───────────────────────────────┘

NMSE Target: < 0.20 (basic), < 0.10 (with TMC)
```

### Weight Storage & Validation

**위치**: Calibration profile file (.xpe_calib)

**무결성**: MD5/SHA-256 해시로 검증

**Size**:
- 3×3 ANN weights: ~0.2 KB (compressed)
- 5×5 ANN weights: ~10 KB (compressed)

**Fallback**: Weight 손상 시 → neighbor averaging (graceful degradation)

---

## Min/Normal/Max 프로필

### 개념

| 프로필 | 목표 | 사용 시기 |
|--------|------|----------|
| **Min** | 환자 중심 (공격적 보정) | 고위험 진단 (의심 병변) |
| **Normal** | 균형 (표준 감도/특이성) | 일반 임상 (권장 기본값) |
| **Max** | 패널 수명 중심 (보수적) | 장기 운영 (패널 보존) |

### 파라미터 비교

| 파라미터 | Min | Normal | Max |
|---------|-----|--------|-----|
| **k threshold** | 3.0 | 4.0 | 5.0 |
| **T1 (Type 5 경계)** | 0.15 | 0.25 | 0.40 |
| **T2 (Type 1 경계)** | 0.50 | 0.75 | 1.00 |
| **MSI threshold** | 0.10 | 0.20 | 0.40 |
| **DWT filter strength** | 2.0× (강) | 1.0× (표준) | 0.5× (약) |
| **3×3 보정** | Always | Always | Always |
| **5×5 보정** | Always | Always | Always |
| **Type 1 라인** | Always | Always | Threshold-based |
| **Type 3 라인** | Always | Always | Selective |
| **Type 5 라인** | Always | Minimal | Never |
| **TMC 활용** | Yes | Optional | No |

---

## 우회 정책

### 정상 운영

**우회 불가능**: 검출 및 보정은 필수 (SRS-SAFE-101)

**Fail-open policy**:
- BPM 로드 실패 → 캐시된 이전 BPM 사용 (또는 동적 검출만)
- 모든 경우 감시 로그 기록

### 진단 모드 (Service Engineer Only)

```c
xpe_defect_set_bypass(true);  // 우회 활성화
// → 모든 호출이 감시 로그에 기록됨
// → 임상 모드에서는 실행 불가능
```

**목적**: Troubleshooting 및 알고리즘 검증

---

## 캘리브레이션 데이터

### 정적 BPM 파일 포맷

```c
struct BPMHeader {
    uint32_t magic;           // 0x585045 ("XPE")
    uint32_t version;         // 1
    uint64_t timestamp_ms;    // Generation time (ms since 2000-01-01)
    uint64_t expiry_ms;       // Expiration time (ms since 2000-01-01)
    uint32_t reserved;
};

// Total header: 34 bytes
// Followed by: uint8 pixel data (3072×3072)
//              CRC-32 checksum (4 bytes)
```

**유효 기간**: 

| 환경 | 유효 기간 |
|------|----------|
| **공장** | 1년 (또는 제조사 정책) |
| **현장** | 설치 후 1년, 이후 정기 갱신 |

### 데이터 관리

| 파일 | 역할 | 크기 |
|------|------|------|
| **BPM.xpe_calib** | Static defect map | 9.4 MB (or <1 MB RLE) |
| **ANN_weights.bin** | 3×3 + 5×5 trained weights | <10 KB |
| **DWT_filter.bin** | Grid suppression filter params | <1 KB |
| **Calibration.log** | Audit trail | Growing |

### CRC-32 검증

```c
uint32_t crc32_poly = 0x04C11DB7;
uint32_t calculated_crc = compute_crc32(bpm_data);
if (calculated_crc != file_crc) {
    return XPE_ERR_IO_FAILED;  // Corruption detected
}
```

---

## API 레퍼런스

### 초기화

```c
XPE_error xpe_defect_init(const XPE_Config* cfg);
```

Load Static BPM, ANN weights, filter parameters.

### 설정

```c
void xpe_defect_set_profile(XPE_DefectProfile profile);
// profile: XPE_PROFILE_MIN, XPE_PROFILE_NORMAL, XPE_PROFILE_MAX
```

### 처리

```c
XPE_error xpe_defect_process(
    const XPE_ImageBuffer* in,    // float32 gain-corrected
    XPE_ImageBuffer* out          // float32 defect-corrected
);
```

All-in-one: detect → correct (clusters + lines + grid)

### 통계

```c
void xpe_defect_get_stats(XPE_DefectStats* stats);
// Returns: defect counts, MSI value, processing time
```

### 정리

```c
void xpe_defect_cleanup(void);
```

Release all allocated memory.

---

## 성능 예산

### 처리 시간

| 단계 | 목표 |
|------|------|
| Dynamic Detection | < 20 ms |
| 3×3 Cluster Correction | < 10 ms |
| 5×5 Cluster Correction | < 15 ms |
| Line Defect Correction | < 30 ms |
| Grid/Moiré Suppression | < 15 ms |
| **Total per 3072×3072 frame** | **< 95 ms** |

### 메모리

| 항목 | 할당 |
|------|------|
| Static BPM | ~10 MB (1회) |
| ANN weights | ~5 MB (1회) |
| Per-frame working | <120 MB |
| **Total peak** | **<150 MB** |

---

## 안전 제약

### Mandatory (SAF-101)

1. Offset correction + Gain correction 필수 (이전 단계)
2. Panel Defect correction은 optional하지만 활성화되면 완전히 실행
3. Fail-open on BPM load failure: proceed with dynamic detection only

### Data Integrity (SAF-203)

1. All calibration files: MD5/SHA-256 hash verification
2. ANN weight corruption detected → fallback to neighbor averaging
3. BPM file corruption detected → use cached previous BPM (if available)

### Audit Trail (SAF-201)

1. All defect detection results logged
2. All corrections applied logged (type, count, pixels affected)
3. Grid suppression MSI values logged
4. Processing time per frame logged

---

## 참고문헌

### Bad Pixel / Cluster / Line Defect

1. **Jeon et al. (2021)** - PMC7930811
   - Deep learning for pixel-defect corrections in FPD
   - 3×3, 5×5 ANN/CNN architectures

2. **FixPix (2023)** - arXiv:2310.11637
   - Lightweight MLP (1425 params) for bad pixel detection
   - FPGA-friendly architecture

3. **CN104463831A** - Chinese patent
   - Method for repairing X-ray FPD bad lines
   - Type 1, 3, 5 classification and repair strategies

### Grid / Moiré / Aliasing

4. **Tang et al. (2012)**
   - 2D DWT-based gridline artifact suppression
   - Baseline methodology for this module

5. **Park et al.**
   - DCT-based dynamic segmentation for grid artifacts
   - Advanced suppression technique

6. **IEC 62220-1-1:2015**
   - Medical device image quality standards
   - Detective quantum efficiency (DQE) metrics

### Standards

7. **IEC 62304:2006 (amended 2015)**
   - Medical device software lifecycle
   - §5 (Software Requirements), §7 (Risk Management)

8. **ISO 14971:2019**
   - Risk management for medical devices
   - Hazard identification, assessment, control

---

**Document Version**: 1.0  
**Classification**: Technical Overview  
**Audience**: Developers, QA, Clinical Engineers  
**Related Documents**:
- [xray-panel-defect-prd.md](xray-panel-defect-prd.md) - Product Requirements
- [SRS-DEFECT-001](SRS-DEFECT-001_Software_Requirements_Specification.md) - Software Requirements
- [SAD-DEFECT-001](SAD-DEFECT-001_Software_Architecture_Document.md) - Architecture
- [SHA-DEFECT-001](SHA-DEFECT-001_Software_Hazard_Analysis.md) - Hazard Analysis
- [RTM-DEFECT-001](RTM-DEFECT-001_Requirements_Traceability_Matrix.md) - Traceability
- [IAP-DEFECT-001](IAP-DEFECT-001_Image_Acquisition_Protocol.md) - Calibration Protocol
- [TDS-DEFECT-001](TDS-DEFECT-001_Test_Dataset_Specification.md) - Test Data

**Last Updated**: 2026-04-14
