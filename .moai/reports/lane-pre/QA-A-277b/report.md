[2026-10-21 21:16 KST]
# QA-A-277b — U01 이 Native 실제 실행 단계를 모드별로 단언하도록 보완 (Reviewer #264 F264-1)

Refs #278. 결과: **U01 보완과 변이 반증은 완료. 그러나 카드 4항의 "모두 PASS" 조건은 충족하지 못해 커밋하지 않았다.** W16 은 건너뜀(환경 변수 없음), IntegrationTests 는 기준 `888d0bc1` 에서 이미 컴파일되지 않는다. 판단은 리더 몫이다(7절).

## 1. 기준과 환경

- 기준 commit `888d0bc1`. `git archive 888d0bc1 gui clients` 로 푼 깨끗한 사본 `build/qa277b/src`(git 무시 경로)에 U01 변경만 적용해 실행. 작업 트리의 미커밋 gui 변경 8개는 건드리지 않았다.
- 네이티브 DLL: CI run **39571675063**(head b140f7f2)의 산출물. 카드가 권한 run 39591918725 는 진행 중이었고 preprocess 산출물이 아직 올라와 있지 않아 쓰지 않았다. 두 head 사이의 gui·native 제품 변경은 없다(QA-A-277 보고서 1절).
- 보정 세트: QA-A-277 에서 만든 `build/qa277/calib-real`(release testdata-calib-v1, SHA256SUMS 확인 후 `xpe_calib_fixture_gen`). 환경 변수는 CI 와 같은 Native 설정이고 경로는 역슬래시 `D:\…`.
- 시험은 한 번에 1개씩, 실행 직후 남은 프로세스 0.

## 2. 단언 설계 (모드 → 예상 단계)

파일: `clients/ImageProcTest.E2ETests/Scenarios/Workflows/UnappliedSettingsScenarios.cs` 한 곳만, 42 줄 추가·삭제 0. 새 helper `AssertNativeStagesExecutedPerMode` 를 Native 분기의 기존 단언 뒤에서 호출한다. 기존 단언은 그대로 두었다.

- 근거 계약: `gui/ImageProcTest/Services/CalibrationStageSelection.cs:5-9` — Off = 맵을 읽지 않고 실행하지 않음, On = 맵 필수, Auto = 맵이 있을 때만. GUI-PRE-GATE-009.
- 예상 집합은 `requested=[...]` echo 가 아니라 **요청 모드 + 보고서 `settings.calib{Offset,Gain,Defect}Dir` 아래 `offset.xcal`·`gain.xcal`·`defect.xcal` 의 디스크 존재**로 계산한다. fixture 에 세 맵이 모두 없으면 단언이 즉시 실패한다(맵이 없으면 On/Auto/Off 가 결과로 구분되지 않기 때문).
  - Off → 해당 단계 없음. On → 있음. Auto → 맵이 있으면 있음(fixture 에는 세 맵이 있으므로 있음).
- 같은 렌더에서 세 가지를 예상 집합과 대조한다.
  1. reason 의 `executed=[...]` 집합(순서 무관)
  2. reason 의 `nativeFlags=0x…` 의 단계 비트만: **offset 0x2000, gain 0x0008, defect 0x0004**. flags 전체 숫자는 고정하지 않는다. 출처: `gui/ImageProcTest/Services/Native/GuiPreprocessRunner.cs` 의 `RunPipelineOut`(`Flags & 0x2000 → offset`, `& 0x0008 → gain`, `& 0x0004 → defect`). 카드 본문의 "gain 0x4000, defect 0x0008" 은 제품 코드와 다르다. 0x4000 은 단계 비트가 아니라 일부 단계가 우회됐다는 표시(Off 렌더에서 flags 0x401c 로 관측)이므로 단언하지 않았다. Reviewer #264 가 적은 값과 코드가 일치한다.
  3. 보고서 `executed.hud` = `"<N> native stages (<a> · <b>)"`(W13a 가 같은 제품 표시 계약을 `Calib:  ` 접두와 함께 단언).
- 기존 관측(QA-A-277 로그)에서 Off 렌더는 `executed=[gain,defect]` 0x401c, `[offset,defect]` 0x6014, `[offset,gain]` 0x2018 이었다. 새 단언은 이것을 자동으로 확인한다.

## 3. 변이 반증

깨끗한 사본에서만 `CalibrationStageSelection.ShouldLoad` 의 Off 분기를 `false` → `exists` 로 바꾸고(`CalibrationStageMode.Off => exists`) gui 를 다시 빌드해 U01 을 실행했다.

- 명령: `dotnet test clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj -c Debug --filter "FullyQualifiedName~U01_CalibrationStages_AreUsableExactlyWhenConnected"` — exit 1, 13 s
- 원문:
  ```
  Assert.Equal() Failure: Collections differ
  Expected: ["defect", "gain"]
  Actual:   ["defect", "gain", "offset"]
  ```
- 같은 변이 상태에서 요청 echo(`requested=[offset:Off,…]`)는 맞고 `preprocess=Applied` 도 그대로여서, QA-A-277 의 U01 은 이 회귀를 통과시켰을 것이다(Reviewer #264 정적 추론과 일치). 로그: `evidence/log-u01-mutant.log`.
- **원복 확인:** 사본의 `CalibrationStageSelection.cs` 를 되돌리고 `diff` 로 `git show 888d0bc1:…` 와 바이트 동일함을 확인, `touch` 후 gui 를 다시 빌드(오류 0)한 뒤 U01 을 다시 실행했다(4절). 작업 트리 제품 코드는 수정하지 않았다.
- 변이는 Off 분기 하나만 시험했다. On·Auto 방향의 변이(예: Auto 가 맵을 무시)는 실행하지 않았다.

## 4. 시험별 명령·종료 코드·결과

공통: 깨끗한 사본, `-c Debug`, `--logger "console;verbosity=detailed" --blame-hang-timeout 5m --blame-hang-dump-type none`.

| 시험 | 필터 | exit | 결과 |
|---|---|---:|---|
| U01 (변이 전, 새 단언) | `U01_CalibrationStages_AreUsableExactlyWhenConnected` | 0 | 통과 40 s (`log-u01-new.log`) |
| U01 (변이 중) | 같음 | 1 | 실패, 3절 원문 (`log-u01-mutant.log`) |
| U01 (원복 후) | 같음 | 0 | 통과 40 s (`log-u01-final.log`) |
| W16 `W16_ARealFrameCalibrationSet_IsUsedByTheApp_WithoutWarnings` | 해당 이름 | 0 | **건너뜀**: `XPE_C247_CALIB/XPE_C247_RAW not set - the real-frame set is not in the repository … XPE-SKIP-ALLOWED:272` (`log-w16.log`) |
| IntegrationTests `SettingsProcessingConnectionTests` | 클래스 이름 | 1 | **컴파일 실패**, 시험 0건 실행 (`log-integration.log`) |

W16 의 건너뜀은 PASS 가 아니다. 이 시험은 calib-cyan-v3 실프레임 세트가 필요한 환경 게이트 시험이고, 그 세트는 저장소에 없다(XPE_C247_* 를 줄 수 없었다). CI 에서도 같은 사유로 허용된 건너뜀이다.

## 5. IntegrationTests 는 기준 888d0bc1 에서 이미 컴파일되지 않는다

```
gui\ImageProcTest\Services\Native\GuiPreprocessRunner.cs(58,27): error CS0103: 'CalibrationStageSelection' 이름이 현재 컨텍스트에 없습니다.
(59,25), (60,27): 같은 오류
gui\ImageProcTest\Services\AppSettingsService.cs(45,45): error CS0103: 'GuiDataPaths' 이름이 현재 컨텍스트에 없습니다.
```

- 대조: U01 변경을 되돌린 상태(= `888d0bc1` 그대로)에서 같은 프로젝트를 빌드해도 **같은 4건**이 난다. 그러므로 U01 변경 때문이 아니다.
- 원인으로 보이는 것(확인한 사실만): `clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj:127` 이 `GuiPreprocessRunner.cs` 를 소스 링크로 가져오는데, 이 csproj 에는 그 파일이 쓰는 `CalibrationStageSelection.cs` 와 `GuiDataPaths.cs` 의 링크가 없다(grep 결과 0건). 두 파일은 `gui/ImageProcTest/Services/` 에 추적돼 있다.
- CI run 39571675063 에서 `dotnet-tests` 잡은 **skipped** 였다. 그래서 이 컴파일 실패는 CI 에서 아직 드러나지 않았다. 잡이 실행되면 실패할 가능성이 높다(추정, 미실행).
- csproj 수정은 이 카드가 허용한 범위(U01 파일 1개)를 넘으므로 하지 않았다. `SettingsProcessingConnectionTests`(QA-A-277 에서 patch 로 바뀐 파일)는 이 상태에서 컴파일·실행 결과를 낼 수 없다.

## 6. 증거와 상태

- 시험 파일 변경은 **커밋하지 않았다.** `xpe-pre` 작업 트리의 `UnappliedSettingsScenarios.cs` 에만 적용된 미커밋 변경 42 줄로 남아 있다(다른 파일은 불변). 되돌리려면 `git checkout -- <그 파일>`.
- 추적되는 증거: 이 보고서와 `evidence/u01.diff.gz.b64.txt`(diff 원문 gzip+base64, 4116 바이트, sha256 `c566eb65fa91224434c2b2da7f225d1c18a2a9fecd4503acdc7abd5cd181cbe4`, 복원: `base64 -d … | gzip -dc`). `.log` 는 무시 규칙 때문에 디스크에만 있다.
- 남은 프로세스 0: 각 실행 직후 `tasklist | grep -ciE 'ImageProcTest|testhost|xpe_ai_worker'` = 0.

## 7. 리더에게 묻는 것

1. U01 변경만 먼저 커밋해도 되는가? U01 은 새 단언 통과 + 변이 반증 + 원복 후 통과를 모두 가졌다. 카드 4항은 "모두 PASS" 를 요구해서 멈췄다.
2. IntegrationTests csproj 에 `CalibrationStageSelection.cs`·`GuiDataPaths.cs` 링크를 더하는 수정을 누가 맡는가(범위 확장 허가 필요).
3. W16 은 실프레임 세트가 있는 곳(CI 또는 수동)에서만 실행 가능하다.

## 8. 미검증

- W16 실행 결과, `SettingsProcessingConnectionTests` 실행 결과(둘 다 없음).
- On·Auto 방향의 변이, gain·defect 단계를 무시하는 변이(Off 분기 한 곳만 변이).
- 작업 트리(dirty) 기준 실행. 이 카드는 깨끗한 사본만 썼다.

후속: 이 보고서 작성 뒤 시험 변경은 c6d211a4 로 커밋됨(4·70·76행의 미커밋 서술은 883bd0e2 시점 상태).
