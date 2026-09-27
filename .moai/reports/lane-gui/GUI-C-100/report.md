# GUI-C-100 보고 — 화소 간격 140 µm 통일, kVp 효과 측정, GSVG 단계 준비 (#180, #182)

- 커밋: `dev/gui` `aa17f9b` (미푸시, main `c72f0e8` 위)
- 증거: `build/e2e-c100/`
- 모듈·표는 `origin/main` `c72f0e8` 에서 읽었다.

## 1. 화소 간격 140 µm (사용자 결정)

### 바꾼 곳

| 파일 | 이전 | 이후 | 이유 |
|---|---|---|---|
| `gui/ImageProcTest/Models/AppSettings.cs` | — | `PixelPitchMm` 추가, 기본 `0.14f`, 0.1~0.5 밖은 기본값 | kVp 와 같은 방식의 **단일 설정**. 범위는 전처리 모듈이 `pixelPitch_mm` 에 대해 보고하는 값(`preprocess.cpp:25`)이다 |
| `gui/ImageProcTest/Services/Native/GuiPreprocessRunner.cs` | `pixelPitchMm: 0.14f` 하드코딩 | 인자로 받음 | 하드코딩 제거 |
| `gui/ImageProcTest/Services/RealXpeBackend.cs` | — | `settings.PixelPitchMm` 전달 | 설정에서 온다 |
| `gui/ImageProcTest/Views/AnalysisPanel.xaml` | — | `PixelPitchInput` 입력칸 | 화면에 보인다 |
| `clients/ImageProcTest/DetectorDefaults.cs` | — | `PixelPitchMm = 0.14f` 신규 | clients 쪽 단일 출처 |
| `clients/ImageProcTest/Diagnostics/XpeEnhanceBasicReadinessProbe.cs:250` | `0.143f` | `DetectorDefaults.PixelPitchMm` | 사용자 결정값으로 통일 |
| `clients/ImageProcTest/Services/NativeEnhanceBasicPreviewService.cs:367` | `0.143f` | 같음 | 같음 |
| `clients/ImageProcTest/Services/NativePreprocessPreviewService.cs:1016` | `0.143f` | 같음 | 같음 |
| `clients/ImageProcTest/Services/NativePresentationExportService.cs:495` | `0.143f` | 같음 | 같음 |

- **바꾸지 않은 곳(이미 0.14)**: `clients/ImageProcTest.IntegrationTests/Functional/DataSizeContractTests.cs:219`, `…/P1AReady/PreprocessCorrectionChainSmokeTests.cs:295`. 값이 같아 손대지 않았다.
- `gui/`·`clients/` 에서 `0.148`·`0.139` 은 0건이었다(검색: `0\.148|0\.139|pixelPitch`).
- **다른 레인 소유 파일(보고만)**: `modules/`·`docs/` 에 남아 있는 값들이다. 고치지 않았다.

| 값 | 위치 | 성격 |
|---|---|---|
| 0.148 | `docs/post-processing/xpe/XPE-ALG-001…md:2038, :5677` 기본값, `.moai/specs/SPEC-XPE-P1B-DICOM/acceptance.md:40`, `modules/dicom/tests/*`, `modules/enhance_basic/tests/test_datasize_guard.cpp:76` | 문서 기본값·시험 픽스처 |
| 0.139 | `modules/gsvg/include/xpe/gsvg/gsvg_api.h:84`(예시), `modules/gsvg/tests/test_virtual_grid.cpp`, `modules/enhance_*/tests/*` | 헤더 예시·시험 픽스처 |
| 0.143 | `modules/preprocess/tests/test_xpe_preprocess_correction.cpp:71` | 시험 픽스처 |

- **clients 와 gui 는 같은 상수를 참조하지 않는다.** 두 프로젝트가 서로를 참조하지 않아서다. 값이 갈라지지 않게 `DetectorDefaults` 주석에 서로를 적었다. 한 곳으로 합치려면 공용 프로젝트가 필요하다 — 이번 범위 밖이다.
- GSVG 의 `vg_pixel_pitch_mm` 은 구현 시 이 설정을 넘긴다(3절).

## 2. kVp 가 결과를 바꾸는지

**측정** (Native, 교정 세트 있음, `kvp-native2.txt`)

| kVp | 연쇄 상태 | 그린 픽셀 해시 |
|---|---|---|
| 70 | `preprocess=Applied` | `9c0c1919e36ff0dc` |
| 120 | `preprocess=Applied` | `9c0c1919e36ff0dc` (같음) |

- **값이 실제로 반영됐다는 증거**를 같은 시험에서 단언한다. 120 을 입력한 직후 오래됨 표시가 뜬다(`ChainInputsDiffer` 에 `ExposureKvp` 가 있으므로). 관측: `stale='STALE — display parameters changed…' text='120'`.
  - 이것이 없으면 두 번 다 70 으로 돌린 것과 구별되지 않는다.

**코드 확인** (`origin/main` `c72f0e8`)
- `modules/preprocess/src` 전체에서 `kvp` 는 **한 곳**에만 나온다: `preprocess.cpp:22` 의 파라미터 범위 표. 이 표는 `xpe_preprocess_get_param_range` 류의 조회(`:91`)에만 쓰인다.
- `offset_correct.cpp:150`, `gain_correct.cpp:239`, `defect_correct.cpp:122` 는 `metadata` 를 **널 검사만** 한다.
- 화소 간격도 같다(`pixelPitch_mm` 은 같은 범위 표에만 있다).

**따라서 kVp 입력이 지금 하는 일**

| 하는 일 | 근거 |
|---|---|
| `XpeImageMetadata.kVp` 에 담겨 세 스테이지 호출에 전달된다 | `GuiPreprocessRunner.cs:100-102` |
| 세 스테이지는 그 값을 읽지 않는다 | 위 모듈 확인 |
| 오래됨 판정에 들어간다 | `ChainInputsDiffer` |
| 보고서(연쇄 기록)에 남는다 | `DescribeChain` |
| 앞으로 GSVG 의 `vg_kvp` 가 된다 | 카드 3절, 구현은 다음 카드 |

- **#182 원칙 적용 여부는 리더 판정이 필요하다.** 지금 화면 결과를 바꾸지 않는다는 점에서는 "미적용" 대상이고, 곧 GSVG 가 읽는다는 점에서는 연결 예정이다. 화소 간격도 같은 상태다.
- 연결 조사 시험은 두 설정을 **연결됨**으로 센다. 실제로 처리 경로(`RunChain` → `RunPreprocessStage` → 네이티브 메타데이터)에서 읽히기 때문이다. 시험은 "네이티브가 그 값을 쓰는지"까지는 보지 못한다 — 한계로 적는다.

## 3. GSVG 단계 준비 (판독, 구현 없음)

### 3.1 `xpe_gsvg_process_ex` 신호 → `StageStatus` 대응

`XpeGsvgResult`(`gsvg_api.h:275-282`)는 `vignetteApplied` / `gridSuppressed` / `virtualGridApplied` / `restoredOriginal` / `reason` 을 준다.

| 이유 코드 | 뜻 | GUI 단계 실행기의 반환 | 결과 `StageStatus` | 근거 |
|---|---|---|---|---|
| 0 `APPLIED` | 설정된 단계가 영상을 바꿨다 | `Ran=true`, 출력 픽셀 | **Applied** (실행기가 입력과 비교) | 정상 적용 |
| 1 `NOT_CONFIGURED` | 억제·가상격자 어느 것도 켜져 있지 않다 | `Ran=false`, 사유 "설정이 비어 있다" | **RequestedNotApplied** | GUI 가 단계를 요청했는데 설정이 비었다면 GUI 쪽 구성 오류다. 조용히 넘어가면 키 오타 함정(`gsvg_api.h:61-66`)이 그대로 숨는다 |
| 2 `IMAGE_TOO_SMALL` | 32 픽셀 미만 | `Ran=false` + 사유 | **RequestedNotApplied** | 요청은 됐고 아무 일도 없었다. 사용자가 이유를 알아야 한다 |
| 3 `NO_GRID_DETECTED` | 격자가 없다 | `Ran=true`, 입력과 같은 픽셀 | **AppliedNoChange** | 단계는 **돌았고** 영상을 검사한 뒤 "바꿀 것이 없다"고 답했다. `NotRequested` 는 거짓이다(요청됐고 실행됐다). `RequestedNotApplied` 로 두면 정상 동작이 경보로 보인다 |
| 4 `GRID_NOT_IN_SUBBANDS` | 입력에 봉우리는 있는데 서브밴드가 확인하지 못해 아무것도 거르지 않았다 | `Ran=false` + 사유 | **RequestedNotApplied** | 3 과 다르다. 격자가 있을 가능성이 남아 있는데 제거하지 못한 상태다. 영상에 격자가 남아 있을 수 있다는 사실이 표시돼야 한다 |
| 5 `VG_REFUSED` | 노출·설정이 표 밖이다. `restoredOriginal=1` | `Ran=false` + 경보 큐의 문구 | **RequestedNotApplied** | 모듈이 원본으로 되돌린 상태다(REQ-GSVG-024) |

- **이 대응은 실행기 반환값만으로 얻어진다.** `ProcessingChainRunner` 가 `Ran=true` 인 경우에 입력과 출력을 비교해 Applied / AppliedNoChange 를 정하므로, 3 번은 자동으로 `AppliedNoChange` 가 된다. 별도 분기가 필요 없다.
- `restoredOriginal` 은 사유 문구에 함께 적는다(모듈이 되돌렸는지, 우리 폴백이 썼는지를 구별하기 위해).
- `vignetteApplied` 는 gain map 을 넘길 때만 의미가 있다. GUI 는 아직 gain map 이 없으므로 이 단계에서는 늘 0 이다.

### 3.2 표 파일을 어디서 찾을지 (선택지 — 설치 규칙은 리더)

제품 표는 `modules/gsvg/data/vg_table_water_csi600_victre.csv` 다. **설치 규칙은 아직 없다** — `modules/gsvg/CMakeLists.txt` 에 `install()` 이 없고, 모듈 시험은 소스 디렉터리를 작업 디렉터리로 삼아 상대 경로로 읽는다.

| 안 | 내용 | 장점 | 단점 |
|---|---|---|---|
| (a) 네이티브 DLL 옆 | `gsvg.dll` 과 같은 디렉터리(또는 그 아래 `data/`)에 두고, GUI 가 DLL 탐색 결과에서 경로를 만든다 | 이미 DLL 경로를 아는 코드가 있다(`NativeSearchPolicy`, `XpeGsvgLibraryLocator` 가 생기면 같은 자리) | CI 가 표를 산출물에 포함해야 한다(`Stage-NativeArtifacts.ps1` 갱신) |
| (b) 설정 경로 | `AppSettings.GsvgTablePath`, 기본값은 (a) 의 경로, 사용자가 바꿀 수 있음 | 교정 경로 3개와 같은 방식. 표를 갈아 끼우기 쉽다 | 잘못된 경로면 단계가 거부된다(사유는 표시된다) |
| (c) 저장소 상대 경로 | 개발 중에만 `modules/gsvg/data/…` 를 참조 | 지금 당장 돌릴 수 있다 | 배포본에는 저장소가 없다. 개발 편의용으로만 |

- 권하는 조합: **(b) + 기본값 (a)**. 교정 경로와 같은 모양이고, 설치 규칙이 정해지기 전에는 사용자가 (c) 를 직접 넣어 시험할 수 있다.
- 어느 안이든 표 파일의 **내용 해시**를 캐시 키와 보고서에 넣어야 한다(C-97 6절).

### 3.3 설정 항목과 기본값 후보

| 설정 | 모듈 키 | 필수 | 기본값 후보 | 출처 / 메모 |
|---|---|---|---|---|
| 켜기 | `virtual_grid` / `grid_suppression` | — | 꺼짐 | 헤더 기본값이 FALSE. 둘 다 켜면 init 실패 |
| 표 경로 | `vg_table_path` | 필수 | 3.2 (b) | 제품 표 파일 |
| 격자비 | `vg_grid_ratio` | 필수 | **10** | 제품 표 `[grid]` 에 6·8·10·12 가 각각 24행. REQ-GSVG-016 의 네 값과 일치 |
| 격자 주파수 | `vg_grid_frequency_per_cm` | **제품 표에서는 필수** | 40 또는 60 — **정할 근거 없음** | 표에 40 과 60 두 설계가 있다. 표에 `freq_per_cm` 열이 있으면 이 키가 필수이고, 없으면 거부된다(`gsvg_api.h:95-97`) |
| 화소 간격 | `vg_pixel_pitch_mm` | 필수 | `AppSettings.PixelPitchMm` (0.14) | 이 카드의 결정 |
| kVp | `vg_kvp` | 필수 | `AppSettings.ExposureKvp` (70) | 표의 kVp 축은 60·80·100·120 이다. **70 은 축 위의 값이 아니다** — 보간 여부/거부 여부를 구현 때 확인해야 한다 |
| 공기 신호 | `vg_air_signal` | 필수 | 60000 | 헤더 예시·벤치뿐. 검출기 근거는 없다 |
| 반복 수 | `vg_iterations` | 필수 | 3 | 헤더 예시·벤치. 범위 1..100 |
| 피라미드·잡음 | `vg_pyramid_levels` / `_gain` / `vg_denoise_k` | 선택 | 끔 | 없으면 꺼짐 |
| 필드 마스크 | 인자 | 선택 | 없음 | 콜리메이션 검출이 있어야 한다 |

- 표의 축: 격자비 6/8/10/12, 주파수 40·60 /cm, 두께 10·20·30 cm, kVp 60·80·100·120.
- 격자비 목록은 표의 `[grid]` 에서 읽는다(C-99 판정). 주파수도 같은 방식으로 읽어야 한다.

## 4. 증거

**빌드** (`build-final.txt`): `GUI=0`, `SC=0`, `CLIENTS=0`, 경고 0

| 실행 | 결과 | 파일 |
|---|---|---|
| 통합 | 실패 0 / 통과 235 / 건너뜀 1 | `full-int.txt` |
| Mock E2E 전체 | 실패 0 / 통과 104 / 건너뜀 3 (C-02, IB-02, NativeProvenance) | `full-mock.txt` |
| Native E2E 전체 | **실패 0 / 통과 107 / 건너뜀 0** | `full-native.txt`, `full-native.trx` |

**반증**
- `RealXpeBackend` 가 설정 대신 `0.14f` 리터럴을 넘기게 하고 재빌드(`BUILD_EXIT=0`) → 연결 조사 시험 실패 2건: `PixelPitchMm: bound in … not read by Real processing, and not declared unconnected` (`f-hardcoded.txt`). 복원 뒤 재빌드 `BUILD_EXIT=0`.
- C-02 는 kVp 입력이 뷰모델에 닿았다는 증거(오래됨 표시, 입력값 `120`)를 함께 단언한다. 이 단언이 없으면 "두 번 다 70" 과 구별되지 않는다.

## 5. 미검증 / 잔여 위험

- **kVp·화소 간격을 네이티브가 쓰는지**: 모듈 소스 검색과 픽셀 해시로만 판단했다. DLL 바이너리를 역으로 확인하지는 않았다.
- **`.xcal` 교정 파일에 화소 간격이 들어 있는지**: 확인하지 않았다(C-99 에서 남긴 항목 그대로).
- **clients 쪽 상수와 gui 설정의 동기화**: 코드로 강제하지 못한다. 한쪽만 바뀌어도 시험이 잡지 않는다.
- **GSVG 판독의 한계**: 표의 kVp 축에 70 이 없다. 기본 kVp 70 으로 가상 격자를 켜면 거부될 수 있다. 구현 카드에서 먼저 확인해야 한다.
- **격자 주파수 기본값**: 제품 표가 두 설계를 담고 있는데 어느 것이 장비의 격자인지 알 근거가 없다.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| 화소 간격 140 µm 통일 + 단일 설정 | GUI-C-100 |
| 다른 레인 소유 파일 보고 | GUI-C-100 (3개 값, 위치 표) |
| kVp 효과 측정 + 코드 확인 + 용도 | GUI-C-100 → #182 적용 여부는 리더 판정 |
| GSVG 신호 대응표 / 표 경로 선택지 / 기본값 | GUI-C-100 (판독) |
| GSVG 단계 구현 | 다음 카드 |
| clients·gui 상수 동기화 | 새 카드 후보 |
