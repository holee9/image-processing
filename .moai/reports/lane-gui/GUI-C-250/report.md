[2026-10-17 16:30 KST] GUI-C-250 보고서 — 문서대로 2판 결함 맵 보정 세트를 만들고 앱에서 판 1·판 2 로 보정 (실테스트, Refs #273)

기준: dev/gui 336d9282 위의 새 커밋. 푸시하지 않는다. 방법과 예측은 실행 전에 064f2dda 로 커밋했다. 실행 범위는 카드대로 세트 생성 1회, 앱 처리 판 2·판 1 각 1회이며, 전체 스위트는 돌리지 않았다.

## 1. 결론

문서를 따라 cyan_test 원시 프레임으로 2판 결함 맵 세트를 만들고, 앱에서 같은 영상 한 장을 판 2 세트와 판 1 세트(`calib-cyan-v3`)로 보정했다. **둘 다 끝까지 된다. 그러나 문서만 보고는 되지 않는 곳이 여럿 있다(§5, 25항목).** 사용자가 막히는 곳은 크게 셋이다. (1) 판 2 를 켜는 옵션 `--bpm-v2`·`--bpm-margins` 가 문서 어디에도 없다. (2) 병합 전 도구는 문서가 알려 준 방법(`Get-CiNativeDlls.ps1`)으로 받을 수 없다. (3) 원시 프레임이 저장소에 없고 어디서 받는지 문서가 말하지 않는다.

| 항목 | 결과 |
|---|---|
| 세트 생성 | 종료 코드 0, 2분 31초. `manifest.json` 에 `xcal_version 2`, 여백 `[6,2,1,1]`, 영역 밖 화소 30,699, 줄 그룹 0, 덩어리 2,201, 결함 화소 2,209 |
| 앱 처리 판 2 | `preprocess=Applied`, 보정 3단계, 알림 2건(INFO 1, 경고 1 = 노출 편차) |
| 앱 처리 판 1 | 같은 결과 |
| 화면 캡처 | **얻지 못했다**(창 안쪽이 균일한 흰색). 앱이 저장한 16비트 PNG 로 대신한다(§4) |
| 판 1 대 판 2 차이 | 두 세트는 offset·gain 파일 자체가 달라 **거의 모든 화소**가 다르다. 결함 단계가 더한 변화는 합집합 2,320 화소 안 2,307곳뿐이다(§3) |

## 2. 만든 세트와 앱 처리

### 2.1 도구와 DLL

- dev/preprocess `c19e98a0dfd2b538292b8d79211f16406d6c8aa3` 의 CI 실행 **38992249313**. 실행 결론은 cancelled(clang-tidy 취소), 빌드·시험 잡은 success.
- 아티팩트(GitHub API digest): preprocess 12086328322 `32958e32…84a`, post 12086161460 `fd96ae97…3ed2`, dicom 12086073335 `3a92cae4…802e`, common 12085209546 `8b49b67b…2b00`. 개인 폴더 `D:\workspace-github\xpe-data\gui-250-dlls` 에 받았다(249 와 같은 스크래치 다운로더: "main 의 성공한 push 실행" 검사만 뺀 복사본, 저장소 스크립트는 그대로). 공유 폴더는 건드리지 않았다(확인 수준: 개인 경로 사용은 앱의 모듈 감사 줄 `6 native module(s) loaded, 6 from the pinned folder, 0 from elsewhere` 로 확인, 공유 폴더 실행 전후 해시는 없음).
- 생성 도구 `xpe_calib_fixture_gen.exe` sha256 앞 16자 `f776445f569a9ae1`, `xpe_preprocess.dll` `612e61b60c3a7de6`. 받은 폴더에는 시험 실행 파일과 .pdb 까지 들어 734 MB 였다.

### 2.2 세트 생성 (IAP-CALIB-001 §1.2~1.3 + 찾아낸 옵션)

목록 파일은 개인 폴더 `gui-250-set-inputs` 에 만들었다: 다크 `Dark_08~14`(7장, §1.3 의 "첫 장 제외"), 게인용 `Bright_23~28`(6장), 결함 맵용 `Bright_17~28`·`31~36`(18장). 명령(출력 `gen_stdout.txt`):

```
xpe_calib_fixture_gen.exe --out D:\workspace-github\xpe-data\gui-250-set --expiry-ms 4102444800000 ^
  --dark-list ...\dark.txt --bright-list ...\gain.txt --bpm-bright-list ...\bpm.txt --bpm-v2 --bpm-margins 6,2,1,1
```

- 출력은 네 줄뿐이다(세 파일의 sha256, 결함 화소 2209, manifest 경로). 진행 표시 없음.
- 세 파일 sha256: offset `5569bacf…0c39`, gain `a7ddadbd…2d22`, defect `3b6fe9ba…2e85`.
- `manifest.json` 의 `defect_format` 인용: `{"xcal_version": 2, "method": "xpe_bpm_generate_v2 + xpe_bpm_find_lines (line mask merged into the pixel map), blobs by xpe_bpm_blobs", "skip_first_dark": true, "margins_ltrb": [6, 2, 1, 1], "pixels_outside_imaging_area": 30699, "line_groups": 0, "line_pixels": 0, "panel_reject": 0, "blobs": 2201, "blobs_listed": 2201, "unconfirmed_candidates": 0, ...}`. `frames` 는 `dark 7, bright 6, bpm_bright 18`.
- 직접 확인한 것: **offset 은 목록의 7장 모두의 평균이다**(`Dark_08..14` 평균과의 최대 차 0.00056, 6장 평균이면 30.5, 8장이면 52.0). `skip_first_dark` 는 결함 맵의 다크 단계에만 적용된다(어느 장을 썼는지는 파일로 확인하지 못했다. 결함 화소가 2,353 → 2,209 로 줄었다는 것만 보았다).
- 비교: 이전 `cal-254`(다크 `07~14` 8장)와 `calib-cyan-v3` 는 offset·gain 파일이 같고, 이번 세트는 둘 다 다르다.

### 2.3 앱 처리 (UIA, 같은 영상 한 장, `Bright_Foot phantom_55kv_200ma_8mas(SID_110)_03.raw`)

시험 코드 `RealTestScenarios.cs`(새 파일). 한 번의 실행이 앱을 띄워 영상을 열고, 상태·로그를 읽고, 비교 모드 세 가지(Source Only, Processed Only, Difference)에서 화면을 복사한 뒤 File > Save Corrected Image 두 항목(float32, 16비트 PNG)을 저장한다. 영상 복사본은 출력 폴더에 두고 연다.

| 읽은 것 | 판 2 (`gui-250-set`) | 판 1 (`calib-cyan-v3`) |
|---|---|---|
| 결과 | 통과 | 통과 |
| 체인 줄 | `preprocess=Applied, enhance_basic=Applied, gsvg=NotRequested, ai_bone_suppress=NotRequested; times: preprocess=249 ms, enhance_basic=244 ms, ...; display input=chain` | 같은 문구, `preprocess=235 ms, enhance_basic=232 ms` |
| 보정 패널(HUD) | `Calib:  3 stages (offset · gain · defect)` | 같음 |
| 알림(로그) | `ALERT INFO REAL_DISPLAY_BACKEND_ACTIVE ...`, `ALERT WARN NATIVE_ALERT: Exposure deviation: DI=10.99 (outside [-3.0, +3.0] range)` | 같은 두 줄, `DI=11.00` |
| 노출 지수(상태줄) | `uncalibrated EI (보정 안 된 EI) = 3142.71, DI = 10.99` | `3143.86`, `DI = 11` |
| 표시 창 | `VOI(auto, LinearExact, C=3665.499, W=1133.181)` | `C=3667.693, W=1129.824` |
| 저장한 float32 sha256 | `2f018e86f64c61c46880a33b31cb46a0b174e30a42469112c862c6c248268bfa` | `3071f62fed14c5582e22…`(전체값은 `run_app_v1.txt`) |
| 가용 메모리 전 → 후 | 18187 → 18294 MB | 18283 → 18301 MB |
| 남은 프로세스 | 0 | 0 |

- 모듈 감사 줄: 초기화 뒤 4개, 첫 체인 뒤 6개 모듈이 적재되고 모두 개인 폴더에서 읽었다.
- 경고 `Exposure deviation` 은 영상의 노출 지수가 범위 밖이라서 나오는 것이고 보정 단계의 경고가 아니다(GUI-C-247 과 같음). 그러나 화면에는 경고로 보인다(§5 항목 17).
- 실행 증거: `run_app_v2.txt`, `run_app_v1.txt`. 헛걸음한 실행은 `try1/` 에 남겼다(§6).

## 3. 판 1 과 판 2 결과 영상의 차이 (수치)

저장한 float32 두 파일을 파이썬으로 비교했다(`analysis_diff.txt`, 분석 스크립트는 커밋하지 않음). 두 결과는 **둘 다** `floor(원시 − offset + 0.5) / gain` 과 결함 마스크 밖에서 0.004 이내(float32 오차)로 같았다. 즉 이 앱의 보정 결과는 "offset 을 뺀 값을 정수로 반올림(.5 는 올림)한 뒤 gain 으로 나누고, 결함 마스크 화소만 이웃 값으로 바꾼 것"이다(이 모형을 위 두 파일에 맞춰 얻었다. 반올림 규칙 자체는 문서에 없다, §5 항목 22).

| 측정 | 판 1 | 판 2 |
|---|---|---|
| 결함 평면 비영 화소 | 134 | 2,209 (두 세트 공통 23, 판 1 에만 111, 판 2 에만 2,186) |
| 모형과 다른 화소(= 결함 단계가 바꾼 화소) | **134 = 마스크와 정확히 같음** | **2,209 = 마스크와 정확히 같음** |
| 마스크 밖에서 모형과 다른 화소 | 0 | 0 |
| 출력 크기 | 3072×3072 (자르지 않음) | 3072×3072 (자르지 않음) |

판 2 대 판 1 (`v2 − v1`):

- 값이 다른 화소 9,437,168 / 9,437,184(거의 전부), 평균 차 −11.57, 중앙값 −11.76, 최대 |차| 574.6. |차| > 5 인 화소 9,256,598 개.
- 원인: offset 파일의 평균 차 +11.84(범위 −51.96~+16.71), gain 비 0.9992~1.0078. 결함 단계를 빼고 두 세트의 offset·gain 만 적용한 값의 차(평균 −11.57)가 위 평균 차와 같다. 즉 **전체적인 차이는 판 2 형식 때문이 아니라 다크 목록 규칙(`Dark_07` 제외) 때문이다.** 판 1 세트는 `Dark_07` 을 포함한 8장으로 만들어졌다.
- 결함 단계가 더한 변화(`(v2 − v1) − (offset·gain 만의 차)` 가 0.5 를 넘는 화소): 2,307곳, 전부 두 마스크의 합집합(2,320 화소) 안이다. 합집합 밖에서는 0곳이다.
- 합집합 안 2,320 화소 중 2,309 곳에서 |차| > 0.5.

촬영 영역 밖 가장자리(판 2 는 여백 좌 6·상 2·우 1·하 1 → 영역 안 9,406,485, 영역 밖 30,699 화소. 파일의 영역 평면이 이 직사각형과 정확히 같음을 확인):

- 판 2: 영역 밖 30,699 화소가 **모두 모형값 그대로**(보간으로 바뀐 화소 0, 자르지 않음). 판 2 결함 평면에는 영역 밖 화소가 0개다.
- 판 1: 영역 밖에 결함 화소 108개가 있고 그 108개가 이웃 값으로 바뀌었다(영역 개념이 없는 판 1 의 동작).
- 판 2 − 판 1 의 영역 밖 차: 30,699 화소 모두 값이 다르고(평균 −6.93, 최대 |차| 381.2), 이는 위의 offset·gain 차이와 판 1 의 영역 밖 결함 보정 108곳이 섞인 것이다. 가장자리 띠 안에서 |차| > 5 인 화소 22,942 개.
- **"영역 밖 가장자리는 값이 그대로여야 한다"(리더 결정 254)는 판 2 에서 성립했다.** 다만 이 결과는 판 2 마스크가 영역 밖을 처음부터 담지 않아서일 수도 있다. 모듈이 영역 평면을 보간에서 빼는 동작(QA-A-255)을 따로 시험한 것은 아니다.

그림(보고서 폴더, 저장소에는 안 올림: `.moai/reports/**` 는 무시 대상): `side_by_side_v1_v2_diff.png`(왼쪽 판 1, 가운데 판 2, 오른쪽 차 −20..+20), `zoom_v1_v2.png`. 모두 float 저장 파일에서 내가 그린 8비트 미리보기이며 앱 화면이 아니다. 차 영상에서 발 모양이 보이는 것은 offset·gain 차이가 신호 크기에 따라 달라서다(결함 맵이 아니라).

## 4. 스크린샷

- 앱 창의 화면 복사(`CopyFromScreen`)와 창이 스스로 그리게 하는 `PrintWindow`(PW_RENDERFULLCONTENT) 를 둘 다 시도했다. 두 방식 모두 창 안쪽이 균일(빈 화면)이었다(`screen_*.png`, `screencopy_*.png`: 각 1560×920). GUI-C-247 과 같은 한계다. 원인은 모른다(이 세션이 WPF 창의 그림을 읽지 못하는 환경으로 추정, 미검증).
- 대신 앱이 저장하는 16비트 PNG(표시용, 값의 1~99 백분위를 늘린 것): `D:\workspace-github\xpe-data\gui-250-out\v2\input_3072x3072_corrected_16bit.png`(16,651,238 바이트, sha256 `f63f4959fb794649dbde4f8988b0820d446cc2cadab5e77bbb9f5634de438269`), `...\v1\...16bit.png`(sha256 `52003463aae73ac1961420fdc641e9a0967fd85e7bcce1fb8dde8c3878bb03a2`). 16 MB 라 보고서 폴더에 복사하지 않았다.
- 보정 전 화면은 얻지 못했다(비교 모드 Source Only 의 화면 복사가 빈 화면). 보정 전 원시 영상은 `input.raw`(출력 폴더의 복사본)다.

## 5. 문서와 실제가 다른 곳 (전부)

표시: **[관측]** 이번 실행에서 직접 확인, **[정적]** 파일·코드를 읽어 확인했고 실행하지 않음. 문서 약칭: IAP = `IAP-CALIB-001`(2.0.0), QS = 빠른 시작 한국어판(`quick-start.ko.html`).

### 5.1 세트 만들기

| # | 문서 | 실제 | 근거 |
|---|---|---|---|
| 1 | IAP §1.3: 도구는 `tools/Get-CiNativeDlls.ps1` 이 내려받는다. QS 준비물도 같은 스크립트 | 스크립트는 "main 의 성공한 push 실행"이 아니면 던진다(`conclusion -ne success -or headBranch -ne main -or event -ne push`). 병합 전 브랜치(dev/preprocess)의 산출물을 받는 방법이 문서에 없다. 이번에는 검사만 뺀 복사본을 썼다 | [정적] 스크립트 49~52줄 근처, [관측] 복사본으로 성공 |
| 2 | IAP 1부·QS: `--bpm-v2`, `--bpm-margins` 가 없다 | 2판 결함 맵은 이 두 옵션을 줘야 만들어진다. 기본은 1판이다. `--help` 에만 있다 | [관측] |
| 3 | IAP: 촬영 영역 여백 값의 권장·측정 방법이 없다 | 이번 값 `6,2,1,1` 은 카드와 이전 세트의 manifest 에서 가져왔다. 패널마다 어떻게 정하는지 문서가 말하지 않는다. 기본 `0,0,0,0` 은 영역 밖 화소 0 | [관측] `--help` |
| 4 | IAP §1.2: "다크 첫 장은 **목록에서 뺀다**" | `--bpm-v2` 도 "첫 다크 프레임을 건너뛴다"고 한다(`--help`). 문서대로 첫 장을 빼고 `--bpm-v2` 를 주면 결함 맵 단계에서 **한 장이 더 빠진다**(목록 `08~14` 중 `08`). 반면 offset 은 목록 7장 모두의 평균이다. 두 규칙의 차이를 문서가 말하지 않는다 | [관측] offset 평균 검증, manifest `skip_first_dark` |
| 5 | IAP §1.5: "문서대로 재현 … v3 와 같은 세트(sha256 일치)" | 현재 절차(`Dark_08~14`)로 만들면 offset·gain 이 v3 와 다르다(`5569bacf…`·`a7ddadbd…` 대 `f1b0f088…`·`e5037865…`). 일치하려면 첫 장 제외 규칙 이전의 `Dark_07~14` 를 써야 한다. §1.3 의 괄호가 이를 일부 밝히지만 §1.5 표는 현행 절차의 재현처럼 읽힌다 | [관측] |
| 6 | IAP §1.1 표: `defect.xcal` ← `xpe_bpm_generate`(32×32 창 …/128×128 창 ±7 %), "결함 화소 맵" | `--bpm-v2` 의 방법은 `xpe_bpm_generate_v2 + xpe_bpm_find_lines`, 파일은 두 평면(결함, 촬영 영역)과 확장 블록(줄·덩어리·후보)을 담는 형식 판 2 다. 표가 갱신되지 않았다 | [관측] manifest |
| 7 | IAP §1.3: 생성 약 7분 | 이 기계에서 2분 31초. 기계·입력에 따라 다를 수 있다 | [관측] |
| 8 | IAP §1.3: `--expiry-ms <만료 Unix ms>`, 값은 "현장 정책" | QS 는 `4102444800000`(2100-01-01)을 쓴다. IAP 만 따르면 값을 직접 계산해야 한다. 두 문서가 서로 가리키지 않는다 | [정적] |
| 9 | IAP §1.2: 원시 프레임 이름(`Dark_08~14` 등)만 말한다 | 프레임이 저장소에 없다(`tests/test_data/cyan_test/` 에는 `README.md`·`fixture.json` 뿐). 어디서 받는지 IAP·QS 모두 말하지 않는다. 이번에는 `image-processing` 워크트리의 `calfiles/` 를 썼다 | [관측] `git ls-files` |
| 10 | IAP §1.3: `manifest.json` 에 입력 파일별 경로·sha256·평균, 장 수, 설정이 기록된다 | 판 2 에서는 `defect_format`(xcal 버전, 여백, 영역 밖 화소, 줄·덩어리 수)이 추가된다. 문서에 없다 | [관측] |
| 11 | IAP §1.3: 도구는 "진행"을 말하지 않는다 | 실행 중 출력이 없고 끝에 네 줄만 나온다(세 파일 sha256, 결함 화소 수, manifest 경로) | [관측] |
| 12 | QS 준비물 "보정 폴더 … 없다면: CalData_6 로 `--caldata` 로 만들 수 있다" | IAP 는 cyan_test 와 목록 파일 방식을 쓴다. 한 사용자가 두 문서에서 서로 다른 생성 절차를 만나며, QS 가 IAP 를 가리키지 않는다(QS 에 IAP 링크 없음). 또 두 문서 모두 판 2 를 말하지 않는다 | [정적] |

### 5.2 앱에서 쓰기·확인

| # | 문서 | 실제 | 근거 |
|---|---|---|---|
| 13 | QS 준비물: DLL 은 `xpe_common`, `xpe_display`, `xpe_preprocess` 와 `fmt`, `spdlog` | 체인 줄이 `enhance_basic=Applied` 라 `xpe_enhance_basic.dll` 도 쓴다(앱의 모듈 감사: 초기화 뒤 4개, 첫 체인 뒤 6개 적재). 준비물 목록에 없다 | [관측] 감사 줄, [추론] 모듈 이름은 로그에 안 나와 개수로만 확인 |
| 14 | QS: 내려받기 "약 300 MB" | 받은 폴더 734 MB(시험 실행 파일·.pdb 포함). 스크립트가 아티팩트를 통째로 복사한다 | [관측] |
| 15 | IAP §1.4: 실행 `tools/Start-ImageProcTestNative.ps1 -CalibDir <폴더> -RawWidth 3072 -RawHeight 3072` | 매개변수 이름은 일치한다(`-RawWidth`·`-RawHeight` 기본 1024, `-NativeDir` 기본 `build\e2e-native-dlls`). 이번에는 이 스크립트를 실행하지 않았고 UIA 시험으로 같은 인자를 앱에 넘겼다 | [정적] |
| 16 | IAP §1.4: 정상이면 **상태줄**에 `Preprocess: xpe_preprocess_pipeline_out (offset -> gain -> defect, input kept)` 가 나온다 | 그 문구는 **로그 패널**에 있고(`Chain preprocess: Applied — Preprocess: xpe_preprocess_pipeline_out (offset -> gain -> defect, input kept) on 3072x3072 (Abdomen)`), 상태줄에는 `chain: preprocess=Applied, ...` 가 나온다. QS 5번은 둘을 올바르게 구분한다 | [관측] |
| 17 | IAP §1.4: "보정 단계 경고·오류가 없다". QS: "볼 수 있는 경고"로 `XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT`, `XPE_WARN_CALIB_SESSION_UNSPECIFIED`, `XPE_WARN_NO_EXPIRY` 를 든다 | 이 세트·이 영상에서는 위 세 가지가 **모두 나오지 않았다**(만료를 줬기 때문). 대신 화면에 `ALERT WARN NATIVE_ALERT: Exposure deviation: DI=10.99 (outside [-3.0, +3.0] range)` 가 나온다. 문서는 이 경고를 어디서도 설명하지 않아, "경고가 없다"고 읽은 사용자는 경고를 본다 | [관측] |
| 18 | QS 5번 정상 화면: 상태줄 끝에 "Exposure index: uncalibrated EI (보정 안 된 EI) = …, DI = … (보정된 영상에서 교정되지 않은 상수로 잰 측정값이며 합격 기준이 아닙니다)" | 실제: `Exposure index: uncalibrated EI (보정 안 된 EI) = 3142.71, DI = 10.99 (measured, not a pass criterion; S0 reference not verified against the gui gain scale).` 앞부분은 같고 괄호 문구가 다르다(영어, 내용도 다름) | [관측] |
| 19 | QS: 보정 영상 저장 시 "원본 파일 이름에 `_corrected_f32le.raw` 를 붙인다" | 실제 저장 이름은 `input_3072x3072_corrected_f32le.raw` — 영상 크기(`_3072x3072`)가 끼어든다. QS 예시의 `wrist_lat_3072x3072_...` 는 원본 이름에 크기가 이미 있어서 구별되지 않는다(이름에 크기가 있을 때 중복되는지는 확인하지 않음). PNG 도 `input_3072x3072_corrected_16bit.png` | [관측] |
| 20 | QS: 상태줄이 저장 뒤 "크기와 SHA-256 을 알려 준다" | 일치: `Saved corrected image (float32 raw): …\input_3072x3072_corrected_f32le.raw — 37748736 bytes, sha256 …` | [관측] 일치 확인 |
| 21 | QS 5번: "times … 이 안내를 확인한 기계에서 3072×3072 가 약 0.3초" | 이 기계에서 `preprocess=249 ms / 235 ms`. 일치 | [관측] 일치 확인 |
| 22 | IAP §1.1: "적용은 `출력 = (raw − offset) / gain` 뒤 결함 화소를 이웃 값으로 채운다" | 실제 결과는 `(raw − offset)` 를 **정수로 반올림(.5 는 올림)** 한 뒤 gain 으로 나눈다(두 세트의 결과가 이 식과 0.004 이내로 일치, 반올림 없는 식과는 약 86~88 % 화소가 0.05 넘게 다름). 문서에 반올림이 없다. 반올림이 의도인지 알 수 없다(문서에 없으므로 설계 의도를 확인할 곳이 없다) | [관측] 분석 |
| 23 | QS 5번: "화면의 영상이 보정 영상" | 이번 세션은 화면을 읽지 못해 확인하지 못했다(§4) | [관측] 한계 |

### 5.3 같은 환경에서 문서 밖으로 만난 것

| # | 무엇 | 근거 |
|---|---|---|
| 24 | 같은 시험 하네스로 열기 대화상자를 열었을 때, 처음 두 번의 앱 처리(§6 의 첫 실행)에서는 되던 "열기" 단추(id 1)를 UI 자동화가 찾지 못하는 상태가 나타났다. 대화상자는 "열기" 분할 단추(`DropDown`)를 가졌고 id 1 단추가 목록에 없었다. 문서와 무관한 시험 환경 문제지만 W16 같은 기존 시험이 같은 기계에서 갑자기 실패한다(원인 미상, 운영체제 쪽 변화로 추정, 미검증) | [관측] `try1/` |
| 25 | 거부 시험 D0~D4(GUI-C-249b)와 함께, 문서가 말하는 "경고 0"을 시험 하네스는 `Exposure deviation` 을 이름으로 허용해 통과시킨다(GUI-C-247 이후 같은 동작). 위 17 번이 같은 사실의 사용자 쪽 면이다 | [정적] |

## 6. 일정에 없던 일과 한계

- **시험 하네스 변경 2건(앱 코드 변경 없음).** `QuickStartWalkthroughScenarios.GuiApp.DriveDialog`: (a) 확인 단추를 UI 자동화가 못 찾으면 그 대화상자의 하위 창을 `GetDlgItem` 으로 id 로 찾아 같은 `BM_CLICK` 를 게시하고, (b) 못 찾았을 때 대화상자 구조를 출력한다. 이 변경 전에는 기존 W16 도 같은 기계에서 같은 단계에서 실패했고(콘솔 확인, 기록 파일은 남기지 않음), 변경 후 통과했다. 변경 전 실패 실행 기록은 `try1/` 에 있다(저장 이름 가정 실수 2회: 판 2·판 1, 열기 대화상자 3회).
- **헛걸음 실행 2종(앱 실행이 카드의 2회를 넘어 총 7회, 기존 W16 확인 3회 별도).** (1) 첫 두 번의 앱 처리는 앱이 저장한 파일 이름이 시험이 가정한 이름(`input_corrected_f32le.raw`)과 달라 저장 대기에서 시간 초과로 실패했다(저장은 됐다: float 파일 존재). 시험의 가정을 고쳤다(§5 항목 19). 그 실행의 앱 상태·알림은 이번 표와 같았다. (2) 이어서 위 대화상자 문제로 세 번(판 2, 판 1, 판 2) 더 실패했다. 최종 결과는 마지막 실행이다.
- 이 실테스트의 범위: 한 영상 한 장, 검출기 1대(cyan_test), 판 1·2 세트 각 1회. 결과 영상이 임상적으로 옳은지는 평가하지 않았다(정답 없음). 화면 캡처는 얻지 못했다.
- 이 시험은 CI 에서 돌지 않는다(세트가 저장소 밖, `XPE-SKIP-ALLOWED:273`).
- 미검증: IAP §1.3 의 "생성은 임시 폴더에서 한 뒤 이름 바꾸기" 동작, 종료 코드 9~14 의 거부 동작(이번 입력은 모두 정상), QS 의 CalData_6 절차, 같은 세트로 `Start-ImageProcTestNative.ps1` 직접 실행.

## 예측과 결과

| 예측 | 결과 |
|---|---|
| 1 `Get-CiNativeDlls.ps1` 은 병합 전 산출물을 거부 | 코드로 확인, 스크래치 복사본으로 성공 |
| 2 종료 코드 0, 결함 파일 version 2 | 맞음 |
| 3 `Dark_08` 이 한 번 더 빠져 **offset 이 6장**으로 만들어짐 | **틀림.** offset 은 목록 7장 모두의 평균이었다. 건너뜀은 결함 맵 단계에만 적용된다. cal-254 와 offset 이 다른 것은 맞으나 이유는 장 수가 아니라 `Dark_07` 포함 여부다. 결함 화소 수가 2,353 과 다른 것(2,209)은 맞음 |
| 4 영역 밖 30,699, 줄 그룹 0 | 맞음 |
| 5 생성 약 7분 | **틀림.** 2분 31초 |
| 6 둘 다 Applied·3단계, 문서가 말한 두 경고는 안 남 | 맞음. 대신 노출 편차 경고 1건 |
| 7 화면 캡처 빈 화면 | 맞음. PrintWindow 도 빈 화면 |
| 8 offset 차가 지배, 결함 효과는 결함 위치에서 따로 | 맞음(결함 단계 변화는 2,307곳, 전부 마스크 합집합 안) |

## Card Cross-Check

| 카드 항목 | card |
|---|---|
| 1. 방법·예측 선커밋 | GUI-C-250 (064f2dda) |
| 2. 문서대로 세트 생성, manifest `defect_format` 인용 | GUI-C-250 (§2.2) |
| 3. 앱에서 판 2·판 1 보정, 상태줄·알림 | GUI-C-250 (§2.3) |
| 3. 보정 전후 스크린샷 | 얻지 못함, 앱의 16비트 PNG 로 대체(§4) |
| 4. 판 1·판 2 차이 수치, 영역 밖 가장자리 | GUI-C-250 (§3) |
| 5. 문서와 실제가 다른 곳 목록 | GUI-C-250 (§5, 25항목) |
| 향상 단계만 적용되는 동작 | #274 |
