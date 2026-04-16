# SPEC-DOC-001: MRD / System PRD / System V&V Plan 수립

**Document ID**: SPEC-DOC-001  
**Version**: 1.0.0  
**Date**: 2026-04-15  
**Status**: Approved for Implementation  
**Author**: MoAI (Ultrathink Cross-Verification)  
**Classification**: IEC 62304 Class B (pending hazard analysis confirmation)  
**Parent SPEC**: SPEC-XPE-MASTER v2.0.0  

---

## 1. 배경 및 목적

### 1.1 현황 분석

본 SPEC은 2026-04-15 교차검증(Cross-Verification Round 5)에서 발견된 문서 계층 갭을 해소하기 위해 작성되었다.

**교차검증 발견 사항:**

| ID | 등급 | 내용 | 출처 |
|----|------|------|------|
| CV-001 | CRITICAL | MRD 완전 부재 — 시장/사업 요구사항 문서 없음 | 이번 세션 |
| CV-002 | CRITICAL | Safety Class 미확정 (Class B/C) | XPE-XVER-001 C1 |
| CV-003 | HIGH | 시스템 수준 V&V Plan 부재 — VVP-001은 XPE 모듈 범위만, Validation(임상) 미포함 | 이번 세션 |
| CV-004 | HIGH | PRD 계층 불완전 — 모듈 PRD 존재, 시스템 PRD 없음 | 이번 세션 |
| CV-005 | HIGH | 지연시간 예산 충돌 — Pipeline-spec Stage(4) 150ms vs Ghost PRD Tier2 200ms | XPE-XVER-001 C2 |
| CV-006 | MEDIUM | IEC 62304 패키지 동기화 미완료 — OPEN-001 지속 | XVER-CONSOLIDATED-001 |

### 1.2 목적

다음 3개 문서를 신규 작성 또는 대폭 보강한다:

1. **XPE-MRD-001** — Market Requirements Document (신규)
2. **XPE-PRD-SYSTEM-001** — System Product Requirements Document (신규, product.md 기반)
3. **XPE-SVVP-001** — System Verification & Validation Plan (신규, VVP-001 통합 확장)

추가로 기존 미해결 이슈(CV-002, CV-005)에 대한 결정사항을 문서화한다.

### 1.3 문서 권위 계층

```
XPE-MRD-001 (시장 요구사항)
    └── XPE-PRD-SYSTEM-001 (제품 요구사항)
          ├── Module PRDs (calibration, defect, ghost, enhance-basic/adv, display, dicom, common, gsvg, ai)
          │     └── SRS per module
          │           └── SDD / SAD / RTM per module
          └── XPE-SVVP-001 (시스템 V&V 계획)
                ├── XPE-VVP-001 (XPE 범위, 기존 유지)
                └── Module-level test plans
```

---

## 2. 범위 (Scope)

### 2.1 포함 범위

- `docs/project/XPE-MRD-001_Market_Requirements_Document.md` 신규 작성
- `docs/project/XPE-PRD-SYSTEM-001_System_Product_Requirements.md` 신규 작성
- `docs/project/XPE-SVVP-001_System_Verification_Validation_Plan.md` 신규 작성
- `docs/project/cross-verification-consolidated.md` OPEN 항목 갱신
- CV-002 (Safety Class), CV-005 (Latency Budget) 결정사항 반영

### 2.2 제외 범위

- 기존 모듈별 SRS/SDD/RTM/VVP 개별 수정 (OPEN-001은 별도 SPEC-DOC-002에서 처리)
- 구현 코드 변경
- 새로운 알고리즘 요구사항 추가

---

## 3. 요구사항 (EARS Format)

### 3.1 MRD 요구사항

#### MRD-REQ-001 문서 메타데이터
WHERE 이 문서가 IEC 62304 규제 패키지의 최상위 기준 문서로 사용될 때,  
XPE-MRD-001은 문서 ID, 버전, 날짜, 승인자, 규제 범위(IEC 62304, ISO 14971, FDA 21 CFR 820.30, EU MDR 2017/745)를 포함해야 한다.

#### MRD-REQ-002 시장 문제 정의
WHEN 고객이 XPE 도입 결정을 내려야 할 때,  
XPE-MRD-001은 FPD 기반 X-ray 시스템에서 Raw 영상 처리가 필요한 시장 문제를 구체적으로 기술해야 한다.

**포함 내용:**
- FPD 제조사가 자체 개발하는 데 드는 비용/시간 문제
- 규제 인증 (IEC 62304 / FDA) 준수 부담
- 알고리즘 품질 차별화 필요성 (경쟁사 대비)

#### MRD-REQ-003 고객 세그먼트
WHERE 영업/마케팅이 XPE를 포지셔닝할 때,  
XPE-MRD-001은 최소 3개의 고객 세그먼트를 정의하고, 각 세그먼트의 핵심 요구사항을 기술해야 한다.

**필수 세그먼트:**
- FPD 제조사 (OEM 통합): 빠른 시장 출시, HW 의존성 최소화
- 의료기기 시스템 통합업체: 규제 패키지 완비, API 안정성
- 병원 IT 부서: DICOM 호환성, 기존 시스템 통합

#### MRD-REQ-004 시장 요구사항 (Market Requirements)
WHEN 제품 기획자가 우선순위를 결정할 때,  
XPE-MRD-001은 MR-xxx ID 형식으로 번호가 부여된 시장 요구사항을 포함해야 한다.

**필수 시장 요구사항 카테고리:**
- MR-FUNC: 기능적 시장 요구사항 (최소 10개)
- MR-PERF: 성능 시장 요구사항 (최소 5개)
- MR-REG: 규제/인증 시장 요구사항 (최소 5개)
- MR-COMPAT: 호환성 시장 요구사항 (최소 3개)
- MR-BUSI: 사업적 시장 요구사항 (최소 3개)

#### MRD-REQ-005 사업 성공 지표
WHERE 제품 출시 후 성과 평가가 이루어질 때,  
XPE-MRD-001은 측정 가능한 사업 성공 지표(KPI)를 정의해야 한다.

**필수 KPI:**
- 기술적 성과 지표 (PSNR, SNR, 처리 시간 등)
- 규제 인증 달성 지표 (IEC 62304 패키지 완비, FDA 제출)
- 고객 수용 지표 (임상 사용자 IQ 점수)

#### MRD-REQ-006 MRD → PRD 추적성
WHEN 시스템 PRD 요구사항이 작성될 때,  
각 MR-xxx 요구사항은 XPE-PRD-SYSTEM-001의 최소 1개 요구사항에 추적되어야 한다.

---

### 3.2 시스템 PRD 요구사항

#### PRD-REQ-001 문서 위상 및 메타데이터
WHERE 이 문서가 모든 모듈 PRD의 상위 기준이 될 때,  
XPE-PRD-SYSTEM-001은 다음을 포함해야 한다:
- 기존 `docs/project/product.md` (XPE-PRODUCT-001 v1.2.0) 내용 통합
- Safety Class 확정 결정사항 (CV-002 해소)
- 각 모듈 PRD에 대한 참조 링크

#### PRD-REQ-002 제품 범위 및 구성
WHEN 고객이 XPE를 평가할 때,  
XPE-PRD-SYSTEM-001은 42개 실행 단위(38 XPE SWU + 4 GSVG SI) 전체를 커버하는 제품 범위를 정의해야 한다.

#### PRD-REQ-003 Phase 정의 일관성
WHERE 개발 Phase 계획이 수립될 때,  
XPE-PRD-SYSTEM-001은 단일 normative Phase 정의를 제공해야 하며, 모든 모듈 PRD는 이 정의를 따라야 한다.

**Phase 정의 (normative):**
- Phase 0: xpe_common.dll, ImageProcTest.exe 기반 인프라
- Phase 1a: xpe_preprocess.dll (detector correction)
- Phase 1b: xpe_enhance_basic.dll, xpe_display.dll, xpe_dicom.dll
- Phase 2: xpe_enhance_advanced.dll, gsvg.dll
- Phase 3: xpe_ai.dll, xpe_ai_worker.exe (assistive only, fallback 필수)

#### PRD-REQ-004 지연시간 예산 결정 (CV-005 해소)
WHEN Ghost Correction 지연시간 예산이 충돌할 때,  
XPE-PRD-SYSTEM-001은 다음을 normative로 정의해야 한다:

```
Pre-processing 총 예산: 500ms (Phase 1a)
  ├── Offset/Gain Correction:  ≤ 80ms
  ├── Defective Pixel:         ≤ 50ms
  ├── Ghost/Lag Correction:
  │     ├── Tier 1 (기본):     ≤ 100ms  ← 기존 150ms에서 조정
  │     ├── Tier 2 (표준):     ≤ 200ms  ← Ghost PRD 기준 채택
  │     └── Tier 3 (NLCSC):    ≤ 400ms  ← 사용자 선택 모드, 예산 초과 허용
  └── 기타:                    나머지 예산
```

Tier 3 사용 시 Pre-processing 총 예산은 700ms로 확장되며, 이는 사용자가 명시적으로 활성화한 경우에만 적용된다.

#### PRD-REQ-005 Safety Class 결정 (CV-002 해소)
WHEN Safety Class가 IEC 62304 패키지에 적용될 때,  
XPE-PRD-SYSTEM-001은 Safety Class를 **Class B**로 확정하고, 다음 근거를 포함해야 한다:

**Class B 근거:**
1. XPE 소프트웨어 출력은 의료 전문가의 검토 없이 환자에게 직접 적용되지 않음
2. 의사/기사가 최종 진단 결정을 내리며, XPE는 보조 도구임
3. 단일 고장이 직접적 환자 해를 초래하는 메커니즘 없음 (hazard analysis 확인 필요)
4. 단, AI 기능(Phase 3)은 임상 승인 여부에 따라 재평가 필요

**ACTION REQUIRED (Phase 1a gate 전):**
- 시스템 Hazard Analysis 수행 및 서명 (ISO 14971)
- 이 결정이 hazard analysis에 의해 번복될 경우, Class C 패키지로 업그레이드

#### PRD-REQ-006 고객 인수 기준
WHEN 고객이 XPE를 최종 검수할 때,  
XPE-PRD-SYSTEM-001은 고객 관점의 인수 기준을 정의해야 한다.

**필수 인수 기준:**
- Phase 1a/1b 기능 완전성 (모든 Must-Have 기능 동작)
- 성능 기준 달성 (처리 시간, 메모리, 정확도)
- IEC 62304 Class B 문서 패키지 완비
- DICOM 호환성 (DVTk 전체 통과)
- 임상 이미지 품질 (독자 IQ ≥ 3.5/5, N=50)

#### PRD-REQ-007 MR → PR 추적성 테이블
WHEN 요구사항 추적성이 감사될 때,  
XPE-PRD-SYSTEM-001은 각 PR-xxx 요구사항이 상위 MR-xxx에 연결되는 추적성 테이블을 포함해야 한다.

---

### 3.3 시스템 V&V Plan 요구사항

#### VVP-REQ-001 문서 범위
WHERE 시스템 V&V 계획이 적용될 때,  
XPE-SVVP-001은 다음 전체 범위를 커버해야 한다:
- XPE 전체 패키지 (7개 DLL + 1개 EXE)
- GSVG 패키지 (독립 IEC 62304)
- C# ImageProcTest GUI

기존 XPE-VVP-001은 XPE 범위 내에서 유효하며, XPE-SVVP-001이 시스템 계층의 상위 문서가 된다.

#### VVP-REQ-002 Verification vs Validation 구분
WHEN V&V 활동이 계획될 때,  
XPE-SVVP-001은 IEC 62304 5.5/5.6/5.7(Verification)과 별도로 Validation 활동을 명시해야 한다.

**Verification (우리가 올바르게 만들었는가):**
- Unit Verification (SWU 단위) — 기존 VVP-001 참조
- Integration Verification — 기존 VVP-001 참조
- System Verification — SRS 기반 테스트

**Validation (우리가 올바른 것을 만들었는가):**
- Clinical Image Quality Validation — 실제 임상 이미지로 독자 평가
- Usability Validation — 목표 사용자 환경에서 실제 사용 시나리오 검증
- Intended Use Validation — 의도된 용도 충족 여부 확인
- Performance Validation — 실제 FPD 하드웨어 환경 성능

#### VVP-REQ-003 시스템 검증 계층
WHEN 시스템 V&V가 수행될 때,  
XPE-SVVP-001은 다음 검증 계층을 순서대로 정의해야 한다:

```
Level 1: Unit Verification  → SWU별 (XPE-VVP-001 §2)
Level 2: Integration Verification → SWI간 (XPE-VVP-001 §3)
Level 3: System Verification → SRS 요구사항 기반 (XPE-VVP-001 §4 확장)
Level 4: Multi-Package Integration → XPE + GSVG + C# GUI 통합
Level 5: Clinical Validation → 실제 임상 환경
Level 6: Field Performance → 배포 후 모니터링
```

#### VVP-REQ-004 Multi-Package 통합 테스트 계획
WHERE XPE, GSVG, C# GUI가 함께 동작할 때,  
XPE-SVVP-001은 패키지 간 통합 테스트 계획을 정의해야 한다.

**필수 Multi-Package 테스트 케이스:**
| Test ID | 설명 | Pass Criteria |
|---------|------|---------------|
| MP-IT-001 | XPE + GSVG 파이프라인 통합 | 출력 이미지 정합성, PSNR ≥ 40dB |
| MP-IT-002 | C# GUI → XPE P/Invoke 안정성 | 100회 연속 처리 무충돌 |
| MP-IT-003 | Full System End-to-End | Raw → DICOM 완전 파이프라인 ≤ 5s |
| MP-IT-004 | AI Worker 격리 테스트 | AI 실패 시 Phase 1/2 출력 유지 |
| MP-IT-005 | DICOM 전체 적합성 | DVTk Full Pass |
| MP-IT-006 | 메모리 안정성 | 1000회 순차 처리 후 RSS 증가 < 5% |

#### VVP-REQ-005 임상 검증 계획
WHEN 제품이 임상 현장에 배포되기 전에,  
XPE-SVVP-001은 임상 이미지 품질 검증 계획을 정의해야 한다.

**임상 검증 요소:**
- 대상: 최소 50장의 임상 이미지 (Chest PA, 사지, 척추 등 다부위 포함)
- 평가자: 최소 2명의 숙련된 방사선사 또는 방사선과 의사
- 평가 방법: 5점 척도 IQ 평가 (1=불량, 5=우수)
- 합격 기준: 평균 IQ ≥ 3.5/5
- 비교 기준: 기존 레퍼런스 시스템 대비 동등 이상

#### VVP-REQ-006 V&V 추적성 (MRD → PRD → SRS → Test)
WHEN 규제 감사가 수행될 때,  
XPE-SVVP-001은 다음 추적성 체인을 완성해야 한다:

```
MR-xxx (MRD) → PR-xxx (System PRD) → SRS-xxx (각 SRS) → Test-xxx (Test Case)
```

각 시장 요구사항은 테스트 케이스까지 추적 가능해야 한다.

#### VVP-REQ-007 Release Gate 정의
WHEN 각 Phase의 릴리즈가 판단될 때,  
XPE-SVVP-001은 Phase별 Release Gate를 정의해야 한다.

| Phase | Verification Gate | Validation Gate |
|-------|-------------------|-----------------|
| Phase 0 | 모든 Common SWU UT 통과, CI 파이프라인 구축 | 해당 없음 |
| Phase 1a | Pre-processing IT-001~IT-007 통과, Unit 커버리지 ≥ 80% | 팬텀 이미지 기초 검증 |
| Phase 1b | Full pipeline ST 통과, DICOM DVTk 통과 | 임상 이미지 파일럿 (N=10) |
| Phase 2 | GSVG 통합 IT 통과, 고급 처리 ST 통과 | 임상 이미지 확장 (N=30) |
| Phase 3 | AI Worker 격리 테스트, 폴백 테스트 통과 | 임상 검증 완료 (N=50) |

#### VVP-REQ-008 문제 해결 절차 연계
WHERE V&V 중 문제가 발견될 때,  
XPE-SVVP-001은 XPE-SPR-001 절차에 연계하고, 시스템 수준 문제에 대한 에스컬레이션 경로를 정의해야 한다.

---

## 4. 수용 기준 (Acceptance Criteria)

### 4.1 MRD 수용 기준

- [ ] AC-MRD-01: 문서 ID, 버전, 날짜, 승인자 칸 포함
- [ ] AC-MRD-02: 시장 문제 섹션 — 최소 3개의 구체적 문제 기술
- [ ] AC-MRD-03: 고객 세그먼트 — 최소 3개 세그먼트, 각 핵심 요구사항 포함
- [ ] AC-MRD-04: MR-FUNC 10개 이상, MR-PERF 5개 이상, MR-REG 5개 이상 포함
- [ ] AC-MRD-05: 사업 KPI — 최소 5개의 측정 가능한 지표
- [ ] AC-MRD-06: MR → PR 추적성 테이블 포함 (MR-xxx별 대응 PR-xxx)

### 4.2 시스템 PRD 수용 기준

- [ ] AC-PRD-01: product.md (XPE-PRODUCT-001 v1.2.0) 내용 완전 통합
- [ ] AC-PRD-02: Safety Class B 결정 + 근거 + ACTION REQUIRED 항목 포함
- [ ] AC-PRD-03: 42개 실행 단위 전체 커버 확인
- [ ] AC-PRD-04: Phase 0/1a/1b/2/3 단일 normative 정의
- [ ] AC-PRD-05: 지연시간 예산 테이블 (Tier별 Ghost 예산 포함)
- [ ] AC-PRD-06: 고객 인수 기준 섹션 — Phase 1b 이상 기준 포함
- [ ] AC-PRD-07: PR → MR 상향 추적성 테이블
- [ ] AC-PRD-08: 기존 모듈 PRD 참조 목록

### 4.3 시스템 V&V Plan 수용 기준

- [ ] AC-VVP-01: 6개 검증 계층 모두 정의 (Unit → Field)
- [ ] AC-VVP-02: Multi-Package 테스트 MP-IT-001~006 포함
- [ ] AC-VVP-03: 임상 검증 계획 (N=50, 2명 평가자, IQ ≥ 3.5 기준)
- [ ] AC-VVP-04: Phase별 Release Gate 테이블 (Phase 0~3)
- [ ] AC-VVP-05: MR → PR → SRS → Test 4단계 추적성 체인 예시 포함
- [ ] AC-VVP-06: XPE-VVP-001과의 관계 명시 (VVP-001이 하위 문서임을 선언)
- [ ] AC-VVP-07: Verification과 Validation 명확히 구분
- [ ] AC-VVP-08: 문제 해결 절차 XPE-SPR-001 연계

---

## 5. 기술적 접근

### 5.1 작성 전략

**MRD 작성 접근:**
- 기존 product.md, PRD-002의 Problem Statement 섹션을 MR 형태로 변환
- 기술 분류 문서(xray_fpd_tech_classification_final.md)에서 차별화 요소 추출
- 경쟁 분석은 현재 문서에 없으므로 일반적 FPD 시장 포지션으로 작성

**System PRD 작성 접근:**
- product.md (XPE-PRODUCT-001 v1.2.0)를 베이스로 확장
- 모든 모듈 PRD에서 핵심 요구사항 추출하여 통합
- CV-002, CV-005 결정사항 반영

**System V&V Plan 작성 접근:**
- 기존 XPE-VVP-001을 Level 1~3의 Verification 참조 문서로 포지셔닝
- Level 4~6 (Multi-Package, Clinical, Field)은 신규 작성
- RTM-001의 추적성 체인을 MRD까지 확장

### 5.2 파일 위치

```
docs/project/
├── XPE-MRD-001_Market_Requirements_Document.md        (신규)
├── XPE-PRD-SYSTEM-001_System_Product_Requirements.md  (신규)
├── XPE-SVVP-001_System_Verification_Validation_Plan.md (신규)
├── product.md                                          (기존 유지, PRD로 흡수 참조)
└── cross-verification-consolidated.md                  (CV-002/005 해소 갱신)
```

### 5.3 규제 정렬

각 문서의 IEC 62304 조항 매핑:

| 문서 | 관련 IEC 62304 조항 |
|------|---------------------|
| MRD | 5.1.1 (소프트웨어 개발 계획), 5.2.1 (소프트웨어 요구사항) |
| System PRD | 5.1.1, 5.2.1, 5.2.2 (요구사항 내용 카테고리) |
| System V&V Plan | 5.5 (Unit V), 5.6 (Integration V), 5.7 (System V), 8 (Configuration) |

---

## 6. 구현 순서

### Phase A: CV 이슈 해소 (선행)
1. cross-verification-consolidated.md에 CV-002, CV-005 결정사항 기록
2. Safety Class B 공식 선언 및 근거 문서화

### Phase B: MRD 작성
1. XPE-MRD-001_Market_Requirements_Document.md 신규 작성
2. MR-xxx ID 부여 및 추적성 테이블 초안

### Phase C: System PRD 작성
1. XPE-PRD-SYSTEM-001_System_Product_Requirements.md 신규 작성
2. product.md 내용 통합 + 확장
3. PR → MR 추적성 테이블

### Phase D: System V&V Plan 작성
1. XPE-SVVP-001_System_Verification_Validation_Plan.md 신규 작성
2. 기존 VVP-001 참조 통합
3. 임상 검증 계획 및 Release Gate 정의
4. 전체 추적성 체인 (MR → Test)

---

## 7. 위험 및 완화

| 위험 | 확률 | 영향 | 완화 방안 |
|------|------|------|----------|
| Safety Class B가 Hazard Analysis에서 번복될 경우 | 중 | 상 | Class B를 working assumption으로 사용하되, Hazard Analysis 완료 후 즉시 반영 |
| MRD 시장 정보 부족 (경쟁사 데이터 없음) | 고 | 중 | 기존 기술 분류 문서 기반으로 작성 가능한 범위로 제한 |
| 임상 검증 계획이 프로토콜 수준 미달 | 중 | 중 | 현재는 계획 수준으로 작성, 실제 임상 수행 시 방사선의학과와 협의 |

---

## 8. 개정 이력

| Rev | 날짜 | 저자 | 설명 |
|-----|------|------|------|
| 1.0.0 | 2026-04-15 | MoAI (Ultrathink CV Round 5) | 초안 작성 |

---

*Document End — SPEC-DOC-001 v1.0.0*
