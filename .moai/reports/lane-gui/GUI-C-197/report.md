# GUI-C-197 — 문서 초안: 출하 빌드의 시험 스위치 · Run Deterministic Baseline

문서는 리더 소유라 **초안만** 냈다(문서·코드 변경 없음). 모든 "현재 문장"은 리더의 `main`(`4e7902b2`, 로컬 커밋 기준)에서 `git grep`/`git show` 로 읽은 것이고, 근거 grep 전부가 `greps.txt` 에 명령과 함께 들어 있다. 이 기준에는 리더의 작업 트리에만 있는 미커밋 문서 수정은 **포함되지 않는다**.

## 1. 출하 빌드의 시험 스위치

### 1.1 문서가 이 스위치를 어디서 서술하는가 (grep 결과)

- 스위치를 **이름으로** 서술하는 문서는 **0곳**이다(`--automation-fault`, `display-pipeline-after`, `ai-worker-*`, `XPE_TEST_FAULTS`, `XPE0001`, `FaultInjecting`, `XpeTestFaults` 를 `docs/`·gui README·참조 README 에서 찾아 무일치 — `greps.txt` §1).
- 그 기능을 **이름 없이 주장하는** 문장이 1곳 있다: `SHA-GUI-001:110` ("결함 주입 장치가 일반 실행에서 비활성 | 명령줄 전용 | W-24"). 이 문장이 지금의 사실과 어긋난다 — 장치는 명령줄 전용일 뿐 아니라 **출하(Release) 빌드에는 아예 존재하지 않는다**.
- 그에 딸린 추적·절차 문장이 3곳(RTM-GUI-001:178, E2E-001:13, gui README Build 절).
- 인접 오기 1건(ARCH-001 §9.2).

### 1.2 사실표 (초안이 인용하는 것, 근거는 코드·CI 이름)

| 사실 | 근거 |
|---|---|
| 시험용 스위치는 `--automation-fault` 의 값 4가지: `display-pipeline-after:N`, `ai-worker-disabled`, `ai-worker-silent`, `ai-worker-silent:0` | `AutomationArgs.cs:194·198·202·206` |
| 이 스위치의 파서 분기·`FaultInjectingBackend`·제목 표식은 `XPE_TEST_FAULTS` 가 정의된 빌드에만 컴파일된다. 기본값은 **Debug 구성에서만** 정의 | `gui/XpeTestFaults.props:1-14`, `AutomationArgs.cs` 의 `#if XPE_TEST_FAULTS` |
| Release 에서 `-p:XpeTestFaults=true` 로 억지로 켜면 컴파일 전에 **빌드 오류 `XPE0001`** | `XpeTestFaults.props:21-24` |
| 출하 exe 는 이 스위치를 "인식 못 하는 automation 스위치"로 **거절**하고 종료 코드 **2**(`InvalidAutomationArgsExitCode`)로 끝난다 | `AutomationArgs.cs` 말미의 `else` 분기, `App.xaml.cs:10-11`, `Test-ShippedBuild.ps1` 4단계 |
| CI 잡 `gui-shipped-build` 가 `tools/ci/Test-ShippedBuild.ps1` 로 (1) Debug 에서 문자열을 **찾는** 양성 대조 (2) Release dll/pdb/xml 에 문자열 부재 (3) `XpeTestFaults=true` 가 XPE0001 로 거절 (4) 출하 exe 가 네 값을 모두 종료 코드 2 로 거절 (5) 포장본이 있으면 그것도 스캔 | `ci.yml:763-784`, `Test-ShippedBuild.ps1` |
| 모든 다른 CI 잡과 E2E 픽스처는 Debug 를 빌드·실행한다(그래서 시험 스위치가 계속 쓰인다) | `XpeTestFaults.props` 머리 주석 |
| W-24 가 확인하는 것은 **Debug 빌드의 일반 실행에 결함이 없음**이지 Release 에 장치가 없음이 아니다 | `ViewportTruthScenarios.cs:216-230`(E2E 는 Debug 실행) |

### 1.3 위치별 초안

| # | 위치 | 현재 문장 | 바꿀 문장 | 근거 |
|---|---|---|---|---|
| 1 | `docs/post-processing/xpe/SHA-GUI-001_Software_Hazard_Analysis.md:110` (HAZ-GUI-004 통제 표의 마지막 행) | `> \| 결함 주입 장치가 일반 실행에서 비활성 \| 명령줄 전용 \| W-24 \|` | `> \| 결함 주입 장치가 일반 실행에서 비활성이고, **출하(Release) 빌드에는 존재하지 않음** \| 시험 빌드(Debug)에만 컴파일(`XPE_TEST_FAULTS`), 명령줄 전용(`--automation-fault`). Release 에서 `XpeTestFaults=true` 는 빌드 오류 `XPE0001`, 출하 exe 는 스위치를 거절(종료 코드 2) \| W-24(Debug 일반 실행에 결함 없음), CI `gui-shipped-build` (`tools/ci/Test-ShippedBuild.ps1`: 양성 대조·Release 문자열 부재·XPE0001·종료 코드 2) \|` | §1.2 의 처음 다섯 행. 사용자 결정 GUI-C-193(2026-10-02, `XpeTestFaults.props` 머리 주석) |
| 2 | `docs/post-processing/xpe/RTM-GUI-001_Requirements_Traceability_Matrix.md:178` (HAZ-GUI-004 행) | `… — 이전 표기 "W-06/W-07" 의 W-06 은 존재하지 않았음 (#171) \|` | 같은 행 끝에 덧붙임: `… (#171); 이 통제들을 증명하는 결함 주입 장치는 출하 빌드에 없음 — CI \`gui-shipped-build\` (GUI-C-193) \|` | 1 번과 동일. 시험 장치가 출하물에 없다는 추적을 W-20~W-26 옆에 둔다 |
| 3 | `docs/project/XPE-GUI-E2E-001_FlaUI_E2E_Plan.md:13` | `… 실행 절차의 기준은 \`.github/workflows/ci.yml\` 의 \`gui-automation\` / \`gui-e2e-native\` 잡입니다.` | `… 기준은 \`ci.yml\` 의 \`gui-automation\` / \`gui-e2e-native\` 잡입니다. E2E 는 시험 스위치가 들어 있는 **Debug** 빌드를 띄우며, 출하(Release) 빌드에 그 스위치가 없다는 확인은 별도 잡 \`gui-shipped-build\` 가 합니다.` | `ci.yml:763-784`, `XpeTestFaults.props` |
| 4 | `gui/ImageProcTest/README.md:36-40` (`## Build`) | 코드 블록 한 줄 `dotnet build gui\ImageProcTest\ImageProcTest.csproj -c Debug` 만 있고 구성 설명이 없다. README 에서 `Release` 는 219행의 무관한 문장 하나뿐(`greps.txt` §3) | 코드 블록 아래에 문단 추가(영문): `The Debug build also compiles the test fault switches (--automation-fault display-pipeline-after:N, ai-worker-disabled, ai-worker-silent, ai-worker-silent:0) that the E2E suite uses. A Release (shipped) build does not contain them: the parser has no branch for them, so the exe refuses the switch (exit code 2), and Release with -p:XpeTestFaults=true fails with XPE0001. The CI job gui-shipped-build checks this.` | §1.2 |
| 5 | `docs/project/XPE-GUI-ARCH-001_XAML_MVVM_Architecture.md:367-368` (§9.2) — **인접 오기, 리더 판단** | `` `ImageProcTest.exe --automation` 플래그로 실행 시 자동 스크립트 모드 진입 `` / `` 인자로 YAML 시나리오 파일 지정 가능 (`--scenario smoke.yaml`) `` | 코드에는 `--automation`·`--scenario` 가 **없다**(`greps.txt` §4: `gui`·`clients` 에서 정확 문자열 무일치). 실제 스위치는 `--automation-raw/-report/-backend/-export-render/-settings/-calib/-width/-height` 이고 모르는 `--automation-*` 는 거절된다(`AutomationArgs.cs` 126-183·말미). 제안: `` `--automation-*` 스위치로 실행하면 자동화 모드에 진입한다(raw·report·backend·export-render·settings·calib·width·height). 모르는 `--automation-*` 스위치는 종료 코드 2 로 거절된다. 시험용 \`--automation-fault\` 는 Debug 빌드에만 있다.`` + YAML 시나리오 줄 삭제 | `AutomationArgs.cs` |

참고: `docs/design/reference/gui-README.md` 는 참조본이고 시험 스위치가 없어 수정할 곳이 없다. `XPE-GUI-MENU-001` 에도 스위치 서술이 없다.

**리더 판단이 필요한 한 가지**: "출하 빌드에 시험 장치가 없음"을 위해 항목(통제)으로 올릴지, 새 위해 ID 를 줄지는 안전 문서 소유자의 일이라 초안은 기존 HAZ-GUI-004 통제 표의 한 행으로만 고쳤다. 새 위해·ID 를 만들지는 않았다.

## 2. Run Deterministic Baseline

### 2.0 용어 주의

문서들은 "deterministic baseline" 을 **제품의 비-AI 영상 경로**(`XPE-PRD-002:253` PRD-PROG-004, `XPE-AI-REG-001:61` 등, `greps.txt` §5)라는 뜻으로 쓴다. 메뉴 `Run Deterministic Baseline` 이 하는 일은 그 경로의 **Phase 1b 체인이 결정적인지 확인**하는 것이다(같은 입력으로 두 번 돌려 출력이 비트 단위로 같은지). 혼동을 막으려고 아래 초안에 한 문장을 넣었다.

### 2.1 `XPE-GUI-MENU-001` §8 표 (231행) — 한 행 교체

| | 문장 |
|---|---|
| 현재 | `\| \`Run Deterministic Baseline\` \| xpe_preprocess.dll \| 1a \| Same + deterministic test mode active \|` |
| 바꿀 | `\| \`Run Deterministic Baseline\` \| xpe_preprocess.dll, xpe_enhance_basic.dll, xpe_display.dll, xpe_dicom.dll \| 1b \| Native backend. At run time: raw image loaded, calibration set present. \|` |
| 근거 | 활성 조건은 코드상 `CanRunDeterministicBaseline => backend is IBaselineBackend { SupportsDeterministicBaseline: true }`(Native 백엔드)뿐이다. 영상이 없으면 명령이 경고 알림 `BASELINE_NO_IMAGE` 를 내고 돌지 않으며, 캘리브레이션이 없으면 명령이 **실패**한다(전처리 거절은 기준 실패 — D5). "deterministic test mode" 라는 모드는 코드에 없다 |

**표 아래 `Rules` 와의 어긋남(리더 판단)**: 8절 첫 문장은 "owner module DLL이 존재하고 `IsNativeReady` 일 때만 enabled" 라고 쓴다. 이 명령은 **DLL 별 준비 상태를 확인하지 않고** Native 백엔드이면 활성이며, DLL 이 빠지면 실행 시 실패한다(A21 이 `xpe_dicom.dll` 부재에서 실제로 `Fail` 을 확인). 문서를 코드에 맞출지 코드를 문서에 맞출지(M7 후보)는 리더가 정한다.

### 2.2 툴팁

(현재 코드에 이미 들어 있는 최종 문구; `MainWindow.xaml` 의 항목 `RunDeterministicBaselineMenuItem`.) 설계 메모 §9 초안에서 "Changes no setting." 를 더했다 — M4 의 동작(설정·화면 영상·체인 상태를 바꾸지 않음)을 시험이 고정한다.

`Runs the fixed Phase 1b chain (preprocess, enhance_basic, display LUTs, DICOM write) twice on the loaded image and reports whether the two outputs are bit-identical. Needs the native backend and a loaded image. Changes no setting.`

### 2.3 `gui/ImageProcTest/README.md` — 새 절 초안 (영문)

`## Run Deterministic Baseline (Pipeline menu)` 를 "Automation E2E" 절 앞에 추가:

> Runs the Phase 1b chain **twice** on the loaded raw image, in the same process, and reports whether the two outputs are bit-identical. Native backend only; the menu item is disabled on Mock. "Deterministic" here is the property being checked, not a mode.
>
> **Chain (fixed; no user setting is read).** preprocess (offset, nonlinearity, gain, defect; uncalibrated EI measured on its float image) → enhance_basic (log, bilateral noise reduction σspace 3.0 / σrange 50.0, CLAHE clip 3.0 with 8×8 tiles, unsharp mask amount 0.5 / radius 2.0 / threshold 10.0, all on one float image, converted to 16 bits once at the end with round-half-even and clamping) → display (modality slope 1 / intercept 0, linear VOI centre 32768 / width 65535, GSDF off). The log normalisation factor is 65535/log10(65536); no document specifies it, so it is an assumption of this command only. Every stage must be applied: a stage the module refuses fails the command (nothing is compared and no DICOM file is written).
>
> **Pass criteria (all of them).** (1) both runs applied every stage; (2) the loaded raw frame is unchanged (SHA-256 before = after); (3) no NaN/Inf was counted in any float intermediate; (4) the two final 16-bit outputs are bit-identical; (5) the first output was written as DICOM, the module's own validator reported `valid:true`, and reading the file back returned the same pixels and the same body part, kVp and pixel pitch; (6) the evidence file `baseline.json` was written. Timing is measured and logged; the 3000 ms figure of the product requirements is **not** asserted.
>
> **What the DICOM metadata carries.** Body part, kVp and pixel pitch from the settings. mAs, source-to-image distance and acquisition time are not known to the app and are written as 0 (unknown), never as invented values.
>
> **Evidence.** `evidence/<RunId>/baseline-<n>/` holds exactly two files: `baseline.dcm` and `baseline.json` (a run that is not a pass leaves no `baseline.dcm`; the file is written under a `.partial` name and renamed only after every check, including `baseline.json`, succeeded). `baseline.json` records: `status`, `failureReason`, `startedAt`, `width`, `height`, `runsExecuted`, `inputPreserved`, `inputSha256Before/After`, `bitIdentical`, `difference` (first index, count, maximum), `nanInfCount`, `nonFiniteByStageRun1`, `outputSha256`, `stageHashesRun1/2` (SHA-256 of each stage's output), `stageTimes`, `runTotalsMs`, `totalMs`, `budgetMs` (3000, "measured against, not asserted"), `exposureIndex`, and `dicom` (`path`, `finalFileWritten`, `passed`, `valid`, `report`, `pixelsIdentical`, `metadataAgrees`, `summary`, `cleanupProblem`).
>
> **Uncalibrated EI.** The command logs and records `uncalibrated EI (보정 안 된 EI) = <EI>, DI = <DI>`: the Exposure Index and Deviation Index the enhance_basic module computes (`EI = EIT × mean / S0`, `DI = 10·log10(EI/EIT)`) on the corrected float image before it is scaled to 16 bits. It is a **measurement, not a pass criterion**: the module's S0 reference (1000) has not been checked against this app's gain scale. On the synthetic 1024×1024 fixture it read EI 3098.64, DI 10.93.
>
> **Does not touch.** Settings, the image on screen, the chain status and the stage timing the last render left. It takes a backend lifetime ticket but no Apply request number, so an Apply in flight is neither superseded by it nor supersedes it.
>
> **Known limits.** The DICOM path is passed to the module as ANSI: a folder name with characters outside the system code page makes the write fail (`XPE_ERR_IO_FAILED`; measured with Thai text on a Korean Windows, code page 949; tracked as #239). The two runs share one process, so state that survives inside a process is not caught. The float intermediates of the display stage are not scanned for NaN/Inf. Time on a 3072×3072 image has not been measured.

근거: 위 문장은 모두 `GUI-C-196` 설계 메모 D1~D8 과 M2~M6 보고서, `BaselineParameters.cs`·`BaselineExecution.cs`·`BaselineDicomExport.cs` 의 실제 값에서 옮겼다. 측정값(3098.64 / 10.93, 210~290 ms)은 로컬 대역 DLL 세트에서의 한 번씩의 관찰이다.

### 2.4 자동화 보고서 필드 — `gui/ImageProcTest/README.md` 의 "The emitted report includes:" 목록(현재 9개 항목, `greps.txt` §6)에 덧붙임

```
- `BaselineMenuEnabled`, `BaselineRan`, `BaselineStatus`
- `BaselineStatusText`
- `BaselineBitIdentical`, `BaselineFirstDifference`, `BaselineOutputSha256`
- `BaselineStageTimes`, `BaselineTotalMs`
- `BaselineDicomValid`, `BaselineDicomRoundTripIdentical`
- `BaselineExposureIndex`, `BaselineEvidenceFolder`
```

그리고 같은 절에 판정 규칙 문단(영문):

> **`BaselineStatus` and `Passed`.** `Pass` — the command ran and passed. `Fail` — it ran and failed, **or it was attempted and produced no result** (it threw, its result was dropped, or it did not finish in 60 s). `NotRun` — it was not attempted: the menu item was disabled (Mock), or preprocessing did not run in this automation run, so no calibration set was given. A report with `Fail` has `Passed=false` (the process exits 1); `NotRun` leaves `Passed` unchanged. `NotRun` is never a pass and never a fail: nothing was compared. In the Native CI job an unexplained skip of the baseline tests fails the job separately.

근거: `BaselineAutomationRule.cs`, `MainWindow.xaml.cs` 의 `report.Passed` 식, 시험 A18~A21·`BaselineAutomationRuleTests`, 종료 코드는 `App.xaml.cs:14-17`(`AutomationFailedExitCode = 1`).

### 2.5 그 밖에 손볼 후보 (제안만)

- `XPE-GUI-MENU-001` §4.4 목록과 §191 의 "Phase 1b … deterministic baseline workflow" 는 이름만 쓰고 있어 수정 불필요.
- `XPE-GUI-MENU-001` §8 의 `Run Full Pipeline | all modules | 1b` 행은 코드에서 `Run AI Bone Suppression`(GUI-C-184) 으로 바뀐 항목과 어긋나 보인다. 이 카드 범위 밖이라 **확인하지 않았고** 알리기만 한다.
- RTM-GUI-001 에 이 명령의 추적 행을 둘지(요구 ID 가 없다)는 리더 판단.

## 검증·한계

- 확인한 것: 위 현재 문장 전부가 `4e7902b2` 에서 grep 으로 읽힌다(`greps.txt`). 스위치 4가지·`XPE0001`·종료 코드 2·잡 이름은 코드와 스크립트에서 읽었다. 보고서 필드 13개 이름은 `GuiAutomationReport.cs` 와 대조했다.
- **확인하지 않은 것**: 초안을 문서에 적용해 렌더링해 보지 않았다. 리더의 미커밋 작업 트리의 문서 상태. 새 `docs/` 문서가 있을 수 있는 다른 레인의 문장. `gui-shipped-build` 잡이 최근 main 에서 실제로 초록인지(워크플로 정의만 읽었다). `Run Full Pipeline` 행(§2.5)의 실상.
- 코드 변경 없음, 문서 변경 없음. 린트 대상은 이 폴더의 `.md`·`.txt` 뿐이다.
