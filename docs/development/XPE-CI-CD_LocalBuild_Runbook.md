# XPE CI/CD 및 로컬 빌드 런북

**문서 ID**: XPE-OPS-001
**버전**: 1.0.0
**날짜**: 2026-04-14
**상태**: 활성
**분류**: 내부

---

## 1. 목적

이 런북은 현재 `XPE` 엔지니어링 기준선을 다음을 통해 검증하는 방법을 정의합니다:

- GitHub Actions CI/CD
- 로컬 Visual Studio 2022 공통 모듈 빌드
- 재현 가능한 검증 스크립트

또한 현재 기준선의 최종 검증 결과를 기록합니다.

## 2. 범위

현재 자동화 범위는 의도적으로 `common` 기준선으로 제한됩니다:

- `modules/common`
- `tests/common_smoke`
- `third_party/common/vcpkg.json`
- 저장소 검증 및 릴리스 번들 패킹

워크플로우 집합은 누락된 다운스트림 모듈이 기준선 품질 게이트를 차단하지 않도록 설계되었습니다.

## 3. GitHub 워크플로우 집합

### 3.1 저장소 보호

워크플로우: `.github/workflows/repository-guard.yml`

목적:

- 필수 파일 존재 검증
- PRD/백로그/문서 동기화 검사
- ABI 메타데이터 플래그 고유성 검사
- 마크다운 링크 검증
- 추적된 텍스트 파일 위생 검사

주요 게이트:

- `tools/ci/Validate-Repo.ps1`
- `tools/ci/Test-MarkdownLinks.ps1`
- `tools/ci/Test-TrackedTextFiles.ps1`

일정:

- 주간 (`cron`)

### 3.2 Windows 공통 빌드

워크플로우: `.github/workflows/windows-common-build.yml`

목적:

- 경량 `vcpkg` 매니페스트 복원
- `ci-common` 구성
- `xpe_common` 빌드
- 스모크 테스트 실행

주요 게이트:

- `XPE_WARNINGS_AS_ERRORS=ON`을 통해 경고를 오류로 활성화
- `ctest --test-dir build/ci-common --output-on-failure`

일정:

- 주간 (`cron`)

### 3.3 CodeQL

워크플로우: `.github/workflows/codeql.yml`

목적:

- 현재 C/C++ 기준선에 대한 정적 보안 및 품질 분석

주요 게이트:

- `github/codeql-action/init@v4`
- `github/codeql-action/analyze@v4`

일정:

- 주간 (`cron`)

### 3.4 전달 번들

워크플로우: `.github/workflows/delivery-bundle.yml`

목적:

- 기준선 문서, 이슈 템플릿, 워크플로우, CI 스크립트, 공통 모듈 아티팩트를 `main`의 전달 아티팩트로 패킹

### 3.5 릴리스 번들

워크플로우: `.github/workflows/release-bundle.yml`

목적:

- 전달 번들을 `v*` 태그 또는 수동 디스패치에서 GitHub Releases로 발행

## 4. GitHub 검증 결과

`main`의 최신 검증 성공 세트:

- `저장소 보호`: 성공, 실행 `24372855093`
- `Windows 공통 빌드`: 성공, 실행 `24372855105`
- `CodeQL`: 성공, 실행 `24372855096`
- `전달 번들`: 성공, 실행 `24372855109`

검증 참조 날짜:

- 2026-04-14 KST

참고:

- 이전의 `Windows 공통 빌드` 실패 (`24372471657`)는 캐시된 `vcpkg` 루트 감지 로직으로 추적되었습니다. 워크플로우는 디렉토리 존재만 검증하는 대신 `bootstrap-vcpkg.bat` 존재를 검증하도록 수정되었습니다.

## 5. 로컬 Visual Studio 2022 빌드

### 5.1 주요 발견

로컬 빌드 문제는 Visual Studio 2022 설치 경로 검색 실패로 인한 것이 **아닙니다**.

저장소는 성공적으로 감지했습니다:

- `D:\Program Files\Microsoft Visual Studio\2022\Professional`

실제 차단 요소는:

- 개발자 셸 환경이 현재 셸에 로드되지 않음
- `vcpkg` 쓰기 가능 캐시/레지스트리 경로가 저장소 워크스페이스로 리디렉션되지 않음
- 구현 정의에서 누락된 `XPE_API` 내보내기 매크로는 경고가 오류로 승격되면 치명적이 됨

### 5.2 로컬 빌드 스크립트

스크립트:

- `tools/ci/Invoke-LocalVsCommonBuild.ps1`

수행 작업:

- `vswhere`로 Visual Studio 2022 위치 지정
- `VsDevCmd.bat` 환경을 현재 프로세스로 가져오기
- Visual Studio 번들 `cmake`, `ctest`, `ninja`, `vcpkg` 사용
- `LOCALAPPDATA`, `APPDATA`, `VCPKG_DOWNLOADS`, 바이너리 캐시, 레지스트리 캐시를 `.cache/`로 리디렉션
- `modules/common` 구성, 빌드, 테스트

### 5.3 로컬 실행 명령

```powershell
PowerShell -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Invoke-LocalVsCommonBuild.ps1 -Clean
```

빌드 트리가 이미 존재하고 빠른 재실행을 원하는 경우:

```powershell
PowerShell -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Invoke-LocalVsCommonBuild.ps1
```

## 6. 로컬 검증 결과

2026-04-14 KST에 로컬에서 검증됨:

- `Validate-Repo.ps1`: 통과
- `Test-MarkdownLinks.ps1`: 통과
- `Test-TrackedTextFiles.ps1`: 통과
- `Invoke-LocalVsCommonBuild.ps1`: 통과
- `xpe_common_smoke`: 통과

생성된 출력:

- `build/local-vs2022-common/bin/xpe_common.dll`
- `build/local-vs2022-common/bin/xpe_common_smoke.exe`

테스트 증거:

- `build/local-vs2022-common/Testing/Temporary/LastTest.log`

## 7. 빌드 완료를 위해 필요한 코드 레벨 수정 사항

최종 성공적인 로컬 빌드에는 다음과 같은 구현 수정이 필요했습니다:

- 다음에서 내보낸 함수 정의에 `XPE_API` 추가:
  - `modules/common/src/xpe_common.cpp`
  - `modules/common/src/xpe_memory.cpp`

이유:

- MSVC가 `C4273` 불일치 DLL 링크 경고를 내보냄
- CI는 `ci-common` 빌드에 대해 경고를 오류로 취급

## 8. 운영 지침

- `저장소 보호`, `Windows 공통 빌드`, `CodeQL`을 최소 필수 품질 게이트로 취급합니다.
- 임시 셸 설정보다 로컬 빌드 스크립트를 선호하며, 현재 머신 특정 제약을 캡처합니다.
- `.cache/`를 로컬로만 유지합니다. 이는 런타임 편의 영역이지 소스 아티팩트가 아닙니다.
- 새 모듈을 추가할 때 선택적 하위 디렉토리 전략을 붕괴하기보다는 현재 기준선을 증분적으로 확장합니다.

## 9. 다음 권장 단계

GitHub 분기 보호를 적용하여 병합 전에 다음 검사가 필요하도록 합니다:

- `저장소 보호`
- `Windows 공통 빌드`
- `CodeQL`

이는 "워크플로우 존재"에서 "품질 게이트 강제"까지의 최단 경로입니다.
