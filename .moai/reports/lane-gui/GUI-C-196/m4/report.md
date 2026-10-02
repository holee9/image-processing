# GUI-C-196 M4 — 명령 · 백엔드 연결 · 메뉴 활성화 · 자동화 보고 · EI-0

증거: `../m4_falsification_arms.txt`, 이 폴더의 `gui_e2e_native_dicom.patch.txt`.

## 만든 것

| 파일 | 역할 |
|---|---|
| `Services/IBaselineBackend.cs` (새, internal) | 백엔드가 기준 명령에 내놓는 것: 지원 여부, 1회 실행, DICOM 세션. `IXpeBackend` 는 건드리지 않았다(DICOM 세션 타입이 internal 이라 public 인터페이스에 못 넣는다). Mock 은 구현하지 않으므로 명령이 비활성 |
| `Services/BaselineExecution.cs` (새) | 명령 전체에서 창을 뺀 것: 두 번 실행 → 판정 → (통과일 때만) DICOM → `baseline.json` → 한 줄 상태. WPF·네이티브 없음, 시험이 연결 |
| `Services/Native/XpeExposureIndexInterop.cs` (새) | `xpe_calc_exposure_index` 선언. M2 의 interop 파일은 건드리지 않으려고 따로 뒀다 |
| `Services/Native/GuiPreprocessRunner.cs` | 선택 인자 `measureExposureIndex`(기본 false). 켜면 float 영상(16비트로 내리기 전)에서 EI·DI 를 재서 단계 요약에 "uncalibrated EI (보정 안 된 EI) = …" 로 붙인다 |
| `Services/RealXpeBackend.cs` | `IBaselineBackend` 구현(명시적). `RunChain` 은 `RunChainCore(..., measureExposureIndex: false)` 로 위임해 평소 Apply 는 그대로. 기준 전용 표시 단계 `RunBaselineDisplay` 는 고정값만 쓰고 최종 uint16 을 돌려준다 |
| `ViewModels/MainWindowViewModel.cs` | `RunDeterministicBaselineCommand`, `CanRunDeterministicBaseline`, `LastBaselineResult`, `BaselineStatusText`, `RunDeterministicBaselineAsync` |
| `MainWindow.xaml` | 메뉴 항목에 `Command` + `IsEnabled="{Binding CanRunDeterministicBaseline}"`, 툴팁은 설계 메모 §9 초안 |
| `MainWindow.xaml.cs` | 비활성 목록에서 이 항목 제거, 자동화 보고서 단계 추가 |
| `Models/GuiAutomationReport.cs` | `BaselineMenuEnabled/Ran/Status/StatusText/BitIdentical/FirstDifference/OutputSha256/StageTimes/TotalMs/DicomValid/DicomRoundTripIdentical/ExposureIndex/EvidenceFolder` |

M4 가 건드린 기존 시험·보조 파일: `BackendLifecycleTests`(멤버 표에 두 항목 추가 — 새 멤버가 `_backend` 를 쓰면 그 표를 요구한다), `SelfCheck/LifetimeScenarios.cs`(전환 중 거절 시나리오에 "Deterministic Baseline" 추가), `ApplicationFixture.cs`(`SharedCalibrationSet` 을 private→internal 한 줄).

## 결정한 것 (리더 확인 필요한 것에 표시)

- 명령은 **어떤 설정도 쓰지 않고**(`Settings.Snapshot()` 읽기만) 화면의 영상·`ChainStatus`·`PipelineTimings`·`LastChain` 을 건드리지 않는다. 시험이 이 금지 목록을 소스에서 고정한다.
- 수명: `TakeTicket()`(세대만), `TakeRequestTicket()` 아님 — 진행 중인 Apply 를 밀어내지도 밀려나지도 않는다.
- 증거 폴더: `evidence/<RunId>/baseline-<n>/{baseline.json, baseline.dcm}`. `RunId` 가 비어 있으면 `adhoc-<시각>`. 기준 실행이 `LastEvidenceFolderPath`(증거 폴더 열기가 보는 값)를 **바꾸지 않는다**.
- 통과는 판정 **그리고** DICOM 이 모두 통과일 때만. 판정이 실패면 DICOM 은 쓰지 않는다(D5).
- **[리더 결정 요청]** 자동화 보고서의 `Passed` 에 기준 결과를 **넣지 않았다**. Native 자동화 실행에서 기준이 Fail 이어도 `Passed` 는 그대로다. 넣으면 보고서의 의미가 바뀌어 기존 CI 판정에 영향이 가므로 따로 정하는 것이 맞다고 보았다.
- 자동화 실행 중 기준은 **전처리가 돌았을 때만** 시도한다. 캘리브레이션이 없는 Native 자동화에서는 기준이 "캘리브레이션이 없어서" 실패하는데 그것은 기준의 판정이 아니므로 `NotRun` + "not attempted … no calibration set" 로 기록한다(시험 A20).
- `FaultInjectingBackend`(테스트 빌드에서 결함을 건 경우)는 `IBaselineBackend` 를 전달하지 않는다. 그 구성에서는 기준 명령이 비활성이다.

## 실제로 돌려 본 것 — 전체 경로 (로컬)

로컬에서 Native 를 돌릴 수 있었다(이전 메모의 "Native 는 로컬에서 안 된다"는 이 구성에서는 틀렸다): `xpe-gui/build/ci-common/bin`(9/26 빌드: common·preprocess·display·gsvg·enhance_basic·enhance_advanced) + `xpe-post` 의 `xpe_dicom.dll`(10/2) + 그 빌드의 vcpkg 런타임을 **덮어쓰지 않고** 추가한 디렉터리(43개). `xpe_ai.dll` 은 없다. 이 DLL 세트는 CI 가 만드는 것이 아니라 대역이다.

A19(앱을 띄워 메뉴를 눌러 기준을 실행, 합성 1024×1024, `xpe_calib_fixture_gen` 의 캘리브레이션 세트):

```
BASELINE: Deterministic Baseline PASS: two runs bit-identical (1024x1024, 290 ms; DICOM valid)
MEASURED times: run1: preprocess=49 ms, enhance_basic=57 ms, total 115 ms | run2: preprocess=60 ms, enhance_basic=50 ms, total 117 ms; total 290.1 ms (budget 3000 ms, not asserted)
EI: uncalibrated EI (보정 안 된 EI) = 3098.64, DI = 10.93 (measured, not a pass criterion; ...)
```

(같은 시험을 앞서 한 번 더 돌렸을 때 237 ms.) `baseline.json`·`baseline.dcm` 이 증거 폴더에 있는 것도 시험이 확인한다.

- **3000 ms**: 이 기계·1024²·합성 영상에서 전체가 약 240~290 ms 다. 단언하지 않았다. 실제 3072² 영상에서의 시간은 **재지 않았다**(M2 에서 enhance 단계만 3072² 에서 약 300 ms/회였고, 전처리까지 포함한 전체는 미측정).
- **EI/DI 관찰**: EI 3098.64, DI 10.93. DI 가 3 을 넘으므로 헤더 말대로면 모듈이 경고를 올린다(알림이 실제로 떴는지는 **관찰하지 않았다**). 이 수치는 모듈의 기준값 S0 = 1000 이 이 합성 영상·gui 게인 척도와 맞는다는 근거가 아니며, 그래서 화면과 보고서에 "보정 안 된"이라고 적었다. 판정에는 쓰지 않는다.
- A19 의 첫 시도(캘리브레이션 세트를 주지 않음)는 `Fail: run 1: stage preprocess was RequestedNotApplied: … calibration file(s) not found` 였다. 모듈이 전처리를 거절하면 다음 단계는 입력 그대로 돌고(enhance_basic 36 ms) 판정이 이를 실패로 읽는다 — D5 가 실제 모듈에서 의도대로 작동한 사례다. 이 일이 자동화에서 "기준을 시도하지 않음" 규칙(A20)을 만들었다.

## 시험

- Functional: 기본 환경 **487 통과·0 실패·3 건너뜀**, 네이티브 디렉터리 지정 환경 **489 통과·0 실패·1 건너뜀**. 새 `BaselineExecutionTests` 14건(두 번 실행·증거 파일·EI 라벨·판정 실패 시 DICOM 없음·거절·예외·유효하지 않은 DICOM·세션 없음·증거 폴더 불가·예산 비단언 + 소스 대조 5건).
- SelfCheck: 16 시나리오 통과(전환 중 "Deterministic Baseline" 거절 포함). `gui/ImageProcTest.E2E` 계약 러너 통과.
- E2E(`AutomationReportBackendTests`): 기본 환경 **17 통과·0 실패·3 건너뜀**(8분). 새 A18(Mock: 기준은 NotRun, 메뉴 비활성), A19·A20(Native). 로컬 네이티브 환경에서 A18·A19·A20 모두 통과. 세 가지 계산(`DisabledFutureCommandCount`, 메뉴 순회, XAML 선언)이 계속 일치함은 A11 이 통과로 확인한다.
- 반증 14건(`../m4_falsification_arms.txt`): 전부 빨강, 바이트 동일 복원, 복원 후 Functional 489/0 · E2E A18·A19·A20 통과. 설정 변경, Apply 번호표 사용, 화면 영상 교체, 표시 단계의 사용자 설정 읽기, 평소 체인의 EI 측정, 메뉴 재비활성, 전환 중 거절 제거, 비활성 목록 잔류, 예산의 단언화, DICOM 실패 무시, 실패 시 DICOM 쓰기, EI 라벨 제거(Native), Mock 보고 오기(Mock), 캘리브레이션 없는 시도(Native).

## CI 에서 일어날 일 — 리더가 같은 푸시에 넣어야 하는 것

`gui-e2e-native` 잡은 "설명 없는 건너뜀은 실패"로 판정하는 게이트가 있고(ci.yml 의 `Assert no unexplained skipped Native E2E cases`), 이 잡은 `xpe-ci-{common,preprocess,post}-binaries` 만 `build/e2e-native-dlls` 에 놓는다. `xpe_dicom.dll` 이 없으면 **A19 가 건너뜀으로 끝나고 이 게이트가 잡을 빨갛게 만든다.**

`gui_e2e_native_dicom.patch.txt`(현재 main `ccae7c45` 기준 `git apply --check` 통과, 적용 결과 대조, YAML 파싱 통과): `needs` 에 `dicom-build`, `xpe-ci-dicom-binaries` 를 내려받아 "없는 이름만 복사"(M3b 규칙), 검증 목록에 `xpe_enhance_basic.dll`·`xpe_dicom.dll`·`dcmdata.dll`·`openjp2.dll`. dotnet-tests 쪽 M3b 패치는 이미 리더가 쓰기로 했으므로 여기에 다시 넣지 않았다.

## 미검증·한계

1. CI 의 Native 잡에서 A19 가 통과하는지는 **미검증**이다(위 DLL 세트는 대역이고 CI 의 vcpkg 트리·빌드 시점이 다르다).
2. 사람이 메뉴를 눌렀을 때 상태 줄·알림이 화면에서 어떻게 보이는지는 **보지 않았다**. 자동화는 메뉴 항목을 프로세스 안에서 눌러 명령을 실행한다.
3. 3072² 실영상에서의 전체 시간, 한글이 든 경로에서의 DICOM 쓰기, 새 프로세스로 두 번 돌리는 판정(D8 은 후속으로 남김)은 미검증이다.
4. "Export Evidence Snapshot" 번들이 `baseline-*/` 를 함께 담는지는 확인하지 않았다(설계 §8 의 미확인 그대로).
5. DI 경고 알림이 실제로 올라오는지, 그것이 화면 알림 목록에 어떻게 나오는지는 관찰하지 않았다.
6. 기준 명령은 같은 프로세스 안에서 두 번이다. 프로세스 전역 상태·캐시에 의존하는 비결정성은 이 판정이 잡지 못한다(D8).

## M5 로 남긴 것

실제 E2E 시나리오(FlaUI 로 메뉴를 사람처럼 누르는 것 포함)와 측정 로그 정리, 리더가 고치는 MENU-001·툴팁 반영 확인.
