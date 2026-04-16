# Software Hazard Analysis - Panel Defect Correction Module

**Document ID:** SHA-DEFECT-001 v1.0  
**IEC 62304 Clause:** 7 (ISO 14971 integration)  
**Module:** `xpe_preprocess.dll`, Stage 3, Layer 1  
**Safety Classification:** Class B  
**Date:** 2026-04-14  
**Author:** XPE Safety & Risk Management Team  
**Language:** Korean (user-facing), English (technical terms)  
**Approval:** __________________ Date: __________  

---

## 목차

1. [목적 및 범위](#목적-및-범위)
2. [위험 수용성 기준](#위험-수용성-기준)
3. [위험 식별 테이블](#위험-식별-테이블)
4. [위험 통제 매핑](#위험-통제-매핑)

---

## 목적 및 범위

이 문서는 Panel Defect Correction Module (xpe_preprocess.dll, Stage 3, Layer 1)에서 발생 가능한 위험한 상황(Hazardous Situations)을 ISO 14971:2019 의료기기 위험 관리 표준에 따라 식별하고 평가합니다.

**평가 범위**:
- 고립 픽셀 결함 검출/보정 기능
- 클러스터 결함 (3×3, 5×5) ANN 기반 보정
- 라인 결함 (Type 1, 3, 5) 보정
- 그리드/모아레 아티팩트 검출/억제
- 프로필 선택 (Min/Normal/Max)

**제외**:
- Hardware FPD 결함 (detector responsibility)
- Power supply 안정성 (system responsibility)
- Network communication (transport layer)

---

## 위험 수용성 기준

ISO 14971 Annex C 심각도(Severity) × 확률(Probability) 행렬:

| 확률 \ 심각도 | Negligible | Minor | Serious | Critical | Catastrophic |
|:---:|:---:|:---:|:---:|:---:|:---:|
| **Frequent** | Low | Medium | **High** | **Unacceptable** | **Unacceptable** |
| **Probable** | Low | Medium | **High** | **High** | **Unacceptable** |
| **Occasional** | Low | Low | Medium | **High** | **High** |
| **Remote** | Low | Low | Low | Medium | **High** |
| **Improbable** | Low | Low | Low | Low | Medium |

**수용 기준**:
- **수용 가능**: Low 또는 Medium (통제 후)
- **불수용**: High 이상 (위험 경감 필수)

---

## 위험 식별 테이블

### HAZ-DEFECT-001: 검출되지 않은 3×3 클러스터 결함 (Missed Cluster)

| 항목 | 설명 |
|------|------|
| **위험 ID** | HAZ-DEFECT-001 |
| **위험한 상황** | 3×3 클러스터 결함이 동적 검출 및 Static BPM에서 모두 누락 → Morphology 단계에서 분류 실패 → ANN 보정 미실행 → 결함 픽셀이 원본 값 유지 → 영상에 밝은/어두운 픽셀 집합 형성 → 진단 오류 위험 |
| **원인 (소프트웨어)** | k·σ 임계값이 너무 높음 (False Negative), 동적 검출 오류, 정적 BPM 불완전 |
| **영향 SWU** | SWU-3.2 (DynamicDefectDetector), SWU-3.3 (MorphologyClassifier) |
| **발생 시나리오** | (1) 클러스터 신호가 배경 노이즈 근처 → (2) 잔차 맵에서 k·σ > threshold 불만족 → (3) 동적 검출 실패 → (4) Static BPM도 비어있거나 캐시된 이전 데이터 → (5) 검출 안 됨 → (6) 보정 안 됨 → (7) 기술자가 영상 검토 시 인공 무늬 관찰 → (8) False detection 진단 오류 가능성 |
| **해(Harm)** | 진단 오류 (위양성/위음성), 치료 지연, 환자 해(Serious 등급) |
| **심각도** | **Serious** (artifact visible to radiologist, diagnostic impact) |
| **확률 (통제 전)** | Occasional (typical defect detection rate > 90%, but small clusters in noise can be missed) |
| **위험 레벨 (통제 전)** | **Medium** |
| **통제 조치 (SRS)** | **SAF-104**: 결함 보정 후 기울기 균등성 모니터링 (gradient consistency check). 이상 감지 시 경고. 또한 SAF-101: fail-open logging으로 QA 검토 유도. |
| **통제 근거** | k·σ 임계값을 empirically tuned (Normal=4.0)하고, Min/Normal/Max 프로필로 trade-off 제공. Min 모드(k=3.0)에서 높은 감지율. 로깅으로 감시. |
| **잔여 위험** | Low (after control) |

### HAZ-DEFECT-002: 과보정 시 인공 경계 생성 (Overcorrection Artifact)

| 항목 | 설명 |
|------|------|
| **위험 ID** | HAZ-DEFECT-002 |
| **위험한 상황** | ANN 또는 보간 보정이 결함 주변 픽셀에 부자연스러운 기울기 생성 → 경계(edge) 형성 → 거짓 병변(false lesion) 오인 가능 → 진단 오류 |
| **원인 (소프트웨어)** | ANN 가중치 부정확, 보간 기울기 불일치, TMC matching 실패, Type 3 곡선 피팅 오류 |
| **영향 SWU** | SWU-3.4, SWU-3.5 (Cluster Correctors), SWU-3.6 (LineDefectCorrector) |
| **발생 시나리오** | (1) 클러스터 또는 라인 보정 실행 → (2) ANN 추론 또는 곡선 피팅으로 결함 영역 채움 → (3) 보정된 값이 주변 픽셀과 기울기 불일치 → (4) Sharp edge 형성 → (5) 영상에서 인공 경계 가시 → (6) 기술자가 거짓 병변으로 해석 |
| **해(Harm)** | 거짓 양성 진단 (False Positive), 불필요한 추가 영상검사, 환자 불안감, Serious |
| **심각도** | **Serious** (false lesion interpretation risk) |
| **확률 (통제 전)** | Occasional (ANN quality is high in literature, but margin cases exist) |
| **위험 레벨 (통제 전)** | **Medium** |
| **통제 조치 (SRS)** | **SAF-104**: Gradient consistency check after correction. If gradient variance > threshold, apply Type 5 (light smoothing) or fallback to neighbor averaging. **FR-301/302**: ANN NMSE < 0.14/0.20 performance requirement. **SAF-101**: Audit log all corrections for QA review. |
| **통제 근거** | ANN 아키텍처는 문헌(Jeon 2021 PMC7930811)에서 입증. NMSE 기준 설정. 기울기 모니터링으로 과보정 감지/완화. |
| **잔여 위험** | Low (after control) |

### HAZ-DEFECT-003: 놓친 라인 결함 (Missed Line Defect)

| 항목 | 설명 |
|------|------|
| **위험 ID** | HAZ-DEFECT-003 |
| **위험한 상황** | Type 1 또는 Type 3 라인 결함이 보정되지 않음 → 영상에 획(stripe) 또는 줄무늬 artifact 가시 → 해부학적 구조 모호 → 진단 정확도 저하 |
| **원인 (소프트웨어)** | diffVal 계산 오류, Type 1/3 분류 실패 (임계값 T1/T2 부정확), 보정 알고리즘 오류 |
| **영향 SWU** | SWU-3.6 (LineDefectCorrector) |
| **발생 시나리오** | (1) 라인 결함 검출 후 → (2) diffVal 계산 → (3) T1/T2와 비교 → (4) Type 분류 오류 (실제 Type 1인데 Type 5로 분류) → (5) 보정 안 됨 → (6) 라인 stripe 그대로 가시 → (7) 진단 품질 저하 |
| **해(Harm)** | 진단 민감도 감소 (reduced diagnostic quality), 미묘한 병변 놓침 가능성, Serious |
| **심각도** | **Serious** (diagnostic image quality degradation) |
| **확률 (통제 전)** | Occasional (diffVal calculation can be affected by noise near defect boundary) |
| **위험 레벨 (통제 전)** | **Medium** |
| **통제 조치 (SRS)** | **FR-402/403**: Type 1, 3 보정 필수 구현. **FR-401**: diffVal 계산 정확도 검증. **SAF-101**: Audit log에 Type 분류 기록. **FR-406**: 라인 보정 시간 제약으로 알고리즘 복잡도 관리. |
| **통제 근거** | diffVal 공식은 CN104463831A 특허에서 정의됨. Type 1, 3는 필수로 지정 (SAF). Min 모드에서 T1, T2 낮추어 over-detection으로 보정. |
| **잔여 위험** | Low (after control) |

### HAZ-DEFECT-004: 손상된 ANN 가중치 (Corrupted ANN Weights)

| 항목 | 설명 |
|------|------|
| **위험 ID** | HAZ-DEFECT-004 |
| **위험한 상황** | ANN 가중치 파일 손상(bit flip, 불완전한 전송) → 체크섬 검증 실패 또는 undetected 손상 → 잘못된 추론 결과 → 클러스터 보정이 오염된 값 생성 → 영상에 인공 아티팩트 → 진단 오류 |
| **원인 (소프트웨어)** | 파일 시스템 오류, 저장 매체 오류, 불완전한 write during calibration update, CRC 검증 미실행 |
| **영향 SWU** | SWU-3.4, SWU-3.5 (Cluster Correctors), SWU-3.11 (CalibFileIO) |
| **발생 시나리오** | (1) ANN 가중치 로드 → (2) CRC 검증 부재 또는 실패 → (3) 손상된 가중치 사용 → (4) 추론 결과 nonsensical (NaN 또는 극값) → (5) 클러스터 픽셀 값 이상 → (6) 영상 품질 급격히 저하 → (7) Service Engineer 경고 또는 기술자 검사 |
| **해(Harm)** | 광범위 영상 아티팩트, 진단 불가능(unreadable image), Serious/Critical |
| **심각도** | **Critical** (widespread image degradation) |
| **확률 (통제 전)** | Remote (modern SSD/HDD have ECC, but risk is non-zero) |
| **위험 레벨 (통제 전)** | **Low** |
| **통제 조치 (SRS)** | **SAF-203**: MD5/SHA-256 해시로 ANN 가중치 무결성 검증. **SAF-301**: 계산 오버플로우/NaN 감지 시 fallback to neighbor averaging. **SAF-202**: 가중치 손상 감지 시 에러 로그. |
| **통제 근거** | 해시 검증으로 손상 감지 (deterministic). Fallback은 graceful degradation. |
| **잔여 위험** | Low (after control) |

### HAZ-DEFECT-005: 그리드 억제 실패 (Grid Suppression Failure)

| 항목 | 설명 |
|------|------|
| **위험 ID** | HAZ-DEFECT-005 |
| **위험한 상황** | DWT 또는 DCT 필터가 그리드 성분을 제대로 억제하지 못함 → 모아레 artifact 남음 → 해부학적 세부 모호 → 진단 정확도 저하 |
| **원인 (소프트웨어)** | DWT 분해 레벨 부정확, 필터 주파수 중심 오류, 동적 분할(DCT) 세그먼트 크기 부적절 |
| **영향 SWU** | SWU-3.7, SWU-3.8 (GridMoireDetector, GridMoireSuppressor) |
| **발생 시나리오** | (1) DWT 분해 → (2) 그리드 주파수 식별 오류 → (3) 필터 design 부정확 → (4) 그리드 에너지 불완전 억제 → (5) MSI 개선 불충분 → (6) 영상에서 모아레 여전히 가시 |
| **해(Harm)** | 이미지 품질 저하 (mild, usually tolerable), 진단 민감도 약간 감소, Minor |
| **심각도** | **Minor** (moiré is visual artifact but not directly diagnostic-critical) |
| **확률 (통제 전)** | Occasional (grid frequency varies by system, aliasing can be complex) |
| **위험 레벨 (통제 전)** | **Low** |
| **통제 조치 (SRS)** | **PERF-506**: Grid suppression output MSI < 0.1 (Low target). **FR-506**: 후처리 MSI 검증. **SAF-101**: 그리드 억제 결과 로그 (MSI 값 기록). Min 모드: 강력한 필터(2×). |
| **통제 근거** | MSI 임계값 and filter strength는 문헌(Tang 2012)에 기반. 모드별 조정으로 trade-off 관리. |
| **잔여 위험** | Low (after control) |

### HAZ-DEFECT-006: BPM 파일 손상 (Corrupted BPM File)

| 항목 | 설명 |
|------|------|
| **위험 ID** | HAZ-DEFECT-006 |
| **위험한 상황** | Static BPM 파일이 손상됨 → 잘못된 픽셀이 결함으로 표시 또는 실제 결함이 누락 → 보정 오류 → 영상 artifact 또는 결함 가시 → 진단 오류 |
| **원인 (소프트웨어)** | 저장 매체 오류, 불완전한 파일 transfer, calibration update 실패 |
| **영향 SWU** | SWU-3.1 (StaticBPMGenerator), SWU-3.11 (CalibFileIO) |
| **발생 시나리오** | (1) BPM 파일 로드 → (2) CRC 검증 실패 또는 미실행 → (3) 손상된 BPM 사용 → (4) 잘못된 픽셀 보정 또는 누락 → (5) 영상 artifact |
| **해(Harm)** | 진단 오류, 영상 품질 저하, Serious |
| **심각도** | **Serious** |
| **확률 (통제 전)** | Remote (storage failure is rare) |
| **위험 레벨 (통제 전)** | **Low** |
| **통제 조치 (SRS)** | **SAF-203**: CRC-32 checksum 검증 모든 calibration 파일. **SAF-102**: BPM 로드 실패 시 캐시된 이전 BPM 사용 (fail-safe). 모든 경우 로그. |
| **통제 근거** | CRC-32 검증은 deterministic 손상 감지. Fallback caching은 서비스 연속성 보장. |
| **잔여 위험** | Low (after control) |

### HAZ-DEFECT-007: 프로필 선택 오류 (Incorrect Profile Selection)

| 항목 | 설명 |
|------|------|
| **위험 ID** | HAZ-DEFECT-007 |
| **위험한 상황** | Operator 또는 시스템이 잘못된 프로필(예: Max 모드 임상 사용)로 보정 실행 → 공격적인 결함 검출 스킵 → 결함 미보정 → 영상 artifact → 진단 오류 |
| **원인 (소프트웨어)** | 프로필 선택 UI 혼동, 자동 프로필 전환 로직 오류 |
| **영향 SWU** | SWU-3.10 (ProfileManager), SWU-3.2 (DynamicDefectDetector) |
| **발생 시나리오** | (1) Operator가 Max 모드 실수로 선택 → (2) 높은 임계값(k=5.0) 적용 → (3) 많은 결함이 undetected → (4) 보정 미실행 → (5) artifact 가시 |
| **해(Harm)** | 영상 품질 저하, 진단 오류, Serious |
| **심각도** | **Serious** |
| **확률 (통제 전)** | Remote (operator training, but risk exists) |
| **위험 레벨 (통제 전)** | **Low** |
| **통제 조치 (SRS)** | **FR-701**: 프로필 선택 API 및 UI. **SAF-101**: 모든 프로필 변경과 적용을 감시 로그. **FR-704**: Max 모드 목적과 제약 문서화 (panel lifespan priority). **SAF-202**: 메타데이터에 사용 프로필 기록. |
| **통제 근거** | 명확한 프로필 정의, 로깅, 메타데이터 기록으로 추적성 확보. |
| **잔여 위험** | Low (after control) |

### HAZ-DEFECT-008: 동적 검출 거짓 양성 (False Positive Detection)

| 항목 | 설명 |
|------|------|
| **위험 ID** | HAZ-DEFECT-008 |
| **위험한 상황** | 건강한 조직이 오염된 결함(false positive)으로 표시 → 불필요한 보정 시도 → 정상 texture 손상 → 진단 정보 손실 → 진단 오류 |
| **원인 (소프트웨어)** | k·σ 임계값이 너무 낮음 (False Positive), 노이즈 샘플이 이상적이지 않음 |
| **영향 SWU** | SWU-3.2 (DynamicDefectDetector) |
| **발생 시나리오** | (1) Min 모드에서 k=3.0 (낮은 임계값) → (2) 높은 신호 지역의 정상 변동이 임계값 초과 → (3) False positive로 표시 → (4) 정상 픽셀 보정 시도 → (5) 영상 texture 손상 |
| **해(Harm)** | 정상 해부학적 세부 손상(artifact of correction), 영상 정보 손실, Minor/Serious |
| **심각도** | **Minor** (false positives usually minor visual impact) |
| **확률 (통제 전)** | Occasional (Min mode is aggressive, false positives expected) |
| **위험 레벨 (통제 전)** | **Low** |
| **통제 조치 (SRS)** | **FR-202**: 오탐율 < 5% requirement (detection validation). **SAF-104**: Gradient consistency check를 통해 부자연스러운 보정 감지/완화. **SAF-101**: 모든 동적 검출 결과 로그. |
| **통제 근거** | 오탐율 요구사항으로 기준 설정. 기울기 모니터링으로 보정 아티팩트 방지. |
| **잔여 위험** | Low (after control) |

---

## 위험 통제 매핑

### SRS ↔ Hazard ↔ Test Traceability

| 위험 ID | SRS 통제 요구 | 검증 방법 | 테스트 케이스 |
|---------|-------------|---------|-------------|
| **HAZ-DEFECT-001** | **SAF-104** (gradient check) | Integration test: gradient consistency validation | IT-001: 작은 클러스터 false negative 시나리오 |
| **HAZ-DEFECT-002** | **SAF-104**, **FR-301/302** (NMSE) | Unit test: ANN output validation, gradient analysis | UT-3.4-001: ANN 추론 + gradient check |
| **HAZ-DEFECT-003** | **FR-402/403**, **FR-401** | Integration test: diffVal calculation, Type classification | IT-003: Type 1 vs. Type 3 분류 정확도 |
| **HAZ-DEFECT-004** | **SAF-203** (hash verification) | Unit test: CRC/hash check | UT-3.11-001: ANN weights hash validation |
| **HAZ-DEFECT-005** | **PERF-506**, **SAF-101** (MSI logging) | Integration test: MSI < 0.1 verification | IT-005: Grid suppression MSI target |
| **HAZ-DEFECT-006** | **SAF-203**, **SAF-102** (caching) | Unit test: BPM CRC, fallback logic | UT-3.11-002: BPM corruption detection |
| **HAZ-DEFECT-007** | **FR-701**, **SAF-101** (logging) | System test: profile selection UI, audit trail | ST-007: Profile change logging |
| **HAZ-DEFECT-008** | **FR-202** (FP < 5%), **SAF-104** | Unit test: k·σ false positive rate | UT-3.2-001: False positive detection validation |

---

**Document Version**: 1.0  
**Total Hazards Identified**: 8  
**Residual Risk Level**: All Low (after controls)  
**Last Updated**: 2026-04-14  
**Next**: RTM-DEFECT-001 (Requirements Traceability Matrix)
