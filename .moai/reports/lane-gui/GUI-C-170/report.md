# GUI-C-170 — `#225` 행 7·8: 패널 둘과 정본 결정

레인: gui · 이슈: `#225` · 브랜치 `dev/gui` · 푸시하지 않음(커밋만)

## 1. 주장

1. **행 7 Calibration Paths 와 행 8 Display Settings 패널이 생겼고**, 두 가시성 플래그는 영속 `Settings.*` 에 있으며 자동화 보고서에 실린다. **판정(`Passed`)에는 들어가지 않는다.**
2. `ShowCalibrationPanel` 은 삭제하지 않고 VM 에서 `Settings` 로 옮겼다. `VM:910-916` 의 정정 주석은 이번 결정으로 갱신했다.
3. 반증 다섯 팔이 모두 예상한 시험에서 빨강이 되고, 대조군은 초록이다.
4. 전체 스위트는 실패 0 이다.

## 2. 증거

### 전체 스위트 (`--logger trx`, 실제 트리, 동시 실행 없음)

명령: `dotnet test clients/ImageProcTest.slnx -c Debug --no-build --logger "trx;LogFileName=c170_final.trx"` · 종료 코드 0 · 원문 `final_run.txt`

| 어셈블리 | 실행(통과+실패) | 총계 | 건너뜀 | 실패 |
|---|---|---|---|---|
| IntegrationTests | 274 | 275 | 1 | 0 |
| E2ETests | 145 | 177 | 32 | 0 |

**DISABLED(미구현 메뉴) 집계 = 7.** `A11` 이 세 축을 대조한다: 앱의 이름 목록 7 · 메뉴 트리 순회 7 · `MainWindow.xaml` 의 `IsEnabled="False"` 7. 대조군에서 통과하고, `U1` 에서 XAML 만 8 이 되자 세 수가 갈라져 빨강이 났다.

### 반증 (`c170_arm.py <팔>` — 팔 하나당 1회 호출)

변형은 `%TEMP%` 복사본에만 적용했다. 실제 트리는 시작·끝 해시를 비교해 **매 팔 `real tree untouched: True`**. 각 팔 직전에 가용 메모리를 쟀다(10.6~12.8 GB, 하한 4 GB).

| 팔 | 변형 | 빨강이 난 시험 | 실패 메시지(요지) |
|---|---|---|---|
| control | 없음 | 없음(통합 3 · E2E 5 통과) | — |
| P1 | 두 플래그에 `[JsonIgnore]` | `PanelFlagPersistence` 2건 + `A13` | 설정 파일에 `showCalibrationPanel=true` 가 없음(저장 안 됨) |
| V1 | 판정에 `DisplayPanelVisible` 재삽입 | `A13` + `A02` | 패널 상태가 판정의 일부가 되면 안 된다 |
| W1 | `Visibility` 바인딩 둘 제거 | `S07` 2건 + `A13` | 토글을 껐는데 패널이 떠 있다 |
| U1 | XAML 에만 `IsEnabled="False"` 1건 추가 | `A11` | 이름 목록 7 · 트리 7 · XAML 8 |
| R1 | 보고서 필드 두 개를 채우지 않음 | `A13` | 저장은 됐는데 보고서가 싣지 못함 |

**R1 에서 찾은 약점과 고친 것.** 첫 실행에서 P1 과 R1 이 **같은 메시지**("flag was not persisted")를 냈다. 저장이 깨진 것과 보고가 깨진 것을 `A13` 만으로는 가를 수 없었다. 그래서 `A13` 이 첫 실행 뒤 **설정 파일을 직접 열어** 저장 여부를 먼저 단언하게 했다. 다시 돌린 결과 P1 은 "STORED", R1 은 "REPORTED" 로 갈린다.

### 이 카드가 바로잡은 시험 전제

`A12` 는 기준선에서 실패했다. 원인은 앱이 아니라 **시험의 전제**였다: "출하 값"을 `bin/appsettings.json` 에서 읽었는데, 그 파일은 다른 시험이 저장하면서 임시 경로로 바뀌어 있었다. 자동화 실행은 파일이 아니라 코드 기본값(`data/calibration/*`)에서 시작한다(`MainWindow.xaml.cs:134`). 기대값을 리터럴 셋으로 바꿨다. 같은 시험이 단독으로 통과, 전체 스위트에서도 통과.

## 3. 기준 귀속

- 기준선: 같은 명령, 같은 트리 — E2E 통과 145 · **실패 1(`A12`)** · 건너뜀 31 · 총 177 (`baseline_run.txt`).
- 최종과의 차이(시험 이름별 비교, 두 trx): 바뀐 것은 **둘뿐** — `A12` 실패→통과, `WindowReacquireTests.ReadableWindow_IsKept_AndLeavesNoNote` 통과→건너뜀.
- 그 건너뜀은 이 카드와 무관하다: FlaUI 가 창을 다시 잡는 일이 실행당 약 7% 로 생기고, 시험이 `XPE-SKIP-ALLOWED:170` 토큰과 함께 일부러 건너뛰도록 설계됐다(GUI-C-74/75). 이번 실행에서 그 일이 한 번 생겼다.

## 4. 미검증

- **Browse 버튼은 누르지 않았다.** 모달 폴더 대화상자라 무인 실행이 답할 수 없다. 러너가 명령이 바인딩돼 있음만 단언한다.
- **Native 백엔드는 돌리지 않았다.** Mock 만 실행했고, 네이티브 DLL 은 CI 몫이다(#98).
- **`bin/appsettings.json` 을 누가 바꾸는지는 추적하지 못했다.** `xpe_calib_<guid>` 와 `xpe-c129-e…` 두 임시 경로가 나왔다. 보고서 없이 뜨는 실행이 `--automation-calib` 을 출하 파일 위에 얹고 `Save` 하는 경로(`MainWindow.xaml.cs:125-131`, `VM:1581`)가 **가설**이다. 확인하지 않았다. `A12` 는 이제 그 파일을 읽지 않지만, 그 파일을 읽는 **다른 시험이 있는지는 보지 않았다.**
- 집계 "9 → 7" 중 **9 는 다시 세지 않았다.** 카드가 준 값이고, 내가 잰 것은 7(세 축 일치)이다.
- 통합 시험의 trx 는 보존되지 않았다(두 어셈블리가 같은 파일명을 써서 뒤의 것이 덮어씀). 통합 수는 콘솔 출력에서 읽었다.

## 5. 잔여 위험

- `A12` 의 기대값 리터럴이 앱의 기본값과 중복이다. 기본값이 바뀌면 시험이 시끄럽게 실패한다(의도한 방향).
- `WindowReacquire` 건너뜀이 실행당 약 7% 로 계속 생긴다. 건너뜀 수는 31~32 사이에서 움직일 수 있다.
- 오염된 `bin/appsettings.json` 은 스위트 실행마다 다시 생길 수 있다. 이 카드에서는 소스 값으로 복원해 두었다.

## 6. 범위 밖 발견 (보고만)

- `clients/ImageProcTest.E2ETests/Scenarios/Workflows/ProcessingChainScenarios.cs:322` — 컴파일 경고 xUnit2031(`Where` 뒤 `Assert.Single`). 이 카드가 건드리지 않은 파일이다.
- 이번 세션에서 첫 기준선 실행을 **내가 10분 제한으로 끊었다**(백그라운드 시간 제한을 600000 으로 줌). 남은 프로세스는 없음을 확인했고, 1시간 제한으로 다시 돌렸다.

## 7. BOM 제거 (리더 판정 후속 커밋)

리더가 `appsettings.json` 첫 바이트에 BOM(U+FEFF)이 새로 붙은 것을 지적했고, 4개 파일 모두 제거하라고 판정했다.

**원인(부분 확인).** 앱 `Save` 는 아니다 — `AppSettingsService.cs:193` 이 `File.WriteAllText(path, text)` 로 BOM 없이 쓰고, 커밋된 diff 는 손으로 쓴 3줄이다. 이 카드의 이전 세션에서 내가 쓴 파이썬 편집 스크립트가 `utf-8-sig` 로 파일을 썼다. 남아 있는 스크립트 둘로는 `SettingsProcessingConnectionTests.cs` 만 확인했고, 나머지 셋은 스크립트가 남아 있지 않아 같은 기전이라는 것은 **추정**이다.

**수정.** `6ee39ec` 의 4개 파일에서 첫 3바이트(`ef bb bf`)만 제거했다. 줄 끝·내용은 그대로.

| 파일 | 수정 전 | 수정 후 |
|---|---|---|
| `gui/ImageProcTest/appsettings.json` | `ef bb bf` | `7b 0a 20` |
| `PanelToggleScenarios.cs` | `ef bb bf` | `2f 2f 20` |
| `SettingsProcessingConnectionTests.cs` | `ef bb bf` | `2f 2f 20` |
| `PanelFlagPersistenceTests.cs` | `ef bb bf` | `2f 2f 20` |

- **바이트 증명**: 4개 모두 "수정 후 파일 == `6ee39ec` 의 blob 에서 앞 3바이트를 뺀 것" 이 `IDENTICAL`, CRLF 수 0 → 0. `git diff --stat` 의 파일당 1줄은 BOM 이 1행에 붙어 있어서이고, 그 줄의 내용은 같다.
- **재실행(지정한 시험만, 전체 스위트는 인코딩만 바뀌어 생략)**: `A12`·`A13`·`S07`(2건) = E2E 4/4, `PanelFlagPersistence` = 통합 3/3. 빌드 오류 0.
- 원문: `bom_evidence.txt`, `bom_rerun.txt`.

**미검증**: BOM 없는 `appsettings.json` 으로 앱이 시작하는 것은 위 시험들이 간접으로만 보인다. 이 4개 외 파일의 BOM 은 건드리지 않았다(`.cs` 는 220개 중 46개가 BOM 이라 저장소 관례가 혼재).

**기존 경고(이 카드·BOM 과 무관)**: 전체 재빌드에서 `MainWindowViewModel.cs` 의 XML 문서 경고 CS1573 ×4 · CS1574 ×2 가 보인다. `f7ca055` 를 같은 방식(`gui`+`clients`)으로 빌드해도 같은 6건이 한 줄씩 밀린 위치에 있다. 증분 빌드는 이 경고를 보여 주지 않아 앞 실행에서는 0 으로 보였다.

## 파일

`gui/ImageProcTest/Views/{CalibrationPathsPanel,DisplaySettingsPanel}.xaml(.cs)` (신규) · `MainWindow.xaml(.cs)` · `Models/{AppSettings,GuiAutomationReport}.cs` · `ViewModels/MainWindowViewModel.cs` · `appsettings.json` · `gui/ImageProcTest.E2E/Program.cs` · `clients/…/PanelFlagPersistenceTests.cs`(신규) · `AutomationReportBackendTests.cs` · `PanelToggleScenarios.cs` · `SettingsProcessingConnectionTests.cs`

🗿 MoAI
