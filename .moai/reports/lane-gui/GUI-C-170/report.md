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

## 8. GUI-C-170b — 옛 `showDisplayPanel=true` 가 새 패널을 열지 않게

### 정정 (리더·Codex 지적)

§1-3 의 "반증 다섯 팔이 모두 예상한 시험에서 빨강" 은 맞지만, 6ee39ec 커밋 메시지와 리더 회신에 쓴 "팔마다 다른 시험이 잡는다" 는 취지는 **틀렸다.** **P1 과 R1 은 둘 다 `A13` 을 실패시킨다.** 첫 실행에서는 메시지도 같아서 구별되지 않았고, `A13` 이 설정 파일을 직접 읽도록 고친 뒤에야 메시지(`STORED` / `REPORTED`)로 갈린다. 시험 이름으로는 갈리지 않는다: P1 은 통합 `PanelFlagPersistence` 가 독립적으로 더 잡고, R1 은 `A13` 하나뿐이다. 이미 낸 커밋 메시지는 고치지 않았다.

### 구별 장치와 이유

**`ShowDisplayPanel` 의 JSON 키를 `showDisplayPanel` → `showDisplaySettingsPanel` 로 바꿨다**(속성 이름·판독처는 그대로). 옛 파일의 옛 키는 로더가 알 수 없는 키로 무시하므로(`AppSettingsService` 에 `Unmapped` 설정 없음) 패널이 닫힌 채 시작하고, 이 코드가 켜서 저장한 값은 새 키로 가서 유지된다. **가장 단순한 이유**: 속성 특성 한 줄이다. 설정 버전 필드는 읽고 쓰고 비교하는 코드와 그 시험이 필요하고, 일회성 마이그레이션은 "이미 옮겼는가" 라는 상태가 필요하다.

옛 `true` 가 사용자 선택이 아니라는 근거는 리더가 `f7ca055` 에서 확인한 것을 그대로 따랐다(기본값 true · 옛 메뉴 `IsEnabled="False"` · Reset Layout 이 true 로 되돌림). 나는 그 사실을 다시 확인하지 않았다.

- **출하 `appsettings.json`**: 새 키 + `false`.
- **`fixtures/gui-s0/appsettings.template.json`**: 같은 기준으로 새 키 + `false`. 이 템플릿의 `true` 도 옛 기본값을 복사한 것이지 선택이 아니고, 옛 키를 남기면 읽히지도 않는 줄이 "패널을 연다"고 읽힌다. 런타임은 이 파일을 설정으로 읽지 않고(`fixtures/gui-s0` 폴더 존재만 확인), S0 러너도 이 값을 단언하지 않는다 — **둘 다 코드를 읽어서 확인한 것이고, 템플릿을 실제로 쓰는 실행은 돌려 보지 않았다.**

### 시험 (통합, `PanelFlagPersistenceTests`)

| 시험 | 내용 |
|---|---|
| `AFileWithoutTheCalibrationKey_LoadsWithThePanelOff_AndKeepsTheRest` | **단언을 `True` → `False` 로 바꿨고 그 자리 주석에 이유를 적었다.** |
| `TheSettingsFileThatShippedBeforeThePanels_LoadsWithTheDisplayPanelClosed_AndKeepsTheRest` (신규) | `f7ca055` 의 출하 파일을 **원문 그대로**(892 바이트) 로드 → 두 플래그 꺼짐, 그리고 다른 값(`rawWidth`·`voiWindowCenter`·보정 경로·`comparisonMode`)은 그대로 도착(통째로 버리는 로더와 구별하는 대조) |
| `ADisplayPanelTurnedOnWithThisCode_StaysOnAfterASaveAndALoad` (신규) | 옛 파일이 아니라 빈 상태에서 켜서 저장 → 다시 로드 → 켜짐 유지 |
| `BothPanelFlags_SurviveASaveAndALoad_AndAreIndependent` | 파일에 새 키 이름이 있는지 확인(키 이름을 못 박음) |

E2E `A13` 의 저장 확인도 새 키 이름으로 바꿨다.

### 반증 (D1: 구별 장치 제거 = 키 이름을 옛 이름으로 되돌림, 복사본에서만)

| 시험 | D1 | 대조군 |
|---|---|---|
| 옛 형식 파일(`AFileWithout…`) | **빨강** | 초록 |
| 출하 원문 파일(신규) | **빨강** | 초록 |
| 새로 켜서 저장 → 유지(신규) | **초록** | 초록 |
| `AreOffByDefault` | 초록 | 초록 |
| `BothPanelFlags_…`(키 이름을 못 박음) | 빨강 | 초록 |

카드가 요구한 "1번 빨강, 2번 초록" 은 그대로다. **`BothPanelFlags_…` 도 빨강인 것은 그 시험이 키 이름을 못 박기 때문이며 의도한 것**이고, 되돌린 코드가 새 키 이름 단언을 깨는 것이지 구별 능력을 따로 증명하는 것은 아니다. 대조군의 통합 5/5 초록.

### 재실행

- 빌드 오류 0. **통합 스위트 전체**(`--logger trx`): **실행 277 · 총계 277(기존 275 + 신규 2) · 통과 276 · 건너뜀 1 · 실패 0.**
- E2E `A12`·`A13` 은 통과한다. **E2E `S07`(Calibration 케이스)은 간헐적으로 실패한다 — 아래.**

### S07 미해결 (이 카드의 결론을 막는 항목)

지정 범위(`A12`·`A13`·`S07` + `PanelFlagPersistence`)를 실제 트리에서 돌리면 **2/2 실패**, 실패는 항상 `S07` 의 **첫 케이스**(`ShowCalibrationPanelMenuItem`)이고 메시지는 "gone from the View menu"(1초). 원인은 **밝히지 못했다.** 확인한 것만 적는다:

| 관찰 | 결과 |
|---|---|
| 현재 트리, S07 단독 | 3/3 통과 |
| 현재 트리, 앞선 시험 1개와 짝(A02/A13/A11) | 각각 통과 |
| 현재 트리, 4개(A11·A13·A02·S07) 함께 | 실제 트리 3회 + 복사본 2회 = **5/5 실패** |
| 170b 이전 `1400af4`, 같은 4개 | 1/1 통과 |
| **`1400af4`, 리더 지정 범위** | **2회 중 1회 같은 S07 실패** |
| 키만 옛 이름으로 되돌림(D1d, A13 도 되돌려 시간표 유지) | 5/5 통과 |
| 같은 되돌림 + 팝업 대기를 폴링으로 바꾼 진단(T1d) | **첫 팝업이 4초간 안 떠서 같은 케이스 실패** |
| 현재 코드 + 같은 폴링 진단(T1) | 팝업이 87~214 ms 에 떠서 5/5 통과 |

**읽는 법.** `1400af4` 도 같은 범위에서 실패하고, 키를 되돌린 코드(T1d)도 첫 팝업이 안 뜬다. **키 이름 변경이 원인이라고 말할 근거는 없고, 170b 의 회귀로 입증된 것도 없다.** 반대로 같은 키 이름 변경 코드(T1)가 통과하기도 해서, 키 이름이 영향이 없다는 것도 입증하지 못했다 — 진단용 폴링이 시간표를 바꾸므로 T1/T1d 는 서로만 비교할 수 있고 평범한 실행과는 비교할 수 없다. 평범한 실행(진단 없음)에서는 현재 트리가 0/5 통과이고, 170b 이전·키를 되돌린 트리(대조군 2회, `1400af4` 차분, D1c, D1d)는 5/5 통과라는 **차이가 남아 있고 설명하지 못했다.** 다만 `1400af4` 가 지정 범위에서는 2회 중 1회 실패했으므로, 이 차이가 코드 때문인지 그 시점 기계 상태 때문인지도 가리지 못했다.
확실한 것은 하나: S07 은 "메뉴가 안 열렸다"와 "항목이 사라졌다"를 구별하지 못한다. `S06` 은 같은 함정을 앵커(`ShowLogsPanelMenuItem`)로 막아 두었다. 이 카드가 `S07` 을 다시 쓰면서 그 앵커를 가져오지 않았다.

**하지 않은 것**: `S07` 을 고치지 않았다(리더 판단 대기). 후보는 `OpenViewMenu` 후 앵커가 보일 때까지 기다리고, 안 열렸으면 "열리지 않았다"로 실패하게 하는 것이다 — 이것은 메뉴가 첫 클릭에 안 열리는 현상을 **가리지** 않고 이름을 바로잡는 쪽으로 쓴다.

### 미검증 · 범위 밖

- **옛 파일을 앱이 실제로 열어 보는 실행은 하지 않았다.** 검증은 로더 수준(통합)이고, 실제 앱이 옛 `showDisplayPanel: true` 파일로 시작해 패널이 닫혀 있는지는 E2E 로 보지 않았다.
- **`docs/design/reference/gui-README.md:182`** 가 옛 키 `showDisplayPanel` 을 적고 있다. `docs/` 는 리더 소유라 손대지 않았다.
- **E2E 전체는 돌리지 않았다**(카드가 생략 허용).

## 파일

`gui/ImageProcTest/Views/{CalibrationPathsPanel,DisplaySettingsPanel}.xaml(.cs)` (신규) · `MainWindow.xaml(.cs)` · `Models/{AppSettings,GuiAutomationReport}.cs` · `ViewModels/MainWindowViewModel.cs` · `appsettings.json` · `gui/ImageProcTest.E2E/Program.cs` · `clients/…/PanelFlagPersistenceTests.cs`(신규) · `AutomationReportBackendTests.cs` · `PanelToggleScenarios.cs` · `SettingsProcessingConnectionTests.cs`

🗿 MoAI
