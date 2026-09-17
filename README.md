# image-processing

X-ray Flat Panel Detector (FPD) 이미지 처리 연구, 실행 계획 및 구현 부트스트랩 저장소입니다.

X-ray 이미지 처리 엔진(`XPE`)의 네이티브 모듈과 C# 호스트를 구현하고, 그 근거가 되는 제품 계획·규제 문서·배포 자동화를 같은 저장소에서 함께 관리합니다. 제품 계획, 규제 문서, 네이티브 모듈 인터페이스, GitHub 배포 자동화를 처음부터 동기화된 상태로 유지하는 것이 목표입니다.

## 현재 상태를 어디서 보는가

**이 절에는 수치가 없습니다. 의도한 것입니다.**

2026-09-17 까지 이 자리에는 `A ~93 / B ~85 (2026-05-09 실측)`, `모듈 구현 현황
(2026-05-09)`, `3-Lane Worktree 현황 — 최종 통합 완료 (2026-04-23)` 같은 표가 있었습니다.
그 표들은 **적힌 날에는 맞았고, 그 뒤로 넉 달 동안 아무도 갱신하지 않았습니다.** 그사이
세 레인이 224장의 카드를 돌렸고 "최종 통합 완료" 는 계속 최종이 아니었습니다. 같은 자리에
`tests/common_smoke` 를 **현재 CI 테스트 대상**이라고 적고 있었는데, 그 디렉터리는 이미
삭제된 뒤였습니다.

**측정값을 산문에 적으면 적는 순간 낡습니다.** 이 저장소는 같은 형태로 세 번 데었습니다 —
위의 README 표, `modules/preprocess/docs/test_suite_report.md` 의 `Total Tests: 54`(실제 643),
그리고 존재한 적 없는 하네스를 센 `405/405`. 그래서 이 절은 **수치 대신 측정하는 자리**를
가리킵니다.

| 알고 싶은 것 | 어디서 |
|---|---|
| 테스트 통과 수 | `ctest --test-dir build/<preset>` — 매 실행이 현재 값입니다 |
| CI 상태 (빌드·테스트·GUI E2E·문서) | GitHub Actions — `XPE CI Pipeline`, `Documentation Generation`, `Delivery Bundle` |
| 성능 | `XPE Benchmark Regression` 워크플로. 게이트 값은 `.github/workflows/` 와 SPEC 에 있습니다 |
| 미결 작업·결함 | GitHub Issues. 열린 이슈가 곧 잔여 작업 목록입니다 |
| 요구사항과 수용 기준 | `.moai/specs/SPEC-*/` — `spec.md` · `plan.md` · `acceptance.md` |
| 모듈별 설계 근거 | `docs/project/`, `docs/calibration/` |

**수치를 문서에 적어야 할 때는 측정일과 명령을 함께 적습니다.** 값만 있는 숫자는
확인할 수 없고, 확인할 수 없는 숫자는 시간이 지나면 주장이 됩니다.

---

## 세션 구성

개발은 워크트리를 나눈 4개 세션으로 진행합니다. 소유 경계와 기록 규약은
`.moai/project/lane-sessions.md` 에 있습니다 — 특히 §1(소유 경계)과 §3.5(기록·이력 규약).

| 세션 | 워크트리 | 소유 |
|---|---|---|
| `xpe-leader` | `image-processing` | 루트 `CMakeLists.txt`, `cmake/`, `.moai/`, `.claude/`, `docs/`, `tools/`, `.github/workflows/` |
| `xpe-pre` | `xpe-pre` | `modules/common`, `modules/preprocess` |
| `xpe-post` | `xpe-post` | `modules/{enhance_basic,enhance_advanced,ai,display,dicom,gsvg}` |
| `xpe-gui` | `xpe-gui` | `clients/`, `gui/` |

각 레인의 측정 보고서는 `.moai/reports/lane-*/` 에 있으며 **커밋되지 않습니다**
(`.gitignore`). 재측정에 필요한 명령·수치·경로는 해당 이슈 코멘트에 남깁니다.

---

## 범위 (Scope)

- 원본 감지기 도메인 입력부터 DICOM 배포까지 `XPE`의 실행 기준을 정의합니다.
- `PRD`, 백로그, 아키텍처, IEC 62304 패키지 문서를 정렬된 상태로 유지합니다.
- `C/C++` 모듈과 `C#` 호스트/오케스트레이터 계층 주변에 안정적인 네이티브 핵심을 구축합니다.
- 구현 규모 증가 전에 GitHub Actions를 통해 품질 게이트를 적용합니다.

---

## 문서 체계 (Documentation System)

전체 문서 인덱스는 **[docs/README.md](docs/README.md)** 를 참조합니다.

본 프로젝트는 **Hybrid 3-Tier + IEC 62304** 문서 체계를 사용합니다:

| 계층 | 역할 | 설명 |
|------|------|------|
| **Normative** | 단일 정보 출처 (SSoT) | 제품 정의, 기술 사양, 파이프라인, API 계약 — 충돌 시 이 문서가 우선 |
| **Informational** | 맥락 및 지침 | 분석 보고서, 보강 계획, 운영 가이드 — Normative를 참조하되 무시하지 않음 |
| **Archive** | 이력 보관 | 대체된 문서 — 감사 추적 전용 |

IEC 62304 규제 패키지는 소프트웨어 항목별(XPE, GSVG, Ghost Correction)로 구성됩니다.

### 문서 현황

| 카테고리 | 문서 수 | 위치 |
|----------|:------:|------|
| Normative 사양 | 13 | [docs/project/](docs/project/) |
| Informational | 5 | [docs/project/](docs/project/), [docs/development/](docs/development/) |
| XPE IEC 62304 패키지 (시스템 레벨) | 22 | [docs/post-processing/xpe/](docs/post-processing/xpe/) |
| GSVG IEC 62304 패키지 | 13 | [docs/post-processing/gsvg/](docs/post-processing/gsvg/) |
| Ghost Correction IEC 62304 | 9 | [docs/ghost-correction/](docs/ghost-correction/) |
| Calibration IEC 62304 패키지 | 8 | [docs/calibration/](docs/calibration/) |
| Panel Defect IEC 62304 패키지 | 9 | [docs/panel-defect/](docs/panel-defect/) |
| Enhance Basic IEC 62304 패키지 | 9 | [docs/enhance-basic/](docs/enhance-basic/) |
| Enhance Advanced IEC 62304 패키지 | 8 | [docs/enhance-advanced/](docs/enhance-advanced/) |
| AI Module IEC 62304 패키지 | 6 | [docs/ai-module/](docs/ai-module/) |
| Display IEC 62304 패키지 | 6 | [docs/display/](docs/display/) |
| DICOM I/O IEC 62304 패키지 | 6 | [docs/dicom/](docs/dicom/) |
| Common Infrastructure IEC 62304 패키지 | 6 | [docs/common/](docs/common/) |
| **보안 문서** ✨ NEW | **2** | [docs/security/](docs/security/), [SECURITY.md](SECURITY.md) |
| **상호운용성 문서** ✨ NEW | **1** | [docs/interop/](docs/interop/) |
| **운영·PMS 문서** ✨ NEW | **1** | [docs/operations/](docs/operations/) |
| **GUI Design 문서** ✨ NEW | **12** | [docs/design/](docs/design/) — Algorithm Evaluation Workbench 프로토타입, 구현 가이드, 기존 GUI 참조 |
| Research (연구/전처리 알고리즘) | 11 | [docs/quality-eval/](docs/quality-eval/), [docs/references/](docs/references/), [docs/panel-defect-algorithm/](docs/panel-defect-algorithm/) |
| Archive | 4 | [docs/archive/](docs/archive/) |
| **합계** | **151** | [docs/README.md v3.6.0](docs/README.md) |

---

## 주요 문서 (Key Documents)

### Normative 사양

| 문서 | 설명 |
|------|------|
| [SPEC-XPE-MASTER](docs/project/SPEC-XPE-MASTER.md) | 마스터 구현 계획 — 43 SWU 인벤토리, Phase 0-3 분해 (v3.0.0, 14 Sprint) |
| [pipeline-spec](docs/project/pipeline-spec.md) | 17단계 정규 파이프라인 및 의존성 그래프 (v1.3.0) |
| [api-spec](docs/project/api-spec.md) | 79개 내보낸 C ABI 함수 계약, 명시적 경로 API 패턴 (v1.4.0 — AED 제거, SPEC-MASTER v3.0) |
| [xpe-algorithm-spec-deepsync](docs/project/xpe-algorithm-spec-deepsync.md) | 규범적 알고리즘 계약, DeepSync 검증 + 사이드카 계약 지침 (v3.2.0-ds4) |
| [XPE-ALG-001](docs/post-processing/xpe/XPE-ALG-001_Unified_Algorithm_Development_Specification.md) | **통합 알고리즘 개발 명세 v1.8** — IEC 62304 §5.4 Detailed Design. 9 라운드 90회 완료, 전체 GAP 완전 해소 (GAP-01~CF). 수식·Python 보정 코드·C++ AVX2 런타임·검증 기준 포함. v1.5~v1.8 신규: 40개 GAP(GAP-AS~CF) 추가 해소 |
| [product](docs/project/product.md) | XPE-PRODUCT-001 v1.2.0 — Phase별 배포 경계, 필수/선택 바이너리 범위, AI 샌드박스 격리 원칙 |
| [structure](docs/project/structure.md) | XPE-STRUCTURE-001 v1.4.0 — 정규 모듈-바이너리 매핑 (38 SWU + 4 GSVG SI), SWU 소유 규칙, `clients/` · `gui/` 두 C# 앱의 역할 구분 |
| [tech](docs/project/tech.md) | 기술 스택 — C++17/C#, SOUP 의존성, ABI 설계 |
| [sprint-plan](docs/project/sprint-plan.md) | 28개 Sprint 분해 및 실행 일정 (v1.4.0) |
| [xpe-milestone-uat-plan](docs/project/xpe-milestone-uat-plan.md) | AI Agent HITL 방법론, Gantt 차트, WBS, M1-M5 Human UAT 시나리오 (v1.0.0) |
| [xpe-implementation-reference](docs/project/xpe-implementation-reference.md) | 개발자 참조 — 로깅/Alert JSON, LUT 형식, GSDF, IPC, 양자화, session_id (v1.1.0) |
| [Algorithm-Benchmark-Pack-Spec](docs/project/Algorithm-Benchmark-Pack-Spec.md) | 13개 벤치마크 패밀리 정의 — BP-01~BP-13, 매니페스트 필드, freeze/promotion 규칙 (v1.3.0) |
| [Algorithm-Evaluation-Protocol](docs/project/Algorithm-Evaluation-Protocol.md) | 알고리즘 개정 비교·승격·보류 기준, 물리 메트릭(MTF/NPS/DQE) 평가 프로토콜, SDT d'/ROC/JAFROC 프레임워크 (v1.3.0) |
| [Regulatory-Feature-Boundary-Matrix](docs/project/Regulatory-Feature-Boundary-Matrix.md) | 릴리스 경계 매트릭스 — release-safe / research-gated / regulatory-hold 분류 (v1.1.0) |

### Informational

| 문서 | 설명 |
|------|------|
| [cross-verification-consolidated](docs/project/cross-verification-consolidated.md) | 교차검증 통합 등록부 v2.0 — 8개 항목 완료, Open 4개 잔존 이슈 추적 |
| [XPE-Module-Reinforcement-Plan](docs/project/XPE-Module-Reinforcement-Plan.md) | Pre/Post 모듈 정밀 보강 계획, Score-lift roadmap to 85 (§6.2), No-regret 가속자 (v1.3.0) |
| [XPE-Implementation-Analysis-Report](docs/project/XPE-Implementation-Analysis-Report.md) | 구현 현황 분석 — 66/100 배점 분해, 85점 달성 조건, 스캐폴딩 우선순위 (v1.3.0) |
| [XPE-Brainstorming-DeepSync-Execution](docs/project/XPE-Brainstorming-DeepSync-Execution.md) | 브레인스토밍 결정 매트릭스 — 14개 아이디어 채택/보류/거부, 66→85 uplift 번들 (v1.1.0) |
| [XPE-CI-CD_LocalBuild_Runbook](docs/development/XPE-CI-CD_LocalBuild_Runbook.md) | CI/CD 파이프라인 및 로컬 빌드 실행 안내 |

### IEC 62304 규제 패키지

각 소프트웨어 항목별 전체 생명주기 문서 (SDP, SRS, SAD, SDD, STP, VVP, RTM, SHA, SOUP 등):

| 소프트웨어 항목 | 문서 수 | IEC 62304 Coverage | 패키지 인덱스 |
|----------------|:------:|:------------------:|--------------|
| **XPE** (시스템 레벨) | 22 | Complete (전체 패키지) | [xpe-iec62304-class-b-package](docs/post-processing/xpe/xpe-iec62304-class-b-package.md) |
| **GSVG** (Grid Suppression Virtual Grid) | 13 | Complete + IAP/TDS | [GSVG_IEC62304_ClassB_Document_Package](docs/post-processing/gsvg/GSVG_IEC62304_ClassB_Document_Package.md) |
| **Ghost Correction** (Lag/Ghost 보정) | 9 | Complete + IAP/TDS/README | [README](docs/ghost-correction/README.md) |
| **Calibration** (전처리 보정 모듈) | 9 | Complete + IAP/TDS + **FUNC-031~033 (모드 선택·최적화·품질 메타데이터, SRS v1.2, RTM v1.3)** | [docs/calibration/](docs/calibration/) · [검증 가이드](docs/calibration/ALGORITHM-VERIFICATION-GUIDE.md) |
| **Panel Defect** (패널 불량 보정) | 9 | Complete (PRD+SRS+SAD+SHA+RTM+IAP+TDS+README+INDEX) | [docs/panel-defect/](docs/panel-defect/) |
| **Enhance Basic** (기본 향상 모듈) | 9 | Complete (PRD+SRS+SAD+SHA+RTM+IAP+TDS+README+MANIFEST) | [docs/enhance-basic/](docs/enhance-basic/) |
| **Enhance Advanced** (고급 향상 모듈) | 8 | Complete (PRD+SRS+SAD+SHA+RTM+IAP+TDS+README) | [docs/enhance-advanced/](docs/enhance-advanced/) |
| **AI Module** (AI 추론 모듈) | 6 | Complete (PRD+SRS+SAD+SHA+RTM+README) | [docs/ai-module/](docs/ai-module/) |
| **Display** (GSDF/LUT 표시 처리) | 6 | Complete (PRD+SRS+SAD+SHA+RTM+README) | [docs/display/](docs/display/) |
| **DICOM I/O** (DCMTK 기반 DICOM 입출력) | 6 | Complete (PRD+SRS+SAD+SHA+RTM+README) | [docs/dicom/](docs/dicom/) |
| **Common Infrastructure** (Layer 0 공통 ABI) | 6 | Complete (PRD+SRS+SAD+SHA+RTM+README) | [docs/common/](docs/common/) |

### 보안·상호운용성·운영 (Security / Interop / Operations)

2026-04-22~23 거버넌스 세션 2~5차에서 착수·리뷰·승인한 CORE 문서입니다.

| 문서 | SPEC REQ | 설명 | 리뷰 상태 |
|------|---------|------|:---------:|
| [SECURITY.md](SECURITY.md) | SPEC-XPE-SEC REQ-SEC-090 | 취약점 공개 정책 (CVD, §524B), **사건대응 7단계, 보안 테스트 피라미드 8종** | ✅ **v1.0 승인 완료** |
| [docs/security/spdf-plan.md](docs/security/spdf-plan.md) | REQ-SEC-010~026 | Secure Product Development Framework — **STRIDE 5경계 19위협, IEC 81001 69% 준수, SLSA 로드맵** | ✅ **v1.0 승인 완료** |
| [docs/interop/dicom-conformance-statement.md](docs/interop/dicom-conformance-statement.md) | REQ-IOP-005 | DICOM Conformance Statement v1.0 — 검증 절차·유지보수·버전관리 섹션 추가 | ✅ **v1.0 승인 완료 (세션 5차)** |
| [docs/operations/pms-plan.md](docs/operations/pms-plan.md) | REQ-OPS-001~006 | Post-Market Surveillance Plan v1.0 — AI 모니터링·드리프트 감지·필드 배포 체계 추가 | ✅ **v1.0 승인 완료 (세션 5차)** |

관련 SPEC: [SPEC-XPE-SEC](.moai/specs/SPEC-XPE-SEC/spec.md) · [SPEC-XPE-IOP](.moai/specs/SPEC-XPE-IOP/spec.md) · [SPEC-XPE-OPS](.moai/specs/SPEC-XPE-OPS/spec.md)

### 심층 연구 산출물 (Deep Research Artifacts)

| 문서 | 설명 |
|------|------|
| [XPE-PreProcess-DeepResearch.json](docs/project/XPE-PreProcess-DeepResearch.json) | 9개 전처리 스테이지 심층 분석 |
| [XPE-PostProcess-DeepResearch.json](docs/project/XPE-PostProcess-DeepResearch.json) | 24개 후처리 모듈 심층 분석 |

### 실행 계획 및 PRD

| 문서 | 설명 |
|------|------|
| [XPE-PRD-002](docs/post-processing/xpe/XPE-PRD-002_Detailed_Project_Execution_PRD.md) | 상세 실행 PRD |
| [XPE-PRD-003](docs/post-processing/xpe/XPE-PRD-003_PRD_Decomposition_and_Backlog.md) | PRD 분해 및 백로그 (기록상 백로그) |
| [XPE-PLAN-001](docs/post-processing/xpe/XPE-PLAN-001_Consolidated_Execution_Plan.md) | 통합 실행 계획 |

---

## 저장소 레이아웃 (Repository Layout)

```text
docs/                       문서 체계 (Normative/Informational/Archive + IEC 62304)
  project/                  핵심 사양 (Normative 13개 + Informational 5개)
  post-processing/xpe/      XPE IEC 62304 Class B 패키지 (22개)
  post-processing/gsvg/     GSVG IEC 62304 Class B 패키지 (13개)
  ghost-correction/         Ghost Correction IEC 62304 패키지 (9개)
  calibration/              전처리 Calibration 모듈 문서
  security/                 보안 문서 (SPDF Plan, 위협모델 등) ← NEW
  interop/                  상호운용성 문서 (DICOM CS, DICOMweb 등) ← NEW
  operations/               운영·PMS 문서 (PMS Plan, 런북 등) ← NEW
  design/                   GUI Design handoff (ImageProcTest 리디자인 프로토타입, 구현 가이드) ← NEW
  quality-eval/             품질 평가 방법론 연구
  references/               외부 참고자료 및 기술 분류
  archive/                  대체된 문서 (감사 추적용)
  development/              CI/CD 및 빌드 운영 가이드
SECURITY.md                 취약점 공개 정책 (CVD) ← NEW
modules/common/             네이티브 공통 ABI 및 메모리 기초 요소
third_party/                vcpkg 매니페스트
tools/ci/                   GitHub 검증 및 번들링 스크립트
.github/workflows/          CI/CD 파이프라인
.github/ISSUE_TEMPLATE/     에픽, 백로그, 문서 동기화 템플릿
.moai/specs/                SPEC 문서 (SPEC-XPE-*, SPEC-SIMD-001, SPEC-BENCH-*, SPEC-XPE-GSVG 등)
```

---

## 빌드 기준 (Build Baseline)

- 최상위 빌드 시스템: `CMake`
- 네이티브 언어 기준: `C++17`
- 의존성 관리자: `vcpkg`
- 현재 CI 빌드 대상: `modules/common`
- CI 테스트 대상은 `.github/workflows/ci.yml` 의 잡 정의가 기준입니다. (이 자리에 `tests/common_smoke` 가 적혀 있었으나 그 디렉터리는 삭제됐습니다 — 2026-09-17 정정)

유용한 로컬 명령어:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Validate-Repo.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Test-MarkdownLinks.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Test-TrackedTextFiles.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Invoke-LocalVsCommonBuild.ps1 -Clean
cmake --preset ci-common
cmake --build --preset ci-common
ctest --test-dir build/ci-common --output-on-failure --build-config RelWithDebInfo

# Preprocess 전용 (Golden Reference + Calibration round-trip)
cmake --preset ci-preprocess
cmake --build --preset ci-preprocess --parallel
ctest --test-dir build/ci-preprocess --output-on-failure --build-config RelWithDebInfo
```

### ImageProcTest GUI fixture E2E

GitHub Issue #28 tracks the GUI preprocess R4 fixture acceptance gate. After local calibration raw payloads and native DLLs are staged, run:

```powershell
dotnet build clients\ImageProcTest\ImageProcTest.csproj -c Debug
dotnet clients\ImageProcTest\bin\Debug\net8.0-windows\ImageProcTest.dll --probe-native-readiness
dotnet clients\ImageProcTest\bin\Debug\net8.0-windows\ImageProcTest.dll --run-preprocess-fixture-e2e
```

The fixture E2E command writes JSON and Markdown reports under `clients/ImageProcTest/bin/Debug/net8.0-windows/preprocess-fixture-e2e/`. It records PRE-E2E-0 inventory, PRE-E2E-2 matching fixture execution, PRE-E2E-5 mismatch-negative evidence, raw SHA-256 preservation, calibration loads, native stage execution, latency, delta metrics, and blocker details.

---

## GitHub CI/CD

저장소는 단계적 GitHub 파이프라인을 사용합니다:

| 워크플로우 | 역할 |
|-----------|------|
| `Repository Guard` | 필수 파일, 백로그/PRD 일관성, ABI 플래그 고유성, 마크다운 링크, 병합 충돌 표시, 후행 공백 검증 |
| `Windows Common Build` | 경량 공통 매니페스트 복원, 컴파일러 경고를 오류로 처리, `xpe_common` 빌드 및 스모크 테스트 |
| `Preprocess Tests` | `xpe_preprocess` 전용 빌드 — Golden Reference 수식 검증 26개 + Calibration round-trip 테스트 |
| `Benchmark Regression (BP-06~09 + 3000ms)` | 수동 dispatch — 벤치마크 동결 GTest + `FullPipelineE2E.PostProcess_3072x3072_Within3000ms` 예산. BP-10(저하 모드)은 `ci.yml` dotnet-tests 의 `DegradedModeReadinessTests` 가 담당(2026-09-10, #56 #128) |
| `Delivery Bundle` | `main` 브랜치에 현재 프로젝트 기준을 아티팩트로 패키징 |
| `Release Bundle` | `v*` 태그에서 배포 번들을 GitHub Releases에 게시 |
| `CodeQL` | C/C++ 기준에 대한 정적 보안 및 품질 분석 (주간 반복) |
| `Dependabot` | GitHub Actions 버전 자동 업데이트 |

검증 워크플로우는 주간 일정으로도 실행되어 저장소 유휴 시에도 의존성/워크플로우 드리프트를 감지합니다.

---

## 배포 전략 (Delivery Strategy)

실행은 기능 덤프가 아닌 단계적으로 진행됩니다:

1. **Phase 0**: ABI, 데이터셋 계약, 셸, 검증 게이트
2. **Phase 1a**: 감지기 도메인 전처리 기준
3. **Phase 1b**: 기본 향상, EI 기준, 디스플레이, DICOM
4. **Phase 2**: 고급 결정론적 임상 처리
5. **Phase 3**: 샌드박스형 AI 워커 및 프리미엄 기능
6. **릴리스 경화**: 공식 패키지 동기화 및 증거 종료

기록상 백로그는 `XPE-PRD-003`입니다.

---

## 기여 주의사항 (Contribution Notes)

- 감지기 도메인 메트릭과 표현 도메인 메트릭을 혼합하지 마십시오.
- 측면 DLL 의존성을 도입하지 마십시오.
- `gsvg.dll`을 `XPE` 패키지 경계에서 독립적으로 유지합니다.
- 규제 문서를 변경할 때는 연결된 `SRS`, `SAD`, `SDD`, `RTM`, `VVP` 아티팩트를 함께 업데이트합니다.
- 워크플로우/구성 변경을 도메인 문서 변경과 분리하여 유지합니다.

---

## 기밀성 (Confidentiality)

이 저장소는 X-ray 이미지 처리를 위한 내부 계획, 규정 준수 및 구현 기준 자료를 포함합니다. 라이선스 및 공개 범위가 명시적으로 게시될 때까지 모든 콘텐츠를 보수적으로 취급합니다.
