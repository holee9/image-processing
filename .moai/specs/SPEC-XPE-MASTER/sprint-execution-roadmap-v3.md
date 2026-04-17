# Sprint Execution Roadmap v3.0.0

**Document ID**: SPRINT-ROADMAP-001
**Version**: 3.0.0
**Date**: 2026-04-17
**Status**: Active (Supersedes v2.0.0)
**Relationship**: Integrates `sprint-execution-roadmap-v2.md` + SPEC-XPE-MASTER v3.0.0 addendum + 4 new SPECs (REG/SEC/IOP/OPS) + P3-AI v1.0
**Classification**: IEC 62304 Class B

---

## Changelog (v2.0.0 → v3.0.0 → v3.1 Strict)

| Change | Detail |
|--------|--------|
| Sprint count | 11 → **14** sprints |
| 3 cross-cutting sprints | S-REG, S-SEC, S-OPS (parallel tracks) added |
| S3 updated | Now SPEC-XPE-P3-AI v1.1 (redefined with SSL+Diffusion+XAI+UQ) |
| Dependency graph | Parallel tracks replace strict sequential flow for non-core sprints |
| Coverage thresholds | Extended to governance SWUs (90%) |
| Pre-sprint checklists | 4 new checklists for new sprints |
| Quality gates | Added Model Observer + Regulatory + Security + Reproducibility gates |
| **v3.1 Strict Must** | **S-REG/SEC/IOP/OPS 각각을 CORE(Must) + EXT(Should)로 분할. P0 Core만 즉시, Ext는 Phase 연결 조건부 진행.** |

---

## 1. Sprint Overview v3.0

### 1.1 Full Sprint Matrix (14 sprints)

| Sprint | SPEC | Phase | DLL/Module | SWU | API | EARS REQ | Priority | Track |
|--------|------|:-----:|-----------|:---:|:---:|:--------:|:--------:|:-----:|
| S0-A | SPEC-XPE-P0 | 0 | Build/CMake | -- | -- | 8 | Must | Core |
| S0-B | SPEC-XPE-P0 | 0 | xpe_common | **11** (was 7, +4 new SWU) | 22 (was 18) | 28 | Must | Core |
| S0-C | SPEC-XPE-P0 | 0 | ImageProcTest | 1 | N/A | 3 | Must | Core |
| **S-REG-CORE** | **SPEC-XPE-REG (Must part)** | **parallel** | docs (IEC 62304/EU MDR/21 CFR) | gov | 2 | **~15** | **Must** | **Regulatory-Core** |
| **S-REG-AI** | **SPEC-XPE-REG (Should part)** | **Phase 3 gating** | PCCP + Model Card + GMLP docs | gov | 2 | **~30** | Should (조건부 Must) | **Regulatory-AI** |
| **S-SEC-CORE** | **SPEC-XPE-SEC (Must part)** | **parallel** | §524B + SBOM + Vuln Mgmt + Input Val | gov | 2 | **~20** | **Must** | **Security-Core** |
| **S-SEC-EXT** | **SPEC-XPE-SEC (Should part)** | **parallel Phase 2** | SLSA L3 + IEC 81001-5-1 + Threat Model 확장 | gov | 1 | **~20** | Should | **Security-Ext** |
| S1-A | SPEC-XPE-P1A | 1a | xpe_preprocess | 9 | 18 | ~45 | Must | Core |
| S1-B1 | SPEC-XPE-P1B-ENH | 1b | xpe_enhance_basic | 5 | 7 | ~28 | Must | Core |
| S1-B2 | SPEC-XPE-P1B-DISP | 1b | xpe_display | 4 | 11 | ~22 | Must | Core |
| S1-B3 | SPEC-XPE-P1B-DICOM | 1b | xpe_dicom | 4 | 10 | ~22 | Must | Core |
| S1-B4 | SPEC-XPE-P1B-GUI | 1b | ImageProcTest | 1+1 | N/A | ~12 | Should | Core |
| **S-OPS-CORE** | **SPEC-XPE-OPS (Must part)** | **parallel** | PMS Plan + 보고 체계 | gov | 1 | **~8** | **Must** | **Ops-Core** |
| **S-OPS-EXT** | **SPEC-XPE-OPS (Should part)** | **parallel Phase 2** | OTEL + Drift + Reproducible + VEX | gov (2) | 4 | **~22** | Should | **Ops-Ext** |
| S2-A | SPEC-XPE-P2-ADV | 2 | xpe_enhance_advanced | 3 | 3 | ~18 | Must | Core |
| S2-B | SPEC-XPE-P2-GSVG | 2 | gsvg | 4 | 8 | ~22 | Must | Core |
| **S-IOP-CORE** | **SPEC-XPE-IOP (Must part)** | **1b 확장** | **xpe_dicom 강화 (Conformance Stmt)** | 0 | 0 | **~10** | **Must** | **Interop-Core** |
| **S-IOP-EXT** | **SPEC-XPE-IOP (Should part)** | **2-3** | **xpe_interop** (new DICOMweb/FHIR/IHE) | **3** | **~14** | **~20** | Should | **Interop-Ext** |
| S3 | **SPEC-XPE-P3-AI** | 3 | xpe_ai + worker | 4 | 15 (expanded) | **~40** (up from ~18) | Should (조건부 Must) | Core |

**Totals v3.1**:
- **Sprints (nominal)**: 14 sub-tracks (was 11 monolithic) — Must-only 진행 시 실제 P0 Sprint 수는 **9** (S0-A, S0-B, S0-C, S-REG-CORE, S-SEC-CORE, S1-A, S1-B1~B4, S-OPS-CORE, S-IOP-CORE)
- **Must EARS REQ**: ~73 (IEC 62304 15 + §524B 20 + PMS 8 + DICOM 10 + Char.Test/Trackability 20)
- **Should EARS REQ**: ~290 (조건부 진행)
- **Total EARS REQ**: ~365 (variable by Phase decision)

**실제 실행 단계**: Must 진행 (9 sprints) → 출시 가능 (M1 76점). 주요 Should 확장 (+ 추가 ~5 sub-tracks) → M1-Ext 85점 달성.

### 1.2 Parallel Track Structure (v3.1 Strict)

```
Time →

Track CORE:        S0-A ─┬─ S0-B ─┬─ S1-A ─┬─ S1-B1 ──┬─ S2-A ──┬─ (S3 조건부)
                          └ S0-C   │        ├─ S1-B2   ├─ S2-B    
                                   │        ├─ S1-B3 ──┼─ S-IOP-CORE (Must, DICOM Conformance)
                                   │        └─ S1-B4   │
Track REGULATORY-CORE: S-REG-CORE (Must: IEC 62304+EU MDR+21 CFR docs) ── 즉시 착수
Track SECURITY-CORE:   ── S-SEC-CORE (Must: §524B+SBOM+Vuln Mgmt+Input Val) ── S0-B 후
Track OPS-CORE:        ─────────────── S-OPS-CORE (Must: PMS Plan) ── S1-B1 후

(Phase 2 extension - Should)
Track REG-AI:          ──────── S-REG-AI (Should: PCCP/Transparency) ──── Phase 3 승인 시
Track SEC-EXT:         ──────── S-SEC-EXT (Should: SLSA L3+IEC 81001-5-1) ── Phase 2
Track OPS-EXT:         ──────── S-OPS-EXT (Should: OTEL/Drift/VEX) ─────── Phase 2
Track IOP-EXT:         ──────── S-IOP-EXT (Should: DICOMweb/FHIR/IHE) ──── Phase 2-3

Critical Path (Must-only, 출시 가능 상태): S0-A → S0-B → S1-A → S1-B1 → S1-B3 → [S-IOP-CORE]
Parallel Must: S-REG-CORE, S-SEC-CORE, S-OPS-CORE (blocker 해소용 문서·프로세스)
Phase 3 (조건부): S3 is Should tier; 시작하려면 Phase 3 AI-DSF 배포 공식 결정 필요
```

**v3.1 Key Change**: S3 (AI)는 Should. 즉, v2.0 목표 "S3 달성"이 출시 블로커가 아니며, Must-only 9 sprints 완료 시 법적으로 출시 가능 (Phase 1/2 결정적 전용 제품).

### 1.3 Sprint Start Gating (v3.1 Strict)

| Sprint | Priority | Start Gate |
|--------|:--------:|-----------|
| S0-A | Must | Project init |
| S0-B | Must | S0-A complete |
| S0-C | Must | S0-A complete |
| **S-REG-CORE** | **Must** | **S0-A complete + legal counsel engaged (Must)** |
| **S-SEC-CORE** | **Must** | **S0-B complete + security lead assigned (Must)** |
| S1-A | Must | S0-B complete + api-spec v1.3 |
| S1-B1-B4 | Must (B1-B3), Should (B4) | S1-A complete |
| **S-OPS-CORE** | **Must** | **S1-B1 complete + PMS plan approved** |
| **S-IOP-CORE** | **Must** | **S1-B3 complete (DICOM base) + Conformance Statement template** |
| S2-A | Must | S1-B1 complete |
| S2-B | Must | S1-A complete |
| **S-SEC-EXT** | Should | S-SEC-CORE complete + SLSA L3 decision |
| **S-OPS-EXT** | Should | S-OPS-CORE complete + OTEL endpoint decision |
| **S-IOP-EXT** | Should | S-IOP-CORE complete + DICOMweb/FHIR business decision |
| **S-REG-AI** | Should→Must | Phase 3 AI-DSF 배포 공식 결정 + FDA Q-Sub 계획 |
| S3 | Should→Must | Phase 3 진입 결정 + S-REG-AI + S-SEC-EXT + S-OPS-EXT complete |

---

## 2. Quality Gates per Sprint (v3.0 Harmonized)

### 2.1 Core Sprints

| Gate | S0-A | S0-B | S0-C | S1-A | S1-B* | S2-A/B | S3 |
|------|:----:|:----:|:----:|:----:|:-----:|:------:|:--:|
| Unit Coverage | ≥85 | **≥90** | ≥70 | **≥90** | ≥85 | ≥85 | ≥80 |
| Branch Coverage | ≥70 | **≥80** | ≥60 | **≥80** | ≥70 | ≥70 | ≥60 |
| Static Analysis | 0 warn | 0 | 0 | 0 | 0 | 0 | 0 |
| Memory Leak (1K frames) | Pass | Pass | Pass | Pass | Pass | Pass | Pass |
| Performance Budget | N/A | <100ms | N/A | <500ms | <3000ms | <2500ms | <3000ms |
| EARS Traceability | 8 | 28 | 3 | ~45 | ~84 | ~40 | ~40 |
| P/Invoke ABI Test | Pass | Pass | Pass | Pass | Pass | Pass | Pass |
| **NEW: Model Observer IQ** | -- | -- | -- | -- | -- | **Pass** | **Pass** |

### 2.2 Cross-Cutting Sprints

| Gate | S-REG | S-SEC | S-OPS | S-IOP |
|------|:-----:|:-----:|:-----:|:-----:|
| Document Coverage | 15 docs | 11 docs | 10 docs | 8 docs |
| **Regulatory Sign-off** | **Legal + RA** | -- | -- | -- |
| **Security Threat Model** | -- | **5 boundaries** | -- | -- |
| **PMS Workflow** | -- | -- | **Tabletop** | -- |
| **Conformance Test** | -- | -- | -- | **dcm4che+Orthanc+AWS HI** |
| SBOM Quality | -- | **SPDX+CDX** | -- | -- |
| SLSA Level | -- | **L2 minimum** | -- | -- |
| Reproducibility | -- | -- | **Dual-build match** | -- |

### 2.3 NEW Cross-Sprint Gates

**Model Observer Quality Gate** (cross-ref MASTER v3 §E):
- Enable for all SWUs producing diagnostic images: S1-B1, S1-B2, S2-A, S2-B, S3
- AUC (Channelized Hotelling Observer) — AI output ≥ baseline - 0.02
- Perceptual metrics (LPIPS, SSIM) reported alongside

**Regulatory Gate** (applies to each release):
- PCCP approved for each AI-DSF module
- Model Card signed off
- Data lineage audit complete

**Security Gate** (applies to each release):
- SBOM published (SPDX 3.0 + CycloneDX 1.6)
- SLSA L2 minimum (L3 from v2.0)
- Zero critical unresolved CVE
- Threat model reviewed within 3 months

**Reproducibility Gate** (applies to each release):
- Build reproducibility verified (dual-build bit-identical)
- Known exceptions documented

---

## 3. Pre-Sprint Checklists (Enhanced)

### 3.1 v2.0 retained (S0-B, S1-A, S1-B* checklists)

See `sprint-execution-roadmap-v2.md` §3.

### 3.2 v3.0 NEW Checklists

**Before S-REG**:
- [ ] Legal counsel engaged
- [ ] Regulatory Affairs lead assigned
- [ ] FDA pre-submission (Q-Sub) booking prepared
- [ ] Notified Body contact established (EU)
- [ ] Clinical SME identified for Model Card review
- [ ] Trend survey 2026 reviewed and approved

**Before S-SEC**:
- [ ] Security Lead assigned
- [ ] Threat modeling training completed (STRIDE + EMB3D)
- [ ] SLSA generator GitHub Actions template prepared
- [ ] SBOM tooling chosen (syft + cyclonedx-cpp-maker)
- [ ] SECURITY.md published
- [ ] VDP contact verified

**Before S-OPS**:
- [ ] PMS Plan drafted and approved
- [ ] OTEL Collector endpoint choice made (on-prem/cloud)
- [ ] Drift detector architecture agreed (on-device/off-device)
- [ ] Reject-analysis schema published
- [ ] AAPM TG-151 methodology review completed

**Before S-IOP**:
- [ ] dcm4che test setup verified
- [ ] Orthanc reference server configured
- [ ] AWS HealthImaging demo account (for DICOMweb testing)
- [ ] HAPI FHIR validator installed
- [ ] IHE Connectathon 2026 calendar checked, registration planned

**Before S3 (updated from v2.0)**:
- [ ] All S-REG Must deliverables approved
- [ ] SPEC-XPE-SEC §4.6 threat model for AI boundary complete
- [ ] Training data governance approved + de-identification verified
- [ ] ONNX Runtime 1.20+ environment validated on CPU/CUDA/TensorRT/DirectML
- [ ] Signed model loading verified
- [ ] Worker isolation process prototype validated
- [ ] PCCP template for each AI-DSF approved
- [ ] Reader study protocol drafted (for Should-tier AI validation)

---

## 4. Dependency Graph (v3.0)

```
                        ┌──── S-REG (docs + Layer 0 additions) ────────┐
                        │                                               │
                        │                                               ├──┐
                        │                                               │  │
S0-A ──┬─── S0-B ────┬──┘                                               │  │
       │             │                                                  │  │
       │             ├──── S-SEC (CI/CD + secure Layer 0) ──────────────┤  │
       │             │                                                  │  │
       │             └─── S1-A ──┬── S1-B1 ──┬── S-OPS (Layer 0 + CI)───┤  │
       │                         │           │                          │  │
       │                         ├── S1-B2   │                          │  │
       └── S0-C                  ├── S1-B3 ──┼── S-IOP (Phase 2-3) ─────┤  │
                                 └── S1-B4   │                          │  │
                                             │                          │  │
                                             ├── S2-A ──────────────────┤  │
                                             │                          │  │
                                             └── S2-B (parallel S2-A)   │  │
                                                                        │  │
                                                              ┌─ S3 ◀───┴──┘
                                                              │
                                                              ▼
                                                        (Release Gate)
```

---

## 5. Sprint Effort Estimates (Priority-Based, Not Time-Based)

### 5.1 Effort Priority Matrix

| Sprint | Effort | Risk | Priority | Can Start |
|--------|:------:|:----:|:--------:|:---------:|
| S0-A | Medium | Low | P0 | Immediately |
| S0-B | High | Low | P0 | After S0-A |
| S0-C | Low | Low | P0 | After S0-A |
| S-REG | Medium-High | Medium (legal complexity) | P0 | After S0-A (in parallel) |
| S-SEC | Medium | Low | P0 | After S0-B (in parallel) |
| S1-A | High | Medium (9 SWUs) | P0 | After S0-B |
| S1-B1 | Medium | Low | P0 | After S1-A |
| S1-B2 | Low | Low | P0 | After S1-A |
| S1-B3 | Medium | Medium (DICOM compat) | P0 | After S1-A |
| S1-B4 | Low | Low | P1 | After S1-B1 |
| S-OPS | Medium | Low | P1 | After S1-B1 |
| S2-A | Medium-High | Medium (CLAHE + MFP) | P1 | After S1-B1 |
| S2-B | High | High (GSVG IEC package) | P1 | After S1-A |
| S-IOP | Medium | Medium (DICOMweb new) | P1 (Must baseline) | After S1-B3 |
| S3 | High | High (AI + reg + sec) | P2 | After S2-A+S-REG+S-SEC+S-OPS |

### 5.2 Phasing Recommendations

**Wave 1** (Parallel, Immediate): S0-A, S-REG (docs)
**Wave 2** (After S0-A): S0-B, S0-C, S-REG continues
**Wave 3** (After S0-B): S-SEC starts, S1-A starts
**Wave 4** (After S1-A): S1-B1, S1-B2, S1-B3 parallel; S-REG/SEC continue
**Wave 5** (After S1-B1): S-OPS, S1-B4, S2-A
**Wave 6** (After S2-A): S-IOP, S2-B
**Wave 7** (After all above + REG/SEC/OPS complete): S3

---

## 6. Release Cadence

### 6.1 Minor Release (v0.x) — per wave

- v0.1: S0-* complete → Phase 0 baseline
- v0.2: S1-A complete → Preprocess available
- v0.3: S1-B* complete → Full Phase 1 (diagnostic-capable deterministic)
- v0.4: S2-* complete → Phase 2 premium deterministic
- v0.5: S-REG + S-SEC + S-OPS artifacts complete → regulatory-ready
- v0.6: S-IOP complete → interoperable

### 6.2 Major Release (v1.x)

- **v1.0**: All Must items complete (target score A=85, B=85)
  - Requires: S0, S1, S2 + S-REG + S-SEC + S-OPS + S-IOP (baseline)
  - Regulatory: first FDA submission
  
- **v2.0**: Must + Should items (target score A=95, B=95)
  - Adds: S3 Phase 3 AI + S-IOP extended (DICOMweb + IHE AIR/AIRA)
  - Regulatory: AI-DSF modules submitted

- **v3.0** (2027+): Must + Should + selected Could (target A=98, B=98, C=60)

---

## 7. Regulatory Calendar (Informational)

| Date | Event | Action |
|------|-------|--------|
| 2025-04-07 | FDA Lifecycle Draft 의견 종료 | Final 발행 대기 |
| 2025-06-16 | UK MHRA PMS Regs 발효 | S-OPS PMS 설계 영향 |
| 2025-06-27 | FDA Cybersecurity Final | S-SEC 정합성 확인 |
| 2026-02-02 | US QMSR 발효 | S-REG QMSR gap analysis 완료 |
| 2026-05-28 | EU EUDAMED 전면 가동 시작 + 6개월 전환 | Q2 2026 준비 |
| 2027-08-01 | **EU AI Act 의료기기 AI 전면 의무** | **S-REG 완료 필수** |

---

## 8. Resources/Roles Needed

| Role | Responsibility | Sprint |
|------|---------------|--------|
| Technical Lead | 아키텍처, 전체 조정 | 전체 |
| Regulatory Affairs Lead | FDA/EU submissions | S-REG, S3 |
| Security Lead | §524B, threat modeling | S-SEC |
| Clinical SME | Model card review, reader study | S-REG, S3 |
| Legal Counsel | 규제 해석 | S-REG |
| DevOps/SRE | CI/CD, SLSA | S0-A, S-SEC, S-OPS |
| Data Governance | Lineage, bias | S-REG, S3 |
| ML Engineer | AI training, benchmarks | S3 |
| DICOM/FHIR Engineer | Interop implementation | S1-B3, S-IOP |

---

## 9. Change Control

- v3.0.0 approval: User 확인 필요
- v3.x minor: 기존 sprint scope 내 세부 변경
- v4.0.0: Phase 4 Research Track 편입

---

**본 roadmap은 v2.0.0을 대체한다. v2.0.0의 기존 11 sprints는 그대로 유지되며, 3개의 cross-cutting parallel sprints가 추가되어 14 sprints로 확장된다.**
