# GUI-C-225 — GUI-IT 묶음 A(020·031·041)와 결정된 008·050 시험 (#249)

대상: `clients/ImageProcTest.IntegrationTests`. 결정은 리더가 전한 사용자 결정(SPEC v1.3.2): 008 은 로케이터 후보 다섯, 050 은 "호스트 생존 + SEHException 기록·실패", 063~065 는 보류.

## 먼저: 219d 의 결함 하나 (별도 커밋 `52b64ec8`, GUI-C-225a)

병합된 219d 의 로드 모듈 감사는 "별도 프로세스인 실제 워커"를 전제하는데, `TheWorker_WritesExactlyOneParsableResultLine` 은 `RunWorker` 를 시험 호스트 안에서 부른다. 같은 호스트의 다른 시험 고정물이 다른 폴더에서 `xpe_common.dll`·`spdlog.dll` 을 이미 로드했으면 그 모듈이 "스냅샷 밖"으로 보고되어, 시험 순서에 따라 실패했다. 환경 폴더가 스테이징된 bin 과 다를 때 두 번 관측했고(`Loaded from outside the snapshot: …\c215_native\xpe_common.dll; …\spdlog.dll`) 일관된 구성에서는 재현되지 않았다. `RunWorker(…, confineLoad = true)` 로 고쳤다(실제 워커는 기본값, 시험 호스트 안 호출만 false). 증거와 순서 의존에 관한 정직한 주석은 `219d_audit_in_process_before_after.txt`. 앱 코드에 "검사를 끄는 길"이 생긴 것이므로 리더가 말한 대로 Codex 검토 대상이다.

## 시험 코드 변경 요약

| 요구 | 시험 | 위치 |
|---|---|---|
| 020 | `DllLoadSmokeTests.TheDll_IsLocatedAndAnswersXpeVersion_WithinTheRequirementsFiveSeconds` — 로케이터와 첫 `xpe_version` 호출을 함께 재서 5초(`LocateBudget`)와 비교, semver 패턴 | `Smoke/DllLoadSmokeTests.cs` |
| 031 | `LoggingHandlerTests.LogFlush_PostInit_DoesNotThrow` 신설. 초기화 전·후·종료 후 세 상태가 한 클래스에 모이고, 각 시험이 먼저 자기 상태에 있음을 `xpe_get_param_range`(NOT_INITIALIZED 여부)로 단언한다 | `Lifecycle/LoggingHandlerTests.cs` |
| 041 | `DllLoadSmokeTests.TheLocator_WithNothingToFind_ReportsNotFound_AndNamesTheFoldersItSearched`(대조: DLL 이 있는 폴더는 찾음), `…LoadingAMissingDll_RaisesDllNotFoundException_ThatNamesIt` | `Smoke/DllLoadSmokeTests.cs` |
| 008 | `DllSearchPathSafetyTests` 에 `TheCandidateCheck_RejectsADecoyOnPath_ASystemFolder_AndAnArbitraryFolder_AndAcceptsACandidate`, `TheCandidateList_IsExactlyTheFiveKindsOfFolderTheRequirementNames` 신설, `ResolvedDllPath_IsUnderBuildTreeOrTestOutput` 에 "후보 폴더 아래" 단언 추가 | `Safety/DllSearchPathSafetyTests.cs` |
| 050 | `BoundaryGuardTests`(SEHException → 기록 + 실패, 정상 반환·다른 예외는 그대로), `NegativeInputPathTests.EveryRegisteredNegativeRow_RunsToCompletion_ThroughTheBoundaryGuard_WithTheHostAlive`(실행 수 = 등록 수) | `Safety/BoundaryGuardTests.cs`, `Fixtures/BoundaryGuard.cs`, `ErrorMapping/NegativeInputPathTests.cs` |

시험 전용 코드가 아닌 변경은 `NativeLibraryFixture`(고정물, 시험 프로젝트 안) 하나다. 로케이터를 `CandidateFolders`(후보 목록 하나)·`Locate(env, baseDir)`(입력을 주는 형태)·`IsUnderAnyFolder` 로 나눴다. `TryLocateDll()` 은 같은 동작이다(탐색 순서·후보 동일). 못 찾았을 때의 메시지가 찾아본 폴더를 말하도록 바뀌었다(이전: "Repository root could not be determined" 뿐).

## 요구별 설명

**008.** 로케이터가 후보 목록을 하나로 노출하고(`CandidateFolders`), 시험은 그 목록과 비교한다(하드코딩 없음). 다섯 종류(XPE_NATIVE_DIR, `<repo>/build/`, `modules/common/build_test`, 시험 출력 폴더, `clients/ImageProcTest/bin`)는 요구 문구에서 가져와 시험에 적었고, 로케이터의 목록이 이 다섯 종류 밖의 폴더를 갖거나 다섯 중 하나를 잃으면 두 번째 시험이 빨개진다("한쪽만 바뀌면 알아챔"). 반증 입력 셋(PATH 맨 앞 디코이 폴더, 시스템 폴더, 후보 밖 임의 폴더)에 접두 일치 형제 폴더(`build-old`)와 `build/other` 를 더했다. 이 반증 시험은 순수 함수에 대한 것이라 네이티브 DLL 없이도 돈다.

**020.** 요구에 숫자(5초)가 있어서 숫자를 단언한다. 측정 대상은 로케이터 호출 + `xpe_version` 호출이다(고정물이 이미 로드해 둔 뒤라 "첫 호출"의 로드 비용은 이 측정에 들어 있지 않다).

**031.** 새 시험은 `xpe_init` 후에만 상태 단언을 통과한다. `xpe_init` 호출을 빼면 "초기화된 상태가 아님"으로 빨개진다(통제, 아래 표).

**041.** 요구는 "고정물 부트스트랩이 `DllNotFoundException` 을 올린다"인데, 현재 고정물은 못 찾으면 올리지 않고 `IsAvailable=false` + `SkipReason`(찾아본 곳을 포함)을 낸다(테스트 발견이 호스트를 죽이지 않게). 그래서 시험은 두 반쪽이다: 로케이터가 (found=false, 찾은 폴더를 말하는 메시지)를 돌려준다, 그리고 런타임 로더가 없는 DLL 이름으로 `DllNotFoundException` 을 올리며 메시지에 그 이름이 있다. 두 번째는 런타임이 만드는 메시지이므로 그 메시지를 제품 코드가 바꿔서 이름을 뺄 수는 없고, 반증은 "시험이 다른 이름을 기대하면 빨강"(단언이 이름을 구별한다는 것)까지만 보인다. 두 번째 시험이 `[DllImport]` 대신 `NativeLibrary.Load` 를 쓰는 이유: 시그니처 일치 시험이 선언된 모든 extern 을 읽어서 지어낸 선언이 발견으로 잡혔다(작성 중 실제로 실패해서 알았다).

**050.** 요구가 말하는 AccessViolation 은 .NET 이 잡지 못하고 호스트를 끝내므로 시험으로 단언할 수 없고, 그 경우는 호스트 종료 = 실행 실패로 드러난다(요구 문구가 그렇게 바뀌었다). 시험할 수 있는 것: `BoundaryGuard.Invoke` 가 SEHException 을 기록하고 시험을 실패시키는 것, 모든 부정 시나리오가 끝까지 도는 것. 부정 행 18개를 모두 가드 아래서 한 번씩 돌려 "실행된 수 = 등록된 수"를 비교한다(기존 이론 시험의 행도 같은 가드를 통과한다).

## 반증 (`falsification_*.txt`)

| 요구 | 일부러 넣은 결함 | 결과 |
|---|---|---|
| 020 | 한도 5초 → 0.001 ms | 빨강 1 |
| 031 | post-init 시험에서 `xpe_init` 호출 제거 | 빨강 1(상태 단언) |
| 041a | 못 찾았을 때 메시지에서 찾은 폴더 제거 | 빨강 1 |
| 041b | 시험이 다른 DLL 이름을 기대 | 빨강 1(이름을 구별함을 보임; 위 설명 참조) |
| 008a | 후보 검사가 항상 true | 빨강 1 |
| 008b | 로케이터 후보에 시스템 폴더 추가 | 빨강 2(후보 종류 시험 + 거부 시험) |
| 050a | 가드가 SEHException 을 잡지 않음 | 빨강 1 |
| 050b | 완전성 시험에서 한 행을 건너뜀 | 빨강 1 |

각 반증 뒤 원본을 복원하고 전체를 다시 돌렸다.

## 실행 증거

- 통합 시험 전체, 네이티브 DLL 폴더 지정: 857 통과 / 0 실패 / 0 건너뜀 (219d 848 + 9건) — `integration_full_suite_with_native.txt`
- 같은 시험, `XPE_NATIVE_DIR` 없음: 857 통과 — `integration_full_suite_without_native.txt`
- 네이티브 DLL 을 뺀 출력 복사본으로 실행: 856 통과 / 1 건너뜀 — `integration_full_suite_no_native_dll.txt`. 다만 로케이터가 저장소의 build 폴더에서 DLL 을 찾았을 수 있어 "DLL 이 아예 없는 구성"을 재현했다고는 말하지 못한다.
- `requirement-matrix.json`(`clients/` 안): REQ-GUI-IT-050 에 `NegativeInputPathTests`·`BoundaryGuardTests` 행 추가. RTM 문서 수정안은 `rtm_patch_proposal.txt`(리더 반영용, 인용한 시험 이름은 모두 grep 으로 존재 확인).

## 미검증과 잔여 위험

- CI 의 Mock 구성(네이티브 DLL 이 없는 잡)에서는 실행하지 않았다. 새 시험 중 네이티브가 필요한 것은 기존과 같은 `SkipHelper` 로 건너뛰고, 나머지(008 순수 시험, 041 로케이터, 050 가드)는 DLL 없이 돈다고 읽었지만 그 구성에서 돌려 보지는 못했다.
- 020 의 5초는 로컬의 빠른 디스크 기준이다. 느린 CI 러너에서 한도에 닿는지는 모른다(전체 호출이 밀리초 단위라 여유는 크다고 읽는다).
- 050 의 "AccessViolation 은 호스트 종료로 드러난다"는 가정은 시험으로 보이지 않았다(보일 수 없다).
- 041 의 `DllNotFoundException` 메시지는 런타임 소유라 .NET 버전에 따라 문구가 달라질 수 있다. 시험은 DLL 이름만 단언한다.
- RTM 의 상태 수치(구현됨/부분/없음)는 다시 세지 않았다. 수정안에 그렇게 적었다.
- 리더가 후속으로 권고한 두 건(의존 DLL 소실 주입 시험, 버튼 → 거절 문구 UI 시험)은 이 카드에서 하지 않았다.
