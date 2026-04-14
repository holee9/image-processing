# Software Problem Resolution Process

**Document ID:** XPE-SPR-001 v1.0  
**IEC 62304 Clause:** 9.1 — 9.8  
**Safety Classification:** Class B  
**Date:** 2026-04-03  
**Author:** XPE Development Team  
**Approval:** __________________ Date: __________  

---

## 1. 목적

XPE 소프트웨어에서 발견된 문제의 보고, 조사, 평가, 해결, 검증 절차를 정의한다.

## 2. 문제 감지 출처 (9.1)

| 출처 | 진입점 |
|--------|-------------|
| Unit testing | CI 실패 → 자동 이슈 생성 |
| Integration testing | CI 실패 → 자동 이슈 생성 |
| System testing | 수동 이슈 생성 |
| Code review | PR 댓글 → 이슈 변환 |
| Field report | 고객 지원 → Gitea Issue |
| Internal discovery | 개발자 → Gitea Issue |
| SOUP advisory | CVE 모니터링 → Gitea Issue |

## 3. 문제 보고 생성 (9.2)

Gitea Issue 생성 시 필수 항목:

| 필드 | 설명 | 필수 |
|-------|-------------|:--------:|
| Title | 간결한 문제 설명 | ✓ |
| Label | `problem-report` + severity label | ✓ |
| SW Version | 문제 발생 버전 (Git tag 또는 commit) | ✓ |
| Description | 상세 설명, reproduction steps | ✓ |
| Environment | OS, HW, configuration | ✓ |
| Expected vs Actual | 기대 동작 vs 실제 동작 | ✓ |
| Severity | Critical / Major / Minor | ✓ |
| Attachments | Screenshot, log, test data | 해당 시 |

## 4. 심각도 분류 (9.3)

| 심각도 | 정의 | 대응 시간 |
|----------|-----------|:------------:|
| **Critical** | 안전 관련, 환자 피해 가능성, 데이터 손실 | 조사 시작: **24h** |
| **Major** | 기능 오작동, 임시 해결책 가능 | 조사: **5 업무일** |
| **Minor** | 미관상, 경미한 사용성, 문서 오류 | **다음 예정된 릴리스** |

## 5. 조사 및 영향 분석 (9.4)

각 problem report에 대해:

### 5.1 근본 원인 분석

- 코드 검사 (해당 SW Unit 식별)
- 재현 시도 (test environment)
- 근본 원인 문서화 (Issue comment)

### 5.2 영향 평가

| 평가 항목 | 방법 |
|----------------|--------|
| 영향 받는 SW Items / Units | RTM 역추적 |
| 영향 받는 요구사항 | SRS trace |
| 안전성 영향 | XPE-SRM-001 risk matrix 재평가 |
| 영향 받는 다른 버전 | Branch/tag comparison |
| 규제 보고 필요 여부 | MDR Vigilance / FDA MDR criteria |

## 6. 조치 방안 (9.5)

| 조치 방안 | 기준 | 조치 |
|-------------|----------|--------|
| **Fix** | 근본 원인 확인됨, 수정 가능 | Change request → Clause 5 재진입 |
| **Defer** | 비치명적, 긴급하지 않음 | 위험 평가 + 알려진 이상 목록 + 정당성 |
| **No action** | 재현 불가, 설계상 의도, 중복 | 정당성 문서화, 이슈 종료 |
| **Workaround** | 수정 연기, 임시 완화책 가능 | 임시 해결책 문서화 + 사용자 통지 |

## 7. 변경 구현 (9.6)

Fix disposition인 경우:

1. Feature/hotfix branch 생성
2. Fix 구현 + unit test 추가/수정
3. PR (Issue reference 필수)
4. Code review + CI pass
5. Merge

## 8. 검증 (9.7)

| 검증 | 범위 |
|-------------|-------|
| Unit test | 해당 unit의 기존 + 신규 test pass |
| Regression test | 영향 범위 기반 regression suite |
| System test | 안전 관련 fix → 관련 ST-SAFE-xxx 재실행 |

## 9. 종료 (9.8)

Problem report 종료 시 기록:

| 필드 | 내용 |
|-------|---------|
| Resolution | Fix 설명 또는 disposition 정당성 |
| Fix version | Git commit SHA + 대상 릴리스 버전 |
| Verification result | Test pass 증거 |
| Closed by | 이름 |
| Close date | 날짜 |

## 10. 추세 분석

| 활동 | 빈도 | 내용 |
|----------|-----------|---------|
| Problem 추세 검토 | 분기별 | 심각도 분포, 모듈 분포, 평균 해결 시간 |
| 반복 이슈 식별 | 분기별 | 동일 module/unit 반복 이슈 → 근본 원인 조사 |
| 프로세스 개선 | 반기별 | Problem resolution process 자체 개선 |

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-03 | XPE Team | Initial release |

---

*Document End — XPE-SPR-001 v1.0*
