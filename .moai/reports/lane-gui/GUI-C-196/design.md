# GUI-C-196 — Run Deterministic Baseline: 설계 메모 (코드 없음)

리더 결정(정의): **pipeline-spec 1b 체인 + 평가 프로토콜의 판정.** 체인은 EI-0 → log → noise → contrast → edge → 표시 LUT → DICOM 쓰기, 판정은 같은 입력으로 두 번 돌려 출력이 비트 동일(`DeterminismRMSE = 0`).

이 메모는 코드를 읽고 쓴 것이고 실행한 것은 없다(Native 는 이 기계에서 못 돌린다, #98). 코드 인용은 지금 트리(`484aa05a`) 기준 파일:줄이다.

## 0. 먼저 결정이 필요한 것 (D1~D6)

설계가 이 여섯에 걸려 있다. 각각 추천을 적었고 근거가 약한 곳은 그렇게 적었다.

| # | 결정 | 추천 | 근거 / 약한 곳 |
|---|---|---|---|
| **D1** | **EI-0 의 입력.** EI 는 "detector 영역 float 영상"에 정의된다(`pipeline-spec` EI-0, `enhance_basic_api.h` `xpe_calc_exposure_index`). 그런데 지금 체인은 단계 사이를 `ushort[]` 로 주고받고, 전처리 단계가 float 를 uint16 으로 내릴 때 **프레임의 최대값으로 정규화하고 잘라 낸다**(`GuiPreprocessRunner.cs:201-220`: `scale = 65535 / max`, `(ushort)value`). 그 uint16 에서 EI 를 계산하면 값이 프레임마다 정규화에 좌우돼 뜻이 없다 | **전처리 단계 안에서, 내리기 직전의 float 로 EI-0 를 계산**해 단계 요약(그리고 결과 값)에 싣는다. 체인 계약(uint16 입출력)은 바꾸지 않는다. 대안(체인에 float 옆 통로를 추가)은 `StageExecution`·`ChainResult`·Mock·시험 전부를 건드린다 | 모듈의 `S0_REFERENCE = 1000`(`exposure_index.cpp:15`)이 "gui 전처리의 gain 보정 float 척도"와 같은 척도인지는 **확인하지 못했다.** EI 는 **측정값으로 보고하고 합격 기준에는 넣지 않는다**(값이 엉뚱해도 결정성 판정은 그대로) |
| **D2** | **단계 입도.** 다섯 함수를 단계 다섯으로 하면 단계마다 uint16 으로 내려가 로그 영역(정밀도가 중요)에서 양자화가 쌓인다 | **합성 단계 하나** `enhance_basic`: 안에서 float 로 log → noise → contrast → edge 를 이어서 하고 **끝에서 한 번만** uint16 으로 내린다. 하위 단계별 결과·시간은 단계 요약 문자열에 싣는다. 하나라도 거부하면 **전체가 RequestedNotApplied**(부분 결과 없음) | 스펙은 각 단계를 "mandatory"로 적고 개별 라벨을 둔다. 합성은 그 라벨을 "하위 단계"로 내리는 해석이다 |
| **D3** | **보통 Apply 에 넣을지.** `PreprocessInChain`·`AiBoneSuppressionInChain` 처럼 설정 스위치를 추가하면 저장 설정·메뉴·"설정이 처리에 닿는가" 조사(`SettingsProcessingConnectionTests`)가 따라온다 | **넣지 않는다.** `ProcessingChainPlan` 에 **고정 기준 단계 목록**(`BuildBaselineStages()`: `preprocess`, `enhance_basic`)만 추가. 일반 Apply 는 한 줄도 안 바뀐다 | 리더 정의가 "명령"이다. 사용자가 조절하는 enhance 는 별개 기능 |
| **D4** | **log 정규화 계수**(`normFactor`). 어느 문서도 값을 정하지 않는다(`docs`·`modules/enhance_basic` 검색: 값 없음. `docs/enhance-basic/README.md:122` 의 예시는 두 번째 인자를 `epsilon` 으로 적어 **API 와 어긋난 문서 결함**) | `normFactor = 65535 / log10(65536)` ≈ 13 606 (M1 정정: 설계 메모의 13 652 는 내 계산 오류, 시험이 `65535 → 65535` 성질과 13 606 을 단언). uint16 만점이 로그 영역에서도 만점이 되어 기존 VOI 기본값이 같은 척도에서 의미를 유지 | **스펙 근거 없는 가정**이다. 리더가 다른 값을 정하면 그 값이 상수 한 곳(`BaselineParameters`)에서 바뀐다 |
| **D5** | **실패 정책.** 일반 Apply 는 거부된 단계를 건너뛰고 입력 그대로 이어 간다(fail-safe pass-through) | 기준 명령은 필수 단계가 하나라도 `RequestedNotApplied` 면 **명령 실패**로 끝낸다(영상·DICOM 쓰기 없음). 통과 영상을 "기준"이라 부르면 안 되기 때문 | 스펙 §5.2 "mandatory"와 `pipeline-spec` 5.3("기준 영상을 조용히 덮지 않는다")의 연장 |
| **D6** | **"같다"의 대상.** DICOM 파일은 SOP Instance UID·시각 때문에 두 번 쓰면 바이트가 다르다 | **표시 LUT 뒤의 최종 uint16 픽셀 배열**을 비트 비교한다(그것이 DICOM 으로 가는 값). 보조로 단계별 SHA-256, 입력 보존(`sha256(raw)` 전후), `NaNInfCount`(프로토콜 §5.7)를 함께 낸다. DICOM 파일은 비교하지 않고 **쓴 뒤 다시 읽어 픽셀이 쓴 것과 같은지**만 본다 | 프로토콜은 "SIMD 반올림이 다르면 경계를 명시한 허용"을 허용하지만 리더 결정이 정확 일치이므로 정확 일치로 만든다. 다를 때는 **어디서 처음 달라지는지(단계, 첫 인덱스, 최대 차이)를 진단으로 낸다**(판정을 완화하는 것이 아니라 원인을 찾기 위해) |

## 1. enhance_basic 을 체인에 넣는 방식

**지금 구조**(읽은 것): `ProcessingChainPlan.BuildStages`(`ProcessingChainPlan.cs:12-24`)가 `[preprocess, gsvg, ai_bone_suppress]` 를 만들고, `ProcessingChainRunner.Run`(`ProcessingChainRunner.cs`)이 순서·복사·실패 시 건너뜀·길이 검사를 한곳에서 강제한다. 백엔드는 `StageExecutor` 하나를 준다: Real `RunChain`(`RealXpeBackend.cs:284-305`)은 `StageIds` 로 분기하고, Mock(`MockXpeBackend.cs:321-337`)은 전부 "네이티브 필요"로 거부한다. 요청 번호·표(`BackendLifecycle`)는 **Apply 명령**의 규칙이고 체인 자체에는 없다.

**끼우는 곳**: `StageIds.EnhanceBasic = "enhance_basic"` 추가, 기준 목록은 `preprocess → enhance_basic`. 보통 체인 순서가 쓰이게 되면(D3 에서 안 하지만) `preprocess → enhance_basic → gsvg → ai` 가 스펙 순서(5~8 뒤에 9, 13)와 같다. 기준 명령은 `gsvg`·AI 를 **넣지 않는다**(스펙 §5.3: 보조 출력이 기준 영상을 덮으면 안 됨 — 사용자 토글과 무관하게).

**`enhance_basic` 단계 내용**(Real 백엔드 `RunEnhanceBasicStage`, 새 `Native/GuiEnhanceRunner.cs`, `RunPreprocessStage` 모양):
1. 입력 `ushort[]` → float32 버퍼(값 그대로, 척도 가정 없음 — 입력이 이미 0..65535).
2. `xpe_log_transform(img, normFactor)` → `xpe_noise_reduce(img, &bilateral)` → `xpe_contrast_enhance(img, &clahe)` → `xpe_edge_enhance(img, &usm)`. 모두 제자리 float32 이고 서로의 출력을 입력으로 받는다(모듈 헤더 `enhance_basic_api.h`).
3. 끝에서 한 번 uint16 으로: `clamp(0, 65535)` 후 **짝수 반올림**(결정적). (전처리 단계는 자르기(truncate)를 쓴다 — 다른 규칙이지만 이번 카드에서 건드리지 않는다. 불일치는 §8 에 적음.)
4. 하위 단계마다 반환 코드·경과 시간·`NaN/Inf` 수를 요약에 담고, 코드가 0 이 아니면 즉시 중단 → `StageExecution(false, null, "enhance_basic: <단계> returned <코드>: <사유>")`.

**매개변수 출처 = 고정 기본값**(D2·D3). 설정이 아니라 상수 한 곳 `BaselineParameters`에 모듈 헤더의 기본값을 적고 줄을 인용한다: noise 는 bilateral `σ_space 3.0`, `σ_range 50.0`(헤더 주석 "default"); CLAHE `clip 3.0`, `tile 8×8`; USM `amount 0.5`, `radius 2.0`, `threshold 10.0`; log 는 D4. 같은 값은 반드시 같은 결과를 내야 하므로(비트 동일 판정) 사용자 설정과 섞지 않는다.
- **약한 곳**: 헤더의 기본값이 "uint16 만점 척도 영상"에서 의미 있는 값인지(예: `σ_range 50` 이 로그 영역 0..65535 에서 적당한지)는 **확인하지 못했다.** 결정성 판정에는 영향이 없지만 영상 품질에는 영향이 있다. 품질은 이 카드의 판정 대상이 아니라고 적어 둔다.

**거부(181f 의 비유한 거부 등)를 받으면**: 입력이 유한한 uint16 이므로 비유한은 모듈 결함일 때만 나온다. 그때 `enhance_basic` 은 `RequestedNotApplied` + 사유(함수 이름과 코드), 모듈이 올린 경보는 이미 있는 `InvokeNative` 경로가 알림 목록으로 나른다(전처리·AI 단계와 같은 방식). 기준 명령은 D5 에 따라 **"Deterministic Baseline FAILED at enhance_basic: …"** 으로 끝나고, 상태 줄·로그·경보(ERROR)에 같은 문장을 낸다.

## 2. 표시 LUT · DICOM 쓰기

**표시 LUT: 이미 있다.** `RealXpeBackend.ApplyDisplayPipelineCore`(`:98`)가 modality → VOI → presentation(GSDF 선택) 을 부른다. 입력은 `ushort[]` 를 float 로 **그대로** 옮긴다(`:132`) — 그러므로 enhance 출력이 uint16 만점 척도여야 기존 VOI 기본값이 맞다(D4 의 이유). 표시 설정은 지금 사용자 설정(`VoiLutMode` 등)을 읽는다: 기준 명령은 설정 스냅샷을 쓰되 **GSDF 는 끈다**(손 입력 두 점 가정이 "기준"에 들어가면 안 된다. 195 §4). 이 선택은 D7(아래)에 올린다.
- **빠진 것**: 이 함수는 결과를 **미리보기 비트맵으로만** 돌려준다(`LoadedImageFrame` 에 `ProcessedPreview`, 최종 uint16 은 `CopyNativeUInt16Pixels` 로 만든 뒤 비트맵으로 버려진다). 비트 비교와 DICOM 쓰기에는 최종 `ushort[]` 가 필요하다.

**DICOM 쓰기 = 운영 앱에서 처음 부르는 경로.** gui 의 csproj 는 DICOM 파일을 연결하지 않는다(195 §3). 옛 클라이언트의 `XpeDicomWrapper`·`NativePresentationExportService` 가 쓰기+검증을 하는 선례다(같은 `xpe_dicom_write` → `xpe_dicom_validate`, JSON 보고서의 `valid`).
- **새 상호운용**: `Native/XpeDicomInterop.cs`(`xpe_dicom_write`, `xpe_dicom_validate`, `xpe_dicom_open`, `xpe_dicom_read_image`, `xpe_dicom_get_metadata`, `xpe_dicom_close`; 다른 Interop 처럼 `GuiNativeLibraryResolver` 로 DLL 해결).
- **출력 위치**: 앱이 이미 쓰는 증거 폴더 규칙 `AppContext.BaseDirectory/evidence/<RunSet.RunId>/`(`MainWindowViewModel.cs:2128`, `:3001`) 아래 `baseline-<n>/baseline.dcm`, 같은 폴더에 `baseline.json`(결과). 운영 앱이 증거 폴더에 쓰는 일은 이미 있다(Export Evidence Snapshot).
- **메타데이터 채우기**(`XpeImageMetadata`: `bodyPart[64]`, `kVp`, `mAs`, `SID_mm`, `pixelPitch_mm`, `acquisitionTime`, `flags`): 앱이 아는 것은 `SelectedBodyPart`, `ExposureKvp`, `PixelPitchMm` 셋뿐이다(`AppSettings.cs:279·549·562`). **`mAs`·`SID_mm` 는 설정이 없다 → 0("모른다")**, `acquisitionTime` 은 0(모른다). 지어낸 값을 넣지 않는다. 쓰기 뒤 `xpe_dicom_validate` 로 DX 적합성을 확인하고 읽기 경로로 되읽어 **쓴 픽셀과 같은지**(D6)를 본다.
- **약한 곳**: 환자 식별 등 필수 태그를 쓰기가 어떻게 채우는지, 모든 필수 태그 검증을 `valid:true` 로 통과하는지는 **이 기계에서 못 돌려 확인하지 못했다.** 첫 Native CI 실행이 그 답을 준다(M3 의 목적). 실패하면 그 사유가 그대로 결과에 실린다.
- 읽기 쪽 MONOCHROME1 반전(QA-B-185)은 이 경로에 영향이 없다: 쓰기는 MONOCHROME2 이고 되읽기도 같은 값을 돌려받는다.

## 3. Deterministic Baseline 명령

**배선**(`RunPreprocessing`·`RunAiBoneSuppression` 의 모양을 따르되 **설정을 바꾸지 않는다** — 그 둘은 `Settings.*InChain = true` 로 저장 설정을 바꾸고 보통 Apply 를 부른다, `MainWindowViewModel.cs:2516-2556`; 기준 명령은 사용자의 토글을 건드리면 안 된다):
- `RunDeterministicBaselineCommand`, `CanRunDeterministicBaseline => _backend.SupportsPreprocessing`(Native 일 때만; `:739` 와 같은 조건 + 기준 지원 확인). 메뉴는 `Command` 와 `IsEnabled` 바인딩으로 바뀐다. Mock 에서는 비활성이고 툴팁이 이유를 말한다(MENU-001 §4.4 "비활성이면 이유를 보인다").
- **입력**: 지금 열려 있는 영상(`ActiveImageFrame.RawPixels`). 새 파일 선택 UI 는 만들지 않는다(Open Raw 로 고른 영상). 영상이 없으면 `DISPLAY_NO_IMAGE` 와 같은 경고.
- **두 번 실행**: 같은 `sourceFrame` 과 같은 설정 스냅샷으로 `[체인 → 표시]` 를 **연달아 두 번**. 한 번째의 출력을 둘째 입력으로 쓰지 않는다. 입력 보존은 실행 전후 `sha256(RawPixels)`.
- **수명 규칙**: `TakeRequestTicket`, `RefusedWhileTransitioning`, 결과 도착 시 `IsCurrent(ticket)` 확인(옛 백엔드 결과는 버림, `ApplyDisplayPipelineAsync` 와 같음). 두 번의 실행은 백그라운드 작업 하나 안에서 이어서 한다.
- **Lane B 는 돌리지 않는다**(Reference 한 줄만). 보통 Apply 는 두 레인으로 같은 체인을 두 번 부르는데(`RenderLane` `:3456`), 기준 명령이 거기까지 겹치면 시간이 불필요하게 늘어 3000 ms 판정(`product.md:183`)과 섞인다.
- **비교**: 새 `Services/BaselineDeterminism.cs`(WPF 없음, 시험이 연결): 두 `ushort[]` 의 완전 일치, 다르면 첫 다른 인덱스·다른 개수·최대 차이, 단계별 SHA-256 목록.

**결과 표시**: ① 상태 줄 한 줄 — 통과 `Deterministic Baseline PASS: two runs bit-identical (<w>×<h>, <ms> ms; DICOM valid)`, 실패 `Deterministic Baseline FAIL: <사유>`; ② 로그에 단계별 시간·해시·EI/DI; ③ 경보 하나(통과 INFO / 실패 ERROR); ④ 증거 폴더의 `baseline.json`. 새 창은 만들지 않는다(기존 알림·상태 줄 패턴).

**자동화 보고서에 실을 값**(`GuiAutomationReport` 에 추가): `BaselineRan`(bool), `BaselineStatus`(`Pass`/`Fail`/`NotRun`), `BaselineBitIdentical`, `BaselineFirstDifference`(없으면 빈 값), `BaselineOutputSha256`, `BaselineStageTimesMs`, `BaselineTotalMs`, `BaselineDicomValid`, `BaselineDicomRoundTripEqual`, `BaselineInputPreserved`, `BaselineNaNInfCount`, `BaselineEi`/`BaselineDi`(D1: 측정값). 자동화 실행 모드가 이 명령을 부르게 할지는 새 `--automation-*` 스위치를 늘리는 일이라(스위치마다 소비 시험 필요, `AutomationRunSelectionConsumptionTests`) **이번에는 하지 않는다**: E2E 가 메뉴를 UIA 로 호출하고 상태·보고서를 읽는다.

## 4. 활성 메뉴·기능과 겹침

| 기존 | 겹침 | 판단 |
|---|---|---|
| Run Preprocessing | 전처리만. 설정을 켜고 Apply | 기준 명령의 첫 단계와 같은 `RunPreprocessStage` 를 재사용(복제 아님). 설정은 안 건드림 |
| Apply Display Pipeline | 체인+표시, 두 레인 | 표시 함수 재사용. 레인·설정 토글은 안 따름 |
| Run AI Bone Suppression(`RunFullPipelineMenuItem`) | 보조 단계 | 기준에서 제외(스펙 §5.3) |
| Stage Timing | 마지막 렌더의 `ChainStatus`·`PipelineTimings` 를 보여 줌(`MainWindowViewModel.cs:1570`) | 기준 명령은 이 두 값을 갱신하지 않는다(화면의 영상과 안 맞는다) → 시간은 `baseline.json`·로그에만 |
| Benchmark Runner | CI 의 ctest 동결 시험 실행 | 목적이 다르다(성능 동결 대 운영 앱 한 영상의 결정성) |
| Export Evidence Snapshot | 자동화 보고서 한 건 | 같은 증거 폴더를 쓴다. 번들에는 `baseline-*/` 가 함께 담긴다(확인 필요: 번들이 폴더 전체를 담는지, §8) |

## 5. 시험 계획

| 층 | 시험 | 환경 |
|---|---|---|
| 순수 | `BaselineDeterminism`: 같음, 한 화소 차이(첫 인덱스·개수·최대 차이), 길이 다름, 빈 입력, 단계별 해시 | 어디서나(`clients/…IntegrationTests` Functional) |
| 순수 | 기준 단계 목록 = `[preprocess, enhance_basic]` 고정, 사용자 토글과 무관, `gsvg`·AI 없음 | 동일 |
| 순수 | 결과 분류 규칙(D5): 필수 단계 하나라도 `RequestedNotApplied` → 명령 실패, 영상·DICOM 쓰기 호출 없음. 가짜 `StageExecutor` 로 `ProcessingChainRunner` 를 두 번 돌려 결정성 판정 전체를 시험 | 동일 |
| 시뮬 | **결정성 단언이 거짓 통과하지 않는다**: 두 번째 실행에서 한 화소를 바꾸는 가짜 실행기 → 판정 FAIL, 첫 다른 인덱스가 맞음 | 동일 |
| Mock | 메뉴 비활성+툴팁 이유, 명령 호출 시 "native 필요" 안내(실행 안 됨), 자동화 보고서 개수 일치(활성 목록에서 빠짐: 직접 비교하는 두 계산이라 수가 바뀌어도 일치해야 함) | Mock E2E(여기서 돈다) |
| Native | 모듈 호출 단위: `GuiEnhanceRunner` 가 알려진 입력(균일·계단)에서 모듈 반환 0 과 유한 출력, 같은 입력 두 번 → 같은 출력(모듈 문서가 말하는 "스레드 수와 무관한 동일 출력"의 확인). DICOM: 쓰기 → 검증 `valid:true` → 되읽기 픽셀 일치(`GsdfCalibrationInputTests` 가 DLL 을 직접 부르는 선례) | `gui-e2e-native` CI(여기서 못 돈다) |
| Native E2E | 합성 1024² 로 기준 명령: PASS, 두 번 비트 동일, DICOM 유효·되읽기 일치, 단계별 시간과 합계를 **로그에 숫자로 찍는다**(192d 의 C-09 방식). 3000 ms 는 처음에는 **단언하지 않고** 측정한다(게이트를 잘못된 기계에서 정하지 않는다) | `gui-e2e-native` |

**반증**(규칙마다 지우고 해당 시험이 빨개지는지): (1) 비교가 한 화소를 못 보게 하기(마지막 인덱스 제외) → 시뮬 시험 빨강; (2) 둘째 실행을 건너뛰고 첫 결과를 두 번 비교 → "두 번 돌렸는가" 단언(실행 횟수 카운터) 빨강; (3) 필수 단계 실패에서 그냥 이어 가기 → 분류 시험 빨강; (4) 입력 보존 해시를 실행 전후 같은 값으로 고정 → 입력을 바꾸는 가짜 실행기에서 빨강; (5) 기준 목록에 `gsvg` 끼우기 → 목록 시험 빨강.

## 6. 규모와 마일스톤 (시간이 아니라 파일·단계 수)

**제품 파일**: 새 7개 — `Services/BaselineDeterminism.cs`, `Services/BaselineParameters.cs`(고정 상수·인용), `Services/Native/XpeEnhanceBasicInterop.cs`, `Services/Native/GuiEnhanceRunner.cs`, `Services/Native/XpeDicomInterop.cs`, `Services/Native/GuiBaselineExporter.cs`(쓰기·검증·되읽기), `Models/BaselineRunResult.cs`. 고침 약 9개 — `ProcessingChain.cs`(`StageIds`), `ProcessingChainPlan.cs`, `IXpeBackend.cs`(기본 구현을 가진 한 멤버로 SelfCheck 의 `ScenarioBackend`·`FaultInjectingBackend` 파급 최소화), `RealXpeBackend.cs`, `MockXpeBackend.cs`, `GuiPreprocessRunner.cs`(D1: EI-0), `MainWindowViewModel.cs`, `MainWindow.xaml`(한 항목), `MainWindow.xaml.cs`(활성 목록에서 제거), `GuiAutomationReport.cs`. 아울러 `ImageProcTest.csproj`(연결 파일). **시험 파일**: 새 4~5개(결정성·계획·분류·Native 호출·E2E 2) + 기존 몇 곳의 기대치.

**마일스톤**(앞 것이 끝나야 뒤를 시작, 각 끝에서 리더 확인):
1. **M1 순수 핵심** — `BaselineDeterminism`, 기준 단계 목록, 결과 분류, 가짜 실행기 시험과 반증. 네이티브 없이 여기서 끝까지 검증된다.
2. **M2 enhance 단계** — `StageIds.EnhanceBasic`, Interop+`GuiEnhanceRunner`(D1~D4 반영), Real/Mock 연결, `GuiPreprocessRunner` 의 EI-0. Native 호출 시험(CI).
3. **M3 DICOM 쓰기·검증·되읽기** — Interop+`GuiBaselineExporter`, 메타데이터 규칙, Native 시험(CI). **이 단계의 첫 CI 실행이 "쓰기가 `valid:true` 를 내는가"의 답이다.**
4. **M4 명령** — 백엔드 멤버, VM 명령(티켓·수명 규칙), XAML 활성화, 자동화 보고서 필드, 상태·경보·증거 파일.
5. **M5 E2E·문서** — Mock E2E(여기서 돌림), Native E2E(측정 로그), 리더가 고치는 MENU-001·툴팁 반영 확인.

## 7. 추가 결정 (D7~D8)

- **D7 표시 설정**: 기준 명령이 사용자의 VOI·GSDF 설정을 따를지, **고정**(선형 modality, 기본 VOI, GSDF 끔)할지. 추천: **고정**. 결정성 판정은 같은 설정이면 둘 다 되지만, "기준"이 사용자 슬라이더에 따라 바뀌면 이름이 거짓말을 한다.
- **D8 두 번째 실행의 의미**: 같은 프로세스 안에서 두 번이다. 프로세스를 새로 띄워 두 번 돌리는 판정(전역 상태·캐시 의존까지 잡는 더 강한 판정)은 하지 않는다. 필요하면 후속.

## 8. 읽은 것의 한계와 미확인

- Native 를 못 돌렸다. 모듈 반환·시간·DICOM 유효성은 전부 **문서와 헤더** 위의 추정이다.
- D1 의 EI 척도, D4 의 `normFactor`, D2 매개변수 기본값이 영상에서 적당한지는 **확인하지 못했다.**
- 전처리 단계는 float→uint16 에서 **자르기**를 쓰고(`GuiPreprocessRunner.cs:216-219`) 이번 enhance 단계는 **짝수 반올림**을 제안한다. 규칙이 갈린다(결정성에는 무관). 통일할지는 리더 판단(통일하면 보통 Apply 의 출력이 바뀐다).
- Export Evidence Snapshot 의 번들이 `evidence/<RunId>/` 전체를 담는지 확인하지 않았다(§4 마지막 행).
- `xpe_dicom_write` 가 환자 식별 필수 태그를 어떻게 채우는지는 읽지 않았다.
- 3000 ms 가 어떤 영상 크기·기계 기준인지 확인하지 않았다(`product.md:183`).

## 9. 리더가 고칠 문구 초안 (MENU-001·툴팁)

**툴팁**(`MainWindow.xaml:363`, 영어 한 문장; 이 줄은 구현 때 내가 바꿔도 되지만 리더가 맞춘다고 하셨으므로 초안만):
`Runs the Phase 1b deterministic baseline (preprocess, enhance_basic, display LUTs, DICOM write) twice on the loaded image and reports whether the two outputs are bit-identical. Needs the native backend and a loaded image.`

**`XPE-GUI-MENU-001` §8 표(231행)** 한 줄 교체:
`| Run Deterministic Baseline | xpe_preprocess.dll, xpe_enhance_basic.dll, xpe_display.dll, xpe_dicom.dll | 1b | Native backend + image loaded + calibration paths valid |`
그리고 같은 문서에서 "deterministic test mode active" 문구 삭제. §4.4 의 항목 이름은 그대로.

**`docs/enhance-basic/README.md:122`**: `xpe_log_transform(image, epsilon);` 는 API 와 어긋난다 → `xpe_log_transform(image, normFactor);`(정규화 계수, 양수)로. 이는 post 레인 문서일 수 있어 알린다.

## 10. 요청

`설계 검토 요청`: D1~D8 에 대한 결정(특히 D1 EI-0 의 위치, D4 `normFactor`, D7 표시 설정 고정)과 마일스톤 분할(M1 부터)에 대한 확인.
