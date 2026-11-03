[2026-10-21 21:59 KST]
# QA-A-277c — IntegrationTests.csproj 의 누락된 소스 링크 2줄 보완

Refs #278. 결과: **링크 2줄을 더하면 IntegrationTests 가 빌드되고 `SettingsProcessingConnectionTests` 12건이 모두 통과한다. 링크가 없으면 같은 4건 오류로 빌드되지 않는다.**

## 1. 기준

- 실행 시점 HEAD: `c6d211a41f51207643451cd6cd77070f3d7dfa85` (QA-A-277b 의 U01 커밋). csproj 변경은 이 보고서와 함께 별도 커밋한다.
- 실행은 `git archive HEAD gui clients` 로 푼 깨끗한 사본 `build/qa277c/src`(git 무시 경로)에서 했다. 작업 트리의 미커밋 gui 제품 변경 8개는 건드리지 않았다.
- 변경 파일은 `clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj` 하나, 2줄 추가·삭제 0. 리더 21:17 허가(링크 2줄에 한함, 다른 항목 불변)의 범위 안이다. diff 원문은 `evidence/csproj.diff.gz.b64.txt`(sha256 `41677473eabecf31b5c7fec52e7caa0aa53bd987551d78044ba7b285b50edb43`, 복원: `base64 -d … | gzip -dc`).

추가한 줄(`GuiPreprocessRunner.cs` 링크 바로 아래):

```xml
<Compile Include="..\..\gui\ImageProcTest\Services\CalibrationStageSelection.cs" Link="Chain\Linked\CalibrationStageSelection.cs" />
<Compile Include="..\..\gui\ImageProcTest\Services\GuiDataPaths.cs" Link="Chain\Linked\GuiDataPaths.cs" />
```

## 2. 대조 — 링크가 없을 때

- 명령: `dotnet build clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug` (사본, csproj 변경 전) — exit 1
- 오류 4건(`error CS0103`, 이름이 현재 컨텍스트에 없음):
  - `gui\ImageProcTest\Services\AppSettingsService.cs(45,45)`: `GuiDataPaths`
  - `gui\ImageProcTest\Services\Native\GuiPreprocessRunner.cs(58,27)`, `(59,25)`, `(60,27)`: `CalibrationStageSelection`
- 이 대조는 QA-A-277b 때(U01 변경 되돌림)와 이번 사본에서 각각 같은 4건으로 재현됐다. 별도 로그 파일은 두 번째 실행(이번 카드)의 출력을 터미널에서 확인했고, 첫 번째는 `QA-A-277b/evidence/log-integration.log` 에 있다.

## 3. 링크를 더한 뒤

- 같은 사본에 `git apply --whitespace=error` 로 diff 를 적용. 빌드 명령은 위와 같고 `오류 0개`.
- 시험: `dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug --filter "FullyQualifiedName~SettingsProcessingConnectionTests" --logger "console;verbosity=detailed" --blame-hang-timeout 5m --blame-hang-dump-type none`
- exit 0. **총 12, 통과 12, 실패 0, 건너뜀 0.** 로그: `evidence/log-integration.log`.
- 통과한 시험: Control_EnabledControlForUnconnectedSetting_IsRed, Control_BindingANewUnreadSetting_IsRed, Control_InterpolatedConfig_IsNotARead_StructuredConfigIs, EveryViewStateSetting_HasADisplayReading, Collection_FindsTheKnownBindings, Control_ViewStateWithoutAReading_IsRed, Control_RemovingAnUnconnectedEntry_IsRed, EveryBoundSetting_IsConnectedOrDeclaredAndDisabled, IsWritable_HonoursAnExplicitMode, PendingConnectionEntries_AreBoundAndCarryAnIssue, Control_NaiveSearch_CountsTheSummaryAsProcessing, ProcessingSearch_FindsKnownReads.
- 실행 직후 남은 프로세스 0(`tasklist | grep -ciE 'ImageProcTest|testhost|xpe_ai_worker'`).
- 환경 변수는 U01 과 같은 `run.sh` 를 썼다(`XPE_E2E_*`, `XPE_C231_*`). 이 통합 시험은 앱을 띄우지 않으며 그 값을 읽지 않는 것으로 보이나, 읽는지 여부는 확인하지 않았다.

## 4. 이 카드가 덮는 범위

- 덮는 것: IntegrationTests 어셈블리가 `888d0bc1`·`c6d211a4` 기준에서 컴파일되지 않던 원인(링크 2줄 누락)과, 그 어셈블리의 `SettingsProcessingConnectionTests` 단독 통과. 이 클래스는 QA-A-277 patch 가 바꾼 파일이라(Reviewer #264 F264-2) 그 변경이 실제로 컴파일·실행된 것은 이번이 처음이다.
- 덮지 않는 것: IntegrationTests 어셈블리의 나머지 시험(전체 스위트 미실행), CI 의 `dotnet-tests` 잡 결과. 다른 csproj 항목은 건드리지 않았다.

## 5. W16 — 미검증 공백으로 남김

시험 `W16_ARealFrameCalibrationSet_IsUsedByTheApp_WithoutWarnings` 는 `XPE_C247_CALIB`(보정 폴더)와 `XPE_C247_RAW`(원시 프레임)가 모두 있을 때만 실행된다(시험 주석, `QuickStartWalkthroughScenarios.cs:473-478`). 이 PC 에서 찾은 것(읽기 전용 조사, 실행 안 함):

- 보정 세트: `D:\workspace-github\xpe-data\calib-cyan-v3`(`offset.xcal`·`gain.xcal`·`defect.xcal`·`manifest.json`·`phantoms/`). 시험 안의 "Last run by hand on 2026-10-15 against calib-cyan-v3" 와 gui 의 GUI-C-247 보고서(`run_cyan_v3.txt` 통과 14 s)가 이 세트를 가리킨다.
- 원시 프레임: GUI-C-247 보고서가 쓴 프레임이 무엇인지는 이 카드에서 확정하지 못했다(보고서 28~30행 부근 참조). `calib-cyan-v3/phantoms/` 의 파일은 `*_ours_f32le.raw` 로 시험이 기대하는 3072×3072 16비트 원시 프레임이 아닐 수 있다(미확인).
- **실행하지 않았다.** 이 카드의 실행 승인은 통합 시험 1건에 한정됐고, W16 은 UI 를 띄우는 별도 실행이므로 사용자 승인 없이 돌리지 않았다. W16 은 미검증 공백(실데이터 있음, 미실행)으로 남는다. 실행하려면 사용자가 별도로 승인하고 프레임 파일을 지정해야 한다.

## 6. 미검증

- W16 실행 결과. `SettingsProcessingConnectionTests` 외 IntegrationTests 시험. CI `dotnet-tests` 잡이 이 변경으로 통과하는지.
