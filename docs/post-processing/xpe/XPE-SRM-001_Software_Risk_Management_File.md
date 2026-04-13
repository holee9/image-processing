# Software Risk Management File

**Document ID:** XPE-SRM-001 v1.0  
**IEC 62304 Clause:** 7.1 — 7.4  
**Companion Standard:** ISO 14971:2019  
**Safety Classification:** Class B  
**Date:** 2026-04-03  
**Author:** XPE Development Team  
**Approval:** __________________ Date: __________  

---

## 1. Purpose

XPE 소프트웨어의 hazardous situation 식별, risk control, verification 및 SOUP risk를 관리한다.

## 2. Risk Acceptability Criteria

ISO 14971 Annex C 기반 risk matrix:

| | Negligible | Minor | Serious | Critical | Catastrophic |
|---|:-:|:-:|:-:|:-:|:-:|
| **Frequent** | Low | Medium | **High** | **Unacceptable** | **Unacceptable** |
| **Probable** | Low | Medium | **High** | **High** | **Unacceptable** |
| **Occasional** | Low | Low | Medium | **High** | **High** |
| **Remote** | Low | Low | Low | Medium | **High** |
| **Improbable** | Low | Low | Low | Low | Medium |

- **Unacceptable:** 절대 출시 불가
- **High:** Risk control 필수, ALARP 원칙 적용
- **Medium:** Risk control 권장, benefit-risk 평가
- **Low:** Acceptable, 모니터링

## 3. Hazard Identification (7.1)

| HAZ ID | Hazardous Situation | Sequence of Events | Severity | Pre-control Prob | Pre-control Risk |
|--------|-------------------|-------------------|:--------:|:----------------:|:----------------:|
| HAZ-001 | 원본 영상 손실 → 재촬영(추가 피폭) | Processing이 원본 overwrite → 재처리 불가 → 재촬영 필요 | Minor | Occasional | **Medium** |
| HAZ-002 | 부적절한 processing으로 진단 정보 손실 | Default preset 오류 → 과도한/부족한 처리 → 미묘한 병변 비가시 | Serious | Occasional | **Medium** |
| HAZ-003 | 미보정 bad pixel → 병변 오인 | Bad pixel map 미갱신 → 밝은/어두운 점 잔존 → 의사가 병변으로 오진 | Serious | Remote | **Low** |
| HAZ-004 | Ghost artifact → 병변 오인 | Lag correction 미적용 → 이전 이미지 잔상 → 허위 구조물 | Serious | Occasional | **Medium** |
| HAZ-005 | 과도한 edge enhancement → 허위 구조물 | Gain 과다 → overshoot halo → 미세 골절/결절로 오인 | Serious | Occasional | **Medium** |
| HAZ-006 | 부적절한 W/L → 병변 비가시 | W/L preset 오류 또는 사용자 실수 → 진단 영역 밖 window | Serious | Probable | **High** |
| HAZ-007 | GSDF 미준수 display → contrast 왜곡 | 비보정 모니터 → GSDF 미적용 → 미묘한 density 차이 비가시 | Minor | Occasional | **Low** |
| HAZ-008 | AI가 병변 제거 또는 허위 생성 | DL bone suppression이 nodule 함께 제거, 또는 artifact 생성 | Serious | Occasional | **Medium** |
| HAZ-009 | Processing 상태 혼동 | 원본 vs 처리 영상 구분 불가 → AI 결과를 원본으로 오인 | Minor | Probable | **Medium** |

## 4. Risk Control Measures (7.2)

| HAZ ID | Risk Control | Type | Implementation | SRS Req |
|--------|-------------|------|---------------|---------|
| HAZ-001 | Non-destructive processing (원본 보존) | Inherent safety | Read-only input buffer, separate output allocation | SRS-SAFE-001 |
| HAZ-002 | Validated body-part preset 자동 적용 | Inherent safety | ParameterValidator + auto-selection | SRS-SAFE-002 |
| HAZ-003 | Defect correction 실패 시 시각적 경고 | Protective measure | ErrorHandler → UI overlay + alert | SRS-SAFE-003 |
| HAZ-004 | Ghost correction 적용 여부 DICOM 기록 | Information for safety | Metadata tag in DICOM output | SRS-SAFE-004 |
| HAZ-005 | Enhancement gain safe-range 제한 | Inherent safety | ParameterValidator max cap per body-part | SRS-SAFE-005 |
| HAZ-006 | W/L 유효 범위 초과 경고 | Protective measure | Range check + UI warning | SRS-SAFE-006 |
| HAZ-007 | GSDF 미보정 display 경고 | Information for safety | Startup GSDF compliance check | SRS-SAFE-007 |
| HAZ-008 | AI-processed label + toggle | Information for safety | "AI-processed" overlay + original 전환 | SRS-SAFE-008, 009 |
| HAZ-009 | Processing state indicator + toggle | Protective measure | UI state display + 1-click toggle | SRS-SAFE-009 |

## 5. Post-Control Risk Evaluation

| HAZ ID | Post-control Severity | Post-control Prob | Post-control Risk | Acceptable? |
|--------|:--------------------:|:-----------------:|:-----------------:|:-----------:|
| HAZ-001 | Minor | Improbable | **Low** | ✓ |
| HAZ-002 | Serious | Remote | **Low** | ✓ |
| HAZ-003 | Minor (경고로 인식) | Remote | **Low** | ✓ |
| HAZ-004 | Minor (기록으로 추적) | Remote | **Low** | ✓ |
| HAZ-005 | Minor (gain 제한됨) | Remote | **Low** | ✓ |
| HAZ-006 | Minor (경고로 인식) | Occasional | **Low** | ✓ |
| HAZ-007 | Negligible | Occasional | **Low** | ✓ |
| HAZ-008 | Minor (label + toggle) | Occasional | **Low** | ✓ |
| HAZ-009 | Negligible | Remote | **Low** | ✓ |

## 6. Risk Control Verification (7.3)

| HAZ ID | Verification Method | Test ID | Expected Result | Status |
|--------|-------------------|---------|-----------------|:------:|
| HAZ-001 | Raw byte comparison after processing | ST-SAFE-001 | Byte-identical | Planned |
| HAZ-002 | All body-part preset validation | ST-SAFE-002 | All within safe range | Planned |
| HAZ-003 | Forced defect correction failure | ST-SAFE-003 | Warning within 2s | Planned |
| HAZ-004 | DICOM tag presence check | ST-004 | Tag present & correct | Planned |
| HAZ-005 | Max gain visual artifact check | ST-013 | No overshoot > 5% | Planned |
| HAZ-006 | W/L out-of-range injection | ST-SAFE-006 | Warning displayed | Planned |
| HAZ-007 | Non-GSDF display simulation | ST-SAFE-007 | Warning displayed | Planned |
| HAZ-008 | AI label presence test | ST-SAFE-008 | Label visible | Planned |
| HAZ-009 | Toggle function timing | ST-SAFE-009 | Switch < 100ms | Planned |

## 7. SOUP Risk Management (7.4)

| SOUP ID | Name | Potential Failure Mode | Impact | Mitigation | Verification |
|---------|------|----------------------|--------|-----------|-------------|
| SOUP-001 | OpenCV | Filter incorrect output | Image quality degradation | Output PSNR check vs reference | IT-008 |
| SOUP-002 | dcmtk | DICOM tag mishandling | Non-conformant output | DVTk conformance in CI | ST-030 |
| SOUP-003 | ONNX Runtime | Inference NaN/Inf/crash | AI module failure | Output validation + fallback | IT-005 |
| SOUP-008 | Eigen | Numerical instability | MFP artifact | Condition number check | UT-2.5-008 |

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-03 | XPE Team | Initial release |

---

*Document End — XPE-SRM-001 v1.0*
