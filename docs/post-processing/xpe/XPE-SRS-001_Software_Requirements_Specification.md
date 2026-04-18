# Software Requirements Specification

**Document ID:** XPE-SRS-001 v1.1  
**IEC 62304 Clause:** 5.2.1 — 5.2.6  
**Safety Classification:** Class B  
**Date:** 2026-04-16  
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
| SRS-FUNC-024 | 시스템 GUI는 원본/처리 영상을 하나의 동기화된 비교 viewport에서 표시해야 하며 swipe, split, overlay, difference, source-only, processed-only 모드를 제공해야 한다. | Must | HAZ-009 |

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
| SRS-SAFE-013 | 시스템은 처리 영상을 갱신하더라도 원본 영상 또는 원본 참조를 덮어쓰지 않아야 한다 | HAZ-009 | Design |

## 8. Performance Requirements

| Req ID | Requirement | Acceptance Criteria |
|--------|------------|-------------------|
| SRS-PERF-001 | Pre-processing pipeline latency | ≤ 500ms (3072×3072, single-thread) |
| SRS-PERF-002 | Full pipeline latency (Phase 1) | ≤ 3s |
| SRS-PERF-003 | VOI LUT interactive latency | ≤ 16ms (60fps) |
| SRS-PERF-004 | Peak memory per image | ≤ 2 GB |
| SRS-PERF-005 | DICOM write time | ≤ 1s (uncompressed), ≤ 3s (J2K) |
| SRS-PERF-006 | Concurrent processing | ≥ 2 images simultaneously |
| SRS-PERF-007 | GUI comparison viewport comfort envelope | 4096×4096 UInt16 source + processed in one synchronized viewport |
| SRS-PERF-008 | GUI comparison interaction | zoom/pan/swipe without full pipeline reprocessing |

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
| SRS-USE-004 | 원본/처리 비교는 기본적으로 swipe/wiper slider 방식으로 제공되어야 한다 | IEC 62366-1 |
| SRS-USE-005 | 대용량 영상 검토를 위해 zoom fit, 100%, zoom in/out, pan, optional detach viewer를 제공해야 한다 | IEC 62366-1 |

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

## 13. Common Foundation Requirements (Phase 0)

### 13.1 Auto Exposure Detection (AED) Requirements

| Req ID | Requirement | Priority | SDD Trace |
|--------|------------|----------|-----------|
| SRS-FUNC-040 | 시스템은 Auto Exposure Detection (AED) 기능을 제공해야 한다. AED는 detector의 노출 이벤트를 감지하고 XPE_STATUS_NO_EVENT (= 1) 코드를 반환하여 폴링 방식으로 이벤트를 제공해야 한다. **참고**: AED는 detector 고유 기능이며, 일반적인 알림/이벤트 디스패치 인프라와는 독립적이다. | Must | SDD-001 §6.4 |
| SRS-FUNC-041 | Auto Exposure Detection (AED) 설정은 JSON configuration string으로 지원해야 한다. NULL 입력 시 default 설정 사용. enable/disable flag, dose threshold, cooldown period, callback mode selection을 포함해야 한다. | Must | SDD-001 §6.4 |
| SRS-FUNC-042 | Auto Exposure Detection (AED) 상태 기계는 3가지 상태를 지원해야 한다: 0=IDLE (미설정), 1=ARMED (노출 대기 중), 2=TRIGGERED (노출 감지). xpe_aed_get_status 함수로 현재 상태 조회 가능해야 한다. | Must | SDD-001 §6.4 |

### 13.2 Logging Subsystem Requirements

| Req ID | Requirement | Priority | SDD Trace |
|--------|------------|----------|-----------|
| SRS-FUNC-043 | 시스템은 6단계 logging subsystem를 제공해야 한다. TRACE=0, DEBUG=1, INFO=2, WARN=3, ERROR=4, OFF=5. xpe_log_set_level 함수로 레벨 조정 가능해야 한다. | Must | SDD-001 §6.5 |
| SRS-FUNC-044 | 로그 출력은 stderr (기본) 또는 file path로 redirect 가능해야 한다. xpe_log_set_file 함수로 파일 경로 지정. NULL 입력 시 stderr로 복귀. | Must | SDD-001 §6.5 |
| SRS-FUNC-045 | 시스템은 강제 log flush 기능을 제공해야 한다. xpe_log_flush 함수로 버퍼링된 로그 메시지를 즉시 출력 대상에 기록해야 한다. | Must | SDD-001 §6.5 |

### 13.3 xpe_common.dll API Requirements

| Req ID | Requirement | Priority | SDD Trace |
|--------|------------|----------|-----------|
| SRS-FUNC-046 | xpe_common.dll은 정확히 18개의 C API 함수를 export해야 한다. Lifecycle (3): xpe_init, xpe_shutdown, xpe_version. Configuration (2): xpe_configure, xpe_get_param_range. Error/Alert (4): xpe_error_string, xpe_get_pending_alert_count, xpe_get_pending_alert, xpe_clear_alerts. Logging (3): xpe_log_set_level, xpe_log_set_file, xpe_log_flush. Auto Exposure Detection (3): xpe_aed_configure, xpe_aed_poll_event, xpe_aed_get_status. Memory (3): xpe_alloc_image, xpe_free_image, xpe_copy_image. | Must | SDD-001 §6 |

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-03 | XPE Team | Initial release (Phase 1 requirements) |
| 1.1 | 2026-04-16 | XPE Team | Auto Exposure Detection (AED) 요구사항 추가, Logging 요구사항 추가, xpe_common API 함수 카운트 18개로 업데이트, EI baseline 단계 재배치 반영 |
| 1.2 | 2026-04-18 | XPE Team | AED 용어 정리: "AED subsystem" → "Auto Exposure Detection (AED) 기능" (detector 고유 기능 명확화, SW 서브시스템 명칭 혼용 제거) |

---

*Document End — XPE-SRS-001 v1.0*
