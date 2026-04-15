# AI Regulatory Strategy

**Document ID**: XPE-AI-REG-001  
**Version**: 1.0.0  
**Date**: 2026-04-15  
**Status**: Controlled Draft  
**Parent**: XPE-PRD-SYSTEM-001, XPE-MRD-001, XPE-REG-BOUNDARY-001  
**Cross-reference**: XPE-SHA-001 §3 HAZ-008/010/012; XPE-SVVP-001 §5/§6

---

## 1. Purpose

본 문서는 XPE 소프트웨어에 포함된 AI/ML 기능의 규제 분류, 허가 전략, 라벨링 요구사항, 사후 시장 감시 계획을 정의한다.

**Scope**: Phase 3 AI 기능 (SWU-2.11 BoneSuppressionEngine, SWU-2.12 DLDenoiser, SWI-1 BodyPartClassifier) 전체. Phase 1/2 결정론적 기능은 별도 규제 track을 따른다.

**Relationship to XPE-REG-BOUNDARY-001**: 본 문서는 XPE-REG-BOUNDARY-001이 정의한 "research-gated assistive" 분류를 상세 규제 요구사항으로 구체화한다.

---

## 2. AI Feature Classification

### 2.1 Intended Use Statement (전체 시스템)

XPE 소프트웨어는 **FPD(Flat Panel Detector) 기반 X-ray 시스템을 위한 영상 후처리 소프트웨어**로서, 의료 전문가가 진단 목적으로 사용할 영상의 화질을 향상시키는 보조 도구이다.

- XPE는 진단 결론을 생성하지 않는다.
- 모든 최종 진단 판단은 자격을 갖춘 의료 전문가(방사선사, 방사선과 전문의)가 수행한다.
- XPE AI 기능은 **Assistive Only** — 진단 의사결정을 대체하지 않는다.

### 2.2 AI Feature Classification Table

| Feature | Software Unit | Classification | Regulatory Category | Claim |
|---------|--------------|----------------|--------------------|----|
| Body-part auto recognition | BodyPartClassifier | **Assistive** | Non-SaMD (image processing aid) | "Automatic body-part parameter selection, operator-overridable" |
| AI collimation refinement | CollimationEstimator (Phase 3) | **Assistive** | Non-SaMD | "Suggested collimation boundary, requires operator confirmation" |
| DL bone suppression | BoneSuppressionEngine | **Assistive** | Non-SaMD | "Bone structure visualization aid; original toggle mandatory" |
| DL denoising | DLDenoiser | **Assistive** | Non-SaMD | "Image noise reduction; AI-processed label required" |
| Pathology-aware enhancement | — (Phase 3+, on-hold) | **Regulatory Hold** | Potential SaMD Class II | Not claimed in current release |
| ALARA advisor | — (Phase 3+, on-hold) | **Regulatory Hold** | Potential SaMD Class II | Not claimed in current release |

### 2.3 Non-SaMD Justification (Assistive Features)

XPE Phase 3 AI 기능이 FDA SaMD 정의를 충족하지 않는 이유:

1. **의도된 사용 목적**: 진단 결정 제공 아님 — 영상 화질 향상 보조 도구
2. **출력 유형**: 처리된 영상(processed image) 또는 파라미터 제안 — 임상 판단 결과 아님
3. **의사결정 체인**: AI 출력이 직접 진단에 사용되지 않음; 방사선사가 원본/처리 영상 모두 열람 가능
4. **Fallback 보장**: AI 기능 비활성화 또는 실패 시 Phase 1/2 결정론적 출력으로 자동 전환 (PR-SAFE-002)

> **[NOTE]**: AI 기능의 claim이 진단 보조(diagnostic decision support)로 확장될 경우, 즉시 별도 FDA 510(k) 또는 De Novo 신청이 필요하다. 현재 claim 범위를 초과하는 기능 추가 시 XPE-AI-REG-001을 개정하고 규제 검토를 선행해야 한다.

---

## 3. PICOS Definitions (AI 기능별)

PICOS(Population, Intervention, Comparison, Outcome, Setting) 정의는 AI 기능의 임상적 맥락을 명확히 하여 라벨링, 훈련, 임상 검증에 활용한다.

### 3.1 DL Bone Suppression (SWU-2.11)

| PICOS Element | Definition |
|--------------|------------|
| **Population** | 흉부 X-ray(PA/AP) 촬영 대상 성인 환자; FPD 기반 시스템 |
| **Intervention** | DL bone suppression 알고리즘 적용 — 늑골/쇄골 구조 시각적 감쇠 |
| **Comparison** | 미적용 원본 영상 (always available via 1-click toggle) |
| **Outcome** | 폐 실질 가시성 향상; 미세 결절/간질성 변화 인식 보조 |
| **Setting** | 방사선과 판독실 또는 동등 환경; 의료 등급 모니터; 자격 있는 방사선사 운용 |

### 3.2 DL Denoising (SWU-2.12)

| PICOS Element | Definition |
|--------------|------------|
| **Population** | 저선량 X-ray 촬영 영상 (특히 소아 또는 반복 촬영 케이스) |
| **Intervention** | DL 기반 noise 감쇠 — structured/quantum noise 동시 처리 |
| **Comparison** | 미적용 원본 영상 (toggle 제공) |
| **Outcome** | 신호 대 잡음비 향상, edge 정보 보존, 진단 가치 있는 구조 가시성 향상 |
| **Setting** | 저선량 프로토콜 사용 시설; 소아 전문 또는 일반 방사선과 |

### 3.3 Body-Part Auto Recognition (BodyPartClassifier)

| PICOS Element | Definition |
|--------------|------------|
| **Population** | 표준 체위(PA 흉부, AP 복부, 사지 등) FPD X-ray 영상 |
| **Intervention** | CNN 기반 body-part 자동 분류 → 최적 processing preset 자동 선택 |
| **Comparison** | 수동 body-part 선택 (operator override 항상 가능) |
| **Outcome** | 체위별 최적화된 processing parameter 자동 적용으로 일관된 화질 제공 |
| **Setting** | 일반 방사선과, 응급실, 정형외과 등 다목적 FPD 운용 환경 |

---

## 4. Training & Evaluation Data Provenance

### 4.1 Data Requirements

| Requirement | Description |
|------------|-------------|
| **Source diversity** | 최소 3개 기관 이상, 2개 FPD 제조사 이상의 영상 포함 |
| **Demographic coverage** | 연령(소아/성인/노인), 체형(BMI 분포), 성별 균형 |
| **Pathology coverage** | 정상 + 다양한 병변(결절, 골절, 삼출액 등) 포함; 클래스 불균형 문서화 |
| **Ground truth** | 자격 있는 방사선과 전문의 2인 이상 독립 판독; 불일치 시 제3자 중재 |
| **Data provenance** | 각 데이터셋: 기관명, IRB 승인 번호, 익명화 방법, 수집 연월 기록 |
| **OOD boundary** | 훈련 데이터 범위 외 입력(특수 체위, 소아과 특수 장비 등) 명시; confidence threshold 0.70 미만 시 UNKNOWN 분류 |

### 4.2 Model Version Traceability

각 AI 모델 릴리즈는 다음을 기록해야 한다:

- **Model ID**: `model_name_vX.Y.Z` (semantic versioning)
- **Training dataset hash**: SHA-256 of dataset manifest file
- **Validation dataset hash**: SHA-256 of holdout set
- **Performance metrics**: AUC, sensitivity/specificity at operating point
- **OOD test results**: confidence distribution on known-OOD samples
- **Approval date and approver**

이 정보는 SOUP(Software of Unknown Provenance) 문서 또는 별도 AI Model Registry에 기록한다.

---

## 5. Labeling & IFU Requirements

### 5.1 Mandatory Labels (AI 기능 활성화 시 항상 표시)

| Label | Location | Content |
|-------|----------|---------|
| AI-processed indicator | Image overlay (persistent) | "AI: [기능명] 적용됨" — 항상 가시적 |
| Original toggle button | Viewer UI | 1-click으로 원본/처리 영상 전환 |
| Confidence indicator | Image overlay or metadata panel | Body-part classification: 신뢰도 % 표시; UNKNOWN 분류 시 경고 |
| Assistive-only disclaimer | Startup / About screen | "본 AI 기능은 보조 도구입니다. 최종 진단 판단은 자격 있는 의료 전문가가 수행해야 합니다." |

### 5.2 IFU (Instructions for Use) Requirements

IFU는 다음 내용을 포함해야 한다:

1. **AI 기능 설명**: 각 기능의 목적, 작동 원리(요약), 한계
2. **Intended Use 명시**: Non-diagnostic assistive tool
3. **Contraindications**: 특수 체위/비표준 FPD에서의 신뢰도 저하 경고
4. **Training requirement** (IFU-TRAIN-001): 사용자는 AI 기능 활성화 전 다음 교육 이수 필요:
   - AI 처리 영상과 원본 영상 구분 방법
   - Original toggle 기능 사용법
   - AI confidence indicator 해석법
   - AI 결과가 의사결정에 미치는 영향 이해
5. **Post-market 보고 경로**: 예상치 못한 AI 동작 발견 시 보고 방법

### 5.3 Labeling Claim Statements

| Feature | Allowed Claim | Prohibited Claim |
|---------|--------------|------------------|
| Bone suppression | "Bone structure visualization aid for chest X-ray" | "Improves lesion detection rate", "Reduces missed findings" |
| DL denoising | "AI-assisted noise reduction for FPD images" | "Equivalent to full-dose quality at reduced dose" |
| Body-part recognition | "Automatic parameter optimization for standard projections" | "Eliminates operator error", "Fully automatic diagnostic workflow" |

---

## 6. Regulatory Pathway by Market

### 6.1 FDA (United States)

| Scenario | Pathway | Notes |
|---------|---------|-------|
| Current Phase 3 AI (assistive, non-SaMD) | **510(k) exemption** or **General Controls** under 21 CFR 880.3710 (general radiology device) | Maintain Non-SaMD status by preserving assistive-only claim |
| Future diagnostic AI (HAZ-012 scenario, pathology detection) | **De Novo** or **510(k)** as SaMD Class II (21 CFR Part 892) | Requires separate FDA AI/ML action plan submission |
| Predetermined Change Control Plan (PCCP) | File with initial 510(k) if AI model updates are planned | Allows iterative model updates without new 510(k) |

### 6.2 EU MDR (Europe)

| Scenario | Classification | Notes |
|---------|---------------|-------|
| Current Phase 3 AI (assistive) | **Class IIa** or **Class IIb** under MDR Annex VIII Rule 11 (software) | AI performing analysis of medical images typically Class IIa minimum |
| Diagnostic AI | **Class IIb** or **Class III** | Requires Notified Body involvement |

> **[ACTION REQUIRED]**: EU MDR 정식 분류는 Notified Body 자문 후 확정. 현재 Class IIa 가정 하에 기술 문서 준비 진행.

### 6.3 국내 (Korea MFDS)

| Scenario | Classification | Notes |
|---------|---------------|-------|
| AI assistive image processing | **2등급 의료기기** (소프트웨어) | 의료기기 소프트웨어 허가심사 가이드라인 적용 |
| 진단 보조 소프트웨어 | **3등급 의료기기** 가능 | AI 기반 의료기기 허가심사 가이드라인(2020) 별도 적용 |

---

## 7. Post-Market Surveillance Plan (AI 기능)

### 7.1 Monitoring Metrics

| Metric | Collection Method | Review Frequency | Threshold for Action |
|--------|------------------|------------------|---------------------|
| AI feature usage rate | Application telemetry (anonymous) | Monthly | Significant drop → investigate |
| User override rate (original toggle) | Application telemetry | Monthly | >30% override rate → model review |
| Adverse event reports (AI-related) | IFU reporting channel + MDR vigilance | Per event | Any report → immediate review |
| OOD detection rate | Model inference logs | Monthly | >5% UNKNOWN → dataset review |
| Confidence score distribution drift | Statistical monitoring | Quarterly | Distribution shift → model validation |

### 7.2 Model Update Policy

AI 모델 업데이트는 다음 분류에 따라 처리한다:

| Update Type | Regulatory Action | Required Evidence |
|------------|-------------------|------------------|
| **Minor** (bug fix, same architecture) | Document in SOUP record, no new 510(k) | Internal validation on holdout set |
| **Moderate** (same task, improved performance) | PCCP review (if filed) or 510(k) supplement | Validation against original performance benchmark |
| **Major** (new task, new architecture, new claim) | New 510(k) / De Novo | Full clinical validation per PICOS |

### 7.3 Feedback Loop

현장 피드백 → XPE-SPR-001(소프트웨어 문제 보고) → AI 모델 평가 → 수정 릴리즈 또는 IFU 업데이트 결정.

---

## 8. Traceability

| AI Regulatory Item | Source Requirement | Implementation | Test/Validation |
|-------------------|--------------------|---------------|-----------------|
| Assistive-only claim | MR-REG-005, PR-SAFE-001 | IFU-TRAIN-001, SRS-SAFE-008 | UV-002, UV-003 (usability) |
| Mandatory AI label | PR-SAFE-003 | SWU-3.3 PresentationLUT | ST-SAFE-008 |
| Original toggle | PR-SAFE-004 | SWU-3.3 + SWU-5.7 | ST-SAFE-009 |
| Confidence display | PR-SAFE-005, SRS-SAFE-010 | AI Router + SWU-2.11/2.12 | ST-SAFE-010 |
| Fallback on AI failure | PR-SAFE-002, SRS-SAFE-011 | SWU-5.7 PipelineOrchestrator | ST-SAFE-011 |
| OOD detection | PR-SAFE-006 | BodyPartClassifier (threshold 0.70) | ST-AI-OOD-001 |
| IFU training requirement | HAZ-012 control (IFU-TRAIN-001) | IFU document | UV-002 (summative usability) |

---

## 9. Open Items

| ID | Item | Priority | Owner |
|----|------|----------|-------|
| AI-REG-OPEN-001 | EU MDR Notified Body 자문 → Class IIa/IIb 확정 | High | Regulatory Affairs |
| AI-REG-OPEN-002 | FDA PCCP(Predetermined Change Control Plan) 작성 여부 결정 | Medium | Regulatory Affairs |
| AI-REG-OPEN-003 | AI Model Registry 문서 양식 수립 (Model ID, dataset hash 관리) | High | QA + Engineering |
| AI-REG-OPEN-004 | 국내 MFDS 2등급 허가 신청 준비 시점 결정 | Medium | Regulatory Affairs |

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0.0 | 2026-04-15 | XPE Team | Initial release — OPEN-004 resolution. AI feature classification, PICOS definitions, training data provenance, labeling requirements, regulatory pathways, and PMS plan established. |

---

*Document End — XPE-AI-REG-001 v1.0.0*
