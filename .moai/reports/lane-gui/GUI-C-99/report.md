# GUI-C-99 보고 — 픽셀 연쇄 1단계: RunChain 계약과 전처리 연결 (#180, #173)

- 커밋: `dev/gui` `b8b4dfb` (미푸시, `bf66a97` 위)
- 증거 디렉터리: `build/e2e-c99/`

## 1. 주장

1. **계약 B 를 도입했다.** `IXpeBackend.RunChain(rawFrame, 단계 목록, settings) → ChainResult`.
   - `RunPreprocessing` 은 인터페이스에서 빠지고 연쇄의 `preprocess` 단계가 됐다.
   - 모델은 `Models/ProcessingChain.cs` 다: `StageStatus` 4값, `StageRequest`, `StageOutcome`, `ChainResult`.
   - `ChainResult.DisplayInput` 은 마지막으로 픽셀을 만든 단계의 결과이고, 그런 단계가 없으면 raw 다.
2. **순서·복사·폴백 규칙을 한 곳에 모았다.** `Services/ProcessingChainRunner.cs` 는 WPF 와 무관하고, Real 과 Mock 이 이것을 함께 쓴다.
   - 단계는 목록 순서대로 실행되고, 각 단계는 앞 결과(없으면 raw)의 **복사본**을 받는다. raw 배열은 누구에게도 쓰기용으로 넘겨지지 않는다.
   - 거부·예외·길이 불일치는 `RequestedNotApplied` 로 기록하고 이유를 남긴다. 다음 단계는 마지막 정상 픽셀로 이어 간다.
   - 입력과 같은 출력은 `AppliedNoChange` 다.
   - 단계가 돌려준 배열은 복사해서 보관한다.
3. **디스플레이는 연쇄 결과에서 시작한다.**
   - `ApplyDisplayPipeline(rawFrame, displayInput, settings)` — Real 과 Mock 모두 `rawFrame.RawPixels` 대신 `displayInput` 을 읽는다.
   - 뷰모델은 **같은 스냅샷**으로 연쇄와 디스플레이를 차례로 돌린다(`MainWindowViewModel.cs` `ApplyDisplayPipelineAsync`).
4. **전처리를 연쇄 단계로 바꿨다.**
   - `AppSettings.PreprocessInChain` 를 추가했다(기본 꺼짐 — 끈 상태의 화면은 이전과 같다).
   - 단계 목록은 `ProcessingChainPlan.BuildStages` 가 만든다.
   - Pipeline → Run Preprocessing 메뉴는 단계를 켜고 다시 렌더한다.
   - Mock 에서는 `RequestedNotApplied` + 경보 `PREPROCESS_NOT_RUN` 이 나오고, 디스플레이는 raw 에서 시작한다.
5. **kVp 를 사용자 설정 하나로 합쳤다.**
   - `AppSettings.ExposureKvp` 를 추가했다(기본 70, 0 이하·NaN 은 70).
   - `GuiPreprocessRunner` 의 하드코딩 70 을 제거하고 인자로 받는다.
   - 분석 패널 Parameters 탭에 입력칸(`ExposureKvpInput`)이 있다.
6. **상태 표시를 붙였다.**

| 표시 | 내용 |
|---|---|
| 상태줄 `ChainStatusText` | `chain: preprocess=<상태>; display input=raw\|chain` |
| 분석 패널 `ChainStatusPanelText` | 같은 문구 |
| 상태 문구 | `<연쇄 요약> \| <디스플레이 요약>` |
| 단계 로그 | 단계마다 한 줄 |
| 경보 | 요청했지만 적용 안 된 단계마다 WARN (`PREPROCESS_NOT_RUN` / `CHAIN_STAGE_NOT_APPLIED`) |
| #171 렌더 입력 스냅샷 | 연쇄도 같은 `Settings.Snapshot()` 을 쓴다 |
| 오래됨 배너 비교 항목 | `ChainInputsDiffer` 추가 — 단계 스위치, kVp, 교정 경로 3개, 부위 |
| 보고서 3종 | 자동화 보고서(`GuiAutomationReport.ChainStatus`, `ChainStages`), 내보내기 JSON(`processingChain`), 증거 `backend.json`(`processingChain`) |

7. **그린 픽셀을 읽을 수 있게 했다.**
   - 뷰포트 HelpText 끝에 `processed=<FNV-1a 16진>` 을 붙였다. 뷰포트가 가진 처리 레이어(`ProcessedImage ?? SourceImage`)의 픽셀 해시다.
   - 해시는 영상 객체당 한 번만 계산한다.

### 기존 시험의 기대값이 바뀐 것

| 시험 | 이전 기대 | 이후 기대 | 이유 |
|---|---|---|---|
| `WorkflowScenarios.W02` (Native) | 상태줄에 `offset -> gain -> defect` | `ChainStatusText` 에 `preprocess=Applied` 와 `display input=chain`, 상태줄에 `skipped` 없음 | 상태줄 문구가 `<연쇄 요약> \| <디스플레이 요약>` 으로 바뀌었다. 단계 문구는 로그로 간다 |
| `SettingsProcessingConnectionTests` 진입점 | `LoadRawImage`, `ApplyDisplayPipeline`, `RunPreprocessing` | `LoadRawImage`, `ApplyDisplayPipeline`, `RunChain`, `BuildStages` (+ 소스 `ProcessingChainPlan.cs`) | 전처리가 연쇄 단계가 되었고, 무엇을 실행할지는 계획이 정한다 |
| 같은 시험의 바인딩 수 | 24 | 26 | `PreprocessInChain`, `ExposureKvp` 추가 |
| 같은 시험의 수집 | 인자 가드도 "따라갈 수 없는 전달"로 빨강 | `ThrowIfNull` 은 읽지 않는 호출로 허용(`NonReadingCallees`) | `BuildStages` 의 null 가드 |
| `ViewStateRenderScenarios` 판독 정규식 | `opacity=` 가 줄 끝 | 뒤에 `; processed=…` 가 붙을 수 있음 | 해시 추가 |
| Native 전처리 화면 (시험 없음) | 전처리 결과를 최소~최대로 늘인 미리보기로 교체, VOI 없음 | 전처리 픽셀에 디스플레이 파이프라인(VOI) 적용 | 연쇄 설계. 이 동작을 단언하던 시험은 없었다 |
| `gui/ImageProcTest.E2E/Program.cs` (옛 하네스) | 전처리 메뉴 비활성 단언 | 변경 없음 | 시험 모음에 포함되지 않는 프로그램이다. 이번에 돌리지 않았다 |

### 화소 간격 — 출처 조사 (코드는 바꾸지 않음)

`origin/main` 의 `docs`, `modules`, `.moai/specs`, `.moai/project` 를 검색했다.

| 값 (mm) | 쓰는 곳 |
|---|---|
| 0.14 | `GuiPreprocessRunner.cs` 메타데이터, `XPE-ALG-001:10726` 주석, 품질평가 문서 두 곳(`docs/quality-eval/01…:1939` "예: 140 μm", `03…:3648`) |
| 0.139 | gsvg 헤더 예시(`gsvg_api.h:84`), gsvg 벤치(`test_virtual_grid.cpp:794`), enhance 시험 4곳 |
| 0.143 | `test_xpe_preprocess_correction.cpp:71` |
| 0.148 | `XPE-ALG-001:2038`, `:5677` 기본값, DICOM 수락 기준(`SPEC-XPE-P1B-DICOM/acceptance.md:40`), dicom·enhance_basic 시험 |
| 범위만 | PRD `xray-postprocessing-prd.md:71` "100-200 μm (detector dependent)", 전처리 검증 범위 0.1~0.5(`preprocess.cpp:25`) |

- **검출기 사양서, 교정 파일 형식, SRS 중 어느 것도 특정 값을 정하지 않는다.** SRS 에서는 "pixel pitch" 검색이 0건이었다.
- 따라서 어느 값이 맞는지 이 조사로는 정할 수 없다. 사용 중인 값만 넷이다.
- `.xcal` 교정 파일에 화소 간격이 들어 있는지는 확인하지 않았다.

## 2. 증거

**빌드** (`build-final.txt`)
- `BUILD_EXIT_GUI=0`, `BUILD_EXIT_SC=0`, `BUILD_EXIT_E2E=0`, `BUILD_EXIT_INT=0`, 경고 0

**전체 실행**

| 실행 | 결과 | 파일 |
|---|---|---|
| 통합 | 실패 0 / 통과 234 / 건너뜀 1 (219 + 연쇄 14 + 대조군 1) | `full-int.txt` |
| Mock E2E 전체 | 실패 0 / 통과 103 / 건너뜀 3 (V-06, IB-02, NativeProvenance) | `full-mock.txt`, `full-mock.trx` |
| **Native E2E 전체** (`build/ci-776292b/bin`) | **실패 0 / 통과 105 / 건너뜀 1** (V-06) | `full-native.txt`, `full-native.trx` |

**연쇄 E2E 관측**

| 백엔드 | 전처리 | 연쇄 상태 | 그린 처리 픽셀 해시 | 파일 |
|---|---|---|---|---|
| Mock | 끔 | `chain: preprocess=NotRequested; display input=raw` | `493639b21667e13b` | `chain-mock1.txt` |
| Mock | 켬 | `chain: preprocess=RequestedNotApplied; display input=raw` | `493639b21667e13b` (같음) | 같음 |
| Native | 끔 | `chain: preprocess=NotRequested; display input=raw` | `9e58cd6029598dfa` | `chain-native1.txt` |
| Native | 켬 | `chain: preprocess=Applied; display input=chain` | `9c0c1919e36ff0dc` (다름) | 같음 |
| Native | 다시 끔 | `…NotRequested…` | `9e58cd6029598dfa` (원래대로) | 같음 |

- Native 교정 세트는 픽스처가 `xpe_calib_fixture_gen` 으로 만든 것이다(`ApplicationFixture.cs:142`).
- W-02(Native)는 새 기대값으로 통과했다.

**반증**

| 약화 | 결과 | 파일 |
|---|---|---|
| 뷰모델이 디스플레이 입력으로 `sourceFrame.RawPixels` 를 넘김 (재빌드 `BUILD_EXIT=0`), Native C-01 | **실패**: `The chain reports '…preprocess=Applied; display input=chain', but the drawn processed pixels are unchanged (9e58cd6029598dfa).` | `f1-display-raw.txt` |
| 실행기가 복사하지 않고 입력 배열을 그대로 넘김 | `StageWritingItsInput_DoesNotTouchRaw` 실패 1 | `f-nocopy.txt` |
| 실행기가 실패한 단계 뒤에 입력을 0 배열로 바꿈 | `FailedStage_FallsBackToItsInput` 의 refuse·throw 실패 2 | `f-nofallback.txt` |

- 복원 뒤 재빌드는 모두 `BUILD_EXIT=0` 이다.
- 약화 ③ 에서 `short`(길이 불일치) 경우는 **다른 분기**라 통과했다. 이 경우의 폴백은 약화되지 않았다.
- 순서 시험에는 목록을 뒤집으면 호출이 뒤집히는 대조군이 있다(`Control_SwappedList_SwapsTheCalls`).
- 보간 문자열로 만든 설정은 처리로 세지 않고, 구조화된 객체로 넘긴 설정은 센다는 대조군이 있다(`Control_InterpolatedConfig_IsNotARead_StructuredConfigIs`).

## 3. 기준과 판단

- **`PreprocessInChain` 기본값을 꺼짐으로 했다.** 기존 화면과 시험을 바꾸지 않기 위해서다. 켜면 위 표의 동작이 된다.
- **Run Preprocessing 메뉴를 유지했다.** 메뉴는 단계를 켜는 지름길이 되었고, Mock 에서는 여전히 비활성이다(`CanRunPreprocessing`). Mock 에서 단계를 요청하는 길은 체크박스다.
- **`ProcessingChainPlan` 을 따로 두었다.** 단계 목록을 뷰모델이 만들면 `PreprocessInChain` 읽기가 연결 조사 시험에 보이지 않는다. 계획을 처리 진입점으로 등록했다.
- **해시 대상**: 뷰포트가 가진 처리 레이어다. `SourceOnly` 모드에서는 그 레이어를 그리지 않지만, 해시는 레이어 기준이다.
- **GSVG 단계는 넣지 않았다**(카드 결정 5).

## 4. 미검증

- **전처리 결과 픽셀의 정확성**: 해시가 달라졌다는 것만 확인했다. 올바르게 보정됐는지는 네이티브 모듈 시험의 몫이다.
- **kVp 가 결과를 바꾸는지**: kVp 를 바꿔서 픽셀이 달라지는지 재지 않았다. 전처리 단계가 kVp 를 쓰는지도 확인하지 않았다(#154/#155 의 "계산했지만 상쇄" 형태일 수 있다).
- **`AppliedNoChange`**: 실제 앱에서는 관측하지 않았다(통합 시험만).
- **3072² 에서의 해시 비용**: 재지 않았다(시험 영상은 1024²).
- **옛 하네스** `gui/ImageProcTest.E2E` 는 돌리지 않았다.
- **`.xcal` 의 화소 간격 필드**: 확인하지 않았다.

## 5. 잔여 위험

- **메모리**: 연쇄를 실행할 때마다 raw 복사본과 단계 출력이 생긴다(1024² 기준 약 2 MB × 2). 캐시는 아직 없다(C-97 6절).
- **오래됨 판정 확대**: 부위를 바꾸면 전처리가 꺼져 있어도 오래됨 표시가 뜬다. 부위 변경은 원래 VOI 프리셋을 다시 적용하므로 영향은 작다고 보지만 측정하지 않았다.
- **연쇄 실패 시 예외 경로**: `RunChain` 자체가 던지면(예: raw 없음) 기존 `StalePipelineFailed` 로 처리된다. 이 경우 연쇄 상태는 이전 값으로 남는다.
- **`LoadedImageFrame.RawPixels` 이름**: 여전히 raw 를 뜻한다. 디스플레이 결과 프레임도 같은 raw 를 들고 다닌다(의도).

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| RunChain / ChainResult / StageStatus | GUI-C-99 |
| 디스플레이 입력 = 연쇄 결과 | GUI-C-99 |
| raw 불변, 실패 시 폴백 + 경보 + 상태 | GUI-C-99 |
| 상태줄·스냅샷·오래됨·보고서 3종 | GUI-C-99 |
| kVp 단일 설정(기본 70, 화면 표시) | GUI-C-99 |
| 연결 조사 시험 경로 조정 (위험 b) | GUI-C-99 |
| 화소 간격 출처 | GUI-C-99 (결론 없음 — 값 넷) |
| GSVG 단계, 적용 신호 ABI, 제품용 표 | QA-B-101 (post 레인) |
| kVp 가 전처리 결과를 바꾸는지 | 새 카드 후보 |
| 포커스 토글 비활성 (C-98 판정 ②) | 별도 커밋 (이 보고서 뒤) |
