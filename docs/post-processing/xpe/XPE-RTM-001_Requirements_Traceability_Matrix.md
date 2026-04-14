# 요구사항 추적성 매트릭스

**Document ID:** XPE-RTM-001 v1.1  
**IEC 62304 Clause:** 5.1.1c, 5.3.6, 7.3.3  
**Safety Classification:** Class B  
**Date:** 2026-04-14  
**Author:** XPE Development Team  
**Approval:** __________________ Date: __________  

---

## 1. 목적

시스템 요구사항 → 소프트웨어 요구사항 → 아키텍처 → 소프트웨어 유닛 → 테스트 → 리스크 제어 간의 양방향 추적성을 제공한다.

## 2. 전방향 추적성 (SRS → Test)

| SRS 요구사항 ID | 아키텍처 (SAD) | 유닛 (SDD-001) | 설계 세부사항 (SDD-002) | 유닛 테스트 (STP-001) | 통합 테스트 | 시스템 테스트 | 리스크 참조 | 해저드 (SHA-001) |
|-----------|:----------:|:----------:|:----------:|:---------:|:---------------:|:-----------:|:--------:|:--------:|
| SRS-FUNC-001 | SWI-1 | SWU-1.1 | SDD-002 §2.1 | UT-1.1-001..005 | IT-001 | ST-001 | — | — |
| SRS-FUNC-002 | SWI-1 | SWU-1.2 | SDD-002 §2.2 | UT-1.2-001..006 | IT-001 | ST-002 | — | — |
| SRS-FUNC-003 | SWI-1 | SWU-1.3 | SDD-002 §2.3 | UT-1.3-001..008 | IT-001 | ST-003 | HAZ-003 | SHA §3 HAZ-003 |
| SRS-FUNC-004 | SWI-1 | SWU-1.4 | SDD-002 §2.4 | UT-1.4-001..006 | IT-001 | ST-004 | HAZ-004 | SHA §3 HAZ-004 |
| SRS-FUNC-010 | SWI-2 | SWU-2.1 | SDD-002 §3.1 | UT-2.1-001..003 | IT-002 | ST-010 | — | — |
| SRS-FUNC-011 | SWI-2 | SWU-2.2 | SDD-002 §3.2 | UT-2.2-001..005 | IT-002 | ST-011 | — | — |
| SRS-FUNC-012 | SWI-2 | SWU-2.3 | SDD-002 §3.3 | UT-2.3-001..006 | IT-002 | ST-012 | — | — |
| SRS-FUNC-013 | SWI-2 | SWU-2.4 | SDD-002 §3.4 | UT-2.4-001..004 | IT-002 | ST-013 | HAZ-005 | SHA §3 HAZ-005 |
| SRS-FUNC-014 | SWI-2 | SWU-2.5 | SDD-002 §3.5 | UT-2.5-001..008 | IT-002 | ST-014 | — | — |
| SRS-FUNC-015 | SWI-2 | SWU-2.6 | SDD-002 §3.6 | UT-2.6-001..004 | IT-002 | ST-015 | — | — |
| SRS-FUNC-016 | SWI-2 | SWU-2.7,2.8,2.10 | SDD-002 §3.7,3.8,3.10 | UT-2.7/2.8/2.10-* | IT-002 | ST-016 | — | — |
| SRS-FUNC-017 | SWI-2 | SWU-2.9 | SDD-002 §3.9 | UT-2.9-001..006 | IT-002 | ST-017 | — | — |
| SRS-FUNC-018 | SWI-2 | SWU-2.11,2.12 | SDD-002 §3.11,3.12 | UT-2.11/2.12-* | IT-002 | ST-018 | HAZ-008 | SHA §3 HAZ-008 |
| SRS-FUNC-020 | SWI-3 | SWU-3.1 | SDD-002 §4.1 | UT-3.1-001..003 | IT-003 | ST-020 | — | — |
| SRS-FUNC-021 | SWI-3 | SWU-3.2 | SDD-002 §4.2 | UT-3.2-001..005 | IT-003 | ST-021 | HAZ-006 | SHA §3 HAZ-006 |
| SRS-FUNC-022 | SWI-3 | SWU-3.3 | SDD-002 §4.3 | UT-3.3-001..006 | IT-003 | ST-022 | HAZ-007 | SHA §3 HAZ-007 |
| SRS-FUNC-023 | SWI-3 | SWU-3.3 | SDD-002 §4.3 | UT-3.3-003,005 | IT-003 | ST-023 | — | — |
| SRS-FUNC-030 | SWI-4 | SWU-4.1,4.2 | SDD-002 §5.1,5.2 | UT-4.1/4.2-001..008 | IT-003 | ST-030 | — | — |
| SRS-FUNC-031 | SWI-4 | SWU-4.3 | SDD-002 §5.3 | UT-4.3-001..004 | IT-003 | ST-031 | — | — |
| SRS-FUNC-032 | SWI-4 | SWU-4.2 | SDD-002 §5.2 | UT-4.2-002,004 | IT-003 | ST-030 | — | — |
| SRS-SAFE-001 | SWI-1,5 | SWU-5.1 | SDD-002 §6.1 | UT-5.1-001..003 | IT-005 | ST-SAFE-001 | HAZ-001 | SHA §3 HAZ-001 |
| SRS-SAFE-002 | SWI-5 | SWU-5.5 | SDD-002 §6.5 | UT-5.5-001..005 | IT-002 | ST-SAFE-002 | HAZ-002 | SHA §3 HAZ-002 |
| SRS-SAFE-003 | SWI-1,5 | SWU-1.3,5.3 | SDD-002 §2.3,6.3 | UT-1.3-008,UT-5.3-001 | IT-005,011 | ST-SAFE-003 | HAZ-003 | SHA §3 HAZ-003 |
| SRS-SAFE-004 | SWI-1,4 | SWU-1.4,4.2 | SDD-002 §2.4,5.2 | UT-1.4-006,UT-4.2-005 | IT-001 | ST-SAFE-004 | HAZ-004 | SHA §3 HAZ-004 |
| SRS-SAFE-005 | SWI-2,5 | SWU-2.4,5.5 | SDD-002 §3.4,6.5 | UT-2.4-003..004,UT-5.5-002..003 | IT-002 | ST-SAFE-005 | HAZ-005 | SHA §3 HAZ-005 |
| SRS-SAFE-006 | SWI-3,5 | SWU-3.2,5.3 | SDD-002 §4.2,6.3 | UT-3.2-004..005 | IT-003 | ST-SAFE-006 | HAZ-006 | SHA §3 HAZ-006 |
| SRS-SAFE-007 | SWI-3,5 | SWU-3.3,5.3 | SDD-002 §4.3,6.3 | UT-3.3-002,004 | IT-003 | ST-SAFE-007 | HAZ-007 | SHA §3 HAZ-007 |
| SRS-SAFE-008 | SWI-2,3 | SWU-2.11,3.3 | SDD-002 §3.11,4.3 | UT-2.11-005,UT-3.3-007 | IT-002 | ST-SAFE-008 | HAZ-008 | SHA §3 HAZ-008 |
| SRS-SAFE-009 | SWI-3,5 | SWU-3.3,5.7 | SDD-002 §4.3,6.7 | UT-3.3-008,UT-5.7-002 | IT-003 | ST-SAFE-009 | HAZ-009 | SHA §3 HAZ-009 |
| SRS-PERF-001 | SWI-1 | All SWU-1.x | SDD-002 §2 | ST-PERF-001 | IT-010 | ST-PERF-001 | — | — |
| SRS-PERF-002 | All SWI | All SWU | SDD-002 §6.7 | — | IT-009 | ST-PERF-002 | — | — |
| SRS-PERF-003 | SWI-3 | SWU-3.2 | SDD-002 §4.2 | UT-3.2-interactive | IT-004 | ST-PERF-003 | — | — |
| SRS-PERF-004 | SWI-5 | SWU-5.1 | SDD-002 §6.1 | UT-5.1-002 | IT-006 | ST-PERF-004 | — | — |

## 3. 리스크 제어 추적성 (7.3.3)

| 해저드 ID | 리스크 제어 (SRM) | SRS-SAFE 요구사항 | 아키텍처 | 유닛 | 검증 테스트 |
|-----------|-------------------|:------------:|:------------:|:----:|:-----------------:|
| HAZ-001 | 비파괴 처리 | SRS-SAFE-001 | SWI-1,5 분리 | SWU-5.1 | ST-SAFE-001 |
| HAZ-002 | 검증된 사전설정 | SRS-SAFE-002 | SWI-5 ParameterValidator | SWU-5.5 | ST-SAFE-002 |
| HAZ-003 | 결함 보정 실패 알림 | SRS-SAFE-003 | SWI-1,5 ErrorHandler | SWU-1.3,5.3 | ST-SAFE-003 |
| HAZ-004 | 고스트 보정 DICOM 태그 | SRS-SAFE-004 | SWI-1,4 메타데이터 | SWU-1.4,4.2 | ST-004 |
| HAZ-005 | 향상 게인 제한 | SRS-SAFE-005 | SWI-2,5 검증자 | SWU-2.4,5.5 | ST-013 |
| HAZ-006 | W/L 범위 경고 | SRS-SAFE-006 | SWI-3,5 ErrorHandler | SWU-3.2,5.3 | ST-SAFE-006 |
| HAZ-007 | GSDF 준수 경고 | SRS-SAFE-007 | SWI-3,5 ErrorHandler | SWU-3.3,5.3 | ST-SAFE-007 |
| HAZ-008 | AI 처리 레이블 | SRS-SAFE-008 | SWI-2,3 오버레이 | SWU-2.11,3.3 | ST-SAFE-008 |
| HAZ-009 | 원본/처리 토글 | SRS-SAFE-009 | SWI-3,5 오케스트레이터 | SWU-3.3,5.7 | ST-SAFE-009 |

## 4. 추적성 요약

| 방향 | 총 항목 | 추적됨 | 추적성 |
|-----------|:-----------:|:------:|:--------:|
| SRS → 아키텍처 (SAD) | 35 | 35 | 100% |
| SRS → 유닛 ID (SDD-001) | 35 | 35 | 100% |
| SRS → 상세 설계 (SDD-002) | 35 | 35 | 100% |
| SRS → 유닛 테스트 (STP-001) | 35 | 35 | 100% |
| SRS → 시스템 테스트 | 35 | 35 | 100% |
| 해저드 (SHA-001) → 리스크 제어 → 테스트 | 9 | 9 | 100% |

---

## 개정 이력

| 개정판 | 날짜 | 작성자 | 설명 |
|-----|------|--------|-------------|
| 1.0 | 2026-04-03 | XPE Team | 초기 릴리스 |
| 1.1 | 2026-04-14 | XPE Team | SDD-002 섹션 참조, SHA-001 해저드 참조, STP-001 테스트 ID 업데이트 추가됨 |

---

*문서 끝 — XPE-RTM-001 v1.0*
