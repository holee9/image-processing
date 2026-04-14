# Software Maintenance Plan

**Document ID:** XPE-SMP-001 v1.0  
**IEC 62304 Clause:** 6.1 — 6.3  
**Safety Classification:** Class B  
**Date:** 2026-04-03  
**Author:** XPE Development Team  
**Approval:** __________________ Date: __________  

---

## 1. Purpose

출시 후 XPE 소프트웨어의 유지보수 활동을 정의한다.

## 2. Maintenance Categories

| Category | Trigger | Process |
|----------|---------|---------|
| Corrective | Field problem report | SPR → fix → regression → release |
| Adaptive | OS update, SOUP update, HW change | Impact analysis → modify → V&V → release |
| Perfective | Feature request, performance improvement | SRS update → full Clause 5 cycle |
| Preventive | Scheduled review | SOUP CVE scan, code quality audit |

## 3. Maintenance Activities (6.1)

### 3.1 Feedback Collection

| Source | Channel | Responsible |
|--------|---------|-------------|
| Field issues | Customer support → Gitea Issue (label: `field-report`) | Support team |
| Internal issues | QA/Dev → Gitea Issue (label: `internal`) | QA/Dev team |
| SOUP advisories | CVE databases, vendor mailing lists | DevOps |
| Regulatory changes | Regulatory monitoring | RA team |

### 3.2 Problem Report Analysis (6.2.3)

각 post-market problem report에 대해:

1. **Safety impact 평가** — ISO 14971 risk matrix로 severity/probability 재평가
2. **영향 범위** — 영향받는 SW Item / Unit 식별 (RTM 역추적)
3. **Regulatory reporting** — 필요 여부 판단 (MDR Vigilance, FDA MDR)
4. **수정 우선순위** — Critical/Major/Minor (XPE-SPR-001 기준)
5. **Regression scope** — 변경 영향 범위에 따라 test 범위 결정

### 3.3 Modification Implementation

변경이 필요한 경우, IEC 62304 Clause 5 해당 단계부터 재진입한다:

| Change Scope | Re-entry Point | Required Activities |
|-------------|---------------|-------------------|
| Requirement 변경 | 5.2 (SRS update) | SRS → SAD → SDD → Impl → UT → IT → ST |
| Architecture 변경 | 5.3 (SAD update) | SAD → SDD → Impl → UT → IT → ST |
| Unit 변경 (bug fix) | 5.5 (Implementation) | Impl → UT → IT (affected) → ST (affected) |
| Config/preset 변경 | 5.7 (System test) | Config update → ST (affected) |

## 4. Preventive Maintenance Schedule

| Activity | Frequency | Scope | Responsible |
|----------|-----------|-------|-------------|
| SOUP CVE scan | Quarterly | All SOUP components | DevOps |
| SOUP version review | Per major SOUP release | Evaluate upgrade necessity | SW Lead |
| Code quality audit | Semi-annually | Static analysis full scan | QA |
| Calibration data review | Annually | Detector-specific cal data validity | Field service |
| Regulatory update review | Quarterly | IEC 62304, DICOM, FDA guidance changes | RA team |

## 5. Maintenance Release Process

Maintenance release도 XPE-SRP-001 release checklist를 준수한다. Patch release(X.Y.Z+1)의 경우 변경 범위에 한정한 V&V만 수행하되, full regression test는 필수.

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-03 | XPE Team | Initial release |

---

*Document End — XPE-SMP-001 v1.0*
