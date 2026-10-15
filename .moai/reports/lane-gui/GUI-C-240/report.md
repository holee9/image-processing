[2026-10-14 16:06 KST] [xpe-gui] GUI-C-240 보고 — 빠른 시작에 "DLL 받기"·"보정 폴더 만들기" 단계 (U1·U9, Refs #251)

푸시하지 않았다. 기준 `6cdaf13b`. 변경: 문서 2개(`gui/ImageProcTest/help/quick-start.html`, `quick-start.ko.html`)와 시험 1개(`clients/ImageProcTest.IntegrationTests/Diagnostics/QuickStartCitationTests.cs`). 제품 코드 변경 없음. 가용 메모리 5845 MB 로 시작해 5976 MB 로 끝났고 남은 앱/testhost 0 (`memory_before.txt`, `memory_after.txt`).

**먼저 알릴 것 (의존)**: 새 인용 시험은 통합 브랜치(#266)의 세 파일이 있어야 통과한다 — `tools/Get-CiNativeDlls.ps1`, `--caldata` 를 해석하는 `modules/preprocess/tools/xpe_calib_fixture_gen.cpp`, 릴리스 `testdata-calib-v1` 단계가 있는 `.github/workflows/ci.yml`. `dev/gui` 단독으로는 그 파일들이 없어 이 시험이 빨갛다. 로컬에서는 `integ/m3-final` 의 세 파일을 작업 트리에 임시로 덮어 돌리고 끝나면 되돌렸다(커밋하지 않았다). #266 에 함께 올라가면 초록이다.

## 1. 문서 (영·한)

- **준비물 1(DLL 폴더)**: `.\tools\Get-CiNativeDlls.ps1` 실행 → `build\e2e-native-dlls` 생성. 필요한 것: GitHub CLI(gh) 로그인(`gh auth login`). 걸리는 것: 약 300 MB 다운로드(리더가 카드에 적은 값; 내가 내려받아 재지는 않았다). 함께 오는 것: 종속 DLL, 보정 세트 생성기 `xpe_calib_fixture_gen.exe`, 출처 기록 `provenance.json`(그래서 시작 스크립트가 `MATCH`). **시작 스크립트는 `-NativeDir` 를 생략하면 그 폴더를 쓴다** — 시작 스크립트의 `.PARAMETER NativeDir` 설명("Default: <repo>\build\e2e-native-dlls if it exists")을 읽고 그대로 적었다.
- **준비물 2(보정 폴더)**: 없으면 공개 릴리스 `testdata-calib-v1` 에서: `gh release download testdata-calib-v1 -R holee9/image-processing -D D:\xpe-data\CalData_6`(릴리스 자산 합계 약 123 MB: BPMap.map, bright04~06, dark, 기준 영상 f32 37.7 MB, `SHA256SUMS.txt`; `gh release view` 로 읽음), 이어서 `.\build\e2e-native-dlls\xpe_calib_fixture_gen.exe --out D:\xpe-data\calib-real --caldata D:\xpe-data\CalData_6 --expiry-ms 4102444800000`(약 2초: GUI-C-239 에서 1.9초 실측). `--expiry-ms` 가 하는 일(만료일 2100-01-01, 빼면 파일마다 `XPE_WARN_NO_EXPIRY`)과 1단계 기준 영상이 같은 릴리스에 있다는 것(5번 비교용)도 적었다.
- **1단계 명령**: 예시를 `-CalibDir D:\xpe-data\calib-real -RawWidth 3072 -RawHeight 3072`(`-NativeDir` 없음)로 바꾸고 "생략하면 `build\e2e-native-dlls`, 다른 폴더는 `-NativeDir D:\xpe\dlls`" 한 줄. 손으로 치는 명령의 보정 경로도 `calib-real` 로 맞췄다.
- **1단계 "그렇지 않고"와 문제 해결 표(Mock 행)**: 배너가 `…could not be used` 다음에 콜론과 이유를 보인다고 적고, 이유 세 가지(`… was not found at '…'`, `… could not be loaded`, `… does not export …` + `(an older build than this application expects)`)와 238/238b 이후의 동작을 한 행에: 앱 폴더에 남은 낡은 `xpe_common.dll`/`xpe_display.dll` 은 직접 시작할 때 흔한 원인(고정이 아니면 앱 폴더를 먼저 찾음), 시작 스크립트(고정)를 쓰면 DLL 폴더만 찾고 그 폴더의 부재·손상 DLL 을 배너가 말함, `EXCLUSIVE=1` 인데 `XPE_NATIVE_DIR` 가 없으면 `XPE_NATIVE_DIR does not name it`.

## 2. 대조 (카드 4)

- **`EveryStringTheQuickStartQuotes_IsAStringOfTheApp`**: 새 문장으로 깨지지 않았다. 인용 106 → **116 건**이 모두 앱 소스에서 확인됨(exact 75, words 41; `citation_run1.txt`). 새 건너뜀 범주는 이유를 적었다(스크립트 `MATCH` 출력, 릴리스 이름 `testdata-calib-v1`; 경로·`gh …`·`build\…`·`-NativeDir`·`D:\…`·`.json/.txt/.exe` 이름은 기존 "명령·파일 이름" 범주 규칙 확장).
- **새 시험 `TheScriptsAndCommandsTheQuickStartGives_NameRealFilesParametersAndOptions`**: ① 페이지가 `.\tools\<x>.ps1` 로 부르는 스크립트가 `tools/` 에 있고, 페이지가 주는 `-매개변수`(5개: CalibDir, RawWidth, RawHeight, NativeDir 등)가 그 스크립트의 매개변수일 것. ② 두 스크립트가 모두 `build\e2e-native-dlls` 를 기본값으로 가질 것(문서가 "생략하면 그 폴더"라고 말하므로). ③ 생성기에 주는 `--option` 이 생성기 소스가 해석하는 것일 것. ④ `gh release download` 의 저장소가 `Get-CiNativeDlls.ps1` 의 기본 저장소이고, 릴리스 이름과 인용한 자산 이름(`SHA256SUMS.txt`, 기준 영상)이 그 릴리스를 내려받는 CI 단계(ci.yml)에 있을 것. ⑤ 페이지가 인용하는 배너 이유 문구가 이유를 **쓰는 코드**(`RealXpeBackend.WhyNativeUnusable`, `XpeBackendFactory`)에 있을 것.
- **반증 5팔 모두 빨강**(`falsify.txt`): 없는 매개변수, 생성기가 모르는 옵션, 없는 스크립트, 앱이 쓰지 않는 이유 문장, CI 가 쓰지 않는 자산 이름. 팔 D 는 처음엔 **빨강이 아니었다**: `cannot be loaded` 가 앱 소스의 다른 곳에 있어 말뭉치 검색이 찾았다. 그래서 ⑤ 를 추가해(이유를 쓰는 두 파일로 범위를 좁힘) 잡았다.
- **W0·W14**(실데이터, 편집한 페이지가 패키지된 `help/` 로 복사된 뒤): 통과(`w14_w0_after_page_edit.txt`). W14 는 1~4단계 명령·요약 줄·체인 줄을 페이지에서 읽는데 새 문장은 이를 건드리지 않는다.

## 한계
- 새 인용 시험이 확인하지 **않는** 것: 릴리스에 그 자산이 지금 정말 있는지, 생성기가 내려받은 DLL 세트에 들어 있는지(네트워크·CI 실행이 필요; 후자는 리더의 CI 단계가 쓴다). 릴리스 자산 목록은 `gh release view` 로 한 번 읽었다.
- "약 300 MB" 는 리더가 적은 값 그대로다. `Get-CiNativeDlls.ps1` 은 내가 실행하지 않았다(리더 소유 스크립트, gh 로그인·다운로드 필요).
- 문서의 `gh release download … -D D:\xpe-data\CalData_6` 와 생성기 명령을 내가 이 순서로 실행해 보지는 않았다(GUI-C-239 에서는 리더 체크아웃의 CalData_6 원본과 pre 빌드 생성기로 같은 생성기 명령을 실행해 1.9초에 세트를 만들었다).
- 한국어 문장은 인용 대조(앱 문자열)는 받지만 문체는 검수하지 않았다.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| 1 DLL 받기(영·한), 기본 폴더, gh 로그인·300 MB | GUI-C-240 |
| 2 보정 폴더 만들기(릴리스 + 생성기 명령) | GUI-C-240 |
| 3 앱 폴더의 낡은 DLL → 이유 있는 Mock, 문제 해결 | GUI-C-240 |
| 4 인용 시험·W14 와 대조(스크립트·옵션·자산) | GUI-C-240 |
