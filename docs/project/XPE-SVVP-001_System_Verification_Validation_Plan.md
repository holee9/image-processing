# System Verification & Validation Plan

**Document ID**: XPE-SVVP-001  
**Version**: 1.0.0  
**Date**: 2026-04-15  
**Status**: Controlled Draft  
**Classification**: Internal / IEC 62304 Compliance  
**Author**: XPE QA Team  
**Approval**: __________________ Date: __________  
**Safety Classification**: IEC 62304 Class B  
**IEC 62304 Clauses**: 5.5 (Unit V), 5.6 (Integration V), 5.7 (System V), 7.3 (Risk Control V)  
**Canonical Scope**: `docs/project/`  
**Parent**: XPE-PRD-SYSTEM-001 v1.0.0  
**Integrates**: XPE-VVP-001 (Level 1~3 Verification, existing), XPE-STP-001, XPE-ITP-001  
**Cross-Ref**: XPE-RTM-001 v1.1, XPE-SRS-001 v1.0, XPE-MRD-001 v1.0.0  

---

## 1. 목적 (Purpose)

본 문서는 XPE 시스템 전체(42개 실행 단위)에 대한 Verification과 Validation 활동을 정의한다.

**Verification (우리가 올바르게 만들었는가):**  
소프트웨어가 SRS, SDD, SAD에 명시된 요구사항을 충족하는지 확인한다.

**Validation (우리가 올바른 것을 만들었는가):**  
소프트웨어가 의도된 용도(Intended Use)와 사용자의 실제 요구를 충족하는지 확인한다.

기존 XPE-VVP-001은 Level 1~3 Verification(XPE 모듈 범위)의 참조 문서로 유효하다.  
본 XPE-SVVP-001은 시스템 계층의 상위 문서이며, 전체 범위(모든 DLL + C# GUI + GSVG)를 포괄한다.

---

## 2. V&V 계층 구조

### 2.1 6단계 V&V 계층

```
Level 1: Unit Verification       → 각 SWU/SI 단위 (→ XPE-VVP-001 §2 참조)
Level 2: Integration Verification → SWI간 인터페이스 (→ XPE-VVP-001 §3 참조)
Level 3: System Verification     → SRS 요구사항 기반 (→ XPE-VVP-001 §4 참조)
Level 4: Multi-Package Integration → XPE + GSVG + C# GUI 통합 (본 문서 §4)
Level 5: Clinical Validation     → 실제 임상 환경 (본 문서 §5)
Level 6: Field Performance       → 배포 후 모니터링 (본 문서 §6)
```

### 2.2 Level 1~3 참조

Level 1 (Unit Verification), Level 2 (Integration Verification), Level 3 (System Verification)은 기존 문서를 참조한다:

- **XPE-VVP-001** (v1.0): XPE 모듈 범위 V&V Plan (Unit, Integration, System)
- **XPE-STP-001**: 소프트웨어 테스트 케이스 (ST-001~ST-PERF-004)
- **XPE-ITP-001**: 통합 테스트 계획 (IT-001~IT-008)
- **XPE-RTM-001**: SRS → Test 추적성 매트릭스

다음 Level 1~3 수용 기준은 본 문서에서 normative로 확인한다:

| 기준 | 목표 | 참조 |
|------|------|------|
| Statement coverage | ≥ 80% per SWU | XPE-VVP-001 §2.2 |
| Branch coverage | ≥ 70% per SWU | XPE-VVP-001 §2.2 |
| 모든 단위 테스트 통과 | 100% (zero failures) | XPE-VVP-001 §2.2 |
| 정적 분석 위반 | Zero critical/high | XPE-VVP-001 §2.2 |
| 메모리 누수 | Zero (ASan clean) | XPE-VVP-001 §2.2 |
| 통합 테스트 IT-001~IT-008 | All Pass | XPE-VVP-001 §3.2 |
| 시스템 테스트 ST-001~ST-PERF-004 | All Pass | XPE-VVP-001 §4.1 |

---

## 3. Level 3: System Verification (확장)

### 3.1 SRS 요구사항 → 시스템 테스트 완전성

Level 3는 XPE-VVP-001 §4를 기반으로 하며, 본 문서에서 System PRD 요구사항(PR-xxx)과의 추적성을 추가한다.

| PR ID | SRS ID | 시스템 테스트 ID | Pass Criteria |
|-------|--------|--------------|---------------|
| PR-FUNC-001 | SRS-FUNC-001 | ST-001 | PSNR ≥ 60dB |
| PR-FUNC-002 | SRS-FUNC-002 | ST-002 | 비균일성 < 2% |
| PR-FUNC-003 | SRS-FUNC-003 | ST-003 | 모든 결함 보정 |
| PR-FUNC-004 | SRS-FUNC-004 | ST-004 | Ghost ≤ 10% |
| PR-FUNC-010 | SRS-FUNC-010 | ST-010 | R² ≥ 0.999 |
| PR-FUNC-011 | SRS-FUNC-011 | ST-011 | SNR 개선 ≥ 3dB |
| PR-FUNC-012 | SRS-FUNC-012 | ST-012 | 독자 IQ ≥ 3.5/5 |
| PR-FUNC-013 | SRS-FUNC-013 | ST-013 | overshoot ≤ 5% |
| PR-FUNC-020 | SRS-FUNC-020 | ST-020 | pixel exact ± 0 |
| PR-FUNC-021 | SRS-FUNC-021 | ST-021 | ± 1 gray level |
| PR-FUNC-022 | SRS-FUNC-022 | ST-022 | Δ JND ≤ 1% |
| PR-FUNC-030 | SRS-FUNC-030 | ST-030 | DVTk full pass |
| PR-SAFE-001 | SRS-SAFE-001 | ST-SAFE-001 | byte-identical raw |
| PR-SAFE-003 | SRS-SAFE-003 | ST-SAFE-003 | 경고 2초 이내 |
| PR-SAFE-008 | SRS-SAFE-008 | ST-SAFE-008 | "AI-processed" 표시 |
| PR-PERF-001 | SRS-PERF-001 | ST-PERF-001 | ≤ 500ms |
| PR-PERF-002 | SRS-PERF-002 | ST-PERF-002 | ≤ 3초 |
| PR-PERF-003 | SRS-PERF-003 | ST-PERF-003 | ≤ 16ms |
| PR-PERF-004 | SRS-PERF-004 | ST-PERF-004 | ≤ 2GB |

---

## 4. Level 4: Multi-Package Integration Verification

### 4.1 범위

Level 4는 개별 패키지 검증 완료 후 수행하며, 패키지 간 인터페이스와 전체 시스템 동작을 검증한다.

**대상 패키지:**
- XPE (xpe_common + xpe_preprocess + xpe_enhance_basic + xpe_display + xpe_dicom)
- GSVG (gsvg.dll, 독립 IEC 62304 패키지)
- C# ImageProcTest GUI (P/Invoke 오케스트레이터)

**전제 조건:**
- Level 1~3 (XPE-VVP-001) 모두 Pass
- GSVG-SVP-001 (GSVG Verification Plan) Pass
- Level 4 시작 전 별도 회의 승인

### 4.2 Multi-Package 테스트 케이스

| Test ID | 설명 | 입력 | 예상 출력 | Pass Criteria |
|---------|------|------|---------|--------------|
| MP-IT-001 | XPE + GSVG 파이프라인 통합 | Grid 영상 (Raw) | Grid 억제 + 품질 유지 | PSNR ≥ 40dB (GSVG 출력 vs 레퍼런스) |
| MP-IT-002 | C# GUI → XPE P/Invoke 안정성 | 100회 연속 전체 파이프라인 | 충돌 없음, 출력 정합 | 100회 모두 성공, 충돌 0 |
| MP-IT-003 | Full System End-to-End | Raw FPD 데이터 | 진단용 DICOM 출력 | 전체 처리 ≤ 5s, DVTk Pass |
| MP-IT-004 | AI Worker 격리 + 폴백 | AI 처리 활성화, AI 강제 실패 | Phase 1/2 결과 자동 반환 | 100ms 이내 폴백, 출력 정합 |
| MP-IT-005 | DICOM 전체 적합성 | 전체 파이프라인 출력 | DICOM 표준 준수 | DVTk Full Pass |
| MP-IT-006 | 메모리 장기 안정성 | 1000회 순차 처리 | 메모리 안정 | RSS 증가 < 5% |
| MP-IT-007 | 동시 파이프라인 (스레드 안전성) | 2개 동시 전체 파이프라인 | 두 파이프라인 모두 성공 | 충돌 없음, 출력 정합 |
| MP-IT-008 | GSVG 없이 Phase 1b 동작 | GSVG DLL 미로드 | Phase 1b 정상 동작 | Phase 1/2 출력 정상, 오류 없음 |

### 4.3 Multi-Package 테스트 환경

| 항목 | 사양 |
|------|------|
| OS | Windows 11 Pro 64-bit |
| RAM | ≥ 32GB |
| CPU | Intel Core i7 이상 (8코어) |
| GPU | (Phase 3 AI용) CUDA 지원 GPU |
| FPD 시뮬레이션 | 합성 Raw 데이터 (팬텀 이미지) |
| DICOM 검증 | DVTk 4.1 이상 |

### 4.4 테스트 기록

각 MP-IT 실행에 대해 다음을 기록한다:

| 필드 | 설명 |
|------|------|
| Test ID | MP-IT-xxx |
| 실행 일시 | |
| SW 버전 | Git commit SHA (전체 패키지) |
| 환경 | OS, HW, 컴파일러 버전 |
| 입력 데이터 | 합성/팬텀 데이터 ID |
| 결과 | Pass / Fail + 측정값 |
| 이상 사항 | 문제 보고서 참조 (있을 경우) |
| 실행자 | 이름 |

---

## 5. Level 5: Clinical Validation (임상 검증)

### 5.1 목적

임상 검증은 XPE가 실제 임상 환경에서 의도된 용도(Intended Use)를 충족하는지 확인한다.

**IEC 62304 근거**: 소프트웨어가 의도된 용도에 적합한지 확인하는 것은 시스템 수준 Validation이며, IEC 62304 단독이 아닌 ISO 14971 및 IEC 62366-1과 연계하여 수행한다.

### 5.2 임상 이미지 품질 검증

**검증 목표**: 독자 평가에서 평균 IQ ≥ 3.5/5 달성

| 항목 | 명세 |
|------|------|
| 평가 이미지 수 | 최소 50장 (Phase 3 완료 시) |
| 파일럿 이미지 수 | 최소 10장 (Phase 1b 완료 시) |
| 신체 부위 다양성 | Chest PA, 손/손목, 무릎, 척추(전후면/측면), 골반 포함 |
| 평가자 | 방사선사 또는 방사선과 의사 최소 2명 |
| 평가 방법 | 5점 척도 IQ 평가 (맹검 비교 포함) |
| 비교 기준 | 레퍼런스 시스템 (기존 임상 사용 시스템) |

**5점 척도 기준:**

| 점수 | 기준 |
|------|------|
| 5 | 우수 — 진단 정보 완전, 아티팩트 없음 |
| 4 | 양호 — 진단 가능, 경미한 아티팩트 |
| 3.5 | **최소 합격 기준** |
| 3 | 보통 — 진단 가능하나 제한적 |
| 2 | 불량 — 진단 어려움 |
| 1 | 매우 불량 — 진단 불가 |

### 5.3 Intended Use 검증 시나리오

| 시나리오 ID | 설명 | Pass Criteria |
|-----------|------|--------------|
| IU-001 | 일반 X-ray 촬영 후 자동 처리 → DICOM 전송 | 처리 시간 ≤ 3초, DICOM 수신 확인 |
| IU-002 | 방사선사 W/L 조정 → 즉각 반응 | 16ms 이내 화면 갱신 |
| IU-003 | 결함 보정 실패 상황 → 사용자 경고 | 2초 이내 경고 표시, 시스템 충돌 없음 |
| IU-004 | AI 기능 비활성화 시 기본 파이프라인 동작 | Phase 1/2 정상 출력 |
| IU-005 | 교정 파라미터 없는 신규 FPD → 적절한 오류 메시지 | 오류 메시지 명확, 무결한 재시도 |

### 5.4 Usability Validation

IEC 62366-1 요구사항에 따라 사용성 검증을 수행한다 (범위는 ImageProcTest.exe GUI에 한정).

| 항목 | 명세 |
|------|------|
| 대상 사용자 | 방사선사 (숙련도 중급 이상) |
| 검증 방법 | Summative Usability Evaluation (시뮬레이션 기반) |
| 핵심 태스크 | W/L 조정, 원본/처리 전환, 교정 파라미터 로드 |
| 합격 기준 | 치명적 사용 오류 0건, 사용자 오류율 < 5% |

---

## 6. Level 6: Field Performance Monitoring (배포 후 모니터링)

### 6.1 목적

배포 후 실제 운영 환경에서 성능 및 안전 지표를 추적하며, IEC 62304 §6.2 (Software Problem Resolution)와 연계한다.

### 6.2 모니터링 지표

| 지표 | 목표 | 모니터링 방법 |
|------|------|------------|
| 처리 실패율 | < 0.1% | 로그 분석 (xpe_log) |
| 평균 처리 시간 (Phase 1a) | ≤ 500ms | 성능 로그 |
| 충돌/비정상 종료 | Zero | 에러 보고 시스템 |
| 사용자 불만 보고 | 매월 분석 | SPR 시스템 (XPE-SPR-001) |
| AI 폴백 발생률 | 추적 및 분석 | 로그 (AI 모듈) |

### 6.3 문제 해결 연계

배포 후 발견된 문제는 XPE-SPR-001 (Software Problem Resolution Process)에 따라 처리하며, 다음 에스컬레이션 경로를 따른다:

```
사용자 보고 / 시스템 로그 감지
    → SPR 등록 (XPE-SPR-001)
    → 영향도 분석 (Safety 여부 확인)
    → Safety 관련: 즉시 XPE-SRM-001 위험 평가
    → Non-safety: 정기 패치 주기
    → Root cause 분석 → Fix → Regression Test → Release
```

---

## 7. 위험 제어 검증 (Risk Control Verification)

IEC 62304 §7.3.3 및 ISO 14971 기준으로 모든 위험 제어 조치의 검증을 확인한다.

| Hazard ID | 위험 제어 조치 | 검증 테스트 | Pass Criteria |
|-----------|-------------|-----------|--------------|
| HAZ-001 | Non-destructive processing (Raw 보존) | ST-SAFE-001 | byte-identical raw 확인 |
| HAZ-002 | 파라미터 검증 (범위 초과 거부) | ST-SAFE-002 | 경고 표시, 처리 거부 |
| HAZ-003 | 결함 보정 실패 경고 | ST-SAFE-003 | 2초 이내 경고 |
| HAZ-004 | DICOM 태그 무결성 | ST-SAFE-004 | 태그 변경 없음 |
| HAZ-005 | Edge 과강조 방지 (safe gain range) | ST-SAFE-005 | overshoot ≤ 5% |
| HAZ-006 | W/L 범위 초과 경고 | ST-SAFE-006 | 경고 표시 |
| HAZ-007 | GSDF 오차 최소화 | ST-SAFE-007 | Δ JND ≤ 1% |
| HAZ-008 | AI 처리 명시 표시 | ST-SAFE-008 | "AI-processed" 표시 |
| HAZ-009 | 원본/처리 즉시 전환 | ST-SAFE-009 | 100ms 이내 전환 |

---

## 8. Phase별 Release Gate

각 Phase의 릴리즈 결정은 본 섹션의 Gate를 통과한 경우에만 승인된다.

### Gate 0: Phase 0 릴리즈 (Foundation)

**Verification Gate:**
- [ ] xpe_common.dll SWU-5.1~5.8 단위 테스트 100% Pass
- [ ] CI 파이프라인 정상 동작 (Gitea Actions, Google Test, gcov)
- [ ] AddressSanitizer clean (zero leaks)
- [ ] ImageProcTest.exe DLL 로드 정상

**Validation Gate:**
- 해당 없음 (Phase 0은 인프라)

---

### Gate 1a: Phase 1a 릴리즈 (Detector Correction)

**Verification Gate:**
- [ ] xpe_preprocess.dll SWU-1.1~1.9 단위 테스트 100% Pass
- [ ] Statement coverage ≥ 80% per SWU
- [ ] Integration 테스트 IT-001~IT-007 All Pass
- [ ] 정적 분석: Zero critical/high (cppcheck, clang-tidy)
- [ ] XPE-VVP-001 Level 1~2 Gate 통과

**Validation Gate:**
- [ ] 팬텀 이미지 기초 검증 (합성 데이터, 알고리즘 정확도 확인)
- [ ] IEC 62304 Class B 문서: SDP, SRS, SDD, SAD, SHA 초안 완비

---

### Gate 1b: Phase 1b 릴리즈 (Core Pipeline — 최소 고객 릴리즈)

**Verification Gate:**
- [ ] Phase 1a Gate 1a 통과
- [ ] xpe_enhance_basic.dll, xpe_display.dll, xpe_dicom.dll 단위 테스트 All Pass
- [ ] 시스템 테스트 ST-001~ST-PERF-004 All Pass
- [ ] DICOM DVTk Full Pass
- [ ] Multi-Package 테스트 MP-IT-002, MP-IT-003, MP-IT-005, MP-IT-006 All Pass
- [ ] XPE-VVP-001 Level 1~3 Gate 통과

**Validation Gate:**
- [ ] 임상 이미지 파일럿 (N=10) IQ ≥ 3.5/5
- [ ] IEC 62304 Class B 문서 패키지 완비 (SDP, SRS, SDD, SAD, RTM, VVP, SHA, SOUP, SRM, SCM, SPR, SMP, SRP)
- [ ] ISO 14971 Hazard Analysis 서명 완료 (Safety Class B 공식 확정)

---

### Gate 2: Phase 2 릴리즈 (Premium Processing)

**Verification Gate:**
- [ ] Phase 1b Gate 통과
- [ ] xpe_enhance_advanced.dll 단위 테스트 All Pass
- [ ] gsvg.dll GSVG-SVP-001 Verification Plan 통과
- [ ] Multi-Package 테스트 MP-IT-001, MP-IT-007, MP-IT-008 All Pass

**Validation Gate:**
- [ ] 임상 이미지 확장 검증 (N=30) IQ ≥ 3.5/5
- [ ] GSVG IEC 62304 패키지 완비

---

### Gate 3: Phase 3 릴리즈 (Assistive AI)

**Verification Gate:**
- [ ] Phase 2 Gate 통과
- [ ] xpe_ai.dll, xpe_ai_worker.exe 단위 테스트 All Pass
- [ ] Multi-Package 테스트 MP-IT-004 (AI 폴백) All Pass

**Validation Gate:**
- [ ] 임상 검증 완료 (N=50) IQ ≥ 3.5/5
- [ ] Usability Validation (치명적 오류 0건)
- [ ] AI 처리 표시 검증 (방사선과 의사 확인)
- [ ] Regulatory AI boundary 결정 완료 (OPEN-004 해소)

---

## 9. 전체 추적성 체인 (MRD → PRD → SRS → Test)

다음은 시장 요구사항부터 테스트까지의 완전한 추적성 체인 예시다.

### 9.1 Ghost Correction 추적성 체인 (완전 예시)

```
MR-FUNC-001 (FPD Raw → 4종 Pre-processing 보정 필요)
    └── PR-FUNC-004 (Ghost/Lag 보정, Tier 1~3)
          └── SRS-FUNC-004 (multi-exponential, ≥90% ghost removal)
                ├── SDD-002 §2.4 (설계 상세)
                ├── SWU-1.4 (소프트웨어 유닛)
                ├── UT-1.4-001~006 (단위 테스트)
                ├── IT-001 (통합 테스트)
                ├── ST-004 (시스템 테스트)
                └── HAZ-004 (위험 관리) → ST-SAFE-004
```

### 9.2 DICOM Display Pipeline 추적성 체인

```
MR-FUNC-003 (DICOM Grayscale Display Pipeline 준수)
    └── PR-FUNC-022 (GSDF P-value, Δ JND ≤ 1%)
          └── SRS-FUNC-022 (Presentation LUT, Δ JND ≤ 1%)
                ├── SDD-002 §4.3
                ├── SWU-3.3
                ├── UT-3.3-001~006
                ├── IT-003
                ├── ST-022
                └── HAZ-007 → ST-SAFE-007
```

### 9.3 시스템 성능 추적성 체인

```
MR-PERF-001 (Pre-processing ≤ 500ms)
    └── PR-PERF-001 (3072×3072, ≤ 500ms)
          └── SRS-PERF-001 (Pre-processing timing)
                └── ST-PERF-001 (시스템 성능 테스트)
                      └── MP-IT-003 (End-to-End ≤ 5s 포함)
```

### 9.4 전체 MR → Test 추적성 요약

| MR ID | PR ID | SRS ID | Unit Test | Integration | System Test | Multi-Pkg | Clinical |
|-------|-------|--------|-----------|-------------|-------------|----------|---------|
| MR-FUNC-001 | PR-FUNC-001~004 | SRS-FUNC-001~004 | UT-1.1~1.4 | IT-001 | ST-001~004 | MP-IT-003 | IU-001 |
| MR-FUNC-002 | PR-FUNC-010~013 | SRS-FUNC-010~013 | UT-2.1~2.4 | IT-002 | ST-010~013 | MP-IT-003 | IU-001, IU-002 |
| MR-FUNC-003 | PR-FUNC-020~023 | SRS-FUNC-020~023 | UT-3.1~3.3 | IT-003 | ST-020~023 | MP-IT-003, 005 | IU-002 |
| MR-FUNC-004 | PR-FUNC-030~032 | SRS-FUNC-030~032 | UT-4.1~4.3 | IT-003 | ST-030~031 | MP-IT-003, 005 | — |
| MR-FUNC-012 | PR-SAFE-010 | SRS-SAFE-010 | — | — | ST-SAFE-008 | MP-IT-004 | IU-004 |
| MR-PERF-001 | PR-PERF-001 | SRS-PERF-001 | — | IT-010 | ST-PERF-001 | MP-IT-003 | IU-001 |
| MR-PERF-002 | PR-PERF-002 | SRS-PERF-002 | — | IT-009 | ST-PERF-002 | MP-IT-003 | IU-001 |
| MR-PERF-003 | PR-PERF-003 | SRS-PERF-003 | UT-3.2-interactive | IT-004 | ST-PERF-003 | MP-IT-002 | IU-002 |
| MR-REG-001 | PR-REG-001 | — | — | — | 문서 감사 | — | — |

---

## 10. 문제 해결 절차 연계 (XPE-SPR-001)

V&V 중 발견된 모든 문제는 XPE-SPR-001(Software Problem Resolution Process)에 따라 처리한다.

### 10.1 에스컬레이션 기준

| 문제 유형 | 에스컬레이션 | 조치 |
|---------|-----------|------|
| Safety-critical 실패 (HAZ-xxx 관련) | 즉시 QA 리드, 규제팀 | 릴리즈 차단, 즉시 조사 |
| Gate 실패 (KPI 미달) | QA 리드 승인 필요 | 수정 후 재검증 |
| Non-blocking 불일치 | 다음 Sprint | SPR 등록 후 추적 |

### 10.2 Regression 정책

- 모든 시스템 테스트(ST-xxx)는 Regression Suite에 포함
- Release Branch merge 전 Full Regression 필수
- Regression 실패 → 릴리즈 차단
- Multi-Package 테스트(MP-IT-xxx)는 Phase Gate 시 Full Run 필수

---

## 11. V&V 기록 관리

### 11.1 기록 보존 위치

| 기록 유형 | 위치 | 보존 기간 |
|---------|------|---------|
| 단위 테스트 리포트 | CI 아티팩트 (Gitea Actions) | 프로젝트 종료 후 10년 |
| 커버리지 리포트 | CI 아티팩트 | 프로젝트 종료 후 10년 |
| 통합/시스템 테스트 기록 | `docs/test-records/` | 프로젝트 종료 후 10년 |
| Multi-Package 테스트 기록 | `docs/test-records/multi-package/` | 프로젝트 종료 후 10년 |
| 임상 검증 기록 | `docs/clinical-validation/` | 프로젝트 종료 후 15년 (의료기기) |
| 문제 보고서 (SPR) | `docs/problem-reports/` | 프로젝트 종료 후 10년 |

### 11.2 IEC 62304 §5.8 문서 요구사항

V&V 활동 결과는 다음을 포함해야 한다:
- 수행된 테스트 목록과 결과
- 사용된 소프트웨어 버전 (Git commit SHA)
- 테스트 환경 전체 사양
- 발견된 이상 사항과 해결 방법
- 서명 및 날짜

---

## 12. 개정 이력

| Rev | 날짜 | 저자 | 설명 |
|-----|------|------|------|
| 1.0.0 | 2026-04-15 | MoAI (SPEC-DOC-001 구현) | 초안 — SPEC-DOC-001 VVP-REQ-001~008 구현. XPE-VVP-001 통합, Multi-Package V, 임상 V, Phase Gate, 추적성 체인 신규 작성 |

---

*Document End — XPE-SVVP-001 v1.0.0*
