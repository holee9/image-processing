# GUI-C-82 보고 — HAZ-GUI-005 통제 (#175)

커밋(dev/gui, 미푸시): bc7f2f0

## 0. 구현 전 확인 — (3) 보고서, (4) 운영/시험 모드

| 확인 | 결과 | 근거 |
|---|---|---|
| 내보내기 보고서가 있는가 | **있다, 3종** | `ExportAutomationReport` → `menu-command-report.json` (File 메뉴와 Evidence Snapshot 메뉴가 같은 명령); `--automation-report` → `GuiAutomationReport`; `ExportEvidenceBundle` → `evidence/<RunId>` 폴더를 zip (내용은 `RecordVerdict` 가 쓰는 `verdicts.json`) |
| 수정 전 Mock 표기 | `menu-command-report.json` 은 `backend = RuntimeInfo` 를 담아 `BackendName=MockXpeBackend` 로 **간접**적으로만 드러났고, `settings.BackendMode` 는 요청값. `GuiAutomationReport.BackendMode` 도 요청값. 증거 묶음에는 백엔드 정보가 **없었다** | `MainWindowViewModel.ExportAutomationReport`, `MainWindow.xaml.cs` 보고서 작성부, `RecordVerdict` |
| 운영/시험 모드 구분이 있는가 | **없다.** (4) 는 적용할 대상이 없는 통제다. 새로 만들지 않았다 | `gui/ImageProcTest` 의 `*.cs/*.xaml/*.csproj/*.json` 에서 `production|IsClinical|operational|ReleaseMode|#if DEBUG|IsTestGui` 검색 — "Production" 은 알고리즘 이름 문자열(`"Production v1.2"`)뿐 |

## 1. 주장

1. 상태 표시줄 `mode=` 는 이제 실제 백엔드를 표시한다. 요청과 다르면 `mode=Mock (requested Native)` 로 둘 다 보인다. 값의 원천은 백엔드가 직접 보고하는 `RuntimeInfo` 이며, C-81 의 `src=` 와 같은 원천이다.
2. 실제 백엔드가 Mock 이면 다음 세 가지가 적용된다.
   - (1) 닫기 컨트롤이 없는 경고 배너를 띄운다.
   - (2) 제목 앞에 `[MOCK]` 을 붙인다.
   - 두 가지 모두 메인 창과 분리 뷰어에 적용된다.
3. (3) 보고서 표기:
   - `GuiAutomationReport` 에 `ActualBackendMode`, `MockBackend` 를 추가했다.
   - `menu-command-report.json` 에 `actualBackendMode`, `mockBackend`, `requestedBackendMode` 를 추가했다.
   - 증거 폴더에 `backend.json` 을 쓴다.
4. 판정 기준: `RealXpeBackend` 가 **아니면** Mock 으로 본다. 알 수 없는 백엔드는 경고 쪽으로 분류된다.
5. 의도적 Mock(Mock 을 요청한 실행)에도 배너와 `[MOCK]` 을 띄운다. HAZ-GUI-005 의 "Mock mode 시" 를 그대로 읽은 결과다. 이때는 대체된 것이 없으므로 "requested" 문구는 붙지 않는다.
6. E-01 시험을 실제로 만들었다 (E-01a~d). Native 실행에서 각 통제를 따로 끄면 그 통제의 시험이 빨개진다.

## 2. 증거 (이 디렉터리)

### 기존 Mock E2E 영향 — 카드의 질문

`full-mock.txt` 는 수정 후 두 번째 전체 실행이다.
- 첫 전체 실행에서 **S11 이 깨졌다.** 원인은 정확한 제목 조회(`ByName("ImageProcTest Comparison Viewer")`)가 `[MOCK] ` 접두사에 걸린 것이다. 제목 끝부분 일치로 바꿨다(`WorkbenchObservation.FindDetached`).
- 같은 실행에서 E-01b, E-01d 도 실패했다. 배너 `Border` 가 자동화 트리에 **없어서**다. 시험이 안쪽 TextBlock 을 읽도록 바꿨다.
- 두 번째 실행에서는 E-01c 가 첫 메뉴 클릭 실패로 떨어졌다. `OpenDetached` 에 재시도를 추가했다.
- `mock3.txt`: 이후 Mock 3회 실행 결과 3회 모두 통과 82 / 실패 0. 실행되지 않은 1건은 `NativeProvenance` 이며 Mock 이라 건너뛴 것이다.
- 기존 시험 중 배너 때문에 **레이아웃이 밀려서** 깨진 것은 관측되지 않았다. 대상은 이 3회 실행, 좌표를 쓰는 W-15 를 포함한다.

### Native 대조와 반증

필터는 `E01|S05|S11`, `-NativeDir build/ci-common/bin` 으로 실행했다.

| 실행 | 실패한 시험 | 비고 |
|---|---|---|
| `native-control` | 없음 (통과 6) | E-01d: `mode=Native · … · src=bin`, 배너 없음, 제목 `ImageProcTest GUI-S0` |
| `falsify-status` (상태줄을 요청값으로) | E-01a, S11 | |
| `falsify-banner` (배너 `Visibility="Collapsed"`) | E-01b, S11 | |
| `falsify-title` (`[MOCK]` 제거) | E-01c, S11 | |
| `falsify-always` (`IsMockBackend => true`) | E-01d, S05, S11 | **대조군이 항상 켜진 배너를 잡는다.** S05 는 기존 시험으로, Native 인데 mock 표식이 보이면 실패하도록 되어 있다 |

trx 의 `outcome="Failed"` 개수에는 실행 전체 요약 1건이 더해져 있다. 시험별 결과는 `testName` 줄로 읽었다.

E-01 기록 (Native 실행, 대체 앱):
- `E01a status-bar='mode=Mock (requested Native)  |  common=v0.0.0-mock  |  display=v0.0.0-mock-display'`
- `E01b banner text='MOCK BACKEND — Native was requested but could not be used. …'`
- `E01c main title='[MOCK] ImageProcTest GUI-S0'`, `detached title='[MOCK] ImageProcTest Comparison Viewer'`, 분리 뷰어 배너 있음

### S11 간헐 실패 — 반증과 무관

- `native-control3`: 수정하지 않은 트리에서 3회 중 2회 S11 이 실패했다.
- `s11diag`: 진단 메시지를 넣고 4회 중 3회 실패했다. 3회 모두 **클릭 시점에 메뉴 위에 있던 창이 `✳ xpe-leader pid=18560`**, 즉 다른 세션의 터미널 창이었다. 앱의 pid 는 매번 달랐다.
- 따라서 `SetForeground` 가 앱을 앞으로 가져오지 못한 **이 기계의 데스크톱 상태**가 원인이다. 제품 결함이나 이번 변경 때문이 아니다. S11 메시지에 이 진단을 남겼다.

### 보고서 실측

- `automation-report.json` (Mock 요청): `BackendMode=Mock ActualBackendMode=Mock MockBackend=True Passed=True`
- `automation-report-fallback.json` (Native 요청, 빈 DLL 폴더): `BackendMode=Native ActualBackendMode=Mock MockBackend=True Passed=True`

### 그 밖

- 통합 시험 `full-int.txt`: 실패 0 / 통과 209 / 건너뜀 1
- 빌드: 경고 0 / 오류 0

## 3. 기준선 귀속

- 모든 수치는 이 세션의 dev/gui 작업본에서 측정했다.
- `falsify-*` 와 `native-control*` 은 S11 진단 메시지를 넣기 전 트리에서 실행했다. 이후 커밋과의 차이는 S11 의 실패 메시지와 메뉴 여는 함수의 반환값뿐이다.
- `mock3` 는 `OpenDetached` 재시도를 넣은 뒤, S11 진단을 넣기 전에 실행했다.
- **bc7f2f0 트리 그대로 전체 Mock 이나 Native 를 다시 돌리지는 않았다.** 이후 빌드만 확인했다.
- Native DLL 은 기존 `build/ci-common/bin` 을 사용했다.

## 4. 미검증

- **배너 닫기 불가:** 구성으로만 보장된다. `Border` 에 TextBlock 하나만 있고 컨트롤이 없다. `Border` 가 자동화 트리에 없어서 "닫기 버튼 없음" 은 시험으로 단언하지 못했다.
- **`menu-command-report.json` 과 `backend.json`:** 실제로 파일을 써서 필드를 읽어 보지는 않았다. 빌드와 코드만 확인했다. 자동화 보고서는 실측했다.
- **배너 가시성:** 화면에서 눈에 띄는지(크기·색·위치)는 픽셀로 확인하지 않았다. 자동화 트리에서 화면 밖이 아니라는 것만 확인했다.
- **실행 중 백엔드 전환:** Backend 메뉴로 백엔드를 바꿀 때 배너와 제목이 바뀌는지는 시험하지 않았다. 코드상으로는 `RuntimeInfo` 와 `BackendMode` 변경 알림에 연결되어 있다.
- **CI:** 결과 없음.

## 5. 잔여 위험

- **자동화 보고서 `Passed`:** 대체 상황(Native 요청 → Mock)에서도 `Passed=True` 다. CI 의 gui-automation 단계는 Mock 을 요청하므로 영향이 없다. 다만 Native 를 요청하는 자동화 실행이 생기면 `ActualBackendMode` 를 따로 검사해야 한다. 관찰만 했고 고치지 않았다.
- **Mock 판정 기준:** `BackendName` 문자열 비교(`"RealXpeBackend"`)다. 이름이 바뀌면 모든 실행이 Mock 으로 표시된다. 경고가 사라지는 방향이 아니라 과하게 뜨는 방향이다. 이 경우 E-01d 의 Native 분기가 빨개진다.
- **S11 과 모든 UI 시험:** 이 기계에서는 다른 세션의 창이 앞에 있으면 흔들린다. CI 러너와는 조건이 다르다.
- **의도적 Mock 배너:** 모든 Mock 실행에 뜬다. 개발자에게는 소음일 수 있다. 등급 판단은 lead 몫이다.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| HAZ-GUI-005 (1)(2)(3) | GUI-C-82 (이 카드) |
| (4) 운영 GUI 대체 금지 — 적용 대상 없음 | lead 판단 (SHA 문구) |
| 자동화 보고서가 대체 상황에서도 Passed | 신규 카드 후보 |
| `Repeat-E2E.ps1` 종료 코드 (C-81) | 신규 카드 후보 (기존) |
