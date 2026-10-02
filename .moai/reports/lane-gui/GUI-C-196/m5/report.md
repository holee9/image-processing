# GUI-C-196 M5 — 보고서 판정 규칙 · FlaUI E2E(UIA 전용) · 측정 로그

증거: `falsification_arms.txt`(이 폴더). M4 의 증거는 `../m4/`, `../m4_falsification_arms.txt`.

## 리더 결정 4 의 반영

자동화 보고서의 `Passed` 에 기준 결과를 넣었다: **기준 `Fail` 이면 `Passed=false`, `NotRun` 이면 불변.** 규칙은 새 파일 `Services/BaselineAutomationRule.cs` 한 곳이고(WPF 없음), `MainWindow.xaml.cs` 의 보고 단계와 `Passed` 식이 같은 규칙을 쓴다.

| 상황 | `BaselineStatus` | `Passed` |
|---|---|---|
| 명령이 결과를 냈고 통과 | `Pass` | 불변 |
| 명령이 결과를 냈고 실패(비교 불일치, 거절, DICOM 실패 등) | `Fail` | **false** |
| 메뉴가 비활성(Mock) | `NotRun` | 불변 |
| 전처리가 안 돌아서 시도하지 않음(캘리브레이션 없음) | `NotRun` + "not attempted…" | 불변 |
| **시도했는데 결과가 없음**(던졌거나, 결과가 버려졌거나, 60초 안에 끝나지 않음) | **`Fail`** | **false** |

마지막 줄이 이번에 추가한 판단이다: 시도하고 결과가 없는 것을 `NotRun` 으로 두면 멈춤이나 예외가 "아무 일도 없었음"으로 통과한다. 그래서 `NotRun` 은 "시도하지 않음"에만 쓴다. 기다림도 명령 자신의 실패 줄(`Deterministic Baseline FAIL…`)에서 끝나므로 예외로 끝난 명령을 1분 기다리지 않는다.

시험:

- `BaselineAutomationRuleTests` 11건(Functional): 결과의 통과/실패, 미시도 → NotRun, 시도·무결과 → Fail, `Pass`·`NotRun` 만 허용(대소문자·빈 값·모르는 값·null 은 불허), 그리고 소스 대조(검증식이 규칙을 쓰고, 기준 단계가 검증식보다 앞이고, 기다림 조건이 실패 줄에서 끝남).
- **A21(Native E2E, 진짜 실패)**: 앱을 `xpe_dicom.dll` 만 뺀 네이티브 디렉터리에 묶어 실행한다. 두 실행은 일치하고 DICOM 쓰기가 불가능하다 → `BaselineStatus=Fail`, `BaselineBitIdentical=true`(실패한 것은 파일), `Passed=false`, 종료 코드 1. 실제 출력: `Deterministic Baseline FAIL: DICOM export: xpe_dicom_write threw: DllNotFoundException: …(0x8007007E)`.

## FlaUI E2E — `DeterministicBaselineScenarios` (UIA 패턴만)

사용한 것은 `Expand`/`Invoke`/`IsChecked`/`Name`/`HelpText`/`IsEnabled` 뿐이다. `Keyboard`·`Mouse`·`GlobalInput` 은 쓰지 않았다(리더 터미널로 입력이 간 사고 때문 — 새 파일에서 `Keyboard|Mouse|GlobalInput` 을 검색하면 1건이 나오고 그것은 "쓰지 않는다"고 적은 머리 주석이다).

| 시험 | 환경 | 내용 |
|---|---|---|
| B01 | Mock·Native | 항목 이름에 "Deterministic Baseline", `IsEnabled == (백엔드가 Native)` |
| B02 | Native | 항목을 Invoke → 상태 줄이 `Deterministic Baseline PASS: two runs bit-identical … DICOM valid`. 실행 전후로 **같아야 하는 것**: 뷰포트가 그린 픽셀 해시, 체인 상태 줄, Parameters 탭의 전처리·AI 스위치. 클릭에서 통과 줄까지의 시간을 기록 |

로컬 결과: Mock 에서 B01 통과(`enabled=False`), B02 는 건너뜀(사유 출력). Native(대역 DLL 세트)에서 B01 `enabled=True`, B02 통과:

```
B02 before: chain='chain: preprocess=NotRequested, gsvg=NotRequested, ai_bone_suppress=NotRequested; …; display input=raw' hash=9e58cd6029598dfa preprocess=False ai=False
B02 status: 'Deterministic Baseline PASS: two runs bit-identical (1024x1024, 210 ms; DICOM valid)'
B02 MEASURED click to pass line: 821 ms (budget 3000 ms, not asserted; this includes the UI Automation polling interval)
```

체인 상태가 실행 뒤에도 `preprocess=NotRequested` 로 남는 것이 의미 있는 관찰이다: 기준 명령은 안에서 전처리를 실행하지만 화면의 체인 상태·설정은 건드리지 않는다.

## 측정 로그 (3000 ms 는 단언하지 않음)

| 측정 | 값 | 조건 |
|---|---|---|
| 기준 명령 내부 전체(두 번 + DICOM) | 210 ms, 237 ms, 290 ms(세 번의 로컬 실행) | 1024², 합성, 로컬 대역 DLL 세트 |
| 한 번 실행 | 약 115~117 ms(전처리 31~60 + enhance 39~57) | 같음 |
| UIA 클릭 → 통과 줄 | 821 ms | 폴링 간격 포함 |

모두 이 기계 한 대의 값이고 CI 러너의 값이 아니다. 3072² 실영상에서의 전체 시간은 **재지 않았다**(M2 에서 enhance 단계만 3072² 에서 약 300 ms/회).

## 검증 요약

- Functional: 기본 환경 **498 통과·0 실패·3 건너뜀**, 네이티브 디렉터리 지정 환경 **500 통과·0 실패·1 건너뜀**.
- E2E `AutomationReportBackendTests`+`DeterministicBaselineScenarios` 기본 환경 **18 통과·0 실패·5 건너뜀**(Native 전용 5건: A19·A20·A21·B02 + 이전부터 있던 1건). 로컬 네이티브 환경에서 A19·A20·A21·B01·B02 통과.
- SelfCheck 통과, `gui/ImageProcTest.E2E` 계약 러너 통과.
- 반증 9건 전부 빨강 후 바이트 동일 복원: 규칙의 Fail→Pass 오기, 시도·무결과의 NotRun 처리, 검증식에서 기준 항 제거(+ A21 빨강), 기다림 조건 제거, DICOM 실패 무시(A21), 명령이 설정을 씀·그린 영상을 바꿈·체인 상태를 가로챔(B02), 메뉴가 항상 활성(B01/Mock). B02 의 세 반증은 각각 의도한 단언(설정 값, 그림 해시, 체인 문자열)에서 빨개진 것을 메시지로 확인했다.
- 작업 중 한 번 모든 E2E 가 1 ms 로 실패했다: 반증 스크립트가 마지막 변형을 빌드한 채 소스만 되돌려 앱 신선도 가드가 걸린 것이다. 다시 빌드하고 전부 재실행해 통과를 확인했다(코드 결함 아님).

## CI 에 대하여

- `gui-e2e-native` 잡에 새로 도는 시험이 늘었다: A19·A20·A21(각 약 25~65 s, 캘리브레이션 생성 53 s 는 한 테스트 프로세스 안에서 공유)·B01·B02. 잡 제한 30분 대비 추가는 약 2~3분으로 **추정**한다(CI 러너에서 재 본 값이 아니다).
- 위 시험들은 `xpe_dicom.dll` 이 그 잡에 stage 되어야 한다(M4 의 `gui_e2e_native_dicom.patch.txt`). 빠지면 A19·A21 이 건너뜀 게이트에 걸린다. B01 은 Mock 잡에서도 돈다(항목이 비활성인지 확인).
- 이 푸시 이후 Mock 잡에서 B01 이 통과하는 것은 로컬 Mock 으로 확인했다. CI 에서 확인한 것은 아니다.

## 미검증·한계

1. CI 의 Native 잡에서 A19·A20·A21·B02 가 통과하는지는 미검증이다(DLL 세트가 대역).
2. 사람이 마우스로 메뉴를 눌러 화면에서 보는 모습(상태 줄 갱신, 알림 목록)은 보지 않았다. B02 는 UIA Invoke 로 같은 명령을 실행하고 상태 줄 텍스트를 읽는다.
3. B02 는 DICOM 파일을 직접 열지 않는다(A19 가 보고서 경로로 확인). 알림 목록에 `BASELINE_PASS` 가 뜨는지, DI>3 경고가 뜨는지는 관찰하지 않았다.
4. 한글 경로, 3072² 실영상 전체 시간, 새 프로세스 두 번(D8)은 미검증이다.
5. `Export Evidence Snapshot` 번들이 `baseline-*/` 를 담는지는 확인하지 않았다.
6. `MENU-001` 문서·툴팁 문구 반영은 리더 몫이다. 툴팁은 M4 에서 설계 메모 §9 초안으로 이미 코드에 들어갔다.
