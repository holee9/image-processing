# QA-A-205b — Codex #29 보류 수정: 출력 용량 계약, 크기 오버플로, 품질 필드 빈 값

기준 커밋 `1f5c072b`(QA-A-202c) 위. 감사 원문은 `evidence/00_codex29_audit_original.md`. 증거는 `evidence/` (번호 순). 이슈 #234, #233.

## 1. 주장

| # | 주장 | 근거 |
|---|---|---|
| A1·A2 | 파이프라인 세 진입점(`xpe_preprocess_pipeline`, `_ex`, `_batch`)은 단계가 하나라도 돌기 **전에** 최종 프레임의 바이트 수를 계산하고, `dataSize` 가 그보다 작으면(0 포함) `XPE_ERR_BUFFER_TOO_SMALL` 을 돌려준다. 거부된 호출은 화소·메타데이터·단계 플래그·형식을 바꾸지 않는다. 성공이면 완전한 최종 프레임을 쓴다. | 빨강 `02_red_capacity.txt`, 초록 `05_green_oom.txt`, 반증 `arm_noroom_*`, `arm_u16only_*` |
| A3 | 폭·높이가 0이거나, 바이트 수(uint16 *2, float32 *4 둘 다)가 `size_t` 에 들어가지 않으면 `XPE_ERR_INVALID_INPUT`. 프레임을 읽기 전에 거부한다. | 빨강 `02_red_capacity.txt`, 반증 `arm_nooverflow_*` |
| B1 | 교정 파일(XCal) 게인의 품질 필드는 키가 **있으면** 숫자여야 한다. 빈 값(`""`), 값 없는 키, 객체·배열, 닫히지 않은 문자열, 잘못된 숫자는 `XPE_ERR_CONFIG_INVALID` 이고 게인 맵·품질 메타데이터는 호출 전 그대로다. 키가 **없을 때만** "없음". | 빨강 `03_red_quality.txt`, 초록 `06_green_quality.txt`, 반증 `arm_emptyabsent_*`, `arm_notscalarabsent_*` |
| B2 | 예전 `atoi/atof` 가 받던 `"2x"`, `"3 "`, 정수 칸의 `"1e2"` 를 거부하는 것은 의도된 정책이며 계약 문장으로 적었다(아래 5절). | 헤더 `preprocess_api.h` `xpe_calib_load_gain` |

## 2. 무엇을 바꿨나

- `pipeline.cpp` `pipeline_core`: ① `frame_bytes()` — 0 검사, `SIZE_MAX / height`, `SIZE_MAX / elemSize` 경계 검사를 곱셈 **전에** 한다. uint16 바이트와 float32 바이트 둘 다 구한다. ② `final_result_is_float()` — 단계 조건을 그대로 따라 최종 결과가 float32(게인·비닝>1·결함·핸들 있는 고스트 중 하나가 돎)인지 uint16인지 정한다. ③ 순서: 오버플로·0 → `INVALID_INPUT`; `0 < dataSize < uint16 프레임` → `INVALID_INPUT`(입력이 안 들어 있음, 205 유지); `dataSize < 최종 프레임` → `BUFFER_TOO_SMALL`. ④ 마지막 복사는 `min(dataSize, …)` 가 아니라 최종 프레임 전체(`outputBytes`)이고, 최종 단계가 호출자 버퍼 자신이면(모든 단계 bypass) 복사하지 않는다(같은 범위로의 memcpy는 미정의). 최종 단계 버퍼가 `outputBytes` 보다 작으면 쓰기 전에 `PROCESSING_FAILED`(도달 불가한 방어).
- `helpers.cpp` / `xpe_preprocess_internal.h`: `xpe_json_find_scalar()` 를 추가 — 키 없음 / 스칼라(빈 값 포함) / 스칼라 아님 셋을 가른다. `xpe_json_get_string()` 은 그 위에 얹어 **동작을 그대로** 유지한다(키 없음·스칼라 아님·빈 값 모두 `""`; 파이프라인 설정이 쓰는 규칙).
- `xpe_calib_mode.cpp`: `read_u8_field` 와 `fit_r_squared` 읽기가 `find_scalar` 를 쓴다 — 키가 있는데 스칼라가 아니거나 비었으면 거부.
- 공개 헤더: 파이프라인 세 함수의 용량·크기 계약, 게인 로더의 품질 필드 계약과 호환성 문장.

## 3. 결정과 이유 (리더가 뒤집을 수 있음)

1. **`0 < dataSize < width*height*2` 는 `INVALID_INPUT` 으로 유지**했다. 카드는 "작으면 BUFFER_TOO_SMALL" 이라 했지만, 이 구간은 출력 이전에 입력 프레임이 버퍼에 들어 있지 않은 경우이고, api-spec 입력 규칙(`api-spec.md:98`)과 QA-A-205 의 시험 `AClaimSmallerThanTheFrameIsRefusedBeforeAnythingIsDone` 이 `INVALID_INPUT` 으로 고정한다. 리더의 시험 행렬({0, N*2, N*4-1, N*4, N*4+13})에는 이 구간 값이 없다. 한 코드로 합치려면 한 줄(두 번째 `if` 삭제)이다.
2. **배치는 프레임마다 검사**한다(첫 프레임 전에 전부 검사하지 않음). 이유: 배치의 계약이 "실패한 프레임 뒤에도 계속하고 첫 오류를 돌려준다" 이고, 프레임들이 같은 치수라는 보장이 없어서, 전부 먼저 검사하면 프레임 3 의 버퍼가 프레임 1 의 처리 여부를 정하게 된다. 시험 `ABatchChecksTheRoomOfEachFrame…`.
3. **float 바이트 수 오버플로는 최종 결과가 uint16 이어도 검사**한다(카드: "출력 바이트 계산(float32)에도 같은 검사"). 보수적이지만 그런 치수는 어차피 처리할 수 없다.
4. **우선순위**: 이 검사는 readout·교정 적재 여부 등 모든 단계보다 앞이다. 그래서 `GainStageFailurePropagates` 같은 "교정 없음" 시험은 버퍼가 맞아야 `CALIB_NOT_LOADED` 에 닿는다(아래 4절 — 이 시험들의 버퍼가 부족했다).

## 4. 이번에 드러난 것 — 같은 결함을 시험 14건이 통과시키고 있었다

계약을 고치자 기존 시험 14건이 `-8` 로 빨개졌다(DLL 9건: `PipelineExTest.{PipelineExWithState, BatchProcessesMultipleFrames, BatchSingleFrameWorks, BatchContinuesOnError}`, `PipelineStageTest.{GainStageFailurePropagates, OffsetAndGainStagesRunWithCalibrationLoaded, AllStagesEnabledRunInSequence, DefectStageFailsWhenItsMapIsNotLoaded}`, `CalibFixtureGenTest.GeneratedSetDrivesThePipeline`; OOM exe 5건: 파이프라인 세 진입점의 OOM 시험 + 202c 의 교정 세트 시험 둘). 열어 보니 각각의 픽스처는 이런 두 모양 중 하나였다(`07_dll_full_first.txt` 가 첫 실행 기록):

- 버퍼가 `uint16 × W*H`(= N*2 바이트)뿐이고 `dataSize = N*2` 인데 게인 또는 결함 단계가 켜져 있다 — float 결과(N*4)의 앞 절반만 쓰이고 OK 가 돌아왔다. Codex #29 A1 의 재현과 같은 모양이다. (DLL 8건: `test_pipeline_ex.cpp` 4건, `test_pipeline_stages.cpp` 4건)
- 버퍼는 N*4 로 넉넉한데 `dataSize = N*2` 라고 적었다 — `CalibFixtureGenTest.GeneratedSetDrivesThePipeline`(DLL 1건)과 OOM exe 의 `pipe::setup` 픽스처(OOM 5건 전부가 이 픽스처를 쓴다).

모두 **버퍼의 실제 크기에 맞춰** 고쳤다(버퍼를 `2*W*H` 개 uint16 으로 키우고 `dataSize = W*H*sizeof(float)`). 시험의 기대값은 하나도 바꾸지 않았다. 새 시험(용량 행렬)이 이전 시험이 못 본 이유: 열어 본 세 시험(`PipelineExWithState`, `BatchContinuesOnError`, `GainStageFailurePropagates`)은 반환 코드와 플래그만 단언했고 출력 프레임의 완전성은 비교하지 않았다(나머지 시험은 열어 보지 않았다). 새 시험은 청결한 실행의 전체 프레임과 바이트 단위로 비교하고, 최종 프레임 뒤의 바이트가 쓰이지 않았음도 단언한다.

## 5. 계약 문장 (리더가 api-spec·변경 이력에 옮김)

> **파이프라인의 용량 계약.** 파이프라인 세 진입점은 결과를 입력과 같은 버퍼(`img->data`)에 쓰므로 `img->dataSize` 는 입력 크기이면서 결과를 위한 용량이다. 최종 프레임은 게인·비닝(binningMode>1)·결함·고스트(핸들 있음) 중 하나라도 돌면 float32(`width×height×4` 바이트), 아니면 uint16(`width×height×2`)이다. `dataSize` 가 최종 프레임보다 작으면 — 0 을 포함해서 — `XPE_ERR_BUFFER_TOO_SMALL` 이고, 읽기·쓰기·플래그 설정은 일어나지 않는다. **변화:** 이 세 진입점에서 `dataSize == 0` 은 더 이상 "크기 미지정"(#123, 입력 전용 버퍼의 규칙)이 아니다 — 출력 용량을 모르면 쓸 수 없다. 개별 단계 API 의 입력 규칙은 그대로다. `0 < dataSize < width×height×2`(입력이 버퍼에 없음)는 `XPE_ERR_INVALID_INPUT`. 폭·높이가 0이거나 바이트 수가 `size_t` 에 들어가지 않는 프레임도 `XPE_ERR_INVALID_INPUT`. 더 큰 `dataSize` 는 받아들여지고 최종 프레임 뒤의 바이트는 쓰이지 않는다. 배치는 프레임마다 이 검사를 하고, 거부된 프레임은 그대로 둔 채 다음 프레임을 처리하며 첫 오류를 돌려준다.

> **교정 파일 게인의 품질 필드.** 설정 블록의 `fit_r_squared`, `polynomial_degree`, `actual_dose_levels`, `calibration_mode` 는 키가 없을 때만 "주어지지 않음"이다. 키가 있으면 `fit_r_squared` 는 유한한 실수, 나머지는 [0,255] 의 정수여야 하고(앞 공백과 단일 `+` 허용, 값 전체가 십진수), 빈 값·스칼라가 아닌 값(객체·배열)·닫히지 않은 문자열·잘못된 숫자는 `XPE_ERR_CONFIG_INVALID` 이며 게인 맵과 품질 메타데이터는 그대로 남는다. **호환성(의도된 정책):** `"2x"`, `"3 "`, 정수 칸의 `"1e2"` 는 예전에는 `atoi/atof` 로 2·3·1 로 읽혔고 이제는 파일을 거부한다 — 숫자가 아닌 품질 필드는 데이터가 아니다.

**두 규칙이 다른 이유 (한 줄).** 파이프라인 설정 JSON 은 GUI 가 미설정 옵션을 `""` 로 보낼 수 있어 "빈 값 = 키 없음"(202b)을 유지하고, XCal 은 생성기가 만든 서명된 데이터라 빈 필드는 미설정이 아니라 결함이다. 시험 `AQualityFieldThatIsAbsentIsStillNotGiven…` 이 두 규칙이 공존함(교정 쪽은 없으면 통과, 설정 쪽 `detectorTempC:""` 는 통과)을 같은 실행에서 단언한다.

## 6. B2 — 보관된 XCal 표본의 점검

- 저장소에 **버전 관리되는 XCal 표본은 0건**이다: `git ls-files | grep -ic "xcal$"` → `0`, `find` 로 작업 트리·`test_data/` 를 훑어도 `.xcal` 은 시험 실행이 남기는 것 외에 없다(같은 `find` 가 같은 실행에서 `CMakePresets.json` 은 찾아냈다 — 대조). 그래서 "거부될 표본 파일"을 셀 수 없고, 점검한 것은 **제품 자신이 만드는 파일**이다.
- 생성기 `xpe_calib_generate_gain.cpp:320-322, 676-681` 은 `%d` 와 `%.9f` 로 따옴표 없는 평범한 숫자를 쓴다(단일 지점 `"polynomial_degree":0,"fit_r_squared":1.000000000`; 다점 `%d`/`%.9f`). 이 파일들은 이번 전체 시험(`GeneratedFileCarriesTheMetadataFields`, `GeneratedPolyFile…`, `CalibFixtureGen…`, 파이프라인 시험의 생성 세트)에서 적재되어 통과한다 — 새 규칙이 거부한 생성 파일은 없다.
- 점검하지 않은 것: 현장에서 보관 중인 구형 서명 XCal(저장소 밖). 그런 파일의 품질 필드가 `"2x"` 같은 값이면 이제 거부된다 — 5절의 정책이다. 이관이 필요한지는 현장 표본이 있어야 정할 수 있다.

## 7. 실측 (이 트리, 이 실행)

| 항목 | 관측 |
|---|---|
| 빌드 | 전체 타깃 `BUILD_EXIT=0` (`21_build_final.txt`) |
| preprocess DLL 시험 | 776 실행 / 768 통과 / 8 건너뜀(원래 건너뛰던 8건), 섞기도 동일 (`24`, `25`) |
| preprocess OOM exe | 29 통과 (`26_pre_oom.txt`; 직전 26) |
| common OOM exe / common 기존 | 9 / 69 통과 (변화 없음) |
| `ctest -N` | 923 (직전 918, 새 시험 5건) |
| 수출 이름 | preprocess 48, common 16 불변 (`29_exports_pre_diff.txt`) |
| 헤더 문서 / 프리셋 | 0 findings / 12 of 12 |

## 8. 반증 (한 번에 하나, 전체 빌드 후 두 시험 실행 파일 실행, 복원 뒤 `cmp` 동일)

| 팔 | 손상 | 빨개진 시험 |
|---|---|---|
| noroom | 용량 검사(`dataSize < outputBytes`) 삭제 | `TheOutputCapacityMustHold…`, `ABatchChecksTheRoomOf…`, `AFrameWhoseByteCount…` |
| u16only | 최종 결과를 항상 uint16 으로 가정 | `TheOutputCapacityMustHold…`, `ABatchChecksTheRoomOf…` |
| nooverflow | 곱셈 전 경계 검사 두 줄 삭제 | `AFrameWhoseByteCount…` |
| emptyabsent | 빈 값을 "없음"으로 읽기(옛 동작) | `AQualityFieldThatIsPresentButEmpty…` |
| notscalarabsent | 스칼라 아닌 값을 "없음"으로 읽기 | `AQualityFieldThatIsPresentButEmpty…` |

## 9. 미검증 (Gaps)

- **게인을 bypass 하고 비닝(>1)·결함·고스트를 켠 구성**: 코드를 읽으면(`pipeline.cpp` 비닝 단계의 `memcpy(stage5Data, stage4.data, pixelCount*sizeof(float))`) 이때 `stage4` 는 uint16 단계 버퍼(N*2 바이트)인데 N*4 바이트를 읽는 것으로 보인다. **가설이다 — 실행하지 않았다**(이 빌드에는 읽기 초과를 보는 도구가 없고, 시험의 정규 할당자는 쓰기 초과만 본다). 이번 변경은 이 구성의 위험을 늘리지도 줄이지도 않는다(전에도 같은 읽기). 별도 카드로 다룰지는 리더 판단.
- 오버플로 경계는 64비트 `size_t` 에서만 시험했다(32비트에서는 시험이 건너뜀 처리되며 실제 32비트 빌드는 없다). "딱 경계에서 맞는" 치수(`size_t` 최댓값을 정확히 채우는 쌍)는 할당 없이는 만들 수 없어, 맞는 쪽 대조는 2^61 화소(2^63 바이트)로 했다.
- `xpe_json_find_scalar` 는 `xpe_json_get_string` 과 같은 단순 `strstr` 키 탐색을 쓴다: 다른 문자열 값 안에 같은 `"key"` 가 먼저 나오면 그쪽을 읽는다(기존 한계, 이번에 고치지 않음).
- 개별 단계 API(`xpe_gain_correct` 등)의 입력 `dataSize` 규칙과 출력 용량 규칙은 건드리지 않았다.
- `ci-preprocess` 구성 하나, Windows/MSVC 에서만 돌렸다. 모듈 밖(`clients/`, `gui/`)에서 파이프라인 세 함수를 부르는 곳은 0건이다(`xpe_preprocess_init` 등은 같은 `grep` 이 찾아내는 것으로 대조) — 그래서 `dataSize==0` 호출자가 깨질 곳은 없다.

## 10. 잔여 위험

- 외부 호출자가 `dataSize = width*height*2` 로 float 결과를 받던 코드가 있으면 이제 `BUFFER_TOO_SMALL` 이 된다(의도된 변화). 그 코드는 전에는 잘린 프레임을 받고 있었다.
- `INVALID_INPUT` / `BUFFER_TOO_SMALL` 경계(3절 1번)는 한 줄로 바꿀 수 있는 결정이다.
