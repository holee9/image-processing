[2026-10-17 22:52 KST] GUI-C-253b 보고서 — 측정값 행: 실제 DLL 시험 수정과 다크 프레임 확인 시에만 측정 (Codex #240 B-2·B-3, Refs #273)

기준: GUI-C-253 `aa8ec6ae` 위의 새 커밋. 푸시하지 않는다. **이 기능은 `clients/ImageProcTest`(WPF 응용의 지표 표)에 붙어 있다.** 빠른 시작이 쓰는 `gui/ImageProcTest` 에는 이 기능이 없다(그 앱에는 이 지표 표가 없다).

## 1. B-2: 실제 DLL 시험 (끝까지 통과)

원인(Codex 가 짚은 대로): `Assert.Equal(0.1875, q.DarkBiasOutput, 3)` 는 기대값 0.1875 를 3자리로 반올림해 0.188 로 만들고, 실제 값 0.18748707914735363 은 0.187 이라 정상 값에서 실패했다. 제품 값은 정상이다.

고침: 보고서(QA-A-266 §4)가 적은 4자리 요약값을 **명시적 절대 오차**로 비교한다(`AssertNear`: 부호 있는 bias −4.0542 ± 5e-5, 출력 bias 0.1875 ± 5e-5, 클램프율 83.1 ± 0.05). 오차는 4자리 요약값의 마지막 자리의 절반이다. 기대값을 바꿔서 통과시킨 것이 아니며, 기대값의 출처와 오차 근거를 시험 주석에 적었다. 시험은 이제 값을 출력 파일에도 남긴다.

실행(로컬 DLL, 한 번에 하나, 남은 프로세스 0, 가용 메모리 15426 MB):

| 항목 | 값 |
|---|---|
| DLL | `D:\workspace-github\xpe-data\gui-253-local-dll\xpe_preprocess.dll` — xpe-pre 워크트리 `build/ci-preprocess/bin/` 에서 복사한 **로컬 빌드(CI 산출물이 아님)**, 원본 수정 시각 2026-10-17 22:45:13 KST, sha256 `d86042811c75e222dce47e0a373288bc65b714ab0d721e7f85ede1ada2ec46c5`. `xpe_verify_offset_signed`·`xpe_verify_offset_signed_file` 두 이름이 들어 있음(바이트 검색으로 확인). 함께 복사: `xpe_common.dll` sha256 `402630bd5d076d3809382883525e067ffcc9f93d76ec379774de8dbb04e4152f`, `fmt.dll`, `spdlog.dll`. 어느 커밋에서 빌드됐는지는 모른다(Codex 는 266b 구현의 로컬 빌드로 봄). pre 가 계속 빌드하므로 복사본을 썼다 |
| 입력 | `Dark_16.raw`(18874368 바이트), `bpm-eval-260/clean_set_v1/offset.xcal`(37748888 바이트) |
| 측정(`run_real_module_local_dll.txt`) | validMask `0x1F`, roiPixels 885197, pixelCount 9437184, 맵 3072×3072, 맵 payload sha256 `fb2a46c03dd6e7c9209fe23003911aa356a26aae1a691df4510b67ce808d6b05`. 부호 있는 bias **−4.054200149311749**, 부호 있는 DSNU 3.3816844736499916, DarkReduction 9.467839263545368 dB, 클램프율 83.10492833455403 %, 출력 0 비율 86.96606953938802 %, 출력 bias **0.18748707914735363**, 출력 DSNU 0.7237585606585097 |
| 결과 | 통과. 파일 변형과 메모리 맵 변형(`xpe_verify_offset_signed`, 같은 맵을 float32 로 읽어 넘김)이 같은 값과 **같은 맵 SHA-256**을 냄 |

반증(`run_mutation_*.txt`, 변형마다 원래 파일로 되돌려 재빌드, 바이트 동일 확인):

| 변형 | 결과 |
|---|---|
| 기대 bias −4.0542 → −4.06 | 빨강: `signed bias: -4.054200149311749 is not within 5E-05 of -4.06` |
| 메모리 맵의 한 값을 +1 | 빨강: 파일 변형과 메모리 변형의 맵 SHA-256 이 달라짐 |

전체 `OffsetQualityTests`: DLL 환경변수를 주면 18건 통과(`run_offset_quality_tests_with_local_dll.txt`), 안 주면 17건 통과 + 실제 모듈 1건 건너뜀(이유와 `XPE-SKIP-ALLOWED:273`, `run_offset_quality_tests_without_dll.txt`). 최종 통합 CI 의 DLL·헤더 출처 확인은 따로 남는다(리더).

## 2. B-3: 다크 프레임으로 확인했을 때만 측정

### 2.1 입력 종류를 아는 기존 수단이 있는가 (코드로 찾은 결과)

| 후보 | 결과 |
|---|---|
| 열어 둔 원시 영상(`RawFileDescriptor`, `RawPreviewResult`) | 이름·크기·경로뿐. 영상 종류를 담는 필드 없음 |
| 취득 메타데이터 | 없음. `CreateMetadata()` 는 고정 상수(`BodyPart = "CHEST"`, kVp 120 등)를 모듈에 넘길 뿐이고 영상에서 읽은 것이 아님 |
| 보정 검증 화면의 데이터셋 역할(`CalibrationRole`: Offset·Gain·Defect·DefectOracle·Reference·Unknown) | **보정 파일**(`FixtureCaseInfo.CalibrationFiles`)에 붙는 역할이다. 열어 둔 영상이 아니라 fixture 폴더의 보정 원본이 대상이다. 역할은 fixture `manifest` 의 명시된 역할이면 확정이지만, 없으면 `FixtureCatalogService.InferCalibrationRole` 이 **파일 이름으로 추정**한다(어느 쪽인지 기록은 남지 않음). 카드가 금지한 "파일 이름 추측"과 같은 뿌리라 "확인됨"의 근거로 쓰지 않았다 |
| 평균 밝기 등 영상 통계 | 쓰지 않음(카드가 금지) |

결론: 앱에 열어 둔 영상의 종류를 **확인**하는 수단은 없다. 그래서 사용자가 고르는 입력 종류를 새로 두었다.

### 2.2 만든 것

- 지표 탭의 "Detector-Domain Dark Metrics" 위에 `RawInputKindComboBox`(자동화 ID 동일): **미확인(기본)**, 다크 프레임, 밝은 영상(평탄), 임상·기타 영상. 옆에 "다크 프레임일 때만 부호 있는 다크 줄을 측정, 다음 네이티브 미리보기 실행부터 적용"이라고 적었다.
- 새 영상을 열면 선택이 **미확인으로 돌아간다**(앞 파일에 대한 말이 다음 파일에 이어지지 않게).
- `NativePreprocessPreviewService.Run` 에 선택적 인자 `inputKind`(기본 `Unconfirmed`)를 더했다. 다른 호출자(`Phase1bFixtureE2eService`, `PreprocessFixtureE2eService`)는 인자를 안 주므로 항상 미확인이다.
- `OffsetQualityPlan.WhyNotCalled` 가 호출 전에 판단한다. **입력 종류를 먼저** 보고(확인된 다크가 아니면 맵이 있어도 호출하지 않음), 그다음 맵(없음/크기 다름은 별도 이유).

| 입력 종류 | 동작 | 화면 |
|---|---|---|
| 다크 프레임 + 원본 크기 맵 | 호출 | 측정값 줄 |
| 미확인(기본) | 호출 안 함 | 모든 줄 `측정 안 됨`·`N/A`, 이유 `다크 프레임 미확인 — 다크 품질을 측정하지 않음` |
| 밝은 영상 / 임상·기타 | 호출 안 함 | `다크 프레임이 아님(입력 종류: …) — 다크 품질을 측정하지 않음` |
| 다크 프레임 + 맵 없음 / 크기 다름 | 호출 안 함 | 별도 이유(`offset 맵 요청이 없음`, `원본 크기(WxH) offset 맵이 없음 … 미리보기 기준`) |
| offset 단계 미실행 | 새 줄 없음 | 기존 동작 그대로(기존 줄이 이미 "not executed" 라고 말함) |

기준 줄은 "다크 프레임으로 확인된 원본 크기 원시 영상 + 같은 크기 offset 맵"이라고 적는다.

### 2.3 시험 (`OffsetQualityTests` 에 5건 추가)

확인된 다크는 호출(`null` = 호출 허용), 미확인은 맵이 있어도·없어도 호출하지 않고 위 이유가 모든 줄에 나옴(`N/A`), 밝은 영상·임상·기타는 호출하지 않고 사용자가 고른 종류가 이유에 나옴, 확인된 다크의 맵 문제는 입력 종류 이유가 아닌 별도 이유, 기본값이 미확인. "offset 미실행은 기존 동작"은 `AttachOffsetQuality` 의 첫 줄 조기 반환이 하는 일이라 단위 시험이 없다(그 함수는 네이티브 모듈 핸들이 필요하다).

## 3. 그 밖의 확인

- 서명 일치 검사 + 제어 시험, 026ea384 헤더로 53건 통과(`run_parity_with_026ea384_header.txt`; 실행 동안만 헤더를 바꿔 놓고 되돌림, `modules/` 변경 없음). main 헤더만으로는 새 API 헤더가 없어 dev/gui 단독에서 이 검사 1건이 빨강인 것은 알려진 상태이며 허용 목록으로 숨기지 않았다(통합 브랜치에서 확인).
- WPF 화면(콤보 상자의 모양, 표의 줄)은 직접 열어 보지 않았다. 콤보 상자 선택 → 서비스 인자로 이어지는 연결은 컴파일되지만 실행으로 확인한 것은 없다. UIA 시험도 없다.
- 호출은 여전히 한 변이 4096 화소 이하인 영상에서만 원본 크기 맵이 생겨 이뤄진다(GUI-C-253 §3).

## Card Cross-Check

| 카드 항목 | card |
|---|---|
| B-2. 절대 오차로 고치고 맵 변형 검사까지 실행, DLL 출처 sha256 | GUI-C-253b (§1) |
| B-3. 기존 수단 조사 후 보고, 없으면 사용자 선택(기본 미확인), 비다크·미확인은 미호출·N/A | GUI-C-253b (§2) |
| 시험: 확인된 다크, 비다크·미확인, 원본 맵 없음 별도 이유, offset 미실행 | GUI-C-253b (§2.3; offset 미실행은 단위 시험 없음) |
| 3. `clients/ImageProcTest` 에 붙었음을 명시 | GUI-C-253b (머리말) |
| 4. 서명 검사는 허용 목록으로 숨기지 않음 | GUI-C-253b (§3) |
