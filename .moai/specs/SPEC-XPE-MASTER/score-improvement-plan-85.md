# XPE Project Score Improvement Plan: 61 → 66 → 85

**Document ID**: SCORE-PLAN-001
**Version**: 2.1.0
**Date**: 2026-04-17
**Status**: Active
**Sources**:
- `docs/project/XPE-Implementation-Analysis-Report.md` (v1.3.0) — primary score model
- `docs/project/XPE-Brainstorming-DeepSync-Execution.md` (v1.1.0) — uplift brainstorming
- `docs/project/XPE-Module-Reinforcement-Plan.md` (v1.3.0) — reinforcement roadmap
- Cross-Validation v5.0 (8-round), SPEC-XPE-MASTER v2.0, Sprint Roadmap v2.0

---

## 1. Two-Score Framework (Reconciled)

이 프로젝트에는 두 개의 독립적인 점수 프레임워크가 공존한다.
두 프레임워크는 관점이 달라 수치가 다르지만 상호보완적이다.

### 1.1 Framework A — Process/Compliance Score (Cross-Validation 기반)

IEC 62304, EARS 추적성, 문서 완전성, 교차검증 이슈 해소를 중심으로 평가.

| 영역 | 배점 | 현재 (2026-04-19) | 비고 |
|------|:----:|:-----------------:|------|
| 요구사항 완전성 (EARS) | 25 | **13** | P1A EARS 완성, P1B~P3 미작성 |
| 문서 품질 (IEC 62304) | 20 | **18** | AED 제거·docs sync 완료, api-spec v1.3.0 출판 |
| 아키텍처 설계 | 20 | **17** | 파이프라인 물리 정확성 확인, Binning-Gain 미문서화 |
| 구현 진행도 | 20 | **13** | Phase 1a 전처리 9스테이지 + pipeline 통합 완료 |
| 품질 보증 | 15 | **13** | P1A 89/90 + GUI-IT 78/78 + common 91/91 GREEN |
| **합계** | **100** | **70** | 2026-04-19 기준 |

**변경 이력 (v2.0.0 → v2.1.0)**:
- 구현 진행도 6→8: xpe_common.cpp 버그 5건 수정, /WX 빌드 통과, 전체 7개 DLL 스텁 빌드 성공
- 품질 보증 8→10: Google Test vcpkg 구성, unit test 86/86 통과 (기존 스모크 전용에서 unit 포함으로 확장)

**변경 이력 (v2.1.0 → v2.2.0)** (2026-04-19):
- 구현 진행도 8→13: Phase 1a 전처리 9스테이지 완료 (Offset/Gain/Defect/Ghost Tier1~3/Temp/Nonlin/Binning/Readout), xpe_preprocess_pipeline 통합
- 품질 보증 10→13: P1A 89/90 + GUI-IT 78/78 + xpe_common 강화 (91/91)
- 문서 품질 17→18: AED 완전 제거 (CRITICAL 정책 준수), docs sync, api-spec v1.3.0

### 1.2 Framework B — Product/Delivery Score (Codex 분석 기반)

기능 범위, 성능 증거, 알고리즘 품질, 규제 준비도, 운영 준비도로 평가.
**1차 구현 완료** = Phase 0 + Phase 1a + Phase 1b deterministic baseline 가정.

| 영역 | 배점 | Phase 1b 완료 시 예측 | 비고 |
|------|:----:|:--------------------:|------|
| 기능 범위 (Functional Scope) | 35 | **26** | baseline 구현, Phase 2/3 미완 |
| 성능·메모리 (Performance) | 15 | **12** | Phase 1 예산 달성 가능, 미증명 |
| 알고리즘 품질·증거 (Quality Evidence) | 20 | **11** | baseline 강점, benchmark 미동결 |
| 규제·문서 준비도 (Regulatory) | 15 | **9** | docs/project 강점, IEC sync 미완 |
| 운영 준비도 (Operational) | 15 | **8** | reject-analysis/DI drift 미완 |
| **합계** | **100** | **66** | Phase 1b 완료 시 예측 |

### 1.3 점수 관계 정리

```
현재 상태 (Framework A): 61점
     ↓  Phase 0 완성 + Phase 1a/1b 구현
Phase 1b 완료 (Framework B): 66점   ← Codex 기준 첫 번째 의미 있는 마일스톤
     ↓  benchmark freeze + IEC sync + Phase 2 deterministic
목표 (양 Framework 공통): 85점
```

**핵심 통찰**: 두 프레임워크 모두 85점을 공통 목표로 지목하며,
"AI를 먼저 넣지 말고 deterministic 기반을 완성하라"는 결론이 일치한다.

---

## 2. 브레인스토밍 결과: 66 → 85 최단 경로

`XPE-Brainstorming-DeepSync-Execution.md §4` 및
`XPE-Module-Reinforcement-Plan.md §6.2` 종합.

### 2.1 채택 우선순위 (Codex 분석)

| 순서 | 행동 | 예상 점수 기여 | 이유 |
|:----:|------|:--------------:|------|
| 1 | `xpe_preprocess` scalar 레퍼런스 + SIMD parity harness | **+5** | 검출기 도메인 최대 품질 리스크 안전하게 해소 |
| 2 | benchmark 매니페스트 동결 + 자동 재현 harness | **+3** | 품질 주장을 재현 가능한 증거로 전환 |
| 3 | deterministic Phase 1b 완전 납품 (측정값 포함) | **+4** | 아키텍처를 실제 제품으로 전환 |
| 4 | IEC 패키지 sync (baseline 범위: SRS, SDD, RTM, VVP) | **+3** | 최대 릴리즈 준비 부채 제거 |
| 5 | baseline collimation + ROI-aware EI 재호출 | **+3** | AI boundary 확장 없이 프리미엄 가치 추가 |
| 6 | reject-analysis + DI drift telemetry end-to-end | **+2** | 운영 성숙도 및 현장 품질 점수 향상 |
| 7 | 선택적 deterministic premium 정제 | **+2** | 기준선 불안정 없이 품질 상한 상승 |
| **합계** | | **+22** | **66 → 88 가능 (최소 부분집합: 1~6 = +20 → 85+)** |

### 2.2 제외된 아이디어 (왜 비효율적인가)

| 아이디어 | 기여 | 이유 |
|---------|:----:|------|
| 조기 AI 투입 (benchmark freeze 前) | 0 또는 음수 | 신뢰성 없는 완성도 신호 생성 |
| 병리 인식 enhancement | 음수 | 규제 경계 위험 과도 |
| GSVG Phase 1 안정화 前 투입 | +1 이하 | 타이밍 리스크 > 이득 |
| 최적화 커널 우선 (레퍼런스 없이) | 음수 | 레퍼런스 없는 최적화 → 침묵 회귀 |

### 2.3 채택 결정 매트릭스 (Brainstorming Decision Matrix)

| 아이디어 | 품질 upside | 구현 가능성 | 증거 강도 | 경계 리스크 | 결정 |
|---------|:-----------:|:----------:|:--------:|:---------:|:----:|
| 캘리브레이션 매니페스트 체인 + 세션 잠금 | 높음 | 높음 | 높음 | 낮음 | **채택** |
| exposure-stratified 비선형성/게인 모델 | 높음 | 중간 | 높음 | 낮음 | **채택** |
| 기하학-인식 heel 보정 | 중상 | 중간 | 중상 | 낮음 | **채택** |
| class-aware 결함 라우팅 + FixPix-lite | 높음 | 중간 | 중상 | 낮~중 | **benchmark gate 포함 채택** |
| lag 잔차 기반 deterministic tiering | 높음 | 중간 | 높음 | 낮음 | **채택** |
| anatomy-bounded virtual-grid 프리셋 | 중상 | 중간 | 중상 | 중간 | **observer gate 포함 채택** |
| ROI-aware EI 재호출 (sidecar ROI) | 중간 | 높음 | 높음 | 낮음 | **채택** |
| stage-wise quality state vector | 중상 | 높음 | 중간 | 낮음 | **채택** |
| scalar-reference + SIMD parity harness | 높음 | 높음 | 높음 | 낮음 | **채택** |
| worker-isolated assistive AI | 중상 | 중간 | 높음 | 중간 | **gated 아키텍처로 채택** |
| 완전 병리-인식 enhancement | 불확실 | 낮음 | 낮~중 | 높음 | **미채택** |
| ALARA 어드바이저 | 불확실 | 낮음 | 중간 | 매우 높음 | **보류** |
| 대형 모델 E2E AI (deterministic 대체) | 불확실 | 낮음 | 낮음 | 매우 높음 | **거부** |

---

## 3. 아키텍처 원칙 (브레인스토밍 도출)

`XPE-Brainstorming-DeepSync-Execution.md §6` 기반.

### 3.1 레퍼런스 우선 구현 (Reference-First)

모든 주요 스테이지는 반드시:
1. scalar reference 구현 1개
2. 최적화 구현 1개 (AVX2/SIMD)
3. parity test harness 1개
4. benchmark 패밀리 바인딩 1개

> AVX2, 멀티스레드, AI path가 유일한 구현이 되어서는 안 된다.

### 3.2 Sidecar 계약 (메타데이터 변이 금지)

다음 출력은 `XpeImageMetadata` 변이가 아닌 sidecar로 전달:
- collimation ROI
- EI refinement ROI
- quality state vector
- AI 신뢰도 + 모델 ID
- GSVG diagnostic reason

### 3.3 Benchmark-First 승격 규칙

기능은 다음 조건이 모두 충족될 때만 "구현 완료"로 인정:
1. 코드 경로 존재
2. benchmark pack 존재
3. 평가 규칙 존재
4. degraded mode 존재
5. release boundary 명시

### 3.4 Deterministic Router 우선

```
1. deterministic baseline (항상 가용)
2. deterministic premium (opt-in, 선행조건 충족 시)
3. assistive AI (isolated, degradable)
4. 명시적 fallback + 증거
```

---

## 4. 실행 경로: 현재(61) → Phase 1b(66) → 목표(85)

### 4.1 구간 1: 현재 → 66점 (Phase 0 완성 + Phase 1a/1b 구현)

이 구간은 Sprint Roadmap v2.1의 S0-A/B/C + S1-A + S1-B1/B2/B3에 해당.

#### 전제: 블로커 해소 (즉시)

#### Phase 0 완성

| Task | 결과물 | Framework A | Framework B |
|------|--------|:-----------:|:-----------:|
| S0-A: Google Test + CTest 통합 | CI 자동화 가동 | QA +1 | Operational +1 |
| S0-A: 8모듈 스캐폴딩 | 디렉토리 구조 | 구현 +1 | Functional +0.5 |
| S0-B: xpe_common 15/15 완성 | DLL + 45 tests | 구현 +3, QA +2 | Functional +1 |
| S0-C: WPF skeleton + P/Invoke | ImageProcTest.exe | 구현 +1 | Functional +0.5 |

#### Phase 1a: xpe_preprocess (SPEC-XPE-P1A 선행 필수)

| Task | 결과물 | 점수 기여 |
|------|--------|----------|
| Offset + Gain + Defect 3 SWU | scalar 레퍼런스 구현 | Framework B +3 |
| Ghost Tier 1 (LTI) | lag correction | Framework B +2 |
| CalibrationManager + parity harness | 세션 무결성 검증 | 품질 +2 |
| 9 SWU 전체 + 90% 커버리지 | S1-A 완료 | +5 total |

#### Phase 1b: enhance_basic + display + dicom

| Task | 결과물 | 점수 기여 |
|------|--------|----------|
| Log + Noise + Contrast + Edge + EI | `xpe_enhance_basic.dll` | Functional +3 |
| Modality/VOI/Presentation LUT | `xpe_display.dll` | Functional +2 |
| DICOM R/W + Network | `xpe_dicom.dll` | Functional +2 |
| end-to-end 파이프라인 테스트 | 전체 경로 검증 | 품질 +2 |

**예측 도달점**: **66/100** (Framework B 기준)

### 4.2 구간 2: 66점 → 85점 (Codex 분석 6-step 경로)

| Step | 행동 | 예상 기여 | 누적 |
|:----:|------|:---------:|:----:|
| 현재 (Phase 1b 완료) | — | — | 66 |
| 1 | xpe_preprocess scalar ref + SIMD parity | +5 | 71 |
| 2 | Benchmark pack BP-01~BP-10 동결 + 자동 재현 | +3 | 74 |
| 3 | Phase 1b 측정값 검증 (budgets 달성 증명) | +4 | 78 |
| 4 | IEC sync (SRS, SDD, RTM, VVP baseline) | +3 | 81 |
| 5 | Baseline collimation + ROI-aware EI 재호출 | +3 | 84 |
| 6 | Reject-analysis + DI drift telemetry | +2 | **86** |

> **Step 1~6 = +20점, 66 → 86 (목표 85 달성)**
> Step 7 (deterministic premium 정제) +2점은 85 이후 보너스.

---

## 5. 점수 예측 타임라인

| 마일스톤 | Framework A | Framework B | 달성 조건 |
|----------|:-----------:|:-----------:|---------|
| 현재 | **61** | ~50 | Baseline |
| Phase 0 완료 (S0) | **68** | ~52 | xpe_common + CI + C# skeleton |
| Phase 1a 완료 (S1-A) | **72** | ~56 | xpe_preprocess 9 SWU |
| Phase 1b 완료 (S1-B) | **76** | **66** | 전체 deterministic pipeline |
| Benchmark freeze + SIMD parity | **78** | **74** | BP-01~10 동결, parity harness |
| IEC sync 완료 | **81** | **78** | SRS/SDD/RTM/VVP 갱신 |
| **85 달성** | **85** | **85** | collimation+ROI EI + reject-analysis |

---

## 6. No-Regret 스캐폴딩 (즉시 착수 가능)

두 프레임워크 공통으로 추천하는 "후회 없는" 선행 작업:

1. **benchmark manifest schema + hash-lock 툴링** — 모든 품질 증거의 기반
2. **scalar 레퍼런스 커널** (preprocess 각 스테이지) — correctness 증명 기반
3. **timing + parity harness** (scalar vs SIMD) — 최적화 안전망
4. **sidecar 구조체** (ROI, quality state, diagnostics) — ABI 안정성
5. **calibration manifest/session 검증 로직** — 기준선 안정성

---

## 7. 85점 성공 기준 (공식 체크리스트)

**Framework A 기준** (Process):
- [ ] EARS 총 요구사항 >= 120개 (P0 33 + P1A 45 + COMMON-CROSS 30 + P1B 일부)
- [ ] api-spec.md v1.2.0 + SDD v1.1 출판 완료
- [ ] OPEN CRITICAL 이슈 0건
- [ ] xpe_common.dll 15/15 함수 + >= 85% 커버리지
- [ ] Google Test + CI/CD 자동화 가동
- [ ] 8/8 모듈 디렉토리 존재

**Framework B 기준** (Product — `XPE-IMPL-ANALYSIS-001 §4.2`):
- [ ] deterministic Phase 1 경로 구현 및 예산 대비 측정값 확인
- [ ] benchmark pack BP-01~BP-10 manifest 수준 동결
- [ ] 주요 detector stage용 scalar reference + parity harness 존재
- [ ] DICOM + GSDF 적합성 증거 존재
- [ ] baseline scope 규제 패키지 sync 부채 해소
- [ ] Phase 2 deterministic premium 경로 1개 이상 구현 (verify degrade-open 동작)

---

## 8. Anti-Pattern 경고

`XPE-Brainstorming-DeepSync-Execution.md §8` 발췌:

- display-side 개선으로 detector-side 회귀를 숨기지 말 것
- scalar-reference + SIMD parity harness 없이 premium AI를 투입하지 말 것
- anatomy-specific 검토 없이 virtual-grid 강도를 전역으로 튜닝하지 말 것
- EI/DI를 제품 언어에서 선량 대리 지표로 기술하지 말 것
- raw research 캡처를 normative 소스로 격상시키지 말 것
- benchmark 동결 없이 optimized 커널만 구현하지 말 것 (parity 없는 최적화 = 침묵 회귀)

---

## 9. 관련 문서

| 문서 | 역할 |
|------|------|
| `docs/project/XPE-Implementation-Analysis-Report.md` | Framework B 점수 모델 원본, 66/100 기준 정의 |
| `docs/project/XPE-Brainstorming-DeepSync-Execution.md` | 브레인스토밍 결정 매트릭스, 66→85 uplift 번들 |
| `docs/project/XPE-Module-Reinforcement-Plan.md` | Score-lift roadmap to 85 (§6.2), 강화 우선순위 |
| `.moai/specs/SPEC-XPE-MASTER/sprint-execution-roadmap-v2.md` | Sprint 단위 실행 계획 (Section 11: 점수 추적) |
| `.moai/specs/SPEC-XPE-MASTER/cross-validation-report-v5.md` | Framework A 근거, Appendix A |

---

## Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0.0 | 2026-04-15 | MoAI | 초기 작성. Cross-Validation 8라운드 기반 61→85 계획 |
| **2.0.0** | **2026-04-15** | **MoAI** | **Codex 분석 3문서 통합. 이중 점수 프레임워크 정립. 66/100 Phase 1b 기준 추가. 브레인스토밍 결정 매트릭스 + Anti-Pattern 반영.** |
