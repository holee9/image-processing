# XPE 개발 계획 (Living Document)

**Document ID**: DEV-PLAN-001  
**Version**: 1.4.0  
**Date**: 2026-04-21  
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

## 1. 현재 상태 (2026-04-21 기준)

### 1.1 통합 현황 (main 기준)

| 모듈 | Lane | 테스트 | main 병합 | 품질 게이트 |
|------|------|--------|----------|------------|
| xpe_common.dll | Pre-A | 91/91 ✅ | ✅ | PASS |
| xpe_preprocess.dll | Pre-A | 202/202 ✅ | ✅ | PASS (M2 API 완료) |
| xpe_enhance_basic.dll | Post-B | 67/67 ✅ | ✅ | PASS |
| xpe_display.dll | Post-B | 48/48 ✅ | ✅ | PASS |
| xpe_dicom.dll | Post-B | 35/35 ✅ | ✅ | PASS |
| ImageProcTest.exe | GUI-C | 78/78 ✅ | ✅ | 진행중 (TASK-GUI-IA-001) |
| xpe_enhance_advanced.dll | Post-B | 65/65 ✅ | ✅ | PASS (전수 GREEN) |
| gsvg.dll | Post-B | 1/1 ✅ (BP-06) | ✅ | 착수 (CMakeLists + benchmark test) |
| xpe_ai.dll | Post-B | - | ❌ | 미착수 (Should) |

### 1.1.1 Lane 브랜치 현황 (2026-04-21 squash merge 완료)

| Lane | 브랜치 | 선행 커밋 | 최신 main 통합 내용 | 상태 |
|------|--------|:---------:|---------|------|
| Pre-A | dev/preprocess | 17 | runtime_detection 검증 보강 + TDD RED test_temp_nonlinearity_binning | main 병합 ✅ |
| Post-B | dev/postprocess | 25 | BP-06~09 baseline + gsvg CMakeLists/test + MSVC 경고 정리 | main 병합 ✅ |
| GUI-C | dev/gui | 13 | Phase1B readiness chain + DICOM/Display/GSVG P/Invoke 래퍼 | main 병합 ✅ |

### 1.2 점수 현황

| Framework | 문서화 | 실제 추정 | 목표 |
|-----------|:------:|:--------:|:----:|
| A (Process/Compliance) | 83 | **~83** | 85 |
| B (Product/Delivery) | ~76 | **~77** | 85 |

3-Lane squash merge 반영 — gsvg 착수 + BP-06~09 baseline 문서 + GUI Phase1B chain (2026-04-21 v1.4.0 기준).

---

## 2. main 워크트리 작업 범위

### 2.1 즉시 착수 가능 (main 고유 작업)

| 작업 | 종류 | 점수 기여 | 선행 조건 |
|------|------|:---------:|---------|
| **S-OPS-CORE** (SPEC-XPE-OPS 필수 부분) | 문서/프로세스 | +? | S1-B1 완료 ✅ |
| **S-IOP-CORE** (DICOM Conformance Statement) | 문서/검증 | +? | S1-B3 완료 ✅ |
| **S-SEC-CORE** (§524B, SBOM, Input Val) | 문서/CI | +? | S0-B 완료 ✅ |
| **IEC 62304 sync** (SRS/SDD/RTM/VVP) | 문서 | **+3** | 병렬 가능 |
| **점수 공식 재측정** (Framework A/B 갱신) | 평가 | 기준선 확립 | 즉시 |
| **api-spec.md 최신화** | 명세 | 아키텍처 점수 | 즉시 |
| **Lane 통합 준비** (next SPEC 정의) | SPEC 생성 | Lane 방향성 | 즉시 |

### 2.2 다음 Lane 방향 결정 (main이 정의)

main이 정의할 다음 Lane 작업 SPEC:

| SPEC | 담당 Lane | 점수 기여 | 현재 상태 |
|------|----------|:---------:|---------|
| SPEC-XPE-P2-ADV 실구현 명세 완성 | Post-B | +3 | 스켈레톤 존재 |
| SPEC-SIMD-001 (scalar ref + parity) | Pre-A | +5 | SPEC 미작성 |
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

### main 직접 기여 항목 (잔여, 점수 증가)

| 항목 | Framework A 기여 | 비고 |
|------|:----------------:|------|
| benchmark BP-01~05 Lane A 동결 | +2 | Pre-A: DegradedMode.* GTest 구현 필요 |
| benchmark BP-06~09 Lane B 동결 | +1 | Post-B: baseline 문서 완료 ✅, DegradedMode GTest 연동 남음 |
| IEC 62304 VVP sync | +1 | 문서 품질 19→20 |

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

### P0: benchmark 동결 ← 임계경로 (+3점)

현재 상태: SIMD 구현 완료. 기준 동결 미수행.

| 팩 | 담당 Lane | 내용 |
|----|----------|------|
| BP-01~05 | **Pre-A** | Preprocess 전용 — Offset/Gain/Defect/Ghost/Temp 측정값 동결 |
| BP-06~09 | **Post-B** | Post 전용 — GSVG/Collimation/EI/DI 측정값 동결 |
| BP-10 | **main** | Degraded-mode stress (cross-lane, 통합 후 측정) |

- main: benchmark regression CI 잡 추가 (양 Lane 결과 통합 검증)
- 기여: Framework A +3, Framework B (알고리즘 품질) +2

### P0: EARS P1B 요구사항 작성 (+2점)

S1-B1~B3 구현 완료됐으나 EARS 요구사항 문서 미작성.
- 대상 SPEC: SPEC-XPE-P1B-ENH, SPEC-XPE-P1B-DISP, SPEC-XPE-P1B-DICOM
- 내용: 각 모듈 EARS 요구사항 작성 → 요구사항 완전성 15→17점

### P1: IEC 62304 VVP sync (+1점)

P2-ADV IEC 패키지는 완성됐으나 VVP (Validation/Verification Plan) 미완성.
- 대상: docs/project/vvp_adv.md 작성
- P1A/P1B 모듈 VVP 항목도 추가

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
