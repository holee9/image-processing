# GUI-C-83 보고 — 자동화 보고서의 Mock 대체 (#175)

커밋(dev/gui, 미푸시): 6e6c8fa

## 0. 구현 전 확인 — CI 영향

`origin/main:.github/workflows/ci.yml` 을 확인했다.
- 378행: `gui-automation` 잡은 `'--automation-backend', 'Mock'` 을 요청한다. 따라서 이번 변경으로 CI 결과가 바뀌지 않는다. 멈출 조건(Native 요청인데 CI 에서 Mock 으로 대체되는 경우)에는 해당하지 않는다.
- 384행: 게이트는 `BackendModeSource -ne 'arg'` 와 `-not $r.Passed`, 그리고 종료 코드를 검사한다.

그 밖에 자동화 모드로 앱을 실행하는 곳(`git grep automation-report origin/main -- .github clients tools`):
- `tools/e2e/Invoke-ImageProcTestGuiRealE2E.ps1:48` 은 `--automation-backend` 없이 실행한다. 이 경우 요청 모드는 설정 파일에서 온다. lead 소유 경로이며 수정하지 않았다.

## 1. 주장

1. `GuiAutomationReport.BackendMatchesRequest` 를 추가했다. 요청 모드(`BackendMode`)와 실제 모드(`ActualBackendMode`)가 같을 때만 참이다. 비교는 대소문자를 구분하지 않는다.
2. `Passed` 는 이제 `BackendMatchesRequest` 가 참이어야 참이 된다.
3. 시험 결과:
   - A-01 (Native 요청 + 빈 DLL 폴더): `Passed=False`.
   - A-02 (Mock 요청, 대조군): `Passed=True`.
   - `Passed` 조건에서 이 항목을 빼면 A-01 만 실패한다.

## 2. 증거 (이 디렉터리)

- `green.txt`: 통과 2.
  - `A01 exit=0 BackendMode=Native ActualBackendMode=Mock BackendMatchesRequest=False Passed=False`
  - `A02 exit=0 BackendMode=Mock ActualBackendMode=Mock BackendMatchesRequest=True Passed=True`
- `falsify.txt`: 조건을 주석 처리한 결과 통과 1 / 실패 1. A-01 만 실패했고 이때 `Passed=True` 였다.
- `automation-native.json`: Native 요청 + `build/ci-common/bin` 으로 실행한 결과 `exit=0 Passed=True BackendMode=Native Actual=Native Matches=True`. 실제 DLL 이 있으면 Native 자동화 실행도 통과한다.
- `full-mock.txt`: Mock E2E 전체 실패 0 / 통과 84 / 건너뜀 1 (`NativeProvenance`).
- `full-int.txt`: 통합 실패 0 / 통과 209 / 건너뜀 1.
- 빌드: 경고 0 / 오류 0.
- 재현: C-82 의 `automation-report-fallback.json` 이 수정 전 상태를 측정해 두었다. 같은 조건에서 `Passed=True` 였다.

## 3. 기준선 귀속

이 세션의 dev/gui 작업본에서 측정했다. 전체 실행은 6e6c8fa 커밋 직전 작업본에서 했고, 커밋과 내용이 같다. Native DLL 은 기존 `build/ci-common/bin` 을 사용했다.

## 4. 미검증

- **Native 환경의 E2E 스위트:** Native 로 이 시험들을 돌리지 않았다. A-01, A-02 는 백엔드를 스스로 고정하므로 환경 변수의 영향을 받지 않는다.
- **`tools/e2e` 스크립트:** 이 변경 뒤 동작을 실행해 보지 않았다. 설정 파일 모드가 Native 이고 DLL 이 없으면, 이제 그 실행은 실패로 보고된다.
- **창 숨김:** 시험이 앱을 실행할 때 `WindowStyle=Hidden` 을 쓰지만 `UseShellExecute=false` 여서 숨김이 적용되는지는 확인하지 않았다. 창이 잠시 보일 수 있다.
- **CI:** 결과 없음.

## 5. 잔여 위험

- **종료 코드:** 앱은 `Passed=False` 여도 0 으로 끝난다(A-01 `exit=0`). CI 게이트는 `Passed` 도 읽으므로 막히지만, 종료 코드만 보는 호출자는 이 실패를 놓친다.
- **판정 기준:** `ActualBackendMode` 는 C-82 와 같은 `BackendName` 문자열 기준이다. 그 한계가 그대로 이어진다.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| 자동화 보고서 대체 판정 | GUI-C-83 (이 카드) |
| 자동화 실패 시 종료 코드 0 | 신규 카드 후보 |
| `tools/e2e` 가 요청 모드를 지정하지 않음 | lead 판단 (tools 소유) |
