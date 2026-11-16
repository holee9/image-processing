[2026-10-17 22:37 KST] GUI-C-253 보고서 — 부호 있는 offset 품질 측정(xpe_verify_offset_signed)을 앱에 "측정값" 행으로 연결 (Refs #273)

기준: dev/gui 위의 새 커밋. 푸시하지 않는다. API 는 dev/preprocess `026ea384` 의 `xpe_verify_offset_signed`(+`_file`). **실제 DLL 로 부른 시험(Dark_16)은 아직 못 돌렸다**: `026ea384` 를 담은 CI 산출물이 아직 없다(§4). 나머지는 모두 돌렸다.

## 1. 한 일

| 파일 | 내용 |
|---|---|
| `clients/ImageProcTest/Services/OffsetQualityRows.cs` (새) | `XpeOffsetQuality`(120바이트, `Pack = 8`, 필드 15개), 마스크 비트 5개, 결과 읽기(`OffsetQualityOutcome.FromModule`: 반환 코드 → `writtenSize` → 비트 순서), 측정값 행(`OffsetQualityRows.Build`/`Append`) |
| `clients/ImageProcTest/Services/OffsetQualityNative.cs` (새) | 두 함수의 delegate 와 호출. raw·맵 버퍼는 `GCHandle` 로 호출 동안 고정하고 `finally` 에서 해제, `_file` 의 경로는 `LPUTF8Str`, 모듈에 함수가 없으면 예외가 아니라 "측정 안 됨(낡은 DLL)" |
| `clients/ImageProcTest/Services/NativePreprocessPreviewService.cs` | `Run` 이 `RunChain` 결과 뒤에 `AttachOffsetQuality` 로 행을 덧붙인다(약 45줄). 기존 계산·모집단·한계는 건드리지 않았다 |
| `clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj` | 새 파일 두 개를 시험 프로젝트에 연결(3줄) |
| `clients/ImageProcTest.IntegrationTests/Functional/OffsetQualityTests.cs` (새) | 시험 12건 |

`MetricsComputationService.cs`, `DefectMetricRows.cs`, `DefectMetricGates.cs`, `MainWindow.xaml` 은 **이번 커밋에서 한 줄도 바뀌지 않았다**(`git diff --stat` 로 확인). 기존 칸(DarkBias, DSNU_ADU, DarkReduction_dB, ClampRate)의 값·한계·합격 칸은 같은 코드가 같은 입력에서 만든다.

## 2. 화면 (사용자가 보는 것)

이 앱(`clients/ImageProcTest`)의 지표 표 중 "Dark" 표(`DarkMetricsGrid`) 맨 아래, 기존 네 줄 뒤에 다음 줄이 붙는다(오프셋 단계가 실행된 때만. 안 돌았으면 기존 줄이 이미 "not executed" 라고 말한다).

| 줄 | 값 | 설명(Gate 칸) | 상태 |
|---|---|---|---|
| 측정값 기준 | 원본 크기 원시 영상 + 같은 크기 offset 맵 | 위 DarkBias·DSNU_ADU·DarkReduction_dB·ClampRate 는 **미리보기 기준**이라 모집단과 단계가 다르다. 한계는 그 칸에만 적용된다 | INFO |
| DarkBias 부호 있음 | `-4.0542 화소값` 등 | 측정값 · 판정 없음 · offset 을 뺀 뒤 0 바닥 전 · 원시의 어두운 10분위 | REPORTED |
| DSNU 부호 있음, DarkReduction 부호 있음 | 값, dB | 같은 틀 | REPORTED |
| DarkBias 출력, DSNU 출력 | 값 | 0 바닥과 반올림 뒤 UINT16 출력 · 같은 어두운 10분위 | REPORTED |
| ClampRate x<0 원본 전체 | 값 % | 0 바닥 전에 음수인 화소 비율 · 프레임 전체 | REPORTED |
| 출력 0 비율 원본 전체 | 값 % | 반올림 뒤 출력이 0 인 화소 비율 · 프레임 전체(위 ClampRate 와 다른 양) | REPORTED |
| Offset 맵 SHA-256(payload) | 앞 16자 + `…` | 맵 payload(float32 바이트열) 기준이라 .xcal 헤더의 해시와 다르다 · 만료를 검사하지 않으므로 적용 가능한 맵이라는 뜻이 아니다 | REPORTED |

- **판정이 없다.** 새 줄의 상태 칸은 `REPORTED`·`INFO`·`N/A` 뿐이고 `PASS`·`REVIEW` 는 없다. 새 줄에는 한계가 적혀 있지 않다(DSNU ≤ 20, ClampRate ≤ 1 % 를 새 값의 승인 기준처럼 설명하지 않음).
- **측정되지 않은 것은 측정된 것처럼 보이지 않는다.** 모듈의 `validMask` 비트가 꺼진 그룹, 호출하지 않았거나 모듈이 오류를 돌려준 경우의 모든 줄은 값 칸이 `측정 안 됨`, 상태 `N/A`, 설명 칸에 이유가 적힌다. 비트가 꺼진 필드가 큰 값을 담고 있어도, 0 이어도 읽지 않는다. 비트가 켜진 0 은 0 으로 보인다(`0 %`).
- 단위는 "화소값"이다(기존 줄의 `ADU` 는 바꾸지 않았다).
- 도움말 한 줄 자리: `OffsetQualityRows.SignChangesHelp`(비어 있음, 비어 있으면 줄이 없다). 문구는 리더가 넘긴다.

## 3. 언제 호출되고 언제 "측정 안 됨"이 나오는가

미리보기는 한 변이 4096 화소 이하이면 줄이지 않는다(`RawPreviewService.LoadUInt16Preview` 의 기본 `maxPreviewSide = 4096`, 줄임 간격 1). 그래서 3072×3072 원시 영상이면 미리보기가 곧 원본 크기이고, 앱이 모듈에 만들어 읽히는 offset 맵(`GenerateOffsetXCal`, 미리보기 크기)도 원본 크기다. 이때 `AttachOffsetQuality` 는 원본 raw 파일을 다시 읽어 그 맵 파일(`_file` 변형)로 부른다. 맵 크기가 원본과 다를 때(한 변이 4096 을 넘어 미리보기가 줄어든 큰 영상)만 호출하지 않고 줄이 `측정 안 됨 — 원본 크기(WxH) offset 맵이 없음: 이 화면의 맵은 미리보기 크기(wxh). 위 칸은 미리보기 기준` 이라고 말한다(pre 초안의 "미리보기만 있으면 이 API 를 부르지 말고 미리보기 기준이라고 표시"). 원시 파일이 `W×H×2` 바이트가 아니거나 읽기가 실패해도 같은 방식으로 이유가 적힌다.

주의: 호출에 쓰는 raw 는 **지금 열어 둔 원시 영상**이다(다크 프레임이라는 보장이 없다). 모듈은 그 영상의 어두운 10분위를 ROI 로 재므로 임상 영상을 열었다면 "어두운 10분위"는 다크 측정이 아니다. 줄의 설명이 "원시의 어두운 10분위"라고만 말하며 다크 프레임이라고 하지 않는다. 앱 전체 경로를 실제 DLL 로 돌려 화면에서 확인한 것은 아직 없다(§4).

## 4. 시험과 증거

시험은 이것만, 동시 실행 1개, 실행 뒤 남은 프로세스 0.

| 실행 | 결과 | 파일 |
|---|---|---|
| `OffsetQualityTests` 12건 | 11건 통과, 1건 건너뜀(아래) | `run_offset_quality_tests.txt` (첫 실행의 실패 1건은 시험이 기존 줄 이름 `DSNU_ADU` 에도 "ADU 금지"를 걸어 생긴 시험 오류. 고쳐 다시 돌림: `run_offset_quality_tests_try1_adu.txt`) |
| 서명 일치 검사 + 제어 시험, **026ea384 헤더**(실행 동안만 `modules/preprocess/include/xpe/preprocess_api.h` 를 바꿔 놓고 되돌림, `git status` 로 변경 없음 확인) | 53건 통과: 새 구조체·delegate 두 개가 헤더와 일치 | `run_parity_with_026ea384_header.txt` |
| 같은 검사, **이 워크트리의 main 헤더**(`xpe_verify_offset_signed` 선언 없음) | `NativeModuleSignatureParityTests` 1건 **빨강**: `"xpe_verify_offset_signed(_file)" is bound to delegate … but no module header declares that export` | `run_parity_with_main_header.txt` |
| 반증: C# 구조체 `RoiPixels` 를 `uint` → `ulong` | 서명 일치 검사 빨강(구조체 레이아웃 불일치)과 `TheStruct_Is120Bytes…` 빨강(120 대 128). 되돌려 바이트 동일 확인 | `run_mutation_struct_field.txt` |

시험이 확인하는 것: 구조체 120바이트와 15개 필드의 오프셋(0, 4, 8, 12, 16, 24, 28, 32, 64, 72, 80, 88, 96, 104, 112)과 필드 수, 마스크 비트 5개, 반환 코드가 실패이면 구조체가 값으로 차 있어도 읽지 않음, `writtenSize` 가 120 이 아니면 읽지 않음, 비트 꺼진 그룹의 "측정 안 됨", 측정된 0 은 0, 유한하지 않은 값은 측정값이 아님, 미호출 사유가 모든 줄에 나옴, 새 줄에 판정·한계·`ADU` 가 없음, `Append` 가 기존 줄을 값·한계·판정·순서 그대로 두고 뒤에 붙임.

- **건너뛴 시험(실제 다크):** `TheRealModule_OnDark16AndTheCleanSetMap_MeasuresTheSignedBiasThePaperReports` 는 `026ea384` 이후 빌드의 `xpe_preprocess.dll`(`XPE_C253_NATIVE_DIR`), `Dark_16.raw`(`XPE_C253_DARK`), `clean_set_v1/offset.xcal`(`XPE_C253_OFFSET`)이 있을 때만 돈다. 없으면 이유와 `XPE-SKIP-ALLOWED:273` 으로 건너뛴다. 기대: 부호 있는 bias −4.0542(소수 셋째 자리), 출력 bias 0.1875, 클램프율 83.1 %, 파일 변형과 메모리 맵 변형이 같은 값과 같은 맵 SHA-256. **이 시험은 지금까지 한 번도 DLL 과 함께 돌지 않았다.** dev/preprocess 의 CI 기록에서 `97b9367b`·`b96707d5`·`58b40297` 모두 `026ea384` 를 조상으로 갖지 않는다(`git merge-base --is-ancestor` 로 확인). 리더가 push 한 뒤의 CI 산출물이 올라오면 이 시험 하나만 돌려 `report.md` 에 덧붙인다.
- **기존 칸 불변의 증거 범위:** 같은 입력에서 앱의 기존 계산 결과를 전·후 비교하는 시험은 만들지 못했다(그 계산은 WPF 비트맵과 큰 서비스 파일에 묶여 시험 프로젝트에 연결할 수 없다). 대신 ① 기존 코드 파일이 이번 커밋에서 바뀌지 않았음(위 §1), ② `Append` 가 기존 줄을 읽지도 바꾸지도 않는다는 시험으로 근거를 삼았다. "같은 입력에서 값과 판정이 같다"는 직접 비교는 아니다.

## 5. 순서 의존 (리더 확인 필요)

이 커밋의 C# delegate 두 개는 헤더에 `xpe_verify_offset_signed(_file)` 가 있어야 서명 일치 검사를 통과한다. 이 워크트리의 헤더(main)에는 아직 없어 **dev/gui 만 따로 CI 를 돌리면 이 검사 1건이 빨강**이다. `026ea384` 의 헤더가 들어간 통합 브랜치에서는 통과한다(53건). 허용 목록에 "한쪽만 선언"으로 넣는 것은 헤더가 들어오면 낡은 항목이 되어 다시 실패하므로 하지 않았다.

## 6. 한계

- 위 §3: 한 변이 4096 을 넘는 영상에서는 "측정 안 됨"이 나온다(미리보기가 줄어 맵이 원본 크기가 아님).
- 앱 전체 경로(미리보기 실행 → 줄 추가)를 실제 DLL 로 돌려 보지 못했다. 단위 시험은 행 만들기와 호출 래퍼의 읽기 규칙까지다.
- 화면(WPF 표)을 직접 보지는 않았다. 줄의 내용은 시험이 `DetectorMetricRow` 로 확인했고 표는 기존 `DarkMetricsGrid` 의 `ItemsSource` 가 그대로 보여 준다. UIA 로 이 표를 읽는 E2E 는 만들지 않았다(기존 표를 읽는 E2E 가 없다).
- 이 카드는 `clients/ImageProcTest`(WPF 응용)의 지표 표에 연결했다. `gui/ImageProcTest`(빠른 시작이 쓰는 앱)에는 이 지표 표가 없다.
- 한계값 변경, 합격 칸 추가, 다크 두 준위의 원인 추정은 하지 않았다(카드가 금지).

## Card Cross-Check

| 카드 항목 | card |
|---|---|
| 1. P/Invoke: 120바이트 `Pack = 8`·오프셋 단언, 반환 코드 → writtenSize → 비트 순서, UTF-8 경로, 버퍼 고정 | GUI-C-253 (§1, §4) |
| 2. 원본 raw 와 맵으로 호출 | GUI-C-253 (§1, §3: 맵이 원본 크기일 때. 한 변 4096 이하 영상은 항상) |
| 3. 화면: 판정 없는 측정값 행, 기준 차이 표시, 미측정 표시, 도움말 자리 | GUI-C-253 (§2) |
| 4. 시험: 구조체 오프셋, 미측정 표시, 기존 합격 칸 불변, Dark_16 | GUI-C-253 (§4; 기존 칸 불변은 간접 증거, Dark_16 은 DLL 대기) |
