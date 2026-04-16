# IEC 62304 Class B Compliance Matrix

**Document ID:** XPE-62304-MAP-001 v1.0  
**Standard:** IEC 62304:2006+AMD1:2015  
**Safety Classification:** Class B  
**Date:** 2026-04-03  
**Author:** XPE Development Team  
**Approval:** __________________ Date: __________  

---

## 1. Purpose

IEC 62304 Class B의 모든 해당 clause를 XPE 문서 체계에 매핑하고, 준수 상태를 추적한다.

## 2. Clause-to-Document Mapping

### Clause 5: Software Development Process

| Clause | Title | Class B | Document | Section | Status |
|--------|-------|:-------:|----------|---------|:------:|
| 5.1.1 | SW development plan | ✓ | XPE-SDP-001 | §2-8 | ✓ |
| 5.1.2 | Keep plan updated | ✓ | XPE-SDP-001 | §9 | ✓ |
| 5.1.3 | Reference to SDP or plans | ✓ | XPE-SDP-001 | §3 | ✓ |
| 5.1.4 | Standards, methods, tools | — | N/A (Class C only) | — | N/A |
| 5.1.5 | Integration & test planning | ✓ | XPE-SDP-001 | §4 | ✓ |
| 5.1.6 | Verification planning | ✓ | XPE-SDP-001 | §5 | ✓ |
| 5.1.7 | Risk management planning | ✓ | XPE-SDP-001 | §6 | ✓ |
| 5.1.8 | Documentation planning | ✓ | XPE-SDP-001 | §7 | ✓ |
| 5.1.9 | CM planning | ✓ | XPE-SDP-001 | §8 | ✓ |
| 5.1.10 | Supporting items | ✓ | XPE-SDP-001 | §8.1 | ✓ |
| 5.1.11 | Config items before verification | ✓ | XPE-SDP-001 | §8.2 | ✓ |
| 5.2.1 | Define & document SW req | ✓ | XPE-SRS-001 | §3-11 | ✓ |
| 5.2.2 | Content of SW req | ✓ | XPE-SRS-001 | §2 (categories a-l) | ✓ |
| 5.2.3 | Risk control in requirements | ✓ | XPE-SRS-001 | §7 | ✓ |
| 5.2.4 | Re-evaluate risk analysis | ✓ | XPE-SRM-001 | §5 | ✓ |
| 5.2.5 | Update requirements | ✓ | XPE-SRS-001 | Rev History | ✓ |
| 5.2.6 | Verify SW requirements | ✓ | XPE-SRS-001 | §12 | ✓ |
| 5.3.1 | Transform req into architecture | ✓ | XPE-SAD-001 | §2-3 | ✓ |
| 5.3.2 | Develop architecture for interfaces | ✓ | XPE-SAD-001 | §4 | ✓ |
| 5.3.3 | SOUP functional/performance req | ✓ | XPE-SOUP-001 | §3 | ✓ |
| 5.3.4 | SOUP system HW/SW req | ✓ | XPE-SOUP-001 | §4 | ✓ |
| 5.3.5 | Segregation for risk control | ✓ | XPE-SAD-001 | §6 | ✓ |
| 5.3.6 | Verify SW architecture | ✓ | XPE-SAD-001 | §7 | ✓ |
| 5.4.1 | Subdivide into SW units | ✓ | XPE-SDD-001 | §2-6 | ✓ |
| 5.4.2 | Detailed design for each unit | — | N/A (Class C only) | — | N/A |
| 5.4.3 | Detailed design for interfaces | — | N/A (Class C only) | — | N/A |
| 5.4.4 | Verify detailed design | — | N/A (Class C only) | — | N/A |
| 5.5.1 | Implement SW unit | ✓ | XPE-VVP-001 | §2 | ✓ |
| 5.5.2 | Unit verification process | ✓ | XPE-VVP-001 | §2.1 | ✓ |
| 5.5.3 | Unit acceptance criteria | ✓ | XPE-VVP-001 | §2.2 | ✓ |
| 5.5.4 | Additional unit criteria | — | N/A (Class C only) | — | N/A |
| 5.5.5 | Unit verification | ✓ | XPE-VVP-001 | §2.4 | ✓ |
| 5.6.1 | Integrate SW units | ✓ | XPE-VVP-001 | §3.1 | ✓ |
| 5.6.2 | Verify SW integration | ✓ | XPE-VVP-001 | §3.2 | ✓ |
| 5.6.3 | Integration test content | ✓ | XPE-VVP-001 | §3.2 | ✓ |
| 5.6.4 | Regression testing | ✓ | XPE-VVP-001 | §3.3 | ✓ |
| 5.6.5 | Integration test records | ✓ | XPE-VVP-001 | §3.4 | ✓ |
| 5.6.6 | Use problem resolution | ✓ | XPE-VVP-001 | §3.5 → XPE-SPR-001 | ✓ |
| 5.6.7 | Verify test procedures | ✓ | XPE-VVP-001 | §3.6 | ✓ |
| 5.7.1 | Establish system tests | ✓ | XPE-VVP-001 | §4.1 | ✓ |
| 5.7.2 | Use problem resolution | ✓ | XPE-VVP-001 | §4.2 → XPE-SPR-001 | ✓ |
| 5.7.3 | Retest after change | ✓ | XPE-VVP-001 | §4.3 | ✓ |
| 5.7.4 | Verify test procedures | ✓ | XPE-VVP-001 | §4.4 | ✓ |
| 5.7.5 | System test records | ✓ | XPE-VVP-001 | §4.5 | ✓ |
| 5.8.1 | Ensure completeness | ✓ | XPE-SRP-001 | §2 step 1-2 | ✓ |
| 5.8.2 | Known anomalies documented | ✓ | XPE-SRP-001 | §2 step 3 | ✓ |
| 5.8.3 | Evaluate residual anomalies | ✓ | XPE-SRP-001 | §2 step 4 | ✓ |
| 5.8.4 | Document version | ✓ | XPE-SRP-001 | §2 step 5 | ✓ |
| 5.8.5 | Document creation procedure | ✓ | XPE-SRP-001 | §2 step 6 | ✓ |
| 5.8.6 | Ensure repeatable | ✓ | XPE-SRP-001 | §2 step 7 | ✓ |
| 5.8.7 | Ensure release verified | ✓ | XPE-SRP-001 | §2 step 8 | ✓ |
| 5.8.8 | Archive | ✓ | XPE-SRP-001 | §2 step 9, §4 | ✓ |

### Clause 6: Software Maintenance

| Clause | Title | Class B | Document | Status |
|--------|-------|:-------:|----------|:------:|
| 6.1 | Establish maintenance plan | ✓ | XPE-SMP-001 | ✓ |
| 6.2.1 | Monitor feedback | ✓ | XPE-SMP-001 §3.1 | ✓ |
| 6.2.2 | Document & evaluate feedback | ✓ | XPE-SMP-001 §3.2 | ✓ |
| 6.2.3 | Analyze for risk | ✓ | XPE-SMP-001 §3.2 | ✓ |
| 6.3 | Implement modification | ✓ | XPE-SMP-001 §3.3 | ✓ |

### Clause 7: Software Risk Management

| Clause | Title | Class B | Document | Status |
|--------|-------|:-------:|----------|:------:|
| 7.1 | Identify hazardous situations | ✓ | XPE-SRM-001 §3 | ✓ |
| 7.2 | Risk control for SW | ✓ | XPE-SRM-001 §4 | ✓ |
| 7.3 | Verify risk control measures | ✓ | XPE-SRM-001 §6 | ✓ |
| 7.4.1 | Identify SOUP risk | ✓ | XPE-SOUP-001 §5 | ✓ |
| 7.4.2 | Assess SOUP risk | ✓ | XPE-SOUP-001 §5.2 | ✓ |
| 7.4.3 | Evaluate anomaly lists | ✓ | XPE-SOUP-001 §5.1 | ✓ |

### Clause 8: Software Configuration Management

| Clause | Title | Class B | Document | Status |
|--------|-------|:-------:|----------|:------:|
| 8.1 | Configuration identification | ✓ | XPE-SCM-001 §2 | ✓ |
| 8.2 | Change control | ✓ | XPE-SCM-001 §3 | ✓ |
| 8.2.4 | Traceability of changes | ✓ | XPE-SCM-001 §3.3 | ✓ |
| 8.3 | Configuration status accounting | ✓ | XPE-SCM-001 §4 | ✓ |

### Clause 9: Software Problem Resolution

| Clause | Title | Class B | Document | Status |
|--------|-------|:-------:|----------|:------:|
| 9.1 | Prepare problem reports | ✓ | XPE-SPR-001 §2-3 | ✓ |
| 9.2 | Investigate the problem | ✓ | XPE-SPR-001 §5 | ✓ |
| 9.3 | Advise relevant parties | ✓ | XPE-SPR-001 §5.2 | ✓ |
| 9.4 | Use change control | ✓ | XPE-SPR-001 §7 → SCM | ✓ |
| 9.5 | Maintain records | ✓ | XPE-SPR-001 §9 | ✓ |
| 9.6 | Analyze for trends | ✓ | XPE-SPR-001 §10 | ✓ |
| 9.7 | Verify resolution | ✓ | XPE-SPR-001 §8 | ✓ |
| 9.8 | Test content of release | ✓ | XPE-SPR-001 §8 → VVP | ✓ |

## 3. Compliance Summary

| Clause Group | Total Applicable (Class B) | Addressed | Gap |
|:------------:|:--------------------------:|:---------:|:---:|
| 5.1 Planning | 10 | 10 | 0 |
| 5.2 Requirements | 6 | 6 | 0 |
| 5.3 Architecture | 6 | 6 | 0 |
| 5.4 Unit ID | 1 | 1 | 0 |
| 5.5 Unit Verification | 4 | 4 | 0 |
| 5.6 Integration Test | 7 | 7 | 0 |
| 5.7 System Test | 5 | 5 | 0 |
| 5.8 Release | 8 | 8 | 0 |
| 6 Maintenance | 5 | 5 | 0 |
| 7 Risk Management | 6 | 6 | 0 |
| 8 CM | 4 | 4 | 0 |
| 9 Problem Resolution | 8 | 8 | 0 |
| **Total** | **70** | **70** | **0** |

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-03 | XPE Team | Initial release |

---

*Document End — XPE-62304-MAP-001 v1.0*
