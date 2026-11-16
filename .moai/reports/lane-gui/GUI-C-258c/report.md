[2026-11-04 11:06:36 KST] GUI-C-258c · Refs #282 · 구현·로컬 검증 완료: 전체 SelfCheck 33개, 계약 1/6/3개, 영향 통합 13개·Mock UIA 1개 통과 · Reviewer 기술수용, 미푸시

## 수신·착수

- 카드: `D:/workspace-github/image-processing/.moai/state/codex-msgs/20261104-0940-gui.md` (리더 표기 09:38 KST).
- 목표: 기존 SelfCheck oracle을 바꾸지 않고 시나리오 7·13 회귀를 고친 뒤 필터 없는 전체 suite 및 GUI-C-256~258 계약 시나리오를 확인한다.
- 부모 레인이 로컬 `main`이 오래된 `1e56f3b2`임을 확인하고, `origin/main`의 공백 정리 커밋 `4ef55176bf17bfffdb9a0cc4f612fc374c4ca1e0`을 `dev/gui`에 fast-forward했다. 본 조사 baseline은 이 HEAD다. 다른 세션의 설정·문서 변경은 보존했다.
- `AGENTS.local.md`는 없었다. `moai-workflow-tdd`를 읽고 기존 실패 시나리오로 RED를 먼저 관찰했다.
- 09:43:49 조사 보고 시점에는 구현 승인 회신이 없어 소스 수정 없이 보류했다. 이후 `20261104-1103-gui.md`(리더 표기 11:00 KST)에서 동일 리더가 계속 담당하며 이전 종료 통보를 정정했고, 두 파일의 제안 수정을 명시 승인했다. 해당 승인 뒤에만 구현·후속 시험을 재개했다.

## Claim

전체 SelfCheck RED에서 카드의 두 실패를 재현한 뒤 승인된 최소 수정을 적용했다. 13은 B 상태를 immutable record에 함께 저장하고 getter를 record 그대로 읽도록 복원했다. 7은 실제 main `None` 처리를 유지하면서 초기 A/B 선택 이름 두 줄을 `No grid correction`으로 맞췄다. 최종 필터 없는 전체 SelfCheck 33개 및 별도 계약 1/6/3개가 exit 0으로 통과했다. 기존 oracle은 바꾸지 않았으며 기본명 변경으로 영향을 받은 14의 refused-grid fixture만 명시 설정했다.

## Evidence

실행 명령(두 번 동일, 필터 없음):

```text
D:\workspace-github\xpe-gui\.moai\reports\lane-gui\GUI-C-254c\env.cmd C:\Progra~1\dotnet\dotnet.exe run --project gui/ImageProcTest.SelfCheck -c Debug
```

두 번째 실행의 전체 stdout/stderr는 [red.txt](red.txt)에 보존했다. 저장할 때 각 줄 끝 공백만 제거했으며, 출력 문자열·순서·숫자는 유지했다. 종료 코드: `-532462766` (unhandled exception).

```text
Unhandled exception. System.InvalidOperationException: Lifetime scenarios failed:
[13 event x field table] A's HUD/status properties are not the record's
[7 older status read never shown] timed out waiting for: the newest status to be shown
[7 older status read never shown] the screen showed 1 change(s) [worker=Active; failures=3; ceiling=3]; expected exactly the newest answer, never the older one
```

첫 실행에서도 같은 세 failure 메시지와 종료 코드가 관찰됐다. 첫 실행 빌드 출력에는 CS1573·CS1574·CA2014·CS8604 경고가 있었다. 두 번째 실행은 증분 빌드여서 빌드 경고가 출력되지 않았다. 이를 경고 해소로 해석하지 않는다.

### 원인과 승인된 수정(행 번호는 수정 전 baseline)

| 항목 | 실제 코드 근거 | 전후 요구 및 제안 |
|---|---|---|
| 13: record 동등성 | `gui/ImageProcTest.SelfCheck/LifetimeScenarios.cs:2156`은 `RenderedVoiMode`, `ChainStatus`, `DisplayPipelineSummary`가 record와 같아야 한다고 단언한다. `gui/ImageProcTest/ViewModels/MainWindowViewModel.cs:812-814`의 `ChainStatus` getter가 record의 문자열에 B summary를 추가하지만, `:3052-3063` record 생성은 main-chain 문자열만 저장한다. | getter를 record 그대로 읽도록 복원하고 `RenderLanesAsync`의 B 성공 `:4277` / 재사용 `:4281` commit에서 B facts와 B summary를 record에 함께 저장하는 안. `UpdateRender :1355-1360`의 render ID guard와 기존 알림 `:1366-1368`을 유지한다. |
| 13: B 표시 보존 | `LifetimeScenarios.cs:732`는 기존 GUI-C-257 계약으로 `vm.ChainStatus`에 B outcome/reason이 있어야 한다고 단언한다. | B를 별도 UI 문자열로만 옮겨 단언을 무력화하지 않는다. immutable record와 getter 모두 B를 포함해야 두 기존 oracle을 함께 유지할 수 있다. |
| 7: 기본 설정 불일치 | `gui/ImageProcTest/Models/AppSettings.cs:58-59`의 A/B 기본 이름은 `Grid suppression`, `:73`의 실제 main grid는 `GsvgModes.None`이다. `RenderLanesAsync :4250-4256`이 Candidate preset을 적용한 사본과 실제 main 설정을 비교하여 다르다고 판단하고 `:4270`에서 추가 chain을 실행한다. | 실제 main `None` 처리를 바꾸지 않고 초기 picker 이름 두 줄만 `No grid correction`으로 맞추는 안. `GsvgMode` 기본값을 GridSuppression으로 바꾸는 안은 실제 기본 처리를 바꾸므로 제외했다. 기존 저장값 migration/덮어쓰기 및 빈값 fallback 변경은 제안 범위 밖이다. |
| 7: 최신 응답 oracle | `LifetimeScenarios.cs:1701`은 기본 설정과 B width 0으로 VM을 만들며, `:1738`은 failures=2 최신 응답을 기다리고 `:1741`은 단 한 번의 `worker=Active; failures=2; ceiling=3` 알림을 요구한다. `HeldReadBackend.GetAiWorkerStatus :340-347`은 AI 호출 수를 ceiling 3으로 제한해 반환한다. | 관찰 출력은 failures=3이며, 기본 설정 mismatch의 별도 B chain 경로가 추가 AI 호출을 만든다. 상태 refresher의 오래된 응답 폐기 oracle이나 ceiling을 바꾸지 않는다. |
| 7: 명시적 mismatch 유지 | `LifetimeScenarios.cs:761-767`은 picker 이름이 같더라도 저장된 main mode가 다르면 B preset을 실제 적용해야 한다고 단언한다. | effective mismatch 조건을 제거하는 우회는 하지 않는다. 기본 이름 정합화 후에도 이 명시적 mismatch 경로는 그대로 보존해야 한다. |

`LoadAndDrawLanes :915-923`은 B 이미지와 UI flush를 기다린 뒤 돌아오며, 13의 `recordA :2152`는 그 뒤에 얻는다. B summary를 B commit에 포함하는 수정으로 기존 record 동등성과 B 표시 단언이 함께 통과했다.

### 구현 후 재현과 GREEN

- 수정 파일: `AppSettings.cs` 초기 선택명 두 줄, `MainWindowViewModel.cs` getter 및 B commit 두 분기. 실제 main grid 기본값, Candidate effective mismatch 판정, 저장값 migration, 빈값 fallback, AI 상태 refresher는 변경하지 않았다.
- 이 두 파일 수정 후 전체 실행에서는 7·13 실패가 사라졌지만 14가 실패했다. 기존 14는 초기 설정이 grid suppression이라는 전제에서 refused-grid tag의 `(not applied)`를 단언했다. 원문은 [intermediate-default-fixture.txt](intermediate-default-fixture.txt)에 보존했다. 종료 코드 `-532462766`.
- 부모에게 영향 범위를 알린 후 `LifetimeScenarios.cs`의 14 setup에 A/B `Grid suppression` 선택 두 줄만 추가했다. 기존 `Check`·expected 문자열을 그대로 유지하여 실제 refused-grid 경로를 계속 검사한다. 다른 SelfCheck oracle은 변경하지 않았다.
- 최종 필터 없는 명령은 RED와 동일하다. [green-full.txt](green-full.txt): `Lifetime scenarios passed (33 scenarios).`, `GUI-S0 self-check passed.`, exit `0`. 7·13·14를 포함한 전체 기존 시나리오와 256~258의 전체-run 포함 계약이 통과했다.
- 별도 계약 명령과 원문은 [green-contracts.txt](green-contracts.txt)에 있다. `dotnet run --project gui/ImageProcTest.SelfCheck -c Debug --no-build -- --study-contract` 1개, `--lane-contract` 6개, `--display-contract` 3개 모두 exit `0`.
- 계약 첫 실행에서도 각 통과 출력이 있었으나 orchestration의 `undefined session_id` 저장 오류로 exit 결과 저장이 중단됐다. exit까지 관찰하기 위해 같은 세 명령을 한 번 더 실행했고, 파일에는 두 번째 실행 결과를 기록했다. 이를 별도 시험 수로 중복 집계하지 않는다.
- 최종 build 출력에는 기존 CS8604 경고가 있었다. 중간 전체 build에는 기존 CS1573·CS1574·CA2014 경고도 있었다. 경고를 끄거나 수정하지 않았다.
- 자체 source diff 검사 `git diff --check -- gui/ImageProcTest/Models/AppSettings.cs gui/ImageProcTest/ViewModels/MainWindowViewModel.cs gui/ImageProcTest.SelfCheck/LifetimeScenarios.cs .moai/reports/lane-gui/GUI-C-258c`: 출력 없음, exit `0`. 최종 staged 검사와 커밋은 부모 레인 담당이다.

### 기본 이름 변경 영향 검사(부모 레인 실행)

- `git grep`으로 E2E와 통합 시험의 `Grid suppression`, `No grid correction`, `LaneAAlgorithm`, `LaneBAlgorithm` 의존을 대조했다. `clients/ImageProcTest.E2ETests/Scenarios/Workflows/LaneDenoiseScenarios.cs:84,134`는 L05/L06 종료 시 새 앱의 기본 상태로 복원하는 설명·상수다. 두 줄을 `No grid correction`으로 맞췄으며 L05/L06의 측정·단언은 바꾸지 않았다.
- `dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug --no-restore --filter 'FullyQualifiedName~AlgorithmPresetTests|FullyQualifiedName~CorruptSettingsFileTests' --logger 'console;verbosity=detailed'`: exit `0`, **13/13 통과**, skip 0. [green-integration.txt](green-integration.txt)에 전체 출력을 보존했다. 기존 이름 alias/실제 preset 매핑과 정상·손상 설정 불러오기를 검사한다. 일부 출력의 생략기호는 캡처 당시 인코딩대로 남겼으며 기존 컴파일·분석기 경고도 보존했다.
- `dotnet test clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj -c Debug --no-restore --filter 'FullyQualifiedName=ImageProcTest.E2ETests.Scenarios.Workflows.LaneDenoiseScenarios.L06_ChoosingTheReferenceAlgorithm_LeavesThePreprocessingSwitchOn' --logger 'console;verbosity=detailed'`: exit `0`, **1/1 통과**, skip 0. [green-e2e.txt](green-e2e.txt)에 전체 출력을 보존했다. 네 선택 모두 `preprocess checked = True`이며 실제 전체 chain과 B 상태를 읽었다. UIA Invoke/Selection/Expand 경로를 사용했으며 전역 키보드·마우스 입력을 사용하지 않았다.
- 각 명령은 기존 `env.cmd`를 통해 실행했고 같은 호출 안에 `DOTNET_CLI_UI_LANGUAGE=en`을 설정했다. E2E는 `XPE_E2E_BACKEND=Mock`, `XPE_GUI_DATA_DIR=D:/workspace-github/xpe-gui/clients/ImageProcTest.E2ETests/bin/Debug/net8.0-windows/gui-c-258c-check`를 설정했다. 로그는 UTF-8이며 줄 끝 공백만 제거했다.
- E2E 전 `ImageProcTest process count=0`, 자체 data root 없음 확인. 종료 뒤에도 프로세스 0개를 확인하고 정규화한 절대경로가 해당 test bin 아래 자체 폴더와 정확히 일치함을 검사한 뒤 그 폴더만 정리했다. 결과 `owned data exists=False`. 기존 설정·외부 작업은 보존했다.
- Reviewer가 승인 원문·4파일 diff·보고서·저장 GREEN 원문을 읽기 대조한 뒤 기술수용 및 신규 차단 없음으로 회신했다. 독립 재실행은 아니며, 기존 oracle과 legacy/mismatch 보존을 확인한 검토다. 부모 레인이 11:06:36 KST에 READ ACK를 반환했다.
- 최초 staged 공백 검사에서 증거 4개 파일의 EOF 빈 줄을 지적하여 그 끝 빈 줄만 제거했다. 줄 끝 공백과 EOF 빈 줄 정리는 시험 출력의 문장·숫자·순서를 바꾸지 않는다. 정리 후 staged 검사를 다시 수행한다.

## Baseline-attribution

- 측정 트리: `D:/workspace-github/xpe-gui`, `dev/gui`, HEAD `4ef55176bf17bfffdb9a0cc4f612fc374c4ca1e0`.
- 커밋 준비 중 `origin/main`이 3개 커밋 전진한 것을 fetch로 확인했다. 차이는 DICOM 시험 두 파일과 post 레인 증거 5개뿐이며 GUI 파일과 겹치지 않았다. `git merge --ff-only origin/main`으로 `47419b40`까지 반영했다. 위 시험을 실행한 기준은 `4ef55176`과 이 카드의 미커밋 변경이며, 동기화 뒤 시험을 다시 실행했다고 주장하지 않는다.
- 소스 변경 전 `git diff --name-only -- gui/ImageProcTest gui/ImageProcTest.SelfCheck`: 출력 없음.
- 수정 전 oracle 파일 blob: `LifetimeScenarios.cs` = `aa43fb46e1aac596a4057089bd3fbdee113d6f58`; `Program.cs` = `ed7305f47ebfcc22faa4477661ef791c6e5edecf`. 이후 LifetimeScenarios 변경은 14 setup 두 줄뿐이며 Program은 변경하지 않았다.
- SelfCheck는 scripted backend/VM 및 기존 WPF 계약 시나리오를 포함한다. 기존 loader 시나리오는 Windows `version.dll` 복사본을 decoy DLL로 로드하여 탐색/참조 수를 확인한다. 부모의 별도 제품 앱 실행은 위 Mock UIA 1건이며, XPE Native 엔진·화질 검증을 의미하지 않는다.
- 카드의 CI job `126293126002` 실패는 부모 레인이 별도로 로그를 읽었으며 로컬 측정과 분리한다. 이 보고서의 수정 검증은 로컬 RED/중간 실패/GREEN 및 위 통합·Mock UIA 결과다.

## Gaps

- 위 영향 통합·Mock UIA 검사는 통과했다. Native 전용 L05의 화질·de-noise 픽셀 변화는 이번에 실행하지 않았다.
- 구현 담당자는 커밋/푸시하지 않았다. 부모 레인이 네 소스 파일과 이 카드 증거만 별도 커밋하며 SHA와 정상 훅·staged 검사 결과를 리더/Reviewer에게 별도로 반환한다. 이 보고서 작성 시점에는 커밋 생성 전이다. 푸시는 하지 않는다.
- 기존 256~258의 부분 계약 실행은 해당 범위의 관찰이었다. 전체 SelfCheck를 실행하지 않아 기존 7·13 회귀를 놓쳤으며, 과거 부분 통과를 전체 suite 통과로 확대할 수 없다.
- LSP 진단·coverage·전체 CI는 측정하지 않았다. 수정 후 repository-wide 판정은 리더가 실행하는 통합 branch CI 소관이며 PENDING이다.
- 저장된 legacy 불일치와 빈값 fallback은 변경하지 않았다. 명시적 mismatch는 기존 257 계약에 포함되어 통과했다. 빈값 fallback 전용 추가 실행은 하지 않았다. Native 전용 E2E/제품 DLL/임상 화질은 미검증이다.

## Residual-risk

두 기존 계약(동일 유효 설정의 재사용, 명시적으로 불일치하는 Candidate의 별도 처리)은 기존 단언을 유지한 SelfCheck로 확인했다. 기본 이름 변경은 위 설정 통합·Mock UIA 범위에서 확인했으며 전체 E2E·Native 경로까지 확대하지 않는다. Tuner 보류와 외부 작업 보존을 유지하며, 제품 배포·실제 엔진/화질까지 검증됐다고 확대하지 않는다.
