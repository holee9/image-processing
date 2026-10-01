# GUI-C-188 — #225 남은 행: File → Open DICOM 조사 (코드 변경 없음)

레인: gui · 이슈: `#225` · 코드 변경 없음(조사 카드), 푸시 없음. 5절 형식(주장 / 증거 / 기준 귀속 / 미검증 / 잔여 위험). 기준 트리: `dev/gui` `9a9be97b`.

## 1. 주장 (결론)

**지금 바로 켤 수 없다. (a) 는 아니다.** 막는 것은 둘이고 성격이 다르다.

1. **(c) 납품·결정 선행 — 가장 큰 막힘.** `xpe_dicom.dll` 은 GUI 가 쓰는 네이티브 스테이징에 들어오지 않는다. CI 의 Native E2E 잡은 `xpe-ci-{common,preprocess,post}-binaries` 세 산출물만 받고, DICOM 을 짓는 잡(`coverage-dicom`)은 DLL 이 아니라 커버리지만 올린다. DCMTK·OpenJPEG 는 vcpkg 로만 얻는다(FetchContent 대안 없음). **DICOM DLL 과 의존 DLL 을 GUI 와 함께 내보낼지, CI 가 어느 잡에서 짓고 올릴지는 리더/배포 결정이다.**
2. **(b) 모듈 쪽 선행 — 작지만 임상 판단이 걸림.** 읽기 API 는 **있다**(`xpe_dicom_open` / `read_image` / `get_metadata` / `close`). 그러나 읽기가 **부호(PixelRepresentation)·리스케일(RescaleSlope/Intercept)·광도 해석(MONOCHROME1/2)·프레임 수·샘플 수를 읽지 않는다**(검색 범위 `modules/dicom/src`, 쓰기만 상수로 적음). 이 값들이 다른 파일은 GUI 가 조용히 틀린 화소를 받는다. post 가 정할 것: 읽기가 이것들을 반영/거부할지, 메타데이터 구조체에 노출할지.

GUI 쪽 변경 자체는 작다(§2-3). 판정은 "모듈이 뭘 보장하는지 + DLL 이 오는지"에 달렸다.

## 2. 증거

### 2-0. 남은 비활성 메뉴 세 축 (리더가 이름 검색 한 축으로 센 4개를 다시 셈)

| 축 | 방법 | 결과 |
|---|---|---|
| 앱의 이름 목록 | `MainWindow.xaml.cs` 의 `DisabledFutureCommandCount` 배열(361행 `OpenDicomMenuItem` 로 시작) | 4 (A11 보고서 값 `DisabledFutureCommandCount: 4`) |
| 메뉴 트리 순회 | 앱이 낸 A11 보고서의 `UnimplementedMenuLeafCount` (잎 항목 중 비활성·`Command` 없음) | 4 |
| XAML 파싱 | `MainWindow.xaml` 의 `<MenuItem …IsEnabled="False">` 를 정규식으로 | 4: `OpenDicomMenuItem`, `RunDeterministicBaselineMenuItem`, `QaConstancyMenuItem`, `GsdfCalibrateMenuItem` (모두 `Command` 없음) |

세 축이 같다(4). 리더가 센 4개와 이름도 같다. 이 수는 A11 시험(통과)이 이미 두 앱 축의 일치를 단언한다.

### 2-1. 재료 — 읽기 API 는 있다

`modules/dicom/include/xpe/dicom/dicom_api.h`:

| 함수 | 줄 | 서명 | 반환 코드 |
|---|---|---|---|
| `xpe_dicom_open` | 70 | `(const char* filePath, XpeDicomHandle** outHandle)` | `XPE_OK`; `INVALID_INPUT`(NULL); `IO_FAILED`(없음/못 읽음); `DICOM_INVALID`; `UNSUPPORTED_FORMAT`(전송 구문). 메타 헤더 없는 파일도 열림(61~67행) |
| `xpe_dicom_read_image` | 107 | `(XpeDicomHandle*, XpeImageBuffer* outImg)` | `OK`; `INVALID_INPUT`; `OUT_OF_MEMORY`; `DICOM_INVALID`(픽셀 없음·Rows/Columns 없음·**짧은 PixelData**·압축 프레임 크기 불일치, 83~99행); `PROCESSING_FAILED`(해독 실패). 버퍼는 `xpe_alloc_image` 로 할당, 해제는 호출자의 `xpe_free_image`, 형식은 `XPE_PIXEL_UINT16` |
| `xpe_dicom_get_metadata` | 125 | `(XpeDicomHandle*, XpeImageMetadata* outMeta)` | `OK`; `INVALID_INPUT`; `DICOM_INVALID`. 빠진 태그는 조용히 기본값(빈 문자열/0), **"없음"과 "빈 값"을 구별 못 함**(120~122행) |
| `xpe_dicom_close` | 135 | `(XpeDicomHandle*)` | 없음. NULL 허용 |

`XpeImageBuffer`(`modules/common/include/xpe/common/xpe_types.h:83`): `width, height, bitsAllocated, bitsStored, format, data, dataSize`. `XpeImageMetadata`(108행): `bodyPart, kVp, mAs, SID_mm, pixelPitch_mm, acquisitionTime, flags` — **부호·리스케일·광도 해석 필드가 없다.**

**스레드 계약: 읽기 함수에는 적혀 있지 않다.** 헤더에서 스레드 안전을 말하는 곳은 `xpe_dicom_cancel`(313행 "Thread-safe. May be called from any thread")뿐이다(검색: `dicom_api.h`·`modules/dicom/src/dicom.cpp` 의 `thread|reentran|concurren`). 그래서 GUI 는 읽기 호출을 한 스레드에서 직렬로 불러야 안전하다고 **가정**해야 한다.

### 2-2. 읽기가 보지 않는 것 (GUI 가 틀린 화소를 받을 수 있는 자리)

`modules/dicom/src/DicomReader.cpp`: 크기·비트(`271~277`), 화소는 `findAndGetUint16Array(DCM_PixelData …)`(353행)로 얻어 `std::memcpy(outImg->data, pixData, expectedBytes)`(379행)로 **16비트 단어 그대로** 복사한다. `bitsStored` 는 기록만 하고(347~348행) 마스킹·부호 확장을 하지 않는다. 검색 `PixelRepresentation|RescaleSlope|RescaleIntercept|PhotometricInterpretation|MONOCHROME1|SamplesPerPixel|NumberOfFrames` 를 `modules/dicom/src` 전체에서 돌리면 **쓰기(`DicomWriter.cpp:146~162`, 상수 `MONOCHROME2`·부호 없음·`0`·`1`)에서만 나오고 읽기에서는 0건**이다. 대조: 같은 검색이 `BitsStored` 는 `DicomReader.cpp` 에서 찾는다.

읽은 코드로부터의 **추론**(실행하지 않음): 부호 있는 화소(`PixelRepresentation=1`)는 부호 없는 값으로 재해석되고, `MONOCHROME1` 은 반전된 채 오며, 리스케일은 무시되고, 여러 프레임·RGB 는 앞 `Rows×Columns×2` 바이트만 복사된다.

### 2-3. 납품 — DLL 이 GUI 에 오는가: 아니오

- **스테이징**: `.github/workflows/ci.yml:772~775` 의 단계가 받는 것은 `xpe-ci-{common,preprocess,post}-binaries`. 그 뒤 확인 단계는 `xpe_common`, `xpe_preprocess`, `xpe_display`, `gsvg`, `xpe_ai`, `xpe_ai_worker.exe` 를 요구한다(`xpe_dicom` 없음).
- **DICOM 을 짓는 곳**: `BUILD_DICOM` 은 루트 CMake 에서 기본 OFF(`CMakeLists.txt:25`). 켜는 프리셋은 `coverage-dicom`(vcpkg DCMTK+OpenJPEG, `ci.yml:1024~1026`)과 `ci-fullstack`(`CMakePresets.json:187~200`). 그런데 **`coverage-dicom` 잡이 올리는 것은 `build/<preset>/coverage/` 뿐**(`ci.yml:1121~1129`)이고, `ci-fullstack` 은 검색 범위 `.github`·`tools` 에서 **사용처가 0건**이다(대조: 같은 검색이 `coverage-dicom` 은 8건 찾음).
- **의존 DLL**: `modules/dicom/CMakeLists.txt` 는 DCMTK(`DCMTK::DCMTK`)와 OpenJPEG(`find_package(OpenJPEG CONFIG REQUIRED)`, 35행)를 링크하고, `third_party/dicom/vcpkg.json` 이 `dcmtk>=3.7.0`·`openjpeg>=2.5.4`·`gtest` 를 선언한다. CI 주석(`ci.yml:1024~1025`)은 "DCMTK + OpenJPEG 는 FetchContent 대안이 없다", DCMTK 빌드는 18~20 분(`:1070~1071`)이라고 적는다. 실행 때 vcpkg 의 DLL 이 옆에 필요한지는 **확인하지 않았다**(CI 주석 `:1094` 은 "vcpkg applocal 이 보통 복사한다"고만 적음).
- **GUI 해석기**: `GuiNativeLibraryResolver.cs:24~28` 의 상수는 `xpe_common`·`xpe_display`·`xpe_preprocess`·`gsvg`·`xpe_ai` 다섯. `xpe_dicom` 은 없다(검색 범위: 그 파일). 오래된 `clients/ImageProcTest/PInvokeWrappers/XpeDicomWrapper.cs` 에 델리게이트 서명이 이미 있어 선언은 참고할 수 있으나 `gui` 앱의 코드는 아니다.

### 2-4. GUI 쪽 맞물림

- **열기 경로**: 대화상자 필터가 `Raw Files (*.raw)|*.raw|All Files (*.*)|*.*`(`MainWindowViewModel.cs:2095`), 확장자 분기는 `RawImageLoader.Load`(`.raw` 만 읽고 나머지는 자리표시 영상, `Services/RawImageLoader.cs:12~19`)에 있다. `.dcm` 은 지금 "Unsupported extension" 자리표시로 간다(`LoadUnsupported`).
- **크기 가정**: Raw 는 설정의 `RawWidth×RawHeight`(기본 3072², `UInt16LE`)로 읽고, 파일이 작으면 `InvalidDataException`(C-187 에서 관측한 메시지). **DICOM 은 크기가 파일 안에 있다**(`Rows`/`Columns` → `XpeImageBuffer.width/height`). 다행히 하류는 설정이 아니라 `LoadedImageFrame.Width/Height/BitsStored/RawPixels`(`LoadedImageFrame.cs:22~28`)를 쓴다(예: `RealXpeBackend` 가 `rawFrame.Width`). `RawWidth/RawHeight` 를 `RawImageLoader`·`AppSettings` 밖에서 읽는 곳은 자동화 인자·`MainWindow.xaml.cs:90~97`·`GuiFixtureManifestService.cs` 뿐이다(검색 범위 `gui/ImageProcTest` `*.cs`·`*.xaml`) — 즉 DICOM 프레임은 **설정을 건드리지 않고** 프레임 값만 채우면 된다.
- **형식**: `XPE_PIXEL_UINT16`, 부호 없음이 `RawPixels`(`ushort[]`)와 맞는다. `BitsStored` 도 실을 수 있다. 맞지 않는 곳은 §2-2 의 부호·광도·리스케일·다중 프레임이다.
- **메타데이터 표시**: `MetadataText` 는 문자열이라 `bodyPart·kVp·mAs·SID·pixelPitch` 를 적을 수 있다. 단 "없음"과 "빈 값"을 구별 못 하므로 0 을 "측정값 0"으로 적으면 안 된다(§2-1).
- **Mock 백엔드**: `MockXpeBackend.cs:77~78` 이 `NO_REAL_DICOM_IN_GUI_S0` 를 돌려준다(DICOM 은 Mock 에서 영영 불가). 메뉴를 켠다면 Native 전용 가드가 필요하다(AI 가 `IAiSessionBackend` 로 한 것과 같은 모양).

**켤 때의 GUI 변경 목록(DLL 이 오는 경우)** — 이 카드는 구현하지 않는다:
1. `GuiNativeLibraryResolver` 에 `xpe_dicom.dll` 상수·후보 경로 한 줄(+ 의존 DLL 은 같은 폴더 해석에 의존).
2. `Services/Native/` 에 P/Invoke 한 파일(`open`/`read_image`/`get_metadata`/`close`)과 읽기 한 번 = `open → read_image → get_metadata → close` 를 `finally` 로 닫는 러너(버퍼 해제 `xpe_free_image`).
3. `IXpeBackend` 에 DICOM 읽기(Native 만; Mock 은 이유와 함께 거절).
4. `LoadedImageFrame` 채우기: `RawPixels/Width/Height/BitsStored` + 메타 문자열. 8비트 미리보기는 `RawImageLoader` 와 같은 최소/최대 정규화를 재사용.
5. `OpenDicomMenuItem` 활성화 + `Command` 연결 + 대화상자 필터 `*.dcm`, 헤더의 "(Phase 1b)" 문구 갱신.
6. 자동화 보고서의 비활성 이름 목록(`MainWindow.xaml.cs:361`)에서 `OpenDicomMenuItem` 제거(A11 의 세 축이 4→3).
7. CI: Native 잡이 `xpe_dicom.dll` 을 스테이징에 받도록(§2-3) — **이것이 (c) 의 결정에 걸린 부분**.

### 2-5. 시험 자료

- **저장소 안의 DICOM 파일: 없다.** 검색 범위: `git ls-files` 전체(추적 파일)에서 확장자 `.dcm`·`.dicom`(0건), 경로에 `dicom` 이 들어간 파일은 목록 앞 30건만 읽었고(보고서·SPEC·문서·모듈 소스·시험 소스·구 클라이언트의 `XpeDicom*` 래퍼) 그 안에 영상 파일은 없었다. 뒤쪽은 읽지 않았다. 대조: 같은 목록이 `modules/dicom/src/*.cpp` 는 찾는다.
- 모듈 시험은 파일이 아니라 **실행 중에 만든다**: `modules/dicom/tests/test_dicom_reader.cpp` 머리말 "Synthetic DICOM files are created in SetUpTestSuite using DicomWriter to avoid dependency on external test assets"(7~8행).
- GUI 가 시험 파일을 얻는 길은 둘: (i) 번들 합성 Raw(`gui/ImageProcTest/fixtures/gui-s0/raw/synthetic_1024x1024.raw`)를 `xpe_dicom_write` 로 감싸 임시 `.dcm` 을 만든다 — **DLL 이 있어야 한다**(§2-3); (ii) 작은 `.dcm` 를 저장소에 커밋한다 — **합성임을 보장하고 환자 정보가 없음을 확인해야 한다**(결정 사항). 부호 있음·`MONOCHROME1`·리스케일 같은 입력은 (i)의 쓰기가 상수만 쓰므로 만들 수 없다(`DicomWriter.cpp:146~162`) — DCMTK 로 직접 만들어야 한다.

## 3. 기준 귀속

모든 인용은 이 트리(`dev/gui` `9a9be97b`)를 이 세션에서 읽어 얻은 것이다. 메뉴 세 축 중 둘째는 이 세션의 가장 최근 A11 실행(Mock)에서 앱이 낸 보고서 값이다. 실행한 코드는 없다.

## 4. 미검증

1. **읽기 동작은 읽기만 했다.** §2-2 의 부호·광도·리스케일·다중 프레임 영향은 코드를 읽은 추론이다. 실제 DICOM 을 읽어 보지 않았다(DLL 도 입력도 없음).
2. **vcpkg 의 DCMTK·OpenJPEG DLL 이 실행 때 `xpe_dicom.dll` 옆에 필요한지, 정적으로 들어가는지** 확인하지 않았다(§2-3).
3. **`xpe_dicom_*` 읽기 함수의 스레드 안전성**은 헤더가 말하지 않아 모른다. DCMTK 전역 초기화(코덱 등록, `DicomReader.cpp:46` `ensure_jpeg_codecs_registered`)가 동시 호출에서 안전한지 보지 않았다.
4. `ci-fullstack` 프리셋이 **어디서도 쓰이지 않는다**는 것은 `.github`·`tools` 범위 검색이다. 로컬 스크립트·문서·메인 저장소의 다른 곳에서 부르는지는 보지 않았다.
5. 전송 구문 지원 목록(어떤 압축 파일이 `UNSUPPORTED_FORMAT` 인지)은 읽지 않았다.
6. 메뉴 세 축의 둘째(트리 순회)는 이 카드에서 새로 돌리지 않고 최근 A11 값을 인용했다.

## 5. 잔여 위험

- **침묵의 오독이 가장 위험하다.** 부호 있음·`MONOCHROME1` 파일이 오류 없이 반전/재해석된 영상으로 보이면, 이 앱의 측정(EI·결함 지표 등)이 그 영상으로 계산될 수 있다. 읽기가 이를 거부하거나 값을 노출하기 전에는 메뉴를 켜지 않는 쪽이 안전하다.
- DICOM 은 환자 정보를 담는다. `MetadataText` 와 로그에 어떤 태그를 적을지 정하지 않으면 GUI 가 PHI 를 화면·로그 파일에 남길 수 있다(결정 필요; 현재 읽기 API 는 `bodyPart` 등만 돌려주므로 API 가 그 범위를 제한한다).
- 남은 다른 3개 메뉴(`RunDeterministicBaselineMenuItem`, `QaConstancyMenuItem`, `GsdfCalibrateMenuItem`)는 이 카드의 범위 밖이다. 툴팁이 말하는 선행 조건만 확인했다: GSDF 는 "검증된 PS3.14 휘도 보정 워크플로가 있을 때"(`MainWindow.xaml:447`), 결정론 기준선은 "preprocess, enhance_basic, display, dicom 모듈 필요"(`:363`).

## 결론별 요청 (리더가 배정할 때 쓰도록)

| | 누구에게 무엇을 |
|---|---|
| (c) | 리더/배포: ① DICOM DLL 과 의존 DLL 을 GUI 와 함께 낼지 ② 어느 CI 잡이 vcpkg 로 짓고 `xpe-ci-dicom-binaries` 처럼 올릴지 ③ 시험용 `.dcm` 을 저장소에 둘지(합성·PHI 없음 확인) |
| (b) | post: 읽기에서 `PixelRepresentation`·`RescaleSlope/Intercept`·`PhotometricInterpretation`·`NumberOfFrames`·`SamplesPerPixel` 를 반영하거나 거부할지, 메타데이터에 노출할지; 읽기 함수의 스레드 계약을 헤더에 적을 것; 부호/반전 입력 시험 파일 생성 |
| (a) | 위 둘이 풀린 뒤에만: §2-4 의 GUI 변경 7항 |

🗿 MoAI
