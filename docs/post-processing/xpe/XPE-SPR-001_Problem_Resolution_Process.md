# Software Problem Resolution Process

**Document ID:** XPE-SPR-001 v1.0  
**IEC 62304 Clause:** 9.1 — 9.8  
**Safety Classification:** Class B  
**Date:** 2026-04-03  
**Author:** XPE Development Team  
**Approval:** __________________ Date: __________  

---

## 1. Purpose

XPE 소프트웨어에서 발견된 문제의 보고, 조사, 평가, 해결, 검증 절차를 정의한다.

## 2. Problem Detection Sources (9.1)

| Source | Entry Point |
|--------|-------------|
| Unit testing | CI failure → auto issue creation |
| Integration testing | CI failure → auto issue creation |
| System testing | Manual issue creation |
| Code review | PR comment → issue conversion |
| Field report | Customer support → Gitea Issue |
| Internal discovery | Developer → Gitea Issue |
| SOUP advisory | CVE monitoring → Gitea Issue |

## 3. Problem Report Creation (9.2)

Gitea Issue 생성 시 필수 항목:

| Field | Description | Required |
|-------|-------------|:--------:|
| Title | 간결한 문제 설명 | ✓ |
| Label | `problem-report` + severity label | ✓ |
| SW Version | 문제 발생 버전 (Git tag 또는 commit) | ✓ |
| Description | 상세 설명, reproduction steps | ✓ |
| Environment | OS, HW, configuration | ✓ |
| Expected vs Actual | 기대 동작 vs 실제 동작 | ✓ |
| Severity | Critical / Major / Minor | ✓ |
| Attachments | Screenshot, log, test data | 해당 시 |

## 4. Severity Classification (9.3)

| Severity | Definition | Response Time |
|----------|-----------|:------------:|
| **Critical** | Safety-related, potential patient harm, data loss | Investigation start: **24h** |
| **Major** | Functional failure, workaround available | Investigation: **5 business days** |
| **Minor** | Cosmetic, minor usability, documentation error | **Next planned release** |

## 5. Investigation & Impact Analysis (9.4)

각 problem report에 대해:

### 5.1 Root Cause Analysis

- Code inspection (해당 SW Unit 식별)
- Reproduction attempt (test environment)
- Root cause 문서화 (Issue comment)

### 5.2 Impact Assessment

| Assessment Item | Method |
|----------------|--------|
| Affected SW Items / Units | RTM 역추적 |
| Affected requirements | SRS trace |
| Safety impact | XPE-SRM-001 risk matrix 재평가 |
| Other versions affected | Branch/tag comparison |
| Regulatory reporting 필요 | MDR Vigilance / FDA MDR criteria |

## 6. Disposition (9.5)

| Disposition | Criteria | Action |
|-------------|----------|--------|
| **Fix** | Root cause identified, fix feasible | Change request → Clause 5 re-entry |
| **Defer** | Non-critical, fix not urgent | Risk evaluation + known anomaly list + justification |
| **No action** | Not reproducible, by design, duplicate | Justification documented, issue closed |
| **Workaround** | Fix deferred, interim mitigation available | Workaround documented + user notification |

## 7. Change Implementation (9.6)

Fix disposition인 경우:

1. Feature/hotfix branch 생성
2. Fix 구현 + unit test 추가/수정
3. PR (Issue reference 필수)
4. Code review + CI pass
5. Merge

## 8. Verification (9.7)

| Verification | Scope |
|-------------|-------|
| Unit test | 해당 unit의 기존 + 신규 test pass |
| Regression test | 영향 범위 기반 regression suite |
| System test | Safety-related fix → 관련 ST-SAFE-xxx 재실행 |

## 9. Closure (9.8)

Problem report closure 시 기록:

| Field | Content |
|-------|---------|
| Resolution | Fix description 또는 disposition rationale |
| Fix version | Git commit SHA + target release version |
| Verification result | Test pass evidence |
| Closed by | Name |
| Close date | Date |

## 10. Trend Analysis

| Activity | Frequency | Content |
|----------|-----------|---------|
| Problem trend review | Quarterly | Severity distribution, module distribution, mean resolution time |
| Recurring issue identification | Quarterly | 동일 module/unit 반복 이슈 → 근본 원인 조사 |
| Process improvement | Semi-annually | Problem resolution process 자체 개선 |

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-03 | XPE Team | Initial release |

---

*Document End — XPE-SPR-001 v1.0*
