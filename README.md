# image-processing

X-ray Flat Panel Detector (FPD) 이미지 처리 연구, 실행 계획 및 구현 부트스트랩 저장소입니다.

이 저장소는 현재 `docs-first` 상태이며 X-ray 이미지 처리 엔진 (`XPE`)을 위한 배포 가능한 엔지니어링 기준으로 업그레이드되고 있습니다. 즉시 목표는 제품 계획, 규제 문서, 네이티브 모듈 인터페이스, GitHub 배포 자동화를 처음부터 동기화된 상태로 유지하는 것입니다.

## 범위 (Scope)

- 원본 감지기 도메인 입력부터 DICOM 배포까지 `XPE`의 실행 기준을 정의합니다.
- `PRD`, 백로그, 아키텍처, IEC 62304 패키지 문서를 정렬된 상태로 유지합니다.
- `C/C++` 모듈과 `C#` 호스트/오케스트레이터 계층 주변에 안정적인 네이티브 핵심을 구축합니다.
- 구현 규모 증가 전에 GitHub Actions를 통해 품질 게이트를 적용합니다.

## 주요 문서 (Key Documents)

### 사양 및 계획 (Specifications and Planning)

- **마스터 SPEC**: [.moai/specs/SPEC-XPE-MASTER/spec.md](.moai/specs/SPEC-XPE-MASTER/spec.md) -- 43 SWU 인벤토리, Phase 0-3 분해 (v2.0.0)
- **알고리즘 사양 (DeepSync)**: [docs/project/xpe-algorithm-spec-deepsync.md](docs/project/xpe-algorithm-spec-deepsync.md) -- 규범적 알고리즘 계약 (v3.0.0-ds2)
- **파이프라인 사양**: [docs/project/pipeline-spec.md](docs/project/pipeline-spec.md) -- 17단계 정규 파이프라인 및 의존성 그래프 (v1.3.0)
- **API 사양**: [docs/project/api-spec.md](docs/project/api-spec.md) -- 82개 내보낸 C ABI 함수 (v1.2.0)
- **모듈 보강 계획**: [docs/project/XPE-Module-Reinforcement-Plan.md](docs/project/XPE-Module-Reinforcement-Plan.md) -- Pre/Post 모듈 정밀 보강 계획 및 혁신 로드맵

### 심층 연구 산출물 (Deep Research Artifacts)

- **전처리 심층 연구**: [docs/project/XPE-PreProcess-DeepResearch.json](docs/project/XPE-PreProcess-DeepResearch.json) -- 9개 전처리 스테이지 심층 분석
- **후처리 심층 연구**: [docs/project/XPE-PostProcess-DeepResearch.json](docs/project/XPE-PostProcess-DeepResearch.json) -- 24개 후처리 모듈 심층 분석

### 프로젝트 문서 (Project Documents)

- **구현 분석 보고서**: [docs/project/XPE-Implementation-Analysis-Report.md](docs/project/XPE-Implementation-Analysis-Report.md)
- **제품 정의**: [docs/project/product.md](docs/project/product.md)
- **아키텍처 구조**: [docs/project/structure.md](docs/project/structure.md)
- **기술 스택**: [docs/project/tech.md](docs/project/tech.md)

### 실행 및 규정 준수 (Execution and Compliance)

- 상세한 실행 PRD: [docs/post-processing/xpe/XPE-PRD-002_Detailed_Project_Execution_PRD.md](docs/post-processing/xpe/XPE-PRD-002_Detailed_Project_Execution_PRD.md)
- PRD 분해 및 백로그: [docs/post-processing/xpe/XPE-PRD-003_PRD_Decomposition_and_Backlog.md](docs/post-processing/xpe/XPE-PRD-003_PRD_Decomposition_and_Backlog.md)
- CI/CD 및 로컬 빌드 실행 안내: [docs/development/XPE-CI-CD_LocalBuild_Runbook.md](docs/development/XPE-CI-CD_LocalBuild_Runbook.md)

## 저장소 레이아웃 (Repository Layout)

```text
docs/                       도메인 연구, PRD, IEC 62304 패키지 문서
modules/common/             네이티브 공통 ABI 및 메모리 기초 요소
tests/common_smoke/         CI용 최소 스모크 테스트
third_party/                vcpkg 매니페스트
tools/ci/                   GitHub 검증 및 번들링 스크립트
.github/workflows/          CI/CD 파이프라인
.github/ISSUE_TEMPLATE/     에픽, 백로그, 문서 동기화 템플릿
```

## 빌드 기준 (Build Baseline)

- 최상위 빌드 시스템: `CMake`
- 네이티브 언어 기준: `C++17`
- 의존성 관리자: `vcpkg`
- 현재 CI 빌드 대상: `modules/common`
- 현재 CI 테스트 대상: `tests/common_smoke`

유용한 로컬 명령어:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Validate-Repo.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Test-MarkdownLinks.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Test-TrackedTextFiles.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Invoke-LocalVsCommonBuild.ps1 -Clean
cmake --preset ci-common
cmake --build --preset ci-common
ctest --test-dir build/ci-common --output-on-failure --build-config RelWithDebInfo
```

## GitHub CI/CD

저장소는 현재 단계적 GitHub 파이프라인을 사용합니다:

- `Repository Guard`: 필수 파일 검증, 백로그/PRD 일관성, ABI 플래그 고유성, 마크다운 링크, 병합 충돌 표시, 코드/구성 파일의 후행 공백을 검증합니다.
- `Windows Common Build`: 경량 공통 매니페스트를 복원하고, 컴파일러 경고를 오류로 처리하며, `xpe_common`을 빌드하고 스모크 테스트를 실행합니다.
- `Delivery Bundle`: 현재 프로젝트 기준을 `main`에 아티팩트로 패키징합니다.
- `Release Bundle`: `v*` 태그에서 배포 번들을 패키징하고 GitHub Releases에 게시합니다.
- `CodeQL`: C/C++ 기준에 대한 정적 보안 및 품질 분석을 실행하며 일정에 따라 반복합니다.
- `Dependabot`: GitHub Actions 버전을 자동으로 최신 상태로 유지합니다.

검증 워크플로우는 또한 주간 일정으로 실행되므로 저장소가 유휴 상태일 때도 의존성 또는 워크플로우 드리프트가 감지됩니다.

## 배포 전략 (Delivery Strategy)

실행은 기능 덤프가 아닌 단계적으로 진행됩니다:

1. Phase 0: ABI, 데이터셋 계약, 셸, 검증 게이트
2. Phase 1a: 감지기 도메인 전처리 기준
3. Phase 1b: 기본 향상, EI 기준, 디스플레이, DICOM
4. Phase 2: 고급 결정론적 임상 처리
5. Phase 3: 샌드박스형 AI 워커 및 프리미엄 기능
6. 릴리스 경화: 공식 패키지 동기화 및 증거 종료

기록상 백로그는 `XPE-PRD-003`입니다.

## 기여 주의사항 (Contribution Notes)

- 감지기 도메인 메트릭과 표현 도메인 메트릭을 혼합하지 마십시오.
- 측면 DLL 의존성을 도입하지 마십시오.
- `gsvg.dll`을 `XPE` 패키지 경계에서 독립적으로 유지합니다.
- 규제된 문서를 변경할 때는 연결된 `SRS`, `SAD`, `SDD`, `RTM`, `VVP` 아티팩트를 함께 업데이트합니다.
- 가능한 경우 워크플로우/구성 변경을 도메인 문서 변경과 분리하여 유지합니다.

## 기밀성 (Confidentiality)

이 저장소는 X-ray 이미지 처리를 위한 내부 계획, 규정 준수 및 구현 기준 자료를 포함합니다. 라이선스 및 공개 범위가 명시적으로 게시될 때까지 모든 콘텐츠를 보수적으로 취급합니다.
