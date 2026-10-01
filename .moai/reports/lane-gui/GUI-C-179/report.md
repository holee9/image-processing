# GUI-C-179 — Troubleshooting 초안 (행 21 준비, 리더 검토용)

레인: gui · 이슈: `#225` · **초안만 쓴다. `docs/` 는 리더 소유이므로 반영하지 않았다.** `IsEnabled` 도 건드리지 않았다.
기준: 이 트리(`dev/gui`, main `edf93005` 병합 후)의 소스. 아무것도 실행하지 않았다.

## 결론

앱이 사용자에게 내는 문구 중 **문제를 알리는 41행**을 "문구 → 원인 → 조치" 표로 만들었다. 모든 행은 소스에서 뽑았고 **출처 `파일:줄` 이 스크립트로 계산**됐다(인용 조각이 파일에 실제로 있는지 단언하고, 없으면 생성을 멈춘다 — `anchors_checked.txt`).

**규칙**: 코드에 없는 증상은 쓰지 않았다. "원인" 은 문구를 만드는 조건·주석에서, "조치" 는 문구가 스스로 말하는 것(또는 같은 문구의 로그 줄)에서만 가져왔다. 코드가 조치를 정하지 않으면 "코드는 조치를 정하지 않는다." 라고 적었다 — 조치를 지어내지 않았다.

**알아 둘 것(리더 판단 사항)**: 이 문구들은 **상태줄·경고 목록·로그 패널**에 흩어져 뜬다. 문서가 "이 문구가 뜨면" 이라고 하려면 문구가 바뀔 때 문서가 같이 바뀌어야 한다 — 이 표의 출처 줄 번호는 오늘 시점이고, 문구 바뀜을 잡아 줄 시험은 없다(§미검증).

## 읽는 법 — 문구가 뜨는 곳

- **상태줄**: 대부분의 `A`·`B` 행(`StatusText = …`).
- **경고 목록**(`Alerts`)과 로그: GUI 가 만드는 경고(`B1`·`B2`·`B3`·`B6`·`B7`·`B14`·`B17`)와 네이티브 알림(`C`). 경고를 올리면 로그에 `ALERT <심각도> <코드>: <문구>` 줄이 같이 남는다(MainWindowViewModel.cs:3046).
- **네이티브 알림**은 모듈이 올리고 GUI 는 큐를 읽기만 한다. 모두 코드 `NATIVE_ALERT`(NativeAlertDrain.cs:23), 심각도는 `0→INFO`, `1→WARN`, `2→ERROR`, 그 밖의 값은 `ERROR` 로 올린다(NativeAlertDrain.cs:130).
- **정상 문구는 뺐다**: `Settings saved.`, `Layout reset.`, `… passed in N ms.` 처럼 문제를 알리지 않는 것.

## A. 메뉴·러너·내보내기 (상태줄)

| # | 뜨는 문구 | 원인 (코드가 말하는 것) | 조치 (코드가 말하는 것) | 출처 파일:줄 |
|---|---|---|---|---|
| A1 | `<라벨> needs the repository; this build is not running from a checkout.` (Self-check · GUI E2E · Benchmark runner · API reference 가 같은 문장) | 앱 실행 폴더에서 위로 올라가며 `docs/project/sprint-plan.md` 를 찾았으나 어디에도 없다 — 저장소 체크아웃 밖에서 실행 중 | 코드는 조치를 정하지 않는다. 문구가 말하는 것은 "이 빌드는 체크아웃에서 실행되고 있지 않다" 뿐이다 | MainWindowViewModel.cs:1485<br>MainWindowViewModel.cs:1423<br>MainWindowViewModel.cs:2747<br>GuiFixtureManifestService.cs:15 |
| A2 | `<라벨> runner is not built.` (Self-check · GUI E2E) | `gui/<프로젝트>/bin/Debug/net8.0-windows/<exe>` 가 없다 | **로그 패널**(상태줄 아님)의 같은 줄이 말한다: `… does not exist. Build gui/<프로젝트> first.` | MainWindowViewModel.cs:1493<br>MainWindowViewModel.cs:1494<br>MainWindowViewModel.cs:1480 |
| A3 | `Benchmark runner is not built: build/ci-post has no CTestTestfile.cmake. Build it first: 'cmake --preset ci-post' and then 'cmake --build --preset ci-post --parallel' (see .github/workflows/benchmark-regression.yml).` | `build/ci-post/CTestTestfile.cmake` 가 없다. 아무것도 실행하지 않는다 | 문구 자체: `cmake --preset ci-post`, 이어서 `cmake --build --preset ci-post --parallel` | BenchmarkRunnerService.cs:67<br>BenchmarkRunnerService.cs:54 |
| A4 | `<라벨> did not run: '<exe>' could not be started (<사유>). Nothing was executed.` | 프로세스를 **시작하지 못했다**(실행 파일이 없음 · 프로그램이 아닌 파일 · 시작 예외). 검증값은 `null` 이다. 이름만 준 실행 파일(예: `ctest`)이 OS 에서 "파일 없음"(오류 2 또는 3)이면 사유에 PATH 안내가 붙는다 | 사유가 말한다: `the file was not found; a bare name such as 'ctest' has to be on PATH: <OS 메시지>` | RunnerProcess.cs:112<br>RunnerProcess.cs:123 |
| A5 | `<라벨> FAILED (exit N): <줄>` | 프로세스가 **시작됐고** 종료 코드가 0 이 아니다. `<줄>` 은 요약 선택기가 없으면 stderr 의 **첫 줄**(없으면 stdout 의 마지막 줄), Benchmark 는 ctest 의 `N% tests passed` 줄부터 끝까지 | 코드는 조치를 정하지 않는다. `<줄>` 이 이유다 — 예: A8 | RunnerProcess.cs:117<br>RunnerProcess.cs:94<br>RunnerProcess.cs:92 |
| A6 | `<라벨> ended abnormally: <메시지>` | 프로세스는 시작됐는데 출력을 읽는 중 예외가 났다 — "돌았다" 쪽이며 "did not run" 이 아니다 | 코드는 조치를 정하지 않는다. | MainWindowViewModel.cs:1536 |
| A7 | `Self-check is already running.` / `GUI E2E is already running.` / `Benchmark runner is already running.` | 같은 명령이 이미 실행 중인 상태(실행 중 플래그)에서 다시 눌렸다 | 코드는 조치를 정하지 않는다. | MainWindowViewModel.cs:1329<br>MainWindowViewModel.cs:1365<br>MainWindowViewModel.cs:1408 |
| A8 | `Self-check FAILED (exit …): Unhandled exception. System.InvalidOperationException: The copy of the gui app that ImageProcTest.SelfCheck runs against is older than its sources, so this run would test the PREVIOUS build. '<앱 경로>' written <시각>; newest source '<파일>' written <시각>. Build the RUNNER, not the app: dotnet build gui/ImageProcTest.SelfCheck/ImageProcTest.SelfCheck.csproj -c Debug. Building gui/ImageProcTest alone does not refresh this copy.` (GUI E2E 는 라벨과 프로젝트 이름만 다름) | 러너(SelfCheck·E2E)가 들고 있는 앱 사본이 앱 소스보다 오래됐다. 러너는 오래된 빌드를 시험하지 않으려고 **예외로 멈춘다**(건너뛰지 않는다) | 문구 자체: `dotnet build gui/<러너 프로젝트>/<러너 프로젝트>.csproj -c Debug` — 앱 프로젝트만 빌드하면 이 사본은 갱신되지 않는다 | BuildFreshnessGuard.cs:65<br>RunnerBuildFreshness.cs:46<br>RunnerBuildFreshness.cs:47 |
| A9 | `API reference has not been generated. Generate it: in docs/help/doxygen run 'git clone https://github.com/jothepro/doxygen-awesome-css doxygen-awesome' and then 'doxygen Doxyfile' (Doxygen 1.12+; see docs/help/doxygen/README.md).` | `docs/help/generated/doxygen/html/index.html` 가 없고 출력 디렉터리(`docs/help/generated/doxygen`)도 없다 | 문구 자체: `doxygen-awesome-css` 를 `docs/help/doxygen` 에 클론한 뒤 `doxygen Doxyfile` (Doxygen 1.12+) | ApiReferenceService.cs:69<br>ApiReferenceService.cs:45<br>ApiReferenceService.cs:34 |
| A10 | `API reference is incomplete: docs/help/generated/doxygen exists but has no html/index.html. <A9 의 조치 문장>` | 출력 디렉터리는 있는데 진입 페이지(`html/index.html`)가 없다 — 생성이 중간에 멈춘 상태(코드 주석) | 문구에 A9 의 생성 방법이 이어진다 | ApiReferenceService.cs:68 |
| A11 | `API reference could not be opened: <메시지>` · `Evidence folder could not be opened: <메시지>` · `Runtime logs could not be exported: <메시지>` · `Export failed: <메시지>` | 해당 호출(브라우저·탐색기 실행, 파일 쓰기, 압축)이 예외를 던졌다. `<메시지>` 는 그 예외의 메시지 그대로다 | 코드는 조치를 정하지 않는다. | MainWindowViewModel.cs:2784<br>MainWindowViewModel.cs:2867<br>MainWindowViewModel.cs:1841<br>MainWindowViewModel.cs:2919 |
| A12 | `Runtime logs not exported: no run set has started.` · `No evidence folder to open: no run set has started.` · `Export failed: no run set has started.` | `RunSet.RunId` 가 비어 있다(런 세트가 시작되지 않았다). 코드 주석: 빈 id 로 내보내면 `evidence/` 를 그 안의 파일로 압축하게 된다(#178) | 코드는 조치를 정하지 않는다. | MainWindowViewModel.cs:1813<br>MainWindowViewModel.cs:2828<br>MainWindowViewModel.cs:2893<br>MainWindowViewModel.cs:2892 |
| A13 | `Runtime logs not exported: the log is empty.` | 내보낼 로그 줄이 0개다(`Logs.Count == 0`) | 코드는 조치를 정하지 않는다. | MainWindowViewModel.cs:1820 |
| A14 | `No evidence folder yet for this run set: <경로>` · `No evidence directory found for current run-set.` | `<앱 폴더>/evidence/<RunId>` 디렉터리가 아직 없다 | 코드는 조치를 정하지 않는다. | MainWindowViewModel.cs:2836<br>MainWindowViewModel.cs:2901<br>MainWindowViewModel.cs:1826 |

## B. 이미지 로드·처리·설정

| # | 뜨는 문구 | 원인 (코드가 말하는 것) | 조치 (코드가 말하는 것) | 출처 파일:줄 |
|---|---|---|---|---|
| B1 | `Load failed: <메시지>` (+ 경고 목록에 `ERROR LOAD_FAILED`) | 이미지 로드가 예외를 던졌다. 코드가 직접 만드는 메시지는 둘: `Raw width and height must be positive.`, `Raw file is too small. Expected at least <N> bytes, got <M>.` (그 밖의 메시지는 .NET 의 예외 메시지) | 코드는 조치를 정하지 않는다. | MainWindowViewModel.cs:1999<br>MainWindowViewModel.cs:2004<br>RawImageLoader.cs:28<br>RawImageLoader.cs:35 |
| B2 | `Display pipeline requires a loaded raw image.` (+ `WARN DISPLAY_NO_IMAGE`) · `Load a raw image before running preprocessing.` | 활성 이미지 프레임이 없다(`ActiveImageFrame is null`) | 문구 자체: 먼저 raw 이미지를 불러온다 | MainWindowViewModel.cs:2036<br>MainWindowViewModel.cs:2041<br>MainWindowViewModel.cs:2177 |
| B3 | `Display pipeline failed: <메시지>` (+ `ERROR DISPLAY_PIPELINE_FAILED`, 미리보기에 `STALE — the display pipeline failed; the image shown is from before the failed attempt.`) | 디스플레이 파이프라인 호출이 예외를 던졌다. 화면의 처리 이미지는 **시도 이전의 것**이며 다음 렌더가 성공하거나 새 이미지를 불러올 때까지 STALE 로 표시된다. 네이티브 오류 코드면 메시지는 `<함수> failed with XPE error code <코드>.`(코드 표는 부록) | 코드는 조치를 정하지 않는다. | MainWindowViewModel.cs:2120<br>MainWindowViewModel.cs:2125<br>MainWindowViewModel.cs:727<br>RealXpeBackend.cs:422 |
| B4 | `STALE — display parameters changed since this image was rendered. Apply the display pipeline to update it.` | 표시 파라미터를 바꿨고 마지막으로 렌더한 값과 다르다 | 문구 자체: 디스플레이 파이프라인을 적용한다 | MainWindowViewModel.cs:719 |
| B5 | `Stop requested; the render in flight will be discarded.` · `Render stopped; the result was discarded after <N> ms of work.` · `Stop: no render is in flight.` | 정지를 눌렀다 → 진행 중인 렌더가 있었다면 결과를 버린다 / 진행 중인 렌더가 없었다. 정지 뒤 미리보기는 `StalePipelineFailed` 사유로 표시된다 | 코드는 조치를 정하지 않는다. | MainWindowViewModel.cs:1269<br>MainWindowViewModel.cs:2088<br>MainWindowViewModel.cs:1263 |
| B6 | `Preprocessing failed: <메시지>` · `VOI preset failed: <메시지>` (+ `ERROR VOI_PRESET_FAILED`) · `Backend initialization failed: <메시지>` | 해당 단계가 예외를 던졌다. 메시지는 예외의 것이다. 초기화 실패는 현재 런 세트를 유지한다(코드 주석) | 코드는 조치를 정하지 않는다. | MainWindowViewModel.cs:2189<br>MainWindowViewModel.cs:2259<br>MainWindowViewModel.cs:2264<br>MainWindowViewModel.cs:1634 |
| B7 | 경고 목록: `WARN PREPROCESS_NOT_RUN` / `WARN CHAIN_STAGE_NOT_APPLIED` — `<단계> was requested and not applied; the display used its input. <사유>` | 요청한 처리 단계가 적용되지 않았다(`RequestedNotApplied`). 화면은 그 단계의 **입력**을 쓴다. `<사유>` 는 아래 B8–B12 중 하나 | 사유 문장이 말한다 | MainWindowViewModel.cs:2227<br>MainWindowViewModel.cs:2226<br>MainWindowViewModel.cs:2226 |
| B8 | `Preprocessing skipped: calibration file(s) not found — <경로들>. Generate a set with xpe_calib_fixture_gen --out <dir> and point the calibration directories at it.` | 오프셋·게인·결함 맵 파일 셋 중 하나 이상이 지정한 디렉터리에 없다 | 문구 자체: `xpe_calib_fixture_gen --out <dir>` 로 한 벌을 만들고 보정 디렉터리를 그곳으로 지정한다 | GuiPreprocessRunner.cs:55<br>GuiPreprocessRunner.cs:56 |
| B9 | `xpe_preprocess_init failed (<코드>).` · `Loading <offset\|gain\|defect> calibration failed (<코드>).` · `xpe_offset_correct failed (<코드>).` · `xpe_nonlinearity_correct failed (<코드>).` · `xpe_gain_correct failed (<코드>).` · `xpe_defect_correct failed (<코드>).` | 해당 네이티브 호출이 0 이 아닌 코드를 돌려줬다. 코드의 뜻은 부록의 `xpe_error.h` 표 | 코드는 조치를 정하지 않는다. | GuiPreprocessRunner.cs:64<br>GuiPreprocessRunner.cs:79<br>GuiPreprocessRunner.cs:128<br>GuiPreprocessRunner.cs:146<br>GuiPreprocessRunner.cs:152<br>GuiPreprocessRunner.cs:158 |
| B10 | `Preprocessing requires the native backend (xpe_preprocess.dll).` · `Grid correction requires the native backend (gsvg.dll).` · `Stage '<id>' is not available in the mock backend.` · `Stage '<id>' is not available in the native backend.` | Mock 백엔드가 활성이다(처리 단계는 네이티브가 필요하다) / 백엔드가 그 단계 id 를 모른다 | 코드는 조치를 정하지 않는다. | MockXpeBackend.cs:332<br>MockXpeBackend.cs:333<br>MockXpeBackend.cs:334<br>RealXpeBackend.cs:297 |
| B11 | `<단계> threw: <메시지>` · `<단계> returned <N> pixels for an input of <M>.` | 단계 실행기가 예외를 던졌다 / 단계가 입력과 다른 픽셀 수를 돌려줬다(체인은 단계를 적용하지 않은 것으로 기록한다) | 코드는 조치를 정하지 않는다. | ProcessingChainRunner.cs:61<br>ProcessingChainRunner.cs:75 |
| B12 | GSVG: `The virtual grid needs a parameter table; none was configured and none was found beside gsvg.dll.` · `The virtual-grid table was not found: <경로>` · `xpe_gsvg_init refused the configuration (<코드>). See the alerts for the reason.` · `GSVG refused this image: <상세>` | 가상 그리드 파라미터 표가 설정에도 gsvg.dll 옆에도 없다 / 설정한 경로에 파일이 없다 / 모듈이 설정을 거부했다 / 모듈이 이미지를 거부했다(코드≠0) | `See the alerts for the reason.` — 이유는 C 의 GSVG 알림에 있다 | GuiGsvgRunner.cs:95<br>GuiGsvgRunner.cs:100<br>GuiGsvgRunner.cs:133<br>GuiGsvgRunner.cs:168 |
| B13 | GSVG 모듈 판정 문구: `GSVG was requested but no correction was enabled in its config — check the key names.` · `The image is too small for GSVG: <상세>` · `GSVG saw a grid peak but no sub-band confirmed it; nothing was filtered and a grid may remain.` · `The virtual grid refused this exposure or these settings; the module restored the original.` · `GSVG returned a reason this build does not know: <상세>` | 모듈이 돌려준 reason 값(`NotConfigured` · `ImageTooSmall` · `GridNotInSubbands` · `VirtualGridRefused` · 모르는 값)에 대응한다. `<상세>` 는 `reason=…, vignette=…, grid=…, virtualGrid=…, restoredOriginal=…, code=…` | `NotConfigured` 만 조치를 말한다: 설정 키 이름을 확인한다. `GridNotInSubbands` 는 "격자가 남아 있을 수 있다" 고 말한다 | GuiGsvgRunner.cs:180<br>GuiGsvgRunner.cs:181<br>GuiGsvgRunner.cs:183<br>GuiGsvgRunner.cs:185<br>GuiGsvgRunner.cs:186 |
| B14 | `Your saved settings could not be read, so this session started from defaults. The original file was kept at '<경로>'.` (+ `WARN SETTINGS_UNREADABLE`) — 반복 실패면 둘째 문장이 `An earlier failure already rescued your original settings, kept at '<경로>'.` | 저장된 설정 파일을 읽지 못해 기본값으로 시작했다. 원본은 보존돼 있다 | 문구가 말한다: 원본 파일의 위치 | MainWindowViewModel.cs:220<br>MainWindowViewModel.cs:223<br>MainWindowViewModel.cs:228 |
| B15 | `Recent file is not available: <경로>` | 목록의 최근 파일이 그 경로에 없다. 코드 주석: 끊긴 공유 폴더의 파일은 돌아올 수 있어 **목록에서 지우지 않는다** | 코드는 조치를 정하지 않는다. | MainWindowViewModel.cs:1708<br>MainWindowViewModel.cs:1706 |
| B17 | 경고 목록: `ERROR VERDICT_WRITE_FAILED` — 문구는 예외 메시지 그대로 (로그에는 `Failed to write verdict file: <메시지>` 가 따로 남는다) | 판정(verdict) 파일을 쓰는 중 예외가 났다 | 코드는 조치를 정하지 않는다. | MainWindowViewModel.cs:2682<br>MainWindowViewModel.cs:2678 |
| B16 | `Could not copy the log line: <메시지>` · `Detached comparison viewer failed to open: <예외 이름>.` | 클립보드가 다른 프로세스에 잡혀 있을 수 있다(코드 주석) / 분리 비교 뷰어 창을 여는 중 예외 | 코드는 조치를 정하지 않는다. | MainWindowViewModel.cs:348<br>MainWindowViewModel.cs:346<br>MainWindowViewModel.cs:2365 |

## C. 네이티브 알림 (모듈이 올림, 경고 목록에 `NATIVE_ALERT` 로 뜸)

심각도는 문구 앞 괄호. GUI 가 실제로 부르는 모듈(`xpe_common`·`xpe_preprocess`·`xpe_display`·`gsvg`)의 알림 중 **GUI 가 도달하는 호출 경로에 있는 것**만 넣었다(제외 목록은 아래).

| # | 뜨는 문구 | 원인 (코드가 말하는 것) | 조치 (코드가 말하는 것) | 출처 파일:줄 |
|---|---|---|---|---|
| C1 | `1 alert was dropped because the alert queue overflowed.` / `<N> alerts were dropped because the alert queue overflowed.` (`ERROR ALERT_QUEUE_OVERFLOW`) | 네이티브 알림 큐가 넘쳐 알림이 버려졌다. SRS: 용량 64 초과 push | 코드는 조치를 정하지 않는다. | AlertDisplayFormatter.cs:53<br>AlertDisplayFormatter.cs:54<br>NativeAlertDrain.cs:26<br>XPE-SRS-001_Software_Requirements_Specification.md:105 |
| C2 | `[unreadable alert]` (`ERROR NATIVE_ALERT`) | 큐 항목 하나를 읽는 호출이 0 이 아닌 코드를 돌려줬거나 메시지가 비었다(`BUFFER_TOO_SMALL` 이면 버퍼 8192 로 한 번만 재시도한다). 조용히 버리지 않고 이 문구로 남긴다 | 코드는 조치를 정하지 않는다. | NativeAlertDrain.cs:20<br>NativeAlertDrain.cs:32 |
| C3 | (WARN) `ALL <N> pixel(s) fell outside the gain polynomial's fitted dose range [<a>, <b>] -- the whole frame was evaluated at one range edge, so the gain applied is effectively constant. Check that the calibration's dose levels are in pixel values (ADU): a file fitted in other units loads without error and produces this (issue #194)` | 게인 다항식의 적합 선량 범위 밖에 **프레임 전체**가 있다 | 문구 자체: 보정의 dose levels 가 화소값(ADU)인지 확인한다 — 다른 단위로 맞춘 파일도 오류 없이 로드된다 | gain_correct.cpp:395<br>gain_correct.cpp:399 |
| C4 | (WARN) `<N> pixel(s) fell outside the gain polynomial's fitted dose range [<a>, <b>] and were evaluated at the range edge; values beyond the calibrated levels are not extrapolated (issue #194)` | 일부 화소가 적합 범위 밖이라 범위 끝값으로 계산했다(외삽하지 않음) | 코드는 조치를 정하지 않는다. | gain_correct.cpp:405 |
| C5 | (WARN) `gain polynomial dose levels span [<a>, <b>], far below the pixel-value range they index (0..65535). CHECK THE UNIT OF THIS CALIBRATION'S dose_levels: … If the unit is right and the calibration is simply very dark, this warning can be ignored. This is a magnitude check, not a unit check -- the file carries no unit field (issue #194)` | 게인 보정 파일을 로드할 때 dose levels 의 최댓값이 너무 작다(크기 검사이며 단위 검사가 아니다) | 문구 자체: dose_levels 의 단위를 확인한다. 단위가 맞고 보정이 정말 어두울 뿐이면 무시해도 된다 | xpe_calib_load_gain.cpp:195<br>xpe_calib_load_gain.cpp:197<br>xpe_calib_load_gain.cpp:201 |
| C6 | (WARN) `gain polynomial loaded without a dose range: this file was generated before the range field existed, so the out-of-range clamp does not apply to it and pixel values beyond the fitted levels are extrapolated; regenerate the calibration to enable the clamp (issue #194)` · (WARN) `gain polynomial carries an inverted dose range [<a>, <b>]: the clamp is not applied and pixel values are extrapolated. … regenerate it (issue #194)` | 게인 파일에 선량 범위 필드가 없다 / 범위가 뒤집혀 있다(생성 당시 dose levels 가 오름차순이 아니었다) | 문구 자체: 보정을 다시 생성한다 | xpe_calib_load_gain.cpp:214<br>xpe_calib_load_gain.cpp:231 |
| C7 | (WARN) `panel.nonlinearity_mode selects the polynomial method but no panel.nonlin_poly_c0..c4 coefficients are present; falling back to the LUT method` · (ERROR) `nonlinearity polynomial has a non-finite coefficient; falling back to the LUT method` · (WARN) `nonlinearity polynomial is non-monotone in [0, ADC_max]; rejected and falling back to the LUT method` | 다항식 방식을 골랐으나 계수가 없다 / 계수가 유한하지 않다 / 다항식이 단조가 아니다 — 모두 LUT 방식으로 되돌아간다 | 코드는 조치를 정하지 않는다. | nonlinearity_correct.cpp:65<br>nonlinearity_correct.cpp:74<br>nonlinearity_correct.cpp:107 |
| C8 | (ERROR) `panel.linear is false but no nonlinearity LUT is loaded; call xpe_calib_load_nonlin_lut() before processing` · (WARN) `nonlinearity correction did nothing: no LUT is loaded and panel.linear is not "false", so the frame passed through unchanged; load a LUT with xpe_calib_load_nonlin_lut() if this panel needs correcting (issue #196)` | 비선형성 보정이 필요한 패널인데 LUT 가 로드되지 않았다(ERROR) / LUT 도 없고 `panel.linear` 도 "false" 가 아니라 프레임이 그대로 지나갔다(WARN) | 문구 자체: `xpe_calib_load_nonlin_lut()` 로 LUT 를 로드한다 | nonlinearity_correct.cpp:238<br>nonlinearity_correct.cpp:356 |
| C9 | (WARN) `gsvg config key '<키>' is not read by this module and has no effect` | GSVG 설정 JSON 에 모듈이 읽지 않는 키가 있다 | 코드는 조치를 정하지 않는다. 문구는 "효과가 없다" 고만 말한다 | gsvg.cpp:131 |
| C10 | (ERROR) `gsvg virtual grid: <이유>` · (WARN) `gsvg virtual grid: no collimation field mask; scatter outside the field is treated as object` · (WARN) `gsvg virtual grid: thickness above the table in <x>% of the pixels (limited to the table maximum in <y>% of the reduced grid)` | 가상 그리드 설정·표 읽기가 `<이유>` 로 실패했다(`<이유>` 는 모듈의 오류 문자열 그대로) / 시준 마스크가 없어 필드 밖 산란을 물체로 취급한다 / 두께가 표 범위를 넘는 화소가 있어 표 최댓값으로 제한했다 | 코드는 조치를 정하지 않는다. | gsvg.cpp:240<br>gsvg.cpp:463<br>gsvg.cpp:483 |

## 부록 — 네이티브 오류 코드 (`xpe_error.h:47` 부터)

`B3`·`B9`·`B12` 의 `<코드>` 가 가리키는 값이다.

| 값 | 이름 | 뜻 (헤더 주석) |
|---|---|---|
| `0` | `XPE_OK` | Success — operation completed without error |
| `-1` | `XPE_ERR_INVALID_INPUT` | NULL pointer, wrong pixel format, or invalid parameter value |
| `-2` | `XPE_ERR_OUT_OF_MEMORY` | Heap allocation failure |
| `-3` | `XPE_ERR_PROCESSING_FAILED` | Internal algorithm failure (e.g. singular matrix, zero-mean image) |
| `-4` | `XPE_ERR_CONFIG_INVALID` | Configuration file missing, malformed, or version mismatch |
| `-5` | `XPE_ERR_CALIBRATION_EXPIRED` | Calibration data present but outside its valid-use window |
| `-6` | `XPE_ERR_NOT_INITIALIZED` | Module-level init function has not been called |
| `-7` | `XPE_ERR_UNSUPPORTED_FORMAT` | Pixel format or image dimensions not supported by this function |
| `-8` | `XPE_ERR_BUFFER_TOO_SMALL` | Caller-supplied output buffer is smaller than required |
| `-9` | `XPE_ERR_IO_FAILED` | File read or write failure |
| `-10` | `XPE_ERR_NETWORK_FAILED` | Network communication failure (DICOM send/receive) |

## 뺀 것과 이유

- **`PipelineOrchestrator.cs` 의 메시지**(`DICOM read failed:` 등): 어디서도 참조되지 않는 파일이라 앱이 내지 않는다(C-152 §3).
- **Mock 백엔드가 만드는 경고 3건**(`MockXpeBackend.cs` 의 `AlertEntry`): Mock 전용 연출이다. 실백엔드 INFO `REAL_DISPLAY_BACKEND_ACTIVE` 도 문제 알림이 아니라서 뺐다.
- **`xpe_calib_generate_gain` 의 `XPE_WARN_CALIB_POOR_FIT` 알림**: GUI 는 이 함수를 부르지 않는다(`gui/ImageProcTest` 에서 `xpe_calib_load_offset`·`_gain`·`_defect_map` 세 로더만 호출됨을 확인) — 도달하지 못하는 증상은 쓰지 않았다.
- **AI·`enhance_*`·dicom 모듈 알림**: GUI 에 연동이 없다(C-175·C-178).
- **`.NET` 예외의 본문**(`A11`·`B1`·`B6` 의 `<메시지>`): 코드에 없는 문장이라 원인을 지어내지 않고 "예외의 메시지 그대로" 라고만 적었다.
- 정보성 상태 문구(`Applied … VOI preset`, `Loaded …`, `Backend initialized: …`, 비교 뷰포트 문구 등).

## 미검증 · 한계

1. **아무것도 실행하지 않았다.** 각 행의 "원인" 은 **문구를 만드는 조건을 읽은 것**이지 그 상황을 재현한 것이 아니다. 예외 한 가지(`A8`)는 GUI-C-177 의 로컬 실행에서 실제로 본 문구라 관측이지만(`GUI-C-177/local_runs.txt`), 나머지는 읽은 것이다.
2. **줄 번호는 오늘 시점이다.** 소스가 바뀌면 어긋난다. 문서에 줄 번호를 넣을지, 파일 이름·문구만 넣을지는 리더가 정할 일이다.
3. **문구↔문서 일치를 지켜 줄 시험이 없다.** 문서에 인용된 문구가 코드에서 바뀌면 문서가 조용히 낡는다(이 저장소가 여러 번 겪은 일 — 인용한 이름은 grep 으로 대조해야 한다). 문서화한다면 "문서의 인용 문구가 소스에 있다" 를 단언하는 시험이 같이 필요하다.
4. **`C` 의 네이티브 알림은 도달 가능성을 호출부로만 판단했다.** 예: `C7`·`C8` 은 GUI 가 `xpe_nonlinearity_correct` 를 부르므로 포함했지만, 어떤 설정에서 실제로 그 분기에 들어가는지는 재현하지 않았다.
5. **GSVG 가상 그리드의 `<이유>`**(`C10`)는 모듈이 문자열로 돌려주는 것이라 가능한 이유 전체를 열거하지 못했다(`gsvg.cpp` 의 `rep.error` 생성부를 읽지 않았다).
6. `A2` 의 조치는 **로그 패널**에만 있다(상태줄에는 "runner is not built." 만). 사용자가 로그 패널을 보지 않으면 조치를 못 본다 — 문서가 이 점을 말해 줄 가치가 있지만, 코드 사실이 아니라 제안이다.
7. 번역·문체는 손대지 않았다. 문구는 영어 원문 그대로 인용했고 열 제목·원인·조치만 한국어다.

## 증거 파일

`anchors_checked.txt`(행·파일·줄·그 조각이 나온 횟수) · `text_lint.txt`

🗿 MoAI
