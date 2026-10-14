[2026-10-14 20:00 KST] GUI-C-244 보고서 — PR #266 Native 잡 멈춤(#268) 원인과 수정, W15 판정 개정 (Refs #251, #268)

기준 487a63ff 위의 새 커밋(GUI-C-243 의 477b3195 와 같은 기준 위에서 독립). 푸시는 리더가 한다.

## 1. #268: 멈춘 시험은 W9, 원인은 시험 스레드의 자기 교착

### 재현
- CI(run 38541801877)의 멈춘 시험 이름: `W9_SuggestedName_Overwrite_AndAWriteFailureKeepsThePreviousFile`(리더가 blame Sequence xml 로 확인). 순서는 W15 → W12 → W9.
- 로컬 실데이터 환경(CI 와 같은 변수)에서 **같은 순서의 전체 실행에서 재현**: W9 한 건이 37분 19초 걸린 뒤 통과했다(GUI-C-243 의 전체 실행, `.moai/reports/lane-gui/GUI-C-243/native_e2e_full.txt`). **W9 단독 실행도 재현**: `--blame-hang-timeout 6m` 로 hang 덤프 확보.

### 덤프로 확인한 것 (`testhost_stacks.txt`, `app_stacks.txt`, 수정 중간 단계 `*_stacks2.txt`)
- 앱 쪽: UI 스레드가 `MessageBox` 안(`SaveFileDialog.PromptUserIfAppropriate` ← `OnFileOk`), 즉 "파일이 이미 있습니다, 바꿀까요?" 확인창이 열린 상태.
- 시험 쪽: 시험 스레드가 UIA 호출(`FindAllDescendants`)에서 대기. 수정을 한 겹 하고 난 뒤의 덤프에서는 `Read("StatusBarText")` 대기.
- 시간순 로그(`w9_after_fix3.txt`): 두 번째 저장에서 `SendMessage(BM_CLICK)` 이후 어떤 줄도 나오지 않고, 워치독이 앱을 종료한 뒤에야 다음 줄이 나왔다.

### 원인
`DriveDialog` 가 확인 버튼 클릭을 `SendMessage(BM_CLICK)` 로 보냈다. SendMessage 는 상대 처리가 끝나야 돌아오는데, 덮어쓰기 저장은 그 클릭 처리 안에서 확인창을 모달로 연다. 그 확인창을 누르는 코드는 같은 시험 스레드의 다음 줄에 있었다 → 시험 스레드가 자기 자신을 기다리는 교착. 첫 저장(새 파일)은 확인창이 없어 통과하고, 두 번째 저장(같은 이름)부터 걸린다. 또한 확인창이 열린 동안 WPF 메인 창을 UIA 로 훑으면 앱의 디스패처가 확인창 안에 있어 응답하지 않는다(첫 수정 시도의 덤프에서 확인).

### 수정 (시험 코드만, `QuickStartWalkthroughScenarios.cs`)
1. 확인 클릭을 `PostMessage` 로(기다리지 않음).
2. 대화상자가 닫혔는지와 덮어쓰기 "예" 응답을 네이티브 창 호출(`EnumWindows`, `IsWindow`, `GetDlgItem`)만으로 처리. 메인 창 UIA 탐색은 확인창이 열려 있는 동안 하지 않는다.
3. 대화상자와 확인창이 모두 두 번 연속 사라졌을 때만 닫힘으로 본다. 상태가 바뀔 때마다 창 목록을 로그로 남긴다(`native (+N ms): …`).
4. **앱 워치독**(`AppWatchdog.cs`, 새 파일): `GuiApp` 마다 기본 600초(`XPE_E2E_APP_MAX_SECONDS` 로 조정) 뒤 `WATCHDOG` 줄을 남기고 앱을 종료한다. UIA 호출은 시험이 시간 한도를 걸 수 없어서 대상이 사라져야만 풀리기 때문이다. 시험은 그 예외로 실패하고 앱은 시험보다 오래 살지 않는다.

### 결과
- W9 단독 4회 연속 통과, 각 19초 (`w9_after_fix4.txt`, `w9_repeat.txt`). 수정 전: 같은 환경에서 3회 모두 5분 hang-timeout 까지 감(`w9_after_fix.txt`).
- 반증(`falsify.txt`): 확인 클릭을 다시 `SendMessage` 로 → W9 가 워치독에 걸려 실패(`watchdog_fired=True`); 복원 후 통과, 워치독 미발동.

### 리더가 물은 두 가지
① **같은 동기 클릭을 쓰는 다른 시험**
- E2E 프로젝트 전체에서 `SendMessage(… BM_CLICK …)` 호출은 수정 후 **1곳**: `Scenarios/Legacy/LegacyPreprocessReadinessScenarios.cs:762`(레거시 앱 파일 열기 대화상자의 [열기]). 존재하지 않는 파일 등으로 확인창이 뜨면 같은 교착이 이론상 가능하지만, 지금 시험은 유효한 파일을 열고 이번 실행에서 모두 통과했다. 이번 카드에서 바꾸지 않았다.
- UIA `.Invoke()` 호출은 81건(32개 파일, 전부 분류하지는 않았다). 대화상자를 여는 호출은 `InvokeMenuItemThenDialog`/`InvokeButtonThenDialog` 를 거치며 둘 다 `Task.Run` 으로 호출한다.
② **워치독의 위치**
- 시험 쪽에만 있다(`clients/ImageProcTest.E2ETests`). 시험이 띄운 앱 프로세스를 시험 프로세스가 종료시킨다. 제품 앱(`gui/`)은 이번 카드에서 바꾼 파일이 없다. `gui/` 의 종료 관련 코드는 기존의 정상 종료 호출 2건(`App.xaml.cs`, `MainWindow.xaml.cs`)뿐이라 사용자 경로에 영향이 없다.

## 2. W15 판정 개정 (`U8Verdict.Judge`)

규칙: 화면이 5회 모두 유효하게 관측되면(각 회 settled 이고 변화 검출) **화면 중앙값 ≤ 3000 ms**, 하나라도 아니면 **프록시 중앙값 ≤ 1500 ms**. 어느 쪽으로 판정했는지 출력한다.

경계 시험 (`U8VerdictTests`, 앱·데이터 없이 모든 러너에서 실행):
- 화면 관측 + 3000 통과 / 3001 실패
- 관측 불가 + 1500 통과 / 1501 실패
- 5회 중 1회라도 관측 불가면 프록시 규칙(프록시 2200 ms 는 실패)
- 화면이 관측되면 프록시는 판정하지 않는다(CI 사례: 프록시 2277, 화면 2464 → 통과)
- 중앙값이지 평균·최댓값이 아님, 순서 무관, 5개 아니면 거부

반증(`falsify.txt`): All→Any 로 바꾸면 1건 빨강, 화면 한도를 1500 으로 바꾸면 3건 빨강, 복원 후 초록.

로컬 W15(`w15_run.txt`): 화면 관측 불가(5회 모두) → 프록시 규칙, 중앙값 1138 ms ≤ 1500 → PASS. 화면 쪽 판정은 이 환경에서 실행되지 않았고 판정 함수 시험으로만 확인했다(CI 러너에서 화면이 읽힘).

## 3. 실데이터 Native E2E 전체 (CI 와 같은 변수, 전역 입력 미설정)

`native_e2e_full.txt`, `--blame-hang-timeout 10m`: 총 349건 중 **통과 331 / 실패 0 / 건너뜀 18**, 40.5분. 앞선 GUI-C-243 의 실행은 70분(W9 37분 포함)이었다.
- 통과: W1, W3, W7, W9, W11, W12, W13a, W13b, W14, W15, W16
- 건너뜀 18건: W10(`XPE-SKIP-ALLOWED:265`), 전역 입력(키·마우스)이 필요한 시험(`RealInputScenarios` 2, `ComparisonEntryPointScenarios.W14` 4 등), 분리 뷰어·뷰 상태 시험, 레거시 시작 측정 2건. GUI-C-243 의 전체 실행도 같은 18건이었고, 이번 카드에서 새로 건너뛴 시험은 없다(건너뜀 사유를 시험별로 대조하지는 않았다: 개수와 이름 목록만 대조).
- 메모리: 실행 전 5401 MB, 후 6325 MB. 실행 뒤 남은 ImageProcTest / testhost 프로세스 없음.

## 4. 한계 (입증하지 못한 것)

- CI 러너에서의 재현은 하지 못했다. 로컬에서 같은 순서·같은 변수로 재현·수정했고 CI 의 멈춤 시험 이름(W9)이 일치하지만, 리더 말대로 CI(관리자 권한, 다른 해상도)에서 닫히지 않는 경로가 이것 하나뿐이라는 보장은 없다. 워치독이 그 경우에도 이름 있는 실패(`WATCHDOG` 줄 + 시험 이름)로 끝나게 한다.
- 워치독이 끝낸 시험의 예외는 FlaUI 가 던지는 호출 실패 예외이고, `WATCHDOG` 줄은 시험 출력에 있다(예외 메시지 자체에 사유가 들어 있지는 않다).
- 워치독은 `GuiApp`(QuickStartWalkthroughScenarios.cs 의 하네스)에만 연결했다. `ApplicationFixture`·레거시 시험의 앱 실행 경로에는 연결하지 않았다.
- 레거시 파일 열기의 `SendMessage(BM_CLICK)` 1곳은 그대로다(위 ①).
- 화면 관측 갈래의 판정은 로컬에서 실행되지 않았다(이 세션은 WPF 화면을 읽지 못한다).
- 로컬 기존 변경(`.claude/scheduled_tasks.lock`, GUI-C-231/232 증거, 설정 백업 등)은 건드리지 않았다.

## Card Cross-Check

| 카드 항목 | card |
|---|---|
| 1. 멈춘 시험 찾기·원인 분리·시간 제한과 앱 종료 보장 | GUI-C-244 |
| 2. W15 판정 개정(화면 우선, 관측 불가일 때만 프록시)과 경계 시험 | GUI-C-244 |
| 산출: 로컬 실데이터 Native 전체 결과 | GUI-C-244 (3절) |
