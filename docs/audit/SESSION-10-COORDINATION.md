# Session 10 Coordination Plan — main Worktree

**작성일**: 2026-05-08
**워크트리**: `D:/workspace-github/image-processing` (main 오케스트레이터)
**기준 감사**: 세션 9차 README (2026-04-28), `SPEC-XPE-P1A-QUALITY-REPORT.md`, 워크트리별 WIP 분석

---

## 1. 세션 10 분배 결과

세션 9차 감사로 식별된 잔여 작업을 4개 워크트리에 분배 완료.

### 1.1 워크트리별 작업 매핑

| 워크트리 | Brief | 핵심 이슈 | 본 세션 범위 |
|---|---|---|---|
| **xpe-pre** (`feat/preprocessing`) | `D:/workspace-github/image-processing-preprocess/docs/audit/SESSION-10-BRIEF.md` | #57, #68, #69, #70, #73 | WIP 정리 + Must P0 4건 + S1 SIMD-001 |
| **xpe-post** (`feat/postprocessing`) | `D:/workspace-github/image-processing-postprocess/docs/audit/SESSION-10-BRIEF.md` | #71 (진행 보고) | WIP 정리만 (S2/S4 차후 세션) |
| **xpe-gui** (`feature/evaluation-workbench`) | `D:/workspace-github/image-processing-gui/docs/audit/SESSION-10-BRIEF.md` | #74 (신규) | 3커밋 squash merge → main |
| **main** | 본 문서 | #60 (M1) | 미커밋 13건 정리 + Gate G1b→G2 성능 실측 |

### 1.2 신규 등록 GitHub Issue

세션 10에서 신규 등록한 이슈:

| # | 제목 | 워크트리 |
|---|---|---|
| **#73** | [Pre-A] P1A 심각 결함 4건 + REQ-P1A-066 오류 경로 테스트 | xpe-pre |
| **#74** | [GUI-C] feature/evaluation-workbench 3커밋 main squash merge | xpe-gui |

세션 10에서 재오픈한 이슈:

| # | 제목 | 사유 |
|---|---|---|
| **#68** | [Pre-A] Offset 이중 구현 제거 + SIMD dispatch 활성화 | xpe-pre WIP 미머지, 작업 진행 중 |
| **#69** | [Pre-B] Calibration multi-method + Cache 스레드 안전성 | 같은 사유 |
| **#70** | [Pre-C] Defect bilinear 보간 검증 + Hampel 검출 + Reflect padding | 같은 사유 |

---

## 2. main 워크트리 직접 작업

### 2.1 미커밋 13건 정리

```
M  .claude/agents/moai/builder-agent.md
M  .claude/agents/moai/builder-plugin.md
... (manager-* / expert-* 다수)
M  .claude/hooks/moai/handle-*.sh (다수)
M  .claude/output-styles/moai/moai.md
M  .gitignore
M  .moai/config/sections/*.yaml (8개)
M  gui/ImageProcTest.E2E/Program.cs
M  gui/ImageProcTest/Views/AnalysisPanel.xaml
M  gui/ImageProcTest/Views/TopBar.xaml
```

조치:
1. `.claude/` + `.moai/config/` 변경은 MoAI 툴체인 동기화 (별도 커밋: `chore(moai): sync agent/hook/config templates`)
2. `gui/` 변경은 xpe-gui 워크트리 PR (#74)과 충돌 가능성 — `git diff main..feature/evaluation-workbench -- gui/` 비교 후 폐기 또는 통합 결정

### 2.2 #60 — Gate G1b → G2 성능 실측 (M1, 출시 블로커)

**범위**: 빌드 환경에서 Phase 1b 파이프라인 통합 성능 검증.

수용 기준:
- [ ] BUILD_DICOM=ON 활성화 후 전체 빌드 통과
- [ ] E2E 파이프라인 < 3000ms (3072×3072 단일 이미지)
- [ ] 메모리 사용량 ≤ 190MB peak
- [ ] BP-01~05 벤치마크 결과 `benchmark/` 디렉토리에 영구 보관
- [ ] 실측 결과를 `.moai/specs/SPEC-XPE-MASTER/` 또는 README "Gate 현황"에 갱신
- [ ] 통과 시 G1b → G2 전환 선언

**전제 조건**: xpe-pre #68 SIMD dispatch 활성화 후 진행 (성능 미측정 위험 회피).

---

## 3. 의존성 그래프

```
xpe-pre WIP commit 정리
   ↓
xpe-pre #68 (Offset SIMD)  ←──┐
xpe-pre #69 (Calibration)    │  병렬 진행 가능
xpe-pre #70 (Defect)          │  (파일 충돌 없음 — 다른 src 파일들)
xpe-pre #73 (P1A defects)  ←──┘
   ↓
xpe-pre #57 (SIMD-001 검증)
   ↓
main #60 (Gate G1b→G2 성능 실측)
   ↓
G2 통과 → Phase 2 본격 진입

병렬 진행 (의존성 없음):
xpe-post WIP 정리 + #71 코멘트
xpe-gui #74 (3커밋 머지)
main 미커밋 .claude/.moai 정리
```

---

## 4. 진행 모니터링

### 4.1 일일 상태 확인 (오케스트레이터 운영)

매일 다음 확인:

```powershell
# 워크트리별 진행 상황
$gh = "C:\Program Files\GitHub CLI\gh.exe"
& $gh -R holee9/image-processing issue list --state open --label "lane:pre" --json number,title,assignees,updatedAt
& $gh -R holee9/image-processing issue list --state open --label "lane:main" --json number,title,assignees,updatedAt

# 워크트리별 커밋 진척
git -C D:/workspace-github/image-processing-preprocess log --oneline main..HEAD
git -C D:/workspace-github/image-processing-postprocess log --oneline main..HEAD
git -C D:/workspace-github/image-processing-gui log --oneline main..HEAD
```

### 4.2 세션 10 종결 조건

본 세션 종결 (사용자 보고) 기준:

- [ ] xpe-pre WIP 13건 정리 완료
- [ ] xpe-pre #68/#69/#70/#73 PR 머지 완료
- [ ] xpe-pre #57 SIMD-001 검증 (또는 차후 세션 이관 사유 명시)
- [ ] xpe-post WIP 5건 정리 완료
- [ ] xpe-gui #74 squash merge 완료, 워크트리 정리
- [ ] main 미커밋 13건 정리 완료
- [ ] main #60 Gate G1b→G2 성능 실측 + 결과 갱신
- [ ] 본 문서에 "세션 10 완료 보고" 섹션 추가

### 4.3 차후 세션 (세션 11+) 이관 항목

- xpe-post S2 — `xpe_ai.dll` 구현 (Phase 3)
- xpe-post S4 — DICOMweb 상호운용성 (WADO-RS / STOW-RS)
- xpe-post #61 — SPEC-XPE-GSVG v0.2.0 → v1.0.0 종결 검증
- main #56 — BP-10 Degraded-mode Cross-lane 통합 검증
- main #58/59 — EARS P1B / IEC 62304 VVP 동기화
- main #38/39/40 — Cross-cutting OPS / IOP / SEC 핵심 문서

---

## 5. 점수 영향도 (Framework A/B v3.2)

본 세션 완료 시 점수 변화 추정:

| 항목 | 현재 | 세션 10 후 | 메모 |
|---|:-:|:-:|---|
| Framework A 합계 | ~90 | ~93~95 | 구현 진행도 +2 (P1A 결함 4건 종결), 품질 +2 (#68/#69/#70 종결) |
| Framework B 합계 | ~80 | ~83~85 | 알고리즘 +2 (SIMD-001), 운영 준비도 +1 (Gate G2 통과) |

**목표**: Framework A 95점, Framework B 85점 도달.

---

## 6. 참조

- 세션 9차 감사: `D:/workspace-github/image-processing/README.md` "프로젝트 완성도 현황 (2026-04-28)"
- 전처리 정밀 감사: `D:/workspace-github/image-processing-preprocess/docs/calibration/PREPROCESS-AUDIT-SESSION9.md`
- 품질 보고서: `D:/workspace-github/image-processing-preprocess/SPEC-XPE-P1A-QUALITY-REPORT.md`
- SPEC 마스터: `.moai/specs/SPEC-XPE-MASTER/spec.md`

---

## 7. 진행 기록

```
2026-05-08 main: 세션 10 분배 완료
  - Brief 4건 작성: xpe-pre, xpe-post, xpe-gui, main
  - 이슈 #68/#69/#70 재오픈, #73/#74 신규 생성
  - 의존성 그래프 확정
```

(공란)
