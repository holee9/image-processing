# 레인 세션 시작 가이드

**Document ID**: LANE-SESSIONS-001
**기준**: `.moai/project/dev-plan.md` v2.0.0 §0 (워크트리 역할 분담 — 불변 원칙)
**복원일**: 2026-08-28
**복원 시점 baseline**: `main` @ 4b1d2ab

---

## 0. 토폴로지

```
[터미널 1] main    D:/workspace-github/image-processing   main
              │ 세션통신 (SendMessage)
              ├──[터미널 2] Lane A   D:/workspace-github/xpe-pre    dev/preprocess
              ├──[터미널 3] Lane B   D:/workspace-github/xpe-post   dev/postprocess
              └──[터미널 4] Lane C   D:/workspace-github/xpe-gui    dev/gui
```

세션 이름은 각 터미널에서 `/rename <이름>` 으로 지정한다. 이름이 곧 주소다.

| 터미널 | 세션 이름 | 워크트리 |
|---|---|---|
| 1 | `xpe-main` | image-processing |
| 2 | `xpe-pre`  | xpe-pre |
| 3 | `xpe-post` | xpe-post |
| 4 | `xpe-gui`  | xpe-gui |

---

## 1. 소유 경계 (침범 금지)

| 레인 | 소유 경로 | 금지 |
|---|---|---|
| main | 루트 `CMakeLists.txt`, `cmake/`, `.moai/`, `.claude/`, `docs/`, `CLAUDE.md` | `modules/**`, `clients/**`, `gui/**` 직접 수정 |
| Lane A | `modules/common/**`, `modules/preprocess/**` | 그 외 모듈 |
| Lane B | `modules/enhance_basic/**`, `modules/enhance_advanced/**`, `modules/ai/**`, `modules/display/**`, `modules/dicom/**`, `modules/gsvg/**` | `common`, `preprocess` |
| Lane C | `clients/**`, `gui/**` | `modules/**` |

`tests/` 는 해당 모듈 테스트 디렉터리를 소유 레인이 함께 가진다.
- Lane A: `tests/common*`, `tests/preprocess*`
- Lane B: `tests/enhance_advanced_tests`, `tests/ai_tests`, `tests/e2e_post_pipeline`
- Lane C: `clients/ImageProcTest.IntegrationTests`, `gui/ImageProcTest.E2E`

---

## 2. 모듈별 QA 게이트 (dev-plan §4.1 — 병합 전 필수)

각 레인이 자기 모듈에 대해 자체 수행한다.

- [ ] `dumpbin /dependents *.dll` → 횡단 의존성 없음
- [ ] Google Test 해당 모듈 100% GREEN
- [ ] 메모리 누수 테스트 1000 프레임 PASS
- [ ] static analysis 0 warning (`/WX` 빌드)
- [ ] P/Invoke ABI 심볼 수 문서와 일치
- [ ] CODEOWNERS 경계 준수 확인

**통합 E2E는 main에서만 실행** (dev-plan §4.2):
`preprocess → enhance_basic → display → dicom` 순차 + IntegrationTests 78개 + 3072×3072 < 3000ms (현재 실측 163ms).

---

## 3. 세션통신 규약

오가는 것은 세 종류뿐. 메시지는 **알림**이고, 판정 근거는 항상 **디스크의 증거 파일**이다.

### main → Lane (작업 지시 — 고정 필드, 산문 금지, 10줄 이내)
```
card: <작업 id>
spec: <SPEC-ID>
cmd: <실행 명령>
wt: D:/workspace-github/xpe-<lane>
evidence: <완료 증거를 남길 경로>
```

### Lane → main (완료 신고)
```
card: <작업 id>
result: PASS | FAIL | BLOCKED
tests: <통과/전체>
evidence: <증거 파일 경로>
branch: dev/<lane>  sha: <커밋>
```

### Lane ↔ Lane (ABI 변경 통보)
`common` 헤더나 export 심볼이 바뀌면 즉시 영향 레인에 통보. 예: Lane A → Lane B "common ABI 변경, 재빌드 필요".

**main의 판정 원칙**: 레인의 "완료했습니다"라는 말이 아니라 `evidence:` 경로의 파일을 읽고 판정한다.

---

## 4. 각 터미널 세션 시작 메시지 (붙여넣기용)

### 터미널 1 — main
```
ultrathink. XPE main 세션. 역할: 통합·거버넌스·SPEC·게이트 판정.
소유: 루트 CMakeLists.txt, cmake/, .moai/, .claude/, docs/. modules/**·clients/**·gui/** 직접 수정 금지.
기준 문서: .moai/project/dev-plan.md §0/§4, .moai/project/lane-sessions.md
먼저 /rename xpe-main 실행 후, ListAgents 로 레인 세션 3개 접속 확인.
첫 작업: SPEC status 드리프트 정정 (4월 이후 미갱신 SPEC 실제 상태 반영).
```

### 터미널 2 — Lane A (preprocess)
```
ultrathink. XPE Lane A 세션. 브랜치 dev/preprocess.
소유: modules/common/**, modules/preprocess/**, tests/common*, tests/preprocess*
그 외 경로 수정 금지. 완료 시 main(xpe-main)에 증거 경로 포함해 신고.
기준 문서: .moai/project/lane-sessions.md §1·§2·§3
먼저 /rename xpe-pre 실행. 첫 작업: 소유 모듈 QA 게이트 6항목 현재 상태 실측.
```

### 터미널 3 — Lane B (postprocess)
```
ultrathink. XPE Lane B 세션. 브랜치 dev/postprocess.
소유: modules/{enhance_basic,enhance_advanced,ai,display,dicom,gsvg}/**, tests/{enhance_advanced_tests,ai_tests,e2e_post_pipeline}
common/preprocess 수정 금지. 완료 시 main(xpe-main)에 증거 경로 포함해 신고.
기준 문서: .moai/project/lane-sessions.md §1·§2·§3
먼저 /rename xpe-post 실행. 첫 작업: 6개 모듈 QA 게이트를 서브에이전트로 병렬 실측.
```

### 터미널 4 — Lane C (gui)
```
ultrathink. XPE Lane C 세션. 브랜치 dev/gui.
소유: clients/**, gui/**
modules/** 수정 금지. 완료 시 main(xpe-main)에 증거 경로 포함해 신고.
기준 문서: .moai/project/lane-sessions.md §1·§2·§3
먼저 /rename xpe-gui 실행.
첫 작업: clients/ImageProcTest 와 gui/ImageProcTest 중복(App.xaml, MainWindow.xaml, csproj 등 5파일 동일) 정리 방안 조사 후 main에 보고. 임의 삭제 금지.
```

---

## 5. 알려진 이슈 (레인 시작 전 인지 필요)

| # | 이슈 | 담당 |
|---|---|---|
| 1 | `clients/ImageProcTest` 와 `gui/ImageProcTest` 중복 (App.xaml/MainWindow.xaml/csproj 동일) | Lane C 조사 → main 판정 |
| 2 | SPEC frontmatter `status` 4월 이후 미갱신 (created==updated) | main |
| 3 | 2026-04-22 이후 main에서 `modules/**` 직접 수정 32건 — dev-plan §2.3 위반분 정리 필요 | main |
| 4 | CODEOWNERS 에 `/gui/` 미등록 | main |
| 5 | vcpkg_installed 3.4GB — 워크트리별 중복 방지 설정 필요 (§6) | main |

---

## 6. 빌드 환경 공유 (워크트리 중복 회피)

각 레인 터미널에서 세션 시작 전 1회 설정:

```powershell
$env:VCPKG_INSTALLED_DIR = "D:/workspace-github/image-processing/vcpkg_installed"
$env:VCPKG_BINARY_SOURCES = "clear;files,D:/workspace-github/.vcpkg-cache,readwrite"
```

미설정 시 워크트리마다 3.4GB를 새로 설치한다.
**주의**: 위 설정은 미검증. 첫 레인 빌드에서 동작 확인 후 확정할 것.

---

## 7. 정리 (레인 종료 시)

```bash
git worktree remove ../xpe-pre
git branch -d dev/preprocess
```
(post/gui 동일)

---

## 8. QA · 리뷰 배치 (4계층)

**원칙: 만든 레인이 자기 작업을 합격시키지 않는다.** 기계가 판정할 수 있는 것만 자기 채점하고, 판단이 들어가는 것은 반드시 다른 주체가 본다.

```
L1 레인 자체 QA   (기계적 · 자기 모듈)      → 레인 세션
L2 CI 무심 판정   (깨끗한 환경 · 푸시 즉시)  → GitHub Actions
L3 교차 리뷰      (소비자가 생산자를 리뷰)   → 다른 레인 세션
L4 최종 판정      (통합 E2E · 게이트)        → main 세션
```

### L1 — 레인 자체 QA (기계적인 것만)

dev-plan §4.1 게이트 6항목. 전부 "통과/실패"가 기계적으로 갈리는 항목이라 자기 채점이 가능하다.
레인은 결과를 **증거 파일로 남기고**, 판정 주장은 하지 않는다.

증거 경로 규약: `.moai/reports/lane-<lane>/<작업id>/`
- `gate.md` — 6항목 체크 결과 + 실행한 명령 + 원문 출력
- `tests.log` — ctest 원문
- `abi.log` — `dumpbin /dependents` 원문

### L2 — CI 무심 판정 (이미 배선돼 있음)

`.github/workflows/ci.yml` 이 **레인을 인식**한다:

```yaml
on:
  push:
    branches: [ main, dev/preprocess, dev/postprocess, dev/gui ]
  pull_request:
    branches: [ main ]
```

`detect-lane` 잡이 브랜치명으로 레인을 판별하고, 잡별 `if` 조건으로 해당 레인 것만 돌린다.
레인이 브랜치를 push하면 **자기 개발 머신이 아닌 깨끗한 환경에서** 검증된다. 로컬 GREEN보다 이쪽이 상위 근거다.

| 잡 | 조건 | 적용 레인 |
|---|---|---|
| `common-build` | `lane != gui` | main · Pre-A · Post-B |
| `preprocess-tests` | `lane == preprocess` | **Pre-A만** |
| `coverage` | 수동 실행 | 필요 시 |

부가 워크플로: `codeql`(보안), `repository-guard`(구조·링크·텍스트 검증), `benchmark-regression`(BP-06~10), `windows-common-build`.

> ⚠️ **확인된 공백**: `postprocess-tests` · `gui-tests` 잡이 **없다.** Lane B(6개 모듈)와 Lane C는 현재 CI 레인 검증을 못 받는다. main의 선행 작업 항목이다.

### L3 — 교차 리뷰 (소비자가 생산자를 리뷰)

리뷰어는 **그 산출물을 실제로 쓰는 쪽**이 맡는다. 파이프라인 방향과 일치하고, "쓰는 입장에서 이게 맞나"라는 검증이 자연히 붙는다.

| 생산 | 산출물 | 리뷰어 | 리뷰 관점 |
|---|---|---|---|
| Lane A | `common` ABI, `preprocess` 출력 | **Lane B** | 헤더 계약, 출력 포맷이 후단에서 쓸 수 있는가 |
| Lane B | post 모듈 DLL, export 심볼 | **Lane C** | P/Invoke 시그니처, 에러 코드 매핑 |
| Lane C | GUI, IntegrationTests | **main** | 통합 E2E 관점, 사용자 흐름 |
| main | SPEC, 루트 CMake, 문서 | **해당 레인** | 실행 가능한 지시인가, 소유 경계 침범 없나 |

리뷰 요청·회신은 세션통신으로 (§3 규약). 리뷰어는 **증거 파일을 읽고** 의견을 낸다.

```
review-request: <작업id>   from: <생산 레인>   to: <리뷰 레인>
evidence: .moai/reports/lane-<lane>/<작업id>/
focus: <리뷰 관점 1줄>
```
```
review-result: <작업id>   verdict: PASS | CHANGES-REQUESTED
findings: <건수>   detail: .moai/reports/lane-<리뷰레인>/review-<작업id>.md
```

### L4 — main 최종 판정

main만 하는 일:
1. L1~L3 증거를 **읽고** 판정 (레인의 "완료했습니다"는 근거가 아니다)
2. 통합 E2E 실행 — dev-plan §4.2: `preprocess → enhance_basic → display → dicom` + IntegrationTests 78개 + 3072×3072 < 3000ms
3. 회의적 재검토가 필요하면 `sync-auditor` 서브에이전트로 독립 평가 (4차원 채점)
4. PASS면 squash merge, FAIL이면 레인에 반려

**PR 경유 권장**: `pull_request: branches: [main]` 이 배선돼 있으므로, 레인 → main 병합은 PR로 올리면 CI 전체가 자동으로 붙는다.

### IEC 62304 Class B 기록 의무

QA·리뷰 결과는 흔적을 남겨야 한다. 각 레인 병합 시 갱신 대상:
- `docs/project/` VVP (검증 계획·결과)
- RTM (요구사항 ↔ 테스트 추적)
- 리뷰 findings 및 처리 결과

증거 파일 경로가 곧 추적 근거이므로 §L1의 경로 규약을 지킬 것.

### QA 공백 요약 (main 선행 작업)

| # | 공백 | 영향 |
|---|---|---|
| 1 | `postprocess-tests` CI 잡 없음 | Lane B 6개 모듈 CI 검증 부재 |
| 2 | `gui-tests` CI 잡 없음 | Lane C CI 검증 부재 |
| 3 | `coverage` 잡이 수동 전용 | 커버리지 회귀 자동 감지 안 됨 |
| 4 | 브랜치 보호 규칙 미확인 | PR 없이 main 직접 push 가능 상태일 수 있음 |
