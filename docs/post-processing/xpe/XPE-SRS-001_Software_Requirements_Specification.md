# Software Requirements Specification

**Document ID:** XPE-SRS-001 v1.0  
**IEC 62304 Clause:** 5.2.1 — 5.2.6  
**Safety Classification:** Class B  
**Date:** 2026-04-03  
**Author:** XPE Development Team  
**Approval:** __________________ Date: __________  

---

## 1. Purpose

XPE 소프트웨어 시스템의 기능, 성능, 인터페이스 및 안전 요구사항을 정의한다. 모든 요구사항은 testable, traceable, unique, consistent 해야 한다.

## 2. Requirements Content Categories (5.2.2)

- **(a)** Functional and capability requirements
- **(b)** Inputs and outputs
- **(c)** Interfaces between SW system and other systems
- **(d)** Alarms, warnings, operator messages
- **(e)** Security requirements
- **(f)** Usability requirements (→ IEC 62366-1)
- **(g)** Data definition and database requirements
- **(h)** Installation and acceptance requirements
- **(i)** Operation and maintenance requirements
- **(j)** Networking requirements
- **(k)** User maintenance requirements
- **(l)** Regulatory requirements

## 3. Functional Requirements (a)

### 3.1 Pre-Processing

| Req ID | Requirement | Priority | Traces to Risk |
|--------|------------|----------|---------------|
| SRS-FUNC-001 | 시스템은 raw detector data에서 dark offset을 감산하여 고유 신호를 제거해야 한다. Offset map은 ≥16 dark frames 평균으로 생성하며, negative 결과는 0으로 clamp한다. | Must | — |
| SRS-FUNC-002 | 시스템은 gain map을 적용하여 pixel 간 감도 차이 및 heel effect를 보정해야 한다. GainMap(x,y) = MeanFlood / [Flood(x,y) - Offset(x,y)]. SID별 개별 gain map을 지원해야 한다. | Must | — |
| SRS-FUNC-003 | 시스템은 bad pixel map 기반으로 point/cluster/line defect를 검출하고 인접 pixel 보간(4/8-neighbor)으로 보정해야 한다. Factory map + runtime 자동 갱신을 지원해야 한다. | Must | HAZ-003 |
| SRS-FUNC-004 | 시스템은 이전 exposure의 잔류 신호(ghost/lag)를 multi-exponential decay model [Lag(t) = Σ αᵢ×exp(-t/τᵢ), i=1..3]로 보정해야 한다. ≥90% ghost removal 달성. | Must | HAZ-004 |

### 3.2 Core Processing

| Req ID | Requirement | Priority | Traces to Risk |
|--------|------------|----------|---------------|
| SRS-FUNC-010 | 시스템은 선형 detector response를 logarithmic domain으로 변환해야 한다. LogImage = -ln(Corrected/I₀). Zero/negative는 ε clamping(1e-6). | Must | — |
| SRS-FUNC-011 | 시스템은 edge-preserving noise reduction을 제공해야 한다. 기본: Bilateral filter(σ_spatial=2.0, σ_range=0.1). 고품질: Non-Local Means. | Must | — |
| SRS-FUNC-012 | 시스템은 CLAHE 기반 adaptive contrast enhancement를 제공해야 한다. Default: block 8×8, clip limit 2.0, 256 bins. 파라미터 조정 가능. | Must | — |
| SRS-FUNC-013 | 시스템은 frequency-selective edge enhancement(unsharp masking)를 제공해야 한다. Gain은 body-part별 safe range 내로 제한. | Must | HAZ-005 |
| SRS-FUNC-014 | 시스템은 Laplacian pyramid 기반 multiscale frequency processing(≥8 level)을 제공해야 한다. 각 level에서 non-linear gain 적용. (Phase 2) | Should | — |
| SRS-FUNC-015 | 시스템은 Fractional Multiscale Processing으로 density transition zone의 artifact를 제거해야 한다. (Phase 2) | Should | — |
| SRS-FUNC-016 | 시스템은 CNN 기반 body-part recognition(≥15 categories, ≥95% accuracy)을 제공하고 processing parameter를 자동 선택해야 한다. (Phase 2) | Should | — |
| SRS-FUNC-017 | 시스템은 full-spine/long-leg panoramic image stitching(2-4 images, 10-30% overlap)을 제공해야 한다. Cobb angle 오차 ≤2°. (Phase 2) | Should | — |
| SRS-FUNC-018 | 시스템은 DL 기반 virtual dual-energy subtraction(bone suppression)을 제공해야 한다. PSNR≥33dB, SSIM≥0.97 vs real DES. (Phase 3) | Could | HAZ-008 |

### 3.3 Display Processing

| Req ID | Requirement | Priority | Traces to Risk |
|--------|------------|----------|---------------|
| SRS-FUNC-020 | 시스템은 DICOM Modality LUT(Rescale Slope/Intercept)를 적용해야 한다. DICOM tag (0028,1053)/(0028,1052) 준수. | Must | — |
| SRS-FUNC-021 | 시스템은 VOI LUT (LINEAR, LINEAR_EXACT, SIGMOID)를 지원해야 한다. Body-part별 ≥20 preset 사전정의. 실시간 W/L drag(≤16ms). | Must | HAZ-006 |
| SRS-FUNC-022 | 시스템은 DICOM PS3.14 GSDF에 따른 Presentation LUT를 적용해야 한다. P-Value 출력. | Must | HAZ-007 |
| SRS-FUNC-023 | 시스템은 Photometric Interpretation MONOCHROME1/MONOCHROME2를 올바르게 처리해야 한다. | Must | — |

### 3.4 DICOM I/O

| Req ID | Requirement | Priority | Traces to Risk |
|--------|------------|----------|---------------|
| SRS-FUNC-030 | 시스템은 DX IOD (SOP 1.2.840.10008.5.1.4.1.1.1.1) FOR PROCESSING / FOR PRESENTATION을 읽고 쓸 수 있어야 한다. 모든 Type 1/2 tag 포함. | Must | — |
| SRS-FUNC-031 | 시스템은 Grayscale Softcopy Presentation State를 생성 및 적용할 수 있어야 한다. | Must | — |
| SRS-FUNC-032 | 시스템은 JPEG 2000 Lossless 및 Explicit VR Little Endian transfer syntax를 지원해야 한다. | Must | — |

## 4. Input / Output Requirements (b)

| Req ID | Direction | Description |
|--------|-----------|-------------|
| SRS-IO-001 | Input | 14-16 bit raw detector data (binary, detector-specific protocol) |
| SRS-IO-002 | Input | Calibration data (offset map, gain map, bad pixel map) — binary |
| SRS-IO-003 | Input | DICOM image files (DX IOD) |
| SRS-IO-004 | Output | Processed DICOM image (FOR PRESENTATION) — Type 1/2 tags 완전 |
| SRS-IO-005 | Output | Processed DICOM image (FOR PROCESSING) — full bit-depth |
| SRS-IO-006 | Output | DICOM Grayscale Softcopy Presentation State |

## 5. Interface Requirements (c)

| Req ID | Interface | Protocol | Direction |
|--------|-----------|----------|-----------|
| SRS-IF-001 | Detector Interface | USB 3.x / Ethernet (detector SDK) | Input |
| SRS-IF-002 | PACS Interface | DICOM C-STORE SCU | Output |
| SRS-IF-003 | Worklist Interface | DICOM C-FIND SCU (MWL) | Input |
| SRS-IF-004 | GUI Interface | C ABI (DLL export) → C# P/Invoke | Bidirectional |
| SRS-IF-005 | CAD Plugin Interface | REST API + ONNX Runtime (Phase 3) | Bidirectional |

## 6. Alarm, Warning & Operator Messages (d)

| Req ID | Condition | Message Type | Action |
|--------|-----------|:------------:|--------|
| SRS-ALERT-001 | Bad pixel correction failure | Warning | 해당 영역 시각적 표시 + 팝업 |
| SRS-ALERT-002 | W/L 유효 범위 초과 | Warning | UI 경고 표시 |
| SRS-ALERT-003 | GSDF 미보정 display 감지 | Warning | 진단 부적합 경고 |
| SRS-ALERT-004 | DL processing 적용됨 | Info | "AI-processed" label 표시 |
| SRS-ALERT-005 | Calibration data 만료/누락 | Error | 촬영 차단 또는 강한 경고 |
| SRS-ALERT-006 | DICOM write 실패 | Error | 재시도 + 임시 저장 |

## 7. Safety Requirements — Risk Control Measures (5.2.3)

| Req ID | Safety Requirement | Hazard Ref | Control Type |
|--------|-------------------|------------|-------------|
| SRS-SAFE-001 | 시스템은 processing 중 원본 raw data를 보존해야 한다 (비파괴) | HAZ-001 | Design |
| SRS-SAFE-002 | 시스템은 모든 processing parameter 기본값을 body-part별 validated preset으로 설정해야 한다 | HAZ-002 | Design |
| SRS-SAFE-003 | 시스템은 bad pixel correction 실패 시 경고를 표시해야 한다 | HAZ-003 | Alert |
| SRS-SAFE-004 | 시스템은 ghost correction 적용 여부를 DICOM tag에 기록해야 한다 | HAZ-004 | Traceability |
| SRS-SAFE-005 | 시스템은 edge enhancement gain을 body-part별 safe range로 제한해야 한다 | HAZ-005 | Design |
| SRS-SAFE-006 | 시스템은 W/L 유효 범위 초과 시 경고를 표시해야 한다 | HAZ-006 | Alert |
| SRS-SAFE-007 | 시스템은 GSDF 미보정 display에서 경고를 표시해야 한다 | HAZ-007 | Alert |
| SRS-SAFE-008 | 시스템은 DL 처리 결과에 "AI-processed" label을 표시해야 한다 | HAZ-008 | Alert |
| SRS-SAFE-009 | 시스템은 원본/처리 영상 간 즉시 전환 기능을 제공해야 한다 | HAZ-009 | Design |

## 8. Performance Requirements

| Req ID | Requirement | Acceptance Criteria |
|--------|------------|-------------------|
| SRS-PERF-001 | Pre-processing pipeline latency | ≤ 500ms (3072×3072, single-thread) |
| SRS-PERF-002 | Full pipeline latency (Phase 1) | ≤ 3s |
| SRS-PERF-003 | VOI LUT interactive latency | ≤ 16ms (60fps) |
| SRS-PERF-004 | Peak memory per image | ≤ 2 GB |
| SRS-PERF-005 | DICOM write time | ≤ 1s (uncompressed), ≤ 3s (J2K) |
| SRS-PERF-006 | Concurrent processing | ≥ 2 images simultaneously |

## 9. Security Requirements (e)

| Req ID | Requirement |
|--------|------------|
| SRS-SEC-001 | DICOM TLS 1.2+ 지원 (PACS 통신) |
| SRS-SEC-002 | 설정/LUT 파일 무결성 검증 (SHA-256 checksum) |
| SRS-SEC-003 | Processing parameter 변경 audit log 기록 |

## 10. Usability Requirements (f)

| Req ID | Requirement | Reference |
|--------|------------|-----------|
| SRS-USE-001 | W/L 조정은 마우스 드래그로 가능해야 한다 | IEC 62366-1 |
| SRS-USE-002 | Processing preset 전환은 1 click 이내여야 한다 | IEC 62366-1 |
| SRS-USE-003 | 원본/처리 전환은 1 click 또는 keyboard shortcut | IEC 62366-1 |

## 11. Regulatory Requirements (l)

| Req ID | Requirement |
|--------|------------|
| SRS-REG-001 | DICOM PS3.3 DX IOD conformance |
| SRS-REG-002 | DICOM PS3.14 GSDF conformance |
| SRS-REG-003 | IEC 62304 Class B lifecycle compliance |
| SRS-REG-004 | ISO 14971 risk management |
| SRS-REG-005 | FDA 21 CFR 820.30 design controls |

## 12. Requirements Verification Method (5.2.6)

| Criterion | Method |
|-----------|--------|
| Testable | 각 요구사항은 pass/fail 판정 가능한 acceptance criteria 보유 |
| Traceable | XPE-RTM-001에서 architecture → test case 추적 |
| Unique | 고유 ID, 중복 없음 |
| Consistent | 상호 모순 없음 (formal review에서 확인) |
| Review record | 검토자 서명 + 일자 기록 |

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-03 | XPE Team | Initial release (Phase 1 requirements) |

---

*Document End — XPE-SRS-001 v1.0*
