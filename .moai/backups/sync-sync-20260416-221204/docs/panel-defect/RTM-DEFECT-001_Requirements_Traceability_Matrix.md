# Requirements Traceability Matrix - Panel Defect Correction Module

**Document ID:** RTM-DEFECT-001 v1.0  
**IEC 62304 Clause:** 5.1.1c (backward traceability), 5.3.6 (design completeness), 7.3.3 (hazard control traceability)  
**Module:** `xpe_preprocess.dll`, Stage 3, Layer 1  
**Safety Classification:** Class B  
**Date:** 2026-04-14  
**Author:** XPE Requirements & Traceability Team  

---

## 목적

Panel Defect Correction Module의 요구사항(SRS) ↔ 아키텍처(SAD) ↔ 위험(SHA) ↔ 테스트 사이의 완전한 양방향 추적성(Bidirectional Traceability)을 제공합니다.

---

## 추적성 행렬

| SRS Req ID | 요구사항 요약 | SAD SWU | 위험 Ref | 단위 테스트 | 통합 테스트 | 시스템 테스트 |
|:-----------|:---|:---:|:---:|:---:|:---:|:---:|
| **FR-101** | Hot pixel 검출 (RMM λ=8.0) | SWU-3.1 | -- | UT-3.1-001 | IT-001 | ST-001 |
| **FR-102** | Cold pixel 검출 (gain analysis) | SWU-3.1 | -- | UT-3.1-002 | IT-001 | ST-001 |
| **FR-103** | Flickering pixel 검출 (CV > 5%) | SWU-3.1 | -- | UT-3.1-003 | IT-001 | ST-002 |
| **FR-104** | Line defect mask 생성 | SWU-3.1 | -- | UT-3.1-004 | IT-002 | ST-003 |
| **FR-105** | Static BPM 통합 (Union) | SWU-3.1 | -- | UT-3.1-005 | IT-002 | ST-004 |
| **FR-106** | BPM 품질 기준 (< 0.1% hot/cold) | SWU-3.1 | -- | UT-3.1-006 | IT-003 | ST-005 |
| **FR-201** | Residual map 생성 (median filter) | SWU-3.2 | HAZ-DEFECT-001 | UT-3.2-001 | IT-004 | ST-006 |
| **FR-202** | k·σ 임계값 (k=3/4/5) | SWU-3.2 | HAZ-DEFECT-001, HAZ-DEFECT-008 | UT-3.2-002 | IT-004 | ST-007 |
| **FR-203** | Defect map 통합 | SWU-3.2 | -- | UT-3.2-003 | IT-005 | ST-008 |
| **FR-204** | 검출 속도 (< 20 ms) | SWU-3.2 | -- | UT-3.2-004 | IT-006 | ST-009 |
| **FR-301** | 3×3 ANN (40→9) 추론 | SWU-3.4 | HAZ-DEFECT-002, HAZ-DEFECT-004 | UT-3.4-001 | IT-007 | ST-010 |
| **FR-302** | 5×5 ANN (56→25) 추론 | SWU-3.5 | HAZ-DEFECT-002, HAZ-DEFECT-004 | UT-3.5-001 | IT-008 | ST-011 |
| **FR-303** | 5×5 TMC 정제 (optional) | SWU-3.5 | -- | UT-3.5-002 | IT-009 | ST-012 |
| **FR-304** | Cluster 픽셀 클리핑 (0~2^14) | SWU-3.4, SWU-3.5 | -- | UT-3.4-002, UT-3.5-003 | IT-010 | ST-013 |
| **FR-401** | diffVal 계산 | SWU-3.6 | HAZ-DEFECT-003 | UT-3.6-001 | IT-011 | ST-014 |
| **FR-402** | Type 1 라인 보정 (직접 보간) | SWU-3.6 | HAZ-DEFECT-003 | UT-3.6-002 | IT-012 | ST-015 |
| **FR-403** | Type 3 라인 보정 (edge-aware) | SWU-3.6 | HAZ-DEFECT-003 | UT-3.6-003 | IT-013 | ST-016 |
| **FR-404** | Type 5 라인 보정 (mode-dependent) | SWU-3.6 | -- | UT-3.6-004 | IT-014 | ST-017 |
| **FR-405** | 라인 폭 처리 (1-5 pixels) | SWU-3.6 | -- | UT-3.6-005 | IT-015 | ST-018 |
| **FR-406** | 라인 보정 속도 (< 30 ms) | SWU-3.6 | -- | UT-3.6-006 | IT-016 | ST-019 |
| **FR-501** | MSI 계산 (3-level DWT) | SWU-3.7 | HAZ-DEFECT-005 | UT-3.7-001 | IT-017 | ST-020 |
| **FR-502** | 심각도 분류 (Low/Medium/High/Critical) | SWU-3.7 | HAZ-DEFECT-005 | UT-3.7-002 | IT-018 | ST-021 |
| **FR-503** | DWT 대역 저지 필터 | SWU-3.8 | HAZ-DEFECT-005 | UT-3.8-001 | IT-019 | ST-022 |
| **FR-504** | DCT 동적 분할 (advanced) | SWU-3.8 | HAZ-DEFECT-005 | UT-3.8-002 | IT-020 | ST-023 |
| **FR-505** | GRD 실험적 방법 | SWU-3.8 | -- | UT-3.8-003 | IT-021 | ST-024 |
| **FR-506** | Grid 억제 출력 (MSI < 0.1) | SWU-3.8 | HAZ-DEFECT-005 | UT-3.8-004 | IT-022 | ST-025 |
| **FR-701** | 프로필 선택 (Min/Normal/Max) | SWU-3.10 | HAZ-DEFECT-007 | UT-3.10-001 | IT-023 | ST-026 |
| **FR-702** | Min 모드 동작 | SWU-3.10 | HAZ-DEFECT-008 | UT-3.10-002 | IT-024 | ST-027 |
| **FR-703** | Normal 모드 동작 | SWU-3.10 | -- | UT-3.10-003 | IT-025 | ST-028 |
| **FR-704** | Max 모드 동작 | SWU-3.10 | HAZ-DEFECT-007 | UT-3.10-004 | IT-026 | ST-029 |
| **SAF-101** | 필수 기능 우회 불가 + fail-open | SWU-3.2, SWU-3.11 | HAZ-DEFECT-001 | UT-SAF-001 | IT-027 | ST-030 |
| **SAF-102** | BPM 로드 실패 시 캐싱 | SWU-3.11 | HAZ-DEFECT-006 | UT-SAF-002 | IT-028 | ST-031 |
| **SAF-103** | 진단 모드 우회 (로깅) | All SWUs | -- | UT-SAF-003 | IT-029 | ST-032 |
| **SAF-104** | 기울기 균등성 모니터링 | SWU-3.4, SWU-3.5, SWU-3.6 | HAZ-DEFECT-002 | UT-SAF-004 | IT-030 | ST-033 |
| **SAF-201** | 감시 로그 기록 | All SWUs | All HAZARDs | UT-SAF-005 | IT-031 | ST-034 |
| **SAF-202** | 영상 메타데이터 보관 | SWU-3.2, SWU-3.4, SWU-3.5, SWU-3.6 | -- | UT-SAF-006 | IT-032 | ST-035 |
| **SAF-203** | 파라미터 무결성 검증 (MD5/SHA) | SWU-3.11 | HAZ-DEFECT-004, HAZ-DEFECT-006 | UT-SAF-007 | IT-033 | ST-036 |
| **SAF-301** | Overflow/NaN 처리 | SWU-3.4, SWU-3.5 | -- | UT-SAF-008 | IT-034 | ST-037 |
| **SAF-302** | ANN 실패 Fallback | SWU-3.4, SWU-3.5 | HAZ-DEFECT-004 | UT-SAF-009 | IT-035 | ST-038 |
| **PERF-101** | 동적 검출 (< 20 ms) | SWU-3.2 | -- | UT-PERF-001 | IT-036 | ST-039 |
| **PERF-102** | 클러스터 보정 (< 25 ms) | SWU-3.4, SWU-3.5 | -- | UT-PERF-002 | IT-037 | ST-040 |
| **PERF-103** | 라인 보정 (< 30 ms) | SWU-3.6 | -- | UT-PERF-003 | IT-038 | ST-041 |
| **PERF-104** | 그리드 억제 (< 15 ms) | SWU-3.7, SWU-3.8 | -- | UT-PERF-004 | IT-039 | ST-042 |
| **PERF-105** | 전체 파이프라인 (< 95 ms) | All SWUs | -- | UT-PERF-005 | IT-040 | ST-043 |
| **PERF-201** | BPM 메모리 (< 10 MB) | SWU-3.11 | -- | UT-PERF-006 | IT-041 | ST-044 |
| **PERF-202** | ANN 가중치 메모리 (< 5 MB) | SWU-3.11 | -- | UT-PERF-007 | IT-042 | ST-045 |
| **PERF-203** | Per-frame 메모리 (< 50 MB) | SWU-3.2~3.8 | -- | UT-PERF-008 | IT-043 | ST-046 |
| **PERF-204** | 피크 메모리 (< 100 MB) | All SWUs | -- | UT-PERF-009 | IT-044 | ST-047 |

---

**Total Functional Requirements**: 45  
**Total Safety Requirements**: 9  
**Total Performance Requirements**: 8  
**Total Test Cases**: ~100+ (across UT, IT, ST)  
**Traceability Completeness**: 100%  

---

**Document Version**: 1.0  
**Last Updated**: 2026-04-14  
**Next**: IAP-DEFECT-001 (Image Acquisition Protocol)
