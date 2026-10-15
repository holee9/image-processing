# GUI-C-214 — 레거시 미리보기 사슬을 끝까지 실행해 확인 (#249)

## 결론

213 이 고친 것은 호출 모양(인자 3개)뿐이었고, 사슬을 실제로 돌려 보니 **사슬은 213 이후에도 여전히 죽어 있었다.** 214 는 그 원인을 고치고, 사슬 전체를 독립 기대값으로 확인하는 시험 5건과 버퍼 경계 시험 5건을 추가했다.

| 항목 | 213 직후 상태 | 214 후 |
|---|---|---|
| 첫 단계(offset) 결과 | `CALIB_NOT_LOADED (-16)` — 예외로 중단 | OK |
| 사슬 3단계 | 한 번도 끝까지 안 돌았음 | offset→gain→defect 모두 OK, 화소값이 기대값과 일치 |

## 213 이 가려 둔 결함 (실행해서 발견)

`NativePreprocessPreviewService` 는 두 가지를 하고 있었다. 둘 다 코드를 읽어서는 보이지 않았고, 사슬을 실제로 돌리자 첫 호출에서 드러났다.

1. **교정을 모듈에 적재하지 않았다.** `LoadCalibrationFiles` 는 파일이 있는지·만료됐는지만 보고 `xpe_calib_load_*` 를 한 번도 부르지 않았다. 모듈의 교정 3종은 모듈 전역 저장소에 적재된 맵을 읽으므로(#117) 적재 없이 부르면 항상 `CALIB_NOT_LOADED` 다.
2. **교정 파일을 자체 형식("XPEC", CRC-32)으로 썼다.** 모듈이 읽는 형식은 XCal v1(헤더 152바이트, 매직 "XCAL", SHA-256) 이고 offset/gain 은 FLOAT32 이어야 한다. 앱이 쓴 UINT16 "XPEC" 파일은 모듈이 거부한다. 즉 1번만 고쳐도 사슬은 안 돌았다.

고친 방식 (`clients/ImageProcTest/Services/NativePreprocessPreviewService.cs`):

- offset·gain 파일은 모듈의 생성 함수 `xpe_calib_generate_offset/gain` 이 만든다(앱이 형식을 따로 구현하지 않는다). gain 은 offset 소스가 있으면 어두운 기준으로 넘긴다.
- defect 는 모듈에 생성 함수가 없어 XCal v1 DEFECT 파일을 앱이 쓴다(`WriteDefectXCal`, 헤더 152바이트·SHA-256).
- 세 파일 모두 `xpe_calib_load_*` 로 적재하고, 실패하면 단계 이름과 코드를 담아 예외를 낸다.
- 옛 "XPEC" 기록기와 CRC-32 코드는 삭제했다(호출처 없음).

## 시험

`clients/ImageProcTest.E2ETests/Scenarios/Legacy/LegacyPreviewChainScenarios.cs` (5건, `XPE_NATIVE_DIR` 이 없으면 사유와 함께 건너뜀). 32×32 합성 사례를 파일로 만들어 `NativePreprocessPreviewService.Run` 을 부르고, 기대값은 **시험 안에서 입력으로부터 산술로** 계산한다(모듈 호출 없음).

- 기대 offset = raw − dark, 기대 gain = (raw − dark) ÷ ((flat − dark) ÷ 평균(flat − dark)), 기대 defect = 표시 화소를 4이웃(표시 안 된) 평균으로, 나머지는 gain 출력 그대로.
- P01 offset 만 / P02 offset→gain (단계 간 전달, 핫 화소가 아직 뜨거움 확인) / P03 세 단계 + "gain 출력과 다른 화소는 표시 화소 하나뿐" / P04 입력 파일 SHA-256 과 미리보기 화소 불변 / P05 세 교정이 모듈에 적재됐고 파일 머리말이 "XCAL".

실행 결과(스테이징된 `xpe_preprocess.dll`): 5/5 통과.

`clients/ImageProcTest.IntegrationTests/P1AReady/PreprocessCorrectionBoundaryTests.cs` (5건): 출력 버퍼를 큰 배열의 가운데에 두고 앞뒤 64개 원소에 비트 패턴(0x7FC0A5A5)을 깔아 한 원소라도 넘어 쓰면 잡는다. offset/gain/defect 각각 "마진 불변·입력 불변·안쪽에는 실제로 씀", 그리고 "출력이 1화소 작다고 선언하면 거부되고 아무것도 쓰지 않음", 마지막으로 마진 검사기 자체의 대조군(앞·뒤 한 원소 쓰기가 감지되는지). 5/5 통과. 이를 위해 `PreprocessCorrectionChainSmokeTests` 의 보조 함수 몇 개를 `private` → `internal` 로 열었다(동작 변경 없음).

## 반증 (`falsification_arms.txt`)

각 시도는 한 가지만 바꾸고, 다시 빌드해 시험을 돌리고, 바이트 단위로 원상 복구했다.

| 시도 | 결과 |
|---|---|
| 213 직후 서비스(적재 없음, XPEC) | 5/5 빨강 |
| 213 이전 서비스(2인자 호출 모양) | 5/5 빨강 |
| defect 를 모듈에 적재하지 않음 | 3/5 빨강 (P03·P04·P05) |
| gain 을 어두운 기준 없이 생성 | 2/5 빨강 (P02·P03) |
| offset 을 dark+1 로 생성 | 3/5 빨강 (P01·P02·P03) |
| gain 단계에 offset 출력 대신 원본 프레임을 줌 | 2/5 빨강 (P02·P03) |
| defect 지도가 아무 화소도 표시하지 않음 | 1/5 빨강 (P03) |
| 출력 포인터를 한 원소 앞으로 | 경계 시험 3/5 빨강 (offset·gain·defect) |
| gain 출력을 입력 배열에 겹침 | **빨강이 아니라 시험 호스트 중단** (아래) |

솔직한 주석 하나: 마지막에서 두 번째 시도는 결과 0건으로 기록됐다. 직접 다시 돌려 확인했더니 float 출력(4B/화소)을 ushort 입력 배열(2B/화소)에 겹쳐 힙을 덮어써 **시험 프로세스가 중단**됐다("테스트 실행이 중단되었습니다"). 결함은 잡혔지만 개별 시험의 빨강이 아니라 호스트 중단으로 잡힌 것이며, 이 입력 방향의 겹침을 시험 하나가 이름으로 잡는다고는 주장하지 않는다.

또 하나: defect 시도는 P03 하나만 빨갛다. P03 이 "표시 화소 하나만 바뀐다"를 단정하는 유일한 시험이라서이고, P04·P05 는 defect 단계 내용을 보지 않는다. 이는 의도된 분담이다.

## 델리게이트 인자 역할 조사

범위: 카드는 "델리게이트 13개"라 했는데, `delegate` 선언과 `GetDelegateForFunctionPointer`/`GetRequiredDelegate` 바인딩을 세어 보니 **내보내기 38개, (내보내기, 델리게이트) 쌍 55개**가 앱·시험에 바인딩돼 있었다(13은 앱 코드 일부로 보인다). 55쌍 모두를 헤더 선언과 **인자 역할**로 나란히 놓고 읽었다(정적 대조이며 실행한 것은 아님).

역할이 헤더와 어긋난 것: **새로 발견된 것 없음.** 213 이 고친 3개(offset/gain/defect 가 입력·출력·메타데이터 자리를 서로 다르게 쓰던 것)와 이번에 드러난 적재 누락이 전부다. 확인한 점:

| 묶음 | 헤더 인자 역할 → C# 델리게이트 | 판정 |
|---|---|---|
| `xpe_offset/gain/defect_correct` | (입력*, 출력*, 메타*) → `(ref 입력, ref 출력, ref 메타)` | 일치 (213) |
| `xpe_calib_check_expiry` | (경로, `bool*` 만료, `int32*` 남은 일수) → `(IntPtr, IntPtr, IntPtr)`; 호출 쪽이 첫 자리를 1바이트 불리언, 둘째를 int32 로 읽음 | 역할 일치. 단 첫 자리를 8바이트로 할당해 두고 구 ABI(epoch ms)와 겸용하는 판별(`int.MinValue` 초기값)이 있다 — 헤더 대조상 문제 없으나 읽기 어려움 |
| `xpe_calib_generate_offset/gain`, `xpe_calib_load_*` | 경로·프레임 배열·어두운 기준 → 같은 순서 | 일치 |
| `xpe_apply_modality/voi/presentation_lut` | (영상*, 파라미터*) → 서비스는 `(ref 영상, ref 파라미터)`; 탐침은 `(IntPtr, IntPtr)` 에 둘 다 null | 서비스 일치. 탐침은 둘 다 null 만 넣으므로 역할 오용이 있어도 **관측 불가** |
| `xpe_gsdf_calibrate` | (휘도*, 개수, 출력 파라미터*) → 일치; 탐침은 raw 포인터에 null | 위와 같음 |
| `xpe_calc_exposure_index` | (영상*, 메타*, `float*` EI, `float*` DI) → `(ref, ref, out ei, out di)` | 일치 |
| `xpe_noise_*`, `xpe_contrast_enhance`, `xpe_edge_enhance`, `xpe_log_transform` | (영상*, 파라미터*/값) | 일치 |
| `xpe_dicom_open/validate/write` | (경로, `Handle**`) / (경로, `char*` 보고서, 길이) / (경로, 영상*, 메타*) | 일치 |
| `xpe_alloc_image` | (너비, 높이, 형식, 출력*) — 시험은 `IntPtr`, 앱은 `out XpeImageBuffer` | 두 표기 모두 헤더와 일치 |
| `xpe_get_pending_alert` | (색인, `char*` 메시지, 길이, `int32*` 심각도) → 일치 | 일치 |
| `xpe_nonlinearity_correct` | (영상*, 설정 JSON 또는 null) | 일치 |

**조사의 한계**: (1) 정적 대조라 "타입은 같고 역할이 다른" 인자는 이름·주석·호출 쪽 사용을 읽어 판정했을 뿐 실행으로 확인하지 않았다. (2) `IntPtr` 두세 개짜리 raw 델리게이트(탐침)는 타입이 역할을 구분하지 못한다 — 지금은 null 만 넣어 어긋남이 보이지 않을 뿐, 어긋나도 이 탐침으로는 안 보인다. (3) 헤더 파서가 주석을 지운 선언만 읽으므로 `XPE_API` 없이 선언된 내보내기는 이 표에 없다. (4) 60개 헤더 함수 중 C# 미바인딩분은 범위 밖.

## 남은 것 (이관)

- **교정이 모듈 전역 저장소를 바꾼다**: 미리보기 `Run` 은 시작할 때 `shutdown(); init()` 을 부르고(기존), 이제 교정까지 적재한다. 운영자가 다른 곳에서 적재해 둔 교정은 지워진다. Codex #96 이 오라클에서 지적한 것과 같은 결함이며 GUI-C-212b 에서 같이 다룬다.
- 레거시 앱의 CI 빌드 단계는 리더가 212 와 함께 병합하는 ci.yml 패치에 있다. 새 시험은 E2E 프로젝트가 앱 소스 14개를 링크하므로 해당 프로젝트 빌드만 필요하다.
- QA-A-229 M4(교정 세션 경고)는 main 에 아직 없다. 210 사슬/오라클(세션 없는 파일) 재확인은 그 병합 뒤.
