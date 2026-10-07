# GUI-C-231 보고 — 운영자 앱으로 1단계(실데이터 전처리)를 따라 하는 도움말 + 실행 스크립트 초안 (Refs #251)

## 한눈에

- 빠른 시작(`quick-start.html`)을 현재 앱에 맞게 다시 썼고(`index`·`scope`·`styles.css` 포함), 적은 절차를 **UIA 만으로 실제 앱에서 따라 해서** 단계마다 화면 문구를 증거로 남겼습니다(전경 키·마우스 입력 없음).
- 따라 하다가 **제품 결함 2건**과 **함정 몇 가지**가 나왔습니다. 고치지 않았고(카드 범위 밖) 아래에 적었습니다. 가장 큰 것: **앱에는 원본 영상의 가로·세로를 넣는 칸이 없고 파일 크기에서 추론도 안 해서, 크기가 작게 잡히면 아무 오류 없이 잘못된 영상이 열립니다.**
- 실행 스크립트는 `Start-ImageProcTestNative.ps1.txt` 초안으로 이 폴더에 있습니다(`tools/` 로 옮기는 것은 리더). 도움말이 `tools\Start-ImageProcTestNative.ps1` 이름을 인용하므로 **같이 옮겨야** 합니다.
- **미관측**: 보정된 화면 자체(배경이 평평해지는 정도, 결함 화소가 사라지는지)는 판정하지 못했습니다. 실데이터 xcal 세트(`calib-real-v2`)는 아직 없었고, 앱은 화소 수치를 보여 주지 않으며 UIA 로 화면 화소를 읽지 않았습니다. 문서는 이를 "기대"로 적고 판정하지 않았다고 밝혔습니다.

## 바뀐 파일

| 파일 | 내용 |
|---|---|
| `gui/ImageProcTest/help/quick-start.html` | Native 로 띄우기 → 보정 패널 → 원본 열기 → Run Preprocessing → 비교 → 흔한 실패 표 → 이 점검이 다루지 않는 것 |
| `help/scope.html`, `help/index.html` | "Mock 전용·native 경로 없음·Phase 1a 제외" 를 현재 상태로 정정 |
| `help/styles.css` | 실패 표용 `table` 규칙 |
| `clients/ImageProcTest.E2ETests/Scenarios/Operator/QuickStartWalkthroughScenarios.cs` (신규) | W0 문서-앱 대조, W1~W6 따라 하기·실패 문구·재시작·크기 오류·패널. W1~W6 은 `XPE_C231_NATIVE_DIR` 가 있을 때만 발견되고(없으면 **건너뜀이 아니라 목록에 없음** — CI Native 잡의 '설명 안 된 건너뜀' 게이트를 울리지 않으려고), W0 은 데이터 없이 항상 돈다 |

## 따라 한 결과 (증거 파일, 모두 이 폴더)

| 단계(문서) | 관측 | 파일 |
|---|---|---|
| 1 Native 로 띄우기 (스크립트 경로) | 제목에 `[MOCK]` 없음, 배너 없음, 하단 `mode=Native \| common=xpe_display 1.0.0 \| display=1.0.0 \| src=c231_native_fresh` | `walkthrough_w2_*` |
| 1 메뉴 경로 `Backend > Backend Mode > Native` | 전환은 되지만 **Run Preprocessing 은 꺼진 채** — 아래 결함 A. `File > Save Settings` → 닫고 재시작하면 켜짐(`settings backendMode: "Native"`) | `walkthrough_w1_*`, `walkthrough_w4_*` |
| 2 `View > Calibration Paths Panel` | 패널 `Calibration paths`, 기본 경로는 `data/calibration/offset·gain·defect`(빈 경로일 때만 `(not set)`). Browse… 3회 → 경로 표시 | `walkthrough_w1_*`, `walkthrough_w6_*` |
| 3 `File > Open Raw...` | 대화상자 `Load Raw Image` 로 열기, 요약 `RAW 3072x3072, min=2481, max=15451, bytes=18874368` | `walkthrough_w1_*`, `w2_*` |
| 3 크기를 잘못 줬을 때 | 1024×1024 로 3072 파일 열기: **오류 없이** `RAW 1024x1024 … bytes=18874368`. 4096×4096 으로 열기: `Load failed: Raw file is too small. Expected at least 33554432 bytes, got 18874368.` | `walkthrough_w5_*` |
| 4 `Pipeline > Run Preprocessing (Phase 1a)` | 3072×3072 + pre 의 a240b 맵: `chain: preprocess=Applied … times: preprocess=276–300 ms … display input=chain`. 로그 `Chain preprocess: Applied — Preprocess: offset -> nonlinearity -> gain -> defect on 3072x3072 (Abdomen).` + WARN `nonlinearity correction did nothing: no LUT is loaded…` | `walkthrough_w2_script_route_3072`, `…_1024_generated_set` |
| 5 `View > Compare Mode` | Difference / Processed Only / Source Only 각각 선택 시 체크 표시가 해당 항목으로 | `walkthrough_w2_*` |

흔한 실패(전부 실제로 만들어서 본 문구, `walkthrough_w3_*`):

| 만든 상황 | 앱이 보인 말 |
|---|---|
| 맵 없는 폴더 | `preprocess: Preprocessing skipped: calibration file(s) not found — …\offset.xcal, …\gain.xcal, …\defect.xcal` |
| 만료된 맵(`--expiry-ms 1`) | `preprocess: Loading offset calibration failed (-5)` |
| 맵 1024, 원본 3072 | 최신 DLL `xpe_offset_correct failed (-1)` / 10-03 DLL `(-8)` — 원인(크기 불일치)을 말하지 않음 |
| gain 값 범위 밖 | `Loading gain calibration failed (-17)` — **10-03 DLL 로만** 관측. 같은 pre a240b 맵을 10-07 DLL 은 받아들여 `Applied` |
| DLL 폴더에 DLL 없음 | 제목 `[MOCK] …`, `mode=Mock (requested Native)`, 배너 `MOCK BACKEND — Native was requested but could not be used…`, Run Preprocessing 꺼짐 |

문서의 `<code>`·`<em>` 인용 31개를 증거 파일에서 문자 그대로 찾는 대조(`doc_vs_evidence.txt`): **31/31 발견**. 첫 대조에서는 6개가 빠졌고 그중 2건이 문서 오류였습니다: ① 맵 패널의 기본 표시를 `(not set)` 이라고 썼는데 실제는 `data/calibration/…` 기본 경로(빈 경로일 때만 `(not set)`) → 정정하고 W6 으로 두 경우를 관측, ② 걸린 시간 `293 ms` 가 한 번의 실측값 → `<n> ms` 로 바꿈. 나머지 4개는 예시 문구와 산문이었습니다(첫 대조의 출력은 파일로 남기지 않았고 대화에서 본 것을 적었습니다). 또 W0(문서의 굵은 메뉴 경로 9개가 실제 앱에 있는지 UIA 로 확인)이 첫 실행에서 `Pipeline > Run Preprocessing` 한 곳이 `Run Preprocessing (Phase 1a)` 와 다르다고 빨갛게 잡았고 고쳤습니다(`doc_menu_paths_check_first_red.txt` → `doc_menu_paths_check.txt`).

## 제품 쪽에서 나온 것 (고치지 않음 — 리더 결정)

- **A. 메뉴로 Native 로 바꾸면 Run Preprocessing 이 꺼진 채** (`MainWindowViewModel.cs:780` `CanRunPreprocessing => _backend.SupportsPreprocessing` 가 변경 알림이 없음, 같은 곳 `:1983` 은 `CanRunDeterministicBaseline` 만 `OnPropertyChanged`). 한 줄 수정 후보. 지금은 도움말이 "저장 후 재시작" 을 안내합니다.
- **B. 원본 크기 입력이 없음.** 앱은 `rawWidth`/`rawHeight`(기본 1024×1024)만 읽고 파일에서 추론하지 않으며, 파일이 더 길면 앞부분만 읽어 **오류 없이** 잘못된 영상을 엽니다(`RawImageLoader.LoadRaw`, W5 관측). 후보: 파일 길이에서 알려진 크기로 추론(레거시 앱 `RawPreviewService.KnownDimensions` 와 같은 방식)하거나, `bytes` 가 가로×세로×2 보다 크면 경고. 지금은 스크립트가 `--automation-width/height` 를 넣어 줍니다.
- C. 오류 문구가 코드만 말합니다(`-8`/`-1` 이 크기 불일치임을 모름).
- D. 낡은 글자들: 창 제목 `ImageProcTest GUI-S0`, 하단 줄의 `CalibrationEval(… preprocess native bridge pending)` 이 보정이 `Applied` 된 뒤에도 그대로, 하단의 `common=xpe_display 1.0.0`(common 칸에 display DLL 이름·버전이 나옴 — 의도인지 모름).
- E. 로그 패널은 기본 꺼짐(`Logs panel checked before: False`)이라 View 메뉴에서 켜고 Log 탭을 눌러야 보입니다(문서에 적음).

## 앱 단계별 호출이 모듈 파이프라인과 다른 점 (코드를 읽은 것 — 실행 비교 아님)

앱: offset → nonlinearity(설정 없음 → LUT 없으면 변화 없음) → gain → defect (`GuiPreprocessRunner`). 모듈 `pipeline.cpp`: 0.5 readout 검증, 1 온도 보상, 2 offset, 3 nonlinearity, 4 gain, 5 binning, 6 defect, 7 ghost. 앱은 0.5·1·5·7 을 안 돌립니다. 노출(mAs 2.0, SID 1000)은 고정, kVp·화소 간격은 설정값. 보정 결과는 float 을 최댓값 기준 16비트로 늘려 표시용으로만 씁니다(밝기 비교 불가, 모양만). 보정 영상 저장 명령 없음. 3단계 카드가 `xpe_preprocess_pipeline_out` 로 바꿀 때 달라지는 점으로 쓰시면 됩니다. 문서·scope 에도 적었습니다.

## 실행 스크립트 초안 (`Start-ImageProcTestNative.ps1.txt`)

앱을 `--automation-backend Native --automation-calib <dir> --automation-width W --automation-height H` 로, `XPE_NATIVE_DIR`·`XPE_NATIVE_DIR_EXCLUSIVE=1` 은 **그 프로세스에만** 주어 띄웁니다. 빠진 DLL·xcal 은 한꺼번에 나열하고 멈춥니다. Windows PowerShell 5.1 에서 시험(ASCII only, `ArgumentList` 대신 인용된 `Arguments`).

**어느 DLL 이 최신 main 과 맞는가** — DLL 이 자기 커밋을 말해 주지 않으므로 있는 것만 씁니다(`script_test_observed.txt`): ① `provenance.json` 이 있으면 md5 를 다시 계산하고(`MODIFIED`), `headSha` 와 `origin/main` 사이에 `modules/` 파일이 바뀌었는지로 `MATCH`/`STALE`; ② 없으면 "DLL 이 `modules/` 를 건드린 마지막 커밋보다 **오래됐으면** 그 커밋을 담을 수 없다 → `STALE`", 새것이면 `UNPROVEN`(어느 커밋인지 증명 못 함). `-RequireFresh` 는 STALE·MODIFIED 에서 멈춥니다. 시험: 10-03 DLL → STALE, 10-07 DLL → UNPROVEN, `c134-stage-new`·`ci-common/bin` → MODIFIED(두 번째는 파이썬 hashlib 로 24개 전부 다름을 별도 확인), MATCH·STALE(provenance 경로)는 **합성 provenance** 로 로직만 시험.

**이 점검이 첫 시도의 오류를 잡았습니다**: 제가 처음 따라 한 DLL(`%TEMP%\c215_native`, 10-03 빌드)이 STALE 이었고, 그 DLL 로는 pre 의 a240b gain 맵이 `-17` 이었습니다. 이후 모든 증거는 `xpe-data/app-run`(10-07 16:20, pre/리더가 방금 둔 것)의 DLL 5개를 복사해(원본은 건드리지 않음) 다시 만든 것입니다. 이 DLL 도 provenance 가 없어 `UNPROVEN` — **최신 main 과 맞다고 증명하지 못했습니다.**

## 미검증 (Gaps)

- 보정 영상 자체(평평한 배경, 결함 화소 사라짐): 판정 안 함. 실데이터 세트(`calib-real-v2`)는 없었고(pre 가 만드는 중), 맵은 pre 의 `build/a240b/maps`(3072×3072, 읽기만)와 `xpe_calib_fixture_gen` 이 만든 1024 합성 세트(`defect 4 pixels`)를 썼습니다. 3072 합성 세트 생성은 수 분을 넘겨 중단했습니다.
- `-17` 이 10-03 DLL 에서만 나고 10-07 DLL 에서는 같은 맵이 통과한 이유는 조사하지 않았습니다(DLL 검사 변경인지 맵 읽기 변경인지 모름). `calib-real-v2` 의 gain 이 0.1~10 안인지는 pre 가 확인해야 합니다.
- 스크립트 없이 일반 실행(기본 settings 파일)에서 `--automation-*` 스위치가 같은 효과인지는 코드(`ApplyRunSelection` 주석)로만 알고 **관측은 named settings 파일을 쓴 경우**였습니다. `-Git` 지정, PowerShell 7, `build\e2e-native-dlls` 기본값 경로, `-SettingsFile` 없이 Save Settings 가 exe 폴더 appsettings.json 을 쓰는 것은 시험하지 않았습니다.
- W5(크기 오류)는 설정 파일 기본값(Mock)으로 띄워 관측했습니다. Native 도 같은 `RawImageLoader` 를 쓰는 것은 코드(`RealXpeBackend.LoadRawImage`)로 확인했지만 Native 로 다시 관측하지는 않았습니다.
- 한국어 Windows 한 대에서만. 파일/폴더 대화상자의 id(`1148`·`1152`·`1`)가 다른 로캘에서 같은지는 모릅니다(실패하면 어서션 메시지와 함께 빨강).
- 도움말은 영어로 썼습니다(앱 UI·기존 번들·`H01` 시험이 영어). 한국어판이 필요하면 따로 지시해 주세요.
- E2E W0 은 CI 에서도 돕니다(GUI 한 번 띄워 메뉴 경로 대조). 메뉴 조회가 느려 흔들릴 수 있어 항목이 나타날 때까지 5초 기다리게 했지만 CI 에서의 안정성은 CI 로 확인되어야 합니다.

## 리더 결정 요청

1. `Start-ImageProcTestNative.ps1.txt` 를 `tools/Start-ImageProcTestNative.ps1` 로 옮길지(도움말이 이 이름을 인용).
2. 결함 A(한 줄), B(크기 추론/경고)를 카드로 만들지.
3. 3단계(`pipeline_out`) 카드에 "앱 단계별 호출 차이" 목록과 D·E 항목을 넘길지.
