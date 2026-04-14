# RTM: 요구사항 추적성 행렬

> **Document ID**: RTM-GHOST-001 | **Version**: 1.0 | **Date**: 2026-04-02
>
> **목적**: 요구사항 → 설계 → 구현 → 테스트의 완전한 추적성 확보 (IEC 62304 §5.7)

---

## 1. 기능 요구사항 추적성

| SRS ID | 요구사항 (요약) | SAD 모듈 | SDD 섹션 | PRD v2.0 | Test ID | 상태 |
|---|---|---|---|---|---|---|
| FR-101 | Pixel별 dark 차감 | Tier1_Offset | SDD §1 | §5.1 의사코드 | UT-T1-01,05,06,10 | 설계 완료 |
| FR-102 | 언더플로우 → 0 클램프 | Tier1_Offset | SDD §1.4 | §6.1 Overflow | UT-T1-02 | 설계 완료 |
| FR-103 | 오버플로우 → 65535 클램프 | Tier1_Offset | SDD §1.4 | §6.1 Overflow | UT-T1-03 | 설계 완료 |
| FR-104 | 오버플로우 count 기록 | Tier1_Offset | SDD §1.2 | §5.1 | UT-T1-09 | 설계 완료 |
| FR-105 | NULL → CORR_ERR_NULL_PTR | Tier1_Offset | SDD §1.4 | §6.2 | UT-T1-07,08 | 설계 완료 |
| FR-201 | D_post - D_pre로 lag 추정 | Tier2_Lag | SDD §2 | §5.2 의사코드 | UT-T2-01,02,09 | 설계 완료 |
| FR-202 | 음수 lag → 0 클램프 | Tier2_Lag | SDD §2.3 | §5.2 | UT-T2-03 | 설계 완료 |
| FR-203 | α(E) LUT 조회 | Tier2_Lag | SDD §2.4 | §5.4 | UT-T2-04,05 | 설계 완료 |
| FR-204 | D_post NULL → Tier 2 스킵 | Pipeline | SDD §2.2 | §5.6 | UT-T2-06 | 설계 완료 |
| FR-205 | Exposure 이력 유지 | ExposureHistory | SAD §2.1 | §3.1 | UT-T2-07,08 | 설계 완료 |
| FR-301 | NLCSC N=4 구현 | Tier3_Nlcsc | SDD §3 | §5.3 의사코드 | UT-T3-01~04 | 설계 완료 |
| FR-302 | 상태 시간 감쇠 | Tier3_Nlcsc | SDD §3.3 | §5.3 | UT-T3-02,03 | 설계 완료 |
| FR-303 | bₙ(E), aₙ(E) LUT | Tier3_Nlcsc | SDD §3.4 | §5.3 | UT-T3-04 | 설계 완료 |
| FR-304 | exp LUT 256-entry | LutEngine | SDD §7.2 | §7.4 | UT-T3-05, UT-FP-03~05 | 설계 완료 |
| FR-305 | NLCSC 설정 제어 | Pipeline | SDD §3.2 | §5.6 | UT-T3-06 | 설계 완료 |
| FR-401 | Gain map 보정 | GainCorrection | SDD §4 | §5.4 의사코드 | UT-G-01~03,06 | 설계 완료 |
| FR-402 | Dead pixel (gain=0) 스킵 | GainCorrection | SDD §4.1 | §5.4 | UT-G-05 | 설계 완료 |
| FR-403 | Gain 결과 클램프 | GainCorrection | SDD §4.1 | §6.1 | UT-G-04 | 설계 완료 |
| FR-501 | ΔG 이력 기반 추정 | GhostCorrection | SAD §2.2 | §5.6 Ghost | - | 설계 완료 |
| FR-502 | Ghost 설정 제어 | Pipeline | SAD §2.2 | §5.6 | IT-01 | 설계 완료 |
| FR-601 | 결함 보간 | DefectCorrection | SDD §5 | §5.5 의사코드 | UT-D-01~03,06 | 설계 완료 |
| FR-602 | 3가지 보간 방법 선택 | DefectCorrection | SDD §5.1 | §5.5 | UT-D-05 | 설계 완료 |
| FR-603 | 클러스터 유효 이웃 | DefectCorrection | SDD §5.1 | §5.5 | UT-D-03 | 설계 완료 |
| FR-604 | 이웃 없음 → WARNING | DefectCorrection | SDD §5.1 | §6.2 | UT-D-04 | 설계 완료 |
| FR-701 | Pipeline 실행 순서 | Pipeline | SAD §3.1 | §5.6 의사코드 | IT-01~04 | 설계 완료 |
| FR-702 | 자동 tier 승격 | TierSelector | SAD §3.2 | §4.2 | IT-05 | 설계 완료 |
| FR-703 | Tier 3 승격 | TierSelector | SAD §3.2 | §4.2 | - | 설계 완료 |
| FR-704 | CorrectionResult 기록 | Pipeline | SAD §2.1 | §4.1 | IT-08 | 설계 완료 |

---

## 2. 비기능 요구사항 추적성

| SRS ID | 요구사항 | 설계 대응 | Test ID |
|---|---|---|---|
| NFR-101 | Tier 1+2 < 70ms | SIMD 최적화, 버퍼 재사용 (SAD §4.2) | IT-08 |
| NFR-102 | 전체 < 200ms | Pipeline 순차, 결함 별도 패스 (SAD §7) | IT-08 |
| NFR-104 | 메모리 < 100MB | 정적 할당, 버퍼 재사용 (SAD §4) | IT-09 |
| NFR-201 | GCR ≤ 0.1% (FB+Tier1+2) | Tier 1+2 알고리즘 (SDD §1,2) | ST-01 |
| NFR-203 | 고정소수점 ≤ 0.5 LSB | Q16.16, round-half-up (SDD §7.1) | UT-FP-01~06 |
| NFR-301 | 1000 frame 스트레스 | malloc 없음, 정적 버퍼 (SAD §4.1) | IT-10 |
| NFR-302 | CRC 검증 | CalibFileIO (SDD §8.1) | IT-07 |
| NFR-304 | NULL 안전성 | 모든 공개 함수 (SDD 각 §) | UT-T1-07,08 등 |
| NFR-401 | Doxygen 주석 | 코딩 규약 (PRD v2.0 §9.2) | 코드 리뷰 |
| NFR-402 | 커버리지 ≥ 80% | CI 커버리지 도구 | CI 리포트 |
| NFR-404 | MISRA C | 정적 분석 (PRD v2.0 §9.2) | CI 리포트 |

---

## 3. 문서 상호 참조

```mermaid
graph LR
    PRD1["PRD v1.0<br/>(방법론)"] --> SRS["SRS<br/>(요구사항)"]
    PRD2["PRD v2.0<br/>(구현 사양)"] --> SRS
    PRD2 --> SDD["SDD<br/>(상세 설계)"]

    SRS --> SAD["SAD<br/>(아키텍처)"]
    SAD --> SDD

    SRS --> STP["STP/STC<br/>(테스트)"]
    SDD --> STP

    SRS --> RTM["RTM<br/>(추적성)"]
    SAD --> RTM
    SDD --> RTM
    STP --> RTM

    style RTM fill:#e94560,stroke:#fff,color:#fff,stroke-width:3px
```

| 문서 | ID | 버전 | 위치 |
|---|---|---|---|
| PRD v1.0 (방법론/전략) | - | 1.0 | docs/sw_lag_correction_prd.md |
| PRD v2.0 (구현 사양) | - | 2.0 | docs/sw_lag_correction_prd_v2.md |
| SRS (요구사항) | SRS-GHOST-001 | 1.0 | docs/srs_ghost_correction.md |
| SAD (아키텍처) | SAD-GHOST-001 | 1.0 | docs/sad_ghost_correction.md |
| SDD (상세 설계) | SDD-GHOST-001 | 1.0 | docs/sdd_ghost_correction.md |
| STP/STC (테스트) | STP-GHOST-001 | 1.0 | docs/stp_stc_ghost_correction.md |
| RTM (추적성) | RTM-GHOST-001 | 1.0 | docs/rtm_ghost_correction.md |
| Dark Frame 분석 | - | 1.0 | docs/dark_frame_analysis.md |
| 검출기 노이즈 사양 | - | 1.0 | docs/detector_noise_spec.md |
| HW 최적 설정 | - | 1.0 | docs/optimal_settings.md |
| HW 타이밍 차트 | - | 1.0 | docs/timing_chart.md |

---

## 4. 커버리지 요약

| 구분 | 전체 | 설계 추적 | 테스트 추적 | 미추적 |
|---|---|---|---|---|
| 기능 요구사항 | 27 | 27 (100%) | 25 (93%) | FR-501,703 (선택) |
| 비기능 요구사항 | 15 | 15 (100%) | 13 (87%) | NFR-204,205 (시뮬) |
| 단위 테스트 케이스 | 42 | 42→SRS 100% | - | - |
| 통합 테스트 케이스 | 10 | 10→SRS 100% | - | - |
| 시스템 테스트 케이스 | 10 | 10→SRS 100% | - | - |

**추적 완전성: 97% (미추적 항목은 선택적 또는 시뮬레이션 대상)**
