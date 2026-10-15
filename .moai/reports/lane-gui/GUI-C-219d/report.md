# GUI-C-219d — Codex #116 보류 4건과 위협 범위 (#249)

대상: `clients/ImageProcTest`(레거시 앱)의 합성 오라클 경로. 219·219b·219c 위에 쌓은 변경이다. gui 앱(`gui/ImageProcTest`)은 이 소스를 링크하지 않고 `NativePreprocessPreviewService`를 주석에서만 언급하므로(grep 확인) 이 카드의 범위 밖이다.

## 위협 범위 (리더 결정, 코드 주석에도 같은 문장을 적음)

| 구분 | 내용 | 이 카드의 처리 |
|---|---|---|
| 범위 안 | 앱이 켜진 동안 DLL이 재빌드·재배포로 우발적으로 바뀌는 경우 | 실행 직전 확인(항목 1), 의존 DLL 복사본 로드와 로드 모듈 감사(항목 2)로 막는다 |
| 범위 밖 | 검사와 로드 사이 수 ms를 노린 의도적 교체(A→B→A) | 막지 않는다. 앱 폴더의 DLL을 바꿀 수 있는 사람은 이 앱이 하는 일을 이미 다 할 수 있으므로 이 앱은 그 사용자에 대한 보안 경계가 아니다 |

**한계로 남는 것**: 실행 직전 확인과 명령이 DLL을 여는 시점 사이에는 수 ms의 틈이 있다. 이 틈을 노린 교체는 잡지 못한다(`ProcessingContentGate`, `PreprocessOracleVerdicts.ConfirmContentAsync`, `OracleModuleConfinement`의 주석에 같은 내용).

## 항목 1 (높음) — 처리 실행 직전 확인

**변경**: `ProcessingContentGate.ConfirmAsync(dllPath)`가 풀 스레드에서 현재 원본 파일들의 정체성(이름+SHA-256 목록, `PreprocessOracleSnapshot.IdentityOfOriginals`)을 계산해 저장된 판정의 정체성과 비교한다. 같으면 true. 다르거나 판정이 없거나 파일을 못 읽으면 false를 돌려주고 검증을 다시 시작한다(그래서 화면은 스스로 "확인 중"으로 간다). 창은 `await`로 기다리므로 UI는 막히지 않는다. `TryGet`이 옛 통과를 즉시 돌려주는 것은 표시용으로 남겼고, 처리를 켜는 근거는 이 확인뿐이다.

**처리 명령 진입점 전체 목록** (`NativePreprocessPreviewService.Run(` 호출 파일 전수 검색, 3개뿐):

| 진입점 | 위치 | 확인 방식 |
|---|---|---|
| Apply Native Preview 버튼 | `MainWindow.xaml.cs` `ApplyNativePreviewButton_Click` | `await ConfirmProcessingContentAsync()` 뒤에 `ApplyNativePreview()` |
| 검증 행 실행(교정·후처리·표시·DICOM 포함 모든 행) | `RunSelectedAlgorithmValidationButton_Click` | 같은 확인 뒤에 `RunSelectedAlgorithmValidation()` |
| 알고리즘 체인 실행(Workflow Run) | `WorkflowRunButton_Click` | 같은 확인 뒤에 `RunWorkflow()` |
| 헤드리스 `--run-preprocess-fixture-e2e` | `Services/PreprocessFixtureE2eService.cs` | `PreprocessOracleVerdicts.IsCurrent(...)`가 false면 `ContentChanged`로 막힌 실행을 반환 |
| 헤드리스 `--run-phase1b-fixture-e2e` | `Services/Phase1bFixtureE2eService.cs` | 같음 |

창 안에서 서비스를 여는 곳은 `RunNativePreprocessPreview` 한 곳이고(정규식으로 1건 단언), 그곳에는 위 세 핸들러의 본체만 닿는다.
참고: 확인은 전처리 단계가 없는 실행(우회 미리보기)에도 걸린다. DLL이 바뀐 직후에는 그 실행도 한 번 거절된다. 단순함을 택했고 영향은 "다시 누르기"뿐이다.

**시험** (`PreprocessOracleVerdicts219dTests`):
- `AProcessingCommand_DoesNotRun_WhenTheOriginalChangedAfterTheVerdict_…`: 통과 판정 뒤 원본 내용을 바꾸고(크기·시각 보존) 확인 → false, 새 내용이 판정을 받은 뒤 → true.
- `TheCheck_SaysNo_ForADependencyThatChanged_ADllThatIsGone_…`: 의존 DLL 변경, DLL 삭제, 판정 없음 → 각각 false.
- `TheCheck_IsNotMadeOnTheCallersThread_…`: "UI 스레드"로 지정한 전용 스레드에서 호출해도 해시는 풀 스레드에서 돈다.
- `EveryEntryPointThatCanRunTheNativePreprocessDll_AsksTheCheckFirst`: 세 핸들러가 확인을 기다린 뒤에만 본체를 부르고, 서비스를 호출하는 파일이 위 세 곳뿐이며 헤드리스 둘이 `IsCurrent`를 묻는지 단언한다. 새 호출자가 생기면 이 시험이 빨개진다.

**미검증**: 실제 앱에서 버튼을 눌러 "DLL을 바꾼 뒤 거절된다"를 보는 UI 시험은 만들지 않았다(E2E에 교정 폴더와 네이티브 DLL을 준비하는 비용이 크다). 확인 로직은 통합 시험으로, 핸들러 연결은 소스 스캔으로 고정했다. 이 둘 사이의 연결(핸들러가 실제로 거절 문구를 띄우는지)은 눈으로도 실행으로도 보지 않았다.

## 항목 2 (높음) — 의존 DLL을 스냅샷 폴더에서만 로드하고 로드된 모듈을 열거해 단언

**재현 먼저**: Codex의 주장(전체 경로로 메인 DLL을 로드해도 의존 DLL은 이름으로 다시 검색되고 실행 파일 폴더가 먼저다)을 그대로 재현했다. 실행 파일 옆에 `xpe_common.dll`을 두고 스냅샷 폴더에 같은 세트를 둔 뒤, 제한 없이 로드하면 워커가 `app\xpe_common.dll`(스냅샷이 아니라 실행 파일 폴더 것)을 로드했다(`falsification_2_plain_load.txt`).

**방식과 이유**: `OracleModuleConfinement.TryLoad`는 `LoadLibraryExW(path, 0, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32)`로 로드한다. 의존 DLL을 "로드하는 DLL의 폴더, 그다음 System32"에서만 찾고 실행 파일 폴더·현재 디렉터리·PATH는 보지 않는다. 호출 단위 제한을 골랐고 `SetDefaultDllDirectories`는 쓰지 않았다. 후자는 프로세스 전체에 걸려 되돌릴 수 없고, 워커가 나중에 여는 다른 모듈의 검색까지 바꾸기 때문이다.

**열거와 단언** (워커 안): 오라클이 끝난 뒤 `Process.GetCurrentProcess().Modules`를 열거해, 이름이 스냅샷 폴더의 파일과 같은 모듈이 모두 그 폴더 아래에서 왔는지 본다(`OracleModuleConfinement.Offenders`, 폴더 비교는 경로 구간 단위로 하고 대소문자를 구분하지 않는다). 하나라도 밖에서 왔으면 판정은 `Synthetic oracle module audit failed`이고 경로를 담는다. 이 단언이 지난 보고서의 한계("로드된 모듈을 열거해 확인하지 않음")를 닫는다.

**Codex가 덧붙인 것**: 찾은 의존 DLL이 복사 중 사라지면 `CopyShared`가 false를 돌려주고 더 작은 세트로 계속 판정하던 경로를 닫았다. 이제 예외를 던져 설정 실패 판정이 된다(실패 닫힘). **이 경로에는 시험이 없다.** 복사 직전에 파일이 사라지는 순간을 결정적으로 만드는 이음매가 없어서 코드만 고쳤고 실행으로는 보지 않았다.

**시험**:
- 통합 `TheAudit_NamesEveryLoadedModuleThatSharesANameWithASnapshotFile…`: 순수 함수로 미끼(같은 이름, 다른 폴더), 접두 일치 형제 폴더, 대소문자, 시스템 모듈 무시를 단언.
- E2E `LegacyOracleConfinementScenarios.TheWorker_LoadsItsDependenciesFromTheSnapshotFolder_…`: 레거시 앱 복사본의 실행 파일 옆에 네이티브 DLL을 미끼로 두고 스냅샷은 별도 폴더에 둔 채 **실제 워커**를 앱이 시작하는 방식으로 실행한다. 판정이 통과해야 한다. `XPE_NATIVE_DIR`가 없으면 건너뛴다.
- 통합 `TheWorkerLoadsTheDllConfined_AndAuditsTheModulesItLoaded`: 워커와 오라클이 제한 로드와 감사를 호출하는 연결을 소스에서 확인.

**남는 한계**: (a) 감사는 스냅샷에 들어 있는 이름만 본다. 실행 중 늦게(지연 import, 동적 `LoadLibrary`) 불러오는데 스냅샷에 없는 이름의 DLL은 열거 대상이 아니다. System32 것은 시스템 모듈로 인정한다. (b) PE import 읽기는 import 디렉터리 1번만 읽으므로 지연 import는 스냅샷에 안 담길 수 있다. 담기지 않은 이름이 로드되면 위 (a)와 같다. (c) 위협 범위 결정에 따라 의도적 교체는 대상이 아니다.

## 항목 3 (보통) — 재실행 요청 삼킴

**원인**: "다른 패스가 필요한가" 검사와 `JobQueued=false` 전환이 서로 다른 잠금이었다. 둘 사이에 `TryGet`이 오면 "작업 중이고 스냅샷은 찍혔다"를 보고 `Rerun`만 세운 뒤 새 작업을 만들지 않았고, 곧이어 정리 블록이 그 요청을 지웠다.

**변경**: 결정과 해제를 한 `Gate` 임계구역으로 합쳤다(`Rerun`이면 한 번 더, 아니면 같은 잠금 안에서 `JobQueued=false`). 해제된 뒤에 온 질문은 작업이 없음을 보고 자기 작업을 시작한다. 정리 블록은 해제하지 못한 경로(예외)에서만 플래그를 지운다. 시험용 이음매 두 개(`BeforeRerunDecision`, `AfterRerunDecision`)를 두었다.

**시험(결정적, 게이트로 틈에 작업 스레드를 멈춤)**: `AnAskJustBeforeTheJobDecidesItIsOver_GetsAnotherPass`, `AnAskJustAfterTheJobDecidedItIsOver_StartsAJobOfItsOwn`(Codex 재현과 같은 순서).

## 항목 4 (낮음) — UI 스레드 가드

**변경**: 가드를 창이 아니라 `App.Application_Startup`의 대화형 경로에서, 창을 만들기 전에 설치한다(`() => Dispatcher.CheckAccess()`, 호출 때마다 판정). 창은 설치도 해제도 하지 않는다. 헤드리스 모드(`--probe-native-readiness` 등)는 같은 디스패처 스레드에서 오라클을 일부러 기다리므로 설치하지 않는다.

**시험**: `TheGuard_IsInstalledByTheApplicationNotTheWindow_AndDecidesAtCallTime` — `App.xaml.cs`가 창 생성 전에 설치하는지, `MainWindow`가 가드를 건드리지 않는지 소스로 단언하고, 가드를 먼저 설치한 뒤 나중에 UI 스레드가 되는 스레드(창 없는 진입점)에서 `Wait`가 던지는지 실행으로 본다. 실제 WPF 디스패처로 한 시험은 아니다(통합 시험 프로젝트는 WPF를 참조하지 않는다).

## 반증 (항목마다, 결과는 `falsification_*.txt`)

| 항목 | 일부러 넣은 결함 | 결과 |
|---|---|---|
| 1 | 실행 직전 확인이 항상 true | 빨강 2건(`AProcessingCommand_…`, `TheCheck_SaysNo_…`) |
| 1b | 핸들러 하나(`WorkflowRunButton_Click`)에서 확인 호출 제거 | 빨강 1건(진입점 스캔) |
| 2 | 제한 없는 로드(플래그 0) | E2E 빨강: `Loaded from outside the snapshot: …\app\xpe_common.dll` |
| 3a | 결정 직전 요청 무시(`another=false`) | 빨강 2건(신규 1, 219c의 `AnAskThatComesAfterTheRunningJobsSnapshot_…` 1) |
| 3b | 옛 구조 복원(해제를 정리 블록으로 미룸) | 빨강 1건(`AnAskJustAfter…`) |
| 4 | 앱 시작에서 가드 설치 제거 | 빨강 1건 |

모든 반증 뒤 원본을 복원하고 다시 돌려 통과를 확인했다.

## 시작 응답 수치 (219c 수준 유지 확인)

219c 커밋(`9368f8cd`, 별도 워크트리에서 빌드)과 이번 트리를 12라운드 번갈아 실행(`start_response_c_vs_d.txt`; 스크립트는 219c 폴더의 `measure_start_launch_relative.py.txt`를 그대로 사용).

| | 창 핸들·보이기·ready ms (12회 중앙값) | 보이기 ms | ready ms | 가장 긴 무응답 ms |
|---|---|---|---|---|
| 219c | 102 | 454 | 662 | 43~46 |
| 219d | 102 | 454 | 670 | 42~46 |

가장 긴 무응답은 12회 모두 42~46 ms로 같다(219 이전 중앙값 963 ms, 최대 4.4 s). 이번 변경은 시작 경로에 새 UI 스레드 작업을 넣지 않았다.

## 실행 증거

- 통합 시험 전체: 848 통과 / 0 실패 / 0 건너뜀 (219c 839 + 이번 9건) — `integration_full_suite.txt`
- 레거시 E2E 전체(네이티브 DLL 있음): 13 통과 — `legacy_e2e_current_tree.txt`
- `PreprocessOracleVerdict*` 30건 통과.
- 같은 환경에서 `ImageProcTest` 앱 빌드 경고 0·오류 0.

## 미검증과 잔여 위험

- 실제 버튼 클릭 → 거절 문구까지의 UI 경로를 실행하지 않았다(위 항목 1).
- 복사 중 의존 DLL 소실의 실패 닫힘은 시험이 없다(항목 2).
- CI(CRLF 체크아웃, Mock 구성) 실행은 아직 없다. 로컬은 전부 빌드된 가장 관대한 구성이다.
- 확인과 DLL 열기 사이 수 ms의 틈은 위협 범위 결정에 따라 남아 있다.
- 같은 PID가 남긴 스냅샷 폴더는 프로세스 종료 전까지 다시 시도하지 않는다는 Codex의 지적은 이번에 건드리지 않았다(보고서 12회 측정에서 잔여 0건).
- 이 카드로 오라클 호출 경로가 바뀌어(워커가 제한 로드를 쓴다) QA-B 쪽 네이티브 DLL의 import 구성이 바뀌면 감사가 실패 판정을 낼 수 있다. 의도한 동작이다.
