# XPE 개발 계획 (Living Document)

**Document ID**: DEV-PLAN-001  
**Version**: 1.9.0  
**Date**: 2026-04-22  
**Status**: Active — 지속적 갱신 대상

---

## 0. 워크트리 역할 분담 (불변 원칙)

```
main (이 문서의 주체)
├── 역할: 통합 관리, 아키텍처 거버넌스, 문서, SPEC 관리
├── 소유: CMakeLists.txt(루트), cmake/, CLAUDE.md, .moai/, .claude/, docs/
└── 금지: modules/**, clients/** 직접 구현 → 각 Lane 담당

xpe-pre/ (Lane A, dev/preprocess)
├── 담당: modules/common/**, modules/preprocess/**
└── Claude 세션 독립 실행

xpe-post/ (Lane B, dev/postprocess)
├── 담당: modules/enhance_**/**, modules/ai/**, modules/display/**, modules/dicom/**, modules/gsvg/**
└── Claude 세션 독립 실행

xpe-gui/ (Lane C, dev/gui)
├── 담당: clients/**
└── Codex 주도, Claude 검토
```

**main의 핵심 가치**: Lane들이 만든 결과물을 품질 기준에 맞게 종합·통합하고,
다음 Lane 작업의 방향(SPEC)을 결정하는 것.

---

## 1. 현재 상태 (2026-04-22 기준)

### 1.1 통합 현황 (main 기준)

| 모듈 | Lane | 테스트 | main 병합 | 품질 게이트 |
|------|------|--------|----------|------------|
| xpe_common.dll | Pre-A | 91/91 ✅ | ✅ | PASS |
| xpe_preprocess.dll | Pre-A | 202/202 ✅ | ✅ | PASS (M2 API + BP-01~05 freeze) |
| xpe_enhance_basic.dll | Post-B | 67/67 ✅ | ✅ | PASS |
| xpe_display.dll | Post-B | 48/48 ✅ | ✅ | PASS |
| xpe_dicom.dll | Post-B | 35/35 ✅ | ✅ | PASS |
| ImageProcTest.exe | GUI-C | 78/78 ✅ | ✅ | Phase1b E2E fixture 완료 |
| xpe_enhance_advanced.dll | Post-B | 65/65 ✅ | ✅ | PASS (전수 GREEN) |
| gsvg.dll | Post-B | 2/2 ✅ (BP-06 + DegradedMode) | ✅ | BP-06~09 freeze 완료 |
| xpe_ai.dll | Post-B | - | ❌ | 미착수 (Should) |

### 1.1.1 Lane 브랜치 현황 (2026-04-22 기준)

| Lane | 브랜치 | 선행 커밋 | 최신 내용 | 상태 |
|------|--------|:---------:|---------|------|
| Pre-A | dev/preprocess | 0 | main 동기화 완료 | ✅ 병합 완료 |
| Post-B | dev/postprocess | 0 | main 동기화 완료 | ✅ 병합 완료 |
| GUI-C | dev/gui | 0 | main 동기화 완료 | ✅ 병합 완료 |

### 1.2 점수 현황

| Framework | 이전 | 현재 실측 | 목표 |
|-----------|:----:|:--------:|:----:|
| A (Process/Compliance) | ~86 | **~85** | 85 ✅ |
| B (Product/Delivery) | ~78 | **~75** | 85 |

- 점수 재측정 (2026-04-22 v1.9.0): 항목별 실측 기반 재계산, AI governance 미반영으로 B 하향 조정
- SPEC-BENCH-PRE/POST v1.0.0 작성 완료 — BP-01~09 freeze SPEC 정식화
- SPEC-XPE-GSVG v1.0.0 작성 완료 — GSVG 26개 요구사항 정식화
- api-spec.md v1.4.0 갱신 — AED 제거, SPEC-MASTER v3.0 참조
- Post-B 재통합 (37파일) — GSVG API + BP freeze + E2E + MSVC 수정
- OPS/IOP/SEC 리뷰: DICOM Conf/PMS = Minor, SECURITY/SPDF = Major 업데이트 필요

---

## 2. main 워크트리 작업 범위

### 2.1 즉시 착수 가능 (main 고유 작업)

| 작업 | 종류 | 점수 기여 | 선행 조건 |
|------|------|:---------:|---------|
| **S-OPS-CORE** (SPEC-XPE-OPS 필수 부분) | 문서/프로세스 | +? | ✅ pms-plan.md v0.1 작성 완료 (2026-04-22) |
| **S-IOP-CORE** (DICOM Conformance Statement) | 문서/검증 | +? | ✅ dicom-conformance-statement.md v0.1 작성 완료 (2026-04-22) |
| **S-SEC-CORE** (§524B, SBOM, Input Val) | 문서/CI | +? | ✅ SECURITY.md + spdf-plan.md v0.1 작성 완료 (2026-04-22) |
| **IEC 62304 sync** (SRS/SDD/RTM/VVP) | 문서 | **+3** | 병렬 가능 |
| **점수 공식 재측정** (Framework A/B 갱신) | 평가 | 기준선 확립 | 즉시 |
| **api-spec.md 최신화** | 명세 | 아키텍처 점수 | 즉시 |
| **Lane 통합 준비** (next SPEC 정의) | SPEC 생성 | Lane 방향성 | 즉시 |

### 2.2 다음 Lane 방향 결정 (main이 정의)

main이 정의할 다음 Lane 작업 SPEC:

| SPEC | 담당 Lane | 점수 기여 | 현재 상태 |
|------|----------|:---------:|---------|
| SPEC-XPE-P2-ADV 실구현 명세 완성 | Post-B | +3 | 스켈레톤 존재 |
| SPEC-SIMD-001 (scalar ref + parity) | Pre-A | +5 | ✅ SPEC v1.0.0 작성 완료 (2026-04-22) |
| SPEC-BENCH-PRE (BP-01~05 preprocess freeze) | Pre-A | +2 | SPEC 미작성 |
| SPEC-BENCH-POST (BP-06~09 post-processing freeze) | Post-B | +1 | SPEC 미작성 |
| BP-10 (degraded-mode stress) | main | +? | cross-lane, 통합 후 측정 |
| S2-B: SPEC-XPE-GSVG 정의 | Post-B | +? | 미작성 |

### 2.3 main 작업 금지 항목

- modules/** 코드 직접 수정 → Pre/Post Lane
- clients/** 코드 직접 수정 → GUI Lane
- 알고리즘 구현 → Lane에서 수행

---

## 3. 85점 달성 main 기여 경로

```
현재 81점 (2026-04-19 v2.3.0)
         ↓ +4점 필요
[main] benchmark freeze + EARS P1B~P2 + IEC VVP sync  → +4~6점 → 85~87점
```

### 완료된 항목 (점수 반영됨)

| 항목 | Framework A 기여 | 완료일 |
|------|:----------------:|--------|
| M2 SIMD (AVX2/FMA) 구현 + parity | +2 (구현진행도) | 2026-04-19 |
| P2-ADV 65/65 + IEC 62304 4종 | +5 (요구사항+문서+구현+품질) | 2026-04-21 |
| Golden Reference 26개 + CI | +1 (품질보증) | 2026-04-19 |
| **SPEC-XPE-P1B-DICOM Released** | +2 (요구사항 완전성 →46개) | 2026-04-21 |
| **BP-10 CI 워크플로우 + 테스트 드라이버** | +2 (benchmark regression 준비) | 2026-04-21 |
| **Post-B + GUI-C → main 통합** | 품질게이트 확인 후 통합 | 2026-04-21 |
| **3-Lane 전체 squash merge (Pre-A + Post-B + GUI-C)** | 런타임 검증 보강, gsvg 착수, Phase1B GUI chain | 2026-04-21 |

### 완료 추가 항목 (2026-04-22 v1.8.0)

| 항목 | Framework 기여 | 완료일 |
|------|:--------------:|--------|
| **3-Lane squash merge (Pre-A+Post-B+GUI-C)** | A +3, B +1 | 2026-04-22 |
| benchmark BP-01~05 Lane A 동결 (6/6 PASS) | A +2 포함 | 2026-04-22 |
| benchmark BP-06~09 Lane B 동결 (4/4 PASS) | A +1 포함 | 2026-04-22 |
| Phase1b E2E fixture + NativePresentationExport | B 보강 | 2026-04-22 |

### main 직접 기여 항목 (잔여, 점수 증가)

| 항목 | Framework 기여 | 비고 |
|------|:--------------:|------|
| **SPEC-SIMD-001 구현** (scalar ref + parity) | **A +5** | ⚠️ 임계 경로 — CMakeLists TODO만 등록, 실구현 미착수 |
| IEC 62304 VVP sync (P1A/P1B 항목 보강) | A +1 | 문서 품질 19→20 |
| Gate G1b → G2 검증 | 게이트 통과 | 빌드 환경 필요, E2E 실측 |

---

## 4. 품질 게이트 관리 (main 책임)

### 4.1 Lane 병합 전 체크리스트

모든 Lane PR이 main 병합 전 통과해야 할 게이트:

- [ ] `dumpbin /dependents *.dll` → 횡단 의존성 없음
- [ ] Google Test 해당 모듈 100% GREEN
- [ ] 메모리 누수 테스트 1000 프레임 PASS
- [ ] static analysis 0 warning (`/WX` 빌드)
- [ ] P/Invoke ABI 심볼 수 문서와 일치
- [ ] CODEOWNERS 경계 준수 확인

### 4.2 통합 테스트 (main에서 실행)

main 병합 후 전체 파이프라인 E2E 검증:
- `xpe_preprocess → xpe_enhance_basic → xpe_display → xpe_dicom` 순차 파이프라인
- ImageProcTest.IntegrationTests 78개 전체 통과
- 성능 예산: Phase 1 < 3000ms (3072×3072)

---

## 5. 다음 main 작업 계획 (우선순위 순)

### ✅ P0: 3-Lane 전체 병합 ← 완료 (2026-04-22)

GUI-C → Post-B → Pre-A 순서로 squash merge 완료.
충돌 4개 해소 (benchmark-regression.yml, BP-06-09-baseline.md, gsvg/CMakeLists.txt, Test-DegradedMode.ps1).

### ✅ P0: benchmark 동결 ← 완료 (2026-04-22)

| 팩 | 담당 Lane | 상태 |
|----|----------|------|
| BP-01~05 | **Pre-A** | ✅ DegradedMode 6/6 PASS, manifest v1.1.0 Frozen |
| BP-06~09 | **Post-B** | ✅ BenchmarkFreeze 4/4 PASS, manifest Frozen |
| BP-10 | **main** | CI 워크플로우 완료 (benchmark-regression.yml) |

- 기여: Framework A +3, Framework B (알고리즘 품질) +2

### ✅ P0: EARS P1B 요구사항 작성 (+2점) — 완료 확인 (2026-04-22)

SPEC 문서 검토 결과 요구사항이 이미 완전히 작성된 상태임을 확인.
- SPEC-XPE-P1B-ENH: 30 EARS 요구사항 ✅ (Status: Implemented)
- SPEC-XPE-P1B-DISP: 35 EARS 요구사항 ✅ (Status: Completed)
- SPEC-XPE-P1B-DICOM: 40 EARS 요구사항 ✅ (Released 2026-04-21)
→ 요구사항 완전성 점수 이미 반영됨

### P1: IEC 62304 VVP sync (+1점)

P2-ADV VVP 완성됨 (vvp_adv.md 작성 완료 2026-04-21).
VVP-PREPROCESS-001 v1.1.0 작성 완료 (SPEC-SIMD-001, BP-01~05 freeze 반영).
**P1B VVP addendum 누락 확인** — ENH/DISP/DICOM 모듈 VVP 보강 필요.

### P1: S-OPS/IOP/SEC v0.1 → v1.0 승인

리뷰 결과 (2026-04-22):
- DICOM Conformance Statement v0.1 → **Minor** (검증 절차, 유지보수 섹션 추가)
- PMS Plan v0.1 → **Minor** (AI 모니터링, 필드 배포 시스템 추가)
- SECURITY.md v0.1 → **Major** (사건 대응 절차, 보안 테스트 계획 필요)
- SPDF Plan v0.1 → **Major** (위협 모델 완성, SLSA L2/L3 구현 필요)

### P1: Gate G1b → G2 준비

Phase 1b 파이프라인 성능 검증 (게이트 블로커):
- Full Phase 1 pipeline < 3000ms for 3072×3072
- Phase 1 peak memory <= 190MB
- Integration test: Raw DICOM → Pre → Post → EI → Display → DICOM Write

**검증 계획 (빌드 환경 필요):**
1. `xpe_preprocess_tests.exe` + `xpe_enhance_basic_tests.exe` + `xpe_display_tests.exe` + `xpe_dicom_tests.exe` 전체 PASS 확인
2. ImageProcTest.IntegrationTests 78개 PASS 확인
3. 성능 측정: 3072×3072 Raw DICOM 입력 → 전체 파이프라인 E2E 타이밍 기록
4. 메모리 프로파일: 1000프레임 연속 처리 피크 메모리 190MB 이하 확인
5. GSVG 통합: GSVG 모듈 R2(ABI smoke test) 달성 후 G2 게이트 통과 가능

**현재 상태**: GSVG API 구현 완료(v0.2.0), DegradedMode GTest 추가됨. 빌드 후 실측 필요.

---

## 6. 변경 이력

| 날짜 | 버전 | 내용 |
|------|------|------|
| 2026-04-19 | 1.0.0 | 초기 생성 — Phase 1b 완료 확인, 85점 임계 경로 분석 |
| 2026-04-19 | 1.1.0 | main 역할 재정의 — 구현은 Lane, main은 통합/거버넌스/문서 |
| 2026-04-19 | 1.2.0 | 점수 갱신 — M2 SIMD + P2-ADV + Golden Ref CI 반영 (73→81); xpe_enhance_advanced.dll 완료; 다음 작업 benchmark 최우선으로 재편 |
| 2026-04-20 | 1.3.0 | benchmark Lane 할당 오류 수정 — BP-06~09(GSVG/Collimation/EI)는 Post-B 담당, Pre-A는 BP-01~05(Preprocess)만; BP-10은 main cross-lane |
| 2026-04-21 | 1.4.0 | 3-Lane 전체 squash merge 반영 — runtime_detection 보강, gsvg 착수, BP-06~09 baseline 완료, GUI Phase1B chain; 점수 갱신 A=83, B=~77 |
| 2026-04-21 | 1.5.0 | 비-AI 잔여 작업 일괄 처리 — VVP 문서 작성, EARS P1B-ENH/DISP 검증(완료 확인), GSVG API 구현(v0.2.0), DegradedMode GTest BP-01~09, Gate G1b→G2 검증 계획 수립 |
| 2026-04-22 | 1.6.0 | 7개 검토사항 순차 완료 — xpe-gui 커밋(+7파일), build_test2 정리, BP-01~05 DegradedMode freeze(6/6 PASS), SPEC-SIMD-001 v1.0.0 작성(+5점 임계경로 문서화), BP-06~09 freeze(4/4 PASS), EARS P1B 완료 확인, Lane 브랜치 현황 갱신 |
| 2026-04-22 | 1.7.0 | S-OPS/IOP/SEC CORE 문서 착수 — DICOM Conformance Statement v0.1 (PS 3.2), SECURITY.md (CVD), SPDF Plan v0.1 (§524B), PMS Plan v0.1 (EU MDR Art 83) |
| 2026-04-22 | 1.8.0 | 3-Lane 전체 squash merge 완료 — GUI-C/Post-B/Pre-A 병합 (충돌 4개 해소); 점수 갱신 A ~83→~86, B ~77→~78; 잔여 임계경로: SPEC-SIMD-001 구현 (+5) |
| 2026-04-22 | 1.9.0 | SPEC-BENCH-PRE/POST + SPEC-XPE-GSVG v1.0.0 작성; api-spec.md v1.4.0 갱신; Post-B 재통합 37파일; 점수 실측 재계산 A ~85, B ~75; OPS/IOP/SEC 리뷰 완료; P1B VVP 누락 확인 |
