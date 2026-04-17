# XPE Score Improvement Plan v3.0.0 (61 → 85 → 95)

**Document ID**: SCORE-PLAN-001
**Version**: 3.0.0
**Date**: 2026-04-17
**Status**: Active (Supersedes v2.0.0)
**Relationship**: Extends `score-improvement-plan-85.md` (v2.0.0) with 85→95 path, Framework C, and Must/Should/Could gating
**Sources**:
- score-improvement-plan-85.md v2.0.0 (retained baseline)
- `.moai/project/trend-survey-2026.md` v1.0.0 (normative input)
- SPEC-XPE-MASTER v3.0.0 addendum

---

## Changelog (v2.0.0 → v3.0.0, v3.1 Strict Reclassification)

| Change | Detail |
|--------|--------|
| 85 → 95 path added | 8-step roadmap beyond 85, driven by Should tier adoption |
| Framework C (Future-Value) introduced | Could tier evaluation, 2027+ horizon |
| Must/Should/Could gating | 모든 점수 이동에 티어 게이트 적용 |
| Regulatory scoring reweighted | FDA PCCP + EU AI Act + ISO 42001 가산점 |
| Security scoring reweighted | SBOM + SLSA + threat model 가산점 |
| Post-market scoring | Drift + OTEL + VEX 운영 준비도 |
| **v3.1 Strict Must** | **Must 27→12로 축소. 이에 따라 Must-only 점수 85→76, Must+major-Should 목표 85 재설정.** 엄격 기준 채택 반영 |

---

## 1. Three-Framework System (v3.0)

### 1.1 Framework A (Process/Compliance) — REWEIGHTED

| 영역 | v2.0 배점 | v3.0 배점 | 변경 사유 |
|------|:--------:|:--------:|-----------|
| 요구사항 완전성 (EARS) | 25 | **25** | 유지 |
| 문서 품질 (IEC 62304) | 20 | **18** | -2 (규제 일부 이전) |
| 아키텍처 설계 | 20 | **18** | -2 (Must arch 일부 이전) |
| 구현 진행도 | 20 | **17** | -3 (AI governance 포함 가능 이전) |
| 품질 보증 | 15 | **12** | -3 (보안/운영 이전) |
| **NEW: 규제 준수** | — | **5** | FDA/EU/ISO |
| **NEW: 사이버보안** | — | **3** | FDA §524B |
| **NEW: 운영 준비도** | — | **2** | PMS, drift |
| **합계** | 100 | **100** | 재분배 |

### 1.2 Framework B (Product/Delivery) — REWEIGHTED

| 영역 | v2.0 배점 | v3.0 배점 |
|------|:--------:|:--------:|
| 기능 범위 | 35 | **30** |
| 성능·메모리 | 15 | **13** |
| 알고리즘 품질 | 20 | **17** |
| 규제·문서 | 15 | **15** |
| 운영 준비도 | 15 | **15** |
| **NEW: 상호운용성** | — | **5** |
| **NEW: AI governance** | — | **5** |
| **합계** | 100 | **100** |

### 1.3 Framework C (Future-Value) — NEW

Could 티어와 2027+ 대비 전략적 옵션 가치 측정.

| 영역 | 배점 | 평가 기준 |
|------|:---:|-----------|
| AI Foundation Readiness | 25 | MedSAM/Federated/Foundation model 실험 인프라 |
| Deployment Flexibility | 20 | GPU/WASM/Rust 대안 경로 준비 |
| Long-term Security | 20 | PQC, Zero-trust 로드맵 |
| Advanced Interoperability | 15 | FHIRcast, breakthrough IHE 프로필 |
| Research Velocity | 20 | Continuous experiment, UQ 고도화 |
| **합계** | 100 | 현재 ~15, 목표 60+ (2027) |

---

## 2. Consolidated Targets (v3.1 Strict)

**v3.1 변경**: Must가 12개로 축소됨에 따라 "Must 완료" 단독 점수는 76점(블로커 해소 상태). 목표 85점은 Must + 주요 Should(Phase 연결 관련) 완료가 필요하며, 이를 "Milestone 1-Extended"로 명명.

| Marker | Framework A | Framework B | Framework C | 정의 |
|:------:|:-----------:|:-----------:|:-----------:|------|
| 현재 (2026-04-17) | 61 | (Phase 1b 완료 시 66) | 15 | 출발점 |
| **Milestone 1 (Must 12 완료)** | **76** (was 85) | **74** (was 85) | 20 | 출시 블로커 해소 (법적 최소) |
| **Milestone 1-Ext (Must + 주요 Should)** | **85** | **85** | 30 | 목표 85 = Must + Phase 연결 Should |
| **Milestone 2 (+ 전체 Should)** | **95** | **95** | 45 | 현대적·경쟁력 |
| **Milestone 3 (+ 선별 Could)** | 98 | 98 | 65 | 연구 모드 |
| Long-term (2027+) | 100 | 100 | 80+ | Phase 4 |

### 주요 Should (Milestone 1-Ext 85점 달성 조건)

- S-QUAL-01 (TRUST 5) — 프로젝트 baseline
- S-QUAL-02 (Reference+SIMD Parity) — 품질 기반
- S-QUAL-03 (MX Tag), S-QUAL-04 (Anti-Spaghetti) — 아키텍처
- S-SEC-01 (SLSA L2→L3), S-SEC-05 (IEC 81001-5-1) — 보안 권장
- S-OPS-05 (Reproducible Builds) — SBOM 전제

AI 관련 Should (S-REG-03~07, S-AI-06~08)는 Phase 3 진입 결정 시 개별 승격.

---

## 3. Path Breakdown (v3.1 Strict)

### 3.1 Path 1: 61 → 76 (Must 12만 완료, 블로커 해소)

| 단계 | 행동 | A 기여 | B 기여 | 누적 A | 누적 B |
|:----:|------|:------:|:------:|:------:|:------:|
| 기존 | Phase 0 + Phase 1a/1b 구현 | +10 | +4 | 71 | 70 |
| Must 1 | IEC 62304 + 21 CFR + EU MDR baseline docs | +2 | +1 | 73 | 71 |
| Must 2 | §524B + SBOM + Vuln Mgmt + Input Val | +2 | +1 | 75 | 72 |
| Must 3 | PMS Plan + 보고 체계 | +0 | +1 | 75 | 73 |
| Must 4 | DICOM Core + Conformance Statement 발행 | +0 | +1 | 75 | 74 |
| Must 5 | Char. Test + Trackability (IEC 62304 §5.4.1) | +1 | +0 | 76 | 74 |

**Milestone 1 (Must 12) 달성 조건 → 블로커 해소, 출시 가능 상태**:
- IEC 62304 Class B 전체 서류
- EU MDR + FDA §524B + 21 CFR 증빙
- SBOM (SPDX 3.0 + CycloneDX 1.6) 발급
- Vulnerability Management 프로세스 가동
- PMS Plan 승인
- DICOM Core + Conformance Statement
- 모든 SWU Unit Test + RTM 매핑 완료

### 3.2 Path 1-Ext: 76 → 85 (Must + 주요 Should)

| 단계 | 행동 | A 기여 | B 기여 | 누적 A | 누적 B |
|:----:|------|:------:|:------:|:------:|:------:|
| Ext 1 | TRUST 5 전체 gate green | +2 | +1 | 78 | 75 |
| Ext 2 | Reference + SIMD Parity 적용 | +1 | +2 | 79 | 77 |
| Ext 3 | SLSA L2 달성 | +1 | +1 | 80 | 78 |
| Ext 4 | Anti-Spaghetti + MX Tag 적용 | +1 | +1 | 81 | 79 |
| Ext 5 | Reproducible Builds 증명 | +1 | +1 | 82 | 80 |
| Ext 6 | IEC 81001-5-1 통합 | +1 | +1 | 83 | 81 |
| Ext 7 | ISO 42001 AIMS 수립 | +1 | +1 | 84 | 82 |
| Ext 8 | IHE RAD SWF.b/PDI/PIR 구현 | +1 | +2 | 85 | 84 |
| Ext 9 | benchmark freeze + 재현 harness | 0 | +1 | 85 | 85 |

**Milestone 1-Ext (85 목표) 달성 조건 → Must + Phase 연결 Should**:
- Milestone 1 전체 완료
- 품질/아키텍처 Should 4개 (TRUST 5, Parity, MX, Anti-Spaghetti)
- 보안 Should 2개 (SLSA L2, IEC 81001-5-1)
- 자발적 Should 1개 (ISO 42001 수립)
- 상호운용 Should 1개 (IHE Baseline)
- 운영 Should 1개 (Reproducible)

### 3.3 Path 2: 85 → 95 (AI-Phase 3 + 전체 Should)

| 단계 | 행동 | A 기여 | B 기여 | 누적 A | 누적 B |
|:----:|------|:------:|:------:|:------:|:------:|
| 6 | S-AI-01 SSL Denoising 배포 | +2 | +3 | 87 | 88 |
| 7 | S-AI-04 Conformal UQ + S-AI-05 ONNX 1.20+ | +1 | +2 | 88 | 90 |
| 8 | S-SEC-01 SLSA L3 달성 | +2 | +1 | 90 | 91 |
| 9 | S-IOP-01 DICOMweb 배포 | +1 | +2 | 91 | 93 |
| 10 | S-OPS-01 Drift Detection live | +2 | +1 | 93 | 94 |
| 11 | Model Observer Quality Gate live | +1 | +1 | 94 | 95 |
| 12 | S-IOP-03 IHE AIR/AIRA 시범 운영 | +1 | 0 | 95 | 95 |

**목표 95 달성 조건**:
- Should 19개 중 15개 이상 배포
- Phase 3 AI 모듈 1개 이상 FDA 제출
- Connectathon 1회 이상 참여
- 판매 후 감시 지표 3개월 운영

### 3.4 Path 3: 95 → 98+ (Could 선별 경로)

- MedSAM 연구 프로토타입 (+1)
- GPU production path (CUDA) (+1)
- FHIRcast 파일럿 (+1)
- Rust safety module 실험 (+1)

---

## 4. Prioritized Backlog (v3.0 - reconciled)

### 4.1 즉시 (Q2 2026, 현재 스프린트)

| 우선순위 | 항목 | SPEC | 예상 기여 (A/B/C) |
|:--------:|------|------|:-------:|
| P0 | SPEC-XPE-REG 초안 승인 | REG | +4 / +3 / +2 |
| P0 | SBOM 자동화 (SPDX 3.0 + CycloneDX 1.6) | SEC | +2 / +2 / +1 |
| P0 | Reproducible build CI | OPS | +1 / +2 / 0 |
| P1 | S1-A xpe_preprocess 완료 | P1A | +5 / +5 / 0 |
| P1 | Phase 1b 3개 DLL 시작 | P1B* | +3 / +3 / 0 |
| P2 | Model Card API 스켈레톤 | REG + P3-AI | +1 / +1 / +1 |

### 4.2 Q3-Q4 2026

| 항목 | SPEC | 예상 기여 |
|------|------|:--------:|
| Phase 1b 완료 (enhance_basic, display, dicom) | P1B* | +5 / +5 / 0 |
| S-SEC 전체 완료 | SEC | +3 / +2 / +3 |
| S-OPS 기본 구현 (PMS, reject-analysis) | OPS | +2 / +3 / +1 |
| S2-A, S2-B (advanced, gsvg) | P2-ADV, P2-GSVG | +4 / +6 / 0 |
| DICOM Conformance Statement 발행 | IOP | +1 / +2 / 0 |

### 4.3 2027

| 항목 | SPEC | 예상 기여 |
|------|------|:--------:|
| S-IOP DICOMweb + FHIR R5 배포 | IOP | +2 / +3 / +5 |
| Phase 3 AI (SSL denoising 먼저) | P3-AI | +3 / +6 / +10 |
| EU AI Act 2027-08 deadline 대응 | REG | +5 / +2 / +5 |
| SLSA L3 프로덕션 | SEC | +2 / +1 / +5 |
| OpenTelemetry 프로덕션 배포 | OPS | +1 / +2 / +5 |

---

## 5. Gate Criteria for Each Milestone

### 5.1 Gate to 76 (Must 12 완료, 블로커 해소, v3.1 NEW)

- [ ] IEC 62304 Class B 전체 서류 (Char. Test, RTM 포함)
- [ ] EU MDR 기본 준수 증명
- [ ] FDA 21 CFR 820.30 + QMSR 2026 준비
- [ ] FDA §524B + SBOM 발급
- [ ] Vulnerability Management 프로세스 가동
- [ ] Basic Input Validation 전체 C ABI 경계
- [ ] DICOM Core + Conformance Statement 발행
- [ ] PMS Plan 승인

### 5.2 Gate to 85 (Must + 주요 Should, 재설정)

- [ ] Milestone 1 (76) 전체 완료
- [ ] 품질/아키텍처 Should 4개 (TRUST 5, Reference+SIMD Parity, MX Tag, Anti-Spaghetti)
- [ ] Security Should: SLSA L2, IEC 81001-5-1 통합
- [ ] Ops Should: Reproducible Builds 증명
- [ ] Voluntary 자발: ISO 42001 수립 또는 roadmap
- [ ] Interop Should: IHE SWF.b/PDI/PIR
- [ ] FDA pre-submission (Q-Sub) 1회 이상 실행

### 5.3 Gate to 95 (Must + 전체 Should, AI 포함)

- [ ] 15개 이상 Should 배포 (19개 중)
- [ ] Framework A ≥ 95
- [ ] Framework B ≥ 93
- [ ] Phase 3 AI 1개 이상 FDA 510(k) 또는 PMA 제출
- [ ] IHE Connectathon 1회 참여
- [ ] 판매 후 감시 3개월 운영 데이터
- [ ] Model Observer Quality Gate 4개 모두 통과
- [ ] SLSA L3 달성

### 5.4 Gate to 98+ (Selective Could)

- [ ] Phase 4 Research Track 3개 프로젝트 완료
- [ ] Framework C ≥ 60
- [ ] Foundation model 실험 1개 이상
- [ ] GPU production path 1개 이상

---

## 6. Risk-Adjusted Scoring

각 경로는 다음 리스크 시나리오로 스트레스 테스트:

| 시나리오 | Framework A 영향 | 완화책 |
|---------|:---------------:|-------|
| FDA 가이던스 재발행 (PCCP 수정) | -3 | 모니터링 구독, 30일 내 대응 |
| EU AI Act 이행법 변경 | -4 | EU Notified Body와 정기 소통 |
| 주요 SOUP EOL (예: vcpkg 패키지) | -2 | 분기별 SOUP 건강 상태 체크 |
| 경쟁사 AI 모듈 규제 실패 사례 | 0 | 학습 기회로 활용 |
| Zero-day CVE on DCMTK | -3 | SLA 60일 패치 |
| 파일럿 병원 drift 알림 폭발 | -2 | threshold 재보정 프로세스 |

---

## 7. 관측 가능한 진척도 지표 (KPI)

주간·월간 트래킹:

- **Framework A**: 문서 완성률, EARS 커버리지, RTM 매핑율
- **Framework B**: SWU 구현률, 커버리지, 벤치마크 달성
- **Framework C**: 실험 완료 수, Phase 4 백로그 진행
- **Regulatory**: Must-REG 완료율, Q-Sub 피드백 반영
- **Security**: CVE 노출률, SBOM 최신성, SLSA 레벨
- **Interoperability**: Conformance 테스트 통과율
- **Operations**: MTBF, drift 알림 정확도, reject 원인 분석

---

## 8. 승인 체크포인트

- **v3.0.0 본 plan 승인**: 사용자 확인 필요
- **85 달성 리뷰**: 분기별
- **95 달성 리뷰**: 반기별
- **98+ 진입 결정**: 연간

---

**본 plan은 v2.0.0을 대체하지 않고 extend한다. 85까지의 경로는 v2.0.0 유지, 85 이후 경로는 본 plan이 normative.**
