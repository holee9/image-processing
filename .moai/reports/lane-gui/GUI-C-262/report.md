[2026-11-13 14:38 KST] F262-1 반영 — 변수 제거 회차 4P는 실측, A06 내부 Real 선택은 미관측 추정

## 판정2 단일 실행 결과

**Claim:** 리더가 승인한 4개만 한 번 실행해 모두 통과했다. 이전 A/B B1/B2는 0P/4F였고, 제품·시험 소스가 같은 현재 트리에서 XPE_GUI_DATA_DIR만 제거한 이번 실행은 4P/0F/0SKIP다. **F262-1: A06은 변수 제거 회차 PASS만 실측이며, 내부 Real 선택은 미관측 추정이다.** evidence 3건은 같은 runId의 실제 파일과 저장·조회 루트 불일치를 확인했다. 시험 `EvidenceBundleScenarios.EvidenceRoot:209–210`이 XPE_GUI_DATA_DIR를 따르지 않는 점은 별도 결함 후보이며 이 회차에서는 수정하지 않았다.

| 시험 | 결과 | 실패 첫 줄 |
|---|---|---|
| A06_GuiE2E_RunsFromTheApp_AndTheVerdictIsReported | PASS | 없음 |
| EB02_TwoRunSets_KeepSeparateEvidence | PASS | 없음 |
| IB01_InitializeBackend_StartsANewRunSet | PASS | 없음 |
| IB02_InitializeAfterTheDllsAppear_RecordsMockThenNative | PASS | 없음 |

**Evidence:** `e2e.result.json` 시작 `2026-11-13T14:29:24.5276558+09:00`, 종료 `14:31:00.6913150+09:00`, exit0, HEAD `09d3cf6496875aa520d3c224f775449574a5eab0`, appAfter=[]。 `e2e.stdout.txt` 원문 `Test Run Successful. / Total tests: 4 / Passed: 4 / Total time: 1.5449 Minutes`; `e2e.trx` total4/executed4/passed4/failed0/aborted0, NotExecuted 결과 없음. 각 결과 이름도 승인한 네 개와 일치했다. 명령은 `dotnet test clients/ImageProcTest.E2ETests -c Debug --no-restore --filter "<승인된 네 FullyQualifiedName의 OR>" --logger "console;verbosity=detailed" --logger "trx;LogFileName=e2e.trx" --results-directory "D:\workspace-github\xpe-gui\.moai\reports\lane-gui\GUI-C-262"`; 축약하지 않은 실제 필터·명령은 `e2e.result.json` 및 `run-once.ps1`에 있다. 추가 회차·추가 build 명령은 실행하지 않았으며 dotnet test의 기본 build는 유지했다.

**환경 대조:** 이전 `GUI-C-261/ab-preflight.json`의 XPE 변수와 이번 `preflight.json` effectiveXpe + 실제 자식 `e2e.result.json` nativeEnvironment를 비교했다. 차이는 `XPE_GUI_DATA_DIR: D:\workspace-github\xpe-gui\clients\ImageProcTest.E2ETests\bin\Debug\net8.0-windows\gui-c-259-check → 없음` 하나였다. Native 변수3개는 같은 절대경로/exclusive1, 전역 입력 금지0도 같았다. 자식 ProcessStartInfo에 XPE_GUI_DATA_DIR가 있으면 실행 전 throw하도록 확인했다. 이 차이는 실행 자식 환경에만 적용했으며 사용자·시스템 환경을 변경하지 않았다.

**새 evidence:** 사전 `evidence-before.json`의 폴더 이름 목록과 실행 후 목록을 대조했다. 새 루트는 `D:/workspace-github/xpe-gui/gui/ImageProcTest/bin/Debug/net8.0-windows/evidence/`. 아래 7개 폴더가 추가됐으며 삭제하지 않았다.

| 새 폴더 | 현재 내용 | backend actual/requested |
|---|---|---|
| 20261113-143007-1727bc | backend.json, verdicts.json | Mock/Native |
| 20261113-143010-3d99d3 | backend.json, verdicts.json | Native/Native |
| 20261113-143014-8aee3d | backend.json, verdicts.json | Native/Native |
| 20261113-143018-50e2ef | backend.json, verdicts.json | Mock/Native |
| 20261113-143021-d72587 | backend.json, verdicts.json | Native/Native |
| 20261113-143024-10ab90 | backend.json, verdicts.json | Native/Native |
| 20261113-143029-a8a0e7 | runtime-log.txt | backend.json 없음 |

**설정 전후 공개:** 외부 앱 기본 루트 appsettings는 실행 전 Mock(아래 SHA), 실행 후 Native / SHA256 `2a4e31fb2ecd8ffd31c17b40045fdf0ffbd170dc340422c58580f46cc7b2848a`로 관측됐다. 에이전트는 해당 파일을 편집·복원하지 않았다. 제품에는 `_settingsService.Save(Settings)` 경로(`MainWindowViewModel.cs:296,2498,2548`)가 있고 승인된 시험 실행에 따른 파일 변화로 기록한다. 어느 저장 호출이 이 파일을 썼는지 추적 계측하지 않았다. 내부 실행기 기본 루트 설정은 실행 전후 모두 Mock / SHA256 `17c769a9b41a1d272bf84124db1bb8db3836cb3b4cb8e8a52a1179517d201651`로 같았다. 기존 공유 데이터 루트의 설정/evidence도 직접 편집하거나 삭제하지 않았다.

**Baseline-attribution / Gaps / Residual-risk:** 현재 트리·HEAD는 위 result에 기록된 값, 종료 후 `git diff --name-only -- gui clients` 출력 없음. 비교 기준은 이전 GUI-C-261 A/B이며 그 원문을 유지한다. 이번 네 시험의 단일 검증만으로 전체 suite 통과를 주장하지 않는다. 과거 12:33 내부 설정값·로드 DLL을 소급 관측한 것은 아니다. 별도 결함 후보 수정과 전체 main CI 검증은 아직 수행하지 않았고, 설정 수동 변경·evidence 삭제·제품/시험 수정·commit/push 없음. 리더/Reviewer 판정을 기다린다.

판정2가 이번 실행 1회를 허용했다. `GuiDataPaths.cs:12–15`에서 변수가 없으면 AppContext.BaseDirectory를 쓴다. 실행 전 실제 파일 조회: 외부 앱 기본 루트 `D:/workspace-github/xpe-gui/gui/ImageProcTest/bin/Debug/net8.0-windows`, appsettings.backendMode=Mock / SHA256 `b6c924de97bfd4ba55881ca5411c1678c54b95e291319f229306f66f15abd2ea`; 내부 실행기 루트 `D:/workspace-github/xpe-gui/gui/ImageProcTest.E2E/bin/Debug/net8.0-windows`, backendMode=Mock / SHA256 `17c769a9b41a1d272bf84124db1bb8db3836cb3b4cb8e8a52a1179517d201651`。 파일을 고치지 않았다. 이전 A/B의 B와 제품·시험 소스는 같고 HEAD는 문서 커밋 `09d3cf64`다. 도우미는 기존 실행 조건에서 XPE_GUI_DATA_DIR만 제거하며, 기존 Native 변수 3개·전역 입력 금지를 유지한다. 필터는 승인된 4개만, 추가 회차 없음. 결과 원문은 이 카드의 e2e stdout/stderr/TRX/result에 보존한다.

ACK 착수 14:24 KST. 설정·evidence·환경·제품·시험은 변경하지 않고 기존 코드·실패 로그와 현재 파일 상태를 조회했다. 시험 실행·임시 폴더 생성·commit/push 없음. 이 보고서만 작성했다. 과거 내부 프로세스 상태와 현재 파일 관측을 구분한다.

## Claim

| 시험 | 코드·관측값 | 판단 | 제안만, 적용하지 않음 |
|---|---|---|---|
| A06 | 내부 `gui/ImageProcTest.E2E/Program.cs:81`은 인수 없이 MainWindow 생성, `:219`는 전처리 메뉴 비활성을 단언한다. 비automation 설정은 `MainWindow.xaml.cs:155–161`, 실제 기본 설정 경로는 `AppSettingsService.cs:45` → `GuiDataPaths.cs:12–15`. 현재 공유 루트 `appsettings.json`의 backendMode는 Native. | **사실:** 설정·환경에 따라 Native 선택이 가능한 경로와 현재 파일 값은 확인. **추정:** 실패 당시 내부 앱이 이 설정으로 Real을 선택하여 메뉴가 켜졌을 가능성이 높다. 그 순간의 설정값·ActualBackendMode·로드 모듈은 미관측이므로 과거 원인 확정은 하지 않는다. | 내부 runner가 Mock을 검증하는 것인지 실제 선택 백엔드를 검증하는 것인지 먼저 계약을 정한다. Mock 의도라면 runner 전용 설정·명시적 Mock 선택을 격리하고 원래 설정을 건드리지 않는다. 실제 백엔드 의도라면 단언을 backend capability 계약과 맞추는 방안을 검토한다. 단언 완화·설정 변경은 별도 승인 필요. |
| EB02 | `EvidenceBundleScenarios.cs:198,209–210`은 EXE 옆 evidence/runId를 구성, `:203–204`에서 backend.json 확인. 앱 writer는 `MainWindowViewModel.cs:3819,3839`에서 **GuiDataPaths.Root/evidence/runId**에 쓴다. 실패 id `20261113-133613-2b51c0`는 기대 경로에 없고 공유 루트에 backend.json·verdicts.json이 존재. | **확인된 직접 실패 조건:** 기록은 있으나 시험 조회 루트가 다르다. 해당 id의 actual/requested는 Native/Native. backend.json 쓰기 자체가 누락됐다는 설명은 현재 관측과 맞지 않는다. | 시험이 앱의 실제 데이터 루트를 사용하도록 계약을 맞춘다. XPE_GUI_DATA_DIR를 지원하는 동일 규칙/주입값으로 EvidenceRoot를 계산하는 최소 변경을 검토한다. 기존 evidence 삭제로 해결하지 않는다. |
| IB01 | `EvidenceBundleScenarios.cs:114`는 before/after EvidenceDirectory 존재를 확인. 기존 stdout:2202의 id `20261113-133621-5b5d9b`·`20261113-133623-59c327`는 EXE 옆에 없고 공유 루트에 각각 backend.json·verdicts.json 존재. | **확인된 직접 실패 조건:** 두 id 모두 올바른 저장 루트에 기록됐고 시험이 다른 루트를 검사한다. 각 actual/requested Native/Native. | EB02와 같은 EvidenceRoot 계약 수정안을 검토. runId 생성·기록 여부를 바꾸거나 기존 폴더를 삭제할 근거는 없다. |
| IB02 | `EvidenceBundleScenarios.cs:148–152,203–210`의 ReadBackend는 같은 EXE 루트 사용. id `20261113-133606-cd8ca7`의 실제 backend.json은 공유 루트에 존재, actual/requested Mock/Native. | **확인된 직접 실패 조건:** 기대 루트에는 없고 실제 데이터 루트에는 있다. 현재 발견한 첫 run의 Mock/Native 기록은 시험 의도와 일치한다. 이후 단언까지 통과할지는 이번에 실행하지 않았으므로 미확인. | EB02와 같은 루트 계약 수정안을 검토한다. Mock→Native 전환 동작 수정은 이번 자료만으로 정당화하지 않는다. |

## Evidence — 기존 기록과 현재 읽기 조회

조회 방법: `ctx_execute`의 Node `fs.readFileSync/statSync/readdirSync`로 아래 특정 경로와 관련 코드 구간을 읽고, JSON의 backendMode·actualBackendMode·requestedBackendMode·writtenAt만 추출했다. DLL은 읽기 SHA256만 계산했으며 로드/실행/exports 검사는 하지 않았다. 새 제품·시험 프로세스는 만들지 않았다.

표의 코드 경로 접두사: 앱은 `D:/workspace-github/xpe-gui/gui/ImageProcTest/`, 시험은 `D:/workspace-github/xpe-gui/clients/ImageProcTest.E2ETests/Scenarios/Workflows/`이다.

### 현재 설정·Native 조건 및 환경 상속

- 기록된 부모 환경: `GUI-C-261/ab-preflight.json`에 `XPE_GUI_DATA_DIR=D:\workspace-github\xpe-gui\clients\ImageProcTest.E2ETests\bin\Debug\net8.0-windows\gui-c-259-check`, `XPE_NATIVE_DIR=D:\workspace-github\image-processing\build\e2e-native-dlls`, `XPE_NATIVE_DIR_EXCLUSIVE=1`, `XPE_C231_NATIVE_DIR`는 같은 DLL 폴더. 이는 기존 실행 시작 기록이며 현재 실행 중 자식의 환경을 덤프한 것은 아니다.
- `ApplicationFixture.cs:146–152,193–201`은 앱 인수에 backend를 넣고 Native 경로를 전달한다. A06 자체는 `AutomationReportBackendTests.cs:182`에서 outer app을 Mock으로 지정한다. 그러나 `MainWindowViewModel.cs:2174–2175,2322` → `RunnerProcess.cs:48–70`은 내부 실행기 인수나 환경을 별도로 Mock으로 고정하지 않는다. ProcessStartInfo의 환경을 교체/삭제하지 않으므로 Native 변수와 데이터 루트는 상속되는 코드 경로다. outer app의 Mock 인수는 내부 새 프로세스 인수가 아니다.
- 내부 `Program.cs:81`의 MainWindow는 `MainWindow.xaml.cs:157–161` 비automation 경로로 설정을 읽는다. `GuiDataPaths.cs:12–15`는 XPE_GUI_DATA_DIR가 있으면 그 절대경로, 없으면 AppContext.BaseDirectory. `AppSettingsService.cs:45`는 그 루트/appsettings.json을 사용한다.
- 현재 `.../gui-c-259-check/appsettings.json`: backendMode=Native, rawWidth=1024, rawHeight=1024; 파일 mtime `2026-11-13T14:13:23.014+09:00`, 2622 bytes, SHA256 `5ac97f3e36cfe273ba58d0809614fec344d88ff8cc11549671d7d8f7b05bd4c3`. **이 수정시각은 12:33 실패보다 뒤이므로 현재 값이 당시에도 같았다고 주장하지 않는다.**
- 비교 파일 `gui/ImageProcTest/bin/Debug/net8.0-windows/appsettings.json`은 현재 Mock, 1024×1024. 저장소 `gui/ImageProcTest/appsettings.json:2`도 Mock이며 csproj:88–89가 PreserveNewest로 출력에 복사한다. 설정 파일이 없으면 `AppSettingsService.cs:74–76`은 새 AppSettings, `Models/AppSettings.cs:24` 기본값 Mock.
- `XpeBackendFactory.cs:20–43`은 BackendMode=Native와 common/display 사용 가능 조건에 따라 Real을 선택하고, 아니면 Mock을 반환한다. `RealXpeBackend.cs:57–59`의 가용성 검사는 로드·exports 확인을 포함하지만 **이번 조사에서는 실행하지 않았다**. 현재 DLL 파일이 존재하는 사실만 읽었다: common 526848 bytes / SHA256 `c47ed489e981b466a51a54335323454290c243489225014499ad479ff3022dcd`, display 80384 bytes / SHA256 `c7d6dba80ed408f5f962effe7b7dd2214846eb1e9b3e1fa24a3e818719a46719`.
- `MainWindow.xaml:349–353`의 IsEnabled는 CanRunPreprocessing, `MainWindowViewModel.cs:835`는 _backend.SupportsPreprocessing, Real:325=true / Mock:349=false. 따라서 현재 Native 설정과 로드 가능한 DLL이라는 조건이면 메뉴가 켜지는 경로가 있다. 12:33 실제 내부 DLL 가용성 검사는 미관측이다.

### backend.json writer와 실제 폴더

`MainWindowViewModel.RecordVerdict:3801–3817`은 현재 loaded study가 없거나 RunId가 비면 반환하여 기록하지 않는다. `:3819–3820`에서 데이터 루트/evidence/runId 생성, `:3839–3848`에서 backend.json 기록, `:3850–3852`에서 예외를 로그로 남긴다. 이번 3건에서는 같은 runId의 backend.json이 실제 루트에 존재하므로 이 가드/쓰기 실패를 직접 원인으로 볼 근거는 없다.

아래 `실제 루트`는 `D:/workspace-github/xpe-gui/clients/ImageProcTest.E2ETests/bin/Debug/net8.0-windows/gui-c-259-check/evidence/`, `시험 루트`는 `D:/workspace-github/xpe-gui/gui/ImageProcTest/bin/Debug/net8.0-windows/evidence/`이다.

| 회차·시험 | runId | 시험 루트 | 실제 루트 내용 | backend actual/requested · writtenAt(KST) |
|---|---|---|---|---|
| 재개4 EB02 | 20261113-133613-2b51c0 | 폴더 없음 | backend.json, verdicts.json | Native/Native · 13:36:14.233 |
| 재개4 IB02 | 20261113-133606-cd8ca7 | 폴더 없음 | backend.json, verdicts.json | Mock/Native · 13:36:07.384 |
| 재개4 IB01 before | 20261113-133621-5b5d9b | 폴더 없음 | backend.json, verdicts.json | Native/Native · 13:36:21.706 |
| 재개4 IB01 after | 20261113-133623-59c327 | 폴더 없음 | backend.json, verdicts.json | Native/Native · 13:36:25.728 |
| 부모 A1 EB02 | 20261113-140826-8bc0c2 | 부모 폴더는 기존 A/B 상태 기록에서 없음 | backend.json, verdicts.json | Native/Native · 14:08:27.228 |
| 부모 A1 IB02 | 20261113-140819-de1334 | 위와 같음 | backend.json, verdicts.json | Mock/Native · 14:08:20.143 |
| 부모 A2 IB02 | 20261113-141133-f82edb | 위와 같음 | backend.json, verdicts.json | Mock/Native · 14:11:33.769 |
| 부모 A2 EB02 | 20261113-141139-859e1a | 위와 같음 | backend.json, verdicts.json | Native/Native · 14:11:40.713 |

부모 worktree는 GUI-C-261에서 이미 제거됐고 새로 만들지 않았다. 부모 A1/A2의 환경도 동일한 B 공유 데이터 루트를 가리켰다는 기존 preflight와 현재 같은 runId의 파일이 일치한다. 따라서 부모 앱 폴더에 evidence가 없었던 것과 기록이 공유 데이터 루트에 존재하는 것은 양립한다. B의 오래된 1302개 evidence 폴더가 이 오류를 만들었다는 근거는 없다. 권한 실패도 이 runId들에는 관측되지 않았다.

### CI 대조의 한계

기존 CI 로그 `image-processing/.moai/reports/lead/COV-20261112/job-131200334499.log`에서 IB02:2754, EB02:2764, IB01:2772, A06:3141 PASS를 이번에 직접 읽었다. CI는 다른 HEAD(a617a8e5)다. 현재 `.github/workflows/ci.yml:1281–1288` Native 단계에는 Native DLL 경로와 전역 입력 opt-in이 있고 XPE_GUI_DATA_DIR 지정은 이 단계에 없다. **CI 당시의 실제 전체 환경·설정 파일을 수집한 것은 아니다.** 코드 기본 경로에서 Mock 설정/기본값을 읽고, writer와 시험의 evidence 경로가 모두 앱 폴더로 맞는 조건이면 이번 로컬 차이를 설명할 수 있지만, CI 실제 상태로 확정하지 않는다.

## Baseline-attribution

현재 트리 `D:/workspace-github/xpe-gui`, 읽기 조회 때 HEAD `09d3cf6496875aa520d3c224f775449574a5eab0`. 제품 코드는 e61be663과 같고 마지막 커밋은 GUI-C-261 문서 3개다. 실패 재실행·DLL 로드·앱 실행 없이 기존 GUI-C-261 결과와 현재 파일만 조회했다. `GUI-C-261/report.md`의 커밋 후 SHA 안내 줄과 기존 다른 변경은 보존한다.

## Gaps / Residual-risk

A06 과거 내부 프로세스는 종료돼 당시 설정값·환경 실측·ActualBackendMode·로드 DLL은 미확인이다. 현재 설정과 코드 상속만으로 과거 원인을 확정하지 않는다. evidence 경로 불일치는 같은 runId의 실제 파일로 확인했지만, 제안 변경 후 4개가 모두 통과할지는 시험하지 않았다. 이 보고서는 수정·설정 변경·실행 재개의 승인이 아니며 추가 작업은 리더/Reviewer 판정을 기다린다.
