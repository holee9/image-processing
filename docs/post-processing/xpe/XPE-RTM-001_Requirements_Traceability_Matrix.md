# Requirements Traceability Matrix

**Document ID:** XPE-RTM-001 v1.0  
**IEC 62304 Clause:** 5.1.1c, 5.3.6, 7.3.3  
**Safety Classification:** Class B  
**Date:** 2026-04-03  
**Author:** XPE Development Team  
**Approval:** __________________ Date: __________  

---

## 1. Purpose

시스템 요구사항 → 소프트웨어 요구사항 → 아키텍처 → 소프트웨어 유닛 → 테스트 → 리스크 제어 간의 양방향 추적성을 제공한다.

## 2. Forward Traceability (SRS → Test)

| SRS Req ID | Arch (SAD) | Unit (SDD) | Unit Test | Integration Test | System Test | Risk Ref |
|-----------|:----------:|:----------:|:---------:|:---------------:|:-----------:|:--------:|
| SRS-FUNC-001 | SWI-1 | SWU-1.1 | UT-1.1-001..005 | IT-001 | ST-001 | — |
| SRS-FUNC-002 | SWI-1 | SWU-1.2 | UT-1.2-001..004 | IT-001 | ST-002 | — |
| SRS-FUNC-003 | SWI-1 | SWU-1.3 | UT-1.3-001..008 | IT-001 | ST-003 | HAZ-003 |
| SRS-FUNC-004 | SWI-1 | SWU-1.4 | UT-1.4-001..006 | IT-001 | ST-004 | HAZ-004 |
| SRS-FUNC-010 | SWI-2 | SWU-2.1 | UT-2.1-001..003 | IT-002 | ST-010 | — |
| SRS-FUNC-011 | SWI-2 | SWU-2.2 | UT-2.2-001..005 | IT-002 | ST-011 | — |
| SRS-FUNC-012 | SWI-2 | SWU-2.3 | UT-2.3-001..006 | IT-002 | ST-012 | — |
| SRS-FUNC-013 | SWI-2 | SWU-2.4 | UT-2.4-001..004 | IT-002 | ST-013 | HAZ-005 |
| SRS-FUNC-014 | SWI-2 | SWU-2.5 | UT-2.5-001..008 | IT-002 | ST-014 | — |
| SRS-FUNC-015 | SWI-2 | SWU-2.6 | UT-2.6-001..004 | IT-002 | ST-015 | — |
| SRS-FUNC-016 | SWI-2 | SWU-2.7,2.8,2.10 | UT-2.7/2.8/2.10-* | IT-002 | ST-016 | — |
| SRS-FUNC-017 | SWI-2 | SWU-2.9 | UT-2.9-001..006 | IT-002 | ST-017 | — |
| SRS-FUNC-018 | SWI-2 | SWU-2.11,2.12 | UT-2.11/2.12-* | IT-002 | ST-018 | HAZ-008 |
| SRS-FUNC-020 | SWI-3 | SWU-3.1 | UT-3.1-001..003 | IT-003 | ST-020 | — |
| SRS-FUNC-021 | SWI-3 | SWU-3.2 | UT-3.2-001..005 | IT-003 | ST-021 | HAZ-006 |
| SRS-FUNC-022 | SWI-3 | SWU-3.3 | UT-3.3-001..004 | IT-003 | ST-022 | HAZ-007 |
| SRS-FUNC-023 | SWI-3 | SWU-3.3 | UT-3.3-005..006 | IT-003 | ST-023 | — |
| SRS-FUNC-030 | SWI-4 | SWU-4.1,4.2 | UT-4.1/4.2-001..008 | IT-003 | ST-030 | — |
| SRS-FUNC-031 | SWI-4 | SWU-4.3 | UT-4.3-001..004 | IT-003 | ST-031 | — |
| SRS-FUNC-032 | SWI-4 | SWU-4.2 | UT-4.2-009..010 | IT-003 | ST-030 | — |
| SRS-SAFE-001 | SWI-1,5 | SWU-5.1 | UT-5.1-001..003 | IT-005 | ST-SAFE-001 | HAZ-001 |
| SRS-SAFE-002 | SWI-5 | SWU-5.5 | UT-5.5-001..005 | IT-002 | ST-SAFE-002 | HAZ-002 |
| SRS-SAFE-003 | SWI-1,5 | SWU-1.3,5.3 | UT-1.3-008,UT-5.3-001 | IT-005 | ST-SAFE-003 | HAZ-003 |
| SRS-SAFE-004 | SWI-1,4 | SWU-1.4,4.2 | UT-1.4-006,UT-4.2-011 | IT-001 | ST-004 | HAZ-004 |
| SRS-SAFE-005 | SWI-2,5 | SWU-2.4,5.5 | UT-2.4-004,UT-5.5-003 | IT-002 | ST-013 | HAZ-005 |
| SRS-SAFE-006 | SWI-3,5 | SWU-3.2,5.3 | UT-3.2-005,UT-5.3-002 | IT-003 | ST-SAFE-006 | HAZ-006 |
| SRS-SAFE-007 | SWI-3,5 | SWU-3.3,5.3 | UT-3.3-004,UT-5.3-003 | IT-003 | ST-SAFE-007 | HAZ-007 |
| SRS-SAFE-008 | SWI-2,3 | SWU-2.11,3.3 | UT-2.11-005,UT-3.3-007 | IT-002 | ST-SAFE-008 | HAZ-008 |
| SRS-SAFE-009 | SWI-3,5 | SWU-3.3,5.7 | UT-3.3-008,UT-5.7-002 | IT-003 | ST-SAFE-009 | HAZ-009 |
| SRS-PERF-001 | SWI-1 | All SWU-1.x | UT-PERF-001 | IT-001 | ST-PERF-001 | — |
| SRS-PERF-002 | All SWI | All SWU | — | IT-004 | ST-PERF-002 | — |
| SRS-PERF-003 | SWI-3 | SWU-3.2 | UT-PERF-003 | IT-004 | ST-PERF-003 | — |
| SRS-PERF-004 | SWI-5 | SWU-5.1 | UT-PERF-004 | IT-006 | ST-PERF-004 | — |

## 3. Risk Control Traceability (7.3.3)

| Hazard ID | Risk Control (SRM) | SRS-SAFE Req | Architecture | Unit | Verification Test |
|-----------|-------------------|:------------:|:------------:|:----:|:-----------------:|
| HAZ-001 | Non-destructive processing | SRS-SAFE-001 | SWI-1,5 segregation | SWU-5.1 | ST-SAFE-001 |
| HAZ-002 | Validated presets | SRS-SAFE-002 | SWI-5 ParameterValidator | SWU-5.5 | ST-SAFE-002 |
| HAZ-003 | Defect correction failure alert | SRS-SAFE-003 | SWI-1,5 ErrorHandler | SWU-1.3,5.3 | ST-SAFE-003 |
| HAZ-004 | Ghost correction DICOM tag | SRS-SAFE-004 | SWI-1,4 metadata | SWU-1.4,4.2 | ST-004 |
| HAZ-005 | Enhancement gain limiting | SRS-SAFE-005 | SWI-2,5 validator | SWU-2.4,5.5 | ST-013 |
| HAZ-006 | W/L range warning | SRS-SAFE-006 | SWI-3,5 ErrorHandler | SWU-3.2,5.3 | ST-SAFE-006 |
| HAZ-007 | GSDF compliance warning | SRS-SAFE-007 | SWI-3,5 ErrorHandler | SWU-3.3,5.3 | ST-SAFE-007 |
| HAZ-008 | AI-processed label | SRS-SAFE-008 | SWI-2,3 overlay | SWU-2.11,3.3 | ST-SAFE-008 |
| HAZ-009 | Original/processed toggle | SRS-SAFE-009 | SWI-3,5 orchestrator | SWU-3.3,5.7 | ST-SAFE-009 |

## 4. Coverage Summary

| Direction | Total Items | Traced | Coverage |
|-----------|:-----------:|:------:|:--------:|
| SRS → Architecture | 35 | 35 | 100% |
| SRS → Unit | 35 | 35 | 100% |
| SRS → System Test | 35 | 35 | 100% |
| Hazard → Risk Control → Test | 9 | 9 | 100% |

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-03 | XPE Team | Initial release |

---

*Document End — XPE-RTM-001 v1.0*
