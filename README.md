# image-processing

X선 평판 디텍터(FPD) 영상 처리 엔진 `XPE`를 만드는 저장소다. 네이티브 C++ 모듈, C# 운영자 앱,
그 근거가 되는 요구사항·설계·규제 문서와 CI 설정을 한곳에서 관리한다.

## 처음 왔다면

| 하고 싶은 것 | 볼 곳 |
|---|---|
| 빌드하고 시험 돌리기 | 이 문서의 [빌드와 시험](#빌드와-시험) |
| 2G 결과와 새 전처리(PRE) 결과를 앱에서 비교하기 | [docs/calibration/HOWTO-Compare-2G-PRE.md](docs/calibration/HOWTO-Compare-2G-PRE.md) |
| 전처리 보정 알고리즘 검증 방법 | [docs/calibration/ALGORITHM-VERIFICATION-GUIDE.md](docs/calibration/ALGORITHM-VERIFICATION-GUIDE.md) |
| 전체 문서 목록 | [docs/README.md](docs/README.md) |
| 남은 일과 결함 | [GitHub Issues](https://github.com/holee9/image-processing/issues) |

## 현재 상태는 측정하는 곳에서 본다

이 문서에는 테스트 수나 점수 같은 측정값을 적지 않는다. 산문에 적은 수치는 적는 순간부터 낡고,
이 저장소는 실제로 낡은 수치 때문에 여러 번 잘못 판단했다. 현재 값은 아래에서 직접 확인한다.

| 알고 싶은 것 | 어디서 |
|---|---|
| 테스트 통과 수 | `ctest --test-dir build/<preset>` |
| CI 상태 | GitHub Actions — `XPE CI Pipeline`, `Documentation Generation`, `Delivery Bundle` |
| 성능 | `XPE Benchmark Regression` 워크플로. 기준값은 `.github/workflows/`와 SPEC에 있다 |
| 요구사항과 수용 기준 | `.moai/specs/SPEC-*/` 의 `spec.md`, `plan.md`, `acceptance.md` |

문서에 수치를 꼭 적어야 하면 측정한 날짜와 명령을 함께 적는다.

## 빌드와 시험

- 빌드 시스템: CMake, 언어: C++17 · C#(.NET 8), 의존성: vcpkg
- CI가 무엇을 빌드하고 시험하는지는 `.github/workflows/ci.yml`의 잡 정의가 기준이다.

```powershell
# 저장소 검사
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Validate-Repo.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Test-MarkdownLinks.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Test-TrackedTextFiles.ps1

# 공통 모듈
cmake --preset ci-common
cmake --build --preset ci-common
ctest --test-dir build/ci-common --output-on-failure --build-config RelWithDebInfo

# 전처리 모듈
cmake --preset ci-preprocess
cmake --build --preset ci-preprocess --parallel
ctest --test-dir build/ci-preprocess --output-on-failure --build-config RelWithDebInfo

# 운영자 앱
dotnet build gui/ImageProcTest/ImageProcTest.csproj -c Debug

# 2G·PRE 비교 계약 시험
cd tools/calibration
python -m unittest test_comparison_manifest
```

## C# 앱이 둘이다

이름이 같아 문서 오류가 실제로 났으므로 구분해 둔다.

| 앱 | 역할 | 주의 |
|---|---|---|
| `gui/ImageProcTest` | 운영자 앱. E2E 시험이 띄우는 앱이다 | AutomationId에 `XPE_` 접두를 쓰지 않는다 |
| `clients/ImageProcTest` | 네이티브 진단 앱 | AutomationId에 `XPE_` 접두를 쓴다 |

진단 앱의 전처리 fixture E2E([#28](https://github.com/holee9/image-processing/issues/28))는 보정 원시
파일과 네이티브 DLL을 준비한 뒤 아래처럼 실행한다. 결과 보고서는
`clients/ImageProcTest/bin/Debug/net8.0-windows/preprocess-fixture-e2e/`에 생긴다.

```powershell
dotnet build clients\ImageProcTest\ImageProcTest.csproj -c Debug
dotnet clients\ImageProcTest\bin\Debug\net8.0-windows\ImageProcTest.dll --probe-native-readiness
dotnet clients\ImageProcTest\bin\Debug\net8.0-windows\ImageProcTest.dll --run-preprocess-fixture-e2e
```

운영자 앱 실제 영상 점검 기록(2026-11-03)은
[docs/design/VERIFICATION-20261103-live-real-frame.md](docs/design/VERIFICATION-20261103-live-real-frame.md)에 있다.

## 개발 세션

개발은 워크트리를 나눈 세션들이 나눠 맡는다. 소유 경계와 기록 규약은
[.moai/project/lane-sessions.md](.moai/project/lane-sessions.md)에 있다.

| 세션 | 워크트리 | 맡는 경로 |
|---|---|---|
| `xpe-leader` | `image-processing` | 루트 `CMakeLists.txt`, `cmake/`, `.moai/`, `.claude/`, `docs/`, `tools/`, `.github/workflows/` |
| `xpe-pre` | `xpe-pre` | `modules/common`, `modules/preprocess` |
| `xpe-post` | `xpe-post` | `modules/{enhance_basic,enhance_advanced,ai,display,dicom,gsvg}` |
| `xpe-gui` | `xpe-gui` | `clients/`, `gui/` |

측정 보고서는 `.moai/reports/lane-*/`에 남긴다. 다시 측정하는 데 필요한 명령·수치·경로는 해당
이슈 코멘트에도 적는다. 원시 영상 같은 대용량 데이터는 커밋하지 않는다.

## 문서 구성

전체 목록은 [docs/README.md](docs/README.md)에 있다. 문서는 세 등급으로 나눈다.

| 등급 | 뜻 |
|---|---|
| Normative | 기준 문서. 다른 문서와 내용이 다르면 이 문서가 우선한다 |
| Informational | 분석·계획·운영 안내. 기준 문서를 따른다 |
| Archive | 대체된 문서. 이력 확인용이다 |

### 기준 문서

| 문서 | 내용 |
|---|---|
| [SPEC-XPE-MASTER](docs/project/SPEC-XPE-MASTER.md) | 전체 구현 계획과 소프트웨어 단위 목록 |
| [pipeline-spec](docs/project/pipeline-spec.md) | 처리 단계 순서와 의존 관계 |
| [api-spec](docs/project/api-spec.md) | 내보내는 C ABI 함수 계약 |
| [xpe-algorithm-spec-deepsync](docs/project/xpe-algorithm-spec-deepsync.md) | 알고리즘 계약과 사이드카 지침 |
| [XPE-ALG-001](docs/post-processing/xpe/XPE-ALG-001_Unified_Algorithm_Development_Specification.md) | 통합 알고리즘 상세 설계(수식, 보정 코드, 검증 기준) |
| [product](docs/project/product.md) | 단계별 배포 범위와 필수·선택 바이너리 |
| [structure](docs/project/structure.md) | 모듈과 바이너리 대응, 두 C# 앱의 역할 |
| [tech](docs/project/tech.md) | 기술 스택, 외부 의존성, ABI 설계 |
| [Algorithm-Benchmark-Pack-Spec](docs/project/Algorithm-Benchmark-Pack-Spec.md) | 벤치마크 묶음 정의와 동결 규칙 |
| [Algorithm-Evaluation-Protocol](docs/project/Algorithm-Evaluation-Protocol.md) | 알고리즘 개정 비교·승격·보류 기준 |
| [Regulatory-Feature-Boundary-Matrix](docs/project/Regulatory-Feature-Boundary-Matrix.md) | 기능별 출시 가능 여부 구분 |
| [XPE-PRD-002](docs/post-processing/xpe/XPE-PRD-002_Detailed_Project_Execution_PRD.md) | 상세 실행 PRD |
| [XPE-PRD-003](docs/post-processing/xpe/XPE-PRD-003_PRD_Decomposition_and_Backlog.md) | PRD 분해와 백로그(기록상 백로그) |

### IEC 62304 문서 묶음

소프트웨어 항목마다 요구사항(SRS), 구조(SAD), 위험 분석(SHA), 추적표(RTM) 등을 둔다.

| 소프트웨어 항목 | 위치 |
|---|---|
| XPE (시스템 수준) | [docs/post-processing/xpe/](docs/post-processing/xpe/xpe-iec62304-class-b-package.md) |
| GSVG (가상 그리드) | [docs/post-processing/gsvg/](docs/post-processing/gsvg/GSVG_IEC62304_ClassB_Document_Package.md) |
| 전처리 보정 (Calibration) | [docs/calibration/](docs/calibration/) — 단일점 게인만 지원. 온도 보정·비선형·다항 게인은 2026-11-16 사용자 결정으로 제거했다. 결함 화소 맵 2판은 기준 미달 4항목을 한계로 기록하고 닫았다(2026-11-17, [#273](https://github.com/holee9/image-processing/issues/273)) |
| 잔상 보정 (Ghost Correction) | [docs/ghost-correction/](docs/ghost-correction/README.md) |
| 패널 불량 보정 (Panel Defect) | [docs/panel-defect/](docs/panel-defect/) |
| 기본 향상 (Enhance Basic) | [docs/enhance-basic/](docs/enhance-basic/) |
| 고급 향상 (Enhance Advanced) | [docs/enhance-advanced/](docs/enhance-advanced/) |
| AI 모듈 | [docs/ai-module/](docs/ai-module/) |
| 표시 처리 (Display) | [docs/display/](docs/display/) |
| 영상 파일 입출력 | [docs/dicom/](docs/dicom/) |
| 공통 기반 (Common) | [docs/common/](docs/common/) |

일정·분석·연구 문서는 [docs/README.md](docs/README.md)에 정리되어 있다.

보안·운영 문서: [SECURITY.md](SECURITY.md), [docs/security/](docs/security/),
[docs/interop/](docs/interop/), [docs/operations/](docs/operations/).

## 저장소 구성

```text
modules/            네이티브 C++ 모듈 (common, preprocess, enhance_*, ai, display, gsvg ...)
gui/ImageProcTest/  운영자 앱 (C# WPF)
clients/            네이티브 진단 앱과 E2E 시험
tools/ci/           저장소 검사·빌드·번들 스크립트
tools/calibration/  2G·PRE 출력 비교 계약 스크립트
docs/               기준·설계·규제 문서
.moai/specs/        SPEC 문서
.github/workflows/  CI 설정
third_party/        vcpkg 매니페스트
```

## CI 워크플로

| 워크플로 | 하는 일 |
|---|---|
| `XPE CI Pipeline` | 텍스트 검사, 모듈 빌드·시험, ASan, .NET 시험, GUI E2E, 정적 분석, 커버리지 |
| `Repository Guard` | 필수 파일, 문서 일관성, 링크, 병합 충돌 표시, 줄 끝 공백 검사 |
| `Windows Common Build` | 공통 모듈 관련 경로가 바뀌면 `xpe_common` 빌드 |
| `Documentation Generation` | 공개 헤더 등 문서 원천이 바뀌면 API 문서 생성 |
| `XPE Benchmark Regression` | 수동 실행. 벤치마크 동결 시험과 3000 ms 처리 시간 기준 |
| `Delivery Bundle` | `main`의 현재 상태를 아티팩트로 묶음 |
| `Release Bundle` | `v*` 태그에서 배포 묶음을 GitHub Releases에 게시 |
| `CodeQL` | C/C++ 정적 보안 분석(주 1회) |
| `Label Sync` | 수동 실행. 이슈 라벨 동기화 |

## 기여할 때

- 디텍터 원시 값 기준 지표와 표시용 지표를 섞지 않는다.
- 모듈 사이에 새로운 옆방향 DLL 의존을 만들지 않는다.
- `gsvg.dll`은 `XPE` 패키지와 따로 유지한다.
- 규제 문서를 바꾸면 연결된 SRS, SAD, SDD, RTM, VVP를 함께 고친다.
- 코드를 바꾼 커밋에서 관련 문서도 함께 고친다.

## 기밀성

이 저장소에는 내부 계획, 규제 대응, 구현 자료가 들어 있다. 라이선스와 공개 범위가 정해질
때까지 모든 내용을 외부에 공개하지 않는다.
