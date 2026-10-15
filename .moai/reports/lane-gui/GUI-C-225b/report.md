# GUI-C-225b — Codex #121 보류 5건 (#249)

커밋 둘: 앱 코드 `GUI-C-225b`(a)(`RunWorker` 의 매개변수 제거, 앱 코드 변경은 이것 하나), 시험·증거 `GUI-C-225b`(b). 시험 쪽은 `clients/ImageProcTest.IntegrationTests` 와 E2E 시험 프로젝트.

## 1. (높음) REQ-041 — 건너뜀 정책 문구(SPEC v1.3.3)에 시험을 맞춤

고정물에 입력을 받는 생성자를 두었다(`internal NativeLibraryFixture(envDir, baseDirectory, registerResolver)`; 매개변수 없는 생성자는 환경 변수·출력 폴더·`registerResolver: true` 로 이것을 부른다). 어셈블리의 DLL 해석기는 프로세스에 한 번만 걸 수 있어서, 시험이 만드는 고정물은 `registerResolver: false` 다.

`TheFixtureBootstrap_WithNoDllInAnyCandidate_DoesNotThrow_IsUnavailable_AndTheSkipReasonNamesEveryFolderSearched`: **실제 고정물**을 후보에 DLL 이 없는 상태(없는 환경 폴더 + 빈 출력 폴더)로 만들어 예외가 없고(`Record.Exception`), `IsAvailable=false` 이고, 건너뜀 사유가 탐색한 폴더를 전부(후보 목록에서 계산) 말하는지 단언한다. 대조: 실제 DLL 이 있는 폴더로 만든 고정물은 사용 가능하다. 직접 로드 쪽(`LoadingAMissingDll_RaisesDllNotFoundException_ThatNamesIt`)과 로케이터 반쪽은 그대로다.
반증: 고정물이 못 찾았을 때 `DllNotFoundException` 을 던지게 하면 이 시험이 빨개진다.

## 2. (중간) REQ-008 — 같은 종류 안의 후보 증감

`TheCandidateList_IsExactlyTheseFolders_InThisOrder`: 입력 네 가지(환경·출력·저장소 모두 / 환경 없음 / 저장소 없음 / 빈 환경)에 대해 후보를 **독립 기대값**(시험에 글자 그대로 적은 9개 경로의 순서와 개수)과 `Assert.Equal` 한다. 변이 반증: 후보 하나 추가(`ci-common\bin\Release`) → 빨강, 하나 제거(`default\bin\Debug`) → 빨강.

## 3. (중간) REQ-008 — 심볼릭 링크와 정션

`NativeLibraryFixture.FinalPathOf(path)`: 경로의 각 구간이 링크·정션이면 그 목적지로 바꾸고(목적지도 같은 방식으로 풀며 순환은 32단계에서 멈춤) 실제 위치를 돌려준다. P/Invoke 없이 `FileSystemInfo.LinkTarget`/`ResolveLinkTarget` 으로 만들었다(시그니처 일치 시험이 `[DllImport]` 를 전부 읽기 때문에 새 extern 을 피했다). `IsUnderAnyFolderResolved` 는 파일의 실제 위치가 승인된 폴더의 **실제 위치** 아래인지 본다(없는 후보 폴더는 건너뜀).
- 실제 시험의 `ResolvedDllPath_IsUnderBuildTreeOrTestOutput` 은 이 검사를 쓰고, 새 `TheLoadedModule_IsReallyUnderAnApprovedFolder_WhenItsLinksAreFollowed` 는 **로드 후** 프로세스 모듈 목록의 `xpe_common.dll` 경로를 같은 검사에 넣는다.
- 회귀 시험: `AJunctionInsideAnApprovedFolder_ThatPointsOutside_IsRejected_AndOneThatStaysInsideIsAccepted`(정션은 `mklink /J` 로 권한 없이 만들어 이 환경에서 **실행됨**). 문자열 검사는 속고(시험이 그것을 단언해 약점을 기록함) 링크를 따르는 검사는 거부하며, 통제로 평범한 파일과 안쪽을 가리키는 정션은 받아들인다. 반증: 링크를 따르지 않는 검사로 바꾸면 빨강.
- 파일 심볼릭 링크 시험(`AFileLinkInsideAnApprovedFolder_ThatPointsOutside_IsRejected`)은 **이 로컬에서는 건너뜀**이다(심볼릭 링크 권한 없음, 사유가 출력됨). CI 러너는 관리자라 거기서 돈다는 전제인데, 로컬에서 실행해 보지 못했다. 파일 링크의 해석은 정션과 같은 `FinalPathOf` 의 `ResolveLinkTarget` 경로라고 읽지만 직접 보지는 않았다.

## 4. (중간) REQ-020 — 첫 로딩 측정

고정물의 첫 생성이 스스로 재서 기록한다(`LocateDuration`, `LoadDuration`, `FirstCallDuration`: 로드 직후 `xpe_version` 의 첫 호출까지 고정물이 직접 부른다). 시험(`TheFirstFixtureCreation_LocatesTheDllWithinFiveSeconds_AndRecordedTheFirstLoadAndCall`)은 기록을 읽어 로케이터가 5초 안이고 로드·첫 호출이 기록되어 있으며 그 합도 5초 안임을 단언한다. 이미 로드된 뒤 다시 재는 부분은 없앴다. 반증: 로케이터에 6초 지연 → 빨강, 첫 로드에 6초 지연 → 빨강.
**카드와 다른 점**: "전용 프로세스에서 측정"을 하지 않았다. 통합 시험 프로젝트에는 시험 호스트가 아닌 별도 실행 파일이 없어서, 고정물이 "프로세스 안의 첫 생성"을 스스로 기록하는 형태로 했다. 첫 생성은 프로세스당 한 번(공유 고정물)이므로 구간은 요구가 말하는 "어셈블리가 로드될 때"와 같고 시험이 언제 도는지와 무관하다. 다만 느린 러너에서 다른 시험의 영향(디스크 경합)이 첫 생성에 섞일 수는 있다.

## 5. (낮음) `confineLoad` 매개변수 제거

앱 코드에서 `RunWorker(dllPath, output)` 로 되돌렸다(감사 끄는 길 없음). 프로토콜 시험은 E2E 프로젝트의 `TheWorker_WritesExactlyOneParsableResultLine_AsARealProcess_WhateverTheHostHasLoaded` 로 옮겼다: 실제 워커를 별도 프로세스로 띄워 표준 출력에서 정확히 한 줄, 결과 접두사, 실제 종료 코드로 부모의 파서를 통과하는지 본다. 시험 호스트가 먼저 **다른 폴더의** `xpe_common.dll` 복사본을 로드해 두어도 흔들리지 않는다(별도 프로세스라서 구조적으로 무관). 소스 스캔(`TheWorkerLoadsTheDllConfined_…`)은 앱 소스 어디에도 `confineLoad` 가 없음을 단언한다. 반증: 매개변수를 되살리면 스캔이 빨개진다.
확인: 환경 폴더가 스테이징된 bin 과 다른 구성에서 통합 시험 3회, 옛 순서 의존 실패(`TheWorker_…`)는 나오지 않았다(그 구성에서 간헐적으로 나오는 `NativeCommonSingleInstanceTests` 는 xpe_common 이 두 경로에서 로드되는 환경 자체의 문제로 이 변경과 무관하며, 3회 중 1회 나왔다).

## 반증 요약 (`falsification_*.txt`)

| 항목 | 일부러 넣은 결함 | 결과 |
|---|---|---|
| 008 | 후보 추가 / 후보 제거 | 각각 빨강 1 |
| 008 | 링크를 따르지 않는 검사(문자열만) | 정션 시험 빨강 |
| 041 | 고정물이 못 찾으면 예외를 던짐 | 빨강 1 |
| 020 | 로케이터 6초 지연 / 첫 로드 6초 지연 | 각각 빨강 1 |
| 5 | `confineLoad` 매개변수 재도입 | 스캔 빨강 |

## 실행 증거

- 통합 시험 전체(네이티브 DLL 폴더 지정): 860 통과 / 0 실패 / 1 건너뜀(파일 링크, 권한) — `integration_full_suite.txt`; 환경 변수 없이: 같은 결과 — `integration_full_suite_without_env.txt`
- 환경 폴더가 스테이징 폴더와 다른 구성 3회: `integration_mixed_env_c215_native_3_runs.txt`
- 레거시 E2E 전체: 15 통과 — `legacy_e2e_current_tree.txt`
- RTM 수정안 갱신: `rtm_patch_proposal_update.txt`(리더 반영용, 인용 이름 모두 grep 확인)

## 미검증과 잔여 위험

- 파일 심볼릭 링크 시험은 권한이 있는 환경에서 실행해 보지 못했다(위 3).
- 020 의 "전용 프로세스" 측정은 하지 않았다(위 4).
- CI Mock 구성과 CRLF 체크아웃에서의 실행은 없다.
- `FinalPathOf` 는 .NET 의 `LinkTarget` 이 아는 링크(심볼릭 링크, 정션)만 따른다. 마운트 지점 같은 다른 재구문점은 다루지 않는다.
- RTM 문서의 상태 수치는 다시 세지 않았다.
